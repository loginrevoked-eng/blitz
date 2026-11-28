import ctypes
import os
import random
import socket
import struct
import time
import uuid
from ctypes import POINTER, byref, c_ubyte, c_void_p, create_string_buffer, wintypes
from dataclasses import dataclass, field

import psutil

from .wpcap import wpcap, pcap_if, pcap_pkthdr


@dataclass
class Interface:
    name: str
    ipv4: str
    mac: str

    @staticmethod
    def build() -> "Interface":
        # enumerate pcap devices and read each one's IPv4 straight from wpcap
        errbuf = create_string_buffer(256)
        devs = POINTER(pcap_if)()
        if wpcap.pcap_findalldevs(byref(devs), errbuf) != 0:
            raise OSError(errbuf.value.decode())

        found = []   # (device name, description, ipv4)
        d = devs
        while d:
            dev = d.contents
            addr = dev.addresses
            while addr:
                # struct pcap_addr: next, addr, netmask, broadaddr, dstaddr (all pointers)
                node = ctypes.cast(addr, POINTER(c_void_p * 5)).contents
                sa = node[1]
                if sa and ctypes.c_ushort.from_address(sa).value == socket.AF_INET:
                    ip = socket.inet_ntoa(ctypes.string_at(sa + 4, 4))
                    if not (dev.flags & 0x1) and ip != "0.0.0.0" and not ip.startswith("169.254."):
                        found.append((dev.name.decode(), (dev.description or b"").decode(), ip))
                addr = node[0]
            d = dev.next
        wpcap.pcap_freealldevs(devs)

        if not found:
            raise OSError("no usable pcap device with an IPv4 address")
        if len(found) == 1:
            name, _, ipv4 = found[0]
        else:
            for i, (n, desc, ip) in enumerate(found):
                print(i, n, "-", desc, "-", ip)
            name, _, ipv4 = found[int(input("device #: "))]

        # wpcap doesn't expose the adapter's MAC, so look it up by the IP wpcap gave us
        mac = None
        for addrs in psutil.net_if_addrs().values():
            if any(a.family == socket.AF_INET and a.address == ipv4 for a in addrs):
                mac = next(
                    (a.address for a in addrs if a.family in (psutil.AF_LINK, getattr(socket, "AF_PACKET", -1))),
                    None,
                )
                break
        if mac is None:
            mac = "-".join(f"{b:02x}" for b in uuid.getnode().to_bytes(6, "big"))

        return Interface(name=name, ipv4=ipv4, mac=mac.lower().replace(":", "-"))


def random_seq_key(n: int = 16) -> bytes:
    return os.urandom(n)


@dataclass
class SenderConfig:
    gateway_mac: str
    src_port:int
    interf: Interface = field(default_factory=Interface.build)

    @staticmethod
    def build(interf: Interface | None = None) -> "SenderConfig":
        class MIB_IPFORWARDROW(ctypes.Structure):
            _fields_ = [
                ("dwForwardDest", wintypes.DWORD),
                ("dwForwardMask", wintypes.DWORD),
                ("dwForwardPolicy", wintypes.DWORD),
                ("dwForwardNextHop", wintypes.DWORD),
                ("dwForwardIfIndex", wintypes.DWORD),
                ("dwForwardType", wintypes.DWORD),
                ("dwForwardProto", wintypes.DWORD),
                ("dwForwardAge", wintypes.DWORD),
                ("dwForwardNextHopAS", wintypes.DWORD),
                ("dwForwardMetric1", wintypes.DWORD),
                ("dwForwardMetric2", wintypes.DWORD),
                ("dwForwardMetric3", wintypes.DWORD),
                ("dwForwardMetric4", wintypes.DWORD),
                ("dwForwardMetric5", wintypes.DWORD),
            ]

        iface = interf or Interface.build()

        # gateway IP: wpcap has no routing table, so this lookup stays
        iphlpapi = ctypes.WinDLL("iphlpapi")
        src_addr = struct.unpack("<I", socket.inet_aton(iface.ipv4))[0]
        dst_addr = struct.unpack("<I", socket.inet_aton("8.8.8.8"))[0]

        route = MIB_IPFORWARDROW()
        if iphlpapi.GetBestRoute(dst_addr, src_addr, ctypes.byref(route)) != 0:
            raise OSError("GetBestRoute failed")
        gw_ip = socket.inet_ntoa(struct.pack("<I", route.dwForwardNextHop))

        # gateway MAC: ARP request/reply over wpcap instead of SendARP
        my_mac = bytes.fromhex(iface.mac.replace("-", ""))
        arp_req = (
            struct.pack("!6s6sH", b"\xff" * 6, my_mac, 0x0806)
            + struct.pack(
                "!HHBBH6s4s6s4s",
                1, 0x0800, 6, 4, 1,
                my_mac, socket.inet_aton(iface.ipv4),
                b"\x00" * 6, socket.inet_aton(gw_ip),
            )
        )
        arp_req = arp_req.ljust(60, b"\x00")

        errbuf = create_string_buffer(256)
        h = wpcap.pcap_open_live(iface.name.encode(), 65535, 0, 100, errbuf)
        if not h:
            raise OSError(errbuf.value.decode())

        hdr = POINTER(pcap_pkthdr)()
        pkt = POINTER(c_ubyte)()
        gw_mac = None
        try:
            for _ in range(3):
                if wpcap.pcap_sendpacket(h, arp_req, len(arp_req)) != 0:
                    raise OSError(wpcap.pcap_geterr(h).decode())
                deadline = time.time() + 1
                while time.time() < deadline and gw_mac is None:
                    r = wpcap.pcap_next_ex(h, byref(hdr), byref(pkt))
                    if r < 0:
                        raise OSError(wpcap.pcap_geterr(h).decode())
                    if r == 0:
                        continue
                    data = bytes(pkt[: hdr.contents.caplen])
                    if (
                        len(data) >= 42
                        and data[12:14] == b"\x08\x06"          # ARP
                        and data[20:22] == b"\x00\x02"          # reply
                        and data[28:32] == socket.inet_aton(gw_ip)
                    ):
                        gw_mac = data[22:28]
                if gw_mac:
                    break
        finally:
            wpcap.pcap_close(h)

        if gw_mac is None:
            raise OSError(f"no ARP reply from gateway {gw_ip}")

        mac = "-".join(f"{b:02x}" for b in gw_mac)
        return SenderConfig(gateway_mac=mac, src_port=random.randint(100, 65000), interf=iface)


@dataclass
class ScanConfig:
    seq_key: bytes = field(default_factory=random_seq_key)
    sender: SenderConfig = field(default_factory=SenderConfig.build)


@dataclass(frozen=True)
class Dest:
    ip: str
    port: int


    def __str__(self) -> str:
        return f"{self.ip}:{self.port}"
