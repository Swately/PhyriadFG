# FG_TRUST_ANTICHEAT_PLAN_RISK_REGISTER — the Tier-2 failure-mode register

> **Diátaxis type:** Planning / design (Explanation, normative outcome). **Status:** `designed` — risks are
> enumerated with mitigation-as-code; status is `open` until the code lands and the verification gate is run
> first-hand. **Plan tier:** Tier 2 — sibling of
> [`FG_TRUST_ANTICHEAT_PLAN_MASTER_PLAN.md`](FG_TRUST_ANTICHEAT_PLAN_MASTER_PLAN.md) and
> [`FG_TRUST_ANTICHEAT_PLAN_IMPLEMENTATION_STRATEGIES.md`](FG_TRUST_ANTICHEAT_PLAN_IMPLEMENTATION_STRATEGIES.md).
>
> **The Tier-2 gate (PLAN_TIER_PROTOCOL §3, hard rule).** The `--capturable` lever (strategy S3) MUST NOT be
> committed while ANY risk below is `open`. Each MUST be `mitigated` (code in + verified first-hand) or
> `accepted` (residual stated, who accepted). A risk cannot be silently dropped.
>
> **Verifiable-reference mandate (FDP §2).** Every `file:line` below was opened first-hand this session.
> A `mitigated` status is `measured`/`verified` (the gate actually run), never `assumed`.

---

## §1 — Why this is Tier 2

