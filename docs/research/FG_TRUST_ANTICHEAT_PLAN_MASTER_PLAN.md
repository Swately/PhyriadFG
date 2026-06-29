# FG_TRUST_ANTICHEAT_PLAN_MASTER_PLAN — the per-engine trust posture (matrix + the WDA de-fingerprint lever)

> **Diátaxis type:** Planning / design (Explanation). **Status:** `designed` — no code in this plan is
> shipping; the levers are specified, not built. The dossier it implements is `measured`/`designed`.
> **Plan tier:** **Tier 2 (risk-bearing)** per [`../canon/PLAN_TIER_PROTOCOL.md`](../canon/PLAN_TIER_PROTOCOL.md)
> §1.1 — the de-fingerprint lever changes an anti-cheat-relevant safety property and re-touches the
> `Style::OwnWindow` no-lock-out contract; it therefore ships with the
> [`FG_TRUST_ANTICHEAT_PLAN_RISK_REGISTER.md`](FG_TRUST_ANTICHEAT_PLAN_RISK_REGISTER.md) (this document's
> sibling, §6).
>
> **The implementation triad (one set, mutually linked):**
> - this MASTER_PLAN (the why, the walls, the architecture, the phases/gates);
> - [`FG_TRUST_ANTICHEAT_PLAN_IMPLEMENTATION_STRATEGIES.md`](FG_TRUST_ANTICHEAT_PLAN_IMPLEMENTATION_STRATEGIES.md)
>   (the concrete edit sites, each citing a risk ID);
> - [`FG_TRUST_ANTICHEAT_PLAN_RISK_REGISTER.md`](FG_TRUST_ANTICHEAT_PLAN_RISK_REGISTER.md) (every failure
>   mode, mitigation-as-code, first-hand verification).
>
> **Source of truth.** The SOTA/analysis is DONE: [`FG_TRUST_ANTICHEAT_PRIOR_ART.md`](FG_TRUST_ANTICHEAT_PRIOR_ART.md)
> (the dossier) and [`PHYRIADFG_OBJECTIVE_VISTA.md`](PHYRIADFG_OBJECTIVE_VISTA.md) §1-F1 / §3 / §5. This
> plan does NOT re-derive findings; it cites them and plans the build. Per FDP §4.6 (single source of
> truth) the survived-account-hours numbers, the per-engine verdicts, and the floor list live in the
> dossier/vista — this plan links, it does not copy.
>
> **Normativity (BCP 14).** MUST / MUST NOT / SHOULD / MAY per RFC 2119/8174 only where a real safety or
> dogma requirement binds.
>
> **Verifiable-reference mandate (FDP §2).** Every in-repo `path:line` and API below was opened and
> confirmed first-hand this session. External claims defer to the dossier's leveled sources (`[VS]`/`[V1..3]`).

---

## §0 — Scope and non-scope

**In scope (objective family F1 — anti-cheat / account safety as a MEASURED trust posture):**
1. The **per-engine trust MATRIX** lifted from the dossier (§4 of the dossier) into a living, evidence-
   graded in-repo artifact, with the **survived-account-hours** metric as its progress column.
2. The **WDA de-fingerprint lever** — a default-off, byte-identical-off opt-in (`--capturable`) that drops
   `WDA_EXCLUDEFROMCAPTURE` and reduces the overlay-window enumeration signature, exposed because it is the
   single strongest cheat-class signal under our control (dossier §2-B★).
3. An **in-product trust GATE** — an informational startup statement (mirroring the existing F2/VRR print)
   that classifies the captured title's anti-cheat engine and states the honest posture (UNTESTED /
   RECOMMEND-AGAINST for kernel titles), never "we believe it is safe."
4. The **real-rig empirical campaign** procedure (the only thing that converts `[V2/V3]` into evidence) —
   a documented protocol, not code.

**Non-scope (named, not planned):**
- **F2** (foreign-present side effects: VRR/G-Sync, MPO, fullscreen) — its honesty print already ships
  (`apps/render_assistant/src/main.cpp:3223-3232`, verified first-hand). F2 is a sibling objective in the
  same dossier; this plan does not touch it.
