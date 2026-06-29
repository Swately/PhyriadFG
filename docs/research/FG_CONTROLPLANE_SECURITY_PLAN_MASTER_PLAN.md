# FG_CONTROLPLANE_SECURITY_PLAN_MASTER_PLAN — securing the telemetry egress + the future control ingress (objective F3)

> **Diátaxis type:** Planning / design (Explanation — goals, non-goals, alternatives, trade-offs).
> **Status:** `designed` — no security code exists yet (the control-plane pillar itself is `designed`, the
> control-ingress is a stub command-ring); every claim below about runtime behaviour is `designed`, never
> `measured`. In-repo `file:line` anchors are first-hand-verified this session (the verification table in §8).
> **Plan tier:** **Tier 2 (risk-bearing)** per [`PLAN_TIER_PROTOCOL.md`](../canon/PLAN_TIER_PROTOCOL.md) §1.1 —
> the control-ingress increment is a **security boundary** (trigger 3) and a **new cross-thread surface**
> (trigger 2: concurrency). It therefore ships with the Tier-1 pair PLUS a
> [`FG_CONTROLPLANE_SECURITY_PLAN_RISK_REGISTER.md`](FG_CONTROLPLANE_SECURITY_PLAN_RISK_REGISTER.md); the three
> are mutually linked (§2.3 of the tier protocol). **Hard gate:** no risk-bearing increment of this plan may be
> committed while any register row is `open`.
> **Normativity (BCP 14):** MUST / MUST NOT / SHOULD / MAY per RFC 2119/8174 — F3 is a real security boundary
> and the requirements below bind.
>
> **Provenance.** Implements objective **F3** from
> [`PHYRIADFG_OBJECTIVE_VISTA.md`](PHYRIADFG_OBJECTIVE_VISTA.md) §1-F3; the SOTA is already done in
> [`FG_CONTROLPLANE_SECURITY_PRIOR_ART.md`](FG_CONTROLPLANE_SECURITY_PRIOR_ART.md). The pillar being secured is
> [`CONTROLPLANE_MASTER_PLAN.md`](CONTROLPLANE_MASTER_PLAN.md) (the deferral that opens the hole is its §6 — "the socket is a future increment ... not any auth/security surface for remote control" — and §7 honest gaps).
> This document is the implementation PLAN per the tier protocol; the concrete edit sites are the sibling
> [`FG_CONTROLPLANE_SECURITY_PLAN_IMPLEMENTATION_STRATEGIES.md`](FG_CONTROLPLANE_SECURITY_PLAN_IMPLEMENTATION_STRATEGIES.md).

---

## §1 — Why this exists (the wall)

The control-plane (`framework/controlplane`, `designed`) is **safe today only by accident of immaturity**: its
MVP transport is stdout NDJSON, in-process, no network listener, and the control-ingress is a stub command-ring
with no transport (`CONTROLPLANE_MASTER_PLAN.md:48`, §5 CP3). `CONTROLPLANE_MASTER_PLAN.md:99` **explicitly
defers** "any auth/security surface for remote control (a future, security-reviewed increment)" — while §1/§5
commit to a **local WebSocket / named-pipe transport and a mobile companion**. The hole opens precisely when
that transport ships. F3 is the named requirement to design that posture **before** the code exists, not after.

The sensitive fields are **already in the FG's working set** (first-hand, §8): the FG resolves the captured
window by **title substring** (`find_window_by_substr`, `main.cpp:1888`; call site `main.cpp:3094`), reads
window titles via **`GetWindowTextA`** (`main.cpp:1884`), resolves the target **PID** via
`GetWindowThreadProcessId` (`main.cpp:6958`), and feeds the window title straight into the telemetry `appname`
(`main.cpp:6959`). The telemetry schema (`FG_TELEMETRY_PRIOR_ART.md §3`) leads group A with `Application`
(window title / process) + `ProcessID`, and group C with per-device GPU telemetry suffixed `_4090` / `_1080Ti`
/ `_iGPU` plus NVML host signals. **The frame IS the egress payload.** The moment a network transport drains
that frame, every one of those fields leaves the box unless classified out.

