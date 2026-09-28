from datetime import datetime

from sqlalchemy import ColumnElement
from sqlalchemy.sql.expression import text
from sqlalchemy.sql.operators import or_, and_

from application.sitemap.query_extensions import SitemapQueryExtensions
from models import Source


class SourceQueryExtensions:
    @staticmethod
    def has_pending_task() -> ColumnElement[bool]:
        return Source.sitemap_processing_tasks.any(
            and_(
                SitemapQueryExtensions.global_sitemap_processing_task(),
                SitemapQueryExtensions.pending_sitemap_process_task() | SitemapQueryExtensions.retryable_sitemap_process_task()
            )
        )
    
    @staticmethod
    def outdated_sources(reference_time: datetime) -> ColumnElement[bool]:
        return and_(
            or_(
                Source.last_task_processed_at.is_(None),
                Source.last_task_processed_at + Source.refetch_seconds * text("INTERVAL '1 second'") < reference_time
            ),
            ~SourceQueryExtensions.has_pending_task()
        )
    
    @staticmethod
    def can_create_new_pages() -> ColumnElement[bool]:
        return or_(
            Source.max_pages_count.is_(None),
            Source.page_count < Source.max_pages_count
        )
    
    @staticmethod
    def with_sitemap() -> ColumnElement[bool]:
        return Source.global_sitemap_url.is_not(None)