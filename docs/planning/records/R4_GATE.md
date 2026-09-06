# R4 — gate record (G-R4, milestone M-R4): STAGE 6 PRESENT extracted · 2026-09-05

> Stage R4 of [`../CONVERGENCE_MASTER_PLAN.md`](../CONVERGENCE_MASTER_PLAN.md) §3 (strategy X12 of
> `../CONVERGENCE_IMPLEMENTATION_STRATEGIES.md`). Every number below is quoted from command output captured in the
> session; the raw logs live in the session scratchpad (ephemeral). Tree: `cbba9c3` + the R4 working tree.
> **Verdict: see §6.**

## 0 · What R4 was asked to be (the contract, re-anchored first)

`STAGE_CONTRACT.md` stage 6 + X12: `src/present/present_stage.hpp/.cpp` owning the `PresentSurface`, the bridge
slots, the async-present machinery and the present chokepoint, called by the P loop with the tick decision,
returning `FlipStats`; the tick decision `{Warp, Dup, Drop, Decimated}` DECLARED (today three inline sites:
`fdrop`, the async drop, the decimation gate); the guard-band drop of a late target added ONLY if no equivalent
exists; MUST NOT change: the drop decision P-local (no lock), the slots provisioned at init, every poll
`vk_live`-wrapped (MR-1), own-window only (MR-7), one submitter per queue (MR-2), reference-only binding (CR1).
**Gate G-R4:** `disp_phase` / `disp_src` and presents/s byte-/spread-identical to pre-R4 on the replayed source
(2 runs/side); under `tools/gpu_load.exe`: drops/s > 0, real frames dropped = 0; validation clean 30 s soak; a
forced TDR (`--tdr-test N`) → clean `g_quit` exit.

**Every offset the plan cites was stale** (the plan said so itself: "R4 must re-anchor every offset"). The
present-side sites, re-anchored 2026-09-05 in the pre-R4 `present.cpp` (3,085 lines): the surface block
`:406-447`, `ps_account` `:459-470`, `bridge_present` `:485-525`, `bridge_present_src` `:673-714`, the slots
`:922-933`, the async preamble `:979-986`, the slot choice `:988-1002`, the submit tail `:1403-1407`,
`--shallow-queue` `:1635-1650`, the present tail `:1651-1660`, `rfp_present` `:1678-1720`, the decimation gate
`:2237`, `fdrop` `:2518-2535`, step 7 `:2637-2646` (WAP) / `:2979-3007` (grid), the per-second stats
`:2752-2943` / `:3012-3070`.

## 1 · What moved, what was declared, what stayed

| | Before | After |
|---|---|---|
| `src/present/present.cpp` | 3,085 lines | 2,952 lines; the loop CALLS the stage through nine methods |
| `src/present/present_stage.hpp` | — | the stage-6 contract: `Decision`, `BridgeSlot`, `FlipStats`, `Tick`, `PresentStage` |
| `src/present/present_stage.cpp` | — | 310 lines: the eight moved bodies + `flip_stats()` + the `--tdr-test` trio |
| `shaders/tdr_hang.comp`, `--tdr-test N` | — | the forced-TDR instrument (built; **not run** — §5) |

**Moved (bodies extracted by anchor from the real file by `scratchpad/r4_patch.py`, never retyped):** the
surface creation → `init()`; `ps_account` → `account()`; `bridge_present` (the `--pace-hard` pin + the slot-0
submit) → `present_front()`; the async preamble → `poll_inflight()` — **one body for the two callers** (the warp
lambda's and `rfp_present`'s copies were the same text minus the mass read; rfp passes `nullptr`); the slot
choice → `begin(decision, count_drop)`; the sync/async submit tail → `submit()`; the grid path's shared-fence
submit → `submit_sync()`; `--shallow-queue` → `shallow_queue()`; the present tail → `present_tick()`.

**Declared:** the tick decision is an INPUT of `begin()` — the loop hands `Warp`, or `Dup` on an exact-duplicate
`--fdrop` tick (`do_warp=false`), and the stage returns `Drop` when a warp is still in flight (the legacy "async
re-present drop", `rdrop_ticks`); `Decimated` is the absence of a call — the decimation gate `continue`s before
any stage runs, now saying so in place. **`FlipStats`** (`{have_flip, sync_qpc, present_count, ps_ok/timeout/err,
device_lost}`) is produced by `flip_stats()` and CONSUMED by the CSV row's `MsBetweenDisplayChange` (both sites),
replacing the direct `last_flip_qpc()` read — the 6 → 4 feedback edge of STAGE_CONTRACT §0 now exists as a call;
`PhaseClock` does not read it yet (no consumer today; declared, not speculative — the CSV is its consumer).

**Aliased, not rewritten:** the loop keeps its former local names as REFERENCES to the stage's fields
(`ra_surface`, `surface_ready`, `ps_ok/timeout/err`, `rdrop_ticks`, `ph_tgt/ph_held/ph_overshoot`, `bslot`,
`async_front`, `sq_hits/misses`), so the ~40 read sites in the stats, the CSV, `--rfp-fresh` and
`--motion-fallback` are byte-identical text. Three aliases fell out unused (`ph_w_ema`, `async_inflight`, the
warp lambda's `back`) and were removed — the compiler named them.

**Stayed in the loop (declared residual):** the per-second stats blocks (`stats_second()` in X12) — ~190 lines
printing ~30 P-local counters; moving them means binding every counter, which is R4's second step, not
skipped silently; `bridge_present_src`'s recording (copy + overlay + badge + blit: the grid path's own
scaling); `rfp_present`'s recording (the real-frame copy + blit); the `--fdrop` decision itself (stage 4's
business, it now feeds `begin()`); the slot OBJECTS (`present_init.cpp`, bound into `pres.bslot[]` at the same
place as before).

