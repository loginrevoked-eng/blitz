import threading
import time
from .ctx import SynScanContext, scan_stop, should_stop
from .logger import log
from .packets import EthernetFrame, build_syn
from .wpcap import open_handle, wpcap
from .models import Dest


def send_syn(handle, packet: EthernetFrame) -> bool:
    data = packet.to_bytes()
    if wpcap.pcap_sendpacket(handle, data, len(data)) != 0:
        log("send failed:", wpcap.pcap_geterr(handle).decode())
        return False
    return True

def scan_send_loop(ctx: SynScanContext) -> None:
    if not ctx.picker:
        return

    handle, err = open_handle(ctx.cfg)
    if not handle:
        log("sender open failed:", err)
        scan_stop(ctx)
        return

    failures = 0
    try:
        while not should_stop(ctx):
            addr = ctx.picker.pick()
            if not addr:
                break
            for port in ctx.ports:
                if should_stop(ctx):
                    break
                frame = build_syn(cfg=ctx.cfg, dest=Dest(ip=str(addr), port=port))
                if send_syn(handle, frame):
                    ctx.stats.sent += 1
                    failures = 0
                else:
                    # Npcap's send queue can fill up briefly under load,
                    # so only give up after a run of failures.
                    failures += 1
                    if failures >= 50:
                        log("too many consecutive send failures, stopping")
                        scan_stop(ctx)
                        break
                    time.sleep(0.01)
    finally:
        wpcap.pcap_close(handle)


def spawn_sender(ctx: SynScanContext) -> None:
    ctx.threads.send_thread = threading.Thread(
        target=scan_send_loop, kwargs={"ctx": ctx}, daemon=True
    )
    ctx.threads.send_thread.start()
