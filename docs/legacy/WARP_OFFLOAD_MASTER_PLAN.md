# WARP_OFFLOAD_MASTER_PLAN — STAGE-103 `--warp-offload`

> **Diátaxis type:** Explanation + plan (a design master-plan, not a tutorial).
> **Status buckets used:** `measured` (a first-hand number or a number the operator
> verified), `designed` (specified, not yet built), `assumed` (a model/estimate,
> flagged). No claim is `shipping` in this doc — nothing here is built yet.
> **Scope:** route the per-tick WAP (warp-at-presenter) synthesis pass to device B
> (the GTX 1080 Ti) when device A (the RTX 4090) is saturated, as an opt-in,
> default-off, byte-identical-off flag. Sibling: `WARP_OFFLOAD_IMPLEMENTATION_STRATEGIES.md`.
> **Verification note:** every `file:line` below was confirmed first-hand against the
> working tree (incl. the uncommitted STAGE-98..102) unless tagged `[map]` — those come
> from the `warp-offload-map` workflow and the implementer MUST re-confirm before citing.

---

## §0 — The honest verdict, up front

**By Phyriad's own measured break-even arithmetic, routing the WAP warp to the
1080 Ti is a known-negative move.** This is not a guess and it is not new:

- `break_even_decide()` (`framework/gpu/include/phyriad/gpu/GpuDescriptor.hpp:95`,
  crossover documented at `:78`) computes the offload crossover for the 4090+1080 Ti
  rig at **~596 FLOP per result-byte**. *(measured — the pillar's own constant.)*
- The WAP warp's arithmetic intensity is **≈ 2.5 FLOP/byte** — roughly **240× below**
  the crossover. It is overwhelmingly transfer-bound, not compute-bound.
- `apps/render_assistant/src/main.cpp:50-52` already records this: *"the gpu pillar's
  break_even_decide() was evaluated for the FG offload decision — at FG's AI≈2.5
  FLOP/byte (vs the ~596 crossover) it returns offload=false ALWAYS, a constant.
  Inlined as such."* *(measured — STAGE-26b, first-hand verified.)*

So the substrate already answered the routing question with a measured "no". STAGE-103
does **not** expect to beat A; it exists to (a) **empirically confirm** that verdict on
the operator's real hardware with his eyes on the result, and (b) hold the flag for the
*one* regime where the arithmetic could flip: a **GPU-bound game where B is genuinely
idle AND the F-thread is not CPU-starved** (the opposite of the BF6-combat profile,
where the 1080 Ti's idle-23% was measured to be CPU-starvation, not spare capacity).

The build is therefore justified as **a measured experiment with a pillar-faithful
decision gate**, not as a performance win we expect. The plan below is written to make
that experiment *safe* (default-off, byte-identical-off), *honest* (the route decision
uses the real `break_even_decide`, not a fresh hardcode), and *cheap to abandon* if the
operator's combat test confirms the negative.

---

## §1 — Motivation (why this is on the table at all)

The combat diagnosis (per-frame telemetry, operator-verified): under BF6 active combat
the FG collapses — present **85 fps** of 240, **fg_multiplier 1.07×**, AddedLatency
**95 ms median / 176 ms max**, 4090 **99%**, 1080 Ti **23%**. Two compounding fronts:
- **(A) latency** — the synchronous present blocked on the warp fence (addressed by
  **STAGE-102 `--async-present`**, built + verified, byte-identical-off).
- **(B) multiplier collapse** — the F-thread's per-pair CPU tail under dissidence:99%
  blows the source budget (addressed by **STAGE-101 `--load-governor`**, built).

The SOTA contention pass (`w6z5bdj3y`) listed **#2 = offload the warp to B** as a
*complementary* lever: free the saturated 4090 by moving the synthesis to the otherwise-
idle 1080 Ti. The SOTA's own F13 caveat: *a too-slow secondary made LSFG's dual-GPU mode
WORSE.* The 1080 Ti is Pascal; this caveat is the live risk, and §2 shows the rig's
measured numbers put us squarely on the wrong side of it.

---

## §2 — The walls (measured, and why they bind)

| # | Wall | Number | Source | Consequence |
|---|------|--------|--------|-------------|
| W1 | Break-even crossover | ~596 FLOP/byte | `GpuDescriptor.hpp:78` *(measured)* | Warp at AI≈2.5 is ~240× under it → `break_even_decide`→false |
| W2 | No consumer P2P | host-bounce only | map readers 3+4, [[gpu-multi-gpu-prior-art]] *(measured, prior art)* | Every A↔B move is device→host-coherent buffer→device; no NVLink/SLI |
| W3 | 1080 Ti link | x4 PCIe ~5 GB/s | map reader 4 `[map]` (re-measure with `measure_transfer_bw`) | The B→A output path is bandwidth-thin |
| W4 | Per-tick output | ~8.3 MB RGBA8 @ up to 240 Hz | map reader 3 *(assumed from WW·WH·4)* | wapOut must reach A's `bridge_img` *every present*, ≈2 GB/s sustained |
| W5 | B is Pascal + busy | flow + gme already on B.q | map reader 2 *(measured: pipelines exist)* | The warp would contend with B's existing per-pair work |
| W6 | Combat B-idle is fake | 1080 Ti 23% = CPU-starved | combat telemetry *(operator-verified)* | In the *target* scenario B has no spare cycles to receive the warp |

**The dominating wall is W4 combined with W1.** Today the warp runs on A, writes
`wapOutA` on A, and blits into `bridge_img` on A — **zero cross-device cost**
(`main.cpp:6308-6310`, verified). Moving the warp to B turns that free A-local blit into
a **per-tick full-frame B→host→A transfer**, on a ~5 GB/s link, for a pass whose
arithmetic intensity is already 240× below the offload crossover. The warp also runs at
the **panel rate** (~240/s) while its inputs change only **per pair** (~30/s) — so the
expensive, output-heavy pass is exactly the wrong one to push across the slow link.

---

## §3 — Architecture: what "warp on B" actually requires

The warp pipeline lives **deliberately on A** (`main.cpp` STAGE-45b note: *"the
warp-at-presenter pipeline lives on A now"*) because A owns the present surface. Moving
it to B requires four things; the map confirms the first is cheap and the last is the cost:

1. **A B-side warp pipeline `wapPipeB`** — mirror `wap_create(...)` on `B.dev`
   (the 14-binding descriptor set + sampler + pipeline + the 200 B push range,
   `wap_create` at `main.cpp:~2196` region, verified pcr.size=200/50 floats). Graceful
   fallback to A on any creation failure (the `--gme-gpu` precedent, `main.cpp:~3716` `[map]`).
2. **Inputs on B — mostly already there.** The two reals are resident as `Bframe[0]`/
   `Bframe[1]`; the forward MV/SAD are `ofp.motion_image()` / `ofp.sad_field_image()` on
   B; dissidence is B-produced when `--gme-gpu`. *(map readers 2+3, consistent.)* The
   A-only masks — inertia `wapPERA`, the iGPU field `wapFIELDA`, the vblend target
   `wapMVTA`, the ts-smooth history `wapPrevOutA` — must be allocated on B and fed
   (`wapFIELDA` from G, `wapPrevOutA` is B-local feedback once the warp is on B).
3. **The route decision** — call the real `break_even_decide` / `select_participants`
   (`framework/holons/.../RoutingPolicy.hpp`) instead of a fresh hardcode; `--warp-offload`
   *enables/forces* the B route for the experiment (overriding the always-false constant).
4. **The output path B→A** — `wapOutB` (B-local) must reach A's `bridge_img` before
   present. This is the new, dominating cost (W4). Options are weighed in the
   IMPLEMENTATION_STRATEGIES doc §S3 (shared NT handle like the bridge / host-bounce /
   CONCURRENT image); all cross the ~5 GB/s link per present.

---

## §4 — Design contract (the principles the build MUST hold)

These are normative for the implementation (BCP 14 MUST/SHOULD):

- **D1 — Default-off, byte-identical-off (MUST).** With `--warp-offload` absent,
  `wapPipeB` and the B-side images are **not allocated**, no route decision runs, and the
  warp path is bit-for-bit today's A path. Mirror the STAGE-102 discipline exactly (the
  `ap`-gate + slot aliasing that made async-present byte-identical, verified this session).
- **D2 — Pillar-faithful decision (MUST).** The route uses `break_even_decide` /
  `select_participants` from the gpu+holons pillars (D-12: don't reinvent). `--warp-offload`
  may *force* the B route for the experiment, but the *measured* decision path MUST exist
  and be the default behavior when the flag enables auto-routing. Do **not** add a second
  parallel hardcoded heuristic — extend/feed the existing one.
- **D3 — Measured, not assumed (MUST).** `measure_transfer_bw` (`framework/gpu/.../Backend.hpp`)
  fills the real B link bandwidth at startup; the per-tick warp cost on B is reported via
  the STAGE-100 `--csv` telemetry (warp_ms + per-GPU util). No "it should be faster" — the
  operator's combat run produces the number.
- **D4 — Graceful degrade (MUST).** Any B-side creation failure, or a measured-unfavorable
  break-even at runtime, falls back to the A path with a clear log line — never a crash,
  never a silent worse-path (D-20).
- **D5 — The present pillar stays A-owned (MUST).** `bridge_img` and the PresentSurface
  remain on A; STAGE-103 offloads only the *synthesis warp*, then transfers its output to
  A's bridge. Do not move present ownership to B.
- **D6 — Honest telemetry (SHOULD).** The `--csv` row gains a `route_device` value that is
  truthful per tick (0=A, 1=B); a forced-but-rejected route logs the rejection.

---

## §5 — Phases and validation gates

| Phase | Deliverable | Gate (how we know it's done) |
|-------|-------------|------------------------------|
| P0 | This plan + IMPLEMENTATION_STRATEGIES | Operator reviews; the negative-verdict framing is explicit |
| P1 | `wapPipeB` creation on B + graceful fallback | Build green; `--warp-offload` off → byte-identical (no B alloc); on → B pipeline logs "ready" or falls back |
| P2 | Input residency + mask moves on B | The 14 bindings on B are all valid (the completeness-trick placeholders mirrored); a one-shot B warp produces a non-garbage frame |
| P3 | Output path B→A + present integration | A forced-B run presents correct frames (operator eye); the B→A transfer cost shows in `--csv` warp_ms |
| P4 | Route decision wired (`break_even_decide` live) | With auto-route, the decision logs A (the expected negative); `--warp-offload` forces B for the test |
| P5 | Operator empirical test (BF6 combat, one-by-one + combined with #1/#3) | `--csv` numbers + operator eye decide: keep flag (rare-case insurance) or shelve as a measured negative |

**Validation is the operator's runtime test** — I cannot runtime-verify (the present is
WDA-excluded; the live combat scenario is his). The build's gate is: build green +
byte-identical-off (verified first-hand, §2.3 applies harder for delegated work) + the
forced-B route produces correct frames.

---

## §6 — Recommendation

Build STAGE-103 as a **measured experiment**, holding D1–D6. Expect the combat test to
confirm the negative (W1+W4 dominate; the substrate's own arithmetic says so). The
deliverable's value is threefold and honest:
1. It **proves the negative empirically** on the operator's hardware (closing the "but did
   you actually try?" question with data, per the no-silent-caps discipline).
2. It leaves a **pillar-faithful routing seam** (the live `break_even_decide` wired into
   the warp path, replacing the STAGE-26b inlined constant) that auto-selects A today and
   would auto-select B *if* a future GPU-bound-game profile ever flips the arithmetic.
3. It is **safe to ship default-off** and **cheap to abandon** — no regression to the
   shipping A path.

If the operator prefers to **not** pay the full build for a known-negative, the honest
lighter alternative (documented in IMPLEMENTATION_STRATEGIES §S9) is a *measurement-only*
flag: time a one-shot B warp + the measured B→A transfer, report the break-even verdict,
and skip wiring the full per-tick B route. That confirms the negative at a fraction of the
cost. The operator has chosen the full build ("we've taken risks at every step"); this doc
honors that while keeping the verdict honest.
