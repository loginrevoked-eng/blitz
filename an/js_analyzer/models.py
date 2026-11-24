"""Data structures returned by the analyzer."""

from dataclasses import dataclass
from typing import Literal


@dataclass(frozen=True)
class JSMention:
    """A JavaScript script reference or an inline script block."""

    type: Literal["inline", "url"]
    source_url: str
    script_index: int
    code: str | None = None
    url: str | None = None
    parent: "JSMention | None" = None


@dataclass(frozen=True)
class XHRFetch:
    """A network call found in JavaScript, with its original source context."""

    kind: Literal["fetch", "xhr"]
    url: str
    method_expression: str | None
    source_url: str
    line: int
    context: str

    @property
    def url_expression(self) -> str:
        """Compatibility alias for the complete URL expression string."""
        return self.url


@dataclass(frozen=True)
class AnalysisResult:
    """All extracted script mentions and network calls for one page."""

    page_url: str
    mentions: tuple[JSMention, ...]
    calls: tuple[XHRFetch, ...]
