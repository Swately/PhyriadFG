# PhyriadFG — STEP 5: FgContext + extraer los 3 hilos a sus módulos

> Estado de entrada: pasos 2-4 hechos (cli/core/flow/capture/warp_blend/present/instrument extraídos,
> build verde). `core/main.cpp` ≈ 7080 líneas = el `main()` gigante: init-seq + los **3 hilos C/F/P
> (lambdas `[&]`)** + present-loop + teardown. Objetivo: los 3 hilos como funciones libres
> `run_capture/flow/present(FgContext&)` en sus módulos; `main()` más delgado.

## La técnica clave (minimiza riesgo — cuerpos VERBATIM)

El problema: los hilos son lambdas `[&]` que capturan cientos de locals de `main()`. Reescribir cada
referencia `x` → `ctx.x` serían MILES de ediciones (alto riesgo). En vez de eso:

1. **`FgContext` = struct de REFERENCIAS** (`T&`) al estado compartido que vive en `main()`. (Variante 1:
   el init de `main()` queda INTACTO; `FgContext` solo agrega referencias a sus locals → riesgo mínimo.)
2. En `main()`, tras el init (sin tocar), se construye **una vez** `FgContext ctx{ .a=a, .b=b, ... }`
   (aggregate-init ligando las referencias).
3. Cada hilo se vuelve `void run_X(FgContext& ctx)` con un **preámbulo de alias** al inicio:
   `auto& a = ctx.a; auto& b = ctx.b; …` (uno por cada shared-local que ESE hilo usa) — y luego el
   **cuerpo del hilo VERBATIM** (sin cambios). El alias `auto&` preserva la semántica de referencia,
   idéntica a la captura `[&]`.
4. `main()` lanza `std::thread tC(run_X, std::ref(ctx));`.

Por qué es seguro: el init no se mueve; los cuerpos no se editan; lo único nuevo es el struct `FgContext`,
su construcción, y el preámbulo de alias por hilo. Los globales (`g_quit`, `vk_live`, …) ya están en
`core/globals.hpp` (no van en `FgContext`). `FgContext` solo lleva los LOCALS de `main()` que comparten.

## Sub-pasos (cada uno build-VERDE; FgContext crece incrementalmente)

- **5.1** `core/fg_context.hpp` — `struct FgContext` (arranca vacío; crece por hilo). `#include` de los
  tipos que necesite (vulkan, los structs de pipeline de las capas, `cli/cli.hpp` para `Config`, etc.).
- **5.2** Extraer **`run_capture`** (hilo C — el más simple: acquire WGC/DDA → convert → publish `c_seq`).
  El agente: lee el cuerpo del hilo C, identifica los locals de `main()` que usa, los añade como campos
  ref en `FgContext`, construye `ctx` en `main()`, pone el preámbulo de alias + el cuerpo VERBATIM en
  `capture/capture.cpp` como `run_capture(FgContext&)`, y `main()` lanza `std::thread(run_capture, std::ref(ctx))`.
  Build verde.
- **5.3** Extraer **`run_flow`** (hilo F) → `flow/flow.cpp`. Igual; `FgContext` crece con lo que F usa
  (mucho solapa con C: el ring, los seq atomics, el canal F→P). Build verde.
- **5.4** Extraer **`run_present`** (hilo P) → `present/present.cpp`. El más grande (present-loop + pacing).
  Build verde.
- **5.5** (opcional, Variante 2) `init_*(ctx)` + `main.cpp`→`core/app.cpp` orquestador delgado: mover el
  init-seq a funciones y que `FgContext` PASE A POSEER el estado (refs → campos propios). Mayor esfuerzo;
  solo si se quiere la pureza total. La Variante 1 (5.1-5.4) ya entrega "los 3 hilos en sus módulos".

## Gates (no negociables)
- Relocación pura: NINGÚN cambio de lógica. Cuerpos de hilo verbatim (solo el preámbulo de alias es nuevo).
- Build VERDE + `--help` exit 0 tras CADA sub-paso (el build es la prueba: un alias faltante = error de
  compilación en run_X; un campo de más = inofensivo).
- Sin tocar `framework/`, shaders. Sin commit. CRLF/no-BOM como los hermanos.
- Si un hilo usa un local que se declara DESPUÉS del punto de construcción de `ctx`, reordenar la
  construcción de `ctx` (no la lógica) o STOP y reportar.

## Verificación de equivalencia final
Tras 5.4: build verde + `--help` + (en el rig, prueba del operador) mismo comportamiento que render_assistant.
La equivalencia de lógica está garantizada por construcción (cuerpos verbatim + alias = misma semántica que `[&]`).
