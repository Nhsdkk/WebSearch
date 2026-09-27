import asyncio
import logging
from contextlib import asynccontextmanager
from dataclasses import dataclass
from typing import AsyncGenerator

from dependency_injector.providers import Resource
from dependency_injector.wiring import inject, Provide
from fastapi import FastAPI
from uvicorn import Server, Config

from application.application_di_container import ApplicationDiContainer
from infrastructure import LogProducer, BaseDiContainer, WorkerManager
from infrastructure.infrastructure_injector import InfrastructureDiContainer

from application import sitemap

@dataclass(frozen=True)
class ApplicationHostConfig:
    host: str = "localhost"
    port: int = 8080

class AppDiContainer(ApplicationDiContainer):
    application_configuration = Resource(
        ApplicationHostConfig,
        host=BaseDiContainer.config.application.host,
        port=BaseDiContainer.config.application.port.as_int(),
    )

class Application(LogProducer):
    _server: Server
    _logger: logging.Logger
    
    def __init__(self) -> None:
        super().__init__()

        self._configure_injections()
        self._server = self._create_app()

    @staticmethod
    def _configure_injections() -> None:
        app_di_container = AppDiContainer()
        app_di_container.init_resources()
        app_di_container.wire(modules=[__name__], packages=[sitemap])
    
    @inject
    def _create_app(self, config: ApplicationHostConfig = Provide[AppDiContainer.application_configuration]) -> Server:
        @asynccontextmanager
        async def lifespan(_: FastAPI) -> AsyncGenerator:
            try:
                self._logger.info("Starting the app")
                yield
            except Exception as e:
                self._logger.exception(
                    "Exception occurred while running the app",
                    exc_info=e)
            finally:
                self.__dispose()
                self._logger.info("Successfully disposed all of the application resources")

        app =  FastAPI(lifespan=lifespan)

        uvicorn_config = Config(
            app=app,
            host=config.host,
            port=config.port,
            lifespan="on",
            timeout_graceful_shutdown=30,
        )
        
        return Server(uvicorn_config)
    
    @inject
    def __dispose(
        self,
        worker_manager: WorkerManager = Provide[ApplicationDiContainer.worker_manager]
    ) -> None:
        worker_manager.dispose()
        

    @inject
    async def run(
        self,
        worker_manager: WorkerManager = Provide[AppDiContainer.worker_manager]) -> None:
        worker_manager.run()
        await self._server.serve()

if __name__ == "__main__":
    application = Application()
    asyncio.run(application.run())