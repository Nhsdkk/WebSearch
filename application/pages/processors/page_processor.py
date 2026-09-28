import uuid
from abc import ABC, abstractmethod, abstractclassmethod
from dataclasses import dataclass
from datetime import datetime
from typing import Optional

from bs4 import BeautifulSoup

from infrastructure import LogProducer
from infrastructure.http import HttpClient
from models import Page

@dataclass
class ProcessedPageDto:
    page_id: uuid.UUID
    title: str
    text_content: str
    full_page_content: BeautifulSoup

class PageProcessor(LogProducer):
    def __init__(self, http_client: HttpClient) -> None:
        super().__init__()
        self._http_client = http_client

    @abstractclassmethod
    def can_process(cls, page: Page) -> bool:
        pass
    
    @abstractmethod
    def _process_page_content(self, html_content: BeautifulSoup, page: Page) -> ProcessedPageDto:
        pass
        
    def _retrieve_page_content(self, page: Page) -> Optional[BeautifulSoup]:
        self._logger.info("Attempting to retrieve page %s content...", page.url)
        
        reference_time = datetime.strftime(page.last_task_processed_at,"%a %d %b %Y %H:%M:%S %z") if page.last_task_processed_at is not None else None
        headers = {'If-Modified-Since': reference_time} if reference_time is not None else None
        
        response = self._http_client.get(page.url, headers=headers)
        if response.status_code == 304:
            self._logger.warning(
                "Page %s has not been modified since %s. Returning...",
                page.url,
                reference_time,
            )

            return None
        
        if response.status_code >= 300:
            raise Exception(f"Failed to retrieve page {page.url} content as server returned status code {response.status_code}")
        
        return BeautifulSoup(response.content, "html.parser")
        
        
    def try_process_page(self, page: Page) -> Optional[ProcessedPageDto]:
        html_content = self._retrieve_page_content(page)
        
        if html_content is None:
            return None
        
        return self._process_page_content(html_content, page)
