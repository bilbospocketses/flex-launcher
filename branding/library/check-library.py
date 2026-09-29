"""CI check for the icon library. Needs Pillow.

Fails when:
- build-library.py --check finds a stale output (a generated file edited by hand, or a source changed
  without re-running it);
- icons.ini lists a file that does not exist, or a file in generic/ or brands/ is not listed;
- a brand PNG is not square RGBA from 512 to 1024 px, is not transparent outside the shared outline and
  opaque inside it, or does not match its sha256;
- a brand PNG has a fully opaque pixel of exactly the default chroma key, #010101, which Transparent mode
  on Windows would show through.
"""
import pathlib
import subprocess
import sys

from PIL import Image, ImageChops

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import libtools  # noqa: E402


def check_brand(library, name, keys):
    problems = []
    path = library / keys["file"]
    if libtools.sha256_file(path) != keys.get("sha256"):
        problems.append(f"[{name}] {path.name} does not match its sha256")
    with Image.open(path) as image:
        if image.mode != "RGBA":
            problems.append(f"[{name}] {path.name} is {image.mode}, not RGBA")
            return problems
        width, height = image.size
        if width != height or not libtools.BRAND_MIN <= width <= libtools.BRAND_MAX:
            problems.append(f"[{name}] {path.name} is {width}x{height}; brand art must be square, "
                            f"{libtools.BRAND_MIN} to {libtools.BRAND_MAX} px")
            return problems
        keyed = libtools.key_pixels(image)
        if keyed:
            problems.append(f"[{name}] {path.name} has {keyed} opaque pixel(s) of exactly the chroma key "
                            f"#010101, which Transparent mode on Windows shows through; make them #000000")
        alpha = image.getchannel("A")
        mask = libtools.outline_mask(width)
        outside = mask.point(lambda v: 255 if v == 0 else 0)
        inside = mask.point(lambda v: 255 if v == 255 else 0)
        if ImageChops.multiply(alpha, outside).getbbox() is not None:
            problems.append(f"[{name}] {path.name} is not transparent outside the outline")
        if ImageChops.multiply(ImageChops.invert(alpha), inside).getbbox() is not None:
            problems.append(f"[{name}] {path.name} is not opaque inside the outline")
    return problems


def check_files(library):
    problems, listed = [], set()
    for name, keys in libtools.read_sections(library / "icons.ini"):
        rel = keys.get("file", "")
        listed.add(rel)
        if not (library / rel).is_file():
            problems.append(f"[{name}] lists {rel}, which does not exist")
            continue
        group = keys.get("group")
        expected = libtools.BRAND_GROUPS if rel.startswith("brands/") else libtools.GENERIC_GROUPS
        if group not in expected:
            problems.append(f"[{name}] group '{group}' is not one of {', '.join(expected)}")
        if rel.startswith("brands/"):
            problems += check_brand(library, name, keys)
    for folder, pattern in (("generic", "*.svg"), ("brands", "*.png")):
        for path in sorted((library / folder).glob(pattern)):
            if f"{folder}/{path.name}" not in listed:
                problems.append(f"{folder}/{path.name} is not listed in icons.ini")
    return problems


def main():
    problems = []
    build = subprocess.run([sys.executable, str(libtools.HERE / "build-library.py"), "--check"],
                           capture_output=True, text=True)
    print(build.stdout.strip())
    if build.returncode:
        problems.append("build-library.py --check failed:\n" + build.stdout + build.stderr)
    problems += check_files(libtools.LIBRARY)
    if problems:
        print("\n".join(f"problem: {p}" for p in problems))
        return 1
    print("icon library: all checks passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
