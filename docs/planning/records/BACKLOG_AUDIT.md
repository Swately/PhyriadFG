# BACKLOG AUDIT — what is left to finish · 2026-09-04

> **What this is.** A full sweep of every plan, gate record and code marker in the repo, with each
> claimed item checked against the tree by an independent skeptic before it was written down. It exists
> because the answer to "¿qué falta?" was scattered across 136 documents, several of which disagreed
> with the repo. **Read this with [`../ACTION_PLAN.md`](../ACTION_PLAN.md)**: the spine is the live
> pointer and node states; this is the inventory behind it.
>
> **Method.** Two passes. Pass 1: eight parallel readers (spine · convergence trio · motion-truth trio ·
> gate records · AAP · side plans · research shelf · code markers) → 184 candidates → 182 after dedup →
> 60 verified by a skeptic each → synthesis → completeness critic. Pass 2: the remaining 122 verified
> (Sonnet, low effort — mechanical grep/read checks; the judgement steps stayed on the session model) →
> 115 survived, 104 of them fresh → consolidation → a second critic. **7 + 7 items were REFUTED** as
> stale or already discharged and are listed in §8 rather than dropped. Everything below is quoted from
> a file or a command; where a number was not measured, it says so.
>
> **State at the audit.** HEAD `99dd28d`, branch `analysis/0.3.0-quality-push`, tree clean before this
> audit's own doc edits.
>
> ## ⚠ STALENESS BANNER (added 2026-09-06) — this is a SNAPSHOT, not a live tracker
>
> **This document was true on the morning of 2026-09-04 and parts of it stopped being true the same
> afternoon.** MOTION_TRUTH **T2, T3, T4 and T5 all CLOSED on 2026-09-04**, hours after the audit was
> written, each with a gate record in THIS directory (`S2_T2_GATE.md`, `S2_T3_GATE.md`, `S2_T4_GATE.md`,
> `S2_T5_GATE.md`) and with the tools committed at `tools/motion_truth/`. **M1 IS MEASURED** for the
> shipping default, with `r`, at [`../../evidence/MOTION_TRUTH_BASELINE.md`](../../evidence/MOTION_TRUTH_BASELINE.md).
> Rows A0′, A6 and the S2.T2–T5 lines below say the opposite and are **superseded** — they are kept as
> the record of what was true when the sweep ran, per the container's never-delete rule.
>
> A session on 2026-09-06 read those rows, believed them over the gate records in the same folder, and
> wrote "MOTION_TRUTH T2–T5 have no code" into five live documents and a commit message
> (`docs/LEARNING_LOG.md` P-018). **The live state is the ACTION_PLAN node states and the gate records;
> this file is dated evidence and must be read with its date.**

---

## 1 · The honest bottom line

The objective — *a pure core that multiplies frames correctly without patches* — is **far**, and it is
far in two independent directions:

- **Structure.** The core has not been made pure at all. `shaders/fg_core.comp` does not exist,
  `shaders/layers/` does not exist, the layer registry is still a **shadow parser** that prints
  `R0: registry SHADOW, wap_warp.comp drives the product` at every launch, and `CorePush` — the 20-byte
  core contract, size-locked by `static_assert` — is **declared and never used**. Four of six restructure
  stages (R3, R4, R5, R6) are unbuilt; R7 waits on a `--legacy-warp` flag that does not exist because
  R3 creates it.
- **Proof.** **M1 — whether the generated motion is actually correct — has never been measured for any
  path.** The multiplication *count* is proven (`records/R1_GATE.md`: 0.2501 source frames of content
  step per present, the 4× ladder is real). Its *correctness* is not. The four instrument phases that
  would measure it (T2–T5) are `designed` with zero code. Even a perfect R3 would land unable to prove
  the thing the operator asked for.

**The single blocker in front of everything structural is S2.T6's sub-pixel deadzone.** Above 4 px of
motion the CPU oracle is right (k = 0.955, corr 0.97); below 0.5 px the shader moves essentially nothing
while the oracle moves content. Six causes are refuted by number. The record leaves open **which side is
wrong** — and if it is the shader, this is not an instrument bug but a defect in exactly the motion
fidelity the objective names.

