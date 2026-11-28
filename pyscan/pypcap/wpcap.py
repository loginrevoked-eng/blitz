from __future__ import annotations

import os
from ctypes import (
    CDLL,
    POINTER,
    Structure,
    c_char_p,
    c_int,
    c_int32,
    c_ubyte,
    c_uint,
    c_uint32,
    c_void_p,
    create_string_buffer,
)

from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from .models import ScanConfig

_npcap_dir = r"C:\Windows\System32\Npcap"
if os.path.isdir(_npcap_dir):
    os.add_dll_directory(_npcap_dir)

wpcap = CDLL("wpcap.dll")


class pcap_if(Structure):
    pass


pcap_if._fields_ = [
    ("next", POINTER(pcap_if)),
    ("name", c_char_p),
    ("description", c_char_p),
    ("addresses", c_void_p),
    ("flags", c_uint),
]


class pcap_pkthdr(Structure):
    _fields_ = [
        ("ts_sec", c_int32),
        ("ts_usec", c_int32),
        ("caplen", c_uint),
        ("len", c_uint),
    ]


class bpf_program(Structure):
    _fields_ = [
        ("bf_len", c_uint),
        ("bf_insns", c_void_p),
    ]


wpcap.pcap_findalldevs.argtypes = [POINTER(POINTER(pcap_if)), c_char_p]
wpcap.pcap_freealldevs.argtypes = [POINTER(pcap_if)]
wpcap.pcap_open_live.argtypes = [c_char_p, c_int, c_int, c_int, c_char_p]
wpcap.pcap_open_live.restype = c_void_p
wpcap.pcap_sendpacket.argtypes = [c_void_p, c_char_p, c_int]
wpcap.pcap_next_ex.argtypes = [
    c_void_p,
    POINTER(POINTER(pcap_pkthdr)),
    POINTER(POINTER(c_ubyte)),
]
wpcap.pcap_next_ex.restype = c_int
wpcap.pcap_geterr.argtypes = [c_void_p]
wpcap.pcap_geterr.restype = c_char_p
wpcap.pcap_close.argtypes = [c_void_p]

wpcap.pcap_compile.argtypes = [
    c_void_p,
    POINTER(bpf_program),
    c_char_p,
    c_int,
    c_uint32,
]
wpcap.pcap_compile.restype = c_int
wpcap.pcap_setfilter.argtypes = [c_void_p, POINTER(bpf_program)]
wpcap.pcap_setfilter.restype = c_int
wpcap.pcap_freecode.argtypes = [POINTER(bpf_program)]


def open_handle(cfg: ScanConfig):
    errbuf = create_string_buffer(256)
    handle = wpcap.pcap_open_live(
        cfg.sender.interf.name.encode(), 65535, 0, 100, errbuf
    )
    if not handle:
        return None, errbuf.value.decode()
    return handle, ""

def require_wpcap() -> None:
    if wpcap is None:
        raise RuntimeError("wpcap.dll did not load")
