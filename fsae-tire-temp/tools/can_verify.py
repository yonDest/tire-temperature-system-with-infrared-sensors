#!/usr/bin/env python3
"""
can_verify.py — Bench CAN verification for FSAE Tire Temp System
-----------------------------------------------------------------
Listens on a CAN interface and decodes tire temperature frames
(0x600–0x603) from the ATmega328P / MCP2515 node.

Requirements:
    pip install python-can

Usage:
    # SocketCAN on Linux (e.g. USB-CAN adapter)
    sudo ip link set can0 type can bitrate 500000
    sudo ip link set can0 up
    python3 can_verify.py --interface socketcan --channel can0

    # PEAK PCAN on Windows/macOS
    python3 can_verify.py --interface pcan --channel PCAN_USBBUS1

    # Virtual bus (loopback testing)
    python3 can_verify.py --interface virtual --channel test

Press Ctrl+C to stop.
"""

import argparse
import struct
import time
import sys

try:
    import can
except ImportError:
    sys.exit("python-can not found. Run: pip install python-can")

# ── Frame definitions ───────────────────────────────────────────────────────
CORNERS = {0x600: "FL", 0x601: "FR", 0x602: "RL", 0x603: "RR"}

FACTOR  = 0.02   # INT16 → °C
AMB_OFF = -40.0  # UINT8 ambient offset

# Status flag masks (Byte 7)
FLAG_OVERTEMP   = 0x80
FLAG_SENSOR_ERR = 0x40
FLAG_CRC_FAIL   = 0x20


def decode_temp(lo: int, hi: int) -> float:
    """Decode a little-endian INT16 temperature value to °C."""
    raw = struct.unpack_from("<h", bytes([lo, hi]))[0]
    return raw * FACTOR


def decode_frame(msg: can.Message) -> dict | None:
    """Parse a tire temp CAN frame. Returns None if not recognised."""
    if msg.arbitration_id not in CORNERS:
        return None
    if len(msg.data) < 8:
        return None

    d = msg.data
    corner  = CORNERS[msg.arbitration_id]
    outer   = decode_temp(d[0], d[1])
    mid     = decode_temp(d[2], d[3])
    inner   = decode_temp(d[4], d[5])
    ambient = d[6] + AMB_OFF
    flags   = d[7]
    wheel   = flags & 0x03

    status_parts = []
    if flags & FLAG_OVERTEMP:   status_parts.append("OVER-TEMP")
    if flags & FLAG_SENSOR_ERR: status_parts.append("SENSOR-ERR")
    if flags & FLAG_CRC_FAIL:   status_parts.append("CRC-FAIL")
    status_str = " | ".join(status_parts) if status_parts else "OK"

    return {
        "corner":  corner,
        "wheel":   wheel,
        "outer":   outer,
        "mid":     mid,
        "inner":   inner,
        "ambient": ambient,
        "flags":   flags,
        "status":  status_str,
    }


def in_window(temp: float) -> str:
    """Return a colour label for the temperature value."""
    if temp < 65:   return "\033[94mCOLD  \033[0m"  # blue
    if temp < 80:   return "\033[93mGRAIN \033[0m"  # yellow
    if temp <= 95:  return "\033[92mOPTIMAL\033[0m" # green
    if temp <= 109: return "\033[91mHOT   \033[0m"  # red
    return             "\033[91;1mCRIT  \033[0m"    # bold red


def print_frame(f: dict) -> None:
    ts = time.strftime("%H:%M:%S")
    print(
        f"[{ts}] {f['corner']}  "
        f"OUT {f['outer']:6.1f}°C {in_window(f['outer'])}  "
        f"MID {f['mid']:6.1f}°C {in_window(f['mid'])}  "
        f"INN {f['inner']:6.1f}°C {in_window(f['inner'])}  "
        f"Ta {f['ambient']:5.1f}°C  "
        f"[{f['status']}]"
    )


def main() -> None:
    parser = argparse.ArgumentParser(description="FSAE Tire Temp CAN Decoder")
    parser.add_argument("--interface", default="socketcan",
                        help="python-can interface type (socketcan, pcan, virtual, …)")
    parser.add_argument("--channel",   default="can0",
                        help="CAN channel name")
    parser.add_argument("--bitrate",   type=int, default=500000,
                        help="CAN bus bitrate (default 500000)")
    parser.add_argument("--timeout",   type=float, default=2.0,
                        help="Message receive timeout in seconds")
    args = parser.parse_args()

    print(f"FSAE Tire Temp Decoder — {args.interface}/{args.channel} @ {args.bitrate} bps")
    print(f"Listening for CAN IDs 0x600–0x603 …  (Ctrl+C to stop)\n")

    try:
        bus = can.interface.Bus(
            interface=args.interface,
            channel=args.channel,
            bitrate=args.bitrate,
        )
    except Exception as e:
        sys.exit(f"Failed to open CAN bus: {e}")

    frame_count = 0
    error_count = 0
    t_start = time.time()

    try:
        while True:
            msg = bus.recv(timeout=args.timeout)
            if msg is None:
                elapsed = time.time() - t_start
                print(f"  [timeout] No message in {args.timeout}s "
                      f"({frame_count} frames, {error_count} errors, {elapsed:.0f}s elapsed)")
                continue

            frame = decode_frame(msg)
            if frame is None:
                continue   # ignore unrelated IDs

            frame_count += 1
            if frame["flags"] & (FLAG_SENSOR_ERR | FLAG_CRC_FAIL):
                error_count += 1

            print_frame(frame)

    except KeyboardInterrupt:
        elapsed = time.time() - t_start
        rate = frame_count / elapsed if elapsed > 0 else 0
        print(f"\n── Summary ────────────────────────────────")
        print(f"  Duration : {elapsed:.1f}s")
        print(f"  Frames   : {frame_count}  ({rate:.1f} Hz)")
        print(f"  Errors   : {error_count}")
        bus.shutdown()


if __name__ == "__main__":
    main()
