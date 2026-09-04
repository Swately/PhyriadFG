# FOTO MENTAL — PhyriadFG (the live-thread snapshot; MENTAL_SNAPSHOT_PROTOCOL)

> The ephemeral re-orientation for the NEXT session/compaction. The durable WHERE is
> [`planning/ACTION_PLAN.md`](planning/ACTION_PLAN.md); the durable knowledge is the D-22 memory
> (`phyriadfg-*.md`). Rewritten at every checkpoint; older snapshots are not kept (the spine is).

**Taken:** 2026-09-03 (after the ATF release and the CONVERGENCE triad).

0. **ACTION-PLAN POINTER** — `docs/planning/ACTION_PLAN.md` → **P → S4 → S4.0 (operator) ‖ S4.C0 (next)**.
1. **OBJECTIVE** — P: perfect PhyriadFG as ONE final FG = the clean minimal core (`apps/minimal_fg`, the
   SG seam) + the layers that earn their place, on the LAYERTAB contract; exact motion measured as DATA
   (S2 MOTION_TRUTH), perceptual quality parked (S5).
2. **FOCUS** — the planning is complete and released; the next action is EXECUTION, which starts only on
   the operator's word: S4.0 (adopt the base — operator-only relocation) and/or the donor track S4.C0.
3. **METHOD** — Phyriad protocols: CONDUCT (verify-before-claim, calibrated, zero praise), PLAN_TIER T2
   (no commit with an `open` risk), AAP (closed: A0 frozen sha256 `670687d0…6933`, 3 candidates, 9
   scorecards, ATG 2 lenses, A3 = C + G1–G5, AT3/AT4/ATF approved — `docs/planning/aap/`), DI-3 two runs
   per number, SUBAGENT_DELEGATION (subordinate output = a claim; every judge citation was spot-verified
   first-hand), ACTION_PLAN_PROTOCOL (pointer + node states). Chat Spanish/usted; docs English; signature
   on every touched code file. Workflow tool is authorized for gate fan-outs (ultracode on).
4. **WHERE WE ARE** — built and verified this arc: E1 thin `main()` (G1 passed, 98.37 % verbatim, 240
   presents/s baseline); the instrument chain proven (`fg_quality_scorer` builds; qdump → scorer T; zoo →
   scorer A). Written, NOT built: MOTION_TRUTH triad (T0–T6), CONVERGENCE triad (C0–C6, XR1..XR13),
   the AAP record. Verified defects to honour: bg-reclaim dead under the default (`wap_warp.comp:344/394/410`);
   `--qdump` inert under `async_present=true`; the base's `img_barrier()` is `ALL_COMMANDS`
   (`apps/minimal_fg/src/main.cpp:159–167`) and re-uploads both anchors every tick (:1301–1330).
   **Uncommitted:** the whole working tree (`git status`: E1 sources, `.bat` fixes, `.gitignore`, `cli.hpp`,
   every `docs/planning/*` file) — commit/push is the operator's call and has NOT been asked for.
   `tools/gate_zoo.ps1` is the operator's own untracked tool (2026-06-12), untouched.
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
5. **NEXT** — (a) **S2.T0/T1** con la luz verde: el port del scorer y los sidecars `--qdump+`, que la
   compuerta M4 de R3 EXIGE. (b) Cambiar el default a `--sg-barriers` es una decisión aparte: pide un
   soak largo y el ojo del operador sobre un juego real, no solo `ball_zoo`. (c) R3 lleva las dos
   restricciones del experimento de columnas: etapa `WEIGHT` con el núcleo partido en
   `fg_sample`/`fg_blend`, y el canal `CH_BLEND` con `select` como fila.
6. **CONSTRAINTS in play** — child projects relocated by the operator only; never delete invested work
   (the donor stays behind `--legacy-*`); byte-identical-off on every new path; M4 is a veto with T6 as
   the oracle; M1 gates are blocked until MOTION_TRUTH T4–T5 pass — no proxy closes them; commit/push/PR
   only on request; heavy-compute pre-flight; DI-3.

## Self-prompt (read this first after a compaction)

I am the Phyriad session working PhyriadFG for the operator (usted, Spanish chat, English docs). The
objective is P in `docs/planning/ACTION_PLAN.md`; the position is S4 (CONVERGENCE), design released
2026-09-03 — do NOT re-open the layer-contract search (A0 is frozen; A3 = LAYERTAB + G1–G5; AT3/AT4/ATF
approved in `docs/planning/aap/AT3_AT4_ATF_VERDICTS.md`) and do NOT re-derive the E1 result or the
instrument-chain verification. Re-read first: `F:\Phyriad\protocols\core\BOOT_PROTOCOL.md`,
`docs/planning/ACTION_PLAN.md` (pointer), `docs/planning/CONVERGENCE_MASTER_PLAN.md` §2–§3, and the memory
`phyriadfg-convergence-spine.md`. Nothing under CONVERGENCE or MOTION_TRUTH is built. The next concrete
action depends on the operator: S4.0 (adoption, his relocation) or a green light on S4.C0 / S2.T0–T1;
if green-lit, start at strategy X1 and run gate G-C0 with two runs per number. Report every number from
output, never from memory; report the not-run.

*Made with my soul - Swately <3*
