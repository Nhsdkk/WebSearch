from dataclasses import dataclass
from datetime import datetime, UTC
from typing import Optional, Self

import requests
from bs4 import BeautifulSoup, Tag
from dependency_injector.providers import Resource
from dependency_injector.wiring import inject, Provide
from sqlalchemy import select, Sequence

from application.sitemap.query_extensions import SitemapQueryExtensions
from infrastructure import BackgroundWorkerBase, JobConfigBase, BaseDiContainer, PsqlDatabase
from infrastructure.infrastructure_injector import InfrastructureDiContainer
from infrastructure.logging.log_producer import LogLevel
from models import SitemapProcessingTask, ProcessingTaskStatus, SitemapType
from models.source import Source


@dataclass
class GlobalSitemapProcessingJobConfig(JobConfigBase):
    batch_size: int = 20

class GlobalSitemapConfigProvider(BaseDiContainer):
    global_sitemap_processing_job_config = Resource(
        GlobalSitemapProcessingJobConfig,
        fail_timeout_seconds=BaseDiContainer.config.global_sitemap_processing_job_config.fail_timeout_seconds.as_int(),
        success_timeout_seconds=BaseDiContainer.config.global_sitemap_processing_job_config.success_timeout_seconds.as_int(),
        batch_size=BaseDiContainer.config.global_sitemap_processing_job_config.batch_size.as_int(),
    )

@dataclass(frozen=True)
class LocalSitemapEntry:
    url: str
    last_modified_at: Optional[datetime]
    
    @classmethod
    def from_xml(cls, xml_data: Tag) -> Self:
        return cls(
            url=xml_data.loc.text,
            last_modified_at=datetime.strptime(xml_data.lastmod.text, '%Y-%m-%d') if xml_data.lastmod is not None else None,
        )

    def changed(self, last_fetched_at: Optional[datetime]) -> bool:
        if self.last_modified_at is None or last_fetched_at is None:
            return True
        
        return self.last_modified_at > last_fetched_at
    
    def to_processing_task(self, source: Source) -> SitemapProcessingTask:
        return SitemapProcessingTask(
            sitemap_url=self.url,
            source=source,
            source_id=source.id,
            type=SitemapType.LOCAL
        )

class GlobalSitemapProcessingJob(BackgroundWorkerBase):
    _batch_size: int
    _database: PsqlDatabase
    
    @inject
    def __init__(
            self,
            job_config: GlobalSitemapProcessingJobConfig = Provide[GlobalSitemapConfigProvider.global_sitemap_processing_job_config],
            database: PsqlDatabase = Provide[InfrastructureDiContainer.database]):
        super().__init__(job_config)

        self._database = database
        self._batch_size = job_config.batch_size

    def do_work(self) -> bool:
        with self._database.create_session() as db_session:
            self._log(
                LogLevel.INFO,
                "Processing next batch of global sitemaps (batch size = %d)",
                self._batch_size,
            )

            tasks: Sequence[SitemapProcessingTask] = db_session.scalars(
                (
                    select(SitemapProcessingTask)
                    .join(SitemapProcessingTask.source)
                    .filter(SitemapQueryExtensions.global_sitemap_processing_task())
                    .filter(SitemapQueryExtensions.pending_sitemap_process_task() | SitemapQueryExtensions.retryable_sitemap_process_task())
                    .order_by(SitemapProcessingTask.created_at.asc())
                    .with_for_update(of=SitemapProcessingTask, skip_locked=True)
                    .limit(self._batch_size)
                )
            ).all()

            if not tasks:
                self._log(LogLevel.WARNING, "No global sitemaps processing tasks are active. Skipping processing...")
                return False

            for task in tasks:
                try:
                    with db_session.begin_nested():
                        self._log(LogLevel.INFO, "Processing task %s", task.id)
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
                            local_sitemaps = self.get_sitemap(task.sitemap_url)
                            changed_sitemaps = [sitemap for sitemap in local_sitemaps if sitemap.changed(task.source.last_task_processed_at)]
                            self._log(
                                LogLevel.INFO,
                                "Found %d local changed sitemaps in url %s. Creating tasks and completing processing...",
                                len(changed_sitemaps),
                                task.source.url,
                            )
                            db_session.add_all([sitemap.to_processing_task(task.source) for sitemap in changed_sitemaps])
                            task.complete()
                            task.source.last_task_processed_at = datetime.now(UTC)
                except Exception as e:
                    self._log(LogLevel.EXCEPTION, "Failed to process task %s", task.id, exc_info=e)
                    task.retry_processing(e)

            db_session.commit()
                    
            return all([task.status == ProcessingTaskStatus.COMPLETED or task.status == ProcessingTaskStatus.SKIPPED for task in tasks])
        
    
    def get_sitemap(self, url: str) -> list[LocalSitemapEntry]:
        self._log(LogLevel.INFO, "Processing sitemap for url %s",url)

        headers = {
            'User-Agent': 'Mozilla/5.0 (Macintosh; Intel Mac OS X 10.15; rv:156.0) Gecko/20100101 Firefox/156.0'
        }

        sitemap_response = requests.get(url, headers=headers)
        if sitemap_response.status_code > 200:
            self._log(
                LogLevel.ERROR,
                "Failed to get sitemap for url %s as server returned status code %d",
                url,
                sitemap_response.status_code,
            )
            
            raise Exception(f"{url} responded with unsuccessful status code {sitemap_response.status_code}")
        
        xml_content = BeautifulSoup(sitemap_response.content, "xml")
        
        local_sitemaps = []
        
        for xml_entry in xml_content.find_all("sitemap"):
            local_sitemaps.append(LocalSitemapEntry.from_xml(xml_entry))
            
        self._log(
            LogLevel.INFO,
            "Found %d local sitemaps in url %s",
            len(local_sitemaps),
            url,
        )
            
        return local_sitemaps            
            
