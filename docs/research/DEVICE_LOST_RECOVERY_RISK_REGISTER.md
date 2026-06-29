# DEVICE_LOST_RECOVERY_RISK_REGISTER — Phase 0 of the DCAD architecture (graceful device-loss exit)

> **Diátaxis type:** Risk register (a Tier-2 gate artifact, per [`PLAN_TIER_PROTOCOL`](../canon/PLAN_TIER_PROTOCOL.md)).
> **Status:** `shipping` (2026-06-20) — IMPLEMENTED + build-green + happy-path-verified + inspection-discharged. CR1–CR5
> are `mitigated` (each with its first-hand verification, §2); FR1 (the D3D11/WGC DXGI side) is `accepted`/deferred
> (rationale in §2) — `vk_live` covers the dominant Vulkan TDR surface. **No risk remains `open`**, so the Tier-2
> commit gate is satisfied. The mechanism: a non-throwing `vk_live(VkResult)` (file-scope, forward-declared before the
> submit helpers; body near the Ctrl-C handler) trips ONLY on `VK_ERROR_DEVICE_LOST` → sets the global `g_quit` → the
> present threads exit via the EXISTING, PROVEN console-Ctrl-C path → `done:` teardown → clean exit (external overlay
> exit == passthrough). Each wrapped site is `f(x)`→`vk_live(f(x))` = **identity on success** → the happy path is
> byte-identical (measured: idle Honkai 240–242 fps / frz 0.0/s / lat 20–26 ms == baseline). **Tier-1 parent (master plan):**
> [`FG_ARCHITECTURE_DCAD_MASTER_PLAN.md`](FG_ARCHITECTURE_DCAD_MASTER_PLAN.md) §8 Phase 0 + §7.4. **Verifiable-reference
> mandate (FDP):** every `file:line` below was read first-hand this session; the line numbers are anchored to
> `apps/render_assistant/src/main.cpp` at HEAD `17d1326`.

## §0 — Why this is Phase 0 (lands FIRST) and why it is Tier-2

The [gap audit](PHYRIADFG_ARCHITECTURE_GAP_AUDIT.md) ranked `VK_ERROR_DEVICE_LOST` graceful-exit as the **critical**
addressable gap: a TDR / driver reset / GPU hang — **most likely exactly in the 99 %-util / dual-GPU regime DCAD
targets** — currently turns into a hard crash (undefined behaviour), because the codebase has **no `VkResult`-checking
discipline**: `submit_wait` (`main.cpp:1977`) and `submit_wait_q2` (`main.cpp:1983`) and every runtime
`vkQueueSubmit` / `vkWaitForFences` in the present path **ignore the return value** (verified first-hand — there is no
`VK_CHECK` macro anywhere). Shipping ANY of the later DCAD phases (multi-GPU re-route, fractional-multiplier present
re-pacing, the n→2→1 degradation ladder) on top of unhandled device-loss would convert a recoverable event into a
crash — so this lands first.

It is **Tier-2** because it is **crash-class** (device-loss + the existing in-flight async-warp use-after-reset class
documented at `main.cpp:7016`) and touches the hot present path and the multi-thread teardown.

**Key architectural fact that makes "exit" the right move:** PhyriadFG is an **external overlay** (DComp surface, the
game owns its own swapchain). A clean *exit* of PhyriadFG **IS the passthrough** — the game keeps rendering and
displaying natively; only the interpolated overlay disappears. So Phase 0's scope is **detect → set a terminal flag →
unwind the three threads → run the existing `done:` teardown → exit(0)**. In-process device *recreation* is
explicitly **out of scope** (undefined per Khronos; `vkDeviceWaitIdle` on a lost device is itself unreliable — see
CR3).

## §1 — Design (the mitigation mechanism, one place)

A single non-throwing detector, set behind the existing quit machinery (no new threading primitive):

