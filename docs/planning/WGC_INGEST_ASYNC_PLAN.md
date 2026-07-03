# WGC `--ingest-async` — plan, strategy, risk register

Tier-2 planning doc (concurrency = crash-class). One doc, three sections, per the plan-tier
protocol. Status: **measured** — implemented 2026-07-03, all risks `mitigated`/`accepted` (see the
register + the results column of the self-test matrix). **No commit while any risk is `open`** —
none is.

---

## 1. PLAN — what / why

**What.** Enable the existing `--ingest-async` opt-in for the WGC capture path: move the convert
(A-path `submit_wait` or iGPU `cpPipe` dispatch, both fence-blocking) OFF the C pickup loop into
the already-existing drop-to-newest convert worker (`run_convert_worker`, `capture.cpp:738`),
exactly as the DDA path already does. **Opt-in only** — the default stays OFF; flipping the
default is a later operator decision after real-game soak.

**Why (measured, this arc).** After the task-0/1 instrumentation + Map-miss fix, the WGC serial
pickup measures (`[ra-acq] (wgc)`, zoo @240, 2026-07-02..03 logs `final_a.log`):

| term | ms/frame | where |
|---|---|---|
| staging (idle wait for delivery) | ~2.3–2.9 | W1 spin — not a cost, the source period residue |
| copy (fence + Map retry) | 0.04–0.07 | post task-1 fix (was 1.7–1.9 pre-fix) |
| memcpy | 0.12–0.14 | CPU readback |
| hash | 0.06–0.08 | dedup sample hash |
| convert (inline, fence-blocked) | ~1.4–1.8 | `c_conv_us` EMA, `[lat-trace] conv:` |

The loop serializes `copy+memcpy+hash+convert` ≈ **1.7–2.1 ms/frame of busy work → an ingest
ceiling of ~1/(pickup+conv) ≈ 480–590/s**, dominated by the *inline blocking convert*. Moving the
convert to the worker leaves the C loop at `copy+memcpy+hash` ≈ **0.25–0.3 ms/frame →
~3000/s-class**, and the ceiling scales with memcpy bandwidth (hardware) instead of the serialized
convert chain. The DDA path already has exactly this split (`capture.cpp:283-393` acquire-only +
worker); WGC was force-disabled at `main.cpp:1142-1145` with the rationale "the convert is already
off the acquire path" — true for the *callback* (delivery), **false for the pickup loop**, which
still converts inline. This plan removes that gate and maps the WGC pickup onto the same
raw-ring/worker machinery.

---

## 2. STRATEGY — the exact mechanical mapping

All line anchors verified first-hand at the current working tree (2026-07-03).

**S1 — un-gate (main.cpp:1142-1145) + re-gate the DDA acquire loop (capture.cpp:283).** Delete the
WGC force-off — but the force-off was LOAD-BEARING for `run_capture`'s DDA acquire-only loop, which
is gated on `cfg.ingest_async` alone and drives `d.dup` (**NULL on WGC** — verified by an immediate
startup segfault on the first un-gated run). That loop becomes
`if(cfg.ingest_async && cfg.capture_api==CA_DD)`; WGC falls through to the main loop where its own
async deposit branch lives (S2). The raw-ring alloc block
(`main.cpp:1146-1163`) becomes API-shared with ONE change: `dxgi_stage2` (the 2nd **DDA** staging
texture, the readback double-buffer) is allocated **only when `capture_api==CA_DD`** — WGC has its
own staging ring (`WgcCtx::ring`, RING_N=4) and never touches `dxgi_stage/dxgi_stage2`. The
alloc-failure fallback (free-what-we-got + force OFF + honest print, `main.cpp:1156-1160`) is kept
verbatim and now covers WGC (R4).

**S2 — the WGC async branch (capture.cpp, inside the existing `#ifdef _MSC_VER` WGC pickup
branch).** When `cfg.ingest_async`, after the successful Map (post fence-wait + bounded retry +
older-slot fallback — all unchanged), branch BEFORE the serial memcpy-to-`Astage` and end in
`continue` so the shared serial tail (t_cap stamp, lt EMAs, dedup, PLL store, convert, `c_seq`
publish) is **unreachable** (R3). The branch, mirroring the DDA acquire (`capture.cpp:330-374`):

