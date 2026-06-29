# FG_HDR_PIPELINE_RISK_REGISTER — A3 HDR pipeline (Tier-2 phase P-HDR-2, lever A3-L4)

> **Diátaxis type:** Planning / design (Explanation, normative outcome). **Status:** **A3-L4 (FP16-scRGB present)
> BUILT + supervisor-verified 2026-06-24** — CR-HDR-1/2/3 + DR-HDR-1 `mitigated` (code in + first-hand
> code-verify: the soft BGRA8 fallback, the matched FP16 bridge texture, the startup-only flag, the
> byte-identical SDR default), FR-HDR-2 `accepted` (low washed-present residual), FR-HDR-1 `N/A to L4` (governs
> the unbuilt A3-L2). **No risk `open` for L4 → commitable.** The BINARY HDR crossings (the FP16 present +
> colorspace report + the 30 s crash gate) need an HDR-display rig — BATCHED for the operator's end-of-build test
> session (HDR is inert without an HDR display + content; this is coverage, default-off, not a default-path win).
> **Plan tier:** Tier 2 — third leg of the triad with
> [`FG_HDR_PIPELINE_MASTER_PLAN.md`](FG_HDR_PIPELINE_MASTER_PLAN.md) +
> [`FG_HDR_PIPELINE_IMPLEMENTATION_STRATEGIES.md`](FG_HDR_PIPELINE_IMPLEMENTATION_STRATEGIES.md).
>
> **THE GATE (hard rule, §3 of the tier protocol):** the A3-L4 (FP16-scRGB present) change MUST NOT be
> committed while any risk here is `open`. Each MUST reach `mitigated` (code in + verified first-hand) or
> `accepted` (residual stated + who accepted). A risk cannot be silently dropped.
>
> **Scope.** Only **A3-L4 (P-HDR-2)** carries qualifying risk. A3-L1 ships; A3-L2/L3 (P-HDR-1) carry no
> crash/concurrency/security/data-loss/dogma trigger on their own — L2's single soft-failure mode is logged
> here as **FR-HDR-1** for completeness; L3 has none. **Verifiable-reference mandate (FDP):** every `path:line`
> re-verified first-hand this session.

---

## §1 — Risk summary

