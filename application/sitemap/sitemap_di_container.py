from dependency_injector.providers import Singleton

from application.sitemap.global_sitemap_processing_job import GlobalSitemapProcessingJob, GlobalSitemapConfigProvider
from application.sitemap.local_sitemap_processing_job import LocalSitemapProcessingJob, LocalSitemapConfigProvider
from application.sitemap.url_filters import UrlFiltersDiContainer
from infrastructure.infrastructure_injector import InfrastructureDiContainer


class SitemapDiContainer(GlobalSitemapConfigProvider, UrlFiltersDiContainer, InfrastructureDiContainer):
    global_sitemap_processing_job = Singleton(
        GlobalSitemapProcessingJob,
        job_config=GlobalSitemapConfigProvider.global_sitemap_processing_job_config,
        database=InfrastructureDiContainer.database,
        http_client=InfrastructureDiContainer.http_client,
    )
    
    local_sitemap_processing_job = Singleton(
        LocalSitemapProcessingJob,
        job_config=LocalSitemapConfigProvider.local_sitemap_processing_job_config,
        database=InfrastructureDiContainer.database,
        http_client=InfrastructureDiContainer.http_client,
        url_filters=UrlFiltersDiContainer.url_filters
    )
