import time
from abc import ABC, abstractmethod
from dataclasses import dataclass
from decimal import InvalidOperation
from enum import Enum
from threading import Thread
from types import TracebackType
from typing import Any, Optional, Mapping, Callable
from uuid import UUID, uuid4

from ..logging import LogProducer
from ..logging.log_producer import LogLevel


class WorkerStatus(Enum):
    Created = 0
    Working = 1
    Terminated = 2


@dataclass
class SharedObject[T]:
    value: T


@dataclass
class JobConfigBase:
    fail_timeout_seconds: int = 3
    success_timeout_seconds: int = 1


class BackgroundWorkerBase(LogProducer, Thread, ABC):
    _id: UUID
    _status: SharedObject[WorkerStatus]
    _base_job_config: JobConfigBase

    def __init__(
            self,
            job_config: JobConfigBase = JobConfigBase()):
        Thread.__init__(self)
        LogProducer.__init__(self)

        self._id = uuid4()
        self._base_job_config = job_config
        self._status = SharedObject(WorkerStatus.Created)

    @property
    def status(self) -> WorkerStatus:
        return self._status.value

    @status.setter
    def status(self, value: WorkerStatus):
        self._status.value = value

    @abstractmethod
    def do_work(self) -> bool:
        pass

    def start(self) -> None:
        if self.status is WorkerStatus.Working:
            raise InvalidOperation("can't start already started worker")

        self.status = WorkerStatus.Working
        super().start()

    def run(self) -> None:
        self._logger.info(
            "Started background worker with id = [%s]",
            self._id,
            extra=self._get_worker_info())

        while self.status != WorkerStatus.Terminated:
            self._logger.info("Status [%s]", self.status)
            try:
                self._logger.info(
                    "Starting to execute iteration on worker with id = [%s]",
                    self._id,
                    extra=self._get_worker_info())
                result = self.do_work()

                if not result:
                    self._logger.error(
                        "Worker with id = [%s] failed the iteration with error result. Next iteration will be in %d seconds",
                        self._id,
                        self._base_job_config.fail_timeout_seconds,
                        extra=self._get_iteration_info(result))
                    time.sleep(self._base_job_config.fail_timeout_seconds)
                    continue

                self._logger.info(
                    "Worker with id = [%s] finished the iteration successfully. Next iteration will be in %d seconds",
                    self._id,
                    self._base_job_config.success_timeout_seconds,
                    extra=self._get_iteration_info(result))
                time.sleep(self._base_job_config.success_timeout_seconds)
            except Exception as e:
                self._logger.exception(
                    "Worker with id = [%s] finished the iteration with exception. Next iteration will be in %d seconds",
                    self._id,
                    self._base_job_config.fail_timeout_seconds,
                    exc_info=e,
                    extra=self._get_iteration_info(success=False))
                time.sleep(self._base_job_config.fail_timeout_seconds)

    def _get_iteration_info(
            self,
            success: bool,
            exception: Optional[Exception] = None) -> dict[str, Any]:
        info = self._get_worker_info()
        info["success"] = success

        if exception is not None:
            info["exception"] = exception

        return info

    def _get_worker_info(self) -> dict[str, Any]:
        return {
            "id": self._id,
            "terminated": self.status,
            "success_timeout": self._base_job_config.success_timeout_seconds,
            "fail_timeout": self._base_job_config.fail_timeout_seconds,
        }

    def _get_logging_func(self, level: LogLevel) -> Callable:
        mapping = {
            LogLevel.DEBUG: self._logger.debug,
            LogLevel.INFO: self._logger.info,
            LogLevel.WARNING: self._logger.warning,
            LogLevel.ERROR: self._logger.error,
            LogLevel.EXCEPTION: self._logger.exception,
            LogLevel.CRITICAL: self._logger.critical,
        }
        
        return mapping[level]

    def _log(
        self,
        level: LogLevel,
        msg: object,
        *args: object,
        exc_info: None | bool | tuple[type[BaseException], BaseException, TracebackType | None] | tuple[None, None, None] | BaseException = None,
        stack_info: bool = False,
        stacklevel: int = 1,
        extra: Mapping[str, object] | None = None) -> None:
        logging_func = self._get_logging_func(level)
        logging_func(
            msg,
            *args,
            exc_info=exc_info,
            stack_info=stack_info,
            extra={
                **(extra if extra is not None else {}),
                **self._get_worker_info(),
            },
            stacklevel=stacklevel,
        )

    def terminate(self) -> None:
        if self.status is not WorkerStatus.Working:
            raise InvalidOperation("can't stop not working worker")

        self.status = WorkerStatus.Terminated

    def __del__(self) -> None:
        self._logger.info(
            "Terminating worker with id = [%s]",
            self._id,
            extra=self._get_worker_info())

        self.terminate()
        self.join()

        self._logger.info(
            "Worker with id = [%s] successfully terminated",
            self._id,
            extra=self._get_worker_info())
