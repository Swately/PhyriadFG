# AT3 · AT4 · ATF — the closing gates of the layer-contract search (verbatim verdicts)

> AAP passes A3-gate / A4 / A5 (`F:\Phyriad\protocols\analysis\ARCHITECTURE_ANALYSIS_PROTOCOL.md`).
> Three sequential clean contexts (sonnet, foreign judges, MODE_LOCK adversary prompt), run
> 2026-09-03 as workflow `wf_e6187286-66c` (3 agents, 38 tool uses, 550.6 s, 365,859 subagent tokens).
> Inputs: `A3_CHOSEN_DESIGN.md`, `A3_SELECTION_RATIONALE.md`, `AT2_SCORECARDS.md`, `CANDIDATE_{A,B,C}.md`,
> `A0_FROZEN_OBJECTIVE.md` (+ `.sha256`, recomputed by ATF), the repo and `apps/minimal_fg` read-only.
> AT3 and AT4 ran blind to each other; ATF received both verdicts inline. The supervisor spot-verified
> the judges' line citations first-hand before persisting (`wap_warp.comp:1070/1334`, `present.cpp:1215–1218`,
> `CANDIDATE_A.md:219–236`, `CANDIDATE_B.md:597–600`, `AT2_SCORECARDS.md:623`) — all confirmed.
>
> **Outcome:** AT3 `CONVERGED` · AT4 `APPROVED FOR HANDOFF` · ATF `APPROVED FOR HANDOFF` → the design is
> released to KAP (documentation) + PLAN_TIER (the `CONVERGENCE_*` triad). One item the AT3 judge flagged
> as not carried into A3 (the CMake `DEPENDS` stale-build gap, `AT2_SCORECARDS.md:623`) is carried into
> `../CONVERGENCE_RISK_REGISTER.md` as **XR13**. The AT2 KC2 textual question (whether A0 §4.2's bare
> "last-writer-wins" admits a declared order) was resolved by the AT4 judge in the design's favour on
> the structural argument (no shared mutable `result`; a declared, static_asserted, printed order); the
> supervisor records that A0's text was NOT amended (AINV-12 holds — see ATF's sha256 recompute).

## AT3 — CONVERGENCE (clean context, sonnet)

**CONVERGED**

Verified first-hand against the objective, scorecards, and all three candidate documents.

