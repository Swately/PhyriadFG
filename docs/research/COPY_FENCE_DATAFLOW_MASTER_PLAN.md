# COPY_FENCE_DATAFLOW_MASTER_PLAN — STAGE-113 `--copy-fence` (the worthy capture-path)

> **Diátaxis type:** Explanation + plan (master-plan + implementation strategies + the **RISK_REGISTER**,
> one doc — the change is contained but **crash-class** so PLAN_TIER **Tier-2** applies; the register is
> the gate). **Status:** `designed`. **Companion:** [`INPUT_LAG_DREDUCTION_MASTER_PLAN.md`](INPUT_LAG_DREDUCTION_MASTER_PLAN.md)
> §8 (the combat decomposition that motivates this), [`PLAN_TIER_PROTOCOL`](../canon/PLAN_TIER_PROTOCOL.md).

## §0 — The operator's mandate (the design stance)

"Una arquitectura digna que sueña la perfección pero abraza la realidad — perfecta donde todo encaja y es
viable." Applied here: the capture pickup currently **busy-polls** (`Map(DO_NOT_WAIT)` + `Sleep(1)`-retry,
**MEASURED ~350 retries/s** in combat) waiting for the WGC `CopyResource` to land on the saturated 4090. A
worthy architecture does NOT spin-sleep for a copy — it waits **by event**, deterministically. This stage
makes the pickup **event-driven (dataflow)**. **The physical wall is DECLARED, not hidden:** the
`CopyResource` itself takes **~4 ms** on the 99 % 4090 (MEASURED, the `copy` line) and the warp ~8 ms — that
is silicon, untouched. The honest reclaim = the busy-poll's ~1 ms granularity overshoot + the wasted CPU +
lower jitter (determinism = "sensación"). NOT a multi-ms latency win — and that is fine: it is *perfect
where it is viable*.

## §1 — Mechanism (the cycles model)

Each WGC frame = a cycle. The `CopyResource` completion is a GPU-timeline event, not a thing to poll:
- **Init:** query `ID3D11Device5` from `D3D.dev` → `CreateFence(0, NONE)` → `ID3D11Fence* copyFence`;
  query `ID3D11DeviceContext4` from `D3D.ctx` → `ctx4`; `CreateEventEx` → `copyEvt`. All stored in `WgcCtx`
  (the callback already captures `raw_wctx`). If `ID3D11Device5`/`ctx4` query fails → `copy_fence` forces
  OFF (CR3 fallback) — the current Map-retry path runs byte-identical.
- **Callback (winrt thread):** after `CopyResource(ring[w%N], tex)` + `++ring_write`, enqueue
  `ctx4->Signal(copyFence, w+1)` — a GPU-timeline signal AFTER the copy. Returns immediately; does NOT hold a
  lock for the copy duration.
- **C-thread pickup:** when `--copy-fence` armed, instead of `Map(DO_NOT_WAIT)`+`Sleep(1)`-retry: target the
  newest slot `w-1`, `copyFence->SetEventOnCompletion(w, copyEvt)` + `WaitForSingleObject(copyEvt, kTimeoutMs)`
  (waits on the EVENT, off the D3D11 context → CR1), then `Map(DO_NOT_WAIT)` (now succeeds) + the existing
  memcpy/Unmap/ring_read. On `WAIT_TIMEOUT` → fall back to the current Map-retry path (CR4).

## §2 — Strategies (each cites its risk)

- **S1 — wait OFF the context, never a blocking `Map` [CR1].** `WaitForSingleObject(copyEvt)` blocks the
  C-thread on the fence EVENT, not on the D3D11 context lock — so the FrameArrived callback can keep doing
  `CopyResource`/`Signal` concurrently (D3D11Multithread already on). A blocking `Map(flags=0)` would hold the
  context lock for the copy → the callback would stall → capture freeze. DISQUALIFIED; use the fence+event.
- **S2 — fence value = ring_write [CR-correctness].** Signal `w+1` after copying slot `w%N`; C waits the
  fence ≥ `use_cnt` (the ring_write it consumes up to). Monotone (ring_write only rises). If the fence is
  already ≥ target, `SetEventOnCompletion` fires the event immediately (no spurious wait).
- **S3 — bounded wait [CR4].** `WaitForSingleObject(copyEvt, kTimeoutMs=33)`; on `WAIT_TIMEOUT` (GPU hang /
  no frame) fall through to the existing `Map(DO_NOT_WAIT)`+older-slot+`Sleep(1)` path — never an unbounded
  block. Reset `copyEvt` (auto-reset event, or `ResetEvent`) per wait.
- **S4 — device5/ctx4 capability gate [CR3].** Query both at init; if either is null, print a notice and
  force `copy_fence=false` → the current path. D3D11.4 (Win10 1703+) is required; the operator is on Win11.
- **S5 — lifetime [CR2].** `copyFence`/`ctx4`/`copyEvt` created in `d3d_init` (or right after the WGC ring
  alloc) and released in `d3d_shutdown`/WgcCtx teardown, AFTER `running=false` + the `Sleep(100)` callback
  drain (so no in-flight callback touches a freed fence). Captured by value into the callback like `raw_ctx`.
- **S6 — byte-identical-off [CR5] + the flag.** `--copy-fence` (cfg.copy_fence, default false) in
  `parse_extra` (NOT the main chain — C1061). Off → the callback's `Signal` is skipped (gated `lt_on`-style
  capture) AND the C-thread takes the current Map-retry path verbatim → byte-identical. lint_hal clean; §9
  signature last line. DD (non-WGC) path is unaffected (no fence).

