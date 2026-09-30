# Per-service research template (StreamFlex D-pad overlay)

Every service gets one section with exactly these fields. Tag every factual claim VERIFIED (an official or primary source, or several independent recent sources), REPORTED (a single or forum source) or UNKNOWN, and cite a source URL. Today is 2026-09-28, and the region is the **United States**; note where other regions differ.

**Our browser targets** (user decision, revised 2026-09-28): like-for-like, three browsers on two OSes. Windows Chrome, Edge and Firefox; Linux Chrome, Edge and Firefox (the full non-snap build). macOS is out of scope.

## <Service name>

1. **Web player URL** and the sign-in URL.
2. **Support and quality per target.** For each of Win Chrome, Win Edge, Win Firefox and Linux Firefox, give:
   - supported: yes, no, or blocked (by user agent or OS);
   - max resolution;
   - HDR / Dolby Vision;
   - audio (stereo / 5.1 / Atmos);
   - the DRM system.

   Note any Linux block explicitly (Peacock is reported to block Linux).
3. **Account model:** profiles, and a profile picker on the web? A PIN or kids profile? Sign-in methods on the desktop web: password, email or text code, passkey, or "sign in with a phone / QR" (web or TV-only?).
4. **UI technology:** the SPA framework if known (React, Angular, a custom one), and whether it uses shadow DOM, iframes or canvas-drawn UI. How keyboard-accessible the site is (does Tab + Enter reach the rows and tiles?), and any published accessibility statement or VPAT.
5. **Player:**
   - the documented keyboard shortcuts;
   - skip intro and skip recap;
   - next-episode autoplay;
   - the subtitle and audio menus;
   - known seek traps (like Netflix M7375).
6. **Stability signal:**
   - how often the web UI has been redesigned (2024-2026);
   - existing browser extensions that target the site, and whether they break often. For example Teleparty, Language Reactor, Enhancer for YouTube, "skip intro" extensions, with their issue trackers;
   - any anti-extension or tamper detection.
7. **Terms of service:** clauses on automated access, extensions and user-agent spoofing. Quote them, with the ToS URL and its date.
8. **Market weight:** US subscribers or MAU (source and date), the price tiers, and whether there is a free or ad tier (and whether the ad tier works in a desktop browser).
9. **A 10-foot option:** any TV-style web UI reachable from a desktop browser (like YouTube's `/tv`).
10. **Verdict for StreamFlex:**
    - include in Tier 1 / Tier 2 / Tier 3, or exclude, with the reason;
    - the expected overlay difficulty (low / medium / high);
    - the single biggest risk.
