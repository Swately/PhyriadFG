# PhyriadFG — mapa de arquitectura (para tomar el control)

Rama-fork standalone del `render_assistant` de Phyriad: **mismo comportamiento, separado por capas, un
build**, libre de los dogmas de Phyriad. Enfoque = **máximo rendimiento**.

## Build (un comando)
`build.bat` → `build\phyriad_fg.exe`  ·  requiere MSVC + Vulkan SDK + Ninja. Un target, un `build\`, **LTO
activado** (la división en módulos es perf-free).

## Estructura
```
src/
  core/      ORQUESTADOR + infra compartida
    main.cpp        main(): init-seq -> FgContext ctx -> lanza 3 hilos -> join -> teardown
    fg_context.hpp  FgContext = estado compartido EXPLÍCITO (lo que antes eran los locals capturados [&])
    device · vk_util · globals · ra_simd · telemetry_csv · compat_reason
  cli/         Config + flags + parse + help
  capture/     run_capture(FgContext&) [hilo C] · D3D/DXGI (DDA) · WGC · convert/unpack
  flow/        run_flow(FgContext&)    [hilo F] · gme_fit · NVOFA · mv-smooth · gme-gpu · median · holons
  warp_blend/  WapPipe (warp-at-presenter) · field/fill · stat-helpers
  present/     run_present(FgContext&) [hilo P] · PresentSurface · pacing · bridge · present-loop
  instrument/  dump helpers
framework/   pilares de Phyriad VENDOREADOS (copias literales): render/present · render/vulkan (OFP + 5
             shaders) · schema · hal · topology
shaders/     15 .comp del app   ·   tools/ capture_dump   ·   bench/ micro-benches
```

## Pipeline de un frame
```
capture (C) -> flow (F: MV/SAD) -> warp+blend (en P: backward-warp A,B -> t) -> present (P, paced, drop-late)
```
3 hilos C/F/P comunicados por `FgContext` (rings SPSC + `c_seq`/`f_seq`/`p_presenting`). Reparto multi-GPU:
flujo en 1080 Ti, warp en 4090, convert en iGPU. Genera frames intermedios I_t entre los reales.

## ¿Dónde toco qué?
| Para tocar… | Archivo |
|---|---|
| flags / defaults | `cli/cli.{hpp,cpp}` |
| captura (DDA/WGC) / ingest | `capture/capture.cpp` (`run_capture`) |
| flujo óptico / gme / holons | `flow/flow.cpp` (`run_flow`) |
| warp / blend / calidad | `warp_blend/warp_blend.cpp` + el warp dentro de `run_present` |
| pacing / present / sync-clock | `present/present.cpp` (`run_present`) |
| estado compartido entre hilos | `core/fg_context.hpp` |
| arranque / device / alloc | `core/main.cpp` (init-seq — frío, corre 1 vez) |
| un shader | `shaders/*.comp` |

## Rendimiento — por qué la estructura NO lo arriesga
- El trabajo **caliente** (por-píxel/por-frame) vive en los **shaders** (GPU, intactos) y **dentro de cada
  `run_X`** (cuerpo verbatim, intra-TU → inlinea igual que antes).
- Helpers calientes de grabación (`img_barrier`/`full_bic`/`now_ms`/`oneshot`) **inline en headers**.
- Las llamadas cross-TU nuevas son **frías** (fábricas) o **dominadas por GPU** (`submit_wait`).
- `FgContext` (referencias) = misma indirección que la captura `[&]`. Sin asignaciones/locks/copias nuevas
  en el hot path.
- **LTO** cubre cualquier inlining cross-TU restante → equivalente o más rápido que el monolito.

## Equivalencia
La separación fue **relocación pura** (cuerpos de hilo verbatim; `auto& x = ctx.x` = misma semántica que
`[&]`). Comportamiento idéntico a `render_assistant`. Build verde from-scratch + `--help` exit 0 en cada
paso. La equivalencia final en ejecución se confirma corriéndolo sobre un juego.

## Pendiente opcional (NO necesario para rendimiento)
- **Paso 5.5** (`docs/STEP5_FGCONTEXT_PLAN.md`): mover la init-seq a `init_*(ctx)` por fases + `FgContext`
  pasa a POSEER el estado (refs→campos) + `main.cpp`→`core/app.cpp` delgado. Solo legibilidad del init
  (que es frío); es el paso más invasivo. Dejado fuera por riesgo/beneficio.
- Renombrar el banner `render_assistant v1`→`PhyriadFG` (cosmético); los prefijos de log `[ra]` quedan.
- Mapa de secciones detallado: `G:\phyriad\docs\slides\PHYRIADFG_CODEMAP_SLIDES.tex` (refleja el monolito
  PRE-separación; las líneas ya no aplican, pero el reparto por capa sí).
