import asyncio

import cli
import scanner


async def run() -> None:
    config = cli.parse_args()
    ctx = scanner.scan_init(config)
    await scanner.scan(ctx)


def main() -> None:
    try:
        asyncio.run(run())
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
