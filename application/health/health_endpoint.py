from typing import Literal

from fastapi import APIRouter, Response, status
from pydantic import BaseModel

from infrastructure import LogProducer, PsqlDatabase
from infrastructure.mongo import MongoDbClient


class HealthResponse(BaseModel):
    status: Literal["ok", "error"]
    checks: dict[str, Literal["ok", "error"]]


class HealthHandler(LogProducer):
    def __init__(self, database: PsqlDatabase, mongo_client: MongoDbClient) -> None:
        super().__init__()
        self.__database = database
        self.__mongo_client = mongo_client

    def handle(self) -> HealthResponse:
        checks = {}
        for name, dependency in (("postgresql", self.__database), ("mongodb", self.__mongo_client)):
            try:
                checks[name] = "ok" if dependency.check_health() else "error"
            except Exception:
                self._logger.exception("Health check failed for %s", name)
                checks[name] = "error"
        return HealthResponse(
            status="ok" if all(value == "ok" for value in checks.values()) else "error",
            checks=checks,
        )


def create_health_router(handler: HealthHandler) -> APIRouter:
    router = APIRouter()

    @router.get("/readyz", response_model=HealthResponse, include_in_schema=False)
    def get_health(response: Response) -> HealthResponse:
        result = handler.handle()
        if result.status == "error":
            response.status_code = status.HTTP_503_SERVICE_UNAVAILABLE
        return result

    @router.get("/healthz", include_in_schema=False)
    def get_liveness() -> dict[str, str]:
        return {"status": "ok"}

    return router
