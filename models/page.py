import uuid
from datetime import datetime
from typing import Optional, List

from sqlalchemy import ForeignKey
from sqlalchemy.orm import Mapped, mapped_column, relationship

from models.base_entity import BaseIdEntity
from models.source import Source


class Page(BaseIdEntity):
    __tablename__ = 'pages'
    
    url: Mapped[str] = mapped_column(nullable=False)
    last_task_processed_at: Mapped[Optional[datetime]] = mapped_column(nullable=True)
    sitemap_url: Mapped[Optional[str]] = mapped_column(nullable=True)

    source: Mapped[Source] = relationship()
    source_id: Mapped[uuid.UUID] = mapped_column(ForeignKey("sources.id", ondelete="CASCADE"), nullable=False)
    
    content_size_in_kbytes: Mapped[Optional[int]] = mapped_column(nullable=True)
    raw_size_in_kbytes: Mapped[Optional[int]] = mapped_column(nullable=True)
    
    processing_tasks: Mapped[List["PageProcessingTask"]] = relationship()