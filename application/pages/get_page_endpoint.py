from datetime import datetime
from uuid import UUID

from fastapi import APIRouter, HTTPException, status
from pydantic import BaseModel

from infrastructure import PsqlDatabase
from infrastructure.mongo import MongoJsonDatabase
from models import Page
from models.mongo.processed_page import ProcessedPage


class PageResponse(BaseModel):
    id: UUID
    url: str
    source_id: UUID
    sitemap_url: str | None
    created_at: datetime
    last_task_processed_at: datetime | None
    title: str | None
    text_content: str | None
    raw_size_in_kbytes: int | None
    content_size_in_kbytes: int | None


class PageNotFoundError(Exception):
    pass


class GetPageHandler:
    def __init__(
        self,
        database: PsqlDatabase,
        content_database: MongoJsonDatabase[dict],
    ) -> None:
        self.__database = database
        self.__content_database = content_database

    def handle(self, page_id: UUID) -> PageResponse:
        with self.__database.create_session() as db_session:
            page = db_session.get(Page, page_id)
            if page is None:
                raise PageNotFoundError

            page_data = {
                "id": page.id,
                "url": page.url,
                "source_id": page.source_id,
                "sitemap_url": page.sitemap_url,
                "created_at": page.created_at,
                "last_task_processed_at": page.last_task_processed_at,
                "raw_page_size_in_kbytes": page.raw_size_in_kbytes,
                "content_size_in_kbytes": page.content_size_in_kbytes,
            }

        retrieved_content = self.__content_database.db.get_collection(ProcessedPage.COLLECTION_NAME).find_one({"page_id": str(page_id)})
        processed_page = ProcessedPage.from_mongo_db(retrieved_content) if retrieved_content is not None else None

        return PageResponse(
            **page_data,
            title=(
                processed_page.title
                if processed_page is not None
                else None
            ),
            text_content=(
                processed_page.text_content
                if processed_page is not None
                else None
            ),
        )


def create_get_page_router(handler: GetPageHandler) -> APIRouter:
    router = APIRouter(tags=["pages"])

    @router.get(
        "/pages/{page_id}",
        response_model=PageResponse,
        responses={status.HTTP_404_NOT_FOUND: {"description": "Page not found"}},
    )
    def get_page(page_id: UUID) -> PageResponse:
        try:
            return handler.handle(page_id)
        except PageNotFoundError as error:
            raise HTTPException(
                status_code=status.HTTP_404_NOT_FOUND,
                detail="Page not found",
            ) from error

    return router
