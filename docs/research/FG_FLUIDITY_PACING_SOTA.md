# FG_FLUIDITY_PACING_SOTA — how the temporal fluidity / frame-pacing of frame generation is measured rigorously (the "escalonado" axis)

> **Diátaxis type:** Analysis (SOTA dossier). Single declared type, held throughout —
> this is not a how-to and not the testbench plan itself.
> **Status:** `measured` for the cited external literature (vendor/academic primary
> docs + tool changelogs, adversarially verified in the source workflow) · `designed`
> for how the Phyriad testbench *applies* them (the temporal-fluidity axis is **not
> built**). Calling either the other would be a reporting defect (FDP §4.2).
>
> **Scope.** The *temporal* quality of frame generation: **where in time** a
> generated frame is placed (interpolation phase / cadence) and **how uniformly**
> real and generated frames are paced — the motion-smoothness / "stepping" /
> "escalonado" axis the operator put as the live FG frontier. Two questions: how the
> field MEASURES this rigorously, and what makes production FG fluid (the fix
> direction). It is the **temporal** sibling of the spatial/quality measurement
> dossier; it does not restate the PhyriadFG roadmap.
>
> **Reconcile-not-duplicate (FDP §4.6).** [`FG_VFI_MEASUREMENT_SOTA.md`](FG_VFI_MEASUREMENT_SOTA.md)
> §5 (its measurement-concern **M4**, throughput + pacing) already introduces the
> Animation-Error formula, the "cannot score fake frames" limitation, and the
> present-vs-display rule **at a summary level**, as one of seven measurement
> concerns. **This dossier is the dedicated, deeper treatment of that temporal
> axis** — it develops the KEY UNLOCK (§2) that M4 only gestured at, ties the axis to
> the perceptual eye dossier and the camera axis, and states the fluidity fix
> direction. Where the formula and the display-time rule appear below, they are the
> load-bearing objects of *this* analysis; M4 is the prior summary mention, linked.
> The perceptual/eye half (BT.500 / 2AFC / JOD / MPRT-as-perceived) lives in
> [`FG_PERCEPTUAL_EVAL_SOTA.md`](FG_PERCEPTUAL_EVAL_SOTA.md) and is cited, not re-derived.
>
> **Provenance.** External facts are from deep-research workflow `w9xsjredn`
> (98 agents; 5 angles → 16 sources → 69 claims → 25 adversarially verified,
> 21 confirmed / 4 killed, 3-vote). The research agents fetched and verified those
> URLs first-hand; **the author did NOT independently re-fetch every external URL in
> this session** — they carry that sweep's verification levels (§7), per FDP §2.4.
> In-repo anchors (the testbench components, the vista fixed points,
> `render_assistant/src/main.cpp` present sites) were read first-hand this session.
> No fabricated citation, number, path, or quotation.
>
> **Normativity (BCP 14, [RFC 2119](https://www.rfc-editor.org/rfc/rfc2119.html) /
> [RFC 8174](https://www.rfc-editor.org/rfc/rfc8174.html)):** MUST / SHOULD / MAY
> appear in capitals only where a real constraint binds (chiefly §3's display-time
> rule). The vendor sources use advisory prose; do not read their "should" as our MUST.

---

## §0 — Scope, and the ID-namespace caution (read first)

**In scope.** The two orthogonal *temporal* measurement axes (§1), the deterministic-source
unlock that makes generated-frame placement scorable (§2), the display-time
measurement rule (§3), the motion-trajectory / pursuit-camera method and its
relation to MPRT (§4), what production FG does to be fluid and where Phyriad's
cadence stands against it (§5), the designed Phyriad mapping (§6), and honest gaps +
refuted claims (§7).

**Out of scope.** Spatial/interpolation *image* quality (ghosting, disocclusion,
the warp-vs-blend cure family) → [`FG_VFI_PRIOR_ART.md`](FG_VFI_PRIOR_ART.md) and
[`FG_VFI_MEASUREMENT_SOTA.md`](FG_VFI_MEASUREMENT_SOTA.md). Input latency → the same
measurement dossier (§4 M3) + [`FG_CADENCE_LATENCY_PRIOR_ART.md`](FG_CADENCE_LATENCY_PRIOR_ART.md).
The subjective-assessment standards (ITU, 2AFC, JOD) → [`FG_PERCEPTUAL_EVAL_SOTA.md`](FG_PERCEPTUAL_EVAL_SOTA.md).

> **★ Namespace caution (THREE distinct `C`-schemes — do not conflate).** This
> dossier ties its designed application to a **new testbench build component this
> dossier proposes, the temporal-fluidity axis, referred to as `TB-C9` (the software
> content-clock placement+pacing metric).** The master plan's *canonical* build
> components are `TB-C1..TB-C8` in
> [`FG_TESTBENCH_MASTER_PLAN.md`](FG_TESTBENCH_MASTER_PLAN.md) §2 (SourceWindow …
> scorer); **`TB-C9` is introduced here as a proposed extension, not an existing
> component** — when it is lifted into the plan it MUST be added there with that ID.
> It is DISTINCT from the **closeable fixed points `C1..C11`** in
> [`PHYRIADFG_OBJECTIVE_VISTA.md`](PHYRIADFG_OBJECTIVE_VISTA.md) §2 (objectives — the
> relevant one is `C6` "pacing jitter < X ms on the panel clock", and `C11` "controlled
> testbench A/B reproducible"); do not read `TB-C9` as a fixed point. It is also
> DISTINCT from the *measurement concerns* `M1..M7` in `FG_VFI_MEASUREMENT_SOTA.md`.
> Read every `TB-C<n>` as a testbench build component, every bare `C<n>` as a vista
> fixed point.

---

## §1 — The TWO orthogonal axes the field separates

The single most important structural fact: **temporal quality is two orthogonal
axes, measured by different instruments**, and conflating them is the master error
in FG analysis.

### (a) Temporal PLACEMENT — "Animation Error"

The de-facto standard is Intel/PresentMon's **Animation Error** (also "simulation
time error"), popularized by GamersNexus. Its formula, verbatim from the GN
methodology white paper `[V3]`:

```
MsAnimationError = (AnimationTime_N − AnimationTime_N-1) − MsBetweenDisplayChange_N
```

i.e. *how much a displayed frame's intended animation-time advance mismatches the
interval it is actually shown for* — GN: *"Animation error is the difference between
the pacing of animation and display,"* a measure of *"a mismatching of game
animation / the game state and the frame being presented... effectively out of sync
with the visual."* It is **orthogonal to frametime**: GN, *"Whereas frametime
measures variations in the delivery interval for each frame, simulation time error
directly measures errors or a mismatching... despite a potentially good frametime
itself."* Corroborated first-hand by the Intel PresentMon README `[V1]`, which
defines `MsAnimationError` as *"the difference between the previous frame's CPU delta
and display delta."* (This formula is also carried in
[`FG_VFI_MEASUREMENT_SOTA.md` §5](FG_VFI_MEASUREMENT_SOTA.md) as the M4 summary.)

**`AnimationTime` is an engine-side simulation timestamp.** PresentMon v2.3.0 added
it as a first-class metric — verbatim `[V1]`: *"Added Animation Time metric which is
the time the CPU started animation work on a frame."* It is normally sourced from
explicit vendor APIs (NVIDIA Reflex / Intel XeLL `SimStart` events) or approximated
by the frame's `CPUStart` (Intel's Tom Petersen, via GN: *"Today people are mostly
using CPUStart as the AnimationTime, which is a pretty good proxy"*).

**WHY it cannot score generated frames.** The reason is specific and uncontested
(verified 3-0): a generated frame **carries no engine animation timestamp**, so the
formula's `AnimationTime_N` half has no reference point. Tom Petersen, via GN: *"there's
no animation time for fake frames, so there's no reference point for calculating
error."* Animation Error is therefore, today, a real-frame-only metric — the field's
standard temporal-placement instrument is structurally blind to exactly the frames
an FG tool produces.

### (b) Present-interval PACING uniformity

The second axis is whether real and generated frames are each shown for **equal
display intervals** — the variance of `MsBetweenDisplayChange`. This half **does**
apply to generated frames: a generated frame has a display interval even though it
has no animation time. So an uninstrumented external-capture FG can score
*display-interval uniformity* (jitter / 1%/0.1% lows on the display clock), but not
full Animation Error. These two axes are independent: a cadence can be perfectly
metronomic (axis b clean) yet land each frame at the wrong moment in time (axis a
wrong), and vice-versa.

---

## §2 — THE KEY UNLOCK: a deterministic source supplies the missing animation-time

This is the load-bearing contribution of the dossier and the reason Phyriad can
score generated-frame fluidity at all.

Animation Error is blind to generated frames **only because no animation-time
reference exists for them**. Remove that gap and the metric works. A **deterministic
synthetic source with an analytic `pos(t)`** is exactly such a reference: the source
*knows* the intended scene state at every instant, including the instants between two
real frames. Feeding that analytic time in as the `AnimationTime` makes the
per-generated-frame temporal-placement error **directly computable** — the metric the
field says is otherwise impossible for "fake" frames becomes available for every
displayed frame, real or generated.

The deductive step (a known analytic `pos(t)` IS a ground-truth animation-time) is
near-tautological and was verified 3-0; it is the direct affirmative answer to the
research question's core hypothesis. It rests on the same construction the VFI field
uses for ground truth: only a deterministic source can guarantee the analytic
trajectory, because for natural video *"given two frames one can only reasonably
expect the motion to be linear. Any other assumption would require an oracle when
synthesizing the in-between frames"* — *"any model other than a linear one would be
underdetermined and hence yield an infinite number of solutions"* (Kiefhaber et al.,
arXiv:2403.17128, `[V1]`). Phyriad's synthetic source escapes the oracle requirement
by construction: it emits `pos(t)` at the exact present instant.

**Concrete metric.** For each displayed frame (real or generated) presented at actual
display time `t_disp`:

- **Placement error** = (the object's displayed position) − `pos(t_disp)` — the
  trajectory deviation (§4 gives the displayed-position read-back), OR, in the
  PresentMon idiom, the analytic-time advance the frame *should* represent minus the
  display interval it was shown for: the synthetic `pos(t)`'s implied animation-time
  feeding the Animation-Error formula.
- **Pacing uniformity** = the variance of `MsBetweenDisplayChange` across the run.

Both are computable for generated frames precisely because `pos(t)` exists.

**Tie to the testbench (`designed`).** This is **`TB-C9` — a software *content-clock*
placement+pacing metric** (the new temporal-fluidity axis; see the §0 namespace
caution). It is the temporal companion of the existing objective scorer
[`TB-C8`](FG_TESTBENCH_MASTER_PLAN.md) (which scores *spatial* fidelity against
held-out truth) and it consumes the same live-GT plumbing the plan already designs:
[`TB-C7`](FG_TESTBENCH_MASTER_PLAN.md) (FrameID alignment) tags each source frame so
each live present — even under non-deterministic drops — can be aligned to the
emittable analytic instant, and `TB-C9` reads `pos(t)` for that instant to compute the
placement error. It scores **both** real and generated frames on one axis, which the
standard Animation-Error metric cannot. `TB-C9` is the apparatus behind vista fixed
point `C6` ("pacing jitter < X ms on the panel clock").

---

## §3 — The methodological RULE: measure on DISPLAY-time, not present-time

This is the one place a real rule binds.

> **Pacing MUST be measured on display-time (`MsBetweenDisplayChange`), NOT
> present-time (`MsBetweenPresents`).** An FG tool paces frames *after* the game's
> `Present()` call, so present-based frame times do not represent the cadence
> actually delivered to the panel; present-based 1%/0.1% lows are misleadingly
> terrible for a correctly-paced FG.

The two metrics measure different events: `MsBetweenPresents` = time between
application `Present()` calls (submission cadence); `MsBetweenDisplayChange` = time
between actual displayed image changes (what the player sees) — wccftech `[V3]`:
*"frame pacing can happen after a game's Present() calls, which means that traditional
present-based frame times may not accurately represent the cadence... delivered to the
monitor."* The decisive **primary** corroboration is NVIDIA's own Streamline DLSS-G
Programming Guide `[V1]`: *"MsBetweenPresents is not suitable for measuring frame
pacing quality because DLSS-G uses specialized hardware to delay the image after
Present() has been called,"* recommending `MsBetweenDisplayChange` (NVIDIA FrameView
16.1 switched to it). PresentMon's **`FrameType`** metric labels each presented frame
as application (real) or generated, and (v2.3.0, `[V1]`) the Displayed-FPS metric now
counts both — so the display-clock cadence can be split by frame type.

This rule binds Phyriad directly: its present path is a DComp overlay that paces after
the captured game's own `Present()`, the exact regime the rule addresses. Any pacing
number reported from the present clock instead of the display clock is unsound.

> **Caveat (carry, do not over-read "photon").** `MsBetweenDisplayChange` measures the
> display-FLIP event, not true scanout/photon emission. It is the right *software*
> clock; true photon-time needs the camera axis (§4).

---

## §4 — The MOTION-TRAJECTORY method (and its relation to MPRT + the pursuit camera)

The rigorous *perceptual* measure of stepping/judder is the **displayed motion
trajectory vs the ideal analytic trajectory at the actual display times**: read the
object's position off each displayed frame (software read-back or a high-speed
camera), plot it against display time, and compare to `pos(t_disp)`. A smooth ramp =
fluid; a staircase = "escalonado." This is the same datum as §2's placement error,
expressed as a trajectory the eye can be shown.

The instrument that captures it *as the eye perceives it* is the **Blur Busters
pursuit camera** `[V1]`: a camera physically tracks the on-screen motion at the
object's speed, so the captured streak is the displayed trajectory the eye (which
tracks moving objects via smooth pursuit) actually integrates — *"camera-tracking
blur equals eye-tracking blur,"* *"extremely accurate at measuring motion blur & other
artifacts, including ghosting and overdrive."* The technique is validated by a
peer-reviewed paper (Rejhon et al., arXiv:1602.07573, cited in the sweep's finding
evidence) and adopted industry-wide (RTINGS, TFTCentral, HDTVtest).

