# FG_CONTROLPLANE_SECURITY_PLAN_RISK_REGISTER — the Tier-2 risk catalog for objective F3

> **Diátaxis type:** Planning (risk register). **Normativity:** BCP 14.
> **Status (updated 2026-06-24):** the SP0/SP1 **egress field-classification** layer is **built + verified
> first-hand** (`framework/controlplane`, header-only, zero main.cpp edits) → **SR1 is `mitigated`**. The SP2/SP3
> rows secure a transport that **does not exist** (the control-ingress is a stub command-ring, no network
> transport, no WebSocket, no mobile companion) and are therefore **`accepted` as a named floor against the
> absent transport** — their mitigation-as-code is *designed* and fixed in the strategies doc, deferred to the
> increment that builds the transport (you cannot authenticate absent code; fabricating it would violate
> AGENT_PROTOCOL §4). **No row is `open`.** See the implementation note immediately below SR1 and the status
> summary for the per-row standing + the coverage/speculative framing. This register was written **before** the
> implementation, per the F3 mandate ("security-reviewed before it exists") and PLAN_TIER_PROTOCOL §3 item 1.
> Each row carries its **mitigation as code** (a concrete guard/type/check, not prose) and the **first-hand
> verification gate** that flips it to `mitigated`.
> **Tier:** **T2** per [`PLAN_TIER_PROTOCOL.md`](../canon/PLAN_TIER_PROTOCOL.md) §1.1 — triggers **3 (security:
> an untrusted-input/injection surface, an RCE primitive)** and **2 (concurrency: a new cross-thread command
> ring)**. Sibling of [`FG_CONTROLPLANE_SECURITY_PLAN_MASTER_PLAN.md`](FG_CONTROLPLANE_SECURITY_PLAN_MASTER_PLAN.md)
> + [`FG_CONTROLPLANE_SECURITY_PLAN_IMPLEMENTATION_STRATEGIES.md`](FG_CONTROLPLANE_SECURITY_PLAN_IMPLEMENTATION_STRATEGIES.md).
>
> **Hard gate (PLAN_TIER_PROTOCOL §3 item 3, Pre-commit GATE):** the SP2 / SP3 increments MUST NOT be committed while any row is
> `open`. Each MUST become `mitigated` (code in + first-hand verification recorded) or `accepted` (bounded
> residual, operator-recorded). SP0/SP1 (Tier-1) gate on the audit (A) being green, not on this register.
> **In-repo `file:line` refs are first-hand-verified this session (2026-06-23); see the master-plan §8 table.**

## Why T2

The control-ingress increment turns a **read-only, in-process, no-network** posture into a **network-reachable
command channel that can mutate FG state**. The dossier's S3 `[V1]` is the governing precedent: a control socket
exposing a file-write/spawn command is an **RCE primitive** (OBS-WebSocket → `.hta` to Startup → RCE on reboot).
That is the PLAN_TIER_PROTOCOL §1.1 security trigger. The command ring is additionally a **new cross-thread
surface** (the transport thread mutates state the FG threads read) — the concurrency trigger. The two together
fix the tier at T2.

The risks cluster: **(1) confidentiality** (SR1, SR3 — what egress leaks), **(2) integrity/RCE** (SR2, SR-RCE,
SR4 — what an attacker can do to the command channel), **(3) crypto-footgun** (SR5, SR6 — the mobile pairing),
**(4) concurrency** (SR-CONC — the command ring as a race surface).

---

## ⚠ Implementation note (2026-06-24) — what was actually built, and the honest gap

