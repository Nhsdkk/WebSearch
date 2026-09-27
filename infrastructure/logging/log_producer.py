import logging
from abc import ABC
from enum import Enum
from logging import Logger

logging.basicConfig(
    handlers=[logging.StreamHandler()],
    level=logging.INFO
)

class LogLevel(Enum):
    DEBUG = logging.DEBUG
    INFO = logging.INFO
    WARNING = logging.WARNING
    ERROR = logging.ERROR
    EXCEPTION = logging.ERROR
    CRITICAL = logging.CRITICAL


class LogProducer(ABC):
    _logger: Logger
    
    def __init__(self, *args, **kwargs) -> None:
        class_name = self.__class__.__name__
        self._logger = logging.getLogger(class_name)
