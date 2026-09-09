# GDUMP_PLAN — `--gdump`, the every-tick capture tap (plan + strategies + RISK_REGISTER, one document)

> **Diátaxis type:** Explanation + plan + the Tier-2 **RISK_REGISTER** in one document (PLAN_TIER_PROTOCOL
> §1.1 triggers 1, 2 and 5 apply: a change to the present stage's command stream and fence/semaphore
> lifecycle, a new cross-thread ring, and the byte-identical-off dogma). **Status:** `measured` — designed
> 2026-09-09 morning, operator's word the same day ("Lo pendiente utiliza tu recomendacion y continua"), built
> and gated the same afternoon (`records/GDUMP_GATE.md`: G0–G5; every register row `mitigated` or `accepted`
> with its evidence beside it). The first live pass found two defects the 25-reader panel had not (CR3, PR2 —
> `docs/LEARNING_LOG.md` P-024); the rows carry both. **No commit while any row is `open`** — none is.
> **Serves:** `tools/scene_truth/` — the operator's exact-truth instrument — which today can only score the
> frames `--qdump` SAMPLES (8 phase bins, ≥ 8 ticks apart, the synchronous path only). The operator's question
> (2026-09-09): *"no podriamos hacer un hilo paralelo … que sirva justo para la captura … y que la captura no
> viva dentro de la propia generacion?"*
> **Design provenance:** the D1 design was refuted by a 22-agent panel (7 refuters, 14 independent
> verifiers, all CONFIRMED, 1 completeness critic) and every correction below was then re-derived first-hand by
> the supervising session against the code. Digests: the session's `tool-results/bbajbc0we.txt` (refutations)
> and `bztkf67mz.txt` (verdicts + gaps); the workflow journal `wf_d21daf4e-88b/journal.jsonl`.

## §0 — What the tap is, and what it is not

**Is:** an INSTRUMENT-plane consumer of `GenFrame` (STAGE_CONTRACT §4) that copies **every recorded warp
output** of the **shipping asynchronous path** into a host ring, from which a dedicated writer thread streams
it to disk, together with everything `tools/ref_warp.py` needs to replay the tick byte-exactly. Real frames
and flow planes are captured **once per pair**. The present thread never waits on the tap.

**Is not:** a replacement for `--qdump` (which stays, as the sampler and the cross-check), a change to any
computed value, or a path that exists when the flag is off. **Off = byte-identical** (strategy S7).

