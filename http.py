




from dataclasses import dataclass
from picker import Picker

@dataclass(frozen=True)
class Config:
    cidr: str
    semaphore: int
    creds: tuple[str, str] | None
    bucket_width: int
    cooldown: int
    status_filter: TelnetStatus | None


def parse_args() -> Config:
    parser = argparse.ArgumentParser()
    parser.add_argument("cidr", help="Target CIDR block (e.g. 203.0.113.0/29)")
    parser.add_argument("--semaphore", type=int, default=10)
    parser.add_argument("--creds", type=parse_cred_pair, default=None)
    parser.add_argument("--bucket-width", type=int, default=2)
    parser.add_argument("--cooldown", type=int, default=500)
    parser.add_argument(
        "--filter",
        dest="status_filter",
        type=parse_status_filter,
        default=None,
        help="Only print summaries matching this TelnetStatus name (case-insensitive)",
    )
    args = parser.parse_args()
    return Config(
        cidr=args.cidr,
        semaphore=args.semaphore,
        creds=args.creds,
        bucket_width=args.bucket_width,
        cooldown=args.cooldown,
        status_filter=args.status_filter,
    )

def main():
	config = parse_args()
	picker = Picker(cidr=config.cidr, cooldown=config.cooldown, bucket_width=config.bucket_width)
	ip = None
	threads_ctx = None
	while (ip := picker.pick()):
	    for port in config.ports:
	        sem.acquire()
	        spawn(http_grab, ip=ip, port=port)
	picker.stop()
