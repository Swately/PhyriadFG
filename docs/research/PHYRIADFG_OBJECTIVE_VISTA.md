# PHYRIADFG_OBJECTIVE_VISTA — the complete objective space of a perfect external-capture FG, resolved

> **Diátaxis type:** Explanation (the objective *constitution* of PhyriadFG) + the closeable-fixed-point
> layer the build roadmap presupposes but never derived. It does NOT re-order the build work — that is
> [`PHYRIADFG_PERFECTION_ROADMAP.md`](PHYRIADFG_PERFECTION_ROADMAP.md). This sits **above** the roadmap:
> the roadmap answers *which lever next*; this answers *what we are optimizing for, why, where "perfect"
> is structurally unreachable, and — the missing piece — which objectives can be CROSSED and called done.*
>
> **Status:** `designed` (the taxonomy + the closeable criteria are forward framing). The STANDING facts
> per objective are `measured`/`shipping`/`designed` and tagged inline. The anti-cheat findings in §3 are
> `[VS]` — **author-verified first-hand this session** against the primary sources (the grep/URL is given).
>
> **Provenance.** Synthesized from two adversarial workflows: the objective-space + completeness audit
> (`wqdznsdsx`, 6 agents — the 9-family taxonomy + the deflation of folklore from real gaps) and the
> gap-family SOTA sweep (`w83r5ydl9`, 6 agents ~563 K tokens — fresh dossiers for the families that had
> no in-repo dossier: trust/anti-cheat, coverage, HDR, vendor, control-plane security, measurement). The
> author re-verified the load-bearing **safety** claims (§3 anti-cheat) first-hand; the rest carry the
> sweep's `[V1]/[V2]/[V3]` levels and its explicit "could-not-verify" flags (§7).
>
> **Verifiable-reference mandate (FDP).** No fabricated path, line, API, or benchmark. First-hand-this-
> session = `[VS]`; external-source levels = `[V1]` primary / `[V2]` corroborated / `[V3]` single-weak;
> in-repo anchors are cited file:line. Where a number is the author's own prior measurement it inherits
> that doc's level. §7 lists everything that could NOT be verified — read it before acting on any cell.
>
> **Normativity (BCP 14).** MUST / MUST NOT / SHOULD / MAY per RFC 2119/8174 only where a real requirement
> binds (a dogma, a plan-tier obligation, a safety gate). Everything else is descriptive.
>
> **Operator framing (governing).** Implementation is FROZEN; this is pure analysis. The goal is a REAL
> product that competes with LSFG — "perfection pointed-at, not chased blindly". The vista's job is to make
> the target FIXED and COMPLETE so the work stops feeling blind, and to name every floor so no future pass
> re-proposes an irreducible as a win.

---

## §0 — How to read this  ·  the one rule that fixes "no avanzamos"

The operator's reported feeling — *"parece que no avanzamos y no tenemos un encuadre fijo"* — is
structural, and the adversarial audit pinpointed why: **the roadmap is built ENTIRELY of asymptotic
objectives (improve a number against a wall, no finish line). There is no binary, closeable objective
anywhere in it.** A roadmap of only-asymptotes *cannot* feel like arrival, because nothing in it can be
crossed.

> **THE RULE — every objective is exactly one of two kinds; label it.**
>
> 1. **FIXED POINT** — binary, has a yes/no test, crosses ONCE and stays crossed. *This is what "avanzar"
>    means.* Collect them, check them off, never re-litigate a crossed one (§2 is the list).
> 2. **ASYMPTOTIC AXIS** — perceptual quality, no-ground-truth metric ceiling ~0.7–0.83 SROCC `[V2]`,
>    disocclusion fill. These have **no "done"** — they get *one tracked metric + a named floor*, and
>    "enough" is a threshold the OPERATOR declares, not a finish line.
>
> The "we never arrive" feeling is treating asymptotic axes as if they were fixed points. Most of the
> roadmap's P-items are fixed points wearing a `designed` label.

Three axes label every objective below, so the same work is findable three ways:

- **Family** — A perceptual-quality · B latency/feel · C performance/headroom · D scalability/multi-GPU ·
  E compatibility/coverage · F trust/safety · G consumption/UX · H measurement/honesty · I mission/identity.
- **Kind** — `FIXED-POINT` (closeable) or `ASYMPTOTE` (track-a-metric).
- **Standing** — `shipping` / `built` (in code, default-off) / `measured` / `designed` / `MISSING`.

The operator's four stated objectives map onto the families as: **quality = A**, **máximo rendimiento =
C**, **multi-GPU / escalabilidad = D**, **facilidad de consumo = G**, and **"FG paralelo en
ventanas/pantallas con o sin foco" = D2**. The **load-bearing ADDITIONS** the audit surfaced — the
objectives genuinely off the radar — are **family F (trust)**, **family E (coverage)**, **A3 (HDR)**, and
**C2 (the measured slice)**. Those are the real gaps; everything else was already tracked or is folklore
(see §6 for the deflation of what is NOT a real gap). **The trade-off RESOLUTION** — how the four goals
trade off when they pull apart (the priority, the pairwise rules, the default-promotion gate that pays down
the flag-debt) — is the doctrine [`../canon/PERFECTION_TRADEOFF_DOCTRINE.md`](../canon/PERFECTION_TRADEOFF_DOCTRINE.md).

---

## §1 — The objective taxonomy, resolved

Each objective: **(a)** the perfection target · **(b)** the honest floor (where perfect is structurally
unreachable for an external-capture tool) · **(c)** current standing (cite) · **(d)** the LSFG-specific
gap · **(e)** the closing lever (+ build tier) · **(f) ★ the closeable success criterion** (the binary
"done", or — if asymptotic — the single metric + its floor).

### FAMILY A — PERCEPTUAL QUALITY (what the eye sees)

**A1 — Generation / disocclusion quality** · `ASYMPTOTE` · `built`/`designed`
- (a) Revealed regions reconstruct cleanly; HUD/text never ghosts; fast objects don't strobe; the picked
  frame doesn't flicker candidate-to-candidate.
- (b) **Floor:** the *true-hole* (visible in NEITHER real frame) and *moving-bg-behind-moving-object* are
  irreducible without neural inpaint; the whole field concedes it. Our extra floor: all-non-neural where
  LSFG is lightweight-neural.
- (c) The most-worked frontier — STAGE-48 bidir detect + side-pick, `--bg-snap`/`--band-xfade` ship;
  P2/P4/S2/S4 designed (roadmap §2.2, §3).
- (d) **We LOSE on fill** (LSFG's neural model reconstructs; ours can't); we MATCH/LEAD on detection (the
  iGPU Sobel edge-gate, S2 — no shipping FG has an image-edge trust gate).
- (e) P4 disocclusion arbiter (T1) → S2 edge-gate promote (T1) → S4 neural-on-idle-4090 fill (T1, research).
- (f) ★ **ASYMPTOTE.** Metric: `flowdsc` (FloLPIPS-analogue) on held-out triples; floor ~0.83 SROCC.
  Progress = per-regime win/match/lose vs LSFG (low-motion / camera-turn / combat). No "done".

