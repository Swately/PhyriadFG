# FG_COMPETITIVE_COMPARISON — PhyriadFG vs the field, mapped to the objective vista

> **Diátaxis type:** Analysis (honest competitive audit). **Status:** `measured`/`shipping` for our own
> standings (tagged inline) · `[V1]/[V2]/[V3]` for competitor facts. Resolves the operator's ask: a
> **consolidated, full-scale** comparison of the existing + free frame-generators against PhyriadFG,
> mapped to the [`PHYRIADFG_OBJECTIVE_VISTA.md`](PHYRIADFG_OBJECTIVE_VISTA.md) — *where the field
> converges · where they fail · where we win · where we fail.* The prior partial work (the
> convergent-behaviour analysis in [`FG_OPEN_ALTERNATIVES_PRIOR_ART.md`](FG_OPEN_ALTERNATIVES_PRIOR_ART.md)
> + the field map in [`FG_VENDOR_LANDSCAPE_PRIOR_ART.md`](FG_VENDOR_LANDSCAPE_PRIOR_ART.md) + the vs-LSFG
> standing in the vista/roadmap §1) is consolidated here into the per-competitor × per-objective scorecard
> that did not exist before.
>
> **★ THE GROUND RULE (anti-inflation — the operator's veto, enforced by an adversarial audit):** a
> **"win" counts only if built AND measured-better.** Everything `designed`/`research`/`built-off-
> unvalidated` is a **BET, not a win** — tagged as such. The WE-LOSE half is authored as completely as
> the WE-WIN half (the operator's explicit requirement).
>
> **Provenance + verification.** From workflow `wygbafii3` (6 agents, ~440 K tokens: field-convergence +
> external-class + engine-integrated + our-position lenses → synthesis → an anti-inflation audit). The
> audit (APPROVE-WITH-FIXES) **demoted three matrix cells the prose already disavowed** (D1/D2 unbuilt
> "wins"), reclassed one competitor cell, added one omitted loss (F3), and softened one (F1) — **all six
> fixes applied here.** The load-bearing in-repo standings were re-verified first-hand this session
> (`DwmGetCompositionTimingInfo`=0, `SetColorSpace1`=0, WGC BGRA8 lock `main.cpp:3053`, the
> `min(c,1.0)` clip `hdr_convert.comp:32`, WDA-exclusion present, the synchronous present-fence
> `7013/7162/7521`). Per FDP §2 no fabricated competitor benchmark; §5 lists what could not be verified.
>
> **Normativity (BCP 14):** descriptive throughout; this is an audit, not a contract.

---

## §0 — The classes, and what a comparison can honestly say

The field splits in two, and the split governs every cell:

- **Class B — external / driver-level FG (PhyriadFG's ACTUAL class):** LSFG, AFMF2, OptiScaler/RIFE. No
  engine integration; forced onto any game's output. **LSFG is the apt peer + north star.**
- **Class A — engine-integrated FG:** DLSS-G / DLSS 4 MFG, FSR3-FG / FSR 4, XeSS-FG. They need engine
  motion-vectors + depth + HUD-less colour, and are (mostly) vendor/HW-locked → **they cannot be forced
  on an arbitrary game.** On raw per-frame quality + latency they beat *all* external tools (us AND
  LSFG) — but via an integration gate we structurally refuse. Most Class-A cells are therefore
  **THEY-WIN-but-MOOT (different class)**, not a head-to-head defeat.

So the honest head-to-head is **vs LSFG**; Class A is the quality/latency ceiling the whole external
class concedes by construction.

---

## §1 — THE MASTER MATRIX

Cell = verdict *vs PhyriadFG* + a 3-word why. Verdict vocab: **THEY-WIN** / **PARITY** / **WE-WIN** /
**WE-LEAD-IF-BUILT** (a designed bet, not a realized win) / **SHARED-FLOOR** / **N-A** (different class).
The **PhyriadFG** column = our maturity (`shipping`/`built`-off/`measured`/`designed`/`research`/`MISSING`).

| Vista objective | LSFG (peer) | DLSS-G/MFG | FSR3-FG | AFMF2 | XeSS-FG | OptiScaler/RIFE | **PhyriadFG (maturity)** |
|---|---|---|---|---|---|---|---|
| **A1 disoccl/fill** | THEY-WIN neural fill | THEY-WIN MVs+inpaint `(class)` | THEY-WIN dual-mask `(class)` | SHARED-FLOOR MV-free | THEY-WIN MVs `(class)` | THEY-WIN¹ MVs/neural | non-neural, color-deforms · `built`/`designed` |
| **A2 cadence** | PARITY (their DWM edge) | THEY-WIN flip-metering `(class)` | PARITY admitted-zigzag | WE-WIN worse-smoothness | THEY-WIN flip-metering `(class)` | PARITY no-assert-slice | pacer good, **no DWM** · `built` |
| **A3 HDR** | THEY-WIN ships-toggle | THEY-WIN native HDR `(class)` | THEY-WIN swapchain-HDR | THEY-WIN driver-chain | THEY-WIN engine-HDR | PARTIAL/THEY-WIN | **can't INGEST** · `MISSING` |
| **B1 latency** | SHARED-FLOOR no-Reflex | THEY-WIN Reflex-hook `(class)` | THEY-WIN AntiLag-hook `(class)` | THEY-WIN driver-hook | THEY-WIN Reflex-class | SHARED-FLOOR +1-hold | match-not-lead · `measured` (73→32.6 ms, **default-OFF**) |
| **C1 saturation** | WE-WIN no-assert-slice | MOOT in-engine | WE-WIN recommends-cap | WE-WIN self-disables-motion | MOOT in-engine | WE-WIN no-degrade | make-space · `built`, **default-OFF** |
| **C2 slice** | PARITY lightweight-both | N-A `(class)` | PARITY +1-hold | PARITY MV-free | N-A `(class)` | PARTIAL | **never measured** · `MISSING` |
| **D1 multi-GPU** | PARITY-pending⁴ | PARITY-pending⁴ | PARITY-pending⁴ | PARITY-pending⁴ | PARITY-pending⁴ | PARITY-pending⁴ | split **asserted-not-measured** · `built-off` |
| **D2 parallel instances** | WE-LEAD-IF-BUILT⁵ | WE-LEAD-IF-BUILT⁵ | WE-LEAD-IF-BUILT⁵ | WE-LEAD-IF-BUILT⁵ | WE-LEAD-IF-BUILT⁵ | WE-LEAD-IF-BUILT⁵ | the moat · **`designed`/`research`, 0 built** |
| **E1 vendor** | THEY-WIN verified-any-vendor | THEY-WIN¹ NVIDIA-only-def | PARITY open-portable | THEY-WIN AMD-deep | THEY-WIN least-locked | mixed | **NVIDIA-rig only** · `MISSING`/unverified |
| **E2 coverage** | THEY-WIN fallback-ladder-moat | WE-WIN integration-locked | WE-WIN integration-locked | PARTIAL² (AMD-deep, narrow) | WE-WIN per-title-locked | WE-WIN³ DX12/offline | any-game ships, **no matrix/ladder** · `shipping`/`MISSING` rigor |
| **F1 anti-cheat** | PARITY safety / THEY-WIN record | THEY-WIN vendor-blessed | THEY-WIN in-engine | THEY-WIN driver-blessed | THEY-WIN in-engine | WE-WIN wrapper=AC-risk | **no track record** · `MISSING` matrix |
| **F2 VRR** | SHARED-FLOOR NV-GSync-off | THEY-WIN keeps-VRR `(class)` | THEY-WIN keeps-VRR | THEY-WIN driver-keeps-VRR | THEY-WIN keeps-VRR | N-A | lost on NVIDIA · **FLOOR** / AMD-untested |
| **F3 security** | THEY-WIN shipped-closed | N-A | N-A | N-A | N-A | N-A | **auth DEFERRED** · `designed`, future surface |
| **G1 UX** | THEY-WIN two-click | THEY-WIN driver-toggle | THEY-WIN driver-toggle | THEY-WIN driver-toggle | THEY-WIN driver-toggle | THEY-WIN/mixed | CLI ~61 flags · `designed` GUI (Tauri/React) |
| **H1 self-measure** | WE-WIN WGC-can't-self-score | WE-WIN no-overlay-score | WE-WIN no-self-score | WE-WIN no-self-score | WE-WIN no-self-score | PARITY offline-metrics | full-ref scorer · `built`, **not-in-CI/uncalibrated** |
| **H2 head-to-head** | SHARED-FLOOR NR-only | SHARED-FLOOR | SHARED-FLOOR | SHARED-FLOOR | SHARED-FLOOR | SHARED-FLOOR | PresentMon doable · `designed`, **unrun** |

**Footnotes.** ¹ DLSS NVIDIA-locked / OptiFG only where engine MVs exist. ² **AFMF is AMD-GPU-locked
(deep-but-narrow)** — a *different axis* (vendor-depth) from coverage-breadth, not a clean win over us.
³ OptiFG DX12-only; RIFE per-clip offline, not live-class. ⁴ **PARITY-pending (audit fix #2):** the
competitors' single-adapter limit is real, but **our** multi-GPU split is *asserted, not A/B-measured*,
and the router is effectively dead in the FG app — `break_even_decide` has **0 call sites in
`apps/render_assistant`** (the one live caller is `framework/holons/RoutingPolicy.hpp:70`; `main.cpp:50`
is a comment that it was evaluated and not used) → a bet, not a win. ⁵ **WE-LEAD-IF-BUILT (audit fix
#1):** the headline differentiator LSFG + Class-A *structurally cannot follow* — but **0 lines built, no
Tier-2 plan**. A `designed` bet; the matrix must not paint a green win on unwritten code.

---

## §2 — Where the FIELD CONVERGES (the shared behaviours + the shared floors)

A convergent behaviour is a likely-correct design the field independently arrived at — **sitting outside
it is a defect, not a differentiator.** A shared floor is structural — **never a place to win, never a
place to call a loss against a Class-B peer.**

### Six convergent behaviours

| # | Convergent behaviour | Field consensus | Where PhyriadFG sits |
|---|---|---|---|
| **CB1** | Hard-pick ONE direction at disocclusion, never blend garbage | FSR3 dual-mask, RIFE fusion mask, GFFE classify-then-fill `[V1]` | **PARTIALLY inside** — coarse backward field, no per-pixel fwd/bwd consistency map; reveal-fill still color-deforms (a defect) |
| **CB2** | Degrade gracefully — assert the slice, never self-disable | FSR3/RIFE/LSFG keep emitting; AFMF's hard cut is its documented worst failure `[V1]` | **INSIDE** (our correct instinct) — `--target-output-fps`/make-space, but `built-off` (default present is synchronous) |
| **CB3** | Pace to a wall-clock MA target + spin, metered on display-change | FSR3 MA + spin + `MsBetweenDisplayChange`; DLSS-G SL pacer; LSFG on the DWM clock `[V1/V2]` | **MOSTLY inside, with a GAP** — we pace self-anchored; **not metered on the DWM clock** (`DwmGetCompositionTimingInfo`=0 first-hand) |
| **CB4** | Own present timing; don't rely on VRR | all own present; VRR architecturally blocked for a DComp overlay; LSFG disables G-Sync `[V1/V2]` | **INSIDE** — we own present timing; VRR-via-overlay correctly abandoned |
| **CB5** | Bound generation cost via reduced internal flow resolution | LSFG "Flow Scale", FSR3/DLSS principle `[V1]` | **INSIDE** — `--flow-scale` (static); runtime-adaptive is roadmap P3 (`designed`) |
| **CB6** | Generate candidates + pick by a no-GT proxy | FSR3 priority-keyed scatter; RIFE refine; VFI selection is provably ill-posed w/o GT `[V1]` | **INSIDE, arguably ahead** — medoid consensus + a Sobel image-edge gate (no shipping FG has one) — but **`built-off` + unvalidated → novelty ≠ win** |

### The shared floors (irreducible — everyone hits them)

- **SF-latency:** the +1-frame interpolation hold + no Reflex/Anti-Lag hook → **FG can never *lower*
  latency.** (Driver/engine tools partially escape — §4.)
- **SF-VRR:** NVIDIA G-Sync doesn't trigger for a non-hooking overlay — **LSFG hits this identically and
  disables G-Sync.**
- **SF-disoccl-hole:** the true-hole + moving-bg-behind-moving-object — no non-neural cure; the whole
  field concedes it.
- **SF-capture:** exclusive-fullscreen bypasses DWM; HDCP → black for all OS capture.
- **SF-no-GT:** live FG output has no ground truth → quality is permanently no-reference; the metric
  ceiling (~0.7–0.83 SROCC) binds everyone.
- **SF-preemption:** consumer GPUs don't preempt a saturating dispatch → everyone uses make-space, not
  priority.
- **SF-L1-cap:** the base-game fps cap (the primary saturation lever) is unavailable to any external tool.

**Against LSFG specifically, the relationship on these floors is PARITY** — same class, same floors,
mostly the same behaviours.

---

## §3 — The three-bucket verdict

### (a) WHERE WE GENUINELY WIN — REALIZED (built + measured/shipping)

1. **H1 self-measurement (`built`)** — `fg_quality_scorer` does true full-reference scoring of our own
   output; **LSFG structurally cannot** (its overlay is WGC-excluded). A real structural capability gap.
   *Caveat (kept per audit fix #6): not wired into CI, not calibrated to human MOS → a tooling win, not
   yet a product win.*
2. **A1 static-HUD / overlay separation (`shipping`)** — HUD/text doesn't ghost; a real, modest, shipped
   win on a known MV-free failure mode the whole external class concedes.
3. **B1 async-present decouple (`measured`)** — 73→32.6 ms in BF6 — but **ships default-OFF**, so the
   default user doesn't get it; it closes our self-inflicted excess to *parity*, not past LSFG.
4. **C1 assert-the-slice vs AFMF (behavioural, `built`)** — AFMF's documented worst failure is
   self-disabling in fast motion; we are architected to do the opposite. Real as a behavioural win; ships
   default-OFF.

### (b) BETS, NOT WINS (designed/research/built-off-unvalidated — the moat-candidates, all unrealized)

- **D2 parallel FG instances per window/screen** — the headline differentiator LSFG + Class-A
  *structurally cannot follow*. **0 lines built, no Tier-2 plan.** The biggest *potential* moat; today a
  design claim only.
- **D1 measured net-positive multi-GPU break-even gate** — the edge over LSFG's ungated dual-GPU; the
  router is effectively dead in the FG app (0 `break_even_decide` call sites in `apps/render_assistant`).
  Built primitives, unwired.
- **CB6 image-field Sobel edge-gate** — genuinely outside convergence (no shipping FG has one); `built-off`
  + **unvalidated** → novelty is not a win until a measured held-out scorer win.

### (c) WHERE WE LOSE / FAIL — by severity

**CRITICAL — binary feature absent / product-defining (the gap between "real product" and "great on the
author's NVIDIA pair"):**
- **A3 HDR — cannot even INGEST.** WGC hard-locked BGRA8 (`main.cpp:3053`); `SetColorSpace1`=0; tone-map
  is a hard `min(c,1.0)` clip (`hdr_convert.comp:32`). The one dimension where a marketed rival feature
  is simply *absent* (LSFG ships a working HDR toggle). `MISSING`.
- **E2 coverage / the fallback-ladder.** LSFG's *actual* moat = the DXGI/WGC/GDI auto-ladder + years of
  per-title validation — **non-algorithmic.** We have WGC + a legacy DD fallback; no GDI rung, no per-OS
  auto-switch, no validated matrix, no graceful-fail-with-reason; tested on BF6 + Star Rail only. The
  single widest gap. `MISSING` rigor.
- **E1 vendor-agnosticism — UNVERIFIED.** `GpuDescriptor` carries `compute_gflops` only (no fp16/DP4a
  field) → can't even *represent* a non-NVIDIA GPU; the break-even constant is the author's 4090 value;
  no cross-vendor run ever. "Great on the author's NVIDIA pair only." `MISSING`. (XeSS-FG's DP4a fallback
  proves the portable path we lack.)
- **F3 control-plane security — the only no-floor objective, currently `MISSING` (audit fix #4).** Auth /
  security is *explicitly deferred* (`CONTROLPLANE_MASTER_PLAN.md:99`) while sensitive fields are already
  in the working set (window TITLES `main.cpp:1839`, PID `main.cpp:6887`, GPU UUIDs, NVML). A **future
  attack surface we are choosing to open** the moment the WebSocket/mobile transport ships — an honesty
  debt with no structural excuse (LSFG carries zero telemetry egress).

**ASYMPTOTIC / QUALITY losses (real, algorithmic, mostly conceded):**
- **A1 disocclusion FILL — THEY-WIN.** LSFG's lightweight-neural reconstruct beats our non-neural
  color-deforming fill; eye-tests concede the gravity/disocclusion frontier. (SHARED-FLOOR on the
  true-hole; LSFG beats us *above* that floor.)
- **G1 UX** — CLI-only ~61 flags vs two-click + per-game profiles. `designed`.
- **F1 anti-cheat track record** — "must run on BF6" is a *requirement, not a verified capability*; zero
  first-hand evidence on any kernel-AC title; our survived-account-hours = 0 vs LSFG's years. The
  WDA-exclusion is **probable-PARITY with LSFG, NOT lab-confirmed** (audit fix #5 — Lightshot is strong
  evidence; a `GetWindowDisplayAffinity` probe is definitive; software-uncapturable ≠ camera-uncapturable).

**UNVERIFIED / UNMEASURED / UNBUILT (honesty debt — cheap to close, currently open):**
- **C2 single-GPU slice NEVER MEASURED** — the load-bearing unknown deciding whether the *majority-case
  single-GPU user* is net-positive at all. The cheapest crossing, still open.
- **A2 DWM metering MISSING** — pacer self-anchored, not metered on the compose clock (P0 ≈ 1 API + 1
  EWMA term).
- **H2 no PresentMon head-to-head vs LSFG** — doable now (FG-agnostic), unrun; head-to-head claims are
  eye, not instrument.

**STRUCTURAL losses vs the DRIVER/ENGINE class (NOT vs LSFG — class-boundary, not algorithm):**
- **B1 latency** — AFMF (driver hook + Anti-Lag) and DLSS-G (Reflex) structurally beat both us *and* LSFG.
- **F2 VRR** — AFMF/DLSS-G keep G-Sync; we and LSFG both lose it on NVIDIA (SHARED-FLOOR with LSFG,
  THEY-WIN vs the driver class).
- **A1/A3 in-engine** — Class A's MVs+depth+pre-tonemap HDR are bought with an integration gate we
  refuse; MOOT, not a head-to-head defeat.

---

## §4 — The one-line honest positioning

**What PhyriadFG IS today:** a Class-B external FG that sits **squarely inside LSFG's tier** — same
floors, mostly the same behaviours — with **two narrow realized wins** (shipping static-HUD separation; a
built self-scoring instrument LSFG structurally can't match), an **off-by-default measured latency
decouple**, and **three product-defining gaps** (HDR can't-ingest, no coverage fallback-ladder,
vendor-breadth unverified → "great on the author's NVIDIA pair").

**What it is DESIGNED to become:** the one external FG that does what LSFG and the in-engine giants
*architecturally cannot* — **independent parallel FG instances across windows/screens (D2)** on a
**measured net-positive multi-GPU split (D1)**, self-validated in CI — **but every one of those
moat-claims is `designed`/`built-off`, zero of them measured.** No convergent behaviour or shared floor
is a place we presently beat LSFG in a built-and-measured way. **The cheapest real progress is
*measuring* (C2 single-GPU slice, H2 PresentMon-vs-LSFG) and *closing the HDR/coverage ingest gaps* —
none of which requires beating LSFG's neural fill.**

---

## §5 — What could NOT be verified + provenance

- **The competitor facts** carry the `[V1]/[V2]/[V3]` levels of `FG_VENDOR_LANDSCAPE_PRIOR_ART` /
  `FG_OPEN_ALTERNATIVES_PRIOR_ART` (LSFG specifics are `[V2]`, closed-source; vendor docs `[V1]`); they
  were not all re-fetched first-hand this pass.
- **The vista's "router = 1 caller `[VS]`"** is slightly imprecise: first-hand, `break_even_decide` has
  **0 call sites in `apps/render_assistant`** (the FG app) and **1 in `framework/holons/RoutingPolicy.hpp`**
  — i.e. the router is wired into the holons pillar, not the FG. The conclusion ("dead in the FG") is
  *strengthened*, not weakened; a future vista pass should sharpen the wording.
- **F1 WDA-parity** is operator-observed (Lightshot), **not lab-confirmed** — a `GetWindowDisplayAffinity`
  probe on the LSFG window is the definitive check (see `FG_TRUST_ANTICHEAT_PRIOR_ART §5`).
- **Our own standings** (`DwmGetCompositionTimingInfo`=0, `SetColorSpace1`=0, BGRA8 lock, the clip, WDA,
  the present-fence) were re-verified first-hand this session.

**Provenance:** workflow `wygbafii3` (convergence + external-class + engine-integrated + our-position →
synthesis → anti-inflation audit, APPROVE-WITH-FIXES). The six audit fixes (demote D1/D2 unbuilt "wins";
reclass E2/AFMF; add F3 to the losses; soften F1; keep H1 tagged) are applied above. Consolidates
`FG_OPEN_ALTERNATIVES_PRIOR_ART` (convergence) + `FG_VENDOR_LANDSCAPE_PRIOR_ART` (field map) + the vista
§1 (vs-LSFG standing); does not duplicate their SOTA.

## §6 — Cross-links

The objective definitions + closeable criteria → [`PHYRIADFG_OBJECTIVE_VISTA.md`](PHYRIADFG_OBJECTIVE_VISTA.md).
The build plan that closes these gaps → [`PHYRIADFG_MASTER_PLAN.md`](PHYRIADFG_MASTER_PLAN.md). The field
map → [`FG_VENDOR_LANDSCAPE_PRIOR_ART.md`](FG_VENDOR_LANDSCAPE_PRIOR_ART.md); the convergent behaviour →
[`FG_OPEN_ALTERNATIVES_PRIOR_ART.md`](FG_OPEN_ALTERNATIVES_PRIOR_ART.md); the anti-cheat detail →
[`FG_TRUST_ANTICHEAT_PRIOR_ART.md`](FG_TRUST_ANTICHEAT_PRIOR_ART.md); HDR →
[`FG_HDR_FORMAT_PRIOR_ART.md`](FG_HDR_FORMAT_PRIOR_ART.md); coverage →
[`FG_COMPATIBILITY_COVERAGE_PRIOR_ART.md`](FG_COMPATIBILITY_COVERAGE_PRIOR_ART.md).
