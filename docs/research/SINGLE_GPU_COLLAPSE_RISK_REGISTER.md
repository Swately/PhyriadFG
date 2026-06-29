# SINGLE_GPU_COLLAPSE_RISK_REGISTER — DCAD Phase 2/P1 (run + measure PhyriadFG on one GPU)

> **Diátaxis type:** Risk register + implementation plan (a Tier-2 gate artifact, per
> [`PLAN_TIER_PROTOCOL`](../canon/PLAN_TIER_PROTOCOL.md)). **Status:** `shipping`/`measured` (2026-06-20) —
> IMPLEMENTED + build-green + **Vulkan-validation-layer-CLEAN** + the P1 slice MEASURED + default path byte-identical.
> The design came from a 9-agent adversarial workflow (`wf_47ed952f-7d9`: 6 code-mappers + 1 synthesizer + 2
> crash-class critics, 1.2 M tokens) whose critics overturned three synth claims; the supervisor's
> validation-layer run then caught a FOURTH crash-class defect the agent missed (a command-pool 3-thread race →
> segfault, now **CR9**, fixed). **No risk remains `open`** (CR1-CR3/CR5-CR9 `mitigated`, CR4/CR8-small-GPU
> `accepted`/deferred, CR8-this-rig `measured`) — the Tier-2 commit gate is satisfied. See **§2.1 Discharge**.
> RESULT: `--force-single-gpu` on the 4090 runs at 238-244 fps / frz 0.0 / gpu A:94-99% B:0% G:0% (B+G truly idle),
> **ZERO validation errors**, gme intact (CR5: `gme(dis fit:~1.2ms)`), and the **single-GPU FG slice = freshage
> ~15-18 ms (pickup~5[conv~3] + build~8[compute~6] + detect~3) + warp ~2 ms → game→screen ~20-22 ms** — the
> DCAD load-bearing unknown, measured for the first time. **Tier-1 parent:**
> [`FG_ARCHITECTURE_DCAD_MASTER_PLAN.md`](FG_ARCHITECTURE_DCAD_MASTER_PLAN.md) §8 Phase 1+2. **Verifiable-reference
> mandate:** every `file:line` is from the agents' first-hand read at HEAD `15b1c3f`; the supervisor re-verifies each
> before the edit lands.

## §0 — Goal + why Tier-2

**Goal:** make PhyriadFG run on **ONE GPU** (device A alone) — the most common user config, today **impossible** (hard-exit
at `main.cpp:2765` `if(!pA||!pB){…return 1;}`) and **never measured** (the DCAD load-bearing unknown). Add a
`--force-single-gpu` flag to exercise the degenerate path on the 3-GPU rig, then **measure the single-GPU FG slice in
ms** (P1). **Tier-2** because it touches device-loss/crash + concurrency (two triggers): re-pointing the optical-flow +
gme producer from device B onto device A's queues is crash-class if any `B.dev`-null site is missed or two threads race
one `VkQueue` handle.

**Default-path safety:** `single_gpu` activates ONLY with `--force-single-gpu` OR when no 2nd discrete GPU physically
exists. On the operator's 3-GPU rig the DEFAULT run is **byte-identical** (nothing reads `single_gpu`). Blast radius to
normal use is contained.

## §1 — The refined change set (critique-corrected)

`bool single_gpu = cfg.force_single_gpu || (pA && !pB);` derived right after device selection; `VDev& FD = single_gpu ?
A : B;` threads the flow/gme producer onto A. The submit lane: **P owns A.q (present, unchanged); F (flow+gme) is the
SOLE submitter to A.q2; convert is merged or serialized, NOT a 2nd A.q2 submitter (CR3).**

