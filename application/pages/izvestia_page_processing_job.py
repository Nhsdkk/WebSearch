from dependency_injector.providers import Resource

from application.pages.page_processing_job import PageProcessingJob, PageProcessingJobConfig
from application.pages.processors.izvestia_page_processor import IzvestiaPageProcessor
from infrastructure import BaseDiContainer


class IzvestiaPageProcessingJobConfigProvider(BaseDiContainer):
    izvestia_page_processing_job_config = Resource(
        PageProcessingJobConfig,
        fail_timeout_seconds=BaseDiContainer.config.izvestia_page_processing_job_config.fail_timeout_seconds.as_float(),
        success_timeout_seconds=BaseDiContainer.config.izvestia_page_processing_job_config.success_timeout_seconds.as_float(),
        batch_size=BaseDiContainer.config.izvestia_page_processing_job_config.batch_size.as_int(),
    )

class IzvestiaPageProcessingJob(PageProcessingJob[IzvestiaPageProcessor]):
    @property
    def processor(self) -> IzvestiaPageProcessor:
        return self._page_processor
