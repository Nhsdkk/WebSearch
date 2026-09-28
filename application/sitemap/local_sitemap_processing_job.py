from dataclasses import dataclass
from datetime import datetime
from typing import Self

from bs4 import Tag, BeautifulSoup
from dependency_injector.providers import Resource
from dependency_injector.wiring import Provide
from sqlalchemy import Sequence
from sqlalchemy.orm import Session
from sqlalchemy.sql.expression import select

from application.pages.pages_query_extensions import PagesQueryExtensions
from application.sitemap.query_extensions import SitemapQueryExtensions
from infrastructure import BackgroundWorkerBase, PsqlDatabase, JobConfigBase, BaseDiContainer
from infrastructure.infrastructure_injector import InfrastructureDiContainer
from infrastructure.http import HttpClient
from infrastructure.logging.log_producer import LogLevel
from infrastructure.utils import UrlUtils
from models import SitemapProcessingTask, ProcessingTaskStatus, Page, Source


@dataclass
class SitemapPageInfo:
    url: str
    # last_modified_at: datetime

    @classmethod
    def from_xml(cls, xml_data: Tag) -> Self:
        return cls(
            url=UrlUtils.normalize_url(xml_data.loc.text),
            # last_modified_at=datetime.strptime(xml_data.lastmod.text, '%Y-%m-%dT%H:%M:%S%z'),
        )
    
    def to_page(self, sitemap_url: str, source: Source) -> Page:
        return Page(
            url=self.url,
            sitemap_url=sitemap_url,
            source=source,
            source_id=source.id,
        )

@dataclass
class LocalSitemapProcessingJobConfig(JobConfigBase):
    batch_size: int = 20

class LocalSitemapConfigProvider(BaseDiContainer):
    local_sitemap_processing_job_config = Resource(
        LocalSitemapProcessingJobConfig,
        fail_timeout_seconds=BaseDiContainer.config.local_sitemap_processing_job_config.fail_timeout_seconds.as_int(),
        success_timeout_seconds=BaseDiContainer.config.local_sitemap_processing_job_config.success_timeout_seconds.as_int(),
        batch_size=BaseDiContainer.config.local_sitemap_processing_job_config.batch_size.as_int(),
    )

class LocalSitemapProcessingJob(BackgroundWorkerBase):
    _batch_size: int
    _database: PsqlDatabase
    _http_client: HttpClient
    
    def __init__(
            self,
            job_config: LocalSitemapProcessingJobConfig = Provide[LocalSitemapConfigProvider.local_sitemap_processing_job_config],
            database: PsqlDatabase = Provide[InfrastructureDiContainer.database],
            http_client: HttpClient = Provide[InfrastructureDiContainer.http_client]):
        super().__init__(job_config)
        
        self._database = database
        self._http_client = http_client
        self._batch_size = job_config.batch_size

    def do_work(self) -> bool:
        with self._database.create_session() as db_session:
            self._log(
                LogLevel.INFO,
                "Processing next batch of local sitemaps (batch size = %d)",
                self._batch_size,
            )
            tasks: Sequence[SitemapProcessingTask] = db_session.scalars(
                (
                    select(SitemapProcessingTask)
                    .join(SitemapProcessingTask.source)
                    .filter(SitemapQueryExtensions.local_sitemap_process_task())
                    .filter(SitemapQueryExtensions.pending_sitemap_process_task() | SitemapQueryExtensions.retryable_sitemap_process_task())
                    .order_by(SitemapProcessingTask.created_at.asc())
                    .with_for_update(of=SitemapProcessingTask, skip_locked=True)
                    .limit(self._batch_size)
                )
            ).all()

            if not tasks:
                self._log(LogLevel.WARNING, "No local sitemaps processing tasks found. Skipping processing...")
                return False

            for task in tasks:
                try:
                    with db_session.begin_nested():
                        self._log(LogLevel.INFO, "Processing task: %s", task.id)
                        current_page_count = task.source.page_count

                        if task.source.max_pages_count is not None and current_page_count >= task.source.max_pages_count:
                            self._log(
                                LogLevel.WARNING,
                                "Source %s has reached its max pages count %d (current count = %d). Skipping task %s...",
                                task.source_id,
                                task.source.max_pages_count,
                                current_page_count,
                                task.id,
                            )
                            task.skip(f"Source {task.source_id} has reached its max pages count. Limit: {task.source.max_pages_count}. Current: {current_page_count}")
                        else:
                            page_infos = self.__retrieve_pages(task.sitemap_url)
                            self.__create_pages(db_session, page_infos, task)
                            task.complete()
                except Exception as e:
                    self._log(LogLevel.EXCEPTION, "Failed to process task %s", task.id, exc_info=e)
                    task.retry_processing(e)

            db_session.commit()

            return all([task.status == ProcessingTaskStatus.COMPLETED or task.status == ProcessingTaskStatus.SKIPPED for task in tasks])
        
    def __create_pages(self, db_session: Session, page_infos: list[SitemapPageInfo], task: SitemapProcessingTask) -> None:
        page_infos_by_url = {page_info.url: page_info for page_info in page_infos}
        page_urls = list(page_infos_by_url)
        
        existing_page_urls = set(
            db_session.scalars(
                select(Page.url)
                .filter(PagesQueryExtensions.by_urls(page_urls))
            ).all()
        )
        
        nonexisting_pages = [
            page for url, page in page_infos_by_url.items() if url not in existing_page_urls
        ]
        
        if len(nonexisting_pages) == 0:
            self._log(
                LogLevel.WARNING,
                "No nonexisting pages found in local sitemap %s. Skipping task creation...",
                task.sitemap_url
            )
            
            return
        
        self._log(
            LogLevel.INFO,
            "Found %d nonexisting pages in local sitemap %s. Creating new pages...",
            len(nonexisting_pages),
            task.sitemap_url
        )
        
        pages_to_add = [page_info.to_page(task.sitemap_url, task.source) for page_info in nonexisting_pages]
        db_session.add_all(pages_to_add)
        
        task.source.page_count += len(pages_to_add)
    
    def __retrieve_pages(self, sitemap_url: str) -> list[SitemapPageInfo]:
        self._log(LogLevel.INFO, "Processing sitemap for url %s",sitemap_url)

        sitemap_response = self._http_client.get(sitemap_url)
        if sitemap_response.status_code > 200:
            self._log(
                LogLevel.ERROR,
                "Failed to get sitemap for url %s as server returned status code %d",
                sitemap_url,
                sitemap_response.status_code,
            )

            raise Exception(f"{sitemap_url} responded with unsuccessful status code {sitemap_response.status_code}")

        xml_content = BeautifulSoup(sitemap_response.content, "xml")

        page_infos = []

        for xml_entry in xml_content.find_all("url"):
            page_infos.append(SitemapPageInfo.from_xml(xml_entry))

        self._log(
            LogLevel.INFO,
            "Found %d page information in url %s",
            len(page_infos),
            sitemap_url,
        )

        return page_infos