**Relation to MPRT — and the honest boundary.** MPRT (Moving Picture Response Time)
quantifies the *persistence* component: on a sample-and-hold display with GtG zeroed,
*"MPRT is... frametime on a sample & hold display"* (Blur Busters GtG-vs-MPRT, `[V1]`)
— linking frametime to perceived smear. **But MPRT does NOT discriminate FG cadence
quality**: at equal frametime, persistence blur is identical for real and generated
frames, so MPRT alone cannot score stepping/phase error (this is why the sweep rated
the MPRT sub-claim *medium*, a 2-1 split). The pursuit camera captures the displayed
*trajectory* (where stepping is visible); MPRT is the persistence sub-component, not
the cadence-error metric. **Do not use MPRT alone to score FG stepping.**

**Tie to the testbench (`designed`).** This is the camera axis already in the plan —
[`TB-C6` CameraKit](FG_TESTBENCH_MASTER_PLAN.md) (the photonic axis): an external
high-speed camera films the panel and reads the on-screen frame-ID markers to recover
the **true** present cadence / duplicates / stepping independent of our own clocks.
It is the **photon-truth validation** of the software content-clock metric (`TB-C9`,
§2): the camera and the software metric measure the *same* present stream by two
independent clocks, and their agreement is the cross-check that catches the
cross-clock present-timestamp errors the plan names in risk `TB-5`. The camera is also
the only axis cross-comparable against LSFG (whose overlay is WGC-uncapturable).

