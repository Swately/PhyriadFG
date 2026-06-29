# FG_PARALLEL_INSTANCES_RISK_REGISTER — D2 parallel FG instances (Tier-2)

Status: `0.1.0-experimental` · Type: **Reference (normative, Tier-2 annex)** · Tier: **2**
The risk-bearing leg of the D2 triad
([`FG_PARALLEL_INSTANCES_MASTER_PLAN.md`](FG_PARALLEL_INSTANCES_MASTER_PLAN.md) +
[`FG_PARALLEL_INSTANCES_IMPLEMENTATION_STRATEGIES.md`](FG_PARALLEL_INSTANCES_IMPLEMENTATION_STRATEGIES.md)).

> **HARD GATE ([PLAN_TIER_PROTOCOL §3](../canon/PLAN_TIER_PROTOCOL.md)).** No D2 change may be committed while
> ANY risk is `open`. **Supervisor reconciliation 2026-06-24 — the D2 UNBLOCKERS are BUILT + verified** (a
> byte-identical single-instance PREREQUISITE, NOT the multi-instance feature): **R-D2-5** (window-class
> collision) `mitigated` (per-instance class name, code-verified — Test 5: two concurrent `create()` succeed;
> suffix-0 = the bare historical name → N=1 byte-identical); **R-D2-6** (byte-identical-off) `mitigated` (the
> class name does not enter the output, the `device_lost()` latch is additive, N=1 unchanged; the C2 diff=0
> harness batched); **R-D2-3** (device-loss) `accepted (PARTIAL)` — the pillar-side `device_lost()` primitive is
> shipped byte-identical/additive, but the CONSUMER-side re-scoping (`vk_live`→per-instance quit in main.cpp) is
> the deferred **FgInstance** increment; the single-instance path is UNAFFECTED (a TDR behaves exactly as today)
> → this commit introduces NO device-loss risk; **R-D2-1 / R-D2-2 / R-D2-4** `deferred (N/A to the unblockers)` —
> they are the MULTI-INSTANCE risks (the race / present-authority / contention) and a 2nd instance is NOT
> enabled by these byte-identical unblockers; they govern the future FgInstance + P5 increment. **No risk `open`
> for the unblockers commit.** Built under the operator's §4-flexibility (coverage; inert single-instance); the
> full multi-instance FEATURE + its risks are the documented FgInstance increment.
>
> **Normativity (BCP 14)** and the honesty buckets as in the triad. All `file:line` standing facts were
> verified first-hand 2026-06-23 (anchor: [master-plan §6](FG_PARALLEL_INSTANCES_MASTER_PLAN.md)).
>
> **Reconcile, do not duplicate.** The cluster register
> [`PHYRIADFG_RISK_REGISTER.md`](PHYRIADFG_RISK_REGISTER.md) already carries R-D2-1..4 in summary form.
> This register is the **dedicated, deeper** D2 set: it restates those four with edit-site precision,
> **adds R-D2-5 (window-class collision — a first-hand finding the cluster did not name) and R-D2-6
> (byte-identical-off)**, and is the authority for the D2 build. When D2 is built, the cluster register's
> R-D2 rows SHOULD cross-link here rather than re-detail (single source of truth,
> [FDP §4.6](../canon/FORMAL_DOCUMENT_PROTOCOL.md)). This register also cross-links — does not duplicate —
> [`DEVICE_LOST_RECOVERY_RISK_REGISTER.md`](DEVICE_LOST_RECOVERY_RISK_REGISTER.md) (R-D2-3 generalizes it).

---

## §1 — The risk table

