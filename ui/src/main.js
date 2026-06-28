// PhyriadFG launcher — frontend logic.
// Builds the option controls from the flag model, assembles the launch args, streams
// the FG's live stdout (fps entrada -> salida + log), and drives start/stop.

const TAURI = window.__TAURI__;
const invoke = TAURI?.core?.invoke;
const listen = TAURI?.event?.listen;

const DEFAULT_EXE = "G:\\PhyriadFG\\build\\phyriad_fg.exe";

// ── Estado del proceso + auto-reinicio ─────────────────────────────────────────
// Declarados aquí arriba (no en el bloque de botones) porque updatePreview() —que llama a
// maybeScheduleRestart()— se ejecuta una vez al cargar, ANTES de ese bloque; con `let` más
// abajo caerían en la zona muerta temporal (TDZ) y lanzarían ReferenceError.
//   running      : ¿hay un FG vivo? (lo mantiene setRunning()).
//   autoRestart  : control SOLO de UI; con el FG vivo, cambiar un flag reinicia el FG.
//   restarting   : true mientras un reinicio está en vuelo, para que "fg-exit" ignore el
//                  cierre del hijo viejo y la UI NO parpadee a "detenido".
//   restartTimer : timer de debounce (reinicia 1 s DESPUÉS del último cambio, no por tecla).
let running = false;
let autoRestart = false;
let restarting = false;
let restartTimer = null;

