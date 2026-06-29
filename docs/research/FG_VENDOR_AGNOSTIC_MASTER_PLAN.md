<!-- FG_VENDOR_AGNOSTIC_MASTER_PLAN — drive PhyriadFG's hardcoded-vendor assumptions to zero so
     the default (flag-free) path is correct on a single arbitrary-vendor GPU (objective E1). -->

# FG_VENDOR_AGNOSTIC_MASTER_PLAN — the E1 implementation (drive hardcoded-vendor assumptions to ZERO)

> **Diátaxis type:** how-to / implementation plan (a build orchestration doc, not analysis — the
> analysis is the dossier [`FG_VENDOR_LANDSCAPE_PRIOR_ART.md`](FG_VENDOR_LANDSCAPE_PRIOR_ART.md) and the
> objective cell [`PHYRIADFG_OBJECTIVE_VISTA.md`](PHYRIADFG_OBJECTIVE_VISTA.md) §1-E1).
>
> **Status:** `designed`. Nothing in this plan is built yet; every claim about the *current* code below is
> `[VS]` (first-hand-verified this session, file:line given). Every claim about the *target* code is
> `designed`.
>
> **Plan tier:** **Tier 2 (risk-bearing).** Trigger §1.1(4) data-format/irreversibility + §1.1(5)
> dogma-at-stake: the central lever changes `GpuDescriptor` — a **cross-process-published POD** whose
> layout is frozen by `static_assert(sizeof(GpuDescriptor) == 128u, "…published cross-process; keep it
> stable")` (`framework/gpu/include/phyriad/gpu/GpuDescriptor.hpp:61` `[VS]`). Any field change is a
> schema-format change that a stale reader could mis-map. Therefore this plan ships with a
> **[`FG_VENDOR_AGNOSTIC_RISK_REGISTER.md`](FG_VENDOR_AGNOSTIC_RISK_REGISTER.md)** and MUST NOT be
> committed while any risk there is `open` (PLAN_TIER_PROTOCOL §3).
>
> **Siblings (one set, mutually linked):** this master plan ·
> [`FG_VENDOR_AGNOSTIC_IMPLEMENTATION_STRATEGIES.md`](FG_VENDOR_AGNOSTIC_IMPLEMENTATION_STRATEGIES.md)
> (the per-edit sites + byte-identical discipline) ·
> [`FG_VENDOR_AGNOSTIC_RISK_REGISTER.md`](FG_VENDOR_AGNOSTIC_RISK_REGISTER.md) (every failure mode + its
> mitigation-as-code). The supervisor registers all three in `FORMAL_DOCUMENTS_REGISTER.md` and threads
> the ACTION_PLAN node — this plan does NOT touch those shared files.
>
> **Normativity (BCP 14):** MUST / MUST NOT / SHOULD used only where a dogma, a plan-tier obligation, or
> the E1 closeable criterion binds.

---

## §0 — Why this exists, and the one inversion that governs it

The operator's stated fear, named exactly in the vista cell: *"great on the author's NVIDIA pair, untested
on a stranger's single Arc/AMD."* E1 is `MISSING`/unverified — **true by omission**, not by a known bug.

The blocker is **hardware access**, not code: the author has only NVIDIA (4090 + 1080 Ti, both `0x10DE`).
A real cross-vendor RUN cannot happen here. **So this plan does NOT attempt the run.** Per the vista's
honest caveat (`PHYRIADFG_OBJECTIVE_VISTA.md:243` `[VS]`) the measurable progress metric until hardware is
available is **the count of hardcoded-vendor assumptions driven to zero**. This plan's deliverable is
exactly that count reaching 0 — i.e. *E1-ready code*, with only the run left.

**The governing inversion (dossier §1 "Key E1 inversion", `[V2]`):** AMD/Intel GPUs *out-perform* consumer
NVIDIA at the FG pass because of superior fp16 throughput. PhyriadFG's instinct to lean on **NVOFA** (an
NVIDIA-only HW block) is therefore **backwards for E1**. The portable winner is **fp16 packed math**, with
**DP4a** (cross-vendor int8: NVIDIA Pascal+, AMD, Intel) as the gated accelerator. NVOFA is a 4090-specific
*bonus*, never the default path.

