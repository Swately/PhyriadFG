# AT1 — diversity gate (clean context, sonnet)

**DIVERSE**

**Check 1 (≥2 candidates):** 3 present (A, B, C). Satisfied.

**Check 2 (genuine independence, not costumes):** The three take opposite, verifiable positions on the one axis A0 §0 says this search must decide — pass/contract granularity:

- **A (FUSED-VARIANT):** always one dispatch, zero intermediate images, layers as spec-const-gated GLSL functions inlined into one kernel. Composition is a fixed set of **9 named typed slots**, two of which (`MV_FETCH`, `BASE`) are claimed **order-independent by construction** via rank-max selection (commutative).
- **B (Stages as SG passes):** always separate SG passes — 8 real dispatches for the default set, fp32 stage-crossing images, ~349 MiB/tick traffic. Composition is a **10-slot algebra** (OR / weighted-mix-with-clamped-sum / target-keyed max / ordered chain) implemented as real barriers between real passes, plus a **novel dominance-warning mechanism** mechanized inside `SeamGraph::compile()` that catches last-writer-wins bugs at build time.
- **C (LAYERTAB):** granularity is a **per-row data column** (`kind: F|P`), defaulting to full fusion (0 images, like A) but generalizing to B's per-pass model on a per-layer basis. It explicitly **disclaims commutativity anywhere** ("any candidate that claims MV-conditioning layers commute is wrong") — a direct structural disagreement with A's central order-independence claim, reached in isolation. It also introduces `arm_mask` for per-generation dynamic disarm (gme/bwd/matte/appear), a mechanism neither A nor B addresses at all.

Contract shapes, composition philosophy (2 provably-commutative slots vs. no-commutativity-claimed vs. real-pass isolation), core function signatures (UBO-bound vs. pure-function-argument), and declaration tooling (per-file `.layer` + `LAYERS.txt` vs. single X-macro `.def` vs. per-stage/per-pass registry) all differ substantively. No pair is "same passes, same contract, relabeled."

**Check 3 (budget closure, recomputed first-hand at 1920×1080):**
- Grid: 2,073,600 px confirmed. Dispatch (1920+7)/8 × (1080+7)/8 = 240×135 = 32,400 workgroups × 64 threads = 2,073,600 — exact fit, no padding (both dims divide 8 evenly). Matches all three candidates.
- Format bytes independently verified: RGBA8/R32F = 8,294,400 B; RG32F = 16,588,800 B; RGBA32F = 33,177,600 B; MV grid (240×135) RG16F = 129,600 B — consistent across all three documents.
- **A:** 0 B new intermediate images (architecturally sound — no `imageStore` beyond final output); ~6 image barriers in 2 batched `vkCmdPipelineBarrier2` calls (estimated, plausible for 4 input-acquire + 2 output/blit). New UBO allocation ~1.1 KiB. Closes.
- **B:** resummed 15.82+15.82+15.82+31.64+7.91 = **87.01 MiB** resident (matches claim). Resummed all 8 per-pass traffic lines (39.67+31.79+47.46+31.76+32.01+31.76+71.19+63.40) = **349.04 MiB/tick** (matches claim). Resummed barrier count per pass (2+2+2+3+3+2+1+2) = **17** (matches claim). Arithmetic is internally consistent and correctly totaled. This is the only candidate whose bandwidth (10–13% of a 240 Hz tick, estimated) is a real open M3 risk — but the envelope (A0 §3) sets no hard VRAM/bandwidth cap, 87 MiB is trivial against the 4090's capacity, and B stages its own B1 milestone specifically to measure this before committing further stages. It closes; it does not violate a hard constraint.
- **C:** 0 B for the default (all-fused) path, 0 added barriers by default — same order-of-magnitude as A for the shipping configuration, independently verified as architecturally sound. The optional per-row `kind=P` split is separately quantified (7.91 MiB channel, +1 barrier/split) and that arithmetic checks out too (15.83 MiB traffic ⁄ 1,008 GB/s ≈ 15–16 µs, ≈0.38% at 240 Hz — recomputed value is ~16.5 µs, within rounding tolerance of the claimed 15.7 µs).

All three close under the section-3 hard constraints (rig/toolchain/SG-only/identity/home/reuse) with no fabricated or self-contradictory arithmetic found on recomputation.
