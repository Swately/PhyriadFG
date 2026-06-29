# FG_HDR_PIPELINE_MASTER_PLAN — ingest + present HDR without an irreversible clip (A3 / fixed-point C4)

> **Diátaxis type:** Planning / design (Explanation). **Status:** `designed` (this is a plan, not shipped;
> the per-lever standing is tagged inline — `shipping` for A3-L1, `designed` for L2/L3/L4).
> **Plan tier:** **Tier 2 (risk-bearing)** per [`PLAN_TIER_PROTOCOL.md`](../canon/PLAN_TIER_PROTOCOL.md)
> §1.1 trigger 1 (crash / device-loss): the L4 FP16-scRGB present path re-creates the swapchain at a new
> format AND re-formats the cross-device shared-texture bridge, where a format mismatch can yield
> `VK_ERROR_DEVICE_LOST` / a present failure. The Tier-1 levers (L2, L3) carry no qualifying risk on their
> own, but the family ships as one risk-bearing set → the triad is mandatory.
>
> **Document set (mutually linked, §2.3 of the tier protocol):**
> - this MASTER_PLAN — the why, the walls, the architecture, the phases/gates;
> - [`FG_HDR_PIPELINE_IMPLEMENTATION_STRATEGIES.md`](FG_HDR_PIPELINE_IMPLEMENTATION_STRATEGIES.md) — the
>   concrete per-lever edit sites + the byte-identical-off discipline + validation;
> - [`FG_HDR_PIPELINE_RISK_REGISTER.md`](FG_HDR_PIPELINE_RISK_REGISTER.md) — every failure mode with its
>   mitigation AS CODE + first-hand verification (Tier-2 gate: no `open` risk at commit).
>
> **Backing dossier (SOTA, do not duplicate):** [`FG_HDR_FORMAT_PRIOR_ART.md`](FG_HDR_FORMAT_PRIOR_ART.md).
> **Objective constitution:** [`PHYRIADFG_OBJECTIVE_VISTA.md`](PHYRIADFG_OBJECTIVE_VISTA.md) §1-A3 + §5 floors.
>
> **Verifiable-reference mandate (FDP).** Every code `path:line`/API below was re-verified first-hand this
> session against the working tree; the dossier's line numbers had drifted (see §6) and are corrected here.
> **Normativity (BCP 14):** MUST / MUST NOT / SHOULD / MAY per RFC 2119/8174 only where a real constraint binds.

---

## §0 — Scope (and non-scope)

**In scope.** The objective family **A3 — HDR / format completeness**: let an HDR source enter PhyriadFG,
survive the pipeline without an *irreversible* [0,1] clip, and present colorimetrically correct (or
tone-mapped to SDR with a highlight rolloff, never a hard clip). The closeable target is fixed-point **C4**
(vista §2): *"HDR ingests without clip."* Concretely the staged binary ladder A3-1 / A3-2 (§3).

**Non-scope (NON-GOALS — stated, per FDP §4.4 / §3 planning-doc requirement):**
- **Internal scRGB-FP16 *arithmetic*** — a NAMED IRREDUCIBLE FLOOR (§1, vista §5), NOT planned. The plan
  carries FP16 *texels* (ingest/storage/present), sampled to FP32, never FP16 *math*.
- **10bpc HDR10 (Option 2) present** — STRUCTURALLY BLOCKED for this surface (alpha-blended DComp overlay;
  §1), NOT planned. The only HDR present path is Option 1 (FP16 scRGB), L4.
- **Wide-gamut interpolation accuracy** (blending in perfect BT.2020 linear) — an ASYMPTOTIC frontier for
  the whole external class (vista §5), NOT a closeable target; tracked as the asymptotic tail of A3, not a "done".
- **Exclusive-fullscreen HDR + HDCP capture** — MPO/DRM-walled for all OS capture (vista §5 floor, shared
  with LSFG), NOT planned.
- **Capping / pacing the captured game** — out of scope by the no-game-cap operator invariant
  (PROVENANCE, not a numbered CANON dogma). This pipeline is **read-only on the source**: it reads the
  captured surface and presents its own overlay; it never throttles, caps, or hooks the game. Structurally honored.