---

## §5 — WHAT MAKES FG FLUID: a uniform display metronome + correct phase (the fix direction)

Production FG converges on **one** smoothness mechanism: a display metronome that
targets *equal per-frame display intervals* for both real and generated frames. This
is `designed` for Phyriad (the direction we aim at), `measured` as a documented fact
of the named systems.

- **FSR3 Frame Generation — a two-thread high-priority pacer** (AMD GPUOpen FSR SDK,
  `[V1]`, primary): *"Presentation and pacing are done using two additional CPU
  threads separate from the main render loop"*; *"A high-priority pacing thread keeps
  track of average frame time, including UI composition time, and calculates the
  target presentation time delta"*; *"The goal is to display each frame for an equal
  amount of time"*; *"A present thread... waits until the calculated present time delta
  has passed since the last presentation, then presents the generated frame. It repeats
  this for the real frame."* — i.e. **both** frame types are gated to the metronome.
- **DLSS Frame Generation — "even pacing"** (NVIDIA DLSS4 article, `[V1]`, primary):
  *"Once the new frames are generated, they are evenly paced to deliver a smooth
  experience."*
- **Blackwell — hardware Flip Metering** (same source): *"Blackwell uses hardware Flip
  Metering, which shifts the frame pacing logic to the display engine, enabling the GPU
  to more precisely manage display timing"* — the metronome moved out of the CPU into
  the display engine for tighter timing.
