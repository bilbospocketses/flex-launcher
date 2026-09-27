"""usage: python parse-ico.py <file.ico>

Struct-parse an .ico: entry count, sizes, bpp, payload kind, offsets in bounds, no duplicates.
Exits non-zero (AssertionError) on any structural defect.
"""
import struct
import sys

with open(sys.argv[1], "rb") as fh:
    data = fh.read()
res, typ, n = struct.unpack_from("<HHH", data, 0)
assert (res, typ) == (0, 1), (res, typ)
seen = set()
for i in range(n):
    w, h, cc, r, planes, bpp, size, off = struct.unpack_from("<BBBBHHII", data, 6 + 16 * i)
    w, h = w or 256, h or 256
    kind = "PNG" if data[off:off + 8] == b"\x89PNG\r\n\x1a\n" else "BMP"
    assert w == h and bpp == 32 and off + size <= len(data) and w not in seen, (i, w, h, bpp, off, size)
    seen.add(w)
    print(f"entry {i}: {w:3d}x{h:<3d} {bpp}bpp {kind} {size:7d} B @ {off}")
print(f"OK: {n} entries, file {len(data)} B")
