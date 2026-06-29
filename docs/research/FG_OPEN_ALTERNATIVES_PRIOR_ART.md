# FG_OPEN_ALTERNATIVES_PRIOR_ART — free/open LSFG alternatives as behavior guides (FSR3 · AFMF · RIFE · the ecosystem)

> **Diátaxis type:** Analysis (a SOTA dossier, sibling of [`FG_VFI_PRIOR_ART.md`](FG_VFI_PRIOR_ART.md) /
> [`FG_CADENCE_LATENCY_PRIOR_ART.md`](FG_CADENCE_LATENCY_PRIOR_ART.md) / [`FG_SATURATION_PRIOR_ART.md`](FG_SATURATION_PRIOR_ART.md) /
> [`FG_NVOFA_PRIOR_ART.md`](FG_NVOFA_PRIOR_ART.md)). **Status:** `measured`/`designed` — from a 6-agent workflow
> (`wf_55c6b763`: 5 researchers + a convergence synthesizer; the synthesizer **read `apps/render_assistant/src/main.cpp`
> first-hand** to place us). **Verifiable-reference mandate (FDP):** every external claim is leveled (§6); the
> FidelityFX pipeline is grounded in the published GPUOpen technique doc + the MIT source.
> **Operator framing (governing):** these are **behavior GUIDES to study, not truths** — every free tool is inferior to
> LSFG *for a reason*. The prize is **where MULTIPLE independent tools CONVERGE** (a likely-correct behavior to perfect)
> and **where each falls short of LSFG** (the line to EXCEED, not copy). We weight the no-engine-data tools (AFMF, LSFG)
> as the apt peers; FSR3/DLSS-G/RIFE get their edge from data an external-capture FG **cannot** capture.

## §1 — Headline: we sit in LSFG's EXACT tier, only harder — LSFG is the north star, the free tools are the floor

PhyriadFG is **uninformed image-space interpolation** (no game motion vectors, no depth) — the SAME class as LSFG.
FSR3, DLSS-G and (partly) RIFE get their quality from **engine motion vectors + depth** we cannot capture; their
algorithms are useful only for the parts that **survive without that data**. The only **free** peer in our exact class
is **AMD AFMF** (driver-level, MV-free) — and it is measurably weaker than LSFG on the axis that matters (smoothness),
which is *why the $7 tool wins despite free options existing* [V2]. We are even harder-constrained than LSFG: no Reflex
hook, no HUD-less UI mask (both available only to the driver/engine path). **LSFG, not the free tools, is the apt north
star;** the free tools are the floor to clear.

## §2 — The three CONVERGENT behavior guides (the prize)

Where several independent tools do the SAME thing, it is a likely-correct behavior to perfect:

1. **HARD-PICK ONE DIRECTION AT DISOCCLUSION — NEVER BLEND GARBAGE.** FSR3 computes a **dual disocclusion mask**
   (interpolated-vs-prev AND interpolated-vs-cur) to *discard* one reprojection per pixel [V1]; RIFE learns a soft
   fusion mask M that hard-leans to one warped source + a RefineNet residual [V1]; the GFFE/extrapolation line
   classifies-then-fills. They converge because a revealed region is, by construction, visible in **exactly one** of the
   two frames — averaging both is provably wrong there. **We lack depth, so the MV-free way to compute the mask is
   forward/backward flow CONSISTENCY** (where fwd and bwd flow disagree = disocclusion; FSR derives the same signal from
   depth).
2. **DEGRADE GRACEFULLY THROUGH FAST MOTION — ASSERT THE SLICE, never self-disable.** AFMF's load-bearing decision —
   *gate FG OFF when motion exceeds correspondence* — is its **biggest documented failure**: a hard cut-to-native
   stutter exactly when smoothness is wanted [V1]. FSR3/RIFE/LSFG all instead **keep emitting** and let quality degrade
   (crossfade / pick-cleanest). A cadence break is perceptually worse than an interpolation artifact. The
   *correctly-transferable* half of AFMF is the explicit **motion-magnitude / correspondence-confidence DETECTOR**
   feeding a graceful **fallback ladder** (crossfade rung, not OFF) + a **measured base-fps floor** (AFMF ~55-70fps;
   LSFG 3 ships a hard <10fps disable [V2]) surfaced from telemetry, not hardcoded. **Our make-space/governor instinct is
   the correct LSFG-side of this fork** — confirmed.
