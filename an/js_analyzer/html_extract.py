"""Extract external and inline script blocks from HTML."""

from html.parser import HTMLParser
from urllib.parse import urljoin

from .models import JSMention


class _ScriptParser(HTMLParser):
    def __init__(self, page_url: str) -> None:
        super().__init__(convert_charrefs=True)
        self.page_url = page_url
        self.mentions: list[JSMention] = []
        self._script_index = 0
        self._inside_script = False
        self._inline_parts: list[str] = []

    def handle_starttag(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
        if tag.lower() != "script":
            return
        attributes = dict(attrs)
        self._script_index += 1
        self._inside_script = True
        self._inline_parts = []
        src = attributes.get("src")
        if src:
            self.mentions.append(
                JSMention(
                    type="url",
                    source_url=self.page_url,
                    script_index=self._script_index,
                    url=urljoin(self.page_url, src),
                )
            )

    def handle_data(self, data: str) -> None:
        if self._inside_script:
            self._inline_parts.append(data)

    def handle_endtag(self, tag: str) -> None:
        if tag.lower() != "script" or not self._inside_script:
            return
        code = "".join(self._inline_parts)
        if code.strip():
            self.mentions.append(
                JSMention(
                    type="inline",
                    source_url=self.page_url,
                    script_index=self._script_index,
                    code=code,
                )
            )
        self._inside_script = False
        self._inline_parts = []


def extract_js_mentions(html: str, page_url: str) -> list[JSMention]:
    """Return script ``src`` URLs and non-empty inline script text in document order."""
    parser = _ScriptParser(page_url)
    parser.feed(html)
    parser.close()
    return parser.mentions
