"""HTTP helpers for downloading pages and linked JavaScript."""

from urllib.request import Request, urlopen

from .models import JSMention

_USER_AGENT = "Mozilla/5.0 (compatible; JSAnalyzer/1.0)"


def download_text(url: str, *, timeout: float = 15.0) -> str:
    """Download a URL and decode its response using its declared charset."""
    request = Request(url, headers={"User-Agent": _USER_AGENT})
    with urlopen(request, timeout=timeout) as response:
        body = response.read()
        charset = response.headers.get_content_charset() or "utf-8"
    return body.decode(charset, errors="replace")


def download_mentions(
    mentions: list[JSMention] | tuple[JSMention, ...], *, timeout: float = 15.0
) -> list[JSMention]:
    """Download each URL mention and return it with its JavaScript source filled in."""
    downloaded: list[JSMention] = []
    for mention in mentions:
        if mention.type != "url":
            continue
        if mention.url is None:
            continue
        downloaded.append(
            JSMention(
                type="url",
                source_url=mention.source_url,
                script_index=mention.script_index,
                url=mention.url,
                code=download_text(mention.url, timeout=timeout),
            )
        )
    return downloaded
