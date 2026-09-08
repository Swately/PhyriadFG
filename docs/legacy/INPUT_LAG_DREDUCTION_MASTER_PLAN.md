# INPUT_LAG_DREDUCTION_MASTER_PLAN — STAGE-108 `--low-d` + STAGE-109 `--real-fast-path`

> **Diátaxis type:** Explanation + plan (a design master-plan incl. the implementation
> strategy + the freeze/crash-risk catalog — one doc, FDP-conform).
> **Status buckets:** `measured` (first-hand / operator-verified), `designed` (specified,
> not built), `assumed` (a model/estimate, flagged). Nothing is `shipping` yet.
> **Scope:** cut the PhyriadFG **input-lag EXCESS** (the ~70 ms combat added-latency vs the
> ~22 ms light-scene floor) WITHOUT introducing present-before-pair-ready freezes. Opt-in,
> default-off, byte-identical-off. Companion SOTA: `FG_CADENCE_LATENCY_PRIOR_ART.md`.
> **Provenance:** the `fg-dreduction-design` workflow (run `wf_e95d2e87`, 4-explore /
> 3-design / adversarial-verify); every `file:line` re-confirmed first-hand against the
> working tree (incl. STAGE-104..107) — the implementer MUST re-confirm (lines drift).

---

## §0 — The honest framing

