# FG_CADENCE_LATENCY_PRIOR_ART — how FG tools deliver a flat frame ladder AND low input lag

> **Diátaxis type:** Analysis (SOTA dossier). **Status:** the mechanism claims are
> `measured`/documented for FSR3 + DLSS-G (first-party) and the Win32 composition/VRR
> APIs (Microsoft); the **LSFG-specific** + **overlay-VRR** claims are `secondary`/
> `community` (LSFG internals are proprietary/undocumented) — leveled per claim.
> **Provenance:** a 104-agent deep-research pass (run `wf_4e970b01`, 2026-06-17): 5 angles,
> 22 sources → 97 claims → top 25 adversarially verified (3-vote) → **23 confirmed / 2 killed**.
> Every external ref is leveled [V1] first-party / [V2] secondary / [V3] community per FDP §2.3.
> **Why:** STAGE-106 `--upload-xfer` delivers the fps COUNT (present 85→~190 in combat) but the
> game "feels terrible" — the frame LADDER lurches + the async pipeline added input lag (~80 ms).
> The operator's anchor: LSFG delivers a flat, clean ladder with barely-noticeable lag even at
> 99% GPU. This dossier answers "how", for an **external-capture DComp-OVERLAY** FG (our + LSFG's
> architecture), and maps it to PhyriadFG. The operator's stance: **the frame ladder is the SOUL of
> a good FG — more than the hallucinations.**

---

## §1 — Verdict

1. **The flat ladder is achieved by OWNING the present timing — NOT by VRR.** FSR3 / DLSS-G / LSFG
   pace presents on a **dedicated high-priority thread** against a **moving-average frame time**
   (FSR3 = a busy-wait/spin loop; DLSS-G = the async "SL pacer"), on the **fixed DWM composition
   clock**. *(F1, F2 — [V1] AMD/NVIDIA.)*
2. **VRR is architecturally BLOCKED for an external DComp overlay (the operator's "G-Sync inversa"
   candidate dies, honestly).** An overlay cannot drive the monitor's VRR like a fullscreen
   swapchain — the GAME's swapchain owns the VRR timing; Windows DRR **virtualizes** the v-blank;
   and the escape hatch (`DXGIDisableVBlankVirtualization`) that would expose the real cadence was
   **REFUTED 0-3**. **LSFG itself DISABLES G-Sync and runs on the fixed global DWM clock.** So the
   flat ladder is reached WITHOUT VRR. *(F3 — [V1] MS for the APIs; [V2/V3] for the LSFG-specific.)*
3. **Low input lag fundamentally needs an in-engine Reflex/Anti-Lag hook that external tools LACK.**
   Interpolation adds ~1 real frame inherently (it holds the next real frame F2). For an external,
   no-hook tool the achievable levers are: **(a) a SHALLOW present queue** (the waitable-swapchain /
   Max-Frame-Latency-1 principle — minimize buffering), and **(b) EXTRAPOLATION** (ASW / PTW /
   Frame-Warp — present-time reprojection from PAST frames cuts latency) — but extrapolation is
   **NOT free** (the "no added latency" claim was **REFUTED 0-3**; it pays in prediction artifacts).
   *(F4, F5 — [V1] NVIDIA/Meta/MS + arXiv.)*

The reconciliation of the operator's observation: LSFG's "flat ladder + low lag at 99% GPU" =
**own-the-present-timing pacing on the DWM clock** (flat) + **a shallow queue** (low lag), as an
external overlay with no Reflex hook — exactly the levers above, NOT VRR and NOT an engine hook.

---

## §2 — Findings (with evidence + the adversarial vote)

- **F1 [V1, 3-0] — Frame pacing is a dedicated present-time mechanism on a high-priority thread.**
  FSR3's frame-generation swapchain runs a **high-priority CPU pacing thread** that **busy-waits
  (spins)** to hit a target present-time delta computed from a moving-average frame time; "pacing
  depends on owning the swapchain's present" (2-1). DLSS-G schedules both the real and the generated
  frame via its async pacer; dynamic/multi-frame DLSS-G has its own internal pacing. *(GPUOpen FSR
  SDK; NVIDIA Streamline DLSS-G guide.)*
- **F2 [V1, 3-0] — The DWM composition clock is readable; the hwnd is ignored.**
  `DwmGetCompositionTimingInfo` returns a `DWM_TIMING_INFO` (qpc of the composition/refresh cadence);
  **since Windows 8.1 the hwnd parameter is ignored** (the global clock). This is the fixed clock an
  overlay paces against. *(MS Win32 docs.)*
- **F3 [V1 APIs / V2-V3 LSFG, mixed] — VRR is not available to an external overlay.**
  Under Windows 11 **Dynamic Refresh Rate (DRR), DXGI virtualizes the v-blank** (2-1), and
  **`DXGIDisableVBlankVirtualization` does NOT expose the real changing v-blank** (REFUTED **0-3**).
  FSR3 **at launch required V-Sync ON** (3-0) and "does not work correctly" without owning the present
  path (3-0). LSFG (external overlay) **disables G-Sync** and uses the DWM clock — *(secondary/
  community; LSFG internals undocumented in any primary source).* → an external overlay cannot drive
  monitor VRR; the game's swapchain owns it.
