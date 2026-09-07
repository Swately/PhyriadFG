# QoL findings — operator's own use of the launcher + FG

**Opened** 2026-09-06, during the operator's first hands-on session with the post-R7 build
(`ui.exe` rebuilt with the `maid` skin, `phyriad_fg.exe` of today beside it).

This record is the *defect ledger* for quality-of-life failures found by USING the product, as
opposed to the convergence gates, which measure the pipeline. Each entry states who reported it,
what is **traced with evidence** and what is still **unverified**. An entry is never promoted from
reported to traced without quoted source lines.

Three entries are open. **None is fixed** — D-1 and D-2 land on the default present/capture path and
wait on the operator's word; D-3 has a confirmed root cause and a one-line stopgap, also unapplied.

---

## D-1 — a selected window that renames itself is silently lost

**Reported by the operator**, 2026-09-06: *"cuando una ventana seleccionada cambia de nombre, por
ejemplo al usarlo en chrome, esta cambia de nombre de vez en cuando y el FG se confunde"*.

**Status: TRACED.** Window identity is a *substring of the title*, resolved once, and a title is
not an identity.

`src/capture/capture.cpp:87-99` — `find_window_by_substr`:

    EnumWindows(...)  ->  GetWindowTextA(h, title, 256)  ->  strstr(title, needle)  ->  IsWindowVisible(h)

and it returns the **first** match. Consequences, in the order they bite:

1. Chrome rewrites its title on every tab switch and on every page that changes `document.title`.
   The needle that matched at start-up need not match a second later.
2. There is no re-resolution loop: the HWND is captured at init and never re-checked, so a *rename*
   does not lose the window — but a **restart of the capture** resolves the needle against whatever
   the title says at that moment, which may be a different window entirely.
3. `strstr` + first-match means a needle like `Chrome` can bind to any of a dozen Chrome windows,
   and which one is decided by `EnumWindows` Z-order, i.e. by whatever the operator clicked last.

The fix is an identity that does not move: resolve the needle once to an HWND **and a PID**, then
hold the HWND, re-validating with `IsWindow()` rather than by re-matching the title. Not applied:
it changes the meaning of the `--window` flag, which is a product-visible contract.

---

## D-2 — clickables do not follow a window that is resized or moved

**Reported by the operator**, 2026-09-06: when a window such as Chrome is rescaled, the location of
the clickable elements no longer matches what the FG presents.

**Status: TRACED**, and it is not one bug but three that compose. Quoted sources:

**(a) the present plane is monitor-sized and click-through.** `src/present/present_stage.cpp:36`

    psd.monitor_index = cfg.pres_mon; psd.width = 0; psd.height = 0;  // full present-monitor extent

`framework/render/present/src/PresentSurface.cpp:327-328,364-368,380`

    impl->mon = pick_monitor(desc.monitor_index);
    impl->W = desc.width ? desc.width : static_cast<UINT>(impl->mon.right - impl->mon.left);
    if (desc.style == Style::OwnWindow) { ex |= (WS_EX_LAYERED | WS_EX_TRANSPARENT); ... }
    SetWindowPos(impl->hwnd, HWND_TOPMOST, impl->mon.left, impl->mon.top, (int)impl->W, (int)impl->H, ...);

So the generated frames are painted over the **whole monitor**, not over the source window's
rectangle. `WS_EX_TRANSPARENT` means the clicks do pass through to whatever is underneath — the
pointer is never captured — but what the operator *sees* at a given pixel is the FG's idea of where
the window was, while what he *hits* is where the window actually is. The DcompCt overlay branch
(`PresentSurface.cpp:349-351`) carries the same two style bits.

**(b) the source rectangle is read once.** `src/capture/capture_init.cpp:413-418` calls
`find_window_by_substr` and then `GetClientRect` a single time, with the comment
*"this feeds NAT_W/NAT_H + the WGC pool today"*. A resize after init is never observed.

**(c) the WGC pool is never recreated.** `src/capture/capture_init.cpp:199` builds the frame pool
with `CreateFreeThreaded`, and **`Recreate()` appears nowhere in the repository**; `ContentSize()`
is read only under `--dpi-probe` (`capture_init.cpp:245`). A window that grows past the pool's
buffer size is therefore captured at the old size and letterboxed or cropped by whatever scales it.

**Workaround that works today:** capture the **monitor** rather than the window. The plane is
already monitor-sized, so source and destination geometry agree and the misalignment disappears.

**Not applied**, and this is the reason: the fix is (plane sized and positioned over the target
rect) + (WM_MOVE/WM_SIZE tracking) + (client-rect re-read) + (`FramePool.Recreate()`), which is a
change to the default present path. That is the operator's call.

---

## D-3 — changing the frame-gen GPU with auto-restart on: no restart, then no shutdown

