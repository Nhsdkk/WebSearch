import uuid
from dataclasses import dataclass
from datetime import datetime
from enum import Enum
from typing import Optional

from sqlalchemy.ext.hybrid import hybrid_property
from sqlalchemy.orm import Mapped, mapped_column

from models.base_entity import BaseIdEntity


class ProcessingTaskStatus(Enum):
    PENDING = 0
    RUNNING = 1
    COMPLETED = 2
    FAILED = 3

MAX_FAIL_COUNT = 5

class ProcessingTask:
    status: Mapped[ProcessingTaskStatus] = mapped_column(nullable=False, default=ProcessingTaskStatus.PENDING)
    fail_count: Mapped[int] = mapped_column(nullable=False, default=0)
    last_error: Mapped[Optional[str]] = mapped_column(nullable=True)

    @hybrid_property
    def retryable(self) -> bool:
        return self.fail_count < MAX_FAIL_COUNT
    
    def retry_processing(self, exception: Exception):
        retryable = self.status == ProcessingTaskStatus.RUNNING or (self.status == ProcessingTaskStatus.FAILED and self.retryable)
        if not retryable:
            raise Exception("Cannot retry processing task.")    
        
        self.status = ProcessingTaskStatus.FAILED
        self.fail_count += 1
        self.last_error = str(exception)
        
    def complete(self) -> None:
        if self.status != ProcessingTaskStatus.RUNNING:
            raise Exception("Cannot complete a task that is not running.")
        
        self.status = ProcessingTaskStatus.COMPLETED