SP0/SP1 were implemented this session as an **egress field-classification layer** in
`framework/controlplane` (header-only, **zero main.cpp edits**):
`framework/controlplane/include/phyriad/controlplane/EgressClass.hpp` (the `Sens` enum, the
`constexpr kEgressFields[]` table, `classify()`, the `EgressClassPolicy`, and `field_visible()` — the
SR1 mitigation **as code**) and `…/ClassifiedNdjson.hpp` (the policy-gated network serializer
`serialize_frame_classified()` + the audit-check-(A) helper `egress_audit_clean()`); validated by
`framework/controlplane/tests/controlplane_egress_class_test.cpp` (31/31 green) with the existing
`controlplane_roundtrip_test` still 15/15 (no regression). **First-hand build (2026-06-24):** the
standalone pillar build (`cmake -S framework/controlplane -B … -DPHYRIAD_BUILD_TESTS=ON`, MinGW g++
15.2.0) compiled clean — the `static_assert`s in EgressClass.hpp all hold.

**The honest gap (verified first-hand against `TelemetryFrame.hpp`, 2026-06-23/24).** The **real**
`TelemetryFrame` that ships today carries **NO host-identifying field**: only the header
(magic/version/kind/qpc/seq), the FG architecture-internal metrics (warp_ms/iter_ms/slip_ms/… — none
identifying), and the per-GPU NVML telemetry (util/power/temp/clock/mem/vram). The window TITLE / PID /
LUID the F3 plan names live in the FG's **`main.cpp` working set, not in the frame**
(`GetWindowTextA` main.cpp:1884, `GetWindowThreadProcessId` main.cpp:6958, `D3D::luid` main.cpp:1838).
Therefore, run over the present frame, the classifier **withholds nothing** — the network serialization
is byte-identical to the unclassified one (the SP1 test proves this, present-frame line = 763 bytes,
`classified==unclassified` under both the `local()` and the `network()` policy). The classifier's value
is **coverage**: it names `Application`/`ProcessID`/`gpu_model`/`profile_name` (`HostId`) and
`gpu_luid`/`gpu_uuid` (`Secret`) **now**, and `classify()` is **fail-closed** (an unknown field is
`Secret`), so the day a future increment adds any of those columns to the frame, the egress already
refuses them by default — the policy precedes the field. This is the operator-directed §4-flexibility
build for **completeness**; whether it earns its place is **tbd by the end-of-batch tests** (operator's
call). See `EgressClass.hpp`'s header note for the full first-hand standing.

---

## SR1 — Host-identifying data egress without opt-in · class `security` (confidentiality) · severity `high` · **BLOCKS SP1 COMMIT**

**Failure mode.** The telemetry frame IS the egress payload (`CONTROLPLANE_MASTER_PLAN.md:39`). The FG already
carries the window TITLE (`GetWindowTextA`, `apps/render_assistant/src/main.cpp:1884`), the PID
(`GetWindowThreadProcessId`, `main.cpp:6958`), and the GPU LUID (`D3D::luid`, `main.cpp:1838`); the title flows
into the telemetry `appname` (`main.cpp:6959-6960`). The moment a network transport drains that frame, every one
of those fields leaves the box — titles can contain document names, usernames, server names; the LUID is a stable
machine ID. **Standing:** no field-sensitivity classification exists in either anchor (dossier §3-F3a(c)).

**Mitigation as code.** SP0 (strategies §1): a `constexpr FieldDesc kFields[]` table tags every field
`Public`/`HostId`/`Secret`; the **cold-drain network serializer** emits a field iff `Public`, or (`HostId` AND
`allow_host_data`); `Secret` (LUID/UUID) is never emitted. `--allow-host-data` defaults `false`. Host-data-off
substitutes: title → salted hash / `active`-bool, `gpu_model` → ordinal, PID → omitted.

```cpp
if (fd.sens == Sens::Secret) continue;                 // never on the wire
if (fd.sens == Sens::HostId && !cfg.allow_host_data) continue;  // opt-in only
emit(fd, frame);                                       // Public, or HostId+opt-in
```

**Verification (flips to `mitigated`).** The `controlplane_security_audit` check (A): a packet capture with
`--allow-host-data` OFF, grepped for the real title/PID/LUID values → **zero hits**; re-run ON → they appear.
Plus the SP0 unit test asserting no field is untagged and LUID is `Secret`. First-hand: run the audit, record the
grep result.