// ── Flag model ───────────────────────────────────────────────────────────────
// type: 'switch'      bool, default OFF -> emits `flag` when ON
//       'switch-off'  quality default ON -> emits the `--no-X` `flag` when turned OFF
//       'switch-on'   bool default OFF -> emits `flag` when ON (e.g. --matte)
//       'select'      enum -> emits `flag value` when value != default
//       'number'      int  -> emits `flag value` when value != default and non-empty
//       'text'        str  -> emits `flag value` when non-empty
const GROUPS = [
  {
    title: "Capture",
    controls: [
      {
        flag: "--capture-api", type: "select", default: "dd",
        name: "Capture API",
        options: [
          { value: "dd", label: "dd (Desktop Duplication)" },
          { value: "wgc", label: "wgc (Windows Graphics Capture)" },
        ],
        desc: "Capture backend: dd (default) or wgc (HW-accel flip/overlay).",
      },
      {
        flag: "--ingest-async", type: "switch", default: false,
        name: "Asynchronous ingest (DDA)",
        desc: "Decouples DDA ingest: a thread that only acquires into a ring + a parallel convert worker (drop-to-newest) that publishes instantly. Raises ingest toward the delivered rate (watch capture vs ingest). DDA only; default off.",
      },
      {
        flag: "--dedup", type: "switch", default: true,
        name: "Drop duplicates (DDA)",
        desc: "DDA composes the desktop at the monitor's refresh rate, so many captures are content DUPLICATES (DWM re-composes the same game frame, which renders fewer unique frames). Drops the duplicates from the pipeline so FG interpolates between TRUE uniques instead of zero-motion pairs. The capture readout always shows the real unique rate. DDA only; default off.",
      },
      {
        flag: "--window", type: "text", default: "",
        name: "Window (substring)", placeholder: "e.g. Battlefield",
        desc: "WGC: captures the window whose title contains this substring (implies --capture-api wgc).",
      },
      {
        flag: "--monitor", type: "number", default: "0", min: 0, step: 1,
        name: "Capture monitor", placeholder: "0",
        desc: "Captures from DXGI output M (default 0).",
      },
      {
        flag: "--present-monitor", type: "number", default: "", min: 0, step: 1,
        name: "Presentation monitor", placeholder: "= capture",
        desc: "Presents on output P (default: the same as the capture output).",
      },
      {
        flag: "--cap-slots", type: "number", default: "0", min: 0, max: 32, step: 1,
        name: "Capture ring slots", placeholder: "0 = auto",
        desc: "Override the capture ring's auto depth (0=auto ~src/10+4, max 32). These are frame slots in RAM.",
      },
      {
        flag: "--ingest-backlog", type: "number", default: "3", min: 1, max: 3, step: 1,
        name: "Ingest backlog",
        desc: "In-order drain depth of the ingest (1=fresher/more span-2 jumps, 3=smoother/more latency). The DOMINANT lever for freshage/input-lag.",
      },
      {
        flag: "--copy-fence", type: "switch", default: false, name: "Copy-fence (WGC)",
        desc: "Event-driven WGC copy pickup (fence+event instead of busy-poll Map). Needs D3D11.4; force-off otherwise.",
      },
      {
        flag: "--copy-device", type: "switch", default: false, name: "Copy on 2nd device (WGC)",
        desc: "Runs the WGC staging CopyResource on a separate D3D11 device of the same adapter (decouples the copy queue). WGC-only; force-off if it fails.",
      },
      {
        flag: "--dpi-probe", type: "switch", default: false, name: "DPI probe (diag)",
        desc: "Read-only diagnostic: logs GetClientRect/GetDpiForWindow/Size for the capture crop on high-DPI.",
      },
      {
        flag: "--cap-route-probe", type: "switch", default: false, name: "Cap-route probe",
        desc: "Prints a capability-based routing decision (has_fp16/dp4a) over the FG's VDevs. MEASUREMENT-ONLY, inert (A/B/G roles fixed).",
      },
    ],
    windowSelector: true,
    extra: "monitors",
  },
  {
    title: "GPU",
    controls: [
      {
        flag: "--fg-gpu", type: "select", default: "auto",
        name: "Frame-gen GPU",
        options: [
          { value: "auto", label: "auto (by load)" },
          { value: "primary", label: "primary" },
          { value: "assist", label: "assist" },
        ],
        desc: "Frame-gen device: auto (default), primary or assist.",
      },
      {
        flag: "--convert-gpu", type: "select", default: "igpu",
        name: "Convert GPU",
        options: [
          { value: "igpu", label: "igpu (fused convert, no PCIe)" },
          { value: "primary", label: "primary" },
        ],
        desc: "Where convert+pack runs: igpu (default, zero-PCIe) or primary.",
      },
      {
        flag: "--present-gpu", type: "text", default: "",
        name: "Presentation GPU", placeholder: "(inherited)",
        desc: "INHERITED / IGNORED: presentation is the adapter that owns the panel since STAGE-45b.",
      },
      {
        flag: "--force-single-gpu", type: "switch", default: false,
        name: "Force single GPU",
        desc: "Drives the single-GPU path (suppresses the 2nd discrete + iGPU; all roles on device A).",
      },
      {
        flag: "--assist-gpu", type: "text", default: "",
        name: "Assist GPU (name)", placeholder: "name fragment",
        desc: "GPU name fragment for the frame-gen (assist device selection).",
      },
    ],
  },
  {
    title: "Frame-gen",
    controls: [
      {
        flag: "--fg-factor", type: "select", default: "2",
        name: "FG factor",
        options: [
          { value: "2", label: "2x (default)" },
          { value: "3", label: "3x" },
          { value: "4", label: "4x" },
          { value: "5", label: "5x" },
          { value: "6", label: "6x" },
          { value: "8", label: "8x" },
          { value: "auto", label: "auto (measured)" },
        ],
        desc: "Output multiplier: 2=2x (default), 3=3x, ... auto=measured.",
      },
      {
        flag: "--flow-scale", type: "select", default: "1",
        name: "Flow scale",
        options: [
          { value: "1", label: "1 (byte-identical)" },
          { value: "2", label: "2 (1/2 res)" },
          { value: "4", label: "4 (1/4 res)" },
          { value: "auto", label: "auto" },
        ],
        desc: "DRS coin-3: runs the flow + CPU tail per pair over an MV grid at 1/N resolution (~N^2 cheaper; WAP only).",
      },
      {
        flag: "--warp-scale", type: "select", default: "1",
        name: "Warp scale",
        options: [
          { value: "1", label: "1 (byte-identical)" },
          { value: "2", label: "2" },
          { value: "4", label: "4" },
        ],
        desc: "Runs OUR warp at WW/N and rescales (PARKED-NEGATIVE: blurry to the operator's eye on BF6 4K; doesn't touch latency).",
      },
      {
        flag: "--nvofa", type: "switch", default: false,
        name: "NVIDIA OFA (HW)",
        desc: "Replaces the classic flow with NVIDIA's HW Optical Flow Accelerator (4090). Auto-fallback if VK_NV_optical_flow is absent.",
      },
      {
        flag: "--nvofa-cost-scale", type: "number", default: "0.5", min: 0, max: 16, step: 0.1,
        name: "NVOFA cost-scale",
        desc: "Remap of the OFA cost (0..255) -> sad_best. PLACEHOLDER (eye-calibration). Only with --nvofa.",
      },
      {
        flag: "--nvofa-sadz-scale", type: "number", default: "4.0", min: 0, max: 64, step: 0.5,
        name: "NVOFA sadz-scale",
        desc: "Block SUM|A-B| -> 8x8 magnitude. PLACEHOLDER (eye-calibration). Only with --nvofa.",
      },
      {
        flag: "--flow-scale-target-mp", type: "number", default: "2.1", min: 0.25, max: 12, step: 0.1,
        name: "Flow-scale auto target (MP)",
        desc: "Flow-work megapixel target for --flow-scale auto (1080p ~div=1; 1440p/4K drop lower). The quality<->latency slider.",
      },
    ],
  },
  {
    title: "Presentation / Pacing",
    controls: [
      {
        flag: "--hdr", type: "switch", default: false,
        name: "HDR (FP16 scRGB)",
        desc: "Presents an FP16 scRGB swapchain for HDR (alias of --present-fp16). INERT without an HDR display+content.",
      },
      {
        flag: "--present-own-window", type: "switch", default: false,
        name: "Own window",
        desc: "Presents our own borderless flip HWND (Independent-Flip plane, LSFG topology). Click-through.",
      },
      {
        flag: "--no-async-present", type: "switch-off", default: false,
        name: "Asynchronous present",
        desc: "DEFAULT ON (PART-A). Decouples present from the warp fence (non-blocking present + drop-interpolated; ~1 frame of latency). Off = blocking SYNCHRONOUS path (byte-identical). Auto-enabled by --target-output-fps/--fdrop/--upload-xfer/--rfp/--motion-fallback/--shallow-queue.",
      },
      {
        flag: "--cap-fps", type: "number", default: "", min: 1, step: 1,
        name: "Capture cap (fps)", placeholder: "0 = off",
        desc: "Limits WGC capture to N fps (MinUpdateInterval=1/N). Diagnostic; doesn't change the algorithm.",
      },
      {
        flag: "--refresh-hz", type: "number", default: "240", min: 1, step: 1,
        name: "Clock refresh (Hz)", placeholder: "240",
        desc: "Output clock tick rate (default 240).",
      },
      {
        flag: "--target-output-fps", type: "number", default: "", min: 1, step: 1,
        name: "Target output FPS", placeholder: "0 = off",
        desc: "Fractional output-rate controller (stable cadence). Auto-enables --async-present. Never caps the game.",
      },
      {
        flag: "--s2-sustain", type: "number", default: "0.93", min: 0.5, max: 1, step: 0.01,
        name: "Sustain frac (target-fps)",
        desc: "Fraction of the measured achievable rate that --target-output-fps aims for ('a steady 180 beats an erratic 191') [0.5,1]. Only with target-output-fps.",
      },
      {
        flag: "--present-sync", type: "select", default: "0",
        name: "Sync interval",
        options: [
          { value: "0", label: "0 (immediate)" },
          { value: "1", label: "1 (pace to compositor)" },
          { value: "2", label: "2" },
          { value: "3", label: "3" },
          { value: "4", label: "4" },
        ],
        desc: "Present(sync_interval): 0 = present-immediately (default, over-presents); 1 = pace to the compositor.",
      },
      {
        flag: "--present-waitable", type: "switch", default: false, name: "Swapchain waitable",
        desc: "SetMaximumFrameLatency(1) + wait-before-present: reduces jitter WITHIN DWM composition (PARTIAL; doesn't reach Independent Flip).",
      },
      {
        flag: "--present-colorspace", type: "select", default: "off", name: "Overlay colorspace",
        options: [
          { value: "off", label: "off (no-op)" },
          { value: "srgb", label: "srgb (declares sRGB)" },
        ],
        desc: "Declares the overlay colorspace (sRGB) so an HDR desktop composes the SDR overlay without washout. Soft if unsupported.",
      },
      {
        flag: "--indicator", type: "switch", default: false, name: "Indicator (WIP)",
        desc: "On-screen PhyriadFG-active mark (WIP, passthrough-only; the stamp on the warp path failed DEVICE_LOST).",
      },
      {
        flag: "--no-upscale", type: "switch", default: false, name: "No upscale",
        desc: "Presents at the working resolution (the bridge blit scales). Emits --no-upscale.",
      },
      {
        flag: "--upscale-lanczos", type: "switch", default: false, name: "Upscale Lanczos-2",
        desc: "Lanczos-2 upscale (sharper, slower).",
      },
    ],
  },
  {
    title: "Quality (default on)",
    note: "Turning a switch off emits its --no-X. (Except Matte, which is off by default and emits --matte.)",
    controls: [
      { flag: "--no-warp-at-presenter", type: "switch-off", default: true, name: "Warp-at-presenter (WAP)",
        desc: "Per-tick re-warp to the exact phase. Off = grid mode (disables bidir/fill-div/rescue/mv-guided/gme/matte/inertia)." },
      { flag: "--no-gme", type: "switch-off", default: true, name: "GME (global model)",
        desc: "Per-pair global affine motion model. Off also disables matte (cascade)." },
      { flag: "--no-bidir", type: "switch-off", default: true, name: "Bidirectional flow",
        desc: "Bidirectional flow + occlusion classification. Off also disables fill-div and matte (cascade)." },
      { flag: "--matte", type: "switch-on", default: false, name: "Fluid matte",
        desc: "Two-layer fluid composite. DEFAULT OFF (the operator's A/B found --no-matte better: it doubled the figure). --matte re-enables it (needs gme + bidir)." },
      { flag: "--no-mv-guided", type: "switch-off", default: true, name: "Color-guided MV",
        desc: "MV assignment guided by color membership. Off = blind median/linear." },
      { flag: "--no-rescue", type: "switch-off", default: true, name: "Candidate rescue",
        desc: "Rescue via neighbor-block MVs. Off = byte-identical." },
      { flag: "--no-inertia", type: "switch-off", default: true, name: "Inertia (persistence)",
        desc: "Motion-persistence prior (state hysteresis). Off = byte-identical." },
      { flag: "--inertia-thresh", type: "number", default: "0.50", min: 0.1, max: 1, step: 0.05, name: "Inertia: threshold",
        desc: "Persistence cutoff (R8-norm [0.1,1]). 0.5 ~ 8 static pairs." },
      { flag: "--no-stasis", type: "switch-off", default: true, name: "Stasis (bypass HUD)",
        desc: "Stasis layer: a block with sad_zero<=thresh presents the real directly. Off = byte-identical." },
      { flag: "--stasis-thresh", type: "number", default: "0.50", min: 0.05, max: 8, step: 0.05, name: "Stasis: threshold",
        desc: "sad_zero cutoff (per-block SUM|A-B| [0.05,8]). ~0 = identical block." },
    ],
  },
  {
    title: "Flow / Vectors (MV)",
    controls: [
      { flag: "--no-mv-subpel", type: "switch-off", default: true, name: "MV sub-pixel",
        desc: "DEFAULT ON. Sub-pixel refinement (parabola of the SAD peak) -> fractional MV. Off = integer best_mv." },
      { flag: "--mv-candsel", type: "switch", default: true, name: "MV candidate-select",
        desc: "Ambiguous interior tiles adopt the coarse region MV (kills the aperture crossfade). Composes with mv-subpel." },
      { flag: "--mv-median", type: "switch", default: false, name: "MV median 3x3",
        desc: "Blind 3x3 vector median over the MV field before the warp (superseded by mv-guided)." },
      { flag: "--mv-smooth", type: "number", default: "0", min: 0, max: 1, step: 0.1, name: "MV smooth (EMA)",
        desc: "Temporal MV EMA alpha (0=off; ~0.6 damps tile jitter)." },
      { flag: "--mv-prior", type: "switch", default: false, name: "MV temporal prior",
        desc: "Temporal MV prior in the matcher (dual-centre, self-heals on cuts). Ignored with bidir (default)." },
      { flag: "--mv-sim", type: "number", default: "0.10", min: 0.02, max: 0.5, step: 0.01, name: "MV-guided: color band",
        desc: "Color-membership band for mv-guided (max-ch [0.02,0.5])." },
      { flag: "--residual-ceil", type: "number", default: "32", min: 0, step: 1, name: "Gate: residual-ceil",
        desc: "Gate (a): max sad_best (default 32)." },
      { flag: "--conf-improv", type: "number", default: "0.20", min: 0, max: 1, step: 0.01, name: "Gate: conf-improv",
        desc: "Gate (b): minimum SAD improvement (fraction, default 0.20)." },
      { flag: "--agreement", type: "number", default: "0.05", min: 0, max: 1, step: 0.01, name: "Gate: agreement",
        desc: "Agreement gate: max d per tile (default 0.05)." },
      { flag: "--occl-thresh", type: "number", default: "1.5", min: 0.25, max: 8, step: 0.1, name: "Occlusion thresh",
        desc: "Round-trip fwd<->bwd threshold (px [0.25,8]). Only with bidir." },
      { flag: "--div-eps", type: "number", default: "0.05", min: 0.005, max: 1, step: 0.01, name: "Divergence eps",
        desc: "Divergence band (px/texel [0.005,1]); used by fill-div." },
      { flag: "--fill-div", type: "switch", default: false, name: "Fill-div",
        desc: "Divergence-driven disocclusion pick. DEFAULT OFF (WASH audit); requires bidir." },
    ],
  },
  {
    title: "Warp / Blend",
    controls: [
      { flag: "--no-soft-gate", type: "switch-off", default: true, name: "Soft-gate",
        desc: "DEFAULT ON. Continuous warp gate. Off = binary keep/freeze per 8x8 block." },
      { flag: "--no-commit", type: "switch-off", default: true, name: "Warp commit",
        desc: "DEFAULT ON (commit_thresh 0.08 + commit_real). Off = averaging + disables rescue + appearance." },
      { flag: "--commit-thresh", type: "number", default: "0.08", min: 0.005, max: 0.5, step: 0.01, name: "Commit: threshold",
        desc: "Commit threshold (color units [0.005,0.5]; 0 = averaging). --no-commit wins if both." },
      { flag: "--no-appearance", type: "switch-off", default: true, name: "Appearance re-blend",
        desc: "DEFAULT ON. Tonal re-blend on committed pixels (a brightness-constancy violator)." },
      { flag: "--appear-band", type: "number", default: "0.10", min: 0.02, max: 0.5, step: 0.01, name: "Appearance: band",
        desc: "Appearance band (max-ch [0.02,0.5])." },
      { flag: "--no-commit-default", type: "switch-off", default: true, name: "Commit-default",
        desc: "DEFAULT ON. The warp is the default; only evidenced garbage (large d_pixel) blends." },
      { flag: "--no-onepos", type: "switch-off", default: true, name: "One-position",
        desc: "DEFAULT ON. One-position-per-pixel collapse (anti double-exposure of the warp itself)." },
      { flag: "--onepos-band", type: "number", default: "1.0", min: 0.05, max: 4, step: 0.05, name: "One-pos: band",
        desc: "Collapse onset scale ([0.05,4]; 1.0 = STAGE-81, lower = collapses faint crescents)." },
      { flag: "--no-member-commit", type: "switch-off", default: true, name: "Member-commit",
        desc: "DEFAULT ON. Membership-beats-the-blend in flat object interiors (anti ghost-step)." },
      { flag: "--no-phase-anchor", type: "switch-off", default: true, name: "Phase-anchor",
        desc: "DEFAULT ON. Primary MV anchored to the phase (cur-anchored at high phase). Needs bidir." },
      { flag: "--no-vblend", type: "switch-off", default: true, name: "V-blend (velocity)",
        desc: "DEFAULT ON. Near pair end, the MV tilts toward the next pair's velocity (anti pulse)." },
      { flag: "--vblend-t0", type: "number", default: "0.6", min: 0, max: 0.95, step: 0.05, name: "V-blend: t0",
        desc: "Phase where the tilt ramp begins [0,0.95]. Only with vblend." },
      { flag: "--vblend-strength", type: "number", default: "0.5", min: 0, max: 1, step: 0.05, name: "V-blend: strength",
        desc: "Max tilt weight at t=1 [0,1]. Only with vblend." },
      { flag: "--vblend-exact", type: "switch", default: false, name: "V-blend exact",
        desc: "EXACT velocity-continuity (real MV of the next pair; +1 source-frame of latency). Implies vblend." },
      { flag: "--no-bg-snap", type: "switch-off", default: true, name: "BG-snap",
        desc: "DEFAULT ON. In the iGPU contour band, snaps background MVs to the gme model (kills disocclusion gravity)." },
      { flag: "--bg-snap-strength", type: "number", default: "1.0", min: 0, max: 4, step: 0.5, name: "BG-snap: strength",
        desc: "Snap weight scale [0,4] (1=soft, 2-4=progressively harder snap)." },
      { flag: "--bg-snap-norm", type: "number", default: "0.04", min: 0.001, max: 1, step: 0.01, name: "BG-snap: norm",
        desc: "Contour-distance->[0,1] normalizer of the band ([0.001,1])." },
      { flag: "--band-xfade-strength", type: "number", default: "1.0", min: 0, max: 1, step: 0.05, name: "Band-xfade (gravity)",
        desc: "Gravity-cancellation crossfade in the band (DEFAULT 1.0; 0 = off = equivalent to --no-band-xfade)." },
      { flag: "--ts-smooth", type: "number", default: "0.1", min: 0, max: 1, step: 0.05, name: "TS-smooth",
        desc: "Adaptive temporal smoothing gated to garbage pixels (DEFAULT 0.1; 0 = off; 0.5 over-smooths)." },
      { flag: "--disoccl-hardpick", type: "number", default: "0", min: 0, max: 1, step: 0.05, name: "Disoccl hard-pick",
        desc: "Hard-pick with a Sobel edge gate in the bidir band (0=off; requires bidir + iGPU field)." },
    ],
  },
  {
    title: "GME / Objects / Matte",
    note: "Matte sub-stack: most are children of --matte/--gme/--bidir/--objects and cascade off if the parent is off.",
    controls: [
      { flag: "--no-gme-gpu", type: "switch-off", default: true, name: "GME on GPU B",
        desc: "DEFAULT ON. Offloads the gme affine fit to GPU B (1080 Ti, where MV/SAD live). Off = CPU. Auto-fallback to CPU if B is absent." },
      { flag: "--gme-gpu-verify", type: "switch", default: false, name: "GME-GPU verify",
        desc: "Runs GPU+CPU gme and prints rel-diff + dis-mask flips. Implies gme-gpu." },
      { flag: "--gme-irls2", type: "switch", default: false, name: "GME IRLS 2-pass",
        desc: "gme_fit with 2 IRLS passes instead of 3 (~0.3-0.7ms/pair). Default 3 (byte-identical)." },
      { flag: "--matte-thresh", type: "number", default: "0.25", min: 0.05, max: 1, step: 0.05, name: "Matte: threshold",
        desc: "Matte dissent cutoff (R8-norm [0.05,1])." },
      { flag: "--mass-k", type: "number", default: "0.5", min: 0, max: 2, step: 0.1, name: "Matte: mass-k",
        desc: "Matte mass-conservation feedback gain [0,2] (0=off/lerp)." },
      { flag: "--obj-fill-rim", type: "switch", default: true, name: "Obj fill-rim",
        desc: "Coherent MV infill across the ENTIRE interior of a rigid object (kills the half-moon crescent). Gives up on a non-rigid rim." },
      { flag: "--disoccl-commit", type: "switch", default: false, name: "Disoccl-commit",
        desc: "One-sided commit in the disocclusion band (replaces the symmetric blend-fallbacks). Requires --matte." },
      { flag: "--no-objects", type: "switch-off", default: true, name: "Object-holon",
        desc: "DEFAULT ON. Clustering + repair via motion inheritance. Off = exact pre-stage." },
      { flag: "--no-shapefield", type: "switch-off", default: true, name: "Shape-field",
        desc: "DEFAULT ON (child of objects). Off = rigid single-MV inheritance (STAGE-61)." },
      { flag: "--no-crescent", type: "switch-off", default: true, name: "Crescent (bg)",
        desc: "DEFAULT ON. Crescent-driven background fetch (matte). Off = blend (1-t,t)." },
      { flag: "--no-travel", type: "switch-off", default: true, name: "Travel (occupancy)",
        desc: "DEFAULT ON. Traveling-silhouette occupancy (matte). Off = STAGE-59 lerp." },
      { flag: "--no-contour", type: "switch-off", default: true, name: "Contour marriage",
        desc: "DEFAULT ON. Contour-band arbitration by color affinity (matte). Off = binary." },
      { flag: "--no-obj-crescent", type: "switch-off", default: true, name: "Obj-crescent",
        desc: "DEFAULT ON. Crescent-side weighting for the OBJECT layer. Off = (1-t,t)." },
      { flag: "--no-memory", type: "switch-off", default: true, name: "Scene memory",
        desc: "DEFAULT ON (child of objects). Advected silhouette memory. Off = only the fresh pair." },
      { flag: "--no-persist-reset", type: "switch-off", default: true, name: "Persist-reset",
        desc: "DEFAULT ON. Membership-beats-inertia (resets the HUD shield in mover interiors)." },
      { flag: "--no-change-gate", type: "switch-off", default: true, name: "Change-gate",
        desc: "DEFAULT ON. Dissent masks require CHANGED content (anti-halo). Off = raw masks." },
      { flag: "--no-expire", type: "switch-off", default: true, name: "Expire (stigmergy)",
        desc: "DEFAULT ON. Expiry of cross-pair EMAs on contradictions. Off = decays through." },
      { flag: "--no-ambig", type: "switch-off", default: true, name: "Ambiguity (2nd cand)",
        desc: "DEFAULT ON (child of gme). Second-best candidate arbitration on SAD ties (anti periodic-texture)." },
    ],
  },
  {
    title: "Multi-candidate",
    controls: [
      { flag: "--multicand", type: "switch", default: false, name: "Multi-candidate",
        desc: "Multi-candidate medoid selection (generate+discriminate+SELECT). DEFAULT OFF (unvalidated edge-gate)." },
      { flag: "--mc-nperturb", type: "select", default: "2", name: "MC: nperturb",
        options: [ { value: "0", label: "0" }, { value: "2", label: "2" }, { value: "4", label: "4" } ],
        desc: "Velocity-perturbed warp candidates (0/2/4) over {warp,A-only,B-only}. Only with multicand." },
      { flag: "--mc-perturb", type: "number", default: "0.15", min: 0, max: 0.5, step: 0.05, name: "MC: perturb",
        desc: "Velocity perturbation fraction [0,0.5]." },
      { flag: "--mc-disp", type: "number", default: "0.35", min: 0, max: 2, step: 0.05, name: "MC: dispersion",
        desc: "Set dispersion threshold (no consensus -> crossfade) [0,2]." },
      { flag: "--mc-edge", type: "number", default: "0.5", min: 0, max: 1, step: 0.05, name: "MC: edge",
        desc: "Sobel edge threshold to keep the hard-pick [0,1] (0=always hard)." },
    ],
  },
  {
    title: "iGPU field",
    note: "The iGPU contour field is DEFAULT ON. Turning it off (--no-igpu-field) cascades off bg-snap/band-xfade/afill (which read it).",
    controls: [
      { flag: "--no-igpu-field", type: "switch-off", default: true, name: "iGPU contour field",
        desc: "Contour Sobel on the iGPU (binding 11). DEFAULT ON. Off emits --no-igpu-field and (cascade) turns off bg-snap/band-xfade/afill, which read it." },
      { flag: "--igpu-field-verify", type: "switch", default: false, name: "iGPU field verify",
        desc: "CPU Sobel oracle vs the GPU field (D-13 byte gate). Implies --igpu-field." },
      { flag: "--afill", type: "switch", default: false, name: "A-fill (visualizer)",
        desc: "A tints the contour band over wapOutA in-place (eye-validates iGPU<->boundary). Auto-enables --igpu-field." },
      { flag: "--afill-strength", type: "number", default: "0.5", min: 0, max: 1, step: 0.1, name: "A-fill: strength",
        desc: "Tint visibility weight [0,1] (0=byte-identical)." },
      { flag: "--afill-edge-norm", type: "number", default: "0.04", min: 0.001, max: 1, step: 0.01, name: "A-fill: edge-norm",
        desc: "Contour-distance->[0,1] normalizer (~1/edge_thr)." },
      { flag: "--afill-mv-gate", type: "number", default: "6", min: 0, max: 64, step: 1, name: "A-fill: still-gate (px)",
        desc: "STILL-pixel gate: the contour fades where |MV| >= this value (px), persists in still areas under motion. 0 = no gate (full contour). Only the afill visualizer." },
    ],
  },
  {
    title: "Advanced pacing (cadence / PLL / metronome)",
    controls: [
      { flag: "--no-sync-clock", type: "switch-off", default: true, name: "Sync-clock (NCO+PLL)",
        desc: "DEFAULT ON. NCO content clock + 2nd-order PLL. Off = legacy per-pair pacing." },
      { flag: "--no-sc-select", type: "switch-off", default: true, name: "SC-select",
        desc: "DEFAULT ON. The content_clock also drives pair SELECTION (full 0->1 sweep). Implies sync-clock." },
      { flag: "--no-phasefix", type: "switch-off", default: true, name: "Phasefix",
        desc: "DEFAULT ON. Phase-ladder coverage fix (recomputes the D anchor). Off = pre-STAGE-78 anchor." },
      { flag: "--sc-phase-gain", type: "number", default: "0.10", min: 0, max: 1, step: 0.01, name: "PLL phase-gain",
        desc: "PLL phase-correction fraction per arrival [0,1]." },
      { flag: "--sc-freq-alpha", type: "number", default: "0.05", min: 0, max: 1, step: 0.01, name: "PLL freq-alpha",
        desc: "PLL frequency EMA weight (T_robust) [0,1]." },
      { flag: "--sc-reseat", type: "number", default: "4.0", min: 0.5, max: 64, step: 0.5, name: "PLL re-seat",
        desc: "Phase-error threshold for re-seat vs slew (source-frames [0.5,64])." },
      { flag: "--phase-norm", type: "switch", default: true, name: "Phase-norm",
        desc: "Normalized N-ladder: intra-pair phase on a uniform grid (j+0.5)/N. DEFAULT OFF." },
      { flag: "--cphase", type: "switch", default: true, name: "C-phase (reshape)",
        desc: "Velocity-continuous reshape of the phase rate (seam-slope match; monotonic cubic). TEXT-safe. DEFAULT OFF." },
      { flag: "--cphase-ease", type: "number", default: "0.25", min: 0.05, max: 0.5, step: 0.05, name: "C-phase: ease",
        desc: "Phase width of the opening ease [0.05,0.5]. Only with cphase." },
      { flag: "--cphase-gain", type: "number", default: "1.0", min: 0, max: 1, step: 0.1, name: "C-phase: gain",
        desc: "Strength of the seam slope match [0,1] (0=linear/off). Only with cphase." },
      { flag: "--pace-variance", type: "switch", default: false, name: "Pace-variance (FSR3)",
        desc: "FSR3 variance-aware pacer (target = SMA10 - varFactor*std - safety). Pure CPU. DEFAULT OFF." },
      { flag: "--pv-safety", type: "number", default: "0.75", min: 0, step: 0.05, name: "PV: safety (ms)",
        desc: "FSR3 safetyMargin of --pace-variance (ms). Only with pace-variance." },
      { flag: "--pv-var", type: "number", default: "0.1", min: 0, step: 0.05, name: "PV: var-factor",
        desc: "FSR3 varianceFactor of --pace-variance. Only with pace-variance." },
      { flag: "--pace-present", type: "switch", default: false, name: "Pace-present (metronome)",
        desc: "Drift-corrected metric pacer: keeps the grid and slews the anchor (MASD->0). DEFAULT OFF." },
      { flag: "--pp-safety", type: "number", default: "0.50", min: 0, max: 8, step: 0.05, name: "PP: safety (ms)",
        desc: "FSR3 safetyMargin of the drift target [0,8]. Only with pace-present." },
      { flag: "--pp-var", type: "number", default: "0.1", min: 0, max: 4, step: 0.05, name: "PP: var-factor",
        desc: "FSR3 varianceFactor of the drift target [0,4]. Only with pace-present." },
      { flag: "--pp-slew", type: "number", default: "0.05", min: 0, max: 0.5, step: 0.01, name: "PP: slew-gain",
        desc: "Per-tick fraction of the grid phase error folded into tick_t0 [0,0.5]. Only with pace-present." },
      { flag: "--pace-hard", type: "switch", default: false, name: "Pace-hard",
        desc: "Hard pacer: pins every frame to the QPC edge (bounded sleep-then-spin). own-window only; freeze-floor < 1 vblank. DEFAULT OFF." },
      { flag: "--ph-spin", type: "number", default: "2.0", min: 0.1, max: 4, step: 0.1, name: "PH: spin (ms)",
        desc: "Final-spin budget of --pace-hard (ms [0.1,4])." },
      { flag: "--pace-vblank", type: "switch", default: false, name: "Pace-vblank",
        desc: "Phase-lock to the real vblank (DwmGetCompositionTimingInfo). own-window + requires pace-hard. DEFAULT OFF." },
      { flag: "--pv-lock-gain", type: "number", default: "0.05", min: 0, max: 0.5, step: 0.01, name: "PV: lock-gain",
        desc: "Per-query fraction of the vblank phase error folded into tick_t0 [0,0.5]. Only with pace-vblank." },
    ],
  },
  {
    title: "Latency / Responsiveness",
    controls: [
      { flag: "--low-d", type: "switch", default: true, name: "Low-D (floor-trim)",
        desc: "Trims the D anchor's span term (less input-lag); freshage_ema remains as the freeze floor. Requires phasefix. DEFAULT OFF." },
      { flag: "--low-d-frac", type: "number", default: "0.5", min: 0, max: 1, step: 0.1, name: "Low-D: frac",
        desc: "Fraction of the span term kept [0,1] (1=no trim). Only with low-d." },
      { flag: "--low-d-span-cap", type: "number", default: "1.5", min: 1, max: 8, step: 0.5, name: "Low-D: span-cap",
        desc: "Growth ceiling of the span term (source-frames [1,8]). Only with low-d." },
      { flag: "--rfp", type: "switch", default: false, name: "Real-fast-path",
        desc: "On ticks that already land on the freshest real, presents the real (no interp pair). Implies async-present. DEFAULT OFF." },
      { flag: "--rfp-window", type: "number", default: "0.15", min: 0, max: 1, step: 0.05, name: "RFP: window",
        desc: "rfp phase tolerance (fires when phase >= 1-window) [0,1]. Only with rfp." },
      { flag: "--rfp-fresh", type: "switch", default: false, name: "RFP-fresh (redesign)",
        desc: "Presents the freshest CAPTURED real (not the pair's). Trade: content sawtooth. Requires --rfp." },
      { flag: "--asw", type: "switch", default: false, name: "ASW (extrapolation)",
        desc: "Bounded fwd extrapolation (projects cur forward; fills deficit). Requires sync-clock. DEFAULT OFF." },
      { flag: "--asw-max", type: "number", default: "1.0", min: 0, max: 4, step: 0.5, name: "ASW: max",
        desc: "Extrapolation bound in phase units [0,4]." },
      { flag: "--camera-twarp", type: "switch", default: false, name: "Camera-twarp",
        desc: "Camera lead in the FG sampling (the view LEADS the capture; syncs the mouse). DEFAULT OFF." },
      { flag: "--camera-twarp-amt", type: "number", default: "0.5", min: 0, max: 3, step: 0.1, name: "Camera-twarp: amt",
        desc: "Eye-tunable lead scalar [0,3]." },
      { flag: "--motion-fallback", type: "switch", default: false, name: "Motion-fallback (AFMF2)",
        desc: "Frame-level fallback: on high gme dispersion presents the real (not garbage warp). Implies async. DEFAULT OFF." },
      { flag: "--mf-disp", type: "number", default: "50", min: 0, max: 100, step: 1, name: "MF: dispersion (%)",
        desc: "gme dispersion bound for the fallback (% [0,100]; above it presents a real)." },
    ],
  },
  {
    title: "Make-space / Throughput",
    controls: [
      { flag: "--no-load-governor", type: "switch-off", default: true, name: "Load governor",
        desc: "DEFAULT ON (PART-A). 4090-util tier-5 floor for the multiplier collapse in combat. Keeps flow+warp; sheds optional work." },
      { flag: "--gov-util", type: "number", default: "92", min: 50, max: 100, step: 1, name: "Gov-util (%)",
        desc: "4090-util threshold that triggers tier-5 [50,100]. Only with load-governor." },
      { flag: "--no-deficit-tier", type: "switch-off", default: true, name: "Deficit-tier",
        desc: "DEFAULT ON. Sheds object_repair/memory in heavy scenes under sustained deficit. Off = no tier-4." },
      { flag: "--no-tiers", type: "switch-off", default: true, name: "Pressure tiers",
        desc: "DEFAULT ON. STAGE-84 pressure tiers (bwd-skip + graduated shed). Off = bwd-skip only." },
      { flag: "--fdrop", type: "switch", default: false, name: "F-drop (exact dups)",
        desc: "Drops EXACT duplicate frames at present (elides redundant warp). Implies async. DEFAULT OFF." },
      { flag: "--fdrop-quiet-ms", type: "number", default: "0", min: 0, step: 0.5, name: "F-drop quiet (DEFERRED)",
        desc: "Stage-B soft near-dup: PARSED but LOGIC DEFERRED (inert at 0)." },
      { flag: "--fdrop-k", type: "number", default: "0", min: 0, step: 0.1, name: "F-drop k (DEFERRED)",
        desc: "Stage-B motion sensitivity: PARSED but LOGIC DEFERRED (inert at 0)." },
      { flag: "--upload-xfer", type: "switch", default: false, name: "Upload-xfer (DMA)",
        desc: "Moves the warp upload to the 4090's transfer/DMA queue (overlap). Implies async. Force-off without a transfer family. DEFAULT OFF." },
      { flag: "--fwd-prestage", type: "switch", default: true, name: "Fwd-prestage",
        desc: "Prestages the B copy outside the flow submit (collapses the build gap). Serial path + iGPU-convert. DEFAULT OFF." },
      { flag: "--fwd-pipeline", type: "switch", default: false, name: "Fwd-pipeline",
        desc: "Cross-pair fwd pipelining (WAP; +1 pair of publish lag). Opt-in A/B. DEFAULT OFF." },
      { flag: "--fg-prebake", type: "switch", default: false, name: "FG-prebake (descriptors)",
        desc: "Pre-bakes descriptor sets at init (avoids the per-pair vkUpdateDescriptorSets burst). Host-CPU relief. DEFAULT OFF." },
      { flag: "--shallow-queue", type: "switch", default: false, name: "Shallow-queue",
        desc: "Collapses the async depth ~1->~0 on ticks with headroom (bounded re-poll). Implies async. DEFAULT OFF." },
      { flag: "--shallow-queue-budget-us", type: "number", default: "350", min: 0, max: 4000, step: 50, name: "Shallow-queue budget (us)",
        desc: "Cap of the early-promote poll (us [0,4000]; 0=inert). Only with shallow-queue." },
    ],
  },
  {
    title: "Thread protection / Stability",
    note: "Turning a switch off emits its --no-X. --no-fg-protect doesn't touch pin/async (they use their own switches).",
    controls: [
      { flag: "--no-pin", type: "switch-off", default: true, name: "Thread pin",
        desc: "DEFAULT ON (PART-A). Pins C/F/P to cores (P elevated RT). Off = bare OS threads (byte-identical). Does NOT fix the GPU-bound slip." },
      { flag: "--pin-test", type: "select", default: "0", name: "Pin-test (ablation)",
        options: [
          { value: "0", label: "0 FULL (pin C/F/P + elevate P+F)" },
          { value: "1", label: "1 NO-FLOW-RT" },
          { value: "2", label: "2 PRIO-ONLY" },
          { value: "3", label: "3 AFFINITY-ONLY" },
          { value: "4", label: "4 NEITHER" },
          { value: "5", label: "5 MMCSS-COMPOSITE" },
        ],
        desc: "Ablation selector for the pin policy (only with --pin). fg-protect forces mode-5 by default." },
      { flag: "--no-fg-protect", type: "switch-off", default: true, name: "FG protect (S3)",
        desc: "DEFAULT ON (PART-A). S3 package against GPU99%+CPU100%: MMCSS-composite mode-5 + async-present + GAME_FLOOR (never touches the game's affinity). (async-present lives in Presentation.)" },
    ],
  },
  {
    title: "Overlay / Diagnostics",
    controls: [
      { flag: "--fps-overlay", type: "switch", default: false, name: "FPS overlay",
        desc: "LSFG-style 'in->out' overlay drawn top-left on the presented frame (sync path; the async path skips it)." },
      { flag: "--csv", type: "text", default: "", name: "Telemetry CSV", placeholder: "output.csv",
        desc: "Exports per-present telemetry to this CSV." },
      { flag: "--latency-trace", type: "switch", default: false, name: "Latency trace",
        desc: "Measurement only: emits a [lat-trace] line with the pipeline's latency breakdown." },
      { flag: "--wsub", type: "switch", default: false, name: "W-sub (warp timings)",
        desc: "Per-tick sub-timings of the warp-lambda (up/rec/gpu/prs ms) in the stats line." },
      { flag: "--fsub", type: "switch", default: false, name: "F-sub (flow split)",
        desc: "Per-pair F split fsub(flow/pair/cpu) ms in the stats line (STAGE-85 premise)." },
      { flag: "--dump", type: "number", default: "0", min: 0, step: 1, name: "Dump frames",
        desc: "Dumps the next N presented frames to frames\\ as BMP (diagnostic)." },
      { flag: "--objdump", type: "number", default: "0", min: 0, step: 1, name: "Obj-dump (grids)",
        desc: "Dumps N pairs of BLOCK GRIDS to frames\\ (dissent mask, |MV|, persist)." },
      { flag: "--pairdump", type: "number", default: "0", min: 0, step: 1, name: "Pair-dump (inputs)",
        desc: "Dumps the 2 full-res frames the warp receives per pair (the INPUT plane)." },
      { flag: "--outdump", type: "number", default: "0", min: 0, step: 1, name: "Out-dump (warp out)",
        desc: "Dumps N presented WARP outputs (wapOutA after the fence, phase t in the name)." },
      { flag: "--phaselog", type: "number", default: "0", min: 0, step: 1, name: "Phase-log",
        desc: "Logs the presented t_use ladder for the next N pairs (STAGE-78)." },
      { flag: "--duration", type: "number", default: "0", min: 0, step: 1, name: "Duration (s)", placeholder: "0 = unlimited",
        desc: "Wall-clock-bounded run: after ~N s, clean teardown (= --exit-after). 0 = unlimited." },
      { flag: "--max-frames", type: "number", default: "0", min: 0, step: 1, name: "Max frames", placeholder: "0 = unlimited",
        desc: "Present-bounded run (total_frames). 0 = unlimited. For the testbench." },
    ],
  },
];

