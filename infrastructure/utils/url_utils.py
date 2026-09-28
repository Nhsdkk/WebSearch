from urllib.parse import urlsplit, urlunsplit


class UrlUtils:
    @staticmethod
    def normalize_url(url: str) -> str:
        parts = urlsplit(url.strip())
        scheme = parts.scheme.lower()
        hostname = parts.hostname
        if scheme not in ("http", "https") or not hostname:
            raise ValueError(f"Invalid HTTP URL: {url}")

        if ":" in hostname:
            host = f"[{hostname.lower()}]"
        else:
            host = hostname.rstrip(".").encode("idna").decode("ascii").lower()
            if not host:
                raise ValueError(f"Invalid HTTP URL: {url}")

        port = parts.port
        default_port = 80 if scheme == "http" else 443
        if port is not None and port != default_port:
            host = f"{host}:{port}"

        return urlunsplit((scheme, host, parts.path or "/", parts.query, ""))
