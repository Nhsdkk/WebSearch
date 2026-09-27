from operator import and_

from sqlalchemy import ColumnElement

from models import SitemapProcessingTask, SitemapType, ProcessingTaskStatus


class SitemapQueryExtensions:
    @staticmethod
    def global_sitemap_processing_task() -> ColumnElement[bool]:
        return SitemapProcessingTask.type == SitemapType.GLOBAL
    
    @staticmethod
    def pending_sitemap_process_task() -> ColumnElement[bool]:
        return SitemapProcessingTask.status == ProcessingTaskStatus.PENDING
    
    @staticmethod
    def retryable_sitemap_process_task() -> ColumnElement[bool]:
        return and_(SitemapProcessingTask.status == ProcessingTaskStatus.FAILED, SitemapProcessingTask.retryable)
    
    @staticmethod
    def local_sitemap_process_task() -> ColumnElement[bool]:
        return SitemapProcessingTask.type == SitemapType.LOCAL