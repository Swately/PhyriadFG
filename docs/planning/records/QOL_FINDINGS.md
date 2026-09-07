# QoL findings — operator's own use of the launcher + FG

**Opened** 2026-09-06, during the operator's first hands-on session with the post-R7 build
(`ui.exe` rebuilt with the `maid` skin, `phyriad_fg.exe` of today beside it).

This record is the *defect ledger* for quality-of-life failures found by USING the product, as
opposed to the convergence gates, which measure the pipeline. Each entry states who reported it,
what is **traced with evidence** and what is still **unverified**. An entry is never promoted from
reported to traced without quoted source lines.

Three entries are open. **None is fixed** — D-1 and D-2 land on the default present/capture path
and wait on the operator's word; D-3 is one hour old and deliberately un-investigated.

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

**Status: REPORTED, NOT INVESTIGATED.** The operator's instruction was explicit — *"anotalo y
esperemos a los agentes"* — so nothing here is traced. The single fact established is *where* the
mechanism lives, from one grep, and it is recorded as a starting point and not as a diagnosis:

`ui/src/main.js:24-26,1011-1032` holds `autoRestart` / `restarting` / `restartTimer`, the change
listener, a `if (!autoRestart || !running) return;` guard and a 1 s debounce before `doRestart`.
Whether the GPU control is wired into that listener at all, and whether `running` is what went
stale, is **unmeasured**.

This lands on the `launcher` dimension of the audit workflow already in flight (run
`wf_d4d5ef46-620`), whose prompt asks *"whether the launcher's UI state can disagree with reality
(a Stop button for a dead process, a running pill for a crashed FG, an orphan process)"* — i.e. the
second symptom is exactly the failure that dimension was sent to look for. Cross-check this entry
against its confirmed findings before opening any code.

---

## Status table

| id | reported | traced | fixed | blocked on |
|---|---|---|---|---|
| D-1 window identity | operator | yes, 1 site | no | `--window` is a product-visible contract |
| D-2 click geometry | operator | yes, 3 sites | no | it is the default present path |
| D-3 GPU change / stuck Stop | operator | **no, by instruction** | no | the audit workflow's `launcher` dimension |

*Made with my soul - Swately <3*
