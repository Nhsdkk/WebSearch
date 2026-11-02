from dependency_injector.providers import Singleton, List

from application.auth import BasicAuth
from application.health.health_di_container import HealthDiContainer
from application.pages.pages_di_container import PagesDiContainer
from application.sitemap import SitemapDiContainer
from application.sources.sources_di_container import SourcesDiContainer
from infrastructure import WorkerManager
from infrastructure.di.base_di_container import BaseDiContainer


class ApplicationDiContainer(SitemapDiContainer, SourcesDiContainer, PagesDiContainer, HealthDiContainer):
    basic_auth = Singleton(
        BasicAuth,
        username=BaseDiContainer.config.basic_auth_config.username,
        password=BaseDiContainer.config.basic_auth_config.password,
    )

    worker_manager = Singleton(
        WorkerManager,
        workers=List(
            SourcesDiContainer.outdated_source_processing_scheduler,

            SitemapDiContainer.global_sitemap_processing_job,
            SitemapDiContainer.local_sitemap_processing_job,

            PagesDiContainer.outdated_page_processing_scheduler,

            PagesDiContainer.ria_page_processing_job,
            PagesDiContainer.izvestia_page_processing_job,
        )
    )
