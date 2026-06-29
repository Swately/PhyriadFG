# FG_PARALLEL_INSTANCES_MASTER_PLAN — D2: parallel FG instances, focus-independent

Status: `0.1.0-experimental` · Type: **Planning / design (Explanation)** · Tier: **2 (risk-bearing)**
Objective family: **D2** — parallel FG across windows/screens, focus-independent
([`PHYRIADFG_OBJECTIVE_VISTA.md`](PHYRIADFG_OBJECTIVE_VISTA.md) §1 · D2).

> **Normativity (BCP 14).** The key words MUST / MUST NOT / SHOULD / SHOULD NOT / MAY are used per
> RFC 2119/8174 only where a real requirement binds (a dogma, a plan-tier obligation, a safety gate).
>
> **Honesty buckets ([FDP](../canon/FORMAL_DOCUMENT_PROTOCOL.md) §4.2).** Nothing in D2 is built; every
> capability claim here is `designed`. The *standing* facts about today's code are `shipping`/`measured`
> and cited at `file:line`, verified first-hand 2026-06-23 (the verification anchor for the whole triad —
> §6; per [FDP §4.8](../canon/FORMAL_DOCUMENT_PROTOCOL.md) the attestation is stated once, here).
>
> **Triad.** This is the Tier-1 master-plan leg. Its siblings are
> [`FG_PARALLEL_INSTANCES_IMPLEMENTATION_STRATEGIES.md`](FG_PARALLEL_INSTANCES_IMPLEMENTATION_STRATEGIES.md)
> (the per-strategy edit sites) and the **mandatory Tier-2**
> [`FG_PARALLEL_INSTANCES_RISK_REGISTER.md`](FG_PARALLEL_INSTANCES_RISK_REGISTER.md). The triad MUST NOT
> be committed while any register risk is `open` ([PLAN_TIER_PROTOCOL](../canon/PLAN_TIER_PROTOCOL.md) §3).
>
> **Relationship to the PHYRIADFG cluster triad.** D2 already has a *summary* strategy block in
> [`PHYRIADFG_IMPLEMENTATION_STRATEGIES.md`](PHYRIADFG_IMPLEMENTATION_STRATEGIES.md) (§D2) and rows
> R-D2-1..4 in [`PHYRIADFG_RISK_REGISTER.md`](PHYRIADFG_RISK_REGISTER.md). This triad is the **dedicated,
> deeper** D2 plan the vista calls for (D2.c: "needs its own Tier-2 design plan that doesn't exist yet").
> It does NOT duplicate the cluster's shared content (single source of truth, [FDP §4.6](../canon/FORMAL_DOCUMENT_PROTOCOL.md));
> it **deepens** the four-pillar strategy, adds first-hand findings the cluster's summary did not carry
> (the window-class collision, §3 R-D2-5; the `g_quit_threads`-is-already-local correction, §6), and
> raises the risk treatment to a full standalone register.

---

## §1 — The objective (what D2 IS, and the floor it does NOT cross)

**Target (vista D2.a).** FG on game B / monitor 2 (out of focus, second screen) while game A runs on
monitor 1 — each correct, no interference. This is the operator's #4 objective and the **headline
differentiator**: LSFG's single-target architecture (multi-frame-gen = stacked passes on ONE target)
**structurally cannot follow** (vista D2.a/D2.d). D2 is a `FIXED-POINT` — it crosses once and stays crossed.

**The closeable criterion (vista D2.f — the binary "done").** Two games on two monitors, each with its
own FG instance, run **≥60 s** with correct output and **no cross-interference**, evidenced by **two
per-instance telemetry CSVs**. Binary. That is the target this plan builds toward.

**The honest floor (vista D2.b + §5).** D2 has **no NEW structural floor of its own** — that is precisely
why it is the headline differentiator. But D2 does **not escape the FG-class floors** named in
[`PHYRIADFG_OBJECTIVE_VISTA.md`](PHYRIADFG_OBJECTIVE_VISTA.md) §5, and this plan MUST NOT re-propose any of
them as a D2 win:

