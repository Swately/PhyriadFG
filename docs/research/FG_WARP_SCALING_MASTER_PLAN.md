# FG_WARP_SCALING_MASTER_PLAN — cheapen the warp slice on heavy frames (the second cost lever)

> **Diátaxis type:** Planning (design / master plan). **Normativity:** BCP 14 (MUST/SHOULD/MAY).
> **Status:** `designed` — NOT built. Every claim about PhyriadFG's *future* behaviour is a hypothesis to
> be MEASURED; only the cited code refs and the prior `FG_LSFG_HEADTOHEAD_MEASURED.md` numbers are
> `measured`/`shipping`. Nothing in this plan is implemented.
> **Tier:** **T2** (crash/device-loss — a runtime warp-pipeline re-init touches live Vulkan resource
> lifetime on the present path; concurrency — re-init mutates warp resources the steady tick reads; a
> dogma cluster at stake — D-2 zero-alloc hot path, byte-identical-off, the no-game-cap *arc-invariant*, D-13 correctness).
> Per [`canon/PLAN_TIER_PROTOCOL.md`](../canon/PLAN_TIER_PROTOCOL.md) §1.1 (triggers 1, 2, 5) this plan is
> one leg of a **mutually-linked triad**:
> - [`FG_WARP_SCALING_IMPLEMENTATION_STRATEGIES.md`](FG_WARP_SCALING_IMPLEMENTATION_STRATEGIES.md) — the build steps.
> - [`FG_WARP_SCALING_RISK_REGISTER.md`](FG_WARP_SCALING_RISK_REGISTER.md) — the failure modes + mitigation-as-code.
>   **No risk may remain `open` at commit; no commit while any is open.**
>
> **Evidence base (the single-sources):** [`FG_LSFG_HEADTOHEAD_MEASURED.md`](FG_LSFG_HEADTOHEAD_MEASURED.md)
> (the measured cost gaps + the flow-scale sweep) · [`FG_OPTION_A_MASTER_PLAN.md`](FG_OPTION_A_MASTER_PLAN.md)
> §7.1 (the heavy-frame WARP-dominates finding) + §7.2 lever #1 (slice-minimization = the no-cap-dogma
> expression of "aprovechar, no desperdiciar") · [`FG_ADAPTIVE_FLOWSCALE_DESIGN.md`](FG_ADAPTIVE_FLOWSCALE_DESIGN.md)
> (the kin lever for FLOW; its §3 establishes init-sized-pipeline re-init = T2, the precedent this plan inherits).

## §1 — Why (the motivation, each number bucketed)

On **light** titles, `--flow-scale 2` (reduced-res flow) closes PhyriadFG's per-frame cost to LSFG parity —
`measured` HSR single-GPU on the 4090: iter 2.73→0.94 ms, util 95→56 %, power 294→138 W
([`FG_LSFG_HEADTOHEAD_MEASURED.md`](FG_LSFG_HEADTOHEAD_MEASURED.md) §2). On **heavy-frame** titles the
flow-scale lever runs out of leverage. The Option-A §7.1 partial result (`measured`, NVML + `--csv`, BF6 4K
idle-but-saturated, single-GPU) recorded a present-lambda warp signal of **`warp_ms ≈ 5.91`** at flow-scale 1
vs **`5.89`** at flow-scale 2, with `MsAddedLat` **124.5 → 122.9 ms** — i.e. cutting the flow grid barely
moves the slice because the WARP dominates it on heavy frames.

**Mechanism (the lever this plan opens, verified first-hand against the live tree).** The warp pass (warp-at-
presenter, "WAP") is dispatched at **full output resolution**, decoupled from `--flow-scale`:

- The single warp dispatch is `vkCmdDispatch(cmdBridge,(WW+7)/8,(WH+7)/8,1)` —
  [`apps/render_assistant/src/main.cpp:7517`](../../apps/render_assistant/src/main.cpp) — one 8×8 workgroup
  per output tile (`layout(local_size_x = 8, local_size_y = 8, …)`,
  [`shaders/wap_warp.comp:201`](../../apps/render_assistant/shaders/wap_warp.comp)).
- The shader keys all per-invocation work off `imageSize(u_output)`: `out_size = imageSize(u_output)`
  ([`wap_warp.comp:759`](../../apps/render_assistant/shaders/wap_warp.comp)); the store is at the integer
  `coord` ([`:1896`](../../apps/render_assistant/shaders/wap_warp.comp)); `u_output` is the rgba8 writeonly
  storage image at binding 4 ([`:207`](../../apps/render_assistant/shaders/wap_warp.comp)). `main()` opens at
  [`:757`](../../apps/render_assistant/shaders/wap_warp.comp). So warp cost scales with the **output pixel
  count `WW×WH`**, NOT the MV/flow grid.
- `--flow-scale` only shrinks the FLOW grid: `flow_div = (uint32_t)cfg.flow_scale`, clamped so `WW/N,WH/N ≥
  256` ([`main.cpp:3100-3101`](../../apps/render_assistant/src/main.cpp)). The warp's sampled MV/SAD/MVB grids
  size off `ofp.motion_width()/height()` and shrink with the flow; the **pair-real inputs (`wPrev`/`wCur`),
  the warp output, and the dispatch dims all stay full-res** — the in-code comment states this directly at
  [`main.cpp:4329-4332`](../../apps/render_assistant/src/main.cpp). This is the proof that warp-scale is a
  **distinct cost axis** from flow-scale.

