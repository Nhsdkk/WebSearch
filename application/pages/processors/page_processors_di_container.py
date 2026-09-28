from dependency_injector.providers import List, Factory

from application.pages.processors.izvestia_page_processor import IzvestiaPageProcessor
from application.pages.processors.ria_page_processor import RiaPageProcessor
from infrastructure import BaseDiContainer
from infrastructure.infrastructure_injector import InfrastructureDiContainer


class PageProcessorsDiContainer(BaseDiContainer):
    izvestia_page_processor = Factory(IzvestiaPageProcessor, http_client=InfrastructureDiContainer.http_client)
    ria_page_processor = Factory(RiaPageProcessor, http_client=InfrastructureDiContainer.http_client)
    
    page_processors = List(izvestia_page_processor, ria_page_processor)
