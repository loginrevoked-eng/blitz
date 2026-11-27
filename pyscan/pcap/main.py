import argparse

from . import scan_init


def main():
    ap = argparse.ArgumentParser(description="SYN scanner")
    ap.add_argument("cidr", nargs="?", default="127.0.0.1/24")
    ap.add_argument("-p", "--ports", help="e.g. 22,80,8000-8100 or all", default=None)
    ap.add_argument("--width", type=int, default=None, help="picker bucket width")
    ap.add_argument(
        "--cooldown", type=float, default=5000, help="picker cooldown milliseconds"
    )
    ap.add_argument(
        "--exclude-private",
        action="store_true",
        help="skip private and reserved IPv4 ranges",
    )
    args = ap.parse_args()

    scan_init(
        args.cidr,
        args.width,
        args.cooldown,
        s_ports=args.ports,
        exclude_private=args.exclude_private,
    )


if __name__ == "__main__":
    main()
