from dataclasses import dataclass
from datetime import datetime
from typing import Optional

from sqlalchemy.orm import Mapped, mapped_column, relationship

from models.base_entity import BaseIdEntity


@dataclass
class Source(BaseIdEntity):
    __tablename__ = 'sources'
    
    url: Mapped[str] = mapped_column(nullable=False)
    refetch_seconds: Mapped[int] = mapped_column(nullable=False)
    global_sitemap_url: Mapped[Optional[str]] = mapped_column(nullable=True)
    max_pages_count: Mapped[Optional[int]] = mapped_column(nullable=True)
    page_count: Mapped[int] = mapped_column(nullable=False, default=0)
    sitemap_processing_tasks: Mapped[list["SitemapProcessingTask"]] = relationship()
    last_task_processed_at: Mapped[Optional[datetime]] = mapped_column(nullable=True)
