#pragma once
// PhyriadFG cli/ layer: the master Config struct + its enums, the cli compile-time
// constants, and the print_help/parse_args declarations. Bodies live in cli.cpp;
// main.cpp #includes this.
#include <cstdint>
#include <cstddef>
#include <string>

// ─── Config ──────────────────────────────────────────────────────────────────
enum CaptureApi { CA_DD, CA_WGC };
enum FgGpu     { FG_AUTO, FG_PRIMARY, FG_ASSIST };    // FG routing target
enum ConvertGpu { CG_IGPU, CG_PRIMARY };               // where to run convert+pack
enum OutputClock { OC_TIMER };       // the timer is the only output clock
struct Config {
    int   cap_mon=0, pres_mon=-1;
    float res_ceil=32.f, conf_improv=0.20f, agreement=0.05f;
    int   fg_factor=2;
    bool  no_upscale=false, lanczos=false;
    char  assist_gpu[128]={};
    bool  list_only=false;
    bool  pin_threads=true;         // --pin: pin C/F/P threads to fixed cores (P RT-elevated) via the topology
                                    // pillar. DEFAULT ON; --no-pin = the OS-scheduled bare threads (byte-identical).
                                    // A scalability tool for hybrid CPUs (avoid E-core landings) / NUMA; it does NOT
                                    // fix the >monitor-refresh present slip (that is GPU/fence-budget-bound).
    int   pin_test=0;               // --pin-test N: thread-protection policy selector, READ ONLY inside the
                                    // if(cfg.pin_threads) paths → fully inert without --pin (byte-identical;
                                    // --pin-test 0 == plain --pin). 0=FULL (pin C/F/P + elevate P TIME_CRITICAL +
                                    // elevate F HIGHEST); 1=NO-FLOW-RT (pin C/F/P + elevate P only; F pinned, not
                                    // elevated); 2=PRIO-ONLY (elevate P+F; no pin/affinity); 3=AFFINITY-ONLY (pin
                                    // C/F/P; no elevation); 4=NEITHER (no pin, no elevate = same as no --pin);
                                    // 5=MMCSS-COMPOSITE (P hard-pin + MMCSS 'Pro Audio'/CRITICAL; F+C soft-affinity
                                    // via set_thread_ideal_processor, migratable + MMCSS 'Capture' HIGH/NORMAL; each
                                    // thread falls back to elevate_thread_rt when its MMCSS token is inactive).
    bool  pin_test_set=false;       // --pin-test was given EXPLICITLY. Distinguishes an explicit `--pin-test 0`
                                    // (the FULL sanity row) from the default 0, so fg_protect's mode-5 promotion
                                    // honours the explicit choice instead of overriding it.
    bool  fg_protect=true;          // --fg-protect: the composed first-class flag for the combined GPU99%+CPU100%
                                    // FG collapse. Composes (REUSES): (a) pin_threads=true + pin_test=5 (drives
                                    // C/F/P down the mode-5 MMCSS-composite policy; honours an explicit --pin-test);
                                    // (b) async_present=true (the latency/smoothness backstop — slot-1 provisioned at
                                    // init, present decouples + drops the interpolated frame); (c) the GAME_FLOOR
                                    // core-reservation = the no-game-cap dogma AS CODE (GAME_FLOOR=max(4,p_cores/2);
                                    // hard_pins <= p_cores-GAME_FLOOR else P demotes to a soft ideal-processor hint;
                                    // the game always keeps >= half the P-cores; we never set the game's
                                    // affinity/priority). INERT on >= 8-P-core rigs (P stays hard-pinned); protects
                                    // <= 4-P-core CPUs. DEFAULT ON; --no-fg-protect disables. For the default case
                                    // the composed pieces are replicated in the post-parse block of parse_args.
    bool  force_single_gpu=false;   // --force-single-gpu: drive the single-GPU degenerate path on a multi-GPU rig
                                    // (suppress pB+pG → all FG roles on device A). DEFAULT OFF (byte-identical;
                                    // single_gpu only derives true with this flag OR no 2nd discrete GPU present).
    bool  cap_route_probe=false;    // --cap-route-probe: a break-even-style capability routing DECISION over the FG's
                                    // app-local VDevs (has_fp16/has_dp4a/fp16_gflops) + a vendor-named capability
                                    // manifest at startup. MEASUREMENT/DIAGNOSTIC ONLY — the fixed A/B/G roles make
                                    // it INERT (A=primary/present, B=flow/gme, G=convert are architecture-forced; the
                                    // offload AI≈2.5 ≪ crossover ⇒ decline always). DEFAULT OFF, byte-identical (it
                                    // only prints; it changes no A/B/G behaviour).
    bool  present_waitable=false;   // --present-waitable: waitable swapchain (SetMaximumFrameLatency(1) + wait-before-
                                    // present) = composed-overlay jitter reduction (partial; does not reach
                                    // Independent Flip). DEFAULT OFF (byte-identical).
    uint32_t present_sync=0;        // --present-sync N: present sync interval (0 = present-immediately; 1 = pace to
                                    // the compositor, stops over-presenting). DEFAULT 0 (byte-identical).
    uint32_t present_colorspace=0;  // --present-colorspace srgb: declare the overlay colorspace (sRGB) so an HDR
                                    // desktop composites the SDR overlay without washout. 0 = no-op. DEFAULT 0
                                    // (byte-identical).
    uint32_t present_format=0;      // --present-fp16 / --hdr: the present surface format. 0 = the 8-bit BGRA8
                                    // swapchain + BGRA8 bridge (the default present path). 1 = an FP16 scRGB swapchain
                                    // (R16G16B16A16_FLOAT + G10_NONE_P709) AND a matching FP16 bridge texture — the
                                    // producer/consumer format MUST agree or the CopyResource into the backbuffer is
                                    // a format mismatch. Soft: the surface falls back to BGRA8 if FP16 create fails; a
                                    // post-create disagreement is a named clean quit (no in-thread rebuild). INERT
                                    // without an HDR display + HDR content. DEFAULT 0 (byte-identical).
    bool  present_own_window=false; // --present-own-window: present our OWN opaque borderless flip-model HWND swapchain
                                    // (Style::OwnWindow) instead of the DComp composition overlay → we become the
                                    // displayed Independent-Flip plane → demote the game to Composed → pace it down.
                                    // Click-through (WS_EX_TRANSPARENT, input still reaches the game) + non-activating
                                    // + the no-lock-out contract (foreground-yield + watchdog + dxgi_live device-loss
                                    // exit). DEFAULT OFF → psd.style stays DcompCt (byte-identical).
    bool  indicator=false;          // --indicator: a teal corner mark showing PhyriadFG is active. WIP /
                                    // DEFAULT OFF: the in-frame stamp on the WAP warp path faults (DEVICE_LOST — that
                                    // path is crash-sensitive). The safe form (warp shader write, or a separate
                                    // layered click-through window) is pending; the cfg + setup + passthrough copy
                                    // remain as the scaffold.
    bool  fps_overlay=false;        // --fps-overlay: the live "in->out" fps overlay (captured fps -> presented fps)
                                    // drawn TOP-LEFT on Apresent (RGBA8) by a compute RMW INSIDE bridge_present_src —
                                    // the safe present path (NOT the WAP warp path, which faults DEVICE_LOST). DEFAULT
                                    // OFF → no overlay barrier/dispatch + Apresent created as usual (byte-identical).
    CaptureApi capture_api=CA_DD;
    char  window_substr[256]={};  // non-empty → capture the MONITOR that window is on, via DDA (default; resolved in
                                  // main()→cap_mon). WGC window-only ONLY with --capture-api wgc.
    bool  dpi_probe=false;        // --dpi-probe: read-only diagnostic — logs GetClientRect / GetDpiForWindow /
                                  // cap_item.Size() / first frame.ContentSize() to diagnose the high-DPI capture-crop.
                                  // Default off → byte-identical.
    FgGpu    fg_gpu=FG_AUTO;      // FG routing (auto|primary|assist)
    ConvertGpu convert_gpu=CG_IGPU; // iGPU fused convert+pack when available
    bool  igpu_field=true;          // --igpu-field: iGPU image-derived contour field on G.q2 (2nd dispatch after
                                    // convert). DEFAULT ON; --no-igpu-field disables. Needs the iGPU convert path
                                    // (auto-disabled if absent); a cascade dependency for bg_snap/band_xfade.
    bool  igpu_field_verify=false;  // --igpu-field-verify: CPU Sobel oracle vs the GPU field (the byte gate). Implies
                                    // --igpu-field.
    uint32_t igpu_field_thr=24;     // occ_class edge-band threshold (Sobel magnitude 0..255).
    bool  afill=false;              // --afill: A reads the iGPU field + tints the contour band onto wapOutA in-place
                                    // (the field VISUALIZER, to eye-validate iGPU↔boundary alignment). DEFAULT OFF
                                    // (byte-identical off; auto-enables igpu_field since A needs the field).
    float afill_strength=0.5f;      // --afill-strength: tint visibility weight (0 = byte-identical).
    float afill_edge_norm=0.04f;    // --afill-edge-norm: contour-distance→[0,1] normalizer (~1/edge_thr).
    float afill_mv_gate=6.0f;       // --afill-mv-gate: gate STILL pixels — |MV|(px) at which the tint fades to 0; the
                                    // contour persists in still zones under motion. 0 = no gate (full contour).
                                    // DEFAULT 6. Affects only the --afill visualizer.
    bool  bg_snap=true;             // --bg-snap: the warp READS the iGPU contour field (binding 11) and at the boundary
                                    // band snaps BACKGROUND-side MVs to the gme model → kills the optical-flow
                                    // disocclusion "gravity". DEFAULT ON; --no-bg-snap disables (auto-enables
                                    // igpu_field; needs the iGPU convert path + gme).
    float bg_snap_strength=1.0f;    // --bg-snap-strength: the snap weight scale, parse-clamped [0,4] (1=soft, 2-4=
                                    // progressively hard; the shader clamps w=strength·band·bg to [0,1] so >1
                                    // saturates the band core).
    float bg_snap_norm=0.04f;       // --bg-snap-norm: contour-distance→[0,1] band normalizer (~1/edge_thr).
    bool  vblend=true;              // --vblend: velocity-continuity warp — near a pair's END the warp tilts the
                                    // effective sample-offset MV toward the NEXT pair's MV (free in the F→P ring) so
                                    // the boundary velocity transitions smoothly instead of stepping. Endpoint exact
                                    // (t=1 → result still cur). DEFAULT ON; --no-vblend disables.
    float vblend_t0=0.6f;           // --vblend-t0: phase where the tilt ramp begins (smoothstep(t0,1,t)); clamp
                                    // [0,0.95]. Read only when vblend on.
    float vblend_strength=0.5f;     // --vblend-strength: max tilt weight at t=1 (mix fraction toward the next pair's
                                    // MV); clamp [0,1]. Read only when vblend on.
    bool  vblend_exact=false;   // --vblend-exact: EXACT velocity-continuity — pay +1 pair of present lead so the REAL
                                // next-pair MV is in the ring, and tilt toward it (not the 2*mv-mv_prev prediction).
                                // DEFAULT OFF (adds ~1 source-frame latency). Implies --vblend.
    float band_xfade=1.0f;          // --band-xfade: 0 = OFF, >0 = strength. DEFAULT ON (1.0); --no-band-xfade sets 0.
                                    // In the image-field disocclusion band (bg-side) blend the result toward
                                    // blend_result (the un-warped cross-fade) = gravity cancellation (static bg →
                                    // clean, object stays sharp). Reuses binding-11 (the iGPU field) + blend_result;
                                    // no gme model needed. `--band-xfade` sets 1.0, `--band-xfade-strength F` sets it.
    float ts_smooth=0.1f;           // --ts-smooth: adaptive temporal smoothing gated to garbage pixels (mask
                                    // disocclusion artifacts, keep clean sharp). DEFAULT 0.1. `--ts-smooth 0` disables
                                    // (byte-identical off); `--ts-smooth F` overrides. The garbage-gate confines the
                                    // blend to low-confidence/disocclusion pixels — a mask for residual gravity
                                    // artifacts, not a fix for generation big-jumps.
    bool  mc_on=false;              // --multicand: scored MULTI-CANDIDATE medoid selection ("generate + discriminate +
                                    // SELECT" for generation big-jumps). DEFAULT OFF (opt-in). Byte-identical off (the
                                    // existing soft_gate/hard path runs).
    int   mc_nperturb=2;            // --mc-nperturb: speed-perturbed warp candidates added to {warp,A-only,B-only}: 0,
                                    // 2, or 4. Default 2. Read only when mc_on.
    float mc_perturb=0.15f;         // --mc-perturb: speed-perturbation fraction for the perturbed warps (mv·(1±f), ±2f
                                    // for 4). Default 0.15. Clamp [0,0.5].
    float mc_disp=0.35f;            // --mc-disp: candidate-set DISPERSION threshold (mean L1 of the medoid to the
                                    // others, color scale ~0..1.7). Above it = no consensus → safe crossfade fallback
                                    // unless on a true edge. Default 0.35. Clamp [0,2].
    float mc_edge=0.5f;             // --mc-edge: iGPU Sobel boundary threshold (0..1). edge ≥ this = a true edge → keep
                                    // the HARD medoid pick (never the linear-blend ghost). Default 0.5 (0 = always
                                    // hard; ~1 = edge-gate off). Clamp [0,1].
    float disoccl_hardpick=0.f;    // --disoccl-hardpick: edge-gated HARD-PICK threshold at the bidir reveal band. 0 =
                                   // OFF (byte-identical). >0 = the iGPU Sobel edge threshold above which the round-
                                   // trip-consistent side is committed FULLY (no soft blend = no smear/step/crescent);
                                   // flat interior keeps the soft blend. Needs --bidir (default on) + the iGPU field
                                   // (default on via bg-snap/band-xfade). Clamp [0,1].
    bool   pace_variance=false;     // --pace-variance: the variance-aware moving-average present pacer. target_interval
                                    // = SMA10(present deltas) − pv_var_factor·stddev − pv_safety_ms, reset on a >100ms
                                    // hitch (the minus-variance term shrinks the target under jitter so a saturated/
                                    // erratic GPU can't lock a slow interval then overshoot). Pure CPU, thread-P-local;
                                    // DEFAULT OFF → the fixed refresh_hz grid → byte-identical. Pairs with
                                    // --async-present.
    double pv_safety_ms=0.75;       // --pv-safety: safetyMargin (ms). Read only when --pace-variance.
    double pv_var_factor=0.1;       // --pv-var: varianceFactor. Read only when --pace-variance.
    float  target_output_fps=0.f;   // --target-output-fps: the fractional output-rate controller. 0 = OFF
                                    // (byte-identical, the passive N). >0 = hold a STEADY sustainable output cadence:
                                    // realized_mult = clamp(target_eff/base_fps, 1, 8) where target_eff = min(this,
                                    // refresh, sustain_frac·measured-achievable-rate); N_target = realized_mult·span
                                    // drives the even grid → the over-production drop refuses to over-command the warp.
                                    // Auto-enables --async-present. NEVER caps the game (output-side).
    float  s2_sustain_frac=0.93f;   // --s2-sustain: kSustainFrac — target this fraction of the measured achievable
                                    // rate. Read only when --target-output-fps>0.
    bool  asw=false;                // --asw: bounded forward EXTRAPOLATION (Oculus-style ASW): when B falls behind and
                                    // the sync-clock phase overshoots the held pair, the warp projects cur FORWARD
                                    // along the MV instead of HOLD-at-1 freezing → fills the throughput deficit.
                                    // DEFAULT OFF (byte-identical off); needs --sync-clock (the content_clock supplies
                                    // the overshoot). Best with --bg-snap (clean MV).
    float asw_max=1.0f;             // --asw-max: the extrapolation bound in PHASE units (overshoot past 1; 1.0 = up to
                                    // one full source-span forward). Higher = fills deeper deficits but more
                                    // extrapolation error on direction changes.
    int   cap_slots=0;              // --cap-slots N: OVERRIDE the auto-sized capture-ring depth (0 = auto: ~src/10+4
                                    // from the --cap-fps ceiling, clamped [4, 32] + a ~768MB mem budget). The ring is
                                    // RAM frame-slots sized by the source/flow/resolution gap, NOT CPU topology.
    bool  ingest_async=false;      // --ingest-async: decouple the DDA capture INGEST so it scales with the rig instead
                                   // of the serial readback+convert chain. OFF (default) = the strictly serial DDA
                                   // loop, BYTE-IDENTICAL (no worker thread, no raw ring, no readback double-buffer).
                                   // ON = the acquire thread does ONLY AcquireNextFrame→CopyResource+Flush→
                                   // Map(DO_NOT_WAIT, the PREVIOUS slot = readback overlap)→memcpy into a kRawSlots RAW
                                   // host ring→publish raw_seq, and a NEW convert WORKER thread DROP-TO-NEWEST converts
                                   // the freshest raw slot, publishing c_seq PROMPTLY on each convert (freshest frame
                                   // reaches F/P at today's age). DDA-only (forced OFF for WGC). On raw-ring alloc
                                   // failure → forced OFF (falls back to serial, never crashes). The `acq=` field in
                                   // [ra-cap] shows the acquire rate (vs `in=` convert rate).
    bool  dedup=false;             // --dedup: DROP content-duplicate captured frames. DDA captures the desktop at the
                                   // DWM COMPOSITE rate (= the monitor refresh, e.g. 240Hz) but the game renders fewer
                                   // UNIQUE frames/s → the surplus captures are content-duplicates. The real unique
                                   // capture rate (dd_uniq, the `uniq=` readout) is ALWAYS reported regardless of this
                                   // flag. ON = a duplicate (identical sample-hash to the previous frame) is dropped so
                                   // the FG interpolates between TRUE uniques instead of zero-motion dup pairs. DEFAULT
                                   // OFF (byte-identical: the hash + dd_uniq counter still run, but no frame is skipped).
    int   ingest_backlog=3;        // --ingest-backlog N: the in-order ingest DRAIN DEPTH — how far behind the newest
                                   // capture F drains IN ORDER (span-1, smooth) before JUMPING to newest (span-2, the
                                   // "walking tremble", but FRESHER). The dominant freshage/input-lag floor component.
                                   // Default 3. LOWER = fresher (less input-lag) at the cost of more span-2 skips.
                                   // Clamped [1,kIngestBacklog]; only REDUCES → the torn-read static_assert
                                   // (cap_slots>=kIngestBacklog+1) stays valid (runtime ≤ compile-time max).
    bool  latency_trace=false;     // --latency-trace: MEASUREMENT-ONLY pipeline latency decomposition. Emits a
                                   // [lat-trace] line: INVISIBLE(compose,copy — pre-tcap, not in our lat) + the
                                   // freshage split (pickup⊇convert, build, derived detect). Each delta is within ONE
                                   // clock (no cross-clock subtraction); compose uses a QPC↔SystemRelativeTime delta
                                   // with an epoch-mismatch guard (→0 if unavailable). Default OFF, byte-identical.
    bool  copy_fence=false;        // --copy-fence: event-driven WGC copy-completion pickup — replaces the
                                   // Map(DO_NOT_WAIT)+Sleep(1) busy-poll with a D3D11 fence+event wait OFF the context
                                   // (the callback Signals after CopyResource; C waits the event). Needs D3D11.4
                                   // (ID3D11Device5/ID3D11DeviceContext4); if unavailable → forced OFF. Default OFF,
                                   // byte-identical.
    bool  copy_device=false;       // --copy-device: run the INVISIBLE WGC staging CopyResource on a SEPARATE D3D11
                                   // device on the SAME adapter, so the copy queue is decoupled from d.dev (the game
                                   // capture/present device) and OVERLAPS rather than fronting pickup. The WGC pool,
                                   // the staging ring, the copy-fence, the callback CopyResource, and the C-thread Map
                                   // all run on the 2nd device when on; d.dev/d.ctx when off (a single alias-pointer
                                   // gate cap_dev/cap_ctx). WGC-only; if the 2nd device fails to create → FORCED OFF
                                   // (cap_dev=d.dev → byte-identical). The slot's t_cap_ms is set ONLY after the
                                   // DO_NOT_WAIT Map of the completed copy (best paired with --copy-fence for the
                                   // explicit edge). Default OFF.
    bool  camera_twarp=false;      // --camera-twarp: RESPONSIVENESS extrapolation. Present the interpolated FG frame's
                                   // SAMPLING base shifted FORWARD by the predicted camera motion over the input lag
                                   // (a global UV lead = the gme AFFINE TRANSLATION (gme6[0],gme6[3]) in px / out_size
                                   // · amt) so the displayed view LEADS the capture → mouse/camera feels synced.
                                   // PRESERVES the A/B interpolation (whole frame samples at uv+cam_lead; store stays
                                   // at coord). OFF or gme invalid → cam_lead=(0,0) → byte-identical. Default OFF.
    float camera_twarp_amt=0.5f;   // --camera-twarp-amt: the eye-tunable lead scalar [0,3]. cam_lead =
                                   // (gme6[0],gme6[3])/out_size · camera_twarp_amt. Read only when camera_twarp.
    double sc_phase_gain=0.10;     // --sc-phase-gain: PLL proportional PHASE-correction fraction per new-pair arrival
                                   // (default 0.10). Runtime override for the cadence-lock sweep; default →
                                   // byte-identical.
    double sc_freq_alpha=0.05;     // --sc-freq-alpha: PLL FREQUENCY (T_robust) EMA weight per outlier-rejected arrival
                                   // (default 0.05). Runtime override; default → byte-identical.
    double sc_reseat_err=4.0;      // --sc-reseat: phase-error threshold (source-frames) above which the loop RE-SEATS
                                   // vs slews (default 4.0). Runtime override; default → byte-identical.
    bool   pace_present=false;     // --pace-present: the metronomic, drift-corrected present pacer. KEEPS the absolute
                                   // output-clock grid (tick_t0 + k·tick_period_ms = the metronome) and only SLOWLY
                                   // SLEWS the grid anchor tick_t0 toward the realized achievable cadence (a variance-
                                   // damped SMA10 drift signal), bounded ±a fraction of a tick per tick. It composes
                                   // WITH the output clock (slews the ANCHOR, never replaces the grid with a per-
                                   // present anchor). Never pushes the effective period below the panel tick (no over-
                                   // production windup). Pure CPU, thread-P-local. DEFAULT OFF → the fixed refresh_hz
                                   // grid → byte-identical.
    double pp_safety_ms=0.50;      // --pp-safety: safetyMargin (ms) of the variance-damped drift target. Read only
                                   // when --pace-present.
    double pp_var_factor=0.1;      // --pp-var: varianceFactor of the drift target. Read only when --pace-present.
    double pp_slew_gain=0.05;      // --pp-slew: per-tick fraction of the grid phase error folded into tick_t0 (the
                                   // 1st-order drift corrector). Small enough to filter present jitter, large enough to
                                   // absorb slow cadence drift. Clamp [0,0.5]. Read only when --pace-present.
    bool   pace_hard=false;        // --pace-hard: the HARD present-target pacer. Composes ABOVE the soft slew: after
                                   // paced_wait_P lands near the steady-clock tick tgt, a BOUNDED final spin re-
                                   // expresses tgt in the QPC domain (an explicit local steady↔QPC bridge sampled
                                   // together, no silent clock-mix) and pins THIS frame to the QPC edge. FREEZE-FLOOR:
                                   // the total hard wait is HARD-CAPPED below one vblank period; a missed/overshot/un-
                                   // derivable target degrades to the soft path and NEVER blocks. Own-window-only (the
                                   // displayed Independent-Flip plane; composed styles are DWM-jitter-limited) — gated
                                   // on cfg.present_own_window too. Pure CPU, thread-P-local, cold-setup QPC freq.
                                   // DEFAULT OFF → the hard branch is unreached → byte-identical.
    double ph_spin_ms=2.0;         // --ph-spin: the bounded final-spin budget of --pace-hard (ms). Mirrors
                                   // paced_wait_P's kSpinMs=2.0. The hard cap on TOTAL hard-wait is one vblank period
                                   // (tick_period_ms), independent of this. Clamp [0.1,4.0]. Read only when --pace-hard.
    bool   pace_vblank=false;      // --pace-vblank: the VBLANK PHASE-LOCK. Aligns the grid ANCHOR tick_t0 to the true
                                   // vblank phase (DwmGetCompositionTimingInfo → qpcVBlank/qpcRefreshPeriod), so ph_tgt
                                   // = tick_t0 + k·tick_period_ms lands AT the panel cadence. COMPOSES ABOVE --pace-
                                   // hard (refines ph_tgt's PHASE; does not replace the pin or widen the budget).
                                   // COLD/periodic query (~once/sec — DwmGetCompositionTimingInfo is an API call,
                                   // never per-tick); the QPC↔steady_clock domains are bridged by ONE paired
                                   // now_ms()+QPC sample (no silent epoch mix). The phase slew is BOUNDED (a fraction
                                   // of a tick) so it never jumps tick_t0 past the re-seat / freeze-floor. Query fail/
                                   // unavailable → tick_t0 left as-is → degrades to plain --pace-hard, NEVER blocks.
                                   // Own-window-only + requires --pace-hard. Pure CPU on the cold path, thread-P-local.
                                   // DEFAULT OFF → the query + the tick_t0 adjust never run → byte-identical.
    double pv_lock_gain=0.05;      // --pv-lock-gain: per-query fraction of the vblank phase error folded into tick_t0
                                   // (the phase-lock slew). Small enough that no single query jumps the grid; bounded
                                   // to ±half a tick anyway. Clamp [0,0.5]. Read only when --pace-vblank.
    bool  sc_select=true;          // --sc-select: the content_clock drives pair SELECTION too (not just phase), so
                                   // selection and phase read the SAME clock → the warp sweeps a FULL 0→1 per pair
                                   // (kills the clock-disagreement top-of-ladder truncation + the start-stall). DEFAULT
                                   // ON; --no-sc-select reverts to the wall-time t_display scan. Implies --sync-clock
                                   // (needs the content_clock). A read-side selector swap; the F/P generation protocol
                                   // is untouched.
    bool  fg_auto=false;            // --fg-factor auto — N from measured capacity
    float mv_smooth=0.f;            // temporal MV EMA alpha (0=off)
    bool  mv_prior=false;           // temporal MV prior — dual-centre coarsest search on the OFP matcher (self-healing
                                    // on cuts, no detector). OFF = byte-identical.
    int   dump_n=0;                 // dump the next N presented frames to frames/ (diagnostic)
    int   objdump_n=0;              // --objdump N: dump N pairs' BLOCK GRIDS to frames/ as tiny BMPs (mvw×mvh): the
                                    // post-repair dissidence mask, |MV|·16 gray, persist[] — the F data plane made
                                    // visible (diagnostic; gitignored).
    int   pairdump_n=0;             // --pairdump: at each WAP pair-advance, dump the TWO full-res frames the warp
                                    // actually receives (hostR[prev_slot]/hostR[cur_slot]) + print
                                    // pair_c/prev_cseq/span/cur_c — the warp INPUT plane made visible.
    int   outdump_n=0;             // dump N presented WARP OUTPUTS (wapOutA readback after the fence, phase t in the
                                   // filename) — the synthesis plane made visible: WHICH pixels/layers paint the
                                   // artifacts, at WHICH phases.
    int   qdump_n=0;               // --qdump N: write ~N held-out TRIPLES to qdump_dir/ — each = (wapPrevA=real N,
                                   // wapOutA=the live FG output, wapCurA=real N+2) as raw RGBA8 .rgba + a truth-less
                                   // manifest line in the fg_quality_scorer format (no mid= — live FG has no held-out
                                   // ground truth; the scorer scores crossfade-only). Sampled (every Kth present) so
                                   // ~N triples span the run. DEFAULT 0 = OFF, byte-identical.
    char  qdump_dir[256]={};        // output dir for --qdump (the .rgba triples + manifest.txt).
    int   phaselog_n=0;             // --phaselog N: for N consecutive PAIRS, collect the t_use of EVERY presented WAP
                                    // tick of that pair; on pair-advance print one ladder line `[ra] phase pair_c=K
                                    // span=S n=T t=[...]` (two decimals, ≤40 values). The fluidity probe: does the
                                    // presented phase sequence per pair cover [0,1)? P-local fixed buffer, zero heap.
    bool  phase_norm=false;         // --phase-norm: the NORMALIZED-N frame ladder. The content-clock places each
                                    // presented frame at its jittery sub-phase → the ~refresh/source ticks a pair gets
                                    // land UNEVENLY → juddery even when the fps COUNT is right. phase-norm DECOUPLES
                                    // the displayed intra-pair phase from the clock: tick j of a pair → the EVEN grid
                                    // (j+0.5)/N, N = predicted ticks-this-pair = span·T_robust/tick_period. The clock
                                    // still SELECTS the pair; only the phase is a uniform sweep. Residual: N unknown
                                    // until the pair closes → an N_pred mismatch over/undershoots the last step (the
                                    // irreducible boundary error, clamped [0,1]). DEFAULT OFF → byte-identical (t_use
                                    // stays the content-clock phase).
    bool  cphase=false;             // --cphase: velocity-continuous intra-pair phase-rate reshape. --phase-norm evens
                                    // the tick SPACING but the displayed VELOCITY still STEPS at the pair seam (each
                                    // pair warps at its own constant mv; t resets 1→0, mv switches → a first-derivative
                                    // jump). cphase bends the SCALAR t-vs-tick curve t→g(t) so the OPENING slope of a
                                    // new pair matches the CLOSING displayed speed of the prev pair (g'(0)·|mv_new| ≈
                                    // |mv_prev|), easing back to linear over a short window. It NEVER touches the MV
                                    // (A/B still converge → fine TEXT is safe, unlike --vblend) and the endpoints are
                                    // EXACT (g(0)=0,g(1)=1 → ZERO added latency, no present lead). g is a monotone
                                    // cubic Hermite (slope clamped to the Fritsch-Carlson bound → never reverses).
                                    // DEFAULT OFF (byte-identical).
    float cphase_ease=0.25f;        // --cphase-ease W∈[0.05,0.5]: phase-width of the opening ease; outside [0,W] g=t.
    float cphase_gain=1.0f;         // --cphase-gain G∈[0,1]: seam-slope match strength (0=linear/off, 1=full match).
    // The output clock is unconditionally the timer (the panel's cadence).
    OutputClock output_clock=OC_TIMER;
    int   refresh_hz=240;           // output-clock tick rate (overrides the panel Hz)
    int   cap_fps=0;                // --cap-fps N: throttle WGC capture to N fps (MinUpdateInterval=1/N). 0 = default
                                    // 8ms (~125/s cap). LOW N (e.g. 15) → fewer real frames → the FG interpolates MANY
                                    // MORE frames per real pair → the disocclusion crescent shows across many more
                                    // presented frames = easier to eyeball. Diagnostic only: does NOT change the FG
                                    // algorithm, only the source cadence.
    bool  warp_at_presenter=true;   // presenter A (the bridge owner) re-warps the pair at the per-tick exact phase from
                                    // B's shipped MV+SAD. OFF = the grid path. DEFAULT ON. --no-warp-at-presenter =
                                    // grid mode.
    bool  soft_gate=true;           // continuous warp gate (smoothstep'd confidence + agreement + isolated-block
                                    // suppression) instead of the binary per-8×8-block keep/freeze. DEFAULT ON.
                                    // --no-soft-gate = the binary gates, byte-identical.
    bool  commit_real=true;         // commit target = UNWARPED nearest real (kills the displaced-fetch edge fringe of
                                    // the warped commit; the pixel drops to source cadence locally). DEFAULT ON.
    float commit_thresh=0.08f;      // commit-don't-hedge warp. 0 = the averaging path, byte-identical. >0 = per-pixel
                                    // max-channel-abs-diff test: above the threshold the warp commits to the temporally
                                    // nearest sample (kills the ghost eye) and the gated fallback uses the nearest real
                                    // frame. DEFAULT 0.08. --no-commit sets commit_thresh=0 + commit_real=false.
    bool  appearance=true;          // appearance-band temporal re-blend on committed pixels. DEFAULT ON. Lives INSIDE
                                    // the commit SOFT band (wc>0.0): a committed (one-sided) pixel whose two anchors at
                                    // the committed fetch position agree on STRUCTURE but differ mildly in TONE (max-
                                    // channel gap inside appear_band) is re-blended toward the phase-t color — the
                                    // brightness-constancy violator (a pulsing/shadowed surface). ONE extra texture
                                    // tap, committed pixels only. Pushed 1.0 iff commit live (commit_thresh>0) AND
                                    // this — commit governs (--no-commit takes it down). --no-appearance = byte-
                                    // identical.
    float appear_band=0.10f;        // the appearance band in max-channel [0,1] color units. DEFAULT 0.10. --appear-band
                                    // F overrides (clamped [0.02,0.5]). Small tone gap → full temporal interp; gap ≥
                                    // 2·band → 0 (one-sided commit stands — a real displacement, the reason commit
                                    // exists).
    bool  bidir=true;               // bidirectional flow + occlusion classification. DEFAULT ON. --no-bidir = byte-
                                    // identical (also disables fill-div and matte via cascade). WAP-path only.
    float occl_thresh=1.5f;         // fwd↔bwd round-trip consistency threshold, PIXEL units. DEFAULT 1.5. --occl-thresh
                                    // F overrides (clamped [0.25,8.0]).
    bool  fill_div=false;           // divergence-directed disocclusion pick. DEFAULT OFF: its hard side-select by
                                    // divergence SIGN is unreliable noise on smooth gradients → the trailing-rim bites
                                    // the commit then sharpens. Its disocclusion job is now done with CLAIM evidence
                                    // instead of the noisy divergence sign. --fill-div re-enables (requires bidir).
    float div_eps=0.05f;            // divergence band, pixels per MV-grid texel. DEFAULT 0.05. --div-eps F overrides
                                    // (clamped [0.005,1.0]).
    bool  rescue=true;              // candidate-rescue. DEFAULT ON. ON = a pixel whose primary MV fails the d_ab commit
                                    // trigger first tries the 8 NEIGHBOR block MVs (3x3 fwd field) and, if the best
                                    // scores ≤ commit_thresh, USES that candidate's average and SKIPS commit. Requires
                                    // the commit trigger (commit_thresh>0). --no-rescue = byte-identical.
    bool  mv_median=false;          // 3x3 component-wise vector-median on the uploaded MV field(s) before the warp
                                    // (A-side, in wap_upload). OFF (default) = the field is uploaded unchanged → byte-
                                    // identical. ON = a stray vector that disagrees with its 8 neighbours is replaced
                                    // by the neighbourhood median — kills the flat-content rim stamps/holes photometric
                                    // verification can't see. WAP-path only. Filters wapMVBA too when --bidir is active.
    bool  mv_guided=true;           // color-guided MV assignment (the contour-membership principle). DEFAULT ON. ON =
                                    // (1) the consensus pass weights each 3x3 vote by COLOR membership in cur_real
                                    // (votes below sim_thresh excluded; <3 cohort → keep own MV) instead of the blind
                                    // median, and (2) the warp's PRIMARY MV fetch becomes a bilateral (per-corner
                                    // color-membership) upsample instead of LINEAR. SUPERSEDES --mv-median's blind pass
                                    // when both given. WAP-path only. --no-mv-guided = byte-identical.
    float mv_sim=0.10f;             // color-membership band, max-channel [0,1] units. DEFAULT 0.10. --mv-sim F overrides
                                    // (clamped [0.02,0.5]).
    bool  gme=true;                 // the frame-holon — a global affine motion model fitted per pair on the CPU (F
                                    // thread) from the fwd MV grid by IRLS least squares. DEFAULT ON. ON = (a) a per-
                                    // block dissidence mask (|mv − model|) is computed + shipped to wapDISA and the %
                                    // dissident is printed; (b) the model is an extra rescue candidate in the warp
                                    // (fast pans beyond the search window); (c) the fill-div revealed case samples cur
                                    // at the model displacement. Requires WAP. --no-gme = byte-identical (also disables
                                    // matte via cascade).
    bool  matte=false;              // the fluid-matte — boundary-first two-layer compositing. DEFAULT OFF: the two-
                                    // layer composite DOUBLES the figure → a crossfade. ON = the warp decides a BINARY
                                    // matte per pixel from the advected dissidence (wapDISA): OBJECT → the existing
                                    // pipeline result stands; BACKGROUND → BOTH warp_result AND blend_result become the
                                    // pure global-model layer sample. Requires --gme AND --bidir. OFF cascades the
                                    // whole matte sub-stack off (crescent/travel/contour/obj_crescent/member_commit
                                    // gate on use_matte → matte_push). --matte re-enables it (A/B).
    float matte_thresh=0.25f;       // the dissidence cutoff in R8-normalized units [0,1]. DEFAULT 0.25. The R8 store is
                                    // min(255,16·r_px); a sample reads v=16·r_px/255, so 0.25 ↔ r≈4px. --matte-thresh F
                                    // overrides (clamped [0.05,1.0]).
    bool  crescent=true;            // crescent-directed mask-weighted background fetch. DEFAULT ON. Lives INSIDE the
                                    // matte BACKGROUND branch: ON = a background-classified pixel's two model-layer
                                    // samples (prev/cur) are re-weighted by their NON-contamination (the anchored
                                    // dissidence dis_fwd/dis_bwd) so the trail and the leading-edge contamination are
                                    // down-weighted. Mask-WEIGHTED side fetch. Pushed 1.0 iff use_matte AND this —
                                    // matte governs (no matte ⇒ no masks ⇒ no crescent). --no-crescent = the unchanged
                                    // (1−t,t) background blend (byte-identical, the push flag is 0).
    bool  travel=true;              // the traveling-silhouette occupancy. DEFAULT ON. Lives in the matte OCCUPANCY
                                    // decision: ON = the self crutches FADE with phase (fwd self ·(1−t), bwd self ·t)
                                    // so the phase-t silhouette TRAVELS with the object instead of smearing across the
                                    // swept band — the gather terms carry the mask at full weight. Kills the residue
                                    // trail behind a mover. The dis_fwd/dis_bwd the crescent reads STAY the full
                                    // max(self,gather) testimonies — only the occupancy decision is faded. Pushed 1.0
                                    // iff use_matte governs. --no-travel = the occupancy lerp of the full testimonies
                                    // (byte-identical, the push flag is 0).
    bool  contour=true;             // the contour marriage — per-pixel boundary-band arbitration by color affinity.
                                    // DEFAULT ON. Lives at the matte COMPOSITION site: ON = in the uncertain band
                                    // (|matte_occ − thr| < thr) each pixel is arbitrated between the OBJECT layer (the
                                    // warp/blend) and the BACKGROUND layer (the crescent-weighted bg) by which the
                                    // time-nearest real at uv resembles (alpha_obj = (d_b+eps)/(d_o+d_b+2eps)) — sub-
                                    // block precision from appearance for the contour-band bites the block-MV warp
                                    // drags across edges. Outside the band the binary !matte_object decision is
                                    // unchanged. Pushed 1.0 iff use_matte governs. --no-contour = the binary matte
                                    // composition (byte-identical, the push flag is 0).
    bool  obj_crescent=true;        // the object crescent — anchored-claim side weighting for the OBJECT layer (the
                                    // symmetric piece of crescent, which only ever touched the BACKGROUND). DEFAULT ON.
                                    // ON = for an OBJECT-classified pixel the warp's A/B blend upgrades from (1−t,t) to
                                    // claim-weighted (w_A ∝ (1−t)·c_f, w_B ∝ t·c_b, normalized, c_f/c_b the same
                                    // crescent weights from dis_fwd/dis_bwd) AND the commit's time-nearest side pick
                                    // respects the claims (prefer the claiming anchor). Fixes the leading-band quarter-
                                    // displacement bite; deep-object (c_f≈c_b≈1) reduces to exactly (1−t,t). Pushed 1.0
                                    // iff use_matte governs. --no-obj-crescent = the plain (1−t,t) object blend + time-
                                    // nearest commit (byte-identical, the push flag is 0).
    bool  member_commit=true;       // MEMBERSHIP BEATS THE BLEND (the cross-fade ghost-step fix). DEFAULT ON. The warp-
                                    // vs-blend selection hedges to the (1−t,t) blend on low-texture object INTERIORS
                                    // whose photometric gates (residual/improvement, agreement) fail — the flat-content
                                    // blind spot — but the field inside a tracked silhouette is holon-repaired, so the
                                    // blend hedge cross-fades two fixed positions (the ghost-step). ON = for a pixel
                                    // inside the object self-mask (max(self_fwd,self_bwd) > matte_thresh — the mv-free
                                    // OBJECT proxy) the blend-fallback pressure is scaled down by (1 − member_strength),
                                    // member_strength = smoothstep(thr,2·thr,self_m). EXTREME escape: d_pixel >
                                    // 3·agreement still blends (genuine garbage protection). Pushed 1.0 iff use_matte
                                    // governs. --no-member-commit = the unchanged selection (byte-identical, push 0).
    bool  commit_default=true;      // the COMMIT-DEFAULT — WARP IS THE DEFAULT. DEFAULT ON. Wherever the warp-vs-blend
                                    // gate weight w<1 (silhouette fringes, sub-mask text, unclustered detail) the
                                    // photometric confidence gates would hedge to blend_result = (1−t)·prev[uv] +
                                    // t·cur[uv] at FIXED positions, so moving nameplate TEXT doubles. ON = the warp
                                    // wins UNLESS the post-warp source disagreement d_pixel is genuinely large (soft: w
                                    // floors at 1−g, g=smoothstep(2·agreement,4·agreement,d_pixel); hard: blend only
                                    // when d_pixel>2·agreement) — the photometric gates no longer FORCE a blend on their
                                    // own. The member boost composes (it only RAISES w). A WARP-LEVEL rule — gated on
                                    // wap, MATTE-INDEPENDENT; --no-wap kills it. --no-commit-default = the unchanged
                                    // photometric-fallback selection (byte-identical, the push flag is 0).
    bool  onepos=true;              // ONE POSITION PER PIXEL — the double-exposure collapse (the ghost-step root).
                                    // DEFAULT ON. warp_result is ITSELF a two-sample blend (1−t)·A[x−t·mv] +
                                    // t·B[x+(1−t)·mv]; with imperfect/zero mv the two samples land on DIFFERENT content
                                    // = a double exposure (nameplate TEXT too thin to cluster, mv=0, is the pure case).
                                    // ON = DISAGREEMENT COLLAPSE: where the two displaced samples agree (d_pixel small)
                                    // the blend stands (averaging sharpens); where they disagree the heavier side paints
                                    // ALONE (single position). c=smoothstep(agreement,2·agreement,d_pixel); the A/B
                                    // weights sharpen toward winner-take-all as c→1. Applied AT the warp_result
                                    // composition site so all downstream consumers inherit. blend_result is UNTOUCHED.
                                    // A WARP-LEVEL rule — gated on wap, MATTE-INDEPENDENT; --no-wap kills it. --no-onepos
                                    // = the unchanged two-sample blend (byte-identical, the push flag is 0).
    bool  phase_anchor=true;        // the phase-anchored primary MV (the stale-vector fix). DEFAULT ON. ON = the warp's
                                    // PRIMARY per-pixel MV is anchored to the instant nearest the output phase t: at
                                    // t≤0.35 the prev-anchored forward field, at t≥0.65 the cur-anchored backward field
                                    // NEGATED (binding 5), smooth-blended through the centre (w_b =
                                    // smoothstep(0.35,0.65,t)). The fwd field, anchored at PREV, fetches stale vectors
                                    // at high phase; the cur-anchored field is correct where the content NOW is. ONE
                                    // primary site → every consumer (A/B fetch, matte advection, crescent geometry,
                                    // rescue primary) inherits coherently; the round-trip occlusion classification is
                                    // EXEMPT (it compares the raw fwd vs raw bwd fields). Pushed 1.0 iff use_bidir AND
                                    // this AND the bwd field is valid this generation (bwd_ok). --no-phase-anchor = mv =
                                    // the prev-anchored forward field only (byte-identical, the push flag is 0).
    bool  ambig=true;               // the AMBIGUITY channel (the periodic-texture killer). DEFAULT ON. ON = B's optical-
                                    // flow pipeline emits the RUNNER-UP candidate (second-best MV + its SAD) per pair
                                    // (cand_image(), RGBA16F: xy=second MV, z=second SAD); F ships it through the per-
                                    // gen host bridge (hostC2 → hC2_b/hC2_a → wapC2A, binding 10) and the warp
                                    // arbitrates SAD TIES: a BACKGROUND/texture-interior pixel where the runner-up is
                                    // near-tied in SAD but a texture-period away from the matcher's pick (periodic
                                    // textures resolve ties arbitrarily) takes whichever of best/runner-up is NEARER
                                    // the referee (the gme background model gme_model_mv). OBJECT pixels are EXEMPT
                                    // (their field is already holon-arbitrated). Needs gme → WAP + bidir + gme; the
                                    // cascades take it down. --no-ambig = the matcher's raw pick (byte-identical, push
                                    // 0, binding 10 placeholder-bound, emit_second_best off → cand_image() not created).
    bool  stasis=true;              // the stasis layer (the HUD fix). DEFAULT ON. ON = a pixel whose block sad_zero ≤
                                    // thresh bypasses ALL motion machinery and presents the current real frame directly.
                                    // Works on ANY stack (no flag dependency); zero extra cost. --no-stasis = byte-
                                    // identical.
    float stasis_thresh=0.50f;      // the sad_zero cutoff. Units = the per-8×8-block SUM of Σ_chan|A−B| over RGB in
                                    // [0,1] color units, range [0,192]; ~0 = identical block. DEFAULT 0.50. --stasis-
                                    // thresh F overrides (clamped [0.05,8.0]). 8.0 ≈ 0.014 mean per-channel — still
                                    // visually identical.
    bool  objects=true;             // the object-holon — F-side connected-component clustering of the dissidence masks +
                                    // a 16-slot temporal-identity table + the conservative motion-INHERITANCE repair.
                                    // DEFAULT ON. ON = inside a tracked moving object's scanline-filled silhouette,
                                    // blocks measuring near-static MV (|mv|≤1px while |mv_obj|≥2px — the motion-
                                    // cancellation signature) get the object's MV written back into the half-float MV
                                    // field (hostMV/hostMVB), EXEMPTING high-inertia-persistence blocks (persist[]≥128
                                    // — the HUD shield). The repaired blocks' dissidence bytes are recomputed for
                                    // mask/field consistency BEFORE the matte mass count + the f_pair publish. Requires
                                    // --gme. --no-objects = repair count 0 = pre-repair behavior.
    bool  shapefield=true;          // the contour shape-field — a child of --objects. DEFAULT ON. ON = the object-
                                    // holon's inheritance no longer writes the single rigid slot MV into every
                                    // cancellation block; instead the closed silhouette contour casts a DISTANCE +
                                    // FEATURE transform inward (2-pass chamfer, weights 3/4, bbox-bounded) so each
                                    // interior block knows its distance to the rim AND its nearest rim block, then
                                    // inherits mix(nearest-rim MV, slot MV, w) with w = clamp(dist/12, 0, 1) — near the
                                    // contour the rim sector wins (rotation/scaling/deformation correct), deep interior
                                    // relaxes to the slot's stable EMA mean. The rim band itself (chamfer dist < 3) is
                                    // NEVER rewritten. Arming: arm iff |mv_obj_slot| ≥ 2px OR the rim MV spread ≥ 2px
                                    // (a scaling/rotating object can have mean ≈ 0 yet a live rim field). Child of
                                    // --objects: --no-objects takes it down. --no-shapefield reverts to rigid single-MV
                                    // inheritance (the else-arm).
    bool  inertia=true;             // the inertia prior (motion-state hysteresis). DEFAULT ON (the shader's
                                    // `inertia_thresh>0.0` short-circuits when 0). ON = per-MV-block persistence (a
                                    // uint8 counter accumulated F-side: static_now ? min(255,persist+16) : 0) is shipped
                                    // to an R8 bridge (u_persistence) and gates three ambiguity-resolution paths for
                                    // pixels with a STATIC HISTORY (persist≥thresh): the rescue loop rejects fast
                                    // candidate MVs, the bilateral fetch refuses a fast color-chosen corner, and the gme
                                    // model rescue candidate is skipped. WAP-only. --no-inertia = byte-identical.
    float inertia_thresh=0.50f;     // the persistence cutoff. Read in the shader as p = texture(u_persistence,uv).r ∈
                                    // [0,1] (the R8 store is persist/255, so 16/255≈0.063 per sustained-static pair).
                                    // DEFAULT 0.50 (≈ 8 static pairs at +16/pair). --inertia-thresh F overrides (clamped
                                    // [0.1,1.0]). 0.5 ↔ ~0.13s of sustained stasis at 60fps source.
    float mass_k=0.5f;              // the mass-conservation feedback gain. The matte's phase-t silhouette is mix(dis_fwd,
                                    // dis_bwd, t), which can THIN at mid-t. The feedback measures the presented matte
                                    // mass (a GPU workgroup counter) vs the expected mass (lerp(m_fwd, m_bwd, t) of the
                                    // anchored masks) and tunes the matte threshold: thr_eff = matte_thresh·(1 +
                                    // mass_k·err_ema), err = (presented − expected)/max(expected,1), err_ema EMA 0.9,
                                    // thr_eff clamped [0.5,2]·matte_thresh. mass low (bites) → thr drops → vessel
                                    // refills; mass high (spill) → thr rises. DEFAULT 0.5. --mass-k F overrides (clamped
                                    // [0,2]; 0 = feedback OFF = the clean lerp only).
    // --no-* tracking bools — set by the escape-hatch flags; used in the post-parse normalization
    // block to apply cascades regardless of flag order on the command line. NOT runtime feature flags
    // (those are the bools above); these are parse-time markers only.
    bool  no_wap=false;             // --no-warp-at-presenter (cascades all WAP-dependent features)
    bool  no_commit=false;          // --no-commit (cascades rescue + appearance)
    bool  no_appearance=false;      // --no-appearance (the committed value is not re-blended)
    bool  no_bidir=false;           // --no-bidir (cascades fill-div + matte)
    bool  no_gme=false;             // --no-gme (cascades matte)
    bool  no_objects=false;         // --no-objects (the object-holon repair off)
    bool  no_shapefield=false;      // --no-shapefield (rigid single-MV inheritance)
    bool  no_crescent=false;        // --no-crescent (the matte background blend reverts to (1−t,t))
    bool  no_travel=false;          // --no-travel (the occupancy reverts to the full-testimony lerp)
    bool  no_contour=false;         // --no-contour (the matte composition reverts to the binary !matte_object decision)
    bool  no_obj_crescent=false;    // --no-obj-crescent (the object A/B blend reverts to (1−t,t) + time-nearest commit)
    bool  no_member_commit=false;   // --no-member-commit (the warp-vs-blend selection stays unchanged)
    bool  no_commit_default=false;  // --no-commit-default (the warp-vs-blend selection keeps the photometric fallback)
    bool  no_onepos=false;          // --no-onepos (warp_result reverts to the two-sample blend)
    bool  disoccl_commit=false;     // --disoccl-commit: occlusion-aware ONE-SIDED commit replaces the two symmetric
                                    // `w_sum<1e-4` BLEND FALLBACKS (object-layer wa + background-layer bg) that paint
                                    // the disocclusion crescent. Commits each contaminated-band pixel to the single
                                    // correct side by the raw dis_fwd/dis_bwd asymmetry (leading edge → cur owns,
                                    // trailing edge → prev owns; bg → the less contaminated side), softly gated on the
                                    // asymmetry magnitude so a true tie keeps the phase blend. Reuses dis_fwd/dis_bwd —
                                    // no new tap; runtime ≈ 0, only in the already-rare w_sum<1e-4 branch. Requires
                                    // --matte (the dissidence evidence). DEFAULT OFF — byte-identical when off.
    float onepos_band=1.0f;         // the collapse-onset scale. The disagreement-collapse onset =
                                    // agreement_threshold·band. 1.0 = the default onset; LOWER = the sub-threshold
                                    // faint crescents collapse too (each halving ≈ halves the residual ghost); the
                                    // NOISE FLOOR is the honest limit (≈0 = winner-take-all on genuinely intermediate
                                    // content → flicker). --onepos-band F clamp [0.05,4].
    bool  expire=true;              // stigmergy expiration — the content-decision memories that cross pairs (the holon
                                    // slot-MV EMA, the mass-feedback err EMA) EXPIRE on contradiction (direction
                                    // reversal / large innovation) instead of decaying through it. EMA for noise,
                                    // expiration for contradiction. DEFAULT ON. --no-expire = pure EMAs (the decay path).
    bool  no_expire=false;          // --no-expire tracking bool
    bool  scene_memory=true;        // the scene-holon silhouette memory — a persistent CUR-anchored dissidence prior
                                    // advected by the fwd MV field each pair, merged max(fresh, decay(prior)) into the
                                    // masks BEFORE clustering so a cluster's identity (and its inheritance) survives a
                                    // CANCELLATION phase (fresh masks go silent → without memory the cluster dies → the
                                    // matte collapses). The expiration rule guards it (confident background evaporates
                                    // the prior instantly; a silent cancellation-compatible block keeps it decayed).
                                    // DEFAULT ON. --no-memory disables.
    bool  no_memory=false;          // --no-memory tracking bool
    bool  persist_reset=true;       // MEMBERSHIP BEATS INERTIA. DEFAULT ON. Inside an ARMED moving cluster's filled
                                    // silhouette the persistence counter RESETS: a mover's flat interior (|mv|≈0 AND
                                    // sad_zero≈0 — immeasurable) is INDISTINGUISHABLE from a HUD to the persistence
                                    // counter, latched ≥128 after ~8 pairs → the shield blocks the inheritance → the
                                    // interior stays pinned at MV=0 → double-image trail/bites. The reset unblocks the
                                    // inheritance AND releases the shader inertia gates (hostPER). Real HUDs never sit
                                    // inside an armed cluster (transient mover-overlap resets re-latch in ~8 pairs — the
                                    // accepted trade). --no-persist-reset reverts to the pure shield.
    bool  no_persist_reset=false;   // --no-persist-reset tracking bool
    bool  change_gate=true;         // THE CHANGE GATE. DEFAULT ON. The dissidence masks require CHANGED content: a block
                                    // with sad_zero < kChangeGateSadZ cannot be dissident regardless of its MV — the
                                    // pyramid matcher's HALO (object MVs painted onto flat background tiles around/behind
                                    // every mover) is the ROOT of the oldest trail/chunk artifact, upstream of every
                                    // mask consumer (matte/crescent/travel/cluster/memory). --no-change-gate = raw masks.
    bool  no_change_gate=false;     // --no-change-gate tracking bool
    bool  no_phase_anchor=false;    // --no-phase-anchor tracking bool (the primary MV stays prev-anchored fwd)
    bool  no_ambig=false;           // --no-ambig tracking bool (the matcher's raw pick, no candidate arbitration)
    bool  low_d=false;             // --low-d: D-anchor span-term floor-trim — cut the input-lag EXCESS WITHOUT a
                                   // present-before-pair-ready freeze. Trims only the additive SPAN term of the phasefix
                                   // D; KEEPS freshage_ema as the freeze floor (D >= freshage_ema always → selects a
                                   // FRESHER pair → less freeze risk). REQUIRES phasefix (default); a no-op under
                                   // --no-phasefix. DEFAULT OFF.
    double lowd_span_frac=0.5;      // fraction of the span term kept (clamp [0,1]; 1.0 = no trim)
    double lowd_span_cap=1.5;       // growth ceiling for the span term, in source frames (clamp [1,8])
    bool   real_fast_path=false;    // --real-fast-path / --rfp: on a tick that already lands at the freshest captured
                                    // REAL (gen_back==0, phase≈1), OVERRIDE the D-selected warp pair with the freshest
                                    // real presented through the DEDICATED async bslot path (NEVER the synchronous
                                    // do_present_P → use-after-reset). A real needs no interpolation pair → no pair-
                                    // publish wait → minimal D for reals; interp ticks keep the full D-anchored cadence.
                                    // Crash-class. IMPLIES --async-present. DEFAULT OFF (byte-identical: the override
                                    // branch is gated on cfg.real_fast_path).
    float  rfp_window=0.15f;        // phase tolerance for "this tick already ≈ the cur real" (fire only when
                                    // phase_global >= 1 - rfp_window; small + biased to phase~1 so the real never
                                    // displaces a genuinely-distinct mid-pair interp → bounds the cadence cost). clamp
                                    // [0,1].
    bool   rfp_fresh=false;         // --rfp-fresh: present the FRESHEST CAPTURED real ((cur_c-1)%cap_slots, the proven-
                                    // safe no-interp read) and measure latency vs ITS tcap → the real tracks the input.
                                    // The content-order key is KEPT at pair_c (NOT advanced to the capture index — that
                                    // is a unit error: content_clock/pair_c trail capture by freshage, so anchoring at
                                    // cur_c-1 forces a hard reseat + a freeze). Keeping pair_c means the interp following
                                    // a fresh real steps back to the (stale) pair content = a CONTENT SAWTOOTH (accepted,
                                    // opt-in; the structural freshage gap, fixed only by reducing freshage). Needs --rfp.
                                    // DEFAULT OFF (read only inside the already-default-off --rfp block).
    bool   motion_fallback=false;   // --motion-fallback: frame-level fast-motion fallback. When the per-pair gme
                                    // DISPERSION (dis% = % of MV blocks the global affine fails on = fast/incoherent/
                                    // disoccluding motion) exceeds mf_disp, present the FRESHEST captured real via the
                                    // dedicated safe bslot path INSTEAD of a strobing-garbage warp (the per-pixel gates
                                    // cannot save a whole-frame breakdown). Reuses the --rfp machinery; auto-enables
                                    // --async-present. DEFAULT OFF (the && short-circuits → byte-identical off).
    float  mf_disp=50.0f;           // --mf-disp: the dispersion bound, a PERCENT in [0,100] (gme dis% units). Above it →
                                    // present a real, not a warp. Default 50 (half the frame off-model = genuine
                                    // breakdown). Clamp [0,100].
    bool  phasefix=true;            // the phase-ladder coverage fix. DEFAULT ON. Without it each pair's presented t_use
                                    // ladder covers only [~0.37, 1.0] then OVER-HOLDS at 1.00 for 4-6 ticks; the lower
                                    // ~37% of phase [0,0.37] is NEVER presented → the content leaps prev(0.0)→0.37 at
                                    // every real-frame boundary. Root cause: the D anchor over-lags relative to the
                                    // present latency, so t_display chronically overshoots the freshest set's fresh edge
                                    // → the !found freeze-at-newest (phase→1.0) fires EVERY pair. ON = D is recomputed to
                                    // land t_display in the INTERIOR of the freshest PUBLISHED set by its MEASURED age
                                    // (freshage_ema = EMA(now − tcap_freshest) sampled feedback-free at set-detect)
                                    // MINUS half a span, AND t_display is capped to the freshest edge so the overshoot-
                                    // freeze cannot fire on a live source. --no-phasefix = the prior D formula
                                    // (byte-identical pacing).
    bool  no_phasefix=false;        // --no-phasefix tracking bool (the prior D anchor + overshoot freeze)
    bool  sync_clock=true;          // --sync-clock: the FRAME-LADDER CADENCE fix (free-running content clock / NCO +
                                    // 2nd-order PLL). DEFAULT ON; --no-sync-clock reverts to the per-pair
                                    // (now−D)/(span·T) phase path (byte-identical). On a LOW-fps jittery source (~21fps
                                    // → 240Hz, ~11.5 ticks/source-frame) the per-pair phase EDGE-SNAPS the content clock
                                    // to jittery arrivals → the non-integer ratio's fractional remainder resolves only
                                    // on discrete ticks → per-pair velocity varies + the START phase JUMPS at pair
                                    // boundaries (a ~7Hz judder). ON = ONE authoritative free-running CONTENT CLOCK in
                                    // SOURCE-FRAME units, advanced each tick by the NCO increment Δ = tick_period_ms /
                                    // T_robust (fractional remainder carried by the double). T_robust = a jitter-robust
                                    // source interval from a 2nd-order PLL (outlier-rejected EMA for the frequency + a
                                    // slow proportional PHASE slew that locks the clock without snapping). The pair phase
                                    // = clamp(content_clock − cur_c, 0, 1), MONOTONE by construction. Scale-invariant:
                                    // everything in ms / source-frame units, no panel-rate constant — correct at 240Hz
                                    // AND 500Hz (the panel just samples content_clock more finely via tick_period_ms).
    bool  wsub=false;               // --wsub: sub-time the wap_warp_present lambda (up/rec/gpu/prs) into the stats line
                                    // ` wsub(...)`. Off by default to keep the line clean; on only for the cost-
                                    // regression bisection.
    std::string csv_path;           // --csv <path>: comprehensive per-present telemetry CSV export (raw + <path>-
                                    // stats.csv). Empty = off. Lock-free ring + low-priority drain thread (NVML/PDH +
                                    // I/O off the present hot path).
    double   run_max_ms=0.0;        // --duration/--exit-after <s> → ×1000: bounded-run WALL-CLOCK cap, ms. On the cap
                                    // the present-loop deadline guard sets the SAME g_quit Ctrl-C/vk_live set, so the
                                    // existing clean teardown runs (no new exit machinery). 0 = unbounded → the guard
                                    // short-circuits before any clock read → byte-identical.
    uint64_t run_max_frames=0;      // --max-frames <N>: bounded-run PRESENT cap (counts total_frames, the canonical
                                    // present counter). 0 = unbounded → the guard short-circuits → byte-identical.
    bool  tiers=true;               // the pressure-tier escalation. DEFAULT ON. Builds on the bwd-skip hysteresis (tier
                                    // 1): once F is in the skip regime AND still over the per-pair arrival budget, tier
                                    // 2 runs the object-holon + memory work every 2nd pair only (the slot tables
                                    // tolerate a skipped pair — advection covers it); tier 3 (deeper deficit) runs them
                                    // every 4th pair. The tier rides the SAME t_pair_ema vs pair_budget_ms signal the
                                    // bwd-skip latch already measures (no new EMA, no new clock). ` tier:N` in the stats
                                    // line when >0. --no-tiers = bwd-skip only; objects/memory run every pair.
    bool  no_tiers=false;           // --no-tiers tracking bool
    bool  deficit_tier=true;        // --deficit-tier: the recovery for the high-fps throughput regression (cons<arr).
                                    // Adds a tier-4 ABOVE the tier ladder: under SUSTAINED deficit (t_pair_ema >
                                    // 1.10×budget) it sheds object_repair + scene-memory on EVERY pair (not every-4th),
                                    // keeping gme-fit + matte + warp + inertia — i.e. it reverts to the raw-flow WAP,
                                    // ONLY while over budget. Hysteresis de-escalates to tier 3 at <0.65×budget (below
                                    // the SHED pair ~0.8×budget, so the shed STICKS); full hallucination quality returns
                                    // the instant the source eases. Byte-identical when not in deficit (tier 4 is
                                    // unreachable unless this flag is set). DEFAULT ON; --no-deficit-tier disables — the
                                    // shed-quality-under-load trade. ` tier:4` in the stats line when engaged.
    bool  load_governor=true;       // --load-governor: a DEEPER work-shed (tier-5) ABOVE the deficit tier-4, for the
                                    // combat frame-multiplier COLLAPSE (4090 pegs 99% + gme dissidence 99% → the
                                    // F-thread's per-pair CPU tail blows the source budget → it consumes ~half the
                                    // captured pairs → the FG multiplier falls toward ~1× — it stops generating). Tier-4
                                    // sheds object_repair + scene-memory; tier-5 sheds MORE so F keeps up: also forces
                                    // BACKWARD optical flow OFF (no bwd pyramid / no bwd gme) and runs the cheapest
                                    // single-pass gme-fit (1 IRLS iter), keeping forward-flow + gme-fit + matte + warp +
                                    // inertia so it STILL generates. Driven by BOTH t_pair_ema vs pair_budget_ms AND the
                                    // live measured 4090 util (g_gpu_a_util, published by the present thread). Hysteresis:
                                    // escalate to 5 at t_pair_ema>1.30×budget (sustained) OR 4090-util>gov_util; de-
                                    // escalate to 4 at <0.70×budget. ` tier:5` in the stats line when engaged. DEFAULT
                                    // ON; --no-load-governor disables. Byte-identical while there is no pressure/high
                                    // util (tier-5 only escalates under combat).
    float gov_util=92.0f;           // --gov-util F: the measured 4090 (device A) utilization threshold (percent, clamp
                                    // [50,100]) that triggers the tier-5 escalation even when t_pair_ema has not yet
                                    // crossed 1.30×budget (the GPU pegs before the CPU EMA reacts). Default 92. Only read
                                    // when load_governor is set.
    bool  async_present=true;       // --async-present: decouple the WAP per-tick present from the warp GPU fence (the
                                    // DLSS-G / FSR3 pattern). OFF: vkQueueSubmit→vkWaitForFences(UINT64_MAX) on fBridge
                                    // blocks the present thread on the 4090 every tick; under 4090 saturation that wait
                                    // IS the latency. ON: a SECOND bridge slot (texture+import+cmd+fence) is allocated;
                                    // the warp submits non-blocking, the present thread polls the in-flight fence with
                                    // vkGetFenceStatus, presents the freshest COMPLETED slot, and DROPS this tick's
                                    // interpolated frame when the warp is still running (≤1 warp in flight, ~1 tick of
                                    // pipeline latency). DEFAULT ON; --no-async-present restores the synchronous blocking
                                    // path (byte-identical). If slot-1 creation fails the init force-OFFs → clean
                                    // degrade, never crashes. Flags that AUTO-enable it: --target-output-fps, --fdrop,
                                    // --upload-xfer, --rfp/--real-fast-path, --motion-fallback, --shallow-queue.
    bool  shallow_queue=false;      // --shallow-queue: collapse the async-present ~1-frame depth to ~0 on HEADROOM
                                    // ticks. After the warp submit, a BOUNDED non-blocking re-poll (cap sq_budget_us) of
                                    // THIS tick's fence — if the 4090 finished the warp within budget, promote+present it
                                    // THIS tick instead of next. A budget MISS falls through to the one-behind present
                                    // (byte-identical). NEVER an unbounded wait (vkGetFenceStatus + a hard wall-clock
                                    // cap, NOT vkWaitForFences). --async-present exists BECAUSE the 4090 overruns a tick
                                    // in combat, so the hit rate (sq:H/M in the stats) is HIGH in mid-motion, ~0 in
                                    // saturated combat → a mid-motion responsiveness refinement; it can inject 0/1-tick
                                    // depth jitter. Implies --async-present. Crash-class. DEFAULT OFF (byte-identical).
    int   sq_budget_us=350;         // --shallow-queue-budget-us: hard cap (µs) of the early-promote poll. 0 = inert.
                                    // clamp [0,4000] (a 240Hz tick is ~4167µs → always a fraction of a tick).
    bool  fdrop=false;              // --fdrop: present-side EXACT-DUPLICATE frame discriminator (WAP path). On a tick
                                    // whose (pair,cand_k) == the last DELIVERED (pair,cand_k) — the backwards-clamp ⇒
                                    // the present would re-warp a byte-identical frame — DROP it: skip the warp record/
                                    // submit (zero 4090 cost) and re-present the completed front slot. A make-space /
                                    // useful-frame-density lever (elides a provably redundant warp). IMPLIES
                                    // --async-present (the drop route reuses the re-present-front path; on the sync path
                                    // the present lives inside the lambda and the drop is not byte-safe). DEFAULT OFF →
                                    // byte-identical.
    float fdrop_quiet_ms=0.0f;      // soft near-duplicate quiet-floor — PARSED, logic not yet wired (the soft single-
                                    // stream arm only drops frames the eye sees as a HOLD → stutter doubling with no
                                    // replacement; it sounds only atop a real 2nd candidate). 0.0 = inert.
    float fdrop_k=0.0f;             // motion sensitivity for the soft arm (parsed, not yet wired). 0.0 = inert.
    bool  upload_xfer=false;        // --upload-xfer: move the per-pair WAP upload off the contended graphics queue A.q
                                    // onto a TRANSFER/async-compute queue A.qT (the 4090's parallel DMA engine) so the
                                    // ~10ms of copies overlap the game+warp graphics work instead of host-blocking the
                                    // present thread. Implies --async-present. CONCURRENT images (no QFOT) + two TIMELINE
                                    // semaphores (no CPU mutex) order upload→warp + the warp→upload WAR back-edge.
                                    // DEFAULT OFF → upload stays on A.q with the sync wait, byte-identical. Force-off if
                                    // no transfer family.
    bool  gme_irls2=false;          // --gme-irls2: gme_fit_affine runs 2 IRLS passes (OLS + 1 Geman-McClure reweight)
                                    // instead of 3, shaving ~0.3-0.7ms off EVERY pair on the dominant CPU axis (the
                                    // iter-2 reweight is marginal; iter-1 does most of the robustification). DEFAULT OFF
                                    // (3 passes = byte-identical).
    bool  gme_gpu=true;             // --gme-gpu: offload gme_fit_affine (the per-pair affine IRLS fit, ~2.7ms of CPU
                                    // that taxes the game) onto device B (the 1080 Ti, where the MV/SAD optical-flow
                                    // output already lives). 3 compute shaders chained in ONE command buffer on B:
                                    // gme_reduce (normal-eq accumulation) → gme_solve (3×3 Cramer) ×3 IRLS iters →
                                    // gme_dissidence (R8 mask). Only the final 6-float model is read back (for
                                    // object_repair); the dis-mask is GPU-produced and flows to A's warp AND to the CPU
                                    // for object_repair. DEFAULT ON (bit-identical to the CPU fit: model rel-diff ~1e-6,
                                    // 0/32400 dis-mask flips). AUTO CPU FALLBACK: if device B is absent OR the B-side
                                    // bridge/pipeline init fails, use_gme_gpu is cleared and the CPU gme_fit_affine
                                    // (byte-identical) runs instead — never a crash. --no-gme-gpu forces the CPU path.
    bool  gme_gpu_verify=false;     // --gme-gpu-verify: run BOTH the GPU gme AND the CPU gme on the same pair, print the
                                    // model rel-diff + dis-mask flip count (the real-GPU precision re-measure). Implies
                                    // --gme-gpu.
    bool  mv_subpel=true;           // --mv-subpel: SUB-PIXEL motion-vector refinement in the pillar matcher
                                    // (OpticalFlowPipeline init mv_subpel). The hierarchical search resolves an INTEGER
                                    // best_mv/8x8 tile; ON, the finest level fits a 1D parabola to the pure SAD at
                                    // best_mv±1/axis and adds the guarded, ±0.5-clamped vertex → a FRACTIONAL MV the WAP
                                    // warp consumes (bilinear float-field sample). ~4 extra pure-SAD blocks at the finest
                                    // level only → DEFAULT ON; --no-mv-subpel reverts to the exact integer best_mv
                                    // (subpel==0 store).
    bool  mv_candsel=false;         // --mv-candsel: at the finest match level, an AMBIGUOUS textureless-interior /
                                    // aperture tile ADOPTS the COHERENT coarse region-predictor MV instead of its noisy
                                    // local best (the region holon OFFERS, the tile holon ADOPTS iff its local match is
                                    // not confidently better: best_sad < sad_pred·(1−0.12)). Only the ambiguous
                                    // interiors the symmetric blend crossfades (not always-warp — that hallucinates
                                    // geometry). Composes with --mv-subpel (candsel selects the integer source, subpel
                                    // refines it). One extra pure-SAD + compare at the finest level only → DEFAULT OFF =
                                    // byte-identical (the candsel==0 store is the exact integer best_mv).
    bool  fg_prebake=false;         // --fg-prebake: PRE-BAKE the matcher/warp descriptor sets at init (one collection
                                    // per ping-pong parity) so record_optical_flow SKIPS the per-pair
                                    // vkUpdateDescriptorSets BURST (~41 host driver calls/record, fwd+bwd ~82/pair) — a
                                    // host-CPU relief on the F-thread under GPU saturation. ADDITIVE, default OFF = the
                                    // per-record update path runs as before (byte-identical). Eligible ONLY on the FG
                                    // default (fg_variant active, --no-ambig, no affine); else inert.
    bool  obj_fill_rim=false;       // --obj-fill-rim: COHERENT-MV INTERIOR INFILL for a RIGID object. The object-holon
                                    // stamps its single slot MV across the WHOLE silhouette INCLUDING the rim band
                                    // (depth-gated), the unreached blocks, and the >static "spurious" annulus where
                                    // aperture-problem flow landed on disc@prev/@next — so A_samp and B_samp ALIGN and
                                    // the symmetric blend cannot double-expose the disc (the half-moon crescent dies).
                                    // RIGIDITY-gated on rim_spread<kObjRimSpreadMin → stands down on a scaling/rotating/
                                    // articulated rim. DEFAULT OFF (byte-identical when off). CPU-only, runtime ~0.
    bool  fsub=false;               // --fsub: split the F per-pair time into ` fsub(flow:F pair:F cpu:F)` ms EMAs. flow
                                    // = the BLOCKING fwd fence wait (the 1080 Ti pyramid GPU leg); pair = the full
                                    // F-pair iteration (t_pair_ema, both fits); cpu = pair − flow = the CPU fit/objects/
                                    // memory that runs SERIALLY after the wait. Off by default → the stats line is byte-
                                    // unchanged; on only for the measurement.
    bool  fwd_pipeline=false;       // --fwd-pipeline: forward-pass cross-pair pipelining (the throughput lever). DEFAULT
                                    // OFF — F-loop concurrency, not app-bench-verifiable; the committed default is the
                                    // byte-identical serial WAP path. When ON (WAP path only): pair N's fwd flow is
                                    // submitted NO-WAIT to its own ping-pong cmd buffer/fence (cmdB_fwd[2]/fB_fwd[2]);
                                    // the per-pair CPU tail (gme fit / mem / objects / matte / refresh) + the F→P publish
                                    // are DEFERRED one pair, so pair N-1's CPU runs while pair N's GPU match executes →
                                    // per-pair F time collapses from flow+cpu (serial) to ~max(flow,cpu). N-1's CPU reads
                                    // ONLY hostX[gen_N-1] (durable after N-1's fwd fence, gen_N-1 ≠ gen_N → no torn read
                                    // of N's in-flight buffers) and mutates the process-wide state (persist/mem_*/obj_*)
                                    // in strict pair order. The bwd pass shares the single ofp + the 2 Bframe slots with
                                    // the fwd match, so a do_bwd pair CANNOT overlap (it drains to the serial path for
                                    // that pair only); the field config (bwd-skip:100%) never hits that drain.
    bool  fwd_prestage=false;       // --fwd-prestage: collapse the F per-pair build gap. On the serial WAP + iGPU-
                                    // convert path the hRP_b[s]→hRP_b_dev[s] device PCIe copy is recorded INLINE into
                                    // cmdF and waited on by the SAME blocking flow submit, so it sits in front of the
                                    // flow compute on EVERY pair (the ~11ms non-compute B-side ingest). ON: that copy is
                                    // split into a dedicated double-buffered cmd buffer (cmdF_pre[2]/fF_pre[2]) and
                                    // submitted NO-WAIT on the F flow lane (A.q2) BEFORE cmdF, so it overlaps the flow-
                                    // record CPU work + the ring-guard spin; cmdF then skips the inline copy but keeps
                                    // the TRANSFER→COMPUTE barrier on hRP_b_dev[s] (the same-queue cross-submission
                                    // memory edge). Build ~20→~9ms → freshage_ema falls → BOTH interp latency AND the D
                                    // lead fall, no flow-quality cost. Gated to the serial path (fwd_pipeline already
                                    // overlaps the same copy). Needs the iGPU-convert path; a no-op + force-off otherwise.
                                    // DEFAULT OFF → the copy stays inline in cmdF, byte-identical.
    int   flow_scale=1;             // --flow-scale N: flow-resolution DRS — run the optical flow AND the whole per-pair
                                    // F-thread CPU tail on a 1/N-resolution MV block grid (N ∈ {1,2,4}), cutting the
                                    // per-pair cost ~N². At N>1 the two captured pair frames are bilinear-downsampled
                                    // into WW/N×WH/N scratch images before the flow; ofp.init is sized WW/N×WH/N so
                                    // motion_width()/motion_height() scale and the host MV/SAD bridges + the WAP sampled
                                    // grids + the CPU tail all auto-scale off ofp.motion_width(). The WARP shader is
                                    // untouched (it fetches the MV by textureSize() + normalized UV → a smaller grid
                                    // auto-upsamples bilinearly; the warp dispatches over FULL output res so output stays
                                    // full-res). WAP-only: the non-WAP grid mode + the primary-FG (ofpA) path consume
                                    // Cinterp full-res and are NOT downscaled, so N>1 is rejected with
                                    // --no-warp-at-presenter. DEFAULT 1 = byte-identical (no blit, ofp.init at WW×WH).
    int   warp_scale=1;             // --warp-scale N (N ∈ {1,2,4}): run OUR warp pass (the WAP synthesis) at WW/N×WH/N
                                    // and upsample the warp output (wapOutA) to the bridge/present extent via the
                                    // existing linear blit. DISTINCT from flow_scale: flow_scale cheapens the MOTION
                                    // estimate (the MV grid); warp_scale cheapens the SYNTHESIS (the per-output-pixel
                                    // warp work, which keys off imageSize(u_output)). The shader is UNTOUCHED (a smaller
                                    // u_output makes the dispatch domain smaller; the same full-res inputs are sampled at
                                    // a coarser uv stride). NO game cap / NO game downscale: the divisor applies ONLY to
                                    // wapOutA (ours) + the blit src; the captured pair-reals (wPrev/wCur) stay FULL-RES.
                                    // Startup pick (init-sized), not runtime re-init. WAP-only. DEFAULT 1 = byte-
                                    // identical (warp_div==1 ⇒ today's code; the off-path SPIR-V is unchanged).
    bool  flow_scale_auto=false;    // --flow-scale auto: the OPTIONAL ADAPTIVE quality-vs-latency knob (drives
                                    // flow_scale_target_mp). At init (the flow pipeline is init-sized → startup pick)
                                    // choose the LOWEST flow_div (best quality) whose flow-work pixels fit the target —
                                    // resolution-adaptive (flow cost ~pixel-bound). Runtime per-scene re-pick + GPU-
                                    // capability scaling of the target are follow-ups.
    float flow_scale_target_mp=2.1f;// --flow-scale-target-mp: the auto target in MEGAPIXELS of flow-work (default 2.1 =
                                    // ~1080p stays div=1 full-quality on a 4090; 1440p/4K auto-scale down). The
                                    // quality<->latency slider. Clamp [0.25,12].
    bool  nvofa=false;              // --nvofa: replace the classical block-match FLOW provider with the NVIDIA hardware
                                    // Optical Flow Accelerator (VK_NV_optical_flow) on the 4090 — off the compute cores
                                    // (<3% under a pegged compute load = the contention relief). Crash-class (touches
                                    // device creation) → EVERYTHING is --nvofa-gated; the default path is byte-identical.
                                    // The OFA produces only flow + an 8-bit cost: sad_best = a CALIBRATED cost remap, and
                                    // sad_zero (the change-gate the OFA does NOT provide) is a SEPARATE |A-B| reduction
                                    // pass (see nvofa_convert.comp). If the device lacks VK_NV_optical_flow → auto-disable
                                    // (fall back to the classical OFP). WAP-path only. DEFAULT OFF.
    float nvofa_cost_scale=0.5f;    // --nvofa-cost-scale: OFA cost(0..255)→sad_best remap. PLACEHOLDER — needs eye-
                                    // calibration vs the OFP confidence gate (residual_ceil in [0,192]).
    float nvofa_sadz_scale=4.0f;    // --nvofa-sadz-scale: input-block SUM|A-B| → WW_flow 8x8 SUM magnitude. The pixel-
                                    // count ratio (8*8)/(blk*blk) is ~4 at flow_div==1; PLACEHOLDER, eye-calibration.
    // ── DERIVED / RESOLVED STATE (computed by resolve_config, NOT parsed) ──────
    // The SINGLE source of truth for cross-layer resource predicates. Layers READ these instead of
    // re-deriving gates by hand: the iGPU contour field provider needs ONE predicate, not a hand-rolled
    // `afill||bg_snap` repeated at the import/create/upload sites while band_xfade (default-on),
    // disoccl_hardpick and mc_on ALSO read the field (binding 11) → under --no-bg-snap those would
    // silently read the 1x1 placeholder. A new field-reading feature registers by adding itself to the
    // `field_to_warp` OR in resolve_config() — the provider/upload/import sites need no edit and cannot
    // drift, because they all read this one predicate.
    struct Derived {
        bool field_to_warp=false;   // the iGPU contour field (binding 11, wapFIELDA) must be bridged to device A.
                                    // = igpu_field && (afill || bg_snap || band_xfade>0 || disoccl_hardpick>0 || mc_on).
                                    // Gates the A-side host import + the wapFIELDA image-create + the per-pair upload.
    } d;
};
// interp-buffer depth — 7 interps per set → N≤8. Memory cost: 2 gens × 7 × ~8.3MB ≈ 116MB host,
// allocated only up to the requested factor (NI_alloc).
static constexpr int kMaxInterp=7;
// auto-N ceiling = the interp-buffer depth (kMaxInterp → N≤8).
static constexpr int kAutoNMax=kMaxInterp+1;

// Referenced by parse_args() (the --obj-fill-rim help text); the rest of the object/contour-holon
// constants block lives in main.cpp (consumed by the warp/flow code).
static constexpr float kObjRimSpreadMin = 2.0f;   // px — rim MV spread that arms inheritance (scaling/rotation)

// CLI entry points (definitions in cli.cpp).
void print_help(const char* a0);
bool parse_args(int argc, char** argv, Config& c);
// The central resolver.
// apply_cascades: the order-independent dependency cascades (the post-parse normalization) PLUS the
//   derived `c.d.*` predicates. Idempotent → re-callable from a runtime degrade (main.cpp's no-iGPU-
//   convert path) to re-derive after a flag is forced off. The `announce` flag gates the [ra] cascade
//   prints (false on the runtime re-call).
// resolve_config: apply_cascades + the one-time STARTUP self-audit (warns when a flag's documented
//   dependency is absent: disoccl_commit w/o matte, the diagnostics inert under async, plain --rfp).
//   Called once at the end of parse_args.
void apply_cascades(Config& c, bool announce=true);
void resolve_config(Config& c, bool announce=true);
// Made with my soul - Swately <3