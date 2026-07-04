# Optimization under GPU saturation — the 0.4.0 arc

Tier-1 planning doc (`*_MASTER_PLAN` + `*_IMPLEMENTATION_STRATEGIES` fused — one doc, the change is
substantial but not risk-bearing: the governor floor is an **advisory** lock-free control word with an
unchanged producer/consumer contract; no crash/device-loss/concurrency/data-loss surface is touched,
and the OLD behavior stays reachable behind a flag for A/B). Status: **measured — finding #1 fix
DESIGNED + implemented + bench-gated this session.** The arc header for the 0.4.0 work.

The key words MUST / MUST NOT / SHOULD / MAY are BCP-14. Every number below the "MEASURED" heading was
taken first-hand on the bench this session (`tools/gpu_load.exe` + `tools/ball_zoo.ps1`); every number
under "DESIGNED" is a threshold choice with its rationale, not a measurement.

---

## 0. The operator's thesis — active bounded-slice reservation

PhyriadFG's frame-generation is an **active, bounded-slice reservation** strategy: it reserves a
bounded slice of the GPU for hallucination and defends the *output cadence* against a rising source
rate + rising game GPU load, degrading its own quality stack gracefully when its slice is squeezed.
This is deliberately UNLIKE LSFG's **passive, source-proportional equilibrium**, where the generator
rides whatever the source hands it and settles at whatever multiplier the shared GPU allows — no
active defense of a floor, no self-health governor. The 0.4.0 arc is about making that active
reservation *correct under contention*: the FG must shed only when ITS OWN slice is actually starved,
never merely because the GPU-as-a-whole is busy.

---

## 1. MEASURED — the collapse curve (this session, first-hand)

Bench: `tools/gpu_load.exe` (in-tree; `cl /O2 gpu_load.cpp d3d11 dxgi d3dcompiler winmm`) is a
ring-fed compute loader whose `--load N` duty % was verified 100% real on `nvidia-smi`. Source =
`ball_zoo.ps1 -Fps 60`; FG defaults; the loader pinned to **GPU A (`--gpu 0`, the RTX 4090 primary)**
at 0/60/85/99 % external load.

| external load on GPU A | output uniq (/s) | lat (ms) | governor tier | bwd-skip |
|------------------------|------------------|----------|---------------|----------|
| 0 %                    | 238–241          | 17.2     | unloaded value | not chronic |
| 60 %                   | 238–241          | ~18.0    | **tier:5**    | **100 %** |
| 85 %                   | 238–241          | ~18.7    | **tier:5**    | **100 %** |
| 99 %                   | 238–241          | 19.6     | **tier:5**    | **100 %** |

**Reading it:** the FG's OWN pipeline is perfectly healthy across the whole sweep — output uniqueness
holds 238–241/s and latency rises only +2.4 ms at 99 % external load. Yet the governor slams the full
deep-shed (`tier:5` + `bwd-skip:100 %`) at just **60 %** external load, and holds it all the way up.
The quality stack (backward optical flow, GME refinement, holon period) is dismantled while nothing in
our pipeline is actually behind.

---

## 2. Finding #1 — the util false-positive

`g_gov_floor` (the util→tier floor the F-thread applies as `max(cpu_ladder, floor)`) was keyed on
**global GPU-A utilization** (`governor_floor_for_util(gpuA_pct, gov_util)`). Global util **cannot
distinguish "the GPU is busy" from "WE are starved."** A game (or a bench loader) that pegs the 4090
drives util past the `gov_util-12/-6/gov_util` bands and forces tiers 3/4/5 — even when the FG is
consuming every captured pair on time, freshage is nominal, and the warp fence is not inflating.

In a real game this false-positive is a component of the 99 %-collapse the operator reports: the FG
throws away its quality the moment the GPU gets busy, rather than the moment its own slice is squeezed.

The CPU tier ladder in `flow.cpp` (`pressure_tier` from `t_pair_ema` vs `pair_budget`, ~1410–1462) is
**already self-health-keyed** and is CORRECT — it is left untouched. Only the util-driven FLOOR that
sits on top of it is mis-keyed.

---