- **Any injection path** — present-hook capture / input hooks are confirmed-ban (`GAP:670`, `:173`,
  verified) and MUST NOT be built. The plan stays inside the no-injection class.
- **A safety GUARANTEE.** This plan MUST NOT produce a document, a print, or a gate that states the tool is
  "safe". The product of this plan is *evidence and honest classification*, asymptotic against a vendor-
  controlled moving target (§3, §5 floors).

---

## §1 — Why (the verified premise change)

The product's stated reason to exist is "external-capture only (anti-cheat-safe; … must run on
Battlefield 6)" — `FG_ARCHITECTURE_DCAD_MASTER_PLAN.md:29-31` (verified first-hand). The dossier's `[VS]`
finding is that this premise is **materially weaker than the repo assumes**:

1. "Anti-cheat-safe by construction" overstates it: no-injection removes the strongest signal (the hook)
   but not all signals; safety is **heuristic, discretionary, asymptotic** against a vendor-controlled
   heuristic (dossier §2-C, §3-F1-b).
2. The overlay is **WDA-excluded** — and WDA-exclusion is itself an active cheat-class signal that anti-
   cheats fingerprint (dossier §2-B★, `[VS]`). **RESOLVED 2026-06-22 (Lightshot):** LSFG carries the same
   signal → this is **PARITY**, not a self-inflicted disadvantage; dropping it is an *optional go-beyond*,
   not a mandatory fix (vista §3-2, dossier §3-F1-d).
3. "Must run on BF6" (`DCAD:29`, verified) is a **requirement, not a verified capability**, and the only
   real-world data (reported LSFG BF6 bans, EA non-clearance) makes BF6/Javelin the **worst-case** title,
   not the safe tier (dossier §2-C★, §3 honesty flag).

The honest posture the dossier prescribes is the **per-engine trust matrix with a survived-account-hours
metric — never "we believe it's safe."** This plan builds exactly that.

### §1.1 — What is ALREADY in the code (first-hand)

The build is not greenfield. Verified this session:

- The WDA-exclusion is **already gated** behind a descriptor field — `PresentSurfaceDesc::capture`
  (`framework/render/present/include/phyriad/render/present/PresentSurface.hpp:81`, default
  `CaptureAffinity::ExcludeFromCapture`), applied only `if (desc.capture == CaptureAffinity::ExcludeFromCapture)`
  at `framework/render/present/src/PresentSurface.cpp:399-401`. The seam for the lever **exists**; main.cpp
  simply never sets `psd.capture` (`apps/render_assistant/src/main.cpp:6976-6985`, verified — `psd.capture`
  is absent → defaults to exclude). The lever is "plumb a flag to this field", not "add capture-affinity".
- The overlay-window fingerprint tuple is assembled at `PresentSurface.cpp:283-310`: `WS_EX_TOPMOST` +
  `WS_EX_NOACTIVATE` always; `WS_EX_LAYERED | WS_EX_TRANSPARENT` for `Style::DcompCt`
  (`PresentSurface.cpp:286-288`); the window is monitor-full-extent (`PresentSurface.cpp:270, 304-310`).
  This is the (game-sized + topmost + layered/transparent) tuple the dossier §2-B names.
- The F2/VRR honesty print at `main.cpp:3223-3232` is the **established pattern** an F1 trust-gate print
  mirrors: a vendor-conditional, always-on, behavior-free startup statement of the honest floor.
- There is **no** existing `--capturable` / trust flag and **no** trust gate today (grep for
  `trust|--capture[^-]|capturable|de-fingerprint` over `main.cpp` returns only `--capture-api`, verified).

---

## §2 — The objective, fixed (from the vista §1-F1-f)

F1 is **NOT a single binary** — it is a **matrix of per-engine binaries**. The two regimes (cite the
vista/dossier; do not re-derive):