// ── Render ───────────────────────────────────────────────────────────────────
const controlsEl = document.getElementById("controls");
const els = []; // {ctrl, input}

function makeSwitch(checked) {
  const label = document.createElement("label");
  label.className = "switch";
  const input = document.createElement("input");
  input.type = "checkbox";
  input.checked = checked;
  const slider = document.createElement("span");
  slider.className = "slider";
  label.appendChild(input);
  label.appendChild(slider);
  return { label, input };
}

function renderGroup(group) {
  const g = document.createElement("div");
  g.className = "group";
  const h = document.createElement("h2");
  h.textContent = group.title;
  g.appendChild(h);

  if (group.note) {
    const n = document.createElement("p");
    n.className = "hint";
    n.style.marginTop = "-6px";
    n.style.marginBottom = "8px";
    n.textContent = group.note;
    g.appendChild(n);
  }

  for (const ctrl of group.controls) {
    const row = document.createElement("div");
    row.className = "row";
    row.title = ctrl.desc || "";

    const lab = document.createElement("div");
    lab.className = "row-label";
    const name = document.createElement("span");
    name.className = "name";
    name.textContent = ctrl.name;
    const fn = document.createElement("span");
    fn.className = "flagname";
    fn.textContent =
      ctrl.type === "switch-off" ? ctrl.flag.replace(/^--no-/, "--") : ctrl.flag;
    lab.appendChild(name);
    lab.appendChild(fn);

    const ctl = document.createElement("div");
    ctl.className = "row-control";

    let input;
    if (ctrl.type === "switch" || ctrl.type === "switch-on") {
      const sw = makeSwitch(!!ctrl.default);
      input = sw.input;
      ctl.appendChild(sw.label);
    } else if (ctrl.type === "switch-off") {
      const sw = makeSwitch(true); // default ON
      input = sw.input;
      ctl.appendChild(sw.label);
    } else if (ctrl.type === "select") {
      input = document.createElement("select");
      for (const o of ctrl.options) {
        const opt = document.createElement("option");
        opt.value = o.value;
        opt.textContent = o.label;
        input.appendChild(opt);
      }
      input.value = ctrl.default;
      ctl.appendChild(input);
    } else if (ctrl.type === "number") {
      input = document.createElement("input");
      input.type = "number";
      if (ctrl.min !== undefined) input.min = ctrl.min;
      if (ctrl.step !== undefined) input.step = ctrl.step;
      input.value = ctrl.default;
      input.placeholder = ctrl.placeholder || "";
      ctl.appendChild(input);
    } else {
      input = document.createElement("input");
      input.type = "text";
      input.value = ctrl.default || "";
      input.placeholder = ctrl.placeholder || "";
      ctl.appendChild(input);
    }

    input.addEventListener("input", updatePreview);
    input.addEventListener("change", updatePreview);

    row.appendChild(lab);
    row.appendChild(ctl);
    g.appendChild(row);
    els.push({ ctrl, input });
  }

  if (group.windowSelector) {
    renderWindowSelector(g);
  }

  if (group.extra === "monitors") {
    const btn = document.createElement("button");
    btn.className = "btn btn-ghost";
    btn.style.marginTop = "10px";
    btn.textContent = "Detect monitors";
    btn.addEventListener("click", detectMonitors);
    g.appendChild(btn);
  }

  controlsEl.appendChild(g);
}

