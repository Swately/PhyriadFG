# FG_PRESENT_PACING_DESIGN — closing the pacing gap (the present-architecture axis)

> **Diátaxis type:** Planning (design) + Explanation. **Normativity:** BCP 14. **Status:** `designed`
> (research-grounded; the verdict is architectural). **Audience:** the operator (a product decision)
> + the next implementer.
>
> **Evidence base:** the measured gap is [`FG_LSFG_HEADTOHEAD_MEASURED.md`](FG_LSFG_HEADTOHEAD_MEASURED.md)
> §1/§5 (our MASD 12.7 ms vs LSFG's 0.026 ms; we present "Composed: Flip", LSFG "Hardware Composed:
> Independent Flip"). The architecture analysis is the deep-research workflow `wq1srry60` (100 agents,
> 23/25 claims confirmed against **Microsoft primary docs** — `comp-swapchain`, `dxgi-flip-model`,
> `multiplane-overlay-hardware-requirements`, `displayable-surfaces`, `reduce-latency-with-dxgi-1-3`;
> NVIDIA Streamline + AMD GPUOpen for DLSS-G/FSR3).

## §1 — The verdict: the pacing gap is ARCHITECTURAL, not a tuning bug

The measured MASD ~490× gap is **structural to our being an overlay**, not a parameter we left wrong:

