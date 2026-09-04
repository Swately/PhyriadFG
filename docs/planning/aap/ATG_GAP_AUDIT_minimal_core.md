# ATG — single-design gap audit of apps/minimal_fg vs A0 (2 clean lenses, sonnet)

## lens: completeness-auditor

## GATE VERDICT: **SCORED**

Full first-hand read completed: `src/main.cpp` (1,560/1,560 lines), `include/minimal_fg/seam_graph.hpp` (549/549), `CMakeLists.txt` (152/152), `test/test_seam_graph.cpp` (437/437), `A0_FROZEN_OBJECTIVE.md`, `DESIGNER_BRIEF.md`, and the full MINIMAL_FG plan triad. All citations below are verified against the actual files, not the triad's or the fact pack's self-reported claims.

---

## Defect list

### Class B — kill-criterion breaches (as built today)

**D1. KC3 breach — `ALL_COMMANDS` barriers on the hot path, the exact anti-pattern SG exists to remove.**
Severity: **FATAL**
`apps/minimal_fg/src/main.cpp:159-167` — `img_barrier()` issues every barrier with `VK_PIPELINE_STAGE_ALL_COMMANDS_BIT` for *both* src and dst. This helper is called on the per-frame hot path at least 7 times per presented frame: `main.cpp:767-770` (init), `main.cpp:1208-1216` (the `up_load` lambda, invoked twice per frame at `1328-1329`, and again at `1376` on the fallback path), `main.cpp:1361-1364` and `1371-1372` (the C↔bridge blit chain), `1377-1380`/`1387-1388` (raw-B fallback), `1400-1402` (warm-up clear). `seam_graph.hpp:38` itself states the discipline this violates: *"PRECISE masks ONLY - never VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT (the cited 13%-waste anti-pattern)."* SG is used for exactly 2 trivial import-acquire barriers (A, B); every other barrier in the pipeline — including the ones added for the one feature this design actually shipped (the overlay) — is hand-written using the forbidden mask. This is a direct, textual violation of frozen A0 §6/§4.3, verifiable independent of any candidate's self-report.

**D2. KC4 breach — cannot express the shipping default; `wap_warp.comp` is not even compiled in.**
Severity: **FATAL**
`apps/minimal_fg/CMakeLists.txt:82-95` compiles exactly 5 OFP shaders (pyr_down, hier_match, hier_match_fg, `optical_flow_warp.comp`, affine_fit) plus `overlay_fps.comp`. `wap_warp.comp` appears nowhere. `main.cpp` contains zero SG passes, structs, or CLI paths for any of the 8 named default-affecting layers (mv-guided, inertia gate, phase-anchor, bg-reclaim, ambig, vblend, single-track/screen-static, stasis) — grep-equivalent confirmed by full-file read; the only `sg.add_pass` call in the entire file is `main.cpp:797` (`"mc_interp"`). No A/B against today is possible on this base as built.

### Class A — M4 (veto), M1, M2, M3: piece present vs. missing

**D3. M4 unattainable as built — different warp shader, no default-set, no `t_use`.**
Severity: **FATAL** (M4 is a VETO)
The interpolation math is `optical_flow_warp.comp` (146 lines, per the pillar) via `ofp.record_optical_flow(...)` at `main.cpp:801`, not `wap_warp.comp`'s cited core (A0 §0, lines 21-22: `A[uv−t·mv]`/`B[uv+(1−t)·mv]` + Gate1/Gate2 + blend, `wap_warp.comp:518-521,679,692-695,1136-1185`). Nothing in the design or triad demonstrates equivalence between the two shaders. Combined with D2 (no default layer set) and D4 (fixed `t`), there is no mechanism by which this design could produce the byte-identical/PSNR≥60dB output M4 requires.

