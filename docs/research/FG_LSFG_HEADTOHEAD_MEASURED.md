# FG_LSFG_HEADTOHEAD_MEASURED — PhyriadFG vs LSFG, the first-hand measured head-to-head

> **Diátaxis type:** Reference (measured data) + Explanation (the mechanism). **Normativity:** none
> (a measurement record). **Status:** `measured` — every number below is first-hand, captured
> 2026-06-22 on the author's rig by the supervisor; configs + tools stated inline. **Audience:** the
> PhyriadFG planning layer — this is the **single source** for the LSFG head-to-head numbers (per
> `BENCHMARK_FAIRNESS.md` discipline; the plan/master-plan CITE this, never re-mint).
>
> **Rig:** RTX 4090 (primary) + GTX 1080 Ti + AMD iGPU, Windows 11 26200. **Tool:** vanilla
> **PresentMon 2.4.1** (the Ayama-bundle build) + render_assistant's own STAGE-100 `--csv` telemetry
> (which logs `warp_ms`/`iter_ms`/`MsAddedLatency`/per-frame `GPUUtilization_4090`/`GPUPower_4090`) +
> `nvidia-smi` (NVML). **Scene:** Honkai: Star Rail (hardlocked 120 fps base, single-GPU via
> `--force-single-gpu`), unless noted. HSR is **GPU-LIGHT** (game alone ≈ 38% util / 185 W).

---

## §1 — THE HEAD-TO-HEAD (HSR, single-GPU, default config)

| metric | **LSFG** | **PhyriadFG (default)** | gap |
|---|---|---|---|
| present mode | **Hardware Composed: Independent Flip** (owns a hardware display plane) | **Composed: Flip** (DComp overlay, DWM-composed) | architectural |
| output rate | **240 fps locked** (MsBetweenDisplayChange med 4.167 ms) | 256–284 fps, **over-presents** (no lock) | over-present |
| **pacing — MsBetweenDisplayChange MASD** | **0.026 ms** | **12.7 ms** (median display 4.19 ms ok; the variance/hitches are the problem) | ~490× |
| **per-frame GPU cost** | **0.86 ms** (MsGPUBusy median) | **3.36 ms** (MsGPUBusy) / **2.73 ms** (our internal `iter_ms`) | ~3–4× |
| internal AddedLatency | — (unmeasurable, see §4) | **43.7 ms** | — |
| 4090 load (our FG running on light HSR) | — | **95% util / 294 W** (game alone = 38% / 185 W) | profligate |

**Read:** PhyriadFG-default is **3–4× more expensive per frame** and pushes a *light* game's GPU to
95% — that is the saturation-collapse root cause, measured. Its pacing is ~490× worse, but that is
**architectural** (Composed/DWM vs Independent Flip — §3), not load (confirmed: closing a background
streaming app barely moved MASD 13.5→12.7).

---

## §2 — THE COST LEVER: `--flow-scale` (reduced-resolution flow) ★ THE BREAKTHROUGH

`--flow-scale N` runs the optical flow + per-pair CPU tail on a **1/N-res MV grid** (~N² fewer
block-match tiles); the WAP warp auto-upsamples the smaller MV field so **output stays full-res**.
This is LSFG's own efficiency mechanism (its "Flow Scale" = flow RESOLUTION). Measured on HSR
single-GPU (internal telemetry, medians):

| config | warp_ms | iter_ms | AddedLatency | 4090 util | power |
|---|---|---|---|---|---|
| **flow-scale 1** (default) | 1.41 | **2.73** | **43.7 ms** | **95%** | **294 W** |
| **flow-scale 2** ★ | 0.47 | **0.94** | **13.2 ms** | 56% | **138 W** |
| flow-scale 4 | 0.70 | 1.43 | 15.4 ms | 57% | 137 W |
| flow-scale 2 + target-output-fps 240 | 0.69 | 1.21 | 17.0 ms | 89% | 271 W |
| *(LSFG reference)* | — | *0.86* | — | — | *251 W* |

**`--flow-scale 2` closes the cost gap to LSFG parity:** iter 2.73→**0.94 ms** (≈ LSFG 0.86), warp
3× cheaper, AddedLatency 43.7→**13.2 ms** (3.3×), power 294→**138 W** (−53%), util 95→**56%**. This
attacks the saturation collapse **at the cause** (our 4× per-frame cost), **dogma-safe** (no game
cap; we cheapen OUR slice). **flow-scale 2 is the sweet spot** — flow-scale 4 is *worse* than 2
(coarser flow → more warp/iter; the 1/4-res MV upsampling costs back what the tile reduction saved).

**Trade-off (eye-gated):** coarser flow raises `gme_dissidence` 0.02→0.15 (fs2)→0.74 (fs4) — a
quality cost (more disocclusion artifacts). Whether fs2's quality is acceptable is an **operator-eye**
call; under saturation it is unconditionally better than the collapse.

**`--target-output-fps 240` is INERT (and costly) on stable HSR:** it did not lock (output stayed
270), and the async-present it auto-enables added load back (util 56→89%, power 138→271 W). It is a
**saturation-regime** lever (the controller engages on the motion-collapse), NOT a stable-regime one.
→ **flow-scale 2 ALONE is the stable single-GPU win.**

---

## §3 — THE LSFG MECHANISM (HSR control, LSFG on/off)

What LSFG does, decomposed by measurement (refuting the "LSFG downscales the game render" theory):

