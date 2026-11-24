import argparse
import socket


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("target")
    parser.add_argument("-p", "--port", type=int)
    parser.add_argument("--timeout", type=float, default=5.0)

    args = parser.parse_args()

    if args.port is None:
        if ":" not in args.target:
            parser.error("provide -p PORT or specify IP:PORT")

        args.target, port = args.target.rsplit(":", 1)

        try:
            args.port = int(port)
        except ValueError:
            parser.error("invalid port")

    if not 1 <= args.port <= 65535:
        parser.error("port must be between 1 and 65535")

    return args


def receive_handshake(host: str, port: int, timeout: float):
    try:
        with socket.create_connection((host, port), timeout=timeout) as sock:
            sock.settimeout(timeout)

            try:
                payload = sock.recv(4096)

                if payload:
                    print(f"[+] Received {len(payload)} bytes")
                    print(f"Raw: {payload!r}")
                    print(f"Hex: {payload.hex()}")
                else:
                    print("[-] Connection closed without payload.")

            except socket.timeout:
                print("[!] Timed out waiting for handshake.")

    except OSError as e:
        print(f"[!] Connection failed: {e}")


def main():
    args = parse_args()
    receive_handshake(args.target, args.port, args.timeout)


if __name__ == "__main__":
    main()