**The guard-band drop (MINIMAL_FG S8):** NOT added. First-hand read: the legacy has no late-target drop; the
plan allowed adding it only behind a flag, measured — that is a behaviour, not an extraction, and it belongs
to a measured decision after R4's gate (not taken here).

## 2 · Verbatim + the documented transforms

`scratchpad/verbatim_r4.py` over `present_stage.cpp` vs the pre-extraction snapshot (whitespace-trimmed lines;
the NEW code — the constructor, `flip_stats`, `submit_sync`, the `--tdr-test` trio — and the alias/scaffold lines
counted apart):

```
present_stage.cpp: 310 lines total | 229 scaffolding | 81 moved-body lines
VERBATIM: 68 / 81 = 83.95 % of the moved body appears byte-identical (whitespace-trimmed) in the pre-extraction present.cpp
non-matching moved-body lines (13) — each must be a DOCUMENTED transform:
```

| # | Transform (the 13 lines) | Why it is safe |
|---|---|---|
| T1 | `init()`: the two `return;` → `return false;` (2 lines) | the enclosing thread body returned; the caller now does (`if(!pres.init(...)) return;`) — same globals set, same prints, same exit |
| T2 | `present_front()` / `present_tick()`: `ps_account(` → `account(` (2 lines) | the lambda became the method; same body |
| T3 | `begin()`: `if(ap && do_warp && …) ++rdrop_ticks;` → gated on `count_drop` (1 line) | the warp path counted the drop, `rfp_present` never did — the flag reproduces both call sites exactly |
| T4 | `submit()`: `ap`/`fBridge`/`back` → `tk.ap`/`tk.fence`/`tk.back` (3 lines) | the lambda's locals became the Tick's fields, filled by `begin()` from the same expressions (the RHS lines of `begin()` are verbatim) |
| T5 | `shallow_queue()`: the same three renames (3 lines) | as T4 |
| T6 | `present_tick()`: `if(!ap)` → `if(!tk.ap)`; `bridge_present()` → `present_front()` (2 lines) | as T4; the lambda became the method |

Plus one dedup outside the measure: `rfp_present`'s async preamble (4 lines, the warp preamble minus the mass
read) is now the same `poll_inflight(nullptr)` call — `presented_out == nullptr` skips exactly the line rfp lacked.

## 3 · Cold checks

| Check | Result (quoted) |
|---|---|
| build | `build-release.bat` exit 0 after the extraction; `[1/16] glslc tdr_hang`; `present_stage.cpp.obj` compiled |
| **new warnings** | the warning set of the R4 build, minus the pre-R4 build's, by message: **empty** (`comm -13` of the two sorted sets printed nothing). Two transient ones were fixed on the way: `FlipStats::present_count` was `uint64_t` against the pillar's `uint32_t` (C4244 ×2 at the CSV sites) → the pillar's type; three unused aliases (C4189) removed. |
| smoke, 12 s, the default set | `[ra] present-surface: async-present slot-1 ready (2 bridge slots; non-blocking present + drop-interpolated)`; `[ra] present: PresentSurface OWN-WINDOW flip plane …`; `bounded-run clean exit: total_presents=2878 (--duration 12s …)` |

## 4 · The A/B (A = the pre-R4 binary `cbba9c3`, md5 `6f6d9360…`; B = R4; the ball zoo 60 fps 1920×1080, 60 s, `--csv`)

### 4.1 · The default set, interleaved A,B,A,B (DI-3: 2 runs/side; `r4_parse.py` over the CSVs)