## §3 — RISK_REGISTER (mitigation AS CODE + first-hand verification; NONE may be `open` at commit)

| # | Risk | Mitigation (as code) | Verification (first-hand) | Status |
|---|---|---|---|---|
| **CR1** | **Deadlock / capture freeze** — C blocks holding the D3D11 context lock → the FrameArrived callback's `CopyResource` stalls → WGC stops delivering → capture death. | `copyFence->SetEventOnCompletion(target, copyEvt); WaitForSingleObject(copyEvt, 33);` — waits on the EVENT, the C-thread holds NO context lock while waiting. `ctx4->Signal` (callback) is a fire-and-return timeline enqueue. | 35 s BF6 `--copy-fence`: `arr` (FrameArrived rate, DIAG) does NOT drop vs off; no capture stall; `# STALL`/watchdog 0. | designed |
| **CR2** | use-after-free of fence/event/ctx4 at teardown (in-flight callback). | create in `d3d_init`; release in `d3d_shutdown` ONLY AFTER `wgc_ctx->running.store(false)` + the existing `Sleep(100)` callback drain (main.cpp ~7990). Null-guard every use. | clean exit (no crash) after a `--copy-fence` run; ASAN-style: the release is post-drain. | designed |
| **CR3** | `ID3D11Device5`/`ID3D11DeviceContext4` unavailable (old OS/driver) → null deref. | `if(FAILED(dev->QueryInterface(IID_ID3D11Device5,...)) || FAILED(ctx->QueryInterface(IID_ID3D11DeviceContext4,...))) { copy_fence=false; printf notice; }` → the current Map-retry path. | force the query to fail (or run on a non-1703 box) → confirm the byte-identical fallback + the notice. | designed |
| **CR4** | the fence signal never arrives (GPU hang / dropped frame) → C blocks forever. | `WaitForSingleObject(copyEvt, kTimeoutMs=33)`; `WAIT_TIMEOUT` → fall through to the existing Map-retry (older-slot + `Sleep(1)`) path. | inject a stall (alt-tab / no frames) → C recovers via the timeout path, no hang; `mapmiss` resumes. | designed |
| **CR5** | off-path not byte-identical → a regression when the flag is off. | `cfg.copy_fence` default false; the callback `Signal` gated on a captured `cf_on` bool (zero work off); the C-thread fence branch gated `if(cfg.copy_fence && copyFence)`. | off-run `--latency-trace` numbers == the pre-STAGE-113 baseline; the byte-identical present (frame_count/fps) unchanged. | designed |
| **CR6** | the `Signal` adds context contention that itself slows the callback. | `Signal` is one timeline enqueue per frame (≤125/s) on the already-multithread-protected context — negligible vs the `CopyResource` it follows. | `arr` rate unchanged (same as CR1's gate). | designed |

## §4 — Build + gate (close every risk first)

Build green (VS18 vcvars64 + `cmake --build build-ra-msvc2`). Runtime gate (operator BF6 combat):
`--upload-xfer --phase-norm --copy-fence --latency-trace`. PASS iff: (a) 35 s clean, no capture freeze,
`arr` unchanged [CR1]; (b) `mapmiss` drops toward ~0 (the busy-poll is gone — the dataflow win) AND the CPU
of the C-thread drops; (c) the `copy`/`pickup` ms do NOT rise (the wait is precise, not slower) — a small
drop (~1 ms granularity) is the expected honest reclaim; (d) `frz`/watchdog 0, byte-identical-off confirmed.
Mark each risk `mitigated` only after its Verification runs first-hand; **do NOT commit with any `open`.**

## §5 — Honest expectation (declared, per §0)

The reclaim is **~1 ms latency + the ~350/s busy-poll CPU + lower jitter** — NOT a floor-mover. The floor
(`copy` ~4 ms + warp ~8 ms on the saturated GPU) is **physics, declared and accepted**. This stage buys the
*dignity* of an event-driven pipeline (the cycles model) where it is viable, and meets the silicon wall
honestly where it is not. That is the mandate.

## §6 — MEASURED (2026-06-17, BF6) — all risks `mitigated`; the prediction held

Implemented (fresh-Opus delegate + supervisor first-hand verify). **Crash gate PASS:** 35 s BF6 `--copy-fence`
ARMED — `arr` 83-86 unchanged (no capture freeze, **CR1 mitigated**), clean `done` exit (**CR2 mitigated**),
the "ARMED (D3D11.4 fence+event)" notice fired (D3D11.4 present; the force-off fallback is structural,
**CR3/CR4 mitigated**), `frz`/watchdog 0. **Same-scene A/B (light scene, warp ~0.9 ms):** freshage 12.3 (off)
vs 12.4 (on), `lat` 19.0 vs 18.9 — **LATENCY-NEUTRAL, byte-identical-off confirmed (CR5 mitigated)**. The win
is the **busy-poll: `mapmiss` 140→86/s** (~40 % fewer Map-retries; larger in heavy combat where it was ~350/s)
= the CPU/determinism dividend, scene-independent. **The prediction held exactly: no latency reclaim (the floor
is physics), a real CPU/jitter/dignity win.** Shipped default-off as the worthy event-driven capture path. No
register risk `open`. **Strategic conclusion: the input-lag floor is now PROVEN physics + the quality/freeze
budget — the felt improvement lives in PHASE-2 (better frames), not lower latency.**
