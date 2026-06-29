# FG_OPTION_A_IMPLEMENTATION_STRATEGIES — the build steps for the own-window present

> **Diátaxis type:** Planning (how-to / implementation strategy). **Normativity:** BCP 14.
> **Status:** `designed` — NOT built. Every file:line below was **verified first-hand** on 2026-06-23
> against the current tree; the *new* code is specified, not written.
> **Tier:** T2 — sibling of [`FG_OPTION_A_MASTER_PLAN.md`](FG_OPTION_A_MASTER_PLAN.md) +
> [`FG_OPTION_A_RISK_REGISTER.md`](FG_OPTION_A_RISK_REGISTER.md). Build order is gated by the RISK register
> (no commit while any risk is `open`).

## §1 — The pillar extension (`PresentSurface`)

The flip-model path **already exists** as `Style::Baseline` — verified first-hand at
`framework/render/present/src/PresentSurface.cpp:218-224`:

```cpp
if (desc.style == Style::Baseline) {                 // classic flip-model HWND swapchain
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    sd.AlphaMode  = DXGI_ALPHA_MODE_IGNORE;
    sd.Scaling    = DXGI_SCALING_NONE;
    hr = fac->CreateSwapChainForHwnd(impl->dev, impl->hwnd, &sd, nullptr, nullptr, &impl->sc);
    ...
}
```

Option A needs a **new `Style::OwnWindow`** (NOT a reuse of `Baseline`, NOT a new `Pacing`), because the
window *recipe* differs (the focus/yield contract) even though the *swapchain* is identical. No
`PresentSurfaceDesc` POD field is added → **no `GpuDescriptor`/POD change → no operator approval needed**
(the POD-change gate does not trip). `Pacing::Immediate` (`Present(sync_interval,0)` at `:325`) is correct
and the existing `waitable`/`sync_interval`/`present_colorspace` knobs (`:214-215, 254-277, 309, 325`) carry
over unchanged.

## §2 — Concrete build steps (verified file:line)

1. **New style enum value** — `framework/render/present/include/phyriad/render/present/PresentSurface.hpp:47-51`:
   add `OwnWindow` to `enum class Style` (after `DcompCt`). Keep `Baseline` untouched (it is the §10 fallback).

2. **Window recipe branch** — `PresentSurface.cpp:181-194`. Current verified recipe:
   `DWORD ex = WS_EX_TOPMOST | WS_EX_NOACTIVATE;` then `+NOREDIRECTIONBITMAP` for Dcomp/DcompCt and
   `+(WS_EX_LAYERED|WS_EX_TRANSPARENT)` for DcompCt. For `Style::OwnWindow`:
   - **KEEP** `WS_EX_TOPMOST | WS_EX_NOACTIVATE` — **non-activating is mandatory** (the game keeps input
     focus; we never steal keyboard/mouse). *(This corrects the design-workflow's first draft, which
     proposed an activatable window — that would steal game input; rejected, see MASTER_PLAN §3 / RISK OA-2.)*
   - **DO NOT** add `WS_EX_NOREDIRECTIONBITMAP` (DComp-only) or `WS_EX_LAYERED|WS_EX_TRANSPARENT` (click-through
     overlay — OwnWindow is the opaque displayed surface).
   - Keep `WS_POPUP`, monitor-extent position (`:188-194`), `SW_SHOWNOACTIVATE` (`:192`). This is
     borderless-FSO, **never** `SetFullscreenState` (RISK OA-6).

3. **Swapchain branch** — `PresentSurface.cpp:218` / `:225`. Today the gate is `if (desc.style == Style::Baseline)
   { flip HWND } else { composition + DComp }`. Route `Style::OwnWindow` down the **flip branch**: change the
   guard to `if (desc.style == Style::Baseline || desc.style == Style::OwnWindow)`. The DComp block (`:225-242`:
   `DCompositionCreateDevice`/`CreateTargetForHwnd`/`CreateVisual`/`SetContent`/`SetRoot`/`Commit`) is then
   naturally skipped for OwnWindow — no DComp resources, the flip HWND swapchain presents straight to the
   display. The backbuffer cache (`:246`), waitable (`:254-261`), colorspace (`:267-277`), and WDA (`:280-284`)
   blocks are all style-agnostic and carry over.

