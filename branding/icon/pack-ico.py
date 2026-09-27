"""usage: python pack-ico.py <frames_dir> <out.ico>

Packs the ten rendered frames into a Windows .ico: the 256 entry as PNG, every
other entry as 32bpp BMP + AND mask (the layout Explorer, WIC and rc.exe expect).
Frames are read as <frames_dir>/ico_<size, 3 digits>.png, written by build-icon.ps1.
"""
import io
import os
import struct
import sys

import cv2
import numpy as np

SIZES = [256, 128, 96, 64, 48, 40, 32, 24, 20, 16]
frames, out = sys.argv[1], sys.argv[2]


def bmp_payload(path, s):
    img = cv2.imread(path, cv2.IMREAD_UNCHANGED)          # BGRA
    assert img.shape == (s, s, 4), (path, img.shape)
    hdr = struct.pack("<IiiHHIIiiII", 40, s, 2 * s, 1, 32, 0, 0, 0, 0, 0, 0)
    color = np.ascontiguousarray(img[::-1]).tobytes()     # bottom-up BGRA
    stride = ((s + 31) // 32) * 4
    mask = bytearray()
    for row in img[::-1, :, 3]:
        mask += np.packbits((row < 128).astype(np.uint8)).tobytes().ljust(stride, b"\0")
    data = hdr + color + bytes(mask)
    assert len(data) == 40 + s * s * 4 + stride * s
    return data


entries = []
for s in SIZES:
    p = os.path.join(frames, f"ico_{s:03d}.png")
    with open(p, "rb") as fh:
        raw = fh.read()
    entries.append((s, raw if s == 256 else bmp_payload(p, s)))

buf = io.BytesIO()
buf.write(struct.pack("<HHH", 0, 1, len(entries)))
off = 6 + 16 * len(entries)
for s, data in entries:
    d = 0 if s == 256 else s                              # 256 is encoded as 0
    buf.write(struct.pack("<BBBBHHII", d, d, 0, 0, 1, 32, len(data), off))
    off += len(data)
for _, data in entries:
    buf.write(data)
with open(out, "wb") as fh:
    fh.write(buf.getvalue())
print("wrote", out, len(buf.getvalue()), "bytes")
