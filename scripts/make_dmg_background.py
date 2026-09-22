#!/usr/bin/env python3
"""Generate the DMG window background: a light canvas with a right arrow that
points from the app icon to the Applications shortcut.

Pure standard library (writes the PNG by hand via zlib), so it works anywhere
python3 is available.

Usage: make_dmg_background.py <out.png> [width height]
"""
import struct
import sys
import zlib

BG = (245, 246, 248)
ARROW = (120, 132, 145)


def write_png(path, width, height, pixels):
    def chunk(tag, data):
        return (struct.pack(">I", len(data)) + tag + data
                + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF))

    raw = bytearray()
    for y in range(height):
        raw.append(0)                       # filter type 0
        raw += bytes(pixels[y])

    ihdr = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)  # 8-bit RGB
    png = (b"\x89PNG\r\n\x1a\n"
           + chunk(b"IHDR", ihdr)
           + chunk(b"IDAT", zlib.compress(bytes(raw), 9))
           + chunk(b"IEND", b""))
    with open(path, "wb") as fh:
        fh.write(png)


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "background.png"
    w = int(sys.argv[2]) if len(sys.argv) > 2 else 660
    h = int(sys.argv[3]) if len(sys.argv) > 3 else 420

    # Match the Finder icon rows: app at x=165, Applications at x=495, y=190.
    cy = 190
    x0, x1, tip = 285, 390, 420
    shaft = 9
    head = 36

    row = bytes(BG) * w
    pixels = [bytearray(row) for _ in range(h)]

    def put(x, y):
        if 0 <= x < w and 0 <= y < h:
            i = x * 3
            pixels[y][i:i + 3] = bytes(ARROW)

    for x in range(x0, x1):
        for dy in range(-shaft, shaft + 1):
            put(x, cy + dy)

    for x in range(x1, tip + 1):
        half = int(head * (tip - x) / (tip - x1 + 1))
        for dy in range(-half, half + 1):
            put(x, cy + dy)

    write_png(out, w, h, pixels)
    print(f"wrote {out} ({w}x{h})")


if __name__ == "__main__":
    main()