- **DLSS 3 FG — two-frame backward interpolation** (NVIDIA DLSS3 article, `[V1]`,
  primary): the generated frame is an *intermediate* placed temporally **between** two
  existing rendered frames (not extrapolated forward), from four inputs — *"current and
  prior game frames, an optical flow field generated by Ada's Optical Flow Accelerator,
  and game engine data such as motion vectors and depth."* This establishes the
  *correct temporal placement* of a 2× generated frame: at a fraction between the two
  reals (the geometric ideal being **t = 0.5**), then presented on the uniform display
  metronome.

> **Honest boundary on the phase axis.** The sources document **uniform display
> interval** as the mechanism; they **underspecify how much the temporal PHASE
> placement (the generated frame at exactly t=0.5) contributes versus interval
> uniformity alone** (sweep open question; §4's finding noted uniform interval is
> *necessary but not sufficient*). Treat "uniform metronome + t=0.5" as the designed
> target; whether a perfectly metronomic but phase-wrong cadence still reads as
> stepping is unresolved by the literature and is a thing `TB-C9` (§2) can measure.

**Phyriad's standing — state it plainly.** PhyriadFG's current cadence is a
**primitive escalonado: it presents a frame when the warp is ready, not to a uniform
display metronome.** There is no high-priority pacing thread computing a target
present-delta on the fixed display clock; the present loop runs against the warp
fence. That is exactly the structure the FSR3/DLSS pacers replace. The fix direction
is therefore concrete and already named in the in-repo plans: build the
**present-pacing thread + a moving-average display-interval target** (the
[`FG_CADENCE_LATENCY_PRIOR_ART.md`](FG_CADENCE_LATENCY_PRIOR_ART.md) verdict: the flat
ladder = *owning* present timing on the fixed DWM clock), and decouple present from
the generation fence (the `--async-present` work in the render-assistant arc;
[`PHYRIADFG_OBJECTIVE_VISTA.md`](PHYRIADFG_OBJECTIVE_VISTA.md) fixed point `C6` is
blocked on this "P0 DWM metering = 0 today"). VRR is NOT a lever for an external
overlay (the game's swapchain owns it; documented refuted in the cadence-latency
dossier) — the metronome is a *software* pacer on the DWM clock, as LSFG itself does.

