import asyncio
import codecs
import re
from dataclasses import dataclass
from urllib.parse import urljoin

import aiohttp
import mmh3

import scanner
from report import logger

_LINK_RES = (
    re.compile(r'<link[^>]+rel=["\'](?:shortcut )?icon["\'][^>]+href=["\']([^"\']+)["\']', re.IGNORECASE),
    re.compile(r'<link[^>]+href=["\']([^"\']+)["\'][^>]+rel=["\'](?:shortcut )?icon["\']', re.IGNORECASE),
)
_TIMEOUT = aiohttp.ClientTimeout(total=8.0)
_MAX_BYTES = 262144


@dataclass(frozen=True)
class Favicon:
    url: str
    source: str
    data: bytes

    @property
    def hash(self) -> int:
        return mmh3.hash(codecs.encode(self.data, "base64"))


async def _read_capped(resp: aiohttp.ClientResponse) -> bytes:
    raw = bytearray()
    async for chunk in resp.content.iter_chunked(8192):
        raw.extend(chunk)
        if len(raw) >= _MAX_BYTES:
            break
    return bytes(raw[:_MAX_BYTES])


async def _icon_link(session: aiohttp.ClientSession, base: str) -> str | None:
    try:
        async with session.get(base, ssl=False) as resp:
            html = (await _read_capped(resp)).decode("utf-8", errors="ignore")
            final_url = str(resp.url)
    except (aiohttp.ClientError, asyncio.TimeoutError):
        return None

    for pattern in _LINK_RES:
        if match := pattern.search(html):
            href = match.group(1)
            if not href.startswith("data:"):
                return urljoin(final_url, href)
    return None


async def _get_icon(session: aiohttp.ClientSession, url: str) -> bytes | None:
    try:
        async with session.get(url, ssl=False) as resp:
            if resp.status != 200:
                return None
            # Many servers answer 200 with an HTML "not found" page.
            if resp.content_type == "text/html":
                return None
            return await _read_capped(resp) or None
    except (aiohttp.ClientError, asyncio.TimeoutError):
        return None


async def _find_favicon(ctx: scanner.ScanContext, ip: str, port: int) -> Favicon | None:
    scheme = "https" if port == 443 else "http"
    base = f"{scheme}://{ip}:{port}"

    async with aiohttp.ClientSession(timeout=_TIMEOUT) as session:
        if ctx.event.is_set():
            return None

        candidates: list[tuple[str, str]] = []
        if link := await _icon_link(session, base):
            candidates.append((link, "HTML <link>"))
        candidates.append((f"{base}/favicon.ico", "Fallback /favicon.ico"))

        for url, source in candidates:
            if ctx.event.is_set():
                return None
            if data := await _get_icon(session, url):
                return Favicon(url=url, source=source, data=data)
    return None


async def http_grab_favicon(*, ctx: scanner.ScanContext, ip: str, port: int) -> None:
    if ctx.event.is_set():
        return
    fav = await _find_favicon(ctx, ip, port)
    if fav is None or ctx.event.is_set():
        return
    logger.sprint(f"{ip}:{port} -> {fav.source} ({fav.url}) | http.favicon.hash:{fav.hash}")
