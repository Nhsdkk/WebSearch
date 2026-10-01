from bs4 import BeautifulSoup

from application.pages.processors.page_processor import PageProcessor, ProcessedPageDto
from models import Page


class RiaPageProcessor(PageProcessor):
    RIA_PAGE_PREFIX = 'https://ria.ru'

    @classmethod
    def can_process(cls, page: Page) -> bool:
        return page.url.lower().startswith(cls.RIA_PAGE_PREFIX)
    
    def _process_page_content(self, html_content: BeautifulSoup, page: Page) -> ProcessedPageDto:
        page_content = self.__get_page_content(html_content)
        page_title = self.__get_page_title(html_content)
        
        return ProcessedPageDto(
            page_id=page.id,
            title=page_title,
            text_content=page_content,
            full_page_content=html_content
        )

    @staticmethod
    def __get_page_content(html_content: BeautifulSoup) -> str:
        content_block = html_content.find_all("div", attrs={"class": "article__body js-mediator-article mia-analytics"})[0]

        div_content_blocks = content_block.find_all("div", attrs={"class": ["article__block"]})
        text_content_blocks = [block for block in div_content_blocks if 'data-type' in block and block["data-type"] == "text" or block["data-type"] == "quote"]

        text_blocks = [block.text for block in text_content_blocks]
        return "\n".join(text_blocks)

    @staticmethod
    def __get_page_title(html_content: BeautifulSoup) -> str:
        return html_content.find_all("meta", attrs={"property": "og:title"})[0]["content"]