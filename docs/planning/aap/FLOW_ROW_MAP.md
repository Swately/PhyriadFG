# FLOW_ROW_MAP — does the LAYERTAB schema hold the holon family? (S4.2 / R5's paper half; risk XR3's residual)

**Question (the one COLUMN_CLOSURE_EXPERIMENT.md left open, its §3 last paragraph):** the ten shader layers
mapped onto the registry with **0 new columns**; the FLOW-stage passes — `flow.cpp`'s holon family, which the
shader does not contain — were not in that experiment. If THEY need columns the schema does not have, LAYERTAB's
central bet fails before R5 writes a line. This document maps every pass of stage 3 (and the two stage-3 passes
that live on the P thread today) onto the schema as it stands after R3/R4, counts the new COLUMNS, and applies
the same decision rule (A3 §4.2: `≤ 1` proceed, `2–3` design-and-review, `≥ 4` re-select).

**Method.** A delegated mechanical inventory of `src/flow/flow.cpp` (2,235 lines) + the `wap_upload` /
`medPipe` sites of `src/present/present.cpp` + the defaults in `src/cli/cli.hpp` (Sonnet, read-only, every claim
line-cited; 2026-09-06); fourteen of its citations re-read first-hand by the session before use (§4). The
mapping and the count are the session's. **No code was written or changed** (the sequence's 4.2 says "No code").

## 0 · The schema the passes are measured against (after R3/R4)

`layer_abi.hpp` — **LAYER columns:** `id, name, stage, rank, kind, default_on, overrides, needs, req_any,
excludes, reads_ch, writes_ch, shadows, arm, group, dominates_ok, help, params(first,count)` + the four
CLI-COMPAT columns `on_flag, off_flag, flag_alias, pflags`. **Stage values:** `MVCOND, SAMPLE, WEIGHT, COMPOSE,
FLOW, HOST` (`FLOW` and `HOST` exist since R0, unused by any row). **Kind values:** `F` (fused body), `P` (own SG
pass), `X` (pseudo-row). **ArmId values:** `ALWAYS, GME, BWD, GME_AND_BWD, COMMIT`, evaluated from `ArmInputs
{gme_ok, bwd_ok, matte_ok, appear_ok, commit_ok}` (R3). **Channel bits:** `CH_MV, MV_RAW_FWD, MV_SAMPLE, SAD, PREV,
CUR, D_PIXEL, STASIS, A_SAMP, B_SAMP, WARP_OK, PERSIST, MV_BWD, CANDIDATES, DISSIDENCE, MV_TARGET, GME, BLEND`.

The contract the rows must fit (STAGE_CONTRACT §1–2): stage 3 turns a `Pair` (+ `FlowSet[gen−1]`) into
`FlowSet[gen]` — MV fwd, SAD, MV bwd, gme model fwd/bwd + valid, dissidence fwd/bwd, persistence, candidates,
`mv_target`, `bwd_valid`, the pair identity, the mass stats — published by `f_seq.fetch_add` (`flow.cpp:1777`);
"every field in `FlowSet` is produced HERE or is absent (validity bit 0)". As in R0's experiment: channel BITS
and stage/kind/arm VALUES are data in existing columns and are counted separately; only a new FIELD of
`LayerDesc` is a COLUMN.

## 1 · The holon family, mapped (stage FLOW; ranks are placeholders that preserve today's order)

