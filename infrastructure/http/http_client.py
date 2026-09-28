from collections.abc import Mapping
from dataclasses import dataclass

import requests
from requests import Response


@dataclass(frozen=True)
class HttpClientConfig:
    user_agent: str
    connect_timeout_seconds: int
    read_timeout_seconds: int


class HttpClient:
    def __init__(self, config: HttpClientConfig) -> None:
        self._config = config

    def get(self, url: str, headers: Mapping[str, str] | None = None) -> Response:
        request_headers = {"User-Agent": self._config.user_agent, **(headers or {})}
        timeout = (self._config.connect_timeout_seconds, self._config.read_timeout_seconds)

        return requests.get(url, headers=request_headers, timeout=timeout)
