# Changelog

PhyriadFG is student-built and LLM-assisted, and every release is tagged `-experimental` because
that is what it is. Numbers in this file are quoted from the run that produced them, or the entry
says they were not measured.

## [0.6.0-experimental] - 2026-09-29

**No default setting changed.** With the one new layer row (`eco_anchor`) off, the frame kernel produced the same pixels as the unchanged legacy kernel in the same build under the comparison's settings (one 30 s run at 640x360, below). Frames were not compared against a 0.5.3 build. What changed is delivery: a default run puts more frames on screen, more evenly (Delivery, below). Three present-path fixes land in this release. Two concern the default own-window output: a present stall no longer turns it off for the rest of the session, and, when it is bound to a target window, a focus change no longer costs up to a second. The third applies when the log is redirected to a file or a pipe, which is how the launcher runs the FG: the presenter no longer waits on the disk to print a line. An interactive console is left as it was. The other engine changes are opt-in and off by default: an `--eco` power preset, an experimental phase anchor for it, a device-resident frame route, and measurement instruments. Two things reach every user without a flag: a reorganized launcher (the command it builds for the same settings is unchanged) and a few more log lines at exit and when things go wrong.

### Changed

- **The default path.** No flag default changed, and no default layer changed. The layer contract hash, printed in the log and in the launcher, moves from `0xBF27BBBA9109A3E3` to `0x0291FB5481748A07`. The hash covers every row of the layer table, and one row was appended (`eco_anchor`, off by default, see Added). A row that is off is folded out of the kernel at pipeline creation. This was measured on the GPU in one 30 s run at 2x on a 640x360 synthetic test window, with a non-shipping comparison build (FMA contraction disabled so the two kernels can be byte-compared). With the row compiled in but off, the default configuration matched the unchanged legacy kernel on the same tick with 0 differing pixels over 7190 ticks. Both kernels had the hold ramp set by flag to the legacy value, the only way the two can be compared since 0.5.3. `--eco` with the same two flags gave 0 differing pixels over 7187 ticks.
- **Delivery against a 0.5.3 build, default flags.** The test used a synthetic 120 fps source whose captured area is 1920x1061, 2x at 240 Hz, single GPU, with the FG's log redirected to a file. Two draws each:
  - displayed frames per second went from 204.8 to 233.7, and from 206.3 to 233.6;
  - the 99th-percentile display interval went from 25.02 to 8.34 ms;
  - board power rose +3.2 / +1.9 W, and FG GPU time rose +6.0 / +5.1 GPU-ms/s, alongside the extra presents (no run attributes the rise to them).

  The build measured predates the focus-change fix below, so that fix is not a candidate. Of the changes in that build, the log pipe is the only one that changes what the present thread waits on in a run without a capture tap. The others add log lines, act only with a capture tap, or act only after a stall. That makes the log pipe the likeliest cause, but this is an inference. No run isolates it, and the 0.5.3 binary's build toolchain was not compared with this one's. Through the launcher's pipe, and with an interactive console, this was **not measured**.
- **Every run logs more at exit and when things go wrong.**
  - One `present stall trace: N tick gaps > 100 ms` line. Each gap over 100 ms gets its own line that splits the time between the parts of the loop.
  - An own-window run adds a `watchdog: hides=H ended=E max_stall_ms=S` line, and each watchdog hide and its end are logged with their time.
  - A redirected run adds one line saying its stdout goes through the in-memory pipe.
- **From this release, the repository's files no longer include the project's working documents or its measurement tools** (earlier commits and tags still contain them). Apart from that, the product (source, shaders, the launcher and the user documentation) is unchanged. The build adds one GPU test program and one documentation-consistency test only where the removed tools and documents are present. User-facing texts were reworded with no change in logic. Help texts no longer point at documents that are not in the repository.
- **`--fg-core-ab`'s help now says what it measures.** Since 0.5.3 the two kernels differ on purpose in `single_track`'s hold ramp, so the instrument counts differing pixels under every configuration. For a parity run, add `--st-hold-lo 1.2 --st-hold-hi 3.0`.