- **The interpolation HOLD** and **no-Reflex-hook latency floor** (§5) apply per instance, unchanged.
  N instances do not lower any one instance's latency.
- **The 2nd cross-device hop (~3–8 ms)** is the D1 multi-GPU tax (vista D1.b); when D2 partitions devices
  across instances it inherits that tax per instance — D2 does not remove it.
- **Consumer-GPU queue non-preemption** (§5) and **the resource-contention ceiling** (two FG slices on a
  shared PCIe/DDR controller) are real: when both instances saturate the same physical bottleneck, the
  honest D2 outcome is **graceful per-instance degrade**, NOT "both run at full slice for free". D2's win
  is *independent correctness*, not *free throughput*. Re-framing the contention ceiling as a beatable
  win is the §5 failure mode and is out of scope.

**What D2 explicitly does NOT claim.** It does not claim lower latency, does not claim N instances scale
throughput linearly, and does not claim the contention ceiling is removable. It claims **independence**:
A's correctness and felt cost are unchanged whether B runs or not, on its own panel/device partition.

---

## §2 — Standing (first-hand, 2026-06-23) — why D2 is not free today

The whole app is **ONE instance**. Verified first-hand:

- **One present authority, process-global.** A single `PresentSurface::create(psd)` at
  [`main.cpp:6987`](../../apps/render_assistant/src/main.cpp) with `psd.monitor_index=cfg.pres_mon`
  (`main.cpp:6977`). The pillar is move-only and owns its own HWND + D3D11 device + composition swapchain
  + DComp target/visual + WDA per instance ([`PresentSurface.hpp:124-176`](../../framework/render/present/include/phyriad/render/present/PresentSurface.hpp)) —
  a **good** foundation: the per-panel ownership D2 needs is already the pillar's shape.
- **Control flow is process-global.** `g_quit` is a `static volatile bool` at `main.cpp:2922`; the
  device-loss detector `vk_live()` sets that **global** `g_quit` on `VK_ERROR_DEVICE_LOST`
  (`main.cpp:2940-2943`). A loss on ANY device today quits the WHOLE process.
- **`g_quit_threads` is already function-local**, not a true global: `std::atomic<bool> g_quit_threads{false}`
  at `main.cpp:5066` (correction to the cluster summary, which implied it was a global like `g_quit`). It
  is still **one** flag shared by all threads of the single instance, so it still needs per-instance scope.
- **Device selection is identity-branched, not capability-routed.** `pG` (iGPU) / `pB` (discrete assist)
  are chosen by `deviceType` + a name-substring `afrag` match (`main.cpp:3212-3213`); the hard-exit "need
  primary + assist" is at `main.cpp:3235`; `--force-single-gpu` (parsed `main.cpp:1146`) drives the
  degenerate path. The capability router (`break_even_decide` / `select_participants` / `run_routed`) is
  **built and Mock-tested in the pillars** but has **zero in-product callers** (the only `main.cpp`
  occurrence is a comment at `main.cpp:50`; verified first-hand — `select_participants`/`run_routed` have
  0 callers under `apps/render_assistant`). This is **P5** (the dead-router wiring) — D2's hard dependency.
- **`--csv` is a single global path.** `cfg.csv_path` (`main.cpp:681`, parsed `:1117`) feeds one
  `TelemetryCsv tcsv` (`main.cpp:6956`). The D2.f evidence needs **two** per-instance CSVs → the path
  must become per-instance/templated.
