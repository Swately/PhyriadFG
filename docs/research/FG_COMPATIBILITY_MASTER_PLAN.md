# FG_COMPATIBILITY_MASTER_PLAN — the coverage moat (matrix + fallback ladder + graceful-fail reason layer)

> **Diátaxis type:** Planning / design (Explanation). **Status:** `designed` (no code yet; the standing
> facts about the current code are `shipping`/`measured` and cited inline). **Plan tier:** **Tier 1**
> (substantial, multi-step, cross-component) per
> [`../canon/PLAN_TIER_PROTOCOL.md`](../canon/PLAN_TIER_PROTOCOL.md) §1 — **no RISK_REGISTER** (the §1.1
> qualifying-risk analysis is in §6; none of the five triggers fires). Companion strategies:
> [`FG_COMPATIBILITY_IMPLEMENTATION_STRATEGIES.md`](FG_COMPATIBILITY_IMPLEMENTATION_STRATEGIES.md).
>
> **Resolves objective E2** — real-library compatibility breadth — from
> [`PHYRIADFG_OBJECTIVE_VISTA.md`](PHYRIADFG_OBJECTIVE_VISTA.md) §1-E2. SOTA dossier:
> [`FG_COMPATIBILITY_COVERAGE_PRIOR_ART.md`](FG_COMPATIBILITY_COVERAGE_PRIOR_ART.md). Floors register:
> the vista §5.
>
> **Normativity (BCP 14).** MUST / MUST NOT / SHOULD / MAY per RFC 2119/8174 only where a real requirement
> binds (a dogma, a plan-tier obligation, a floor).
>
> **Verifiable-reference mandate (FDP §2).** Every `file:line` below was opened first-hand this session
> (2026-06-23) on `apps/render_assistant/src/main.cpp` and
> `framework/render/present/src/PresentSurface.cpp`. Where the vista/dossier cited a stale line, this plan
> cites the corrected one and flags the drift (§7). Nothing here is fabricated.

---

## §0 — Scope, and the one thing this plan is FOR

**In scope.** The compatibility/coverage surface of an external-capture FG: which of the user's real games
capture+present correctly, and — when they cannot — that they fail with a **specific, enumerated reason**
instead of a black screen or a silent no-op. Three deliverables, which TOGETHER are the moat (dossier §3
E2-MOAT):

1. **The E2-MATRIX** — a living artifact enumerating every (windowing-mode × content-protection ×
   anti-cheat) cell with an expected verdict + a reason code for the non-`works` cells (dossier §3
   E2-MATRIX). The render API (DX11/12/Vk/GL) is **NOT** a matrix axis — it collapses to the same WGC
   composition path (dossier §3 "Key simplification"; verified: WGC captures the composed desktop output
   regardless of the game's render API — `main.cpp:1868-1871`, the format is forced `B8G8R8A8` for ALL
   WGC sources).
2. **The graceful-fail reason layer (E2-GRACE)** — a pre-flight probe + a fixed reason-code taxonomy so
   every excluded/broken cell is reached via a named code, never a generic failure. This is the **closeable
   fixed point** (§1).
3. **The fallback ladder** — a GDI last-resort capture rung + a per-Windows-build capture-mode auto-selector,
   so the capture backend is chosen, not hard-set by the operator (today it is one static field,
   `main.cpp:125` `capture_api=CA_DD`).

**Non-goals (explicit, FDP §3 planning-doc requirement).**
- **NOT** an attempt to beat any irreducible floor (§5). Exclusive-fullscreen capture, HDCP/protected →
  black, and hybrid-laptop dGPU Desktop-Duplication-unsupported are **NAMED and surfaced as reason codes**,
  never planned-to-defeat. Re-proposing an irreducible as a win is the failure mode this plan refuses.
- **NOT** an injection/present-hook path. Sub-composition capture (the only way to reach true
  exclusive-fullscreen) = injection = anti-cheat ban (dossier §2-A `[V1]`); it stays off the table.
- **NOT** the HDR colour pipeline (family A3), the VRR/present-mode rework beyond surfacing its caveat
  (family A2/E2-PRESENT — that is P0/T2 work, cross-linked not absorbed), or the anti-cheat empirical
  campaign (family F1 — cross-linked; this plan only emits the `ANTICHEAT_UNVERIFIED` advisory).