**Reported by the operator**, 2026-09-06: *"cuando cambie la gpu seleccionada para el renderizado
del FG, con el restart automatico activo este no reinicio y simplemente no dejo apagarlo aun con el
FG apagado"*.

Two symptoms in sequence:

1. The frame-gen GPU selector was changed while **Auto-restart on config change** was enabled, and
   **no restart occurred**.
2. Afterwards the launcher **would not stop the FG**, even though the FG was already off — the UI's
   process state disagreed with reality (a live Stop path for a dead process).

**Status: CONFIRMED, root cause found — and it is ONE bug, not two.** Found independently by three
of the audit's five dimensions (`launcher` L-2 · `config` C-1 · `encoding` E-7, run `wf_d4d5ef46-620`,
`QOL_AUDIT_2026-09-06.md`) and then **checked first-hand by this session**, line by line.

**The mechanism.** `ui/src/main.js:1035` sets `restarting = true` before `await invoke("restart")`.
It is cleared by a bare timer 600 ms after that promise resolves (`:1046-1048`) — i.e. the window
opens *after the new child has already been spawned* and covers its entire early life. Line `:1099`
is an unconditional `if (restarting) return;`, so an `fg-exit` arriving in that window is dropped and
`setRunning(false)` at `:1102` never runs. From there:

- `running` stays `true` → `btnStart.disabled` stays set (`:997`) → **the FG cannot be started**;
- the pill still reads *running* (`:1000`) → **the UI asserts a process that is dead**;
- `btnStop`'s handler (`:1079-1086`) awaits `invoke("stop")` and does nothing else, while
  `lib.rs:423-431` takes an already-empty slot, kills nothing and returns `Ok(())` **with no event**
  → **Stop is inert**;
- `grep -n "is_running" ui/src/main.js` returns **exactly one line, `:1119`**, inside
  `DOMContentLoaded`. There is no polling and no resync. The state is stuck until the window reloads.

**The guard swallows the wrong child, and its own comment proves it.** The comment at `:1033-1034`
states the event being dropped is the previous process's — *"este `fg-exit` es del hijo VIEJO"*. But
`lib.rs:320-346` shows the backend's epoch guard already makes the old child's reader **exit in
silence — `NO emite fg-exit`**. The old child therefore cannot produce the event the window exists to
swallow. The only `fg-exit` those 600 ms can ever eat is the **new** child's. A belt-and-suspenders
guard against an impossible event, catching the real one instead.

**Both of the operator's symptoms are this single defect.** The restart *did* fire; it killed the old
child (`lib.rs:399-407`) and spawned a new one that died fast. From the operator's seat a process that
is killed and whose replacement dies immediately is indistinguishable from *"no reinició"*. The
launcher then wedged, which is *"no dejó apagarlo aun con el FG apagado"* — the FG was indeed off, and
the launcher was asserting otherwise.

**What is still UNVERIFIED: why his new child died.** The wedge requires a death inside ~600 ms, so
one occurred, but this session has not reproduced it and does not claim a cause. Two fast fatal exits
are quoted in the tree and are candidates, neither confirmed for this run:
`src/core/core_init.cpp:96` — `need primary GPU (LUID) + assist discrete (or --force-single-gpu).
B=MISSING` → `return 1`, and `src/core/main.cpp:208` — `return cfg.parse_failed ? 2 : 0`, which
`--fg-gpu`'s own validator reaches at `cli.cpp:503` (`unknown '%s' (auto|primary|assist)`). Settling
it costs one console run of `phyriad_fg.exe` with the exact value he selected.

**Fix, from the agents and unapplied.** Make the guard child-scoped rather than time-scoped: put the
child's epoch in the `fg-exit` payload and ignore only events older than the epoch the restart just
created. The one-line stopgap that also removes the dead end:
`setTimeout(async () => { restarting = false; setRunning(await invoke("is_running")); }, 600)`.

---

## Status table

| id | reported | traced | fixed | blocked on |
|---|---|---|---|---|
| D-1 window identity | operator | yes, 1 site | no | `--window` is a product-visible contract |
| D-2 click geometry | operator | yes, 3 sites | no | it is the default present path |
| D-3 GPU change / stuck Stop | operator | **yes** — 3 dimensions + checked here | no | a one-line stopgap exists; the fix is the operator's call |

**Update 2026-09-06, same day:** the audit (`wf_d4d5ef46-620`) returned **40 CONFIRMED findings, 8 of
them high**, of which these three are D-1, D-2 and D-3. The other 37 — including a real out-of-range
index at `capture_init.cpp:402` and four config controls whose two UI switches can silently disagree —
are inventoried verbatim in [`QOL_AUDIT_2026-09-06.md`](QOL_AUDIT_2026-09-06.md). This file stays the
short list of what the OPERATOR hit himself.

*Made with my soul - Swately <3*