**D4. M1 unmeasurable under the named workload — `t` is hardcoded, no replay-record production path.**
Severity: **FATAL / F3**
`main.cpp:801` — `ofp.record_optical_flow(c, ofp_a_view, ofp_b_view, c_view, 0.5f)` — `t` is the literal constant `0.5f`. The render loop (`main.cpp:1218-1468`) has no NCO/PLL/`t_use` equivalent to the full FG's `present.cpp:1975/2054-2059/2221→2370` (per DESIGNER_BRIEF §1). The motion-truth zoo requires arbitrary phase `t`; this design can only ever be exercised at one fixed phase. There is also no `--qdump+`-equivalent replay-record path (prev/next/live + t + MV + SAD + gme + push block, per A0 KC1) anywhere in `main.cpp` — `--sg-dump` only dumps the compiled *barrier graph*, not a per-frame record. Missing piece would live in the render loop at `main.cpp:1218-1468`; it does not exist.

**D5. M2 — the core/layer contract, the entire subject of this search, is absent.**
Severity: **FATAL**
`seam_graph.hpp:88-93` (`Access{res,stage,access,layout}`) and `:511-516` (`PassDecl{name,reads,writes,record}`) carry no scalar-parameter declaration mechanism at all — SG can express image reads/writes only, never a layer's parameter list. `main.cpp` adds nothing on top to close this gap: there is exactly one pass, zero layers, zero enable/disable contract, zero parameter registry. DESIGNER_BRIEF §2 names this exact question ("how the generation core and its layers are expressed as passes and contracts… the contract between core and layer") as the one thing this search decides — the candidate under audit does not address it at any granularity.

**D6. M2(a) — the one real "add a feature" data point measured, not estimated, and it fails badly.**
Severity: **MAJOR**
The FPS overlay is the only feature this design actually added after the core scaffold. It touches 3 files (`main.cpp`, new `shaders/overlay_fps.comp`, `CMakeLists.txt:92-95,100`) and 3 non-contiguous sites within `main.cpp` alone: pipeline setup (`826-865`), per-frame dispatch (`1340-1355`), teardown (`1528-1535`) — against the ≤2-files target of M2(a). This is measured evidence from the candidate's own committed code, not a hypothetical.

**D7. M2(b) — the only layer-like feature bypasses SG's cull-when-off, is invisible to the `--sg-dump` instrument.**
Severity: **MAJOR / F3**
The overlay is never registered via `sg.add_pass`/`declare_image` (confirmed: only one `add_pass` call exists in the file, at `797`, for `mc_interp`). Its enable/disable is a runtime bool check (`main.cpp:1340`: `if (overlay_enabled && overlay_ready)`) with a hand-written barrier (`1341-1349`), not SG culling. `--sg-dump` (`main.cpp:818-820`) therefore cannot certify the overlay's disabled-cost is 0 — the one instrument M2(b) names cannot see the one layer-shaped thing in the codebase.

**D8. M2(c) — core contract size is undefined, not merely large.**
Severity: **MAJOR / F3**
No struct or SG-tracked parameter set corresponds to "the core contract" (target ≤8 params: `residual_ceil, improvement_frac, agreement_threshold, t, mv-source, output`). The only visible entry point is the raw call `main.cpp:801` (5 positional args, `t` fixed, `mv-source` unexposed/internal to OFP). There is no header, table, or struct at any file:line that this metric can be read off.

**D9. M3 unmeasurable — no `--csv` telemetry.**
Severity: **MAJOR / F3**
Full-file read of `main.cpp` shows only `printf`-based fps logging (`main.cpp:1280-1284`, `"[mfg] fps in=.. out=.."`). The M3 instrument named in A0 (`--csv` + `ball_zoo.ps1`) has no producer in this design; the missing piece would live in the per-frame stats block at `main.cpp:1249-1286` and does not exist there.

### Class C — triad/code contradictions

**D10. Master plan's declared MC-2→MC-3→MC-4 separable-pass architecture is not what the code builds.**
Severity: **MAJOR**
`MINIMAL_FG_MASTER_PLAN.md:79-83` (§2 diagram) and `MINIMAL_FG_IMPLEMENTATION_STRATEGIES.md:119-136` (S4) describe flow, warp, and blend as three distinct SG passes with declared edges — exactly the shape `test/test_seam_graph.cpp:81-96` (`build_chain()`, the canonical 4-pass golden fixture) illustrates. The actual `main.cpp` graph is one opaque pass (`mc_interp`, `797`) wrapping a single third-party call (`801`) whose internal flow/warp/gating is invisible to SG. The golden test exercises a synthetic chain that main.cpp's real graph never matches — the test proves the library works, not that the design follows the plan's own architecture.

