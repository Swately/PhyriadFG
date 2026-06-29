# MINIMAL_FG_SOTA_DOSSIER — prior art for the "hello-world" Frame Generator

> **Diátaxis type:** Reference (prior-art / SOTA dossier). Descriptive, **non-normative**:
> it states no requirements, so BCP 14 keywords are absent by design (FDP §3, D8). The
> requirements that consume this dossier live in its three siblings.
> **Status:** dated snapshot **2026-06-26**, anchored to `0.1.0-experimental`
> ([`version.hpp:35`](../../framework/_meta/include/phyriad/version.hpp)). Every third-party
> fact is in the `measured`/`documented` bucket — it is something an external party documented,
> stated, or that was reverse-engineered, **not** something Phyriad has measured on its own rig.
> **Role:** the verifiable-reference foundation cited by
> [`MINIMAL_FG_MASTER_PLAN.md`](MINIMAL_FG_MASTER_PLAN.md) and its strategies/risk siblings. The
> master plan's component IDs **MC-1 capture · MC-2 flow · MC-3 warp · MC-4 blend · MC-5 present ·
> SG seam** ([master plan §0.1](MINIMAL_FG_MASTER_PLAN.md)) are used here verbatim.

## §0 — Scope, sourcing, and the provenance contract

**Subject.** The documented and reverse-engineered state of the art for *external-capture frame
generation* (interpolation between two captured game frames), and the GPU-pipeline ("seam")
discipline that wires it. **Boundary:** this is prior art only — it makes no claim about
PhyriadFG's own behavior; those claims live in the plan/strategies/risk register.

**Single source of truth for citations.** This dossier is lifted from one corpus: the 2026-06-26
five-agent SOTA barrido (`scratchpad/minimal_fg_sota_notes.md`), conclusion-first and adversarially
verified by the research agents. **No URL, number, API, or quotation appears here that is not in
that corpus.** Where the master plan's prose is richer than the corpus, this dossier defers to the
corpus string, not the plan's paraphrase.

**Verification honesty (FDP §2.2–§2.4).** The corpus's references were fetched **first-hand by the
research agents**, not re-fetched in this authoring pass. From this document's standpoint they are
therefore **secondary** until the supervisor re-verifies them; the citation register (§8) carries
that disclaimer per-reference rather than implying a first-hand `[V1]` this pass did not earn.

**Provenance tags** (applied to every load-bearing claim; preserved from the corpus's own taxonomy):

| Tag | Meaning |
|---|---|
| **[DOCUMENTED url]** | third-party public documentation, source code, or peer paper; URL/arXiv id in §8; bucket `measured`. |
| **[STATED-BY-DEV]** | the Lossless Scaling developer ("THS") asserted it in a Steam discussion — a claim about design *intent*, not an independent measurement. |
| **[REVERSE-ENGINEERED]** | derived from the `lsfg-vk` reimplementation — an *observational model* of LSFG, not its source. |
| **[INFERRED]** | arithmetic or logical derivation from a documented fact (flagged, not asserted as primary). |
| **[UNKNOWN]** | named in the corpus but not sourced there, or outside the corpus — listed in §7 for the supervisor. |

**Two standing correction flags (carried from the corpus, do not drop):**
1. **Notebookcheck INVERTS the LSFG Flow-Scale direction** — treat any Notebookcheck statement of
   which way Flow Scale moves resolution as wrong-signed; the dev-sourced direction (§3) governs.
2. **App version ≠ LSFG-subsystem version** — the Steam app's version number is not the
   interpolation subsystem's; the corpus records **app 3.2 = LSFG 3.1**. Do not equate them.

---

## §1 — The convergent verdict

**All four independent lenses converge on the same minimal shape, and all four confirm the
operator's seam diagnosis.** The lenses: FSR3 (open / documented), LSFG (dev-stated + reverse-
engineered), academic VFI (RIFE / softmax-splatting / AceVFI survey), and GPU-pipeline architecture
(Frostbite FrameGraph / Granite / Khronos). [DOCUMENTED — see §2–§5]

### 1.1 The irreducible motion-compensated core (3 components, identical across FSR3 / RIFE / LSFG)

The interpolation kernel reduces to exactly three components, mapped onto the master plan's IDs:

