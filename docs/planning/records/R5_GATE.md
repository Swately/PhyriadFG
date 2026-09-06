# R5_GATE — Stage 3 FLOW: the rows declared, `FlowSet`/`FlowRing`, the holons bound, `wap_upload` conditional (M-R5)

**Status: IN PROGRESS — step 1 of 4 closed (2026-09-06).** The operator's word: "adelante, continua segun todas tus
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
3. **Four ArmId values, not three:** `PRIOR`, `HOLON`, `BWD_HOLON`, `BIDIR_OK` — the backward legs' arm (bwd ∧ the
   holon shedding) is its own combination; `ArmInputs` gains five fields (`has_prev, tier, holon_skip, pipelined,
   bwd_skipping`), the map's four plus the bwd-skip hysteresis it had folded into "tier".
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

## 2 · Step 2 — `FlowSet` + `FlowRing` (pending)

## 3 · Step 3 — the holons bound (pending)

## 4 · Step 4 — `wap_upload` conditional (pending)

## 5 · G-R5 (pending)

## 7 · Honesty ledger (running)

- Step 1 changes NO behaviour on the default path except two CLI-COMPAT edges, both stated: `--mv-smooth` values
  outside [0,1] are now clamped (they were undefined), and `--no-mv-consensus` exists (default ON → identical).
- The parity corpus is the session's choice of 39 combinations, not an exhaustive product of tokens; the shadow
  parser's new multi-carrier rule is exercised by five of them (`--no-inertia`, `--no-ambig`, `--no-memory`,
  `--no-objects`, and the eight-token line).
- The row COUNT (28 of 32) is a hard ceiling the next stage hits; the widening (64-bit masks or two words) is S5's
  first task, not R5's.
- The 36 warnings are the full-rebuild set seen since R3 (`fopen` C4996, C4189 unreferenced locals, C4456 shadowed
  `d`, C4127 in `layer_registry.cpp:25`); none is on a line this step touched.

*Made with my soul - Swately <3*
