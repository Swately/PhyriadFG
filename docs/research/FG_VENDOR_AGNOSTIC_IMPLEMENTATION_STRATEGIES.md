<!-- FG_VENDOR_AGNOSTIC_IMPLEMENTATION_STRATEGIES — the per-edit-site plan for E1: representable
     GpuDescriptor, queried fp16/DP4a, capability-driven offload, no vendor string in any decision path. -->

# FG_VENDOR_AGNOSTIC_IMPLEMENTATION_STRATEGIES — the per-edit sites for E1

> **Diátaxis type:** how-to (concrete edit sites + the byte-identical discipline + the validation gates).
> Companion to [`FG_VENDOR_AGNOSTIC_MASTER_PLAN.md`](FG_VENDOR_AGNOSTIC_MASTER_PLAN.md) (the why/phases) and
> [`FG_VENDOR_AGNOSTIC_RISK_REGISTER.md`](FG_VENDOR_AGNOSTIC_RISK_REGISTER.md) (the Tier-2 failure modes).
> Each edit site cites the risk ID it mitigates (PLAN_TIER_PROTOCOL §2.3).
>
> **Status:** `designed`. Every CURRENT-code line is `[VS]` (verified first-hand this session). Every
> TARGET edit is `designed` (not yet written). **No fabricated path/line/API** (FDP verifiable-reference
> mandate).
>
> **Tier 2.** The hard gate (no commit while any register risk is `open`) binds this whole document.

---

## §0 — The discipline that governs every edit

1. **Byte-identical-off / byte-identical-on-the-author-rig (dogma).** The author has only NVIDIA. The
   *intent* of E1 is that the DEFAULT path becomes vendor-agnostic; therefore the binding assertion is:
   **on the 4090(+1080Ti) rig, the default render output is bit-identical before vs after every edit in
   this plan.** A diff != 0 there is a regression, regardless of cross-vendor intent.
2. **No vendor string in any decision path.** `vendor_id` / device-name substrings MAY appear in
   identity/telemetry/honest-messaging, MUST NOT gate FG behavior. The closeable grep (§4) enforces this.
3. **POD layout is frozen at 128 B.** `GpuDescriptor` is cross-process-published; new fields consume
   existing `_pad` bytes — `sizeof` MUST stay 128 (`GpuDescriptor.hpp:61` `[VS]`). (See DR1/DR2.)
4. **HAL discipline.** Any new atomic goes through `hal::` wrappers; none is expected here (these are
   cold init-path edits), but `lint_hal` MUST stay clean.

---

## §1 — Strategy S1 (VP-0): make `GpuDescriptor` representable for any vendor

**Current `[VS]`** (`framework/gpu/include/phyriad/gpu/GpuDescriptor.hpp`):
- MEASURED block, line 49: `float compute_gflops` is the only throughput field.
- `_pad0[6]` (line 34), `_pad1[5]` (line 46) — reserved bytes already present.
- `static_assert(sizeof(GpuDescriptor) == 128u, …)` line 61; `alignof == 8` line 63;
  `PHYRIAD_ASSERT_POD` line 64.

**Edit (designed):**
- Add to the **QUERIED capability block** (cold, populated by the backend):
  `uint8_t has_fp16 = 0;` (shaderFloat16 supported), `uint8_t has_dp4a = 0;`
  (shaderIntegerDotProduct / SM6.4 int8-dot supported). Consume two of the existing `_pad0[6]`/`_pad1[5]`
  reserved bytes so **`sizeof` stays 128** (shrink the matching `_pad` array, do NOT append after the
  measured block).
- Add to the **MEASURED block** (filled only by the harness; zeroed == not-characterized — honest):
  `float fp16_gflops = 0.0f;` (measured fp16 packed-math throughput — the E1 routing key that is currently
  unrepresentable). Consume reserved space by **re-deriving the 128-B budget**: the current measured block
  is 5 × `float` (20 B) + the identity/capability/interconnect blocks. Adding one `float` requires
  reclaiming 4 B from `_pad`; document the exact byte accounting in the edit so the 128-B assert is met by
  construction, not by luck.
- **Do NOT widen the struct.** If the byte budget cannot absorb both new capability bytes and the new
  float within 128 B, split into a follow-up that bumps the published-size constant + the schema
  fingerprint deliberately (a separate Tier-2 increment) — see DR2. The first increment SHOULD fit by
  consuming `_pad`.

