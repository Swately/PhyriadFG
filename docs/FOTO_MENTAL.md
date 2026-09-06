# FOTO MENTAL — PhyriadFG (the live-thread snapshot; MENTAL_SNAPSHOT_PROTOCOL)

> The ephemeral re-orientation for the NEXT session/compaction. The durable WHERE is
> [`planning/ACTION_PLAN.md`](planning/ACTION_PLAN.md); the durable knowledge is the D-22 memory
> (`phyriadfg-*.md`). Rewritten at every checkpoint; older snapshots are not kept (the spine is).

**Taken:** 2026-09-06 (R7 code half; the 2026-09-04 backlog-audit stamp is superseded). Sections 4b-4j are the
accumulated record and are NOT rewritten — only the orientation (0, 2, 4, 5, Self-prompt) is.

0. **ACTION-PLAN POINTER** — `docs/planning/ACTION_PLAN.md:14` →
   **`P → S2.T6 displacement gap → S4.R3 (fg_core.comp) · next`**. (The old pointer here read
   `S4.0 (operator) ‖ S4.C0 (next)`; BOTH of those nodes are closed — S4.0 done inside R2a, and
   S4.C0 is a retired v1 id whose v2 equivalent S4.R0 is done with a gate record.)
1. **OBJECTIVE** — P: perfect PhyriadFG as ONE final FG = the clean minimal core (`apps/minimal_fg`, the
   SG seam) + the layers that earn their place, on the LAYERTAB contract; exact motion measured as DATA
   (S2 MOTION_TRUTH), perceptual quality parked (S5).
2. **FOCUS** — execution is UNDER WAY, not pending a green light. R0, R1, R2 of the in-place
   restructure and T0/T1/T1b/T1c of the instrument are done and gated; T6 is built and its gate does
   NOT pass. The live work is closing T6's sub-pixel deadzone, because R3's M4 veto rests on that
   oracle. Everything downstream (R3→R7) is unbuilt.
3. **METHOD** — Phyriad protocols: CONDUCT (verify-before-claim, calibrated, zero praise), PLAN_TIER T2
   (no commit with an `open` risk), AAP (closed: A0 frozen sha256 `670687d0…6933`, 3 candidates, 9
   scorecards, ATG 2 lenses, A3 = C + G1–G5, AT3/AT4/ATF approved — `docs/planning/aap/`), DI-3 two runs
   per number, SUBAGENT_DELEGATION (subordinate output = a claim; every judge citation was spot-verified
   first-hand), ACTION_PLAN_PROTOCOL (pointer + node states). Chat Spanish/usted; docs English; signature
   on every touched code file. Workflow tool is authorized for gate fan-outs (ultracode on).