## 3. DESIGNED — re-key the floor from global util to the FG's OWN slice health

The floor is produced by the PRESENT thread (`present.cpp` ~1832–1839) and consumed by the F-thread
(`flow.cpp` ~1467). The producer is re-keyed; the consumer and the atomic's contract are unchanged.

**Consumer map of `g_gov_floor` (verified first-hand this session):**

| site | role |
|------|------|
| `core/globals.cpp:8` / `globals.hpp:28` | the atomic — advisory, lock-free, 1 producer → 1 consumer |
| `present.cpp` ~1832–1839 | **PRODUCER** — decode util→floor + raise-fast/lower-on-dwell, `store` |
| `flow.cpp` ~1467 | **CONSUMER** — `if(gf>pressure_tier) pressure_tier=gf` (max over the CPU ladder) |
| `present.cpp` ~893 | `warp_light` — the P-side warp shed, derived from the SAME decode (`floor>=5`) |

### 3.1 The new keying

The floor MUST engage only on the FG's OWN distress. Util is retained as a **corroborator**, never
sufficient alone. All inputs are always-on and already computed on the PRESENT thread:

- **(a) freshage distress** — `freshage_ema_ms` (the EMA of `now − tcap` at set-detect = our observed
  pipeline delay) trending above `K_fresh × T_src`, where `T_src = src_interval_ema_ms` is the live
  source period. When F keeps up, freshage sits at a low multiple of the source period (~7 ms at a
  120 fps source per `PREDICT_MODE_PLAN.md`); when F starves and sets land chronically late, freshage
  inflates. **`K_fresh = 3.0`** (a set that is >3 source-periods old at detect is genuinely behind, not
  jitter). Guarded on `T_src>0 && delay_init` (no false fire before the EMA is seeded).
- **(b) warp/present distress** — `wap_warp_ema` (the per-tick warp/present λ cost EMA) inflated vs its
  OWN slow baseline `warp_base` (a 0.98/0.02 EMA that tracks the calm cost). Distress when
  `wap_warp_ema > K_warp × warp_base` with `warp_base` seeded and above a 0.2 ms noise floor.
  **`K_warp = 1.6`** (a 60 %-inflated warp fence-wait = the 4090 is starving our slice). This catches
  the case where freshage is still nominal but the 4090 is beginning to squeeze the warp.
- **corroborator** — `util_floor = governor_floor_for_util(gpuA_pct, gov_util)`, the SAME decode as
  today. It supplies the floor's DEPTH (which band, 3/4/5); self-distress supplies the GATE.

**Rule:** `target = self_distress ? util_floor : 0`, where `self_distress = fresh_distress ||
warp_distress`. Consequences, by design:

- util high **AND** self-distress → floor engages at the util band (defend the slice).
- util high **ALONE** (healthy FG) → `self_distress=false` → `target=0` → NO floor. **This is the
  finding-#1 fix.**
- util cool but self-distressed → `util_floor=0` anyway → this path contributes 0; the F-thread's OWN
  `t_pair_ema` ladder does the escalation (the self-keyed path, untouched). The two arms stay
  consistent — the floor never *undoes* a legitimate self-keyed escalation, it only ever *adds*.

A **distress latch** with a short dwell (`kDistDwell = 3` util updates ≈ 3 s) de-bounces the
`self_distress` flag so a one-tick freshage/warp spike cannot flap the floor. The existing raise-fast /
lower-on-dwell (`kGovDwell`) on the resulting `target` is kept, mirroring the ladder discipline (the
0.95/0.80-style bands the CPU ladder already uses).

`warp_light` (present.cpp ~893) is gated by the SAME latched `self_distress` so the P-side warp shed
and the F-side tier floor never disagree (one decision, two arms) — the same one-mapping discipline
already documented at that site.

### 3.2 Honest prints

When the floor engages or releases, the PRESENT thread prints WHICH signal fired (the operator reads
these logs), e.g.:

```
[ra] gov-floor ENGAGE tier:5 (freshage 54.3ms > 3.0x Tsrc 16.7ms; util 99% band:5)
[ra] gov-floor RELEASE (freshage 21.1ms, warp 1.2ms/base 1.1ms; util 34%)
```

