"""processing_task_remove_running_status

Revision ID: 62c0d813ff81
Revises: 223999591ea1
Create Date: 2026-09-28 11:14:33.268490

"""
from typing import Sequence, Union

from alembic import op


# revision identifiers, used by Alembic.
revision: str = '62c0d813ff81'
down_revision: Union[str, Sequence[str], None] = '223999591ea1'
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None


def upgrade() -> None:
    op.execute(
        "UPDATE page_processing_tasks SET status = 'PENDING' WHERE status = 'RUNNING'"
    )
    op.execute(
        "UPDATE sitemap_processing_tasks SET status = 'PENDING' WHERE status = 'RUNNING'"
    )
    op.execute("ALTER TYPE processingtaskstatus RENAME TO processingtaskstatus_old")
    op.execute(
        "CREATE TYPE processingtaskstatus AS ENUM "
        "('PENDING', 'COMPLETED', 'FAILED', 'SKIPPED')"
    )
    for table in ("page_processing_tasks", "sitemap_processing_tasks"):
        op.execute(
            f"ALTER TABLE {table} ALTER COLUMN status TYPE processingtaskstatus "
            "USING status::text::processingtaskstatus"
        )
    op.execute("DROP TYPE processingtaskstatus_old")


def downgrade() -> None:
    op.execute("ALTER TYPE processingtaskstatus RENAME TO processingtaskstatus_new")
    op.execute(
        "CREATE TYPE processingtaskstatus AS ENUM "
        "('PENDING', 'RUNNING', 'COMPLETED', 'FAILED', 'SKIPPED')"
    )
    for table in ("page_processing_tasks", "sitemap_processing_tasks"):
        op.execute(
            f"ALTER TABLE {table} ALTER COLUMN status TYPE processingtaskstatus "
            "USING status::text::processingtaskstatus"
        )
    op.execute("DROP TYPE processingtaskstatus_new")