**→ The heavy-frame cost lever is WARP-scaling:** run the warp pass at a reduced output resolution and
upscale the result — the operator's "our own scaling" idea, here data-justified
([`FG_OPTION_A_MASTER_PLAN.md`](FG_OPTION_A_MASTER_PLAN.md) §7.1 conclusion-2, §7.2 lever #1) as the next
slice-minimizer.

## §2 — The contract ("done")

A new **default-off** mode `--warp-scale N` (`N ∈ {1, 2, 4}`, default `1`) that runs the warp pass at
`WW/N × WH/N` and upscales the warp output to the bridge/present extent, such that, on a heavy-frame title on
this rig:

1. **Slice cost reduced, MEASURED.** The warp GPU term drops — isolated via `--wsub`'s `gpu` sub-timing
   ([`main.cpp:1399`](../../apps/render_assistant/src/main.cpp)), NOT the whole `wap_warp_ema`
   present-lambda EMA ([`main.cpp:8647`](../../apps/render_assistant/src/main.cpp), which bundles
   up+rec+gpu+prs). A `warp_ms` headline attributed to the dispatch alone is a defect (§7).
2. **Quality bounded by an oracle + eye gate (D-13).** Reduced-res warp + upscale changes the output; it
   ships with a quality measurement (the C8 `fg_quality_scorer` gate + the operator's eye), not a
   faster-but-wrong path.
3. **Byte-identical-off, verified.** At `N==1` every divisor site collapses to today's code; proven on an
   unchanged HSR run (the C8 regression gate).
4. **The no-game-cap / no-game-downscale invariant held.** Warp-scaling reduces the resolution of **OUR**
   synthesized frame (`wapOutA`), then upscales it for present — it is NOT a downscale or cap of the game's
   own rendered frame. The game's per-frame cost is untouched. This is the load-bearing dogma check (§5).

## §3 — Architecture

The warp pass already owns its output image and an upscale blit; warp-scaling is a divisor threaded into
both, plus the resource-lifetime handling that makes it T2.

- **The warp output `wapOutA`** is created ONCE at init at full-res — `img_create(WD,WW,WH,
  VK_FORMAT_R8G8B8A8_UNORM, STORAGE|TRANSFER_SRC, wOut)`,
  [`main.cpp:4342`](../../apps/render_assistant/src/main.cpp). Its `imageSize` is what the shader reads as the
  dispatch domain. **A warp_div MUST shrink THIS extent** (or select a smaller pre-sized output image) to
  actually reduce work.
- **The upscaler already exists.** The warp output is blitted to the bridge with a LINEAR filter —
  `vkCmdBlitImage(…, wapOutA.img → bridge_img.img, srcOffsets {WW,WH}, dstOffsets {bridge_w,bridge_h},
  VK_FILTER_LINEAR)`, [`main.cpp:7555`](../../apps/render_assistant/src/main.cpp). If `wapOutA` is warp-scaled
  smaller, this same blit upsamples it — the `srcOffsets` become `{WW/N, WH/N, 1}`. No new upscale pass.
- **The pipeline is resolution-agnostic.** The single `wapPipeA` derives its work from `imageSize(u_output)`
  at dispatch, so the VkPipeline itself needs NO re-create; **no shader edit is needed for the core lever**
  (the off-path SPIR-V is literally unchanged). The descriptor set, however, binds `wapOutA`'s view at
  `wap_create` ([`main.cpp:2794`](../../apps/render_assistant/src/main.cpp), `out_v` parameter) — so the
  OUTPUT IMAGE + the descriptor binding are what a resolution change touches.

**The two implementable shapes (the §4 crux chooses one):**

- **Shape A — STARTUP-only pick (init-sized; ~T1-shaped, NOT this plan's productized form).** One `wapOutA`
  created at `WW/N × WH/N`, one descriptor write, the dispatch dims and the blit `srcOffsets` derived from
  the scaled extent at init. Operationally identical to `--flow-scale` (a startup choice). No runtime re-init
  → most of the T2 surface does not arise.
- **Shape B — RUNTIME warp-pipeline re-init (the T2 in-scope shape this plan productizes).** Switch `N` mid-
  stream by recreating/re-sizing `wapOutA` + re-writing the descriptor binding on a labelled **cold path**
  (never the steady tick, D-2). This is the device-loss-adjacent, concurrency-bearing shape — exactly the
  hazard [`FG_ADAPTIVE_FLOWSCALE_DESIGN.md`](FG_ADAPTIVE_FLOWSCALE_DESIGN.md) §3 classified as T2 for the
  analogous FLOW lever (init-sized resource, a runtime resolution change = "a big stall + device-loss-adjacent
  concurrency"). Warp-scaling carries the identical re-init hazard on the warp output + descriptor.

This plan productizes **Shape B** (runtime re-init is in scope per the task), and therefore binds T2. The
RISK_REGISTER's commit gate is the consequence. (If the operator elects Shape A only, the change drops to T1
and the RISK_REGISTER's re-init rows fall away — recorded for honesty, not assumed.)

## §4 — The crux tension (the honest #1 open question)

The runtime-re-init stall + the in-flight-resource hazard (Shape B) vs the modest extra VRAM of a pre-sized
ladder of output images (a Shape-A/B hybrid that keeps `{1×,2×,4×}` `wapOutA` images + descriptor sets and
switches per tick with no re-init). The pre-sized ladder trades a few extra RGBA8 `WW×WH/N²` images (modest)
for the elimination of the re-init class — the same trade
[`FG_ADAPTIVE_FLOWSCALE_DESIGN.md`](FG_ADAPTIVE_FLOWSCALE_DESIGN.md) §3 weighs for FLOW (where it found the
pre-built form is itself architectural because the MV grid sizes the whole downstream — but the warp output
does NOT size anything downstream, so the warp ladder is genuinely lighter). The second open question is
**same-frame correctness of the upscaled warp at disocclusion**: the per-pixel commit/rescue/matte decisions
are taken at fewer points, so thin features (text, edges) degrade more directly than under flow-scale — the
D-13 oracle bounds it.

## §5 — Honest scope (what warp-scaling does and does NOT promise)

**Does (the bet):**
- Cheapen the WARP slice on heavy frames — the [`FG_OPTION_A_MASTER_PLAN.md`](FG_OPTION_A_MASTER_PLAN.md) §7.2
  lever #1 ("minimize the FG slice"), a cost-DOWN lever that shrinks our footprint (and hence the necessary
  saturation headroom) toward zero. It REDUCES VRAM/bandwidth (smaller `wapOutA`, fewer invocations, smaller
  blit source).

**Does NOT:**
- **No game cap, no per-frame game downscale.** The scaled surface is `wapOutA` (OURS); the game's render is
  untouched (§2.4). This is the distinction that keeps the no-cap / no-recommend-cap dogma.
- **No 100 % quality match.** Reduced-res synthesis + upscale loses fine output detail; the trade-off is
  eye/oracle-gated, mirroring the `--flow-scale` clamp discipline (degrade gracefully, never break).
- **No claim the freed headroom is "filled."** Slice-minimization shrinks the necessary headroom; it does not
  fill it with more frames ([`FG_OPTION_A_MASTER_PLAN.md`](FG_OPTION_A_MASTER_PLAN.md) §7.2 corrects the
  fill-the-headroom framing for single-GPU).
- **No replacement for `--flow-scale`.** The two compose but are not redundant: flow-scale coarsens the MOTION
  ESTIMATE; warp-scale coarsens the SYNTHESIS sampling of full-res inputs then upscales.

## §6 — Staged plan

- **Stage 0 — bank any independent win.** The static `--flow-scale 2`/`auto` (the FLOW cost lever) is already
  the shipping cost lever for light titles; it is not part of this arc. Noted for sequencing.
- **Stage W1 — the scaled warp + upscale (flag default-off).** Add `--warp-scale N` (default 1); shrink the
  `wapOutA` extent + the dispatch dims + the blit `srcOffsets`; the `--afill` in-place visualizer dispatch
  ([`main.cpp:7534`](../../apps/render_assistant/src/main.cpp)) must use the same scaled extent. **Measure:**
  the warp `gpu` sub-timing delta (`--wsub`) and the quality delta (the C8 oracle + the eye).
- **Stage W2 — device-loss / re-init hardening (Shape B; REQUIRED to commit).** The cold-path re(create) of
  `wapOutA` + descriptor re-write, quiesced against in-flight warp work; the warp-path crash-class wrapping
  (`vk_live`) preserved. The RISK_REGISTER's re-init + concurrency rows MUST be `mitigated` before any commit.
- **Stage W3+ — deferred.** The runtime-adaptive controller that auto-raises `N` under saturation (the kin of
  the flow-scale-adaptive controller); the pre-sized ladder as a no-re-init alternative if W2's stall proves
  visible.

## §7 — Open measurements before any number is written as `measured`

1. **The slice-cost delta** — the `--wsub` `gpu` sub-timing at `N=1` vs `2` vs `4` on a heavy-frame title (the
   `wap_warp_ema` present-lambda EMA MUST NOT be attributed to the warp dispatch alone — §2.1).
2. **The quality delta** — the C8 oracle score + the operator's eye at each `N`.
3. **The re-init stall duration** — the cold-path `wapOutA`-recreate + descriptor-write time (Shape B).
4. **Whether re-init introduces a hazard under load** — a soak with the `--csv` freeze counter clean.

The `~5.9 ms` warp figure of [`FG_OPTION_A_MASTER_PLAN.md`](FG_OPTION_A_MASTER_PLAN.md) §7.1 is a present-
lambda (`wap_warp_ema`) reading, NOT an isolated warp-GPU time; it is the motivation, not a warp-dispatch
measurement. No warp-scaling number is `measured` until the `--wsub` `gpu` term is captured.

*Made with my soul — the supervisor, for Swately.*
