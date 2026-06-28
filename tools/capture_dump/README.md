# capture_dump — the digital screen-capture tap for the FG-quality test-field

> Standalone Windows tool: the digital capture tap. It captures a window or a
> monitor and dumps every presented frame as RGBA8 `.rgba` + a manifest the
> `fg_quality_scorer` consumes. Not wired into any CMake / CI. No Vulkan — pure
> capture.

## Why this exists

The canonical use is to capture **LSFG's output window** — a black-box external
frame-gen tool we have no code or metrics for — so we can score *its* presented
frames (including its interpolated ones) with our own scorer. The capture is the
source's **present cadence**: a 240 Hz panel showing LSFG output → ~240 distinct
frames/s, and **no de-duplication** is done (we want every presented frame, the
interpolated ones included). The same tap captures PhyriadFG's own output, or any
other window/monitor, for an apples-to-apples FG-quality A/B.

## CLI

```
capture_dump --window "<title substr>" --frames N --out <dir> [--api wgc|dd]
capture_dump --monitor M --frames N --out <dir> [--api dd|wgc]
```

| flag | meaning |
|---|---|
| `--window SUBSTR` | Capture the first visible window whose title contains `SUBSTR` (mirrors `find_window_by_substr`). **Implies `--api wgc`** — a window can only be item-captured via WGC. |
| `--monitor M` | Capture DXGI display output `M` (default 0). Implies `--api dd` unless `--api wgc` is also given (WGC can capture a monitor too, via `CreateForMonitor`). |
| `--frames N` | Number of frames to capture (default 120). |
| `--out DIR` | Output directory (created if absent). Default `capture`. |
| `--api wgc\|dd` | Capture backend. **Default `wgc`** — Windows.Graphics.Capture handles flip-model / borderless / overlay present paths (the LSFG-output case). `dd` = DXGI Desktop Duplication (whole monitor). |

`--api wgc` requires the **MSVC + C++/WinRT** build; the MinGW build compiles the
DXGI-DD path only and errors at runtime if `--api wgc` is requested.

## Output — the `fg_quality_scorer` `.rgba` contract

- `<dir>/cap_%06d.rgba` — raw **RGBA8**, row-major, `W*H*4` bytes, **no header**
  (the same format `stage11`/`stage31`/`fg_quality_scorer` load).
- `<dir>/manifest.txt`:

  ```
  # capture_dump — PhyriadFG FG-quality test-field tap
  # api=wgc target=Lossless target=... adapter=...
  size <W> <H>
  0 <qpc_ms> cap_000000.rgba
  1 <qpc_ms> cap_000001.rgba
  ...
  ```

  A `size W H` line (one per file) + one line per captured frame:
  `<capture-index> <high-res QPC timestamp in ms> <filename>`. The timestamp is
  `QueryPerformanceCounter / QueryPerformanceFrequency * 1000`, taken at the
  moment the frame is mapped (consumed), so consecutive deltas give the observed
  capture cadence.

> Note on consuming this with `fg_quality_scorer`: that tool's manifest grammar
> is `triple`/`preset` lines (held-out N/N+1/N+2). `capture_dump` emits a **flat
> per-frame** manifest (it is a raw capture, it does not know which frames form a
> held-out triple). To score, build the triples from this dump — e.g. take
> consecutive `cap_i / cap_{i+1} / cap_{i+2}` as `prev/mid/next` — and write a
> `triple` manifest pointing at these `.rgba` files. The `size W H` line and the
> raw-RGBA8 byte layout are identical, so the files load directly.

## Channel order — the B/R swap (load-bearing)

WGC always delivers **BGRA8** (`B8G8R8A8UIntNormalized`), and DXGI DD on the
desktop is also typically `B8G8R8A8_UNORM`. The scorer's `stage11` loader expects
**RGBA8**. The tool therefore **swaps B↔R on write** (`row[x*4+0]=r;
row[x*4+1]=g; row[x*4+2]=b; row[x*4+3]=255`) and forces an opaque alpha — the
same swap `dump_bmp` does, but in the RGBA direction. If a DD output is already
`R8G8B8A8_UNORM`, the rows are copied straight through (no swap). Only 8-bit
BGRA/RGBA SDR targets are accepted; an HDR (FP16) or 10-bit output is rejected up
front (tone-mapping is deliberately out of scope — it would silently corrupt the
`.rgba`).

## Capturability test — the make-or-break

WGC/DXGI **cannot** capture some exclusive-fullscreen / protected
(`WDA_MONITOR`) present paths — the captured frame returns **all-black** or the
call fails. This is the whole point of the first-run test. The tool:

- accumulates a cheap subsampled **luma mean + variance** for every written
  frame (green-channel proxy, every 16th pixel of every 8th row);
- after capture, inspects the **first 10** frames: if **all** of them have
  `mean < 2.0` **and** `variance < 1.0` (luma 0..255), it prints a **LOUD**
  warning:

  ```
  WARNING: frames are all-black — target may use an uncapturable
           exclusive/protected present path; digital capture not
           possible for this target (camera fallback needed)
  ```

- **still writes** the (black) frames it got — it does **not** silently dump
  black frames as if they were valid, and it tells you not to score them;
- returns **exit code 2** in the all-black case, **3** if nothing was captured,
  **0** otherwise — so a script can detect the uncapturable target.

The threshold requires **both** low mean and low variance precisely so a
genuinely dark scene (which still has spatial variance) is **not** false-flagged;
a failed protected capture returns a uniform zero plane (mean≈0, var≈0).

## Run — capture an LSFG output window

```
# LSFG presents into its own borderless output window; capture 240 frames of it
capture_dump --window "Lossless Scaling" --frames 240 --out lsfg_cap
# (or by the game's window title if LSFG presents in-place)
```

Whole monitor (e.g. the panel LSFG outputs to), via Desktop Duplication:

```
capture_dump --monitor 0 --frames 240 --out mon0_cap --api dd
```

On success it prints the selected target (title/monitor + resolution), the api,
per-30-frame progress, and a final summary: frames written + the **mean
inter-frame ms** from the QPC timestamps (the observed capture cadence) — e.g.
`mean inter-frame: 4.167 ms (~240.0 fps observed capture cadence)`.

## Build discipline / caveats

- **Standalone** CMake build; links `d3d11 dxgi user32 gdi32` (+ `windowsapp` on
  MSVC for WGC). No Vulkan.
- WGC is **MSVC-only** (C++/WinRT + `windowsapp` + `/EHsc`); MinGW builds the DD
  path only.
- PhyriadFG uses **both** C++/WinRT *and* the interop COM path
  (`IGraphicsCaptureItemInterop::CreateForWindow/CreateForMonitor`,
  `IDirect3DDxgiInterfaceAccess` declared inline); this tool matches that
  pattern verbatim.
- `capture/`, `build-capture-dump/`, and `*.rgba` outputs are expected to be
  git-ignored (raw frame dumps; do not commit).

## Made with my soul - Swately <3
