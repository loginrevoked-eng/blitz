import hashlib
import hmac
import socket
import struct
from dataclasses import dataclass

from .models import Dest, ScanConfig


def calculate_checksum(data: bytes) -> int:
    if len(data) % 2 != 0:
        data += b"\x00"
    words = struct.unpack(f"!{len(data) // 2}H", data)
    total = sum(words)
    while total >> 16:
        total = (total & 0xFFFF) + (total >> 16)
    return ~total & 0xFFFF


@dataclass
class TCPPacket:
    src_port: int
    dst_port: int
    seq: int = 0
    ack: int = 0
    flags: int = 0x02
    window: int = 65535
    payload: bytes = b""

    def to_bytes(self, src_ip: str, dst_ip: str) -> bytes:
        offset_flags = (5 << 12) | self.flags
        header_no_csum = struct.pack(
            "!HHLLHHHH",
            self.src_port,
            self.dst_port,
            self.seq,
            self.ack,
            offset_flags,
            self.window,
            0,
            0,
        )
        pseudo_header = struct.pack(
            "!4s4sBBH",
            socket.inet_aton(src_ip),
            socket.inet_aton(dst_ip),
            0,
            6,
            len(header_no_csum) + len(self.payload),
        )
        csum = calculate_checksum(pseudo_header + header_no_csum + self.payload)
        header = struct.pack(
            "!HHLLHHHH",
            self.src_port,
            self.dst_port,
            self.seq,
            self.ack,
            offset_flags,
            self.window,
            csum,
            0,
        )
        return header + self.payload


@dataclass
class IPPacket:
    src_ip: str
    dst_ip: str
    tcp: TCPPacket
    ttl: int = 64
    packet_id: int = 54321

    def to_bytes(self) -> bytes:
        tcp_bytes = self.tcp.to_bytes(self.src_ip, self.dst_ip)
        total_length = 20 + len(tcp_bytes)
        src_bytes = socket.inet_aton(self.src_ip)
        dst_bytes = socket.inet_aton(self.dst_ip)
        header_no_csum = struct.pack(
            "!BBHHHBBH4s4s",
            0x45,
            0,
            total_length,
            self.packet_id,
            0,
            self.ttl,
            6,
            0,
            src_bytes,
            dst_bytes,
        )
        csum = calculate_checksum(header_no_csum)
        header = struct.pack(
            "!BBHHHBBH4s4s",
            0x45,
            0,
            total_length,
            self.packet_id,
            0,
            self.ttl,
            6,
            csum,
            src_bytes,
            dst_bytes,
        )
        return header + tcp_bytes


@dataclass
class EthernetFrame:
    dst_mac: str
    src_mac: str
    ip: IPPacket
    ethertype: int = 0x0800

    @staticmethod
    def _mac_to_bytes(mac: str) -> bytes:
        return bytes.fromhex(mac.replace(":", "").replace("-", ""))

    @staticmethod
    def _bytes_to_mac(b: bytes) -> str:
        return ":".join(f"{x:02x}" for x in b)

    @staticmethod
    def from_wire(data: bytes) -> "EthernetFrame | None":
        """Parse captured bytes into an EthernetFrame. Returns None if the
        frame isn't IPv4/TCP or is too short/malformed."""
        if len(data) < 54:
            return None
        ethertype = struct.unpack("!H", data[12:14])[0]
        if ethertype != 0x0800:
            return None

        ver_ihl = data[14]
        if ver_ihl >> 4 != 4:
            return None
        ihl = (ver_ihl & 0x0F) * 4
        if ihl < 20 or len(data) < 14 + ihl + 20:
            return None

        total_len, packet_id = struct.unpack("!HH", data[16:20])
        ttl, proto = data[22], data[23]
        if proto != 6:
            return None
        src_ip = socket.inet_ntoa(data[26:30])
        dst_ip = socket.inet_ntoa(data[30:34])

        t = 14 + ihl
        src_port, dst_port, seq, ack, offset_flags, window = struct.unpack(
            "!HHLLHH", data[t : t + 16]
        )
        tcp_hlen = (offset_flags >> 12) * 4
        if tcp_hlen < 20:
            return None
        flags = offset_flags & 0x01FF

        # IP total length trims Ethernet padding off short frames
        payload = data[t + tcp_hlen : 14 + total_len]

        return EthernetFrame(
            dst_mac=EthernetFrame._bytes_to_mac(data[0:6]),
            src_mac=EthernetFrame._bytes_to_mac(data[6:12]),
            ethertype=ethertype,
            ip=IPPacket(
                src_ip=src_ip,
                dst_ip=dst_ip,
                ttl=ttl,
                packet_id=packet_id,
                tcp=TCPPacket(
                    src_port=src_port,
                    dst_port=dst_port,
                    seq=seq,
                    ack=ack,
                    flags=flags,
                    window=window,
                    payload=payload,
                ),
            ),
        )

    def to_bytes(self) -> bytes:
        eth_header = struct.pack(
            "!6s6sH",
            self._mac_to_bytes(self.dst_mac),
            self._mac_to_bytes(self.src_mac),
            self.ethertype,
        )
        return eth_header + self.ip.to_bytes()


# --- Standalone HMAC Signing & Verification ---


def _compute_cookie(
    src_ip: str, src_port: int, dst_ip: str, dst_port: int, key: bytes
) -> int:
    tuple_bytes = struct.pack(
        "!4sH4sH",
        socket.inet_aton(src_ip),
        src_port,
        socket.inet_aton(dst_ip),
        dst_port,
    )
    digest = hmac.new(key, tuple_bytes, hashlib.sha256).digest()
    return int.from_bytes(digest[:4], "big")


def sign_packet(frame: EthernetFrame, key: bytes) -> EthernetFrame:
    """Signs an outgoing EthernetFrame in-place using HMAC-SHA256 truncated to 32 bits."""
    ip = frame.ip
    tcp = ip.tcp
    tcp.seq = _compute_cookie(ip.src_ip, tcp.src_port, ip.dst_ip, tcp.dst_port, key)
    return frame


def verify_reply(reply_frame: EthernetFrame, key: bytes) -> bool:
    """Verifies an incoming SYN-ACK frame originated from this scanner."""
    ip = reply_frame.ip
    tcp = ip.tcp
    expected_seq = _compute_cookie(
        ip.dst_ip, tcp.dst_port, ip.src_ip, tcp.src_port, key
    )
    received_seq = (tcp.ack - 1) & 0xFFFFFFFF
    return hmac.compare_digest(
        expected_seq.to_bytes(4, "big"),
        received_seq.to_bytes(4, "big"),
    )


def build_syn(*, cfg: ScanConfig, dest: Dest) -> EthernetFrame:
    iface = cfg.sender.interf
    frame = EthernetFrame(
        dst_mac=cfg.sender.gateway_mac,
        src_mac=iface.mac,
        ip=IPPacket(
            src_ip=iface.ipv4,
            dst_ip=dest.ip,
            tcp=TCPPacket(src_port=cfg.sender.src_port, dst_port=dest.port),
        ),
    )
    return sign_packet(frame, cfg.seq_key)