**Check 1 (rests on scorecards, not taste):** Every claim in `A3_SELECTION_RATIONALE.md` §1–2 is a specific quote or number pulled from `AT2_SCORECARDS.md` (e.g. B's M2a "Fails as specified," C's M2b "2/2/2," A's M3 "2/2/3"). No unsupported assertion of taste found.

**Check 2 (frozen priority, not reordered):** Applied exactly as A0 §2 states: M1 first, M2 second, M3 third, M4 veto-only. The rationale does not invert or relabel this order. Within M2's three lettered sub-items (a/b/c, which A0 does not itself sub-prioritize), the rationale's discount of C's poor M2b (2/2/2, the lowest score anywhere in the table) rests on a real, cross-checked observation — the rationale explicitly notices "C's fused rows use the SAME spec-constant mechanism A is graded 5/5 for," i.e., it catches and names an inconsistency in how the lenses scored a common mechanism, rather than inventing a discount. This is a defensible scoring-consistency argument grounded in the record, not a smuggled reordering.

**Check 3 (does the emergent winner match the crowned one):** Recomputed per-candidate means from the raw AT2 table independently. On M1 alone C actually leads outright (avg 4.67 vs A 4.33 vs B 4.0) — the rationale's "ties A" framing is if anything conservative, not inflated toward C. B is correctly eliminated by its own M2a self-contradiction (unanimous "fails as specified," confirmed in all three B lenses' first-hand file-count recompute). Between A and C, M3 clearly favors C (avg 4.0 vs 2.33, and A's own document names its M3 risk as unmeasured/unbounded) while M2 is genuinely mixed — a plausible tie-break under M1>M2>M3. The winner is not fabricated; it is a defensible reading of the actual numbers, not one the scorecards contradict.

**Check 4 (grafts traceable):** Directly verified in the candidate source files, not just the graft table's citations:
- G1/G2 (single-writer output; convex `RESULT_MIX` fold, `weight==0 ⇒ identity`) — present verbatim in `CANDIDATE_A.md` §3.4 (lines 219–236).
- G3/G4 (dominance-warning WAW branch + `.dominates_ok`; `optional_write`/liveness spec constants) — present verbatim in `CANDIDATE_B.md` (lines 103, 330–331, 359, 729–730).
- G5 (zero-allocation `execute()`, ~1,920 allocs/s) — present verbatim in `CANDIDATE_B.md` Appendix A.3 (lines 597–600, 736–738).
No new idea introduced at convergence that isn't already in a candidate.

**Check 5 (weakest point named, not buried):** `A3_CHOSEN_DESIGN.md` §5 and `A3_SELECTION_RATIONALE.md` §4 both give the column-closure/`shadows`-backdoor risk its own section, matching C's own §7.2 and all three C-lens scorecards' F2 findings on `single_track`. This is the design's most structurally consequential weakness (it strikes at the central "structurally kills the order defect" claim) and it is surfaced prominently, with a named falsifier experiment gating any further shader work — not buried under a lesser issue.

**Check 6 (optimality scoped):** Both documents state "best of the THREE angles searched... no absolute-optimality claim" / "not the best possible design," correctly scoped to the candidate set and objective (AINV-11 satisfied).

Minor observations that don't rise to RE-SELECT: the rationale doesn't discuss the raw "MAJOR defects (counted)" row, where C has the highest count (3/3/5) of the three candidates — but this is an AT2-lens bookkeeping artifact, not an A0 metric, and plausibly reflects C's greater self-disclosed detail (also the highest-scored candidate on novelty/optimality honesty) rather than a hidden flaw; the one CMake incremental-dependency gap flagged by C/adoption-skeptic (undisclosed, MAJOR) is not mentioned in either convergence document, but it is a narrower build-correctness risk than the named schema-closure weakest point, not a competing central risk that was suppressed in its favor.

## AT4 — FEASIBILITY VETO (clean context, sonnet)

**APPROVED FOR HANDOFF**

Reasons (independently re-verified against the repo, not taken from the candidate's self-report):

**Check 1 — kill criteria (A0 §4), recomputed:**
- KC1 (unmeasurable): the core body in `fg_core_math.glsl` is byte-for-byte the math at `wap_warp.comp:496–498, 520–522, 637, 679` — verified by direct read. Same MV path/sampling form → analytically clear per A0's own gating note (instrument not yet built). Real residual risk (§4.1's `1.0+sim` packing, 1‑ulp) is named and given a two-step measurement protocol, not swept aside.
- KC2 (last-writer-wins): confirmed defect in `wap_warp.comp` — `result` declared line 1070, sequentially overwritten through `stasis`(1191)→`single_track`(1321-1329)→`imageStore`(1334). LAYERTAB's `vec4 f(c_in, ctx)` COMPOSE signature with a build-`static_assert`ed unique `rank` and a printed `overrides` bit structurally removes shared mutable state. Clear.
- KC3 (seam regression): `seam_graph.hpp` header (line 38) itself forbids `ALL_COMMANDS`; default set stays 1 dispatch, 0 added barriers, matching the present 1-dispatch+3-barrier baseline at `present.cpp:1215–1218` (verified exact line match). Clear.
- KC4 (shipping default): all eight rows (mv_guided, inertia, bg_reclaim, phase_anchor, ambig, vblend, single_track, stasis) map to verified source lines (358–444, 507–514, 1191, 1321–1329). Clear, with an honestly-flagged wart (`shadows` reaching back into core for single_track) that doesn't reintroduce order-dependence.
- KC5/KC6: no capture/device-count coupling; codegen is a CMake `cmake -P` build step with a real precedent already in the tree (`CMakeLists.txt:30–41` `pfg_spv()`, verified). No new shipped-binary dependency. Clear.

**Check 2 — real-unit closure at 1920×1080/240Hz, recomputed independently:** default set = 1 dispatch, 0 intermediate images, 0 added barriers (verified against `present.cpp`). Even the non-default single-channel split (`CH_MV`, 7.91 MiB, RTX 4090 @ 1008 GB/s) costs ≈16.5 µs/tick by my own recompute (candidate says 15.7 µs — a ~5% deviation, immaterial) against a 4.167 ms budget — under 0.4% either way, and the default configuration pays none of it. UBO arithmetic (40B→48B std140) checks out. Slack is large in every case that matters to the default path.

**Check 3 — staged route to a real number:** M-C1 (registry + `--layer-dump` + `layer_config_parity()` self-check, shader untouched) is measurable today on the rig via the existing `ball_zoo.ps1` against the already-measured E1 baseline (240 presents/s) — zero product risk, no dependency on any unbuilt instrument. M-C2 (the real shader cut) is correctly gated on MOTION_TRUTH's own T6/S6 phase rather than inventing a parallel oracle. Route exists.

**Check 4 — unmeasured numbers that could break the veto, and their resolution:** the column-set-convergence claim (§7.2, the design's own stated weakest point) is unmeasured and could falsify the central M2 bet — but A3 §4 stages exactly the required falsifier (map 10 more layers, ≤1 new column proceeds, ≥4 sends it back to A3) *before* M-C2 commits the shader risk. The `sim` packing 1-ulp question is likewise resolved inside M-C2's own acceptance step before the representation change ships. Verified `seam_graph.hpp:455–476` also confirms the G5 graft targets a real defect (`execute()` heap-allocates a `std::vector<VkImageMemoryBarrier2>` per pass per call) rather than a fabricated one — the grafts are grounded in code, not invention.

No kill criterion fails, the envelope closes with margin in real units, and a staged, zero-risk first milestone exists on the rig today with the one real unresolved number (schema closure) explicitly gated before scale commitment.

## ATF — SEARCH CLOSE / HANDOFF (clean context, sonnet)

**APPROVED FOR HANDOFF**

Recomputed first-hand against `A0_FROZEN_OBJECTIVE.md`, `A0_FROZEN_OBJECTIVE.sha256`, `A3_CHOSEN_DESIGN.md`, `AT2_SCORECARDS.md`, and all three `CANDIDATE_{A,B,C}.md` files, plus the two verdicts quoted inline. No file modified.

**AINV-1/12 (frozen, byte-intact):** Recomputed sha256 of `A0_FROZEN_OBJECTIVE.md` myself: `670687d03261a01faf7e287b41d098a1bbcd51d821fa43e5814c6e560d66e933` — matches the recorded `.sha256` file exactly and matches the abbreviated hash cited in `A3_CHOSEN_DESIGN.md` (`670687d0…6933`). The A0 header states the block was written "2026-09-02 BEFORE any candidate exists," and all three candidates cite A0 as their basis. No drift.

**AINV-4/6 (≥2 independent, non-producer-scored):** Three genuinely distinct candidates on disk — A (fused-variant, 576 lines), B (staged SG passes, 762 lines), C (data-driven layer table, 745 lines) — each stating explicit isolation ("No other candidate was consulted" / "in isolation from candidates A and B"). `AT2_SCORECARDS.md` scores each with 3 independent "clean context" lenses (budget-auditor, performance-realist, adoption-skeptic), distinct from the designer role. 9 scorecards total, each opening with its own first-hand feasibility recompute against repo source rather than trusting the candidate's numbers.

**AINV-7 (selection earned by scorecards, frozen priority):** Independently recomputed the per-candidate M1 and M3 means from the raw AT2 table to check AT3's arithmetic: M1 — A (4+4+5)/3=4.33, B (3+5+4)/3=4.0, C (5+4+5)/3=4.67 → matches AT3's "C leads outright, avg 4.67 vs 4.33 vs 4.0" exactly. M3 — A (2+2+3)/3=2.33, C (4+5+3)/3=4.0 → matches AT3's "M3 favors C, avg 4.0 vs 2.33" exactly. B's M2a is 2/5 "Fails as specified" in all three lenses (verified: each lens independently found 4 files against B's own claimed 2, a self-contradiction in B's own §1.3), confirming AT3's elimination reasoning. Priority order M1>M2>M3, M4-veto is applied as A0 states, not reordered.

**AINV-8/9 (veto cleared, route to measurement):** AT4 verdict text states all six kill criteria "Clear" (KC1 unmeasurable, KC2 order-dependence, KC3 seam regression, KC4 default-set, KC5 injection/multi-GPU, KC6 new dependency), gives a real-unit closure recompute (1 dispatch / 0 barriers / 0 intermediate images for the default set; ≈16.5 µs worst-case single split against a 4.167 ms budget), and stages a zero-risk first milestone (M-C1) measurable today, with the M-C2 shader-risk gated behind a named falsifier (column-closure experiment). Ends "APPROVED FOR HANDOFF."

**AINV-10 (trust-tiered, no fabrication):** Verified directly in the scorecards — grafts G1/G2 (single-writer output, convex `RESULT_MIX` fold with `weight==0⇒identity`) are present verbatim at `CANDIDATE_A.md` lines 219–236 as AT3 cites; G3/G4/G5 (dominance-warning WAW branch, `optional_write` liveness spec constants, zero-allocation `execute()`) are present verbatim at the exact `CANDIDATE_B.md` lines AT3 cites (330–331, 359, 597–600, 729–738). No graft introduces an idea absent from its source candidate. The scorecards themselves show the adversarial discipline actually catching errors (barrier undercounts, L2-credit slips, unit-conflation on MiB vs GB) rather than rubber-stamping — evidence the trust-tiering is functioning, not decorative.

**AINV-11 (no absolute-optimality claim):** `A3_CHOSEN_DESIGN.md` header explicitly states "no absolute-optimality claim is made (AINV-11)... best of the THREE angles searched." AT3 Check 6 confirms this scoping is honored, not smuggled.

**Honest ceiling, not buried:** `A3_CHOSEN_DESIGN.md` §5 gives the column-closure/`shadows`-backdoor risk (the design's most consequential weakness — it undercuts the "structurally kills the order defect" claim for exactly the layer, `single_track`, that embodies today's defect) its own prominent section, matching all three C-lens scorecards' independently-found F2. It is gated by a named falsifier (§4.2) before any further shader investment, not deferred silently.

No invariant broken, no smuggled reordering, feasibility independently recomputed and consistent, kill criteria crossed with margin, weakest point surfaced rather than buried.

*Made with my soul - Swately <3*
