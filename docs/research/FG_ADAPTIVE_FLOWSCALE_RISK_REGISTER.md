# FG_ADAPTIVE_FLOWSCALE_RISK_REGISTER — runtime-adaptive `--flow-scale` (Tier-2, device-loss / concurrency class)

> **Diátaxis type:** Planning / design (Explanation, normative outcome). **Status:** **IMPLEMENTATION
> BLOCKED — runtime re-init NOT shipped this session.** The runtime divisor change cannot be made
> crash-safe in a clean scope without first building a primitive that does not exist (a 3-thread
> quiesce/resume barrier; §2 R-AFS-1). Per the directive ("if the runtime re-init cannot be made
> crash-safe in a clean scope, STOP + report the honest blocker rather than ship a racy re-init"), no
> re-init machinery was added. This register is the hazard map AS CODE for the focused build that the
> design ([`FG_ADAPTIVE_FLOWSCALE_DESIGN.md`](FG_ADAPTIVE_FLOWSCALE_DESIGN.md) §3) sequences AFTER the
> present-architecture (own-window) decision settles the pipeline foundations.
>
> **Plan tier:** Tier 2 — third leg of the triad. Its siblings are the DESIGN (which doubles as the
> master plan + implementation strategy at this stage; a dedicated `*_MASTER_PLAN.md` /
> `*_IMPLEMENTATION_STRATEGIES.md` pair is to be split out at the start of the focused build).
>
> **THE GATE (hard rule, §3 of the Plan-Tier protocol):** the runtime-adaptive `--flow-scale` change MUST
> NOT be committed while any risk here is `open`. Each MUST reach `mitigated` (code in + verified
> first-hand) or `accepted` (residual stated + who accepted). A risk cannot be silently dropped. **At the
> time of writing, R-AFS-1 / R-AFS-2 / R-AFS-3 are `open` (no mitigating code exists) and R-AFS-DOGMA is
> `mitigated`-by-construction (the flag is unbuilt → the static path is literally untouched).** Therefore
> the runtime lever is NOT commitable today; this register exists so the next builder enters with the
> failure modes already mapped to the real sites.
>
> **Verifiable-reference mandate (FDP):** every `path:line` and symbol below was re-anchored first-hand in
> this session against `apps/render_assistant/src/main.cpp`. Line numbers drift under the concurrent edits
> on this file — the **symbol** is the durable anchor; the line is a hint at the time of writing.

---

## §0 — Why this register opens `blocked`, not `mitigated`

The design's §3 (corrected first-hand 2026-06-22) already re-classified the runtime form from T1 to **T2
ARCHITECTURAL**. This session re-verified that verdict against the source and it holds — in fact the
coupling is wider than the "~10 sites" the design estimated. The grounds, all first-hand:

1. **There is no quiesce/resume barrier between the three worker threads.** `thr_c` / `thr_f` / `thr_p`
   (spawned `main.cpp:9545-9547`, joined only at shutdown `:9557`) coordinate through exactly two
   primitives: `g_quit` / `g_quit_threads` (`:3012`, `:5389`) — a **terminal** signal (exit only, never
   resume) — and `a_q2_mtx` (`:5329`) — a submit-only serialization of A.q2 between C and F, **not** a
   safe-point barrier. A runtime re-init needs all three threads PARKED at a known-safe point with no GPU
   work in flight; that primitive must be **built from scratch**, and building a 3-thread park/resume
   barrier is itself the risk-bearing concurrency work.

2. **The flow_div divisor is an init-time foundational constant**, computed ONCE (`flow_div`,
   `main.cpp:3246`; `WW_flow`/`WH_flow`, `:3250`) and threaded through ~30 resources across **three
   devices** (A, FD, G) — far more than a "small pipeline swap":
   - `ofp` (the whole `OpticalFlowPipeline`: pyramid + MV image + SAD + cand_image), `ofp.init(...,
     WW_flow,WH_flow,...)` `:4468`, plus its INTERNAL prebaked FG descriptor sets
     `ofp.prebake_fg_descriptors(...)` `:4504`.
   - The EMH host bridges, **double-imported on FD+A**, ×`kGenRing`: `hostMV`/`hostSAD`/`hostMVB`/
     `hostC2`/`hostDIS`/`hostDISB`/`hostPER` + their `hMV_b`/`hMV_a`/… imports (`:4049-4114`). These are
     `_aligned_malloc` + `hbuf_import` — **external-host-pointer device imports**, an L3-class resource
     window per `AGENT_PROTOCOL.md §13.1`.
   - The P-thread warp's sampled grids: `wapMVA`/`wapSADA`/`wapMVBA`/`wapPERA`/`wapC2A` (`mvw×mvh`,
     `:4684-4718`) + the warp descriptor set bound to them (`wap_create`, `:2910`).
   - The aliasing consumers: `gme` (storage views alias ofp's mv/sad, `gme_destroy` `:9678`), `mv_smooth`
     (`:9675`), the `nvofa` provider convert sets (`:9682`), `Bflow[0]/Bflow[1]` (the downsample scratch,
     `:9695`).

3. **All of that is consumed live by all three threads simultaneously:** C uploads `Bframe`; F dispatches
   the flow, reads the MV grid for the CPU gme fit, fills the host bridges; P uploads the host bridges into
   the WAP grids and dispatches the **warp** reading `wMV`/`wSAD`/`wMVB`. Re-sizing any of them while a warp
   dispatch is in flight, or while F's copy-out is mid-write to a host bridge, is a torn read → garbage or a
   device fault.

4. **The pipeline foundations are not settled** (the design's explicit precondition). The present-class
   work is in flight right now (`HEAD` neighbourhood: D2 per-instance window-class + device-loss latch,
   E1/D1 capability fields, A3-L4 FP16 present, own-window FG_OPTION_A). The design sequences the runtime
   re-init AFTER own-window precisely because both re-touch the pipeline's foundations.

This is **not** a refusal of the work — it is the honest scoping the directive asked for. The static
`--flow-scale N` (the measured cost win) and the init-time `--flow-scale auto` (`flow_scale_auto`, `:3237`)
are the SHIPPING cost levers and are untouched. The runtime-adaptive form is the deliberate architectural
project this register front-loads.

---

## §1 — Risk summary

| ID | Class | One-line failure mode | Severity | Status |
|---|---|---|---|---|
| **R-AFS-1** | crash / device-loss / concurrency | runtime tear-down of MV-grid resources races a live warp dispatch (P) / flow record (F) / Bframe upload (C) — no quiesce barrier exists → torn read or device fault | high | `open` (no mitigating code; the 3-thread park/resume barrier is unbuilt — the blocker) |
| **R-AFS-2** | crash (FATAL assert) / data-corruption | the MV-grid size is replicated at ~3+ sites (pre-init host-bridge derive, post-init accessor, WAP upload); a non-atomic re-size diverges them → the `:4478` FATAL assert fires, or an over/under-read corrupts the warp | high | `open` (no atomic-resize routine; the FATAL assert is present + correct as a tripwire, but no re-size path feeds it) |
| **R-AFS-3** | oscillation (quality pumping) | the adaptive responder (drop flow-res) and the shipped `--load-governor` (shed work) react to the same util/EMA from different angles → they FIGHT → quality pumping, possibly tripping the >4·period pacer re-seat | medium | `open` (no arbitration code; the single-responder-authority + hysteresis design is stated below, unbuilt) |
| **R-AFS-DOGMA** | dogma (byte-identical-off) | the adaptive flag's default path is not bit-identical to the current build → a silent regression of every static-`--flow-scale` user | high | `mitigated`-by-construction (the flag is UNBUILT → `parse_extra` is untouched, `flow_div` is still a single init constant, no runtime machinery exists → the static path is literally the current bytes) |

No **security** trigger (this surface reads a captured frame + presents locally; it adds no untrusted-input
/ network / privilege path). The **data-loss/irreversibility** trigger is NOT present (no persistent
artifact is written); the qualifying T2 triggers here are **crash/device-loss** (R-AFS-1/2) and
**concurrency** (R-AFS-1), plus the **dogma** trigger (R-AFS-DOGMA).

---

## §2 — Risk detail (each: failure mode @ site · mitigation AS CODE · verification · status)

### R-AFS-1 — the re-init RACE: tear-down vs live warp/flow/upload, with no quiesce barrier

- **Class:** crash / device-loss / concurrency. **This is the blocker.**
- **Failure mode (code terms):** changing `flow_div` at runtime requires destroying + recreating the
  MV-grid resources (§0.2). But:
  - The **P-thread** is, at any moment, mid-`wap_upload` (`main.cpp:7577`) writing into `wapMVA`/`wapSADA`/
    `wapMVBA` and then dispatching the **warp** that SAMPLES them; destroying or `img_create`-resizing
    `wMV`/`wSAD`/`wMVB` (`:4684-4686`) under that dispatch is a use-after-free / torn read on the GPU.
  - The **F-thread** is mid-`record_optical_flow` on `ofp` and mid-copy-out into `hostMV`/`hostSAD`
    (the EMH host bridges, `:4049-4059`); `ofp.shutdown` (`:9683`) + freeing the host pointers under that
    write faults.
  - The **C-thread** is uploading `Bframe` (the flow input); at `flow_div>1` F also blits `Bframe`→`Bflow`
    (`flow_downsample`, `:5717`) — resizing `Bflow` mid-blit faults.
  - **There is no existing safe point** where all three are parked: `g_quit`/`g_quit_threads` is terminal,
    `a_q2_mtx` is submit-only (§0.1). So today the re-init has nowhere safe to run.
- **Mitigation AS CODE (the design that a crash-safe build MUST implement — UNBUILT):** a single
  **re-init authority** owned by ONE thread (the **F-thread**, because it OWNS `ofp` + the flow dispatch +
  the gme read of the MV grid — the design's "P-side authority" framing is corrected here: the flow
  pipeline and its primary grid live on F, the warp on P only SAMPLES the grid), driving a built-from-scratch
  3-thread quiesce/resume barrier:
  ```cpp
  // NEW file-scope coordination (sketch — none of this exists today):
  std::atomic<int>  g_reinit_request{0};   // F sets the target flow_div; 0 = none
  std::atomic<bool> g_p_parked{false};     // P acks it is parked at a pair boundary, no warp in flight
  std::atomic<bool> g_c_parked{false};     // C acks it has stopped feeding Bframe
  std::atomic<bool> g_reinit_done{false};  // F clears it; P/C resume

  // F-thread, ONLY at a pair boundary (warp fence already retired for the pair it owns):
  if (want_div != flow_div_live.load()) {
      g_reinit_request.store(want_div);                 // ask P + C to park
      while (!g_p_parked.load() || !g_c_parked.load()) hal::spin_hint();  // bounded wait + timeout→abort-reinit
      vkDeviceWaitIdle(A.dev); if(FD.dev!=A.dev) vkDeviceWaitIdle(FD.dev);// EVERY in-flight GPU op retired
      reinit_mv_grid_resources(want_div);               // R-AFS-2 atomic re-size (one routine, all sites)
      flow_div_live.store(want_div);
      g_reinit_done.store(true);
  }
  // P-thread, at the top of its present loop, BEFORE any warp:
  if (g_reinit_request.load() && !g_reinit_done.load()) {
      vkWaitForFences(A.dev,1,&fBridge,VK_TRUE,UINT64_MAX);  // its own last warp retired
      g_p_parked.store(true);
      while (!g_reinit_done.load()) hal::spin_hint();        // park (or g_quit → exit)
      g_p_parked.store(false);                               // resume re-binds the WAP descriptor set
  }
  // C-thread mirrors P (park before the next Bframe upload).
  ```
  The barrier MUST: (a) be entered ONLY at a pair boundary by F; (b) `vkDeviceWaitIdle` BOTH devices
  before ANY `vkDestroy`/`img_create` (no GPU op may touch the old resources); (c) have a **bounded**
  park wait with a **timeout→abort-the-reinit** (never deadlock the present loop — a missed park degrades
  to "keep the current flow_div", never blocks); (d) be **rare** (gated behind the K-frame + dwell
  hysteresis of R-AFS-3, so the stall is bounded and infrequent, not per-frame). The EMH host-pointer
  re-import is an **L3 window** (§13.1) — the open/close path (`_aligned_free` of the OLD pointers AFTER
  the new imports succeed; reachable from crash-unwind via the existing `done:` teardown) must be
  verified before the first run, with `window_guard.py open/close` around the re-import.
- **Verification (when built):** (1) build-green; (2) a soak run (BF6 combat / Furmark) forcing repeated
  div switches under load with the validation layers ON → zero `VK_ERROR_DEVICE_LOST`, zero use-after-free
  / sync-hazard reports, the `--csv` `frz` counter shows no torn-read spike at a switch; (3) a 30 s crash
  gate clean (the §13 temporal-window gate). **None of this can be claimed until the barrier exists.**
- **Status:** `open`. No mitigating code shipped this session. The barrier is the blocker; it is the
  risk-bearing concurrency work the focused build must execute against this entry.

### R-AFS-2 — MV-grid atomic re-size: the FATAL assert / over-read at the replicated sites

- **Class:** crash (the FATAL assert) / data-corruption (warp over/under-read).
- **Failure mode (code terms):** the MV-grid size is derived at **three+ sites that MUST agree**:
  - pre-init, off `WW_flow`: `const uint32_t mvw=(WW_flow+7u)/8u, mvh=(WH_flow+7u)/8u;` (`main.cpp:4049`)
    — sizes the host bridges + the EMH imports.
  - post-init, off the accessor: `const uint32_t mvw=ofp.motion_width(), mvh=ofp.motion_height();`
    (`:4678`) — sizes the WAP sampled grids.
  - the WAP per-pair upload: `const uint32_t wap_mvw=ofp.motion_width(), wap_mvh=ofp.motion_height();`
    (`:7565`) — the `vkCmdCopyBufferToImage` extent.
  - A **FATAL init assert** already guards the pre-init-vs-accessor agreement:
    `if(ofp.motion_width()!=(WW_flow+7u)/8u || ...) { ...FATAL... }` (`:4478`). At init it aborts; at
    RUNTIME, if a re-size updates `ofp` but not the host bridges (or vice versa), the bridges over/under-read
    → corrupt MV field into the warp, or the assert (if re-checked) aborts the process mid-session.
- **Mitigation AS CODE (UNBUILT):** ONE `reinit_mv_grid_resources(uint32_t new_div)` routine that re-derives
  `WW_flow`/`WH_flow` and re-sizes EVERY site **atomically** (inside the R-AFS-1 barrier, after
  `vkDeviceWaitIdle`), in dependency order, then re-runs the SAME FATAL-assert check as a post-condition:
  ```cpp
  static bool reinit_mv_grid_resources(uint32_t new_div /*, all the resource refs by pointer*/) {
      const uint32_t WWf = WW/new_div, WHf = WH/new_div;
      const uint32_t mvw=(WWf+7u)/8u, mvh=(WHf+7u)/8u;
      // 1. destroy in reverse-dependency order: warp desc set, WAP grids, gme/mvsm/nvofa sets,
      //    host bridges (+ EMH imports), Bflow, ofp.  (Mirror the :9645-9695 teardown EXACTLY.)
      // 2. recreate ofp.init(...,WWf,WHf,...); re-derive mvw/mvh from ofp.motion_width().
      // 3. re-alloc + re-import the host bridges at the NEW mvw*mvh (the :4049 block, lifted to a fn).
      // 4. img_create the WAP grids at the new mvw*mvh (the :4684 block, lifted to a fn).
      // 5. wap_create / prebake_fg_descriptors re-bind the descriptor sets to the NEW views.
      // 6. POST-CONDITION (the same tripwire as init):
      if (ofp.motion_width()!=(WWf+7u)/8u || ofp.motion_height()!=(WHf+7u)/8u) return false; // → abort the reinit, keep old div
      return true;
  }
  ```
  The key invariant: the host-bridge derive (`:4049`), the WAP grid (`:4678`), and the upload extent
  (`:7565`) all re-read from the SAME re-derivation in the SAME critical section — never a partial update.
  A re-size FAILURE (alloc/import/create returns false) must roll back to the OLD div (keep the surviving
  old resources if possible, else exit cleanly via `g_quit`) — never run with a half-re-sized grid. The
  existing `:4478` assert is **correct as a tripwire** and is preserved; the mitigation is to give it a
  re-size path that never makes it fire mid-session.
- **Verification (when built):** the soak of R-AFS-1 with validation layers + the FATAL assert compiled
  in; assert MUST NOT fire across thousands of switches; `--qdump` of a frame right after a switch shows a
  coherent (non-garbage) MV field.
- **Status:** `open`. The FATAL assert tripwire exists and is correct (`:4478`); no atomic re-size routine
  feeds it.

### R-AFS-3 — the governor FIGHT: adaptive-flow-scale vs `--load-governor` oscillation

- **Class:** oscillation (quality pumping), with a secondary path to the pacer re-seat.
- **Failure mode (code terms):** the shipped `--load-governor` (`load_governor`, `main.cpp:727`) ALSO
  reacts to saturation: its F-thread tier ladder escalates `pressure_tier` off `t_pair_ema` vs
  `pair_budget_ms` AND the present-published 4090 util `g_gpu_a_util` (`:3018`), via `gov_floor`
  (`:5708`) — and SHEDS work (object_repair, scene-memory, gme iters, bwd) at tier 4/5. The
  adaptive-flow-scale responder would react to the SAME signals from a different angle (drop the flow
  RESOLUTION). Two independent controllers chasing one util target → they FIGHT: the governor sheds work →
  EMA drops → adaptive raises flow-res back → util climbs → governor sheds again → visible **quality
  pumping**, and if the period of the fight exceeds 4× the present period it can trip the
  `--pace-present` / re-seat machinery (`sc_reseat_err`, `:1207`).
- **Mitigation AS CODE (UNBUILT) — single saturation-responder authority + explicit arbitration +
  hysteresis:** the adaptive responder and the governor compose under ONE arbitration rule, not two
  free-running loops. Both read the SAME `t_pair_ema` + `g_gpu_a_util` the governor already computes (no
  new signal, no new clock — reuse the F-thread's existing measurement):
  ```cpp
  // F-thread, at the pair boundary, AFTER gov_floor is computed (one authority orders the two levers):
  // ORDER: flow-res is the COARSE lever (cheapens the whole slice), the governor's work-shed is the FINE
  //   lever (trims optional passes). Escalate flow-res FIRST under deep saturation, shed SECOND.
  const bool deep_sat = (t_pair_ema > 1.30*pair_budget_ms) ||
                        (g_gpu_a_util.load() >= 0 && g_gpu_a_util.load() > (int)cfg.gov_util);
  if (deep_sat) { afs_hi_run++; afs_lo_run = 0; }
  else if (t_pair_ema < 0.70*pair_budget_ms) { afs_lo_run++; afs_hi_run = 0; }
  else { afs_hi_run = 0; afs_lo_run = 0; }
  // K-frame hysteresis + a per-level minimum dwell (mirror kTier5DwellPairs, :5707) so a brief spike
  // cannot thrash the re-init (which is expensive — R-AFS-1):
  uint32_t want_div = flow_div_live.load();
  if (afs_hi_run >= kAfsK && want_div < kAfsMaxDiv && afs_dwell_elapsed())  want_div <<= 1;  // 1→2(→4 behind a flag)
  if (afs_lo_run >= kAfsK && want_div > 1u        && afs_dwell_elapsed())  want_div >>= 1;
  // The governor's tier ladder is UNCHANGED; it simply has LESS to shed once flow-res has already
  //   cheapened the slice (the two now compose: coarse lever first, fine lever on the residual).
  ```
  The arbitration is: **flow-res escalates first (coarse), the governor sheds on the residual (fine);
  de-escalation is the reverse** (the governor restores work first, then flow-res restores resolution),
  with the K-frame run-length + dwell making the EXPENSIVE re-init rare. Thresholds (escalate `>1.30×` or
  `util>gov_util`; de-escalate `<0.70×`) are the design's §4 knobs and are deliberately conservative; the
  dwell mirrors `kTier5DwellPairs` (`:5707`) so a momentary calm cannot flap. STATED in-code at the
  responder site + here.
- **Verification (when built):** under a sustained BF6-combat load with BOTH `--load-governor` and the
  adaptive flag on, the `tier:N` + the flow_div trace show a SINGLE settled operating point (no >2 Hz
  pumping), and the `--pace-present` re-seat counter does not increment from the fight.
- **Status:** `open`. No arbitration code shipped; the single-authority + hysteresis design is stated
  above for the focused build.

### R-AFS-DOGMA — byte-identical-off (the C2 dogma / D-2)

- **Class:** dogma (byte-identical-off).
- **Failure mode:** the new adaptive flag's default path changes the presented bytes vs the current build
  — a silent regression of every user on the static `--flow-scale N` / `--flow-scale auto` path.
- **Mitigation AS CODE:** the flag is **UNBUILT this session** (the implementation is blocked on R-AFS-1).
  Therefore the static path is `mitigated`-by-construction in the strongest possible form: `parse_extra`
  (`main.cpp:1180-1213`) is **untouched**, `flow_div` (`:3246`) is still a single init constant read once,
  `WW_flow`/`WH_flow` (`:3250`) are still init constants, and **no runtime re-init machinery exists** —
  the static `--flow-scale N` path is literally the current bytes, character-for-character. When the flag
  IS built, the dogma contract is: a new `--flow-scale-adaptive` (default-off, in `parse_extra` to dodge
  the MSVC C1061 chain limit — verified the existing adaptive-flow knobs `--flow-scale-target-mp` `:1195`
  already live there), with EVERY runtime-re-init code path guarded by `if (cfg.flow_scale_adaptive)` so
  that with the flag absent: `flow_div` is set once and never re-read, the barrier atomics are never
  touched, the re-size routine is never called → the slice is byte-identical. The mandatory close:
  `--qdump`/`--outdump` diff = 0 vs the pre-change build with the flag absent (the C2 fixed point).
- **Verification:** with the flag UNBUILT (today) the static path is unchanged by inspection (no edit was
  made to `main.cpp`). When built: the `--qdump` diff=0 gate is MANDATORY before the lever's commit.
- **Status:** `mitigated` (by-construction: the flag is unbuilt, the static path is the current bytes).
  Re-affirm the diff=0 gate at build time.

---

## §3 — Acceptance log

No risk is `accepted` here. R-AFS-1/2/3 are `open` because the runtime lever is **not built** — they are
the engineering the focused build must perform, not residuals to wave through. R-AFS-DOGMA is
`mitigated`-by-construction (unbuilt flag). The supervisor closes risks only after the focused build ships
the mitigating code AND a rig-soak (BF6/Furmark) verifies R-AFS-1's device-loss-free + R-AFS-2's
assert-never-fires + R-AFS-3's no-pumping behavior first-hand.

---

## §4 — What WAS done this session (honest scope)

- **First-hand re-verification** of the design's §3 T2-architectural verdict against
  `apps/render_assistant/src/main.cpp` (the 3-thread model, the ~30-resource MV-grid coupling across A/FD/G,
  the absence of any quiesce barrier, the EMH host-pointer L3 window). The verdict holds; the coupling is
  wider than the design's "~10 sites" estimate.
- **This RISK_REGISTER** — the hazard map AS CODE (R-AFS-1/2/3 + R-AFS-DOGMA), each with the failure mode at
  the verified site, the mitigation the crash-safe build must implement, the verification gate, and an
  honest status.
- **No code change to `main.cpp`** — the static `--flow-scale N` / `--flow-scale auto` cost levers are
  untouched (the byte-identical-off invariant is held trivially). No racy re-init was shipped, per the
  directive.

## §5 — Links

- DESIGN (doubles as master plan + strategy at this stage):
  [`FG_ADAPTIVE_FLOWSCALE_DESIGN.md`](FG_ADAPTIVE_FLOWSCALE_DESIGN.md) (§3 the T2 re-classification, §4 the
  hysteretic controller, §5 the relation to `--flow-scale auto` + the governor).
- The governor it must arbitrate with: `--load-governor` (`main.cpp` `load_governor` / `gov_floor` /
  `g_gpu_a_util` / `t_pair_ema` / `kTier5DwellPairs`).
- The format of this register follows
  [`FG_HDR_PIPELINE_RISK_REGISTER.md`](FG_HDR_PIPELINE_RISK_REGISTER.md).
- Plan-tier protocol: [`../canon/PLAN_TIER_PROTOCOL.md`](../canon/PLAN_TIER_PROTOCOL.md) (§3 the no-commit-while-open gate).
- Temporal-window gate (the EMH re-import is L3): [`../training/TEMPORAL_WINDOW_GATE.md`](../training/TEMPORAL_WINDOW_GATE.md)
  + `AGENT_PROTOCOL.md §13`.
- Registration in [`../FORMAL_DOCUMENTS_REGISTER.md`](../FORMAL_DOCUMENTS_REGISTER.md) is the supervisor's
  job (not touched here — shared-file collision avoidance, the convention the sibling HDR register follows).