3. **PACE TO A WALL-CLOCK TARGET WITH A SMOOTHED INTERVAL + SPIN-WAIT, metered on DISPLAY-change.** The FSR3 open pacer
   (the only fully documented one) prescribes: a high-priority thread holding a **MOVING AVERAGE** of the real-frame
   interval (**never the instantaneous delta** — that is the 1-2ms alternating oscillation), a computed target present
   delta, and a **busy-WAIT (spin, not Sleep** — scheduler granularity reintroduces the step) to the wall-clock target;
   present generated-then-real so every adjacent interval equals the target; meter on `MsBetweenDisplayChange`, not
   `MsBetweenPresents` [V1]. Convergence: **stepping is a present/pacing problem (constant cadence vs the real/generated
   frametime alternation), NOT a warp-quality problem** — the cure is a clock, not a better flow.

The FSR3-specific-but-MV-free extras worth stealing: the **half-vector midpoint scatter** (priority-resolved atomic
scatter of flow candidates to t=0.5, best-candidate-wins-cell — a parallel alternative to our medoid/consensus selection)
and the **hole-aware pyramid inpaint** (leave disoccluded bands EMPTY, build a hole-ignoring mip pyramid, fill from
nearest valid color — instead of the color-deforming reveal-fill).

## §3 — Per-tool findings (the guides, honestly)

- **FSR3 FG** [open-source, MIT, GPUOpen]: a fixed 9-pass interpolation — setup/atomic-clear → estimate-interp-depth
  (half-vector midpoint reprojection) → game-MV scatter field (priority-keyed) → SPD MV inpaint → optical-flow field
  (8×8 SAD over a 6-level luma pyramid) → dual disocclusion mask → two-step blend (disocclusion-gated, then game-MV vs
  OF by color match, holes LEFT EMPTY) → SPD inpaint pyramid → inpaint. **Weakness vs LSFG:** structurally non-transferable
  (hard-dependent on MVs+depth+per-game integration); AMD admits a **zig-zag present pattern** and recommends the user
  cap to ½ refresh; no assert-the-slice under saturation. **Strength:** disocclusion QUALITY (the dual-mask) — the
  frontier we concede to LSFG.
- **AFMF / AFMF 2** [free, driver-level, **MV-free — our closest peer**]: infers all motion from pixel correspondence on
  the last two presented frames; **self-DISABLES under fast motion** (the failure); ~55-70fps floor; VSync-off,
  fullscreen/borderless required; AMD-only. AFMF 2 adds Search Modes + ~28% lower latency w/ Anti-Lag [V2]. **Honest edge
  over LSFG:** lower input lag (driver present hook + droppable interp frame) — which a third-party overlay **cannot**
  structurally match. **We must win on robustness, not latency.**
- **RIFE / Practical-RIFE / SVP** [open-source neural VFI]: **IFNet** = a coarse-to-fine (1/4→1/2→1/1) stack predicting
  the **intermediate flow directly** (Ft→0, Ft→1) with no cost volume, then a learned **fusion mask M** + RefineNet.
  **Weakness:** pure interpolation (same +1-frame anchor); GPU cost far heavier than LSFG's tiny pass (68ms@1080p on
  Titan X; ~RTX-3080-class for 1080p60 realtime [V2]); same disocclusion/fast-pan failure modes. **The neural BEHAVIOR
  to study** (the 4090 has idle tensor cores): direct intermediate-flow + the learned fusion-mask-as-disocclusion-arbiter.
- **The ecosystem** [OptiScaler (MIT, OptiFG = FSR3-FG, DX12-only), XeSS-FG, ExtraSS]: the community gap — LSFG wins on
  **universal two-click external capture + smoothness** on uncapped/contended sources; the free tools are per-API or
  per-engine or AMD-locked. AFMF 2 has lower lag + fewer raw artifacts than LSFG but **worse overall smoothness** [V2].

## §4 — The gaps this revealed in OUR stack (first-hand-verified)

