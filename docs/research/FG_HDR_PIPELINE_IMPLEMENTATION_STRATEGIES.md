# FG_HDR_PIPELINE_IMPLEMENTATION_STRATEGIES — concrete edit sites for the A3 HDR levers

> **Diátaxis type:** Planning / design (Explanation). **Status:** `designed`. **Plan tier:** Tier 2 — sibling
> of [`FG_HDR_PIPELINE_MASTER_PLAN.md`](FG_HDR_PIPELINE_MASTER_PLAN.md) and
> [`FG_HDR_PIPELINE_RISK_REGISTER.md`](FG_HDR_PIPELINE_RISK_REGISTER.md). Each L4 edit site below cites its
> mitigating risk **ID** (RISK_REGISTER §2.3 traceability requirement: edit → risk).
>
> **Verifiable-reference mandate (FDP).** Every `path:line`/API re-verified first-hand this session. **Every
> new flag is default-off and byte-identical-off** (the C2 dogma): with the flag absent, the code path is
> unreachable and the bytes presented are identical to the current build.
>
> **Normativity (BCP 14):** MUST / MUST NOT / SHOULD / MAY per RFC 2119/8174.

---

## §0 — The discipline that binds every strategy

1. **Default-off, byte-identical-off.** A new flag's default leaves the existing code path literally
   unentered (an `if (cfg.X)` guard, X defaulting to the current value). Validated by a `--qdump`/`--outdump`
   diff = 0 vs the pre-change build.
