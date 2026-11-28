import argparse
from dataclasses import dataclass

from pypcap.scan import parse_ports


@dataclass(frozen=True)
class Config:
    cidr: str
    ports: list[int]
    threads: int
    run: list[str]
    verbose: bool
    bucket_width: int | None
    cooldown: int
    service_port_mapped: bool
    response_drain_timeout: float


def parse_runs(raw: str) -> list[str]:
    names = [name.strip() for name in raw.split(",")]
    if not names or any(not name for name in names):
        raise argparse.ArgumentTypeError("--run requires comma-separated grabber names")
    return names


def parse_args() -> Config:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "cidr",
        help="Target CIDR block (e.g. 203.0.113.0/29), you can pass single ipv4 address too",
    )
    parser.add_argument(
        "--ports",
        type=parse_ports,
        default=[80, 443],
        help='Comma-separated ports (e.g. 80,8080) or in range 80-90 or just pass "all" to sweep all 2**16-1 ports',
    )
    parser.add_argument("--threads", type=int, default=10)
    parser.add_argument(
        "--run",
        type=parse_runs,
        default=["http"],
        help="Comma-separated grabbers to assign to workers (e.g. http,banner)",
    )
    parser.add_argument("--verbose", action="store_true", help="Print grabber output")
    parser.add_argument("--bucket-width", type=int, default=None)
    parser.add_argument("--cooldown", type=int, default=500)
    parser.add_argument(
        "--service-port-mapped",
        action="store_true",
        help="Only run grabbers on their mapped ports",
    )
    parser.add_argument(
        "--response-drain-timeout",
        type=float,
        default=0.01,
        help="Seconds to wait for trailing responses after sending finishes",
    )
    args = parser.parse_args()
    if args.threads < 1:
        parser.error("--threads must be at least 1")
    if args.response_drain_timeout < 0:
        parser.error("--response-drain-timeout cannot be negative")
    return Config(
        cidr=args.cidr,
        ports=args.ports,
        threads=args.threads,
        run=args.run,
        verbose=args.verbose,
        bucket_width=args.bucket_width,
        cooldown=args.cooldown,
        service_port_mapped=args.service_port_mapped,
        response_drain_timeout=args.response_drain_timeout,
    )