The synthesizer read our code and found:
- **Our reveal-fill still COLOR-DEFORMS** (`mix(result, cur[uv], …)` — the operator's already-flagged bug). The
  convergent fix is **hard-commit + hole-aware inpaint**, never blend.
- **We do NOT compute a per-pixel forward/backward flow-CONSISTENCY map** — we have a backward field (STAGE-48
  `occl_thresh`) but use it coarsely, not as a clean per-pixel disagreement → hard-pick. The disocclusion frontier we
  lose on is exactly this.
- **Pacing is already done right** (verified: a fixed-period wall-clock target `tick_t0 + tick_k·tick_period_ms`,
  finished by coarse-sleep + spin via `paced_wait_P`/`hal::cpu_wait_for_ns`, with a resync-when->4-periods-behind guard;
  low measured `slip`). The only **audit gap**: confirm we smooth the interval as a **moving average** and meter on
  display-change.
- **Forward-extrapolation infra exists** (`--asw`/`--asw-max`, `--camera-twarp`) but is gated behind disocclusion-fill
  quality.

## §5 — The recommended next lever + the one axis to EXCEED LSFG

**Highest-value next build = the forward/backward-flow-CONSISTENCY disocclusion mask → HARD per-pixel direction-pick +
HOLE-AWARE pyramid inpaint (NOT color-blend).** It is the single most convergent idea (FSR3 dual-mask + RIFE mask M +
GFFE classify-then-fill), genuinely **MV-free**, and it is **exactly the line where we honestly lose to LSFG today** (the
disocclusion/gravity frontier our eye-tests concede). Doubly-motivated: it replaces the color-deformation bug AND likely
fixes the per-tick **selection-instability "vibration"** (a stable motion-derived pick replacing the unstable medoid).
Build: (a) a per-pixel fwd/bwd disagreement = occlusion confidence (add the bwd pass, or reuse the block-match cost
residual as a cheap proxy, on the near-free 1080 Ti flow stage); (b) in the disoccluded band, hard-commit the single
source that can see the region; (c) where neither is valid, leave EMPTY + fill from a hole-ignoring mip pyramid.
Default-off, byte-identical, no-regression, eye-pass vs LSFG.

**The one axis where a no-MV tool could BEAT LSFG = EXTRAPOLATION** (`--camera-twarp`: warp the latest real forward,
never hold the next — the only **structural** latency win available without engine MVs). But it is **gated behind this
disocclusion-fill quality** (extrapolation reveals more holes). So: **the mask is the prerequisite; extrapolation is the
move that surpasses LSFG once the holes are clean.**

## §6 — References (leveled; ✔ = supervisor-verified first-hand)

| # | Claim | Source | Level |
|---|---|---|---|
|  | FSR3 FG open-source under MIT FidelityFX SDK | https://gpuopen.com/news/fsr3-source-available/ | V1 |
|  | FSR3 9-pass pipeline + dual disocclusion mask + holes-left-empty-then-inpaint | https://gpuopen.com/manuals/fidelityfx_sdk/techniques/frame-interpolation/ | V1 |
|  | FSR3 priority-keyed atomic MV scatter; half-vector midpoint reprojection | https://github.com/AzagraMac/FidelityFX-SDK-FSR3/blob/master/docs/techniques/frame-interpolation.md | V1 |
|  | AFMF is driver-level, MV-free (no engine motion vectors) | https://www.xda-developers.com/amd-fluid-motion-frames/ | V1 |
|  | AFMF DISABLES FG during very fast/chaotic motion (the gate-off failure) | https://www.pcgamesn.com/amd/afmf-fast-motion-response | V1 |
|  | AFMF ~55fps@1080p / ~70fps@1440p floor; VSync-off + fullscreen required | https://www.pchardwarepro.com/en/How-to-activate-AMD-Fluid-Motion-Frames | V2 |
|  | AFMF 2 ~28% lower latency at 4K w/ Anti-Lag vs AFMF 1.3 | https://www.tomshardware.com/pc-components/gpus/amd-fluid-motion-frames-2-lowers-latency-by-28-percent | V2 |
|  | RIFE IFNet: coarse-to-fine (1/4→1/2→1/1) DIRECT intermediate flow + fusion mask M + RefineNet | https://ar5iv.labs.arxiv.org/html/2011.06294 | V1 |
|  | RIFE realtime cost: 68ms@1080p on Titan X Pascal; ~RTX 3080 for 1080p60 (SVP) | https://www.svp-team.com/wiki/RIFE_AI_interpolation | V2 |
|  | Community A/B: AFMF 2 lower lag + fewer artifacts, but LSFG better overall smoothness | https://steamcommunity.com/app/993090/discussions/0/7340374196892441129/ | V2 |
|  | OptiScaler (MIT) OptiFG = FSR3-FG, DX12-only; LSFG = universal two-click external | https://github.com/optiscaler/OptiScaler/wiki/Frame-Generation-Options | V2 |
| ✔ | Our present clock is a fixed wall-clock target + spin-finish + resync guard (low slip) | `apps/render_assistant/src/main.cpp` (`paced_wait_P`) | V1 first-hand |
