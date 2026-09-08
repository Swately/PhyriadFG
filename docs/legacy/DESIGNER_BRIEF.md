# DESIGNER BRIEF — the self-contained fact pack for the isolated DESIGNER contexts (AAP A1)

> Every DESIGNER reads THIS + the frozen objective (`A0_FROZEN_OBJECTIVE.md`) + the files it cites,
> first-hand. Designers are mutually isolated: they write against the objective, never against each
> other. Each produces exactly ONE complete, buildable candidate from its ASSIGNED ANGLE (§4), with its
> own budget arithmetic and its own declared weakest point (AAP §3.A1).

## 1. What exists (verified first-hand 2026-08-29 … 09-02 by the supervising session)

**The full FG** — `F:\Phyriad\projects\PhyriadFG` (git; branch `analysis/0.3.0-quality-push`):
- Three pipelined threads through `src/core/fg_context.hpp` (`FgContext`, references to shared state):
  **C** capture (`src/capture/capture.cpp::run_capture`, DDA/WGC → convert → `hostR[slot]`, publishes
  `c_seq`), **F** flow (`src/flow/flow.cpp::run_flow`, per pair: `OpticalFlowPipeline::record_optical_flow`
  → copies MV+SAD to host bridges `hostMV/hostSAD[gen]` → CPU tail `consume_wap` (gme fit, holons) →
  publishes `f_pair_*` + `f_seq`), **P** present (`src/present/present.cpp::run_present`: the
  OUTPUT-CLOCK loop from line 1491; content clock NCO line 1975 / PLL 2054–2059; pair pick 2118–2122;
  `t_use` 2221→2370; `wap_upload` 713–836 uploads pair reals + MV/SAD to device-A images;
  `wap_warp_present` 879–1402 assembles the push block (976–1150) and dispatches the warp 1151–1152).
