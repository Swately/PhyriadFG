# RENDER_ASSISTANT — ready-to-dispatch stage kickoffs

**Date.** 2026-06-07. **Status.** ONGOING — dispatch queue. **Type.** How-to (procedure).
**Parent.** [`RENDER_ASSISTANT_PLAN.md`](../RENDER_ASSISTANT_PLAN.md) §8.

The operator delegated stage selection: the supervisor decides the sequence and pre-stages the Sonnet
kickoffs here so the operator dispatches immediately. Sequence toward LSFG-parity:

1. **STAGE-28b** — WGC capture backend in the app (the missing capture half). *Ready.*
2. **STAGE-29** — Nx present pacing (real+interpolated interleave = the LSFG-defining high-fps output). *Ready; may be lightly adjusted after 28b verifies.*
3. **STAGE-26** — load-aware router + iGPU payload-reduction (the multi-GPU differentiator). *Ready; adjusted after 29.*

Each is self-contained. Discipline for all: standalone where applicable, **NO commit** (report for supervisor
verification), `.gitignore` generated data, preserve protected last lines, INVENTORY scoped, do not touch
`ideas/`. The supervisor verifies first-hand + commits per protocol. **MSVC build recipe** (28b):
`cmd /c "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat && cmake -S <dir> -B <build> -G Ninja -DCMAKE_CXX_COMPILER=cl && cmake --build <build>"`.

---

## STAGE-28b — WGC capture backend in `render_assistant` (MSVC build)

Add a Windows Graphics Capture backend to `apps/render_assistant/`, behind `--capture-api dd|wgc` (default
`dd` for now; `wgc` opt-in until proven in-app). WGC captures HW-accelerated flip/overlay content that DD
cannot (proven in `bench/stage28_wgc`, plan §8) — the route to capturing real games/apps.

**Build move:** the app currently builds with mingw; WGC/WinRT needs **MSVC**. The bench proved D3D11+WinRT
compiles under VS18; Vulkan also compiles under MSVC. So **move the app's build to MSVC** (one build). Expect
to fix mingw-isms (GCC extensions, `__attribute__`, designated-init quirks) the MSVC compiler rejects — report
each. Keep the `CMakeLists` documenting both the old mingw note and the new MSVC recipe.

**The WGC backend** (reuse the proven code from `bench/stage28_wgc`): `GraphicsCaptureItem` from a **monitor**
(`--monitor N` → HMONITOR via `IGraphicsCaptureItemInterop::CreateForMonitor`) **or a window**
(`--window <substring>` → find the HWND by title substring → `CreateForWindow` — capturing the GAME window
directly is cleaner than the whole monitor). `Direct3D11CaptureFramePool::CreateFreeThreaded` → `FrameArrived`
→ `IDirect3DDxgiInterfaceAccess` (declare inline — the SDK 26100 interop header doesn't surface it under
cppwinrt) → `ID3D11Texture2D`. Feed that texture into the **existing** pipeline unchanged (convert → transfer →
FG → upscale → present). The format-routing (`route_for`) still applies (WGC frames are typically
`B8G8R8A8_UNORM` or `R16G16B16A16_FLOAT` for HDR).

**Verify (supervisor, first-hand):** MSVC build clean; `--capture-api wgc --window <game>` captures a HW-accel
game/app the DD path froze on, and the full pipeline (FG + present) runs. Honest report: the build move's
friction, any WGC frame-format surprises, the latency vs DD.

---

## STAGE-29 — Nx present pacing (the LSFG-defining high-fps output)

LSFG's defining feature is **fps multiplication**: present real + interpolated frames interleaved, paced to the
display, for a smooth Nx output. The app currently produces the FG interpolated frame but does not interleave +
pace it as a sustained Nx stream. Make the present output **real frame N → interpolated N+0.5 → real N+1 → …**
(2×; generalise to `--fg-factor N` for 3×/4× by generating N−1 intermediates at fractional phases), **paced**
to the present monitor's refresh (a frame-pacing clock — even spacing, not bursty), so the perceived motion is
smooth high-fps, not a double-image from mis-timed presents.

**Design notes:** the warp midpoint generalises from 0.5 to phase `t∈(0,1)` — `C_t = (1−t)·A[x−t·mv] +
t·B[x+(1−t)·mv]` (verify the sign against the pillar's MV convention A[o]≈B[o+mv]); for `--fg-factor N`,
generate intermediates at `t = k/N`. Pace presents on a steady clock; drop/duplicate gracefully if the source
rate drifts. The agreement gate + confidence gate apply per intermediate.

**Verify (supervisor):** the present sustains ~Nx the source fps, evenly paced (measure inter-present jitter),
real+interp interleaved correctly (no ghost/double-image from mis-pacing), motion visibly smoother. Honest:
latency cost of the added present cadence; behaviour when the source rate is irregular. Depends on STAGE-28b
(a working capture feeding it) — confirm the capture path before pacing on top of it.

---

## STAGE-26 — iGPU zero-copy assist stage + load-aware router (the multi-GPU differentiator)

**REVISED 2026-06-09 (Fable-5 fresh verification of the iGPU's role, operator-requested).** The measured
record re-confirms: the iGPU CANNOT add bandwidth to the 1080 Ti's x4 (no P2P; the x4 is its only door) and
CANNOT FG (256 ms). But the current app UNDERUSES it (upscale only; idle in --no-upscale) — its real value,
CPU-proximity (sysRAM IS its VRAM, 73.9 GB/s host-buffer BW, 0.76 ms 4090→iGPU), maps to a concrete move:

**26a — convert(+pack) on the iGPU, zero-copy in sysRAM.** Today the A-stage round-trips the 4090: capture →
Astage(host) → x16 up → convert on the 4090 → x16 down → hostR. The iGPU can do the SAME convert reading
Astage and writing hostR **directly in sysRAM — zero PCIe crossings** — and FUSE the STAGE-12 lossless pack
into the same pass, so the 1080 Ti pulls ~25 % fewer bytes over its x4 (bandwidth-bound → ~1:1 time win,
proven bit-exact). Effects: (1) the 4090 is freed of ALL assist work except present — the product case is a
SATURATED 4090; (2) two x16 crossings removed; (3) numerically: A ~1.3 ms off the 4090 + ~25 % off B's x4
legs ≈ iter 18.3 → ~16.5 ms ≤ the 60 fps cadence → drop→0 → **fg-factor 3 = 180 present on the 180 Hz
monitor** (the operator's authorized budget). Honest: the iGPU pass itself costs ~0.5–1 ms + a sync; in the
serialized loop the net gain is moderate — the BIG win is architectural (4090 freed) and compounds with
pipelining (STAGE-30) and the saturated case. Bench first (convert-on-iGPU vs 4090 round-trip: time +
bit-exact), integrate behind `--convert-gpu igpu|primary`.

**26b — load-aware router (STAGE-13):** per-frame 4090-spare FG (1.84 ms, no transfer) vs 1080 Ti offload
(when the 4090 saturates); `break_even_decide` from the `gpu` pillar; no per-frame stall, no oscillation.

**Rig-level option (operator's hardware decision, NOT in this stage):** cabling the 180 Hz monitor to the
motherboard/iGPU output would let the app present FROM the iGPU zero-copy (the 4090 does literally nothing
in the assist path). Cost: DWM cross-adapter copies for that monitor's normal desktop use. Flagged, not chosen.

**Verify (supervisor):** 26a bit-exact + timed on the rig; with fg 3 on a 60-capped source: sustained ~180
present / ~60 src / drop≈0 / lat stable / slip <2 ms; 26b routes correctly in both regimes. Honest: report
the iGPU pass cost and the net serialized gain separately from the architectural claim.

---

## STAGE-31 — extrapolation-mode bench (the latency-respecting FG variant)

*Added 2026-06-09 after the operator's input-lag question. Independent of STAGE-26 (own bench dir) — safe to
run as a second parallel Sonnet, or after 26.*

Interpolation inherently delays the real frame (~(N−1)/N·T + compute) — fps of *smoothness*, not input
*freshness*. The latency-respecting variant is **extrapolation** (VR ASW / async reprojection / Frame Warp —
known technique, NOT novel): forward-warp the LATEST frame by the measured MV, predicting Δ ahead, waiting for
nothing → ~zero added latency. Expected strictly WORSE quality (one source → no agreement gate possible;
disocclusion holes where the background gets revealed). The bench measures HOW MUCH worse, with numbers,
before any opinion.

**Bench `framework/render/vulkan/bench/stage31_extrapolation/`** (reuse the stage19/23 harness pattern):
- Mechanism: single-source forward warp `E_Δ(x) = B[x − Δ·mv]` using the A→B MV field from the pyramid
  (the only motion info available; the two-source form at t>1 weights A negatively — unstable, don't).
- Presets (analytic translation; frames A(t=0), B(t=1), ground truth T(t=1.5)): uniform motion, moving edge,
  fine-detail/HUD-static-over-moving-bg, and a **disocclusion** preset (an object revealing background — the
  canonical extrapolation failure; include it deliberately).
- Measure: PSNR(E_0.5 vs T) vs the SAME content's interpolation PSNR(C_0.5 vs its midpoint truth) — both are
  0.5 frames from the nearest source, comparable artifact scale. Sweep Δ (0.25/0.5/0.75). Report cost (one
  source → the warp is cheaper than interp).
- Verdict to report: the quality gap per preset + where it breaks (disocclusion) → is a `--fg-mode
  interp|extrap` app mode worth it for latency-sensitive use. Discipline: standalone, `.gitignore` for
  frames/, prep with explicit `--out`, protected last lines, NO commit — the supervisor verifies on the rig.

---

## STAGE-26c — iGPU convert integration + saturation-aware router objective

*Added 2026-06-09 after the FIRST REAL-GAME run (Battlefield 6, 4090 @96 %, 200-220 fps menu). The log proved
two defects with numbers: (1) the assistant STOLE 20-50 fps from the game — A-stage measured 9.7–11.7 ms
DURING play vs 2.37 ms after the game unloaded: the convert (+FG when routed primary) ran on the saturated
4090, competing with the game; (2) the router's objective is WRONG under saturation — "A fits my source-interval
budget" chose primary while blind to the game's fps loss, and thrashed (5 assist↔primary flips, signal at the
threshold).*

**26c-1 — integrate the proven 26a path** (`bench/stage26a_igpu_convert`, WIN 0.184 ms vs 0.649 ms, bit-exact):
`--convert-gpu igpu|primary` (default `igpu` when an iGPU exists). The iGPU converts(+packs RGBA→RGB) reading
Astage and writing hostR in sysRAM, zero PCIe crossings; the 1080 Ti FG path uploads the PACKED buffer over its
x4 (~25 % fewer bytes) and unpacks in its convert/FG ingest. The 4090's assist work shrinks to capture-copy +
present (the irreducible floor — LSFG pays the same class of cost).

**26c-2 — fix the router objective:** track the A-stage's IDLE baseline as a RUNNING minimum (never a one-shot
startup measure); when `a_ms_ema > kContendFactor × baseline` (start 2×), the primary GPU is CONTENDED (the
game owns it) → force assist regardless of budget-fit, with the existing K/dead-band hysteresis. The inflation
IS the saturation signal — measured: idle 2.37 ms, BF6 @96 % → 9.7–11.7 ms. This also kills the
threshold-thrashing seen live. **Game load is DYNAMIC (operator's field observation, measured): within one
match, A swings 3.8↔8.2 ms as the scene changes (sky ↔ hot zone) — the signal must stay per-frame/EMA
(~200 ms adaptation) and the hysteresis must absorb scene flips without thrash. Note the asymmetric isolation
measured live: B (1080 Ti) stays flat 6.7–7.0 ms regardless of the game — only 4090-resident work feels the
load. After 26c-1 the 4090's assist work = capture + present (the irreducible floor; not routable — the frame
lives on the game's GPU), so auto-routing mainly serves the NON-saturated case.**

**Verify (supervisor + operator):** on the rig with BF6 saturated: (a) the game's fps loss with the assistant
running drops to the capture+present floor (operator reads the game counter); (b) the router stays on assist
(no thrashing) while the game runs, returns to primary when the game closes; (c) bit-exact convert preserved
(the 26a gate); (d) both toolchains + the stats line shows the packed-path B leg shrink. NO commit — supervisor
verifies first-hand.

---

## STAGE-32 — capture throughput (the ~60/s callback ceiling)

*Added 2026-06-09 after the Fortnite + BF6-MAX real-game runs. THE MEASURED THREE-REGIME MAP: headroom
(Fortnite, 4090 <75 %) → the assistant's tax is ABSORBED, zero real-fps cost; saturated (BF6 MAX 200 % scale,
70-90 native) → ~10 fps tax AND present ~63 < native (negative value TODAY — the 26c chain: A 11.5-13.4 ms
contention → iter ~32 ms → only ~31/55 arrivals processed); and in BOTH regimes **`arr` never exceeds ~55-60/s
even when the game renders 90-110 fps** — the WGC callback itself is the source ceiling.*

**The bottleneck:** the FrameArrived callback does CopyResource → Map (blocks until the game GPU completes the
copy) → row-by-row CPU memcpy, all serialized under one mutex with one staging texture. That caps capture at
~60/s and degrades FG quality (pairs span 2-3 native frames at 90-110 fps sources).

**Fix candidates (measure in order, stop at the first that lifts the ceiling):**
1. **Staging ring (cheap):** N=3-4 staging textures; the callback only issues CopyResource into the next ring
   slot + timestamps it (no Map, no memcpy in the callback); the main loop Maps the newest COMPLETED slot
   (D3D11_MAP_FLAG_DO_NOT_WAIT probe). Decouples capture issue-rate from consume-rate.
2. **Skip the CPU memcpy:** map directly into the Vulkan-imported host buffer (write the staging Map's rows
   straight into Astage.mapped is what we do — the extra copy is Map→Astage; eliminating it needs the D3D11
   staging row-pitch to be consumable in place, or a D3D11 buffer (not texture) the CopyResource can target).
3. **(bigger, only if 1-2 insufficient) D3D11↔Vulkan shared-handle interop:** import the WGC frame texture into
   Vulkan on the SAME GPU (zero CPU round-trip) — the long-flagged zero-copy capture; substantial work.
**Verify:** `arr` follows the source (90-110 fps game → arr ≈ 90-110), capture tax on the game GPU does not
grow, bit-exact frames. Depends on 26c landing first (the loop must be able to CONSUME >60/s — today iter
~18-32 ms couldn't anyway). NO commit — supervisor verifies on the rig.

---

## STAGE-32b — FramePool depth under saturated queue

*Added 2026-06-10. STAGE-32's staging ring removed OUR staging serialisation, but arr ceiling ~57/90 persists
(measured §9 point 4). Root cause: `bufferCount=2` in `CreateFreeThreaded`. Under the saturated 4090 queue,
WGC's internal GPU copy takes ~25 ms/frame; with only 2 pool slots and frames arriving every ~11 ms, slot #3
finds the pool full → WGC silently discards the frame (FrameArrived never fires). Math: ⌈25/11⌉=3 > 2 → every
3rd frame dropped → arr ≈ 2/3 × 90 ≈ 60/s.*

**Fix-1 (landed, cheap):** `bufferCount` 2 → 6. Six pool slots absorb up to ~66 ms of GPU copy backlog before
any WGC-level drop. No change to `RING_N` (4) — ring capacity is a separate resource; ring-full drops are
design-intentional (always take newest).

**Fix-2 (only if Fix-1 insufficient — separate D3D11 device for CopyResource):** WGC passes its source texture
on OUR D3D11 device's queue. Our CopyResource also runs there. Under heavy game load both compete on the same
device queue. Fix: create a second `ID3D11Device` for CopyResource only; share the WGC source texture to it via
`IDXGIResource1::CreateSharedHandle` + second device opens handle; CopyResource runs on the second device's queue
(independent of game-GPU or WGC-internal queue). WGC's pool slot is recycled when the WinRT frame goes out of
scope (unchanged); the second device's queue drains independently. Substantially more code; only attempt if
Fix-1 leaves arr still <80/90.

**If Fix-1+Fix-2 both insufficient → document as OS/WGC wall:** WGC's own capture pipeline is rate-limited
under saturation regardless of buffer count or queue placement. Honest ceiling documented in PLAN.md §9 and
honesty status updated; do not force further.

**Gate:** BF6 saturated (~90 fps native) → arr ≥ 85/s (≥ 94 % of source). Both toolchains; protected lines;
INVENTORY unchanged; NO commit.

---

## STAGE-26c-3 — igpu-convert without upscale + DMA-then-unpack B ingest

*Added 2026-06-09 after the 26c integration verification (committed `bad79f3`). Two measured defects remain:*

1. **igpu-convert REQUIRES upscale today** (`use_igpu_convert` gates on `use_upscale`) — but the same-res game
   case (BF6/Fortnite on a same-res present monitor) wants `--no-upscale`, exactly where the 4090-freeing
   matters most. Fix: the pack pass gains a SECOND output — besides the packed RGB8 (hostRP, for B), write the
   unpacked RGBA8 into hostR (for the A-side present and the no-upscale path). sysRAM write cost ~+8 MB ≈
   +0.12 ms on the iGPU (73.9 GB/s) — trivial. Then drop `use_upscale` from the gate (keep the HDR/NAT!=WW
   fallbacks); the no-upscale present path reads hostR exactly as before.
2. **B's packed ingest reads host memory via SSBO ACROSS the x4** (compute-over-bus reads) — measured B
   6.85 → 9.4 ms, ERASING the byte saving. Fix: DMA-copy hostRP → a device-LOCAL buffer on B
   (vkCmdCopyBuffer, 5.9 MB over x4 ≈ 1.85 ms, less than the 8.3 MB RGBA upload it replaces), THEN unpack
   from VRAM (~0.2 ms local). Net B target ≈ 6.4 ms or better — the 25 % byte saving actually realized.

**Verify (supervisor):** with `--convert-gpu igpu --no-upscale --fg-gpu assist`: igpu-convert ACTIVE (no
fallback print), A ≈ 0.7-1 ms, B ≤ 6.9 ms (no SSBO-over-bus regression), present ≥ the pre-26c baseline
(~108 @ 60 fps desktop source), bit-exact visual. Both toolchains; protected lines; INVENTORY scoped;
NO commit.

---

## STAGE-33 — present from the iGPU (the recabled monitor; the 4090 down to capture-copy only)

*Added 2026-06-10. The operator CABLED the 180 Hz monitor to the motherboard/iGPU output (the 4090 now drives
only the 240 Hz game monitor). This was flagged twice as the structural endgame: the BF6 run (a) measured that
presents queueing on a saturated 4090 are the dominant remaining cost. With the present moved to G, the 4090's
assist work drops to the WGC capture-copy — the physical minimum of the genre.*

**Current gap (measured post-recable):** `--list-monitors` shows ONLY DISPLAY1 — `d3d_init` enumerates the
4090 adapter's outputs, so the iGPU-attached monitor is INVISIBLE to `--present-monitor`. Fix as part of this
stage: enumerate present candidates across ALL DXGI adapters (capture enumeration stays on the game GPU's
adapter); print which adapter owns each output in `--list-monitors`.

**The present-from-G path** (`--present-gpu auto|primary|igpu`, auto = the adapter that owns the chosen
present output):
- Window on the iGPU monitor's coords (borderless/topmost as today); VkSurface + swapchain created on **G's**
  VkPhysicalDevice/VkDevice (G presenting to its own attached output = the native path, no DWM cross-adapter
  copy). Reuse `pick_present_mode` (MAILBOX if the AMD iGPU exposes it, else IMMEDIATE, else FIFO — the
  STAGE-27a acquire-stall lesson applies).
- The present source on G: same pattern as A's (host buffer → Gpresent image → blit to swapchain) — but on G
  the "upload" reads sysRAM natively (≈free, 73.9 GB/s). Present semaphores/fences on G. The pacing loop and
  do_present parameterization (STAGE-29b) stay; do_present gains the device variant.
- When present-gpu=igpu: A's remaining per-frame work = the WGC CopyResource ONLY (no Apresent upload, no
  blit, no present submit on A).

**Verify (supervisor + operator):** (a) `--list-monitors` shows both monitors with owning adapters; (b) on the
rig: present window on the 180 Hz monitor updates smoothly, paced, content correct (bit-identical path);
(c) the decisive gate — BF6 saturated: game-fps retention improves beyond 26c-3's 85-75 (the present-queue
contention term disappears), `lat` stable or better, no present stalls on G (watch slip); (d) both toolchains;
the Vulkan surface on G must be guarded for rigs where the iGPU has no attached output (fallback to A-present
with a printed note). Honest: DWM pays cross-adapter copies for the NORMAL desktop on that monitor (the
operator accepted this by recabling); our app's present itself is native on G. NO commit — supervisor verifies.

---

## STAGE-33b — overlay mode: present ON the game monitor (the LSFG-style play experience)

*Added 2026-06-10. All testing so far presents on the secondary monitor (capture-A/present-B — chosen
originally to avoid self-capture feedback). The operator now wants the REAL play test: the generated stream
fullscreen ON the 240 Hz main monitor, eyes on it, LSFG-style.*

**Why this needs a stage (two structural facts):**
1. **Self-capture feedback:** monitor capture of output 0 + a topmost present window on output 0 = the capture
   sees our own output → hall-of-mirrors. The fix is STRUCTURAL: capture the GAME WINDOW
   (`--window <substring>`, the `CreateForWindow` path landed in 28b but UNTESTED on the rig) — the overlay
   never appears inside the captured window. GUARD: refuse overlay+monitor-capture of the same output with a
   printed error directing to `--window`.
2. **The overlay must be input-invisible:** today's present window steals focus and eats clicks. Overlay mode
   = `WS_EX_NOACTIVATE | WS_EX_TRANSPARENT | WS_EX_LAYERED` (+ alpha 255) on the borderless-topmost window —
   never activates (the game keeps focus + full-rate rendering), all mouse/keyboard passes through to the
   game beneath. `--overlay` flag, or auto when the chosen present output == the captured window's monitor.

**Present device on the 4090's monitor — measure BOTH:** `--present-gpu primary` (A presents; the present-tax
on the saturated 4090 returns: ~180-240 presents/s × upload+blit) vs `--present-gpu igpu` (G renders its
swapchain; DWM cross-adapter-copies to the 4090's scanout — submits stay off A's 3D queue, the copy cost
lands on DWM). Unknown which taxes the game less — measure game-fps both ways on the rig, set the better as
overlay-auto.

**Also:** BF6 must run borderless (window capture); WGC yellow border already handled (IsBorderRequired
false). Honest expectation to state in the report: lat ~19-21 ms is now IN the play loop (the operator judges
playability; on the secondary screen he called it "mínimo notorio, bastante cómodo").

**Verify (supervisor + operator):** `--window "Battlefield"` captures BF6 (first rig test of the window path);
the overlay covers the monitor, never takes focus, clicks reach the game; no feedback loop; game-fps tax
measured for both present devices; the operator's play verdict at fg 2/3. Both toolchains; protected lines;
NO commit — supervisor verifies.

---

## STAGE-30 — pipelining (the real-cadence unlock; revived by the overlay play test)

*Added 2026-06-10. The §9 overlay test named the blocker: serialized iter 22-25 ms → src consumed ~40-45 of
arr ~55, of a game at ~80 → our real-motion cadence is ~40 Hz vs LSFG's in-chain 80. STAGE-30 (proven as a
3-thread pattern in bench/stage8) overlaps the stages so consumption ≈ arrivals. 32b fix-2 (arr 55→80)
follows it. Combined goal: fg 2 × ~80 src = ~160 at LSFG pair-cadence with the −5 % tax.*

