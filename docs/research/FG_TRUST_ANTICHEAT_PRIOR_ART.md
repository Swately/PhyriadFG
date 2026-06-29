# FG_TRUST_ANTICHEAT_PRIOR_ART — anti-cheat ban/flag safety + the foreign-present side effects

> **Diátaxis type:** Analysis (SOTA dossier). **Status:** `measured` (external track record + API
> behaviour) / `designed` (the trust matrix + the real-rig protocol). Resolves objectives **F1**
> (anti-cheat ban/flag risk of an external-capture, no-injection FG) and **F2** (the foreign-present
> surface side effects: VRR/G-Sync, MPO, fullscreen). The companion to
> [`PHYRIADFG_OBJECTIVE_VISTA.md`](PHYRIADFG_OBJECTIVE_VISTA.md) §1-F / §3.
>
> **This is a SAFETY dossier — treated maximally conservatively.** PhyriadFG's stated reason to exist
> ("external-capture only, anti-cheat-safe; must run on Battlefield 6", `FG_ARCHITECTURE_DCAD_MASTER_PLAN.md:29-31`)
> rests on this dimension, and the dimension had **no dedicated dossier** before this one.
>
> **Provenance + verification.** From the gap-family SOTA sweep (workflow `w83r5ydl9`, the trust agent,
> web-grounded). **The load-bearing safety claims here were re-verified FIRST-HAND by the author this
> session** (tagged `[VS]` with the primary URL); the rest carry the sweep's `[V1]/[V2]/[V3]` levels.
> Per FDP §2: no fabricated citation; §7 lists what could NOT be verified — read it before acting.
>
> **Normativity (BCP 14):** MUST / MUST NOT / SHOULD used only where a real safety requirement binds.

---

## §1 — PhyriadFG current standing (first-hand, in-repo)

- F1 is the **product's reason to exist**: "external-capture only (anti-cheat-safe; no engine /
  motion-vector injection; must run on Battlefield 6)" — `FG_ARCHITECTURE_DCAD_MASTER_PLAN.md:29-31`.
- WGC capture is asserted anti-cheat-safe on **`[V2]` community grounds only**: "WGC anti-cheat-safe
  (OBS-class, no injection); residual risk = overlay fingerprint, not capture" — `DCAD:361` (the cited
  source is the LSFG Steam page, i.e. a community claim, not a vendor statement).
- The overlay fingerprint is an **OPEN, UNRESOLVED operator decision**: "a game-sized WDA-excluded
  topmost DComp overlay matches a known EAC heuristic pattern… For BF6 this is product-killing if it
  fires… Needs a real-rig verdict" — `DCAD:323-325` (§9 decision-4). **No real-rig test protocol exists
  yet** — this dossier designs it (§4).
- F2 floor already named: "VRR/G-Sync/Reflex categorically unavailable to a DComp overlay (LSFG hits the
  identical wall, disables G-Sync)" — `PHYRIADFG_ARCHITECTURE_GAP_AUDIT.md:399`, `:673`.
- The injection line is correctly drawn: "present-hook injection = anti-cheat ban" — `GAP:670`, `:173`.

**One line:** the safety claim currently rests on a `[V2/V3]` community premise + one untested open
decision, with **no first-hand evidence and no trust matrix.** The research below shows that premise is
*mostly* defensible **but has two concrete holes the repo under-weights**, one of which (WDA-exclusion) is
a design choice PhyriadFG actively makes that LSFG's track record may not cover.

---

## §2 — SOTA findings (leveled)

### A — The injection boundary (what actually triggers a flag)

