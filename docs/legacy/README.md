# docs/legacy — the shelf

Obsolete documents, kept. **Nothing here was deleted and nothing here should be.** A document lands
on this shelf when its work shipped, when a later document replaced it, or when a measurement killed
the route it proposed — and it lands with the record that closed it, so the reasoning stays
recoverable. If you are reading one of these, read its closing record too.

Moved 2026-09-08. Every relative link into and out of these files was rewritten in the same commit,
so the cross-references still resolve.

Held back deliberately, still live in their original places:

- `docs/research/ongoing/STAGE39_OUTPUT_CLOCK_DESIGN.md` — cited from `src/capture/capture_init.cpp:695`
  (by line, `:252`) and `src/control/cli.hpp:422`. A shipped source file points at it; it stays.
- The four `docs/research/PHYRIADFG_*` spine documents — called superseded by inference only, with no
  explicit successor. That call is the operator's, not a catalogue's.

## Delivered — the work shipped (24)

- [`SEPARATION_PLAN.md`](SEPARATION_PLAN.md) — was `docs/SEPARATION_PLAN.md`
  closed by: docs/planning/RESTRUCTURE_PLAN.md
- [`STEP5_FGCONTEXT_PLAN.md`](STEP5_FGCONTEXT_PLAN.md) — was `docs/STEP5_FGCONTEXT_PLAN.md`
  closed by: docs/planning/RESTRUCTURE_PLAN.md
- [`COPY_FENCE_DATAFLOW_MASTER_PLAN.md`](COPY_FENCE_DATAFLOW_MASTER_PLAN.md) — was `docs/research/COPY_FENCE_DATAFLOW_MASTER_PLAN.md`
  closed by: src/control/cli.cpp
- [`FG_OPTION_A_IMPLEMENTATION_STRATEGIES.md`](FG_OPTION_A_IMPLEMENTATION_STRATEGIES.md) — was `docs/research/FG_OPTION_A_IMPLEMENTATION_STRATEGIES.md`
  closed by: commit 18d0a60441ea72914ed9473b3a99086e74887cd1 "feat: LSFG-class own-window presentation by default + high-source-rate recalibration (v0.2.0)" (2026-07-02)
- [`FG_OPTION_A_MASTER_PLAN.md`](FG_OPTION_A_MASTER_PLAN.md) — was `docs/research/FG_OPTION_A_MASTER_PLAN.md`
  closed by: commit 18d0a60441ea72914ed9473b3a99086e74887cd1 "feat: LSFG-class own-window presentation by default + high-source-rate recalibration (v0.2.0)" (2026-07-02)
- [`FG_PRESENT_PACING_DESIGN.md`](FG_PRESENT_PACING_DESIGN.md) — was `docs/research/FG_PRESENT_PACING_DESIGN.md`
  closed by: docs/legacy/FG_OPTION_A_MASTER_PLAN.md (now shelved here) (explicitly 'productizes option A') + commit 18d0a60441ea72914ed9473b3a99086e74887cd1
- [`FG_PRESENT_TARGET_PACER_IMPLEMENTATION_STRATEGIES.md`](FG_PRESENT_TARGET_PACER_IMPLEMENTATION_STRATEGIES.md) — was `docs/research/FG_PRESENT_TARGET_PACER_IMPLEMENTATION_STRATEGIES.md`
  closed by: commit 18d0a60441ea72914ed9473b3a99086e74887cd1 "feat: LSFG-class own-window presentation by default + high-source-rate recalibration (v0.2.0)" (2026-07-02)
- [`FG_PRESENT_TARGET_PACER_MASTER_PLAN.md`](FG_PRESENT_TARGET_PACER_MASTER_PLAN.md) — was `docs/research/FG_PRESENT_TARGET_PACER_MASTER_PLAN.md`
  closed by: commit 18d0a60441ea72914ed9473b3a99086e74887cd1 "feat: LSFG-class own-window presentation by default + high-source-rate recalibration (v0.2.0)" (2026-07-02)
- [`FG_REALFAST_PATH_IMPLEMENTATION_STRATEGIES.md`](FG_REALFAST_PATH_IMPLEMENTATION_STRATEGIES.md) — was `docs/research/FG_REALFAST_PATH_IMPLEMENTATION_STRATEGIES.md`
  closed by: commit 18d0a60441ea72914ed9473b3a99086e74887cd1 "feat: LSFG-class own-window presentation by default + high-source-rate recalibration (v0.2.0)" (2026-07-02)
- [`FG_REALFAST_PATH_MASTER_PLAN.md`](FG_REALFAST_PATH_MASTER_PLAN.md) — was `docs/research/FG_REALFAST_PATH_MASTER_PLAN.md`
  closed by: commit 18d0a60441ea72914ed9473b3a99086e74887cd1 "feat: LSFG-class own-window presentation by default + high-source-rate recalibration (v0.2.0)" (2026-07-02)