// ── Target-window selector ─────────────────────────────────────────────────────
// A dropdown of visible top-level windows (from the `list_windows` backend command)
// that fills the free-text `--window` flag with the selected window's TITLE. The text
// field stays editable as a manual override; this just populates it.
let lastWindows = []; // [{title, exe, pid}]

function windowInput() {
  const e = els.find((x) => x.ctrl.flag === "--window");
  return e ? e.input : null;
}

function renderWindowSelector(g) {
  const wrap = document.createElement("div");
  wrap.className = "winsel";

  const head = document.createElement("div");
  head.className = "winsel-head";
  const lbl = document.createElement("span");
  lbl.className = "field-label";
  lbl.style.margin = "0";
  lbl.textContent = "Target window (capture)";
  const refresh = document.createElement("button");
  refresh.className = "btn btn-ghost";
  refresh.textContent = "Refresh";
  refresh.addEventListener("click", refreshWindows);
  head.appendChild(lbl);
  head.appendChild(refresh);

  const sel = document.createElement("select");
  sel.id = "window-select";
  sel.addEventListener("change", () => {
    const opt = sel.selectedOptions[0];
    if (!opt || opt.value === "") return; // placeholder: keep manual value
    const title = opt.value;
    const exe = opt.dataset.exe || "";
    const wi = windowInput();
    if (wi) wi.value = title;
    updateTarget(title, exe);
    updatePreview();
  });

  const target = document.createElement("div");
  target.className = "winsel-target";
  target.id = "window-target";
  target.textContent = "Target: (none)";

  wrap.appendChild(head);
  wrap.appendChild(sel);
  wrap.appendChild(target);
  g.appendChild(wrap);

  resetWindowSelect("Click \"Refresh\" to list windows");

  // Keep the prominent "Objetivo:" line in sync with manual edits of the text field.
  const wi = windowInput();
  if (wi) {
    wi.addEventListener("input", () => {
      const v = wi.value.trim();
      const match = lastWindows.find((w) => w.title === v);
      updateTarget(v, match ? match.exe : "");
    });
  }
}