**The asymmetry that governs the whole design** (vista §1-F3(d); dossier §3-F3b(d)): egress is a
**confidentiality** risk (what leaves the box); control ingress is an **integrity / RCE** risk (the
OBS-WebSocket → RCE chain, dossier S3 `[V1]`, proves a file-write/spawn command over a control socket is an RCE
primitive). The two halves get categorically different treatment — F3a is field classification; F3b is
authentication + a non-constructible-insecure-config invariant.

## §2 — The objective: target, floor, closeable criterion (NAMED, not re-derived)

From the vista cell F3 (read first-hand) and the dossier §3 — restated here as the contract this plan delivers
against, **not** re-litigated:

- **(a) Target.** Telemetry egress + any future remote/mobile control are authenticated, scoped, and leak no
  host fingerprint (GPU LUIDs, window titles, profile names, PID); security-reviewed *before* it exists.
- **(b) FLOOR — NAMED, never planned-to-beat.** F3 is **the only objective family with NO structural floor**
  (vista §1-F3(b), dossier §3-F3a(b)): *"no structural floor — fully the product's to control."* The single
  **soft** tension this plan acknowledges and does NOT try to dissolve: **a remote monitor is useful precisely
  because it shows host state** — so "leak zero host data" and "be a useful remote monitor" are in genuine
  tension; the resolution is the explicit `--allow-host-data` opt-in, NOT a cleverer default that escapes the
  tension. This plan does **not** propose to make host-data egress simultaneously zero-by-default AND
  fully-informative — that is the soft floor, named here so no later pass re-proposes dissolving it as a win.
- **(f) ★ The closeable criterion (the target this plan IS built to cross).** The committed, CI-runnable
  **`controlplane_security_audit`** with three green checks:
  - **(A) egress grep = 0** — a packet capture of the network transport with `--allow-host-data` OFF over a
    full session contains NO window-title string, NO raw PID, NO GPU UUID/LUID, NO game-profile name (grep the
    captured stream for the known title/PID/UUID values → zero hits).
  - **(B) unauth client → zero state change** — an unauthenticated client (a raw socket skipping the handshake
    AND a cross-origin browser with a forged JS `Origin`) causes ZERO observable FG state change; every mutating
    command is rejected pre-auth, evidenced by a harness that asserts FG state is byte-identical before/after.
  - **(C) no insecure config is constructible** — there is NO build/config in which a network-reachable control
    transport is active with authentication disabled (a build/type-level invariant). Auth-off is permitted only
    for the loopback + Origin-locked local case, if at all.
  - All three green = **F3 closed for that increment**. This is a FIXED-POINT (binary), not an asymptote.

## §3 — Scope, non-scope, and the increment boundary

This plan is **transport-and-increment-staged** because the risk grows monotonically with the surface. The
ceremony matches the blast radius increment by increment (PLAN_TIER_PROTOCOL §4 anti-over-process):

| Increment | What ships | Tier of THAT increment | Risks engaged |
|---|---|---|---|
| **SP0 — field classification** | a per-field sensitivity tag on the telemetry frame; the network serializer emits only `public` unless `--allow-host-data` | **Tier 0/1** (no new attack surface — it *removes* egress; still default-off byte-identical) | F3a only (confidentiality) |
| **SP1 — egress audit + ring split** | the `controlplane_security_audit` check (A); the ring topology split so a telemetry transport is constructible with NO command-ring attached (read-only by construction) | **Tier 1** (an architectural invariant, no RCE surface yet) | F3a; the SP-SR "ingress-absent-by-default" invariant |
| **SP2 — local control transport** | the WebSocket/named-pipe control channel: loopback-only bind + Origin allowlist + auth-before-every-mutating-command + a closed command vocabulary | **Tier 2** (RCE-class + concurrency) | F3b (integrity/RCE); audit checks (B)+(C) |
| **SP3 — mobile pairing** | the mobile companion over WSS/TLS with a VETTED pairing handshake (never hand-rolled PIN+salt) + explicit non-loopback opt-in | **Tier 2** (RCE + crypto-footgun) | F3b; the pairing-crypto risk |

**Non-scope.**
- This plan does **not** build the control-plane pillar itself (that is `CONTROLPLANE_MASTER_PLAN.md` CP0–CP3);
  it adds the **security layer** to that pillar's network increments. SP0/SP1 attach to CP1/CP2; SP2 to the
  future WebSocket transport; SP3 to the future mobile companion.
