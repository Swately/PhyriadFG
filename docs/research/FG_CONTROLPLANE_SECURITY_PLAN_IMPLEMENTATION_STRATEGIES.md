# FG_CONTROLPLANE_SECURITY_PLAN_IMPLEMENTATION_STRATEGIES — concrete edit sites for objective F3

> **Diátaxis type:** Planning / design (Explanation — the per-strategy edit sites + the byte-identical-off
> discipline + the validation). Sibling of
> [`FG_CONTROLPLANE_SECURITY_PLAN_MASTER_PLAN.md`](FG_CONTROLPLANE_SECURITY_PLAN_MASTER_PLAN.md) (the why/architecture)
> and [`FG_CONTROLPLANE_SECURITY_PLAN_RISK_REGISTER.md`](FG_CONTROLPLANE_SECURITY_PLAN_RISK_REGISTER.md) (the Tier-2
> risk catalog). **Status:** `designed` — no code exists; the control-plane pillar is `designed` and its concrete
> source files are not yet written, so the strategy sites below are stated against the **pillar the
> CONTROLPLANE_MASTER_PLAN defines** (`framework/controlplane`, not yet on disk) and the **FG sites that already
> exist** (verified first-hand, master-plan §8). Where a site does not yet exist it is marked `(to-be-created by
> CONTROLPLANE_MASTER_PLAN CP0/CP1)`. **Normativity (BCP 14).**
>
> **Discipline.** Each strategy cites the RISK_REGISTER **ID** it mitigates (PLAN_TIER_PROTOCOL §2.3: edit → risk
> traceable). Each new flag is default-OFF and byte-identical-off. No `file:line` is invented — existing sites are
> the verified ones from the master-plan §8; non-existent sites are labelled.

---

## §0 — The dependency on CONTROLPLANE_MASTER_PLAN

This security layer attaches to the control-plane pillar's phases (`CONTROLPLANE_MASTER_PLAN.md:79-92`):

| Security phase | Attaches to | What the host pillar must already provide |
|---|---|---|
| **SP0** field classification | CP0 (frame schema + ring) / CP1 (NDJSON transport) | the POD telemetry frame + the cold drain serializer |
| **SP1** ring-split + audit (A) | CP1 / CP2 (FG adopts the boundary) | the transport interface + the FG pushing real fields |
| **SP2** local control transport security | the future WebSocket transport (CP "future") | a socket transport + the command-ring (CP3 stub today) |
| **SP3** mobile pairing | the future mobile companion | the WSS transport |

SP0/SP1 are buildable as soon as CP0–CP2 land. SP2/SP3 are **gated on the WebSocket transport existing** — until
then the closeable criterion's checks (B)/(C) test a non-existent surface and are vacuously green; the *value* of
SP2/SP3 is that the design is fixed **before** that transport ships (the F3 mandate: "security-reviewed before it
exists").

---

## §1 — SP0: per-field sensitivity classification (mitigates SR1)

**Goal.** Every telemetry field carries a sensitivity class; the network serializer emits `host-identifying`
fields only under `--allow-host-data`, and never emits `secret`.

**Edit sites.**

1. **The frame schema** `framework/controlplane/include/phyriad/controlplane/TelemetryFrame.hpp`
   *(to-be-created by CONTROLPLANE CP0)* — annotate each POD field with a compile-time class. Design as a
   parallel `constexpr` descriptor array (the POD frame stays a plain POD for the lock-free ring — D-8; the class
   table is static metadata, NOT a per-field runtime tag that would bloat the ring slot):

   ```cpp
   enum class Sens : uint8_t { Public, HostId, Secret };
   struct FieldDesc { const char* name; Sens sens; };
   // one static table; index-aligned with the serialized field order
   inline constexpr FieldDesc kFields[] = {
       {"MsBetweenPresents", Sens::Public},  {"FrameType", Sens::Public},
       {"flow_ms", Sens::Public},            /* … the architecture-internal columns … */
       {"Application", Sens::HostId},        {"ProcessID", Sens::HostId},
       {"gpu_model", Sens::HostId},          {"gpu_luid", Sens::Secret},
       {"GPUPower", Sens::Public},           {"GPUTemperature", Sens::Public}, /* SR3 residual */
   };
   ```

   The field set is the telemetry schema's (`FG_TELEMETRY_PRIOR_ART.md §3`: group A `Application`/`ProcessID`,
   the architecture columns `flow_ms`/`warp_ms`/`pack_ms`/`h2d_ms`/`ring_occ`/`frz`/…, group C per-GPU + NVML).

