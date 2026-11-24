"""Readable terminal report formatting."""

from .models import AnalysisResult, XHRFetch
from js_syntax_highlight_cli import JSSyntaxLayout, JSSyntaxWindow


class _Colors:
    RESET = "\033[0m"
    BOLD = "\033[1m"
    DIM = "\033[2m"
    CYAN = "\033[36m"
    BLUE = "\033[34m"
    GREEN = "\033[32m"
    YELLOW = "\033[33m"
    MAGENTA = "\033[35m"


def _paint(value: str, color: str, enabled: bool) -> str:
    if not enabled:
        return value
    return f"{color}{value}{_Colors.RESET}"


def _format_call(call: XHRFetch, index: int, color: bool) -> list[str]:
    kind_color = _Colors.GREEN if call.kind == "fetch" else _Colors.MAGENTA
    kind = _paint(call.kind.upper(), kind_color + _Colors.BOLD, color)
    lines = [f"  {index:02d}  {kind}  {_paint(call.url or '<missing URL>', _Colors.BOLD, color)}"]
    if call.method_expression:
        lines.append(f"      Method   {_paint(call.method_expression, _Colors.YELLOW, color)}")
    lines.append(
        f"      Source   {_paint(call.source_url, _Colors.CYAN, color)}:{call.line}"
    )
    window = JSSyntaxWindow(
        call.context,
        layout=JSSyntaxLayout(color=color, line_numbers=True, width=88),
    )
    lines.append("      Context")
    lines.extend(f"      {line}" for line in window.render().splitlines())
    return lines


def _lineage(mention) -> str:
    ancestors = []
    parent = mention.parent
    while parent is not None:
        ancestors.append(parent)
        parent = parent.parent
    ancestors.reverse()
    if not ancestors:
        return mention.source_url

    chain = [ancestors[0].source_url]
    for ancestor in ancestors:
        if ancestor.type == "url" and ancestor.url and chain[-1] != ancestor.url:
            chain.append(ancestor.url)
        elif ancestor.type == "inline":
            chain.append(f"inline #{ancestor.script_index}")
    if mention.type == "url" and mention.url and chain[-1] != mention.url:
        chain.append(mention.url)
    return " → ".join(chain)


def pretty_print(result: AnalysisResult, *, color: bool = True) -> str:
    """Format an analysis result as a colored, readable terminal report.

    Pass ``color=False`` when writing to a file or a terminal that does not
    support ANSI escape sequences.
    """
    title = _paint(" JAVASCRIPT ANALYSIS ", _Colors.BOLD + _Colors.CYAN, color)
    lines = [f"╭─{title}────────────────────────────────────────────"]
    lines.append(f"│ Page      {_paint(result.page_url, _Colors.BOLD, color)}")
    lines.append(f"│ Scripts   {len(result.mentions)}")
    lines.append(f"│ Calls     {len(result.calls)}")
    lines.append("├─ SCRIPTS ──────────────────────────────────────────")

    if result.mentions:
        for index, mention in enumerate(result.mentions, start=1):
            if mention.type == "url":
                kind = _paint("URL", _Colors.BLUE + _Colors.BOLD, color)
                detail = mention.url or "<missing URL>"
            else:
                kind = _paint("INLINE", _Colors.YELLOW + _Colors.BOLD, color)
                detail = f"inline JavaScript from {mention.source_url}"
            lines.append(f"│  {index:02d}  [{kind}]  {detail}")
            lines.append(f"│      via  {_lineage(mention)}")
    else:
        lines.append(f"│  {_paint('No script tags found.', _Colors.DIM, color)}")

    lines.append("├─ NETWORK CALLS ────────────────────────────────────")
    if result.calls:
        for index, call in enumerate(result.calls, start=1):
            lines.extend(f"│{line}" for line in _format_call(call, index, color))
    else:
        lines.append(f"│  {_paint('No fetch or XMLHttpRequest calls found.', _Colors.DIM, color)}")
    lines.append("╰────────────────────────────────────────────────────")
    return "\n".join(lines)
