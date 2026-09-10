# 10. Reproducibility anchor

<!-- KAP Phase 2 stub (SCAFFOLD.md, section 11 of 11). Every slot below is EMPTY; Phase 3 fills the ones marked EXISTS -->

## RA.1 — Code: the repository, the tool files, the kernel builds each row names (`f2e4a9a`, `0df0332` + md5)

*serves:* ET · OB · *evidence:* the identity block; `GDUMP_GATE.md` G0 · *status:* `EXISTS`

**Tool files** (identity block): `tools/scene_truth/scene_zoo.py` — the scenario set; `tools/scene_truth/scene_report.py`
— the six verdict terms and the rulers.

**Builds**, quoted first-hand (`git log`): `f2e4a9a 2026-09-08 fix(scene_truth): the barcode strip out of EVERY term,
and a live arm scored at its own exact phase` — the synchronous `--qdump` rows of `B1_FIRST_FG_ROW.md` and
`B1_SPEED_TEST.md` (identity block); `0df0332 2026-09-09 feat(instrument): --gdump, the every-tick capture tap of the
shipping async path (Tier-2, gated)` — "md5 `186CB46C` at the gate; base `364efc5`, md5 `F36FDE32`" for the
`--gdump` rows of `GDUMP_GATE.md` (identity block).

**G0** (`GDUMP_GATE.md` header, G0): base = HEAD `364efc5`, md5 `F36FDE32`; new = the working tree, md5 `186CB46C`
(header). "build ×2 (`build-release.bat`) | clean, `LTO/IPO enabled`; `100% tests passed out of 51`" (G0).

## RA.2 — Seeds and corpora: the seeds per row, the corpus manifests and their paths, what is KEEP

*serves:* RE · ET · *evidence:* `C:\PhyriadFG\runs\*` manifests; `REGIME_TEST_MATRIX.md` §9 · *status:* `EXISTS`

**Seeds.** Base 240 fps, seed 7 (seed 11 for the k = 4 DI-3 point) (`B1_SPEED_TEST.md` header).

**Corpus manifests** (`C:\PhyriadFG\runs\<corpus>\manifest.txt`): `sc_live2`'s names `scene=mixed seed=11 ss=3`,
`size 640 360`, `base_fps 240`, `frames 240`, `sequence frames/f_ 240`, `fps 240`
(`C:\PhyriadFG\runs\sc_live2\manifest.txt`); its per-k manifest names each held-out triple by `prev` / `mid` / `next`
and `phase` — "held-out triples for multiplier 4: source = every 4-th base frame"
(`C:\PhyriadFG\runs\sc_live2\manifest_k4.txt`).

**What is KEEP.** "Live corpora (`sc_live`, `sc_live2`, `sc_v05`, `sc_v2`, `sc_v4`) with their `qdump_k*/` and
`arms/fg_k*/` are marked KEEP under the session scratchpads; the FG output arms are the irrecoverable part"
(`B1_SPEED_TEST.md` §5); a run marked KEEP refuses an overwrite unless `-Overwrite` is passed
(`REGIME_TEST_MATRIX.md` §8).

## RA.3 — Commands: render, capture, align, score (with `--jobs` and `--silhouette`), gate — one per arm

*serves:* ET · *evidence:* `REGIME_TEST_MATRIX.md` §8 · *status:* `EXISTS`

**Per arm** (`REGIME_TEST_MATRIX.md` §8; family 0, the default arm). Capture: `tools\scene_truth\scene_live.ps1 -Run
C:\PhyriadFG\runs\sc_live2 -K 4 -Gdump -Loop -Tag async`. Align: `scene_align.py`, which "files cuts apart" (§9).
Read the arm: `scene_step.py` / `scene_review.py` on `arms/fg_k4_async`; `scene_pages.py add-scene` (§8). The
scoring itself is `scene_report.py --run <corpus> --k 4 --arm <arm>` — `--run` "twice for DI-3", `--arm` "arm dir
name(s) under <corpus>/arms/", `--jobs` defaulting to the core count minus 2, `--silhouette {tau,coverage}` defaulting
to `tau` (`scene_report.py:769-782`). Gate, before any
capture: `scene_report.py --run <corpus> --k 4 --gate`, CPU, parallel, must print `GATE PASSED (T1..T6)` (§8);
pooled at `--jobs 30`, ~4 min each corpus (§9).

Render precedes capture — "Every corpus below is rendered under `C:\PhyriadFG\runs\` (CPU only, no screen was used)"
(§8).

**Flags.** Extraction and scoring run "in parallel (`--jobs`, cores − 2)" (§8); the scorer's `--silhouette coverage`
operator replaced the default `tau` for the re-scored speed rows (§9).

## RA.4 — Hardware: the GPU and CPU per record — the B1 records name no GPU (to be quoted from the rig before the anchor is filled; `M1_SRC_RATE.md` line 3 names the RTX 4090)

*serves:* ET · *evidence:* `docs/evidence/M1_SRC_RATE.md`; `docs/planning/records/QOL_FINDINGS.md` lines 136–143 · *status:* `EXISTS`

The B1 records name no GPU: `B1_SPEED_TEST.md`'s header states Date, Tools, Binary, Scene, Protocol — no hardware
field (`B1_SPEED_TEST.md` header).

`M1_SRC_RATE.md` line 3 names the rig for the marker-chain instrument: "**Rig** RTX 4090, machine otherwise idle
(BF6 closed first; GPU 1 %, 77 W)" (`M1_SRC_RATE.md` line 3).

`QOL_FINDINGS.md` (lines 133–145): "The rig enumerates three GPUs to Windows but only **two to Vulkan**";
`nvidia-smi -L -> GPU 0: NVIDIA GeForce RTX 4090 (one line, the only line)`; "The GTX 1080 Ti is in driver error 31"
("Windows cannot load the drivers required for this device"), invisible to Vulkan.

THE RIG, quoted first-hand by the supervisor (`nvidia-smi -L` / `Get-CimInstance`, 2026-09-10): `GPU 0: NVIDIA
GeForce RTX 4090 (UUID: GPU-25e32071-8ef4-1185-c7f7-42cb0cac5220)`, driver `610.88`, `24564 MiB`; CPU `AMD Ryzen 9
7950X3D 16-Core Processor`.

**Inference, labelled as such:** the rig had one GPU visible to PhyriadFG at the time (`QOL_FINDINGS.md` lines
136–139) and the FG therefore ran on the RTX 4090 — an inference, not measured.

*Made with my soul - Swately <3*
