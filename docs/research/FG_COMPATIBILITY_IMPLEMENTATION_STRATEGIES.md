# FG_COMPATIBILITY_IMPLEMENTATION_STRATEGIES — concrete edit sites for the E2 coverage moat

> **Diátaxis type:** Planning / design (Explanation). **Status:** `designed` (no code yet). **Plan tier:**
> Tier 1 (companion to [`FG_COMPATIBILITY_MASTER_PLAN.md`](FG_COMPATIBILITY_MASTER_PLAN.md); no
> RISK_REGISTER — master-plan §6). **Resolves** objective E2
> ([`PHYRIADFG_OBJECTIVE_VISTA.md`](PHYRIADFG_OBJECTIVE_VISTA.md) §1-E2); SOTA dossier
> [`FG_COMPATIBILITY_COVERAGE_PRIOR_ART.md`](FG_COMPATIBILITY_COVERAGE_PRIOR_ART.md).
>
> **Normativity (BCP 14).** MUST/SHOULD/MAY where a real constraint binds.
>
> **Verifiable-reference mandate (FDP §2).** Every `file:line` was opened first-hand 2026-06-23. The edit
> sites are the EXACT insertion points; where a line will shift as code lands, the strategy names a stable
> anchor symbol so a future implementer re-locates it.

---

## §0 — How to read this

Each strategy: **(target)** the master-plan deliverable · **(site)** the verified edit location · **(edit)**
the concrete change · **(byte-identical-off)** how the default path is preserved · **(verify)** the
first-hand gate. The byte-identical-off discipline (the dogma) is restated per strategy because each
touches the live capture/present path. Flags follow the **`parse_extra` C1061 dodge** convention already in
the file (`main.cpp:1097-1144` — new flags go in `parse_extra`, NOT the main `if/else if` chain, or MSVC
hits the 128-nesting limit C1061; verified the existing flags do exactly this).

The reason layer (S1–S3) is the closeable fixed point and lands first; the ladder (S4–S5) is the reach
improvement; the matrix harness (S6) is the gate.

---

## S1 — The advisory print layer (mirror the shipping F2 precedent)

- **(target)** E2-P3 · the human-readable side of the reason taxonomy.
- **(site)** `main.cpp:3223-3232` is the EXISTING F2 VRR/G-Sync honest-advisory block (verified: an
  always-on `std::printf` that classifies NVIDIA/AMD and prints the honest floor). It is the precedent to
  mirror: a cold-path, init-time, behavior-neutral advisory.
- **(edit)** Add a sibling block right after it (init scope, before the capture/present threads spawn) that,
  given the resolved `ReasonCode` from the probes (S3), prints the code's honest advisory string. One
  `printf` per surfaced condition, e.g. on `EXCLUSIVE_FULLSCREEN`:
  `"[ra] E2/CAP (honest): the target presents EXCLUSIVE-FULLSCREEN — WGC capture is unreliable there (it
  bypasses DWM; only injection reaches it = an anti-cheat ban). Switch the game to BORDERLESS/windowed."`
  Each string states the FLOOR plainly (master-plan §5) — explains, never promises.
- **(byte-identical-off)** Advisories are `printf` only — **no behavior change**, like the F2 block. They
  are always-on (informational), so there is no flag and nothing to make identical; the present output is
  untouched.
- **(verify)** Each code, triggered by a contrived condition (S3), prints its string; grep the run log for
  the generic `"capture output %d unavailable"` (`main.cpp:3055`) → it MUST no longer be the only signal on
  a classifiable failure.

## S2 — The reason-code taxonomy header (single source of truth)

- **(target)** E2-P1 · the enum + the `REASON_UNCLASSIFIED` tripwire.
- **(site)** New header under the render-assistant app, e.g.
  `apps/render_assistant/src/compat_reason.hpp` (app-local — this is product-policy, not a pillar
  capability; if it later qualifies as a pillar primitive it goes through the FR gate, not here). The
  precedent for a small POD enum is `framework/schema/include/phyriad/schema/Error.hpp:27-56` (verified: a
  flat `enum class : uint32_t` with stable values + `Unavailable=20`); model the new enum on it but keep it
  app-local.