- **NOT** a per-title quality regression (that is the `fg_quality_scorer`, family H1).

**What it is FOR, in one line:** convert an open-ended "does it work on my game?" question into a **finite,
testable "no silent failures" target** — the dossier's E2-GRACE, the highest-leverage closeable in E2.

---

## §1 — The closeable fixed point (what "done" means here)

Per the vista §1-E2(f) and dossier §3 E2-GRACE, E2 has ONE binary fixed point and an asymptotic remainder:

> **★ FIXED-POINT (E2-GRACE):** every floor-excluded or broken cell in the E2-MATRIX is reached via a
> **specific reason code from a fixed enumerated taxonomy**, with **ZERO `broken-without-explanation`
> cells**. Binary, testable.

- **Binary test.** Run the fixed test set across the matrix; assert every non-`works` outcome carries a
  reason code from the §3 taxonomy and no path produces a generic/unknown failure (the
  `REASON_UNCLASSIFIED` sentinel count = 0). When that holds, E2-GRACE is **CROSSED**.
- **Asymptotic remainder** (NOT a fixed point — track-a-metric only): library growth + anti-cheat drift
  reduce to **one** metric — passing-cell count over an expanding test set. "Enough" is operator-declared,
  never 1.0 (vista §0 THE RULE).

Per-deliverable closeable status, honest:
- **E2-GRACE** (reason layer): **fully closeable, no floor** (dossier §3 E2-GRACE: "the one E2 objective
  with no structural floor"). This plan's primary target.
- **E2-CAP** (capture reach): **asymptotic against the exclusive-FS + DRM floor**; the binary sub-criterion
  is "every test-set game in borderless/windowed/flip-FS yields a non-black, source-cadence-correct stream".
- **E2-PRESENT** (VRR/HDR coexist): VRR-removal + the +1 compose frame are **floor** (§5); only the
  `displayable`-DComp independent-flip probe is a binary sub-goal, and it is **T2** (deferred, cross-linked,
  NOT built here).
- **E2-MOAT**: asymptotic; "done enough" = zero `broken-without-explanation` cells = E2-GRACE crossed.

---

## §2 — Current standing (in-repo, verified first-hand 2026-06-23)

| Fact | Site (verified) | Standing |
|---|---|---|
| Capture API is a **single static choice**, default `CA_DD` (Desktop Duplication) | `main.cpp:107` `enum CaptureApi { CA_DD, CA_WGC }`; `main.cpp:125` `capture_api=CA_DD` | `shipping` |
| `--capture-api {dd,wgc}` selects it; `--window` implies WGC | `main.cpp:1201-1202`, `main.cpp:1488-1490` | `shipping` |
| **No fallback ladder, no per-OS auto-select** — the choice never changes at runtime | (absence; every `capture_api` read is a static branch — the complete set is `main.cpp:2997,3053,3093,4020,5119,8204,8212,8834,8840,9070,9076`, all `==CA_WGC`/`==CA_DD` compares, none re-assigns) | gap |
| **No GDI rung** | grep `GDI\|BitBlt\|PrintWindow\|GetDC` over `main.cpp` = **0 matches** | gap |
| **No `GraphicsCaptureSession::IsSupported()` pre-flight probe** | grep `IsSupported` over `main.cpp` = **0 matches** | gap |
| WGC delivers BGRA8, dimensions from the output/window rect | `main.cpp:1868-1871` (output), `main.cpp:3093-3098` (window) | `shipping` |
| DD path = `DuplicateOutput` on the chosen output | `main.cpp:1864-1865` | `shipping` |
| Capture-init failure → **one generic message + exit 1** (no reason code) | `main.cpp:3054-3057` `"[ra] capture output %d unavailable (api=%s)"` | gap (the silent-failure surface) |
| Present = `CreateSwapChainForComposition` + DComp visual (DWM-composed overlay), classic-flip fallback | `PresentSurface.cpp:335-352` | `shipping` |
| Present-time pacing (`submit_at`) is **`Unavailable`** (Pacing::Immediate ships) | `PresentSurface.cpp:540-544,573,580` | `designed` not shipping |
| An **F2 VRR/G-Sync honest startup advisory already exists** (the precedent to mirror for E2-GRACE prints) | `main.cpp:3223-3232` | `shipping` |
| An `ErrorCode` enum exists (the reason-code precedent; `enum class ErrorCode : uint32_t`, 4 bytes — e.g. `Unavailable=20`, `Unsupported`-adjacent), carried by a 16-byte `alignas(16) struct Error` | `framework/schema/include/phyriad/schema/Error.hpp:27` (enum), `:58` (`alignas(16) struct Error`) | `shipping` |

**Conclusion.** The capture/present *reach* is class-competitive (WGC + DD), but the three moat pieces —
ladder, per-OS selector, reason layer — are **absent**. The single widest coverage gap to LSFG is not an
algorithm gap; it is the missing ladder + the silent-failure surface (vista §1-E2(d); dossier §3 E2-MOAT).

---

## §3 — The reason-code taxonomy (the heart of E2-GRACE)

A fixed enumerated set. Every excluded/broken cell MUST map to exactly one. The set is closed; an outcome
that matches none MUST surface `REASON_UNCLASSIFIED` (the sentinel whose runtime count must be 0 to cross
the fixed point) — it is the tripwire, never a silent pass.

| Reason code | Trigger (the OS signal that detects it) | Detect-before-break? | Floor? |
|---|---|---|---|
| `OK` | capture init succeeds + first frame non-black + source-cadence present | n/a | — |
| `EXCLUSIVE_FULLSCREEN` | the target presents in true exclusive FS (bypasses DWM → WGC unreliable) | advisory: recommend switch-to-borderless | **FLOOR** (§5) |
| `PROTECTED_CONTENT` | first captured frame is zero-luma/black under a known-good source (DRM/HDCP marked) | post-init (one-frame probe) | **FLOOR** (§5) |
| `HYBRID_DD_WRONG_GPU` | `DuplicateOutput1` → `DXGI_ERROR_UNSUPPORTED` on a Microsoft-Hybrid laptop dGPU | pre-flight (the HRESULT) | **FLOOR** for the DD path (§5); mitigation = force WGC |
| `WGC_UNSUPPORTED_OS` | `GraphicsCaptureSession::IsSupported()` == false (build < 1803) | pre-flight | — (ladder falls to DD/GDI) |
| `VRR_WILL_BE_DISABLED` | a topmost composed overlay + the source in independent-flip/VRR | advisory (non-fatal) | **FLOOR** on NVIDIA (§5); AMD/Intel may persist |
| `ANTICHEAT_UNVERIFIED` | the target matches a known kernel-AC title and is not in the verified set | advisory (non-fatal); refuse only on operator opt-in | partial (family F1) |
| `WINDOW_NOT_FOUND` | `--window SUBSTR` matched no visible window | pre-flight | — |
| `CAPTURE_INIT_FAILED` | the backend's device/duplication/session create failed for an OS reason | at init | — (ladder retries next rung) |
| `REASON_UNCLASSIFIED` | **none of the above** — the tripwire | n/a | — (count MUST be 0 to cross E2-GRACE) |

**Design rules (MUST):**
- The taxonomy is the SINGLE source of truth for a failure reason; it lives in ONE header
  (strategies S2). No second copy.
- Each code carries a **human advisory string** (the F2 precedent, `main.cpp:3223-3232`) AND a stable
  machine token (for the matrix harness + future control-plane egress).
- A `FLOOR`-tagged code is **surfaced, never retried-to-victory** — it explains, it does not promise. This
  is the §5 honesty gate encoded in the type.

---

## §4 — The fallback ladder (E2-CAP) and the per-OS selector

Today the rung is a static field. The ladder makes capture-backend selection a **measured, ordered
descent** with a reason code at each refusal:

```
select_capture_backend(os_build, target):
  if !GraphicsCaptureSession::IsSupported(os_build):   # build < 1803
      → skip WGC, reason WGC_UNSUPPORTED_OS
  prefer per OS build (dossier §2-C, [V1] LSFG ladder shape):
    WGC   — best compatibility + cross-GPU default (our multi-GPU edge); no exclusive-FS
    DD    — only on the display's GPU (HYBRID_DD_WRONG_GPU otherwise); 24H2-MPO caveat
    GDI   — last resort (NEW rung): BitBlt/PrintWindow; lowest fidelity, no HW cursor, slow
  each rung that fails → its reason code → descend; exhausted → CAPTURE_INIT_FAILED
```

**Honest scope of the ladder (NOT a floor-beater):**
- The GDI rung is a **last-resort correctness rung**, not a quality play — it reaches surfaces WGC/DD miss
  on old builds, at a fidelity + latency cost stated plainly. It is the dossier's "GDI = last resort"
  (§2-C `[V1]`).
- The per-OS selector mirrors LSFG's ladder SHAPE (DXGI default / WGC / GDI, switched per Windows version —
  dossier §2-C `[V1]`); it is **NOT** a claim that we replicate LSFG's years of per-title validation —
  that is the asymptotic remainder (§1), tracked as passing-cell count.
