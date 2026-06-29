# FG_TRUST_ANTICHEAT_PLAN_IMPLEMENTATION_STRATEGIES — the concrete edit sites

> **Diátaxis type:** Planning / design (Explanation). **Status:** `designed` (no code shipping).
> **Plan tier:** Tier 2 — sibling of [`FG_TRUST_ANTICHEAT_PLAN_MASTER_PLAN.md`](FG_TRUST_ANTICHEAT_PLAN_MASTER_PLAN.md)
> and [`FG_TRUST_ANTICHEAT_PLAN_RISK_REGISTER.md`](FG_TRUST_ANTICHEAT_PLAN_RISK_REGISTER.md). Per
> PLAN_TIER_PROTOCOL §2.3, **each edit site below cites its risk ID** so a reviewer can trace edit → risk.
>
> **Verifiable-reference mandate (FDP §2).** Every `path:line`, symbol, and API below was opened and
> confirmed first-hand this session. Where a line is a *target for new code* it is marked "INSERT near"
> with the verified anchor, not an invented post-edit line number.

This document is the per-strategy edit map. The MASTER_PLAN holds the why and the phasing; this holds the
where and the how, with the byte-identical-off discipline and the validation per piece.

---

## S0 — Conventions (binding)

- **Flag placement:** all new flags go in the `parse_extra` lambda (`apps/render_assistant/src/main.cpp:1084`,
  verified) — the MSVC C1061 (≤127 `else if` per chain) dodge that every recent flag uses (e.g.
  `--async-present` at `:1118`, `--force-single-gpu` at `:1146`, `--pin` at `:1159`, all verified). New
  flags MUST NOT be added to the main `else if` chain at `:1199` onward.
- **Default-off, byte-identical-off (DOGMA, re-asserted by vista C2):** every flag here defaults OFF; OFF
  ⇒ the corresponding field keeps its current default ⇒ the present/window path is bit-identical to today.
  This is RISK **SR1**'s mitigation and is validated in S-VALIDATE.
- **Honesty (FDP §4, MASTER_PLAN §3):** no print, help-text, or doc string may contain "we believe it is
  safe" or an equivalent unqualified claim. This is RISK **SR2**.

---

## S1 — A1: the per-engine trust MATRIX (doc artifact) · risk: none (T0)

**Action.** Author `docs/planning/FG_TRUST_MATRIX.md` (a NEW living artifact) that lifts the dossier's §4
matrix into a maintained table whose **last column is the survived-account-hours evidence column**, zeroed.

**Why a separate doc, not inline in the dossier.** FDP §4.6 single-source-of-truth: the dossier is the
*analysis* (frozen SOTA); the matrix is the *living evidence ledger* the campaign mutates. They have
different lifecycles. The matrix links back to the dossier §4 for the rationale of each verdict; it does
not re-argue them.

**Columns (MANDATORY).** `Engine | Class (CLOSEABLE per-title / ASYMPTOTIC) | PhyriadFG verdict (HEURISTIC
until evidenced) | survived-account-hours (per title × AC-version, default 0) | last-reset (AC-version) |
closeable criterion`. Seed the rows from the dossier §4 table (BattlEye / EAC / VAC / Vanguard / BF6
Javelin), every verdict tagged HEURISTIC/UNTESTED.

**Hard rule (RISK SR3).** A cell's survived-account-hours **MUST reset to 0** on any anti-cheat update; the
doc states this as a normative MUST and the campaign procedure (S4) enforces it on append. No cell is ever
"DONE" on the kernel tier — the metric does not converge there (MASTER_PLAN §7 floor).

**Register.** Add `FG_TRUST_MATRIX.md` to `FORMAL_DOCUMENTS_REGISTER.md` in the same change (FDP §7).
*(Note: the supervisor owns the register edit — see this plan's delivery note; do not edit it from a
parallel sibling.)*

**Validation.** The matrix exists, every cell HEURISTIC/UNTESTED, the survived-hours column present and 0,
no "safe" string (grep). Binary.

---

## S2 — A2: the in-product trust GATE (startup print) · risk: SR2 (honesty over-claim)

**Edit site.** `apps/render_assistant/src/main.cpp` — INSERT a new informational block immediately AFTER the
F2/VRR print block (the block spans `:3223-3232`, verified; insert at `:3233` before
`const bool single_gpu = ...`). Mirror that block's shape exactly: a scoped `{ ... }`, always-on, no
behavior change.

