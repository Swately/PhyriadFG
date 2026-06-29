# FG_HDR_FORMAT_PRIOR_ART — HDR / format completeness for an external-capture FG

> **Diátaxis type:** Analysis (SOTA dossier). **Status:** `measured` (the Windows HDR API behaviour +
> the field's posture) / `designed` (the PhyriadFG path). Resolves objective **A3** ("an HDR source passes
> through PhyriadFG without irreversible colour loss; present is colorimetrically correct on the target
> display"). Companion to [`PHYRIADFG_OBJECTIVE_VISTA.md`](PHYRIADFG_OBJECTIVE_VISTA.md) §1-A3.
>
> **Provenance + verification.** From workflow `w83r5ydl9` (the HDR agent). **The two design-load-bearing
> claims — which overturn the gap-audit's "10bpc is the cheap target" — were re-verified FIRST-HAND by the
> author this session** (`[VS]`, primary URLs). Other claims carry the sweep's levels. Per FDP §2: no
> fabricated citation; §6 lists what could not be verified.
>
> **Normativity (BCP 14):** MUST / MUST NOT used only where a real constraint binds.

---

## §1 — PhyriadFG current standing (first-hand, in-repo)

Better than `GAP §3.12` implies, but still **an 8-bit SDR pipeline that recognises HDR formats and
tone-maps-to-SDR-by-clip:**

- A **format router exists**: `route_for()` maps `R16G16B16A16_FLOAT → RT_HDR`, `R10G10B10A2_UNORM →
  RT_10`, BGRA8/RGBA8 → `RT_PASS` (`main.cpp:1778-1787`); `IS_HDR=(route==RT_HDR)` (`:3058`).
- An **HDR-ingest tone-map shader exists** but **hard-clips**: `hdr_convert.comp` does
  `c=max(c,0)*exposure; c=min(c,1.0); c=srgb_oetf(c)` — the `min(c,vec3(1.0))` (`:32`) **destroys all
  >1.0 (>80-nit) highlight detail before anything downstream sees it.** Mirrored in
  `igpu_convert_pack.comp` (the iGPU DD-HDR path).
- **WGC ingest is hard-locked BGRA8:** `d.fmt=DXGI_FORMAT_B8G8R8A8_UNORM` is forced at `main.cpp:3053`
  regardless of source → under production capture HDR **never even reaches** `route_for`'s RT_HDR branch
  (only the DD fallback can deliver FP16 today).
- **Present is hard-locked SDR, no colorspace:** `sd.Format = DXGI_FORMAT_B8G8R8A8_UNORM`
  (`PresentSurface.cpp:206`); **zero** `SetColorSpace1`/`SetHDRMetaData` in `framework/render/present`.
- All warp/blend math runs in **gamma space** (sRGB-encoded 8-bit), not linear (`GAP §2 #20`).

---

## §2 — SOTA findings (leveled)

### The Windows HDR present pipeline — the two canonical options (`[VS]`)

Microsoft exposes exactly two Advanced-Color present configurations (MS Learn, *Use DirectX with Advanced
Color*, https://learn.microsoft.com/en-us/windows/win32/direct3darticles/high-dynamic-range — author-fetched):

- **Option 1 — FP16 scRGB:** `DXGI_FORMAT_R16G16B16A16_FLOAT`, default-treated as
  `DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709` (scRGB, linear, BT.709, values outside [0,1] legal). Verbatim:
  **"the only option that works for all types of Advanced Color displays, content, and rendering APIs"** and
  it "provides you with the numeric range and precision to specify any physically possible color, and
  perform arbitrary processing **including blending**." Cost **64 bpp** (2× bandwidth/memory). This is the
  DWM's own composition format (CCCS).
- **Option 2 — UINT10/HDR10:** `DXGI_FORMAT_R10G10B10A2_UNORM`, default-treated as sRGB, so you MUST call
  `IDXGISwapChain3::SetColorSpace1(DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020)`. **32 bpp.** Verbatim
  constraint — **available "only if your app meets ALL of the following conditions: Targets an HDR display;
  Uses Direct3D 12 or Direct3D 11; Swap chain doesn't require blending with alpha/transparency."**

> **★ Load-bearing (overturns `GAP §2 #10`):** the present surface is a **DComp composition swapchain with
> `DXGI_ALPHA_MODE_PREMULTIPLIED`** (`PresentSurface.cpp:222`). MS Option-2 **explicitly excludes
> alpha-blended swapchains** → **a DComp overlay that needs alpha CANNOT use the cheap 10bpc HDR10 path; it
> MUST use Option 1 (FP16 scRGB).** The gap-audit's "10bpc R10G10B10A2 is the SOTA-cheapest target" is
> wrong for THIS surface.

Also confirmed in the same doc `[VS]`: SDR-on-Advanced-Color downconvert = **numeric clipping** of values
outside [0,1]; reference white scRGB(1,1,1)=80 nits, adjust SDR by `SdrWhiteLevelInNits/80` (obtain via
`DISPLAYCONFIG_SDR_WHITE_LEVEL`/`QueryDisplayConfig`); GPU support floor = NVIDIA Pascal (GeForce 10)+ /
AMD Polaris (RX 400)+ / Intel Ice Lake+. A simplified **Reinhard tonemapper** is given verbatim.

### WGC HDR capture

- **`[VS]`** "APIs that support specifying pixel formats, such as `Windows.Graphics.Capture`, and
  `IDXGIOutput5::DuplicateOutput1`, provide the capability to capture HDR and WCG content without losing
  pixel information" (same MS doc). `Direct3D11CaptureFramePool.Create` takes a `DirectXPixelFormat`
  argument → **the capture format is a caller choice, not fixed to BGRA8.** WGC delivers HDR as scRGB FP16.

### What the field does for HDR

- **`[VS]` DLSS-G (NVIDIA Streamline):** "DLSS-G currently does **NOT** support FP16 pixel format and scRGB
  color space because it is too expensive in terms of compute and bandwidth cost… If your game supports HDR
  please make sure to use **UINT10/RGB10 pixel format and HDR10/BT.2100 color space**."
  (https://github.com/NVIDIA-RTX/Streamline/blob/main/docs/ProgrammingGuideDLSS_G.md — author-fetched)
- **`[V1]` XeSS-FG (Intel):** identical posture — "supports HDR with `R10G10B10A2_UNORM`… does not support
  the FP16 HDR format and scRGB color space."
- **`[V2]` FSR3-FG:** AMD ships **FidelityFX LPM** (Luma-Preserving Mapper) as the HDR tone/gamut path; no
  verbatim AMD scRGB-FP16 refusal found, so none claimed.
- **`[V2/V3]` LSFG:** ships an explicit **HDR Support toggle** ("uses more VRAM"; required when game+monitor
  output HDR, or with DXGI capture + Win11 "Automatically manage color"). The "auto tone-map" mechanism is
  `[V3]` inference from observed symptoms (the only model consistent with the MS reference-white behaviour);
  LSFG has no public technical doc.

### Pascal FP16 constraint

- **`[V1/V2]`** GP102 (1080 Ti) runs FP16 at **1/64 of FP32** (~177 GFLOPS FP16 vs ~11.3 TFLOPS FP32). So an
  internal scRGB-FP16 *compute* pipeline (flow + warp) on the 1080 Ti is a measured non-starter — exactly
  why NVIDIA/Intel refuse internal scRGB-FP16 even on tensor HW; on Pascal it is 64× worse. **Nuance:** this
  constrains the *internal working math*, NOT the *present/ingest format* — FP16 *texels sampled to FP32*
  carry no 1/64 penalty; only FP16 *arithmetic* does.

---

## §3 — The objective resolved (A3)

One objective, three separable sub-fronts: **(i) ingest** (can HDR enter), **(ii) internal handling** (is
it irreversibly clipped), **(iii) present** (shown colorimetrically correct).

- **(a) Perfection target.** HDR enters at full precision (scRGB FP16 from WGC/DD), survives flow/warp with
  **no irreversible [0,1] clip**, and presents either (1) to an HDR display in HDR10 or scRGB FP16 with
  correct colorspace + reference-white, or (2) tone-mapped to SDR with a highlight rolloff + dither — never
  a hard clip. On an SDR Advanced-Color desktop, the overlay is not washed out (correct `SetColorSpace1`).
- **(b) Honest floor.** Internal scRGB-FP16 *math* is foreclosed (field consensus + Pascal 1/64) →
  reachable target = FP16 ingest+storage+present with the interp math quantized. **10bpc HDR10 present is
  blocked for the alpha overlay `[VS]`** → the available HDR present is Option 1 (64 bpp). HDR→SDR is
  inherently lossy. Exclusive-fullscreen HDR + HDCP is MPO/DRM-walled (shared with LSFG). Wide-gamut
  *interpolation* (blending in correct linear BT.2020) is an open frontier for the whole external class.
- **(c) Standing.** §1: recognises HDR formats, hard-clips them, and even that path is unreachable under
  WGC (BGRA8 forced). The routing skeleton exists; no HDR-preserving path end-to-end.
- **(d) LSFG-specific gap.** **We LOSE on the headline feature.** LSFG ships a working HDR toggle that
  preserves HDR end-to-end + corrects SDR-on-HDR brightness; PhyriadFG hard-clips at ingest and can't even
  ingest under WGC. On an HDR rig today PhyriadFG produces a washed-out/clipped overlay; LSFG correct HDR.
  **A binary feature gap, not a quality-degree gap.** We match only in the degenerate SDR-source/SDR-display
  case.
- **(e) Closing levers (ordered by value):**
  1. **`SetColorSpace1` on present (T0–T1)** — query `IDXGIOutput6::GetDesc1().ColorSpace`; on an
     Advanced-Color desktop set the right colorspace so the BGRA8 overlay is **not washed out** (fixes a
     *current* defect even for SDR; `GAP §3.12 (d)`).
  2. **Stop forcing BGRA8 on the WGC path (T1)** — request `R16G16B16A16Float` when the captured surface is
     HDR (detect via `IDXGIOutput6`); route through the existing RT_HDR branch so `hdr_convert.comp` fires.
  3. **Replace the hard clip with highlight-rolloff + dither (T1)** — swap `min(c,1.0)` for a
     Reinhard/ACES/BT.2390 rolloff (MS gives Reinhard verbatim) + ordered dither before the 8-bit store.
  4. **True HDR present — FP16 scRGB, NOT 10bpc (T2)** — because the surface is alpha-blended DComp, the
     correct HDR present is Option 1: present `R16G16B16A16_FLOAT` with default scRGB, warp-sample to FP32
     (no FP16 math → no Pascal penalty) + reference-white handling. **The gap-audit's "10bpc" prescription
     is rejected here.**
  5. **Linear-light compositing for blends (T1, measure-gated)** — decode sRGB→linear before
     matte/crossfade/disocclusion blends, re-encode after (`GAP §2 #20`); largest error on high-contrast
     moving edges (where camera-twarp operates) → hot-path, gate on measurement.
- **(f) ★ Closeable success criterion (a staged binary ladder):**
  > **A3-1 (binary — "no irreversible clip"):** on a real HDR game (HDR on in-game + Windows HDR on),
  > capturing via WGC, a known >1.0-luminance region (sun/muzzle flash, via `--qdump`/`dump_rgba`) reaches
  > the warp un-clipped (pre-tonemap values >1.0 in the captured buffer) and post-tonemap highlights are
  > **rolled off, not flat-clipped** (the output histogram shows no pile-up spike at 255). Independent of
  > the present display.
  > **A3-2 (binary — "correct present on an HDR display"):** the overlay shows no washout/banding side-by-
  > side with the game + LSFG; objective check = the present swapchain reports an HDR colorspace, not
  > default sRGB.
  > **Asymptotic tail** (never "done"): wide-gamut *interpolation* accuracy (blending in perfect BT.2020
  > linear) — a frontier for the whole external class. Metric: **highlight-retention ratio** = (>1.0-scRGB
  > captured pixels that survive to present with distinguishable values) / (clipped). Today = **0**; A3-1
  > moves it off zero; A3-2 drives it toward 1.0. Computable from `dump_rgba` histograms, no hardware.

---

## §4 — Build sites (first-hand, for whoever un-freezes implementation)

`route_for` `main.cpp:1778-1787`; `IS_HDR` `:3058`; **WGC BGRA8 force `:3053`** (lever 2);
`hdr_convert.comp:32` **hard clip** (lever 3) + `igpu_convert_pack.comp`; `PresentSurface.cpp:206`
**BGRA8 present / no SetColorSpace1** (levers 1, 4); blend math in gamma space (`GAP §2 #20`, lever 5).

---

## §5 — Sources (leveled)

`[VS]` author-fetched: MS *Use DirectX with Advanced Color*
(learn.microsoft.com/.../direct3darticles/high-dynamic-range) — the two options, the alpha-exclusion
constraint, the reference-white math, the Reinhard operator, WGC HDR-capture; NVIDIA Streamline DLSS-G
guide (github.com/NVIDIA-RTX/Streamline/.../ProgrammingGuideDLSS_G.md) — the FP16/scRGB refusal.
`[V1/V2/V3]` from the sweep: Intel XeSS-FG developer guide; GPUOpen LPM; the GP102 1/64-FP16 figure
(Beyond3D); LSFG HDR-toggle behaviour (sageinfinity LS settings, community).
In-repo: `apps/render_assistant/src/main.cpp`, `apps/render_assistant/shaders/hdr_convert.comp`,
`framework/render/present/src/PresentSurface.cpp`, `PHYRIADFG_ARCHITECTURE_GAP_AUDIT.md §3.12 / §2 #10/#19/#20`.

---

## §6 — What could NOT be verified (first-hand)

- **LSFG's internal working/present format** — closed source; "auto tone-map" is `[V3]` inference; the
  *fact* of an HDR toggle + its VRAM cost is `[V2]`.
- **A first-party MS enumeration of the exact allowed `Direct3D11CaptureFramePool.Create` pixel formats** —
  the recommendation to use R16F in the HDR pool is `[VS]`, but a hard "only these N formats" allow-list was
  not found ("WGC accepts R16F" is therefore `[V2]`, strongly corroborated).
- **FSR3-FG's explicit scRGB-FP16 stance** — no verbatim AMD refusal found; only that AMD's HDR path is LPM.
- **Whether the DComp premultiplied-alpha overlay can use 10bpc via any sanctioned workaround** — MS
  excludes alpha-blended swapchains `[VS]`; treated as blocked (could be wrong if a non-alpha visual path
  exists — untested).