**A2 — Pacing / cadence smoothness** · `FIXED-POINT` (mostly) · `built`
- (a) Flat frame ladder locked to the panel clock; zero boundary-pulse, no beat-judder at 99 % GPU.
- (b) **Floor:** the +1 DWM-compose frame + the per-pair seam velocity-pulse (no engine MVs across the
  seam) are class floors.
- (c) PLL/NCO + `--sc-select` + `--phase-norm` ship; the PACING half is now LARGELY SHIPPING — `--pace-hard`
  (per-frame present-target PIN, present-MASD ~3.1→~0.95 ms ≈3.3×) + `--pace-vblank` (the DWM-metered
  vblank-phase lock via `DwmGetCompositionTimingInfo`, HOOK C, −39 % present-MASD) — **on the own-window path**
  (`--present-own-window` / Option A); the default DComp overlay stays DWM-jitter-limited. The earlier "NOT
  DWM-metered / 0 matches" is SUPERSEDED (`FG_PRESENT_TARGET_PACER_RISK_REGISTER` shipping; code `main.cpp:8375-8501`).
- (d) LSFG/FSR3 meter against the DWM composition clock; we now do too **on own-window**. The REMAINING gap is
  the PLACEMENT half — `--phase-norm` evens the tick SPACING but the displayed VELOCITY still STEPS at pair
  boundaries (`main.cpp:250`); that is the escalonado's unfixed half.
- (e) **P0 (DWM-metered pacing) is DONE** (`--pace-vblank`). NEXT = the PLACEMENT half (a phase-ladder
  controller pinning the interpolation phase toward the uniform/0.5 ideal) under the **measure→fix→measure
  loop** TB-C9 now enables — plan [`FG_FLUIDITY_FIX_MASTER_PLAN`](FG_FLUIDITY_FIX_MASTER_PLAN.md) (the pacing
  half INDEXED from the pacer triad; the placement half = the new content). Operator note: a big chunk of the
  escalonado is fixable NOW via `--present-own-window --pace-hard --pace-vblank` (untested by eye yet).
