# FG_MEASUREMENT_METHODOLOGY_PRIOR_ART — measurement parity + the closeable-objective framing

> **Diátaxis type:** Analysis (SOTA dossier). **Status:** `measured` (the instrument is built; the field's
> benchmarking methods) / `designed` (the regression loop + the head-to-head). Resolves objectives **H1**
> (objective/reproducible FG-quality measurement) + **H2** (honest head-to-head vs LSFG), **and the
> meta-problem**: the roadmap has no binary closeable objective — the structural source of "no avanzamos".
> Companion to [`PHYRIADFG_OBJECTIVE_VISTA.md`](PHYRIADFG_OBJECTIVE_VISTA.md) §0/§1-H/§2.
>
> **Provenance.** From workflow `w83r5ydl9` (the measurement agent). Benchmarking-tool facts carry the
> sweep's `[V1]/[V2]/[V3]` levels (PresentMon / NVIDIA / GamersNexus / GPUOpen primary `[V1]`); the
> SROCC numbers are `[V2]` (corroborated band, dataset-fragile decimals). The author did NOT re-fetch every
> URL this session (§5). Per FDP §2: no fabricated citation.

---

## §1 — PhyriadFG current standing (in-repo, first-hand)

- **FG quality has a BUILT instrument, not a capability gap.** `fg_quality_scorer` exists with held-out
  triples, three modes (A flow+warp, B full-pipeline, T truth-less) and motion-masked metrics `dbl_edg_m` /
  `flowdsc` (a FloLPIPS-analogue) — `framework/render/vulkan/bench/fg_quality_scorer/README.md:108-120`. It
  is **NOT wired into CMake/CI** (`README.md:5-7`).
- **The measurement gap is the LOOP, not the instrument.** Roadmap P7 = "an eye-test GATING LOOP + metric
  CALIBRATION against BVI-VFI" (`:212-216`); S6 = the camera/no-reference axis + a concurrent-scorer-on-
  1080Ti (`:275-284`).
- The roadmap's own status is `designed` for the entire P/S list (`:11-13`) — **nothing in it is a *closed*
  objective**; it is all forward work, which is exactly the operator's complaint surfaced by the doc's own
  "pointed at, not obsessed over" framing (`:29-30`).
- The "uncapturable" wall: LSFG's overlay output "is digitally uncapturable, so it literally cannot
  self-measure full-reference; we can" (`roadmap:90-91`; `FG_VFI §8`).
- The metric-correlation ceiling is documented (`FG_VFI §2`): PSNR/SSIM ≪ LPIPS/DISTS ≪ bespoke VFI
  metrics; the ordering robust, the absolute SROCC dataset-fragile.

**Net:** PhyriadFG has the *full-reference self-measurement* capability LSFG structurally cannot have, but
has (a) not wired it into a regression loop, (b) not calibrated its metrics against a human-rated set,
(c) not run the one cross-comparable axis (physical capture) vs LSFG.

---

## §2 — SOTA findings (leveled)

### Industry FG/latency benchmarking standards

- **`[V1]` PresentMon (Intel)** is the open instrumentation layer the whole industry sits on. `GPUBusy`
  (render-period engine-busy), `MsUntilDisplayed` (Present→flip). FrameView + NVIDIA's reviewer chain are
  built on it.
- **`[V1]` PresentMon distinguishes generated vs rendered frames via `FrameType`** (Application / Repeated /
  Intel XeFG / AMD AFMF / NV) and exposes **"All Input to Photon Latency"** + Displayed-FPS separately —
  the field's answer to "FG inflates FPS but not responsiveness."
- **`[V1]` the NVIDIA PC-latency pipeline** is formally decomposed (I2FS + FS2P + P2D), measured via the
  Reflex SDK ETW markers; **render-queue depth lives inside P2D** — why a shallow present queue is the
  external tool's only software latency lever.
- **`[V1]` hardware ground-truth = LDAT / Reflex Analyzer** (a luminance sensor measuring click/motion-to-
  photon incl. the panel) — the only FG-agnostic method that includes display latency.
