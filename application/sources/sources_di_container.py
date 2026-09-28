from dependency_injector.providers import Factory, Singleton

from application.sources.create_source_endpoint import CreateSourceHandler
from application.sources.outdated_source_processing_scheduler import OutdatedSourceProcessingSchedulerConfigProvider, OutdatedSourceProcessingScheduler
from infrastructure.infrastructure_injector import InfrastructureDiContainer


class SourcesDiContainer(OutdatedSourceProcessingSchedulerConfigProvider, InfrastructureDiContainer):
    create_source_handler = Factory(
        CreateSourceHandler,
        database=InfrastructureDiContainer.database,
    )

    outdated_source_processing_scheduler = Singleton(
        OutdatedSourceProcessingScheduler,
        job_config=OutdatedSourceProcessingSchedulerConfigProvider.outdated_source_processing_scheduler_config,
        database=InfrastructureDiContainer.database
    )