**Verification recorded (2026-06-24, first-hand).** The classification layer + the static guards + the
`egress_audit_clean()` helper are built and green:
- `controlplane_egress_class_test` 31/31 — asserts `gpu_luid`/`gpu_uuid` are `Secret`,
  `Application`/`ProcessID`/`gpu_model` are `HostId`, the architecture metrics are `Public`, the
  `field_visible` truth-table (Public always · HostId opt-in · Secret never), the classified-vs-unclassified
  byte-identity for the present frame, and audit check (A) (`egress_audit_clean` true; a negative control proves
  the grep is wired, not a no-op).
- The `static_assert`s in `EgressClass.hpp` enforce the same invariants at compile time (fail the build on a
  mis-classification) — the strongest form of "no field untagged".
- `controlplane_roundtrip_test` 15/15 (no regression to the existing serializer).

**Honest residual (why this is `mitigated`, not "closed").** The full check-(A) wire-grep over a real network
**packet capture** is **not yet runnable**: there is no network transport (the MVP sink is local stdout) and the
present frame has no host-identifying field to leak. The buildable, CI-runnable part — the classifier, the
fail-closed policy, the static guards, and the in-memory audit helper — is done and verified; the rig-side packet
capture becomes meaningful only once a network transport + a host-bearing frame exist. The mitigation **as code**
is in and exercised; the remaining check is the deferred rig step, recorded here honestly.

**Status:** `mitigated` (classification layer in + verified first-hand; the rig packet-capture grep is deferred
until a network transport and a host-identifying frame field exist — see the implementation note above).

---

## SR2 — Unauthenticated control ingress mutates FG state · class `security` (integrity) · severity `critical` · **BLOCKS SP2 COMMIT**

**Failure mode.** When the WebSocket control channel ships, a client that never authenticated sends a mutating
command (e.g. `SetTargetOutputFps`) and the FG applies it. OBS proved the dangerous failure mode is auth being
*disableable* (dossier S3, "one click away"). A connection is not authorization (OWASP S5c).

**Mitigation as code.** SP2 (strategies §3 item 3): **per-command** auth (challenge/response over a first-run-generated
secret, the OBS SHA-256 model S1 / Afterburner PSK floor S8), checked on **each mutating verb**, never
auth-at-connect:

```cpp
Result handle(const Cmd& c, Session& s) {
    if (is_mutating(c) && !s.authenticated)            // per-message, per OWASP S5c
        return Result::Rejected_Unauthenticated;       // zero state change
    return apply(c);
}
```

The transport's command-handler constructor REQUIRES an authenticator argument (no auth-less overload for the
network type) — see SR4 for the non-constructible invariant.

**Verification.** Audit check (B): the unauth harness sends each `Cmd` over a raw socket skipping the handshake;
snapshot the FG config struct before/after and `memcmp` → byte-identical (zero observable state change).
First-hand: run the harness, record the memcmp result.

**Accepted-floor rationale (2026-06-24).** There is **no transport to authenticate**. Verified first-hand: the
control-ingress is a STUB (`Ingress.hpp` / `ControlCommand.hpp`) — a lock-free command ring whose vocabulary is
`Noop`/`Start`/`Stop`, fed `push_command()` **in-process** only, with **no network listener, no WebSocket, no
handshake** (the frontend steers the FG by spawning it with flags). You cannot authenticate code that does not
exist; fabricating an authenticator here would violate AGENT_PROTOCOL §4 (no fabricated absent transport). The
mitigation **as code** (per-command auth) is **designed** in the strategies doc and **fixed before the transport
ships** — that is the F3 mandate's value ("security-reviewed before it exists"). Accepted as a **named floor**:
the design is recorded, the code is deferred to the increment that builds the transport.

**Status:** `accepted` (named floor — no transport exists to secure; the per-command-auth design is fixed in the
strategies doc; **revisit → flip toward `mitigated` when the WebSocket/named-pipe ingress is built**).

---

## SR-RCE — A command verb is an RCE primitive (file-write / spawn / path) · class `security` (RCE) · severity `critical` · **BLOCKS SP2 COMMIT**

