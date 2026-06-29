# FG_OPTION_A_MASTER_PLAN — the own-window Independent-Flip present that paces the game

> **Diátaxis type:** Planning (design / master plan). **Normativity:** BCP 14 (MUST/SHOULD/MAY).
> **Status:** `designed` — NOT built. Every claim about PhyriadFG's *future* behaviour is a hypothesis to
> be MEASURED; only the cited code refs and the LSFG-side measurements are `measured`/`shipping`.
> **Tier:** **T2** (crash-class — we come to own the displayed scanout plane; a dogma — the no-lock-out
> invariant — is at stake). Per [`canon/PLAN_TIER_PROTOCOL.md`](../canon/PLAN_TIER_PROTOCOL.md) this plan
> is one leg of a **mutually-linked triad**:
> - [`FG_OPTION_A_IMPLEMENTATION_STRATEGIES.md`](FG_OPTION_A_IMPLEMENTATION_STRATEGIES.md) — the build steps.
> - [`FG_OPTION_A_RISK_REGISTER.md`](FG_OPTION_A_RISK_REGISTER.md) — the failure modes + mitigation-as-code.
>   **No risk may remain `open` at commit; no commit while any is open.**
>
> **Evidence base (the single-sources):** [`FG_LSFG_HEADTOHEAD_MEASURED.md`](FG_LSFG_HEADTOHEAD_MEASURED.md)
> (the measured gaps + the mechanism control) · [`FG_PRESENT_PACING_DESIGN.md`](FG_PRESENT_PACING_DESIGN.md)
> (the structural verdict + options A/B/C). This plan productizes option A.

## §1 — Why (the confirmed mechanism)

The C7 head-to-head reduced PhyriadFG's deficit to two measured gaps: **cost** (closed by `--flow-scale 2`)
and **pacing** (MASD 12.7 ms vs LSFG 0.026 — `FG_LSFG_HEADTOHEAD_MEASURED.md` §1). The 2026-06-23
operator session **nailed the pacing mechanism first-hand**, single-GPU on the 4090, LSFG Scaling OFF:

- The game's **real fps drops ~20** (BF6 ~110-120 → **90-100 base**, read off the LSFG overlay).
- The 4090 **un-pegs**: NVML A/B **546 W / 98 % SM / 42 % mem → 503 W / 94 % / 36 %** (−8 % power, −14 % mem-bw).
- The freed budget pays for cheap interpolation → 240 smooth presented frames.

**It is back-pressure, not a cap and not downscaling.** The decisive control (`FG_LSFG_HEADTOHEAD_MEASURED.md`
§3): the game's **per-frame GPU cost is identical** with LSFG on/off (2.97 vs 3.07 ms). The game is not
drawing cheaper frames and is not being capped — its `Present()` simply *returns later*. The chain, grounded
in Microsoft DXGI primary docs:

1. A flip-model `Present()` blocks the game's render thread once its DXGI present queue fills (default
   Maximum Frame Latency = 3). The queue drains only as a presented buffer is **released by its consumer**.
2. When a tool presents its **own borderless-fullscreen flip swapchain**, it **becomes the displayed window**
   and takes **Hardware Independent Flip**; by the singular-scanout-plane rule the **game is demoted to
   "Composed: Flip."**
3. In Composed mode the game's buffers release at the DWM composition cadence (vsync-gated, slower than the
   prior uncapped flip rate) → `Present()` returns later → the render loop paces down → the GPU un-pegs.

Our current presenter **cannot** do this. `PresentSurface` today is a **transparent composition overlay**
(`Style::DcompCt`: `CreateSwapChainForComposition` + premultiplied alpha + `WS_EX_LAYERED|WS_EX_TRANSPARENT`,
`framework/render/present/src/PresentSurface.cpp:225-242, 181-187`) — it never becomes the opaque displayed
surface, so it does not demote the game to a paced Composed path. The operator's lived experience (BF6
saturated → the game stays pegged → our FG multiplier collapses) is consistent with **our overlay not pacing
the game at all.** Closing the pacing gap therefore is not tunable; it requires becoming the displayed
window — **option A.**

## §2 — The contract ("done")

A new **default-off** present mode that makes PhyriadFG present its own borderless-fullscreen flip-model
window (the displayed Independent-Flip surface) over the captured game, such that, on a non-kernel-AC title
on this rig:

1. **Pacing measured:** the game's presented rate paces down and the 4090 un-pegs (the
   `FG_LSFG_HEADTOHEAD_MEASURED.md` §3 signature: util/power/mem-bw drop) **with the game's per-frame GPU
   cost unchanged** — proving pacing, not cap, not starvation.
