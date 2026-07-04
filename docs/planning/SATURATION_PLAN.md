# Optimization under GPU saturation — the 0.4.0 arc

Tier-1 planning doc (`*_MASTER_PLAN` + `*_IMPLEMENTATION_STRATEGIES` fused — one doc, the change is
substantial but not risk-bearing: the governor floor is an **advisory** lock-free control word with an
unchanged producer/consumer contract; no crash/device-loss/concurrency/data-loss surface is touched,
and the OLD behavior stays reachable behind a flag for A/B). Status: **measured — finding #1 fix
DESIGNED + implemented + bench-gated (§1-§7), and the `--gpu-priority` levers implemented + A/B'd on
LIVE BF6 (§8): `realtime` cures the collapse (mult 0.84→2.76) at a -18.6 % game cost → OPT-IN.** The
arc header for the 0.4.0 work.

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
   **→ IMPLEMENTED + MEASURED, see §8 (`--gpu-priority`).**
3. **`--copy-device` under saturation** — route the capture copy through a device that is not the
   saturated one, so our ingest is not starved by the game's GPU load. Measured under the graphics-load
   mode once it exists. **→ MEASURED on live BF6 in §8 (pair 4): net-negative without REALTIME.**

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

---

## 8. §levers-measured — `--gpu-priority` on LIVE Battlefield 6 (task 2, measured this session)

**The regime under test** (live BF6, GPU-A 99 %, quantified before this slice): the P-tick present
BLOCKS under the game's contention (iter 9-12 ms avg / 20-29 ms worst; the warp itself healthy at
2-3.4 ms), the gen-ring guard then holds F (preflow spin 13-16 ms), F consumes only ~48 of ~107
captured pairs/s, freshage 36-47 ms (≈4× the source period), multiplier ~0.85×, lat 78-89 ms, gme dis
94-97 %. The governor fix (§3) behaves honestly here — genuine distress, named in the engage prints.
The cure attempted in this slice: **getting our GPU slices via scheduling priority.**

### 8.1 The three levers (one flag: `--gpu-priority {high|realtime}`, DEFAULT OFF)

| # | lever | where it landed | outcome on this rig (printed honestly at startup) |
|---|-------|-----------------|-----------------------------------------------------|
| 1 | `D3DKMTSetProcessSchedulingPriorityClass` (gdi32.dll, the OBS "GPU priority" mechanism) | `src/core/main.cpp` right after `parse_args`, BEFORE any device creation. Enum verified FIRST-HAND from the WDK header `Windows Kits/10/Include/10.0.26100.0/shared/d3dkmthk.h:4534-4542` (IDLE=0…ABOVE_NORMAL=3, **HIGH=4, REALTIME=5**) + prototype `:5954`. | **HIGH=4: NTSTATUS 0x00000000 OK. REALTIME=5: NTSTATUS 0x00000000 OK — no elevation needed on this rig** (the expected `STATUS_PRIVILEGE_NOT_HELD 0xC0000061` did not occur). |
| 2 | `VK_EXT/KHR_global_priority` on device A's queues | `src/core/device.cpp` `vdev_create(..., global_priority)` — chained per queue create-info, with a `VK_EXT_global_priority_query` pre-check and a NOT_PERMITTED retry at normal. All names/values verified first-hand from `G:\VulkanSDK\Include\vulkan\vulkan_core.h` (HIGH=512, REALTIME=1024, `VK_ERROR_NOT_PERMITTED=-1000174001`). Device A only (B/G stay default). | **Driver-refused: the query ext reports qfam 0 on the RTX 4090 supports nothing above MEDIUM** → honest downgrade print, queues stay normal. **Lever 2 is a measured NO-OP on this NVIDIA Windows driver** (both at high and realtime). |
| 3 | `IDXGIDevice::SetGPUThreadPriority(+7)` on the D3D11 CAPTURE device | `src/capture/capture.cpp` `d3d_init(..., gpu_thread_prio)`, right after the existing IDXGIDevice query. | **OK (hr S_OK), lever ACTIVE** — but see the A/B: no measurable effect without lever 1 at REALTIME. |

Fallback behavior: every lever prints its own outcome and NEVER aborts; a VK create-failure with the
priority chained gets ONE retry at normal priority.

### 8.2 The paired A/B (live BF6, operator playing; ~45 s runs, back-to-back pairs, steady-state
means after a 10-stat-line warmup; `in` = the game's captured fps = the cost side)