- **F4 [V1, 3-0] — Interpolation FG adds ~1 frame; the inherent latency is real.**
  "For an interpolated frame I_1.5 between real frames … [the tool must hold the next real frame]" →
  +~1 frame. "Frame generation does not reduce input latency" (2-1); FSR3 measured higher input
  latency than native (2-1). Low lag in-engine comes from **Reflex/Anti-Lag, which DLSS-G REQUIRES
  the host application to integrate** (3-0) — unavailable to an external tool. *(arXiv; journalism;
  NVIDIA.)*
- **F5 [V1, 3-0] — The external-achievable latency levers: shallow queue + extrapolation.**
  "The fix for queued-present latency is a **waitable swapchain**" (3-0); the default Max Frame
  Latency is higher and `Present()` blocks the renderer when the queue is full (3-0) → minimize the
  queue depth. **Positional Timewarp (PTW) reduces latency by executing at present time** (3-0);
  **Asynchronous Spacewarp (ASW) generates synthetic frames** (3-0); **NVIDIA Reflex Frame Warp
  (DLSS 4) is a VR-style timewarp** (3-0). BUT "interpolation inherently adds latency / extrapolation
  adds NONE" was **REFUTED 0-3** — extrapolation cuts the *hold* latency but pays in artifacts.
  *(Meta ASW/ATW; MS DXGI; NVIDIA.)*

---

## §3 — Implications for PhyriadFG (the map to our levers)

**The flat ladder (the soul):**
- ✅ **A present-pacing thread** (own the timing): pace presents against the **DWM clock**
  (`DwmGetCompositionTimingInfo`, F2 — we CAN read it) + a moving-average frame time, high-priority,
  busy-wait-finish (we already have `hal::cpu_wait_for_ns` for the spin-finish). This is the
  documented FSR3/DLSS-G/LSFG mechanism. Our present loop today paces to its OWN timer at
  `refresh_hz` — aligning it to the actual DWM cadence removes the timer-vs-composition drift.
- ✅ **`--phase-norm` (STAGE-107, built) + the continuous-trajectory upgrade**: the even intra-pair
  phase placement. Composes with the pacing thread (even PHASES × even TIMES).
- ❌ **VRR / integer-N pacing — DROP it** (F3): our DComp overlay can't drive the monitor VRR;
  LSFG doesn't either. The operator's definitive-fix candidate is architecturally out.

**The input lag:**
- ❌ **Reflex/engine hook — unavailable** (F4): we cannot match in-engine low lag. Honest ceiling.
- ✅ **Collapse the pipeline/queue depth** (F5, the dominant lever): our ~80 ms is ~68 ms ABOVE the
  inherent ~1-frame (~12.5 ms @ 80 fps source). That excess is our queue depth — the **D-anchor**
  (pair-publish lag, ~50-68 ms, the arc's measured `delay_ema`) + the async double/triple buffering.
  The arc's **real-fast-path** (present the freshest captured real, bypass the D-anchor) + a shallow
  present queue = the LSFG-class low-lag path. Target: ~80 ms → ~20-25 ms.
- ✅ **Extrapolation (`--asw`, built)**: cuts the inherent +1-frame hold by reprojecting from past
  frames — a real lever (F5) but **not free** (artifact cost; the "no-latency" claim refuted). Use
  as an opt-in latency-vs-quality trade, not a default.

**The honest tension (F1+F5):** even pacing wants a ~1-frame buffer; low lag wants a shallow queue.
The SOTA resolution is a MINIMAL (≈1-frame) pacing buffer — which is the inherent FG frame anyway.
We are ~68 ms above that floor, so there is large headroom to cut before the tension bites.

---

## §4 — The candidate verdict (what to build, what to drop)

| Candidate | Verdict for our (external DComp-overlay, no-hook) architecture |
|---|---|
| present-pacing thread (DWM clock + moving-average + spin-finish) | **BUILD** — the documented flat-ladder mechanism (F1/F2) |
| `--phase-norm` even phases + continuous-trajectory | **KEEP/UPGRADE** — composes with the pacing thread |
| D-reduction / real-fast-path / shallow queue | **BUILD** — the dominant input-lag lever (F5); ~68 ms removable |
| extrapolation (`--asw`/PTW) | **OPT-IN** — cuts hold-latency, artifact cost (F5; "free" refuted) |
| VRR / integer-N pacing | **DROP** — overlay can't drive VRR; LSFG disables G-Sync (F3) |
| Reflex/Anti-Lag hook | **N/A** — needs an engine hook we lack (F4) |
| oversample + blend (motion-blur) | **DEFER** — perceptual smoother, not a ladder/latency fix |

---

## §5 — Honest gaps

- **LSFG internals are proprietary.** No primary-source LSFG code/developer statement on its pacing
  or queue management exists in the evidence; the LSFG-specific claims (disables G-Sync, DWM clock,
  shallow queue) are **secondary/community** inference by analogy to FSR3/DLSS-G + the Win32 APIs.
- **The overlay-VRR claim is medium-confidence** (F3): the Win32 API behavior is [V1], but "an
  external overlay definitively cannot drive VRR in all Win11 MPO/DRR configurations" is an
  inference — a first-hand probe (does our present rate move the monitor's refresh? `DwmGetComposition
  TimingInfo` deltas) would confirm it on the rig before fully closing the VRR door.
- **Time-sensitivity:** DLSS 4 Frame Warp + multi-frame DLSS-G are 2025-current; FSR3's VSync-on
  requirement was launch-only.
- The next SOTA pass (per the operator) runs AFTER the pacing-thread + D-reduction are working, on
  what the updated implementations reveal.
