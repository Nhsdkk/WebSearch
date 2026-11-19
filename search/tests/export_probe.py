"""Return values from the Python export helpers for the GoogleTest cases."""

from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from export_corpus import escape_field, read_environment, valid_uuid


def main() -> None:
    operation, *arguments = sys.argv[1:]
    if operation == "read_environment":
        path, *keys = arguments
        values = read_environment(Path(path))
        result = "\n".join(values[key] for key in keys)
    elif operation == "escape_field":
        result = escape_field(arguments[0])
    elif operation == "valid_uuid":
        result = valid_uuid(arguments[0]) or ""
    else:
        raise ValueError(f"Unknown probe operation: {operation}")
    sys.stdout.buffer.write(result.encode("utf-8"))


if __name__ == "__main__":
    main()