4. **WHERE WE ARE** — CORRECTED 2026-09-04; the previous text here was false in three places and is
   kept only as this note: it said the MOTION_TRUTH and CONVERGENCE triads were "written, NOT built"
   (seven gate records in `planning/records/` say otherwise), it said `--qdump` is inert under
   `async_present=true` (`resolve_config` auto-disables async for the run — corrected at T0), and it
   said the whole working tree was uncommitted (`git status --short` returns 0 lines).
   **BUILT AND GATED:** E1 thin `main()`; R0 (layer registry), R1 (PhaseClock), R2 (seam engine);
   T0 (scorer port), T1 (`--qdump+`), T1b (coverage sampler), T1c (record completeness). Each has a
   record in `planning/records/`. **BUILT, GATE NOT PASSED:** T6 (`tools/ref_warp.py`).
   **NOT BUILT:** R3, R4, R5, R6, R7; T2, T3, T4, T5.
   **Verified defect still to honour:** bg-reclaim dead under the shipping default
   (`wap_warp.comp:344/394/410`) — reproduced bug-for-bug by design (XR7), its fix an operator call.
   **GIT (measured 2026-09-04, at HEAD `99dd28d` before this audit's own edits):** the tree was clean
   — the audit then modified the docs listed in `records/BACKLOG_AUDIT.md`, so do not read "clean" as a
   present-tense claim. The branch `analysis/0.3.0-quality-push` is **10 commits ahead of `origin` and
   unpushed, AND HAS NO UPSTREAM REF** (`git rev-parse @{u}` → "no upstream configured"), so a bare
   `git push` fails; two remotes are configured (`origin` = Swately/PhyriadFG, `chlmateus`). Push is the
   operator's call and has not been asked for. **Tags:** the newest is `v0.2.2-experimental`, **26
   commits back** — 0.3.0 and 0.4.0 shipped untagged, and R7 requires a pre-R3 tag as its A/B reference
   (`CONVERGENCE_MASTER_PLAN.md:275`), which must be cut BEFORE R3 changes the default path. `tools/gate_zoo.ps1` is the operator's own untracked
   tool (2026-06-12), untouched.
4b. **MEDIDO 2026-09-03 (nuevo, el primer número de la base)** — `minimal_fg.exe` (binario del 2026-09-02,
   fuentes sin cambios desde 2026-06-26) corre limpio: contra `gate_zoo` (fuente ~30 fps, ventana 1280×720)
   1,200 presents, `ok=1200 timeout=0 err=0`, sin device-loss, `ring_dropped=0`, SG compilado = 1 pase /
   2 barreras; **`in ≈ 22 fps` con `out = 240 presents/s` y `captured = 108`** → ~1,092 de 1,200 presents
   re-muestran la MISMA imagen interpolada (t=0.5 fijo, un warp por par). Es decir: la base **no multiplica
   frames**; su contenido único por segundo = la ingesta. No verifiqué por frame si se presentó la rama
   interpolada (`blit_c`) o el respaldo crudo (`blit_raw`) — los contadores no las distinguen.
4c. **RUMBO (2026-09-03, operador: "hagamos tu recomendación")** — reestructurar el donante EN SU SITIO por
   etapa; la base es plano, no anfitrión; se adopta solo `seam_graph.hpp` + su prueba. Definición de las
   seis etapas + dos planos en `planning/STAGE_CONTRACT.md` (`proposed`; orden R0–R6 en §5; decisiones
   abiertas en §6: split CAPTURE/INGEST, nombres de directorios, multi-GPU condicional, reloj antes que
   núcleo). **APROBADO** ("Sí adelante", 2026-09-03): STAGE_CONTRACT `approved`; la tríada CONVERGENCE
   re-apuntada a v2 (R0–R7 en el donante; v1 C0–C6 conservada como registro en §2.1; XR8/XR12
   `superseded`, XR10 re-alcance, XR14 nuevo = paridad de bits del reloj); S4.0 = adoptar solo
   `seam_graph.hpp` + test (dentro de R2). Regla nueva en vigor: ningún plan nuevo sin una medición nueva.
4d. **R0 HECHO (2026-09-03, G-R0 PASÓ — `planning/records/R0_GATE.md`)** — `src/layers/` (esquema + registro
   de 12 filas + runtime), `tools/layer_gen.cpp` (generador C++ desde las mismas tablas), `tools/check_flag_roundtrip.py`,
   ediciones en cli/main/CMake/UI. Números: builds ×2 OK; `--layer-dump` idéntico ×2, contrato
   `0xB8C7BD1BC5BACB3C`; paridad 0 fallos en 297 lanzamientos (default + 38 combos + round-trip de 258 tokens);
   `--help` −7/+24; JSON 18 controles y la UI los renderiza desde el binario; **M2a = 2 archivos**; **M3** (2/lado,
   intercalado, 60 s): presents 14,391±12 vs 14,383.5±1, únicos/s 239.15 vs 239.06, lat 15.18±2.26 vs 16.58±0.12 ms
   — dentro de la dispersión; **cierre de columnas = 0 columnas nuevas → PROCEDER** (restricciones para R3: etapa
   `WEIGHT` con el núcleo partido en `fg_sample`/`fg_blend`; canal `CH_BLEND` + fila `select`). El producto no
   cambió (shader/push/defaults intactos). El árbol sigue sin commit.
4e. **R1 HECHO (2026-09-03, G-R1 PASÓ — `planning/records/R1_GATE.md`)** — el RELOJ es ahora
   `src/clock/phase_clock.{hpp,cpp}` (621 líneas, 4 métodos) con prueba de CPU `pfg_clock_test` y el
   instrumento `--arrival-log`. `present.cpp` 3,054 -> 2,818. Números: **98.80 % verbatim** (5
   transformaciones documentadas T1..T5 + un hoist); **replay de 14,390 ticks vivos con 0 discrepancias**
   en todos los campos; sintético: engancha en 6 ticks, 0 retrocesos, 4 fases distintas por par, el PLL
   sigue un salto 60->30 fps; A/B en vivo (2 corridas/lado): media de fase 0.5020 -> 0.5015, sd 0.2829 ->
   0.2823, **paso de contenido por present 0.2502 -> 0.2501 frames fuente = la multiplicación 4x medida**;
   humo 120 s exit 0 (28,797 presents). Las CAPAS de fase (phase-norm / s2 / cphase / fdrop) siguen en
   present.cpp tras sus flags: el reloj quedó puro.
4f. **R2a HECHO / R2b PENDIENTE (2026-09-03 — `planning/records/R2_GATE.md`)** — habilitado Vulkan 1.3 +
   `synchronization2` (consultado, no asumido; ambos dispositivos ENABLED; `--no-sync2` es el brazo A/B);
   añadido `--validation`: **0 líneas de validación en 12 s**. Motor de costura adoptado a
   `src/seam/seam_graph.hpp` + `tests/seam/` con los injertos G3 (aviso de dominancia), G4
   (`optional_write` + liveness) y G5 (`execute()` sin asignaciones, con `graph_id` por XR11):
   **142 checks pasan** (122 adoptados + 13 de injertos + 7 de forma). La copia del contenedor quedó
   congelada con una nota (XR10). El instrumento se cazó a sí mismo: la primera corrida con validación
   reportó una fuga que era mi propio mensajero sin destruir; corregido.
   **R2b, mismo día: los dos huecos del motor CERRADOS** — capa de importación por imagen
   (`declare_image(..., import_layout)`) y capa de reposo declarada (`set_resting`) que emite una barrera
   de EPÍLOGO entre frames. `--sg-barriers` ya graba las barreras de salida de la etapa 5 por el grafo, y
   deriva EXACTAMENTE las tres escritas a mano (afirmado campo por campo; `--sg-dump` idéntico ×2).
   **148 checks**; G3 cazó una dominancia real del camino vivo (el blit sobrescribe todo el puente), ahora
   DECLARADA con su razón; **0 líneas de validación** en un soak de 60 s saturado con `gpu_load`; M3 con
   2 corridas por lado dentro de la dispersión (14,384 presents en las cuatro). **G-R2 PASÓ.** La ruta
   derivada es OPT-IN: el default sigue con las barreras a mano, así que el producto no cambió.
4g. **S2.T0 + T1 HECHOS (2026-09-03 — `planning/records/S2_T0_T1_GATE.md`)** — scorer portado a
   `tools/fg_quality_scorer/` apuntando al flujo VENDORIZADO (build exit 0; modo T sobre un volcado fresco
   → 6 filas). Dos defectos de build del catálogo corregidos (`/O2` fijo contra `/RTC1` de Debug; sin tipo
   de build por defecto). **Una afirmación documentada era FALSA y quedó corregida**: `--qdump` NO es
   inerte bajo el default — `resolve_config` desactiva `--async-present` para la corrida y lo anuncia
   (verificado). T1: sidecars por tick `mv.rg16f` + `sad.rg16f` (129,600 B = 240×135×4) + `push.bin`
   (232 B constante) + tokens del manifiesto; la generación se RECORDA en el sitio de `wap_upload()`, no
   se recalcula; el push se copia en el sitio del submit. `tools/check_qdump_plus.py` valida tamaños, el
   decode float16 del MV, y el `t` del push contra el `t` del manifiesto (iguales en todos los ticks).
   **HALLAZGO que bloquea R3:** el MUESTREO no reparte fases. En 16 triples el `t` cayó en solo 2 bins
   (diez en ≈0.125, seis en ≈0.375) y todos en UNA generación. M4 sobre ese corpus probaría el núcleo en
   una o dos fases. El checker ya AVISA. Eso es S2.T1b y es precondición de R3.
4h. **S2.T1b HECHO (2026-09-03 — `planning/records/S2_T1B_GATE.md`)** — el stride se reemplazó por un
   MUESTREADOR DE COBERTURA: se vuelca en el tick cuyo bin de fase es el menos cubierto de los que la
   escalera realmente produce, y cuyo slot del anillo también lo es (la condición de slot se suelta tras 64
   descartes para que las dos no se traben; un hueco de 8 ticks aleja al siguiente candidato del stall que
   acabamos de causar). Un stride NUNCA podía funcionar: el volcado frena su propio tick y el reloj se
   recupera igual cada vez, así que la fase N ticks después es función del stall, no de N. Medido en dos
   corridas independientes de 16 triples: **8/8 bins a 2 cada uno** y **4/4 bins alcanzables a 4 cada
   uno**, 3/3 slots del anillo en ambas (antes: 2 bins, 10/6, 1 slot). La diferencia 8 contra 4 es el
   transitorio de ENGANCHE del reloj, no varianza del muestreador: una escalera enganchada a 4× emite
   exactamente cuatro fases, y ese techo lo pone la razón de refresco, no el muestreador.
4i. **S2.T1c HECHO (2026-09-03 — `planning/records/S2_T1C_GATE.md`)** — no estaba planeado. Al empezar a
   dimensionar T6 decodifiqué un push real (58 floats) en vez de creerle a la premisa del plan, y la
   premisa NO sobrevivió: bajo el DEFAULT DE PRODUCCIÓN el warp lee cuatro planos más que el registro de T1
   no tenía — MV hacia atrás (`occl_thresh`, `phase_anchor_on`), persistencia (`inertia_thresh`), los
   candidatos SAD (`ambig_on` + `gme_on`) y el MV de la generación objetivo (`vblend_on`). (Mi primer
   pase dijo SEIS: agregué las dos máscaras de disidencia creyéndolas ligadas a `gme_on`. Releyendo cada
   sitio `texture(u_dissidence` con su `if` envolvente, todas están ligadas a `matte_on`, que aquí es 0.
   Se vuelcan igual, pero el auditor ya no las exige para un push que no las lee.) Un warp de referencia alimentado con el registro de T1 no habría podido
   reproducir NI UN tick del default, y la falla se habría visto como bug de la referencia y no como
   hueco del registro. Los cuatro ya se vuelcan (más las dos máscaras, para las corridas que sí las leen) y `tgen` queda
   registrado en el sitio del upload; un plano ausente se nombra `-`. Verificado que NO se leen bajo este default, del mismo push: `u_field` y
   `u_prev_out`. `check_qdump_plus.py` ahora AUDITA la reproducibilidad por registro: el registro previo
   dice NOT REPLAYABLE con los seis huecos nombrados; el de T1c dice REPLAYABLE.
   **Lo que esto le aclara a T6:** bajo `single_track = 1.0` el store final es
   `mix(B_samp, cur[uv], w_s)` y toda la cascada commit/matte/onepos/blend queda sombreada; el trabajo
   real de la referencia es el **MV efectivo hacia adelante** (fetch guiado `mv_guided=1.1`,
   `bg_reclaim=4`, ancla de fase contra el campo hacia atrás, y el tilt de vblend hacia `mvt`).
4j. **S2.T6 CONSTRUIDO, COMPUERTA NO PASADA (2026-09-03 — `planning/records/S2_T6_GATE.md`)** —
   `tools/ref_warp.py` reproduce el store del default. La superficie es chica: con `single_track = 1.0`
   el store es `mix(B_samp, cur[uv], w_s)` y toda la cascada commit/matte/onepos/blend queda sombreada,
   así que sólo deciden las líneas 279–522 del shader más el bool de stasis. El uso que el ancla de fase
   hace del `mv_fwd` PRE-reclaim se reproduce bug-por-bug (XR7); `unsupported()` RECHAZA por nombre
   cualquier push que arme un camino no implementado.
   **Pasa:** 99.88% de píxeles exactos en el default (739–788 px por cuadro fuera, todos en un anillo en
   la silueta en movimiento). **NO pasa:** con `--st-no-stasis` (que quita la copia de stasis del 99% y
   deja `result = B_samp` pelado) el exacto cae a 84.39% y la escala de desplazamiento por mínimos
   cuadrados da **k = 0.702** (0.829 en el registro default) contra el 1.000 que la compuerta exige.
   **La FORMA del error importa más que su tamaño:** al binear el ajuste por magnitud de desplazamiento
   aparece una ZONA MUERTA sub-píxel, no una escala. Arriba de 4 px la referencia acierta (k = 0.955,
   corr 0.97); abajo de 0.5 px el shader NO mueve nada (cambio medio 0.03–0.09 de 255) mientras la
   referencia mueve 0.9–4.0. El k de cuadro entero es sólo el promedio de esas dos poblaciones.
   **Descartado, cada uno con un número:** registro rancio (una instantánea tomada en la llamada a
   `wap_upload` es idéntica a la lectura tardía en 100.00% de los téxeles), cuantización del filtro de la
   GPU (8 y 6 bits EMPEORAN el ajuste), cada etapa del MV por ablación (k se mueve 0.005), la regla de
   ambigüedad como amortiguador (dispara en 0.06% de los bloques) y `bg_reclaim` como amortiguador (su
   `nonconf` es 0 en el fondo). **Pista abierta:** el campo MV trae un patrón sub-píxel de periodo 3
   (−0.5, +0.1666, 0) que coincide con la retícula de 24 px del zoo, en bloques cuyo propio `sad_best` es
   0 (coincidencia perfecta) en 99.5% de la grilla.
4k. **ZONA MUERTA RESUELTA (2026-09-04 — `planning/records/S2_T6_GATE.md` §6)** — era el CONTENIDO de
   prueba, no el shader. Se le dio a `ball_zoo.ps1` un fondo aperiódico (`-BgClass noise`) dentro del
   MISMO arnés, y se repitió la medición idéntica. **Mecanismo medido:** el matcher declara
   `sad_best = 0` en ~99.5% de los bloques en AMBOS fondos, pero sobre la retícula emite igual un
   vector de medio píxel ahí: mediana |mv| **0.500 px** contra **0.034 px** en aperiódico (15x). El
   patrón de periodo 3 era eso: un ajuste parabólico sub-píxel sobre contenido cuyo mínimo de SAD se
   repite cada 24 px. **La población de la zona muerta pasa de 138,696 píxeles a 67.** Donde el
   movimiento es real: **k = 0.956, corr 0.987** (arriba de 4 px). Cuadro entero 0.889 / 0.903 en dos
   corridas (DI-3, spread 0.014), exacto 99.02% / 99.09% contra 84.39% sobre la retícula.
   **NO se confirma** el miedo de que el default de producción fuera insensible al movimiento
   sub-píxel. **Queda abierto** un residuo de 5–13% de sobre-desplazamiento en contenido en
   movimiento (k debe llegar a 1.000): cuestión de precisión, ya no patología. **Consecuencia para
   R3: el corpus de M4 DEBE ser aperiódico** — los registros sobre retícula describen un modo de
   falla del matcher, no el comportamiento del núcleo.

4l. **S2.T2–T5 HECHOS (2026-09-04 — `planning/records/S2_T2..T5_GATE.md`)** — el instrumento de
   movimiento completo: zoo con verdad analítica `p(t)` y código de barras por cuadro; reproductor a
   60/120 fps con 0 ticks perdidos y el código de barras sobreviviendo TODA la cadena de captura (paso 1
   en 12/12 triples); extractor visto en rojo dos veces y corregido (búsqueda de fase sub-píxel en vez de
   la parábola, que se trababa a 0.188 px) con §4.1 a 0.045–0.060 px y §4.2 a 0.085 px; y el reporte
   con `r` por clase entre dos corridas. **M1 EXISTE POR PRIMERA VEZ CON SU FIABILIDAD**
   (`docs/evidence/MOTION_TRUTH_BASELINE.md`): default de producción, fondo aperiódico estático, 2×16
   triples — accel 0.646 px (r 0.76), circular 1.001 (r 0.95), crossing 1.213 (r 0.91), `fast` 5.160
   (r 1.00, el control esperado dispara); `linear` 0.620 a **r 0.47 — UNRELIABLE**, marcado por la
   propia herramienta. **El error cae con la fase: 1.668 px a t≈0.1 → 0.373 a t≈0.85** — es la firma
   del default single-track ("t=0+ paga el warp completo hacia atrás"), ahora medida. En tiempo, el
   cuadro generado está a 4–6 ms de donde dice estar contra un par de 16.7 ms. HUD sobre mundo en
   movimiento: 0.046 px (una corrida). También corregido: el triple es (N, N+1), no (N, N+2).

4m. **HALLAZGO M1 (2026-09-04 — `planning/records/M1_LOWPHASE_FINDING.md`)** — el operador preguntó si
   1.7 px a fase baja es aceptable. Descompuesto antes de contestar: el campo MV volcado es honesto
   (EPE 0.164 px en `linear`), `(1−t)·EPE` explica sólo 0.52 de 1.67, y el error tiene SIGNO hacia
   adelante a lo largo del movimiento (88–100 % de los marcadores hacia su posición de t=1, ~0.5 de
   `(1−t)·|v|`). Cuatro condiciones, dos corridas cada una: `--st-no-stasis` no cambia nada (1.693);
   `--no-mv-guided --mv-median` tampoco (1.493); **`--no-mv-guided` solo lo elimina: 1.668 → 0.271 px,
   plano en fase**. Causa: `shaders/mv_median.comp`, la mediana vectorial 3×3 de consenso, armada por
   defecto vía `mv_guided=true` (`present.cpp:820`, corre si `mv_median||mv_guided`): la tile de un
   objeto pequeño en movimiento ES el vector "disidente" y sus 8 vecinos estáticos lo votan a cero.
   El pase reescribe `wapMVA` en GPU DESPUÉS de la copia host → el `mv=` de `--qdump+` es PRE-pase y T6
   nunca lo vio (parte de su residuo). **Estándar:** A0 congeló M1 como COMPARATIVO (≤ 0.10 px contra
   el default) sin cota absoluta — tal como está, penalizaría el arreglo; la literatura no tiene cota
   posicional para FG; perceptualmente 2.5–3 arcmin media, 21–97 % del movimiento por cuadro, rango que
   la literatura de judder trata como visible. **El default NO se tocó.**

4n. **`mv1` Y EL RESIDUO DE T6 EXPLICADO (2026-09-04 — `S2_T6_GATE.md` §8, `S2_T1C_GATE.md` §6)** —
   `--qdump+` lee `wapMVA` DESPUÉS del pase de consenso (`mv1=`); `mv=` era el campo de ANTES. Alimentado
   con `mv1`, el oráculo pasa de k 0.730/0.735 a **0.891/0.867** por cuadro, y por banda: **1.000 en
   4–8 px, 0.957 en 2–4**, sobre el default de producción. El pase toca el 97.5 % de los téxeles y
   DUPLICA el EPE en las tiles de marcador (0.83 → 1.64 px), dejando 63 % del movimiento. **El criterio
   propio de T6 se cumple en ≥ 4 px.** El auditor exige `mv1` a cualquier push con el pase armado; los
   registros default anteriores auditan NOT REPLAYABLE, correctamente.

4o. **EL PASE SOBRE SU PROPIO CONTENIDO (2026-09-04 — `M1_LOWPHASE_FINDING.md` §6)** — `ball_zoo.ps1
   -BgClass flat`, disco de 260 px a 7 px/cuadro sobre campo uniforme, el contenido que `mv_median.comp`
   dice arreglar. El matcher emite 500–900 vectores espurios por cuadro ahí y **el pase no quita ninguno**
   (sellos 7,494 → 7,500), quita 5 % de los huecos y no toca el movimiento del disco: su premisa es un
   outlier AISLADO y el matcher produce cúmulos. A la salida: disco a 0.13 px con el pase, 0.06 sin él,
   0 % desgarro en ambos; oráculo exacto al byte a fase baja/media sin el pase. **Las dos mitades de la
   decisión existen: costo 1.4 px en objetos pequeños, beneficio nulo medido en su contenido objetivo.
   El default sigue intacto; el interruptor es del operador.** Hueco nuevo: el pase filtra también el
   campo hacia atrás y `--qdump+` sólo lee el de ida — `mvb1` pendiente.

4p. **T6 PASA (2026-09-04 — `S2_T6_GATE.md` §9)** — `mvb1=` (el campo hacia atrás post-pase; el pase lo
   reescribe tanto como el de ida, 97.6 % de téxeles en textura). Con AMBOS campos post-pase el oráculo
   es **exacto al byte en 12 de 16 triples por corrida** en la escena plana (máx 1 nivel, 0 px > 8,
   k = 1.000) y en el default texturizado con el store de producción llega a k 0.966 / 0.999 / 1.000 /
   1.000 por banda (0.5–1 / 1–2 / 2–4 / 4–8 px), exacto 99.88 / 99.86 % en dos corridas. Las tres cosas
   que estuvieron entre el oráculo y esto nunca fueron el oráculo: la retícula periódica, el campo de
   ida pre-pase, el campo hacia atrás pre-pase. **Regla final del corpus de M4: aperiódico, con `mv1` +
   `mvb1`, puntuado desde 1 px.** Residuo restante: sólo t≈0.87, ≤ 0.06 % del cuadro, registrado.

4q. **R3 CERRADO (2026-09-05, `records/R3_GATE.md`, gate PASSED (residual attributed))** — construido y medido el mismo día; lo que sigue describe lo construido: — el núcleo puro existe como código: `shaders/fg_core_math.glsl`
   (`fg_sample` / WEIGHT / `fg_blend`, matemática literal de `wap_warp.comp:497-522, 637, 679, 694`), doce cuerpos
   de fila en `shaders/layers/` (los ocho del default + `select` 260, `single_track_wa` 190, `fetch_mv` 0,
   `mv_edge_snap` 5), el kernel `shaders/fg_core.comp` (mismos 14 bindings que `wap_warp` + UBO en 15; push
   44 B = CorePush 20 + gme 24, desviación declarada de C §2), el instrumento `shaders/fg_ab_diff.comp`
   (`--fg-core-ab`: ambos kernels por tick sobre las MISMAS entradas, cuenta píxeles distintos), el generador con
   etapa WEIGHT + canal CH_BLEND + regla `needs` declarada + chequeo `overrides`/`c_in`, y el parche del host
   (`r3_patch.py` en el scratchpad: ABI, tabla, .def, CMake con depfile XR13, factorías, propiedad, init,
   present, CLI, registro, auditor, docs). Decisiones tomadas por la sesión bajo la delegación del operador
   ("investígalos cuando los necesites"): el default del pase de consenso NO está en la ruta de R3 (vive en la
   etapa 3; R5 lo hace fila) → diferido a R5, sigue siendo suyo; A0/M1 no compuerta R3 (compuerta R7) → diferido;
   R3 reproduce el default bug-por-bug (XR7) y su envolvente de identidad es el set default con
   `single_track` ON (bajo `--no-single-track` `select` reproduce solo la ruta dura; soft_gate/commit_default/
   multicand no son filas). Tres de las cuatro `shadows` de single_track están MUERTAS bajo el override
   (:692/:1089/:810/:1267 escriben acumuladores que :1321 descarta — verificado leyendo el shader) y no se
   reproducen; la cuarta es la fila WEIGHT. `warp_light` (governor) apaga vblend por tick en el legado y no puede
   apagar una constante de especialización: los ticks light no se comparan (desviación declarada).
   **Medido (2026-09-05):** con la contracción FMA permitida (la build de producto) los dos kernels difieren en
   ~7×10⁻⁹ de los píxeles, todos de exactamente 1 nivel y todos dentro de la banda de la pelota; con
   `NoContraction` en AMBOS módulos (`PFG_NOCONTRACT=ON`, dentro de las reglas glslc) son **byte-idénticos en cada
   uno de 2,382 ticks comparados** — el residuo ES la contracción del compilador del driver, atribuido por
   experimento (§4 del registro). La build de producto se restauró y se verificó idéntica por md5. Un revisor
   Sonnet (14 agentes) no halló desviación que toque la salida por defecto; sí una trampa latente en
   `layer_arm_mask` (sin rama `COMMIT`), corregida. El arnés completo (grid/noise/pan ×2, control ROJO
   `--no-single-track`, sim limpia, M3 2/lado) y el corpus del oráculo (`--fg-core --qdump`, noise 720p ×2) están
   en §5–§7 del registro.
4r. **R4 CERRADO (2026-09-05, `records/R4_GATE.md`, gate PASSED; el punto TDR cerrado por las tres corridas del operador el 2026-09-06 — R4c)** — la etapa 6 PRESENT es
   `src/present/present_stage.{hpp,cpp}`: la superficie (creada en el hilo P), la contabilidad de submits, las dos
   ranuras del puente, el preámbulo asíncrono (UN cuerpo para las dos copias que había), `begin(decision)` con la
   decisión `{Warp, Dup, Drop}` como ENTRADA (Drop devuelto cuando hay warp en vuelo; Decimated = la ausencia de
   llamada, dicha en la compuerta), `submit`, `--shallow-queue`, `present_tick`, y `FlipStats` consumido por la
   fila CSV (el borde 6→4 existe como llamada). Cuerpos EXTRAÍDOS por ancla del archivo real, no transcritos:
   83.95 % byte-idéntico, las 13 diferencias son los renombres declarados. El lazo conserva sus nombres como
   alias a los campos de la etapa. `--tdr-test N` CONSTRUIDO y no ejecutado: es L3 sobre su GPU interactiva
   (SAFETY §5) — requiere su palabra. Residuo declarado: las estadísticas por segundo siguen en el lazo; el drop
   de banda de guarda (S8) no se añadió (no existe equivalente legado; es una decisión medida, no una extracción).
4s. **R4b — LOS INSTRUMENTOS DEL HALLAZGO Y SU MECANISMO (2026-09-05, `records/R4_GATE.md` §4.6, registro XR15,
   17 corridas)** — tres adiciones al plano INSTRUMENT, ninguna toca un píxel: el guardián de `--exit-after` izado
   al límite del tick (el modo retícula ya termina solo), la columna `phyriadfg_fresh` por tick + `fresh_count` +
   `fresh:N/s`, y `--warp-timing` (timestamps de GPU alrededor del lote del warp + latencia submit→fence).
   **Medido (DI-3, 60 s):** asíncrono (default) 49.9 % frescos = 119.3/s de 240 (spread 0.04), GPU p50 0.107 ms,
   el fence se ve al SEGUNDO sondeo (9.1 ms); síncrono (`--no-async-present`) 100 % = 239.3/s, GPU 0.079 ms,
   fence a 4.03 ms constante, +0.77 ms de MsAddedLatency, hilo P ocupado ~4 de cada 4.17 ms. **El lote PUEDE
   completarse en 0.3–0.6 ms** (el giro de `--shallow-queue` a 4000 µs lo ve así en su meseta) — la pérdida es
   una ESPERA, no trabajo. La cola superficial es biestable (100 % ↔ 62 %) con un ciclo de 16.3 s que es del
   propio lazo: los presents van clavados al panel (239.755 Hz), el reloj corre a 240.000 → el slip crece 1.02
   ms/s y la rejilla se re-asienta a los 4 períodos (`present.cpp:1819`); la prueba del batido (zoo a 61 y 59
   fps, mismo período) descarta la fuente. Mecanismo, cuatro predicciones cumplidas: el acquire del keyed mutex
   del lote VK espera al `CopyResource` del present anterior, que el swapchain flip de dos búferes ejecuta detrás
   del flip anterior. Prueba con `--present-waitable` (botón existente, apagado por defecto): 99.85 / 99.84 % frescos en el camino asíncrono (14,362 / 14,360 de 14,383), fence al primer sondeo, 0.25 ms p50 con giro, MsAddedLatency 21.21 vs 20.86 del default — la espera ES la cadena; la opción (4) es ese botón, pendiente de un par DI-3 bajo `gpu_load` y con contenido real. La
   decisión de política de present (aceptar 120/s, pasar el default a síncrono, o quitar la espera) es del
   operador — un default de producto con historia visible.
4t. **4.2 CERRADO — LA MITAD DE PAPEL DE R5 (2026-09-06, `docs/planning/aap/FLOW_ROW_MAP.md`)** — la familia de
   holones de `flow.cpp` (fuente de flujo, mv_smooth, ambig, bidir, persistencia, gme fwd/bwd, memoria
   fwd/bwd/refresh, objetos fwd/bwd) más el pase de consenso de MV que hoy corre en P (`present.cpp:734–765`)
   mapeados sobre el esquema LAYERTAB como trece filas FLOW: **0 columnas nuevas** (1 valor de Kind `H` = pase
   en host; 3 valores de arm `HAS_PREV`/`PRESSURE_LT4`/`PRESSURE_LT5` + 4 campos de `ArmInputs`; 7 bits de
   canal; 1 relajación de invariante: `--inertia` arma dos filas). Un candidato a columna con nombre (la
   lectura temporal gen−1, `reads_prev_ch`), juzgado innecesario y dicho. Cinco cosas NO son filas: el publish
   (el anillo), `wap_upload` (transporte 3→5), `fwd_pipeline` (modo de la etapa leído por el arm), el gobernador
   (CONTROL), nvofa (parámetro proveedor). Decisión por la regla A3 §4.2: PROCEDER a R5. El inventario lo hizo un
   agente Sonnet (solo lectura); catorce citas re-leídas de primera mano antes de mapear. El residuo de XR3
   queda descargado. Sin código.
4u. **R5 CERRADO Y G-R5 APROBADA (2026-09-06, `records/R5_GATE.md`)** — pasos 3c y 4 con su palabra. 3c:
   `consume_wap` (el orquestador de la etapa, 444 líneas) extraído por ancla a `flow/flow_consume.{hpp,cpp}`,
   339/339 líneas idénticas; las 61 capturas del contexto re-enlazadas con las MISMAS líneas de alias, 30
   referencias de estado y 5 invocables tipados — los sitios de llamada del cuerpo intactos. Destapó 32 alias
   muertos en `run_flow` (el compilador nombró cada uno): `flow.cpp` compila con CERO advertencias y pasó de 2,235
   a 1,429 líneas. 4: el transporte 3→5 lo gobiernan las filas (cada subida por el ON efectivo de su fila
   productora, la condición manual al lado como segundo oráculo) y el registro RESPONDE la pregunta del plan con
   una medida: en el set por defecto la CPU es autora de `mv_raw_fwd,persist,mv_bwd,dissidence` y NO de
   `sad,candidates,mv_target` (más `prev/cur`, que son de la etapa 2) — tres de nueve canales podrían compartirse
   del lado del dispositivo, y eso sigue siendo un diseño de ruta de datos, ahora con número. **G-R5:** A/B de dos
   corridas por lado contra el binario pre-R5 (`3bf654b`, reconstruido para ello) con todos los deltas por debajo
   de su propia dispersión; y la corrida bajo presión que faltaba — con el arbitrador de `gpu_oc` saturando la 4090
   (96–100 %, 355 W, frente al 32–35 % de `gpu_load.exe`) y la fuente a 120 fps, el gobernador enganchó `tier:4` y
   `tier:5`, `bwd-skip:96%`, y ambos instrumentos siguieron en 0 discrepancias (34,171 + 28,146 decisiones).
   Aprendizaje registrado: saturar la GPU NO mueve la escalera de tiers — la mueve el presupuesto por par (la tasa
   de la fuente); el instrumento de doble oráculo sigue en el código a propósito (retirarlo es de R7).

4v. **PROTOCOLO DE METACOGNICIÓN (2026-09-06, directiva del operador)** — `protocols/core/METACOGNITION_PROTOCOL.md`
   y su `LEARNING_LEDGER.md`, cableados en el arranque, el conjunto durable, `DURABLE_CORE` y `CLAUDE.md`. CONDUCT
   verifica una AFIRMACIÓN; este verifica lo que DIRIGE: un marco, una premisa de plan, una regla heredada. Cinco
   disparadores obligan una entrada; un marco nunca se rechaza por su linaje (la regla del operador sobre lo
   holónico se conserva), se le pide re-ganar el derecho a dirigir; el sustantivo colectivo sobre un marco es la
   forma de mayor riesgo. Entradas fundacionales L-001 (lo holónico), L-002 (la captura de aprendizaje murió por un
   acoplamiento a git), L-003 (la mudanza `G:`→`F:` barrió los documentos, no los scripts). PhyriadFG lleva su
   propio `docs/LEARNING_LOG.md` (8 entradas).
4w. **4.3 CERRADO — LAS PRUEBAS ESTÁN CABLEADAS Y CADA UNA SE VIO EN ROJO (2026-09-06,
   `records/S4_3_GATE.md`)** — 43 pruebas, ~1.5 s, sin GPU: las 148 comprobaciones de la costura (R2), el oráculo
   de bit-paridad del reloj (R1) sobre un registro grabado de 2,877 ticks (`tests/clock/fixtures/`), el corpus de
   paridad del registro (34 combinaciones de tokens), el hash de contrato fijado, los códigos de salida del CLI y
   dos pruebas negativas. `build-release.bat` las corre y falla si hay rojo; `tools/run_tests.bat` es la entrada
   suelta. **Dos falsos verdes destapados y corregidos:** la prueba del reloj imprimía "all checks passed (0)" sin
   haber reproducido un solo tick, y con un registro ilegible imprimía SKIP y salía 0; y **un error de parseo salía
   con 0** — una bandera mal escrita corría con la configuración POR DEFECTO y reportaba éxito, cosa que todos los
   arneses de `tools/` habrían creído (ahora sale 2). Cada compuerta se rompió a propósito para verla roja; el
   primer intento fue demasiado sutil para cambiar nada y se conserva en el registro, porque un verde tras una
   perturbación casi siempre significa que la perturbación fue invisible, no que la compuerta esté ciega.
4x. **R6 CERRADO Y G-R6 APROBADA (2026-09-06, `records/R6_GATE.md`)** — tres pasos. (1) La etapa 2 es su propio
   módulo: la cola de convert + publish y el worker de `--ingest-async` salieron de `run_capture` a `src/ingest/`
   por ancla (225/225 líneas idénticas; la región se verificó balanceada en llaves ANTES de moverla; 32 capturas
   re-enlazadas con las mismas líneas de alias). Después el compilador nombró 25 alias muertos: `capture.cpp` pasó
   de 972 a 683 líneas y compila sin advertencias. Hallazgo: la lógica de convert existe DOS veces (cola serial y
   worker) — no se deduplicó, eso es comportamiento. (2) Los dos anillos de ingesta, y sobre todo **un solo
   `publish()` que posee el orden** que R6 no puede cambiar (sello antes del `fetch_add`), antes escrito a mano en
   dos sitios. Las vistas `RawFrame`/`RealFrame` que el plan nombra se escribieron y se BORRARON antes de
   confirmar: los ocho lectores direccionan por ranura arbitraria, no por secuencia, así que serían una envoltura
   sin consumidor (regla 1). (3) Los nombres de directorio que el operador adoptó: `warp_blend/`→`generate/`,
   `cli/`+`layers/`→`control/`. Trampa evitada: `src/layers/` y `shaders/layers/` son cosas distintas; el reescritor
   se ancló en `layers/layer_` y verifica que el include de los cuerpos sobreviva. Regla aplicada a los documentos:
   el que DESCRIBE el sistema actual se actualiza; el que REGISTRA lo hecho conserva sus rutas (por eso la foto
   mental y `aap/` se revirtieron). Compuerta: tasas idénticas al binario pre-R6 dentro del ruido, arranque sin
   cambios, humo de 120 s limpio, 45 pruebas verdes.
4y. **R7 (la mitad de código) HECHO 2026-09-06 — `records/R7_GATE.md`.** R7 tiene dos mitades y no son la misma
   clase de cosa. (a) `--legacy-warp` fuera del default sigue **BLOQUEADO**, y ahora con el bloqueo nombrado: la
   tabla M1 tiene que existir en las DOS rutas y MOTION_TRUTH T2–T5 son `designed` con cero código; ningún proxy
   cierra una compuerta M1. Además es un default de producto — suyo. (b) Los residuos de código que R5 y R6
   dejaron nombrados, cerrados: **el instrumento de dos oráculos retirado** (la condición de R5 era "una segunda
   corrida bajo presión con 0"; se corrió dos veces más, la segunda CON `--fwd-pipeline` — la combinación que R5
   declaró nunca probada y el único productor del arm-input `pipelined` — 107,867 decisiones, 0 desacuerdos);
   **el convert unificado** (los dos copias eran idénticas salvo dos cosas, medidas antes de tocar nada);
   **el RawRing con sus reglas** (el publish bajo el lock y el drop-to-newest, escritos a mano en tres sitios).
   Lo que NO se aceptó: cerrar el oráculo apoyándose en las corridas. Las dos reportan `bwd-skip:100%`, que es
   exactamente cuando los cuatro sitios dentro de `if(do_bwd)` dejan de ejecutarse — la corrida que certifica el
   shedding es la que no puede ejercitar sus vecinos (P-016). El conocimiento se mudó: `pfg_arm_test` enumera los
   11 sitios sobre TODAS las combinaciones de los arm-inputs (8,209 checks; visto en rojo tres veces, dos veces:
   la segunda tras ensanchar el test). `stats_second()` se MIDIÓ y se dejó abierto con el número al lado (60
   referencias compartidas — el doble de lo que necesitó `consume_wap`) y con el método que sí lo cerraría.

5. **NEXT** — R0–R6 cerrados y aprobados; 4.2, 4.3 y la mitad de código de R7 cerrados. Lo que queda:
   (a) **R7(a), del operador y bloqueado por medición**: `--legacy-warp` fuera del default necesita la tabla M1 en
   la ruta `--fg-core` también, y eso significa construir MOTION_TRUTH T2–T5 (`records/BACKLOG_AUDIT.md` A6,
   "large"). Es el único camino: ningún proxy cierra M1.
   (b) **`stats_second()`**: abierto, medido, método especificado (`R7_GATE.md` §6). No es una mudanza sino un
   paso de diseño del plano INSTRUMENT: los ~25 acumuladores por ventana deben ser de un tipo que los posea.
   (c) Sin residuos ocultos: la lista que esta foto traía (oráculos, convert duplicado, RawRing) está cerrada.
   No re-derivar: R3 (4q), R4 (4r), R4b (4s), 4.2 (4t), R5 (4u), metacognición (4v), 4.3 (4w), R6 (4x), R7 (4y).

**Auto-prompt (post-compactación):** soy la sesión de PhyriadFG que acaba de cerrar la mitad de código de R7
(`records/R7_GATE.md`): el instrumento de dos oráculos retirado con su reemplazo estático (`pfg_arm_test`), el
convert unificado y el RawRing con sus reglas. Releer primero CONDUCT, esta foto (4y + NEXT) y
`records/R7_GATE.md` §0 — que dice qué mitad de R7 NO se cerró y por qué. Luego `git status`: si el árbol está
limpio en `analysis/0.3.0-quality-push`, el trabajo está confirmado (push NO autorizado, sin upstream). Lo
siguiente NO es "seguir con R7": R7(a) es del operador y está bloqueado por una medición que no existe (M1 en la
ruta `--fg-core` → MOTION_TRUTH T2–T5, sin código). Reportarle con números y la línea de evidencia; no pedirle
decisiones que ya delegó.

6. **CONSTRAINTS in play** — child projects relocated by the operator only; never delete invested work
   (the donor stays behind `--legacy-*`); byte-identical-off on every new path; M4 is a veto with T6 as
   the oracle; M1 gates are blocked until MOTION_TRUTH T4–T5 pass — no proxy closes them; commit/push/PR
   only on request; heavy-compute pre-flight; DI-3.

## Self-prompt (read this first after a compaction)

I am the Phyriad session working PhyriadFG for the operator (usted, Spanish chat, English docs). The
objective is P in `docs/planning/ACTION_PLAN.md`; the position is S4 (CONVERGENCE), executing in
place. Do NOT re-open the layer-contract search (A0 is frozen; A3 = LAYERTAB + G1–G5; AT3/AT4/ATF
approved in `docs/planning/aap/AT3_AT4_ATF_VERDICTS.md`) and do NOT re-derive the E1 result or the
instrument-chain verification.

**Do NOT re-derive any of this — it is BUILT and GATED, each with a record in
`docs/planning/records/`:** R0 (the layer registry + `--layer-dump`), R1 (`PhaseClock` extracted, the
4× multiplication measured at 0.2501 source frames per present), R2 (the seam engine deriving stage
5's barriers, opt-in behind `--sg-barriers`), T0 (the scorer port), T1 (`--qdump+` sidecars), T1b (the
coverage sampler), T1c (the replay record's completeness audit). An earlier version of this very
paragraph said "Nothing under CONVERGENCE or MOTION_TRUTH is built" — that was false and cost is the
reason this warning is here.

**The live work:** S2.T6's sub-pixel deadzone (`records/S2_T6_GATE.md`). `tools/ref_warp.py` is built
and its gate does NOT pass. Six causes are refuted by number; do not re-test them.

Re-read first: `F:\Phyriad\protocols\core\BOOT_PROTOCOL.md`, `docs/planning/ACTION_PLAN.md`
(pointer, line 14), `docs/planning/records/BACKLOG_AUDIT.md` (what is left, audited 2026-09-04), and
the memories `phyriadfg-convergence-spine.md` + `phyriadfg-m4-oracle.md`.
Report every number from output, never from memory; report the not-run.

*Made with my soul - Swately <3*
