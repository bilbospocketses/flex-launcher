# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project uses [Semantic Versioning](https://semver.org/).

This project started from complexlogic's Flex Launcher at v2.2 and is developed independently. The original release history up to v2.2 is in [`CHANGELOG`](CHANGELOG), which is frozen.

## [Unreleased]

### Changed
- CI runs on `ubuntu-24.04` instead of `ubuntu-latest`, which GitHub moves to Ubuntu 26 from October 19, 2026. Every job now names its runner image (Windows was already `windows-2022` and Raspberry Pi `ubuntu-24.04-arm`), so a new image arrives in its own deliberate change rather than under a build that had not changed.

### Fixed
- The docs caught up with the icon library and the font fix. The configuration guide lists SVG among the supported image formats, says a relative `Font` path is also looked for next to the executable and that library icons don't depend on the working directory, and names the entry's middle field `icon` throughout. The README, the docs home page and the default config's comments point to the library; `CONTRIBUTING.md` counts the Icon library check in the required gate and points to the library's own tests; `SECURITY.md` includes the library manifest in scope. The setup guide's contents also named a section "Maintaining Controls" instead of "Maintaining Contrast".
- **If the scroll arrows cannot be drawn, StreamFlex turns them off and carries on.** Before, it freed their memory twice, once on turning them off and again at exit, which could crash on quit or corrupt memory.
- **A config without `HPadding` or `VPadding` gets the documented 30 px.** Before, an unset padding was -1: the highlight hugged the button with no padding, and an outline was drawn with a width of -1. The shipped config sets both, so it looks the same.
- **Back works after an entry opens the menu it is already in.** Before, that menu became its own Back target, so Back stayed where it was and the way back was lost until a restart.
- **`MaxButtons` and `IconSize` refuse junk, as `Rows` and `Columns` already did, and the log says so.** `MaxButtons=7x` used to be read as 7 and `IconSize=200px` as 200; both are now ignored. `IconSize` takes a whole number from 32 to 1024, in `[Layout]` or in a menu.
- **Huge `Rows`, `Columns`, `IconSpacing` or `VPadding` values no longer overflow the layout arithmetic.** `IconSpacing` is capped at the screen width, with a log line, and no more rows or columns are tried than 32 px buttons could fill. Before, `Columns=999999` with a screen-wide gap overflowed an `int` and laid out a nonsense grid.
- The debug log shows every menu's grid and button size, including menus that are never opened. Before, a menu's layout appeared only once it was loaded.
- A menu whose grid is reduced to fit the screen is reported in the log once, not every time it is opened.

## [0.2.0] - 2026-09-27

The first part of the overhaul. Menus can be grids of several rows, buttons grow to fill the screen, and an entry can name an icon from a built-in library instead of giving a file path. A config written for 0.1.3 still works. One visible difference: a menu that doesn't set `IconSize` now gets buttons that grow to fill the screen, where it used to get 256 px.

### Added
- **A built-in icon library.** The app icons of 33 streaming and media services, and 36 generic icons for system actions, kinds of media, general use and devices, all on one rounded-square outline. An entry names one instead of a path: `Entry1=Netflix;netflix;...`. The docs site has a gallery of every icon and its name.
- **Menus can show several rows of buttons.** `Rows` sets how many rows are visible at once. With two or more, a menu is a grid: Left and Right stop at the end of a row, Up and Down move between rows, and the grid scrolls one row at a time. A single row still scrolls sideways.
- **Buttons are sized to fill the grid.** Choose its shape with `Rows` and `Columns`, and the buttons scale to fit the screen, titles included.
- **Each menu can have its own layout.** `Rows`, `Columns` and `IconSize` in a menu's section override the `[Layout]` settings for that menu.
- The `:up` and `:down` special commands. The Up and Down arrow keys also move between rows; a hotkey already bound to Up or Down keeps working.
- Gamepads get Up and Down by default. When the config maps nothing to `:up` or `:down`, the D-pad and the left stick's vertical axis run them, unless the config uses those controls for something else.
- Unit tests for the layout logic, run by CTest in CI.

### Changed
- **The default config uses library icons.** The seven Numix icons are gone. A config that still points at one of their old files gets the matching library icon, and the log suggests its name.
- **`MaxButtons` is now `Columns`.** The old name still works; if both are set, `Columns` wins.
- **`IconSize` is now the largest a button may grow**, not a fixed size. Menus that set it look the same as before. A config with no `IconSize` line now gets buttons that grow to fill the screen, where it used to get 256 px.
- **A single row with more buttons than fit now slides one button at a time**, instead of flipping to the next page.
- **If `IconSpacing` is too large for a row to fit at `IconSize`, the buttons now shrink to fit.** Before, the spacing was reduced instead. The spacing is kept as set, and the button follows the grid.
- **With the clock on, a menu is kept below it.** A row set high with `VCenter` (for example `25%` with the time and date shown) moves down just far enough to clear the clock, where before it could overlap it.
- SVG icons are drawn at the button's size, so they stay sharp at any size.