**D11. Risk register's "Commit scoping" note is stale against the code under audit.**
Severity: **MINOR**
`MINIMAL_FG_RISK_REGISTER.md:28-38` states the SG engine "is NOT wired into any FG runtime (no capture, no present…)" and that MR-1/MR-5/MR-7 (present-pacing/device-loss, no-game-cap, present-surface) "remain open." `main.cpp` (STEP 2-4) now wires real WGC/DDA capture and real `PresentSurface` present into SG. None of MR-1/5/7's named verifications (30s soak, TDR-force, affinity/priority grep) have a recorded pass for this code; the register was not updated to reflect the STEP 2-4 integration it now covers.

### Class D — coverage gaps vs. the named workload

**D12. Static-HUD-over-moving-background marker class has no candidate mechanism.**
Severity: **MINOR** (downstream of D2/D5 — no layers exist, so no HUD-handling layer can exist either; not a separate root cause, listed for completeness per instructions)

---

## Salvageables (present in the candidate, unchanged by the objective)

- **`include/minimal_fg/seam_graph.hpp`** (549 lines) — the RAW/WAW/WAR/import-acquire derivation, precise-mask discipline, deterministic `compile()`/`dump()`. This is the frozen "declared constraint" itself; nothing in this audit's findings touches its correctness.
- **`test/test_seam_graph.cpp`** — the GPU-free golden/layerability/cull/determinism/WAW-WAR/cross-scope-read/RMW/layout-after-read suite. A real, reusable regression harness for the derivation logic regardless of how the core/layer contract question is eventually resolved.
- **The host-staged capture ring** (`main.cpp` CapCtx/DdaCtx, WGC + DDA backends) — a genuine, documented stability fix (removes the A/B keyed-mutex stall class), orthogonal to the core/layer question and reusable as MC-1 under any angle.
- **`--sg-dump`** mechanism (`seam_graph.hpp` dump/stringifiers) — working infrastructure, currently under-exercised (2 barriers) but structurally reusable once a richer pass graph exists.
- **OpticalFlowPipeline-by-direct-source-inclusion build pattern** (`CMakeLists.txt` `mfg_spv()` + direct `.cpp` inclusion) — a working, reusable build mechanism independent of the contract question.

## lens: practitioner-realist

## GATE ATG — SINGLE-DESIGN GAP AUDIT: `apps/minimal_fg` vs `A0_FROZEN_OBJECTIVE.md`

**Gate verdict: SCORED**

---

### A/B/D — Defect list (metric/kill-criterion mapping + workload coverage)

**FATAL — F3 (M1, unmeasurable / structurally single-phase)**
`src/main.cpp:801` — the only call into the generation core hardcodes `t=0.5f` as a source literal (`ofp.record_optical_flow(c, ofp_a_view, ofp_b_view, c_view, 0.5f)`; confirmed by comment at `main.cpp:38`). There is no content-clock/NCO/PLL and no per-frame `t` anywhere in the file. M1's instrument needs per-marker error **as a function of phase t** across the motion-truth zoo (§1, "phase error in ms-equivalent"); this candidate can only ever sample the exact midpoint. No amount of instrument-build closes this — the candidate itself has no varying-`t` operating point to measure.

**FATAL — F3 (M1, KC1 unmeasurable / no replay-record feed)**
`src/main.cpp:483-503` (the complete arg-parse loop) — no `--qdump`/`--qdump+` flag or any prev/next/live+t+MV+SAD+gme record-emission exists anywhere in the file (verified: zero grep hits for "qdump" in the whole source). The only "dump" is `--sg-dump` (`main.cpp:818-820`), a structural SG-graph printout, not a data replay record. KC1's instrument literally cannot be fed by this candidate as built.

**FATAL — M4 veto (different core math, not the reused wap_warp core)**
`src/main.cpp:33-41,801`; `CMakeLists.txt:88-89` — the generation core is not `wap_warp.comp`'s A/B-sample + confidence/agreement-gate + blend math at all; it is OFP's own `optical_flow_warp.comp` (146 lines), explicitly described in-source as "REPLACES the prior hand-rolled... flow/warp passes." `A0_FROZEN_OBJECTIVE.md:61-62` lists `wap_warp.comp`'s core math as an asset "a candidate MUST reuse, not rewrite." Byte-diff/PSNR≥60dB equivalence against today's default (M4, the veto metric) is not achievable from different sampling math, independent of any layer question.

