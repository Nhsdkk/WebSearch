from dependency_injector.providers import List, Factory

from application.sitemap.url_filters.url_filter import IzvestiaContentPageFilter
from infrastructure import BaseDiContainer


class UrlFiltersDiContainer(BaseDiContainer):
    url_filters = List(
        Factory(IzvestiaContentPageFilter)
    )