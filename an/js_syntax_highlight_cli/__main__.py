"""Highlight one JavaScript file in the terminal."""

import argparse
from pathlib import Path

from . import highlight_js


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Print a JavaScript file with simple ANSI highlighting."
    )
    parser.add_argument("file", type=Path, help="JavaScript file to display")
    parser.add_argument("--no-color", action="store_true", help="print without ANSI colors")
    args = parser.parse_args()
    source = args.file.read_text(encoding="utf-8", errors="replace")
    print(highlight_js(source, enabled=not args.no_color), end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