**Why the observer effect is not zero and how it is measured (the operator's question, answered honestly):**
the copy is one more command on the tick's queue. Its cost is bounded by the copy's own GPU time and shows up
in three places, each of which the FG already measures: the tick's GPU batch (`--warp-timing`), the
pair-advance upload wait (`--wsub` `up`, strategy S3), and the async drop counter (`rdrop_ticks`). Gate G2
measures all three in a **three-arm** run, because arming the tap also changes creation-time state (usage
bits, the timeline feature) that could perturb the kernel independently of the copy.

## §1 — Mechanism (the corrected D1, one paragraph per moving part)

1. **Staging.** `N` host buffers for the generated frame (`WW_warp × WH_warp` RGBA8; default `N = 64`) and
   `M` pair sets (default `M = 4`), each `_aligned_malloc` + `hbuf_import` on device A — the existing
   host-buffer discipline (`vk_util.hpp:45-50`, `warp_blend_init.cpp:238-266`). Allocation size is rounded
   up to `host_align`; **the logical size is what is written** (gap 6).
2. **Per recorded tick, on P.** Before recording, the tap reports whether a staging slot is free: the slot
   index is `ring.write_cursor() & (N-1)` — **from the ring, never from the timeline counter** (P4-2,
   CONFIRMED twice). Inside the tick's command buffer the copy `wapOutA → staging[slot]` is recorded while
   `wapOutA` is `TRANSFER_SRC_OPTIMAL`: on the hand-written path between the blit and the back-barrier
   (`present.cpp:1296-1300`); on the `--sg-barriers` path **inside the blit's record lambda** immediately
   after `vkCmdBlitImage` (`present.cpp:1278-1280`), because the seam graph emits the RAW barrier, the pass
   and the epilogue in one `execute()` and culls any pass that writes no marked output (P2-1). The submit
   gains a second timeline signal `semCapTL = ++cap_seq`; `cap_seq` advances on **every** recorded submit so
   the timeline is strictly increasing (P3-4 rejected; P4-2 adopted). After the submit P pushes a 64-byte
   descriptor into `phyriad::ipc::Ring<Desc, N>` (SPSC; P is the sole producer). The push block `pcw` is
   memcpy'd into a per-slot side buffer at record time. **P never calls `vkWaitSemaphores` on `semCapTL`.**
3. **Per pair, on P.** At the pair-advance site (`present.cpp:2517-2523`, right after `wap_upload` returns)
   the tap memcpy's the **host** planes of the uploaded generation into a free pair set — `hostSAD` (required
   unconditionally by `ref_warp.py:438`), `hostC2`, `hostDIS`, `hostDISB`, `hostPER`, `hostMV[target_gen]`
   (mvt), `hostMV[gen]` (mv), `hostMVB[gen]` (mvb), `f_pair_gme_a` + `_valid` — because F rewrites those
   slots and nothing guards a deferred reader (P5-3). It then marks the pair **pending**. The **GPU** planes
   — `wapPrevA`, `wapCurA` (WW×WH) and the post-consensus `wapMVA`/`wapMVBA` — are copied inside the **first
   tick that records** after the upload (the upload tick itself can be a Drop with no command buffer, P5-5 /
   P7-3), with the RO→TRANSFER_SRC→RO barrier pairs of the qdump oneshot (`present.cpp:1418-1437`). If a new
   upload fires while a pair is still pending, the pending set is **discarded and counted** (`pair_clobbered`,
   P7-3's corrected fix): its GPU images no longer hold that pair.
4. **The writer thread** (`src/instrument/gdump.cpp`, one thread, created when the tap arms): peek the next
   descriptor, `vk_wait_sem_live(semCapTL ≥ cap_seq)` (the bounded, device-loss-aware wait of
   `globals.cpp:43`), write the frame from `staging[slot].mapped` straight to the stream (no memcpy), write the
   index line, write the pair set if the descriptor carries one, **then** pop (the pop is what frees the slot
   and the pair set). Ring empty → spin → pause → yield → sleep (the 15-line backoff, inline; `transport/` is
   not vendored). On `g_quit` it drains what has completed and exits; the tap's destructor joins it **before**
   `run_present` returns, i.e. before `vkDeviceWaitIdle`/`vdev_destroy` in `main.cpp:1112/1274`.
5. **Ring full / pair set busy → skip and count**, never wait. The record names what is missing (§2).
6. **What is refused** (resolve-time, printed, the tap disarms, the run proceeds as asked): `--afill` and
   `--fps-overlay` write `wapOutA` in place before the blit and are not in the push (gap 2); `--ts-smooth`
   makes ticks depend on the previous output (gap 3). `--gdump` does **not** force the synchronous path.

## §2 — The record (the contract the module, the adapter and the tools build against)

Directory `<dir>/`, created by the tap. All text files are UTF-8, `\n`, space-separated tokens, `#` comments.

| File | Written by | Content |
|---|---|---|
| `gdump.hdr` | P at arm | `gdump 1` · `size WW WH` · `live_div D WW_warp WH_warp` (**always**, even D=1) · `mv mvw mvh` · `push_bytes B` (= `sizeof(pcw)`) · `kernel fg_core\|wap_warp` · `contract 0x…` · `bidir 0\|1` · `xfer 0\|1` · `async 0\|1` · `ring N pairs M` · `qpc_hz F` · `afill 0` `fps_overlay 0` `ts_smooth 0` (the refused flags, recorded as 0 by construction) |
| `live.rgba` | writer | the generated frames, appended in capture order, **exactly** `WW_warp·WH_warp·4` bytes each |
| `push.bin` | writer | `push_bytes` per captured frame, same order (the `pcw` shadow — what `ref_warp.py` consumes; under `fg_core` the GPU received `FgPush`, the header's `kernel` line says so, P6-5) |
| `ticks.tsv` | writer | one line per **captured** frame: `seq slot tick t gen tgen pair decision flags qpc live_off push_off pairset` — `seq` = `cap_seq` of the tick, `pair` = the monotone pair id (`pair_c`, the capture sequence of the pair's B frame; **never** `f_gen`, which is a ring slot mod 3, P6-4), `decision` ∈ `warp` (only warps are captured), `flags` bits: `PAIR` (this tick carried the pair copies of `pair`), `XFER`, `SG`, `QDUMP` (see `qdump_xref.tsv`) |
| `pairs.tsv` | writer | one line per pair whose planes were written: `pair gen tgen seq_recorded gme_valid g0 g1 g2 g3 g4 g5 prev next mv1 mvb1 sad c2 dis disb per mvt mv mvb` — file names, or `-` for a plane absent in this configuration (bidir off, ambig off, …); `sad` is never `-` (a pair with no SAD is not written and is counted) |
| `pairs/p<pair>_<plane>.<ext>` | writer | `prev.rgba` `next.rgba` (WW×WH), `mv1.rg16f` `mvb1.rg16f` `sad.rg16f` `mvt.rg16f` `mv.rg16f` `mvb.rg16f` (mvw×mvh×4), `c2.rgba16f` (×8), `dis.r8` `disb.r8` `per.r8` (×1) — the qdump plane names, so the adapter is a renaming |
| `qdump_xref.tsv` | P (sync path only) | `seq qdump_idx` whenever `--qdump` dumped on a captured tick — the cross-check key (P7-6; qdump's manifest has no tick) |
| `summary.txt` | P at stop, after the writer joined | the reconciliation: `ticks T` · `recorded R` · `captured C` · `ring_full F` · `drop Dr` `dup Du` `decimated De` `real Re` (non-record ticks by decision) · `pairs_uploaded Pu` `pairs_written Pw` `pairs_busy Pb` `pairs_clobbered Pc` · `writer_timeouts W` · `bytes B` · `total_presents TP` (the FG's own counter) · the identity `C + F = R` and the note that `interp=` in the exit line is presents − ingested reals, not a generated count (P7-1) |

Reader contract: `np.memmap(live.rgba, u8, shape=(C, WH_warp, WW_warp, 4))`; `tools/scene_truth/gdump_adapter.py`
turns `<dir>` into a `--qdump`-shaped `manifest.txt` + per-triple files on demand (a renaming plus a slice), so
`scene_align.py`, `ref_warp.py`, `check_qdump_plus.py` and `marker_extract.py` consume it unchanged (gap 5).

## §3 — Strategies (edit sites; each cites the risk it mitigates)

- **S1 — Vendor the ring** [BR1]: `framework/ipc/include/phyriad/ipc/Ring.hpp` copied verbatim from
  `F:\Phyriad\catalog\cpp\ipc\include\phyriad\ipc\Ring.hpp` (its two includes resolve against the vendored
  `hal/`, verified byte-identical); `CMakeLists.txt:324-330` gains `"${_fw}/ipc/include"`; `gdump.cpp` joins
  the source list. `framework/` is the repo's own copy, not the container (P4-5, CONFIRMED twice).
- **S2 — The flag + the refusals** [DR1, DR2]: `cli.hpp` gains `gdump_dir[256]`, `gdump_ring=64`,
  `gdump_pairs=4`; `cli.cpp` parses `--gdump <dir>`, `--gdump-ring N`, `--gdump-pairs N` in the `--qdump`
  block's style; `resolve_config()` refuses `afill`/`fps_overlay`/`ts_smooth>0` by disarming the tap with a
  printed line, and leaves `async_present` alone.
- **S3 — The upload wait is measured, not hidden** [CR4]: on the shipping default (`upload_xfer=false`) the
  pair-advance `wap_upload` host-waits on `A.q` (`present.cpp:808`) behind an ALL_COMMANDS barrier, so it now
  also waits for the previous tick's copy (P1-3, CONFIRMED). This is accepted as a bounded cost and measured
  by the `up` segment of `--wsub` in G2; if the measured delta matters, the escape is D3 (a device-local shadow
  ring read back in batches on `A.qT`), not a second queue over the EXCLUSIVE `wapOutA` (critic, contradiction 1).
- **S4 — Timeline feature + the usage bits, gated** [CR1, CR2, CR5]: `device.cpp:189` becomes
  `want_ts = (want_xfer_q && qfamT!=MAX) || want_timeline` with a new `vdev_create` parameter fed from
  `core_init.cpp:127` (`cfg.gdump_on`), plus a physical-device `apiVersion ≥ 1.2` guard (P3-2's unverified
  note). `warp_blend_init.cpp:104-108` ORs `VK_IMAGE_USAGE_TRANSFER_SRC_BIT` into `wPrev/wCur/wMV/wMVB` when
  `cfg.qdump_n>0 || cfg.gdump_on` — which also brings today's `--qdump` copies inside the spec (P2-3:
  `VUID-vkCmdCopyImageToBuffer-srcImage-00186`, quoted from the installed SDK's `validusage.json`).
- **S5 — The five sites in `present.cpp`** [CR2, CR3, CR6, RR1]: (a) the tap is constructed in the pre-loop
  block next to the qdump state (`:835`), from `A`, the dims and `use_bidir`; (b) in the record block, before
  the blit, `slot = tap.begin_tick()`; the copy inside the blit lambda / between `:1299` and `:1300`; the pair
  copies after the blit block when `tap.pair_pending()`; (c) the submit chain: `pSignalSemaphores` becomes a
  2-array `{semWarpTL?, semCapTL}` with the matching values, chained through the same
  `VkTimelineSemaphoreSubmitInfo` (which is now chained whenever `xfer_on || tap`); (d) after `pres.submit`,
  `tap.on_submitted(...)` with the tick's scalars and `pcw`; (e) at `:2517-2523`, `tap.on_pair_upload(...)`
  with the host planes; (f) on every tick that does not record, `tap.count(decision)`; (g) in the qdump block,
  `tap.note_qdump(qdump_idx)`.
- **S6 — The module** [CR3, RR1, RR2, DR3]: `src/instrument/gdump.hpp` (the interface, written by the
  supervisor) + `gdump.cpp` (the bodies: staging, the ring, the writer, the files). Pure-CPU bookkeeping
  (`GdumpBook`: cursors, counters, index formatting, the reconciliation identity) is separable from the Vulkan
  glue (`GdumpTap`) so `tests/instrument/test_gdump_book.cpp` can gate it without a GPU.
- **S7 — Byte-identical off** [DR1]: every allocation, the semaphore, the thread and every edit site are
  behind `cfg.gdump_on`; the `VkTimelineSemaphoreSubmitInfo` chaining condition is `xfer_on || tap.armed()`
  so an unarmed run chains exactly what it chains today.
- **S8 — The adapter** [DR3]: `tools/scene_truth/gdump_adapter.py --dir <dir> --out <qdump-shaped dir>
  [--seq a:b]` — writes a `manifest.txt` with `size`, `live_div` and one `triple` line per captured tick in
  the exact token order `present.cpp:1513-1531` writes, materialising `q%06d_{prev,next,live}.rgba` and the
  plane files by slicing the stream and linking the pair files. `scene_live.ps1` gains `-Gdump` to run the
  tap instead of (or beside) `--qdump`.

## §4 — RISK_REGISTER (mitigation as code; first-hand verification; nothing commits with an `open` row)

| ID | Class | Failure mode (site) | Mitigation (code) | Verification | Status |
|---|---|---|---|---|---|
| **CR1** | crash | timeline feature not enabled → `vkCreateSemaphore(TIMELINE)` / `vkWaitSemaphores` undefined (`device.cpp:189`) | `want_ts \|\| want_timeline` + `d.has_timeline`; apiVersion ≥ 1.2 guard; the tap disarms with a printed line when the feature is absent (`gdump.cpp` ctor) | 2026-09-09, `--validation --gdump --exit-after 5` on the new binary: **0 spec messages** (the 4 matching lines are the FG's own `--validation`/`--gdump` prints); the guard path is unreachable on this rig (4090 = 1.3) and stays code-inspected | **mitigated** |
| **CR2** | crash | copy from an image without `TRANSFER_SRC` usage (`warp_blend_init.cpp:104-108`; VUID 00186) | usage bit ORed at creation under `qdump_n>0 \|\| gdump_on` | 2026-09-09 validation layer, three 5 s runs: **base binary + `--qdump`: 25 validation lines, 10 × `VUID-vkCmdCopyImageToBuffer-srcImage-00186` (the red, pre-existing since --qdump was written); new binary + `--gdump`: 0; new binary + `--qdump`: 0** (`C:\PhyriadFG\runs\g5_live\validation\*.log`) | **mitigated** |
| **CR3** | crash / hang | writer blocked in `vkWaitSemaphores` when the device is lost or the process quits; semaphore destroyed under it — **and (SEEN RED 2026-09-09, observer run gdump r2) a signal still pending on `semCapTL` when the destructor destroyed it:** the signal rides EVERY recorded submit but a ring-full tick pushes no descriptor, so when the last ticks of a run are ring-full nobody waits their values and `vkDestroySemaphore` runs out of spec — the exit wedged in the driver after the tap's own summary line (r1, whose last ticks were captured, exited clean) | `vk_wait_sem_live` (20 ms slices, `g_device_lost` + `g_quit` latches); dtor joins before destroying; the tap is a `run_present` local so it dies before `main.cpp:1112`; **`stop()` now waits `semCapTL ≥ cap_seq_` (bounded) after the join and before any destroy** (`gdump.cpp`); the observer runner waits the FG with a 30 s bound and names a kill | the r2 hang reproduced the failure (PID 18032 alive, 0 CPU, blocked, no `done` line); **after the fix, the re-run (17:08): all 8 runs, both gdump arms included, printed `done (...)` and `bounded-run clean exit`; G4's and G5's runs likewise; no leftover process** | **mitigated** |
| **CR4** | concurrency | the pair-advance upload's host wait now covers the previous copy (`present.cpp:808`, P1-3) | accepted as bounded; **measured** (G2 `up` delta) | G2 re-run: `wsub up` base 0.31 / off 0.31 / nocopy 0.31 / gdump 0.32 ms (per-run means 0.32 / 0.32; the per-second r is ~0 for every arm, so only the means are quoted) → **+0.01 ms per pair-advance, at the spread** | **accepted** (the residual is 0.01 ms on the default path; the operator's word covers it) |
| **CR5** | dogma | arming changes the object under test (usage bits, feature chain; critic gap 4) | G2 is three-arm: off / armed-no-copy (`--gdump-ring 0`) / armed | G2 re-run (`records/GDUMP_GATE.md`): **nocopy vs base: fps +0.10, warp −0.12 ms, gpu A −0.07 %** — the creation-time changes cost nothing measurable; the copy alone (gdump − nocopy) is +0.06 ms of GPU batch (`wt gpu` 0.03 → 0.09), +0.25 ms warp EMA, +1.3 % GPU util, 0 rdrops | **mitigated** |
| **CR6** | concurrency | WAR/RAW on `wapOutA` between the copy and the next warp or `--ts-smooth` (`present.cpp:1300`, `:1310-1316`) | the copy sits inside the existing barrier pair (manual) or the blit pass (sg); `--ts-smooth` refused | validation clean on the default (hand-written) path with `--gdump`; the `--ts-smooth --gdump` refusal shares RR6's code path (the same `resolve_config` line); **the `--sg-barriers` path is NOT validation-run (opt-in, off by default) — code-inspected only: the copy sits inside the blit lambda that the graph's RAW barrier precedes and its epilogue follows** | **accepted** (sg path inspected, not run; run it with `--sg-barriers --gdump --validation` before flipping `--sg-barriers` to default) |
| **CR7** | concurrency | queue-family ownership under `--upload-xfer` for the pair images (CONCURRENT) and `wapOutA` (EXCLUSIVE) | all copies stay on `A.q`; no new family touches any image (P2-5 holds) | **not run** (`--upload-xfer` is opt-in and off by default); code-inspected: every tap copy is recorded into the tick's own `A.q` command buffer | **accepted** (same condition as CR6: a validation run before `--upload-xfer` is ever defaulted) |
| **RR1** | data | slot reuse: a copy lands on a slot the writer still streams (P4-2) | slot from `write_cursor()`; the writer pops only after its writes; producer-side `size() < N` before recording (`gdump_book.cpp` `begin_tick`) | `test_gdump_book` T1 (2026-09-09, `pfg_gdump_test.exe` exit 0): the N=2/W=3 interleaving — the cap_seq counter-model overwrites a streaming slot (red), `GdumpBook::begin_tick()` never does (green); T3 the identity, seen red under a corrupted counter | **mitigated** |
| **RR2** | data | a pair's GPU planes captured under the wrong pair id after a re-upload (P7-3) | `GdumpBook::on_pair_upload` discards a still-pending set (`pairs_clobbered`) and re-pends; the descriptor carries `pair` (`pair_c`) and `pairset` | `test_gdump_book` T2 (2026-09-09): upload(10) → no record → upload(11) → `pairs_clobbered==1`, the claimed set holds 11; all M sets busy → `pairs_busy==1` | **mitigated** |
| **RR3** | data | host planes read late race F's `kGenRing` writer (P5-3) | memcpy on P at the upload site (`present.cpp` S5e), never deferred | code inspection (the memcpy is inside the pair-advance block, before `wap_pair_c_up` is set); G5: `check_qdump_plus.py` over 1,805 adapted ticks reports `REPLAYABLE` and `RESULT: all checks passed`, and `ref_warp.py` fed the record's `mv1`/`mvb1` reproduces the live frames at **99.50 % exact, 100.00 % within 1 LSB, no refusals** over 20 ticks — a stale or torn plane would not | **mitigated** |
| **RR4** | data | `ref_warp.py` cannot load the record (no header, no dims, `sad` missing, gen ambiguity; P6-2/3/4) | §2's header + `pair` id + unconditional SAD; the adapter (`gdump_adapter.py`, `mv` also required) | G5 (2026-09-09): adapter `1805 triples written, 0 skipped`; `check_qdump_plus.py`: `pushsz constant = 232 B`, `REPLAYABLE`, `all checks passed`; `ref_warp.py --triples 20 --mv-plane mv1 --mvb-plane mvb1`: `RESULT: no refusals` | **mitigated** |
| **RR5** | data | logical vs rounded size written (gap 6) | the writer writes `WW_warp·WH_warp·4` and the plane sizes from the header, never the buffer size | G3: `bytes 3523241904 = 3822 × 921600 + 3822 × 232` in the first pass; G4: every joined frame `921600 bytes` and byte-identical to qdump's | **mitigated** |
| **RR6** | data | in-place writers of `wapOutA` contaminate the record (gap 2) | `afill`/`fps_overlay`/`ts_smooth` refused in `resolve_config()` (`cli.cpp`): the tap disarms, the run proceeds as asked | 2026-09-09: `phyriad_fg.exe --gdump <d> --afill --dump-config` prints `[ra] --gdump: refused with --afill / --fps-overlay / --ts-smooth ... the tap is DISARMED for this run.` and exits 0; the header writes the three as 0 by construction (`gdump.cpp`) | **mitigated** |
| **DR1** | dogma | off is not byte-identical | S7; `--dump-config` identical except the three new fields; G1 against the pre-change binary | G0: `--dump-config` diff = `sizeof(Config) 1824→2088` only, hash `0x9517AE73A530EAFE` on both; `--help` diff = the `--gdump` lines only; flag round-trip 274 tokens identical + 3 new; G1 (re-run, 2 × 20 s each, interleaved with base): **off − base = fps +0.06, warp −0.08 ms, fresh −0.10/s, `up` 0.00, gpu +0.18 % — inside base's own repeat spread** (`records/GDUMP_GATE.md`) | **mitigated** |
| **DR2** | dogma | the tap forces the sync path like `--qdump` | it does not touch `async_present` | the gdump arms' stats lines carry `fresh:…/s` (the async fresh counter) and the exit line's `interp=` matches an async run; `cli.cpp`'s qdump auto-disable is untouched | **mitigated** |
| **DR3** | data | the tool contract breaks (gap 5) | the adapter emits the qdump layout; nothing downstream changes | G5 (2026-09-09): `scene_live.ps1 -Gdump` → adapter → `scene_align.py` (180 of 180 mids) → `scene_report.py` produced `g5_live/fg_k4.md` (pos 0.240 px, one run, reliability not measured); `check_qdump_plus.py` and `ref_warp.py` consumed the adapted dir unchanged (`records/GDUMP_GATE.md`) | **mitigated** |
| **PR1** | performance | the copy pushes the batch past the tick period → chronic rdrop (P1-2) | measured; the stop rule: if `rdrop/s` rises above the untapped baseline the tap is not the instrument at that resolution — the escape is D3 | G2 re-run at 640×360 @ 240 Hz: **rdrop 0.00/s on every arm**, fresh 241.85 vs 241.91/s, presents 4449–4454 vs 4457–4459 per 20 s (−0.1 %); the panel-native (1080p) size is NOT measured — the corpus is 640×360 and PR2's arithmetic says 1080p @ 240 exceeds C:'s steady write rate | **accepted** (640×360 measured; 1080p deferred to its own run, expected to be a counted sampler) |
| **PR2** | performance | the disk cannot take the rate (critic gap 1) — **and (MEASURED 2026-09-09, first observer pass) the OS write cache, not the disk:** with a 64-slot ring the writer stalled in two bursts of 0.73 s and 0.26 s at 16.5 s and 17.8 s into a 20 s run (`ticks.tsv` gaps of 153 and 53 ticks; `ring_full` 236 / 222 of ~4,000 recorded = 6 %, `pairs_busy` ~100) while the bytes were 170 MB/s against a measured 1.3–1.9 GB/s | measured 2026-09-09: F: 124–138 MB/s (**cannot** take 640×360@240 = 221 MB/s); C: 980 PRO 1.3–1.9 GB/s; runs live under `C:\PhyriadFG\runs\`; `ring_full` counts what the disk dropped; **`live.rgba` and the pair reals now bypass the cache (`FILE_FLAG_NO_BUFFERING`, when the frame is a sector multiple from a sector-aligned slot — 640×360 and 1080p both are), the default ring is 256 (1.07 s at 240 fps) and the pair sets 16** | after the fix, four 640×360 runs on C: — observer r1 `captured 4449 / recorded 4449 / ring_full 0`, observer r2 `4455 / 4455 / 0`, G4 `4507 / 4507 / 0`, G5 `1805 / 1805 / 0` — 3.9 GB per 20 s written with `writer_timeouts 0`; `pairs_busy` 19 of 1,199 in one run (r1), 0 in the other three; the 1080p case is NOT run (its arithmetic: 2.0 GB/s > the 980 PRO's 1.4–1.9 GB/s steady state → a counted sampler); 12 files/pair measured at 3.4 ms/pair (294 pairs/s capacity vs 55 needed — not the bottleneck, kept) | **mitigated** at 640×360; 1080p **accepted** as a counted sampler until measured |

## §5 — Gates (each first-hand; a gate binds to the exact claim it tested)

- **G0 — build ×2 + the CPU tests** (`pfg_gdump_test` green after being seen red on RR1/RR2) + `--help`
  round-trip + `--dump-config` diff = the new fields only.
- **G1 — byte-identical off**: the 120 s smoke on the default; rates inside the noise of the pre-change binary.
- **G2 — the observer, three arms** (operator's screen): `sc_live` corpus at 640×360, k=4, 20 s, off /
  `--gdump-ring 0` / `--gdump`, two runs each: present fps, `warp` ms, `--warp-timing` gpu/lat, `--wsub`
  `up`, `rdrop/s`, `fresh/s`, r per number (DI-3).
- **G3 — captured = recorded**: `summary.txt`'s `C + F = R` holds and `F = 0` at 640×360 on C:.
- **G4 — the cross-check**: `--qdump <d> 8 --gdump <d2>` in one (sync) run; for every `qdump_xref` pair the
  qdump `live` bytes equal the stream frame **byte-for-byte**.
- **G5 — the tools**: the adapter's output passes `check_qdump_plus.py`; `scene_align.py` + `scene_report.py`
  produce an `fg_k4` row from a `--gdump` run; `ref_warp.py` replays ≥ 20 adapted ticks with its existing gate.

## §6 — Honest expectation, and what was measured

**Expected (morning):** at 640×360 the copy is ~0.04 ms of a 4.17 ms tick (scaled from the measured 0.32 ms at
1080p on this 4090, `stage2_image_xfer`), the disk has 6× headroom, and the record should be complete
(`ring_full = 0`). The two real costs are the pair-advance `up` wait every k ticks (CR4, bounded, measured) and
the creation-time perturbation (CR5, isolated by the third arm). At 1080p @ 240 the disk is at its steady-state
limit and the tap becomes a counted sampler; that is reported, not hidden. The kernel's computed values do not
change in any arm — the tap reads, it never writes an image the kernel reads.

**Measured (afternoon, `records/GDUMP_GATE.md`):** the copy is **0.06 ms** of GPU batch per tick (`wt gpu` 0.03 →
0.09 ms), the `up` wait grew **0.01 ms**, the creation-time changes cost **nothing measurable** (nocopy = base
inside the spread), rdrops stayed at **0.00/s**, presents fell **0.1 %**, GPU utilisation rose **1.3 points**;
`ring_full = 0` on every run after PR2's fix, with 3.9 GB written per 20 s. The expectation was right on the
copy and on the disk's bytes and wrong on the OS: the write cache, not the disk, was the first-pass bottleneck
(0.26–0.73 s flush stalls), and a signal nobody waited for wedged one exit (CR3). Both are fixed and both have
their rows. The record's fidelity to the qdump sampler is byte-exact (G4: 16 of 16), and the oracle replays it
at 99.50 % exact / 100 % within 1 LSB (G5) — the same numbers the sampler's record gave. What the tap adds over
the sampler is COVERAGE: 180 of 180 possible mids of the corpus in one 8 s run, against ~133 in 20 s before.

*Made with my soul - Swately <3*