**The single next action that moves it most: build S2.T2, the non-periodic marker zoo.** It is unblocked
today, it is on M1's critical path regardless, and it is the only thing in the repo that can answer the
T6 gate's own largest unmeasured question — whether the deadzone exists outside the 24 px lattice that
produces the suspicious MV pattern. It is a better bet than another round of attacking the deadzone on
the zoo that may be causing it.

---

## 2 · The critical path

| # | Item | State | Blocked by | Scale |
|---|---|---|---|---|
| A0 | **S2.T6 — close the sub-pixel deadzone** | built, gate NOT passed | nothing; this is the live work | medium as an investigation, unbounded as a fix |
| A0′ | ~~**S2.T2 — the non-periodic marker zoo**~~ **SUPERSEDED 2026-09-04** — built and gated (`S2_T2_GATE.md`) | `designed`, zero code | nothing | medium |
| A1 | **S4.R3 — `fg_core.comp` + the fused rows** | not started | A0 (its M4 gate needs a trustworthy oracle) | large |
| A2 | **S4.R4 — stage 6 PRESENT extracted** | not started | **not blocked** — sequencing preference only | medium–large |
| A3 | **S4.R5 — `FlowSet`/`FlowRing`, holons as rows** | not started | **not blocked** — but its scope overlaps R4's | large |
| A4 | **S4.R6 — stages 1–2 named (`capture/`+`ingest/`)** | not started | R5 | medium |
| A5 | **S4.R7 — `--legacy-warp` out of the default** | not available | a four-deep chain, none of it lifted | medium |
| A6 | ~~**M1 — the motion baseline**~~ **SUPERSEDED 2026-09-04** — T2–T5 built; M1 MEASURED with `r` on the shipping default (`evidence/MOTION_TRUTH_BASELINE.md`). What remains is M1 on the `--fg-core` path: a RUN, not a build. | never measured, any path | T2 → T3 → T4 → T5 | large |

**Corrections this audit made to the path itself:**

- **R4 and R5 were labelled `blocked (R1)` and `blocked (R2)` while R1 and R2 are `done` with passing
  gate records.** Both are `startable`. The real constraints are the STAGE_CONTRACT order table (a
  preference), the register's rule against *committing* while MR-1/MR-2/MR-7/CR1 are `open`, and R5's
  scope overlapping R4's (`present.cpp:713-836`, `:779`). Corrected in the spine.