function resetWindowSelect(placeholderLabel) {
  const sel = document.getElementById("window-select");
  if (!sel) return;
  sel.innerHTML = "";
  const ph = document.createElement("option");
  ph.value = "";
  ph.textContent = placeholderLabel;
  sel.appendChild(ph);
}

function updateTarget(title, exe) {
  const t = document.getElementById("window-target");
  if (!t) return;
  if (!title) {
    t.textContent = "Target: (none)";
  } else if (exe) {
    t.textContent = `Target: ${title} (${exe})`;
  } else {
    t.textContent = `Target: ${title}`;
  }
}

async function refreshWindows() {
  if (!invoke) return;
  const sel = document.getElementById("window-select");
  resetWindowSelect("loading...");
  try {
    const wins = await invoke("list_windows");
    lastWindows = Array.isArray(wins) ? wins : [];
    resetWindowSelect(
      lastWindows.length
        ? `- select window (${lastWindows.length}) -`
        : "- no titled windows -"
    );
    for (const w of lastWindows) {
      const opt = document.createElement("option");
      opt.value = w.title;
      opt.dataset.exe = w.exe || "";
      opt.textContent = w.exe ? `${w.title} — ${w.exe}` : w.title;
      sel.appendChild(opt);
    }
    // Reflect the current --window field selection if it matches an enumerated window.
    const wi = windowInput();
    const cur = wi ? wi.value.trim() : "";
    if (cur) {
      const match = lastWindows.find((w) => w.title === cur);
      if (match) sel.value = cur;
      updateTarget(cur, match ? match.exe : "");
    }
  } catch (e) {
    resetWindowSelect("error listing windows");
    logLine("[ui] error list_windows: " + e, "exit");
  }
}