| metric | A run 1 / run 2 | B run 1 / run 2 | A mean (spread) | B mean (spread) | Δ B−A |
|---|---|---|---|---|---|
| presents / 60 s | 14,384 / 14,385 | 14,384 / 14,386 | 14,384.5 (1) | 14,385.0 (2) | +0.5 |
| `disp_phase` mean | 0.5010 / 0.5012 | 0.5013 / 0.5012 | 0.5011 (0.0003) | 0.5013 (0.0000) | +0.0002 |
| `disp_phase` sd | 0.2814 / 0.2819 | 0.2818 / 0.2818 | 0.2817 (0.0005) | 0.2818 (0.0001) | +0.0001 |
| pinned fraction (t = 0 or 1) | 0.0122 / 0.0145 | 0.0140 / 0.0138 | 0.0133 (0.0023) | 0.0139 (0.0001) | +0.0006 |
| `disp_src` step (source frames / present) | 0.2495 / 0.2496 | 0.2495 / 0.2495 | 0.2495 (0.0001) | 0.2495 (0.0000) | −0.0000 |
| uniq / s | 239.1 / 239.0 | 238.9 / 239.0 | 239.02 (0.08) | 238.99 (0.10) | −0.03 |
| `MsAddedLatency` (EMA) | 20.40 / 20.42 | 20.61 / 20.52 | 20.41 (0.02) | 20.56 (0.09) | +0.15 ms |
| `MsBetweenDisplayChange` median | 4.171 | 4.171 | | | 0 |
| `ps … ok / to / er` (a mid-run line) | `ps 240/s ok=2700 to=0 er=0` | `ps 240/s ok=2700 to=0 er=0` | | | identical |
| CSV rows | 14,387 / 14,387 | 14,387 / 14,387 | | | |

Every placement number (phase mean/sd, pinned fraction, source step, uniq/s) is inside A's own run-to-run spread
or within 0.0002 of it; presents Δ +0.5 on a spread of 1–2. **Latency: B +0.15 ms (0.7 %)** — larger than THIS
pair's spread (0.02 ms, n = 2) and far inside the run-to-run latency spread every earlier A/B measured on this
rig (R0: 2.26 ms; R2: inside spread). Reported as measured, not rounded away; by DI-3 it is not a signal at n = 2.
Adjacent instrument reported: the four `--pace-hard` / `--rfp` / `--shallow-queue` runs below have B faster on
one and slower on two — consistent with noise around 20.5 ms.

### 4.2 · The touched paths, 1 run/side (reliability not measured — one run each; the numbers are the deltas)

| path | presents A / B | phase mean A / B | phase sd A / B | src step A / B | uniq/s A / B | lat A / B |
|---|---|---|---|---|---|---|
| `--shallow-queue` (the early promote → `shallow_queue()`) | 14,384 / 14,384 | 0.5007 / 0.5006 | 0.2811 / 0.2810 | 0.2496 / 0.2496 | 239.03 / 239.02 | 21.08 / 21.14 |
| `--rfp` (the real-frame present → `begin(Warp, count_drop=false)` + `submit` + `present_tick`) | 14,386 / 14,384 | 0.5013 / 0.5010 | 0.2820 / 0.2815 | 0.2496 / 0.2496 | 239.04 / 238.91 | 20.21 / 20.85 |
| `--pace-hard` (the pin → `present_front()`) | 14,384 / 14,386 | 0.5014 / 0.5013 | 0.2820 / 0.2820 | 0.2495 / 0.2496 | 238.93 / 239.07 | 20.60 / 20.43 |
| `--no-warp-at-presenter` (the grid path → `submit_sync()` + `present_front()`; **no CSV row and no self-bound on this path — a legacy defect, §4.4**; compared on the per-second stats line) | `242.3 fps (present) … uniq 127/s … lat 11.1ms … ps 242/s ok=16380 to=0 er=0` (75 s, external stop) | | | | | `242.3 fps (present) … uniq 127/s … lat 12.4ms … ps 242/s ok=16650 to=0 er=0` (75 s, external stop) |

### 4.3 · Under `tools/gpu_load.exe` (B, 60 s) and the `--validation` soak (B, 30 s, under the same load)

| item | result |
|---|---|
| async re-present drops/s under load | 117–125 (mean 120.2; `rdrop:120/s` ×45, `123/s` ×38, `117/s` ×29 of 159 windows) (the stats line's `rdrop:N/s`, mean over the run's windows) — **> 0** ✓ |
| real frames dropped under load | `ringfull=0/s`, `dd_lost=0` — **0** ✓ (the capture ring never overflowed; no lost desktop-duplication frames) |
| `--validation` soak, 30 s under load | **0 `[vk-` lines** |

### 4.4 · Two findings the gate exposed (both pre-existing; neither R4's)

1. **`--exit-after` / `--max-frames` are honoured only on the WAP path.** The bounded-run guard sits inside the
   WAP tick branch; in grid mode (`--no-warp-at-presenter`) it is never evaluated and the run never ends — the
   baseline binary ran 31 minutes before it was killed (`ps 240/s ok=444330`). The CSV row push has the same
   scope. Fix owed: hoist the guard to the tick boundary (INSTRUMENT plane, one line) — a separate small change
   after R4, not folded in.