- [`FG_SATURATION_STABILITY_IMPLEMENTATION_STRATEGIES.md`](FG_SATURATION_STABILITY_IMPLEMENTATION_STRATEGIES.md) — was `docs/research/FG_SATURATION_STABILITY_IMPLEMENTATION_STRATEGIES.md`
  closed by: docs/planning/STAGE_CONTRACT.md
- [`FG_SATURATION_STABILITY_MASTER_PLAN.md`](FG_SATURATION_STABILITY_MASTER_PLAN.md) — was `docs/research/FG_SATURATION_STABILITY_MASTER_PLAN.md`
  closed by: docs/planning/STAGE_CONTRACT.md
- [`FG_SATURATION_STABILITY_RISK_REGISTER.md`](FG_SATURATION_STABILITY_RISK_REGISTER.md) — was `docs/research/FG_SATURATION_STABILITY_RISK_REGISTER.md`
  closed by: docs/planning/STAGE_CONTRACT.md
- [`FG_VENDOR_AGNOSTIC_RISK_REGISTER.md`](FG_VENDOR_AGNOSTIC_RISK_REGISTER.md) — was `docs/research/FG_VENDOR_AGNOSTIC_RISK_REGISTER.md`
  closed by: src/core/device.cpp (lines 28-44) + src/core/device.hpp (lines 55-68)
- [`FG_WARP_SCALING_IMPLEMENTATION_STRATEGIES.md`](FG_WARP_SCALING_IMPLEMENTATION_STRATEGIES.md) — was `docs/research/FG_WARP_SCALING_IMPLEMENTATION_STRATEGIES.md`
  closed by: docs/legacy/FG_WARP_SCALING_RISK_REGISTER.md (now shelved here) (records the closing verdict) + src/control/cli.cpp (the shipped --warp-scale flag)
- [`FG_WARP_SCALING_MASTER_PLAN.md`](FG_WARP_SCALING_MASTER_PLAN.md) — was `docs/research/FG_WARP_SCALING_MASTER_PLAN.md`
  closed by: docs/legacy/FG_WARP_SCALING_RISK_REGISTER.md (now shelved here) + src/control/cli.cpp
- [`FG_WARP_SCALING_RISK_REGISTER.md`](FG_WARP_SCALING_RISK_REGISTER.md) — was `docs/research/FG_WARP_SCALING_RISK_REGISTER.md`
  closed by: src/control/cli.cpp (the shipped, documented --warp-scale flag, line 453)
- [`INPUT_LAG_DREDUCTION_MASTER_PLAN.md`](INPUT_LAG_DREDUCTION_MASTER_PLAN.md) — was `docs/research/INPUT_LAG_DREDUCTION_MASTER_PLAN.md`
  closed by: src/control/cli.hpp:752 (`bool low_d=true;` DEFAULT ON), src/control/cli.cpp:419-424 (--low-d/--low-d-frac/--low-d-span-cap/--real-fast-path/--rfp-window/--rfp-fresh, wording matches the plan verbatim), src/present/present.cpp (rfp override path) — all first committed at 79b5014d035588bbdf6ea655263d8c548a91f16a
- [`REAL_FAST_PATH_IMPLEMENTATION_STRATEGIES.md`](REAL_FAST_PATH_IMPLEMENTATION_STRATEGIES.md) — was `docs/research/REAL_FAST_PATH_IMPLEMENTATION_STRATEGIES.md`
  closed by: src/present/present.cpp:2234,2288 (dedicated bslot rfp_present path), src/core/core_init.cpp:215-223 (upscale-parity refusal), src/control/cli.cpp:422 (flag) — commit 79b5014d035588bbdf6ea655263d8c548a91f16a
- [`REAL_FAST_PATH_RISK_REGISTER.md`](REAL_FAST_PATH_RISK_REGISTER.md) — was `docs/research/REAL_FAST_PATH_RISK_REGISTER.md`
  closed by: src/present/present.cpp, src/core/core_init.cpp, src/control/cli.cpp:422
- [`RENDER_ASSISTANT_PLAN.md`](RENDER_ASSISTANT_PLAN.md) — was `docs/research/RENDER_ASSISTANT_PLAN.md`
  closed by: README.md; src/ (the shipped PhyriadFG tree)
- [`UPLOAD_OFFLOAD_IMPLEMENTATION_STRATEGIES.md`](UPLOAD_OFFLOAD_IMPLEMENTATION_STRATEGIES.md) — was `docs/research/UPLOAD_OFFLOAD_IMPLEMENTATION_STRATEGIES.md`
  closed by: src/control/cli.cpp:411 (commit 79b5014d035588bbdf6ea655263d8c548a91f16a)
- [`UPLOAD_OFFLOAD_MASTER_PLAN.md`](UPLOAD_OFFLOAD_MASTER_PLAN.md) — was `docs/research/UPLOAD_OFFLOAD_MASTER_PLAN.md`
  closed by: src/control/cli.cpp:411
- [`RENDER_ASSISTANT_KICKOFFS.md`](RENDER_ASSISTANT_KICKOFFS.md) — was `docs/research/ongoing/RENDER_ASSISTANT_KICKOFFS.md`
  closed by: src/flow/holons.cpp; docs/planning/ACTION_PLAN.md (S5 entry)