- **`[V1]` FCAT** = colour-bar overlay + high-speed capture, measuring *delivered* frames; premise:
  **"software cannot measure the frames actually delivered to the display."**
- **`[V1]` AMD Frame Latency Meter (FLM)** measures mouse-to-pixel **in software, no camera** (inject a
  synthetic mouse-move, time until captured pixels change). **Critical nuance: it stops at the captured
  framebuffer, NOT photons — it EXCLUDES physical display latency** (the no-camera approximation). Works on
  any DX11+ GPU.
- **`[V1]` GamersNexus "Animation Error"** is the formal frame-pacing-variance metric — `MsAnimationError =
  (AnimTime_N − AnimTime_{N-1}) − MsBetweenDisplayChange_N`. **★ Load-bearing: it provably CANNOT score FG**
  — "there's no animation time for fake frames, so there's no reference point for calculating error."
  (The *display-interval* half — `MsBetweenDisplayChange` jitter — DOES apply to fake frames; the
  *animation-time* half does not.)

### FG quality metrics (the no-GT ceiling)

- **`[V2]`** On **BVI-VFI** (36 refs, 10,800+ human ratings): the best *classic* metric (LPIPS/FAST) sits
  **below ~0.6-0.64 SROCC**; FloLPIPS beats LPIPS by +0.084 SROCC; a bespoke VFI metric reaches **~0.83
  SROCC.** **Even the best no-GT FG metric explains only ~0.7 of human-judgment variance — measurement of
  the perceptual axis is asymptotic, not closeable.**
- **`[V1]`** A 2025 no-reference rendered-video metric (arXiv:2510.13349) explicitly targets FG ("assess
  frame generation strategies") — the most current NR instrument for exactly our problem (its SROCC table
  could not be extracted — §5).
- **`[V1]`** Full-reference held-out (decimate a high-rate source, reconstruct, compare to the withheld
  real) is the arbiter — PhyriadFG already implements it (`sequence` mode).

### Honest head-to-head vs LSFG given digital uncapturability

- **`[V1]`** The mechanism of LSFG's uncapturability is `SetWindowDisplayAffinity(WDA_EXCLUDEFROMCAPTURE)`
  → removed from all DWM capture surfaces. **Honest refinement of the repo's "digitally uncapturable":** it
  is uncapturable by *window/WGC* capture; it IS capturable by **display duplication / a capture card** (the
  photons are on the panel) — BUT that capture is **unsynchronized to the source reals**, so **full-reference
  scoring of LSFG is impossible**; only no-reference (camera/FCAT/pacing) survives.
- **`[V1]`** The field's cross-vendor method when digital capture is unavailable = **FCAT colour-overlay OR
  high-speed camera** — the only shared instrument that sees both PhyriadFG and LSFG at the photon level.

---

## §3 — The objectives resolved

### H1 — Objective, reproducible FG-quality measurement

- (a) A regression-gated, reproducible per-build number that (i) tracks the operator's eye (calibrated ≥0.7
  SROCC), (ii) runs full-reference where truth exists + truth-less where it doesn't, (iii) gates every
  "improved/regressed" claim in CI, not by eyeball.
- (b) Floor: no-GT perceptual quality is **asymptotic against ~0.7-0.83 SROCC** `[V2]`; live FG output has
  no ground truth by construction → the truth-less live number is permanently a flag, not a verdict.
- (c) Instrument BUILT, **not wired to CI**, **not calibrated** vs BVI-VFI.
- (d) **We LEAD:** LSFG cannot self-score full-reference (overlay WGC-uncapturable + no held-out harness);
  our digital self-capture gives true FR on our own output. We trail the published field only on calibration
  rigor — but so does every shipping FG.
- (e) T0: wire the scorer into CI as a regression gate on a fixed `sequence` corpus. T1: calibrate
  `dbl_edg_m`/`flowdsc` SROCC vs BVI-VFI; durable A/B eye-verdict log → regression signal; adopt the cheap
  NR **DIV** (flow divergence, ~87 % of FloLPIPS correlation at ~37 % cost) into Mode T.