- **24H2 DD/MPO breakage** and **"NVIDIA 566.36 restored DXGI on 24H2"** are `[V2]`, driver-version-fragile
  (dossier §6.4) — the selector MUST NOT hard-code a driver assumption; it probes capability
  (`IsSupported`, the `DuplicateOutput1` HRESULT) and falls back, rather than gating on a version string.

**Default-off discipline (dogma, MUST).** The auto-selector and the GDI rung ship behind an opt-in flag
(strategies S4/S5); with the flag off, `capture_api` resolution is **byte-identical** to today (the static
`CA_DD`/`CA_WGC` branch). The existing `--capture-api`/`--window` behaviour is preserved exactly.

---

## §5 — The floors this plan NAMES and does NOT plan to beat

Per the vista §5 and dossier §3, the following are **irreducible for the external-capture class**. This
plan's contract is to **surface each via its reason code** (§3) — never to re-propose defeating it:

| Floor | Why irreducible | Reason code that surfaces it |
|---|---|---|
| **Exclusive-fullscreen capture** | bypasses DWM; only injection reaches it = ban (vista §5; dossier §2-A `[V1]`) | `EXCLUSIVE_FULLSCREEN` (+ switch-to-borderless advisory) |
| **Protected/HDCP content → black** | OS-marked; black to ALL OS capture paths, no software workaround (dossier §2-A `[V1]`) | `PROTECTED_CONTENT` |
| **Hybrid-laptop dGPU Desktop Duplication UNSUPPORTED** | `DuplicateOutput1` → `DXGI_ERROR_UNSUPPORTED` by design (dossier §2-A `[V1]`) | `HYBRID_DD_WRONG_GPU` (→ force WGC) |
| **NVIDIA G-Sync OFF for a non-hooking overlay** + the +1 DWM-compose frame | a non-hooking external overlay can't drive the game's flip chain; shared exactly with LSFG (vista §5; already stated in-repo `main.cpp:3223-3231`) | `VRR_WILL_BE_DISABLED` (advisory) |
| **Anti-cheat certainty on kernel titles** | discretionary + server-side; asymptotic, not certifiable (vista §5; family F1) | `ANTICHEAT_UNVERIFIED` (advisory; campaign is F1) |