- This plan does **not** weaken the no-game-cap operator invariant (no command verb caps or throttles the
  captured game; the command vocabulary is FG-tuning verbs only — §5).
- This plan claims **no measured behaviour**. Every acceptance test here is `designed`; it becomes `measured`
  only when the increment's gate is actually run first-hand.

## §4 — Architecture: the two halves

### §4.1 — F3a egress (confidentiality): classify, then filter at the wire

The frame schema gains a **per-field sensitivity class** — `public` / `host-identifying` / `secret` — evaluated
**at serialization (the cold drain), not at the producer push** (the producer hot path stays untouched; D-2,
`CONTROLPLANE_MASTER_PLAN.md:39-42`). The network serializer emits a field only if its class is `public`, OR if
the field is `host-identifying` AND the explicit `--allow-host-data` flag is set. `secret` fields are never
emitted over the network regardless of the flag. The known fields and their default class:

| Field | Source (first-hand) | Default class | Public substitute when host-data OFF |
|---|---|---|---|
| `Application` / window title | `GetWindowTextA` `main.cpp:1884` → `appname` `main.cpp:6959` | `host-identifying` | a stable salted hash, or an `active`/`idle` bool |
| `ProcessID` | `GetWindowThreadProcessId` `main.cpp:6958` | `host-identifying` | omitted |
| GPU model identity (`_4090`/`_1080Ti`/`_iGPU` suffixes) | telemetry schema, `FG_TELEMETRY_PRIOR_ART.md §3` group C | `host-identifying` | a generic ordinal (`gpu0`/`gpu1`) |
| GPU LUID / UUID (if ever added) | `D3D::luid` `main.cpp:1838` | `secret` | never emitted |
| NVML host signals (power/temp/clock/mem) | NVML, telemetry schema group C | `public` (aggregate rates) — BUT flagged: in aggregate over time they are a rig fingerprint (dossier §3-F3a(b)) | emitted; the fingerprint residual is `accepted` (RR SR3) |
| per-game profile name / library (future) | future | `host-identifying` | omitted |

**Default OFF, byte-identical.** `--allow-host-data` defaults OFF; with the network transport itself default-off
(it does not exist in the MVP), the FG's shipping behaviour is byte-identical to today. The classification is a
*serializer-side filter*; the producer frame is unchanged.

### §4.2 — F3b ingress (integrity/RCE): non-constructible-insecure-by-design

Four layered controls, each a dossier lever (§3-F3b(e)):

1. **Ring split (SP1, the structural invariant).** Egress and ingress are **independent rings**. The telemetry
   transport MUST be constructible with NO command-ring attached → the default build is **read-only by
   construction** (ingress absent, not merely disabled). This is the `controlplane_security_audit` check (C)'s
   foundation: you cannot configure a network-reachable command channel that is also auth-disabled, because the
   command channel's *type* requires an authenticator to construct.
2. **Loopback bind + Origin allowlist (SP2).** The control socket binds `127.0.0.1` by default; non-loopback is a
   separate explicit opt-in (Afterburner model, dossier S8). **Crucially** (dossier S2/S4 `[V1]`): localhost
   binding does NOT stop the user's own browser — the Same-Origin Policy does not apply to WebSocket handshakes
   and the browser does not enforce SOP on loopback (CSWSH). Therefore an **`Origin`-header allowlist is
   MANDATORY** on every handshake (the browser sends a truthful, JS-unforgeable `Origin`). This is not optional
   hardening; without it loopback binding is defeated by any page the user visits.
3. **Auth-before-every-mutating-command, per-message (SP2).** Per OWASP (dossier S5c `[V1]`): "don't assume a
   WebSocket connection equals unlimited access — check authorization for each action." Auth is checked
   per-command, not auth-at-connect. The insecure failure mode OBS proved (dossier S3) is the user *disabling*
   auth ("one click away") → SP2's invariant (control C above) makes auth-disabled-on-a-network-bind
   **non-constructible**, not merely defaulted-on.