| # | Change | Site | Risk |
|---|---|---|---|
| 1 | `--force-single-gpu` flag in `parse_extra` (C1061 convention; sets `c.force_single_gpu`, returns 0) | `main.cpp:1029-1099` (lambda), Config struct | low |
| 2 | Relax the hard-exit to `if(!pA){…return 1;}`; derive `single_gpu`; under `--force-single-gpu` null out `pB` AND `pG` (drive the true degenerate, no aliasing) | `main.cpp:2746-2765` | CR-entry (Tier-2) |
| 3 | Skip B's `vdev_create` when `single_gpu`; **force `want_pfg=false`** (else ofpA double-allocates a full OFP on A — CR6); convert lane resolved (CR3) | `3084` (vdev_create), `2586` (want_pfg), `3128` (convert resolve) | CR3/CR6 |
| 4 | `FD`-route the assist `ofp` + Bframe/Cinterp/Bflow + mv_prev/mvsm + cmdB/cmdB_bwd/cmdB_fwd + fB/fB2/fB_fwd + the oneshot(B) layout barriers onto A — **the COMPLETE census, CR2** | `3465-3479, 3672, 3690-3705, 4156-4187` | CR2 (crash-class) |
| 5 | Route the flow SUBMIT to A.q2 (`submit_wait_q2`) — **and the consume-side waits** `consume_wap`@5462 + bidir fB2@5689 to A.dev (CR2 census) | `5462, 5607, 5689, 6021, 6033` | CR1/CR2 |
| 6 | gme: minimal cut rides the CPU fit (the `B.dev==null` guard @4104, byte-identical) — **but the host bridges MUST be re-pointed to A (written by the on-A copy-out), or the CPU fit reads a null/stale buffer (CR7)** | `4104, 5982, 5992-5993, 5588` | CR7 |
| 7 | Re-point (not just gate-off) the B-side host-bridge imports to A — **a forgotten import early-returns false (NOT a crash) and silently flips use_gme/bidir/ambig OFF via the ok-cascade (CR5)** | `1950` (hbuf_import early-return), `3252-3443` (imports), `3358-3429` (ok-cascade) | CR5 (silent) |
| 8 | Measurement: store `c_conv_us` in the A-convert branch (mirror the iGPU branch @4678) → `[lat-trace] conv` is non-zero single-GPU; add a derived `[fg-slice]`/csv `fg_slice_ms` = conv+compute(flow+gme)+warp+present | `4627-4644`, `4677-4679`, `8067-8078` | low (measurement-only) |

## §2 — Risk register

