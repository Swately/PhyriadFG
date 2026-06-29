# FG_WARP_SCALING_RISK_REGISTER — the warp-scale cost lever

> **Diátaxis type:** Planning (risk register). **Normativity:** BCP 14.
> **Status:** `parked-negative` (W1 Shape-A) — `--warp-scale N` (Shape A, init-sized) is BUILT,
> **runtime-verified-safe** (rig 2026-06-23, BF6 4K combat single-GPU: N=1/2/4 ran clean, `frz 0`, no
> device-loss → the WS-1/2/3 re-init class does not arise for Shape A), **byte-identical-off** (N=1), no-game-cap
> held (pair-reals full-res), and **COMMITTED default-off** — but **rig-REJECTED on QUALITY**: operator eye, BF6
> 2026-06-23, the reduced-res warp + upscale is BLURRY (N=2 blurry, N=4 markedly worse + the object/disocclusion
> detection collapses), NOT a viable quality trade. So it is **KEPT default-off + documented** (the flag handler
> states why; UI-configurable per the operator) and **NOT recommended**. Honest reframe: it is a COST lever and
> does NOT touch the **saturated-game LATENCY** — the real felt problem on BF6 (~0.5 s, structural, present at
> N=1 too). The Shape-B runtime form stays `designed` (and now moot, given Shape A is eye-rejected). Per-risk:
> WS-6 byte-identical + WS-7 no-cap `mitigated` (verified); WS-5 quality `rejected` (eye); WS-1/2/3/4 N/A for
> Shape A (not built).
> **Tier:** **T2.** Sibling of [`FG_WARP_SCALING_MASTER_PLAN.md`](FG_WARP_SCALING_MASTER_PLAN.md) +
> [`FG_WARP_SCALING_IMPLEMENTATION_STRATEGIES.md`](FG_WARP_SCALING_IMPLEMENTATION_STRATEGIES.md).
> **Hard gate ([`canon/PLAN_TIER_PROTOCOL.md`](../canon/PLAN_TIER_PROTOCOL.md) §3):** a T2 change MUST NOT be
> committed while any risk is `open`; each MUST be `mitigated` (with a recorded first-hand verification) or
> explicitly `accepted` (bounded residual, operator-recorded). All file:line refs verified first-hand
> 2026-06-23.

## Why T2

Warp-scaling's **runtime form (Shape B,
[`FG_WARP_SCALING_MASTER_PLAN.md`](FG_WARP_SCALING_MASTER_PLAN.md) §3)** recreates the warp output image
`wapOutA` ([`main.cpp:4342`](../../apps/render_assistant/src/main.cpp)) and re-writes its descriptor binding
([`main.cpp:2794`](../../apps/render_assistant/src/main.cpp)) **mid-stream, on the crash-sensitive warp/present
path** — the [`PLAN_TIER_PROTOCOL`](../canon/PLAN_TIER_PROTOCOL.md) §1.1 trigger-1 (device-loss) + trigger-2
(concurrency on a resource the steady tick reads) surface, the same hazard
[`FG_ADAPTIVE_FLOWSCALE_DESIGN.md`](FG_ADAPTIVE_FLOWSCALE_DESIGN.md) §3 classifies T2 for the analogous FLOW
lever. The risks cluster into three families: **(1) crash / device-loss from re-init** (WS-1), **(2)
concurrency on the in-flight warp resources** (WS-2), **(3) the dogma cluster** — D-2 zero-alloc hot path
(WS-3), the re-init stall / freeze floor (WS-4), D-13 correctness of upscaled warp (WS-5), byte-identical-off
(WS-6), no-game-cap (WS-7).

(If the operator elects **Shape A** — startup-only `--warp-scale`, no runtime re-init — WS-1/WS-2/WS-3/WS-4
do not arise and the change drops to T1; that election MUST be recorded here before relying on it.)

---

## WS-1 — Device-loss / warp-image recreation · class `crash` · severity `high` · **BLOCKS COMMIT**