4. **Closed command vocabulary, no RCE primitive (SP2).** The command channel exposes a **closed enum of
   FG-tuning verbs only** — never a file-write / arbitrary-path / process-spawn primitive (the OBS RCE chain
   depended on `SaveSourceScreenshot`'s arbitrary write, dossier S3). No verb caps/throttles the captured game
   (no-game-cap invariant). The vocabulary is enumerated in the strategies doc.

### §4.3 — SP3 mobile pairing (the Tier-2 crypto-footgun)

When the mobile companion ships: WSS/TLS (never plain `ws://`, dossier S5a); a **VETTED pairing handshake** —
**do NOT hand-roll PIN+salt crypto** (Moonlight's CVE-2020-11024 came from exactly that: concatenating the PIN
to the salt too early leaked the PIN, dossier S7 `[V1]`). Floor = Afterburner PSK-before-control (S8); target =
Moonlight PIN-pairing + cert-pinning (S6) over a vetted library, not a bespoke construction. Network exposure
beyond loopback is a separate explicit opt-in. SP3's risks are detailed in the RISK_REGISTER (SR5/SR6).

## §5 — The no-game-cap invariant + the dogmas this plan upholds

- **No-game-cap (operator invariant, PROVENANCE not a numbered dogma).** No command verb caps, throttles, or
  otherwise degrades the captured game. The command vocabulary is a closed enum of **FG-tuning** verbs
  (flow-scale, governor target, present mode…) — never anything that reaches into the game. Named here so SP2's
  vocabulary design honors it.
- **Byte-identical-off (D-2 class / DOGMA_SPECIFICATIONS).** Every new flag (`--allow-host-data`, the future
  control-transport enable flag) defaults OFF and the FG runs byte-identical with it off. SP0/SP1 are
  egress-removing (strictly safe); the network transport does not exist until opt-in.
- **Lock-free hot path (D-2/D-3/D-8).** Field classification + auth + serialization all live on the **cold
  drain** (`CONTROLPLANE_MASTER_PLAN.md:39-42`). The producer push stays `ring.push(frame)`, zero-alloc,
  lock-free. The auth/Origin checks never touch the FG tick. (This is also why F3b is a **concurrency** trigger:
  the command-ring is a new cross-thread surface — handled in the RISK_REGISTER SR2.)
- **Efficiency mandate.** The security layer is cold-path by construction; it adds zero hot-path cost. The one
  measurable claim deferred to acceptance: the egress-on-vs-off byte-identical gate (CP1's existing acceptance,
  extended with `--allow-host-data` off).

## §6 — Phases & gates (the build sequence)

| Phase | Deliverable | Acceptance gate (the binary "done") | Tier |
|---|---|---|---|
| **SP0** | per-field sensitivity class on the frame; serializer emits `public` only unless `--allow-host-data` | every schema field carries a class (progress metric: untagged-field count → 0); host-data-off run drops the host fields | T0/T1 |
| **SP1** | ring-split (telemetry constructible with no command-ring); `controlplane_security_audit` check (A) | audit (A) green: packet capture, host-data OFF, grep for known title/PID/UUID = 0; with `--allow-host-data` ON they appear | T1 |
| **SP2** | local control transport: loopback bind + Origin allowlist + per-command auth + closed verb enum; audit checks (B)+(C) | audit (B) green: unauth raw-socket + forged-Origin browser → zero state change; (C) green: no `network-bind + auth-disabled` config constructible (compile/type-level) | **T2 — RISK_REGISTER gate** |
| **SP3** | mobile pairing over WSS/TLS, vetted handshake, non-loopback opt-in | pairing required before any control; no hand-rolled PIN crypto (a vetted library); MITM-resistance reviewed | **T2 — RISK_REGISTER gate** |

**The hard gate (PLAN_TIER_PROTOCOL §3):** SP2 and SP3 MUST NOT be committed while any RISK_REGISTER row is
`open` — each MUST be `mitigated` (code in + verified first-hand) or explicitly `accepted` (bounded residual,
operator-recorded). SP0/SP1 are Tier-1 and gate on the audit (A) being green, not the register.

## §7 — Honest gaps / what this plan does NOT claim

- **No measured runtime behaviour.** The control-plane pillar is `designed`; the ingress is a stub; no security
  code exists. Every acceptance gate is `designed` until run.
- **The dossier's external sources were not re-fetched first-hand this session** (dossier §6): OBS-WebSocket's
  exact default bind interface, whether it validates `Origin` when auth is on, Moonlight's six-step pairing
  crypto detail, and Afterburner's wire crypto are `[V2]/[V3]` in the dossier. The security *lessons* (auth
  default-on, Origin allowlist mandatory, don't hand-roll pairing) are `[V1]`-grounded and load-bearing; the
  specific mechanism decimals are not re-confirmed here.
- **The NVML-aggregate fingerprint residual** (§4.1, SR3) is `accepted`, not eliminated: per-GPU power/temp/clock
  over time is a rig fingerprint even when `public`. The honest posture is to document it, not to claim it leaks
  nothing.
- **The soft floor is named, not beaten** (§2(b)): a useful remote monitor shows host state; the
  `--allow-host-data` opt-in resolves the tension by user choice, not by a default that is both private and
  fully-informative.

## §8 — First-hand verification table (FDP verifiable-reference mandate)

Every code anchor below was re-read first-hand this session (2026-06-23). The dossier cited `main.cpp:1839` /
`:6887` — those line numbers had **drifted**; the verified-this-session sites are:

| Claim | Verified site (this session) | Note |
|---|---|---|
| FG reads window title | `GetWindowTextA(h,title,…)` `apps/render_assistant/src/main.cpp:1884` | inside `enum_wnd_cb` |
| FG resolves window by title substring | `find_window_by_substr` `main.cpp:1888`; call site `main.cpp:3094`; flag store `main.cpp:1205` (`--window`) | `window_substr` declared `main.cpp:126` |
| FG resolves target PID | `GetWindowThreadProcessId(wgc_target_hwnd,&tgt_pid)` `main.cpp:6958` | |
| window title → telemetry `appname` | `appname = cfg.window_substr[0] ? … : "monitor"` `main.cpp:6959` → `tcsv.start(…,appname,(unsigned)tgt_pid,…)` `main.cpp:6960` | the egress payload |
| GPU LUID present in the working set | `D3D::luid` `main.cpp:1838` | `secret`-class candidate if ever serialized |
| telemetry schema (group A `Application`/`ProcessID`; group C per-GPU + NVML) | `FG_TELEMETRY_PRIOR_ART.md §1`, §3 | the per-field source |
| control-plane defers the auth surface | `CONTROLPLANE_MASTER_PLAN.md:99` | "not any auth/security surface for remote control" |
| WebSocket/mobile transport committed | `CONTROLPLANE_MASTER_PLAN.md:44-45`, §5 (future) | the hole-opening transport |
| producer hot path is lock-free cold-drain serialization | `CONTROLPLANE_MASTER_PLAN.md:39-42` | where the security filter lives (cold side) |

External SOTA references and their verification levels: see
[`FG_CONTROLPLANE_SECURITY_PRIOR_ART.md`](FG_CONTROLPLANE_SECURITY_PRIOR_ART.md) §2/§4/§6 (the single source of
truth for the OBS/CSWSH/OWASP/Moonlight/Afterburner facts — not duplicated here per FDP §4.6).

## §9 — Cross-links

- SOTA / prior art (the evidence): [`FG_CONTROLPLANE_SECURITY_PRIOR_ART.md`](FG_CONTROLPLANE_SECURITY_PRIOR_ART.md).
- The objective cell: [`PHYRIADFG_OBJECTIVE_VISTA.md`](PHYRIADFG_OBJECTIVE_VISTA.md) §1-F3.
- The pillar secured: [`CONTROLPLANE_MASTER_PLAN.md`](CONTROLPLANE_MASTER_PLAN.md).
- The telemetry schema: [`FG_TELEMETRY_PRIOR_ART.md`](FG_TELEMETRY_PRIOR_ART.md) §3.
- Sibling plan docs: [`FG_CONTROLPLANE_SECURITY_PLAN_IMPLEMENTATION_STRATEGIES.md`](FG_CONTROLPLANE_SECURITY_PLAN_IMPLEMENTATION_STRATEGIES.md)
  · [`FG_CONTROLPLANE_SECURITY_PLAN_RISK_REGISTER.md`](FG_CONTROLPLANE_SECURITY_PLAN_RISK_REGISTER.md).
- The tier protocol: [`PLAN_TIER_PROTOCOL.md`](../canon/PLAN_TIER_PROTOCOL.md).
