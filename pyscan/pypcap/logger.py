import ctypes
import sys
import threading
from ctypes import wintypes

_lock = threading.RLock()
_ansi_enabled = False
_status_line: str | None = None
_quiet = False


def set_quiet(quiet: bool) -> None:
    global _quiet
    with _lock:
        _quiet = quiet


def log(*args, **kwargs) -> None:
    with _lock:
        if _ansi_enabled and _status_line:
            sys.stdout.write("\r\x1b[2K")
        print(*args, **kwargs, flush=True)
        if _ansi_enabled and _status_line:
            sys.stdout.write(_status_line)
            sys.stdout.flush()


def sprint(*args, **kwargs) -> None:
    with _lock:
        if _quiet:
            return
        log(*args, **kwargs)


def set_status_line(status: str | None) -> None:
    global _status_line
    with _lock:
        if not _ansi_enabled:
            return
        sys.stdout.write("\r\x1b[2K")
        _status_line = status
        if status:
            sys.stdout.write(status)
        sys.stdout.flush()


def enable_ansi() -> bool:
    global _ansi_enabled

    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel32.GetStdHandle.argtypes = [wintypes.DWORD]
    kernel32.GetStdHandle.restype = wintypes.HANDLE
    kernel32.GetConsoleMode.argtypes = [wintypes.HANDLE, wintypes.LPDWORD]
    kernel32.GetConsoleMode.restype = wintypes.BOOL
    kernel32.SetConsoleMode.argtypes = [wintypes.HANDLE, wintypes.DWORD]
    kernel32.SetConsoleMode.restype = wintypes.BOOL

    STD_OUTPUT_HANDLE = -11
    ENABLE_VIRTUAL_TERMINAL_PROCESSING = 0x0004

    handle = kernel32.GetStdHandle(STD_OUTPUT_HANDLE)
    mode = wintypes.DWORD()
    if not kernel32.GetConsoleMode(handle, ctypes.byref(mode)):
        return False  # stdout isn't a console (redirected or piped)
    enabled = bool(
        kernel32.SetConsoleMode(handle, mode.value | ENABLE_VIRTUAL_TERMINAL_PROCESSING)
    )
    with _lock:
        _ansi_enabled = enabled
    return enabled
