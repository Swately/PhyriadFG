# PHYRIADFG_RISK_REGISTER — the Tier-2 triad risk register (cross-family)

> **Diátaxis type:** Reference (normative). This is the **risk-bearing** leg of the PhyriadFG
> master-plan triad — the THIRD document, paired with
> [`PHYRIADFG_MASTER_PLAN.md`](PHYRIADFG_MASTER_PLAN.md) (the *what + contract*) and
> [`PHYRIADFG_IMPLEMENTATION_STRATEGIES.md`](PHYRIADFG_IMPLEMENTATION_STRATEGIES.md) (the *how*),
> the three bound to ONE anti-drift spine (§0).
>
> **What this file owns.** The NEW risk surfaces of the cross-family objectives that have no
> existing per-feature register, and the CONSOLIDATED INDEX that reconciles them against the
> four EXISTING registers. It **INDEXES** — never duplicates — the existing registers
> (`FG_SATURATION_STABILITY_RISK_REGISTER`, `SINGLE_GPU_COLLAPSE_RISK_REGISTER`,
> `DEVICE_LOST_RECOVERY_RISK_REGISTER`, `REAL_FAST_PATH_RISK_REGISTER`).
>
> **THE HARD GATE (PLAN_TIER_PROTOCOL, Tier-2).** **No Tier-2 change MUST be committed while any
> of its risks is `open`.** Each risk MUST be either `mitigated`-as-code (a code-level mitigation
> exists + a first-hand verification has passed) or explicitly `accepted` (the operator has
> recorded the residual). **Nothing in this register is built yet** — every new risk is therefore
> `designed`/`open`: the mitigations below are the *designs* that MUST be realized AS CODE and
> first-hand-verified before the owning objective may commit. A `designed` mitigation is NOT a
> discharge.
>
> **Verifiable-reference mandate (FDP).** Every `file:line` here is copied VERBATIM from the
> first-hand-grounded cluster plans; it is NOT re-minted in this pass. Lines drift every session
> (see the spine's DRIFT-1/2/3) — the build agent MUST re-grep each site before editing it. No
> benchmark number is minted here; numbers cite the SOTA dossier or `bf6_p1_baseline.csv` /
> `docs/planning/BENCHMARK_FAIRNESS.md`.

---

## §0 — The binding spine (the anti-drift invariant — all three triad docs hold this VERBATIM)

> **This triad operationalizes `PHYRIADFG_OBJECTIVE_VISTA.md` (the 9-family objective constitution A1–H2 + the C1–C10 closeable fixed points + the FIXED-POINT-vs-ASYMPTOTE rule) into a buildable, dependency-ordered, risk-bearing plan.** It sits BELOW the vista (which says *what*) and ABOVE the per-feature dossiers + the existing per-feature plans (which own the *SOTA* and the *per-gap how*). It is the UNIFYING layer: it INDEXES the dossiers (`FG_*_PRIOR_ART`), the architecture plan (`FG_ARCHITECTURE_DCAD_MASTER_PLAN`), the per-feature triads (`FG_SATURATION_STABILITY_*`, `SINGLE_GPU_COLLAPSE_RISK_REGISTER`, `DEVICE_LOST_RECOVERY_RISK_REGISTER`, `REAL_FAST_PATH_*`, `INPUT_LAG_DREDUCTION_MASTER_PLAN`, `CONTROLPLANE_MASTER_PLAN`, `PHYRIADFG_UI_MASTER_PLAN`), and the cross-dimension `PHYRIADFG_PERFECTION_ROADMAP` (the P0–P7/S1–S6 sequence + the §5 dependency spine + the §10 honest floors); it re-derives none of them. Where the roadmap and the vista disagree with first-hand HEAD, **HEAD wins and the divergence is logged in §6 (CONFLICTS/DEDUP).**
>
> **Binding invariants (every objective, every wave, every commit MUST uphold — non-negotiable):**
> - **Byte-identical-off.** Every new lever is a default-off flag in `parse_extra` (NOT the main parse chain — C1061); the default binary is bit-identical to current. Verified by a `--csv`/pixel diff (off-run == baseline), never asserted. The lone exceptions are explicitly measured-gated, not off-proven: A1·S2 field-quality changes to default-on consumers (`bg_snap`/`band_xfade`), and any default-flip — each gated on a committed held-out scorer win, not a byte-proof.
> - **No-cap dogma.** Nothing caps, throttles, or recommends-capping the captured game's render rate. The make-space / fractional-controller / governor work asserts the FG slice (LSFG-AFG-style target-output-fps), never L1 base-game-cap.
> - **Verify-before-claim (FDP verifiable-reference mandate).** No fabricated path/line/API/benchmark; every code site is grepped first-hand before it is edited (lines drift every session — DRIFT-1/2/3 below are proof); every cited dossier number inherits its `[V1]/[V2]/[V3]` level and is not re-minted. The single benchmark source of truth is `docs/planning/BENCHMARK_FAIRNESS.md` — committed numbers route there, never duplicated.
> - **FIXED-POINT-vs-ASYMPTOTE labelling.** Every objective declares one: a FIXED-POINT carries a *binary* closeable test (the C1–C10 crossings); an ASYMPTOTE carries a *single tracked metric + a floor* and an operator-declared "enough" — never a false "done". The triad does not promise to close an asymptote.
> - **Efficiency mandate.** Every hot-path lever is measure-gated; "if you can't measure the hot path you can't claim it's fast." Zero-overhead-off over defensive-on.
> - **Tier ceremony matches blast radius.** T0 → no plan; T1 → this MASTER_PLAN + IMPLEMENTATION_STRATEGIES; T2 → also a RISK_REGISTER, and **no T2 commit while any risk is `open`** (each `mitigated`-as-code or explicitly `accepted`).
>
> **Honesty reconciliations carried forward (HEAD `8664445`, verified this pass — these correct stale vista/roadmap/dossier text):**
> - **DRIFT-1** — the synchronous present-fence sites moved `7001/7150/7507` (roadmap §11) → **`7013/7162/7521`** at HEAD and are already `vk_live`-wrapped (the device-loss hole there is closed; the open async-poll hole is `7228/7628/7675`, `mitigated 3f6cbd2`).
> - **DRIFT-2** — the single-GPU slice (C2) is **no longer MISSING**: `measured` on the 4090 (~20–22 ms) per `SINGLE_GPU_COLLAPSE_RISK_REGISTER §2.1`; the open remainder is the 1080p/1440p/4K sweep + a small-GPU VRAM number.
> - **DRIFT-3** — the fractional controller (P1/C9, STAGE-119 `--target-output-fps`) is **BUILT** default-off (`main.cpp:7844–8428`); open = the flip + the motion-collapse validation, not the build.
> - The **control-plane pillar is built through CP0/CP1/CP3** (`b3253a7`); CP2 (FG adoption) is genuinely unwired (`controlplane` grep in `apps/render_assistant` = 0). The wire POD `TelemetryFrame` carries **no title/PID** → F3a is host-data-light by construction; near-term F3a work is *classification discipline*, not redaction.
> - **F1 WDA divergence collapsed to PARITY** (Lightshot, 2026-06-22) — LSFG is also display-affinity-excluded; the capturable-overlay lever is OPTIONAL go-beyond, not a fix.
> - **G1 frontend = Tauri + React** (CURRENT — operator's 2026-06-21 decision, `CONTROLPLANE_MASTER_PLAN.md:18` "PhyriadFG's UI is a Tauri + React app"). `PHYRIADFG_UI_MASTER_PLAN.md:49` is STALE (still ImGui/GLFW, pre-pivot) and MUST be updated; do not author ImGui.

---

## §1 — CONSOLIDATED RISK INDEX (the existing registers INDEXED + the NEW rows)

The four existing per-feature registers are cited, **not** duplicated. The NEW rows are detailed
in full in §2. **De-dup applied (synth §6):** the `GpuDescriptor`-POD risk pair, the fp16-blind
mis-route pair, and the startup-probe device-loss pair were each flagged independently by the D and
E clusters → each is ONE cross-linked row here (`R-D1-1 ≡ R-E1a-1`, etc.).

| Risk ID | Objective | Class | Register (CITE existing / NEW = §2 here) | Status | Detailed in |
|---|---|---|---|---|---|
| CR1–CR5 | P3 / C1·STEP3 / B1 async-present | crash / correctness / dogma | **`FG_SATURATION_STABILITY_RISK_REGISTER`** (existing) | CR1/2/4/5 `mitigated`, CR3 `accepted` | that register (do not duplicate) |
| CR6–CR9 | C1·STEP4 make-space router | crash / POD / usability / perf | **`FG_SATURATION_STABILITY_RISK_REGISTER`** (existing) | all `open`, operator-gated | that register |
| (discharged) | C2 / D1-collapse | crash / concurrency | **`SINGLE_GPU_COLLAPSE_RISK_REGISTER`** (existing) | `mitigated`/`accepted` (2026-06-20) | that register |
| (discharged) | D2-R3 / device-loss base | crash / device-loss | **`DEVICE_LOST_RECOVERY_RISK_REGISTER`** (existing) | discharged (2026-06-20) | that register |
| (parent) | B1 `--rfp` real-fast-path | latency / crash | **`REAL_FAST_PATH_RISK_REGISTER`** (existing, CR1–5/FR1) | per that register | that register |
| **R-A3-1** | A3·L4 FP16 present | device-loss (D3D11↔VK shared-tex format mismatch) | **NEW → §2.1** | `designed` / `open` | §2.1 |
| **R-A3-2** | A3·L4 FP16 present | dogma / correctness (10bpc alpha-exclusion invariant) | **NEW → §2.1** | `designed` / `open` | §2.1 |
| **R-A3-3** | A3·L4 FP16 present | perf (64 bpp bandwidth / slice regress) | **NEW → §2.1** | `designed` / `open` | §2.1 |
| **R-D1-1 ≡ R-E1a-1** | D1 / E1a (P5) | POD-ABI (`GpuDescriptor` cross-process layout) | **NEW → §2.2 — ONE row, cross-linked from D1 + E1** | `designed` / `open`, operator-gated | §2.2 |
| **R-D1-2 ≡ R-E1a-2** | D1 / E1a (P5) | data-integrity (fp16-blind mis-route; characterization-fail collapse) | **NEW → §2.2**; inherits the `SINGLE_GPU_COLLAPSE` collapse path | `designed` / `open` | §2.2 |
| **R-D1-3 ≡ R-E1a-3** | D1 / E1a (P5) | crash (device-loss during startup fp16 probe) | **NEW → §2.2**; inherits `DEVICE_LOST_RECOVERY` | `designed` / `open` (depends on DCAD P0) | §2.2 |
| **R-D2-1** | D2 independent instances | concurrency (cross-instance shared-singleton corruption) | **NEW → §2.3** | `designed` / `open` | §2.3 |
| **R-D2-2** | D2 | correctness (present-authority / SLI runt-frames) | **NEW → §2.3** | `designed` / `open` | §2.3 |
| **R-D2-3** | D2 | crash / device-loss (partial collapse) | **NEW → §2.3**; EXTENDS `DEVICE_LOST_RECOVERY` to per-instance | `designed` / `open` | §2.3 |
| **R-D2-4** | D2 | perf (resource contention, no net benefit) | **NEW → §2.3** (discharge = the D2.f two-CSV test) | `designed` / `open` | §2.3 |
| **CP-CR1..CP6** | F3b-L3 mobile pairing | RCE / concurrency (auth, CSWSH/Origin, no-bind-auth-off, pairing-crypto, command vocab, listener↔ring race) | **NEW → §2.4** (here for now; MAY split into a dedicated `CONTROLPLANE_RISK_REGISTER` when F3b-L3 is built) | all `designed` / `open` | §2.4 |
| **R-G1-1** | G1 CP2 egress | concurrency (present-thread egress push) | **NEW → §2.5**; reconcile with `CONTROLPLANE_MASTER_PLAN.md §7` | `designed` / `open` | §2.5 |
| (probe) | E2-PRESENT displayable DComp surface | correctness probe | **NEW → §2.6** (home decided HERE — the triad register) | `designed` / `open` | §2.6 |

**T2 items requiring a `mitigated`/`accepted` register before commit:** P3 (existing register, satisfied),
P5 / E1a / D1 (§2.2 — NEW), D2 (§2.3 — NEW), A3·L4 (§2.1 — NEW), F3b-L3 (§2.4 — NEW),
C1·STEP4 (existing register, `open`). **D3** is T1 hygiene with **no standalone register** — its
risk-bearing parts are R-D1-1 (POD) and R-D2-1..4, registered here under D1/D2. **S4-neural** is
T2-research but **build-deferred** (needs its own dedicated SOTA + P4 + P5 first) — recorded as a
downstream node, not a build-ready risk surface. **H1, H2** are register-clean (the scorer is an
offline read-only tool; PresentMon is external) — their only caveats (GPU-runner availability,
the PresentMon-through-DComp attribution probe) are logistics/verification dependencies, not
register failure modes.

---

## §2 — THE NEW RISK ENTRIES (in full)

Every entry below is `designed` and `open`. The mitigation is a *design that MUST be realized AS
CODE*; the verification plan MUST pass first-hand before the owning objective commits. The Tier-2
hard gate (§0) binds.

### §2.1 — A3·L4 (FP16 scRGB present) — `R-A3-1..3`

**Owner objective:** A3 lever 4 (true HDR present, `R16G16B16A16_FLOAT` scRGB). **Tier:** T2.
**No existing register covers HDR/present-format** (the existing FG registers are
`FG_SATURATION_STABILITY` and `SINGLE_GPU_COLLAPSE` — neither touches HDR) → these three are
genuinely NEW and homed here.

#### R-A3-1 — D3D11↔Vulkan shared-texture format mismatch / device-loss
- **Failure mode + site.** The cross-process bridge uses keyed-mutex shared textures
  (`IDXGIKeyedMutex` at `PresentSurface.cpp:135`; the bridge images at `main.cpp:4707/4723/4752/4765`,
  all `B8G8R8A8_UNORM` today). Changing the present/bridge format to FP16 on one side but not the
  other → undefined sampling or **device-loss** on a shared keyed-mutex texture.
- **Mitigation AS CODE (designed).** A single `present_fmt` constant threaded through the present
  surface (`PresentSurface.cpp:206`), the bridge D3D11 `btd.Format`, and the Vulkan `ici.format`
  (the four `main.cpp:4707/4723/4752/4765` sites), with a **startup assert** that the D3D11 and
  Vulkan formats are interop-compatible (`B8G8R8A8`↔`R16G16B16A16` validated against the DXGI/VK
  interop table) **before the first present**.
- **First-hand verification plan.** Run the FP16 path under the `vk_live()` watchdog; confirm no
  device-loss over a 30 s capture + a pixel-correct present (no garbage). Grep every reader/writer
  of the four bridge-image sites to confirm the single-constant threading is complete.
- **Status:** `designed` / `open`.

#### R-A3-2 — alpha-mode regression (the Option-2 10bpc exclusion is real, a dogma/correctness invariant)
- **Failure mode + site.** If a future change tries 10bpc HDR10 (`R10G10B10A2_UNORM`) it will
  silently break the premultiplied-alpha DComp present — `DXGI_ALPHA_MODE_PREMULTIPLIED` at
  `PresentSurface.cpp:222` (confirmed first-hand). MS excludes alpha-blended swapchains from 10bpc
  Advanced-Color → the only correct HDR present path for this surface is Option 1 (FP16 scRGB).
- **Mitigation AS CODE (designed).** A compile-time / static guard that the HDR present path
  asserts `R16G16B16A16_FLOAT` (Option 1) and **refuses** `R10G10B10A2_UNORM` for the composition
  (alpha) style — encode the dossier's `[VS]` exclusion as an *invariant*, not a comment. The hard
  clip this path replaces is at `hdr_convert.comp:32` (verified first-hand; the dossier's `:32`
  cite was correct).
- **First-hand verification plan.** Attempt to construct the `10bpc + composition` config → it MUST
  fail at build/startup (a `static_assert` or a constructor-time reject), proving the invalid pairing
  is non-constructible.
- **Status:** `designed` / `open`.

#### R-A3-3 — bandwidth / slice regression from 64 bpp
- **Failure mode + site.** Doubling present + bridge bandwidth (BGRA8 32 bpp → FP16 64 bpp at the
  four bridge sites + `PresentSurface.cpp:206`) may regress the C2 slice and the saturation behavior.
- **Mitigation AS CODE (designed).** The FP16 present path is **opt-in** (default BGRA8,
  byte-identical-off), measure-gated — a committed `--csv` slice comparison BGRA8-vs-FP16 MUST clear
  before any default flip.
- **First-hand verification plan.** The `--csv` slice harness on the author rig; the committed
  BGRA8-vs-FP16 slice delta routes to `docs/planning/BENCHMARK_FAIRNESS.md` (no number minted here).
- **Status:** `designed` / `open`.

---

### §2.2 — P5 router POD + wire (the shared D1 / E1a surface) — `R-D1-1..3 ≡ R-E1a-1..3`

**Owner objective:** P5 (the wired router) — ONE objective; **D1** (the multi-GPU A/B measurement)
and **E1a** (the vendor-agnostic ranking) are two CONSUMERS that each flagged the same three risks.
Per synth §6 #2 these are **single cross-linked rows**, homed here. **Tier:** T2.
**Reconcile (do not duplicate):** `SINGLE_GPU_COLLAPSE_RISK_REGISTER` (the collapse path inverted
here; discharged) and `DEVICE_LOST_RECOVERY_RISK_REGISTER` (the DEGRADE path; discharged).

#### R-D1-1 ≡ R-E1a-1 — POD-ABI break (`GpuDescriptor` layout)
- **Failure mode + site.** `GpuDescriptor` is `static_assert(sizeof==128u)` + `PHYRIAD_ASSERT_POD` +
  "published cross-process" (`GpuDescriptor.hpp`). Adding the fp16/DP4a field P5 needs can break
  SHM/IPC consumers and silently mis-parse a cross-process catalog.
- **Mitigation AS CODE (designed).** Consume the existing `_pad1[5]` (no size change) OR grow with a
  new `static_assert(sizeof==N)` + a bumped layout-version byte read by every consumer; keep
  `characterized()` semantics. Operator-approval-gated (DCAD §9.6).
- **First-hand verification plan.** Grep every reader of `GpuDescriptor` across the SHM/bridge
  boundary; run `framework/holons/tests/test_routing.cpp` + `test_heterogeneous.cpp` green; confirm
  `sizeof`/`alignof`/POD asserts compile.
- **Status:** `designed` / `open`, operator-gated. **Cross-linked from both D1 and E1a.**

#### R-D1-2 ≡ R-E1a-2 — router on an uncharacterized / mis-characterized catalog mis-routes FLOW (the fp16-blind hazard)
- **Failure mode + site.** Ranking FLOW on `compute_gflops` alone (the only compute field today)
  routes a raster-fast / fp16-slow card (Pascal fp16 = 1/64 fp32, DCAD §2.2) onto FLOW →
  sub-game-rate flow. The hardcoded selection it replaces is `main.cpp:3154-3180`.
- **Mitigation AS CODE (designed).** ASSIGN ranks FLOW by the new fp16/DP4a field, **never**
  `compute_gflops`; an uncharacterized catalog → supervisor-only (`RoutingPolicy.hpp:19-21`); the
  decision is LOGGED (DCAD §5 STEP D). Inherits the `SINGLE_GPU_COLLAPSE` collapse path as the
  degenerate.
- **First-hand verification plan.** Force an uncharacterized catalog → assert supervisor-only
  placement == today's single-device path (byte-identical-off); diff the chosen assignment against
  the current ternary at `main.cpp:3180`.
- **Status:** `designed` / `open`.

#### R-D1-3 ≡ R-E1a-3 — startup characterization on the hot path / device-loss during the fp16 probe
- **Failure mode + site.** A probe dispatch on a flaky device could TDR at startup.
- **Mitigation AS CODE (designed).** The probe runs ONCE off the hot path (DCAD §2 "four phases run
  ONCE at startup"); wrap in the existing `vk_live()` / `VK_ERROR_DEVICE_LOST` graceful-exit
  (`main.cpp:2895`) → DEGRADE to FG-OFF passthrough (DCAD §2.4). Indexes
  `DEVICE_LOST_RECOVERY_RISK_REGISTER` (the mechanism is shipping/discharged) — does not duplicate.
- **First-hand verification plan.** Force a device-loss during the probe → graceful FG-OFF exit, no
  crash; depends on DCAD P0 (device-loss, shipping).
- **Status:** `designed` / `open` (the device-loss mechanism it leans on is shipping/discharged).

---

### §2.3 — D2 (independent FG instances across windows/screens) — `R-D2-1..4`

**Owner objective:** D2 (roadmap S1 — the headline differentiator). **Tier:** T2 (the cluster's
biggest design gap; no existing per-feature plan/register). The substrate is single-instance today.
**Reconcile (do not duplicate):** DCAD §2 (the one-present-authority invariant, here generalized to
per-panel) and `DEVICE_LOST_RECOVERY_RISK_REGISTER` (device-loss must now collapse ONE instance, not
the process).

> **First-hand FLAG carried from the D-cluster:** the DComp multi-surface independent-flip behavior
> (whether N `ExcludeFromCapture` composition-target HWNDs flip without cross-contention) is NOT
> verifiable from `main.cpp` alone — it lives in `PresentSurface.cpp` (pillar). R-D2-2 assumes
> per-instance surfaces are independent; **this MUST be verified first-hand in `PresentSurface.cpp`
> before the D2 build.**

#### R-D2-1 — concurrency: cross-instance shared-state corruption
- **Failure mode + site.** Today's process-global handles (`g_quit` / `g_quit_threads` e.g.
  `main.cpp:6914`, the bridge ring, device/queue, the gme host buffers) are unguarded singletons;
  N instances racing them = data race / crash.
- **Mitigation AS CODE (designed).** Every mutable singleton becomes `FgInstance`-owned (no shared
  mutable state across instances); the only shared object is the read-only characterized catalog +
  the DComp device (if shared, under a documented contract). Per-instance quit replaces `g_quit`.
- **First-hand verification plan.** ThreadSanitizer / validation-layer clean across a 2-instance
  60 s run; assert (grep + audit) no `g_*` global is written by >1 instance.
- **Status:** `designed` / `open`.

#### R-D2-2 — present-authority violation (SLI runt-frames reintroduced)
- **Failure mode + site.** If two instances ever share a present thread/surface, the DCAD §2
  one-present-authority invariant breaks → micro-stutter. The current surface is one
  `PresentSurface` (`main.cpp:6901`) with `monitor_index=cfg.pres_mon` (`main.cpp:6906`); the
  pillar's same-thread create()/submit() contract is at `main.cpp:6897`.
- **Mitigation AS CODE (designed).** The pillar's same-thread create()/submit() contract makes a
  shared present thread structurally impossible per instance; assert ONE `PresentSurface` per
  `FgInstance`, ONE present thread per instance (the invariant held per panel).
- **First-hand verification plan.** Count present threads == instance count at runtime; per-panel
  `MsBetweenDisplayChange` jitter of A unchanged with B running.
- **Status:** `designed` / `open`. **Gated on the DComp independent-flip FLAG above.**

#### R-D2-3 — device-loss now partial
- **Failure mode + site.** A TDR on GPU-2 must collapse only instance B to passthrough, leaving
  instance A running — today's `VK_ERROR_DEVICE_LOST` path (`main.cpp:2895`) sets the **global** quit.
- **Mitigation AS CODE (designed).** Route `vk_live()` device-loss to the owning `FgInstance`'s
  quit, not `g_quit`; the surviving instance keeps running. **Generalizes the discharged
  `DEVICE_LOST_RECOVERY` mechanism to per-instance scope** (does not redefine it).
- **First-hand verification plan.** Force a device-loss on one instance's device → that instance
  exits clean, the other continues ≥60 s.
- **Status:** `designed` / `open`.

#### R-D2-4 — resource contention silently degrades both instances (no net benefit)
- **Failure mode + site.** Two FG slices on a shared PCIe/DDR controller (iGPU convert + dual
  host-bounce) can each push the other below the present deadline.
- **Mitigation AS CODE (designed).** The per-instance device/CPU partition (`select_participants`
  from P5 + the topology pillar `phyriad::hw::pin_current_thread`, consumed at `f711102`) + a
  per-instance measured slice gate (reuse C2/D1 telemetry); if instance B cannot clear its deadline
  on its partition it DEGRADEs (DCAD §2.4), it does not steal from A. `--csv` is currently a single
  global path (`main.cpp:1092`) → must become per-instance for the evidence.
- **First-hand verification plan.** The D2.f closeable test IS this risk's discharge: two real games
  on two monitors ≥60 s + two per-instance `--csv` outputs showing slice/latency of A unchanged with
  B running vs A alone (no cross-interference).
- **Status:** `designed` / `open`.

---

### §2.4 — F3b-L3 (mobile-companion pairing) — `CP-CR1..CP6`

**Owner objective:** F3b-L3 (the control-plane mobile-pairing increment). **Tier:** T2 — **RCE-class**
(a control channel with mutating commands is an RCE primitive, dossier S3) **+ concurrency** (a
network listener thread feeding the lock-free command ring). It MUST NOT be committed while any risk
is `open`. **Reconcile (do not merge):** this does NOT duplicate `DEVICE_LOST_RECOVERY_RISK_REGISTER`
(Vulkan TDR is an orthogonal crash class) — cross-link only. These rows MAY later move into a dedicated
`CONTROLPLANE_RISK_REGISTER.md` when F3b-L3 is actually built (it un-defers `CONTROLPLANE_MASTER_PLAN §6`).
**As-built grounding (corrects the stale dossier "no code"):** the control-plane pillar is built
through CP0/CP1/CP3 (`b3253a7`); the `CommandKind` enum is already a closed Start/Stop set
(`ControlCommand.hpp:36-40`); the `Ingress` stub has no transport/dispatch/auth (`Ingress.hpp:6-8`),
so F3b is satisfied **vacuously** today.

| ID | Failure mode (+ site) | Mitigation AS CODE (designed) | First-hand verification plan | Status |
|----|----|----|----|----|
| **CP-CR1** | **Unauthenticated control ingress** — a raw socket skipping the handshake mutates FG state (the OBS→RCE class, dossier S3). | Per-message auth: every mutating `ControlCommand` carries a token verified before dispatch; `Ingress::poll` consumers MUST reject any command whose auth state is unverified. Type-level: a `ControlCommand` cannot reach dispatch without passing through an `Authenticated<ControlCommand>` wrapper constructible only by the verified-handshake path. | Audit part (B): a harness sends each `CommandKind` over a raw socket pre-handshake; assert FG state byte-identical (`--csv` fields unchanged) before/after. | `designed` / `open` |
| **CP-CR2** | **Cross-Site WebSocket Hijacking** — the user's own browser opens `ws://localhost:<port>` from a malicious page (SOP does not cover loopback, dossier S4). | Server-side `Origin`-allowlist checked on **every** handshake before any upgrade; reject on mismatch/absent. Origin is browser-set and JS-unforgeable. | Audit part (B): a cross-origin handshake with a forged JS `Origin` header → assert rejected, zero state change. | `designed` / `open` |
| **CP-CR3** | **A network-reachable transport with auth disabled is constructible** (the "auth one-click-off" footgun, dossier S3 / F3b-f-2). | Build/type invariant: the `network-bind` transport type and the `auth-disabled` flag are **mutually exclusive at construction** — no config object pairs them. Auth-off permitted only for the loopback+Origin-locked local case. | Audit part (C): a `static_assert`/compile-fail test that the forbidden pairing does not compile. | `designed` / `open` |
| **CP-CR4** | **Hand-rolled pairing crypto leaks the PIN** (Moonlight CVE-2020-11024, dossier S7 — concatenating PIN to salt too early). | Do **NOT** hand-roll PIN+salt. Use a vetted handshake (Afterburner-PSK floor S8; Moonlight PIN-pairing + cert-pinning target S6) over WSS/TLS (S5a). No bespoke salt+PIN concatenation. | Code review against the S7 CVE mechanism; confirm no bespoke salt/PIN concat exists; the handshake is a named vetted primitive. | `designed` / `open` |
| **CP-CR5** | **Dangerous command vocabulary** — a file-write/path/process-spawn verb over the wire is an RCE primitive (S3's chain hinged on `SaveSourceScreenshot`'s arbitrary write). | Command vocabulary = a **closed enum of FG-tuning verbs only** (the as-built `CommandKind` Start/Stop, `ControlCommand.hpp:36-40`); never a path/write/spawn primitive. No payload field is interpreted as a filesystem path or command line. | Code review of the `CommandKind` enum + the dispatch switch; assert no path/spawn handler exists. | `designed` / `open` |
| **CP-CR6** | **Listener-thread ↔ command-ring data race** — the network thread feeds `Ingress` concurrently with the FG's `poll`. | Reuse the existing lock-free `transport::Ring` (the same SWMR ring `Egress` uses; `Ingress.hpp:10-11`); single subscribed reader (the FG), single producer (the transport). No new primitive. | TSan-clean test on the producer↔poll ring (extend `controlplane_concurrency_test.cpp`); `lint_hal` clean (default `seq_cst`, no raw `memory_order_*`). | `designed` / `open` |

**Reconcile note.** Audit part (C) and CP-CR3 are the SAME invariant viewed two ways (CI runtime
proof vs compile-time proof); the F3 fixed-point requires **both** green. Cross-link this set to
`FG_CONTROLPLANE_SECURITY_PRIOR_ART.md` (the S-numbered prior art each mitigation cites) and to
`CONTROLPLANE_MASTER_PLAN.md §6` (which *defers* exactly this surface — these rows are what un-defer it).

---

### §2.5 — G1 (CP2 telemetry egress on the FG present path) — `R-G1-1`

**Owner objective:** G1 (the two-click launcher + CP2 wiring). **Tier:** T1 (the launcher is a
separate process; CP2 is the one piece that touches the FG process — it stays T1 because the egress
is default-off + the pillar's lock-free contract is already built/tested, but it carries this one
concurrency line item). **Reconcile, do not duplicate:** `CONTROLPLANE_MASTER_PLAN.md §7` already
owns "no runtime behaviour measured / lock-free is a design commitment proven by CP0's acceptance
test"; this row's *new* surface is only the FG-process integration point (the present-thread push).

#### R-G1-1 — concurrency: CP2 egress push from the FG present path
- **Failure mode + site.** Pushing a `TelemetryFrame` into the `Egress` ring from the present thread
  must be lock-free + zero-alloc to not perturb pacing. The integration point is adjacent to the
  existing `telemetry_csv.hpp` emission (`--csv` at `main.cpp:1092`); `TelemetryFrame` IS the
  `CsvRow` repacked as a wire-stable POD.
- **Mitigation AS CODE (designed).** Use the pillar's existing lock-free `Egress` (one slot-claim +
  POD copy), cold drain off-thread (`Ndjson.hpp`); gate behind a default-off `--cp-egress` flag so
  the default path is untouched (byte-identical-off).
- **First-hand verification plan.** The pillar already ships `controlplane_zeroalloc_test.cpp` +
  `controlplane_concurrency_test.cpp` (TSan). For CP2 specifically: a byte-identical-off pixel diff
  (egress on vs off, FG-output pixel diff = 0) + a present-tick timing-delta measurement (`--csv`
  iter-ms with/without egress).
- **Status:** `designed` / `open`.

---

### §2.6 — E2-PRESENT displayable-DComp-surface probe (register home decided here)

**Owner objective:** E2-PRESENT (the library-breadth present-mode increment). **Tier:** T2 (one probe).
Per audit fix #3 + synth §6 #7, the E-cluster deferred this probe's register to a "DCAD P5
RISK_REGISTER" **which does not exist** → its home is decided HERE, in the triad register, rather
than left orphaned. (Note: not in `FG_SATURATION_STABILITY` / `REAL_FAST_PATH`.)

#### R-E2P-1 — a displayable DComp surface may break the premultiplied-alpha overlay or VRR posture
- **Failure mode + site.** Probing a displayable/MPO present mode (to keep VRR alive, an E2/F2
  go-beyond) against the DComp overlay risks the same `DXGI_ALPHA_MODE_PREMULTIPLIED` exclusion class
  as R-A3-2 (`PresentSurface.cpp:222`) and may regress the present path.
- **Mitigation AS CODE (designed).** Scope it as a probe with an **honest null-allowed outcome**
  (the dossier predicts null on NVIDIA); gate behind a default-off flag; assert the premultiplied-
  alpha overlay invariant is preserved (reuse R-A3-2's static guard). Never hook the game swapchain.
- **First-hand verification plan.** Construct the displayable-surface config under `vk_live()`; if it
  cannot preserve the overlay invariant or the present regresses, the probe records a null result and
  the lever stays OFF — no commit on an `open` regression.
- **Status:** `designed` / `open`.

---

## §3 — Discharge ledger (to be filled as risks close — none discharged this pass)

| Risk ID | Date | Discharge evidence (first-hand) | New status |
|---|---|---|---|
| _(none — nothing in this register is built; every row is `designed`/`open`)_ | — | — | — |

> **Reminder (the Tier-2 hard gate).** The owning objective MUST NOT commit while any of its rows
> above is `open`. To close a row: realize the mitigation AS CODE, run its verification plan
> first-hand, record the evidence in this ledger, and flip the status to `mitigated` (or `accepted`
> with the operator's recorded residual).
