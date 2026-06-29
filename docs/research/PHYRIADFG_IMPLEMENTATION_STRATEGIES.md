# PHYRIADFG_IMPLEMENTATION_STRATEGIES — the HOW of the 9-family plan

> **Diátaxis type:** How-to / strategy (the implementation sibling of
> [`PHYRIADFG_MASTER_PLAN.md`](PHYRIADFG_MASTER_PLAN.md) — the *what* — and of
> [`PHYRIADFG_RISK_REGISTER.md`](PHYRIADFG_RISK_REGISTER.md) — the crash/concurrency/POD/
> device-loss/RCE/dogma risks). The three are bound to ONE anti-drift spine (§0, shared
> verbatim across the triad). This file holds the *per-objective build strategy* (code sites,
> byte-identical-off discipline, build/test/gate) + the dependency-ordered BUILD WAVE plan.
>
> **Status:** `designed`. This is PLANNING — implementation is FROZEN. The triad
> operationalizes [`PHYRIADFG_OBJECTIVE_VISTA.md`](PHYRIADFG_OBJECTIVE_VISTA.md) (the
> constitution: families A1–H2, the C1–C10 closeable fixed points, the FIXED-POINT-vs-ASYMPTOTE
> rule). For COVERED objectives this file is a thin pointer into the existing per-feature
> strategies docs; for the NEW families (A3, D2, E1, E2, F1, F3) it carries the full strategy.
>
> **Plan-tier (PLAN_TIER_PROTOCOL):** **Tier-2** — it contains T2 objectives (P3 async-flip,
> P5/E1a/D1 router, D2 instances, A3·L4 FP16 present, F3b-L3 mobile pairing, C1·STEP4
> make-space). The RISK_REGISTER is therefore mandatory; **no T2 step is committed while any of
> its risks is `open`.** Each T2 risk home is the triad
> [`PHYRIADFG_RISK_REGISTER.md`](PHYRIADFG_RISK_REGISTER.md).
>
> **Verifiable-reference mandate (FDP).** Every `file:line` below was grepped first-hand by the
> cluster authors against HEAD `8664445`; they are copied **verbatim** from the audited cluster
> plans and MUST be re-grepped before any edit (lines drift every session — DRIFT-1/2/3 in §0
> are the proof). No benchmark is minted here; numbers cite the dossiers or
> `docs/planning/BENCHMARK_FAIRNESS.md`.

---

## §0 — The binding spine (the anti-drift invariant — all three triad docs hold this VERBATIM)

