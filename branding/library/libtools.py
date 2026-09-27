"""Shared paths and file formats for the icon library tools. Standard library only
(outline_mask, added with the brand tools, imports Pillow when it is called)."""
import hashlib
import pathlib
import re

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[1]
LIBRARY = ROOT / "assets" / "icons" / "library"
GLYPHS = HERE / "glyphs"
BRANDS_INI = HERE / "brands.ini"

NAME_RE = re.compile(r"^[a-z0-9-]{1,32}$")
GENERIC_GROUPS = ("system", "media", "general", "devices")
BRAND_GROUPS = ("video", "free-tv", "servers", "music", "games", "web")
OUTLINE_RADIUS = 0.22           # corner radius as a share of the side: the app icon's plate, 54 of 244
BRAND_MIN, BRAND_MAX = 512, 1024
BRAND_KEYS = ("title", "group", "owner", "android", "source", "art", "fill", "size", "sha256")


def read_sections(path):
    """Read an INI file into [(section, {key: value})] in file order. A repeated section or key is an error."""
    sections, current, seen = [], None, set()
    for number, raw in enumerate(pathlib.Path(path).read_text(encoding="utf-8").splitlines(), 1):
        line = raw.strip()
        if not line or line.startswith(";") or line.startswith("#"):
            continue
        if line.startswith("[") and line.endswith("]"):
            name = line[1:-1].strip()
            if name in seen:
                raise ValueError(f"{path}:{number}: section [{name}] appears twice")
            seen.add(name)
            current = (name, {})
            sections.append(current)
            continue
        if "=" not in line or current is None:
            raise ValueError(f"{path}:{number}: expected 'key = value' inside a section: {raw!r}")
        key, value = (part.strip() for part in line.split("=", 1))
        if key in current[1]:
            raise ValueError(f"{path}:{number}: key '{key}' appears twice in [{current[0]}]")
        current[1][key] = value
    return sections


def write_brands(sections, path=BRANDS_INI):
    """Write brands.ini with a fixed key order, so hand edits and tool edits never fight over layout."""
    lines = [
        "; Brand icons, one section per icon, in the order the manifest lists them.",
        "; import-brand.py writes source, art, fill, size and sha256; title, group, owner and android are edited by hand.",
        "; After any change, run build-library.py to regenerate assets/icons/library/icons.ini and brands/NOTICE.md.",
    ]
    for name, keys in sections:
        lines += ["", f"[{name}]"]
        lines += [f"{key} = {keys[key]}" for key in BRAND_KEYS if keys.get(key)]
    pathlib.Path(path).write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")


def sha256_file(path):
    return hashlib.sha256(pathlib.Path(path).read_bytes()).hexdigest()
