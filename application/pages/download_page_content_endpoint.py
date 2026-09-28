from uuid import UUID

from fastapi import APIRouter, HTTPException, Response, status

from infrastructure import PsqlDatabase
from infrastructure.mongo import MongoRawDataDatabase
from models import Page


class PageNotFoundError(Exception):
    pass


class PageContentNotFoundError(Exception):
    pass


class DownloadPageContentHandler:
    def __init__(
        self,
        database: PsqlDatabase,
        raw_content_database: MongoRawDataDatabase,
    ) -> None:
        self.__database = database
        self.__raw_content_database = raw_content_database

    def handle(self, page_id: UUID) -> bytes:
        with self.__database.create_session() as db_session:
            if db_session.get(Page, page_id) is None:
                raise PageNotFoundError

        content = self.__raw_content_database.get_file(page_id)
        if content is None:
            raise PageContentNotFoundError

        return content


def create_download_page_content_router(
    handler: DownloadPageContentHandler,
) -> APIRouter:
    router = APIRouter(tags=["pages"])

    @router.get(
        "/pages/{page_id}/content",
        response_class=Response,
        responses={
            status.HTTP_200_OK: {
                "description": "Original page HTML",
                "content": {"text/html": {}},
            },
            status.HTTP_404_NOT_FOUND: {
                "description": "Page or full content not found",
            },
        },
    )
    def download_page_content(page_id: UUID) -> Response:
        try:
            content = handler.handle(page_id)
        except PageNotFoundError as error:
            raise HTTPException(
                status_code=status.HTTP_404_NOT_FOUND,
                detail="Page not found",
            ) from error
        except PageContentNotFoundError as error:
            raise HTTPException(
                status_code=status.HTTP_404_NOT_FOUND,
                detail="Full page content not found",
            ) from error

        return Response(
            content=content,
            media_type="text/html",
            headers={
                "Content-Disposition": f'attachment; filename="{page_id}.html"',
            },
        )

    return router
