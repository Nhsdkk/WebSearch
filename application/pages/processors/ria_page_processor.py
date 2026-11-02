from bs4 import BeautifulSoup
from sqlalchemy import ColumnElement, func

from application.pages.processors.page_processor import PageProcessor, ProcessedPageDto
from models import Page


class RiaPageProcessor(PageProcessor):
    RIA_PAGE_PREFIX = 'https://ria.ru'

    @classmethod
    def page_filter(cls) -> ColumnElement[bool]:
        return func.lower(Page.url).startswith(cls.RIA_PAGE_PREFIX + "/", autoescape=True)
    
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
        body = html_content.select_one(".article__body")
        if body is None:
            raise ValueError("RIA article body was not found")

        # Match individual CSS classes; tracking classes can change independently.
        blocks = body.select(
            '.article__block[data-type="text"], .article__block[data-type="quote"]'
        )

        paragraphs = [" ".join(block.stripped_strings) for block in blocks]
        content = "\n".join(paragraph for paragraph in paragraphs if paragraph.strip())
        if not content.strip():
            raise ValueError("RIA article body contains no text or quotes")

        return content

    @staticmethod
    def __get_page_title(html_content: BeautifulSoup) -> str:
        return html_content.find_all("meta", attrs={"property": "og:title"})[0]["content"]
