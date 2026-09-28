from dependency_injector.providers import Factory, Singleton

from application.pages.download_page_content_endpoint import DownloadPageContentHandler
from application.pages.get_page_endpoint import GetPageHandler
from application.pages.outdated_page_processing_scheduler import OutdatedPageProcessingSchedulerConfigProvider, \
    OutdatedPageProcessingScheduler
from application.pages.page_processing_job import PageProcessingJobConfigProvider, PageProcessingJob
from application.pages.processors.page_processors_di_container import PageProcessorsDiContainer
from infrastructure.infrastructure_injector import InfrastructureDiContainer


class PagesDiContainer(OutdatedPageProcessingSchedulerConfigProvider, PageProcessingJobConfigProvider, PageProcessorsDiContainer):
    get_page_handler = Factory(
        GetPageHandler,
        database=InfrastructureDiContainer.database,
        content_database=InfrastructureDiContainer.mongo_json_db,
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

    page_processing_job = Singleton(
        PageProcessingJob,
        job_config=PageProcessingJobConfigProvider.page_processing_job_config,
        database=InfrastructureDiContainer.database,
        page_processors=PageProcessorsDiContainer.page_processors,
        content_database=InfrastructureDiContainer.mongo_json_db,
        raw_content_database=InfrastructureDiContainer.mongo_raw_data_db
    )