---

## §6 — The designed Phyriad application (`designed`)

Mapping §1–§5 onto Phyriad's deterministic-source reality. Everything here is
`designed` — the temporal-fluidity axis is not built.

| Element | Designed form | Grounded in |
|---|---|---|
| **Placement metric** | feed the synthetic `pos(t)` as the `AnimationTime` → per-generated-frame Animation-Error / trajectory-deviation, scoring real **and** generated frames | §1(a), §2 |
| **Pacing metric** | variance / p99 of `MsBetweenDisplayChange`, split by `FrameType` | §1(b), §3 |
| **Measurement clock** | display-time (`MsBetweenDisplayChange`), never present-time — REQUIRED (§3) | §3 |
| **Software axis** | `TB-C9` content-clock metric, on `TB-C7` FrameID alignment + the `TB-C8` scorer plumbing | §2 |
| **Photon-truth axis** | `TB-C6` CameraKit pursuit/slow-mo of the panel — independent clock, the cross-check + the LSFG-comparable axis | §4 |
| **Source** | the deterministic `TB-C1` SourceWindow (it already emits analytic `pos(t)`); known sweep velocity + set res/fps so phase and interval are dialed | §2, master plan |
| **Fix under test** | a present-pacing thread (uniform DWM metronome) + t≈0.5 placement + `--async-present` decouple; measured A/B on this axis | §5 |
| **Significance** | A/A null run returns zero; the perceptual confirmation is `TB-C5` 2AFC (the eye sets "enough", §7) | §4, perceptual dossier |