| run | in (game) | present | **mult** | cons | lat ms | fresh ms | spin ms | iter/worst ms | copy ms | fwake ms |
|-----|-----------|---------|----------|------|--------|----------|---------|---------------|---------|----------|
| p1 a base      | 106.4 | 89.4 | 0.84 | 47.9 | 82.8 | 42.5 | 13.9 | 11.2 / 22.8 | 6.0 | 9.6 |
| p1 b high      | 106.2 | 90.4 | 0.85 | 48.7 | 79.9 | 40.9 | 14.0 | 11.1 / 24.1 | 6.2 | 8.7 |
| p2 b high      | 106.6 | 87.7 | 0.82 | 48.5 | 80.5 | 41.4 | 13.9 | 11.4 / 23.5 | 6.4 | 8.7 |
| p2 a base      | 106.4 | 86.2 | 0.81 | 49.4 | 78.6 | 40.6 | 14.3 | 11.6 / 24.4 | 6.2 | 8.7 |
| p3 a base      | 106.1 | 94.7 | 0.89 | 48.1 | 82.0 | 41.8 | 14.7 | 10.6 / 23.5 | 5.9 | 9.1 |
| p3 b high      | 106.1 | 92.0 | 0.87 | 49.2 | 79.5 | 40.5 | 13.6 | 10.9 / 23.0 | 5.8 | 9.7 |
| p4 b high      | 106.9 | 87.2 | 0.82 | 47.6 | 79.5 | 41.6 | 14.2 | 11.5 / 24.4 | 6.3 | 8.2 |
| p4 c high+copy | 106.5 | 83.6 | 0.78 | 46.6 | 81.2 | 42.4 | 15.0 | 12.0 / 26.7 | 4.6 | 8.0 |
| p5 a base      | 106.7 | 92.2 | 0.86 | 48.9 | 79.9 | 41.2 | 14.4 | 10.9 / 24.1 | 6.3 | 8.6 |
| **p5 d realtime** | **87.4** | **239.8** | **2.74** | **87.4** | **19.5** | **10.1** | **0.0** | **3.9 / 5.8** | **1.0** | **0.0** |
| **p6 d realtime** | **86.2** | **239.9** | **2.78** | **86.2** | **21.4** | **11.9** | **0.0** | **3.8 / 5.8** | **1.0** | **0.0** |
| p6 a base      | 107.3 | 88.7 | 0.83 | 48.0 | 80.2 | 41.8 | 14.6 | 11.3 / 24.1 | 6.0 | 9.1 |

Within-run `in=` trajectories are steady (baseline 105-109 every line; realtime 83-91 after a 3-line
transient) — the game-fps change is CAUSED by the config, not scene drift, and p6 (order-flipped)
confirms p5 exactly: the game bounced back to 107 the instant the baseline run replaced the realtime
one.

### 8.3 Verdict per lever

- **`high` (levers 1@HIGH + 3; lever 2 driver-refused): DEAD.** Three paired runs, mult Δ within
  ±0.03 (noise), every distress metric unchanged. WDDM HIGH does not preempt a foreground game's
  submissions on this scheduler, and +7 capture-thread priority alone moves nothing.
- **`high --copy-device`: DEAD (net-negative).** mult 0.82→0.78 in its pair, worst iter +2.3 ms. The
  second D3D11 device adds copy overhead (though `copy` drops 6.3→4.6 ms, the pipeline pays more
  elsewhere). Without REALTIME backing it, decoupling the copy queue buys nothing.
- **`realtime` (lever 1@REALTIME + 3): TRANSFORMATIVE — the collapse is CURED.** Confirmed by two
  order-flipped pairs: multiplier 0.84→**2.76** (present 88→**240/s**, the full tick rate, uniq=240/s),
  cons = in (F consumes EVERY captured pair), lat 80→**20.5 ms**, freshage 41.5→**11 ms** (healthy,
  <1× T_src), preflow spin 14→**0**, iter worst 24→**5.8 ms**, INVISIBLE copy 6.2→**1.0 ms**, fwake
  8.9→**0**. The whole starvation chain (present-block → ring-hold → F-starve) decongests at once.
- **The cost, honestly:** the game loses 106.6→86.8 fps ≈ **-18.6 %** — ABOVE the ≤10-15 % band this
  protocol set. Two mitigating notes, reported plainly: (a) part of that cost is the FG's own new
  work (the pipeline runs 240/s instead of 88/s on the same GPU — it is USING the slice it won, gpu-A
  92-93 % vs 99 %); (b) the cost is instantly reversible (p6). Net screen experience: 87 fps game +
  240 uniq/s smooth output + 20 ms lat, vs 107 fps game + 88/s stuttering output + 80 ms lat.

**Recommendation: `--gpu-priority realtime` = OPT-IN (ship the flag, do NOT default it).** It exceeds
the game-cost band, needs per-game/per-rig validation, and WDDM REALTIME for an unelevated process is
rig-dependent (it happened to be granted here). `high` and `--copy-device` earn nothing on this rig —
keep them available for other-rig A/B only. Promotion to default would require the game-cost gate
(≤10-15 %) to pass, plausibly via a capped-output mode (e.g. `--target-output-fps` limiting the FG's
own new load) — registered as the next slice's question: **realtime + output-cap sweep**.

### 8.4 Honest anomalies

