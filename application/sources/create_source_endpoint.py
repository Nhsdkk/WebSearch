from datetime import datetime
from uuid import UUID

from fastapi import APIRouter, status
from pydantic import BaseModel, ConfigDict, Field, HttpUrl

from infrastructure import PsqlDatabase
from models import Source


class CreateSourceRequest(BaseModel):
    url: HttpUrl
    refetch_seconds: int = Field(default=900, gt=0)
    global_sitemap_url: HttpUrl | None = None
    max_pages_count: int | None = Field(default=None, gt=0)


class CreateSourceResponse(BaseModel):
    model_config = ConfigDict(from_attributes=True)

    id: UUID
    url: str
    refetch_seconds: int
    global_sitemap_url: str | None
    max_pages_count: int | None
    page_count: int
    last_task_processed_at: datetime | None
    created_at: datetime


class CreateSourceHandler:
    def __init__(self, database: PsqlDatabase) -> None:
        self.__database = database

    def handle(self, request: CreateSourceRequest) -> CreateSourceResponse:
        source = Source(
            url=str(request.url),
            refetch_seconds=request.refetch_seconds,
            global_sitemap_url=(
                str(request.global_sitemap_url)
                if request.global_sitemap_url is not None
                else None
            ),
            max_pages_count=request.max_pages_count,
        )

        with self.__database.create_session() as db_session:
            db_session.add(source)
            db_session.commit()
            db_session.refresh(source)

            return CreateSourceResponse.model_validate(source)


def create_source_router(handler: CreateSourceHandler) -> APIRouter:
    router = APIRouter(tags=["sources"])

    @router.post(
        "/sources",
        response_model=CreateSourceResponse,
        status_code=status.HTTP_201_CREATED,
    )
    def create_source(request: CreateSourceRequest) -> CreateSourceResponse:
        return handler.handle(request)

    return router
