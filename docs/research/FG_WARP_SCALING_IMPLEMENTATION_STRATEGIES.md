# FG_WARP_SCALING_IMPLEMENTATION_STRATEGIES — the build steps for the warp-scale lever

> **Diátaxis type:** Planning (how-to / implementation strategy). **Normativity:** BCP 14.
> **Status:** `designed` — NOT built. Every file:line below was **verified first-hand** on 2026-06-23
> against the current tree; the *new* code is specified, not written.
> **Tier:** T2 — sibling of [`FG_WARP_SCALING_MASTER_PLAN.md`](FG_WARP_SCALING_MASTER_PLAN.md) +
> [`FG_WARP_SCALING_RISK_REGISTER.md`](FG_WARP_SCALING_RISK_REGISTER.md). Build order is gated by the RISK
> register (no commit while any risk is `open`).

## §1 — The pillar extension (the WAP warp pass)

The warp pass lives entirely in `apps/render_assistant/src/main.cpp` + its compute shader
`apps/render_assistant/shaders/wap_warp.comp`. The cost-bearing resources, verified first-hand:

- **`wapOutA`** — the warp OUTPUT image, created ONCE at init at full-res:
  `img_create(WD,WW,WH,VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT,wOut)`,
  [`main.cpp:4342`](../../apps/render_assistant/src/main.cpp). Its extent = the shader's `imageSize(u_output)`.
  **This is the resource warp-scaling re-sizes.**
- **The single resolution-agnostic pipeline `wapPipeA`** — bound per-tick inside the bridge command buffer;
  its descriptor set binds `wapOutA`'s view at `wap_create`
  ([`main.cpp:2794`](../../apps/render_assistant/src/main.cpp), the `out_v` parameter). The pipeline does NOT
  need re-create for a resolution change (work derives from `imageSize`); the **output image + the descriptor
  binding** do.
- **The sampled MV/SAD/MVB grids + the pair-real inputs (`wPrev`/`wCur`)** are REUSED unchanged: the warp
  samples them via `texture()` at `uv`, so a smaller output dispatch samples the SAME full-res inputs at a
  coarser `uv` stride for free — no input re-size. The quality cost of warp-scaling = fewer output samples of
  the same inputs (the comment at [`main.cpp:4331`](../../apps/render_assistant/src/main.cpp) already
  documents the warp upsampling a smaller MV field; warp-scale is the orthogonal output-side analogue).

**POD / `GpuDescriptor` change:** none. `--warp-scale N` is a `cfg` int + a derived `warp_div` scalar; it adds
no field to a POD shared across the GPU ABI → **no POD-change gate → no operator approval needed** on that
ground (the build go-ahead is still operator-gated as a T2 change).

## §2 — Concrete build steps (verified file:line)

1. **The output-image divisor (Shape B: a cold-path (re)create helper).** At init,
   [`main.cpp:4342`](../../apps/render_assistant/src/main.cpp) creates `wOut` at `WW,WH`. Replace the literal
   extent with `WW/warp_div, WH/warp_div` (clamped toward 1 if degenerate — mirroring the flow_div clamp at
   [`main.cpp:3100-3101`](../../apps/render_assistant/src/main.cpp)). For a runtime switch, factor this into a
   **cold-path `wap_out_resize(N)` helper** (destroy `wapOutA` + re-`img_create` at the new extent + re-write
   the descriptor binding via the `wap_create` `out_v` path, [`main.cpp:2794`](../../apps/render_assistant/src/main.cpp))
   — invoked ONLY on a pair boundary while no warp tick is in flight, **never on the steady tick** (D-2; RISK
   WS-3). This is the same "cold path, never on the steady tick" discipline the FG_OPTION_A `ResizeBuffers`
   helper carries.

