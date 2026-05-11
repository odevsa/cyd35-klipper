#!/usr/bin/env python3
"""
screenshot.py - capture a screenshot from the CYD device over Serial.

Usage:
    python3 screenshot.py [port] [baud] [output.png]

The firmware uses readRect which stores each pixel byte-swapped (for pushRect
compatibility). Each row is sent as W×4 hex chars (raw uint16 in big-endian).

Decode: undo the byte-swap → standard RGB-565 → RGB-888.

Dependencies:  pip install pyserial Pillow
"""

import sys
import os
import datetime
import time
import serial
from PIL import Image

COMMAND      = b"screenshot\n"
BOOT_DRAIN_S = 5
DATA_TIMEOUT = 120

PORT = sys.argv[1] if len(sys.argv) > 1 else "/dev/ttyUSB0"
BAUD = int(sys.argv[2]) if len(sys.argv) > 2 else 921600
default_out = f"screenshot/{datetime.datetime.now().strftime('%Y%m%d_%H%M%S')}.png"
OUT = sys.argv[3] if len(sys.argv) > 3 else default_out

os.makedirs(os.path.dirname(OUT) or ".", exist_ok=True)

# ── 1. Open port ──────────────────────────────────────────────────────────────
print(f"Connecting to {PORT} @ {BAUD} baud …")
try:
    ser = serial.Serial(PORT, BAUD, timeout=1)
except serial.SerialException as e:
    sys.exit(f"Error opening port: {e}")

# ── 2. Drain boot noise ───────────────────────────────────────────────────────
print(f"Waiting {BOOT_DRAIN_S}s for device to be ready …")
boot_deadline = time.monotonic() + BOOT_DRAIN_S
while time.monotonic() < boot_deadline:
    ser.read(512)
ser.reset_input_buffer()

# ── 3. Send command ───────────────────────────────────────────────────────────
print("Sending screenshot command …")
ser.write(COMMAND)
ser.flush()

# ── 4. Wait for header (print PIXEL_VERIFY diagnostic if present) ─────────────
print("Waiting for header …")
W = H = None
deadline = time.monotonic() + DATA_TIMEOUT


def decode_rgb565(raw_bswapped):
    """Undo readRect byte-swap then decode RGB-565 to (r8, g8, b8)."""
    c = ((raw_bswapped & 0xFF) << 8) | ((raw_bswapped >> 8) & 0xFF)
    r8 = ((c >> 11) & 0x1F) << 3
    g8 = ((c >>  5) & 0x3F) << 2
    b8 = ( c        & 0x1F) << 3
    return r8, g8, b8


while time.monotonic() < deadline:
    line = ser.readline().decode("ascii", errors="replace").strip()
    if line.startswith("PIXEL_VERIFY:"):
        print(f"\n[Diagnostic] {line}")
        # Format: "PIXEL_VERIFY: raw=0xXXXX decoded=0xXXXX R=NNN G=NNN B=NNN"
        # Parse only space-separated key=value tokens, stop at first non-kv word.
        try:
            fields = {}
            for token in line[len("PIXEL_VERIFY:"):].split():
                if "=" in token:
                    k, v = token.split("=", 1)
                    if k not in fields:          # keep FIRST occurrence only
                        fields[k] = v
            raw = int(fields["raw"], 16)
            r, g, b = decode_rgb565(raw)
            print(f"  Pixel (10,15):  raw readRect=0x{raw:04X}  → R={r} G={g} B={b}")
            exp_r, exp_g, exp_b = 16, 20, 16  # COLOR_NAV_BG = 0x10A2
            diff = abs(r - exp_r) + abs(g - exp_g) + abs(b - exp_b)
            if diff <= 24:
                print(f"  ✓ Close to expected COLOR_NAV_BG (R=16 G=20 B=16) — readback OK!")
            else:
                print(f"  ? Differs from expected COLOR_NAV_BG (R={exp_r} G={exp_g} B={exp_b}).")
                print(f"    If colours look wrong, try lowering SPI_READ_FREQUENCY further in User_Setup.h.")
        except Exception:
            pass
        print()
    elif line.startswith("SCREENSHOT:"):
        try:
            w_str, h_str = line[len("SCREENSHOT:"):].split("x")
            W, H = int(w_str), int(h_str)
            print(f"Header received: {W}x{H}")
            break
        except (ValueError, IndexError):
            pass

if W is None or H is None:
    ser.close()
    sys.exit("No SCREENSHOT header received. Is the firmware a DEBUG build?")

# ── 5. Receive rows of raw readRect hex (4 chars per pixel) ──────────────────
# readRect stores each pixel as byte-swapped uint16. We receive 4 hex chars per
# pixel (the raw swapped value), undo the swap, then decode RGB-565 → RGB-888.
expected_chars = W * 4
raw_buf = []      # list of raw uint16 (byte-swapped as stored by readRect)
rows_received = 0

print(f"Receiving {H} rows …")

while rows_received < H and time.monotonic() < deadline:
    line = ser.readline().decode("ascii", errors="replace").strip()

    if line == "END":
        break

    if len(line) != expected_chars:
        print(f"  Warning: row {rows_received} has {len(line)} chars "
              f"(expected {expected_chars}) – skipped.")
        continue

    try:
        for x in range(W):
            raw_buf.append(int(line[x * 4: x * 4 + 4], 16))
    except ValueError:
        print(f"  Warning: row {rows_received} contains invalid hex – skipped.")
        continue

    rows_received += 1
    if rows_received % 32 == 0 or rows_received == H:
        print(f"  {rows_received}/{H} rows received …")

ser.close()

if rows_received == 0:
    sys.exit("No pixel data received.")
if rows_received < H:
    print(f"Warning: received only {rows_received}/{H} rows.")

H_actual = rows_received

# ── 6. Diagnostic: first pixel ────────────────────────────────────────────────
if raw_buf:
    r, g, b = decode_rgb565(raw_buf[0])
    print(f"\nFirst pixel (0,0): raw=0x{raw_buf[0]:04X}  → R={r} G={g} B={b}")
    print(f"  Expected status-bar background COLOR_NAV_BG ≈ R=16 G=20 B=16\n")

# ── 7. Build RGB image ────────────────────────────────────────────────────────
buf = bytearray(W * H_actual * 3)
for i, raw in enumerate(raw_buf):
    r, g, b = decode_rgb565(raw)
    buf[i * 3]     = r
    buf[i * 3 + 1] = g
    buf[i * 3 + 2] = b

img = Image.frombytes("RGB", (W, H_actual), bytes(buf))
img.save(OUT)
print(f"Saved → {OUT}")

