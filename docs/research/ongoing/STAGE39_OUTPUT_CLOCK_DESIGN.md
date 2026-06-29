# STAGE-39 OUTPUT-CLOCK — design (the present clock becomes the monitor's)

**Date.** 2026-06-10. **Status.** PROPOSED — design only (no code touched). **Type.** Design (explanation + how-to).
**Parent.** [`RENDER_ASSISTANT_PLAN.md`](../RENDER_ASSISTANT_PLAN.md) §9 + §9-coda. **Sibling kickoffs.**
[`ongoing/RENDER_ASSISTANT_KICKOFFS.md`](RENDER_ASSISTANT_KICKOFFS.md) (STAGE-39c, 40a — the generation-ring + B-copy levers this builds atop).

The key words MUST, MUST NOT, SHOULD, SHOULD NOT, MAY are to be interpreted per BCP 14 (RFC 2119/8174).
Every code anchor below was read first-hand in `apps/render_assistant/src/main.cpp` at the cited line; line
numbers are a snapshot (2026-06-10) and will drift — the function/variable names are the durable reference.
Numbers tagged **MEASURED** come from PLAN §9 / its coda (operator runs). Everything else is **ESTIMATED** and
labelled so; this document fabricates no benchmark.

---

## §0 — The defect, stated precisely

Today the present clock is the **capture/F clock**, not the monitor. Thread P (`thr_p_fn`, ~line 1553) wakes on a
new capture (`c_cv.wait_for`, ~line 1673), reads the set F published, and paces that set's `N_set` presents over
`T_set = src_interval_ema_ms × span` from a fixed-timestep anchor (`anchor_ms`, ~line 1555; advance ~line 1804).
The swapchain runs in **MAILBOX or IMMEDIATE** (`pick_present_mode`, ~line 553 — `low_latency` true for the
borderless/overlay window, so FIFO is never chosen for the live path). So we emit `≈ N × src` presents/s that are
**not phase-locked to the panel's vblank**.