---

## §1 — The current state, verified first-hand (the assumptions to drive to zero)

Each row is `[VS]` — grepped/read this session at the cited site. This is the **baseline count**: the plan
closes it to 0.

| # | Hardcoded-vendor assumption | Site (`[VS]`) | What is wrong for E1 |
|---|---|---|---|
| **VA-1** | `GpuDescriptor` cannot REPRESENT a non-NVIDIA capability profile — its only measured-throughput field is `compute_gflops`; there is **no fp16 field, no DP4a/int8-dot field**. | `GpuDescriptor.hpp:49` (the MEASURED block) `[VS]` | The capability table literally cannot record "this GPU is fp16-fast / has DP4a" — the very facts E1 routes on. The table is NVIDIA-FLOPS-shaped. |
| **VA-2** | The offload decision is a **hardcoded constant**, not a runtime call. `break_even_decide()` has **0 call sites in `main.cpp`** — only a comment: *"at FG's AI≈2.5 FLOP/byte (vs the ~596 crossover) it returns offload=false ALWAYS, a constant. Inlined as such."* The `~596` crossover is the author's **4090+1080Ti-measured** value. | `main.cpp:50-52` (comment, 0 actual calls) `[VS]`; the constant origin `GpuDescriptor.hpp:79` `[VS]` | The "router is effectively dead" the dossier names. A constant tuned to one NVIDIA pair is not a per-rig measured gate. (Note: the dossier said "1 caller"; first-hand it is **0** calls — inlined to a constant. Stronger than the dossier stated.) |
| **VA-3** | Device-role selection branches on `deviceType` + LUID with no capability fallback: primary = LUID-match; `INTEGRATED_GPU` → iGPU role; `DISCRETE_GPU` → assist role. | `main.cpp:3210-3213` `[VS]` | Correct on a 4090+1080Ti+iGPU rig. On a *single* arbitrary-vendor GPU it must collapse to the single-GPU path — which it DOES via `single_gpu` (`main.cpp:3233` `[VS]`), but the branch is `deviceType`-shaped, not capability-shaped, and has never run on a non-NVIDIA single GPU. |
| **VA-4** | No fp16 capability QUERY at device creation; fp16 is not asserted-default, not the declared default primitive. The flow path's storage format is not gated on `VK_KHR_shader_float16_int8`. | `vdev_create` (`main.cpp:1930`) queries OFA (`main.cpp:1942`) but not `shaderFloat16`/`shaderInt8` `[VS]` (grep = 0 for `shaderFloat16` in `main.cpp`) | E1 wants fp16 as THE default primitive (dossier §2 Tier-0). Today it is neither queried nor declared; "is fp16 the default" is unanswerable from the code. |
| **VA-5** | DP4a (cross-vendor int8 dot, the Tier-1 accelerator) is **absent** — no `VK_KHR_shader_integer_dot_product` query, no int8 flow correlation path. | grep `integer_dot`/`shaderInt8`/`dp4a` in `main.cpp` = 0 `[VS]` | The one tier that would match XeSS-FG's portable acceleration on AMD/Intel does not exist. (This is an *additive* gap, not a hardcode to remove — tracked here because the dossier §2 lists it as the missing Tier-1; see §3 on scope.) |

**Not a defect (verified, do NOT "fix"):**
- **NVOFA is already correct for E1.** It is fully `--nvofa`-gated, default-off, auto-disables when the
  device lacks `VK_NV_optical_flow`, and the default path is byte-identical (`main.cpp:882-889`,
  `main.cpp:1992`, `main.cpp:2071` `[VS]`). It is the 4090-bonus the dossier sanctions, not a default
  dependency. **MUST NOT** be made a default.
- **The F2/VRR vendor-string branch** (`is_nv`/`is_amd` from the device name, `main.cpp:3226-3231` `[VS]`)
  is **honest startup MESSAGING**, not a decision path — it changes no FG output, only what is printed.
  It is *correct* vendor-conditional honesty (the G-Sync floor is vendor-asymmetric, vista F2). It is
  **out of E1 scope** and MUST NOT be counted as a hardcode-to-remove; removing it would *lose* an honest
  statement. Named here so a later pass does not mistake it for a violation.