## Superseded — a later document replaced it (11)

- [`DESIGNER_BRIEF.md`](DESIGNER_BRIEF.md) — was `docs/planning/aap/DESIGNER_BRIEF.md`
  closed by: docs/planning/aap/A3_CHOSEN_DESIGN.md
- [`AYAMA_LAYERED_FG_IMPLEMENTATION_STRATEGIES.md`](AYAMA_LAYERED_FG_IMPLEMENTATION_STRATEGIES.md) — was `docs/research/AYAMA_LAYERED_FG_IMPLEMENTATION_STRATEGIES.md`
  closed by: docs/research/FG_ARCHITECTURE_DCAD_MASTER_PLAN.md
- [`AYAMA_LAYERED_FG_MASTER_PLAN.md`](AYAMA_LAYERED_FG_MASTER_PLAN.md) — was `docs/research/AYAMA_LAYERED_FG_MASTER_PLAN.md`
  closed by: docs/research/FG_ARCHITECTURE_DCAD_MASTER_PLAN.md
- [`CAPTURE_LAYER_IMPLEMENTATION_STRATEGIES.md`](CAPTURE_LAYER_IMPLEMENTATION_STRATEGIES.md) — was `docs/research/CAPTURE_LAYER_IMPLEMENTATION_STRATEGIES.md`
  closed by: docs/planning/STAGE_CONTRACT.md
- [`CAPTURE_LAYER_MASTER_PLAN.md`](CAPTURE_LAYER_MASTER_PLAN.md) — was `docs/research/CAPTURE_LAYER_MASTER_PLAN.md`
  closed by: docs/planning/STAGE_CONTRACT.md
- [`CAPTURE_LAYER_RISK_REGISTER.md`](CAPTURE_LAYER_RISK_REGISTER.md) — was `docs/research/CAPTURE_LAYER_RISK_REGISTER.md`
  closed by: docs/planning/STAGE_CONTRACT.md
- [`FG_REARCHITECTURE_MASTER_PLAN.md`](FG_REARCHITECTURE_MASTER_PLAN.md) — was `docs/research/FG_REARCHITECTURE_MASTER_PLAN.md`
  closed by: docs/legacy/INPUT_LAG_DREDUCTION_MASTER_PLAN.md (now shelved here) (its §8 'COMBAT decomposition')
- [`MINIMAL_FG_IMPLEMENTATION_STRATEGIES.md`](MINIMAL_FG_IMPLEMENTATION_STRATEGIES.md) — was `docs/research/MINIMAL_FG_IMPLEMENTATION_STRATEGIES.md`
  closed by: docs/planning/CONVERGENCE_IMPLEMENTATION_STRATEGIES.md (and docs/planning/CONVERGENCE_MASTER_PLAN.md)
- [`MINIMAL_FG_RISK_REGISTER.md`](MINIMAL_FG_RISK_REGISTER.md) — was `docs/research/MINIMAL_FG_RISK_REGISTER.md`
  closed by: docs/planning/CONVERGENCE_RISK_REGISTER.md
- [`PHYRIADFG_UI_IMPLEMENTATION_STRATEGIES.md`](PHYRIADFG_UI_IMPLEMENTATION_STRATEGIES.md) — was `docs/research/PHYRIADFG_UI_IMPLEMENTATION_STRATEGIES.md`
  closed by: ui/src-tauri/tauri.conf.json (first committed 79b5014d035588bbdf6ea655263d8c548a91f16a, 2026-06-28)
- [`PHYRIADFG_UI_MASTER_PLAN.md`](PHYRIADFG_UI_MASTER_PLAN.md) — was `docs/research/PHYRIADFG_UI_MASTER_PLAN.md`
  closed by: ui/src-tauri/tauri.conf.json (commit 79b5014d035588bbdf6ea655263d8c548a91f16a); README.md

## Abandoned — a measurement killed the route (3)

- [`CANDIDATE_A.md`](CANDIDATE_A.md) — was `docs/planning/aap/CANDIDATE_A.md`
  closed by: docs/planning/aap/A3_SELECTION_RATIONALE.md
- [`CANDIDATE_B.md`](CANDIDATE_B.md) — was `docs/planning/aap/CANDIDATE_B.md`
  closed by: docs/planning/aap/A3_SELECTION_RATIONALE.md
- [`WARP_OFFLOAD_MASTER_PLAN.md`](WARP_OFFLOAD_MASTER_PLAN.md) — was `docs/research/WARP_OFFLOAD_MASTER_PLAN.md`
  closed by: n/a

## Stale — it describes something that no longer exists (1)

- [`FG_PROJECT_IMPROVEMENT_MAP.md`](FG_PROJECT_IMPROVEMENT_MAP.md) — was `docs/research/FG_PROJECT_IMPROVEMENT_MAP.md`
  closed by: docs/planning/records/BACKLOG_AUDIT.md (lines 292-298)

*Made with my soul - Swately <3*
