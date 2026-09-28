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
from application.pages.download_page_content_endpoint import (
    DownloadPageContentHandler,
    create_download_page_content_router,
)
from application.pages.get_page_endpoint import (
    GetPageHandler,
    create_get_page_router,
)
from application.sources.create_source_endpoint import (
    CreateSourceHandler,
    create_source_router,
)
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
    def _create_app(
        self,
        config: ApplicationHostConfig = Provide[AppDiContainer.application_configuration],
    ) -> Server:
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

        app = FastAPI(
            title="WebSearch API",
            version="1.0.0",
            lifespan=lifespan,
            docs_url="/swagger",
            openapi_url="/openapi.json",
        )
        self._include_routers(app)

        uvicorn_config = Config(
            app=app,
            host=config.host,
            port=config.port,
            lifespan="on",
            timeout_graceful_shutdown=30,
        )
        
        return Server(uvicorn_config)

    @inject
    def _include_routers(
        self,
        app: FastAPI,
        create_source_handler: CreateSourceHandler = Provide[AppDiContainer.create_source_handler],
        get_page_handler: GetPageHandler = Provide[AppDiContainer.get_page_handler],
        download_page_content_handler: DownloadPageContentHandler = Provide[AppDiContainer.download_page_content_handler],
    ) -> None:
        app.include_router(create_source_router(create_source_handler))
        app.include_router(create_get_page_router(get_page_handler))
        app.include_router(
            create_download_page_content_router(download_page_content_handler)
        )
    
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