| ID | Failure mode (+ site) | Mitigation AS CODE | Verification | Status |
|----|----|----|----|----|
| **CR1** | **`A.q2==A.q` on a single-graphics-queue GPU** (`q2mode` stays 2 when `qfs[qfam].queueCount==1`, `d.q2=d.q` @1894): the F-thread flow `submit_wait_q2` (@1997) falls through to A.q and **races the present thread P** → device-lost. The live WAP collapse has NO `A.q2!=A.q` gate (only the dead pfg path gates @3749). The 4090 hides it (`queueCount>1`). | At startup under `single_gpu`: detect `A.q2==A.q` → **hard-refuse** with a clear message (the safe first cut; `--force-single-gpu` on the 4090 passes since `A.q2!=A.q`). The serialize-on-A.q-behind-a-mutex degraded mode is a documented follow-up (operator policy, §4). NEVER a silent `submit_wait_q2` fallthrough. | Inspect the guard; on the 4090 confirm `A.q2!=A.q` (the run proceeds); a validation-layer run shows no queue-sync error. | `open` |
| **CR2** | **Null-`B.dev` Vulkan CREATION + consume-side waits** crash immediately when `B=VDev{}`. COMPLETE census (critics): `img_create(B,Bframe/Cinterp/Bflow)` @3465-3471, `oneshot(B,…)` barriers @3476/3479, `ofp.init(B…)` @3672, `vkAllocateCommandBuffers(B.dev,&cmdB)` @4156 (UNCONDITIONAL), `vkCreateFence(B.dev,&fB)` @4182, `cmdB_fwd`@4163 / `fB_fwd`@4184, mv_prev/mvsm @3690-3705, **`consume_wap` `vkWaitForFences(B.dev,pc.fwd_fence)` @5462**, **bidir `vkWaitForFences(B.dev,&fB2)` @5689**. | `FD`-route EVERY one (`VDev& FD = single_gpu?A:B`); the two consume-side waits use `FD.dev`. Teardown is largely self-protecting (img_destroy/hbuf_destroy guard the resource handle not `d.dev`; `vkDeviceWaitIdle(B.dev)` @8237 already `&& B.dev`-tested) — but trace every `goto done` predecessor to confirm. | A grep proves NO bare `B.dev`/`B.q`/`B.pool` remains on the `single_gpu` path; build green; `--force-single-gpu` runs without crash; validation-layer clean. | `open` |
| **CR3** | **Convert/flow collide on A.q2 (DESIGN-INTRODUCED crash-class).** Forcing `convert_gpu=CG_PRIMARY` routes the C thread's convert to A.q2 (@4644) while the F thread's flow also goes to A.q2 → **two threads on one `VkQueue` handle** = external-sync violation. The codebase's own resolver @3124-3127 prevents this but only under `fg_gpu==FG_PRIMARY` (FALSE in the default FG_AUTO). | Pick ONE crash-safe lane (NOT two submitters on A.q2): **(a)** merge convert into the F-thread's `cmdF` (one submitter), OR **(b)** serialize C+F on A.q2 behind a dedicated mutex (the `g_q_mtx` precedent @4622), OR **(c)** A.qT (rejected for the minimal cut — needs `--upload-xfer`→`--async-present`, two more Tier-2 paths). **Decision: (b) mutex-serialize for the first cut** (smallest diff, provably race-free), measure, then (a) as an efficiency follow-up. | Inspect: confirm only ONE thread submits to each of A.q/A.q2 at any time (or the mutex wraps both A.q2 submitters); validation-layer shows no concurrent-queue-use error. | `open` |
| **CR4** | **A.qT lacks COMPUTE** if any dispatch (convert/on-A gme) is routed there: cvPipe + gme reduce/solve/dissidence are COMPUTE → a transfer-only `qfamT` = device-lost. | The minimal cut does NOT use A.qT (CR3 picks mutex-serialize on A.q2). If a future efficiency cut uses A.qT, replicate the `--upload-xfer` CR8 guard @3780 (force off when `qfamT` lacks COMPUTE). | N/A for the minimal cut (no A.qT dispatch); documented for the follow-up. | `accepted` (deferred) |
| **CR5** | **Silent feature-strip (NOT a crash, worse).** `hbuf_import(B,…)` early-returns false on a null device (`@1950 if(!d.has_emh||!d.pfnHostPtr) return false`) → the ok-cascade @3358-3429 flips `use_gme/use_bidir/use_ambig` OFF with only a printf → a SILENTLY degraded FG, violating the "byte-identical" claim. | Re-POINT the B-side imports to A (`FD`), do NOT merely `!single_gpu`-gate them out; the on-A copy-out writes the same host bridges A re-reads. If a feature is *intended* off single-GPU, print it LOUD (not via the silent cascade). | Confirm the `single_gpu` run keeps gme/bidir/ambig ON (the stat line shows `gme(dis:…)`, not stripped); diff the feature banners vs the multi-GPU run. | `open` |
| **CR6** | **ofpA double-allocates on A.** `want_pfg=(fg_gpu!=FG_ASSIST)` is TRUE by default → ofpA + AframeA + cmdA_fg + fA_fg are allocated on A even though the pfg path is dead (FG_AUTO) → a 2nd full-res OFP pyramid inflates the unmeasured VRAM slice (material on a 4 GB target). | `if(single_gpu) want_pfg=false;` before the ofpA/AframeA allocations (@2586/3492-3496/3707). | Confirm ofpA/AframeA are NOT allocated under `single_gpu` (VRAM + a guard inspection). | `open` |
| **CR7** | **CPU-gme reads unwritten/null host bridges.** The CPU `gme_fit_affine` reads `hostMV/hostSAD/hostDIS`, written by the on-A ofp copy-out into `hMV_b/hSAD_b/hDIS_b` (@5992-5993/5588). If those B-side bridges are gated-off rather than re-pointed to A, the copy-out targets a null `VkBuffer` (crash) or the fit reads stale memory (garbage). | Bind CR5's "re-point the bridges to A" as a HARD precondition of the CPU-gme path: the on-A copy-out + the host re-read use the SAME `FD`/A-imported buffers. | The `single_gpu` run produces a valid affine fit (`dis:NN%` sane, not garbage); no null-buffer copy. | `open` |
| **CR8** | **VRAM budget (non-crash, load-bearing UNKNOWN).** Moving Bframe/Cinterp/Bflow + the ofp pyramid (+ mv_prev/mvsm) onto A adds ~3-4× WW·WH·4 + the pyramid. Negligible on a 4090; UNMEASURED on a 4 GB-class single dGPU (the DCAD §8 load-bearing slice). | Not crash-class on the test rig; it is the P1 MEASUREMENT target. Report VRAM + the FG slice; flag the small-GPU risk as a measured number, not an assumption. | The P1 run reports VRAM use + the slice; sweep 1080p/1440p/4K. | `open` |

## §2.1 — Discharge (2026-06-20, first-hand + validation-layer)

Implemented (foundational by the supervisor; the ~40-site FD-routing collapse by a clean-context Opus agent against
this register; the supervisor then found + fixed the CR9 pool race). Verified first-hand on the 4090 rig:

- **Default path byte-identical-OFF** (no `--force-single-gpu`): A(4090)+B(1080Ti)+G(iGPU), 240 fps / frz 0.0 / lat
  21 ms / warp 0.7 ms — unchanged from baseline. Every collapse edit is `single_gpu`/`FD`-gated → the operator's tuned
  multi-GPU pipeline does not regress.
- **`--force-single-gpu` run under the Vulkan validation layer** (`VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation`):
  **ZERO validation errors**, runs stably (no segfault, alive through the full run), 238-244 fps / frz 0.0 / gpu
  A:94-99% **B:0% G:0%** (the assist GPUs truly idle = a genuine single-GPU topology).
- **The P1 slice (the load-bearing unknown, measured):** freshage ~15-18 ms (pickup ~5 [conv ~3] + build ~8 [compute
  ~6] + detect ~3) + warp ~2 ms → game→screen ~20-22 ms on a single 4090.

