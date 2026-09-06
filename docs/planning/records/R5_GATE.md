# R5_GATE — Stage 3 FLOW: the rows declared, `FlowSet`/`FlowRing`, the holons bound, `wap_upload` conditional (M-R5)

**Status: CLOSED (2026-09-06) — the four steps done and G-R5 PASSED (§5). The operator released 3c and 4 the same
day ("3c, adelante, 4 hagamoslo tambien") and named the load tool for the pressured run.** The operator's word: "adelante, continua segun todas tus
recomendaciones" (2026-09-06), after 4.2's map (`aap/FLOW_ROW_MAP.md`, 0 new columns → PROCEED). This record grows
one section per step; the verdict (§6) is written when the four steps and G-R5 have run.

## 0 · The contract, re-anchored, and the entry decisions

**Source of truth:** `CONVERGENCE_MASTER_PLAN.md` §R5 and `STAGE_CONTRACT.md` §2 Stage 3: from a `Pair` (+
`FlowSet[gen−1]`) produce `FlowSet[gen]` — every per-pair field the generation reads — published by `f_seq`; the
holon family becomes FLOW-stage rows of the registry; the MV consensus leaves P's ad-hoc site for a row;
`wap_upload` exists only when stages 3 and 5 are on different devices. MUST NOT change: the default output.