| Component (corpus) | Plan ID | What it is | Why it is irreducible |
|---|---|---|---|
| Bidirectional optical flow F(A→t), F(B→t) | **MC-2** | the motion source | external capture has **no engine MVs** → estimated flow is the *only* motion source [DOCUMENTED] |
| **Backward** warp A,B → t (bilinear) | **MC-3** | resample each frame toward the midpoint | dense output, **no holes**; what RIFE uses; AceVFI-favored [DOCUMENTED arXiv:2011.06294 / arXiv:2506.01061] |
| Soft occlusion-mask blend I_t = M·Â + (1−M)·B̂ | **MC-4** | composite the two warps | a non-trivial M is the **minimal disocclusion handler** [DOCUMENTED] |

Wrapped by the two non-interpolation stages: **MC-1 capture** (GPU-resident, zero host copy) before,
and **MC-5 paced present with DROP-interpolated** after. The full minimal pipeline is
`capture → flow → warp×2 → blend → paced-present(drop-late)`. [DOCUMENTED — corpus "irreducible core"]

### 1.2 Why backward-warp beats forward / splatting

**Backward warp is the minimal *solid* choice, not merely the simplest.** [DOCUMENTED arXiv:2003.05534
/ arXiv:2506.01061] Backward warp samples the source at flow-displaced coordinates → every output
pixel is filled (dense, no holes). **Forward warp / softmax-splatting scatters source pixels
forward → leaves holes that require a synthesis network to fill** — i.e. forward splatting is *more*
complex, not simpler, and is therefore disqualified as the minimal base. RIFE itself uses backward
warp with a learned mask; the AceVFI survey favors backward warp for the same reason.

### 1.3 Why blend-only is the toy

**Pure blend / crossfade is a temporal average and ghosts on every moving edge.** [DOCUMENTED —
AceVFI arXiv:2506.01061] It is the TOY: admissible only as a fail-safe fallback, never as the base.
The minimal *solid* core (flow + backward-warp + soft mask) degrades to a **soft artifact at true
disocclusion, never a hole** — that graceful degradation is exactly what separates "solid" from
"toy".

### 1.4 The seam, not the passes, is the operator's bottleneck

**The corpus's headline finding, with measured numbers:** the cost of "many layers" is the
**inter-stage seam** — fence-per-stage + host round-trips + over-broad barriers — **not** the
per-pass GPU timings. [DOCUMENTED — §5] This confirms the operator's hypothesis directly and is the
reason the arc's deliverable is **SG (the render-graph seam)**, not a new shader.

---

## §2 — FSR3 (AMD FidelityFX, open / documented)

FSR3 is the only major FG implementation with a **public, documented** interpolation pipeline, so it
anchors the `measured` facts. [DOCUMENTED gpuopen frame-interpolation]

### 2.1 The 9-pass pipeline and the 4-pass minimal subset

**The documented interpolation pipeline is 9 passes; the corpus identifies a 4-pass minimal subset.**
[DOCUMENTED gpuopen frame-interpolation]

| | Passes |
|---|---|
| Full documented pipeline | **9 passes** (count documented; the full per-pass enumeration is **not transcribed in the corpus** — see §7) |
| **Minimal subset (corpus)** | **4 passes: (1) setup, (2) optical-flow vector field, (3) disocclusion mask, (4) interpolation** |

The 4-pass subset is the empirical confirmation, from a shipping product, of the §1.1 three-component
core plus a setup pass.

### 2.2 The no-MV optical-flow path

**FSR3 computes its own optical flow rather than relying on engine motion vectors** — the same
constraint external capture lives under. Documented parameters: [DOCUMENTED gpuopen optical-flow]

| Quantity | Value |
|---|---|
| Flow field resolution | `displaySize / 8` |
| Block size | 8 × 8 |
| Flow vector format | `R16G16_SINT` |
| Flow VRAM footprint @ 4K | ~26–28 MB |

### 2.3 Disocclusion mechanism

**FSR3 carries an explicit disocclusion-mask pass** (pass 3 of the minimal subset). [DOCUMENTED
gpuopen frame-interpolation] This is the same role MC-4's soft mask plays: it is the minimal
mechanism for the regions a single backward-warp cannot resolve. The corpus does not transcribe the
internal mask formula — see §7.

