# RENDER_ASSISTANT_PLAN.md — a multi-GPU render assistant (anti-obsolescence)

**Date.** 2026-06-06.
**Status.** PROPOSED — design + roadmap. The code is what's truthful when they disagree.
**Parent.** [`CANON.md`](../../CANON.md). Builds on the `gpu` pillar + the `holons↔gpu`
router + the `render` pillar.
**Companions (the evidence this plan stands on).**
[`GPU_MULTI_GPU_PRIOR_ART.md`](GPU_MULTI_GPU_PRIOR_ART.md) (§7.9 the measured render-split
wall) · [`GPU_INTERCEPTION_GRIETA.md`](GPU_INTERCEPTION_GRIETA.md) (the interception verdict +
the legal insertion point) · [`GPU_PILAR_MASTER_PLAN.md`](GPU_PILAR_MASTER_PLAN.md) (the
router, break-even, auto-characterization, `run_routed`).

---

## §0 — The goal (the author's #3, landed)

Make the **idle/older GPUs earn their keep** — a 1080 Ti + an AMD iGPU enhancing the output
of *any* application, to fight obsolescence. Constraints the author set, kept verbatim:
- **App-agnostic** — works on any app, *external* to it (no injection into the app process).
- **Non-damaging** — never touches the app's integrity (read-only on its output).
- **Compatible per-limitation** — safe everywhere it can be; degrades gracefully where it can't.
- **An optimization, NOT a cheat** — anti-obsolescence, not an advantage hack.

## §1 — Why THIS architecture (the walls, measured + researched, not assumed)

Two investigations converged (both first-hand on the rig + cited):
- **render-SPLIT is walled** (`GPU_MULTI_GPU_PRIOR_ART.md §7.9`): the cross-device G-buffer
  round-trip over the x4 link = **105 % of the 60 fps frame budget**, before any compute. No
  P2P on consumer cards; the work is device-bound. Going aggressive past anti-cheat does NOT
  change this — the wall is *physics*, not anti-cheat.
- **There is no redistribution grieta** (`GPU_INTERCEPTION_GRIETA.md`): interception is solved
  + legal (Vulkan implicit layer), but the work past the seam is device-non-portable; the
  **only feasible capture point is the FINAL FRAME** (the Lossless-Scaling model).
