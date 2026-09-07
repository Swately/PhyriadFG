# Verdict — the prompt-driven filter, after four attacks

**Run** `wf_8eb3f6ee-9dd`, 2026-09-07. Four agents were told to break a proposed teacher/student
architecture; a fifth judged. The proposal was this session's own, and it did not survive intact.

> **Read this before the verdict.** The brief those attackers were given contained THREE factual
> errors about this repository, all of them mine, all of them found and corrected by the attack. They
> are tabulated in the verdict's section 3.1 and repeated here because they were told to the operator
> as fact:
>
> 1. Bindings 11 and 13 are NOT free data. Binding 11 is a **1x1 R32_UINT placeholder**
>    (`wapFIELDph`) unless `--igpu-field` is passed, and binding 13 is **the CURRENT real frame**,
>    not the previous output, because `ts_smooth` defaults to `0.0f` and
>    `prevout_view = wapPrevOutA.view ? wapPrevOutA.view : wCur.view`. A layout slot is not a data source.
> 2. The object labels and the chamfer distance-to-rim field **never reach the GPU**. `obj_label`,
>    `obj_chamf` and `obj_feat` are host `std::vector` in `HolonScratch`, computed and consumed inside
>    `object_repair` to rewrite motion vectors, then discarded. The single best cel-shading primitive
>    in the inventory is not available to a shader at all.
> 3. The Sobel field is zero-PCIe on its **production** side only. Consuming it costs a full-res
>    `vkCmdCopyBufferToImage` per pair — **8.29 MB per real frame at 1080p**, half of it padding.

> What IS free, and is worth more than the slots: the **pass shape**. `--afill` is already an in-place
> per-pixel compute dispatch on `wapOutA` inside `cmdBridge`, after the warp and before the blit, with
> no host bounce and no extra submit, byte-identical when off. **A style pass is that shader with a
> different body.**

---

# VERDICT — teacher/student, PhyriadFG

*Every code claim below was re-verified first-hand in `F:\Phyriad\projects\PhyriadFG` on 2026-09-07, not taken from the attackers. Where I found an attacker wrong, I say so.*

---

## 1. REPLACED

The runtime half survives untouched — a pure per-pixel operator applied every frame is genuinely free and genuinely cannot flicker, and PhyriadFG already has the exact pass shape to host it. Everything around it is replaced: the teacher moves offline and off the shipped product, the prompt becomes a *selector over an authored basis* rather than a *fit target*, and the operator class widens beyond RGB — which is NVIDIA's architecture with a prompt bolted onto the parameter step, i.e. the framing the proposal was written to differentiate itself from.

I am not calling this SURVIVES AMENDED. Three of the proposal's four load-bearing elements changed: *when* the teacher runs, *what* the prompt does, and *what the student reads*. Only the per-frame pass is the same.

---

## 2. The strongest objection that landed

**Content-adaptivity and non-flicker are the same knob turned in opposite directions, so the proposal cannot hold both of its two selling points at once.** (Attack 4 stated it cleanest; Attack 1 and Attack 2 arrived at the same wall from the fitting side.)

A 3D LUT cannot flicker *within* a LUT. But the proposal's entire differentiator is that the LUT is re-estimated online from a stochastic diffusion teacher. Every re-fit changes the LUT, and a LUT change is a **whole-screen simultaneous grade jump**. At the proposed 1–2 fps teacher rate on a 240 Hz output that is a 1–2 Hz global colour pulse across **120–240 presented frames per refit** (arithmetic on the stated teacher rate and the panel rate) — perceptually worse than local shimmer, because the eye integrates a full-frame pop and ignores a scattered one. The only fix is to slew the LUT over ~1 s, which at 240 Hz is ~120 frames of visibly-wrong filter after every scene change, and a slewed LUT no longer tracks the scene: it converges to the prompt's average look. Scene-adaptivity is the *only* thing the online teacher buys over an offline-authored basis. The no-flicker guarantee therefore requires disabling the sole justification for the most expensive component in the design.

Two flanking numbers make it worse rather than rescuing it:

- **The fit is priced at zero and is not zero.** An honest per-scene 3D-LUT fit by test-time optimization is **16.1 s** (SA-LUT's own timing of NLUT, RTX 3090) to **<30 s** (NLUT, ~20 Adam iterations, Titan RTX). The only sub-second number in the literature — **0.2128 s** — is a *forward pass through a network trained offline*, not a fit. Three groups (Zeng et al., AdaCM, SA-LUT) independently chose that architecture. Continuity does not buy back capacity; it trades a fixed wrong answer for a drifting one.
- **The LUT is under-determined by the teacher and over-determined by the scene.** Fitting from one captured frame constrains only the lattice cells that frame's colours occupy; the rest of the 35,937-vertex cube is your regularizer. This number is **not established for PhyriadFG captures and must be measured** — it is item (i) of Stage 1.

**And even if all of that failed, the deployment clause dies separately, on this rig, with no external citation.** Verified in `docs/planning/SATURATION_PLAN.md` §8.2 (live BF6, operator playing, ~45 s paired runs, PhyriadFG at defaults, **no teacher present**): row `p1 a base` reads multiplier **0.84×**, present 89.4/s, `iter` **11.2 / 22.8 ms** against a 4.16 ms tick deadline, preflow spin 13.9 ms, latency 82.8 ms, cons 47.9 of ~106 captured pairs/s. The frame generator is a net **loss** before anyone adds anything. The only cure ever measured — `--gpu-priority realtime` (§8.2 p5/p6) — restores 2.74–2.78× and 5.8 ms worst iter *by taking more GPU*, at **−18.6 % of the game's fps**, above the project's own ≤10–15 % band, which is why it ships opt-in. And there is no lever to schedule with: §8.1 lever 2 records that `VK_EXT_global_priority_query` reports **qfam 0 on this RTX 4090 supports nothing above MEDIUM** — the Vulkan priority path is a measured no-op — and WDDM `HIGH` is dead (mult Δ within ±0.03 across three paired runs). Preemption relocates a stall; it does not delete the GPU-seconds a UNet must consume.

---

## 3. What the architecture becomes

### 3.1 Corrections that must be carried forward — the brief was wrong three times

Verified today, all three attackers who checked (1, 2, 4) are right and the brief is wrong. Attack 3 repeated the brief's claim from a code *comment* rather than the code; on this point Attack 3 is mistaken.

| Brief's claim | Reality |
|---|---|
| binding 11 = contour field, free | **1×1 R32_UINT placeholder** (`wapFIELDph`) by default. `src/generate/warp_blend_init.cpp` — the real image `wapFIELDA` is created only under `cfg.d.field_to_warp`; `field_view = wapFIELDA.view ? wapFIELDA.view : wapFIELDph.view`. `igpu_field=false` at `src/control/cli.hpp:125`, and a bare run cascades `bg_snap`/`band_xfade`/`afill` off. |
| binding 13 = previous output, free | **The CURRENT real frame.** `prevout_view = wapPrevOutA.view ? wapPrevOutA.view : wCur.view`; `wapPrevOutA` exists only when `ts_smooth>0`, and the initializer at `cli.hpp:176` is `0.0f`. A pass binding 13 expecting "prev output" silently reads frame B. *(Side finding: the comment on that same line says "DEFAULT 0.1" while the initializer is `0.0f` — a doc/code mismatch worth a one-line correction.)* |
| Object masks + chamfer distance-to-rim are "already computed", implying available | **Never reach the GPU.** `obj_label` / `obj_chamf` / `obj_feat` are host `std::vector` in `HolonScratch`; grep across `src/` and `shaders/` returns only `flow.cpp:759,772-776,801-808` (allocation/aliasing) and `holons.cpp` (computation + consumption to rewrite `mv_field`). No VkBuffer, no image, no upload. The distance-to-rim ramp — the single best cel primitive in the list — is not on the GPU at all. |
| The Sobel field is "ZERO PCIe" | Zero-PCIe on the iGPU's **production** side only. Consumption costs a full-res `vkCmdCopyBufferToImage`, `full_bic(WW,WH)` into an R32_UINT image, per pair, in `src/present/present.cpp` (~:726). At 1080p that is **8.29 MB per real frame ≈ 0.99 GB/s at 120 real fps**, of which 2 of every 4 bytes are padding (`igpu_field.comp` packs `dist \| occ<<8`). |

**What *is* genuinely free, and nobody said it plainly:** the **pass shape**. `--afill` is already an in-place per-pixel compute dispatch on `wapOutA`, inside `cmdBridge`, after the warp dispatch and before the GENERAL→TRANSFER_SRC blit — no host bounce, no extra submit, race-free because each invocation touches only its own pixel, and byte-identical when off (`present.cpp` ~:1200-1226; `shaders/wap_fill.comp`). A style pass **is that shader with a different body.** That is the real "no plumbing work" in this codebase, and it is worth more than the descriptor slots the brief was counting.

### 3.2 The amended design

**Offline, on the operator's machine, shipped as data:**

- The teacher (img2img diffusion, or an LLM emitting parameters) runs **once per style**, never on a user's GPU. Corpus comes from `--qdump <dir> N` / `--dump N`, which already write real frames — and, uniquely, frames co-registered with the flow field, dissidence masks and Sobel field that the literature pays 882 ms of CPU Farnebäck to approximate.
- Output is a **basis of K authored styles**, K ≈ 8–24 (the size the state of the art picked when it had a free hand). Each style is a few hundred floats. Author them against **synthetic colour sweeps plus a held-out game corpus**, not one captured frame — this removes the under-determination problem by construction, because you control the source distribution over the whole cube.

**Runtime, in the .exe, per frame — three tiers, ship in this order:**

- **v0, the portable floor:** 3D LUT (or a DNCM-style 3→16→3 matrix pair, which is strictly cheaper in state: 256 floats per style, and preset vectors blend meaningfully where raw weights do not). Pure RGB. Runs on the iGPU too. Cost anchor: NLUT's measured apply is 0.05 ms at 512², 0.43 ms at 4K on a Titan RTX.
- **v1, the differentiator that costs almost nothing:** the same pass, with **Sobel magnitude as a fourth axis** — a 4D LUT, RGB × context. This is exactly SA-LUT's mechanism (which bought 4.70 dB PSNR / 66.7 % LPIPS over a 3D LUT on PST50), obtained from a signal PhyriadFG already computes per pixel. It requires `--igpu-field` on and its 8.29 MB/pair upload. **This is the single highest value-per-unit-risk change on the table**, and Stage 1 tests it for free alongside the kill test.
- **v2, the real project, do not price it as free:** an operator over `(RGB, Sobel mag, edge class, chamfer distance-to-rim, nearest-rim direction, object slot id, |MV|)`. Needs (a) a host bridge + EMH import + image + descriptor binding + F→A sync for `obj_label`/`obj_chamf`/`obj_feat`, and (b) a **guided upsample** from the 240×135 MV grid to per-pixel using the Sobel field as guide, because an 8×8-block silhouette drawn straight is 8 px chunky exactly where the eye looks. Neither exists. Attack 4's cooperative-vector MLP is the right runtime for this tier and its cost is measured on the right GPU at the right resolution — **0.055–0.113 ms at 1080p on a 4090** for a D=64 MLP, versus **2.3–2.8 ms** on the software-FMA fallback, so it is NVIDIA-gated and v0/v1 must remain the portable path.

**The prompt:** text → embedding → **blend weights over the K basis presets**, computed **once per prompt change**, off the hot path (remote API, or a 250M local model — 1.38 s measured for FlanT5-base on an L4 emitting 14 grading params plus a LUT choice from 24 presets). **No runtime refit, ever.** The operator changes only when the user types, at which point a visible transition is a feature. That makes "cannot flicker" *true* instead of self-contradictory.

**One synthesis point the attackers missed, and it defends the proposal's student class.** Attack 1's alternative — style only the real frames, let the warp carry the style, 4× duty reduction — collides with Attack 4's observation that a convolutional operator does not commute with the warp: `CNN(warp(x)) ≠ warp(CNN(x))`, so real and generated frames would stylize differently and the detail would strobe at the real-frame rate under a 240 Hz present. Attack 1 also concedes it needs a second colour-texture pair, because cel-flattened frames are the aperture problem everywhere and would wreck the block matcher. **A per-pixel operator commutes exactly and is immune to both.** So the per-pixel constraint is not merely a cost choice — it is what makes styling *after* the warp correct. That was the proposal's best instinct and it should be kept. Both are inferences, not measurements; neither attacker measured it.

---

## 4. The expressiveness ceiling, honestly

**The rule to ship, in one line: prompts that name a *palette* or a *tonal treatment* land; prompts that name a *medium* or a *drawing process* fail.**

A per-pixel operator can only re-map what is already in the pixel and its immediate neighbourhood. It cannot introduce texture (paper tooth, canvas weave, grain, hatching, stippling, halftone, brush strokes), cannot move a pixel (no silhouette simplification, no squash-and-stretch, no resample to a pixel grid), and cannot draw a line that is not already a gradient. The project's own recon already says this at `DLSS5_RECON.md`: Tier 1 "cannot add lighting or material detail — it changes colour, tone and contrast. That is a real limit, and it should be stated to the operator rather than sold around." The proposal re-sells around it by wrapping the same Tier-1 operator in a continuous-distillation frame.

**Lands well:** cel/toon, comic-ink, noir, sepia/period, bleach-bypass, "washed out", neon/synthwave grade, thermal/infrared, colourblind simulation, high-contrast posterize, "Ghibli-ish palette" (the palette, not the drawing).

**Disappoints categorically, not approximately:** watercolour (pigment granulation + paper tooth are texture; wet-edge darkening needs the inward distance ramp that is CPU-only; colour bleeding needs a diffusion operator that does not exist), oil painting (needs a stroke-direction field), pencil/hatching, pixel art (needs resampling — and its screen-locked quantization grid crawls violently over moving content, which is flicker from the *structural* half even while the LUT half is provably stable), 1930s rubber-hose (that is character design, timing and film artifacts — a re-drawing, not a filter), mosaic, papercraft.

### "estilo caricaturizado" — it lands, and it is an unusually favourable case

**It lands.** Posterize plus a dark outline is literally the classical cel recipe, and both halves are within the operator class: flat regions are a many-to-one colour map, which is precisely what a LUT does best, and the outline is a threshold on a gradient PhyriadFG already computes per pixel.

**Why it is favourable rather than representative — the structural reason:** cartoon's identity is a **reduction** of information. Fewer colours, flattened gradients, suppressed texture. A per-pixel operator is an information-destroying map, so its natural range is exactly the set of styles defined by subtraction. It also collapses the under-determination problem: when the target has twelve colours, you do not need the fitted cube to be right everywhere, because large regions of input colour map to the same output. Every style that fails above fails because its identity is **added** information — a surface, a stroke, a grain, a redrawn shape. Cartoon is the one popular style that is mostly subtraction. Generalising from its success to "the filter is prompt-driven" would be the single most expensive mistake available here.

**Where cartoon still disappoints, concretely:**

1. **The outlines are "Find Edges", not silhouettes.** Sobel on a photorealistic game frame fires on brick mortar, foliage, gravel, fabric weave and dither as hard as on a character's rim. Fixing that is the guided upsample of the block masks — the piece that decides whether the look reads as *authored* or as a *filter*, and it does not exist.
2. **The contour signal collapses under motion**, documented in the repo's own shader: `shaders/wap_fill.comp:43-46` — "the game's motion blur erases edges in FAST regions (gradient below the threshold)", which is why `--afill` fades the tint by `|mv|`. Outlines will pop in and out with player movement.
3. **The outline inherits MV error exactly at edges.** The field is per-real-frame and must be advected to the present phase — `wap_fill.comp:31-35` reads it at `p + (1-t)*mv` — so the FG stack's worst error lands where an outline is most visible.
4. **The HUD gets cel-outlined.** Capture is post-composite: crosshair, subtitles, health bars, minimap. There is no UI mask. Binding 8's persistence counter is a staticness proxy at 8×8 blocks, and a stationary camera makes the whole world high-persist.
5. **The motion-derived object masks vanish when the player stands still** (clusters are connected components of dissidence > threshold — "moves differently from the fitted GME model"), and light up the whole screen as the camera pans. A look must hold when nothing moves.

**Where the attackers disagree, both readings:** Attack 1 says cartoon is a trap — posterize + edge overlay is a free ReShade preset and the teacher contributes nothing. Attack 4 says the depth-free motion silhouette (GME + dissidence + 16-slot temporal identity + inward chamfer ramp, on captured frames with no engine hooks) is a differentiator no ReShade filter has. **Both are right about different halves.** The *colour* half of cartoon is a preset that already exists for free. The *silhouette* half is where PhyriadFG could be uniquely good — and it is precisely the half that is not plumbed to the GPU, and precisely the half a diffusion teacher does not produce for you. The value in this idea is concentrated where the teacher is absent.

---

## 5. STAGE 1 — the experiment that proves or kills it

Days, not weeks. Produces numbers. Uses machinery that exists. **Note that the amended architecture puts zero teacher work on the box, so no contention experiment is needed — §8.2 already settled that question and the answer removed it.**

### S1a — the projection oracle (the kill test). ~1.5 days, no new C++.

Same shape as the M4 oracle gate: measure the **upper bound of the model class against the teacher**, on his own content.

1. **Capture** ~40 frames spanning 3–4 areas of one game, deliberately including a dark interior and a bright exterior: `--qdump <dir> N` or `--dump N` (frames\ BMP). Existing flags, zero code. Run one pass with `--igpu-field` so the per-pixel Sobel field is captured alongside.
2. **Teacher**: any img2img stylizer, offline, seconds per frame is fine — it is an oracle, not a runtime. Prompt: `estilo caricaturizado`. Run a plain colour-grade target too, as the easy control.
3. **Fit the best possible operator of each class** by regularized sparse least squares (trilinear weights are linear in the LUT vertices; 3D Laplacian smoothness + monotonicity; CG on the normal equations). Do not cheap out — you want the class's *bound*, not a bad implementation of it.
   - **class A** = 33³ 3D LUT (the proposal's student)
   - **class B** = 4D LUT, RGB × Sobel magnitude, 2 context bins (the v1 upgrade)
4. **Report five numbers:**
   - **(i) occupancy** — fraction of the 35,937 lattice cells with ≥1 and with ≥64 samples, per single frame and across a 30-second traverse.
   - **(ii) in-scene projection error** — PSNR and LPIPS of class-A output vs **the teacher's output** (not vs the source; vs the teacher is the only number that matters).
   - **(iii) cross-scene** — fit on scene A, apply to scene B, same metrics.
   - **(iv) class B, same two.**
   - **(v) the paired spatial probe** — Sobel edge-magnitude ratio and HF-FFT energy of student vs teacher, because the recon documents a configuration that improved warping error 18.34 → 12.96 *while collapsing Sobel by 60 %*: a metric win that was a quality loss. Any style gate must pair a temporal/error metric with a detail probe, exactly as M4's oracle paired k with byte-exactness.

**Stop rules — what result means stop:**

- **In-scene class-A PSNR-vs-teacher below ~22 dB, or LPIPS above ~0.30, on the cartoon target → the LUT student cannot carry "estilo caricaturizado". Stop.** (Anchor: NLUT scores 20.59 dB / 0.36 LPIPS on the *easier* photorealistic-grading task; a cartoon target that scores worse than that is not a near miss.)
- **Cross-scene PSNR more than ~3 dB below in-scene → the fitted operator is scene-bound.** That does not stop the project; it *decides the product*: no frozen single fit is shippable, and since continuous refit is forbidden by §2, the answer is an authored basis. Expect this outcome.
- **Occupancy at ≥64 samples below ~10 % of cells → over 90 % of any teacher-fitted LUT is your regularizer.** Stop fitting from captured frames; author the basis against synthetic colour sweeps.
- **Class B fails to beat class A by ≥2 dB → the Sobel context axis is not earning its 8.29 MB/pair upload**, v1 collapses back to v0, and C-2 gains no second reason (see §6).

### S1b — the contour-stability probe. ~0.5 day, no new C++.

Before believing any outline-based look, find out whether the signal is stable enough to draw a line on. Run `--igpu-field --afill --afill-mv-gate 0 --csv --dump N` on live content and on the deterministic `ball_zoo` source. From the dumped frames compute **edge-band persistence**: over a 100-frame window, for every pixel that is edge-band (`occ=1`) in at least one frame, the fraction of frames it stays edge-band after MV advection.

**Stop rule: median persistence below ~0.7 on live content → a threshold-on-Sobel outline will visibly strobe**, and cartoon needs the block-mask guided upsample (v2, a real project with a host bridge), not a shader tweak. Do not ship an outline on a signal that fails this.

### S1c — the free cost number. Minutes.

A/B `--afill` on/off with `--csv` and read the `iter_ms` delta. `--afill` is already a per-pixel in-place pass on `wapOutA` in the same submit — its marginal cost *is* the style pass's cost class. Sanity-check it against the derived headroom: **4.16 − 3.36 = 0.80 ms** at `--flow-scale 1` on a GPU-light scene (`docs/BENCHMARKS.md` §12: MsGPUBusy 3.36 ms, internal iter 2.73 ms, 95 % util / 294 W), and **~3.2 ms** at `--flow-scale 2` (§13: iter 0.94 ms, 56 % util, 138 W).

---

## 6. What is the operator's call, not ours

### (a) The model-pack product shape

Engineering has removed the hard constraint: with the teacher offline, the shipped artifact is **K presets × a few hundred floats = kilobytes**, embeddable exactly the way `CMakeLists.txt:48-65` already embeds every compiled SPIR-V blob as a `constexpr` array. The single self-contained .exe survives — *for the operator*. The open question is the **prompt encoder**, and it is a product-identity question:

- **(i)** Presets embedded, prompt → blend weights via a remote API. Single .exe intact; the styled mode needs network. 
- **(ii)** Presets embedded plus a small local text encoder embedded. Single .exe intact, no network, adds tens of MB and fixes the prompt vocabulary to whatever that encoder knows.
- **(iii)** Optional downloadable pack. Single .exe dies for the styled mode only.

There is no measurement that decides between these. It is what he wants the product to *be*.

### (b) The default output rate the styled mode forces

Verified nuance that changes the framing, and no attacker had it: **`SATURATION_PLAN.md` §9 REFUTES the assumption that capping output returns GPU to the game.** Live BF6, realtime: uncapped 240.7 present / game `arr` **83**; cap 120 → present 120.1 / game `arr` **79**; gpu-A only 91 → 88 %; and the latency *floor* worsens (min 14.5 → 18.5 ms), which the operator felt as "same or slightly worse". Root cause named there: the game's saturation tax is **capture-side** — the WGC `CopyResource` of every *game* frame plus bandwidth contention — which scales with the game's fps, not our output.

So: a per-presented-frame style pass costs *us* linearly in output rate, and decimating output returns nothing to the game. The real levers are therefore (1) accept the pass at full rate inside the 0.80 ms headroom, (2) make the styled mode default to `--flow-scale 2`, which is the only lever that actually returns GPU (iter 2.73 → 0.94 ms, util 95 → 56 %, power 294 → 138 W) at a quality cost of `gme_dissidence` 0.02 → 0.15, or (3) refuse to run the styled mode in the saturation regime. Our recommendation is (2), because it is the only one that buys headroom without costing output rate — **but the dissidence trade is explicitly logged as an operator-eye call in §13, so it is his.**

### (c) C-2 — is `--igpu-field` on by default now justified by a second reason?

**Conditionally yes, and I will not hand you a free yes.**

The genuine second reason: the per-pixel Sobel field is the **only** structural signal that reaches the GPU at all (the object labels and the chamfer field are host-only, verified), and it is the context axis that turns a 3D LUT into a 4D one — SA-LUT's exact mechanism, worth a measured 4.70 dB on the published comparison, obtained from a signal the codebase already produces. That is a real, independent justification, unrelated to M1.

But it is **not free**, and the brief was wrong that it is: 8.29 MB per pair at 1080p (`present.cpp` ~:726, `full_bic(WW,WH)` into R32_UINT), ~0.99 GB/s at 120 real fps, **half of it padding** because `igpu_field.comp` packs only `dist | occ<<8` into a 4-byte uint. It also cascades off entirely without the iGPU convert path, so any style tier built on it needs a discrete-GPU Sobel fallback.

**Therefore: C-2's second reason is contingent on S1a's class-B result.** If the Sobel context axis beats the plain LUT by ≥2 dB against the teacher, the field has earned a second justification and the default flip stands on two legs instead of waiting on M1 alone. If it does not, the second reason evaporates and C-2 goes back to resting entirely on M1. Either way the packing waste is a separate, cheap win worth taking: 2 useful bytes in a 4-byte word is a 50 % PCIe reduction available for the cost of a format change.

---

### Where the attackers disagreed — both readings, on the record

1. **Is the student's plumbing free?** Attack 3 says yes (quoting `fg_core.comp`'s own comment about bindings 11/13 staying "in the layout as unused entries"); Attacks 1, 2 and 4 say no. **Verified: 1/2/4 are right — the descriptor slot exists, the data does not.** But Attack 3 is right about something that matters more and that the others missed: the *pass shape* (`--afill` / `fillPipeA`) is free, tested, and byte-identical when off.
2. **Is the middle rung affordable?** Attack 1 estimates a 4-layer 32-channel CNN at 0.5–1.5 ms → 24 % GPU duty at 240 Hz and kills it on the per-second cost; Attack 4 estimates 0.8–3 ms and calls it borderline, while citing a *measured* per-pixel MLP at 0.055–0.113 ms at 1080p on a 4090 via cooperative vectors. Both CNN figures are estimates; the distinction between them is real — a conv stack is bandwidth-bound on 132 MB activation tensors, a per-invocation MLP is not — and it means the MLP is affordable where the CNN is not. **Neither is measured on this rig. That is a measurement, not an argument.**
3. **Is cartoon a differentiator or a free preset?** Covered in §4 — both, on different halves.
4. **Style the reals and let the warp carry it?** Attack 1 proposes it for the 4× duty reduction; Attack 4's non-commutation argument refutes it for any convolutional operator, and Attack 1 itself concedes it needs a second colour-texture pair so the flow stack keeps reading unstylized frames. Both are inferences. The per-pixel operator sidesteps the whole dispute.

---

## The four attacks, verbatim

### ATTACK 1 — THE STUDENT IS TOO WEAK TO CARRY A PROMPT

**Verdict on the proposal:** REJECT AS FRAMED; SALVAGE ONE HALF. The architecture is sound as engineering (a LUT genuinely costs nothing per frame and genuinely cannot flicker) and is wrong as a product claim. The student's reachable set is roughly 40-50 meaningful scalars over {colour map} x {posterize} x {gradient-threshold outline}; "cartoon" partially works, and "watercolour", "pixel art", "1930s rubber-hose", "oil painting" fail categorically, not approximately, because a 3D LUT cannot introduce texture, cannot move a pixel, and has no neighbourhood. The honest product is a prompt that selects and blends among authored presets. Separately, three concrete claims about PhyriadFG in the proposal are false at default settings: binding 11 is a 1x1 placeholder (igpu_field default OFF, cascade at cli.cpp:205-222), binding 13 is the CURRENT real frame not the previous output (ts_smooth=0 default, warp_blend_init.cpp:235), and the object labels + chamfer distance-to-rim field never reach the GPU at all — they are CPU std::vector scratch consumed to rewrite MVs and discarded each pair (holons.hpp:33-45, holons.cpp:337-360). The project's own recon already stated the honest limit (DLSS5_RECON.md:676) and this proposal re-sells around it.

**Strongest objection:** THE LUT IS UNDER-DETERMINED BY THE TEACHER AND OVER-DETERMINED BY THE SCENE — so the proposal's central guarantee cannot coexist with its central mechanism. Fitting a 3D LUT from one occasional captured frame only constrains the lattice cells that frame's colours occupy; the rest of the cube is your regularizer. Freeze the LUT and it is wrong in every scene but the fitting scene. Re-fit continuously — which IS the proposal's differentiator versus NVIDIA's fit-once — and the operator becomes content-dependent, so "cannot flicker" degrades into a slow global grade drift under camera pan and a hard colour pop at every scene cut (menus, loading, cutscenes, alt-tab). DLSS5_RECON.md:628 already concedes the drift for Zeng et al.; :640-ff flags cuts as an unsolved hazard for every warp/cache stabilizer. Continuity does not buy back capacity: NVIDIA distilled once into 148M parameters of expressive room; this distils continuously into ~45 scalars, trading a fixed wrong answer for a drifting one.

| what | value | conditions | est? |
|---|---|---|---|
| Meaningful steerable DOF of the proposed student (colour grade + cel knobs), ver | ~40-50 continuous parameters (nominal 107,811) | 26 of them anchored on the published prompt-to-parameter system in DLSS5_RECON.md:616 (arXiv 2410.02952: 14 gl | est |
| Dimension of the projection a prompt must survive: CLIP text embedding -> studen | 768 -> ~45 | CLIP ViT-L/14 text embedding width is 768; target is the DOF count above. The projection is easily learnable;  | est |
| Maximum tracked objects and minimum object size in PhyriadFG's segmentation | 16 slots; 6 blocks minimum (= 384 px at 1080p) | kObjSlots=16 and kObjMinMass=6 at src/flow/flow.hpp:32-33; MV grid is 8x8 blocks, 240x135 at 1920x1080. Faces, | measured |
| Silhouette-outline granularity if drawn from the object/dissidence masks | 8 px blocky at 1080p | dissidence and object masks live on the MV grid (240x135 for 1920x1080); the Sobel field (shaders/igpu_field.c | measured |
| PCIe cost of getting the 'zero PCIe' contour field onto the 4090 | 8.29 MB per real frame (= 497 MB/s at 60 captured fps); 50%  | vkCmdCopyBufferToImage with full_bic(WW,WH) into an R32_UINT image, src/present/present.cpp:726, at 1920x1080. | measured |
| Per-frame cost of the middle rung — a ~20K-weight, 4-layer 32-channel 3x3 CNN at | ~0.5-1.5 ms per frame (compute 0.25-0.42 ms, bandwidth ~0.3- | MY ARITHMETIC, unmeasured. 2.0736 Mpx x 20,160 MAC/px = 83.6 GFLOP/frame; 4090 spec ~330 TFLOPS dense FP16 ten | est |
| Duty cycle of that CNN student at the 240 Hz target, versus at the 60 fps real-c | 24% of the GPU at 240 Hz; 6% at 60 fps | Assuming the 1 ms midpoint above. 240 frames/s x 1 ms = 240 ms/s. This rig is single-GPU to PhyriadFG and shar | est |
| Per-frame cost of the LUT half | under 0.2 ms at 1080p; cube is 287 KB | One trilinear fetch per pixel from a cache-resident 33^3 RGBA16F 3D texture on a 4090, my estimate. Independen | est |
| Correction to the '600K-parameter model produces a LUT' figure the proposal lean | 3 basis LUTs of steerable DOF, not 600K | DLSS5_RECON.md:622 — in Zeng et al. the small CNN runs on a DOWNSAMPLED copy to predict fusion weights over a  | measured |
| Frame budget the whole argument sits inside | 4.16 ms at 240 Hz / 8.33 ms at 120 Hz / 16.6 ms at 60 Hz, sh | DLSS5_RECON.md:666-667. One distilled diffusion step alone is 10.65 ms at 512x512 on a 4090 with TensorRT — 2. | measured |

**What would fix it:** Four things, in order, and the first two are cheap enough to run this week.

1. MEASURE THE LUT'S UNDER-DETERMINATION BEFORE BUILDING ANYTHING. Take 20 captured frames from 3-4 different areas of one game. For each, count how many of the 35,937 cells of a 33^3 lattice contain at least one pixel. My estimate is 5-25% occupancy; if it is at the low end, the proposal's central mechanism is dead on arrival because 75-95% of every fitted LUT is your prior. Then fit a LUT on frame set A and apply it to frame set B and measure the delta against the teacher's own output on B. That one experiment settles the whole architecture and costs a Python afternoon. It is the same shape as the M4 oracle gate.

2. STOP CALLING IT GENERATION. Ship the prompt as a SELECTOR AND BLENDER over K authored looks (K = 8-24, exactly what arXiv 2410.02952 chose when it had a free hand, DLSS5_RECON.md:616). Prompt -> CLIP embedding -> blend weights over K authored LUT+parameter presets. This is honest, it is bounded, the reachable set is inspectable, and it removes the under-determination problem entirely because the LUTs are authored over the full cube, not fitted from one frame. It also removes the teacher from the hot loop, so scene cuts and pans stop producing colour pops.

3. IF THE STRUCTURAL LOOK IS THE POINT, DO THE PLUMBING THE PROPOSAL ASSUMES EXISTS. Specifically: (a) upload obj_label as a per-block R8 label image and obj_chamf as a per-block R8 normalized distance-to-rim (they currently die in H

**Better alternative:** STYLE THE REAL FRAMES ONLY; LET THE WARP CARRY THE STYLE TO THE GENERATED ONES.

The proposal spends its entire budget defending "the operator must be free per frame", which is what forces the student down to a LUT. That constraint is self-imposed. PhyriadFG captures real frames at ~60 fps and generates the rest. If the style pass runs only on captured frames, the duty cycle drops 4x (my estimate above: 24% -> 6% at a 1 ms pass), and the middle rung — a ~20K-weight per-pixel CNN, which CAN add texture, hatching and stroke structure, and whose temporal stability is bought at training time with a flow loss at zero runtime cost (DLSS5_RECON.md:681) — becomes affordable. The generated frames inherit the style for free, because warping a stylized frame yields a stylized frame. That is strictly more expressive than a LUT at strictly lower total GPU cost than styling every presented frame.

Two hazards it must be built around, both concrete:

(a) COMPUTE FLOW ON THE ORIGINALS, WARP THE STYLIZED COPIES. Cel-shading destroys optical flow: flat posterized regions are the aperture problem everywhere, and a block matcher on them returns garbage. PhyriadFG's binding layout already separates the sampled colour (bindings 0/1) from the MV field (binding 2), so the fix is a second pair of colour textures holding the stylized reals, with the flow stack left reading the originals untouched. That is bounded, real plumbing — and unlike the proposal's claimed plumbing, it is honestly scoped.

(b) 

---

### ATTACK 2 — THE FIT IS THE UNSOLVED PART

**Verdict on the proposal:** REJECT AS SPECIFIED; a narrower version survives. Three separable claims, three separate fates.

(1) "Run an expensive teacher off the hot path and apply a cheap per-pixel operator every frame" — SOUND, and published. This is Transform Recipes (SIGGRAPH Asia 2015), Bilateral Guided Upsampling (SIGGRAPH Asia 2016), and, in the exact LUT form proposed, "Video Color Grading via Look-Up Table Generation" (arXiv 2508.00548, 2025). Keep it.

(2) "The student is a 3D LUT + cel-shader parameters, and this makes a prompt-driven CARTOON filter viable" — FAILS. The LUT half loses a measured 4.70 dB PSNR / 3x LPIPS to a merely one-dimension-richer spatially-adaptive operator, on the EASIER task (photorealistic grading, where the target really is a colour function). The cel half has no fitting procedure at all — the proposal never states one, and recovering hand-shader parameters from a teacher IMAGE is inverse rendering against non-differentiable GLSL, not a fit. And the whole guided-upsampling family's published boundary is exactly "cannot introduce new edges", which is what a cel shader is for.

(3) "It cannot flicker because the teacher outputs a FUNCTION" — SELF-CONTRADICTORY. True only of a FROZEN LUT. A frozen LUT is precisely the version that fails on scene change (its own authors say so). An adapting LUT is a global whole-frame discontinuity on every refit, from a stochastic diffusion teacher, at 16-30 s per honest fit. You get flicker-immunity or adaptivity, never both.

(4) The

**Strongest objection:** The proposal's two halves fail for the SAME reason, and the split between them is what conceals it.

A stylizer maps (colour, neighbourhood) -> colour. The proposal factors this into "LUT handles colour, hand shader handles structure" and then fits only the LUT. But the fit is driven by a teacher IMAGE, and an image is a sample of the JOINT function. Projecting it onto the LUT (a pure per-pixel colour function) is measured to cost 4.70 dB PSNR and 3x LPIPS on the easier photorealistic-grading task, because — SA-LUT's words — a 3D LUT applies an "identical transformation for same-color pixels across different semantic regions (e.g. sky vs. sea)." And the residual, the part the projection throws away, is exactly the STRUCTURAL part that the hand-written cel shader was supposed to supply. So the fitting step discards precisely the information needed to set the cel shader's parameters, and there is no second procedure that recovers it. The factorization does not divide the problem; it drops half of it on the floor and never picks it up.

This is not speculation. The most generous published version of this exact fitting scheme — BGU, whose student is a full bilateral grid of local affine matrices, strictly richer than a global LUT — is documented at its boundary in Guided Linear Upsampling §6: for style transfer, "BGU may completely ignore new edges," and these methods are "not suitable for applications that may introduce new edges," a "common limitation for universal guided upsampling methods including JBU and BGU." Introducing new edges is the definition of cel shading. The published boundary of the entire method family runs through the middle of the stated goal.

And when you patch the scene-adaptation hole, the headline claim dies too: "cannot flicker" describes a FROZEN function, but the proposal's value is that the function is re-estimated online, from a stochastic diffusion teacher, at a real cost of 16-30 s per honest fit (0.21 s only for an offline-trained feed-forward predictor). Every refit is a global, whole-frame, discontinuous recolour on an overlay whose whole reason for existing is 240 Hz freshness. Flicker-immunity and prompt-driven adaptivity are the same knob at opposite ends.

| what | value | conditions | est? |
|---|---|---|---|
| 3D LUT baseline (NLUT) on photorealistic style transfer — the projection-error d | LPIPS 0.36 / PSNR 20.59 dB / SSIM 0.80, vs SA-LUT LPIPS 0.12 | SA-LUT, ICCV 2025, PST50 benchmark, photorealistic style transfer. SA-LUT's extra capacity is ONE context dime | measured |
| Fit cost of a 3D LUT by test-time optimization — the number the proposal prices  | ~20 Adam iterations, under 30 seconds per content/style pair | NLUT (arXiv 2303.09170), single NVIDIA TITAN RTX, batch size 8, after 350k-iteration offline pretraining on MS | measured |
| Fit cost when the fit is replaced by an OFFLINE-TRAINED feed-forward predictor — | 0.2128 s (SA-LUT generator); 6 ms for a full 4K frame (AdaCM | SA-LUT on RTX 3090; AdaCM on one V100 at 4K; Zeng et al. on one Titan RTX at 4K, model trained at 480p and app | measured |
| LUT APPLICATION cost — the one part of the proposal's cost model that is correct | 0.05 ms at 512x512, 0.43 ms at 4K, 1.72 ms at 8K (7680x4320) | NLUT, TITAN RTX, S=32 W=32 compressed LUT. Against a 4.16 ms budget at 240 Hz, the per-frame apply is genuinel | measured |
| The proposal, already published: a diffusion model that outputs a 3D LUT, and it | 16x16x16x3 LUT, 25 DDIM steps, 12.10 s for 480 frames at 512 | "Video Color Grading via Look-Up Table Generation", arXiv 2508.00548, Condensed Movie Dataset, trained on 4x A | measured |
| Per-pixel in-shader MLP inference cost — pricing STUDENT B | 0.60-0.70 ms at 1080p | NVIDIA RTX Neural Texture Compression "Inference on Sample", RTX 5060, 1080p, via cooperative vectors (Tom's H | measured |
| Extrapolated MLP-student inference cost on the target rig, as a fraction of fram | roughly 0.2-0.6 ms at 1080p, i.e. 5-15% of the 4.16 ms budge | ESTIMATE. Scaling the measured RTX 5060 0.60-0.70 ms figure to a 4090 by rough throughput ratio. Conclusion is | est |
| Direct closed-form 33^3 LUT least-squares fit from one 1080p pair (the cheap fit | order 50-300 ms single-thread CPU, or ~10 ms on GPU | ESTIMATE, derived not measured. 2,073,600 pixel pairs, 8 trilinear vertex weights each, 35,937 unknowns per ch | est |
| 3D LUT grid occupancy from a single game frame — THE UNMEASURED NUMBER THAT GATE | NOT ESTABLISHED — must be measured on PhyriadFG captures bef | 33^3 grid = 32,768 cells / 35,937 vertices; a 1080p frame supplies 2,073,600 samples but they lie on a thin ma | measured |
| Temporal smoothing cost if the LUT is re-fitted online, at PhyriadFG's target ra | a 0.5 s EMA time constant = 120 frames of visibly-wrong filt | ESTIMATE, arithmetic from the 240 Hz target. Required because the teacher is a diffusion model: different nois | est |
| Cooperative-matrix / cooperative-vector machinery present in PhyriadFG today | zero | MEASURED IN CODE 2026-09-07. grep -rci cooperative over src/ and shaders/ returns no hits. Enabled device exte | measured |
| How many of the fourteen descriptor views the brief calls free are actually PLAC | 2 of the 3 the proposal wants (binding 11 and binding 13) | MEASURED IN CODE 2026-09-07. src/generate/warp_blend_init.cpp:223 — binding 11 is wapFIELDA only under --afill | measured |
| Whether the object-id / distance-to-rim inputs for an MLP student exist GPU-side | no — they are host-side std::vectors, never uploaded | MEASURED IN CODE 2026-09-07. obj_chamf is std::vector<int32_t> and obj_feat is std::vector<uint32_t> at src/fl | measured |
| Granularity mismatch between the proposed MLP's inputs | per-pixel colour and Sobel vs 240x135 (8x8 blocks) for objec | MEASURED IN CODE. shaders/igpu_field.comp writes 1 uint per pixel (byte 0 = Sobel magnitude, byte 1 = edge-ban | measured |

**What would fix it:** Three experiments, in this order, none of which requires writing a line of PhyriadFG code until the third. Total cost: about two days. Any of the first two can kill the project cheaply, which is the point.

EXPERIMENT 1 — RGB CUBE OCCUPANCY (half a day, one Python script, no new C++).
Take real captured frames from 3-4 titles, deliberately spanning dark interiors and bright exteriors. Histogram them into a 33^3 grid. Report, per frame: fraction of the 32,768 cells with >=1 sample, with >=8 samples, with >=64 samples. Then report the same across a whole 30-second traverse.
DECISION RULE: if a typical single frame populates under ~10% of cells, then over 90% of the shipped LUT is authored by the smoothness regularizer and not by the teacher, and "prompt-driven" is false for most of the colour cube. Fix the fit by pooling N frames from the same scene (which raises coverage but also raises latency and makes the operator less scene-specific — measure the trade, do not assume it).

EXPERIMENT 2 — THE PROJECTION TEST (one day, entirely offline, no PhyriadFG involvement).
This is the empirical version of SA-LUT's 4.70 dB, measured on HIS content with HIS style.
  a. Capture 20 frames spanning several scenes.
  b. Run ANY img2img stylizer offline at whatever speed you like — seconds per frame is fine, it is an oracle, not a runtime.
  c. For each pair, fit the BEST POSSIBLE 33^3 LUT by regularized least squares (procedure in §1 of the attack). Do not cheap out — this is the upper boun

**Better alternative:** THE ARCHITECTURE THE FIELD CONVERGED ON, WITH THE PROMPT BOLTED ON WHERE IT ACTUALLY FITS.

Invert the proposal's central move. It says: expensive teacher ONLINE, cheap FIT online. Three independent groups (Zeng et al., AdaCM/AAAI 2023, SA-LUT/ICCV 2025) all say instead: expensive training ONCE OFFLINE, and the runtime step is a FORWARD PASS THAT PREDICTS THE OPERATOR'S PARAMETERS — never a fit. That is the whole difference between 0.2 ms and 30 s.

CONCRETELY:

STEP 1 — CHANGE THE STUDENT'S FUNCTION CLASS. Replace the global 3D LUT with a SPATIALLY VARYING per-pixel operator. Two options, both per-pixel evaluable in a shader, both cheap, both strictly better than a LUT:
  (a) A BILATERAL GRID OF LOCAL AFFINE MATRICES (BGU, SIGGRAPH Asia 2016). Runtime = one trilinear slice + one 3x4 matvec per pixel. No ML runtime, no new Vulkan extension, no NVIDIA lock-in, kilobytes of state, and it works on the AMD iGPU too. This is the highest value-per-unit-risk change available.
  (b) A 4D LUT WITH ONE CONTEXT CHANNEL (SA-LUT's shape: 17x17x17 RGB x 2 context bins). And PhyriadFG already computes a natural context channel for free: the Sobel contour magnitude in shaders/igpu_field.comp, which is per-pixel and needs no new granularity work. Feed contour magnitude as the 4th axis. That is SA-LUT's 4.70 dB / 66.7%-LPIPS mechanism, obtained from a signal the codebase already produces.
Either one buys the measured gap the proposal's LUT is currently paying.

STEP 2 — MOVE THE EXPENSIVE WORK

---

### ## ATTACK 3 — the system will not hold together on one 4090 with a game running

**Verdict on the proposal:** REJECTED as specified, on measured evidence from this exact rig — but the rejection lands on one clause, not the whole idea. The student half (LUT + cel parameters fitted offline, consumed per-frame from machinery PhyriadFG already computes) is sound and cheap and has plumbing waiting for it (fg_core.comp bindings 11/13 already in the descriptor set). The teacher half — "a large pretrained img2img diffusion model on a background queue on the same 4090" — is refused by three independent measured facts: (1) SATURATION_PLAN §8.2, live BF6, no teacher, the FG is ALREADY at 0.84x multiplier with iter 11.2 ms avg / 22.8 ms worst against a 4.16 ms deadline; (2) §8.1, VK_EXT_global_priority above MEDIUM is DRIVER-REFUSED on this 4090, and WDDM HIGH is measured dead, so no scheduling lever protects the present loop; (3) the only cure that works, --gpu-priority realtime, costs the game 18.6% fps and works by TAKING more GPU — the teacher would spend the cure. VRAM is NOT the constraint (and the brief's 598 MB ring figure is sysRAM, not VRAM — core_init.cpp:412). Compute contention is, and the single-file shipping model is collateral. Verdict: kill the diffusion teacher on the 4090; keep the student; move the teacher off-box, offline, or make it a 138K-600K-param net on the iGPU.

**Strongest objection:** The teacher wants the one resource this project has already MEASURED itself to be starved of, on this exact rig, with no teacher present. SATURATION_PLAN.md §8.2 (live Battlefield 6, operator playing, PhyriadFG at defaults): multiplier 0.84x, iter 11.2 ms avg / 22.8 ms worst against a 4.16 ms tick deadline, preflow spin 13.9 ms, latency 82.8 ms, F consuming 48 of ~107 captured pairs/s. The frame generator is a net LOSS under a real game before anyone adds anything. The only cure ever measured on this rig — `--gpu-priority realtime` — restores 2.76x multiplier and 5.8 ms worst iter by TAKING MORE GPU, at a measured cost of 18.6% of the game's fps, which the project's own protocol flags as above its 10-15% band and therefore ships opt-in and off. The proposal asks to spend that cure on a teacher. There is no headroom to schedule into; there is a deficit. And there is no lever to schedule with: VK_EXT/KHR_global_priority above MEDIUM is DRIVER-REFUSED on this RTX 4090 (measured, §8.1 lever 2), D3DKMT HIGH is measured dead (mult delta +/-0.03 across three paired runs), and CUDA MPS is 64-bit Linux only. GPU preemption is the proposal's best remaining hope and it still fails, because preemption relocates a stall — it does not delete the GPU-seconds the UNet must consume.

| what | value | conditions | est? |
|---|---|---|---|
| PhyriadFG multiplier under live Battlefield 6, defaults, NO teacher | 0.84x (present 88/s against a 240 Hz tick; cons 47.9/s of ~1 | live BF6, operator playing, ~45 s run, steady-state mean after 10-stat-line warmup, GPU-A 99%; docs/planning/S | measured |
| PhyriadFG tick-loop `iter` under live BF6, no teacher, vs the 240 Hz frame budge | 11.2 ms avg / 22.8 ms worst, against a 4.16 ms budget — 2.7x | same run as above; SATURATION_PLAN.md §8.2 | measured |
| Governor deep-shed trip point vs external GPU-A load | tier:5 + bwd-skip 100% engaged at just 60% external load, he | tools/gpu_load.exe pinned to GPU A (--gpu 0), ball_zoo.ps1 -Fps 60 source, FG defaults; SATURATION_PLAN.md §1. | measured |
| --gpu-priority realtime: the only measured cure, and its price | mult 0.84 -> 2.76, present 88 -> 240/s, iter worst 24 -> 5.8 | live BF6, two order-flipped pairs (p5d/p6d) confirming each other; SATURATION_PLAN.md §8.2-8.3. -18.6% is ABOV | measured |
| VK_EXT/KHR_global_priority on the RTX 4090 (the Vulkan lever to rank the present | DRIVER-REFUSED — qfam 0 supports nothing above MEDIUM; measu | VK_EXT_global_priority_query pre-check, src/core/device.cpp:112-156, NVIDIA Windows driver on this rig; SATURA | measured |
| D3DKMTSetProcessSchedulingPriorityClass at HIGH=4 (the OBS 'GPU priority' mechan | DEAD — multiplier delta within +/-0.03 (noise) across three  | live BF6, src/core/main.cpp:220ff, NTSTATUS 0 (call succeeded); SATURATION_PLAN.md §8.3 | measured |
| One distilled denoising step, 512x512, RTX 4090, TensorRT | 10.65 ms | established fact from docs/research/DLSS5_RECON.md. Frame budget: 4.16 ms at 240 Hz, 8.33 ms at 120 Hz, 16.6 m | measured |
| Full SD-Turbo teacher invocation (text-encode + 1-4 UNet steps + VAE decode + LU | 50-250 ms, i.e. 12 to 60 consecutive missed 240 Hz tick dead | extrapolated from the 10.65 ms measured per-step anchor; ESTIMATE | est |
| SD-Turbo weights in VRAM (SD2.1 UNet 865M + VAE 83M + OpenCLIP-H 340M) | ~2.6 GB FP16 / ~1.3 GB INT8-FP8, plus ~0.5-1 GB activation+w | computed from published parameter counts; ESTIMATE. Fits comfortably in the 9.5-14.5 GB free — VRAM is NOT the | est |
| SDXL-Turbo weights in VRAM (UNet 2.6B + CLIP-L + OpenCLIP-bigG + VAE) | ~7 GB FP16 / ~3.5 GB INT8-FP8 + ~1-2 GB activations at 512x5 | param-count estimate for the working set; the 13.88 GB and 12 GB figures are cited. Fits only in the 12 GB-fre | est |
| Free VRAM on the 4090 with a game running | 9.5-14.5 GB (24 GB - game 8-12 GB - PhyriadFG ~0.5 GB - desk | ESTIMATE. CORRECTION to the brief: PhyriadFG's ~598 MB 28-slot capture ring is SYSTEM RAM, not VRAM — src/core | est |
| PhyriadFG's runtime VRAM-budget awareness | ZERO — no VK_EXT_memory_budget query exists in src/core/devi | grepped 2026-09-07. Consequence: if the teacher pushes the working set past 24 GB, WDDM evicts to sysRAM over  | measured |
| Shipping binary sizes today | build-release/phyriad_fg.exe = 1,355,264 bytes; dist/ui.exe  | measured on disk 2026-09-07. Payload mechanism: ui/src-tauri/src/lib.rs:19, extracted to LOCALAPPDATA named by | measured |
| TensorRT runtime footprint (the runtime, not the weights) | nvinfer.dll alone ~441 MB; full DLL set with cuDNN/CUDA depe | cited from NVIDIA developer forums and TensorRT installation docs; the 100-300 MB lean figure is an ESTIMATE.  | est |

**What would fix it:** FOUR fixes, in order of how much they cost and how much they save. The first is not optional and should happen before any code is written.

1. MEASURE IT, WITH THE INSTRUMENT THAT ALREADY EXISTS (cost: one afternoon, zero new code). tools/gpu_load.exe is in-tree and its --load N duty was verified 100% real on nvidia-smi. Run it as a FAKE TEACHER: 250 ms bursts at 1 Hz pinned to --gpu 0, under live BF6, with --csv, in four configurations (base / realtime x teacher-on / teacher-off). Read the per-present columns iter_ms, MsBetweenDisplayChange, phyriadfg_fresh, ring_occ and frz. Do NOT read slip — present.cpp:452-465 proves it will report a false all-clear. If the answer is "one 200 ms worst-iter line and ~48 rdrops per burst, plus a ring lap," the proposal is dead on numbers rather than on argument, and it dies in a day instead of a quarter. If the answer is milder than the collapse curve predicts, the proposal has earned the right to continue. Either way the project learns something durable about its own contention behaviour. Bonus experiment while you are there: query whether VK_QUEUE_GLOBAL_PRIORITY_LOW/IDLE is exposed on qfam 0 (device.cpp:132-145 already has the query plumbing). §8.1 established only that ABOVE-MEDIUM is refused; if LOW is granted, the teacher can at least be scheduled beneath the present queue, which is the single mechanism that would change the analysis.

2. MOVE THE TEACHER OFF THE BOX. This is the fix the proposal's own architecture is begging for and

**Better alternative:** The teacher runs OFF THIS MACHINE, or OFFLINE, or is a 138K-600K-parameter net on the iGPU — and the student stays exactly as proposed.

The proposal's central insight is correct and I want to be clear that I am not attacking it: an expensive model whose output is a FUNCTION (a 3D LUT + cel parameters) rather than an image costs nothing per frame, cannot flicker, and can be fitted far less often than it is consumed. That asymmetry is real, it is well-chosen, and PhyriadFG is unusually well-positioned to exploit it — fg_core.comp:12 states bindings 11 (contour field) and 13 (prev-output) "stay in the layout as unused entries" and the host already writes all fourteen views, so a cel/toon pass gets the per-pixel Sobel contour field AND the previous output frame with zero plumbing work. --afill already proves the contour field can be read and composited in place. The student is a weekend, not a quarter.

What the proposal has not noticed is that its own insight refutes the deployment it chose. If the teacher's output is a few kilobytes at 1-2 fps, then the teacher has NO REASON to be on the gaming machine at all. A few KB per second is a network payload. Every single constraint in this attack — 7 GB of weights, a 2.5 GB TensorRT runtime, a first-run engine build, VRAM eviction PhyriadFG cannot detect, a 250 ms hitch into a pipeline already measured at 0.84x multiplier, a driver that refuses every Vulkan priority lever, a preemption model that relocates stalls rather than deleting

---

### ATTACK 4 — THE PROPOSAL IS A LOCAL OPTIMUM, AND THE THING IT IS LOCAL TO IS "KEEP THE TEACHER RESIDENT."

**Verdict on the proposal:** Right runtime, wrong placement of the teacher — a local optimum whose locality is entirely the decision to keep the teacher resident. The student half is correct and should be built: a per-pixel operator applied every frame is genuinely free (measured comparable operators: 0.055-0.113 ms at 1080p on a 4090) and genuinely cannot flicker. The online teacher is a 6.94 GB liability that buys exactly one property — scene-adaptivity — which the proposal's own no-flicker guarantee forces it to slew away, at which point it is offline distillation with the expensive half left running in VRAM. It also violates the single-file shipping constraint outright, and it periodically bursts 10-60 ms of diffusion onto a GPU this repo has already measured collapsing from 240 to ~100 fps under saturation. Reject the online teacher; keep and substantially upgrade the student.

**Strongest objection:** Content-adaptivity and non-flicker are the same knob turned in opposite directions, so the proposal cannot hold both of its claims at once. A 3D LUT cannot flicker WITHIN a LUT, but every teacher re-fit changes the LUT and jumps the entire screen's grade at once — at the proposed 1-2 fps teacher rate that is a 1-2 Hz whole-frame colour pulse across 120-240 presented frames, perceptually worse than local shimmer. The only fix is to slew the LUT over ~1 s, and a slewed LUT no longer tracks the scene: it converges to the prompt's average look. Scene-adaptivity is the ONLY thing the online teacher buys over an offline-distilled basis. Therefore the proposal must disable the sole justification for its own most expensive component. What survives is offline distillation plus 6.94 GB of resident weights attached for no measurable benefit — a benefit the proposal never proposes to measure. Confirming this: StatLUT (arXiv 2607.08227, 2026-07-09) already produces a 16^3 3D LUT from a text prompt with NO reference image and NO img2img teacher, with a 0.38M-parameter mapper and a sub-0.1 ms apply, which means the proposal's output manifold is reachable from text alone.

| what | value | conditions | est? |
|---|---|---|---|
| PhyriadFG's own GPU cost per presented frame, single-GPU 4090 | 3.36 ms MsGPUBusy (internal iter_ms 2.73 ms); 4090 at 95% ut | Honkai: Star Rail, GPU-LIGHT scene, --force-single-gpu, PresentMon 2.4.1 + internal --csv telemetry, 2026-06-2 | measured |
| Actual per-frame budget available for a style pass at 240 Hz (default config) | 0.80 ms | My derivation: 4.16 ms (240 Hz) minus the measured 3.36 ms MsGPUBusy above, on a card already at 95% utilizati | est |
| Budget with --flow-scale 2 (the measured cost sweet spot) | iter_ms 0.94 ms, 56% util, 138 W -> roughly 3.2 ms free | HSR single-GPU, internal telemetry medians, same session as §12. docs/BENCHMARKS.md §13. Quality cost: gme_dis | measured |
| PhyriadFG under real GPU saturation — the failure any added per-frame work worse | presented fps 240 -> ~100; warp_ms 1.2 -> 5.2 ms; AddedLaten | BF6 max settings, 4090 at 99% in combat, bf6_p1_baseline.csv, first-hand. docs/BENCHMARKS.md §11 | measured |
| PhyriadFG's existing warp batch GPU time — the reference for 'cheap' in this cod | 0.08-0.11 ms | records/R4_GATE.md §4.6, cited in docs/LEARNING_LOG.md P-005 | measured |
| Per-pixel MLP evaluated in-shader via cooperative vectors — the winner's measure | D=64: 0.055-0.113 ms; D=32: 0.06-0.079 ms | MEASURED on an NVIDIA RTX 4090 at 1080p display resolution, Table 2, arXiv 2506.06040 (VarA/VarB). Excludes vi | measured |
| The same MLP WITHOUT VK_NV_cooperative_vector (software FMA fallback) — why the  | D=64: 2.3-2.8 ms (x20-49 slower); D=32: 0.387-0.785 ms | Same table, same RTX 4090, same 1080p. 2.3-2.8 ms is ~3x over the 0.80 ms default budget — the portable fallba | measured |
| Neural Preset / DNCM — the better colour student than a 3D LUT | 5.15M params total; FHD 0.013 s / 1.96 GB; 4K 0.019 s / 1.96 | MEASURED on NVIDIA RTX 3090 (CPU i9-11900KF: FHD 0.215 s, 4K 0.686 s). CVPR 2023, arXiv 2303.13511. Note the 1 | measured |
| StatLUT — prompt -> 3D LUT with NO img2img teacher and NO reference image | ~2 s per prompt end-to-end; final LUT apply <0.1 ms; 16^3 LU | MEASURED on an A800 GPU. Image-driven variant <50 ms/image. arXiv 2607.08227v1, 2026-07-09 | measured |
| SDXL-Turbo (the class of teacher the proposal would make resident) — the shippin | 6.94 GB fp16 on disk; ~7.0 GB VRAM at fp16 | stabilityai/sdxl-turbo sd_xl_turbo_1.0_fp16.safetensors on HuggingFace; VRAM figure for standard 1024x1024 gen | measured |
| Prompt -> grading parameters via a small LLM — the cheap replacement for the tea | 1.38 s for FlanT5-base (250M) on an L4; 1.63 s for Llama-2-7 | MEASURED, single inference pass covering all three tools; emits 14 global-adjust params + 12 selective-adjust  | measured |
| Candidate B (per-frame small CNN) cost at 1080p on a 4090 | 0.8-3 ms | MY EXTRAPOLATION from GameSR's measured 26.56 ms per 1080p frame on an Adreno 750 mobile GPU (138K params, 604 | est |
| Candidate B training cost — the honest solo-dev number, per style | 30,000 steps, batch 2, 640x360, single GTX 1080 Ti (ReCoNet) | Training config MEASURED/stated in arXiv 1807.01197; the 4090 wall-clock is my estimate. Prompt-conditioning i | est |
| Candidate C — defensible N (presented frames between teacher updates) and why | teacher period 67-71 ms -> N = 16-17 presented frames at 240 | MY ARITHMETIC from the measured 14-15 fps of the best full 1-step distilled + ControlNet pipeline on a single  | est |

**What would fix it:** TWO CHANGES, IN ORDER.

(1) MOVE THE TEACHER OFFLINE — it becomes a developer-machine label generator, not a shipped component. Run it once per style over a corpus PhyriadFG captures itself (~50 min of 4090 time per style, my estimate), fit the student to the teacher's output, ship only the student. This costs nothing the proposal actually keeps and recovers 6.94 GB of disk, ~7 GB of contended VRAM, the single-file .exe, months of diffusion-runtime integration, and the periodic GPU bursts that would trigger the documented 240 -> 100 fps saturation collapse.

(2) UPGRADE THE STUDENT FROM 'LUT + hand-written toon params' TO A PER-PIXEL MLP OVER PHYRIADFG'S OWN FEATURE VECTOR — (R,G,B, Sobel magnitude, edge-band class, chamfer distance-to-rim, nearest-rim direction, object slot id, |MV|) — evaluated in-shader via VK_NV_cooperative_vector, measured at 0.055-0.113 ms at 1080p on a 4090 (arXiv 2506.06040 Table 2), comparable to the existing warp batch's measured 0.08-0.11 ms. A 3D LUT is the degenerate 3-in/3-out case of this and costs about the same while discarding every feature the project already computes. Ship a portable LUT/DNCM fallback for non-NVIDIA and for rigs without the iGPU convert path (--igpu-field is DEFAULT OFF and cascades off entirely without it, core_init.cpp:447-453).

BEFORE EITHER, RUN THE ABLATION THAT DECIDES WHETHER THE TEACHER EXISTS AT ALL. Produce the same style three ways and blind eye-gate them: (a) LUT fitted from an img2img teacher's output on the 

**Better alternative:** Offline-distilled per-pixel neural operator over PhyriadFG's existing feature vector. The teacher (an img2img diffusion model, or an LLM emitting shader code) runs ONCE, OFFLINE, on the developer's machine, to author a basis of N styles; it never ships and never touches the user's GPU. Each style is a small parameter set — a DNCM-class 256-float preset (Neural Preset, CVPR 2023: 5.15M params, 0.013 s at FHD / 1.96 GB on an RTX 3090, k=16, only 256 image-adaptive params per image) or a set of MLP weights under ~50K params. At runtime the prompt maps to a CLIP text embedding and a barycentric blend over that basis, computed once per prompt change — or handed to a 250M FlanT5 / a remote API in 1.38 s (arXiv 2410.02952, measured). Per frame, one per-pixel operator applies: an MLP over (R,G,B, Sobel magnitude, edge-band class, chamfer distance-to-rim, nearest-rim direction, object slot id, |MV|), evaluated in-shader via VK_NV_cooperative_vector at a MEASURED 0.055-0.113 ms at 1080p on an RTX 4090 (arXiv 2506.06040 Table 2). A 3D LUT or DNCM is the degenerate RGB-only case and is the portable fallback for AMD and for rigs without the iGPU field path. This beats the proposal on every axis: identical per-frame cost, identical zero added present latency, strictly greater expressiveness (it learns ink-line width, rim falloff and interior quantization from the contour and chamfer fields instead of from hand-written parameters), strictly better temporal stability (per-pixel, so it commut

---

*Made with my soul - Swately <3*