**Failure mode.** The dossier's S3 `[V1]` chain depended on a command that could **write an arbitrary file**
(`SaveSourceScreenshot`). If the FG's command vocabulary ever exposes a file path, an arbitrary string written to
disk, a process spawn/kill, an "exec", or a "load config from path", an attacker who reaches the channel (or
chains past auth via a future bug) gains code execution, not just FG mis-tuning.

**Mitigation as code.** SP2 (strategies §3 item 4): the command channel accepts only a **closed `enum class Cmd`** of
FG-tuning verbs with **scalar/enum args only** — `SetFlowScale(n)`, `SetTargetOutputFps(n)`,
`SetPresentMode(enum)`, `TogglePause`, `QueryState`. No verb carries a path or an unconstrained string; the
parser rejects any unknown verb. There is **no** file/spawn/exec verb in the type at all (the RCE primitive does
not exist to be reached).

```cpp
enum class Cmd : uint8_t { SetFlowScale, SetTargetOutputFps, SetPresentMode, TogglePause, QueryState };
// args are unions of bounded scalars/enums; NO std::string path, NO spawn. Unknown wire verb -> reject.
```

**Verification.** A static review + a test enumerating every `Cmd` and asserting none takes a path/string/exec
arg; a negative test that an unknown or path-bearing wire message is rejected. First-hand: read the `Cmd`
definition, confirm no file/spawn member; run the reject test.

**Accepted-floor rationale + a first-hand reassurance (2026-06-24).** No network command channel exists to carry
an RCE primitive. Verified first-hand: the present `ControlCommand` (`ControlCommand.hpp`) is a **fixed POD** —
`{magic, version, CommandKind kind, seq, uint8_t payload[32]}` with `CommandKind ∈ {Noop, Start, Stop}` — it has
**no path, no string, no spawn, no exec** member already; the payload is 32 fixed bytes read by a discriminator.
So the stub is, by construction, free of the RCE-primitive class today. The closed-vocabulary discipline for the
**future** verb set is fixed in the strategies doc. Accepted as a **named floor**: nothing to exploit now, the
discipline is recorded for the increment that adds real verbs.

**Status:** `accepted` (named floor — the present POD carries no RCE primitive; the closed-vocabulary rule is
fixed in the strategies doc; **revisit when real control verbs are added**).

---

## SR4 — Cross-Site WebSocket Hijacking (CSWSH) — the browser defeats loopback binding · class `security` · severity `high` · **BLOCKS SP2 COMMIT**

**Failure mode.** Binding `127.0.0.1` does NOT protect the channel: the Same-Origin Policy does not apply to
WebSocket handshakes and the browser does not enforce SOP on loopback (dossier S4 `[V1]`). Any page the user
visits can open `ws://localhost:<port>` and drive the channel — so loopback-only is necessary-but-insufficient.

**Mitigation as code.** SP2 (strategies §3 items 1–2): loopback bind by default (non-loopback = a separate explicit
opt-in, S8) **PLUS** a mandatory server-side `Origin`-header allowlist on **every** handshake (the browser sends
a truthful, JS-unforgeable `Origin`):

```cpp
bool on_handshake(const Request& r) {
    return kAllowedOrigins.contains(r.header("Origin"));  // reject browser-borne cross-origin; no wildcard
}
```

Combined with SR2 (per-command auth) this stops both the browser path (Origin) and the native raw-socket path
(auth).

**Verification.** Audit check (B), the cross-origin half: a simulated browser handshake with a forged JS `Origin`
is rejected at the handshake; FG state byte-identical. First-hand: run the forged-Origin harness, record the
rejection.

**Accepted-floor rationale (2026-06-24).** There is **no WebSocket handshake to hijack** — the ingress is the
in-process stub, with no socket bind and no `Origin`-bearing upgrade path. CSWSH is unreachable today. The
mandatory-`Origin`-allowlist design is fixed in the strategies doc. Accepted as a **named floor**.

**Status:** `accepted` (named floor — no WebSocket handshake exists; the Origin-allowlist design is fixed;
**revisit when the socket transport is built**).

