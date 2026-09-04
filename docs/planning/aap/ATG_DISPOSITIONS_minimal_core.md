# ATG dispositions — `apps/minimal_fg` gap-audited against A0 (AAP §6.1, 2026-09-02)

> The single-design gap audit of the existing minimal core (no rivals; it stays *chosen, not
> analysed*). Two clean lenses (`ATG_GAP_AUDIT_minimal_core.md`: completeness-auditor and
> practitioner-realist, both `SCORED`) against the objective frozen BEFORE they ran. The supervisor's
> own sweep was sealed before any verdict (`SUPERVISOR_SEALED_SWEEP_minimal_core.md`, sha256
> `a90a61f2…5a24`). Status granted: **`gap-audited (2026-09-02)`**.

## 1. Sweep-vs-verdict diff (the supervisor's blind spots, measured)

| Judges' FATAL | In the sealed sweep? | First-hand re-verification |
|---|---|---|
| KC3: `ALL_COMMANDS` barriers on the hot path (`main.cpp:159–167`, called at 767–770 and per frame) | **NO — missed** | Verified 2026-09-02: `img_barrier()` uses `VK_PIPELINE_STAGE_ALL_COMMANDS_BIT` for both src and dst — the exact anti-pattern the June arc's §3 named as overhead cause #3 |
| KC3: host round-trip between stages (`make_host_buffer` 203–230, `map_copy`/`up_load` 1196–1216) | partially (sweep #7, "host-staged capture ring") | consistent with the sweep; the judges classify it as a stage round-trip, not only ingest |
| KC4: cannot express the shipping default; `wap_warp.comp` not compiled (`CMakeLists.txt:82–95`) | yes (sweep #5) | agrees |
| M4 veto: different core math (`optical_flow_warp.comp` via `record_optical_flow`, `main.cpp:801`) | yes (sweep #5) | agrees |
| M1/KC1: `t` hardcoded `0.5f` (`main.cpp:801`); no phase clock | yes (sweep #1) | agrees |
| M1/KC1: no replay record / no `--qdump` (`main.cpp:483–503`) | yes (sweep #6) | agrees |
| M2: the core/layer contract is absent (`seam_graph.hpp:88–93, 511–516` carry no such concept) | yes (sweep #5/#8) | agrees |

One FATAL class the supervisor did not see; the rest overlapped. The producer/judge separation earned
its cost once more.

## 2. Dispositions (every accepted finding gets one)

| Finding | Disposition | Where it is supplied |
|---|---|---|
| `ALL_COMMANDS` barriers (KC3) | **deferred → fixed in convergence stage C1** (adopt SG for ALL barriers in the core; `img_barrier()` retired; precise stage masks) | `CONVERGENCE_MASTER_PLAN` C1 |
| host round-trip capture (KC3) | **deferred → C2**: the donor's lock-free host-staged SPSC ring stays (the June-solved mechanism, BF6 deadlock lesson) but ingest→GPU is one import, never a stage round-trip; GPU-resident capture is the later opt-in | C2 |
| cannot express the default / different core math / no contract (KC4, M4, M2) | **deferred → C3**: `fg_core.comp` (the `wap_warp` core, LAYERTAB rows) replaces the pillar's `optical_flow_warp.comp` inside the `mc_interp` pass | C3 |
| `t = 0.5` hardcoded, no phase clock (M1/KC1) | **deferred → C4**: the donor's content clock (NCO+PLL, `present.cpp:1574/1975/2054–2059`), pair selection and per-tick `t_use` ported as the P-side of the base | C4 |
| no replay record (M1/KC1) | **deferred → S2.T1** (`--qdump+` on the donor first; the base inherits the writer at C4) | MOTION_TRUTH T1 |
| naive present, no pacing/drop (MR-1) · single queue (MR-2) | **deferred → C5** (MINIMAL_FG P3/P2, unchanged) | C5 |
| catalog coupling (sweep #4) | **deferred → S4.0 adoption**: the base is adopted into the repo and re-pointed at `framework/render/vulkan` (the vendored lineage) — operator decision | S4.0 |
| no quality layers; t=0.5 was P1's scope | **deliberate** (hello-world discipline, MINIMAL_FG §2) | — |

## 3. What this audit does NOT grant

No `analysed`, no release token. The minimal core is the BASE of the convergence because the operator
chose it (June, re-affirmed 2026-09-02) and because its SG engine survives the objective unchanged
(both lenses list it as salvage); its shortfalls above are the convergence plan's stage list, not a
verdict against the choice.