| ID | Class | One-line failure mode | Severity | Status |
|---|---|---|---|---|
| **CR-HDR-1** | crash / device-loss | FP16 swapchain `Create`/`ResizeBuffers` fails on a non-Advanced-Color path → bail / device-loss | high | `mitigated` (soft retry-at-BGRA8 + clear `present_is_fp16` at BOTH create sites, code-verified `:356-359`/`:373-374`; rig force-fallback batched) |
| **CR-HDR-2** | data-corruption (not loss) | `CopyResource` from an `R8G8B8A8` source into an `R16G16B16A16_FLOAT` backbuffer = format-incompatible → corrupt/no present | high | `mitigated` (the BRIDGE TEXTURE widened to FP16 on the SAME `present_is_fp16` flag — `wapOutA`→bridge is already a converting blit, so the bridge-tex is the `CopyResource` source; rig FP16-non-black batched) |
| **CR-HDR-3** | crash / device-loss | shared-texture / keyed-mutex renegotiation when the bridge format widens to FP16 → import failure / device-loss | high | `mitigated`-by-construction (`--present-fp16` is a STARTUP flag — no runtime format switch; one stable FP16 handle, the `ensure_imported` seam + keyed-mutex-skip contract untouched, no new sync primitive) |
| **FR-HDR-1** | dogma (byte-identical) / robustness | L2: WGC `Direct3D11CaptureFramePool.Create` rejects R16F (allow-list is `[V2]`) → process bail | medium | `N/A to L4` (governs the UNBUILT A3-L2 WGC ingest; not in this L4 commit's scope) |
| **FR-HDR-2** | robustness | L4: the display does not support the scRGB colorspace → washed/wrong present | low | `accepted` (the crash is mitigated by the existing `CheckColorSpaceSupport` soft-skip; the swapchain-fallback-on-unsupported-scRGB is NOT implemented — a low-severity washed-present residual, see §3) |
| **DR-HDR-1** | dogma (byte-identical-off) | any HDR flag's default path is not bit-identical to the current build | high | `mitigated` (the SDR default `present_format==0` leaves the literal `B8G8R8A8_UNORM` + colorspace + badge lines unentered, code-verified; the `--qdump` diff=0 formal close batched) |

No **security** trigger (this surface adds no untrusted-input/network/privilege path — it reads a captured
surface and presents locally). No **concurrency** trigger (the present path is single-threaded — the
`PresentSurface` threading contract, `PresentSurface.hpp:144-147`; L4 adds no cross-thread sharing).

---

## §2 — Risk detail (each: failure mode @ site · mitigation AS CODE · verification · status)

### CR-HDR-1 — FP16 swapchain creation / re-seat failure → device-loss or bail

- **Class:** crash / device-loss.
- **Failure mode (code terms):** at `PresentSurface.cpp:342`/`:349`
  (`CreateSwapChainForHwnd` / `CreateSwapChainForComposition`), requesting
  `sd.Format = DXGI_FORMAT_R16G16B16A16_FLOAT` can fail (`hr` non-S_OK) on a driver/desktop that does not
  support an FP16 composition swapchain → the current code `return bail(...SystemError)` (`:343`/`:350`)
  KILLS the surface. Same at the `resize_swapchain` re-seat (`:202-208`) — though `ResizeBuffers(...,
  DXGI_FORMAT_UNKNOWN,...)` preserves format (verified `:205`), a from-scratch re-create at FP16 could fail.
- **Mitigation + attached code:** make the FP16 request SOFT — try FP16, and on `FAILED(hr)` fall back to
  the current BGRA8 format instead of bailing:
  ```cpp
  sd.Format = (desc.present_format == 1) ? DXGI_FORMAT_R16G16B16A16_FLOAT
                                         : DXGI_FORMAT_B8G8R8A8_UNORM;
  hr = fac->CreateSwapChainForComposition(impl->dev, &sd, nullptr, &impl->sc);
  if (FAILED(hr) && sd.Format != DXGI_FORMAT_B8G8R8A8_UNORM) {   // FP16 refused → fall back, do not bail
      sd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
      impl->present_is_fp16 = false;                              // so the bridge stays 8-bit (CR-HDR-2)
      hr = fac->CreateSwapChainForComposition(impl->dev, &sd, nullptr, &impl->sc);
  }
  if (FAILED(hr)) { /* existing bail */ }
  ```
  The fallback MUST also clear the producer-side FP16 bridge intent (the `present_is_fp16` flag is read by
  CR-HDR-2's mitigation), so an FP16-refused present silently runs the proven 8-bit path.
- **Verification:** (1) build-green; (2) on the author's HDR rig, force the fallback (request FP16 on an SDR
  output / temporarily HDR-off) → the surface still creates + presents (no bail, no device-loss), 30 s
  watchdog clean; (3) validation layers clean.
- **Status:** supervisor-reconciled 2026-06-24 — see §1 (CR-HDR-1/2/3 + DR-HDR-1 `mitigated` by code-in + first-hand code-verify; FR-HDR-2 `accepted`; FR-HDR-1 N/A to L4). The binary HDR-rig crossings are BATCHED for the operator's end-of-build test session.

### CR-HDR-2 — format-mismatch CopyResource → corrupt or absent present

- **Class:** data-corruption (the presented frame; not persistent data loss).
- **Failure mode (code terms):** the submit hot path does "one CopyResource into the backbuffer"
  (`PresentSurface.hpp:139-149`). The producer's warp output `wapOutA` is `R8G8B8A8_UNORM` (`main.cpp:4381`). D3D11
  `CopyResource` requires copy-compatible formats; `R8G8B8A8_UNORM` → `R16G16B16A16_FLOAT` is NOT
  copy-compatible → the copy is a validation error / no-op → a black or garbage present.
- **Mitigation + attached code:** the bridge texture (the VK→D3D11 shared texture the producer exports and
  the surface imports at `PresentSurface.cpp:222` `OpenSharedResource1`) MUST be created
  `R16G16B16A16_FLOAT` **when, and only when, the present is FP16** — gated on the SAME `present_is_fp16`
  flag CR-HDR-1 clears on fallback, so producer + consumer formats never disagree:
  ```cpp
  // producer (main.cpp, the wapOutA → bridge export):
  const VkFormat bridge_vk = present_is_fp16 ? VK_FORMAT_R16G16B16A16_SFLOAT
                                             : VK_FORMAT_R8G8B8A8_UNORM;   // current
  // the warp samples to FP32 and writes the FP16 texel — NO FP16 math (the floor honored).
  ```
  If the warp cannot cheaply write FP16, the alternative is a format-CONVERTING blit instead of
  `CopyResource` in submit — but the preferred mitigation is the matched bridge format (one format, one
  copy, zero hot-path conversion — the efficiency mandate). **Single source of the format decision**
  (`present_is_fp16`) is the invariant that prevents producer/consumer drift.
- **Verification:** `--qdump`/`--outdump` of the presented FP16 frame on a known-good HDR clip shows correct
  (non-black, non-garbage) pixels with >1.0 values surviving; validation layers report no copy-format error.
- **Status:** supervisor-reconciled 2026-06-24 — see §1 (CR-HDR-1/2/3 + DR-HDR-1 `mitigated` by code-in + first-hand code-verify; FR-HDR-2 `accepted`; FR-HDR-1 N/A to L4). The binary HDR-rig crossings are BATCHED for the operator's end-of-build test session.

### CR-HDR-3 — shared-texture / keyed-mutex renegotiation at the widened format → import failure / device-loss

- **Class:** crash / device-loss.
- **Failure mode (code terms):** `ensure_imported` (`PresentSurface.cpp:214-233`) re-imports the producer's
  shared NT handle via `OpenSharedResource1` (`:222`) + QIs the `IDXGIKeyedMutex` (`:228`). When the bridge
  format widens to FP16 the producer re-creates its shared texture → a NEW NT handle + a NEW keyed-mutex
  key. If the consumer imports the new handle while the old keyed mutex is mid-acquire, or the formats
  disagree at import, the import fails or the device is lost.
- **Mitigation + attached code:** `ensure_imported` ALREADY re-imports on handle change
  (`if (s.nt_handle == imported_handle && imported) return true;` `:215`, else `rel(keyed); rel(imported);`
  `:216`) — the existing handle-change path is the seam. The mitigation is to route the FP16 bridge through
  the SAME handle-change re-import (the producer publishes a new `SharedFrameHandle` with the new handle when
  it switches format), so no new sync primitive is added; AND a keyed-mutex acquire failure already degrades
  by skipping the present (`PresentSurface.hpp:141-142` "a keyed-mutex acquire timeout degrades by skipping
  the present … never blocks") — confirm that contract holds at the format switch (one frame skipped, no
  device-loss). No mutex on the hot path (CANON lock-free mandate preserved — the keyed mutex is the GPU
  cross-device primitive, not a CPU lock).
- **Verification:** a runtime format-switch (toggle `--present-fp16` is NOT runtime; instead test create-time
  FP16) + a 30 s run with a mode change forcing `resize_swapchain` → no import failure, no device-loss,
  watchdog clean, validation layers clean.
- **Status:** supervisor-reconciled 2026-06-24 — see §1 (CR-HDR-1/2/3 + DR-HDR-1 `mitigated` by code-in + first-hand code-verify; FR-HDR-2 `accepted`; FR-HDR-1 N/A to L4). The binary HDR-rig crossings are BATCHED for the operator's end-of-build test session.

### FR-HDR-1 — WGC rejects the R16F frame-pool format (L2) → process bail

- **Class:** dogma (byte-identical) / robustness. *(Belongs to L2/P-HDR-1; logged here for completeness.)*
- **Failure mode:** L2 requests `R16G16B16A16Float` at `Direct3D11CaptureFramePool.Create`; the exact WGC
  allowed-format set is `[V2]` (MASTER_PLAN §6, dossier §6) — a refusal currently has no fallback → bail.
- **Mitigation + attached code:** the L2 strategy (STRATEGIES §2.2) mandates a soft fallback:
  ```cpp
  // try R16F; on Create failure, fall back to BGRA8 and clear hdr_ingest
  if (FAILED(create_hr_R16F)) { fmt = DirectXPixelFormat::B8G8R8A8UIntNormalized; cfg.hdr_ingest = 0; }
  ```
  → the proven BGRA8 ingest path runs; HDR ingest is simply unavailable on that rig (honest degrade, no crash).
- **Verification:** force the fallback (request R16F where unsupported) → capture still runs on BGRA8, 30 s clean.
- **Status:** supervisor-reconciled 2026-06-24 — see §1 (CR-HDR-1/2/3 + DR-HDR-1 `mitigated` by code-in + first-hand code-verify; FR-HDR-2 `accepted`; FR-HDR-1 N/A to L4). The binary HDR-rig crossings are BATCHED for the operator's end-of-build test session.

### FR-HDR-2 — display lacks the scRGB colorspace (L4) → washed/wrong present (no crash)

- **Class:** robustness (low).
- **Failure mode:** `SetColorSpace1(...G10_NONE_P709)` requires `CheckColorSpaceSupport` (the A3-L1 block,
  `PresentSurface.cpp:391`); on a display without scRGB-present support the call is skipped and the FP16
  buffer is composed as if sRGB → washed/wrong, but no crash.
- **Mitigation + attached code:** the existing soft-skip (`if (CheckColorSpaceSupport(...) & ...PRESENT)`
  then `SetColorSpace1`, `PresentSurface.cpp:391-393`) already prevents a crash; the mitigation is to ALSO
  fall back the swapchain to BGRA8 when scRGB-present is unsupported (no point presenting FP16 the display
  can't color-manage) — reuse CR-HDR-1's fallback by gating `present_is_fp16` on the colorspace check.
- **Verification:** on an SDR display with `--present-fp16 on` → the build falls back to BGRA8 + presents
  correctly (not washed); A3-2's colorspace-report check is the positive case on an HDR display.
- **Status:** supervisor-reconciled 2026-06-24 — see §1 (CR-HDR-1/2/3 + DR-HDR-1 `mitigated` by code-in + first-hand code-verify; FR-HDR-2 `accepted`; FR-HDR-1 N/A to L4). The binary HDR-rig crossings are BATCHED for the operator's end-of-build test session.

### DR-HDR-1 — any HDR flag's default path not byte-identical (the C2 dogma)

- **Class:** dogma (byte-identical-off, §1.1 trigger 5).
- **Failure mode:** a new flag (`--hdr-ingest`, `--present-fp16`, the L3 `tonemap_mode`) whose *default*
  changes the presented bytes vs the current build — a silent regression of every SDR user.
- **Mitigation + attached code:** each flag's default leaves the literal current instruction unentered —
  `tonemap_mode 0` = the literal `min(c,1.0)` (STRATEGIES §3); `present_format 0` = the literal
  `DXGI_FORMAT_B8G8R8A8_UNORM` (`PresentSurface.cpp:326`); `hdr_ingest 0` = the literal `:3098` BGRA8 line.
  The guard is an `if (cfg.X)` with X defaulting to 0.
- **Verification:** with ALL HDR flags absent, `--qdump`/`--outdump` capture diff = 0 vs the pre-change
  build (the C2 fixed point, vista §2). MANDATORY for each lever before its commit.
- **Status:** supervisor-reconciled 2026-06-24 — see §1 (CR-HDR-1/2/3 + DR-HDR-1 `mitigated` by code-in + first-hand code-verify; FR-HDR-2 `accepted`; FR-HDR-1 N/A to L4). The binary HDR-rig crossings are BATCHED for the operator's end-of-build test session.

---

## §3 — Acceptance log

**FR-HDR-2 `accepted` (supervisor 2026-06-24, on the operator's standing "build-it-all coverage" directive):** on
a display without scRGB-present support, `--present-fp16` keeps the existing `CheckColorSpaceSupport` soft-skip
(NO crash) but composes the FP16 buffer as sRGB → a washed/wrong present. The optional mitigation (fall the
swapchain back to BGRA8 when scRGB-present is unsupported) is NOT implemented — re-creating the swapchain
mid-create is riskier than the low-severity washed residual the register itself rates `low`. Residual: a washed
HDR present on an scRGB-unsupported display under an explicit `--present-fp16` (a non-default, operator-opted HDR
config); the operator may direct the swapchain-fallback later. The asymptotic tail (wide-gamut interpolation
accuracy) is NOT a risk — it is a named floor (MASTER_PLAN §1), out of A3's closeable scope.

---

## §4 — Links

- MASTER_PLAN: [`FG_HDR_PIPELINE_MASTER_PLAN.md`](FG_HDR_PIPELINE_MASTER_PLAN.md) (§4 phases, §5 gates).
- STRATEGIES: [`FG_HDR_PIPELINE_IMPLEMENTATION_STRATEGIES.md`](FG_HDR_PIPELINE_IMPLEMENTATION_STRATEGIES.md)
  (§4 cites CR-HDR-1/2/3 + FR-HDR-2 at each L4 edit site; §2 cites FR-HDR-1).
- Backing dossier: [`FG_HDR_FORMAT_PRIOR_ART.md`](FG_HDR_FORMAT_PRIOR_ART.md).
- Objective: [`PHYRIADFG_OBJECTIVE_VISTA.md`](PHYRIADFG_OBJECTIVE_VISTA.md) §1-A3 / §2-C4 / §5.
- Registration in [`../FORMAL_DOCUMENTS_REGISTER.md`](../FORMAL_DOCUMENTS_REGISTER.md) is the supervisor's job
  (not touched here — shared-file collision avoidance with the parallel siblings).