---

## SR-INVARIANT (audit check C) — a `network-bind + auth-disabled` config is constructible · class `security` / `dogma` · severity `high` · **BLOCKS SP2 COMMIT**

**Failure mode.** Even with auth implemented, if a build/config can put a network-reachable command transport
into an auth-disabled state (a flag, a default, an overload), the OBS "one click away" failure recurs. The F3
closeable criterion (C) requires this be **impossible by construction**, not merely defaulted-safe.

**Mitigation as code.** SP1/SP2: the command transport's constructor for the **network** type REQUIRES the
authenticator + the Origin allowlist as non-optional arguments; auth-off exists only as a **distinct type** for
the loopback+Origin-locked local case (if at all). A `static_assert` / negative compile test guards that no
network-command-transport is constructible without an authenticator. The egress (read-only) transport is a
**separate type** that takes no command-ring (the SP1 ring-split) — so "telemetry without a command channel" is
the default *type*, not a configured mode.

**Verification.** Audit check (C): a negative compile test (constructing the network command transport without an
authenticator fails to compile) + a review confirming no auth-disable flag on the network type. First-hand:
attempt the construction, confirm it does not compile.

**Accepted-floor rationale + the part that IS structurally present (2026-06-24).** There is **no network command
transport type** to make non-constructible-insecure — so the negative compile test has nothing to guard yet.
However, the **egress / ingress ring-split** the invariant rests on is **already structural**, first-hand: the
telemetry `Egress` and the command `Ingress` are **separate types** (`Egress.hpp` vs `Ingress.hpp`), constructed
independently; `Egress` takes only an `ITransport*` and has no member, parameter, or path referencing a
command-ring. So "telemetry without a command channel" is the **default type**, not a configured mode — the SP1
ring-split foundation holds today. The SP0 `EgressClassPolicy` further makes the **egress** side carry its
host-data policy at construction (the network default withholds `HostId`). What is deferred is only the
**command-transport-side** authenticator-required constructor (no such type exists yet). Accepted as a **named
floor** for the command-transport half; the egress/ring-split half is structurally present and verified.

**Status:** `accepted` (named floor — the network command-transport type does not exist; the egress/ingress
ring-split it rests on IS structurally present and verified first-hand; **revisit the authenticator-required
constructor when the command transport is built**).

---

## SR-CONC — the command ring is a new cross-thread race surface · class `concurrency` · severity `medium` · **BLOCKS SP2 COMMIT**

**Failure mode.** The control transport thread writes commands that the FG threads read/apply; an unsynchronized
shared mutation is a data race (Phyriad's lock-free mandate makes any new cross-thread sharing presumptively
Tier-2, PLAN_TIER_PROTOCOL §1.1 trigger 2). A mutex on the FG hot path is forbidden (D-2).

**Mitigation as code.** The command ingress is a **lock-free SPSC ring** (the same `transport`/Ring pattern the
control-plane already mandates, `CONTROLPLANE_MASTER_PLAN.md:39-42`): the transport thread is the single
producer, the FG drains it at a tick boundary (single consumer). Applied state is a tick-boundary swap, not a
hot-path lock. No new mutex on the present/producer path.

