#pragma once
// PhyriadFG cli/ layer (STEP 2 of the layered separation — PURE RELOCATION from
// src/core/main.cpp; no logic change). Holds the master Config struct + its enums,
// the cli compile-time constants, and the print_help/parse_args declarations.
// The bodies live in cli.cpp. main.cpp #includes this instead of defining them.
#include <cstdint>
#include <cstddef>
#include <string>

// ─── Config ──────────────────────────────────────────────────────────────────
enum CaptureApi { CA_DD, CA_WGC };
enum FgGpu     { FG_AUTO, FG_PRIMARY, FG_ASSIST };    // STAGE-26b: FG routing
enum ConvertGpu { CG_IGPU, CG_PRIMARY };               // STAGE-26c-1: where to run convert+pack
enum OutputClock { OC_TIMER };       // STAGE-45b: timer is the only clock (OC_OFF/OC_FIFO died with the legacy swapchain)
struct Config {
    int   cap_mon=0, pres_mon=-1;
    float res_ceil=32.f, conf_improv=0.20f, agreement=0.05f;
    int   fg_factor=2;
    bool  no_upscale=false, lanczos=false;
    char  assist_gpu[128]={};
    bool  list_only=false;
    bool  pin_threads=true;         // FIX #1 (arc-audit): --pin pins C/F/P to fixed cores (P RT-elevated) via the topology pillar. DEFAULT OFF — MEASURED (Halo@480Hz) it does NOT fix the >monitor slip (that is GPU/fence-budget-bound: iter~1.9ms vs 2.08ms tick, NOT CPU-placement) and default-ON slightly raised it. It is an opt-in SCALABILITY tool for HYBRID CPUs (avoid E-core landings) / NUMA. OFF = the OS-scheduled bare threads (byte-identical). PART-A: ahora DEFAULT-ON (--no-pin desactiva; el "DEFAULT OFF" histórico de arriba precede al flip — decisión del operador: protección por defecto).
    int   pin_test=0;               // THREAD_PROTECTION S0 (--pin-test N): ablation lever-select, READ ONLY inside the if(cfg.pin_threads) paths (the --pin flag) → fully inert without --pin (byte-identical-off; --pin-test 0 == today's --pin). 0=FULL(pin C/F/P + elevate P TIME_CRITICAL + elevate F HIGHEST = current --pin, byte-identical default); 1=NO-FLOW-RT(pin C/F/P + elevate P only; F pinned-NOT-elevated = the pre-S1 state); 2=PRIO-ONLY(elevate P+F; NO pin/affinity on C/F/P); 3=AFFINITY-ONLY(pin C/F/P; NO elevation); 4=NEITHER(no pin, no elevate = same as no --pin, the sanity row). Cleanly attributes the S1 win to PRIORITY vs AFFINITY and tests the F-elevate B(1080Ti)→5% root-cause hypothesis under the reproducible FG collapse. THREAD_PROTECTION S2: 5=MMCSS-COMPOSITE(the DATA-REFINED policy from the ablation — P HARD-pin + MMCSS 'Pro Audio'/CRITICAL; F+C SOFT-affinity(set_thread_ideal_processor, migratable — NOT hard pin, the affinity-trap R7) + MMCSS 'Capture' HIGH/NORMAL; each thread falls back to elevate_thread_rt when its MMCSS token is inactive). Composes PIN-FULL's present-floor with PRIO-ONLY's B/latency.
    bool  pin_test_set=false;       // 2026-06-28 layer-sync R6: --pin-test was given EXPLICITLY. Distinguishes
                                    // an explicit `--pin-test 0` (the FULL ablation sanity row) from the default 0,
                                    // so fg_protect's mode-5 promotion honours the explicit choice instead of
                                    // silently overriding it (the old `pin_test==0` sentinel collided with both).
    bool  fg_protect=true;          // THREAD_PROTECTION S3 (--fg-protect): the composed FIRST-CLASS flag = the measured fix for the COMBINED GPU99%+CPU100% FG collapse. Composes (REUSES, does not duplicate): (a) pin_threads=true + pin_test=5 (drives C/F/P down the data-validated mode-5 MMCSS-composite policy; honours an explicit --pin-test if one was given); (b) async_present=true (the latency/smoothness backstop — slot-1 provisioned at init, present decouples + drop-interpolated; pin+async measured 22-35ms vs pin-alone 47ms); (c) the GAME_FLOOR core-reservation = the NO-GAME-CAP dogma AS CODE (GAME_FLOOR=max(4,p_cores/2); the hard-pin count — only P today — must satisfy hard_pins <= p_cores-GAME_FLOOR else P demotes to a SOFT ideal-processor hint; the game ALWAYS keeps >= half the P-cores; we NEVER call set_process_affinity/set_process_priority on the game). INERT on >= 8-P-core rigs (P stays hard-pinned); protects <= 4-P-core CPUs. PART-A: ahora DEFAULT-ON (--no-fg-protect desactiva). Su pieza compuesta pin_test=5 la fija el arm --fg-protect; para el caso POR-DEFECTO (flag no pasado) se replica en el bloque post-parse de parse_args. pin_threads/async_present ya son DEFAULT-ON; --no-pin / --no-async-present los desactivan por separado.
    bool  force_single_gpu=false;   // DCAD Phase 2 (--force-single-gpu): drive the SINGLE-GPU degenerate path on a multi-GPU rig (suppress pB+pG → all FG roles on device A). DEFAULT OFF (byte-identical; single_gpu only derives true with this flag OR no 2nd discrete GPU physically present). The collapse + its risks: SINGLE_GPU_COLLAPSE_RISK_REGISTER.md.
    bool  cap_route_probe=false;    // FG_VENDOR_AGNOSTIC D1 (--cap-route-probe): a break-even-style CAPABILITY routing DECISION over the FG's app-local VDevs (has_fp16/has_dp4a/fp16_gflops) + a vendor-NAMED capability manifest at startup. MEASUREMENT/DIAGNOSTIC ONLY — the FG's FIXED A/B/G roles make the decision INERT (it routes NOTHING: A=primary/present, B=flow/gme, G=convert are architecture-forced; the offload AI≈2.5 ≪ crossover ⇒ decline ALWAYS, the const-inlined invariant). Built for COVERAGE (§4-flexibility, operator 2026-06-24): speculative + inert-by-default on the operator's NVIDIA / fixed-role FG. DEFAULT OFF, byte-identical off (it only prints; it changes no A/B/G behaviour). HONEST: this does NOT pretend to route when it does not — see the print.
    bool  present_waitable=false;   // FG_PRESENT_PACING_DESIGN B (--present-waitable): waitable swapchain (SetMaximumFrameLatency(1) + wait-before-present) = composed-overlay jitter reduction (PARTIAL; does NOT reach Independent Flip). DEFAULT OFF (byte-identical).
    uint32_t present_sync=0;        // FG_PRESENT_PACING_DESIGN B (--present-sync N): Present sync interval (0 = present-immediately = current; 1 = pace to the compositor, stops over-presenting). DEFAULT 0 (byte-identical).
    uint32_t present_colorspace=0;  // A3-L1 (--present-colorspace srgb): declare the overlay colorspace (sRGB) so an HDR desktop composites the SDR overlay without washout. 0 = no-op (current). DEFAULT 0 (byte-identical).
    uint32_t present_format=0;      // A3-L4 (--present-fp16 / --hdr): the FP16 scRGB HDR present (FG_HDR_PIPELINE, Tier-2). 0 = the current 8-bit BGRA8 swapchain + BGRA8 bridge (the only present path today). 1 = an FP16 scRGB swapchain (R16G16B16A16_FLOAT + G10_NONE_P709) AND a matching FP16 bridge texture (CR-HDR-2: the producer/consumer format MUST agree or the one CopyResource into the backbuffer is a format-mismatch). SOFT (CR-HDR-1): the surface falls back to BGRA8 if FP16 create fails; a post-create disagreement is a named clean quit (no in-thread rebuild). HDR is INERT without an HDR display + HDR content (coverage/correctness, NOT a default-path win). DEFAULT 0 (byte-identical, DR-HDR-1).
    bool  present_own_window=false; // FG_OPTION_A (--present-own-window): present our OWN opaque borderless flip-model HWND swapchain (Style::OwnWindow) instead of the DComp composition overlay → we BECOME the displayed Independent-Flip plane → demote the game to Composed → pace it down (the LSFG present topology; tests OA-7). CLICK-THROUGH (WS_EX_TRANSPARENT, input still reaches the game) + NON-ACTIVATING + the no-lock-out contract (foreground-yield + watchdog + dxgi_live device-loss exit, RISK OA-1/2/10). DEFAULT OFF → psd.style stays DcompCt (byte-identical). Plan: FG_OPTION_A_{MASTER_PLAN,IMPLEMENTATION_STRATEGIES,RISK_REGISTER}.md.
    bool  indicator=false;          // FG_OPTION_A UX (--indicator): a teal corner mark so the operator SEES PhyriadFG is active. WIP / DEFAULT OFF: the in-frame stamp on the WAP warp path faulted (DEVICE_LOST — that path is crash-sensitive). The SAFE form (warp shader write, or a separate layered click-through window) is pending; the cfg + setup + passthrough copy remain as the scaffold.
    bool  fps_overlay=false;        // FPS-OVERLAY (--fps-overlay): the LSFG-style live "in->out" fps overlay (real-captured fps -> presented fps) drawn TOP-LEFT on Apresent (RGBA8) by a compute RMW INSIDE bridge_present_src — the SAFE present path (NOT the WAP warp path, which faulted DEVICE_LOST). DEFAULT OFF → no overlay barrier/dispatch recorded + Apresent created exactly as today (byte-identical-off). Mirrors apps/minimal_fg's overlay.
    CaptureApi capture_api=CA_DD;
    char  window_substr[256]={};  // non-empty → capture the MONITOR that window is on, via DDA (default; resolved in main()→cap_mon). WGC window-only ONLY with --capture-api wgc. (2026-06-28: comment was stale "WGC CreateForWindow".)
    bool  dpi_probe=false;        // --dpi-probe: read-only diagnostic — logs GetClientRect / GetDpiForWindow / cap_item.Size() / first frame.ContentSize() to diagnose the high-DPI capture-crop. Default off → byte-identical.
    FgGpu    fg_gpu=FG_AUTO;      // STAGE-26b: FG routing (auto|primary|assist)
    ConvertGpu convert_gpu=CG_IGPU; // STAGE-26c-1: iGPU fused convert+pack when available
    bool  igpu_field=true;          // STAGE-G1 (P0): --igpu-field — iGPU image-derived contour field on G.q2 (2nd dispatch after convert). DEFAULT ON; --no-X disables (cascade dependency for bg_snap/band_xfade; auto-disabled below if no iGPU convert path; needs the iGPU convert path).
    bool  igpu_field_verify=false;  // STAGE-G1 (P0): --igpu-field-verify — CPU Sobel oracle vs the GPU field (the D-13 byte gate). Implies --igpu-field.
    uint32_t igpu_field_thr=24;     // STAGE-G1 (P0): occ_class edge-band threshold (Sobel magnitude 0..255).
    bool  afill=false;              // STAGE-A-FILL (P1): --afill — A reads the iGPU field + tints the contour band onto wapOutA in-place (the field VISUALIZER, eye-validate iGPU↔boundary alignment). DEFAULT OFF (byte-identical off; auto-enables igpu_field since A needs the field).
    float afill_strength=0.5f;      // STAGE-A-FILL (P1): --afill-strength — tint visibility weight (0 = byte-identical).
    float afill_edge_norm=0.04f;    // STAGE-A-FILL (P1): --afill-edge-norm — contour-distance→[0,1] normalizer (~1/edge_thr).
    float afill_mv_gate=6.0f;       // STAGE-A-FILL: --afill-mv-gate — gate STILL-PIXELS (idea del operador): |MV|(px) al que el tinte se desvanece a 0; el contorno persiste en zonas quietas bajo movimiento. 0 = sin gate (contorno completo). DEFAULT 6. SOLO el visualizador --afill.
    bool  bg_snap=true;             // STAGE-90 (P2b): --bg-snap — the warp READS the iGPU contour field (binding 11) and at the boundary band snaps BACKGROUND-side MVs to the gme model → kills the optical-flow disocclusion "gravity" (= LSFG's artifact). DEFAULT ON; --no-X disables (auto-enables igpu_field; needs the iGPU convert path + gme).
    float bg_snap_strength=1.0f;    // STAGE-90 (P2b): --bg-snap-strength — the snap weight scale, parse-clamped [0,4] (1=legacy soft, 2-4=progressively hard; the shader clamps w=strength·band·bg to [0,1] so >1 SATURATES the band core). (2026-06-28: comment said [0,1]; the parse clamp is [0,4].)
    float bg_snap_norm=0.04f;       // STAGE-90 (P2b): --bg-snap-norm — contour-distance→[0,1] band normalizer (~1/edge_thr).
    bool  vblend=true;              // STAGE-94 (--vblend): velocity-continuity warp — near a pair's END the warp tilts the effective sample-offset MV toward the NEXT pair's MV (the next-fresher generation, free in the F→P ring) so the boundary velocity transitions smoothly instead of STEPPING (the perceived "pulse"). Endpoint exact (t=1 → result still cur). DEFAULT ON; --no-X disables.
    float vblend_t0=0.6f;           // STAGE-94 (--vblend): --vblend-t0 — phase where the tilt ramp begins (smoothstep(t0,1,t)); clamp [0,0.95]. Read only when vblend on.
    float vblend_strength=0.5f;     // STAGE-94 (--vblend): --vblend-strength — max tilt weight at t=1 (mix fraction toward the next pair's MV); clamp [0,1]. Read only when vblend on.
    bool  vblend_exact=false;   // STAGE-96 (--vblend-exact): EXACT velocity-continuity — pay +1 pair of present lead so the REAL next-pair MV is in the ring, and tilt toward it (not the 2*mv-mv_prev prediction). DEFAULT OFF (adds ~1 source-frame latency). Implies --vblend.
    float band_xfade=1.0f;          // STAGE-95 (--band-xfade): 0 = OFF, >0 = strength. DEFAULT ON (1.0); --no-band-xfade sets 0. In the image-field disocclusion band (bg-side) blend the result toward blend_result (the un-warped cross-fade) = the gravity-CANCELLATION (our push + LSFG's pull average to the crossfade; static bg → clean, object stays sharp). Reuses binding-11 (the iGPU field) + blend_result; no gme model needed. `--band-xfade` sets 1.0, `--band-xfade-strength F` sets it.
    float ts_smooth=0.1f;           // STAGE-97 (--ts-smooth): adaptive temporal smoothing gated to garbage pixels (mask disocclusion artifacts, keep clean sharp). DEFAULT 0.1 (operator eye: aporta sin estorbar; 0.5 over-smooths). `--ts-smooth 0` disables (byte-identical off); `--ts-smooth F` overrides. The garbage-gate confines the blend to low-confidence/disocclusion pixels — a mask for the residual gravity artifacts, NOT the fix for generation big-jumps (that is the multi-candidate arc).
    bool  mc_on=false;              // STAGE-98 (--multicand): scored MULTI-CANDIDATE medoid selection (the "generate + discriminate + SELECT" fix for the generation big-jumps; SOTA-grounded, FG_VFI_PRIOR_ART §11). DEFAULT OFF (opt-in; the edge-gate is novel/unvalidated → validate by eye before flipping). Byte-identical off (the existing soft_gate/hard path runs).
    int   mc_nperturb=2;            // STAGE-98 (--mc-nperturb): speed-perturbed warp candidates added to {warp,A-only,B-only}: 0, 2, or 4 (generate-more). Default 2. Read only when mc_on.
    float mc_perturb=0.15f;         // STAGE-98 (--mc-perturb): speed-perturbation fraction for the perturbed warps (mv·(1±f), ±2f for 4). Default 0.15. Clamp [0,0.5].
    float mc_disp=0.35f;            // STAGE-98 (--mc-disp): candidate-set DISPERSION threshold (mean L1 of the medoid to the others, color scale ~0..1.7). Above it = no consensus → safe crossfade fallback UNLESS on a true edge. Default 0.35. Clamp [0,2].
    float mc_edge=0.5f;             // STAGE-98 (--mc-edge): iGPU Sobel boundary threshold (0..1). edge ≥ this = a TRUE edge → keep the HARD medoid pick (never the linear-blend ghost). Default 0.5 (0 = always hard; ~1 = edge-gate off). Clamp [0,1].
    float disoccl_hardpick=0.f;    // STAGE-116 (--disoccl-hardpick): edge-gated HARD-PICK threshold at the STAGE-48 bidir reveal band. 0 = OFF (byte-identical). >0 = the iGPU Sobel edge threshold above which the round-trip-consistent side is committed FULLY (no soft STAGE-49 blend = no smear/step/crescent); flat interior keeps the soft blend. Needs --bidir (default on) + the iGPU field (default on via bg-snap/band-xfade). Clamp [0,1].
    bool   pace_variance=false;     // STAGE-117 (--pace-variance): STEP 0 of FG_SATURATION_STABILITY — the FSR3 variance-aware moving-average present pacer. target_interval = SMA10(present deltas) − pv_var_factor·stddev − pv_safety_ms, reset on a >100ms hitch (the minus-variance term shrinks the target under jitter so a saturated/erratic GPU can't lock a slow interval then overshoot). Smooths the present-interval CoV-0.65 (the unstable mouse) in the light/stable regime. Pure CPU, thread-P-local; DEFAULT OFF → the fixed refresh_hz grid → byte-identical. Pairs with --async-present (STEP 3) for the saturation collapse.
    double pv_safety_ms=0.75;       // STAGE-117 (--pv-safety): FSR3 safetyMargin (tuning-set-A 0.75ms). Read only when --pace-variance.
    double pv_var_factor=0.1;       // STAGE-117 (--pv-var): FSR3 varianceFactor (tuning-set-A 0.1). Read only when --pace-variance.
    float  target_output_fps=0.f;   // STAGE-119 (--target-output-fps): STEP 2 of FG_SATURATION_STABILITY — the fractional output-rate controller (the cadence/instability fix). 0 = OFF (byte-identical, the passive N). >0 = hold a STEADY SUSTAINABLE output cadence: realized_mult = clamp(target_eff/base_fps, 1, 8) where target_eff = min(this, refresh, sustain_frac·MEASURED-achievable-rate); N_target = realized_mult·span drives the even-grid → the over-production drop refuses to over-command the warp (the anti-windup gate that STOPS the --pace-variance race). Auto-enables --async-present (its natural home). NEVER caps the game (output-side). Engineered for our decoupled-present / warp-throughput-limited case, NOT a copy.
    float  s2_sustain_frac=0.93f;   // STAGE-119 (--s2-sustain): kSustainFrac — target this fraction of the measured achievable rate ("a steady 180 beats an erratic 191"). Read only when --target-output-fps>0.
    bool  asw=false;                // STAGE-91 (P2): --asw — bounded forward EXTRAPOLATION (Oculus-style ASW): when B falls behind and the sync-clock phase overshoots the held pair, the warp projects cur FORWARD along the MV instead of HOLD-at-1 freezing → fills the throughput deficit (the LSFG-killer). DEFAULT OFF (byte-identical off); needs --sync-clock (the content_clock supplies the overshoot). Best with --bg-snap (clean MV).
    float asw_max=1.0f;             // STAGE-91 (P2): --asw-max — the extrapolation bound in PHASE units (overshoot past 1; 1.0 = up to one full source-span forward). Higher = fills deeper deficits but more extrapolation error on direction changes.
    int   cap_slots=0;              // STAGE-92c: --cap-slots N — OVERRIDE the auto-sized capture-ring depth (0 = auto: ~src/10+4 from the --cap-fps ceiling, clamped [4, 32] + a ~768MB mem budget). The ring is RAM frame-slots sized by the source/flow/resolution gap, NOT CPU topology.
    bool  ingest_async=false;      // INGEST-ASYNC (--ingest-async): decouple the DDA capture INGEST so it scales with the
                                   // rig instead of the serial readback+convert chain. OFF (default) = today's strictly
                                   // serial DDA loop, BYTE-IDENTICAL (no worker thread, no raw ring, no readback double-
                                   // buffer). ON = the acquire thread does ONLY AcquireNextFrame→CopyResource+Flush→
                                   // Map(DO_NOT_WAIT, the PREVIOUS slot = readback overlap)→memcpy into a kRawSlots RAW
                                   // host ring→publish raw_seq, and a NEW convert WORKER thread DROP-TO-NEWEST converts the
                                   // freshest raw slot (the existing convert tail verbatim), publishing c_seq PROMPTLY on
                                   // each convert (NO STAGE-85 deferred publish → freshest frame reaches F/P at today's age).
                                   // DDA-only (forced OFF for WGC). On raw-ring alloc failure → forced OFF (falls back to
                                   // serial, never crashes). The `acq=` field in [ra-cap] shows the acquire rate (vs `in=` convert rate).
    bool  dedup=false;             // CAPTURE-DEDUP (--dedup): DROP content-duplicate captured frames. DDA captures the
                                   // desktop at the DWM COMPOSITE rate (= the monitor refresh, e.g. 240Hz) but the game
                                   // renders fewer UNIQUE frames/s → the surplus captures are content-duplicates (the DWM
                                   // re-composited the same game frame). The real UNIQUE capture rate (dd_uniq, the `uniq=`
                                   // readout) is ALWAYS reported regardless of this flag. ON = a duplicate (identical
                                   // sample-hash to the previous frame) is dropped from the pipeline so the FG interpolates
                                   // between TRUE uniques instead of zero-motion dup pairs. DEFAULT OFF (byte-identical: the
                                   // hash + dd_uniq counter still run, but NO frame is skipped → today's pipeline output).
    int   ingest_backlog=3;        // STAGE-111 (--ingest-backlog N): the in-order ingest DRAIN DEPTH — how far behind
                                   // the newest capture F drains IN ORDER (span-1, smooth) before JUMPING to newest
                                   // (span-2, the STAGE-36 "walking tremble", but FRESHER). MEASURED the DOMINANT
                                   // freshage/input-lag floor component: F-pair COMPUTE is only ~8.7ms yet freshage is
                                   // ~36ms — the gap is ~this backlog × T_src (3 frames ≈ 31ms @95fps). Default 3 (= the
                                   // STAGE-92b kIngestBacklog → byte-identical). LOWER = fresher (less input-lag) at the
                                   // cost of more span-2 skips. Clamped [1,kIngestBacklog]; only REDUCES → the torn-read
                                   // static_assert (cap_slots>=kIngestBacklog+1) stays valid (runtime ≤ compile-time max).
    bool  latency_trace=false;     // STAGE-112 (--latency-trace): MEASUREMENT-ONLY pipeline latency decomposition.
                                   // Emits a [lat-trace] line: INVISIBLE(compose,copy — pre-tcap, NOT in our lat) +
                                   // the freshage split (pickup⊇convert, build, derived detect). Each delta is within
                                   // ONE clock (no cross-clock subtraction); compose uses a QPC↔SystemRelativeTime
                                   // delta with an epoch-mismatch guard (→0 if unavailable). Default OFF, byte-identical.
    bool  copy_fence=false;        // STAGE-113 (--copy-fence): event-driven WGC copy-completion pickup — replaces the
                                   // ~350/s Map(DO_NOT_WAIT)+Sleep(1) busy-poll with a D3D11 fence+event wait OFF the
                                   // context (the callback Signals after CopyResource; C waits the event). Needs D3D11.4
                                   // (ID3D11Device5/ID3D11DeviceContext4); if unavailable → forced OFF. Default OFF, byte-identical.
    bool  copy_device=false;       // STAGE-121 (--copy-device): L3 of FG_REALFAST_PATH — run the INVISIBLE WGC staging
                                   // CopyResource (~5.6ms lt_copy_us, the 4090 executing it BEHIND the saturated game's
                                   // queue) on a SEPARATE D3D11 device on the SAME adapter, so the copy queue is decoupled
                                   // from d.dev (the game capture/present device) and OVERLAPS rather than fronting pickup.
                                   // The WGC pool, the staging ring, the copy-fence (STAGE-113), the callback CopyResource,
                                   // and the C-thread Map all run on the 2nd device when on; d.dev/d.ctx when off (a single
                                   // alias-pointer gate cap_dev/cap_ctx). Pre-tcap (NOT in our lat) but perceived. WGC-only;
                                   // if the 2nd device fails to create → FORCED OFF (cap_dev=d.dev → byte-identical). RFP-FR2:
                                   // the slot's t_cap_ms is set ONLY after the DO_NOT_WAIT Map of the completed copy (the
                                   // existing capture-ready gate; best paired with --copy-fence for the explicit edge). Default OFF.
    bool  camera_twarp=false;      // STAGE-114 (--camera-twarp): RESPONSIVENESS extrapolation. Present the interpolated FG
                                   // frame's SAMPLING base shifted FORWARD by the predicted camera motion over the input
                                   // lag (a global UV lead = the gme AFFINE TRANSLATION (gme6[0],gme6[3]) in px / out_size ·
                                   // amt) so the displayed view LEADS the capture → mouse/camera feels synced (compensates the
                                   // external-capture pipeline latency PERCEPTUALLY). PRESERVES the A/B interpolation (whole
                                   // frame samples at uv+cam_lead; store stays at coord) — NOT the cur-only --asw/pc.extrap path.
                                   // OFF or gme invalid → cam_lead=(0,0) → byte-identical. Default OFF.
    float camera_twarp_amt=0.5f;   // STAGE-114 (--camera-twarp-amt): the eye-tunable lead scalar [0,3]. cam_lead =
                                   // (gme6[0],gme6[3])/out_size · camera_twarp_amt. Read only when camera_twarp.
    double sc_phase_gain=0.10;     // STAGE-93 (cadence): --sc-phase-gain — PLL proportional PHASE-correction fraction per new-pair arrival (default 0.10 = the baked kScPhaseGain). RUNTIME OVERRIDE for the cadence-lock sweep; default → byte-identical.
    double sc_freq_alpha=0.05;     // STAGE-93 (cadence): --sc-freq-alpha — PLL FREQUENCY (T_robust) EMA weight per outlier-rejected arrival (default 0.05 = the baked kScFreqAlpha). RUNTIME OVERRIDE; default → byte-identical.
    double sc_reseat_err=4.0;      // STAGE-93 (cadence): --sc-reseat — phase-error threshold (source-frames) above which the loop RE-SEATS vs slews (default 4.0 = the baked kScReseatErr). RUNTIME OVERRIDE; default → byte-identical.
    bool   pace_present=false;     // STAGE-121 (--pace-present): STEP 0 of FG_SATURATION_STABILITY — the metronomic, drift-corrected present pacer. The present MASD (mean-abs-successive-diff of MsBetweenPresents) was measured ~7.1ms vs LSFG ~0.004ms: we OVER-PRESENT with jitter, LSFG releases METRONOMICALLY. The fix KEEPS the absolute output-clock grid (tick_t0 + k·tick_period_ms = the metronome that gives MASD→0 BY CONSTRUCTION when hit) and only SLOWLY SLEWS the grid anchor tick_t0 toward the realized achievable cadence (an FSR3-style SMA10 variance-damped DRIFT signal), bounded ±a fraction of a tick per tick. It COMPOSES WITH the output clock (slews the ANCHOR, never replaces the grid with a per-present anchor) — the inverse of the parked-negative relative-anchor pacer that re-locked to the jittery present instant and raced the decoupled present. Never pushes the effective period below the panel tick (no over-production windup). Pure CPU, thread-P-local. DEFAULT OFF → the fixed refresh_hz grid is unchanged → byte-identical.
    double pp_safety_ms=0.50;      // STAGE-121 (--pp-safety): FSR3-style safetyMargin (ms) of the variance-damped drift target. Read only when --pace-present.
    double pp_var_factor=0.1;      // STAGE-121 (--pp-var): FSR3-style varianceFactor of the drift target. Read only when --pace-present.
    double pp_slew_gain=0.05;      // STAGE-121 (--pp-slew): per-tick fraction of the grid phase error folded into tick_t0 (the 1st-order drift corrector — same family as the --sc-phase-gain 0.10 PLL and the STEP-1 DWM lock 0.05). Small enough to filter present jitter, large enough to absorb slow cadence drift. Clamp [0,0.5]. Read only when --pace-present.
    bool   pace_hard=false;        // FG_PRESENT_TARGET_PACER P1 (--pace-hard): the HARD present-target pacer. The soft --pace-present only SLEWS the grid anchor (drift-corrects the AVERAGE phase) — it never pins an INDIVIDUAL frame, so per-tick scheduler/spin/Present jitter leaks through (the ~4.28ms present-MASD floor vs LSFG ~0.004ms). This composes ABOVE the soft slew: after paced_wait_P lands near the steady-clock tick tgt, a BOUNDED final spin re-expresses tgt in the QPC domain (an explicit local steady↔QPC bridge sampled together, no silent clock-mix) and pins THIS frame to the QPC edge. FREEZE-FLOOR (RISK PP-1): the total hard wait is HARD-CAPPED below one vblank period; a missed/overshot/un-derivable target degrades to the soft path and NEVER blocks. own-window-only (the displayed Independent-Flip plane; composed styles are DWM-jitter-limited regardless) — gated cfg.present_own_window too. Pure CPU, thread-P-local, cold-setup QPC freq. DEFAULT OFF → the hard branch is unreached → byte-identical.
    double ph_spin_ms=2.0;         // FG_PRESENT_TARGET_PACER P1 (--ph-spin): the bounded final-spin budget of --pace-hard (ms). Mirrors paced_wait_P's kSpinMs=2.0 (the efficiency mandate forbids widening the busy-wait). The hard cap on TOTAL hard-wait is one vblank period (tick_period_ms), independent of this. Clamp [0.1,4.0]. Read only when --pace-hard.
    bool   pace_vblank=false;      // FG_PRESENT_TARGET_PACER P3 / HOOK C (--pace-vblank): the VBLANK PHASE-LOCK. --pace-hard pins each frame to ph_tgt+budget, but the grid (tick_t0 + k·tick_period_ms) free-runs on steady_clock with NO phase reference to the actual display vblank — so the flips land on a metronome that is correct in PERIOD but arbitrary in PHASE vs the panel scanout. This aligns the grid ANCHOR tick_t0 to the true vblank phase (DwmGetCompositionTimingInfo → qpcVBlank/qpcRefreshPeriod), so ph_tgt = tick_t0 + k·tick_period_ms lands AT the panel cadence (the path toward an LSFG-class metronome). COMPOSES ABOVE --pace-hard (refines ph_tgt's PHASE; does NOT replace the pin or widen the budget). COLD/periodic query (~once/sec, D-2 — DwmGetCompositionTimingInfo is an API call, never per-tick); the QPC↔steady_clock domains are bridged by ONE paired now_ms()+QPC sample (no silent epoch mix). The phase slew is BOUNDED (a fraction of a tick) so it never jumps tick_t0 past the >4·period re-seat / freeze-floor. Query fail/unavailable → tick_t0 left as-is → degrades to plain --pace-hard, NEVER blocks. own-window-only + requires --pace-hard. Pure CPU on the cold path, thread-P-local. DEFAULT OFF → the query + the tick_t0 adjust never run → byte-identical.
    double pv_lock_gain=0.05;      // FG_PRESENT_TARGET_PACER P3 (--pv-lock-gain): per-query fraction of the vblank phase error folded into tick_t0 (the HOOK-C phase-lock slew — same 1st-order family as --pp-slew 0.05 / --sc-phase-gain 0.10). Small enough that no single query jumps the grid; bounded to ±half a tick anyway. Clamp [0,0.5]. Read only when --pace-vblank.
    bool  sc_select=true;          // STAGE-93 (cadence): --sc-select — the content_clock drives pair SELECTION too (not just phase), so selection and phase read the SAME clock → the warp sweeps a FULL 0→1 per pair (kills the clock-disagreement top-of-ladder truncation + the start-stall, the boundary "pulse"). DEFAULT ON; --no-sc-select reverts to the wall-time t_display scan. Implies --sync-clock (needs the content_clock). NOT the STAGE-85 dead-end: a read-side selector swap, the F/P generation protocol is untouched.
    bool  fg_auto=false;            // STAGE-35-R4: --fg-factor auto — N from measured capacity
    float mv_smooth=0.f;            // STAGE-35-R2: temporal MV EMA alpha (0=off)
    bool  mv_prior=false;           // STAGE-42: temporal MV prior — dual-centre coarsest search on the
                                    // OFP matcher (self-healing on cuts, no detector). OFF = byte-identical.
    int   dump_n=0;                 // STAGE-39b: dump the next N presented frames to frames/ (diagnostic)
    int   objdump_n=0;              // STAGE-69b: dump N pairs' BLOCK GRIDS to frames/ as tiny BMPs (mvw×mvh):
                                    // the post-repair dissidence mask, |MV|·16 gray, persist[] — the F data
                                    // plane made visible (supervisor instrument; --objdump N; gitignored).
    int   pairdump_n=0;             // STAGE-73 instrument: at each WAP pair-advance, dump the TWO full-res
                                    // frames the warp actually receives (hostR[prev_slot]/hostR[cur_slot])
                                    // + print pair_c/prev_cseq/span/cur_c — the warp INPUT plane made
                                    // visible (the operator's old-frames theory, tested directly).
    int   outdump_n=0;              // STAGE-73 instrument: dump N presented WARP OUTPUTS (wapOutA readback
                                    // after the fence, phase t in the filename) — the synthesis plane made
                                    // visible: WHICH pixels/layers paint the artifacts, at WHICH phases.
    int   qdump_n=0;               // §11.4 layer 1 (the FG-quality TEST-FIELD data tap): write ~N held-out
                                    // TRIPLES to qdump_dir/ — each = (wapPrevA=real N, wapOutA=the live FG
                                    // output, wapCurA=real N+2) as raw RGBA8 .rgba + a TRUTH-LESS manifest
                                    // line in the fg_quality_scorer format (NO mid= — live FG has no held-out
                                    // ground truth; the scorer scores crossfade-only). Sampled (every Kth
                                    // present) so ~N triples span the run. DEFAULT 0 = OFF, byte-identical.
    char  qdump_dir[256]={};        // §11.4 layer 1: output dir for --qdump (the .rgba triples + manifest.txt).
    int   phaselog_n=0;             // STAGE-78 instrument: for N consecutive PAIRS, collect the t_use of EVERY
                                    // presented WAP tick of that pair; on pair-advance print one ladder line
                                    // `[ra] phase pair_c=K span=S n=T t=[...]` (two decimals, ≤40 values). The
                                    // fluidity probe: does the presented phase sequence per pair cover [0,1)?
                                    // P-local fixed buffer, zero heap. --phaselog N.
    bool  phase_norm=false;         // STAGE-107 (--phase-norm): the NORMALIZED-N frame ladder. The content-clock
                                    // places each presented frame at its jittery sub-phase → the ~refresh/source
                                    // ticks a pair gets land UNEVENLY (single mid-ticks, clustered-late, 1.00 holds)
                                    // → juddery even when the fps COUNT is right. phase-norm DECOUPLES the displayed
                                    // intra-pair phase from the clock: tick j of a pair → the EVEN grid (j+0.5)/N,
                                    // N = predicted ticks-this-pair = span·T_robust/tick_period (the PLL's freq est).
                                    // The clock still SELECTS the pair (temporal tracking); only the phase is a
                                    // uniform sweep. Residual: N unknown until the pair closes → an N_pred mismatch
                                    // over/undershoots the last step (the irreducible boundary error, clamped [0,1]).
                                    // The definitive normalized-ladder within the refresh/source step ceiling.
                                    // DEFAULT OFF → byte-identical (t_use stays the content-clock phase).
    bool  cphase=false;             // STAGE-110 (--cphase): C1 (velocity-continuous) intra-pair phase-rate reshape.
                                    // --phase-norm evens the tick SPACING but the displayed VELOCITY still STEPS at
                                    // the pair seam (each pair warps at its own constant mv; t resets 1→0, mv switches
                                    // → a first-derivative jump = the residual boundary "pulse"). cphase bends the
                                    // SCALAR t-vs-tick curve t→g(t) so the OPENING slope of a new pair matches the
                                    // CLOSING displayed speed of the prev pair (g'(0)·|mv_new| ≈ |mv_prev|), easing
                                    // back to linear over a short window. It NEVER touches the MV (A/B still converge
                                    // → fine TEXT is safe, unlike --vblend) and the endpoints are EXACT (g(0)=0,g(1)=1
                                    // → ZERO added latency, no present lead). g is a monotone cubic Hermite (slope
                                    // clamped to the Fritsch-Carlson bound → never reverses; the --vblend math bug).
                                    // SENSATION/cadence lever, NOT a latency-number lever. DEFAULT OFF (byte-identical).
    float cphase_ease=0.25f;        // --cphase-ease W∈[0.05,0.5]: phase-width of the opening ease; outside [0,W] g=t.
    float cphase_gain=1.0f;         // --cphase-gain G∈[0,1]: seam-slope match strength (0=linear/off, 1=full match).
    // STAGE-45b: the output clock is unconditionally the timer (the panel's cadence). OC_OFF's
    // non-clock cadence and OC_FIFO's vblank-paced swapchain both died with the legacy present.
    OutputClock output_clock=OC_TIMER;
    int   refresh_hz=240;           // STAGE-39d: output-clock tick rate (overrides the panel Hz)
    int   cap_fps=0;                // STAGE-90: --cap-fps N — throttle WGC capture to N fps (MinUpdateInterval=1/N).
                                    // 0 = default 8ms (~125/s cap). LOW N (e.g. 15) → fewer real frames → the FG
                                    // interpolates MANY MORE frames per real pair → the disocclusion crescent shows
                                    // across many more presented frames = easier to eyeball (the operator's by-eye
                                    // aid). Diagnostic only: does NOT change the FG algorithm, only the source cadence.
    bool  warp_at_presenter=true;   // STAGE-41/45: presenter (A, the bridge owner since STAGE-45)
                                    // re-warps the pair at the per-tick exact phase from B's shipped
                                    // MV+SAD (vs pre-warped interps). OFF = the grid path. DEFAULT ON
                                    // since STAGE-58 (field-proven). --no-warp-at-presenter = grid mode.
    bool  soft_gate=true;           // STAGE-46: continuous warp gate (smoothstep'd confidence +
                                    // agreement + isolated-block suppression) instead of the binary
                                    // per-8×8-block keep/freeze. DEFAULT ON since STAGE-58.
                                    // --no-soft-gate = the binary gates, byte-identical.
    bool  commit_real=true;         // STAGE-47b: commit target = UNWARPED nearest real (kills the
                                    // displaced-fetch edge fringe of the warped commit; the pixel
                                    // drops to source cadence locally). DEFAULT ON since STAGE-58.
    float commit_thresh=0.08f;      // STAGE-47: commit-don't-hedge warp. 0 = the averaging
                                    // path, byte-identical. >0 = per-pixel max-channel-abs-diff test:
                                    // above the threshold the warp commits to the temporally nearest
                                    // sample (kills the ghost eye) and the gated fallback uses the
                                    // nearest real frame. DEFAULT 0.08 since STAGE-58.
                                    // --no-commit sets commit_thresh=0 + commit_real=false.
    bool  appearance=true;          // STAGE-64: appearance-band temporal re-blend on committed pixels.
                                    // DEFAULT ON. Lives INSIDE the commit SOFT band (wc>0.0): a committed
                                    // (one-sided) pixel whose two anchors at the committed fetch position
                                    // agree on STRUCTURE but differ mildly in TONE (max-channel gap inside
                                    // appear_band) is re-blended toward the phase-t color — the
                                    // brightness-constancy violator (a pulsing/shadowed surface). ONE extra
                                    // texture tap (the other anchor at the same position), committed pixels
                                    // only. Pushed 1.0 iff commit live (commit_thresh>0) AND this — commit
                                    // governs (--no-commit takes it down, it lives in the commit paths).
                                    // --no-appearance = byte-identical (the committed value is unchanged).
    float appear_band=0.10f;        // STAGE-64: the appearance band in max-channel [0,1] color units.
                                    // DEFAULT 0.10. --appear-band F overrides (clamped [0.02,0.5]). Small
                                    // tone gap → full temporal interp; gap ≥ 2·band → 0 (one-sided commit
                                    // stands — a real displacement, the reason commit exists).
    bool  bidir=true;               // STAGE-48: bidirectional flow + occlusion classification. DEFAULT
                                    // ON since STAGE-58. --no-bidir = byte-identical (also disables
                                    // fill-div and matte via cascade). WAP-path only.
    float occl_thresh=1.5f;         // STAGE-48: fwd↔bwd round-trip consistency threshold, PIXEL units.
                                    // DEFAULT 1.5 since STAGE-58 (bidir on by default).
                                    // --occl-thresh F overrides (clamped [0.25,8.0]).
    bool  fill_div=false;           // STAGE-50: divergence-directed disocclusion pick. DEFAULT OFF since
                                    // STAGE-75 (the WASH AUDIT verdict, frames/WASH_AUDIT.md): its hard
                                    // side-select by divergence SIGN is unreliable noise on smooth
                                    // gradients → trailing-rim bites the commit then sharpens. The toggle
                                    // matrix isolated it: --no-fill-div alone = clean 6/6+3/3 high-t; every
                                    // other toggle = damage intact. Its disocclusion job is now done with
                                    // CLAIM evidence by 62/72/74 instead of the noisy divergence sign.
                                    // --fill-div re-enables (requires bidir).
                                    // NOTE (matte default flip, this consolidation): an OLDER finding here
                                    // read "the matte partially HID — --no-matte was WORSE". SUPERSEDED — the
                                    // operator's RECENT first-hand test found --no-matte BETTER: the matte's
                                    // two-layer composite was DOUBLING the figure → the crossfade. matte is
                                    // now DEFAULT OFF (see `matte` below); --matte re-enables for A/B.
    float div_eps=0.05f;            // STAGE-50: divergence band, pixels per MV-grid texel. DEFAULT
                                    // 0.05 since STAGE-58 (fill-div on by default).
                                    // --div-eps F overrides (clamped [0.005,1.0]).
    bool  rescue=true;              // STAGE-49: candidate-rescue. DEFAULT ON since STAGE-58. ON = a
                                    // pixel whose primary MV fails the d_ab commit trigger first tries
                                    // the 8 NEIGHBOR block MVs (3x3 fwd field) and, if the best scores
                                    // ≤ commit_thresh, USES that candidate's average and SKIPS commit.
                                    // Requires the commit trigger (commit_thresh>0).
                                    // --no-rescue = byte-identical.
    bool  mv_median=false;          // STAGE-49b: 3x3 component-wise vector-median on the uploaded MV
                                    // field(s) before the warp (A-side, in wap_upload). OFF (default) =
                                    // the field is uploaded unchanged → byte-identical. ON = a stray
                                    // vector that disagrees with its 8 neighbours is replaced by the
                                    // neighbourhood median (F9) — kills the flat-content rim stamps/holes
                                    // photometric verification can't see. WAP-path only (it filters the
                                    // WAP MV images). Filters wapMVBA too when --bidir is active.
    bool  mv_guided=true;           // STAGE-49c: color-guided MV assignment (the operator's contour-
                                    // membership principle). DEFAULT ON since STAGE-58. ON = (1) the
                                    // consensus pass weights each 3x3 vote by COLOR membership in cur_real
                                    // (votes below sim_thresh excluded; <3 cohort → keep own MV) instead
                                    // of the blind median, and (2) the warp's PRIMARY MV fetch becomes a
                                    // bilateral (per-corner color-membership) upsample instead of LINEAR.
                                    // SUPERSEDES --mv-median's blind pass when both given (the median pass
                                    // runs with sim_thresh>0). WAP-path only. --no-mv-guided = byte-identical.
    float mv_sim=0.10f;             // STAGE-49c: color-membership band, max-channel [0,1] units. DEFAULT
                                    // 0.10 since STAGE-58 (mv-guided on by default).
                                    // --mv-sim F overrides (clamped [0.02,0.5]).
    bool  gme=true;                 // STAGE-52: the frame-holon — a global affine motion model fitted per
                                    // pair on the CPU (F thread) from the fwd MV grid by IRLS least squares.
                                    // DEFAULT ON since STAGE-58. ON = (a) a per-block dissidence mask
                                    // (|mv − model|) is computed + shipped to wapDISA (R8) and the %
                                    // dissident is printed; (b) the model is an extra rescue candidate in
                                    // the warp (fast pans beyond the search window); (c) the fill-div
                                    // revealed case samples cur at the model displacement. Requires WAP.
                                    // --no-gme = byte-identical (also disables matte via cascade).
    bool  matte=false;              // STAGE-53: the fluid-matte — boundary-first two-layer compositing.
                                    // DEFAULT OFF since this consolidation (was DEFAULT ON STAGE-58→STAGE-89).
                                    // The operator's RECENT first-hand A/B found --no-matte BETTER: the
                                    // two-layer composite was DOUBLING the figure → the crossfade. ON = the
                                    // warp decides a BINARY matte per pixel from the advected dissidence
                                    // (wapDISA): OBJECT → the existing 41..52 pipeline result stands;
                                    // BACKGROUND → BOTH warp_result AND blend_result become the pure
                                    // global-model layer sample. Requires --gme AND --bidir. OFF cascades the
                                    // whole matte sub-stack off (crescent/travel/contour/obj_crescent/
                                    // member_commit gate on use_matte → matte_push). --matte re-enables it
                                    // (A/B); --no-matte is the now-default (byte-identical to the off path).
    float matte_thresh=0.25f;       // STAGE-53: the dissidence cutoff in R8-normalized units [0,1]. DEFAULT
                                    // 0.25 since STAGE-58 (matte on by default). The R8 store is
                                    // min(255,16·r_px); a sample reads v=16·r_px/255, so 0.25 ↔ r≈4px.
                                    // --matte-thresh F overrides (clamped [0.05,1.0]).
    bool  crescent=true;            // STAGE-62: crescent-directed mask-weighted background fetch. DEFAULT ON.
                                    // Lives INSIDE the matte BACKGROUND branch: ON = a background-classified
                                    // pixel's two model-layer samples (prev/cur) are re-weighted by their
                                    // NON-contamination (the anchored dissidence dis_fwd/dis_bwd the matte
                                    // already computed) so the trail (prev-side object in the trailing crescent)
                                    // and the leading-edge contamination (cur-side object in the leading
                                    // crescent) are down-weighted. Mask-WEIGHTED side fetch (FSR3 form). Pushed
                                    // 1.0 iff use_matte AND this — matte governs (no matte ⇒ no masks ⇒ no
                                    // crescent; the --no-matte/--no-gme/--no-bidir/--no-wap cascades take it
                                    // with them). --no-crescent = the unchanged (1−t,t) background blend
                                    // (byte-identical, the push flag is 0).
    bool  travel=true;              // STAGE-67: the traveling-silhouette occupancy. DEFAULT ON. Lives in the
                                    // matte OCCUPANCY decision: ON = the self crutches (53b's cover for bad
                                    // MVs) FADE with phase (fwd self ·(1−t), bwd self ·t) so the phase-t
                                    // silhouette TRAVELS with the object instead of smearing across the swept
                                    // band — the gather terms (which carry the object MV after STAGE-61/63
                                    // repair) carry the mask at full weight. Kills the residue trail behind a
                                    // mover (the swept band no longer classifies OBJECT for ~75% of the phase).
                                    // The dis_fwd/dis_bwd the crescent reads STAY the full max(self,gather)
                                    // testimonies — only the occupancy decision is faded. Pushed 1.0 iff
                                    // use_matte governs (no matte ⇒ no masks ⇒ no travel; the --no-matte/
                                    // --no-gme/--no-bidir/--no-wap cascades take it down via matte_push).
                                    // --no-travel = the STAGE-59 occupancy lerp of the full testimonies
                                    // (byte-identical, the push flag is 0).
    bool  contour=true;             // STAGE-71: the contour marriage — per-pixel boundary-band arbitration by
                                    // color affinity. DEFAULT ON. Lives at the matte COMPOSITION site: ON = in
                                    // the uncertain band (|matte_occ − thr| < thr) each pixel is arbitrated
                                    // between the OBJECT layer (the 41..64 warp/blend) and the BACKGROUND layer
                                    // (the crescent-weighted bg) by which the time-nearest real at uv resembles
                                    // (alpha_obj = (d_b+eps)/(d_o+d_b+2eps)) — sub-block precision from
                                    // appearance for the contour-band bites/chunks the block-MV warp drags
                                    // across edges. Outside the band the binary !matte_object decision is EXACTLY
                                    // as today. Pushed 1.0 iff use_matte governs (no matte ⇒ no masks ⇒ no band;
                                    // the --no-matte/--no-gme/--no-bidir/--no-wap cascades take it down via
                                    // matte_push). --no-contour = the binary matte composition (byte-identical,
                                    // the push flag is 0).
    bool  obj_crescent=true;        // STAGE-72: the object crescent — anchored-claim side weighting for the OBJECT
                                    // layer (the symmetric piece of STAGE-62, which only ever touched the
                                    // BACKGROUND). DEFAULT ON. ON = for an OBJECT-classified pixel the warp's A/B
                                    // blend upgrades from (1−t,t) to claim-weighted (w_A ∝ (1−t)·c_f, w_B ∝ t·c_b,
                                    // normalized, c_f/c_b the SAME crescent weights from dis_fwd/dis_bwd the matte
                                    // already computed) AND the commit's time-nearest side pick respects the claims
                                    // (a committed fetch from a NON-claiming anchor is the bite — prefer the claiming
                                    // anchor). Fixes the leading-band quarter-displacement bite at fg4 (cur carries
                                    // the object that already arrived); deep-object (c_f≈c_b≈1) reduces to EXACTLY
                                    // (1−t,t). Pushed 1.0 iff use_matte governs (no matte ⇒ no masks ⇒ no claims;
                                    // the --no-matte/--no-gme/--no-bidir/--no-wap cascades take it down via
                                    // matte_push, same as crescent/travel/contour). --no-obj-crescent = the plain
                                    // (1−t,t) object blend + time-nearest commit (byte-identical, the push flag is 0).
    bool  member_commit=true;       // STAGE-79: MEMBERSHIP BEATS THE BLEND (the cross-fade ghost-step fix). DEFAULT
                                    // ON. The warp-vs-blend selection hedges to the (1−t,t) blend on low-texture
                                    // object INTERIORS whose photometric gates (Gate 1 residual/improvement, Gate 2
                                    // agreement) fail — the flat-content blind spot — but post-STAGE-61/63 the field
                                    // inside a tracked silhouette is holon-repaired, so the blend hedge is obsolete
                                    // timidity that cross-fades two fixed positions (the operator's GHOST-STEP: the
                                    // object materializes at its new position while a trace lingers at the old). ON =
                                    // for a pixel inside the object self-mask (max(self_fwd,self_bwd) > matte_thresh —
                                    // the mv-free OBJECT proxy STAGE-77b uses) the blend-fallback pressure is scaled
                                    // down by (1 − member_strength), member_strength = smoothstep(thr,2·thr,self_m);
                                    // soft mode pulls the gate weight toward 1 (pure warp), hard mode selects warp on
                                    // a confident member. EXTREME escape: d_pixel > 3·agreement still blends (genuine
                                    // garbage protection). Pushed 1.0 iff use_matte governs (the self-mask needs the
                                    // dissidence masks; the --no-matte/--no-gme/--no-bidir/--no-wap cascades take it
                                    // down via matte_push, same as crescent/travel/contour/obj-crescent).
                                    // --no-member-commit = the unchanged selection (byte-identical, the push flag is 0).
    bool  commit_default=true;      // STAGE-80: the COMMIT-DEFAULT flip — WARP IS THE DEFAULT (the cross-fade endgame).
                                    // DEFAULT ON. Post-STAGE-79 the FIXED-position cross-fade is the operator's dominant
                                    // residual: wherever the warp-vs-blend gate weight w<1 (silhouette fringes, sub-mask
                                    // text the member boost does not cover, unclustered detail) the photometric confidence
                                    // gates still hedge to blend_result = (1−t)·prev[uv] + t·cur[uv] at FIXED positions, so
                                    // moving nameplate TEXT doubles (two phantom copies fading). LSFG's lesson: COMMIT to the
                                    // warp; the fixed cross-fade is only for EVIDENCED garbage. ON = the warp wins UNLESS the
                                    // post-warp source disagreement d_pixel is genuinely large (soft: w floors at 1−g,
                                    // g=smoothstep(2·agreement,4·agreement,d_pixel); hard: blend only when d_pixel>2·agreement)
                                    // — the photometric gates no longer FORCE a blend on their own. The mbb member boost
                                    // (STAGE-79) composes (it only RAISES w). This is a WARP-LEVEL rule — gated on wap,
                                    // MATTE-INDEPENDENT (a fringe/text pixel outside any matte still benefits); --no-wap kills
                                    // it (no warp). --no-commit-default = the unchanged photometric-fallback selection
                                    // (byte-identical, the push flag is 0).
    bool  onepos=true;              // STAGE-81: ONE POSITION PER PIXEL — the double-exposure collapse (the ghost-step ROOT).
                                    // DEFAULT ON. STAGE-79/80 killed the FALLBACK cross-fade (blend_result) but warp_result is
                                    // ITSELF a two-sample blend (1−t)·A[x−t·mv] + t·B[x+(1−t)·mv]; with imperfect/zero mv the
                                    // two samples land on DIFFERENT content = a double exposure. The nameplate TEXT (too thin
                                    // to cluster, mv=0) is the pure case: the warp's own blend ≡ the cross-fade by identity.
                                    // ON = DISAGREEMENT COLLAPSE: where the two displaced samples agree (d_pixel small) the
                                    // blend stands (identical content, averaging sharpens); where they disagree the heavier
                                    // side paints ALONE (single position). c=smoothstep(agreement,2·agreement,d_pixel); the
                                    // effective A/B weights sharpen toward winner-take-all as c→1. Applied AT the warp_result
                                    // composition site so ALL downstream consumers inherit. blend_result is UNTOUCHED. This is
                                    // a WARP-LEVEL rule — gated on wap, MATTE-INDEPENDENT (like commit-default); --no-wap kills
                                    // it (no warp). --no-onepos = the unchanged two-sample blend (byte-identical, the push
                                    // flag is 0).
    bool  phase_anchor=true;        // STAGE-73: the phase-anchored primary MV (the stale-vector fix). DEFAULT ON.
                                    // ON = the warp's PRIMARY per-pixel MV is anchored to the instant nearest the
                                    // output phase t: at t≤0.35 the prev-anchored forward field (today), at t≥0.65 the
                                    // cur-anchored backward field NEGATED (binding 5), smooth-blended through the centre
                                    // (w_b = smoothstep(0.35,0.65,t)). The fwd field, anchored at PREV, fetches stale
                                    // vectors at high phase (content moved 2-4 blocks past where its vectors live — the
                                    // outdump damage that grows with t); the cur-anchored field is correct where the
                                    // content NOW is. ONE primary site → every consumer (A/B fetch, matte advection,
                                    // crescent geometry, rescue primary) inherits coherently; the round-trip occlusion
                                    // classification is EXEMPT (it compares the raw fwd vs raw bwd fields). Pushed 1.0
                                    // iff use_bidir AND this AND the bwd field is valid this generation (bwd_ok) — i.e.
                                    // exactly the occl_thresh>0 condition (no bwd field ⇒ no anchor; the --no-bidir/
                                    // --no-wap cascades take it down via bwd_push). --no-phase-anchor = mv = the prev-
                                    // anchored forward field only (byte-identical, the push flag is 0).
    bool  ambig=true;               // STAGE-77: the AMBIGUITY channel (the periodic-texture killer). DEFAULT ON.
                                    // ON = B's optical-flow pipeline emits the RUNNER-UP candidate (second-best MV
                                    // + its SAD) per pair (cand_image(), RGBA16F: xy=second MV, z=second SAD); F
                                    // ships it through the per-gen host bridge (hostC2 → hC2_b/hC2_a → wapC2A,
                                    // binding 10) and the warp arbitrates SAD TIES: a BACKGROUND/texture-interior
                                    // pixel where the runner-up is near-tied in SAD but a texture-period away from
                                    // the matcher's pick (periodic textures — striped flags, concentric tunnels —
                                    // resolve ties arbitrarily) takes whichever of best/runner-up is NEARER the
                                    // referee (the gme background model gme_model_mv). OBJECT pixels are EXEMPT
                                    // (their field is already holon-arbitrated). Needs gme (the model referee + the
                                    // dissidence masks for the object exemption) → WAP + bidir + gme; the cascades
                                    // take it down. --no-ambig = the matcher's raw pick (byte-identical, push 0,
                                    // binding 10 placeholder-bound, emit_second_best off → cand_image() not created).
    bool  stasis=true;              // STAGE-56: the stasis layer (the HUD fix). DEFAULT ON since STAGE-58.
                                    // ON = a pixel whose block sad_zero ≤ thresh bypasses ALL motion
                                    // machinery and presents the current real frame directly. Works on ANY
                                    // stack (no flag dependency); zero extra cost. --no-stasis = byte-identical.
    float stasis_thresh=0.50f;      // STAGE-56: the sad_zero cutoff. Units = the pillar's per-8×8-block SUM
                                    // of Σ_chan|A−B| over RGB in [0,1] color units, range [0,192]; ~0 =
                                    // identical block. DEFAULT 0.50 since STAGE-58 (stasis on by default).
                                    // --stasis-thresh F overrides (clamped [0.05,8.0]).
                                    // 8.0 ≈ 0.014 mean per-channel — still visually identical.
    bool  objects=true;             // STAGE-61: the object-holon — F-side connected-component clustering of
                                    // the dissidence masks + a 16-slot temporal-identity table + the
                                    // conservative motion-INHERITANCE repair. DEFAULT ON. ON = inside a
                                    // tracked moving object's scanline-filled silhouette, blocks measuring
                                    // near-static MV (|mv|≤1px while |mv_obj|≥2px — the motion-cancellation
                                    // signature) get the object's MV written back into the half-float MV
                                    // field (hostMV/hostMVB), EXEMPTING high-inertia-persistence blocks
                                    // (persist[]≥128 — the HUD shield). The repaired blocks' dissidence
                                    // bytes are recomputed for mask/field consistency BEFORE the matte mass
                                    // count + the f_pair publish. Requires --gme (it acts on the gme
                                    // dissidence masks). --no-objects = repair count 0 = pre-stage exact
                                    // behavior (the off-proof is by construction).
    bool  shapefield=true;          // STAGE-63: the contour shape-field — a child of --objects. DEFAULT ON.
                                    // ON = the object-holon's inheritance no longer writes the single rigid
                                    // slot MV into every cancellation block; instead the closed silhouette
                                    // contour casts a DISTANCE + FEATURE transform inward (2-pass chamfer,
                                    // weights 3/4, bbox-bounded) so each interior block knows its distance to
                                    // the rim AND its nearest rim block, then inherits mix(nearest-rim MV,
                                    // slot MV, w) with w = clamp(dist/12, 0, 1) — near the contour the rim
                                    // sector wins (rotation/scaling/deformation correct, which the rigid
                                    // single-MV path cannot do), deep interior relaxes to the slot's stable
                                    // EMA mean. The rim band itself (chamfer dist < 3) is NEVER rewritten (the
                                    // operator's stable contour is the instrument). Arming generalizes: arm
                                    // iff |mv_obj_slot| ≥ 2px OR the rim MV spread ≥ 2px (a scaling/rotating
                                    // object can have mean ≈ 0 yet a live rim field). Child of --objects:
                                    // --no-objects takes it down. --no-shapefield alone reverts to STAGE-61's
                                    // rigid single-MV inheritance EXACTLY (the clean A/B — the rigid path is
                                    // preserved as the else-arm, not re-derived).
    bool  inertia=true;             // STAGE-57: the inertia prior (motion-state hysteresis). DEFAULT ON
                                    // since STAGE-58 (the shader's `inertia_thresh>0.0` short-circuits when
                                    // 0). ON = per-MV-block persistence (a uint8 counter accumulated F-side:
                                    // static_now ? min(255,persist+16) : 0) is shipped to an R8 bridge
                                    // (u_persistence) and gates three ambiguity-resolution paths for pixels
                                    // with a STATIC HISTORY (persist≥thresh): the rescue loop rejects fast
                                    // candidate MVs, the bilateral fetch refuses a fast color-chosen corner,
                                    // and the gme model rescue candidate is skipped. WAP-only.
                                    // --no-inertia = byte-identical.
    float inertia_thresh=0.50f;     // STAGE-57: the persistence cutoff. Read in the shader as p =
                                    // texture(u_persistence,uv).r ∈ [0,1] (the R8 store is persist/255, so
                                    // 16/255≈0.063 per sustained-static pair). DEFAULT 0.50 since STAGE-58
                                    // (inertia on by default; ≈ 8 static pairs at +16/pair).
                                    // --inertia-thresh F overrides (clamped [0.1,1.0]).
                                    // 0.5 ↔ ~0.13s of sustained stasis at 60fps source.
    float mass_k=0.5f;              // STAGE-59: the laser mass-conservation feedback gain. The matte's
                                    // phase-t silhouette is now mix(dis_fwd, dis_bwd, t) (the occupancy
                                    // lerp), which can THIN at mid-t. The feedback measures the presented
                                    // matte mass (a GPU workgroup counter) vs the expected mass
                                    // (lerp(m_fwd, m_bwd, t) of the anchored masks) and tunes the matte
                                    // threshold: thr_eff = matte_thresh·(1 + mass_k·err_ema), err =
                                    // (presented − expected)/max(expected,1), err_ema EMA 0.9, thr_eff
                                    // clamped [0.5,2]·matte_thresh. mass low (bites) → thr drops → vessel
                                    // refills; mass high (spill) → thr rises. DEFAULT 0.5. --mass-k F
                                    // overrides (clamped [0,2]; 0 = feedback OFF = the clean lerp only).
    // STAGE-58: --no-* tracking bools — set by the escape-hatch flags; used in the post-parse
    // normalization block to apply cascades regardless of flag order on the command line.
    // NOT runtime feature flags (those are the bools above); these are parse-time markers only.
    bool  no_wap=false;             // --no-warp-at-presenter (cascades all WAP-dependent features)
    bool  no_commit=false;          // --no-commit (cascades rescue + appearance)
    bool  no_appearance=false;      // STAGE-64: --no-appearance (the committed value is not re-blended)
    bool  no_bidir=false;           // --no-bidir (cascades fill-div + matte)
    bool  no_gme=false;             // --no-gme (cascades matte)
    bool  no_objects=false;         // STAGE-61: --no-objects (the object-holon repair off — pre-stage exact)
    bool  no_shapefield=false;      // STAGE-63: --no-shapefield (rigid single-MV inheritance — STAGE-61 exact)
    bool  no_crescent=false;        // STAGE-62: --no-crescent (the matte background blend reverts to (1−t,t))
    bool  no_travel=false;          // STAGE-67: --no-travel (the occupancy reverts to the STAGE-59 full-testimony lerp)
    bool  no_contour=false;         // STAGE-71: --no-contour (the matte composition reverts to the binary !matte_object decision)
    bool  no_obj_crescent=false;    // STAGE-72: --no-obj-crescent (the object A/B blend reverts to (1−t,t) + time-nearest commit)
    bool  no_member_commit=false;   // STAGE-79: --no-member-commit (the warp-vs-blend selection stays unchanged — the cross-fade ghost-step)
    bool  no_commit_default=false;  // STAGE-80: --no-commit-default (the warp-vs-blend selection keeps the photometric fallback — the fixed cross-fade default)
    bool  no_onepos=false;          // STAGE-81: --no-onepos (warp_result reverts to the two-sample blend — the double-exposure ghost)
    bool  disoccl_commit=false;     // STAGE-89: --disoccl-commit — occlusion-aware ONE-SIDED commit replaces the two
                                    // symmetric `w_sum<1e-4` BLEND FALLBACKS (object-layer STAGE-72/81 wa + background-
                                    // layer STAGE-62 bg) that paint the disocclusion crescent / estela. Commits each
                                    // contaminated-band pixel to the single correct side by the raw dis_fwd/dis_bwd
                                    // asymmetry (leading edge → cur owns, trailing edge → prev owns; bg → the less
                                    // contaminated side), softly gated on the asymmetry magnitude so a true tie keeps
                                    // the phase blend (no mis-pick on ambiguity). Dossier F8/F9 (softmax-splat z-order /
                                    // PerVFI asymmetric blend). REUSES dis_fwd/dis_bwd — no new tap; runtime ≈ 0, only
                                    // in the already-rare w_sum<1e-4 branch. Requires --matte (the dissidence evidence).
                                    // DEFAULT OFF — byte-identical when off.
    float onepos_band=1.0f;         // STAGE-82: the collapse-onset scale (the operator's factor-of-2 dial). The
                                    // disagreement-collapse onset = agreement_threshold·band. 1.0 = STAGE-81
                                    // exactly; LOWER = the sub-threshold faint crescents collapse too (each
                                    // halving ≈ halves the residual ghost — the operator's "launch it toward
                                    // infinity"); the NOISE FLOOR is the honest limit (≈0 = winner-take-all on
                                    // genuinely intermediate content → flicker). --onepos-band F clamp [0.05,4].
    bool  expire=true;              // STAGE-65: stigmergy expiration (the operator's principle) — the content-
                                    // decision memories that cross pairs (the holon slot-MV EMA, the mass-
                                    // feedback err EMA) EXPIRE on contradiction (direction reversal / large
                                    // innovation) instead of decaying through it. EMA for noise, expiration for
                                    // contradiction (the asymmetry STAGE-57's inertia already uses). DEFAULT ON.
                                    // --no-expire = pure EMAs (the pre-stage decay — the clean A/B).
    bool  no_expire=false;          // STAGE-65: --no-expire tracking bool
    bool  scene_memory=true;        // STAGE-66: the scene-holon silhouette memory — a persistent CUR-anchored
                                    // dissidence prior advected by the fwd MV field each pair, merged
                                    // max(fresh, decay(prior)) into the masks BEFORE clustering so a cluster's
                                    // identity (and its STAGE-61/63 inheritance) survives a CANCELLATION phase
                                    // (fresh masks go silent → without memory the cluster dies → the matte
                                    // collapses). The STAGE-65 expiration rule guards it (confident background
                                    // evaporates the prior instantly; a silent cancellation-compatible block
                                    // keeps it decayed). DEFAULT ON. --no-memory = the pre-stage exact path.
    bool  no_memory=false;          // STAGE-66: --no-memory tracking bool
    bool  persist_reset=true;       // STAGE-69: MEMBERSHIP BEATS INERTIA. DEFAULT ON. Inside an ARMED moving
                                    // cluster's filled silhouette the persistence counter RESETS: a mover's
                                    // flat interior (|mv|≈0 AND sad_zero≈0 — immeasurable) is INDISTINGUISHABLE
                                    // from a HUD to STAGE-57's counter, latched ≥128 after ~8 pairs inside a
                                    // big object → the shield blocked the 61/63 inheritance → the interior
                                    // stayed pinned at MV=0 → the double-image trail/bites carried since the
                                    // beginning (rep:0-4% = only the fresh entry band ever repaired). The reset
                                    // unblocks the inheritance AND releases the shader inertia gates (hostPER).
                                    // Real HUDs never sit inside an armed cluster (transient mover-overlap
                                    // resets re-latch in ~8 pairs — the accepted trade). --no-persist-reset
                                    // reverts to the pure STAGE-61 shield (the A/B).
    bool  no_persist_reset=false;   // STAGE-69: --no-persist-reset tracking bool
    bool  change_gate=true;         // STAGE-70: THE CHANGE GATE (supervisor, objdump-diagnosed). DEFAULT ON.
                                    // The dissidence masks require CHANGED content: a block with sad_zero <
                                    // kChangeGateSadZ cannot be dissident regardless of its MV — the pyramid
                                    // matcher's HALO (object MVs painted onto flat background tiles around/
                                    // behind every mover, blobs 2-4× the object in the objdump grids) is the
                                    // ROOT of the oldest trail/chunk artifact, upstream of every mask consumer
                                    // (matte/crescent/travel/cluster/memory). --no-change-gate = raw masks.
    bool  no_change_gate=false;     // STAGE-70: --no-change-gate tracking bool
    bool  no_phase_anchor=false;    // STAGE-73: --no-phase-anchor tracking bool (the primary MV stays prev-anchored fwd)
    bool  no_ambig=false;           // STAGE-77: --no-ambig tracking bool (the matcher's raw pick, no candidate arbitration)
    bool  low_d=false;             // STAGE-108 (--low-d): D-anchor span-term floor-trim — cut the input-lag EXCESS
                                    // (the lap-escape-inflated combat D) WITHOUT a present-before-pair-ready freeze.
                                    // Trims only the additive SPAN term of the phasefix D; KEEPS freshage_ema as the
                                    // freeze floor (D >= freshage_ema always → selects a FRESHER pair → less freeze
                                    // risk). REQUIRES phasefix (default); a no-op under --no-phasefix. DEFAULT OFF.
    double lowd_span_frac=0.5;      // STAGE-108: fraction of the span term kept (clamp [0,1]; 1.0 = no trim)
    double lowd_span_cap=1.5;       // STAGE-108: lap-escape growth ceiling for the span term, in source frames (clamp [1,8])
    bool   real_fast_path=false;    // STAGE-109 (--real-fast-path / --rfp): on a tick that already lands at the
                                    // freshest captured REAL (gen_back==0, phase≈1), OVERRIDE the D-selected warp
                                    // pair with the freshest real presented through the DEDICATED async bslot path
                                    // (NEVER the synchronous do_present_P → CR1 use-after-reset). A real needs no
                                    // interpolation pair → no pair-publish wait → minimal D for reals; interp ticks
                                    // keep the full D-anchored cadence (UNCHANGED). Crash-class (Tier-2): see
                                    // REAL_FAST_PATH_RISK_REGISTER.md CR1-5/FR1. IMPLIES --async-present. DEFAULT OFF
                                    // (byte-identical: the override branch is gated on cfg.real_fast_path).
    float  rfp_window=0.15f;        // STAGE-109: phase tolerance for "this tick already ≈ the cur real" (fire only
                                    // when phase_global >= 1 - rfp_window; small + biased to phase~1 so the real
                                    // never displaces a genuinely-distinct mid-pair interp → bounds the cadence cost,
                                    // CR5). clamp [0,1].
    bool   rfp_fresh=false;         // STAGE-110 (--rfp-fresh): --rfp REDESIGN. The shipped --rfp presents the PAIR's
                                    // real (f_pair_slot_a[f_gen], ~freshage=33ms old) → MEASURED NULL (no responsive-
                                    // ness). --rfp-fresh instead presents the FRESHEST CAPTURED real ((cur_c-1)%cap_slots,
                                    // the proven-safe no-interp read) and measures latency vs ITS tcap → the real tracks
                                    // the input (the LSFG responsive-real feel). The content-order key is KEPT at pair_c
                                    // (NOT advanced to the capture index — that is a unit error: content_clock/pair_c
                                    // trail capture by freshage, so anchoring at cur_c-1 forces a hard reseat + a freeze,
                                    // = the operator-observed instability). KEEPING pair_c means the interp following a
                                    // fresh real steps back to the (stale) pair content = a CONTENT SAWTOOTH — an
                                    // ACCEPTED, opt-in visual effect (the freshage gap between fresh reals and delayed
                                    // interp is structural; the clean fix is reducing freshage, a separate arc). Needs
                                    // --rfp. DEFAULT OFF (read only inside the already-default-off --rfp block).
    bool   motion_fallback=false;   // P4 (--motion-fallback): AFMF2-style FRAME-LEVEL fast-motion fallback. When the
                                    // per-pair gme DISPERSION (dis% = % of MV blocks the global affine fails on = fast/
                                    // incoherent/disoccluding motion) exceeds mf_disp, present the FRESHEST captured real
                                    // via the dedicated CR1-safe bslot path INSTEAD of a strobing-garbage warp (the
                                    // per-pixel gates cannot save a whole-frame breakdown). Reuses the --rfp machinery;
                                    // auto-enables --async-present. DEFAULT OFF (the && short-circuits -> byte-identical off).
    float  mf_disp=50.0f;           // P4 (--mf-disp): the dispersion bound, a PERCENT in [0,100] (gme dis% units). Above
                                    // it -> present a real, not a warp. Default 50 (half the frame off-model = genuine
                                    // breakdown). Tune by eye against the dis:NN% stat. Clamp [0,100].
    bool  phasefix=true;            // STAGE-78: the phase-ladder coverage fix (the fluidity frontier). DEFAULT ON.
                                    // The measured defect (steady-state phaselog): each pair's presented t_use ladder
                                    // covers only [~0.37, 1.0] then OVER-HOLDS at 1.00 for 4-6 ticks; the lower
                                    // ~37% of phase [0,0.37] is NEVER presented → the content leaps prev(0.0)→0.37
                                    // at every real-frame boundary (the operator's "jumps at real-frame boundaries").
                                    // Root cause: the D anchor (delay_ema·1.2 + 0.5·span) over-lags relative to the
                                    // measured present latency, so t_display chronically overshoots the freshest set's
                                    // fresh edge → the !found freeze-at-newest (phase→1.0) fires EVERY pair (not only on
                                    // stalls), and the next set is grabbed already ~0.37 into its window. ON = D is
                                    // recomputed to land t_display in the INTERIOR of the freshest PUBLISHED set by its
                                    // MEASURED age (freshage_ema = EMA(now − tcap_freshest) sampled feedback-free at
                                    // set-detect) MINUS half a span, AND t_display is capped to the freshest edge so the
                                    // overshoot-freeze cannot fire on a live source. --no-phasefix = the STAGE-77b D
                                    // formula EXACTLY (byte-identical pacing — the pre-fix ladder).
    bool  no_phasefix=false;        // STAGE-78: --no-phasefix tracking bool (the pre-fix D anchor + overshoot freeze)
    bool  sync_clock=true;          // STAGE-90: --sync-clock — the FRAME-LADDER CADENCE fix (free-running content clock /
                                    // NCO + 2nd-order PLL). DEFAULT ON (Halo-validated 2026-06-14: slip ~0, 0 re-seats); --no-sync-clock reverts to OFF → the per-pair (now−D)/(span·T) phase path runs
                                    // UNCHANGED (byte-identical). The measured defect this targets: on a LOW-fps jittery
                                    // source (~21fps → 240Hz, ~11.5 ticks/source-frame) the per-pair phase EDGE-SNAPS the
                                    // content clock to jittery arrivals (phase = (now−D−pair_t0)/span·T_src is recomputed
                                    // per pair with T_src an arrival-delta EMA). The non-integer 11.5 ratio's fractional
                                    // remainder resolves only on discrete ticks → per-pair velocity varies + the START
                                    // phase JUMPS at pair boundaries (~+0.13–0.22 every ~3 pairs, a ~7Hz judder). Intra-
                                    // pair Δt is already even; the BOUNDARIES are the problem. ON = ONE authoritative
                                    // free-running CONTENT CLOCK in SOURCE-FRAME units, advanced each tick by the NCO
                                    // increment Δ = tick_period_ms / T_robust (fractional remainder carried naturally by
                                    // the double). T_robust = a jitter-robust source interval from a 2nd-order PLL
                                    // (outlier-rejected EMA for the frequency + a slow proportional PHASE slew that locks
                                    // the clock to the source WITHOUT snapping). The pair phase = clamp(content_clock −
                                    // cur_c, 0, 1), MONOTONE by construction. Scale-invariant: everything in ms / source-
                                    // frame units, no panel-rate constant — correct at 240Hz AND 500Hz with this code (the
                                    // panel just samples content_clock more finely via the parametric tick_period_ms).
    bool  wsub=false;               // STAGE-76: --wsub — sub-time the wap_warp_present lambda (up/rec/gpu/prs) into
                                    // the stats line ` wsub(...)`. Off by default to keep the line clean; on only
                                    // for the cost-regression bisection (the warp-lambda 8.5→12.3ms diagnosis).
    std::string csv_path;           // STAGE-100: --csv <path> — comprehensive per-present telemetry CSV export
                                    // (raw + <path>-stats.csv). Empty = off. Lock-free ring + low-priority drain
                                    // thread (NVML/PDH + I/O off the present hot path). See FG_TELEMETRY_PRIOR_ART.md.
    double   run_max_ms=0.0;        // TB-C2 (--duration/--exit-after <s> → ×1000): bounded-run WALL-CLOCK cap, ms.
                                    // The sweep orchestrator bounds a per-config FG run; on the cap the present-loop
                                    // deadline guard sets the SAME g_quit Ctrl-C/vk_live set, so the existing clean
                                    // teardown runs (NO new exit machinery). 0 = unbounded (today's behavior) → the
                                    // guard short-circuits before any clock read → byte-identical. See FG_TESTBENCH §2.
    uint64_t run_max_frames=0;      // TB-C2 (--max-frames <N>): bounded-run PRESENT cap (counts total_frames, the
                                    // canonical present counter). 0 = unbounded → the guard short-circuits → byte-
                                    // identical. → TB-3, TB-6 (FG_TESTBENCH_IMPLEMENTATION_STRATEGIES §2 B1).
    bool  tiers=true;               // STAGE-84: the pressure-tier escalation (the HSR field kill). DEFAULT ON.
                                    // Builds on the STAGE-55 bwd-skip hysteresis (tier 1): once F is in the skip
                                    // regime AND still over the per-pair arrival budget, tier 2 runs the
                                    // object-holon + memory work every 2nd pair only (the slot tables tolerate a
                                    // skipped pair — advection covers it); tier 3 (deeper deficit) runs them every
                                    // 4th pair. The tier rides the SAME t_pair_ema vs pair_budget_ms signal the
                                    // bwd-skip latch already measures (no new EMA, no new clock). ` tier:N` in the
                                    // stats line when >0. --no-tiers = the STAGE-55 behaviour exactly (bwd-skip
                                    // only; objects/memory run every pair regardless of pressure).
    bool  no_tiers=false;           // STAGE-84: --no-tiers tracking bool
    bool  deficit_tier=true;        // STAGE-87: --deficit-tier — the recovery for the 120fps throughput
                                    // REGRESSION (cons<arr; the STAGE-48→66 hallucination-fix CPU tail crossed
                                    // the 8.3ms/120fps budget — see HSR120_THROUGHPUT_DIAGNOSIS §9). Adds a
                                    // tier-4 ABOVE the STAGE-84 ladder: under SUSTAINED deficit
                                    // (t_pair_ema > 1.10×budget — CORRECTED from 1.85× which the ~1.2×budget HSR
                                    // pair never reached) it sheds object_repair + scene-memory on EVERY
                                    // pair (not every-4th), keeping gme-fit + matte + warp + inertia — i.e. it
                                    // reverts to the raw-flow WAP that worked at 120fps pre-STAGE-61, ONLY while
                                    // over budget. Hysteresis de-escalates to tier 3 at <0.65×budget (below the
                                    // SHED pair ~0.8×budget, so the shed STICKS — was a buggy 1.50× that fired on
                                    // the shed pair every iteration); full
                                    // hallucination quality returns the instant the source eases. Byte-identical
                                    // when not in deficit (tier 4 is unreachable unless this flag is set).
                                    // DEFAULT ON; --no-deficit-tier disables — the shed-quality-under-load trade
                                    // was the operator's A/B call and his eyes confirmed (the STAGE-85 precedent
                                    // is now satisfied). ` tier:4` in the stats line when engaged.
    bool  load_governor=true;       // STAGE-101: --load-governor — a DEEPER work-shed (tier-5) ABOVE the
                                    // STAGE-87 deficit tier-4, for the combat frame-multiplier COLLAPSE
                                    // (BF6: 4090 pegs 99% + gme dissidence 99% → the F-thread's per-pair CPU
                                    // tail blows the source budget → it consumes ~40/79 captured pairs → the
                                    // FG multiplier falls to ~1.07× — it stops generating). Tier-4 sheds
                                    // object_repair + scene-memory; tier-5 sheds MORE so F keeps up: also
                                    // forces BACKWARD optical flow OFF (no bwd pyramid / no bwd gme) and runs
                                    // the cheapest single-pass gme-fit (1 IRLS iter), keeping forward-flow +
                                    // gme-fit + matte + warp + inertia so it STILL generates. Driven by BOTH
                                    // t_pair_ema vs pair_budget_ms AND the live measured 4090 util
                                    // (g_gpu_a_util, published by the present thread). Hysteresis: escalate to
                                    // 5 at t_pair_ema>1.30×budget (sustained) OR 4090-util>gov_util;
                                    // de-escalate to 4 at <0.70×budget. The shed-quality-under-load trade is
                                    // the operator's accepted call (same as STAGE-87). DEFAULT OFF → tier-5 is
                                    // UNREACHABLE (pressure_tier capped ≤4) → byte-identical to today.
                                    // ` tier:5` in the stats line when engaged.
                                    // PART-A: ahora DEFAULT-ON (--no-load-governor desactiva; el "DEFAULT OFF →
                                    // tier-5 UNREACHABLE" de arriba precede al flip). Sigue siendo byte-identical
                                    // mientras no haya presión/util alta (tier-5 sólo escala bajo combate).
    float gov_util=92.0f;           // STAGE-101: --gov-util F — the measured 4090 (device A) utilization
                                    // threshold (percent, clamp [50,100]) that triggers the tier-5 escalation
                                    // even when t_pair_ema has not yet crossed 1.30×budget (the GPU pegs before
                                    // the CPU EMA reacts under combat). Default 92. Only read when
                                    // load_governor is set.
    bool  async_present=true;       // STAGE-102: --async-present — decouple the WAP per-tick present from the
                                    // warp GPU fence (the DLSS-G / FSR3 pattern). OFF: today's path —
                                    // vkQueueSubmit→vkWaitForFences(UINT64_MAX) on fBridge blocks the present
                                    // thread on the 4090 every tick; under 4090 saturation (BF6 combat) that
                                    // wait IS the latency. ON: a SECOND bridge slot (texture+import+cmd+fence)
                                    // is allocated; the warp submits non-blocking, the present thread polls the
                                    // in-flight fence with vkGetFenceStatus, presents the freshest COMPLETED
                                    // slot, and DROPS this tick's interpolated frame when the warp is still
                                    // running (≤1 warp in flight, ~1 tick of pipeline latency). DEFAULT OFF →
                                    // slot-1 not allocated, the single-fence blocking path is BYTE-IDENTICAL to
                                    // today. If slot-1 creation fails it falls back to off (never crashes).
                                    // PART-A: ahora DEFAULT-ON (--no-async-present restaura la ruta SÍNCRONA
                                    // bloqueante, byte-identical). El init en main.cpp ya hace force-OFF si la
                                    // creación del slot-1 falla → degrada limpio. Flags que la AUTO-activan si se
                                    // pasan: --target-output-fps, --fdrop, --upload-xfer, --rfp/--real-fast-path,
                                    // --motion-fallback, --shallow-queue (la cascada sigue funcionando).
    bool  shallow_queue=false;      // STAGE-110 (--shallow-queue): collapse the async-present ~1-frame depth to ~0 on
                                    // HEADROOM ticks. After the warp submit, a BOUNDED non-blocking re-poll (cap
                                    // sq_budget_us) of THIS tick's fence — if the 4090 finished the warp within budget,
                                    // promote+present it THIS tick instead of next. A budget MISS falls through to the
                                    // one-behind present (byte-identical). NEVER an unbounded wait (vkGetFenceStatus +
                                    // a hard wall-clock cap, NOT vkWaitForFences → never reintroduces the STAGE-102
                                    // stall). HONEST: --async-present exists BECAUSE the 4090 overruns a tick in combat,
                                    // so the hit rate (sq:H/M in the stats) is HIGH in mid-motion, ~0 in saturated
                                    // combat → this is a mid-motion responsiveness refinement, not a combat lever; it
                                    // can inject 0/1-tick depth jitter (watch present-interval variance). Implies
                                    // --async-present. Crash-class Tier-2. DEFAULT OFF (byte-identical).
    int   sq_budget_us=350;         // --shallow-queue-budget-us: hard cap (µs) of the early-promote poll. 0 = inert.
                                    // clamp [0,4000] (a 240Hz tick is ~4167µs → always a fraction of a tick).
    bool  fdrop=false;              // STAGE-105 (--fdrop): present-side EXACT-DUPLICATE frame discriminator (WAP
                                    // path). On a tick whose (pair,cand_k) == the last DELIVERED (pair,cand_k) — the
                                    // 6992 backwards-clamp ⇒ the present would re-warp a byte-identical frame — DROP
                                    // it: skip the warp record/submit (zero 4090 cost) and re-present the completed
                                    // front slot. A make-space / useful-frame-density lever (elides a provably
                                    // redundant warp), NOT a sensation/latency win (that = the deferred Stage-D
                                    // assistant cascade). IMPLIES --async-present (the drop route reuses STAGE-102's
                                    // re-present-front path; on the sync path the present lives inside the lambda and
                                    // the drop is not byte-safe). DEFAULT OFF → byte-identical.
    float fdrop_quiet_ms=0.0f;      // STAGE-105 Stage-B (DEFERRED): soft near-duplicate quiet-floor — PARSED now,
                                    // LOGIC not yet wired (3 adversarial lenses ruled the soft single-stream arm
                                    // strictly dominated: it can only drop frames the eye sees as a HOLD → STAGE-102
                                    // stutter doubling with no replacement; sound ONLY atop a real 2nd candidate /
                                    // the Stage-D cascade). 0.0 = inert.
    float fdrop_k=0.0f;             // STAGE-105 Stage-B (DEFERRED): motion sensitivity for the soft arm. 0.0 = inert.
    bool  upload_xfer=false;        // STAGE-106 (--upload-xfer): move the per-pair WAP upload off the contended
                                    // graphics queue A.q onto a TRANSFER/async-compute queue A.qT (the 4090's
                                    // parallel DMA engine) so the ~10ms of copies overlap the game+warp graphics
                                    // work instead of host-blocking the present thread. Implies --async-present.
                                    // CONCURRENT images (no QFOT) + two TIMELINE semaphores (no CPU mutex) order
                                    // upload→warp + the warp→upload WAR back-edge. DEFAULT OFF → upload stays on
                                    // A.q with the sync wait, byte-identical. Force-off if no transfer family.
    bool  gme_irls2=false;          // STAGE-87: --gme-irls2 — gme_fit_affine runs 2 IRLS passes (OLS + 1
                                    // Geman-McClure reweight) instead of 3, shaving ~0.3-0.7ms off EVERY pair on
                                    // the dominant CPU axis (the iter-2 reweight is marginal; iter-1 does most of
                                    // the robustification). DEFAULT OFF (3 passes = byte-identical to STAGE-86b);
                                    // flag-gated for the --fsub A/B.
    bool  gme_gpu=true;             // IDEA-1: --gme-gpu — offload gme_fit_affine (the per-pair affine IRLS fit,
                                    // the dominant ~2.7ms CPU cost that taxes the game's CPU) onto device B (the
                                    // 1080 Ti, where the MV/SAD optical-flow output ALREADY lives). 3 compute
                                    // shaders chained in ONE command buffer on B: gme_reduce (normal-eq
                                    // accumulation) → gme_solve (3×3 Cramer) ×3 IRLS iters → gme_dissidence (R8
                                    // mask). Only the final 6-float model is read back (for object_repair); the
                                    // dis-mask is GPU-produced and flows to A's warp (replacing the CPU→hostDIS
                                    // upload) AND to the CPU for object_repair. DEFAULT ON since this consolidation
                                    // (was DEFAULT OFF): verified bit-identical on the real GPU — model rel-diff
                                    // ~1e-6, 0/32400 dis-mask flips. AUTO CPU FALLBACK: if device B is absent OR
                                    // the B-side bridge/pipeline init fails, use_gme_gpu is cleared and the CPU
                                    // gme_fit_affine (the pre-IDEA-1 path, byte-identical) runs instead — never a
                                    // crash. --no-gme-gpu forces the CPU path. The fp32 precision gate is
                                    // GREEN (gme_fit_bench: model rel-diff 5e-05, dis-mask flips ≤5/32400).
    bool  gme_gpu_verify=false;     // IDEA-1: --gme-gpu-verify — run BOTH the GPU gme AND the CPU gme on the same
                                    // pair, print the model rel-diff + dis-mask flip count (the REAL-GPU precision
                                    // re-measure; the bench was a CPU sim). Implies --gme-gpu.
    bool  mv_subpel=true;           // ARC-A LEVER-1b: --mv-subpel — SUB-PIXEL motion-vector refinement in the pillar
                                    // matcher (OpticalFlowPipeline init mv_subpel). The hierarchical search resolves
                                    // an INTEGER best_mv/8x8 tile; ON, the finest level fits a 1D parabola to the pure
                                    // SAD at best_mv±1/axis and adds the guarded, ±0.5-clamped vertex → a FRACTIONAL
                                    // MV the WAP warp already consumes (bilinear float-field sample). Held-out gate
                                    // (fractional-motion zoo): flowdsc DOWN on EVERY motion type — pan −38%, zoom −34%,
                                    // orbit −25%, mixed −19% — with warp-output PSNR up; attacks the crossfade at its
                                    // root (flow-error). ~4 extra pure-SAD blocks at the finest level only → DEFAULT
                                    // ON; --no-mv-subpel reverts to the exact integer best_mv (subpel==0 store).
    bool  mv_candsel=false;         // ARC-A holonic CANDIDATE-SELECTION: --mv-candsel — at the finest match level,
                                    // an AMBIGUOUS textureless-interior / aperture tile ADOPTS the COHERENT coarse
                                    // region-predictor MV instead of its noisy local best (the region holon OFFERS,
                                    // the tile holon ADOPTS iff its local match is not confidently better:
                                    // best_sad < sad_pred·(1−0.12)). NOT always-warp (rejected — hallucinates
                                    // geometry); only the ambiguous interiors that the symmetric blend crossfades.
                                    // Composes with --mv-subpel (candsel selects the integer source, subpel then
                                    // refines it). One extra pure-SAD + compare at the finest level only → DEFAULT
                                    // OFF = byte-identical (the candsel==0 store is the exact integer best_mv).
    bool  fg_prebake=false;         // CANON #12 app-local FORK: --fg-prebake — PRE-BAKE the matcher/warp descriptor
                                    // sets at init (one collection per ping-pong parity) so record_optical_flow
                                    // SKIPS the per-pair vkUpdateDescriptorSets BURST (~41 host driver calls/record,
                                    // fwd+bwd ~82/pair, ~10k/s @125 pairs) — a host-CPU relief on the F-thread under
                                    // GPU saturation (the multiplier-collapse front). ADDITIVE, default OFF = the
                                    // per-record update path runs exactly as before (byte-identical). Eligible ONLY
                                    // on the FG default (fg_variant active, --no-ambig, no affine); else inert. The
                                    // saving is UNCONFIRMED until the host call-count instrument reads it (G2).
    bool  obj_fill_rim=false;       // STAGE-88: --obj-fill-rim — COHERENT-MV INTERIOR INFILL for a RIGID object.
                                    // The object-holon stamps its single slot MV across the WHOLE silhouette INCLUDING
                                    // the rim band (depth-gated), the unreached blocks, and the >static "spurious"
                                    // annulus where aperture-problem flow landed on disc@prev/@next — so A_samp and
                                    // B_samp ALIGN and the symmetric blend (wap_warp.comp warp_result) cannot
                                    // double-expose the disc (the half-moon crescent dies). RIGIDITY-gated on
                                    // rim_spread<kObjRimSpreadMin → stands down on a scaling/rotating/articulated rim.
                                    // DEFAULT OFF (byte-identical when off); the A/B path. CPU-only, runtime ~0.
    bool  fsub=false;               // STAGE-85 (pre-build instrument): --fsub — split the F per-pair time into
                                    // ` fsub(flow:F pair:F cpu:F)` ms EMAs. flow = the BLOCKING fwd fence wait
                                    // (submit_wait @ ~main.cpp:3893 — the 1080 Ti pyramid GPU leg); pair = the
                                    // full F-pair iteration (t_pair_ema, tp0→both fits); cpu = pair − flow = the
                                    // CPU fit/objects/memory that today runs SERIALLY after the wait and that
                                    // STAGE-85 forward-pipelining would HIDE under flow. Confirms the ~6.7/4.7
                                    // split (inferred, never profiled) before the refactor. Off by default → the
                                    // stats line is byte-unchanged; on only for the measurement.
    bool  fwd_pipeline=false;       // STAGE-85: forward-pass cross-pair pipelining (the throughput lever, §6.1).
                                    // DEFAULT OFF — this is F-loop concurrency, NOT app-bench-verifiable; the
                                    // committed default behaviour MUST be the byte-identical serial WAP path, and
                                    // the operator A/B-tests the overlap via this opt-in flag. When ON (WAP path
                                    // only): pair N's fwd flow is submitted NO-WAIT to its own ping-pong cmd
                                    // buffer/fence (cmdB_fwd[2]/fB_fwd[2]); the per-pair CPU tail (gme fit / mem /
                                    // objects / matte / refresh) + the F→P publish are DEFERRED one pair, so pair
                                    // N-1's CPU runs while pair N's GPU match executes → per-pair F time collapses
                                    // from flow+cpu (serial) to ~max(flow,cpu). N-1's CPU reads ONLY hostX[gen_N-1]
                                    // (durable after N-1's fwd fence, gen_N-1 ≠ gen_N → no torn read of N's
                                    // in-flight buffers) and mutates the process-wide state (persist/mem_*/obj_*)
                                    // in strict pair order. The bwd pass shares the single ofp + the 2 Bframe slots
                                    // with the fwd match, so a do_bwd pair CANNOT overlap (it drains to the serial
                                    // path for that pair only — see §6.2 + the F-loop comment); the field config
                                    // (bwd-skip:100%) never hits that drain, so the steady-state overlap is pure.
    bool  fwd_prestage=false;       // STAGE-120 (--fwd-prestage): L2 of the real-fast-path triad — collapse the F
                                    // per-pair build gap (FG_REALFAST_PATH_MASTER_PLAN.md §4.3). On the serial WAP
                                    // + iGPU-convert path the hRP_b[s]→hRP_b_dev[s] device PCIe copy (~WW·WH·3 B)
                                    // is recorded INLINE into cmdF and waited on by the SAME blocking flow submit,
                                    // so it sits in front of the flow compute on EVERY pair (the measured ~11ms
                                    // NON-compute B-side ingest, the build−fsub(8.7) gap). ON: that copy is split
                                    // into a dedicated double-buffered cmd buffer (cmdF_pre[2]/fF_pre[2]) and
                                    // submitted NO-WAIT on the F flow lane (A.q2) BEFORE cmdF, so it overlaps the
                                    // flow-record CPU work + the ring-guard spin already paid; cmdF then skips the
                                    // inline copy but keeps the TRANSFER→COMPUTE barrier on hRP_b_dev[s] (the
                                    // same-queue cross-submission memory edge — RFP-CR2). Target build ~20→~9ms →
                                    // freshage_ema falls → BOTH interp latency AND the D lead fall, NO flow-quality
                                    // cost. Gated to the serial path (fwd_pipeline already overlaps the same copy).
                                    // Needs the iGPU-convert path (the only one with this copy); a no-op + force-off
                                    // otherwise. DEFAULT OFF → the copy stays inline in cmdF, byte-identical.
    int   flow_scale=1;             // coin-3: flow-resolution DRS — run the optical flow AND the whole per-pair
                                    // F-thread CPU tail on a 1/N-resolution MV block grid (N ∈ {1,2,4}), cutting
                                    // the per-pair cost ~N² (O(pixels)). At N>1 the two captured pair frames are
                                    // bilinear-downsampled into WW/N×WH/N scratch images before the flow; ofp.init
                                    // is sized WW/N×WH/N so motion_width()/motion_height() halve/quarter and the
                                    // host MV/SAD bridges + the WAP sampled grids + the CPU tail all auto-scale off
                                    // ofp.motion_width(). The WARP shader is untouched (it fetches the MV by
                                    // textureSize() + normalized UV → a smaller grid auto-upsamples bilinearly;
                                    // the warp dispatches over FULL output res so output stays full-res). WAP-only:
                                    // the non-WAP grid mode + the primary-FG (ofpA) path consume Cinterp full-res
                                    // and are NOT downscaled (flow-scale does not claim to accelerate them), so N>1
                                    // is rejected with --no-warp-at-presenter. DEFAULT 1 = byte-identical to today
                                    // (no blit, ofp.init at WW×WH, every grid recompute == the unscaled WW/8).
    int   warp_scale=1;             // FG_WARP_SCALING Stage W1 (Shape A, init-sized): --warp-scale N (N ∈ {1,2,4}) —
                                    // run OUR warp pass (the WAP synthesis) at WW/N×WH/N and upsample the warp output
                                    // (wapOutA) to the bridge/present extent via the EXISTING linear blit. The heavy-frame
                                    // cost lever DISTINCT from flow_scale: flow_scale cheapens the MOTION estimate (the MV
                                    // grid); warp_scale cheapens the SYNTHESIS (the per-output-pixel warp work, which keys
                                    // off imageSize(u_output) — NOT the flow grid). The shader is UNTOUCHED (a smaller
                                    // u_output makes the dispatch domain smaller; the same full-res inputs are sampled at a
                                    // coarser uv stride). NO game cap / NO game downscale: the divisor applies ONLY to
                                    // wapOutA (OURS) + the blit src; the captured pair-reals (wPrev/wCur) stay FULL-RES.
                                    // STARTUP pick (init-sized, like --flow-scale), NOT runtime re-init (Shape B / W2 is
                                    // out of scope). WAP-only. DEFAULT 1 = byte-identical (warp_div==1 ⇒ every divisor site
                                    // collapses to today's code; the off-path SPIR-V is unchanged).
    bool  flow_scale_auto=false;    // --flow-scale auto: the OPTIONAL ADAPTIVE quality-vs-latency knob (the future
                                    // end-user UI drives flow_scale_target_mp). At init (the flow pipeline is
                                    // init-sized → STARTUP pick, NOT runtime) choose the LOWEST flow_div (best quality)
                                    // whose flow-work pixels fit the target — resolution-adaptive (flow cost ~pixel-bound).
                                    // Runtime per-scene re-pick (re-init at a util/slice threshold) + GPU-capability
                                    // scaling of the target = documented follow-ups (the init-sized-pipeline complexity).
    float flow_scale_target_mp=2.1f;// --flow-scale-target-mp: the auto target in MEGAPIXELS of flow-work (default 2.1 =
                                    // ~1080p stays div=1 full-quality on a 4090; 1440p/4K auto-scale down). The future
                                    // UI exposes this as the user's quality<->latency slider. Clamp [0.25,12].
    bool  nvofa=false;              // STAGE-115 (--nvofa): replace the classical block-match FLOW provider with the
                                    // NVIDIA hardware Optical Flow Accelerator (VK_NV_optical_flow) on the 4090 —
                                    // microbenchmarked 6.8x/4.2x/1.9x faster at 1080p/1440p/4K, off the compute cores
                                    // (<3% under a pegged compute load = the contention relief). Crash-class (touches
                                    // device creation) → EVERYTHING is --nvofa-gated; the default path is byte-identical.
                                    // The OFA produces only flow+an 8-bit cost: sad_best = a CALIBRATED cost remap, and
                                    // sad_zero (the change-gate the OFA does NOT provide) is a SEPARATE |A-B| reduction
                                    // pass (see nvofa_convert.comp). If the device lacks VK_NV_optical_flow → auto-disable
                                    // (fall back to the classical OFP). WAP-path only (the warp-at-presenter flow). DEFAULT OFF.
    float nvofa_cost_scale=0.5f;    // STAGE-115 (--nvofa-cost-scale): OFA cost(0..255)->sad_best remap. PLACEHOLDER — needs
                                    // the operator's eye-calibration vs the OFP confidence gate (residual_ceil in [0,192]).
    float nvofa_sadz_scale=4.0f;    // STAGE-115 (--nvofa-sadz-scale): input-block SUM|A-B| -> WW_flow 8x8 SUM magnitude. The
                                    // pixel-count ratio (8*8)/(blk*blk) is ~4 at flow_div==1; PLACEHOLDER, eye-calibration.
    // ── DERIVED / RESOLVED STATE (2026-06-28 layer-sync; computed by resolve_config, NOT parsed) ──────
    // The SINGLE source of truth for cross-layer resource predicates. Layers READ these instead of
    // re-deriving gates by hand — the cure for the audit's root finding: the iGPU contour field provider
    // hand-rolled `afill||bg_snap` in THREE sites (import/create/upload) while band_xfade (default-on),
    // disoccl_hardpick and mc_on ALSO read the field (binding 11) → under --no-bg-snap those silently read
    // the 1x1 placeholder. THE ANCHOR FOR NEW WORK: a new field-reading feature registers by adding itself
    // to the `field_to_warp` OR in resolve_config() — the provider/upload/import sites need no edit and
    // cannot drift, because they all read this one predicate.
    struct Derived {
        bool field_to_warp=false;   // the iGPU contour field (binding 11, wapFIELDA) must be bridged to device A.
                                    // = igpu_field && (afill || bg_snap || band_xfade>0 || disoccl_hardpick>0 || mc_on).
                                    // Gates the A-side host import + the wapFIELDA image-create + the per-pair upload.
    } d;
};
// STAGE-39a: interp-buffer depth — 7 interps per set → N≤8 (operator: 30→240 = x8).
// Memory cost: 2 gens × 7 × ~8.3MB ≈ 116MB host (was 50MB) — only allocated up to the
// requested factor (NI_alloc). The old [2][3] arrays overflowed at --fg-factor >4 (the
// startup crash right after "[3-thread]").
static constexpr int kMaxInterp=7;
// STAGE-35-R4: auto-N ceiling = the interp-buffer depth (kMaxInterp → N≤8).
static constexpr int kAutoNMax=kMaxInterp+1;

