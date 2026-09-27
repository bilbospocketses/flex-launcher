"""Build the StreamFlex icon SVG masters from one shared geometry.

full  -> frames 256..48, Linux hicolor/scalable SVG (docs/streamflex.svg), 48 px Linux PNG
small -> frames 40..16 and the 32 px favicon: bolder, no glow, no fine detail

Writes streamflex-full.svg and streamflex-small.svg next to this script. Edit the
geometry or palette HERE and re-run build-icon.ps1; never hand-edit the SVGs.
The palette was sampled by k-means from the original 2816x1536 logo (see README.md).
"""
import math
import os
import sys

OUT = os.path.dirname(os.path.abspath(__file__))

# ---- palette (banner k-means) ----
NAVY, TEAL_BG = "#061436", "#07606C"
RING_LIGHT, RING_TEAL, RING_BLUE, RING_INDIGO = "#5BDFD7", "#2BBFBC", "#1B8AB5", "#424AB2"
RIBBON_BLUE, RIBBON_MAGENTA, RIBBON_ORANGE, RIBBON_AMBER = "#1FA1DC", "#B24377", "#F3643A", "#F7A948"
GLASS_HI, GLASS_MID, GLASS_LO, GLASS_DEEP = "#E6F6FA", "#BFE5F0", "#8CC4D6", "#265D88"
CYAN_GLOW = "#3FDAF2"


def pt(cx, cy, r, deg):
    a = math.radians(deg)
    return cx + r * math.cos(a), cy + r * math.sin(a)


def arc(cx, cy, r, a0, a1):
    """Clockwise (screen) arc from a0 to a1 degrees, a1 > a0."""
    x0, y0 = pt(cx, cy, r, a0)
    x1, y1 = pt(cx, cy, r, a1)
    large = 1 if (a1 - a0) % 360 > 180 else 0
    return f"M {x0:.2f} {y0:.2f} A {r} {r} 0 {large} 1 {x1:.2f} {y1:.2f}"


def arrowhead(base, direction, length, half_w):
    bx, by = base
    dx, dy = direction
    n = math.hypot(dx, dy)
    dx, dy = dx / n, dy / n
    tip = (bx + dx * length, by + dy * length)
    px, py = -dy, dx
    l = (bx + px * half_w, by + py * half_w)
    r = (bx - px * half_w, by - py * half_w)
    return f"M {tip[0]:.2f} {tip[1]:.2f} L {l[0]:.2f} {l[1]:.2f} L {r[0]:.2f} {r[1]:.2f} Z", tip


