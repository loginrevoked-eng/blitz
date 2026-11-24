"""Find simple static and dynamic JavaScript module imports."""

import re
from urllib.parse import urljoin, urlsplit

from .js_calls import _code_view

_IMPORT_EXPORT = re.compile(r"\b(import|export)\b")
_FROM = re.compile(r"\bfrom\b")


def _skip_space(source: str, position: int) -> int:
    while position < len(source) and source[position].isspace():
        position += 1
    return position


def _read_string(source: str, position: int) -> tuple[str, int] | None:
    if position >= len(source) or source[position] not in "'\"`":
        return None
    quote = source[position]
    start = position + 1
    position += 1
    escaped = False
    while position < len(source):
        char = source[position]
        if escaped:
            escaped = False
        elif char == "\\":
            escaped = True
        elif char == quote:
            value = source[start:position]
            if quote == "`" and "${" in value:
                return None
            return value, position + 1
        position += 1
    return None


def _module_specifiers(source: str) -> list[str]:
    code = _code_view(source)
    specifiers: list[str] = []
    for match in _IMPORT_EXPORT.finditer(code):
        keyword = match.group(1)
        if match.start() and code[match.start() - 1] in ".$":
            continue
        position = _skip_space(source, match.end())

        if keyword == "import" and position < len(source) and source[position] == "(":
            literal = _read_string(source, _skip_space(source, position + 1))
            if literal:
                specifiers.append(literal[0])
            continue

        if keyword == "import":
            literal = _read_string(source, position)
            if literal:
                specifiers.append(literal[0])
                continue

        statement_end = code.find(";", match.end())
        if statement_end == -1:
            statement_end = min(len(code), match.end() + 1200)
        from_match = _FROM.search(code, position, statement_end)
        if not from_match:
            continue
        literal = _read_string(source, _skip_space(source, from_match.end()))
        if literal:
            specifiers.append(literal[0])
    return specifiers


def extract_module_urls(source: str, source_url: str) -> list[str]:
    """Resolve URL-like import/export specifiers against their source script."""
    urls: list[str] = []
    for specifier in _module_specifiers(source):
        parsed = urlsplit(specifier)
        url_like = (
            specifier.startswith((".", "/", "//"))
            or parsed.scheme in {"http", "https"}
            or parsed.path.lower().endswith((".js", ".mjs", ".cjs"))
        )
        if not url_like:
            continue
        resolved = urljoin(source_url, specifier)
        if urlsplit(resolved).scheme in {"http", "https"} and resolved not in urls:
            urls.append(resolved)
    return urls
