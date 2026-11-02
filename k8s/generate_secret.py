#!/usr/bin/env python3
"""Generate k8s/secret.yaml from an env file.

The env file (k8s/secret.env by default, see k8s/config.env.example) holds one
KEY: value or KEY=value per line. Blank lines and lines starting with # are skipped.

CI has no env file on disk, so it passes the same contents in SECRET_ENV, which
takes precedence over the file.
"""
import argparse
import base64
import os
import re
import sys
from pathlib import Path

K8S_DIR = Path(__file__).resolve().parent
SECRET_NAME = "websearch-secrets"
NAMESPACE = "websearch"
ENV_LINE = re.compile(r"([A-Za-z_][A-Za-z0-9_]*)\s*[:=]\s*(.*)")


def parse_env(contents: str) -> dict[str, str]:
    values = {}
    for number, line in enumerate(contents.splitlines(), start=1):
        line = line.strip()
        if not line or line.startswith("#"):
            continue

        match = ENV_LINE.fullmatch(line)
        if match is None:
            # The line stays out of the message: it may hold a secret and this ends up in CI logs.
            raise ValueError(f"line {number}: expected KEY: value or KEY=value")

        key, value = match.groups()
        if len(value) >= 2 and value[0] == value[-1] and value[0] in "\"'":
            value = value[1:-1]
        values[key] = value

    # Applying a secret without keys would erase the ones already in the cluster.
    if not values:
        raise ValueError("no keys found")
    return values


def render_secret(values: dict[str, str]) -> str:
    lines = [
        "apiVersion: v1",
        "kind: Secret",
        "metadata:",
        f"  name: {SECRET_NAME}",
        f"  namespace: {NAMESPACE}",
        "type: Opaque",
        "data:",
    ]
    for key, value in sorted(values.items()):
        # Quoted because a bare empty value is YAML null, which kubectl apply treats as "delete this key".
        lines.append(f'  {key}: "{base64.b64encode(value.encode()).decode()}"')
    return "\n".join(lines) + "\n"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("env_file", nargs="?", type=Path, default=K8S_DIR / "secret.env")
    parser.add_argument("-o", "--output", type=Path, default=K8S_DIR / "secret.yaml")
    args = parser.parse_args()

    try:
        values = parse_env(os.environ.get("SECRET_ENV") or args.env_file.read_text())
    except (OSError, ValueError) as error:
        sys.exit(f"Cannot generate {args.output.name}: {error}")

    args.output.write_text(render_secret(values))
    print(f"Wrote {args.output} with keys: {', '.join(sorted(values))}")


if __name__ == "__main__":
    main()