def build(variant):
    full = variant == "full"
    # Composition (from the banner): a closed ring loops out from BEHIND the rising ribbon,
    # whose shaft enters from the plate's bottom edge and ends in an arrowhead above the ring's upper-left.
    if full:
        plate = dict(x=6, y=6, s=244, rx=54)
        cx, cy, r, ring_w = 148, 142, 70, 24
        shaft = "M 58 262 C 90 206, 52 150, 80 72"
        shaft_end, shaft_dir = (80, 72), (28, -78)
        shaft_w, head_len, head_hw = 22, 46, 28
        win = dict(w=74, h=56, rx=8, bar=14)
    else:
        plate = dict(x=0, y=0, s=256, rx=52)
        cx, cy, r, ring_w = 150, 146, 66, 32
        shaft = "M 56 270 C 84 206, 58 150, 84 80"
        shaft_end, shaft_dir = (84, 80), (26, -70)
        shaft_w, head_len, head_hw = 32, 58, 40
        win = dict(w=72, h=56, rx=10, bar=18)

    ring_path = arc(cx, cy, r, 0, 359.99)
    head_d, tip = arrowhead(shaft_end, shaft_dir, head_len, head_hw)
    wx, wy = cx - win["w"] / 2, cy - win["h"] / 2
    px, py, ps, prx = plate["x"], plate["y"], plate["s"], plate["rx"]

    defs = f"""
    <linearGradient id="plate" x1="{px}" y1="{py + ps}" x2="{px + ps}" y2="{py}" gradientUnits="userSpaceOnUse">
      <stop offset="0" stop-color="{NAVY}"/><stop offset="1" stop-color="{TEAL_BG}"/>
    </linearGradient>
    <radialGradient id="halo" cx="{cx}" cy="{cy}" r="{r + 60}" gradientUnits="userSpaceOnUse">
      <stop offset="0.45" stop-color="{RING_TEAL}" stop-opacity="{0.55 if full else 0.35}"/>
      <stop offset="1" stop-color="{RING_TEAL}" stop-opacity="0"/>
    </radialGradient>
    <linearGradient id="ring" x1="{cx + r}" y1="{cy - r}" x2="{cx - r}" y2="{cy + r}" gradientUnits="userSpaceOnUse">
      <stop offset="0" stop-color="{RING_LIGHT}"/><stop offset="0.45" stop-color="{RING_TEAL}"/>
      <stop offset="0.75" stop-color="{RING_BLUE}"/><stop offset="1" stop-color="{RING_INDIGO}"/>
    </linearGradient>
    <linearGradient id="ribbon" x1="62" y1="250" x2="{tip[0]:.1f}" y2="{tip[1]:.1f}" gradientUnits="userSpaceOnUse">
      <stop offset="0" stop-color="{RIBBON_BLUE}"/><stop offset="0.38" stop-color="{RIBBON_MAGENTA}"/>
      <stop offset="0.66" stop-color="{RIBBON_ORANGE}"/><stop offset="1" stop-color="{RIBBON_AMBER}"/>
    </linearGradient>
    <linearGradient id="glass" x1="0" y1="{wy}" x2="0" y2="{wy + win['h']}" gradientUnits="userSpaceOnUse">
      <stop offset="0" stop-color="{GLASS_HI}"/><stop offset="1" stop-color="{GLASS_MID}"/>
    </linearGradient>
    <clipPath id="plateclip"><rect x="{px}" y="{py}" width="{ps}" height="{ps}" rx="{prx}"/></clipPath>"""
    if full:
        # Filter-free glow: a ring's glow is radially symmetric, so a radial-gradient annulus reproduces it
        # and renders identically in librsvg, Qt SVG (no filters before Qt 6.7), browsers and Inkscape.
        g_out = r + ring_w / 2 + 24
        inner = (r - ring_w / 2 - 14) / g_out
        edge_in = (r - ring_w / 2) / g_out
        edge_out = (r + ring_w / 2) / g_out
        defs += f"""
    <radialGradient id="glow" cx="{cx}" cy="{cy}" r="{g_out}" gradientUnits="userSpaceOnUse">
      <stop offset="{inner:.4f}" stop-color="{CYAN_GLOW}" stop-opacity="0"/>
      <stop offset="{edge_in:.4f}" stop-color="{CYAN_GLOW}" stop-opacity="0.55"/>
      <stop offset="{edge_out:.4f}" stop-color="{CYAN_GLOW}" stop-opacity="0.55"/>
      <stop offset="{(edge_out + 1) / 2:.4f}" stop-color="{CYAN_GLOW}" stop-opacity="0.16"/>
      <stop offset="1" stop-color="{CYAN_GLOW}" stop-opacity="0"/>
    </radialGradient>"""

    body = [f'<rect x="{px}" y="{py}" width="{ps}" height="{ps}" rx="{prx}" fill="url(#plate)"/>',
            '<g clip-path="url(#plateclip)">',
            f'<circle cx="{cx}" cy="{cy}" r="{r + 60}" fill="url(#halo)"/>']
    if full:
        body.append(f'<circle cx="{cx}" cy="{cy}" r="{r + ring_w / 2 + 24}" fill="url(#glow)"/>')
    body.append(f'<path d="{ring_path}" fill="none" stroke="url(#ring)" stroke-width="{ring_w}" stroke-linecap="round"/>')
    if full:
        # glossy highlight along the upper-right of the ring
        body.append(f'<path d="{arc(cx, cy, r + ring_w * 0.22, 250, 350)}" fill="none" stroke="#C8FFFA" '
                    f'stroke-width="3" stroke-linecap="round" opacity="0.75"/>')

    # window glyph in the ring
    if full:
        # soft shadow faked with stacked translucent rects (no filter)
        for grow, op in ((g, 0.045) for g in (7, 6, 5, 4, 3, 2, 1, 0)):
            body.append(f'<rect x="{wx + 2 - grow}" y="{wy + 4 - grow}" width="{win["w"] + 2 * grow}" '
                        f'height="{win["h"] + 2 * grow}" rx="{win["rx"] + grow}" fill="#021026" opacity="{op}"/>')
    body.append(f'<rect x="{wx}" y="{wy}" width="{win["w"]}" height="{win["h"]}" rx="{win["rx"]}" fill="url(#glass)"/>')
    body.append(f'<path d="M {wx} {wy + win["bar"]} V {wy + win["rx"]} Q {wx} {wy} {wx + win["rx"]} {wy} '
                f'H {wx + win["w"] - win["rx"]} Q {wx + win["w"]} {wy} {wx + win["w"]} {wy + win["rx"]} '
                f'V {wy + win["bar"]} Z" fill="{GLASS_DEEP}"/>')
    if full:
        for i, c in enumerate((RIBBON_ORANGE, RIBBON_AMBER, CYAN_GLOW)):
            body.append(f'<circle cx="{wx + 10 + i * 9}" cy="{wy + win["bar"] / 2}" r="3" fill="{c}"/>')
        cy0 = wy + win["bar"] + 7
        ch = win["h"] - win["bar"] - 14
        body.append(f'<rect x="{wx + 7}" y="{cy0}" width="18" height="{ch}" rx="3" fill="{GLASS_LO}"/>')
        body.append(f'<rect x="{wx + 30}" y="{cy0}" width="{win["w"] - 37}" height="{ch * 0.42:.1f}" rx="3" fill="{GLASS_LO}"/>')
        body.append(f'<rect x="{wx + 30}" y="{cy0 + ch * 0.58:.1f}" width="{(win["w"] - 41) / 2:.1f}" height="{ch * 0.42:.1f}" rx="3" fill="{GLASS_LO}"/>')
        body.append(f'<rect x="{wx + 34 + (win["w"] - 41) / 2:.1f}" y="{cy0 + ch * 0.58:.1f}" width="{(win["w"] - 41) / 2:.1f}" height="{ch * 0.42:.1f}" rx="3" fill="{GLASS_LO}"/>')

    # rising ribbon + arrowhead, in front of the ring
    if full:
        for grow, op in ((12, 0.10), (6, 0.18)):
            body.append(f'<path d="{shaft}" fill="none" stroke="#021026" stroke-width="{shaft_w + grow}" '
                        f'stroke-linecap="round" opacity="{op}"/>')
    body.append(f'<path d="{shaft}" fill="none" stroke="url(#ribbon)" stroke-width="{shaft_w}" stroke-linecap="round"/>')
    body.append(f'<path d="{head_d}" fill="url(#ribbon)" stroke="url(#ribbon)" stroke-width="{3 if full else 4}" stroke-linejoin="round"/>')
    body.append('</g>')
    if full:
        body.append(f'<rect x="{px + 0.75}" y="{py + 0.75}" width="{ps - 1.5}" height="{ps - 1.5}" rx="{prx - 0.75}" '
                    f'fill="none" stroke="#8BF8F3" stroke-opacity="0.22" stroke-width="1.5"/>')
    else:
        body.append(f'<rect x="{px + 4}" y="{py + 4}" width="{ps - 8}" height="{ps - 8}" rx="{prx - 4}" '
                    f'fill="none" stroke="#8BF8F3" stroke-opacity="0.30" stroke-width="8"/>')

    svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="256" height="256" viewBox="0 0 256 256">\n'
           f'  <title>StreamFlex</title>\n  <defs>{defs}\n  </defs>\n  ' + "\n  ".join(body) + "\n</svg>\n")
    path = os.path.join(OUT, f"streamflex-{variant}.svg")
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(svg)
    print("wrote", path)


for v in sys.argv[1:] or ("full", "small"):
    build(v)