2. **Verify the site before editing.** The line numbers below are this-session-verified; re-grep before
   touching (the dossier's numbers had already drifted once — MASTER_PLAN §6).
3. **Soft, not hard, on a platform refusal.** Any new format request (capture format, swapchain format,
   colorspace) MUST degrade to the current path if the API rejects it — never bail the process.
4. **Cite the risk ID** at each L4 edit site (Tier-2 traceability).

---

## §1 — A3-L1 — `SetColorSpace1(sRGB)` on present (T0) — ALREADY SHIPPING

No work. Recorded for completeness + to correct the dossier. Implemented at:
- `framework/render/present/src/PresentSurface.cpp:382-394` — QI `IDXGISwapChain3`, `CheckColorSpaceSupport`,
  `SetColorSpace1(DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709)` gated on `desc.present_colorspace != 0`.
- field `PresentSurfaceDesc::present_colorspace` `framework/render/present/include/phyriad/render/present/PresentSurface.hpp:95`.
- flag `--present-colorspace [srgb|off]` `apps/render_assistant/src/main.cpp:1149`; plumbed `:6979`.

Byte-identical at the default 0 (the `if (desc.present_colorspace != 0)` guard). **Status `shipping`.**

---

## §2 — A3-L2 — parameterize WGC ingest off the BGRA8 lock (T1, phase P-HDR-1)

**Goal.** When the captured surface is HDR, request `R16G16B16A16Float` from WGC instead of the forced
BGRA8, so the FP16 highlights reach `route_for`'s RT_HDR branch and `hdr_convert.comp` fires under
production capture (not only the DD fallback).

**Edit sites (verified first-hand):**

1. **The hard lock** — `apps/render_assistant/src/main.cpp:3098`:
   ```cpp
   d.fmt=DXGI_FORMAT_B8G8R8A8_UNORM; // WGC always delivers BGRA8   ← the lock
   ```
   Replace with a conditional: when `cfg.hdr_ingest` is set AND the captured output reports an HDR colorspace
   (probe `IDXGIOutput6::GetDesc1().ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020`, dossier §2(e)1),
   set `d.fmt = DXGI_FORMAT_R16G16B16A16_FLOAT`. Default (`cfg.hdr_ingest==0`) keeps the literal BGRA8 line →
   byte-identical.

2. **The WGC frame-pool format** — the `Direct3D11CaptureFramePool::CreateFreeThreaded` call takes a
   `DirectXPixelFormat` (today `B8G8R8A8UIntNormalized`, `main.cpp:4050-4051`, in the `#ifdef _MSC_VER` WGC
   path; the pool member is declared at `:3015`). Pass the chosen format there. **SOFT:** if
   `Create` throws / returns an HRESULT failure on R16F, catch it and fall back to BGRA8 + clear `hdr_ingest`
   (the format-allow-list is `[V2]`, not guaranteed — MASTER_PLAN §6). → **see RISK_REGISTER FR-HDR-1.**

3. **New flag** — add `--hdr-ingest [on|off]` in the `parse_extra` chain (NOT the main parse chain — the
   main chain hits C1061; the A3-L1 flag at `main.cpp:1149` is the pattern to copy), default off. Add the
   `cfg` field next to `present_colorspace` (`main.cpp:122`).

**Why no qualifying risk on its own:** the request is soft (FR-HDR-1 mitigates the refusal); it changes only
the *capture* format, which already flows through the existing `route_for`/`IS_HDR` branch
(`main.cpp:1823-1828` / `:3103`) that handles `R16G16B16A16_FLOAT` today via the DD path.

**Validation:** with `--hdr-ingest off` → `--qdump` diff = 0 vs pre-change. With `--hdr-ingest on` on a real
HDR game → the `--qdump` dump shows pre-tonemap values >1.0 (the highlights reached the warp). Half of A3-1.

---

## §3 — A3-L3 — rolloff + dither replacing the hard clip (T1, phase P-HDR-1)

**Goal.** Replace `min(c, 1.0)` (which piles every highlight at 255) with a highlight rolloff (Reinhard or
ACES — MS gives Reinhard verbatim, dossier §2) + ordered dither before the 8-bit store, so highlights are
*distinguishable*, not flat-clipped. **Both shaders** carry the clip and BOTH MUST change:

1. **`apps/render_assistant/shaders/hdr_convert.comp:32`** — verified verbatim:
   ```glsl
   c = min(c, vec3(1.0));   ← replace
   ```
   with a rolloff, e.g. Reinhard extended `c = c / (c + vec3(1.0))` (or ACES-fitted), then ordered dither
   before `srgb_oetf`. Gate the new path on a new push-constant `tonemap_mode` (0 = current `min` clip,
   1 = rolloff) so default-off is the *identical* `min` instruction → byte-identical. The push block is
   `PC { uint is_hdr; float exposure; }` (`hdr_convert.comp:15`) — extend it; update the push at
   `main.cpp:5214` (`pcv{IS_HDR?1u:0u,1.f}` — verified) to pass `tonemap_mode`.

2. **`apps/render_assistant/shaders/igpu_convert_pack.comp:46`** — verified verbatim:
   ```glsl
   c = min(max(c, vec3(0.0)) * expo, vec3(1.0));   ← replace the min(...,1.0) factor
   ```
   the same rolloff + dither, gated on the same mode push-constant. The push block is at
   `igpu_convert_pack.comp:32`; the push site is `main.cpp:5243` (`pcg{...,IS_HDR?1u:0u,1.f}` — verified).

**The dither.** An ordered (Bayer 8×8) or blue-noise dither before quantizing to 8-bit RGBA — prevents the
banding the rolloff would otherwise introduce in the now-compressed highlight range. The store stays
`rgba8` (the shaders write `image2D` rgba8 — `hdr_convert.comp:14`); this lever does NOT widen storage (that
is L4).

**Why no qualifying risk on its own:** a pure shader-math change inside the existing `is_hdr != 0` branch;
no new resource, no swapchain change, no cross-device sync. The mode push defaults to the current `min`.

**Validation:** `tonemap_mode 0` → byte-identical. `tonemap_mode 1` on a real HDR game → the `--qdump`
*output* histogram shows NO 255 pile-up spike (rolloff, not clip). **This completes A3-1** (with L2).

---

## §4 — A3-L4 — FP16 scRGB present (T2, phase P-HDR-2) — the risk-bearing lever

**Goal.** Present an `R16G16B16A16_FLOAT` scRGB swapchain so an HDR display receives HDR (Option 1, the
ONLY HDR present path for an alpha-premult overlay — MASTER_PLAN §1). **This is the Tier-2 phase; each edit
cites its RISK_REGISTER ID.**

**Edit sites (verified first-hand):**

1. **Swapchain format** — `framework/render/present/src/PresentSurface.cpp:326`:
   ```cpp
   sd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;   ← conditional on a new desc field
   ```
   When `desc.present_format == FP16` set `sd.Format = DXGI_FORMAT_R16G16B16A16_FLOAT`. Default (BGRA8)
   keeps the literal line. Add `uint8_t present_format = 0;` to `PresentSurfaceDesc` (next to
   `present_colorspace`, `PresentSurface.hpp:95`) — re-check the `_pad`/`static_assert` POD layout
   (`PresentSurface.hpp:96,107-108`). → **CR-HDR-1** (swapchain create-failure on FP16 → device-loss/bail).

2. **The colorspace** — with FP16 the default treatment is scRGB
   (`DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709`, dossier §2 Option 1). Extend the existing A3-L1 block
   (`PresentSurface.cpp:386-394`): when `present_format==FP16`, set `cs = ...G10_NONE_P709` (linear scRGB)
   instead of the sRGB `...G22_NONE_P709`. Keep the `CheckColorSpaceSupport` soft-skip. → **FR-HDR-2**
   (colorspace unsupported on the display → present washed/wrong but no crash).

3. **The shared-texture bridge format** — THE crash-class edit. The submit path is "one CopyResource into
   the backbuffer" (`PresentSurface.hpp:139-149`); the producer's warp output `wapOutA` is `R8G8B8A8_UNORM`
   (`main.cpp:4381`). A `CopyResource` from an `R8G8B8A8_UNORM` source into an `R16G16B16A16_FLOAT`
   backbuffer is **a format mismatch → `CopyResource` is a no-op/validation error / undefined** (D3D11
   requires copy-compatible formats). The producer's bridge texture (the VK→D3D11 shared texture) MUST be
   created `R16G16B16A16_FLOAT` when the present is FP16, AND the warp must write FP16 (or the copy must
   become a format-converting blit). → **CR-HDR-2** (format-mismatch copy → corrupt/no present),
   **CR-HDR-3** (keyed-mutex/shared-handle format renegotiation → device-loss).

4. **Swapchain-recreate path** — `PresentSurface.cpp:200-209` (`resize_swapchain`, the OA-1 re-seat) calls
   `ResizeBuffers(0,0,0,DXGI_FORMAT_UNKNOWN,0)` — `DXGI_FORMAT_UNKNOWN` preserves the existing format, so an
   FP16 swapchain stays FP16 across a re-seat (verified the call preserves format). Confirm no path hardcodes
   BGRA8 on re-create. → **CR-HDR-1** (re-seat at the wrong format).

5. **New flag** — `--present-fp16 [on|off]` in `parse_extra`, default off; the `cfg` field + the plumb at
   `main.cpp:6979` (next to `present_colorspace`). Default off → `desc.present_format==0` → the literal BGRA8
   swapchain → byte-identical.

**The math stays FP32 (the floor honored).** L4 widens the *present* and the *bridge texture*, NOT the warp
arithmetic. Warp output sampled to FP32, written to an FP16 texel — no FP16 *math*, so no Pascal 1/64
penalty (dossier §2 nuance: "FP16 *texels sampled to FP32* carry no penalty; only FP16 *arithmetic* does").

**Validation:** `--present-fp16 off` → byte-identical. `--present-fp16 on` → the §5 crash gate (RISK_REGISTER)
THEN A3-2 (HDR display: no washout + the swapchain reports `...G10_NONE_P709`).

---

## §5 — Linear-light compositing (A3-L5, T1, measure-gated) — OPTIONAL, deferred

The dossier's lever 5 (decode sRGB→linear before matte/crossfade/disocclusion blends, re-encode after —
`GAP §2 #20`) is **out of A3's closeable scope** (C4 is ingest+present, not blend accuracy) and is a hot-path
change → **measure-gated, deferred, not in P-HDR-1/2.** Listed so it is not lost; it belongs to the A3
asymptotic tail (wide-gamut interpolation accuracy), not the binary crossing. Do NOT implement without a
measured before/after on the high-contrast-moving-edge case (where camera-twarp operates) — the efficiency mandate.

---

## §6 — Strategy → gate map

| Strategy | Lever | Tier | Phase | Gate to pass | Risk IDs |
|---|---|---|---|---|---|
| §1 | A3-L1 | T0 | DONE | (shipping) | — |
| §2 | A3-L2 | T1 | P-HDR-1 | byte-id-off; >1.0 reaches warp | FR-HDR-1 |
| §3 | A3-L3 | T1 | P-HDR-1 | byte-id-off; A3-1 (no 255 pile-up) | — |
| §4 | A3-L4 | **T2** | P-HDR-2 | byte-id-off; crash gate; A3-2 | CR-HDR-1/2/3, FR-HDR-2 |
| §5 | A3-L5 | T1 | deferred | measured before/after (efficiency) | — |

All risk IDs are detailed in [`FG_HDR_PIPELINE_RISK_REGISTER.md`](FG_HDR_PIPELINE_RISK_REGISTER.md). No L4
strategy may be committed while any of its risks is `open` (Tier-2 gate).