2. **The async present re-shows the front on about half the ticks.** On both binaries, every 60 s run on the ball
   zoo prints `rdrop:117–125/s` on 147–155 of its 159 stats windows (`[ra] 240.0 fps (present) | wap tick 240/s
   | … | uniq 240/s rdrop:123/s | … | warp 2.39ms | … | ps 240/s ok=2700 to=0 er=0`). `rdrop` counts ticks
   where a new warp was wanted but the previous one was still in flight, so the completed front was re-presented:
   ~120 of 240 presents/s carry the previous image. The `--async-present` design accepts "~1 tick of pipeline
   latency" (cli.hpp:820); on this rig the warp's GPU completion latency is ≥ 1 tick most of the time (the flow
   matcher shares the device: `gpu(A:36%)`), so the design's occasional drop is the steady state. **The fresh-frame
   rate is therefore ~120/s on this content, not 240/s** — `uniq/s` (distinct pair-phase DECISIONS) and `disp_src`
   (the decided step) cannot see it; `MsBetweenDisplayChange` cannot either (a re-present is a flip). Probe, the
   synchronous path (`--no-async-present`, B, 60 s): presents 14,383 / 60 s, uniq 240 (stats) / 238.9 (CSV)/s, warp 3.9–4.1 ms, lat 21.4–22.5 (CSV mean 21.63) ms —
   see §4.5. This is a finding for the operator's objective ("la correcta multiplicación de frames"), recorded
   here because R4's gate is where it surfaced; the cause (warp completion behind the flow's GPU work) is the
   MR-2 / S7 `--async-queue` question, and the instrument that would settle it is a per-tick "fresh vs re-shown"
   count in the CSV (owed).

### 4.5 · The sync-present probe (B, `--no-async-present`, 60 s) — what the re-present rate costs and hides

| | async (the default; 4 default-set runs + load) | sync (`--no-async-present`, 1 run) |
|---|---|---|
| presents / 60 s | 14,384–14,386 | 14,383 |
| `rdrop` (re-shown front) / s | 117–125 on every window | **0** (by construction: the tick blocks on the fence) |
| fresh warps reaching the screen / s | **≈ 120** (240 − rdrop) | **240** |
| `warp` (the lambda's own time; sync = record + GPU completion) | 2.3–3.0 ms | **3.9–4.1 ms** of a 4.17 ms tick |
| `iter` / worst | 4.17 / 6.4–6.9 ms | 4.17 / **5.0 ms** |
| `lat` (EMA) | 20.4–21.2 ms | 21.4–22.5 ms (CSV mean 21.63) |
| `gpu(A)` | 36–39 % | 40–41 % |

Quoted (sync): `[ra] 240.0 fps (present) | wap tick 240/s (arr 61) | cap 61/s | cons 59/s | uniq 240/s | frz 0.0/s |
warp 3.91ms | iter 4.17/worst 5.07ms | lat 21.6ms | … | ps 240/s ok=8100 to=0 er=0 gpu(A:41% B:0% G:0%)`.

Reading: the warp batch takes **~3.9 ms from submit to fence** on this rig, against a **4.17 ms tick** — so the
async poll one tick after the submit finds the previous warp still in flight about every other tick, and the
completed FRONT is re-shown: the shipping default puts a fresh interpolated frame on the panel **~120 times per
second, not 240**. The sync path gets all 240 at +1 ms of latency, with each tick 94 % occupied (worst 5.0 ms:
occasional overruns). `gpu(A:41%)` says the device is busy ~1.7 ms of those 3.9 — the rest is submit-to-execute
latency and the present's own D3D copy / keyed-mutex hand-off, i.e. stage 6's pacing domain. Hypothesis, not
established: the keyed-mutex acquire on the bridge memory serialises the warp's blit behind the previous
present's `CopyResource`. **What would settle it:** a GPU timestamp query around the warp batch (execution vs
latency) and a per-tick "fresh / re-shown" column in the CSV — both INSTRUMENT-plane additions, owed. No
product change is proposed here; this is the measurement the operator's objective ("la correcta multiplicación
de frames") needs before anything is tuned.

### 4.6 · R4b — the instruments, and the finding measured (same day; 17 runs)

Three INSTRUMENT-plane additions (`r4b_patch.py`; none touches a presented pixel; the moved bodies of §2 are
untouched — verbatim still 68/81): (1) the `--exit-after` / `--max-frames` guard HOISTED to the tick boundary
(grid mode now self-bounds: `--exit-after 10` → `total_presents=2395 (--duration 10s …)`); (2) **`phyriadfg_fresh`**
per CSV row (1 = this present carries a warp completed since the previous present: async → a promotion happened;
sync → this tick recorded one), `fresh_count` in `-stats.csv`, and `fresh:N/s` beside `uniq` on the stats line;
(3) **`--warp-timing`**: `vkCmdWriteTimestamp` top-of-pipe after `vkBeginCommandBuffer` and bottom-of-pipe after
the blit, read at promotion (`vkGetQueryPoolResults`), plus the host-observed submit→completion latency (the host
clock at `vkQueueSubmit` to the poll that saw the fence) — per fresh present in the CSV (`phyriadfg_warp_gpu_ms`,
`_lat_ms`), EMAs on the stats line (`gpu … sub2fence …`). Zero new warnings against the full pre-R4 build's set.

**Measured** (`r4b_measure.ps1` + the probes; the ball zoo 60 fps 1920×1080, 60 s, `--csv --warp-timing`;
`r4b_parse.py`; "completion" = the submit→fence latency as SEEN by the poll that found it, so it is quantised by
how often the thread looks):

