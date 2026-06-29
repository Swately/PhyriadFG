# FG_CONTROLPLANE_SECURITY_PRIOR_ART — telemetry/control-plane privacy & security

> **Diátaxis type:** Analysis (SOTA dossier). **Status:** `designed` (the control-plane is `designed`;
> control ingress is a stub — all F3 claims are about *design state*, not measured behaviour). Resolves
> objective **F3** — **F3a** (no host-identifying data egress without opt-in) + **F3b** (no unauthenticated
> control ingress). Companion to [`PHYRIADFG_OBJECTIVE_VISTA.md`](PHYRIADFG_OBJECTIVE_VISTA.md) §1-F3 and
> the implementation parent [`CONTROLPLANE_MASTER_PLAN.md`](CONTROLPLANE_MASTER_PLAN.md).
>
> **Provenance.** From workflow `w83r5ydl9` (the control-plane-security agent; it read the FG capture
> sites in `main.cpp` first-hand). Security prior-art carries the sweep's `[V1]/[V2]/[V3]` levels (OWASP /
> PortSwigger / vendor advisories are `[V1]`). The author did NOT independently re-fetch every URL this
> session (§6 flags it). Per FDP §2: no fabricated citation.
>
> **Normativity (BCP 14):** this dossier uses MUST / MUST NOT deliberately — F3 is a security boundary, and
> the requirements below are real. Per `PLAN_TIER_PROTOCOL`, the control-ingress increment (F3b/L3) is
> **Tier-2** (RCE-class + concurrency) and **MUST carry a `CONTROLPLANE_RISK_REGISTER.md`** with each risk
> `mitigated`-as-code before commit.

---

## §1 — PhyriadFG current standing (in-repo, first-hand)

- `CONTROLPLANE_MASTER_PLAN.md` **explicitly defers** "any auth/security surface for remote control (a
  future, security-reviewed increment)" — **F3 is the named hole.**
- MVP transport = **stdout NDJSON** (no network); the local WebSocket / named pipe + mobile companion are
  *designed, not built* (D-12); control ingress is a **stub command-ring** only.
- Design: a producer pushes a POD frame to a lock-free ring; a cold drain serializes versioned NDJSON.
  **The frame is the egress payload.**
- The FG already has the sensitive fields in its working set first-hand: it resolves the captured window by
  **title substring** (`main.cpp:121`), reads window titles via `GetWindowTextA` (`:1839`), and resolves a
  target **PID** (`:6887`); the telemetry schema (`FG_TELEMETRY_PRIOR_ART.md §3`) leads group-A with
  `Application` (window title/process) + `ProcessID`, group-C with per-device GPU telemetry (`_4090` /
  `_1080Ti` / `_iGPU` suffixes).

**One line:** F3 is **safe by default today only by accident of immaturity** (stdout, in-process) — the
hole opens precisely when the WebSocket/mobile transport ships, with no posture defined yet. That posture
is this dossier's deliverable.

---

## §2 — SOTA findings (leveled)

- **S1 `[V1]` OBS-WebSocket (the canonical local control-plane prior art):** default port 4455;
  **authentication ON by default with a randomly-generated password** since v5.x; auth = SHA-256
  challenge/response before any control. (github.com/obsproject/obs-websocket protocol)
- **S2 `[V1]` the binding/threat-model lesson:** a maintainer **dismissed a "bind localhost-only" request**,
  arguing password auth suffices and "websites can abuse even local-only sockets" — **the second clause is
  the key F3 insight: localhost binding does NOT protect against a browser-borne attacker** (see S4). The
  reachable-socket attack surface enumerated: write arbitrary files, read config/secrets, capture audio.
  (obs-websocket #907)
- **S3 `[V1]` OBS-WebSocket → RCE chain:** malicious JS on a visited site connects to `ws://localhost:4455`;
  if auth is disabled (UI "one click away"), it chains `CreateInput`(browser source) + `SaveSourceScreenshot`
  to write an `.hta` to Startup → RCE on reboot. **Lesson: a control channel with file-write/spawn commands
  is an RCE primitive; auth-default is the only thing between the user and remote compromise.**
  (jorianwoltjer.com OBS WebSocket to RCE)
- **S4 `[V1]` Cross-Site WebSocket Hijacking (CSWSH) + the loopback gap:** **the Same-Origin Policy does
  NOT apply to WebSocket handshakes, and the browser does NOT enforce SOP on loopback sockets** — any page
  the user visits can open a handshake to `ws://localhost:<port>`. This is *why* "localhost-only" is
  necessary-but-insufficient. **Mitigation = a server-side `Origin`-header allowlist on every handshake**
  (browsers send a truthful, JS-unforgeable `Origin`). (PortSwigger CSWSH; OWASP WebSocket Cheat Sheet)
- **S5 `[V1]` OWASP WebSocket posture (the minimal correct set):** (a) WSS/TLS in production, never plain
  `ws://`; (b) validate `Origin` against an explicit allowlist on every handshake; (c) **"don't assume a
  WebSocket connection equals unlimited access — check authorization for each action"** (auth-before-
  control, per-message); (d) treat all inbound as untrusted, size-limit; (e) don't log sensitive data.