- **(edit)** Define `enum class ReasonCode : uint32_t { OK=0, EXCLUSIVE_FULLSCREEN, PROTECTED_CONTENT,
  HYBRID_DD_WRONG_GPU, WGC_UNSUPPORTED_OS, VRR_WILL_BE_DISABLED, ANTICHEAT_UNVERIFIED, WINDOW_NOT_FOUND,
  CAPTURE_INIT_FAILED, REASON_UNCLASSIFIED };` plus a `constexpr const char* advisory(ReasonCode)` and a
  `constexpr const char* token(ReasonCode)` (machine token for the matrix harness + any future control-plane
  egress). The enum is the ONLY definition of the set (master-plan §3 "single source"). `REASON_UNCLASSIFIED`
  is the sentinel; a `static std::atomic<uint32_t> g_unclassified_count` (or a plain init-thread counter,
  since it is set pre-loop) increments wherever a failure cannot be mapped — the tripwire whose value MUST
  be 0 to cross E2-GRACE.
- **(byte-identical-off)** A new header consumed only by the new probe/advisory code; the steady present
  path never includes it. No effect on the default run.
- **(verify)** Compiles; the enum is the only copy (grep the repo for a second `EXCLUSIVE_FULLSCREEN`
  definition = 0); `token()`/`advisory()` cover every enumerator (a `static_assert` or a switch with no
  default that warns on a missing case).
- **(INVENTORY)** Adding this file → an `INVENTORY.md` entry in the same change (CLAUDE.md / AGENT_PROTOCOL
  §10). Brief: "PhyriadFG E2 compatibility reason-code taxonomy (graceful-fail-with-reason)."

## S3 — The pre-flight + post-init probes (detect-before-break)

- **(target)** E2-P2 · wire OS signals to reason codes BEFORE the run breaks.
- **(sites, each verified):**
  - **`WGC_UNSUPPORTED_OS`** — call `GraphicsCaptureSession::IsSupported()` in the WGC backend-init block
    `main.cpp:4018-4031` (verified: the `interop->CreateForWindow`/WGC item creation lives here; today there
    is NO `IsSupported` guard — grep `IsSupported` over `main.cpp` = 0). On false → emit the code, descend.
  - **`HYBRID_DD_WRONG_GPU`** — the DD path is `DuplicateOutput` at `main.cpp:1864-1865` (verified). Upgrade
    to `DuplicateOutput1` (or inspect the existing `DuplicateOutput` HRESULT) and map `DXGI_ERROR_UNSUPPORTED`
    → the code (dossier §2-A `[V1]`: hybrid dGPU returns exactly this). On it → force WGC (master-plan §5).
  - **`WINDOW_NOT_FOUND`** — `main.cpp:3094-3095` already detects "no visible window matching '%s'" and
    exits 1 (verified). Replace the bare exit with the `WINDOW_NOT_FOUND` code + advisory (this is the
    cheapest single conversion of an existing silent-ish failure into a coded one).
  - **`PROTECTED_CONTENT`** — a one-frame zero-luma probe AFTER first capture. The delivered frame **pixels**
    live in the WGC pixel ring `main.cpp:3031` `ID3D11Texture2D* ring[RING_N]` (verified); the per-slot
    timestamp/compose-tracking structure at `main.cpp:3034-3040` (verified) locates **which** slot was just
    delivered (it tracks timestamps, not pixels). Sample the delivered slot's luma (sum/Σ over a stride) and
    if ≈0 under a known-active source → the code. This is post-init (one frame), cold-path. (Dossier §2-A
    `[V1]`: DRM/HDCP → black for ALL OS capture.)
  - **`CAPTURE_INIT_FAILED`** — the generic failure at `main.cpp:3054-3057` (verified: prints
    `"capture output %d unavailable (api=%s)"` then exits 1). Map it to this code (the ladder S4 then
    retries the next rung instead of exiting).
