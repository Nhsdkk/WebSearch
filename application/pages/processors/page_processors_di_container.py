from dependency_injector.providers import List, Factory

from application.pages.processors.izvestia_page_processor import IzvestiaPageProcessor
from application.pages.processors.ria_page_processor import RiaPageProcessor
from infrastructure import BaseDiContainer


class PageProcessorsDiContainer(BaseDiContainer):
    izvestia_page_processor = Factory(IzvestiaPageProcessor)
    ria_page_processor = Factory(RiaPageProcessor)
    
    page_processors = List(izvestia_page_processor, ria_page_processor)
