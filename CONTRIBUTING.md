# Contributing to StreamFlex

StreamFlex is a customizable application launcher and desktop replacement for Windows and Linux, written in C with SDL2.

This repository is an independent project. It started from complexlogic's Flex Launcher at v2.2 and has been developed separately since; it does not track, sync with, or send changes back to that project. Changes land here through the process below.

## Building

Build instructions for Windows (Visual Studio + vcpkg) and Linux (CMake + distro packages) are in [`docs/compilation.md`](docs/compilation.md). The CI workflow in [`.github/workflows/build.yml`](.github/workflows/build.yml) is the authoritative, always-exercised recipe for all four targets: Windows, Debian, Raspberry Pi, and Arch Linux. After building, run the unit tests with `ctest --test-dir build -C Release --output-on-failure`. The icon library's tooling has Python tests of its own, and its README says how to run them and `check-library.py`; see [`branding/library/README.md`](branding/library/README.md).

The unit tests reach failures no real run gives through two seams, both for tests only. The pure modules (fileio, inidoc, config_save, settings, browser) allocate through `alloc.h`, and `alloc_set_hooks()` puts a test's allocator in its place, which `test_alloc` uses to make every allocation fail in turn. `fileio_set_fault()` makes one fileio step fail: `FILEIO_FAULT_LIST_READ` fails a listing's read after a given number of entries with a given error, `FILEIO_FAULT_KEEP` makes a replace unable to keep the old file's permissions or attributes, and `FILEIO_FAULT_NO_KIND` (Linux) makes a listing give no entry's kind, as some file systems give none. On Linux, `fileio_set_mount_table()` points fileio at a pretend `/proc/self/mounts`.