4. **The foreground-yield contract (THE load-bearing new code)** — `overlay_proc` (`PresentSurface.cpp:83`
   area) is today a near-stub. Because OwnWindow is **non-activating**, it does **not** get its own
   `WM_KILLFOCUS`. The yield MUST be driven by monitoring the **foreground window**:
   - Run a lightweight foreground monitor (a `SetWinEventHook(EVENT_SYSTEM_FOREGROUND, …)` on the create()
     thread, dispatched by the existing `submit()` message pump at `:301-304`, **or** a poll of
     `GetForegroundWindow()` each present tick). When the foreground is **neither our window nor the captured
     game** (the user alt-tabbed to the desktop / another app), set an `impl->yielded` atomic + drop topmost
     (`SetWindowPos(HWND_NOTOPMOST)` + `ShowWindow(SW_HIDE)`), so the desktop is reachable.
   - When the game returns to foreground, clear `yielded` + re-assert topmost + show.
   - `MonitorFromWindow(game_hwnd, MONITOR_DEFAULTTONEAREST)` (the game HWND is `wgc_target_hwnd`,
     `main.cpp:3009/3055`) binds us to the game's monitor and re-derives on a monitor move (RISK OA-8).

5. **The `yielded` early-return in the hot path** — `PresentSurface.cpp:302` (right after the message pump):
   add `if (impl_->yielded.load(std::memory_order_acquire)) return {};` — shaped exactly like the keyed-mutex
   timeout early-return at `:317`. While yielded we present nothing → the game holds the scanout plane →
   **byte-identical to passthrough.** This is the no-lock-out invariant's primary release path (RISK OA-2).

6. **The present-thread watchdog** — a separate thread monitors a heartbeat the present thread bumps each
   tick; if it stalls > N ms **while displayed**, the watchdog force-hides our window
   (`ShowWindow(SW_HIDE)`+`HWND_NOTOPMOST`) so a wedged FG can never hold the panel. This is the **same idea**
   as the existing window-death watchdog verified at `main.cpp:7919-7926` (the `!IsWindow(wgc_target_hwnd)`
   gate that distinguishes death from minimize) — extend that machinery to also cover the present-thread
   stall. `N` is a parameter to choose + measure (target: low single-digit ms / a couple of frames).

7. **The device-loss branch** — `PresentSurface.cpp:325`. Today: `HRESULT hr = impl_->sc->Present(sync_interval,0);
   return SUCCEEDED(hr) ? {} : fail(SystemError);` — no device-loss branch. Add a `dxgi_live(HRESULT)` twin of
   the verified `vk_live` (`main.cpp:2900-2902`, `g_device_lost` atomic at `:2893`):
   - `DXGI_STATUS_MODE_CHANGED` / `DXGI_STATUS_OCCLUDED` → `ResizeBuffers` + re-`GetBuffer` (a **cold-path**
     helper on `Impl`, mirroring `:246`; **never** allocate on the steady present path, D-2). No exit.
   - `DXGI_ERROR_DEVICE_REMOVED` / `DXGI_ERROR_DEVICE_RESET` → set the same terminal `g_quit`/`g_device_lost`
     flags the present-site `vk_live` sets (`main.cpp:7031` is the VK-side bridge submit already guarded) →
     the proven Ctrl-C unwind → teardown (guarded `&& !g_device_lost`, `main.cpp:8925`) → exit. **Exit IS
     passthrough** (the game keeps rendering behind us); here display-return is also load-bearing (RISK OA-3).

## §3 — The consumer-side wiring (`apps/render_assistant/src/main.cpp`)

The present descriptor is built at `main.cpp:6922-6924` (verified):

```cpp
psd.monitor_index=cfg.pres_mon; psd.width=0; psd.height=0;
psd.waitable=cfg.present_waitable; psd.sync_interval=(uint8_t)cfg.present_sync;
psd.present_colorspace=(uint8_t)cfg.present_colorspace;
```

`psd.style` is **NOT set here** → it takes the header default `Style::DcompCt` (`PresentSurface.hpp:71`).
Option A adds a new default-off flag `--present-own-window` (a `cfg.present_own_window` bool, parsed in the
`parse_extra` block per the established pattern, NOT the main `parse` chain — the C1061 lesson) that, when
set, adds one line near `:6922`:

