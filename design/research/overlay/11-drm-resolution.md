# R1: DRM and the video-quality ceiling (verified pass)

Researched 2026-09-28 via WebSearch/WebFetch only. Nothing was installed or run locally.

**Evidence tags**
- VERIFIED = a primary or official page was fetched, or several independent recent sources agree.
- REPORTED = a single source, a forum, or a secondary article that cites no primary.
- UNKNOWN = nothing found, or the sources conflict.

Many primary help pages (Disney+, Hulu, Paramount+, Crunchyroll, HBO Max browser page) are JavaScript-rendered and would not fetch. Where I could only get a search snippet of an official page, I say so.

---

## 0. Headline changes versus the original doc

1. **Chrome on Windows can now reach 4K on Netflix.** The mechanism is Microsoft **PlayReady SL3000 through Media Foundation**, not Widevine L1. Chrome's Widevine is still L3. The original doc said Chrome is stuck at Widevine L3 and 720p/1080p, which is out of date for Netflix.
2. **Edge is no longer the only 4K browser on Windows for Netflix.** It is still the only one that gets HDR, Dolby Vision and Atmos there (per Netflix).
3. **Linux is 1080p per Netflix's own table, not 720p.** In practice reports still say 720p by default, and Prime Video is SD on Linux, confirmed by Amazon's own page.
4. **WebView2 does support PlayReady**, including the hardware key system, though with bugs. The original doc said "likely doesn't". The Netflix Windows app is reported to be a WebView2 PWA.
5. **Most services other than Netflix are capped at 1080p or lower in any desktop browser**, so Edge's advantage is mostly a Netflix-only story.

---

## 1. Per-service x per-browser table

Values are the maximum available in a desktop browser or Store app. "?" means no reliable source found.

| Service | Windows Edge | Windows Chrome | Windows Firefox | Linux (Chrome/Chromium/Firefox) | Windows Store app | Tag |
|---|---|---|---|---|---|---|
| **Netflix** | 4K; HDR, Dolby Vision (needs certified display), Atmos | 4K only if PlayReady SL3000 path works (Chrome 140+ on x64 Win11); **no HDR, no Dolby Vision, no Atmos** | 1080p | Netflix table says 1080p; users report 720p by default (1080p only via extension) | Reported to be an Edge-WebView2 PWA; 4K + HDR + Atmos | Table and Chrome 4K: VERIFIED (Netflix help). Chrome no-HDR: REPORTED by 3 secondary outlets citing Netflix. Linux 720p: REPORTED. |
| **Disney+** | 1080p | 1080p (some reports say 720p) | 1080p or lower | Widevine L3 only; expect 720p or lower (?) | Store app reported as 4K/HDR/Atmos (a low-quality source; may be a PWA wrapper) | Browser "4K not available on computer browsers": VERIFIED (Disney help snippet). App claim: REPORTED. Linux: UNKNOWN. |
| **Prime Video** | 1080p max | 1080p max | 1080p max | **SD only** ("any OS other than Windows or macOS is restricted to SD") | Store app 1080p, stereo audio | VERIFIED (Amazon help, fetched) |
| **Max (HBO Max)** | 1080p (?) | 1080p (?) | 1080p (?) | UNKNOWN | ? | HBO Max's official 4K/HDR/Atmos device list has **no computers or browsers**: VERIFIED. A minitool page claiming Edge/Firefox get 4K is REPORTED and contradicted by the official list. |
| **Hulu** | 720p | 720p | 480p (HDCP cannot be verified) | UNKNOWN (likely lower) | ? | REPORTED: a search snippet of Hulu's help page. The fetch failed. Hulu is being merged into the Disney+ app in 2026, so this may change. |
| **Peacock** | Not specified | Not specified | Not specified | UNKNOWN | ? | 4K only on "some compatible devices", browsers not addressed: VERIFIED (Peacock help). Desktop max UNKNOWN. |
| **Paramount+** | Not specified; the Premium/Showtime plan has 4K on devices | same | same | UNKNOWN | ? | Browser max UNKNOWN (help page failed to load). |
| **Apple TV+** | 1080p | 1080p | 1080p | Same as Firefox/Chrome on Linux; officially unlisted | none | VERIFIED (Apple Support): Chrome/Firefox/Edge on Mac, Windows or Android give up to 1080p. Only Safari on Mac gets 4K HDR. |
| **YouTube** | 4K VP9/AV1 | 4K | 4K (VP9 on Linux too) | 4K, no DRM involved | n/a | REPORTED (Mozilla help plus general knowledge). 5.1 and Atmos playback are not offered in browsers: REPORTED. |
| **Crunchyroll** | 1080p | 1080p | 1080p | 1080p (Widevine L3 is enough) | n/a | REPORTED. Crunchyroll has no content above 1080p. |

