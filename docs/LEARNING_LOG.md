# LEARNING_LOG — PhyriadFG (project tier)

> Governed by the container's [`METACOGNITION_PROTOCOL`](../../../protocols/core/METACOGNITION_PROTOCOL.md);
> the container ledger points here (L-004). **What belongs here:** a fact about this project that a
> future session would pay to re-derive — a plan premise the code refuted, a build trap, the measured
> character of an instrument, a tool that is weaker than its name suggests. **What does not:** an
> ordinary bug, a completed task, a number that lives in a gate record. Entry shape and triggers: the
> protocol's §3–§4. Newest first.

---

### P-007 · `gpu_load.exe` is a moderate loader, not a saturator — and the strong tool already exists
- **class:** refuted-premise · **date:** 2026-09-06 · **recurrences:** 0 · **status:** open
- **evidence:** `records/R4_GATE.md` §4.6.1 (measured this session): under `tools/gpu_load.exe` the
  4090 sits at **32–35 % utilisation** — the "under load" A/B of XR15 was a moderate-load A/B, said
  so in the record. `SATURATION_PLAN.md:141–144` had already named the reason: it is "a pure
  **compute** loader (a ring-fed dispatch) … it does NOT contend for the graphics/copy engines, the
  present queue, or VRAM bandwidth the way a real game's render does". Its buffer is 1 MiB fixed
  (`tools/gpu_load.cpp`, `kElems = 256*1024` floats), one D3D11 context, `kInflight = 3`.
- **lesson:** "Under `gpu_load`" in any PhyriadFG record means *moderate compute contention on one
  queue*, not saturation. A gate that needs real pressure (the FG governor's tier ladder, the
  `HOLON` / `BIDIR_OK` shedding arms of R5 step 3b) will not be exercised by it.
- **corrective:** the sibling project `projects/gpu_oc/` already contains `escalera_arbiter.exe`
  (prebuilt, D3D11, six detectors, profiles `mixed|heavy|light|chaos|sweep`, `--gpu N --secs S`,
  a 9 GB VRAM pass and a power-virus profile) — verified present first-hand 2026-09-06. R5's
  pressured run uses it; `SATURATION_PLAN`'s unbuilt `--graphics-load` idea is superseded by reuse.

### P-006 · A wedged present thread cannot be rescued from inside the process
- **class:** refuted-premise · **date:** 2026-09-06 · **recurrences:** 0 · **status:** corrected
- **evidence:** the operator's three `--tdr-test` runs (`records/R4_GATE.md` §5). After the device
  loss the P thread never returned from a driver/DXGI call (F printed `P pinned on gen … 64ms` from
  the dispatch tick on; the DXGI-loss line never printed), and the pillar's OA-10 watchdog could not
  hide the plane because `ShowWindow`/`SetWindowPos` from another thread route through the wedged
  thread's message loop.
- **lesson:** Two independent bounds are needed on a device-loss exit: every GPU wait must be
  bounded (`vk_wait_live`, 14 sites) **and** the worker joins must have a deadline, because a thread
  stuck inside the driver runs no code of ours again. The only give-back of an own-window plane is
  the process ending.
- **corrective:** both shipped (`fbb9566`, `90e2e2d`); the clean teardown (CSV finalize) does not
  run on that path and the exit line says so.

### P-005 · The async present's lost frames were a wait, not work
- **class:** refuted-premise · **date:** 2026-09-05/06 · **recurrences:** 0 · **status:** corrected
- **evidence:** `records/R4_GATE.md` §4.6: the warp batch costs 0.08–0.11 ms of GPU time and CAN
  complete 0.3 ms after submit, yet under the former default only 49.9 % of presents carried a new
  frame. `--present-waitable` (an existing knob) took it to 99.8 %.
- **lesson:** `uniq/s`, `disp_src` and `MsBetweenDisplayChange` count DECISIONS and cannot see a
  re-presented frame; only a per-tick fresh/re-shown column can. When a rate looks right and the
  result looks wrong, suspect the instrument's blindness before the algorithm.
- **corrective:** `phyriadfg_fresh` per CSV row + `fresh:N/s`; XR15 decided 2026-09-06.

### P-004 · The registry's parser chain is at MSVC's nesting limit
- **class:** recurrence-risk · **date:** 2026-09-06 · **recurrences:** 0 · **status:** accepted
- **evidence:** adding a second `else if` case to `parse_args`'s chain in `src/cli/cli.cpp` produced
  `fatal error C1061: compiler limit: blocks nested too deeply` (R5 step 1, build log).
- **lesson:** New CLI tokens go in `parse_extra`'s `if(...){ …; return 0; }` matcher, not in the
  long `else if` chain. This is a hard compiler ceiling, not a style preference.
- **corrective:** `--mv-consensus` / `--no-mv-consensus` live in `parse_extra`; noted in
  `records/R5_GATE.md` §1 as a finding for E4 (the flag-surface single-source stage).
- **accepted by:** the session — the chain is not restructured here; E4 owns that.

### P-003 · Files written by a patch script can carry `\r\r\n`
- **class:** recurrence-risk · **date:** 2026-09-06 · **recurrences:** 0 · **status:** corrected
- **evidence:** `flow/holons.{hpp,cpp}` were written with a double CRLF conversion (text already
  containing `\r\n`, then written with `newline='\r\n'`) → MSVC `warning C4335: Mac file format
  detected` on both files (R5 step 3a build).
- **lesson:** When a Python patch script writes a NEW source file, write `'\n'` text with
  `newline=''` and let the content carry its own endings, or normalise once before writing. The
  compiler is the only thing that notices.
- **corrective:** both files normalised; the pattern is named here.

### P-002 · A PowerShell-host launch writes UTF-16 logs
- **class:** recurrence-risk · **date:** 2026-09-05 · **recurrences:** 1 · **status:** corrected
- **evidence:** `tools/r4b/*.ps1` run from the PowerShell tool wrote `> $log` as UTF-16 LE
  (`ff fe` BOM); the same scripts launched under bash wrote UTF-8 with a BOM. The parser silently
  found zero windows on the UTF-16 logs until it was taught both.
- **lesson:** Any log parser in `tools/` reads the first two bytes and decodes `utf-16` on `ff fe`,
  else `utf-8`. A log that parses to zero rows is more likely an encoding than an empty run.
- **corrective:** `tools/r4b/r4b_parse.py` `read_log()`.

### P-001 · The plan said the holons ship OFF; they ship ON
- **class:** refuted-premise · **date:** 2026-09-06 · **recurrences:** 0 · **status:** corrected
- **evidence:** `CONVERGENCE_MASTER_PLAN.md` §R5 said the holon rows would be registered "**off by
  default**", claiming "the shipping default already has `igpu_field=false` cascading them off". The
  code: `cfg.gme`, `bidir`, `ambig`, `objects`, `scene_memory`, `inertia` are all `true`
  (`src/cli/cli.hpp:413, 489, 581, 600, 621, 675`) and every run log prints them ACTIVE.
- **lesson:** R5's gate is therefore **byte-identity of the default output**, not a "rows off"
  identity — a different and stronger test. More generally: a plan sentence about what "ships by
  default" is a claim about `cli.hpp`, and is checked there before a step is built on it.
- **corrective:** `records/R5_GATE.md` §0 entry decision 1; the container ledger's L-001 carries the
  frame half of this (the collective noun "the holon family" produced two false inferences in one
  day, this being one).

*Made with my soul - Swately <3*