### Fixed

- **One present stall longer than 250 ms turned the FG's output off for the rest of the session, and nothing said so.**
  - What happened: the own-window watchdog hides the plane when the present thread stops for longer than 250 ms, so that a wedged FG never holds the panel. Nothing ever brought the plane back. The FG kept presenting to a hidden window while the game or desktop showed through, and the log still reported the plane as displayed.
  - The fix: a hide now ends at the first present after the stall. The stall is measured, and the plane is re-asserted unless the foreground has moved elsewhere.
  - Verified live in two runs under an artificial disk load: 6 and 8 hides, all ended with a re-assert, longest stalls 906 and 1047 ms.
- **With a target window bound, returning to the game after a focus change cost up to a second of output.**
  - What happened: while the own-window plane was yielded (the foreground was neither the game nor the FG), each tick still waited up to 1000 ms on the frame-latency waitable. Nothing was being presented, so nothing could signal it. A yielded FG ticked about once per second, and every return to the game waited up to a second before the plane came back.
  - The fix: the wait now happens only on a tick that will present. A yielded tick pauses 50 ms instead.
  - Diagnostic A/B, one 20 s run per build, three forced focus changes:
    - before: five tick gaps of about 1 s each and 3591 presents;
    - after: no gap over 100 ms and 4023 presents.
  - The cost of a yielded FG now ticking 10 to 20 times per second instead of once is **not measured**.
- **With stdout redirected, the presenter could wait on the disk to print a line.**
  - What happened: stdout is unbuffered, so every log line from the present thread was a synchronous write. When the log's disk was busy, one line could take 0.1 to 0.8 s.
  - The fix: a stdout redirected to a file or a pipe now goes through a 1 MB in-memory pipe that one thread writes out, in order. An interactive console is left as it was.
  - Measured in one 20 s run each way with a capture tap streaming to the log's disk: 23 tick gaps over 100 ms and 14 watchdog hides before the fix, 0 and 0 after.
- **`--warp-timing` destroyed its timestamp pool while the last warp batch could still be in flight.** The validation layer reported one error at teardown. The teardown now waits for that batch. This only happens with the flag on.

### Added