> **This triad operationalizes `PHYRIADFG_OBJECTIVE_VISTA.md` (the 9-family objective constitution A1–H2 + the C1–C10 closeable fixed points + the FIXED-POINT-vs-ASYMPTOTE rule) into a buildable, dependency-ordered, risk-bearing plan.** It sits BELOW the vista (which says *what*) and ABOVE the per-feature dossiers + the existing per-feature plans (which own the *SOTA* and the *per-gap how*). It is the UNIFYING layer: it INDEXES the dossiers (`FG_*_PRIOR_ART`), the architecture plan (`FG_ARCHITECTURE_DCAD_MASTER_PLAN`), the per-feature triads (`FG_SATURATION_STABILITY_*`, `SINGLE_GPU_COLLAPSE_RISK_REGISTER`, `DEVICE_LOST_RECOVERY_RISK_REGISTER`, `REAL_FAST_PATH_*`, `INPUT_LAG_DREDUCTION_MASTER_PLAN`, `CONTROLPLANE_MASTER_PLAN`, `PHYRIADFG_UI_MASTER_PLAN`), and the cross-dimension `PHYRIADFG_PERFECTION_ROADMAP` (the P0–P7/S1–S6 sequence + the §5 dependency spine + the §10 honest floors); it re-derives none of them. Where the roadmap and the vista disagree with first-hand HEAD, **HEAD wins and the divergence is logged in §6 (CONFLICTS/DEDUP).**
>
> **Binding invariants (every objective, every wave, every commit MUST uphold — non-negotiable):**
> - **Byte-identical-off.** Every new lever is a default-off flag in `parse_extra` (NOT the main parse chain — C1061); the default binary is bit-identical to current. Verified by a `--csv`/pixel diff (off-run == baseline), never asserted. The lone exceptions are explicitly measured-gated, not off-proven: A1·S2 field-quality changes to default-on consumers (`bg_snap`/`band_xfade`), and any default-flip — each gated on a committed held-out scorer win, not a byte-proof.
> - **No-cap dogma.** Nothing caps, throttles, or recommends-capping the captured game's render rate. The make-space / fractional-controller / governor work asserts the FG slice (LSFG-AFG-style target-output-fps), never L1 base-game-cap.
> - **Verify-before-claim (FDP verifiable-reference mandate).** No fabricated path/line/API/benchmark; every code site is grepped first-hand before it is edited (lines drift every session — DRIFT-1/2/3 below are proof); every cited dossier number inherits its `[V1]/[V2]/[V3]` level and is not re-minted. The single benchmark source of truth is `docs/planning/BENCHMARK_FAIRNESS.md` — committed numbers route there, never duplicated.
> - **FIXED-POINT-vs-ASYMPTOTE labelling.** Every objective declares one: a FIXED-POINT carries a *binary* closeable test (the C1–C10 crossings); an ASYMPTOTE carries a *single tracked metric + a floor* and an operator-declared "enough" — never a false "done". The triad does not promise to close an asymptote.
> - **Efficiency mandate.** Every hot-path lever is measure-gated; "if you can't measure the hot path you can't claim it's fast." Zero-overhead-off over defensive-on.
> - **Tier ceremony matches blast radius.** T0 → no plan; T1 → this MASTER_PLAN + IMPLEMENTATION_STRATEGIES; T2 → also a RISK_REGISTER, and **no T2 commit while any risk is `open`** (each `mitigated`-as-code or explicitly `accepted`).
>
> **Honesty reconciliations carried forward (HEAD `8664445`, verified this pass — these correct stale vista/roadmap/dossier text):**
> - **DRIFT-1** — the synchronous present-fence sites moved `7001/7150/7507` (roadmap §11) → **`7013/7162/7521`** at HEAD and are already `vk_live`-wrapped (the device-loss hole there is closed; the open async-poll hole is `7228/7628/7675`, `mitigated 3f6cbd2`).
> - **DRIFT-2** — the single-GPU slice (C2) is **no longer MISSING**: `measured` on the 4090 (~20–22 ms) per `SINGLE_GPU_COLLAPSE_RISK_REGISTER §2.1`; the open remainder is the 1080p/1440p/4K sweep + a small-GPU VRAM number.
> - **DRIFT-3** — the fractional controller (P1/C9, STAGE-119 `--target-output-fps`) is **BUILT** default-off (`main.cpp:7844–8428`); open = the flip + the motion-collapse validation, not the build.
> - The **control-plane pillar is built through CP0/CP1/CP3** (`b3253a7`); CP2 (FG adoption) is genuinely unwired (`controlplane` grep in `apps/render_assistant` = 0). The wire POD `TelemetryFrame` carries **no title/PID** → F3a is host-data-light by construction; near-term F3a work is *classification discipline*, not redaction.
> - **F1 WDA divergence collapsed to PARITY** (Lightshot, 2026-06-22) — LSFG is also display-affinity-excluded; the capturable-overlay lever is OPTIONAL go-beyond, not a fix.
> - **G1 frontend = Tauri + React** (CURRENT — operator's 2026-06-21 decision, confirmed at `CONTROLPLANE_MASTER_PLAN.md:18` "PhyriadFG's UI is a Tauri + React app"). `PHYRIADFG_UI_MASTER_PLAN.md:49` is STALE (still says ImGui/GLFW — pre-pivot) and **needs updating**; this triad writes G1 as Tauri/React. The vista G1(e) "Tauri/React" wording AND `ACTION_PLAN.md:66` are reconciled to Tauri/React.

---

## §1 — How to read this file

- **COVERED objectives** (A1, A2, B1, C1, C2, D1, D3, G1, H1, H2) already have a per-feature
  strategies doc or dossier. Their entry here is a **compact pointer**: the existing strategy
  document + the one or two load-bearing code sites + the byte-identical-off note. Do not
  re-derive — open the indexed doc.
- **NEW objectives** (A3, D2, E1, E2, F1, F3) have NO existing per-feature plan. Their entry is
  the **full strategy** (code sites to touch, byte-identical-off discipline, build/test/gate),
  carried from the cluster plans.
- All builds land via `G:/phyriad/build-ra-msvc2/_build_ra.bat` (MSVC/Ninja under
  `build-ra-msvc2`) unless noted. New flags go in `parse_extra` (NOT the main parse chain —
  C1061). New atomics use default `seq_cst` (`lint_hal` gate). Any new file → `INVENTORY.md` in
  the same commit; any new formal doc → `docs/FORMAL_DOCUMENTS_REGISTER.md`.

---

## FAMILY A — perceptual quality

### A1 (COVERED — DCAD §4 + dossiers; no per-feature plan, indexes the DCAD)
A1 is a cluster of four roadmap items, ASYMPTOTE (tracked by `flowdsc`, floor ~0.83 SROCC).

- **A1·P2 — selection-stability hysteresis (T0→T1).** Strategy: extend the inertia persistence
  counter (STAGE-57, `main.cpp:499`) and the `u_persistence` R8 sampler (`wap_warp.comp:220`) to
  gate the *candidate PICK*, not just MV-adoption; new term `pick_new = (confidence_new −
  confidence_held > hyst_margin) ? new : held` in the multicand block fed by `mc_disp`/`mc_edge`
  (`main.cpp:142-143`). Flag `--pick-hysteresis F` (default 0 = OFF, short-circuits per the
  STAGE-56 `>0.0` pattern, `wap_warp.comp:170` → byte-identical). Gate: operator-eye (vibration)
  + `--qdump` → `fg_quality_scorer` flip-rate ↓, no `dbl_edg_m` regression. The shader names the
  empty slot itself: `wap_warp.comp:178`. → indexes FG_VFI_PRIOR_ART §11 + roadmap P2.
- **A1·P4 — disocclusion arbiter (T1).** Strategy: add a one-pass fwd/bwd flow-CONSISTENCY
  scatter (range-map occupancy from the existing MV field — **no second flow dispatch**); feed it
  as a new binding into `wap_warp.comp` alongside the bidir machinery (`wap_warp.comp:35`/`:279`);
  promote STAGE-116 `--disoccl-hardpick` (`main.cpp:144`, shader `:645`) to consume the clean map.
  **CROSS-CUTTING SAFETY FIX (shared with S5):** `--camera-twarp` leads by the full 6-param affine
  `gme_model_mv(uv)` (`wap_warp.comp:738-743`, applied `:770-773`, fit `main.cpp:1572-1608`) —
  restrict the lead to the rotational component or relabel with its judder ceiling before any
  default-on. Byte-identical-off behind the default-0 hardpick flag + the new map's own flag.
  Gate: `--qdump` held-out triples + scorer reveal-band win. **Unblocks S5.** → indexes
  FG_OPEN_ALTERNATIVES_PRIOR_ART §4–§5, DCAD §4.
- **A1·S2 — edge-gate promotion (T1).** Strategy: promote `apps/render_assistant/shaders/igpu_field.comp`
  from luma-Sobel to a chamfer-distance/occlusion-class producer; the four consumers already read
  binding-11 (`main.cpp:130/137/144`) so the binding contract is stable. NOT byte-identical
  (bg_snap/band_xfade are default-on consumers) → each consumer flip gated on a measured held-out
  scorer win (roadmap §9). → indexes FG_VFI §10/§11.
- **A1·S4 — neural-on-idle-4090 (T1, research).** **Deferred — needs its own dedicated SOTA + P4
  + P5 first.** No strategy authored; recorded as a downstream research node only.

### A2 (COVERED — DCAD §5 PACING + FG_CADENCE_LATENCY_PRIOR_ART; no per-feature plan)
- **A2·P0 — DWM-metered pacer (T0→T1).** **★ This is ONE item with three names: A2·P0 ≡
  C1·STEP1 ≡ roadmap P0 (the DWM clock). It is HOMED HERE under A2/C6; B1 and C1 CITE it, they do
  not re-implement it.** Strategy: in `paced_wait_P` (`main.cpp:6941`) and the caller's target
  computation (`main.cpp:7871`, the `tgt=tick_t0+tick_k*tick_period_ms` free-running grid) add a
  `DwmGetCompositionTimingInfo` call and anchor the target to `qpcVBlank + n·qpcRefreshPeriod` +
  one EWMA frame-time term. **Critical:** fold the metering into the present-pacer anchor, NOT the
  content PLL (`T_robust`) — a PLL slew re-introduces the STAGE-93 lurch. Compose with the
  `pace_variance` SMA10 substrate (STAGE-117, `main.cpp:145`); keep the `hal::cpu_wait_for_ns`
  spin-finish (`main.cpp:6947`). Flag `--dwm-pace` (default OFF → `main.cpp:7871` unchanged,
  byte-identical). **DROP (do not re-propose):** VRR/integer-N pacing — an external DComp overlay
  cannot drive monitor VRR (FG_CADENCE_LATENCY §1 verdict 2). Gate: PresentMon
  `MsBetweenDisplayChange` p99 jitter (C6 evidence) + operator-eye boundary-pulse. → indexes
  FG_CADENCE_LATENCY_PRIOR_ART §1/§4, DCAD §5.

---

### A3 (NEW — full strategy; dossier FG_HDR_FORMAT_PRIOR_ART, no implementation plan)

A3 = HDR/format completeness, FIXED-POINT C4. Five ordered levers. Honest floors: internal
scRGB-FP16 *arithmetic* is foreclosed (NVIDIA/Intel refuse it; Pascal 1080Ti = 1/64 FP32) → the
reachable path is FP16 *ingest+storage+present* with quantized interp math. **10bpc HDR10
(Option 2) is BLOCKED for this surface** — MS excludes alpha-blended swapchains and the DComp
overlay is `DXGI_ALPHA_MODE_PREMULTIPLIED` (`PresentSurface.cpp:222`, confirmed first-hand) → the
only HDR present path is Option 1 (FP16 scRGB, 64 bpp). The "10bpc is the cheap target" prescription
is REJECTED for this surface.

Current standing (first-hand): an 8-bit SDR pipeline that recognizes HDR and hard-clips it.
`route_for()` (`main.cpp:1778-1784`) maps `R16G16B16A16_FLOAT→RT_HDR`; `IS_HDR=(route==RT_HDR)`
(`main.cpp:3058`). WGC ingest hard-locked BGRA8 at `main.cpp:3053`. **Hard clip:
`hdr_convert.comp:32` `c = min(c, vec3(1.0));` inside the `pc.is_hdr != 0u` branch.** Present
BGRA8 with no colorspace: `sd.Format = DXGI_FORMAT_B8G8R8A8_UNORM` (`PresentSurface.cpp:206`),
zero `SetColorSpace1`/`SetHDRMetaData` in the present pillar; swapchain stored as
`IDXGISwapChain1*` (`PresentSurface.cpp:102`).

- **A3·L1 — `SetColorSpace1` on present (T0).** Strategy: in
  `framework/render/present/src/PresentSurface.cpp`, `QueryInterface(__uuidof(IDXGISwapChain3),…)`
  on `impl->sc` (the same QI pattern at `:126,:135,:240`); query the display colorspace via
  `IDXGIOutput6::GetDesc1().ColorSpace`; `CheckColorSpaceSupport` then `SetColorSpace1` after
  `CreateSwapChainForComposition` (`:224`). Flag `--present-colorspace auto` (default off/no-op →
  byte-identical on non-Advanced-Color desktops). Touches a published pillar → a doc-sync of
  `docs/reference/<pillar>.md` may be owed (CI `check_doc_sync.sh`). Test: operator-eye on an
  Advanced-Color desktop (washout gone) + the swapchain colorspace reported (A3-2 check).
- **A3·L2 — parameterize WGC ingest format (T1, precondition for A3-1).** Strategy: at
  `main.cpp:3053` (the BGRA8 force), detect HDR via `IDXGIOutput6` and set
  `d.fmt=DXGI_FORMAT_R16G16B16A16_FLOAT` when HDR (the `Direct3D11CaptureFramePool.Create`
  `DirectXPixelFormat` is a caller choice). `route_for`/`IS_HDR` already drive the
  `hdr_convert.comp` `is_hdr` push-constant (`main.cpp:5143,5172`). FP16 sampled-to-FP32 carries
  no Pascal 1/64 penalty (only FP16 *arithmetic* does). **FLAG (`[V2]`, unverified):** "WGC accepts
  R16F" — the build agent MUST verify the `Create` call succeeds first-hand on the rig.
  Byte-identical-off: default keeps BGRA8 unless HDR detected + flag opts in. **Test-instrument
  gap (FLAG):** `--qdump`/`dump_rgba` (`main.cpp:1770`) writes raw RGBA8 no-header
  (`main.cpp:1765`) — it cannot represent >1.0 FP16; A3-1's histogram test needs an FP16-aware
  dump path (or a pre-clip FP16 warp-input buffer tap). Add one. Test: A3-1 `--qdump` on a real
  HDR game showing >1.0 scRGB pre-tonemap.
- **A3·L3 — replace the hard clip with rolloff + dither (T1, the A3-1 binary).** Strategy: at
  `hdr_convert.comp:32` replace `c = min(c, vec3(1.0));` with a Reinhard/ACES/BT.2390 rolloff +
  ordered dither before the `srgb_oetf` + 8-bit `imageStore` (`:33-34`). **Mirror in
  `igpu_convert_pack.comp`** (the iGPU DD-HDR path). Reference-white: scRGB(1,1,1)=80 nits, adjust
  SDR by `SdrWhiteLevelInNits/80` via `DISPLAYCONFIG_SDR_WHITE_LEVEL`/`QueryDisplayConfig`.
  Byte-identical-off behind a flag (default = current clip). Depends on L2. Test: A3-1 histogram
  (no 255 pile-up) via the FP16-aware dump.
- **A3·L4 — true HDR present: FP16 scRGB, NOT 10bpc (T2 — the only T2 in A3).** Strategy:
  `PresentSurface.cpp:206` (`B8G8R8A8_UNORM → R16G16B16A16_FLOAT` for the HDR path); backbuffer at
  `:240`; the bridge images at `main.cpp:4707,4723,4752,4765` (all `B8G8R8A8_UNORM` → FP16 for the
  HDR path); the `Present(0,0)` at `:285`. Default scRGB is treated as `RGB_FULL_G10_NONE_P709`;
  A3-2's check (swapchain reports HDR colorspace) is best satisfied by an explicit set. T2 because
  it touches the present pillar's published surface + the cross-process D3D11↔Vulkan shared-texture
  bridge (format mismatch = device-loss/corruption). **Byte-identical-off:** opt-in, default
  BGRA8; a committed `--csv` slice comparison BGRA8-vs-FP16 must clear before any default flip
  (64 bpp doubles bridge bandwidth — measure-gate). Risks **R-A3-1/2/3** → triad RISK_REGISTER.
- **A3·L5 — linear-light compositing for blends (T1, measure-gated).** Strategy: decode
  sRGB→linear before matte/crossfade/disocclusion blends in `wap_warp.comp`, re-encode after.
  Hot-path → gate on measurement. Lowest priority; sequence after the ingest/present ladder. An
  A1↔A3 cross-cutting correctness item (improves disocclusion blends too).

A3 dependency ladder: L1 (T0) ∥ L2 (T1) → L3 (T1) → L4 (T2); L5 (T1) cross-cuts A1.

---

## FAMILY B — latency / feel

### B1 (COVERED — INPUT_LAG_DREDUCTION + REAL_FAST_PATH strategies)
- **B1 strategy pointer.** The space-makers are SHIPPED — no new build. → indexes
  [`INPUT_LAG_DREDUCTION_MASTER_PLAN.md`](INPUT_LAG_DREDUCTION_MASTER_PLAN.md) (the D-reduction arc
  + §6 measured levers + §7 "latency is a budget" + §8 combat decomposition — the live floor is
  the ~11 ms per-pair B-side build gap, NOT F-compute) and
  [`REAL_FAST_PATH_IMPLEMENTATION_STRATEGIES.md`](REAL_FAST_PATH_IMPLEMENTATION_STRATEGIES.md) +
  [`REAL_FAST_PATH_RISK_REGISTER.md`](REAL_FAST_PATH_RISK_REGISTER.md). Code sites:
  `main.cpp:608` (`--rfp-fresh`), `:631` (`--cphase` auto-async), `:720` (`--async-present`,
  STAGE-102 second bridge slot `:3400`), `:731-741` (`--shallow-queue`).
- **B1·C7 — latency ≤ LSFG + Δ (T0, FG-agnostic). ★ This CITES the C7 probe HOMED under
  H2/C7 (the PresentMon-through-DComp attribution probe is ONE item — see H2).** Procedure
  (single-sourced under H2): same BF6 scene, capture PresentMon "All Input to Photon" for
  PhyriadFG-on and LSFG-on, commit the CSV pair. **PROBE-GATE FIRST** (cross-check vs the internal
  `added_lat_ms` `--csv` field; divergence → escalate to H2's camera rig). Non-register; C7
  metric-validity is a correctness risk mitigated by the probe, `designed`.
- **B1·async-present.** **★ The `--async-present` default-flip is ONE action HOMED under
  C1·STEP3; B1 inherits the latency win (measured 73→32.6 ms in BF6). Do not duplicate.** Its T2
  risk surface is fully in `FG_SATURATION_STABILITY_RISK_REGISTER` (CR1–CR5).
- **B1·S5 — extrapolation (T1, research, out of cluster).** HARD-gated behind P4's clean
  disocclusion fill; named as a dependency only.

---

## FAMILY C — performance / headroom

### C1 (COVERED — the FG_SATURATION_STABILITY triad IS this objective)
- **C1 strategy pointer.** Reuse
  [`FG_SATURATION_STABILITY_IMPLEMENTATION_STRATEGIES.md`](FG_SATURATION_STABILITY_IMPLEMENTATION_STRATEGIES.md)
  S0–S5 verbatim (cited, not duplicated) + the MASTER_PLAN STEP 0–4 + the RISK_REGISTER CR1–CR9 +
  the INTEGRATION (the why). The synchronous present (the C1 target) is at
  **`main.cpp:7013, 7162, 7521`** — `vk_live(vkWaitForFences(A.dev,1,&fBridge,VK_TRUE,UINT64_MAX))`
  (DRIFT-1: moved from the stale roadmap `7001/7150/7507`, and already `vk_live`-wrapped; the open
  async-poll hole is `7228/7628/7675`, `mitigated 3f6cbd2`). STEP-2 controller is BUILT at
  `main.cpp:7844-8428` (`--s2-sustain` `:149/:1089`). The unifying build sequence:
  1. **STEP 0 keystone** — `SimpleMovingAverage<10>` of present deltas, `target = avg/realized_mult
     − varianceFactor·stddev − safetyMargin`, reset on >100 ms; site `main.cpp:7831-7838` + KEEP the
     `paced_wait_P` spin-finish (`main.cpp:6931-6940`); FSR3 pitfall: `getVariance()` already
     returns the stddev — apply `varianceFactor·stddev` once.
  2. **Flip `--async-present`** (STEP-3 register discharged bar the operator eye + a `--validation`
     `VK_EXT_debug_utils` messenger). **★ This is the ONE async-flip action; B1 inherits it.**
  3. **Validate STEP 2 in its target regime** (sustained motion — closes C9).
  - Byte-identical-off per the strategies doc (`--csv` diff, `lint_hal` clean, `parse_extra`).
    Gate: BF6 max / 4090 99% + `--csv` → **MASD + absolute present-interval stddev toward <4 ms,
    mean reported alongside (NOT CoV — CoV inverts under a mean shift), frz not up, watchdog 0.**
  - STEP 4 (make-space router, T2) — risks CR6–CR9 `open`, operator-gated, last; see the saturation
    RISK_REGISTER. **★ STEP 1 (the DWM clock) is the SAME item as A2·P0 — see A2; C1 cites it.**

### C2 (COVERED — SINGLE_GPU_COLLAPSE_RISK_REGISTER §2.1/§3 + DCAD Phase 1)
- **C2 fidelity (do not flatten).** The single-GPU FG CAPABILITY is **CROSSED** — the 4090 slice
  is `measured` ~20–22 ms (`SINGLE_GPU_COLLAPSE_RISK_REGISTER §2.1`). **OPEN:** the
  1080p/1440p/4K SWEEP + the small-GPU VRAM number. Never "C2 done"; never "C2 missing".
- **★ C2 slice-measurement procedure — SINGLE-SOURCED HERE (fix 10). D1's strategy CITES this,
  it does not re-state it.**
  - Instrument (shipped): `--latency-trace` (STAGE-112, `main.cpp:161, 1117`, emit `:8733-8740`),
    the single-GPU A-convert `c_conv_us` fix (`main.cpp:5152-5162, 5196-5197`), and the `--csv` row
    carrying `warp_ms / ms_in_present_api / iter_ms / added_lat_ms / gme_dis_pct`.
  - **The procedure:** `--force-single-gpu --latency-trace --csv slice_<res>` (resolution is fixed
    at startup from the captured window → 1080p / 1440p / 4K via the captured-window size, zero code
    change), each run ≥30 s on the 4090. Commit the three CSVs + the derived
    `[fg-slice] = conv + compute(flow+gme) + warp + present`. **This is the single cheapest
    "avanzar" available** — no hardware beyond the author's rig, no eye, no flip.
  - The small-GPU VRAM number (CR8) is **hardware-blocked, not a code task** — record as
    measured-when-available; do NOT manufacture a number.
  - Byte-identical-off: N/A — `--force-single-gpu`/`--latency-trace`/`--csv` are opt-in/measurement-only;
    the default path is byte-identical (SINGLE_GPU §0/§2.1). No new T2 surface (the enablement risks
    CR1–CR9 are already discharged). Gate: three committed CSVs → C2 CROSSED-for-this-rig, honest
    small-GPU-VRAM caveat noted.

---

## FAMILY D — scalability / multi-GPU

### D1 (COVERED — DCAD §2/§3/§5/§8 + GPU_MULTI_GPU_PRIOR_ART; no per-feature plan, indexes DCAD)
- **D1 strategy pointer (T2).** **★ D1 is a CONSUMER of the ONE wired router (P5); E1a is the
  other consumer. P5 is presented once (see E1a); D1 is the A/B MEASUREMENT half.** Four steps,
  indexing DCAD STEP A–D:
  1. POD change — add the fp16/DP4a field to `GpuDescriptor` (`GpuDescriptor.hpp`; struct is 128 B
     with `_pad1[5]` + `static_assert(sizeof==128u)`). **This is the SAME pillar change E1a needs —
     ONE risk row R-D1-1 ≡ R-E1a-1 in the triad RISK_REGISTER.** Operator-approval-gated (DCAD §9.6).
  2. CHARACTERIZE at startup — one-time fp16/DP4a probe + reuse `measure_transfer_bw`, off the hot
     path; persist + invalidate-on-device/driver-change.
  3. Replace the hardcoded selection at `main.cpp:3154-3180` (the deviceType/LUID hardcodes + the
     `single_gpu` ternary at `:3180`) with `select_participants(cat, n, arithmetic_intensity)`
     (FG's AI ≈ 2.5). Keep `--force-single-gpu` + the LUID/deviceType DETECT as inventory; make
     ASSIGN the decision-maker. LOG the decision (DCAD §5 STEP D).
  4. The A/B measurement (the binary) — **CITES the C2 procedure above** (`--force-single-gpu
     --latency-trace --csv`): capture single-GPU vs multi-GPU on the same scene; the slice +
     latency columns are the CSV pair (criterion D1.f, multi ≤ single).
  - Byte-identical-off: uncharacterized catalog → today's exact placement (RoutingPolicy.hpp:19-21
     "uncharacterized → supervisor only"); on the author's 4090+1080Ti the router MUST reproduce
     the current split (diff the chosen assignment vs the `main.cpp:3180` ternary). Gate: the
     committed CSV pair + the router decision log showing a *measured* engage/decline. Risks
     R-D1-1/2/3 → triad RISK_REGISTER (merged with E1a). **FLAG:** `arithmetic_intensity` for the
     FG work-item is not computed in `main.cpp` today — derive/measure when P5 wires the router.
     → indexes DCAD §2/§3-T2/§5/§8, GPU_MULTI_GPU_PRIOR_ART.

---

### D2 (NEW — full strategy, the headline differentiator; roadmap S1, no existing plan)

D2 = parallel FG across windows/screens, focus-independent — the operator's #4 objective; **LSFG
structurally cannot follow** (its multi-frame-gen is stacked passes on ONE target). FIXED-POINT
(vista D2.f): two games on two monitors, each its own FG instance, ≥60 s correct + no
cross-interference, two telemetry CSVs. **T2 — the cluster's biggest design gap; no existing
per-feature plan.** Depends HARD on **P5** (the wired router partitions the device set per
instance, roadmap S1:224), the topology pillar (`phyriad::hw`, consumed at `f711102` for `--pin`),
and the present-surface pillar already carrying `monitor_index`.

Current standing (first-hand): the whole app is ONE instance — ONE present thread creating ONE
`PresentSurface` (`main.cpp:6901`) with `PresentSurfaceDesc.monitor_index=cfg.pres_mon`
(`main.cpp:6906`); control flow is process-global (`g_quit`/`g_quit_threads`, e.g.
`main.cpp:6914`); threads/bridges/device handles are singletons. No per-instance lifetime/quit/
device-partition. No existing plan file matches "parallel FG instances".

**STRATEGY (the T2 design contract) — four pillars, each grounded in an existing substrate hook:**
1. **N independent role graphs.** Refactor the single-instance globals (`g_quit`, the bridge ring,
   device/queue handles, the P thread) into an **`FgInstance` struct** owning its own lifetime,
   capture target (HWND/monitor), bridge ring, command pool/queue, per-instance quit flag. The
   monolith becomes `for each target: spawn FgInstance`. **Byte-identical-off:** N=1 MUST reduce to
   today's exact single-instance flow (the regression gate).
2. **Independent present authority PER PANEL.** Each `FgInstance` creates its OWN `PresentSurface`
   with its own `monitor_index` (hook exists, `main.cpp:6906`). The pillar's hard
   create()+submit()-on-the-same-thread contract (`main.cpp:6897`) means **each instance owns its
   present thread** — one authority per panel, preserving the DCAD §2 one-present-authority
   invariant per-panel (no SLI runt-frames across panels).
3. **Multi-surface DComp.** N DComp visuals, one per panel, each WDA-excluded + click-through
   (today's defaults, `main.cpp:6917`). **FLAG (verify first-hand in `PresentSurface.cpp` before
   build):** whether N ExcludeFromCapture composition-target HWNDs flip independently without
   cross-contention — not verifiable from `main.cpp` alone.
4. **Scheduler partition.** Use `select_participants` (P5) to partition the MEASURED device set
   across instances; pin each instance's CPU threads to a disjoint CCX/core set via
   `phyriad::hw::pin_current_thread` (consumed at `f711102`) so A's F/P threads do not starve B
   (roadmap S1:252).

- **Build/test/gate:** the binary test is two real games on two monitors ≥60 s + two `--csv`
  outputs (per-instance — `--csv` is a single global path today, `main.cpp:1092`, must become
  per-instance/templated) showing no cross-interference (A's slice/latency unchanged with B
  running vs A alone). Reuse: the single-instance pipeline IS the per-instance body — D2 is
  encapsulation + N-fold instantiation, not new FG math. Risks R-D2-1..4 → triad RISK_REGISTER.

### D3 (COVERED — the spine DCAD describes; emergent, no standalone build)
- **D3 strategy pointer (T1 hygiene).** Not a standalone task — the discipline applied while doing
  D1 (one ASSIGN replaces the `main.cpp:3154-3180` hardcodes/ternaries) and D2 (the `FgInstance`
  struct collapses process-global state). Track the metric (hardcoded-topology-branch count → one
  unified path), like E1's vendor-assumption count. N=1 single-GPU all-default MUST stay
  byte-identical through the refactor. **No standalone register** — its risk-bearing parts are
  R-D1-1 (POD) and R-D2-1..4. → indexes DCAD §2/§4/§3-T(n).

---

## FAMILY E — compatibility / coverage (NEW — full strategy; no existing plan for E1/E2)

### E1 — vendor-agnosticism

- **E1a — `GpuDescriptor` fp16/DP4a field + wire the dead router (T2 — the only T2 in E1).**
  **★ E1a IS the body of P5; D1 (the A/B measurement) and E1a (the vendor-agnostic ranking) are
  the two CONSUMERS of the ONE wired router. P5 is presented once, here.** Current standing
  (first-hand): `GpuDescriptor.hpp:48-58` MEASURED block has `compute_gflops` only (no fp16/DP4a);
  `static_assert(sizeof==128u)` at `:61`, `PHYRIAD_ASSERT_POD` at `:64`; `vendor_id` at `:30` is
  identity not capability; `_pad0[6]` (`:34`), `_pad1[5]` (`:46`). `break_even_decide` appears
  ONLY in a comment (`main.cpp:50-52`, `offload=false` inlined, zero live callers). Device
  selection branches deviceType + name-substring (`main.cpp:3154-3155`); the `!pA`/`!pB`
  hard-exit at `main.cpp:3164-3167` (`--force-single-gpu` at `:3165`). No `shaderFloat16`/
  `VK_KHR_shader_integer_dot_product` probe (grep = 0).
  - **Strategy:** (1) POD field — add `float fp16_gflops` + a 1-byte `fp16_native` discriminator
    drawn from existing pad; re-derive the 128 B layout (consume pad, OR bump to 144 B + update
    BOTH `static_assert`s + every cross-process reader + a layout-version byte). **Grep all readers
    of the SHM artifact before changing the size** (published cross-process, CANON principle 6). (2)
    CHARACTERIZE — query `VkPhysicalDeviceShaderFloat16Int8Features.shaderFloat16` +
    `VkPhysicalDeviceShaderIntegerDotProductFeatures`; run a measured fp16 GEMM/SAD micro-bench to
    fill `fp16_gflops`; reuse `measure_transfer_bw` for `h2d_bw_gbps`. (3) Wire — replace the
    `main.cpp:50-52` constant with a live `break_even_decide`; rank B-candidates by measured
    `fp16_gflops` at `main.cpp:3154-3155` not "first discrete/afrag"; keep `--force-single-gpu` +
    afrag as opt-in; LOG the decision (DCAD §5 STEP D).
  - **Byte-identical-off:** on the author's 4090+1080Ti the measured router MUST pick the same
    A=4090/B=1080Ti split → diff = 0 vs current build; new behavior only on non-author hardware.
  - **★ Success criterion = grep-zero** (the code-readiness proxy until cross-vendor hardware
    exists): deviceType/name-substring branches in the *decision* path, the NVIDIA-only break-even
    default, and `GpuDescriptor` fields that cannot represent a non-NVIDIA GPU all = 0. Run it as a
    CI lint. Risks R-E1a-1/2/3 ≡ R-D1-1/2/3 → ONE merged set of rows in the triad RISK_REGISTER.
- **E1b — fp16 as the default primitive; NVOFA/DP4a measured-gated (T1).** Strategy: add a DP4a
  int8 correlation variant of the SAD flow kernel (new `.comp`, compiled only when
  `VK_KHR_shader_integer_dot_product` is present), gated behind the E1a measured break-even; make
  fp16-vs-fp32 flow an explicit measured choice keyed on `shaderFloat16`. NVOFA already built,
  default-off, measured net-negative (`main.cpp:1123, 1864, 2119`). Byte-identical-off: the
  all-gates-fail state reproduces Tier-0 bit-for-bit (verify via the C2 byte-identical harness).
  The DP4a-on-Intel correctness is hardware-blocked → rolls into E1-VERIFY; the grep-zero check is
  the gate. Depends on E1a.
- **E1c — low-power-GPU default profile (T1).** Strategy: bind the existing flow-scale/governor/
  adaptive-ring knobs to a measured `fp16_gflops` threshold from E1a; byte-identical-equivalent on
  a strong GPU, auto-engaged below the threshold. Test on the 1080 Ti as a proxy weak GPU (partial
  verification); full verification rolls into E1-VERIFY. Depends on E1a.
- **E1-VERIFY — the cross-vendor run (C5, T0-mechanical / hardware-blocked).** No code. Define the
  run harness (`--csv` + scorer per GPU) so it is turnkey when ☐ non-author NVIDIA · ☐ AMD (RDNA2)
  · ☐ Intel (Arc/Xe) hardware appears. Until then the tracked metric IS the E1a grep-zero count.

### E2 — real-library breadth

- **E2-GRACE — graceful-fail-with-reason (T1, the highest-leverage closeable, fully binary).**
  Current standing (first-hand): capture init uses `winrt::check_hresult` (`main.cpp:3956,
  3962-3963, 3966-3967`) — throws, no reason code; no `GraphicsCaptureSession.IsSupported()`
  preflight; `DuplicateOutput` at `main.cpp:1820` checked inline but silent on the hybrid case; no
  zero-luma→`PROTECTED_CONTENT` detect; WGC pool-full drops are silent by the code's own comment
  (`main.cpp:3977-3978`). **Strategy:** add a pre-flight probe + a FIXED reason-code enum BEFORE
  capture init — `GraphicsCaptureSession.IsSupported()`→`CAPTURE_UNSUPPORTED`; build query→
  backend-caveat; exclusive-FS→`EXCLUSIVE_FULLSCREEN_SWITCH_TO_BORDERLESS`; first-frame zero-luma→
  `PROTECTED_CONTENT`; `DuplicateOutput1`→`DXGI_ERROR_UNSUPPORTED`→`HYBRID_DD_WRONG_GPU`; MPO/iflip
  probe→`VRR_WILL_BE_DISABLED`; AC-sensitive title→`ANTICHEAT_UNVERIFIED` (from the trust dossier —
  the wording MUST advise, not assert safety). Replace the bare `check_hresult` at
  `main.cpp:3956/3962/3966` with a try/catch mapping the HRESULT to a code; surface the WGC silent
  pool-drop (`:3977`) as a counted `CAPTURE_BACKPRESSURE_DROP` telemetry field.
  - Byte-identical-off: the SUCCESS path adds only logging (diff = 0 on BF6); new behavior is on
    the FAILURE path only. Gate: the operator triggers each non-hardware-blocked cell (protected-
    content video → `PROTECTED_CONTENT`; exclusive-FS → the switch code; the hybrid case is
    unit-testable with a stubbed HRESULT) → every non-`works` cell emits its specific code, zero
    generic failures. **T1 — no RISK block** (pre-hot-path, additive on success).
- **E2-MATRIX — the coverage matrix as a living artifact (T0).** Strategy: create the matrix as a
  tracked doc under `docs/`, seeded from the dossier's expected-verdict table; bind each cell to a
  reproducible `--csv`/scorer run + the E2-GRACE reason code; register it in
  `docs/FORMAL_DOCUMENTS_REGISTER.md`. No build; the gate is zero `broken` cells. Depends on
  E2-GRACE. (The load-bearing axis is windowing-mode + content-protection + anti-cheat, not render
  API — the 4 render APIs collapse to the same WGC path.)
- **E2-CAP / E2-PRESENT / E2-MOAT (T1 + one T2 probe).** Current standing (first-hand): WGC
  (`main.cpp:3952+`) + legacy DD (`:1820`); no GDI rung (grep = 0); capture API is a flag
  `--capture-api` (`:1165`), no per-OS auto-selector; `DirtyRegionMode` unused (grep = 0); present
  is the DComp overlay (`main.cpp:6917`) paced to the internal `refresh_hz` NCO; `submit_at()`
  returns Unavailable.
  - **E2-CAP (T1):** add a GDI `BitBlt` last-resort backend (a rung below DD); a per-Windows-build
    selector (DXGI best pacing / WGC best compat / GDI last resort), flag-overridable; wire
    `DirtyRegionMode` for source-cadence detection. Byte-identical-off: on the author's Win 11 rig
    the selector picks WGC (current default) → diff = 0.
  - **E2-PRESENT (T1 + one T2 probe):** the DWM-compose-QPC pacing is **A2·P0** (see A2 — cite, do
    not re-implement). The VRR-coexistence config recipe (LSFG-2.0-style, base-cap = VRR-cap ÷
    multiplier) is config + docs (T1). **★ The T2 "displayable DComp surface" independent-flip/MPO
    probe touches the present path (`submit_at`/`SetTargetTime`) → device-loss-adjacent; its risk
    home = the triad PHYRIADFG_RISK_REGISTER** (fix 3 — NOT a "DCAD P5 RISK_REGISTER", which does
    not exist). It MUST inherit the `VK_ERROR_DEVICE_LOST`/present-path mitigations (cite
    DEVICE_LOST_RECOVERY + REAL_FAST_PATH).
  - **E2-MOAT:** no new code beyond E2-CAP + E2-GRACE + E2-MATRIX — the assembly of the three;
    "done enough" = zero `broken-without-explanation` cells.

---

## FAMILY F — trust / safety (NEW — full strategy; no per-feature plan)

> Material correction (first-hand, HEAD `8664445`): the control-plane pillar is **built through
> CP0/CP1/CP3** (`b3253a7`) — the dossiers' "designed, no code" is STALE. CP2 (FG adoption) is
> genuinely unwired. The wire POD `TelemetryFrame` carries no title/PID → F3a is host-data-light
> by construction.

### F1 — anti-cheat as a measured trust posture
Current standing (first-hand): WDA-exclusion at `PresentSurface.cpp:247`
(`SetWindowDisplayAffinity(impl->hwnd, WDA_EXCLUDEFROMCAPTURE)`), conditioned by
`PresentSurfaceDesc.capture` (default `ExcludeFromCapture`, `PresentSurface.hpp:60-63,72`); the
overlay fingerprint tuple at `PresentSurface.cpp:177-184` (`WS_EX_TOPMOST|WS_EX_NOACTIVATE` +
`WS_EX_NOREDIRECTIONBITMAP` + `WS_EX_LAYERED|WS_EX_TRANSPARENT` + `WS_POPUP`, game-sized
`:184-185`); title by substring `main.cpp:1839` (`GetWindowTextA`) via `EnumWindows` (`:1844`),
target `window_substr` (`main.cpp:121`, `--window` `:1168`), PID `main.cpp:6887`. No trust
matrix/gate/campaign-log in-repo. **STALE-REF FLAG (cleanup):** `PresentSurface.cpp:12` comments
"main.cpp:277-285" for the WDA call — the actual call is `PresentSurface.cpp:247`.

**STRATEGY:**
1. **Trust matrix as a living artifact (T1)** — promote the dossier §4 table into a committed,
   machine-readable file, **the same artifact as E2's compatibility matrix** (one
   `per_engine_matrix` source with `trust_class` + `survived_account_hours` + `last_ac_version`
   columns); reuse E2-GRACE's reason-code taxonomy for the UNTESTED/RECOMMEND-AGAINST classes.
2. **In-product trust gate (T1)** — at window resolution (`main.cpp:1839`/`:3049`, after
   `find_window_by_substr`), classify the title against the matrix and print a clear verdict
   (TRUSTED-with-evidence / UNTESTED-warn / RECOMMEND-AGAINST for Vanguard/Javelin) before the
   present loop; default for any unmatched title = UNTESTED-warn, never silent-allow. Cold-init
   read-side classification → byte-identical to the hot path. New `--trust-gate {warn|block|off}`
   in `parse_extra`, default `warn`.
3. **WDA-condition lever (T0, OPTIONAL)** — `--capturable-overlay` sets
   `PresentSurfaceDesc.capture = Normal` (the switch exists, `PresentSurface.cpp:247` via the desc
   field). **The WDA divergence collapsed to PARITY (Lightshot 2026-06-22)** — LSFG is also
   display-affinity-excluded → this is go-beyond, not a fix. Default-OFF.
4. **Window de-fingerprint probe (T1, null-allowed)** — investigate a DComp child-visual without
   the classic layered HWND; honest null-allowed outcome, do not promise it works.
5. **Real-rig campaign (T2, operator-gated)** — the dossier §4 5-step protocol as an operator
   runbook + a `survived_account_hours` append-log the gate reads; two arms (WDA-on vs WDA-off);
   throwaway accounts ONLY; Vanguard/Javelin stay RECOMMEND-AGAINST regardless of hours. **No
   register row — the only irreversible hazard is the account ban, mitigated by the
   throwaway-account protocol rule, not by code** (not a crash/concurrency/POD/RCE tier).
- Build via `_build_ra.bat`. Gate-on tests: the gate prints the right class for a known-title
  fixture; `--capturable-overlay` flips `PresentSurface::capture_excluded()` (`PresentSurface.hpp:128`);
  byte-identical-off measured (`--csv` unchanged, gate=off vs baseline on Honkai).

### F2 — don't destabilize the captured game (VRR/G-Sync vendor split)
Current standing (first-hand): **no VRR/G-Sync/FreeSync/AllowTearing code in `main.cpp`** (grep =
0) — F2 is today purely a documentation/honesty objective. The floor itself is structural
(NVIDIA G-Sync off for a non-hooking overlay, `[V1]`). **STRATEGY (mostly T0):** (1) vendor-
conditional honesty print at startup once the present GPU vendor is known — NVIDIA → "G-Sync
disabled while attached (shared with all external-overlay FG; use windowed G-Sync as a partial
mitigation)"; AMD/Intel → "VRR remains active" (wording from dossier §3-F2 verbatim; cold-init
print, byte-identical). (2) Windowed-G-Sync guidance doc (the only NVIDIA partial mitigation). (3)
MPO/displayable present-mode probe (T1, null-allowed on NVIDIA) — **never hook the game swapchain**
(= injection = ban). Gate: AMD/Intel = the operator reads the monitor OSD photo-evidenced; NVIDIA =
documented-limitation. **No register block** (T0/T1, no crash/RCE surface).