GROUPS.forEach(renderGroup);

// ── Args assembly ────────────────────────────────────────────────────────────
function buildArgs() {
  const args = [];
  for (const { ctrl, input } of els) {
    switch (ctrl.type) {
      case "switch":
      case "switch-on":
        if (input.checked) args.push(ctrl.flag);
        break;
      case "switch-off":
        if (!input.checked) args.push(ctrl.flag); // emit --no-X only when OFF
        break;
      case "select":
        if (input.value !== ctrl.default) args.push(ctrl.flag, input.value);
        break;
      case "number": {
        const v = input.value.trim();
        if (v !== "" && v !== String(ctrl.default)) args.push(ctrl.flag, v);
        break;
      }
      case "text": {
        const v = input.value.trim();
        if (v !== "") args.push(ctrl.flag, v);
        break;
      }
    }
  }
  // raw flags box (advanced) — appended last
  const raw = document.getElementById("raw-flags").value;
  for (const tok of tokenize(raw)) args.push(tok);
  return args;
}

// Minimal tokenizer honouring double quotes (args go straight to argv, not a shell).
function tokenize(str) {
  const out = [];
  let cur = "";
  let q = false;
  let has = false;
  for (let i = 0; i < str.length; i++) {
    const ch = str[i];
    if (ch === '"') {
      q = !q;
      has = true;
      continue;
    }
    if (!q && /\s/.test(ch)) {
      if (has) {
        out.push(cur);
        cur = "";
        has = false;
      }
      continue;
    }
    cur += ch;
    has = true;
  }
  if (has) out.push(cur);
  return out;
}

