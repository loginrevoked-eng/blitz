"""A tiny ANSI highlighter for JavaScript snippets in terminal reports."""

from dataclasses import dataclass
import re
import textwrap

_RESET = "\033[0m"
_PATTERN = re.compile(
    r"(?P<comment>//[^\n]*|/\*[\s\S]*?\*/)"
    r"|(?P<string>'(?:\\.|[^'\\])*'|\"(?:\\.|[^\"\\])*\"|`(?:\\.|[^`\\])*`)"
    r"|(?P<number>\b\d+(?:\.\d+)?\b)"
    r"|(?P<keyword>\b(?:async|await|break|case|catch|class|const|continue|debugger|default|delete|do|else|export|extends|false|finally|for|from|function|if|import|in|instanceof|let|new|null|of|return|static|super|switch|this|throw|true|try|typeof|undefined|var|void|while|yield)\b)"
    r"|(?P<builtin>\b(?:fetch|XMLHttpRequest|Promise|JSON|Object|Array|String|Number|Math|Date|console|window|document)\b)"
)
_COLORS = {
    "comment": "\033[2;90m",
    "string": "\033[32m",
    "number": "\033[35m",
    "keyword": "\033[34m",
    "builtin": "\033[36m",
}


def highlight_js(source: str, *, enabled: bool = True) -> str:
    """Color a few common JavaScript tokens while leaving the source intact."""
    if not enabled:
        return source
    return _PATTERN.sub(
        lambda match: f"{_COLORS[match.lastgroup]}{match.group(0)}{_RESET}",
        source,
    )


@dataclass
class JSSyntaxLayout:
    """Simple display options for a JavaScript syntax window."""

    width: int = 88
    color: bool = True
    line_numbers: bool = True
    max_lines: int = 8
    title: str = "JavaScript context"

    def __post_init__(self) -> None:
        if self.width < 24:
            raise ValueError("layout width must be at least 24 columns")
        if self.max_lines < 1:
            raise ValueError("max_lines must be at least 1")


class JSSyntaxWindow:
    """Render a JavaScript snippet in a bordered, optionally colored TUI box."""

    def __init__(self, ctx_code: str, layout: JSSyntaxLayout | None = None) -> None:
        self.ctx_code = ctx_code
        self.layout = layout or JSSyntaxLayout()

    def render(self) -> str:
        layout = self.layout
        inner_width = layout.width - 4
        source_lines = self.ctx_code.splitlines() or [""]
        truncated = len(source_lines) > layout.max_lines
        source_lines = source_lines[: layout.max_lines]

        number_width = len(str(len(source_lines))) if layout.line_numbers else 0
        gutter = number_width + 3 if layout.line_numbers else 0
        code_width = max(1, inner_width - gutter - 1)

        rendered_lines: list[str] = []
        for line_number, source_line in enumerate(source_lines, start=1):
            wrapped = textwrap.wrap(
                source_line,
                width=code_width,
                break_long_words=True,
                break_on_hyphens=False,
                replace_whitespace=False,
                drop_whitespace=False,
            ) or [""]
            for part_index, part in enumerate(wrapped):
                if layout.line_numbers:
                    line_label = f"{line_number:>{number_width}} │ " if part_index == 0 else " " * (number_width + 3)
                else:
                    line_label = ""
                colored = highlight_js(part, enabled=layout.color)
                plain_length = len(line_label) + len(part)
                padding = " " * max(0, inner_width - plain_length)
                rendered_lines.append(f"│ {line_label}{colored}{padding} │")

        if truncated:
            marker = "… more lines omitted …"
            marker = marker[:inner_width]
            rendered_lines.append(f"│ {marker}{' ' * (inner_width - len(marker))} │")

        title = f" {layout.title} "
        title = title[: layout.width - 4]
        rule_width = layout.width - len(title) - 3
        top = f"╭─{title}{'─' * max(0, rule_width)}╮"
        bottom = f"╰{'─' * (layout.width - 2)}╯"
        return "\n".join([top, *rendered_lines, bottom])


__all__ = ["JSSyntaxLayout", "JSSyntaxWindow", "highlight_js"]