### F3 — telemetry / control-plane privacy & security
Current standing (first-hand): `framework/controlplane/` built through CP0/CP1/CP3 (`b3253a7`) —
`Egress` (ring+drain, `Egress.hpp:84,143`), `StdoutNdjsonTransport` (`ITransport.hpp:33`),
`Ndjson.hpp` (fields by name + a `"gpu":[...]` array `:204`), `Ingress` stub (`Ingress.hpp:38`),
`ControlCommand` (Start/Stop only, `ControlCommand.hpp:36-40`), 4 tests. **The wire POD
`TelemetryFrame` carries NO title and NO PID** (`TelemetryFrame.hpp:76-110`) → F3a's worst fields
are NOT on any wire path today (title/PID live only in the human `[ra]` line `main.cpp:6888` +
`:1839`/`:6887`; `telemetry_csv.hpp` has no Application/PID columns). **CP2 not done** (`main.cpp`
does not reference `controlplane::Egress`/`Ingress`/`TelemetryFrame`) → F3b satisfied vacuously
today. No `controlplane_security_audit` script; no `--allow-host-data`/Origin/auth/handshake/
WebSocket anywhere (grep = 0).

**STRATEGY (split by increment):**
1. **F3a-L1 field-sensitivity classification (T0, do now)** — add a `Sensitivity {Public,
   HostIdentifying, Secret}` discipline to the serializer (`Ndjson.hpp serialize_frame:77`): the
   network serializer emits only `Public` fields unless `--allow-host-data`; any title→ a hash or
   `active/idle` bool by default, any PID→ omitted. Encode the rule as a `static_assert`/review
   gate that no `HostIdentifying` field serializes without the toggle. Progress metric: schema
   fields lacking a sensitivity tag → 0.