- **`[VS]`** Epic Online Services / EAC: tools that "rely on undocumented OS behaviors, use self-modifying
  code, or patch system libraries" conflict with the anti-cheat; an injected render hook is the banned
  class. (https://dev.epicgames.com/docs/game-services/anti-cheat/using-anti-cheat)
- **`[V1]`** OBS *game-capture* (an injection hook) requires an explicit anti-cheat allow-list via a signed
  certificate; EAC is updated per-game to exempt OBS's hook — proving injection-capture IS monitored and
  needs a vendor exemption, **whereas WGC needs none because it does not inject.**
  (https://obsproject.com/kb/capture-hook-certificate-update)
- **`[V2]`** "Overlays based on graphics hooks are technically a form of process injection" and some
  anti-cheats auto-flag the hook (ReShade/GeForce Freestyle in protected titles).

### B — The overlay-WINDOW fingerprint (the real residual risk for the no-injection class)

- **`[V2]`** Anti-cheats enumerate top-level windows (user-mode `EnumWindows`/`GetWindow`, and from the
  kernel) and flag a window that is (a) sized to the game client area, (b) `WS_EX_LAYERED` +
  `WS_EX_TRANSPARENT`, (c) `WS_EX_TOPMOST`: "if the size of the queried window matches the size of the game
  window with the topmost style, this may result in a flag and/or ban." This is **exactly the DComp overlay
  fingerprint `DCAD §9-4` worried about.** (guidedhacking detect-external-overlays; ucp-anticheat.org)
- **`[VS]`** Microsoft documents `SetWindowDisplayAffinity(WDA_EXCLUDEFROMCAPTURE)` as, verbatim, **"unlike
  a security feature or an implementation of Digital Rights Management (DRM), there is no guarantee that
  using SetWindowDisplayAffinity… will strictly protect windowed content"** and it "works only when the
  Desktop Window Manager (DWM) is composing the desktop."
  (https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowdisplayaffinity)
- **★ `[VS]` — the single most important finding.** Cheats use `WDA_EXCLUDEFROMCAPTURE` to hide their
  overlays from recording/screenshot tools, and **anti-cheat systems actively monitor for this technique
  as a cheat indicator, saving it for server-side upload** ("anti-cheat systems loop through all windows
  checking for layered window styles and comparing window rectangles with game rectangles to detect
  external overlays"). Corroborated across the CoD/TAC reverse-engineering write-up (ssno.cc), the
  screen-invisible-overlay write-up (Medium / adamsvoboda), and overlayhack.com — author re-confirmed via
  first-hand search this session. **PhyriadFG's overlay is WDA-excluded (`DCAD §9-4`), so it performs the
  exact technique anti-cheats fingerprint as cheat-class.** This is a divergence from a plainly-capturable
  overlay.

### C — Track record of comparable tools (the empirical safety basis)

- **`[VS]` (strongest pro-safety source) BattlEye FAQ:** "non-cheat overlays and visual enhancement tools
  like Reshade or SweetFX are generally supported unless desired otherwise by the game developers (the
  latter two are currently blocked in PUBG, Fortnite and Islands of Nyne)… [BattlEye may] kick (not ban)…
  for… macro tools." (https://www.battleye.com/support/faq/)
- **`[V1]` LSFG's own line (Steam):** "The community has found Lossless Scaling to be safe… **However,
  this cannot be guaranteed and should be used with user discretion.**" — the rival makes **no safety
  guarantee**; the basis is community track record. (https://store.steampowered.com/app/993090/)
- **★ `[VS]` (BF6-specific, refutes safe-by-construction for BF6):** multiple Battlefield 6 users report
  account bans after running Lossless Scaling ("3-day bans for Gameplay Enhancements"); EA's Community
  Manager response on one case = ban "may have been applied incorrectly", directed to appeal — i.e.
  **likely false-positive, NOT a confirmed detection, but explicitly NOT cleared either.** EA's repeated
  official stance: *"EA doesn't recommend third-party software that may affect gameplay."* EA has **not
  tested, cleared, or documented** LSFG-class tools for Javelin. (EA Forums 12741924 / 12780739, verified)
- **`[V2]` BF6 Javelin posture:** kernel-level (Secure Boot + TPM 2.0 mandatory); coverage reports
  "overlays, crosshair apps, **some frame interpolators** and other third-party tools can look like cheat
  tools and be blocked." Note: "blocked" ≠ "banned" — likely launch-block, unverified which.
- **`[V3]` VAC (CS2):** third-party overlays require `-allow_third_party_software` → **non-trusted mode**
  (matchmaking-restricted, not banned). False-positive ban waves exist (AMD Anti-Lag+ Oct 2023; a Jan 2026
  CS2 wave) — all reversed.
- **`[V2]` Riot Vanguard:** kernel, boot-start; **screenshots the full screen including overlays,
  server-side** — a categorically different surface that a WDA-excluded overlay specifically tries to
  defeat (the adversarial posture Vanguard's screenshot module exists to catch).

### D — F2: foreign present surface side effects

- **★ `[V1]` (the F2 answer):** "the NVIDIA G-Sync activation algorithm **does not** trigger for any
  Window/Overlay that doesn't hook into the game, while AMD and Intel's VRR do." LSFG creates a fullscreen
  unfocused overlay that **directly disables G-Sync**; PresentMon hits the same. No official NVIDIA fix.
  (https://forums.developer.nvidia.com/t/fix-vrr-for-overlays-always-on-top-windows/296168)
- **`[V2]` AFMF (the truest F2 comparable) does NOT break VRR** — it is **driver-integrated** (intercepts
  the swapchain inside the AMD driver, not a foreign window), so VRR is recommended and works. **The
  structural F2 split:** driver-level FG keeps the game's flip/VRR; external-overlay FG (LSFG, PhyriadFG)
  cannot — on NVIDIA specifically. On AMD/Intel VRR DOES work with overlays.

---

## §3 — The objectives resolved

### F1 — Anti-cheat ban/flag safety

- **(a) Perfection target.** Runs on every kernel-anti-cheat title with zero ban risk AND zero launch-block,
  evidenced on real rigs, indefinitely.
- **(b) Honest floor (why perfect is unreachable).** (1) No vendor guarantees external-tool safety — it is
  heuristic + discretionary; you can be flagged or launch-blocked without injecting. (2) Kernel anti-cheats
  see the overlay regardless of injection (Vanguard's server-side screenshot; Javelin's kernel window
  enumeration). (3) **WDA-exclusion is an active cheat-class signal** (§2-B★) — a self-inflicted risk LSFG
  may not share. Safety is **asymptotic against a moving, vendor-controlled heuristic**, not certifiable.
- **(c) Standing.** Asserted safe on `[V2/V3]` grounds (`DCAD:361`); the fingerprint is an admitted open,
  untested decision (`DCAD:323-325`). The "must run on BF6" claim (`DCAD:29`) is **unverified and
  contradicted by the reported (ambiguous) LSFG BF6 bans.**
- **(d) LSFG-specific gap.** Parity on the no-injection capture (both WGC, OBS-class). **★ UPDATE
  2026-06-22 (operator-observed, Lightshot):** the operator confirmed that LSFG's FPS-counter overlay is
  NOT captured by software screenshot tools (Lightshot) — strong evidence, with the community OBS-black
  reports, that **LSFG uses the SAME display-affinity exclusion (`SetWindowDisplayAffinity`) as PhyriadFG.**
  So axis (1) (the WDA-exclusion divergence) **collapses to PARITY** — we do NOT carry a cheat-signal LSFG
  lacks; the signal is **shared**, and LSFG's empirical tolerance now partially covers us. The only residual
  asymmetry is (2) **track record** — LSFG has years of millions-of-users tolerance, we have zero. Best
  achievable = *match LSFG* (no longer "minus the WDA divergence"). Strong evidence, **not lab-confirmed** —
  a `GetWindowDisplayAffinity` probe on the LSFG window would make it definitive (and would distinguish
  `WDA_EXCLUDEFROMCAPTURE` from `WDA_MONITOR`, though the anti-cheat treats both as the same signal).
- **(e) Closing levers.**
  - **T0 — drop/condition WDA-exclusion (now OPTIONAL given the §(d) parity update).** A capturable overlay
    removes the WDA cheat-signal and would make us *safer than LSFG* on that axis — but since LSFG carries
    the same signal and is empirically tolerated, this is a *go-beyond-LSFG* option, not a mandatory fix.
    Trade-off: the overlay then appears in the user's own captures/streams. Decide per the kernel-AC tier.
  - **T0 — de-fingerprint the window:** avoid the exact (game-sized + `WS_EX_TOPMOST` +
    `WS_EX_LAYERED|TRANSPARENT`) tuple; a DComp child-visual without a classic layered HWND reduces the
    enumeration signature (untested — §7).
  - **T1 — the per-engine trust matrix (§4) + an in-product trust gate** that refuses/warns on UNSAFE /
    UNTESTED titles.
  - **T2 — the real-rig empirical campaign (§4):** the only thing that converts `[V2/V3]` into evidence.
  - **Out-of-class (never):** present-hook capture / fresh-input hooks — confirmed ban (`GAP:670`).
- **(f) ★ Closeable success criterion — a MATRIX of per-engine binaries** (not one global binary):
  > **Per-engine (binary, evidenced):** "on engine E, a throwaway account runs PhyriadFG attached to a real
  > protected title for ≥N hours of active multiplayer across ≥2 driver versions and ≥1 anti-cheat update
  > cycle, with zero kick / launch-block / ban and trusted-mode retained where applicable — evidenced with
  > screenshots + an account-status log."
  > - **VAC / BattlEye / EAC: CLOSEABLE per-title.**
  > - **Vanguard / BF6 Javelin: ASYMPTOTIC, not binary** — server-side discretionary; honest "done" =
  >   "survives the campaign AND the operator accepts residual risk + recommends against ranked use." The
  >   metric: **cumulative survived-account-hours per (engine × title × anti-cheat-version), reset to 0 on
  >   any anti-cheat update.** Never let it become "we believe it's safe."

> **Hard honesty flag:** `DCAD:29` "must run on Battlefield 6" is a **requirement, not a verified
> capability**, and the only real-world data (LSFG BF6 bans, EA non-clearance) makes BF6/Javelin the
> *worst-case* title, not the safe-by-construction tier.

### F2 — Foreign present surface side effects (VRR/G-Sync, MPO, fullscreen)

- **(a)** No display-stack regression: VRR stays engaged, no MPO/fullscreen destabilization.
- **(b) Floor (structural on NVIDIA):** a non-hooking external present surface can't enter the game's flip
  chain → DWM composition → **NVIDIA G-Sync does not trigger** (§2-D★, `[V1]`). No external tool fixes it;
  the double-present is also irreducible. **AMD/Intel VRR works with overlays → closeable there.**
- **(c) Standing.** Correctly named as a floor in-repo (`GAP:399`, `:673`); this dossier upgrades it to a
  **vendor-asymmetric, primary-sourced** floor + the AFMF contrast.
- **(d) LSFG gap.** Exact parity on NVIDIA (both lose G-Sync); AFMF (driver) leads both; on AMD/Intel we
  can match-not-lose.
- **(e) Levers.** T0 vendor-conditional honesty + windowed-G-Sync guidance on NVIDIA; T1 a displayable/MPO
  present-mode probe (likely null on NVIDIA, possibly positive on AMD/Intel). **Never:** hook the game's
  swapchain to inherit its flip = injection = ban.
- **(f) ★ Closeable criterion — split by vendor:**
  > - **AMD / Intel: CLOSEABLE BINARY** — monitor OSD reports VRR ACTIVE with PhyriadFG attached,
  >   photo/OSD-evidenced.
  > - **NVIDIA G-Sync: NOT closeable — a permanent class floor (shared exactly with LSFG).** Stop trying;
  >   honest "done" = documented limitation + the windowed-G-Sync partial mitigation.

---

## §4 — Deliverable: the per-engine trust matrix + the real-rig protocol

### Trust matrix (initial, evidence-graded — every cell HEURISTIC/UNTESTED until the campaign runs)

| Engine | Injection | No-inject overlay tolerance | WDA-exclusion risk | F2 (VRR) | PhyriadFG class | Closeable criterion |
|---|---|---|---|---|---|---|
| **BattlEye** | hook=ban `[VS]` | tolerated unless dev-blocked `[VS]` | elevated | NV off | PROBABLY-SAFE (if not dev-blocked + WDA off) | survive N hrs; check dev blocklist |
| **EAC** | hook=ban, defers to game `[V1]` | per-game; OBS hook needs allowlist, WGC doesn't | elevated | NV off | PER-TITLE — UNTESTED | per-title survival; confirm no WGC allowlist |
| **VAC** | overlays → non-trusted `[V3]` | tolerated, trust-flagged | moderate | NV off | SAFE-FROM-BAN, TRUST-COST | survive + retain trusted mode (or doc cost) |
| **Vanguard** | kernel; screenshots server-side `[V2]` | adversarial to capture-evasion | **HIGH (WDA = the thing it hunts)** | NV off | HIGH-RISK / RECOMMEND-AGAINST | asymptotic; track survived-hours only |
| **BF6 Javelin** | kernel + Secure Boot; "some frame interpolators blocked" `[V2]`; reported LSFG bans `[VS]` | unclear; EA does not clear | **HIGH** | NV off | **HIGH-RISK / UNVERIFIED — the worst-case title** | asymptotic; treat `DCAD:29` as UNMET |

### Real-rig test protocol (the closeable instrument)

1. **Throwaway accounts only** (never the operator's main; bans may be unappealable). One per engine.
2. **Per (engine × title × anti-cheat-version × GPU-vendor):** confirm attach (or record a launch-block);
   active multiplayer ≥N hours (N≥10, ≥2 sessions, ≥2 driver versions, spanning ≥1 anti-cheat update); run
   **two arms — WDA-excluded ON vs OFF** (isolates §2-B★); log every kick / launch-block / trust-flag
   change / ban; screenshot account status before+after.
3. **F2 arm:** on a VRR panel, read the monitor OSD for VRR-active per vendor; photo evidence.
4. **Record:** append **survived-account-hours** per cell; **reset the cell to UNTESTED on any anti-cheat
   update.** This is the matrix's living evidence column and the operator's progress metric.
5. **Gate:** until a cell is evidenced, the in-product trust gate classifies the title UNTESTED and warns;
   Vanguard/Javelin titles default to RECOMMEND-AGAINST regardless of survived-hours.

---

## §5 — What could NOT be verified (first-hand)

- ~~Whether LSFG's overlay is WDA-excluded or plainly capturable.~~ **RESOLVED 2026-06-22 (operator-observed,
  Lightshot):** LSFG's FPS overlay is NOT captured by software screenshot tools → LSFG uses the same
  display-affinity exclusion → the divergence is **PARITY** (strong evidence, not lab-confirmed; a
  `GetWindowDisplayAffinity` probe would make it definitive). NOTE: this means LSFG is uncapturable by
  *software* (Lightshot/OBS/screenshots — all DWM-composition capture), **NOT by a physical camera /
  capture card** (the photons are on the panel; MS's own doc says display-affinity "is not a security
  feature… someone takes a photograph of the screen") — which is exactly why the H2 head-to-head needs a
  camera (see `FG_MEASUREMENT_METHODOLOGY_PRIOR_ART.md`).
- **Whether BF6/Javelin BANS vs merely LAUNCH-BLOCKS** external FG — the causal link is unconfirmed in both
  directions (the one reported LSFG ban was called "may have been applied incorrectly").
- **Whether a pure-DComp child-visual (no classic layered HWND) evades the enumeration fingerprint** —
  untested; could be safer, OR the WDA flag could dominate regardless.
- **Whether Vanguard's server-side kernel capture sees a WDA-excluded window specifically** — kernel
  capture likely bypasses DWM affinity, but unverified.
- **No anti-cheat vendor has any published statement about frame-generation tools specifically** — all
  FG-specific conclusions are extrapolated from the overlay/capture-tool class.

---

## §6 — Sources (leveled)

`[VS]` author-verified first-hand this session: MS SetWindowDisplayAffinity
(learn.microsoft.com/.../nf-winuser-setwindowdisplayaffinity); BattlEye FAQ (battleye.com/support/faq);
the WDA-as-cheat-signal corroboration (ssno.cc TAC reversing, adamsvoboda.net, medium @vedavyas9990); EA
Forums BF6+LSFG ban threads 12741924 / 12780739; Epic/EOS anti-cheat docs; LSFG Steam page.
`[V1/V2/V3]` from the sweep: guidedhacking/ucp-anticheat overlay-fingerprint; OBS hook-certificate;
NVIDIA dev-forum VRR-for-overlays; AFMF release notes; Vanguard / VAC behaviour (community-leveled).
In-repo `[VS]`: `FG_ARCHITECTURE_DCAD_MASTER_PLAN.md` (§0:29-31, §6.4:230-231, §9-4:323-325, §10:361),
`PHYRIADFG_ARCHITECTURE_GAP_AUDIT.md` (§3.7:399, :673, §4 floor:670-684, :173).
