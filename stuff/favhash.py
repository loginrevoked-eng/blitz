import argparse
import asyncio
import codecs
import re
import ssl
import sys
import traceback
from dataclasses import dataclass
from pathlib import Path
from urllib.parse import urljoin, urlparse

import aiohttp
import mmh3
import pyperclip


@dataclass(frozen=True)
class Candidate:
    site: str
    url: str
    source: str


@dataclass(frozen=True)
class Favicon:
    candidate: Candidate
    data: bytes

    @property
    def hash(self) -> int:
        return mmh3.hash(codecs.encode(self.data, "base64"))

    def save(self, path: Path, prefix: int = 0) -> Path:
        if path.is_dir():
            name = f"{prefix}_favicon.ico" if prefix else "favicon.ico"
            dest = path / name
        elif prefix:
            dest = path.with_stem(f"{prefix}_{path.stem}")
        else:
            dest = path

        dest.write_bytes(self.data)
        return dest.resolve()

    def save_all(self, directory: Path = Path(".")) -> Path:
        raw = self.candidate.site if "://" in self.candidate.site else f"https://{self.candidate.site}"
        host = urlparse(raw).netloc.replace(":", "_")
        clean_source = re.sub(r"[^a-zA-Z0-9_-]+", "_", self.candidate.source).strip("_").lower()
        ext = Path(urlparse(self.candidate.url).path).suffix or ".ico"

        dest = directory / f"{host}-{clean_source}{ext}"
        dest.write_bytes(self.data)
        return dest.resolve()


class HashFmtr:
    def __init__(self, quiet: bool = False) -> None:
        self.quiet = quiet
        self.collected: list[tuple[str, int]] = []

    def pretty(self, f_hash: int, candidate: Candidate) -> None:
        self.collected.append((candidate.site, f_hash))
        if not self.quiet:
            print(f"[+] {candidate.site} | {candidate.source} ({candidate.url}) -> http.favicon.hash:{f_hash}")

    def copy(self) -> None:
        if len(self.collected) == 1:
            payload = str(self.collected[0][1])
        else:
            payload = "\n".join(f"{site}:{f_hash}" for site, f_hash in self.collected)

        try:
            pyperclip.copy(payload)
            if not self.quiet:
                print(f"[+] Copied {len(self.collected)} hash(es) to clipboard.")
        except pyperclip.PyperclipException:
            print("[!] Clipboard unavailable.", file=sys.stderr)


class ShodFavHasher:
    LINK_REGEXES = (
        r'<link[^>]+rel=["\'](?:shortcut )?icon["\'][^>]+href=["\']([^"\']+)["\']',
        r'<link[^>]+href=["\']([^"\']+)["\'][^>]+rel=["\'](?:shortcut )?icon["\']',
    )

    def __init__(self, timeout: int = 10, verify_ssl: bool = False) -> None:
        self.timeout = aiohttp.ClientTimeout(total=timeout)
        self.ssl: ssl.SSLContext | bool = True if verify_ssl else False

    async def fetch(self, session: aiohttp.ClientSession, site: str) -> tuple[str, Favicon | None]:
        for candidate in await self._candidates(session, site):
            try:
                async with session.get(candidate.url, ssl=self.ssl) as resp:
                    resp.raise_for_status()
                    return site, Favicon(candidate=candidate, data=await resp.read())
            except aiohttp.ClientResponseError as err:
                if err.status == 404:
                    continue
                raise
        return site, None

    async def _candidates(self, session: aiohttp.ClientSession, site: str) -> list[Candidate]:
        base = site if site.startswith(("http://", "https://")) else f"https://{site}"
        candidates: list[Candidate] = []
        try:
            async with session.get(base, ssl=self.ssl) as resp:
                html = await resp.text(errors="ignore")
                final_url = str(resp.url)
                for pattern in self.LINK_REGEXES:
                    if match := re.search(pattern, html, re.IGNORECASE):
                        candidates.append(Candidate(site, urljoin(final_url, match.group(1)), "HTML <link>"))
                        break
        except aiohttp.ClientError:
            pass

        candidates.append(Candidate(site, f"{base.rstrip('/')}/favicon.ico", "Fallback /favicon.ico"))
        return candidates


def catch(show_tb: bool, exc: Exception, site: str | None = None) -> None:
    if show_tb:
        traceback.print_exception(exc)
    else:
        prefix = f"[!] {site}: " if site else "[!] "
        print(f"{prefix}{type(exc).__name__}: {exc}", file=sys.stderr)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Calculate Shodan favicon hashes.")
    parser.add_argument("websites", nargs="+", help="Target domain(s) or URL(s)")
    parser.add_argument("-q", "--quiet", action="store_true", help="Suppress stdout output")
    parser.add_argument("--no-clipboard", action="store_true", help="Skip clipboard copy")
    parser.add_argument("--traceback", action="store_true", help="Show full tracebacks on errors")

    dl_group = parser.add_mutually_exclusive_group()
    dl_group.add_argument("-d", "--download", nargs="?", const="favicon.ico", type=Path, help="Save path")
    dl_group.add_argument("--download-all", action="store_true", help="Save each as website-source.ext")
    return parser


async def run(args: argparse.Namespace) -> None:
    hasher = ShodFavHasher()
    fmt = HashFmtr(quiet=args.quiet)

    async def _safe_fetch(session: aiohttp.ClientSession, site: str) -> tuple[str, Favicon | None, Exception | None]:
        try:
            _, fav = await hasher.fetch(session, site)
            return site, fav, None
        except Exception as exc:
            return site, None, exc

    async with aiohttp.ClientSession(timeout=hasher.timeout) as session:
        tasks = [asyncio.create_task(_safe_fetch(session, site)) for site in args.websites]

        for idx, task in enumerate(asyncio.as_completed(tasks)):
            site, fav, exc = await task
            if exc:
                catch(args.traceback, exc, site)
                continue

            if not fav:
                print(f"[-] {site}: Not found", file=sys.stderr)
                continue

            fmt.pretty(fav.hash, fav.candidate)

            if not args.no_clipboard:
                fmt.copy()

            if args.download_all:
                saved = fav.save_all()
                if not args.quiet:
                    print(f"    Saved: {saved}")
            elif args.download:
                saved = fav.save(args.download, prefix=idx)
                if not args.quiet:
                    print(f"    Saved: {saved}")

    if not fmt.collected:
        sys.exit(1)


def main() -> None:
    args = build_parser().parse_args()
    try:
        asyncio.run(run(args))
    except Exception as exc:
        catch(args.traceback, exc)
        sys.exit(1)


if __name__ == "__main__":
    main()