**The baseline count of E1-blocking hardcoded-vendor assumptions = 4** (VA-1..VA-4). VA-5 is an additive
capability gap, tracked separately (§3). The plan's success metric is VA-1..VA-4 → 0.

---

## §2 — The architecture (what "E1-ready" looks like)

The dossier's measured 3-tier accelerator ladder (§2 sub-objective) is the target shape:

- **Tier 0 — the portable default (the E1 path = the LSFG-parity path).** Vulkan compute + fp16 +
  the existing 8×8-SAD pyramid flow. Runs on every SM6.0+ GPU of any vendor. **This is the default; flags
  off ⇒ this path, byte-identical to today on the author's rig.**
- **Tier 1 — DP4a (cross-vendor), opt-in + measured-gated.** When the runtime reports
  `VK_KHR_shader_integer_dot_product` AND a per-rig measured net-positive, use int8 flow correlation.
- **Tier 2 — NVOFA (NVIDIA-only bonus), opt-in + measured-gated.** Already shipped as `--nvofa`,
  default-off. Unchanged by this plan.

The capability that makes the ladder *decidable per rig* is the populated `GpuDescriptor` (VA-1) read by a
real `break_even_decide` call / characterize step (VA-2). **No vendor name appears in any decision path**
(the E1 grep-checkable criterion): decisions key on *measured capability fields*, not `vendor_id` strings.

`vendor_id` (`GpuDescriptor.hpp:30`) MAY remain as **identity/telemetry** (it is honest provenance), but
MUST NOT gate behavior. The E1 grep is over the *enable/route logic*, not over the struct.

---

## §3 — Scope decision (the floors named, and what this plan does and does NOT do)

**Per PLAN_TIER_PROTOCOL anti-over-process + the FDP honesty mandate, this plan is scoped to the
HARDCODE-REMOVAL, not the new-accelerator build.**

- **IN scope (the closeable progress metric):** drive VA-1..VA-4 to 0 — i.e. make the code *representable*
  and *capability-driven* for any vendor, with fp16 as the declared+queried default primitive and the
  offload decision a real per-rig gate (or an honestly-named characterized constant), no vendor string in
  any decision path.
