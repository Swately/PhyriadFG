# FG_PROJECT_IMPROVEMENT_MAP — the project-wide improvement audit (within + between)

> **Diátaxis type:** Analysis (improvement audit). **Status:** the findings are tagged
> `[verified]` (first-hand) / `[inferred]` / `[speculative]`; the *map* is a snapshot
> **2026-06-25**. **Provenance:** workflow `w3z3g1i24` — four first-hand area audits
> (A: FG present/pacing · B: FG flow/warp/disocclusion · C: pillars + fork-vs-lift ·
> D: testbench/autonomy/process) + a synthesis, each grounded against the working tree.
> **Why this exists:** the operator asked for a whole-project improvement analysis
> "entre ellas y sí mismas" (BETWEEN components + WITHIN each). It is a sibling of the
> project-wide fork survey (`widpumzly`) — same discipline: **non-opportunities are a
> first-class output** (a carefully-built system is mostly already good; the honest job
> is to find the narrow real gaps, not to manufacture work). No inflation; match
> ceremony to blast-radius.

---

## §1 — The strong base (the deflation, first — so the rest is trusted)

The four audits independently agree: **most of this system is correct and must not be
touched.** The present **pacer** (freeze-floor hard-cap, byte-identical-off, no-game-cap)
is complete + dogma-bound (`main.cpp:7418-7443`); the content-clock **PLL** is a textbook
constant-rate tracker, sufficient by construction (Kalman explicitly forbidden); the
**disocclusion SELECTION** stack (bidir round-trip ownership, divergence pick,
dissidence-asymmetry bg-extension) is as good as a non-neural FG gets and honestly concedes
the true-hole floor (`wap_warp.comp:75`); the **iGPU Sobel edge-gate** is a genuine
differentiator (no shipping FG has an image-edge trust signal); the **multi-GPU A/B/G**
split is contextually optimal and the break-even router is correctly inert (`offload=false`,
AI≈2.5 ≪ ~596 crossover); the **fork decisions are deliberate + verified** (the F→P
generation-ring is genuinely beyond `transport::Latest`; the telemetry SPSC ring is rightly
lighter than the SWMR Disruptor; the hardest fork — the DComp/WDA/VK-D3D11 bridge — was
*already lifted* into the present pillar); the **measurement system** is mature (median-robust
direction-aware regression gates; the testbench's Tier-2 RISK_REGISTER; clean tool-enforced
INVENTORY; honestly-caveated VFI-grounded scorer metrics). **The real gaps are narrow —
mostly *unrun measurement* + *one build bug*, not missing design.**

---

## §2 — The recommended few (highest value/cost) + status

