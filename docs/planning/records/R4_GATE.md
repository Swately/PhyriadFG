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

## 5 · `--tdr-test` — built, NOT run

The forced GPU hang resets the device that carries the operator's interactive display: an **L3** operation under
`SAFETY_PROTOCOL.md` §5 (GPU context), which requires the working tree checkpointed first (this commit) and the
operator's explicit approval. The instrument is complete (`shaders/tdr_hang.comp`: one workgroup whose loop bound
is a push value the compiler cannot fold; `PresentStage::tdr_arm/tdr_maybe`: armed at init, recorded ONCE into
the tick's command buffer at T+N s, before `vkEndCommandBuffer`). Expected path: the hang → Windows TDR (~2 s) →
`VK_ERROR_DEVICE_LOST` on the next fence wait/poll → `vk_live()` → `g_quit` → the clean teardown. **G-R4's TDR
item is `built, awaiting the operator's word`**; the record will carry the run's output when he gives it.

## 6 · Verdict — **G-R4 PASSED, except the forced-TDR item (built, awaiting the operator's word)**

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
- **`--tdr-test N`:** built, not run (§5) — the one G-R4 item that waits, for the stated safety reason.
- **Two findings for the operator (§4.4–4.5), neither R4's to fix:** `--exit-after` is WAP-only; the async
  present re-shows the front on ~half the ticks — the fresh-frame rate is ~120/s under the shipping default.

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

*Made with my soul - Swately <3*
