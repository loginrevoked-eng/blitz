"""Small Python port of the IPv4 bucket picker.

Pickers spread requests across address buckets, permute addresses within each
bucket without repeats, and delay reuse of a bucket for the cooldown period.
"""

from __future__ import annotations

import ipaddress
import random
import secrets
import threading
import time
from dataclasses import dataclass


_CLOUDFLARE = tuple(map(ipaddress.ip_network, (
    "173.245.48.0/20", "103.21.244.0/22", "103.22.200.0/22",
    "103.31.4.0/22", "141.101.64.0/18", "108.162.192.0/18",
    "190.93.240.0/20", "188.114.96.0/20", "197.234.240.0/22",
    "198.41.128.0/17", "162.158.0.0/15", "104.16.0.0/13",
    "104.24.0.0/14", "172.64.0.0/13", "131.0.72.0/22",
)))
_PRIVATE_RESERVED = tuple(map(ipaddress.ip_network, (
    "0.0.0.0/8", "10.0.0.0/8", "100.64.0.0/10", "127.0.0.0/8",
    "169.254.0.0/16", "172.16.0.0/12", "192.0.0.0/24",
    "192.0.2.0/24", "192.168.0.0/16", "198.18.0.0/15",
    "198.51.100.0/24", "203.0.113.0/24", "224.0.0.0/4",
    "240.0.0.0/4",
)))


def _splitmix64(state: int) -> tuple[int, int]:
    state = (state + 0x9E3779B97F4A7C15) & 0xFFFFFFFFFFFFFFFF
    value = state
    value = ((value ^ (value >> 30)) * 0xBF58476D1CE4E5B9) & 0xFFFFFFFFFFFFFFFF
    value = ((value ^ (value >> 27)) * 0x94D049BB133111EB) & 0xFFFFFFFFFFFFFFFF
    return state, value ^ (value >> 31)


def _fmix32(value: int) -> int:
    value &= 0xFFFFFFFF
    value ^= value >> 16
    value = (value * 0x85EBCA6B) & 0xFFFFFFFF
    value ^= value >> 13
    value = (value * 0xC2B2AE35) & 0xFFFFFFFF
    return (value ^ (value >> 16)) & 0xFFFFFFFF


def _permute(seed: int, index: int, width: int) -> int:
    """Feistel cycle-walk permutation matching picker/crypto/feistel.c."""
    bits = 2
    while bits < 32 and (1 << bits) < width:
        bits += 2
    half_bits = bits // 2
    mask = (1 << half_bits) - 1
    state = seed
    keys = []
    for _ in range(6):
        state, value = _splitmix64(state)
        keys.append(value & 0xFFFFFFFF)
    value = index
    while True:
        left, right = value >> half_bits, value & mask
        for key in keys:
            left, right = right, left ^ (_fmix32(right ^ key) & mask)
        value = (left << half_bits) | right
        if value < width:
            return value


@dataclass
class _Bucket:
    start: int
    end: int
    seed: int
    current: int = 0

    @property
    def width(self) -> int:
        return self.end - self.start + 1


class Picker:
    """Pick IPv4 addresses from a CIDR, with per-bucket cooldown.

    ``pick()`` returns an :class:`ipaddress.IPv4Address`. Cloudflare ranges
    are always skipped, matching the C picker; private/reserved ranges can
    also be skipped with ``exclude_private_ranges=True``.
    """

    def __init__(self, cidr: str, bucket_width: int = 256,
                 cooldown_period: int = 0, *,
                 exclude_private_ranges: bool = True):
        if not isinstance(bucket_width, int) or bucket_width < 1 or bucket_width > 0xFFFFFFFF:
            raise ValueError("bucket_width must be between 1 and 2^32-1")
        if cooldown_period < 0:
            raise ValueError("cooldown_period cannot be negative")
        self.network = ipaddress.ip_network(cidr, strict=False)
        if self.network.version != 4:
            raise ValueError("cidr must be an IPv4 network")
        self.bucket_width = bucket_width
        self.cooldown_period = cooldown_period
        exclusions = sorted(
            _CLOUDFLARE + (_PRIVATE_RESERVED if exclude_private_ranges else ()),
            key=lambda network: int(network.network_address),
        )
        self._eligible: list[_Bucket] = []
        self._cooldown: list[tuple[float, _Bucket]] = []
        self._condition = threading.Condition()
        self._stopped = False

        start, end = int(self.network.network_address), int(self.network.broadcast_address)
        cursor = start
        for excluded in exclusions:
            low = max(start, int(excluded.network_address))
            high = min(end, int(excluded.broadcast_address))
            if low > high:
                continue
            if cursor < low:
                self._append_buckets(cursor, low - 1)
            cursor = max(cursor, high + 1)
            if cursor > end:
                break
        if cursor <= end:
            self._append_buckets(cursor, end)

    def _append_buckets(self, start: int, end: int) -> None:
        while start <= end:
            bucket_end = min(end, start + self.bucket_width - 1)
            self._eligible.append(_Bucket(start, bucket_end, secrets.randbits(32)))
            start = bucket_end + 1

    @property
    def initial_exhaustion_count(self) -> int:
        """Number of buckets initially available."""
        return len(self._eligible)

    def pick(self) -> ipaddress.IPv4Address | None:
        """Return the next address, waiting through cooldown if needed.

        Returns ``None`` after :meth:`stop` or after all addresses are used.
        """
        with self._condition:
            while not self._stopped:
                now = time.monotonic()
                while self._cooldown and self._cooldown[0][0] <= now:
                    _, bucket = self._cooldown.pop(0)
                    self._eligible.append(bucket)
                if self._eligible:
                    slot = random.randrange(len(self._eligible))
                    bucket = self._eligible.pop(slot)
                    address = bucket.start + _permute(bucket.seed, bucket.current, bucket.width)
                    bucket.current += 1
                    if bucket.current < bucket.width:
                        self._cooldown.append((now + self.cooldown_period / 1000, bucket))
                        self._cooldown.sort(key=lambda entry: entry[0])
                    self._condition.notify_all()
                    return ipaddress.IPv4Address(address)
                if not self._cooldown:
                    return None
                self._condition.wait(max(0, self._cooldown[0][0] - now))
            return None

    def stop(self) -> None:
        """Wake any waiting ``pick()`` call and make future calls return None."""
        with self._condition:
            self._stopped = True
            self._condition.notify_all()


__all__ = ["Picker"]
