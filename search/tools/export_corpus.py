#!/usr/bin/env python3
"""Read the LR1 stores and atomically export title/URL/body escaped TSV.

This is a data-transfer adapter for the existing Python crawler. Indexing and
search are performed exclusively by the C++ programs. No database is modified.
Environment files accept KEY=value and KEY: value; process variables override
file values. Values are literal (no expansion); matching outer quotes are removed.
"""

import argparse
import itertools
import json
import os
from pathlib import Path
import re
import sys
import tempfile
import time
import uuid


def read_environment(path: Path) -> dict[str, str]:
    values: dict[str, str] = {}
    if not path.exists():
        return values
    for number, line in enumerate(path.read_text(encoding="utf-8-sig").splitlines(), 1):
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        match = re.fullmatch(r"(?:export\s+)?([A-Za-z_][A-Za-z_0-9]*)\s*[:=]\s*(.*)", line)
        if match is None:
            raise ValueError(f"Invalid environment-file assignment at line {number}")
        key, value = match.groups()
        if len(value) >= 2 and value[0] in "\"'" and value[-1] == value[0]:
            value = value[1:-1]
        values[key] = value
    return values


def escape_field(value: str) -> str:
    return value.replace("\\", "\\\\").replace("\t", "\\t").replace("\n", "\\n").replace("\r", "\\r")


def valid_uuid(value: object) -> str | None:
    try:
        return str(uuid.UUID(str(value)))
    except (ValueError, TypeError, AttributeError):
        return None


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser(description=__doc__)
    result.add_argument("--env", type=Path, default=Path(__file__).resolve().parents[2] / ".env")
    result.add_argument("--host", help="Host for both databases unless overridden; default: environment or localhost")
    result.add_argument("--mongo-host")
    result.add_argument("--postgres-host")
    result.add_argument("--mongo-port", type=int)
    result.add_argument("--postgres-port", type=int)
    result.add_argument("--mongo-db")
    result.add_argument("--postgres-db")
    result.add_argument("--mongo-auth-source", default="admin")
    result.add_argument("--collection", default="page_content")
    result.add_argument("--output", required=True, type=Path)
    result.add_argument("--limit", type=int, help="Stop after this many exported documents")
    return result


def export(args: argparse.Namespace) -> dict:
    from pymongo import MongoClient
    import psycopg2

    if args.limit is not None and args.limit < 1:
        raise ValueError("--limit must be positive")
    environment = read_environment(args.env)
    environment.update(os.environ)

    def credential(name: str) -> str:
        value = environment.get(name)
        if not value:
            raise ValueError(f"Missing required credential: {name}")
        return value

    mongo_host = args.mongo_host or args.host or environment.get("MONGO_DB_HOST", "localhost")
    postgres_host = args.postgres_host or args.host or environment.get("DATABASE_HOST", "localhost")
    mongo_port = args.mongo_port or int(environment.get("MONGO_DB_PORT", "27017"))
    postgres_port = args.postgres_port or int(environment.get("DATABASE_PORT", "5432"))
    if not 1 <= mongo_port <= 65535 or not 1 <= postgres_port <= 65535:
        raise ValueError("Database ports must be between 1 and 65535")
    mongo_db = args.mongo_db or environment.get("MONGO_DB_JSON_DATA_DATABASE_NAME", "processed_content_db")
    postgres_db = args.postgres_db or environment.get("DATABASE_NAME", "robot_db")
    output = args.output.resolve()
    if output == args.env.resolve():
        raise ValueError("Output must differ from the environment file")
    output.parent.mkdir(parents=True, exist_ok=True)
    started = time.perf_counter()
    statistics = {"examined": 0, "exported": 0, "skipped_empty_content": 0, "skipped_missing_url": 0}
    mongo = None
    postgres = None
    temporary = None
    cursor = None
    try:
        # Credentials are passed separately: never construct or print a secret-bearing URI.
        mongo = MongoClient(
            host=mongo_host, port=mongo_port,
            username=credential("MONGO_DB_USERNAME"), password=credential("MONGO_DB_PASSWORD"),
            authSource=args.mongo_auth_source, directConnection=True,
            serverSelectionTimeoutMS=15000, connectTimeoutMS=15000, socketTimeoutMS=60000,
        )
        collection = mongo[mongo_db][args.collection]
        cursor = collection.find({}, {"page_id": 1, "title": 1, "text_content": 1, "url": 1}).sort("_id", 1).batch_size(500)
        with tempfile.NamedTemporaryFile(mode="w", encoding="utf-8", newline="\n", dir=output.parent,
                                         prefix=output.name + ".", suffix=".tmp", delete=False) as stream:
            temporary = Path(stream.name)
            while batch := list(itertools.islice(cursor, 500)):
                needed = []
                for document in batch:
                    if not isinstance(document.get("url"), str) or not document["url"].strip():
                        identifier = valid_uuid(document.get("page_id", document.get("_id")))
                        if identifier is not None:
                            needed.append(identifier)
                urls = {}
                if needed:
                    if postgres is None:
                        postgres = psycopg2.connect(
                            host=postgres_host, port=postgres_port, dbname=postgres_db,
                            user=credential("DATABASE_USERNAME"), password=credential("DATABASE_PASSWORD"),
                            connect_timeout=15, options="-c statement_timeout=30000",
                        )
                        postgres.set_session(readonly=True, autocommit=True)
                    with postgres.cursor() as lookup:
                        lookup.execute("SELECT id::text, url FROM pages WHERE id = ANY(%s::uuid[]) AND NOT is_removed", (needed,))
                        urls = dict(lookup.fetchall())
                for document in batch:
                    statistics["examined"] += 1
                    title, body = document.get("title"), document.get("text_content")
                    if not isinstance(title, str) or not title.strip() or not isinstance(body, str) or not body.strip():
                        statistics["skipped_empty_content"] += 1
                        continue
                    url = document.get("url")
                    if not isinstance(url, str) or not url.strip():
                        url = urls.get(valid_uuid(document.get("page_id", document.get("_id"))))
                    if not isinstance(url, str) or not url.strip():
                        statistics["skipped_missing_url"] += 1
                        continue
                    stream.write("\t".join(escape_field(field) for field in (title, url, body)) + "\n")
                    statistics["exported"] += 1
                    if args.limit is not None and statistics["exported"] >= args.limit:
                        break
                if statistics["examined"] and statistics["examined"] % 5000 < 500:
                    print(f"Exported {statistics['exported']} documents; examined {statistics['examined']}", file=sys.stderr)
                if args.limit is not None and statistics["exported"] >= args.limit:
                    break
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, output)
        temporary = None
        statistics["output_bytes"] = output.stat().st_size
        statistics["elapsed_seconds"] = round(time.perf_counter() - started, 6)
        return statistics
    finally:
        if cursor is not None:
            cursor.close()
        if postgres is not None:
            postgres.close()
        if mongo is not None:
            mongo.close()
        if temporary is not None:
            temporary.unlink(missing_ok=True)


def main() -> int:
    args = parser().parse_args()
    try:
        print(json.dumps(export(args), ensure_ascii=False, indent=2))
        return 0
    except (ValueError, ImportError) as error:
        print(f"Export failed: {error}", file=sys.stderr)
    except Exception as error:
        # Driver exception messages may include connection details. Keep credentials private.
        print(f"Export failed ({type(error).__name__}). Check database access, credentials, ports and output permissions.", file=sys.stderr)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
