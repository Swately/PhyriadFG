# FG_VENDOR_LANDSCAPE_PRIOR_ART — vendor-agnosticism + the full competitive FG landscape

> **Diátaxis type:** Analysis (SOTA dossier). **Status:** `measured` (the 2025-26 field map) / `designed`
> (the vendor-neutral default-path principle). Resolves objective **E1** (the default FG path stays
> vendor-neutral; vendor accelerators are opt-in, measured-gated) + the competitive frame. Companion to
> [`PHYRIADFG_OBJECTIVE_VISTA.md`](PHYRIADFG_OBJECTIVE_VISTA.md) §1-E1.
>
> **Provenance.** From workflow `w83r5ydl9` (the vendor agent). Field facts carry the sweep's
> `[V1]/[V2]/[V3]` levels (vendor docs `[V1]`; LSFG specifics `[V2]`, closed-source). The author did NOT
> independently re-fetch every URL (unlike the safety/HDR dossiers); §5 flags this. Per FDP §2: no
> fabricated citation.
>
> **Normativity (BCP 14):** MUST / MUST NOT used only where a real constraint binds.

---

## §1 — The complete 2025-26 frame-generation field (the competitive map)

The field splits into two structural classes, and the split IS the competitive story.

### Class A — engine-integrated FG (vendor + per-game locked)

| Tool | Marketed objective | Vendor / HW lock | Per-game integration? | What it CANNOT do |
|---|---|---|---|---|
| **DLSS-G / DLSS 4 MFG / 4.5 6X** | up to 4X (MFG) → 6X at CES-2026; lowest artifacts | **MFG/6X/Dynamic = RTX 50 (Blackwell) EXCLUSIVE** `[V1]`; base FG = RTX 40+ OFA | **YES** — Streamline needs depth + MVs + HUD-less colour `[V1]` | run on a non-integrated game; run on AMD/Intel; be forced externally |
| **FSR 3 / 3.1 FG / FSR 4 (ML)** | open-source MIT FG; FSR 4 = ML | FSR3 FG broad; **ML FSR-FG/FSR4 = RDNA 4 (FP8)**; FSR 4.1 backported to RDNA 3 (INT8) `[V1/V2]` | **YES** | same — needs engine MVs + depth + per-title integration |
| **XeSS 2 / XeSS-FG / XeSS 3 MFG** | AI FG, now MFG | XMX preferred; **XMX requirement DROPPED** — DP4a/SM6.4 fallback runs on GTX 10-series, RX 5000, any SM6.4 GPU `[V1]` | **YES** — "not driver-level; each game must integrate" `[V1]` | run on a non-integrated game (cross-vendor in HW, still per-title) |

**Class-A takeaway:** all three big stacks are **per-game-integration-locked** (they need the engine to
hand over motion vectors + depth + HUD-less colour). None can be forced onto an arbitrary game. That gate
is the wall LSFG and PhyriadFG walk around. `[V1]`

### Class B — external / driver-level FG (the LSFG tier — PhyriadFG's actual class)

| Tool | Vendor/HW lock | Game integration? | Moat / loss |
|---|---|---|---|
| **Lossless Scaling (LSFG 3 / 3.1 AFG)** | **VENDOR-AGNOSTIC** — any DX11/12/Vulkan game in borderless; ~GTX 1060/RX 580 floor; proprietary from-scratch ML model + a separate low-power model; any-vendor dual-GPU incl. iGPU `[V2]` | **NO** — pure external, no MVs/depth | **MOAT:** universal capture + smoothness + per-game profiles + true any-vendor dual-GPU. **LOSS:** no Reflex hook; MV-free quality ceiling |
| **AFMF / AFMF2 / 2.1** | **AMD-ONLY** (driver-level, RDNA2/3 + 700M/800M iGPU) `[V1]` | NO (driver hook, MV-free) | lower lag than LSFG (driver present hook + droppable frame) but **self-disables in fast motion** + AMD-locked; worse smoothness `[V2]` |
| **OptiScaler / OptiFG** | wrapper; OptiFG = FSR3-FG, **DX12-only** | swaps an upscaler's hooks | per-API locked, not universal external capture `[V2]` |

**Class-B takeaway:** only **two** tools are in PhyriadFG's exact class (external, MV-free, forceable on any
game): LSFG and AFMF. AFMF is AMD-locked + self-disables. **LSFG is the ONLY tool in the entire field that
is BOTH (a) any-game-no-integration AND (b) any-vendor-GPU.** That double-universality is its precise moat
— the standard PhyriadFG must meet to be a real competitor, **not just on the author's rig.** `[V2]`

### LSFG's moat, pinned