- (f) ★ **FIXED-POINT C6:** after P0, displayed-interval p99 jitter on `MsBetweenDisplayChange` < X ms
  (X = panel period × tolerance, operator-chosen — GamersNexus gives no fixed threshold `[V1]`), evidenced
  by a PresentMon CSV (or TB-C9's display-interval pacing). Binary once X is set. The escalonado is now
  MEASURABLE end-to-end (TB-C9: placement RMS + pacing stddev) → the fix is iterable, not eyeballed.

**A3 — HDR / format completeness** · `FIXED-POINT` (ingest+present) · `MISSING` (the real gap)
- (a) HDR source enters at full precision, survives the pipeline with **no irreversible [0,1] clip**, and
  presents colorimetrically correct (HDR10/scRGB on HDR displays, rolloff-tonemapped on SDR).
- (b) **Floor:** internal scRGB-FP16 *arithmetic* is foreclosed — NVIDIA `[V1]` and Intel `[V1]` both
  refuse it on tensor HW; Pascal 1080Ti runs FP16 at **1/64 FP32** `[V1/V2]`. So "carry FP16 through the
  warp math" is a floor; the reachable path is FP16 *ingest+storage+present* with quantized interp math.
- (c) **8-bit SDR pipeline that recognizes HDR formats and hard-clips them.** A `route_for()` format router
  + an `hdr_convert.comp` exist, BUT WGC ingest is **hard-locked BGRA8** (`main.cpp:3053`) so HDR never
  reaches the HDR branch under production capture; present is BGRA8 with **no `SetColorSpace1` anywhere**
  (`PresentSurface.cpp:206`); the tone-map is a hard `min(c,1.0)` clip (`hdr_convert.comp:32`). `[VS via
  dossier]`
- (d) **Binary feature gap — LSFG ships a working HDR toggle, we cannot even INGEST HDR.** The one audit
  dimension where a marketed rival feature is simply absent. NOTE: the gap-audit's "10bpc R10G10B10A2 is
  the cheap target" is **WRONG for this surface** — MS HDR10 (Option 2) **excludes alpha-blended
  swapchains** `[V1]`, and the DComp overlay is premultiplied-alpha → the only HDR present path is Option 1
  (FP16 scRGB, 64 bpp).
- (e) `SetColorSpace1` on present (T0, fixes washout even for SDR on an Advanced-Color desktop) →
  parameterize WGC ingest format (T1) → replace the hard clip with a Reinhard/ACES rolloff + dither (T1)
  → FP16-scRGB present path (T2).
- (f) ★ **FIXED-POINT C4 (two-stage binary):** **A3-1** = on a real HDR game, a known >1.0-luminance
  region (sun/muzzle flash, via `--qdump`) reaches the warp un-clipped and tone-maps with rolloff not a
  255-pile-up — evidenced by a histogram. **A3-2** = on an HDR display, no washout/banding + the present
  swapchain reports an HDR colorspace. Asymptotic tail = wide-gamut interpolation accuracy (whole-class
  frontier). Backing: [`FG_HDR_FORMAT_PRIOR_ART`](FG_HDR_FORMAT_PRIOR_ART.md).

### FAMILY B — LATENCY / FEEL (what the hand feels)

**B1 — Input-to-photon responsiveness** · `ASYMPTOTE` (floored) · `measured`
- (a) Added lag below the user's perceptual/competitive threshold; feels native.
- (b) **Hard structural floor:** no Reflex/Anti-Lag hook (unavailable to ALL external tools) + ~1-frame
  interpolation hold → structurally ~1–2 frames worse than in-engine FG; cannot match AFMF2's
  driver-present hook. **FG can NEVER lower latency — the lever is variance/judder, not responsiveness.**
- (c) Measured AddedLat ~108 ms = ~68 ms removable D-anchor + ~40 ms WGC floor (roadmap §2.4); the
  `--async-present` decouple measured **73→32.6 ms** in BF6 (the biggest win — render-assistant arc).
- (d) **Parity floor with LSFG** (both no-hook external); AFMF (driver-integrated) leads both. We can MATCH
  not LEAD; the removable excess is the D-anchor + the present-fence.
- (e) flip `--async-present` default-ON (T2, RISK_REGISTER) → P1 fractional controller → S5 extrapolation
  (the one axis interpolation-class LSFG can't beat, HARD-gated on P4's clean fill).
- (f) ★ **ASYMPTOTE against a floor.** Metric: PresentMon "All Input to Photon Latency". **FIXED-POINT
  C7:** on the same BF6 scene, our latency ≤ LSFG + Δ, evidenced by a committed PresentMon CSV pair —
  doable NOW (T0), the instrument is FG-agnostic `[V1]`.

### FAMILY C — PERFORMANCE / HEADROOM (the slice cost)

**C1 — Robustness under saturation ("assert the slice")** · `FIXED-POINT`-ish · `built`/`designed`
- (a) FG stays on + degrades gracefully through fast motion and 99 % GPU; never cuts to native (the cadence
  break is worse than an artifact).
- (b) **Floor:** consumer-GPU queues do NOT preempt a saturating dispatch (proven dead-end — do not
  re-propose a priority scheme). Make-space, not preemption.
- (c) `--async-present` (decouple), `--target-output-fps` (STAGE-119 fractional governor), make-space
  built; SHIPPING default present is still synchronous (`vkWaitForFences(...fBridge...UINT64_MAX)` `[VS]`).
- (d) The field (FSR3 `SimpleMovingAverage<10>` variance-aware target) solves this with a display-locked
  clock + drop-on-slip + cost-bound generation; **we built the primitives but ship them OFF.**
- (e) flip async-present default-ON → P1 fractional controller → P3 runtime-adaptive flow-scale → S3
  three-queue split.
- (f) ★ The closeable part is C9 (the one-knob holds a target output fps). The "graceful under saturation"
  feel is operator-eye-gated (his felt-stability). Metric: present-interval variance (MASD/abs-stddev,
  **NOT CoV** — CoV inverts under a mean shift).

**C2 — Minimal MEASURED GPU slice (the efficiency mandate as an objective)** · `FIXED-POINT` · `MISSING`
- (a) The smallest slice for the quality, **measured per-rig** including the single-GPU degenerate case;
  "if you can't measure the hot path you can't claim it's fast" (CLAUDE.md) enforced as a gate.
- (b) **Floor:** the interpolation hold (~12–17 ms) + double-present + cross-device freshage tax are
  irreducible slice components; efficiency optimizes *around* them.
- (c) The efficiency mandate is canon but **the single-GPU FG slice has NEVER been measured** (DCAD
  Phase 1, "the load-bearing UNMEASURED unknown"). The operator tracks throughput/fps, not "minimal
  measured slice".
- (d) Not an LSFG head-to-head — an internal honesty objective; the one number that decides whether the
  *majority-case* (single-GPU) user is net-positive at all.
- (e) Run the single-GPU slice measurement (T0 mechanically; it is the cheapest closeable fixed point).
- (f) ★ **FIXED-POINT C1-companion:** a committed CSV records the single-GPU slice ms at 1080p/1440p/4K on
  one GPU, with no fallback-to-vanilla. Binary, owned, unrun. **This is the cheapest "avanzar" available.**

**C3 — Power / thermal cost** · `ASYMPTOTE` (trivial) · `MISSING`
- (a) Running idle silicon (the 2nd GPU) hot has a watts/thermal budget the user can see.
- (b) No structural floor. (c) No doc tracks watts/thermals — only fps and slice-ms. (d) Invisible for a
  tool sold partly on "use the idle GPU". (e) A future control-plane telemetry field (T0). (f) ★ Lowest
  blast-radius; a tracked NVML power/temp field, not a product gate. *Deflated from "objective" to telemetry.*

### FAMILY D — SCALABILITY / MULTI-GPU (scaling up)

**D1 — Multi-GPU as a felt NET benefit** · `FIXED-POINT` · `shipping` (asserted)
- (a) Extra GPUs add headroom and never show up as felt lag.
- (b) **Floor:** the 2nd cross-device hop (~3–8 ms) is a real tax single-GPU LSFG never pays.
- (c) Single-GPU collapse ships; the split is **asserted, not A/B-measured** (gap-audit §3.2).
- (d) LSFG also does any-vendor dual-GPU (secondary can be a different brand/iGPU `[V2]`); our edge is a
  *measured* break-even gate LSFG lacks — but it is unrealized until P5 wires the router.
- (e) P5 wire the router (T2) + A/B-measure the multi-GPU net benefit vs single-GPU.
- (f) ★ **FIXED-POINT:** a CSV pair shows the multi-GPU configuration's felt latency + slice ≤ the
  single-GPU configuration's on the same scene. Binary, currently asserted-not-measured.

**D2 — Parallel FG across windows / screens (focus-independent)** · `FIXED-POINT` · `designed`/`research`
- (a) FG on game B / monitor 2 (out of focus, second screen) while game A runs on monitor 1, each correct,
  no interference. **The operator's #4 objective; LSFG's single-target architecture STRUCTURALLY cannot
  follow** (multi-frame-gen = stacked passes on ONE target).
- (b) No structural floor — the headline differentiator. Each capture→flow→warp→present chain is a
  self-contained role graph; `select_participants` partitions a measured device set; one PresentSurface
  per panel holds the "one present authority" invariant *per panel*, so N panels don't reintroduce SLI
  runt-frames.
- (c) Roadmap S1, **needs its own Tier-2 design plan that doesn't exist yet** (depends on P5).
- (d) **We can LEAD** — LSFG cannot do true independent instances. Unrealized.
- (e) P5 router → an S1 Tier-2 design plan (independent present authorities + scheduler partition +
  multi-surface DComp).
- (f) ★ **FIXED-POINT:** two games on two monitors, each with its own FG instance, run ≥60 s with correct
  output and no cross-interference, evidenced by two telemetry CSVs. Binary.

**D3 — Internal maintainability / N-scaling spine** · `ASYMPTOTE` · `designed` (named debt)
- The ~115-stage flag cascade ("parts bin, not yet a machine") consolidated into one DCAD spine scaling
  1→n. n>2 GPU is beyond documented art. *Engineering hygiene; tracked as design-debt, not a product end.
  Deflated — real but not a first-class objective.*

### FAMILY E — COMPATIBILITY / COVERAGE (scaling down and out)

**E1 — Vendor-agnosticism (robustness DOWN to one arbitrary-vendor GPU)** · `FIXED-POINT` · `MISSING`/unverified
- (a) The default (flag-free) path produces correct, smooth output on a *single* GPU of each vendor —
  NVIDIA (Pascal+), AMD (RDNA), Intel (Arc/Xe iGPU) — using only portable primitives (Vulkan compute +
  fp16, optional DP4a); every vendor accelerator (NVOFA) is opt-in, measured-gated, never a default
  dependency.
- (b) **Floor:** no HW-OFA on AMD/Intel exists `[V1]` → OFA acceleration is intrinsically a single-vendor
  bonus; the AMD-render + NVIDIA-secondary launch failure is a driver-class floor LSFG hits too.
- (c) **UNVERIFIED — "great on the author's NVIDIA pair, untested on a stranger's single Arc/AMD."** The
  router is effectively dead (`break_even_decide` 1 caller `[VS]`); `GpuDescriptor` has `compute_gflops`
  only — **no fp16/DP4a field**; the break-even constant is the author's 4090-measured value. No
  cross-vendor run has ever happened. **This is exactly the operator's stated fear, currently true by
  omission.**
- (d) **We LOSE today** — LSFG is verified any-vendor (its whole user base is mixed) + ships a separate
  low-power model. **★ Key SOTA inversion:** AMD/Intel GPUs *out-perform* NVIDIA at the FG pass due to
  superior fp16 throughput `[V2]` — so leaning on NVOFA (NVIDIA-only) is **backwards for E1**; the portable
  winner is fp16/DP4a. NVOFA is a 4090-bonus, not the path.
- (e) P5 add fp16/DP4a field to `GpuDescriptor` + wire CHARACTERIZE (T2) → make fp16 the default primitive,
  gate NVOFA/DP4a on per-rig measured break-even (T1) → low-power-GPU default profile (T1).
- (f) ★ **FIXED-POINT C5 (three sub-checks):** the default path runs correct+non-crashing ≥60 s on
  ☐ one non-author NVIDIA, ☐ one AMD, ☐ one Intel GPU, each with a `fg_quality_scorer` run in the
  LSFG-comparable band + a clean telemetry CSV + the capability table populated by runtime CHARACTERIZE
  (no vendor name in the decision path — `[VS]` grep = 0). **Honest caveat:** the blocker is HARDWARE
  ACCESS (author has only NVIDIA), not code. Until hardware: the progress metric is *the count of
  hardcoded-vendor assumptions driven to zero* (deviceType/LUID branches, the NVIDIA break-even default,
  the non-representable-GPU fields) — when those hit 0 the code is E1-ready and only the run remains.
  Backing: [`FG_VENDOR_LANDSCAPE_PRIOR_ART`](FG_VENDOR_LANDSCAPE_PRIOR_ART.md).

**E2 — Real-library compatibility breadth** · `FIXED-POINT` (via "no silent failures") · `MISSING`
- (a) The user's actual games — DX11/12/Vulkan/OpenGL × exclusive/borderless/windowed — capture+present
  correctly OR fail gracefully with a clear reason.
- (b) **Floor:** exclusive-fullscreen bypasses DWM → WGC unreliable (only injection reaches it = ban);
  protected/HDCP content → black for ALL OS capture `[V1]`; hybrid-laptop dGPU DD is `UNSUPPORTED` by
  design `[V1]`.
- (c) "fullscreen" appears **once** in the entire 14-dimension gap-audit; tested only on BF6 + Star Rail;
  no per-API matrix, no graceful-fail-with-reason layer.
- (d) **LSFG's moat is the FALLBACK LADDER (DXGI/WGC/GDI auto-selected per Windows version) + years of
  per-title community validation `[V1]`, not a superior algorithm.** We have WGC + a legacy DD fallback,
  no GDI rung, no per-OS auto-switch, no validated matrix. **This is the single widest coverage gap —
  wider than any algorithm gap.** Key simplification: the 4 render APIs collapse to the SAME WGC
  composition path, so the load-bearing axis is *windowing mode + content-protection + anti-cheat*, NOT
  the render API.
- (e) build the compatibility matrix as a living artifact + the graceful-fail reason layer + the fallback
  ladder (T1 each — together they ARE the moat).
- (f) ★ **FIXED-POINT (E2-GRACE, the highest-leverage closeable in E2):** every floor-excluded/broken cell
  in the matrix is reached via a specific reason code from a fixed enumerated taxonomy, with **NO
  generic/unknown failures** — i.e. *zero `broken-without-explanation` cells.* Binary, testable, and it
  converts the open-ended library into a finite "no silent failures" target. The asymptotic remainder
  (library growth, AC drift) reduces to one metric: passing-cell count over an expanding test set.
  Backing: [`FG_COMPATIBILITY_COVERAGE_PRIOR_ART`](FG_COMPATIBILITY_COVERAGE_PRIOR_ART.md).

### FAMILY F — TRUST / SAFETY (will it hurt me) — the largest blind spot

**F1 — Anti-cheat / account safety as a MEASURED trust posture** · per-engine binary matrix · `MISSING`
- (a) *Justified, evidenced* confidence the tool won't flag a given online game's anti-cheat — false-
  positive heuristics included, not just true injection.
- (b) **Floor (three structural reasons safety is asymptotic, not certifiable):** (1) no anti-cheat vendor
  guarantees external-tool safety — it is heuristic + discretionary (BattlEye defers to per-game-dev
  blocklists `[VS]`; EA "does not recommend third-party software" `[VS]`; LSFG itself disclaims any
  guarantee `[V1]`); (2) kernel anti-cheats see the overlay regardless of injection (Vanguard screenshots
  the desktop server-side; Javelin enumerates windows from the kernel); (3) **WDA-exclusion is itself an
  active cheat-class signal** (below).
- (c) Asserted safe on `[V2/V3]` community grounds (DCAD:361 cites the LSFG Steam page); the overlay
  fingerprint is an admitted **open, untested** decision (DCAD §9-4); **no first-hand evidence on any
  kernel-anti-cheat title, no trust matrix.** The in-repo "must run on BF6" (DCAD:29) is a **requirement,
  not a verified capability.**
- (d) **★ Author-verified `[VS]` this session — two concrete holes the repo under-weights:**
  - **WDA-exclusion fingerprint.** PhyriadFG's overlay is WDA-excluded. Microsoft documents
    `WDA_EXCLUDEFROMCAPTURE` as **"unlike a security feature… no guarantee"**, DWM-only `[VS,
    learn.microsoft.com SetWindowDisplayAffinity]`. Cheats use this exact flag to hide overlays from
    capture, and **anti-cheats actively monitor it as a cheat indicator** `[VS, corroborated:
    ssno.cc TAC-RE, adamsvoboda, Medium]`. → PhyriadFG performs the exact technique anti-cheats
    fingerprint. **If LSFG's overlay is plainly capturable, we carry a cheat-signal LSFG does not.**
  - **BF6/Javelin is the worst-case, not a given.** Reported BF6 bans after running Lossless Scaling
    ("3-day bans for Gameplay Enhancements"); EA's only stance: *"may have been applied incorrectly,
    appeal"* + *"doesn't recommend third-party software"*; **no clearance** `[VS, EA Forums 12741924 /
    12780739]`. Javelin (kernel) flags external FG in a "gray area".
  - Pro-safety basis (real but discretionary): BattlEye tolerates non-cheat overlays "unless the game dev
    decides otherwise" `[VS, battleye.com/support/faq]` (blocked in PUBG/Fortnite/IoN).
- (e) **★ Highest-value, lowest-cost lever — T0, free, under our control:** drop/condition WDA-exclusion
  (a capturable overlay, like OBS-class tools that ARE tolerated, removes the single strongest cheat
  signal). Then: de-fingerprint the window (avoid the game-sized + topmost + layered/transparent tuple) →
  a per-engine trust gate that refuses/warns on UNTESTED titles (T1) → the real-rig empirical campaign (T2).
- (f) ★ **NOT a single binary — a MATRIX of per-engine binaries**, each closeable by evidence:
  - **VAC / BattlEye / EAC: CLOSEABLE per-title** — "a throwaway account survives ≥N hrs active MP across
    ≥2 driver versions + ≥1 anti-cheat update: zero kick/launch-block/ban, trusted-mode retained where
    applicable, evidenced." (FIXED-POINT C3 is the BF6-class instance of this.)
  - **Vanguard / BF6 Javelin (kernel + active capture): ASYMPTOTIC, not binary** — server-side
    discretionary; honest "done" = "survives the campaign AND the operator accepts residual risk +
    recommends against ranked use." Single metric: **cumulative survived-account-hours per (engine × title
    × anti-cheat-version), reset to ZERO on any anti-cheat update.** Never let it become "we believe it's
    safe."
  - **Operator check — RESOLVED 2026-06-22 (Lightshot):** LSFG's FPS overlay is uncapturable by software
    screenshot tools → LSFG is display-affinity-excluded like us → the divergence is **PARITY** (lever (e)
    is optional, not mandatory). Strong evidence, not lab-confirmed (a `GetWindowDisplayAffinity` probe is
    definitive). Uncapturable by *software* ≠ by a camera/capture card.
  Backing: [`FG_TRUST_ANTICHEAT_PRIOR_ART`](FG_TRUST_ANTICHEAT_PRIOR_ART.md).

**F2 — Don't destabilize the captured game** · split-by-vendor · `built`/`MISSING`-as-contract
- (a) FG-on never makes the game crash, stutter worse, lose VRR/G-Sync, or break fullscreen — the user's
  baseline is never degraded.
- (b) **Floor (NVIDIA, structural):** an external present surface that doesn't hook the game can't enter
  its flip chain → game loses independent-flip → **NVIDIA's G-Sync activation algorithm does not trigger
  for a non-hooking overlay** `[V1, NVIDIA dev forum]`. No external tool fixes this; **AFMF (driver-
  integrated) keeps VRR — external-overlay FG (LSFG + us) cannot, on NVIDIA.** On AMD/Intel, VRR DOES work
  with overlays → closeable there.
- (c) Named as a floor in-repo ("LSFG hits the identical wall, disables G-Sync"); upgraded here to a
  vendor-asymmetric, primary-sourced floor; the *contract* ("the game's experience is never worse with FG
  on") is not stated as such.
- (d) **Exact parity with LSFG on NVIDIA** (both lose G-Sync); AFMF leads both. On AMD/Intel we can MATCH-
  not-lose.
- (e) vendor-conditional honesty + windowed-G-Sync guidance on NVIDIA (T0); a displayable/MPO present-mode
  probe (T1, likely null on NVIDIA, possibly positive on AMD/Intel).
- (f) ★ **AMD/Intel: FIXED-POINT** — monitor OSD reports VRR ACTIVE with PhyriadFG attached, photo-
  evidenced. **NVIDIA G-Sync: NOT closeable — a permanent class floor shared exactly with LSFG.** Stop
  trying to close it; the honest "done" = documented limitation + windowed-G-Sync partial mitigation.

**F3 — Telemetry / control-plane privacy & security** · `FIXED-POINT` · `MISSING` (the only one with NO floor)
- (a) Telemetry egress + any future remote/mobile control are authenticated, scoped, leak no host
  fingerprint (GPU LUIDs, window titles, profile names); security-reviewed *before* it exists.
- (b) **No structural floor — fully the product's to control.** The only soft tension: a remote monitor is
  useful precisely because it shows host state.
- (c) **Safe by default today by accident of immaturity:** MVP transport = stdout NDJSON, in-process — but
  `CONTROLPLANE_MASTER_PLAN.md:99` **explicitly defers "any auth/security surface"** while §1/§3 commit to
  a WebSocket/mobile transport. Sensitive fields are already in the working set: window TITLES
  (`main.cpp:1839` `GetWindowTextA`), PID (`main.cpp:6887`), GPU identities, NVML host signals.
- (d) LSFG has zero telemetry egress → zero surface. "Matching LSFG" = the default stays as egress-free as
  LSFG; we diverge the moment the WebSocket ships. **Asymmetry to internalize:** read-only egress is a
  confidentiality risk; **control ingress is an integrity/RCE risk** (the OBS-WebSocket→RCE chain `[V1]`
  proves a file-write/spawn command is an RCE primitive). Cross-Site WebSocket Hijacking means localhost
  binding does NOT stop the user's own browser `[V1, PortSwigger/OWASP]` → an Origin-allowlist is mandatory.
- (e) classify frame fields by sensitivity, emit only `public` over the network unless `--allow-host-data`
  (T0) → split the ring so egress is constructible with NO command-ring attached (T0) → local-only bind +
  Origin allowlist + auth-before-every-command when control ships (T1) → mobile pairing with a VETTED
  handshake, never hand-rolled PIN+salt (Moonlight's CVE-2020-11024 came from exactly that `[V1]`) (T2,
  RISK_REGISTER).
- (f) ★ **FIXED-POINT (committed CI audit `controlplane_security_audit`):** (A) a packet capture with
  `--allow-host-data` OFF contains NO window title / raw PID / GPU UUID / profile name (grep = 0);
  (B) an unauthenticated client (raw socket skipping the handshake + a cross-origin browser with forged
  JS Origin) causes ZERO observable FG state change; (C) no `network-bind + auth-disabled` config is
  constructible (a build/type-level invariant). All three green = F3 closed for that increment.
  Backing: [`FG_CONTROLPLANE_SECURITY_PRIOR_ART`](FG_CONTROLPLANE_SECURITY_PRIOR_ART.md).

### FAMILY G — CONSUMPTION / UX

**G1 — Two-click, set-once, never-fiddle** · `FIXED-POINT`-ish · `designed`
- (a) Install → pick window → one trustworthy default → forget; settings persist per game; zero per-game
  knob-tuning *ever*.
- (b) No floor — purely addressable.
- (c) CLI-only, ~61 flags; the Tauri/React frontend + per-game profiles are designed (PHYRIADFG_UI plan);
  the "Auto" default's quality is **deferred to operator eye → the set-and-forget GUARANTEE is unowned.**
- (d) LSFG = two clicks + per-game profiles. We trail until P6.
- (e) P1 (the one-knob fractional target) → P6 (CP2 + the frontend: window-pick + Start + profiles + the
  quality↔latency slider over `--flow-scale auto`).
- (f) ★ **FIXED-POINT C9:** a user sets ONE target-fps knob and FG holds it without per-flag tuning,
  across a session, no per-game fiddling — evidenced by a clean run with default flags only.

### FAMILY H — MEASUREMENT / HONESTY (is the claim evidence or eye)

**H1 — Legible, non-inflated user feedback + reproducible quality measurement** · split · `built`/`MISSING`-loop
- (a) The user sees the real multiplier / added latency / whether GPU2 helps, without a rig, never an
  inflated number; every "we improved/regressed" claim is CI-gated, not eyeballed.
- (b) **Floor:** no-GT perceptual quality is asymptotic against **~0.7–0.83 SROCC** `[V2, BVI-VFI]` — no
  metric ever explains all human FG-judgment variance; live FG output has no ground truth by construction
  (withholding a real manufactures the artifact).
- (c) **The instrument is BUILT** (`fg_quality_scorer`: held-out triples, modes A/B/T, `dbl_edg_m` /
  `flowdsc`) — LSFG structurally CANNOT self-score full-reference (its overlay is WGC-uncapturable, we
  can). But it is **NOT wired into CI** and **NOT calibrated** against a human-rated set.
- (d) **We LEAD on self-measurement** (true full-reference on our own output); we trail the published field
  only on calibration rigor — but so does every shipping FG tool.
- (e) wire the scorer into CI as a regression gate (T0) → calibrate `dbl_edg_m`/`flowdsc` SROCC vs BVI-VFI
  (T1) → durable A/B eye-verdict log → regression signal (T1).
- (f) ★ **FIXED-POINT C8:** the scorer runs in CI on a pinned held-out corpus and **fails the build on a
  >X % `dbl_edg_m` regression** — evidenced by a deliberate regression turning CI red. + **ASYMPTOTE:**
  SROCC of `flowdsc` vs BVI-VFI human MOS, "enough" ≥ 0.7, ceiling ~0.83.

**H2 — Honest head-to-head vs LSFG** · mostly `FIXED-POINT` · `designed`
- (a) A reproducible, same-instrument, same-scene comparison scored by an axis that sees BOTH.
- (b) **Floor:** full-reference comparison of LSFG is structurally impossible (its output is WGC-excluded;
  display-duplication capture is unsynchronized to the source reals → no held-out triples). The only
  shared instrument is **photon-level** (capture card / high-speed camera / FCAT), scored no-reference
  (~0.7 ceiling) — and Animation-Error pacing **cannot score fake frames at all** `[V1, GamersNexus]`.
- (c) `designed` (S6a). (d) Even on the shared axis; we LEAD on self-measurement.
- (e) **latency + pacing head-to-head are FG-agnostic and doable NOW (T0):** PresentMon "All-Input-to-
  Photon" + `FrameType` + `MsBetweenDisplayChange` variance on both; the camera/NR quality rig is T1.
- (f) ★ **FIXED-POINT C7:** committed PresentMon CSV pair shows our latency ≤ LSFG + Δ AND our displayed-
  interval p99 jitter ≤ LSFG's, same BF6 scene, no camera. + **ASYMPTOTE:** the camera/NR quality verdict
  = per-regime win/match/lose scorecard.
  Backing: [`FG_MEASUREMENT_METHODOLOGY_PRIOR_ART`](FG_MEASUREMENT_METHODOLOGY_PRIOR_ART.md).

**H3 — The controlled FG test ENVIRONMENT (the testbench)** · infrastructure that makes H1/H2/C7/C8 cleanly closeable · `designed`
- (a) ONE deterministic synthetic source feeding THREE measurement axes — **objective** (numbers vs analytic
  ground truth), **perceptual** (blind 2AFC eye-A/B → JOD/Bradley-Terry), **photonic** (high-speed-camera
  pacing, the operator's S24 slow-mo) — so every lever is measured under CONTROLLED, REPEATABLE
  saturation/resolution/fps instead of scene-confounded real games. **★ The input-connected evolution
  (TB-C12 testbench_minigame, `designed` 2026-06-25):** a NEW sibling app — ONE customizable minigame
  (dialable GPU saturation · controllable movement · live input) over the reused TB-C1 scene + an external
  `fg_latency_scorer.py` — adds a fourth measurement axis, **input→photon** (the felt ~0.5 s), and
  **resolves the open TB-2 realism risk**: a real minigame whose FIFO present lets the render-ahead queue
  build IS the graphics+present back-pressure that the existing held-util load (TB-C3, which offscreen-
  decouples its present) structurally cannot be. `render_assistant` stays zero-touch (the scorer infers
  interpolated FG frames EXTERNALLY). It is the EVOLUTION of this objective, not a new one.
- (b) **Floor:** real-game testing is structurally scene-confounded (the measured multiplier ranged
  **0.59×–3.1×** across BF6 combat, drowning any per-lever signal; skipped-frame GT violates the linear-motion
  assumption → "impossible without an oracle" `[V1, arXiv:2403.17128]`) — the synthetic source is the ONLY
  rigorous escape. Single-observer eye-A/B is ITU "informal" (compensated by many randomized within-subject
  trials); the eye-proxy (calibrating the scorer to MOS) is an OPEN problem (no metric > PLCC ~0.6 for FG —
  H1's ceiling). The graphics-queue saturation load is UNSOURCED in the SOTA (pioneered). **For the
  input→photon axis (TB-C12):** absolute latency truth comes ONLY from the camera/photodiode over the FG
  OUTPUT window — the software CSV + PresentMon EXCLUDE scanout + pixel response (proxies, not truth);
  camera GT is frame-quantized (±1 frame: 4.17 ms @240 fps, 1.04 ms @960 fps) → report DISTRIBUTIONS only;
  GT-quality and live-latency are DIFFERENT runs, never simultaneous; bit-exact replay holds only over
  SHORT hash-verified windows; the felt ~0.5 s is hardware/driver/present-mode-specific. And **TB-2 is NOT
  closed by the TB-C12 design** — only the operator's first-hand rig A/B lifts it (a monitor-free
  supervisor can compile-verify but not measure).
- (c) `designed` — the Tier-2 plan triad + the 2 SOTA dossiers are authored (this objective's annexes). The
  objective-quality HALF already exists OFFLINE (`prep_zoo_sequence.py` deterministic source + held-out GT +
  `fg_quality_scorer` + `--csv`); the foundation (TB-C1 source + TB-C2 bounded-run + TB-C3 held-util load +
  TB-C4 sweep + TB-C6 camera + TB-C7 aligner) is built + self-test-verified; the **TB-C12 minigame**
  (input→photon, the TB-2 resolution) is `designed` (Phase-0 governance written, no code) — its risks are
  the MG-1..MG-9 namespace in the RISK_REGISTER, all `open`, with MG-2 liftable only by the operator's rig
  A/B. The eye axis (TB-C5) remains unbuilt (needs the operator's eye).
- (d) The controlled rig is OURS — LSFG cannot self-measure full-reference (WGC-uncapturable); the deterministic
  testbench is a measurement moat, not a parity feature.
- (e) build the 3 axes in PARALLEL (operator steer 2026-06-24) + frame-ID alignment for live-absolute GT
  (component TB-C7) → the first measurable win is a CLEAN A/B of `--async-present` (the root-caused
  combat-collapse fix, the multiplier being a throughput axis distinct from latency) under deterministic
  saturation — the scene-confound made structurally impossible. **+ the TB-C12 minigame** (input→photon
  axis): three reusable headers (`latency_tap` / `gfx_present_load` / `interactive_scene`) over the reused
  TB-C1 scene + the external `fg_latency_scorer.py` → the operator's rig A/B reproduces the BF6 collapse
  signature under a REAL graphics+present load (lifting TB-2/MG-2) and quantifies the felt ~0.5 s as a
  distribution. Zero `render_assistant` edit.
- (f) ★ **FIXED-POINT:** the testbench produces a reproducible per-config A/B matrix — the same synthetic scene
  + the same target util → a deterministic quality+throughput+pacing delta, and an **A/A null run returns zero
  difference** (the closeable test). + **ASYMPTOTE:** the eye-verdict dataset → scorer-to-MOS calibration
  (H1's (e); open ceiling). Plan: [`FG_TESTBENCH_MASTER_PLAN`](FG_TESTBENCH_MASTER_PLAN.md) (+
  `_IMPLEMENTATION_STRATEGIES`, `_RISK_REGISTER`). SOTA: [`FG_VFI_MEASUREMENT_SOTA`](FG_VFI_MEASUREMENT_SOTA.md)
  + [`FG_PERCEPTUAL_EVAL_SOTA`](FG_PERCEPTUAL_EVAL_SOTA.md).

### FAMILY I — MISSION / IDENTITY (why this exists at all)

**I1 — Anti-obsolescence (weak/old silicon as contextual value)** · `value` (not a movable objective)
- Each GPU (1080Ti = flow, iGPU = convert/pack) has a role where it beats/relieves the 4090; product value
  scales UP as the hardware mix gets older/more heterogeneous. **Honest floor: this is in genuine tension
  with latency** (the same heterogeneity is the freshage tax) — a *trade*, not a free win. **It is a
  VALUE, not a closeable objective** (you can't move it without regressing B1). Tracked as the *why* behind
  D/E1; do not invent a metric for a tradeoff. *Deflated from the synthesis's "objective with a target".*

**I2 — "Pointed-at, not obsessed-over" (calibrated pursuit)** · meta · `shipping`
- The project knows which floors are irreducible and refuses to spend effort there (§7 honest floors). The
  strongest-tracked identity objective; only failure mode is drift (re-proposing a dead-end). Operationally
  satisfied by this document's §7.

**I3 — FG as the forcing function for substrate growth (the recursive-FR loop)** · meta-framing · *deflated*
- The project's autonomy north-star: FG-discovered gaps qualified through the FR gate become pillar
  capability. Real **as project philosophy** (it is the autonomy north-star) — but **as a PhyriadFG product
  objective it is a category error**: a perfect FG tool could harvest ZERO FRs (honest-null is legal). The
  adversarial audit deflated it: "grow the substrate" is not a property of a good FG; the router + control-
  plane are not "the first harvested FRs of a loop" — they are dependencies to wire (P5/P6). **The roadmap
  dropping it is CORRECT, not a miss.** Kept here only to record the deflation so it is not re-promoted.

---

## §2 — The closeable fixed points (the antidote, consolidated)

The checklist of crossings. "Avanzar" = moving a row from OPEN to CROSSED. Never re-litigate a CROSSED row.

| # | Fixed point | Family | Binary test (the "done") | Status |
|---|---|---|---|---|
| **C1** | Single-GPU slice MEASURED | C2/perf | committed CSV: FG on one GPU, slice ms recorded, `--force-single-gpu` clean | **CROSSED** (shipped) — but the *slice measurement itself* is still OPEN (DCAD Phase 1) |
| **C2** | Byte-identical-off | robustness | every default-off flag → bit-identical to FG-disabled (diff = 0) | largely CROSSED — re-assert in CI |
| **C3** | Trusted on BF6 + anti-cheat, evidenced | F1/ux | throwaway account survives ≥N hrs BF6 MP, no kick/block/ban, video-evidenced | **OPEN** — and §3 shows BF6 is the asymptotic-risk tier, treat DCAD:29 "must run" as UNMET |
| **C4** | HDR ingests without clip | A3 | HDR source round-trips capture→present, no clip/tonemap delta, pixel-diff evidenced | **OPEN** — clean closeable win, not in roadmap |
| **C5** | Runs on each vendor | E1 | FG runs (no crash, slice measured) on NVIDIA + AMD + Intel, one CSV each, CHARACTERIZE-populated | **OPEN** — blocked on hardware access, not code |
| **C6** | Pacing jitter < X ms on the panel clock | A2 | after P0, displayed-interval p99 jitter < X (operator-set), PresentMon CSV | **OPEN** — blocked on P0 (DWM metering = 0 today) |
| **C7** | Input latency ≤ LSFG + Δ, same scene | B1/H2 | PresentMon All-Input-to-Photon CSV pair | **OPEN**, doable NOW (T0) |
| **C8** | FG-quality regression gate live in CI | H1 | a deliberate quality regression turns CI red | **OPEN** — scorer built, unwired |
| **C9** | The one-knob default exists | G1 | user sets one target-fps knob, FG holds it, no per-flag tuning | **OPEN** — P1 verified unbuilt |
| **C10** | Graceful single-GPU degrade | C1/robustness | kill GPU-2 mid-run → no crash, FG continues on GPU-1, evidenced | partial — `vk_live()` ships; make it an *evidenced* test |
| **C11** | Controlled testbench A/B reproducible | H3 | same synthetic scene + same target util → a deterministic quality/throughput/pacing delta, and an A/A null run returns diff = 0; + the TB-C12 minigame reproduces the input→photon collapse signature under a real graphics+present load (lifts TB-2/MG-2, operator rig A/B) | **OPEN** — foundation built+self-tested; the TB-C12 minigame (input→photon, the TB-2 resolution) is `designed`, MG-1..MG-9 all `open` |

**The cheapest crossings available right now (T0, no hardware, no eye):** **C1** (measure the single-GPU
slice — the one number that decides if the majority user is net-positive), **C7** (PresentMon latency +
pacing vs LSFG), **C8** (wire the scorer into CI). Crossing any of these is unambiguous progress.

---

## §3 — The anti-cheat reframe (the verified finding that changes the premise)

The product's stated reason to exist is "anti-cheat-safe by construction; must run on BF6" (DCAD:29-31).
**Author-verified first-hand this session, that premise is materially weaker than the repo assumes** — see
F1(d) for the cited evidence. The honest reframe:

1. **"Anti-cheat-safe by construction" overstates it.** No-injection removes the *strongest* signal (the
   hook) but not *all* signals. Safety is **heuristic, discretionary, and asymptotic against a vendor-
   controlled moving target** — not a binary you can certify, and no vendor (EAC/BattlEye/VAC/Riot/EA) has
   any published statement about FG tools specifically.
2. **The WDA-excluded overlay is a real cheat-signal — but it is SHARED with LSFG, not self-inflicted.**
   [★ Operator-observed 2026-06-22, Lightshot: LSFG's FPS overlay is uncapturable by software screenshot
   tools → LSFG uses the same display-affinity exclusion.] So this is **PARITY**, not a disadvantage; making
   our overlay capturable (T0) is now an *optional go-beyond-LSFG* lever, not a mandatory fix. (Strong
   evidence, not lab-confirmed — a `GetWindowDisplayAffinity` probe is definitive.)
3. **BF6/Javelin is the asymptotic-risk tier, not the safe tier.** Treat "must run on BF6" as a
   *requirement currently UNMET and contradicted by real (ambiguous) data*, and recommend against ranked/
   competitive use on kernel anti-cheats until the empirical campaign provides evidence.
4. **The honest posture is the per-engine trust matrix (F1-f)** with a measurable survived-account-hours
   metric — never "we believe it's safe."

This is `[VS]` and load-bearing; it MUST steer the framing of the whole product, not sit in a footnote.

---

## §4 — What the vista adds over the roadmap, and what was NOT a real gap

**The roadmap is sound as a build SEQUENCE** (its dependency spine P0→{P1,P3}, P4→S5, P5→{S1,S3,S4} is
correct). The vista adds the two layers it lacked: **(1)** the *complete objective set* — the roadmap was
organized as "where we lose to LSFG on the PERCEPTUAL axis", so it was structurally blind to the
non-perceptual families **F (trust)** and **E (coverage)** + **A3 (HDR)** + **C2 (the measured slice)**;
**(2)** the *closeable-fixed-point layer* (§2) — re-annotating each P-item with its binary test (P0→C6,
P1→C9, P3→C7, P5→C5, P6→C9, P7→C8) converts the roadmap from "an obsession with perfection" into "a
checklist of crossings + two named asymptotes (fill quality, metric SROCC)".

**What the completeness audit DEFLATED (NOT real product gaps — do not re-promote):** **I3** (substrate-
growth = project meta-framing, a perfect FG harvests zero FRs); **I1** (anti-obsolescence = a value in
tension with latency, not a movable objective); **C3** (power/thermal = a telemetry field, not a gate);
**D3 / G1-guarantee** (engineering hygiene already named as debt). The synthesis's headline ("the load-
bearing additions are F, E, and I3") was **2/3 right** — F and E are the real drops; **I3 is folklore.**

---

## §5 — Honest floors (the irreducibles, consolidated — never re-propose these as wins)

- **The interpolation HOLD** — both LSFG and we pay ~1 source frame by definition (only S5 extrapolation
  escapes it, behind P4, at the cost of prediction artifacts).
- **No Reflex/Anti-Lag hook** — the external class is structurally ~1–2 frames worse than in-engine FG on
  a GPU-bound single dGPU. **FG can never lower latency; the lever is variance/judder.**
- **NVIDIA G-Sync off for a non-hooking overlay** `[V1]` — shared exactly with LSFG; AFMF (driver) escapes
  it, we cannot. AMD/Intel VRR-with-overlay works.
- **The true-hole + moving-bg-behind-moving-object** — no non-neural cure; the whole field concedes it.
- **Exclusive-fullscreen capture** (bypasses DWM; only injection reaches it = ban) + **protected/HDCP
  content → black** for all OS capture `[V1]`.
- **Internal scRGB-FP16 arithmetic** — refused by NVIDIA/Intel on tensor HW; 1/64 rate on Pascal `[V1]`.
- **No HW-OFA on AMD/Intel** `[V1]` — OFA acceleration is intrinsically single-vendor.
- **No-GT FG-quality metric ceiling ~0.7–0.83 SROCC** `[V2]` — measurement of the perceptual axis is
  asymptotic; "done" is a declared threshold, never 1.0.
- **Full-reference comparison of LSFG** is impossible (WGC-excluded + unsynchronized) — the head-to-head
  is permanently photon-level no-reference.
- **L1 base-game cap** — structurally unavailable to an external tool AND blocked by the no-cap dogma.
- **Consumer-GPU queue preemption** — does NOT preempt a saturating dispatch (proven dead-end).
- **Anti-cheat certainty on kernel titles** — discretionary + server-side; asymptotic, not certifiable.

---

## §6 — Provenance, source levels, and what could NOT be verified

**Provenance:** the 9-family taxonomy + the folklore deflation come from workflow `wqdznsdsx` (objective-
space + completeness audit, 6 agents). The gap-family SOTA comes from workflow `w83r5ydl9` (6 agents,
~563 K tokens, web-grounded), which produced six dossier-grade analyses now being authored as the
companion `FG_*_PRIOR_ART.md` files (§1 links). The author re-verified the §3 anti-cheat **safety** claims
first-hand (`[VS]`, primary URLs in F1). Tracked-family facts inherit the existing dossiers
(FG_VFI / FG_OPEN_ALTERNATIVES / FG_CADENCE_LATENCY / FG_SATURATION_* / FG_NVOFA / FG_TELEMETRY,
GPU_MULTI_GPU_PRIOR_ART) and the gap-audit / DCAD plan.

**Could NOT be verified first-hand (read before acting):**
- ~~Whether LSFG's overlay is WDA-excluded or capturable~~ — **RESOLVED 2026-06-22 (operator, Lightshot):**
  LSFG's overlay is uncapturable by software → the same display-affinity exclusion as ours → **PARITY**
  (strong evidence, not lab-confirmed). Uncapturable by software ≠ by a camera/capture card.
- **Whether BF6/Javelin BANS vs merely LAUNCH-BLOCKS** external FG — the one reported LSFG ban was called
  "may have been applied incorrectly"; the causal link is unconfirmed in both directions.
- **Whether a pure-DComp child-visual (no classic layered HWND) evades the window-enumeration fingerprint**
  — untested; could be safer, OR the WDA flag could dominate regardless.
- **WGC's exact allowed `DirectXPixelFormat` set** — "WGC accepts R16F" is `[V2]` (MS recommends it for
  the HDR pool, but no hard allow-list found).
- **Whether the DComp premultiplied-alpha overlay can use 10bpc HDR10 via any workaround** — MS excludes
  alpha-blended swapchains `[V1]`; treated as blocked.
- **Whether PhyriadFG's default path actually runs on a single AMD/Intel GPU** — no first-hand evidence
  either way; the central E1 unknown.
- **PresentMon "All-Input-to-Photon" through a third-party DComp overlay** — metric exists; not confirmed
  it attributes latency correctly through an external overlay (probe before claiming C7 turnkey).
- **The BVI-VFI SROCC decimals** — `[V2]` corroborated band ~0.6–0.83; individual decimals dataset-fragile.
- **The full chains** of the OBS-WebSocket RCE Origin-check status, Moonlight's six-step pairing crypto,
  and Afterburner's wire crypto — security *lessons* are sound; specific mechanism details are `[V2/V3]`.

(The companion dossiers carry each family's full leveled source list + its own "could not verify" section.)

---

## §7 — Status & next

This document is the FIXED ENCUADRE the operator asked for: the complete objective space (9 families),
each objective resolved with its floor and its **closeable success criterion**, plus the §2 fixed-point
checklist that is the antidote to "no avanzamos". Implementation stays FROZEN. The companion gap dossiers
(§1 links, `FG_*_PRIOR_ART.md`) are authored and registered in
[`../FORMAL_DOCUMENTS_REGISTER.md`](../FORMAL_DOCUMENTS_REGISTER.md). The roadmap
([`PHYRIADFG_PERFECTION_ROADMAP.md`](PHYRIADFG_PERFECTION_ROADMAP.md)) remains the build SEQUENCE; this is
the objective CONSTITUTION above it.

The single highest-leverage non-code action is the **anti-cheat reframe (§3)** — it changes the product's
core premise and has a free T0 fix. The cheapest *crossing* is **C1** (measure the single-GPU slice). Both
are available the moment implementation un-freezes.
