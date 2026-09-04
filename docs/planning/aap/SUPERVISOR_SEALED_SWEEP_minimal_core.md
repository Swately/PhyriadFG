# SUPERVISOR'S SEALED GAP SWEEP — apps/minimal_fg vs A0 (written BEFORE any ATG verdict; sha256 sealed)

Per AAP §6.1 requirement 3: the supervising session's own enumeration of the minimal core's holes
against the frozen objective, written and hashed BEFORE reading any adversary verdict, so the
sweep-vs-verdict diff measures the session's blind spots. Facts below were verified first-hand
2026-09-02 (file reads); the gap classification is the session's judgment.

## Gaps I can see (the minimal core, `F:\Phyriad\apps\minimal_fg`, as it stands)

1. **Fixed phase t=0.5.** `main.cpp:801` records `record_optical_flow(c, a, b, c_view, 0.5f)`; there is
   no per-tick phase, no content clock, no pair selection by phase. Objective M1 needs generated frames
   at arbitrary t → the core must take `t` per tick (the full FG's WAP does; the minimal core does not).
2. **No pacing / no drop (MR-1 open).** Present is naive; P3 of its own plan was never built. Motion
   exactness in TIME (phase error) cannot be measured without a deterministic output clock.
3. **Single queue (MR-2 open).** No async-compute; the seam's own budget (1 fence/frame, compute‖present)
   is unproven at runtime (MR-3 runtime sync-validation "pending").
4. **Coupled to the CATALOG's pipeline, not PhyriadFG's.** CMake compiles
   `../../catalog/cpp/render/vulkan/src/OpticalFlowPipeline.cpp` + `PresentSurface.cpp`; PhyriadFG's
   vendored `OpticalFlowPipeline.cpp` differs by 427 lines (fg-variant matcher, candsel, prebake…) and
   the matcher shader by 146. Adopting the core into the repo means choosing ONE pipeline lineage.
5. **The warp is the pillar's `optical_flow_warp.comp`, not `wap_warp.comp`.** Correct temporal form,
   but it has NO layer contract at all — the objective's decision (layer contract) has no substrate in
   the minimal core yet; it is a bare core.
6. **No MV export / no replay record.** The MV+SAD images stay on the GPU; no `--qdump`-class dump →
   M1/M4 cannot be measured on it today (kill criterion 1 until a dump exists).
7. **Host-staged capture ring, not GPU-resident.** The June record's "zero-copy" claim was for WGC in the
   earlier version; the CP1 DDA path is host-staged (`CapCtx` ring) — a host round-trip on the capture
   side (kill criterion 3 targets STAGES; capture ingest is arguably outside, but it is a seam cost).
8. **No UI / no config surface.** Flags are a hand parser (`--sg-dump`, `--capture-backend`, …); the
   four-site drift problem does not exist here only because there are almost no options.
9. **No stability parity with the full FG.** Only "round 1" fixes (FrameArrived guard, keyed-mutex
   recovery, window watchdog) were ported; thread-protection, device-loss recovery, the governor,
   HDR convert, rotated-display fix, DPI awareness are all in the full FG only.
10. **Single-GPU only (by design, fine for this rig).**
11. **No tests beyond the SG golden test**; no scorer hook; not in the fleet testbench report.
12. **Git-less location** (`apps/` in the container) — adoption into PhyriadFG is a prerequisite for any
    plan stage to be reviewable.

## What I believe is NOT a gap (deliberate, per its plan)
- No quality layers (the hello-world discipline, MINIMAL_FG §2).
- t=0.5-only was P1's scope; the phase generality was expected from the full FG's WAP at re-layer time.

## What I could not verify
- Whether the SG runtime is validation-layer clean today (no run with validation layers this session).
- Whether the June "se siente bien" eye verdict holds on the CURRENT rig state (single 4090, 25H2).
