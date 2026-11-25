"""Probe an ADB TCP endpoint with the initial ADB transport handshake."""

import argparse
import socket
import struct
import sys

ADB_VERSION = 0x01000000
ADB_MAX_DATA = 256 * 1024
HEADER = struct.Struct("<6I")
HOST_BANNER = b"host::\0"


def _command_id(name: str) -> int:
    return int.from_bytes(name.encode("ascii"), "little")


def _command_name(value: int) -> str:
    try:
        return value.to_bytes(4, "little").decode("ascii")
    except (OverflowError, UnicodeDecodeError):
        return f"0x{value:08x}"


def _send_packet(sock: socket.socket, command: str, arg0: int, arg1: int, payload: bytes) -> None:
    command_id = _command_id(command)
    header = HEADER.pack(
        command_id,
        arg0,
        arg1,
        len(payload),
        sum(payload) & 0xFFFFFFFF,
        command_id ^ 0xFFFFFFFF,
    )
    sock.sendall(header + payload)


def _read_exact(sock: socket.socket, size: int) -> bytes:
    chunks = bytearray()
    while len(chunks) < size:
        chunk = sock.recv(size - len(chunks))
        if not chunk:
            raise ConnectionError("peer closed the connection during the ADB handshake")
        chunks.extend(chunk)
    return bytes(chunks)


def _receive_packet(sock: socket.socket) -> tuple[str, int, int, bytes]:
    raw_header = _read_exact(sock, HEADER.size)
    command, arg0, arg1, data_length, _checksum, magic = HEADER.unpack(raw_header)
    if magic != (command ^ 0xFFFFFFFF):
        raise ValueError("response has an invalid ADB packet header")
    if data_length > ADB_MAX_DATA:
        raise ValueError(f"ADB payload is too large ({data_length} bytes)")
    payload = _read_exact(sock, data_length) if data_length else b""
    return _command_name(command), arg0, arg1, payload


def _decode_banner(payload: bytes) -> str:
    return payload.split(b"\0", 1)[0].decode("utf-8", errors="replace")


def probe(host: str, port: int, timeout: float) -> tuple[str, int, int, str]:
    """Send CNXN and return the first ADB response and its decoded banner."""
    with socket.create_connection((host, port), timeout=timeout) as sock:
        sock.settimeout(timeout)
        _send_packet(sock, "CNXN", ADB_VERSION, ADB_MAX_DATA, HOST_BANNER)
        command, arg0, arg1, payload = _receive_packet(sock)
    return command, arg0, arg1, _decode_banner(payload)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Check whether a TCP endpoint speaks the ADB transport protocol."
    )
    parser.add_argument("host", help="device hostname or IP address")
    parser.add_argument("port", type=int, nargs="?", default=5555)
    parser.add_argument("--timeout", type=float, default=3.0, help="socket timeout in seconds")
    args = parser.parse_args()

    if not 1 <= args.port <= 65535:
        parser.error("port must be between 1 and 65535")
    if args.timeout <= 0:
        parser.error("timeout must be greater than zero")

    try:
        command, arg0, arg1, banner = probe(args.host, args.port, args.timeout)
    except (OSError, ValueError, ConnectionError) as exc:
        print(f"No valid ADB handshake from {args.host}:{args.port}: {exc}", file=sys.stderr)
        return 1

    endpoint = f"{args.host}:{args.port}"
    if command == "AUTH":
        auth_types = {1: "TOKEN", 2: "SIGNATURE", 3: "RSAPUBLICKEY"}
        auth_type = auth_types.get(arg0, f"unknown type {arg0}")
        print(f"ADB transport detected at {endpoint}")
        print(f"Response: AUTH {auth_type}")
        print("The endpoint requires authentication; no key was sent.")
    elif command == "CNXN":
        print(f"ADB transport is online at {endpoint}")
        print(f"Device protocol version: 0x{arg0:08x}")
        print(f"Device max payload: {arg1} bytes")
        print(f"Device banner: {banner or '<empty>'}")
    elif command == "STLS":
        print(f"ADB transport detected at {endpoint}; TLS upgrade requested (STLS).")
    else:
        print(f"{endpoint} replied with ADB packet {command!r}, not a handshake response.")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