- **OUT of scope, by floor or by dependency:**
  - **The actual cross-vendor RUN (E1-NV / E1-AMD / E1-INT).** Blocked on **hardware access** — a
    NAMED FLOOR (vista f-caveat, dossier §2(f), `[VS]`). This plan **does not plan to beat it**; it
    produces E1-ready code so the run is the only remaining step. *Re-proposing the run as closeable-now
    would be the failure mode; it is not.*
  - **DP4a int8 flow correlation (VA-5 / dossier Tier-1).** An *additive feature*, Tier-1 in its own
    right, with its own measured-net-positive gate. It is **deferred to a follow-up** (it is net-new
    shader work, not a de-vendoring edit) and tracked as the residual count item. The hardcode metric
    reaches 0 without it; E1-readiness for the *portable default* (Tier 0) does not require Tier 1.
  - **Low-power-GPU default profile** (LSFG's "separate model" analogue, dossier lever (e)(3), T1) —
    a follow-up, not a de-vendoring edit.

**Floors NAMED (never planned to beat) — from vista §5 + the E1 cell:**
1. **No HW-OFA on AMD/Intel** `[V1]` → OFA acceleration is intrinsically single-vendor. NVOFA stays a
   bonus; we do NOT try to make optical-flow-acceleration portable.
2. **Hardware access** is the E1-run blocker, not code — the run is not closeable on the author's rig.
3. **No Reflex/Anti-Lag hook on any vendor** (shared with all external tools) — irrelevant to the de-
   vendoring but named so it is not re-litigated under E1.
4. **AMD-render + NVIDIA-secondary launch failure** — a driver-class floor LSFG hits too; we do not
   attempt to cure it.

---

## §4 — Phases & gates

The work is small in line-count and large in blast-radius (the POD change). Phases match the risk
register's mitigation order.

| Phase | Deliverable | Gate (verify-before-claim) | Risk IDs |
|---|---|---|---|
| **VP-0 — schema bump** | Add fp16 + DP4a-capability fields to `GpuDescriptor`'s MEASURED/QUERIED blocks **keeping `sizeof == 128` by consuming reserved/`_pad` bytes**; bump the schema fingerprint path so a stale cross-process reader hard-fails at attach, not silently mis-maps. | `static_assert(sizeof==128)` holds; the existing `PHYRIAD_ASSERT_POD` + 128-B assert compile; a deliberately-stale mapping is rejected (the schema-hash gate). | DR1, DR2 |
| **VP-1 — query** | In `vdev_create`, query `shaderFloat16` / `shaderInt8` (`VK_KHR_shader_float16_int8`) + `shaderIntegerDotProduct` (`VK_KHR_shader_integer_dot_product`); populate the new descriptor fields. Default path behavior unchanged (query only). | New flags read; default render byte-identical (diff = 0 vs pre-change on the author rig). | CR1 |
| **VP-2 — characterize** | Replace the **inlined `offload=false` constant** with either (a) a real `break_even_decide` call over the populated descriptors, or (b) an explicitly-named characterized per-rig constant with the vendor assumption removed. Default decision MUST remain `offload=false` on the author rig (FG AI≈2.5 ⇒ decline) so the change is byte-identical there. | The decision is now a function of measured fields, not a literal; on the author rig it still yields `offload=false` (byte-identical); grep for a vendor string in the decision = 0. | VA-2 closure; DG1 (dogma: byte-identical-off) |
| **VP-3 — capability-route the role select** | Make the `single_gpu` collapse + role assignment robust to a single non-NVIDIA GPU: keep the `deviceType` branch but ensure the *capability* fallback (fp16-default Tier-0) is what runs when no NVIDIA-specific path applies — no `vendor_id`/name in the route. | On the author rig: identical role assignment (byte-identical). Code review confirms the single-GPU degenerate path is capability-gated, not vendor-gated. | CR2 |
| **VP-4 — count audit** | A committed grep manifest proving VA-1..VA-4 = 0 (the fields exist; the decision is capability-driven; no vendor string in any enable/route logic). | The four greps in the strategies doc run and show the documented counts; the manifest is the E1-readiness evidence. | — |

**Hard gate (Tier-2):** VP-0..VP-3 MUST NOT be committed while any RISK_REGISTER risk is `open`
(PLAN_TIER_PROTOCOL §3). Every new flag, if any, is **default-off, byte-identical-off** (dogma); but note
the *intent* here is that the DEFAULT path becomes vendor-agnostic — so the byte-identical assertion is
**against the author's NVIDIA rig** (the only rig available): the change must produce bit-identical output
*there*, while becoming *representable + correct-by-construction* elsewhere.

---

## §5 — The E1 closeable criterion (the target) vs the floor (named, not beaten)

- **★ Closeable (vista f / dossier §2(f)):** the default flag-free path runs correct + non-crashing ≥60 s
  on one non-author NVIDIA ☐, one AMD ☐, one Intel ☐ — each with a `fg_quality_scorer` run in the
  LSFG-comparable band + a clean telemetry CSV + the capability table populated by runtime CHARACTERIZE
  (no vendor name in the decision path, grep = 0).
- **The honest floor (NAMED, NOT planned to beat):** the blocker is **hardware access**. This plan delivers
  the *grep = 0 / capability-table-populated / fp16-default* half — i.e. **E1-ready code**. The three
  ☐ runs remain OPEN until hardware exists. **This plan re-proposes nothing as a win that is an
  irreducible.** The progress claim is bounded to "VA-1..VA-4 = 0", which IS the operator-named metric.

---

## §6 — Honesty buckets (per claim)

- `[VS]` (first-hand this session): every §1 current-state row, the NVOFA-gated-and-correct finding, the
  F2/VRR-is-messaging finding, the 0-callers-of-`break_even_decide` finding.
- `designed`: §2 architecture, §4 phases, the VP-0..VP-4 edits (none built).
- `[V1]/[V2]` (inherited from the dossier, NOT re-fetched this session): the AMD/Intel-faster-fp16
  inversion `[V2]`, DP4a cross-vendor scope `[V1]`, no-HW-OFA-on-AMD/Intel `[V1]`.
- **Named floor, not a claim of victory:** the cross-vendor run (hardware-blocked).
