# StreamFlex app icon — source

Everything needed to regenerate the app icon. The icon itself ships in four places, and this folder produces all of them:

| Output | Used as |
|---|---|
| `docs/streamflex.svg` | Linux scalable icon (`hicolor/scalable/apps`), installed by `CMakeLists.txt` |
| `docs/streamflex.png` | Linux 48×48 icon (`hicolor/48x48/apps`) |
| `docs/assets/icons/favicon.png` | Documentation site favicon (32×32) |
| `config/streamflex.ico` | Windows executable icon, compiled in through `config/streamflex.rc` |

## Files

| File | What it is |
|---|---|
| `build-svg.py` | **The source.** One shared geometry and palette, written out as two vector masters. Edit this, not the SVGs |
| `streamflex-full.svg` | Master for 256–48 px: glow, highlight, window detail. Identical to `docs/streamflex.svg` |
| `streamflex-small.svg` | Master for 40–16 px and the favicon: bolder strokes, no glow, no fine detail, so it stays legible when tiny |
| `build-icon.ps1` | Runs the whole pipeline, installs the outputs above, and verifies the `.ico` |
| `pack-ico.py` | Packs the ten frames into the `.ico`: the 256 entry as PNG, every other entry as 32-bit BMP with an AND mask |
| `parse-ico.py` | Checks the `.ico` structure: entry count, sizes, bit depth, offsets in bounds |

## Regenerating

```powershell
pwsh branding/icon/build-icon.ps1
```

Requirements: PowerShell 7, Python 3 with `numpy` and `opencv-python`, Inkscape 1.x, and ImageMagick 7, all on `PATH`.

The script renders every frame straight from its vector master (never by resizing another frame). It forces 8-bit RGBA and strips PNG date chunks, so an unchanged source rebuilds **byte for byte**. It ends by printing `git status` for the icon files, and an empty result means the rebuild reproduced the committed icon exactly. Intermediate frames go to `build/`, which is not committed.

## Design notes

- The mark is the logo's teal ring looping out from behind a rising ribbon arrow (blue → magenta → orange), with an app-window glyph in the ring, on a dark-teal rounded plate. Unlike the logo banner it has **no penguin**, so the icon stays platform-neutral.
- The colours are the logo's own. They were sampled by k-means from the original 2816×1536 logo image (ribbon, ring, window glass and backdrop regions) and are recorded as constants at the top of `build-svg.py`.
- The SVGs use **no filters**. The glow is a radial-gradient ring and the shadows are stacked translucent shapes, so Qt's SVG renderer before Qt 6.7, librsvg, browsers and Inkscape all draw it the same way.
- The `.ico` keeps the 256 frame PNG-compressed, the Windows Vista-and-later convention. Explorer, WIC and `rc.exe` read it; only legacy GDI+ (`System.Drawing.Icon`) falls back to the 128 frame, and StreamFlex does not use it.