**FATAL — KC4 (cannot express the shipping default)**
`src/main.cpp:786-823` (`setup_interp_pipeline`, the entire SG/OFP wiring) — none of the 8 default-affecting layers named in `DESIGNER_BRIEF.md:27-30` (mv-guided, inertia gate, phase-anchor, bg-reclaim, ambig, vblend, single-track/screen-static, stasis) exist anywhere in the 1,560-line file. Verified by full-file read, not a sample. Zero of 8.

**FATAL — KC3 (host round-trip between stages)**
`src/main.cpp:19-32` (design rationale), `203-230` (`make_host_buffer`), `1196-1216` (`map_copy`/`up_load`), `1328-1329` (invoked every frame for both A and B) — the MC-1→MC-2 boundary is a deliberate D3D11-staging→CPU-`memcpy`→host-VK-buffer→`vkCmdCopyBufferToImage` round trip, every frame, for both images. This directly contradicts the design's own P1 gate criterion: `MINIMAL_FG_IMPLEMENTATION_STRATEGIES.md:39` — *"GPU-resident verified (**no host round-trip in the core**)"* — and the master plan's own diagram label `MINIMAL_FG_MASTER_PLAN.md:44,80-83` — *"GPU-resident texture import (**zero host copy**)."* The file that names itself "the P1 GATE" (`main.cpp:3`) fails the P1 gate's own written text.

**FATAL — KC3 (ALL_COMMANDS barriers on the per-frame hot path)**
`src/main.cpp:159-167` (`img_barrier`, both src/dst = `VK_PIPELINE_STAGE_ALL_COMMANDS_BIT`) — called at `767-770` (init), `1209-1215`×2 (every-frame A/B upload), `1361-1364`, `1371-1372`, `1377-1388`, `1400-1401`. This is the majority of the real per-frame barrier traffic (only `mc_interp`'s 2 import-acquires go through the SG's precise-mask path). `seam_graph.hpp:38` and `MINIMAL_FG_IMPLEMENTATION_STRATEGIES.md:168` both state the design must "never" use `ALL_COMMANDS` — KC4's own text names this exact pattern as "the overhead the MINIMAL_FG arc removed."

**MAJOR — M2 unaddressed (no core/layer separation mechanism)**
`src/main.cpp:462-1559` — one monolithic `main()`; `745-866` inlines device+capture+OFP+SG+overlay setup in a single lambda; layer/flag parsing is a hand `strcmp` chain (`488-502`), not a registry/table. There is no file boundary between "core" and "a layer" for M2a's "≤2 files to add a layer" to even apply to — the mechanism the metric measures does not exist yet.

**MAJOR — M2c core-contract mismatch**
`src/main.cpp:797-802` — the `mc_interp` pass exposes **zero** runtime parameters (not the required `residual_ceil, improvement_frac, agreement_threshold, t, mv-source, output`); `t` is a compiled-in literal. Vacuously "≤8," but not the parameter set the objective defines, because the underlying shader isn't the one the objective specifies (see M4 finding above).

**MAJOR — Check C: the design's only extra "layer" violates its own plan's discipline**
`src/main.cpp:1340-1355` — the FPS overlay is gated by a runtime `if (overlay_enabled && overlay_ready)` and hand-records a classic `VkImageMemoryBarrier`/`vkCmdPipelineBarrier`, entirely outside `sg.execute(cmd, sg_compiled)` (`1330`). `MINIMAL_FG_MASTER_PLAN.md:113-114` states a disabled layer must be "CULLED... not an `if` inside a shader and not a registered-but-skipped pass." The one worked example of "a second pass" in the codebase is exactly the anti-pattern the plan disclaims.

**MAJOR — M3 unmeasurable as built**
No `--csv`/telemetry/`lat` output exists anywhere in `main.cpp` (confirmed by grep: zero hits). The only rate signal is an ad hoc `printf` FPS pair (`main.cpp:1271-1285`), not the `DESIGNER_BRIEF.md`-cited `--csv` + `lat` stat instrument M3 requires. Present cadence vs panel rate cannot be produced.