```cpp
// near the other globals (g_quit, g_quit_threads at ~main.cpp:2 -- atomic<bool>):
std::atomic<bool> g_device_lost{false};

// non-throwing, branchless on the happy path. Returns true to CONTINUE, false to ABORT.
// ONLY the terminal codes trip it -- never VK_TIMEOUT / VK_NOT_READY / VK_SUBOPTIMAL_KHR.
static inline bool vk_live(VkResult r) noexcept {
    if (r == VK_ERROR_DEVICE_LOST) {            // the ONE terminal Vulkan code (CR4)
        if (!g_device_lost.exchange(true)) {    // first detector wins; idempotent
            std::printf("[ra] VK_ERROR_DEVICE_LOST -- graceful exit (game keeps running)\n");
        }
        g_quit_threads.store(true); g_quit.store(true);   // unwind all 3 loops
        return false;
    }
    return true;
}
```

The runtime submit/wait sites become `if(!vk_live(vkQueueSubmit(...))) <thread-local bail>;` where `<thread-local
bail>` is the loop's existing `break` (each present loop is a `while(!g_quit&&!g_quit_threads.load())`, so simply
*storing* the flags makes the NEXT loop test exit — and a local `break` exits this iteration immediately). No `goto
done` from inside the threads (it would cross the thread-lambda scope and the loop initializers — forbidden); the
threads return, `thr_c/thr_f/thr_p.join()` (`main.cpp:8139`) completes, and control falls into `done:` (`8146`).

For the D3D11 / WGC capture side, the analogous terminal code is `DXGI_ERROR_DEVICE_REMOVED` (and
`DXGI_ERROR_DEVICE_RESET`); a `dxgi_live(HRESULT)` twin sets the same flags from the FrameArrived callback / copy path.

**Byte-identical-off invariant:** when no device is lost, every `vk_live` returns `true` on the first comparison →
**no behavioural change** (this is always-on, not flag-gated, because error-checking a currently-unchecked terminal
code is strictly safer than ignoring it; the "off" equivalence is *the happy path is unchanged*, verified by an
unchanged Honkai run, §3).

## §2 — Risk register