---

## §1 — The walls (irreducible; NAMED, never planned-to-beat)

These are the §5 floors of the vista, re-stated here so no phase silently re-proposes one as a win.

| Wall | Why irreducible | Source |
|---|---|---|
| **Internal scRGB-FP16 arithmetic** | NVIDIA (DLSS-G) + Intel (XeSS-FG) both refuse FP16/scRGB *compute* as too expensive; Pascal (1080 Ti / GP102) runs FP16 at **1/64 FP32** | dossier §2 `[V1/V2]`, vista §5 |
| **10bpc HDR10 present for an alpha overlay** | MS Option 2 (`R10G10B10A2` + `SetColorSpace1(G2084_P2020)`) is "available only if … the swap chain doesn't require blending with alpha"; our surface is `CreateSwapChainForComposition` with `DXGI_ALPHA_MODE_PREMULTIPLIED` (`PresentSurface.cpp:347`) | dossier §2 `[VS]`, vista §1-A3(d) |
| **HDR→SDR is inherently lossy** | tone-mapping a wider range into [0,1] discards information by definition; the lever is rolloff-not-clip, not zero loss | dossier §3(b) |
| **Exclusive-fullscreen HDR + HDCP** | bypasses DWM / DRM-walled → black for all OS capture | vista §5 `[V1]` |

The reachable target is therefore: **FP16 ingest + storage-as-needed + present, with the interp math
quantized** (8-bit today), **rolloff not clip** on the SDR path, and **Option-1 FP16 scRGB** on the HDR
present path. That is what C4 asks for — nothing in the closeable criterion requires crossing a wall above.

---

## §2 — Current standing (first-hand, corrected line numbers)

The dossier's standing is accurate in substance but its line numbers have drifted, and **lever 1 has since
been built** (the dossier predates it). Re-verified against the working tree this session:

- **Format router exists** — `route_for()` at `apps/render_assistant/src/main.cpp:1823-1828` maps
  `R16G16B16A16_FLOAT → RT_HDR (FP16 HDR scRGB)`, `B8G8R8A8/R8G8B8A8 → RT_PASS`; `IS_HDR=(route==RT_HDR)`
  at `:3103`. *(dossier said 1778-1787 / :3058 — drifted.)*
- **HDR-ingest tone-map shader exists but HARD-CLIPS** — `apps/render_assistant/shaders/hdr_convert.comp:32`
  `c = min(c, vec3(1.0));` destroys all >1.0 (>80-nit) highlight detail. Mirrored in the iGPU DD-HDR path
  `apps/render_assistant/shaders/igpu_convert_pack.comp:46` `c = min(max(c, vec3(0.0)) * expo, vec3(1.0));`.
- **WGC ingest is hard-locked BGRA8** — `main.cpp:3098` `d.fmt=DXGI_FORMAT_B8G8R8A8_UNORM; // WGC always
  delivers BGRA8` → under production WGC capture HDR never reaches `route_for`'s RT_HDR branch (only the DD
  fallback can deliver FP16 today). *(dossier said :3053 — drifted.)*
- **Internal warp/blend buffers are 8-bit** — `wapPrevA/wapCurA/wapOutA` are all `VK_FORMAT_R8G8B8A8_UNORM`,
  created at `main.cpp:4376` (wPrev=wapPrevA), `:4377` (wCur=wapCurA), `:4381` (wOut=wapOutA) — verified
  verbatim (the wPrev/wCur/wOut aliases are bound at `:4359-4363`). *(The dossier's `main.cpp:1811` cite is a
  COMMENT inside `dump_rgba()`, not the create — repointed to the real img_create sites.)* All warp/blend
  math runs in gamma-encoded 8-bit.
- **Present** — `PresentSurface.cpp:326` `sd.Format = DXGI_FORMAT_B8G8R8A8_UNORM`; the submit path is
  "one CopyResource into the backbuffer" (`PresentSurface.hpp:139-149`).