- **VAC / BattlEye / EAC — CLOSEABLE per-title (FIXED-POINT).** The closeable criterion: *a throwaway
  account runs PhyriadFG attached to a real protected title for ≥N hours of active multiplayer across
  ≥2 driver versions and ≥1 anti-cheat update cycle, with zero kick / launch-block / ban and trusted-mode
  retained where applicable — evidenced.* (This is the vista's fixed-point **C3** at the BF6-class instance.)
- **Vanguard / BF6 Javelin (kernel + active capture) — ASYMPTOTIC, not binary.** Server-side discretionary;
  honest "done" = *survives the campaign AND the operator accepts residual risk + recommends against ranked
  use.* The single tracked metric: **cumulative survived-account-hours per (engine × title × anti-cheat-
  version), reset to 0 on any anti-cheat update.**

**The matrix is the deliverable; the campaign feeds it; the lever and the gate are the code around it.**

---

## §3 — The reframe that governs every artifact (vista §3 — load-bearing)

Per the vista §3 (`[VS]`, "MUST steer the framing of the whole product, not sit in a footnote"), every
document, print, and gate this plan produces MUST hold this posture:

1. Safety is **heuristic / discretionary / asymptotic**, NOT certifiable. No artifact states or implies the
   tool is "safe".
2. The WDA cheat-signal is **PARITY with LSFG** (not self-inflicted) → the de-fingerprint lever is an
   *optional go-beyond*, default-off, NOT a mandatory remediation.
3. BF6/Javelin is the **asymptotic-risk tier**; `DCAD:29` "must run on BF6" is treated as a requirement
   **currently UNMET**; the gate RECOMMENDS-AGAINST ranked/competitive use on kernel anti-cheats until the
   campaign provides evidence.
4. The honest posture is the **matrix + survived-account-hours**, never "we believe it's safe."

> **BCP 14:** any artifact produced under this plan **MUST NOT** contain the string "we believe it is safe"
> or an equivalent unqualified safety claim. This is the dossier's and the task's hard rule.

---

## §4 — Architecture: four artifacts, three of them cheap

| # | Artifact | Kind | Tier of the piece | Byte-identical-off |
|---|---|---|---|---|
| **A1** | The per-engine trust MATRIX (living doc) | doc | T0 (a doc) | n/a |
| **A2** | The in-product trust GATE (startup print) | code, informational | T0-mechanically (mirrors F2 print) | YES — it is print-only, no behavior change |
| **A3** | The WDA de-fingerprint lever (`--capturable`) | code, opt-in | **T2** (touches the safety boundary + the OwnWindow contract) | YES — default-off ⇒ `psd.capture` unset ⇒ bit-identical to today |
| **A4** | The real-rig empirical campaign | procedure | T0 (a documented procedure; the RUN needs hardware + accounts) | n/a |

**The tier of the PLAN is the max of its pieces = T2**, driven solely by A3. A1/A2/A4 are individually
mundane; they are planned here for coherence but carry no qualifying risk. The RISK_REGISTER (§6) treats
**only A3** (and the one second-order risk A2 could introduce — a print that over-claims).

### §4.1 — A3 design contract (the only risk-bearing piece)

- **Flag:** `--capturable` (default OFF). Lives in `parse_extra` (the C1061-dodge convention every recent
  flag follows — `main.cpp:1084` onward, verified), NOT the main `else if` chain.
- **Effect when ON:** (1) set `psd.capture = pp::CaptureAffinity::Normal` at `main.cpp:6976-6985` so
  `PresentSurface.cpp:399-401` skips `SetWindowDisplayAffinity` → the overlay is software-capturable; (2)
  OPTIONALLY de-fingerprint the window styles — **deferred to a second sub-lever** (see §5 phasing); the
  v1 lever is the WDA drop alone (the single strongest signal, dossier §2-B★).
- **Effect when OFF (the dogma):** `psd.capture` stays at its `ExcludeFromCapture` default → not a single
  byte of the present path changes → **byte-identical to today** (DOGMA `D` byte-identical-off;
  re-asserted by the vista fixed-point **C2**).
- **The honest trade-off the flag's help-text MUST state:** a capturable overlay then appears in the
  user's own captures/streams/screenshots; it removes the WDA cheat-signal but the signal is shared with
  LSFG (PARITY), so this is a go-beyond, not a fix.

### §4.2 — A2 design contract (the trust gate)

- An always-on, informational startup print mirroring `main.cpp:3223-3232` (the F2/VRR print), emitted
  near it. It MUST NOT change behavior (no refusal, no exit) in v1 — classification + honest warning only.
- It classifies the captured title's engine into the matrix's regimes when that is **known first-hand**
  (e.g. the operator-supplied title, or a future per-title profile). When the engine is unknown, it prints
  **UNTESTED** + the §3 posture. Kernel engines (Vanguard/Javelin) → **RECOMMEND-AGAINST** regardless.
- It MUST NOT fabricate a per-title verdict the matrix has not evidenced (FDP §2). "UNTESTED" is the honest
  default; the gate reads the matrix, it does not invent cells.

---

## §5 — Phases and gates

The plan is sequenced cheapest-first; each phase is independently shippable.

| Phase | Deliverable | Gate (binary) | Tier |
|---|---|---|---|
| **P0** | A1 — author the trust matrix doc + register it | the matrix exists, every cell HEURISTIC/UNTESTED, survived-account-hours column present and zeroed; in FORMAL_DOCUMENTS_REGISTER | T0 |
| **P1** | A2 — the trust-gate print | startup prints the engine classification + the §3 posture; no behavior change; UNTESTED is the default; contains NO "safe" claim | T0 |
| **P2** | A3 — the `--capturable` WDA-drop lever | default-off ⇒ byte-identical (diff = 0 vs FG-disabled present path); ON ⇒ `capture_excluded()` reports false + the overlay is software-capturable, verified on the rig; the RISK_REGISTER's risks all `mitigated`/`accepted` | **T2** |
| **P3** (deferred) | A3-b — the window de-fingerprint sub-lever | a DComp child-visual / non-layered recipe reduces the enumeration signature WITHOUT breaking click-through or the OwnWindow no-lock-out contract | T2 (re-uses the same register; **untested → see §7**) |
| **P4** (campaign) | A4 — run the real-rig protocol on ≥1 throwaway account | survived-account-hours appended to ≥1 matrix cell, evidenced (screenshots + account-status log); cell stays UNTESTED until the gate's N is met | T2 (the RUN; blocked on hardware + throwaway accounts) |

**P2 is the gated piece** — it MUST NOT commit while any RISK_REGISTER risk is `open` (PLAN_TIER §3).
**P3 is explicitly deferred** because the dossier §7 flags "whether a pure-DComp child-visual evades the
fingerprint" as UNVERIFIED — building it before a real-rig test would be guessing; the v1 lever (WDA drop)
is the high-confidence piece.

---

## §6 — Risk treatment (defer to the register)

This is Tier 2; the failure modes of A3 (and the one A2 over-claim risk) are enumerated, each with its
mitigation-as-code and first-hand verification, in
[`FG_TRUST_ANTICHEAT_PLAN_RISK_REGISTER.md`](FG_TRUST_ANTICHEAT_PLAN_RISK_REGISTER.md). Summary only here
(the register is authoritative):

- **SR1 (security/boundary):** the lever weakens a safety boundary if it flips ON by accident → default-off
  + byte-identical-off invariant.
- **CR1 (crash/contract):** the de-fingerprint sub-lever (P3) could break the `Style::OwnWindow` no-lock-out
  contract (foreground-yield + watchdog, `PresentSurface.cpp:405-419`) → P3 deferred; v1 touches only WDA.
- **SR2 (honesty/dogma):** an artifact states "safe" → §3 BCP-14 prohibition + reviewer gate.
- **CR2 (crash):** `psd.capture=Normal` interacts with the click-through/flip recipe → verified the WDA call
  is independent of the swapchain (`PresentSurface.cpp:398-403` is post-swapchain, soft-fail).

The pre-commit GATE for P2: **no register risk `open`** (each `mitigated` or `accepted`).

---

## §7 — Honest floors (NAMED, never planned-to-beat — vista §5)

Per the task's hard rule (re-proposing an irreducible as a win is the failure mode), the following floors
are **named and explicitly NOT a target of this plan**:

- **Anti-cheat certainty on kernel titles is not certifiable** — discretionary + server-side; asymptotic
  (vista §5; dossier §3-F1-b). The plan tracks survived-account-hours; it does **not** plan to reach
  "certified safe". The metric **resets to 0 on any anti-cheat update** by design — there is no convergence
  to a closed win on the kernel tier.
- **Kernel anti-cheats see the overlay regardless of injection** (Vanguard server-side screenshot; Javelin
  kernel window enumeration) — the de-fingerprint lever reduces a *user-mode* enumeration signature only; it
  is named here so P3 is not mis-sold as defeating kernel capture (dossier §3-F1-b-2, §7).
- **WDA-exclusion as a cheat-signal is PARITY with LSFG** — dropping it is a go-beyond, not a remediation;
  the plan does not claim it makes us "safer than LSFG" as a shipped fact (it would, on that one axis, but
  the net posture is still asymptotic).
- **No vendor has any published statement about FG tools specifically** — all conclusions are extrapolated
  from the overlay/capture-tool class (dossier §7); the matrix cells inherit this uncertainty and stay
  HEURISTIC.
- **`DCAD:29` "must run on BF6"** is treated as a requirement currently **UNMET**, not a capability to
  assert (dossier §3 honesty flag).

These are not pessimism; they are the dossier's verified structure. The plan's job is to make the posture
*evidenced and honest*, not to manufacture a certainty that does not exist.

---

## §8 — What this plan does NOT claim

- It does not claim the de-fingerprint lever makes any title safe.
- It does not claim BF6/Javelin is or will be safe; it plans to *measure survived-hours* and *recommend
  against ranked use*.
- It does not claim parity findings are lab-confirmed — the LSFG-WDA-parity is strong evidence
  (operator-observed, Lightshot), not a `GetWindowDisplayAffinity` probe result (dossier §5, §7).
- It does not introduce any injection path.

---

## §9 — References (verifiable)

In-repo, all opened first-hand this session:
- `framework/render/present/include/phyriad/render/present/PresentSurface.hpp:47-106` — `Style`,
  `CaptureAffinity`, `PresentSurfaceDesc` (the lever's seam).
- `framework/render/present/src/PresentSurface.cpp:283-310` (the fingerprint window styles), `:398-403`
  (the gated WDA call).
- `apps/render_assistant/src/main.cpp:1084` (`parse_extra`), `:3223-3232` (the F2 print pattern),
  `:6976-6985` (where `psd` is built; `psd.capture` currently unset).
- `docs/planning/FG_ARCHITECTURE_DCAD_MASTER_PLAN.md:29-31, :323-325, :361` (the premise + the open
  decision + the community source).
- `docs/planning/PHYRIADFG_ARCHITECTURE_GAP_AUDIT.md:399, :670, :673, :173` (the F2 floor + the injection
  boundary).

Analysis (single source of truth; this plan links, does not copy):
- [`FG_TRUST_ANTICHEAT_PRIOR_ART.md`](FG_TRUST_ANTICHEAT_PRIOR_ART.md) — the dossier (`[VS]` + leveled
  externals; its §5/§6 carry the could-not-verify list).
- [`PHYRIADFG_OBJECTIVE_VISTA.md`](PHYRIADFG_OBJECTIVE_VISTA.md) §1-F1, §3, §5.

Process:
- [`../canon/PLAN_TIER_PROTOCOL.md`](../canon/PLAN_TIER_PROTOCOL.md),
  [`../canon/FORMAL_DOCUMENT_PROTOCOL.md`](../canon/FORMAL_DOCUMENT_PROTOCOL.md).
