from bs4 import BeautifulSoup
from sqlalchemy import ColumnElement, func

from application.pages.processors.page_processor import PageProcessor, ProcessedPageDto
from models import Page


class IzvestiaPageProcessor(PageProcessor):
    IZVESTIA_URL_PREFIX = 'https://iz.ru'

    @classmethod
    def page_filter(cls) -> ColumnElement[bool]:
        return func.lower(Page.url).startswith(cls.IZVESTIA_URL_PREFIX + "/", autoescape=True)

    def _process_page_content(self, html_content: BeautifulSoup, page: Page) -> ProcessedPageDto:
        page_content = self.__get_text_content(html_content)
        page_title = self.__get_page_title(html_content)
        
        return ProcessedPageDto(
            page_id=page.id,
            title=page_title,
            text_content=page_content,
            full_page_content=html_content,
        )

    @staticmethod
    def __get_text_content(http_content: BeautifulSoup) -> str:
        content_block = http_content.select_one(".text-article")
        if content_block is None:
            raise ValueError("Izvestia article body was not found")

        paragraphs = [block.get_text().strip() for block in content_block.find_all("p")]
        content = "\n".join(paragraph for paragraph in paragraphs if paragraph)
        if not content.strip():
            raise ValueError("Izvestia article body contains no text paragraphs")

        return content

    @staticmethod
    def __get_page_title(html_content: BeautifulSoup) -> str:
        return html_content.find_all("meta", attrs={"property": "og:title"})[0]["content"]
