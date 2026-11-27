import asyncio
import re
import ssl

import scanner
from report import logger

_TITLE_RE = re.compile(rb"<title[^>]*>(.*?)</title>", re.IGNORECASE | re.DOTALL)
_SERVER_RE = re.compile(r"^Server:\s*(.+)$", re.IGNORECASE | re.MULTILINE)


async def _http_request(ctx: scanner.ScanContext, ip: str, port: int) -> None:
    if ctx.event.is_set():
        return

    tls = None
    if port == 443:
        tls = ssl.create_default_context()
        tls.check_hostname = False
        tls.verify_mode = ssl.CERT_NONE

    writer = None
    try:
        reader, writer = await asyncio.wait_for(
            asyncio.open_connection(
                ip,
                port,
                ssl=tls,
                server_hostname=ip if tls else None,
            ),
            timeout=4.0,
        )
        request = (
            "GET / HTTP/1.1\r\n"
            f"Host: {ip}\r\n"
            "User-Agent: Mozilla/5.0\r\n"
            "Connection: close\r\n\r\n"
        ).encode("ascii")
        writer.write(request)
        await writer.drain()

        raw = bytearray()
        while len(raw) < 16384:
            try:
                data = await asyncio.wait_for(reader.read(4096), timeout=4.0)
            except asyncio.TimeoutError:
                break
            if not data:
                break
            raw.extend(data[:16384 - len(raw)])
    except (OSError, asyncio.TimeoutError):
        return
    finally:
        if writer is not None:
            writer.close()
            try:
                await writer.wait_closed()
            except OSError:
                pass

    if not raw or ctx.event.is_set():
        return

    header_blob, _, body = bytes(raw).partition(b"\r\n\r\n")
    headers = header_blob.decode("latin-1", errors="replace")
    status_line = headers.splitlines()[0] if headers.splitlines() else "unknown"
    server_match = _SERVER_RE.search(headers)
    server = server_match.group(1).strip() if server_match else "unknown"

    title_match = _TITLE_RE.search(body)
    if title_match:
        title = re.sub(
            r"\s+", " ", title_match.group(1).decode("utf-8", errors="replace").strip()
        )
    else:
        title = ""

    title_part = f" | title={title!r}" if title else ""
    logger.sprint(f"{ip}:{port} -> {status_line} | server={server}{title_part}")


async def http_grab(*, ctx: scanner.ScanContext, ip: str, port: int) -> None:
    if ctx.event.is_set():
        return
    task = asyncio.create_task(_http_request(ctx, ip, port))
    await task
