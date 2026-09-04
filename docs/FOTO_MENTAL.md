# FOTO MENTAL — PhyriadFG (the live-thread snapshot; MENTAL_SNAPSHOT_PROTOCOL)

> The ephemeral re-orientation for the NEXT session/compaction. The durable WHERE is
> [`planning/ACTION_PLAN.md`](planning/ACTION_PLAN.md); the durable knowledge is the D-22 memory
> (`phyriadfg-*.md`). Rewritten at every checkpoint; older snapshots are not kept (the spine is).

**Taken:** 2026-09-04 (backlog audit; supersedes the 2026-09-03 stamp). Sections 4b-4j are the
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

5. **NEXT** — (a) el **residuo de S2.T6**: k = 0.956 sobre contenido en movimiento, la compuerta pide 1.000. Arriba de 4 px la referencia ya
   acierta (k = 0.955); abajo de 0.5 px el shader no mueve nada y la referencia sí. M4 no debe correr
   sobre este oráculo antes, porque un oráculo con ese error lavaría justo el defecto que M4 existe
   para detectar. La pista: el patrón de periodo 3 (−0.5, +0.1666, 0) sobre bloques cuyo `sad_best`
   es 0. (b) **Cobertura alternativa, desbloqueada hoy:** construir **S2.T2** (el zoo de marcadores NO
   periódico) — es la única cosa del repo que puede responder la pregunta que el propio registro de T6
   deja abierta: si la zona muerta existe fuera de la retícula que produce el patrón. Está además en la
   ruta crítica de M1 de todos modos. (c) El inventario completo de lo que falta está en
   `planning/records/BACKLOG_AUDIT.md` (auditoría 2026-09-04). (b) Cambiar el default a `--sg-barriers` es una decisión aparte: pide un
   soak largo y el ojo del operador sobre un juego real, no solo `ball_zoo`. (c) R3 lleva las dos
   restricciones del experimento de columnas: etapa `WEIGHT` con el núcleo partido en
   `fg_sample`/`fg_blend`, y el canal `CH_BLEND` con `select` como fila.
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
