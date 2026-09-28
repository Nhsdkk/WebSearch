from dataclasses import dataclass
from datetime import datetime, UTC

from dependency_injector.providers import Resource
from dependency_injector.wiring import Provide
from sqlalchemy import Sequence
from sqlalchemy.sql.expression import select

from application.sources.sources_query_extensions import SourceQueryExtensions
from infrastructure import BackgroundWorkerBase, JobConfigBase, BaseDiContainer, PsqlDatabase
from infrastructure.infrastructure_injector import InfrastructureDiContainer
from infrastructure.logging.log_producer import LogLevel
from models import Source, SitemapType, SitemapProcessingTask


@dataclass
class OutdatedSourceProcessingSchedulerConfig(JobConfigBase):
    batch_size: int = 20

class OutdatedSourceProcessingSchedulerConfigProvider(BaseDiContainer):
    outdated_source_processing_scheduler_config = Resource(
        OutdatedSourceProcessingSchedulerConfig,
        fail_timeout_seconds=BaseDiContainer.config.outdated_source_processing_scheduler_config.fail_timeout_seconds.as_int(),
        success_timeout_seconds=BaseDiContainer.config.outdated_source_processing_scheduler_config.success_timeout_seconds.as_int(),
        batch_size=BaseDiContainer.config.outdated_source_processing_scheduler_config.batch_size.as_int(),
    )

class OutdatedSourceProcessingScheduler(BackgroundWorkerBase):
    __batch_size: int
    __db: PsqlDatabase
    
    def __init__(
        self,
        job_config: OutdatedSourceProcessingSchedulerConfig = Provide[OutdatedSourceProcessingSchedulerConfigProvider.outdated_source_processing_scheduler_config],
        database: PsqlDatabase = Provide[InfrastructureDiContainer.database]):
        super().__init__()
        
        self.__batch_size = job_config.batch_size
        self.__db = database

    def do_work(self) -> bool:
        with self.__db.create_session() as db_session:
            self._log(
                LogLevel.INFO,
                "Processing another batch of outdated sources (batch size = %d)...",
                self.__batch_size,
            )

            now = datetime.now(UTC)
            outdated_sources : Sequence[Source] = db_session.scalars(
                select(Source)
                .filter(SourceQueryExtensions.with_sitemap())
                .filter(SourceQueryExtensions.outdated_sources(now))
                .filter(SourceQueryExtensions.can_create_new_pages())
                .order_by(Source.created_at.asc())
                .limit(self.__batch_size)
            ).all()
            
            if len(outdated_sources) == 0:
                self._log(
                    LogLevel.WARNING,
                    "No outdated sources found. Skipping processing...",
                )
                
                return False
        
            self._log(
                LogLevel.INFO,
                "Found %d outdated sources. Creating processing tasks...",
                len(outdated_sources),
            )

            processing_tasks = [self.__create_processing_task(source) for source in outdated_sources]
            db_session.add_all(processing_tasks)
                
            db_session.commit()
            
            return True

    @staticmethod
    def __create_processing_task(source: Source) -> SitemapProcessingTask:
        if source.global_sitemap_url is None:
            raise AttributeError('global_sitemap_url cannot be None')

        return SitemapProcessingTask(
            sitemap_url=source.global_sitemap_url,
            source=source,
            source_id=source.id,
            type=SitemapType.GLOBAL
        )