1. `rk = wpub % kRawSlots` — `wpub` is a new loop-local monotone **published-raw counter**
   (the DDA analog is `pframe`; `raw_seq` stores `wpub+1`, the worker derives
   `rk=(raw_seq-1)%kRawSlots` — same arithmetic).
2. **Busy check** `rk==raw_busy.load()` → drop: `stat_mapmiss.fetch_add(1)` (the DDA-async
   convention, `capture.cpp:335`), still `Unmap` + `ring_read.store(use_cnt)` (the staging slot IS
   consumed), `continue`. Documented meaning: in async mode `mapmiss` = busy-drops (+ any residual
   Sleep-cycle misses), visible in `[lat-trace] mapmiss:`.
3. memcpy rows → `raw_astage_a[rk].mapped` (NOT `Astage.mapped`), then `Unmap` +
   `ring_read.store(use_cnt)`, `arr_ts=now_ms()`, and the lt submit/compose slot reads
   (`ring_submit_us/ring_compose_us`, gated `latency_trace`) — the serial order preserved.
4. `dd_acq.fetch_add(1)` (pre-dedup pickup rate — `acq=` readout parity with serial).
5. `frame_sample_hash` on the raw slot → `dd_uniq` always; `[ra-acq] (wgc)` 4-way accumulation
   (see S4); `if(cfg.dedup && dup) continue` — **no publish** (the slot is reused next frame;
   `wpub` does not advance).
6. Unique (published) path: PLL arr-delta **post-dedup** from `arr_ts` vs `last_ing_arr_ms`
   (the same local + guards `iv>0.5&&iv<500` the serial tail uses at `capture.cpp:632-644`),
   stored to **`wgc_ctx->arr_delta_us`** (the WGC PLL counter — NOT `dd_arr_delta_us`) (R5);
   `raw_tcap[rk]=now_ms()`; carry `raw_lt_submit[rk]/raw_lt_compose[rk]` (R6);
   publish `{ lock_guard(raw_mtx); raw_seq.store(wpub+1); } raw_cv.notify_one(); ++wpub;`
   (the lost-wakeup-safe publish pattern, `capture.cpp:370-373`); `continue`.

**S3 — the worker (capture.cpp:738+).** NO functional change to the convert itself. Additions:
(a) comment updated (it is no longer "(DDA)"-only); (b) the R6 lt carry: after
`c_slots[s].t_cap_ms=raw_tcap[rk]` (line 802), when `cfg.latency_trace`, fold
`(t_cap−raw_lt_submit[rk])` into `lt_copy_us` and `raw_lt_compose[rk]` into `lt_compose_us`
(the same 0.8/0.2 EMAs the serial tail uses at `capture.cpp:600-608`) — guarded `>0` so the DDA
path (stamps stay 0) is inert. `cap_rot180` stays CA_DD-gated (R8). The `t_pub_ms` fwake stamp
(task-1 change 3) already covers the worker publish.

**S4 — instrumentation continuity (R7).** The `[ra-acq] (wgc)` accumulate+print block (currently
inline in the shared-tail hash block) is hoisted into a loop-scope lambda `_wacq_acc(st,cp,mc,hs)`
called from BOTH the serial tail (unchanged gating) and the async branch (step 5). staging/copy
are measured before the Map (shared); memcpy/hash are measured at the async branch's own sites.
`ringfull` (callback-side) is unaffected by definition.

**S5 — plumbing.** New per-slot carry arrays `double raw_lt_submit[kRawSlots]`,
`raw_lt_compose[kRawSlots]` in main.cpp next to `raw_tcap` (line 542), exposed through
`FgContext` (new members after `raw_tcap`, `fg_context.hpp:107`, designated-init order matched)
and aliased in `run_capture` + `run_convert_worker`. `cli.hpp:186-195` + the `--ingest-async`
parse print (`cli.cpp:385`) updated: "DDA-only (forced OFF for WGC)" is no longer true.

**S6 — OFF-path discipline.** `--ingest-async` absent → `wpub` dead, the async branch is never
entered, no raw ring allocated, worker never spawned — byte-identical serial behavior (T2).

