"""add_on_delete_cascade

Revision ID: c11028ba0963
Revises: fa8edf8aa93f
Create Date: 2026-09-28 01:33:28.875766

"""
from typing import Sequence, Union

from alembic import op
import sqlalchemy as sa


# revision identifiers, used by Alembic.
revision: str = 'c11028ba0963'
down_revision: Union[str, Sequence[str], None] = 'fa8edf8aa93f'
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None


def upgrade() -> None:
    op.drop_constraint(op.f('page_processing_tasks_page_id_fkey'), 'page_processing_tasks', type_='foreignkey')
    op.create_foreign_key('page_processing_tasks_page_id_fkey', 'page_processing_tasks', 'pages', ['page_id'], ['id'], ondelete='CASCADE')
    op.drop_constraint(op.f('pages_source_id_fkey'), 'pages', type_='foreignkey')
    op.create_foreign_key('pages_source_id_fkey', 'pages', 'sources', ['source_id'], ['id'], ondelete='CASCADE')
    op.drop_constraint(op.f('sitemap_processing_tasks_source_id_fkey'), 'sitemap_processing_tasks', type_='foreignkey')
    op.create_foreign_key('sitemap_processing_tasks_source_id_fkey', 'sitemap_processing_tasks', 'sources', ['source_id'], ['id'], ondelete='CASCADE')


def downgrade() -> None:
    op.drop_constraint('sitemap_processing_tasks_source_id_fkey', 'sitemap_processing_tasks', type_='foreignkey')
    op.create_foreign_key(op.f('sitemap_processing_tasks_source_id_fkey'), 'sitemap_processing_tasks', 'sources', ['source_id'], ['id'])
    op.drop_constraint('pages_source_id_fkey', 'pages', type_='foreignkey')
    op.create_foreign_key(op.f('pages_source_id_fkey'), 'pages', 'sources', ['source_id'], ['id'])
    op.drop_constraint('page_processing_tasks_page_id_fkey', 'page_processing_tasks', type_='foreignkey')
    op.create_foreign_key(op.f('page_processing_tasks_page_id_fkey'), 'page_processing_tasks', 'pages', ['page_id'], ['id'])