| # | Pass (flag) | Source (flow.cpp unless noted) | Stage · rank · kind | Row | needs / excludes / arm | reads → writes (channels) | Params | New COLUMN? | Notes |
|---|---|---|---|---|---|---|---|---|---|
| 1 | **flow source** — the OFP fwd match | record sites `:1963–2000` (WAP), `:2103–2117`; `flow_downsample :706–721`; `NvofaProvider :136–268` | FLOW · 0 · P | `flow_source` | arm ALWAYS; not user-toggleable (it IS the motion source — the FLOW twin of `fetch_mv`) | PREV, CUR → **MV_RAW_FWD**, SAD | `provider` {ofp, nvofa} (`--nvofa`, default off, create-time validity: single-GPU ∧ `flow_div==1` ∧ OFA support, `flow_init.cpp:85–96`, falls back byte-identically); `flow_div` (the LINEAR blit before the match); `nvofa_cost_scale` 0.5 / `nvofa_sadz_scale` 4.0 | — | the provider is a PARAM with a validity rule, like R0's cascades; the downsample is the source's input scaling, not a row |
| 2 | **mv_smooth** (`--mv-smooth`) | factory `:269–291`; dispatch `:1991–2000` / `:2103–2117` | FLOW · 10 · P | `mv_smooth` | needs FLOW_SOURCE; arm ALWAYS | MV_RAW_FWD, **MV_PREV_GEN** (new bit: the pipe's own previous-generation store) → MV_RAW_FWD (in place) | `alpha` (default 0 = OFF), `kMvCut` 6.0 (`:475`) | — | the gen−1 read is the row's own state — the CH_PERSIST pattern (§2.5) |
| 3 | **ambig** (`--ambig`) | copy-out `:2035–2039`; `emit_second_best` at `flow_init.cpp:40` | FLOW · 20 · P | `ambig` | needs GME (`flow_init.cpp:201` `use_ambig=use_ambig&&use_gme`); arm HAS_PREV | MV_RAW_FWD (the match's runner-up) → CANDIDATES | none | — | the emission is a mode of the source + a copy-out; a row because its `needs` is a row relation (the referee is gme); `CH_CANDIDATES` exists |
| 4 | **bidir** (`--bidir`) | `:1488–1532` | FLOW · 30 · P | `bidir` | needs FLOW_SOURCE; arm HAS_PREV ∧ **PRESSURE_LT5** (`:1488` `do_bwd = allow_bwd && use_bidir && have_prev_f && !bwd_skipping && !tier5_active`) | PREV, CUR (swapped) → MV_BWD, SAD(bwd, the live image) | none here (`occl_thresh` is stage 5's) | — | `bwd_skipping` is a CONTROL hysteresis on `t_pair_ema` (`:1428–1431`) — folded into the pressure input (§2.4); `allow_bwd=false` under the pipeline mode (§2.3) |
| 5 | **persistence** — the inertia field producer | `consume_wap :1411–1426` → `hostPER` | FLOW · 35 · **H** | `persistence` | needs FLOW_SOURCE; arm ALWAYS | MV_RAW_FWD, SAD, PERSIST(gen−1) → PERSIST | `--inertia` — the SAME token as the MVCOND `inertia` consumer row | — | shared token → the invariant relaxation of §2.2; Kind H = a host pass (§2.1) |
| 6 | **gme** fwd (`--gme`) | CPU `gme_fit_affine :44–134`, fit `:1535–1590`; GPU `gme_create :303–372`, `gme_record :398–439`, record `:2008–2019` | FLOW · 40 · **H** (CPU) / P (device-B variant) | `gme` (+ `gme_gpu` as its excluding variant row, or one row with a `device` PARAM) | needs FLOW_SOURCE; arm HAS_PREV | MV_RAW_FWD, SAD → **GME**, DISSIDENCE | `--gme` (ON), `--gme-gpu` (ON; forced off on single-GPU, `flow_init.cpp:177–180`), `--gme-gpu-verify` (off), `gme_irls2` (off), `kChangeGateSadZ` 0.5; tier-5 forces `iters=1` (`:1485`) | — | the CPU/GPU split is two Kind values of one algorithm — two rows that `excludes` each other under one flag, or a `device` param; a judgement (§2.1) |
| 7 | **mem_fwd** — advect + merge (`--memory`) | `mem_advect :1259–1274`, `mem_merge :1290–1310`; call `:1590–1595` | FLOW · 50 · H | `mem_fwd` | needs OBJECTS (`flow_init.cpp:221` `use_memory = use_objects && cfg.scene_memory`); arm HAS_PREV ∧ **PRESSURE_LT4** (`:1590` `use_memory && !holon_skip_pair`) | MV_RAW_FWD, DISSIDENCE, **MEM_PRIOR**(gen−1) → DISSIDENCE (merged in place), **MEM_ADV** | `kPriorDecay` 0.55 (`flow.hpp:54`) | — | must precede `objects` (merge, then cluster the merged mask — `:1590–1611`): rank order |
| 8 | **objects** fwd (`--objects`) | state `:780–850`, `object_repair :885–1245` (RIGID `:1027`, SHAPE-FIELD `:1076`); call `:1604–1614` | FLOW · 60 · H | `objects` | needs GME (`core_init.cpp:327` `use_objects=cfg.objects&&use_gme`); arm HAS_PREV ∧ PRESSURE_LT4 (`:1604`) | MV_RAW_FWD, DISSIDENCE, GME, PERSIST, **OBJ_STATE**(gen−1: the 16-slot identity table) → MV_RAW_FWD (repaired in place), DISSIDENCE (re-walked), PERSIST (when `persist_reset`, `:1609`), OBJ_STATE, the matte mass `f_pair_mfwd_a` (a FlowSet scalar) | `--shapefield` (ON: selects the arm), `--obj-fill-rim` (ON), `--expire` (ON), `--persist-reset` (ON, fwd only); the `kObj*` constants (`flow.hpp:33–44, 68–72`) = params without flags | — | writes PERSIST, a field the MVCOND `inertia` row reads: the 3→5 contract, not a new relation |
| 9 | **gme_bwd** | GPU `:1515–1528`; CPU `:1697–1711` | FLOW · 70 · H / P | `gme_bwd` | needs GME, BIDIR; arm BWD | MV_BWD, SAD(bwd) → **GME_BWD** (or the GME payload's bwd half), DISSIDENCE(bwd half) | as `gme` | — | finding for R5, not a column: the GPU bwd fit reads the live bwd SAD image, the CPU bwd fit reads `hostSAD` (fwd) — `:1516–1520`, the inventory's note; a divergence to close when the row is declared |
| 10 | **mem_bwd** — merge | `mem_merge :1290–1310`; call `:1712–1716` | FLOW · 71 · H | `mem_bwd` | needs MEM_FWD, BIDIR; arm BWD ∧ PRESSURE_LT4 | DISSIDENCE(bwd), MEM_ADV → DISSIDENCE(bwd) | — | — | — |
| 11 | **objects_bwd** | call `:1717–1729` | FLOW · 72 · H | `objects_bwd` | needs OBJECTS, BIDIR; arm BWD ∧ PRESSURE_LT4 | MV_BWD, DISSIDENCE(bwd), GME_BWD, OBJ_STATE → MV_BWD, DISSIDENCE(bwd), OBJ_STATE, **WAKE** (the wake records `:826`, `:1338`), `f_pair_mbwd_a` | as `objects` | — | leaves the `obj_label` scratch `mem_refresh` reads (`:818–825`): rank order + OBJ_STATE, no column |
| 12 | **mem_refresh** (+ wake evaporation) | `:1324–1380`; call `:1734–1739` | FLOW · 80 · H | `mem_refresh` | needs MEM_FWD; arm HAS_PREV ∧ PRESSURE_LT4 (`:1734` `use_memory && gme_did_fit && !holon_skip_pair`); the evaporation part arm BWD (`:1328/:1345`) | MEM_ADV, DISSIDENCE, OBJ_STATE, WAKE, MV_RAW_FWD → MEM_PRIOR | `kPriorDecay` | — | the LAST holon: consumes what the bwd repair left |
| 13 | **mv_consensus** — today P's `medPipe` | `present.cpp:734–765` (inside `wap_upload`), gate `:747` `if(use_mv_median||use_mv_guided)`, `:748` `sim_push = use_mv_guided ? cfg.mv_sim : 0.f`; factory `flow.cpp:442–469` | FLOW · 90 · P | `mv_consensus` | needs FLOW_SOURCE; arm ALWAYS | MV_RAW_FWD, MV_BWD (if BIDIR), CUR → MV_RAW_FWD, MV_BWD (in place via scratch) | today: `--mv-median` (blind 3×3, default OFF) and `--mv-guided` + `--mv-sim` (the color-weighted consensus, default ON) — the SAME token as the MVCOND `mv_guided` row; **R5 gives this row its OWN token** (`--mv-consensus` / `--no-mv-consensus`, default ON to stay byte-identical) = the operator's declared switch (the 1.7 px low-phase finding, `phyriadfg-verification-as-data`) | — | `blind` = a PARAM; the second shared-token case today (§2.2) — it disappears with the new token; moving the pass from P (per pair-advance) to F (per pair) keeps the count and the field: G-R5's CSV byte-diff is the check |
| — | **publish** | `:1774–1778` (WAP), `:2188–2192` (non-WAP) | not a row | — | — | the FlowRing's `f_seq` | — | — | the ring contract (STAGE_CONTRACT §1) |
| — | **wap_upload** | `present.cpp:668–791`, called on pair-advance `:2444` | not a row | — | — | the 3→5 transport of `FlowSet[gen]` to the warp's images | `--upload-xfer` (off) | — | exists only when stages 3 and 5 are on different devices (STAGE_CONTRACT §3): a `FlowSet.device` fact, not a row; its embedded consensus dispatch leaves with row 13 |
| — | **fwd_pipeline** | `:722–740`, `:2041–2070` | not a row | — | — | stage 3's scheduling mode | `--fwd-pipeline` (off) | — | forbids the bwd rows (`allow_bwd=false`, `:1386–1389`): a row↔MODE relation — §2.3 |
| — | **pressure ladder / governor** | `:1427–1485`; `g_gov_floor` at `:1467` | not a row | — | — | CONTROL (STAGE_CONTRACT §3 `GovernorFloor`) | the tier thresholds (ratios of `pair_budget_ms`), `kTier4DwellPairs` 90 | — | its per-row effect = the PRESSURE_LT4 / LT5 arm values + the tier-2/3 decimation (`holon_period`, `:1469`) — §2.4 |

The three helper lambdas (`flow_submit_nowait :632–644`, `flow_submit_q2_chain :650–657`, `flow_downsample
:706–721`) are the stage's submit/scale plumbing — the SG seam's job (R2), not rows.

## 2 · The findings that are NOT columns but shape R5

### 2.1 A host pass needs a Kind value, not a column
Nine of the thirteen rows run on the CPU (`persistence`, `gme` CPU, `mem_*`, `objects*`, `mem_refresh`) —
single-threaded host functions over `FlowSet` fields, "zero per-pair heap" (`:861`, `:1246`). `Kind::P` means "own
SG pass": the seam would try to derive GPU barriers for them and the generator would look for a GLSL body.
**A third Kind value, `H` (host pass),** keeps them in FLOW's rank space (their ORDER among the GPU passes is the
whole point: merge before cluster, refresh last) while telling the seam and the generator to skip them. A value in
an existing column. The gme CPU/GPU pair is the one algorithm with both kinds: two rows that `excludes` each other
under one flag (`--gme-gpu` selects), or one row with a `device` PARAM — R5 decides at entry; neither is a column.

### 2.2 Two tokens drive two rows each today (an invariant relaxation, not a column)
`--inertia` arms the MVCOND `inertia` row (the consumer) AND the FLOW `persistence` row (the producer of
`hostPER`, `:1411–1426`); `--mv-guided` arms the MVCOND `mv_guided` row (the per-pixel guided fetch in the shader)
AND the consensus pass (`present.cpp:747–748`). The CLI-COMPAT columns assume one token → one row (the parser
shadow's parity). For `inertia`: **relax the invariant** — a token may appear as `off_flag` on two rows; the
shadow maps it to both (the cascade `use_inertia` already does exactly this by hand). For the consensus: the
plan's decision stands — **its own token**, so the shared-token case does not survive R5 there. No new field.

### 2.3 A row-vs-mode relation: `fwd_pipeline` forbids the bwd rows
Under the pipeline the deferred consume cannot host a second in-flight match (`:1386–1389`), so `allow_bwd=false`.
The schema expresses row↔row exclusion (`excludes`), not row↔mode. Two ways, neither a column: (a) the mode is
made a pseudo-row (`Kind::X` has no enable — it would need to be a P row with no pass, an abuse); **(b)
recommended: the mode is an `ArmInputs` field (`pipelined`) and the bwd rows' arm reads it** — the same
mechanism as the tier below. R5 takes (b).

### 2.4 `ArmInputs` grows by four fields; `ArmId` by three values
Today `ArmInputs {gme_ok, bwd_ok, matte_ok, appear_ok, commit_ok}` are per-generation validity bits derived from
`FlowSet` fields. The FLOW rows need: **`has_prev`** (the pair has a prior — the first pair after start runs
nothing but the source), **`tier`** (the CONTROL pressure tier, `pressure_tier` after the governor floor `:1467`),
**`holon_skip`** (the tier-2/3 decimation decided by CONTROL: `holon_pair_ctr % holon_period`, `:1468–1474`),
**`pipelined`** (§2.3). ArmId values: **`HAS_PREV`, `PRESSURE_LT4`** (memory/objects: shed at tier ≥ 4 and
decimated at 2/3), **`PRESSURE_LT5`** (bidir: shed at tier 5; gme's `iters=1` at tier 5 is a PARAM override the
governor applies, `:1485`). The ladder itself stays in CONTROL; the rows only READ the arm. Fields and values, not
columns — the same shape R3 used for `COMMIT`.

### 2.5 Seven channel bits, and the one candidate for a column (named, not counted)
New bits: `MV_PREV_GEN` (mv_smooth's store), `MEM_PRIOR`, `MEM_ADV`, `OBJ_STATE`, `WAKE`, `GME_BWD` (or a payload
split of `GME`), `PAIR_STATS` (the FlowSet scalars `f_pair_mfwd/mbwd/disp/bwd_valid`). Four of the reads are
**temporal** — the row reads its OWN output from gen−1 (`PERSIST`, `MEM_PRIOR`, `OBJ_STATE`, `MV_PREV_GEN`).
Today this is implicit inside each producer (the row copies its state forward), exactly like `inertia` reading
`hostPER[gen]`. If R5's seam needs the temporal read DECLARED to derive a cross-submission hazard for the GPU
rows (`mv_smooth` is the only GPU one), that is a `reads_prev_ch` mask — **the one place a reviewer can turn
this map into 1 new column.** The session's judgement: not needed — the seam already orders passes within a
submission, and a gen−1 store written by the previous submission is complete by the fence the stage already
waits (`:1409`, `:1696`); the convention "a row's temporal state is its own, read at entry" is documented on the
channel. Stated so the disagreement has a name.

## 3 · The count and the decision

| Measure | Value |
|---|---|
| **New COLUMNS from the thirteen FLOW rows (the decision-rule number)** | **0** (one named candidate, §2.5, judged unnecessary) |
| New Kind VALUES | 1 (`H`, host pass — §2.1) |
| New ArmId VALUES / `ArmInputs` FIELDS | 3 / 5 (§2.4; R5 step 3b: `PRIOR`, `HOLON`, `BIDIR_OK` — the backward legs' arm is the existing `BWD`, they are not decimated today; `bwd_skipping` is a fifth input) |
| New channel BITS | 7 (§2.5) |
| Invariant relaxations | 1 (a token may drive two rows — `--inertia`, §2.2) |
| Rows whose default is the operator's declared switch | 1 (`mv_consensus` with its own token, default ON — §1 row 13) |
| Passes that are NOT rows (declared elsewhere) | 5 (publish → the ring; `wap_upload` → the 3→5 transport; `fwd_pipeline` → a stage mode read by the arm; the governor → CONTROL; nvofa → a provider PARAM) |
| Findings for R5 that are not schema | 1 (the bwd gme fit's SAD source differs between the CPU and GPU variants, `:1516–1520`) |

**Decision (A3 §4.2 rule, `≤ 1` → proceed): PROCEED to R5.** The growth again went to VALUES in existing columns
(a Kind, three arms, seven bits), not to columns; the two things that looked like columns — a host pass, a
row-vs-mode relation — resolve with a value and an arm input. **Constraints R5 carries from here:** `Kind::H`;
`ArmInputs {…, has_prev, tier, holon_skip, pipelined}` with `HAS_PREV / PRESSURE_LT4 / PRESSURE_LT5`; the bwd legs
as their own rows (`needs BIDIR`) so the fwd/bwd order is rank, not code; `mem_fwd` before `objects` and
`mem_refresh` last by rank; the consensus row with its own token (the operator's switch, default ON); the shared
`--inertia` token as a declared relaxation; `wap_upload`, the publish, the pipeline mode and the governor OUTSIDE
the table. **XR3's residual is discharged by this map: the FLOW stage fits the schema without a column.**

## 4 · Honesty ledger

- The inventory was delegated (one Sonnet agent, read-only, 39 tool reads); it is a claim. The session re-read
  fourteen of its citations first-hand before mapping — `flow.cpp:1604, 1590, 1467, 1488, 1734, 1777`,
  `present.cpp:747, 748`, `cli.hpp:911, 424`, `flow_init.cpp:221, 201`, `core_init.cpp:327`, `flow.hpp:54` — all
  fourteen matched the quoted text exactly. The remaining line ranges are the agent's; R5 re-reads each pass it
  moves.
- The mapping is on paper: no row compiles, no `Kind::H` exists, no arm value exists; the ranks are placeholders
  preserving today's execution order (`:1590 → :1604 → :1712 → :1717 → :1734`), not a design.
- Three judgements a reviewer can disagree with, each named where it lives: the host pass as a Kind VALUE (§2.1;
  disagreeing makes it a column: `exec_domain`), the temporal self-read left undeclared (§2.5; disagreeing makes it
  a column: `reads_prev_ch`), the pipeline mode as an arm input (§2.3; disagreeing makes it a pseudo-row, not a
  column). Two of the three can turn the count into 1 or 2 — still inside "proceed" and "design-and-review".
- What this map does NOT do: it does not decide the gme CPU/GPU representation, the exact ranks, or whether
  `mv_consensus` sits at FLOW rank 90 or as an MVCOND row of stage 5 (STAGE_CONTRACT §2 allows either; the FLOW
  placement follows its "never a P-side ad-hoc pass" rule). Those are R5's entry decisions.
- The delegated inventory cost 274,606 tokens; the session's own reads for this map were the schema, the
  contract and the fourteen checks.

*Made with my soul - Swately <3*
