# REAL_FAST_PATH_IMPLEMENTATION_STRATEGIES — STAGE-109 `--real-fast-path` (`--rfp`)

> **Diátaxis type:** How-to (implementation strategies). **Tier-2** (PLAN_TIER_PROTOCOL): the
> companion strategies of the master-plan + the **RISK_REGISTER** (the gate). Each strategy below
> cites the risk ID it mitigates; NO commit while any register risk is `open`.
> **Linked set:** [`INPUT_LAG_DREDUCTION_MASTER_PLAN.md`](INPUT_LAG_DREDUCTION_MASTER_PLAN.md) ·
> [`REAL_FAST_PATH_RISK_REGISTER.md`](REAL_FAST_PATH_RISK_REGISTER.md) · this.
> **Detailed edit spec:** the `fg-dreduction-design` workflow output (run `wf_e95d2e87`) holds the
> HEAD-accurate Stage-2 edit detail; the implementer reads it + the code first-hand (lines drift).
> **Status:** `designed`.

## The mechanism

On a tick the present loop would show a **REAL** frame (not an interpolated phase), `--rfp` presents
the **freshest captured real** with minimal D — a real needs no interpolation pair, so it skips the
pair-publish wait that inflates D for interp ticks. Interp ticks are UNCHANGED (they keep the
Stage-1-trimmed D). The win: the reals track the input (the LSFG-class "responsive reals + smooth
interp" feel) — a PARTIAL latency cut (reals only), not a full pipeline-depth reduction. Default-off,
byte-identical-off; implies `--async-present` (the present must be the non-blocking bslot path).

## Strategies (each cites its risk ID)

- **S1 — present the real via the DEDICATED async slot, never `do_present_P` [CR1].** The real-fast-path
  presents through the `bslot[]` non-blocking re-present tail in `wap_warp_present`
  (`ra_surface.submit(bslot[front].nt)`), NOT the synchronous `do_present_P`/`bridge_present_src`
  (which resets/awaits the SHARED `cmdBridge`/`fBridge` → use-after-reset with an in-flight async warp).
  Record the real into a dedicated slot or reuse the async front-slot re-present. No present-thread
  reset of the shared bridge while a warp is in flight.
- **S2 — the override point.** The "bypass D" framing is FALSE — D + the pair selection run ABOVE the
  `if(use_wap)` block. So `--rfp` does NOT remove D; it **overrides the already-D-selected pair with the
  freshest real for this tick** when the tick is a real-present (and a valid real is available [FR1]).
  Insert the override after the selection, gated on `cfg.real_fast_path && <real-tick> && <real-ready>`.
- **S3 — true phase-1 key on the real tick [CR2].** Set `last_pres_k = (int)(span_ms/0.1+0.5)` (NOT
  INT_MAX) so the next interp's backwards guard + `--phase-norm` grid resume cleanly [CR5].
- **S4 — upscale parity [CR3].** Replicate `do_upscale_real_P(rs)` on the real-fast-path tick, OR gate
  `--rfp` off under `use_upscale` with a printed notice (assert `!use_upscale`).
- **S5 — no manual frame count [CR4].** Do not bump `total_frames`; the present helper on the path
  taken already increments it.
- **S6 — the valid-real gate [FR1].** Take the real-fast-path only when a fresh captured real is
  available (mirror the `--fdrop` `async_front>=0`-style guard); else fall through to the normal
  interp present.
- **S7 — byte-identical-off + the flag.** `--real-fast-path` (alias `--rfp`) in `parse_extra` (NOT the
  main else-if chain — C1061), default-off, implies `--async-present`. When off, NO override path is
  taken → byte-identical. lint_hal clean; §9 signature last line.

## Build + gate (PLAN_TIER_PROTOCOL §3 — close every register risk first)

Build green incrementally. Then the runtime gate (operator BF6 combat): `--upload-xfer --rfp` (+
`--phase-norm`) vs the base. PASS iff: (a) 30 s clean, NO device-lost, watchdog 0 stalls [CR1]; (b)
the real-tick `lat` drops vs base AND the DIAG/phaselog confirms it [the win]; (c) `frz` does NOT rise
[CR2/FR1]; (d) operator eye: more responsive reals, no new judder, the `--phase-norm` ladder stays
coherent [CR5]. Mark each register risk `mitigated` only after its row's Verification is run
first-hand; do NOT commit with any risk `open`.