- **(byte-identical-off)** The probes are init-time (before the present loop); `IsSupported` and the
  HRESULT inspection are read-only. The zero-luma probe reads one already-captured frame (no extra capture).
  With the ladder flag off (S4), a probe that fires still resolves to the SAME single static backend as
  today and the same exit behavior — only the *message* gains a code. The present path is untouched.
- **(verify)** Force each condition (an unsupported-OS mock / a hybrid rig or a faked HRESULT / a
  `--window` with no match / a DRM video / a bad output index) → the matching code is emitted, no
  `REASON_UNCLASSIFIED`.

## S4 — The per-OS capture-backend selector (behind a default-off flag)

- **(target)** E2-P4 · replace the static `capture_api` with an ordered descent.
- **(site)** The selection input is `cfg.capture_api`, set at `main.cpp:1201-1202` (the `--capture-api`
  parse) and defaulted at `main.cpp:125` (`CA_DD`); it is consumed as a static branch at
  `main.cpp:2997,3053,4020` (init) and `5119,8204,8834,9070` (loop) — all verified. The selector wraps the
  RESOLUTION of `cfg.capture_api` at init (before `main.cpp:3053` `want_dd`), leaving every consumer
  untouched.
- **(edit)** Add `--capture-ladder` (in `parse_extra`, per the C1061 convention `main.cpp:1097-1144`). When
  ON: query the Windows build (the repo already does `EnumDisplaySettingsExA`/DXGI enumeration nearby —
  `main.cpp:1855-1859` — add a build query) and run `select_capture_backend` (master-plan §4): probe
  `IsSupported` (S3) for the WGC rung, prefer per the dossier §2-C `[V1]` ladder shape, and on each rung's
  reason-coded refusal descend to the next. The selector only *writes* `cfg.capture_api` (and may set a GDI
  sentinel, S5); the downstream branches are unchanged.
- **(byte-identical-off)** Flag OFF (default) → `select_capture_backend` is never called → `cfg.capture_api`
  keeps its parsed/default value → the run is **byte-identical** to today (the static `CA_DD`/`CA_WGC`
  branch at `main.cpp:3053`). Risk-ref: this is the §6-master-plan dogma constraint, honored by
  construction.
- **(verify)** Flag-off: a diff of the present output / a `--capture-api` run is identical to the current
  binary's. Flag-on: on a rig where WGC is unavailable, the log shows the descent with a code at the WGC
  refusal and a successful lower rung.

## S5 — The GDI last-resort rung (isolated backend)

- **(target)** E2-P4 · the third ladder rung; reaches surfaces WGC/DD miss on old builds.
- **(site)** A new capture path parallel to the DD path (`d3d_init`/`DuplicateOutput`,
  `main.cpp:1843-1876`, verified) and the WGC path (`main.cpp:4018+`). It is selected only by the S4
  selector's GDI sentinel (a new `CA_GDI` enumerator in `enum CaptureApi` `main.cpp:107`).
- **(edit)** Implement a GDI capture (`GetWindowDC`/`BitBlt` into a DIB, or `PrintWindow` for occluded
  windows) into the SAME `D3D11` staging texture the other backends feed (so the downstream upload/warp is
  unchanged — it consumes a BGRA8 staging copy regardless of backend, per the format-uniform observation
  `main.cpp:1871`). The rung is **self-contained**: its own DC/DIB + a single CPU→staging copy; it does
  **NOT** touch the cross-device upload ring, the command pools, or any GPU queue (master-plan §6 — this
  isolation is what keeps the change Tier 1).
- **(byte-identical-off)** `CA_GDI` is only reachable via `--capture-ladder` (S4, default-off) OR an
  explicit `--capture-api gdi`; the default `CA_DD`/`CA_WGC` paths are unchanged.
- **(escalation tripwire, master-plan §6).** If a future revision makes GDI share the WGC/DD ring, pools, or
  the upload path, THAT revision escalates to Tier 2 + a RISK_REGISTER. As scoped (isolated staging) it
  stays Tier 1.