2. **The dispatch-dims divisor (THE primary lever site).** The warp dispatch is
   `vkCmdDispatch(cmdBridge,(WW+7)/8,(WH+7)/8,1)`,
   [`main.cpp:7517`](../../apps/render_assistant/src/main.cpp). It becomes
   `vkCmdDispatch(cmdBridge,((WW/warp_div)+7)/8,((WH/warp_div)+7)/8,1)` — and MUST equal the warp-scaled
   `wapOutA` extent (the shader's `valid` test reads `imageSize(u_output)`,
   [`wap_warp.comp:760`](../../apps/render_assistant/shaders/wap_warp.comp)). At `warp_div==1` this collapses
   to today's `(WW+7)/8,(WH+7)/8` — byte-identical.

3. **The `--afill` secondary dispatch.** The field-visualizer writes IN-PLACE onto `wapOutA` with its own
   `vkCmdDispatch(cmdBridge,(WW+7)/8,(WH+7)/8,1)` + a `pcf{w=WW,h=WH,…}`,
   [`main.cpp:7534`](../../apps/render_assistant/src/main.cpp). If warp-scaled, this dispatch AND its
   `pcf.w/h` MUST use the scaled extent too (it reads/writes the same output image). `--afill` is itself
   default-off, so this only matters when both are on.

4. **The upscale blit (the existing upsampler — reused, not new).** The warp output → bridge blit is
   `srcOffsets[1]={(int)WW,(int)WH,1}`, `dstOffsets[1]={(int)bridge_w,(int)bridge_h,1}`, `VK_FILTER_LINEAR`,
   [`main.cpp:7555`](../../apps/render_assistant/src/main.cpp). The `srcOffsets` become
   `{(int)(WW/warp_div),(int)(WH/warp_div),1}` so the SAME linear blit upsamples the warp-scaled output to the
   bridge extent. The `GENERAL→TRANSFER_SRC` and back `img_barrier`s bracketing it
   ([`main.cpp:7553`, `:7556`](../../apps/render_assistant/src/main.cpp)) are extent-agnostic and carry over.

5. **No shader edit for the core lever.** `out_size = imageSize(u_output)`; `uv0 = (vec2(coord)+0.5)/
   vec2(out_size)`; the store is at the integer `coord` — [`wap_warp.comp:757`](../../apps/render_assistant/shaders/wap_warp.comp)
   `main()`, store at [`:1896`](../../apps/render_assistant/shaders/wap_warp.comp). A smaller output image makes
   `uv` stride coarser over the SAME sampled inputs. Only a later quality-tuning pass (e.g. mip-aware
   sampling) would touch the shader; the off-path SPIR-V is unchanged.

## §3 — The consumer-side wiring (`apps/render_assistant/src/main.cpp`)

`--warp-scale N` is a new default-off flag, parsed in the `parse_extra` block (the MSVC C1061 chain-limit
dodge the codebase uses — the same place `--flow-scale-target-mp` is parsed,
[`main.cpp:1137`](../../apps/render_assistant/src/main.cpp)), NOT the main `parse` chain. It sets
`cfg.warp_scale ∈ {1,2,4}` (reject other values with a printf, mirroring the `--flow-scale` validation at
[`main.cpp:1222`](../../apps/render_assistant/src/main.cpp)), default `1`. Derive `warp_div=(uint32_t)cfg.warp_scale`
clamped toward 1 if `WW/N` or `WH/N` would be degenerate (the [`main.cpp:3100-3101`](../../apps/render_assistant/src/main.cpp)
flow_div clamp is the template). **WAP-only gate:** `--warp-scale N>1` requires warp-at-presenter — mirror the
verified `--flow-scale` WAP-only guard at [`main.cpp:1587-1588`](../../apps/render_assistant/src/main.cpp)
(`if (c.flow_scale>1 && !c.warp_at_presenter) { … reverting to flow-scale 1 }`): revert `warp_scale` to 1 with
a printf when WAP is off. The init allocation-fail clean-degrade (`use_wap=false` at
[`main.cpp:4343`](../../apps/render_assistant/src/main.cpp)) is unchanged.

## §4 — Re-init / capture-continuity handling (the T2-specific section)

This section exists because Shape B (runtime re-init) is in scope; if the operator elects Shape A (startup-
only), §4 collapses to "no runtime re-init occurs" and the WS-1/WS-2 risks fall away.

