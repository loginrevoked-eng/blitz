import threading
import time
from dataclasses import dataclass, field
from queue import Queue

from .models import Dest, ScanConfig
from .picker import Picker


@dataclass
class ScanStats:
    interrupted: bool = False
    sent: int = 0
    responses: int = 0
    opens: int = 0
    started: float = field(default_factory=time.perf_counter)
    _lock: threading.Lock = field(default_factory=threading.Lock, repr=False)

    def inc_responses(self) -> None:
        with self._lock:
            self.responses += 1

    def get_responses(self) -> int:
        with self._lock:
            return self.responses

    def inc_opens(self) -> None:
        with self._lock:
            self.opens += 1

    def get_opens(self) -> int:
        with self._lock:
            return self.opens


@dataclass
class Threads:
    send_thread: threading.Thread | None = None
    recv_thread: threading.Thread | None = None
    stats_thread: threading.Thread | None = None
    drainer_thread: threading.Thread | None = None


@dataclass
class SynScanContext:
    response_drain_timeout:float | None = None
    stats: ScanStats = field(default_factory=ScanStats)
    open: Queue[Dest] = field(default_factory=Queue)
    lock: threading.Lock = field(default_factory=threading.Lock)
    quit: threading.Event = field(default_factory=threading.Event)
    cfg: ScanConfig = field(default_factory=ScanConfig)
    threads: Threads = field(default_factory=Threads)
    ports: list[int] = field(default_factory=list)
    picker: Picker | None = None
    results: list = field(default_factory=list)
    alive: set[Dest] = field(default_factory=set)


def should_stop(ctx: SynScanContext) -> bool:
    return ctx.quit.is_set()


def scan_stop(ctx: SynScanContext) -> None:
    ctx.quit.set()
