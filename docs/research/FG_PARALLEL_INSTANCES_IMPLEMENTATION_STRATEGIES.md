# FG_PARALLEL_INSTANCES_IMPLEMENTATION_STRATEGIES — D2 per-strategy edit sites

Status: `0.1.0-experimental` · Type: **How-to / strategy (Reference + Explanation)** · Tier: **2**
Companion to [`FG_PARALLEL_INSTANCES_MASTER_PLAN.md`](FG_PARALLEL_INSTANCES_MASTER_PLAN.md) and the
mandatory [`FG_PARALLEL_INSTANCES_RISK_REGISTER.md`](FG_PARALLEL_INSTANCES_RISK_REGISTER.md).

> **Normativity (BCP 14)** as in the master plan. **Honesty:** everything below is `designed`; the
> `file:line` standing facts were verified first-hand 2026-06-23 (anchor:
> [master plan §6](FG_PARALLEL_INSTANCES_MASTER_PLAN.md)). Every edit site that exists to mitigate a
> Tier-2 risk **cites the risk ID** ([PLAN_TIER_PROTOCOL §2.3](../canon/PLAN_TIER_PROTOCOL.md)).
>
> **No duplication.** The cluster's [`PHYRIADFG_IMPLEMENTATION_STRATEGIES.md`](PHYRIADFG_IMPLEMENTATION_STRATEGIES.md)
> §D2 carries the *summary* four-pillar strategy. This document is the *deeper, edit-site-level* form; it
> does not re-state the cluster's rationale ([FDP §4.6](../canon/FORMAL_DOCUMENT_PROTOCOL.md)).

---

## §0 — Discipline that binds every strategy below

- **Byte-identical-off (dogma).** N=1, default flags MUST produce bit-identical output to today's build.
  The regression harness is the existing C2 byte-identical comparison (FG-on vs the same single-instance
  path). Any `FgInstance` refactor that changes the N=1 byte stream is a defect. (R-D2-1, R-D2-6.)
- **No-game-cap (operator provenance).** No strategy here caps a base game. Device/CPU partition and
  per-instance degrade are the make-space levers; capping a game is never one.
- **Pillar-faithful routing (D-12).** The device partition uses the real `select_participants` /
  `break_even_decide` (P5 wired), never a fresh hardcode.
- **New flags default-off.** `--instances` / per-target spec (S1) ships default-off; absent it, the app is
  exactly today's single instance.

---

## §S1 — `FgInstance` encapsulation (master-plan pillar 1) · mitigates R-D2-1, R-D2-3, R-D2-6

**Goal.** Collapse the single-instance process-global state into one `FgInstance` struct, then instantiate
N. The single-instance body is reused verbatim as the per-instance body.

**Edit sites (first-hand standing).**
- **The quit flags.** `static volatile bool g_quit` (`main.cpp:2922`) and the function-local
  `std::atomic<bool> g_quit_threads{false}` (`main.cpp:5066`) become **per-instance members**
  (`FgInstance::quit`). Both the `while(!g_quit&&!g_quit_threads.load())` thread **loops**
  (`main.cpp:5115`, `:6442`, `:6525`, `:8034`; plus the `while(!g_quit)` join at `:9127`) and the
  per-present `if(!g_quit&&!g_quit_threads.load())` present-one-frame **guards** (`main.cpp:8761`,
  `:9028` — these are `if(...)` guards, NOT loops) read their OWNING instance's flag. (R-D2-1.)
- **The device-loss path.** `vk_live()` sets the global `g_quit` (`main.cpp:2940-2943`); re-scope it to set
  the **owning instance's** quit so a loss collapses only that instance (R-D2-3). The detector is a free
  function today — it must learn its instance (thread-local instance pointer, or pass-through), without
  redefining the discharged device-loss mechanism (generalize, do not redefine —
  [`DEVICE_LOST_RECOVERY_RISK_REGISTER.md`](DEVICE_LOST_RECOVERY_RISK_REGISTER.md)).
- **The shared device/queue/bridge singletons** (the `VDev` handles around `main.cpp:1897`+, the bridge
  ring, the gme host buffers) become `FgInstance`-owned. The ONLY shared object is the read-only
  characterized GPU catalog + (if shared) the DComp/D3D device under a documented contract (R-D2-1).
