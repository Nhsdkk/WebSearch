import uuid
from datetime import datetime, UTC

from sqlalchemy import false
from sqlalchemy.orm import DeclarativeBase, mapped_column, Mapped


class SoftDeleteMixin:
    is_removed: Mapped[bool] = mapped_column(nullable=False, default=False, server_default=false())


class BaseEntity(DeclarativeBase):
    pass

class BaseIdEntity(DeclarativeBase):
    id: Mapped[uuid.UUID] = mapped_column(primary_key=True, default=uuid.uuid4)
    created_at: Mapped[datetime] = mapped_column(default=datetime.now(UTC))