**Failure mode.** A runtime `--warp-scale` switch destroys `wapOutA` and re-`img_create`s it at the new extent
([`main.cpp:4342`](../../apps/render_assistant/src/main.cpp)) + re-writes the descriptor binding
([`main.cpp:2794`](../../apps/render_assistant/src/main.cpp)). If the recreate fails (out-of-memory, a driver
fault) mid-switch, the descriptor is left bound to a destroyed/half-created image → a `VK_ERROR_DEVICE_LOST`
on the next warp dispatch ([`main.cpp:7517`](../../apps/render_assistant/src/main.cpp)), on the path that now
owns the present.

**Mitigation as code.** A **last-good** recreate: `wap_out_resize(N)` builds the NEW `wapOutA` into a TEMP
handle FIRST; only on success does it (a) re-write the descriptor binding, (b) destroy the OLD image. On any
`img_create` failure it logs, keeps the OLD image + the OLD `N`, and returns false (no torn descriptor) —
the D-20 graceful-degrade discipline, shaped like the verified WAP allocation-fail degrade
`use_wap=false` at [`main.cpp:4343`](../../apps/render_assistant/src/main.cpp). Every warp/present submit on
the path stays `vk_live`-wrapped (verified [`main.cpp:7093`](../../apps/render_assistant/src/main.cpp)); a
`DEVICE_REMOVED`/`DEVICE_RESET` routes through the proven terminal-flag unwind, never a spin.

**Verification (to flip → `mitigated`).** Build-green + a forced recreate-fail (inject an `img_create` false
return) shows the warp keeps running at the prior `N`, no device-lost, no torn descriptor; a 30 s soak with
runtime switches on a real title is `--csv`-clean (no device-lost, no hang). **Status: `open`.**

## WS-2 — In-flight-resource concurrency on the switch · class `concurrency` · severity `high` · **BLOCKS COMMIT**

**Failure mode.** The steady warp tick reads `wapOutA` (write target) + samples the grids; the `--warp-scale`
switch destroys/recreates `wapOutA`. If the recreate runs while a `cmdBridge` referencing the OLD image is
recorded-but-unsubmitted or still in flight (the present fence not yet signalled), the GPU reads a freed image
→ a race / device-lost. Under `--async-present`'s ping-pong slots a per-slot-stale extent compounds it.