- **S6 `[V1]` Sunshine/Moonlight (the mobile-companion pairing benchmark):** control requires a **PIN-based
  cryptographic pairing first** (PIN salted → AES key; mutual cert exchange + cert pinning). No control
  before pairing.
- **S7 `[V1]` Moonlight pairing-crypto CVE (how this goes wrong even when designed):** CVE-2020-11024 —
  iOS/tvOS concatenated the PIN to the salt too early, leaking the PIN → MITM fraudulent pairing. **Lesson:
  hand-rolled pairing crypto is a recurring footgun — prefer a vetted handshake over a bespoke salt+PIN.**
- **S8 `[V1]` MSI Afterburner Remote Server (the closest direct analog: GPU telemetry + remote control to a
  phone):** configurable IP:port (LAN, not localhost-default), pre-shared "security key", gates remote
  overclocking, manual firewall rule. Posture = **PSK-before-control + explicit network opt-in (a separate
  component you must run).**
- **S9 `[V2]` RTSS local IPC** = shared memory (`RTSSSharedMemory`), no network listener by default; the
  network surface is the separate Remote Server (S8). **The local-IPC default (SHM/named pipe) carries no
  network threat model at all** — matches PhyriadFG's stdout-NDJSON MVP.

---

## §3 — The objectives resolved

### F3a — No host-identifying data leaves the machine without explicit opt-in

- **(a) Perfection target.** Every byte that can leave the box is enumerated + classified; the default
  transport is local-only; host-data fields are absent or behind an explicit network opt-in; the user can
  audit exactly what a remote consumer receives.
- **(b) Honest floor.** **None structural for egress** — what you put in the frame is fully under your
  control. The sensitive fields, enumerated from the in-repo schema: **`Application` / window title**
  (`:1839`; titles can contain document names, usernames, server names), **`ProcessID`** (`:6887`), **GPU
  model identity** (the `_4090`/`_1080Ti`/`_iGPU` suffixes; LUIDs/UUIDs if ever added are stable machine
  IDs), **NVML host signals** (a rig fingerprint in aggregate + a covert/side-channel surface over time),
  **per-game profile names / library** (future).
- **(c) Standing.** Safe by default today (stdout, in-process); **no field-sensitivity classification
  exists** in either anchor; window title/PID would flow into `Application`/`ProcessID` the moment the
  schema is populated.
- **(d) LSFG gap.** Not directly comparable — LSFG has **no telemetry egress or companion**, so its egress
  surface is ~zero. PhyriadFG's companion ambition is a **net-new surface LSFG does not carry**; "matching
  LSFG" = the default must remain as egress-free as LSFG. We currently match (stdout only); we **diverge the
  moment the WebSocket transport ships.**
- **(e) Closing levers.** L1 (T0): add a **field-sensitivity classification** to the frame schema
  (`public` / `host-identifying` / `secret`); the network serializer emits only `public` unless an explicit
  `--allow-host-data`; window title → a hash or "active/idle" bool by default. L2 (T0): **document the
  egress inventory** in CONTROLPLANE (the table the audit checks). L3 (T1): when the WebSocket lands,
  default to **local-only bind + Origin allowlist** (S2/S4).
- **(f) ★ Closeable success criterion — binary, testable:** "with the network transport enabled and
  `--allow-host-data` OFF, a packet capture of the transport over a full BF6 session contains NO window
  title string, NO raw ProcessID, NO GPU UUID/LUID, NO game-profile name — verified by grepping the captured
  stream for the known title/PID/UUID values." Done = grep returns zero with the toggle off, and them only
  with it explicitly on. Progress metric while building: **count of schema fields lacking a sensitivity
  tag (target 0).**

### F3b — No unauthenticated control ingress exists

- **(a) Perfection target.** No command-channel message mutates FG state (or spawns/kills a process, or
  writes a file) without the sender having authenticated; transport local-only by default; any network
  exposure explicit opt-in; auth-before-action **per-message** (S5c), not auth-at-connect.
- **(b) Honest floor.** None structural — a pure design discipline. The realistic floor is
  **operator-error-resistance**: OBS proves the dangerous failure mode is the user *disabling* auth (S3,
  "one click away"), and the localhost-binding debate (S2) proves a single config choice can't be trusted
  to protect a powerful API. So perfection = *the insecure configuration must be impossible-by-construction
  or loud-and-explicit*, not merely default-off.
- **(c) Standing.** **No control ingress is reachable today** — a stub command-ring with no transport;
  "control" = the Tauri shell re-spawns the FG with flags (in-process). F3b is satisfied **vacuously**
  (nothing to attack); the plan defers the auth surface → the hole opens when CP3 + WebSocket ships.