**The split (3 worker threads + the main/pump thread), double-buffered slots:**
- **C (capture+convert):** drain the WGC ring → Astage → iGPU convert+pack submit → hostR/hostRP slot k ready
  (slots ×2; a slot carries its capture timestamp for the lat stat).
- **F (frame-gen):** on a new packed slot: B DMA ingest + block-match + interp(s) → hostI slot(s). Free-runs on
  the freshest pair; never paces.
- **P (present):** the paced loop (anchor + k·T/N, real last — the 29b ordering), presenting from the newest
  COMPLETE slots on G. Owns anchor/EMA/lat/slip.
- **G-queue rule:** convert (C) and present (P) both submit to G — VkQueue submission is externally
  synchronized: ONE mutex around all G submits (convert ~0.5 ms, present ~1 ms; contention acceptable). Same
  rule if any other queue is shared. Per-thread command pools/buffers/fences (pools are NOT thread-safe).
- Main thread keeps pump()/Esc; g_quit joins all three. Teardown: signal → join → existing reverse-order
  destruction (device-idle per device first).
- Stats line unchanged in shape; iter becomes the PRESENT-thread cycle; add `cons X/s` (frames consumed by F)
  so consumption-vs-arr is visible.
- **Also in this pass (one-liner):** overlay mode's `--present-gpu` AUTO default = `igpu` when available — the
  measured winner (lat 21-22 vs 24-25 on the rig).

**Gates (rig, BF6 ~80 fps source, overlay, fg 3):** src/cons ≥ 0.9 × arr (consumption no longer the
constraint); lat ≤ the serialized baseline +2 ms; slip ≤ 2 ms avg; no validation errors; teardown clean
(no hang on Esc/Ctrl+C). Honest: report where the new binding constraint lands (expected: arr ~55 → 32b
fix-2 is next). Both toolchains; protected lines; INVENTORY scoped; NO commit — supervisor verifies.

---

## STAGE-34 — the full combination: DD capture at source rate + capture-proof overlay

*Added 2026-06-10. The operator's DD probe BROKE the capture ceiling: `cons 77-95/s` — Desktop Duplication
delivers the game's full ~80-92 fps where WGC throttles at ~55 (LSFG captures at ~75 for the same reason).
BUT the same run exposed the DD path as half-broken in the 3-thread app: `real=2499 interp=0` — ZERO
interpolated frames (the FG never ran), HDR desktop forced igpu-convert off, and DD feeds no arrival cadence
to the pacing. Fixing these + a capture-proof overlay = the "combinación total": ~80 real + interps at fg 3
= ~240 on the 240 Hz panel with 12 ms FG pairs.*

**34-1 — FG on the DD path (the interp=0 bug):** the 3-thread slot flow must work identically for DD: C fills
the slots from the DD Map (today's DD branch appears to bypass the packed/igpu flow in a way that starves F or
P of interp slots — find the actual break and fix; the gate is `interp ≈ (N−1)×real` in the done line).
**34-2 — DD arrival cadence + stats:** timestamp successful AcquireNextFrame deliveries (the DD "arrival"),
feed the same arrival-delta EMA the WGC path uses (the processed-cadence ratchet is a known runaway), and
print arr/drop for DD.
**34-3 — HDR into the iGPU pack:** the desktop is HDR (DD sees FP16 scRGB). Extend `igpu_convert_pack.comp`
with the is_hdr tone-map (the A-path `hdr_convert` already has the logic) so igpu-convert stays ACTIVE on
HDR captures; fallback to primary-convert remains for other formats.
**34-4 — capture-proof overlay:** `SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE)` on the present
window (Win10 2004+) — the window becomes invisible to DD AND WGC → overlay + monitor-capture on the SAME
output is feedback-free by OS guarantee. Relax the 33b guard accordingly (print the affinity state; keep the
guard if the call fails). This is the standard overlay-app technique.
**Verify (supervisor + operator):** desktop run sane; the operator's BF6 run with
`--capture-api dd --overlay --present-monitor 0`: cons ≈ src ≈ the game's 80-92, `interp=(N−1)×real`,
present ≈ N× source, no hall-of-mirrors, lat reported honestly (DD adds its own copy latency — measure).
Both toolchains; protected lines; INVENTORY scoped; NO commit — supervisor verifies.

---

## STAGE-35 — adaptive generation (the operator's principle: never force N; the assistant's comfortable max)

*Added 2026-06-11. Implements the §9-wrap refinement backlog R1-R3 + the operator's design principle as
`--fg-factor auto`. Context: the transport forensics are closed; B (1080 Ti) runs at ~50 % capacity at fg 3
(5.8-6.4 ms of a ~13 ms budget, cons=arr always) — our "stale content" artifacts were SCHEDULING defects,
not over-generation; but the adaptive policy is the robust fix family AND the right product behavior.*

**35-R1 — pair-span pacing (the walking vibration):** when F's pair spans >1 source frame (skips happen,
drop 2-7/s measured), its interps represent span×T of motion but are paced over 1×T → momentary 2× velocity
then ½×. F already publishes pair identity (34e: f_pair_cseq_a) — ALSO publish the span (cur_c − the
PREVIOUS ingested cseq; F must remember its previous ingest). P paces that set's slots over span×T_ema and
the real at base+span×T_ema (the anchor advance uses span×T too).

