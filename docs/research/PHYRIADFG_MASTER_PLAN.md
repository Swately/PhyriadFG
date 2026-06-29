# PHYRIADFG_MASTER_PLAN — the unified Tier-2 plan for PhyriadFG (the WHAT + CONTRACT)

> **Diátaxis type:** Planning (Explanation + master-plan). The **what + contract** for the whole
> of PhyriadFG, operationalizing the 9-family objective constitution of
> [`PHYRIADFG_OBJECTIVE_VISTA.md`](PHYRIADFG_OBJECTIVE_VISTA.md) (families A–H + the C1–C10
> closeable fixed points) into a buildable, dependency-ordered, risk-bearing plan.
>
> **Tier-2 triad — the three are mutually linked and bound to ONE anti-drift spine (§0):**
> - this file (the **WHAT + CONTRACT**),
> - [`PHYRIADFG_IMPLEMENTATION_STRATEGIES.md`](PHYRIADFG_IMPLEMENTATION_STRATEGIES.md) (the **HOW**),
> - [`PHYRIADFG_RISK_REGISTER.md`](PHYRIADFG_RISK_REGISTER.md) (the crash / concurrency / POD-ABI /
>   device-loss / RCE / dogma risks).
>
> Per [`docs/canon/PLAN_TIER_PROTOCOL.md`](../canon/PLAN_TIER_PROTOCOL.md) §2.3 the three are
> co-required for a Tier-2 change and reference one another. **No Tier-2 objective is committed
> while any of its risks is `open`** (each `mitigated`-as-code or explicitly `accepted`).
>
> **Status:** `designed` (this is planning; implementation is FROZEN). Per-objective honesty buckets
> are stated inline (`shipping` / `built` / `measured` / `designed` / `MISSING`); they are first-hand
> at HEAD `8664445` (branch `main`) and supersede stale vista/roadmap/dossier text — see §0 below.
>
> **Plan-tier (PLAN_TIER_PROTOCOL): Tier-2.** Multiple objectives are crash/device-loss/POD-ABI/
> concurrency/RCE class (P3, P5/E1a/D1, D2, A3·L4, F3b-L3, C1·STEP4) → the RISK_REGISTER is
> **mandatory**. **New risk homes declared here = the triad
> [`PHYRIADFG_RISK_REGISTER.md`](PHYRIADFG_RISK_REGISTER.md):** A3 (R-A3-1..3); the merged
> P5/E1a≡D1 `GpuDescriptor`-POD rows; **D2 (R-D2-1..4)**; the **E2-PRESENT "displayable DComp
> surface" probe**; F3b-L3 (CP-CR1..6 — may later split into a dedicated
> `CONTROLPLANE_RISK_REGISTER.md` when F3b-L3 is built); G1 (R-G1-1). The four existing per-feature
> registers (`FG_SATURATION_STABILITY_RISK_REGISTER`, `SINGLE_GPU_COLLAPSE_RISK_REGISTER`,
> `DEVICE_LOST_RECOVERY_RISK_REGISTER`, `REAL_FAST_PATH_RISK_REGISTER`) are **INDEXED, not
> duplicated**.
>
> **Verifiable-reference mandate (FDP).** Every `file:line` here is copied verbatim from the
> first-hand-grounded cluster plans; **lines drift every session — re-grep before editing** (the
> present-fence drift, DRIFT-1, is proof). No benchmark is minted here; numbers cite the dossier/
> register that measured them and the single source of truth
> [`docs/planning/BENCHMARK_FAIRNESS.md`](BENCHMARK_FAIRNESS.md).
>
> **Annexed flag (do not lose):** [`PHYRIADFG_UI_MASTER_PLAN.md`](PHYRIADFG_UI_MASTER_PLAN.md) is
> **stale** at `:49` (still says ImGui/GLFW — pre-pivot). G1 here is authored as **Tauri + React**
> per the operator's 2026-06-21 decision (confirmed at
> [`CONTROLPLANE_MASTER_PLAN.md`](CONTROLPLANE_MASTER_PLAN.md)`:18` — "PhyriadFG's UI is a Tauri +
> React app"). **`PHYRIADFG_UI_MASTER_PLAN.md` needs updating to Tauri/React.**

---

## §0 — THE ANTI-DRIFT SPINE (shared verbatim across all three triad files)

> **This triad operationalizes `PHYRIADFG_OBJECTIVE_VISTA.md` (the 9-family objective constitution
> A1–H2 + the C1–C10 closeable fixed points + the FIXED-POINT-vs-ASYMPTOTE rule) into a buildable,
> dependency-ordered, risk-bearing plan.** It sits BELOW the vista (which says *what*) and ABOVE the
> per-feature dossiers + the existing per-feature plans (which own the *SOTA* and the *per-gap how*).
> It is the UNIFYING layer: it INDEXES the dossiers (`FG_*_PRIOR_ART`), the architecture plan
> (`FG_ARCHITECTURE_DCAD_MASTER_PLAN`), the per-feature triads (`FG_SATURATION_STABILITY_*`,
> `SINGLE_GPU_COLLAPSE_RISK_REGISTER`, `DEVICE_LOST_RECOVERY_RISK_REGISTER`, `REAL_FAST_PATH_*`,
> `INPUT_LAG_DREDUCTION_MASTER_PLAN`, `CONTROLPLANE_MASTER_PLAN`, `PHYRIADFG_UI_MASTER_PLAN`), and
> the cross-dimension `PHYRIADFG_PERFECTION_ROADMAP` (the P0–P7/S1–S6 sequence + the §5 dependency
> spine + the §10 honest floors); it re-derives none of them. Where the roadmap and the vista
> disagree with first-hand HEAD, **HEAD wins and the divergence is logged in §6 of the synthesis
> (CONFLICTS/DEDUP), carried into the per-objective standings below.**
>
> **Binding invariants (every objective, every wave, every commit MUST uphold — non-negotiable):**
> - **Byte-identical-off.** Every new lever is a default-off flag in `parse_extra` (NOT the main
>   parse chain — C1061); the default binary is bit-identical to current. Verified by a `--csv`/
>   pixel diff (off-run == baseline), never asserted. The lone exceptions are explicitly
>   measured-gated, not off-proven: A1·S2 field-quality changes to default-on consumers
>   (`bg_snap`/`band_xfade`), and any default-flip — each gated on a committed held-out scorer win,
>   not a byte-proof.
> - **No-cap dogma.** Nothing caps, throttles, or recommends-capping the captured game's render
>   rate. The make-space / fractional-controller / governor work asserts the FG slice
>   (LSFG-AFG-style target-output-fps), never L1 base-game-cap.
> - **Verify-before-claim (FDP verifiable-reference mandate).** No fabricated path/line/API/
>   benchmark; every code site is grepped first-hand before it is edited (lines drift every
>   session — DRIFT-1/2/3 below are proof); every cited dossier number inherits its `[V1]/[V2]/[V3]`
>   level and is not re-minted. The single benchmark source of truth is
>   `docs/planning/BENCHMARK_FAIRNESS.md` — committed numbers route there, never duplicated.
> - **FIXED-POINT-vs-ASYMPTOTE labelling.** Every objective declares one: a FIXED-POINT carries a
>   *binary* closeable test (the C1–C10 crossings); an ASYMPTOTE carries a *single tracked metric +
>   a floor* and an operator-declared "enough" — never a false "done". The triad does not promise
>   to close an asymptote.
> - **Efficiency mandate.** Every hot-path lever is measure-gated; "if you can't measure the hot
>   path you can't claim it's fast." Zero-overhead-off over defensive-on.
> - **Tier ceremony matches blast radius.** T0 → no plan; T1 → this MASTER_PLAN +
>   IMPLEMENTATION_STRATEGIES; T2 → also a RISK_REGISTER, and **no T2 commit while any risk is
>   `open`** (each `mitigated`-as-code or explicitly `accepted`).
>
> **Honesty reconciliations carried forward (HEAD `8664445`, verified this pass — these correct
> stale vista/roadmap/dossier text):**
> - **DRIFT-1** — the synchronous present-fence sites moved `7001/7150/7507` (roadmap §11) →
>   **`7013/7162/7521`** at HEAD and are already `vk_live`-wrapped (the device-loss hole there is
>   closed; the open async-poll hole is `7228/7628/7675`, `mitigated 3f6cbd2`).
> - **DRIFT-2** — the single-GPU slice (C2) is **no longer MISSING**: `measured` on the 4090
>   (~20–22 ms) per `SINGLE_GPU_COLLAPSE_RISK_REGISTER §2.1`; the open remainder is the
>   1080p/1440p/4K sweep + a small-GPU VRAM number.
> - **DRIFT-3** — the fractional controller (P1/C9, STAGE-119 `--target-output-fps`) is **BUILT**
>   default-off (`main.cpp:7844–8428`); open = the flip + the motion-collapse validation, not the
>   build.
> - The **control-plane pillar is built through CP0/CP1/CP3** (`b3253a7`); CP2 (FG adoption) is
>   genuinely unwired (`controlplane` grep in `apps/render_assistant` = 0). The wire POD
>   `TelemetryFrame` carries **no title/PID** → F3a is host-data-light by construction; near-term
>   F3a work is *classification discipline*, not redaction.
> - **F1 WDA divergence collapsed to PARITY** (Lightshot, 2026-06-22) — LSFG is also
>   display-affinity-excluded; the capturable-overlay lever is OPTIONAL go-beyond, not a fix.
> - **G1 frontend = Tauri + React** (operator's 2026-06-21 decision, confirmed at
>   `CONTROLPLANE_MASTER_PLAN.md:18`). `PHYRIADFG_UI_MASTER_PLAN.md:49` is STALE (still ImGui/GLFW —
>   pre-pivot) and the vista G1(e) + `ACTION_PLAN.md:66` wording must be reconciled to Tauri/React.
>   *(This supersedes the synthesis's earlier "ImGui" note, which was based only on the stale UI
>   plan; the operator override governs.)*