- **(verify)** On a window WGC/DD cannot reach (e.g. a pre-1803 mock or a specific occluded window),
  `--capture-api gdi` yields a non-black, correctly-sized stream; the fidelity/latency cost is recorded
  honestly (it is a cost, not a win — master-plan §9).
- **(INVENTORY)** If the GDI rung lands in a new `.cpp`, an `INVENTORY.md` entry in the same change.

## S6 — The E2-MATRIX living artifact + the harness (the gate)

- **(target)** E2-P5 · the closeable-fixed-point test.
- **(site)** A doc + a test harness, not main-loop code. The matrix doc seed is dossier §3 "E2-MATRIX"
  (verified present). The harness is a script (e.g. under `frames/` or `scripts/`, gitignore-aware like the
  existing `frames/gate_zoo.ps1` per the render-assistant arc memory) that runs the fixed test set and
  parses the run log's reason tokens (S2 `token()`).
- **(edit)** For each test-set entry (a game × windowing-mode × protection × AC), run PhyriadFG, capture the
  emitted `ReasonCode` token, and assert: (a) every non-`OK` cell carries a taxonomy code; (b) the
  `REASON_UNCLASSIFIED` count = 0. Record the verdict + evidence per cell in the matrix doc (the living
  artifact). The render API is NOT a row axis (master-plan §0 — collapses to WGC).
- **(byte-identical-off)** Pure test/doc tooling; no product-path change.
- **(verify — THE fixed-point gate)** Over the fixed test set: zero `REASON_UNCLASSIFIED`, every
  floor-excluded cell (`EXCLUSIVE_FULLSCREEN`/`PROTECTED_CONTENT`/`HYBRID_DD_WRONG_GPU`) reached via its
  code → **E2-GRACE CROSSED** (master-plan §1). The asymptotic remainder = passing-cell count over the
  expanding set (the one tracked metric).

---

## §7 — Edit-site summary (traceability table)

| Strategy | Verified site | Reason code(s) wired | Flag (default) |
|---|---|---|---|
| S1 advisory layer | `main.cpp:3223-3232` (F2 precedent) | all | none (always-on print) |
| S2 taxonomy header | new `compat_reason.hpp`; model `Error.hpp:27-56` | the enum itself + `REASON_UNCLASSIFIED` | none |
| S3 probes | `4018-4031` (WGC), `1864-1865` (DD), `3094-3095` (window), `3031` pixel ring + `3034-3040` slot-track (first-frame luma), `3054-3057` (generic) | `WGC_UNSUPPORTED_OS`, `HYBRID_DD_WRONG_GPU`, `WINDOW_NOT_FOUND`, `PROTECTED_CONTENT`, `CAPTURE_INIT_FAILED` | none (probes) |
| S4 selector | resolves `cfg.capture_api` before `3053`; parse in `parse_extra` (`1097-1144`) | descent codes | `--capture-ladder` (OFF) |
| S5 GDI rung | new path beside `1843-1876` / `4018+`; `CA_GDI` in `enum` `107` | `CAPTURE_INIT_FAILED` on exhaustion | `--capture-ladder` / `--capture-api gdi` (OFF) |
| S6 matrix harness | dossier §3 seed; script per `frames/` convention | reads `token()` | n/a (test/doc) |

## §8 — What these strategies do NOT do

- Do **not** add any GPU-queue, fence, or cross-device-upload change (that isolation is the Tier-1 boundary
  — master-plan §6; S5 escalation tripwire).
- Do **not** alter the present path, the warp, or the pacing — the reason layer is init-time + advisory.
- Do **not** implement the VRR independent-flip probe, the HDR present path, or the anti-cheat empirical
  campaign (cross-linked T2 / family A3 / family F1 — out of scope, master-plan §0).
- Do **not** hard-code a Windows-driver-version assumption in the selector (the `[V2]` 24H2/566.36 facts are
  fragile — dossier §6; probe capability, don't gate on a version string).