**Mitigates:** DR1 (stale cross-process mis-map), DR2 (size-assert break). **Verification:** the two
`static_assert`s + `PHYRIAD_ASSERT_POD` compile; a unit check writes a descriptor with the new fields and
round-trips it; a deliberately-stale schema-hash mapping is rejected at attach.

**Doc-sync:** `GpuDescriptor` is a public header of the `gpu` pillar → `docs/reference/gpu.md` MUST be
updated in the same commit (`check_doc_sync.sh`), and `INVENTORY.md` if the file's brief changes.

---

## §2 — Strategy S2 (VP-1): query fp16 / DP4a at device creation

**Current `[VS]`** (`apps/render_assistant/src/main.cpp`):
- `vdev_create(...)` line 1930 enumerates device extensions; line 1942 checks
  `VK_NV_OPTICAL_FLOW_EXTENSION_NAME`. **No `shaderFloat16` / `shaderInt8` /
  `shaderIntegerDotProduct` query exists** (grep = 0 `[VS]`).

**Edit (designed):**
- In `vdev_create`, chain `VkPhysicalDeviceShaderFloat16Int8FeaturesKHR` (and, for DP4a,
  `VkPhysicalDeviceShaderIntegerDotProductFeaturesKHR`) onto the `VkPhysicalDeviceFeatures2` query already
  used for OFA, and set `d`'s new `has_fp16` / `has_dp4a` bits. **Query only — no enablement change, no
  pipeline change.** This is observation, mirroring how `has_optical_flow` is captured (`main.cpp:1942-1943`
  `[VS]`).
- Populate the new `GpuDescriptor` capability bytes from these for any rig where a descriptor is built.

**Mitigates:** CR1 (a query/feature-chain that changes device creation could crash or alter the default).
**Verification:** default render on the author rig is **byte-identical** (diff = 0) — the query MUST NOT
enable any feature or change any pipeline; only `has_fp16`/`has_dp4a` are now populated. Validation layers
clean.

