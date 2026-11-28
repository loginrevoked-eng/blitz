import signal
import threading
import time

from .ctx import SynScanContext, scan_stop
from .logger import enable_ansi, log, set_status_line
from .picker import Picker
from .recv import spawn_receiver
from .send import spawn_sender
from .wpcap import require_wpcap

_DEFAULT_EXCLUDED_PORTS = frozenset({113, 2000, 5060})


def parse_ports(text: str, exclude: set[int] | None = None) -> list[int]:
    if exclude is None:
        exclude = set(_DEFAULT_EXCLUDED_PORTS)

    cleaned = text.strip().lower()
    if cleaned == "all":
        ports = set(range(1, 65536))
    else:
        ports = set()
        for part in cleaned.split(","):
            part = part.strip()
            if not part:
                continue
            if "-" in part:
                lo_str, hi_str = part.split("-", 1)
                lo, hi = int(lo_str), int(hi_str)
                if not (1 <= lo <= hi <= 65535):
                    raise ValueError(f"Invalid port range: {part!r}")
                ports.update(range(lo, hi + 1))
            else:
                port = int(part)
                if not (1 <= port <= 65535):
                    raise ValueError(f"Invalid port: {port}")
                ports.add(port)

    ports -= exclude
    return sorted(ports)




def summarize(ctx: SynScanContext) -> None:
    elapsed = time.perf_counter() - ctx.stats.started
    state = "interrupted" if ctx.stats.interrupted else "finished"
    log(
        f"scan {state} in {elapsed:.2f} seconds, {ctx.stats.sent} SYNs sent, "
        f"{ctx.stats.get_responses()} responses, {ctx.stats.get_opens()} open"
    )


def stats_loop(ctx: SynScanContext) -> None:
    if not enable_ansi():
        log("couldn't enable ansi for the console, and stats will not be available...")
        return
    last_sent, last_t = 0, ctx.stats.started
    try:
        while not ctx.quit.wait(1):
            now = time.perf_counter()
            sent = ctx.stats.sent
            rate = (sent - last_sent) / (now - last_t)
            last_sent, last_t = sent, now
            set_status_line(
                f"[stats] {sent} sent | {rate:.0f} pps | "
                f"{ctx.stats.get_responses()} responses | "
                f"{ctx.stats.get_opens()} open | "
                f"{now - ctx.stats.started:.0f}s"
            )
    finally:
        set_status_line(None)


def spawn_stats_thread(ctx: SynScanContext) -> None:
    ctx.threads.stats_thread = threading.Thread(
        target=stats_loop, kwargs={"ctx": ctx}, daemon=True
    )
    ctx.threads.stats_thread.start()


def register_hooks(ctx: SynScanContext) -> None:
    def handle_sigint(signum, frame):
        ctx.stats.interrupted = True
        scan_stop(ctx)

    signal.signal(signal.SIGINT, handle_sigint)
    signal.signal(signal.SIGTERM, handle_sigint)


def scan_shutdown(ctx: SynScanContext) -> None:
    scan_stop(ctx)
    for t in (
        ctx.threads.send_thread,
        ctx.threads.recv_thread,
        ctx.threads.stats_thread,
    ):
        if t:
            t.join(timeout=2)
    summarize(ctx)


def _drain_timeout(sent: int, responses: int, min_wait: float = 0.5, max_wait: float | None = None) -> float:
    max_wait = max_wait or 8.0
    unanswered = max(0, sent - responses)
    if unanswered == 0:
        return 0.0
    return min(max_wait, max(min_wait, unanswered * 0.0005))


def scan_wait(ctx: SynScanContext, response_drain_timeout:float | None = None) -> None:
    while not ctx.quit.wait(0.2):
        if ctx.stats.interrupted:
            scan_stop(ctx)
            return
        sender = ctx.threads.send_thread
        if sender and not sender.is_alive():
            break

    wait_sec = _drain_timeout(ctx.stats.sent, ctx.stats.responses, max_wait=response_drain_timeout or ctx.response_drain_timeout)
    if wait_sec > 0 and not ctx.stats.interrupted:
        deadline = time.monotonic() + wait_sec
        while not ctx.quit.wait(0.1):
            if ctx.stats.interrupted or ctx.stats.responses >= ctx.stats.sent:
                break
            if time.monotonic() >= deadline:
                break

    scan_stop(ctx)



def scan_init(
    cidr,
    width,
    cooldown,
    *,
    s_ports: str | None = None,
    ports: list[int] | None = None,
    exclude_private: bool = False,
    enable_stats_tui: bool = True,
    register_signal_handlers: bool = True,
    response_drain_timeout:float=5.0
) -> SynScanContext:
    require_wpcap()
    if ports is None:
        ports = parse_ports(s_ports if s_ports is not None else "all")

    ctx = SynScanContext(
        ports=ports,
        picker=Picker(
            cidr=cidr,
            cooldown_period=cooldown,
            bucket_width=width,
            exclude_private_ranges=exclude_private,
        ),
        response_drain_timeout=response_drain_timeout
    )

    if register_signal_handlers:
        register_hooks(ctx)

    try:
        spawn_receiver(ctx)
        if enable_stats_tui:
            spawn_stats_thread(ctx)
        spawn_sender(ctx)
    except BaseException:
        ctx.stats.interrupted = True
        scan_shutdown(ctx)
        raise

    return ctx