- **The overlay window class is a fixed process-global string.** `kOverlayClass = "phyriad_present_overlay"`
  ([`PresentSurface.cpp:91`](../../framework/render/present/src/PresentSurface.cpp)); `create()` calls
  `RegisterClassExW` **unconditionally** (`PresentSurface.cpp:280`) and `bail(SystemError)` if it fails
  (`:281`). **First-hand finding (new to this triad):** a second concurrent `PresentSurface::create()`
  re-registers the SAME class name → `RegisterClassExW` returns 0 with `ERROR_CLASS_ALREADY_EXISTS` →
  the second instance **cannot be created today**. This is a concrete, code-level D2 blocker the cluster
  summary did not name → registered as **R-D2-5** (RISK_REGISTER §R-D2-5).

**Net:** D2 = encapsulate the single-instance pipeline into an `FgInstance` and instantiate it N times,
with a per-instance present authority, a per-instance device/CPU partition, and per-instance lifetime.
It is **not new FG math** — the single-instance body IS the per-instance body.

---

## §3 — Why Tier-2 (the qualifying risks)

Per [PLAN_TIER_PROTOCOL §1.1](../canon/PLAN_TIER_PROTOCOL.md), D2 is Tier-2 on multiple triggers:

1. **Concurrency hazard (§1.1-2).** N instances racing today's process-global singletons (`g_quit`, the
   bridge ring, device/queue handles, gme host buffers) is a data race. Phyriad's lock-free mandate makes
   any new cross-thread sharing presumptively Tier-2 — D2 introduces N-fold sharing.
2. **Crash / device-loss / hang (§1.1-1).** Device-loss is currently process-global (`main.cpp:2940-2943`);
   under D2 a TDR on instance B's device MUST collapse only B, not A. Getting that partial-recovery wrong
   is a crash class. The window-class collision (R-D2-5) is itself a create-failure class.
3. **A documented dogma at stake (§1.1-5).** **Byte-identical-off**: the `FgInstance` refactor MUST reduce
   to today's exact single-instance flow at N=1 (DOGMA byte-identical). The **no-game-cap** invariant
   (operator FG-arc provenance, cited in [`PHYRIADFG_RISK_REGISTER.md`](PHYRIADFG_RISK_REGISTER.md)) MUST
   hold per instance — D2 never caps a base game to make room.

The register ([`FG_PARALLEL_INSTANCES_RISK_REGISTER.md`](FG_PARALLEL_INSTANCES_RISK_REGISTER.md)) details
every failure mode with its mitigation AS CODE + a first-hand verification plan; no `open` risk may ship.

---

## §4 — The architecture (four pillars, each grounded in an existing substrate hook)

This is the deepened form of the cluster summary's four-pillar strategy. Each pillar names its existing
hook; the concrete edit sites are in the strategies sibling.

1. **N independent role graphs (`FgInstance`).** Refactor the single-instance process-globals into an
   `FgInstance` struct owning its own lifetime, capture target (HWND/monitor), bridge ring, command
   pool/queue, and **per-instance quit flag** (replacing both `g_quit` and the shared `g_quit_threads`).
   The monolith becomes `for each target: spawn FgInstance`. **N=1 MUST be byte-identical to today.**

2. **Independent present authority PER PANEL.** Each `FgInstance` creates its OWN `PresentSurface` with
   its own `monitor_index` — the pillar already owns HWND+device+swapchain+DComp+WDA per instance
   (`PresentSurface.hpp:124-176`), and its **same-thread create()/submit() threading contract**
   (`PresentSurface.hpp:144-147`) makes a shared present thread structurally impossible: **each instance
   owns its present thread → one present authority per panel.** This preserves the
   [`FG_ARCHITECTURE_DCAD_MASTER_PLAN.md`](FG_ARCHITECTURE_DCAD_MASTER_PLAN.md) §2 one-present-authority
   invariant **per panel**, so N panels do NOT reintroduce SLI/CrossFire runt-frames (each panel has
   exactly one presenter; the runt-frame pathology is *two presenters racing one panel*, which never
   occurs here). **Prerequisite:** the window-class collision (R-D2-5) MUST be fixed first, or the second
   `create()` fails.

