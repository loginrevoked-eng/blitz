import asyncio
from pathlib import Path

from aardwolf.commons.iosettings import RDPIOSettings
from aardwolf.commons.queuedata.constants import VIDEO_FORMAT
from aardwolf.commons.target import RDPTarget
from aardwolf.connection import RDPConnection
from aardwolf.protocol.x224.constants import SUPP_PROTOCOLS
from asyauth.common.constants import asyauthSecret
from asyauth.common.credentials.ntlm import NTLMCredential

import scanner
from report import logger

_BASE_SETTINGS = RDPIOSettings()
_BASE_SETTINGS.channels = []
_BASE_SETTINGS.clipboard_use_pyperclip = False
_BASE_SETTINGS.video_width = 1280
_BASE_SETTINGS.video_height = 720
_BASE_SETTINGS.video_bpp_min = 15
_BASE_SETTINGS.video_bpp_max = 32
_BASE_SETTINGS.video_out_format = VIDEO_FORMAT.PIL

_DUMMY_CREDS = NTLMCredential(
    secret="",
    username="",
    domain="",
    stype=asyauthSecret.PASS,
)

from collections import Counter


def is_solid_screen(raw_bgra: bytes, bytes_per_pixel: int = 4, step: int = 64, threshold: float = 0.90) -> bool:
    """Checks if a raw framebuffer is mostly one solid color by sampling every Nth pixel."""
    stride = bytes_per_pixel * step
    # Slice out 1 pixel every `step` pixels (fast C-level memoryview slicing)
    mv = memoryview(raw_bgra)
    samples = [bytes(mv[i : i + 3]) for i in range(0, len(mv) - 3, stride)]

    if not samples:
        return True

    _, most_common_count = Counter(samples).most_common(1)[0]
    return (most_common_count / len(samples)) >= threshold

async def _rdp_capture(
    ctx: scanner.ScanContext,
    ip: str,
    port: int,
    output_dir: str = "./rdp-screenshots",
) -> None:
    if ctx.event.is_set():
        return

    target = RDPTarget(
        ip=ip,
        port=port,
        domain="FAKE",
        timeout=4.0,
        unsafe_ssl=True,
        autologon=False,
    )

    logger.sprint(f"attempting to grab logon screenshot of {ip}...")

    for proto in (SUPP_PROTOCOLS.SSL, SUPP_PROTOCOLS.RDP):
        if ctx.event.is_set():
            return

        settings = _BASE_SETTINGS.clone_for_connection()
        settings.supported_protocols = proto
        conn = RDPConnection(
            iosettings=settings,
            target=target,
            credentials=_DUMMY_CREDS,
        )

        try:
            _, err = await asyncio.wait_for(conn.connect(), timeout=4.0)
            if err is not None:
                continue

            elapsed = 0.0
            while elapsed < 3.0 and not ctx.event.is_set():
                if conn.desktop_buffer_has_data:
                    await asyncio.sleep(0.4)
                    if ctx.event.is_set():
                        return

                    img = conn.get_desktop_buffer(VIDEO_FORMAT.PIL)
                    out_path = Path(output_dir) / f"{ip}.png"
                    await asyncio.to_thread(img.save, out_path, "PNG")
                    logger.sprint(f"{ip}:{port} -> saved {out_path} (proto={proto.name})")
                    return

                await asyncio.sleep(0.1)
                elapsed += 0.1

        except (OSError, asyncio.TimeoutError, Exception):
            pass
        finally:
            try:
                await conn.terminate()
            except Exception:
                pass


async def rdp_grab_screenshot(*, ctx: scanner.ScanContext, ip: str, port: int) -> None:
    if ctx.event.is_set():
        return
    await _rdp_capture(ctx, ip, port)