The axis scores both frame types, on the display clock, against analytic truth — the
combination the field says is otherwise unavailable for generated frames. It closes
the temporal half of vista fixed point `C11` (controlled testbench A/B reproducible)
and feeds `C6` (pacing jitter).

---

## §7 — Honest gaps + refuted claims

### Open gaps (FDP §2.4 / D6)

1. **LSFG / Lossless Scaling / THS internal pacing logic is PROPRIETARY.** No surveyed
   source documents how LSFG paces or times its generated frames — its present-timing,
   queue management, or any "frame pacing" feature. LSFG's smoother cadence vs an
   external-capture FG is *observed*, but its mechanism is **unverified**; treat any
   claim about HOW LSFG paces as unverified, and use the DLSS-G/FSR3 first-party
   mechanisms (§5) as the documented analogs (the same stance as
   [`FG_CADENCE_LATENCY_PRIOR_ART.md`](FG_CADENCE_LATENCY_PRIOR_ART.md)).
2. **No quantitative just-noticeable-difference (JND) threshold survived.** No source
   gives a ms-level threshold for perceptible frame-pacing irregularity, nor a
   degrees-of-phase threshold for temporal-placement error. There is no literature
   number to cite; **the operator's eye sets "enough"** (the `TB-C5` 2AFC harness, the
   perceptual dossier's single-observer-"informal" regime, locates *his own* JND on the
   deterministic scene empirically). Do not cite a JND figure as if one existed.
3. **The end-to-end unlock is sound but undemonstrated.** Whether feeding the
   synthetic `pos(t)` as `AnimationTime` reproduces PresentMon-style Animation Error
   for generated frames *in practice* — and how to align the synthetic clock to
   display-flip timestamps without cross-clock error — is logically sound (§2) but no
   source demonstrates the full harness. This is exactly what building `TB-C9` would
   establish first-hand.
4. **Phase vs interval-uniformity contribution underspecified** (§5 boundary): the
   sources establish uniform-interval as the mechanism but do not quantify how much
   correct t=0.5 phase adds beyond it.
5. **"Photon-time" is loose** throughout the literature: `MsBetweenDisplayChange` and
   the pursuit camera capture the display-FLIP event, not true scanout/photon emission;
   genuine photon-time needs a high-speed camera observing emitted light (the `TB-C6`
   axis).