- (f) ★ **CLOSEABLE (binary) = fixed point C8:** "the scorer runs in CI on a pinned held-out corpus and
  fails the build on a >X % `dbl_edg_m` regression — evidenced by a deliberate regression turning CI red."
  Crosses once, stays crossed. **+ ASYMPTOTE:** SROCC of `flowdsc` vs BVI-VFI MOS, "enough" ≥ 0.7, ceiling
  ~0.83.

### H2 — Honest head-to-head vs LSFG

- (a) A reproducible, same-instrument, same-scene comparison scored by an axis that sees BOTH.
- (b) Floor: **full-reference comparison of LSFG is structurally impossible** (WGC-excluded + unsynchronized
  → no held-out triples). The only shared instrument is **photon-level** (capture card / high-speed camera /
  FCAT), scored no-reference (~0.7 ceiling) — and Animation-Error pacing **cannot score fake frames at all**.
- (c) `designed` (S6a).
- (d) Even on the only shared axis (neither can be FR-scored by an external party; both reduce to photon-NR);
  we **LEAD on self-measurement.** The genuine open comparison = photon-level pacing + camera-rated quality
  in the three regimes (low-motion / camera-turn / combat).
- (e) **latency + pacing head-to-head are FG-agnostic and doable NOW (T0):** PresentMon "All-Input-to-Photon"
  + `FrameType` + `MsBetweenDisplayChange` variance on both (and/or LDAT for true photon latency). T1: the
  capture-card / high-speed-camera rig → `dbl_edg_m`/DIV/E_warp on camera-captured **LSFG vs ours**, same
  scene — the field-first honest head-to-head.
