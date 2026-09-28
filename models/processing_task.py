from enum import Enum
from typing import Optional

from sqlalchemy.ext.hybrid import hybrid_property
from sqlalchemy.orm import Mapped, mapped_column


class ProcessingTaskStatus(Enum):
    PENDING = 0
    COMPLETED = 2
    FAILED = 3
    SKIPPED = 4

MAX_FAIL_COUNT = 5

class ProcessingTask:
    status: Mapped[ProcessingTaskStatus] = mapped_column(nullable=False, default=ProcessingTaskStatus.PENDING)
    fail_count: Mapped[int] = mapped_column(nullable=False, default=0)
    last_error: Mapped[Optional[str]] = mapped_column(nullable=True)

    @hybrid_property
    def retryable(self) -> bool:
        return self.fail_count < MAX_FAIL_COUNT
    
    def retry_processing(self, exception: Exception):
        if not self._can_process():
            raise Exception("Cannot retry processing task.")    
        
        self.status = ProcessingTaskStatus.FAILED
        self.fail_count += 1
        self.last_error = str(exception)
        
    def complete(self) -> None:
        if not self._can_process():
            raise Exception("Cannot complete a task that is not pending or retryable.")
        
        self.status = ProcessingTaskStatus.COMPLETED
        
    def skip(self, reason: str) -> None:
        if not self._can_process():
            raise Exception("Cannot skip a task that is not pending or retryable.")
        
        self.status = ProcessingTaskStatus.SKIPPED
        self.last_error = reason

    def _can_process(self) -> bool:
        return self.status in (ProcessingTaskStatus.PENDING, ProcessingTaskStatus.FAILED) and self.retryable