**MAJOR — catalog-coupling / adoption unaddressed**
`CMakeLists.txt:53,109,112,118-127` — `${CMAKE_CURRENT_SOURCE_DIR}/../../catalog/cpp/...` hardcodes exactly two directory levels below a `catalog/` sibling, true only at the container root (`F:\Phyriad\apps\minimal_fg`). `A0_FROZEN_OBJECTIVE.md:57-59` declares Home as `F:\Phyriad\projects\PhyriadFG` (git); `DESIGNER_BRIEF.md:46,59` states adoption is a "declared operator decision, not a candidate's assumption." None of the triad or the CMake addresses path resolution once moved into the repo — the relative paths break outright at a different nesting depth.

**MAJOR — Check C: risk-register/gate process contradiction**
`MINIMAL_FG_RISK_REGISTER.md:17` ("No commit of minimal-core code while any MR is `open`") vs `MINIMAL_FG_RISK_REGISTER.md:42-53` (all MR-1…MR-8 still tabulated `open`, dated "design time" 2026-06-26) — while `main.cpp` (labeled "STEP 4 of the P1 increment... the P1 GATE," build-green) already ships real capture/present/device-loss runtime code engaging MR-1/MR-5/MR-7 territory. The register was never reconciled against the shipped code.

**MAJOR — Check D: P2–P5 unbuilt, each load-bearing for the objective**
- P2 (harden SG: async-compute, precise batched barriers) — not built; `IMPLEMENTATION_STRATEGIES.md:156,348` scope it to P2/S7, and the ALL_COMMANDS finding above shows it's not even the precise-barrier half of P2 that's done outside the single SG pass.
- P3 (pacing + DROP) — explicitly out per `main.cpp:69-71`; present uses a flat `Sleep(2)` (`1464-1467`) whose own comment records **measured** internal evidence of waste ("in stayed ~72 while out spun to ~401 DUPLICATE presents") — directly relevant to, and undermining, M3.
- P4 (measure minimal-vs-full) — not run; no instrumentation to run it with (see M3 finding).
- P5 (re-layer under net-gain gate) — this stage **is** the subject matter of the frozen objective's §2 decision ("how core and layers are expressed as passes and contracts"); nothing in `minimal_fg` attempts it.

**No violation found — KC5, KC6:** external capture only (WGC/DDA), LUID-matched single device, zero foreign-PID affinity/priority/frame-cap calls found in a full-file read — KC5 compliant. `windowsapp`/WinRT (`CMakeLists.txt:147`) is an OS component, not a new redistributable — read as compliant with KC6, though it sits outside the literal "Vulkan + D3D11/DXGI + VC++ runtime" wording (flagged, not scored as a defect).

---

### E — Salvageables (present in the candidate, unchanged)

- **`SeamGraph` engine** (`include/minimal_fg/seam_graph.hpp:229-548`): RAW/WAW/WAR derivation with precise stage/access masks, import-acquire handling, backward-reachability culling, deterministic `dump()` — correct within its declared single-queue first-cut scope. This is the "already fixed SEAM" `A0_FROZEN_OBJECTIVE.md` itself carries forward as a declared constraint.
- **`test/test_seam_graph.cpp`**: GPU-free golden/adversarial suite (GOLDEN, LAYERABILITY, CULL, DETERMINISM, WAW/WAR checks; `CHECK` macro confirmed present) — a reusable regression harness for any SG-based core.
- **`--sg-dump`** (`main.cpp:818-820`): matches M2's own cited instrument column verbatim ("`--sg-dump` pass/barrier census").
- **`OpticalFlowPipeline` reuse for the flow stage** (`main.cpp:722,801`; MV+SAD at W/8) — legitimate compliant reuse of the mandated flow asset, independent of the warp-substitution defect above.
- **Dual capture backend (WGC default + DDA fallback)** with device-loss latches, fence-stall backstop, window-death watchdog (`main.cpp` throughout §6-9) — a working, defensively engineered MC-1 capture implementation, salvageable as the capture stage on its own, independent of how it currently feeds downstream (the host-round-trip critique above).