---

## 3. RISK REGISTER

Every risk names its mitigation **as code** and its first-hand verification. Status legend:
`mitigated` (code + verified) / `accepted` (bounded, documented, deliberate). None may ship `open`.

| # | Risk | Mitigation AS CODE | Verification (first-hand) | Status target |
|---|---|---|---|---|
| R1 | **Torn raw slot** — the C pickup overwrites `raw_astage_a[rk]` while the worker converts it | The `raw_busy` guard: worker `raw_busy.store(rk)` before convert / `store(-1)` after (`capture.cpp:800,851`); the pickup drops the frame when `rk==raw_busy.load()` (S2 step 2). BEST-EFFORT by timing (memcpy ~0.13ms ≫ the sub-µs latch window + slot separation ≥1 mod 4): worst case = one self-correcting torn frame, never a crash — the same bound the DDA design documents (`fg_context.hpp:39-41`) | T1/T5: no visual corruption reported by the run; busy-drop counter visible via `mapmiss` | **accepted** (bounded, DDA-parity) |
| R2 | **Teardown ordering + gate widening** — worker touches `cmdA/cmdG/fA/fG` + raw imports after Vulkan teardown → UAF/device-lost; or a DDA-assuming `ingest_async` gate fires on WGC | Spawn `main.cpp:2872` (`if(cfg.ingest_async)` — API-agnostic); quit latch + `raw_cv.notify_all()` (`main.cpp:2880`); `thr_cw.join()` at `main.cpp:2883` BEFORE any convert/Vulkan teardown; the worker wait has a 5ms quit-safety timeout (`capture.cpp:792-793`). Gate sweep (grep `ingest_async` across src): `main.cpp:1142` force-off REMOVED; **`capture.cpp:283` DDA acquire loop NARROWED to `&& capture_api==CA_DD`** — found the hard way: the first un-gated run segfaulted on `d.dup->AcquireNextFrame` (NULL on WGC); fixed + re-verified | T4: 3× clean start/stop of the async WGC run — no hang, no device-lost, clean exit prints each time; T1 startup no longer crashes | mitigated |
| R3 | **Convert-state single-owner violation** — both the worker AND the serial tail drive `cmdA/cmdG/fA/fG/Anative/Awork/cpPipe` → command-buffer reset races, fence double-wait, device loss | The WGC async branch ends in `continue` BEFORE the shared serial tail (S2); with `cfg.ingest_async` armed the serial convert tail is **unreachable** from the WGC branch — stated as an invariant comment at the branch point in code. `cfg.ingest_async` is immutable after init (set once in `resolve/alloc`, read-only in the threads) | Code inspection of the branch (`continue` present, no fall-through path) + T1/T5 stability (a violation crashes within seconds under 240fps) | mitigated |
| R4 | **Alloc-failure crash path** — raw ring alloc fails with WGC and the code proceeds half-armed | The existing fallback block (`main.cpp:1156-1160`) — free what was allocated, `cfg.ingest_async=false`, honest print, serial path runs — now covers WGC because the gate above it is removed; `dxgi_stage2` alloc is CA_DD-gated so WGC cannot fail on it | Code inspection (the fallback is shared, not duplicated); alloc failure is not directly injectable on this rig — the fallback logic is unchanged from the DDA-proven path | mitigated (inherited, verified by inspection) |
| R5 | **dedup/PLL corruption** — async bypasses the shared-tail dedup+PLL; wrong placement re-opens the per-DELIVERY delta bug (T_robust corrupted by the delivered/unique ratio; the "PLL units fix" this arc) | Dedup runs post-memcpy on the raw slot (S2 step 5), BEFORE publish — the worker only ever sees uniques (DDA-async parity, `capture.cpp:348-357`). The PLL delta is stored POST-dedup, between PUBLISHED frames, from the WGC consume instant `arr_ts`, into `wgc_ctx->arr_delta_us` (the counter the WGC PLL reads — serial parity, `capture.cpp:637-641`) | T3: static ball (`-SpeedPx 0`) with dedup ON → `uniq=`/`in=` collapse to ~0-few/s while `acq=` stays ~240 — proves dedup gates the publish; T1: cadence/PLL sane at 240 (no vibración/bistability symptom) | mitigated |
| R6 | **lat-trace lies in async mode** — `lt_wgc_submit/compose` die at the `continue`; `[lat-trace] INVISIBLE copy` reads 0/stale | Carry both stamps through the raw ring (`raw_lt_submit/raw_lt_compose[kRawSlots]`, written at publish, read by the worker next to `raw_tcap[rk]` → folded into `lt_copy_us/lt_compose_us` with the serial-tail EMA constants); `t_cap_ms=raw_tcap[rk]` already carried; `t_pub_ms` stamped at the worker publish (fwake correct) | T1: `[lat-trace]` shows non-zero plausible `copy:` and `fwake:` in async mode; compare against the serial run's values | mitigated |
| R7 | **Instrumentation discontinuity** — `[ra-acq] (wgc)` stops printing (or prints garbage) in async mode | The hoisted `_wacq_acc` lambda called from both paths (S4); staging/copy brackets untouched; memcpy/hash measured at the async sites | T1: `[ra-acq] (wgc)` prints in async mode with staging≈period-residue, copy≈0.05, memcpy≈0.13, hash≈0.07 and NO conv-class term; `ringfull=` still in `[ra-cap]` | mitigated |
| R8 | **Rotation double-fix** — the worker applies ROTATE180 to WGC frames | Already correct: `cap_rot180` in the worker is `capture_api==CA_DD`-gated (`capture.cpp:742`); WGC delivers logically-oriented frames (measured on this rig, comment `capture.cpp:239-246`) → push-constant stays 0 for WGC. No code change needed | Code inspection (line 742 read first-hand); T1 visual: overlay content upright | mitigated (pre-existing) |

