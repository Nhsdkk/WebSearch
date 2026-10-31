from fastapi import APIRouter


def create_health_router() -> APIRouter:
    router = APIRouter()

    @router.get("/health", include_in_schema=False)
    def get_health() -> dict[str, str]:
        return {"status": "ok"}

    return router