- **LSFG presents its OWN borderless-fullscreen flip-model swapchain → it BECOMES the displayed
  window → it qualifies for Hardware Independent Flip** (a dedicated GPU scanout plane / DWM bypass).
  That is the source of its near-zero pacing variance + ~1-frame latency. (The underlying game then
  reports "Composed Flip" because LSFG, not the game, is the displayed surface — community PresentMon
  evidence; THS's own docs were not located first-hand.)
- **DLSS-G / FSR3 sidestep it entirely by being IN-ENGINE:** they proxy/replace the game's OWN
  swapchain (DLSS-G intercepts `IDXGISwapChain::Present`; FSR3 substitutes a `FrameInterpolationSwapChain`
  implementing `IDXGISwapChain4` with its own high-priority busy-wait pacing thread, present-interval
  0), so interpolated frames travel the game's native present path → Independent Flip.
- **We are a premultiplied-alpha DirectComposition overlay composited OVER an arbitrary unmodified
  game window** (`PresentSurface.cpp`: `CreateSwapChainForComposition` + `IDCompositionVisual`,
  `Present(0,0)`). Microsoft documents this as **"Composition" — the default and *least-efficient*
  present path** (DWM re-renders the present into its own backbuffer; +≥1 frame; "graceful fallback
  to the minimum possible" *not* the Independent-Flip 1-frame minimum). **"Surfaces composited over
  non-displayable arbitrary game windows will never get the benefits of independent flip."**

**So as an overlay-over-the-game we are STRUCTURALLY locked to the composed path.** The MASD gap is
not closeable by tuning; it is the price of the overlay topology.

## §2 — The three options (the product decision)

| option | pacing | what it costs | tier |
|---|---|---|---|
| **A. Own fullscreen window** (the LSFG model) | **Independent Flip — LSFG-class** | we BECOME the displayed window (re-composite the captured game + our FG frames into our own fullscreen flip-model swapchain); focus/Z-order/HDR/exclusive-fullscreen-game limits | **T2**, architectural |
| **B. Overlay jitter-reduction** (stay composed) | composed floor — **partial**, never LSFG-class | byte-identical-off flags; bounded gain | T1 |
| **C. Displayable-surface middle path** (MPO promotion) | iflip IF promoted | **fragile** — WDDM 3.0+ / Win11 + a free MPO plane; "over a foreign window → never iflip"; plane scarcity makes non-promotion the common outcome | T2, low-confidence |

- **A — Own fullscreen window — the only ROBUST route to LSFG-class pacing.** We stop being an
  overlay: we present our own borderless-fullscreen flip-model swapchain (the existing `Style::Baseline`
  is the seed — `CreateSwapChainForHwnd` + `FLIP_DISCARD`), and we composite the captured game frame +
  our interpolated frames INTO it (we already HAVE the captured game frame — we capture it). We become
  the displayed window; the game renders behind us (reports Composed Flip, like under LSFG). **Trade-offs
  (the operator's call):** we own focus/Z-order; HDR passthrough must be handled; an exclusive-fullscreen
  game can't be overlaid this way (must be borderless — same constraint LSFG has). This is a real new
  present MODE (`Style::OwnWindow` / a `--present-own-window` path), T2 (it changes the displayed
  surface + the focus model → needs a RISK_REGISTER).
- **B — Overlay jitter-reduction — the cheap PARTIAL win (byte-identical-off).** Stays composed (never
  LSFG-class) but lowers our composed MASD toward the composed floor:
  - **Waitable swapchain** (VERIFIED first-hand, MS docs): create with
    `DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT`, `GetFrameLatencyWaitableObject` +
    `WaitForSingleObjectEx` before each frame, `SetMaximumFrameLatency(1)` → render on current data,
    queue when the system is ready → minimizes the ~1-frame composed wait + aligns to readiness.
  - **Stop over-presenting** (we measured 256–284 fps to a 240 Hz panel): cap our present rate to the
    refresh (sync-interval / a present-rate cap) so DWM isn't dropping 42% of our frames. *(Best-practice;
    not first-hand-verified in the sweep — measure the MASD delta before claiming it.)*
  - **Pace presents to the DWM vblank** via `DwmGetCompositionTimingInfo` (`qpcVBlank`/`qpcRefreshPeriod`)
    — align our present to the compositor's clock. *(Best-practice; measure.)*
- **C — Displayable-surface middle path — DEFER.** Genuine on paper (`D3D11_RESOURCE_MISC_SHARED_DISPLAYABLE`)
  but the docs lean to "over a foreign window → never iflip", and MPO planes are scarce/driver-optional.
  Low-confidence for an overlay-over-an-arbitrary-game; revisit only if A is rejected and B is insufficient.

## §3 — Recommendation (honest, sequenced)

1. **Bank the COST win first** (`--flow-scale 2`, the other gap) — it is the bigger felt problem (the
   saturation collapse) and it is already a flag (eye-gate the quality, flip the default). Pacing is the
   *second* gap.
2. **Build B (overlay jitter-reduction) as the cheap partial step** — waitable swapchain + stop
   over-presenting, byte-identical-off flags, MEASURE the MASD delta (the research is explicit these
   help *within* composition but cannot reach Independent-Flip; do not over-claim). This is the
   autonomous-safe increment.
3. **Put A (own-window mode) to the operator as a PRODUCT DECISION** — it is the only route to LSFG-class
   pacing, but it changes what PhyriadFG *is* (the displayed window, not a passive overlay). It is a T2
   architectural arc (a RISK_REGISTER, the focus/HDR/exclusive-fullscreen handling). **Honest framing:
   the pacing gap cannot be closed while we insist on being an overlay; LSFG "wins" pacing by not being
   one.** Whether to adopt its model is a deliberate trade (the overlay's flexibility — compose over any
   window, click-through, capture-proof — vs LSFG-class pacing).

## §4 — Open questions (carried from the research)

- Can an overlay-over-an-UNMODIFIED-foreign-window ever get its own surface promoted to iflip in
  practice on Win11/WDDM 3.0 (the displayable-surface rule leans "no")? — a rig probe
  (`IDXGIOutput6::CheckHardwareCompositionSupport`) would settle it.
- What is the achievable composed-path MASD floor (option B) vs LSFG's 0.026 ms — i.e., how much of the
  12.7 ms is recoverable without the architecture change? — MEASURE after building B.
- The own-window mode's exact focus/Z-order/HDR/exclusive-fullscreen limits on a real game (option A).

*Made with my soul — the supervisor, for Swately.*
