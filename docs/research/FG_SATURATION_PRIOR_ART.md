# FG_SATURATION_PRIOR_ART — how FG tools deliver smooth frames on a saturated GPU

> **Diátaxis type:** Analysis (SOTA dossier). **Status:** the mechanism claims are
> `measured`/documented for DLSS-G + FSR3 (first-party); the LSFG-specific claims are
> `secondary` (community/vendor) — LSFG internals are **proprietary/undocumented**.
> **Provenance:** a 99-agent deep-research pass (run `wf_ec290a6a`, 2026-06-17): 5 angles,
> 17 sources, 77 claims → 25 verified (3-vote adversarial) → 21 confirmed / 4 killed.
> Every external ref is leveled [V1] first-party / [V2] secondary / [V3] community per FDP §2.3.
> **Why:** the operator's worst-case is FG collapsing to ~1.05× when the 4090 pegs at 99% in
> combat, vs LSFG entering the same saturated game and delivering a comfortable 240 at the cost
> of ~30 game-fps. This dossier answers "how" and maps it to PhyriadFG's levers.

---

## §1 — Verdict

Documented FG systems do **NOT** defy GPU saturation by forcing compute onto a pegged GPU.
They **avoid pegging it** (leave headroom) and **shed gracefully** (drop the generated frame,
not the whole multiplier) when pacing slips. The user's two observations are the **same
phenomenon in two regimes**: LSFG's "comfortable 240" is the *capped / headroom-left* regime;
PhyriadFG's "collapse to 1.05×" is the *uncapped-saturated* regime. They reconcile, not conflict.

Three load-bearing levers (all confirmed), plus one load-bearing negative:

| Lever | What | Level |
|---|---|---|
| **L1 — leave headroom (cap the game)** | the PRIMARY lever; cap the base fps so the GPU sits < ~85% and a fixed FG GPU-time slice fits | [V1] FSR3 half-rate (AMD GPUOpen); [V2/V3] LSFG ~70-85% heuristic |
| **L2 — timer-paced present + drop-on-slip** | present on a dedicated TIMER thread decoupled from the generation fence; DROP the generated frame when presents desync — never block | [V1] FSR3 (high-prio pacing thread + busy-wait), DLSS-G (async SL pacer; "always present the real, drop the interpolated if too close to the last real") |
| **L3 — bound the generation cost (reduced flow resolution)** | run motion estimation at reduced internal resolution, warp back at native — caps the per-frame GPU cost | [V1] LSFG "Flow Scale" tooltip (75%@1440p / 50%@4K); [V1] the principle in FSR/DLSS |
| **N1 — queue priority / preemption does NOT work (negative)** | a saturating dispatch can't be preempted (DISPATCH_BOUNDARY); D3D12 PRIORITY_HIGH no effect on AMD/NVIDIA (only Intel iGPU); Vulkan priority a non-binding hint; async-compute shares cores | [V1] MJP GPUView; [V1] Vulkan docs |

---

## §2 — Findings (with evidence)

- **F1 [V1, 3-0] — Queue priority/preemption is the WRONG lever on consumer GPUs.** A large
  saturating dispatch runs to completion (preemption granularity = dispatch/submission boundary,
  `D3DKMDT_COMPUTE_PREEMPTION_DISPATCH_BOUNDARY`). `D3D12 PRIORITY_HIGH` "doesn't make much
  difference on AMD and Nvidia" (only Intel reorders). Vulkan queue priority is an
  "implementation-independent intention" (the separate `VK_KHR_global_priority` exists because base
  priority is a weak hint). Async-compute overlaps work on the SAME shader cores — bounded by core
  availability, not new capacity. *(MJP; Vulkan docs.)* → an external tool cannot out-prioritize
  or preempt a saturating game.
- **F2 [V1 FSR / V2-3 LSFG, 3-0] — Headroom via a base-fps cap is the primary lever.** AMD: "the
  rendered frame rate slightly below HALF the output frame rate"; VSync implicitly caps render to
  ½ refresh. LSFG community/vendor: ~70-85% GPU util before enabling FG (a heuristic, not a
  developer constant — one "85-90%" variant was REFUTED 1-2 as over-specific). At 95-100% there is
  no slice → base fps drops + stutters = PhyriadFG's collapse. *(GPUOpen; Corsair; Hone.)*
- **F3 [V1, 3-0] — Enforce the cap with an external limiter locked to an INTEGER DIVISOR of the
  refresh** (60 for 120Hz×2, 40 for ×3) so base × multiplier = refresh; RTSS preferred over driver
  caps (steadier frametimes). **Distinction:** the integer divisor governs *pacing* (even spacing);
  capping *low enough* for GPU headroom is the *separate* saturation lever — a divisor alone does
  not guarantee headroom. An external tool must do BOTH. *(Hone; Corsair; the official LSFG Steam
  guide: "Limiting your fps … helps reduce this load to make space for frame generation.")*
- **F4 [V1, 3-0] — Pacing is a dedicated present-time mechanism, decoupled from the generation
  fence.** FSR3: a frame-generation swapchain proxy with a **high-priority CPU pacing thread**
  (7-frame moving-average frametime incl. UI → a target present-time delta) + a **present thread
  that busy-waits** until the delta elapses then presents (generated, then real), for DX12 + Vulkan;
  "busy wait … for the best possible timing." DLSS-G: intercepts `Present`/`vkQueuePresentKHR`, runs
  it **async** on a separate queue (the "SL pacer"), present drops ~1ms→0.2ms (non-blocking).
  *(GPUOpen; NVIDIA Streamline.)*