- **R4 hides an unbuilt tool.** G-R4 requires `--tdr-test`; `grep -rn "tdr-test" src/` returns zero.
  Two other required flags are equally absent: `--pacing` (MR-4's opt-in list) and `--async-queue`
  (MR-2's S7 arm).
- **R5's paper half is unblocked today and needs no code:** mapping `src/flow/flow.cpp`'s holon family
  onto the LAYERTAB schema — the exercise R0 did for ten shader layers. It is **the only place
  LAYERTAB's "0 new columns" claim is still falsifiable** (XR3's accepted residual).
- **R7 needs a git tag that does not exist.** `CONVERGENCE_MASTER_PLAN.md:275` names "the pre-R3 binary,
  tagged in git as the A/B reference". `git tag -l` → four tags, newest `v0.2.2-experimental`, **26
  commits back**. The tag must be cut **before R3 changes the default path**, not at R7.
- **Three R3 design constraints, not two.** The column-closure experiment imposed three; the spine and
  the master plan carried two, and the third (a declared-`needs` rule for cross-row parameter reads)
  survived only inside the experiment doc — `grep -rn "cross-row" docs/ src/ tools/` hit one file.
  All three are now attached to R3's design section. Note the second contradicts the section's own
  sentence: the eight rows of `CANDIDATE_C` §4.1–4.8 contain **no `select` row and no `CH_BLEND`**.

---

## 3 · Instrument debt

### Blocks the critical path

| Item | State | Blocks |
|---|---|---|
| **S2.T6 deadzone** | built, gate NOT passed | R3's M4 veto |
| **S2.T2** `marker_zoo.py` + `positions.csv` + the 16-bit frame-ID barcode | ~~`designed`, zero code~~ **BUILT + GATED 2026-09-04 — `S2_T2_GATE.md`** | T4/T5 → M1 → R7, **and T6's generality question** |
| **S2.T3** the on-screen player at a fixed 60/120 fps | ~~`designed`, zero code~~ **BUILT + GATED 2026-09-04 — `S2_T3_GATE.md`** | T4/T5 |
| **S2.T4** `marker_extract.py` + its two self-tests | ~~`designed`, zero code~~ **BUILT + GATED 2026-09-04 — `S2_T4_GATE.md`** | M1 |
| **S2.T5** `motion_report.py` + the DI-3 two-run baseline | ~~`designed`, zero code~~ **BUILT + GATED 2026-09-04 — `S2_T5_GATE.md`** | M1 |

`tools/motion_truth/` does not exist. **Do not mistake the existing zoos for T2:** `A0_FROZEN_OBJECTIVE`
forecloses it — `prep_zoo_sequence.py`'s presets "are NOT that zoo and are not extended to become it",
and `ball_zoo.ps1` is a cadence bench with "one ball, no frame index, no analytic export".

### Would strengthen it

- **`tools/capture_dump/`** — 520 lines with its own README, **no CMakeLists, in no build script**, yet
  `STAGE_CONTRACT.md:77` names it as stage 1's isolation test.
- **`bench/`** — three tracked units (`nvofa_bench.cpp`, `copy_bench/`, `gme_fit_bench/`), **no
  `CMakeLists.txt` anywhere under `bench/`** and unreferenced by the root CMake or either `.bat`.
  Uncompilable by any checked-in mechanism.
- **`FG_QUALITY_BASELINE.json` does not exist anywhere in the repo.** The T0 record says it "was not
  re-locked"; the stronger fact is that the scorer's README links it five levels up, escaping the repo
  root, and neither `docs/framework/` nor `docs/evidence/` exists. Every baseline number in that README
  is historical, measured against the catalog lineage.
- **The `--qdump` corpus describes the SYNCHRONOUS present path only** (`resolve_config` auto-disables
  `--async-present` for the run). No motion-truth number will describe the shipping async path until an
  async-present dump exists — explicitly out of Tier-1 scope.

---

## 4 · Gate debt

Nineteen distinct obligations, all quoted from the records' own honesty ledgers. Nothing here is a hidden
failure; it is disclosed debt that nobody has scheduled.

**`S2_T6_GATE.md` — the oracle (the most consequential group)**
1. Deadzone generality on non-periodic content, and the edge **located** rather than bracketed (the
   0.5–1.0 px band holds 97 and 67 pixels). Blocked on T2.
2. Whether `k` depends on `t` — the per-triple spread is 0.447–0.797. A per-`t` refit over the existing
   corpus; cheap, not started.
3. **The reference was validated against the shader's TEXT, never a second implementation.** Every
   number in the diagnosis inherits one reading of `wap_warp.comp`; a shared misreading is invisible to
   all of them — and "which side is wrong" is exactly what a single implementation cannot settle.
4. `--fit`'s gradient mask (`|∇| > 20`) and the 8-pixel disagreement threshold are chosen, not derived.
5. The **narrowed-M4 option** (score only genuinely moving content, where k = 0.955) is explicitly left
   undecided. It is a decision, not work.

**`S2_T0_T1_GATE.md`**
6. The scorer baseline was never re-locked (see §3).
7. The dump's per-tick cost was never timed — restated in T1b (the sampler's own cost) and T1c (the
   record's disk-footprint pressure). One obligation, three records.

**`S2_T1B_GATE.md`**
8. Content coverage never measured; only phase and ring slot were.
9. The sampler's 8-tick gap and 64-skip escape are chosen constants no measurement justifies.
10. Phase resolution is capped at four phases by the refresh ratio — an R3 corpus-design choice.

**`S2_T1C_GATE.md`**
11. The R8 masks and the RGBA16F candidate plane are validated by **size only**; a wrong-but-same-size
    plane passes.
12. The armed-feature→binding map in `check_qdump_plus.py` is a hand reading of the shader's gates with
    **nothing enforcing it against drift** — the record calls it the weakest link, and it has already
    been wrong once (commit `cda2a7f` fixed it). It goes live the moment R3 changes a gate.
13. The corpus never exercises the edge-snap MV path.

**`R0_GATE.md`**
14. MR-4's CSV byte-diff was not run. The real item is an unmade decision: the telemetry has timestamps
    and derived rate columns, so a raw byte-diff can never be zero by construction and no normaliser
    exists. Either the instrument reduces to genuinely discrete columns, or MR-4's instance is formally
    re-scoped. **Must be settled before R3's instance comes due.**
15. **Cascade parity is PRE-cascade only.** `--no-warp-at-presenter`, `--no-gme` and `--no-bidir` are
    not registry rows; `needs` is non-zero on exactly one row (INERTIA) and **no code applies it**. R3
    must admit the cascade drivers into the registry, then resolve `needs` — mirroring is insufficient.
16. One parity case untested by construction (a malformed value exits before parity).
17. M2b and M2c never measured; the AAP's substitute instrument for M2b (SPIR-V variant diff + Nsight)
    has never been run.

**`R1_GATE.md`**
18. **No pre-vs-post bit-parity result exists**, and the check as written is **unrunnable retroactively**
    — the pre-extraction tree had no per-tick ring snapshot for the replay to read. Two exits, both open:
    back-port the snapshot onto `c043780` (which validates T5, not the extraction), or reclassify XR14
    `mitigated` → `accepted` with the residual, rationale and accepter recorded. **This is a live
    decision, not a closed disclosure.**

**`R2_GATE.md`**
19. Zero steady-state allocation was asserted structurally, **never counted** — and the `execute()` it
    would instrument is on the **opt-in** arm, so a soak of the default binary never enters it. Plus:
    the Vulkan 1.3 / `synchronization2` device-creation change ships in the **default** with only a 60 s
    soak; and the `--sg-barriers` A/B ran on `ball_zoo` only — **no real-game run, no operator-eye
    verdict**. The default flip is three things (soak + real game + retiring the hand arm) and is
    **not** downstream of the deadzone: it is the one substantial item a real-game session could close
    independently.

**Cross-cutting:** M1 has never been measured; R0, R1 and R2 all passed with it listed blocked; XR9 is
precisely the risk of closing a gate on a proxy.

---

## 5 · Shipped-but-unfinished code — live in the product today

These are not backlog. They are in the binary.

| Site | What ships half-done |
|---|---|
| `src/cli/cli.cpp:390-391`, `cli.hpp:850,853` | `--fdrop-quiet-ms` / `--fdrop-k` parse, clamp and store; **zero consumers repo-wide**. The CLI itself prints "PARSED, LOGIC NOT IMPLEMENTED." Two flags the operator can set that do nothing. |
| `src/cli/cli.hpp:972-976` | `nvofa_cost_scale` and `nvofa_sadz_scale` are both marked "PLACEHOLDER — needs eye-calibration". The NVOFA path ships on two uncalibrated constants. |
| `src/flow/flow_init.cpp:93` | `--nvofa` + `--flow-scale>1` silently falls back to the classical OFP: "unit reconciliation is a follow-up". |
| `src/cli/cli.hpp:958-960` | `--flow-scale auto` picks the divisor **once at init**; the runtime re-pick is a follow-up. |
| `src/core/core_init.cpp:141-143` | `--force-single-gpu` **hard-refuses** on any GPU with one graphics queue — the degraded mode "is not yet shipped". A whole GPU class cannot run that mode. |
| `src/present/present.cpp:1228-1233` | `--afill` under `--warp-scale N>1` reads only the top-left corner of the contour field → misaligned tint. Diagnostic overlay only. |
| **`shaders/wap_warp.comp:1312-1313`** | **KNOWN LIMIT in the generation core:** content moving exactly one texture period per pair aliases `d_zero ≈ 0` — "the same two-frame ambiguity the SAD matcher has". Documented, untracked, no fix planned. **See §9 — this may not be unrelated to the deadzone.** |
| `src/layers/layer_abi.hpp:110-117` | `CorePush` declared, `static_assert`ed at 20 bytes, and **never used**. |
| `src/layers/layer_registry.cpp:189` | Prints at every launch: "R0: registry SHADOW, wap_warp.comp drives the product". Both parsers run. |
| `CMakeLists.txt:135-141` | The shipping build **regenerates 8 GLSL/offset files on every compile that nothing includes** — dead work, not merely dead output. |
| `tests/clock/test_phase_clock.cpp:249` | The replay arm — R1's bit-parity oracle — **SKIPs by default and exits 0** with no arrival-log, and no fixture is checked in. |
| `src/cli/cli.hpp:215` | `ingest_async=false`; the default-flip soak was never run. |

---

## 6 · Repo-level facts nobody had written down

- **There is no CI and no CTest.** No `.github/`, no `.gitlab-ci.yml`; `enable_testing`/`add_test` count
  across every `CMakeLists.txt` is **0**. `pfg_seam_test` (R2's 148 checks) and `pfg_clock_test` are
  built and **never run automatically**. An unwired test is a test that stops being run.
- **The branch has no upstream.** `git rev-parse @{u}` → "no upstream configured", so a bare `git push`
  fails. All 10 commits and all 7 gate records live on one local branch. Two remotes are configured
  (`origin` = Swately/PhyriadFG, `chlmateus`).
- **Tags are 26 commits stale.** Newest is `v0.2.2-experimental`; `README.md` and the UI's `Cargo.toml`
  both say 0.4.0. R7's A/B reference tag must be cut before R3.
- **`INTERPROCEDURAL_OPTIMIZATION` is set on `phyriad_fg` alone** (`CMakeLists.txt:157`), so
  `pfg_clock_test` — the target producing XR14's bit-parity oracle — compiles `phase_clock.cpp` under
  different whole-program-optimization settings than the shipping binary. Stated hazard, not a measured
  defect, but exactly the "across a call boundary" class XR14 names.
- **`dist/` is stale.** It ships a `phyriad_fg.exe` dated 2026-08-29, predating all nine convergence
  commits, and carries a 4.8 MB `observer-live.log` inside the shipped directory.
- **UI drift is larger than the restructure inventory's "6 flags".** `ui/src/main.js` contains zero
  occurrences of `--sg-barriers`, `--arrival-log`, `--qdump`, `--mv-edge-snap`, `--predict` or
  `--tdr-test`. The 84-flag census with its "Sum check ✓" predates R0/R1/R2.
- **`framework/` is vendored with no provenance** — 28 tracked files compiled by direct inclusion, no
  upstream pin, no sync check. Nothing tells a session which half is current.
- **Root hygiene:** two release ZIPs at the repo root; `scripts/` is an empty directory; three stale
  build dirs. A tracked `.pyc` was found and untracked in this audit (§8).

---

## 7 · Side plans and research — what each still owes

| Arc | What remains | Load-bearing for the objective? |
|---|---|---|
| **MV edge-snap** | "measured — INCONCLUSIVE"; geometry gates 1 and 3 UNRESOLVED, gate 2 FAIL/inside noise; a synthetic aliasing-free edge probe and a per-tap guide are "not done here". Ships default-OFF. | **Indirectly** — the replay corpus never exercises this path, so T1c's completeness claim has a hole shaped exactly like it. |
| **Predict mode** | R3 (disocclusion trails) never independently measured. The plan does not propose promoting it. | No. |
| **Restructure** | E4's V-B drift verifier for the ~150 host flags (an option, not an obligation); E5 (cli.hpp split) never decided; E6 (CPU-kernel testbench) depends on parked E3; I4's six drifted CLI flags still absent from the UI; PR2 and DR1 `open` with no arc left to run them. Its own precondition — SATURATION formally closed — was never verified before this arc started. | Only marginally. |
| **Saturation** | `--graphics-load` never built, and it is the *honest* validation of the governor fix; the tier shed-list re-audit never run; the warp-distress ratchet latches tier 5 forever under `--gpu-priority realtime`. | **The ratchet is** — it can distort any measurement run under load. |
| **Single-track** | **v3.2 ships as the DEFAULT with its operator eye gate still pending**, inside a ladder whose own rule is that nothing advances without that gate. §9's two operator-observed residuals (tile-quantized reclaim boundaries, leading-edge sticky lines) have unbuilt candidate fixes. | **Yes** — this is the shipping default's own unclosed gate. |
| **WGC ingest async** | Opt-in; the real-game soak was never run. Separately the **F in-order-drain fwake bistability** (standing 2–3 slot backlog, ~+10 ms input lag) is parked as out of scope and **no plan owns it**. | Capture-side, not the generation core. |
| **Research / testbench** | **C8** (quality regression gate, unwired — there is no CI to wire it to); **X1** (GPU-less bench smoke job); **TB-2/TB-5** — the testbench's own FIRST measurement and its A/A null run were never done, so **no A/B number from that bench is interpretable yet**; the LSFG flow-quality A/B, named "THE GATE for any 'beat' claim", designed and not run; three shipped disocclusion levers default-off solely because the scorer was never run over them; STAGE-87's cons recovery is a projection; W5's pacer attribution A/B. | C8 and TB-2/TB-5 are. |

---

## 8 · Errors found — what disagreed with the repo

### Corrected in this audit

| File | The false claim | The fact |
|---|---|---|
| `docs/FOTO_MENTAL.md` | "Nothing under CONVERGENCE or MOTION_TRUTH is built" — in the section labelled *read this first after a compaction* | Seven gate records. The line would have made the next session re-derive R0, R1, R2, T0, T1, T1b, T1c. |
| `docs/FOTO_MENTAL.md` | "**Uncommitted:** the whole working tree" | `git status --short` → 0 lines. The sentence was committed *inside* a commit. |
| `docs/FOTO_MENTAL.md` | pointer → `S4.0 (operator) ‖ S4.C0 (next)` | Both closed; S4.C0 is a retired v1 id. |
| `docs/FOTO_MENTAL.md` | "`--qdump` inert under `async_present=true`" | `resolve_config` auto-disables async for the run — corrected at T0 and re-asserted here. |
| `ACTION_PLAN.md:111` | a **duplicate** S2.T6 row reading `blocked` on T1 | Contradicted the dated row 22 lines above. Marked `superseded`, not deleted. |
| `ACTION_PLAN.md` | R4 `blocked (R1)`, R5 `blocked (R2)` | R1 and R2 are `done` with passing gates. Both are `startable`. |
| `CONVERGENCE_RISK_REGISTER.md:6` | "no code is built; every risk starts `open`" | R0/R1/R2 built and gated. The rows were maintained; the summary was not. |
| `CONVERGENCE_RISK_REGISTER.md:44` | "XR-1 … XR-12, all `open`" | Rows run to XR14; several are `mitigated`. |
| `CONVERGENCE_RISK_REGISTER.md:63` | "Residual / accepted (none yet)" | Four residuals were already named three lines below it; three more have accrued. |
| `CONVERGENCE_RISK_REGISTER.md` XR4 | "the UI already restarts the FG on a flag change, `ui/src/main.js:19`" | `:19` is a **comment**; the code is `let autoRestart = false;` at `:24` behind an opt-in checkbox. The residual is larger than the row states. |
| `CONVERGENCE_RISK_REGISTER.md` MR-5 | cites a precedent close at commit `45fb505` | `git cat-file -t 45fb505` → "Not a valid object name". Evidence-integrity defect. |
| `CONVERGENCE_MASTER_PLAN.md:64` | "MOTION_TRUTH `designed`; T0–T6 unbuilt" | T0/T1/T1b/T1c done, T6 built. |
| `CONVERGENCE_MASTER_PLAN.md` | the async-drop cited at `present.cpp:1186` | That line is now inside the 58-float push; the drop is at `:962-978`. Every R4 offset needs re-anchoring. |
| `records/R2_GATE.md:58` | "`img_barrier` … still 40" | **42**, and 42 at R2's own commit — wrong when written, not drifted (`737278f` = 41, `0d5b76d` = 42). |
| `records/R2_GATE.md:14` | "carries one pointer line" | Two. |
| `docs/ARCHITECTURE.md` | layout omits `src/clock/`, `src/seam/`, `src/layers/`; files `bench/` under `tools/` | The three directories R0/R1/R2 created; `bench/` is at the repo root. |
| `README.md:36`, `CMakeLists.txt:4` | "One target" | Four `add_executable` targets. |
| `STAGE_CONTRACT.md:261` | "none [of the eight types] exists as a struct" | 8/8 unbuilt is right, but **`FlipStats` is already taken** by a 2-field framework struct — R4 faces a name collision nobody had recorded. |
| `tools/__pycache__/*.pyc` | tracked in git | Committed by a `git add -A` on 2026-09-03. Untracked here; `__pycache__/` and `*.pyc` added to `.gitignore`. |

### Still outstanding

- **`docs/research/FG_PROJECT_IMPROVEMENT_MAP.md:40-47` describes a different tree.** It claims twelve
  standalone benches were fixed and cites `framework/render/vulkan/cmake/optical_flow_shaders.cmake`;
  that file does not exist here (only `spv_to_header.cmake`), and this repo has **three** benches with
  no CMake at all. Same evidence-integrity class as MR-5's dead commit. **Not corrected — it is a
  research-shelf document and correcting it needs a decision about what it was describing.**
- **`STAGE_CONTRACT.md:77`** names `tools/capture_dump` as stage 1's isolation test while the tool's own
  README says it is wired to nothing.
- **`RESTRUCTURE_INVENTORY.md:135-147`'s 84-flag census with its "Sum check ✓" predates R0/R1/R2** and
  is presented as current.

### Refuted — items that looked pending and are not

Fourteen candidates were killed by their skeptics. The instructive ones: the R0 pipeline-creation time
and the R0 UI screenshot are **self-flagged honesty-ledger caveats**, correctly disclosed, not tasks; the
`--qdump` sync-path restriction is a **declared scope exclusion**, not an omission; warp-scaling Shape-B
is **deliberately abandoned** after an eye rejection, not parked. One refutation was itself wrong and is
reinstated above: R1's pre-vs-post bit-parity is a **live decision**, not a closed disclosure.

---

## 9 · One hypothesis worth testing before another refutation round

Stated as a hypothesis; **nothing measured connects these yet.**

Three periodic facts are currently tracked as unrelated:
1. The T6 deadzone was measured on a **24 px lattice** — hard-edged, non-antialiased, periodic.
2. The MV field carries a **period-3 sub-pixel pattern** (−0.5, +0.1666, 0) on blocks whose own
   `sad_best` is **0**, over 99.5 % of the grid. Three 8 px MV blocks = the lattice's 24 px period
   exactly, and those two values are what a 3-point parabolic sub-pixel fit returns for the neighbour
   ratios periodic content produces.
3. `shaders/wap_warp.comp:1312-1313` independently documents a **KNOWN LIMIT**: content moving exactly
   one texture period per pair aliases `d_zero ≈ 0` — "the same two-frame ambiguity the SAD matcher has".

If these are one mechanism, the deadzone is a property of the test content and **S2.T2 resolves it**. If
they are not, ruling it out costs one non-periodic run. Either way T2 is the move.

---

*Made with my soul - Swately <3*