| configuration | runs | fresh | fresh/s | rdrop/s | GPU p50 / p95 (ms) | completion p50 / p95 (ms) | `MsAddedLatency` |
|---|---|---|---|---|---|---|---|
| **async — the shipping default** | 2 | **49.9 %** (7,174 / 7,176 of 14,386) | **119.3** (spread 0.04) | 119.7 / 119.6 | **0.107** / 0.123 | 9.10 / 10.53 — the SECOND poll after submit | 20.86 (spread 0.06) |
| **sync — `--no-async-present`** | 2 | **100 %** (14,383 / 14,383) | **239.3** (spread 0.01) | 0 | **0.079** / 0.087 | **4.03** / 4.69 (max 6.3–7.4), constant over the run | 21.62 (spread 0.05) |
| `--shallow-queue` (default budget 350 µs) | 1 | 54.5 % | 130.3 | 106.7 | 0.102 / 0.121 | 6.93 / 10.53 | 20.24 |
| `--shallow-queue --shallow-queue-budget-us 2000` | 2 | 50.4 / 50.1 % | 120.5 / 119.9 | 118.1 / 118.8 | 0.105 / 0.103 | 9.30 / 9.61 p50 | 20.82 / 21.04 |
| `--shallow-queue --shallow-queue-budget-us 4000` | 3 | 72.6 / 78.0 / 76.5 % | 173.5 / 186.5 / 183.2 | 44.6 / 35.6 / 38.0 | 0.094 / 0.095 / 0.093 | **1.96–2.58** / 10.67 | 21.00 / 20.20 / 20.42 |
| the same at 4000 µs, zoo at 61 / 59 fps (the beat test) | 1 + 1 | 71.6 / 74.4 % | — | — | — | 2.68 / 2.53 p50 | — |
| `--present-waitable` (the mechanism test, below) | 2 (+1 with sq 4000) | **99.85 / 99.84 %** (14,362 / 14,360 of 14,383) | 239.0 stats mean (`fresh:240/s` on every window after the ~4 s start-up ramp) | 0 | 0.088 / 0.092 | 4.16 / 5.62 — the FIRST poll, at the next tick; with the 4000 µs spin on top: **0.25** / 0.66 (max 1.80), 99.8 %, NO 16 s cycle | 21.21 / 21.24 (with the spin 21.16) |

Quoted (sync run 1): `[ra] 239.8 fps (present) | … | uniq 240/s fresh:240/s | … | warp 3.42ms | iter 3.68/worst
4.40ms | lat 20.1ms | … | gpu 0.08ms sub2fence 3.34ms`; (async run 2): `… uniq 239/s fresh:119/s rdrop:119/s | …
| gpu 0.11ms sub2fence 8.71ms`; (4000 µs, run 2, first window): `[ra] 135.3 fps (present) | … | uniq 134/s
fresh:135/s | … | warp 0.52ms | iter 0.80/worst 2.95ms | lat 16.1ms | … | sq:135H/0M gpu 0.10ms sub2fence 0.31ms`.

