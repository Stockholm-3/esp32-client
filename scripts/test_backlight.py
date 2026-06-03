#!/usr/bin/env python3
"""
Backlight test runner — sends `test_backlight` to the UART CLI and streams output.

Usage:
    python scripts/test_backlight.py [--port /dev/ttyACM0] [--baud 115200]

Requirements:
    pip install pyserial
"""

import argparse
import sys
import time

import serial

PROMPT = b"esp32-s3> "
COMMAND = "test_backlight"


def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("--port", default="/dev/ttyACM0")
    p.add_argument("--baud", type=int, default=115200)
    args = p.parse_args()

    print(f"Connecting to {args.port} @ {args.baud} …")
    ser = serial.Serial(args.port, args.baud, timeout=1)
    time.sleep(0.3)
    ser.reset_input_buffer()

    # Wake CLI
    ser.write(b"\r\n")
    time.sleep(0.3)
    ser.reset_input_buffer()

    # Send command
    ser.write((COMMAND + "\r\n").encode())

    # Stream output until prompt returns (allow up to 15s for the 5s wait + overhead)
    deadline = time.time() + 15
    buf = b""
    while time.time() < deadline:
        chunk = ser.read(256)
        if chunk:
            buf += chunk
            # Print new output as it arrives
            try:
                print(chunk.decode(errors="replace"), end="", flush=True)
            except Exception:
                pass
            if PROMPT in buf:
                break

    ser.close()
    print()

    passed = b"3/3 passed" in buf
    return 0 if passed else 1


if __name__ == "__main__":
    sys.exit(main())
