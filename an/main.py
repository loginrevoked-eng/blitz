"""Command-line entry point for the JavaScript analyzer."""

import argparse
import sys
from urllib.error import HTTPError, URLError

from js_analyzer import analyze_url, pretty_print


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Find fetch and XMLHttpRequest calls in a page's JavaScript."
    )
    parser.add_argument(
        "url_argument",
        nargs="?",
        metavar="URL",
        help="HTTP or HTTPS page URL to analyze",
    )
    parser.add_argument(
        "--url",
        dest="url_option",
        metavar="URL",
        help="HTTP or HTTPS page URL to analyze",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=15.0,
        help="per-request timeout in seconds (default: 15)",
    )
    parser.add_argument(
        "--depth",
        type=int,
        default=2,
        help="script levels to download (1 follows HTML scripts; default: 2)",
    )
    parser.add_argument(
        "--no-color",
        action="store_true",
        help="disable ANSI colors in the report",
    )
    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    if args.url_argument and args.url_option:
        parser.error("provide the URL either positionally or with --url, not both")
    url = args.url_option or args.url_argument
    if not url:
        parser.error("a URL is required; pass it as URL or --url URL")
    if args.timeout <= 0:
        print("error: --timeout must be greater than zero", file=sys.stderr)
        return 2
    if args.depth < 0:
        print("error: --depth must be zero or greater", file=sys.stderr)
        return 2

    try:
        result = analyze_url(url, timeout=args.timeout, depth=args.depth)
    except (HTTPError, URLError, TimeoutError, ValueError) as exc:
        print(f"error: could not analyze {url}: {exc}", file=sys.stderr)
        return 1

    print(pretty_print(result, color=not args.no_color))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