// Relocated from the object/contour-holon constants block in main.cpp because
// parse_args() references it (the --obj-fill-rim help text). The rest of that
// block stays in main.cpp (consumed by the warp/flow code).
static constexpr float kObjRimSpreadMin = 2.0f;   // px — rim MV spread that arms inheritance (scaling/rotation)

// CLI entry points (definitions in cli.cpp). Signatures are verbatim from main.cpp
// (the leading `static` is dropped so the bodies in cli.cpp link from main.cpp).
void print_help(const char* a0);
bool parse_args(int argc, char** argv, Config& c);
// 2026-06-28 layer-sync — THE central resolver (the "decode + control lines" anchor).
// apply_cascades: the order-independent dependency cascades (the post-parse normalization that used to
//   live inline in parse_args) PLUS the derived `c.d.*` predicates. Idempotent → re-callable from a
//   runtime degrade (main.cpp's no-iGPU-convert path) to re-derive after a flag is forced off. The
//   `announce` flag gates the [ra] cascade prints (false on the runtime re-call).
// resolve_config: apply_cascades + the one-time STARTUP self-audit (the "illegal-instruction" check —
//   warns when a flag's documented dependency is absent: disoccl_commit w/o matte, the diagnostics
//   inert under async, plain --rfp). Called once at the end of parse_args.
void apply_cascades(Config& c, bool announce=true);
void resolve_config(Config& c, bool announce=true);
// Made with my soul - Swately <3