6. **The author did not re-fetch the §8 URLs first-hand this session** — they carry the
   `w9xsjredn` levels, stated rather than re-attested (FDP §2.4).

### Refuted in the sweep — do NOT reintroduce

- **"CPU/software frame pacing is insufficient for FG because its timing variability
  *compounds* as more frames are inserted"** — killed **0-3** (claimed against the
  NVIDIA DLSS4 page). Do not assert software pacing is fundamentally inadequate or that
  jitter compounds with frame count.
- **"Blur Busters Law: 1 ms persistence = 1 px blur per 1000 px/s, a closed-form
  falsifiable conversion"** — killed **0-3**. Do not present it as a closed-form
  trajectory-error conversion.
- **"Frame-generation judder is driven by the beat between output fps and refresh; correct
  pacing only when FG maxes the refresh rate"** — killed **0-3**. Do not attribute
  stepping to an fps↔refresh beat pattern.
- **"With VSync off, FSR 3 FG is fundamentally broken / ~half the frames are not shown"**
  — killed **1-2**. The FSR3 launch pacing criticism is real but scoped to *launch*
  (FSR 3.1 / "Redstone" improved it); do not state the strong "half frames dropped"
  version.

---

## §8 — Sources (leveled per FDP §2.3)

External facts verified in workflow `w9xsjredn` (3-vote adversarial). `[V1]` =
primary, first-hand to the research agents; `[V3]` = reputable secondary. The author
did not independently re-fetch these this session (§7.6).

| # | Source | What it grounds | Level |
|---|---|---|---|
| 1 | GamersNexus — Animation Error methodology white paper · https://gamersnexus.net/gpus-gn-extras-cpus/problem-gpu-benchmarks-reality-vs-numbers-animation-error-methodology-white | Animation-Error formula; "no animation time for fake frames" (quoting Intel/Tom Petersen) | **[V3]** |
| 2 | GamersNexus — FPS Benchmarks Are Flawed (engineering discussion) · https://gamersnexus.net/gpus-cpus-deep-dive/fps-benchmarks-are-flawed-introducing-animation-error-engineering-discussion | Animation Error orthogonal to frametime | **[V3]** |
| 3 | Intel PresentMon (GameTechDev) · https://github.com/GameTechDev/PresentMon | `MsAnimationError` definition (CPU-delta vs display-delta) | **[V1]** |
| 4 | PresentMon v2.3.0 release · https://github.com/GameTechDev/PresentMon/releases/tag/v2.3.0 | "Animation Time" metric; `FrameType`; Displayed-FPS counts generated frames | **[V1]** |
| 5 | videocardz — PresentMon 2.3 / XeFG-XeLL-AFMF · https://videocardz.com/pixel/presentmon-2-3-update-focuses-on-intels-xefg-xell-and-amd-fluid-motion-frames | FG-tracking / FrameType context | **[V3]** |
| 6 | wccftech — Display Times, Frame Times & the Truth About Smoothness · https://wccftech.com/display-times-frame-times-and-the-truth-about-smoothness/ | `MsBetweenPresents` vs `MsBetweenDisplayChange`; FG paces after Present() | **[V3]** |
| 7 | NVIDIA Streamline (DLSS-G Programming Guide) · https://github.com/NVIDIA-RTX/Streamline | **primary** display-time rule: MsBetweenPresents unsuitable, use MsBetweenDisplayChange | **[V1]** |
| 8 | AMD GPUOpen — FSR SDK, Frame Interpolation Swapchain · https://gpuopen.com/manuals/fsr_sdk/techniques/frame-interpolation-swap-chain/ | two-thread high-priority pacer; "display each frame for an equal amount of time" | **[V1]** |
| 9 | NVIDIA — DLSS 4 Multi Frame Generation · https://www.nvidia.com/en-us/geforce/news/dlss4-multi-frame-generation-ai-innovations/ | "evenly paced"; Blackwell hardware Flip Metering | **[V1]** |
| 10 | NVIDIA — DLSS 3 · https://www.nvidia.com/en-us/geforce/news/dlss3-ai-powered-neural-graphics-innovations/ | DLSS3-FG = backward interpolation, intermediate frame between two reals; 4 inputs | **[V1]** |
| 11 | TechSpot (Hardware Unboxed) — AMD FSR 3 Tech · https://www.techspot.com/article/2747-amd-fsr-3-tech/ | DLSS3 paces correctly vs FSR3-at-launch (the launch-scoped caveat + refuted sub-claims) | **[V3]** |
| 12 | Blur Busters — Pursuit Camera · https://blurbusters.com/motion-tests/pursuit-camera/ | pursuit camera = displayed-trajectory capture; "camera = eye-tracking blur" | **[V1]** |
| 13 | Blur Busters — GtG vs MPRT FAQ · https://blurbusters.com/gtg-versus-mprt-frequently-asked-questions-about-display-pixel-response/ | MPRT = frametime on sample-and-hold (GtG zeroed); persistence ≠ cadence | **[V1]** |
| 14 | Rejhon et al. — pursuit-camera measurement (peer-reviewed) · https://arxiv.org/abs/1602.07573 | peer-reviewed validation of the pursuit-camera method (cited in the sweep's finding evidence) | **[V1]** |
| 15 | Kiefhaber/Niklaus/Liu/Schaub-Meyer 2024 · https://arxiv.org/html/2403.17128v1 | only a deterministic source guarantees analytic GT motion ("would require an oracle") | **[V1]** |