- **Quiesce in-flight work before re-init.** The warp tick reads `wapOutA` (write) + samples the grids; a
  runtime `wap_out_resize(N)` MUST run on a pair boundary AFTER the present fence for the prior tick has
  signalled (the warp/present fence sites are `vk_live`-wrapped at
  [`main.cpp:7093`](../../apps/render_assistant/src/main.cpp) and the bridge submit chain) and BEFORE the next
  warp dispatch is recorded — never concurrently with a recorded-but-unsubmitted `cmdBridge` (RISK WS-2). The
  switch decision is a per-tick scalar threaded like `t`/`extrap` (a push-constant-adjacent scalar), NOT a
  per-slot constant under `--async-present`'s ping-pong (RISK WS-2 residual).
- **Device-loss-adjacent concurrency.** Destroying + recreating `wapOutA` + re-writing the descriptor is the
  device-loss-adjacent class [`FG_ADAPTIVE_FLOWSCALE_DESIGN.md`](FG_ADAPTIVE_FLOWSCALE_DESIGN.md) §3 names for
  the FLOW lever; the warp case is lighter (one image + one binding, nothing downstream sizes off the warp
  output) but still a Vulkan resource-lifetime mutation that MUST be `vk_live`-guarded (RISK WS-1).
- **The degrade path if re-init fails.** A failed `wap_out_resize` MUST degrade to the LAST-GOOD `wapOutA`
  (keep the prior image, log, stay at the prior `N`), never a torn descriptor or a crash (D-20; RISK WS-1).

## §5 — Default-off / byte-identical discipline

- `--warp-scale` default `1` → `warp_div==1` → the `wapOutA` extent stays `WW×WH`, the dispatch stays
  `(WW+7)/8,(WH+7)/8`, the blit `srcOffsets` stay `{WW,WH}` — every site collapses to today's code, the
  identical pattern `--flow-scale` asserts as "DEFAULT 1 = byte-identical"
  ([`main.cpp:1232`](../../apps/render_assistant/src/main.cpp)). Verify on an unchanged HSR run (the C8 gate).
- The shader is untouched → the off-path SPIR-V is literally unchanged.
- `--warp-scale N>1` is reachable ONLY via the flag and ONLY with WAP on (the §3 guard).

## §6 — Build & validation gates

- Build: `vcvars64` + `cmake --build build-ra-msvc2` (the established RA build).
- `lint_hal` clean (default `seq_cst` in `apps/`, no raw `memory_order_*` — any new atomic for the re-init
  handshake uses the default or an explicit acquire/release with a justification).
- Doc-sync: this change touches no public pillar header (the warp pass is app-local), so `check_doc_sync` is
  not triggered; confirm by grep before commit.
- The C8 `fg_quality_scorer` regression gate green (byte-identical-off proof + the quality delta at `N>1`).
- **Runtime (the operator's eye + the rig probes of MASTER_PLAN §7):** W1 measures the `--wsub` `gpu` slice
  delta + the quality delta on a heavy-frame title; W2 exercises the re-init under a soak before WS-1/WS-2
  flip from `open` to `mitigated`.

## §7 — Staging order (gated by the RISK register)

**W1** (scaled warp + upscale, flag default-off, Shape A or B's static form) → measure the `gpu` slice + the
quality → **W2** (Shape B re-init / device-loss hardening — **required to commit** the runtime form: WS-1,
WS-2) → only THEN consider an eye/measure gate for a possible runtime-adaptive controller (W3, deferred). No
commit of the runtime form while any of WS-1/WS-2/WS-3 is `open`.

**Author's flags (must be built + verified before claiming `mitigated`):** the `wap_out_resize(N)` cold-path
helper does not yet exist; the per-tick switch handshake is new code; the `N` clamp + the WAP-only guard are
specified, not written; the `--wsub` `gpu`-isolation measurement has not been run. None of these is built —
the RISK register seeds them `open` with the mitigation-as-code specified, flipping to `mitigated` only after
build-green + the named first-hand runtime check is recorded.

*Made with my soul — the supervisor, for Swately.*
