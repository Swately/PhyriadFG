# PhyriadFG — Plan de separación del monolito por capas

> Estado: **vertido coarse HECHO** (todo el código en la estructura; `src/core/main.cpp` = el monolito de
> render_assistant, 9912 líneas, + 4 helpers). Este doc = el análisis profundo + el mapa de la separación
> FINA por capas + el orden de extracción **verificado-por-build**. Fundamento: el code-map de 127 secciones
> (`G:\phyriad\docs\slides\PHYRIADFG_CODEMAP_SLIDES.tex`, verificado de primera hand).

## 0 · El principio (por qué NO se atomiza de un golpe)

`main.cpp` es **un solo `main()` gigante** (líneas ~3072–9912): una secuencia de init ordenada-por-dependencias
+ los 3 hilos **C/F/P como lambdas `[&]`** que capturan **cientos de locals** + el bucle de present. Eso NO se
puede repartir en módulos sin antes hacer explícito ese estado en un **`FgContext`** y convertir cada captura
`[&]` en `ctx.campo`. Hacerlo todo de un tirón = un mar de código que no compila.

**Disciplina:** verde-primero, extracción incremental. Cada etapa deja el build VERDE antes de la siguiente.

Esto parte el código en dos clases:
- **PRE-`main()` (líneas 1–3071): top-level, extraíble LIMPIO ya** — Config/cli, funciones-fábrica libres,
  structs, helpers, globals. Son unidades autocontenidas → se mueven a su módulo y `main.cpp` las `#include`.
- **DENTRO de `main()` (3072–9912): atado-a-`main()`** — el init-seq, los hilos C/F/P, el present-loop, el
  teardown. **Se queda en `core/` (el orquestador)** hasta la etapa profunda del `FgContext`.

## 1 · Mapa capa → módulo (regla: la `Capa` del code-map decide el módulo)