2. **F3b-L1 egress/ingress topology split (T0, do now in CP3)** — the telemetry transport MUST be
   constructible with NO command-ring attached (read-only by construction); make the default build
   path not link a network ingress at all. The as-built `Egress`/`Ingress` are already independent
   types (separate headers/rings) — this is the cheap high-leverage move that makes F3b
   satisfiable-by-construction before any socket ships.
3. **F3a-L2 egress inventory doc (T0)** — document the egress inventory table (the one the audit
   greps) in the controlplane reference; reuse the `TelemetryFrame.hpp` field list verbatim.
4. **F3b-L2 WebSocket hardening (T1, when it ships)** — local-only bind default + server-side
   `Origin`-allowlist on every handshake (S4 — SOP does not cover loopback WebSockets) +
   auth-before-every-mutating-command per-message (S5c) + a closed-enum command vocabulary (S3/L4 —
   the as-built `CommandKind` is already closed Start/Stop). Slots into the existing `ITransport`
   seam (`ITransport.hpp:21`), no producer/drain change.
5. **The `controlplane_security_audit` CI script (the F3 fixed-point, T1)** — three parts: (A) the
   egress grep (`--allow-host-data` OFF → known title/PID/UUID/profile = 0 in a packet capture);
   (B) the unauthenticated-client harness (raw-socket + forged-Origin → FG state byte-identical);
   (C) a static-assert that `network-bind + auth-disabled` is non-constructible. Register in
   `.github/workflows/ci-linux.yml` + the pre-commit hook, mirroring `lint_hal`/`check_inventory`.