- **The launch site.** Today the body runs once after device setup; wrap it as
  `for (auto& tgt : targets) instances.emplace_back(spawn_instance(tgt, shared_catalog));` behind a
  default-off `--instances`/target-spec parse (added in `parse_extra`, NOT the main `if/else` chain — the
  MSVC C1061 nesting limit, the same constraint that put `--force-single-gpu` in `parse_extra` at
  `main.cpp:1146`).

**Validation.** N=1 byte-identical (C2 harness, diff = 0); ThreadSanitizer / Vulkan validation-layer clean
across a 2-instance 60 s run; grep-audit that no `g_*` mutable global is written by >1 instance (R-D2-1).

---

## §S2 — Per-instance window class (master-plan §4 prerequisite) · mitigates R-D2-5

**Goal.** Remove the process-global window-class collision so a second `PresentSurface::create()` succeeds.

**Edit site (first-hand standing).** `kOverlayClass = L"phyriad_present_overlay"`
([`PresentSurface.cpp:91`](../../framework/render/present/src/PresentSurface.cpp)) is a fixed constant;
`RegisterClassExW(&wc)` is unconditional in `create()` (`PresentSurface.cpp:280`) with `bail(SystemError)`
on failure (`:281`); `destroy()` unregisters (`:257`). Two concurrent instances → the second
`RegisterClassExW` returns `ERROR_CLASS_ALREADY_EXISTS` → bail.

**Mitigation AS CODE (two options — pick the simplest that holds the pillar's contract).**
1. **Per-instance unique class name** — derive the class name with a per-surface counter/atomic suffix
   (`L"phyriad_present_overlay_%u"`), so each `Impl` registers a distinct class and `destroy()`
   unregisters exactly its own. Keeps create/destroy symmetric; no shared state.
2. **Tolerate already-registered** — treat `ERROR_CLASS_ALREADY_EXISTS` as success and skip
   `UnregisterClassW` for non-owners (refcount the class). More shared state; rejected unless option 1
   has a cost, because it reintroduces a process-global lifetime the pillar otherwise avoids.

**Pick: option 1** (per-instance unique class) — it preserves the pillar's "each `PresentSurface` owns its
window state" property with zero new shared state. This is a **pillar edit** (`PresentSurface.cpp`), so it
MUST update [`docs/reference/present.md`](../reference/present.md) in the same change (`check_doc_sync` CI).

**Validation.** Two `create()` on two monitors both return a live surface; each `destroy()` unregisters
its own class (no leak — `RegisterClassExW`/`UnregisterClassW` balanced per instance); N=1 byte-identical
(the single-instance class name is irrelevant to the output byte stream — verify via the C2 harness). (R-D2-5.)

---

## §S3 — Per-instance present authority + multi-surface DComp (pillars 2–3) · mitigates R-D2-2

**Goal.** Each `FgInstance` owns one `PresentSurface` on its own monitor, one present thread.

**Edit sites (first-hand standing).** Today: one `PresentSurface::create(psd)` at `main.cpp:6987` with
`psd.monitor_index=cfg.pres_mon` (`main.cpp:6977`). Move this into `FgInstance` so each instance sets its
own `monitor_index` from its target spec. The pillar's threading contract
([`PresentSurface.hpp:144-147`](../../framework/render/present/include/phyriad/render/present/PresentSurface.hpp))
already forces create()+submit() on the same thread → each instance's present thread is the authority for
its panel (R-D2-2). No code makes two instances share a present thread; the refactor MUST keep it that way.

**Multi-surface DComp independence (the cluster's open FLAG, now resolved first-hand).** Each `create()`
allocates its own `Impl` (`PresentSurface.cpp:264`), HWND (`:304`), D3D11 device (`:315`), DComp chain,
and WDA per-HWND (`SetWindowDisplayAffinity(impl->hwnd, ...)`, `:401`). After S2 removes the only shared
process-global (the class name), N surfaces are structurally independent. No further pillar edit is needed
for independence beyond S2 — the FLAG is **resolved: independent by construction once S2 lands** (R-D2-2).

**Validation.** Runtime present-thread count == instance count; A's per-panel `MsBetweenDisplayChange`
p99 jitter unchanged with B running vs A alone (PresentMon CSV per panel). (R-D2-2.)

---

## §S4 — Per-instance telemetry (`--csv`) · mitigates R-D2-4 (evidence) + enables D2.f

**Goal.** Produce the **two** per-instance CSVs the closeable test (vista D2.f) needs.

**Edit site (first-hand standing).** `cfg.csv_path` is one global string (`main.cpp:681`, parsed `:1117`)
feeding one `TelemetryCsv tcsv` (`main.cpp:6956`, `tcsv.start(...)` `:6960`, per-present `tcsv.push(row)`
`:8499`/`:8770`). Move `TelemetryCsv` into `FgInstance`; template the path per instance
(`<path>-inst<N>.csv` / `<path>-inst<N>-stats.csv`). `row.route_device` (`main.cpp:8506`, today hardcoded
`0`) records the instance's routed device (from S5's partition) so the two CSVs are attributable.

**Validation.** Two real games on two monitors ≥60 s → two CSV pairs; A's slice/latency in its CSV is
unchanged with B running vs A alone (the no-cross-interference evidence — R-D2-4). This IS the D2.f test.

---

## §S5 — Device / CPU partition via the wired router (pillar 4) · mitigates R-D2-4

**Goal.** Partition the measured device set across instances; pin each instance's CPU threads disjointly.

**Edit sites (first-hand standing).** Device selection is identity-branched at `main.cpp:3212-3213`
(`deviceType` + name-substring `afrag`), with the hard-exit at `main.cpp:3235` and `--force-single-gpu`
at `main.cpp:1146`. After **P5** wires the router, replace the per-instance device choice with
`select_participants(catalog, n, arithmetic_intensity)`
([`RoutingPolicy.hpp:48-74`](../../framework/holons/include_gpu/phyriad/holons/RoutingPolicy.hpp)) over the
**measured** catalog, partitioned so instances do not double-book a device beyond its measured headroom.
Pin each instance's F/P threads to a disjoint CCX/core set via `phyriad::hw::pin_current_thread` (the
topology pillar, consumed for `--pin` at `f711102`). (R-D2-4.)