In-repo (read first-hand this session): [`FG_TESTBENCH_MASTER_PLAN.md`](FG_TESTBENCH_MASTER_PLAN.md)
(TB-C1..TB-C8, the three axes), [`PHYRIADFG_OBJECTIVE_VISTA.md`](PHYRIADFG_OBJECTIVE_VISTA.md)
(C1–C11 fixed points; C6 pacing), [`FG_VFI_MEASUREMENT_SOTA.md`](FG_VFI_MEASUREMENT_SOTA.md)
(the M4 summary this dossier deepens), `apps/render_assistant/src/main.cpp` (the
warp-fence present path = the primitive escalonado).

---

## §9 — Cross-links

- The seven measurement concerns this temporal axis is one of (M4 pacing summary) +
  the GT-source routes + the saturation generator → [`FG_VFI_MEASUREMENT_SOTA.md`](FG_VFI_MEASUREMENT_SOTA.md).
- The subjective/eye axis the JND gap defers to (BT.500 / 2AFC / JOD / MPRT-as-perceived,
  the single-observer-"informal" regime) → [`FG_PERCEPTUAL_EVAL_SOTA.md`](FG_PERCEPTUAL_EVAL_SOTA.md).
- The testbench this dossier proposes `TB-C9` into (the three axes, TB-C1/TB-C6/TB-C7/TB-C8) →
  [`FG_TESTBENCH_MASTER_PLAN.md`](FG_TESTBENCH_MASTER_PLAN.md).
- The fixed points this axis closes (C6 pacing jitter, C11 testbench A/B) →
  [`PHYRIADFG_OBJECTIVE_VISTA.md`](PHYRIADFG_OBJECTIVE_VISTA.md) §2.
- The pacing/latency floors + why the metronome is software-on-DWM not VRR →
  [`FG_CADENCE_LATENCY_PRIOR_ART.md`](FG_CADENCE_LATENCY_PRIOR_ART.md).
- Spatial/image quality (ghosting/disocclusion cure) → [`FG_VFI_PRIOR_ART.md`](FG_VFI_PRIOR_ART.md).

**Registration:** this dossier is registered in
[`../FORMAL_DOCUMENTS_REGISTER.md`](../FORMAL_DOCUMENTS_REGISTER.md) (Analysis / SOTA
dossier), in the FG SOTA-dossier cluster.
