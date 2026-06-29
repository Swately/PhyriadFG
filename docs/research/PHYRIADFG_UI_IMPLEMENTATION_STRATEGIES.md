# PHYRIADFG_UI_IMPLEMENTATION_STRATEGIES — the buildable how-to for the launcher

> **Diátaxis type:** How-to (the concrete build approach for each phase of
> [`PHYRIADFG_UI_MASTER_PLAN.md`](PHYRIADFG_UI_MASTER_PLAN.md)). **Status:** `designed` — code anchors are first-hand
> recon-verified; the Win32/ImGui mechanics are standard-API designs, not yet built. **Plan tier:** Tier-1 companion.

## S0 — The rename (P0)

Mechanical, contained to render_assistant + FG docs (master-plan §2). Sequence:
1. `apps/render_assistant/src/telemetry_csv.hpp`: `namespace ayama` → `namespace phyriadfg` (L29 + its closing brace);
   the `"AyamaFG"` comment L1 + CSV schema header L263 → `"PhyriadFG"`; `ayama_frame_index`/`ayama_*` column names →
   `phyriadfg_*`. Grep `ayama::` in render_assistant to catch any qualified use of the renamed namespace.
2. `apps/render_assistant/src/main.cpp`: the `"AyamaFG"` strings at 2873/2885/3517/6873/8210/8448 → `"PhyriadFG"`.
3. Build-confirm (`_build_ra.bat`) → run 5 s on Star Rail → byte-identical behaviour (only banner text changed).
4. Docs: `AyamaFG`→`PhyriadFG` in `docs/planning/FG_*.md` + `LSFG_PROPOSAL_BRIEF*.md`; `git mv
   AYAMAFG_ARCHITECTURE_GAP_AUDIT.md PHYRIADFG_ARCHITECTURE_GAP_AUDIT.md`; fix the inbound links in
   `FORMAL_DOCUMENTS_REGISTER.md` + `FG_ARCHITECTURE_DCAD_MASTER_PLAN.md` §11.
5. Gates: render_assistant builds + runs; `check_inventory` + `lint_docs` green; commit by pathspec.

## S1 — Scaffold the target (P1)

- **Location:** `apps/render_assistant/ui/` (keeps it adjacent to its only purpose) or a sibling `apps/phyriadfg_ui/`.
  Add `add_subdirectory` under the `PHYRIAD_BUILD_RENDER` gate (which already builds `imgui_lib`+GLFW).
- **Target:** `add_executable(phyriadfg-ui WIN32 main.cpp UiState.cpp panels/*.cpp)`;
  `target_link_libraries(phyriadfg-ui PRIVATE imgui_lib glfw phyriad_topology)` (+ the `framework/ui` lib if we reuse
  `Application::run()`; for the MVP a thin direct GLFW+ImGui loop is fine and avoids the graph DSL coupling).
- **Skeleton:** GLFW window + ImGui OpenGL3 backend + a docking root; one empty "PhyriadFG" window that opens/closes. No
  FG coupling. This is the build-feasibility proof.

## S2 — Spawn + argv composition (P2)

- **Window enumeration:** `EnumWindows` → filter visible top-level with a non-empty `GetWindowTextW` → an ImGui combo;
  selected title substring → `--window`. Monitor path: enumerate via DXGI `IDXGIFactory::EnumAdapters`/outputs → `--cap-mon`.
- **argv builder:** a `std::vector<std::string>` assembled from the panel state; join to a command line for `CreateProcessW`.
  Keep a single source-of-truth `UiState → argv` function so every panel just edits `UiState`.
- **Spawn:** `CreateProcessW("render_assistant.exe", cmdline, …, &si, &pi)` with `si.hStdOutput`/`hStdError` set to the
  write end of a `CreatePipe` (and `bInheritHandles=TRUE`, `STARTF_USESTDHANDLES`). Stop = `TerminateProcess`/`WM_CLOSE`
  on the FG HWND (graceful: the FG already exits cleanly on its quit path / window death — STAGE P0 device-lost).
- **Resolve the exe path** next to the launcher (same build output dir) — no hard-coded absolute path.

## S3 — Telemetry readback (P3)

- A reader thread drains the pipe's read end line-by-line; parse the documented `[ra] … fps (present) | … lat …ms |
  frz …/s | … gpu(A:…%)` fields with a tolerant tokenizer (the line is stable, fields are ` | `-separated `key value`).
- Push parsed samples into a lock-free ring the UI thread reads each frame (mirror the FG's own telemetry-ring
  discipline). Render fps/latency/freeze/util + a `ImGui::PlotLines` frame-time sparkline.
- Deep metrics: pass `--csv %TEMP%\phyriadfg_run.csv`; tail the companion `-stats.csv` for 1% lows/percentiles on demand
  (read-only file poll; the FG flushes ~4 Hz).

## S4 — The quality↔latency knob (P4)

Implement master-plan §5 as a single function `compose_quality(int slider01_100) → flags`. Three anchor presets
(Lowest-latency / Balanced[default] / Highest-quality) with the slider interpolating the discrete-valued flags
(`--ingest-backlog`, `--flow-scale`, `--fg-factor`) to the nearest legal step. `--nvofa` is a separate checkbox
(capability-gated: only enabled if the active GPU advertises `VK_NV_optical_flow` — query once at startup, or just let the
FG auto-fall-back and show a tooltip). Changing the knob while running = stop + relaunch (the FG reads flags at startup;
live re-tune is a later FG feature, not an MVP requirement).

## S5 — Affinity tab (P5)

- Read the topology once via `phyriad::hw::probe()` → render a read-only summary (total cores, P/E split, V-Cache CCD,
  NUMA nodes) using the `HardwareTopology` accessors.
- A single checkbox "Pin FG threads" → adds `--pin` to argv. (The FG already self-selects the cores via
  `optimal_producer_consumer_pair`; the UI just toggles the behaviour — no core-by-core picker in the MVP.)
- **Growth (not MVP):** the per-game affinity section would talk to `ayama-agent` (its SHM/command path) to set the
  *game process's* affinity — gated behind master-plan §8.3b operator approval; its own plan.

## S6 — Polish + eye (P6)

Layout, docking defaults, a dark theme matching the FG's aesthetic, a single-screen "happy path" (pick game → Auto →
Start). **First eye-gated step** — hold for the operator at the monitor.

## Build/verify gates (every phase)

- `_build_ra.bat` green (the launcher builds under the same MSVC/Ninja toolchain as render_assistant).
- The FG runs **unchanged** when launched by the UI vs by hand (the UI only composes the same argv).
- `check_inventory` (the new `.cpp`/`.hpp` files indexed) + `lint_docs` + `lint_hal` green before each commit.
- No new third-party dependency (GLFW + ImGui + topology are all already in-tree).