**Honest scope (the §1 floor — do NOT plan to beat it).** When the measured partition cannot give both
instances enough (shared PCIe/DDR controller saturated), the strategy is **per-instance degrade** (DCAD
§2.4 make-space, applied per instance), NOT stealing from A and NOT capping a game. D2's win is
independence, not free throughput (master-plan §1 floor). (R-D2-4.)

**Dependency.** S5 assumes P5 is wired (its own plan — `PHYRIADFG` cluster E1a/P5). Until P5, D2 can ship
D2-0..D2-3 (encapsulation + present authority + device-loss scoping) on the existing identity-branch device
choice, but the *measured* partition (the contention guarantee) waits on P5.

---

## §6 — Strategy → risk traceability (every edit cites its risk)

| Strategy | Risk(s) mitigated | Edit-site anchor |
|---|---|---|
| S1 `FgInstance` encapsulation | R-D2-1, R-D2-3, R-D2-6 | `main.cpp:2922`, `:5066`, `:2940-2943`, `:1897`, the loop sites (`:5115`/`:6442`/`:6525`/`:8034`) + present guards (`:8761`/`:9028`) |
| S2 per-instance window class | **R-D2-5** | `PresentSurface.cpp:91`, `:280-281`, `:257` |
| S3 present authority + DComp | R-D2-2 | `main.cpp:6977`, `:6987`; `PresentSurface.{hpp:144-147, cpp:264/304/315/401}` |
| S4 per-instance `--csv` | R-D2-4 (evidence) | `main.cpp:681`, `:1117`, `:6956-6960`, `:8499`/`:8770`, `:8506` |
| S5 device/CPU partition (P5) | R-D2-4 | `main.cpp:3212-3213`, `:3235`, `:1146`; `RoutingPolicy.hpp:48-74`; `f711102` |

The register ([`FG_PARALLEL_INSTANCES_RISK_REGISTER.md`](FG_PARALLEL_INSTANCES_RISK_REGISTER.md)) holds
each risk's mitigation-AS-CODE + first-hand verification plan + status.
