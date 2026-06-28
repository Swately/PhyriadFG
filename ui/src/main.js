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
    title: "Captura",
    controls: [
      {
        flag: "--capture-api", type: "select", default: "dd",
        name: "API de captura",
        options: [
          { value: "dd", label: "dd (Desktop Duplication)" },
          { value: "wgc", label: "wgc (Windows Graphics Capture)" },
        ],
        desc: "Backend de captura: dd (por defecto) o wgc (HW-accel flip/overlay).",
      },
      {
        flag: "--ingest-async", type: "switch", default: false,
        name: "Ingesta asíncrona (DDA)",
        desc: "Desacopla la ingesta DDA: hilo que solo adquiere a un ring + worker de convert en paralelo (drop-to-newest) que publica al instante. Sube la ingesta hacia la tasa entregada (mira captura vs ingesta). Solo DDA; default OFF.",
      },
      {
        flag: "--dedup", type: "switch", default: true,
        name: "Descartar duplicados (DDA)",
        desc: "DDA compone el escritorio a la tasa de refresco del monitor, así que muchas capturas son DUPLICADOS de contenido (el DWM re-compone el mismo frame del juego, que renderiza menos frames únicos). Descarta los duplicados del pipeline para que el FG interpole entre únicos VERDADEROS en vez de pares de movimiento-cero. La lectura de captura muestra siempre la tasa única real. Solo DDA; default OFF.",
      },
      {
        flag: "--window", type: "text", default: "",
        name: "Ventana (subcadena)", placeholder: "p.ej. Battlefield",
        desc: "WGC: captura la ventana cuyo título contiene esta subcadena (implica --capture-api wgc).",
      },
      {
        flag: "--monitor", type: "number", default: "0", min: 0, step: 1,
        name: "Monitor de captura", placeholder: "0",
        desc: "Captura desde la salida DXGI M (por defecto 0).",
      },
      {
        flag: "--present-monitor", type: "number", default: "", min: 0, step: 1,
        name: "Monitor de presentación", placeholder: "= captura",
        desc: "Presenta en la salida P (por defecto: la misma que la de captura).",
      },
      {
        flag: "--cap-slots", type: "number", default: "0", min: 0, max: 32, step: 1,
        name: "Slots del ring de captura", placeholder: "0 = auto",
        desc: "Override de la profundidad auto del ring de captura (0=auto ~src/10+4, máx 32). Son slots de frame en RAM.",
      },
      {
        flag: "--ingest-backlog", type: "number", default: "3", min: 1, max: 3, step: 1,
        name: "Backlog de ingesta",
        desc: "Profundidad de drenaje en-orden de la ingesta (1=más fresco/más saltos span-2, 3=más suave/más latencia). El lever DOMINANTE de freshage/input-lag.",
      },
      {
        flag: "--copy-fence", type: "switch", default: false, name: "Copy-fence (WGC)",
        desc: "Pickup de copia WGC dirigido por evento (fence+event en vez de busy-poll Map). Necesita D3D11.4; force-off si no.",
      },
      {
        flag: "--copy-device", type: "switch", default: false, name: "Copy en 2º device (WGC)",
        desc: "Corre el CopyResource de staging WGC en un D3D11 device separado del mismo adaptador (desacopla la cola de copia). WGC-only; force-off si falla.",
      },
      {
        flag: "--dpi-probe", type: "switch", default: false, name: "DPI probe (diag)",
        desc: "Diagnóstico de solo-lectura: loguea GetClientRect/GetDpiForWindow/Size para el crop de captura en alto-DPI.",
      },
      {
        flag: "--cap-route-probe", type: "switch", default: false, name: "Cap-route probe",
        desc: "Imprime una decisión de routing por capacidad (has_fp16/dp4a) sobre los VDev del FG. MEDICIÓN-SÓLO, inerte (roles A/B/G fijos).",
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
        name: "GPU de frame-gen",
        options: [
          { value: "auto", label: "auto (según carga)" },
          { value: "primary", label: "primary" },
          { value: "assist", label: "assist" },
        ],
        desc: "Dispositivo del frame-gen: auto (por defecto), primary o assist.",
      },
      {
        flag: "--convert-gpu", type: "select", default: "igpu",
        name: "GPU de conversión",
        options: [
          { value: "igpu", label: "igpu (convert fusionado sin PCIe)" },
          { value: "primary", label: "primary" },
        ],
        desc: "Dónde corre el convert+pack: igpu (por defecto, zero-PCIe) o primary.",
      },
      {
        flag: "--present-gpu", type: "text", default: "",
        name: "GPU de presentación", placeholder: "(heredado)",
        desc: "HEREDADO / IGNORADO: la presentación es el adaptador dueño del panel desde STAGE-45b.",
      },
      {
        flag: "--force-single-gpu", type: "switch", default: false,
        name: "Forzar GPU única",
        desc: "Conduce la ruta de una sola GPU (suprime la 2.ª discreta + iGPU; todos los roles en el dispositivo A).",
      },
      {
        flag: "--assist-gpu", type: "text", default: "",
        name: "GPU asistente (nombre)", placeholder: "fragmento del nombre",
        desc: "Fragmento del nombre de la GPU para el frame-gen (selección del dispositivo asistente).",
      },
    ],
  },
  {
    title: "Frame-gen",
    controls: [
      {
        flag: "--fg-factor", type: "select", default: "2",
        name: "Factor FG",
        options: [
          { value: "2", label: "2x (por defecto)" },
          { value: "3", label: "3x" },
          { value: "4", label: "4x" },
          { value: "5", label: "5x" },
          { value: "6", label: "6x" },
          { value: "8", label: "8x" },
          { value: "auto", label: "auto (medido)" },
        ],
        desc: "Multiplicador de salida: 2=2x (por defecto), 3=3x, ... auto=medido.",
      },
      {
        flag: "--flow-scale", type: "select", default: "1",
        name: "Escala de flujo",
        options: [
          { value: "1", label: "1 (byte-identical)" },
          { value: "2", label: "2 (1/2 res)" },
          { value: "4", label: "4 (1/4 res)" },
          { value: "auto", label: "auto" },
        ],
        desc: "DRS coin-3: corre el flujo + la cola CPU por par sobre una rejilla MV a 1/N de resolución (~N^2 más barato; solo WAP).",
      },
      {
        flag: "--warp-scale", type: "select", default: "1",
        name: "Escala de warp",
        options: [
          { value: "1", label: "1 (byte-identical)" },
          { value: "2", label: "2" },
          { value: "4", label: "4" },
        ],
        desc: "Corre NUESTRO warp a WW/N y reescala (PARKED-NEGATIVE: borroso por el ojo del operador en BF6 4K; no toca la latencia).",
      },
      {
        flag: "--nvofa", type: "switch", default: false,
        name: "NVIDIA OFA (HW)",
        desc: "Reemplaza el flujo clásico por el Optical Flow Accelerator HW de NVIDIA (4090). Auto-fallback si no hay VK_NV_optical_flow.",
      },
      {
        flag: "--nvofa-cost-scale", type: "number", default: "0.5", min: 0, max: 16, step: 0.1,
        name: "NVOFA cost-scale",
        desc: "Remap del coste OFA (0..255) -> sad_best. PLACEHOLDER (eye-calibración). Sólo con --nvofa.",
      },
      {
        flag: "--nvofa-sadz-scale", type: "number", default: "4.0", min: 0, max: 64, step: 0.5,
        name: "NVOFA sadz-scale",
        desc: "SUM|A-B| del bloque -> magnitud 8x8. PLACEHOLDER (eye-calibración). Sólo con --nvofa.",
      },
      {
        flag: "--flow-scale-target-mp", type: "number", default: "2.1", min: 0.25, max: 12, step: 0.1,
        name: "Flow-scale auto target (MP)",
        desc: "Target en megapíxeles de trabajo de flujo para --flow-scale auto (1080p ~div=1; 1440p/4K bajan). El slider calidad<->latencia.",
      },
    ],
  },
  {
    title: "Presentación / Pacing",
    controls: [
      {
        flag: "--hdr", type: "switch", default: false,
        name: "HDR (FP16 scRGB)",
        desc: "Presenta un swapchain FP16 scRGB para HDR (alias de --present-fp16). INERTE sin pantalla+contenido HDR.",
      },
      {
        flag: "--present-own-window", type: "switch", default: false,
        name: "Ventana propia",
        desc: "Presenta nuestra propia HWND flip borderless (plano Independent-Flip, topología LSFG). Click-through.",
      },
      {
        flag: "--no-async-present", type: "switch-off", default: false,
        name: "Presentación asíncrona",
        desc: "DEFAULT ON (PART-A). Desacopla la presentación de la fence del warp (present no bloqueante + drop-interpolated; ~1 frame de latencia). Apagar = ruta SÍNCRONA bloqueante (byte-identical). La auto-activan --target-output-fps/--fdrop/--upload-xfer/--rfp/--motion-fallback/--shallow-queue.",
      },
      {
        flag: "--cap-fps", type: "number", default: "", min: 1, step: 1,
        name: "Límite de captura (fps)", placeholder: "0 = off",
        desc: "Limita la captura WGC a N fps (MinUpdateInterval=1/N). Diagnóstico; no cambia el algoritmo.",
      },
      {
        flag: "--refresh-hz", type: "number", default: "240", min: 1, step: 1,
        name: "Refresh del reloj (Hz)", placeholder: "240",
        desc: "Tasa de tick del reloj de salida (por defecto 240).",
      },
      {
        flag: "--target-output-fps", type: "number", default: "", min: 1, step: 1,
        name: "FPS de salida objetivo", placeholder: "0 = off",
        desc: "Controlador fraccional de tasa de salida (cadencia estable). Auto-activa --async-present. Nunca capa el juego.",
      },
      {
        flag: "--s2-sustain", type: "number", default: "0.93", min: 0.5, max: 1, step: 0.01,
        name: "Sustain frac (target-fps)",
        desc: "Fracción de la tasa medida alcanzable que apunta --target-output-fps ('un 180 estable gana a un 191 errático') [0.5,1]. Sólo con target-output-fps.",
      },
      {
        flag: "--present-sync", type: "select", default: "0",
        name: "Sync interval",
        options: [
          { value: "0", label: "0 (inmediato)" },
          { value: "1", label: "1 (pace al compositor)" },
          { value: "2", label: "2" },
          { value: "3", label: "3" },
          { value: "4", label: "4" },
        ],
        desc: "Present(sync_interval): 0 = present-immediately (por defecto, sobre-presenta); 1 = pace al compositor.",
      },
      {
        flag: "--present-waitable", type: "switch", default: false, name: "Swapchain waitable",
        desc: "SetMaximumFrameLatency(1) + wait-before-present: reduce jitter DENTRO de la composición DWM (PARCIAL; no llega a Independent Flip).",
      },
      {
        flag: "--present-colorspace", type: "select", default: "off", name: "Colorspace del overlay",
        options: [
          { value: "off", label: "off (no-op)" },
          { value: "srgb", label: "srgb (declara sRGB)" },
        ],
        desc: "Declara el colorspace del overlay (sRGB) para que un escritorio HDR componga el overlay SDR sin lavado. Soft si no hay soporte.",
      },
      {
        flag: "--indicator", type: "switch", default: false, name: "Indicador (WIP)",
        desc: "Marca en pantalla de PhyriadFG-activo (WIP, passthrough-only; el stamp en la ruta warp falló DEVICE_LOST).",
      },
      {
        flag: "--no-upscale", type: "switch", default: false, name: "Sin upscale",
        desc: "Presenta a la resolución de trabajo (el blit del bridge escala). Emite --no-upscale.",
      },
      {
        flag: "--upscale-lanczos", type: "switch", default: false, name: "Upscale Lanczos-2",
        desc: "Upscale Lanczos-2 (más nítido, más lento).",
      },
    ],
  },
  {
    title: "Calidad (por defecto ON)",
    note: "Apagar un interruptor emite su --no-X. (Excepto Matte, que está OFF por defecto y emite --matte.)",
    controls: [
      { flag: "--no-warp-at-presenter", type: "switch-off", default: true, name: "Warp-at-presenter (WAP)",
        desc: "Re-warp por tick a la fase exacta. Apagar = modo rejilla (deshabilita bidir/fill-div/rescue/mv-guided/gme/matte/inertia)." },
      { flag: "--no-gme", type: "switch-off", default: true, name: "GME (modelo global)",
        desc: "Modelo de movimiento afín global por par. Apagar también deshabilita matte (cascada)." },
      { flag: "--no-bidir", type: "switch-off", default: true, name: "Flujo bidireccional",
        desc: "Flujo bidireccional + clasificación de oclusión. Apagar también deshabilita fill-div y matte (cascada)." },
      { flag: "--matte", type: "switch-on", default: false, name: "Matte fluida",
        desc: "Composición fluida de dos capas. POR DEFECTO OFF (el A/B del operador halló --no-matte mejor: doblaba la figura). --matte la reactiva (necesita gme + bidir)." },
      { flag: "--no-mv-guided", type: "switch-off", default: true, name: "MV guiada por color",
        desc: "Asignación de MV guiada por membresía de color. Apagar = mediana/lineal ciega." },
      { flag: "--no-rescue", type: "switch-off", default: true, name: "Rescate de candidatos",
        desc: "Rescate por MV de bloques vecinos. Apagar = byte-identical." },
      { flag: "--no-inertia", type: "switch-off", default: true, name: "Inercia (persistencia)",
        desc: "Prior de persistencia de movimiento (histéresis de estado). Apagar = byte-identical." },
      { flag: "--inertia-thresh", type: "number", default: "0.50", min: 0.1, max: 1, step: 0.05, name: "Inercia: umbral",
        desc: "Corte de persistencia (R8-norm [0.1,1]). 0.5 ≈ 8 pares estáticos." },
      { flag: "--no-stasis", type: "switch-off", default: true, name: "Stasis (bypass HUD)",
        desc: "Capa de stasis: un bloque sad_zero<=thresh presenta el real directamente. Apagar = byte-identical." },
      { flag: "--stasis-thresh", type: "number", default: "0.50", min: 0.05, max: 8, step: 0.05, name: "Stasis: umbral",
        desc: "Corte sad_zero (SUM|A-B| por bloque [0.05,8]). ~0 = bloque idéntico." },
    ],
  },
  {
    title: "Flujo / Vectores (MV)",
    controls: [
      { flag: "--no-mv-subpel", type: "switch-off", default: true, name: "MV sub-pixel",
        desc: "DEFAULT ON. Refinamiento sub-pixel (parábola del pico SAD) -> MV fraccional. Apagar = best_mv entero." },
      { flag: "--mv-candsel", type: "switch", default: true, name: "MV candidate-select",
        desc: "Tiles interiores ambiguos adoptan la MV de región coarse (mata el crossfade de apertura). Compone con mv-subpel." },
      { flag: "--mv-median", type: "switch", default: false, name: "MV mediana 3x3",
        desc: "Mediana vectorial 3x3 ciega sobre el campo MV antes del warp (superado por mv-guided)." },
      { flag: "--mv-smooth", type: "number", default: "0", min: 0, max: 1, step: 0.1, name: "MV smooth (EMA)",
        desc: "Alpha de EMA temporal de MV (0=off; ~0.6 amortigua el jitter de tile)." },
      { flag: "--mv-prior", type: "switch", default: false, name: "MV prior temporal",
        desc: "Prior temporal de MV en el matcher (dual-centre, auto-cura en cortes). Ignorado con bidir (default)." },
      { flag: "--mv-sim", type: "number", default: "0.10", min: 0.02, max: 0.5, step: 0.01, name: "MV-guided: banda color",
        desc: "Banda de membresía de color para mv-guided (max-ch [0.02,0.5])." },
      { flag: "--residual-ceil", type: "number", default: "32", min: 0, step: 1, name: "Gate: residual-ceil",
        desc: "Gate (a): sad_best máximo (default 32)." },
      { flag: "--conf-improv", type: "number", default: "0.20", min: 0, max: 1, step: 0.01, name: "Gate: conf-improv",
        desc: "Gate (b): mejora mínima de SAD (fracción, default 0.20)." },
      { flag: "--agreement", type: "number", default: "0.05", min: 0, max: 1, step: 0.01, name: "Gate: agreement",
        desc: "Gate de acuerdo: d máximo por tile (default 0.05)." },
      { flag: "--occl-thresh", type: "number", default: "1.5", min: 0.25, max: 8, step: 0.1, name: "Occlusion thresh",
        desc: "Umbral round-trip fwd<->bwd (px [0.25,8]). Sólo con bidir." },
      { flag: "--div-eps", type: "number", default: "0.05", min: 0.005, max: 1, step: 0.01, name: "Divergence eps",
        desc: "Banda de divergencia (px/texel [0.005,1]); usado por fill-div." },
      { flag: "--fill-div", type: "switch", default: false, name: "Fill-div",
        desc: "Pick de disoclusión dirigido por divergencia. DEFAULT OFF (auditoría WASH); requiere bidir." },
    ],
  },
  {
    title: "Warp / Mezcla",
    controls: [
      { flag: "--no-soft-gate", type: "switch-off", default: true, name: "Soft-gate",
        desc: "DEFAULT ON. Gate continuo de warp. Apagar = keep/freeze binario por bloque 8x8." },
      { flag: "--no-commit", type: "switch-off", default: true, name: "Commit del warp",
        desc: "DEFAULT ON (commit_thresh 0.08 + commit_real). Apagar = averaging + deshabilita rescue + appearance." },
      { flag: "--commit-thresh", type: "number", default: "0.08", min: 0.005, max: 0.5, step: 0.01, name: "Commit: umbral",
        desc: "Umbral de commit (color units [0.005,0.5]; 0 = averaging). --no-commit gana si ambos." },
      { flag: "--no-appearance", type: "switch-off", default: true, name: "Appearance re-blend",
        desc: "DEFAULT ON. Re-blend tonal en píxeles committeados (violador de brightness-constancy)." },
      { flag: "--appear-band", type: "number", default: "0.10", min: 0.02, max: 0.5, step: 0.01, name: "Appearance: banda",
        desc: "Banda de apariencia (max-ch [0.02,0.5])." },
      { flag: "--no-commit-default", type: "switch-off", default: true, name: "Commit-default",
        desc: "DEFAULT ON. El warp es el default; sólo basura evidenciada (d_pixel grande) hace blend." },
      { flag: "--no-onepos", type: "switch-off", default: true, name: "One-position",
        desc: "DEFAULT ON. Colapso una-posición-por-píxel (anti doble-exposición del propio warp)." },
      { flag: "--onepos-band", type: "number", default: "1.0", min: 0.05, max: 4, step: 0.05, name: "One-pos: banda",
        desc: "Escala de inicio del colapso ([0.05,4]; 1.0 = STAGE-81, menor = colapsa crescents tenues)." },
      { flag: "--no-member-commit", type: "switch-off", default: true, name: "Member-commit",
        desc: "DEFAULT ON. Membership-beats-the-blend en interiores planos de objeto (anti ghost-step)." },
      { flag: "--no-phase-anchor", type: "switch-off", default: true, name: "Phase-anchor",
        desc: "DEFAULT ON. MV primaria anclada a la fase (cur-anchored a fase alta). Necesita bidir." },
      { flag: "--no-vblend", type: "switch-off", default: true, name: "V-blend (velocity)",
        desc: "DEFAULT ON. Cerca del fin de par, la MV se inclina hacia la velocidad del próximo par (anti pulse)." },
      { flag: "--vblend-t0", type: "number", default: "0.6", min: 0, max: 0.95, step: 0.05, name: "V-blend: t0",
        desc: "Fase donde empieza la rampa de tilt [0,0.95]. Sólo con vblend." },
      { flag: "--vblend-strength", type: "number", default: "0.5", min: 0, max: 1, step: 0.05, name: "V-blend: fuerza",
        desc: "Peso máx de tilt en t=1 [0,1]. Sólo con vblend." },
      { flag: "--vblend-exact", type: "switch", default: false, name: "V-blend exacto",
        desc: "Velocity-continuity EXACTA (MV real del próximo par; +1 source-frame de latencia). Implica vblend." },
      { flag: "--no-bg-snap", type: "switch-off", default: true, name: "BG-snap",
        desc: "DEFAULT ON. En la banda de contorno iGPU snapea MVs de fondo al modelo gme (mata la gravedad de disoclusión)." },
      { flag: "--bg-snap-strength", type: "number", default: "1.0", min: 0, max: 4, step: 0.5, name: "BG-snap: fuerza",
        desc: "Escala del peso de snap [0,4] (1=soft, 2-4=snap progresivamente duro)." },
      { flag: "--bg-snap-norm", type: "number", default: "0.04", min: 0.001, max: 1, step: 0.01, name: "BG-snap: norm",
        desc: "Normalizador dist-contorno->[0,1] de la banda ([0.001,1])." },
      { flag: "--band-xfade-strength", type: "number", default: "1.0", min: 0, max: 1, step: 0.05, name: "Band-xfade (gravedad)",
        desc: "Crossfade de cancelación de gravedad en la banda (DEFAULT 1.0; 0 = off = equivale a --no-band-xfade)." },
      { flag: "--ts-smooth", type: "number", default: "0.1", min: 0, max: 1, step: 0.05, name: "TS-smooth",
        desc: "Suavizado temporal adaptativo gateado a píxeles basura (DEFAULT 0.1; 0 = off; 0.5 over-smooths)." },
      { flag: "--disoccl-hardpick", type: "number", default: "0", min: 0, max: 1, step: 0.05, name: "Disoccl hard-pick",
        desc: "Hard-pick con gate de borde Sobel en la banda bidir (0=off; requiere bidir + campo iGPU)." },
    ],
  },
  {
    title: "GME / Objetos / Matte",
    note: "Sub-stack de la matte: la mayoría son hijos de --matte/--gme/--bidir/--objects y caen por cascada si el padre está apagado.",
    controls: [
      { flag: "--no-gme-gpu", type: "switch-off", default: true, name: "GME en GPU B",
        desc: "DEFAULT ON. Offload del fit afín gme a la GPU B (1080 Ti, donde vive MV/SAD). Apagar = CPU. Auto-fallback a CPU si B no está." },
      { flag: "--gme-gpu-verify", type: "switch", default: false, name: "GME-GPU verify",
        desc: "Corre GPU+CPU gme e imprime rel-diff + dis-mask flips. Implica gme-gpu." },
      { flag: "--gme-irls2", type: "switch", default: false, name: "GME IRLS 2-pasadas",
        desc: "gme_fit con 2 pasadas IRLS en vez de 3 (~0.3-0.7ms/par). Default 3 (byte-identical)." },
      { flag: "--matte-thresh", type: "number", default: "0.25", min: 0.05, max: 1, step: 0.05, name: "Matte: umbral",
        desc: "Corte de disidencia de la matte (R8-norm [0.05,1])." },
      { flag: "--mass-k", type: "number", default: "0.5", min: 0, max: 2, step: 0.1, name: "Matte: mass-k",
        desc: "Ganancia de feedback de masa-conservación de la matte [0,2] (0=off/lerp)." },
      { flag: "--obj-fill-rim", type: "switch", default: true, name: "Obj fill-rim",
        desc: "Infill de MV coherente en TODO el interior de un objeto rígido (mata el crescent media-luna). Se rinde en rim no-rígido." },
      { flag: "--disoccl-commit", type: "switch", default: false, name: "Disoccl-commit",
        desc: "Commit de un solo lado en la banda de disoclusión (reemplaza los blend-fallback simétricos). Requiere --matte." },
      { flag: "--no-objects", type: "switch-off", default: true, name: "Object-holon",
        desc: "DEFAULT ON. Clustering + reparación por herencia de movimiento. Apagar = pre-stage exacto." },
      { flag: "--no-shapefield", type: "switch-off", default: true, name: "Shape-field",
        desc: "DEFAULT ON (hijo de objects). Apagar = herencia rígida de MV única (STAGE-61)." },
      { flag: "--no-crescent", type: "switch-off", default: true, name: "Crescent (bg)",
        desc: "DEFAULT ON. Fetch de fondo dirigido por crescent (matte). Apagar = blend (1-t,t)." },
      { flag: "--no-travel", type: "switch-off", default: true, name: "Travel (ocupación)",
        desc: "DEFAULT ON. Ocupación de silueta viajera (matte). Apagar = lerp STAGE-59." },
      { flag: "--no-contour", type: "switch-off", default: true, name: "Contour marriage",
        desc: "DEFAULT ON. Arbitraje de banda de contorno por afinidad de color (matte). Apagar = binario." },
      { flag: "--no-obj-crescent", type: "switch-off", default: true, name: "Obj-crescent",
        desc: "DEFAULT ON. Ponderación de lado del crescent para la capa OBJETO. Apagar = (1-t,t)." },
      { flag: "--no-memory", type: "switch-off", default: true, name: "Scene memory",
        desc: "DEFAULT ON (hijo de objects). Memoria de silueta advectada. Apagar = sólo el par fresco." },
      { flag: "--no-persist-reset", type: "switch-off", default: true, name: "Persist-reset",
        desc: "DEFAULT ON. Membership-beats-inertia (reset del escudo HUD en interiores de mover)." },
      { flag: "--no-change-gate", type: "switch-off", default: true, name: "Change-gate",
        desc: "DEFAULT ON. Las máscaras de disidencia exigen contenido CAMBIADO (anti-halo). Apagar = máscaras crudas." },
      { flag: "--no-expire", type: "switch-off", default: true, name: "Expire (stigmergy)",
        desc: "DEFAULT ON. Expiración de EMAs cross-par en contradicciones. Apagar = decae a través." },
      { flag: "--no-ambig", type: "switch-off", default: true, name: "Ambiguity (2º cand)",
        desc: "DEFAULT ON (hijo de gme). Arbitraje del segundo-mejor candidato en empates de SAD (anti textura periódica)." },
    ],
  },
  {
    title: "Multi-candidato",
    controls: [
      { flag: "--multicand", type: "switch", default: false, name: "Multi-candidato",
        desc: "Selección medoid multi-candidato (generate+discriminate+SELECT). DEFAULT OFF (edge-gate sin validar)." },
      { flag: "--mc-nperturb", type: "select", default: "2", name: "MC: nperturb",
        options: [ { value: "0", label: "0" }, { value: "2", label: "2" }, { value: "4", label: "4" } ],
        desc: "Candidatos warp perturbados en velocidad (0/2/4) sobre {warp,A-only,B-only}. Sólo con multicand." },
      { flag: "--mc-perturb", type: "number", default: "0.15", min: 0, max: 0.5, step: 0.05, name: "MC: perturb",
        desc: "Fracción de perturbación de velocidad [0,0.5]." },
      { flag: "--mc-disp", type: "number", default: "0.35", min: 0, max: 2, step: 0.05, name: "MC: dispersión",
        desc: "Umbral de dispersión del set (sin consenso -> crossfade) [0,2]." },
      { flag: "--mc-edge", type: "number", default: "0.5", min: 0, max: 1, step: 0.05, name: "MC: edge",
        desc: "Umbral de borde Sobel para mantener el hard-pick [0,1] (0=siempre duro)." },
    ],
  },
  {
    title: "iGPU field",
    note: "El campo de contornos iGPU es DEFAULT ON. Apagarlo (--no-igpu-field) cascada apaga bg-snap/band-xfade/afill (lo leen).",
    controls: [
      { flag: "--no-igpu-field", type: "switch-off", default: true, name: "Campo de contornos iGPU",
        desc: "Sobel de contornos en la iGPU (binding 11). DEFAULT ON. Apagar emite --no-igpu-field y (cascada) apaga bg-snap/band-xfade/afill, que lo leen." },
      { flag: "--igpu-field-verify", type: "switch", default: false, name: "iGPU field verify",
        desc: "Oráculo CPU Sobel vs el campo GPU (gate de bytes D-13). Implica --igpu-field." },
      { flag: "--afill", type: "switch", default: false, name: "A-fill (visualizador)",
        desc: "A tinta la banda de contorno sobre wapOutA in-place (eye-valida iGPU<->frontera). Auto-activa --igpu-field." },
      { flag: "--afill-strength", type: "number", default: "0.5", min: 0, max: 1, step: 0.1, name: "A-fill: fuerza",
        desc: "Peso de visibilidad del tinte [0,1] (0=byte-identical)." },
      { flag: "--afill-edge-norm", type: "number", default: "0.04", min: 0.001, max: 1, step: 0.01, name: "A-fill: edge-norm",
        desc: "Normalizador dist-contorno->[0,1] (~1/edge_thr)." },
      { flag: "--afill-mv-gate", type: "number", default: "6", min: 0, max: 64, step: 1, name: "A-fill: still-gate (px)",
        desc: "Gate de pixeles QUIETOS: el contorno se desvanece donde |MV| >= este valor (px), persiste en zonas quietas bajo movimiento. 0 = sin gate (contorno completo). Solo el visualizador afill." },
    ],
  },
  {
    title: "Pacing avanzado (cadencia / PLL / metrónomo)",
    controls: [
      { flag: "--no-sync-clock", type: "switch-off", default: true, name: "Sync-clock (NCO+PLL)",
        desc: "DEFAULT ON. Reloj de contenido NCO + PLL 2º orden. Apagar = pacing por-par legacy." },
      { flag: "--no-sc-select", type: "switch-off", default: true, name: "SC-select",
        desc: "DEFAULT ON. El content_clock dirige también la SELECCIÓN de par (barrido 0->1 completo). Implica sync-clock." },
      { flag: "--no-phasefix", type: "switch-off", default: true, name: "Phasefix",
        desc: "DEFAULT ON. Fix de cobertura de la escalera de fase (recalcula el ancla D). Apagar = ancla pre-STAGE-78." },
      { flag: "--sc-phase-gain", type: "number", default: "0.10", min: 0, max: 1, step: 0.01, name: "PLL phase-gain",
        desc: "Fracción de corrección de fase del PLL por arribo [0,1]." },
      { flag: "--sc-freq-alpha", type: "number", default: "0.05", min: 0, max: 1, step: 0.01, name: "PLL freq-alpha",
        desc: "Peso EMA de frecuencia (T_robust) del PLL [0,1]." },
      { flag: "--sc-reseat", type: "number", default: "4.0", min: 0.5, max: 64, step: 0.5, name: "PLL re-seat",
        desc: "Umbral de error de fase para re-seat vs slew (source-frames [0.5,64])." },
      { flag: "--phase-norm", type: "switch", default: true, name: "Phase-norm",
        desc: "Escalera-N normalizada: fase intra-par en rejilla uniforme (j+0.5)/N. DEFAULT OFF." },
      { flag: "--cphase", type: "switch", default: true, name: "C-phase (reshape)",
        desc: "Reshape velocity-continuous de la tasa de fase (seam-slope match; cúbico monótono). TEXT-safe. DEFAULT OFF." },
      { flag: "--cphase-ease", type: "number", default: "0.25", min: 0.05, max: 0.5, step: 0.05, name: "C-phase: ease",
        desc: "Ancho de fase del ease de apertura [0.05,0.5]. Sólo con cphase." },
      { flag: "--cphase-gain", type: "number", default: "1.0", min: 0, max: 1, step: 0.1, name: "C-phase: gain",
        desc: "Fuerza del match de pendiente del seam [0,1] (0=lineal/off). Sólo con cphase." },
      { flag: "--pace-variance", type: "switch", default: false, name: "Pace-variance (FSR3)",
        desc: "Pacer FSR3 variance-aware (target = SMA10 - varFactor·std - safety). Pure CPU. DEFAULT OFF." },
      { flag: "--pv-safety", type: "number", default: "0.75", min: 0, step: 0.05, name: "PV: safety (ms)",
        desc: "FSR3 safetyMargin de --pace-variance (ms). Sólo con pace-variance." },
      { flag: "--pv-var", type: "number", default: "0.1", min: 0, step: 0.05, name: "PV: var-factor",
        desc: "FSR3 varianceFactor de --pace-variance. Sólo con pace-variance." },
      { flag: "--pace-present", type: "switch", default: false, name: "Pace-present (metrónomo)",
        desc: "Pacer métrico drift-corrected: mantiene el grid y slewea el ancla (MASD->0). DEFAULT OFF." },
      { flag: "--pp-safety", type: "number", default: "0.50", min: 0, max: 8, step: 0.05, name: "PP: safety (ms)",
        desc: "FSR3 safetyMargin del drift target [0,8]. Sólo con pace-present." },
      { flag: "--pp-var", type: "number", default: "0.1", min: 0, max: 4, step: 0.05, name: "PP: var-factor",
        desc: "FSR3 varianceFactor del drift target [0,4]. Sólo con pace-present." },
      { flag: "--pp-slew", type: "number", default: "0.05", min: 0, max: 0.5, step: 0.01, name: "PP: slew-gain",
        desc: "Fracción por-tick del error de fase del grid plegada en tick_t0 [0,0.5]. Sólo con pace-present." },
      { flag: "--pace-hard", type: "switch", default: false, name: "Pace-hard",
        desc: "Pacer duro: fija cada frame al borde QPC (sleep-then-spin acotado). own-window only; freeze-floor < 1 vblank. DEFAULT OFF." },
      { flag: "--ph-spin", type: "number", default: "2.0", min: 0.1, max: 4, step: 0.1, name: "PH: spin (ms)",
        desc: "Presupuesto del spin final de --pace-hard (ms [0.1,4])." },
      { flag: "--pace-vblank", type: "switch", default: false, name: "Pace-vblank",
        desc: "Phase-lock al vblank real (DwmGetCompositionTimingInfo). own-window + requiere pace-hard. DEFAULT OFF." },
      { flag: "--pv-lock-gain", type: "number", default: "0.05", min: 0, max: 0.5, step: 0.01, name: "PV: lock-gain",
        desc: "Fracción por-query del error de fase de vblank plegada en tick_t0 [0,0.5]. Sólo con pace-vblank." },
    ],
  },
  {
    title: "Latencia / Responsividad",
    controls: [
      { flag: "--low-d", type: "switch", default: true, name: "Low-D (floor-trim)",
        desc: "Recorta el término span del ancla D (menos input-lag); freshage_ema queda como piso de freeze. Requiere phasefix. DEFAULT OFF." },
      { flag: "--low-d-frac", type: "number", default: "0.5", min: 0, max: 1, step: 0.1, name: "Low-D: frac",
        desc: "Fracción del término span conservada [0,1] (1=sin recorte). Sólo con low-d." },
      { flag: "--low-d-span-cap", type: "number", default: "1.5", min: 1, max: 8, step: 0.5, name: "Low-D: span-cap",
        desc: "Techo de crecimiento del término span (source-frames [1,8]). Sólo con low-d." },
      { flag: "--rfp", type: "switch", default: false, name: "Real-fast-path",
        desc: "En ticks que ya caen al real más fresco, presenta el real (sin par de interp). Implica async-present. DEFAULT OFF." },
      { flag: "--rfp-window", type: "number", default: "0.15", min: 0, max: 1, step: 0.05, name: "RFP: window",
        desc: "Tolerancia de fase del rfp (dispara con phase >= 1-window) [0,1]. Sólo con rfp." },
      { flag: "--rfp-fresh", type: "switch", default: false, name: "RFP-fresh (rediseño)",
        desc: "Presenta el real CAPTURADO más fresco (no el del par). Trade: content sawtooth. Requiere --rfp." },
      { flag: "--asw", type: "switch", default: false, name: "ASW (extrapolación)",
        desc: "Extrapolación fwd acotada (proyecta cur al frente; rellena déficit). Requiere sync-clock. DEFAULT OFF." },
      { flag: "--asw-max", type: "number", default: "1.0", min: 0, max: 4, step: 0.5, name: "ASW: max",
        desc: "Cota de extrapolación en unidades de fase [0,4]." },
      { flag: "--camera-twarp", type: "switch", default: false, name: "Camera-twarp",
        desc: "Lead de cámara en el muestreo del FG (la vista LIDERA la captura; sincroniza el ratón). DEFAULT OFF." },
      { flag: "--camera-twarp-amt", type: "number", default: "0.5", min: 0, max: 3, step: 0.1, name: "Camera-twarp: amt",
        desc: "Escalar de lead eye-tunable [0,3]." },
      { flag: "--motion-fallback", type: "switch", default: false, name: "Motion-fallback (AFMF2)",
        desc: "Fallback frame-level: sobre dispersión gme alta presenta el real (no warp basura). Implica async. DEFAULT OFF." },
      { flag: "--mf-disp", type: "number", default: "50", min: 0, max: 100, step: 1, name: "MF: dispersión (%)",
        desc: "Cota de dispersión gme para el fallback (% [0,100]; sobre ella presenta un real)." },
    ],
  },
  {
    title: "Make-space / Throughput",
    controls: [
      { flag: "--no-load-governor", type: "switch-off", default: true, name: "Load governor",
        desc: "DEFAULT ON (PART-A). Piso de tier-5 por util del 4090 para el colapso del multiplicador en combate. Mantiene flow+warp; suelta trabajo opcional." },
      { flag: "--gov-util", type: "number", default: "92", min: 50, max: 100, step: 1, name: "Gov-util (%)",
        desc: "Umbral de util del 4090 que dispara el tier-5 [50,100]. Sólo con load-governor." },
      { flag: "--no-deficit-tier", type: "switch-off", default: true, name: "Deficit-tier",
        desc: "DEFAULT ON. Shed de object_repair/memory en escenas pesadas bajo déficit sostenido. Apagar = sin tier-4." },
      { flag: "--no-tiers", type: "switch-off", default: true, name: "Pressure tiers",
        desc: "DEFAULT ON. Tiers de presión STAGE-84 (bwd-skip + shed graduado). Apagar = sólo bwd-skip." },
      { flag: "--fdrop", type: "switch", default: false, name: "F-drop (dup exactos)",
        desc: "Descarta frames duplicados EXACTOS en presentación (elide warp redundante). Implica async. DEFAULT OFF." },
      { flag: "--fdrop-quiet-ms", type: "number", default: "0", min: 0, step: 0.5, name: "F-drop quiet (DIFERIDO)",
        desc: "Stage-B soft near-dup: PARSEADO pero LÓGICA DIFERIDA (inerte en 0)." },
      { flag: "--fdrop-k", type: "number", default: "0", min: 0, step: 0.1, name: "F-drop k (DIFERIDO)",
        desc: "Stage-B sensibilidad de movimiento: PARSEADO pero LÓGICA DIFERIDA (inerte en 0)." },
      { flag: "--upload-xfer", type: "switch", default: false, name: "Upload-xfer (DMA)",
        desc: "Mueve el upload del warp a la cola transfer/DMA del 4090 (overlap). Implica async. Force-off sin familia transfer. DEFAULT OFF." },
      { flag: "--fwd-prestage", type: "switch", default: true, name: "Fwd-prestage",
        desc: "Prestage de la copia B fuera del flow submit (colapsa el gap de build). Ruta serial + iGPU-convert. DEFAULT OFF." },
      { flag: "--fwd-pipeline", type: "switch", default: false, name: "Fwd-pipeline",
        desc: "Pipelining fwd cross-par (WAP; +1 par de lag de publish). Opt-in A/B. DEFAULT OFF." },
      { flag: "--fg-prebake", type: "switch", default: false, name: "FG-prebake (descriptores)",
        desc: "Pre-bake de descriptor sets al init (evita el burst vkUpdateDescriptorSets por-par). Alivio host-CPU. DEFAULT OFF." },
      { flag: "--shallow-queue", type: "switch", default: false, name: "Shallow-queue",
        desc: "Colapsa la profundidad async ~1->~0 en ticks con headroom (re-poll acotado). Implica async. DEFAULT OFF." },
      { flag: "--shallow-queue-budget-us", type: "number", default: "350", min: 0, max: 4000, step: 50, name: "Shallow-queue budget (us)",
        desc: "Cap del poll de early-promote (us [0,4000]; 0=inerte). Sólo con shallow-queue." },
    ],
  },
  {
    title: "Protección de hilos / Estabilidad",
    note: "Apagar un interruptor emite su --no-X. --no-fg-protect no toca pin/async (usa sus propios interruptores).",
    controls: [
      { flag: "--no-pin", type: "switch-off", default: true, name: "Pin de hilos",
        desc: "DEFAULT ON (PART-A). Fija C/F/P a núcleos (P elevado RT). Apagar = hilos OS bare (byte-identical). NO arregla el slip ligado a GPU." },
      { flag: "--pin-test", type: "select", default: "0", name: "Pin-test (ablación)",
        options: [
          { value: "0", label: "0 FULL (pin C/F/P + elevate P+F)" },
          { value: "1", label: "1 NO-FLOW-RT" },
          { value: "2", label: "2 PRIO-ONLY" },
          { value: "3", label: "3 AFFINITY-ONLY" },
          { value: "4", label: "4 NEITHER" },
          { value: "5", label: "5 MMCSS-COMPOSITE" },
        ],
        desc: "Selector de ablación de la política de pin (sólo con --pin). fg-protect fija mode-5 por defecto." },
      { flag: "--no-fg-protect", type: "switch-off", default: true, name: "FG protect (S3)",
        desc: "DEFAULT ON (PART-A). Paquete S3 contra GPU99%+CPU100%: MMCSS-composite mode-5 + async-present + GAME_FLOOR (nunca toca la afinidad del juego). (async-present vive en Presentación.)" },
    ],
  },
  {
    title: "Overlay / Diagnóstico",
    controls: [
      { flag: "--fps-overlay", type: "switch", default: false, name: "Overlay de FPS",
        desc: "Overlay 'in->out' estilo LSFG dibujado arriba-izquierda sobre el frame presentado (ruta sync; el path async lo omite)." },
      { flag: "--csv", type: "text", default: "", name: "CSV de telemetría", placeholder: "salida.csv",
        desc: "Exporta telemetría por present a este CSV." },
      { flag: "--latency-trace", type: "switch", default: false, name: "Latency trace",
        desc: "Solo medición: emite una línea [lat-trace] con la descomposición de latencia del pipeline." },
      { flag: "--wsub", type: "switch", default: false, name: "W-sub (warp timings)",
        desc: "Sub-tiempos por-tick del warp-lambda (up/rec/gpu/prs ms) en la línea de stats." },
      { flag: "--fsub", type: "switch", default: false, name: "F-sub (flow split)",
        desc: "Split F por-par fsub(flow/pair/cpu) ms en la línea de stats (premisa STAGE-85)." },
      { flag: "--dump", type: "number", default: "0", min: 0, step: 1, name: "Dump frames",
        desc: "Vuelca los próximos N frames presentados a frames\\ como BMP (diagnóstico)." },
      { flag: "--objdump", type: "number", default: "0", min: 0, step: 1, name: "Obj-dump (grids)",
        desc: "Vuelca N pares de BLOCK GRIDS a frames\\ (máscara de disidencia, |MV|, persist)." },
      { flag: "--pairdump", type: "number", default: "0", min: 0, step: 1, name: "Pair-dump (inputs)",
        desc: "Vuelca los 2 frames full-res que el warp recibe por par (el plano de INPUT)." },
      { flag: "--outdump", type: "number", default: "0", min: 0, step: 1, name: "Out-dump (warp out)",
        desc: "Vuelca N salidas WARP presentadas (wapOutA tras la fence, fase t en el nombre)." },
      { flag: "--phaselog", type: "number", default: "0", min: 0, step: 1, name: "Phase-log",
        desc: "Loguea la escalera de t_use presentado para los próximos N pares (STAGE-78)." },
      { flag: "--duration", type: "number", default: "0", min: 0, step: 1, name: "Duración (s)", placeholder: "0 = ilimitado",
        desc: "Run acotado por wall-clock: tras ~N s, teardown limpio (= --exit-after). 0 = ilimitado." },
      { flag: "--max-frames", type: "number", default: "0", min: 0, step: 1, name: "Max frames", placeholder: "0 = ilimitado",
        desc: "Run acotado por presents (total_frames). 0 = ilimitado. Para el testbench." },
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
    btn.textContent = "Detectar monitores";
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
  lbl.textContent = "Ventana objetivo (captura)";
  const refresh = document.createElement("button");
  refresh.className = "btn btn-ghost";
  refresh.textContent = "Refrescar";
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
  target.textContent = "Objetivo: (ninguno)";

  wrap.appendChild(head);
  wrap.appendChild(sel);
  wrap.appendChild(target);
  g.appendChild(wrap);

  resetWindowSelect("Pulsa «Refrescar» para listar ventanas");

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
    t.textContent = "Objetivo: (ninguno)";
  } else if (exe) {
    t.textContent = `Objetivo: ${title} (${exe})`;
  } else {
    t.textContent = `Objetivo: ${title}`;
  }
}

