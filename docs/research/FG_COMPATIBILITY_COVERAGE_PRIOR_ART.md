# FG_COMPATIBILITY_COVERAGE_PRIOR_ART — real-library breadth for an external-capture FG

> **Diátaxis type:** Analysis (SOTA dossier). **Status:** `measured` (capture/present API behaviour) /
> `designed` (the matrix + the graceful-fail layer). Resolves objective **E2** (the user's actual games
> capture+present correctly, or fail gracefully with a reason). Companion to
> [`PHYRIADFG_OBJECTIVE_VISTA.md`](PHYRIADFG_OBJECTIVE_VISTA.md) §1-E2.
>
> **Provenance.** From workflow `w83r5ydl9` (the coverage agent; the synthesizer read the in-repo anchors
> first-hand). Claims carry the sweep's `[V1]/[V2]/[V3]` levels; the load-bearing API facts are vendor/MS
> primary `[V1]`. The author did NOT independently re-fetch every URL here (unlike the safety/HDR
> dossiers) — §6 flags this. Per FDP §2: no fabricated citation.
>
> **Normativity (BCP 14):** MUST / MUST NOT used only where a real constraint binds.

---

## §1 — PhyriadFG current standing (in-repo, first-hand)

WGC capture + DComp overlay present, single present authority:
- Capture: WGC live `main.cpp:3479-3489` with a DD fallback; 8 ms `MinUpdateInterval`; the one-DWM-compose
  floor is "irreducible for the external class" and the operator's belief it is near-limit is "CORRECT"
  (`GAP §3.1 / :157-191`).
- Present: **DComp overlay, NOT a VkSwapchain** (WDA-excluded, click-through); "VRR/G-Sync/Reflex
  categorically unavailable to a DComp overlay (LSFG hits the identical wall)" (`GAP:390-414`).
- Floors: external-capture only, no injection (`injection = ban`); the anti-cheat overlay fingerprint is an
  open risk (`DCAD:323`). The open-alternatives dossier names LSFG's moat as "universal two-click external
  capture + smoothness" but does **not** decompose the capture/present compatibility surface — the gap this
  dossier fills.

---

## §2 — SOTA findings (leveled)

### A — The capture-API landscape

- **`[V1]`** WGC (`Windows.Graphics.Capture`) captures inside the DWM composition pipeline → works
  reliably on surfaces DWM composes: **borderless / windowed + flip-model fullscreen**. **True exclusive
  fullscreen bypasses DWM → WGC capture "not guaranteed, may behave inconsistently."**
- **`[V1]`** WGC works **cross-GPU with no user intervention**; DXGI Desktop Duplication requires the
  capture app to run **on the same GPU that drives the display** — decisive for a multi-GPU rig.
- **`[V1]`** DXGI Desktop Duplication `DuplicateOutput1` fails `DXGI_ERROR_UNSUPPORTED` against the dGPU on
  a Microsoft Hybrid (laptop iGPU+dGPU) system, **by design.**
- **`[V1]`** WGC `GraphicsCaptureSession`: `IsCursorCaptureEnabled` (Win 2004), `MinUpdateInterval`,
  `DirtyRegionMode` (the only way to tell a new game frame from a new compose tick); base API requires
  Win 1803; the yellow border disables only on Win 11 and **re-appears in exclusive fullscreen.**
- **`[V1]`** **Protected content (DRM/HDCP) returns BLACK to ANY OS capture path** — OS-marked; no
  software-only workaround (identical for WGC, DD, a present-hook).
- **`[V2]`** Present-hooking / swapchain injection is the only sub-compose capture (lower latency, exclusive-
  FS reach) but **counts as injection → anti-cheat ban risk**; LSFG and OBS-class deliberately avoid it.

### B — Present paths, VRR, fullscreen-mode coverage

- **`[V1]`** **Independent flip / DirectFlip** bypasses DWM composition — the only present mode allowing
  G-Sync/VRR + lowest latency. **MPO** lets DWM reserve a HW scanout plane to keep an app in independent
  flip with content on top.
- **`[V1]`** A **composition swapchain** (`IPresentationManager`/DComp) supports per-present target times +
  a skip-to-latest queue, and can go independent-flip **only if "displayable"** — else DWM-composed (+1
  compose frame). An ordinary topmost DComp overlay is composed, and **a topmost overlay forces the game
  underneath OUT of independent flip → disables that game's VRR.**
- **`[V1]` 24H2 broke DXGI/DD for same-monitor FG:** MS made DD "heavily reliant on MPO"; without MPO, DD
  cannot distinguish game-window updates from the FG-window updates on the same monitor → broken pacing
  (LSFG official blog). NVIDIA driver 566.36 later restored DXGI on 24H2 `[V2]`.
- **`[V1]` WGC + HW cursor + VRR:** "in games where a hardware cursor is displayed and VRR is enabled, WGC
  disables independent flip mode globally" (LSFG blog) — capturing via WGC can itself knock the source out
  of VRR.

