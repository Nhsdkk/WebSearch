from threading import Lock

from .background_worker_base import BackgroundWorkerBase, WorkerStatus
from ..logging import LogProducer

class WorkerManager(LogProducer):
    _lock: Lock
    _workers: list[BackgroundWorkerBase]

    def __init__(self, workers: list[BackgroundWorkerBase]) -> None:
        super().__init__()
        
        self._lock = Lock()
        self._workers = workers
            
    def run(self) -> None:
        for worker in self._workers:
            if worker.status is not WorkerStatus.Working: 
                worker.start()

    def _force_cleanup(self) -> None:
        self._lock.acquire(blocking=True)

        for worker in self._workers:
            if worker.status is not WorkerStatus.Working:
                self._logger.warning("Worker %s is not working. Skipping termination...", worker.name)
                continue
            
            worker.terminate()

        for worker in self._workers:
            worker.join()

        self._workers = []

        self._lock.release()

    def dispose(self) -> None:
        self._logger.info("Terminating workers...")
        self._force_cleanup()
        self._logger.info("Successfully terminated all workers")

    def __del__(self):
        self.dispose()
