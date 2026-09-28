"""processing_task_add_skipped_status

Revision ID: 223999591ea1
Revises: a0b1d75a0856
Create Date: 2026-09-28 10:43:17.519447

"""
from typing import Sequence, Union

from alembic import op


# revision identifiers, used by Alembic.
revision: str = '223999591ea1'
down_revision: Union[str, Sequence[str], None] = 'a0b1d75a0856'
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None


def upgrade() -> None:
    """Upgrade schema."""
    with op.get_context().autocommit_block():
        op.execute(
            "ALTER TYPE processingtaskstatus "
            "ADD VALUE IF NOT EXISTS 'SKIPPED'"
        )


def downgrade() -> None:
    """Downgrade schema."""
    op.execute(
        "UPDATE page_processing_tasks "
        "SET status = 'FAILED' "
        "WHERE status = 'SKIPPED'"
    )
    op.execute(
        "UPDATE sitemap_processing_tasks "
        "SET status = 'FAILED' "
        "WHERE status = 'SKIPPED'"
    )
    op.execute(
        "ALTER TYPE processingtaskstatus "
        "RENAME TO processingtaskstatus_old"
    )
    op.execute(
        "CREATE TYPE processingtaskstatus AS ENUM "
        "('PENDING', 'RUNNING', 'COMPLETED', 'FAILED')"
    )
    op.execute(
        "ALTER TABLE page_processing_tasks "
        "ALTER COLUMN status TYPE processingtaskstatus "
        "USING status::text::processingtaskstatus"
    )
    op.execute(
        "ALTER TABLE sitemap_processing_tasks "
        "ALTER COLUMN status TYPE processingtaskstatus "
        "USING status::text::processingtaskstatus"
    )
    op.execute("DROP TYPE processingtaskstatus_old")