- **★ A3-L1 (SetColorSpace1) is ALREADY SHIPPING (default-off).** `PresentSurface.cpp:382-394` queries
  `IDXGISwapChain3`, `CheckColorSpaceSupport`, and calls `SetColorSpace1(DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709)`
  when `desc.present_colorspace != 0`; wired via `--present-colorspace srgb` (`main.cpp:1149`, field
  `PresentSurfaceDesc::present_colorspace` `PresentSurface.hpp:95`, plumbed `main.cpp:6979`). Byte-identical
  at the default 0. **The dossier's "zero SetColorSpace1 anywhere" standing is STALE** — lever 1 is done.

Net: lever 1 is `shipping`; levers 2-4 are `designed`. The highlight-retention ratio (dossier §3-f metric)
is **0** today and stays 0 until L2+L3 land (L1 alone fixes SDR-desktop washout, not HDR ingest).

---

## §3 — The closeable criterion (C4, the staged binary ladder — quoted from the dossier, not re-derived)

> **A3-1 (binary — "no irreversible clip"):** on a real HDR game (HDR on in-game + Windows HDR on),
> capturing via WGC, a known >1.0-luminance region (sun/muzzle flash, via `--qdump`) reaches the warp
> un-clipped and post-tonemap highlights are **rolled off, not flat-clipped** (the output histogram shows
> no pile-up spike at 255). Independent of the present display.
>
> **A3-2 (binary — "correct present on an HDR display"):** the overlay shows no washout/banding + the
> present swapchain reports an HDR colorspace, not default sRGB.

`--qdump DIR N` (the test tap) is verified present: `main.cpp:1379` (parse), `dump_rgba()` `main.cpp:1815`,
the readback wiring `main.cpp:3558`. **Metric:** highlight-retention ratio = (>1.0-scRGB captured pixels that
survive to present with distinguishable values) / (clipped); today **0**; A3-1 moves it off 0; A3-2 → toward 1.

---

## §4 — The lever ladder + phase plan (architecture)

The dossier orders the levers by value; this plan groups them into phases by tier and dependency. **Every
new flag is default-off and byte-identical-off** (CANON / DOGMA_SPECIFICATIONS; the §1.1-trigger-5 dogma).

| Lever | What | Tier | Phase | Standing |
|---|---|---|---|---|
| **A3-L1** | `SetColorSpace1(sRGB)` on present — SDR-on-Advanced-Color washout fix | T0 | DONE | `shipping` (`--present-colorspace`) |
| **A3-L2** | parameterize WGC ingest off the hard BGRA8 lock → request `R16G16B16A16Float` when the captured surface is HDR | T1 | P-HDR-1 | `designed` |
| **A3-L3** | replace the hard `min(c,1.0)` clip with a Reinhard/ACES rolloff + ordered dither (both `hdr_convert.comp` AND `igpu_convert_pack.comp`) | T1 | P-HDR-1 | `designed` |
| **A3-L4** | true HDR present — FP16 scRGB (Option 1, 64 bpp): present `R16G16B16A16_FLOAT` + default scRGB colorspace, warp-sample to FP32 | **T2** | P-HDR-2 | `designed` |

**Phase P-HDR-1 (Tier-1, closes A3-1).** L2 + L3 together. L2 unlocks the RT_HDR branch under WGC so the
>1.0 highlights *reach* the warp; L3 makes the SDR-store roll those highlights off instead of piling them at
255. Gate: **A3-1 binary passes** — a `--qdump` histogram on a real HDR game shows pre-tonemap >1.0 values
present AND no 255 pile-up spike. No qualifying risk on their own (L2 is a capture-format request that falls
back to BGRA8 on refusal; L3 is a shader-math swap behind the existing `is_hdr` gate) → no register entries
*caused* by L2/L3, but they are listed in the register as `accepted`-low for completeness.

