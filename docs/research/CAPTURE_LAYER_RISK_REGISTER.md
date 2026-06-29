# CAPTURE_LAYER_RISK_REGISTER — the owned DDA capture layer (`CaptureSurface`)

> **Diátaxis type:** Risk register (the Tier-2 document of [`PLAN_TIER_PROTOCOL.md`](../canon/PLAN_TIER_PROTOCOL.md)).
> **Status:** `designed` — **no capture-layer code is built under this plan; every risk starts `open`** and
> is tagged **`DESIGN — not yet built`**. A risk reaches `mitigated` ONLY when its code is in **and** its
> Verification was run first-hand; `accepted` records a residual + the rationale + who accepted it.
> **Linked set (Tier-2 triad):** master plan [`CAPTURE_LAYER_MASTER_PLAN.md`](CAPTURE_LAYER_MASTER_PLAN.md) ·
> strategies [`CAPTURE_LAYER_IMPLEMENTATION_STRATEGIES.md`](CAPTURE_LAYER_IMPLEMENTATION_STRATEGIES.md) ·
> this register. Cited foundation: [`MINIMAL_FG_SOTA_DOSSIER.md`](MINIMAL_FG_SOTA_DOSSIER.md) §6.1.
> **Component IDs** (canonical, from master-plan §0.2): CL-1 DDA acquire · CL-2 lock-free SPSC ring ·
> CL-3 zero-copy · CL-4 composed-flip forcing · CL-5 the `CaptureSurface` seam · CL-6 backend select.
> **Provenance:** the 2026-06-26 capture-rate SOTA (workflow `wvfhoot01`, 23/26 confirmed) — the failure
> modes are the SOTA's `DDA-FM-1..8` re-expressed as CR-IDs + the dogma/system-state risks the build opens.

## The gate (PLAN_TIER_PROTOCOL §3)

1. **No commit of capture-layer code while any CR is `open`** (each → `mitigated` or `accepted`).
2. **Byte-identical-off (CR-6)** is a release gate: `--capture-backend` default `wgc` → the existing WGC
   default path is untouched; a run with the default must be byte-identical to the prior `minimal_fg`
   baseline before the DDA backend is committed.
3. **No-game-cap (CR-7)** is a dogma gate: zero frame-limit / affinity / priority call against the game PID.
4. **No keyed-mutex on the hot path (CR-3)** is a crash-class gate (the BF6 deadlock): the ingest path is
   verified — by grep + by a saturation soak — to contain no `AcquireSync`/`ReleaseSync` on the per-frame
   path.
5. **Global-state restore (CR-8)** is a gate on the composed-flip fallback (CL-4): any DWM/registry mutation
   is saved before + restored on exit AND on crash, and is opt-in default-off.
6. **Build-state caveat:** the Verification cells are **pending build / pending rig** — specified, not run.
   The operator's BF6 run (CP2) is the runtime measurement that, together with my first-hand build-green +
   validation-clean check, discharges CR-1/CR-2/CR-3/CR-6 for CP1.

---

## CP1 build status (2026-06-26d — built, NOT yet committed; supervisor-verified first-hand)

CP1 (CL-1+CL-2+CL-6, the DDA host-staged backend) is **BUILT in `apps/minimal_fg/src/main.cpp`** (uncommitted)
and verified first-hand this pass: build green (`ninja: no work to do` on re-invocation); a bounded smoke
run shows `capture-backend = dda`, `DuplicateOutput on the OWNING adapter (reuse-d3d=yes)`, the pull thread
live (FINITE 8 ms, NO keyed mutex), the instrument line (`fps in=.. | dda_arrived=.. ring_dropped=.. dda_timeouts=..`),
and the correct static-desktop behaviour (`in≈0`, `dda_timeouts≈120/s` — DDA is event-driven; CR-2).

