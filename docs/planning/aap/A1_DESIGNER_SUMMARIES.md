# A1 — designer return summaries (candidates: CANDIDATE_A/B/C.md; opus, isolated)

## designer A

CANDIDATE_A written to `F:\Phyriad\projects\PhyriadFG\docs\planning\aap\CANDIDATE_A.md`.

```
1. NAME: FUSED-VARIANT — one compute pass, compiled layer variants (Angle A).
2. BET: a layer is a compile-time participant (a specialization constant the driver DCEs) writing only into a typed, ranked slot of the core — never into the store — so a disabled layer costs zero instructions, zero bytes, zero barriers, and adding one costs zero new images.
3. CORE PARAMETER COUNT: 5 (residual_ceil, improvement_frac, agreement_threshold, mv_source in a CoreParams UBO; t alone in a 4-byte push) vs 57 today. M2c target is <=8.
4. PASSES PER TICK: 1 compute dispatch (fg_core) + the pre-existing blit + the per-pair upload pass SG culls on non-advance ticks — unchanged from today. Intermediate images introduced: ZERO bytes.
5. ORDER (kill criterion 2): hard guarantee = single-writer output (no FINAL slot exists; the store has one source expression) + rank-max SELECTION slots (MV_FETCH, BASE) that are genuinely commutative; the fold slots use an EXPLICIT declared integer rank (duplicate rank = build error, printed by --layer-dump), with RESULT_MIX constrained convex so no contributor can discard upstream except at weight==1.
6. DEFAULT-SET MAPPING: all 9 mapped as verbatim ports with today's source order as their ranks; M4 byte-identity argued expression-by-expression (single_track claims BASE at rank 100 -> base=B_samp; stasis_present(10) then screen_static(20) fold to exactly today's mix(B_samp,cur0,w_s)). Verified first-hand that bg_snap/band_xfade cascade OFF on this rig (igpu_field=false), so the brief's list of 8 is the correct rig default set.
7. BIGGEST ESTIMATED COST: none in bandwidth — and that is the finding that cuts against my own angle. 42-100 MB/tick at 240 ticks/s = 10-24 GB/s = 1-2.4% of the 4090's spec bandwidth, so "fuse for bandwidth" is a weak justification here; the real wins are zero new allocation, M2b=0 by construction, zero added submits/fences, and -2 buffer barriers + a 4-byte DMA per tick that today's build records unconditionally with matte off.
8. WEAKEST POINT: a fused pass has NO cost isolation — every layer shares one register budget and one occupancy class, so adding a layer is a non-local cost landing on every pixel including ones it never touches; registers/thread and achieved occupancy per variant are unmeasured, and a register-cliff regression would force a hybrid split that partially concedes to Angle B.
9. SECOND WEAKNESS: variant explosion / cold-compile hitch on a mid-session toggle (mitigated by a persisted VkPipelineCache + background compile + tick-boundary swap, and by pre-warming the {default, warp_light} pair since --load-governor's per-tick shed becomes a variant swap).
10. FIRST MILESTONE A-M1: --fg-core behind a flag (old path stays default), then ball_zoo 60fps 1920x1080 -> --qdump 240 frames both sides byte-diffed (M4 veto), git diff --stat on an add-a-no-op-layer exercise (M2a=2), --sg-dump with --no-ambig --no-vblend (M2b=0), --csv/--wsub 60s x2 per side vs the G1 240 presents/s baseline (M3), plus Nsight occupancy per variant (the weakest-point probe). Uses only instruments that exist today; does not depend on the unbuilt MOTION_TRUTH instrument.
```

## designer B

CANDIDATE_B written to `F:\Phyriad\projects\PhyriadFG\docs\planning\aap\CANDIDATE_B.md`.

