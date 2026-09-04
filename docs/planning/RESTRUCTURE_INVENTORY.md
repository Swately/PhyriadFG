# RESTRUCTURE arc — E0 pre-flight inventories

Companion to [`RESTRUCTURE_PLAN.md`](RESTRUCTURE_PLAN.md) /
[`RESTRUCTURE_RISK_REGISTER.md`](RESTRUCTURE_RISK_REGISTER.md). Produced 2026-08-31 under
`SUBAGENT_DELEGATION_PROTOCOL`: I1–I4 are light-model output (provenance per section), each
spot-verified first-hand by the supervising session (§0) before any stage may consume it (gate G0,
risk DR1). Line numbers reference commit `c043780` + the operator's uncommitted `cli.hpp` change.

## §0 · G0 spot-check record

| Inventory | Model | Samples checked | Result |
|---|---|---|---|
| I1 (init-seq map) | haiku | 5 (both directions) | **Section map usable; two systematic defects found and corrected below.** (1) OMISSION: ranges 501–622 and 1220–1334 missing from the delegate's table — inventoried first-hand below. (2) MISATTRIBUTION: the delegate lists resource variables as "declared" in their init sections; first-hand check shows declarations live in the 502–622 block (`VDev A{},B{},G{}` at line 507, `use_wap` at 522), NOT at the init sites (e.g. "Devices" 845 initializes, does not declare). |
| I2 (present.cpp captures) | sonnet | 5 (both directions) | **Passed 5/5** (preamble `cfg`@58 + `cmdBridge`@222; the `cmdBridge` shadow confirmed at 724/735 with its own source comment; `bslot`@867 + `async_front/inflight`@876–877; `bridge_present` call sites 1389 and 1453; reverse check at ~1000: lambda-locals correctly excluded, `cfg` listed). |
| I3 (flow.cpp captures) | sonnet | 5 (both directions) | **Passed 5/5** (preamble `cfg`@476 + `mvw_f/mvh_f`@492–493; `obj_label` declared 787; `object_repair` called at 1607; `ObjCluster` struct inside `run_flow` at 791; reverse check at 1462–1468: `pressure_tier`/`holon_pair_ctr` listed, `g_gov_floor` correctly excluded as global). |
| I4 (flag triage) | haiku | 5 (both directions) | **Classification passed 5/5** (`--exit-after` alias confirmed at `cli.cpp:378`; `--predict` block confirmed absent from `main.js`, 0 grep hits; `--windowed` deprecation confirmed at `cli.cpp:482`; `--mv-edge-snap` confirmed CLI-only, `cli.cpp:364`). **Line citations NOT reliable one-by-one:** 1/5 wrong (the delegate cites `main.js:368` for both `--band-xfade` and `--vblend`; 368 is `--no-vblend`). Use I4's buckets, re-derive any line before acting on it. |

## §1 · I1 — `main.cpp` init-seq section map (corrected)

**Structural finding (first-hand, decisive for E1):** lines 502–505 state the file's own contract:
*"From here: ALL resource variables declared BEFORE any goto. The cleanup block at `done:`
releases everything that was initialised (checks != null)."* The init-seq is a
**goto-cleanup pattern**: declarations are hoisted into one block (502–622) so `goto done` never
jumps over an initialization. E1's ownership structs MUST preserve both halves of that contract:
zero-initialized members (the structs replace the hoisted declarations) and a `done:` cleanup that
still null-checks every member. This is the concrete mechanism behind risks CR2/FR1.

