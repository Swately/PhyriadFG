# COLUMN_CLOSURE_EXPERIMENT — does the LAYERTAB schema converge? (R0 exit gate; A3 §4.2; risk XR3)

> **What this is:** the falsifier of the chosen design's own weakest point (`A3_CHOSEN_DESIGN.md` §5,
> `CANDIDATE_C.md` §7.2): "map TEN more layers of `wap_warp.comp` onto the row schema on paper and count
> NEW COLUMNS — `≤ 1` proceed to R3; `2–3` design + review first; `≥ 4` RE-SELECT toward Candidate A."
> **When:** 2026-09-03, after R0's registry exists (`src/layers/layer_table.def`, 12 rows, 12 params) and
> BEFORE any shader file (`shaders/fg_core.comp`, `shaders/layers/*.glsl`) is written.
> **How:** every gate below was read first-hand in `shaders/wap_warp.comp` (1,336 lines; the line ranges
> cited) and mapped onto the schema of `src/layers/layer_abi.hpp` exactly as it stands at R0. Paper only:
> no row was added to the registry. Status: `measured` (a count, not an estimate).

## 0 · The schema at the start of the experiment (what "new column" is measured against)

`layer_abi.hpp` (R0) — **LAYER columns:** `id, name, stage, rank, kind, default_on, overrides, needs,
req_any, excludes, reads_ch, writes_ch, shadows, arm, group, dominates_ok, help, params(first,count)`;
**per-param:** `name, type, dflt, lo, hi, flag, flag_alias, pflags, ui, help`. **CLI-COMPAT columns**
(exist only to reproduce today's CLI token-for-token; R3 may retire them): `on_flag, off_flag, flag_alias,
pflags`.

**Honest ledger of R0's own growth vs Candidate C §1.4** (from the eight default layers, before this
experiment): C's four extensions (`req_any`, `shadows`, `CH_STASIS`, pseudo-rows = `Kind::X`) plus SIX
more: `excludes` (mv_guided vs mv_edge_snap), `dominates_ok` (the printed justification, graft G3's
`.dominates_ok`), and the four CLI-COMPAT columns (`on_flag`, `off_flag`, `flag_alias`, `pflags`). Two
LAYER columns + four CLI-COMPAT columns. This is recorded as a finding in its own right (§3), separate
from the ten-layer count the decision rule reads.

Channel BITS (`CH_*`) and stage VALUES are NOT columns: adding a bit to `reads_ch` or a value to the
`stage` enum is data in an existing column. They are counted separately below because two of them turn
out to matter.

## 1 · The ten layers, mapped (all non-default under the rig's shipping set)

| # | Layer (push field) | Source lines | Stage · rank · kind | Row(s) | needs / excludes / arm | reads → writes (channels) | Params (flags) | New COLUMN needed? | Notes |
|---|---|---|---|---|---|---|---|---|---|
| 1 | **rescue** (`rescue_on`) | 792–850 | COMPOSE · 230 · F | `rescue` | needs COMMIT; excludes SINGLE_TRACK; arm ALWAYS (gme candidate gated by `L_GME`) | MV_FIELD, PREV, CUR, PERSIST, GME, D_PIXEL → color, **RESCUED** | — (`--no-rescue`) | **no** | Re-samples the pair with 8 neighbour MVs + the gme model, scored by `window_sad()` (a shared helper, 146–163). Publishes `ctx.rescued` (a bool channel like `CH_STASIS`) that `commit` reads to zero `wc`. Today it is NESTED inside commit's `if`; as rows: rank 230 < commit 240 + a channel edge. |
| 2 | **commit** (`commit_thresh`, `commit_real`) + **appearance** (`appear_on`, `appear_band`) | 780–905 | COMPOSE · 240 · F | `commit` | excludes SINGLE_TRACK; arm COMMIT | A_SAMP, B_SAMP, PREV, CUR, RESCUED, MATTE(c_f,c_b) → color, **BLEND** | `thresh` F32, `real` BOOL, `appear` BOOL, `appear_band` F32 (`--commit-thresh --no-commit --commit-real --no-appearance --appear-band`) | **no** — but see §2.2 | `appearance` is a sub-branch that only re-tones commit's own target → a PARAM of `commit`, not a row. `commit` ALSO writes the second accumulator `blend_result` (`:903`) → the channel `CH_BLEND` (§2.2). |
| 3 | **occlusion classification / winner** (`occl_thresh`) + **disoccl_hardpick** | 704–779 | COMPOSE · 210 · F | `occl` (+ `disoccl_hardpick` as a PARAM) | arm BWD | MV_RAW_FWD, MV_BWD(field), MV_FIELD, A_SAMP, B_SAMP, PREV, CUR, **FIELD** → color, BLEND, **OCCL_CLASS** (fwd_ok, bwd_ok, e_fwd, e_bwd) | `thresh` F32 (`--occl-thresh`), `hardpick` F32 (`--disoccl-hardpick`) | **no** | Reads the iGPU Sobel field (`u_field`, `imageLoad` 761–767) → a new channel BIT `CH_FIELD`. Publishes the 4-way class for fill-div (row 4). |
| 4 | **fill-div** (`div_eps`) | 781–806 | COMPOSE · 215 · F | `fill_div` | needs OCCL; arm BWD (+ GME for the model displacement) | OCCL_CLASS, MV_FIELD (4 taps → divergence), GME, PREV, CUR, A_SAMP, B_SAMP → color, BLEND | `eps` F32 (`--div-eps`, `--fill-div`) | **no** | Entered only when `!fwd_ok && !bwd_ok` — a declared read of `CH_OCCL_CLASS`. |
| 5 | **matte** (`matte_on`, `matte_thresh`) + **travel** (`travel_on`) | decision 531–580, override 905–960 | COMPOSE · 200 (`matte_decide`) + 250 (`matte_apply`) · F | TWO rows | needs GME; arm GME_AND_BWD | decide: MV, DISSIDENCE(fwd+bwd, 2 gathers each), A_SAMP, B_SAMP, CUR → **MATTE** (matte_object, matte_occ, dis_fwd, dis_bwd); apply: MATTE, GME, PREV, CUR, BG_WEIGHTS → color, BLEND, **OBJ_COLOR** (the pre-override snapshot 923–924) | `thresh` F32, `travel` BOOL (`--matte --no-matte --matte-thresh --no-travel`) | **no** | Two rows because the shader computes the decision EARLY (needs A/B samples) and applies it LATE (after occl/commit). `travel` changes the occupancy formula → a BOOL param of `matte_decide`. The color cross-check (569–577) reads `L_MV_GUIDED` + `commit.thresh` — a cross-row PARAM read (§2.3). |
| 6 | **crescent** (`crescent_on`) | 936–958 | COMPOSE · 245 · F | `crescent` | needs MATTE; arm GME_AND_BWD | MATTE(dis_fwd, dis_bwd), t → **BG_WEIGHTS** (w_prev, w_cur) | — (`--no-crescent`) | **no** | A layer OF a layer: it only re-weights how `matte_apply` fetches `bg`. Expressed as a producer of `ctx.bg_weights` with the default `(1−t, t)` when the row is off. The `disoccl_commit` sub-branch (944–953) is a PARAM (`commit_w`) of this row. |
| 7 | **contour marriage** (`contour_on`) | 970–993 | COMPOSE · 255 · F | `contour` | needs MATTE; arm GME_AND_BWD | MATTE(occ), OBJ_COLOR, **BG_COLOR**, PREV, CUR → color, BLEND | — (`--no-contour`) | **no** | Needs both layer colors at once → `matte_apply` must PUBLISH `bg` (`CH_BG_COLOR`) and the pre-override object color (`CH_OBJ_COLOR`). Two more channel payloads, no column. |
| 8 | **onepos** (`onepos_on`, `onepos_band`) + **obj_crescent** (`obj_crescent_on`) + the disoccl-commit inside it | 606–690 | **WEIGHT** · 150/160 · F | `obj_crescent` (150), `onepos` (160) | onepos: needs none; obj_crescent: needs MATTE; arm GME_AND_BWD | onepos: D_PIXEL, MATTE(dis_fwd, dis_bwd, matte_object), **WA** → WA; obj_crescent: MATTE → WA | `band` F32 (`--onepos-band --no-onepos`); `--no-obj-crescent` | **no column — but a NEW STAGE VALUE** (§2.1) | These layers rewrite `wa`, the core's A/B blend weight, BETWEEN the core's two displaced samples (Gate 2, 518–521) and its blend (`:637`, `:679`). C §2's `fg_core()` is ONE function containing both → no row can sit between them. The mapping requires the core to split: `fg_sample()` → WEIGHT stage (`float f(float wa, ctx)`) → `fg_blend()`. |
| 9 | **bg_snap** (`bg_snap_on/strength/norm`) + the **disocclusion fill** it arms (`bgs_w`, `bx_w`) | snap ~380–392 (MVCOND); fill 1200–1222 | MVCOND · 28 · F (`bg_snap`) + COMPOSE · 282 · F (`bg_fill`) | TWO rows | needs GME + IGPU_FIELD (a HOST-stage row: `--igpu-field`); arm GME; `bg_fill` needs OCCL too | snap: MV, **FIELD**, GME → MV; fill: PREV, CUR, DISSIDENCE, D_PIXEL → color | `strength` F32, `norm` F32 (`--bg-snap-strength --bg-snap-norm --no-bg-snap`); `band_xfade` F32 (`--band-xfade-strength`) arms `bg_fill` too | **no** | `bg_fill` reads the strength of ANOTHER row (`bgs_w`, `bx_w`) — a cross-row PARAM read (§2.3), expressible as `needs` + reading the other row's UBO field by its generated alias. |
| 10 | **multicand medoid** (`mc_on`, `mc_nperturb`, `mc_perturb`, `mc_disp`, `mc_edge`) | 1078–1125 | COMPOSE · 270 · F | `multicand` | excludes SINGLE_TRACK, excludes SELECT (it REPLACES the select); arm ALWAYS | A_SAMP, B_SAMP, BLEND, PREV, CUR, MV_SAMPLE (perturbed re-warps), **FIELD**, WA → color | `nperturb` I32, `perturb` F32, `disp` F32, `edge` F32 (`--multicand … --no-multicand`) | **no** | An ALTERNATIVE to the default `select` row (the soft/hard gate, 1126–1185, itself a row at rank 260 reading `warp_ok`, `d_tile`, `SAD`, MEMBER, BLEND); `excludes` expresses "one of the two". Its `wa`-based perturbed candidates read `CH_WA` from the WEIGHT stage. |

**Ten more layers: 0 new columns.** Everything above lands in existing columns; what grew is the SET of
channel bits (11 new payload channels: `MV_FIELD`, `FIELD`, `BLEND`, `RESCUED`, `OCCL_CLASS`, `MATTE`,
`BG_WEIGHTS`, `OBJ_COLOR`, `BG_COLOR`, `WA`, `MEMBER`/`D_TILE` for the select) and the STAGE enum (one
value, §2.1).

Also mapped in passing (not in the ten, recorded so the next batch is not blind): **camera-twarp**
(`cam_lead`, 292–307: rewrites `uv` before everything → a second new stage value, `UV`, `vec2 f(vec2 uv, ctx)`),
**extrap / ASW** (`extrap`, `predict_p2`, 1230–1245: a COMPOSE OVERRIDE row reading `t > 1` from the clock
and `MV_SAMPLE`; `predict_p2` is its param), **ts_smooth** (1250–1258: COMPOSE row, excludes SINGLE_TRACK,
reads a new channel `PREV_OUT` = the previous output image; today's "MUST be the LAST modification" is a
rank), **band_xfade** (arms `bg_fill`, param), **blend_solo** (1332: an INSTRUMENT-plane OVERRIDE row at the
highest rank), **soft_gate / member_commit / commit_default** (params of `select` + a `member` row
publishing `CH_MEMBER` from the dissidence masks, 1000–1012), **stasis** already a row. None needs a column.

## 2 · The three findings that are NOT columns but change R3's design

### 2.1 A WEIGHT stage between the core's samples and its blend (from onepos / obj_crescent / single_track)

The core as frozen in `A0` M2c and written in `CANDIDATE_C.md` §2 is one function: samples → Gate 2 →
`wa = 1 − t` → blend → `CoreOut`. Three shipping-relevant layers rewrite `wa` after the samples and before
the blend: `onepos` (655–690), `obj_crescent` (606–655) and — under the DEFAULT — `single_track`'s
`wa_eff = 0` collapse (`:678`, the first of its four `shadows`). The schema expresses this only if the core
splits into `fg_sample()` (Gate 1 evidence, the two displaced samples, `d_pixel`, the tile reduction) and
`fg_blend(wa)`, with a **WEIGHT** stage (`float f(float wa, in LayerCtx ctx)`, ranks 100–199) between them.
Consequence for the frozen contract: the core is still 6 parameters / the same math, byte-identical when no
WEIGHT row is on (`wa = 1 − t` is the stage's identity); it is TWO functions instead of one. **Bonus:** the
`SH_WA_EFF_ZERO` shadow of `single_track` becomes an ordinary WEIGHT row (`single_track_wa`, rank 190,
returns 0) — one of the four warts dissolves into the schema. Candidate A's design had exactly this slot
(`WEIGHT`, `CANDIDATE_A.md` §3.4 item 3), which is evidence the split is natural, not bespoke.

### 2.2 The second accumulator (`blend_result`) is a channel, and the select is a row

The shader carries TWO colors through its chain — `warp_result` and `blend_result` (the fallback: the
`(1−t)/t` crossfade, or `cur` under single_track) — and `select` (1126–1185) picks between them by the
gates. Seven layers write BOTH (single_track base 692–695, occl winner 776–777, fill-div 801–805, commit
903–904, matte 993–998, contour 990–991). The schema's rule "a COMPOSE row writes its return value only"
(`stage_writes_legal()`) is too strict by one channel: COMPOSE rows may ALSO write `CH_BLEND` (a `ctx`
field), and `select` is a default-on COMPOSE row (rank 260) reading `warp_ok`/`d_tile`/`SAD`/`MEMBER` and
`CH_BLEND`. G1 (single-writer store) is untouched: the store still has one source, the COMPOSE chain's
return. This is an invariant relaxation + a `CoreOut.blend` field, not a column.

### 2.3 Cross-row parameter reads (three sites)

`matte_decide` reads `L_MV_GUIDED` and `commit.thresh` (569–577); `bg_fill` reads `bg_snap.strength` and
`band_xfade` (1200–1222); `rescue` reads `commit.thresh` (the acceptance bar, 866). The generated per-row
`#define` aliases already make every UBO field addressable; the schema needs no column — only the rule
that a cross-row read is DECLARED (a `needs` bit on the row whose param is read), so the generator can
check it. Recorded because it is the coupling the stage model most wants to forbid and cannot yet.

## 3 · The count and the decision

| Measure | Value |
|---|---|
| New COLUMNS from the ten layers (the decision-rule number) | **0** |
| New stage VALUES | 2 (`WEIGHT` needed by 3 of the ten; `UV` by camera-twarp, outside the ten) |
| New channel BITS / payloads | 11 |
| Invariant relaxations | 1 (COMPOSE may write `CH_BLEND`) |
| Core-contract refinements | 1 (`fg_core` → `fg_sample` + WEIGHT + `fg_blend`; same 6 parameters, same math) |
| Gates that became PARAMS of a row instead of rows | 7 (appearance, travel, disoccl_commit×2, soft_gate, commit_default, predict_p2) |
| R0's own column growth from the eight default layers (recorded, not part of the rule) | 6 (2 LAYER: `excludes`, `dominates_ok`; 4 CLI-COMPAT) |

**Decision (A3 §4.2 rule, `≤ 1` → proceed): PROCEED to R3.** The schema converged on the ten: the growth
moved from COLUMNS (0) to VALUES in existing columns, which is what a converging schema looks like. Two
things go into R3's design as constraints, not options: the WEIGHT stage with the core split (§2.1) and the
`CH_BLEND` channel with `select` as a row (§2.2). The cross-row reads (§2.3) get a declared-`needs` rule in
the generator.

**What would still falsify LAYERTAB later (the residual, stated):** if the perceptual layers (S5) — the
holon family in `flow.cpp`, which the shader does not contain — need columns the FLOW stage does not have
(they were not part of this experiment; `flow.cpp` is the next map, at R5).

## 4 · Honesty ledger

- Line ranges are from the 2026-09-03 read of `wap_warp.comp` (E1 tree, `c043780` + working tree); the
  exact rank numbers above are placeholders chosen to preserve today's order, not a design.
- The mapping is on paper: none of these rows compiles yet; the WEIGHT-stage split is a reasoning about
  the code, verified only by reading (`:637` `wa`, `:678` `wa_eff`, `:655–690` onepos).
- The "7 gates became params" is a judgement (a gate that only changes another row's formula), stated so
  a reviewer can disagree per gate; disagreeing turns a param into a row, never into a column.
- R0's own 6-column growth is the honest part of the answer: the CLI-compat family exists because parity
  with the hand parser was made a gate; R3 may retire it when the hand cases go.

*Made with my soul - Swately <3*
