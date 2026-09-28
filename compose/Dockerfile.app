FROM ghcr.io/astral-sh/uv:python3.13-bookworm-slim

WORKDIR /app
ENV PYTHONUNBUFFERED=1
ENV PATH="/app/.venv/bin:$PATH"

COPY pyproject.toml uv.lock ./
RUN uv sync --frozen --no-dev --no-install-project

COPY main.py config.yaml ./
COPY application ./application
COPY infrastructure ./infrastructure
COPY models ./models

EXPOSE 8080
CMD ["python", "main.py"]