---

## §1 — THE UNIFIED DEPENDENCY-ORDERED BUILD PLAN

**The dependency spine (single source, reconciling roadmap §5 with the six clusters):**

```
WAVE 0  (cheapest closeable crossings — T0, no hardware, no eye)
   C2 single-GPU slice SWEEP ─┐
   C8 scorer→CI (self-hosted GPU) │ all independent, no cross-blocks
   C7 PresentMon probe + CSV pair │ (C7 gated only by the DComp-attribution probe)
   C1·STEP0 variance-pacer keystone ┘   [DRIFT-2/3 mean these are tiny]

WAVE 1  (the roadmap P-sequence — the parity spine)
   P0 (DWM clock, T0→T1) ──┬──► P1 (fractional controller flip+validate = C9, T1; BUILT)
                           └──► P3 (--async-present default-flip, T2; register DISCHARGED, eye-held)
   P2 (selection hysteresis, T0→T1) ── independent, parallel
   P4 (disocclusion arbiter, T1) ──────────────────────────► (unblocks S5)
   [A2·P0 == C1·STEP1 == roadmap P0 — ONE item, three names; see §3 / synth §6]

WAVE 2  (new families + surpass — gated on Wave-1 foundations)
   A3 HDR ladder: L1 colorspace(T0) ∥ L2 ingest(T1) → L3 rolloff(T1) → L4 FP16-present(T2) ; L5 linear-light(T1)
   S2 edge-gate promotion (T1) ── feeds P2/P4/HUD ── promote early behind measured win
   E2-GRACE (T1) → E2-MATRIX (T0) → E2-CAP/E2-PRESENT/E2-MOAT (T1 + one T2 probe)
   E1a router POD+wire (T2, P5 body) → E1b fp16-default+DP4a (T1) → E1c low-power profile (T1)
   P5 router (T2) ──┬──► D1 multi-GPU A/B-measure (T2)
                    ├──► D2 independent instances (T2, headline differentiator = roadmap S1)
                    └──► E1 / S3 / S4 capability-gated offload
   F3a/F3b-L1 classification+topology-split (T0, do early) → F3b-L2 WebSocket (T1) → F3b-L3 pairing (T2)
   F1 trust matrix+gate (T1) ; F2 vendor-honesty (T0) ; G1 CP2+launcher (T1, P6) ; H1-calibration/S4-neural/S6 (research tail)
   D3 maintainability spine ── EMERGENT across D1+D2+E1 (T1 hygiene, no standalone build)

HARDWARE / OPERATOR-GATED TAIL (T0-mechanical, blocked on access — never a code gap)
   C5 cross-vendor run (E1-VERIFY: AMD/Intel/non-author-NVIDIA) ── proxy gate = E1a grep-zero
   C3 / F1-BF6 real-rig account-safety campaign (throwaway accounts) ── asymptote
   C2 small-GPU VRAM number ; H2 camera/NR quality rig ; H1 BVI-VFI MOS calibration
```

**Wave assignment table (which fixed point, which wave, which roadmap item):**

