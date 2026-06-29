# FG_ADAPTIVE_FLOWSCALE_DESIGN — the saturation-adaptive flow-scale (the productized cost lever)

> **Diátaxis type:** Planning (design). **Normativity:** BCP 14. **Status:** `designed` (not built —
> the runtime re-init is the engineering). **Tier:** T1 (with the pre-built-pipelines approach; the
> re-init approach is T2 — device-loss-adjacent). **Audience:** the next implementer.
>
> **Evidence base (measured, the single-source):** [`FG_LSFG_HEADTOHEAD_MEASURED.md`](FG_LSFG_HEADTOHEAD_MEASURED.md).
> This design productizes that dossier's §2 breakthrough.

## §1 — Why (the measured motivation)

PhyriadFG-default is **3–4× more expensive per frame than LSFG** (iter 2.73 vs 0.86 ms) and pushes
even a *light* game's 4090 to 95% util — the saturation-collapse root cause. **`--flow-scale 2`
(reduced-res flow) cuts it to LSFG parity** (iter→0.94 ms, util 95→56%, power 294→138 W) — the same
mechanism LSFG uses, dogma-safe (it cheapens OUR slice; it never caps the game). But `--flow-scale N`
is a **static, init-fixed** choice today: pick N=1 (best quality, collapses under load) or N=2
(survives load, costs quality everywhere). **The lever wants to be DYNAMIC:** run N=1 under headroom
(full quality) and auto-raise N under saturation (make space) — the quality↔latency trade-off,
automated. That is this design.

## §2 — The contract ("done")

A new default-off flag `--flow-scale-adaptive` (byte-identical off) that, at runtime, **raises
`flow_div` when the GPU saturates and restores it under headroom**, so the FG slice stays affordable
through a load spike instead of collapsing — with no game cap, no hitching, and a measured net win
(the FG keeps generating where N=1 would collapse). Closeable test: on a load spike (or BF6 combat),
the adaptive run holds a higher sustained `fg_multiplier` + lower `iter_ms` than the static N=1 run,
PresentMon/`--csv`-evidenced, with no device-loss and no per-switch hitch > one frame.

## §3 — The engineering (the hard part: runtime divisor change)

`flow_div` sizes the flow pipeline at init (the downsample scratch images WW/N×WH/N, the OFP pyramid,
the MV grid — `main.cpp:837-856`, `OpticalFlowPipeline` built once). Changing N at runtime means new
resource sizes. Two options:

- **(A) Re-init on switch — T2, REJECTED as the default.** Destroy + recreate the flow pipeline at the
  new N mid-stream. Simple to code but causes a **re-allocation STALL** on every switch (a visible
  hitch) and touches live Vulkan resource lifetime on the present path → device-loss-adjacent. A
  thrashing controller would hitch repeatedly. Only acceptable if switches are very rare + gated.
- **(B) Pre-built pipelines + switch — ~~T1~~ → T2 ARCHITECTURAL (corrected 2026-06-22, first-hand).**
  The original estimate ("just two small flow pipelines, switch which one the per-pair flow uses")
  was OPTIMISTIC. First-hand scope of `main.cpp`: **`ofp.motion_width()/height()` (the MV grid size)
  is a foundational INIT-time constant that the WHOLE downstream pipeline is sized against** — the
  host MV bridges (`hostMV`, `:1585`), the warp's sampled MV/SAD/MVB grids (`:4306-4310`), the
  block-provider's `mvw/mvh` (`:2142/2190`), gme (`:4608`), NVOFA (`:4164`), mv_smooth (`:4189-4199`),
  and a **FATAL init assert** that the grid equals `(WW_flow+7)/8` (`:4137`). So a runtime flow_div
  change is NOT a cheap pipeline swap: it requires **re-sizing/re-allocating every MV-grid-sized
  downstream resource** (option-A re-init = a big stall + device-loss-adjacent concurrency across
  ~10 sites) OR **building TWO of the entire downstream pipeline** (the host bridges, the warp grids,
  the provider, gme, nvofa, mv_smooth — ~2× memory + a per-pair selector through all of them).
  **Either way it is a T2 ARCHITECTURAL ARC, not a T1 lever** — it needs its own RISK_REGISTER, a
  focused build, and the operator's eye. The static `--flow-scale 2` (the measured cost win) + the
  init-time `--flow-scale auto` are the SHIPPING cost levers; the runtime-adaptive form is a deliberate
  architectural project, correctly sequenced AFTER the present-architecture (own-window) decision since
  both re-touch the pipeline's foundations.

## §4 — The controller (util-driven, hysteretic)

- **Signal:** the per-frame **`GPUUtilization_4090`** (already logged in the STAGE-100 telemetry) or,
  more robustly, the FG slice's own `iter_ms` vs the present budget (the slice doesn't need NVML —
  measure whether the warp+flow fit the frame period). Prefer the **slice-fits-budget** signal: it is
  internal, low-latency, and vendor-neutral (no nvidia-smi dependency).
- **State machine (hysteresis to avoid thrashing):** raise N (1→2→4) after **K consecutive** frames
  where the slice overruns its budget (e.g., iter_ms > sustain_frac · frame_period); lower N after **K
  consecutive** frames comfortably under (e.g., iter_ms < 0.5 · frame_period). K ≈ 8–16 frames + a
  minimum dwell time per level (e.g., ≥ 250 ms) so a brief spike doesn't thrash. The thresholds are
  the eye-calibratable knobs (`--flow-scale-adaptive-hi/lo`).
- **Bound:** N ∈ {1, 2} by default (the measured sweet spot — N=4 is *worse* than N=2, dossier §2);
  expose N=4 only behind an explicit max-divisor flag for very-low-end GPUs.

## §5 — Relation to the existing knobs / dogmas

- **Composes with `--flow-scale auto`** (the existing INIT-time megapixel-budget pick, `main.cpp:3072`):
  auto sets the headroom N at startup from the resolution; adaptive RAISES it under runtime load. They
  are orthogonal (init baseline vs runtime spike response).
- **Dogma-safe:** never caps/throttles the game; it only changes OUR flow resolution. The
  efficiency mandate is served (we measure the slice and adapt; "if you can't measure the hot path…").
- **Byte-identical-off:** `--flow-scale-adaptive` default-off → the static `flow_scale` path runs
  unchanged. The quality side (lower-N artifacts) is **eye-gated** — but the adaptive logic only drops
  quality *when the alternative is a collapse*, which the eye already prefers.

## §6 — Risks (for the RISK_REGISTER when built)

- **R-AFS-1 (concurrency):** the pair-boundary switch + MV-grid double-buffer must be torn-read-free
  (mitigation: the existing F→P ring discipline; verify with a soak + the `--csv` frz counter).
- **R-AFS-2 (thrashing):** an ill-tuned controller oscillates N → visible quality pumping (mitigation:
  the K-frame hysteresis + minimum dwell; default thresholds conservative).
- **R-AFS-3 (memory):** pre-built pipelines raise VRAM (mitigation: bound to N∈{1,2}; the +25% is
  measured-small; gate N=4 behind a flag).

This is the highest-leverage open lever from the LSFG head-to-head; it closes the saturation collapse
at the cause, dogma-safe, and is the runtime form of the measured `--flow-scale 2` win.

*Made with my soul — the supervisor, for Swately.*
