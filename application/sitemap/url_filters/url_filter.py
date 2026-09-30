from pathlib import Path
from urllib import parse
from abc import ABC, abstractmethod, abstractclassmethod

from infrastructure import LogProducer


class UrlFilter(LogProducer, ABC):
    @abstractclassmethod
    def can_apply(cls, url: str) -> bool:
        pass
    
    @abstractmethod
    def filter(self, url: str) -> bool:
        pass
    
class IzvestiaContentPageFilter(UrlFilter):
    IZVESTIA_URL_PREFIX = 'https://iz.ru'
    
    @classmethod
    def can_apply(cls, url: str) -> bool:
        return url.lower().startswith(cls.IZVESTIA_URL_PREFIX)
    
    def filter(self, url: str) -> bool:
        path = Path(parse.urlparse(url).path).parts[1:]
        filter_result = "video" not in path
        
        self._logger.info(f"Filtering url {url}. Filter result: {filter_result}")

        return filter_result
    
    