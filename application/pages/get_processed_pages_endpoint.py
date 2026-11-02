from datetime import datetime
from uuid import UUID

from fastapi import APIRouter, Query
from pydantic import BaseModel
from sqlalchemy import func, select

from infrastructure import PsqlDatabase
from infrastructure.mongo import MongoJsonDatabase
from models import Page
from models.mongo.processed_page import ProcessedPage


class ProcessedPageSummaryResponse(BaseModel):
    id: UUID
    url: str
    source_id: UUID
    sitemap_url: str | None
    created_at: datetime
    last_task_processed_at: datetime
    raw_size_in_kbytes: int | None
    content_size_in_kbytes: int | None
    title: str | None


class ProcessedPagesResponse(BaseModel):
    items: list[ProcessedPageSummaryResponse]
    total: int
    page: int
    page_size: int
    total_pages: int


class GetProcessedPagesHandler:
    def __init__(self, database: PsqlDatabase, content_database: MongoJsonDatabase[dict]) -> None:
        self.__database = database
        self.__content_database = content_database

    def handle(self, page: int, page_size: int, source_id: UUID | None = None) -> ProcessedPagesResponse:
        filters = [Page.last_task_processed_at.is_not(None)]
        if source_id is not None:
            filters.append(Page.source_id == source_id)

        with self.__database.create_session() as session:
            total = session.scalar(select(func.count()).select_from(Page).where(*filters))
            pages = session.scalars(
                select(Page)
                .where(*filters)
                .order_by(Page.last_task_processed_at.desc(), Page.id.desc())
                .offset((page - 1) * page_size)
                .limit(page_size)
            ).all()
            items = [
                {
                    "id": item.id,
                    "url": item.url,
                    "source_id": item.source_id,
                    "sitemap_url": item.sitemap_url,
                    "created_at": item.created_at,
                    "last_task_processed_at": item.last_task_processed_at,
                    "raw_size_in_kbytes": item.raw_size_in_kbytes,
                    "content_size_in_kbytes": item.content_size_in_kbytes,
                }
                for item in pages
            ]

        titles = {}
        if items:
            collection = self.__content_database.db.get_collection(ProcessedPage.COLLECTION_NAME)
            documents = collection.find(
                {"page_id": {"$in": [str(item["id"]) for item in items]}},
                {"_id": 0, "page_id": 1, "title": 1},
            )
            titles = {document["page_id"]: document.get("title") for document in documents}

        return ProcessedPagesResponse(
            items=[ProcessedPageSummaryResponse(**item, title=titles.get(str(item["id"]))) for item in items],
            total=total,
            page=page,
            page_size=page_size,
            total_pages=(total + page_size - 1) // page_size,
        )


def create_get_processed_pages_router(handler: GetProcessedPagesHandler) -> APIRouter:
    router = APIRouter(tags=["pages"])

    @router.get(
        "/pages",
        response_model=ProcessedPagesResponse,
        description="Pages with a successful processing timestamp, newest first. Use the page detail and download endpoints to retrieve content.",
    )
    def get_processed_pages(
        page: int = Query(default=1, ge=1),
        page_size: int = Query(default=20, ge=1, le=100),
        source_id: UUID | None = None,
    ) -> ProcessedPagesResponse:
        return handler.handle(page=page, page_size=page_size, source_id=source_id)

    return router
