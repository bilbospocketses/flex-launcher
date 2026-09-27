# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project uses [Semantic Versioning](https://semver.org/).

This project started from complexlogic's Flex Launcher at v2.2 and is developed independently. The original release history up to v2.2 is in [`CHANGELOG`](CHANGELOG), which is frozen.

## [Unreleased]

### Added
- The app icon's source is in the repository under `branding/icon/`. `build-svg.py` holds its geometry and colours and writes the two vector masters, and `build-icon.ps1` renders, packs, installs and verifies all four icon files. Run on the committed source, it reproduces the 0.1.2 icon byte for byte, so a later icon change can be regenerated rather than redrawn.

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