- **Control plane vs data plane:** Phyriad's fast comm (stigmergy deposit ~3000 M op/s gate,
  ring ~42 M op/s) makes *mirroring the call flow* free — but the wall is the **data plane**
  (the frame's bytes crossing PCIe), which throughput-of-control does not move.

So the only architecture that satisfies §0 is:

```
external capture (final frame)  →  cross-device transfer  →  enhance on idle GPU(s)  →  present
```

render-ASSIST, not render-split. The MEASURED one-way cost for the final frame (STAGE-2, 2026-06-06,
verified first-hand) is **~2.90 ms @1080p** (4090→1080 Ti, real DEVICE_LOCAL image copy; §7.9's 1.71 ms
buffer-streaming *estimate* was ~1.7× optimistic) — still fits the 16.67 ms budget (**17 %**), pipelined
(+1 frame). And the **iGPU is the faster transfer-bound assistant** (4090→iGPU = ~0.76 ms — no PCIe
leg-2, since its VRAM is system RAM), a measured-capability fact the §2 router exploits.

## §2 — The Phyriad differentiation (why this isn't just "another Lossless Scaling")

Lossless Scaling uses **one** assistant GPU. Phyriad's angle: the **holon router distributes
the enhancement across ALL idle resources** (1080 Ti + iGPU together) by *measured capability*
— `run_routed` + `break_even_decide` + the auto-characterized catalog already do exactly this
for divisible compute. The enhancement (upscale tiles / frame-gen) is divisible work; routing
it across the idle catalog by capability is the genuine contribution. (Honest scope: this is
solid systems engineering on a known shape, not a novel rendering technique.)

**Measured-grounded — and the coordination is FREE.** The whole plan stands on first-hand
numbers, not assumptions: the per-frame budget is the cross-device handoff (**~2.90 ms @1080p
measured**, STAGE-2); the device capabilities are the measured bandwidth + compute (the catalog); and the
COORDINATION costs effectively nothing — the router's per-frame decision is ~8 ns
(`FillBasedRouter::decide`, ~121 M op/s) and a stigmergy claim ~1.5 ns (~655 M op/s, measured
live 2026-06-06; the old "≥3000 M op/s" doc gate was a pre-Phyriad aspirational number, now
corrected). So even hundreds of routing/claim ops per frame cost a few µs against the 16.67 ms
budget. That headroom buys a real upgrade: the enhancement can be **re-routed across the idle
GPUs EVERY frame** (dynamic adaptation to load), not just statically split once. The binding
constraint stays the **data plane** (the frame's bytes over PCIe) — never the control plane.

## §3 — What exists (the head start)

- **`render/vulkan`:** `Swapchain`, `VulkanContext` (single-device), `OffscreenScene` (offscreen
  target; clear+checker today, geometry is v1.1), `ExternalImage`/`ExternalTexture` (real
  VK_KHR_external_memory — **cross-API single-GPU**), `FSR2Pass`/`FSR3Pass` (real, SDK-gated),
  `OpticalFlowPipeline`/`BlendPipeline` (real frame-gen/blend), `GpuTelemetry` (per-frame ms).
- **`gpu` + `holons_gpu`:** the catalog (`GpuDescriptor`, `enumerate_gpus`,
  `measure_transfer_bw`), `break_even_decide`, the router (`select_participants`), the one-call
  `run_routed`, the measured cross-device handoff (§7.9 / `bench_dispatch` STAGE-1).

## §4 — What's missing (the build)

| stage | capability | risk | notes |
|---|---|---|---|
| ~~**STAGE-2**~~ ✅ **DONE** | cross-**DEVICE** image transfer (VkImage GPU A → host → GPU B; no P2P) | — | **measured 2026-06-06**: 2.90 ms @1080p 4090→1080Ti, 0.76 ms 4090→iGPU; data CORRECT cross-vendor; `external_memory_host` shared host buffer, no memcpy. `bench/stage2_image_xfer/`. |
| ~~**STAGE-3**~~ ✅ **DONE** | external capture → VkImage | — | **measured 2026-06-06**: DXGI Desktop Duplication chosen (WGC needs MSVC; both AC-safe). D3D11→VK interop WORKS (shared handle, GPU-resident, checksum-verified) + CPU-bounce fallback; capture→VkImage ~1.3 ms (never the bottleneck); rate gated by `AcquireNextFrame` (~60 Hz on desktop change). DRM/HDCP/static desktop → black (honest limit; path proven regardless). HDR desktop = FP16 16.6 MB/frame (2× RGBA8 → 2× transfer). `bench/stage3_capture/`. |
| ~~**STAGE-4**~~ ✅ **DONE** | enhancement compute cost on each GPU | — | **measured 2026-06-06**: frame-gen (optical flow) @1080p R8 = 1.94 ms (4090) / 6.70 ms (1080 Ti) / **256 ms (iGPU — CANNOT)**; upscale 1080p→4K = ≪1 ms (discretes) / 1.08 ms bilinear (iGPU). Pipeline viable WITH routing: frame-gen on 1080 Ti @1080p R8 + Lanczos upscale = 7.55 ms < 13.77 ms budget. `bench/stage4_enhance/`. |
| ~~**STAGE-5**~~ ✅ **DONE (loop)** | real-time loop: capture → transfer → enhance, frame-paced | — | **measured 2026-06-06**: the live loop runs + **sustains a 60 fps source** — assistant cost **10.8 ms < 16.67 ms** (≈92 fps headroom; capture 0.6 + 4090 1.0 + 1080Ti frame-gen 8.8 + iGPU upscale 2.25 overlapped), enhanced outputs verified non-trivial, ~2.6-frame latency. **Finding:** external timeline-semaphore handoff 4090→1080Ti FAILS to import (`VK_ERROR_INITIALIZATION_FAILED`) — the **no-P2P wall at the SYNC level**; a targeted host-fence is the correct floor (not STAGE-3's gross device-idle). `bench/stage5_assistant/`. |
| ~~**STAGE-5b**~~ ✅ **DONE** | windowed present-from-assistant + display routing | — | **measured 2026-06-06**: the enhanced (1080 Ti) frame copies back to the 4090 → FIFO swapchain → a window shows it, frame-paced; loop **sustains >60 fps WITH present** (~13.7 ms iter < 16.67; copy-back 2.51 + blit 0.32 + present 0.53). **Finding:** WDDM reports all 3 GPUs "support present" but a non-display GPU presenting = a hidden cross-adapter copy (same cost); a truly free one-way present needs the monitor physically on the assistant GPU (deployment lever, not software). `bench/stage5b_present/`. |
| ~~**STAGE-6**~~ ✅ **DONE** | capability-based pass placement (the §2 router for discrete passes) | — | **measured 2026-06-06**: `route_enhancement` (sibling of `select_participants`) places each pass by the catalog — **21/21 CI test (no GPU)**: derives STAGE-5's assignment (frame-gen→1080 Ti, bilinear→iGPU), REJECTS naive frame-gen→iGPU (134 ms infeasible), generalises to any GPU mix, supervisor fallback when uncharacterized. Finding: a single `compute_gflops` mispredicts memory-bound frame-gen (catalog estimate; §8.5.2 must probe a *representative* workload) — decisions still correct. `bench/stage6_route/` (wiring into the loop = a small swap, offered). |

## §5 — Constraints + honesty (kept, per §0)

- **Anti-cheat:** external capture (DXGI DD / WGC) is a *separate process* — it does NOT inject
  into the app, so kernel AC's loaded-module inspection doesn't see it (why OBS / LS work with
  AC games). This is the AC-safe path. The *in-process implicit layer* (lower latency) is for
  **non-AC apps only** (e.g. Minecraft vanilla); the aggressive in-process path is rejected
  (AC-flagged AND doesn't help — §1).
- **Data plane is the budget (MEASURED, STAGE-2).** 1080p final = 8 MB → **2.90 ms** one-way
  4090→1080 Ti (17 %), **0.76 ms** 4090→iGPU; 4K = 33 MB → 11.6 ms / 3.2 ms; all pipelined.
  The cross-vendor `external_memory_host` host bridge **works on all three devices, no memcpy
  fallback** (verified) — and the iGPU is the cheaper destination (no PCIe write-back leg).
- **Latency.** +1 frame (the assist runs on frame N while the app renders N+1). The
  Lossless-Scaling tradeoff; acceptable for the anti-obsolescence goal, not for esports.
- **Scope.** This makes the OLD GPUs useful for ANY app's *output enhancement* — it does NOT
  redistribute the app's own GPU work (walled). That distinction is the whole point.

## §5.1 — End-to-end budget (does the whole pipeline fit 60 fps?)

Pipelined, each stage runs once per frame and must fit ~16.67 ms (the frame period); the user-visible
LATENCY is the sum (+N frames — the Lossless-Scaling tradeoff). All four rows are now **MEASURED**
(STAGE-2/3/4, first-hand 2026-06-06):

| stage | per-frame cost | source |
|---|---|---|
| capture (DXGI DD → VkImage) | **~1.3 ms** (CPU bounce; interop cheaper); rate gated by `AcquireNextFrame` ~60 Hz | **MEASURED (STAGE-3)** |
| transfer to assistant @1080p | **2.90 ms** (1080 Ti) / **0.76 ms** (iGPU) — ×2 for HDR FP16 | **MEASURED (STAGE-2)** |
| enhancement @1080p | frame-gen R8 **6.70 ms** (1080 Ti) / **256 ms (iGPU — busts)**; upscale Lanczos **0.85 ms** (1080 Ti) / bilinear **1.08 ms** (iGPU) | **MEASURED (STAGE-4)** |
| present (assistant → its display) | ~0 ms if monitor on the assistant; else a transfer back | the display-routing rule |

**Verdict — VIABLE, but only with correct routing (measured, not assumed):** the **1080 Ti** path fits —
transfer 2.90 + frame-gen R8 6.70 + Lanczos upscale 0.85 = **~10.5 ms < 16.67 ms** (capture overlaps,
pipelined). The naive "offload everything to the idle iGPU" **FAILS**: the iGPU *cannot* do optical-flow
frame-gen (256 ms @1080p R8, ~16× its budget) — it earns its keep only as a *light* assistant (bilinear
upscale 1.08 ms). That is exactly the asymmetry the §2 router must respect: compute-bound frame-gen → the
1080 Ti (capped @1080p R≤8; 4K / R16 bust); transfer-bound / cheap upscale → the iGPU. User-visible
latency +2–3 frames (~33–50 ms): fine for anti-obsolescence, not esports.

## §6 — Progress

**STAGE-2 — DONE (2026-06-06, verified first-hand).** The cross-device image transfer primitive
works: a DEVICE_LOCAL `VkImage` host-bounced GPU A → GPU B over two `VkDevice`, data verified
CORRECT (FNV-1a round-trip) on NVIDIA→NVIDIA *and* cross-vendor NVIDIA→AMD, per-leg on-GPU
timestamps. `VK_EXT_external_memory_host` shares ONE host buffer across all three devices (no
memcpy). Measured 4090→1080 Ti 2.90 ms @1080p / 11.6 ms @4K; 4090→iGPU 0.76 ms / 3.2 ms. §7.9's
verdict holds, its number revised up (real image copy ~1.7× the buffer-streaming estimate). Lives
standalone in `framework/render/vulkan/bench/stage2_image_xfer/` (raw two-device Vulkan, like
`bench/gpu_walls/` — the render pillar's single-device `VulkanContext` doesn't fit a two-device bench).

**STAGE-3 + STAGE-4 — DONE (2026-06-06, verified first-hand, measured in parallel).** STAGE-3: DXGI
Desktop Duplication captures any app's final frame *externally* (AC-safe, no injection); D3D11→VK interop
verified GPU-resident + correct, ~1.3 ms capture→VkImage (never the bottleneck); DRM/static desktop →
black (the honest limit; the path is proven by the synthetic-pattern checksum regardless). STAGE-4: the
per-device enhancement costs (§5.1) — confirming **by measurement** that the iGPU *cannot* frame-gen and
the 1080 Ti is the capped workhorse; the pipeline is viable **only with capability-routing**. Benches:
`framework/render/vulkan/bench/stage3_capture/` + `stage4_enhance/`.

**STAGE-5 — DONE (2026-06-06, verified first-hand).** The real-time loop runs and **sustains a 60 fps
source** — capture(4090) → host-bounce transfer → frame-gen(1080 Ti) + upscale(iGPU), assistant cost
**10.8 ms** (≈92 fps headroom), enhanced outputs verified non-trivial, ~2.6-frame latency. **A real
architectural finding:** the "clean GPU-to-GPU handoff" the plan hoped for is *unavailable* between the
two physical consumer GPUs — the external timeline semaphore exports from the 4090 but FAILS to import
on the 1080 Ti (`VK_ERROR_INITIALIZATION_FAILED`), the **no-P2P wall (§1) at the SYNC level**, not just
the data level. A targeted host-fence is the correct floor; the throughput lever is **pipelining**
(overlap) — not a clean semaphore. `bench/stage5_assistant/`.

**STAGE-5b + STAGE-6 — DONE (2026-06-06, verified first-hand, two parallel Opus).** STAGE-5b: the
enhanced frame is **presented to a window** (copy-back to the 4090 → FIFO swapchain), the loop **sustains
>60 fps WITH present** (~13.7 ms iter); display routing is affordable (~2.8 ms copy-back) but free only if
the monitor is on the assistant GPU (deployment, not software). STAGE-6: the §2 router — `route_enhancement`
places each pass by the **measured catalog** (21/21 CI test, no GPU): derives STAGE-5's assignment, REJECTS
the naive frame-gen→iGPU, generalises to any GPU mix. So the assignment is provably correct + portable.

**The assistant is COMPLETE and RUNNING** — capture → transfer → capability-routed enhance → present, in
real time, >60 fps, *showing* the enhanced output. The anti-obsolescence thesis (§0) is **empirically
proven on the rig**: the idle 1080 Ti + iGPU do useful real-time enhancement, each in its measured niche.
Remaining is optimization, not feasibility: **pipelining** (overlap capture N+1 with enhance N — the lever
STAGE-5 surfaced), wiring STAGE-6's router into the live loop (a small swap, offered), and §8.5.2
characterization probing a *representative* workload (STAGE-6 found a single `compute_gflops` mispredicts
memory-bound frame-gen).

**STAGE-7 — the FIRE TEST PASSES on real content (2026-06-06, verified first-hand).** `bench/stage7_livefire/`
runs the loop on the REAL desktop (no synthetic substitution) + a `hdr_convert.comp` (proper scRGB→SDR
tone-map, replacing STAGE-5's naive blit). **Verified in SDR (8 bpc, DXGI=87):** Part 1 RAW capture
15/15 non-black → "REAL CAPTURE WORKS"; Part 2 live-fire **71 fps, REAL-content 150/150, presented frame
non-black** — the assistant captures + enhances + presents *real* screen content >60 fps. **The fire test
is GREEN in SDR.** Two real-world capture findings: (1) **bit depth matters** — a 10 bpc/HDR (FP16, DXGI=10)
output delivered BLACK raw pixels via DXGI DD while an 8 bpc (RGBA8) output gave real content; forcing 8 bpc
fixed it. (2) **multi-monitor matters** — DD captures ONE output at a time; a 2-monitor mixed-bit-depth rig
needs output selection (and offers capture-on-A / present-on-B to avoid the self-capture feedback loop).
OPEN: whether DD-on-HDR (FP16) delivers real raw pixels or is walled (MPO) — STAGE-7b adds output selection
+ dynamic format routing (8/10 bpc SDR + FP16) + the raw-pixel HDR test. Capture matrix (honest): SDR/windowed
✓, HDR/windowed ✓ when awake+tone-mapped (pending the raw-pixel confirmation), exclusive-fullscreen often ✗
(bypasses the compositor), AC game ✓ (external capture is AC-safe — OBS-class), DRM/HDCP ✗ (black by design).

**DEFINITIVE REAL-GAME fire test — PASSED (2026-06-06, operator-run on the rig + supervisor-read).** Target:
**Minecraft 26.1.2 with shaders** (Complementary Reimagined r5.7.1, Iris + Sodium) — played normally, windowed,
1080 Ti idle, 4090 ~20 % (high mob density). Result: Part 1 RAW 24/24 non-black + DD-update 24/24 → REAL
CAPTURE WORKS; Part 2 **62.6 fps, REAL-content 150/150**, presented frame non-black → the idle 1080 Ti + iGPU
enhanced real game output live, no injection. **Honest real-game caveats** (vs the synthetic ~71 fps / 14 ms):
margin is TIGHTER — iteration mean **15.97 ms** (vs 14), **62.6 fps** (vs 71), worst **20.40 ms** (occasional
over-budget hitch). Cause: the assistant's primary-GPU work (capture+convert ~1.4 + copy-back 2.45 + present
0.33 ≈ 4 ms) **shares the 4090 with the game** — when the primary GPU is busy, the budget margin shrinks. This
is exactly what **pipelining** (overlap, the STAGE-5 lever) buys back — so on a real game the optimization
stops being optional. Also: a brief **black at startup** = normal DXGI DD warm-up (first AcquireNextFrame(s)
blank until the first desktop update) — a wait-for-first-valid-frame polish for STAGE-7b.

## §7 — Ceilings & escape routes (the high-fps map)

The strategy (author's framing): **find every ceiling, measure where each one binds, and look for the route
that escapes toward the goal** (boost toward the monitor refresh). Measured baseline: serial ~76 fps;
inter-device pipelined **~89 fps** (STAGE-8), bound by the 1080 Ti's *serial* leg2 + frame-gen + copy-back
= 11.2 ms on one queue.

| escape route | mechanism | ceiling | status |
|---|---|---|---|
| inter-device pipelining | hide the 4090 work behind the 1080 Ti stage | **~89 fps** | ✅ DONE (STAGE-8) |
| 1080 Ti DMA queue | overlap copy-back(N) with frame-gen(N+1) on the 1080 Ti's *separate transfer queue* → removes the copy-back (~2.5 ms) from the critical path | **~114 fps** (leg2+FG ≈ 8.75 ms) | candidate — the clearest measured lever |
| cheaper frame-gen | lower block-match R (R4 ≈ 2.24 ms on the 1080 Ti vs 6.7 ms R8), or a lighter FG algorithm | R4 → **~137 fps** (leg2+FG+copyback ≈ 7.3 ms); sub-ms FG → higher | candidate; quality tradeoff (measure it) |
| **iGPU payload-reduction** (author's idea) | the iGPU (native sysRAM access) packs/compresses the source frame *in sysRAM* before the 1080 Ti pulls it → **fewer bytes over the slow x4** → faster leg2/copy-back | depends on reduction ratio vs the 1080 Ti's decompress cost — net-win unproven | candidate (speculative, **measure**). NOTE: the iGPU CANNOT bridge the 1080 Ti's PCIe (no P2P; the x4 is the 1080 Ti's only door) — it can only shrink the *payload* that crosses it. |
| **4090-spare frame-gen** | FG on the **4090's own spare capacity** (1.94 ms, measured) — **NO cross-device transfer at all** | bounded by the 4090's spare, not by PCIe — potentially **200+ fps** when the game leaves headroom (the MC <50 % case) | **THE BIG ESCAPE** — bypasses the transfer wall entirely; needs the baseline measurement |
| load-aware router | per-frame: 4090-spare when free, offload to idle GPUs when the 4090 is saturated | orchestration, not a ceiling | the multi-GPU differentiator (LSFG's dual-GPU is static) |

**Honest strategic read.** The cross-device **offload** path is *transfer-walled*: ~89 now → ~114 (DMA queue)
→ ~137 (cheaper FG), but every step is still bound by the 1080 Ti's x4 link + the no-P2P host-bounce. The
route that **escapes** the transfer wall is **4090-spare FG** (no transfer) — the most promising for the
common *unsaturated* case. Offload + iGPU-payload-reduction matter in the *saturated* regime (demanding
games that leave the 4090 no spare). The **load-aware router** picks per-frame which regime applies — that's
where the 3-GPU advantage is genuinely defensible. Next experiment: the **4090-spare FG baseline** (does the
4090 alone hit high fps with the game running? at what cost to the game's base fps?) = the bar every other
route must beat.

**MEASURED (2026-06-06, all verified first-hand).**
- **4090-spare FG (STAGE-9) — THE BAR, confirmed.** Light/vsync-capped game → **240 fps presented FREE**
  (base 60 preserved; spare 16.6 ms / 1.84 ms ≈ 8 intermediates → ceiling ~540 fps). Cross-device offload is
  IRRELEVANT in the common case. Offload only earns its place when the 4090 **saturates** (heavy game,
  render 14.8 ms → spare 1.9 ms → base DROPS to 25, presented 100); uncapped/GPU-bound, FG steals base fps
  hard. **Verdict: the multi-GPU render assistant is a *saturation-relief tool*, not a general win** — the
  honest scope of the whole arc.
- **1080 Ti DMA queue (STAGE-10) — ~89 → ~98 fps** (+14 %), NOT the 114 ideal. Copy-back overlapped off the
  critical path (transfer queue 25 % occupancy), but ~1.5 ms of cross-engine contention (frame N's d2h vs
  N+1's h2d on the x4) leaks back. The transfer wall still governs.
- **iGPU payload-reduction (STAGE-12) — WINS** (overturns the pessimistic prior). The x4 link is
  *bandwidth*-bound (−25 % bytes → −25 % time, linear), so iGPU-side packing pays ~1:1 while on-device
  decompress is near-free (~0.05 ms on the 1080 Ti's 480 GB/s VRAM). **lossless-25% cuts 0.60 ms (23 %)
  BIT-EXACT**; rgb565-50% cuts 49 % but is lossy (damages the FG source).
- **R-sweep (STAGE-11) — found + fixed a PILLAR BUG first.** The committed `optical_flow_warp.comp` had a
  sign-inverted half-MV offset → a ghost *worse than naive blend* (verified by ground-truth PSNR; fixed in
  `9b509d7`). With the fix the sweet spot = the smallest R covering the displacement D (D≤4 → R4 ~45 dB at
  3.4× less cost than R8; D>8 needs hierarchical search). Cost axis unchanged — the bug was quality-only.

**Net:** the offload path tops out ~98–137 fps (transfer-walled); **4090-spare FG is the real high-fps
route** (240 free, common case). The 3-GPU edge is the **load-aware router** (4090-spare when free; offload
+ iGPU-payload-reduction when the 4090 saturates) — the defensible differentiator vs LSFG's static modes.

**Load-aware router (STAGE-13) — DONE + measured.** Unites the three levers behind one per-frame decision:
STAGE-6 `route_enhancement` is the static backbone (IF offloading, where → 1080 Ti; iGPU rejected), an
adaptive-feedback loop + hysteresis is the dynamic overlay (CAN the 4090 self-serve, or must it offload?).
Verified live (4090 + 1080 Ti + iGPU): **LIGHT → 4090-SPARE** (base 157 fps, 627 presented); **HEAVY →
OFFLOAD-1080** (counterfactual proof: staying SPARE crushes base 49→44; offload keeps the 4090 for the game
+ adds 94 interp/s from the idle 1080 Ti, with STAGE-12 RED25 trimming the bind 11.32→10.69 ms). Switching
is stable — K=3-consecutive confirm + dead-band → **0 spurious switches** under borderline-flap stress.
Decision cost negligible (~8 ns/decision, 120 M/s — the router's own op; the plan's earlier "~655 M op/s"
was the gpu-pillar control-plane probe, a different op). This is the saturation-relief assistant integrated.

**Quality / failure-mode vs Lossless Scaling (observed first-hand on Minecraft, 2026-06-06).** A genuine,
honest differentiator: our block-match + warp-and-**BLEND** degrades to **blur** (double-exposure) when the
block-match fails on fast motion (displacement > R); LSFG's flow-warp degrades to **geometric hallucination**
(invented curves / morphing). Blur reads as natural motion blur (the eye tolerates it); hallucinated geometry
is jarring (uncanny). So for visual *comfort* on fast motion the blend-fallback is arguably better — an honest
TRADEOFF (we are blurrier / less smooth overall, but we never invent geometry). The graceful failure is a
property of the simple algorithm, not sophistication.

**Hierarchical block-match (STAGE-14) — WINS quality AND cost; pillar-upgrade DECIDED.** A 4-level pyramid
(coarse-to-fine) search vs the flat ±R block-match, ground-truth PSNR on large displacements: D=16 -> HIER
**41.1 dB @ 1.23 ms** vs flat-R16 37.0 @ 5.2 ms (+4 dB, 4x cheaper); D=32 -> 39.4 vs 33.6 @ 17.5 ms (+6 dB,
14x); D=64 -> 38.3 vs 31.4 @ 68.8 ms (+7 dB, **56x**). HIER cost is FLAT ~1.2 ms (O(R^2*levels)) while flat
explodes (O(D^2)). It directly fixes the Minecraft **fast-motion blur** — flat R<=32 at D=64 collapses to
~19 dB ~= the blend floor, and a fast camera pan IS the large-D case. It even BEATS full-search flat R=64
(the coarse-to-fine regularization prior rejects spurious low-SAD matches in repetitive game textures —
expected, not a fluke). The committed pipeline caps R<=32 (`OpticalFlowPipeline.cpp:277`, can't reach D=64);
the pyramid sidesteps it. **Decision: LIFT into the pillar** (`optical_flow_block_match` -> pyramid) — a
substantial multi-pass change (new pyr_down + hier_match shaders + coarse-to-fine orchestration in
`OpticalFlowPipeline`), done via the pillar-modification protocol (the pyramid shaders are proven in
`bench/stage14_hierarchical/`; the API stays, `optical_flow_test` must still pass). Caveat for the pillar
version: coarse averaging could miss objects smaller than a coarse tile (not exercised by whole-frame-motion
presets) — verify a small-fast-object case when promoting. (Promoted in STAGE-15, committed `4370d70`.)

**Static-region detection / HUD protection (STAGE-16) — both mechanisms pin the static HUD.** The frame-gen
warps the WHOLE frame → a static HUD (hotbar/health) gets dragged by the scene's motion → it trembles (the
operator hit this on Minecraft; the classic LSFG artifact). Measured vs ground-truth (768×432, real MC pan
D=12, static HUD, ±2 dither): BASELINE opaque-HUD **9.5 dB / maxerr 214** (destroyed) → **per-TILE 43.4 dB /
maxerr 4** (force MV=0 when a tile's SAD@MV0 < ~2.0) or **per-PIXEL 42.3 dB / maxerr 4** (warp passthrough
when |A[x]−B[x]| < ~0.08). maxerr 4 = the dither floor → pinned; >50× shimmer cut. Cost ~FREE (per-tile reuses
the existing SAD: 1.65→1.59 ms; per-pixel = +2 samples: 0.006→0.007 ms). **Per-tile is the better default**
(free, best full-frame PSNR 35.4 — acts on coherent 8×8 MV blocks); per-pixel complements it for sub-tile HUD
edges. Threshold = just above the noise floor (below it, static pixels' own dither warps them anyway). **Honest
caveat (measured):** a TRANSLUCENT HUD over moving background is only ~halved (opaque maxerr 4 vs translucent
~64), not pinned — the composite changes as the scene slides under it, so |A−B|≠0. Opaque overlays are the
protected case. **Decision: LIFT per-tile static-lock to the pillar** (`optical_flow_hier_match` ± per-pixel in
the warp) — a real visible fix, free; pillar-modification protocol.

**fast-FG (STAGE-17) — and an honest reframe-correction.** Measured the pyramid FG cost/quality curve (1080 Ti
/ 4090, 1080p, params-only, pillar shaders unmodified). **The reconciliation matters: the pillar pyramid FG is
already 2.64 ms on the 1080 Ti (<3 ms), NOT the "~5 ms" §7/earlier framing.** That ~5-7.65 ms was the *flat-R8*
(pre-pyramid, STAGE-4) and the ~11 ms is *end-to-end with the host-bounce legs* (leg2 + copy-back) — the FG
COMPUTE was never the wall; the TRANSFER is. So fast-FG was attacking a non-bottleneck. Still, trimming the
coarse/fine radii at full-res is a real cheap lever: **Rc6/Rf2 = 1.08 ms (0.41×, near-lossless ≤~0.5-3 dB)**;
Rc5/Rf1 = 0.66 ms (0.25×, small loss). HALF-res is DOMINATED (costs more than Rc5/Rf1 full-res AND caps quality
~30 dB — the coarse search is the cost driver, shrinking it beats splitting resolution; the operator's
"biggest-saving/biggest-risk" hypothesis measured: risk materializes, saving doesn't). A lighter warp saves
nothing (warp ≪ block-match). **Decision: change the pillar default to Rc6/Rf2** (~2.2-2.4× cheaper FG compute,
near-lossless for common motion D≤32; Rc=10 is over-provisioned) — amplifies the 4090-spare route. Exposing
Rc/levels in `init()` for per-route router tuning is a deferred API change (not needed until the router tunes
per-route).

**Confidence gate (STAGE-19) — the principled, threshold-FREE static/HUD fix; PROMOTE decided.** STAGE-18's
absolute `static_thresh` is content-specific (the operator had to sweep it to 128 for Minecraft+shaders, and
even then the static HUD vs slow-world conflict has no single global value). STAGE-19 replaces it with a
per-tile RELATIVE confidence gate using two SADs the block-match already computes: `sad_best` (residual at the
winning MV = match confidence) + `sad_zero` (in-place change). **WARP only if `sad_best < residual_ceil` AND
`sad_zero·(1−improvement_frac) > sad_best`** (motion-comp improved the match by > frac); else BLEND. **KEY:
`(sad_zero − sad_best)` CANCELS the local noise floor** → the HUD pins at ANY noise WITHOUT a per-content
threshold (solves the operator's pain; it's the dynamic/per-section "smart knob" intuition, done principled +
self-normalizing). Measured (1080p, ground-truth PSNR, verified first-hand): **hud_trans16 +5.67 dB** (62.6 vs
warp 56.9); trans08 (slow) = warp everywhere (99 dB, zero tradeoff); trans32 −0.25 dB (negligible, Rc32/if50);
**cost 0.81×** (BLEND tiles skip two texture fetches). The D>48 partial-match case: warp_pure scores +1 dB PSNR
(its partial match at MV≈48 beats blend) — **but that is a PSNR artifact: there the warp produces the geometric
CURVES the operator found worse than blur; the gate's blend there is the perceptual WIN. PSNR rewards the
numeric partial-match and is blind to geometric distortion.** The deeper-pyramid (larger Rc) is the orthogonal
lever for D>48, not the gate. **Decision: PROMOTE — replace the static_thresh with the confidence gate** in the
pillar (block-match outputs the SAD pair; the warp reads it + gates). Threshold-free, noise-invariant, cheaper.
Recommended residual_ceil 32 / improvement_frac 0.5. Optional refinement: cross-scale confidence deferral
(uncertain fine tile → defer to the coarse level's confidence) for the D>48 partial-match case.

**Motion-boundary-aware blend (STAGE-21) — MIXED, NOT promoted.** The operator's "contour" idea (force BLEND
at motion boundaries + a dilated N-tile contour, to protect the fine detail / HUD-as-fine-detail that
hallucinates at high `residual_ceil`). Measured first-hand (1080 Ti, 256², ground-truth PSNR vs conf_only at
Rc=128): **hud_fine +44.80 dB** (fine-detail/HUD → near-perfect — the operator's target, a real win). BUT
**blob_edge −1.84 dB** (a moving blob's edge is wrongly blended; there the warp, 27.5, beat blend, 25.7) and
**large_uni is margin-sensitive** (margin=2px → −20 dB over-blend on uniform motion whose MV carries ±2px
noise → false boundaries; margin≥8px avoids it). Cost +54.5% on the tiny warp pass (0.008→0.012 ms @256²).
**Verdict: NOT a clean promote** — the boundary-blend is too BLUNT: it blends ALL detected boundaries, but
blend only helps where the warp HALLUCINATES (hud_fine) and HURTS where the warp is acceptable (blob_edge).
**Refinement (STAGE-22): combine the boundary signal WITH the confidence** — blend a boundary tile only if its
warp residual (`sad_best`) is ALSO high there, so it protects the hallucinating boundaries (fine detail/HUD)
without blending the good ones. Bench `bench/stage21_boundary/` kept as the measured finding.

**Confidence-combined boundary blend (STAGE-22) — hypothesis FALSIFIED (verified first-hand).** Tried: blend
a boundary tile only if its `sad_best` is ALSO high (protect the hallucinating boundaries without blending the
good ones). The data refutes it: at usable thresholds (≥16) the SAD filter fires on NO HUD tile → **+0.00 on
hud_fine**. The HUD boundary tiles have LOW `sad_best` (≤16) — the block-match assigns a **compromise MV** with
low A→B SAD that STILL produces a bad midpoint warp. **KEY INSIGHT: `sad_best` (the block-match A→B residual)
does NOT discriminate the bad-MIDPOINT-warp** — they are different signals (a compromise MV explains both sides
of a boundary partially → low SAD → bad midpoint). Second insight: STAGE-21's +44.8 dB was mostly the
`texture()` BILINEAR MV read (implicit boundary smoothing — the pillar ALREADY uses it: hud_fine 39.6
`texelFetch` → 54.2 `texture`), not the edge gate (only +6.54 on `texelFetch`). So the SAD-based per-tile
signals are **exhausted** for the HUD/fine-detail tail. **The right next signal measures WARP QUALITY directly,
not A→B SAD: STAGE-23 = post-warp source-agreement** `d = |A[x−mv/2] − B[x+mv/2]|` — the two warped sources
disagree where the motion is wrong/occluded (e.g., a static glyph under the compromise MV) → BLEND; they agree
where the warp is correct → WARP. Cheap (the two samples are already taken for the warp) and it measures the
actual artifact, not a proxy. Honest: this is the genuinely-better signal, but the external-capture /
translucent-HUD / noisy-game ceiling remains the bound — STAGE-23 is one principled shot, not a ceiling break.

**Post-warp source-agreement gate (STAGE-23) — the RIGHT signal, verified first-hand; PROMOTE decided.**
`d = |A[x−mv/2] − B[x+mv/2]|` (the two sources the warp blends). Candidate `agree per-tile thresh=0.20`:
**hud_fine +39.11 dB** (the HUD ghost SUPPRESSED — the compromise MV makes the two warped sources disagree at
the static glyph → BLEND), **blob_edge +0.00** (no gate regression), **large_uni +0.00** (d≈0 in uniform →
gate doesn't fire), two_vel ≈same. The gate fires exactly as predicted: mixed HUD/bg → d high → blend;
correct-motion → d≈0 → warp, no false positives. Cost +12 % (per-tile, a workgroup barrier) or +0.7 %
(per-pixel) on the tiny warp. The strict win (vs the BILINEAR reference) is short by ~1 dB on blob_edge — but
that is purely the BENCH's `texelFetch` MV-read penalty (texel 44.4 vs bilinear 45.5), NOT the gate (its
blob_edge delta is 0.00). **The pillar warp ALREADY uses `texture()`/bilinear MV → no such penalty there.**
This is the first refinement to cleanly suppress the HUD ghost without regressing correct motion — SAD-based
signals (STAGE-19/21/22) all failed it. **Decision: PROMOTE (STAGE-24)** — add the agreement condition to the
pillar's `optical_flow_warp` (which already uses `texture()` MV), combined with the confidence gate (WARP iff
confidence passes AND d < agreement_thresh); should meet the strict win without the texelFetch penalty. Fold in
the long-deferred **warp-OUTPUT-readback test** (the coverage gap flagged in STAGE-18/20: assert the static
overlay tile is BLENDED/pinned via the gate while the moving region warps). **Promoted (STAGE-24, committed
`c8d0a52`); verified live on Minecraft — the HUD/fine-detail ghost is barely noticeable even on abrupt motion.
The residual is the inherent worst-case (edge-of-FOV fast motion + translucent HUD + external capture — even
the native game tears there).** The quality arc (§7) is resolved + consolidated in the pillar.

## §8 — Consolidation: the usable render-assistant app

§0–§7 proved every piece in standalone benches + the pillar. To make it USABLE (the author's stated goal),
consolidate into one app: **`apps/render_assistant/`** — a continuously-running binary that captures a real
app's output, enhances it (frame-gen + upscale), and presents it, with flags — NOT a fixed-frame measurement.

**What it integrates (all proven; assemble, do not reinvent):**
| piece | source |
|---|---|
| capture (DXGI DD → VkImage) + dynamic format routing (8/10 bpc SDR + FP16 HDR tone-map) + monitor selection | STAGE-3/7/7b |
| cross-device transfer (host-bounce, `external_memory_host`) | STAGE-2 |
| frame-gen (pyramid + corrected warp + confidence + agreement) | the pillar `OpticalFlowPipeline` |
| upscale (bilinear / Lanczos) | STAGE-4 |
| present (copy-back + swapchain) + capture-A / present-B | STAGE-5b/7b |
| (later) load-aware router (4090-spare ↔ 1080 Ti offload) | STAGE-13 |

**Staged:** **STAGE-25** — render_assistant v1: the core continuous loop (capture→transfer→FG→upscale→present)
+ format-routing + monitor-selection + clean flags = the usable single binary. **STAGE-26** — wire the
load-aware router (STAGE-13) into the loop. **STAGE-27** — polish (pacing, FG-factor N, latency, defaults
tuned for real content).

**Honest scope:** this is SYSTEMS INTEGRATION of proven pieces, not new research — the value is "usable"; the
measured walls + quality from §0–§7 carry over unchanged. **Capture is SDR-only** (DD-on-HDR is MPO-walled,
STAGE-7b); the residual fast-edge-HUD artifact is the inherent worst-case (§7). Lives in `apps/`, links
`phyriad_render_vulkan` (the pillar FG), raw Win32/D3D11/DXGI/Vulkan for capture/transfer/present (the
pillar's single-device surface-bound `VulkanContext` does not fit a two-device capture/present app).

**DD CAPTURE WALL — characterized live (2026-06-07), the reason STAGE-28 (WGC) is core, not optional.** DXGI
Desktop Duplication delivers a new frame ONLY when the **DWM-composed desktop** changes. Content presented on
**hardware-overlay / flip-model planes** — video players (browser HW-accelerated video), and flip games — is
composited by the display controller, BYPASSING the DWM surface DD captures. So `AcquireNextFrame` does not
fire for it → the capture FREEZES on overlay/flip content at rest, refreshing only on a desktop-level change
(mouse move, focus/z-order change, UI paint). Confirmed by the mouse-move tell (`main.cpp` AcquireNextFrame(33)
→ skip on timeout) and the HW-accel lose-lose: Chrome HW-accel ON → flip/overlay → DD degrades; OFF → DWM
composites so DD captures, but the web app loses GPU performance. This **corrected two wrong supervisor
hypotheses** (source-throttle, fullscreen-flip) — the single cause is the DD↔overlay mismatch. **Quality of the
FG output is GOOD** (verified live on the WebGL Aquarium: near-identical to source, residual hallucination only
on **transparent/semi-transparent regions** — the named wall: a transparent layer over moving background has
two motions per pixel, unresolvable by a single MV; the agreement gate suppresses most). **Implication:** DD is
a desktop-duplication API, wrong for the assistant's real target (HW-accel games/apps). **STAGE-28 = a WGC
(Windows Graphics Capture) backend** — captures a window/monitor at the content's real rate INCLUDING
overlay/flip, without disabling HW-accel; needs MSVC (the mingw/WinRT wall, STAGE-3). The present-WINDOW polish
(focus/freeze of the debug view) is explicitly DEPRIORITIZED; the capture MECHANISM is the core path to
LSFG-parity.

## §9 — FIRST HEAD-TO-HEAD vs Lossless Scaling (2026-06-10, operator-run, controlled)

BF6 firing range (stable fps, expected load), 4090 @98 %, same scene/settings. Four runs:

| run | game fps (real) | output delivered | notes |
|---|---|---|---|
| baseline | 89-95 | 89-95 | GPU 98 % |
| LSFG 3.2 X2 single | 72-75 (**−20 %**) | 144-150 (true 2×) | GPU drops to 89-90 % (LSFG displaces game work) |
| LSFG 3.2 X2 dual (1080 Ti) | 75-78 (−16 %) | **110-120 (2× broken)** | input lag "incómodo al punto de preferir no activarlo" |
| **ours** (igpu-convert + G-present + assist FG) | **86-88 (−5 %)** | 93-96 | A=1 ms, lat 20.4-21.2 ms STABLE, slip ~1 ms |

**What the data says, honestly:**
1. **Game-fps retention is our decisive structural win**: −3..7 fps vs LSFG's −15..20. The whole 26c/33 arc
   (iGPU convert+pack, G-present, 1080 Ti FG) shows up exactly where designed. The 4090 stays at 99 % FOR THE GAME.
2. **LSFG's dual-GPU mode BREAKS on this rig** (x4 1080 Ti): 2× not delivered + unusable input lag — it pays
   the same PCIe-x4 physics we measured in STAGE-2; our architecture (pack + sysRAM staging + contention
   routing) is precisely what absorbs that wall. Our dual-GPU mode works where theirs doesn't.
3. **Our decisive loss: output rate.** We present 93-96 ≈ the native 89-95 — 2× of a HALVED source
   (src consumed ~47 of ~90). Today we add ~20 ms lag without adding smoothness vs native. LSFG single
   delivers a real 144-150 stream (at 4× our game tax).
4. **STAGE-32's gate FAILED under saturation: arr 55-62 from a 90 fps source.** The ring removed OUR staging
   serialization, but the ceiling persists → the bottleneck is deeper: the WGC FramePool's buffer recycling
   (and our CopyResource) EXECUTE behind the saturated 4090's queue — capture throughput is gated by the game
   GPU's queue latency, not by our staging. Candidates: more FramePool buffers (32b, cheap), and STAGE-30
   pipelining (cut iter ~21 ms → consume more of what does arrive + sustain fg-factor ≥3).
5. **The cheap immediate lever: `--fg-factor 3`** — present ~120-140 from the same ~47 src (vs LSFG-dual's
   broken 110-120, at a fraction of the game tax). Quality cost: FG pairs span ~2 native frames.
**Positioning evidence**: "lowest game tax + the dual-GPU mode that actually works on x4 rigs + measured
latency" is real and demonstrated; the output-rate gap is the remaining blocker to usability parity.

**FOLLOW-UP RUN (same session): `--fg-factor 3` CLOSES the output gap.**
| run | game fps | output | lat |
|---|---|---|---|
| LSFG 3.2 X2 single | 72-75 (−20 %) | 144-150 | unmeasured |
| **ours fg 3** | **86-88 (−5 %)** | **148-157** | **18.5-19.8 ms measured, slip ~1** |

Operator verdict: "perfectamente jugable en la otra pantalla sin ver el juego original; mínimo input lag
notorio vs la ventana real; sensación bastante cómoda." B sustains 2 intermediates at 5.8-6.0 ms (warp_only
re-dispatch is cheap); lat IMPROVED vs fg 2. **We now beat LSFG-single's output rate at a quarter of its game
tax, on the rig where its dual-GPU mode breaks.** Remaining gaps to parity: FG quality vs LSFG 3.x flow on
hard content (untested head-to-head), single-monitor use-case (theirs), arr ceiling (~57 of 90 — STAGE-32b),
and the 180 Hz present-monitor ceiling (fg 3 × full-source would exceed it; fg 2 × 90 src = the perfect fit
once 32b lands).

**STAGE-32b GATE RESULT (operator-run, BF6 firing range): fix-1 FAILED — and then DEFERRED on perceptual
grounds.** FramePool depth 2→6 did NOT move the ceiling: arr ~53-58 at both fg 2 and fg 3 (identical to
depth-2). Pool starvation was not the binding constraint — the WGC capture pipeline itself delivers ~55-60/s
when the source GPU is saturated (its internal compose/copy work is throttled at a deeper level). Fix-2
(dedicated D3D11 copy device) remains speculative: D3D11 exposes no explicit copy-queue control, so a second
device only changes WDDM scheduling odds. **DEFERRED — the operator's perceptual verdict at fg 3
(150-157 present / lat ~19 ms): "ya no puedo notar una diferencia relevante con mis ojos."** The remaining
chain (arr ~56 ≈ consumption ~50) sits beyond his perception threshold; fix-2's payoff (1-native-frame FG
pairs at fg 2×90) is now invisible-by-measurement. Revival condition: if the STAGE-33b overlay play test
surfaces motion-quality complaints attributable to 2-frame FG pairs, fix-2 + STAGE-30 pipelining revive
together. (His fg-3-feels-better datum has a measured basis, not placebo: lat 19.1 vs 20.7 ms and 150 vs 94
motion samples/s.)

**STAGE-33b OVERLAY PLAY TEST (operator, BF6 ~80 fps, 240 Hz main monitor): playable, G-present wins, and the
real-cadence gap is now THE named blocker.** Window capture works; the overlay is input-invisible; verdicts:
A-present lat 22.5-25 ms / G-present (DWM cross-adapter) lat 21-22 ms + A 1.6 vs 2.4 ms → **G is the overlay
default**. fg 4 reached 155-199 present. BUT the operator: "LSFG single aún gana" — and the numbers say why:
overlay iter 22-25 ms → src consumed ~40-45 of arr ~55, of a game at ~80. **Our real-motion cadence is
~40 Hz** (interpolating 25 ms gaps); LSFG sits IN the present chain — zero capture loss — keeping all ~80
real frames (12.5 ms gaps). Same present rate, double the real cadence, half the artifact magnitude (the
screenshot's torn glass/signage = the transparent wall × 2-frame pair spans). **The 32b-deferral revival
condition fired exactly as written** → STAGE-30 (pipelining: iter → ~8 ms, consume all arrivals) + 32b fix-2
(arr 55 → game fps) revive together; with both, fg 2 × 80 src = 160 at LSFG pair-cadence with our −5 % tax.

**§9 WRAP — THE SMOOTHNESS FORENSICS CLOSED (2026-06-10/11, stages 34d-f).** The transport pipeline is
exhausted as a defect source — every link verified live: full capture (MinUpdateInterval 8 ms after the 1 ms
duplicate-flood poisoned the pacing EMA), monotonic content (34e: the real presented is THE PAIR's cur — the
operator's theory, confirmed), no set re-presents (34f: the backwards-flash while walking), spaced presents,
honest lat ~28-32 ms. Operator state: first acknowledged smoothness improvements; "ya no hay más dónde
rascarle para GENERARLOS, pero sí en CÓMO generarlos" — agreed, the remaining work is FG QUALITY:

| refinement item | the artifact it targets | cost class |
|---|---|---|
| **R1 pair-span pacing**: when F's pair spans 2 source frames (a skip), its interps represent 2T of motion but are paced over 1T → momentary 2× velocity then ½× → the "vibración al caminar". F publishes the span; P paces the set over span×T. | position vibration | SMALL (plumbing exists from 34e) |
| **R2 temporal MV smoothing**: EMA the MV field across consecutive pairs → damps per-tile MV quantization jitter (±1 px tile oscillation between interps). | residual vibration / tile shimmer | SMALL (one pass or in-shader blend) |
| **R3 the slip texture after 34f**: skipping consumed sets changed P's wake timing (slip avg 1-2 → 3-7, max 20-34 in the last run) — the skip should re-wait on f_seq, not fall through to the next capture wake. | pacing evenness | SMALL |
| **R4 the knob trade** (ghost↔HUD-wobble): per-region or adaptive gates rather than global thresholds. | HUD wobble at relaxed gates | MEDIUM |
| **R5 better flow** (the LSFG-3.x gap): richer motion estimation (multi-scale refinement+filtering, or learned flow). Knobs and R1-R4 do NOT close this. | the quality ceiling | LARGE (its own arc; cost/benefit vs the product decision) |

**The product position after the full head-to-head saga:** we win game-tax (−5 % vs −20 %), working dual-GPU
on x4 rigs, measured latency honesty; LSFG wins raw FG quality. R1-R3 are the cheap polish that may close the
perceptual gap substantially; R5 is the strategic decision (build the quality arc vs package the
lowest-tax-FG positioning with Ayama).

**§9 CODA — THE WALKING TREMBLE: ROOT-CAUSED AND KILLED (2026-06-10/11, STAGE-35→36, operator-verified
"desapareció").** STAGE-35 (Opus: R1 span pacing, R2 `--mv-smooth`, R3 f_seq re-wait, R4 `--fg-factor auto`;
commit `58e496f` + supervisor R2b span-normalization: EMA in VELOCITY space, raw MVs of different-span pairs
differ 2× for the same true velocity) did NOT kill the tremble — and the forensic chain that followed is the
arc's cleanest:
1. **R4b (`e9a30ef`)**: the auto-N freshness rule measured set-age at publish = pipeline DEPTH, not staleness
   (degrading N never shrank it) → guaranteed ~1 Hz N-flap (log: auto:3↔2, present 230↔155). Fix: span>1
   persistence as the falling-behind signal + dead band (0.90/0.65×Tsrc) + 90-set dwell. N stabilized;
   tremble persisted → N exonerated (operator's fixed-fg2 control run confirmed).
2. **The fg1 discriminator** (operator-run): passthrough = slip 0.0/0.0, drop 0, lat 3.5 ms — capture,
   convert, overlay, present all EXONERATED. The tremble lives only in the FG chain.
3. **STAGE-36 (`1148b7a`) — the root**: F consumed the NEWEST capture, so every long F iteration (worst
   13-16 ms vs the ~12.8 ms source gap) skipped a source → span-2 pair → 2× displacement → degraded interps,
   2-5×/s, irregular. Fix: **in-order ingest** (consume last+1, span 1; the 1-frame backlog drains because F
   is faster on average; jump only on real stalls) over a deepened capture ring (kCapSlots=4, separate from
   the F→P generation ring NS=2). New stat `skip N/s` = the tremble signature, the direct observable.
**Operator verdict on BF6: tremble GONE; `skip 0.0/s` on every line (~85 lines), `fg auto:2` stable, lat
20-27 ms.** Side effect: the intermittent startup crash (1-of-2, the 2-slot producer race) probed 0/6 after
the ring deepening. R1 in the table above is thereby superseded in the common case (spans are now always 1
except genuine stalls, where R1's span pacing remains the safety net). Remaining empirical: the `--mv-smooth`
A/B (R2) — now meaningful, the tremble no longer masks it; auto settles at N=2 under BF6 (the comfortable
maximum per the operator's never-force principle) — `--fg-factor 3` fixed remains available, now skip-free,
if peak output is preferred over headroom.

## §10 — Product tiers (operator direction, 2026-06-11)

After the head-to-head verdict ("en nuestro terreno ganamos: su input lag es abismal, el nuestro jugable"),
the operator set the market expansion: attack LSFG's own turf too, as configurations of THE SAME binary —
the router/capability detection already makes the three tiers one codebase:

| tier | hardware | status | honest positioning |
|---|---|---|---|
| 1 | single dGPU | EXISTS (`--fg-gpu/--convert-gpu/--present-gpu primary`, the 26b ofpA path) — needs config testing + tuning, not building | LSFG's exact game; differentiators = measured latency, the never-force adaptive principle (35-R4), blur-not-curves artifacts. **An alternative, not a killer** (operator's own calibration). |
| 2 | dGPU + iGPU auxiliary | tech PROVEN (26c zero-copy convert+pack, 33 present-from-iGPU) — needs the tier packaging | **THE PRIZE: the majority market** (most gaming desktops/laptops = one dGPU + an idle iGPU; laptops' iGPU often OWNS the display → our G-present is the native path there). LSFG has nothing here. Pitch: "tu iGPU por fin gana su lugar" — the anti-obsolescence thesis on hardware everyone already has. |
| 3 | multi-dGPU | PROVEN (−5 % game tax vs LSFG's −20 %; the dual-GPU mode that works on x4 rigs) | the crown; small niche, strongest evidence. |

Sequence: STAGE-35 (adaptive generation) closes the refinement first; then tier-1/2 config validation runs
(same rig: tier 1 = primary-everything; tier 2 = primary+iGPU, no 1080 Ti) before the Ayama-convergence
design folds all tiers into the product story.