### Audio, in one place
- Netflix Atmos on Windows: **Edge or the Netflix app only**, on Atmos-capable computers (Netflix help, VERIFIED). Chrome does not get it (REPORTED).
- Disney+ Atmos and 4K need a device. "Not available on computer browsers": VERIFIED.
- Prime Video app and web are stereo or 5.1 at most (Windows app: stereo per Amazon).
- 5.1 in a browser generally means Dolby Digital Plus, which needs Edge on Windows. Treat 5.1 as UNKNOWN per service without a hardware test.

---

## 2. Answers to the six questions

### Q1. Windows Chrome hardware-secure DRM: yes, but PlayReady, not Widevine

- **Widevine on Windows Chrome is still L3.** Not VERIFIED as a Google announcement, but several sources agree, including the DoveRunner and forasoft technical write-ups. One article (unstore.io) and a dev.to post say "Chrome shipped Widevine L1 on Windows 11". The dev.to post is from January 2025, before the change, and unstore.io is not credible on this. **Treat any "Chrome has Widevine L1 on Windows" claim as wrong.** Netflix's 4K path uses PlayReady.
- **Chrome now supports hardware-secure PlayReady through Media Foundation.** VERIFIED (DoveRunner docs; multiple independent outlets).
  - Key system: `com.microsoft.playready.recommendation.3000` (hardware SL3000). Robustness strings are left empty.
  - Requires **Chrome 140.0.7339.0 or later**, Windows 11 21H2 (build 22000) or later, **x64 only**. Not x86, not native ARM64, not x64-emulated-on-ARM64.
  - Flag: `chrome://flags/#enable-hardware-secure-decryption` (the HardwareSecureDecryption feature). Sources conflict on whether it is on by default. One says default-on from M140, and DoveRunner tells you to enable the flag. **Treat as UNKNOWN. Test on a clean Chrome profile.**
  - Chromium tracker: issue 378869813 "Support hardware secure PlayReady playback in Chromium". It needs a login and would not fetch.
  - Windows Report says Chrome M138 Beta introduced it. That is REPORTED.
