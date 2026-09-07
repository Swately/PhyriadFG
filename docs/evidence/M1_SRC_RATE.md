# M1 at a 30 fps source — does the 8x multiplier hold POSITIONALLY?

**Date** 2026-09-07 · **Rig** RTX 4090, machine otherwise idle (BF6 closed first; GPU 1 %, 77 W)
**Instrument** the committed `tools/motion_truth/` chain — `marker_zoo.py` -> `play_frames.ps1` ->
`--qdump 48` -> `marker_extract.py` -> `motion_report.py`. Full tables:
[`M1_SRC30.md`](M1_SRC30.md), [`M1_SRC60.md`](M1_SRC60.md).

## The question

[E1d](E1D_FULL_STACK.md) measured that the frame generator sustains **7.5-8.3x** from a 30 fps source
with perfect pipeline health — `uniq 240/s`, `fresh 240/s`, `frz 0.0/s`. It also said plainly what
those metrics do NOT prove: that the interpolated frames are in the RIGHT PLACE. Health says the
frames arrive; M1 says whether they are where they claim to be. M1 had never been run from a 30 fps
source, and the concern was concrete — inter-frame displacement doubles, and the block matcher works
on an 8x8 grid.

## Method

The same trajectory, sampled at two rates. `marker_zoo.py` with identical parameters
(1280x720, 4 s, `--bg noise --bg-pan 0 --seed 20260904`) at `--fps 60` and `--fps 30`: the analytic
p(t) is parameterised in SECONDS, so the 30 fps corpus is the same motion with **twice the
displacement per captured pair**. Played by `play_frames.ps1` at its own rate, captured by the release
binary at defaults via `--window-pid`, 48 triples per run, **two runs per side (DI-3)**.

At 240 Hz output this makes the 60 fps source a **4x** case and the 30 fps source an **8x** case.

Playback fidelity, quoted: `fps=30.0 target=30 missed=0` and `fps=60.1 target=60 missed=0`, all four
runs. Capture-path floor (`marker_extract.py` checks itself on the REAL plane before reporting
anything), all four runs: **0.116 / 0.116 / 0.119 / 0.118 px** mean against its 0.25 px bar — the same
on both sides, which is what says the two conditions were photographed the same way.

## Result — on the classes DI-3 permits citing

The project's own rule: **a class with run-to-run `r` < 0.5 is UNRELIABLE and carries no verdict.**
Two classes clear it on BOTH sides:

| class | source 30 fps (8x) | source 60 fps (4x) | r30 | r60 |
|---|---|---|---|---|
| **circular** | **0.791 px** | **0.777 px** | 0.50 | 0.57 |
| **hud** | **0.206 px** | **0.206 px** | 1.00 | 1.00 |

**Indistinguishable.** Circular differs by 1.8 %; hud is identical to three decimals. **Halving the
source rate — doubling the per-pair displacement — did not degrade positional accuracy on anything
the instrument is willing to stand behind.**

## The four classes that carry no verdict, shown anyway

Each fails `r` on at least one side, so none of these differences may be cited. They are here because
suppressing them would be selecting the result:

| class | 30 fps | 60 fps | r30 | r60 | direction |
|---|---|---|---|---|---|
| linear | 0.633 | 0.690 | 0.42 ✗ | -0.13 ✗ | 30 fps better |
| accel | 2.373 | 1.751 | 0.90 | -0.00 ✗ | 30 fps worse |
| crossing | 1.830 | 1.330 | 0.80 | -0.24 ✗ | 30 fps worse |
| fast | 5.462 | 5.995 | 0.35 ✗ | 0.99 | 30 fps better |

Mixed in direction, and unreliable in both directions. Note the 60 fps side is the one that fails `r`
most often here (four classes at or below zero), which is itself worth a look someday — at 48 triples
the 60 fps condition resolves its own classes worse than the 30 fps condition does.

## The real cost, and it is NOT positional

`phase_ms` — how far in TIME the generated frame sits from where it claims to be — is consistently
about **twice as large** at the halved source rate:

| class | 30 fps | 60 fps |
|---|---|---|
| linear | 8.35 ms | 4.82 ms |
| accel | 11.52 ms | 5.28 ms |
| circular | 9.12 ms | 4.73 ms |
| crossing | 10.09 ms | 5.82 ms |
| fast | 13.69 ms | 7.52 ms |

This is arithmetic, not a defect: `phase_ms = err / |v| * 1000/fps`, so the same pixel error at half
the rate is twice the time. **The same picture error, carrying twice the temporal meaning.** Whether
that reads as worse to a human is a perceptual question this instrument does not answer.

## Verdict

**The 8x multiplier from a 30 fps source is positionally as accurate as 4x from 60 fps**, on the two
classes DI-3 allows a verdict on. The concern that motivated this run — that doubled displacement
would break an 8x8-grid block matcher — is not supported by the measurement.

What this does NOT say: that 30 fps *feels* the same (phase error doubles in time); that the four
unreliable classes are fine (they are unmeasured, not measured-good); or that anything here transfers
to a live game, since this is a synthetic corpus by construction — which is the only reason it has
ground truth at all.

*Made with my soul - Swately <3*
