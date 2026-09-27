from dataclasses import dataclass
from datetime import datetime, UTC
from typing import Sequence

from dependency_injector.providers import Resource
from dependency_injector.wiring import Provide, inject
from sqlalchemy.sql.expression import select

from application.pages.pages_query_extensions import PagesQueryExtensions
from application.pages.processors.page_processor import PageProcessor
from application.pages.processors.page_processors_di_container import PageProcessorsDiContainer
from infrastructure import BackgroundWorkerBase, PsqlDatabase, JobConfigBase, BaseDiContainer
from infrastructure.infrastructure_injector import InfrastructureDiContainer
from infrastructure.logging.log_producer import LogLevel
from infrastructure.mongo import MongoJsonDatabase, MongoRawDataDatabase
from models import PageProcessingTask, ProcessingTaskStatus
from models.mongo.processed_page import ProcessedPage


@dataclass
class PageProcessingJobConfig(JobConfigBase):
    batch_size: int = 20

class PageProcessingJobConfigProvider(BaseDiContainer):
    page_processing_job_config = Resource(
        PageProcessingJobConfig,
        fail_timeout_seconds=BaseDiContainer.config.page_processing_job_config.fail_timeout_seconds.as_int(),
        success_timeout_seconds=BaseDiContainer.config.page_processing_job_config.success_timeout_seconds.as_int(),
        batch_size=BaseDiContainer.config.page_processing_job_config.batch_size.as_int(),
    )

class PageProcessingJob(BackgroundWorkerBase):
    __batch_size: int
    
    __db: PsqlDatabase
    __page_processors: list[PageProcessor]
    
    __content_database: MongoJsonDatabase[ProcessedPage]
    __raw_content_database: MongoRawDataDatabase
    
    @inject
    def __init__(
        self,
        job_config: PageProcessingJobConfig = Provide[PageProcessingJobConfigProvider.page_processing_job_config],
        database: PsqlDatabase = Provide[InfrastructureDiContainer.database],
        page_processors: list[PageProcessor] = Provide[PageProcessorsDiContainer.providers],
        content_database: MongoJsonDatabase = Provide[InfrastructureDiContainer.mongo_json_db],
        raw_content_database: MongoRawDataDatabase = Provide[InfrastructureDiContainer.mongo_raw_data_db]):
        super().__init__(job_config)
        
        self.__batch_size = job_config.batch_size
        
        self.__db = database
        self.__page_processors = page_processors
        
        self.__content_database = content_database
        self.__raw_content_database = raw_content_database
    
    def do_work(self) -> bool:
        with self.__db.create_session() as db_session:
            self._log(
                LogLevel.INFO,
                "Processing another batch of pages (batch size = %d)...",
                self.__batch_size
            )
            
            tasks: Sequence[PageProcessingTask] = db_session.scalars(
                select(PageProcessingTask)
                .join(PageProcessingTask.page)
                .filter(PagesQueryExtensions.pending_task() | PagesQueryExtensions.retryable_task())
                .order_by(PageProcessingTask.created_at.asc())
                .with_for_update(skip_locked=True)
                .limit(self.__batch_size)
            ).all()
            
            if len(tasks) == 0:
                self._log(LogLevel.WARNING, "No page processing tasks found, skipping this batch...")
                return False
            
            for task in tasks:
                task.status = ProcessingTaskStatus.RUNNING
                
            db_session.commit()
            
            now = datetime.now(UTC)
            for task in tasks:
                try:
                    self.__process_task(task, now)
                except Exception as ex:
                    self._log(
                        LogLevel.EXCEPTION,
                        "Exception occurred while processing page processing task with id %d",
                        task.id,
                        exc_info=ex
                    )

                    task.retry_processing(ex)
                    
                db_session.commit()
                
            return all([task.status == ProcessingTaskStatus.COMPLETED for task in tasks])
    
    def __process_task(
        self,
        task: PageProcessingTask,
        reference_time: datetime):
        page = task.page
        
        self._log(
            LogLevel.INFO,
            "Processing page processing task with id %d for page with id %d and url '%s'...",
            task.id,
            page.id,
            page.url
        )

        processor = next((p for p in self.__page_processors if p.can_process(page)))
        processed_page = processor.try_process_page(page)
        
        if processed_page is not None:
            self._log(
                LogLevel.INFO,
                "Successfully retrieved new page content for page with id %d and url '%s', saving to database...",
                page.id,
                page.url
            )

            collection = self.__content_database.db.get_collection("page_content")
            
            page_data = ProcessedPage(
                page_id=str(page.id),
                title=processed_page.title,
                text_content=processed_page.text_content
            )

            collection.replace_one({ "page_id": page_data.page_id }, page_data.to_mongo_db(), upsert=True)

            file_content = bytes(processed_page.full_page_content.prettify(), encoding="utf-8")
            self.__raw_content_database.upsert_file(processed_page.page_id, file_content)
    
        task.complete()
        task.page.last_task_processed_at = reference_time
        
        self._log(
            LogLevel.INFO,
            "Successfully processed page processing task with id %d for page with id %d and url '%s'.",
            task.id,
            page.id,
            page.url
        )