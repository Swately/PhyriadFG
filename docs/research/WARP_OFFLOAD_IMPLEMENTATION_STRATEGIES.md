# WARP_OFFLOAD_IMPLEMENTATION_STRATEGIES — STAGE-103 `--warp-offload`

> **Diátaxis type:** How-to / strategy (the concrete options for building STAGE-103,
> each with its alternatives weighed). Sibling to `WARP_OFFLOAD_MASTER_PLAN.md` — read
> that first for the goal, the measured walls, and the design contract D1–D6.
> **Status:** `designed` (nothing built). Every `file:line` is to be **re-verified
> first-hand by the implementer** before editing — the working tree has uncommitted
> STAGE-98..102 that shift numbers; map-sourced anchors are tagged `[map]`.
> **Audience:** the implementation holon (a fresh protocol-loaded Opus) + the supervisor
> who verifies it.

---

## How to read this

Each strategy Sx states the **decision**, the **options**, the **weighing**, and the
**pick**. The picks compose into the "Recommended build order" at the end. The design
contract D1–D6 from the master plan is normative throughout; where a strategy interacts
with a contract clause it is named (e.g. *(D1: byte-identical-off)*).

---

## S1 — The B-side warp pipeline `wapPipeB`

**Decision:** how to stand up the warp compute pipeline on `B.dev`.

**Options.**
- **(a) Reuse `wap_create(...)` with B as the device argument.** The function already
  takes a device + the image views + the SPIR-V (it builds the sampler, the 14-binding
  DSL, the 200 B push layout, the pipeline, the pool, the set). Call it a second time
  with `B.dev` and the B-side views. *(map reader 1: `wap_create` builds all of this.)*
- **(b) Hand-roll a B-specific creation block.** More code, no benefit.

