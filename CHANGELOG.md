# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

This project started from complexlogic's Flex Launcher at v2.2 and is developed independently. The original release history up to v2.2 is in [`CHANGELOG`](CHANGELOG), which is frozen.

## [Unreleased]

### Added
- `build-and-test` gate job in `build.yml`, the single required status check for `master`. It passes only when the Windows, Debian, and Raspberry Pi builds all succeed.
- OpenSSF Scorecard workflow (`scorecard.yml`).
- Dependabot version updates for GitHub Actions, with an auto-merge workflow for patch, minor, security, and grouped action bumps.
- `SECURITY.md`, `CONTRIBUTING.md`, `.github/CODEOWNERS`, and this changelog.
- `CONTRIBUTING.md` lists all three required status checks: `build-and-test`, `CodeQL`, and `Scorecard analysis`.
- Arch Linux package (`.pkg.tar.zst`), built in CI with makepkg from the existing PKGBUILD template.
- A single `Release` job that publishes all four packages to a GitHub release when a `v*` tag is pushed, with Sigstore build provenance attestations for each package. On every other run it performs a dry run of the release.
- Documentation site workflow (`pages.yml`) that builds `docs/` with Jekyll and deploys it to GitHub Pages; pull requests that touch `docs/` build it without deploying.

### Changed
- The repository is now a standalone project, detached from complexlogic/flex-launcher's fork network. `CONTRIBUTING.md`, `SECURITY.md`, and this changelog no longer describe an upstream to sync with or report to, and the original `CHANGELOG` is frozen.
- All workflow actions are pinned to commit SHAs and updated: `actions/checkout` v3 → v7.0.1, `actions/upload-artifact` v4 → v7.0.1, `friendlyanon/setup-vcpkg` v1 → v1.7.0, `softprops/action-gh-release` v1 → v3.0.3.
- Linux packages are built on Debian 12 (bookworm) instead of Debian 11 (bullseye), so the `.deb` files now need Debian 12, Ubuntu 22.04, or newer.
- The Raspberry Pi package is built in a Debian bookworm container on a native arm64 runner, replacing the QEMU chroot into a 2022 Raspberry Pi OS image.
- The workflow token is read-only by default; only the release job can write, and releases use the repository's own token instead of a separate `ACTIONS_SECRET`.
- Pull requests to `master` run the build even when they only touch Markdown, so the required check always reports.

### Fixed
- Windows CI build. The previous vcpkg pin fetched `getopt-win32` from a GitHub archive that now returns 404; vcpkg is now pinned to release 2026.07.29.
- Debian and Raspberry Pi CI builds. Debian 11 is past end of life and its security mirror returns 404 for package downloads, so both builds failed at dependency install.
