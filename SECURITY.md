# Security Policy

This repository is a fork of [complexlogic/flex-launcher](https://github.com/complexlogic/flex-launcher). This policy covers the fork, `bilbospocketses/flex-launcher`.

## Reporting a Vulnerability

**Please do not report security vulnerabilities through public GitHub issues.**

Report security issues privately through GitHub's built-in security advisory flow:

**[Report a vulnerability](https://github.com/bilbospocketses/flex-launcher/security/advisories/new)**

This opens a private channel between you and the maintainer — no public disclosure until a fix is ready.

## What to Include

When reporting, please provide:

- A clear description of the vulnerability and its impact
- Steps to reproduce (a proof-of-concept config file, input, or environment)
- The affected version / commit
- Any mitigations you're aware of

## Response Expectations

- **Acknowledgement:** within **72 hours** of receipt
- **Triage and initial assessment:** within one week
- **Fix and disclosure timeline:** discussed with the reporter on a per-issue basis, depending on severity and complexity

## Supported Versions

Security fixes target the latest commit on `master`. Older commits and tags are not maintained.

## Scope

In scope: the launcher itself — config file parsing, application launching, image and font loading, and the Windows and Linux platform layers under `src/platform/`.

Out of scope:
- Vulnerabilities in third-party libraries (SDL2, SDL2_image, SDL2_ttf, inih, getopt) that are not specific to how flex-launcher uses them — report those to the library's maintainers.
- Issues present in the upstream project and not introduced by this fork — please also report those to [complexlogic/flex-launcher](https://github.com/complexlogic/flex-launcher).
- Behaviour that follows from a user deliberately configuring the launcher to run a given command. The config file is trusted input written by the machine's owner.

Thanks for helping keep the project safe.