2. **The cold-drain serializer** `framework/controlplane/src/*Drain*.cpp` *(to-be-created by CONTROLPLANE
   CP1)* — at the **stdout/network serialize step** (the cold path, `CONTROLPLANE_MASTER_PLAN.md:39-42`), gate
   each field: emit iff `sens == Public`, OR (`sens == HostId` AND `allow_host_data`). Never emit `Secret`. The
   **stdout** (local sidecar) transport MAY emit everything (the Tauri child's stdout is local, in-process) —
   the filter binds the **network** transport only; this is a transport-property, set when the transport is
   constructed.

3. **The substitution rule for host-data-OFF** (so the stream is still useful, resolving the soft floor by
   substitution not omission where cheap): `Application` → a stable salted hash or an `active`/`idle` bool;
   `gpu_model` → a generic ordinal `gpu0`/`gpu1`; `ProcessID` → omitted. The salt is per-run, in-memory, never
   serialized.

4. **The flag.** Add `--allow-host-data` (default **false**) to the FG config struct — the same `extra`-parse
   region as the existing FG flags (declared near `apps/render_assistant/src/main.cpp:115-126`, parsed in the
   `--window` neighborhood `main.cpp:1205`). It sets the transport-construction parameter for the **network**
   transport. Default-off ⇒ byte-identical (the network transport does not exist in the MVP; the flag is inert
   until the WebSocket lands).

**Progress metric (vista F3a-f).** count of schema fields lacking a `Sens` tag → target **0**.

**Validation.** A unit test over `kFields` asserting (a) no field is untagged, (b) `gpu_luid`/any UUID is
`Secret`, (c) `Application`/`ProcessID` are `HostId`. CI-runnable, no rig.

---

## §2 — SP1: ring-split + the egress audit check (A) (mitigates SR1, founds SR-INVARIANT for SR4)

**Goal.** A telemetry transport is constructible with **no command-ring attached** (read-only by construction);
the `controlplane_security_audit` check (A) proves the host-data-off egress is clean.

**Edit sites.**

1. **The ring topology** `framework/controlplane/include/phyriad/controlplane/*.hpp` *(CP0/CP3)* — the egress
   ring (telemetry-out) and the ingress ring (command-in) are **separate types**, constructed independently. The
   telemetry transport's constructor takes only the egress ring; it has no member, parameter, or path that
   references a command-ring. This is the structural foundation of audit check (C): a read-only transport is the
   default *type*, not a configured *mode*.

2. **The audit harness** `framework/controlplane/test/controlplane_security_audit.*` or
   `apps/render_assistant/test/…` *(to-be-created)* — check (A):
   - Run the FG with the network transport on + `--allow-host-data` **OFF**.
   - Capture the transport bytes (a loopback packet capture, or a transport-tap test double that records the
     serialized stream).
   - `grep` the captured stream for the known values: the actual window-title substring (from `--window`), the
     resolved PID (`GetWindowThreadProcessId`, `main.cpp:6958`), the GPU LUID (`D3D::luid`, `main.cpp:1838`), and
     any profile name. **Assert zero hits.**
   - Re-run with `--allow-host-data` **ON**; assert the title/PID **do** appear (proves the gate, not a
     coincidental absence).

**Validation.** Check (A) green = F3a-f closed for that increment. CI-runnable once CP1/CP2 populate real fields;
on a host with no real game, a synthetic `--window` target + a known PID stand in.

---

## §3 — SP2: local control transport security (mitigates SR2, SR4, SR-RCE)

**Goal.** When the WebSocket/named-pipe control channel ships: loopback bind + Origin allowlist + per-command
auth + a closed verb enum, such that no unauthenticated client can change FG state and no auth-off-on-network
config is constructible.

**Edit sites** *(all in the future `framework/controlplane` socket transport — to-be-created; the design is
fixed now per the F3 mandate)*:

1. **Loopback bind default + non-loopback opt-in** (dossier S8, mitigates the CSWSH reachability of SR4). The
   socket binds `127.0.0.1` by default; a non-loopback bind requires an explicit, separate opt-in flag
   (`--control-bind <addr>`, default unset = loopback). Mirrors the Afterburner "separate component you must
   run" model.

2. **`Origin`-allowlist on EVERY handshake** (dossier S4 `[V1]`, mitigates SR4 CSWSH). The WebSocket upgrade
   handler reads the `Origin` header and rejects the handshake unless `Origin` ∈ a fixed allowlist (the Tauri
   app's origin; configured, not wildcard). **This is mandatory, not optional** — loopback binding alone does
   not stop a browser-borne attacker (the browser does not enforce SOP on loopback handshakes). The `Origin`
   header is browser-sent and JS-unforgeable, so it is a sound gate against page-driven attacks (it is NOT a
   gate against a native raw-socket client — that is what auth, item 3, covers).

3. **Per-command authentication** (dossier S5c `[V1]`, mitigates SR2). The handler checks authorization on
   **each mutating command**, not at connect ("a WebSocket connection ≠ unlimited access"). Auth = a
   challenge/response over a pre-shared secret (the OBS-WebSocket SHA-256 challenge model, dossier S1; the floor
   is Afterburner PSK, S8). The secret is generated on first run, displayed to the user, never defaulted to a
   known value. **The non-constructible invariant (check C):** the command transport's constructor REQUIRES an
   authenticator argument — there is no overload that builds a network-reachable command transport without one.
   Auth-disabled is permitted **only** for the loopback + Origin-locked local case via a distinct, clearly-named
   type, never as a flag on the network type.

4. **Closed command vocabulary, no RCE primitive** (dossier S3/L4, mitigates SR-RCE). The command channel
   accepts only a `enum class Cmd` of FG-tuning verbs. Proposed closed set (FG-tuning only, NONE reaching the
   game — no-game-cap):

   | Verb | Effect | Why safe |
   |---|---|---|
   | `SetFlowScale(n)` | FG internal flow resolution | tuning, bounded enum-arg |
   | `SetTargetOutputFps(n)` | the governor target | tuning; does NOT cap the game (output side) |
   | `SetPresentMode(enum)` | present pacing mode | tuning |
   | `TogglePause` | pause/resume FG generation | FG-internal |
   | `QueryState` | read-only state echo | non-mutating |

   **Explicitly excluded** (the RCE-primitive class): any file path, any arbitrary string written to disk, any
   process-spawn/kill, any `exec`/shell, any "load config from path". A command carrying a path or an
   unconstrained string MUST be rejected at the type level (the verb args are scalars/enums, never paths).

**Validation — the audit checks (B) + (C):**
- **(B)** the unauth harness: for each `Cmd`, send it (i) over a raw socket skipping the auth handshake, and
  (ii) from a simulated cross-origin browser with a forged JS `Origin`; assert FG state is byte-identical
  before/after (zero observable state change). Snapshot the FG config struct before/after and `memcmp`.
- **(C)** a compile-time / static assertion (or a constructed-type audit) that no `network-bind + auth-disabled`
  command transport is constructible — i.e. the type system rejects it. Evidenced by the absence of such a
  constructor + a `static_assert`-style guard or a negative compile test.

---

## §4 — SP3: mobile pairing (mitigates SR5, SR6 — Tier-2 crypto)

**Goal.** The mobile companion pairs over a VETTED handshake; no hand-rolled PIN crypto; WSS/TLS only.

**Edit sites** *(future mobile transport — to-be-created)*:

1. **WSS/TLS only** (dossier S5a) — the mobile transport refuses plain `ws://`; the constructor takes a TLS
   context, no plaintext fallback.

2. **Vetted pairing library, NOT hand-rolled** (dossier S7 `[V1]`, mitigates SR5). Pairing uses an
   established, reviewed handshake (a vetted PSK/PAKE library or the platform's pairing primitive) — **never** a
   bespoke PIN+salt concatenation (CVE-2020-11024 came from exactly that: PIN concatenated to salt too early
   leaked the PIN). Floor = Afterburner PSK-before-control (S8); target = Moonlight PIN-pairing + cert-pinning
   (S6).

3. **Non-loopback opt-in** (mitigates SR6) — exposing the companion beyond loopback is a separate explicit
   opt-in (`--control-bind`), with the user warned; cert-pinning so a MITM cannot impersonate the FG.

**Validation.** Pairing required before any control verb; a review/test that no command path is reachable
pre-pairing; the crypto is a named vetted dependency (not in-repo bespoke), recorded in the RISK_REGISTER SR5.

---

## §5 — Byte-identical-off ledger (the discipline, per flag)

| Flag | Default | Off-behaviour | Why byte-identical |
|---|---|---|---|
| `--allow-host-data` | `false` | network serializer drops `HostId` fields | filter is network-transport-only; the MVP has no network transport → no observable change to the shipping FG |
| `--control-bind <addr>` | unset (loopback) | no non-loopback listener | the listener does not exist in the MVP |
| (control transport enable, future) | off | no command socket | F3b satisfied vacuously until it ships (dossier §3-F3b(c)) |

The FG tick, present path, and producer push are untouched by every strategy here (all logic is cold-drain or
transport-construction side). Verify at SP1: the CP1 "egress on vs off byte-identical" acceptance, extended with
`--allow-host-data` off, must still hold (`CONTROLPLANE_MASTER_PLAN.md:88`).

---

## §6 — Risk traceability (edit → register ID)

| Strategy | RISK_REGISTER ID(s) |
|---|---|
| SP0 field classification (§1) | SR1 (host-data egress); SR3 (NVML aggregate residual, `accepted`) |
| SP1 ring-split + audit (A) (§2) | SR1; founds SR4's non-constructible invariant |
| SP2 loopback + Origin (§3 items 1–2) | SR4 (CSWSH) |
| SP2 per-command auth + non-constructible config (§3 item 3) | SR2 (unauth ingress); the audit check (C) |
| SP2 closed vocabulary (§3 item 4) | SR-RCE (command-primitive RCE) |
| SP3 vetted pairing (§4) | SR5 (hand-rolled crypto); SR6 (MITM / non-loopback exposure) |
| the new command-ring cross-thread surface | SR-CONC (concurrency, §RISK_REGISTER) |

## §7 — Cross-links

Master-plan (why/architecture/floors): [`FG_CONTROLPLANE_SECURITY_PLAN_MASTER_PLAN.md`](FG_CONTROLPLANE_SECURITY_PLAN_MASTER_PLAN.md).
Risk register (the Tier-2 catalog): [`FG_CONTROLPLANE_SECURITY_PLAN_RISK_REGISTER.md`](FG_CONTROLPLANE_SECURITY_PLAN_RISK_REGISTER.md).
SOTA evidence: [`FG_CONTROLPLANE_SECURITY_PRIOR_ART.md`](FG_CONTROLPLANE_SECURITY_PRIOR_ART.md).
The pillar: [`CONTROLPLANE_MASTER_PLAN.md`](CONTROLPLANE_MASTER_PLAN.md). The schema:
[`FG_TELEMETRY_PRIOR_ART.md`](FG_TELEMETRY_PRIOR_ART.md) §3.