The trust plan is mostly mundane (a matrix doc, an informational print, a campaign runbook — all T0). ONE
piece carries a qualifying risk per PLAN_TIER_PROTOCOL §1.1: the **`--capturable` WDA-drop lever (S3)** and
its deferred **window de-fingerprint sub-lever (P3)** touch a **safety boundary** (§1.1-3: "weakens an
existing safety boundary" — here, an anti-cheat-relevant property) and re-touch the **`Style::OwnWindow`
no-lock-out contract** (§1.1-1: the crash/lock-out class). The plan is therefore Tier 2 and carries this
register. The matrix and the gate-print are T0 and are listed only where a second-order honesty risk (SR2,
SR3) attaches to them.

---

## §2 — The register

Fields per PLAN_TIER_PROTOCOL §2.2: **ID · Class · Failure mode (file:line) · Mitigation + attached code ·
Verification · Status.**

### SR1 — the lever weakens the safety boundary by accident (default-on / non-byte-identical)

- **Class:** security (boundary) + dogma (byte-identical-off).
- **Failure mode.** If `--capturable` defaulted ON, or if plumbing it changed the OFF-path present
  behavior, every user would silently drop `WDA_EXCLUDEFROMCAPTURE` (the overlay becomes capturable) and the
  OFF run would no longer be bit-identical to FG-disabled — a regression of vista fixed-point **C2**. Site:
  the descriptor plumb at `apps/render_assistant/src/main.cpp:6976-6985` (where `psd` is built) and the
  config field.
- **Mitigation + attached code.** Default the field OFF, and plumb it so OFF assigns the value the struct
  *already* defaults to:
  ```
  // config struct:   bool capturable = false;                                  // default OFF
  // psd build (:6976-6985):
  psd.capture = cfg.capturable ? pp::CaptureAffinity::Normal
                               : pp::CaptureAffinity::ExcludeFromCapture;        // OFF == the header default (:81)
  ```
  `PresentSurfaceDesc::capture` already defaults to `CaptureAffinity::ExcludeFromCapture`
  (`framework/render/present/include/phyriad/render/present/PresentSurface.hpp:81`, verified first-hand), so
  OFF reproduces the current behavior exactly — no byte changes.
- **Verification.** Build; run with no flags and with `--capturable` absent; confirm `capture_excluded()`
  prints "yes" (`main.cpp:6998`, verified) and the present output diffs **0** against a pre-edit build on the
  same scene (the C2 byte-identical check). Run is REQUIRED before flipping status.
- **Status:** `open` (becomes `mitigated` after the C2 diff = 0 is run first-hand).

### CR1 — the de-fingerprint sub-lever breaks the OwnWindow no-lock-out contract

- **Class:** crash / lock-out (the user's panel held / input locked).
- **Failure mode.** The P3 sub-lever changes the window styles at
  `framework/render/present/src/PresentSurface.cpp:283-310`. The `Style::OwnWindow` no-lock-out contract
  depends on `WS_EX_TRANSPARENT` (hit-test pass-through) + `WS_EX_NOACTIVATE` (no focus steal) +
  the present-thread watchdog at `PresentSurface.cpp:405-419` (force-hide on a stall). Removing
  `WS_EX_TOPMOST` or altering `WS_EX_TRANSPARENT` to de-fingerprint could re-introduce a focus/topmost
  lock-out or break click-through — a user unable to reach the desktop while the plane is displayed.
- **Mitigation + attached code.** **DEFER P3 out of this commit.** The v1 lever (S3) drops ONLY the WDA
  flag (`PresentSurface.cpp:399-401`) and changes **no** window style — the styles at `:283-310` are
  untouched. The mitigation is structural: the committed change does not reach the contract's code. P3, when
  built, MUST keep `WS_EX_TRANSPARENT` + `WS_EX_NOACTIVATE` + the watchdog and validate the no-lock-out
  contract (S-VALIDATE row "OwnWindow regression") before it lands — under a fresh review against this same
  register.
- **Verification.** Confirm first-hand the S3 diff touches only the WDA gate + the psd plumb + the flag, and
  does NOT modify `PresentSurface.cpp:283-310` or `:405-419` (a `git diff` path/line check). With
  `--capturable` ON in OwnWindow mode, run 30 s and confirm the watchdog still force-hides on a simulated
  stall (the contract holds).
- **Status:** `open` → expected `mitigated` by deferral (the change does not touch the contract code) once
  the diff-scope check is run. P3 re-opens it.

### CR2 — `capture=Normal` interacts with the swapchain / click-through and crashes

- **Class:** crash / device.
- **Failure mode.** A worry that skipping `SetWindowDisplayAffinity` (or setting `capture=Normal`) perturbs
  the composition swapchain, the click-through, or DComp setup → a `create()` failure or a present crash.
- **Mitigation + attached code.** The WDA call is **already independent and soft-fail** — verified
  first-hand at `framework/render/present/src/PresentSurface.cpp:398-403`:
  ```
  // ── WDA (probe apply_wda) — failure is soft: overlay works, just capturable ─
  if (desc.capture == CaptureAffinity::ExcludeFromCapture) {
      SetLastError(0);
      impl->wda_ok = SetWindowDisplayAffinity(impl->hwnd, WDA_EXCLUDEFROMCAPTURE) != 0;
      // wda_ok==false surfaces via capture_excluded(); NOT a create() failure (§1.5.3).
  }
  ```
  It runs AFTER the swapchain (`:333-396`) and AFTER colorspace, touches nothing else, and a `false` result
  is already a non-fatal, reported state. `capture=Normal` simply takes the `if`-false branch (no call) — the
  least-effect path. No new crash surface is introduced.
- **Verification.** Run `--capturable` ON for 30 s on the rig (WGC capture, real game); confirm no crash,
  `capture_excluded()` = "no", and a software screenshot captures the overlay. Validation-layer / watchdog
  clean.
- **Status:** `open` (becomes `mitigated` after the 30 s ON run is clean first-hand).

### SR2 — an artifact states or implies "safe" (honesty / over-claim)

- **Class:** dogma (honest-reporting; FDP §4; the task's hard rule).
- **Failure mode.** Any print, help-text, matrix cell, or doc line that says "we believe it is safe" / "is
  anti-cheat-safe" as an unqualified claim. Sites at risk: the S2 gate print (`main.cpp:~3233`, new), the
  `--capturable` help-text (`main.cpp:~1146`, new), the S1 matrix cells.
- **Mitigation + attached code.** The strings are written to state HEURISTIC + UNTESTED + RECOMMEND-AGAINST
  (S2), and the help-text states the PARITY/go-beyond framing + the trade-off (S3.2) — never "safe". The
  MASTER_PLAN §3 makes the prohibition a BCP-14 MUST NOT. Mitigation is the wording itself plus the reviewer
  gate.
- **Verification.** `grep -in "is safe\|believe.*safe\|anti-cheat-safe" ` over the diff = 0; reviewer
  confirms each new string carries the honest posture (PLAN_TIER §4 reviewer-enforced).
- **Status:** `open` (becomes `mitigated` when the grep = 0 on the actual diff + reviewer sign-off).

### SR3 — stale evidence: a survived-hours cell outlives its anti-cheat version

- **Class:** data-integrity / honesty (a stale cell reads as a live safety claim).
- **Failure mode.** A matrix cell shows N survived-account-hours that were earned on a *previous* anti-cheat
  version; after the vendor updates, the number is meaningless but still reads as evidence — the exact "we
  believe it's safe" failure the dossier forbids (dossier §4-step-4, §3-F1-f). Site: `FG_TRUST_MATRIX.md`
  (S1, new doc).
- **Mitigation + attached code.** The matrix carries a per-cell `last-reset (AC-version)` column; the
  campaign append step (S4-step-4) is a documented MUST: *on any anti-cheat update, reset the cell's
  survived-hours to 0 / UNTESTED.* The kernel tier (Vanguard/Javelin) NEVER reaches a closed verdict — its
  metric resets by design (MASTER_PLAN §7 floor). The mitigation is the procedure + the schema column, not a
  daemon (the campaign is manual).
- **Verification.** Exercise the reset once: record a cell, bump the AC-version in the procedure, confirm the
  append step zeros it. The gate-print (S2) shows UNTESTED for any un-evidenced or reset cell.
- **Status:** `open` (becomes `mitigated` when the reset rule is exercised once first-hand).

---

## §3 — Accepted residuals (stated honestly, not hidden)

These are NOT risks the code closes; they are the named floors (MASTER_PLAN §7, vista §5) — `accepted` by
construction because they are irreducible, with the rationale stated:

- **AR1 — kernel-AC certainty is not certifiable** (`accepted`). Vanguard/Javelin are server-side
  discretionary; no code, no campaign, no de-fingerprint makes them certifiably safe. Accepted: the plan
  tracks survived-hours and RECOMMENDS-AGAINST ranked use; it does not claim to close this. Rationale: the
  dossier's `[VS]` floor; re-proposing it as closeable is the failure mode the task forbids.
- **AR2 — kernel capture sees the overlay regardless of injection** (`accepted`). The de-fingerprint lever
  reduces a *user-mode* enumeration signature only; Vanguard's server-side screenshot / Javelin's kernel
  enumeration bypass it. Accepted: P3 is not sold as defeating kernel capture (dossier §7).
- **AR3 — the LSFG-WDA parity is strong evidence, not lab-confirmed** (`accepted`). The PARITY framing rests
  on the operator's Lightshot observation, not a `GetWindowDisplayAffinity` probe (dossier §5, §7). Accepted:
  the framing is stated as strong-evidence, never as a probe result. A future probe would upgrade it.

Acceptance authority: the operator (Swately) per the §3.1 last-word convention; this register records the
residuals so they are visible, not buried.

---

## §4 — Pre-commit checklist (the gate, for the P2 commit of S3)

- [ ] SR1 `mitigated` — C2 byte-identical diff = 0 run first-hand.
- [ ] CR1 `mitigated` — diff-scope check confirms `PresentSurface.cpp:283-310` and `:405-419` untouched; P3
      not in this commit.
- [ ] CR2 `mitigated` — 30 s `--capturable` ON run clean, `capture_excluded()` = "no", overlay captured.
- [ ] SR2 `mitigated` — grep for "safe" claims over the diff = 0; reviewer sign-off.
- [ ] SR3 `mitigated` — the matrix reset rule exercised once (applies to the S1 increment, not strictly the
      S3 commit, but the schema column MUST exist before any survived-hours is recorded).
- [ ] AR1–AR3 `accepted` with the rationale above, operator-acknowledged.

No box may be skipped; an `open` risk blocks the commit (PLAN_TIER §3).

---

## §5 — References (verifiable, first-hand)

- `framework/render/present/src/PresentSurface.cpp:398-403` (the soft, independent, gated WDA call — CR2),
  `:283-310` (the fingerprint styles — CR1), `:405-419` (the OwnWindow watchdog — CR1).
- `framework/render/present/include/phyriad/render/present/PresentSurface.hpp:81` (`capture` default — SR1),
  `:69-72` (`CaptureAffinity`).
- `apps/render_assistant/src/main.cpp:6976-6985` (the psd plumb — SR1), `:6998` (`capture_excluded()` print
  — verification hook), `:3223-3232` (the F2 print the gate mirrors — SR2 site context), `:1146` (the flag
  convention — SR2 site).
- Sibling plan docs (the edit sites + the why): the MASTER_PLAN and the IMPLEMENTATION_STRATEGIES (linked in
  the header). Dossier [`FG_TRUST_ANTICHEAT_PRIOR_ART.md`](FG_TRUST_ANTICHEAT_PRIOR_ART.md) §4 (campaign /
  matrix), §5/§7 (could-not-verify → AR1–AR3).
