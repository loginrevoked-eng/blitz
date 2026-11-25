"""A lightweight scanner for fetch() and XMLHttpRequest.open() calls.

This is deliberately a small lexical parser rather than a full JavaScript
interpreter: it balances nested delimiters, skips comments and quoted strings
while locating calls, and retains the original argument expressions.
"""

from dataclasses import dataclass
import re

from .models import XHRFetch

_IDENTIFIER = re.compile(r"[A-Za-z_$][\w$]*")
_FETCH = re.compile(r"(?<![\w$])(?:window\s*\.\s*)?fetch\s*\(")
_XHR_CONSTRUCTOR = re.compile(
    r"\b(?:var|let|const)\s+([A-Za-z_$][\w$]*)\s*=\s*new\s+XMLHttpRequest\s*\(\s*\)"
)


@dataclass(frozen=True)
class _Call:
    start: int
    open_paren: int
    end: int


def _matching_paren(source: str, open_paren: int) -> int | None:
    depth = 0
    quote: str | None = None
    escaped = False
    line_comment = False
    block_comment = False
    i = open_paren
    while i < len(source):
        char = source[i]
        following = source[i + 1] if i + 1 < len(source) else ""
        if line_comment:
            if char == "\n":
                line_comment = False
        elif block_comment:
            if char == "*" and following == "/":
                block_comment = False
                i += 1
        elif quote:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == quote:
                quote = None
        elif char == "/" and following == "/":
            line_comment = True
            i += 1
        elif char == "/" and following == "*":
            block_comment = True
            i += 1
        elif char in "'\"`":
            quote = char
        elif char == "(":
            depth += 1
        elif char == ")":
            depth -= 1
            if depth == 0:
                return i + 1
        i += 1
    return None


def _code_view(source: str) -> str:
    """Blank comments and quoted text so call matching ignores examples in text."""
    chars = list(source)
    quote: str | None = None
    escaped = False
    line_comment = False
    block_comment = False
    i = 0
    while i < len(source):
        char = source[i]
        following = source[i + 1] if i + 1 < len(source) else ""
        if line_comment:
            if char == "\n":
                line_comment = False
            else:
                chars[i] = " "
        elif block_comment:
            if char == "*" and following == "/":
                chars[i] = chars[i + 1] = " "
                block_comment = False
                i += 1
            elif char != "\n":
                chars[i] = " "
        elif quote:
            if char != "\n":
                chars[i] = " "
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == quote:
                quote = None
        elif char == "/" and following == "/":
            chars[i] = chars[i + 1] = " "
            line_comment = True
            i += 1
        elif char == "/" and following == "*":
            chars[i] = chars[i + 1] = " "
            block_comment = True
            i += 1
        elif char in "'\"`":
            chars[i] = " "
            quote = char
        i += 1
    return "".join(chars)


def _split_arguments(source: str) -> list[str]:
    args: list[str] = []
    start = 0
    stack: list[str] = []
    pairs = {")": "(", "]": "[", "}": "{"}
    quote: str | None = None
    escaped = False
    line_comment = False
    block_comment = False
    i = 0
    while i < len(source):
        char = source[i]
        following = source[i + 1] if i + 1 < len(source) else ""
        if line_comment:
            if char == "\n":
                line_comment = False
        elif block_comment:
            if char == "*" and following == "/":
                block_comment = False
                i += 1
        elif quote:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == quote:
                quote = None
        elif char == "/" and following == "/":
            line_comment = True
            i += 1
        elif char == "/" and following == "*":
            block_comment = True
            i += 1
        elif char in "'\"`":
            quote = char
        elif char in "([{":
            stack.append(char)
        elif char in ")]}" and stack and stack[-1] == pairs[char]:
            stack.pop()
        elif char == "," and not stack:
            args.append(source[start:i].strip())
            start = i + 1
        i += 1
    last = source[start:].strip()
    if last or args:
        args.append(last)
    return args


def _calls(source: str, pattern: re.Pattern[str]):
    for match in pattern.finditer(source):
        open_paren = source.find("(", match.start(), match.end())
        end = _matching_paren(source, open_paren)
        if end is not None:
            yield _Call(match.start(), open_paren, end)


def _context(source: str, start: int, end: int, radius: int = 100) -> str:
    left = max(0, start - radius)
    right = min(len(source), end + radius)
    snippet = source[left:right].strip()
    return ("…" if left else "") + snippet + ("…" if right < len(source) else "")


def extract_xhr_fetch_calls(source: str, source_url: str = "<inline>") -> list[XHRFetch]:
    """Find fetch calls and ``open`` calls on variables created as XMLHttpRequest.

    Dynamic URLs are returned as their source expressions (for example
    ``baseUrl + path``), since evaluating JavaScript safely is out of scope.
    """
    code = _code_view(source)
    xhr_names = {
        match.group(1) for match in _XHR_CONSTRUCTOR.finditer(code)
    }
    patterns: list[tuple[str, re.Pattern[str]]] = [("fetch", _FETCH)]
    if xhr_names:
        names = "|".join(re.escape(name) for name in sorted(xhr_names, key=len, reverse=True))
        patterns.append(("xhr", re.compile(rf"\b(?:{names})\s*\.\s*open\s*\(")))

    found: list[XHRFetch] = []
    seen: set[int] = set()
    for kind, pattern in patterns:
        for call in _calls(code, pattern):
            if call.start in seen:
                continue
            seen.add(call.start)
            args = _split_arguments(source[call.open_paren + 1 : call.end - 1])
            if kind == "fetch":
                url_expression = args[0] if args else ""
                method_expression = None
                if len(args) > 1:
                    method_match = re.search(r"\bmethod\s*:\s*([^,}\n]+)", args[1])
                    if method_match:
                        method_expression = method_match.group(1).strip()
            else:
                method_expression = args[0] if args else None
                url_expression = args[1] if len(args) > 1 else ""
            found.append(
                XHRFetch(
                    kind=kind,
                    url=url_expression,
                    method_expression=method_expression,
                    source_url=source_url,
                    line=source.count("\n", 0, call.start) + 1,
                    context=_context(source, call.start, call.end),
                )
            )
    return sorted(found, key=lambda item: (item.line, item.source_url))
