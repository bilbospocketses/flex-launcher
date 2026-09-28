"""Write three small solid-colour PNGs for the headless background checks.

Usage: python3 make_images.py <folder>
"""
import os
import struct
import sys
import zlib


def png(path, width, height, rgb):
    """Write a solid RGB PNG, with no library beyond the standard one."""
    raw = b"".join(b"\x00" + bytes(rgb) * width for _ in range(height))

    def chunk(kind, data):
        body = kind + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)

    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)))
        f.write(chunk(b"IDAT", zlib.compress(raw)))
        f.write(chunk(b"IEND", b""))


folder = sys.argv[1]
os.makedirs(folder, exist_ok=True)
for name, rgb in (("blue", (40, 70, 160)), ("green", (40, 140, 70)), ("red", (170, 40, 40))):
    png(os.path.join(folder, name + ".png"), 64, 36, rgb)
