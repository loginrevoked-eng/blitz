import threading
from ctypes import POINTER, byref, c_ubyte, string_at

from .ctx import SynScanContext, scan_stop, should_stop
from .logger import log
from .models import Dest
from .packets import EthernetFrame, verify_reply
from .wpcap import bpf_program, open_handle, pcap_pkthdr, wpcap


def set_capture_filter(handle, src_port: int) -> None:
    """Only let replies to our source port reach Python (kernel-side BPF)."""
    prog = bpf_program()
    expr = f"tcp dst port {src_port}".encode()
    if wpcap.pcap_compile(handle, byref(prog), expr, 1, 0xFFFFFFFF) != 0:
        log("filter compile failed:", wpcap.pcap_geterr(handle).decode())
        return
    try:
        if wpcap.pcap_setfilter(handle, byref(prog)) != 0:
            log("setfilter failed:", wpcap.pcap_geterr(handle).decode())
    finally:
        wpcap.pcap_freecode(byref(prog))


def enqueue_or_drop(ctx: SynScanContext, frame: EthernetFrame | None) -> None:
    if frame is None:
        return
    ip, tcp = frame.ip, frame.ip.tcp
    if tcp.dst_port != ctx.cfg.sender.src_port:
        return
    if not verify_reply(frame, ctx.cfg.seq_key):
        return
    ctx.stats.inc_responses()

    src = Dest(ip=ip.src_ip, port=tcp.src_port)
    if tcp.flags & 0x12 == 0x12:
        ctx.open.put(src)
        ctx.stats.inc_opens()
        flags = "open (SYN-ACK)"
    elif tcp.flags & 0x04:
        ctx.alive.add(src)
        flags = "closed (RST)"
    else:
        flags = f"other flags 0x{tcp.flags:02x}"
    log(f"{src} -> {flags}")


def pcap_recv(pcap_t) -> bytes | None:
    hdr = POINTER(pcap_pkthdr)()
    pkt = POINTER(c_ubyte)()

    r = wpcap.pcap_next_ex(pcap_t, byref(hdr), byref(pkt))
    if r < 0:
        raise RuntimeError(wpcap.pcap_geterr(pcap_t).decode())
    if r == 0:  # timeout
        return None
    return string_at(pkt, hdr.contents.caplen)


def open_receiver_handle(ctx: SynScanContext):
    pcap_t, err = open_handle(ctx.cfg)
    if not pcap_t:
        log("receiver open failed:", err)
        scan_stop(ctx)
        return None

    set_capture_filter(pcap_t, ctx.cfg.sender.src_port)
    return pcap_t


def process_next_frame(
    ctx: SynScanContext, pcap_t, *, stop_on_timeout: bool = False
) -> bool:
    try:
        raw = pcap_recv(pcap_t)
    except RuntimeError as e:
        log("capture error:", e)
        scan_stop(ctx)
        return False

    if raw is None:
        return not stop_on_timeout

    reply = EthernetFrame.from_wire(raw)
    enqueue_or_drop(ctx, reply)
    return True


def scan_recv_loop(ctx: SynScanContext) -> None:
    pcap_t = open_receiver_handle(ctx)
    if not pcap_t:
        return
    try:
        while not should_stop(ctx):
            if not process_next_frame(ctx, pcap_t):
                break
        if should_stop(ctx):
            # Drain packets already queued by Npcap, stopping after one timeout.
            while process_next_frame(ctx, pcap_t, stop_on_timeout=True):
                pass
    finally:
        wpcap.pcap_close(pcap_t)


def spawn_receiver(ctx: SynScanContext) -> None:
    ctx.threads.recv_thread = threading.Thread(
        target=scan_recv_loop,
        kwargs={"ctx": ctx},
        daemon=True,
    )
    ctx.threads.recv_thread.start()