6. **F3b-L3 mobile-companion pairing (T2 — RCE-class + concurrency).** Risks **CP-CR1..6 → the
   triad PHYRIADFG_RISK_REGISTER** (and MAY later split into a dedicated `CONTROLPLANE_RISK_REGISTER`
   when F3b-L3 is built — it un-defers `CONTROLPLANE_MASTER_PLAN §6`; cross-link, do not merge with
   DEVICE_LOST). Strategy carries the per-row mitigations-as-code: per-message auth via an
   `Authenticated<ControlCommand>` wrapper; Origin-allowlist; the mutually-exclusive
   network-bind/auth-off construction; a vetted handshake (no hand-rolled PIN+salt — Moonlight
   CVE-2020-11024); the closed FG-tuning-verb vocabulary; the existing lock-free SWMR ring for the
   listener↔poll path. **MUST NOT commit while any CP-CR is `open`.**
- Build via `_build_ra.bat`; byte-identical-off = egress OFF (the ring is not constructed,
  `FR_CONTROLPLANE_IMPL.md §1 step 5`). Operator runtime = a BF6-session packet capture for arm (A).

---

## FAMILY G — consumption / UX

### G1 (COVERED — PHYRIADFG_UI strategies + CONTROLPLANE CP2; ★ frontend = Tauri + React)
- **G1 strategy pointer (T1).** **★ Frontend = Tauri + React** (operator's 2026-06-21 decision,
  `CONTROLPLANE_MASTER_PLAN.md:18`). **`PHYRIADFG_UI_MASTER_PLAN.md:49` is STALE (says ImGui/GLFW —
  pre-pivot) and needs updating in a separate change.** Do not write ImGui. → indexes
  [`PHYRIADFG_UI_IMPLEMENTATION_STRATEGIES.md`](PHYRIADFG_UI_IMPLEMENTATION_STRATEGIES.md) (the
  launcher design, window-pick, argv-composition, the slider→flag map, the P0–P6 increments) +
  `CONTROLPLANE_MASTER_PLAN.md` CP2.
  - The slider mechanism is BUILT: `--flow-scale auto` + `--flow-scale-target-mp`
    (`main.cpp:850-856, 1122, 1194-1205, 3066-3079`); the "Auto" default is the shipped 2.1 MP
    target (`main.cpp:856`). The Tauri/React frontend drives `--flow-scale-target-mp` (continuous,
    clamp [0.25,12], `main.cpp:1122`) + the coordinated flag set; **no new FG flag is needed for the
    slider** — the UI is the missing half. Per-game profiles = frontend-side persisted config keyed
    by target-window executable name (no FG change).
  - **CP2 wiring (the one FG-process change):** add a `phyriad::controlplane::Egress` instance + a
    `TelemetryFrame` fill adjacent to the existing `telemetry_csv.hpp` emission (the frame IS the
    `CsvRow` repacked as a wire-stable POD, `TelemetryFrame.hpp:6-7`); gate behind `--cp-egress`
    (default-off, mirror `--csv` at `main.cpp:1092`). Acceptance: parity with the current `--csv`
    fields + byte-identical egress-off (`CONTROLPLANE_MASTER_PLAN §7`). Risk R-G1-1 (present-thread
    egress push) → triad RISK_REGISTER, reconciled against (not duplicating)
    `CONTROLPLANE_MASTER_PLAN §7`. Mitigation: the pillar's lock-free zero-alloc `Egress` (slot-claim
    + POD copy), cold drain off-thread; verified by the pillar's `controlplane_zeroalloc_test` +
    `controlplane_concurrency_test` (TSan) + a CP2 byte-identical-off pixel diff + an iter-ms delta.
  - **C9 test:** the operator runs the launcher, sets the one knob, plays a real game, confirms FG
    holds with default flags only (captured via `--csv`/scorer). Sequence P1 (one-knob, BUILT —
    validate) before P6 (frontend + profiles + CP2).