**Note (fp16 as the DEFAULT primitive):** the flow path's storage format (the actual fp16 default) lives in
the shader/`OpticalFlowPipeline` plumbing, not in `main.cpp` (the format constants are not in the header
read this session — `[VS]` grep = 0 in `OpticalFlowPipeline.hpp`). **Making fp16 the *executed* default
primitive is a follow-up** once `has_fp16` is queryable: this plan delivers the *queryable capability +
the declared intent*, and the flow-format default change is gated on a per-rig measured net-positive (it
MUST NOT regress the author rig's bit-output → it is its own measured step, not a blind flip). Stated
honestly so a later pass does not claim "fp16 is the default" before the flow format is actually switched.

---

## §3 — Strategy S3 (VP-2): replace the inlined offload constant with a capability-driven decision

**Current `[VS]`** (`apps/render_assistant/src/main.cpp:50-52`):
```
// STAGE-26b router note: the gpu pillar's break_even_decide() was evaluated for the FG
// offload decision — at FG's AI≈2.5 FLOP/byte (vs the ~596 crossover) it returns
// offload=false ALWAYS, a constant. Inlined as such.
```
`break_even_decide` has **0 call sites in `main.cpp`** (`[VS]`); the `~596` crossover is the author's
4090+1080Ti-measured constant (`GpuDescriptor.hpp:79` `[VS]`).

**Edit (designed):** at the FG offload-decision point, call
`phyriad::gpu::break_even_decide(supervisor_flops, assistant_flops, link_bytes_per_s,
arithmetic_intensity≈2.5)` using the **measured** descriptor fields (S1/S2) rather than an inlined literal.
On the author rig the inputs still yield `offload=false` (FG's AI≈2.5 ≪ the crossover for *that* link), so
the behavior is **byte-identical there** — but the decision is now a function of *measured capability*, not
a vendor-tuned constant. If a descriptor is not yet characterized (`compute_gflops == 0`), the function
already returns `offload=false` by its degenerate-input guard (`GpuDescriptor.hpp:102-104` `[VS]`) — the
honest default.

**Mitigates:** VA-2 closure; DG1 (byte-identical-off dogma). **Verification:** on the author rig the
decision is still `offload=false` (byte-identical render); a unit/inspection check shows the decision now
reads `compute_gflops`/`fp16_gflops`/link fields, not a literal; grep for a vendor string in the decision
block = 0.

---

## §4 — Strategy S4 (VP-3): keep the role-select capability-gated, not vendor-gated

**Current `[VS]`** (`apps/render_assistant/src/main.cpp:3210-3213`): primary = LUID-match; integrated →
iGPU role; discrete → assist role. `single_gpu = cfg.force_single_gpu || (pA && !pB)` (line 3233 `[VS]`);
under `force_single_gpu`, pB/pG are nulled (line 3234 `[VS]`) so the degenerate path is driven.

**Finding:** this is already **capability/role-shaped, not vendor-name-shaped** — it branches on
`deviceType` (a portable Vulkan enum) and LUID (the panel-owner identity), never on `vendor_id` or a vendor
string. The single-GPU collapse (the arbitrary-single-vendor case E1 cares about) is reached via
`single_gpu`. **The de-vendoring edit here is minimal:** confirm + assert (in a comment + the count
manifest) that no `vendor_id`/name gates the role assignment, and that the single-GPU degenerate runs the
Tier-0 fp16 default. No behavioral edit is required if the audit confirms it; if any latent vendor
assumption is found in the role path, remove it.

**Mitigates:** CR2 (a role-select change could break the existing 3-GPU split). **Verification:** author-rig
role assignment **byte-identical**; the count manifest (§5) shows 0 vendor strings in the role path.

---

## §5 — Strategy S5 (VP-4): the count manifest (the E1-readiness evidence)

A committed text artifact (e.g. `apps/render_assistant/bench/e1_vendor_count.txt` or a script) that runs
and records these greps, proving VA-1..VA-4 = 0:

| Check | Command (ripgrep) | Pass condition |
|---|---|---|
| VA-1 closed | descriptor has fp16/DP4a fields | `rg "has_fp16|has_dp4a|fp16_gflops" framework/gpu/include/phyriad/gpu/GpuDescriptor.hpp` ⇒ ≥3 hits |
| VA-2 closed | offload decision is a call, not a constant | `rg -n "break_even_decide\(" apps/render_assistant/src/main.cpp` ⇒ ≥1 **call** (today: 0) |
| VA-3/VA-4 vendor-clean decision paths | no vendor string gates behavior | `rg -n "0x10[Dd][Ee]|0x1002|0x8086|vendor_id\s*==|\"nvidia\"|\"amd\"|\"intel\"" apps/render_assistant/src/main.cpp` ⇒ **0 in any enable/route/decision block** (the F2/VRR *messaging* substring match is permitted + must be annotated as messaging, not a decision) |
| fp16 queryable | the feature query exists | `rg -n "ShaderFloat16Int8|shaderFloat16" apps/render_assistant/src/main.cpp` ⇒ ≥1 (today: 0) |

The manifest is the artifact the operator reads as "the code is E1-ready; only the cross-vendor RUN
remains." It does NOT assert the run is done (that is the hardware-blocked floor).

**Excluded from the count by design (named so the grep is honest):**
- The **F2/VRR vendor-string messaging** (`main.cpp:3226-3231` `[VS]`) — honest startup print, not a
  decision. It MUST be annotated (a `// E1: messaging, not a decision path` comment) so the manifest can
  distinguish it. Removing it would lose an honest statement; it stays.
- `vendor_id` as a **struct field / telemetry** — identity, not a gate. Permitted.

---

## §6 — Validation summary (verify-before-claim, per phase)

| Phase | Build gate | Runtime/output gate | Static gate |
|---|---|---|---|
| S1 (VP-0) | `sizeof==128` + POD asserts compile; `docs/reference/gpu.md` synced | descriptor round-trip unit check; stale-schema mapping rejected | — |
| S2 (VP-1) | compiles; validation layers clean | author-rig render **byte-identical** (diff=0) | grep fp16-query ≥1 |
| S3 (VP-2) | compiles | author-rig decision still `offload=false`, render byte-identical | grep vendor-in-decision = 0 |
| S4 (VP-3) | compiles | author-rig role assignment byte-identical | grep vendor-in-role = 0 |
| S5 (VP-4) | — | — | the §5 manifest greps all pass |

Each "byte-identical" gate is **measured** on the author rig (the only rig), not assumed. A `mitigated`
risk-register row is only marked so after its gate is actually run (PLAN_TIER_PROTOCOL §3.2).
