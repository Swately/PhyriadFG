#include "cli/cli.hpp"
// PhyriadFG cli/ layer bodies — print_help + parse_args.
#include <cstdio>
#include <cstring>
#include <cstdlib>

void print_help(const char* a0) {
    std::printf(
        "render_assistant v1 — DXGI capture -> frame-gen -> upscale -> present\n"
        "Default stack is the full pipeline. Minimal invocation: --window X\n"
        "Runs until Ctrl+C or console close.\n\n"
        "Usage: %s [options]\n\n"
        "DEFAULT STACK (all ON):\n"
        "  warp-at-presenter, soft-gate, commit-real (thresh 0.08), bidir (occl 1.5px),\n"
        "  rescue, mv-guided (sim 0.10), gme, mv-subpel, sync-clock, sc-select, vblend, bg-snap, band-xfade,\n"
        "  phasefix, phase-anchor, onepos, commit-default, member-commit, objects, stasis (0.50), inertia (0.50);\n"
        "  conf-improv 0.20, agreement 0.05.  (matte + fill-div are DEFAULT OFF; see --matte / --fill-div)\n\n"
        "ESCAPE HATCHES (--no-* disables a feature; cascades apply regardless of flag order):\n"
        "  --no-warp-at-presenter  Grid mode (no WAP); also disables bidir, fill-div, rescue,\n"
        "                          mv-guided, gme, matte, inertia (all WAP-dependent)\n"
        "  --no-soft-gate          Binary per-block keep/freeze (byte-identical)\n"
        "  --no-commit             commit_thresh=0 + commit_real=false; also disables rescue + appearance\n"
        "  --no-appearance         No appearance-band temporal re-blend on committed pixels (byte-identical)\n"
        "  --no-bidir              No bidirectional flow; also disables fill-div and matte\n"
        "  --no-fill-div           No divergence-directed disocclusion pick\n"
        "  --no-rescue             No neighbor-MV candidate rescue\n"
        "  --no-mv-guided          No color-guided MV (blind median/linear instead)\n"
        "  --no-gme                No global affine motion model; also disables matte\n"
        "  --no-matte              No fluid-matte compositing\n"
        "  --no-stasis             No stasis bypass layer\n"
        "  --no-inertia            No inertia persistence prior\n"
        "  --no-objects            No object-holon clustering + motion-inheritance repair\n"
        "  --no-shapefield         No contour shape-field; rigid single-MV inheritance\n"
        "  --no-crescent           No crescent-directed background fetch (matte bg blend reverts to (1-t,t))\n"
        "  --no-travel             No traveling-silhouette occupancy (matte occupancy uses the full-testimony lerp)\n"
        "  --no-contour            No contour marriage (matte composition reverts to the binary !matte_object decision)\n"
        "  --no-obj-crescent       No object-crescent side weighting (object A/B blend reverts to (1-t,t) + time-nearest commit)\n"
        "  --no-member-commit      No membership-beats-the-blend (warp-vs-blend selection unchanged; the cross-fade ghost-step; child of matte)\n"
        "  --no-expire             No stigmergy expiration (cross-pair EMAs decay through contradictions)\n"
        "  --no-persist-reset      No membership-beats-inertia (HUD shield blocks mover interiors)\n"
        "  --no-change-gate        No changed-content requirement on the dissidence masks (raw halo masks)\n"
        "  --no-memory             No scene-holon silhouette memory (mask is the fresh pair only; child of --objects)\n"
        "  --no-ambig              No second-best candidate arbitration of SAD ties (periodic-texture aliasing; child of --gme)\n"
        "  --no-tiers              No pressure tiers (bwd-skip only; objects/memory every pair under all load)\n"
        "  --load-governor         A util-driven GRADUATED tier FLOOR for the combat multiplier collapse,\n"
        "                          decoupled from the t_pair_ema>budget gate. The live 4090\n"
        "                          util maps to a tier floor (>=gov-util->5 deep-shed, >=gov-util-6->4, >=gov-util-12->3)\n"
        "                          = max over the CPU ladder; sheds optional work (object/memory/bwd/gme-iters) but NEVER\n"
        "                          the core fwd-flow+warp (keep-generating). Hysteresis: raise fast, lower on dwell. DEFAULT ON (--no-load-governor disables)\n"
        "  --gov-util F            4090 util %% = the floor's top band (>=F->5); -6/-12 bands below (clamp [50,100], default 92)\n"
        "  --wsub                  Diagnostic: per-tick warp-lambda sub-timings in the stats line (up/rec/gpu/prs ms)\n"
        "  --fsub                  Diagnostic: F per-pair split fsub(flow:fwd-fence-wait pair:full cpu:pair-flow) ms\n"
        "  --fwd-pipeline          Forward-pass cross-pair pipelining (WAP only; overlaps pair N GPU with pair N-1 CPU; +1-pair publish lag). DEFAULT OFF.\n\n"
        "RUNTIME DEFAULT-ON (proteccion / cadencia; --no-* desactiva, byte-identical-off salvo la politica de hilos):\n"
        "  --pin / --no-pin                       C/F/P pin (P RT-elevado). DEFAULT ON (--no-pin = hilos OS bare, byte-identical)\n"
        "  --fg-protect / --no-fg-protect         thread policy (MMCSS-composite mode-5) + async-present + GAME_FLOOR. DEFAULT ON\n"
        "  --load-governor / --no-load-governor   util-driven tier-5 floor (combat shed). DEFAULT ON\n"
        "  --async-present / --no-async-present    decoupled non-blocking present (drop-interpolated). DEFAULT ON\n\n"
        "NOTE: --no-commit wins over --commit-thresh F if both given (--no-* applied last).\n\n"
        "TUNING KNOBS (value flags; --no-* applied after these if also present):\n"
        "  --conf-improv F       FG gate (b): min SAD improvement fraction (default 0.20)\n"
        "  --agreement F         FG agreement gate: max per-tile d (default 0.05)\n"
        "  --residual-ceil F     FG gate (a): max sad_best (default 32.0)\n"
        "  --commit-thresh F     Override commit threshold (color units, clamped [0.005,0.5])\n"
        "  --appear-band F       Override appearance band (max-ch [0,1], clamped [0.02,0.5], default 0.10)\n"
        "  --occl-thresh F       Override fwd<->bwd round-trip threshold (px, clamped [0.25,8.0])\n"
        "  --div-eps F           Override divergence band (px/texel, clamped [0.005,1.0])\n"
        "  --mv-sim F            Override color-membership band (max-ch [0,1], clamped [0.02,0.5])\n"
        "  --matte-thresh F      Override dissidence cutoff (R8-norm [0,1], clamped [0.05,1.0])\n"
        "  --mass-k F            Matte mass-feedback gain (clamped [0,2]; 0=off/lerp only, default 0.5)\n"
        "  --stasis-thresh F     Override sad_zero cutoff (per-block SUM |A-B|, clamped [0.05,8.0])\n"
        "  --inertia-thresh F    Override persistence cutoff (R8-norm [0,1], clamped [0.1,1.0])\n\n"
        "CAPTURE / ROUTING:\n"
        "  --monitor M           Capture from DXGI output M (default: 0)\n"
        "  --present-monitor P   Present on output P (default: same as capture)\n"
        "  --list-monitors       Print available outputs and exit\n"
        "  --fg-factor N|auto    Output multiplier: 2=2x (default), 3=3x, ... auto=measured\n"
        "  --capture-api API     Capture backend: dd (default) or wgc\n"
        "                        wgc = Windows Graphics Capture (HW-accel flip/overlay; MSVC)\n"
        "  --window SUBSTR       Capture the MONITOR that window is on, via DDA (add --capture-api wgc for window-only)\n"
        "  --fg-gpu GPU          FG device: auto (default = primary-FG enabled; NOT load-aware), primary, assist\n"
        "  --convert-gpu DEV     igpu (default, zero-PCIe fused convert) or primary\n"
        "  --ingest-async        Decouple DDA capture ingest: acquire-only thread (readback-overlap) +\n"
        "                        a convert WORKER thread (drop-to-newest) so ingest scales past the\n"
        "                        serial readback+convert cap. DDA-only; default OFF (byte-identical)\n"
        "  --dedup               Drop content-duplicate captured frames (DDA composites the desktop at\n"
        "                        refresh; the game renders fewer unique frames). FG then interpolates\n"
        "                        between true uniques. Default off\n"
        "  --refresh-hz N        Output-clock tick rate (default 240)\n"
        "  --pace-present        Metronomic drift-corrected present pacer (collapses the present MASD toward a steady metronome). Default off, byte-identical.\n"
        "  --pace-hard           HARD present-target pacer: pin each frame to the QPC edge of tgt (bounded sleep-then-spin) — composes above --pace-present; own-window-only; freeze-floor-capped < one vblank. Default off, byte-identical.\n"
        "  --flow-scale N        DRS: run the flow + per-pair CPU tail on a 1/N-res MV grid\n"
        "                        (N=1 default/byte-identical, 2, or 4; ~N^2 cheaper/pair; WAP-only)\n"
        "  --no-upscale          Present at working resolution (bridge blit scales)\n"
        "  --upscale-lanczos     Lanczos-2 upscale (sharper, slower)\n"
        "  --assist-gpu NAME     GPU name fragment for frame-gen\n"
        "  --mv-smooth A         Temporal MV EMA alpha (0=off; ~0.6 damps tile jitter)\n"
        "  --mv-prior            Temporal MV prior on OFP matcher (dual-centre, self-heals on cuts)\n"
        "  --mv-subpel / --no-mv-subpel  Sub-pixel MV (parabolic SAD-peak) — cuts flow-error/crossfade. DEFAULT ON\n"
        "  --mv-candsel          Ambiguous interior tiles adopt the coarse region MV — kills aperture crossfade. DEFAULT OFF\n"
        "  --matte / --no-matte  Fluid-matte two-layer compositing. DEFAULT OFF (the composite can\n"
        "                        double the figure into a crossfade).\n"
        "                        --matte re-enables it (cascades its sub-stack on; needs --gme + --bidir).\n"
        "  --mv-median           3x3 blind vector-median on WAP MV field\n"
        "  --dump N              Dump next N presented frames to frames\\ as BMP (diagnostic)\n"
        "  --qdump DIR N         FG-quality test-field tap: write ~N held-out triples (real N / live FG /\n"
        "                        real N+2) to DIR\\ as raw RGBA8 .rgba + a truth-less manifest.txt for\n"
        "                        fg_quality_scorer (default off, byte-identical when absent)\n"
        "  --phaselog N          Log the presented t_use ladder for the next N pairs\n"
        "  --sync-clock          Frame-ladder cadence (DEFAULT ON): free-running content-clock NCO + 2nd-order PLL\n"
        "  --no-sync-clock       Disable the cadence fix (per-pair phase pacing)\n"
        "                        (kills the per-pair start-phase JUMP at real-frame boundaries on low-fps jittery\n"
        "                        sources). --no-sync-clock = the per-pair phase (byte-identical). Scale-invariant (240/500Hz)\n"
        "  --gme-gpu / --no-gme-gpu  Offload the affine fit (gme_fit_affine, ~2.7ms/pair) onto device B\n"
        "                        (the 1080 Ti where MV/SAD live) — frees the game's CPU. DEFAULT ON, bit-identical\n"
        "                        (rel-diff ~1e-6, 0/32400 dis-mask flips). Auto CPU-gme fallback\n"
        "                        if device B / the B-side pipeline is unavailable. --no-gme-gpu forces the CPU path.\n"
        "  --gme-gpu-verify      Run BOTH GPU+CPU gme, print model rel-diff + dis-mask flips. Implies --gme-gpu\n"
        "  --output-clock MODE   timer ONLY\n\n"
        "LEGACY NO-OP FLAGS (accepted, already default):\n"
        "  --warp-at-presenter, --soft-gate, --commit-warp, --commit-real, --bidir,\n"
        "  --rescue, --mv-guided, --gme, --stasis, --inertia,\n"
        "  --objects, --crescent, --travel, --contour, --obj-crescent, --member-commit, --appearance, --present-surface, --present-gpu DEV, --overlay\n\n"
        "  --help                This message\n", a0);
}

