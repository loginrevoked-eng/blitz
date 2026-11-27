import asyncio

import scanner
from report import logger


async def banner_grab(*, ctx: scanner.ScanContext, ip: str, port: int) -> None:
    if ctx.event.is_set():
        return

    writer = None
    try:
        reader, writer = await asyncio.wait_for(
            asyncio.open_connection(ip, port), timeout=4.0
        )
        data = await asyncio.wait_for(reader.read(4096), timeout=0.005)
        if data:
            logger.sprint(f"{ip}:{port} -> {data!r}")
    except (OSError, asyncio.TimeoutError):
        pass
    finally:
        if writer is not None:
            writer.close()
            try:
                await writer.wait_closed()
            except OSError:
                pass