- **(d) LSFG gap.** LSFG has **no remote control ingress** → zero surface; the stub matches today. **The
  asymmetry to internalize: read-only egress is a confidentiality risk; control ingress is an
  integrity/RCE risk** (S3) — a categorically larger surface. The companion's control ambition is where
  PhyriadFG would *exceed* LSFG's surface; the bar is to ship it Moonlight-grade (S6), not OBS-grade
  "auth-you-can-click-off" (S3).
- **(e) Closing levers.** L1 (T0, do now in CP3): **split the ring topology so egress and ingress are
  independent** — the telemetry transport MUST be constructible with NO command-ring attached (read-only by
  construction; default build = ingress absent). L2 (T1): when control ships — local-only bind default +
  Origin allowlist (S4/S2) + auth-before-every-mutating-command (S5c, never "connected ⇒ trusted"). L3
  (T2, RISK_REGISTER): the mobile-companion pairing — **do NOT hand-roll PIN+salt crypto (S7 = a CVE from
  exactly that);** use a vetted PSK/handshake (Afterburner-PSK floor S8; Moonlight PIN-pairing + cert-pinning
  target S6) over WSS/TLS (S5a); network exposure beyond localhost = a separate explicit opt-in (Afterburner
  model S8). L4 (T0): **constrain the command vocabulary** — never expose a file-write/path/process-spawn
  primitive over the wire (S3's chain depended on `SaveSourceScreenshot`'s arbitrary write); commands = a
  closed enum of FG-tuning verbs.
- **(f) ★ Closeable success criterion — binary, two-part:**
  > (1) "with the control transport enabled, an unauthenticated client (a raw socket skipping the handshake
  > AND a cross-origin browser with a forged JS `Origin`) causes ZERO observable FG state change — every
  > mutating command rejected pre-auth, evidenced by a harness that sends each verb unauthenticated and
  > asserts the FG state is byte-identical before/after."
  > (2) "there exists NO build configuration in which a network-reachable control transport is active with
  > authentication disabled" (auth-off permitted only for the loopback+Origin-locked local case, if at all).

### The audit that proves F3 (a committed, CI-runnable `controlplane_security_audit`)

Against a running FG with the network transport on: **(A)** the egress grep (F3a-f) is clean with host-data
off; **(B)** the F3b-f unauthenticated-client harness asserts zero state change; **(C)** a static/type-level
assertion that no `network-bind + auth-disabled` config is constructible. All three green = F3 closed for
that increment. Belongs in a **`CONTROLPLANE_RISK_REGISTER.md`** (Tier-2) with each risk `mitigated`-as-code
before commit.

---

## §4 — Sources (leveled)

`[V1]`: obs-websocket protocol + issue #907 + the Woltjer RCE write-up; PortSwigger CSWSH; OWASP WebSocket
Security Cheat Sheet; Moonlight setup guide + GHSA-g298-gp8q-h6j3 (CVE-2020-11024); MSI Afterburner Android
remote-server guide. `[V2]`: RTSS shared-memory (Wikipedia/guru3D); Afterburner forum corroboration.
In-repo: `CONTROLPLANE_MASTER_PLAN.md` (the security deferral + the transport-pluggable boundary),
`FG_TELEMETRY_PRIOR_ART.md §3` (the schema), `apps/render_assistant/src/main.cpp` (:121, :1839, :6887).

## §5 — Cross-links

The telemetry schema this secures → `FG_TELEMETRY_PRIOR_ART.md`. The pillar being secured →
`CONTROLPLANE_MASTER_PLAN.md`. The trust family it belongs to →
[`FG_TRUST_ANTICHEAT_PRIOR_ART.md`](FG_TRUST_ANTICHEAT_PRIOR_ART.md).

## §6 — What could NOT be verified (first-hand)

- **obs-websocket's exact default bind interface** (127.0.0.1 vs 0.0.0.0) in current 5.x — port 4455 +
  auth-default-on confirmed by the sweep; the bind default not stated verbatim. The RCE chain (S3) is a
  researcher write-up, not a vendor-acknowledged advisory.
- **Whether obs-websocket validates `Origin` when auth IS on** — `[V3]`/unconfirmed (the RCE hinges on the
  user *disabling* auth).
- **Moonlight's six-step pairing crypto detail** — `[V2]` corroborated; the CVE mechanism IS `[V1]`.
- **Afterburner Remote Server's wire crypto** (hashed/TLS vs plaintext-PSK) — unconfirmed (likely plaintext
  given its vintage).
- **No PhyriadFG runtime security behaviour is measured** — none exists yet (CONTROLPLANE is `designed`,
  ingress is a stub); all F3 standing claims are about design state.
- **The author did not re-fetch the §2 URLs first-hand this session** — they carry the sweep's levels.
