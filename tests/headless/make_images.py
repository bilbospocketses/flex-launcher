"""Write small PNGs for the headless checks.

Usage: python3 make_images.py <folder>
           three solid-colour PNGs for the background checks
       python3 make_images.py --keyed <file.png> <r,g,b> <rgb|rgba>
           a 64 px icon for the chroma key check: a frame of the colour given, 16 px wide, around a
           square of the default chroma key, #010101, fully opaque; RGB, or RGBA with alpha 255
"""
import os
import struct
import sys
import zlib


def png(path, width, height, rows, colour_type):
    """Write a PNG from its rows of pixel bytes (colour type 2 = RGB, 6 = RGBA), with no library
    beyond the standard one."""
    raw = b"".join(b"\x00" + row for row in rows)

    def chunk(kind, data):
        body = kind + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)

    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, colour_type, 0, 0, 0)))
        f.write(chunk(b"IDAT", zlib.compress(raw)))
        f.write(chunk(b"IEND", b""))


def keyed(path, frame, alpha):
    """Write the chroma key check's icon: the frame colour, with the key in a square in the middle."""
    key = (1, 1, 1)
    extra = b"\xff" if alpha else b""
    rows = []
    for y in range(64):
        rows.append(b"".join(bytes(key if 16 <= x < 48 and 16 <= y < 48 else frame) + extra for x in range(64)))
    png(path, 64, 64, rows, 6 if alpha else 2)


if sys.argv[1] == "--keyed":
    keyed(sys.argv[2], tuple(int(n) for n in sys.argv[3].split(",")), sys.argv[4] == "rgba")
else:
    folder = sys.argv[1]
    os.makedirs(folder, exist_ok=True)
    for name, rgb in (("blue", (40, 70, 160)), ("green", (40, 140, 70)), ("red", (170, 40, 40))):
        png(os.path.join(folder, name + ".png"), 64, 36, [bytes(rgb) * 64] * 36, 2)