(1) any game, no integration (shared with AFMF; AFMF AMD-only); (2) any-vendor single-GPU path (~GTX
1060/RX 580 of *any* brand; a separate low-power model for weak GPUs); (3) any-vendor dual-GPU (secondary
can be a different brand/iGPU; display must connect to the secondary; *AMD-render + NVIDIA-secondary* can
fail to launch, *NVIDIA-render + AMD/Intel-secondary works* — PhyriadFG's same-vendor 4090+1080Ti avoids
the failing combo by accident); (4) two-click + per-game profiles. **Where LSFG loses:** no Reflex hook;
MV-free quality ceiling; cannot pull HW optical-flow (it gave up HW-OFA to stay portable). `[V2 + V1]`

### The vendor-accelerator hardware map (the E1 design substrate)

| Accelerator | Vendor scope | Verdict for a default path |
|---|---|---|
| **NVOFA** (HW optical flow) | NVIDIA Turing+ only; no AMD/Intel equivalent `[V1]` | **Opt-in only.** A default depending on it is NVIDIA-only → fails E1 |
| **DP4a** (int8 dot product) | **cross-vendor** — D3D12 SM6.4 / Vulkan; NVIDIA Pascal+, AMD, Intel `[V1/V2]` | **Acceptable as a gated accelerator** (Intel's own XeSS-FG fallback rides it) |
| **fp16 packed math** | cross-vendor; **AMD/Intel often FASTER fp16 than consumer NVIDIA** `[V2]` | **Should be the DEFAULT compute primitive** |

> **★ Key E1 inversion (from the LSFG dual-GPU guide `[V2]`):** AMD/Intel GPUs *out-perform* NVIDIA at the
> FG pass because of superior fp16 throughput. PhyriadFG's instinct to lean on NVOFA (an NVIDIA-only HW
> block) is therefore **backwards for E1** — the portable winner is fp16/DP4a; NVOFA is a 4090-specific
> bonus, not the path.

---

## §2 — The objective resolved (E1)

- **(a) Perfection target.** The default (flag-free) pipeline produces correct, smooth output on a *single*
  GPU of each vendor — NVIDIA (Pascal+), AMD (RDNA), Intel (Arc/Xe iGPU) — using only portable primitives
  (Vulkan compute + fp16, optional DP4a). Every vendor accelerator (NVOFA) is opt-in, measured-gated, and
  when absent/net-negative leaves the default path unchanged and competitive with LSFG on that GPU.
- **(b) Honest floor.** No Reflex offset on any vendor (shared with all external tools). No HW-OFA on
  AMD/Intel → OFA acceleration is intrinsically single-vendor; "OFA on every vendor" is unreachable by
  physics. The AMD-render + NVIDIA-secondary launch failure is a driver-class floor LSFG hits too.
- **(c) Standing (first-hand).** Single-GPU collapse SHIPS (`--force-single-gpu`, RR discharged). BUT the
  router is **effectively dead in-product**: `break_even_decide` appears **once** in `main.cpp` `[VS via
  roadmap §11]`; `offload=false` effectively hardcoded; **`GpuDescriptor` carries `compute_gflops` only —
  no fp16/DP4a field**; the break-even crossover is the author's 4090-measured constant. NVOFA is integrated
  but default-OFF + net-negative (STAGE-115). **No cross-vendor run has ever happened — E1 is UNVERIFIED
  ("great on the author's NVIDIA pair, untested on a stranger's single Arc/AMD"). Exactly the operator's
  stated fear, currently true by omission.**
- **(d) LSFG-specific gap.** **We LOSE on E1 today.** LSFG is *verified* any-vendor (mixed-vendor user base,
  iGPU secondaries) + ships a separate low-power model; PhyriadFG has neither a measured cross-vendor run
  nor a low-power code path, and its capability table can't even represent a non-NVIDIA GPU (no fp16/DP4a
  field). We **match the structural primitives** (Vulkan/compute, MV-free, borderless-capture) — the gap is
  *unwired router + unverified*, not *unbuildable*. We could **lead** on dual-GPU honesty (a *measured*
  "use the secondary only if net-positive" gate LSFG lacks) — unrealized until P5.
- **(e) Closing levers.** (1) add fp16/DP4a to `GpuDescriptor` + wire `measure_transfer_bw` /
  `select_participants` at startup — **T2** (cross-process POD change → operator-approval-gated). (2) make
  fp16 the default primitive; gate NVOFA & DP4a behind per-rig measured break-even (never ship the author's
  ~596 FLOP/byte constant as a default) — T1. (3) a low-power-GPU default profile (LSFG's "separate model"
  analogue) — T1. (4) run the cross-vendor matrix (T0 mechanically; needs hardware access — see (f)).
