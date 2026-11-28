import asyncio
import signal
import threading
import time
import traceback
from collections.abc import Awaitable, Callable
from dataclasses import dataclass, field, replace
from functools import partial

from cli import Config
from grabbers.banner import banner_grab
from grabbers.ghttp.envf import http_grab_envf
from grabbers.ghttp.favicon import http_grab_favicon
from grabbers.ghttp.http import http_grab
from grabbers.rdp.screenshot import rdp_grab_screenshot
from pypcap import ScanContext as SynScanContext
from pypcap import scan_init as syn_scan_init
from pypcap.scan import scan_shutdown as syn_scan_shutdown
from pypcap.scan import scan_wait as syn_scan_wait
from report import logger
from stats import ScanStats, spawn_stat_thread, stop_stat_thread

_HTTP_PORTS = frozenset({80, 443, 8000, 8008, 8080, 8443, 8888})


@dataclass(frozen=True)
class Grabber:
    fn: Callable[..., Awaitable[None]]
    applicable_ports: frozenset[int] | None = None

    def applies_to(self, port: int, ctx: "ScanContext") -> bool:
        if ctx.config.service_port_mapped:
            return self.applicable_ports is None or port in self.applicable_ports
        return True


GRABBERS: dict[str, Grabber] = {
    "http": Grabber(http_grab, _HTTP_PORTS),
    "banner": Grabber(banner_grab, None),
    "ssh": Grabber(banner_grab, frozenset({22, 2222})),
    "favicon-hash": Grabber(http_grab_favicon, _HTTP_PORTS),
    "env-files": Grabber(http_grab_envf, _HTTP_PORTS),
    "rdp-screenshot": Grabber(rdp_grab_screenshot, frozenset({3389})),
}


@dataclass
class ScanContext:
    config: Config
    syn_scan: SynScanContext
    lock: threading.Lock = field(default_factory=threading.Lock)
    event: threading.Event = field(default_factory=threading.Event)
    grabbers: list[Grabber] = field(default_factory=list)
    workers: set[asyncio.Task[None]] = field(default_factory=set)
    stats: ScanStats = field(default_factory=ScanStats)
    producer_stopped: bool = False
    workers_stopped: bool = False


def resolve_grabbers(run: list[str], *args, **kwargs) -> list[Grabber]:
    selected: list[Grabber] = []
    for name in run:
        grabber = GRABBERS.get(name)
        if grabber is None:
            available = ", ".join(sorted(GRABBERS))
            raise ValueError(
                f"Unknown grabber {name!r}; available grabbers: {available}"
            )
        selected.append(replace(grabber, fn=partial(grabber.fn, *args, **kwargs)))
    if not selected:
        raise ValueError("At least one grabber must be selected with --run")
    return selected


def init_grabbers(ctx: ScanContext, *args, **kwargs) -> None:
    ctx.grabbers = resolve_grabbers(ctx.config.run, *args, **kwargs)


async def _run_grabber(grabber: Grabber, ctx: ScanContext, ip: str, port: int) -> None:
    try:
        await asyncio.wait_for(grabber.fn(ctx=ctx, ip=ip, port=port), timeout=15.0)
    except asyncio.TimeoutError:
        pass
    except Exception:
        traceback.print_exc()


async def drainer(ctx: ScanContext) -> None:
    open_queue = ctx.syn_scan.open
    while True:
        target = await asyncio.to_thread(open_queue.get)
        if target is None:
            open_queue.task_done()
            return

        try:
            if not ctx.event.is_set():
                matching = [
                    g for g in ctx.grabbers if g.applies_to(target.port, ctx)
                ]
                if matching:
                    ctx.stats.alive_add(target.ip)
                    ctx.stats.scanned_inc()
                    ctx.stats.active_inc()
                    try:
                        await asyncio.gather(
                            *(
                                _run_grabber(g, ctx, target.ip, target.port)
                                for g in matching
                            )
                        )
                    finally:
                        ctx.stats.active_dec()
        finally:
            open_queue.task_done()


def spawn_workers(ctx: ScanContext) -> None:
    if not ctx.grabbers:
        raise ValueError("At least one grabber must be selected with --run")
    for _ in range(ctx.config.threads):
        ctx.workers.add(asyncio.create_task(drainer(ctx)))


def scan_summarize(ctx: ScanContext) -> None:
    syn = ctx.syn_scan
    syn_elapsed = time.perf_counter() - syn.stats.started
    state = "interrupted" if (ctx.event.is_set() or syn.stats.interrupted) else "finished"

    print(
        f"[syn] {state} in {syn_elapsed:.2f}s | "
        f"{syn.stats.sent} sent | "
        f"{syn.stats.get_responses()} responses | "
        f"{syn.stats.get_opens()} open"
    )
    print(
        f"[scan] {state} | "
        f"grabbers={','.join(ctx.config.run)} | "
        f"hits={getattr(ctx.stats, 'hits', getattr(ctx.stats, 'completed', 0))} | "
        f"errors={getattr(ctx.stats, 'errors', 0)}"
    )


async def stop_workers(ctx: ScanContext, *, cancel: bool = False) -> None:
    if ctx.workers_stopped:
        return
    ctx.workers_stopped = True
    for _ in ctx.workers:
        ctx.syn_scan.open.put(None)
    if cancel:
        for task in ctx.workers:
            task.cancel()
    try:
        await asyncio.wait_for(
            asyncio.gather(*ctx.workers, return_exceptions=True),
            timeout=3.0,
        )
    except asyncio.TimeoutError:
        pass



async def stop_producer(ctx: ScanContext) -> None:
    if ctx.producer_stopped:
        return
    await asyncio.to_thread(syn_scan_shutdown, ctx.syn_scan)
    ctx.producer_stopped = True


def register_hooks(ctx: ScanContext) -> None:
    def handle_sigterm(signum, frame):
        ctx.event.set()
        ctx.syn_scan.stats.interrupted = True
        ctx.syn_scan.quit.set()

    signal.signal(signal.SIGTERM, handle_sigterm)


def scan_init(config: Config, *args, **kwargs) -> ScanContext:
    logger.set_quiet(not config.verbose)

    selected = resolve_grabbers(config.run, *args, **kwargs)

    syn_ctx = syn_scan_init(
        config.cidr,
        config.bucket_width,
        config.cooldown,
        ports=config.ports,
        enable_stats_tui=False,
        register_signal_handlers=False,
        response_drain_timeout=config.response_drain_timeout,
    )
    ctx = ScanContext(
        config=config,
        syn_scan=syn_ctx,
        grabbers=selected,
    )
    register_hooks(ctx)
    spawn_workers(ctx)
    spawn_stat_thread(ctx)
    return ctx


async def scanner_shutdown(ctx: ScanContext, *, interrupted: bool = False) -> None:
    if interrupted:
        ctx.event.set()
    await stop_producer(ctx)
    await stop_workers(ctx, cancel=interrupted)
    stop_stat_thread(ctx.stats)
    scan_summarize(ctx)


async def scan(ctx: ScanContext) -> None:
    interrupted = False
    try:
        await asyncio.to_thread(syn_scan_wait, ctx.syn_scan)
    except KeyboardInterrupt:
        interrupted = True
        raise
    except asyncio.CancelledError:
        interrupted = True
        ctx.event.set()
        ctx.syn_scan.stats.interrupted = True
        ctx.syn_scan.quit.set()
        raise
    finally:
        await scanner_shutdown(
            ctx, interrupted=interrupted or ctx.event.is_set()
        )