### Fixed
- On Windows, the default config's icons no longer depend on the folder StreamFlex was started from.
- A font named by a relative path (the Windows default config uses `.\assets\fonts\...`) is now found next to the executable when StreamFlex is started from another folder, such as a shortcut's "Start in" folder. Before, it logged "Could not initialize font from config file" and fell back to the default font.
- The README, the setup guide and the `OversizeMode` and Scroll Indicators docs still described the single-row layout, and the setup guide said SVG menu icons were not supported. They now describe grids, and recommend SVG icons.
- Titles on very small buttons no longer write outside their memory. When fewer than three characters of a title fitted, truncating it walked back past the start of the text, which could crash the launcher or corrupt memory. It becomes `...` instead, and a one- or two-character title is left as it is. Small buttons were rare before grids; a dense grid on a small screen makes them ordinary.

## [0.1.3] - 2026-09-27

On Linux the app icon now comes in the small sizes that menus and panels use. Everything else here is about the icon's source and tooling; the launcher behaves exactly as in 0.1.2.

### Added
- The app icon's source is in the repository under `branding/icon/`. `build-svg.py` holds its geometry and colours and writes the two vector masters, and `build-icon.ps1` renders, packs, installs and verifies all four icon files. Run on the committed source, it reproduces the 0.1.2 icon byte for byte, so a later icon change can be regenerated rather than redrawn.
- Three icon tools sit beside it. `review-sheet.py` lays every icon size out on light and dark backgrounds, with pixel zooms of the small sizes, for judging a design by eye. `render-check.py` renders the icon in Chromium and Inkscape and measures the difference, a portability check for design changes. `palette.py` re-derives the icon's colours from the logo banner.
- On Windows, `build-icon.ps1` also checks that the icon reads correctly through WIC (Explorer's decoder) and GDI+, and that `rc.exe` compiles it.
- The full-resolution logo original (2816×1536) is in the repository as `branding/logo/streamflex-logo.jpg`. The README and docs banner is a downscale of it.
- The Windows build instructions in the compilation guide now reproduce the CI build: Visual Studio 2022, vcpkg pinned to the version CI uses, and CI's generator and library settings. The `build` directory is ignored by git.

### Fixed
- **Linux: the app icon now comes in 16, 24 and 32 px sizes**, drawn from the icon's simplified small-size design. Until now only the 48 px and scalable icons were installed, so menus, panels and taskbars at small sizes shrank the detailed artwork, and the design made for small sizes only reached Windows.

## [0.1.2] - 2026-09-27

StreamFlex's own app icon. The launcher behaves exactly as in 0.1.1.

### Changed
- **New app icon**, drawn from the StreamFlex logo. It shows the logo's teal ring and rising ribbon arrow around an app-window glyph, on a dark-teal rounded plate. The icon is platform-neutral, so unlike the banner it has no penguin. It replaces the icon inherited from the original project everywhere:
  - The Windows executable (`config/streamflex.ico`): ten sizes from 16 to 256 px, all with alpha. The small sizes use a simplified, bolder drawing so they stay legible.
  - The Linux desktop entry: `streamflex.svg` for the scalable icon and a 48×48 `streamflex.png`, installed into the hicolor theme.
  - The documentation site's favicon.

## [0.1.1] - 2026-09-27

First release under the name StreamFlex. The launcher behaves exactly as in 0.1.0; what changes is its name, and with it the executable, package and directory names listed below. The new app icon follows in 0.1.2.

### Changed
- **The project is renamed from Flex Launcher to StreamFlex**, with a new logo. Everything named after the project follows, so an existing install does not carry over:
  - The executable is `streamflex` (`streamflex.exe` on Windows), and the packages are `streamflex_<version>_amd64.deb`, `streamflex_<version>_arm64.deb`, `streamflex-<version>-1-x86_64.pkg.tar.zst` and `streamflex-<version>-win64.zip`. The Debian/Arch package name is `streamflex`; the `flex-launcher` package is not replaced or removed by it.
  - On Linux the user config directory is now `~/.config/streamflex/` (was `~/.config/flex-launcher/`), the debug log is written to `~/.local/share/streamflex/`, and the packaged defaults install to `/usr/share/streamflex/`. Move an existing config with `mv ~/.config/flex-launcher ~/.config/streamflex` and replace `flex-launcher` with `streamflex` in any paths inside it.
  - The repository is `bilbospocketses/streamflex` and the documentation site is https://bilbospocketses.github.io/streamflex/.
  - The Debian packages carry no epoch (`0.1.1`, not `1:0.1.1`). The epoch on v0.1.0 existed to sort the `flex-launcher` package above the original project's 2.2; the new `streamflex` package name has no earlier versions to outrank.
- The Debian and Raspberry Pi packages list each dependency once. The `Depends` field combined a hand-written list with the one `dpkg-shlibdeps` computes from the binary, so every library appeared twice; it is now the computed list alone.
- CI builds a pull request once per commit instead of twice. The build workflow's `push` trigger now fires only for `master` and `v*` tags, leaving PR branches to the `pull_request` trigger.
- The docs site's download page takes its version and file links from the latest published release, read at build time, and the `Release` job rebuilds the site once a release is published. Previously the version was bumped by hand in the release PR and deployed when that PR merged, so the download links pointed at a release that did not exist yet until the tag was pushed and published. `launcher_version` in `docs/_config.yml` is gone.

### Fixed
- The documentation site no longer reports visits to the original author's Google Analytics property. A `docs/_includes/head-custom-google-analytics.html` override inherited from the original project hardcoded its measurement ID; it is removed, so the theme's default include applies and stays inactive unless `google_analytics` is set in `docs/_config.yml`.
- `CONTRIBUTING.md` described three build targets where there are four (Arch Linux was missing from the build-and-test gate description) and misdescribed the `config/` and `assets/` directories.
- The compilation guide's dependency list now includes inih, and getopt on Windows.

## [0.1.0] - 2026-09-26

First release of the independent project. Versioning restarts at 0.1.0; the project stays below 1.0 until its major overhaul is in place. There are no changes to the launcher's behaviour or configuration compared with the original v2.2 — this release is about licensing, packaging, and the build.

### Added
- Arch Linux package (`.pkg.tar.zst`), built with makepkg from the existing PKGBUILD template.
- Every release package now ships with Sigstore build provenance attestations, so a download can be verified with `gh attestation verify <file> -R bilbospocketses/flex-launcher`.
- The packages include the licence text: `LICENSE.txt` in the Windows zip, and `/usr/share/licenses/flex-launcher/LICENSE` in the Linux packages.
- Documentation site at https://bilbospocketses.github.io/flex-launcher/, built from `docs/` and deployed by the `pages.yml` workflow.
- `SECURITY.md` with a private vulnerability reporting flow, `CONTRIBUTING.md` including a release procedure, and `.github/CODEOWNERS`.
- CI: a `build-and-test` gate (the required status check for `master`) that passes only when the Windows, Debian, Raspberry Pi, and Arch builds all succeed; a single `Release` job that dry-runs on every pull request; OpenSSF Scorecard; CodeQL; Dependabot for GitHub Actions with auto-merge.

### Changed
- **Licence: GNU General Public License v3.0**, replacing the Unlicense. The original project's code was public domain, which permits relicensing; the original author is credited in the README.
- **Linux packages now need Debian 12 / Ubuntu 22.04 / Raspberry Pi OS 12 (Bookworm) or newer.** They are built on Debian 12 instead of Debian 11, which is past end of life.
- The Debian packages' dependencies, including the minimum glibc, are now computed from the built binary by `dpkg-shlibdeps`. Previously they declared `libc6 (>= 2.31)` while the binary needed glibc 2.34, so the package would install on systems where it could not run.
- The Debian packages carry epoch 1 (`1:0.1.0`), so apt treats them as newer than the original project's 2.2 packages despite the lower version number.
- Package metadata points at this project: homepage `https://github.com/bilbospocketses/flex-launcher`, maintainer `bilbospocketses`. The README, docs site, and the default `config.ini` link to this project's releases and documentation.
- The Windows zip ships this changelog as `CHANGELOG.txt` instead of the original project's frozen one.
- The repository is a standalone project, detached from complexlogic/flex-launcher's fork network.
- The Raspberry Pi package is built in a Debian Bookworm container on a native arm64 runner, replacing the QEMU chroot into a 2022 Raspberry Pi OS image.
- Build workflows: every action pinned to a commit SHA and updated to its current release, read-only token by default, and releases published with the repository's own token.

### Fixed
- Windows build. The previous vcpkg pin fetched `getopt-win32` from a GitHub archive that now returns 404; vcpkg is now pinned to release 2026.07.29.
- Debian and Raspberry Pi builds, which failed at dependency install because the Debian 11 security mirror now returns 404.