**Weighing.** (a) is the D-12 (don't-reinvent) path and reuses the *same* embedded SPIR-V
(`kWapWarpSpv`), so A and B run byte-identical shader logic — essential for the experiment
to be a fair A-vs-B comparison. The only B-specific work is allocating the B-side images
(S2) and passing their views.

**Pick: (a).** Gate the entire `wapPipeB` + B-image allocation on `cfg.warp_offload`
*(D1)*. On any `vkCreate*` failure, release the partial pipeline, log
`[ra] warp-offload: wapPipeB create failed — falling back to A`, and force
`cfg.warp_offload=false` *(D4; mirrors the STAGE-102 slot-1 fallback and the `--gme-gpu`
fallback `[map]`)*.

---

## S2 — Input residency on B (what to allocate, what is already there)

**Decision:** which of the 14 warp bindings need new B allocations vs are already resident.

**Findings (map readers 2+3, consistent with the current tree).**
- **Already on B (no new transfer):** the two reals (`Bframe[0]`/`Bframe[1]`), forward MV
  (`ofp.motion_image()`), SAD (`ofp.sad_field_image()`), candidate (`cand_image()` when
  `--ambig`), dissidence (B-produced when `--gme-gpu`). The `devMass` SSBO can be a B-local
  4-byte buffer.
- **A-only today → must be allocated on B and fed:** inertia `wapPERA` (R8 grid), the iGPU
  contour field `wapFIELDA` (from G — host-bounce G→B instead of G→A), the vblend target
  `wapMVTA` (RG16F grid), the ts-smooth history `wapPrevOutA` (RGBA8 full-res; becomes
  B-local feedback once the warp is on B).
- **Completeness-trick MUST be preserved:** `wap_create` writes ALL bindings
  unconditionally; off-feature bindings point at a valid placeholder view (e.g. `wMV.view`).
  The B-side set MUST replicate the exact placeholder chain or descriptor writes read
  uninitialized data *(map reader 1 risk #3/#4; verify the placeholder logic first-hand)*.

**Pick.** Allocate on B only the masks that are A-only today (`wapPERA_B`, `wapFIELDA_B`,
`wapMVTA_B`, `wapPrevOutA_B`) plus `wapOutB` + `devMass_B`; bind the already-resident B
images directly. VRAM cost ≈ 8.3 MB (`wapOutB`) + the masks ≈ **~20-30 MB net** *(map
reader 2: well within the 1080 Ti's ~11 GB)*. All gated on `cfg.warp_offload` *(D1)*.

---

## S3 — The output path B→A (the dominating cost — weigh carefully)

**Decision:** how `wapOutB` (B-local, full-res RGBA8) reaches A's `bridge_img` before present.

**Options.**
- **(a) Shared NT handle, B→A, like the bridge.** Create `wapOutB` as a D3D11 shared
  texture on B's interop device + keyed mutex, import into A, blit into `bridge_img` on
  A.q. Mirrors the existing `bridge_tex`/`bridge_nt` machinery (`main.cpp:3793-3826`,
  verified). **Cost:** one cross-device surface + KM sync per present; still crosses the
  ~5 GB/s link but avoids a CPU copy.
- **(b) Host-bounce (the codebase's universal pattern).** `wapOutB` → host-coherent
  imported buffer (EMH) → A imports the same pointer → A blits into `bridge_img`. *(map
  reader 3: this is how EVERY A↔B move works today — no P2P.)* **Cost:** B→host write +
  A host→VRAM read, ~125-200 µs for 8.3 MB `[map, assumed]`, per present.
- **(c) CONCURRENT-mode image, B writes / A reads.** `VK_SHARING_MODE_CONCURRENT` on a
  shared allocation; skip explicit ownership transfer. **Risk:** CONCURRENT across two
  *physical* devices is not the same as across queue families on one device; on a no-P2P
  rig this still funnels through host memory and adds validation complexity for no gain.

**Weighing.** (b) is the proven, lowest-surprise path and matches every other cross-device
move in the app — the implementer already has the pattern to copy. (a) is marginally
faster (skips one CPU touch) but doubles the shared-texture machinery and the KM dance for
a *second* surface; given W4 dominates regardless, the marginal win does not justify the
complexity for an experiment expected to be negative. (c) is a trap on a no-P2P rig.

**Pick: (b) host-bounce**, reusing `hbuf_import` and the established
device→host→device idiom. Keep `bridge_img` strictly A-owned *(D5)*. Instrument the
transfer in `--csv` so the operator sees its per-tick cost *(D3, D6)*. Note in the build:
this transfer is the realization of wall W4 — it is *expected* to be the thing that makes
B-routing lose.

---

## S4 — The route decision (pillar-faithful, not a new hardcode)

**Decision:** how STAGE-103 decides A vs B per pair/tick.

**Options.**
- **(a) Live `break_even_decide` / `select_participants`** (`framework/gpu` +
  `framework/holons/.../RoutingPolicy.hpp` `[map]`), fed the measured catalog
  (`measure_transfer_bw` for B's link at startup) + the warp's AI. This *replaces* the
  STAGE-26b inlined `offload=false` constant (`main.cpp:50-52`, verified) with the real
  call. Expected output today: A (the measured negative).
- **(b) A fresh saturation heuristic** (e.g. route to B when measured 4090-util > N).
  Violates D2 (don't add a second parallel hardcode).
- **(c) `--warp-offload` as a pure force-flag** (no decision, always B when set).

**Weighing.** D2 mandates the pillar's arithmetic be the decision-maker. But the *point*
of the experiment is to SEE B run even though the arithmetic says no — so we need a force
path too. Reconcile: `--warp-offload` (bare) = **enable auto-routing** via (a) (logs A,
the honest negative); `--warp-offload force` (or `--warp-offload-force`) = **force B** for
the empirical test, logging that it overrides a measured-unfavorable decision.

**Pick: (a) + a force sub-flag.** Wire `break_even_decide` live (kill the STAGE-26b
constant, replacing the note with the real call), default to its verdict, and add an
explicit force token so the operator can drive B for the test. **Granularity:** decide
**per-pair** (cache `route_device` at the pair-advance site `wap_upload` `[map ~6985]`),
not per-tick — the inputs and the saturation state are pair-stable, and a per-tick
re-decision would thrash *(map reader 5; the tier-EMA precedent at STAGE-84)*. The cache is
an `std::atomic<int>` written by the sole pair-publisher, read by the present thread *(map
reader 5 race note)*.

---

## S5 — Integration with STAGE-102 `--async-present`

**Decision:** how a B-routed warp fits the 2-slot non-blocking present already built.

**Findings (verified this session + map reader 5).** STAGE-102 polls
`vkGetFenceStatus(A.dev, bslot[async_inflight].fence)` and presents `bslot[async_front]`.
A Vulkan fence is device-scoped: a B-routed warp signals **B's** fence, and the output
needs a B→A copy (S3) *before* the present can read `bridge_img`.

**Strategy.**
- When `route_device==B`: submit the warp to `B.q` with a B fence; the present-thread
  preamble polls **that B fence** (the poll call is identical — `vkGetFenceStatus` is
  device-agnostic, just pass `B.dev` + the B fence). On completion, run the **S3 B→A
  host-bounce + blit into `bridge_img`** (a short A.q step), then present as STAGE-102 does.
- The drop-interpolated invariant is unchanged: if the B fence isn't ready, re-present the
  front slot *(this composes cleanly with STAGE-102; the only addition is the B→A copy on
  completion)*.
- The `bslot` ping-pong stays A-side (both slots are A's `bridge_img`); B-routing only
  changes *where the synthesis ran* and adds the *output transfer*, not the present slots
  *(map reader 5)*.

**Caveat to measure (W3/W5):** a B fence may complete later than an A fence (cross-device
+ B contention) → drops may cluster. The `--csv` frz/warp_ms columns will show this; do not
mask it.

---

## S6 — Mass-conservation feedback (`devMass`)

**Decision:** the matte's mass counter is an A-side SSBO read after the warp fence
(`main.cpp` STAGE-59/76, verified). With the warp on B it would be written on B.

**Options:** (a) keep `devMass` on A and gate the shader's `atomicAdd` to the A route
only; (b) duplicate (`devMass_a` + `devMass_b`) and read the route's buffer; (c) share.

**Pick: (b) a B-local `devMass_B`** read back via B's copy (the warp already does the
on-GPU reset + copy-back pattern; replicate it on B). When `route_device==B`, the present
preamble reads the B-side host-mass after the B fence completes (same place STAGE-102 moved
the mass read to). Simplest correct; avoids cross-device SSBO sync *(map reader 1/5 risk)*.

---

## S7 — Byte-identical-off discipline (D1, the non-negotiable)

Mirror the STAGE-102 pattern that was verified byte-identical this session:
- All B resources (`wapPipeB`, the B images, `devMass_B`, the B fence/cmd) are allocated
  **only inside `if(cfg.warp_offload){...}`**; declared null/`VK_NULL_HANDLE` at file scope
  so teardown guards are inert when off.
- A `route_device` that is **always 0 (A)** when `cfg.warp_offload` is false → the warp
  records/submits/presents on exactly today's A path.
- Use the shadow-alias trick (as STAGE-102 did with `cmdBridge`/`fBridge`/`bridge_img`):
  the route picks the device-specific cmd/queue/fence/output via locals that **alias the A
  resources when off**, so the large record body stays textually unchanged and any missed
  reference is still correct on the off path.
- Teardown mirrors slot-1's guarded cleanup.
**Gate:** the supervisor re-reads the off path + builds green + reasons byte-identical
(the §2.3 bar — delegated work verified harder).

---

## S8 — Measurement and validation

- **`--csv` (STAGE-100) already carries** warp_ms, per-GPU util (`_4090`/`_1080Ti` via
  NVML, STAGE-99), frz, AddedLatency. Add a truthful `route_device` per row *(D6)*.
- **Startup:** call `measure_transfer_bw(B.stream)` to fill B's real link GB/s and log it
  (replaces the `[map]` ~5 GB/s estimate with a measured number) *(D3)*.
- **The operator's gate (P5):** BF6 combat, `--warp-offload force` one-by-one then combined
  with `--async-present` + `--load-governor`. Compare warp_ms(A) vs warp_ms(B) + the B→A
  transfer + the latency/multiplier deltas. The expected result is "B loses"; the value is
  the *measured* confirmation.

---

## S9 — The honest lighter alternative (if the full build is not worth it)

If the operator later decides a known-negative is not worth the full per-tick B-route:
a **measurement-only flag** times a *one-shot* B warp (a single dispatch on `wapPipeB`
over one captured pair) + the measured B→A host-bounce, prints
`warp_A=… ms, warp_B=… ms, xfer_B→A=… ms, break_even=offload:false`, and never wires the
per-tick route. This confirms the §0 verdict with one B dispatch instead of a steady-state
240 Hz B pipeline. *(Recorded per the no-silent-caps discipline; the operator has chosen
the full build, so this is a fallback, not the plan.)*

---

## Recommended build order (composing the picks)

1. **P1** — `cfg.warp_offload` flag in `parse_extra` (NOT the main chain, C1061) +
   `wapPipeB` via `wap_create(B,…)` gated on the flag, graceful fallback (S1, S7). Build
   green; off → byte-identical (verify).
2. **P2** — B-side images for the A-only masks + `wapOutB` + `devMass_B`; replicate the
   placeholder/completeness chain (S2, S6). One-shot B warp produces a sane frame.
3. **P3** — the S3 host-bounce B→A + blit into `bridge_img`, integrated into the STAGE-102
   present preamble (S3, S5). Forced-B run presents correct frames (operator eye).
4. **P4** — wire live `break_even_decide` (kill the STAGE-26b constant) + the per-pair
   `route_device` cache + the `force` sub-flag (S4). Auto logs A; force drives B.
5. **P5** — `route_device` in `--csv` + `measure_transfer_bw` at startup (S8); hand to the
   operator's combat test.

Each phase is build-green + byte-identical-off gated; the implementer reports per §7 of
`AGENT_PROTOCOL.md` (honest: "compiles" ≠ "validated"); the supervisor verifies first-hand
(§2.3). Runtime validation is the operator's — not claimable by the build.