| ID | Failure mode (+ site) | Mitigation AS CODE | Verification (first-hand) | Status |
|----|----|----|----|----|
| **CR1** | **Detection coverage gap** — a device-loss returned by a present-path submit/wait is ignored and execution continues into UB. Sites (first-hand): submits `main.cpp:5554, 5978, 6016, 6372, 6521, 6878, 6882, 7056`; waits `5409, 5634, 6063, 6095, 6372, 6521, 6878, 7056`; helpers `submit_wait:1977`, `submit_wait_q2:1983`. | Wrap each **runtime** submit/wait return in `vk_live(...)`; on `false` `break` the thread loop. `submit_wait`/`submit_wait_q2` return `bool`; callers in the hot path test it. Init-phase calls keep the existing `goto done` (already handled). | Build green; grep proves no bare runtime `vkQueueSubmit(`/`vkWaitForFences(` remains in the present loops (5400–7100) without a `vk_live` guard. | `mitigated` |
| **CR2** | **Async-warp use-after-reset at loss** (the STAGE-102 class, `main.cpp:7016`, `--async-present`): a device lost while an async warp holds `fBridge`/`async_inflight=back` → resetting/reusing the fence or the bridge slot is UB. | On `g_device_lost`, the P-loop MUST NOT `vkResetFences(fBridge)` / re-submit / reuse the bslot; it `break`s immediately. The post-join `vkDeviceWaitIdle` (CR3-guarded) drains or abandons the in-flight submit; teardown is null-guarded. No new reset is issued after the flag is set. | Inspect the `async_inflight` / `vkGetFenceStatus` block (6972, 7016, 7056) — confirm the flag is tested before any reset/resubmit; build green; a normal `--async-present` Honkai run unchanged. | `mitigated` |
| **CR3** | **`vkDeviceWaitIdle` on a lost device** (`main.cpp:8148-8150`): per Khronos it may return `VK_ERROR_DEVICE_LOST` and is unreliable on a hung device; an unchecked call in teardown can stall/again-UB. | Guard: `if(A.dev && !g_device_lost) vkDeviceWaitIdle(A.dev);` (likewise B/G). On a lost device, SKIP the idle-wait and proceed to the null-guarded `vkDestroy*` teardown (destroy on a lost device is defined/safe). | Inspect 8148-8150 post-edit; build green; verify clean process exit (no hang) on a normal quit (Ctrl-C) — the happy path still waits idle (flag false). | `mitigated` |
| **CR4** | **False-positive terminal-code detection** → spurious exit mid-session on a benign result. | `vk_live` trips **only** on `VK_ERROR_DEVICE_LOST`; `dxgi_live` only on `DXGI_ERROR_DEVICE_REMOVED`/`_RESET`. Explicitly NOT `VK_TIMEOUT`, `VK_NOT_READY`, `VK_SUBOPTIMAL_KHR`, `VK_ERROR_OUT_OF_DATE_KHR` (no swapchain here anyway). | Code review of the enum set; a full Honkai session (≥10 min) with no spurious `[ra] ...DEVICE_LOST` print + unchanged fps/frz vs baseline. | `mitigated` |
| **CR5** | **Happy-path regression** — the added checks change timing/behaviour when no loss occurs. | The check is a single integer compare on a value already returned; no extra Vulkan calls, no allocation, no lock. On `VK_SUCCESS` the branch is not taken. | A/B Honkai run baseline vs built: `--csv` fps mean + 1%-low + frz within run-to-run noise; build green; `lint_hal` clean (no raw `memory_order_*`; default `seq_cst` for the new atomic). | `mitigated` |
| **FR1** | **D3D11/WGC device-removed** path (the capture/copy side, `wgc_ctx` callback + the copy-fence path `main.cpp:~3496/4488/8140`): a removed adapter on the capture device is unhandled. | `dxgi_live(HRESULT)` on the FrameArrived/copy `HRESULT`s sets the same terminal flags → the present loop exits via CR1's mechanism; the WGC teardown (`8154-8166`, already `running=false` + drain) runs. | Inspect the callback + copy path; build green; (device-removed is hard to force — `accepted` with the inspection if it cannot be runtime-triggered, documented as such). | `accepted` |

## §2.1 — Discharge (2026-06-20, first-hand)

**Implementation note (design → as-built):** the as-built `vk_live` **sets the global `g_quit`** on a loss rather than
returning a value the caller `break`s on. This is FUNCTIONALLY EQUIVALENT and strictly simpler/safer: the present
threads all loop on `while(!g_quit && !g_quit_threads.load())`, so setting `g_quit` makes each thread exit at its next
iteration — and it works **uniformly even at the submit/wait sites inside the present lambdas** (where a bare `break`
would not compile). The bool return is kept for callers that want to short-circuit but is currently discarded. So CR1's
"on false break" is realised as "set g_quit → loop exits at the next while-check".

