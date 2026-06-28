# render_assistant

**STAGE-25/28b/29/29b — RENDER_ASSISTANT_PLAN.md §8.**
Continuously-running binary: DXGI Desktop Duplication / WGC capture → frame-gen
→ optional upscale → Nx present pacing. Assembles the proven bench pieces from
STAGE-2/3/4/5/5b/7/7b into one usable app. No measurement scaffolding.

## What it does

```
Capture (DXGI DD or WGC, STAGE-28b)
  → format routing (8/10bpc SDR passthrough · FP16 HDR tone-map)
  → host-bounce cross-device transfer (VK_EXT_external_memory_host, STAGE-2)
  → OpticalFlowPipeline frame-gen (pyramid + confidence + agreement gate, STAGE-15/20/24)
      real N → [record_optical_flow t=1/fg_factor] → interp 1
               [record_warp_only   t=k/fg_factor]  → interp k  (k=2..fg_factor-1)
  → optional bilinear/Lanczos 2× upscale on iGPU (STAGE-4)
  → Nx present pacing (STAGE-29b — content-monotonic order):
      … → interp 1(prv→cur) → … → interp fg_factor-1 → real cur → interp 1(cur→nxt) → …
      paced to src_interval_ema / fg_factor spacing (sleep_until, EMA α=0.1, STAGE-29)
  → present on monitor P
```

Source-paced (blocks on `AcquireNextFrame`). The interps of `prv→cur` are presented
BEFORE the real `cur` (their content time precedes it) — interpolation therefore
inherently DELAYS the real frame; the cost is reported as the `lat` stat (STAGE-29b).
The real frame (hostR) and the interp frame (hostI) live in DISJOINT host allocations,
so the FG write no longer clobbers the real frame. `--fg-factor 1` = passthrough (real
frames only, no FG, zero added latency). Runs until Esc or window close. Teardown is
RAII — every Vulkan/D3D11 handle is released.

## Build

Standalone — does NOT need the root Phyriad CMake. Needs VULKAN_SDK.

**MinGW (DD-only, no WGC):**
```
cmake -S apps/render_assistant -B build-render-assistant -G Ninja
cmake --build build-render-assistant
```

**MSVC (DD + WGC, required for `--capture-api wgc` — STAGE-28b):**
```
cmd /c "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat && cmake -S apps\render_assistant -B build-render-assistant -G Ninja -DCMAKE_CXX_COMPILER=cl && cmake --build build-render-assistant"
build-render-assistant\render_assistant.exe --help
```

## Usage

```
render_assistant [options]

  --monitor M             Capture from DXGI output M (default: 0)
  --present-monitor P     Present on DXGI output P (default: auto = other monitor)
  --list-monitors         Print available outputs and exit
  --residual-ceil F       FG gate (a): max SAD residual (default: 32.0)
  --conf-improv F         FG gate (b): min SAD improvement fraction (default: 0.5)
  --agreement F           FG agreement gate: max per-tile d (default: 0.20)
  --fg-factor N           Output multiplier: 1=passthrough, 2=2× (default), 3=3×, 4=4× (N-1 interps)
  --no-upscale            Skip 2× upscale; present at working resolution
  --upscale-lanczos       Lanczos-2 upscale instead of bilinear (slower, sharper)
  --assist-gpu NAME       GPU name fragment for frame-gen (default: first non-primary discrete)
  --windowed              Windowed+titled mode (debug); default = borderless+topmost
  --capture-api API       dd (default) | wgc — MSVC build required for wgc
  --window SUBSTR         WGC only: capture by window title substring instead of monitor
  --help                  This message
```

Example (capture monitor 0, present on monitor 1):
```
render_assistant.exe --monitor 0 --present-monitor 1
```

## Honest scope