Two failure modes follow, both **MEASURED** (PLAN §9-coda, the operator's slow-motion camera):

1. **The beat.** With output rate `R_out = N·src` free-running against the panel `R_panel` (e.g. 240 Hz), the
   instantaneous phase between a present and the next vblank drifts at `|R_panel − R_out|` Hz. When two presents
   land inside one refresh interval, the earlier is **discarded** (never scanned out); when none lands, the prior
   frame is **repeated**. Both are visible as a periodic hitch — the operator's "saltos".
2. **Slip pile-up.** `paced_wait_P` (~line 1570) targets even spacing, but the present submit itself (G-queue
   blit+present under `g_q_mtx`, `do_present_g_P` ~line 1642) takes non-zero, jittery time; when a cycle runs long
   the next present is late, and a burst of catch-up presents amontonan into one refresh → the discard case again.

**Operator metric (MEASURED).** % unique transitions in a slow-motion capture of the output: **LSFG-adaptive 84 %
with uniform steps; ours 74.6 %** after the STAGE-39c content fix. The gap is *uniformity*, not content
correctness — the frames are right (39c's checksum gate), they land on the wrong refreshes.

**Why LSFG does not have the beat.** Its adaptive mode **fixes the OUTPUT** to the panel and *chooses the
multiplier* (and the per-tick phase) to fit — output-clocked by construction. We are input-clocked. STAGE-39
inverts that: **the present clock becomes the monitor's; each tick selects/synthesises the capture-timeline phase
that belongs at that tick's display time.** A fractional multiplier becomes natural, and the beat becomes
**impossible by construction** (there is no second free-running rate to beat against).

---

## §1 — Architecture: how P derives the output clock

The output-clock loop replaces P's "wake on capture, burst N presents" structure with "wake on **vblank**, present
**exactly one** frame whose phase = now". Three mechanisms can supply the vblank tick; they differ in *how P learns
a refresh happened* and in residual jitter. All three keep F and C unchanged (F still builds sets capture-paced;
only P's consumption changes).

### Option A — FIFO present mode as the natural vblank lock (RECOMMENDED for G-present / iGPU)

`vkQueuePresentKHR` into a **FIFO** swapchain blocks the next `vkAcquireNextImageKHR` until an image frees, which
on a 2–3-image swapchain (`swap_create`, ~line 512: `minImageCount+1`) happens **at a vblank**. So a P loop of
`{ acquire; build present phase; submit; present }` with FIFO self-paces to exactly `R_panel` with **zero added
timer code** — the driver's vblank IS the clock. This is the present-wait already inside Vulkan.

- **Trade-offs (ESTIMATED unless tagged).** Pro: exact phase lock, no beat, no `paced_wait_P` spin needed (frees
  P's dedicated-core busy-wait, ~line 1576). Con: FIFO latency is up to one refresh of queue depth vs MAILBOX; at
  240 Hz that is ≤4.17 ms — small against the **MEASURED** end-to-end lat 20–32 ms (§9-coda). Con: FIFO gives
  **no slack signal** — if a present is submitted late, FIFO simply shows it one refresh later (a silent repeat),
  so P MUST still measure its own phase error to drive auto-N (§3).
- **Why G/iGPU first.** The iGPU present path (`do_present_g_P`, ~line 1642) is already the operator's overlay
  default (PLAN §9, STAGE-33b: G-present lat 21–22 vs A 22.5–25 ms, **MEASURED**) and presents to its *own*
  attached output (no DWM cross-adapter copy). A FIFO swapchain on G is the cleanest, lowest-jitter vblank source
  we have. The G-queue is shared with C's convert under `g_q_mtx` (~line 1210) — but at one present/refresh the
  contention is far below STAGE-37's burst case.

### Option B — `VK_KHR_present_wait` / waitable swapchain (RECOMMENDED only if a fractional offset is wanted)

`vkWaitForPresentKHR` blocks until a given `presentId` has been **presented** (i.e. hit the screen), returning the
actual present time. This lets P keep a **low-latency** present mode (MAILBOX/IMMEDIATE) yet still learn each
vblank precisely, and schedule the *next* phase a controlled fraction ahead of the measured vblank.

- **Trade-offs.** Pro: vblank-accurate timing without FIFO's queue latency; gives the slack signal FIFO withholds.
  Con: extension support is **not guaranteed** on the AMD iGPU path — `present_wait` MUST be probed at device
  create (alongside `pick_present_mode`) and the code MUST fall back to A or C when absent. Con: more plumbing
  (presentId bookkeeping, a `VK_KHR_present_id` companion). Recommendation: **defer** — adopt only if Option A's
  fixed FIFO phase proves too laggy in the operator's feel test, which the **MEASURED** lat budget says is
  unlikely.

### Option C — high-resolution timer + IMMEDIATE (the universal fallback; RECOMMENDED for A-present)

Keep IMMEDIATE present and drive the tick from a software clock locked to the measured refresh period: P sleeps to
`tick_k = t0 + k·(1/R_panel)` using the existing precision pacer (`paced_wait_P` already does coarse-sleep +
`hal::cpu_wait_for_ns` spin-finish, ~line 1570; `timeBeginPeriod(1)` is set, ~line 570). The refresh period is
read once from the OS (`DXGI_MODE_DESC.RefreshRate` via the existing `d3d_init` enumeration the STAGE-33 monitor
code already touches) or a `--refresh-hz` override.

- **Trade-offs.** Pro: works everywhere, no extension, present device-agnostic — the right choice for the **A-present**
  path (the 4090 swapchain `do_present_P`, ~line 1620), where FIFO would re-introduce present-tax on the saturated
  game GPU and a software clock keeps that tax a single present/tick. Con: the software clock is *open-loop* — it
  does not see real vblanks, so it slowly drifts against the panel (a slow residual beat at `|R_timer − R_panel|`,
  the very defect we removed). Mitigation: phase-correct the timer against `present_wait` (Option B) when present,
  or against the DWM `DwmGetCompositionTimingInfo` vblank count on Windows (a **read-only** sync source — no driver
  hook, AC-safe per §0's external constraint). With phase correction this collapses to Option B's accuracy.

### Recommendation (the per-path verdict)

| present path | clock source | rationale |
|---|---|---|
| **G-present (iGPU)** — overlay default | **Option A (FIFO on G)** | vblank lock for free; lowest jitter; G owns the output (no DWM copy). The default STAGE-39 mode. |
| **A-present (4090)** | **Option C (timer + IMMEDIATE)** + DWM phase-correct | avoids FIFO present-tax on the saturated game GPU; software clock kept honest by the read-only DWM vblank. |
| (optional, later) | **Option B (`present_wait`)** | if the feel test wants MAILBOX latency *and* exact vblank; probe-gated, deferred. |

`--output-clock fifo|timer|off` (default `off` until validated) selects the mode; `auto` picks FIFO when the
present device is G and exposes FIFO (always), timer when A.

---

## §2 — Phase selection per tick (the fractional multiplier)

This is the heart of the inversion. At each output tick the present time is `t_tick` (the vblank, or the timer
target). P must present the capture-timeline phase that **belongs** at `t_tick`.

### §2.1 — The timeline P already has

F publishes, per set, everything needed (the `f_pair_*` arrays, written before the `f_seq` bump, ~line 1513):
`f_pair_cseq_a` (the pair's *cur* capture seq), `f_pair_tcap_a` (its capture wall-time, ms), `f_pair_span_a`
(source frames the pair covers), `f_pair_n_a` (N actually built). P reads `src_interval_ema_ms` (the **arrival**
cadence EMA, ~line 1687/1695 — already the right clock, fed by `arr_delta_us`, never the processed cadence).

So the capture timeline is sampled at known wall-times: the pair `[prev, cur]` represents motion from
`tcap − span·T_src` to `tcap`. A phase `t ∈ [0,1]` across the pair maps to wall-time
`t_wall(t) = tcap − span·T_src·(1 − t)`.

### §2.2 — The fractional phase math

Define a **presentation anchor** `t_anchor` = the wall-time we are currently showing on the capture timeline
(initialised to the first pair's `tcap`). At each output tick:

```
t_present_wall = t_anchor + (t_tick − t_tick_prev)          // advance the shown-time by one real refresh period
phase_global   = (t_present_wall − tcap_prev) / (span·T_src) // position within the current pair, in [0,1]
```

When `phase_global` exceeds 1, the current pair is exhausted → advance to the next published set (the freshest
`f_seq` whose `tcap_prev ≤ t_present_wall`), recompute. The key change vs today: **P no longer emits a fixed
`N_set` presents per pair**; it emits *however many ticks fall inside that pair's wall-time span*, which is
`span·T_src / (1/R_panel) = span·T_src·R_panel` ticks — a **fractional** number absorbed by the running
`t_anchor`. At `src=90`, `R_panel=240`: 2.67 ticks/pair (fractional — the beat source today, now just an anchor
remainder). At `src=80`, `R_panel=240`: exactly 3.0. The multiplier is **derived, not commanded**.

### §2.3 — From phase to pixels: nearest vs synthesis

`phase_global` is a continuous phase; the question is what to show for it. Two strategies, in the order they SHOULD
be tried:

**(2.3a) Nearest pre-generated phase — RECOMMENDED FIRST (zero GPU cost).** F already builds `NI_set` discrete
interps at phases `k/N_use` plus the two reals at 0 and 1 (the warp loop, ~line 1473; `record_warp_only(cmd,
float(k+1)/N_use)`, ~line 1476). P picks the buffer whose phase is closest to `phase_global`:
`k* = round(phase_global · N_set)` clamped to `[0, N_set]`, presenting `hostI[f_gen][k*−1]` for interior k or the
real (`hR_*`) for the endpoints. This is a pure index change inside the present cycle — **no new GPU work, no new
buffers**. Cost: **zero** added per tick (one extra `round` + clamp). Quality: the phase is quantised to the
nearest `1/N` grid → a residual temporal error up to `½·(1/N)·span·T_src`. At N=8, src=80: `½·(1/8)·12.5 ms ≈
0.78 ms` worst-case temporal jitter — **ESTIMATED** below the **MEASURED** present jitter we already tolerate.
This alone removes the beat (the *tick* is vblank-locked; only the *content* is quantised) and SHOULD lift the
uniqueness metric materially because every refresh now gets exactly one, monotonically-advancing frame.

**(2.3b) On-demand synthesis — the second lever (cost B, MEASURED-adjacent).** When the nearest-grid error is
judged too large (high N already mitigates it), P (or better, a dedicated synthesis on B) re-warps at the *exact*
`phase_global` via `record_warp_only(cmd, phase_global)` — the pillar already accepts an arbitrary `t ∈ (0,1)`
(`OpticalFlowPipeline.hpp:135`, "Re-dispatches the warp at a different temporal phase t, reusing the MV and SAD").
Cost per synthesised tick: one warp + one copy-out. **MEASURED proxy:** PLAN §7/STAGE-17 puts pillar warp at far
below the block-match; a `record_warp_only` re-dispatch is **MEASURED** in §9 as "cheap" (5.8–6.0 ms sustained TWO
re-dispatches at fg-factor 3, so ~one warp ≈ **ESTIMATED** ≤1.5 ms on the 1080 Ti at 1080p, plus the ~2.9 ms x4
copy-out that STAGE-40a is moving off the critical path). The catch: synthesis needs the pair's MV/SAD resident,
which lives on B, not on P's present device — so on-demand synthesis MUST run on **B keyed by the tick**, not on P.
That is a larger structural change (B becomes tick-driven for the active pair) and is therefore **deferred to a
later sub-stage**; nearest-phase (2.3a) is the STAGE-39 deliverable.

**Quantified comparison (per the efficiency mandate).**

| strategy | added GPU cost/tick | temporal error | when |
|---|---|---|---|
| nearest pre-generated (2.3a) | **0** | ≤ ½·(span·T_src)/N — **ESTIMATED** ~0.78 ms @N8/src80 | STAGE-39 default; raise N to shrink error |
| on-demand warp (2.3b) | ~1 warp + 1 copy-out — **ESTIMATED** ≤1.5 ms + the x4 leg (40a overlaps it) | ~0 (exact phase) | only if 2.3a's grid error is perceptible at the max feasible N |

The honest read: **2.3a is almost certainly sufficient** because raising N (auto-N already pushes toward the cap,
§3) shrinks the grid error faster than synthesis would help, at zero cost. Synthesis is the fallback if the
operator's slow-mo metric still shows quantised steps at high N.

---

## §3 — Interactions with the existing machinery

The output-clock loop MUST compose cleanly with the five mechanisms already in P/F. Each is addressed:

**(R1) span pacing (`f_pair_span_a`, the §35-R1 walking-tremble fix).** Today P stretches a span-S set's presents
over `S·T_src` (`T_set`, ~line 1762; `anchor_step`, ~line 1763). Under the output clock this becomes **automatic
and exact**: the §2.2 wall-time map already multiplies by `span·T_src`, so a span-2 pair simply occupies twice as
many ticks — no separate pacing path. R1's plumbing (the published `span`) is *reused as input* to the phase map;
its bespoke pacing arithmetic in P is **superseded** by the tick loop. This is a simplification, not just a
co-existence.

**(R4b) auto-N (`live_n_f`, the feasibility/dwell logic ~line 1520).** Unchanged on the F side — F still chooses N
by measured build capacity. The output clock changes what N *means* for P: N is now only the **grid density** for
nearest-phase selection (§2.3a), not the present count. So auto-N's objective gains a second pull: **higher N =
finer phase grid = lower 2.3a quantisation error**, independent of output rate. The §3 recommendation: when the
output clock is active, auto-N SHOULD bias *upward* (push to the cap whenever build budget allows), because output
rate is now decoupled from N (the panel fixes it) — N only buys phase resolution. The existing dwell/dead-band
(~line 1531) prevents oscillation; no change needed, only a comment that the upward bias is now desirable.

**(39c) the `p_presenting` guard + `kGenRing=3` generation ring.** STILL REQUIRED and slightly more exercised. The
output clock can hold a pair across *more* ticks than today (a slow source at a fast panel), so P dwells on one
`f_gen` longer → F is more likely to catch up to the ring-overwrite hazard. The guard (`p_presenting.store(cur_f)`
~line 1745; F's wait `p_presenting==fs+1−kGenRing` ~line 1407) handles exactly this and MUST be preserved verbatim.
P MUST publish `p_presenting` for the pair it is *currently sampling* (the one `phase_global` is inside), updated
each time §2.2 advances to a new set — the store moves from "per present-cycle" to "per pair-advance". `kGenRing`
MAY need to rise if the panel/source ratio is large and a pair spans many ticks; **ESTIMATED** `kGenRing=3`
suffices for the common 240/80 = 3 case, but the gate (§4) MUST check it at the fg8/30fps extreme (8× ratio).

**(source stalls — what does a content-less tick present?).** This is the output clock's defining new question:
the panel ticks even when no new capture arrived. Three behaviours, in increasing sophistication:

- **freeze (STAGE-39 default, MUST):** when `phase_global` would exceed the newest available pair's `[0,1]`, clamp
  to `phase=1` (the newest real) and re-present it. The panel keeps ticking at full rate (no beat), the *content*
  holds — exactly today's "repeat" but now **deliberate and phase-aligned** rather than an accidental vblank
  collision. Honest: a true stall still looks frozen, but it no longer adds a *beat* on top of the freeze.
- **clamp-and-hold with extrapolation flag (future, STAGE-40c stall-filler):** instead of freezing at `phase=1`,
  *extrapolate* past it via single-source forward-warp `B[x − Δ·mv]` (the STAGE-31 extrapolation bench mechanism,
  KICKOFFS §STAGE-31). The output clock is the *natural consumer* of extrapolation: it asks "give me phase
  `1+Δ`" and the warp obliges, hiding a 1–2 tick stall as smooth motion. This is the documented future tie-in —
  **deferred**, but the output-clock phase API (`record_warp_only(t)` accepting `t>1`) is exactly its insertion
  point. Honest caveat (STAGE-31): extrapolation has disocclusion holes and no agreement gate → strictly lower
  quality; it is a stall *mask*, not a free frame.

**Latency.** The output clock adds **at most one refresh** of scheduling latency (the tick granularity) — ≤4.17 ms
@240 Hz, ≤8.33 ms @120 Hz — on top of the **MEASURED** 20–32 ms pipeline. Stated honestly in the report; it is the
LSFG-class tradeoff and well within the operator's accepted band.

---

## §4 — Implementation plan (staged, anchored, gated)

All changes are in `apps/render_assistant/src/main.cpp` unless noted. Discipline (every kickoff's): no explicit
`std::memory_order_*` (lint_hal blocks); preserve the file's protected last line and CMakeLists' last line; build
**both** toolchains yourself (MinGW `build-ra-mingw`, MSVC vcvars64 VS2022 BuildTools `build-ra-msvc2`); NO
commits; NO INVENTORY edits; the supervisor verifies on the rig and commits.

### STAGE-39d — nearest-phase output clock, timer mode (Option C), G + A paths

**Why timer-first:** it is device-agnostic and isolates the *phase-selection* change from the *vblank-source*
change — debug one variable at a time.

- **New state (in `main`'s thread block, near the other atomics ~line 1199):** none cross-thread beyond reusing
  `f_pair_*`. P-local: `double t_anchor` (shown capture wall-time), `double tick_period_ms` (= 1000/R_panel),
  `uint64_t cur_pair_seq` (which `f_seq` the anchor is inside).
- **New CLI (in `Config`, ~line 83, + `parse_args` ~line 174):** `--output-clock off|timer|fifo|auto` (default
  `off`), `--refresh-hz <N>` (override; else read the present monitor's `RefreshRate` in the STAGE-33 enumeration).
- **Changed function — `thr_p_fn` (~line 1553):** replace the wake-on-capture + burst-N body (~line 1670–1796)
  with, under `--output-clock timer`: a loop ticking on `paced_wait_P(tick_k)` where `tick_k = t0 + k·tick_period_ms`;
  per tick compute `phase_global` (§2.2) from the freshest published set whose `tcap_prev ≤ t_present_wall`; pick
  `k* = round(phase_global·N_set)`; present that one buffer via the existing `do_present_g_P`/`do_present_P`. Keep
  `paced_wait_P` (~line 1570) verbatim — it is the timer. Keep the slip/lat/iter stats (~line 1808) but redefine
  `iter` as the tick cycle. Add a stat `uniq/s` (count of ticks whose presented `(pair_seq,k*)` differs from the
  previous tick's — the **direct in-app proxy for the operator's slow-mo uniqueness metric**).
- **Preserve:** the `--fg-factor 1`/passthrough early-out (~line 1707) — at N=1 the output clock degenerates to
  "present the newest real each tick" (still beat-free); the 39c `p_presenting` store moves to pair-advance
  (§3); the 39b `--dump` path (the buffer chosen by `k*` is what gets dumped).
- **Gate (measurable):** (a) both toolchains build; (b) `--output-clock timer --refresh-hz 240` on a 240 Hz
  panel: present rate pins at ~240 ±2 (not `N·src`), `slip` avg ≤2 ms, no validation errors; (c) the in-app
  `uniq/s` rises toward `min(R_panel, motion-distinct phases)`; (d) **operator gate (the real one):** slow-mo
  camera uniqueness % rises from the **MEASURED** 74.6 % toward LSFG's 84 % at matched N. Report `uniq/s` vs the
  camera % so the proxy is calibrated.

### STAGE-39e — FIFO vblank lock (Option A) for the G-present path

- **Changed:** `pick_present_mode` (~line 553) gains an output-clock awareness — when `--output-clock fifo|auto`
  and present device is G, return `VK_PRESENT_MODE_FIFO_KHR` (the natural lock); print it (the existing print
  ~line 1177). The P loop under FIFO **drops the timer**: the loop body becomes `{ pick phase for "now"; present }`
  and the `acquire→present` blocking IS the pacer (no `paced_wait_P`). "Now" for FIFO = the measured time the
  previous present returned (FIFO already paced it to vblank).
- **Risk:** FIFO + a borderless topmost overlay historically caused the STAGE-27a **acquire stall** (the comment
  at ~line 1166 documents it). MUST re-verify FIFO does not stall the overlay on G; if it does, fall back to timer
  (Option C) and document the wall. This is the single highest-risk item.
- **Gate:** G-present FIFO holds ~R_panel with **lower** slip than timer mode (the driver is more precise than our
  spin), no acquire stall over a 60 s overlay run, lat within +1 refresh of timer mode.

### STAGE-39f (optional, deferred) — `present_wait` phase feedback (Option B) / DWM phase-correct for timer

- Probe `VK_KHR_present_wait`+`present_id` at device create (next to the present-mode probe); if present, feed the
  measured vblank time back to correct `t0` drift in timer mode (closes Option C's open-loop gap). On Windows
  without the extension, use read-only `DwmGetCompositionTimingInfo` as the vblank reference (AC-safe — no hook).
- **Gate:** timer-mode residual drift (the slow secondary beat) measured ~0 over 5 min; only pursued if 39d/e show
  a residual drift the operator can see.

### STAGE-39g (deferred, gated on need) — on-demand exact-phase synthesis (§2.3b)

- Only if the operator's slow-mo metric still shows quantised steps at the max feasible auto-N. Move synthesis to
  **B keyed by the tick** (B re-warps the active pair at `phase_global` via `record_warp_only(cmd, phase_global)`,
  `OpticalFlowPipeline.hpp:135`); composes with STAGE-40a's overlapped copy-outs (the copy-out is the cost, and
  40a is already moving it off B's critical path). Substantial — its own stage; nearest-phase is the bar it must
  beat.

### Risks (consolidated, honest)

| risk | severity | mitigation |
|---|---|---|
| FIFO acquire-stall on the overlay (STAGE-27a regression) | HIGH | timer-mode fallback; gate 39e on a 60 s overlay run |
| timer open-loop drift re-introduces a slow beat | MED | DWM/`present_wait` phase-correct (39f); measure residual drift in the 39d gate |
| nearest-phase grid too coarse at low N (small src or capped build) | MED | auto-N upward bias (§3); 39g synthesis as the last resort |
| `kGenRing=3` insufficient when panel/source ratio is large (pair spans many ticks) | MED | the §4 gate checks fg8/30fps (8× ratio); raise `kGenRing` if `p_presenting` guard fires (it is the explicit signal) |
| present-tax returns on A-present FIFO (saturated 4090) | LOW | timer+IMMEDIATE is the A-path recommendation (§1), not FIFO |
| stall freeze still reads as frozen (no extrapolation yet) | LOW-by-design | honest: freeze is correct-but-static; STAGE-40c extrapolation is the future mask, not in scope |

---

## §5 — One-paragraph honest summary (for the report)

The output clock makes the **present cadence the panel's** and selects, per refresh, the capture-timeline phase
due at that refresh — the multiplier becomes a derived fraction and the beat (`|R_panel − N·src|`) becomes
structurally impossible, matching LSFG-adaptive's defining property. The **MEASURED** defect it targets is the
74.6 %-vs-84 % uniqueness gap (PLAN §9-coda). The recommended first build is **nearest-phase selection over the
already-generated `1/N` grid** (zero added GPU cost — a pure index change in `thr_p_fn`), with the vblank supplied
by **FIFO on the G/iGPU present path** and a **DWM-corrected software timer on the A/4090 path**. Span pacing (R1)
is *subsumed* by the wall-time phase map; auto-N (R4b) is *repurposed* as phase-grid density and SHOULD bias up;
the 39c ring guard is *preserved and slightly more exercised*; a content-less tick **freezes at the newest real**
today and is the **natural insertion point for STAGE-40c extrapolation** later. On-demand exact-phase synthesis
(§2.3b) is the deferred fallback, justified only if high-N nearest-phase still shows quantised steps. The only
numbers stated as fact here are the operator-MEASURED ones from §9; the per-tick cost and grid-error figures are
explicitly **ESTIMATED** and must be measured at the §4 gates before any are claimed.