async function refreshWindows() {
  if (!invoke) return;
  const sel = document.getElementById("window-select");
  resetWindowSelect("cargando…");
  try {
    const wins = await invoke("list_windows");
    lastWindows = Array.isArray(wins) ? wins : [];
    resetWindowSelect(
      lastWindows.length
        ? `— seleccionar ventana (${lastWindows.length}) —`
        : "— sin ventanas con título —"
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
    resetWindowSelect("error al listar ventanas");
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
  statePill.textContent = on ? "en ejecución" : "detenido";
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
  logLine("[ui] reiniciando FG con la nueva config…", "sys");
  // Belt-and-suspenders del guard de epoch del backend: marcar el reinicio en vuelo para que
  // el "fg-exit" del hijo viejo (si el backend llegara a emitir uno) no nos pase a "detenido".
  restarting = true;
  try {
    await invoke("restart", { args, exePath });
  } catch (e) {
    logLine("[ui] error al reiniciar: " + e, "exit");
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
  logLine("[ui] consultando --list-monitors ...", "sys");
  try {
    const out = await invoke("list_monitors", { exePath: exeInput.value || DEFAULT_EXE });
    for (const l of String(out).split(/\r?\n/)) if (l.trim() !== "") logLine(l, classify(l));
  } catch (e) {
    logLine("[ui] error: " + e, "exit");
  }
}

btnStart.addEventListener("click", async () => {
  if (!invoke) {
    logLine("[ui] API de Tauri no disponible.", "exit");
    return;
  }
  const args = buildArgs();
  const exePath = exeInput.value || DEFAULT_EXE;
  setRunning(true);
  logLine("[ui] iniciando: " + exeBasename(exePath) + " " + args.map(quoteArg).join(" "), "sys");
  try {
    await invoke("launch", { args, exePath });
  } catch (e) {
    logLine("[ui] no se pudo iniciar: " + e, "exit");
    setRunning(false);
  }
});

btnStop.addEventListener("click", async () => {
  if (!invoke) return;
  try {
    await invoke("stop");
  } catch (e) {
    logLine("[ui] error al detener: " + e, "exit");
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
    logLine("[ui] proceso finalizado (código " + (code ?? "?") + ")", "exit");
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