| Módulo | Recibe (capas del code-map) | Secciones PRE-main extraíbles ya | Parte atada-a-main() → queda en `core/` |
|---|---|---|---|
| **cli/** | cli/config | Config enums+struct (109–961), print_help (1020–1126), parse_args+parse_extra (1127–1683) | — |
| **capture/** | capture | WGC/WinRT interop (87–107), `route_for` (1887–1897), D3D/DXGI init `d3d_init`+staging (1899–1961), WgcCtx (3145–3191), convert-pipe factory (4460–4479 es post-main → queda) | Thread C body (5490–5697), capture init en main (3193–3251, 4242–4459) |
| **flow/** | flow | `gme_fit_affine` (1706–1830), NVOFA provider (2271–2525), mv-smooth pipe (2650–2674), gme-gpu pipe (2676–2849), median pipe (2986–3022) | Thread F body + consume_wap (5699–7315), OFP/nvofa init en main (4480–4611) |
| **warp_blend/** | warp · blend · quality | WapPipe factory (2851–2984), matte/dis stat helpers (1831–1856), holon/scene constants (963–1018) | `object_repair`/scene holons (5954–6800, dentro de F), `wap_warp_present` dispatch+blend (7757–8361) |
| **present/** | present · sync | upscale factories (parte de 2527–2648) | PresentSurface create (7459–7536), pacer (7537–7601), `bridge_present_src` (7712–7755), present-loop (8461–9801) |
| **instrument/** | instrument | `telemetry_csv.hpp` (ya helper), dump helpers (1857–1885) | stats por-segundo + GPU-util (en el present-loop) |
| **core/** | init · teardown · util · other · sync(globals) | includes/STL (1–85), VDev (1963–2189), Img/HBuf+barrier/submit helpers (2191–2269), `now_ms` (1685–1704), quit/device-lost globals (3024–3070) | **TODO `main()` (3072–9912):** bootstrap, init-seq, los 3 hilos, present-loop, teardown → `core/app.cpp` |

**Honesto:** en la primera versión compilable, los hilos C/F/P + el init-seq + el present-loop **viven en
`core/app.cpp`** (era `main.cpp`). Los módulos `capture/flow/warp_blend/present` reciben primero solo sus
**fábricas y structs PRE-main**; sus *bodies* de hilo migran en la etapa profunda (FgContext), no antes.

## 2 · Mapa de shaders → capa (los 15 viven planos en `shaders/`; el build los glslc todos)

| Capa | Shaders |
|---|---|
| flow | `gme_reduce`, `gme_solve`, `gme_dissidence`, `mv_smooth`, `mv_median`, `nvofa_convert` |
| warp_blend | `wap_warp`, `wap_fill`, `igpu_field` |
| capture | `hdr_convert`, `igpu_convert_pack`, `unpack_packed` |
| present | `upscale_bilinear`, `upscale_lanczos` |
| instrument | `overlay_fps` |
| (framework/render/vulkan, vendoreado) | `optical_flow_{pyr_down,hier_match,hier_match_fg,warp,affine_fit}` |

## 3 · `FgContext` — la pieza clave (etapa profunda)

El estado compartido que hoy son los locals de `main()` capturados por `[&]` (code-map: declaraciones
3401–3725, alloc 3943–4241, estado cross-thread + canal F→P 5299–5478). Se agrupa por categoría en
`core/fg_context.hpp`:
- **Devices/queues:** A (4090) · B (1080 Ti) · G (iGPU) · FD — los `VDev` + colas.
- **Capture:** el ring D3D11 + `c_seq`/seq atomics + las staging/host bridges.
- **VK images:** Anative/Awork/Bframe/Cinterp/WAP/present-bridge + sus layouts.
- **Pipelines:** convert/flow/warp/upscale/gme/median/overlay + los OFP/NVOFA.
- **Canal F→P:** los arrays `f_pair_*` + `f_seq`/`p_presenting` (backpressure).
- **Pacing/sync:** content_clock NCO, D-calibration, EMAs.
- **Instrument:** los atomics de stats/util/overlay + telemetry_csv.
- **Config:** `const Config&` (de cli/).

Los hilos pasan de lambdas `[&]` a **`run_capture(FgContext&)` / `run_flow(FgContext&)` /
`run_present(FgContext&)`** (free functions en capture/flow/present), y el init-seq a `init_*(FgContext&)`.

## 4 · Orden de extracción (cada paso deja el build VERDE)

1. **Build base (verde-primero):** `CMakeLists.txt` único + `build.bat` que compile `core/main.cpp` + los .cpp
   vendoreados (`framework/**/src`) + genere los `*_spv.hpp` de los 20 shaders (`glslc`). Sin mover nada del
   monolito todavía → `phyriad_fg.exe` compila desde la estructura. **Este es el gate #1.**
2. **cli/** ← extraer Config + parse + print_help a `cli/cli.{hpp,cpp}`; `main.cpp` las incluye. Build verde.
3. **core/util** ← VDev, Img/HBuf, barrier/submit, now_ms, route_for, globals → `core/{vk_util,device,globals}.hpp/.cpp`. Build verde.
4. **Fábricas libres por capa** ← gme_fit/nvofa/mv-smooth/gme-gpu/median → `flow/`; WapPipe → `warp_blend/`;
   convert/upscale/field/fill/unpack → capture/present/warp_blend; d3d_init/route_for/WgcCtx → `capture/`;
   dump → `instrument/`. Cada una build-verde.
5. **`FgContext` (profundo)** ← definir `core/fg_context.hpp` con el inventario completo; reescribir el
   init-seq como `init_*(ctx)` y los 3 hilos como `run_capture/flow/present(ctx)` en sus módulos; `core/app.cpp`
   queda como orquestador delgado (parse → ctx → lanzar hilos → join). **La etapa grande; por sub-pasos.**
6. **tools/ + bench/** ← sus `CMakeLists`/build propios mínimos (opt-in; no en el build del exe principal).

## 5 · Build (mata la "cantidad exagerada de builds")

UN `CMakeLists.txt`: un `add_executable(phyriad_fg core/*.cpp <todos los módulos>/*.cpp framework/**/src/*.cpp)`
\+ una función `spv()` que glslc cada `.comp` (shaders/ + framework/render/vulkan/shaders/) → `*_spv.hpp` en un
dir generado del include-path. **Sin `add_subdirectory`, sin targets de librería, un solo `build/`.** `build.bat`
= `vcvars64 && cmake -B build -G Ninja && cmake --build build`.

## 6 · Estado actual
- ✓ framework/ vendoreado (24 archivos, 5 pilares).
- ✓ vertido coarse: monolito + helpers en `src/core/`, 15 shaders, capture_dump, 3 benches, root.
- ▶ SIGUIENTE = paso 1 (build base verde), luego 2→5 incrementales.