The operator confirmed STAGE-107 `--phase-norm` halves the perceived input-lag "weight"
(the even ladder makes motion track the input). The **other half is the actual latency** —
the **D-anchor** (`D`, the pipeline delay so the interpolated pair is published before its
ticks come due). `measured`: D ≈ 22 ms light-scene, ≈ 50-89 ms combat (inflated by F-thread
**lap-escapes** — the F-thread skipping source frames → wide-span pairs → each skipped frame
× T_src straight into D). The light-scene ~22 ms latency PROVES the pipeline can be
LSFG-class; the combat excess is **removable**. The SOTA verdict (`FG_CADENCE_LATENCY`): for
an external no-Reflex-hook tool, low lag = a **shallow present queue** + trimming the D
excess. NO Reflex (we lack the hook); NO VRR (overlay can't drive it).

**The crux tension:** D EXISTS to prevent present-before-pair-ready (a freeze). Cut D too far
→ the present reaches for a pair not yet published → freeze. So every lever must cut the
EXCESS while staying above the freeze floor (`freshage_ema_ms` — the feedback-free
detect-time pipeline measure).

---

## §1 — Ground-truth corrections (verified; they reshape the naive design)

- **"bypass D" is FALSE.** `if(use_wap){` opens at ~7182; D (7006-7008), `t_display = now−D`
  (~7038), and the pair selection (~7058-7178) all run ABOVE it. D already ran this tick — a
  branch at 7182 cannot "remove D". The honest scope is **override the already-D-selected pair
  with a fresher real for this tick** (Stage 2), or **trim D's excess at the source** (Stage 1).
- **`do_present_P`/`bridge_present_src` is SYNCHRONOUS on `A.q` and touches the SHARED
  `cmdBridge`/`fBridge`/`bridge_img`** (`bridge_present_src` `:7165-7184`, `vkQueueSubmit(A.q)`+`vkWaitForFences`
  `:7182`; `do_present_P` `:7188` — corrected 2026-06-23 from a drifted `:6111-6129` cite). Under `--async-present` the warp uses the
  DEDICATED `cmdBridgeA0`/`fBridgeA0`/`bslot[0]` (the STAGE-102 split). Calling `do_present_P`
  while an async slot-0 warp is in flight = **use-after-reset GPU-fault (crash-class)** — the
  exact reason STAGE-102 split the buffers. → the naive `--real-fast-path` v1 (call
  `do_present_P`; `continue`) is **DISQUALIFIED when co-armed with async**.
- **`total_frames.fetch_add(1)` lives ONLY in the present helpers** (6043, 6723). A manual
  `++total_frames` in a real-fast-path tick double-counts.
- **The `last_pres_k=INT_MAX` sentinel is self-defeating** — the backwards guard
  (`pair_c==last_pres_cseq && cand_k<last_pres_k`) then trips on the next interp of the same
  fresh pair → forces `t_use→1.0` → the pair freezes at phase 1. Use the **true phase-1 key**
  `(int)(span_ms/0.1+0.5)`.
- **`do_present_P` skips the upscale pre-pass** (`do_upscale_real_P`); a real-fast-path tick
  must replicate it or assert `!use_upscale`.

---

## §2 — STAGE 1: `--low-d` (D-anchor span-term floor-trim) — SHIP FIRST (lowest freeze risk)

Trim ONLY the additive **span term** of the phasefix D (7006-7008); KEEP `freshage_ema_ms` as
the freeze floor. Attacks the lap-escape inflation. No real-frame mixing → cannot break the
`--phase-norm` ladder (verified orthogonal). `designed`.

**Edits (verified sites):**
1. **cfg, beside `bool phasefix=true;` (main.cpp:548):**
   ```cpp
   bool   low_d=false;          // STAGE-108 (--low-d): D-anchor span-term floor-trim
   double lowd_span_frac=0.5;   // span coverage kept (clamp [0,1]; 1.0 = no trim)
   double lowd_span_cap=1.5;    // lap-escape growth ceiling, in source frames (clamp [1,8])
   ```
2. **The D expression (main.cpp:7006-7008)** — add a THIRD arm gated `cfg.phasefix && cfg.low_d`;
   leave the other two arms byte-identical:
   ```cpp
   const double D = !delay_init ? 0.0
       : (cfg.phasefix
            ? (cfg.low_d
                 ? std::clamp(freshage_ema_ms + std::min(cfg.lowd_span_frac*span_fresh_ms,
                                                         cfg.lowd_span_cap*T_src),
                              std::max(4.0, freshage_ema_ms), 250.0)
                 : std::clamp(freshage_ema_ms + span_fresh_ms, 4.0, 250.0))
            : std::clamp(delay_ema_ms*1.2 + 0.5*span_fresh_ms, 4.0, 250.0));
   ```
3. **parse_extra, after the `--phase-norm` matcher (main.cpp:970):** `--low-d`, `--low-d-frac F`,
   `--low-d-span-cap F` (in parse_extra, NOT the main chain — C1061).
4. **`--help`** — one `--low-d` parity line.

**FREEZE-GUARD (the floor, why Stage 1 stays above it):** the existing phasefix EDGE CAP
(`t_display = min(t_display, f_pair_tcap_a[f_gen_new])`, ~7043-7045, active under phasefix)
pins t_display to the freshest published edge. The trim only ADDS the span term and raises the
lower clamp to `max(4.0, freshage_ema_ms)`, so **D ≥ freshage_ema_ms always** → `t_display =
now−D ≤ now−freshage ≈ tcap_new` → a smaller D selects a FRESHER (more-published) pair →
monotonically REDUCES freeze risk. **Honest:** this is a NEW freshage-floored operating point
(~0.2·freshage below the legacy `delay_ema·1.2`), `validated-not-asserted` — do NOT claim it
equals a shipped-freeze-free point.

**Byte-identical-off + lock-free:** `low_d=false` → the inner ternary selects the original
phasefix clamp character-for-character; the scalars are inert. P-thread-local double arithmetic
on in-scope values (`freshage_ema_ms`, `span_fresh_ms`, `T_src`); `std::min/max/clamp` pure; no
atomic/mutex/heap. lint_hal clean.

**Runtime gate (same BF6-combat scene, DEFAULT path = sc-select+sync-clock ON):**
`--upload-xfer --phaselog 20 --csv base` vs `--upload-xfer --low-d --phaselog 20 --csv lowd`.
PASS iff: (a) `--phaselog` `lat=` drops AND the live DIAG `D=` (~7629) actually dropped
(cross-check; part of the lat delta is the phase shift toward 0.5, NOT all D); (b) **`frz` does
NOT rise** + the watchdog stays 0 stalls; (c) operator eye: the "weight" drops further without a
new judder. A null/negative or any `frz` rise = STOP.

---

## §3 — STAGE 2: `--real-fast-path` (deferred; crash-class — Opus + verify like the upload sync)

Override the D-selected pair with the **freshest captured REAL** for this tick (reals need no
interpolation pair → no D-wait → ~floor latency + max responsiveness), interp ticks keep D.
`designed`, deferred behind Stage 1's measured win. **MUST** (per §1): present the real via the
DEDICATED async slot path (NOT `do_present_P` — crash-class with async); use the true phase-1
`last_pres_*` key (not INT_MAX); replicate/assert the upscale pre-pass; NOT manually bump
`total_frames`; and reconcile with `--phase-norm` (a low-lag real interleaved with delayed
interp must not break the even ladder — likely gate real-fast-path to advance `last_pres_*`
cleanly). Crash-class → the upload-offload procedure: a short strategies pass + a protocol-Opus
implement + first-hand verify + a 30 s BF6 crash/freeze gate.

---

## §4 — Freeze/crash-risk catalog (the guardrail)

| # | Risk | Mitigation | Stage |
|---|---|---|---|
| FR1 | **present-before-pair-ready freeze** (D cut below the published edge) | D ≥ freshage_ema floor + the phasefix edge cap; `frz`-must-not-rise gate | 1 |
| FR2 | lap-escape D-balloon not actually trimmed | the `lowd_span_cap·T_src` clamp on the span term; cross-check the DIAG `D=` | 1 |
| CR1 | **use-after-reset GPU-fault** (real-fast-path calls sync `do_present_P` while an async warp holds slot-0) | present the real via the dedicated bslot path, never `do_present_P` under async | 2 |
| CR2 | pair frozen at phase 1 (INT_MAX sentinel trips the backwards guard) | true phase-1 key `(int)(span_ms/0.1+0.5)` | 2 |
| CR3 | wrong-size bridge copy (upscale pre-pass skipped) | replicate `do_upscale_real_P` or assert `!use_upscale` | 2 |
| CR4 | `total_frames` double-count | rely on the present helper's increment; no manual bump | 2 |
| CR5 | real-fast-path breaks the `--phase-norm` even ladder (low-lag real among delayed interp) | reconcile `last_pres_*` advance; eye-validate the ladder under both flags | 2 |

### STAGE-110 risk catalog (the three implemented levers — mitigation AS CODE + first-hand verification; none `open`)

| # | Risk | Mitigation (as code) | Verification (first-hand) | Status |
|---|---|---|---|---|
| RFR-UNIT | **`--rfp-fresh` content rewind/freeze** — anchoring the content-order key at the CAPTURE index (`cur_c-1`) trips a hard PLL reseat + a `freshage`-long backwards-guard freeze (content_clock/pair_c trail capture by freshage). This is the designer's proposed reconciliation; it REINTRODUCES the operator-observed instability. | Present the FRESH pixels but KEEP the order key at `pair_c`: `last_pres_cseq=pair_c; last_pres_k=(int)(span_ms/0.1+0.5);` and NO `content_clock` push. The fresh real is `rfp_present((cur_c-1)%cap_slots)`; the ladder bookkeeping is byte-identical to the shipped `--rfp`. | 30 s BF6: `stall_count 0`, `freeze_count 1`, `er=0`; real-tick `MsAddedLatency` mean **10.5 ms** (n=83) vs interp **72.9 ms** → the responsiveness mechanism works; the cost is the accepted content sawtooth. | mitigated |
| SQ1 | **shallow-queue double-promote / foreign-slot touch** (promote a slot not submitted this tick, or re-promote next tick). | Guard `record_this_tick && async_inflight==back && back>=0`; on hit `async_front=back; async_inflight=-1;` (the preamble's `async_inflight>=0` guard is then false next tick). | 30 s BF6 `--upload-xfer --shallow-queue`: `stall_count 0`, `er=0`, `ps ok` clean; `done` line balanced. | mitigated |
| SQ2 | **shallow-queue reintroduces the STAGE-102 blocking stall.** | `vkGetFenceStatus` (non-blocking) inside `do{…}while((now_ms()-sq_t0)<sq_cap_ms)` with `sq_budget_us` clamp `[0,4000]`; HAL `phyriad::hal::spin_hint()` (NOT raw `_mm_pause`, NOT `vkWaitForFences`). | grep the block: zero `vkWaitForFences`; `max_stall_ms 0.0`; present_fps held (175 ≈ the `--upload-xfer` baseline) → no count regression. | mitigated |
| SQ3 | **present an in-flight (not-yet-signalled) slot** → torn frame. | promote reached ONLY via `if(sq_done)` set EXCLUSIVELY by `vkGetFenceStatus(...)==VK_SUCCESS`; on timeout the prior (confirmed-complete) `async_front` is shown. The keyed-mutex (`bridge_use_km`) serialises the compositor read vs the next overwrite (unchanged). | 30 s BF6 fast-camera combat: `er=0`, no visible tear in the overlay; the hit path presents EARLIER but overwrites on the same schedule → MORE compositor cushion, not less. | mitigated |
| CPH-MONO | **cphase `g(t)` non-monotone** (the design's Hermite-blend reversed for ratio≠1, manufacturing the pulse). | Proper cubic Hermite `g=W·[s0·(u³−2u²+u)+(−u³+2u²)]` with `s0∈[0.5,2]` (Fritsch-Carlson bound `s0²+1≤9`); ratio clamp `[0.5,2]`; key guard `if(!(have_last_pres&&pair_c==last_pres_cseq&&gk<last_pres_k))`. | `--phaselog` ladder STRICTLY non-decreasing under cphase: `t=[0.03 0.14 0.25 0.36 0.44 0.51 0.58 0.65]` (opening eased, monotone); `freeze_count 0`; multiplier 1.90 preserved; `adaptive_stdev 4.1 ms` (no jitter). | mitigated |
| CPH-TEXT | **cphase ghosts fine text** (the `--vblend-strength 1.0` failure: A/B desync). | mv is NEVER touched (only the scalar `t`); endpoints EXACT (`g(0)=0,g(1)=1` by construction + FP clamp) → A/B converge → text-safe. | endpoints exact in the ladder (`t→0`/`t→1` unchanged); operator eye: no new text ghosting. | mitigated |

---

## §5 — Recommendation + the measured result

**MEASURED (2026-06-17, BF6 combat A/B, operator eye + data): STAGE-108 `--low-d` is a NULL/marginal
lever — DO NOT ship as the input-lag fix.** Same-scene A/B: D median 45.5→46.6 ms, lat median
74.5→72.2 ms (within noise); operator eye: "~50 g off the 2 kg, not really notable". Root cause
(honest): the scene had no lap-escapes (span=1 → the span term was already small, nothing to trim),
AND `--low-d` only trims the span term (~6 ms) while the **dominant D component is `freshage_ema`
(~33 ms, the pair-publish pipeline lag) which `--low-d` keeps untouched by design** (the freeze
floor). `--low-d` stays BUILT (default-off, byte-identical, harmless) but UNCOMMITTED — re-evaluate
only if a lap-escape-heavy combat shows it caps the worst-case D spikes.

**The real input-lag lever is STAGE-109 `--real-fast-path` (+ the shallow-queue).** The ~73 ms
breaks down as: `freshage` ~33 ms (pair-publish) + async buffering ~28 ms (`--upload-xfer` +
`--async-present`, ~1 frame each — the throughput config) + capture/present ~12 ms.

## §6 — STAGE-110: the three levers built + MEASURED (2026-06-17, BF6 combat, adversarial design wf wy025t0v3)

An adversarial design workflow (3 design + 3 skeptic agents) refuted the naive forms of all three and
reshaped them; each was then built default-off, byte-identical-off, lint-clean, and gated 30 s on BF6.

- **`--rfp-fresh` (STAGE-110 `--rfp` REDESIGN) — `measured` WIN, narrow.** The shipped `--rfp` was NULL
  because it presented the PAIR's real (`f_pair_slot_a[f_gen]`, ~freshage old). `--rfp-fresh` presents
  the FRESHEST captured real (`(cur_c-1)%cap_slots`) + measures vs its own tcap → real-tick
  `MsAddedLatency` **72.9 → 10.5 ms** (median 9.9, n=83/30 s ≈ 2.8/s; LSFG-class on those ticks). The
  designer's reconciliation (anchor the order key at the capture index) was a UNIT ERROR that would
  reseat/freeze (= the operator's observed instability) — avoided by KEEPING the order key at `pair_c`
  (RFR-UNIT). Cost: a content SAWTOOTH (fresh real → stale interp), an accepted opt-in visual effect;
  the clean fix is reducing freshage (a separate arc). Fires on only ~1.9 % of ticks → a responsiveness
  FEEL flag, not a global number.
- **`--cphase` (continuous-trajectory) — `measured` sound, SENSATION (zero ms).** C1 velocity-continuous
  intra-pair phase reshape; the design's `g(t)` was non-monotone (it MANUFACTURED the pulse) — replaced
  with a Fritsch-Carlson-bounded cubic Hermite (verified strictly monotone in the live ladder), two-var
  carry (stable ratio across the pair), no closing ease. Text-safe (mv untouched, endpoints exact → zero
  latency). Multiplier 1.90 preserved, `freeze_count 0`, no jitter. Cadence/sensation lever.
- **`--shallow-queue` — `measured` NULL in combat (the honest core).** Bounded non-blocking early-promote
  of the just-submitted warp. `--async-present` exists BECAUSE the 4090 overruns a tick in combat, so the
  hit rate is `sq:~0H/53M` at 99 % GPU → inert exactly where latency matters. NOT count-regressive
  (present_fps held). A mid-motion-only refinement; default-off, documented. The skeptic's prediction,
  confirmed first-hand.

## §7 — The operator's reframe (2026-06-17): latency is a BUDGET, not an enemy

Operator eye-verdict on all three: technically good, but **to the eye it still feels heavy / "lag" —
and that is NOT a failure.** It means the stack now has **headroom (space) to SPEND real-ms on filters /
techniques that refine the INTER-FRAME SENSATION.** The objective is not to minimise input-lag; the
input-lag is a *budget* we can pay down in exchange for perceptual smoothness/quality between frames.
This extends "sensación > números": the next arc is **inter-frame sensation filters that cost real ms by
design** (e.g. temporal smoothing, multi-candidate select/discriminate, disocclusion/gravity quality,
shutter/persistence shaping) — paid for with the latency we stopped fighting. `--cphase`/`--rfp-fresh`
are the first such *space-makers*; build the sensation passes ON the space they (and the count win) give.

The three STAGE-110 levers are the new opt-in substrate. Build STAGE-109/110 went via the upload-offload
procedure (crash-class care: §1's corrections + §4's catalog → adversarial design → first-hand verify →
30 s BF6 crash/freeze gate). Re-SOTA on the inter-frame-sensation direction next (operator's plan).

## §8 — PHASE-1 floor-minimization (operator: minimize the FORCED ms first, THEN spend as currency)

The operator reframed the order: FIRST squeeze the input-lag floor to the architectural minimum, THEN spend
ms on inter-frame quality. Two new measurement levers + a full combat decomposition (all default-off,
byte-identical, lint-clean):

- **STAGE-111 `--ingest-backlog N`** (default 3 = the kIngestBacklog, byte-identical). The in-order ingest
  drain depth (main.cpp ~5664). **MEASURED net-neutral:** backlog 3→1 cuts freshage 41→31 ms BUT explodes
  span2+ 22%→50% (the STAGE-36 walking tremble) → net D unchanged (53 ms). It TRADES freshage for span, not a
  free reclaim. flow-scale 2 (F-cycle 8.8→5.6 ms) did NOT reduce combat freshage (41.7→40.1) → **F-compute is
  NOT the freshage bottleneck** (the decisive refutation). Both stay default; neither is the floor lever.

- **STAGE-112 `--latency-trace`** (measurement-only, default-off). Per-stage latency EMAs, each computed within
  ONE clock (no cross-clock subtraction); compose via a guarded QPC↔SystemRelativeTime delta (→0 on epoch
  mismatch). Emits `[lat-trace] INVISIBLE(compose,copy) | freshage=[pickup(conv) build detect] | game→screen`.

### The COMBAT decomposition (MEASURED 2026-06-17, BF6 99% 4090, src ~82, median of 8 samples)

```
INVISIBLE (pre-tcap):  compose 0.8ms   copy 5.6ms          (perceived, NOT in our lat/freshage)
freshage ~41ms:        pickup 15ms (convert 3.2) + build 20ms + detect 5ms
present-side ~32ms:    = lat(73-84) − freshage(41)          (async double-buffer + the deliberate D lead)
```

**The decisive finding — the F per-pair `build` is 20 ms but its COMPUTE (fsub flow 4.7 + cpu 4.1 = 8.7 ms) is
less than half of it.** F runs at ~50 pairs/s (20 ms/pair) when its compute rate would be ~115/s. The ~11 ms
gap is NON-compute, UPSTREAM of the fsub window = the B-side ingest (the hRP_b→device PCIe copy + the B unpack
+ B-queue contention on the 1080 Ti — flow + gme-gpu + unpack share B). **This is why flow-scale was null:** it
cut the 8.7 ms compute, not the ~11 ms B-side gap. **The #1 reclaimable, quality-free floor lever = collapse the
F per-pair build gap** (pre-stage the B-copy off the F iteration / reduce B-queue contention / pipeline F) →
build 20→~9 ms AND F catches the source → the pickup backlog shrinks too (double win, no flow-quality cost).

Card-deck verdicts (workflow w8jfwl9ec, 5 stage-mappers): **DEAD** (measured null/blocked) — `compose` (WGC
delivery is 0.8 ms, NOT the invisible suspect), MinUpdateInterval (anti-flood floor), `--shallow-queue`
(combat-null), `--low-d` (null), `--ingest-backlog` alone (net-neutral), GPU-priority (dogma), present
depth-reduction (already minimal), present-model swap (loses WDA). **LIVE** (pending the sub-split measure) —
the F-pair build-gap collapse (≈11 ms, the prize), the WGC `copy` separate-device (≈5.6 ms), convert-async
(≈3.2 ms). NEXT: instrument the F-pair sub-split (upload/unpack/B-wait) to partition the 11 ms, then play the
exact card.
