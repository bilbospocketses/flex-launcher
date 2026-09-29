"""Read the colour at chosen points of a screenshot taken with `xwd -root`, and compare it.

Usage: python3 pixels.py <file.xwd> x,y=r,g,b [x,y=r,g,b ...]

Prints one line per point, "x,y r,g,b (want r,g,b)", and exits 0 only when every point is
within 4 of the colour it wants in each channel, 1 when any is not, and 2 when the file is
not a screenshot it can read.
"""
import struct
import sys

TOLERANCE = 4


def channel(value, mask):
    """Take one colour channel out of a pixel value, scaled to 0-255."""
    if mask == 0:
        return 0
    shift = (mask & -mask).bit_length() - 1
    bits = bin(mask).count("1")
    return ((value & mask) >> shift) * 255 // ((1 << bits) - 1)


def main():
    with open(sys.argv[1], "rb") as f:
        data = f.read()
    if len(data) < 100:
        print("not an xwd file: too short")
        return 2
    # The header is 25 big-endian 32-bit fields, whatever the machine; the window name follows it
    (header_size, version, pixmap_format, _depth, width, height, _xoffset, byte_order, _unit,
     _bit_order, _pad, bits_per_pixel, bytes_per_line, _visual_class, red_mask, green_mask,
     blue_mask, _bits_per_rgb, _colormap_entries, ncolors) = struct.unpack(">25I", data[:100])[:20]
    if version != 7 or pixmap_format != 2 or bits_per_pixel not in (24, 32):
        print(f"cannot read this xwd file: version {version}, format {pixmap_format}, {bits_per_pixel} bpp")
        return 2
    start = header_size + ncolors * 12
    size = bits_per_pixel // 8
    ok = True
    for arg in sys.argv[2:]:
        point, want = arg.split("=")
        x, y = (int(n) for n in point.split(","))
        want = tuple(int(n) for n in want.split(","))
        if not (0 <= x < width and 0 <= y < height):
            print(f"{point} is outside the {width} x {height} screen")
            ok = False
            continue
        offset = start + y * bytes_per_line + x * size
        value = int.from_bytes(data[offset:offset + size], "little" if byte_order == 0 else "big")
        got = tuple(channel(value, mask) for mask in (red_mask, green_mask, blue_mask))
        print(f"{point} {','.join(map(str, got))} (want {','.join(map(str, want))})")
        ok = ok and all(abs(g - w) <= TOLERANCE for g, w in zip(got, want))
    return 0 if ok else 1


sys.exit(main())
