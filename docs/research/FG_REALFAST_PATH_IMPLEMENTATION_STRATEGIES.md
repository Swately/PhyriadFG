# FG_REALFAST_PATH_IMPLEMENTATION_STRATEGIES — the buildable sibling

> **Diátaxis type:** How-to (implementation strategies). **Tier:** **Tier-2** (PLAN_TIER_PROTOCOL) — the
> companion strategies of the master-plan + the **RISK_REGISTER** (the gate). Each strategy below cites the
> risk ID it mitigates; NO commit while any register risk is `open`.
> **Status:** `designed` (L2/L3 not built; L0 `--rfp`/`--rfp-fresh` is `measured`, indexed only; **L1's
> `--rfp-window F` flag is already `shipping`** — `main.cpp:621`/`:1133`/`:8478` — so L1's only `designed` delta
> is the RFP-FR1 safe-ceiling clamp on the already-exposed window).
> **Linked set:** [`FG_REALFAST_PATH_MASTER_PLAN.md`](FG_REALFAST_PATH_MASTER_PLAN.md) ·
> [`FG_REALFAST_PATH_RISK_REGISTER.md`](FG_REALFAST_PATH_RISK_REGISTER.md) · this.
> **Prior triad it extends:** [`REAL_FAST_PATH_IMPLEMENTATION_STRATEGIES.md`](REAL_FAST_PATH_IMPLEMENTATION_STRATEGIES.md)
> (the STAGE-109 `--rfp` strategies — L0's S1-S7 are NOT re-stated; this doc adds L1/L2/L3).
> **Anchors:** all `file:line` re-confirmed first-hand against `apps/render_assistant/src/main.cpp` in the
> authoring pass; lines drift → the implementer + verifier re-confirm before editing.

The key words MUST, MUST NOT, SHOULD, MAY are BCP 14 (RFC 2119 / RFC 8174).

---

## The mechanism (recap; full design in the master-plan §3-§4)

PhyriadFG's own added latency is ~73-84 ms (`measured`), dominated by `freshage_ema` ~33-41 ms (the
pair-publish pipeline lag; `main.cpp:8251`, used in D at `:8281-8283`). The floor cannot be cut by trimming D
(D >= `freshage_ema` always, clamp `:8282`); it can only be cut by **shrinking `freshage`**. Three levers add
to the established `--rfp` real-tick path:

- **L1 `--rfp-window F`** — widen the real-fast-path firing fraction (read-side, responsiveness). The flag is
  ALREADY `shipping` (`main.cpp:621`/`:1133`/`:8478`); L1's only `designed` work is the RFP-FR1 safe-ceiling clamp.
- **L2 `--fwd-prestage`** — collapse the F per-pair build gap ~20 → ~9 ms (the PRIZE; hot F-build path).
- **L3 `--copy-device`** — reclaim the INVISIBLE WGC copy ~5.6 ms (pre-`tcap`, lower-risk).

All opt-in, default-off, byte-identical-off.

---

## Strategies (each cites its risk ID)

### Lever L1 — `--rfp-window F`: widen the firing window

- **S-L1.1 — the firing condition relax [RFP-FR1]. NOTE (corrected 2026-06-23): the `--rfp-window F` flag is
  ALREADY shipping** — `cfg.rfp_window` exists with default `0.15f` (`main.cpp:621`), the `--rfp-window` parser
  is already in `parse_extra` (`main.cpp:1133`, clamp `[0,1]`), driving the fire gate at `main.cpp:8478`
  (`cfg.real_fast_path && have_interp && real_valid && cur_c>=1 && cur_c!=last_rfp_c && gen_back==0 &&
  phase_global >= 1.0 - (double)cfg.rfp_window`). Widening (a LARGER `F` → fires at a lower `phase_global`) is
  thus available TODAY; L1 does NOT need to expose the flag. **The only design work here is the FR1 safe-ceiling
  clamp:** keep `gen_back==0` (only the freshest pair) and `cur_c!=last_rfp_c` (no double-fire) UNCHANGED — they
  are the safety invariants; and tighten the current `[0,1]` parse clamp at `:1133` to a verified freeze-safe
  ceiling so a larger window MUST NOT fire below the phase where the next interp's backwards-guard key
  `(int)(span_ms/0.1+0.5)` would self-trip.
- **S-L1.2 — preserve the CR2/CR5 bookkeeping [from L0].** The widened path reuses the EXISTING `--rfp` tail
  (`main.cpp:8495-8508`): `last_pres_cseq=pair_c; last_pres_k=(int)(span_ms/0.1+0.5);` (NOT the capture index,
  NOT INT_MAX — the RFR-UNIT fix), no manual `total_frames` bump (CR4), `continue`. L1 changes ONLY the FIRE
  CONDITION, never the present body → it inherits L0's mitigated CR1-CR5.
- **S-L1.3 — byte-identical-off + the flag [RFP-DOGMA1].** `--rfp-window` already lives in `parse_extra` (NOT
  the main else-if chain — the C1061 lesson) at `main.cpp:1133`; the FR1 clamp tightening edits THAT parser in
  place. When `--rfp` is off the whole block is skipped → byte-identical; when on
  but the window is its default the behavior equals shipped `--rfp`. lint_hal clean; §9 signature last line.