```cpp
if (cfg.present_own_window) psd.style = pp::Style::OwnWindow;
```

The create-fail clean-quit (`main.cpp:6930`, D-20, no fallback) is unchanged. The capture path is unchanged:
the game is captured by `wgc_target_hwnd` via `CreateForWindow` (`main.cpp:3978`) or `CreateForMonitor`
(`:3982`) — set at init, BEFORE the present window exists, so it targets the GAME, not our window (RISK OA-5).
A startup assert `wgc_target_hwnd != our_present_hwnd` prevents a capture-feedback loop. WDA on our window
(`PresentSurface.cpp:282`, `CaptureAffinity::ExcludeFromCapture` default) is **kept** — load-bearing here to
keep a monitor/desktop capture (OBS, our own loop) from grabbing our output and feeding it back.

## §4 — Capture-continuity handling

The game keeps rendering behind us (Composed Flip) → WGC capture continues; the captured frame remains the
interpolation input. The `bridge_present_src` blit/keyed-mutex/`submit()` chain (`main.cpp:7015-7033`,
`bridge_present` at `:6943-6950`/`:7663`/`:7727`) is unchanged — only the destination plane differs.
**A1 open item (MASTER_PLAN §7):** measure whether WGC capture rate of a now-Composed game changes the FG
input cadence (the design-workflow notes WGC reliably tracks source rate only on Win11 24H2); if it collapses
below a floor, degrade to FG-OFF passthrough (a measured per-game outcome, RISK OA-5).

## §5 — Default-off / byte-identical discipline

- `--present-own-window` default-off → `psd.style` stays `DcompCt` → **byte-identical** to today's overlay
  (the existing default-off precedent of `--present-waitable`/`--present-sync`/`--present-colorspace`,
  `PresentSurface.hpp:74-87`, `main.cpp:6923-6924`). Verify on an unchanged HSR run (the C8 regression gate).
- `dxgi_live` is **always-on but identity-on-success** (the `vk_live` discipline — the happy path is
  unchanged; only an actual `DEVICE_REMOVED`/`MODE_CHANGED` trips it). Verify byte-identical on an HSR run.
- `Style::OwnWindow` is reachable ONLY via the new flag; `DcompCt`/`Dcomp`/`Baseline` paths are untouched.

## §6 — Build & validation gates

- Build: `vcvars64` + `cmake --build build-ra-msvc2` (the established RA build).
- `lint_hal` clean (default `seq_cst` in `apps/`, no raw `memory_order_*` — the `yielded` atomic uses the
  default load/store or an explicit acquire/release with justification).
- Doc-sync: changing `PresentSurface.hpp`'s public surface (the new `Style` value) requires updating
  `docs/reference/present.md` in the same commit (the `check_doc_sync` CI job).
- The C8 FG-quality regression gate green (byte-identical-off proof).
- **Runtime (the operator's eye + the rig probes of MASTER_PLAN §7):** A1 measures pacing/MASD/IF-vs-capture;
  the no-lock-out invariant is exercised first-hand (alt-tab/minimize/exit/forced-crash all return the
  display) before OA-2/OA-3 flip from `open` to `mitigated`.

## §7 — Staging order (gated by the RISK register)

**A1** (OwnWindow + foreground-yield + watchdog, flag default-off) → measure pacing + MASD + IF-vs-capture →
**A2** (`dxgi_live`, **required to commit** — OA-1) → exercise no-lock-out first-hand (OA-2/OA-3/OA-10) →
only THEN consider an eye/measure gate for a possible default flip. **A3** (exclusive fullscreen) and **A4**
(two-monitor split / D2 substrate) are deferred. No commit while any of OA-1/OA-2/OA-3 is `open`.

**Author's flags (must be built + verified before claiming `mitigated`):** the `ResizeBuffers` cold-path
helper on `Impl` does not yet exist; the foreground-yield monitor is new code; the watchdog stall-threshold
`N` is unchosen. None of these is built — the RISK register seeds them `open` with the mitigation-as-code
specified, flipping to `mitigated` only after build-green + the named first-hand runtime check is recorded.

*Made with my soul — the supervisor, for Swately.*