- **Netflix gives Chrome 4K because of it.** VERIFIED: Netflix help node 30081 lists Chrome 117+ on Windows as "Ultra HD (2160p)*", and node 23931 lists Chrome as up to 4K. Coverage dates July 28 to August 6, 2026.
  - "Chrome 117" is Netflix's floor for the browser. The real requirement is the PlayReady-capable Chrome (140+).
  - Chrome 4K is **without HDR, Dolby Vision or Atmos** (ecoustics, windowsforum, Neowin: REPORTED, consistent with Netflix's note that Atmos is Edge or app only).
- **Other services on Chrome via this path:** REPORTED only. windowsreport and a forum say Disney+ and Prime "need PlayReady for 4K on Chrome". But Disney's and Amazon's own help pages still say browsers max out at 1080p and no 4K. **No service besides Netflix is confirmed to give Chrome above 1080p.**

### Q2. Edge PlayReady SL3000: exactly what 4K needs

From Netflix's own Windows page (help node 23931, VERIFIED):
- Windows **11** with the latest updates (Windows 10 is the minimum for anything, but 4K and HDR need 11).
- GPU: **NVIDIA GeForce GTX 1050 or newer, AMD Radeon RX 400 or newer, Intel 7th-gen (Kaby Lake) or newer, or an AMD Ryzen APU.**
- **4K display at 60 Hz with HDCP 2.2** end to end (HDMI/DP cable, GPU port, display).
- **Every active display must meet the requirements** (Netflix: "Every active display must also meet these requirements"). A second non-HDCP-2.2 monitor drops you to 1080p: VERIFIED by Netflix's wording plus a forum. See Q6.
- 15 Mbps or more, a Premium plan, and playback quality set to Auto or High.
- **HEVC Video Extension** on some devices (Netflix: "Some devices require purchasing the HEVC video extension"). Newer GPUs with AV1 hardware decode may not need it (REPORTED).
- Hardware acceleration on in Edge (REPORTED).
- **AMD RX 6000/7000 GPUs were blocked by a Chromium hardware-ID blocklist.** Fixed in Edge 137.0.3296.93 or later plus Windows KB5060999 (June 2025) plus Netflix app 6.0050.500.911. Workaround flag: `--disable-gpu-driver-bug-workarounds`. REPORTED (a Microsoft Q&A answer).
- Confirm with Ctrl+Shift+Alt+D during playback, or the Stats for Nerds view. Look for 2160p and "PlayReady SL3000". REPORTED.

**Firefox and PlayReady:** Firefox 132 (October 2024) added PlayReady support (gHacks, REPORTED). Netflix still lists Firefox 129+ on Windows at **1080p**. So Firefox has the CDM but not a 4K path on Netflix. UNKNOWN whether that has changed since July 2026.

### Q3. Launch modes

**Honest answer: I found no source that says `--kiosk`, `--app=` or a custom `--user-data-dir` change PlayReady or Widevine resolution, and no source that says they do not.** This is the largest gap and needs real-hardware testing.

What I did find:
- **`--remote-debugging-port`**
  - Chrome 136 and later ignore `--remote-debugging-port` and `--remote-debugging-pipe` on the default data dir. They must be paired with a non-default `--user-data-dir`. VERIFIED (developer.chrome.com). Chrome for Testing is exempt.
  - Group-policy blocking of `--remote-debugging-*` is reported on managed machines (Playwright issue 39637).
  - **No source found for a Netflix error caused by attached DevTools or CDP.** Searches for DRM black screens under Puppeteer/Playwright all pointed at a missing CDM, a stale Widevine component or a missing profile, not at CDP itself. UNKNOWN for Netflix and hardware DRM specifically.
- **Kiosk mode.** Microsoft documents Edge kiosk (Edge 87 and later) for digital and interactive signage. Nothing was found tying it to a DRM change. Some kiosk features are only enabled in an assigned-access single-app scenario. UNKNOWN for DRM.
- **`--app=URL`.** No DRM-specific finding. UNKNOWN.
- **Loaded extensions.** The netflix-force-4k extension works by spoofing capability checks in JavaScript and its author says it cannot get past hardware DRM (Jan 2025, REPORTED). That suggests an overlay content script does not itself block DRM, but no source tests this directly. **Do not spoof or modify DRM negotiation.**
- **Flags that can hurt:** the AMD workaround `--no-sandbox` reduces protection and should not be shipped. Any flag that disables GPU or hardware acceleration will remove the hardware DRM path.
- **VMP:** not applicable to stock Chrome or Edge, which are already signed. VMP matters for Electron and CEF (Q4).
- **PlayReady error seen on Edge:** `d7353-5102-6` on a Windows 11 Insider build in 2021 (old, REPORTED).

### Q4. Embedded web views

| Host | Widevine | PlayReady | Real ceiling | Tag |
|---|---|---|---|---|
| **WebView2** | Not supported (feature request open since Sept 2024, no Microsoft reply) | **Supported** (software). The hardware key system (`recommendation.3000`) works but has a black-screen regression on Windows 11 24H2, reported Nov 2024 with runtime 130 | Uncertain; the Netflix Windows app is reported to be a WebView2 PWA delivering 4K + HDR | Issues: VERIFIED (fetched). Netflix-app claim: REPORTED (streamfab, viwizard, Microsoft Q&A) |
| **WebView2 Fixed Runtime** | n/a | **PlayReady reported not working** (issue 4632, June 2024) | Treat as blocked; use the Evergreen runtime | REPORTED (single issue) |
| **Electron, castLabs ECS** | Widevine CDM with VMP signing; "full support for Windows and macOS, partial Linux". Dev builds are VMP-signed for testing only; production needs castLabs' EVS service | None | castLabs advertises L1/L3 validation for its customers, but a desktop Electron build gets the Widevine desktop ceiling, i.e. **L3**. UNKNOWN whether services will accept an ECS build at any tier | castLabs page: VERIFIED. Ceiling: UNKNOWN |
| **CEF** | No bundled Widevine; you must obtain and register the CDM yourself, and bundling needs a Google license | None built in | L3 at best, if you obtain a licensed CDM | REPORTED (CEF issue tracker) |

**Conclusion:** the original doc's advice to avoid embedding is still sound. The one exception is WebView2, which does support PlayReady, so an evergreen WebView2 shell could in principle inherit Edge's hardware DRM. That needs a hardware test because of the 24H2 regression.

### Q5. Linux

- **No path above L3 in 2026.** Multiple sources: L1 needs hardware vendor and driver support plus Google enabling it, and none exists on desktop Linux. VERIFIED by several independent sources (forasoft, XDA on Linux HDR, Asahi Netflix write-up).
- Per-service:
  - **Netflix:** official table says 1080p for Chrome, Firefox, Edge and Opera on Linux, with support "not guaranteed". Reports say the default is 720p, and 1080p needs the client to request it at the protocol level, which extensions do. Video is always software-decoded on Linux. **Do not rely on 1080p.**
  - **Prime Video:** SD only. VERIFIED (Amazon help).
  - **Disney+, Max, Hulu, Peacock, Paramount+:** no official Linux statement found. Expect Widevine L3 limits (720p or lower). UNKNOWN.
  - **Apple TV+:** officially only Chrome, Firefox and Edge on Mac, Windows and Android are listed. Linux UNKNOWN.
  - **Crunchyroll and YouTube:** fine (1080p, and 4K without DRM).
- **Edge on Linux:** Netflix lists 1080p, but this is the same Widevine L3.

### Q6. Displays

- **Multi-monitor:** every active display must be HDCP 2.2 capable for Netflix 4K. Netflix's own wording: "Every active display must also meet these requirements". A forum thread adds that one non-compliant monitor downgrades the whole session to 1080p. VERIFIED (Netflix) plus REPORTED (forum).
- **Duplicated second display:** a Microsoft Q&A thread reports a blank Netflix player on a duplicated second display, with a workaround of turning PlayReady off in Edge, which costs 720p. REPORTED.
- **Docks, adapters, cheap cables:** a chain that fails to negotiate HDCP 2.2 drops you to 1080p or lower. REPORTED (PCQuest, others).
- **Hulu** needs HDCP 1.4 even for 720p on Chrome and Edge. REPORTED.
- **Sleep, monitor change or display topology change:** keys are lost in the hardware secure path and playback does not resume by itself (DoveRunner, VERIFIED as a vendor note).
- **RDP, virtual displays, capture:** DRM surfaces are blanked from capture APIs. A headless host with no real output has nothing to show. An HDMI dummy plug helps only if it advertises HDCP; UNKNOWN whether cheap dummy plugs satisfy HDCP 2.2. Remote desktop apps cannot show DRM content (Screenify, REPORTED). **An HTPC needs a real HDCP 2.2 display attached and active.**
- **HDR with system HDR on:** Chromium issue 40892501 reports Dolby Vision breaking on Windows when system HDR is on. Not read in full. REPORTED.

---

## 3. Corrections to the original doc

| # | Original claim | Correction | Tag |
|---|---|---|---|
| 1 | Windows Chrome/Firefox: Widevine L3, 720p or sometimes 1080p | Chrome on Windows 11 can reach **4K on Netflix** via PlayReady SL3000 (Chrome 140+, x64). Firefox stays at 1080p. | VERIFIED |
| 2 | "Windows + Edge is the strongest target" because of PlayReady | Still true for **HDR, Dolby Vision and Atmos** on Netflix. For raw resolution, Chrome now matches Edge on Netflix. | VERIFIED |
| 3 | 1080p "often 4K" on Edge across Netflix, Disney+, Prime, Max | 4K is a **Netflix-only** outcome in a browser. Disney+, Prime, Apple TV+ are capped at 1080p; Hulu at 720p; Max has no browser 4K listed; Peacock/Paramount+ unknown. | VERIFIED for Disney+, Prime, Apple, Max. REPORTED for Hulu. |
| 4 | Linux: about 720p; Prime often SD | Netflix officially lists 1080p on Linux, though 720p is common in practice. **Prime Video is officially SD-only on Linux.** | VERIFIED (Netflix, Amazon) |
| 5 | WebView2 "likely doesn't support hardware PlayReady (confirm)" | WebView2 **does support PlayReady**, including the hardware key system, with a Windows 11 24H2 black-screen regression. Widevine is not supported. | VERIFIED (Microsoft issue tracker) |
| 6 | Electron: "only Widevine L3 through castLabs" | Roughly right, but castLabs needs VMP signing, production use needs the EVS service, and Linux support is partial. | VERIFIED |
| 7 | CEF has no Widevine by default | Correct. You must register a licensed CDM yourself. | REPORTED |
| 8 | 4K needs "modern Intel/AMD/NVIDIA GPU, HEVC extension, HDCP 2.2 display" | Add: **Windows 11**, GTX 1050+/RX 400+/Kaby Lake+ or Ryzen, 4K 60 Hz, **every active display HDCP 2.2**, 15 Mbps, Auto/High quality setting. | VERIFIED (Netflix help) |
| 9 | Launch `--kiosk` / `--app` with a dedicated `--user-data-dir` is fine | Not disproved, but **untested**. `--remote-debugging-port` needs a non-default `--user-data-dir` (Chrome 136+), which is compatible with your plan. | VERIFIED for the CDP rule; UNKNOWN for DRM |
| 10 | Netflix M7375 error when setting `currentTime` | Not part of R1 and not re-checked here. | out of scope |
| 11 | Not mentioned | Windows Store apps: Netflix (reported PWA on Edge WebView2, 4K/HDR/Atmos), Prime Video (1080p stereo). Their architecture is REPORTED, not confirmed by Microsoft. | REPORTED |
| 12 | Not mentioned | Firefox 132+ has PlayReady but Netflix still gives it 1080p. | REPORTED |

---

## 4. Open questions that need real hardware

1. **Does Chrome on the target PC actually engage SL3000?** Which Chrome version, is the flag needed, and does the Netflix stats overlay show 2160p and "PlayReady SL3000"?
2. **Do `--kiosk`, `--app=`, `--user-data-dir`, `--load-extension` and `--remote-debugging-port` each preserve 2160p** on Edge and on Chrome? Test each one separately against a baseline launch.
3. **Does a content script or attached CDP session cause a Netflix error or downgrade?** No source found either way.
4. **Does an evergreen WebView2 shell reach SL3000 on Windows 11 24H2 and later?** The 2024 regression may be fixed.
5. **Does a chosen HDMI dummy plug or an AV receiver in the chain pass HDCP 2.2** and keep 4K?
6. **What are the real caps for Max, Peacock, Paramount+ and Disney+ on Edge versus Chrome** at 1080p versus lower? Check with each service's stats overlay.
7. **Netflix Linux default: 720p or 1080p** on current Chrome, Firefox and Chromium builds?
8. **Which Chrome and Edge builds give Atmos or 5.1 for Disney+, Max and Prime?**
9. **Windows Store apps:** is the Netflix app really a WebView2 PWA, and do the Disney+ and Prime apps use PlayReady hardware DRM? If so, could the launcher drive an app instead of a browser?
10. **Does the reduced-DRM behavior on the Microsoft "duplicate display" case reproduce** on current Edge?

---

## 5. Sources

**Primary or official**
- Netflix supported browsers and system requirements (browser table): https://help.netflix.com/en/node/30081
- Netflix, how to use Netflix on Windows (4K, HDR, Atmos, GPUs, all-displays rule): https://help.netflix.com/en/node/23931
- Netflix, browser table (older copy): https://help.netflix.com/en/node/23742
- Netflix, Ultra HD requirements: https://help.netflix.com/en/node/13444
- Amazon, Prime Video system requirements: https://www.primevideo.com/help?nodeId=GUX9FYHU5D8LC9EJ and https://www.amazon.com/gp/help/customer/display.html?nodeId=GUVGB3QMQRYRERYW
- Apple Support, Apple TV 4K/HDR/Atmos and web playback: https://support.apple.com/en-us/119599
- HBO Max, 4K/HDR device list: https://help.hbomax.com/us/Answer/Detail/000002523 and https://help.hbomax.com/devices
- Peacock, 4K help: https://www.peacocktv.com/help/article/4k-uhd
- Disney+, video quality and browser requirements (snippets only): https://help.disneyplus.com/article/disneyplus-video-quality and https://help.disneyplus.com/article/disneyplus-computer-browser-requirements
- Hulu, video quality (snippet only): https://help.hulu.com/article/hulu-video-quality
- Chrome for Developers, remote-debugging change (Chrome 136): https://developer.chrome.com/blog/remote-debugging-port
- Microsoft Learn, Edge kiosk mode: https://learn.microsoft.com/en-us/deployedge/microsoft-edge-configure-kiosk-mode
- castLabs, Widevine certification and VMP signing: https://castlabs.com/security/widevine-certification/ and https://github.com/castlabs/electron-releases
- WebView2 feedback tracker: https://github.com/MicrosoftEdge/WebView2Feedback/issues/4828, /4632, /4935
- Chromium tracker (login required, not read): https://issues.chromium.org/issues/378869813

**Vendor technical docs**
- DoveRunner, PlayReady SL3000 on Windows Chrome (version, x64-only, key system, flag): https://docs.doverunner.com/content-security/multi-drm/advanced-guides/playready-sl3000-windows-chrome/
- forasoft, Widevine L1/L2/L3: https://www.forasoft.com/learn/video-streaming/articles-streaming/widevine-l1-l2-l3
- forasoft, PlayReady deep dive: https://www.forasoft.com/learn/video-streaming/articles-streaming/playready-deep-dive

**Secondary news (2026) on Chrome 4K Netflix**
- Son-Video, July 2026: https://blog.son-video.com/en/2026/07/watching-netflix-in-4k-on-chrome-is-finally-possible/
- PCQuest, August 5, 2026: https://www.pcquest.com/news/netflix-just-unlocked-4k-in-chrome-but-your-pc-may-still-block-it-12231817
- ecoustics, August 6, 2026: https://www.ecoustics.com/news/netflix-4k-chrome-windows/
- Windows Forum (page returned 403; seen via search snippets): https://windowsforum.com/news/netflix-adds-chrome-4k-on-windows-11-but-not-hdr.441848/
- Windows Report, Chrome PlayReady prep: https://windowsreport.com/google-chrome-prepares-playready-drm-support-for-windows-11/
- HowToGeek, March 28, 2026 (pre-change; says Chrome is capped): https://www.howtogeek.com/why-netflix-caps-chrome-at-1080p-without-telling-you/

**Forums and community (REPORTED)**
- Microsoft Q&A, AMD RX blocklist and PlayReady CDM: https://learn.microsoft.com/en-us/answers/questions/2405977/playready-content-decryption-module
- Microsoft Q&A, PlayReady vs Widevine on Edge: https://learn.microsoft.com/en-us/answers/questions/2383299/playready-drm-and-widevine-drm-conflict-on-edge-at
- dev.to, netflix-force-4k, January 2025: https://dev.to/picklepixel/how-i-made-netflix-give-me-4k-because-apparently-my-browser-wasnt-good-enough-4fa2
- gHacks, Firefox 132 PlayReady, October 2024: https://www.ghacks.net/2024/10/29/firefox-132-mozilla-paves-way-for-4k-netflix-playback/
- Linux Mint forums and Asahi write-up on Linux Netflix: https://forums.linuxmint.com/viewtopic.php?t=433004 and https://www.da.vidbuchanan.co.uk/blog/netflix-on-asahi.html

**Low-reliability sources noted and not relied on**
- unstore.io and windowsmode.com make claims (Chrome "Widevine L1", Disney+ Windows app 4K) that conflict with the primary pages above.
- minitool's HBO Max "Edge and Firefox get 4K" claim conflicts with HBO Max's own device list.
- One search (Peacock query) tripped the local prompt-injection hook on a "Unicode escape sequence". The results contained no instructions, and nothing from them was used beyond ordinary facts.