### 2.4 Present / pacing model

**FSR3's cadence algorithm is open and is the verifiable pacing reference:** `moving-average frametime
→ target present time → wait-until-target`. [DOCUMENTED gpuopen frame-pacing] **The structural defect
of FSR3's model is that its present BLOCKS** — under GPU saturation the blocking present stalls the
game thread. [DOCUMENTED] The corpus's lesson is to keep FSR3's *cadence math* but reject its
*blocking* in favor of the DLSS-G drop model (§6.3).

### 2.5 Documented-numbers table (consolidated)

| Quantity | Value | Provenance |
|---|---|---|
| Interpolation passes | 9 | [DOCUMENTED gpuopen frame-interpolation] |
| Minimal subset | 4 (setup / OF-field / disoccl-mask / interp) | [DOCUMENTED] |
| Flow resolution | displaySize/8 | [DOCUMENTED gpuopen optical-flow] |
| Flow block | 8×8 | [DOCUMENTED] |
| Flow format | R16G16_SINT | [DOCUMENTED] |
| Flow VRAM @4K | ~26–28 MB | [DOCUMENTED] |
| Cadence algorithm | moving-avg → target time → wait | [DOCUMENTED gpuopen frame-pacing] |

---

## §3 — LSFG (Lossless Scaling Frame Generation)

LSFG is closed-source; the corpus characterizes it from two honest provenances kept distinct: the
developer's **stated** design intent (Steam) and a **reverse-engineered** model (`lsfg-vk`).

### 3.1 Dev-stated design philosophy (THS, Steam)

**The defining fact about LSFG is that its developer deliberately traded interpolation quality for a
cheap pass.** [STATED-BY-DEV] The corpus's verbatim developer strings:

- *"main disadvantage is execution time"* — the design accepts quality loss to keep the pass light.
- *"all guesswork, always inaccurate"* / *"it's all guesswork"* — the developer's own framing of the
  no-MV disocclusion limit (§4.4, §6.4).

These are claims about **intent**, not independent measurements; they are not promoted to `measured`.

### 3.2 Reverse-engineered pipeline (`lsfg-vk`, PancakeTAS)

**The observed LSFG kernel is the RIFE family:** coarse-to-fine flow **pyramid** → bidirectional warp
→ composite with an A/B fallback. [REVERSE-ENGINEERED github.com/PancakeTAS/lsfg-vk] This is the §1.1
core with a multi-scale flow front end; it does **not** contradict the irreducible-minimum finding.

### 3.3 The Flow-Scale knob

**"Flow Scale" controls flow *resolution* — it is LSFG's only cost knob, and the output is always
native resolution (no upscale).** [STATED-BY-DEV Steam thread 510701184375375597] **Correction flag:
Notebookcheck inverts the direction of this knob** — use the dev-sourced direction, not
Notebookcheck's. Lower flow resolution = cheaper pass, coarser motion; native output regardless.

### 3.4 Cap-the-game

**LSFG does not defy GPU saturation; it caps the game to leave headroom for its light pass.**
[STATED-BY-DEV — corpus "lsfg-3" cap-the-game] It wins by *making room*, not by being preemptible.

### 3.5 Why it "feels smooth"

**The perceived smoothness is display-level even PACING plus a light pass — NOT interpolation
quality.** [STATED-BY-DEV / REVERSE-ENGINEERED] The two pacing mechanisms named in the corpus:
**EMA jitter smoothing** and a **Queue Target**. The takeaway the master plan acts on: *weight goes to
pacing + pass-lightness, not stacked quality passes.*

### 3.6 The explicit NULL list (what LSFG does NOT do)

| LSFG does not… | Source | Phyriad note |
|---|---|---|
| protect its threads / use thread-priority | [REVERSE-ENGINEERED] | Phyriad's `--fg-protect` **exceeds** LSFG here (orthogonal, already closed — plan §7) |
| defy saturation (it caps the game instead) | [STATED-BY-DEV] | §3.4 |
| claim accuracy at disocclusion ("all guesswork") | [STATED-BY-DEV] | the universal floor, §4.4 |
| upscale when Flow Scale < native | [STATED-BY-DEV] | output is always native, §3.3 |

---

## §4 — Academic VFI (video frame interpolation)

The academic literature supplies the **simple → correct hierarchy** and the irreducible-minimum
confirmation.

### 4.1 The simple → correct hierarchy

| Tier | Method | Verdict |
|---|---|---|
| simplest | **blend / crossfade** | TOY — temporal average, ghosts every moving edge [DOCUMENTED AceVFI arXiv:2506.01061] |
| minimal solid | **backward-warp + soft mask** | the irreducible base; dense, no holes [DOCUMENTED arXiv:2011.06294] |
| more complex | **forward warp / softmax-splatting** | scatters → holes → needs a synthesis net (more complex) [DOCUMENTED arXiv:2003.05534] |
| full | **bidirectional flow + learned mask** | RIFE/IFRNet; the shipping research form [DOCUMENTED arXiv:2011.06294 / arXiv:2205.14620] |

### 4.2 The cited works

| Work | arXiv | Corpus-recorded facts |
|---|---|---|
| **RIFE** | **arXiv:2011.06294** | backward warp + learned mask; **9.8 M** params; **16 ms @ 480p on TITAN X** |
| **Softmax splatting** | **arXiv:2003.05534** | the forward-warp / splatting reference (needs synthesis) |
| **IFRNet** | **arXiv:2205.14620** | bidirectional intermediate-flow network |
| **AceVFI (survey)** | **arXiv:2506.01061** | the survey grounding "backward-warp favored", "blend = toy" |

### 4.3 Disocclusion / hole analysis

**Backward warp produces no holes** (every output pixel samples a source location); **forward warp
produces holes** at disocclusions and around fast edges, which is precisely why splatting methods
bolt a synthesis network on top. [DOCUMENTED arXiv:2003.05534] The soft mask (MC-4) is the minimal
substitute for that network: it cannot *recover* truly-hidden content, but it converts a hard hole
into a soft, bounded artifact.

### 4.4 The irreducible-minimum confirmation (and the hard floor)

**The academic lens independently lands on the same three-component minimum** as FSR3 and LSFG —
that triple-convergence is the dossier's strongest result. **It also marks the hard floor:** true
disocclusion (a region hidden in *both* A and B) is **information-theoretically unrecoverable without
hallucination**, and with no depth buffer there is no correct occlusion ordering. [DOCUMENTED — corpus
"HARD FLOOR"] The operator's observed *gravity / pulse / stick* is the linear-flow approximation
breaking at occlusion boundaries — the universal no-MV frontier, shared by FSR3 and LSFG, **not** a
Phyriad bug.

---

## §5 — The seam discipline (SG)

**This is the operator's bottleneck, SOTA-confirmed with measured numbers.** The "many layers =
overhead" failure mode has three measured causes, none of them the per-pass GPU cost. [DOCUMENTED]

### 5.1 The measured sync costs

| Cause | Measured cost | Provenance |
|---|---|---|
| **Fence per stage** | each `vkWaitForFences` blocks the host; each `vkQueueSubmit` ~**24–36 µs** CPU | [DOCUMENTED — corpus "MEASURED"; specific URL **not in corpus**, §7] |
| **Host round-trips** (GPU→CPU→GPU) | flush the full GPU **L2 → RAM over PCIe** — the latency killer | [DOCUMENTED — corpus; no per-number URL] |
| **Over-broad barriers** (`ALL_COMMANDS`) | **13 % frame-time wasted** vs precise stage masks | [DOCUMENTED — Khronos barrier sample, MEASURED] |

The canonical anti-pattern in the existing tree is `vkWaitForFences(fBridge, UINT64_MAX)` (per the
corpus and [master plan §3](MINIMAL_FG_MASTER_PLAN.md)).

### 5.2 The cure — render-graph patterns

**The cure is a render-graph: passes DECLARE the resources they read/write, and a CPU-side compiler
derives every barrier.** [DOCUMENTED] Patterns the corpus names:

- **Frostbite FrameGraph** — O'Donnell, GDC 2017 [DOCUMENTED — GDC Vault; full URL not in corpus, §7].
- **Granite / Themaister render-graph** (2017) [DOCUMENTED — full URL not in corpus, §7].
- Also named without a URL: **Loggini**, **Our Machinery** [UNKNOWN url — §7].

### 5.3 Async-compute wins

**Flow + warp on the compute queue overlap present on the graphics queue, joined by one timeline
semaphore → present is DECOUPLED from generation.** [DOCUMENTED] Measured wins: **5–29 %** (corpus:
"Vulkan sample / Interplay-of-Light 2025"; precise per-number URL **not in corpus**, §7).

### 5.4 The layerability rules (what makes the core re-layerable without re-introducing overhead)

[DOCUMENTED — render-graph patterns]

- **Add a layer = 0 hand-written barriers, 0 new fences** (compiler re-derives; at most **+1
  `VkEvent`**).
- **A disabled layer is CULLED from the graph** — 0 execution, 0 allocation, 0 barrier — **not** an
  `if` inside a shader, **not** a registered-but-skipped pass.
- **1 fence per frame-in-flight**, never per stage; inter-stage sync = `VkEvent` (intra-queue) /
  timeline semaphore (cross-queue).
- **Data stays GPU-resident** between stages — no host round-trip, ever.
- **Precise stage masks** (signal-early / wait-late), barriers batched at the frame boundary.

---

## §6 — External-capture irreducibles (Windows)

The structural constraints an *external* FG tool cannot escape.

### 6.1 Capture method

**WGC (Windows Graphics Capture) is the documented external path:** cross-GPU, stable, with a **≥ 1
VBLANK floor** (~4.2 ms @ 240 Hz). [DOCUMENTED — corpus; the 4.2 ms figure is [INFERRED] = 1/240 Hz].
**DComp `CreateTargetForHwnd` cross-process returns `ACCESS_DENIED`** — composing into the game's own
window is OS-blocked. [DOCUMENTED — corpus; no URL]. The DDA + injection-rate gap the original corpus
left open (U7) is now **closed by the 2026-06-26 capture-rate barrido** (§6.1.1).

### 6.1.1 Capture RATE — the DWM-compositor ceiling (2026-06-26 barrido, citation-bound)

The 2026-06-26 capture-rate SOTA (6-angle fan-out, 3-vote adversarial verify; **23/26 claims confirmed,
0 refuted**) closes U7 and **corrects a tempting assumption**: switching WGC→DDA does **not by itself**
lift a capture-rate cap. The verified mechanism:

- **WGC and DDA share ONE architectural limiter: both capture from the DWM compositor** — they only ever
  see frames that pass through composition. WGC `CreateForWindow`, WGC `CreateForMonitor`, and DDA all
  inherit the same ceiling; the choice between them is **not** a rate lever. [REVERSE-ENG —
  `github.com/fernandoenzo/ForceComposedFlip`; DOCUMENTED — `learn.microsoft.com/.../wgc-capturing-exclusive-fullscreen-games-apps`].
- **Flip-model fullscreen/borderless (Independent / Direct Flip) BYPASSES DWM** — frames go straight to
  scanout "in the same way that exclusive fullscreen does"; once Independent-Flipped the DWM can sleep.
  A modern fullscreen game is the canonical case. Those frames are **invisible to WGC AND DDA at full
  rate** — both see only the ~compose-rate subset. [DOCUMENTED —
  `learn.microsoft.com/.../for-best-performance--use-dxgi-flip-model`].
- **No non-injecting method can exceed the monitor refresh.** MS engineer robmikh: WGC "shouldn't
  outpace the monitor"; the only rate knob (`MinUpdateInterval`, build 26100) is a CAP that lowers,
  never raises, delivery. [DEV-STATED — `api.github.com/repos/robmikh/Win32CaptureSample/issues/79/comments`].
  DDA's `AcquireNextFrame` is event-driven (fires only on a desktop-image update), bounded by the
  compose/refresh cadence. [DOCUMENTED — `learn.microsoft.com/.../idxgioutputduplication-acquirenextframe`].
- **The pre-24H2 WGC framerate-capture bug** capped WGC ingest below the source on builds < 26100; fixed
  on 24H2 (26100). **RULED OUT for this rig**: the operator's box is **Windows 11 25H2, build 26200.8457**
  (verified first-hand via the registry this pass) → past the bug. So the rig's ~50-62 cap is NOT that bug.
- **LSFG-Windows is an EXTERNAL capturer with two backends — DXGI Desktop Duplication (default) and WGC —
  NOT a present hook.** Its DXGI capture is bound to the game's flip cadence (doubles the game's rate).
  [DEV-STATED — `downloadsource` settings guide; Steam discussion]. The Linux reimplementation **lsfg-vk
  IS a present hook** (a Vulkan implicit layer over `vkQueuePresentKHR` — injection), which is **not** what
  the Windows app does. [REVERSE-ENG — `github.com/PancakeTAS/lsfg-vk` wiki].
  → **Decisive reconciliation:** the operator observes **LSFG consuming all fps** on this rig. Since
  LSFG-Windows does so via **DXGI/DDA** (the same DWM-bound method available to us), that is **first-hand
  evidence DDA reaches the full rate on THIS hardware/title** — i.e. BF6 is effectively composed for the
  capturer here (or LSFG forces it), so the WGC-**window** ~60 is a WGC-window shortfall that DDA would
  close. To be **confirmed by measurement**, not assumed.

**The measurement gate (efficiency mandate — measure before the rewrite):**
1. `winver`/build — **DONE: 25H2 / 26200.8457** → pre-24H2 bug ruled out.
2. **PresentMon on BF6** → the `PresentMode` column: "Hardware: Independent Flip" vs "Composed/MPO" —
   the direct test of whether the bypass is active.
3. **LSFG backend confirm** — that LSFG is on its **DXGI** backend when it gets the full rate (the cheap
   proof DDA works here). *Equivalent in-house test:* run the existing `render_assistant` DDA path (or a
   DDA backend added to `minimal_fg`) on BF6 and read its ingest instrument — one operator run measures + validates.

**The decision tree the gate resolves:**
- Composed/MPO (or LSFG-DXGI / our DDA proven) → **build the owned DDA capture** (game ≈100 fps < 240 Hz
  refresh → DDA delivers every game frame). The convergence-correct move.
- Independent Flip **and** DDA also caps → the only non-injecting lever is **forcing Composed Flip**
  (disable MPO via `OverlayTestMode`=5 + a 1×1 click-through topmost overlay reasserted ~500 ms), at
  **+1 frame latency** — opt-in, default-off (it taxes MAX-PERFORMANCE). [REVERSE-ENG — ForceComposedFlip].
- Present-hook injection (the only >refresh method) stays **REJECTED for BF6**: BF6 ships EA Javelin
  (kernel anti-cheat; Secure Boot + TPM 2.0) → injection risks a ban + breaks the external/non-injecting
  property. [DEV-STATED/REVERSE-ENG — OBS Game-Capture, Special K, RTSS].

### 6.1.2 Max-performance ingest (the path, when we build the owned DDA layer)

GPU-resident, lock-free, **zero-copy** — reconciled with the keyed-mutex crash fix (the BF6 deadlock):
- **Acquire** keeps the `IDXGIResource` GPU-resident (no host readback); use `DuplicateOutput1` in the
  game's native format (avoid a forced BGRA convert).
- **Share** via `IDXGIResource1::CreateSharedHandle` (NT handle → `VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT`,
  the variant that OWNS a reference — never `_KMT_BIT`, which dangles). Imported ONCE → aliases the same
  physical allocation → **no per-frame copy**. [DOCUMENTED — Khronos external-memory; NVIDIA interop blog].
- **Synchronize with a shared TIMELINE semaphore / fence** (`VK_KHR_external_semaphore` ↔ D3D fence),
  **NOT a keyed mutex** — wait/signal back-pressures instead of deadlocking a GPU-starved producer
  (the keyed-mutex `AcquireSync(INFINITE)` was exactly our crash class). [DOCUMENTED — MS surface-sharing;
  IDXGIKeyedMutex]. A ring of N pre-imported shared surfaces keeps SPSC lock-free.
- **Multi-GPU:** create the DDA device on the adapter that **owns** the display (the 4090); cross to the
  1080 Ti/iGPU via the NT-handle shared surface (GPU-driven), never a CPU readback round-trip.
- Per-frame cost collapses to **one fence wait** (no PCIe transfer). The host-staged path (GPU→CPU→GPU,
  ~0.6–1.5 ms @1080p) is a **degraded fallback only** — proven + crash-safe, but the latency the mandate
  forbids as a default.

### 6.2 Present surface

**The viable present model is our own window: a separate topmost HWND + own swapchain (the LSFG
model); Phyriad's in-process DComp window satisfies this.** [DOCUMENTED — corpus] The cross-process
DComp-into-the-game path is the blocked one (§6.1).

### 6.3 DROP vs BLOCK

**DROP the interpolated frame, never block.** [DOCUMENTED] DLSS-G's present is non-blocking and the
**interpolated frame CAN be dropped** when late (the real frame is always shown) [DOCUMENTED —
NVIDIA-RTX/Streamline `ProgrammingGuideDLSS_G.md`]; FSR3's blocking present stalls the game thread
under saturation (§2.4). An external tool MUST adopt the drop model (this is the plan's requirement,
stated normatively there, not here).

### 6.4 The no-MV structural penalty

**No engine MVs → estimated flow is the only motion source → no depth → no correct occlusion order;
the UI always warps.** [DOCUMENTED] The developer summary is the THS line — *"it's all guesswork"*
[STATED-BY-DEV]. This is a structural ceiling on external capture, not a defect to be engineered away.

---

## §7 — Honest hard-truths + the consolidated UNKNOWN list

### 7.1 Hard-truths (uninflated)

- **The disocclusion floor is universal and ours too** — information-theoretically irreducible
  without hallucination; FSR3 and LSFG share it (§4.4, §6.4). The minimal arc removes the **seam**
  component of the artifact and paces better; it does **not** claim to beat the floor.
- **Minimal is not automatically better** — this dossier establishes the *shape*, not a verdict that
  fewer passes win on Phyriad's rig. That is the plan's `minimal-NET > complex-NET` measurement, not a
  claim of this dossier.
- **LSFG's edge is minimalism + pacing, not interpolation quality** — established dev-stated +
  reverse-engineered, **not** independently measured by Phyriad.

### 7.2 Consolidated UNKNOWN list (what the corpus does NOT source — where to look)

| # | Gap | Where to look |
|---|---|---|
| U1 | **Full 9-pass FSR3 enumeration** — corpus names only the 4-pass minimal subset | gpuopen frame-interpolation manual (§8) — read the per-pass list first-hand |
| U2 | **`vkQueueSubmit` ~24–36 µs** and the host-round-trip L2-flush — stated MEASURED, **no per-number URL** in corpus | a primary Vulkan submit-overhead benchmark; treat as `unmeasured-by-us` until a source is attached |
| U3 | **Async-compute 5–29 %** — "Vulkan sample / Interplay-of-Light 2025" named, no precise URL | Interplay-of-Light 2025 article + the Khronos/Vulkan async-compute sample |
| U4 | **Khronos 13 % broad-barrier sample** — measured, no resolvable URL in corpus | the Khronos `Vulkan-Samples` barrier/pipeline-barrier sample |
| U5 | **Frostbite FrameGraph (O'Donnell 2017), Themaister, Granite, Loggini, Our Machinery** — named, **no resolvable URLs** | GDC Vault (O'Donnell 2017); themaister.net; the others by title |
| U6 | **DComp cross-process `ACCESS_DENIED`; WGC VBLANK floor** — stated, no URL; the 4.2 ms is [INFERRED] arithmetic | Microsoft DirectComposition / WGC docs; measure the VBLANK floor first-hand |
| U7 | **DDA + injection/hook capture rate** — ~~outside the corpus~~ **CLOSED by the 2026-06-26 capture-rate barrido (§6.1.1)**: DDA shares WGC's DWM-compositor ceiling (WGC→DDA is not a rate lever); the rate cap is the Independent-Flip DWM-bypass; present-hook = injection (AC-unsafe for BF6) | §6.1.1 (cited) — RESOLVED |
| U8 | **FSR3 disocclusion-mask internal formula; LSFG EMA/Queue-Target constants** — mechanisms named, internals not transcribed | gpuopen frame-interpolation source; lsfg-vk source |
| U9 | **`frame-pacing.md` (azagramac mirror)** — file named, full mirror URL not in corpus | the gpuopen FidelityFX SDK repo / the azagramac mirror |

---

## §8 — Citation register

**Provenance disclaimer (applies to every row):** these were fetched **first-hand by the 2026-06-26
research agents** and recorded in `scratchpad/minimal_fg_sota_notes.md`; **this authoring pass did
not re-fetch them.** From this document's standpoint they are therefore **secondary [V3-this-pass]**
pending the supervisor's first-hand re-verification (FDP §2.3, §2.4). URLs are reproduced exactly as
the corpus records them; rows marked "partial" had only a partial pointer in the corpus.

| Ref | What | Corpus provenance | URL / id (as in corpus) |
|---|---|---|---|
| FSR3 frame-interpolation | 9-pass pipeline, disoccl mask | [DOCUMENTED] | https://gpuopen.com/manuals/fidelityfx_sdk/techniques/frame-interpolation/ |
| FSR3 optical-flow | flow params (displaySize/8, 8×8, R16G16_SINT, 26–28 MB) | [DOCUMENTED] | https://gpuopen.com/manuals/fidelityfx_sdk/techniques/optical-flow/ |
| FSR3 frame-pacing | cadence algorithm | [DOCUMENTED] (partial — `frame-pacing.md`, azagramac mirror; full URL not in corpus) | gpuopen FidelityFX SDK · `frame-pacing.md` (azagramac mirror) |
| DLSS-G drop | interpolated frame CAN be dropped; non-blocking present | [DOCUMENTED] | github.com/NVIDIA-RTX/Streamline · `ProgrammingGuideDLSS_G.md` |
| RIFE | backward warp + learned mask; 9.8 M; 16 ms @ 480p TITAN X | [DOCUMENTED] | arXiv:2011.06294 |
| Softmax splatting | forward-warp/splatting reference | [DOCUMENTED] | arXiv:2003.05534 |
| IFRNet | bidirectional intermediate-flow net | [DOCUMENTED] | arXiv:2205.14620 |
| AceVFI survey | backward-warp favored; blend = toy | [DOCUMENTED] | arXiv:2506.01061 |
| Frostbite FrameGraph | render-graph pattern | [DOCUMENTED] (partial — title only) | GDC Vault · O'Donnell 2017 |
| Themaister render-graph | render-graph pattern (2017) | [DOCUMENTED] (partial — title only) | Themaister render-graph 2017 |
| Khronos barrier sample | 13 % broad-barrier waste | [DOCUMENTED] (partial — no URL) | Khronos barrier sample |
| Loggini · Our Machinery | render-graph patterns | [UNKNOWN url] | (named, no URL) |
| async-compute wins | 5–29 % | [DOCUMENTED] (partial — no URL) | "Vulkan sample / Interplay-of-Light 2025" |
| LSFG design (THS) | interp design; "guesswork"; "execution time" | [STATED-BY-DEV] | Steam app/993090 discussions |
| LSFG Flow-Scale | flow resolution, no upscale | [STATED-BY-DEV] | Steam thread 510701184375375597 |
| LSFG cap-the-game | caps the game for headroom | [STATED-BY-DEV] | Steam "lsfg-3" discussion |
| LSFG reverse-engineering | RIFE-family pipeline; NULL list | [REVERSE-ENGINEERED] | github.com/PancakeTAS/lsfg-vk |

**Correction flags (repeat, do not drop):** (1) **Notebookcheck inverts the Flow-Scale direction.**
(2) **App version ≠ LSFG-subsystem version** (app 3.2 = LSFG 3.1).

---

## §9 — Links

- Consumes nothing; consumed by [`MINIMAL_FG_MASTER_PLAN.md`](MINIMAL_FG_MASTER_PLAN.md) and its
  siblings [`MINIMAL_FG_IMPLEMENTATION_STRATEGIES.md`](MINIMAL_FG_IMPLEMENTATION_STRATEGIES.md) ·
  [`MINIMAL_FG_RISK_REGISTER.md`](MINIMAL_FG_RISK_REGISTER.md).
- Process spec obeyed: [`../canon/FORMAL_DOCUMENT_PROTOCOL.md`](../canon/FORMAL_DOCUMENT_PROTOCOL.md).
- Registration in [`../FORMAL_DOCUMENTS_REGISTER.md`](../FORMAL_DOCUMENTS_REGISTER.md) is handled with
  the triad at the P0 close (do not edit it from this file).