| Fixed point / item | Objective(s) | Wave | Roadmap | Tier | Gate to advance |
|---|---|---|---|---|---|
| C2 slice sweep (1080p/1440p/4K) | C2 | 0 | DCAD P1 | T0 | 3 committed CSVs on author rig |
| C8 scorer-in-CI | H1 | 0 | P7 | T0 | deliberate-regression red run (self-hosted GPU) |
| C7 PresentMon vs LSFG | B1/H2 | 0 | (FG-agnostic) | T0 | DComp-attribution probe PASS → CSV pair |
| C1 STEP0 variance-pacer | C1 | 0 | (pre-P0) | T0 | MASD/stddev not worse, watchdog 0 |
| **P0 DWM clock** = **A2·P0** = **C6** | A2/C1/B1 | 1 | P0 | T0→T1 | PresentMon `MsBetweenDisplayChange` p99 < X |
| **P1 = C9 = G1 one-knob** (BUILT) | C1/G1 | 1 | P1 | T1 | flip + motion-collapse validation |
| **P3 async-present flip** | B1/C1 | 1 | P3 | **T2** | operator eye (register discharged) + `--validation` |
| P2 selection hysteresis | A1 | 1 | P2 | T0→T1 | scorer flip-rate ↓, no `dbl_edg_m` regress |
| P4 disocclusion arbiter | A1 | 1 | P4 | T1 | reveal-band scorer win; **gates S5** |
| A3 HDR ladder (L1–L5) | A3/C4 | 2 | NEW | T0/T1/**T2** | A3-1 histogram, A3-2 HDR-display |
| S2 edge-gate promotion | A1 | 2 | S2 | T1 | per-payoff measured win |
| E2-GRACE | E2 | 2 | NEW | T1 | every non-`works` cell → reason code |
| E1a router POD+wire = **P5** | D1/E1 | 2 | P5 | **T2** | operator POD approval; grep-zero vendor |
| D1 multi-GPU A/B | D1 | 2 | P5-consumer | **T2** | committed CSV pair (multi ≤ single) |
| D2 independent instances | D2/C10 | 2 | S1 | **T2** | two CSVs, 60 s, no cross-interference |
| F3b-L3 mobile pairing | F3 | 2 | NEW | **T2** | `controlplane_security_audit` A+B+C green |
| F1 trust matrix/gate | F1 | 2 | NEW | T1 | gate classifies; campaign = tail |
| G1 CP2 + launcher | G1 | 2 | P6 | T1 | CP2 parity w/ `--csv`; C9 operator run |
| C5 cross-vendor | E1 | tail | C5 | T0/HW | grep-zero proxy until hardware |
| C3 / F1-BF6 campaign | F1 | tail | C3 | T2-protocol | throwaway-account survival hrs |

---

## §2 — THE PLAN-TIER TABLE

| Tier | Objectives | RISK_REGISTER required? |
|---|---|---|
| **T0** | C2 slice sweep, C8 wiring, C7 probe+CSV, C1·STEP0/STEP1, A2·P0 (≈), P0 EWMA term, single-scalar P2, A3·L1 colorspace, E2-MATRIX, F2 vendor-honesty, F3a/F3b-L1 classification+topology-split, F1 WDA-lever (optional) | No |
| **T1** | P1 (validate, BUILT), P2 (if grows), P4, A3·L2/L3/L5, S2, E1b, E1c, E2-GRACE, E2-CAP/PRESENT/MOAT (minus the T2 probe), F1 trust matrix+gate, G1 (launcher+CP2), H1 calibration, F3b-L2, D3 (hygiene), S3, S5, P7 | No (MASTER_PLAN + IMPLEMENTATION_STRATEGIES only) |
| **T2** | **P3** async-flip · **P5/E1a/D1** router POD+wire+A/B · **D2** independent instances · **A3·L4** FP16 present · **F3b-L3** mobile pairing · **C1·STEP4** make-space router · **S1**(=D2) · **S4** neural (research, gated on P4+P5+own SOTA) | **YES — no commit while any risk `open`** |

D3 is T1 hygiene with **no standalone register** — its risk-bearing parts are R-D1-1 (POD) and
R-D2-1..4, registered under D1/D2. S4-neural is T2-research but **build-deferred** (needs its own
dedicated SOTA + P4 + P5 first) — recorded as a downstream node, not a build-ready item.

---

## §3 — PER-FAMILY MASTER SECTIONS

> COMPACT index entries for COVERED-family objectives (A1, A2, B1, C1, C2, D1, D3, G1, H1, H2 —
> they have an existing per-feature plan/dossier/register that owns the detail). FULL contract
> blocks for the NEW-family objectives (A3, D2, E1, E2, F1, F3 — no existing per-feature plan).
> The `*_STRATEGIES` annex holds every HOW; the `*_RISK_REGISTER` annex holds every risk row.

### Family A — PERCEPTUAL QUALITY

**A1 — generation / disocclusion quality.** *(COMPACT; ASYMPTOTE; → indexes
`FG_VFI_PRIOR_ART`, `FG_OPEN_ALTERNATIVES_PRIOR_ART`, `FG_ARCHITECTURE_DCAD_MASTER_PLAN §4`,
roadmap P2/P4/S2/S4.)* A cluster of four roadmap items, not one objective; tracked by `flowdsc`
per-regime win/match/lose vs LSFG (floor ~0.83 SROCC). **★ Closeable proxy:** held-out `--qdump`
sequences through `fg_quality_scorer` show the targeted win with **no `dbl_edg_m` regression** +
operator-eye.
- **A1·P2 selection hysteresis** — T0→T1; **MISSING (designed)**; the shader names the empty slot
  at `wap_warp.comp:178`; reuse the STAGE-57 inertia persistence counter (`main.cpp:499`) — extend
  it to gate the candidate PICK, not just MV-adoption. New `--pick-hysteresis F` default 0.
- **A1·P4 disocclusion arbiter** — T1; **partial** (skeleton ships: `bg_snap=true main.cpp:130`,
  `band_xfade=1.0 main.cpp:137`; built-off `disoccl_hardpick=0.f main.cpp:144` wired at
  `wap_warp.comp:645`); the clean per-pixel fwd/bwd consistency MAP is MISSING. **Unblocks S5.**
  Carries the shared SAFETY FIX: `--camera-twarp` leads by the full 6-param affine
  (`wap_warp.comp:738-743`, applied `:770-773`, fit `main.cpp:1572-1608`) — restrict to rotation
  or relabel with its judder ceiling before any default-on.
- **A1·S2 edge-gate promotion** — T1; **built (stub, default-ON dependency)** `igpu_field=true
  main.cpp:124` (binding-11 consumed by bg_snap/band_xfade/disoccl_hardpick) — promote
  `igpu_field.comp` from luma-Sobel to chamfer-distance/occlusion-class; each consumer flip gated
  on a measured held-out scorer win (NOT byte-identical, the consumers are default-on).
- **A1·S4 neural-on-idle-4090** — T1 **research**; **MISSING**; **build-deferred** — needs its own
  dedicated SOTA + P4 + P5 first. Recorded as a downstream node only, no strategy authored.

**A2 — pacing / cadence smoothness.** *(COMPACT; FIXED-POINT C6; → indexes
`FG_CADENCE_LATENCY_PRIOR_ART §1/§4`, `FG_ARCHITECTURE_DCAD_MASTER_PLAN §5`, roadmap P0.)* Flat
ladder locked to the panel clock; the gap is **P0 — the present pacer is a free-running self-anchored
fixed grid, NOT DWM-metered** (grep `DwmGetCompositionTimingInfo|qpcVBlank|DWM_TIMING_INFO` in
`main.cpp` = **0**). Pacer = `paced_wait_P` (`main.cpp:6941`) to `tgt=tick_t0+tick_k*tick_period_ms`
(`main.cpp:7756,7871`). **★ Closeable (C6):** after P0, `MsBetweenDisplayChange` p99 jitter < X
(panel-period × tolerance, operator-set), PresentMon-evidenced. T0→T1. Built-off neighbour
`pace_variance=false main.cpp:145` (STAGE-117) is the STEP-0 substrate. Anchor the DWM metering at
the **present-pacer**, NOT the content PLL (a PLL slew re-introduces the STAGE-93 lurch). New
`--dwm-pace` default OFF. **DROP (do not re-propose):** VRR/integer-N pacing — an external DComp
overlay cannot drive monitor VRR; LSFG disables G-Sync.

> **★ DEDUP — ONE item, three names:** the DWM clock is **A2·P0 = C1·STEP1 = roadmap P0**. It is
> **homed here under A2 (the cadence home, C6)**; B1 and C1 **CITE** it, they do not restate it.

**A3 — HDR / format completeness.** *(FULL; the load-bearing NEW content; FIXED-POINT C4 for
ingest+present; → indexes `FG_HDR_FORMAT_PRIOR_ART §1–§6`, vista A3.)*
- **Design contract (vista A3·a):** HDR source enters at full precision, survives the pipeline with
  no irreversible [0,1] clip, presents colorimetrically correct (HDR10/scRGB on HDR displays,
  rolloff-tonemapped on SDR).
- **★ Closeable success criterion (C4, two-stage binary ladder):** **A3-1** = on a real HDR game
  (WGC capture) a known >1.0-luminance region (via `--qdump`) reaches the warp un-clipped and
  tone-maps with **rolloff not a 255-pile-up** (histogram-evidenced); **A3-2** = on an HDR display,
  no washout/banding + the present swapchain reports an HDR colorspace. Asymptotic tail =
  wide-gamut interpolation accuracy (highlight-retention ratio, today = 0).
- **Honest floors (vista A3·b / dossier §2):** internal scRGB-FP16 *arithmetic* is foreclosed
  (NVIDIA/Intel refuse it; Pascal 1080Ti = 1/64 FP32) → the path is FP16 *ingest+storage+present*
  with quantized interp math, NOT FP16 math through the warp. **10bpc HDR10 (Option 2) is BLOCKED
  for this surface** — MS excludes alpha-blended swapchains and the DComp overlay is
  `DXGI_ALPHA_MODE_PREMULTIPLIED` (`PresentSurface.cpp:222`, confirmed first-hand) → the only HDR
  present path is Option 1 (FP16 scRGB, 64 bpp). The gap-audit's "10bpc is the cheap target" is
  **REJECTED for this surface.**
- **Build tier:** L1 **T0** ∥ L2 **T1** → L3 **T1** → L4 **T2** ; L5 **T1** (measure-gated).
- **Dependencies:** L4 ← L2 (ingest) + L1 (colorspace). The whole ladder is the new family.
- **Current standing (first-hand):** an 8-bit SDR pipeline that recognizes HDR formats and
  hard-clips them. `route_for()` `main.cpp:1778-1784` maps `R16G16B16A16_FLOAT→RT_HDR`;
  `IS_HDR=(route==RT_HDR)` `main.cpp:3058`. **WGC ingest hard-locked BGRA8:**
  `d.fmt=DXGI_FORMAT_B8G8R8A8_UNORM` `main.cpp:3053`. **Hard clip:** `hdr_convert.comp:32`
  `c = min(c, vec3(1.0));`. **Present BGRA8, no colorspace:** `sd.Format =
  DXGI_FORMAT_B8G8R8A8_UNORM` `PresentSurface.cpp:206`; zero `SetColorSpace1`/`SetHDRMetaData`
  (grep = 0). Honesty: **MISSING** (recognizes + clips). *(Fix #1 applied: the hard-clip anchor is
  `hdr_convert.comp:32`; the dossier's `:32` was correct — there is no "dossier drifted by one".)*
- **The five levers (→ full HOW in `PHYRIADFG_IMPLEMENTATION_STRATEGIES.md` A3):**
  - **L1 `SetColorSpace1` on present — T0.** QI `IDXGISwapChain3` on `impl->sc`
    (`PresentSurface.cpp:102`, QI pattern at `:126,:135,:240`); query display colorspace via
    `IDXGIOutput6::GetDesc1().ColorSpace`; `CheckColorSpaceSupport` before set; flag
    `--present-colorspace auto` default no-op. Fixes washout even for SDR on Advanced-Color desktops.
  - **L2 parameterize WGC ingest format — T1.** Detect HDR via `IDXGIOutput6` and set
    `d.fmt=DXGI_FORMAT_R16G16B16A16_FLOAT` at `main.cpp:3053` so HDR reaches RT_HDR + fires
    `hdr_convert.comp`. **Precondition for A3-1.** FLAG: "WGC accepts R16F" is `[V2]` — verify the
    `Direct3D11CaptureFramePool.Create` FP16 call succeeds first-hand. **Test-dependency FLAG:**
    `--qdump`/`dump_rgba` (`main.cpp:1770`) writes raw RGBA8 no-header (`main.cpp:1765`) → cannot
    represent >1.0 FP16; A3-1's histogram needs a new FP16-aware dump (or a pre-clip FP16 buffer
    tap).
  - **L3 replace the hard clip with rolloff + dither — T1.** At `hdr_convert.comp:32` swap
    `min(c,1.0)` for a Reinhard/ACES/BT.2390 rolloff + ordered dither before the srgb_oetf + 8-bit
    store; **mirror in `igpu_convert_pack.comp`** (the clip is mirrored on the iGPU DD-HDR path).
    Reference-white via `DISPLAYCONFIG_SDR_WHITE_LEVEL`. **This is the A3-1 binary.**
  - **L4 true HDR present: FP16 scRGB (NOT 10bpc) — T2.** `PresentSurface.cpp:206`
    BGRA8→`R16G16B16A16_FLOAT`; the bridge images `main.cpp:4707,4723,4752,4765` (all BGRA8 today)
    must become FP16. **This is A3-2 in full.** Touches the present pillar's published surface + the
    cross-process keyed-mutex shared-texture interop → device-loss/corruption class. Risks
    **R-A3-1..3 → triad RISK_REGISTER**.
  - **L5 linear-light compositing for blends — T1, measure-gated.** Decode sRGB→linear around the
    matte/band_xfade/disoccl blends in `wap_warp.comp`, re-encode after; A1↔A3 cross-cutting
    correctness. Lowest priority; sequence after the ingest/present ladder.

### Family B — LATENCY / FEEL

**B1 — input-to-photon responsiveness.** *(COMPACT; ASYMPTOTE-against-a-floor + FIXED-POINT C7;
→ indexes `INPUT_LAG_DREDUCTION_MASTER_PLAN`, `REAL_FAST_PATH_*`, `FG_CADENCE_LATENCY_PRIOR_ART`,
`FG_SATURATION_STABILITY_*`.)* Drive the *removable* added-latency excess toward the structural
floor without a present-before-pair-ready freeze, and make the LSFG comparison evidenced. **★ The
fixed point is C7:** same BF6 scene, our latency ≤ LSFG + Δ, committed PresentMon CSV pair — FG-
agnostic, T0-doable **but PROBE FIRST** (does PresentMon attribute latency through the DComp overlay
— see H2/C7). Standing: space-makers `--rfp-fresh` (`main.cpp:608`), `--cphase` (`:631`),
`--shallow-queue` (`:731-741`) all **measured** default-off; `--async-present` decouple
(`main.cpp:720`, STAGE-102) **built/measured** 73→32.6 ms; C7 CSV pair **MISSING**. The
input-lag floor work converged on the operator reframe (latency is a *budget* — INPUT_LAG §7); the
live floor-minimization front is the ~11 ms F per-pair build gap (INPUT_LAG §8), indexed not
duplicated.

> **★ DEDUP:** the `--async-present` flip is **ONE action homed under C1·STEP3**; B1 **inherits**
> its latency win. The C7 PresentMon-attribution probe is **ONE item homed under H2/C7**; B1 and C1
> **cite** it.

### Family C — PERFORMANCE / HEADROOM

**C1 — assert the slice under saturation ("make-space, not preemption").** *(COMPACT; FIXED-POINT
C9 + variance ASYMPTOTE; → indexes the entire `FG_SATURATION_STABILITY_` triad (MASTER STEP 0–4 +
STRATEGIES S0–S5 + RISK_REGISTER CR1-CR9 + INTEGRATION), `FG_SATURATION_PRIOR_ART`,
`FG_CADENCE_LATENCY_PRIOR_ART`, DCAD §5.)* FG stays on + degrades gracefully through 99% GPU + fast
motion, never cuts to native; the present interval stays flat at a held target. **★ Closeable
(C9):** the one-knob `--target-output-fps` holds a target output fps, measured by present-interval
**MASD + absolute stddev (NOT CoV — CoV inverts under a mean shift; verified vs `bf6_p1_baseline`)**,
target drop toward <~4 ms; "graceful" *feel* is operator-eye-gated. Standing (first-hand, DRIFT-1):
the synchronous present (the C1 target) is `vk_live(vkWaitForFences(A.dev,1,&fBridge,VK_TRUE,
UINT64_MAX))` at **`main.cpp:7013, 7162, 7521`** (moved from the roadmap `7001/7150/7507`, already
`vk_live`-wrapped; the open async-poll hole is `7228/7628/7675`). STEP 2 `--target-output-fps`
**BUILT** default-off (`main.cpp:7844-8428`), measured-inert in the stable 240/18 ms regime, its
target motion-collapse regime UNVALIDATED. STEP 3 `--async-present` BUILT, **flip HELD for operator
eye** (register CR1/2/4/5 `mitigated`, CR3 `accepted`). STEP 4 make-space router **MISSING**
(CR6-CR9 `open`). *(Fix #2 applied: present-fence lines `7013/7162/7521`, not the BC-cluster's
`7003/7152/7510`.)*

> Risks fully enumerated in `FG_SATURATION_STABILITY_RISK_REGISTER` (CR1-CR9) — **indexed, not
> duplicated**. STEP-3 flip is READY (no `open` risk) but HELD for the operator eye + ideally a
> `--validation` messenger first. **No STEP-4 commit until CR6-CR9 are `mitigated`/`accepted`.**

**C2 — the minimal MEASURED single-GPU slice.** *(COMPACT; FIXED-POINT, the cheapest crossing;
→ indexes `SINGLE_GPU_COLLAPSE_RISK_REGISTER`, `FG_ARCHITECTURE_DCAD_MASTER_PLAN §6-8`.)* **★
Closeable:** a committed CSV records the single-GPU slice ms at **1080p / 1440p / 4K** on one GPU,
no fallback-to-vanilla (`--force-single-gpu`). **Fidelity (fix #5 — do NOT flatten):** the
single-GPU FG **CAPABILITY is CROSSED** — 4090 measured ~20-22 ms game→screen
(`SINGLE_GPU_COLLAPSE_RISK_REGISTER §2.1`); the **1080p/1440p/4K SWEEP + the small-GPU VRAM number
are OPEN**. T0 mechanically (the crash-class enablement is already discharged; the 4090 sweep is 3
runs, zero code). Instrument shipped: `--latency-trace` (`main.cpp:161, 1117`, emit `:8733-8740`) +
the `--csv` row (`warp_ms / ms_in_present_api / iter_ms / added_lat_ms / gme_dis_pct`). The small-GPU
VRAM number is hardware-blocked — record as measured-when-available; **do NOT manufacture it**.

> **★ Single-source (fix #10):** the C2 slice-measurement procedure
> (`--force-single-gpu --latency-trace --csv`, each ≥30 s, commit 3 CSVs) is owned **HERE under
> C2**; D1's strategy **CITES** it rather than re-stating it.

### Family D — SCALABILITY / MULTI-GPU

**D1 — multi-GPU as a felt NET benefit.** *(COMPACT; FIXED-POINT; T2; → indexes
`FG_ARCHITECTURE_DCAD_MASTER_PLAN §2/§3/§5/§8`, `FG_VENDOR_LANDSCAPE_PRIOR_ART §2`,
`GPU_MULTI_GPU_PRIOR_ART`; reconcile with `SINGLE_GPU_COLLAPSE_RISK_REGISTER` +
`DEVICE_LOST_RECOVERY_RISK_REGISTER`.)* **★ Closeable:** a committed CSV **pair** shows the
multi-GPU config's felt latency + slice each ≤ the single-GPU config's on the same scene — binary,
currently **asserted-not-measured**. Standing: device selection hardcoded by LUID + deviceType
(`main.cpp:3154`, `:3155`, FD ternary `:3180`); the router is **dead in-product**
(`break_even_decide` = 1 comment at `main.cpp:50`; `select_participants`/`run_routed` = 0 callers in
`apps/`); `GpuDescriptor` has `compute_gflops` only, **no fp16/DP4a field** (the fp16-bound
mis-route hazard). Honesty: **shipping but asserted, not A/B-measured.** D1 = the A/B *measurement*
consumer of the wired router; the wiring body is **P5** (= E1a — see dedup).

> **★ DEDUP (fix #7c):** P5 is **ONE objective** (replace the deviceType/LUID hardcodes at
> `main.cpp:3154-3180` with measured CHARACTERIZE + the POD field + `select_participants`). **D1 (the
> A/B measurement) and E1a (the vendor-agnostic ranking) are two CONSUMERS that cite it.** D1's
> risk surface (the `GpuDescriptor` POD change) is the **same** as E1a's → merged single rows in the
> RISK_REGISTER (fix #8).

**D2 — parallel FG across windows / screens (focus-independent) — roadmap S1.** *(FULL; the
headline differentiator, NO existing per-feature plan; FIXED-POINT C10; T2; → indexes vista D2 +
roadmap S1, DCAD §2 invariants.)*
- **Design contract / "done":** FG runs **independently on ≥2 targets at once** — game A on
  monitor 1 AND game/window B on monitor 2 (out of focus), each correct, neither interfering. The
  operator's #4 objective; **LSFG structurally cannot follow** (its multi-frame-gen is stacked
  passes on ONE target). This is where we LEAD.
- **★ Closeable success criterion (C10):** two games on two monitors, each with its own FG
  instance, run ≥60 s with correct output and **no cross-interference**, evidenced by **two
  telemetry CSVs**. Binary.
- **Build tier:** **T2** (N present authorities + N capture/flow/warp chains + scheduler partition +
  multi-surface DComp + N-way device/queue ownership — a crash/contention surface that does not
  exist today; the cluster's biggest design gap).
- **Dependencies:** **P5** (the wired router partitions the device set per instance) is a HARD gate
  (roadmap S1); the topology pillar (`phyriad::hw`, consumed at `f711102` for `--pin`) for the
  scheduler partition; the present-surface pillar already carries `monitor_index`.
- **Current standing (first-hand) — `designed`/`research`, the substrate is single-instance:** ONE
  present thread → ONE `PresentSurface` (`main.cpp:6901`) with
  `PresentSurfaceDesc.monitor_index=cfg.pres_mon` (`main.cpp:6906` — the surface already knows its
  monitor, but there is one); control flow is process-global (`g_quit`/`g_quit_threads`,
  `main.cpp:6914`); no per-instance lifetime/quit/device partition. No existing plan file matches
  "parallel FG instances." Honesty: **DOES NOT EXIST yet.**
- **The T2 design contract (four pillars — full HOW in the STRATEGIES annex):** (1) refactor the
  process-global singletons into an **`FgInstance` struct** owning its lifetime/target/bridge/queue/
  quit — N=1 MUST reduce to today's exact flow (the regression gate); (2) **independent present
  authority per panel** — each instance its own `PresentSurface`+`monitor_index` and its own present
  thread (the pillar's same-thread create()/submit() contract, `main.cpp:6897`, makes a shared
  present thread structurally impossible → no SLI runt-frames across panels); (3) **multi-surface
  DComp** — N WDA-excluded click-through visuals (verify N ExcludeFromCapture targets flip without
  cross-contention — see FLAG); (4) **scheduler partition** via `select_participants` (P5) + disjoint
  CCX/core pinning (`phyriad::hw::pin_current_thread`). Per-instance `--csv` path templating owed
  (it is a single global path today, `main.cpp:1092`).
- **Risks R-D2-1..4 → triad RISK_REGISTER (fix #4):** R-D2-1 cross-instance shared-singleton
  corruption (concurrency); R-D2-2 present-authority/SLI runt-frames (correctness); R-D2-3
  device-loss now partial — **EXTENDS** `DEVICE_LOST_RECOVERY_RISK_REGISTER` to per-instance scope;
  R-D2-4 resource contention with no net benefit (perf — its discharge IS the two-CSV C10 test).
  **FLAG:** the DComp multi-surface independent-flip behaviour lives in `PresentSurface.cpp` (not
  graspable from `main.cpp` alone) — verify first-hand before the D2 build.

**D3 — internal maintainability / N-scaling spine.** *(COMPACT; ASYMPTOTE, explicitly DEFLATED to
design-debt; T1 hygiene; → indexes DCAD §2/§4, vista D3.)* The ~115-stage `--flag` cascade
consolidated into ONE DCAD spine that scales 1→n GPUs from a single role-graph instantiation.
Tracked metric: the count of hardcoded-topology branches (deviceType/LUID hardcodes at
`main.cpp:3154-3180`, `single_gpu` ternaries, per-stage special-cases) driven toward one unified
ASSIGN path; **no "done", operator declares "enough".** **Emergent** product of doing D1 (one
ASSIGN replaces the hardcodes) + D2 (the `FgInstance` encapsulation IS the 1→n machine) cleanly —
not a standalone build. **No T2 register of its own**; its risk-bearing parts are R-D1-1 (POD) and
R-D2-1..4.

### Family E — COMPATIBILITY / COVERAGE

**E1 — vendor-agnosticism.** *(FULL; NO existing per-feature plan; → indexes
`FG_VENDOR_LANDSCAPE_PRIOR_ART`, DCAD §5 STEP A / §8 P2 / §9 #6; reconcile with
`DEVICE_LOST_RECOVERY_RISK_REGISTER` + `SINGLE_GPU_COLLAPSE_RISK_REGISTER`.)* Splits into E1a (POD
field + router wiring, T2), E1b (fp16 default + measured-gated accelerators, T1), E1c (low-power
profile, T1), E1-VERIFY (cross-vendor run, T0-mechanical / hardware-blocked).
- **E1a — `GpuDescriptor` fp16/DP4a field + wire the dead router (= P5) — T2.**
  - **Contract / "done":** `GpuDescriptor` gains a capability field representing a non-NVIDIA GPU's
    FG-relevant throughput (fp16 packed-math GFLOP/s + a DP4a int8 fallback); the startup path
    replaces the deviceType/name-substring selection (`main.cpp:3154-3155`) and the
    `break_even_decide` dead constant (`main.cpp:50-52`) with a one-time CHARACTERIZE that populates
    by measurement; FLOW ranked by measured fp16/DP4a, not raster, not vendor name. POD layout
    change is operator-approved (DCAD §9 #6).
  - **★ Closeable (the progress metric until hardware):** the count of hardcoded-vendor assumptions
    reaches **zero** — `grep` for (a) deviceType/name-substring branches in the *decision* path,
    (b) the NVIDIA-only break-even default, (c) `GpuDescriptor` fields that cannot represent a
    non-NVIDIA GPU = 0. Grep-checkable binary.
  - **Build tier:** **T2** (published cross-process POD ABI change;
    `GpuDescriptor.hpp` `static_assert(sizeof==128u)` at `:61`, `PHYRIAD_ASSERT_POD` at `:64`;
    touches device-creation startup → device-loss-adjacent). Operator sign-off per DCAD §9 #6.
  - **Dependencies:** DCAD P2; depends on DCAD P1 (the single-GPU slice MEASURED — the break-even
    crossover re-derived per-rig, not the author's ~596). Blocks E1b + E1-VERIFY.
  - **Current standing (first-hand):** `GpuDescriptor.hpp:48-58` MEASURED block has `compute_gflops`
    only, no fp16/DP4a (**MISSING**); `vendor_id` at `:30` is identity not capability;
    `break_even_decide` comment-only at `main.cpp:50-52`, `offload=false` inlined (**MISSING**, dead
    router); selection branches at `main.cpp:3154-3155` (built but vendor/raster-blind); hard-exit
    `!pA`/`!pB` at `main.cpp:3164-3167` (shipping; `--force-single-gpu` at `:3165`); no
    `shaderFloat16`/`VK_KHR_shader_integer_dot_product` probe (grep = 0, **MISSING**).
  - Risks **R-E1a-1≡R-D1-1, R-E1a-2≡R-D1-2, R-E1a-3≡R-D1-3 → single cross-linked rows in the triad
    RISK_REGISTER (fix #8):** POD-ABI break (`GpuDescriptor` layout); fp16-blind mis-route
    (inherits `SINGLE_GPU_COLLAPSE` collapse); startup-probe device-loss (inherits
    `DEVICE_LOST_RECOVERY`, depends on DCAD P0).
- **E1b — fp16 as the default primitive; NVOFA/DP4a measured-gated — T1.** **★ Criterion:** flip
  every accelerator OFF → byte-identical Tier-0 output on any GPU; each enables only on a measured
  break-even; grep for vendor-string branches in the enable logic = 0. NVOFA built/measured
  net-negative (`--nvofa main.cpp:1123`, correctly default-off); the new piece = **Tier-1 DP4a int8
  flow** (MISSING; grep `dot_product`/`shaderInt8` = 0). Depends on E1a's field.
- **E1c — low-power-GPU default profile — T1.** **★ Criterion:** part of C5 — the default path runs
  ≥60 s correct + non-crashing on a low-power GPU with the profile auto-selected (no flag);
  asymptotic on quality. **MISSING** as an auto-selected vendor-neutral profile (the knobs —
  flow-scale/governor/adaptive-ring — exist but are not bound to a measured low-power trigger).
  Partial verification on the 1080 Ti as a proxy weak GPU.
- **E1-VERIFY — the cross-vendor run (C5) — T0-mechanical / hardware-blocked.** The default path
  runs ≥60 s correct on ☐ non-author NVIDIA · ☐ AMD (RDNA2) · ☐ Intel (Arc/Xe), each scorer-banded
  + clean CSV + CHARACTERIZE-populated table. **OPEN** (never run; author has only NVIDIA
  4090+1080Ti). The code-readiness proxy until hardware = the E1a grep-zero count. **No code; do NOT
  manufacture cross-vendor numbers.**

**E2 — real-library breadth.** *(FULL; NO existing per-feature plan; → indexes
`FG_COMPATIBILITY_COVERAGE_PRIOR_ART §2/§3`, DCAD §5/§8 P5, vista E2.)* Five sub-objectives;
prioritize E2-GRACE (the one fully-closeable, zero structural floor) + E2-MATRIX.
- **E2-GRACE — graceful-fail-with-reason — T1.** **★ Closeable (fully binary):** every
  floor-excluded/broken matrix cell is reached via a specific reason code from a **fixed enumerated
  taxonomy**, NO generic/unknown failures. **MISSING** today: capture init uses
  `winrt::check_hresult` (`main.cpp:3956, 3962-3963, 3966-3967` — throws, no reason code); no
  `GraphicsCaptureSession.IsSupported()` preflight (grep = 0); DD `DuplicateOutput`
  (`main.cpp:1820`) silent on the hybrid case; no zero-luma `PROTECTED_CONTENT` detect; WGC
  pool-full drops silent (`main.cpp:3977-3978`). Add a pre-flight probe + reason taxonomy (a fixed
  enum) BEFORE capture init; success path stays byte-identical (additive logging). **No RISK block**
  (pre-hot-path, additive). The `ANTICHEAT_UNVERIFIED` code MUST NOT assert safety (honesty dogma) —
  advises, per the trust dossier (hand-off to F1).
- **E2-MATRIX — the coverage matrix as a living artifact — T0.** Every cell filled with verdict +
  evidence + (non-`works`) a reason code; **zero `broken` cells.** The load-bearing axis is
  windowing-mode + content-protection + anti-cheat, NOT render API (the 4 APIs collapse to the same
  WGC composition path). **MISSING** (tested only BF6 + Star Rail). Create as a tracked doc; register
  in `docs/FORMAL_DOCUMENTS_REGISTER.md`; depends on E2-GRACE for the reason codes.
- **E2-CAP / E2-PRESENT / E2-MOAT — the fallback ladder + present reach — T1 (+ one T2 probe).**
  E2-CAP: a GDI `BitBlt` last-resort rung (none today, grep = 0) + a per-Windows-build capture-mode
  auto-selector (`--capture-api main.cpp:1165` is a flag) + `DirtyRegionMode` source-cadence detect
  (unused, grep = 0). E2-PRESENT: DWM-compose-QPC pacing + spin-finish (= DCAD P5); the
  VRR-coexistence recipe (LSFG-2.0-style) is config+docs (T1); **the "displayable DComp surface"
  probe is the one T2 item** (touches `submit_at`/the present path → device-loss-adjacent).
  E2-MOAT: the assembly of E2-CAP + E2-GRACE + E2-MATRIX; "done enough" = zero
  `broken-without-explanation` cells.

> **★ RISK HOME (fix #3):** the E2-PRESENT T2 "displayable DComp surface" probe risk home is the
> triad **`PHYRIADFG_RISK_REGISTER`** (NOT the non-existent "DCAD P5 RISK_REGISTER" the E-cluster
> deferred to). It MUST inherit the `VK_ERROR_DEVICE_LOST`/present-path mitigations of
> `DEVICE_LOST_RECOVERY_RISK_REGISTER` + `REAL_FAST_PATH_RISK_REGISTER` (cite, do not duplicate).

### Family F — TRUST / SAFETY

**F1 — anti-cheat as a measured trust posture.** *(FULL; NO existing per-feature plan; → indexes
`FG_TRUST_ANTICHEAT_PRIOR_ART §3-F1/§4/§2-B/§5`; the trust matrix is the SAFETY COLUMN of E2's
per-engine table — unify, do not duplicate.)*
- **Design contract / "done":** (1) a living **per-engine trust matrix** populated by evidence, (2)
  an **in-product trust gate** that classifies the attached title and refuses/warns on UNTESTED or
  RECOMMEND-AGAINST engines, (3) a **real-rig empirical protocol** run against throwaway accounts.
  The product never asserts safety it has not measured.
- **★ Closeable criterion — a MATRIX of per-engine binaries:** VAC/BattlEye/EAC **CLOSEABLE
  per-title** ("a throwaway account survives ≥N hrs (N≥10) active MP across ≥2 driver versions + ≥1
  anti-cheat update: zero kick/launch-block/ban, trusted-mode retained, evidenced"); Vanguard / BF6
  Javelin **ASYMPTOTIC** (tracked = cumulative survived-account-hours per engine×title×AC-version,
  **reset to ZERO on any anti-cheat update**; honest "done" = survives + operator accepts residual
  risk + recommends-against-ranked).
- **Build tiers (split):** trust gate **T1**; window de-fingerprint probe **T1**; the WDA-condition
  lever **T0 OPTIONAL**; the empirical campaign **T2** (operator-gated, irreversible account-ban
  risk — but a *measurement protocol*, the risk lives in the throwaway-account rule, not crashable
  code).
- **Current standing (first-hand):** WDA-exclusion applied at `PresentSurface.cpp:247`
  (`SetWindowDisplayAffinity(impl->hwnd, WDA_EXCLUDEFROMCAPTURE)`), conditioned by
  `PresentSurfaceDesc.capture` (default ExcludeFromCapture, `PresentSurface.hpp:60-63,72`) — **the
  capturable-overlay switch already exists** (flip the desc to `Normal`, no new code). The overlay
  fingerprint tuple is at `PresentSurface.cpp:177-184` (topmost+layered+transparent, game-sized,
  un-mitigated). Title resolved by substring at `main.cpp:1839` via `EnumWindows` (`:1844`); PID at
  `main.cpp:6887`. **No trust matrix / gate / campaign log exist** — the F1 deliverables are MISSING.
  **F1 WDA divergence collapsed to PARITY** (LSFG is also display-affinity-excluded, Lightshot
  2026-06-22) → the capturable-overlay lever is OPTIONAL go-beyond, not a fix. **STALE-REF FLAG (a
  cleanup):** `PresentSurface.cpp:12` comments "WDA … main.cpp:277-285" — the call is in that same
  file at `:247`.
- **The deliverables (full HOW in the STRATEGIES annex):** (1) the trust matrix as the same
  artifact as E2's compatibility matrix (one `per_engine_matrix` with a `trust_class` +
  `survived_account_hours` + `last_ac_version` column), reusing E2-GRACE's reason-code taxonomy; (2)
  the in-product gate at window resolution (`main.cpp:1839`/`:3049`) — print a clear verdict
  (TRUSTED-with-evidence / UNTESTED-warn / RECOMMEND-AGAINST) before the present loop, default for
  unmatched = UNTESTED-warn (never silent-allow); new `--trust-gate {warn|block|off}` in
  `parse_extra`, default `warn`; (3) `--capturable-overlay` flipping
  `PresentSurfaceDesc.capture=Normal`, default-OFF; (4) a window de-fingerprint probe (honest
  null-allowed); (5) the §4 5-step campaign as an operator runbook + a `survived_account_hours`
  append-log, **two arms (WDA-on vs WDA-off)**, throwaway accounts only.
- **No RISK block** — the only irreversible hazard is the account ban, mitigated by the
  throwaway-account protocol rule (not crashable code).

**F2 — don't destabilize the captured game (VRR/G-Sync vendor split).** *(FULL; small, NO separate
plan; → indexes `FG_TRUST_ANTICHEAT_PRIOR_ART §2-D/§3-F2`.)*
- **Design contract / "done":** FG-on never degrades the baseline AND the product **honestly states
  the VRR/G-Sync outcome per GPU vendor** rather than implying parity it cannot deliver.
- **★ Closeable criterion — split by vendor:** **AMD/Intel CLOSEABLE BINARY** (monitor OSD reports
  VRR ACTIVE with PhyriadFG attached, photo/OSD-evidenced); **NVIDIA G-Sync NOT closeable — a
  permanent class floor shared exactly with LSFG** (honest "done" = documented limitation + the
  windowed-G-Sync partial mitigation; **stop trying to close it**).
- **Build tier:** **T0** (vendor-conditional honesty string + windowed-G-Sync guidance doc); the
  MPO/displayable present-mode probe **T1** (likely null on NVIDIA, possibly positive on AMD/Intel).
- **Current standing (first-hand):** **no VRR/G-Sync/FreeSync/AllowTearing code exists in
  `main.cpp`** (grep = 0) — F2 is today purely a documentation/honesty objective; the floor itself is
  structural/measured in the dossier (`[V1]` NVIDIA dev forum); named at
  `PHYRIADFG_ARCHITECTURE_GAP_AUDIT.md:399,:673` but the contract is unstated. **MISSING** (the
  honesty surface). Strategy: print the honest vendor verdict at startup (sourced from dossier §3-F2
  verbatim); a windowed-G-Sync guidance doc section; the MPO probe with an honest null-allowed
  outcome. **Never hook the game swapchain** (= injection = ban). No RISK block (T0/T1, no crash/RCE
  surface).

**F3 — telemetry / control-plane privacy & security.** *(FULL; NO separate plan; → indexes
`FG_CONTROLPLANE_SECURITY_PRIOR_ART §2 (S1–S9)/§3`, `CONTROLPLANE_MASTER_PLAN §5/§6`,
`FR_CONTROLPLANE_IMPL §0/§1`.)*
- **Design contract / "done" (two halves):** **F3a (egress confidentiality)** — every byte that can
  leave the box is enumerated + sensitivity-classified; the network transport emits only `public`
  fields unless an explicit `--allow-host-data`; title / raw PID / GPU UUID / profile name never
  egress by default. **F3b (ingress integrity)** — no command-channel message mutates FG state (or
  spawns/writes/kills) without per-message authentication; transport local-only by default; **no
  `network-bind + auth-disabled` configuration is constructible** (a build/type-level invariant, the
  OBS "one-click-off auth" footgun).
- **★ Closeable criterion — the committed CI `controlplane_security_audit`, three parts all
  green:** **(A)** packet capture with `--allow-host-data` OFF over a full BF6 session →
  `grep` for the known title/PID/UUID/profile values returns **0**; **(B)** an unauthenticated
  client (raw socket skipping the handshake **+** a cross-origin browser with a forged JS `Origin`)
  causes **ZERO** observable FG state change; **(C)** a **static/type-level assertion** that no
  `network-bind + auth-disabled` config is constructible.
- **Build tiers (split):** **F3a-L1/L2 + F3b-L1 (egress/ingress split + sensitivity classification):
  T0** (do now, before any socket); **F3b-L2 (local WebSocket + Origin allowlist + auth-before-
  command): T1**; **F3b-L3 (mobile-companion pairing): T2 — RCE-class + concurrency → CP-CR1..6 →
  triad RISK_REGISTER** (may later split into a dedicated `CONTROLPLANE_RISK_REGISTER.md` when
  F3b-L3 is built).
- **Current standing (first-hand — CORRECTS the dossiers):** `framework/controlplane/` is **built
  through CP0/CP1/CP3** (`b3253a7`): `Egress` (ring+drain, `Egress.hpp:84,143`),
  `StdoutNdjsonTransport` (`ITransport.hpp:33`), `Ndjson.hpp` serializer (fields by name, a
  `"gpu":[...]` array at `:204`), `Ingress` stub (`Ingress.hpp:38`), `ControlCommand` (Start/Stop
  only, `ControlCommand.hpp:36-40`) + 4 tests. **The wire POD `TelemetryFrame` carries NO title and
  NO PID** (`TelemetryFrame.hpp:76-110`: header + numeric metrics + `GpuTelemetry[4]`; GPU identity
  is the array index) → **F3a is safe-by-construction for the egress POD today**; the missing piece
  is the *classification discipline* (nothing stops a future unclassified field). **CP2 not done**
  (no `controlplane::Egress`/`Ingress`/`TelemetryFrame` ref in `main.cpp`) → only egress is local
  stdout/`--csv`, zero network; F3b satisfied **vacuously** (the Ingress stub has no transport/
  dispatch/auth). **No `controlplane_security_audit` script** (grep = 0). No `--allow-host-data`/
  Origin/auth/handshake anywhere (grep = 0) — the entire ingress-security surface is MISSING.
- **The strategy (full HOW + the CP-CR1..6 risk table in the annexes):** (1) field-sensitivity
  classification in the serializer (`Ndjson.hpp serialize_frame:77`) — a `Sensitivity{Public,
  HostIdentifying,Secret}` tag; progress metric = schema fields lacking a tag → 0; (2)
  egress/ingress topology split (F3b-L1) — the transport MUST be constructible with NO command-ring
  (the default build links no network ingress); (3) the egress-inventory doc; (4) WebSocket
  hardening when it ships — local-only bind + server-side Origin allowlist on every handshake +
  auth-before-every-mutating-command + a closed-enum command vocabulary (never a file-write/path/
  spawn primitive); (5) the three-part `controlplane_security_audit` CI harness registered in
  `.github/workflows/ci-linux.yml` + the pre-commit hook (mirroring `lint_hal`/`check_inventory`).
- **F3b-L3 risks CP-CR1..6 → triad RISK_REGISTER** (all `designed`): CR-CP1 unauthenticated ingress
  (RCE class, type-level `Authenticated<ControlCommand>` wrapper); CR-CP2 CSWSH (Origin allowlist);
  CR-CP3 `network-bind + auth-disabled` constructible (the type-level mutual-exclusion = audit part
  C, two views of one invariant); CR-CP4 hand-rolled pairing crypto (use a vetted handshake, not
  bespoke PIN+salt); CR-CP5 dangerous command vocabulary (closed enum only); CR-CP6 listener↔ring
  race (reuse the lock-free `transport::Ring`). Cross-link to
  `FG_CONTROLPLANE_SECURITY_PRIOR_ART` (the S-numbered prior art each mitigation cites) and
  `CONTROLPLANE_MASTER_PLAN §6` (which *defers* exactly this surface — these rows un-defer it). Do
  **not** merge with `DEVICE_LOST_RECOVERY` (orthogonal crash class) — cross-link only.

### Family G — CONSUMPTION / UX

**G1 — two-click, set-once, one-knob (per-game profiles + quality↔latency slider + CP2 wiring).**
*(COMPACT; FIXED-POINT C9; T1; → indexes `PHYRIADFG_UI_MASTER_PLAN` + `*_IMPLEMENTATION_STRATEGIES`,
`CONTROLPLANE_MASTER_PLAN` CP2 `:89-90`.)* **★ Closeable (C9):** a user sets ONE target-fps knob, FG
holds it across a session with **default flags only**, no per-game fiddling — clean-run-evidenced
(a fixed point, "no floor — purely addressable"). **★ Frontend = Tauri + React (fix #9 —
operator's 2026-06-21 decision, `CONTROLPLANE_MASTER_PLAN.md:18`).
`PHYRIADFG_UI_MASTER_PLAN.md:49` is STALE (ImGui/GLFW, pre-pivot) and MUST be updated; do NOT write
ImGui here.** Standing: the slider **mechanism is BUILT** (`--flow-scale auto`
`main.cpp:1194-1205, 3072-3076`; clamp `[0.25,12]` at `:1122`; shipped 2.1 MP target at `:856`) —
`built` for the mechanism, `designed` for the breakpoint mapping; the **UI is MISSING** (no code);
**CP2 is MISSING** (no `controlplane` ref in `main.cpp`) on a `built` pillar; **per-game profiles
MISSING**. The launcher composes argv + spawns `render_assistant.exe` (no FG hot-path touch); the
**one FG-process change is CP2 wiring** — add a `controlplane::Egress` + a `TelemetryFrame` fill
(`TelemetryFrame` IS the `--csv` `CsvRow` re-packed as a wire-stable POD) adjacent to the
`telemetry_csv.hpp` emission, gated behind a default-off `--cp-egress`; acceptance = parity with the
`--csv` fields + byte-identical egress-off. Sequence P1 (one-knob) before P6 (frontend). **Risk
R-G1-1 → triad RISK_REGISTER** (concurrency: the present-thread egress push — use the pillar's
lock-free `Egress` + cold off-thread drain; reconcile with, do not duplicate,
`CONTROLPLANE_MASTER_PLAN §7`).

### Family H — MEASUREMENT / HONESTY

**H1 — reproducible FG-quality measurement (wire scorer into CI = C8; calibrate vs BVI-VFI).**
*(COMPACT; FIXED-POINT C8 + a calibration ASYMPTOTE; → indexes `FG_MEASUREMENT_METHODOLOGY_PRIOR_ART
§1`, the scorer `README.md`, `.github/workflows/bench-regression.yml`.)* **★ Closeable (C8):** a
**deliberate quality regression turns CI red** (binary once X is operator-set). **PLUS an ASYMPTOTE
(calibration):** SROCC of `flowdsc` vs BVI-VFI human MOS, floor 0.70 / ceiling ~0.83. Standing: the
instrument is **BUILT** (`fg_quality_scorer/main.cpp` + `CMakeLists.txt` + `README.md`; metrics
`dbl_edg_m`/`flowdsc`); CI wiring **MISSING** (standalone; grep in `.github/workflows` = 0);
calibration **MISSING**. **★ HEAD-vs-vista drift (fix #6):** `fg_quality_scorer` needs **Vulkan +
GPU** (`CMakeLists.txt:14 find_package(Vulkan REQUIRED)`) → GitHub-hosted runners have no GPU, so
the C8 gate MUST be a **self-hosted / local GPU gate** (the `bench-regression.yml:44-45` precedent),
NOT a GitHub-hosted runner; the vista's "turns CI red" wording assumes a GPU runner that does not
exist on hosted CI. Clone the `bench-regression.yml` pattern (a pinned held-out corpus via
`prep_zoo_sequence.py`, a `FGQ_BASELINE.json`, a `scripts/check_fgq_regression.ps1` comparator);
the deliberate-regression red run crosses C8. The calibration arm needs the external BVI-VFI MOS
labels (an access dependency, not a code gap). **No RISK row** (offline read-only tool).

**H2 — honest head-to-head vs LSFG (PresentMon latency + pacing = C7; camera/NR rig = asymptote).**
*(COMPACT; FIXED-POINT C7 + a quality ASYMPTOTE; → indexes `FG_MEASUREMENT_METHODOLOGY_PRIOR_ART
§1-2`, the C7 caveat, `BENCHMARK_FAIRNESS.md`.)* **★ Closeable (C7):** a committed PresentMon CSV
pair shows our **All-Input-to-Photon** latency ≤ LSFG + Δ **AND** `MsBetweenDisplayChange` p99
jitter ≤ LSFG's, same BF6 scene, no camera (binary once Δ is operator-set). **PLUS ASYMPTOTE:** the
camera/NR quality verdict = a per-regime win/match/lose scorecard (~0.7 NR ceiling;
Animation-Error cannot score fake frames). T0 for the latency arm (PresentMon is external, zero FG
code); T1 for the camera/NR rig.

> **★ DEDUP + the C7 probe home (fix #7c):** the **C7 PresentMon-attribution probe is ONE item,
> homed HERE under H2/C7**; B1 and C1 cite it. **It is load-bearing and gates C7:** whether
> PresentMon "All-Input-to-Photon" attributes latency correctly through our third-party DComp
> overlay is **unconfirmed** (dossier — "metric exists; not confirmed for an external overlay
> path"). PhyriadFG presents via a DirectComposition overlay HWND, so the probe MUST be C7's first
> step; the named fallback is AMD **FLM** (software, no camera, framebuffer-not-photons) or **LDAT**
> (hardware, FG-agnostic). Record which instrument survived the probe. **No RISK row** (external
> instrument / offline capture; the probe is an assumption-to-validate, not a register failure mode).

---

## §4 — RISK INDEX (pointer)

The consolidated risk index lives in [`PHYRIADFG_RISK_REGISTER.md`](PHYRIADFG_RISK_REGISTER.md). It
holds: the NEW rows owned by this triad (A3 R-A3-1..3; the merged **P5/E1a≡D1** `GpuDescriptor`-POD
rows R-D1-1≡R-E1a-1 / R-D1-2≡R-E1a-2 / R-D1-3≡R-E1a-3; D2 R-D2-1..4; the **E2-PRESENT** "displayable
DComp surface" probe; F3b-L3 CP-CR1..6; G1 R-G1-1) plus an index of the four existing per-feature
registers it CITES (does not duplicate): `FG_SATURATION_STABILITY_RISK_REGISTER` (CR1-CR9),
`SINGLE_GPU_COLLAPSE_RISK_REGISTER` (discharged), `DEVICE_LOST_RECOVERY_RISK_REGISTER` (discharged),
`REAL_FAST_PATH_RISK_REGISTER`. **T2 items requiring all risks `mitigated`/`accepted` before any
commit:** P3 (have, register discharged), P5/E1a/D1 (NEW), D2 (NEW), A3·L4 (NEW), F3b-L3 (NEW),
C1·STEP4 (have, `open`).
