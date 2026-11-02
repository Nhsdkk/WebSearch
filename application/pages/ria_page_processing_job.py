from dependency_injector.providers import Resource

from application.pages.page_processing_job import PageProcessingJob, PageProcessingJobConfig
from application.pages.processors.ria_page_processor import RiaPageProcessor
from infrastructure import BaseDiContainer


class RiaPageProcessingJobConfigProvider(BaseDiContainer):
    ria_page_processing_job_config = Resource(
        PageProcessingJobConfig,
        fail_timeout_seconds=BaseDiContainer.config.ria_page_processing_job_config.fail_timeout_seconds.as_float(),
        success_timeout_seconds=BaseDiContainer.config.ria_page_processing_job_config.success_timeout_seconds.as_float(),
        batch_size=BaseDiContainer.config.ria_page_processing_job_config.batch_size.as_int(),
    )

class RiaPageProcessingJob(PageProcessingJob[RiaPageProcessor]):
    @property
    def processor(self) -> RiaPageProcessor:
        return self._page_processor