These are NAMED here so no future pass re-promotes one as a win (vista I2 / §0 governing framing).

---

## §6 — Plan-tier classification (why Tier 1, not Tier 2)

Per [`../canon/PLAN_TIER_PROTOCOL.md`](../canon/PLAN_TIER_PROTOCOL.md) §1.1, walking the five triggers:

1. **Crash / device-loss / hang.** The reason layer is cold-path startup diagnostics + advisory prints (the
   F2 precedent, `main.cpp:3223`). The per-OS selector is a backend choice made ONCE at init, before the
   present loop. The GDI rung is a self-contained capture path — **no cross-queue GPU sync, no fence/command-
   buffer reuse, no queue-family ownership change** (the presumptive-Tier-2 list). **Does not fire.**
2. **Concurrency.** No new cross-thread sharing on a hot path; the probe + selection run at init on the main
   thread before the capture/present threads spawn. **Does not fire.**
3. **Security.** No new privilege, injection surface, or untrusted-input path. The advisory strings are
   emitted to stdout (no network egress here). **Does not fire.**
4. **Data loss / irreversibility.** None. **Does not fire.**
5. **A dogma at stake.** The byte-identical-off dogma BINDS the new flags (§4) — but it is *honored by
   construction* (default-off, the static path unchanged), not *risked in a way that needs explicit
   treatment*. The no-game-cap dogma is untouched. **Does not fire** (the trigger is "risks violating a
   dogma in a way that needs explicit treatment"; here it is a standard discipline, not a hazard).

**Verdict: Tier 1** — MASTER_PLAN + IMPLEMENTATION_STRATEGIES, **no RISK_REGISTER**. The vista (e) tags
each E2 lever T1; this confirms it.

**The one borderline, stated honestly (§4.4 / D6).** The **GDI rung** introduces a new capture backend. If
a future revision makes GDI share the WGC/DD ring buffers, command pools, or the cross-device upload path
(rather than its own isolated D3D11-staging copy as designed in strategies S5), that sharing could become a
concurrency/crash trigger and would **escalate THAT revision to Tier 2** with a RISK_REGISTER. As scoped
here (isolated backend, its own staging, behind a default-off flag) it stays Tier 1. The escalation
condition is recorded so the boundary is not crossed silently.

---

## §7 — Reference drift caught (FDP §2.4 — what the vista/dossier cited stale)

Verifying first-hand surfaced two stale anchors; this plan cites the corrected sites and records the drift:

1. **Vista §1-A2/A3 and dossier §1 cite "WGC live `main.cpp:3479-3489`".** Those lines are now STAGE-106
   `--upload-xfer` declarations (verified first-hand). The actual WGC backend init is `main.cpp:4018-4031`;
   the WGC format/dimension setup is `main.cpp:1868-1871` and `main.cpp:3093-3098`. This plan uses the
   corrected sites. (The repo has churned since the vista was written; line-drift is expected and is exactly
   why FDP §2.2 mandates re-verification.)
2. **Vista §1-A3 says "no `SetColorSpace1` anywhere (`PresentSurface.cpp:206`)".** First-hand,
   `SetColorSpace1` IS now present at `PresentSurface.cpp:384-393` (an sRGB call; an A3 change landed since
   the vista). This is an HDR-family fact, not E2, and this plan does not rely on the stale line — recorded
   only so the drift is not propagated.

These do not change any E2 conclusion; they correct the anchors so this plan's references are verifiable
today.

---

## §8 — Phases and gates

| Phase | Deliverable | Gate (the §1 closeable test, or its precursor) | Tier |
|---|---|---|---|
| **E2-P1** | The reason-code taxonomy header + the `REASON_UNCLASSIFIED` tripwire (strategies S2) | builds; the enum is the single source; tripwire wired | T1 |
| **E2-P2** | Pre-flight probes wired to codes: `IsSupported`, `DuplicateOutput1` HRESULT, `--window` not-found, first-frame zero-luma (strategies S3) | each detectable condition emits its code on a contrived trigger | T1 |
| **E2-P3** | The advisory print layer (mirror the F2 precedent `main.cpp:3223`) — each code → its honest string (strategies S1) | every code prints its advisory; no generic message remains | T1 |
| **E2-P4** | The per-OS capture-backend selector + the GDI last-resort rung, behind a default-off flag (strategies S4/S5) | flag-off byte-identical (diff=0 vs today); flag-on descends with a code at each refusal | T1 |
| **E2-P5** | The E2-MATRIX living artifact + the harness that asserts zero `REASON_UNCLASSIFIED` over the fixed test set (strategies S6) | **★ E2-GRACE CROSSED**: every non-`works` cell carries a code; tripwire count = 0 | T1 |

**Dependency spine:** P1 → P2 → P3 (the reason layer) is independent of and lighter than P4 (the ladder);
P5 (the matrix harness) consumes both. The reason layer (P1–P3) is the highest-leverage, lowest-blast
slice and SHOULD land first (it is the closeable fixed point; the ladder is the asymptotic-reach
improvement).

---

## §9 — What this plan does NOT claim

- It does **not** claim the matrix will ever be exhaustive — library/AC drift is asymptotic (§1).
- It does **not** claim to capture exclusive-fullscreen, defeat HDCP-black, or restore NVIDIA G-Sync —
  those are NAMED floors (§5), surfaced not solved.
- It does **not** claim the per-OS selector replicates LSFG's per-title validation corpus — only the ladder
  SHAPE; the corpus is the tracked metric, not a deliverable.
- It does **not** build the VRR independent-flip probe or the HDR present path (T2 / family A3 — cross-
  linked, out of scope).
- The GDI rung's fidelity/latency cost is stated as a **cost**, not hidden as a win.