// ════════════════════════════════════════════════════════════════════════════════════════════════
// THE CENTRAL RESOLVER (the "decode + control lines" anchor)
// apply_cascades: the order-independent dependency cascades PLUS the derived c.d.* predicates. ONE site,
//   idempotent → re-callable from a runtime degrade (main.cpp's no-iGPU-convert path) to re-derive after a
//   flag is forced off. `announce` gates the [ra] cascade prints (false on the runtime re-call to avoid noise).
//   Rule: --no-* flags always WIN (applied here, after all positional flags).
// ════════════════════════════════════════════════════════════════════════════════════════════════
void apply_cascades(Config& c, bool announce) {
    if (c.no_commit) {
        c.commit_thresh=0.f; c.commit_real=false;
        if (c.rescue) {
            if(announce) std::printf("[ra] --no-commit cascade: rescue disabled (needs commit trigger)\n");
            c.rescue=false;
        }
        if (c.appearance) {
            if(announce) std::printf("[ra] --no-commit cascade: appearance disabled (re-blends only committed pixels)\n");
            c.appearance=false;
        }
    } else if (c.rescue && c.commit_thresh<=0.f) {
        if(announce) std::printf("[ra] rescue disabled — commit_thresh=0 (no commit trigger)\n");
        c.rescue=false;
    }
    if (c.no_bidir) {
        c.bidir=false; c.occl_thresh=0.f;
        if (c.fill_div)  { if(announce) std::printf("[ra] --no-bidir cascade: fill-div disabled (needs bidir occlusion classification)\n"); c.fill_div=false; c.div_eps=0.f; }
        if (c.matte)     { if(announce) std::printf("[ra] --no-bidir cascade: matte disabled (dual-anchored matte needs bwd MV field)\n"); c.matte=false; c.matte_thresh=0.f; }
        if (c.phase_anchor) { if(announce) std::printf("[ra] --no-bidir cascade: phase-anchor disabled (the cur-anchored re-anchor needs the bwd MV field)\n"); c.phase_anchor=false; }
    } else if (!c.bidir && c.fill_div) {
        if(announce) std::printf("[ra] fill-div disabled — requires bidir (no occlusion classification)\n");
        c.fill_div=false; c.div_eps=0.f;
    }
    if (c.no_gme) {
        c.gme=false;
        if (c.matte)   { if(announce) std::printf("[ra] --no-gme cascade: matte disabled (needs dissidence mask + global model from gme)\n"); c.matte=false; c.matte_thresh=0.f; }
        if (c.objects) { if(announce) std::printf("[ra] --no-gme cascade: object-holon disabled (the repair acts on the gme dissidence mask)\n"); c.objects=false; }
        if (c.ambig)   { if(announce) std::printf("[ra] --no-gme cascade: ambiguity disabled (its referee is the gme background model)\n"); c.ambig=false; }
    } else if (!c.gme && c.matte) {
        if(announce) std::printf("[ra] matte disabled — requires gme (no dissidence mask or global model)\n");
        c.matte=false; c.matte_thresh=0.f;
    }
    if (c.no_wap) {
        c.warp_at_presenter=false;
        if (c.bidir)     { if(announce) std::printf("[ra] --no-warp-at-presenter cascade: bidir disabled\n");     c.bidir=false; c.occl_thresh=0.f; }
        if (c.phase_anchor) { if(announce) std::printf("[ra] --no-warp-at-presenter cascade: phase-anchor disabled\n"); c.phase_anchor=false; }
        if (c.fill_div)  { if(announce) std::printf("[ra] --no-warp-at-presenter cascade: fill-div disabled\n");  c.fill_div=false; c.div_eps=0.f; }
        if (c.rescue)    { if(announce) std::printf("[ra] --no-warp-at-presenter cascade: rescue disabled\n");     c.rescue=false; }
        if (c.mv_guided) { if(announce) std::printf("[ra] --no-warp-at-presenter cascade: mv-guided disabled\n"); c.mv_guided=false; c.mv_sim=0.f; }
        if (c.gme)       { if(announce) std::printf("[ra] --no-warp-at-presenter cascade: gme disabled\n");       c.gme=false; }
        if (c.ambig)     { if(announce) std::printf("[ra] --no-warp-at-presenter cascade: ambiguity disabled\n"); c.ambig=false; }
        if (c.matte)     { if(announce) std::printf("[ra] --no-warp-at-presenter cascade: matte disabled\n");     c.matte=false; c.matte_thresh=0.f; }
        if (c.inertia)   { if(announce) std::printf("[ra] --no-warp-at-presenter cascade: inertia disabled\n");   c.inertia=false; c.inertia_thresh=0.f; }
        if (c.objects)   { if(announce) std::printf("[ra] --no-warp-at-presenter cascade: object-holon disabled\n"); c.objects=false; }
        if (c.commit_default) { if(announce) std::printf("[ra] --no-warp-at-presenter cascade: commit-default disabled\n"); c.commit_default=false; c.no_commit_default=true; }
        if (c.onepos)    { if(announce) std::printf("[ra] --no-warp-at-presenter cascade: one-position disabled\n"); c.onepos=false; c.no_onepos=true; }
    }
    if (!c.objects && c.shapefield)    { c.shapefield=false; }       // child of --objects
    if (!c.objects && c.scene_memory)  { c.scene_memory=false; }     // child of --objects
    if (c.bidir && c.mv_prior) {                                     // bidir ⊗ mv-prior mutually exclusive
        if(announce) std::printf("[ra] --bidir ignores --mv-prior (alternating fwd/bwd records would poison the temporal prior)\n");
        c.mv_prior=false;
    }
    if (c.flow_scale>1 && !c.warp_at_presenter) {
        if(announce) std::printf("[ra] --flow-scale %d ignored — requires warp-at-presenter (the default); the non-WAP grid mode + primary-FG path consume the interp output full-res. Reverting to flow-scale 1.\n",c.flow_scale);
        c.flow_scale=1;
    }
    if (c.warp_scale>1 && !c.warp_at_presenter) {
        if(announce) std::printf("[ra] --warp-scale %d ignored — requires warp-at-presenter (the default); only the WAP synthesis owns wapOutA + its upscale blit. Reverting to warp-scale 1.\n",c.warp_scale);
        c.warp_scale=1;
    }
    // --no-igpu-field cascade: EVERY consumer that READS the iGPU contour field (binding 11) must go off
    // when the field is absent (else it reads the 1x1 placeholder). disoccl_hardpick edge-gates on the field.
    // mc_on is intentionally LEFT on: its field read degrades SAFELY to crossfade-on-dispersion.
    if (!c.igpu_field) {
        if (c.bg_snap)              { if(announce) std::printf("[ra] --no-igpu-field cascade: bg-snap OFF (lee el campo)\n");                 c.bg_snap=false; }
        if (c.band_xfade>0.f)       { if(announce) std::printf("[ra] --no-igpu-field cascade: band-xfade OFF (lee el campo)\n");              c.band_xfade=0.f; }
        if (c.afill)                { if(announce) std::printf("[ra] --no-igpu-field cascade: afill OFF (visualiza el campo)\n");             c.afill=false; }
        if (c.disoccl_hardpick>0.f) { if(announce) std::printf("[ra] --no-igpu-field cascade: disoccl-hardpick OFF (edge-gates sobre el campo)\n"); c.disoccl_hardpick=0.f; }
    }
    // --disoccl-commit REQUIRES --matte (its dissidence evidence). matte is default-OFF, so without it
    // --disoccl-commit is inert (matte_push false) → cascade it off + warn, like the fill-div/matte siblings.
    if (c.disoccl_commit && !c.matte) {
        if(announce) std::printf("[ra] --disoccl-commit disabled — requires --matte (the dissidence evidence; matte is default-off)\n");
        c.disoccl_commit=false;
    }
    // fg_protect (DEFAULT-ON) selects MMCSS-composite mode-5, but gate on pin_test_set (NOT pin_test==0) so
    // an EXPLICIT `--pin-test 0` (the full ablation sanity row) survives instead of being overridden to mode-5.
    if (c.fg_protect && !c.pin_test_set) c.pin_test=5;
    // ── DERIVED PREDICATES (the single source of truth — providers read c.d.*, never re-derive a gate) ──
    // ANCHOR: a new feature that reads the iGPU field adds itself to THIS OR; the 3 provider sites are done.
    c.d.field_to_warp = c.igpu_field && (c.afill || c.bg_snap || c.band_xfade>0.f || c.disoccl_hardpick>0.f || c.mc_on);
}

// resolve_config = apply_cascades + the ONE-TIME startup self-audit (the "illegal-instruction" check):
// it warns where a flag's documented dependency is absent so a silent no-op is never a surprise.
void resolve_config(Config& c, bool announce) {
    apply_cascades(c, announce);
    if (!announce) return;   // a runtime re-call (main.cpp degrade) wants the cascade+derive only, no audit
    // --outdump/--qdump read wapOutA AFTER the warp fence; the async path does not fence-wait → they
    // record nothing. They are inherently SYNCHRONOUS diagnostics → take the sync path for the run.
    if (c.async_present && (c.outdump_n>0 || c.qdump_n>0)) {
        std::printf("[ra] --outdump/--qdump need the SYNCHRONOUS present path (they read wapOutA after the warp fence); auto-disabling --async-present for this diagnostic run.\n");
        c.async_present=false;
    }
    // --fps-overlay lives only in bridge_present_src (the grid/sync path); the WAP+async default never reaches it.
    if (c.fps_overlay && (c.warp_at_presenter || c.async_present)) {
        std::printf("[ra] WARNING: --fps-overlay only draws on the grid sync path; it is INERT on the default WAP/async path — add --no-warp-at-presenter --no-async-present to see it.\n");
    }
    // plain --rfp is a no-op (the override fires only with --rfp-fresh) — say so, don't imply an effect.
    if (c.real_fast_path && !c.rfp_fresh) {
        std::printf("[ra] note: plain --rfp/--real-fast-path is a no-op (the override gate also needs --rfp-fresh); add --rfp-fresh for the responsive-real effect (it carries a content sawtooth by design).\n");
    }
}