2. **MASD measured** at the Independent-Flip floor for our presenter (target: LSFG-class; LSFG = 0.026 ms),
   on a title where PresentMon ETW works (BF6's Javelin blocks ETW — `FG_LSFG_HEADTOHEAD_MEASURED.md` §4.2).
3. **The no-lock-out invariant holds first-hand:** alt-tab, minimize, focus-loss, exit, and a forced crash
   all return the user's native display within bounded time (RISK OA-2/OA-3/OA-10).
4. **Byte-identical-off:** with the flag absent, `psd.style` stays `DcompCt` and the present path is
   unchanged (the existing default-off discipline of `--present-waitable`/`--present-sync`, verified on an
   unchanged HSR run).

## §3 — Architecture

PhyriadFG already **captures** the game (WGC) and **interpolates**; today it composites that into a DComp
overlay. Option A redirects the **same** composited output (captured-game + interpolated frames) into our
**own flip-model HWND swapchain that owns the scanout plane.**

**The flip path already exists.** `Style::Baseline` (`PresentSurface.cpp:218-224`) is exactly the swapchain
we want — `CreateSwapChainForHwnd` + `DXGI_SWAP_EFFECT_FLIP_DISCARD` + `DXGI_ALPHA_MODE_IGNORE` +
`DXGI_SCALING_NONE` — an opaque flip-model HWND swapchain. Its window recipe (`:181-194`) is
`WS_POPUP | WS_EX_TOPMOST | WS_EX_NOACTIVATE`, monitor-extent, `SW_SHOWNOACTIVATE`.

**Design decision (corrected from the design-workflow's first draft, verified first-hand):** the recipe
**MUST stay NON-ACTIVATING** (`WS_EX_NOACTIVATE`). A frame-gen presenter must never steal the game's input
focus — the user is *playing the game*; keyboard/mouse MUST keep flowing to the game window. This is also
LSFG's posture. The workflow-draft's suggestion to "make the window activatable so it gets WM_ACTIVATE/
WM_KILLFOCUS" is **rejected**: it would steal game input. Consequence: our window will **not** receive its
own focus-loss messages, so the alt-tab/minimize **yield contract MUST be driven by monitoring the
foreground window** (the game), not by our own `WM_KILLFOCUS` (RISK OA-2). This is the load-bearing new code.

Option A is therefore best expressed as a **new `Style::OwnWindow`** = Baseline's opaque flip swapchain
(`PresentSurface.cpp:218-224`, the `style != Baseline` DComp gate at `:225` extended to also skip
`OwnWindow`) + a recipe that keeps `WS_EX_NOACTIVATE`, drops the click-through `WS_EX_LAYERED|WS_EX_TRANSPARENT`,
and is wired to a foreground-yield + watchdog contract. The hot path (`submit()`, `:290-328`) is unchanged
save one early-return on a `yielded` flag.

## §4 — The crux tension (the honest #1 open question)

On a **single display**, becoming the displayed Independent-Flip surface AND keeping a reliable WGC capture
of the game may be **mutually constraining**, and this is the design's central uncertainty (design-workflow
research, MS docs + the `ForceComposedFlip` prior art — marked `inferred`, MUST be rig-probed):

- WGC captures the game by reading its DWM **composition/redirection surface**. This works **only while the
  game's frames flow through DWM composition** — i.e. while the game is in Composed Flip. Our opaque topmost
  window **forces the game into Composed Flip** (good — keeps it capturable, and is the pacing trigger).
- BUT when **our** window takes **true** Independent Flip, DWM "steps back" for our plane and may stop
  compositing the fully-occluded game → the game's redirection surface could stop updating → **our capture
  of the game could stall.** Whether 24H2 DWM keeps compositing an occluded captured window is hardware/
  driver-dependent and undocumented.

**Honest consequence for scope:**
- **The PACING (game demoted → un-peg → the saturation/latency relief, the operator's MAIN want) depends
  only on our opaque window OCCLUDING the game, not on our own window reaching true IF.** It is the higher-
  confidence win.
- **Our-own-Independent-Flip (the last present-latency increment for OUR output) is the uncertain part on
  same-display** — we may have to ACCEPT our presenter staying in Composed Flip (a +1-vblank tax) to keep
  the game capturable. We still get the game's pacing/un-peg; we may not get the full LSFG-class MASD on the
  same display.
- **The two-monitor split gets BOTH cleanly:** capture the game on monitor A (Composed, WGC reliable),
  present Independent-Flip on monitor B. This is the documented way to have both — and it is the natural
  substrate for the future **D2 "independent FG instances per screen"** objective (the headline LSFG can't
  follow). The monitor binding MUST be per-instance, not a process global, so D2 inherits it cleanly
  (RISK OA-8).

## §5 — Honest scope (what Option A does and does NOT promise)

**Does (the bets):**
- Reproduce LSFG's pacing/back-pressure **without injecting into the game** — we own only our own present;
  we set neither the game's MaximumFrameLatency, sync interval, nor swap effect (those live inside the
  game's swapchain and are unreachable without injection). The pacing is induced *indirectly* by owning the
  plane (mechanism a) and by our bounded capture/consumption rate (mechanism d).
- Relieve the saturation-collapse at its cause (the game un-pegs → headroom for the FG slice → no multiplier
  collapse) — the dogma-clean equivalent of LSFG's headroom, with NO game cap.

**Does NOT:**
- **No 100 % LSFG match.** The **external-capture pipeline-depth floor remains** (we capture an
  already-presented game frame; a fixed capture→interpolate→present latency is structural). LSFG's
  input-to-photon edge is partly its own consumer-side levers we can only partially mirror.
- **No input-to-photon claim.** `MsAllInputToPhotonLatency` is **unmeasurable** for external-capture FG via
  PresentMon (`FG_LSFG_HEADTOHEAD_MEASURED.md` §4 — empty for LSFG *and* us). Pacing (MASD) is the FG-agnostic
  metric; latency parity is neither promised nor the target.
- **No game cap.** Pacing-by-present-ownership is NOT a user-facing fps cap and is NOT a per-frame downscale
  — the §3 control (identical per-frame cost) is the proof. Satisfies the no-cap / no-recommend-cap dogma.
- **No exclusive-fullscreen game support in v1.** An exclusive-fullscreen game cannot be overlaid this way —
  the same constraint LSFG carries; detect + degrade (RISK OA-6), never fight for the mode.
- **No same-display IF guarantee** — see §4; the MASD floor on same-display is an open measurement.

## §6 — Staged plan

- **Stage 0 — bank the COST win.** `--flow-scale 2` (the larger felt gap; already a flag) is independent and
  ships first. Not part of this arc; noted for sequencing.
- **Stage A1 — borderless-FSO own-window (`--present-own-window`, default-off).** New `Style::OwnWindow`;
  the foreground-yield + watchdog contract (RISK OA-2/OA-10); prefer borderless-FSO, never exclusive
  (RISK OA-6). **Measure:** the back-pressure (game fps down, 4090 un-peg, per-frame cost unchanged), the
  MASD, and whether our presenter reaches IF vs stays Composed while capture survives (§4).
- **Stage A2 — device-loss hardening (`dxgi_live`).** Promote `DEVICE_LOST_RECOVERY` FR1 from `accepted` to
  `mitigated` for this mode (RISK OA-1) — **required to commit** (we now own the DXGI present that takes the
  device-loss hit at `PresentSurface.cpp:325`).
- **Stage A3 — exclusive fullscreen.** DEFERRED, out of v1 scope. Only if borderless-FSO pacing proves
  insufficient; carries the `SetFullscreenState`/mode-restore crash surface (RISK OA-3 layer 3).
- **Stage A4 (future) — the two-monitor split** for guaranteed IF + capture (and the D2 substrate).

## §7 — Open measurements before any number is written as `measured`

1. **Does Option A pace the game where the overlay does not?** (the core bet — §1/§4). The BF6 deltas in §1
   are **LSFG's** measured effect; our own-window reproducing them is the A1 hypothesis, NOT a result.
2. **The MASD floor** our own-window reaches on this rig (LSFG-class is the design target, not a measurement).
3. **Same-display IF-vs-capture** (§4): does our presenter reach IF while the game stays WGC-capturable?
4. **WGC source-rate of a now-Composed game** (RISK OA-5): does demoting the game change our FG input cadence?

BF6 is kernel-AC and blocks PresentMon ETW (`FG_LSFG_HEADTOHEAD_MEASURED.md` §4.2): MASD MUST be measured on
HSR/Hogwarts; the BF6 un-peg is NVML-only. No pacing/back-pressure number is `measured` until captured.

## §7.1 — Partial result (2026-06-23, NVML first-hand, BF6 idle-but-saturated)

Measured our **current overlay** under BF6 saturation (idle keeps the 4090 at ~98 % — the operator confirmed
the edge holds without combat), single-GPU, vs the BF6-alone baseline:

| 4090 | BF6 alone | + our overlay (flow-scale 1) | + our overlay (flow-scale 2) |
|---|---|---|---|
| SM util | ~98 % | **99 %** (NO un-peg) | **99 %** (NO un-peg) |
| power | ~570 W | ~492 W (−14 %) | ~493 W (−14 %) |
| mem-bw | ~42 % | ~34 % | ~34 % |
| MsAddedLat (`--csv`) | — | 124.5 ms | 122.9 ms |
| warp_ms / iter_ms | — | 5.91 / 11.74 | 5.89 / 11.28 |

**Two grounded conclusions:**
1. **Our overlay does NOT create headroom (util stays pegged 99 %), but it DOES displace game work** (power
   −14 %, mem −14 %). So the overlay *competes* (the game renders somewhat less, the LSFG-class power/mem
   drop) yet **refills the freed util by running flat-out** — where LSFG *un-pegs* to 94 % by pacing to a
   target and stopping. **→ the un-peg (the latency-relieving headroom) needs BOTH (a) pacing the game
   (Option A / present-ownership) AND (b) pacing OUR output to a target** (the saturation-stability STEP 2
   controller, `FG_SATURATION_STABILITY_*`) so our own FG does not refill the headroom. Option A alone is
   necessary but **not sufficient** for the un-peg — this REFINES OA-7. *(Still unconfirmed: that Option A's
   opaque own-window paces the game MORE than the transparent overlay already does — the A1 bet.)*
2. **On heavy-frame games (BF6 4K) the WARP dominates the slice (5.9 ms); `--flow-scale 2` barely helps**
   (122.9 vs 124.5 ms — it only cheapens the flow, a small fraction here), unlike light HSR where it closed
   the cost to parity. **→ the cost lever for heavy-frame saturation is WARP-scaling** (process the warp at
   reduced resolution + upscale the output — the operator's "our own scaling" idea, here data-justified as a
   DISTINCT lever from flow-scale). A candidate single-GPU sub-objective, operator-gated, tracked separately.

## §7.2 — The perfect-partition analysis (the operator's "zero-waste" thesis, honestly bounded)

The operator's objective: run the FG at OUR max within saturation + leave the game its needed share, **zero
wasted free performance** ("lo perfecto no busca desperdiciar, busca aprovechar"). Deep analysis (workflow
`wai3s5qvx`: web + queueing theory + our code, adversarially verified). **Honest verdict:**

**On ONE graphics engine, "zero-waste AND low-latency" is an ASYMPTOTE, not reachable.** A single GPU is a
single-server queue; as utilization ρ→100 % the latency tail explodes (ρ/(1−ρ), M/G/1). "Zero idle" (ρ=1)
and "low stable latency" (ρ bounded below 1) are antipodal limits of the SAME ρ — approached, never both
reached. **The headroom LSFG leaves is the NECESSARY price of low latency, not waste.** Consumer NVIDIA
cannot rescue it: queue priorities are ignored for warp arbitration (NVIDIA's own words: "no meaningful
effect" — verified our queues are all prio 1.0f at `main.cpp:1957`), preemption is whole-kernel (can't
protect the game's per-frame critical path), async-compute shares the same shader cores (no free capacity).
Filling the idle bubbles steals from the game → re-adds latency (base-render starvation).

**This CORRECTS §7.1's conclusion-(b) framing AND the supervisor's over-concession to the operator (2026-06-23):
leaving the game its deadline headroom is RIGHT for single-GPU — it is not "waste to fill."** The operator's
instinct is honored by THREE distinct levers (NOT by filling the headroom with more frames):

1. **MINIMIZE the FG slice (the dominant single-GPU lever).** The headroom's SIZE = our FG footprint; cheapen
   the slice → the necessary headroom shrinks toward zero → minimal waste. Measured: flow-scale 3.36→0.94 ms;
   on heavy frames the WARP dominates (§7.1) → **warp-scaling** is the next slice-minimizer. "Don't waste" =
   "make our footprint tiny," NOT "fill the headroom."
2. **The novel QUALITY-in-headroom niche (genuinely unoccupied — every shipping FG goes LIGHTER, none fills
   the freed budget).** On a PACED game (Option A), the budget above the game's responsiveness floor CAN go
   to a HEAVIER/BETTER pass (more candidates, neural-flow on the 4090 tensor cores) for QUALITY — bounded by
   a **break-even gate** (`framework/gpu break_even_decide`; the moment it starves the base render, latency
   regresses — FSR3's "render below half output, else disable FG" IS this boundary). The operator's "use the
   free performance" instinct, correctly scoped: quality, bounded by the game's deadline.
3. **The CLEAN zero-waste = the SECOND GPU** (the roadmap's gpu+igpu / n-gpu phase). Offload the FG → the
   4090 is 100 % FOR THE GAME (untouched), the FG GPU does the FG → zero waste ACROSS the system, no
   single-engine wall. **The single-GPU case is fundamentally bounded by the queueing wall; multi-GPU is the
   structural realization of the operator's zero-waste vision** — exactly why the multi-GPU thesis exists.

**So Option A's role is precise: pace the game to its responsiveness threshold (create the NECESSARY, minimal
headroom) — neither leaving LSFG-sized waste nor filling it with more frames.** Slice-minimization
(warp-scaling) + the bounded quality-fill + the multi-GPU offload are the three honest expressions of
"aprovechar, no desperdiciar."

*Made with my soul — the supervisor, for Swately.*