3. **Multi-surface DComp.** N DComp visuals, one per panel, each WDA-excluded + click-through (today's
   defaults). **First-hand resolution of the cluster's open FLAG:** independence is structurally present
   — each `create()` allocates its own `Impl` (`PresentSurface.cpp:264`), its own HWND (`:304`), its own
   D3D11 device (`:315`), and applies `SetWindowDisplayAffinity` per-HWND (`:401`). The ONLY shared
   process-global is the window-class name (R-D2-5); once that is per-instance, N surfaces are independent.
   DComp independent-flip across distinct HWNDs on distinct monitors is the normal Windows compositor case.

4. **Scheduler / device partition.** Use the wired router (`select_participants`, P5) to partition the
   **measured** device set across instances, and pin each instance's CPU threads to a disjoint CCX/core
   set via `phyriad::hw::pin_current_thread` (the topology pillar, consumed for `--pin` at commit
   `f711102`) so A's F/P threads do not starve B's. When the partition cannot give both instances enough,
   the honest outcome is **per-instance degrade** (§1 floor), not theft from A.

---

## §5 — Phases and gates

D2 depends HARD on **P5** (the wired router) and on the `FgInstance` encapsulation. Suggested ordering:

| Phase | What | Gate (first-hand) |
|---|---|---|
| **D2-0** | Fix R-D2-5 (per-instance window class) | two `PresentSurface::create()` on two monitors both succeed; N=1 byte-identical |
| **D2-1** | `FgInstance` encapsulation (pillar 1) | N=1 reduces to today's exact flow — byte-identical-off harness diff = 0 |
| **D2-2** | Per-instance present authority + multi-surface DComp (pillars 2–3) | present-thread count == instance count; A's `MsBetweenDisplayChange` jitter unchanged with B running |
| **D2-3** | Per-instance device-loss scoping (R-D2-3) | force a loss on one instance's device → that instance exits clean, the other continues ≥60 s |
| **D2-4** | Device/CPU partition via P5 router (pillar 4) | A's slice/latency unchanged with B running vs A alone (the contention test) |
| **D2-5 ★** | The closeable test (vista D2.f) | two real games, two monitors, ≥60 s, **two per-instance CSVs**, no cross-interference |

**Commit gate (Tier-2).** D2 MUST NOT be committed while any
[`FG_PARALLEL_INSTANCES_RISK_REGISTER.md`](FG_PARALLEL_INSTANCES_RISK_REGISTER.md) risk is `open`. P5
(the router wiring) is a prerequisite, not part of this change; D2-4 assumes it is wired (its own plan).

---

## §6 — Verification anchor + what could NOT be verified

**Verified first-hand 2026-06-23** (the single attestation anchor for the triad — [FDP §4.8](../canon/FORMAL_DOCUMENT_PROTOCOL.md)):
every `file:line` in §2/§4 was opened and confirmed to say what is claimed —
`PresentSurface.{hpp,cpp}` (the per-instance ownership, the threading contract, the window-class
registration + bail), `main.cpp` (the single `create()`, `g_quit`/`g_quit_threads`, the device-loss
path, device selection, `--csv`), and the router primitives' 0-caller status under `apps/render_assistant`.

**Could NOT be verified first-hand (read before building):**
- **Whether N DComp independent flips on distinct monitors actually stay phase-independent at runtime** —
  the *structure* is independent (§4 pillar 3), but the runtime flip behavior under two saturating
  instances is a RUNTIME observation, gated on a real 2-monitor run (R-D2-2's verification plan). Designed,
  not measured.
- **The exact contention profile** of two FG slices on this rig's shared PCIe/DDR controller — the §1
  ceiling is real but its *magnitude* is unmeasured until D2-4 (R-D2-4).
- **P5's wired-router behavior** — D2 assumes P5 partitions the device set correctly; that is P5's own
  plan/verification, inherited here as a dependency, not re-verified.
