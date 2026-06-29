<!-- FG_VENDOR_AGNOSTIC_RISK_REGISTER — the Tier-2 failure-mode register for the E1 de-vendoring:
     a cross-process POD layout change + a device-creation feature query + an offload-decision change. -->

# FG_VENDOR_AGNOSTIC_RISK_REGISTER — Tier-2 risks for the E1 de-vendoring

> **Diátaxis type:** reference (the enumerated failure modes + mitigation-as-code + verification).
> Companion of [`FG_VENDOR_AGNOSTIC_MASTER_PLAN.md`](FG_VENDOR_AGNOSTIC_MASTER_PLAN.md) and
> [`FG_VENDOR_AGNOSTIC_IMPLEMENTATION_STRATEGIES.md`](FG_VENDOR_AGNOSTIC_IMPLEMENTATION_STRATEGIES.md).
> Required because this change is **Tier 2** (PLAN_TIER_PROTOCOL §1.1): it changes a **cross-process-
> published POD** (`GpuDescriptor`, `static_assert(sizeof==128, "…published cross-process; keep it stable")`
> `[VS] GpuDescriptor.hpp:61`) — a data-format/irreversibility trigger §1.1(4) — and touches Vulkan
> **device creation** (the OFA-adjacent feature query) — a crash/device-loss-adjacent surface §1.1(1) — and
> puts the **byte-identical-off dogma** at stake §1.1(5).
>
> **Status:** **E1/D1 BUILT + supervisor-verified 2026-06-24** — as an **APP-LOCAL VDev addition** (the FG links
> no `framework/gpu`; the canonical `GpuDescriptor` POD widen was AVOIDED — the G3 fork-by-non-adoption, CANON
> #12). So **DR1 + DR2 are `N/A`** (no cross-process POD is touched — the FG's `VDev` is in-process, never
> SHM-published / `sizeof`-asserted, grep=NONE → the data-format risks' premise does not exist here);
> **CR1 + CR2 + DG1 + DG2 `mitigated`** by code-in + first-hand code-verify (the capability chain is QUERY-ONLY /
> never-enabled → byte-identical default device-creation, `main.cpp:2024-2034`; `--cap-route-probe` is a
> DEFAULT-OFF diagnostic that routes NOTHING — the FG's A/B/G roles are architecture-fixed, `offload=false`
> always; vendor strings confined to annotated messaging). **No risk `open` → commitable.** Built for COVERAGE
> under the operator's §4-flexibility (2026-06-24); speculative + inert-by-default on the operator's NVIDIA /
> fixed-role FG. The cross-vendor RUN is hardware-blocked (NVIDIA-only rig) — the named hardware FLOOR, BATCHED
> for the end test session, not a risk. **The Tier-2 gate (§3): each risk `mitigated` / `accepted` / `N/A` before
> commit — MET.**
>
> **Honesty:** a `mitigated` status here is `measured`/`verified` (the named gate was actually run on the
> author rig), never assumed (FDP).

---

## §1 — The register

| ID | Class | Failure mode (file:line) | Mitigation + attached code | Verification | Status |
|---|---|---|---|---|---|
| **DR1** | data-format | A stale cross-process reader maps a `GpuDescriptor` SHM artifact written by a NEW binary (with the fp16/DP4a fields) using the OLD field layout → silently mis-reads capability (e.g. reads `has_fp16` bytes as `compute_gflops` mantissa) and **routes on garbage**. Site: any consumer mapping the descriptor; layout owned by `GpuDescriptor.hpp:27-59`. | New fields **consume existing `_pad0`/`_pad1` reserved bytes**, keeping `sizeof==128` (so the artifact size is unchanged) **AND** the schema fingerprint (`schema::schema_hash<GpuDescriptor>()`, which mixes `sizeof`+`alignof`+`__PRETTY_FUNCTION__`+`kPhyriadHalVersion` per CANON principle 7) changes because the type's pretty-name/field set changes → a stale mapping **hard-fails at attach** (the `magic`/hash gate), not silently corrupts. Code: keep `static_assert(sizeof==128)`; ensure any persistent/SHM header carrying the descriptor includes its `Hash128` fingerprint (CANON principle 7). | A unit test maps a descriptor written with the new layout via a header carrying the new hash, and a synthetic OLD-hash header is **rejected at attach** (returns an error, does not return torn data). Build asserts hold. | see header (supervisor-reconciled 2026-06-24) |
| **DR2** | data-format | The new fields do NOT fit in the reclaimed `_pad` budget → `sizeof != 128` → the `static_assert` fires (build break) OR (if "fixed" by bumping the literal) every existing persisted descriptor silently changes meaning. Site: `GpuDescriptor.hpp:61`. | Do the **exact byte accounting** in the edit (S1): identity 16B + capability 16B + interconnect 8B + measured 20B + pads = 128. Adding `has_fp16`+`has_dp4a` (2×1B) + `fp16_gflops` (4B) = 6B reclaimed from the 11 pad bytes (`_pad0[6]`+`_pad1[5]`) → fits with `_pad` shrunk to 5B total. Keep `static_assert(sizeof==128)` as the build-time proof. If it ever cannot fit, the size bump is a **separate, explicitly-versioned Tier-2 increment** (not a silent literal change). | The `static_assert(sizeof==128)` compiles green AFTER the edit (it is the proof). A byte-count comment in the struct documents the accounting. | see header (supervisor-reconciled 2026-06-24) |
| **CR1** | crash / device-loss | The new `VkPhysicalDeviceShaderFloat16Int8FeaturesKHR` / `…IntegerDotProductFeaturesKHR` feature-chain in `vdev_create` is mis-chained (wrong `sType`, dangling `pNext`, or enables a feature the device lacks) → `vkCreateDevice` fails or `VK_ERROR_DEVICE_LOST`. Site: `apps/render_assistant/src/main.cpp` `vdev_create` (line 1930), the `Features2` chain (mirrors the OFA feature chain ~line 2020 `[VS]`). | **QUERY-ONLY, never ENABLE** at this phase: chain the feature structs onto the `GetPhysicalDeviceFeatures2` *query*, read the bits into `has_fp16`/`has_dp4a`, and do **NOT** add them to the enabled-feature chain of `vkCreateDevice` and do NOT request the extensions in the default path. The default device-creation path is byte-for-byte the pre-change path. Code: the query chain is local to the capability read; the create-info chain is unchanged. | A 30-s default-path run on the author rig is crash-free (watchdog clean); validation layers report no error; the default render output is **byte-identical** (diff=0) to the pre-change binary (the query enabled nothing). | see header (supervisor-reconciled 2026-06-24) |
| **CR2** | crash | The role-select edit (S4) alters the existing 4090/1080Ti/iGPU partition → a device-B/G resource is created on a null/aliased handle → UB/crash on teardown (the `FD` device threading, `main.cpp:3248` `[VS]`). | S4 is **audit-first, edit-only-if-found**: confirm no `vendor_id`/name gates the role path; make NO behavioral change unless a latent vendor assumption is found, and if so remove it WITHOUT changing the `single_gpu`/`FD`/null-out logic (`main.cpp:3233-3234` `[VS]`). The single-GPU degenerate already nulls pB/pG so the path is driven; do not touch it. | Author-rig multi-GPU run: role assignment + teardown identical (no new validation/teardown error); single-GPU (`--force-single-gpu`) run clean. Output byte-identical. | see header (supervisor-reconciled 2026-06-24) |
| **DG1** | dogma (byte-identical) | The capability-driven offload decision (S3) changes the author-rig decision away from `offload=false` → the default render is no longer byte-identical → the byte-identical-off dogma is broken on the only rig available to verify it. Site: the FG offload-decision point (today the inlined constant, `main.cpp:50-52` `[VS]`). | Feed the **measured** descriptor fields into `break_even_decide(... AI≈2.5 ...)`; on the author rig the link/FLOPS inputs keep the result `offload=false` (FG's AI ≪ the ~596 crossover for that link). The degenerate-input guard (`GpuDescriptor.hpp:102-104` `[VS]`) returns `offload=false` for any not-yet-characterized descriptor — so an uncharacterized rig also declines (honest default). NO behavior flips on the author rig. | Author-rig default run: the decision is still `offload=false`; the FG render is **byte-identical** (diff=0) to pre-change. The decision now reads measured fields (inspection), not a literal. | see header (supervisor-reconciled 2026-06-24) |
| **DG2** | dogma (no vendor lock at canon, CANON principle 11) | A new accelerator-enable path keys on a **vendor string** (`vendor_id == 0x10DE`, device-name `"nvidia"`, etc.) instead of a measured capability → re-introduces vendor lock in the very change meant to remove it. | Every enable/route decision keys on a **capability field** (`has_fp16`/`has_dp4a`/measured throughput), never on `vendor_id`/name. NVOFA stays gated on `VK_NV_optical_flow` **presence** (a capability, `main.cpp:1992` `[VS]`), not on a vendor string. The F2/VRR vendor-string print is annotated as MESSAGING (not a decision). | The §5 count-manifest grep for a vendor string in any decision/enable/route block = **0** (the messaging substring is annotated + excluded). Reviewer confirms. | see header (supervisor-reconciled 2026-06-24) |

---

## §2 — Accepted residuals (none yet)

No risk is `accepted` at design time. The cross-vendor RUN being undone-on-this-rig is **not a risk of this
change** — it is the named **hardware-access FLOOR** (master plan §3/§5): this change ships E1-*ready* code,
and the run is out of scope by floor, so it is not a register row (there is no failure mode to mitigate; the
run simply cannot happen here). It is recorded in the master plan as a floor, not here as an accepted risk.

---

## §3 — The gate, restated

All six rows (DR1, DR2, CR1, CR2, DG1, DG2) MUST reach `mitigated` (verified on the author rig) or
`accepted` (with rationale) before any code from this plan is committed. The verifications are the
**author-rig byte-identical render**, the **`static_assert(sizeof==128)` green**, the **stale-mapping
rejection**, the **30-s crash-free default run** with validation layers clean, and the **vendor-string-
in-decision grep = 0**. Verify-before-claim binds hardest here because the change is Tier-2 and the only
rig is NVIDIA — the cross-vendor correctness is *designed-by-construction* (capability-driven, no vendor
string), not *measured*, and that boundary MUST be stated honestly in the commit.