function exeBasename(p) {
  const m = String(p).split(/[\\/]/);
  return m[m.length - 1] || p;
}

function quoteArg(a) {
  return /\s/.test(a) ? `"${a}"` : a;
}

const cmdPreview = document.getElementById("cmd-preview");
const exeInput = document.getElementById("exe-path");

function updatePreview() {
  const args = buildArgs();
  const exe = exeBasename(exeInput.value || DEFAULT_EXE);
  cmdPreview.textContent = [exe, ...args.map(quoteArg)].join(" ");
  // Ruta común de TODO cambio de flag: si el auto-reinicio está activo y el FG está vivo,
  // programa un reinicio debounced con la nueva config. (No-op si está off o no hay proceso.)
  maybeScheduleRestart();
}

document.getElementById("raw-flags").addEventListener("input", updatePreview);
exeInput.addEventListener("input", updatePreview);
updatePreview();

// ── Live status parsing ──────────────────────────────────────────────────────
const fpsCapEl = document.getElementById("fps-cap");
const fpsInEl = document.getElementById("fps-in");
const fpsOutEl = document.getElementById("fps-out");

function parseStatus(line) {
  let m;
  if ((m = line.match(/([\d.]+)\s*fps\s*\(present\)/))) fpsOutEl.textContent = Math.round(+m[1]);
  // [ra] ... | cap N/s ...   (real frames ingested = entrada)
  if ((m = line.match(/\bcap\s+([\d.]+)\s*\/s/))) fpsInEl.textContent = Math.round(+m[1]);
  // [ra-cap] in=N/s arrived=M/s   and   [mfg] ... in=.. out=..
  if ((m = line.match(/\bin=([\d.]+)/))) fpsInEl.textContent = Math.round(+m[1]);
  if ((m = line.match(/\bout=([\d.]+)/))) fpsOutEl.textContent = Math.round(+m[1]);
  // captura = frames ÚNICOS reales/s (dd_uniq; descarta los duplicados de contenido que el DWM
  // re-compone a la tasa de refresco). Coincide con la tasa real del juego (su overlay), a diferencia
  // de acq= (dd_acq, que cuenta TODAS las adquisiciones DDA incluidos los duplicados). Con --dedup ON,
  // además se descartan del pipeline para que el FG interpole entre únicos verdaderos.
  if ((m = line.match(/\buniq=([\d.]+)/))) fpsCapEl.textContent = Math.round(+m[1]);
}

