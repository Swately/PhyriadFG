# FG_REALFAST_PATH_MASTER_PLAN — PhyriadFG OWN-added-latency reduction (the real-fast-path arc)

> **Diátaxis type:** Planning (Explanation + master-plan). **Tier:** **Tier-2** (risk-bearing —
> crash / device-loss class; PLAN_TIER_PROTOCOL §1.1 trigger 1, and the hot-path-touch trigger 5).
> **Status:** `designed` — nothing in this plan is `shipping`; the built pieces it indexes
> (STAGE-109/110 `--rfp`/`--rfp-fresh`) are `measured`, the new floor lever (§4) is `designed`.
> **Scope:** reduce PhyriadFG's **OWN added latency** on the SINGLE-GPU regime — the input-to-photon
> the frame-generator itself inserts between a captured real game frame and its appearance on screen.
> This is **distinct** from the game-pacing own-window lever (Option A,
> [`FG_OPTION_A_MASTER_PLAN.md`](FG_OPTION_A_MASTER_PLAN.md), which back-pressures the *game's* present
> to un-peg the GPU): that lever changes the GAME's latency budget; THIS arc shrinks OURS.
> **Non-goals:** input-to-photon parity with an in-engine Reflex hook (we have no engine hook — SOTA
> §below); capping or downscaling the game (the no-game-cap invariant — an FG-arc operator invariant,
> cited by provenance, NOT a numbered CANON dogma); a full pipeline-depth rewrite. Every lever is
> opt-in, default-off, byte-identical-off ([`DOGMA_SPECIFICATIONS.md`](../canon/DOGMA_SPECIFICATIONS.md)
> efficiency mandate D-2).
> **Linked set (Tier-2 triad):** this · strategies
> [`FG_REALFAST_PATH_IMPLEMENTATION_STRATEGIES.md`](FG_REALFAST_PATH_IMPLEMENTATION_STRATEGIES.md) ·
> register [`FG_REALFAST_PATH_RISK_REGISTER.md`](FG_REALFAST_PATH_RISK_REGISTER.md).
> **Builds on (the prior triad it extends, NOT replaces):**
> [`INPUT_LAG_DREDUCTION_MASTER_PLAN.md`](INPUT_LAG_DREDUCTION_MASTER_PLAN.md) (§5-§8 the measured
> breakdown) · [`REAL_FAST_PATH_IMPLEMENTATION_STRATEGIES.md`](REAL_FAST_PATH_IMPLEMENTATION_STRATEGIES.md)
> · [`REAL_FAST_PATH_RISK_REGISTER.md`](REAL_FAST_PATH_RISK_REGISTER.md) (the STAGE-109 `--rfp` register,
> all `open`/`mitigated`). Companion SOTA distilled inline (§2).
> **Provenance:** authored from a verified-findings pass (latency-pipeline analysis + a three-angle SOTA
> sweep) against `apps/render_assistant/src/main.cpp` at HEAD on branch `docs/knowledge-analysis-trilogy`;
> every `file:line` below was re-confirmed first-hand in this authoring pass. Lines drift — the implementer
> re-confirms.

