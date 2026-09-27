from dataclasses import dataclass
from datetime import datetime, UTC

from dependency_injector.providers import Resource
from dependency_injector.wiring import Provide, inject
from sqlalchemy.sql.expression import select

from application.pages.pages_query_extensions import PagesQueryExtensions
from infrastructure import BackgroundWorkerBase, JobConfigBase, BaseDiContainer, PsqlDatabase
from infrastructure.infrastructure_injector import InfrastructureDiContainer
from infrastructure.logging.log_producer import LogLevel
from models import Page, PageProcessingTask


@dataclass
class OutdatedPageProcessingSchedulerConfig(JobConfigBase):
    batch_size: int = 20
    
class OutdatedPageProcessingSchedulerConfigProvider(InfrastructureDiContainer):
    outdated_page_processing_scheduler_config = Resource(
        OutdatedPageProcessingSchedulerConfig,
        fail_timeout_seconds=BaseDiContainer.config.outdated_page_processing_scheduler_config.fail_timeout_seconds.as_int(),
        success_timeout_seconds=BaseDiContainer.config.outdated_page_processing_scheduler_config.success_timeout_seconds.as_int(),
        batch_size=BaseDiContainer.config.outdated_page_processing_scheduler_config.batch_size.as_int(),
    )

class OutdatedPageProcessingScheduler(BackgroundWorkerBase):
    __batch_size: int
    __db: PsqlDatabase
    
    @inject
    def __init__(
        self,
        job_config: OutdatedPageProcessingSchedulerConfig = Provide[OutdatedPageProcessingSchedulerConfigProvider.outdated_page_processing_scheduler_config],
        database: PsqlDatabase = Provide[InfrastructureDiContainer.database]):
        super().__init__(job_config)
        
        self.__db = database
        self.__batch_size = job_config.batch_size
    
    def do_work(self) -> bool:
        with self.__db.create_session() as db_session:
            self._log(
                LogLevel.INFO,
                "Processing another batch of the outdated pages (batch size = %d)...",
                self.__batch_size
            )

            now = datetime.now(UTC)
            outdated_pages = db_session.scalars(
                select(Page)
                .join(Page.source)
                .filter(PagesQueryExtensions.outdated_pages(now))
                .order_by(Page.created_at.asc())
                .with_for_update(skip_locked=True)
                .limit(self.__batch_size)
            ).all()
            
            if len(outdated_pages) == 0:
                self._log(LogLevel.WARNING,"No outdated pages were found in the database. Skipping...")
                return False
            
            self._log(
                LogLevel.INFO,
                "Found %d outdated pages in the database. Creating processing tasks for them...",
                len(outdated_pages)
            )

            processing_tasks = [self.__create_processing_task(outdated_page) for outdated_page in outdated_pages]
            db_session.add_all(processing_tasks)
            
            db_session.commit()
            
            return True
            
    @staticmethod
    def __create_processing_task(page: Page) -> PageProcessingTask:
        return PageProcessingTask(
            page=page,
            page_id=page.id,
        )