| ID | Class | Failure mode (+ `file:line` site) | Mitigation AS CODE (designed) | First-hand verification plan | Status |
|----|----|----|----|----|----|
| **R-D2-1** | concurrency | N instances race today's process-global singletons: `g_quit` (`main.cpp:2922`), the bridge ring, device/queue handles (`VDev` ~`main.cpp:1897`+), the gme host buffers, and the function-local-but-instance-wide `g_quit_threads` (`main.cpp:5066`) → data race / corruption / crash. | Every mutable singleton becomes `FgInstance`-owned; no shared MUTABLE state across instances. Only the **read-only** characterized catalog (+ the DComp/D3D device IF shared, under a documented contract) is shared. Per-instance quit replaces `g_quit`/`g_quit_threads`. (S1.) | ThreadSanitizer / Vulkan validation-layer clean across a 2-instance 60 s run; grep+audit that no `g_*` mutable global is written by >1 instance. | see header (supervisor-reconciled 2026-06-24) |
| **R-D2-2** | crash / dogma | Present-authority violation → SLI/CrossFire runt-frames. If two instances ever shared a present thread/surface the DCAD §2 one-present-authority invariant breaks. Today's surface is one `PresentSurface` (`main.cpp:6987`), `monitor_index=cfg.pres_mon` (`main.cpp:6977`); the same-thread create()/submit() contract is `PresentSurface.hpp:144-147`. | The pillar's same-thread create()/submit() contract makes a shared present thread structurally impossible: assert ONE `PresentSurface` + ONE present thread per `FgInstance` (the invariant held *per panel*). Each `create()` already owns its `Impl`/HWND/D3D device/DComp/WDA (`PresentSurface.cpp:264/304/315/401`) → independent once R-D2-5 is fixed. (S3.) | Runtime present-thread count == instance count; per-panel `MsBetweenDisplayChange` p99 jitter of A unchanged with B running (PresentMon CSV). | see header (supervisor-reconciled 2026-06-24) |
| **R-D2-3** | crash / device-loss | Device-loss is process-global: `vk_live()` sets the **global** `g_quit` on `VK_ERROR_DEVICE_LOST` (`main.cpp:2940-2943`). A TDR on instance B's GPU quits A too. | Route `vk_live()`'s loss to the **owning** `FgInstance`'s quit (thread-local instance ptr / pass-through), not `g_quit`; the surviving instance keeps running. **Resolution contract:** the thread-local instance pointer is set at each instance's thread entry (`spawn_instance`'s thread body), and `vk_live()` is only ever called from instance-owned threads (the per-instance F/P/warp threads) — so the TLS pointer always resolves to a live owner; a null TLS (an unexpected non-instance thread) falls back to the legacy global `g_quit` (fail-safe, no worse than today). [If the pass-through variant is chosen at build time instead of TLS, the owning `FgInstance*` is threaded through `vk_live()`'s call sites — deferred-to-build.] **Generalizes** the discharged device-loss mechanism to per-instance scope — does NOT redefine it (cross-link `DEVICE_LOST_RECOVERY_RISK_REGISTER`). (S1.) | Force a device-loss on one instance's device → that instance exits clean; the other continues ≥60 s, validation-layer clean. | see header (supervisor-reconciled 2026-06-24) |
| **R-D2-4** | dogma (perf) / concurrency | Resource contention silently degrades BOTH instances (no net benefit / A regresses when B runs). Two FG slices on a shared PCIe/DDR controller (dual host-bounce + iGPU convert) push each other past the present deadline. `--csv` is one global path (`main.cpp:681`, `:1117`, `:6956`) → no per-instance evidence. | Per-instance device/CPU partition via the wired router `select_participants` (`RoutingPolicy.hpp:48-74`, P5) + `phyriad::hw::pin_current_thread` (topology pillar, `f711102`); if an instance cannot clear its deadline on its partition it **DEGRADEs per-instance** (DCAD §2.4 make-space), it does NOT steal from A and does NOT cap a game (no-game-cap). Per-instance `TelemetryCsv` (S4) for attributable evidence. | The D2.f closeable test IS the discharge: two real games, two monitors, ≥60 s, **two per-instance CSVs**, A's slice/latency unchanged with B running vs A alone. | see header (supervisor-reconciled 2026-06-24) |
| **R-D2-5** | crash (create-failure) | **Window-class collision (first-hand finding, new to this triad).** `kOverlayClass = L"phyriad_present_overlay"` is a fixed constant (`PresentSurface.cpp:91`); `create()` calls `RegisterClassExW` unconditionally (`:280`) and `bail(SystemError)` on failure (`:281`). A second concurrent `PresentSurface::create()` re-registers the SAME class → `RegisterClassExW` returns 0 (`ERROR_CLASS_ALREADY_EXISTS`) → **the second instance cannot be created today.** This is a hard D2 blocker. | Per-instance UNIQUE class name (atomic/counter suffix `L"phyriad_present_overlay_%u"`); each `Impl` registers a distinct class and `destroy()` (`:257`) unregisters exactly its own → no shared lifetime, create/destroy balanced. (S2 — option 1.) **Pillar edit** → updates `docs/reference/present.md` same change (`check_doc_sync`). | Two `PresentSurface::create()` on two monitors both return a live surface; each `destroy()` unregisters its own class (no leak across repeated create/destroy); N=1 byte-identical (C2 harness). | see header (supervisor-reconciled 2026-06-24) |
| **R-D2-6** | dogma (byte-identical-off) | The `FgInstance` refactor changes the N=1 output byte stream (a reordered submit, a renamed window class affecting present, a changed default), breaking byte-identical-off. | N=1 with default flags MUST reduce to today's exact single-instance flow: the per-instance members default to today's values; `--instances`/target-spec is default-off (parsed in `parse_extra`, the C1061-dodging path like `--force-single-gpu` `main.cpp:1146`); the window-class name (R-D2-5) does not enter the output byte stream. (S0/S1/S2.) | The existing C2 byte-identical harness: FG-on N=1 vs the pre-refactor single-instance build, pixel/byte diff = 0. | see header (supervisor-reconciled 2026-06-24) |

---

## §2 — Linkage and lifecycle

- **Master-plan link.** Tier declared + register linked in
  [`FG_PARALLEL_INSTANCES_MASTER_PLAN.md`](FG_PARALLEL_INSTANCES_MASTER_PLAN.md) §3/§5.
- **Strategy link.** Each edit site in
  [`FG_PARALLEL_INSTANCES_IMPLEMENTATION_STRATEGIES.md`](FG_PARALLEL_INSTANCES_IMPLEMENTATION_STRATEGIES.md)
  §6 cites its risk ID.
- **Lifecycle ([PLAN_TIER_PROTOCOL §3](../canon/PLAN_TIER_PROTOCOL.md)).** Each risk starts `open`; mark
  `mitigated` ONLY after its verification gate is actually run first-hand (not assumed). A Tier-2 commit
  MUST NOT land while any row is `open`. A risk discovered during the build is ADDED here (living register).
- **Register entry.** This document + its two siblings MUST be added to
  [`FORMAL_DOCUMENTS_REGISTER.md`](../FORMAL_DOCUMENTS_REGISTER.md) (the supervisor applies that — this
  triad does not touch the shared register).

## §3 — Honest residuals (what is NOT yet closeable here)

- **R-D2-4's contention magnitude is unmeasured** until D2-4 on real hardware; the floor (shared
  controller saturation → per-instance degrade) is named, not beaten (master-plan §1). Do not record
  R-D2-4 `mitigated` on design alone — its discharge is the runtime D2.f evidence.
- **R-D2-2's runtime flip-independence** is `designed` (structurally independent post-R-D2-5); the
  two-saturating-instance flip phase behavior is a runtime observation, not yet measured.
- **P5 dependency.** R-D2-4's measured partition assumes P5 wires the router; that is P5's own gate,
  inherited, not re-verified here.
