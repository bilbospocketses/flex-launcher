#!/usr/bin/env python3
"""make-test-images.py: the picture folder for StreamFlex's Linux hands-on check.

PURPOSE
    Makes one folder whose contents test the folder browser and the slideshow scan:
      rouge.png   solid red    (255, 0, 0)  PNG
      VERT.PNG    solid green  (0, 160, 0)  PNG with an upper-case extension
      BLEU.JPG    solid blue   (0, 0, 255)  JPEG with an upper-case extension
      .caché.png  a hidden image: never listed, never counted
      notes.txt   not an image: never listed, never counted
    So the browser lists exactly BLEU.JPG, rouge.png and VERT.PNG, "Use this folder" says
    3 images, and the slideshow logs "Found 3 images". A scan that ignores upper-case
    extensions finds 1 (plus the hidden one if it also counts hidden files: 2), so the
    count alone tells the fixed scan from the old one.

USAGE
    make-test-images.py FOLDER [--owner USER]
    FOLDER and its parents are made if missing. --owner hands the files to USER (run as
    root for that). Prints what it made.

REQUIREMENTS
    python3 standard library. BLEU.JPG is a real JPEG when python3-gi with GdkPixbuf is
    installed (Ubuntu desktop has it); otherwise it holds PNG data under the .JPG name,
    which SDL_image still opens (it reads the content, not the name), and the script
    says so.
"""

import argparse
import os
import pwd
import struct
import sys
import zlib

WIDTH, HEIGHT = 640, 360


def png_bytes(rgb, width=WIDTH, height=HEIGHT):
    def chunk(kind, data):
        body = kind + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)

    row = b"\x00" + bytes(rgb) * width          # filter type 0, then RGB pixels
    header = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header)
            + chunk(b"IDAT", zlib.compress(row * height, 9)) + chunk(b"IEND", b""))


def write_jpeg(path, rgb):
    """A real JPEG through GdkPixbuf; False when it is not available."""
    try:
        import gi
        gi.require_version("GdkPixbuf", "2.0")
        from gi.repository import GdkPixbuf
    except (ImportError, ValueError):
        return False
    pixbuf = GdkPixbuf.Pixbuf.new(GdkPixbuf.Colorspace.RGB, False, 8, WIDTH, HEIGHT)
    pixbuf.fill((rgb[0] << 24) | (rgb[1] << 16) | (rgb[2] << 8) | 0xFF)
    pixbuf.savev(path, "jpeg", ["quality"], ["95"])
    return True


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("folder")
    parser.add_argument("--owner", help="give the folder and files to this user")
    args = parser.parse_args()

    os.makedirs(args.folder, exist_ok=True)
    made = []
    for name, rgb in (("rouge.png", (255, 0, 0)), ("VERT.PNG", (0, 160, 0)),
                      (".caché.png", (255, 255, 0))):
        with open(os.path.join(args.folder, name), "wb") as f:
            f.write(png_bytes(rgb))
        made.append(name)
    jpeg = os.path.join(args.folder, "BLEU.JPG")
    if write_jpeg(jpeg, (0, 0, 255)):
        made.append("BLEU.JPG (JPEG)")
    else:
        with open(jpeg, "wb") as f:
            f.write(png_bytes((0, 0, 255)))
        made.append("BLEU.JPG (PNG data: GdkPixbuf is not available)")
    with open(os.path.join(args.folder, "notes.txt"), "w", encoding="utf-8") as f:
        f.write("Not an image. StreamFlex must not list or count this file.\n")
    made.append("notes.txt")

    if args.owner:
        user = pwd.getpwnam(args.owner)
        # The folder and every parent under the owner's home, so the user can read them all
        home = os.path.realpath(user.pw_dir)
        path = os.path.realpath(args.folder)
        while path.startswith(home + os.sep):
            os.chown(path, user.pw_uid, user.pw_gid)
            path = os.path.dirname(path)
        for name in os.listdir(args.folder):
            os.chown(os.path.join(args.folder, name), user.pw_uid, user.pw_gid)

    print("made in %s: %s" % (args.folder, ", ".join(made)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
