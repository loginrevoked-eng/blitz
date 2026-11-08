import logging
import hashlib
from scapy.all import IP, TCP, sr1, conf

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
    datefmt="%H:%M:%S"
)
log = logging.getLogger("probe")

SECRET = b"my-scan-secret"

def compute_seq(dst_ip, dst_port):
    h = hashlib.sha256(SECRET + dst_ip.encode() + dst_port.to_bytes(2, "big")).digest()
    return int.from_bytes(h[:4], "big")

def probe(dst_ip, dst_port, timeout=1):
    log.info(f"Target: {dst_ip}:{dst_port}")

    # show what route/interface scapy will actually use
    route = conf.route.route(dst_ip)
    log.info(f"Route -> iface={route[0]} gw={route[2]}")

    seq = compute_seq(dst_ip, dst_port)
    log.info(f"Computed seq={seq}")

    pkt = IP(dst=dst_ip) / TCP(dport=dst_port, flags="S", seq=seq)
    log.info("Sending SYN...")
    resp = sr1(pkt, timeout=timeout, verbose=0)

    if resp is None:
        log.warning(f"No response from {dst_ip}:{dst_port} (timeout or filtered)")
        return

    if not resp.haslayer(TCP):
        log.warning(f"Response received but no TCP layer (got {resp.summary()})")
        return

    flags = resp[TCP].flags
    log.info(f"Response flags: {flags}")

    if flags == "SA":
        expected = (seq + 1) % (2**32)
        if resp[TCP].ack == expected:
            log.info(f"✔ CONFIRMED OPEN: {dst_ip}:{dst_port} (ack matched)")
        else:
            log.warning(f"✘ ACK MISMATCH: got {resp[TCP].ack}, expected {expected} — discarding, not our probe")
    elif flags == "RA" or flags == "R":
        log.info(f"✘ CLOSED: {dst_ip}:{dst_port} (RST received)")
    else:
        log.info(f"? Unexpected flags from {dst_ip}:{dst_port}: {flags}")

if __name__ == "__main__":
    probe("10.4.81.1", 22)