**Behavior.**
```
{   // F1 (FG_TRUST_ANTICHEAT §3): the honest anti-cheat posture at startup. Heuristic + discretionary +
    // asymptotic — NEVER "we believe it's safe". Informational only (no refusal, no exit).
    std::printf("[ra] F1/anti-cheat (honest): external-capture, NO injection (we never hook the game). "
                "Safety is HEURISTIC + vendor-discretionary, NOT certifiable. This title's engine is "
                "UNTESTED here unless evidenced in FG_TRUST_MATRIX.md.\n");
    // kernel-AC tier → RECOMMEND-AGAINST ranked/competitive use until the campaign evidences survived-hours
    std::printf("[ra] F1/kernel-AC: on Vanguard / BF6 Javelin (kernel + server-side capture) the overlay is "
                "visible regardless of injection; RECOMMEND-AGAINST ranked/competitive use. The WDA overlay "
                "exclusion is a shared cheat-signal (parity with LSFG); --capturable drops it (go-beyond, "
                "default off).\n");
}
```

**Risk SR2 mitigation (in the code itself):** the strings above contain **no** "safe" claim; they state
HEURISTIC + UNTESTED + RECOMMEND-AGAINST. The reviewer gate (PLAN_TIER §4) confirms the prohibited string is
absent (`grep -i "believe.*safe\|is safe" ` over the diff = 0).

**Per-title classification (v1 = minimal).** v1 prints UNTESTED unconditionally (the matrix has no evidenced
cell yet). A future increment MAY read a per-title engine profile and print the matched regime — but it
**MUST NOT** print a verdict the matrix has not evidenced (RISK SR2 again: do not fabricate a per-title
"PROBABLY-SAFE"; UNTESTED is the honest default).

**Byte-identical note.** This print is always-on and additive; it does not alter the present path. It is
*not* a default-off flag because it changes no behavior — it only adds stdout (the same stance as the F2
print at `:3223`, which is unconditional). If the operator prefers it gated, a `--no-trust-banner` hatch is
trivial; the default posture is "always tell the honest truth at startup".

**Validation.** Run with WGC capture; confirm the two lines print; confirm no "safe" string; confirm FG
output is unchanged from a run before the edit (the print is orthogonal to the pipeline).

---

## S3 — A3: the `--capturable` WDA-drop lever · risk: SR1, CR2 (T2 — the gated piece)

The seam already exists (MASTER_PLAN §1.1): `PresentSurfaceDesc::capture` gates the WDA call. Three small
edits.

### S3.1 — config field
**Edit site.** The `Config`/`cfg` struct in `apps/render_assistant/src/main.cpp` (the struct `parse_extra`
mutates; fields like `c.async_present`, `c.force_single_gpu`, `c.present_colorspace` live there — verified
at `:1118, :1146, :1149`). ADD `bool capturable = false;` (RISK **SR1**: default false).

### S3.2 — the flag (in parse_extra)
**Edit site.** INSERT near `apps/render_assistant/src/main.cpp:1146` (next to `--force-single-gpu`, same
convention).
```
if(!std::strcmp(arg,"--capturable")){ c.capturable=true; std::printf("[ra] --capturable: DROP the overlay's "
  "WDA_EXCLUDEFROMCAPTURE (FG_TRUST_ANTICHEAT §2-B). The overlay becomes software-capturable -> removes the "
  "WDA cheat-signal (PARITY with LSFG, so this is a GO-BEYOND, not a fix) AT THE COST of the overlay now "
  "appearing in your own captures/streams/screenshots. DEFAULT OFF (byte-identical when off).\n"); return 0; }
```
RISK **SR1** mitigation: default OFF; the help-text states the honest trade-off and the parity framing
(MASTER_PLAN §3) — no "safe" claim (RISK SR2).

### S3.3 — plumb to the descriptor
**Edit site.** `apps/render_assistant/src/main.cpp:6976-6985` (the `pp::PresentSurfaceDesc psd{}; …` block,
verified — `psd.capture` is currently **never set**, so it defaults to `ExcludeFromCapture`). ADD exactly
one line in that block:
```
psd.capture = cfg.capturable ? pp::CaptureAffinity::Normal : pp::CaptureAffinity::ExcludeFromCapture;
```
- RISK **SR1** (byte-identical-off): when `cfg.capturable==false` this assigns the SAME value the struct
  already defaults to (`ExcludeFromCapture`, header `:81`, verified) → no behavior change → byte-identical.
- RISK **CR2** (crash): `PresentSurface.cpp:398-403` (verified) applies WDA *after* the swapchain is built
  and is soft-fail (`wda_ok==false` surfaces via `capture_excluded()`, not a create() failure). Setting
  `capture=Normal` simply skips the call (`if (desc.capture == ExcludeFromCapture)` is false) — it does NOT
  touch the swapchain, the click-through styles, or the OwnWindow contract. No crash surface. The mitigation
  is structural: the WDA branch is independent of every other branch in `create()`.

