# FG_REARCHITECTURE_MASTER_PLAN — the present-model + routing re-architecture of PhyriadFG

> **Diátaxis type:** Explanation + plan (a design master-plan).
> **Status buckets:** `measured` (first-hand number, this session, BF6 combat),
> `designed` (specified, not built), `assumed` (model/estimate, flagged).
> **Origin:** two operator architectural insights (2026-06-17, validated against measurement
> + the gpu pillar canon) that together re-orient PhyriadFG's present model and GPU routing.
> Supersedes the framing of `WARP_OFFLOAD_MASTER_PLAN.md` (#2): B is no longer a candidate
> *principal* warp host — it becomes a *droppable supplement* (the corrected role).
> **Verification:** `file:line` confirmed first-hand against the working tree (incl. uncommitted
> STAGE-98..102) unless tagged `[map]` (from the `warp-offload-map` workflow — re-confirm before citing).

---

## §0 — The two insights (and why they are correct)

**Insight 1 — the latency is the pair-anchor, not the present block (MEASURED).**
BF6-combat `--phaselog`, 797 pairs: the present's structural delay is the **D-anchor ≈ 68 ms
steady** (spikes to 117-129 ms), of a total AddedLat ≈ 108 ms. *(measured, this session.)*
D is the pair-publish lag — the present waits D so the interpolation pair is ready. A **real
frame does not need a pair** → presenting the freshest captured real bypasses D → real-frame
latency ≈ the residual **~40 ms**, and the **spikes vanish** (the D-spikes are pairs the F-thread
stalled on; reals never wait for them). The "injugable" worst-case is the anchor, and it is on
the *interpolation* path, not the *real* path.

**Insight 2 — the principal FG must run on the STRONGEST GPU; the assistant is a supplement,
never the critical path (operator, aligns with the gpu canon).**
PhyriadFG today puts the **principal optical flow on B (the GTX 1080 Ti)** — the *weak* GPU — and
the FG *depends* on B (the F-thread feeds B; the warp on A consumes B's MV/SAD). So when B / the
F-thread can't keep up (combat: `cons` ≈ half of `arr`), the **whole FG collapses** (multiplier
1.07×). The weak GPU is on the critical path. **That is backwards.** Phyriad's own gpu pillar says
the opposite: `select_participants` (`framework/holons/.../RoutingPolicy.hpp` `[map]`) engages the
**supervisor = argmax(compute_gflops) ALWAYS**, and assistants **only if `break_even_decide` says
they help**. The 4090 is the supervisor; B is an assistant. The principal FG belongs on the 4090;
B should *supplement* (add frames with its own headroom, independently, droppably) and must
**never become the principal** — exactly the operator's correction.

These are not two plans. They are **one present model**: the 4090 owns the real fast-path AND the
principal interpolation; B contributes extra interpolated frames off the critical path.

---

## §1 — The target architecture: a one-directional interpolation CASCADE

The assistants do NOT generate independent frames "between reals" (which would compete with the
principal's frames and need cross-coordination). They **stack on the principal's output** — each
assistant takes the stream the previous stage already produced and interpolates ON TOP of it. This
is **PhyriadFG fed into PhyriadFG** — the in-process form of the operator-observed "LSFG-on-PhyriadFG"
stacking, but with the second stage moved onto the IDLE assistants instead of contending on the 4090
(which is exactly why the external LSFG-on-4090 stack degraded us: slip 2-7 ms / +lat / +frz from
4090 contention — `[project record]`). The dependency is strictly one-directional: stage N+1 consumes
stage N's output; the principal never depends on any assistant.

```
                    ┌──────────── 4090 (A) — supervisor / STAGE-0 + STAGE-1 ────────────┐
 BF6 ─capture(WGC)─►│  STAGE-0 REAL FAST-PATH: freshest real ──► present (D≈0, ~40ms)     │
                    │  STAGE-1 PRINCIPAL INTERP: flow+warp ──► interpolated frames (pair-D)│──┐
                    └──────────────────────────────────────────────────────────────────────┘  │ the principal output stream
                                                                                                │ (reals + stage-1 interp)
              ┌──────────── 1080 Ti (B) — STAGE-2 (cascade) ───────────┐                        │
              │  consumes the stage-1 stream, interpolates ON TOP ─────►│◄───────────────────────┘
              │  (small motion → crossfade-class smoothing) ──► more frames ──► present  (droppable) │
              └──────────────────────────────────────────────────────────────────────────────────┘
              ┌──────────── iGPU (G) — STAGE-3 (cascade, best-fit) ────┐
              │  zero-copy sysRAM; consumes the previous stage, stacks again (droppable) │──► present
              └──────────────────────────────────────────────────────────────────────────┘
```

- **Stage-0 real fast-path (A):** present the freshest captured real ASAP — no pair wait, no warp.
  The responsiveness path; ~40 ms (Insight 1). The user's hand-eye latency is THIS, and it is NEVER
  routed through the cascade (reals are not "buried" in stage-2 re-interpolation).
- **Stage-1 principal interp (A):** the 4090 runs flow+warp itself, interpolated frames between
  reals. Pays the pair-D (~68 ms) — *additive smoothness*, not responsiveness. On A because A is the
  supervisor (most capable) and local = no cross-device hop for the principal.
- **Stage-2…N assistant cascade (B, iGPU):** each assistant consumes the previous stage's output
  stream and interpolates ON TOP, adding more frames. Because the input is already dense, the motion
  per pair is small → the work degenerates to **crossfade-class smoothing** (the observed
  LSFG-on-our-output behaviour) — so a stage can run a LIGHTER pipeline (no heavy object/matte/gme;
  small-motion warp/crossfade only). **Droppable + one-directional:** an assistant slow/absent →
  fewer cascade frames, the principal stream still flows. The FG is fully functional on the 4090
  alone (single-GPU = stage-0 + stage-1 only → today's PhyriadFG). **This is the multi-GPU thesis done
  correctly:** assistants earn their keep by stacking, never on the critical path.

**Compatibility-with-all-systems (the operator's requirement):** the cascade depth = the number of
engaged assistants. 1 GPU → stages 0-1. +1080 Ti → +stage-2. +iGPU → +stage-3. The iGPU is the
best-fit cascade host (zero-copy sysRAM, 73 GB/s, no PCIe write-back — the cheap transfer a cascade
stage wants).

### §1.1 — The usefulness discriminator (why "more latency, better feel" — and how we earn it)

The operator's observed paradox: external LSFG-on-our-output RAISED raw latency yet FELT better. The
resolution: **LSFG discarded the non-useful frames** (duplicates, stale) and presented only frames
that ADVANCE the motion — so the eye sees continuous meaningful motion, no "dead" repeated frames
breaking it → it feels smoother/more responsive *despite* higher input-to-photon latency. This is a
**perceptual** win, not a raw-latency win (and that is exactly the goal — sensation over the number).

**The mechanism (we can do it in-process with our holonic telemetry):** a per-tick **usefulness
discriminator** over the cascade's available candidate frames — present the freshest candidate that
*advances* the motion; **discard** a candidate that is a **duplicate** (`N+1 ≈ N`, no new
information) or **stale** (for a moment already passed). The signals already exist per-frame:
`uniq` (the uniqueness proxy), `frz` (re-show/duplicate count), `d_pixel` (the source-disagreement
signal STAGE-97 ts-smooth used), and the motion delta. Exact-duplicate (`N+1 == N`) is a cheap,
reliable drop; the borderline "barely-advancing" case needs a calibrated threshold.

**The threshold is GLOBAL-MOTION-ADAPTIVE, not a constant (operator, SOTA-aligned).** A raw
frame-difference is confounded by motion magnitude — in high motion every frame "looks very
different" (→ always useful) and in a still scene every frame "looks the same" (→ always duplicate);
both wrong. The fix (the operator's "average per the whole system's motion") is to **NORMALIZE the
per-frame usefulness signal by the GLOBAL motion** the holonic telemetry already carries: the `gme`
affine model (scene global motion), the mean flow magnitude, `gme_dis` (disocclusion). So the keep/drop
threshold scales with the scene:
- **Quiet scene → low threshold → keep the subtle-motion frames.** The eye notices micro-judder MOST
  when the scene is still, so subtle advances matter more there — this is exactly why LSFG-on-our-output
  took MORE of our frames when the scene was quiet (the operator's observation).
- **High motion → threshold normalized against the large motion** → only meaningfully-advancing frames
  count, and artifact-laden frames (the gravity/disocclusion) fall to the discriminator.
This is `FG_VFI_PRIOR_ART`'s point (naive frame-difference is useless without motion-normalization).
It is a whole-FRAME decision (global signal is the right granularity); per-REGION usefulness is the
separate, complementary spatial axis of STAGE-98 `--multicand`.

**Why this is the PRINCIPLED form of STAGE-102's crude drop (measured):** STAGE-102 `--async-present`
drops whatever interpolated frame is *late* — blindly — and that **doubled stutter (3.4 → 8.25 %)**
in the BF6 test. The discriminator drops by *uselessness*, not by *lateness* → it should REDUCE
stutter rather than add it. It is also the TEMPORAL analogue of STAGE-98 `--multicand` (which selects
among SPATIAL candidates of one frame); this selects among TEMPORAL candidates across cascade stages.

**Honest bound:** the threshold is the whole risk — too aggressive drops useful frames (judder, the
STAGE-102 failure mode); too lax shows duplicates (the waste LSFG avoids). `stutter_time_pct` + the
operator's eye are the guardrail. The discriminator improves FEEL and useful-frame density; it does
not lower the input-to-photon floor (the real fast-path, §1 stage-0, is what carries responsiveness).

---

## §2 — Why this fixes what we measured

| Combat symptom (measured) | Cause | This architecture |
|---|---|---|
| AddedLat ~108 ms + spikes ("injugable") | the pair-D anchor (~68 ms) on the present path | real fast-path bypasses D → ~40 ms, no spikes |
| fg_multiplier collapse to 1.07× | the principal flow is on weak B; F-thread can't feed it → consumes half the pairs | principal FG on the 4090 (capable); B only supplements → no critical-path bottleneck |
| 1080 Ti 23% "idle" in combat | CPU-starved feeding B (B on critical path, F-thread the funnel) | B pulls work opportunistically; not fed through a critical funnel |
| warp-offload (#2) was net-negative | routing the *critical* warp to slow B + per-tick transfer | B is no longer critical → slow B is acceptable → #2's premise rescued as a *supplement* |

---

## §3 — Design contract (normative, BCP 14)

- **D1 — Responsiveness = the real fast-path (MUST).** Real frames present at the freshest-available
  latency, never gated on the interpolation pair-D. This is the metric the user feels.
- **D2 — Supervisor = strongest, always (MUST).** The principal FG runs on the most-capable engaged
  GPU (the 4090) per the gpu pillar's `select_participants`/`break_even_decide` — NOT a hardcoded
  weak-GPU assignment. Replace the "flow always on B" hardcode with the capability decision.
- **D3 — The assistant is droppable (MUST).** B's contribution is opportunistic and non-blocking:
  inserted if ready, dropped if late. **A run with B absent or failing MUST still produce a fully
  functional FG** (degraded smoothness only). B MUST NEVER be on the critical path.
- **D4 — Make-room over offload when local wins (SHOULD).** Even with the 4090 saturated, running the
  FG locally (scheduling a slice / the load-governor shed) may beat offloading to slow B in ms — the
  decision is `break_even_decide` on MEASURED costs, not a static rule. Measure; don't assume.
- **D5 — Default-off, incremental, byte-identical-off per flag (MUST).** Ship as opt-in flags that
  compose; each byte-identical when off (the STAGE-97/101/102 discipline). No big-bang rewrite.
- **D6 — Honest telemetry (SHOULD).** The `--csv` exposes per-frame: which path (real-fast / A-interp
  / B-supplement), the real-frame latency vs the interp latency, B's contributed/dropped counts.
- **D7 — Discriminate by uselessness, never by lateness (MUST, §1.1).** A cascade candidate is dropped
  only when it is a DUPLICATE (`N+1 ≈ N`) or STALE (past its present moment) — judged from the holonic
  per-frame signals (`uniq`/`frz`/`d_pixel`/motion-delta) — NOT merely because it arrived late (the
  STAGE-102 crude-drop that doubled stutter). `stutter_time_pct` is the calibration guardrail: a
  discriminator tuning that raises stutter over the no-discriminator baseline is mis-tuned. The
  discriminator improves FEEL + useful-frame density; responsiveness is carried by the stage-0 real
  fast-path, not by the discriminator.

---

## §4 — The hard parts (honest)

1. **Mixed present cadence.** Interleaving a low-latency real path with higher-latency interpolated
   frames (from A and B) on one panel clock is non-trivial pacing: a real and an interpolated frame
   must not fight for the same tick; the cadence (STAGE-93 sc-select, the content-clock NCO) must
   schedule reals + A-interp + B-supplement coherently. This is the core build risk.
2. **The inherent interpolation hold.** To INSERT an interpolated frame between real-N and real-N+1
   you must hold N+1 briefly — so "reals at zero latency AND interpolation" is bounded: the real path
   is ~40 ms, but enabling interpolation adds a small hold. Removing even that needs EXTRAPOLATION
   (ASW: predict past the last real, no hold), a separate arc with its own artifacts.
3. **The ~40 ms floor.** Insight 1 bounds the real-fast-path at ~40 ms (the residual capture+present),
   not ~15 ms. Going lower needs a leaner present path and hits the WGC-capture inherent floor
   (unmeasured — `freshage` would pin it; a 1-run probe if we want the exact number).
4. **B as a true independent supplement** is new machinery: B generates interpolated frames on its own
   timeline, transfers them A-ward (host-bounce, no P2P — `[map]`), and the present inserts the ones
   that arrive in time. The transfer + insertion-timing is the build cost; but since B is droppable,
   *correctness* never depends on it (D3) — which is what makes it safe.
5. **The 4090-principal-FG cost is unmeasured.** Whether the 4090 can carry capture + present + real
   fast-path + principal flow + warp within budget (even "making room") is the open question. There
   is a partial probe (§5).

---

## §5 — Phases and validation gates

| Phase | Deliverable | Gate |
|---|---|---|
| P0 | This plan | operator review |
| P1 — **probe** | Measure the 4090-principal-FG cost: run `--fg-gpu primary` (needs igpu-convert off so `pfg_enabled` stays true — verify the combo works with WAP first; line 3408 disables pfg under igpu-convert). Compare combat `lat`/multiplier/4090-util vs the B-flow default. | a clean combat run; the number decides whether 4090-principal is viable as-is or needs the load-governor to "make room" |
| P2 — real fast-path | Present the freshest captured real on a low-D path (bypass the pair-anchor for reals); `--csv` shows real-frame lat. | real-frame lat ≈ ~40 ms measured; byte-identical off |
| P3 — principal interp on A | The 4090 runs flow+warp (capability-routed default = supervisor), interpolated frames inserted between reals. | multiplier holds without depending on B; build green; byte-identical off |
| P4 — B as droppable supplement | B generates extra interp frames independently; inserted-if-ready / dropped-if-late; **B-absent still works**. | pull B's cable mid-run → FG keeps working (D3); B adds measured frames when present |
| P5 — operator combat test | the composed flags, one-by-one + combined | operator eye + `--csv`: real-frame latency playable (~40 ms, no spikes) + smoothness from the supplements |

Validation is the operator's runtime (the present is WDA-excluded). The build's gate per phase:
build green + byte-identical-off (verified first-hand, §2.3 harder for delegated work) + the
measured number.

---

## §6 — Relationship to prior work

- **Rescues #2 (`WARP_OFFLOAD_MASTER_PLAN`):** that plan's verdict (routing the *critical* warp to B
  is break-even-negative) STANDS. This plan changes B's role from *principal* to *droppable
  supplement* — off the critical path, so B-slow is acceptable. The negative was about criticality,
  not about B contributing at all.
- **Keeps #1 (`--async-present`, STAGE-102, fixed this session):** the non-blocking present is a
  building block of the real fast-path (present without blocking on a warp fence). Keep it.
- **Reframes #3 (`--load-governor`):** it becomes the "make room" lever (D4) — shed work so the
  4090-principal FG fits, rather than a standalone multiplier fix.
- **Aligns with the gpu pillar canon** (`break_even_decide`/`select_participants`, supervisor=strongest)
  — this plan brings the render_assistant's routing back into compliance with `CANON.md`'s gpu pillar.

---

## §7 — Recommendation

Build incrementally P1→P5, holding D1–D6. Start with **P1 (the `--fg-gpu primary` probe)** because it
is cheap (an existing flag + a config tweak + one combat run) and it answers the load-bearing open
question (§4.5): can the 4090 carry the principal FG? That number decides whether the principal-on-A
design is viable as-is or needs the load-governor to "make room." Everything after P1 is real
re-architecture (the present model), justified by the measured ~108→~40 ms real-frame latency win and
the elimination of the weak-GPU critical-path collapse.
