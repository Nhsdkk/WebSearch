from datetime import datetime

from sqlalchemy import ColumnElement
from sqlalchemy.sql.expression import text, or_, and_

from models import Page, Source, PageProcessingTask, ProcessingTaskStatus


class PagesQueryExtensions:
    @staticmethod
    def by_urls(urls: list[str]) -> ColumnElement[bool]:
        return Page.url.in_(urls)
    
    @staticmethod
    def has_active_task() -> ColumnElement[bool]:
        return Page.processing_tasks.any(PagesQueryExtensions.retryable_task() | PagesQueryExtensions.pending_task())
    
    @staticmethod
    def pending_task() -> ColumnElement[bool]:
        return PageProcessingTask.status == ProcessingTaskStatus.PENDING
    
    @staticmethod
    def retryable_task() -> ColumnElement[bool]:
        return and_(PageProcessingTask.status == ProcessingTaskStatus.FAILED, PageProcessingTask.retryable)
    
    @staticmethod
    def outdated_pages(reference_time: datetime) -> ColumnElement[bool]:
        return and_(
            or_(
                Page.last_task_processed_at.is_(None),
                Page.last_task_processed_at + Source.refetch_seconds * text("INTERVAL '1 second'") < reference_time
            ),
            ~PagesQueryExtensions.has_active_task()
        )