Per-risk: **CR1** `mitigated` (A.q2!=A.q on the 4090; the hard-refuse guard present for single-graphics-queue GPUs).
**CR2** `mitigated` (the full FD-route census incl. the two missed waits; validation 0 null-device derefs + the run
works). **CR3** `mitigated` (the a_q2_mtx serializes the shared A.q2 SUBMITS). **CR5** `mitigated` (`gme(dis fit:)`
proves gme is ON, not silently stripped). **CR6** `mitigated` (`want_pfg=false`). **CR7** `mitigated` (the CPU gme fit
produces a valid `dis` from the A-aliased bridges). **CR4** `accepted` (A.qT unused). **CR8** `measured` on this rig
(the slice above; the small-GPU VRAM budget remains a future small-target measurement — `accepted`/deferred).

- **CR9 (NEW — the supervisor's validation-layer catch, the one the agent missed)** → `mitigated`. The agent's
  `a_q2_mtx` serialized the A.q2 **queue submits** but NOT the command-buffer **recording**: under single_gpu the 3
  threads P(present)/C(convert)/F(flow) all recorded from `A.pool`, and **command pools are externally-synchronized**
  → `UNASSIGNED-Threading-MultipleThreads-Write` (×11) + a mid-run **segfault** (the concurrent-pool UB). **Fix AS
  CODE:** two dedicated A-side command pools created under single_gpu — `a_cpool_sg` for C, `a_fpool_sg` for F (both
  `A.qfam` = A.q2's family, valid since CR1 guarantees q2mode=1); P keeps `A.pool`. The a_q2_mtx stays for the shared
  A.q2 submits (a queue and a pool are distinct externally-synchronized objects). **Verification:** the re-run under
  the validation layer = **0 threading errors + no segfault** (above). Lesson: a queue mutex does NOT cover the
  command-pool recording — they are separate sync domains.

## §3 — P1 measurement (the load-bearing number)

The instrumentation EXISTS (`--latency-trace`, STAGE-112, `main.cpp:8067-8078`): on single-GPU, `freshage = pickup(⊇conv)
+ build(⊇preflow[spin]+compute) + detect` IS the FG-slice proxy; per-stage atomics `gme_fit_us`, `wap_warp_ema`,
`t_flow_ema`, `present_cost_us` already exist; the `--csv` row carries per-present `warp_ms`, `ms_in_present_api`,
`iter_ms`, `added_lat_ms` (end-to-end game→screen), `gme_dis_pct`. **ONE gap (the single measurement add):** the A-convert
branch (`!use_igpu_convert`, @4627-4644) does NOT store `c_conv_us` (only the iGPU branch @4678 does) → `[lat-trace] conv`
reads 0. **Fix:** wrap the 4644 submit with a `now_ms()` delta + mirror the @4678 EMA store (cheap, measurement-only).
Then report a derived `[fg-slice]` = conv + compute(flow+gme) + warp + present (capture+convert+flow+warp+double-present,
all on A). Sweep 1080p/1440p/4K (resolution is fixed at startup from the captured window → 3 runs, zero code change).

## §4 — Open decisions (operator policy, do NOT block the test-rig implementation)

1. **`A.q2==A.q` fallback policy** (the gate between "runs on the common single-GPU user" and "refuses on single-graphics-queue GPUs"): hard-refuse (safe first cut, shipped) vs CPU-mutex-serialize flow+present on A.q (degraded but functional). The minimal cut SHIPS hard-refuse; the serialize mode is the operator's call. The 4090 rig has `A.q2!=A.q`, so the test runs either way.
2. **Convert lane** (CR3): mutex-serialize on A.q2 (first cut) vs merge-into-cmdF (efficiency) vs A.qT (needs --upload-xfer). Pick by the P1 slice numbers.
3. **Efficiency follow-up:** elide the VRAM→host→VRAM bridge round-trip (blit `ofp.motion_image()`→`wapMVA` in-VRAM; both RG16F at mvw×mvh) — a separate efficiency-mandate change after the crash-safe minimal cut is measured.

## §5 — Implementation order (each step: build green → `--force-single-gpu` on the rig → validation-layer → next)

P0 the flag + measurement (low-risk, lands first) → relax the hard-exit + `single_gpu`/`FD` + `want_pfg=false` → the
COMPLETE CR2 `FD`-route census (creation + the two consume waits) → CR3 convert-lane mutex → CR1 `A.q2==A.q` guard → CR5/CR7
re-point the bridges to A → run under `--force-single-gpu` (validation layer ON) → discharge every risk → P1 measure +
sweep → commit by pathspec (Tier-2: no risk `open`).
