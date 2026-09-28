from dependency_injector.providers import Singleton, List

from application.pages.pages_di_container import PagesDiContainer
from application.sitemap import SitemapDiContainer
from application.sources.sources_di_container import SourcesDiContainer
from infrastructure import WorkerManager


class ApplicationDiContainer(SitemapDiContainer, SourcesDiContainer, PagesDiContainer):
    worker_manager = Singleton(
        WorkerManager,
        workers=List(
            SourcesDiContainer.outdated_source_processing_scheduler,

            SitemapDiContainer.global_sitemap_processing_job,
            SitemapDiContainer.local_sitemap_processing_job,

            PagesDiContainer.outdated_page_processing_scheduler,
            PagesDiContainer.page_processing_job,
        )
    )