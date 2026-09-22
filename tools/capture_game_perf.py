#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Capture Linux USB Serial/JTAG logs without changing DTR/RTS or resetting.

Flash/reset separately first. Opening with pyserial can change modem control
lines and put a running board back into download mode.
"""
import argparse
import os
from pathlib import Path
import select
import termios
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("--port", default="/dev/ttyACM0")
    parser.add_argument("--seconds", type=float, default=50)
    args = parser.parse_args()
    if args.seconds <= 0:
        parser.error("--seconds must be positive")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    fd = os.open(args.port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    try:
        attrs = termios.tcgetattr(fd)
        attrs[0] = attrs[1] = attrs[3] = 0
        attrs[2] = (attrs[2] & ~(termios.PARENB | termios.CSTOPB | termios.CSIZE |
                               termios.HUPCL | termios.CRTSCTS)) | termios.CS8 | termios.CLOCAL | termios.CREAD
        attrs[4] = attrs[5] = termios.B115200
        attrs[6][termios.VMIN] = attrs[6][termios.VTIME] = 0
        termios.tcsetattr(fd, termios.TCSANOW, attrs)
        deadline = time.monotonic() + args.seconds
        with args.output.open("wb") as output:
            while time.monotonic() < deadline:
                if select.select([fd], [], [], min(.2, max(0, deadline-time.monotonic())))[0]:
                    data = os.read(fd, 8192)
                    if not data:
                        raise RuntimeError("Serial device disconnected")
                    output.write(data)
                    output.flush()
    finally:
        os.close(fd)


if __name__ == "__main__":
    main()