**Mitigation as code.** Gate `wap_out_resize(N)` to run ONLY on a **pair boundary**, AFTER the prior tick's
present fence has signalled and BEFORE the next `cmdBridge` is recorded — the warp/present fence is
`vk_live`-wrapped at [`main.cpp:7093`](../../apps/render_assistant/src/main.cpp); the resize sits in the gap
between fence-wait and the next dispatch record ([`main.cpp:7517`](../../apps/render_assistant/src/main.cpp)),
so no in-flight command references the image being recreated. The active `N` is a single per-tick scalar
threaded like `t`/`extrap` (the push-constant scalars at
[`main.cpp:7484`](../../apps/render_assistant/src/main.cpp)), read once per tick, NOT a per-slot constant —
the dispatch dims + the blit `srcOffsets` derive from the SAME scalar read, so a slot can never present at a
stale extent. No hot-path lock (Phyriad's lock-free mandate); the handshake is the fence already in the path.

**Verification (to flip → `mitigated`).** A Vulkan validation-layer run with runtime switches under load shows
no use-after-free / no sync error; a 30 s `--async-present` + `--warp-scale` switch soak is freeze-clean
(`--csv` frz=0). **Status: `open`.**

## WS-3 — Hot-path allocation / D-2 · class `dogma` · severity `medium` · **BLOCKS COMMIT** (runtime form)

**Failure mode.** A runtime re-init allocates GPU resources (`img_create` + descriptor write). If reached from
the steady warp tick, it violates D-2 (zero allocation in the steady-state hot path).

**Mitigation as code.** `wap_out_resize(N)` is a LABELLED **cold-path** helper, invoked only on a switch event
(a per-tick `N`-changed check on the pair boundary), never per tick — the same "cold path, never on the steady
tick, D-2" discipline the FG_OPTION_A `ResizeBuffers`/`dxgi_live` helpers carry
([`FG_OPTION_A_IMPLEMENTATION_STRATEGIES.md`](FG_OPTION_A_IMPLEMENTATION_STRATEGIES.md) §2 step 7). The steady
tick at `N`-unchanged does ZERO allocation — it reuses the existing `wapOutA` and the bound descriptor, exactly
as today.

**Verification (to flip → `mitigated`).** Code-review/grep confirms the only `img_create`/descriptor-write for
`wapOutA` outside init is inside `wap_out_resize`, gated by an `N`-changed predicate; a steady run (no switch)
shows no per-tick allocation (validation-layer / a counter). **Status: `open`.**

## WS-4 — Re-init stall / freeze floor · class `dogma` (graceful-degrade) · severity `medium`

**Failure mode.** Each runtime switch incurs a destroy+recreate+descriptor-write stall; a thrashing controller
(or a noisy switch signal) re-inits repeatedly → a visible per-switch hitch, the freeze-floor regression the
adaptive-flow design warns of ([`FG_ADAPTIVE_FLOWSCALE_DESIGN.md`](FG_ADAPTIVE_FLOWSCALE_DESIGN.md) §6
R-AFS-2).

**Mitigation as code.** (1) Bound `N ∈ {1,2,4}` and require a **minimum dwell** + hysteresis before a switch
(the kin controller's K-frame + min-dwell, §3 of the flow design) when the W3 adaptive controller is built;
for W1/W2 the switch is operator-driven (a flag/console), not auto, so no thrash. (2) The resize is a single
small RGBA8 image + one descriptor write (NOT the whole pipeline) → the stall is bounded; MEASURE it (§7 of the
master plan) and keep it ≤ a couple of frames or fall back to Shape A (startup-only). (3) On a switch the warp
keeps presenting the LAST-GOOD frame across the one-tick gap, never a black frame.

**Verification (to flip → `mitigated`).** Measure the re-init stall first-hand (`--csv` per-tick); it is ≤ the
chosen freeze floor and a manual switch storm does not accumulate freezes. **Status: `open`.**

## WS-5 — Correctness of the upscaled warp (D-13) · class `dogma` · severity `medium`

**Failure mode.** Reduced-res warp + linear upscale changes the output: the per-pixel commit/rescue/matte
decisions ([`wap_warp.comp:757-1896`](../../apps/render_assistant/shaders/wap_warp.comp)) are taken at fewer
points, degrading thin features (text, edges) and disocclusion bands more directly than `--flow-scale`. A
faster-but-wrong path violates D-13 (correctness before speed).

**Mitigation as code.** Ship the lever WITH a quality gate, not a cost-only flag: the C8 `fg_quality_scorer`
regression gate scores `N=1` vs `N=2` vs `N=4` (the same oracle the C8 commit wired), and the operator's eye
confirms before any default flip — `--warp-scale` stays default-off and eye-gated, mirroring the
`--flow-scale 2` quality-flip discipline (EYE-gated in `ACTION_PLAN.md`). The clamp toward 1 on a degenerate
extent ([`main.cpp:3100-3101`](../../apps/render_assistant/src/main.cpp) template) prevents a broken tiny
output.

**Verification (to flip → `mitigated`).** The C8 oracle delta at each `N` is recorded + the operator's eye pass
on a heavy-frame title; the degradation is bounded and graceful (no break), not silently shipped. **Status:
`open`.**

## WS-6 — Byte-identical-off · class `dogma` · severity `medium`

**Failure mode.** A regression in the `N==1` path would make the default (off) build differ from today —
breaking the opt-in-flag invariant.

**Mitigation as code.** At `warp_div==1` every divisor site collapses to today's code by construction: the
`wapOutA` extent stays `WW×WH` ([`main.cpp:4342`](../../apps/render_assistant/src/main.cpp)), the dispatch
stays `(WW+7)/8,(WH+7)/8` ([`main.cpp:7517`](../../apps/render_assistant/src/main.cpp)), the blit `srcOffsets`
stay `{WW,WH}` ([`main.cpp:7555`](../../apps/render_assistant/src/main.cpp)), the shader is untouched (the
off-path SPIR-V is unchanged). The flag defaults to 1 and is parsed in `parse_extra` like the other default-off
knobs.

**Verification (to flip → `mitigated`).** The C8 regression gate green on an unchanged HSR run with the flag
absent (byte-identical to the prior build); a `--csv` diff is zero. **Status: `open`.**

## WS-7 — No-game-cap / no-game-downscale dogma · class `dogma` · severity `high`

**Failure mode.** Warp-scaling could be mis-read (or mis-implemented) as a downscale/cap of the GAME's
rendered frame — the dogma's red line.

**Mitigation as code (the load-bearing distinction).** The divisor applies ONLY to `wapOutA` — OUR synthesized
output image ([`main.cpp:4342`](../../apps/render_assistant/src/main.cpp)) — and the existing linear blit
upscales it back to the bridge/present extent ([`main.cpp:7555`](../../apps/render_assistant/src/main.cpp)).
The game's captured pair-real inputs (`wPrev`/`wCur`) are SAMPLED full-res unchanged
([`main.cpp:4329-4332`](../../apps/render_assistant/src/main.cpp)); we never touch the game's swapchain,
render resolution, or present. The game's per-frame cost is untouched (the
[`FG_LSFG_HEADTOHEAD_MEASURED.md`](FG_LSFG_HEADTOHEAD_MEASURED.md) §3 identical-cost control is the proof
template). This is the §7.2-lever-#1 "minimize OUR slice," never a game cap.

**Verification (to flip → `mitigated`).** Code-review confirms the divisor is applied to `wapOutA`/the
dispatch/the blit-src ONLY, never to the WGC capture or any game-side resource; the game's per-frame GPU cost
is unchanged on/off (measured). **Status: `open`.**

> **Provenance of the no-cap rule.** The no-game-cap / no-recommend-cap invariant is an *operator-stated
> FG-arc rule* — established in [`FG_OPTION_A_MASTER_PLAN.md`](FG_OPTION_A_MASTER_PLAN.md) and reinforced
> across the saturation arc — carried with dogma force *within this arc*, but it is NOT (yet) a numbered
> dogma in [`CANON.md`](../../CANON.md) / [`canon/DOGMA_SPECIFICATIONS.md`](../canon/DOGMA_SPECIFICATIONS.md).
> The D-numbered dogmas this triad also cites (D-2, D-13, D-20) are canon; the no-cap rule's elevation to a
> D-number is an open operator decision. It is cited here by provenance, not as a canon anchor.

---

## Commit gate (the hard line)

| Risk | Class | Severity | Status | Blocks commit? |
|---|---|---|---|---|
| WS-1 device-loss / image recreation | crash | high | `open` | **YES** — until `mitigated` |
| WS-2 in-flight-resource concurrency | concurrency | high | `open` | **YES** — until `mitigated` |
| WS-3 hot-path allocation / D-2 | dogma | medium | `open` | **YES** (runtime form) — until `mitigated` |
| WS-4 re-init stall / freeze floor | dogma | medium | `open` | until `mitigated`/`accepted` |
| WS-5 upscaled-warp correctness (D-13) | dogma | medium | `open` | until `mitigated`/`accepted` |
| WS-6 byte-identical-off | dogma | medium | `open` | until `mitigated` |
| WS-7 no-game-cap | dogma | high | `open` | until `mitigated` |

**Every row is `open`/`designed`; nothing is built.** No commit of the warp-scaling runtime form while any row
is `open`. The mitigations ground in the existing WAP pass (`main.cpp` + `wap_warp.comp`) + the FG_OPTION_A
cold-path/`vk_live` discipline + the no-cap dogma. The three docs cross-link per
[`PLAN_TIER_PROTOCOL`](../canon/PLAN_TIER_PROTOCOL.md).

## Dogma checks

- **No game cap (WS-7).** The divisor is on `wapOutA` (OURS), upscaled by the existing blit; the game's render
  + per-frame cost are untouched (the [`FG_LSFG_HEADTOHEAD_MEASURED.md`](FG_LSFG_HEADTOHEAD_MEASURED.md) §3
  control). Slice-minimization, not a game downscale.
- **Zero-alloc hot path / cold-path re-init (WS-3).** The `wap_out_resize` allocation is a labelled cold path,
  reached only on a switch event on a pair boundary, never the steady tick.
- **Byte-identical-off (WS-6).** Flag default 1 → every divisor site collapses to today's code; the shader is
  unchanged; verified on an HSR run.
- **Correctness oracle (WS-5).** Reduced-res warp ships with the C8 oracle + eye gate, default-off, not a
  faster-but-wrong path.
- **Device-loss / graceful degrade (WS-1, D-20).** A failed re-init keeps the last-good image + `N`, never a
  torn descriptor or a crash.

*Made with my soul — the supervisor, for Swately.*