### 3.3 The A/B flag

`--gov-util-floor` restores the OLD pure-util behavior exactly (`target = util_floor`, no distress
gate). **Default OFF** after this fix; the self-keyed path is the default. No new flags beyond it. This
is the A/B proof for gate G1: the same bench run with `--gov-util-floor` reproduces `tier:5` at 60 %.

---

## 4. The bench limit — pure compute ≠ game-class contention (planned extension)

`gpu_load.exe` is a pure **compute** loader (a ring-fed dispatch). It pegs GPU util and starves the
shared queue, but it does NOT contend for the graphics/copy engines, the present queue, or VRAM
bandwidth the way a real game's render does. So the bench proves the *util false-positive is
eliminated* and that self-distress still escalates, but it under-represents game-class contention.

**Planned extension (not this slice):** a `--graphics-load` mode for the loader (a real per-frame
render + present on GPU A) to reproduce present-queue + copy-engine contention, which is the contention
class that actually squeezes our warp fence and our capture copy. The self-keyed floor is designed to
respond to exactly that (warp distress + freshage), so the graphics-load mode is the honest validation
of the fix's *intent*, where the compute loader validates its *mechanism*.

---

## 5. REGISTERED FUTURE CASE — the CPU bottleneck (operator-requested, NOT in this slice)

A CPU-saturated game starves our **C/F/P threads**, not the GPU. The GPU-util governor is blind to
this by construction — util can be LOW while our threads are being descheduled and missing their
budgets. The natural sensors already exist in the pipeline:

- **`fwake`** — the publish→consume wake latency (already surfaced in `[lat-trace]`): if the F-thread's
  wake after a publish inflates, the OS is not scheduling us.
- **cv wake delays** — the condition-variable wake latency on the C→F and F→P handoffs.

The response would be a CPU-side analogue of this floor (shed work / raise thread priority /
`D3DKMTSetProcessSchedulingPriorityClass`) keyed on `fwake` + cv-wake distress, NOT on util. This is
registered as its own arc slice; it is explicitly **out of scope** for the finding-#1 fix and no code
for it lands here.

---

## 6. The remaining arc levers (0.4.0 backlog)

1. **Never-undo-the-multiplier tier re-audit** — audit every tier's shed list to confirm no shed ever
   *reduces* the frame multiplier below the source-proportional floor (the shed must trade quality, not
   throughput). The tier ladder was designed this way; the audit verifies it holds under the new floor
   keying.
2. **`D3DKMTSetProcessSchedulingPriorityClass`** — raise our process's GPU scheduling priority so the
   reserved slice is honored by the scheduler, not just requested. Pairs with the CPU-bottleneck case.
3. **`--copy-device` under saturation** — route the capture copy through a device that is not the
   saturated one, so our ingest is not starved by the game's GPU load. Measured under the graphics-load
   mode once it exists.

---

## 7. Gates (all measured on the bench — kill all zoo/FG/loader between runs; fresh zoo per run)

| gate | premise | pass criterion |
|------|---------|----------------|
| **G1** | external-load false-positive ELIMINATED | loader on GPU A at 60/85/99 % + zoo 60 fps + FG defaults → tier stays at its unloaded value (no chronic `tier:5`, bwd-skip not chronic), uniq ≥238/s, lat ≤ baseline+4 ms. **A/B:** the same run with `--gov-util-floor` reproduces the old `tier:5`-at-60 %. |
| **G2** | self-starvation still escalates | loader on GPU B (`--gpu 2`, the 1080 Ti flow GPU) at 99 % → the F ladder escalates legitimately (`t_pair_ema` inflates); tiers rise, output degrades gracefully (no crash). |
| **G3** | no-load regression | zoo 60 fps and 15 fps unloaded → identical to today (tier normal, cadence CSV clean, stats shape unchanged). |
| **G4** | OFF-path | `--gov-util-floor` reproduces today's `tier:5`-at-60 % exactly; build clean; no new flags beyond it. |

The G1 vs G4 pair IS the fix's proof: same external load, opposite tier — the new default holds the
quality stack, the OLD flag throws it away.
