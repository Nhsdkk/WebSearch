from dependency_injector.providers import Factory, Singleton

from application.pages.download_page_content_endpoint import DownloadPageContentHandler
from application.pages.get_page_endpoint import GetPageHandler
from application.pages.get_page_processing_task_stats_endpoint import GetPageProcessingTaskStatsHandler
from application.pages.get_processed_pages_endpoint import GetProcessedPagesHandler
from application.pages.outdated_page_processing_scheduler import OutdatedPageProcessingSchedulerConfigProvider, OutdatedPageProcessingScheduler
from application.pages.ria_page_processing_job import RiaPageProcessingJob, RiaPageProcessingJobConfigProvider
from application.pages.izvestia_page_processing_job import IzvestiaPageProcessingJob, IzvestiaPageProcessingJobConfigProvider
from application.pages.processors.page_processors_di_container import PageProcessorsDiContainer
from infrastructure.infrastructure_injector import InfrastructureDiContainer


class PagesDiContainer(
    OutdatedPageProcessingSchedulerConfigProvider,
    IzvestiaPageProcessingJobConfigProvider,
    RiaPageProcessingJobConfigProvider,
    PageProcessorsDiContainer):
    get_page_handler = Factory(
        GetPageHandler,
        database=InfrastructureDiContainer.database,
        content_database=InfrastructureDiContainer.mongo_json_db,
    )

    get_processed_pages_handler = Factory(
        GetProcessedPagesHandler,
        database=InfrastructureDiContainer.database,
        content_database=InfrastructureDiContainer.mongo_json_db,
    )

    get_page_processing_task_stats_handler = Factory(
        GetPageProcessingTaskStatsHandler,
        database=InfrastructureDiContainer.database,
    )

    download_page_content_handler = Factory(
        DownloadPageContentHandler,
        database=InfrastructureDiContainer.database,
        raw_content_database=InfrastructureDiContainer.mongo_raw_data_db,
    )

    outdated_page_processing_scheduler = Singleton(
        OutdatedPageProcessingScheduler,
        job_config=OutdatedPageProcessingSchedulerConfigProvider.outdated_page_processing_scheduler_config,
        database=InfrastructureDiContainer.database,
    )

    ria_page_processing_job = Singleton(
        RiaPageProcessingJob,
        job_config=RiaPageProcessingJobConfigProvider.ria_page_processing_job_config,
        database=InfrastructureDiContainer.database,
        page_processor=PageProcessorsDiContainer.ria_page_processor,
        content_database=InfrastructureDiContainer.mongo_json_db,
        raw_content_database=InfrastructureDiContainer.mongo_raw_data_db
    )

    izvestia_page_processing_job = Singleton(
        IzvestiaPageProcessingJob,
        job_config=IzvestiaPageProcessingJobConfigProvider.izvestia_page_processing_job_config,
        database=InfrastructureDiContainer.database,
        page_processor=PageProcessorsDiContainer.izvestia_page_processor,
        content_database=InfrastructureDiContainer.mongo_json_db,
        raw_content_database=InfrastructureDiContainer.mongo_raw_data_db,
    )