| CR | CP1 status | basis |
|---|---|---|
| **CR-2** | `mitigated` | `AcquireNextFrame(8u)` finite; smoke-run shows the timeout streak incrementing correctly on a static screen. |
| **CR-3** | `mitigated` | grep + code read: the capture/ingest path has ZERO `AcquireSync`/`ReleaseSync`/`KEYEDMUTEX` (plain `CopyResource`); keyed-mutex lives ONLY on the pre-existing VK→D3D present-bridge. *(Runtime saturation soak = the operator's BF6 run.)* |
| **CR-4** | `mitigated` | `init_dda` enumerates adapter→output, matches `DXGI_OUTPUT_DESC.Monitor`, creates/reuses the device on the owning adapter + sets `SetMultithreadProtected(TRUE)` on the shared immediate context; smoke-run confirms `reuse-d3d=yes`. |
| **CR-6** | `mitigated` | `--capture-backend` default `wgc`; the DDA branch is gated; the WGC producer + consume loop are functionally unchanged (git-diff = comments only); structural inertness verified. *(Full CSV byte-diff = at the commit gate.)* |
| **CR-7** | `mitigated` | the capture thread issues zero game-PID affinity/priority/limit; ring-full DROPS (no back-pressure). *(Game-frametime evidence deferred like THREAD_PROTECTION R2.)* |
| **CR-1** | `open` | recreate code present (`dda_recreate`, ACCESS_LOST/DEVICE_REMOVED handlers) but the runtime path is **not yet forced** — verification = induce a UAC/mode-change during DDA capture. |
| **CR-9** | `open` | `UNSUPPORTED` at `DuplicateOutput` → WGC fallback is handled; the all-black/protected mid-stream skip is **not** implemented yet. |
| **CR-10** | `open` | the Independent-Flip / refresh regime is characterized by the **CP2 operator BF6 run** (the `in`-rate read), not yet taken. |
| **CR-5** | `open` | CP3 (zero-copy) — not built; CL-2 ships independently. |
| **CR-8** | `open` | CP4 (composed-flip forcing) — not built; conditional on CP2-b. |

**Commit scoping:** a CP1 commit is gated by the CRs **CP1 engages** — CR-1, CR-2, CR-3, CR-4, CR-6, CR-7,
CR-9, CR-10. Of these, **CR-1, CR-9, CR-10 remain `open`** → **CP1 is NOT yet committable**: it is ready to
**TEST** (the operator's BF6 run = CP2, which discharges CR-10 and is the natural moment to also force CR-1
and decide CR-9). CR-5 (CP3) and CR-8 (CP4) are not engaged by CP1 code and do not gate its commit (the
MINIMAL_FG SG-engine commit-scoping precedent). Push remains the operator's call regardless.

---

## Risk table (CR-1 … CR-10 — design record; CP1 status above)

| ID | Class | Failure mode | Mitigation — AS CODE (`DESIGN — not yet built`) | Verification (pending build / rig) | Status |
|---|---|---|---|---|---|
| **CR-1** | crash / device-loss | `DuplicateOutput`/`AcquireNextFrame` → `DXGI_ERROR_ACCESS_LOST` (mode/resolution change, secure desktop / UAC / lock screen / Ctrl-Alt-Del, fullscreen transition, fast-user-switch, GPU TDR) **or** `DXGI_ERROR_DEVICE_REMOVED` / output hotplug — the duplication interface is invalidated; naive code crashes or hangs. | On `ACCESS_LOST`: `ReleaseFrame`, release the `IDXGIOutputDuplication`, **recreate** it (re-`EnumOutputs`→`DuplicateOutput1`) under a bounded backoff loop; keep emitting the last-good ring frame until re-armed. On `DEVICE_REMOVED`/`NOT_FOUND`: re-enumerate adapters+outputs, rebind to the owning adapter, recreate device+dup; a watchdog so a vanished output cannot hang the capture thread. Terminal loss → `device_lost()` latch + clean quit (reconcile `vk_live` from `DEVICE_LOST_RECOVERY`). | Force a mode change + a UAC prompt during capture → no crash, dup recreated, capture resumes; kill the source mid-capture → clean exit ≤ ~2 s (the existing watchdog). | `open` |
| **CR-2** | correctness / liveness | `AcquireNextFrame` blocks or spins: an INFINITE timeout hangs the thread; or a static screen / Independent-Flip yields `WAIT_TIMEOUT` forever and the code errors out. | **Finite timeout (8-16 ms), NEVER INFINITE.** On `WAIT_TIMEOUT`: not an error — reuse the last published frame, increment a no-new-frame counter, continue. A sustained timeout streak is the **runtime Independent-Flip signal** → surface it (and, if CL-4 enabled, escalate to the composed-flip probe). | Static-screen run → timeouts counted, no error, no busy-spin (thread yields); the streak counter reads non-zero and is logged. | `open` |
| **CR-3** | concurrency / crash (THE crash class) | Keyed-mutex deadlock regression on the ingest hot path: `D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX` + `IDXGIKeyedMutex::AcquireSync(INFINITE)` — a GPU-starved holder never reaches `ReleaseSync` → the consumer's `AcquireSync` never returns (the BF6 deadlock, dossier §6.1.1). | **Forbid keyed-mutex on the hot path by construction.** CL-2 host-staged = the CPU memcpy is the only cross-API barrier (no shared lock). CL-3 zero-copy = a **shared TIMELINE semaphore** (`VK_KHR_external_semaphore` ↔ D3D fence): producer signals value N, consumer waits N — wait/signal back-pressures, never CPU-stall-deadlocks. If an external keyed-mutex surface is ever unavoidable: finite `AcquireSync` timeout + copy-to-intermediate + recreate-on-`WAIT_ABANDONED`, never INFINITE. | `grep` proves zero `AcquireSync`/`ReleaseSync`/`KEYEDMUTEX` on the capture+ingest path; a 30 s BF6/saturation soak runs without the stall (the experiment that proved the host-staged WGC fix). | `open` |
| **CR-4** | concurrency / cross-GPU (multi-GPU ownership) | `DuplicateOutput` on an adapter that does NOT own the target output → `E_INVALIDARG`/`UNSUPPORTED` (the Microsoft-Hybrid dGPU case, `render_assistant` main.cpp:3184). | Enumerate `IDXGIAdapter`→`IDXGIOutput`, match `DXGI_OUTPUT_DESC.Monitor` to the target output, create the D3D11 device + the duplication **on THAT adapter** (here the 4090 drives the 240 Hz monitor). Do processing on another GPU only via the CL-3 NT-handle shared surface — never duplicate on a non-owning adapter. Fall back to WGC (CR-6) on `UNSUPPORTED`. | Confirm the duplication is created on the output-owner; on an induced mismatch the code re-binds to the owner (or falls back to WGC) instead of failing. | `open` |
| **CR-5** | concurrency (CL-3 zero-copy follow-up) | The shared timeline-semaphore cross-API sync is a new concurrency class: a missed signal/wait, a slot reused while the consumer still holds it, or hot-path allocation of shared surfaces. | A ring of **N pre-imported** shared NT-handle surfaces, provisioned at INIT (never on the hot path); SPSC: producer writes slot w + signals value w, consumer waits value r ≤ w; the import (`CreateSharedHandle`→`vkImportMemory` / `vkImportSemaphore`) happens ONCE at create. Stays separate from CL-2 → **CL-2 ships independently** of this risk. | Validation-clean (`VK_LAYER_KHRONOS_validation`, sync-val) across a 30 s soak; a trace confirms one producer/one consumer per surface and zero hot-path allocation. | `open` |
| **CR-6** | dogma / byte-identical-off | The DDA backend is a NEW path coexisting with the WGC default; it could change the default's behaviour/output even when not selected. | `--capture-backend` default `wgc`; the DDA init/thread is entered ONLY when `dda` is selected → core-not-selected runs **zero** new code (assertable). The new flag is parsed in `parse_extra` (NOT the C1061 main chain). The WGC `FrameArrived` producer is untouched. | A WGC-default run is byte-identical to the prior baseline (`in`/`out`/`ring_dropped`/present-count identical); symbol/trace inertness of the DDA path; `lint_hal` clean (default `seq_cst`, no raw `memory_order_*` in `apps/`). | `open` |
| **CR-7** | dogma / NO-GAME-CAP | The capture shares the GPU with the game; a back-pressuring capture or any frame-limit / affinity / priority call against the game PID is an implicit cap. | DDA captures the **output**, never the game process; it issues **zero** `set_process_affinity`/`set_process_priority`/frame-limit against the game PID; under load the **present side drops** (it never stalls the source). The capture thread is our own thread (priority changes, if any, apply to OURS, never the game's). | `grep` proves zero foreign-PID affinity/priority/limit calls on the capture path; the game's own frametime under our DDA vs off (the operator's in-situ check, deferrable like THREAD_PROTECTION R2). | `open` |
| **CR-8** | dogma / system-state mutation (CL-4) | Composed-flip forcing via `OverlayTestMode`=5 mutates a **global** DWM registry key (`HKLM\SOFTWARE\Microsoft\Windows\Dwm`) affecting the WHOLE desktop, and adds +1 frame latency; a crash could leave the desktop with MPO disabled. | **Prefer the least-invasive method:** a 1×1 click-through topmost composited overlay reasserted ~500 ms forces the captured surface through composition **without** any registry change → try this FIRST. Use `OverlayTestMode` ONLY if the overlay alone fails: **save** the prior value, set, and **restore on exit AND on crash** (a guard/`atexit`/SEH). Opt-in, **default-off**; the +1 frame is characterized (PresentMon), not silently absorbed. | Enable CL-4 → MPO state is captured before, restored after a normal exit AND a forced kill; the overlay-only path forces composition with no registry write; the +1 frame is recorded. | `open` |
| **CR-9** | external-capture / protected content | DRM/HDCP-protected surfaces on the duplicated output → `DXGI_ERROR_UNSUPPORTED` or black/blank frames. | Detect the all-black/`UNSUPPORTED` case → skip the frame, surface a diagnostic, continue (do not crash). Not expected for BF6 gameplay; handled defensively. | An induced protected surface (or the `UNSUPPORTED` HRESULT) → the capture skips + logs, does not crash. | `open` |
| **CR-10** | external ceiling (characterize, not absorb) | The no-injection **refresh ceiling** + the **Independent-Flip bypass** are presented as defects to engineer away, or silently absorbed as if fixed. | Treat both as **documented external ceilings** (dossier §6.1.1), characterized by the CP2 measurement, not regressions we introduce. The runtime timeout-streak (CR-2) surfaces the Independent-Flip regime LOUD; present-hook (the only >refresh path) stays REJECTED for BF6 (AC). The +1-frame CL-4 cost is recorded when engaged. | CP2's PresentMon/`in`-rate record characterizes the regime; no silent claim that DDA "fixed the rate" without the measurement. | `open` |

## Residual / accepted (none yet)

To be filled at each commit gate. As of design time **all ten are `open`** (no code built). CR-7's
game-frametime evidence is expected to carry the same operator-deferred residual the THREAD_PROTECTION close
did (R2); CR-10 is a documented-ceiling acceptance once CP2 characterizes the regime.

---

## Parent reconciliation (link, do not re-derive)

| Parent register | What it already discharges (reused here) |
|---|---|
| [`MINIMAL_FG_RISK_REGISTER.md`](MINIMAL_FG_RISK_REGISTER.md) | byte-identical-off (MR-4 → CR-6), no-game-cap (MR-5 → CR-7), the own-window present ceiling (MR-7), the operator-gated default-flip pattern. |
| [`DEVICE_LOST_RECOVERY_RISK_REGISTER.md`](DEVICE_LOST_RECOVERY_RISK_REGISTER.md) | `vk_live` graceful-exit on `VK_ERROR_DEVICE_LOST` → `g_quit` (the device-loss backstop CR-1 reuses for terminal loss). |
| [`FG_SATURATION_STABILITY_RISK_REGISTER.md`](FG_SATURATION_STABILITY_RISK_REGISTER.md) | the host-staged lock-free SPSC ring (the proven pattern CL-2 retargets; the keyed-mutex-avoidance precedent CR-3 enforces). |
| [`THREAD_PROTECTION_RISK_REGISTER.md`](THREAD_PROTECTION_RISK_REGISTER.md) | the no-game-cap discipline (zero foreign-PID affinity/priority calls, verified first-hand; closed `45fb505`) — CR-7 reuses it. |

> **Honest gap:** the parent registers' internal commit hashes are quoted from those registers, not re-run
> via `git` this authoring pass; the implementer re-confirms first-hand when closing CR-1/CR-3.

## Registration

This register + its two companions are added to
[`../FORMAL_DOCUMENTS_REGISTER.md`](../FORMAL_DOCUMENTS_REGISTER.md) in the same change (handled by the
supervisor at the CP0 close).
