from uuid import UUID

from fastapi import APIRouter
from pydantic import BaseModel
from sqlalchemy import func, select

from infrastructure import PsqlDatabase
from models import Page, PageProcessingTask, ProcessingTaskStatus, Source


class SourcePageProcessingTaskStatsResponse(BaseModel):
    source_id: UUID
    source_url: str
    total: int
    pending: int
    completed: int
    failed: int
    skipped: int
    retryable_failed: int
    exhausted_failed: int


class GetPageProcessingTaskStatsHandler:
    def __init__(self, database: PsqlDatabase) -> None:
        self.__database = database

    def handle(self) -> list[SourcePageProcessingTaskStatsResponse]:
        task = PageProcessingTask
        failed = task.status == ProcessingTaskStatus.FAILED
        query = (
            select(
                Source.id.label("source_id"),
                Source.url.label("source_url"),
                func.count(task.id).label("total"),
                func.count(task.id).filter(task.status == ProcessingTaskStatus.PENDING).label("pending"),
                func.count(task.id).filter(task.status == ProcessingTaskStatus.COMPLETED).label("completed"),
                func.count(task.id).filter(failed).label("failed"),
                func.count(task.id).filter(task.status == ProcessingTaskStatus.SKIPPED).label("skipped"),
                func.count(task.id).filter(failed & task.retryable).label("retryable_failed"),
                func.count(task.id).filter(failed & ~task.retryable).label("exhausted_failed"),
            )
            .select_from(Source)
            .outerjoin(Page, Page.source_id == Source.id)
            .outerjoin(task, task.page_id == Page.id)
            .group_by(Source.id, Source.url)
            .order_by(Source.url, Source.id)
        )
        with self.__database.create_session() as session:
            return [SourcePageProcessingTaskStatsResponse(**row) for row in session.execute(query).mappings()]


def create_get_page_processing_task_stats_router(handler: GetPageProcessingTaskStatsHandler) -> APIRouter:
    router = APIRouter(tags=["page processing tasks"])

    @router.get(
        "/page-processing-tasks/stats",
        response_model=list[SourcePageProcessingTaskStatsResponse],
        description="Task counts for every source, including sources with no tasks. Failed counts include retryable and exhausted tasks.",
    )
    def get_page_processing_task_stats() -> list[SourcePageProcessingTaskStatsResponse]:
        return handler.handle()

    return router