### Lever L2 — `--fwd-prestage`: collapse the F per-pair build gap (the PRIZE)

- **S-L2.1 — locate the gap [the measured target].** The F build accrues at `main.cpp:6390` (`t_fuse_ema`) and
  publishes at `:6406-6413`; the record region is `:6489-6557`. The ~11 ms non-compute gap is the `hRP_b`→device
  PCIe copy `:6550` + B unpack + B-queue contention (flow + gme-gpu + unpack share queue B). The compute window
  (fsub flow + cpu) is only ~8.7 ms — confirm with `--latency-trace` `build` vs the flow/cpu EMAs before editing.
- **S-L2.2 — pre-stage the B-copy OFF the F iteration [RFP-CR2, RFP-DOGMA1].** Move the `hRP_b`→device copy so
  it runs during the PRIOR pair's compute (it depends only on the captured slot, available earlier) — a
  double-buffered staging copy submitted ahead, so the F build iteration finds the device buffer already
  populated. This is a pipeline reshape exactly like the existing `--upload-xfer` (`fwd_pipe`,
  `cmdB_fwd[(int)(g_seq&1u)]`/`fB_fwd[(int)(g_seq&1u)]` at `:6545-6546`) — REUSE that double-buffer discipline.
  Gate ALL new code on a `cfg.fwd_prestage` bool; when false, the else-arm MUST be character-for-character the
  current `:6550` copy on `cmdF` → byte-identical-off.
- **S-L2.3 — OR a dedicated B-transfer queue [RFP-CR2].** Alternatively (or additionally) submit the B-copy on a
  dedicated transfer queue so it does not contend with flow/gme-gpu/unpack on queue B. A NEW queue sharing
  ownership of the staging buffer is a **concurrency hazard** — it MUST carry an explicit acquire/release
  ownership transfer (queue-family barrier) or a timeline-semaphore sync so the F-build's first read of the
  device buffer happens-after the transfer queue's copy completes. NO mutex on the F hot path (lock-free mandate).
  This is the load-bearing crash/concurrency risk → its mitigation-as-code is RFP-CR2 in the register.
- **S-L2.4 — measure the sub-split FIRST [efficiency mandate].** Before committing to pre-stage vs dedicated
  queue, instrument the F-pair sub-split (upload / unpack / B-wait) to partition the 11 ms (the master-plan §8
  "instrument the F-pair sub-split" NEXT). Build the lever that the measured partition says wins. D-2: if you
  cannot measure the gap closing, you cannot claim the lever.
- **S-L2.5 — byte-identical-off discipline [RFP-DOGMA1].** `--fwd-prestage` (alias TBD) in `parse_extra`,
  default-off. The proof obligation is a `--no-`/off A/B with `--csv` producing a byte-identical present stream
  (the established STAGE-108..112 discipline). lint_hal clean; §9 signature last line.

### Lever L3 — `--copy-device`: reclaim the INVISIBLE WGC copy

- **S-L3.1 — move the WGC `CopyResource` staging [lower-risk, pre-`tcap`].** The ~5.6 ms `lt_copy_us` (printed
  at `main.cpp:8984-8985`) is the WGC staging copy, upstream of `tcap` (not in our `lat`, but perceived). Route
  it to a separate device / async copy so it overlaps the compose. It does NOT touch the bridge/present path →
  NOT crash-class on the present side, but it MUST NOT hand a torn/partial staged frame to pickup → a completion
  fence before the slot is marked captured.
- **S-L3.2 — byte-identical-off + the flag.** `--copy-device` in `parse_extra`, default-off; when off, the copy
  stays on the current device path character-for-character. lint_hal clean; §9 signature last line.

---

## Build order + gate (PLAN_TIER_PROTOCOL §3 — close every register risk first)

Build green incrementally. Recommended order: **L1 + L3 first** (lighter — bank the responsiveness widen + the
invisible-copy reclaim) while L2's sub-split measurement (S-L2.4) + the RFP-CR2 ownership-sync are hardened;
**L2 last** (the prize, the heaviest, the crash/concurrency gate).

Runtime gate (operator BF6 combat, same scene, default path):

- **L1:** `--rfp --rfp-window <wide>` vs base `--rfp` — real-tick `MsAddedLatency` win persists, `frz` NOT up,
  watchdog 0 stalls, eye: sawtooth tolerable [RFP-FR1].
- **L2:** `--upload-xfer --fwd-prestage` vs base — `--latency-trace` `build` drops (~20 → ~9 ms) AND
  `freshage_ema` drops AND `lat` drops; **30 s clean, NO device-lost, watchdog 0 stalls, clean exit** [RFP-CR2];
  `--no-fwd-prestage` byte-identical via `--csv` diff [RFP-DOGMA1]; validation layers clean if armed.
- **L3:** `--copy-device` — `--latency-trace` `copy` drops; no tear/artifact; 30 s clean.

Mark each register risk `mitigated` ONLY after its row's Verification is run first-hand. Do NOT commit with any
risk `open`. A null/negative or any freeze/crash = STOP.