### C — How LSFG achieves "works on basically anything", and where it still fails

- **`[V1]`** LSFG ships **three capture backends (DXGI default, WGC, GDI fallback)** and switches per
  Windows version / MPO availability. DXGI = best pacing + some VRR; WGC = better compatibility + lower
  latency + 6-9 % less overhead but no dynamic output-rate; GDI = last resort. **The moat is the fallback
  ladder + auto-selection, not any one API.**
- **`[V1]`** LSFG present = a borderless unfocused overlay; **no exclusive fullscreen** (borderless/windowed
  only) — the same class boundary PhyriadFG has.
- **`[V1/V2]`** LSFG disables G-Sync by overlay construction; **LSFG 2.0 added working VRR** via config
  coordination (cap base = VRR cap ÷ multiplier + sync-mode), not a fundamental fix.
- **`[V2]`** LSFG anti-cheat status is **probabilistic per title** (clean in many EAC titles;
  Vanguard/BattlEye may flag; rare CoD ban reports); no formal whitelist.
- **`[V1]`** Residual LSFG failures: exclusive fullscreen (none); 24H2 DXGI/MPO breakage; WGC+cursor+VRR
  kills independent flip; protected content → black; anti-cheat probabilistic.

### D — Graceful-fail-with-reason (the detect-and-explain design)

- **`[V1]`** The OS exposes the exact signals to detect-and-explain before breaking:
  `GraphicsCaptureSession.IsSupported()` (capture available); `DuplicateOutput1`→`DXGI_ERROR_UNSUPPORTED`
  (hybrid/wrong-GPU); a black/zero-luma first frame ⇒ protected content; the Windows build query ⇒ which
  backend + caveats; an MPO-capability probe ⇒ whether independent flip is attainable. **No SOTA standard
  for surfacing the reason exists** (LSFG mostly fails silently) → a structured reason-coded refusal is an
  axis where PhyriadFG can **lead, not merely match.**

---

## §3 — The objective resolved (E2 — five sub-objectives)

**E2-CAP (capture reach).** Floor: exclusive-fullscreen (WGC unreliable; only injection reaches it = ban);
DRM → black; one-DWM-compose latency; hybrid-laptop dGPU DD `UNSUPPORTED`. Standing: WGC + legacy DD,
near theoretical limit, API-agnostic (WGC captures the composed output regardless of the game's render
API → DX11/12/Vk/GL all reach it). LSFG gap: **we match API-reach but trail on the fallback ladder**
(no GDI rung, no per-OS auto-switch); we **lead** on multi-GPU (WGC cross-GPU is our default). Lever: add
a GDI last-resort rung + an explicit per-Windows-build capture-mode selector + `DirtyRegionMode`
source-cadence detection (T1). ★ Criterion: **asymptotic against the exclusive-FS + DRM floor**; the
binary sub-criterion = "every test-set game in borderless/windowed/flip-FS yields a non-black,
source-cadence-correct stream, per the E2-MATRIX"; metric = % of capturable matrix cells passing.

**E2-PRESENT (present reach + VRR/HDR coexist).** Floor: a topmost composed overlay forces the game out of
independent flip → kills VRR + 1 compose frame; HW flip metering unreplicable in software. Standing: DComp
overlay paced to internal `refresh_hz` (not the DWM clock); `submit_at()` returns Unavailable. LSFG gap:
**LSFG 2.0 regained working VRR via config; we have no VRR story** + don't pace to the DWM clock (P0).
Levers: pace to predicted DWM-compose QPC + spin-finish (T1); probe a "displayable" DComp surface →
independent-flip/MPO (T2); a VRR-coexistence config recipe mirroring LSFG 2.0 (T1, mostly config+docs). ★
Criterion: VRR-removal + the compose frame are **floor** (track present-interval jitter on
`MsBetweenDisplayChange`, target sub-4 ms); the *displayable-surface independent-flip* sub-goal IS binary
(verify via PresentMon flip-mode = iflip MPO on ≥1 game).

**E2-MOAT (match "works on basically anything").** **LSFG's moat is the fallback ladder + per-OS
auto-select + years of per-title validation — NOT a superior algorithm.** PhyriadFG structurally CAN match
the capture/present reach (same class) but lacks (i) the multi-backend ladder, (ii) a per-OS auto-selector,
(iii) a validated per-title matrix, (iv) any anti-cheat posture statement. **The single widest *coverage*
gap to LSFG — wider than any algorithm gap.** Levers: the matrix + the graceful-fail layer + the fallback
ladder (T1 each — together they ARE the moat). ★ Criterion: asymptotic; "done enough" = **zero
`broken-without-explanation` cells.**