1. **W1 + X1 — the live build break + a GPU-less compile-CI job.** `[DONE this session, monitor-free]`
   Fork #1 added two generated-shader `#include`s to `OpticalFlowPipeline.cpp:83-87`; **12
   standalone benches** (the audit's "≥5" undercounted) never got the matching CMake gen rules
   → they could not compile (invisible because no CI builds any bench). **FIXED**: the 2 gen
   rules + DEPENDS added to each of the 12; a shared root-fix helper
   `framework/render/vulkan/cmake/optical_flow_shaders.cmake` (inert, adopt opportunistically)
   prevents recurrence; verified `stage11` compiles green. **X1 (the GPU-less bench-build smoke
   CI job) is still designed** — it would have caught this the moment the pillar `#include`s
   changed (compiling needs only the Vulkan SDK + Ninja, not a GPU). `[X1: do next, monitor-free]`
2. **W2 + W3 — run the held-out scorer over the 3 eye-gated disocclusion levers** (`disoccl_hardpick`
   STAGE-116 first). `[scorer monitor-free on fixtures; eye-veto stays the final gate]` Three
   shipped code paths sit in eye-limbo solely because they were never measured; `crescent_band_energy`
   scores the exact LSFG-losing reveal-band ("media luna"). Ends the blind eye-iteration (the
   "no avanzamos" feeling) at the frontier where the project measurably loses. Honest caveat:
   the band metric is itself a luma/saturation proxy (FX-6) — it ends blind iteration, it does
   not replace the eye-veto.
3. **W4 → W5 — wire the display-flip clock, THEN run the deterministic pacer A/B (forced order).**
   `[W4 DONE this session — monitor-free/byte-identical-off; the API yield + W5 need the rig]`
   TB-C9 was scoring the pacing axis on `MsBetweenPresents` (present-call time) — the very clock
   `--pace-hard` controls → **any pacer A/B was partly CIRCULAR.** **FIXED (W4)**: the own-window
   flip swapchain's `GetFrameStatistics().SyncQPCTime` is now wired into `MsBetweenDisplayChange`
   (`PresentSurface::last_flip_qpc()` + the `--csv` push under `if(tcsv.active())`); the scorer
   already prefers the column. **W5 (the FL1 attribution — OFF vs `--pace-hard` vs `--pace-vblank`
   on the TB-C1 source) is the next step, and MUST run AFTER W4's API yield is rig-confirmed** —
   else the pacer win is tautological. Caveat: `GetFrameStatistics` yield on the *composed*
   (non-independent-flip) path is a per-config outcome to confirm; scope fluidity claims to the
   own-window independent-flip path and report the composed case as a measured outcome, not a defect.
4. **(stretch) X4 — the `break_even_decide` DRY lift.** `[monitor-free]` A genuinely cheap
   single-source-of-truth win (the header-only `constexpr` is reachable without linking
   `phyriad_gpu`), but **zero behavior/perf value** (the probe is inert by design) → maintenance
   only; do it when touching that code, not as a priority.

**Sequencing spine:** W1/X1 immediate; **W4 gates W5**; W5 (+ the fix it justifies) gates W6
(a fluidity regression gate) + W7 (decouple the realized-N even-grid placement ladder into a
default-off `--fluid-ladder`, `main.cpp:9028-9038` — only if W5 says placement is the residual,
D-12 measure-first). W2/W3 are independent of the fluidity chain.

---

## §3 — The map (condensed; full detail in workflow `w3z3g1i24`)

**WITHIN (top):** W1 build-break `[done]` · W2/W3 scorer-over-disoccl-levers `[high, cheap]` ·
W4 display-clock `[done]` · W5 FL1 attribution `[high, cheap, rig]` · W6 fluidity regression
gate `[high]` · W7 `--fluid-ladder` `[medium, gated on W5]` · W8 enrich iGPU `occ_class` beyond
the binary edge-band stub (`igpu_field.comp:52`) `[medium, speculative]`.

**BETWEEN (top):** X1 GPU-less bench-build CI `[high]` · X2 generalize W2 into the C8
measure-improve loop (the FG full-reference-scores its OWN output — a capability LSFG
structurally lacks) `[high, cheap]` · X3 the present→testbench display-clock wiring + forced
sequencing (= W4 + the ordering) `[high]` · X4 break_even DRY lift `[medium, inert]` · X5 wire
the fluidity analyser into the TB-C4 sweep orchestrator (`fg_testbench_sweep.ps1` only harvests
throughput/pacing today) `[medium, cheap]` · X6 lift the replicated Ollama/WinHTTP LLM client
(`master_holon` ↔ `voyager_agent`, two hand-copied WinHTTP clients diverging) into one shared
util `[medium]`.

---

## §4 — Honest non-opportunities (do NOT re-propose)

- **Present/pacing (A):** the pacer freeze-floor/hard-cap (`main.cpp:7418-7443`) — correct +
  complete; the byte-identical-off discipline — the model others copy; the 2nd-order PLL —
  sufficient, Kalman forbidden (the stepping is intra-pair tick mismatch, not a PLL defect);
  merging `--cphase` into `--phase-norm` — deliberately orthogonal (re-invites the STAGE-94
  fine-text ceiling); widening the spin budget — bounded ~2 ms by the efficiency mandate; the
  fluidity-fix doc's own FL-Padd-as-written — **circular** (logs the commanded grid) + DWM is
  cold ~1/sec (superseded by W4); pulling `--async-present` into the placement fix — correctly
  deferred (separate crash-class).
- **Flow/warp (B):** the per-tile affine post-pass — **measured net-negative** (47 worse / 8
  better, `OpticalFlowPipeline.cpp:196-202`), hardcoded false, correctly parked; the CANON#12
  fork-#1 matcher + descriptor-prebake — inert by design (the default rightly uses the quality
  channels the forks strip); the disocclusion SELECTION machinery — 100% real-content, improvable
  only by **neural fill** (the one true non-neural floor, the single THEY-WIN-vs-LSFG axis;
  HIGH value but HEAVY/research + in direct tension with combat-saturation — ranked below the
  cheap measured wins); the Sobel trust-gate — a strength to leverage (W8), not a gap; the A/B/G
  role split — architecture-forced; the per-frame perf hot path — already root-caused
  (`wap_upload` on the graphics queue; `--upload-xfer` in flight).
- **Pillars (C — the dominant finding was non-opportunities):** the F→P channel "hand-rolls past
  Latest" — correctly forked (generation ring + GPU-image coupling + reader-pin backpressure,
  beyond Latest/Ring); the telemetry SPSC ring vs `transport::Ring` — correctly lighter; the
  pacer living in the app not `PresentSurface::submit_at()` — correct (the pillar's contract
  disowns "when/which"); `break_even_decide` arithmetic — validated (1.139× measured vs 1.128×
  predicted); the holons pillar being unused (fixed-role graph ≠ divisible Cooperation), the F3
  egress classifier, the cap-route-probe — all correctly inert by design. HAL `f32_to_f16_batch`
  (codec symmetry only, no contiguous hot loop) + `transport::Latest` aarch64 verification
  (x86/Windows-only substrate) — ~nil value.
- **Testbench/process (D):** the perf/quality gate scripts + the shared `prep_zoo_sequence.py`
  fixture — well-engineered; the scorer metric definitions — exemplary honesty ("improving" by
  dropping caveats would REGRESS it); a mechanical honesty-status linter — deliberately declined
  (the semantic verify-before-claim reflex is the right instrument); unifying the three
  verification harnesses (FG scorer / eeg s2_equiv / voyager VerifierHolon) — they verify
  fundamentally different things (forcing a common abstraction violates match-ceremony-to-blast-
  radius); the eye-vs-proxy correlation study — the deepest autonomy-H1 gap but HEAVY, consumes
  the scarce operator-eye, and may return a null that is itself the answer.

---

## §5 — Provenance + honest limits

From workflow `w3z3g1i24` (4 audit agents + synthesis, ~645 K tokens), each grounded first-hand
against the tree. `[inferred]` items (W4's display-clock-fixes-the-circularity, W8) and
`[speculative]` items are tagged as such. **W1 + W4 were ACTED ON + verified first-hand this
session** (stage11 compiles green; the FG compiles byte-identical-off with the display-flip
column wired); the rest are recorded here so the recommended few guide the work and the
non-opportunities are not re-litigated. The eye-veto remains the final quality arbiter for every
FG lever — no measured win ships without it.