// ── Log ──────────────────────────────────────────────────────────────────────
const logEl = document.getElementById("log");
const MAX_LINES = 400;

function logLine(text, cls) {
  const atBottom = logEl.scrollTop + logEl.clientHeight >= logEl.scrollHeight - 24;
  const div = document.createElement("div");
  div.className = "line" + (cls ? " " + cls : "");
  div.textContent = text;
  logEl.appendChild(div);
  while (logEl.childElementCount > MAX_LINES) logEl.removeChild(logEl.firstChild);
  if (atBottom) logEl.scrollTop = logEl.scrollHeight;
}

function classify(line) {
  if (line.startsWith("[ra-cap]")) return "cap";
  if (line.startsWith("[ra]")) return "ra";
  return "";
}

document.getElementById("btn-clear").addEventListener("click", () => {
  logEl.innerHTML = "";
});

// ── State / buttons ──────────────────────────────────────────────────────────
const btnStart = document.getElementById("btn-start");
const btnStop = document.getElementById("btn-stop");
const statePill = document.getElementById("state-pill");

function setRunning(on) {
  running = on;
  btnStart.disabled = on;
  btnStop.disabled = !on;
  statePill.textContent = on ? "running" : "stopped";
  statePill.className = "state-pill " + (on ? "running" : "stopped");
  if (!on) {
    fpsCapEl.textContent = "--";
    fpsInEl.textContent = "--";
    fpsOutEl.textContent = "--";
  }
}

// Checkbox de auto-reinicio (en la run-card). NO entra en el modelo GROUPS ni llama a
// updatePreview, así que cambiarlo nunca dispara por sí mismo un reinicio (queda excluido).
const autoRestartEl = document.getElementById("auto-restart");
if (autoRestartEl) {
  autoRestartEl.addEventListener("change", () => {
    autoRestart = autoRestartEl.checked;
  });
}

// Programa un reinicio DEBOUNCED si el auto-reinicio está activo y hay un FG vivo. Se llama
// desde updatePreview() (la ruta común de TODO cambio de flag: controles GROUPS + caja de
// flags crudos + ruta del ejecutable). Si no hay proceso o el auto-reinicio está off, no hace
// nada extra (el cambio solo actualiza la preview, como siempre).
function maybeScheduleRestart() {
  if (!autoRestart || !running) return;
  clearTimeout(restartTimer);
  restartTimer = setTimeout(doRestart, 1000); // 1 s tras el ÚLTIMO cambio
}

async function doRestart() {
  if (!invoke || !running) return;
  const args = buildArgs();
  const exePath = exeInput.value || DEFAULT_EXE;
  logLine("[ui] restarting FG with new config...", "sys");
  // Belt-and-suspenders del guard de epoch del backend: marcar el reinicio en vuelo para que
  // el "fg-exit" del hijo viejo (si el backend llegara a emitir uno) no nos pase a "detenido".
  restarting = true;
  try {
    await invoke("restart", { args, exePath });
  } catch (e) {
    logLine("[ui] restart error: " + e, "exit");
    restarting = false;
    setRunning(false); // el reinicio falló (p.ej. spawn): el FG ya no está corriendo
    return;
  }
  // Éxito: el hijo nuevo corre; seguimos "en ejecución". Limpiar el guard tras un margen breve
  // para descartar un posible fg-exit tardío del hijo viejo (ventana de carrera del backend).
  setTimeout(() => {
    restarting = false;
  }, 600);
}

async function detectMonitors() {
  if (!invoke) return;
  logLine("[ui] querying --list-monitors ...", "sys");
  try {
    const out = await invoke("list_monitors", { exePath: exeInput.value || DEFAULT_EXE });
    for (const l of String(out).split(/\r?\n/)) if (l.trim() !== "") logLine(l, classify(l));
  } catch (e) {
    logLine("[ui] error: " + e, "exit");
  }
}

btnStart.addEventListener("click", async () => {
  if (!invoke) {
    logLine("[ui] Tauri API not available.", "exit");
    return;
  }
  const args = buildArgs();
  const exePath = exeInput.value || DEFAULT_EXE;
  setRunning(true);
  logLine("[ui] starting: " + exeBasename(exePath) + " " + args.map(quoteArg).join(" "), "sys");
  try {
    await invoke("launch", { args, exePath });
  } catch (e) {
    logLine("[ui] failed to start: " + e, "exit");
    setRunning(false);
  }
});

btnStop.addEventListener("click", async () => {
  if (!invoke) return;
  try {
    await invoke("stop");
  } catch (e) {
    logLine("[ui] stop error: " + e, "exit");
  }
});

// ── Wire events ──────────────────────────────────────────────────────────────
if (listen) {
  listen("fg-log", (e) => {
    const line = String(e.payload ?? "");
    parseStatus(line);
    logLine(line, classify(line));
  });
  listen("fg-exit", (e) => {
    // Belt-and-suspenders del guard de epoch del backend: si hay un reinicio en vuelo, este
    // "fg-exit" es del hijo VIEJO (o un cierre transitorio del swap); ignorarlo para que la UI
    // siga "en ejecución" sin parpadear a "detenido".
    if (restarting) return;
    const code = e.payload;
    logLine("[ui] process finished (code " + (code ?? "?") + ")", "exit");
    setRunning(false);
  });
}

// initial sync (in case a process is already running from a prior window session)
window.addEventListener("DOMContentLoaded", async () => {
  if (!invoke) return;
  // Populate the target-window selector once at startup (non-fatal if it fails).
  refreshWindows();
  try {
    const isUp = await invoke("is_running");
    setRunning(!!isUp);
  } catch {
    setRunning(false);
  }
});

// Made with my soul - Swately <3