**E2-GRACE (graceful-fail-with-reason).** Floor: **none — fully achievable** (the one E2 objective with no
structural floor). Standing: not present in repo. LSFG gap: **LSFG fails mostly silently → an axis where
PhyriadFG can LEAD.** Lever: a pre-flight probe + reason taxonomy (T1) — `IsSupported()`; build-version
gates; first-frame zero-luma → `PROTECTED_CONTENT`; `DXGI_ERROR_UNSUPPORTED` → `HYBRID_DD_WRONG_GPU`;
MPO/iflip probe → `VRR_WILL_BE_DISABLED`; exclusive-FS detect → `EXCLUSIVE_FULLSCREEN_SWITCH_TO_BORDERLESS`;
AC-sensitive title → `ANTICHEAT_UNVERIFIED`. ★ **Criterion — fully binary:** "every floor-excluded/broken
cell is reached via a specific reason code from a fixed enumerated taxonomy, NO generic/unknown failures."
**The highest-leverage closeable objective in E2.**

### E2-MATRIX — the coverage matrix (the dimension's closeable criterion)

| Source API | Windowing mode | Expected verdict | Closeable? |
|---|---|---|---|
| DX11/12/Vk/GL | Borderless | works (composed → WGC reaches it, API-agnostic) | yes — pass/fail per title |
| (all 4) | Windowed | works (composed) | yes |
| (all 4) | Flip-model "fullscreen" | works, +1 compose frame | yes |
| (all 4) | **Exclusive fullscreen** | floor-excluded (`EXCLUSIVE_FULLSCREEN`) | won-by-explanation |
| (any) | + HW cursor + VRR | works-with-caveat (WGC disables independent flip → VRR lost) | yes |
| (any) | + HDR / wide-gamut | works (WGC auto-converts) — verify colour (see HDR dossier) | yes |
| (any, protected) | any | floor-excluded (`PROTECTED_CONTENT` → black) | won-by-explanation |
| (any) | hybrid laptop, DD path | DD `UNSUPPORTED` → must be WGC (`HYBRID_DD`) | yes |
| (any) | + kernel AC | unverified → advisory, per-title empirical (see trust dossier) | partial |

**Key simplification:** the 4 source APIs collapse to the same WGC composition path → the load-bearing axis
is **windowing mode + content-protection + anti-cheat, NOT the render API.** ★ **Closeable criterion for E2
as a whole:** "every matrix cell filled with a verdict + evidence + (for non-`works` cells) a reason code,
on the fixed test set; **zero `broken` cells.**" The asymptotic remainder reduces to one tracked metric:
passing-cell count over an expanding test set.

---

## §4 — Sources (leveled)

`[V1]` (sweep, vendor/MS primary): MS Q&A WGC-exclusive-fullscreen; MS Learn DuplicateOutput1 /
Microsoft-Hybrid DDA-unsupported; WGC `GraphicsCaptureSession` API ref; MS "use DXGI flip model" /
comp-swapchain programming guide; the LSFG official blog (24H2 DXGI/MPO + WGC-cursor-VRR); the LSFG Steam
capture-backend guide. `[V2/V3]`: OBS forum (DD same-GPU), Blur Busters / NVIDIA dev-forum (overlay kills
independent flip), the 6-9 % WGC overhead figure (LSFG community), per-title anti-cheat reports.
In-repo: `FG_ARCHITECTURE_DCAD_MASTER_PLAN.md`, `PHYRIADFG_ARCHITECTURE_GAP_AUDIT.md §3.1/§3.7/§3.12`,
`FG_OPEN_ALTERNATIVES_PRIOR_ART.md`.

## §5 — Cross-links

Anti-cheat per-title verdicts → [`FG_TRUST_ANTICHEAT_PRIOR_ART.md`](FG_TRUST_ANTICHEAT_PRIOR_ART.md).
HDR ingest/present colour → [`FG_HDR_FORMAT_PRIOR_ART.md`](FG_HDR_FORMAT_PRIOR_ART.md). Cadence/DWM-clock →
`FG_CADENCE_LATENCY_PRIOR_ART.md`.

## §6 — What could NOT be verified (first-hand)

1. **Whether PhyriadFG already emits any reason-coded capture/present failures** — inferred from the
   anchors, not code-confirmed (the capture/present error paths in `main.cpp` were not read line-by-line).
2. **Whether a topmost DComp overlay forces the *underlying game* out of independent flip in PhyriadFG's
   exact WDA-excluded click-through configuration** — `[V1/V2]` general Windows behaviour; measure with
   PresentMon.
3. **The 6-9 % WGC-vs-DXGI overhead** — LSFG's community number, not independently benchmarked, not
   necessarily transferable.
4. **"NVIDIA 566.36 fixes DXGI on 24H2"** — single-source `[V2]`, driver-version-specific, likely
   superseded by June-2026 drivers; do not rely on it.
5. **Per-title anti-cheat outcomes** — community-empirical, inherently unstable; see the trust dossier.
6. **Exact WGC behaviour under exclusive fullscreen** — documented as "inconsistent/not guaranteed"; the
   matrix marks it `floor-excluded` conservatively.
7. **The author did not re-fetch the §2 URLs first-hand this session** (unlike the trust/HDR dossiers) —
   they carry the sweep's levels; the agent (and the DCAD/gap-audit synthesizer) read them first-hand.