- **`dis` stays 94-97 % in EVERY config, including healthy realtime** — gme dissidence under BF6 is
  dominated by real scene motion + `bwd-skip:100%` (tier-5 forces bwd off), NOT by pair staleness
  alone. The earlier reading "dis 54-70 % = stale pairs" conflated two sources; staleness is gone
  under realtime (freshage 11 ms) yet dis stays high.
- **tier:5 persists under realtime via the warp-distress arm's low-start ratchet**: the engage print
  named it honestly (`warp 3.68ms/base 1.62ms`). The warp cost early in the run (~1.6 ms) forms a LOW
  `warp_base`, and the sustained contended warp (2.7-3.7 ms) then reads as >1.6× baseline forever.
  The §3 design note anticipated this feedback; under BF6 it manifests. Candidate fix for a later
  slice: freeze `warp_base` updates while distress is latched (or a windowed median). NOT changed in
  this slice — the shed it causes is conservative (quality-, not throughput-reducing), and output is
  a full 240 uniq/s regardless.
- **The stat-line cadence is per-90-presents, not per-second** — under realtime (240/s) the same 45 s
  yields ~119 stat lines vs ~45. The steady-state means above are still time-honest (every line is a
  fixed-work window), but the warmup skip covers less wall-time in the realtime runs.
- REALTIME succeeding unelevated (NTSTATUS 0) contradicts the common claim that it requires
  elevation — on this rig (Win 11 Pro 26200) the call was granted to a normal process. Verified
  twice; do not assume it generalizes.

---

## 9. Output-side decimation — the cost-return hypothesis, REFUTED

**Hypothesis (task 3):** `--gpu-priority realtime` cures the collapse but costs the game ~19-33 %
fps. If we cap our OUTPUT rate (present fewer frames), the freed GPU should return to the game.

**First, a measured negative on the OLD lever.** `--target-output-fps` pre-decimation only ran the
s2 *content-quantizer*: it re-presented duplicates while the pipeline still ticked+warped at panel
rate. Sweep (live BF6, realtime + cap {180,160,144}): present stayed 240.6-240.7, `uniq` dropped
241→235→222→195, game `arr` FLAT at 86-88. Same GPU cost, fewer unique frames — **returns nothing**.

**The fix — TRUE tick decimation.** With a cap set (default mechanism; `--no-decimate` restores the
s2 quantizer for A/B), the P loop SKIPS warp+present entirely on non-selected vblank slots (flip-chain
persistence holds the previous frame) so our GPU cost scales with the output rate. v1 snaps the
request to the nearest exact DIVISOR of `refresh_hz` (the fixed-panel even-vblank constraint; ties
prefer the lower rate), honest print at present-init. The clock/PLL/NCO/governor all advance every
tick in panel units (the decimation gate `continue`s AFTER them — the PLL-units invariant); only
selection/warp/present/bookkeeping decimate. `dec_every==1` (cap≥panel / off) → byte-identical.

Verified (ball zoo, 240 panel): cap 120 → present locks 120.0, cap 80 → 80.1, cap 160 → snaps 120.2,
all slip 0 / frz 0 / clean cadence; no-cap 240.7 byte-identical. **The mechanism is correct.**

**But the cost hypothesis is REFUTED (measured, live BF6, realtime, ~30 s means + operator's own
in-game fps counter):**

| config | present | game `arr` | lat mean | lat min | gpu-A |
|---|---|---|---|---|---|
| realtime uncapped | 240.7 | 83 | 35.5 (1 hitch 468) | **14.5** | 91 % |
| realtime + cap 120 | 120.1 | **79** | 22.9 | **18.5** | 88 % |

The game does NOT recover (arr 83→79, within scene variance; operator's own fps counter: unchanged).
gpu-A barely moves (91→88 %). And the latency FLOOR WORSENS (min 14.5→18.5 ms — at 120 output the
displayed content updates half as often; the operator felt this as "same or slightly worse"). The
lower *mean* is only fewer hitches in that window.

**Root cause of the game's cost — it is CAPTURE-side, not OUTPUT-side.** Halving our present/warp
work returned nothing because the game's saturation tax is the WGC `CopyResource` of every GAME frame
(scales with the game's fps, not our output) + memory-bandwidth contention — a per-game-frame cost
decimation cannot touch. This is the irreducible external-capture FG tax; LSFG pays the same. What
`realtime` buys is making that cost PLAYABLE (a full-rate smooth output), not cheaper.

**Verdict:** decimation ships as an OPT-IN output-rate / power lever (default OFF, byte-identical;
correct even-divisor lock), NOT as the cost-return solution it was hypothesized to be. **Arc redirect:
the game-cost lever, if one exists, is on the CAPTURE side** — re-measure `--copy-device` for the
GAME's fps specifically (task 2 only A/B'd it for OUR latency), and investigate copy-queue / bandwidth
isolation. Registered as the next 0.4.0 slice.
