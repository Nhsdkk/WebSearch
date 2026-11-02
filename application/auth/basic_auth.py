from secrets import compare_digest

from fastapi import FastAPI, HTTPException, status
from fastapi.security import HTTPBasic, HTTPBasicCredentials
from starlette.requests import Request
from starlette.responses import JSONResponse
from starlette.types import ASGIApp, Receive, Scope, Send


class BasicAuth:
    def __init__(self, username: str, password: str) -> None:
        if not username or not password:
            raise ValueError("Basic auth credentials must not be empty")

        self._username = username.encode("utf-8")
        self._password = password.encode("utf-8")

    def verify(self, credentials: HTTPBasicCredentials) -> None:
        valid_username = compare_digest(credentials.username.encode("utf-8"), self._username)
        valid_password = compare_digest(credentials.password.encode("utf-8"), self._password)
        if not (valid_username and valid_password):
            raise HTTPException(
                status_code=status.HTTP_401_UNAUTHORIZED,
                detail="Invalid credentials",
                headers={"WWW-Authenticate": "Basic"},
            )


class BasicAuthMiddleware:
    def __init__(self, app: ASGIApp, auth: BasicAuth) -> None:
        self._app = app
        self._auth = auth
        self._security = HTTPBasic()

    async def __call__(self, scope: Scope, receive: Receive, send: Send) -> None:
        if scope["type"] != "http" or self.__is_health_path(scope["path"]):
            await self._app(scope, receive, send)
            return

        try:
            credentials = await self._security(Request(scope, receive=receive))
            if credentials is None:
                raise HTTPException(
                    status_code=status.HTTP_401_UNAUTHORIZED,
                    detail="Missing credentials",
                    headers={"WWW-Authenticate": "Basic"},
                )

            self._auth.verify(credentials)
        except HTTPException as exc:
            response = JSONResponse(
                {"detail": exc.detail},
                status_code=exc.status_code,
                headers=exc.headers,
            )
            await response(scope, receive, send)
            return

        await self._app(scope, receive, send)
        
    @staticmethod
    def __is_health_path(path: str) -> bool:
        return path in ("/healthz", "/readyz")


def add_basic_auth(app: FastAPI, auth: BasicAuth) -> None:
    app.add_middleware(BasicAuthMiddleware, auth=auth)

    original_openapi = app.openapi

    def openapi() -> dict:
        schema = original_openapi()
        schema.setdefault("components", {}).setdefault("securitySchemes", {})["HTTPBasic"] = {
            "type": "http",
            "scheme": "basic",
        }
        schema["security"] = [{"HTTPBasic": []}]
        return schema

    app.openapi = openapi