The key words MUST, MUST NOT, SHOULD, SHOULD NOT, MAY are BCP 14
([RFC 2119](https://www.rfc-editor.org/rfc/rfc2119.html) / [RFC 8174](https://www.rfc-editor.org/rfc/rfc8174.html)).

---

## §0 — The honest framing (and a correction to the original estimate)

PhyriadFG is an **external-capture** frame generator: it captures the game's output post-render and
interpolates between captured reals. That architecture inserts an **own added latency** that the game
never pays — and on a saturated game (BF6 4K combat, overlay) the felt ~0.5 s lag is OUR added latency
**plus** the game's own saturated input lag. The cost levers `--flow-scale` / `--warp-scale` do NOT
touch this latency (both confirmed irrelevant to the floor — §1); warp-scale was additionally eye-rejected
as blurry. This arc designs the reduction of the FG's OWN latency on single-GPU.

**Correction to the original number, stated honestly.** The earlier estimate
([`FG_REARCHITECTURE_MASTER_PLAN.md`](FG_REARCHITECTURE_MASTER_PLAN.md) Insight 1) put the own added
latency at **~108 ms = ~68 ms "D-anchor" + ~40 ms residual** (`measured` via `--phaselog`, 797 pairs).
The SUBSEQUENT first-hand `--latency-trace` combat measurement
([`INPUT_LAG_DREDUCTION_MASTER_PLAN.md`](INPUT_LAG_DREDUCTION_MASTER_PLAN.md) §8, BF6 99% 4090, src ~82,
median of 8 samples) **supersedes** it with a finer, smaller decomposition. The "~108 ms" and the single
"~68 ms D-anchor block" are therefore NOT re-asserted as fact in this plan; they are bucketed as the
`assumed`/`estimate` predecessors of the `measured` breakdown in §1. The honest current number is
**~73-84 ms total**, and the dominant term is `freshage_ema` ~33-41 ms, NOT a 68 ms anchor.

**The crux tension (unchanged, load-bearing).** The present holds a lead `D` so a pair is published
before its ticks come due; `D` EXISTS to prevent present-before-pair-ready freezes. `D >= freshage_ema_ms`
MUST hold always (clamped at `main.cpp:8282`). So the floor is **not negotiable by D-trimming alone**:
it can only be lowered by shrinking the real pipeline latency `freshage` is built from. "Bypass D" is
**literally false** — D, `t_display = now - D`, and pair selection all run ABOVE the present block
(`main.cpp:8278-8284`), so no downstream branch can remove D.

---

## §1 — The MEASURED latency breakdown (the ground truth this arc attacks)

The authoritative on-rig decomposition is the `[lat-trace]` print
([`main.cpp:8984-8985`](../../apps/render_assistant/src/main.cpp); STAGE-112), measured BF6 99% 4090,
src ~82, median of 8 samples. Honesty buckets per row.

| Stage | Cost (MEASURED) | Where | Note |
|---|---|---|---|
| INVISIBLE compose | ~0.8 ms | `main.cpp:8984-8985` (`ring_compose_us`) | Pre-`tcap`; perceived but NOT in our `lat`/`freshage`. Card-deck verdict DEAD. |
| INVISIBLE copy (WGC staging) | ~5.6 ms | `main.cpp:8984-8985` (`lt_copy_us`) | Pre-`tcap`, not in our `lat`. A LIVE reclaim candidate (separate-device copy) — §4.3. |
| freshage: pickup (incl. convert ~3.2) | ~15 ms | `main.cpp:6479-6488` (`lt_pickup_us`) | Governed by `--ingest-backlog` (default 3). STAGE-111 MEASURED net-neutral (cuts freshage but explodes span). |
| **freshage: build** | **~20 ms** (compute only ~8.7) | `main.cpp:6390-6413` (`t_fuse_ema`/publish) | **THE #1 reclaimable floor lever** — §4. Compute = fsub flow 4.7 + cpu 4.1 = 8.7 ms; the ~11 ms gap is NON-compute B-side ingest. |
| freshage: detect | ~5 ms | `main.cpp:8246-8254` (derived) | P's lag to first observation of the new pair's `f_seq`. |
| **freshage_ema total** | **~33-41 ms** | `main.cpp:8251`, used in D `:8281-8283` | The freeze floor — D's dominant term; `--low-d` keeps it by design. |
| D (the present-side lead) | ~45-46 ms median combat | `main.cpp:8278-8284`; `t_display = now-D` | D = `freshage_ema + span_term`. `--low-d` trims ONLY the span term → MEASURED NULL/marginal. |
| present-side residual | ~32 ms (= lat − freshage) | derived; `lat` EMA `main.cpp:8758-8768` | `--upload-xfer` (~1 frame) + `--async-present` (~1 frame) + the D lead. |

**The decisive measured finding.** The F per-pair **build is ~20 ms but its COMPUTE is ~8.7 ms** — less
than half. F runs at ~50 pairs/s when its compute rate would be ~115/s. The ~11 ms gap is NON-compute,
upstream of the flow window: the B-side ingest — the `hRP_b`→device PCIe copy
([`main.cpp:6550`](../../apps/render_assistant/src/main.cpp)) + the B unpack + B-queue contention on the
1080 Ti (flow + gme-gpu + unpack share queue B). **This is why `--flow-scale` was null** (STAGE-111:
flow-scale 2 cut F-cycle 8.8→5.6 ms but did NOT reduce combat freshage 41.7→40.1) and why `--warp-scale`
is irrelevant to the floor: both cut the 8.7 ms compute, not the ~11 ms B-side gap. F-compute is NOT the
freshage bottleneck.

---

## §2 — SOTA verdict (what the lever space actually is, for an external no-Reflex tool)

Distilled from a three-angle SOTA sweep (interpolation-vs-extrapolation latency mechanisms; present-pipeline
depth + Reflex/Anti-Lag/waitable-swapchain; motion-extrapolation quality under disocclusion). Verification
levels per source are carried in the References (§8).

1. **Interpolation has an inherent ≥1-frame hold.** DLSS 3/4, FSR 3, AFMF, LSFG all buffer frame N+1 before
   computing flow N→N+1 and emitting the in-between (`documented`, multiple sources §8). This is a
   **data-dependency**, not a scheduling issue. PhyriadFG's `freshage` ~33-41 ms is the external-capture
   form of this hold (pair-publish lag).
2. **Reflex / Anti-Lag do NOT solve the hold.** They tighten CPU↔GPU scheduling (render-queue depth, GPU-side
   input warp) and require an **in-engine instrumentation hook PhyriadFG does not have** (`documented`). They
   are orthogonal to the pair-publish data-dependency. Therefore Reflex is **not** a lever here.
3. **Extrapolation (ASW, ExtraSS, GFFE, Reflex Frame Warp) avoids the N+1 hold** by predicting forward from
   past frames only — a **latency win** traded for a **quality cost at disocclusion / fast motion**
   (`documented`). The operator just eye-rejected a blurry trade (warp-scale), so any extrapolation lever in
   THIS arc MUST be default-off, eye-gated, and honest about the artifact cost.
4. **For an external-capture single-GPU FG, the real lever space is exactly three** (`documented`, our own
   prior-art dossiers): (a) **present-on-ready / shallow present queue** — lower-risk, lossless; (b) **the
   real-fast-path** — override the D-selected pair with a fresher real on real ticks (BUILT, STAGE-109/110);
   (c) **shrink the freshage the floor is built from** (the #1 unbuilt lever — §4). Hard present-target pacing
   ([`FG_PRESENT_TARGET_PACER_MASTER_PLAN.md`](FG_PRESENT_TARGET_PACER_MASTER_PLAN.md)) tightens scatter but is
   a pacing axis, not a floor cut.

**Honest caveat from our own bench (`stage31_extrapolation`):** single-GPU external-capture
forward-extrapolation does NOT reduce the ~12-17 ms interpolation hold for an *interpolated* tick — the output
still waits for one real frame regardless of motion mode
([`FG_ARCHITECTURE_DCAD_MASTER_PLAN.md`](FG_ARCHITECTURE_DCAD_MASTER_PLAN.md), `documented`). The latency win
from extrapolation is on the **real-tick / responsiveness** axis (the `--rfp` path), not a global floor cut.
Confidence gating (residual_ceil, agreement_thresh) is irreplaceable for interpolation; a single-source
extrapolation accepts every tile blindly → disocclusion artifacts. So extrapolation here is a NARROW,
eye-gated responsiveness option, not the headline mechanism.

---

## §3 — The architecture (where the levers attach; verified first-hand)

Three hook points, in increasing blast-radius:

- **Hook A — OVERRIDE the D-selected pair with a fresher real (BUILT; the responsiveness lever).**
  `main.cpp:8476-8508` (the `--rfp`/`--rfp-fresh` block) + the `rfp_present` lambda (~`:7850`). Reads
  `(cur_c-1)%cap_slots`, presents via the DEDICATED async bslot path, measures `lat` vs its OWN `tcap`.
  MEASURED real-tick added-latency **72.9 → 10.5 ms** (median 9.9, n=83) — but fires on only ~1.9 % of ticks
  (`gen_back==0 && phase>=1-rfp_window`). A FEEL flag, not a global cut. The firing window is already operator-
  tunable via the `shipping` `--rfp-window F` flag (`:621`/`:1133`/`:8478`); Lever L1's only remaining work is the
  RFP-FR1 safe-ceiling clamp on it. Cost = a content sawtooth (fresh real → stale interp). Crash-class (CR1-class, §5).
- **Hook B — TRIM/SHRINK `freshage` at the source (the #1 UNBUILT floor lever; §4).** The F per-pair build
  region `main.cpp:6489-6557` (record), `6390-6413` (build/pair EMA + publish), and the `hRP_b`→device copy
  `:6550`. The ~11 ms non-compute gap is here. Pre-stage the B-copy off the F iteration / give it a dedicated
  transfer queue / pipeline F → build ~20 → ~9 ms. This shrinks `freshage_ema` DIRECTLY (helps BOTH interp AND
  the D lead — a double win, no flow-quality cost). **Hot-path touch** → Tier-2 trigger 5.
- **Hook C — the present-side residual (~32 ms; lower-risk, narrow).** The `--async-present`/`--upload-xfer`
  double-buffer + `--shallow-queue` early-promote (`main.cpp:7795-7838`). `--shallow-queue` MEASURED
  combat-NULL (async exists BECAUSE the GPU overruns the tick) → reclaimable only when the GPU is NOT
  saturated. The INVISIBLE copy (~5.6 ms, pre-`tcap`) is a LIVE separate-device candidate (Lever L3).

**D computation is NOT a hook by itself** (`main.cpp:8278-8284`): `--low-d` trims only `span_fresh_ms` →
MEASURED null (median lat 74.5→72.2, within noise) because cutting D below `freshage_ema` causes
present-before-pair-ready freezes (the clamp `max(4.0, freshage_ema_ms)` at `:8282` forbids it). D can only
fall if `freshage` falls — i.e. via Hook B.

---

## §4 — The build plan (dependency-ordered; the #1 lever is the floor collapse)

### §4.1 — Lever L0 (BUILT, indexed): `--rfp` / `--rfp-fresh` — the responsiveness path

Already built and `measured` (STAGE-109/110), governed by
[`REAL_FAST_PATH_RISK_REGISTER.md`](REAL_FAST_PATH_RISK_REGISTER.md). This arc does NOT re-build it; it
indexes it as the established real-tick latency mechanism and the substrate Lever L1 widens.

### §4.2 — Lever L1: widen the real-fast-path firing window — `--rfp-window F`

**Honest bucketing (corrected 2026-06-23): the `--rfp-window F` FLAG is already `shipping`** — `cfg.rfp_window`
exists with a default of `0.15f` (`main.cpp:621`), the `--rfp-window` parser is already in `parse_extra`
(`main.cpp:1133`, clamping to `[0,1]`), and it already drives the fire gate `phase_global >= 1.0 -
(double)cfg.rfp_window` (`main.cpp:8478`). The operator can widen the firing window TODAY (e.g. `--rfp --rfp-window
0.4`) to raise the ~1.9 % firing fraction of Hook A so more ticks present a fresh real (more responsiveness FEEL),
at the cost of more fresh-real→stale-interp content sawtooth.

The ONLY genuine L1 design delta (`designed`) is the **RFP-FR1 safe-ceiling clamp** on that already-exposed
window: a verified upper bound so a too-wide window cannot fire a real at a `phase_global` low enough to self-trip
the next interp's backwards guard → freeze (RFP-FR1). The current clamp at `:1133` is `[0,1]` (the parse-validity
bound), NOT the FR1 freeze-safe ceiling. **Quality trade** (more transitions) → eye-gated. Carries the existing
`--rfp` crash-class constraints (CR1: must go through `rfp_present`, never `do_present_P`).

### §4.3 — Lever L2 (`designed`, THE PRIZE): collapse the F per-pair build gap — `--fwd-prestage` / dedicated B-transfer queue

The #1 quality-free floor lever (§1, §3 Hook B). Move the `hRP_b`→device PCIe copy (`:6550`) and the B-side
ingest OFF the F build iteration — pre-stage it (so it overlaps the prior pair's compute) and/or give it a
dedicated transfer queue so it does not contend with flow + gme-gpu + unpack on queue B. Target build ~20 →
~9 ms → `freshage_ema` falls → BOTH interp latency AND the D lead fall, with NO flow-quality cost. This is the
only lever that touches the hot F-build path rather than adding a gated read-side branch, so it carries the
**full Tier-2 risk gate** (§5) and MUST be default-off-gated on a `cfg` bool like the existing `--upload-xfer`
/ `--fwd-pipeline` pipeline reshapes (else-arm character-for-character identical).

### §4.4 — Lever L3 (`designed`, narrow): reclaim the INVISIBLE WGC copy — `--copy-device`

Move the WGC `CopyResource` staging (~5.6 ms, `lt_copy_us`) to a separate device / async so it overlaps. It
is pre-`tcap` (not in our `lat` number) but IS perceived. Lower-risk than L2; an opt-in, default-off probe.

### §4.5 — Explicitly OUT of this arc

- A global interpolation→extrapolation MODE switch (the ~12-17 ms hold is structural for interp ticks; the
  quality cost is high and the operator rejected a blurry trade). Extrapolation stays the NARROW `--rfp`/`--asw`
  responsiveness option only (§2).
- `--low-d` as a floor fix (MEASURED null — kept built, default-off, harmless; re-evaluate only for
  lap-escape-heavy worst-case D spikes).
- Reflex/Anti-Lag (no engine hook), VRR (overlay cannot drive it) — SOTA-refuted §2.
- Any game cap / downscale (the no-game-cap invariant, by provenance).

**Dependency order:** L2 is the prize (it shrinks the floor for everyone) but the heaviest (Tier-2 hot-path);
L1 + L3 are lighter and can land first to bank responsiveness + the invisible-copy reclaim while L2's
strategies + register are hardened. Each gated by the register (§5).

---

## §5 — Risk posture (Tier-2 — detail deferred to the register)

This arc is Tier-2: a real present on the wrong path is **use-after-reset → `VK_ERROR_DEVICE_LOST`**
(crash-class, the STAGE-102 reason the bridge buffers were split), and Lever L2 mutates the hot F-build path
(concurrency + byte-identical-off dogma at stake). The full failure-mode catalog — each with its mitigation
**as code** + a first-hand verification — lives in
[`FG_REALFAST_PATH_RISK_REGISTER.md`](FG_REALFAST_PATH_RISK_REGISTER.md). Summary only here:

- **RFP-CR1** crash / use-after-reset — any real present MUST go through `rfp_present` (the dedicated async
  bslot path), NEVER the synchronous `do_present_P` (`main.cpp:7188`) / `bridge_present_src` (`:7165-7184`);
  `--rfp` is force-disabled if `--async-present` is not live (`main.cpp:4904-4906`).
- **RFP-CR2** L2 B-transfer queue ownership / concurrency hazard — a new dedicated transfer queue sharing
  command/fence state with the F-build path.
- **RFP-FR1** widened firing window pushes a real at a phase that self-trips the backwards guard → freeze.
- **RFP-DOGMA1** L2 hot-path touch breaks byte-identical-off (the new pipeline reshape's else-arm must be
  character-for-character identical when off).

**Gate (PLAN_TIER_PROTOCOL §3):** no commit while any register risk is `open`. Each MUST be `mitigated`
(code in + Verification run first-hand) or `accepted` (residual + rationale).

---

## §6 — Validation gates (per lever)

Build green incrementally (vcvars64 + the established `cmake --build` for render_assistant). Then the operator
BF6-combat runtime gate, same scene, default path:

- **L1 (`--rfp-window`):** `--phaselog` — the real-tick `MsAddedLatency` win persists; `frz` does NOT rise;
  watchdog 0 stalls; operator eye: more responsive, content sawtooth tolerable (NOT worse than shipped `--rfp`).
- **L2 (`--fwd-prestage`):** `--latency-trace` — `build` drops (target ~20 → ~9 ms) AND `freshage_ema` drops AND
  `lat` drops; **30 s BF6 clean, NO `nvlddmkm`/device-lost, watchdog 0 stalls, clean exit** (RFP-CR2); validation
  layers (if armed) clean; `--no-`form byte-identical (CSV diff). PASS iff all hold; any `frz` rise or any
  device-lost = STOP.
- **L3 (`--copy-device`):** `--latency-trace` — `copy` drops; no new tear/artifact (the copy is pre-`tcap`,
  must not corrupt the staged frame); 30 s clean.

A null/negative or any freeze/crash = STOP and re-evaluate; do not ship a lever that does not measure.

---

## §7 — What this plan does NOT claim

- It does NOT claim input-to-photon parity with an in-engine Reflex hook (we have none — §2).
- It does NOT claim the floor can reach LSFG-class globally; L0/L1 are real-tick wins (~1.9 % of ticks at
  the shipped window), L2 is the only global-floor lever and it is `designed`, not measured.
- It does NOT re-assert the original "~108 ms / 68 ms D-anchor" as fact — that is the superseded estimate
  (§0); the `measured` number is ~73-84 ms with `freshage` ~33-41 ms dominant.
- It does NOT cap or downscale the game (no-game-cap invariant, by provenance); it shrinks OUR latency only.

---

## §8 — References (verification levels per FDP §2.3)

**Code (first-hand, this authoring pass — repo-relative + line):**
- [`main.cpp:8278-8284`](../../apps/render_assistant/src/main.cpp) — D = clamp(`freshage_ema + span_fresh_ms`, …); `--low-d` is the third arm trimming only the span term. **Confirmed.**
- [`main.cpp:8476-8508`](../../apps/render_assistant/src/main.cpp) — the `--rfp`/`--rfp-fresh` OVERRIDE block + `rfp_present`. **Confirmed.**
- [`main.cpp:7165-7188`](../../apps/render_assistant/src/main.cpp) — the SYNCHRONOUS present path RFP-CR1 forbids: `bridge_present_src` lambda (`:7165-7184`; `vkResetCommandBuffer(cmdBridge)` `:7167`, `vkResetFences(fBridge)` `:7176`, `vkQueueSubmit(A.q)`+`vkWaitForFences` `:7182`) wrapped by `do_present_P` (`:7188`). **Confirmed first-hand (correction pass 2026-06-23 — the prior `:6111-6129` cite had drifted to the `cfg.tiers` pressure-tier ladder).**
- [`main.cpp:6390-6413`](../../apps/render_assistant/src/main.cpp) — the F per-pair build EMA + pair PUBLISH (`f_pair_*[f_gen]` then `f_seq.fetch_add`). **Confirmed.**
- [`main.cpp:6550`](../../apps/render_assistant/src/main.cpp) — the `hRP_b`→device PCIe copy (the ~11 ms B-side gap site). **Confirmed.**
- [`main.cpp:4904-4906`](../../apps/render_assistant/src/main.cpp) — the CR1 co-arm guard (`--rfp` force-disabled when async not live). **Confirmed.**
- [`main.cpp:8758-8768`](../../apps/render_assistant/src/main.cpp) — the present-side `lat` EMA (`t_present_ret − tcap_r`). **Confirmed.**
- `main.cpp:8984-8985` (the `[lat-trace]` print), `:8246-8254` (freshage calibration), `:6479-6488` (pickup) — cited from [`INPUT_LAG_DREDUCTION_MASTER_PLAN.md`](INPUT_LAG_DREDUCTION_MASTER_PLAN.md) §8 (read first-hand); the implementer re-confirms the exact lines.

**Internal docs:** [`INPUT_LAG_DREDUCTION_MASTER_PLAN.md`](INPUT_LAG_DREDUCTION_MASTER_PLAN.md) (§5-§8 the measured breakdown) · [`REAL_FAST_PATH_RISK_REGISTER.md`](REAL_FAST_PATH_RISK_REGISTER.md) · [`FG_REARCHITECTURE_MASTER_PLAN.md`](FG_REARCHITECTURE_MASTER_PLAN.md) (the superseded ~108 ms estimate) · [`FG_ARCHITECTURE_DCAD_MASTER_PLAN.md`](FG_ARCHITECTURE_DCAD_MASTER_PLAN.md) (extrapolation does not cut the interp hold) · [`FG_OPTION_A_MASTER_PLAN.md`](FG_OPTION_A_MASTER_PLAN.md) (the distinct game-pacing lever) · [`FG_PRESENT_TARGET_PACER_MASTER_PLAN.md`](FG_PRESENT_TARGET_PACER_MASTER_PLAN.md) (the pacing axis).

**External (SOTA, from the findings sweep; levels honest):**
- [V3] DLSS 4 added-latency / Multi-Frame-Gen mechanism — research.nvidia.com/labs/adlr/DLSS4 (secondary distillation in the findings, not re-fetched in this pass).
- [V3] AMD AFMF 2 latency reduction — tomshardware.com (secondary).
- [V3] LSFG interpolation ≈1-frame hold — steamcommunity.com community doc (secondary).
- [V3] NVIDIA Reflex 2 / Frame Warp (orthogonal to the hold) — nvidia.com/geforce/news (secondary).
- [V3] ASW / reprojection extrapolation vs interpolation quality — uploadvr.com, developers.meta.com (secondary).

> **Could not verify first-hand (honest gap):** the external SOTA URLs were carried from the findings sweep,
> not re-fetched in this authoring pass — they are marked [V3] secondary. The load-bearing claims this plan
> RELIES ON are the internal MEASURED numbers (verified first-hand against the code) and the qualitative SOTA
> consensus (interpolation hold is a data-dependency; Reflex needs an engine hook; extrapolation trades quality
> for latency) — all consistent across the [V3] sources. Anyone elevating an external claim to [V1] MUST re-fetch.