---

## FAMILY H — measurement / honesty

### H1 (COVERED — FG_MEASUREMENT_METHODOLOGY + the scorer README; no per-feature plan)
- **H1 strategy pointer (C8 = T0 closeable; calibration = T1 asymptote).** The instrument is
  BUILT but standalone: `framework/render/vulkan/bench/fg_quality_scorer/{main.cpp, CMakeLists.txt,
  README.md}` (metrics `dbl_edg_m`/`flowdsc`, CSV columns README.md:275). → clone the
  `.github/workflows/bench-regression.yml` pattern (baseline JSON + a PS comparator + the
  CI-informational/local-gating split, lines 38-45/65-76):
  1. Pin a held-out corpus via the scorer's `prep_zoo_sequence.py` (README.md:233-235), committed
     or seeded-in-CI.
  2. Lock `docs/framework/FGQ_BASELINE.json` (mirror `PERF_BASELINE.json`) with per-metric values +
     a tolerance X.
  3. `scripts/check_fgq_regression.ps1` (mirror `check_perf_regression.ps1`): parse the scorer CSV,
     compare `dbl_edg_m` to baseline+tolerance, exit 1 on regression.
  4. **★ CRITICAL (fix 6): the scorer needs Vulkan + a GPU (`CMakeLists.txt:14
     find_package(Vulkan REQUIRED)`). GitHub-hosted runners have no GPU → the C8 gate MUST be a
     SELF-HOSTED / local GPU gate** (the `bench-regression.yml:44-45` "run locally for an actual
     regression gate" model), NOT a GitHub-hosted runner. This is a HEAD-vs-vista drift: the vista's
     "turns CI red" wording assumes a GPU runner exists; the gate runs on the operator's rig / a
     self-hosted runner with the 1080 Ti.
  5. C8 proof: a deliberate `dbl_edg_m` regression in a throwaway branch → the job exits red →
     C8 CROSSED.
  - Calibration arm (T1 asymptote): SROCC of `flowdsc`/`dbl_edg_m` vs BVI-VFI human MOS (external
    dataset — access dependency, not code); floor 0.70, ceiling ~0.83. `flowdsc` is a flow-
    discrepancy localizer, not LPIPS — it will not reach 1.0 by construction. **No register**
    (offline read-only tool, README.md:347). → indexes FG_MEASUREMENT_METHODOLOGY_PRIOR_ART §1 +
    the scorer README (do not re-derive metric defs).

### H2 (COVERED — FG_MEASUREMENT_METHODOLOGY §1-2; no per-feature plan)
- **H2 strategy pointer (C7 = T0 closeable; camera/NR = T1 asymptote).** ★ **The C7
  PresentMon-attribution probe is HOMED HERE (under H2/C7); B1 and C1 CITE it.** The C7 latency+
  pacing arm needs no FG code (PresentMon is external):
  1. **Probe FIRST (gates everything):** run PresentMon on PhyriadFG's DComp-overlay present and
     confirm `FrameType` tags the generated frames and "All-Input-to-Photon" produces a
     non-degenerate value through the overlay (the dossier's open unknown, `:176-178`). If it
     attributes incorrectly, fall back to AMD FLM (software, no camera, stops at framebuffer) or
     LDAT (hardware, FG-agnostic). **Record which instrument survived the probe.**
  2. Same BF6 combat scene twice (PhyriadFG, then LSFG); compute `MsBetweenDisplayChange` p99
     jitter from each.
  3. Commit the CSV pair + the latency/jitter deltas → if PhyriadFG ≤ LSFG+Δ on latency AND ≤ on
     p99 jitter → C7 CROSSED. **Honesty guard:** pacing-variance cannot score the fake frames
     themselves → the claim is on the *displayed-interval* clock, not motion-correctness.
  - Camera/NR quality arm (T1 asymptote): capture-card/high-speed-camera both outputs, score
    no-reference (LSFG's output is WGC-excluded → no full-reference triples); a per-regime
    win/match/lose scorecard, ~0.7 NR ceiling. PhyriadFG's full-reference self-measurement (which
    LSFG structurally cannot do) is the honest lead. **No register** (external/offline). Route any
    committed ratio to `docs/planning/BENCHMARK_FAIRNESS.md`, never duplicated. → indexes
    FG_MEASUREMENT_METHODOLOGY_PRIOR_ART §1-2.

---

## §2 — BUILD-ORDER detail (the waves, with per-wave entry/exit gates)

The dependency spine (single source, reconciling roadmap §5 with the six clusters):

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
   [A2·P0 == C1·STEP1 == roadmap P0 — ONE item, three names]

WAVE 2  (new families + surpass — gated on Wave-1 foundations)
   A3 HDR ladder: L1 colorspace(T0) ∥ L2 ingest(T1) → L3 rolloff(T1) → L4 FP16-present(T2) ; L5 linear-light(T1)
   S2 edge-gate promotion (T1) ── feeds P2/P4/HUD ── promote early behind measured win
   E2-GRACE (T1) → E2-MATRIX (T0) → E2-CAP/E2-PRESENT/E2-MOAT (T1 + one T2 probe → triad RR)
   E1a router POD+wire (T2, P5 body) → E1b fp16-default+DP4a (T1) → E1c low-power profile (T1)
   P5 router (T2) ──┬──► D1 multi-GPU A/B-measure (T2)
                    ├──► D2 independent instances (T2, headline differentiator = roadmap S1)
                    └──► E1 / S3 / S4 capability-gated offload
   F3a/F3b-L1 classification+topology-split (T0, do early) → F3b-L2 WebSocket (T1) → F3b-L3 pairing (T2)
   F1 trust matrix+gate (T1) ; F2 vendor-honesty (T0) ; G1 CP2+launcher (T1, P6) ; H1-calibration/S4-neural/S6 (research tail)
   D3 maintainability spine ── EMERGENT across D1+D2+E1 (T1 hygiene, no standalone build)

HARDWARE / OPERATOR-GATED TAIL (T0-mechanical, blocked on access — never a code gap)
   C5 cross-vendor run (E1-VERIFY) ── proxy gate = E1a grep-zero
   C3 / F1-BF6 real-rig account-safety campaign (throwaway accounts) ── asymptote
   C2 small-GPU VRAM number ; H2 camera/NR quality rig ; H1 BVI-VFI MOS calibration
```

**Per-wave entry/exit gates:**

| Wave | Entry gate | Exit gate (advance when) |
|---|---|---|
| **0** | none (HEAD is the baseline) | C2: 3 committed CSVs on the author rig · C8: a deliberate-regression red run on the self-hosted GPU gate · C7: DComp-attribution probe PASS → committed CSV pair · C1·STEP0: MASD/stddev not worse, watchdog 0 |
| **1** | Wave-0's C1·STEP0 keystone landed | P0: PresentMon `MsBetweenDisplayChange` p99 < X · P1: flip + motion-collapse validation (closes C9) · P3: operator eye (register discharged) + `--validation` · P2: scorer flip-rate ↓, no `dbl_edg_m` regress · P4: reveal-band scorer win (unblocks S5) |
| **2** | P0 (timebase) + P5 (the wired router) landed for the D/E/router-gated items; A3/S2/E2/F families are independent of P5 and may start once Wave-1's P0 is in | A3: A3-1 histogram + A3-2 HDR-display · E1a: operator POD approval + grep-zero vendor · D1: committed CSV pair (multi ≤ single) · D2: two 60 s CSVs, no cross-interference · F3b-L3: `controlplane_security_audit` A+B+C green (all CP-CR `mitigated`/`accepted`) · F1: gate classifies · G1: CP2 parity w/ `--csv` + C9 operator run |
| **Tail** | Wave-2 code-ready + the grep-zero proxies crossed | hardware/operator access appears → run; never a code gap |

**T2-commit reminder:** every T2 item (P3, P5/E1a/D1, D2, A3·L4, F3b-L3, C1·STEP4, E2-PRESENT
probe) MUST have all its triad-RISK_REGISTER rows `mitigated`-as-code or explicitly `accepted`
before commit — no T2 commit while any of its risks is `open`.