- (f) ★ **CLOSEABLE (binary) = fixed point C7:** "on the same BF6 combat scene, PhyriadFG's PresentMon
  All-Input-to-Photon latency ≤ LSFG's + Δ AND displayed-interval (`MsBetweenDisplayChange`) p99 jitter ≤
  LSFG's — both evidenced by a committed PresentMon CSV pair." Binary, repeatable, the field's own `[V1]`
  instrument, **no camera.** **+ ASYMPTOTE:** the camera/NR quality verdict = a per-regime win/match/lose
  scorecard (progress metric = # of the 3 regimes where camera-`dbl_edg_m`(ours) ≤ camera-`dbl_edg_m`(LSFG)).

---

## §4 — ★ The meta-problem: the set of closeable fixed points

The roadmap is *all asymptotic perceptual axes* (`designed`, "pointed at, not obsessed over") — which is
**why it feels like "no fixed frame, we never arrive."** The cure: **separate the closeable fixed points
(binary, cross-once) from the asymptotic axes (track a metric + a named floor).**

| # | Closeable fixed point | Family | Binary test | Status |
|---|---|---|---|---|
| **C1** | Single-GPU slice measured | perf | committed CSV, slice ms recorded | CROSSED (shipped); the *measurement* still OPEN (DCAD Phase1) |
| **C2** | Byte-identical-off | robustness | every default-off flag → bit-identical, diff=0 | largely CROSSED — re-assert in CI |
| **C3** | Trusted on BF6 + anti-cheat | ux/robustness | 30 s BF6+AC clean, no crash/ban, evidenced | OPEN (BF6 = asymptotic-risk tier, see trust dossier) |
| **C4** | HDR ingests without clip | quality | HDR source round-trips, no clip, pixel-diff | OPEN (see HDR dossier) |
| **C5** | Runs on each vendor | scale | NVIDIA+AMD+Intel, one CSV each | OPEN (hardware-blocked; see vendor dossier) |
| **C6** | Pacing jitter < X ms on the panel clock | quality | after P0, p99 interval < X, PresentMon CSV | OPEN/blocked on P0 |
| **C7** | Input latency ≤ LSFG + Δ | perf | PresentMon All-Input-to-Photon CSV pair | **OPEN, doable NOW (T0)** |
| **C8** | FG-quality regression gate in CI | quality | a deliberate regression turns CI red | **OPEN (scorer built, unwired)** |
| **C9** | The one-knob default exists | ux | one target-fps knob, FG holds it, no per-flag tuning | OPEN (P1 unbuilt) |
| **C10** | Graceful single-GPU degrade | robustness | kill GPU-2 → no crash, FG continues | partial (`vk_live()` ships) |

> **The framing rule to give the operator:** **every objective is exactly one kind — label it.** (1) A
> FIXED POINT — binary yes/no test, crosses once, never re-litigated (C1-C10). (2) An ASYMPTOTIC AXIS —
> perceptual quality, no-GT ceiling ~0.7-0.83 SROCC; a single tracked metric + a named floor, "enough" is a
> declared threshold, never 1.0. **The "we never arrive" feeling is treating asymptotic axes as fixed
> points.** Re-annotating each roadmap P-item with its C-test (P0→C6, P1→C9, P3→C7-enabling, P5→C5, P6→C9,
> P7→C8) converts the roadmap from "an obsession with perfection" into "a checklist of crossings + two
> tracked asymptotes (fill quality, metric SROCC)." **The cheapest crossings now (T0, no hardware, no eye):
> C1, C7, C8.**

---

## §5 — Sources + what could NOT be verified

`[V1]`: presentmon.com + github.com/GameTechDev/PresentMon; developer.nvidia.com PC-latency blog; NVIDIA
reviewer toolkit + tftcentral (LDAT/Reflex Analyzer); NVIDIA FCAT Reviewer's Guide + pcper; gpuopen.com FLM
+ github.com/GPUOpen-Tools/frame_latency_meter; gamersnexus.net Animation Error; arxiv 2510.13349; learn.
microsoft.com SetWindowDisplayAffinity. `[V2]`: arxiv BVI-VFI 2210.00823 / FloLPIPS 2207.08119 / 2202.07727
(the SROCC band). In-repo: `fg_quality_scorer/README.md`, `PHYRIADFG_PERFECTION_ROADMAP.md` (§7 P7, §10,
S6), `FG_VFI_PRIOR_ART.md §2/§5/§8`, `FG_CADENCE_LATENCY_PRIOR_ART.md`.

**Could NOT be verified (first-hand):**
- The exact SROCC/PLCC table of arXiv:2510.13349 (PDF unparseable) — abstract-level claims `[V1]`, numbers
  unconfirmed.
- The BVI-VFI decimals — `[V2]` corroborated band ~0.6-0.83; individual decimals dataset-fragile.
- Whether PresentMon "All-Input-to-Photon" attributes latency correctly **through a third-party DComp
  overlay** — metric exists; not confirmed for an external overlay path. **Probe before declaring C7
  turnkey.**
- AMD FLM's exact pixel-change detection + whether it captures a WDA-excluded overlay (likely yes via
  Desktop Duplication of the physical output; unverified) — and its exclusion of panel latency is an
  inference, not `[V1]`.
- Whether LSFG specifically uses `WDA_EXCLUDEFROMCAPTURE` (vs another exclusion) — `[V3]` inference (see the
  trust dossier; the operator-only OBS-capture check resolves it).
- Animation Error has **no fixed good/bad threshold** — C6's "X ms" MUST be chosen by the operator against
  his panel, not borrowed.
- The author did not re-fetch the §2 URLs first-hand this session.

## §6 — Cross-links

The quality metrics → `FG_VFI_PRIOR_ART.md`. The latency/pacing floors → `FG_CADENCE_LATENCY_PRIOR_ART.md`.
The uncapturability mechanism → [`FG_TRUST_ANTICHEAT_PRIOR_ART.md`](FG_TRUST_ANTICHEAT_PRIOR_ART.md). The
built instrument → `framework/render/vulkan/bench/fg_quality_scorer/`.