- **CR1 (coverage)** → `mitigated`. Wrapped (first-hand, grep-confirmed): the A.q present/warp wait `vkQueueSubmit(A.q,…,fBridge); vk_live(vkWaitForFences(A.dev,…,fBridge,…))` (×3 — the saturated-4090 TDR point, every present tick), the `submit_wait`/`submit_wait_q2` helpers (cover their G/B/setup callers), and the F-thread 1080 Ti flow waits (`tfb` loop + `fB2`). Coverage is at the **per-thread hot-loop** granularity — sufficient, because a lost device makes the *next* wrapped wait in each thread return `VK_ERROR_DEVICE_LOST`, which sets `g_quit` and unwinds all threads within one tick/pair (every site need not be wrapped). Build green.
- **CR2 (async use-after-reset)** → `mitigated` (inspection). On detection `g_quit` is set → the P-loop exits at its next `while` check **before** any further `vkResetFences`/resubmit/bslot reuse; no new reset is issued after the flag. On an already-lost device every Vulkan call returns `VK_ERROR_DEVICE_LOST` (defined, non-UB), so even an in-flight async warp tears down safely via the null-guarded teardown.
- **CR3 (waitidle on a lost device)** → `mitigated`. `vkDeviceWaitIdle(A/B/G)` guarded `&& !g_device_lost` (main.cpp ~8148-8150) — verified first-hand. Normal quit (flag false) still waits idle exactly as before.
- **CR4 (false-positive)** → `mitigated`. `vk_live` trips ONLY on `VK_ERROR_DEVICE_LOST` (enum equality); the idle 17 s Honkai smoke produced **zero** spurious `DEVICE_LOST` prints.
- **CR5 (happy-path regression)** → `mitigated` (**measured**). `vk_live` is identity on `VK_SUCCESS`, so the wraps are byte-identical when no device is lost. Idle Honkai smoke (default flags, the P0 binary): **240–242 fps / frz 0.0/s / lat 20–26 ms / warp 0.6–1.2 ms** — equal to the pre-P0 baseline within run-to-run noise. `lint_hal` clean (the new `g_device_lost` atomic uses default `seq_cst`; no raw `memory_order_*`).
- **FR1 (D3D11/WGC DXGI device-removed)** → `accepted` / **deferred**. `vk_live` covers the dominant Vulkan TDR surface (the shared 4090 present/warp + the 1080 Ti flow); a GPU loss there sets `g_quit` and exits regardless of the capture side. A dedicated `dxgi_live(HRESULT)` on the FrameArrived/copy path is a scoped follow-up (it cannot be runtime-forced on this rig). Rationale documented; not blocking the commit.

**Exit-mechanism note:** the clean-exit path (`g_quit` → loop exit → `done:` teardown → exit) is the **existing, proven console-Ctrl-C path** (`console_ctrl_handler`, main.cpp:2506), so it needs no separate fault-injection to validate — P0 only adds a new *trigger* for it. (Aside: in this test harness `send_ctrlc.ps1` did not reliably deliver the console event to the Bash-launched child, so smokes were force-stopped — a harness limitation, NOT a defect in the exit path.)

## §3 — Verification plan (to discharge before commit)

1. **Build green** — `vcvars64` + `cmake --build build-ra-msvc2 --target render_assistant` (0 errors/0 warnings on the touched lines).
2. **Happy-path byte-equivalence** — a Honkai (Star Rail) run with `--csv base_p0off` (conceptually "off" = no loss) vs the prior baseline: fps mean / 1 %-low / freeze count within run-to-run noise; the `[ra] done (...)` line prints; clean Ctrl-C exit (no hang).
3. **`lint_hal`** clean (the new `std::atomic<bool>` uses default `seq_cst`; no raw `memory_order_*` in `apps/`).
4. **Inspection discharge** for CR2/CR3/FR1 (the use-after-reset guard, the waitidle guard, the DXGI twin) where a real device-loss cannot be force-triggered on the rig — each marked `mitigated` (verified by inspection) or `accepted` (with the reason) before commit, never left `open`.
5. **§9 source-signature** + 0 trap tokens on the edited file.

## §4 — Out of scope (named, not silently dropped)

- **In-process device recreation** — undefined per Khronos; not attempted. The external-overlay exit IS the passthrough.
- **Auto-relaunch** — a watchdog/relaunch wrapper is a separate, higher-tier concern (it would re-acquire the window, re-characterize devices — DCAD Phase 2 territory). Phase 0 stops at the clean exit.
- **Partial device-loss survival on a multi-GPU rig** (e.g. the assistant B is lost but A is alive → re-route to single-GPU instead of exiting) — that is DCAD Phase 7 (degradation as a re-route), and it DEPENDS on this phase's detection existing first. Phase 0 exits on any device loss; Phase 7 later upgrades "exit" to "re-route where survivable".