- **`--eco`**, an opt-in power preset, off by default. It is exactly `--frame-vram --no-bidir`, including `--no-bidir`'s cascade.
  - Route: single GPU only. If the PC has a discrete GPU other than the one driving the display, add `--force-single-gpu` (the launcher's single-GPU switch). Otherwise the run ends at start-up with a refusal message, as with `--frame-vram`. An integrated GPU does not count: a PC with one discrete GPU and an integrated one already takes the single-GPU route.
  - Power, 1920x1061, 2x (a 120 fps source on a 240 Hz display), two draws against the default, on one overclocked RTX 4090 (driver 610.88, +85 MHz core offset). The watts are the GPU's board power as the driver reports it, not whole-system power:
    - board power -43.1 and -43.5 W;
    - the GPU drops from its top clock bin to 2610 MHz;
    - median latency changed by +0.01 and +0.09 ms, within noise.
  - Memory:
    - device memory +236 MiB;
    - host commit +235 to +243 MiB. The working set stayed at the default's level in 7 of the 8 `--eco` runs; the eighth read about 54 MiB higher and was not investigated.
  - Quality was scored against an analytically exact rendering of a synthetic scene at 640x360, 2x, one scene at two speeds. `--frame-vram` and `--gme-sub2-force` (a diagnostic, below, that starts the global-motion fit in its sub-sampled state) were set on both arms, so the comparison isolates `--no-bidir`.
    - Result: a regression of 2 to 9 % per presented frame, averaged over the two phase positions a 2x run presents at. Those are t~0.25 and t~0.75, a quarter and three quarters of the way from one real frame to the next.
    - All of the regression is in the t~0.75 frame, where the affected terms move +7.5 to +50 %.
    - A few other terms improve: sphere missing area -54 % on the faster scene, and sphere position error on the slower one. The error moves between hallucinated and missing area, differently per object and per speed.

    `--eco` itself does not set the `--gme-sub2-force` pin. In the logged `--eco` runs without it, the fit never reached that state, so these figures describe a fit state the shipped preset was not seen to run in.
  - `--eco`'s quality exactly as shipped, and its quality at 1080p: **not measured**.
- **`--eco-anchor {1|2}`**: EXPERIMENTAL and off by default. It is a phase anchor for `--eco` that re-selects the high-t motion vector from the forward field alone.
  - 1 = one fixed-point tap. Mode 1 was scored only on the 40 frames used to choose the mode, where it raised box missing against `--eco`. It has no other result.
  - 2 = lean candidate re-selection, the only mode validated.
  - It needs `--eco` (or `--no-bidir`). Bidirectional flow is on by default, so on its own the flag is refused with a parse error. It is also refused with `--legacy-warp` or `--fg-core-ab`. Any value other than 1 or 2 turns it off and says so.
  - At start it prints the mode it runs in.
  - At t <= 0.35 it returns its input before any texture read. On the GPU, in one 30 s run on a 640x360 synthetic test window, that frame was byte-identical to `--eco`: 0 pixels over 3591 ticks. At t~0.75 it changed about 4270 pixels (1.85 % of that 640x360 frame) per tick.
  - One 640x360 capture (1190 frames, global-motion fit pinned) was replayed against a CPU reference implementation. The GPU output matched it within 1 LSB on at least 99.961 % of pixels per frame, about the same agreement that reference shows with `--eco` without the row.
  - Cost of mode 2 over `--eco`, at 2x and 1920x1061:
    - warp time +2.3 GPU-ms/s (+15 % of the warp pass), read from the FG's own logs in one draw of two runs per arm, with `--site-timing` on in both arms;
    - the whole-process GPU-time difference, measured without `--site-timing` over two draws, was within noise;
    - board power about +0.7 W, with `--eco`'s 2610 MHz clock kept, read from the same driver-reported GPU board power as the `--eco` figures. Whole-system power was not measured.
  - Quality was measured offline only, as a CPU replay of `--eco` captures at 640x360 and 2x, with `--gme-sub2-force` set, one scene at two speeds, scored on the t~0.75 frame. Against `--eco` on the faster scene:
    - sphere hallucination -57 % and sphere shape error -50 %;
    - box hallucination -28 % and box missing -7 %;
    - but sphere missing **+70 %**. Over two thirds of that rise sits at the loop seam and where the sphere leaves the frame. At the seam, most of the added missing pixels come from the row's fallback to the global-motion vector.

    That fails the acceptance bar set before the test. On the slower scene it is better than `--eco` on every term.
- **`--eco-anchor-ab`**, a measurement instrument that requires `--eco-anchor N`. The product runs with the row off, and a second kernel with the row on runs beside it from the same inputs. Differing pixels are counted per phase slot. If its second kernel cannot be set up, it logs that the instrument is off and the product keeps running.
- **`--frame-vram`**, off by default, not listed in `--help` on its own (`--eco`'s help names it). The converted frame stays on the GPU: a device mirror of the capture ring feeds the flow and the presenter, with no round trip through host memory.
  - Single GPU only, on the same terms as `--eco` above. It is also refused, and the run ends, wherever the host ring is still read: iGPU convert, `--upscale`, `--upload-xfer`, `--real-fast-path`, `--rfp-fresh`, `--motion-fallback`, `--dump`, `--pairdump`, or without warp-at-presenter.
  - Its planned evaluation could not be completed, because runs in it hit the focus-change stall fixed above. Its effect is therefore **not measured** under that plan.
  - Informative deltas from two draws, not a verdict. The draws ran at 1920x1061, 2x at 240 Hz, with `--gme-sub2-force` on both arms. The first draw was re-run once, and one of its runs was still excluded because it hit the start-up focus-change stall.
    - FG GPU busy time -174 / -175 GPU-ms/s, about -30 %;
    - board power -1.25 / -1.21 W, with the GPU staying at its top clock bin in every run;
    - device memory +236 / +237 MiB;
    - host commit +237 MiB, with resident memory flat.
- **`--site-timing`**, a measurement instrument, off by default, not listed in `--help`. It places GPU timestamps at the named sites of a frame's path (convert, forward and backward flow, bridge upload, the D3D11 capture and present copies) and records host wall times. After a 5 s warm-up it prints distributions and cost per second at teardown, and it implies `--warp-timing`.
  - Its own cost is not settled. The readings with it on:
    - a GPU-engine counter sampled twice per run read the FG's GPU busy time +11 % in the first pair of draws, and it disagreed with the other counters throughout;
    - whole-process counters on the same runs read about zero;
    - later pairs read 1.4 to 1.5 % fewer interpolated frames;
    - counting only displayed presents, the displayed rate was about 5 to 9 per second higher in all ten draws measured.

    Do not compare a `--site-timing` run with a run without it.
- **`--gdump-live 0|1`** (default 1, unchanged): 0 makes the capture tap record no live warp frames, so its writer spends the disk on the pair sets. In one pair of 20 s runs, pairs written went from 381 of 2360 (16 %) to 1384 of 2359 (59 %), and the record from 39 to 23 GB.
- **Diagnostic formats** (relevant only if you parse these outputs yourself). Every `--qdump` manifest line now carries `eco=N eco_h=H`. The `--gdump` header adds `live 0` and `eco_anchor N H` lines when those are not default. `--dump-config`'s `[old2]` line adds `eco_anchor=`.
- **Tests.** New ctest cases cover the `--eco` preset, the site-timing book, `--frame-vram`'s arming, the plane watchdog, the stall trace, the log pipe and the `--eco-anchor` refusals; the layer contract pin moved with the new row.
- **`--gme-sub2-force`**, a diagnostic, off by default, not listed in `--help`. The global-motion fit starts in the sub-sampled state that its one-way CPU-time latch can reach mid-run. The latch did fire in default runs at 1920x1061; under `--eco`, and at 640x360, it never fired. With the fit started there, equivalence runs do not depend on whether or when the latch fires.
- **Launcher: an "Essentials" card first, and everything else in one folded "Advanced options" section.**
  - Essentials holds: target window, window title, monitor, multiplier, frame-gen GPU, single GPU, FPS overlay, HDR, refresh rate and output FPS cap.
  - Advanced holds every other group, the layer registry, raw flags, and the executable path (moved out of the sidebar).
  - Each flag still has exactly one control, and the command it builds is unchanged.
  - The multiplier list is now 2x / 3x / 4x / auto. 5x, 6x and 8x are still accepted by the FG through Raw flags (`--fg-factor 6`).
    - On the default output route the FG presents on every display refresh (or at the output FPS cap), so the effective multiplier is the refresh rate divided by the game's frame rate.
    - On that route `--fg-factor` only sizes a fallback buffer, as its start-up log line says.
  - A new "Eco (power)" group carries `--eco`, which had no control before, and the eco anchor as a select marked experimental.
  - The note field and its Mark button are removed from the live output.
  - The FPS overlay's description no longer says the default path skips it. It is drawn there (by the code; not re-verified live in this release).
  - The native dropdowns that remain get themed option colours in light and dark.
- **Launcher: the target-window picker is a themed list instead of a native dropdown.**
  - Each window has two lines: its caption, then exe, pid and minimized state.
  - It is themed for light and dark, stays open while the list refreshes, and supports type-ahead. These were checked in a browser with the launcher's backend stubbed, not yet in the built launcher (see Known issues).
  - Minimized windows are listed but cannot be picked.
  - A pick still passes `--window-pid` / `--hwnd`, and picking the window already bound no longer schedules a restart.

### Known issues

- **`--eco-anchor` is experimental in the plain sense.**
  - The live A/B against `--eco` has **no result**: captures exist, nothing has been scored, and nothing from it is in these notes.
  - It was validated at 2x only.
    - At 3x, by the generator's phase rule (no 3x run was recorded to confirm it), a third of the generated frames would sit at t = 0.5, the middle of its blend ramp, which is untested.
    - At 4x (phases measured on one 640x360 capture) the row is nearly off at t~0.375 and nearly full at t~0.625.
  - At 1080p its candidate ring spans 3 motion-vector texels against 1 in every validated frame, so it is a different candidate set, and its 1080p quality is **not measured**.
  - It fails its own offline acceptance bar on sphere missing (above).
- **`--eco` was measured at 2x only.**
  - Its quality was measured at 640x360 with the global-motion fit pinned by `--gme-sub2-force`, a state the shipped preset was not seen to run in.
  - Its power was measured at 1920x1061, from one synthetic 120 fps scene at 240 Hz on one machine.
  - Its 1080p quality, its quality exactly as shipped, and its power and quality at 3x and 4x are **not measured**.
- **Every GPU measurement in this release ran on the single-GPU route.** The multi-GPU route (convert on the integrated GPU, optical flow on a second GPU) was not measured.
- **A hard crash can lose the last log lines.** With the log pipe armed, a crash waits at most 200 ms for the pipe to drain. Lines still in it after that are lost.
- **The launcher changes were checked in a browser with the launcher's backend stubbed**, using a faked window list that included two windows of one process, duplicate titles and a minimized window.
  - There were no console errors.
  - The command built for the Essentials, `--eco` and `--eco-anchor 2` was right, a pick adds `--window-pid` / `--hwnd`, and a minimized window cannot be picked.
  - On the window picker, row and status-line clicks, a scrollbar drag and the arrow keys were real mouse and key input. Enter, Escape, Home, End and type-ahead were tested with scripted key events.
  - Both themes were rendered.

  The built launcher was not exercised against live windows before this release.

### Requirements

To run the release build you need Windows 10 or 11 (x64), the Microsoft Visual C++ Redistributable 2015 or later (x64), and a GPU driver with Vulkan support. The launcher also needs the Microsoft Edge WebView2 runtime, which ships with Windows 11. Keep `phyriad_fg.exe` and `ui.exe` in the same folder.

## [0.5.3-experimental] - 2026-09-13

**A default moved, and the measurement that moved it is printed below including the case where it
loses.** The one warp-vs-hold decision the shipping composition still consults now holds a pixel
sooner. Everything else here is a teardown fix, two measurement flags, and one honest disclosure
about three flags that have been doing nothing.

### Changed

- **`single_track`'s hold ramp ships at 1.0 / 1.6 (was 1.2 / 3.0).** Twenty-eight live captures
  across five synthetic corpora spanning 1.7 to 13.4 px of motion per source pair, every generated
  frame matched against an analytically exact rendering of the same scene at that frame's own phase,
  and the old value restored BY FLAG from the same binary:

  | corpus | px/pair | box position | box hallucinated mass | sphere position | static panel | sharpness |
  |---|---|---|---|---|---|---|
  | sc_v05 k=4 | 1.7 | -5.8 % | -10.6 % | -6.4 % | - | 0.9665 -> 0.9696 |
  | sc_live2 k=4 | 3.4 | +2.0 % | -8.0 % | +2.2 % | -56.2 % | 0.9419 -> 0.9415 |
  | sc_live k=8 | 6.7 | **-15.1 %** | -25.6 % | **+11.1 %** | -84.8 % | 0.9279 -> 0.9296 |
  | sc_v4 k=4 | 13.4 | **-24.9 %** | **-43.6 %** | **-27.1 %** | -95.0 % | 0.8913 -> **0.9030** |

  Negative is better everywhere except the last column. Three separate measurement rounds reproduce
  the box's position at 6.7 px/pair (-13.7 / -14.2 / -15.1 %), the hallucinated mass everywhere, the
  static panel, and **the sphere's position getting WORSE at 6.7 px/pair (+5.4 / +10.7 / +11.1 %)**.
  That last one is the price of this change and it is real: holding a pixel helps a wrongly-matched
  rotating surface and hurts a correctly-matched translating one. The 1.7 px/pair row reads **not
  measured** -- two rounds disagree in SIGN there, on absolute values of 0.16 px.

  These are offline numbers from a synthetic scene with exact ground truth, not from a game, and
  they say nothing about how the change looks to an eye. **`--st-hold-lo 1.2 --st-hold-hi 3.0`
  restores the previous behaviour exactly.**

### Fixed

- **Quitting could freeze the window with the display panel still held.** `destroy()` joined the
  plane watchdog with no deadline while the message pump sat below the join, and the watchdog's own
  `SetWindowPos` / `ShowWindow` block until the window's owning thread dispatches -- the thread
  inside the join. Both waited on each other, `DestroyWindow` was never reached, and the panel was
  never given back, which inverts the watchdog's entire purpose. The wait now pumps the queue on a
  2 s deadline and detaches rather than joins if the watchdog does not return. NOT verified live:
  reproducing it needs a run that ends with the plane displayed.

### Added

- **`--gdump <dir>`** -- the every-tick capture tap on the shipping asynchronous path. A writer
  thread streams every recorded warp plus the pair planes once per pair and the present loop never
  waits on it. A measurement instrument, off by default, refused together with
  `--afill` / `--fps-overlay` / `--ts-smooth`.
- **`--st-hold-lo` / `--st-hold-hi`** -- the two edges of the ramp above, so the previous default is
  reachable from the same binary.

### Known issues

- **`--residual-ceil`, `--conf-improv` and `--agreement` do nothing under the shipping default.**
  The per-pixel confidence they compute is consumed only by the `select` row at COMPOSE rank 260,
  and the two rows after it -- `stasis` and `single_track` -- are both declared OVERRIDE and discard
  it. Proven twice: an A/B across those flags moved zero pixels, and a CPU reference that omits the
  gate entirely reproduces the GPU output at 99.47 %. The flags are left in place, and documented
  here, rather than silently removed.
- Measurement numbers published before 2026-09-11 describe the previous default;
  `--st-hold-lo 1.2 --st-hold-hi 3.0` reproduces the old value.

## [0.5.2-experimental] - 2026-09-06

**Three regressions from 0.5.0/0.5.1, all found in the author's own session log.** If you are on
0.5.0 or 0.5.1, upgrade: the first one makes the launcher unusable after a single Stop.

### Fixed

- **Start stopped working after the first Stop.** Stopping the FG left the launcher unable to spawn
  anything ever again -- Start flashed "Running" and nothing launched. The two-stage stop added in
  0.5.0 reached the console-less child through `AttachConsole`, which replaces and then closes the
  *launcher's own* standard handles; every later spawn then failed with
  `The handle is invalid. (os error 6)`. The console path is REMOVED, and the FG is now spawned with
  its own explicit stdin so no external handle state can reach it. Stop is an immediate kill again;
  for a finalized `-stats.csv`, end the run with `--duration` or `--max-frames`, which run the FG's
  full teardown.
- **A one-pixel flutter ended the session.** 0.5.0's mid-run resize guard treated ANY size change as
  fatal, and a browser relaying itself out by a single pixel -- `1920x1080 -> 1920x1079` -- was enough
  to quit. It now exits only when the source GROWS beyond the size the pipeline was built for, which
  is the case where frames are actually being cropped. A source that shrinks says so once and keeps
  generating frames.
- **Selecting a window, then letting it rename itself, lost the binding.** The launcher re-validated
  the picked window by TITLE, so a browser tab switch dropped the pid and handle and fell back to
  spawning with the stale caption alone -- which the FG then correctly refused. Identity is now
  re-validated by handle, then by pid, never by title; a rename is followed, and the field updates
  with it. `--window-pid` exists precisely to survive a rename, and the launcher was discarding it at
  the moment it became useful.

## [0.5.1-experimental] — 2026-09-06

**One file is now enough.** The launcher carries the frame generator inside itself and writes it out
on first run, so `PhyriadFG.exe` alone is a complete install — no second download, no archive to
extract. Nothing about the architecture changed: they are still two processes, because the launcher
spawns, streams, kills and respawns the FG, and the whole epoch-scoped restart contract lives in that
separation.

### Added

- **`PhyriadFG.exe` is self-contained.** The FG payload is embedded at build time and extracted to
  `%LOCALAPPDATA%\PhyriadFG\bin\phyriad_fg-<hash>.exe` on first launch. The file is named by the
  hash of its own contents, so a new version writes a new file instead of racing to overwrite one an
  older launcher may still be running, and extraction is skipped entirely once it is there.

### Unchanged, deliberately

- **A `phyriad_fg.exe` sitting next to the launcher still WINS.** Verified: with one beside it, the
  launcher never extracts anything at all. Dropping a freshly built binary next to the launcher and
  running THAT one is how this project is developed, and an embedded copy quietly taking precedence
  would have made testing a new build impossible.
- The executable-path field in the UI still overrides everything.
- A launcher built in a tree whose C++ was never built simply carries an empty payload and looks for
  the FG on disk, exactly as before — `cargo build` on its own must not fail.

## [0.5.0-experimental] — 2026-09-06

Two arcs land together: a convergence restructure that made the frame-generation kernel a
single pure path, and a 40-finding quality-of-life audit whose fixes touch nearly every surface a
person actually operates.

### The headline

**The pure kernel `fg_core.comp` is now the shipping default.** `--legacy-warp` selects the previous
`wap_warp.comp`, which stays in the tree — the revert is one token. The default was flipped only
after the acceptance criterion was found unmeetable by the incumbent, amended *before*
being used, and then measured on two scenes and two instruments.

### Behaviour changes — read this section before upgrading

Ten changes alter what an existing user sees. None is a redesign; each closes a case where a surface
claimed one thing and the code did another, or where a failure was silent.

1. **The output clock now runs at the panel's rate.** It was hardcoded to 240 Hz while the log
   asserted it came from the panel. On a 165 Hz panel the FG ticked at 240 against a 165 Hz vblank;
   it now ticks at 165, and the log states its source (`--refresh-hz override` / `derived from the
   present monitor` / `built-in default`). **Any stored A/B baseline taken without an explicit
   `--refresh-hz` is no longer comparable.** Pass `--refresh-hz 240` to reproduce the old timing.
2. **An unbound present plane never yields.** With the default `--present-own-window` and no
   `--window` (plain monitor capture), the plane used to vanish the moment anything else took focus —
   i.e. almost always, which is why monitor capture appeared to do nothing. It now stays displayed
   for the whole run. It is click-through and excluded from capture, but it does own that panel until
   PhyriadFG quits.
3. **A mid-run source resize now exits.** Maximise, F11, a DPI change or a docked DevTools pane used
   to leave a running FG quietly cropping or letterboxing, because the capture pool is created once
   and was never recreated. It now prints `REASON SOURCE_RESIZED` and exits. The full fix — pool
   recreate plus a staging-ring realloc — needs a re-init path this codebase does not have yet.
4. **`--window` may resolve to a different window than before.** The finder no longer takes the first
   Z-order match: it accumulates every visible match, compares case-insensitively, excludes
   PhyriadFG's own windows, and picks exact title equality first, else the largest client rect. When
   more than one matched it says so and names its choice.
5. **The overlay opens on the target window's monitor.** On the default WGC + `--window` path the
   present monitor never followed the window; it does now. `--present-monitor N` also selects the row
   `--list-monitors` printed, rather than a different enumeration's N.
6. **The "Swapchain waitable" switch works in both directions.** Turning it off previously did
   nothing. Anyone who believed they had disabled it will now actually get the non-waitable path —
   measured at 49.9 % fresh versus 99.9 %, so their frame freshness will drop. The switch is
   finally doing what it says.
7. **Stop asks for a clean shutdown first.** A run stopped from the launcher now produces its
   `<csv>-stats.csv`, which never appeared before. Stop can take up to 4 s before the UI flips.
8. **A minimized target refuses to start**, with `REASON SOURCE_MINIMIZED`, instead of capturing the
   monitor and reporting healthy statistics over the wrong picture.
9. **Every failed init exits non-zero.** Nine device-init stages used to print one line and return 0,
   indistinguishable from a clean quit. The exit contract is now **0** clean / **1** fatal / **2**
   parse error / **3** parity failure.
10. **The process speaks UTF-8** (Windows 10 1903+, via an application manifest). Non-ASCII window
    titles now match, echo correctly, and round-trip through `--csv` paths. The CSV's Application
    column carries UTF-8 rather than CP_ACP bytes for such titles.

### Added

- **`--window-pid N` and `--hwnd N`** — bind the capture target by owning process or by exact window
  handle. Resolution order is `--hwnd` (while `IsWindow`) → `--window-pid` → `--window` substring, so
  a title that changes mid-session no longer loses the target. The launcher emits them automatically
  when a window is picked from its dropdown.
- **Six new failure reasons**: `SOURCE_MINIMIZED`, `SOURCE_RESIZED`, `SOURCE_CLOSED`, `NO_FRAMES`,
  `DEVICE_INIT_FAILED`, `DEVICE_LOST`. Every one prints a machine token and a one-line advisory.
- **A first-frame timeout** on the WGC path: a session that starts but delivers nothing now exits
  after 5 s instead of idling at `cap 0/s` forever.
- **Source-death detection** for monitor capture, and cadence that follows a window dragged to a
  panel of a different refresh rate.

### Fixed

- **The launcher could wedge, showing "running" for a process that had died.** A 600 ms time-based
  guard swallowed the new child's exit after a config-change restart, leaving Start disabled and Stop
  inert until the window was reloaded. The guard is now scoped to the child's epoch, which travels in
  the exit event, and Stop always resynchronises the UI with the backend's real state.
- **`--fg-gpu primary` killed single-GPU rigs** during init. It is now inert there, with one line
  saying so; multi-GPU behaviour is unchanged.
- **`--capture-api dd` with a stale title silently captured monitor 0.** It now reports
  `WINDOW_NOT_FOUND`.
- **An out-of-range `--monitor` read past the end of the output list** (undefined behaviour). It is
  now reported and clamped.
- **A window title containing a comma or a quote corrupted the telemetry CSV** from the Application
  column onward.
- **Auto-restart fired on half-typed values** and killed the running FG before spawning its
  replacement, so a partial executable path left nothing running. Free-text fields now commit on
  blur, and the new process is validated before the old one is destroyed.
- **The launcher's self-exclusion filter never matched** (it compared against `PhyriadFG.exe` while
  the binary is `phyriad_fg.exe`), so the FG's own overlay was offered as a capture target; and the
  window list collapsed distinct windows that happened to share a caption.
- Probe subprocesses escaped the kill-on-close job object; a `[ra] REASON` line was styled as
  ordinary log chatter; five launcher controls duplicated registry-owned flags and could silently
  disagree with them.

### Known limitations

- `--igpu-field` still defaults **off**, so `bg-snap` and `band-xfade` do not run in a default
  session. The help text now says so rather than claiming otherwise. Turning it on changes presented
  pixels and is deliberately left as a measured decision, not a silent flip.
- The capture pipeline is sized once at init. A resize is detected and reported, not absorbed.
- On the DDA path, a captured window moved to another monitor is still not followed.

*Made with my soul - Swately <3*