**No window-style change in v1.** The fingerprint de-fingerprint (changing `WS_EX_LAYERED|TRANSPARENT` /
the topmost / the sizing at `PresentSurface.cpp:283-310`) is **DEFERRED to P3** (MASTER_PLAN §5) — it risks
the click-through and the `Style::OwnWindow` no-lock-out contract (RISK **CR1**) and the dossier §7 flags
its efficacy as UNVERIFIED. v1 drops only the WDA flag, the single strongest signal (dossier §2-B★).

### S3.4 — observability
`capture_excluded()` (`PresentSurface.hpp`, reported at `main.cpp:6998` via
`ra_surface.capture_excluded()?"yes":"no"` — verified) already prints whether the overlay is excluded. With
`--capturable` it MUST print "no". This is the runtime validation hook for S-VALIDATE.

**Validation (the T2 gate — see S-VALIDATE for the full list).** Default-off ⇒ diff-clean vs FG-disabled
present path; ON ⇒ `capture_excluded()` reports "no" + a software screenshot (Lightshot/PrintScreen) now
captures the overlay, confirmed on the rig.

---

## S4 — A4: the real-rig empirical campaign (procedure) · risk: SR3 (stale evidence)

This is the dossier §4 "Real-rig test protocol" operationalized as a repeatable procedure. It is a
*document + manual runbook*, not code; the RUN is blocked on hardware + throwaway accounts (MASTER_PLAN §5
P4).

**Procedure (per the dossier §4, cite it; do not re-derive):**
1. **Throwaway accounts only** — never the operator's main (bans may be unappealable). One per engine.
2. **Per (engine × title × AC-version × GPU-vendor):** confirm attach or record a launch-block; ≥N hours
   active multiplayer (N≥10, ≥2 sessions, ≥2 driver versions, ≥1 AC-update cycle); run **two arms —
   `--capturable` OFF (WDA on) vs ON (WDA off)** to isolate the §2-B★ signal; log every kick / launch-block
   / trust-flag change / ban; screenshot account status before+after.
3. **F2 arm** (sibling, opportunistic): read the monitor OSD for VRR-active per vendor; photo evidence.
4. **Append** survived-account-hours to the matrix cell. RISK **SR3** mitigation: **reset the cell to 0 /
   UNTESTED on ANY anti-cheat update** — the procedure's append step checks the AC-version against the
   cell's `last-reset` and zeros it on mismatch.
5. **Gate:** until a cell meets N, the in-product trust gate (S2) classifies the title UNTESTED;
   Vanguard/Javelin default to RECOMMEND-AGAINST regardless of survived-hours.

**Honesty:** the campaign produces *evidence and survived-hours*, never a "safe" verdict (MASTER_PLAN §3,
§7 floors). The kernel-tier metric never converges — that is the floor, named, not a defect to fix.

---

## S-VALIDATE — the validation matrix (what makes each piece "done")

| Piece | Gate (binary, first-hand) | Risk closed |
|---|---|---|
| S1 matrix | doc exists; all cells HEURISTIC/UNTESTED; survived-hours col = 0; no "safe" string (grep) | — |
| S2 gate print | both lines print on a WGC run; no "safe" string in the diff (grep); FG output unchanged | SR2 |
| S3 `--capturable` OFF | the present path byte-identical to FG-disabled (the C2 diff = 0 check); `capture_excluded()` = "yes" | SR1 |
| S3 `--capturable` ON | `capture_excluded()` = "no"; a software screenshot captures the overlay; no crash over a 30 s run | CR2 |
| S3 OwnWindow regression | with `--capturable` ON **and** OwnWindow mode, the no-lock-out contract still holds (foreground-yield + watchdog fire on a stall) — confirm the watchdog at `PresentSurface.cpp:405-419` is untouched | CR1 (deferred-P3 boundary) |
| S4 campaign | ≥1 cell has appended survived-hours, evidenced; the AC-update reset rule exercised once | SR3 |

The **P2 commit gate** (PLAN_TIER §3): every RISK_REGISTER risk `mitigated`/`accepted` before
`--capturable` lands. The matrix (S1) and the gate print (S2) carry no qualifying risk and MAY land
independently as T0 increments.

---

## S-REFERENCES (verifiable, first-hand)

- `apps/render_assistant/src/main.cpp:1084` (parse_extra), `:1118/:1146/:1149/:1159` (the flag convention),
  `:3223-3232` (F2 print to mirror), `:6976-6985` (psd build; `psd.capture` unset), `:6998`
  (`capture_excluded()` print).
- `framework/render/present/include/phyriad/render/present/PresentSurface.hpp:69-72` (`CaptureAffinity`),
  `:81` (`capture` default), `:47-60` (`Style`).
- `framework/render/present/src/PresentSurface.cpp:283-310` (fingerprint styles), `:398-403` (gated WDA),
  `:405-419` (OwnWindow watchdog — the CR1 boundary).
- Dossier §4 (the matrix + the campaign protocol), §2-B★ (the WDA signal), §3 (the reframe).
