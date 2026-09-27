import uuid
from enum import Enum

from sqlalchemy import ForeignKey
from sqlalchemy.orm import Mapped, mapped_column, Relationship

from models import Source
from models.base_entity import BaseIdEntity
from models.processing_task import ProcessingTask


class SitemapType(Enum):
    GLOBAL = 0
    LOCAL = 1

class SitemapProcessingTask(ProcessingTask, BaseIdEntity):
    __tablename__ = 'sitemap_processing_tasks'
    
    sitemap_url: Mapped[str] = mapped_column(nullable=False)
    source : Mapped[Source] = Relationship(back_populates="sitemap_processing_tasks")
    source_id : Mapped[uuid.UUID] = mapped_column(ForeignKey("sources.id", ondelete="CASCADE"), nullable=False)
    type: Mapped[SitemapType] = mapped_column(nullable=False)
