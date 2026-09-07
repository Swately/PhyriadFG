# Changelog

PhyriadFG is student-built and LLM-assisted, and every release is tagged `-experimental` because
that is what it is. Numbers in this file are quoted from the run that produced them, or the entry
says they were not measured.

## [0.5.0-experimental] — 2026-09-06

Two arcs land together: the CONVERGENCE restructure (R0–R7) that made the frame-generation kernel a
single pure path, and a 40-finding quality-of-life audit whose fixes touch nearly every surface a
person actually operates.

### The headline

**The pure kernel `fg_core.comp` is now the shipping default.** `--legacy-warp` selects the previous
`wap_warp.comp`, which stays in the tree — the revert is one token. The default was flipped only
after the acceptance criterion was found unmeetable by the incumbent, amended in the plan *before*
being used, and then measured on two scenes and two instruments (`docs/planning/records/R7_GATE.md`).

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
   measured at 49.9 % fresh versus 99.9 % (XR15), so their frame freshness will drop. The switch is
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
