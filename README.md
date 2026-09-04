# PhyriadFG

**External-capture, multi-GPU frame generation.** PhyriadFG runs *outside* the game: it captures the
already-rendered output, estimates the motion between real frames, synthesizes intermediate frames, and
presents them — with no access to the game engine, its motion vectors, or its depth. One executable, one
build.

> `0.4.0-experimental` · Windows · student-built, LLM-assisted.

[![Discord](https://img.shields.io/badge/Discord-join%20the%20server-5865F2?logo=discord&logoColor=white)](https://discord.gg/x6ZsjVchs)
Questions, testing feedback, and release announcements: **[discord.gg/x6ZsjVchs](https://discord.gg/x6ZsjVchs)**

## What it is

- A frame generator that works on **any** game because it only sees the final image (like Lossless
  Scaling Frame Generation), not an in-engine FG.
- **Multi-GPU by design**: capture + convert on the integrated GPU, optical flow on a second GPU, warp
  and present on the primary GPU — each role on the device where its work pays off. It degrades to a
  single GPU with `--force-single-gpu`.
- A quality stack (occlusion / disocclusion handling, a global motion model, edge-gated picks, content-
  clock pacing) layered on a backward-warp + blend core.
- A Tauri launcher UI under `ui/` for picking the target window and toggling flags.

## Build

Requires **Windows**, **[MSVC Build Tools](https://aka.ms/vs/17/release/vs_BuildTools.exe)**,
the **[Vulkan SDK](https://vulkan.lunarg.com/sdk/home)**, and **Ninja** (included with the
Build Tools). Set the `VULKAN_SDK` environment variable before building (the SDK installer
does this automatically).

```
build-release.bat    ->  build-release\phyriad_fg.exe   (distributable, no debug DLLs)
build.bat            ->  build\phyriad_fg.exe            (debug build)
```

Four targets (`phyriad_fg`, `pfg_seam_test`, `pfg_clock_test`, `pfg_layer_gen`); LTO on `phyriad_fg`
only. (Corrected 2026-09-04 — this line said "one target".) Both scripts detect Visual Studio automatically via
`vswhere.exe` — no hardcoded paths.

## Run

```
build\phyriad_fg.exe --window "Game Title"      capture the monitor that window is on (DDA), generate, present
build\phyriad_fg.exe --monitor 0                capture monitor 0
build\phyriad_fg.exe --help                     the full flag list
```

DDA (Desktop Duplication) is the default capture path; `--capture-api wgc` selects Windows Graphics
Capture (window-only). The launcher UI (`ui\run.bat`) wraps the same flags with a target-window picker.

The intended workflow on a multi-GPU rig: the primary GPU owns the display and does the warp + present,
a second GPU runs the optical flow, and the integrated GPU does the zero-copy convert.

## Honest scope

- **It cannot exceed the display refresh** without frame injection or a display driver — it is an
  external overlay sharing the GPU(s) with the game.
- **Interpolation adds latency.** Like every external-capture FG, an intermediate frame is shown before
  the real frame it precedes; the cost is reported live (the `lat` stat). Use it for fluidity, not for
  competitive latency.
- **No engine data.** Disocclusion (newly revealed regions) is the hard case for any image-only FG; the
  quality layers mitigate it but cannot fully solve it.
- Built and tested on one Windows multi-GPU rig; treat hardware coverage as experimental.

## Architecture

See [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) for the full map, and `docs/diagrams/` +
`docs/slides/` for the figures:

- Three pipelined threads — **C** (capture, iGPU) → **F** (optical flow, second GPU) → **P** (warp +
  present, primary GPU) — communicating through `FgContext` (lock-free SPSC rings).
- A central config resolver (`resolve_config`) that reconciles flag dependencies and owns the
  cross-layer predicates in one place — the single source of truth.
- A governor control word that lets the pipeline graduate down non-essential work under GPU saturation
  while it keeps generating.

## License & attribution

Released under the **MIT License** — free to use, modify, and distribute. See [`LICENSE`](LICENSE).

If you use PhyriadFG or any of its code, the MIT license requires you to **keep the copyright notice**,
i.e. to **credit Eduardo Ramos Mendoza (Swately)**. A visible mention in your project's credits,
documentation, or about screen is appreciated.

© 2026 Eduardo Ramos Mendoza (Swately).