**Phase P-HDR-2 (Tier-2, closes A3-2).** L4. Present an FP16 scRGB swapchain so an HDR display receives HDR.
This is the risk-bearing phase: it re-creates the swapchain at `R16G16B16A16_FLOAT` and must re-format the
cross-device shared-texture bridge (the producer's warp output `wapOutA` is `R8G8B8A8_UNORM` today —
`main.cpp:4381`).
**Every CR/FR in the RISK_REGISTER attaches here.** Gate: **A3-2 binary passes** + the §5 crash gate.

**Dependency:** P-HDR-1 → P-HDR-2 (A3-1 must be green — the highlights must survive ingest — before there is
anything HDR to present). L1 is independent and already shipped.

---

## §5 — Gates (per phase)

1. **Build-green** — `cmake --build` clean (MSVC), validation layers clean on a 30 s run.
2. **Byte-identical-off** — with every new HDR flag absent, the presented output is bit-identical to the
   current build (the C2 dogma; diff = 0 on a `--qdump`/`--outdump` capture). MANDATORY for each lever.
3. **A3-1 (P-HDR-1)** — `--qdump` histogram on a real HDR game: >1.0 pre-tonemap values present + no 255 pile-up.
4. **Crash gate (P-HDR-2 only)** — a 30 s run on a real HDR game with the watchdog clean, validation-layers
   clean, no `VK_ERROR_DEVICE_LOST` / present failure, with the FP16 path ON; AND a swapchain-recreate cycle
   (mode change) survives. This is the Tier-2 gate — see the RISK_REGISTER.
5. **A3-2 (P-HDR-2)** — on an HDR display: no washout/banding side-by-side with the game + LSFG; the present
   swapchain reports an HDR colorspace (programmatic `GetColorSpace1`/the Windows HDR indicator).

**Hardware honesty (per FDP §2.4):** A3-1's histogram and A3-2's display check require **a real HDR game +
an HDR display**, which the author has. The plan is code-complete-able offline; the *binary crossings* need
the rig. State that gap, do not claim C4 crossed from code review alone.

---

## §6 — What could NOT be verified first-hand / corrections to the dossier

- **Dossier line-number drift (corrected in §2):** `route_for` is `main.cpp:1823-1828` not 1778-1787;
  the WGC BGRA8 force is `:3098` not `:3053`; `IS_HDR` is `:3103` not `:3058`. The `hdr_convert.comp:32`
  clip and `PresentSurface.cpp` alpha-premult are correct (the present `sd.Format` is at `:326`, not the
  dossier's `:206` which is a different file region — `:206` is the `resize_swapchain` rebuild path).
- **Dossier "zero SetColorSpace1" is stale** — A3-L1 is now shipping (§2). The dossier predates the build.
- **WGC's exact allowed `DirectXPixelFormat` set** — "WGC accepts R16F/R16G16B16A16Float" is `[V2]`
  (MS recommends it; no hard allow-list found). L2 MUST therefore treat the format request as *soft* (fall
  back to BGRA8 if `Direct3D11CaptureFramePool.Create` rejects R16F) — see the RISK_REGISTER (FR-HDR-1).
- **Whether the alpha-premult DComp overlay can use 10bpc via any workaround** — treated as blocked
  (dossier §6); L4 (FP16, not 10bpc) is the only HDR present path planned.
- The C4 binary crossings (A3-1 histogram, A3-2 display) are **`designed`/unrun** — they need the HDR rig.

---

## §7 — Provenance + registration

Synthesized from the A3 dossier ([`FG_HDR_FORMAT_PRIOR_ART.md`](FG_HDR_FORMAT_PRIOR_ART.md), workflow
`w83r5ydl9` HDR agent; its two design-load-bearing MS claims author-`[VS]`) and the objective constitution
([`PHYRIADFG_OBJECTIVE_VISTA.md`](PHYRIADFG_OBJECTIVE_VISTA.md) §1-A3 / §2-C4 / §5). All in-repo `path:line`
re-verified first-hand this session (the supervisor registers this triad in
[`../FORMAL_DOCUMENTS_REGISTER.md`](../FORMAL_DOCUMENTS_REGISTER.md) and the live
[`ACTION_PLAN.md`](../ACTION_PLAN.md) — not touched here, to avoid a shared-file collision with the parallel siblings).
