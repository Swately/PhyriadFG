# AT2 — adversary scorecards (clean context, sonnet; 3 lenses × 3 candidates)


# ===== CANDIDATE A =====


## Candidate A — lens: budget-auditor

**SCORED**

## A. Feasibility recompute (first-hand)

Verified directly against source, not the candidate's self-report:

- Push block: `wap_warp.comp:77-135` = 57 named fields, `cam_lead` is `vec2` → 58 floats / 232 B. `warp_blend.cpp:117` `pcr.size=232`. **Matches candidate exactly.**
- Descriptor layout `warp_blend.cpp:100-115`: `VkDescriptorSetLayoutBinding bd[14]` — 14 bindings, all always-bound (placeholder-view technique). **Matches.**
- Post-dispatch barriers, default config (`present.cpp:1200-1219`): 2 `VkBufferMemoryBarrier` (mass-counter copy, unconditional — no enclosing `if`) + 3 `img_barrier()` calls (wapOutA→TRANSFER_SRC, bridge_img→TRANSFER_DST, wapOutA→GENERAL) = **5 calls, confirmed exactly** as claimed.
- `img_barrier` (`vk_util.hpp:59`): every call uses `VK_PIPELINE_STAGE_ALL_COMMANDS_BIT` for **both** src and dst masks — today's barriers are already coarse `ALL_COMMANDS` barriers, one per `vkCmdPipelineBarrier` call, never batched. This matters for two things below.
- Recomputed the **real read set of the fg_core pass under the shipping default** (not the candidate's "core reads" subset): the candidate's own §4 default-set mapping requires `phase_anchor` (default ON) to read `mv_bwd` (binding 5), `ambig` (default ON) to read the runner-up candidate field (binding 10), `vblend` (default ON) to read `mv_target` (binding 12), and `inertia_gate`'s `PREDICATE` (default ON) to read `persistence` (binding 8). None of these four images appear in §5.4's "SG-derived per tick: RAW prev,cur,mv,sad → fg_core (**4** image barriers, batched into 1)" line. The default variant's true RAW dependency set is **≥8 images**, not 4 — the headline barrier-efficiency number in §5.4 is undercounted by roughly 2× for the exact layer set the candidate is required to reproduce (M4/KC4). I did not find a matching undercounting in the DRAM-traffic estimate (§5.3) since these extra images are all MV-grid-resolution (129,600-259,200 B each) and immaterial next to the full-res sampling terms already dominating that sum — the arithmetic there checks out.
- Image byte-size table (§5.1): independently recomputed from format×extent (RGBA8 1920×1080 = 8,294,400 B; RG16F 240×135 = 129,600 B; RGBA16F 240×135 = 259,200 B; R8 240×135 = 32,400 B) — **all match**.
- Dispatch grid: `(1920+7)/8=240`, `(1080+7)/8=135` (integer division), 240×135×64 = 2,073,600 = exactly the pixel count — **checks out**.
- New device memory (§5.2, ~1.1 KiB) and DRAM traffic (§5.3, 42-100 MB/tick, 1-2.4% of 1,008 GB/s) — arithmetic re-derived, **consistent** with stated inputs.

**Fit verdict: fits**, comfortably, on every headline number (bandwidth, new memory, push-constant size — only 4 B used of a 256 B device limit). The envelope is not where this candidate is at risk.

**F1 — under-priced line item (the real wall):** the document supplies exact numbers for every axis that is cheap (images=0, new memory=0.003%, bandwidth=2%, barriers="≈ today") and supplies **zero** estimate — not even an order-of-magnitude guess — for the one axis its own §8 names as the load-bearing risk: registers/thread and achieved occupancy of the fused `fg_core` kernel, especially under the **"(a) faithful" default variant it commits to building for A-M1** (§4.2), which keeps ~24 layers' worth of Config-enabled-but-shadowed code live in the same dispatch (soft_gate, commit family, onepos, bidir/occl_thresh, the whole matte family) rather than the lean ~9-layer set §4's table implies. This is not a hypothetical edge case — it is the first build the candidate proposes to run M4 and M3 against. `wap_warp.comp` was open in the same session for every other exact-line citation; no rough register/occupancy ballpark was attempted. Severity: **MAJOR**.

## B. Scorecard

| Metric | Score | Justification |
|---|---|---|
| **M1** (motion exactness) | 4/5 | MV-path/sampling-form preservation verified algebraically (§4.1's `mix` chain matches GLSL semantics bit-for-bit at the boundary weights I checked); satisfies A0's "analysis of preservation" gating note. Docked 1 for an unverified claim: that the `--qdump+` record format can silently absorb "+8 B key, +512 B params, −232 B push" without touching the (uncited, unread by me or the candidate in this doc) MOTION_TRUTH replay-record schema. |
| **M2a** (files to add a layer) | 4/5 | 2 files, exactly at target — verified by construction for the demonstrated no-op exercise. Docked 1: the ≤2 claim holds only because the chosen exercise layer reuses an existing binding; a layer needing a genuinely new GPU resource would need a 3rd site (the fixed `bd[14]` array in `warp_blend.cpp`), a caveat the candidate doesn't name. |
| **M2b** (disabled-layer GPU cost = 0) | 5/5 | Mechanistically airtight: a `false` spec constant is provably DCE'd by any conforming SPIR-V optimizer — 0 by construction, not by convention. Strongest claim in the document. |
| **M2c** (core params ≤8) | 5/5 | 4 UBO fields + 1 push float = 5, verified directly against the struct text shown. |
| **M3** (overhead) | 2/5 | Every number is honestly `unmeasured`, with named instruments — but the one input that would let anyone *judge* feasibility (occupancy) is absent (see F1), and the barrier accounting that *is* given for M3-adjacent cost is undercounted by ~2× (see A). Cannot be scored higher than "unverifiable, and the visible arithmetic is soft." |
| **M4** (veto) | 4/5 | Expression-by-expression algebra is correct and independently re-derivable from GLSL `mix` semantics; the "verbatim port" discipline (§1, §4) is the right mechanism for byte-identity. Not yet run — candidate itself says "an argument, not a result" (A-M1 step 1 pending). Appropriately hedged, not a violation. |

## C. Attack on the central claim

The angle's bet ("one fused pass... maximum GPU efficiency + zero runtime branching," per its own bandwidth framing) does **not** survive its own author's arithmetic: §5.3 concedes bandwidth utilization is ~1-2.4% of the 4090's ceiling, i.e. the "one pass for bandwidth" justification is admitted-weak on this rig at this resolution. That is not a strawman I am constructing — the candidate names it. What remains defensible is narrower: zero allocations, M2b=0 by construction, no extra submits/fences, no branch divergence.

**Layer set where it wins:** the lean, "(b) honest" ~9-layer default (§4.2's second option) — zero new images, comparable-or-fewer barriers even after my §A correction, disabled layers cost nothing.

**Layer set that embarrasses it:** the "(a) faithful" default variant — the one the candidate actually commits to shipping for A-M1 ("this is what the A-M1 milestone builds, because M4 is a veto and I will not risk it") — carries the full ~24-layer, high-live-state instruction stream in a single dispatch, which is exactly the register-pressure pathology §8 names as unbounded and unmeasured. The document's own choice of which variant to actually build routes straight into its own worst case, and prices that collision at zero.

## D. Kill criteria

1. **Unmeasurable:** PASS with margin — MV path/sampling form preserved, record delta stated and small.
2. **Order-dependent semantics:** PASS with margin — single-writer store structurally enforced; fold slots use the KC2 escape clause (explicit declared, dumped, diffable order) correctly and by name; candidate is honest that this is "declared," not "free."
3. **Seam regression (fence/round-trip/ALL_COMMANDS):** PASS, contingent — verified first-hand that *today's* `img_barrier` already uses `ALL_COMMANDS_BIT`/`ALL_COMMANDS_BIT`, so if SG truly emits "precise masks only" per `seam_graph.hpp:38-39` (cited but not independently re-read by me in this pass — SG's properties are a declared, previously-adjudicated constraint, out of this candidate's scope to re-litigate), this candidate *improves* on today rather than regressing. Not independently re-verified against `seam_graph.hpp` itself in this session.
4. **Cannot express the shipping default:** PASS with margin — all 8 named default-affecting layers (mv-guided, inertia gate, phase-anchor, bg-reclaim, ambig, vblend, single-track/screen-static, stasis) are explicitly mapped in §4.
5. **Injection/multi-GPU:** PASS trivially — not touched, capture/identity untouched.
6. **New runtime dependency:** PASS — `gen_layers.py` is build-time only (stdlib), pipeline cache uses existing Vulkan API.

## E. Constraint smuggling

No violation of §6's forbidden list found. The push-constant override chain is genuinely replaced by a different mechanism (fold slots + rank-max selection), not renamed; the persisting "dead-by-value work" under choice (a) is a *related* waste pattern but not the *same* mechanism KC2 targets (output meaning is no longer order-dependent even when computation is wasted), and it is transparently named by the candidate itself, not smuggled. No `--minimal`-style strip flag, no injection, no "fewer layers" slogan.

## F. Novelty/optimality honesty

No unscoped "optimal"/"novel" claims found. The document is unusually disciplined here — every strong claim is trust-tiered, and the weakest points (§5.3's bandwidth concession, §8's occupancy risk, §3.4's "honest residual") are self-reported rather than buried. Clean.

## G. Salvageables (candidate's own ideas)

- The rank-max-selection argument for `MV_FETCH`/`BASE` (commutative by construction, a genuine "guarantee" not a "discipline") — reusable regardless of angle.
- The convex `RESULT_MIX` fold with the `weight==0 ⇒ identity` obligation, checked by the M4 replay — a concrete, testable composition discipline.
- `gen_layers.py` deriving four artifacts (shader UBO, Config/hpp, CLI parse+help+cascade, UI JSON) from one manifest — directly targets the 257-vs-173 drift and is orthogonal to fused-vs-staged.
- Persisted `VkPipelineCache` + background compile thread + tick-boundary atomic pipeline swap for variant-explosion cost — reusable under any variant-bearing design.
- `--layer-dump` printing `shadowed-by: X(SLOT,rank)` — makes "dead work" visible and auditable, independent of this angle's fate.
- The A-M1 staging discipline itself: new path lives behind `--fg-core`, old path stays default, one binary does the A/B — low-risk regardless of which angle is chosen.
- The explicit "(a) faithful vs (b) honest" fork in §4.2 — naming a byte-identity/leanness tradeoff as a declared operator decision rather than deciding it silently.


## Candidate A — lens: performance-realist

## Gate Verdict

**SCORED**

---

## A. Feasibility recompute (first-hand)

Independent recompute at 1920×1080 (2,073,600 px; dispatch 240×135 workgroups):

- Store `wapOutA`: 8,294,400 B. Blit read+write `wapOutA`↔`bridge_img`: 16,588,800 B. Compulsory `prev`/`cur` sample footprint (one tap each): 16,588,800 B. Upper-bound at ~9 taps/px: 74,649,600 B. MV+SAD: 259,200 B.
- **My sum:** lower bound 41,731,200 B (41.7 MB), upper bound 99,792,000 B (99.8 MB) — matches the candidate's "42–100 MB/tick" exactly.
- At 240 ticks/s: 10.0–24.0 GB/s → 1.0–2.4 % of RTX 4090's 1,008 GB/s spec bandwidth. Matches candidate's figures.
- **New allocation check:** I independently summed the candidate's own §5.1 image table (wapPrevA+wapCurA+wapOutA+bridge_img = 33,177,600 + 8,294,400... ) → **34,052,400 B ≈ 32.48 MiB**, not the "~41 MiB" the candidate cites as the denominator in §5.2. A ~26 % overstatement of its own reference figure (F1, MINOR — the "~0.003 %" conclusion is unaffected either way: 1.1 KiB / 32.48 MiB ≈ 0.0033 %, vs. the candidate's 0.003 %).
- Push-constant reduction 232 B → 4 B is correctly measured against `warp_blend.cpp:117` (`pcr.size=232`) and the shader's 57-field block, both verified by direct read: exactly 56 floats + 1 vec2 = 58 floats.

**Fit verdict: fits** (bandwidth and new-allocation lines are trivially inside the envelope). The arithmetic that DOES matter for M3 — SM occupancy / register pressure needed to hide latency inside the 4.17 ms/240 Hz tick — is **not computed anywhere in the document**; only bytes-moved is computed, and bytes-moved is not the binding constraint at 1–2 % of peak bandwidth (F1, MAJOR — see C).

---

## B. Scorecard

| Metric | Score | Justification |
|---|---|---|
| **M1 · Motion exactness** | 4/5 | §4.1's expression-by-expression `mix(a,b,0)=a` algebra for the `RESULT_MIX` chain is correct IEEE-754 reasoning and the rank order is verified to reproduce today's *source* order (inertia→bg_reclaim→phase_anchor→ambig, matching `wap_warp.comp:332,358,397,426`). Correctly tiered `estimated`, not `measured`. Docked one point: the `mv_guided` repack (today's single packed float `1.0+sim_thresh` → split bool + separate UBO scalar, §3.5) is asserted "verbatim" but never walked bit-for-bit the way `RESULT_MIX` was. |
| **M2a · Files to add a layer** | 5/5 | 2 files (`.layer` + one `LAYERS.txt` line), demonstrated with a concrete no-op example and a milestone test (A-M1 step 2). Meets the ≤2 target exactly. |
| **M2b · Disabled-layer GPU work = 0** | 4/5 | The instruction/dispatch/register mechanism (spec-const → driver DCE at pipeline creation) is real, standard, and genuinely delivers *zero by construction*, the strongest part of the candidate. Docked one point: the buffer-barrier culling claim (mass SSBO, ambig/vblend uploads) is attributed to "SG's backward reachability," but SG (verified: `declare_image` is its only resource primitive) has **no buffer-resource concept** — the candidate's own §0 fact table says so. The actual mechanism (culling the *enclosing pass*, whose callback happens to also carry hand-written buffer code) is plausible but unspecified, sitting exactly on the boundary the candidate itself flagged as SG's limitation. |
| **M2c · Core parameters ≤8** | 5/5 | 5 parameters (`residual_ceil, improvement_frac, agreement_threshold, mv_source, t`), verified against the source's 4-parameter core-math citation in `DESIGNER_BRIEF.md` + `wap_warp.comp:78–81`. Comfortably under target. |
| **M3 · Overhead** | 2/5 | **Zero measured numbers.** No GPU time, no presents/s, no register/occupancy data, no `--wsub gpu:`/`rec:` reading — despite the candidate itself stating A-M1 uses "only instruments that exist today" and naming `--csv`, `--wsub`, and Nsight Compute as already available. The one quantitative claim ("−1 to +3 µs/tick... inside noise") is explicitly `unmeasured`. §8 further concedes an *unbounded* register/occupancy-cliff risk specific to this exact metric ("M3 regresses for the default set even though the default set did not change"). My own recompute (§A) confirms the DRAM-bandwidth argument answers the wrong question for a 4.17 ms/240 Hz tick budget — occupancy, not bytes moved, is the binding constraint at 1–2 % of peak bandwidth, and that number is nowhere in the document. |
| **M4 · Default-output equivalence (veto)** | 4/5 | The algebraic default-path derivation is careful, specific, and grounded in verified line citations (`wap_warp.comp:1315–1332`). Correctly chooses the conservative "(a) faithful" treatment for shadowed layers (§4.2) rather than risk the veto for an M2 gain — respects the M1/M4 > M2 priority correctly. Docked one point because zero replay/byte-diff has actually been run; the proof obligation is honestly deferred to A-M1 step 1, not fabricated, but M4 is a hard veto and stands entirely unverified today. |

---

## C. Attack on the central claim

The assigned bet (`DESIGNER_BRIEF.md` §4, Angle A) is "one pass **for bandwidth**... maximum GPU efficiency + zero runtime branching." The candidate's own §5.3 arithmetic (which I reproduced independently in §A and it holds) **concedes half its own bet**: *"the fused pass's headline justification — 'one pass for bandwidth' — is weak on this rig at this resolution... a staged design could afford several full-res RGBA8 intermediates... before bandwidth became the binding constraint."* This is not a strawman comparison — the competitor (a staged SG-passes design) is a real sibling candidate in this same AAP round (`CANDIDATE_B.md` exists on disk), and "today's shipping `wap_warp`" is verified, running production code.

What survives: the zero-branching / compile-out mechanism is real and independently verified (spec constants are a standard, well-founded Vulkan technique for exactly this). The "zero new images" claim is genuinely robust at **any** layer-set scale, since PhyriadFG's existing descriptor-set-always-bound placeholder-view trick (verified at `warp_blend.cpp:101–154`) is reused unchanged.

**Layer set where it wins cleanly:** the shipping DEFAULT set (9 layers, A0's named workload) — M2b=0 by construction, 0 new bytes, a careful M4 argument, minimal barrier/fence growth. This is squarely the A-M1 milestone's scope and the win is real.

**Layer set where it embarrasses itself:** the FULL layer catalog the candidate's own tree (§1) plans to eventually re-admit (matte, crescent, contour, obj_crescent, multicand, commit family — ~24 total layers, "K≈24 today" per §8). Here the candidate's own admissions compound: (1) **no cost isolation** — a register-heavy layer taxes every pixel's occupancy even on pixels it never touches, unlike a staged design that confines the cost to its own pass; (2) **no per-layer timestamp attribution** — a staged design gets this by construction, this design cannot give it at all; (3) a **user-facing toggle hitch** with **zero order-of-magnitude estimate** anywhere in the document (every other number is at least `estimated`; this one is bare `unmeasured` with no ballpark) — today, EVERY flag toggle is a push-constant write (zero compile cost, verified: `present.cpp` assembles the 232 B push every tick with no pipeline swap); under this design, any combination outside the two pre-warmed keys (`default`, `default & ~warp_light_mask`) triggers a live driver compile of a ~1,300-line-equivalent shader mid-session — a regression class the shipping design structurally cannot have. The candidate's own words: *"at that point this candidate has partially conceded to Angle B."*

---

## D. Kill criteria (margin check)

| # | Criterion | Verdict | Margin |
|---|---|---|---|
| KC1 Unmeasurable | Clears | With margin — the `--qdump+` record's replaceable fields are named exactly (`+8 B key, +512 B params, −232 B push`), and marker extraction operates on generated frames, not the push block, so replacing it doesn't affect measurability. |
| KC2 Order-dependent semantics | **Contested, not "with margin"** | The candidate's `MV_FETCH`/`BASE` slots ARE order-independent by construction (`max(rank)`, verified commutative). But the fold slots (`MV_ADJUST`, `SAMPLE_MV`, `WEIGHT`, `GATE`, `RESULT_MIX`) have output that depends on a declared rank order — the candidate itself says "I am not claiming commutativity." It clears this via an "escape clause" it calls *"Kill criterion 2's own"* (§3.4 item 3) — but that clause exists only in `DESIGNER_BRIEF.md` §3.3 (a paraphrase of what a candidate must contain), **not** in the frozen `A0_FROZEN_OBJECTIVE.md` §4 item 2 text, which reads only "*the meaning of the output depends on the ORDER in which layers write (last-writer-wins)*" — with no explicit escape clause. Whether the parenthetical "(last-writer-wins)" scopes the criterion to arbitrary/implicit overrides (letting a declared, diffable fold through) is a genuinely open textual question the candidate resolves in its own favor by misattributing brief language to the frozen text (F3, MAJOR). |
| KC3 Seam regression | Clears | With margin — verified in `seam_graph.hpp:38` ("PRECISE masks ONLY — never ALL_COMMANDS"), `ALL_COMMANDS` appears only in a debug-name lookup table, never as an emitted barrier stage; today's single `fBridge` fence confirmed at `present.cpp:928,1433`; no per-stage fence or host round-trip proposed. |
| KC4 Cannot express default | Clears | With margin — all 9 default-affecting layers (mv-guided, inertia gate, phase-anchor, bg-reclaim, ambig, vblend, single-track, screen-static, stasis) are explicitly mapped in §4, each against a verified source-line citation. |
| KC5 Injection/multi-GPU | Clears | Trivially — capture path (DDA/WGC) is explicitly reused unchanged; no second-GPU dependency introduced. |
| KC6 New runtime dependency | Clears | Trivially — persisted `VkPipelineCache` uses the existing Vulkan runtime + filesystem; no new library/runtime added. |

---

## E. Constraint smuggling

None found. Checked against all five §6 items: no perceptual/ghosting metric used as a selection criterion; the new `--fg-core` path is a genuinely new parallel unit, not a strip-flag on the accreted `main.cpp`; no injection/present-hook introduced; the push-constant override chain is structurally replaced, not renamed (single-writer output is a real architectural change, not a relabeled push block); no "fewer layers" slogan — layer removal (§4.2 option b) is explicitly gated on a byte-diff proof obligation, not asserted.

---

## F. Novelty/optimality honesty

Clean. No unscoped "optimal"/"novel"/"best" claims found. The one superlative present — "the one budget line where this candidate is unambiguously strongest" (§5.1, re: zero new intermediate images) — is scoped to a single, verified axis (0 bytes vs. any staged alternative's necessarily-nonzero intermediate), not a general design-superiority claim. The document's self-critical §5.3 paragraph (undercutting its own headline bandwidth justification) is the opposite of overclaiming.

---

## G. Salvageables (candidate's own ideas only)

1. **Single-writer output as a hard structural guarantee** (§3.4.1) — no slot exists after `compose()`; the store's source expression is fixed and unreachable from any layer body. This is the cleanest, most literal repair of the "sequential overrides of `result`" defect present in any part of the document and is independent of fused-vs-staged topology.
2. **Spec-constant compile-out for the DEFAULT-variant hot path** (§3.5) — a verified, standard technique delivering true zero-instruction cost for a disabled layer, worth grafting into a staged design's per-stage internals even if the top-level pass topology is rejected.
3. **`max(rank)` selection for mutually-exclusive claim slots** (`MV_FETCH`, `BASE`) — commutative/associative by construction, a reusable pattern for "exactly one winner" composition regardless of overall architecture.
4. **Convex `RESULT_MIX` fold with the `weight==0 ⇒ identity` contract, checked by replay** (§3.4.3) — a concrete, testable non-discarding-composition discipline.
5. **`gen_layers.py`, one manifest → four generated artifacts** (glsl UBO, Config/spec-ids, CLI parse+help, UI json) — directly and structurally attacks the verified 257-vs-173 CLI/UI drift; stdlib-only Python, inside the envelope.
6. **`--layer-dump` printing `shadowed-by:` relationships** (§4.2) — turns today's invisible dead code (the discarded 531–1320 range) into an auditable, diffable fact, independently useful as a diagnostic regardless of which candidate is chosen.
7. **Pre-warming the two known-hot variant keys at startup** (§7 point 6) — a narrow but concrete mitigation pattern for the compile-hitch problem, salvageable even though it doesn't solve the general case.


## Candidate A — lens: adoption-skeptic

GATE VERDICT: **SCORED**

---

## A. FEASIBILITY RECOMPUTE (first-hand)

Recomputed independently against the repo, not the candidate's numbers:

- **Push block**: read `shaders/wap_warp.comp:77-135` and `warp_blend.cpp:117` myself — `pcr.size=232`, 58 floats (57 struct fields, `cam_lead` a `vec2`) confirmed exactly. Candidate's replacement (`CoreParams` 4 fields + `CorePush.t` 4B) leaves 252B of the 256B device limit unused — no overflow, huge margin.
- **Dispatch grid**: 1920/8=240, 1080/8=135 exactly (both divide evenly) → 32,400 workgroups × 64 threads = 2,073,600 = pixel count. Checks out.
- **New device memory**: `CoreParams` 16B×2 + `LayerParams` ~512B×2 ≈ 1.1 KiB against ~41 MiB of existing images (0.003%). Fits trivially. **Minor arithmetic slip found**: candidate computes layer-scalar count as "57 push fields minus the 5 core = 52," but only 4 of the original 57 fields move to the core (`residual_ceil, improvement_frac, agreement_threshold, t`) — `mv_source` is a *new* field, not one of the 57. Correct subtraction is 57−4=53, not 52. Immaterial to the conclusion (~1 KiB either way) but it's an arithmetic error inside a number the candidate itself tags "estimated."
- **Bandwidth**: recomputed the range myself — compulsory floor ≈8.3+16.6+16.6+0.26+0.0005 ≈ 41.7 MB/tick, ceiling with 9 taps/px ≈ 8.3+16.6+74.6+0.26 ≈ 99.8 MB/tick → matches the stated "42–100 MB/tick," 10–24 GB/s at 240 Hz, 1–2.4% of the 4090's 1,008 GB/s. Arithmetic is sound.
- **257 vs 173 flags, 5-barrier post-dispatch set, unconditional mass-counter block, SG's image-only barrier model, backward-reachability culling, `RESTRUCTURE_PLAN`'s 28,799-presents baseline, `seam_graph.hpp` 549 lines, `test_seam_graph.cpp` 436 lines**: all verified byte-for-byte against the actual files. All correct.
- **One exception found by direct recount**: `test_seam_graph.cpp` has 111 `CHECK(` call sites + 4 `check_str_eq(`, not the "122 checks" the candidate states (inherited verbatim from `DESIGNER_BRIEF.md` without independent re-verification). Immaterial to any scored metric — flagged only because the candidate claims first-hand verification discipline and this figure wasn't actually re-checked.

**Fit verdict: FITS**, with wide margin on every hard, quantifiable envelope line (push-constant size, parameter count, file count, new memory, bandwidth). No overflow anywhere I can compute. The one real open question is not a hard-envelope overflow but a risk metric (GPU register/occupancy pressure, §8) that is unmeasured, not measured-and-failing.

## B. SCORECARD

| Metric | Score | Justification |
|---|---|---|
| **M1** | 5/5 | Core math (mv fetch → A/B samples → gates → blend) preserved in literal operation order; §4.1 gives an expression-by-expression byte-identity argument for the default set (`mix(a,b,0)=a` exactly, same rounding). Correctly gated as "expected, not measured" per A0's own gating note — the strongest a candidate can be at this stage. |
| **M2a** | 4/5 | Verified mechanism: 2 files (`.layer` + one `LAYERS.txt` line) for the demonstrated no-op case, meets target exactly. Docked because the ≤2-files claim is proven only for a layer that fits one of the 9 *predefined* slots; a layer needing new `CoreState` exposure would require editing `fg_core.comp` itself — a 3rd file — and this bound is never stated. |
| **M2b** | 4/5 | Real, verifiable zero for *image*-resource layers (spec-const DCE + SG's actual backward-reachability culling, confirmed against `seam_graph.hpp` first-hand). Docked for the "*and negative* for some layers" bonus claim (§4.3, citing `matte`'s buffer barriers vanishing) — this **contradicts the candidate's own measured fact that SG has no buffer-resource concept**, depends on a "buffer extension" the tree diagram defers to "a later, separately-gated change" cited as "§6.4" (a section that does not exist in this document), and `matte` isn't even in A-M1's 9-layer port list. Stated as flat fact, no trust tier. |
| **M2c** | 5/5 | 5 core parameters ≤ 8 target, directly countable off `fg_core_contract.hpp`, unambiguous. |
| **M3** | 3/5 | The candidate's own §5.3 arithmetic self-refutes the fused pass's headline bandwidth justification (~1–2.4% of peak — "weak on this rig"). Remaining CPU-side wins are real but small and unmeasured (−1…+3 µs, "inside noise"). The dominant regression risk — no register/occupancy isolation across a growing fused shader (§8) — is entirely unmeasured and self-named as the weakest point, threatening exactly this metric. Plausible not to regress; not demonstrated. |
| **M4** | 4/5 | Rigorous, checkable byte-identity argument for the shipping default, explicitly labeled "estimated… an argument, not a result" until A-M1 runs. The veto risk is not fully closed: §4.2's faithful-vs-honest fork is left as an undecided operator choice, and the byte-diff hasn't executed. |

## C. ATTACK ON THE CENTRAL CLAIM

The comparator is real, not a strawman — every core-math claim is pinned to actual file/line citations in the live repo, verified above. But the central bet ("one fused pass **for bandwidth**") is undercut by the candidate's own numbers: at ~1–2.4% of the 4090's bandwidth, there was never a bandwidth problem to solve on this rig, a fact the candidate states outright rather than sells around — commendable honesty, but it means the differentiator vs. a staged (Angle B) design collapses to secondary benefits (zero new allocations, M2b=0-by-construction, fewer submits/fences) that are real but modest. **Where it wins**: the current 9-layer default set — clean, minimal, verifiably byte-exact by construction. **Where it embarrasses itself**: exactly the scenario the search is FOR — growing to the full ~24-layer set. §8 admits that all layers share one register budget/occupancy class with zero cost isolation; if driver DCE doesn't perfectly eliminate dead spec-const branches at scale (a real, named, unmeasured risk), every future layer's register pressure lands on the DEFAULT variant's every pixel, and the candidate's own fallback is to partially concede to Angle B ("split the register-heaviest family into a second pass"). That is a genuine, self-identified crack in the central claim's durability, not a fabricated one.

## D. KILL CRITERIA

All 6 pass with margin, one interpretive note:
- **KC1**: does not bite — record format is `+8B key, +512B params, −232B push`, MV path/sampling form unchanged, argued specifically (§6 step 7).
- **KC2**: single-writer store is a hard, structural guarantee (verified: today's 6-site override chain is unrepresentable in the new contract). Selection slots (`MV_FETCH`, `BASE`) are order-free by `max()`. Fold slots are order-**declared**, not order-free — the candidate is explicit that this leans on the "EXPLICIT declared order… part of the contract" carve-out named in `DESIGNER_BRIEF.md` §3.3, not verbatim present in A0's own bare KC2 text ("meaning… depends on the ORDER in which layers write"). Under a strict textual reading, the fold slots ARE order-dependent (candidate admits non-commutativity). Since the brief is the operative assignment document and explicitly sanctions this treatment, I do not score this a KC2 failure — but the honest residual is real and fully disclosed by the candidate itself (§3.4), not smuggled.
- **KC3**: satisfied — no per-stage fence, no host round-trip, SG's own derivation rules (verified: "PRECISE masks ONLY — never `ALL_COMMANDS`") are inherited unchanged.
- **KC4**: satisfied with margin — all 8 named default-affecting layers explicitly mapped (§4).
- **KC5**: satisfied trivially — nothing touches capture/injection.
- **KC6**: satisfied — `gen_layers.py` is build-time only (stdlib Python), `VkPipelineCache` is a file, no new shipped dependency.

## E. CONSTRAINT SMUGGLING

Checked each §6 item specifically: push-constant override chain (gone — structurally, not by convention), last-writer-wins (addressed head-on, honestly qualified, not smuggled), `--minimal` strip flag (absent — `--fg-core` is a genuinely separate new pipeline, not a strip flag on the accreted `main.cpp`), perceptual metrics as selection criteria (PSNR use is confined to M4's veto oracle, which A0 itself designates for exactly that purpose — not used to *select* among candidates), injection/multi-GPU (absent). **No smuggling found.**

## F. NOVELTY/OPTIMALITY HONESTY

No unscoped "optimal"/"novel" claims. The document is unusually disciplined — every number trust-tiered, §5.3 argues against its own headline justification, §8 names the design's own probable failure mode. This is a real strength of the candidate's presentation.

## Defect list

| # | Class | Severity | Description |
|---|---|---|---|
| 1 | F1 | MAJOR | §4.3's "negative barrier count for matte" claim contradicts the candidate's own measured fact that SG has no buffer-resource concept; rests on an unbuilt "buffer extension" cited as "§6.4" (a section that does not exist in this document) and on `matte`, which isn't in A-M1's port list. Stated as flat fact, untiered. |
| 2 | F2 | MAJOR | Central bandwidth-savings justification is self-admitted weak (~1–2.4% of peak); the design's real differentiators are narrower than the "bet" framing implies; the biggest concession (register/occupancy non-isolation, §8) directly threatens M3 and is entirely unmeasured. |
| 3 | F1 | MINOR | LayerParams scalar count arithmetic (57−5=52) double-subtracts the new `mv_source` field; correct is 57−4=53. Doesn't change the ≈1 KiB conclusion. |
| 4 | F1 | MINOR | "122 checks" for `test_seam_graph.cpp` (inherited from `DESIGNER_BRIEF.md`, uncritically repeated) vs. first-hand recount of 111 `CHECK(` + 4 `check_str_eq(` ≈ 115. Immaterial to any scored metric. |
| 5 | — | MINOR | M2a's ≤2-files guarantee is demonstrated only for a layer fitting one of the 9 predefined slots; a layer needing new core-state exposure costs more (edits `fg_core.comp`), and this bound is never stated. |

## G. SALVAGEABLES (present in the candidate, not invented here)

1. The **single-writer store guarantee** — no slot exists after `compose()`; a layer physically cannot reach `u_output`. This alone kills KC2's literal defect (the override chain) regardless of which angle is chosen.
2. The **`.layer` manifest format** (`@layer/@slot/@rank/@needs/@reads/@enable/@param/@glsl`) generating 4 artifacts (shader decl, config/spec-ids, CLI parse+help, UI json) from one file — genuinely collapses the *layer-flag* hand-sync problem to one source, independent of fused vs. staged.
3. **`RESULT_MIX`'s convexity discipline**: contributor returns `(target, weight∈[0,1])`, `weight==0 ⇒ identity` checked by the M4 replay — a concrete, checkable non-discard invariant for any order-declared accumulation slot.
4. **`--layer-dump`'s "shadowed-by" annotation** (§4.2) — makes today's dead-by-value layers *visible and removable* instead of silently discarded; useful independent of which angle wins.
5. **Pre-warming `{default, default & ~warp_light_mask}`** at startup to avoid a runtime-toggle compile hitch on the one adaptive-shedding path (`--load-governor`) — a concrete, cheap, already-scoped mitigation.
6. The **`@reduce` mechanism** — lets a layer obtain workgroup-scope shared data without ever touching `barrier()` itself, preserving the single top-level-barrier constraint cleanly.
7. The **trust-tier discipline** itself (every number tagged measured/estimated/unmeasured, named instrument for each unmeasured claim) is a procedural asset worth carrying into whichever candidate is chosen.


# ===== CANDIDATE B =====


## Candidate B — lens: budget-auditor

GATE VERDICT: **SCORED**

## A. Feasibility recompute (first-hand, independent of the candidate's numbers)

I re-traced `SeamGraph::compile()` (`apps/minimal_fg/include/minimal_fg/seam_graph.hpp:262-451`, read in full) pass-by-pass against Candidate B's own declared reads/writes for all 8 default nodes and got **17 barriers / 8 passes**, matching the candidate exactly — this line item is genuinely derived, not asserted.

Independently recomputing bandwidth from the candidate's own per-pass reads/writes table (§6.2): the raw sum is 349 MiB/tick (confirmed). But the "L2 credit" arithmetic does not hold up: the candidate's own table shows `prev_real` read 3× total and `cur_real` read 4× total (7 occurrences, 2 of them "first" reads) → only **5 repeat reads** are crediteable (5 × 7.91 MiB = 39.55 MiB), not the "**three** repeat reads of `prev_real` **and of** `cur_real`" (6 × 7.91 = 47.46 MiB) the candidate credits. Corrected: **≈309 MiB/tick**, not 302. Small (≈2%), but it is exactly the kind of headline-sum error the lens exists to catch.

**The real wall is elsewhere: the COMPOSE stage is silently pre-fused in the budget.** §1.2/§3.4 declare `COMPOSE/PIN` as "many, target-keyed," and §1.3's own file-placement plan gives `base_b_track/`, `pin_screen_static/`, `pin_stasis/` **three separate directories** (three separate `node.comp` shaders, three separate `record.cpp` 4-call sequences) — i.e. three SG passes. But §4.9, §6.2 row 8, and §6.3 row 8 collapse all three into **one** "compose" dispatch with one set of barriers, with no accumulator mechanism ever specified for PIN (unlike DAMP, which gets an explicit `damp_acc` two-or-more-node treatment in §3.4). Recomputing PIN as three real RMW passes on `result` (base writes result; pin_screen_static reads result+d_pixel, RAW×2, writes result; pin_stasis reads result, RAW×1, writes result) adds **+2 dispatches, +2 barriers, and ≈+31.6 MiB/tick** (two extra RGBA8 result RMW round-trips) that the budget never counts. Corrected totals: **10 passes, ≈19 barriers, ≈340–420 MiB/tick worst-case (≈370 MiB after L2 credit)**, and CPU record cost rises from the claimed 0.07 ms toward ≈0.09 ms (≈2.2% of the 240 Hz tick, still small in absolute terms but a ≈25–30% miss on the candidate's own headline).

**Fit verdict: TIGHT, not proven.** The corrected GPU-time addition (≈0.47–0.65 ms vs. the claimed 0.42–0.56 ms) is still the right order of magnitude and the M3 conclusion probably doesn't flip — but the only empirical anchor in the whole document (`gpu (warp) ≈ 3–4 ms`, verified verbatim at `docs/research/UPLOAD_OFFLOAD_MASTER_PLAN.md:29`) was measured **under BF6 combat with the 4090 at 98.8%**, not on the objective's actual M3 workload (idle 4090, `ball_zoo`, 1920×1080). The candidate itself flags this (§8) as "a number without a denominator" — correct, and the honesty is commendable, but it means M3 pass/fail is **unmeasured** on the real envelope, full stop.

## B. Scorecard

| Metric | Score | Justification |
|---|---|---|
| **M1** | 3/5 | Core sampling form (`core_sample.comp`) verified textually identical to `wap_warp.comp`'s registers-only arithmetic (line 344 `mv_fwd`, line 394/410 `mix()` calls, all spot-checked byte-for-byte against the real file). No new external inputs. But the "fp32-crossing is provably lossless" argument rests on an *unverified* Vulkan-spec-silence claim, and the fp32-vs-register ordering risk is explicitly `unmeasured` by the candidate's own admission (§8) — stacked assumptions, not yet an instrument result. |
| **M2a** (≤2 files) | 2/5 | **Fails as specified.** §1.3's own file-placement plan requires `desc.hpp` + `node.comp` + `record.cpp` (3 new files) + 1 line in `layers.inc` = **4 files** in a `git diff --stat`, not the "2 files" §3.1 claims meets the ≤2 target. Self-contradictory within the document. |
| **M2b** (disabled=0) | 5/5 | Grounded in real, adopted, golden-tested code (`seam_graph.hpp:266-301`, verified verbatim). Culling is unconditional and structural, not a promise. |
| **M2c** (≤8 core params) | 5/5 | Core push is 1 field (`t`), verified against the shown shader — comfortably under target. |
| **M3** | 2/5 | Recomputed traffic (≈340-420 MiB/tick worst-case, ≈19 barriers, ≈10 passes) is materially higher than the candidate's own 349 MiB/17-barrier/8-pass headline once the compose-stage undercount is corrected. The only measured anchor is off-condition (saturated GPU, real game, not the idle-4090 `ball_zoo` target). Pass/fail against "no regression beyond run-to-run spread" is not demonstrated. |
| **M4** (veto) | 5/5 — passes | Every default-graph node is a verbatim, line-cited port (spot-verified against the real file: push-block field count 57/232B/offset-228 exact match, `single_track` block lines 1321-1330 char-for-char match, all 8 cited `cli.hpp` defaults exact match). The bg-reclaim/phase-anchor discard is deliberately *reproduced*, which is what M4 (bug-for-bug fidelity) requires. |

## C. Attack on the central claim

The central bet — "modularity purchased with bandwidth, the exchange rate stated honestly" — **survives** independent recompute at the order-of-magnitude level (my corrected ≈370 MiB/tick vs. their 302-349 is the same "10-15×" story). The competitor (today's shipping fused `wap_warp.comp`, ≈24 MiB/tick equivalent) is real, not a strawman — verified against the actual shader. **F2 (MAJOR):** the ONLY GPU-cost number in the document that could validate affordability was measured under conditions (BF6 combat, 98.8% GPU saturation) that do not represent the objective's named M3 workload (idle 4090, `ball_zoo`, 1920×1080) — the candidate says so itself, and proposes its own remedy (milestone B1) rather than asserting fit. This is honest, but it means the document does **not** currently demonstrate M3 feasibility with margin; it demonstrates a plan to find out.

## D. Kill criteria

1. **KC1 (unmeasurable):** clears — no new external replay-record input; sampling form unchanged.
2. **KC2 (order-dependent last-writer-wins):** **F3, MAJOR — margin not established.** A0's text is unconditional: "the meaning of the output depends on the ORDER in which layers write (last-writer-wins) — the defect under repair, reintroduced." The default graph's `phase_anchor` write to `mv_cond` (verified: `mv = mix(mv_fwd, -mv_bwd, w_b)` at line 410, reading `mv_fwd` from *before* `bg_reclaim`'s line-394 write) **is** exactly this pattern, deliberately kept (§4.3: "This is deliberate… reproduces today's default output"), suppressed via a `.dominates_ok` annotation rather than eliminated. The candidate's defense leans on DESIGNER_BRIEF's softer "explicit declared order" carve-out, which is a real, arguable escape (the ANCHOR-after-DAMP relationship is now a structural slot property, not accidental line order) — but the frozen A0 text itself contains no such carve-out. This is the single most consequential judgment call in the document and I score it as **not cleared with margin**.
3. **KC3 (seam regression):** clears with margin — verified against real source: no `ALL_COMMANDS` anywhere in `seam_graph.hpp` (explicit prohibition at line 38, confirmed), single fence into the existing `cmdBridge`, no host round-trip.
4. **KC4 (hosts default set):** clears — all 8 default-affecting layers mapped with cited shader-line provenance, spot-verified accurate.
5. **KC5 (injection/multi-GPU):** clears — untouched.
6. **KC6 (new runtime dependency):** clears — verified `seam_graph.hpp` includes only `<vulkan/vulkan.h>` + STL.

## E. Constraint smuggling

**F4: none found** as a distinct forbidden driver reintroduced under a new name. The push-constant override *mechanism* is genuinely dropped (57-field block → 1-field core push, verified). The closest thing to smuggling is the KC2 tension above — the *mechanism* changed but one instance of its *failure mode* was kept on purpose — already captured under D, not a separate violation of §6's list.

## F. Novelty/optimality honesty

**F5: clean.** No unscoped "optimal"/"novel" claims found anywhere in the document. Every number carries a measured/estimated/unmeasured tag (Appendix B is exhaustive and, on inspection, accurately tiered — including tagging its own two shakiest numbers, "achievable bandwidth fraction" and "today's warp cost on `ball_zoo`," as `unmeasured`).

## G. Salvageables (present in the candidate, independent of Angle B winning)

1. **The dominance-warning mechanism** (§3.5, Appendix A.1) — ≈10 lines added to `SeamGraph::compile()`'s existing WAW branch that turns any silent last-writer-wins discard into a build-time-visible, nameable event. This is angle-agnostic and cheap; verified the hook point (`seam_graph.hpp:411-417`) is real and exactly where claimed.
2. **Zero-allocation `execute()`** (Appendix A.3) — the per-pass `std::vector<VkImageMemoryBarrier2>` heap allocation is a real, verified defect in the *adopted* `seam_graph.hpp` (confirmed at lines 458-459) that will cost any SG-based candidate ~1,920 allocs/s at 240 Hz × 8 passes — worth fixing regardless of which angle ships.
3. **The B1 "two-pass identity split" milestone** (§6.5) — a cheap, byte-diff-gated first step that puts the fp32-intermediate assumption on an instrument before committing further. Sound staging discipline, transferable to any staged design.
4. **`--dump-flags` + one-time Tauri-command UI sync** (§5.4) — a minimal, concrete fix for the CLI/UI drift that doesn't require rewriting `main.js`'s renderers, just feeding them from the registry once at startup.


## Candidate B — lens: performance-realist

GATE VERDICT: **SCORED**

---

## A. FEASIBILITY RECOMPUTE (first-hand, independent of the candidate's own arithmetic)

I re-derived every number in §6 from the read source files (`wap_warp.comp`, `cli.hpp`, `present.cpp`, `seam_graph.hpp`), not from the candidate's summary.

**Image sizes (verified):** 1920×1080 = 2,073,600 px. RGBA8/R32F = 8,294,400 B (7.91 MiB); RG32F = 16,588,800 B (15.82 MiB); RGBA32F = 33,177,600 B (31.64 MiB). MV grid 240×135 = 32,400 texels; RG16F = 126.6 KiB; RGBA16F = 253.1 KiB; R8 = 31.6 KiB. **Matches the candidate's arithmetic exactly.**

**Resident VRAM added:** 15.82×3 + 31.64 + 7.91 = **87.01 MiB** against 24 GB — trivial, confirmed.

**Per-tick DRAM traffic, 8-pass default graph:** I resummed all 8 rows independently: 39.67+31.79+47.46+31.76+32.01+31.76+71.19+63.40 = **349.04 MiB/tick worst-case** — matches the candidate's "≈349 MiB" exactly.

**L2-credit line item — F1, MINOR:** the candidate claims "crediting AD102's 72 MB L2 for the three repeat reads of prev_real and of cur_real (−47.5 MiB)". I recomputed the actual repeat-read count: `cur_real` is read in 4 passes (1,3,7,8) → 3 repeats after first touch; `prev_real` is read in only 3 passes (3,7,8) → **2 repeats**, not 3 (its first touch is pass 3, not pass 1). Correct credit = (3+2)×7.91 = **39.55 MiB**, not 47.5 MiB. The candidate silently assumed symmetric 3-repeat counts for both images. Corrected total: 349.04−39.55 = **≈309 MiB/tick**, not "≈302". This is a real slip but small (≈2% of the total) — it does not move the stated 300–350 MiB band or the derived 0.42–0.56 ms/tick figure materially (recomputed: 0.43–0.56 ms).

**Barrier baseline — F1, MAJOR:** the candidate states "today's default `wap_warp_present` path records 1 buffer-memory barrier for the mass counter and 1 image barrier for the blit… so +15 barriers per tick." I read `present.cpp` directly (lines 940–1219, default path, `afill`/`fps_overlay` both off as claimed). The actual default-path barrier count is: `vkCmdFillBuffer`→shader barrier (1 buffer), shader-write→transfer-read (1 buffer), transfer-write→host-read (1 buffer), `wapOutA` GENERAL→TRANSFER_SRC (1 image), `bridge_img` UNDEFINED→TRANSFER_DST (1 image), `wapOutA` TRANSFER_SRC→GENERAL (1 image) = **6 barriers today, not 2**. Against the candidate's own 17, the true marginal delta is **≈+11 barriers**, not +15 — a ~27% overstatement of the very axis the performance-realist lens singles out (pipeline-switch/barrier cost). This weakens the "barriers are cheap, the real cost is serialization" argument's framing without invalidating it (still no `ALL_COMMANDS`, still one fence — that qualitative claim holds, verified against `seam_graph.hpp:38-39` and the golden-determinism note).

**Pass-count self-contradiction — F1, MAJOR:** §1.4 asserts unconditionally "`WapPipe` is replaced by **one pipeline per live node**." But §4.9's own live-pass list for the shipping default folds THREE nodes (`base_b_track`, `pin_screen_static`, `pin_stasis`) into ONE pass, "compose" — which is also exactly how §6.2/§6.3's "8 passes / 17 barriers" arithmetic is built. The DAMP slot explicitly declares its within-slot fusion mechanism (singleton fast-path vs. accumulator, "a property of the slot, not of any layer," §3.4); no equivalent mechanism is declared for COMPOSE/PIN's same-key case, so it is unstated *how* three nominally-independent nodes/pipelines collapse into the one pass the budget table depends on. Either the "one pipeline per node" claim is wrong, or the 8-pass/17-barrier figure that anchors the whole M3 arithmetic is undercounted (the honest per-node reading would need 10 passes, not 8, with correspondingly more barriers). This is not fatal (the fusion is algebraically trivial to do — same-key `max` composes fine in one shader) but it is a genuine, first-hand-found gap between the document's general architectural claim and its own worked numbers.

**Fit verdict: TIGHT.** Nothing overflows a hard limit (VRAM, single-GPU, no `ALL_COMMANDS`, single command buffer/fence, no new runtime dependency all verified true). But the M3-relevant numbers, once corrected, are a bit worse than claimed (barrier delta ≈+11 not +15 is actually *less* alarming numerically, but exposes sloppy baseline-reading; the pass-count question is unresolved), and the central M3 denominator — today's warp cost on the *actual named workload* (idle 4090, `ball_zoo`) — remains `unmeasured` by the candidate's own admission. "Fits the envelope" in raw bandwidth/VRAM terms; does **not yet fit** the strict, zero-slack M3 acceptance bar as a *proven* fact.

---

## B. SCORECARD

| Metric | Score | Justification (numbers, not adjectives) |
|---|---|---|
| **M1 · Motion exactness** | **5/5** | `core_sample.comp`'s fetch math is a verified verbatim port of `wap_warp.comp:518-521` (Gate-2 offsets); no binding beyond today's 0–13 is added (confirmed by reading `cli.hpp`/shader bindings); sampling form unchanged ⇒ marker position stays a pure function of the `--qdump+` record, satisfying A0's current "scored by analysis" phase. |
| **M2a · Files to add a layer (≤2)** | **2/5** | The candidate's OWN §1.3 file-placement spec requires 3 new files per layer (`desc.hpp`, `node.comp`, `record.cpp`) + 1 line in `layers.inc` = **4 files** under `git diff --stat`, the literal instrument named. §3.1's claim of "2 files… ≤2" conflates "1 directory" with "1 file." This is a self-contradiction verifiable from the document alone, not an external attack. |
| **M2b · Disabled-layer GPU cost (=0)** | **5/5** | Verified against the actual `seam_graph.hpp` source: backward-reachability culling (`:266-301` region) and the WRITES-loop branch used for the dominance warning (`:411-417`, confirmed present verbatim) are real, existing, golden-tested mechanisms — not a promise. |
| **M2c · Core parameters (≤8)** | **5/5** | 1 push field (`t`). Even generously recounted under A0's own broader "mv-source, output" framing (which treats resource bindings as parameters), the count stays ≤7 — safely under target either way. (Minor: the "1" framing is a bit self-flattering next to A0's own definition, but doesn't change pass/fail.) |
| **M3 · Overhead (no regression)** | **2/5** | Independent recompute: ≈309–349 MiB/tick added traffic ⇒ ≈0.43–0.56 ms/tick ⇒ **10–13% of the 4.17 ms tick at 240 Hz** — magnitude confirmed, but two of the three supporting sub-arithmetic items (L2 credit, barrier-baseline count) were independently found wrong, and the M3-critical denominator (today's warp cost on the *actual* idle-4090 `ball_zoo` workload) is `unmeasured` — not merely uncertain in the candidate's own honest framing, but **undetermined against a strict zero-slack target.** |
| **M4 · Default-output equivalence (VETO)** | **4/5** | Deliberate bug-for-bug reproduction (phase-anchor overwriting bg-reclaim) verified byte-for-byte against `wap_warp.comp:344,358-396,397-410`; RGBA32F/R32F crossings carry no format-quantization loss (verified: these are raw-bit formats, not UNORM); same-key PIN `max` reduction to `wap_warp.comp:1321-1328` verified algebraically. Docked one point: the document never addresses SPIR-V/driver codegen determinism (FMA contraction, instruction scheduling) when one compilation unit becomes eight — a real, if probably small, residual M4 risk left unaddressed analytically (though its proposed B1 milestone would empirically catch it before further investment, which is good process). |

---

## C. ATTACK ON THE CENTRAL CLAIM

The central bet — a fixed 4-stage skeleton with a per-slot composition algebra, culled by the existing SG mechanism — **partially survives** first-hand arithmetic. The competitor is real, not a strawman: the E1 baseline (240 presents/s, 28,799/120s) is confirmed present in `RESTRUCTURE_PLAN.md:99`, and the cited warp cost (`gpu ≈ 3-4 ms`, `rec ≈ 0.03 ms`, BF6 combat, 4090 at 98.8%) is confirmed present verbatim in `UPLOAD_OFFLOAD_MASTER_PLAN.md:25-29`.

**Where it wins:** the culling mechanism (M2b) is real and free, verified against actual code, not aspiration. The 57-field→1-field core reduction is real. Fact 1 (the live KC2 bug in the shipping default) is a genuine, independently-confirmed finding (lines 344/358-396/397-410 read exactly as claimed) — this is the single strongest piece of evidence in the whole document, and it is honestly *not* exploited to silently change the product (the bug is deliberately reproduced, gated behind M1).

**Where it embarrasses itself:** the layer set that matters for M3 — the eight *always-on default* nodes — is precisely the set for which the "disabled layer costs 0" win never fires. Every one of the 8 default passes is live; 100% of the ~309-349 MiB/tick, ~11-17 barriers, and 8× pipeline/dispatch overhead is pure tax on exactly the configuration that must not regress, while today's actual default cost (after the shader's own last-writer-wins collapse, per Fact 2) is close to the ~24 MiB single-fused-dispatch baseline the candidate cites. The modularity payoff (M2) is real for the *non-default*, *disabled* 30+ layers; the overhead bill (M3) is charged entirely against the *default*, *always-live* 8. That asymmetry is the honest shape of this candidate's trade, and the document itself half-admits it in §8 ("the exchange rate is not yet known").

---

## D. KILL CRITERIA

1. **Unmeasurable** — passes (no new external input beyond bindings 0-13, verified; sampling form unchanged).
2. **Order-dependent semantics** — passes under DESIGNER_BRIEF's own reading of KC2 ("order-independent OR an EXPLICIT declared order that is part of the contract"): the phase-anchor/bg-reclaim dominance is declared, printed by `--sg-dump`, and gated by an explicit `.dominates_ok` annotation rather than silent.
3. **Seam regression** — passes: single command buffer, single submit, single fence (verified consistent with `seam_graph.hpp`'s own explicit `VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT` prohibition at its header, confirmed present).
4. **Cannot express the shipping default** — passes: all 8 default-affecting layers mapped (§4), verified against `cli.hpp` defaults line-for-line.
5. **Injection/multi-GPU** — passes: untouched, inherits existing capture identity.
6. **New runtime dependency** — passes: `seam_graph.hpp` confirmed header-only over Vulkan + STL.

All six criteria are crossed with margin, on the evidence read.

## E. CONSTRAINT SMUGGLING

None found. The push-constant override chain (the explicit target of the ban) is the one thing this candidate demonstrably removes, not reintroduces. No perceptual-quality selection criterion, no `--minimal` strip-flag, no present-hook, no "fewer layers" slogan (explicitly: layers are *moved*, not dropped). Clean.

## F. NOVELTY/OPTIMALITY HONESTY

No unscoped "optimal"/"novel" claims found. The document is unusually disciplined about trust tiers throughout and explicitly refuses to claim the bandwidth cost is free (§8: "the exchange rate is not yet known"). No F5 violation.

---

## DEFECTS

| # | Class | Severity | Finding |
|---|---|---|---|
| 1 | F1 | **MAJOR** | M2a's "2 files" claim is contradicted by the candidate's own §1.3 file layout (3 new files + 1 registration line = 4 under `git diff --stat`), a self-inconsistency, not an external attack. |
| 2 | F1 | **MAJOR** | The "+15 barriers" M3 figure understates today's actual default-path barrier count (verified 6, not 2, from `present.cpp`); true delta ≈+11. Weakens (does not reverse) the barrier-cost argument. |
| 3 | F1 | **MAJOR** | §1.4's "one pipeline per live node" is contradicted by §4.9's own 8-pass default-graph table, which fuses 3 nodes (`base_b_track`+`pin_screen_static`+`pin_stasis`) into one "compose" pass with no declared fusion mechanism (unlike DAMP's explicit singleton/accumulator rule) — leaves the pass/barrier count the whole M3 arithmetic depends on under-specified. |
| 4 | F1 | MINOR | L2-cache credit (§6.2) overstated: −47.5 MiB claimed vs −39.55 MiB by direct recount (prev_real has only 2 repeat reads, not 3, since its first touch is pass 3 not pass 1). Corrected traffic ≈309 MiB/tick vs claimed ≈302; does not move the stated band or downstream ms figures materially. |
| 5 | F2 | MINOR | M4's byte-identity argument (§8, fp32 crossings) does not address SPIR-V/driver codegen determinism (FMA contraction, scheduling differences) between one fused compilation unit and eight separate ones — a real, if likely small, unaddressed residual risk. Partially mitigated procedurally by the B1 milestone's empirical byte-diff test. |

No FATAL finding: nothing here breaks the hard envelope (VRAM/bandwidth/no-new-deps/single-GPU) or falsifies the structural claim that culling and the composition algebra work as described (both independently verified against real `seam_graph.hpp` code). The defects sit entirely in the M2a/M3 self-grading arithmetic, not in overall feasibility.

---

## SALVAGEABLES (present in the candidate, nothing added)

1. **The dominance-warning mechanism** (§3.5 / Appendix A.1) — instrumenting `SeamGraph::compile()`'s existing WAW branch (`:411-417`, confirmed present verbatim) to emit a build-time warning naming both parties whenever a pass overwrites a resource it doesn't read. Cheap (~10 lines), general, independent of whether the rest of this candidate is adopted.
2. **Fact 1 itself** — the first-hand-confirmed discovery that `bg_reclaim`'s computed damp is unconditionally discarded by `phase_anchor` under the shipping default (verified at `wap_warp.comp:344,358-396,397-410`). Real, standalone knowledge for the eventual M1-gated product decision regardless of which candidate wins.
3. **`optional_write` / liveness-driven spec constants** (Appendix A.2) — compiling out unused output stores via a spec-constant driven by graph liveness (`kStoreA`), a reusable technique independent of the stage-skeleton architecture.
4. **The `.dominates_ok` escape-hatch pattern** — forcing every discovered order-dependency to be either fixed or explicitly justified in source, turning a silent hazard into a greppable, reviewable artifact.
5. **The B1 "two-pass identity split" milestone** (§6.5) — a small, cheap, falsifiable first experiment (byte-diff M4 + `--csv` M3 + `--sg-dump` M2b, all before any further investment) that tests exactly the riskiest assumption first. Good staged-risk process, salvageable regardless of the architectural verdict.
6. **The zero-allocation `execute()` fix** (Appendix A.3) — flags and fixes a real, currently-existing defect in the ADOPTED `seam_graph.hpp` (a per-pass, per-call heap allocation at `:458-459`, confirmed present) that costs ~1,920 allocs/s at 240 Hz. Worth fixing in SG regardless of which candidate is chosen.


## Candidate B — lens: adoption-skeptic

SCORED

## A. Feasibility recompute (first-hand, independent of the candidate's own arithmetic)

Redone from scratch against `wap_warp.comp` bindings/formats and cross-checked line-by-line:

- 1920×1080 = 2,073,600 px. RGBA8=8,294,400 B · R32F=8,294,400 B · RG32F=16,588,800 B · RGBA32F=33,177,600 B. MV grid 240×135=32,400 texels: RG16F=129,600 B, RGBA16F=259,200 B, R8=32,400 B. All match §6.
- Resident VRAM added: mv_fwd+mv_cond+mv_samp (RG32F×3=47.46 MiB) + samp_B (RGBA32F=31.64 MiB) + d_pixel (R32F=7.91 MiB) = **87.01 MiB** — confirmed by hand, negligible against 24 GB.
- Per-tick traffic: I re-summed all 8 rows of §6.2 independently (39.67+31.79+47.46+31.76+32.01+31.76+71.19+63.40) = **349.04 MiB/tick**, matching the candidate's total exactly. With the L2-credit subtraction (−47.5 MiB) → **≈302 MiB/tick**, also matches.
- Time: 1,008 GB/s = 938.7 GiB/s; at 65–75% achievable (610–704 GiB/s) → 302–349 MiB ⁄ (624,640–720,896 MiB/s) = **0.419–0.559 ms/tick**, matching the claimed 0.42–0.56 ms. At 240 Hz (4.167 ms budget) = **10.1–13.4%**; at 180 Hz (5.556 ms) = **7.5–10.1%**. Both confirmed.
- Barrier census: 2+2+2+3+3+2+1+2 = **17**, confirmed by hand against `seam_graph.hpp`'s actual WAW/RAW/WAR logic (read first-hand, lines 343–448 — the `else if (s.has_producer && !read_here)` WAW branch is at exactly 411–417 as cited).

**Verdict: TIGHT — fits VRAM capacity trivially, consumes 10–13% of the panel's tick-time budget in bandwidth alone, with no measured denominator on the actual named workload (see M3, F2).** Not overflowing, not comfortably fitting.

**Under-priced line item (F1, MINOR):** the "today ≈24.0 MiB/tick" baseline (§6.2) omits `mvb_grid`/`cand_grid`/`mvt_grid` reads that the *current* shipping default already performs (phase-anchor, ambig, vblend all read them today, verified against `wap_warp.comp:398,412-444,507-514`) — baseline understated by ~0.5 MiB (~2%), marginally inflating the quoted 13–15× ratio. Immaterial to the verdict.

## B. Scorecard

| Metric | Score | Basis |
|---|---|---|
| **M1** Motion exactness | 4/5 | `core_sample.comp` verified textually near-identical to `wap_warp.comp:518-522` (read first-hand, confirmed). MV provenance chain is a faithful decomposition including Fact-1's known defect, deliberately kept for M4 parity. Point withheld: exactness for *future* layer additions depends entirely on the dominance-warning firing correctly (see F2/F3) — a necessary, not sufficient, guarantee. Scored by analysis only, per A0's gating note (instrument not built). |
| **M2a** Files to add a layer | 2/5 | Candidate's own §1.3 tree requires `desc.hpp` + `node.comp` + `record.cpp` (3 new files) + 1 line in `layers.inc` (1 modified file) = **4 files** under `git diff --stat`, against the claimed "2 files" (§3.1) and the objective's ≤2 target. The candidate's own file-placement table falsifies its own headline claim (F1, MAJOR). |
| **M2b** GPU work of disabled layer = 0 | 5/5 | Verified against the real `seam_graph.hpp` (backward-reachability cull at lines 266-301, confirmed by direct read) plus "never `add_pass`'d if `enable_flag` off." Mechanism is real, pre-existing, golden-tested — not a promise. |
| **M2c** Core parameters ≤8 | 5/5 | Core push = `{float t;}` — 1 field, verified in the shown shader text; gate thresholds demonstrably relocated to `SAMPLE/GATE` UBOs and culled by default (§4.9). Beats target and A0's own "needs 6" baseline. |
| **M3** Overhead | 2/5 | Every number in §6.2-6.4 independently recomputed and confirmed correct (0.42-0.56 ms/tick, 10-13%/7.5-10% of tick, 17 barriers, ~+36-40µs CPU record). But the one number that would settle affordability — today's warp GPU cost on the objective's *actual named workload* (`ball_zoo`, idle 4090) — is explicitly `unmeasured`; the only measured figure (3-4ms) is from a saturated-GPU BF6-combat regime, materially different from the target workload. A real, sizeable cost proposed against an unmeasured budget. |
| **M4** Default-output equivalence (veto) | 4/5 | Every one of the 8 default nodes checked first-hand against its cited `wap_warp.comp` region: `mv_fetch_guided`, `inertia_restrict`, `bg_reclaim`, `ambig_arbitrate`, `vblend_predict` all verified as direct ports; the PIN max-composition independently verified algebraically identical to lines 1321-1330 (`max(w_s, stasis?1:0) ≡ if(stasis) w_s=1.0`, confirmed by substitution). Held below 5 because M4 is asserted, not measured — B1 (§6.5) is the first point this becomes instrument-backed rather than argument. |

## C. Attack on the central claim

The competitor is real, not a strawman — every cited default and line number in `wap_warp.comp`/`cli.hpp` checks out on first-hand read, including the load-bearing Fact 1 (bg-reclaim's damp discarded by phase-anchor, confirmed at `wap_warp.comp:344,394,410` against defaults at `cli.hpp:470,566,409`).

**Where it wins:** M2b (real, mechanized zero-cost culling — today's fused shader cannot claim this at all, since a disabled layer there still costs a per-thread branch check inside the one permanent dispatch) and M2c (1 param vs today's 57). These are genuine, verified structural wins over "the existing FG's WAP as-is."

**Where it is embarrassed:** the headline bet — "the composition rule of each slot is an algebra... rather than an assignment" (§0) — does **not** hold *across* the six COND sub-slots (`FETCH/RESTRICT/DAMP/ANCHOR/ARBITRATE/PREDICT`), which by the candidate's own §3.2 rule all share write-ownership of `mv_cond`/`mv_samp`. Cross-slot composition is bare sequential RMW in registry/slot order — structurally the same "last write wins" pattern KC2 names — with only a build-time warning-promoted-to-error (`dominates_ok`) standing between silent and loud order-dependence (§3.5, verified real against `seam_graph.hpp:411-417`). The candidate discloses this and confines it to exactly one instance in the default set (phase-anchor over bg-reclaim, the pre-existing bug), which is honest — but it means the "algebra, not assignment" claim is true *within* a slot and false *across* slots, undermining the document's own framing in §0/§3.4. Combined with the M2a miscount (F1) and M3's unmeasured denominator, the candidate's own §8 concedes the natural remedy for a bad M3 result collapses it toward Angle A ("progressively less distinguishable from a fused design"), which is the designer's own admission that Angle B's distinguishing property is the one most at risk.

## D. Kill criteria

| KC | Verdict | Note |
|---|---|---|
| 1 (measurability) | Passes, with caveat | Honestly flags a pre-existing gap in the instrument's declared record fields (§4.10), common to any candidate, not unique to this one. |
| 2 (order-dependent semantics) | **Does NOT cross with clean margin (F3, MAJOR)** | Structurally permits last-writer-wins across COND sub-slots (see Section C) and exercises it once, by design, in the shipping default. Mechanized detection gates it but does not eliminate it — a judgment call for AT3/AT4, not a clean pass. |
| 3 (no per-stage fence/host round-trip/ALL_COMMANDS) | Passes with margin | Verified directly in `seam_graph.hpp`: single `cmdBridge`/single fence path, precise stage/access masks only, no `ALL_COMMANDS` token anywhere in the 549-line file. |
| 4 (must host default set) | Passes with margin | All 8 default-affecting layers mapped with cited, verified line ranges. |
| 5 (injection/multi-GPU) | Passes trivially | Not touched; uses existing external capture. |
| 6 (new runtime dependency) | Passes trivially | `seam_graph.hpp` confirmed to include only `<vulkan/vulkan.h>` + STL. |

## E. Constraint smuggling (A0 §6)

No evidence of: a `--minimal` strip flag, perceptual metrics used as selection criteria, present-hook/injection, or the 57-field push chain reintroduced under a new name (core push independently verified at exactly 1 field). **MINOR (F4):** the `COND/DAMP` slot is labeled "commutative" in the summary table (§1.2) but immediately qualified in §3.4 as "numerically order-dependent in the last ULP" for ≥2 nodes — disclosed in the same document, so a labeling looseness rather than a hidden reintroduction. The more serious instance of order-dependence (cross-slot, COND stage) is scored under F2/F3 above rather than here, since it is explicitly disclosed rather than smuggled.

## F. Novelty/optimality honesty

No unscoped "optimal"/"novel" claims found (F5: none). Every figure I independently recomputed (§6.2's traffic table, byte sizes, barrier count, bandwidth-to-time conversion) matched the candidate's own numbers exactly. The document explicitly declines to claim victory on M3 ("a number without a denominator") and explicitly names its own worst-case convergence toward the rival Angle A.

## G. Salvageables (present in the candidate only)

1. **Dominance-warning mechanism** (§3.5, Appendix A.1) — reuses `seam_graph.hpp`'s existing WAW branch (verified real, `:411-417`) to surface any write-without-read as a named, build-time warning, promotable to a hard error with an explicit `.dominates_ok` escape hatch. Cheap (~10 lines per the candidate), independently useful for catching this exact defect class regardless of which angle is chosen.
2. **`optional_write` + liveness-driven specialization constants** (Appendix A.2) — compiles out a store when nothing live reads it; general technique, not angle-specific.
3. **The B1 milestone** ("two-pass identity split," §6.5) — a small, cheap, immediately buildable experiment that puts the fp32-sample-crossing bandwidth question on a real instrument (byte-diff + `ball_zoo.ps1`) before any further engineering commitment, independent of which angle wins.
4. **Zero-allocation `execute()` fix** (Appendix A.3) — removes ~1,920 heap allocations/s from the present thread at 240 Hz; a real, standalone bug fix worth doing regardless of this search's outcome.
5. **The fp16-vs-fp32 PSNR argument for stage-crossing images** (§8) — a reusable, quantitatively worked argument (≈6% of pixels ±1 LSB ⇒ PSNR≈60.3 dB, sitting on M4's threshold) for why any staged design's sample-packet crossing needs fp32, not fp16.


# ===== CANDIDATE C =====


## Candidate C — lens: budget-auditor

SCORED

## A. Feasibility recompute (first-hand)

Verified directly against source (not the candidate's self-report):

- **Push-constant block** `shaders/wap_warp.comp:77–135` — counted 57 declared fields (56 scalars + 1 `vec2`) = 58 floats = 232 B. Matches candidate exactly.
- **Gate derivation + push assembly** `present.cpp:968–1150` — confirmed the ~140-line `*_push` boolean chain and the anonymous 58-field struct at line 1116, `vkCmdPushConstants` at 1151. Matches.
- **Default-path barriers** `present.cpp:1215/1216/1218` — confirmed exactly 3 `img_barrier` calls around the output blit in the no-`--afill`/no-overlay/no-`--ts-smooth` path. Matches.
- **SG citations** `seam_graph.hpp` — confirmed the backward-reachability cull (266–306), the same-scope-reader barrier-suppression logic (~361–390), and the "+1 pass == +1 barrier" comment (~line 44–47). Real, not fabricated.
- **CMake codegen precedent** — confirmed `pfg_spv()` at `CMakeLists.txt:30–41` runs `cmake -P .../spv_to_header.cmake`. Real precedent, though it converts one `.spv`→header; parsing an X-macro `.def` into six generated GLSL fragments + std140 offsets in CMake script mode is a materially harder text-processing job than the cited precedent, a gap the candidate itself flags (Python fallback).
- **Flag counts** — `grep -o` on `cli.cpp` → **259** (candidate: 259, matches); `grep -c 'flag: "'` on `main.js` → **173** (matches exactly).
- **Descriptor bindings** — confirmed 0–13 in use; binding 15 for `LayerParams` is free (binding 14 silently skipped — cosmetic, not a defect).

**My own bandwidth recompute** (the one figure the candidate labels `estimated`): 15.83 MiB = 16,598,780 B; ÷ 1.008×10¹² B/s (1008 GB/s decimal) = **16.47 µs**, not the claimed 15.7 µs — a ~5% understatement from mixing binary MiB against decimal GB/s. At 240 ticks/s that's ≈0.40% of wall time, not 0.38%. Trivial in isolation, but it is exactly the kind of quiet unit-conflation the lens asks to hunt for (F1, MINOR) — and it sits in the one part of the arithmetic that IS an estimate.

**Fit verdict: FITS, with wide margin.** For the actually-committed default set: 1 dispatch (unchanged), 0 intermediate images, 0 added barriers, 1 new descriptor binding, CPU push 232 B→20 B. None of this is close to any wall in the envelope (RTX 4090, 1920×1080, 240 Hz panel). **The real wall this candidate is exposed to is not in the GPU/byte/barrier arithmetic at all — it's unbudgeted build-tooling complexity** (a CMake-script-mode parser for a bespoke `.def` DSL) and **unbudgeted schema-convergence risk** (§7.2: 8 layers forced 4 new schema columns; 53 layers remain unmapped; the candidate's own falsification test — map 10 more, count new columns — has not been run). Neither has a byte/pass/barrier unit to recompute against, so it doesn't move the numeric fit verdict, but it is where this design's actual risk lives, and the candidate names it rather than hiding it.

## B. Scorecard

| Metric | Score | Justification |
|---|---|---|
| **M1** Motion exactness | **5/5** | `fg_core()` is a verbatim port of `wap_warp.comp:496–529,637,679`; identical sampling form, identical MV path. One real bit-level risk (mv_guided packing, §4.1) is proactively flagged with a two-phase measurement protocol rather than hidden. |
| **M2a** Files/layer (≤2) | **5/5** | Exactly 2 files (`.def` block + `.glsl` body), verified against the mechanism described; 4-file exception for a new device image is disclosed, not hidden. |
| **M2b** Disabled-layer GPU cost (=0) | **2/5** | For the actual shipping configuration, **all 8 default rows are `kind=F`** — there is no per-layer SG node, so the stated instrument (`--sg-dump` pass/barrier census) has nothing to attribute "0" to. The claim rests entirely on unmeasured driver spec-constant dead-code-elimination, with the candidate's own honest floor being "same cost as today's branch" if that fails. The candidate itself declines to score this as passed (§7.3) — I agree and mark it down accordingly. |
| **M2c** Core params (≤8) | **4/5** | Meets the target on the objective's own enumeration (6) and on descriptor count (5); misses it on raw signature arity (9). Disclosed honestly, small deduction for not meeting all three readings. |
| **M3** Overhead (no regression) | **4/5** | Architecturally non-regressive by construction (same dispatch count, smaller push, no new barriers for default set) but zero measured numbers exist yet — `estimated`/`unmeasured` throughout §6.4/§6.6, correctly labeled as such. |
| **M4** Default-output equivalence (veto) | **not yet clearable — no structural blocker** | 7/8 rows claimed byte-identical with source-line justification; 1/8 (mv_guided) carries a disclosed 1-ulp risk with a concrete resolution protocol. Nothing in the design precludes passing M4; it is simply unproven until M-C2's actual byte-diff run. |

## C. Attack on the central claim

The bet has two halves. **Half one — "kill the four-site drift structurally" — survives.** M2a is exactly met (2 files, mechanically verified), and the mechanism (one `.def` generating Config/parser/help/UI-JSON) is architecturally real, not aspirational — precedented by the existing `pfg_spv()` codegen step. This is where the candidate clearly wins, and the competitor (today's `wap_warp.comp` + `cli.cpp`/`cli.hpp`/`main.js`, all read first-hand) is real, not a strawman.

**Half two — "and that structurally kills the order defect too" — cracks on exactly the layer that matters most.** `single_track` (the layer whose last-writer override IS today's defect, per the objective's own §0) forces a `shadows` column that reaches backward into the core's blend internals (`wa_eff`, `blend_result` base, forced selection, commit inertness) — the candidate's own words: "precisely the coupling the stage model is supposed to forbid." So the layer set where LAYERTAB embarrasses itself is the shipping default's own flagship override layer — the one the whole search exists to fix. The candidate names this rather than hiding it (§7.2 defect #2), which is to its credit, but it does mean the central claim's second half is not structurally proven, only structurally *declared and printed* (an audit trail, not an elimination).

## D. Kill criteria

All six clear with margin, verified first-hand (not merely asserted): KC1 via unchanged sampling form; KC2 via no shared mutable `result` + `static_assert`ed unique `rank` + printed `--layer-dump` (the `overrides` bit is a declared, hashed fact, not a runtime accident — legitimate under the brief's "explicit declared order" option, not order-independence); KC3 via SG's own precise-mask derivation (confirmed against `seam_graph.hpp` source, no `ALL_COMMANDS`/fence/round-trip introduced); KC4 via the full 8-row mapping with source lines; KC5/KC6 clear (no injection, no multi-GPU, codegen is build-time only, confirmed against the real `pfg_spv()` precedent).

## E. Constraint smuggling

None found. Explicitly non-subtractive (all 53 layers preserved, "fewer layers" is not invoked as a virtue); no perceptual/ghosting metric used for selection; no `--minimal` strip-flag; no injection/present-hook; the push-constant override chain is genuinely replaced (UBO + spec constants + `arm_mask`), not renamed.

## F. Novelty/optimality honesty

Clean. No unscoped "optimal"/"novel" claims found anywhere in the document. The document is unusually self-critical (three-way honest readings of M2c, an entire named-weakest-point section, explicit "I decline to score M2b as passed").

## Defects

1. **[F1, MINOR]** Bandwidth-time estimate (§6.2) mixes binary MiB against decimal GB/s: recomputed 16.47 µs vs the claimed 15.7 µs (~5% understatement). Doesn't change the fit verdict (still <1% of frame budget either way) but is a real unit-conflation error in the one figure explicitly marked `estimated`.
2. **[F1, MAJOR]** §6.4's claim that "four of the eight default layers must disarm per generation" citing `gme_push, bwd_push, matte_push, appear_push` is internally inconsistent with the candidate's own §3.5/§4 mapping: only 3 of the 8 default rows (`bg_reclaim`, `phase_anchor`, `ambig`) carry non-`ALWAYS` arm, using 2 arm classes (GME, BWD) — `matte_push`/`appear_push` govern layers that are OFF by default and outside the eight. The `arm_mask` mechanism itself is still sound; the stated rationale for it is miscounted.
3. **[F1, MAJOR]** M2b's "0" is architecturally unattributable by the stated instrument for the actual shipping configuration (all rows `kind=F`, no per-layer SG node exists to measure). The candidate half-concedes this in §7.3 but the scorecard/summary sections elsewhere still list it as satisfied — this should be carried through consistently as an open item, not a cleared one.
4. **[F2, MAJOR]** The central claim's "kills the order defect structurally" half is not fully borne out: the one layer that embodies today's defect (`single_track`) needs a backward-reaching `shadows` escape hatch that the candidate itself names as the exact coupling the model is supposed to forbid. Self-disclosed, but it is the layer set that most embarrasses the bet, not a peripheral one.

No F3 or F5 findings.

## Salvageables (candidate's own ideas only)

1. The `.def`/X-macro single source of truth generating `Config` + parser + help + UI-JSON model — mechanically verified to close the 4-site drift at 2 files/layer, independent of whether the shader side ends up fused or staged.
2. `--layer-model-json` + a thin Tauri command that makes the UI a **renderer of the binary's own model** rather than a second hand-maintained list — structurally (not just practically) prevents the 84-flag CLI/UI gap from recurring.
3. The `arm` column + runtime `arm_mask`, layered on top of static spec-constant compile-out — a genuinely necessary fix for the fact that some gates (`gme_push`, `bwd_ok`) are per-generation-dynamic, not just per-config-static; a pure "compile it out" design misses this entirely.
4. The FNV-1a 64-bit contract hash stamped into `--csv`/`--qdump` telemetry — makes a replay record attributable to an exact layer contract, useful for M1/M4 measurement discipline regardless of which candidate is chosen.
5. The `overrides` bit + build-time check that a COMPOSE row referencing `c_in` must not declare `overrides=false` — a cheap, real guard against silently reintroducing today's accident.
6. The self-named convergence experiment (map 10 more layers onto the schema, count new columns; ≤1 converges, ≥4 falsifies the central claim) — a concrete, cheap test that survives independent of this candidate's fate.


## Candidate C — lens: performance-realist

GATE VERDICT: **SCORED**

*(First-hand recompute performed: push-constant field count re-derived from `shaders/wap_warp.comp` [57 fields / 58 scalar floats — cam_lead is vec2 — confirmed against `present.cpp:1119` struct, 232B], barrier line numbers 1215/1216/1218 confirmed byte-for-byte, `seam_graph.hpp` citations at lines 38-39/47/266-306/361-390 confirmed, CLI-flag grep re-run live [259, confirmed], UI `flag:` count re-run live [173, confirmed], `single_track`/`stasis` shader body at lines 1191/1315-1329 diffed against the candidate's GLSL reproduction, bandwidth arithmetic for the RTX 4090 at 1008 GB/s independently redone.)*

---

## A. Feasibility recompute (first-hand, not the candidate's number)

**Default layer set (all 8 rows `kind=F`): 1 compute dispatch, 0 added bytes, 0 added barriers, 3 pre-existing image barriers unchanged (verified at present.cpp:1215/1216/1218), push constant shrinks 232B→20B.** This is not a new arithmetic result — it is the *current shipping config*, unchanged. My redo agrees: **fits, wide margin.**

Independently redone worst-case (5 MVCOND rows split to `kind=P`): 5 × 15.83 MiB (R+W of a 7.91 MiB R16G16_SFLOAT full-res channel) = 79.15 MiB/tick → 82.3 µs/tick at 1008 GB/s → 240 × 82.3 µs = 19.76 ms/s ≈ **1.98 %** of the 4.17 ms/240 Hz tick budget. Candidate says "≈1.9 %" — close enough to confirm the qualitative claim (a single split is cheap; several are still cheap on this rig), but the single-split figure itself is off: candidate's "≈15.7 µs" for 15.83 MiB at 1008 GB/s is obtained by treating "MiB" as decimal MB (15.83×10⁶/1.008×10¹²=15.7µs); the binary-correct value is 15.83×1,048,576/1.008×10¹²≈**16.5 µs** (≈5 % low). Immaterial to the verdict (both are <<4.17 ms) but it is exactly the kind of unit slip a bandwidth-per-tick review is supposed to catch — **F1, MINOR**.

**Verdict: fits.**

---

## B. Scorecard

| Metric | Score | Justification |
|---|---|---|
| **M1** motion exactness | **4/5** | Core math is the objective's own lines (496-529/637/679) verified verbatim; MV path, sampling form, `t`-entry unchanged. 7/8 default layers argued byte-identical from source correspondence (spot-checked: `single_track`/`stasis` body matches lines 1191, 1315-1329 closely). 1/8 (`mv_guided`, §4.1) has an admitted, IEEE-754-real 1-ulp risk from de-packing `1.0+sim`→raw `sim`; disclosed with a two-phase verification protocol, not yet executed. |
| **M2a** files/layer | **5/5** | 2 files, meets target exactly; independently reproduced the cited counts (259 CLI flags via live grep, 173 UI entries via live grep, both exact). |
| **M2b** disabled-layer GPU work = 0 | **2/5** | Not demonstrable by the objective's own instrument (`--sg-dump` pass/barrier census) for the case that is actually the shipping default: all 8 rows are `kind=F`, so none of them are ever SG passes — there is nothing in the census to read "0" off of. The "0" is undecided-by-instrument, not measured. Candidate itself declines to self-score this (§7.3) — I score it lower than that self-assessment because the gap is not cosmetic, it is the stated M2 instrument being blind to the candidate's own preferred configuration. |
| **M2c** ≤8 core params | **4/5** | Meets the objective's own enumeration (6) and descriptor count (5); misses raw-argument count (9) by one, transparently reported. |
| **M3** overhead | **5/5** | Default-set arithmetic is identical-or-better than today on every line (1 dispatch, 0 added images/barriers, 20B vs 232B push); independent recompute confirms even the pessimistic multi-split scenario stays under 2% of the 240 Hz tick budget. The only blemish (F1 above) doesn't move this. |
| **M4** default equivalence (veto) | **3/5** | Not measured (design-stage document — correctly labeled `unmeasured`/`computed` throughout). 7/8 mappings independently spot-checked and consistent with source. The 1 flagged risk (mv_guided) plus the `shadows` coupling (below) mean this metric is a *plan to close*, not a closed veto — and it is the metric with veto power. |

---

## C. Attack on the central claim

The bet (§0): "once a layer is a row... 'the frame that ships is whatever wrote result last' stops being emergent and becomes a printable, hashable fact." For the layer that *is* the objective's convicted defect — `single_track` at line 1321, discarding lines 531-1320 — this is only **partly** true. §3.2 declares a COMPOSE row "may write: its return value only." §4.7/§7.2 then admit `single_track` needs a **`shadows`** column of four extra spec constants the *core itself* reads (`wa_eff` collapse, `blend_result` base, forced warp selection, commit inertness) — a COMPOSE row reaching backward into the core, exactly the coupling the stage model exists to forbid. The candidate names this itself as "precisely the coupling the stage model is supposed to forbid" and "a genuine wart" — which is honest, but it means the central claim ("structurally kills the defect") is overstated for the one case that matters most: the coupling is not eliminated, it is *made visible*. That is real, salvageable progress (an invisible four-site accident becomes one printed, hashed column) — but it is a demotion from the paper's own framing, and it directly touches KC2, which the self-check (§9) calls "Clear" without naming this caveat in that table row. — **F2, MAJOR.**

Is the competitor real or a strawman? Real: the comparison is against the actual shipping `wap_warp.comp` (1,336 lines, read first-hand and independently spot-checked here), not a rebuilt strawman, and the SG citations (`seam_graph.hpp:38-39/47/266-306/361-390`) check out exactly against the real file.

**Layer set where it wins:** the shipping default (all 8, `kind=F`) — it collapses to Angle A's own bet (one fused dispatch, spec-constant compile-out) while *also* closing M2's four-site drift, at zero measured/computed bandwidth cost. On this named workload it is the strongest of the three angles on paper.

**Layer set where it embarrasses itself:** any future layer needing live toggle-without-restart, or a genuinely dynamic new image. Toggling degrades from a free push value (today) to a `vkCreateComputePipeline` rebuild, `unmeasured`, "typically O(1-10ms)" (§6.6) — a real regression axis the candidate names but does not close. And §6.4's own load-bearing arithmetic ("four of the eight default layers must disarm per generation: gme_push, bwd_push, matte_push, appear_push") is itself imprecise: independently checked against `present.cpp:960-1055`, `matte_push` and `appear_push` gate matte/appearance features that are **default-OFF**, not among the 8 default-affecting rows enumerated in §4 — only `gme_push` (covering bg_reclaim + ambig) and `bwd_push` (covering phase_anchor) actually apply to the default set. The arm-mask mechanism itself is unaffected, but the document over-counts its own scope here. — **F1, MINOR.**

---

## D. Kill criteria (with margin?)

| # | Criterion | Verdict | Margin |
|---|---|---|---|
| 1 | Unmeasurable | Clear | Wide — MV path/sampling form untouched, verified against source. |
| 2 | Order-dependent semantics | Clear, but thin | The `rank`/no-shared-`result` mechanism holds everywhere *except* the `shadows` backdoor (§C above) on exactly the historically defective layer. Not a KC2 failure (order is still declared, not ambiguous), but not the clean margin the self-check table implies either. |
| 3 | Seam regression | Clear | Wide — verified 3 pre-existing barriers unchanged at exact line numbers; SG's precise-mask derivation confirmed in source for `kind=P`. |
| 4 | Cannot express default | Clear | Wide — all 8 verified against source line-for-line, including a direct diff of the `single_track`/`stasis` GLSL against the real shader body. |
| 5 | Injection/multi-GPU | Clear | Wide — nothing touches capture identity or device count. |
| 6 | New runtime dependency | Clear | Wide — codegen is `cmake -P`, precedent confirmed exactly at `CMakeLists.txt:30-41`. |

---

## E. Constraint smuggling

No clean violation of A0 §6's forbidden list. One boundary case worth naming: `arm_mask` reintroduces a **push-constant bitmask gating layer behavior at dispatch time** — not the forbidden mechanism (it gates per-generation *validity*, not compositional *order/override*; `rank` alone still owns order), but it is close enough in shape to the retired pattern that a reviewer should watch it as the schema grows. **F4, MINOR.**

---

## F. Novelty/optimality honesty

No unscoped "optimal"/"novel" claims found. The document is consistently hedged (`estimated`/`unmeasured`/"I decline to score..."). **Clean.**

---

## Defect list

- **F2 (MAJOR)** — the `shadows` column: a COMPOSE row (`single_track`, the convicted-defect site) reaches backward into the core via four extra spec constants, contradicting §3.2's own "return value only" contract. Central claim ("structurally kills the defect") does not fully hold for the layer it matters most for.
- **F3 (MAJOR)** — M2b is undecidable by the objective's own stated instrument (`--sg-dump` pass/barrier census) for the shipping-default configuration, since all 8 rows are `kind=F` and never appear as SG passes.
- **F1 (MINOR)** — bandwidth-time arithmetic in §6.2 mixes binary MiB and decimal GB, understating the per-split time by ≈5% (16.5µs actual vs 15.7µs claimed); doesn't change the fits/tight/overflows verdict.
- **F1 (MINOR)** — §6.4's "four of the eight default layers... disarm per generation" over-counts scope: `matte_push`/`appear_push` gate default-OFF features, not members of the 8-row default set; actual default-set arming need is `gme_push`+`bwd_push` only.
- **F2 (MINOR, disclosed)** — §4.1 `mv_guided` de-packing carries a real (candidate-computed) IEEE-754 1-ulp risk against the M4 veto; mitigated by a stated two-phase protocol, not yet executed.
- **F4 (MINOR)** — `arm_mask` is a push-constant-driven runtime gate on layer behavior; boundary-adjacent to the forbidden "push-constant override chain as layer mechanism" driver, though it gates validity not composition order.

## Salvageables (present in the candidate only)

1. The FNV-1a 64-bit **contract hash** stamped into `--csv`/`--qdump` telemetry (§3.5.5) — closes the "which build produced this CSV" attribution gap for A/B comparisons, independent of whether the table mechanism wins.
2. **M-C1** as a staged milestone: ship the CLI/UI drift-fix generator with `shaders/wap_warp.comp` completely untouched, gated by a runtime `layer_config_parity()` self-check against the legacy `Config` — decouples M2 delivery from M1/M4 shader risk.
3. The declared `overrides` bit + a generator-enforced check ("a COMPOSE row's body must reference `c_in` unless `overrides=true`") — a cheap, concrete static check aimed directly at the convicted defect, salvageable regardless of the rest of the schema.
4. The `arm` column distinguishing statically-disabled (spec constant) vs per-generation-disarmed (runtime bit) layer validity — names a real distinction today's shader encodes only ad hoc in `present.cpp`'s push-derivation block.
5. `flag = nullptr` for internal parameters (tie_ratio, min_sep_px, matte_thresh-style magic numbers) — promotes a hidden constant to a named, dumped, hash-covered value without forcing it onto the CLI/UI surface.


## Candidate C — lens: adoption-skeptic

**SCORED**

## A. Feasibility recompute (first-hand)

Default set (M-C1/M-C2, all 8 rows `kind=F`), recomputed independently against `A0` §3:

| Item | My recompute | Candidate's claim |
|---|---|---|
| Dispatches/tick | 1 (verified: single `vkCmdDispatch` in `present.cpp`, unchanged) | 1 |
| Barriers added by the layer mechanism | 0 (fused, register-resident; the 3 `img_barrier` calls around the output blit are pre-existing plumbing, not layer cost — verified at `present.cpp:1215-1218`, exact match) | 0 |
| Intermediate image bytes | 0 | 0 |
| Push block | 3×f32+f32+u32 = 20 B (recomputed field-by-field, matches `static_assert`) | 20 B |
| Layer UBO, 8 rows | 10 scalar fields × 4 B = 40 B, std140-rounds to 48 B (recomputed) | 40 B / 48 B |
| Hypothetical 1 kind=P split, 1920×1080 | 8.29 MB/channel ×2(R+W) ≈ 16.6 MB ÷ 1,008 GB/s ≈ 16.5 µs/tick ≈ 0.4 % at 240 Hz (my recompute) | ≈15.7 µs, ≈0.38 % |

**Fit verdict: fits, with margin.** No hard envelope constraint (§3) is threatened by the default-set arithmetic; no gross underpricing found in the runtime-resource numbers.

**F1 — one real underpriced item, undisclosed:** the CMakeLists wiring. The cited precedent (`pfg_spv()`, `CMakeLists.txt:30-41`, verified) declares `DEPENDS "${_src}"` only — the top-level `.comp` file, not its transitive `#include`s. LAYERTAB's own generated files (`layer_includes.glsl`, `chain_*.glsl`) are exactly such transitive includes of `fg_core.comp`. Nothing in §1.2/§1.3 adds these generated files to the `.spv` custom command's `DEPENDS`, so editing an *existing* layer's `.glsl` body (not adding a row) is a plausible silent-stale-build hazard — undisclosed, not in the trust-tier table, not named as the weakest point. MAJOR (build-correctness gap in the exact mechanism M2a showcases), not fatal to runtime feasibility.

## B. Scorecard

| Metric | Score | Justification (number, not adjective) |
|---|---|---|
| **M1** | 5/5 | Core math independently verified byte-for-byte against `wap_warp.comp` (lines 315-330, 495-530, 635-640, 677-696 all checked, all match the candidate's citations exactly). One disclosed 1-ulp risk (`mv_guided` packing, §4.1) with a stated two-step verify protocol, not fatal. |
| **M2a** | 4/5 | 2-file claim (`layer_table.def` + `shaders/layers/<name>.glsl`) is architecturally consistent with the described codegen — verified there's no third CMake-side file needed *if* the F1 dependency gap is ignored. Downgraded for that gap, and for the disclosed 4-file exception (new device image). |
| **M2b** | 2/5 | Target is "0, per SG-dump / timestamp query." For every one of the 8 demonstrated default rows (all `kind=F`), the SG-dump census is structurally inapplicable (no separate pass exists to census) and no timestamp measurement was taken. The candidate itself declines to score this passed (§7.3) — I agree and score it low rather than let the vacuous truth stand in for the metric. |
| **M2c** | 5/5 | 6 (objective's own enumeration) / 9 (raw args) / 5 (descriptors) — meets ≤8 on two of three honest readings; the 9th-arg overage is `agreement_threshold`, itself a documented gap in the objective's own count, not the candidate's error. `arm_mask` verified NOT read by `fg_core()` — doesn't inflate the core-function count. |
| **M3** | 3/5 | Structural argument is credible (58→5 stores, 232 B→20 B push, ~140 lines of gate derivation → one call, all `estimated`) but **zero measurement** was run against the M3 instrument (`--csv` + `ball_zoo.ps1`). One newly introduced regression axis (pipeline recreation on toggle, "O(1-10 ms), `unmeasured`") is disclosed but not bounded. |
| **M4 (veto)** | 4/5 | All 8 default layers mapped with source-line citations; I independently spot-verified `mv_guided`, `bg_reclaim`, `phase_anchor`, `vblend`, `single_track`, `stasis` against the shader and found the citations accurate. Correctly deferred to measured acceptance at M-C2, not claimed passed prematurely. The one open representation-change risk (§4.1) is real and unresolved, not merely decorative. |

## C. Attack on the central claim

The central claim ("a printable rank + declared channel + override bit kills the drift *and* the override chain") survives on the four-site-drift half (M2a) — verified: today's four sites are real (787-line `cli.cpp`, 1011-line `cli.hpp`, 173-entry `main.js`, all counts confirmed first-hand) and LAYERTAB's 2-file answer is structurally sound modulo F1.

**The sharper attack:** none of the 8 demonstrated default rows is `kind=P`. The SG-culling story — the part that would make LAYERTAB's "modularity" claim broader than "a nicer table for CLI/UI generation" — is **completely unexercised** by the only concrete workload the candidate maps. Stripped to what's actually demonstrated and measured-or-measurable, LAYERTAB's verified win is the table-driven CLI/parser/help/UI generator (M-C1) — which the candidate's *own* milestone plan ships with `wap_warp.comp` **untouched**. Everything M-C2 adds on top (fused-kernel codegen, GLSL `#include` chains, SPIR-V specialization constants, `arm_mask`) currently has **no measured benefit** over stopping at M-C1: M3's saving is self-described as "at most single-digit microseconds, no profile exists," and M2b — the metric that would justify the specialization-constant machinery — is self-admitted vacuous for exactly this row set. So the *expensive* half of the design (shader codegen) is currently paying for itself only against the override-chain defect (KC2), not against any measured number, while carrying the undisclosed F1 build-dependency risk and the disclosed-but-unresolved column-convergence risk (below). This is real, but it is not fatal — MAJOR, because the shader-side restructuring is still *necessary* to clear KC2 for the shipped `wap_warp.comp` itself (M-C1 alone leaves the override chain live), so "just stop at M-C1" is not a free win — only the specific fused/codegen *mechanism* for M-C2 is unjustified by measurement, not the need for some M-C2.

**Second attack, escalating the candidate's own §7.2:** 8/53 layers mapped (15 %), already forcing 4 schema extensions (ANY-semantics on `requires`, the `shadows` column, the `CH_STASIS` channel, pseudo-rows). The candidate names the correct falsifying experiment (map 10 more, count new columns) and does not run it. Central-claim convergence is genuinely `unmeasured`, by the candidate's own honest label — I escalate this from "self-disclosed weak point" to a load-bearing open question, since without it the "table replaces the four-site drift *and doesn't just relocate the shader's own accretion pattern into a schema*" claim is unverified past the first 8 of 53 rows.

## D. Kill criteria (recomputed against §4, with margin)

| # | Verdict | Margin note |
|---|---|---|
| 1 Unmeasurable | Clear | Same MV path/sampling form verified line-for-line; record strictly more recoverable (typed + hashed). |
| 2 Order-dependent semantics | Clear, softer than claimed | Structural fix (no shared mutable `result`; `c_in` threaded as an argument) verified sound. But the "override honesty" enforcement is a grep-level check for a syntactic reference to `c_in` (§3.5.4) — trivially satisfiable by a body that references `c_in` without functionally using it (e.g. `c_in*0.0 + x`). The **structural** guarantee (no shared mutable state) holds regardless; the **honesty** guarantee is weaker than the confident prose ("is not expressible") implies. MINOR softness, not a violation. |
| 3 Seam regression | Clear | `seam_graph.hpp` verified to emit precise stage/access masks (lines 355-392 read first-hand), never `ALL_COMMANDS`; default set is 1 fused dispatch. |
| 4 Cannot express default | Clear | All 8 rows mapped with source lines I independently spot-checked as accurate. |
| 5 Injection/multi-GPU | Clear | Nothing in the table touches capture identity or device count. |
| 6 New runtime dependency | Clear | Codegen is CMake script-mode (`cmake -P`), precedent verified present in the tree already; shipped binary gains nothing. (Build-time risk is F1, not a KC6 violation — KC6 is about the shipped binary.) |

## E. Constraint smuggling (§6)

All 5 forbidden/dropped drivers checked — **none reintroduced under a new name**:
- Perceptual metrics: not used as a selection criterion anywhere.
- `--minimal` strip-flag: absent.
- Injection/present-hook: absent, capture identity untouched.
- Push-constant override chain: the MVCOND rank-chain (`mv = pfg_mvcond_x(mv, ctx)`) is a genuinely different pattern (declared, printed, composing rather than discarding) from the specific defect named in A0 §0 (a shared mutable color `result` silently fully discarded by the last writer) — the COMPOSE stage, where that defect actually lived, gets the explicit `c_in`-threading + override-bit treatment. Not smuggling.
- "Fewer layers": explicitly disclaimed (§7.1, "deliberately non-subtractive"); no layer count reduction claimed or hidden.

## F. Novelty/optimality honesty

Clear. No unscoped "optimal" or "novel" claim found anywhere in the document. Notably disciplined — explicit declines to claim wins on M2b, M3, and column-set convergence, and every number carries a trust tier.

## Defect list

| Class | Severity | Defect |
|---|---|---|
| F1 | MAJOR | CMake incremental-dependency gap: `pfg_spv()`'s precedent `DEPENDS` covers only the top-level `.comp`, not the generated transitive `#include` chain LAYERTAB introduces — a plausible silent stale-shader-build hazard, undisclosed. |
| F2 | MAJOR | The shader-codegen half of M-C2 (fusion, GLSL codegen, spec constants, `arm_mask`) has no measured benefit over stopping at the candidate's own M-C1 (plain table, `wap_warp.comp` untouched) — M3's saving is self-described sub-microsecond/unmeasured, M2b is self-admitted vacuous for this row set. (Necessary for KC2, but the specific mechanism chosen is unjustified by any number.) |
| F2 | MAJOR | Column-set convergence (the central claim's generalizability) is `unmeasured`: 8/53 layers mapped, already 4 new schema columns forced; the candidate names but does not run its own falsifying experiment. |
| F3/F4 | MINOR | KC2's "override honesty" enforcement is a gameable grep-level check on textual `c_in` reference, weaker than the "not expressible" language suggests — the structural fix (no shared mutable state) still holds independently. |
| F1 | MINOR | Pipeline-recreation cost on layer toggle is `unmeasured` (O(1-10 ms) claimed typical, not measured on this rig); disclosed by the candidate, carried forward as the reason M3 isn't fully cleared. |

## Salvageables (ideas already in the candidate)

1. The FNV-1a 64-bit **contract hash** stamped into `--csv`/`--qdump` telemetry — cheap, closes the "which build produced this CSV" A/B-fragility hole, portable to any angle.
2. **`--layer-dump`**'s printed execution order (rank/stage/kind/arm/params/OVERRIDE) — turns today's comment-archaeology into an artifact, independent of the table-codegen bet.
3. The **`arm` / `arm_mask`** insight: 4 of 8 default layers need per-generation (not just static) disarming, which a pure "compile it out" scheme cannot express — a genuinely portable finding for any candidate attempting compile-time layer gating.
4. **M-C1 as a staged, zero-shader-risk milestone** with `layer_config_parity()` as a startup self-check against the existing `Config` — a sound de-risking sequencing pattern, reusable regardless of which angle wins.
5. The explicit **pseudo-row** device (`:snapshot mv_raw_fwd`, `:sample sad`) for pinning today's comment-only ordering facts as printable rows — valuable documentation discipline independent of the codegen mechanism.
6. The **`shadows`** column naming exactly where `single_track` reaches back into core state — a useful, honestly-named defect-tracking device for any angle that inherits this coupling.