**35-R2 — temporal MV smoothing (tile jitter):** damp per-tile MV quantization oscillation across
consecutive pairs. Cheapest correct form: an EMA on the MV field (new small compute pass on B after
hier_match, or fold into the warp's MV read): mv_smooth = α·mv_new + (1−α)·mv_prev, α≈0.6, BYPASS when
|mv_new − mv_prev| > a cut threshold (scene change / direction reversal must not smear). Keep it behind
`--mv-smooth A` (0 = off) so the operator can A/B.

**35-R3 — restore the slip texture:** after 34f's consumed-set skip, P falls through to the next CAPTURE
wake → uneven cycle starts (slip avg 1-2 → 3-7, max 20-34 measured). On skip, P should wait on f_seq (a NEW
set) with a short deadline, not on c_seq.

**35-R4 — `--fg-factor auto` (the operator's principle):** choose N dynamically from MEASURED capacity:
N feasible iff F's set time (ingest + match + (N−1)·warp, EMA-measured per stage) ≤ margin×T_src (margin
~0.75) AND the present budget fits (N presents ≤ T_src with G-submit costs). Re-evaluate on a slow EMA
(seconds, hysteresis — no oscillation). NEVER present a set older than ~1.5×T_src (freshness gate — degrade
N before showing stale content). Stats: print the live N (e.g., `fg auto:3`). Default stays explicit
`--fg-factor 3`; auto is opt-in until the operator validates feel.

**Verify (supervisor + operator):** rig: slip back to ≤2 avg; fg auto picks 3 at 75-80 src (B at ~50 %),
degrades when B is artificially loaded; no stale presents. Operator (the only gauge that matters): the
walking vibration gone or strongly reduced; the A/B of --mv-smooth. Both toolchains; protected lines;
INVENTORY scoped; NO commit — supervisor verifies on the rig.

---

## STAGE-37a kickoff — G queue split (the >100fps-source wall) — tier: OPUS

**Context (read first):** `docs/planning/RENDER_ASSISTANT_PLAN.md` §9 coda + this. The Fortnite run
(src ~105/s) exposed the wall: thread C (WGC copy + iGPU convert) and thread P (up to 270 presents/s)
share **G's single VkQueue** under `g_q_mtx` — their GPU work serializes on one hardware queue, C falls
behind and silently drops WGC frames (ring-newest read, upstream of c_seq → invisible to `skip`), and
auto-N thrashes 1↔2 on the contention feedback loop (the operator's "tirones"). Instrumentation landed
(`f6cf878`): stats fields `cap N/s` (C publish rate; arr−cap = silent drops) and `cw X.Xms` (C's
convert round-trip EMA). The app is C++23 now (`f6cf878`) and consumes phyriad_hal (`b733d48`).

**The change — all in `apps/render_assistant/src/main.cpp` (+ nothing else):**
1. `VDev` (line ~276) gains a second queue: `VkQueue q2; uint32_t qfam2; VkCommandPool pool2;`.
   In `vdev_create` (~277-291), after picking `qfam`: enumerate queue families again and prefer, in
   order: (a) a DISTINCT compute-capable family (COMPUTE set, GRAPHICS clear — AMD async compute);
   (b) a second queue in `qfam` if `queueCount>1` (two VkDeviceQueueCreateInfo entries or queueCount=2);
   (c) fallback `q2=q, qfam2=qfam, pool2=pool` (shared — behavior identical to today). Create `pool2`
   from `qfam2` when distinct (command pools are FAMILY-bound — a cmd buffer recorded from `G.pool`
   CANNOT submit to a different-family q2; this is the trap).
2. Thread C's igpu-convert submit (~1245-1260, the `cmdG`/`fG` block with the `tcv0` STAGE-37 probe):
   allocate `cmdG` from `G.pool2` (at the existing cmdG allocation site), submit to `G.q2`, and drop
   the `g_q_mtx` lock there iff `q2!=q` (C is q2's ONLY submitter → externally synchronized by
   construction; keep the lock when shared-fallback). Keep the `c_conv_us` probe exactly as is — it is
   the before/after evidence.
3. Startup print: `[ra] G queues: convert fam=%u q=%u / present fam=%u q=%u (%s)` with `split-family`,
   `split-queue`, or `shared`.
4. Teardown: destroy `pool2` only when distinct.

**Pinned constraints (every one has burned a delegate before):**
- NO explicit `std::memory_order_*` args anywhere (lint_hal blocks; plain seq_cst defaults only).
- Preserve the file's last line and the CMakeLists' protected last line; CMakeLists likely needs NO edit.
- Both toolchains must build: MinGW `cmake --build build-ra-mingw`; MSVC
  `cmd /c "<vcvars64 VS2022 BuildTools> && cmake --build build-ra-msvc2"`. MSVC-only breaks are the
  recurring delegate failure — build BOTH yourself.
- NO commits. NO INVENTORY edits. Supervisor verifies on the rig and commits.

**Gate:** desktop sanity (`--capture-api wgc --monitor 0 --fg-factor auto --convert-gpu igpu
--no-upscale --fg-gpu assist --present-monitor 1`, ~15s): prints the split mode, `cw` not worse, no
validation errors, present ≈ N×src. The REAL gate is the operator's Fortnite run: `cap` tracking `arr`
(~105) and the tirones gone — supervisor-owned.

## STAGE-37c kickoff — copy-path bench (SlotCopy + hugepages) — tier: SONNET — DISJOINT FILES

**Goal (measure-first, the efficiency mandate):** decide with NUMBERS whether thread C's per-frame CPU
copy (~8.3 MB BGRA 1080p from the WGC D3D11 mapped staging into `Astage.mapped`) and the host-buffer
allocation path are worth migrating to the pillar artifacts (`transport::SlotCopy` SIMD dispatch +
`hal::Allocator` hugepage hints). DO NOT touch `apps/render_assistant/src/main.cpp` — this lane is a
standalone bench; integration is a separate decision made from your table.

**Deliverable:** `apps/render_assistant/bench/copy_bench/` — standalone CMake (pattern:
`framework/render/vulkan/bench/gpu_walls/`, NOT wired into root CMake/CI) with `main.cpp` + tiny
`CMakeLists.txt`. C++23. Include dirs: `framework/hal/include`, `framework/transport/include`; compile
`framework/transport/src/SlotCopy.cpp` directly into the bench (the OpticalFlowPipeline.cpp direct-source
pattern — do NOT link the pillar .a).

**Matrix (median of ≥200 reps each, 8.3 MB = 1920×1080×4 and 6.2 MB = packed RGB8 sizes):**
rows = {std::memcpy, transport SlotCopy (pick_slot_copy() dispatch), row-strided copy loop (RowPitch
2304·4 vs tight 1920·4 — the real WGC Map shape)} × columns = {_aligned_malloc 4KB-aligned buffers,
hal::aligned_alloc_hint with hugepage hint}. Report GB/s + ms/frame at 1080p, and the projected ms/s at
125 frames/s. Print a markdown table to stdout.

**Pinned constraints:** no memory_order_* args; new files end with no protected-line requirement (bench
files are yours) BUT `framework/` files are READ-ONLY for this lane; both toolchains build (MinGW may
lack hugepage path — guard and report "n/a"); NO commits; NO INVENTORY edits (supervisor does both).

**Gate:** the table itself. If SlotCopy or hugepages win <10% on the strided real-shape row, the honest
recommendation is "do not integrate" — say so explicitly.

---

## STAGE-39c kickoff — generation-ring overwrite fix (THE x8 motion-delivery bug) — tier: OPUS

**The bug (forensics complete, checksum-proven):** the F→P interp generation ring (`NS=2`)
assumed P presents each set within 1 source period. STAGE-35-R1 paces a span-S set over S×T,
while F (capture-paced) builds one set per period and wraps the 2-deep ring — **overwriting
the generation P is still presenting** whenever span≥2. Duck-tracker evidence: presented
buffers 4-6 of a span-3 set held the NEXT-NEXT set's early phases (F-side `ducksum` prints
vs P-side dumped BMPs: buffers 1-3 byte-exact match, 4-6 mismatch). At fg8/30fps the B-GPU
set build exceeds 33ms so EVERY set has span>1 → chronic corruption (operator's slow-mo
camera: only 36% unique transitions vs LSFG's 84%). At fg2-4 it fired only on occasional
skips → the never-quite-gone residual tremble.

**State in the working tree (HALF-APPLIED — finish it, do not restart):**
- DONE: `kGenRing=3` constant (file scope, ~line 107) + `hostI`/`hI_a`/`hI_b`/`hI_g`
  declarations now `[kGenRing][kMaxInterp]` (~lines 725-735).
- DONE (keep): the `--dump N` diagnostic (BMP dump in P, gated) + the F-side
  `dbg F warp ... duckdiff/ducksum` prints (gated by `cfg.dump_n>0`).

**Remaining:**
1. `static constexpr int NS=2;` (inside main, ~line 1115) → `static constexpr int NS=kGenRing;`
   (the comment should say: generation ring — see kGenRing rationale).
2. Allocation loop `for(int _g=0;_g<2;++_g)` (hostI alloc, ~line 870) → `_g<kGenRing`; and the
   THREE teardown loops `for(int _g=0;_g<2;++_g)` near the end (hI_g destroy / hI_a+hI_b
   destroy / hostI free) → `_g<kGenRing`.
3. The F-side guard (belt for spans ≥ kGenRing, e.g. stall recovery): new
   `std::atomic<uint64_t> p_presenting{0};` next to the other cross-thread atomics. P stores
   `cur_f` into it right after picking the set to present (after the `f_gen` compute), and F —
   just before building a new set (right after its `f_gen` compute) — waits while
   `p_presenting.load() == f_seq.load()+1-(uint64_t)kGenRing` (Sleep(1) loop with
   g_quit/g_quit_threads checks). Plain seq_cst defaults ONLY (lint_hal).
4. Memory note for the commit: 3 gens × 7 interps × 8.3MB ≈ 174MB host worst case (fg8),
   scaled by NI_alloc as today.

**Gate (the decisive one):** both toolchains build; then run
`--capture-api wgc --monitor 0 --fg-factor 8 --convert-gpu igpu --no-upscale --fg-gpu assist
--present-monitor 1 --dump 18` ~12s and verify the F-printed `ducksum` values MATCH the
P-dumped BMPs' rect sums (RGB sum over x[460,800) y[180,560) + 255×129200 for alpha) for ALL
k of every dumped set — including the TAIL buffers of span>1 sets (the previously-corrupt
ones). A Python one-liner over frames/*.bmp suffices. NO commits — supervisor verifies on
the rig and commits.

---

## STAGE-40a kickoff — overlap B's interp copy-outs (the x8 cadence lever) — tier: OPUS

**Context:** post-39c the CONTENT is correct (15/15 checksum gate) but at fg8/30fps the B set
build = fuse(~7ms) + 6 warps(~2ms) + **7 SERIAL copy-outs Cinterp→hI_b over the x4 (~2.9ms
each ≈ 20ms)** ≈ 39ms > 33ms budget → sets span 5-12 source frames → elastic cadence (the
remaining fluidity gap: operator metric 74.6% unique vs LSFG 84%). The copies are pure
transfer: move them to B's second queue, overlapped with the next warp.

**Pinned design (fence-ordered, NO pillar changes, NO semaphores):**
1. Resources: 7 B-VRAM staging buffers `sbI[kMaxInterp]` (dbuf_create pattern of hRP_b_dev,
   8.3MB each ≈ 58MB VRAM); 7 cmd buffers from `B.pool2` + 7 fences (reset per set).
   GUARD: if `B.q2==B.q` (shared fallback) keep today's serial path entirely.
2. Main-queue (B.q) per warp k: warp k → Cinterp; barrier; **copy Cinterp→sbI[k] (VRAM→VRAM,
   ~0.5ms)**; barrier back; submit+fence-wait as today. The x4 leg leaves this chain.
3. After each fB wait, F submits on B.q2: copy sbI[k]→hI_b[gen][k].buf with fence tfb[k] and
   does NOT wait it inline — CPU fence ordering (submit-after-host-wait) makes the sbI[k]
   content visible to q2 without semaphores. The k=0 copy (inside the fuse cmd today) gets
   the same treatment.
4. At SET END, before `f_pair_*` publication + f_seq bump: wait ALL pending tfb fences (P
   reads hostI after publish — nothing may still be in flight).
5. Keep the 39b/c diagnostics intact (`--dump`, dbg prints, p_presenting guard). The dbg
   ducksum reads hostI AFTER the set publishes — still valid.

**Constraints (each has burned a delegate):** no explicit std::memory_order_* (lint blocks);
protected last lines untouched; both toolchains built BY YOU (MinGW `cmake --build
G:\phyriad\build-ra-mingw`; MSVC vcvars64 VS2022 BuildTools + `cmake --build
G:\phyriad\build-ra-msvc2`); NO commits; NO INVENTORY edits; do NOT delete build dirs.

**Gate:** run `--capture-api wgc --monitor 0 --fg-factor 8 --convert-gpu igpu --no-upscale
--fg-gpu assist --present-monitor 1 --dump 18` ~12s with MOVING content on monitor 0 (spawn
a small animated window if the desktop is static — a bouncing shape suffices). Verify:
(a) F dbg set cseq jumps SHRINK (was c+5..c+12 per set; target c+1..c+2);
(b) the ducksum↔BMP match still holds (15/15-class, rect sum + 32946000 alpha);
(c) cons/s rises toward arr. Report the three honestly — if spans don't shrink, say so.

---

## STAGE-41 kickoff — warp-at-presenter ("ship the recipe, not the cake") — tier: OPUS — NO ON-SCREEN GATES

**Decision basis (measured, `2aca647`):** warp cost 1080p — 4090 0.021ms | 1080 Ti 0.100ms |
iGPU 1.15ms (27.6% @ an even 240/s). Shipping MV+SAD (~0.5MB/set) instead of 7 warped interps
(~58MB/set over B's x4) is compute-viable. Wins: x4 output ÷450, B set ~27→~10ms, EXACT-phase
synthesis per output-clock tick (subsumes the pre-generated grid + its uniq-coverage defect),
and a real input-lag cut (interp exists at match-end).

**Scope — `apps/render_assistant/` only, ALL behind `--warp-at-presenter` (default OFF =
current paths byte-identical). Requires `--output-clock timer|fifo` (error out otherwise).
v1 warper = G (the presenter today); routing to A/B is a later stage.**

1. **F (B-path), WAP mode:** after `record_optical_flow` (the match), SKIP the 7 warps+copies
   entirely. Instead copy B's MV image (`ofp.motion_image()`, RG16F, `motion_width()×motion_height()`)
   AND the SAD image to two new per-gen host bridges `hostMV[kGenRing]`/`hostSAD[kGenRing]`
   (~260KB each; imports on B for the copy-out and on G for the upload). CAUTION: the SAD image
   is pillar-internal — check OpticalFlowPipeline.hpp for an accessor; if none exists, add a
   `sad_image()` const accessor next to `motion_image()` (header-only, additive — the ONE pillar
   touch allowed; keep its protected last line; doc comment in the same style). Images are in
   SHADER_READ_ONLY after the flow → barrier to TRANSFER_SRC → copy → barrier back. Publish
   f_pair_* as today (span/N still travel; N_use is the auto/explicit value for PHASE GRID
   semantics only).
2. **P (output-clock tick), WAP mode:** an app-local warp pipeline on G — new shader
   `shaders/wap_warp.comp` = the pillar's `optical_flow_warp.comp` logic (BOTH gates: confidence
   sad pair + per-tile agreement via shared-memory mean — copy the unconditional-barrier pattern
   EXACTLY, it has a divergence trap documented in the pillar shader) with app bindings:
   sampler2D prevReal, curReal, MV, SAD; rgba8 storage out; push {res_ceil, conf_improv,
   agreement, t}. Per PAIR advance: upload on G — hostMV/hostSAD buffer→image (~0.1ms) and the
   TWO pair reals hR_g[prev_slot]/hR_g[rs] buffer→image (iGPU host BW 73GB/s ≈ 0.23ms each);
   prev_slot derives from pair_c−span through the capture ring (kCapSlots staleness rules apply —
   if the prev real's slot is no longer safe, fall back to freeze-at-cur and count it in a stat).
   Per TICK: ONE warp dispatch at the tick's EXACT fractional t (no grid, no rounding) into a
   G image → present it. Keep the monotonicity guard semantics (t never steps backwards within
   a pair; pair advance only forward).
3. **Stats:** keep the line; in WAP mode `uniq/s` counts distinct (pair,t-quantized-to-0.1ms)
   presents; add `wap` marker to the startup print with the warper device + measured per-warp
   EMA. `--dump` in WAP mode v1: print "dump unsupported in warp-at-presenter v1" and ignore.
4. **NO ON-SCREEN GATE — the operator is using the rig.** Deliverable = implementation + BOTH
   toolchains building clean + lint discipline. The supervisor batch-runs the behavioral gates
   later (uniq ≈ tick rate, content checksums vs F's MV/SAD, iGPU occupancy, lat delta).

**Pinned constraints:** the working tree contains UNCOMMITTED 39f' edits in main.cpp (the
delay-calibrated phase mapping in the output-clock loop) — build on them, do NOT revert them;
no `std::memory_order_*`; protected last lines (main.cpp, the pillar header if touched, shaders
end with the soul line ONLY where already present — your new shader does NOT add one); no
commits; no INVENTORY edits; both toolchains built by you; do NOT run the app.

---

## STAGE-44a kickoff — presentation-probe bench (the 4 empirical gaps) — tier: OPUS — on-screen ALLOWED

**Context (read FIRST):** `docs/planning/PRESENTATION_LAYER_PRIOR_ART.md` — the verified
dossier. This probe measures, on the target rig, the gaps no primary source answers (§8):
(1) does `LAYERED|TRANSPARENT` + `NOREDIRECTIONBITMAP`+composition-swapchain keep flip-grade
presentation? (2) does `WDA_EXCLUDEFROMCAPTURE` work on such windows (and does it cost the
presentation grade)? (4-partial) present pacing under each mode. Gap (3) (game+overlay
concurrent planes) needs the operator's real game — OUT of this probe's scope; the probe
must leave a `--style`-selectable overlay running so the operator session can measure (3)
later with the same tool.

**Deliverable:** `framework/render/vulkan/bench/stage44a_present_probe/` — standalone CMake
(stage3_capture pattern: raw Win32 + D3D11 + DComp, no phyriad deps; MSVC NOT required —
plain D3D11/DXGI/DComp compiles on MinGW; if dcomp.h is missing on MinGW, fall back to MSVC
vcvars and SAY SO). One exe, CLI:
  `--style baseline|dcomp|dcomp-ct` (+`--wda`) `--monitor N` `--seconds S` `--rate HZ`
- **baseline**: WS_POPUP + TOPMOST|NOACTIVATE, non-layered, classic HWND flip-model DXGI
  swapchain (FLIP_DISCARD) — the current render_assistant shape (the control).
- **dcomp**: WS_EX_NOREDIRECTIONBITMAP window + `CreateSwapChainForComposition`
  (FLIP_SEQUENTIAL, premultiplied alpha) + IDCompositionDevice/target/visual, Commit ONCE.
- **dcomp-ct**: dcomp + `WS_EX_LAYERED|WS_EX_TRANSPARENT` (THE gap-1 combination).
- `--wda`: apply `SetWindowDisplayAffinity(WDA_EXCLUDEFROMCAPTURE)` after creation.
Content: a moving vertical bar + frame counter text-free (pixel-only), presented at --rate
(default 240) with sleep+spin pacing; window 1920x1080 borderless on the chosen monitor;
alpha 50% background in dcomp modes (the alpha-overlay case from the dossier §3.4).

**The probe AUTOMATES three measurements per config and prints a verdict table:**
1. **Present mode**: spawn `F:\PresetMon\PresentMon.exe` (v2.4.1 — run `--help` FIRST and
   adapt args; Ayama's `apps/ayama/tools/ayama-ui/PresentMonSpawner.hpp` is prior art for
   spawning v1-style; v2 flags may differ) targeting the probe's own process for ~5s; parse
   the CSV PresentMode column; report the dominant mode (e.g. "Hardware: Independent Flip" /
   "Hardware Composed: Independent Flip" / "Composed: Flip") + present rate + p95 frame time.
2. **Click-through**: BEFORE the overlay, create a witness window (normal, 400x300, center
   of the overlay's monitor, with a WM_LBUTTONDOWN counter). After the overlay is up,
   SendInput one absolute-coordinate click at the witness center; report whether the witness
   received it (YES = clicks pass through the overlay).
3. **Capture exclusion**: run a short WGC monitor capture of the overlay's monitor (prior
   art: `bench/stage28_wgc` — MSVC/WinRT; if the probe is MinGW, do this check with DXGI
   Desktop Duplication instead, stage3_capture pattern) and test whether the overlay's
   moving bar appears in captured pixels (sample the bar's expected positions across 2
   frames; moving = visible = WDA FAILED; static/absent = excluded).

**Gate (run it yourself — the operator is away; monitor 0 is free):** the full matrix
{baseline, dcomp, dcomp-ct} × {wda off, on} on monitor 0, table to stdout. Honesty rules:
report the PresentMon mode VERBATIM per config; if dcomp-ct loses flip-grade (composed) or
WDA kills promotion, that IS the result — no massaging. If PresentMon v2 cannot attribute
the probe's presents (composition-swapchain presents may attribute to DWM), report that
limitation explicitly and fall back to reporting the OVERLAY's effect on a flip-model
HELPER window's mode (spawn the baseline-style window as the "game proxy" behind, measure
ITS mode with the overlay above — labeled clearly as a proxy, NOT the real gap-3).

**Pinned constraints:** new files only (your bench dir); framework/ read-only; no commits;
no INVENTORY edits; no memory_order_*; build whatever toolchain works and SAY which; kill
every window/process you spawn (incl. PresentMon — it needs admin? if PresentMon fails for
privileges, report the exact error; do NOT attempt elevation).

---

## STAGE-45 kickoff — consumer migration: PresentSurface (FR-RENDER-1 §1.9) — tier: OPUS — NO ON-SCREEN GATES

**Decision basis (measured, gap-3 2026-06-11, dossier §10):** our CURRENT overlay window
(baseline-style, non-layered) knocks the GAME down to `Composed: Flip` (−41 fps, 285→244);
the pillar's `dcomp-ct + WDA` recipe keeps the game at `Hardware: Independent Flip` at
control-level fps AND solves click-through + cursor + capture-proof permanently (the 39h
self-coupling class dies by construction). Migration = UX fix + performance gift.

**Context (read FIRST):** `docs/canon/FR_RENDER_IMPL.md §1.9` (the migration contract),
`docs/reference/present.md` (the pillar API — note the THREADING CONTRACT: `create()` and
`submit()` on the SAME thread), `framework/render/present/` (read-only).

**Scope — `apps/render_assistant/` only, ALL behind `--present-surface` (default OFF =
current paths byte-identical). v1 support matrix: requires `--output-clock timer` (fifo's
clock IS the legacy swapchain — error out with a clear print); `--igpu-present` without
WAP + surface → error out (drop one). Primary target: WAP + timer + surface.**

1. **CMake:** consume the pillar by direct source inclusion (the OpticalFlowPipeline.cpp
   pattern): add `framework/render/present/src/PresentSurface.cpp` to the executable,
   `framework/render/present/include` + `framework/schema/include` to includes, `dcomp` to
   links, and `WINVER=0x0A00 _WIN32_WINNT=0x0A00` defines (the pillar needs them; it
   builds clean on MinGW — verified in build-present). While there, fix the stale NOTE
   ("this app is C++17") — the app is C++23 since STAGE-37.
2. **Surface lifecycle (P thread):** when active, do NOT create the legacy overlay window
   at all (skip make_window/WDA/cursor/HTTRANSPARENT logic — PresentSurface owns its HWND
   and pumps its own messages inside submit()). Create the surface IN P's setup
   (threading contract), `PresentSurfaceDesc{ monitor_index = the overlay monitor, 0, 0 }`
   (defaults = DcompCt + ExcludeFromCapture + Immediate). `create()` failure → print +
   auto-fallback to the legacy path (D-20), not an exit.
3. **Bridge (the §7 producer):** one D3D11 device on the default adapter (the 4090 — the
   panel owner, same adapter the surface picks) + one shared B8G8R8A8 texture at monitor
   extent, `MISC_SHARED_NTHANDLE|SHARED_KEYEDMUTEX`, `CreateSharedHandle` → NT handle.
   Import into VK-A as an image (`VkExternalMemoryImageCreateInfo` +
   `VkImportMemoryWin32HandleInfoKHR`, dedicated alloc); keyed-mutex sync from the VK side
   via `VK_KHR_win32_keyed_mutex` (`VkWin32KeyedMutexAcquireReleaseInfoKHR`, acquire/release
   key 0, chained on the bridge-blit submit). VERIFY the extension is exposed by A
   (vkEnumerateDeviceExtensionProperties; NVIDIA exposes it) — if absent, fall back to a
   no-KM shared texture (drop SHARED_KEYEDMUTEX; the pillar skips AcquireSync when the
   producer has no KM) + CPU-fence ordering (P waits the blit fence before submit() —
   same thread, ordering holds) and SAY SO in the delivery report.
4. **Routing, non-WAP A-present:** where `do_present_P(src)` runs today, replace (surface
   mode) with: A blit `src` → bridge image (KM acquire/release on that submit) → fence →
   `surface.submit({nt, 0, W, H})`. `Timeout` = skipped present, count it in a stat.
5. **Routing, WAP (supervisor's call — A re-warps at the tick):** G's warp stays legacy-only.
   Surface mode: instantiate the WAP pipe on **A** (same `wap_warp.comp` SPIR-V), import
   hostMV/hostSAD on A (NEW imports — EMH sizes are already align-rounded, the STAGE-41
   lesson: import with the ROUNDED size), upload per PAIR advance on A (MV/SAD ~0.1ms +
   the two pair reals from hR host bridges ≈0.3ms each over x16; ~30/s → ~2% of A's copy
   engine; use A's transfer queue if one exists, else A.q), warp per TICK at the exact
   fractional t directly into the bridge image (measured 0.021ms on the 4090 — cheaper
   than G-warp + a host bounce ~0.25ms, drops G from the present path, lowest latency),
   then submit. Keep the freeze-at-cur fallback + monotonicity guards semantics identical
   to the G path.
6. **Stats/UX:** startup print `[ra] present: PresentSurface dcomp-ct+WDA (FR-RENDER-1) —
   click_through=%s capture_excluded=%s`; stats line gains `ps` marker + submit
   ok/timeout/err counters + per-warp EMA on A (WAP mode).
7. **NO ON-SCREEN GATE — the operator is using the rig.** Deliverable = implementation +
   BOTH toolchains building clean (MinGW `cmake --build G:\phyriad\build-ra-mingw`; MSVC
   vcvars64 VS2022 BuildTools + `cmake --build G:\phyriad\build-ra-msvc2`) + a delivery
   report (ext-presence findings, fallbacks taken, exact files/lines). The supervisor
   batch-runs behavioral gates with controlled motion (uniq/s ≈ tick rate, PresentMon
   `Composed: Flip` on our PID at ~tick rate, submit errors 0); the operator eye-tests on
   a real game; the §1.9 clean break (delete legacy + flip the default) is STAGE-45b,
   AFTER that eye test.

**Pinned constraints:** `framework/` READ-ONLY (consume the pillar via its public header —
if you find a pillar gap, REPORT it, do not patch it); build on committed HEAD (the tree is
clean); no `std::memory_order_*` in the app; protected soul lines (main.cpp + CMakeLists
end with the soul line — preserve them; new shaders: none expected); no commits; no
INVENTORY edits (no files added/removed); do NOT delete build dirs; do NOT run the app.

---

## STAGE-46 kickoff — soft-gate warp (judder↔hallucination both, not either) — tier: OPUS — NO ON-SCREEN GATES

**Decision basis (measured, 2026-06-11, operator camera + lag-8 autocorr):** the WAP gates are
BINARY per 8×8 block. Default (0.50/0.20) → motion pulses at the source rate (lag-8 autocorr
peak +0.25 over baseline = the operator's "30fps-ish"). Loosened (0.20/0.05) → the lag-8 peak
dies (+0.02) BUT isolated wrong-MV blocks stamp whole (operator-photographed block
hallucinations at disocclusion contours). A binary threshold cannot win both; a continuous
blend can.

**Scope — `apps/render_assistant/shaders/wap_warp.comp` + its push constants in main.cpp,
ALL behind `--soft-gate` (default OFF = current binary gates byte-identical).**

1. **Soft confidence weight:** today the two gates (sad-pair confidence improvement; per-tile
   agreement via the shared-memory mean) produce a binary keep/freeze. Soft mode: map each
   gate's MARGIN through a smoothstep centred on its threshold (reuse conf_improv/agreement
   push values as midpoints; width = half the midpoint, clamped sane) → w_conf, w_agree;
   final w = w_conf * w_agree; out = mix(frozen_sample, warped_sample, w).
2. **Isolated-block suppression:** the shared-memory tile already aggregates neighbours.
   Add a neighbour-agreement factor: a block whose MV disagrees with ALL its tile
   neighbours (use the existing agreement statistic — do NOT add new shared arrays) gets
   w *= that factor — an isolated bad block fades toward freeze instead of stamping.
   This kills the operator's stray-duck-block class specifically.
3. **CRITICAL — the divergence trap:** the pillar-derived shader uses UNCONDITIONAL
   barrier()s (documented in the shader); ALL barrier() calls MUST remain unconditional —
   compute weights per-thread AFTER the barriers, never branch around a barrier. Copy the
   existing pattern exactly.
4. **Plumbing:** one new push-constant float/flag (soft on/off — pack into the existing
   push block, do not grow past 128 bytes); `--soft-gate` in parse_args + help + the FG
   startup print gains `soft` marker. BOTH warp instances (G legacy WapPipe + A surface
   wapPipeA) consume the same SPIR-V — one shader change serves both; verify both pipe
   creation paths still pass the same push layout.
5. **NO ON-SCREEN GATES — build-only.** Both toolchains green (MinGW build-ra-mingw;
   MSVC vcvars64 VS2022 BuildTools build-ra-msvc2 — glslc recompiles the shader at build).
   The supervisor gates with controlled motion; the operator's camera A/B (lag-8 +
   moved-fraction + eye hallucination check at 0.20/0.05+soft) is the acceptance.

**Pinned constraints:** framework/ READ-ONLY; no commits; no INVENTORY edits (no new files
— the shader exists); preserve soul lines (main.cpp; wap_warp.comp has NO soul line — do
not add one); no memory_order_*; do NOT run the app; do NOT delete build dirs.

---

## STAGE-45b kickoff — legacy-present deletion (the §1.9 clean break) — tier: OPUS — NO ON-SCREEN GATES

**Maintainer go: 2026-06-12 ("si borra legacy y documenta lo que creas importante sobre el").**
PresentSurface becomes THE present path. Default flow = WAP + output-clock timer + surface.

### Obituary — what the legacy present path was, and what it taught (READ, then delete)
The legacy path (STAGE-25→44, dies at the commit that lands this kickoff; recover via git
history at the parent of that commit):
- **A borderless Win32 HWND + VkSwapchainKHR** (`Swap`, make_swapchain, MAILBOX/IMMEDIATE
  pick — the STAGE-27a acquire-stall lesson) on A, or on **G** (`igpu-present`, STAGE-33's
  recabled-monitor era: present from the adapter that owns the panel; G-present measured as
  overlay winner in STAGE-30 §9). The presenter-owns-the-panel PRINCIPLE survives — it now
  lives in the pillar (the bridge is on the panel-owning adapter by construction).
- **The overlay war** it fought and lost: WS_EX_LAYERED|TRANSPARENT = input-invisible but
  CPU-composited (33b); non-layered = flip-capable but hit-tests solid (34c); per-mode
  toggling self-couples on cursor ownership (39h/39i). The war's resolution is FR-RENDER-1
  (dcomp-ct + WDA, trilemma dead by construction) — and gap-3 proved the legacy window was
  ALSO knocking games out of independent flip (−41 fps, Composed: Flip) its whole life.
- **OC_FIFO**: the output clock derived from the G-swapchain's vblank-paced FIFO present —
  honest hardware pacing, but it WAS the legacy swapchain; timer mode (39d/e + 37b CpuWait
  spin-finish, slip ~0.0-0.2ms measured) replaced it in practice. Dies with the swapchain.
- **Non-clock cadence (OC_OFF)**: present-per-generation, the original STAGE-25/29/30 flow.
  Superseded by the output clock (the panel is the cadence, 39d/e).
- **WAP-on-G**: STAGE-41's v1 warper (G re-warps at tick). Replaced by WAP-on-A (STAGE-45,
  measured 0.021ms warp, no host bounce, G out of the present path). G keeps its REAL niche:
  igpu-convert (fused convert+pack, CPU-proximity) — contextual value, not presentation.

### Scope — `apps/render_assistant/` only
1. **Delete**: legacy HWND path (make_window + pump + WDA call + cursor/HTTRANSPARENT wndproc
   logic + g_overlay_hide_cursor), VkSurfaceKHR, `Swap` struct + make_swapchain/swap_create/
   swap_destroy + sc/sc_g + sc_mode/sc_mode_g + their semaphores/cmd buffers where
   present-only, do_present_P's legacy body, do_present_g_P, WAP-on-G (G WapPipe instance,
   wapPrev/Cur/MV/SAD/Out G images, hMV_g/hSAD_g imports, the G branches of wap_upload/
   wap_warp_present, fifo_g), use_igpu_present/want_g_present routing, OC_OFF + OC_FIFO
   modes (the whole non-clock present block + fifo pacing/stat branches; `use_fifo_pace`,
   vblank/period stat labels), the same-monitor feedback NOTE (WDA is unconditional now).
2. **Defaults/flags (operator-UX soft landing)**: `present_surface` ceases to exist as state
   (the path is unconditional); `--present-surface` accepted + prints "default since
   STAGE-45b"; `--overlay` accepted + prints "the surface is always an input-invisible
   overlay"; `--present-gpu igpu/auto` prints "present is the panel-owning adapter since
   STAGE-45b (G keeps convert)" + ignored; `--output-clock` default becomes **timer**;
   `off`/`fifo` → clear error citing this stage; `--windowed` → clear error (debug window
   died with the legacy path).
3. **Keep**: C/F threads untouched; EMH mailboxes; WAP-on-A; the bridge + PresentSurface
   (P-thread, threading contract); grid mode under timer+surface (bridge_present_src);
   `--dump` (grid mode); adaptive R4b; igpu-convert; stats (minus fifo branches — `ps`
   counters stay). **Upscale machinery stays compiled but remains auto-forced-off**
   (unreachable matrix) with a comment: the bridge blit already scales (FILTER_LINEAR,
   work→monitor extent) — re-enabling upscale THROUGH the bridge is the future stage, do
   not delete the pipes.
4. **Quit path**: the legacy window owned Esc. Add `SetConsoleCtrlHandler` → set the
   existing quit flags so Ctrl+C exits CLEANLY (today it force-kills mid-frame); update the
   "running — press Esc" print to "running — Ctrl+C to quit".
5. **Gates**: BOTH toolchains green (MinGW build-ra-mingw, MSVC vcvars64 build-ra-msvc2);
   `--help` reflects reality; a default-flag run (`--window X --fg-factor 8`) must reach the
   surface path with NO extra flags (timer default). NO on-screen runs — supervisor gates.

**Pinned constraints:** framework/ READ-ONLY; no commits; no INVENTORY edits; preserve the
main.cpp soul line; no memory_order_*; do NOT run the app; do NOT delete build dirs. The
deletion should be a NET-NEGATIVE diff (target: several hundred lines removed); if something
resists deletion because a kept feature secretly depends on it, STOP and report rather than
keeping zombie code alive.

---

## STAGE-42 kickoff — temporal MV prior (dual-centre coarsest search) — tier: OPUS — PILLAR TOUCH — NO ON-SCREEN GATES

**Decision basis (measured, 2026-06-11/12):** after STAGE-46, coverage is the binding
constraint (moved-fraction p50 0.077 vs LSFG 0.102; operator: fast-motion artifacts "esperado
por la baja tasa de frames reales"). At 30 fps source the per-pair displacement is large; the
hier matcher starts every pair FROM ZERO at the coarsest level (`optical_flow_hier_match.comp`
line ~7: zero predictor + pred_scale=0), so fast motion beyond the ±R_coarse window is simply
unreachable. Game motion is temporally coherent — the previous pair's coarse MV is an
excellent search seed. This is the real fluidity lever left.

**Design — dual-centre, self-healing (NO scene-cut detector needed):** the matcher searches
±R around `centre = round(texture(u_predictor)*pred_scale)` with a small predictor-bias
(kLambda). A poisoned prior (scene cut) would centre the window WRONG. v1 therefore evaluates
BOTH centres at the coarsest level only — zero AND the temporal prior — and keeps the
lower-cost winner: on a cut, the zero centre wins by construction; on coherent motion, the
prior centre reaches displacements the zero window never could. The coarsest level is the
smallest dispatch (grid/2^(L-1)); doubling its window cost is noise.

**Scope:**
1. **Pillar (`framework/render/vulkan/`, ADDITIVE, default OFF = byte-identical):**
   - `OpticalFlowPipeline.hpp/.cpp`: a `set_temporal_prior(bool) noexcept` (or equivalent
     opt-in param); persist the previous pair's coarsest-level MV — at record start (before
     the new coarsest match overwrites `mvl_img_[n_levels_-1]`), copy it into the existing
     `zero_mv_img_` slot (rename to `prior_mv_img_` if that reads honest — it serves both
     roles: cleared-to-zero when the prior is off OR on the FIRST pair; copied-from-last when
     on). Layout choreography mirrors the existing clear path (TRANSFER_DST → sampled).
   - `optical_flow_hier_match.comp`: push gains `int dual_centre` (coarsest sets it 1 when
     the prior is armed; finer levels keep 0 — their predictor is the upsampled coarser MV,
     unchanged). When dual: evaluate the ±R window around BOTH centres (0 and the prior),
     pick the lower cost. Keep kLambda bias semantics per-centre; ties resolve as today.
   - **Pillar-modification protocol applies** (AGENT_PROTOCOL): additive, default-off,
     doc-sync — update the pillar's `docs/reference/` page in the SAME change (check
     `scripts/check_doc_sync.sh` mapping for framework/render/vulkan headers); preserve the
     header's protected last line.
2. **App (`apps/render_assistant/src/main.cpp`):** `--mv-prior` flag (default OFF) →
   arm it on B's OFP instance (and A's pfg instance where created, for symmetry); `[ra] FG:`
   print gains `mv-prior` marker.
3. **Gates:** both toolchains green; pillar's existing OFP consumers (benches) still build.
   Acceptance = supervisor controlled-motion gate (fast-moving source: moved-fraction must
   RISE vs prior-off at identical scene) + the operator camera A/B (fast-motion scene,
   lag-8 + moved-fraction + eye) at `0.20/0.05 --soft-gate --mv-prior`.

**Pinned constraints:** pillar touch is EXACTLY the two files + shader above (additive,
default-off); no commits; no INVENTORY edits (no new files); preserve protected last lines
(pillar header + main.cpp; the pillar shader keeps its existing trailer if any); no
memory_order_*; do NOT run the app; do NOT delete build dirs. The OFP change must leave
EVERY existing consumer byte-identical with the prior off — that is the pillar bar.

---

## STAGE-47 kickoff — commit-don't-hedge warp (the ghost-eye fix) — tier: OPUS — NO ON-SCREEN GATES

**Decision basis (operator-discovered, 2026-06-12, photographic):** at 120fps x2 the rotating
duck shows a DOUBLE EYE — the new frame contaminated by the previous frame's eye at its old
position. Root cause = our two AVERAGING layers: (1) `warp_result = (1-t)*A_samp + t*B_samp`
shows BOTH samples whenever the block MV is wrong for a sub-block feature (small high-contrast
features inside an 8x8 block whose MV the surrounding body dominates); (2) the soft-gate
fallback `blend_result = (1-t)*prev + t*cur` is a static cross-fade = a double exposure of
moving content by construction. Consequences the operator nailed: ghosts inflate the
moved/uniq camera metrics (counted as motion, add no real fluidity) and explain the
sensation gap vs LSFG — they COMMIT to one coherent image per frame; we hedge by averaging.

**Scope — `apps/render_assistant/shaders/wap_warp.comp` + push/flag plumbing in main.cpp,
ALL behind `--commit-warp` (default OFF = current behavior byte-identical).**
1. **Warp agreement-commit:** after computing A_samp/B_samp (both already exist), compute
   per-pixel disagreement `d_ab` = max-channel abs diff (or luminance — say which and why).
   `d_ab <= commit_thresh` → keep the existing average (where the MV is right, averaging
   only sharpens). `d_ab > commit_thresh` → the MV is wrong for THIS pixel: commit to the
   temporally NEAREST sample — `t < 0.5 ? A_samp : B_samp` (tie at exactly 0.5 → B/cur,
   forward-consistent, no flicker).
2. **Fallback nearest-real:** in commit mode, `blend_result` becomes the nearest real
   sample — `t < 0.5 ? texture(prev) : texture(cur)` (tie → cur). Identical for static
   content; kills the old-frame double exposure for moving gated content (the honest cost:
   gated moving regions step at source cadence instead of ghosting — that IS what a
   fallback should look like).
3. **Plumbing:** push gains `float commit_thresh` (0 = off/hard-identical; flag sets the
   default 0.05 in normalized [0,1] color units — also expose `--commit-thresh F` for
   tuning). Both dispatch sites + wap_create pcr.size grow together (20→24B). `--commit-warp`
   flag + help + `commit` marker in the FG print. The barrier discipline is unchanged — all
   new math is per-thread post-barrier; keep every barrier() unconditional.
4. **Gates:** both toolchains green (glslc recompiles); supervisor controlled-motion run
   (er=0 both modes); acceptance = the operator's duck: the double eye must DIE in commit
   mode at 120fps x2, then the 30fps x8 regression (the ghost class should also shrink
   there — it was the "30fps-ish" smear).

**Pinned constraints:** framework/ READ-ONLY; no commits; no INVENTORY edits; preserve the
main.cpp soul line (the shader has none — do not add); no memory_order_*; do NOT run the
app; do NOT delete build dirs.

---

## STAGE-48 kickoff — bidirectional flow + occlusion masks + one-sided sampling — tier: OPUS — NO ON-SCREEN GATES

**Strategic basis (operator go, 2026-06-12):** the REAL-content approach peaked at 47c (eye
verdict: "más pulida; el tope de este enfoque"). Entering LSFG's terrain by the correct door
= the three-rung ladder: (1) finer flow, (2) OCCLUSION REASONING ← this stage, (3) masked
disocclusion fill (the first honest lie, STAGE-50). Today A's warp approximates backward
flow by negating the forward field — disocclusions ghost because the SAME field serves both
directions; d_ab/commit is a blind proxy. With true backward flow + fwd-bwd consistency,
each pixel is CLASSIFIED: visible-in-both → average (sharp); visible-one-side → sample THE
side where the content exists (one-sided; the correct decision, not the nearest); truly
inconsistent → the existing 47b/c commit fallback (safety net, unchanged).

**Scope — apps/render_assistant only (the pillar is input-agnostic: backward flow =
record_optical_flow with swapped views). ALL behind `--bidir` (default OFF byte-identical).**
1. **F (B-path):** per pair advance, record TWO flows: fwd (prev,cur — as today, copy MV+SAD
   out to hostMV/hostSAD) then bwd (cur,prev — copy MV out to NEW hostMVB[kGenRing] bridges;
   SAD-bwd not shipped v1). The second record overwrites the OFP's internal MV/SAD images —
   strictly copy-out fwd BEFORE recording bwd. Bridge sizes: same mvw*mvh RG16F, same
   align-rounded hfb discipline (import on B write-side + A read-side).
   **mv-prior interaction:** alternating fwd/bwd records would poison the temporal prior
   (the "previous coarsest MV" alternates direction). v1: `--bidir` + `--mv-prior` → ignore
   mv-prior with a print.
2. **A (WAP warp):** wap pipe gains a 6th sampled binding (MV_bwd; upload alongside MV/SAD
   per pair). Shader (`wap_warp.comp`): per output pixel compute fwd-bwd consistency
   (sample MV_f here, follow it, sample MV_b there, error = |MV_f + MV_b_followed| in pixel
   units; threshold push `occl_thresh`, default ~1.5 block-units — expose --occl-thresh).
   Classification: consistent → existing average path (incl. 47c safety net downstream);
   inconsistent → determine WHICH side owns the content (the side whose own flow round-trips
   better; if fwd round-trips and bwd does not → content exists in prev → sample A side
   only; vice versa → B side only; neither → fall through to the 47b/c commit machinery
   UNCHANGED). All new math per-thread, post-barrier; every barrier() stays unconditional.
   Push grows accordingly (28→32B: occl_thresh; bidir presence = occl_thresh>0, same
   gating discipline as commit_thresh — 0 byte-identical).
3. **Budget/stats:** B now does 2 match+pyramid passes per pair (~12-16ms at 1080p) — fine
   at ≤60fps source; at higher rates the span machinery absorbs (R4b) but PRINT the pair
   EMA so the operator sees it; add `bidir` FG marker. If B's pair time makes fg8@30
   regress (cons<arr sustained), report — do not silently half-res.
4. **Gates:** both toolchains; controlled-motion run er=0 both modes (--bidir on/off);
   acceptance = the operator's duck at 120 x2 (contours/disocclusion rim must clean up vs
   47c) then 30 x8.

**Pinned constraints:** framework/ READ-ONLY (the pillar needs NO change — if you find it
does, STOP and report); no commits; no INVENTORY edits; preserve the main.cpp soul line;
shader has no soul line; no memory_order_*; do NOT run the app; do NOT delete build dirs;
kill any process you spawn.

---

## STAGE-50 kickoff — divergence-directed disocclusion pick (the honest rung-3) — tier: OPUS — NO ON-SCREEN GATES

**Decision basis (operator slow-mo, 2026-06-12, post-48):** the residual is a SMALL CONSTANT
trail of speckle pixels behind moving objects — the "neither round-trips" class (genuine
disocclusion slivers) falling to commit-real = nearest real IN TIME, which for a
disocclusion is the WRONG side half the ticks (revealed background exists ALWAYS in cur;
occluded background ALWAYS in prev). Physics picks the side: the forward-MV field DIVERGES
where content is being revealed (expansion) and CONVERGES where it is being occluded. Still
100% real content — only the selection criterion changes in the ni-ni sliver. Invented fill
(stretch/inpaint) stays OUT of this stage; only if residue survives.

**Scope — `apps/render_assistant/shaders/wap_warp.comp` only (+ flag plumbing in main.cpp),
behind `--fill-div` (default OFF byte-identical; requires --bidir, error otherwise).**
1. In the STAGE-48 classification, the "neither round-trips" case currently falls through
   to 47b/c. Under fill-div: compute the local divergence of MV_fwd at this grid uv
   (central differences over ±1 MV-grid texel: div = d(mv.x)/dx + d(mv.y)/dy, texel step
   derived from the MV texture size — query textureSize, do NOT hardcode the grid dims).
   div > +div_eps → revealed → warp_result = B_samp, blend_result = texture(cur, uv);
   div < -div_eps → occluded → A_samp / texture(prev, uv);
   |div| <= div_eps (ambiguous) → fall through to 47b/c as today.
   div_eps push-tunable (`--div-eps`, default 0.05 px/texel, clamp [0.005, 1.0]).
2. Push 32→36B (`float div_eps`; 0 = off, the uniform-gating discipline). FG print gains
   `fill-div` marker. Help entries style-matched.
3. All per-thread, post-barrier; every barrier() unconditional. The added cost is 4 extra
   MV texture taps ONLY inside the neither-class branch (already rare).
4. Gates: both toolchains; controlled-motion run er=0 on/off; acceptance = the operator's
   duck trail (the constant speckle estela must shrink/die), 120 x2 then 30 x8.

**Pinned constraints:** framework/ READ-ONLY; only the shader + main.cpp; no commits; no
INVENTORY edits; no memory_order_*; do NOT run the app; preserve the main.cpp soul line
(shader has none); do NOT delete build dirs; kill anything you spawn.

---

## STAGE-49 kickoff — candidate-rescue + mask-weighted occlusion (research-directed) — tier: OPUS — NO ON-SCREEN GATES

**Decision basis:** `docs/planning/VFI_REFINEMENT_PRIOR_ART.md` (106-agent verified dossier).
Two research-directed refinements, both in the warp shader only:

1. **Candidate-rescue (F1/F0 — the production sub-block pattern, `--rescue`):** a pixel
   whose primary MV fails (the d_ab commit trigger) tries the 8 NEIGHBOR block MVs (3x3
   around its MV-grid cell, fwd field) as candidates BEFORE falling to commit: for each
   candidate mv_c, resample A_samp/B_samp with mv_c and score by the same max-channel
   d_ab; if the best candidate scores <= commit_thresh, USE it (warp_result = its average;
   the small feature found its true vector — per-pixel MV by SELECTION, Qualcomm
   US8537283's pattern on our textures). Cost confined to the failing branch (<=8
   candidates x 2 taps + the MV taps, only where d_ab already failed). Order: rescue
   FIRST; only unrescued pixels proceed to the 47b/c commit.
2. **Mask-weighted one-sided blend (F6 — FSR3's shipping practice, replaces the hard pick
   under `--bidir`, no new flag):** STAGE-48's hard side pick becomes continuous: weight
   w_side = smoothstep over the round-trip error RATIO (e.g. r = e_loser/(e_winner+eps),
   full one-sided only when r >> 1; near-tie blends both sides). Keep the same
   classification; soften only the application. The 47c band discipline applies.

**Plumbing:** `--rescue` (default OFF byte-identical; requires --commit-warp since the
trigger is d_ab — error otherwise); push 36→40B (`float rescue_on`; uniform-gated). The
48 weighting change is behavior-affecting under --bidir: gate it on the SAME occl_thresh
push (no new flag) but document in the header that 49 changed the application from hard
pick to weighted — the operator A/Bs via build version, the off-path (no --bidir) is
untouched. All math per-thread post-barrier; the ONE barrier() stays unconditional.

**Gates:** both toolchains; controlled-motion er=0 (rescue on/off); acceptance = operator:
(a) duck-eye class at low thresh WITHOUT commit speckle (the rescue should recover detail
the commit was dropping), (b) disocclusion rims smoother than the hard pick (48b).

**Pinned constraints:** shader + main.cpp only; framework/ READ-ONLY; no commits; no
INVENTORY edits; no memory_order_*; do NOT run the app; preserve the main.cpp soul line
(shader has none); do NOT delete build dirs; kill anything you spawn.

---

## STAGE-49b kickoff — MV vector-median + windowed rescue (the flat-content blind spot) — tier: OPUS — NO ON-SCREEN GATES

**Decision basis (operator photo, 2026-06-12, the synthetic ball fullscreen):** block-sized
HOLES inside the moving flat-color object and detached block STAMPS outside it, at any
speed. Mechanism: flat content defeats photometric verification BY CONSTRUCTION — a wrong
MV landing both samples on identical wrong content (yellow-yellow / bg-bg) passes d_ab
with ~0, and the bwd matcher fails identically on the same flat content so the round-trip
"closes". Dossier F2 predicted it verbatim ("SAD fails at low local variation"). The
artifacts are ISOLATED bad vectors at the rim; the production cures are field-level
consensus + windowed scoring (F9 vector-median; F1 small-window SAD).

**Scope — apps/render_assistant only, two pieces:**
1. **A-side 3x3 vector-median on the uploaded MV fields (`--mv-median`, default OFF):**
   a tiny app-local compute pass (new shader `mv_median.comp`, mvw x mvh grid) run after
   the per-pair upload in wap_upload, filtering wapMVA (and wapMVBA when bidir) in place
   (ping-pong via one scratch RG16F image or read-into-scratch-write-back — your call, say
   which). Component-wise median of the 3x3 neighborhood (the standard cheap vector-median
   approximation; a true vector median by L1 distance is also acceptable if compact).
   Edge texels clamp. Cost ~mvw*mvh*9 taps, negligible. This kills isolated wrong vectors
   (the stamps and rim holes) by neighborhood consensus — content-independent.
2. **Windowed rescue scoring (upgrade inside `--rescue`, no new flag):** the candidate
   score becomes a 3x3 small-window SAD (sample A_c/B_c at the pixel and its 8 immediate
   FULL-RES neighbors, sum the max-channel diffs) instead of the single-pixel d_ab — a
   flat pixel cannot discriminate candidates, a window crossing the edge can. Also re-score
   the PRIMARY MV with the same window for a fair comparison. Cost: x9 taps in the
   already-failing branch only.
3. Push unchanged for item 2; item 1 is host-side dispatch (no push growth needed if the
   median shader uses its own tiny pipeline+set — follow the existing app-local pipe
   patterns, e.g. the cp/un pipes). CMake: add the new shader via ra_spv like the others.
4. **Gates:** both toolchains; controlled-motion er=0 (median on/off); the supervisor will
   PHOTOGRAPH the synthetic ball A/B (this stage has a perfect reproducible witness).

**Pinned constraints:** framework/ READ-ONLY; main.cpp + new app shader + CMakeLists only;
INVENTORY: adding mv_median.comp is a NEW code file — run
`python scripts/generate_inventory.py --refresh` and include the INVENTORY.md change (the
check-inventory hook blocks otherwise); no commits; no memory_order_*; do NOT run the app;
preserve the main.cpp + CMakeLists soul lines (new shader gets NO soul line); do NOT
delete build dirs.

---

## STAGE-49c kickoff — color-guided MV assignment (the operator's contour-membership principle) — tier: OPUS — NO ON-SCREEN GATES

**Decision basis (operator A/B photos + design proposal, 2026-06-12):** mv-median reduced
the flat-content artifacts (isolated vectors die by consensus) but COORDINATED rim errors
survive (several rim blocks wrong together — blind consensus can't fix peers that agree on
wrong). The operator proposed the cure: pixels belong to objects by COLOR-SIBLING relation;
the contour is where that relation breaks; motion should be assigned by MEMBERSHIP, not
photometric measurement — which sidesteps the aperture problem entirely (measuring an MV on
flat yellow is impossible; inheriting the yellow object's MV is trivial). Dossier backing:
F1 halo fix = local intensity+MV segmentation; F4 = MV clustering + boundary-only
refinement; edge-aware/cross-bilateral MV filtering is the named production family.

**Scope — apps/render_assistant only, behind `--mv-guided` (default OFF; implies and
supersedes --mv-median's pass when both given — say the precedence you ship):**
1. **Color-weighted consensus (upgrade in mv_median.comp or a sibling shader — your call):**
   add a cur_real sampler binding; for each 3x3 neighbor j of block i, weight its vote by
   color similarity sim(c_i, c_j) where c_* = cur_real sampled at the BLOCK CENTERS
   (one tap each at the mv-grid-to-fullres mapping); votes with sim below a threshold are
   EXCLUDED from the median; if fewer than 3 similar neighbors remain, keep own MV
   (no consensus across the contour). Push: a `float sim_thresh` (default ~0.10 max-channel
   units, expose `--mv-sim F` clamp [0.02,0.5]).
2. **Bilateral MV upsampling at warp (wap_warp.comp):** the MV texture is currently
   sampled with LINEAR filtering — rim pixels receive a MIXED ball/background MV, wrong for
   both. Under mv-guided: texelFetch the 4 surrounding MV-grid corners, sample cur_real at
   each corner's block center, compare with the pixel's own color (cur_real at uv), and
   select the corner whose center color is most similar (winner-take-all; ties -> the
   bilinear default). Per-pixel membership at the contour; the staircase softens. Gate the
   whole path on a `float mv_guided` push (>0.5; 0 = byte-identical LINEAR sampling).
   Apply the guided lookup to the PRIMARY MV fetch; candidates in the rescue loop keep
   texelFetch as-is (they are already discrete).
3. Push: wap grows 40->44B (`mv_guided`); the median/guided pipe carries its own small
   push. FG print gains ` mv-guided(sim:F)`. Help entries. --mv-guided without
   --warp-at-presenter errors out (same pattern as --rescue).
4. **Gates:** both toolchains; controlled-motion er=0 (guided on/off); acceptance = the
   operator's ball A/B photos round 2 (the coordinated rim chunks + staircase must shrink)
   and his game eye-check (no new artifacts on textured content where color-membership is
   noisier — if textured content degrades, the sim threshold is the knob; report the
   tradeoff honestly).

**Pinned constraints:** framework/ READ-ONLY; files = main.cpp, wap_warp.comp,
mv_median.comp (or new sibling .comp — if NEW file, remember .comp is not
INVENTORY-indexed, do NOT run the refresh), CMakeLists.txt if a new shader; no commits;
no memory_order_*; do NOT run the app; preserve soul lines (shaders get none); every
barrier() unconditional; do NOT delete build dirs; kill anything you spawn.

---

## STAGE-52 kickoff — the frame-holon: global motion model + dissidence mask — tier: OPUS — NO ON-SCREEN GATES

**Decision basis (operator's holonic architecture + ball rounds 3/4):** the remaining
artifact family (dim half-weight fringes at object rims) is produced by EVERY
partial-weight blender in the pipe — content-first/verify-after cannot kill it as a class.
The cure is boundary-first compositing (STAGE-53, the operator's fluid-in-vessel matte),
whose prerequisite is knowing per block WHAT is object vs background and HOW each moves.
The frame-holon provides both: fit a cheap GLOBAL parametric motion model per pair (the
camera/background), and blocks that dissent are the moving objects — segmentation by
motion dissidence, complementing 49c's color membership. Also immediately buys: (a) the
background MV that fill-div's documented MEMC sibling needs (dossier F5 half-length
background-MV repair), (b) a global-motion rescue candidate that handles fast pans beyond
the search window (the operator's fast-camera artifacts), (c) the object/background prior
for STAGE-53 mattes.

**Scope — apps/render_assistant only, behind `--gme` (default OFF):**
1. **F-thread CPU fit (after the fwd copy-out, before publish):** the fwd MV field is in
   host memory (hostMV[gen], RG16F mvw x mvh). Fit an affine model
   mv(x,y) = (a + b*x + c*y, d + e*x + f*y) by least squares over the grid (convert RG16F
   half-floats — check how hostMV is written: the copy is the raw image bytes; decode
   half->float on CPU or document if the field is actually fp16-in-fp32 — VERIFY the byte
   layout first-hand from the copy-out code), with 2 IRLS reweight iterations (weight =
   1/(1+(r/2px)^2), r = residual length) to reject object outliers. Cost target <2ms CPU
   for ~32k vectors at 1080p grid — measure with the existing now_ms() and print EMA in
   the bidir/fg line if it exceeds 1ms.
2. **Dissidence mask:** per block, r = |mv_block - model(x,y)| in px; publish a compact
   uint8 mask (r quantized, e.g. min(255, r*16)) to a NEW small host bridge hostDIS[gen]
   (mvw*mvh bytes, align-rounded hfb discipline, B-write... no — the mask is computed on
   CPU in F: plain _aligned_malloc host buffer + A-side import for sampling in the warp
   later (STAGE-53); for THIS stage just allocate + fill + import on A + upload alongside
   MV/SAD as an R8 image (wapDISA). The warp does NOT consume it yet (53 will) — but DO
   add a debug stat: % dissident blocks (r > 4px) printed in the FG stats line as `dis:NN%`.
3. **Model as rescue candidate:** in wap_warp.comp's rescue loop (only when --gme armed,
   new push float gme_on + 2x vec3 affine rows... 6 floats — push grows 44->72B is too fat;
   instead pass the affine params via 6 push floats ONLY when gme (44->68B is still fat)…
   ALTERNATIVE (pick this): evaluate the model MV on CPU is impossible per-pixel — but the
   model IS evaluable in-shader from 6 constants: add 6 push floats {a,b,c,d,e,f} + gme_on
   (44 -> 72B total; push budget is 128B min guaranteed — fine). The rescue loop adds ONE
   extra candidate: mv_gme = model(uv) (evaluate from push), scored with the same windowed
   SAD. Fast pans get rescued by the global model even when the local window failed.
4. **Background MV for fill-div:** in the fill-div branch (divergence pick), when gme is
   armed, the 'revealed' case samples cur at uv displaced by the MODEL MV instead of raw
   uv (the documented MEMC background-extension, F5) — gate this sub-change on gme_on so
   fill-div alone stays byte-identical.
5. Flags: `--gme` (requires --warp-at-presenter); FG marker ` gme(dis:..%)`. Help entries.
6. **Gates:** both toolchains; controlled-motion er=0 (gme on/off); the dissidence % must
   be sane on the synthetic ball (one object on static background: low % overall); print
   the fitted model's translation for a static scene ~ (0,0) as a sanity line at runtime
   (e.g. first 3 fits). Acceptance = supervisor numeric + the operator's next round.

**Pinned constraints:** framework/ READ-ONLY; main.cpp + wap_warp.comp only (+ no new
shader needed; wapDISA is an R8 image via existing img_create/upload patterns); no
commits; no INVENTORY edits; no memory_order_* (the F-thread fit is single-threaded CPU
inside F's existing loop); do NOT run the app; preserve soul lines; every barrier()
unconditional; do NOT delete build dirs; kill anything you spawn.

---

## STAGE-53 kickoff — the fluid-matte: boundary-first two-layer compositing — tier: OPUS — NO ON-SCREEN GATES

**Decision basis (operator's fluid-in-vessel principle + ball round 4):** the residual
artifact family is dim HALF-WEIGHT fringes at object rims — produced by EVERY
partial-weight blender in the pipe (soft-gate band, commit band, side-weight). The
content-first/verify-after architecture cannot kill the class. The operator's principle
inverts it: decide the BOUNDARY first (binary matte = the vessel), then content can only
fill its vessel — rim pixels are pure object or pure background, never a mix; a matte
error is a displaced clean contour (low salience), not a chunk/halo. Film-retiming
architecture (layered compositing), feasible now because STAGE-52 provides the two layers:
the dissidence mask (what is object) and the global model (how the background moves —
valid EVERYWHERE, including under/behind the object).

**Scope — wap_warp.comp + flag plumbing in main.cpp, behind `--matte` (default OFF;
requires --gme, error otherwise — the matte needs both wapDISA and the model):**
1. **Bind wapDISA** (created+uploaded by 52, currently unread): new binding 6 (R8 sampler)
   in wap_create (descriptor layout/pool grow; SPIR-V binding matches).
2. **The matte decision (per output pixel, BEFORE the existing pipeline's result is
   finalized):** classify by ADVECTED dissidence — the object silhouette at phase t.
   v1 gather-approximation (consistent with the rest of the warp): sample
   dis_o = DIS at the LOCAL-MV gather position and dis_b = DIS at uv directly;
   matte = (max(dis_o, dis_b) quantized-dissidence > matte_thresh) — the pixel is OBJECT
   if either the content it would gather or its own cell is dissident. Push
   `matte_thresh` (uint8-scale 0..1 after R8 normalization; default 0.25 = r>4px at the
   16x quantization; expose --matte-thresh clamp [0.05,1.0]).
   CROSS-CHECK with color membership where available (mv-guided armed): an OBJECT-matte
   pixel whose color matches neither warped object sample within 2*commit_thresh degrades
   to background (kills matte false-positives on the open background). Say exactly what
   you ship.
3. **Two layers, binary composite:**
   - matte=OBJECT -> the EXISTING pipeline result (everything 41..52 computed) stands.
   - matte=BACKGROUND -> the output is the pure model-layer sample:
     bg = (1-t)*texture(prev, uv - model*t/sz) + t*texture(cur, uv + model*(1-t)/sz)
     (the model is valid everywhere; this average is sharp because the model is exact for
     the background by construction). NO blending between layers — the binary decision IS
     the point. This replaces warp_result AND blend_result for background pixels (the
     soft-gate mix then mixes two identical values = no half-weight fringe can survive on
     background-classified pixels).
4. **Gates:** both toolchains; controlled-motion er=0 (matte on/off); the supervisor's
   ball A/B (the halo must die on background-classified rim pixels); the operator's
   rounds. Honest expectation to report: matte errors will show as a slightly displaced
   contour or 1-block-late silhouette on direction changes — state it, do not hide it.

**Pinned constraints:** framework/ READ-ONLY; main.cpp + wap_warp.comp only; no commits;
no INVENTORY edits; no memory_order_*; do NOT run the app; preserve the main.cpp soul
line; every barrier() unconditional; do NOT delete build dirs; kill anything you spawn.

---

## STAGE-53b kickoff — dual-anchored matte (the vessel gets both walls) — tier: OPUS — NO ON-SCREEN GATES

**Decision basis (operator ball round 5):** the matte WORKED — artifact class transformed
from hard blocks/halo to soft motion-conforming contour flex ("viscous liquid"), and
trailing-side hallucinations died. The residual is leading-edge bites: the matte is
prev-anchored only, advected forward — the silhouette arrives LATE at the leading edge
(object pixels momentarily classified background -> model-layer bites). Cure: anchor the
vessel at BOTH ends of time. The bwd MV field exists (--bidir); fit + dissidence on it
give the cur-anchored silhouette; matte(t) = the UNION of prev-anchored advected forward
and cur-anchored advected backward — leading edge covered by the cur wall, trailing by
the prev wall.

**Scope — main.cpp + wap_warp.comp, inside the existing --matte (no new flag; requires
--bidir now in addition to --gme — error if --matte without --bidir):**
1. **F-thread:** after the bwd copy-out (hostMVB), run the SAME gme_fit_affine on the bwd
   field -> 6 bwd params + bwd dissidence mask -> NEW hostDISB[gen] (+hDISB_a import,
   hfb discipline) + publish the bwd params alongside the fwd ones in the f_pair channel
   (the bwd model is ALSO useful later; for this stage only the mask matters). Fit cost
   doubles (~2.4ms total CPU in F) — extend the existing fit EMA stat to cover both.
2. **A-side:** new R8 image wapDISBA, uploaded in wap_upload alongside wapDISA.
3. **Shader:** binding 7 = u_dissidence_bwd. The matte decision becomes:
   dis_fwd = max(DIS_fwd at uv, DIS_fwd at uv - (mv*t)/sz)           [as today]
   dis_bwd = max(DIS_bwd at uv, DIS_bwd at uv + (mv*(1-t))/sz)      [cur-anchored, advected back]
   matte_object = (max(dis_fwd, dis_bwd) > matte_thresh)             [the union]
   Color cross-check unchanged. Push unchanged (no new params needed — the bwd model is
   published but not pushed in this stage).
4. **Gates:** both toolchains; controlled-motion er=0; fit EMA must stay <3ms total.
   Acceptance = ball round 6: the leading-edge bites must close; the honest residual is
   a symmetric, smaller flex.

**Pinned constraints:** framework/ READ-ONLY; main.cpp + wap_warp.comp only; no commits;
no INVENTORY edits; no memory_order_* (reuse the f_pair publish + the existing atomics
exactly as 52 did); do NOT run the app; preserve the main.cpp soul line; every barrier()
unconditional; do NOT delete build dirs; kill anything you spawn.

---

## STAGE-55 kickoff — F-pipeline throughput: overlap CPU fits + adaptive bwd (the cyclic-vibration fix) — tier: OPUS — NO ON-SCREEN GATES

**Decision basis (operator Fortnite test 3, desaturated 4090):** ticks solid at 242/s but
cons ~85-100 vs arr ~115 — B drops ~25% of pairs even in calm. Root cause is F-thread
SEQUENCING, not the 1080 Ti: per pair F does [wait fwd fence] -> [CPU fit ~2.5ms] ->
[record+wait bwd fence] -> [CPU bwd fit ~2.5ms] (+5ms total CPU fits IN the critical path
under game CPU contention) -> pair time ~15ms -> ~65-100 pairs/s ceiling. The marginal
deficit makes spans FLAP periodically (the R4-flap class, frz pinned at tick rate) = the
operator's cyclic vibration.

**Scope — main.cpp only, two pieces, NO new flags (these are unconditional wins):**
1. **Overlap the fwd fit with the bwd GPU match:** reorder F's pair iteration:
   record+submit bwd (NO wait) -> run the fwd CPU fit while the GPU matches -> wait the
   bwd fence -> bwd CPU fit. The fwd fit reads hostMV (already fenced by the fwd
   submit_wait); the bwd record does not touch hostMV. CAREFUL: the bwd record overwrites
   the OFP internal MV/SAD images — the fwd COPY-OUT is already fenced before this point
   (verify the current ordering and preserve that invariant); only the CPU fit moves.
   Expected saving ~2.5-5ms/pair.
2. **Adaptive bwd skip under pressure:** track the pair-time EMA (or reuse t_fuse_ema);
   when it exceeds the arrival interval (pair budget = 1000/arr_rate ms, use the existing
   arrival-rate signal F already has), SKIP the bwd record+fit for that pair and mark the
   generation's bwd validity false (the existing per-gen validity discipline): P then
   pushes occl_thresh=0 and matte_on=0 for that generation (classification + matte degrade
   gracefully for that pair only — the 47b/c safety net still runs). Add hysteresis (the
   R4b dead-band lesson): skip when EMA > 0.95*budget, resume full when EMA < 0.80*budget
   — NO flapping. Stats: count skipped-bwd pairs, print `bwd-skip:N%` in the gme stats
   when nonzero.
3. **Gates:** both toolchains; controlled-motion er=0; the numeric target: cons==arr at
   the synthetic ball (trivially true) and the stats machinery proving the hysteresis
   bounds. Field acceptance = the operator's Fortnite re-run: cons tracking arr (>95%),
   frz no longer pinned at tick rate, the cyclic vibration gone.

**Pinned constraints:** framework/ READ-ONLY; main.cpp only; no commits; no INVENTORY
edits; no memory_order_* (reuse the existing atomics/publish patterns verbatim); do NOT
run the app; preserve the soul line; do NOT delete build dirs; kill anything you spawn.

---

## STAGE-56 kickoff — the stasis layer (the HUD fix) — tier: OPUS — NO ON-SCREEN GATES

**Decision basis (operator Fortnite verdict post-55):** cons==arr, frz unpinned, lat
16-20ms — the pipeline is healthy; the declared pain is contour hallucinations, WORST on
the HUD. Mechanism: static overlay pixels over a moving world are damaged by every
motion-helping layer — the bilateral fetch lends them a moving corner's MV, the rescue
"rescues" their flat pixels with world vectors (the flat-content blind spot at HUD edges),
the matte classifies them dissident -> object layer -> dragged edges. The exact answer is
already in the data: sad_zero ~ 0 means the block is IDENTICAL in both frames — its
correct interpolation at ANY phase is that same content, untouched.

**Scope — wap_warp.comp + flag plumbing, behind `--stasis` (default OFF):**
1. EARLY stasis decision (computed post-barrier with the other per-pixel logic — the
   barrier discipline is untouchable; implement as a final SELECT not an early return):
   stz = texture(u_sad_field, uv).g (sad_zero at this position, block-res bilinear).
   stasis = (pc.stasis_thresh > 0.0) && (stz <= pc.stasis_thresh).
2. FINAL select (after everything, just before imageStore): if (stasis) result =
   texture(u_cur_real, uv). This OVERRIDES warp/commit/matte/soft for stasis pixels —
   identical-content pixels are exact by definition and immune to vector lending.
3. Push 80->84B (`float stasis_thresh`; 0 = off byte-identical, uniform discipline).
   `--stasis` sets the default threshold (calibrate against the SAD field's units — READ
   how sad_zero is computed in the pillar's block-match shader to choose sane units;
   expose `--stasis-thresh F` with a clamp you justify). FG marker ` stasis(thr:F)`.
4. Honest tradeoffs to document: (a) bilinear sampling of the block-res SAD field smears
   the stasis boundary by up to a block — pixels NEAR a static block lean static (safer
   than the reverse); (b) slowly-changing HUD elements (a ticking number) have small
   nonzero sad_zero and may flicker between stasis/normal at the threshold — if the
   operator reports it, the known cure is temporal hysteresis (a later stage), state it.
5. Gates: both toolchains; controlled-motion er=0 (on/off); the ball bench is INERT for
   this stage (no static overlay) — the acceptance is the operator's Fortnite HUD eye
   check.

**Pinned constraints:** framework/ READ-ONLY; wap_warp.comp + main.cpp only; no commits;
no INVENTORY edits; no memory_order_*; do NOT run the app; preserve the soul line; every
barrier() unconditional; do NOT delete build dirs; kill anything you spawn.

---

## STAGE-57 kickoff — the inertia prior (motion-state hysteresis) — tier: OPUS — NO ON-SCREEN GATES

**Decision basis (operator theory + photos, 60fps x4):** contamination is DIRECTIONAL —
fast exterior content invades slow interior objects in every ambiguous decision (the
3rd-person character's rim adopts the world's big MVs via rescue/guided-fetch; windowed
SAD on blurred textured ground lets wrong vectors win). The asymmetric cure the theory
dictates: INERTIA — content with a static history must demand SUSTAINED evidence before
adopting motion; moving->static is instant. (Also noted for the record: semi-transparent
HUD genuinely changes (sad_zero>0) — stasis honestly cannot hold it; physics limit of
capture-based FG, not a defect.)

**Scope — main.cpp + wap_warp.comp, behind `--inertia` (default OFF):**
1. **F-side persistence counter (CPU, in the existing fit walk):** per MV-grid block,
   maintain uint8 persist[mvw*mvh] across pairs (F-local array, NOT per-gen):
   static_now = (|mv_block| <= 1.0px && sad_zero_block <= 2*stasis-scale... use the SAD
   field copy already in hostSAD — read its .r/.g layout first; define static_now from
   |mv| <= 1px AND sad_zero <= 8.0 sum-units); persist = static_now ? min(255,persist+16)
   : 0 (instant reset on confident motion — asymmetry by construction). Ship to a NEW R8
   bridge hostPER/wapPERA (the hostDIS pattern verbatim; upload in wap_upload).
2. **Shader gating (binding 8 = u_persistence; threshold push `inertia_thresh`, default
   0.5 = ~8 static pairs at +16/pair):** for pixels whose persistence >= thresh:
   (a) the rescue loop REJECTS candidates with |mv_c| > 2.0px (the interior cannot adopt
   exterior speed from a neighbor); (b) the guided bilateral fetch falls back to the
   plain LINEAR result if the color-chosen corner's |mv| > 2.0px (no fast-corner
   lending); (c) the gme model rescue candidate is skipped (the global motion is exterior
   by definition for a static-history block). The block's OWN matched MV is never
   restricted — if the matcher itself confidently finds big motion, static_now=false
   already reset the counter F-side (the evidence path).
3. Push 84->88B (`float inertia_thresh`; 0=off byte-identical, uniform discipline).
   `--inertia` (requires --warp-at-presenter), `--inertia-thresh F` clamp [0.1,1.0].
   FG marker ` inertia(thr:F)`.
4. **Gates:** both toolchains; controlled-motion er=0 on/off (the bouncing ball NEVER
   rests -> persist stays 0 on it; the static background reaches max -> protected, which
   the ball bench can witness: the rim stamps INTO background should die). Field
   acceptance = the operator: the character-rim invasion and opaque-HUD-core bleed.

**Pinned constraints:** framework/ READ-ONLY; main.cpp + wap_warp.comp only; no commits;
no INVENTORY edits; no memory_order_* (the persist array is F-thread-local; the bridge
follows the hostDIS publish pattern); do NOT run the app; preserve the soul line; every
barrier() unconditional; do NOT delete build dirs; kill anything you spawn.

---

## STAGE-58 kickoff — the defaults flip (what is proven wins by default) — tier: SONNET — MECHANICAL — NO ON-SCREEN GATES

**Maintainer go (operator, 2026-06-13): "haz default lo que ya es win".** The proven stack
is 14 flags; the command must return to `--window X --fg-factor N`.

**Scope — main.cpp only. Flip these Config defaults to ON/value (all field-proven):**
warp_at_presenter=true; soft_gate=true; conf_improv=0.20f; agreement=0.05f;
commit_real=true + commit_thresh=0.08f; bidir=true; fill_div=true (div_eps=0.05f);
rescue=true; mv_guided=true (mv_sim=0.10f); gme=true; matte=true (matte_thresh=0.25f);
stasis=true (stasis_thresh=0.50f); inertia=true (inertia_thresh=0.50f).

**Escape hatches (new flags, each prints a one-line note when used):** --no-warp-at-presenter
(grid mode), --no-soft-gate, --no-commit (commit_thresh=0), --no-bidir, --no-fill-div,
--no-rescue, --no-mv-guided, --no-gme, --no-matte, --no-stasis, --no-inertia.

**Dependency cascades (CRITICAL — get these right):** --no-bidir must auto-disable
fill-div AND matte (matte requires bidir since 53b) with prints; --no-gme must auto-disable
matte with a print; --no-commit must auto-disable rescue (rescue requires commit) with a
print; --no-warp-at-presenter disables ALL WAP-dependent features (bidir/fill-div/rescue/
mv-guided/gme/matte/inertia ride the WAP path — verify each use_* derivation handles
wap=false gracefully, most already do via use_*=cfg.*&&use_wap). The old positive flags
(--warp-at-presenter, --soft-gate, --bidir, etc.) remain accepted as no-ops (already-default
prints, the STAGE-45b soft-landing pattern). The tuning-value flags (--conf-improv,
--commit-thresh, --occl-thresh, --div-eps, --mv-sim, --matte-thresh, --stasis-thresh,
--inertia-thresh, --fg-factor, --window etc.) keep working unchanged.

**Also:** update the help text header to present the DEFAULT stack + the --no-* hatches;
the validation interactions (e.g. the old '--matte requires --gme' parse errors must not
fire on defaults; convert those checks to the cascade prints).

**Gates:** both toolchains; `--help` reflects reality; the supervisor verifies: a
default-flag run (`--window X --fg-factor 2`) produces the IDENTICAL FG/feature print set
as the old 14-flag command, and each --no-* parses with its cascade prints (verifiable in
the startup print block without running long).

**Pinned constraints:** main.cpp only; framework/ READ-ONLY; no commits; no INVENTORY
edits; no memory_order_*; do NOT run the app beyond startup-print verification IF needed
(prefer build-only; the supervisor runs the gates); preserve the soul line; do NOT delete
build dirs.

---

## STAGE-59 kickoff — occupancy-lerp matte + laser mass-feedback (the operator's laser, fielded) — tier: OPUS — NO ON-SCREEN GATES

**Decision basis (operator design dialogue, 2026-06-13):** the residual at x2@120 is
extreme-motion hallucination — exactly where the matte's UNION of the two anchored
silhouettes over-covers most (both ends inflated when displacement is large). The
operator's laser principle (radial extent vs expected, sign+magnitude) in field form:
the phase-t silhouette should be the t-weighted INTERPOLATION of the two anchored
occupancies, not their union; and the laser's aggregated measurement (expected mass vs
presented mass) is the feedback thermostat (his density principle) calibrating the
threshold.

**Scope — main.cpp + wap_warp.comp, both pieces inside the existing matte machinery
(no new flag; --matte governs; one new tuning knob):**
1. **Occupancy lerp (shader):** the matte decision's dual-anchor combination changes from
   max(dis_fwd, dis_bwd) to mix(dis_fwd, dis_bwd, pc.t) — at t~0 the prev-anchored
   silhouette dominates, at t~1 the cur-anchored, between them the correct interpolated
   shape. KEEP a small union floor to avoid mid-t thinning artifacts: matte_occ =
   max(mix(dis_fwd, dis_bwd, t), 0.5*max-combination)... NO — ship it clean first:
   matte_occ = mix(dis_fwd, dis_bwd, pc.t); matte_object = matte_occ > thr_effective.
   Document the risk honestly: at mid-t with imperfect masks the lerp can thin the
   silhouette (the union never did) — the mass feedback (piece 2) is the counterweight,
   and the ball bench will witness.
2. **Laser mass-feedback (F-side CPU + a small push):** in the F walk (the masks are in
   hostDIS/hostDISB bytes), compute the anchored masses m_fwd, m_bwd = count(dis >
   matte_thresh-quantized). Expected mass at phase t is lerp(m_fwd, m_bwd, t) — but F
   doesn't know t (P does, per tick). So publish m_fwd and m_bwd (2 floats) through the
   f_pair channel; P-side, per stat window, ALSO needs the PRESENTED mass — add a cheap
   GPU counter: the warp shader atomically increments a small storage buffer counter
   (one atomicAdd per WORKGROUP using subgroup/shared reduction of matte_object count —
   NOT per pixel; a shared uint + atomicAdd at local_invocation==0) into a host-visible
   4-byte buffer P reads per tick. The feedback: err = (presented - expected)/expected;
   thr_effective = matte_thresh * (1 + k*err_ema) with k=0.5, err_ema EMA 0.9, clamped
   [0.5*thr, 2*thr] — mass low (bites) -> thr drops -> vessel refills; mass high
   (spill) -> thr rises. Push: matte_thresh becomes thr_effective P-side (no shader
   change for the knob); add `--mass-k F` clamp [0,2] default 0.5 (0 = feedback off,
   lerp only). Print `mass:±NN%` in the gme stats segment when nonzero.
   If the workgroup-atomic counter is too invasive, an acceptable v1 fallback: estimate
   presented mass P-side on CPU from the same masks (lerp of quantized masses with the
   CURRENT thr) — a model-based estimate rather than measured; SAY which you shipped and
   why. The measured GPU counter is preferred.
3. **Stats/marker:** matte marker becomes ` matte(thr:F occ-lerp mass-k:F)`.
4. **Gates:** both toolchains; controlled-motion er=0; ball witness: the silhouette at
   mid-t must stay full (no thinning) with feedback on; the leading/trailing flex should
   SHRINK vs the union (the over-coverage was the soft halo's last refuge).

**Pinned constraints:** framework/ READ-ONLY; main.cpp + wap_warp.comp only; no commits;
no INVENTORY edits; no memory_order_* in C++ (the GPU atomic is GLSL atomicAdd — allowed;
the host readback follows the existing host-visible buffer patterns); do NOT run the app;
preserve the soul line; every barrier() unconditional (the shared-reduction barrier for
the counter must be the EXISTING barrier or placed with the same unconditional
discipline — if it needs a second barrier(), it must also be top-level unconditional;
say exactly what you shipped); do NOT delete build dirs; kill anything you spawn.

## STAGE-61 kickoff — the object-holon v1: cluster identity + motion inheritance (the cancellation-class kill) — tier: OPUS — NO ON-SCREEN GATES

**Decision basis (operator experiment, 2026-06-12, camera-CONFIRMED + rival-CONFIRMED):** the
strongest hallucination class is the MOTION-CANCELLATION DEGENERACY: internal texture velocity
cancels object velocity (zoo FLAG: scroll +5 vs vy=-5 -> stripes genuinely screen-static inside
a moving body). Every pixel mechanism is CORRECTLY fooled (MV=0 is true; sad_zero~0; dissidence
low -> interior classed BACKGROUND while the rim stays OBJECT -> the matte becomes an annulus),
COMPOUNDED by period aliasing (stripe period 24px -> +-24px SAD ties resolve chaotically once the
true motion's authority vanishes -> the photographed block-chunk shred). The 90-deg-rotated
witness (FLAG-2: vertical stripes, horizontal scroll) fired on HORIZONTAL bounces exactly as the
operator predicted -> the class follows the degeneracy axis. Field identity: camera-follows-
player AND any object that stops/restarts transits the state. **LSFG exhibits the SAME class on
the zoo bench (operator-verified)** — killing it is a measurable differentiator, not parity.
No pixel-level fix exists; the contradiction (interior static vs silhouette translating) only
exists at OBJECT level. This stage builds that level's v1.

**Scope — main.cpp ONLY (no shader change, no new bindings, push stays 88B — the repair acts on
the MV fields F already ships; the whole downstream stack self-corrects through it):**
1. **F-side clustering (CPU, after the fwd fit + mask fill):** connected components (4-conn BFS,
   no heap in the loop — fixed scratch) over blocks with dis_byte > matte_thresh*255 (the exact
   OBJECT test) on hostDIS[g] (mvw x mvh). Keep top-K=16 clusters by mass, min mass 6 blocks.
   Per cluster: mass, centroid (block units), mean MV over member blocks from hostMV (REUSE the
   existing half-float decode the GME fit uses), bbox.
2. **Temporal identity (F-local, persists across pairs):** fixed table of 16 object slots: id,
   centroid, mv, mass, age, miss-count. Match clusters to slots by predicted centroid (prev
   centroid + prev mv*span/8.0 in block units — frame-holon advection); nearest-within-radius
   (gate ~6 blocks); unmatched cluster -> new slot (evict oldest-missed); unmatched slot ->
   miss++, retire after 4 misses. EMA the slot's mv (0.5) for stability.
3. **The INHERITANCE repair (the arbitration — conservative, the degeneracy signature ONLY):**
   for each tracked object with |mv_obj| >= 2px: scanline-FILL its silhouette (per-row min..max
   member columns within the bbox — the vessel filled row-wise); for every block in the filled
   region whose own MV is near-static (|mv_block| <= 1px) — the cancellation signature: a static
   interior inside a moving silhouette — OVERWRITE that block's MV in hostMV with mv_obj (write
   half-floats back). EXEMPTION (the HUD shield): skip blocks whose inertia persistence
   (persist[] — F owns it since STAGE-57) >= the inertia threshold equivalent (128) — long-static
   history wins over membership; document honestly that a long degenerate phase fades the repair
   after ~8 pairs (the trade that protects every real HUD).
4. **Consistency re-walk:** repaired blocks' dissidence bytes in hostDIS[g] must be RECOMPUTED
   (|mv_rep - model_mv| quantized min(255,16*r) — the same quantization the mask fill uses) so
   mask and field ship consistent; THEN matte_mass_count runs (the published masses see the
   repaired field). Same treatment for the bwd side (hostDISB/hostMVB, its own clusters) but
   ONLY when the bwd pass ran this pair (the STAGE-55 skip leaves bwd untouched + matte_on=0
   covers it). Publish order discipline: all writes BEFORE the f_seq.fetch_add.
5. **Budget honesty:** measure the added F cost (cluster+identity+repair) into its own EMA;
   print once at startup-settle like the gme fit-cost line if >0.3ms; it joins the pair budget
   the STAGE-55 hysteresis already manages. At 1080p/8 (~32k blocks) expect well under 1ms — but
   MEASURE, never assume.
6. **Knobs/marker/stats:** default ON; `--no-objects` hatch (repair count 0 = pre-stage exact
   behavior — the off-proof is trivial by construction, state it). Marker: ` objects(k:16 min:6
   inh:2px shield:per)`. Stats (gme segment): ` obj:K rep:N%` — live tracked objects + percent
   of in-fill blocks repaired this window (omit when 0, line unchanged).
7. **Gates:** both toolchains green; push size assert unchanged (88B); --no-objects byte-path
   proof; the on-rig zoo witnesses are the SUPERVISOR's (FL/FLAG-2 degenerate-phase shred ->
   bounded phase error; HUD untouched; BOSS_X; baseline ball unchanged; LSFG side-by-side).

**Pinned constraints:** framework/ READ-ONLY; main.cpp ONLY — if you conclude a shader change is
required, STOP and report instead of shipping one; no commits; no INVENTORY edits; no
memory_order_* (lint_hal); no heap allocation per pair (fixed scratch arrays sized at startup);
reuse the existing half-float decode/encode helpers (do NOT hand-roll a second one); the f_pair
publish-before-fetch_add discipline; do NOT run the app; preserve the soul line; do NOT delete
build dirs; kill anything you spawn.

## STAGE-62 kickoff — crescent-directed background fetch (the trail/terrain kill — the oldest artifact) — tier: OPUS — NO ON-SCREEN GATES

**Decision basis (operator forensics, 2026-06-12, the artifact carried since the beginning):** every
moving object leaves (a) a TRAIL of half-weight object ghosts behind it and (b) background
contamination on the terrain it enters, while the CONTOUR stays stable. This is the canonical
MEMC occlusion/disocclusion problem (dossier: F1 halo = fetch from the side where content is
visible; FSR3 = mask-WEIGHTED side fetch, hard one-sided refuted 0-3). The mechanism in OUR
pipeline: the matte background branch blends prev+cur model-layer samples in the two CRESCENTS
where only ONE side actually shows background — the trailing crescent (revealed: object covered
it in PREV, gone in CUR) gets prev-side OBJECT pixels = the trail; the leading crescent (object
there in CUR, not yet at phase t) gets cur-side contamination. The round-trip (48) / fill-div
(50) / SAD side-blend (49b) miss it on flat content (the photometric blind spot); the anchored
dissidence silhouettes are MODEL-based and immune — and both are ALREADY BOUND (bindings 6/7)
with the EXACT semantics needed: dis_fwd high = this pixel''s prev-side sample lands on object;
dis_bwd high = its cur-side sample lands on object.

**Scope — wap_warp.comp + main.cpp (the matte BACKGROUND branch only; push grows 88→92B):**
1. **Shader — crescent weighting in the matte background branch:** where the background/model
   layer currently blends its prev/cur samples (weights (1−t, t)), re-weight each side by its
   NON-contamination: c_f = smoothstep(0.5·thr, 1.5·thr, dis_fwd), c_b = same on dis_bwd (thr =
   pc.matte_thresh, the same quantization both masks share); w_prev = (1−t)·(1−c_f), w_cur =
   t·(1−c_b); if w_prev+w_cur < 1e-4 (both sides contaminated — the ambiguous overlap) fall back
   to the CURRENT blend unchanged; else normalize and blend. This is mask-weighted side fetch
   (FSR3 form), NOT hard one-sided. Reuse the dis_fwd/dis_bwd values the matte block already
   computed this invocation (do NOT re-sample the masks). The whole thing gated on the new push
   flag crescent_on > 0.5 AND inside the existing matte_on branch — off ⇒ byte-identical.
2. **Push 88→92B (23 floats):** + crescent_on. Update pcr.size, the pcw array, the shader
   PushConsts block, and the size comment chain. The flag is 1.0 iff use_matte AND cfg.crescent
   (matte governs — no matte ⇒ no masks ⇒ no crescent).
3. **Knob/marker:** cfg.crescent default ON; --no-crescent hatch (+ legacy --crescent no-op
   print; --no-matte/--no-gme/--no-bidir/--no-wap cascades take crescent with them — it lives
   inside the matte branch). Marker: extend the matte marker to ` matte(thr:F occ-lerp mass-k:F
   crescent)` when on (omit the word when off).
4. **Gates:** both toolchains; the shader must keep EVERY barrier() top-level unconditional and
   add NO new barriers (the change is pure ALU inside an existing branch); push size assert
   comments updated consistently; --no-crescent ⇒ byte-identical (the push flag is 0 — argue it,
   the off-proof is the uniform-gate discipline every stage uses). NO on-screen gates (the
   supervisor runs the zoo + the operator''s ball round: the trail ghosts and the terrain
   contamination should collapse while the contour stays).

**Pinned constraints:** framework/ READ-ONLY; main.cpp + shaders/wap_warp.comp ONLY; no commits;
no INVENTORY edits; no memory_order_*; do NOT run the app; preserve the soul lines; do NOT
delete build dirs; kill anything you spawn. If the background branch''s current sample/blend
structure differs from this kickoff''s assumption (read it FIRST), STOP and report instead of
improvising a different mechanism.

## STAGE-63 kickoff — the contour shape-field: distance+feature transform, rim-sector inheritance (the operator''s normal-rays) — tier: OPUS — NO ON-SCREEN GATES

**Decision basis (operator design, 2026-06-12):** "the contour is the only consistent thing — let
the contour cast normalized/orthogonal rays inward so the object knows its own shape as a trace."
Formal form: the silhouette''s DISTANCE TRANSFORM + FEATURE TRANSFORM (every interior block knows
its distance to the contour AND its nearest rim block). What it buys: (a) the closed contour
integrates away the aperture problem — the rim is the best motion instrument the object has; (b)
rim-SECTOR inheritance handles rotation/scaling/deformation that STAGE-61''s single rigid mv_obj
cannot (the zoo''s scaling ball + BOSS_X are the witnesses); (c) depth grades every mechanism
(deep interior = certain object; the rim band = the mixed zone that must NOT be repainted).

**Scope — main.cpp ONLY (extends STAGE-61''s object_repair; no shader/push/binding change):**
1. **Rim extraction:** within each kept cluster''s bbox, a member block is RIM iff any 4-neighbour
   is non-member (label != cid). Store the rim blocks'' own MEASURED MVs (the half-float decode
   already in object_repair) — these are the contour''s instruments.
2. **Chamfer distance + feature transform (2-pass, bbox-bounded):** over the cluster''s FILLED
   silhouette (the scanline in-fill STAGE-61 already computes): forward pass (TL→BR) + backward
   pass (BR→TL), integer chamfer weights 3 (orthogonal) / 4 (diagonal); alongside the distance,
   propagate the NEAREST RIM BLOCK''s MV (feature transform — when a neighbour relaxes the
   distance, copy its rim-MV). Fixed scratch sized once at F-thread start (two int16/float
   arrays of mvw_f·mvh_f — follow the STAGE-61 scratch pattern; no per-pair heap).
3. **Rim-sector inheritance (replaces the rigid mv_obj write inside the repair loop):** for a
   block passing the cancellation gate (|mv_block| ≤ 1px, persist shield clear), the inherited MV
   becomes mix(mv_rim_nearest, mv_obj_slot, w) with w = clamp(dist_chamfer/(3·4), 0, 1) — near
   the contour the nearest rim sector wins (rotation/scaling correct), deep interior relaxes to
   the slot''s stable EMA mean. The dissidence recompute stays (against the inherited value).
4. **Depth gate (the contour protection):** the repair only fires on blocks with chamfer dist ≥ 3
   (strictly interior, ≥1 block from the rim) — the rim band itself is NEVER rewritten (the
   operator''s stable contour is the asset; do not touch the instrument).
5. **Arming precondition change:** with rim-sector inheritance the |mv_obj| ≥ 2px arming gate
   GENERALIZES: arm iff (|mv_obj_slot| ≥ 2px) OR (the rim MV SPREAD ≥ 2px — max |mv_rim_i −
   mv_rim_j| over the rim sample, capped at 32 rim samples for the spread scan) — a scaling/
   rotating object can have mean ~0 yet a live rim field. Document which arm fired is NOT
   distinguished in stats (keep it lean).
6. **Knob/marker:** cfg.shapefield default ON, --no-shapefield hatch (child of objects: --no-
   objects takes it down; --no-shapefield alone reverts to STAGE-61''s rigid mv_obj inheritance —
   the clean A/B). Marker: the objects marker gains " shape" → ` objects(k:16 min:6 inh:2px
   shield:per shape)`. Legacy --shapefield no-op print. No new stats (rep:N% already covers).
7. **Gates:** both toolchains green; --no-shapefield = STAGE-61 behavior exactly (the rigid-MV
   path must be PRESERVED as the else-arm, not re-derived); no heap in the per-pair path; cost
   joins obj_cost_ema (no new EMA). NO on-screen gates — the supervisor''s zoo witnesses: the
   scaling ball + BOSS_X (deformation correctness), the tunnels (clean contour, chaotic
   interior), the flags'' degenerate phases.

**Pinned constraints:** framework/ READ-ONLY; main.cpp ONLY; no commits; no INVENTORY edits; no
memory_order_*; no per-pair heap (fixed scratch, STAGE-61 pattern); reuse half_to_float/
float_to_half (do NOT add another codec); do NOT run the app; preserve the soul line; do NOT
delete build dirs; kill anything you spawn. Read STAGE-61''s object_repair lambda FIRST and
extend it in place — if its structure conflicts with this kickoff, STOP and report.

## STAGE-64 kickoff — appearance-band temporal re-blend on committed pixels (the brightness-constancy class) — tier: OPUS — NO ON-SCREEN GATES

**Decision basis (operator photo, 2026-06-12, the pulsing zoo ball):** an object whose APPEARANCE
changes between the pair anchors (brightness pulse, shadows, reflections — the zoo''s SC ball by
design) violates the brightness-constancy assumption every vector method rests on. The normal
warp path handles it (the (1−t,t) blend interpolates color); the ONE-SIDED paths betray it: the
commit family (commit-real / the soft-commit band) shows ONE anchor''s color un-interpolated →
patches of the WRONG-TIME color on the object surface (the operator''s photo: stains of the
previous tone). It compounds: the appearance change inflates SAD everywhere on the object → more
commits exactly where they hurt most. The art: codec weighted prediction (gain/offset
compensation for one-sided fetches); census/illumination-robust matching. The v1 here is the
cheapest principled form: committed pixels re-blend TEMPORALLY inside an appearance band.

**Mechanism (shader-local, the commit family only — read the commit paths FIRST):**
for a pixel the commit machinery resolved one-sided (the commit-real unwarped fetch and the
committed component of the soft-commit band): let S_n = the committed (time-nearest) side''s
sample at its fetch position, S_o = the OTHER anchor sampled at the SAME position. If the two
times agree on STRUCTURE but differ mildly in TONE (max-channel |S_n−S_o| inside the appearance
band) the correct phase-t color is the temporal blend — re-blend toward (1−t,t) with weight
w_app = 1 − smoothstep(band, 2·band, dmax(S_n,S_o)): small difference → full temporal
interpolation (the pulsing surface gets its phase-correct tone); large difference = structural
disagreement (displacement, the reason commit exists) → w_app→0, the one-sided commit stands
unchanged. result = mix(S_n, (1−t)·S_prev_pos + t·S_cur_pos, w_app) where both samples are at
the committed position. ONE extra texture tap per committed pixel (the other side at the same
uv) — committed pixels only, no new bindings.

**Scope — wap_warp.comp + main.cpp:**
1. Shader: the re-blend in the commit-real path and the soft-commit band''s committed component
   (read their current structure FIRST; if the band''s committed component is not separable,
   apply to commit-real only and SAY so). NOT stasis (sad_zero already excludes changing
   content), NOT rescue (its winner is a scored displaced fetch, not a time-side pick).
2. Push 92→100B (+2 floats: appear_on, appear_band). Default band 0.10 (max-channel, [0,1]
   scale) — clamp knob [0.02,0.5]. appear_on = 1.0 iff cfg.appearance (default ON) AND the
   commit machinery is live (commit_thresh>0 — the same governance commit already has).
3. Knobs/marker: --no-appearance hatch; --appear-band F; legacy --appearance no-op. Marker: the
   commit marker gains the band → ` commit:0.080(real appear:0.10)`; --no-appearance omits the
   word. --no-commit cascade takes appearance down (it lives inside the commit paths).
4. Gates: both toolchains; every barrier() stays top-level unconditional, NO new barriers (pure
   ALU + one tap inside existing branches); --no-appearance ⇒ byte-identical (uniform push gate,
   the established off-proof). NO on-screen gates — the supervisor''s zoo witness: the pulsing
   ball''s wrong-tone patches collapse; the operator''s photo class is the acceptance test.

**Pinned constraints:** framework/ READ-ONLY; main.cpp + shaders/wap_warp.comp ONLY; no commits;
no INVENTORY edits; no memory_order_*; do NOT run the app; preserve the soul lines; do NOT
delete build dirs; kill anything you spawn. If the commit paths'' structure conflicts with this
kickoff''s assumption, STOP and report instead of improvising.

## STAGE-65 — stigmergy expiration (supervisor-built; the operator''s bed-reflection principle, 2026-06-12)

**Decision basis:** the operator''s principle from REFLEXION_ALUCINACIONES reading: generated-frame
information should EXPIRE — "todo el contenido se ve igual, pero no toda la informacion en el es
valida". Audit result: pair-scoped info already expires correctly (34e pair identity, 34f skip-
consumed, no feedback recursion); what does NOT expire = the cross-pair DERIVED memories, which
decay on fixed time constants THROUGH contradictions: (a) the holon slot-MV EMA (0.5) carries a
STALE direction for ~1-2 pairs after every bounce — the operator''s "flags still hallucinate at the
bounce" residual post-61; (b) the mass-feedback err EMA (0.9) steers thr_eff through regime changes
for ~10 ticks. The Phyriad-native name: stigmergy EVAPORATION — the rule is EMA for noise,
EXPIRATION for contradiction (the asymmetry STAGE-57''s inertia already uses, generalized).

**Shipped (main.cpp only, supervisor-built+verified):** (1) slot-MV innovation gate: direction
reversal (dot<0) OR delta > kObjMvExpirePx (4px) → adopt the fresh cluster MV outright, else EMA as
before; (2) mass-feedback innovation gate: |err−ema| > kMassErrExpire (1.0) → adopt fresh err, else
0.9-blend; (3) cfg.expire default ON, --no-expire hatch (pure-EMA pre-stage decay = the clean A/B),
legacy --expire no-op, " expire" FG marker. Gates: both toolchains green; zoo default marker
" expire" + er=0; --no-expire control marker absent + er=0. The behavioral witness (bounce-moment
hallucination shrinking — the holon adopting the new direction in ONE pair) = the operator''s round.

## STAGE-66 kickoff — the scene-holon v1: silhouette memory with stigmergic expiration — tier: OPUS — NO ON-SCREEN GATES

**Decision basis (operator architecture reflection, 2026-06-12 night):** the presented-time SCENE
should be the superior holon — pairs stop being the truth and become MEASUREMENTS that update a
persistent belief; the operator''s stigmergy-expiration (STAGE-65) is that holon''s belief-update
rule, built before the memory it protects (the right order). V1 = the first organ: the SILHOUETTE
persists across pairs. Why this first: during a cancellation phase the fresh masks go silent → the
cluster DIES → STAGE-61/63''s inheritance cannot fire (no holon to inherit from) → the matte
collapses and the bounce-moment hallucinations bloom. With memory: the silhouette survives the
silence → the cluster + identity survive → the inheritance keeps firing → the interior keeps the
object''s motion. "The object does not die when it stops dissenting; it merely stops being
confirmed."

**Scope — main.cpp ONLY (F-side; no shader/push/binding change — the masks already travel):**
1. **The persistent prior:** one byte array `prior` (mvw_f·mvh_f, the SAME 16·r quantization the
   masks use), living in CUR-anchored space, persisting across pairs. Plus one advection scratch.
   Both sized ONCE at F-thread start (the STAGE-61 scratch discipline; no per-pair heap).
2. **Advection (per pair, before the merges):** the previous pair''s cur space is THIS pair''s prev
   space; the fwd MV field maps prev→cur. Nearest-block GATHER with the destination block''s MV
   (valid for smooth fields at block scale): src = round(b_xy − mv(b)/8.0) (the MV is the full
   per-pair displacement in px; /8 = block units; NO span factor — the field already spans the
   pair); out-of-grid or unmapped → 0. prior_adv = gather(prior). Keep BOTH: prior (pre-advection
   = prev-anchored) and prior_adv (cur-anchored).
3. **The merges (after each fresh mask is written, BEFORE clustering/repair/mass-count):**
   - hostDIS  (prev-anchored): dis = max(dis_fresh, decay(prior))      [prior pre-advection]
   - hostDISB (cur-anchored, only when the bwd pass ran): same with prior_adv.
   decay(p) = (uint8)(p · kPriorDecay), kPriorDecay = 0.75 (unconfirmed memory dies in ~5 pairs).
   **EXPIRATION (the STAGE-65 rule, applied per block BEFORE the merge):** if the fresh evidence
   CONTRADICTS the memory — fresh_dis low AND the block''s measured |mv| > kObjStaticMax (it moves
   WITH the world'' model → it is confidently BACKGROUND) — the prior is 0 there (evaporates
   instantly). A silent block (fresh low AND |mv| ≤ kObjStaticMax) is cancellation-compatible →
   the memory persists, decayed. Fresh dissent ≥ prior → the memory is refreshed by the merge.
4. **Order in F (per anchor):** fit → fresh mask → expiration+merge → clustering + identity +
   inheritance repair (STAGE-61/63, now over the MERGED mask — identity survives the silence) →
   dissidence recompute → matte mass count → publish. All writes before the f_seq bump.
5. **The prior refresh (end of pair):** prior := max(decay(prior_adv), hostDISB post-repair) when
   the bwd pass ran; else prior := decay(prior_adv) (pure decay — a skipped pair never refreshes).
6. **The self-reinforcement brake (DOCUMENT it honestly in code):** the repair''s recomputed bytes
   can refresh the prior → a remembered silhouette can sustain itself through a LONG cancellation.
   That is CORRECT physics when the cluster is armed (a moving rim — the body still dissents at
   its contour); the false-positive case (a genuinely static region) cannot arm (mv_obj ≈ 0, no
   rim spread) → no repair refresh → the prior dies in ~5 pairs by pure decay. The decay is the
   unconditional floor; the expiration rule is the instant kill.
7. **Knob/marker/stats:** cfg.scene_memory default ON; --no-memory hatch (+legacy --memory no-op);
   child of --objects (the memory exists to keep the holon alive; --no-objects/--no-gme/--no-wap
   cascade it down). Marker: the objects segment gains ` mem:0.75` → ` objects(k:16 min:6 inh:2px
   shield:per shape mem:0.75)`. No new stats v1 (obj:K already shows identity survival — the
   witness is K holding through a degenerate phase instead of dropping).
8. **Gates:** both toolchains; --no-memory = pre-stage exact (the merge/advection/refresh all
   gated); no per-pair heap; cost joins obj_cost_ema. NO on-screen gates — the supervisor''s zoo
   witnesses: (a) the flags'' degenerate phases — obj:K stable + the interior KEEPS moving (the
   inheritance no longer starves); (b) NO trailing silhouette ghosts (the expiration working —
   watch the ball''s wake); (c) HUD untouched; (d) the scaling ball (memory must not fight a
   legitimately changing silhouette — the fresh dissent always wins the max()).

**Pinned constraints:** framework/ READ-ONLY; main.cpp ONLY; no commits; no INVENTORY edits; no
memory_order_*; no per-pair heap; reuse half_to_float (no new codecs); do NOT run the app;
preserve the soul line; do NOT delete build dirs; kill anything you spawn. Read the STAGE-61/63
object_repair machinery and the mask-fill sites FIRST; if the F-thread order conflicts with item
4, STOP and report instead of improvising.

## STAGE-67 kickoff — the traveling silhouette: swept-band matte geometry fix (THE dominant artifact) — tier: OPUS — NO ON-SCREEN GATES

**Decision basis (operator full-zoo photo, 2026-06-12 morning — PRIMARY OBJECTIVE):** the oldest
artifact (object-residue trail behind every mover + background contamination ahead) DOMINATES and
masks every other class. Root cause located by the supervisor: the matte''s dual-anchor terms each
take max(dis_SELF, dis_gather). The SELF terms anchor the silhouette at BOTH end positions for the
whole pair, so the SWEPT BAND (between prev-pos and cur-pos) classifies OBJECT for ~75% of the
phase window (saturated mask ~1.0 vs thr 0.25 → mix crosses at t≈0.75). The object LAYER then
paints object content into the vacated wake (the photographed residue) and the entered terrain.
STAGE-62''s crescent fetch never gets jurisdiction (it only acts on BACKGROUND-classified pixels).
The cure: the phase-t silhouette must TRAVEL, not smear. The gather terms ARE the traveling
silhouette — and since STAGE-61/63 repair the fields, the prev-footprint blocks carry the OBJECT''s
MV (the gather displacement is correct there). The self crutches (53b''s cover for bad MVs) must
fade with phase.

**MANDATORY FIRST DELIVERABLE — the region×phase truth table.** Before coding, derive on paper the
classification for the SIX cells {WAKE (vacated band), CORE (object at phase-t), LEAD (entered
band)} × {early t (~0.2), late t (~0.8)} under BOTH candidate formulas:
  (A) phase-faded selves: dis_fwd = max(dis_self·(1−t), dis_gather); dis_bwd = max(dis_self_b·t,
      dis_gather_b); occupancy = mix(dis_fwd, dis_bwd, t) (the existing lerp).
  (B) gather-primary: occupancy = mix(dis_gather, dis_gather_b, t), with the selves kept ONLY as
      a small confidence floor (e.g. ·0.25) folded by max().
Trace each cell using the REAL gather formulas in the shader (dis_gather at uv − mv·t/sz;
dis_gather_b at uv + mv·(1−t)/sz, both using the warp''s per-pixel mv) and the REAL field contents
post-repair (prev-footprint blocks carry mv_obj; pre-arrival blocks carry ~0 → gather_b
degenerates to self_b there, which is what covers the leading edge). REQUIRED outcomes: WAKE →
background by t≈0.2 (the trail dies); LEAD → object coverage no worse than today (the STAGE-53b
leading-edge bite must NOT regress); CORE → object at all t. Pick the formula whose table closes;
if NEITHER closes all six cells, STOP and report the table instead of shipping.

**Scope — wap_warp.comp (the matte decision block) + main.cpp:**
1. Shader: the chosen formula, gated on a new push flag travel_on>0.5 (uniform; off = the EXACT
   current max(self,gather) forms → byte-identical). The dis_fwd/dis_bwd values that STAGE-62''s
   crescent weighting reads must REMAIN the full max(self,gather) testimonies (the contamination
   evidence — a wake pixel''s prev side IS contaminated regardless of phase); only the OCCUPANCY
   (matte_object decision) changes. Keep them as separate locals if needed.
2. Push 100→104B (+travel_on, 26 floats). Host: travel_push = matte_push && cfg.travel.
3. Knob/marker: cfg.travel default ON; --no-travel hatch; legacy --travel no-op; the matte marker
   gains " travel". Cascades: matte governs (same as crescent).
4. SECONDARY (same stage, small): kPriorDecay 0.75→0.55 (the unconfirmed-memory wake tail shortens
   ~5→~3 pairs; the armed-rim confirmation path is unaffected — document that the matte fix makes
   the remembered wake harmless for the OBJECT layer anyway, this just trims the crescent-evidence
   tail). Update the marker mem:0.55 and the STAGE-66 comments that cite 0.75.
5. Gates: both toolchains (glslc must run); every barrier top-level unconditional, no new
   barriers; --no-travel byte-identical (uniform push gate); --no-memory/--no-objects still clean.
   NO on-screen gates — the supervisor''s zoo witnesses: the yellow ball''s wake (the residue MUST
   die), the leading edge (no new bites — the 53b regression watch), the flags'' degenerate phases
   (the memory+inheritance loop must still hold them).

**Pinned constraints:** framework/ READ-ONLY; main.cpp + shaders/wap_warp.comp ONLY; no commits;
no INVENTORY edits; no memory_order_*; do NOT run the app; preserve the soul lines; do NOT delete
build dirs; kill anything you spawn. Read the matte decision block + the crescent block FIRST;
the truth table is the contract — paste it in the report.

## STAGE-68 kickoff — wake evaporation + gather validity (closing the memory-vs-travel loophole) — tier: OPUS — NO ON-SCREEN GATES

**Decision basis (operator zoo photo post-67, 2026-06-12):** the trail persists WITH travel on.
Diagnosis: the memory-kept wake blocks carry |mv|≈0 (static background) → the occupancy gather
displaces by zero → gather ≡ self (the remembered byte reads itself) → STAGE-67''s fade never
engages. Two compounding holes: (a) the STAGE-66 expiration cannot distinguish wake from
cancellation on a static background (both are fresh-low + static); (b) a zero-displacement gather
is not independent testimony. Two surgical pieces, both refinements of existing stages:

**Piece 1 — holonic wake evaporation (main.cpp, the mem_refresh site):** "the object takes its
silhouette with it." After the identity match + repair, for each ARMED slot (|mv_obj| ≥
kObjInhMin, fwd table): expire (zero) mem_prior blocks that are (prior > 0) AND (NOT inside the
slot''s matched cluster''s scanline-filled silhouette) AND (inside the cluster bbox EXPANDED
opposite to the slot MV by ceil(|mv|/8)+1 blocks — the trailing sweep). Order: runs at the
refresh step (cur-anchored space — use mem_adv→mem_prior AFTER the normal refresh max). Needs the
fwd cluster fill bounds at refresh time: persist the per-cluster bbox+rowmin/rowmax of the ARMED
fwd clusters into small fixed scratch during object_repair (cap kObjSlots entries; the rowmin/max
arrays are per-cluster rows — store per slot a bbox + a copy of its row bounds into a fixed
[kObjSlots][rows-of-bbox] scratch is overkill: simpler, RE-derive the fill bounds from obj_label
for the matched cluster id inside the refresh — obj_label still holds THIS pair''s bwd labels...
CAREFUL: at refresh time obj_label holds the BWD pass''s labels (the bwd repair ran last). The
wake evaporation needs CUR-anchored geometry = exactly the BWD clusters → use the BWD slot table
+ the bwd labels as-is. If the bwd pass was skipped this pair, skip evaporation too (the throttle
case; pure decay still bounds it).**
**Piece 2 — gather validity in the occupancy (wap_warp.comp, inside the travel_on branch only):**
a gather with no displacement is self in disguise. g_f = smoothstep(0.5, 1.5, length(mv)) (mv in
px — the warp''s per-pixel mv already in scope); occ_fwd = max(dis_self·(1−t), dis_gather·
mix(1−t, 1.0, g_f)); occ_bwd = max(dis_self_b·t, dis_gather_b·mix(t, 1.0, g_f)). Static blocks
collapse to pure faded selves (the wake fades with phase no matter what the memory says); moving
blocks (incl. the repair-armed cancellation interiors, |mv_obj|≥2px → g_f=1) keep the full
traveling gather. NO push change (the constants are geometry like the gather offsets; all inside
travel_on — --no-travel reverts everything).

**Gates:** both toolchains (glslc must run); barriers unchanged; --no-travel still byte-identical
(piece 2 inside the gate); --no-memory unaffected by piece 1 (gated on use_memory); the
--no-objects/--no-gme cascades still clean. NO on-screen gates — the supervisor''s zoo witness:
the yellow ball''s wake MUST die now (with memory ON); the flags'' degenerate phases must STILL
hold (the repair path keeps g_f=1 there).

**Pinned constraints:** framework/ READ-ONLY; main.cpp + shaders/wap_warp.comp ONLY; no commits;
no INVENTORY edits; no memory_order_*; no per-pair heap (fixed scratch only); do NOT run the app;
preserve the soul lines; kill anything you spawn. Read mem_refresh + object_repair + the
travel_on occupancy block FIRST; if the bwd-labels-at-refresh assumption does not hold (verify
what obj_label contains at the refresh call site), STOP and report.

## STAGE-71 kickoff — the contour marriage v1: per-pixel boundary arbitration by color affinity — tier: OPUS — NO ON-SCREEN GATES

**Decision basis (operator close-up photos + the supervisor''s objdump evidence, 2026-06-12):** with
the masks now TIGHT (STAGE-70, verified first-hand in the objdump grids) the visible artifacts did
NOT change — therefore they are not painted by the mask machinery. The photos localize them: every
remaining bite/chunk lives in the CONTOUR BAND at block scale (object edges; object-object overlap
frontiers). Root: the warp''s fetches are driven by 8px-block MVs — a block straddling the contour
drags the wrong side''s content (background into the object = bites; object onto background =
residue) regardless of classification. The error is SUB-BLOCK; no block-level mechanism can fix it.
The cure is the appearance-contour marriage: per-PIXEL arbitration in the boundary band, by color
affinity — the operator''s pixel-siblings principle at pixel granularity, with everything needed
already in shader registers.

**Scope — wap_warp.comp (the matte composition site) + main.cpp (push/knob/marker):**
1. **The boundary band:** pixels where the block-level occupancy is UNCERTAIN: |matte_occ −
   matte_thresh| < matte_thresh (i.e. occ in (0, 2·thr) — wide enough to cover the bilinear
   transition zone of the R8 masks). Outside the band: the binary matte decision EXACTLY as today.
2. **The per-pixel arbitration (inside the band only):** let obj_c = the OBJECT-layer color this
   invocation already produced (the committed/warped object content) and bg_c = the BACKGROUND/
   model-layer color (the crescent-weighted blend — compute both for band pixels). Arbiter ref =
   the TIME-NEAREST real at uv (near_real — already sampled by the commit machinery; reuse, do
   not re-tap if in scope, else one tap). Affinity: d_o = maxch|ref − obj_c|, d_b = maxch|ref −
   bg_c|; alpha_obj = (d_b + eps) / (d_o + d_b + 2·eps), eps=1e-3. result = mix(bg_c, obj_c,
   alpha_obj). The real frame is the ground truth of what this pixel looks like near this moment:
   if it resembles the object''s color the pixel is object, if the background''s, background —
   sub-block precision from appearance, no new data.
   HONEST LIMIT (document): near_real''s geometry is anchored at one end (bias bounded by the
   phase distance to the nearest anchor — worst at t=0.5); the arbitration is still per-pixel
   where the current failure is per-block (8px chunks → ≤ sub-block fringe).
3. **Push 104→108B (+contour_on, 27 floats).** contour_push = matte_push && cfg.contour (matte
   governs — the band needs the masks).
4. **Knob/marker:** cfg.contour default ON; --no-contour hatch; legacy --contour no-op; matte
   marker gains " contour". Cascades via matte_push as usual.
5. **Gates:** both toolchains (glslc must run); NO new barriers, all ALU inside the existing
   matte branch; --no-contour ⇒ byte-identical (uniform push gate); the 53b leading-edge watch
   AGAIN: the band arbitration must only ever act where the binary decision was uncertain — the
   deep-object and deep-background pixels are untouched by construction (assert the band
   condition bounds in the report).

**Pinned constraints:** framework/ READ-ONLY; main.cpp + shaders/wap_warp.comp ONLY; no commits;
no INVENTORY edits; no memory_order_*; do NOT run the app; preserve the soul lines; kill anything
you spawn. Read the matte composition site FIRST (where matte_object selects between the object
result and the background bg — quote its CURRENT structure in the report); if obj/bg layer colors
are not both materializable for band pixels without restructuring, STOP and report.

## STAGE-72 kickoff — the object crescent: anchored-claim side weighting for the OBJECT layer — tier: OPUS — NO ON-SCREEN GATES

**Decision basis (operator fg4 close-ups, 2026-06-12):** post-71 the bites'' FILL is correct (clean
background through the holes — the operator''s "reflejan extrañamente bien el fondo" = 62/70/71
working) but the bites EXIST because the OBJECT layer itself carries background in the entry band:
at the leading band the prev-side fetch uses the prev-anchored local MV (background, ~model) →
A_samp = background; the (1−t,t) blend washes it; the commit one-sides to the time-nearest anchor
(= prev at early t) → a quarter-displacement bite at fg4 phases. The correct content EXISTS in the
OTHER anchor (cur — the object already arrived). This is EXACTLY the STAGE-62 crescent lesson,
applied to the background layer and never to the object layer. The missing symmetric piece: weight
the object layer''s two warped samples by each anchor''s OBJECT-PRESENCE CLAIM.

**Scope — wap_warp.comp (+ main.cpp knob/marker/push):**
1. **The claim weights (reuse, in scope):** c_f = smoothstep(0.5·thr, 1.5·thr, dis_fwd), c_b =
   same on dis_bwd (the EXACT crescent weights — if 62''s c_f/c_b locals are scoped away, hoist or
   recompute identically; say which). c_f = "the prev anchor claims this pixel as object"; c_b =
   the cur anchor''s claim.
2. **Object-layer side weighting (gated obj_crescent_on>0.5 AND matte_on>0.5 AND matte_object —
   the OBJECT-classified pixels only):** the warp''s A/B blend weights upgrade from (1−t, t) to
   w_A ∝ (1−t)·mix(1.0, c_f, k) and w_B ∝ t·mix(1.0, c_b, k) with k=1 v1 (full claim gating),
   normalized; both claims ≈ 0 → fall back to (1−t,t) unchanged. Read the warp''s ACTUAL blend
   site(s) FIRST — warp_result and blend_result composition, plus how the soft-commit band picks
   its committed side — and apply the weighting so that: leading band (c_f≈0, c_b≈1) → the cur
   sample carries at ANY t; trailing (c_b≈0, c_f≈1) → prev carries. The commit''s time-nearest
   side pick must RESPECT the claims in the band (a committed fetch from an anchor that does NOT
   claim the pixel is the bite — prefer the claiming anchor''s sample; quote the before/after).
   If the commit machinery''s structure makes the claim-aware pick ambiguous, apply the weighting
   to the pre-commit blend only and SAY so.
3. **Push 108→112B (+obj_crescent_on, 28 floats).** Host: objcres_push = matte_push &&
   cfg.obj_crescent (matte governs — the claims come from the masks).
4. **Knob/marker:** cfg.obj_crescent default ON; --no-obj-crescent hatch; legacy --obj-crescent
   no-op; the matte marker gains " objcres". Cascades via matte_push.
5. **Gates:** both toolchains (glslc must run); NO new barriers; --no-obj-crescent ⇒
   byte-identical (uniform push gate); the deep-object case (c_f≈c_b≈1) must reduce to ~(1−t,t)
   (argue it: mix(1,1,k)=1 both sides → normalized = (1−t,t) exactly); the 53b watch: the leading
   band gets MORE object content, never less.

**Pinned constraints:** framework/ READ-ONLY; main.cpp + shaders/wap_warp.comp ONLY; no commits;
no INVENTORY edits; no memory_order_*; do NOT run the app; preserve the soul lines; kill anything
you spawn. Read the warp blend + commit sites FIRST; if the structure conflicts, STOP and report.

## STAGE-73 kickoff — the phase-anchored primary MV (the stale-vector fix; outdump-diagnosed) — tier: OPUS — NO ON-SCREEN GATES

**Decision basis (supervisor outdump evidence, 2026-06-12):** the warp output at t=0.14 is nearly
clean; at t=0.78 heavily damaged — the damage GROWS WITH PHASE. The inputs are pristine (pairdump:
span=1, adjacent, clean). Diagnosis: the primary MV is fetched from the PREV-anchored fwd field AT
the output pixel uv; at high phase the content has moved 2-4 blocks from where its vectors live →
stale-vector fetches. The CUR-anchored bwd field (binding 5, uploaded since STAGE-48) is never used
as primary — only for classification. Production MEMC re-anchors the field to the interpolation
instant; our cheap exact form: USE THE ANCHOR NEAREST TO t.

**Scope — wap_warp.comp + main.cpp (knob/push):**
1. **The phase-anchored primary:** where the warp resolves its primary per-pixel mv (the bilateral/
   guided fetch of u_motion_vectors at uv — find it; ALL downstream consumers use that mv), make
   the source phase-anchored when the bwd field is valid this generation (occl_thresh>0 ⇒ bwd_ok):
   mv_fwd = the existing fetch (u_motion_vectors at uv); mv_bwd = the SAME fetch machinery applied
   to u_motion_vectors_bwd at uv, NEGATED (the bwd field maps cur→prev; the warp formula wants the
   fwd convention: mv_true ≈ −mv_bwd at cur-anchored positions). Blend: w_b = smoothstep(0.35,
   0.65, t); mv = mix(mv_fwd, −mv_bwd, w_b). At t<0.35 = exactly today; at t>0.65 the cur-anchored
   field (correct where the content now IS); smooth center (both near-valid there, no pop).
   When bwd invalid (occl_thresh==0: the 55 skip / no-bidir) → mv_fwd only (today exactly).
2. **Apply to the ONE primary resolution site** so every consumer (the A/B fetch positions, the
   gathers, the matte advection terms, the crescents'' geometry) inherits coherently. Do NOT touch
   the round-trip classification (it deliberately compares the two fields).
3. **Push 112→116B (+phase_anchor_on, 29 floats).** phase_push = use_bidir && cfg.phase_anchor
   (bwd field required). --no-phase-anchor hatch (default ON); legacy --phase-anchor no-op; the
   bidir marker gains "+pa": ` bidir(occl:1.50px+pa)`.
4. **Gates:** both toolchains (glslc); no new barriers; --no-phase-anchor ⇒ byte-identical
   (uniform gate); the 53b leading-edge + the t=0 / t=1 limits must reduce to the pure anchors
   (argue: w_b=0 at t≤0.35 exact; at t≥0.65 the formula with −mv_bwd — trace the WAKE/CORE/LEAD
   cells at t=0.8 like STAGE-67''s table, paste the trace). The supervisor''s outdump A/B is the
   acceptance: t≈0.78 dumps must approach t≈0.14 cleanliness.

**Pinned constraints:** framework/ READ-ONLY; main.cpp + shaders/wap_warp.comp ONLY; no commits;
no INVENTORY edits; no memory_order_*; do NOT run the app; preserve the soul lines; kill anything
you spawn. Read the primary-mv resolution site FIRST (search "bilateral" / "fetch" / the mv
assignment all consumers read); if there are MULTIPLE independent primary fetches that cannot be
unified at one site, STOP and report the list instead of improvising.

---

## STAGE-77 kickoff — deep measurement on B v1: the AMBIGUITY channel (the periodic-texture killer + the first B-capacity holonic feed) — tier: OPUS — PILLAR TOUCH

**Decision basis (2026-06-12):** the 1080 Ti (B) runs the pillar optical flow per pair and sits mostly
idle. The remaining artifact class — periodic textures (striped flags, concentric tunnels in the zoo) —
comes from SAD TIES the matcher resolves arbitrarily: best and a runner-up ~one texture period away have
near-tied SADs, and the matcher picks one with no awareness of the other. Production cure: candidate
awareness. STAGE-77 spends B's idle capacity to SHIP the runner-up to the presenter.

**Scope — PILLAR (framework/render/vulkan) + APP (apps/render_assistant) + WARP (shaders/wap_warp.comp):**

1. **PILLAR — second-best output.** `optical_flow_hier_match.comp` + `OpticalFlowPipeline` gain an
   optional SECOND-BEST output: at the FINEST level, emit the runner-up MV + its SAD into one extra
   RGBA16F image (`xy=second MV`, `z=second SAD`, `w=0`). Gate it so existing callers are unchanged: a
   new `init(..., emit_second_best=false)` flag, default OFF; the matcher tracks the runner-up over the
   same scan as best (top-2 by the lambda-biased cost), gated store on a new `int emit_second` push
   (push 12→16B), the binding always present in the layout (a 1×1 RGBA16F placeholder bound when off —
   the placeholder discipline). The pillar test MUST pass unmodified, PLUS a new case asserts the
   second-best on a synthetic periodic pattern where best/second are one period apart with near-tied SADs.

2. **APP — copy out the candidate field per pair.** Enable the flag; copy the candidate field out
   alongside MV/SAD (same hfb-rounded host-bounce discipline as `hostMV` — clone the `hostMV` plumbing:
   `hostC2` → `hC2_b`/`hC2_a` → upload to a new A-side image bound to the warp). F ships it through the
   existing per-gen channel.

3. **WARP — the ambiguity rule.** A new binding (next free slot after 9 = binding 10) samples the
   candidate field. AMBIGUITY: a block is ALIASED when `second_sad ≤ 1.15·best_sad` AND
   `|mv_best − mv_second| > 2px`. For aliased OBJECT pixels (`matte_object`), prefer the candidate
   CLOSER to the holon's expectation — approximate the holon mv at the pixel by the existing
   inheritance-repaired field (the 3×3 median of the primary field / the `guided_mv` result) as the
   referee; pick whichever of best/second is nearer. Push +1 float (`ambig_on`); `--no-ambig` hatch
   default ON; marker ` ambig` in the bidir segment. Byte-identical off (uniform gate; binding always
   valid via the placeholder trick).

4. **GATES:** `optical_flow_test` PASS (all cases incl. the new one); both app toolchains green (MSVC +
   MinGW); zoo run (`--window "RA Gate Zoo" --fg-factor 4`) er=0 + marker; optional `--outdump N` PNG
   reading on the flags/tunnels interiors.

5. **SCOPE VALVE:** if the full chain is too large, STOP after the PILLAR part (second-best output +
   test green) and report — the supervisor stages the rest.

**Pinned constraints:** no commits; no INVENTORY edits unless a file is ADDED; no `memory_order_*`; hfb
discipline on any host import; all shader barriers top-level unconditional; preserve the soul lines; kill
everything you spawn.

**Build/run note (pillar test):** the standalone vulkan-pillar build needs the glfw/imgui include dirs
the top-level FetchContent populated — configure with
`cmake -S framework/render/vulkan -B <build> -G Ninja -DCMAKE_CXX_COMPILER=C:/mingw64/bin/c++.exe -DPHYRIAD_BUILD_TESTS=ON -DCMAKE_CXX_FLAGS="-IG:/phyriad/build/_deps/imgui-src -IG:/phyriad/build/_deps/imgui-src/backends -IG:/phyriad/build/_deps/glfw-src/include"`
then `cmake --build <build> --target optical_flow_test` and run `optical_flow_test.exe` (RTX 4090 picked).