**Out-of-scope, noted:** the F in-order-drain fwake bistability (standing 2–3 slot backlog,
~+10ms input lag, pre-existing — evidenced in pre-fix logs) is orthogonal to this change and NOT
addressed here; async mode neither causes nor cures it.

---

### Self-test matrix (acceptance) — RESULTS 2026-07-03 (all runs: VS build, fresh zoo per run)

| T | run | pass criteria | result |
|---|---|---|---|
| T1 | async ON, zoo 240, 20s+, `--latency-trace` | `in=241` sustained; `[ra-acq] (wgc)` CPU-side loop ~0.3ms-class (no inline conv); `c_conv_us` still ~1.5 (overlapped in worker); freshage/lat ≤ serial; `ringfull=0`; stable | **PASS** — in=241/s; CPU-busy 0.31–0.36ms (staging 3.8ms = idle period residue); conv:1.5–1.6 in worker; freshage 6.7–7.6 ≈ serial (7.0–7.7); ringfull=0 |
| T2 | default run (no flag) | byte-identical spot-check vs today's lines ([ra-cap]/[ra-acq]/[lat-trace]/present) | **PASS** — no worker, serial pickup shape intact (staging 2.2–2.4 + conv inline), present 240.7 |
| T3 | async ON + `-SpeedPx 0` zoo (static) | `uniq=`/`in=` collapse to ~0-few/s, `acq=`~240 (R5 dedup placement proven) | **PASS** — acq=241/s, uniq=0/s, in=0/s, frz=240.7/s |
| T4 | 3× start/stop async ON | no hang, no device-lost, clean exit ×3 (R2) | **PASS** — 3× orderly SELF-exit via captured-window-close (3s each, "[ra] done" teardown prints, thr_cw joined). NOTE: console-injected quits (WM_CLOSE / CTRL_CLOSE / attached CTRL_C_EVENT) do not exit the app within 8–12s on EITHER path (serial controls identical) — pre-existing harness-side limitation, not an async regression. Also: minimized-console runs get Win11 EcoQoS-throttled (in fell to ~70/s) — always test with a Normal window |
| T5 | 60s soak async ON @240 | no drift, no crash, stats stable | **PASS** — in=241/s steady post-startup (one early window ringfull=2/s during warmup, caught by the always-on counter), present 240.7 er=0 through ok=14670, no drift; the pre-existing fwake bistability appears in async exactly as in serial |
