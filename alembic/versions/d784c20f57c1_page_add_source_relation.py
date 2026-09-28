"""page_add_source_relation

Revision ID: d784c20f57c1
Revises: 55921907a75e
Create Date: 2026-09-27 15:14:49.794950

"""
from typing import Sequence, Union

from alembic import op
import sqlalchemy as sa


# revision identifiers, used by Alembic.
revision: str = 'd784c20f57c1'
down_revision: Union[str, Sequence[str], None] = '55921907a75e'
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None


def upgrade() -> None:
    op.add_column('pages', sa.Column('source_id', sa.Uuid(), nullable=False))
    op.create_foreign_key('pages_source_id_fkey', 'pages', 'sources', ['source_id'], ['id'])


def downgrade() -> None:
    op.drop_constraint('pages_source_id_fkey', 'pages', type_='foreignkey')
    op.drop_column('pages', 'source_id')