**Verification.** A TSan run (the control-plane's CP0 mandates TSan tests, `CONTROLPLANE_MASTER_PLAN.md:85`)
over the command-ring producer/consumer → clean; confirm no mutex appears on the FG tick path (review +
`lint_hal` for raw memory orders in `apps/`). First-hand: run TSan, record clean.

**Accepted-floor rationale (2026-06-24).** SP0/SP1 as built add **no new cross-thread surface**: the
classification layer (`EgressClass.hpp`) is pure `constexpr` metadata + predicates, and the classified serializer
(`ClassifiedNdjson.hpp`) runs on the **cold drain thread only** (same as the existing `serialize_frame`) — no
atomics, no shared mutable state (`grep` for `memory_order`/`std::atomic` in both headers → zero). The command
ring this risk concerns is the **stub `Ingress`**, which is the existing CP3 SPSC ring (already TSan-tested by
`controlplane_concurrency_test`); no new command-thread was introduced this session. The new cross-thread *race
surface* the risk anticipates appears only when a real control transport thread is added. Accepted as a **named
floor** for that future thread; SP0/SP1 are concurrency-neutral (verified: no atomics added).

**Status:** `accepted` (named floor — SP0/SP1 add no new cross-thread surface [no atomics in either header]; the
race surface this concerns appears only with the future control-transport thread; **revisit + TSan when that
thread is built**).

---

## SR5 — Hand-rolled mobile-pairing crypto leaks the PIN · class `security` (crypto) · severity `high` · **BLOCKS SP3 COMMIT**

**Failure mode.** Moonlight's CVE-2020-11024 (dossier S7 `[V1]`): iOS/tvOS concatenated the PIN to the salt too
early, leaking the PIN → MITM fraudulent pairing. A bespoke PIN+salt construction is a recurring footgun.

**Mitigation as code.** SP3 (strategies §4.2): pairing uses a **vetted** PSK/PAKE library or platform pairing
primitive — **no** in-repo bespoke PIN+salt concatenation. The named dependency is recorded here, not
hand-written. Floor = Afterburner PSK (S8); target = Moonlight PIN-pairing + cert-pinning (S6).

**Verification.** A review confirming the pairing path calls only the vetted library (no in-repo crypto
construction); the dependency is named + version-pinned. First-hand: read the pairing code, confirm no bespoke
salt/PIN concatenation.

**Accepted-floor rationale (2026-06-24).** There is **no mobile companion and no pairing code** — there is
nothing to review for a hand-rolled-crypto footgun, and writing pairing crypto now (with no transport to use it)
would be the fabrication §4 forbids. The "use a vetted PSK/PAKE library, never bespoke PIN+salt" rule is fixed in
the strategies doc. Accepted as a **named floor**.

**Status:** `accepted` (named floor — no mobile pairing code exists; the vetted-library rule is fixed in the
strategies doc; **revisit when the mobile companion is built**).

---

## SR6 — Non-loopback exposure / MITM of the mobile companion · class `security` · severity `high` · **BLOCKS SP3 COMMIT**

**Failure mode.** The companion is useful over the LAN, but a non-loopback listener without TLS + cert-pinning is
MITM-able; plain `ws://` over the network exposes the secret/commands.

**Mitigation as code.** SP3 (strategies §4.1, §4.3): WSS/TLS only (the transport refuses plain `ws://`, no
plaintext fallback, S5a); cert-pinning so a MITM cannot impersonate the FG; non-loopback exposure is a separate
explicit opt-in (`--control-bind`) with the user warned.

**Verification.** A review/test that the mobile transport rejects plain `ws://`; cert-pinning present; the
non-loopback path is opt-in only. First-hand: confirm the plaintext path does not construct.

**Accepted-floor rationale (2026-06-24).** No non-loopback listener and no mobile transport exist — there is no
`ws://` path to reject and no MITM surface today (the MVP sink is a local in-process stdout). The WSS/TLS-only +
cert-pinning + non-loopback-opt-in design is fixed in the strategies doc. Accepted as a **named floor**.

**Status:** `accepted` (named floor — no non-loopback transport exists; the WSS/cert-pin design is fixed;
**revisit when the mobile/non-loopback transport is built**).

---

## SR3 — NVML-aggregate rig fingerprint (the named residual) · class `security` (confidentiality) · severity `low` · **`accepted`**

**Failure mode.** Even classified `Public`, per-GPU power/temp/clock/memory over time is a rig fingerprint + a
covert/side-channel surface (dossier §3-F3a(b)). Reducing it to zero would gut the legitimate telemetry use
(a remote monitor exists to show host state — the **soft floor**, master-plan §2(b)).

**Mitigation as code.** None that preserves utility — this is the soft floor NAMED, not beaten. The honest
posture: the NVML signals stay `Public` (telemetry must work); the residual is documented, and a future
coarsening option (rate-bucketing) MAY be offered but is NOT required to close F3.

**Verification.** N/A — `accepted` with the rationale above. Operator-recorded: the residual is the soft tension
the `--allow-host-data` opt-in does not remove for the always-on NVML fields.

**Status:** `accepted` (bounded residual; the soft floor, not a defect).

---

## Status summary

| ID | Class | Severity | Status | Gate |
|---|---|---|---|---|
| SR1 | confidentiality | high | `mitigated` | classification layer in + verified (audit (A) helper green; rig packet-capture deferred) |
| SR2 | integrity | critical | `accepted` (named floor) | no transport to authenticate — auth design fixed; revisit at ingress build |
| SR-RCE | RCE | critical | `accepted` (named floor) | present POD carries no RCE primitive; closed-vocabulary rule fixed |
| SR4 | CSWSH | high | `accepted` (named floor) | no WebSocket handshake exists; Origin-allowlist design fixed |
| SR-INVARIANT | security/dogma | high | `accepted` (named floor) | egress/ingress ring-split IS structural; command-transport ctor deferred |
| SR-CONC | concurrency | medium | `accepted` (named floor) | SP0/SP1 add no cross-thread surface (no atomics); future-thread risk deferred |
| SR5 | crypto | high | `accepted` (named floor) | no pairing code exists; vetted-library rule fixed |
| SR6 | MITM | high | `accepted` (named floor) | no non-loopback transport exists; WSS/cert-pin design fixed |
| SR3 | confidentiality | low | `accepted` | the named soft floor |

**Gate reminder.** Original gate: SP1 cannot commit while SR1 is `open`; SP2 cannot commit while SR2/SR-RCE/SR4/
SR-INVARIANT/SR-CONC are `open`; SP3 cannot commit while SR5/SR6 are `open`. **As of 2026-06-24 no row is `open`**
— SR1 is `mitigated` (the SP0/SP1 classification layer is in + verified first-hand), and every SP2/SP3 row is
`accepted` as a **named floor against the absent transport** (no transport exists to secure; each design is fixed
in the strategies doc; each is to be **revisited when the corresponding ingress/transport is built**, at which
point it should be re-opened and driven to `mitigated`). SR3 is the named soft-floor residual. This satisfies the
hard gate **for the SP0/SP1 increment as built**; the SP2/SP3 increments remain *designed-only* and their
`accepted` status is explicitly **provisional on the transport's absence** — building the transport without
re-opening these rows would be a protocol violation.

> **Coverage / speculative status (operator directive, §4 flexibility, 2026-06-24).** F3 was built for
> **completeness**, ahead of need: SP0/SP1 ship a classification layer for a future control-ingress that is today
> a stub, and SP2/SP3 secure a transport that does not exist. The operator's call: *"we lose nothing now vs
> later; the tests decide its value."* The value of the SP0/SP1 layer over the **present** frame is **inert** (the
> frame has no host-identifying field — the layer withholds nothing today); it becomes load-bearing only when a
> future increment adds `Application`/`ProcessID`/`gpu_luid` to the frame or a network transport drains it.
> Whether this coverage earns its place is **tbd by the end-of-batch tests**.

## Cross-links

Master-plan: [`FG_CONTROLPLANE_SECURITY_PLAN_MASTER_PLAN.md`](FG_CONTROLPLANE_SECURITY_PLAN_MASTER_PLAN.md).
Strategies (edit sites, risk traceability §6): [`FG_CONTROLPLANE_SECURITY_PLAN_IMPLEMENTATION_STRATEGIES.md`](FG_CONTROLPLANE_SECURITY_PLAN_IMPLEMENTATION_STRATEGIES.md).
SOTA evidence (S1–S9 levels): [`FG_CONTROLPLANE_SECURITY_PRIOR_ART.md`](FG_CONTROLPLANE_SECURITY_PRIOR_ART.md).
Tier protocol: [`PLAN_TIER_PROTOCOL.md`](../canon/PLAN_TIER_PROTOCOL.md).