- **(f) ★ Closeable success criterion (E1) — binary, not asymptotic:**
  > **E1-DONE ≡** the default (flag-free) path produces correct, artifact-bounded, non-crashing output for
  > ≥60 s on a *single* GPU of each vendor — one NVIDIA (DP4a path, no 2nd GPU), one AMD (RDNA2), one Intel
  > (Arc/Xe iGPU) — each evidenced by (i) a `fg_quality_scorer` run within the LSFG-comparable band on that
  > GPU and (ii) a clean telemetry CSV (no fallback-to-vanilla, no device-loss), with the capability table
  > populated by runtime CHARACTERIZE (no vendor name in the decision path — grep = 0).
  > - ☐ E1-NV (non-author NVIDIA, DP4a, single-GPU) · ☐ E1-AMD · ☐ E1-INT.
  > **Honest gating caveat:** the blocker is HARDWARE ACCESS (the author has 4090+1080Ti, both NVIDIA), not
  > code. **The measurable progress metric until hardware is available:** the count of hardcoded-vendor
  > assumptions driven to zero — (a) deviceType/LUID hardcodes in `main.cpp`, (b) the NVIDIA-only break-even
  > default, (c) `GpuDescriptor` fields that can't represent a non-NVIDIA GPU. When those reach 0 the code
  > is *E1-ready* and only the run remains.

### Sub-objective — the vendor-accelerator design principle (a measured 3-tier ladder)

- **Tier 0 (portable default):** Vulkan compute + fp16 + the 8×8-SAD pyramid flow. Every SM6.0+ GPU. *This
  is the E1 path and the LSFG-parity path.*
- **Tier 1 (DP4a, cross-vendor):** if the runtime reports DP4a (SM6.4 / `VK_KHR_shader_integer_dot_product`),
  use int8 flow correlation — present on NVIDIA Pascal+, AMD, Intel. Gated on a measured net-positive
  (Intel's XeSS-FG fallback validates this tier works cross-vendor `[V1]`).
- **Tier 2 (NVOFA, NVIDIA-only bonus):** only when the GPU exposes NVOFA AND the measured slice is
  net-positive on *this* rig. Today net-*negative* (STAGE-115) → correctly off.
- ★ **Closeable criterion:** flipping every accelerator OFF yields **byte-identical** Tier-0 output on any
  GPU, AND each accelerator auto-enables only when a per-rig measured break-even shows net-positive (grep
  for vendor-string branches in the enable logic = 0). Binary and grep-checkable. **Today: only Tier 0
  exists + a dormant NVIDIA-only Tier 2; the cross-vendor Tier 1 (DP4a) is absent** — the one tier that
  would match XeSS-FG's portable acceleration on AMD/Intel.

---

## §3 — Sources (leveled)

`[V1]` (vendor primary): NVIDIA GeForce DLSS 4.5 / 6X news + DLSS4-MFG RTX-50-exclusivity (VideoCardz,
Tom's Hardware); DLSS-G Streamline programming guide (needs depth+MV+HUD-less); AMD FSR 4.1→RDNA3 /
FSR4→RDNA4 (Neowin, XDA, Tom's Hardware); AFMF2 AMD-only release notes; Intel XeSS-FG drops XMX / DP4a-SM6.4
(Club386), XeSS 2 all-GPU (JonPeddie), XeSS 3 MFG (Tom's Hardware); NVOFA Turing+ (NVIDIA OFA SDK); DP4a
D3D12 SM6.4 cross-vendor (gpuweb). `[V2]`: LSFG dual-GPU guide (sageinfinity); LSFG VideoCardz 3.1 AFG;
OptiScaler OptiFG wiki. In-repo `[VS]` (via roadmap §11 ledger): `PHYRIADFG_PERFECTION_ROADMAP.md` §1
(lines 89, 132), §2.6, §11; `FG_OPEN_ALTERNATIVES_PRIOR_ART.md`.

## §4 — Cross-links

The capture/present class boundary → [`FG_COMPATIBILITY_COVERAGE_PRIOR_ART.md`](FG_COMPATIBILITY_COVERAGE_PRIOR_ART.md).
NVOFA as a flow provider → `FG_NVOFA_PRIOR_ART.md`. Multi-GPU routing → `GPU_MULTI_GPU_PRIOR_ART` (memory)
+ `FG_ARCHITECTURE_DCAD_MASTER_PLAN.md §5`.

## §5 — What could NOT be verified (first-hand)

- **LSFG's exact internal architecture** (ML model size, internal DP4a/fp16 use) — proprietary; the
  "from-scratch model + separate low-power model" + "any-vendor dual-GPU" are `[V2]` community/guide; the
  official losslessscaling.com page publishes no system requirements.
- **Whether PhyriadFG's default path actually runs on a single AMD/Intel GPU** — *no first-hand evidence
  either way*; the vendor-hardcoding (dead router, no fp16/DP4a field) is from the roadmap's `[VS]` ledger,
  not re-grepped this session; no cross-vendor run has been done. **The central honest unknown.**
- **DLSS 4.5 6X exact HW gate** — RTX-50-exclusivity is `[V1]`; the "Flip Metering in the display engine"
  attribution is `[V2]`.
- **The AMD/Intel-faster-fp16 claim** is `[V2]` (LSFG dual-GPU guide) — directionally consistent with the
  well-known consumer-NVIDIA fp16 gimping, but the specific "out-perform at the FG pass" is community-sourced.
- **The author did not re-fetch the §1/§3 URLs first-hand this session** — they carry the sweep's levels.