- **The warp shader** `shaders/wap_warp.comp` (1,336 lines). Push-constant block lines 77–135 = **57
  fields**; the generation core needs 4 of them (`residual_ceil, improvement_frac, agreement_threshold, t`,
  lines 78–81). The core math: primary MV fetch line 326; SAD pair 412–414; Gate 1 (confidence) 496–498;
  the two displaced samples 518–521 (`A_samp = prev[uv − t·mv]`, `B_samp = cur[uv + (1−t)·mv]`); Gate 2
  (tile agreement via shared memory) 522–529; `wa = 1−t` 637; `warp_result` 679; `blend_result` 692–695;
  final soft/hard selection 1136–1185; `imageStore` 1334. Everything else is layers gated by push fields,
  composed as sequential overrides of `result`; under the shipping defaults (`single_track=true`,
  `src/cli/cli.hpp:446`) the LAST writer is lines 1321–1328 (`result = mix(B_samp, cur[uv], w_s)`),
  which discards lines 531–1320 (the shader's own comment 1315–1317 says so). Layers that DO affect the
  default output are the ones mutating `mv` before line 521: mv-guided (324), inertia gate (332),
  phase-anchor (397–410), bg-reclaim (358–395, default 4.0), ambig (426–453), vblend (507–514); plus
  stasis folded into `w_s` (1327).
- **Motion estimation** — `framework/render/vulkan/`: `shaders/optical_flow_hier_match.comp` (267 lines;
  8×8 block SAD search ±radius, best/second, sub-pel; stores MV at 258 and {sad_best, sad_zero} at 259),
  `optical_flow_pyr_down.comp`, driver `src/OpticalFlowPipeline.cpp::record_optical_flow` 783–1025
  (pyramid 909–918, coarse→fine loop 927–956). MV+SAD live at W/8 × H/8, RG16F.
- **E1 done (2026-09-01):** `main.cpp` 3,133 → 1,222 lines; the init-seq is 13 `init_*` functions in
  `core/core_init.cpp`, `capture/capture_init.cpp`, `flow/flow_init.cpp`, `warp_blend/warp_blend_init.cpp`,
  `present/present_init.cpp`, over ownership structs in `core/app_init.hpp` (`DevicesInit`, `HostBridgeInit`,
  `ImagesInit`, `WapInit`, `BridgeInit`, `CmdSyncInit`, …). Main loop + teardown untouched.
- **Instruments:** `--qdump DIR N` (truth-less triples prev/live/next + `t` per triple; fires ONLY with
  `--no-async-present`, `present.cpp:1315`); `fg_quality_scorer` (in the container's catalog
  `F:\Phyriad\catalog\cpp\render\vulkan\bench\fg_quality_scorer`, builds today; Modes A/B/T; PSNR/SSIM/
  `dbl_edg_m`/`flowdsc`); `prep_zoo_sequence.py` (numpy-only deterministic zoo, 7 motion presets);
  `tools/ball_zoo.ps1` (live GDI ball at exact fps). A **MOTION_TRUTH** instrument (marker positions vs
  analytic trajectories, from `--qdump` + an MV dump) is PLANNED, not built.

**The minimal core** — `F:\Phyriad\apps\minimal_fg` (container, no git; builds today, 8.3 s):
- `src/main.cpp` (1,560 lines): WGC or DDA host-staged capture ring (`--capture-backend`), ONE SG pass
  `mc_interp` that records the catalog's `OpticalFlowPipeline::record_optical_flow(A, B, C, t=0.5)`
  (pyramid flow + the pillar's OWN gated warp `optical_flow_warp.comp`, 146 lines, correct temporal
  form) → blit C → `PresentSurface` present. Fixed t=0.5, naive present (no pacing/drop), single queue.
- `include/minimal_fg/seam_graph.hpp` (549 lines): `SeamGraph` — `declare_image(name, imported)`,
  `add_pass(name, reads, writes, record_callback)`, `mark_output`, `bind_image`, `compile()` (derives
  RAW/WAW/WAR barriers, layouts), `execute(cmd, compiled)`, `dump()` (`--sg-dump`); golden test
  `test/test_seam_graph.cpp` (436 lines, 122 checks, GPU-free).
- Its plan triad: `docs/research/MINIMAL_FG_{MASTER_PLAN,IMPLEMENTATION_STRATEGIES,RISK_REGISTER}.md`
  (Tier-2; MR-1 pacing/drop, MR-2 async-compute, MR-4 byte-identical-off, MR-5..MR-8 all `open`; MR-3
  SG hazards `mitigated` by derivation). Master plan §2–§4: MC-1 capture → MC-2 flow → MC-3 warp →
  MC-4 soft-mask blend → MC-5 paced present with DROP; SG properties: add a layer = 0 hand barriers,
  disabled layer = CULLED, 1 fence per frame-in-flight, GPU-resident, async-compute.

## 2. The decision this search makes (and only this)

**How the generation core and its layers are expressed as passes and data contracts on the SG seam** —
granularity (one fused pass vs many), the contract between core and layer (what a layer may read/write,
where a layer's parameters live, how a disabled layer contributes nothing), and how the shipping default
layer set is expressed so the A/B against today is possible (objective M4). NOT in scope: the seam
mechanism itself (SG, decided), the flow algorithm, the thread model, the capture backend, perceptual
quality passes' merits.

## 3. What a candidate MUST contain (AAP §3.A1 — a complete, buildable proposal)

1. **Units:** the passes / shaders / structs, named, with file placement in the PhyriadFG tree.
2. **The core contract:** the exact parameter list the core pass consumes (objective M2c ≤ 8), the images
   it reads/writes, and where `t` and the MV source enter.
3. **The layer contract:** how a layer is declared, what it may read/write, where its parameters live
   (no push-constant block shared with the core), how enabling/disabling works, and — decisively — how
   the composition of N enabled layers is ORDER-INDEPENDENT or has an EXPLICIT declared order that is
   part of the contract (kill criterion 2).
4. **The default-set mapping:** how EACH of the default-affecting layers (mv-guided, inertia gate,
   phase-anchor, bg-reclaim, ambig, vblend, single-track/screen-static, stasis) is expressed, so M4's
   replay equivalence can be run.
5. **The flags/UI touchpoint:** how a layer's options reach the CLI and the UI without four hand-synced sites
   (today's drift: 257 CLI flags vs 173 UI entries).
6. **Budget arithmetic under the envelope:** extra GPU passes/bandwidth per tick at 1920×1080 (state the
   intermediate images and their bytes), extra barriers per frame in the SG dump, CPU record cost; a
   first measurable milestone on the rig.
7. **What it drops** from the current design, explicitly, and **its own honest weakest point.**
8. **Trust tiers on every number** (`measured` / `estimated` / `unmeasured`) — no fabricated figure.

## 4. The three assigned angles (one per isolated designer; distinct bets, not costumes)

- **ANGLE A — "one fused pass, compiled variants":** keep the warp as ONE compute pass for bandwidth; the
  layers become compile-time SPECIALIZATION CONSTANTS / shader variants (a disabled layer is compiled out,
  not branched), the core contract is a fixed uniform block, and layer parameters live in a separate
  per-layer uniform buffer bound only when enabled. Bet: maximum GPU efficiency + zero runtime branching.
- **ANGLE B — "stages as SG passes":** the generation is a small fixed set of separable STAGE passes on
  SG (e.g. MV-conditioning → sampling/warp → composition → post), each with a declared image contract; a
  layer is a node that plugs into exactly one stage's declared slot; disabled = culled by SG. Bet: maximum
  modularity, order made explicit by stage topology, at some intermediate-image bandwidth cost.
- **ANGLE C — "data-driven layer table":** a single registry (a table / X-macro / manifest) is the ONE
  source of truth per layer: its shader entry, its parameters (with defaults and CLI/UI metadata), its
  reads/writes and its stage. Codegen or constexpr tables derive the Config fields, the parser, the help,
  the UI model and the SG registration. Bet: kill the four-site drift structurally; the shader side may be
  either fused or staged (the designer picks and justifies), but the contract is the table.

Each designer writes its candidate to `docs/planning/aap/CANDIDATE_<A|B|C>.md` (English, self-contained,
every number trust-tiered, the weakest point named). Length: as long as needed to be buildable; no padding.
