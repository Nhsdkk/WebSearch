from dependency_injector.providers import Singleton

from application.sitemap.global_sitemap_processing_job import GlobalSitemapProcessingJob, GlobalSitemapConfigProvider
from application.sitemap.local_sitemap_processing_job import LocalSitemapProcessingJob, LocalSitemapConfigProvider
from infrastructure.infrastructure_injector import InfrastructureDiContainer


class SitemapDiContainer(GlobalSitemapConfigProvider, InfrastructureDiContainer):
    global_sitemap_processing_job : GlobalSitemapProcessingJob = Singleton(
        GlobalSitemapProcessingJob,
        job_config=GlobalSitemapConfigProvider.global_sitemap_processing_job_config,
        database=InfrastructureDiContainer.database,
    )
    
    local_sitemap_processing_job : LocalSitemapProcessingJob = Singleton(
        LocalSitemapProcessingJob,
        job_config=LocalSitemapConfigProvider.local_sitemap_processing_job_config,
        database=InfrastructureDiContainer.database,
    )