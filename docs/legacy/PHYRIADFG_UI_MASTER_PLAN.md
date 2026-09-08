# PHYRIADFG_UI_MASTER_PLAN — a standalone config launcher for the frame generator (FG-first, affinity-optional)

> **Diátaxis type:** Explanation + plan (a design/master-plan, not a how-to). **Status:** `designed` — no UI code
> written yet; the in-repo foundation anchors (`file:line`, the reusable pillars) are **first-hand recon-verified**
> (4-agent workflow `wf_4fbf0832-af2`, read-only). **Plan tier:** **Tier-1** (substantial, needs finer orchestration;
> NOT risk-bearing — the launcher is a *separate process* that only spawns `render_assistant.exe` with flags and reads
> its telemetry; it does not touch the FG hot path, has no crash/concurrency/data-loss surface in the FG, and stakes no
> dogma). Companion: [`PHYRIADFG_UI_IMPLEMENTATION_STRATEGIES.md`](PHYRIADFG_UI_IMPLEMENTATION_STRATEGIES.md).
> **Verifiable-reference mandate (FDP):** every `file:line` and every "already in the repo" claim below was re-read
> first-hand this pass; nothing about the UI's *runtime behaviour* is claimed as measured (there is no UI to measure).

## §0 — Why now (the pivot)

