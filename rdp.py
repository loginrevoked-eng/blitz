import sys
import asyncio
from aardwolf.connection import RDPConnection
from aardwolf.commons.iosettings import RDPIOSettings
from aardwolf.commons.target import RDPTarget
from aardwolf.commons.queuedata.constants import VIDEO_FORMAT
from aardwolf.protocol.x224.constants import SUPP_PROTOCOLS
from asyauth.common.credentials.ntlm import NTLMCredential
from asyauth.common.constants import asyauthSecret

# Shared base settings & dummy credentials across all tasks
BASE_SETTINGS = RDPIOSettings()
BASE_SETTINGS.channels = []
BASE_SETTINGS.clipboard_use_pyperclip = False
BASE_SETTINGS.video_width = 1280
BASE_SETTINGS.video_height = 720
BASE_SETTINGS.video_bpp_min = 15
BASE_SETTINGS.video_bpp_max = 32
BASE_SETTINGS.video_out_format = VIDEO_FORMAT.PIL

DUMMY_CREDS = NTLMCredential(secret="", username="", domain="", stype=asyauthSecret.PASS)


async def capture_preauth(target_ip: str, output_file: str = None, timeout: float = 4.0) -> bool:
    if output_file is None:
        output_file = f"{target_ip}.png"

    target = RDPTarget(
        ip=target_ip,
        port=3389,
        domain="FAKE",
        timeout=timeout,
        unsafe_ssl=True,
        autologon=False
    )

    for proto in (SUPP_PROTOCOLS.SSL, SUPP_PROTOCOLS.RDP):
        settings = BASE_SETTINGS.clone_for_connection()
        settings.supported_protocols = proto
        conn = RDPConnection(iosettings=settings, target=target, credentials=DUMMY_CREDS)

        try:
            _, err = await asyncio.wait_for(conn.connect(), timeout=timeout)
            if err is not None:
                continue

            # Poll every 100ms instead of sleeping a blind 3s; give 0.4s after first frame so UI finishes painting
            elapsed = 0.0
            while elapsed < 3.0:
                if conn.desktop_buffer_has_data:
                    await asyncio.sleep(0.4)
                    img = conn.get_desktop_buffer(VIDEO_FORMAT.PIL)
                    img.save(output_file, "PNG")
                    print(f"[+] {target_ip} -> {output_file}")
                    return True
                await asyncio.sleep(0.1)
                elapsed += 0.1

        except (asyncio.TimeoutError, Exception):
            pass
        finally:
            await conn.terminate()

    print(f"[-] {target_ip} failed (NLA enforced or unreachable)")
    return False


async def capture_many(target_ips: list[str], concurrency: int = 25):
    sem = asyncio.Semaphore(concurrency)

    async def worker(ip: str):
        async with sem:
            return await capture_preauth(ip)

    await asyncio.gather(*(worker(ip) for ip in target_ips))


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python rdp.py <ip1> [ip2 ip3 ...]")
        sys.exit(1)

    # Pass one or multiple IPs directly from CLI
    asyncio.run(capture_many(sys.argv[1:]))
