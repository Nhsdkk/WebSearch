import uuid

from sqlalchemy import ForeignKey
from sqlalchemy.orm import Mapped, mapped_column, relationship

from models import Page
from models.base_entity import BaseIdEntity
from models.processing_task import ProcessingTask


class PageProcessingTask(ProcessingTask, BaseIdEntity):
    __tablename__ = 'page_processing_tasks'
    
    page: Mapped[Page] = relationship(back_populates="processing_tasks")
    page_id: Mapped[uuid.UUID] = mapped_column(ForeignKey("pages.id", ondelete="CASCADE"), nullable=False)