**The shallow-queue runs are BISTABLE.** The per-second fresh fraction of every 4000 µs run alternates between a
**≥ 99 % plateau** (hits ≈ 240/s, completion p50 0.49–0.84 ms, p90 2.3–2.6 ms, nothing above 4 ms) and a
**≈ 62 % floor** (hits ≈ 88/s; completion clustered at [0–0.5) 17–19 %, [2–4) 41–44 %, [4.5–6) 15–16 % — the
next tick's poll — and [10–12) 16 % — the poll after that). Plateau starts, in seconds: run 2 `0, 22, 39, 56`;
run 3 `0, 19, 36, 53`; run 1 `9, 25, 41, 58` — a **16–17 s period** (autocorrelation peak 17 / 17 / 16 s, r
0.47 / 0.59 / 0.65). The 2000 µs runs never enter the plateau (hits 0.6/s → 0.2–0.4/s); the 350 µs run held 185
hits/s for its first ~4 s and never again (`sq:0H/128M`). Neither the async nor the sync run shows the period
in `fresh` (119.7 → 119.2 and 229 → 240 flat over 159 windows).

**The 16 s cycle is the loop's own, and the code names it.** The panel flips every **4.17093 ms = 239.755 Hz**
(CSV `MsBetweenDisplayChange` over 14,379 single-period flips, sync run 1); the output timer ticks at 4.16667 ms
= 240.000 Hz; the presents are spaced at the PANEL period (median `qpc_present` delta 4.1710 ms) — the loop is
flip-locked, `Present(0)` blocking on the flip-model swapchain's two buffers (`BufferCount 2`; the waitable
object and `SetMaximumFrameLatency(1)` are the default-off `--present-waitable`). So the tick's lateness against
the timer grid — the CSV `slip_ms` — is a SAWTOOTH in every run: it grows **1.02 ms/s** (4.26 µs per tick, the
two periods' difference) and resets when it exceeds four periods (`present.cpp:1819`: `if(tn-tgt>4.0*tick_period_ms){
tick_t0=tn; tick_k=0; …}`): 16.67 ms ÷ 1.02 ms/s = **16.3 s**. The shallow-queue plateau is the stretch after
each re-seat in which the slip is ≈ 0 (4000 µs run 2: slip 0.00 at 2–8 s → 100 % fresh; 1.83 ms at 12 s → 63 %;
0.00 at 24 s → 100 %; 1.41 at 28 s → 63 %; 0.00 at 40–42 s → 100 %): with the tick starting ON its target the
spun fence completes at 0.3–0.8 ms; once the loop is flip-locked (the tick starts late, right after the previous
present returned) the same spin sees 2–4 ms or misses. **The beat test refutes the source as the driver:** the
zoo at 61 fps (plateaus 11–14, 27–30, 44–47 s) and 59 fps (7–11, 24–27, 40–43, 57–58 s) show the SAME 16 s period
(autocorrelation r 0.66 / 0.69), where a source/panel beat would have given ≈ 1 s. The sync path is immune to
the slip (100 % at every phase) and the async path blind to it (50 % at every phase).

**What the numbers establish.** (1) The warp batch (upload barriers + warp + blit) costs **0.08–0.11 ms of GPU
time**. (2) It CAN complete **0.3–0.6 ms after submit** (the plateau's p50, with the thread spinning on the
fence). (3) Under the shipping default it is NOT complete at the first poll ~4 ms after submit and is found at
the second: **half of the presents carry the previous image (fresh 119.3/s of 240)**. (4) The sync path completes
at a constant 4.03 ms — one panel period — and delivers all 240 at +0.77 ms of added latency, with the present
thread blocked ~4 ms of every 4.17 ms tick (worst 5.0 ms). (5) Whether a spun fence completes at 0.3 ms or at
2–4+ ms is decided by the tick's phase against the flip, which cycles every 16.3 s. `uniq/s`, `disp_src` and
`MsBetweenDisplayChange` cannot see any of this; `fresh` can. §4.5's reading "the device is busy ~1.7 ms of the
3.9" is SUPERSEDED: the device is busy 0.1 ms; the rest is waiting. A correlate, not a cause: the 4090 sits at
3,060 MHz / 127 W on the sync path and 2,610 MHz / 83 W on the async one — the load pattern.

**The mechanism — one hypothesis, four predictions matched, then tested.** The VK batch of tick k writes the
bridge slot the previous present read (`present_front`: `AcquireSync(0) → CopyResource → ReleaseSync(0) →
Present(0)`); its keyed-mutex acquire therefore waits for that `CopyResource`, and on a flip-model swapchain with
two buffers the D3D queue runs that copy behind the previous frame's flip. Predicted and matched: the sync
path's constant one-period completion (the chain VK k+1 ← copy k ← flip k−1 runs everything one vblank behind);
the async path's exact 50 % (a re-shown slot is copied twice in a row, its next VK acquire waits a flip); the
plateau's 0.3 ms with the slots alternating every tick (the copy the batch waits for is two presents old); the
plateau's collapse once the loop is flip-locked. **The test** — the default-off `--present-waitable` makes the
present pillar wait for the previous frame's consumption BEFORE `Present` (`SetMaximumFrameLatency(1)` + the
waitable object), so the copy no longer queues behind a flip; if the chain is the cause the shipping async
path must rise from 49.9 % fresh without any other change: **it does — 99.85 / 99.84 % fresh (14,362 / 14,360 of 14,383; `fresh:240/s` on every window after the start-up ramp; DI-3 spread 0.01 %)**, the fence now found at the FIRST poll (p50 4.16 ms = the tick) and, with the 4000 µs spin on top, at **0.25 ms p50 / 0.66 p95 / 1.80 max** over the whole minute — the 16 s bistability gone while `slip_ms` still sawtooths underneath. The cost: `MsAddedLatency` 21.21 / 21.24 (+0.35 ms over the default's 20.86; the sync path's is 21.62) and the present thread now blocked in the waitable wait instead of the fence wait (`warp 3.61–3.91ms | iter 3.92–4.16/worst 4.10–4.40ms` on the stats line; CSV `iter` p50 4.17, p95 4.22, max 8.1). Quoted (run 1, last window): `[ra] 240.0 fps (present) | wap tick 240/s (arr 59) | … | uniq 240/s fresh:240/s | … | warp 3.61ms | iter 3.92/worst 4.10ms | lat 20.9ms | slip 0.00/max 0.00ms | … | ps 240/s ok=14310 to=0 er=0 gpu(A:39% …) vbhit:61/s gpu 0.09ms sub2fence 4.26ms`.

**Not established:** which link of the chain the waitable cuts — it makes the previous frame's consumption precede BOTH the copy and the batch, so "the copy behind the flip" and "the D3D packet ahead in the WDDM queue" are removed together (the `--no-keyed-mutex` probe would separate them); and the async default's true completion time (the poll cadence quantises it to "before the next tick"; the spun 0.25 ms is the measurement we have). The other named probes remain: a `--no-keyed-mutex` run (the no-KM path exists in
`present_init.cpp:55`, hardware-gated today; its async safety must be validated first), an ETW / GPUView trace of
the present packets. **Register: XR15.** The present POLICY is the operator's — a product default with visible
history — with these numbers: (1) accept ~120 fresh/s (ships today); (2) flip the default to sync: 100 % fresh,
+0.8 ms, the P thread 94 % busy; (3) `--shallow-queue` at 4000 µs: 73–78 % fresh at LOWER latency than the
default, but bistable on the 16 s cycle — not a product setting; (4) `--present-waitable` — an EXISTING default-off knob (the pillar's FG_PRESENT_PACING_DESIGN option B): 99.85 % fresh on the async path at +0.35 ms `MsAddedLatency`, the P thread blocked in the waitable wait; making it the default is the same product decision, with that design note as its history. The session's recommendation, for his decision: (4) over (2) — the same 240 fresh/s at 0.4 ms less added latency than sync and without the fence wait's overrun exposure — after a DI-3 pair under `gpu_load` and on real game content, neither run here.

## 5 · `--tdr-test` — run by the operator: the detection PASSED, the teardown HUNG, the hang fixed, re-run pending

The forced GPU hang resets the device that carries the operator's interactive display: an **L3** operation under
`SAFETY_PROTOCOL.md` §5, so the operator ran it himself (2026-09-05 evening, `build-release\phyriad_fg.exe --window
"RA Ball Zoo" --exit-after 60 --tdr-test 15`, the R4b binary). The instrument: `shaders/tdr_hang.comp` (one workgroup
whose loop bound is a push value the compiler cannot fold) + `PresentStage::tdr_arm/tdr_maybe` (armed at init,
recorded ONCE into the tick's command buffer at T+N s, before `vkEndCommandBuffer`).

**What his run printed (quoted):** `[ra] --tdr-test 15: ARMED -- a never-terminating compute dispatch will be
recorded into the present-stage command buffer at T+15 s to force a GPU timeout (TDR) and prove the device-loss
exit path` → 40 stats windows of normal presenting (`[ra] 242.5 fps (present) | … | ps 242/s ok=3510 to=0 er=0`)
→ `[ra] --tdr-test: dispatching the GPU hang NOW (expect VK_ERROR_DEVICE_LOST within the TDR window, then the
clean g_quit exit)` → `[ra] VK_ERROR_DEVICE_LOST -- graceful exit (the game keeps running; PhyriadFG is an external
overlay)` → **nothing more**: the process stayed alive and he had to force-close it ("cuando da el ultimo mensaje,
crashea … tengo que forzar su cierre"). The desktop stayed usable (the TDR recovered the display).

**Reading.** Two halves. (1) **The detection path PASSED:** the hang was dispatched at T+15 s, Windows' TDR reset
the device, the next fence poll returned `VK_ERROR_DEVICE_LOST`, `vk_live` latched `g_device_lost`, printed the
one-shot line and set `g_quit` — exactly the designed chain. (2) **The teardown FAILED:** neither `[ra] done (…)`
nor the bounded-run clean-exit line printed, and both sit AFTER the worker joins (`main.cpp:1051-1053`:
`thr_c.join(); … thr_f.join(); thr_p.join();`) — so a worker thread never exited. Cause, read from the code: the
workers waited with `vkWaitForFences(…, VK_TRUE, UINT64_MAX)` (F: `flow.cpp:1409/1696/2146/2177`, `flow.hpp:201`;
C: `capture.cpp:762/938`; P: `present_stage.cpp:197/210`, `present.cpp:255/686/789`; the init helpers
`vk_util.cpp:66-67`, `vk_util.hpp:60`). After the loss a submit is refused and its fence never signals; an
unbounded wait on it never returns, and `vk_live` wrapped around such a wait never gets to run. Under single-GPU
(this run: flow + convert on A.q2, present on A.q) every worker shares the lost device. The thread was not
identified by a dump — the operator's console is the only evidence — but every candidate is this pattern.

**The fix (same day, `r4c_patch.py`):** ONE helper, `vk_wait_live(dev, fence)` / `vk_wait_sem_live(dev, wi)` in
`core/globals.{hpp,cpp}`, beside `vk_live`: 20 ms slices; `VK_SUCCESS` → true; any error → `vk_live` (latches the
loss) → false; `VK_TIMEOUT` with `g_device_lost` already latched on ANOTHER thread → false; an object still
unsignalled 2 s after a quit request → false, said once (`[ra] vk_wait_live: an object stayed unsignalled 2 s
after the quit request -- abandoning the wait (teardown proceeds)`). All 14 unbounded waits routed through it;
`grep vkWait.*UINT64_MAX src/` is empty. On a healthy device a signalled fence returns on its slice — the same
result as before, ~20 ms later at worst when a wait genuinely straddles a slice boundary (the sync present's
4.03 ms fence never does). Build: 0 errors, 34 warnings, none on a touched line (the same `fopen`/C4189/C4456 set).

**Verification of the healthy paths (the session, 10 s bounded runs, ball zoo):** {VERIFY}

**Status of G-R4's TDR item:** the detection half is PROVEN by the operator's run; the teardown half is FIXED and
**awaits his re-run** of the same command line on the new binary — the record will carry that output. Side
evidence from his run, on 1280×720 capture: the first eight stats windows show `fresh:243/s` with `slip 0.00`, then
`fresh:121/s rdrop:121/s` as `slip` climbs 0.26 → 7.35 ms — §4.6's mechanism on a second content and session.

## 6 · Verdict — **G-R4 PASSED, except the forced-TDR item (detection proven; the teardown fix awaits his re-run)**

- **The extraction changes nothing the instruments can see:** presents Δ +0.5 (spread 1–2), `disp_phase` mean
  Δ +0.0002 (A's spread 0.0003), `disp_src` step 0.2495 both, uniq/s Δ −0.03, `MsBetweenDisplayChange` median
  4.171 ms both, `ps ok/to/er` identical, latency +0.15 ms at n = 2 (inside every earlier A/B's spread on this
  rig); the four touched paths (`--shallow-queue`, `--rfp`, `--pace-hard`, the grid path) each within the same
  bands (§4.1–4.2).
- **Under `gpu_load`:** drops/s > 0 (120/s) and real frames dropped = 0 (`ringfull=0/s`, `dd_lost=0` on all 159
  windows); **`--validation` 30 s under load: 0 lines** (§4.3).
- **The invariants:** the drop decision is `begin()`'s scalar compare on P-thread state, no lock (MR-1); every
  fence poll `vk_live`-wrapped, moved verbatim (MR-1); one submitter per queue, the same calls (MR-2); the same
  own-window surface, `last_flip_qpc()` now a `FlipStats` edge (MR-7); nine references bound, nothing copied,
  the slots bound not re-created (CR1); its own TU under LTO, presents/latency inside spread (PR1).
- **The decision is declared** — `Decision::{Warp, Dup, Drop, Decimated}` — and load-bearing: `begin()` consumes it.
- **`--tdr-test N`:** run by the operator (§5): the device-loss DETECTION proven (`VK_ERROR_DEVICE_LOST` latched, the
  graceful-exit line printed), the TEARDOWN hung on unbounded fence waits — fixed the same day (`vk_wait_live`, 14
  sites), healthy paths re-verified; the re-run on the fixed binary is his and pending.
- **Two findings for the operator (§4.4–4.6), neither R4's to fix:** `--exit-after` was WAP-only (fixed by R4b,
  hoisted); the async present re-shows the front on half the ticks — **49.9 % fresh, 119.3/s**, measured with
  the R4b instruments against 100 % / 239.3/s on the sync path; the batch executes in 0.1 ms and CAN complete in
  0.3 ms, so the loss is a wait, not work (register XR15, the operator's present-policy decision).

## 7 · Honesty ledger

- The moved bodies are verbatim to 83.95 % by a whitespace-trimmed line multiset; the 13 differences are the
  six transforms above, each a rename or a return-value change, none a logic change. The measure cannot see a
  moved line that is identical text but now runs under a different condition — the A/B is what covers that.
- `stats_second()` did not move (§1); X12 listed it. R4's scope is the present machinery + the declared decision
  + the feedback edge + the TDR instrument; the stats move is owed and named.
- No delegated review was run for R4: the compiler proved no moved identifier is still referenced by the loop
  (the three C4189s were exactly the fallen-out aliases), and the A/B measures the behaviour; a Sonnet reader
  would re-read text a script already diffed.
- PR1 (de-inlining): the stage lives in its own TU (like R1's clock); LTO/IPO is on; the presents/s and latency
  numbers of §4 are the measurement.
- **R4c (§5):** the TDR hang was diagnosed from the operator's console output and the code, not from a thread
  dump; the bounded-wait fix covers the whole class (every unbounded GPU wait) rather than the one thread that
  hung, because that thread was not identified. The 2 s post-quit abandon is a new behaviour on a HEALTHY device
  only if a fence is genuinely stuck — a condition that previously hung the process; it is printed when it fires.
- **R4b (§4.6):** every number is from one content (the ball zoo, 60 fps, 1920×1080) on this rig, 60 s runs;
  no `gpu_load` soak and no real game were run with the R4b instruments — the `--present-waitable` result is a
  mechanism test, not a product qualification. The "completion" latency is what the poll SAW: the async
  default's 9.1 ms is two poll periods, the waitable's 4.16 ms is one; only a spinning poll (the shallow queue)
  measures the batch itself (0.25–0.8 ms). The harness (`tools/r4b/`: `r4b_measure.ps1`, `r4b_beat.ps1`,
  `r4b_waitable.ps1`, `r4b_parse.py`) ran from the session's scratchpad; the tools copy writes under its own
  directory. A PowerShell-host launch writes the logs as UTF-16 (the parser reads both); the `total_presents`
  of the waitable runs (14,382 / 14,382 / 14,382) come from that re-read. The 16 s mechanism (the grid re-seat)
  and the keyed-mutex chain are read from the code (`present.cpp:1819`, `present_stage.cpp` `present_front`)
  and matched by the data; the chain's individual links were not separated (the `--no-keyed-mutex` probe is
  the named next step).

*Made with my soul - Swately <3*