The headless checks run in Docker, as the `Headless (Debian)` and `Headless (Fedora)` jobs in `build.yml` do: build an image from `tests/headless`, then run `run.sh` in it with the repository mounted read-only at `/src` and a folder for the results at `/out`, with the seccomp profile off (the harness turns address randomization off with `setarch -R`, which ASan needs and Docker's default profile refuses):

    docker build -t streamflex-test tests/headless
    docker run --rm --security-opt seccomp=unconfined -v "$PWD:/src:ro" -v "$PWD/headless-out:/out" streamflex-test bash /src/tests/headless/run.sh local

There are two images and one `run.sh`. `Dockerfile` is Debian with SDL2 itself; `Dockerfile.fedora` is Fedora 44, whose SDL2 is sdl2-compat over SDL3 and whose Mesa has llvmpipe but no softpipe. What differs between them is detected, never branched on by name, and `run.sh` prints the Mesa driver it found (`Mesa: <driver>, from <path>`). For the Fedora pass, build its image and give its tag to the same `docker run`:

    docker build -t streamflex-test-fedora -f tests/headless/Dockerfile.fedora tests/headless

A second argument to `run.sh` picks the mode:

- `run.sh <label>` runs every check in `checks/`, in name order, starting with `00-harness.sh`, which checks the harness's own helpers. The launcher is built with ASan and UBSan, and a compiler warning outside `src/external/` fails the run. Some checks also read the screen: `xwd` takes a screenshot and `pixels.py` reads chosen points, such as the settings preview's colour, image and Transparent checkerboard.
- `run.sh <label> scrollfail` runs item 11 only, with the scroll arrow's texture forced to fail, a path no config or input can reach.
- `run.sh <label> leaks` runs every check again with LeakSanitizer on. Mesa's software driver is preloaded into the launcher so the blocks it holds at exit stay reachable, and there is no suppressions file: a run that leaks fails its check, and each leak is also listed at the end as a `LEAK` line that counts in `N failed`.

The harness builds with `-DSTREAMFLEX_TEST_HOOKS`, which nothing else defines. It adds test-only hooks, environment variables a check sets to force what no config or input can reach; normal builds contain none of them:

- `STREAMFLEX_TEST_DECODE_DELAY_MS=<ms>`: the settings preview waits that long before decoding an image, so a check can press a key during the decode.
- `STREAMFLEX_TEST_PAD=<file>`: attaches a virtual gamepad, since Xvfb has none, and holds its Start button while `<file>` exists. It does nothing when the gamepad is turned off.
- `STREAMFLEX_TEST_NO_RENDER_TARGETS`: settings draw as they would on a renderer without render targets.
- `STREAMFLEX_TEST_FAIL_TITLE_SIZE=<pt>`: the title font fails to open at that size.
- `STREAMFLEX_TEST_FAIL_SHRINK_STEP`: every step down in Shrink mode fails to open its font.
- `STREAMFLEX_TEST_NO_MESSAGE_BOX`: a fatal error quits without its message box, which some SDLs show and wait on.
- `STREAMFLEX_TEST_FAIL=places|browser|command|keep`: that step of the settings screen (finding the browser's places, opening the browser, a browser command) runs as if memory had run out, or for `keep`, as if the saved file's permissions could not be kept.

The hook build also logs how many paragraphs the settings screen measured while it was open, so a check can catch a note measured again in every frame. The helpers checks share (`run_keys`, `ran_clean`, `in_range`, `precedes`, `look`, `stop_run` and others) live in `run.sh`.

Each run prints one `PASS` or `FAIL` line per check, then `N failed`, and keeps each check's output and log, and the launcher's exit code, under `headless-out/<label>/` (which the harness leaves out of the source it builds). Locally the default pass takes about 4.8 minutes, the leak pass about 5.5 and scrollfail about 10 seconds. On GitHub each Headless job, which runs all three, takes about 10 minutes.

## Project Structure

```
src/                 Launcher core (launcher.c, layout.c, library.c, image.c, clock.c, util.c, utf8.c, debug.c) and the settings screen (settings_screen.c, settings.c, browser.c, inidoc.c, config_save.c, fileio.c)
                     test_hooks.c: the harness's STREAMFLEX_TEST_FAIL hook, built into every build but compiled to no code unless STREAMFLEX_TEST_HOOKS is defined; the other hooks sit inline in image.c, launcher.c and settings_screen.c
src/platform/        Windows and Linux platform layers
src/external/        Vendored third-party sources
config/              Default config template, packaging and platform templates (PKGBUILD, .desktop, manifest, icon)
assets/              Icons and fonts
branding/icon/       Source for the app icon; regenerate with build-icon.ps1 (see its README)
branding/logo/       The full-resolution logo original; the docs banner is a downscale of it
branding/library/    Tools for the icon library: generator, brand importer, checks, gallery (see its README)
design/              Design specs and implementation plans (not published; docs/ is the site)
tests/               Unit tests (CTest); run them with ctest after building
tests/headless/      Headless checks: the launcher under Xvfb, driven by key presses (see run.sh); CI runs them on Debian and Fedora
docs/                Documentation site (GitHub Pages / Jekyll)
```

## Branch Strategy

`master` is **PR-gated**. Direct pushes are blocked by a branch ruleset; every change goes branch → PR → required checks green → squash-merge.

**Required status checks** (all must be green before merge, and the branch must be up to date with `master`):
- `build-and-test` — the gate job in `build.yml`; passes only when the Windows, Debian, Raspberry Pi, and Arch Linux builds, the `Icon library` check and the `Headless (Debian)` and `Headless (Fedora)` checks (two legs of the `headless` job) all succeed.
- `CodeQL` — code scanning via CodeQL default setup (C/C++ and GitHub Actions). It is required as the single `CodeQL` result rather than the per-language `Analyze (...)` jobs, so PRs where those jobs don't run are not blocked forever.
- `Scorecard analysis` — OpenSSF supply-chain scoring from `scorecard.yml`.

**Merge method:** squash only. Rebase merges are disallowed because they skip GitHub's signature on the merged commit.

**Signed commits required.** Commits to `master` and `v*` tags must be signed.

**Workflow file edits:** every action in `.github/workflows/*.yml` must be pinned to a full commit SHA (not an annotated-tag object SHA) with a precise version comment such as `# v7.0.1`, never a bare `# v7`. The repository enforces SHA pinning and only allows an explicit list of third-party actions; a new third-party action has to be added to that allowlist before its workflow can run.

## Changelog

Record changes in `CHANGELOG.md`. The older `CHANGELOG` file holds the original project's release history up to v2.2 and is frozen; don't add to it.

## Releasing

Versions follow [Semantic Versioning](https://semver.org/) with three parts (`0.1.0`). The project stays below 1.0 until the major overhaul is in place.

1. In one PR, set `VERSION` in `CMakeLists.txt` to the new version, and rename `## [Unreleased]` in `CHANGELOG.md` to `## [x.y.z] - YYYY-MM-DD` with a fresh empty `## [Unreleased]` above it. The `Release` job's dry run on that PR checks that every package carries the new version and that the notes extract.
2. After it merges, tag the merge commit with a signed annotated tag and push it:
   ```bash
   git tag -s vx.y.z -m "vx.y.z"
   git push origin vx.y.z
   ```
3. The `Release` job builds all four packages, attests them, and publishes the GitHub release with the CHANGELOG section as its notes. It then dispatches the `Docs site` workflow, which rebuilds the download page from the new release. The docs site never carries a version by hand, so there is nothing to bump there.

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
