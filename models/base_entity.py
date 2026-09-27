import uuid
from dataclasses import dataclass
from datetime import datetime, UTC

from sqlalchemy.orm import DeclarativeBase, mapped_column, Mapped


class BaseEntity(DeclarativeBase):    
    pass

class BaseIdEntity(DeclarativeBase):
    id: Mapped[uuid.UUID] = mapped_column(primary_key=True, default=uuid.uuid4)
    created_at: Mapped[datetime] = mapped_column(default=datetime.now(UTC))