Corrected map (delegate's table + the two first-hand-inventoried holes, marked ★):

| Range | Section | Destination (E1) | Notes |
|---|---|---|---|
| 208–263 | --gpu-priority lever 1 (D3DKMT) | stays in `main()` | process-level, pre-init |
| 263–270 | WGC state | capture/ | tiny; rides with capture init |
| 270–389 | Capture init (DDA route, NAT/WW dims) | capture/ | declares route/NAT_W/WW at 345–349 (verified) |
| 389–443 | Captured-monitor refresh | capture/ | |
| 443–460 | Vulkan instance | core/ | |
| 460–501 | Physical device selection | core/ | LUIDs, pA/pB/pG |
| ★ 502–622 | THE DECLARATION BLOCK (pre-goto hoist) | becomes the ownership structs | `VDev A{},B{},G{}` (507), `have_igpu/use_upscale/use_igpu_convert` (520), `use_wap` (522), ring constants + `cap_slots` (~560–575), host-pointer arrays (hostR/hostI/…), HBuf arrays |
| 622–733 | gme-gpu device-B dissidence bridges | flow/ | |
| 733–845 | PresentSurface bridge | present/ | |
| 845–1073 | Devices (vdev_create A/B/G, capability gates) | core/ | initializes the 507-block VDevs |
| 1073–1193 | Host bridge (+ ring auto-size 1080–1102) | core/ | |
| 1193–1220 | --ingest-async raw ring | capture/ | |
| ★ 1220–1334 | dissidence host imports + gme-gpu B-side readback (inside the host-bridge alloc loop) | core/ (rides the host-bridge section) | `hostDISB` import at ~1284; gme-gpu model readback ~1287+ |
| 1334–1395 | Images | core/ | |
| 1395–1628 | WGC backend init | capture/ | |
| 1628–1648 | Convert pipeline (A) | capture/ | |
| 1648–1701 | OpticalFlowPipeline (B) | flow/ | |
| 1701–1744 | --nvofa provider | flow/ | |
| 1744–1761 | MV smoothing | flow/ | |
| 1761–1771 | OpticalFlowPipeline (A) | flow/ | |
| 1771–1814 | Upscale pipeline (G) | present/ | |
| 1814–2117 | warp-at-presenter pipeline | warp_blend/ | the largest single section (303 lines) |
| 2117–2153 | MV median scratch | warp_blend/ | |
| 2153–2213 | gme-gpu affine-fit pipeline set | flow/ | |
| 2213–2267 | Command buffers/fences/semaphores | core/ | |
| 2267–2326 | Producer-side D3D11 bridge + VK import | present/ | |
| 2326–2410 | --async-present second slot | present/ | |
| 2410–2441 | --real-fast-path co-arm guard | stays in `main()` (validation, not alloc) | verified: config gating only |

Provenance: delegate table (haiku, 2026-08-31) + first-hand corrections by the supervising
session (the ★ rows and the declaration-block finding). The per-section "key locals" lists in the
delegate's raw output are RETAINED ONLY as hints — each E1 ownership struct is derived from the
source at extraction time, never from the hint list (DR1).

## §2 · I2 — `present.cpp` extractable-lambda captures (delegate: sonnet; spot-checked §0)

Alias preamble: lines 56–235. E2-decisive findings (each verified first-hand at spot-check):

1. **`bridge_present` is a shared sink**: called by `bridge_present_src` (692),
   `wap_warp_present` (1389, non-async path only) and `rfp_present` (1453, non-async path only).
   `wap_upload` calls no sibling. → **E2 extraction order: `bridge_present` first**, then
   `bridge_present_src` + `wap_upload` (independent), then `wap_warp_present` + `rfp_present`.
2. **The `cmdBridge` shadow** (`wap_upload`): line 724 reads the ALIAS `cmdBridge` (preamble 222)
   to pick `ucmd`; line 735 then declares a local `VkCommandBuffer cmdBridge = ucmd;` whose own
   comment says "shadow: keep the large record body below textually unchanged". The binding
   struct must carry the ALIAS; the shadow line moves verbatim inside the function.
3. **`bslot`-derived locals** (`wap_warp_present` 927–930): `cmdBridge/fBridge/bridge_img/
   bridge_mem` there are lambda-locals derived from the captured `bslot[2]` (declared 867) — NOT
   the same-named preamble aliases. Binding must pass `bslot`, never those four names.
4. `kOverlayW/kOverlayH` are namespace-scope `constexpr` (`present.hpp:25–26`) — no capture needed.

Capture counts per lambda (delegate tables, retained verbatim in the delegate output; the binding
structs are derived from those tables at E2 time, re-checked row-by-row per CR1):

| Lambda | ALIAS | LOOP-LOCAL | OTHER-LAMBDA | Unverified |
|---|---|---|---|---|
| `bridge_present` (466–507) | 4 (`cfg bridge_nt bridge_w bridge_h`) | 7 (`ph_* ps_account ra_surface surface_ready`) | 0 | 0 |
| `bridge_present_src` (654–693) | 10 | 8 | 1 (`bridge_present`) | 0 |
| `wap_upload` (713–836) | ~40 (the WAP image/import pairs) | 2 (`wap_mvw wap_mvh`) | 0 | 0 |
| `wap_warp_present` (879–1402) | ~45 | ~25 (pacing EMAs, qdump/outdump state, `bslot` family, overlay state) | 1 (`bridge_present`) | 0 |
| `rfp_present` (1416–1458) | 10 | 6 | 1 (`bridge_present`) | 0 |

Full row-level tables: delegate output (session artifact, 2026-08-31); each row consumed at E2
is re-verified against source when the binding struct is written (DR1 — counts above are the
delegate's, spot-verified only at the §0 samples).

## §3 · I3 — `flow.cpp` holon-family captures (delegate: sonnet; spot-checked §0)

Alias preamble: lines 476–590. E3-decisive findings (verified first-hand at spot-check):

1. **Dependency chain**: `consume_wap` directly calls ALL four others — `object_repair` (1607
   fwd, 1721 bwd), `mem_advect` (1592), `mem_merge` (1593, 1714), `mem_refresh` (1736). None of
   the four calls back. → **E3 extraction order: the four leaves first, `consume_wap` last.**
2. **`consume_wap` additionally needs** three flow-local helper lambdas visible:
   `flow_submit_nowait` (632), `flow_submit_q2_chain` (650), `flow_downsample` (706) — E3 either
   extracts them too or passes them as callables. This WIDENS E3's scope vs the plan's first cut;
   the plan carries the note.
3. **Struct definitions live inside `run_flow`**: `ObjCluster` (791), `ObjSlot` (807), `WakeRec`
   (826) — they move to `holons.hpp` with the functions.
4. The `kObj*/kChamf*` constants + `kPriorDecay` are `static constexpr` in `flow.hpp`
   (33/54/68…) — compile-time, no capture. `kTier4DwellPairs` is a run_flow LOCAL (694).

Capture counts per lambda:

| Lambda | ALIAS | FLOW-LOCAL | OTHER-LAMBDA | Unverified |
|---|---|---|---|---|
| `object_repair` (885–1245) | 3 (`cfg mvw_f mvh_f`) | 8 (the obj_* scratch vectors, 787–804) | 0 | 0 |
| `mem_advect` (1259–1274) | 2 | 2 (`mem_adv mem_prior`, 816–817) | 0 | 0 |
| `mem_merge` (1290–1310) | 3 | 0 | 0 | 0 |
| `mem_refresh` (1324–1380) | 2 | 5 | 0 | 0 |
| `consume_wap` (1396–1809) | ~60 (the F→P channel `f_pair_*` block 557–568, host bridges 532–551, stats atomics 572–585, devices/pipes) | ~35 (EMAs, tier state, obj slots, the 3 helper lambdas) | 4 (all leaves) | 0 |

Full row-level tables: delegate output (session artifact, 2026-08-31); same DR1 rule as §2.

## §4 · I4 — the 84 CLI-only flags, triaged

Mechanical base (first-hand, computed): 257 distinct `--flags` in `cli/`, 173 in the UI model,
84 CLI-only, **0 UI-only**. Delegate triage (haiku), classes spot-verified 5/5, line citations
not individually reliable (§0):

- **A · UI-covered-indirectly: 68** — the UI's `switch-off` entries emit the `--no-X` form of a
  default-ON flag (so the positive form needs no entry), plus aliases the UI reaches by the other
  name (`--exit-after`≡`--duration`, `--real-fast-path`≡`--rfp`, `--present-fp16`≡`--hdr`) and
  legacy no-ops (`--commit-real`, `--commit-warp`, `--overlay`, `--present-surface`).
- **B · Deliberate CLI-only: 10** — meta/diagnostic/ablation: `--help --list-monitors --qdump
  --mv-audit --blend-solo --gov-util-floor --no-decimate --output-clock --st-no-stasis
  --windowed`(deprecated, errors out).
- **C · LIKELY REAL DRIFT: 6** — `--mv-edge-snap`, `--mes-sim`, and the predict block
  (`--predict --predict-e --predict-p2 --no-predict`). First-hand note: the predict block's own
  help text calls it a measured NO-GO *as a latency feature* kept as an opt-in perceptual mode
  (`PREDICT_MODE_PLAN.md`) — its UI absence may be deliberate. **Operator decision, not a bug
  list:** expose in the UI, or record as intentional in the UI model's comments (either closes
  the drift).
- **D · Unverified: 0.**

Sum check: 68+10+6+0 = 84 ✓ (computed).