- **F5 [V1, 3-0] — Quality holds because FG DROPS the generated frame on slip + BOUNDS its cost —
  it does NOT shed the multiplier.** DLSS-G "always presents the real frame but the interpolated
  frame can be dropped if presents go out of sync." LSFG "Flow Scale" runs motion estimation at
  reduced resolution to "avoid 100% GPU load." **This is the exact opposite of PhyriadFG's timid
  shed-the-whole-pass.** *(NVIDIA Streamline; LSFG tooltip / Hone.)*
- **F6 [V1, 3-0 w/ qualification] — FSR3 also uses async-compute + RELATIVE queue priority for the
  interpolation pass (opt-in, default-off), explicitly NOT preemption** — but this GPU-side lever is
  weak; the dominant smoothness mechanism is the CPU-side pacing thread (F4), not queue priority.
  *(GPUOpen + verified SDK source.)*
- **F7 [V2, 2-1] — DLSS-G's Blackwell "hardware Flip Metering"** moves the pacing delay into the
  display engine (so `MsBetweenPresents` understates pacing; use `MsBetweenDisplayChange`). This is
  the ONE element an external tool **cannot** replicate (proprietary display-engine function); the
  software present-decoupling + drop-on-slip CAN be. *(NVIDIA.)*

---

## §3 — Implications for PhyriadFG (external-capture, single shared GPU)

**What we ALREADY have (2 of the 3 levers, partial):**
- **L2 (partial):** a timer-paced present loop (the STAGE-39 output-clock) + `--async-present`
  (STAGE-102) which already drops the interpolated frame. BUT our pressure response was the
  *timid shed-the-whole-multiplier* (the deficit-tier / `--load-governor`), which F5 identifies as
  the wrong move — the SOTA keeps generating and drops only the late frame.
- **L3:** `flow_div` / `--flow-scale` already runs the flow at reduced resolution — the LSFG
  "Flow Scale" equivalent. NOT currently tied adaptively to GPU util.

**What we LACK:**
- **L1 (the primary lever):** headroom. PhyriadFG runs in the uncapped-saturated regime — the game
  pegs the 4090 and we have no slice. The fix is "make space" = a base-fps cap. **An external tool
  cannot cap a game it does not control** (open question §4) — LSFG relies on the user capping via
  RTSS/in-game. PhyriadFG can *recommend/compute* the optimal cap; auto-enforcing it externally is the
  harder follow-up.
- **The right efficiency response under load:** bound the per-tick cost (adaptive flow/warp
  resolution tied to GPU util) **while continuing to generate** (drop-on-slip, not shed) — replacing
  the timid governor.

**The negative we must respect (N1/F1):** do NOT design around GPU queue priority/preemption — it
does not work on consumer GPUs (already independently confirmed as our SOTA F1 and the STAGE-26b
inlined `offload=false`).

---

## §4 — The cap arithmetic + open questions

The cap that "makes space": pick a base fps such that (a) base × multiplier = refresh (an integer
divisor, for pacing) AND (b) base leaves the GPU below ~85% (for the slice). Roughly
`cap ≈ 1000 / (display_period_per_frame + FG_generation_slice_ms)`. The FG slice is measurable from
our own telemetry (`warp_ms` + the per-tick GPU cost). **The smaller the FG slice, the smaller the
cap penalty on the game** — which is exactly why minimizing the FG's per-tick cost FIRST (before
capping) yields a more precise, more comfortable threshold.

Open questions (the research could not resolve from public sources):
1. Does LSFG internally drop-on-slip + decouple present onto a thread, or block? (Inferred by
   analogy to DLSS-G/FSR3, NOT confirmed — LSFG is closed.)
2. For an external-capture tool with no swapchain proxy and no engine hook, the cleanest way to
   enforce an automatic game cap (RTSS-style injected limiter / a present-pacing layer on the
   captured stream / a Reflex-like feedback loop) — and the measured headroom recovery + added
   latency of each.
3. How LSFG handles a game that itself pegs the GPU with no cooperative limiter — does it ever drive
   a cap on a game it doesn't control, or rely on the user (likely the latter)?
4. The concrete per-frame GPU-time slice an external interpolation pass needs at a given resolution
   + flow scale, so the cap can be computed (none of the sources give this for external capture).

---

## §5 — Honest gaps

- **LSFG internals are proprietary.** No first-party LSFG source or developer (THS) statement on its
  scheduling exists in the evidence; THS's public stance is qualitative ("if already at 99% GPU use,
  LS has no room to function"). All mechanism claims are DLSS-G (NVIDIA Streamline) + FSR3 (AMD
  GPUOpen) as documented **analogs**. LSFG-attributable facts (Flow Scale, ~70-85% headroom,
  RTSS-integer-divisor cap, saturation collapse) are secondary/community.
- **Time-sensitivity:** the FSR3 VSync-on requirement was launch-only (obsolete post-3.1); DLSS-G
  hardware Flip Metering is Blackwell-specific (DLSS 4, 2025).
- The "70-80%" / "85%" headroom figures are heuristics, not published constants.
