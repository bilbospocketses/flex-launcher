# Contributing to Flex Launcher

Flex Launcher is a customizable application launcher and desktop replacement for Windows and Linux, written in C with SDL2.

This repository is an independent project. It started from complexlogic's Flex Launcher at v2.2 and has been developed separately since; it does not track, sync with, or send changes back to that project. Changes land here through the process below.

## Building

Build instructions for Windows (Visual Studio + vcpkg) and Linux (CMake + distro packages) are in [`docs/compilation.md`](docs/compilation.md). The CI workflow in [`.github/workflows/build.yml`](.github/workflows/build.yml) is the authoritative, always-exercised recipe for all four targets: Windows, Debian, Raspberry Pi, and Arch Linux.

## Project Structure

```
src/                 Launcher core (launcher.c, image.c, clock.c, util.c, debug.c)
src/platform/        Windows and Linux platform layers
src/external/        Vendored third-party sources
config/              Default config template, packaging and platform templates (PKGBUILD, .desktop, manifest, icon)
assets/              Icons and fonts
docs/                Documentation site (GitHub Pages / Jekyll)
```

## Branch Strategy

`master` is **PR-gated**. Direct pushes are blocked by a branch ruleset; every change goes branch → PR → required checks green → squash-merge.

**Required status checks** (all must be green before merge, and the branch must be up to date with `master`):
- `build-and-test` — the gate job in `build.yml`; passes only when the Windows, Debian, Raspberry Pi, and Arch Linux builds all succeed.
- `CodeQL` — code scanning via CodeQL default setup (C/C++ and GitHub Actions). It is required as the single `CodeQL` result rather than the per-language `Analyze (...)` jobs, so PRs where those jobs don't run are not blocked forever.
- `Scorecard analysis` — OpenSSF supply-chain scoring from `scorecard.yml`.

**Merge method:** squash only. Rebase merges are disallowed because they skip GitHub's signature on the merged commit.

**Signed commits required.** Commits to `master` and `v*` tags must be signed.

**Workflow file edits:** every action in `.github/workflows/*.yml` must be pinned to a full commit SHA (not an annotated-tag object SHA) with a precise version comment such as `# v7.0.1`, never a bare `# v7`. The repository enforces SHA pinning and only allows an explicit list of third-party actions; a new third-party action has to be added to that allowlist before its workflow can run.

## Changelog

Record changes in `CHANGELOG.md`. The older `CHANGELOG` file holds the original project's release history up to v2.2 and is frozen; don't add to it.

## Releasing

Versions follow [Semantic Versioning](https://semver.org/) with three parts (`0.1.0`). The project stays below 1.0 until the major overhaul is in place.

1. In one PR, set `VERSION` in `CMakeLists.txt` and `launcher_version` in `docs/_config.yml` to the new version, and rename `## [Unreleased]` in `CHANGELOG.md` to `## [x.y.z] - YYYY-MM-DD` with a fresh empty `## [Unreleased]` above it. The `Release` job's dry run on that PR checks that every package carries the new version, that `launcher_version` matches it, and that the notes extract.
2. After it merges, tag the merge commit with a signed annotated tag and push it:
   ```bash
   git tag -s vx.y.z -m "vx.y.z"
   git push origin vx.y.z
   ```
3. The `Release` job builds all four packages, attests them, and publishes the GitHub release with the CHANGELOG section as its notes.

`v*` tags can't be deleted or moved, so a failed release can't be retried under the same number: fix the problem and release the next patch version.

## Commit Messages

Use conventional-commit-style prefixes: `feat:`, `fix:`, `refactor:`, `docs:`, `style:`, `chore:`, `build:`, `ci:`, `test:`. Keep the subject short and imperative, wrap the body at 72 columns, and reference issue numbers when applicable.

## Pull Requests

- Keep PRs focused on one concern.
- Update `CHANGELOG.md` under `[Unreleased]` for any user-visible change.
- Update the relevant page in `docs/` when user-facing behaviour or configuration changes.

## Reporting Bugs

Open an issue with:

- Expected vs actual behaviour
- OS and version (Windows 10/11, Linux distro, Raspberry Pi OS)
- Your `config.ini` (or the relevant section of it)
- Output from running with debug logging enabled (see the README's Debugging section)

## Reporting Security Issues

Do **not** file a public issue. See [`SECURITY.md`](SECURITY.md) for the private reporting flow.

## License

The project is released under the [GNU General Public License v3.0](LICENSE). By contributing you agree to license your contributions under the same terms.
