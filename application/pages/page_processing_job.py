from abc import abstractmethod
from dataclasses import dataclass
from datetime import datetime, UTC
from typing import Sequence

from sqlalchemy.sql.expression import select

from application.pages.pages_query_extensions import PagesQueryExtensions
from application.pages.processors.page_processor import PageProcessor
from infrastructure import BackgroundWorkerBase, PsqlDatabase, JobConfigBase
from infrastructure.logging.log_producer import LogLevel
from infrastructure.mongo import MongoJsonDatabase, MongoRawDataDatabase
from models import PageProcessingTask, ProcessingTaskStatus
from models.mongo.processed_page import ProcessedPage


@dataclass
class PageProcessingJobConfig(JobConfigBase):
    batch_size: int = 20

class PageProcessingJob[TProcessor: PageProcessor](BackgroundWorkerBase):
    __batch_size: int
    
    __db: PsqlDatabase
    _page_processor: TProcessor
    
    __content_database: MongoJsonDatabase[dict]
    __raw_content_database: MongoRawDataDatabase
    
    def __init__(
        self,
        job_config: PageProcessingJobConfig,
        database: PsqlDatabase,
        page_processor: TProcessor,
        content_database: MongoJsonDatabase[dict],
        raw_content_database: MongoRawDataDatabase):
        super().__init__(job_config)
        
        self.__batch_size = job_config.batch_size
        
        self.__db = database
        self._page_processor = page_processor
        
        self.__content_database = content_database
        self.__raw_content_database = raw_content_database

    @property
    @abstractmethod
    def processor(self) -> TProcessor:
        pass
    
    def do_work(self) -> bool:
        with self.__db.create_session() as db_session:
            self._log(
                LogLevel.INFO,
                "Processing another batch of pages (batch size = %d)...",
                self.__batch_size,
            )

            tasks: Sequence[PageProcessingTask] = db_session.scalars(
                (
                    select(PageProcessingTask)
                    .join(PageProcessingTask.page)
                    .filter(self.processor.page_filter())
                    .filter(PagesQueryExtensions.pending_task() | PagesQueryExtensions.retryable_task())
                    .order_by(PageProcessingTask.created_at.asc())
                    .with_for_update(of=PageProcessingTask, skip_locked=True)
                    .limit(self.__batch_size)
                )
            ).all()

            if not tasks:
                self._log(LogLevel.WARNING, "No page processing tasks found, skipping this batch...")
                return False

            for task in tasks:
                try:
                    with db_session.begin_nested():
                        self.__process_task(task, datetime.now(UTC))
                except Exception as ex:
                    self._log(
                        LogLevel.EXCEPTION,
                        "Exception occurred while processing page processing task with id %s",
                        task.id,
                        exc_info=ex,
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

        processed_page = self.processor.try_process_page(page)
        
        if processed_page is None:
            self._log(
                LogLevel.ERROR,
                "Failed to process page with id %d and url '%s' using processor %s...",
                page.id,
                page.url,
                self.processor.__class__.__name__
            )
            
            raise Exception(f"Failed to process page with id {page.id} and url '{page.url}' using processor {self.processor.__class__.__name__}")


        self._log(
            LogLevel.INFO,
            "Successfully retrieved new page content for page with id %d and url '%s', saving to database...",
            page.id,
            page.url
        )

        collection = self.__content_database.db.get_collection(ProcessedPage.COLLECTION_NAME)

        page_data = ProcessedPage(
            page_id=str(page.id),
            title=processed_page.title,
            text_content=processed_page.text_content,
            processed_at=reference_time,
        )

        collection.replace_one({ "page_id": page_data.page_id }, page_data.to_mongo_db(), upsert=True)

        file_content = bytes(processed_page.full_page_content.prettify(), encoding="utf-8")
        self.__raw_content_database.upsert_file(processed_page.page_id, file_content)

        task.page.content_size_in_kbytes = processed_page.content_size_in_kbytes
        task.page.raw_size_in_kbytes = len(file_content) // 1024

        task.complete()
        task.page.last_task_processed_at = reference_time
        
        self._log(
            LogLevel.INFO,
            "Successfully processed page processing task with id %d for page with id %d and url '%s'.",
            task.id,
            page.id,
            page.url
        )