```
1. NAME: Candidate B — "Stages as SG passes": a fixed 4-stage skeleton (COND -> SAMPLE -> COMPOSE -> POST) whose boundaries are real SG images, with layers as slotted graph nodes.
2. BET: order becomes a property of the stage/slot topology with a per-slot composition ALGEBRA (one-of / OR / weighted-mix / max), never an assignment to a shared `result`, so a disabled or unreachable layer is culled by SeamGraph::compile() with no human deciding.
3. CORE PARAMETER COUNT: 1 (`float t`, a 4-byte push). Objective M2c target is <=8; the 3 gate thresholds move into SAMPLE/GATE layer nodes that the shipping default culls.
4. PASSES PER TICK (default set): 8 compute passes — mv_fetch_guided, inertia_restrict, bg_reclaim, phase_anchor, ambig_arbitrate, vblend_predict, core_sample, compose — in 1 command buffer, 1 submit, 1 fence, with 17 SG-derived barriers (+15 vs today), no per-stage fence and no host round-trip.
5. BIGGEST ESTIMATED COST: 300-350 MiB/tick of DRAM traffic (~13-15x today's ~24 MiB fused warp) = +0.42-0.56 ms/tick estimated at 1920x1080, i.e. +11% to +19% on the only measured warp figure this project has (gpu ~3-4 ms, BF6 combat, 4090 at 98.8%).
6. WEAKEST POINT: the sample-packet crossing — samp_B (RGBA32F) + d_pixel (R32F) = 79.1 MiB/tick moved through DRAM to carry values the fused shader keeps in registers, buying zero arithmetic; fp32 is the only format that makes M4 byte-identity PROVABLE (fp16 halves the cost but lands at an estimated PSNR ~60.3 dB, on M4's threshold), and the mitigation ladder for a bad M3 verdict is stage fusion — which is Angle A's move, so B's modularity is bought with bandwidth at an exchange rate that is not yet measured.
7. FIRST MILESTONE (B1, on the rig): split wap_warp.comp into just core_sample + compose_st with the rest in a culled `legacy_tail` pass; gates = byte-identical --qdump+/T6 replay (M4), ball_zoo 60s x2 runs --csv cadence + --wsub gpu/rec vs the E1 240-presents/s baseline (M3), --sg-dump showing 0 dispatches/barriers for legacy_tail (M2b), 122/122 golden SG test still green.
8. KEY FINDING (read first-hand, feeds the design): under the SHIPPING DEFAULT, bg-reclaim is dead — wap_warp.comp:410 rebases `mv` on `mv_fwd` (captured at :344, before bg-reclaim's damp at :394), discarding it entirely. Kill criterion 2, live, in the default config.
9. KC-2 IS MECHANIZED: the branch in SeamGraph::compile() that emits a WAW barrier is exactly "a pass overwrites a resource it does not read"; ~10 lines there emit a `dominates:` warning naming both parties — the design's own conformance check finds the bug above without anyone looking for it. Requires 4 small SG extensions (Appendix A), none touching dump()'s golden bytes.
10. DROPS: the 57-field push block, magnitude-encoded gates (mv_guided=1.0+sim, single_track in {0,1,2}), `result` as a shared mutable, the "armed but shadowed" single-track upstream gates, the hand-written barriers + in-place RMW in wap_warp_present, blend-solo as a shader branch. Flags/UI: ParamDesc in each layer's desc.hpp becomes the single source; parser/help/--dump-flags/UI all derive from it (layer surface only, ~40 of 257 flags — stated, not inflated).
```

## designer C

CANDIDATE_C written to `F:\Phyriad\projects\PhyriadFG\docs\planning\aap\CANDIDATE_C.md`.

1. **Name:** LAYERTAB — "the layer table is the contract" (Angle C).
2. **The bet:** one X-macro registry row per layer (`src/layers/layer_table.def`) is the sole declaration site, and Config fields / parser / help / UI-JSON model / SG registration / GLSL spec-constants / UBO layout / chain order all derive from it — so the four-site drift becomes structurally impossible, and the shader's fusion granularity becomes a per-row `kind` column rather than a global architecture bet.
3. **Core parameter count:** 6 contract parameters (objective's M2c enumeration), 9 function arguments, 5 descriptor bindings, 20 B push block (`residual_ceil, improvement_frac, agreement_threshold, t, arm_mask`) — down from 57 fields / 232 B.
4. **Passes per tick (default set):** 1 compute dispatch, 0 intermediate images, 0 added barriers; all eight default layers are `kind = F` (fused, spec-constant-gated).
5. **Biggest estimated cost:** if any MVCOND row is flipped to `kind = P`, a full-res RG16F `mv_eff` image = 8,294,400 B (7.91 MiB) and ~15.83 MiB traffic/tick ≈ 15.7 µs ≈ 0.38 % of wall time at 240 ticks/s (estimated from the 1,008 GB/s spec figure). Default set pays 0.
6. **Order answer (KC2):** no order-independence claimed — explicit declared order: fixed stage pipeline MVCOND→SAMPLE→core→COMPOSE, one `rank` integer per row, COMPOSE layers are `vec4 f(vec4 c_in, ctx)` so no shared mutable `result` exists, `overrides` is a declared+printed bit, and a FNV-1a contract hash stamps every CSV/qdump run.
7. **Default-set mapping:** all eight mapped verbatim with source lines (mv-guided 179–202/319–327, inertia 304–306+332–334, bg-reclaim 358–396, phase-anchor 397–411, ambig 426–444, vblend 507–514, single-track 1321–1329, stasis 1191+1327 split into a channel publisher + a COMPOSE row).
8. **First milestone:** M-C1 ships the registry with the shader untouched and a field-by-field `layer_config_parity()` assert against today's `Config` — M2a measurable, M4 veto cannot fire; M-C2 then swaps in `fg_core.comp` under the M4 replay gate.
9. **Weakest point (mine):** the column set is not proven closed — mapping only 8 layers already forced 4 schema extensions (`requires`-ANY, `shadows`, a `CH_STASIS` channel, pseudo-rows); 53 layers exist. The settling experiment is named: map ten more and count new columns. Secondary: `arm_mask` is a runtime gate I had to reintroduce because 4 default layers disarm per generation, so "compiled out" is only true for statically-disabled layers; and toggling moves from a free push value to a pipeline rebuild.
10. **Flagged M4 risk I did not hide:** unpacking `mv_guided = 1.0 + sim` changes `sim_thresh` by up to 1 ulp (`1.0f+0.10f-1.0f != 0.10f`), so the M4 run must first reproduce the packed value host-side before flipping to the clean field.
