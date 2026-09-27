from dependency_injector.providers import Singleton

from application.sources.outdated_source_processing_scheduler import OutdatedSourceProcessingSchedulerConfigProvider, OutdatedSourceProcessingScheduler
from infrastructure.infrastructure_injector import InfrastructureDiContainer


class SourcesDiContainer(OutdatedSourceProcessingSchedulerConfigProvider, InfrastructureDiContainer):
    outdated_source_processing_scheduler = Singleton(
        OutdatedSourceProcessingScheduler,
        job_config=OutdatedSourceProcessingSchedulerConfigProvider.outdated_source_processing_scheduler_config,
        database=InfrastructureDiContainer.database
    ) 