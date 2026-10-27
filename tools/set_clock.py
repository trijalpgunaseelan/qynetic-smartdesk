#!/usr/bin/env python3
"""Set SmartDesk time over USB, without Wi-Fi or Internet (requires pyserial)."""
import argparse
import time
import serial

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--port", default="/dev/cu.usbmodem101")
args = parser.parse_args()
with serial.Serial(args.port, 115200, timeout=1) as port:
    port.reset_input_buffer()
    port.write(("TIME " + str(int(time.time())) + "\n").encode("ascii"))
    port.flush()
    deadline = time.monotonic() + 5
    while time.monotonic() < deadline:
        line = port.readline()
        if b"Clock set from USB" in line:
            print("Desk clock set. It will keep ticking without a network while powered.")
            break
    else:
        raise SystemExit("No clock acknowledgement. Check firmware version and close serial monitors.")