bool parse_args(int argc, char** argv, Config& c) {
    for (int i=1;i<argc;++i) {
        const char* a=argv[i];
        auto next=[&](const char* o)->const char*{ return (i+1<argc)?argv[++i]:(std::printf("[ra] %s needs arg\n",o),nullptr); };
        // These flags live in this SEPARATE matcher (called from the terminal else below) rather than as
        // more `else if` links — the main arg chain is near MSVC's C1061 nested-block limit, and a lambda
        // BODY is a fresh scope (nesting restarts at 0), so this adds ZERO depth to the chain. Returns
        // 0 = matched OK, 1 = matched but missing arg (→ parse fail), -1 = not matched (→ unknown-option path).
        auto parse_extra=[&](const char* arg)->int{
            if(!std::strcmp(arg,"--dedup")){ c.dedup=true; std::printf("[ra] --dedup: descarta frames capturados DUPLICADOS de contenido (DDA compone el escritorio al refresh del monitor; el juego renderiza menos frames únicos). El FG interpola entre únicos verdaderos en vez de pares de movimiento-cero. La tasa única real (uniq=) se reporta SIEMPRE. DEFAULT OFF.\n"); return 0; }   // capture-dedup
            if(!std::strcmp(arg,"--bg-snap")){
                std::printf("[ra] --bg-snap: the warp reads the iGPU contour field (binding 11) and at the boundary band snaps BACKGROUND-side MVs (photometric side: object warps consistent=low d, attracted bg=high d) toward the gme model BEFORE sampling -> kills the optical-flow disocclusion 'gravity' with NO side-pick. Auto-enables --igpu-field (the warp needs the field); needs the iGPU convert path + gme. DEFAULT ON (--no-bg-snap disables).\n");
                c.bg_snap=true; c.igpu_field=true; return 0;
            }
            if(!std::strcmp(arg,"--bg-snap-strength")){ if(auto v=next(arg)){ float f=(float)std::atof(v); c.bg_snap_strength = f<0.f?0.f:(f>4.f?4.f:f); return 0; } return 1; }   // snap weight scale; the shader clamps w=strength·band·bg to [0,1], so >1 SATURATES w→1 in the band core = HARDER snap. Range [0,4]: 1=soft, 2-4=progressively hard.
            if(!std::strcmp(arg,"--bg-snap-norm")){ if(auto v=next(arg)){ float f=(float)std::atof(v); c.bg_snap_norm = f<0.001f?0.001f:(f>1.f?1.f:f); return 0; } return 1; }   // contour-dist->[0,1] band normalizer
            if(!std::strcmp(arg,"--vblend")){
                std::printf("[ra] --vblend: velocity-continuity warp (PREDICT). Near a pair's END (high phase t) the warp tilts the effective A/B sample-offset MV toward the PREDICTED next-pair velocity (2*mv - mv_prev, constant-accel extrapolation from the PREV pair in the F->P ring) so the boundary velocity transitions SMOOTHLY instead of STEPPING (the perceived 'pulse'). Prediction is the no-latency path (exact-lookahead would cost ~1 pair latency). Endpoint exact (t=1 -> result still cur). Only the sample offsets tilt; occlusion classification keeps the RAW fields. DEFAULT ON (--no-vblend disables).\n");
                c.vblend=true; return 0;
            }
            if(!std::strcmp(arg,"--vblend-t0")){ if(auto v=next(arg)){ float f=(float)std::atof(v); c.vblend_t0 = f<0.f?0.f:(f>0.95f?0.95f:f); return 0; } return 1; }   // tilt-ramp onset phase (clamp [0,0.95])
            if(!std::strcmp(arg,"--vblend-strength")){ if(auto v=next(arg)){ float f=(float)std::atof(v); c.vblend_strength = f<0.f?0.f:(f>1.f?1.f:f); return 0; } return 1; }   // max tilt weight at t=1 (clamp [0,1])
            if(!std::strcmp(arg,"--vblend-exact")){ c.vblend_exact=true; c.vblend=true; std::printf("[ra] --vblend-exact: EXACT velocity-continuity (real next-pair MV via +1 pair of present lead; ~+1 source-frame latency). Implies --vblend. DEFAULT OFF.\n"); return 0; }
            if(!std::strcmp(arg,"--band-xfade")){
                std::printf("[ra] --band-xfade: the GRAVITY-CANCELLATION crossfade. On the iGPU image-field disocclusion band, BACKGROUND side, blend the result toward the un-warped (1-t)*prev+t*cur cross-fade. STATIC bg -> clean background (no gravity/no double); the OBJECT keeps its sharp warp (band/bg-gated). Auto-enables --igpu-field (needs the field); no gme model needed. DEFAULT ON (--no-band-xfade disables).\n");
                c.band_xfade=1.0f; c.igpu_field=true; return 0;
            }
            if(!std::strcmp(arg,"--band-xfade-strength")){ if(auto v=next(arg)){ float f=(float)std::atof(v); c.band_xfade = f<0.f?0.f:(f>1.f?1.f:f); c.igpu_field=true; return 0; } return 1; }   // crossfade strength in the band (0=off, clamp [0,1])
            if(!std::strcmp(arg,"--afill-mv-gate")){ if(auto v=next(arg)){ float f=(float)std::atof(v); c.afill_mv_gate = f<0.f?0.f:(f>64.f?64.f:f); return 0; } return 1; }   // gate still-pixels (px; 0=sin gate)
            if(!std::strcmp(arg,"--ts-smooth")){ if(auto v=next(arg)){ float f=(float)std::atof(v); c.ts_smooth = f<0.f?0.f:(f>1.f?1.f:f); std::printf("[ra] --ts-smooth %.2f: adaptive temporal smoothing (blend toward prev output on garbage/low-conf pixels; clean stays sharp). DEFAULT 0.1 (--ts-smooth 0 disables).\n",c.ts_smooth); return 0; } return 1; }
            if(!std::strcmp(arg,"--multicand")){ c.mc_on=true; std::printf("[ra] --multicand: scored MULTI-CANDIDATE medoid selection (generate+discriminate+SELECT). Candidates: warp + A-only + B-only + %d speed-perturbed; medoid consensus, dispersion->crossfade fallback, Sobel edge-gate keeps the hard pick at true edges. DEFAULT OFF.\n",c.mc_nperturb); return 0; }
            if(!std::strcmp(arg,"--mc-nperturb")){ if(auto v=next(arg)){ int n=std::atoi(v); c.mc_nperturb = (n>=4)?4:((n>=2)?2:0); return 0; } return 1; }   // 0/2/4 speed-perturbed candidates
            if(!std::strcmp(arg,"--mc-perturb")){ if(auto v=next(arg)){ float f=(float)std::atof(v); c.mc_perturb = f<0.f?0.f:(f>0.5f?0.5f:f); return 0; } return 1; }   // speed-perturbation fraction (clamp [0,0.5])
            if(!std::strcmp(arg,"--mc-disp")){ if(auto v=next(arg)){ float f=(float)std::atof(v); c.mc_disp = f<0.f?0.f:(f>2.f?2.f:f); return 0; } return 1; }   // dispersion threshold (no-consensus -> crossfade; clamp [0,2])
            if(!std::strcmp(arg,"--mc-edge")){ if(auto v=next(arg)){ float f=(float)std::atof(v); c.mc_edge = f<0.f?0.f:(f>1.f?1.f:f); return 0; } return 1; }   // Sobel edge threshold for the hard-pick gate (clamp [0,1])
            if(!std::strcmp(arg,"--disoccl-hardpick")){ if(auto v=next(arg)){ float f=(float)std::atof(v); c.disoccl_hardpick = f<0.f?0.f:(f>1.f?1.f:f); std::printf("[ra] --disoccl-hardpick %.2f: edge-gated HARD-PICK at the bidir reveal band (round-trip consistency + the iGPU image-edge gate). At a TRUE Sobel contour (>= threshold) commit FULLY to the round-trip-consistent side (no soft blend = no smear/step/crescent); flat interior keeps the soft blend; a binary edge-gated pick is also steadier than the ratio-weighted blend (eases the vibration). Needs --bidir (default on) + the iGPU field (default on via bg-snap/band-xfade). 0 = OFF byte-identical. Clamp [0,1].\n", c.disoccl_hardpick); return 0; } return 1; }   // edge-gated disocclusion hard-pick threshold (clamp [0,1])
            if(!std::strcmp(arg,"--pace-variance")){ c.pace_variance=true; std::printf("[ra] --pace-variance: FSR3 variance-aware moving-average present pacer (target = SMA10(present deltas) − varFactor·stddev − safetyMargin; defaults 0.1/0.75ms; reset on >100ms hitch). Smooths the present-interval CoV in the light/stable regime; pair with --async-present for the saturation collapse. Pure CPU; default-off byte-identical.\n"); return 0; }   // FSR3 variance-aware present pacer
            if(!std::strcmp(arg,"--pv-safety")){ if(auto v=next(arg)){ c.pv_safety_ms=std::atof(v); return 0; } return 1; }   // FSR3 safetyMargin override (ms)
            if(!std::strcmp(arg,"--pv-var")){ if(auto v=next(arg)){ c.pv_var_factor=std::atof(v); return 0; } return 1; }   // FSR3 varianceFactor override
            if(!std::strcmp(arg,"--target-output-fps")){ if(auto v=next(arg)){ c.target_output_fps=(!std::strcmp(v,"auto"))?(float)c.refresh_hz:(float)std::atof(v); if(c.target_output_fps>0.f) c.async_present=true; std::printf("[ra] --target-output-fps %.0f: the fractional output-rate controller (holds a STEADY SUSTAINABLE cadence). realized_mult=clamp(target_eff/base,1,8), target_eff=min(this,refresh,sustain_frac*measured-achievable); N=mult*span drives the even-grid; the over-production drop refuses to over-command the warp (anti-windup). Auto-enables --async-present. NEVER caps the game. 0=OFF byte-identical.\n", c.target_output_fps); return 0; } return 1; }   // auto-enables async
            if(!std::strcmp(arg,"--s2-sustain")){ if(auto v=next(arg)){ float f=(float)std::atof(v); c.s2_sustain_frac = f<0.5f?0.5f:(f>1.f?1.f:f); return 0; } return 1; }   // kSustainFrac override [0.5,1]
            if(!std::strcmp(arg,"--load-governor")){ c.load_governor=true; std::printf("[ra] --load-governor: a util-driven GRADUATED tier FLOOR (decoupled from the t_pair_ema>budget gate). The live 4090 util maps to a tier floor: >=%.0f%%->5 (bwd-off + single-pass gme), >=%.0f%%->4 (object_repair/memory shed), >=%.0f%%->3 (period-4 holon); the floor is a MAX over the CPU ladder. The util LEADS the CPU EMA (the GPU pegs before the pair time does), so the shed engages under combat. Keeps fwd-flow+warp ALWAYS (keep-generating). Hysteresis: raise fast, lower on dwell (~90 pairs). DEFAULT ON (--no-load-governor desactiva; byte-identical mientras no haya presión/util alta).\n",c.gov_util,c.gov_util-6.f,c.gov_util-12.f); return 0; }   // DEFAULT-ON load governor
            if(!std::strcmp(arg,"--gov-util")){ if(auto v=next(arg)){ float f=(float)std::atof(v); c.gov_util = f<50.f?50.f:(f>100.f?100.f:f); std::printf("[ra] --gov-util %.0f%%: the floor's top band (>=%.0f%%->tier-5); -6/-12%% are the tier-4/tier-3 bands below (clamp [50,100]; default 92). Needs --load-governor.\n",c.gov_util,c.gov_util); return 0; } return 1; }   // 4090-util floor top band
            if(!std::strcmp(arg,"--csv")){ if(auto v=next(arg)){ c.csv_path=v; return 0; } return 1; }   // comprehensive per-present telemetry CSV export
            if(!std::strcmp(arg,"--duration")||!std::strcmp(arg,"--exit-after")){ if(auto v=next(arg)){ double s=std::atof(v); c.run_max_ms = s>0.0?s*1000.0:0.0; std::printf("[ra] %s %.4gs: FG bounded-run — after ~%.4gs of presenting, set g_quit (the same signal as Ctrl-C/vk_live) → the clean teardown (CSV finalize + bounded-run summary) runs. 0 = unbounded. DEFAULT 0 (byte-identical-off).\n",arg,s,s); return 0; } return 1; }   // bounded-run wall-clock; --exit-after is an alias of --duration
            if(!std::strcmp(arg,"--max-frames")){ if(auto v=next(arg)){ int n=std::atoi(v); c.run_max_frames = n>0?(uint64_t)n:0ull; std::printf("[ra] --max-frames %llu: FG bounded-run — after this many presents (total_frames) set g_quit → the clean teardown runs. 0 = unbounded. DEFAULT 0 (byte-identical-off).\n",(unsigned long long)c.run_max_frames); return 0; } return 1; }   // bounded-run present cap
            if(!std::strcmp(arg,"--fps-overlay")){ c.fps_overlay=true; std::printf("[ra] --fps-overlay: live \"in->out\" fps overlay (real-captured in -> presented out) drawn TOP-LEFT on the presented frame by a compute RMW on Apresent (RGBA8) INSIDE bridge_present_src — the SAFE sync present path (the WAP warp path faults DEVICE_LOST). Shows on the default (sync do_present_P) path; the --async-present path bypasses bridge_present_src. DEFAULT OFF (byte-identical-off).\n"); return 0; }   // opt-in in->out fps overlay
            if(!std::strcmp(arg,"--async-present")){ c.async_present=true; std::printf("[ra] --async-present: decouple the WAP present from the warp fence (non-blocking present + drop-interpolated; ~1 frame pipeline latency). DEFAULT ON (--no-async-present restaura la ruta síncrona).\n"); return 0; }   // DEFAULT-ON async present
            if(!std::strcmp(arg,"--fdrop")){ c.fdrop=true; if(!c.async_present){ c.async_present=true; std::printf("[ra] --fdrop: auto-enabling --async-present (the exact-duplicate drop reuses its re-present-front path; the sync path's present is inside the lambda and not byte-safe to skip).\n"); } std::printf("[ra] --fdrop: present-side EXACT-DUPLICATE discriminator — a tick whose (pair,cand_k)==last DELIVERED frame is DROPPED (skip the warp, re-present the front) = a redundant-warp elision (frame-density). DEFAULT OFF (byte-identical).\n"); return 0; }   // opt-in exact-dup discriminator
            if(!std::strcmp(arg,"--fdrop-quiet-ms")){ if(auto v=next(arg)){ float f=(float)std::atof(v); c.fdrop_quiet_ms = f<0.f?0.f:(f>1000.f?1000.f:f); std::printf("[ra] --fdrop-quiet-ms %.1f: soft near-dup floor — PARSED, LOGIC NOT IMPLEMENTED. Inert at 0.0.\n",c.fdrop_quiet_ms); return 0; } return 1; }   // soft near-dup floor (not implemented)
            if(!std::strcmp(arg,"--fdrop-k")){ if(auto v=next(arg)){ float f=(float)std::atof(v); c.fdrop_k = f<0.f?0.f:(f>16.f?16.f:f); std::printf("[ra] --fdrop-k %.2f: soft-arm motion sensitivity — PARSED, LOGIC NOT IMPLEMENTED. Inert at 0.0.\n",c.fdrop_k); return 0; } return 1; }   // soft-arm sensitivity (not implemented)
            if(!std::strcmp(arg,"--upload-xfer")){ c.upload_xfer=true; if(!c.async_present){ c.async_present=true; std::printf("[ra] --upload-xfer: auto-enabling --async-present (the warp must be the non-blocking bslot path so the present thread is not re-blocked).\n"); } std::printf("[ra] --upload-xfer: move the per-pair WAP upload onto the 4090's transfer/async-compute queue (parallel DMA engine) so the ~10ms of copies overlap the graphics work instead of host-blocking the present thread. CONCURRENT images + two timeline semaphores. Forces OFF if no transfer family exists. DEFAULT OFF (byte-identical).\n"); return 0; }   // opt-in upload-offload
            if(!std::strcmp(arg,"--fwd-prestage")){ c.fwd_prestage=true; std::printf("[ra] --fwd-prestage: collapse the F per-pair build gap — split the hRP_b[s]->hRP_b_dev[s] device copy out of the blocking flow submit into a double-buffered no-wait prestage on the F flow lane (A.q2), so the ~11ms non-compute B-side ingest overlaps the flow-record + ring-guard wait instead of fronting it. Shrinks freshage_ema (build ~20->~9ms target) -> lower interp latency AND D lead, no flow-quality cost. Serial WAP + iGPU-convert path only; force-off otherwise. Crash-class. DEFAULT OFF (byte-identical: the copy stays inline in cmdF).\n"); return 0; }   // opt-in fwd-prestage
            if(!std::strcmp(arg,"--shallow-queue")){ c.shallow_queue=true; if(!c.async_present){ c.async_present=true; std::printf("[ra] --shallow-queue: auto-enabling --async-present (the early-promote operates ONLY on the dedicated non-blocking bslot machine).\n"); } std::printf("[ra] --shallow-queue: after the warp submit, a BOUNDED non-blocking re-poll (cap %d us) of THIS tick's fence → present it THIS tick when the 4090 had headroom (async depth ~1→~0); a budget MISS falls through to the one-behind present (byte-identical). Hit rate is high in mid-motion, ~0 in saturated combat. Watch sq:H/M + present jitter. Crash-class. DEFAULT OFF.\n", c.sq_budget_us); return 0; }   // opt-in shallow-queue; implies --async-present
            if(!std::strcmp(arg,"--shallow-queue-budget-us")){ if(auto v=next(arg)){ int b=std::atoi(v); c.sq_budget_us = b<0?0:(b>4000?4000:b); std::printf("[ra] --shallow-queue-budget-us %d: early-promote poll cap (us). 0 = inert. Needs --shallow-queue.\n", c.sq_budget_us); return 0; } return 1; }   // early-promote poll cap
            if(!std::strcmp(arg,"--phase-norm")){ c.phase_norm=true; std::printf("[ra] --phase-norm: NORMALIZED-N frame ladder — the intra-pair displayed phase is placed on the EVEN grid (j+0.5)/N (N=span*T_robust/tick_period) instead of the jittery content-clock sub-phase, so the ~refresh/source ticks a pair gets sweep 0->1 UNIFORMLY (the clock still selects the pair). The boundary step still over/undershoots when N_pred mismatches (no lookahead). DEFAULT OFF (byte-identical).\n"); return 0; }   // opt-in phase-reshape
            if(!std::strcmp(arg,"--cphase")){ c.cphase=true; std::printf("[ra] --cphase: velocity-continuous intra-pair phase-rate reshape — the new pair's opening phase-rate matches the prev pair's closing displayed speed (monotone cubic, slope-clamped), easing the boundary velocity STEP --phase-norm leaves. mv untouched → TEXT-safe; endpoints exact → ZERO added latency. Composes with --phase-norm. DEFAULT OFF (byte-identical).\n"); return 0; }   // opt-in cadence reshape
            if(!std::strcmp(arg,"--cphase-ease")){ if(auto v=next(arg)){ float f=(float)std::atof(v); c.cphase_ease=f<0.05f?0.05f:(f>0.5f?0.5f:f); std::printf("[ra] --cphase-ease %.2f: phase-width of the opening ease window. Needs --cphase.\n",c.cphase_ease); return 0; } return 1; }   // ease window width
            if(!std::strcmp(arg,"--cphase-gain")){ if(auto v=next(arg)){ float f=(float)std::atof(v); c.cphase_gain=f<0.f?0.f:(f>1.f?1.f:f); std::printf("[ra] --cphase-gain %.2f: seam-slope match strength (0=off/linear, 1=full). Needs --cphase.\n",c.cphase_gain); return 0; } return 1; }   // seam-slope match strength
            if(!std::strcmp(arg,"--low-d")){ c.low_d=true; std::printf("[ra] --low-d: D-anchor floor-trim — trims the phasefix D SPAN term (half-span coverage by default + a lap-escape growth clamp at %.1f source-frames); freshage_ema is KEPT as the freeze floor (D>=freshage always → selects a fresher pair → less freeze risk, never more). Lower input lag. REQUIRES phasefix (default); a no-op under --no-phasefix. DEFAULT OFF (byte-identical).\n",c.lowd_span_cap); return 0; }   // opt-in D-anchor floor-trim
            if(!std::strcmp(arg,"--low-d-frac")){ if(auto v=next(arg)){ double f=std::atof(v); c.lowd_span_frac = f<0.0?0.0:(f>1.0?1.0:f); std::printf("[ra] --low-d-frac %.2f: fraction of the span term kept (1.0=no trim). Needs --low-d.\n",c.lowd_span_frac); return 0; } return 1; }   // span-term fraction
            if(!std::strcmp(arg,"--low-d-span-cap")){ if(auto v=next(arg)){ double f=std::atof(v); c.lowd_span_cap = f<1.0?1.0:(f>8.0?8.0:f); std::printf("[ra] --low-d-span-cap %.1f: lap-escape growth ceiling for the span term, in source frames. Needs --low-d.\n",c.lowd_span_cap); return 0; } return 1; }   // span-term growth ceiling
            if(!std::strcmp(arg,"--real-fast-path")||!std::strcmp(arg,"--rfp")){ c.real_fast_path=true; if(!c.async_present){ c.async_present=true; std::printf("[ra] --rfp: auto-enabling --async-present (the real MUST be presented through the DEDICATED non-blocking bslot path — NEVER the synchronous do_present_P that resets the SHARED cmdBridge/fBridge mid-async-flight = use-after-reset / device-lost).\n"); } std::printf("[ra] --real-fast-path (--rfp): on a tick already landing at the freshest captured REAL (gen_back==0, phase>=%.2f), OVERRIDE the D-selected warp pair with the freshest real presented via the dedicated async bslot path — a real needs no interpolation pair → minimal D for REALS (the responsiveness lever); interp ticks keep the full D-anchored cadence (UNCHANGED). Crash-class. DEFAULT OFF (byte-identical).\n",1.0f-c.rfp_window); return 0; }   // opt-in real-fast-path; implies --async-present
            if(!std::strcmp(arg,"--rfp-window")){ if(auto v=next(arg)){ float f=(float)std::atof(v); c.rfp_window = f<0.f?0.f:(f>1.f?1.f:f); std::printf("[ra] --rfp-window %.2f: fire the real-fast-path only when phase_global >= %.2f (smaller = closer to phase 1 = less cadence disturbance). Needs --rfp.\n",c.rfp_window,1.0f-c.rfp_window); return 0; } return 1; }   // rfp window
            if(!std::strcmp(arg,"--rfp-fresh")){ c.rfp_fresh=true; std::printf("[ra] --rfp-fresh: on a real-fast-path tick present the FRESHEST captured real ((cur_c-1)%%cap_slots) not the lagged pair real; latency measured vs c_slots[fresh].t_cap_ms (the responsiveness lever plain --rfp lacks). Content-order key KEPT at pair_c (no clock push → no reseat/freeze); the trade is a content SAWTOOTH (fresh real → stale interp), an accepted opt-in visual effect. Needs --rfp. DEFAULT OFF.\n"); return 0; }   // --rfp fresh-real sub-toggle
            if(!std::strcmp(arg,"--asw")){
                std::printf("[ra] --asw: bounded forward EXTRAPOLATION (ASW): when B falls behind and the sync-clock phase overshoots the held pair, the warp projects cur FORWARD along the MV instead of HOLD-at-1 freezing -> fills the throughput deficit. Needs --sync-clock (default ON). Best with --bg-snap. DEFAULT OFF, byte-identical off.\n");
                c.asw=true; return 0;
            }
            if(!std::strcmp(arg,"--asw-max")){ if(auto v=next(arg)){ float f=(float)std::atof(v); c.asw_max = f<0.f?0.f:(f>4.f?4.f:f); return 0; } return 1; }   // extrapolation bound in phase units (clamp [0,4])
            if(!std::strcmp(arg,"--cap-slots")){ if(auto v=next(arg)){ int n=std::atoi(v); c.cap_slots = n<0?0:(n>32?32:n); return 0; } return 1; }   // override the auto-sized capture-ring depth (0=auto, max 32)
            if(!std::strcmp(arg,"--ingest-backlog")){ if(auto v=next(arg)){ int n=std::atoi(v); c.ingest_backlog = n<1?1:(n>3?3:n); std::printf("[ra] --ingest-backlog %d: in-order ingest drain depth (the DOMINANT freshage/input-lag floor lever; F-pair compute ~8.7ms but freshage ~36ms = mostly this backlog). 1=freshest/most span-2 skips, 3=smoothest/most latency. Default 3 (byte-identical).\n",c.ingest_backlog); return 0; } return 1; }   // in-order ingest drain depth (3=kIngestBacklog max; only reduces → torn-read-safe)
            if(!std::strcmp(arg,"--latency-trace")){ c.latency_trace=true; std::printf("[ra] --latency-trace: MEASUREMENT-ONLY pipeline latency decomposition — emits a [lat-trace] line (INVISIBLE compose/copy pre-tcap + freshage split pickup/convert/build/detect). Each delta is single-clock; compose via a guarded QPC delta (0 if epoch-unavailable). Default OFF (byte-identical).\n"); return 0; }   // latency-trace (measurement-only)
            if(!std::strcmp(arg,"--copy-fence")){ c.copy_fence=true; std::printf("[ra] --copy-fence: event-driven WGC copy-completion pickup (a fence+event wait instead of a Map-retry busy-poll, off the D3D11 context → no callback deadlock). Needs D3D11.4. DEFAULT OFF.\n"); return 0; }   // copy-fence
            if(!std::strcmp(arg,"--copy-device")){ c.copy_device=true; std::printf("[ra] --copy-device: run the INVISIBLE WGC staging CopyResource (~5.6ms, the 4090 executing it behind the saturated game queue) on a SEPARATE D3D11 device on the SAME adapter → the copy queue is decoupled from d.dev and OVERLAPS rather than fronting pickup. Pre-tcap (NOT in our lat) but perceived. WGC-only; 2nd-device create-fail → FORCED OFF (byte-identical). t_cap_ms set only after the DO_NOT_WAIT Map of the completed copy (pair with --copy-fence for the explicit completion edge). DEFAULT OFF.\n"); return 0; }   // copy-device
            if(!std::strcmp(arg,"--motion-fallback")){ c.motion_fallback=true; c.async_present=true; std::printf("[ra] --motion-fallback: AFMF2-style FRAME-LEVEL fast-motion fallback — above --mf-disp gme dispersion, present the freshest real via the dedicated bslot path instead of a strobing-garbage warp (auto-enables --async-present; reuses the --rfp machinery). DEFAULT OFF.\n"); return 0; }   // frame-level fast-motion fallback
            if(!std::strcmp(arg,"--mf-disp")){ if(auto v=next(arg)){ float f=(float)std::atof(v); c.mf_disp = f<0.f?0.f:(f>100.f?100.f:f); std::printf("[ra] --mf-disp: motion-fallback dispersion bound = %.1f%% (gme dis%%; above it -> present a real). Clamp [0,100].\n", c.mf_disp); return 0; } return 1; }   // dispersion threshold
            if(!std::strcmp(arg,"--force-single-gpu")){ c.force_single_gpu=true; std::printf("[ra] --force-single-gpu: drive the SINGLE-GPU path (suppress the 2nd discrete + iGPU; all FG roles on device A). DEFAULT OFF.\n"); return 0; }   // single-GPU path
            if(!std::strcmp(arg,"--present-waitable")){ c.present_waitable=true; std::printf("[ra] --present-waitable: waitable swapchain (SetMaximumFrameLatency(1) + wait-before-present) for composed-overlay jitter reduction. PARTIAL: reduces jitter WITHIN DWM composition, does NOT reach Independent Flip (that needs the own-window mode). DEFAULT OFF.\n"); return 0; }   // waitable swapchain
            if(!std::strcmp(arg,"--present-sync")){ if(auto v=next(arg)){ int n=std::atoi(v); c.present_sync=(uint32_t)(n<0?0:(n>4?4:n)); std::printf("[ra] --present-sync %u: Present(sync_interval) — 0 = present-immediately (over-presents past refresh); 1 = pace to the compositor (stops over-presenting). DEFAULT 0.\n",c.present_sync); return 0; } return 1; }   // Present(sync_interval)
            if(!std::strcmp(arg,"--present-colorspace")){ if(auto v=next(arg)){ c.present_colorspace=(!std::strcmp(v,"off"))?0u:1u; std::printf("[ra] --present-colorspace %s: declare the overlay colorspace (sRGB) so an HDR/Advanced-Color desktop composites the SDR overlay WITHOUT washout (soft: skipped if IDXGISwapChain3/CheckColorSpaceSupport unavailable). DEFAULT off.\n",v); return 0; } return 1; }   // overlay colorspace
            if(!std::strcmp(arg,"--present-fp16")||!std::strcmp(arg,"--hdr")){ c.present_format=1; std::printf("[ra] %s: present an FP16 scRGB swapchain (R16G16B16A16_FLOAT + DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709) so an HDR display receives HDR, AND widen the VK->D3D11 bridge texture to FP16 so the one CopyResource into the backbuffer stays format-compatible. SOFT: if the FP16 swapchain create fails the surface falls back to BGRA8 and a producer/consumer format disagreement is a NAMED clean quit (no in-thread rebuild). HONEST: HDR is INERT unless you run an HDR display + HDR content. DEFAULT off (byte-identical).\n",arg); return 0; }   // FP16 scRGB present
            if(!std::strcmp(arg,"--present-own-window")){ c.present_own_window=true; std::printf("[ra] --present-own-window: present our OWN opaque borderless flip-model HWND swapchain (Style::OwnWindow) → BECOME the displayed Independent-Flip plane instead of the DComp overlay → demote+pace the game. CLICK-THROUGH (your input still reaches the game) + the no-lock-out contract (alt-tab/minimize yields the plane, a watchdog force-hides a wedged FG, device-loss exits to passthrough). DEFAULT OFF.\n"); return 0; }   // own-window present
            if(!std::strcmp(arg,"--indicator")){ c.indicator=true; std::printf("[ra] --indicator: WIP on-screen PhyriadFG-active mark (passthrough-only for now; the warp-path stamp is pending the safe redesign — shader write or a separate layered window).\n"); return 0; }   // on-screen active mark
            if(!std::strcmp(arg,"--no-indicator")){ c.indicator=false; std::printf("[ra] --no-indicator: on-screen active mark OFF. DEFAULT is OFF (the indicator is opt-in via --indicator).\n"); return 0; }   // indicator disabler
            if(!std::strcmp(arg,"--flow-scale-target-mp")){ if(auto v=next(arg)){ float f=(float)std::atof(v); c.flow_scale_target_mp = f<0.25f?0.25f:(f>12.f?12.f:f); std::printf("[ra] --flow-scale-target-mp %.2f: the --flow-scale auto target (megapixels of flow-work; the quality<->latency slider). Clamp [0.25,12].\n",c.flow_scale_target_mp); return 0; } return 1; }   // adaptive flow-scale target
            if(!std::strcmp(arg,"--warp-scale")){ if(auto v=next(arg)){ const int n=std::atoi(v); if(n!=1&&n!=2&&n!=4){ std::printf("[ra] --warp-scale: '%s' invalid (must be 1, 2, or 4)\n",v); return 1; } c.warp_scale=n; if(n>1) std::printf("[ra] --warp-scale %d: OUR warp (the WAP synthesis) runs at WW/%d x WH/%d and the existing linear blit upsamples the warp output to the present extent (~%dx fewer warp invocations). The captured game pair-reals stay FULL-RES (no game cap, no game downscale). STARTUP pick, WAP-only. DEFAULT 1 = byte-identical.\n    NOTE (why default OFF): reduced-res warp + upscale is BLURRY (N=2 blurry, N=4 markedly worse + the object/disocclusion detection collapses); it is a COST lever and does NOT touch latency. Configurable for those who want the cost trade.\n",n,n,n,n*n); return 0; } return 1; }   // warp-scale: reduced-res WAP synthesis upsampled to present extent; cost lever, default-off
            if(!std::strcmp(arg,"--nvofa")){ c.nvofa=true; std::printf("[ra] --nvofa: replace the classical block-match flow with the NVIDIA hardware Optical Flow Accelerator (VK_NV_optical_flow) on the 4090. ~6.8x/4.2x/1.9x faster (1080p/1440p/4K), off the compute cores (<3%% under a pegged compute load = contention relief). The OFA gives flow + an 8-bit cost only: sad_best = a calibrated cost remap (--nvofa-cost-scale), sad_zero = a SEPARATE |A-B| reduction. Auto-disables (falls back to the classical OFP) if the device lacks VK_NV_optical_flow. Crash-class (touches device creation); DEFAULT OFF (byte-identical).\n"); return 0; }   // opt-in HW OFA
            if(!std::strcmp(arg,"--nvofa-cost-scale")){ if(auto v=next(arg)){ float f=(float)std::atof(v); c.nvofa_cost_scale = f<0.f?0.f:(f>16.f?16.f:f); std::printf("[ra] --nvofa-cost-scale %.3f: OFA cost(0..255)->sad_best remap (eye-calibration vs the OFP gate, residual_ceil in [0,192]). Needs --nvofa.\n",c.nvofa_cost_scale); return 0; } return 1; }   // OFA cost->sad_best remap
            if(!std::strcmp(arg,"--nvofa-sadz-scale")){ if(auto v=next(arg)){ float f=(float)std::atof(v); c.nvofa_sadz_scale = f<0.f?0.f:(f>64.f?64.f:f); std::printf("[ra] --nvofa-sadz-scale %.3f: input-block SUM|A-B|->WW_flow 8x8 SUM magnitude (eye-calibration). Needs --nvofa.\n",c.nvofa_sadz_scale); return 0; } return 1; }   // OFA sad_zero scale
            if(!std::strcmp(arg,"--camera-twarp")){ c.camera_twarp=true; std::printf("[ra] --camera-twarp: responsiveness extrapolation — present the interpolated FG frame's SAMPLING base shifted FORWARD by the predicted camera motion (gme affine translation · amt) so the displayed view LEADS the capture → mouse/camera feels synced (compensates the external-capture latency perceptually). PRESERVES A/B interpolation (NOT the cur-only --asw path). The SIGN is eye-tunable. DEFAULT OFF, byte-identical off.\n"); return 0; }   // camera-twarp
            if(!std::strcmp(arg,"--camera-twarp-amt")){ if(auto v=next(arg)){ float f=(float)std::atof(v); c.camera_twarp_amt = f<0.f?0.f:(f>3.f?3.f:f); return 0; } return 1; }   // the eye-tunable lead scalar (clamp [0,3])
            if(!std::strcmp(arg,"--pin")){ c.pin_threads=true; std::printf("[ra] --pin: topology-pillar thread pinning ENABLED (C/F/P to fixed cores, P RT-elevated). NOTE: does NOT fix the GPU-bound >monitor slip; opt-in for hybrid-CPU/NUMA scalability. DEFAULT ON (--no-pin disables).\n"); return 0; }   // DEFAULT-ON thread pinning
            if(!std::strcmp(arg,"--pin-test")){ if(auto v=next(arg)){ int n=std::atoi(v); c.pin_test=(n<0?0:(n>5?5:n)); c.pin_test_set=true; static const char* kNm[6]={"FULL(pin C/F/P + elevate P TIME_CRITICAL + elevate F HIGHEST = current --pin)","NO-FLOW-RT(pin C/F/P + elevate P only; F pinned-NOT-elevated)","PRIO-ONLY(elevate P+F; NO pin/affinity on C/F/P)","AFFINITY-ONLY(pin C/F/P; NO elevation)","NEITHER(no pin, no elevate = same as no --pin)","MMCSS-COMPOSITE(P HARD-pin + MMCSS 'Pro Audio'/CRITICAL; C/F SOFT-affinity + MMCSS 'Capture' HIGH/NORMAL; elevate_thread_rt fallback per thread when MMCSS inactive = the composite thread policy)"}; std::printf("[ra] --pin-test %d: thread-policy ablation lever-select (ACTIVE ONLY WITH --pin) = %s. DEFAULT 0 (== --pin; INERT/byte-identical-off without --pin).\n", c.pin_test, kNm[c.pin_test]); return 0; } return 1; }   // thread-policy ablation lever-select
            if(!std::strcmp(arg,"--fg-protect")){ c.fg_protect=true; c.pin_threads=true; if(!c.pin_test_set) c.pin_test=5; c.async_present=true; std::printf("[ra] --fg-protect: composed thread-protection bundle for the COMBINED GPU99%%+CPU100%% FG collapse (CPU-starvation of the C/F/P threads). (a) thread policy = %s (P HARD-pin + MMCSS 'Pro Audio'/CRITICAL; C/F SOFT-affinity + MMCSS 'Capture' HIGH/NORMAL; elevate_thread_rt TIME_CRITICAL/HIGHEST fallback when MMCSS inactive); (b) async-present ARMED (slot-1 at init + decoupled non-blocking present + drop-interpolated); (c) GAME_FLOOR=max(4,p_cores/2) bounds the hard-pin count (only P today) so the game keeps >= half the P-cores (NEVER touches the game's affinity/priority — the NO-GAME-CAP dogma as code; INERT >= 8 P-cores, demotes P to soft on <= 4 P-cores). DEFAULT ON (--no-fg-protect desactiva). NOTA: para el caso por-defecto (flag no pasado) el pin_test=5 lo fija el bloque post-parse de parse_args.\n", c.pin_test==5?"--pin-test 5 (MMCSS-composite)":"the explicitly-set --pin-test (override honoured)"); return 0; }   // DEFAULT-ON; composes pin_test=5 + async_present + GAME_FLOOR
            if(!std::strcmp(arg,"--dpi-probe")){ c.dpi_probe=true; std::printf("[ra] --dpi-probe: diagnostic — logs GetClientRect, GetDpiForWindow, cap_item.Size(), and the first frame.ContentSize() to diagnose the high-DPI capture-crop (corner-zoom). Read-only, no behavior change.\n"); return 0; }   // DPI-probe
            if(!std::strcmp(arg,"--fg-prebake")){ c.fg_prebake=true; std::printf("[ra] --fg-prebake: pre-bake the matcher/warp descriptor sets at init (one collection per Bframe ping-pong parity) so record_optical_flow SKIPS the per-pair vkUpdateDescriptorSets BURST (~41 host driver calls/record, fwd+bwd ~82/pair, ~10k/s @125 pairs). A host-CPU relief on the F-thread under GPU saturation; the GPU slice is unchanged. ADDITIVE, default OFF (byte-identical). Eligible only on the FG default path (fg_variant active + --no-ambig + no affine); otherwise inert. Read the saving via the vkUpdateDescriptorSets call-count delta printed at the F-thread teardown.\n"); return 0; }   // descriptor-prebake fork
            if(!std::strcmp(arg,"--sc-phase-gain")){ if(auto v=next(arg)){ double f=std::atof(v); c.sc_phase_gain = f<0.0?0.0:(f>1.0?1.0:f); std::printf("[ra] --sc-phase-gain %.3f (PLL phase-correction fraction/arrival; default 0.10) — cadence-lock sweep\n",c.sc_phase_gain); return 0; } return 1; }   // PLL phase-gain override
            if(!std::strcmp(arg,"--sc-freq-alpha")){ if(auto v=next(arg)){ double f=std::atof(v); c.sc_freq_alpha = f<0.0?0.0:(f>1.0?1.0:f); std::printf("[ra] --sc-freq-alpha %.3f (PLL freq EMA weight/arrival; default 0.05) — cadence-lock sweep\n",c.sc_freq_alpha); return 0; } return 1; }   // PLL freq-alpha override
            if(!std::strcmp(arg,"--sc-reseat")){ if(auto v=next(arg)){ double f=std::atof(v); c.sc_reseat_err = f<0.5?0.5:(f>64.0?64.0:f); std::printf("[ra] --sc-reseat %.2f (PLL re-seat threshold, source-frames; default 4.0)\n",c.sc_reseat_err); return 0; } return 1; }   // PLL re-seat threshold override
            if(!std::strcmp(arg,"--pace-present")){ c.pace_present=true; std::printf("[ra] --pace-present: the metronomic, drift-corrected present pacer. KEEPS the absolute output-clock grid (tick_t0 + k·tick_period_ms = the MASD->0 metronome) and only SLOWLY slews tick_t0 toward the realized achievable cadence (FSR3 SMA10 variance-damped drift signal, gain %.3f, bounded). Composes WITH the output clock (slews the ANCHOR, never a per-present anchor). Never pushes the period below the panel tick (no over-production windup). Pure CPU; default-off byte-identical.\n", c.pp_slew_gain); return 0; }   // drift-corrected present pacer
            if(!std::strcmp(arg,"--pp-safety")){ if(auto v=next(arg)){ double f=std::atof(v); c.pp_safety_ms = f<0.0?0.0:(f>8.0?8.0:f); std::printf("[ra] --pp-safety %.3f ms (FSR3 safetyMargin of the drift target; default 0.50). Read only when --pace-present.\n",c.pp_safety_ms); return 0; } return 1; }   // safetyMargin override
            if(!std::strcmp(arg,"--pp-var")){ if(auto v=next(arg)){ double f=std::atof(v); c.pp_var_factor = f<0.0?0.0:(f>4.0?4.0:f); std::printf("[ra] --pp-var %.3f (FSR3 varianceFactor of the drift target; default 0.10). Read only when --pace-present.\n",c.pp_var_factor); return 0; } return 1; }   // varianceFactor override
            if(!std::strcmp(arg,"--pp-slew")){ if(auto v=next(arg)){ double f=std::atof(v); c.pp_slew_gain = f<0.0?0.0:(f>0.5?0.5:f); std::printf("[ra] --pp-slew %.3f (per-tick fraction of the grid phase error folded into tick_t0 — the drift corrector; default 0.05, clamp [0,0.5]). Read only when --pace-present.\n",c.pp_slew_gain); return 0; } return 1; }   // drift-corrector gain
            if(!std::strcmp(arg,"--pace-hard")){ c.pace_hard=true; std::printf("[ra] --pace-hard: the HARD present-target pacer. COMPOSES ABOVE --pace-present (the soft slew supplies the drifted grid anchor; this pins THIS frame to the QPC edge of tgt via a bounded sleep-then-spin). own-window-only (the displayed Independent-Flip plane). FREEZE-FLOOR: total hard wait HARD-CAPPED < one vblank; missed/overshot/un-derivable target degrades to the soft path, never blocks (held/overshoot surfaced as ph: in the DIAG line). Pure CPU, thread-P-local. DEFAULT OFF, byte-identical.\n"); return 0; }   // HARD present-target pacer
            if(!std::strcmp(arg,"--ph-spin")){ if(auto v=next(arg)){ double f=std::atof(v); c.ph_spin_ms = f<0.1?0.1:(f>4.0?4.0:f); std::printf("[ra] --ph-spin %.3f ms (bounded final-spin budget of --pace-hard; default 2.0, clamp [0.1,4.0]). The total hard wait is independently hard-capped < one vblank. Read only when --pace-hard.\n",c.ph_spin_ms); return 0; } return 1; }   // final-spin budget
            if(!std::strcmp(arg,"--pace-vblank")){ c.pace_vblank=true; std::printf("[ra] --pace-vblank: the VBLANK PHASE-LOCK. COMPOSES ABOVE --pace-hard (needs it): aligns the present-target grid anchor tick_t0 to the ACTUAL display vblank (DwmGetCompositionTimingInfo qpcVBlank/qpcRefreshPeriod, queried ~1/sec on the COLD path), so ph_tgt = tick_t0 + k·tick_period_ms lands on the panel cadence. QPC↔steady_clock bridged by ONE paired sample (no epoch mix); the phase slew is BOUNDED (lock-gain %.3f) so it never trips the re-seat / freeze-floor. Query fail → grid left as-is → degrades to plain --pace-hard, never blocks. own-window-only. DEFAULT OFF, byte-identical.\n", c.pv_lock_gain); return 0; }   // VBLANK phase-lock
            if(!std::strcmp(arg,"--pv-lock-gain")){ if(auto v=next(arg)){ double f=std::atof(v); c.pv_lock_gain = f<0.0?0.0:(f>0.5?0.5:f); std::printf("[ra] --pv-lock-gain %.3f (per-query fraction of the vblank phase error folded into tick_t0 — the phase-lock slew; default 0.05, clamp [0,0.5]). Read only when --pace-vblank.\n",c.pv_lock_gain); return 0; } return 1; }   // vblank phase-lock slew
            if(!std::strcmp(arg,"--sc-select")){ c.sc_select=true; c.sync_clock=true; std::printf("[ra] --sc-select: content_clock drives pair SELECTION too — full 0->1 phase sweep per pair, seamless boundaries (kills the clock-disagreement truncation/start-stall pulse). Implies --sync-clock. Read-side selector swap, F/P protocol untouched. DEFAULT ON (--no-sc-select disables).\n"); return 0; }   // content_clock-driven selection
            if(!std::strcmp(arg,"--no-sc-select")){    c.sc_select=false;    std::printf("[ra] --no-sc-select: cadence pair-SELECTION reverts to the wall-time t_display scan. DEFAULT is ON.\n"); return 0; }
            if(!std::strcmp(arg,"--no-vblend")){       c.vblend=false;       std::printf("[ra] --no-vblend: velocity-continuity warp OFF. DEFAULT is ON.\n"); return 0; }
            if(!std::strcmp(arg,"--no-mv-subpel")){    c.mv_subpel=false;    std::printf("[ra] --no-mv-subpel: sub-pixel MV refinement OFF (integer best_mv). DEFAULT is ON.\n"); return 0; }
            if(!std::strcmp(arg,"--no-bg-snap")){      c.bg_snap=false;      std::printf("[ra] --no-bg-snap: background-MV snap OFF. DEFAULT is ON. (igpu-field stays on if band-xfade needs it.)\n"); return 0; }
            if(!std::strcmp(arg,"--no-band-xfade")){   c.band_xfade=0.f;     std::printf("[ra] --no-band-xfade: gravity-cancellation reveal-fill OFF. DEFAULT is ON.\n"); return 0; }
            if(!std::strcmp(arg,"--no-igpu-field")){   c.igpu_field=false;   std::printf("[ra] --no-igpu-field: campo de contornos iGPU OFF. DEFAULT is ON. Cascada (post-parse): apaga bg-snap/band-xfade/afill (leen el campo).\n"); return 0; }
            if(!std::strcmp(arg,"--no-deficit-tier")){ c.deficit_tier=false; std::printf("[ra] --no-deficit-tier: heavy-scene object_repair/memory shed OFF. DEFAULT is ON.\n"); return 0; }
            // The 4 DEFAULT-ON flags have their --no-X disabler here (in parse_extra, dodging the main-chain
            // C1061 limit). Each disables ONLY its own field; --no-fg-protect does NOT touch pin_threads/
            // async_present (use --no-pin / --no-async-present for those).
            if(!std::strcmp(arg,"--no-pin")){           c.pin_threads=false;   std::printf("[ra] --no-pin: pin de hilos OFF — hilos bare programados por el OS (byte-identical). DEFAULT es ON.\n"); return 0; }
            if(!std::strcmp(arg,"--no-fg-protect")){    c.fg_protect=false;     std::printf("[ra] --no-fg-protect: paquete de proteccion (MMCSS-composite mode-5 + GAME_FLOOR) OFF. DEFAULT es ON. NOTA: pin_threads/async_present siguen sus propios defaults (ON) — usa --no-pin / --no-async-present para desactivarlos.\n"); return 0; }
            if(!std::strcmp(arg,"--no-load-governor")){ c.load_governor=false;  std::printf("[ra] --no-load-governor: piso de tier graduado por util del 4090 OFF (tier-5 inalcanzable, byte-identical). DEFAULT es ON.\n"); return 0; }
            if(!std::strcmp(arg,"--no-async-present")){ c.async_present=false;   std::printf("[ra] --no-async-present: presentación asíncrona OFF → restaura la ruta SÍNCRONA bloqueante (byte-identical). DEFAULT es ON. NOTA: --target-output-fps/--fdrop/--upload-xfer/--rfp/--motion-fallback/--shallow-queue la AUTO-activan si se pasan DESPUÉS de este flag.\n"); return 0; }
            if(!std::strcmp(arg,"--ingest-async")){ c.ingest_async=true; std::printf("[ra] --ingest-async: decouple the DDA capture INGEST. The acquire thread does ONLY AcquireNextFrame->CopyResource+Flush->Map(DO_NOT_WAIT the PREVIOUS slot = readback overlap)->memcpy into a 4-slot RAW host ring->publish; a convert WORKER thread DROP-TO-NEWEST converts the freshest raw slot and publishes c_seq PROMPTLY so the freshest frame reaches F/P at minimal age. DDA-only (forced OFF for WGC); raw-ring alloc failure -> forced OFF (serial fallback, never crashes). The [ra-cap] `acq=` field shows the acquire rate vs `in=` (convert rate). DEFAULT OFF (byte-identical).\n"); return 0; }   // ingest-async
            if(!std::strcmp(arg,"--cap-route-probe")){ c.cap_route_probe=true; std::printf("[ra] --cap-route-probe: print a break-even-style CAPABILITY routing decision over the FG's app-local VDevs (has_fp16/has_dp4a/measured-fp16) + a vendor-NAMED capability manifest. MEASUREMENT-ONLY, byte-identical. HONEST: the decision is INERT — the FG's FIXED A/B/G roles (A=present, B=flow/gme, G=convert) are architecture-forced + the FG's AI~2.5 << the break-even crossover ⇒ offload=false ALWAYS. It ROUTES NOTHING; it only reports what a capability-driven decision WOULD say. DEFAULT OFF.\n"); return 0; }   // capability-routing probe, inert-by-design
            return -1;
        };
        if      (!std::strcmp(a,"--help"))            { print_help(argv[0]); return false; }
        else if (!std::strcmp(a,"--list-monitors"))   { c.list_only=true; }
        else if (!std::strcmp(a,"--no-upscale"))      { c.no_upscale=true; }
        else if (!std::strcmp(a,"--upscale-lanczos")) { c.lanczos=true; }
        else if (!std::strcmp(a,"--monitor"))         { if(auto v=next(a)) c.cap_mon=std::atoi(v); else return false; }
        else if (!std::strcmp(a,"--present-monitor")) { if(auto v=next(a)) c.pres_mon=std::atoi(v); else return false; }
        else if (!std::strcmp(a,"--residual-ceil"))   { if(auto v=next(a)) c.res_ceil=(float)std::atof(v); else return false; }
        else if (!std::strcmp(a,"--conf-improv"))     { if(auto v=next(a)) c.conf_improv=(float)std::atof(v); else return false; }
        else if (!std::strcmp(a,"--agreement"))       { if(auto v=next(a)) c.agreement=(float)std::atof(v); else return false; }
        else if (!std::strcmp(a,"--fg-factor"))       { if(auto v=next(a)){ if(!std::strcmp(v,"auto")){c.fg_auto=true;c.fg_factor=kAutoNMax;} else { c.fg_factor=std::atoi(v);
            // clamp to the interp-buffer depth (past it the alloc loop would overrun).
            if(c.fg_factor>kMaxInterp+1){ std::printf("[ra] --fg-factor %d clamped to %d (interp-buffer depth)\n",c.fg_factor,kMaxInterp+1); c.fg_factor=kMaxInterp+1; }
            if(c.fg_factor<1) c.fg_factor=1; } } else return false; }
        else if (!std::strcmp(a,"--mv-smooth"))       { if(auto v=next(a)) c.mv_smooth=(float)std::atof(v); else return false; }
        else if (!std::strcmp(a,"--mv-prior"))        { c.mv_prior=true; }
        else if (!std::strcmp(a,"--dump"))            { if(auto v=next(a)) c.dump_n=std::atoi(v); else return false; }
        else if (!std::strcmp(a,"--assist-gpu"))      { if(auto v=next(a)) std::snprintf(c.assist_gpu,sizeof(c.assist_gpu),"%s",v); else return false; }
        else if (!std::strcmp(a,"--windowed"))        {
            std::printf("[ra] ERROR: --windowed is no longer supported (the debug overlapped window is gone)\n");
            return false;
        }
        else if (!std::strcmp(a,"--capture-api"))     {
            auto v=next(a); if(!v) return false;
            if(!std::strcmp(v,"dd")) c.capture_api=CA_DD;
            else if(!std::strcmp(v,"wgc")) c.capture_api=CA_WGC;
            else { std::printf("[ra] --capture-api: unknown '%s' (dd|wgc)\n",v); return false; }
        }
        else if (!std::strcmp(a,"--window"))          { if(auto v=next(a)) std::snprintf(c.window_substr,sizeof(c.window_substr),"%s",v); else return false; }
        else if (!std::strcmp(a,"--fg-gpu")) {
            auto v=next(a); if(!v) return false;
            if     (!std::strcmp(v,"auto"))    c.fg_gpu=FG_AUTO;
            else if(!std::strcmp(v,"primary")) c.fg_gpu=FG_PRIMARY;
            else if(!std::strcmp(v,"assist"))  c.fg_gpu=FG_ASSIST;
            else { std::printf("[ra] --fg-gpu: unknown '%s' (auto|primary|assist)\n",v); return false; }
        }
        else if (!std::strcmp(a,"--convert-gpu")) {
            auto v=next(a); if(!v) return false;
            if     (!std::strcmp(v,"igpu"))    c.convert_gpu=CG_IGPU;
            else if(!std::strcmp(v,"primary")) c.convert_gpu=CG_PRIMARY;
            else { std::printf("[ra] --convert-gpu: unknown '%s' (igpu|primary)\n",v); return false; }
        }
        else if (!std::strcmp(a,"--present-gpu")) {
            // present is the panel-owning adapter (A/4090) by construction.
            auto v=next(a); if(!v) return false;
            std::printf("[ra] --present-gpu %s: ignored — present is the panel-owning adapter\n",v);
        }
        else if (!std::strcmp(a,"--output-clock")) {
            auto v=next(a); if(!v) return false;
            if(!std::strcmp(v,"timer")) c.output_clock=OC_TIMER;
            else { std::printf("[ra] ERROR: --output-clock '%s' not supported — only 'timer' remains\n",v); return false; }
        }
        else if (!std::strcmp(a,"--refresh-hz")) { if(auto v=next(a)){ c.refresh_hz=std::atoi(v); if(c.refresh_hz<1) c.refresh_hz=1; } else return false; }
        else if (!std::strcmp(a,"--cap-fps"))    { if(auto v=next(a)){ c.cap_fps=std::atoi(v); if(c.cap_fps<1) c.cap_fps=0; } else return false; }
        else if (!std::strcmp(a,"--flow-scale")) {
            // flow-resolution DRS. N ∈ {1,2,4}; reject anything else (no silent clamp).
            // N=1 = default = byte-identical (no blit, full-res init).
            auto v=next(a); if(!v) return false;
            if(!std::strcmp(v,"auto")){
                c.flow_scale_auto=true;
                std::printf("[ra] --flow-scale auto: the ADAPTIVE quality<->latency knob — flow_div auto-picked at init from the resolution + the --flow-scale-target-mp budget (default 2.1 MP; the future end-user UI drives it). Override with --flow-scale N.\n");
            } else {
                const int n=std::atoi(v);
                if(n!=1&&n!=2&&n!=4){ std::printf("[ra] --flow-scale: '%s' invalid (must be 1, 2, 4, or auto)\n",v); return false; }
                c.flow_scale=n;
                if(n>1) std::printf("[ra] --flow-scale %d: flow + the per-pair CPU tail run on a 1/%d-res MV grid (~%dx fewer block-match tiles + CPU work/pair). The two pair frames are bilinear-downsampled to WW/%d×WH/%d before the flow; the WAP warp auto-upsamples the smaller MV field (output stays full-res). WAP-only. DEFAULT 1 = byte-identical.\n",n,n,n*n,n,n);
            }
        }
        // ── positive legacy flags — accepted as no-ops (already default) ────────────
        else if (!std::strcmp(a,"--warp-at-presenter")) { std::printf("[ra] --warp-at-presenter: already default\n"); }
        else if (!std::strcmp(a,"--soft-gate"))         { std::printf("[ra] --soft-gate: already default\n"); }
        else if (!std::strcmp(a,"--commit-warp"))       { std::printf("[ra] --commit-warp: already default (commit_thresh=0.08, commit_real=true)\n"); }
        else if (!std::strcmp(a,"--commit-real"))       { std::printf("[ra] --commit-real: already default\n"); }
        else if (!std::strcmp(a,"--bidir"))             { std::printf("[ra] --bidir: already default\n"); }
        else if (!std::strcmp(a,"--fill-div"))          { std::printf("[ra] --fill-div: divergence pick enabled. DEFAULT OFF.\n"); c.fill_div=true; }
        else if (!std::strcmp(a,"--rescue"))            { std::printf("[ra] --rescue: already default\n"); }
        else if (!std::strcmp(a,"--mv-guided"))         { std::printf("[ra] --mv-guided: already default\n"); }
        else if (!std::strcmp(a,"--gme"))               { std::printf("[ra] --gme: already default\n"); }
        else if (!std::strcmp(a,"--matte"))             { std::printf("[ra] --matte: fluid-matte enabled. DEFAULT OFF.\n"); c.matte=true; if(c.matte_thresh<=0.f) c.matte_thresh=0.25f; }
        else if (!std::strcmp(a,"--stasis"))            { std::printf("[ra] --stasis: already default\n"); }
        else if (!std::strcmp(a,"--inertia"))           { std::printf("[ra] --inertia: already default\n"); }
        else if (!std::strcmp(a,"--objects"))           { std::printf("[ra] --objects: already default\n"); }
        else if (!std::strcmp(a,"--shapefield"))        { std::printf("[ra] --shapefield: already default\n"); }
        else if (!std::strcmp(a,"--crescent"))          { std::printf("[ra] --crescent: already default\n"); }
        else if (!std::strcmp(a,"--travel"))            { std::printf("[ra] --travel: already default\n"); }
        else if (!std::strcmp(a,"--contour"))           { std::printf("[ra] --contour: already default\n"); }
        else if (!std::strcmp(a,"--obj-crescent"))      { std::printf("[ra] --obj-crescent: already default\n"); }
        else if (!std::strcmp(a,"--appearance"))        { std::printf("[ra] --appearance: already default\n"); }
        else if (!std::strcmp(a,"--present-surface"))   { std::printf("[ra] --present-surface: already default\n"); }
        else if (!std::strcmp(a,"--overlay"))           { std::printf("[ra] --overlay: always active\n"); }
        // ── tuning-value flags (unchanged behavior; always accepted) ────────────────
        else if (!std::strcmp(a,"--commit-thresh")) { if(auto v=next(a)){ float f=(float)std::atof(v); c.commit_thresh = f<0.005f?0.005f:(f>0.5f?0.5f:f); } else return false; }
        else if (!std::strcmp(a,"--appear-band"))   { if(auto v=next(a)){ float f=(float)std::atof(v); c.appear_band = f<0.02f?0.02f:(f>0.5f?0.5f:f); } else return false; }
        else if (!std::strcmp(a,"--occl-thresh"))   { if(auto v=next(a)){ float f=(float)std::atof(v); c.occl_thresh = f<0.25f?0.25f:(f>8.0f?8.0f:f); } else return false; }
        else if (!std::strcmp(a,"--div-eps"))        { if(auto v=next(a)){ float f=(float)std::atof(v); c.div_eps = f<0.005f?0.005f:(f>1.0f?1.0f:f); } else return false; }
        else if (!std::strcmp(a,"--mv-median"))      { c.mv_median=true; }
        else if (!std::strcmp(a,"--mv-sim"))         { if(auto v=next(a)){ float f=(float)std::atof(v); c.mv_sim = f<0.02f?0.02f:(f>0.5f?0.5f:f); } else return false; }
        else if (!std::strcmp(a,"--matte-thresh"))   { if(auto v=next(a)){ float f=(float)std::atof(v); c.matte_thresh = f<0.05f?0.05f:(f>1.0f?1.0f:f); } else return false; }
        else if (!std::strcmp(a,"--mass-k"))          { if(auto v=next(a)){ float f=(float)std::atof(v); c.mass_k = f<0.0f?0.0f:(f>2.0f?2.0f:f); } else return false; }
        else if (!std::strcmp(a,"--stasis-thresh"))  { if(auto v=next(a)){ float f=(float)std::atof(v); c.stasis_thresh = f<0.05f?0.05f:(f>8.0f?8.0f:f); } else return false; }
        else if (!std::strcmp(a,"--inertia-thresh")) { if(auto v=next(a)){ float f=(float)std::atof(v); c.inertia_thresh = f<0.1f?0.1f:(f>1.0f?1.0f:f); } else return false; }
        // ── escape hatches — set tracking bools; cascade in post-parse normalization ─
        else if (!std::strcmp(a,"--no-warp-at-presenter")) {
            std::printf("[ra] --no-warp-at-presenter: grid mode (WAP disabled)\n");
            c.warp_at_presenter=false; c.no_wap=true;
        }
        else if (!std::strcmp(a,"--no-soft-gate")) {
            std::printf("[ra] --no-soft-gate: binary per-block keep/freeze\n");
            c.soft_gate=false;
        }
        else if (!std::strcmp(a,"--no-commit")) {
            std::printf("[ra] --no-commit: commit disabled (commit_thresh=0, commit_real=false)\n");
            c.commit_thresh=0.f; c.commit_real=false; c.no_commit=true;
        }
        else if (!std::strcmp(a,"--no-appearance")) {
            std::printf("[ra] --no-appearance: appearance-band temporal re-blend disabled\n");
            c.appearance=false; c.no_appearance=true;
        }
        else if (!std::strcmp(a,"--no-bidir")) {
            std::printf("[ra] --no-bidir: bidirectional flow disabled\n");
            c.bidir=false; c.occl_thresh=0.f; c.no_bidir=true;
        }
        else if (!std::strcmp(a,"--no-fill-div")) {
            std::printf("[ra] --no-fill-div: divergence-directed disocclusion disabled\n");
            c.fill_div=false; c.div_eps=0.f;
        }
        else if (!std::strcmp(a,"--no-rescue")) {
            std::printf("[ra] --no-rescue: candidate-rescue disabled\n");
            c.rescue=false;
        }
        else if (!std::strcmp(a,"--no-mv-guided")) {
            std::printf("[ra] --no-mv-guided: color-guided MV disabled\n");
            c.mv_guided=false; c.mv_sim=0.f;
        }
        else if (!std::strcmp(a,"--no-gme")) {
            std::printf("[ra] --no-gme: global motion model disabled\n");
            c.gme=false; c.no_gme=true;
        }
        else if (!std::strcmp(a,"--no-matte")) {
            std::printf("[ra] --no-matte: fluid-matte disabled\n");
            c.matte=false; c.matte_thresh=0.f;
        }
        else if (!std::strcmp(a,"--no-stasis")) {
            std::printf("[ra] --no-stasis: stasis bypass layer disabled\n");
            c.stasis=false; c.stasis_thresh=0.f;
        }
        else if (!std::strcmp(a,"--no-inertia")) {
            std::printf("[ra] --no-inertia: inertia persistence prior disabled\n");
            c.inertia=false; c.inertia_thresh=0.f;
        }
        else if (!std::strcmp(a,"--no-objects")) {
            std::printf("[ra] --no-objects: object-holon clustering + inheritance repair disabled\n");
            c.objects=false; c.no_objects=true;
        }
        else if (!std::strcmp(a,"--no-shapefield")) {
            std::printf("[ra] --no-shapefield: contour shape-field off — rigid single-MV inheritance\n");
            c.shapefield=false; c.no_shapefield=true;
        }
        else if (!std::strcmp(a,"--no-crescent")) {
            std::printf("[ra] --no-crescent: crescent-directed background fetch disabled (matte bg blend reverts to (1-t,t))\n");
            c.crescent=false; c.no_crescent=true;
        }
        else if (!std::strcmp(a,"--no-travel")) {
            std::printf("[ra] --no-travel: traveling-silhouette occupancy off — the full-testimony lerp (byte-identical)\n");
            c.travel=false; c.no_travel=true;
        }
        else if (!std::strcmp(a,"--no-contour")) {
            std::printf("[ra] --no-contour: contour marriage off — binary !matte_object composition (byte-identical)\n");
            c.contour=false; c.no_contour=true;
        }
        else if (!std::strcmp(a,"--no-obj-crescent")) {
            std::printf("[ra] --no-obj-crescent: object-crescent side weighting off — (1-t,t) object blend + time-nearest commit (byte-identical)\n");
            c.obj_crescent=false; c.no_obj_crescent=true;
        }
        else if (!std::strcmp(a,"--member-commit"))    { std::printf("[ra] --member-commit: already default\n"); }
        else if (!std::strcmp(a,"--no-member-commit")) {
            std::printf("[ra] --no-member-commit: membership-beats-the-blend off — the warp-vs-blend selection stays unchanged (the cross-fade ghost-step, byte-identical)\n");
            c.member_commit=false; c.no_member_commit=true;
        }
        else if (!std::strcmp(a,"--commit-default"))    { std::printf("[ra] --commit-default: already default\n"); }
        else if (!std::strcmp(a,"--no-commit-default")) {
            std::printf("[ra] --no-commit-default: commit-default flip off — the warp-vs-blend selection keeps the photometric fallback (the fixed cross-fade default, byte-identical)\n");
            c.commit_default=false; c.no_commit_default=true;
        }
        else if (!std::strcmp(a,"--onepos"))    { std::printf("[ra] --onepos: already default\n"); }
        else if (!std::strcmp(a,"--onepos-band"))    { if(auto v=next(a)){ float f=(float)std::atof(v); c.onepos_band = f<0.05f?0.05f:(f>4.0f?4.0f:f); } else return false; }
        else if (!std::strcmp(a,"--no-onepos")) {
            std::printf("[ra] --no-onepos: one-position collapse off — warp_result reverts to the two-sample blend (the double-exposure ghost, byte-identical)\n");
            c.onepos=false; c.no_onepos=true;
        }
        else if (!std::strcmp(a,"--disoccl-commit")) {
            std::printf("[ra] --disoccl-commit: occlusion-aware ONE-SIDED commit — replaces the two symmetric `w_sum<1e-4` BLEND fallbacks (object + background layers) that paint the disocclusion crescent/estela. Each contaminated-band pixel commits to the single correct side by the dis_fwd/dis_bwd asymmetry (leading→cur, trailing→prev; bg→less-contaminated side), softly gated so a true tie keeps the phase blend. Reuses the dissidence (no new tap, runtime ~0). Requires --matte. DEFAULT OFF — byte-identical when off.\n");
            c.disoccl_commit=true;
        }
        else if (!std::strcmp(a,"--expire"))    { std::printf("[ra] --expire: already default\n"); }
        else if (!std::strcmp(a,"--no-expire")) {
            std::printf("[ra] --no-expire: stigmergy expiration off — cross-pair EMAs decay through contradictions\n");
            c.expire=false; c.no_expire=true;
        }
        else if (!std::strcmp(a,"--objdump"))          { if(auto v=next(a)) c.objdump_n=std::atoi(v); else return false; }   // diagnostic
        else if (!std::strcmp(a,"--pairdump"))         { if(auto v=next(a)) c.pairdump_n=std::atoi(v); else return false; }  // diagnostic
        else if (!std::strcmp(a,"--outdump"))          { if(auto v=next(a)) c.outdump_n=std::atoi(v); else return false; }   // diagnostic
        else if (!std::strcmp(a,"--qdump"))            { const char* d=next(a); if(!d) return false; const char* n=next(a); if(!n) return false;   // --qdump <dir> N
                                                         std::snprintf(c.qdump_dir,sizeof(c.qdump_dir),"%s",d); c.qdump_n=std::atoi(n); }
        else if (!std::strcmp(a,"--phaselog"))         { if(auto v=next(a)) c.phaselog_n=std::atoi(v); else return false; }  // instrument
        else if (!std::strcmp(a,"--persist-reset"))    { std::printf("[ra] --persist-reset: already default\n"); }
        else if (!std::strcmp(a,"--no-persist-reset")) {
            std::printf("[ra] --no-persist-reset: membership-beats-inertia off — the HUD shield blocks mover interiors\n");
            c.persist_reset=false; c.no_persist_reset=true;
        }
        else if (!std::strcmp(a,"--change-gate"))    { std::printf("[ra] --change-gate: already default\n"); }
        else if (!std::strcmp(a,"--no-change-gate")) {
            std::printf("[ra] --no-change-gate: the dissidence masks no longer require changed content (raw halo masks)\n");
            c.change_gate=false; c.no_change_gate=true;
        }
        else if (!std::strcmp(a,"--phase-anchor"))    { std::printf("[ra] --phase-anchor: already default\n"); }
        else if (!std::strcmp(a,"--no-phase-anchor")) {
            std::printf("[ra] --no-phase-anchor: primary MV stays prev-anchored forward (no cur-anchored re-anchoring at high phase)\n");
            c.phase_anchor=false; c.no_phase_anchor=true;
        }
        else if (!std::strcmp(a,"--ambig"))    { std::printf("[ra] --ambig: already default\n"); }
        else if (!std::strcmp(a,"--no-ambig")) {
            std::printf("[ra] --no-ambig: the matcher's raw pick (no second-best candidate arbitration of SAD ties)\n");
            c.ambig=false; c.no_ambig=true;
        }
        else if (!std::strcmp(a,"--phasefix")) { std::printf("[ra] --phasefix: already default\n"); }
        else if (!std::strcmp(a,"--no-phasefix")) {
            std::printf("[ra] --no-phasefix: the simple D anchor (delay_ema*1.2+0.5*span) + overshoot freeze — the truncated ladder\n");
            c.phasefix=false; c.no_phasefix=true;
        }
        else if (!std::strcmp(a,"--sync-clock")) {
            std::printf("[ra] --sync-clock: free-running content-clock NCO + 2nd-order PLL (replaces the per-pair (now-D)/span.T phase) — kills the real-frame-boundary start-phase jump on low-fps jittery sources\n");
            c.sync_clock=true;
        }
        else if (!std::strcmp(a,"--no-sync-clock")) {
            std::printf("[ra] --no-sync-clock: cadence fix OFF — reverts to the per-pair (now-D)/span.T phase path (byte-identical pacing). DEFAULT is ON.\n");
            c.sync_clock=false;
        }
        else if (!std::strcmp(a,"--wsub")) {
            std::printf("[ra] --wsub: per-tick warp-lambda sub-timings on — stats line gains wsub(up:F rec:F gpu:F prs:F) ms EMAs\n");
            c.wsub=true;
        }
        else if (!std::strcmp(a,"--fsub")) {
            std::printf("[ra] --fsub: F per-pair split on — stats line gains fsub(flow:F pair:F cpu:F) ms (flow=blocking fwd fence wait; cpu=pair-flow)\n");
            c.fsub=true;
        }
        else if (!std::strcmp(a,"--fwd-pipeline")) {
            std::printf("[ra] --fwd-pipeline: forward-pass pipelining ON (WAP only) — pair N fwd submitted no-wait; pair N-1 CPU tail + publish deferred one pair to overlap N's GPU match; +1-pair publish lag. A do_bwd pair drains to serial (single ofp/2 Bframe slots). DEFAULT is OFF (serial).\n");
            c.fwd_pipeline=true;
        }
        else if (!std::strcmp(a,"--tiers")) { std::printf("[ra] --tiers: already default\n"); }
        else if (!std::strcmp(a,"--no-tiers")) {
            std::printf("[ra] --no-tiers: pressure tiers off — bwd-skip (tier 1) only; objects/memory run every pair under all pressure\n");
            c.tiers=false; c.no_tiers=true;
        }
        else if (!std::strcmp(a,"--deficit-tier")) {
            std::printf("[ra] --deficit-tier: recovery tier above the pressure ladder; under sustained deficit (t_pair_ema>0.92x budget) sheds object_repair+memory EVERY pair (keeps gme+matte+warp+inertia = raw-flow WAP), holds with a dwell, releases only when eased (<0.55x budget AND dwell expired). Quality cost ONLY under load. DEFAULT ON (--no-deficit-tier disables).\n");
            c.deficit_tier=true;
        }
        else if (!std::strcmp(a,"--gme-irls2")) {
            std::printf("[ra] --gme-irls2: gme_fit_affine 2 IRLS passes (OLS + 1 reweight) instead of 3 — ~0.3-0.7ms/pair off the CPU floor on every pair. DEFAULT is 3.\n");
            c.gme_irls2=true;
        }
        else if (!std::strcmp(a,"--gme-gpu")) {
            std::printf("[ra] --gme-gpu: offload gme_fit_affine (the ~2.7ms/pair affine IRLS fit) onto device B (the 1080 Ti, where MV/SAD already live). reduce->solve x3 + dissidence chained in ONE B command buffer; only the 6-float model reads back (for object_repair), the dis-mask is GPU-produced. Frees the CPU tail (the game's CPU). DEFAULT ON, bit-identical; auto CPU fallback if device B unavailable.\n");
            c.gme_gpu=true;
        }
        else if (!std::strcmp(a,"--no-gme-gpu")) {
            std::printf("[ra] --no-gme-gpu: the per-pair affine fit runs on the CPU (gme_fit_affine, byte-identical). DEFAULT is ON.\n");
            c.gme_gpu=false; c.gme_gpu_verify=false;
        }
        else if (!std::strcmp(a,"--gme-gpu-verify")) {
            std::printf("[ra] --gme-gpu-verify: run BOTH the GPU gme AND the CPU gme on each pair + print model rel-diff + dis-mask flip count (precision check). Implies --gme-gpu.\n");
            c.gme_gpu=true; c.gme_gpu_verify=true;
        }
        else if (!std::strcmp(a,"--igpu-field")) {
            std::printf("[ra] --igpu-field: iGPU computes an image-derived contour field (Sobel) on G.q2 after the convert, into hostFIELD. DEFAULT ON (--no-igpu-field disables); consumers read binding 11: afill (visualizer), bg-snap, band-xfade, disoccl-hardpick, multicand edge-gate. Needs the iGPU convert path.\n");
            c.igpu_field=true;
        }
        else if (!std::strcmp(a,"--igpu-field-verify")) {
            std::printf("[ra] --igpu-field-verify: byte gate — re-derive the field on the CPU + compare to the GPU field. Implies --igpu-field.\n");
            c.igpu_field=true; c.igpu_field_verify=true;
        }
        else if (!std::strcmp(a,"--afill")) {
            std::printf("[ra] --afill: A reads the iGPU contour field + tints the boundary band onto wapOutA IN-PLACE (the field VISUALIZER: eye-validate iGPU Sobel ↔ object boundaries on the presented frame). Auto-enables --igpu-field (A needs the field); DEFAULT OFF, byte-identical off, needs the iGPU convert path.\n");
            c.afill=true; c.igpu_field=true;
        }
        else if (!std::strcmp(a,"--afill-strength")) { if(auto v=next(a)){ float f=(float)std::atof(v); c.afill_strength = f<0.f?0.f:(f>1.f?1.f:f); } else return false; }   // tint visibility (0=byte-identical)
        else if (!std::strcmp(a,"--afill-edge-norm")) { if(auto v=next(a)){ float f=(float)std::atof(v); c.afill_edge_norm = f<0.001f?0.001f:(f>1.f?1.f:f); } else return false; }   // contour-dist→[0,1] normalizer
        else if (!std::strcmp(a,"--mv-subpel")) {
            std::printf("[ra] --mv-subpel: sub-pixel MV refinement at the finest match level (parabolic SAD-peak interpolation → fractional best_mv into the rg16f field the WAP warp already samples bilinearly). Reduces flow-error across motion types (pan -38%%, zoom -34%%, orbit -25%%, mixed -19%%) and raises warp PSNR — attacks the crossfade at its flow-error root. ~4 extra pure-SAD blocks at the finest level only. DEFAULT ON (--no-mv-subpel disables).\n");
            c.mv_subpel=true;
        }
        else if (!std::strcmp(a,"--mv-candsel")) {
            std::printf("[ra] --mv-candsel: holonic CANDIDATE-SELECTION — at the finest match level, an ambiguous textureless-interior / aperture tile (where the local +-R search finds a noisy best_mv that the symmetric blend then crossfades) ADOPTS the coherent coarse region-predictor MV instead. The region holon offers the model MV; the tile holon adopts it only when its own match is NOT confidently better (best_sad < sad_pred*(1-0.12)). Composes with --mv-subpel (candsel selects the integer source, subpel refines it). One extra pure-SAD + compare at the finest level only. DEFAULT OFF (byte-identical: the candsel==0 store is the exact integer best_mv).\n");
            c.mv_candsel=true;
        }
        else if (!std::strcmp(a,"--obj-fill-rim")) {
            std::printf("[ra] --obj-fill-rim: coherent-MV interior infill — the object-holon stamps its single rigid slot MV across the WHOLE silhouette INCLUDING the rim band + the >static excluded annulus, but ONLY for a COHERENT rim (rim_spread < %.2gpx = a rigid translating object). A_samp/B_samp then ALIGN -> the symmetric blend cannot double-expose the disc (the half-moon crescent dies). Stands down on a scaling/rotating/articulated rim. DEFAULT OFF -- byte-identical when off.\n", (double)kObjRimSpreadMin);
            c.obj_fill_rim=true;
        }
        else if (!std::strcmp(a,"--memory"))    { std::printf("[ra] --memory: already default\n"); }
        else if (!std::strcmp(a,"--no-memory")) {
            std::printf("[ra] --no-memory: scene-holon silhouette memory off — the mask is the fresh pair only\n");
            c.scene_memory=false; c.no_memory=true;
        }
        else { int r=parse_extra(a); if(r>0) return false; if(r<0){ std::printf("[ra] unknown option: %s\n",a); return false; } }   // try the parse_extra matcher; r==0 matched OK, r==1 missing-arg, r<0 truly unknown
    }
    // DDA es la ruta de captura PRIMARIA: --window significa "captura el MONITOR donde está esa ventana,
    // vía DDA" (se resuelve en main() → cfg.cap_mon; DDA llega al refresh, WGC-ventana se auto-capa ~125
    // por el anti-duplicate-flood). Para forzar captura SOLO-de-la-ventana (avanzado), pasa
    // --capture-api wgc EXPLÍCITO junto a --window.
    if (c.window_substr[0] && c.capture_api==CA_WGC) {
        std::printf("[ra] --window + --capture-api wgc: captura solo-de-la-ventana (WGC, ruta avanzada)\n");
    } else if (c.window_substr[0]) {
        std::printf("[ra] --window '%s': capturara su MONITOR via DDA (usa --capture-api wgc para captura solo-de-la-ventana)\n", c.window_substr);
    }
    // The post-parse cascades + the derived c.d.* predicates live in ONE site, resolve_config() above
    // (re-callable from the runtime degrade in main.cpp). See cli.hpp. This is the central "decode" —
    // every layer reads the resolved flags + c.d.* instead of re-deriving a gate.
    resolve_config(c);
    return true;
}
// Made with my soul - Swately <3