- **SDR only.** DXGI Desktop Duplication on an HDR output delivers black raw pixels via the
  MPO/overlay wall (STAGE-7b measured, definitive). Set the captured monitor to SDR or use
  WGC for HDR. The format-routing code handles FP16 HDR (`--agreement 0` won't change this).
- **Fast-edge HUD artifact.** The agreement gate (STAGE-24) suppresses the static HUD ghost
  nearly completely on standard motion. The residual worst-case is abrupt camera motion +
  translucent HUD + the external-capture round-trip — even the native game tears there. This
  is the inherent limit of post-warp agreement on external capture.
- **+1 frame latency (capture round-trip).** The assistant processes frame N while the game
  renders N+1 (the Lossless-Scaling tradeoff). Acceptable for anti-obsolescence; not for esports.
- **Interpolation delay (Nx, `--fg-factor ≥ 2`).** Because interp(prv→cur) precedes real `cur`
  in display order (content-monotonic, STAGE-29b), the real frame is held back by ~(N−1)/N·T
  (the interp present slots) plus FG compute. This is the inherent, honest cost of interpolation
  — the same tradeoff Lossless Scaling makes — and is reported live as the `lat X.Xms` stat
  (real-present time minus that frame's capture arrival, EMA α=0.1). Passthrough (`--fg-factor 1`)
  adds none.
- **Budget.** 1080p: transfer ~2.9ms, FG ~2.6ms (pillar default Rc6/Rf2), upscale <1ms.
  Total ~7ms on the 1080Ti path < 16.67ms. Budget is TIGHTER when GPU A (4090) is shared
  with a heavy game (STAGE-7 Minecraft: 15.97ms mean). STAGE-26 wires the load-aware router.
- **`--fg-factor N` (STAGE-29 / 29b, default 2×).** Presents N frames per source frame:
  N−1 interpolated at phases t=k/N (k=1..N-1) followed by one real frame — **in that order**.
  The interps of the `prv→cur` pair lie at content time BEFORE `cur`, so they are displayed
  first; the real `cur` follows (STAGE-29b corrected ordering — the prior code presented real
  first and the older interps after, making displayed content time regress every interp slot →
  judder). The first interp (k=1) runs the full pyramid block-match (`record_optical_flow`);
  subsequent interps (k>1) reuse its MV via `record_warp_only` (skip block-match, cheaper).
  Pacing: an `anchor` is the ACTUAL timestamp of the previous real present; interp k is paced to
  `anchor + k·(T/N)` and the next real to `anchor + T`, where T is the EMA of inter-capture
  intervals (`sleep_until`, α=0.1). After each real present the anchor is re-seated on its
  measured time → self-correcting drift. If the source frame rate varies, EMA adapts over ~10
  frames; transient drops cause a brief burst of catch-up presents before the anchor re-seats.
  Latency: the real frame is delayed by ~(N−1)/N·T + FG compute (reported as `lat`). The host
  bridge uses two disjoint WW×WH×4 allocations (hostR real / hostI interp, ~+8 MB) so the FG
  write cannot clobber the real frame. `--fg-factor 1` = passthrough (real frames only, no FG,
  no pacing or latency overhead).
- **Fullscreen-exclusive source.** If the captured app is running fullscreen-exclusive, DXGI
  DD may deliver partial / torn frames due to MPO flip-model interaction. Use windowed or
  borderless-windowed mode on the captured application to avoid this.
- **Borderless + topmost default (STAGE-27a).** The present window is `WS_POPUP + WS_EX_TOPMOST`
  by default; DWM always composites it regardless of which app has focus. This enables the
  intended workflow: focus the game on monitor A, watch the enhanced output on monitor B — the
  assistant window keeps updating. Esc or Alt+F4 closes it.
- **`--windowed` mode.** Use for debug (shows title bar / chrome, easier to resize and move).
  With `--windowed`, DWM may throttle the window when it's not focused — the focus-freeze issue
  returns. Also uses FIFO present mode (vsync-locked) instead of MAILBOX.
- **Topmost window is intrusively always-on-top.** On a single-monitor setup (or if
  `--present-monitor` equals `--monitor`) the topmost popup will cover other windows. Use
  `--windowed` or pick a separate present monitor in that case.

## Architecture

Two-GPU setup (measured on the author's 4090 + 1080Ti + AMD iGPU rig):
- **GPU A** (primary, LUID-matched to DXGI adapter): capture pipeline + convert/tonemap +
  present (swapchain + blit).
- **GPU B** (assist, first non-primary discrete or `--assist-gpu` fragment): frame-gen via
  `OpticalFlowPipeline` (the pillar).
- **iGPU** (optional): bilinear/Lanczos upscale if `--no-upscale` not set.
- **Host bridge**: one `VK_EXT_external_memory_host` buffer imported by all three devices —
  zero-copy cross-device transfer (STAGE-2 pattern).

STAGE-26: load-aware router (4090-spare FG ↔ 1080Ti offload per frame, STAGE-13).
STAGE-27: pipelining (overlap capture N+1 with enhance N), latency tuning.
STAGE-29 (DONE): Nx present pacing — warp parametric t, record_warp_only for k>1 interps,
  EMA pacing clock.
STAGE-29b (DONE): content-monotonic present order — de-aliased host bridge (hostR/hostI),
  interp(prv→cur) presented BEFORE real cur, anchor-based self-correcting pacing, `lat` stat.