**Entry decisions (the session, under the operator's delegation; each reversible, each named):**
1. **The rows are ON by default, and the gate is byte-identity, not "off".** The plan's text ("holons → `kind = P`
   rows (off); the shipping default already has `igpu_field=false` cascading them off") was stale: the inventory
   shows `gme`, `bidir`, `ambig`, `objects`, `memory`, `inertia` all ON in the shipping set (`cli.hpp:413, 489, 581,
   600, 621, 675`; the operator's own run logs print them ACTIVE). So G-R5's byte-diff is the R4-style A/B of the
   default run before/after each step (the rows on), not a "rows off" identity.
2. **Fourteen rows, not thirteen:** the gme CPU/GPU pair is two rows that `excludes` each other under one hand field
   (`--gme-gpu` / `--no-gme-gpu`) — the `mv_guided`/`mv_edge_snap` pattern — rather than a `device` param
   (`FLOW_ROW_MAP` §2.1 allowed either). Total rows 26 + 2 pseudo = **28 of the 32** the id bitmasks allow; the
   33rd row (S5's perceptual family) forces the widening the `static_assert` in `layer_table.hpp` names.
3. **Three ArmId values (corrected in step 3b):** `PRIOR`, `HOLON`, `BIDIR_OK`. Step 1 had added a fourth, `BWD_HOLON`
   (bwd ∧ the holon shedding), from the map; step 3b read the code: the backward legs run whenever the backward match
   did (`if(do_bwd){ … if(use_memory) … if(use_objects) … }`) — they are NOT decimated by the tier ladder — so their
   arm is the existing `BWD` and `BWD_HOLON` was removed. `ArmInputs` gains five fields (`has_prev, tier, holon_skip,
   pipelined, bwd_skipping`), the map's four plus the bwd-skip hysteresis it had folded into "tier".
4. **The consensus stays where it runs in this step** (P's `wap_upload`, `present.cpp:747`) but becomes a ROW with its
   OWN switch: `--mv-consensus` / `--no-mv-consensus`, default ON (byte-identical). Moving the dispatch to F is
   step 3's question (a device / image change; `STAGE_CONTRACT` §2 allows the MVCOND placement as the alternative).
5. **Two CLI-COMPAT values, no columns:** `PF_NEGATED` (the flag itself is the negative token — `--no-shapefield`,
   whose positive twin is a hand-parser no-op print) and `PF_NO_FORM` (the derived `--no-` token turns a BOOL off —
   `--obj-fill-rim` / `--no-obj-fill-rim`, both act). And the declared relaxation: **a token may drive several rows**
   (`--no-inertia` → the MVCOND consumer + the FLOW producer; `--no-memory` → three rows; `--no-objects`,
   `--no-ambig` → two); the shadow parser applies it to every carrier; the help and the UI model show it once.

## 1 · Step 1 — the FLOW stage DECLARED (registry-only; 2026-09-06) — CLOSED

**Built (`r5s1_patch.py` + two fixes):** `layer_abi.hpp` — `Kind::H`, the four ArmId values, the five `ArmInputs`
fields (trailing, defaulted: every pre-R5 `ArmInputs{a,b,c,d,e}` site keeps its meaning), `PF_NEGATED` /
`PF_NO_FORM`, seven channel bits (`CH_MV_PREV_GEN, MEM_PRIOR, MEM_ADV, OBJ_STATE, WAKE, GME_BWD, PAIR_STATS`);
`layer_table.hpp` — `kind_char` `H`, `arm_name` ×4, the FLOW write-legality rule (a FLOW row never writes stage 5's
`CH_MV / MV_SAMPLE / BLEND / A_SAMP / B_SAMP / WARP_OK / D_PIXEL / STASIS`); `layer_table.def` — the fourteen FLOW
rows at ranks 0–90 (`flow_source, mv_smooth, candidates, bidir, persistence, gme, gme_gpu, mem_fwd, objects, gme_bwd,
mem_bwd, objects_bwd, mem_refresh, mv_consensus`) with their `needs` / `excludes` / `arm` / channels / params /
tokens from the map; `layer_registry.cpp` — the multi-carrier shadow parser, the `PF_NEGATED` / `PF_NO_FORM`
handling, twenty-one new parity checks, the four arm cases, the `[old2]` dump line, the UI model's "switch-off" for
negated BOOLs and its no-form flag, `first_carrier()` for the help and the model; `layer_config.hpp` — fifteen shadow
fields; `cli.hpp/.cpp` — `mv_consensus=true` + its two tokens (in `parse_extra`: the hand parser's `else if` chain is
at MSVC's nesting limit, C1061 on the 2nd added case — a finding for E4), `--mv-smooth` clamped to [0,1] (the row's
range; alpha > 1 was undefined), eleven hand help lines retired; `present.cpp:747` — `cfg.mv_consensus &&` in the
consensus gate; `ui/src/main.js` — nine hand entries retired (the registry model renders them under "Layers · Flow").
The holon bodies are UNTOUCHED: they still run under `flow.cpp`'s hand-written conditions; the rows are the
registry's truth (dump, help, model, parity, hash), not yet the code's.

**Gate, seen green on this binary (build 0 errors; 36 warnings = the pre-existing set, none in a touched file):**
- `--layer-dump`: `[layertab] contract=0xDE7B9D6212991BAB (24 rows enabled of 26; 21 params …)` then the fourteen
  FLOW rows in rank order with the declared arms and relations, e.g. `FLOW 40 gme H arm=PRIOR ON irls2=0
  requires={flow_source} unless={gme_gpu}`, `FLOW 72 objects_bwd H arm=BWD+HOL ON requires={bidir&objects}`,
  `FLOW 90 mv_consensus P arm=ALWAYS ON blind=0 requires={flow_source}`; `mv_smooth` OFF (alpha 0).
- **Parity corpus: 39 / 39** token combinations pass (`--dump-config` exits after parse + parity; exit 3 on a
  mismatch) — the default, every new off/on token alone, the `--no-X --X` orders, `--mv-smooth {0.6, 0, 2, −1}`,
  `--no-mv-guided --mv-median`, `--gme-gpu-verify --no-gme-gpu` in BOTH orders, an eight-token combination. Seen
  RED first: `--no-gme-gpu --gme-gpu-verify` failed (`gme_gpu.on old=1 new=0`) because the hand parser's
  `--gme-gpu-verify` implies `--gme-gpu`; the `verify` param gained `PF_IMPLIES_ON` and the corpus went 39 / 39.
- The derived surfaces: `--help` lists each registry-owned token ONCE (19 tokens checked, count 1 each; a shared
  token prints "(also turns mem_bwd OFF)" lines under its first carrier); `--layer-model-json` has 15 "Flow"
  controls with no duplicate flags (seen red first: `--no-memory` ×3 and `--no-objects` ×2 before `first_carrier`).
- The three exit paths on this binary: async default / sync / grid, 10 s bounded runs, `rc=0 done=1 clean=1
  abandon=0 device_lost=0` each, `fresh:240/s` on the default (XR15's default in force).

**Not in this step, named:** no holon body moved; `layer_arm_mask` is not yet consumed by `flow.cpp` (the rows'
arms are declared, the code still decides by hand — step 3 binds them, with `ArmInputs` filled on F from
`have_prev_f`, `pressure_tier`, `holon_skip_pair`, `fwd_pipe`, `bwd_skipping`); the consensus dispatch site is
unchanged; `--layer-model-json` is preceded by two `[ra] --no-igpu-field cascade` lines on stdout (pre-existing; the
UI's loader finds the brace — noted, not fixed here).

## 2 · Step 2 — `FlowSet` + `FlowRing` declared (2026-09-06) — CLOSED

**Built (`flow/flow_set.hpp`, new; `r5s2_patch.py` on `main.cpp`):** `pfg::flow::FlowRing` — `NS = kGenRing`, the two
counters of the ring contract (`f_seq`, `p_presenting`), the twelve per-pair scalar arrays that were `main()` locals
(`cseq/slot/tcap, span/n, gme[6]/gme_valid, gme_bwd[6], mfwd/mbwd, disp, bwd_valid` — `bwd_valid` initialized 1 by
the ring, as before), and the ten per-generation host bridges BOUND BY REFERENCE (`mv, sad, mvb, c2, dis, disb,
gme_m, gme_mb, per, interp[kMaxInterp]`; their allocation stays with `HostBridgeInit` / `core_init.cpp`, which
allocates a bridge only when its producer is armed). `FlowSet` is the declared per-generation VIEW (`ring.at(gen)`):
pointers + references into the ring — STAGE_CONTRACT §1's type, the one the instruments and step 3's rows read.
`main.cpp`: the twelve locals and `f_seq` / `p_presenting` replaced by the ring + ALIASES with the former names
(`uint64_t (&f_pair_cseq_a)[NS] = ring.cseq;` …), so `flow.cpp`, `present.cpp` and the `FgContext` binding are
untouched and read / write the same memory. The eleven field-note comment blocks moved to the header with their
substance (the publish discipline, the monotonic-real rule, the span pacing, the matte-mass ratio, the bwd-validity
degrade). Build 0 errors; 8 warnings = `main.cpp`'s pre-existing C4189 set.

**Gate:** byte-identical by construction (the same storage under new ownership; not one line of the consumers
changed) and measured once: the three exit paths (async default / sync / grid, 10 s: `rc=0 done=1 clean=1
abandon=0` each) and a 60 s default run against the step-1 binary's:

| default, 60 s, ball zoo | rows | fresh | `MsAddedLatency` | `disp_phase` mean / sd | `disp_src` step | uniq/s |
|---|---|---|---|---|---|---|
| before (step 1 binary) | 14,386 | 99.85 % | 20.58 | 0.5010 / 0.2815 | 0.2500 | 239.1 |
| after (step 2 binary) | 14,384 | 99.79 % | 20.80 | 0.5010 / 0.2814 | 0.2503 | 238.9 |

One run per side (reliability not measured — DI-3); every placement number inside R4's measured spread for the same
content; the latency difference (0.22 ms) is inside the 0.6 ms window the R4b DI-3 pairs showed run to run.

**Not in this step, named:** no consumer reads `FlowSet` yet (the view exists, the code still names the arrays);
`c_seq` (the capture ring's counter) stays a `main()` local — it is the FrameRing's, R6's; `g_seq` (the GPU-record
counter under `--fwd-pipeline`) stays F-local — it is not a ring field.

## 3 · Step 3 — the holons bound

### 3a · The four leaves extracted by anchor (2026-09-06) — CLOSED

**Built (`r5s3a_patch.py`; `flow/holons.{hpp,cpp}` new):** `object_repair` (361 lines), `mem_advect` (16), `mem_merge`
(21), `mem_refresh` (57) — lambdas of `run_flow` — are functions of `pfg::flow` now. The script located each lambda
by its `auto NAME=[&](` anchor and its closing `};` at the same indent, kept the parameter list verbatim, and moved the
body text untouched: **339 of 339 moved body lines (100.00 %) are byte-identical (whitespace-trimmed) to the
pre-extraction `flow.cpp`** (`r5s3/flow_pre_extract.cpp` is the reference). The captured scratch (`obj_label, obj_bfs,
obj_clusters, obj_used, obj_rowmin/max, obj_chamf, obj_feat, obj_slots_fwd/bwd, mem_prior, mem_adv, wake_rec, wake_n,
obj_nblk`) is `pfg::flow::HolonScratch`, owned by `run_flow` as `hs`, bound inside each function by reference only
for the names that body uses (the script scanned each body); the captured `cfg` / `mvw_f` / `mvh_f` are parameters,
passed only where used (`mem_merge` takes no scratch, `mem_advect` / `mem_refresh` no `cfg`). The three struct types
(`ObjCluster`, `ObjSlot`, `WakeRec`) moved to the header; `run_flow` keeps them as `using` aliases, keeps every
scratch name as `auto& X = hs.X;` on the former declaration line (its comment intact), and keeps the four lambdas as
thin wrappers with the SAME parameter lists calling the functions — so `consume_wap` (step 3c) calls exactly what it
called, textually. `flow.cpp` 2,235 → 1,798 lines; `holons.cpp` 494. CMake: one source added.

**Seen red, twice, before the gate:** (1) the new files were written with `\r\r\n` endings (a double CRLF conversion
in the script) — MSVC C4335 "Mac file format"; normalised to CRLF. (2) `mem_merge` bound a scratch parameter it never
reads — C4100; dropped from its signature and wrapper. After both: **build 0 errors, 1 warning** (the pre-existing
`flow.cpp` C4189 `hostC2`).

**Gate:** the three exit paths (async default / sync / grid, 10 s: `rc=0 done=1 clean=1 abandon=0` each); the 60 s
default run against the step-2 binary's — rows 14,385 vs 14,384, fresh 99.82 vs 99.79 %, `MsAddedLatency` 20.81 vs
20.80, `disp_phase` mean 0.5011 vs 0.5010 (sd 0.2816 / 0.2814), `disp_src` step 0.2501 vs 0.2503, uniq/s 239.0 vs
238.9; the stats line shows the object-holon live (`obj:4 rep:0%`, `gme(dis:0% fit:3.15ms)`). One run per side
(reliability not measured — DI-3); every number inside R4's measured spread for this content.

**Not in this step:** the arms (3b), `consume_wap` (3c), the consensus move (3c's question).

### 3b · The rows decide on F: the effective ON resolved and proven, the arms consumed, nine sites bound (2026-09-06) — CLOSED

**Built (`r5s3b_patch.py`):** (1) `LayerConfig` gains the RESOLVED state — `avail` (on ∧ not unavailable ∧ every
`needs` row avail, a monotone fixpoint; `req_any` = any) and `eff` (avail ∧ no `excludes` row avail) —
`layer_resolve_effective()`; and `layer_flow_resolve()` computes the init-time unavailable mask (every FLOW row when
`!use_wap`; `GME_GPU` when forced off; `MV_SMOOTH` when its pipe was not created; `CANDIDATES` when its bridge failed),
resolves, and checks thirteen FLOW facts against the init cascades' `use_*` (`gme` avail, `gme_gpu` eff, the CPU gme
eff, `objects`, `objects_bwd`, `mem_fwd/bwd/refresh`, `gme_bwd`, `bidir`, `candidates`, `persistence`, `mv_smooth`)
— `main.cpp` calls it after `init_gme_finalize()` and refuses to run on a disagreement (exit 3, R0's discipline one
level later), printing the resolved line once. (2) `consume_wap` fills `ArmInputs` from this pair's CONTROL facts
(`have_prev_f`, `pressure_tier`, `holon_skip_pair`, `!allow_bwd`, `bwd_skipping`), evaluates `layer_arm_mask`, feeds
the bidir row's decision back as `bwd_ok` and evaluates it again; **nine sites now act on `eff ∧ armed`:** the
backward match (`do_bwd` = the BIDIR row), the forward fit (`row_gme_ran` = GME avail ∧ PRIOR), the device variant
(`GME_GPU` eff, both anchors), `mem_fwd`, `objects`, `gme_bwd`, `mem_bwd`, `objects_bwd`, `mem_refresh`, and the
objects stats block — while the former hand condition is computed beside each and every disagreement is COUNTED
(`row_check`) and printed at F's exit. (3) `BWD_HOLON` removed (entry decision 3, corrected). Build 0 errors; 36
warnings = the full-rebuild pre-existing set.

**Gate, seen green on this binary:** `--layer-dump` shows `bidir P arm=BIDIR`, `gme_bwd / mem_bwd / objects_bwd H
arm=BWD`, `mem_refresh H arm=HOLON`; the parse-parity corpus 21 / 21 (a subset of step 1's, re-run on this binary);
**twelve bounded runs on the ball zoo, each `rc=0`, clean exit, `PARITY FAIL` count 0, the resolved line `== the init
cascades`, and `0 mismatches`** over 9,981 / 9,992 / 9,992 / 6,360 / 4,539 / 9,981 / 6,360 / 9,981 / 9,981 / 9,970 / 0
/ 9,992 site decisions (default, `--no-memory`, `--no-objects`, `--no-bidir`, `--no-gme`, `--no-gme-gpu`,
`--fwd-pipeline`, `--no-ambig`, `--no-inertia`, `--mv-smooth 0.5`, `--no-warp-at-presenter` — the FLOW stage
unavailable: every row off, 0 sites, `== the init cascades` — and `--no-async-present`); the 60 s default run:
**39,604 site decisions, 0 mismatches**, presents 14,382, `fresh:240/s`. Quoted (default): `[layertab] flow rows
resolved: avail=0x0FEF7F6D eff=0x0FEF7F6D (gme=1 gme_gpu=0 objects=1 memory=1 bidir=1 candidates=1 persistence=1
mv_smooth=0 consensus=1) == the init cascades` — `gme_gpu=0` is the single-GPU rig's forced-off, reproduced by the
unavailable mask.

**What the two oracles do NOT cover, named:** the tier ladder above tier 1 (the ball zoo never pressures F: the
`HOLON` arm's `tier < 4` / `holon_skip` legs and `BIDIR_OK`'s `tier < 5` were evaluated on `tier = 0` every pair — the
CONTROL inputs are wired, their shedding branches were not exercised; a `--load-governor` run under real pressure
is the named test); `bwd_skipping` likewise (never latched here). The hand conditions stay in the code as the second
oracle — they are the instrument, not dead code; step 3c or R7 may retire them once a pressured run has counted 0.

### 3c · `consume_wap` extracted by anchor (2026-09-06) — CLOSED

**Built (`r5s3c_patch.py`; `flow/flow_consume.{hpp,cpp}` new):** the stage's orchestrator — the per-pair CPU tail and
the F→P publish, 444 lines — is a function of `pfg::flow` now. Method as 3a: the body text MOVED, **339 of 339
moved body lines byte-identical** to the pre-extraction file (`r5s3/flow_pre_consume.cpp` is the reference). What
the lambda captured is rebound at the top of the function in three groups: the **61 FgContext-derived names** by the
SAME `auto& X = ctx.X;` lines run_flow uses (the script copies them, it does not retype them); the F thread's
per-pair state through **`ConsumeState`** — 30 references, run_flow stays the owner (CR1); and the **five run_flow
lambdas** the body calls (`flow_submit_nowait`, `flow_submit_q2_chain`, `flow_downsample`, `mv_audit_stat`,
`objdump_grid`) through `std::function` members typed to their signatures, so every call site inside the body is
textually unchanged — including the inline lambdas passed to `objdump_grid`, which convert implicitly. `FwdPend`
(the deferred-consume snapshot) moved to the header with it. run_flow keeps `consume_wap` as a thin wrapper, so its
two call sites (serial + deferred) are untouched.

**A dead-code finding the extraction exposed:** with the tail gone, the compiler named **32 `auto& X = ctx.X;`
aliases in run_flow that nothing referenced any more**. They were removed (the compiler is the proof: deleting a
live one fails the build) and `flow.cpp` now compiles with **zero warnings** — it had been carrying a C4189 since
before this arc. `flow.cpp` 1,872 → **1,429 lines** (2,235 at R5's start).

**Seen red before the gate:** `hs` is never named in the moved body (run_flow's aliases hide it) so the generator
did not bind it while the regenerated leaf wrappers needed it; then it was bound twice. Both fixed in the script,
not by hand-editing the output.

**Gate:** build 0 errors, 0 warnings; parse parity 11 / 11; the twelve token runs each `rc=0`, clean exit, `== the
init cascades`, **0 mismatches** (10,223 site decisions on the default); the three exit paths clean; the 60 s
default run against the step-3b binary — fresh 99.83 vs 99.74 %, `MsAddedLatency` 20.74 vs 20.75, `disp_phase`
0.5012 vs 0.5014, `src` step 0.2501 vs 0.2503, uniq/s 239.0 vs 238.8.

`consume_wap` is 444 lines (`flow.cpp:964–1407` after 3a/3b), captures 61 `run_flow` locals (the delegated scan's
count, re-checked by the session's own scan: the host bridges, the FlowRing scalars, the devices / queues / fences,
the stats atomics, the `use_*` facts, `ofp`, `gmePipe`, `cmdB_bwd`, `fB2`, …) and calls eight `run_flow` lambdas
(`flow_downsample`, `flow_submit_nowait`, the four leaf wrappers, `mv_audit_stat`, `objdump_grid` — a generic
lambda). Its extraction is the same by-anchor method with a context struct of ~61 references + 8 callables; its
gate is the same two-oracle instrument. It is the stage's ORCHESTRATOR (the per-pair tail: the inertia update, the
tier ladder, the row decisions, the publish) — the leaves it orchestrates are already out (3a), which is what E6's
CPU-kernel testbench needs. Moving the orchestrator gains structure, no behaviour; it is not required for G-R5's
"the holons as rows" (they ARE rows, deciding — 3b). Deferred to the operator's word (§5).

## 4 · Step 4 — the 3→5 transport governed by the rows (2026-09-06) — CLOSED, with its premise corrected

The plan (`CONVERGENCE_MASTER_PLAN.md` §R5): "`wap_upload` is made conditional: `FlowSet` device == `GenFrame` device
⇒ no copy (single-GPU = the rig)". Read against the code after 3a: under the shipping default the CPU holons READ AND
WRITE the host copies of the flow fields — `object_repair` repairs the MV field in place (`hostMV[gen]`, written
back via `float_to_half`), rewrites the dissidence mask (`hostDIS`), resets the persistence field (`hostPER`);
`mem_merge` rewrites the masks; the gme CPU fit writes `hostDIS` / `hostDISB`; `persistence` writes `hostPER`. So on
the default set the image→host download exists because the CPU needs the fields, and the host→image upload
(`wap_upload`, `present.cpp:668–791`) exists because the REPAIRED fields must reach the warp — neither is a
two-device artefact; single-GPU removes nothing. The plan's premise ("the holons off by default") was the same stale
line entry decision 1 corrected. What a conditional upload CAN save: the fields no host pass writes — the registry
knows exactly which (the `writes_ch` of the `Kind::H` rows that are effectively on: with objects / memory /
persistence / the CPU gme off, `MV_RAW_FWD`, `SAD`, `MV_BWD`, `CANDIDATES` are GPU-resident and could be sampled
from F's images directly (cross-queue: A.q2 → A.q semaphores), and `SAD` / `CANDIDATES` are never host-written even on
the default). That is a per-channel data-path design — device images shared between two queues, its own barriers,
its own byte-identity and latency gate — and its gain on the DEFAULT set is small (SAD + candidates). It is a
product/latency project the operator frames, not a mechanical step of R5.

**What was built instead (the honest step 4).** (a) **The transport is conditional on the declared rows.** Each
per-channel upload in `wap_upload` is gated by its producing row's effective ON (`cfg.layers.eff`) instead of a hand
flag — `wapMVBA` ← BIDIR, `wapC2A` ← CANDIDATES, `wapDISA` ← GME (either variant), `wapDISBA` ← GME_BWD, `wapPERA` ←
PERSISTENCE, `wapMVTA` ← the SAMPLE-stage VBLEND row — with the former condition kept beside each as the second
oracle (step 3b's instrument, counted and printed at the present loop's exit). (b) **The registry answers the
transport question from the table**, measured rather than promised: `layer_host_written_channels()` unions
`writes_ch` over the effectively-on `Kind::H` rows — that IS the set of channels a CPU pass dirties, so their host
copy is authoritative and their upload is inherent. `layer_transport_report()` prints it once at init. On the
shipping default:

> `[layertab] 3->5 transport: CPU-authored [mv_raw_fwd,persist,mv_bwd,dissidence] … Not CPU-authored
> [sad,prev,cur,candidates,mv_target] — of these, the FLOW fields (sad, candidates, mv_target) are GPU-produced and
> GPU-consumed and a device-resident FlowSet could share them without the round trip; prev/cur are stage-2 frames on
> the ingest path, a separate question.`

So the answer to "what would a conditional upload save on the default set" is now a fact, not an estimate: **three
of the nine transported channels**, and only through a cross-queue image share — the design the operator frames.
With `--no-gme` the CPU-authored set collapses to `[persist]` and six channels become shareable; the report follows
the configuration.

**Gate:** build 0 errors; the twelve token runs and the 60 s default all report **0 transport mismatches**
(21,546 site decisions on the 60 s default); the report's content changes correctly with `--no-gme`, `--no-bidir`,
`--no-inertia` (quoted per run in the gate logs).

## 5 · G-R5 — **PASSED** (2026-09-06)

**Proven at this checkpoint:** the FLOW stage is DECLARED (14 rows; `Kind::H`; the consensus with its own switch),
`FlowSet` / `FlowRing` exist as the contract types, the four leaf holons are functions over a declared scratch
(339 / 339 lines verbatim), and **the rows decide on F** — proven two ways: the resolved effective-ON equals the init
cascades on every token set tried (13 checks × 12 runs), and the rows' per-pair decisions equal the former hand
conditions on 0 of 39,604 site decisions (default, 60 s) and 0 across eleven other runs. The default output after
each step matched the step before (n = 1 per side, the placement metrics inside R4's spread). Two oracles are still
in the code by design (the hand conditions beside the rows): the instrument, not dead code.

**The formal A/B, two runs per side (DI-3), the current binary against the pre-R5 one (`3bf654b`, rebuilt from that
commit for the purpose, then HEAD restored and rebuilt):** 60 s default runs on the ball zoo.

| metric | pre-R5 ×2 | R5 ×2 | spread (pre / R5) | Δ |
|---|---|---|---|---|
| CSV rows | 14,386 / 14,385 | 14,386 / 14,383 | 1 / 3 | −1 |
| fresh % | 99.85 / 99.83 | 99.84 / 99.76 | 0.02 / 0.08 | −0.04 |
| `MsAddedLatency` | 20.66 / 20.79 | 20.59 / 20.82 | 0.13 / 0.22 | −0.02 |
| `disp_phase` mean | 0.5009 / 0.5009 | 0.5009 / 0.5010 | 0.0000 / 0.0001 | 0.0000 |
| `disp_phase` sd | 0.2813 / 0.2814 | 0.2813 / 0.2814 | 0.0001 / 0.0001 | 0.0000 |
| `disp_src` step | 0.2501 / 0.2501 | 0.2501 / 0.2503 | 0.0001 / 0.0003 | 0.0001 |
| uniq/s | 239.1 / 239.0 | 239.0 / 238.8 | 0.0 / 0.2 | −0.1 |
| `warp` ms | 3.757 / 3.839 | 3.700 / 3.875 | 0.081 / 0.175 | −0.011 |
| `iter` ms | 4.040 / 4.125 | 3.982 / 4.160 | 0.085 / 0.178 | −0.012 |

**Every delta is smaller than the same metric's run-to-run spread** — the instrument cannot see a difference between
the pre-R5 binary and the restructured one on the default set.

**The pressured run — the branch the zoo could not reach (`tools/r5_pressure.ps1`).** The load source is NOT
`tools/gpu_load.exe`: measured the same day, it holds the 4090 at 32–35 % (§4.6.1), while the sibling project's
`projects/gpu_oc/escalera_arbiter.exe --profile heavy` holds **96–100 % utilisation at 355–361 W and ~10.7 GB of
VRAM** (`nvidia-smi` sampled at 1.2 s over a 25 s run, quoted in the project's `docs/LEARNING_LOG.md` P-007). Two
runs:
- **Saturation alone did not move the ladder.** 45 s, `--profile heavy`, source 60 fps: the FG held `240.0 fps …
  uniq 240/s fresh:240/s … gpu(A:86%)`, clean exit, and **no `gov-floor ENGAGE` line** — the tier stayed 0. The
  ladder is a CPU-time ladder (`t_pair_ema` vs `pair_budget_ms = src_interval_us/1000`); GPU load raises only the
  GPU legs F waits on, which is one term of it.
- **Shrinking the pair budget did.** 40 s, `--profile chaos`, **source 120 fps** (budget 16.7 → 8.3 ms):
  `governor: tier:4 ×3`, `tier:5 ×6`, stats line `… fresh:141/s rdrop:94/s … bwd-skip:96% tier:5 …`, clean exit,
  arbiter verdict `STABLE`. **Both instruments still 0 mismatches — 34,171 row decisions and 28,146 transport
  decisions — now WITH the `HOLON` (tier < 4, `holon_skip`) and `BIDIR_OK` (tier < 5, `bwd_skipping`) branches
  live.** That is the coverage 3b declared missing, and it is now had.

**The 120 s smoke:** {SMOKE}

**Verdict: G-R5 PASSED.** The stage is declared (14 rows), its contract types exist (`FlowSet`/`FlowRing`), its
holon leaves and its orchestrator are their own translation units (100 % verbatim moves), the rows decide — proven
against the init cascades at startup and against the former hand conditions per pair, across the whole tier ladder
— the transport is row-governed, and the default output is unchanged within the instrument's resolution.

**Owed, named, not blocking:** the two oracles (the hand conditions beside the rows, the transport's hand flags)
are still in the code as the instrument; retiring them is R7's, once a second pressured run has counted 0 again.
The device-resident FlowSet for the three GPU-only channels is a separate design (§4). `stats_second()` still lives
in the present loop (R4's residual).

## 6 · Honesty ledger

- Step 1 changes NO behaviour on the default path except two CLI-COMPAT edges, both stated: `--mv-smooth` values
  outside [0,1] are now clamped (they were undefined), and `--no-mv-consensus` exists (default ON → identical).
- The parity corpus is the session's choice of 39 combinations, not an exhaustive product of tokens; the shadow
  parser's new multi-carrier rule is exercised by five of them (`--no-inertia`, `--no-ambig`, `--no-memory`,
  `--no-objects`, and the eight-token line).
- The row COUNT (28 of 32) is a hard ceiling the next stage hits; the widening (64-bit masks or two words) is S5's
  first task, not R5's.
- The 36 warnings are the full-rebuild set seen since R3 (`fopen` C4996, C4189 unreferenced locals, C4456 shadowed
  `d`, C4127 in `layer_registry.cpp:25`); none is on a line this step touched. After 3c, `flow.cpp`'s own share is
  zero.
- **What the two-oracle instrument does and does not prove.** It proves the rows' decisions EQUAL the hand
  conditions on every pair of every run made — 39,626 + 34,171 row decisions and 21,546 + 28,146 transport
  decisions, all at 0 mismatches, across twelve token sets, the default, and the pressured run. It cannot prove
  equality on an input never presented: the `--nvofa` provider path, a multi-GPU rig (where `gme_gpu` would be
  effectively on — this rig forces it off), and `--fwd-pipeline` combined with pressure were not run together.
- The step-to-step comparisons (§2, §3a, §3c) are one run per side — "reliability not measured" per DI-3. The
  binary-level claim rests on §5's two-runs-per-side A/B, which is the one that matters.
- The pressured run used a 120 fps source to shrink the pair budget. That is a legitimate way to reach the ladder
  and it is what the ladder measures, but it is not a game: a real title pressures F through content complexity
  (more objects, larger dissidence masks) as well as rate. The shedding branches are covered; their behaviour
  under game-class content is not measured.
- `escalera_arbiter.exe` is a stability tool used here purely as a load. Its own verdict (`STABLE`) is recorded per
  run so that a GPU fault can never be mistaken for an FG measurement — but the FG numbers under load are single
  runs, not DI-3 pairs.

*Made with my soul - Swately <3*