Every remaining **performance** lever on the frame generator is **eye-blocked**: the operator is remote and cannot run
the gameplay A/B that gates the NVOFA bidir-collapse (the net-slice-win lever), the SAD calibration, and the P3/P4/P6
"feel" knobs. The FG itself is committed + pushed (`origin/main` @ `6ed7534`). So we advance the one substantial track
that needs **no eye until the very end**: a UI to drive the FG. This is also the natural home for the operator's second
idea — folding **per-game CPU affinity** (the original Ayama's domain) into the FG, *optionally*.

**The operator's framing, kept honest:** "lo que más brilla de Ayama es el FG." The UI foregrounds the FG; affinity is a
secondary, opt-in capability — not co-equal billing.

## §0.1 — Product positioning (DECIDED 2026-06-20)

The operator settled the standalone-vs-suite question. The model:

- **Phyriad = the private substrate** — the repo where the protocols live and the LLM operates; a personal tool that
  **does not seek public adoption** (`CLAUDE.md` stance). It stays private; it is the *engine room*, not a product.
- **PhyriadFG = a RESULT of Phyriad, with its OWN separate identity** — its own name, version line, changelog, and a
  one-line promise (external-capture frame generation with measured multi-GPU routing). Described as "built on the
  Phyriad substrate" (an implementation detail), **never** as "a feature OF Phyriad" or "one of Ayama's utilities."

**Three axes, deliberately decoupled** (the market-research verdict — workflow `wf_0f8d2c10-392`, 5 agents incl. an
adversarial steelman; the direct comparable **Lossless Scaling** is a single standalone app under one identity, and
**Sysinternals/RTSS** prove a tool keeps its own brand while sharing a substrate and being optionally bundled):

| axis | decision | why |
|---|---|---|
| **code / substrate** (invisible) | **shared** — one `framework/*` engine, one updater under the hood | DRY + the efficiency mandate; a substrate fix lands once |
| **identity / brand** (user-facing) | **PhyriadFG's own** — name, version, changelog, one-line promise | focus is what gets adopted; LSFG/Magpie/Sysinternals all win on it |
| **distribution** | the **same binary both ways** — standalone download AND optionally inside an "Ayama" bundle; the bundle is additive, **never a gate** (no login/telemetry precondition) | the suite players got punished for gating (GeForce Experience login) — a lean standalone advertises that AS its differentiator |

**Honest caveat (not hype):** the "if it becomes popular" framing is a hypothetical future — Phyriad is `0.1.0-experimental`
and most tools never go viral. This decision is taken because the recommended structure is **free now and correct either
way** (clean separation + reusable engine is also the right call for a personal tool), NOT because popularity is assumed.
We do **not** build go-to-market machinery (Steam page, installer, suite bundler) prematurely — today's decision is name +
structure only. (Recorded per the operator's request.)

## §1 — Headline: a separate native ImGui/GLFW launcher, NOT an in-process overlay

**Decision: build `phyriadfg-ui` as a standalone Win32 + Dear ImGui application** that (1) picks a target window/monitor,
(2) drives the FG's quality↔latency knob and a few high-value toggles, (3) spawns `render_assistant.exe` with the composed
flags, (4) reads its `stdout`/`--csv` telemetry into a live panel, (5) can foreground the FG, and (6) optionally enables
CPU affinity (`--pin`, with room to grow into per-game affinity via the topology pillar / `ayama-agent`).

**Why standalone, not in-process overlay** (recon-verified rationale):
- `render_assistant`'s present loop is tightly coupled to the **DirectComposition** overlay HWND + the FG GPU pipeline.
  Injecting ImGui would need a second swapchain/shader path **on the FG device**, adding latency and defeating the
  zero-overhead mandate. Both prior GUIs in the repo (`ayama-ui`, `danzig`) chose **separate binaries** for this reason.
- A separate launcher keeps `render_assistant` a pure, autonomously-runnable CLI tool (its current contract) — the UI is
  a *driver*, not a dependency. The FG can still be run headless / by an orchestrator.
- Rejected alternatives: **system-tray** (hides the live knobs the operator needs visible — tray suits background
  daemons, not a foreground interactive tool); **web/localhost** (adds an HTTP server + JSON serialization to a
  native-first C++ project, and gives no native thread-affinity control).

## §2 — The rename (AyamaFG → PhyriadFG): LOW difficulty, contained to the FG

The recon found **two distinct products** the operator must not conflate (one recon agent did):

| | what it is | "Ayama" footprint | in the rename? |
|---|---|---|---|
| **`apps/ayama/`** | the original **Ayama = a CPU runtime-optimizer app** (8 libs, 5 exes incl. its own `ayama-ui` + `ayama-agent` daemon + Windows service `AyamaAgent` + per-game memory/policy) | ~12,518 occurrences, 24 CMake targets, 43 namespaces, a Windows service | **NO** — untouched |
| **`apps/render_assistant/`** | **AyamaFG = the frame generator** (this arc's product) | ~6 code spots + ~20 doc refs + 1 doc filename | **YES** — this is the rename |

**The operator is renaming the FG, which is tiny.** Concrete scope (first-hand):
- **Code (render_assistant):** the `"AyamaFG"` product-name strings — `telemetry_csv.hpp` L1 (comment) + L263 (CSV
  schema header); `main.cpp` 2873/2885/3517/6873/8210/8448 (banner/error strings). Pure string edits.
- **One structural bit:** `namespace ayama` in `telemetry_csv.hpp:29` → `namespace phyriadfg` (render_assistant-local;
  the only non-string change; low risk, a single TU's namespace). `ayama_frame_index`/`ayama_*` CSV column names →
  `phyriadfg_*` (a schema-header change; the `--csv` consumer is our own scripts).
- **Docs:** `AyamaFG` → `PhyriadFG` in `docs/planning/FG_*.md` bodies + `LSFG_PROPOSAL_BRIEF*.md` (string replace);
  `git mv docs/planning/AYAMAFG_ARCHITECTURE_GAP_AUDIT.md → PHYRIADFG_ARCHITECTURE_GAP_AUDIT.md` + update its 1 entry +
  inbound links in `FORMAL_DOCUMENTS_REGISTER.md` (line ~182) and `FG_ARCHITECTURE_DCAD_MASTER_PLAN.md` (§11 companion link).
- **NOT touched:** `render_assistant.exe` (already neutrally named — no binary rename), the `apps/ayama/` optimizer app,
  its CMake targets, its Windows service, its namespaces. **No build-target surgery.**
- **Effort: ~30–60 min**, mostly mechanical, one build to confirm the namespace edit. **No collision** with the optimizer
  (the two names become cleanly distinct — exactly the decoupling the operator wants).
- **Why do it:** the FG "salió del rumbo de lo que alguna vez quiso ser Ayama" — the brand should match. Recommended;
  awaiting the operator's green light (a branding decision is his). The UI work below is named `phyriadfg-ui` on that basis.

## §3 — Foundation already in the repo (reuse, do not reinvent)

- **UI pillar** `framework/ui/` — `Application::run()` (GLFW init → window → ImGui setup → backend OpenGL3/Vulkan →
  graph → main loop), `RenderNode<State...>` (draws ImGui per tick), `UIThreadNode` (GLFW input source), `ProfileKind`
  {LATENCY,BALANCED,POWER}. Reference: `docs/reference/ui.md`.
- **GLFW 3.4 + ImGui v1.91.5-docking** already vendored in root `CMakeLists.txt:557-617`, built into `imgui_lib` when
  `PHYRIAD_BUILD_RENDER` (the render_assistant build already turns this on).
- **The proven standalone pattern** — `apps/ayama/tools/ayama-ui/` (GLFW + ImGui + `RenderNode<AyamaAppState>` +
  `AyamaTrayIcon` + multi-panel dashboard, talks to its daemon over SHM). We mirror its *structure*, not its IPC.
- **Affinity substrate** `framework/topology/` — `HardwareTopology` (V-Cache/NUMA/CCX/E-core detection) + `hw::`
  (`pin_current_thread`, `elevate_thread_rt`, `set_thread_affinity`, `set_thread_ideal_processor`,
  `optimal_producer_consumer_pair`). render_assistant already consumes it via `--pin` (default OFF;
  `main.cpp` 5028/5216/6788-6790).
- **The FG's control surface** — `render_assistant` is CLI-only (~60 flags via `parse_extra`, no config file). The UI's
  whole job on the FG side is *compose argv + spawn + read telemetry*. (Full flag inventory: this plan §5 + the recon.)
- **Telemetry already emitted** — the `[ra] … fps … | …` stats line (stdout, ~1/s) + the `--csv` per-present export
  (`telemetry_csv.hpp`, lock-free ring + drain thread) + `-stats.csv` (1% lows, percentiles, stutter).

## §4 — The UI design (panels)

A single resizable window, docked panels (ImGui docking is vendored). **FG is the hero; affinity is one optional tab.**

1. **Target** — window picker (`EnumWindows`+`GetWindowText` → dropdown → `--window SUBSTR`) or monitor picker (DXGI DD
   → `--cap-mon`); capture-API note. A "▶ Start / ■ Stop" control + a "Bring FG to front" button (`SetForegroundWindow`).
2. **Quality ↔ Latency** (the hero control) — ONE slider, plus an "Auto" default. Maps to a *coordinated* flag set, not a
   single flag (§5). Secondary: `--nvofa` checkbox (HW OFA, NVIDIA-only, auto-detected), GPU-routing dropdown
   (`--fg-gpu auto|primary|assist`, `--force-single-gpu`).
3. **Live** — telemetry panel: present fps, source fps, **added latency**, freeze/s, FG multiplier (uniq/source), 4090
   util, tier; a small frame-time sparkline. Fed by the spawned process's stdout/`--csv` (§6).
4. **CPU Affinity (optional tab)** — a checkbox "Pin FG threads (hybrid-CPU / NUMA)" → `--pin`; a read-only topology
   summary from the topology pillar (P/E cores, V-Cache CCD). **Growth path** (operator-steered): a "per-game affinity"
   section that drives the existing `ayama-agent` to set the *game's* affinity — see §8.
5. **Expert (collapsed)** — raw access to the advanced toggles (the ~50 `--no-*`/feature flags) for power use; default
   hidden so it never competes with the hero knob.

**Foreground-FG:** after spawn, the launcher can `SetForegroundWindow` the FG's DComp HWND (or stay behind). The FG window
is already a borderless overlay; the launcher is the control plane.

## §5 — The quality↔latency knob (the flag composition — the design's heart)

The slider is **not** a binary mode switch; it's a continuous dial composed onto several real-time-meaningful flags. The
honest mapping (from the recon's flag inventory; all are existing, measured-OFF-byte-identical or measured defaults):

| slider position | intent | composed flags |
|---|---|---|
| **Lowest latency** | freshest, lightest | `--ingest-backlog 1` (the input-lag floor, STAGE-111), `--flow-scale 2` (¼ flow cost), `--nvofa` if available, `--fg-factor 2` |
| **Balanced (default/Auto)** | the field-proven stack | `--ingest-backlog 3` (byte-identical), `--flow-scale auto --flow-scale-target-mp 2.1`, `--nvofa` if available |
| **Highest quality** | richest interpolation | `--flow-scale 1` (full-res flow), `--fg-factor` higher, the full default feature stack |

`--flow-scale auto`/`--flow-scale-target-mp` is explicitly the future-UI quality slider (it was built + marked for this).
The slider's exact breakpoints are a **tuning question that needs the operator's eye** (deferred) — the *mechanism* (UI →
argv composition) does not. The UI ships with the Balanced/Auto default and the slider exposed but documented as
"perceptual, tune to taste."

## §6 — Telemetry readback (no new FG code needed)

The launcher spawns `render_assistant.exe` with `stdout` redirected to a pipe (`CreateProcess` + `STARTUPINFO` handles) and
parses the `[ra] …` stats lines in real time (a tiny line parser for the documented fields). For deeper metrics it passes
`--csv <tmp>` and tails the `-stats.csv` (or the raw CSV's last rows). The FG side is **lock-free + already emits this** —
the UI is a pure consumer; zero coordination, zero FG changes. (If a richer live feed is later wanted, a shared-memory
stats block mirroring `ayama-agent`'s SHM seqlock is the growth path — not needed for the MVP.)

## §7 — Implementation phases (Tier-1 orchestration)

- **P0 — Rename (prerequisite, ~30-60 min, operator-greenlit).** Execute §2; build-confirm the `namespace` edit;
  `git mv` the doc + register fix. Gate: render_assistant builds + runs byte-identical; `check_inventory`/`lint_docs` green.
- **P1 — Scaffold.** New `apps/render_assistant/ui/` (or `apps/phyriadfg_ui/`) CMake target linking `imgui_lib` + GLFW +
  the `framework/ui` + `framework/topology` pillars; an empty docked ImGui window that opens + closes cleanly. Gate: builds
  + runs, no FG coupling.
- **P2 — Spawn + compose.** The Target panel (window/monitor enumeration) + argv composition + `CreateProcess` spawn/stop
  of `render_assistant.exe`. Gate: launches the FG on a chosen window; stop is clean.
- **P3 — Telemetry panel.** Pipe-read the stdout stats line → live panel (fps/latency/freeze/util). Gate: numbers update
  live and match the console.
- **P4 — The quality↔latency knob + nvofa/GPU routing** (§5). Gate: slider re-launches/relaunches the FG with the composed
  flags; Auto is the default.
- **P5 — Affinity tab.** `--pin` checkbox + topology summary. Gate: toggling `--pin` is reflected in the spawned process.
- **P6 — Polish + the operator's eye.** Layout/UX/visuals — **this is the first eye-gated step** (deferred until he's back).

P0-P5 need **no eye**. P6 (and the §5 slider tuning) do.

## §8 — Open product decisions (need the operator's steer)

1. **Rename green light?** Recommended (§2). Names the UI `phyriadfg-ui`.
2. **Standalone vs extend `ayama-ui`?** Recommendation: **standalone** `phyriadfg-ui` (the FG is its own product now; coupling
   to the optimizer's UI re-entangles the brand we're decoupling). `ayama-ui` is the *pattern* to copy, not the host.
3. **Affinity depth?** (a) MVP = just `--pin` (FG-thread pinning; self-contained, no daemon). (b) Growth = drive
   `ayama-agent` to set the *game's* per-game affinity (the original Ayama capability) — bigger, pulls in the agent/SHM/
   service. Recommendation: ship (a) in the MVP, design (b) as a follow-up so "afinidad de juegos + FG" is real but optional.
4. **Slider breakpoints** (§5) — perceptual, deferred to his eye.

## §9 — What is NOT claimed / honest gaps

- No UI runtime behaviour is measured (no UI exists yet) — all "feels"/latency claims about the slider await P6 + his eye.
- The `CreateProcess` stdout-pipe read + `--csv` tail are standard Win32 + file I/O (designed, not yet built).
- The per-game-affinity growth path (§8.3b) is **scoped, not designed** — it would get its own plan if greenlit.
- Build feasibility is high-confidence (GLFW/ImGui/topology/ui all already build here) but unproven until P1 compiles.
