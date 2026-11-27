import threading
from dataclasses import dataclass, field
from typing import TYPE_CHECKING

from report import logger

if TYPE_CHECKING:
    from scanner import ScanContext


@dataclass
class ScanStats:
    queued: int = 0
    active: int = 0
    alive: int = 0
    confirmed: int = 0
    scanned: int = 0
    alive_hosts: set[str] = field(default_factory=set, repr=False)
    lock: threading.Lock = field(default_factory=threading.Lock, repr=False)
    stop_event: threading.Event = field(default_factory=threading.Event, repr=False)
    thread: threading.Thread | None = field(default=None, repr=False)

    def queued_inc(self) -> None:
        with self.lock:
            self.queued += 1

    def queued_dec(self) -> None:
        with self.lock:
            self.queued -= 1

    def active_inc(self) -> None:
        with self.lock:
            self.active += 1

    def active_dec(self) -> None:
        with self.lock:
            self.active -= 1

    def alive_add(self, ip: str) -> None:
        with self.lock:
            self.alive_hosts.add(ip)
            self.alive = len(self.alive_hosts)

    def scanned_inc(self) -> None:
        with self.lock:
            self.scanned += 1

    def scanned_dec(self) -> None:
        with self.lock:
            self.scanned -= 1

    def confirmed_inc(self) -> None:
        with self.lock:
            self.confirmed += 1

    def confirmed_dec(self) -> None:
        with self.lock:
            self.confirmed -= 1

    def snapshot(self) -> tuple[int, int, int, int, int]:
        with self.lock:
            return self.queued, self.active, self.alive, self.confirmed, self.scanned


def stat_highlight(ctx: "ScanContext") -> None:
    _, active, alive, confirmed, scanned = ctx.stats.snapshot()
    queued = ctx.syn_scan.open.qsize()
    logger.set_status_line(
        f"SYN tx={ctx.syn_scan.stats.sent} "
        f"rx={ctx.syn_scan.stats.get_responses()} "
        f"open={ctx.syn_scan.stats.get_opens()} | "
        f"scan q={queued} active={active} scanned={scanned} "
        f"hosts={alive} confirmed={confirmed}"
    )


def spawn_stat_thread(
    ctx: "ScanContext", interval: float = 1.0
) -> threading.Thread | None:
    if interval <= 0:
        raise ValueError("interval must be positive")
    if not logger.enable_ansi():
        logger.log("couldn't enable ansi for the console, and stats will not be available...")
        return None

    def update() -> None:
        while not ctx.stats.stop_event.is_set():
            stat_highlight(ctx)
            ctx.stats.stop_event.wait(interval)
        stat_highlight(ctx)  # final totals after the last wait
        logger.set_status_line(None)

    thread = threading.Thread(target=update, name="scan-stats", daemon=True)
    ctx.stats.thread = thread
    thread.start()
    return thread


def stop_stat_thread(stats: ScanStats) -> None:
    stats.stop_event.set()
    if stats.thread is not None:
        stats.thread.join()
        stats.thread = None