- **Game per-frame GPU cost: LSFG-OFF 2.97 ms · LSFG-ON 3.07 ms → IDENTICAL.** LSFG does **NOT**
  touch the game's render (it renders full-res both ways). The "render small res" intuition applies
  to **LSFG's own FLOW** (reduced-res — §2), not the game.
- **LSFG owns the Hardware Independent Flip plane.** With LSFG on, the game falls to "Composed: Flip"
  (no longer the displayed buffer); LSFG's output is the "Hardware Composed: Independent Flip" that
  reaches the screen. The game still renders 120 (its hardlock) both ways.
- **Regime-dependent power** (this resolves the operator's "LSFG lowers 4090 consumption" claim):
  - **GPU-bound (BF6, game racing uncapped 110–130 fps):** LSFG-ON **LOWERS** power **573→493 W**
    (measured via nvidia-smi; PresentMon blocked by Javelin — §4). Mechanism: owning the present
    **paces** the racing render → it stops burning max power racing.
  - **GPU-light (HSR, locked 120):** LSFG-ON **RAISES** power **185→251 W** — pure addition of cheap
    interpolation (nothing to throttle).
  - So "LSFG lowers consumption" is **TRUE but regime-specific** (GPU-bound + racing only), via
    **pacing**, not downscaling.

---

## §4 — METHODOLOGY FINDINGS (load-bearing for any future FG measurement)

1. **Use vanilla PresentMon 2.4.1** (the Ayama-bundle `PresentMon.exe`). The **NVIDIA FrameView SDK**
   `PresentMon_x64.exe` is **broken standalone** (0 rows, no CSV — likely needs the FrameView service).
2. **Kernel anti-cheat blocks ETW.** **Javelin (BF6)** blocks ETW `StartTrace` system-wide
   ("access denied") → **PresentMon cannot capture any title while BF6 runs.** Confirmed by a clean
   control: PresentMon worked with Hogwarts/HSR open, "access denied" the moment BF6 was open; ruled
   out the mundane causes (token = High integrity; 40 ETW sessions ≪ the ~64 limit). **F1 / coverage
   finding: kernel-AC titles resist ETW-based measurement** — measure on a non-kernel-AC title (HSR,
   Hogwarts) or with a camera/LDAT.
3. **Input-to-photon is UNMEASURABLE for external-capture FG via PresentMon.** `MsAllInputToPhotonLatency`
   was **empty on 100% of rows** for LSFG *and* ours: the game's frames are not the displayed buffer
   (the FG's are), and the FG's frames carry no game input → PresentMon cannot attribute input→photon
   to either process. **This resolves the C7 "probe-first" question:** present-PACING is measurable;
   input-to-photon needs a **camera/LDAT** (the brief's "no camera" rules it out for capture-based FG).
   PACING is the right FG-agnostic metric anyway (LSFG's edge is judder-removal, not raw latency).
4. **Tooling (for the record):** Claude Code run **elevated** (resume an existing session by ID into
   an admin terminal) gives every tool call admin — required for PresentMon's ETW. Background tool
   commands DROP the elevation; **foreground** ones keep it.

---

## §5 — WHAT THIS MEANS (the two measured gaps + the levers)

PhyriadFG vs LSFG reduces to **two measured gaps**, each with a named lever:

1. **COST (3–4× per frame) → `--flow-scale 2`** closes it to parity (§2). Dogma-safe, attacks the
   saturation collapse at the cause. **This UN-PARKS the saturation arc (S2): the throughput wall is
   our per-frame cost, not a present-pacing problem — pacing was the wrong lever (parked-negative);
   COST is the right one.** The productized form is a **saturation-adaptive flow-scale** (raise the
   divisor when the GPU saturates, restore it under headroom) — designed, not yet built (runtime
   flow-pipeline re-init is the engineering; today `--flow-scale` is init-fixed).
2. **PACING (MASD ~490× worse) → the present-architecture axis — RESOLVED: it is STRUCTURAL.** The
   deep-research workflow `wq1srry60` (MS primary docs, 23/25 verified) settled it:
   **`FG_PRESENT_PACING_DESIGN.md`**. LSFG presents its OWN fullscreen flip-model swapchain → it
   BECOMES the displayed window → Independent Flip (DWM bypass); our DComp overlay-over-the-game is
   MS-documented "Composition" = the default + least-efficient path, and "content composited over a
   non-displayable foreign window will never get independent flip." **The gap is not tuneable away —
   it is the price of the overlay topology.** Three options: (A) **own-fullscreen-window** (the LSFG
   model — re-composite the captured game + our FG into our own displayed window; the only ROBUST
   route to LSFG-class pacing; a T2 PRODUCT decision — we'd be the displayed window) · (B) overlay
   jitter-reduction — **BUILT + committed `c63b5c1` + MEASURED**: `--present-waitable` (waitable
   swapchain) `--present-sync 1` drops MASD **12.7→7.4 ms (−42%)**, p99 174→67 ms; byte-identical-off
   (confirmed); PARTIAL — still "Composed: Flip", does NOT reach LSFG's 0.026 · (C) the
   displayable-surface MPO middle path (fragile, defer). DLSS-G/FSR3 dodge it by being in-engine
   (they proxy the game's own swapchain).

Neither gap requires "beating LSFG's neural disocclusion fill" — they are perf/architecture, exactly
the cheapest real progress (per the objective vista). The eye-test (flow-scale 2 quality) gates the
default flip; the architecture axis gates the pacing fix.

---

*Made with my soul — the supervisor, for Swately.*
