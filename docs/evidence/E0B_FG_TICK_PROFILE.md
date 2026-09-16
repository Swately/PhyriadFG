# E0B — where the FG's own tick goes, idle and under contention

> **This page is DATA, not an OAP O0.** The optimization protocol's first O0 on this project is the
> operator's to define — tier, instrument, corpus. This is input for that decision, measured because he
> asked whether an optimization pass would move performance enough to pause other work. It does not
> claim the protocol's vocabulary.

**Date:** 2026-09-15 · **Rig** RTX 4090, single-GPU to PhyriadFG (the 1080 Ti is in driver error 31)
**Binary** `build-release/phyriad_fg.exe` at `1ec408f`, contract `0xBF27BBBA9109A3E3`, shipping default
**Instrument** the FG's own `--wsub` (per-tick warp-lambda sub-timings) and its `--fg-prebake`
descriptor-count instrument · **Competitor** `tools/gpu_load.exe --gpu 0 --load 99 --heavy 4`, the same
one E0 used · **Corpus** `sc_live/source_k4` (640×360, shown at 60 fps, `--fg-factor 4`)
**Runs** 8 — two per arm, 20 s each, the first 3 stats lines of every run discarded (the clock is still
locking and the EMAs are still filling)

---

## 1. The method note that decides the result

This does **not** capture through `--qdump`. The dump stalls its own tick on purpose — `present.cpp:830`
calls it "three full-frame copies + a fence" — so a profile taken through it measures the instrument and
not the FG. The runs here drive the FG against the player window with no tap at all; its stdout is the
only thing read.

## 2. The split

`warp` is the per-tick lambda and decomposes into `rec` + `gpu` + `prs`. `up` is timed at its own call
site (the pair-advance upload is a distinct submit, outside the lambda's window), so the tick total
below is `up + rec + gpu + prs`.

| arm | fps | tick | up | rec | gpu | prs | lat | util A |
|---|---|---|---|---|---|---|---|---|
| idle, default | 240.0 | 4.41 ms | 7.5 % | 0.5 % | **0.6 %** | **91.4 %** | 13.2 ms | 18 % |
| idle, `--fg-prebake` | 240.0 | 4.26 ms | 9.6 % | 0.5 % | 0.6 % | 89.3 % | 13.1 ms | 23 % |
| saturated, default | 110.9 | 11.55 ms | **31.3 %** | 0.2 % | **0.3 %** | 68.3 % | 103.8 ms | 100 % |
| saturated, `--fg-prebake` | 110.6 | 11.54 ms | 33.1 % | 0.2 % | 0.3 % | 66.5 % | 104.2 ms | 100 % |

In milliseconds, the two terms an optimization pass would act on: `rec` = 0.02 ms and `gpu` = 0.03 ms,
in every arm. **Together 1.1 % of the tick idle and 0.5 % saturated.**

## 3. What the time actually is

**`prs` tracks the frame period.** 4.03 ms against a 4.17 ms period at 240 fps; 7.88 ms against a
9.02 ms period at 110.9 fps. Two points, so this is an inference and not a fit — but tracking the period
is the signature of a blocking present, not of computation. There is no kernel here to make faster.

**`up` is the one term that explodes under contention:** 0.332 → 3.616 ms, **×10.9**. It is well outside
its own noise — the two runs give 0.331 / 0.332 ms idle (spread 0.3 %) and 3.655 / 3.577 ms saturated
(spread 2.1 %). This is the only specific optimization target the profile identifies, and it is one
submit rather than a body of code.

**Contention costs the FG what E0 predicted it would.** 240.0 → 110.9 fps and 13.2 → 103.8 ms of
latency, with device A pinned at 100 % by the competitor.

## 4. `--fg-prebake` cannot arm on the shipping default

The flag pre-bakes the matcher/warp descriptor sets so `record_optical_flow` skips the per-pair
`vkUpdateDescriptorSets` burst (~41 host calls per record, ~82 per pair). Its instrument prints a
lifetime count at teardown on **every run this project makes**, inviting someone to read the armed
delta. Armed, the FG answers:

```
INERT (eligibility not met) (fg_variant_active=0, use_ambig=1) — per-pair vkUpdateDescriptorSets burst unchanged.
```

Its own help states the condition — `fg_variant active + --no-ambig + no affine` — and the shipping
default violates two thirds of it. The two armed arms above therefore compared the baseline against
itself, and every delta landed inside the noise band, which is what an inert flag predicts. The
descriptor counts confirm it: 99,220 → 99,220 idle, identical.

## 5. What this does NOT claim

- **It is 640×360.** A 1920×1080 source is nine times the pixels and the 0.6 % GPU slice is the one
  number here that is resolution-bound. The conclusion that kernel optimization is not where the time
  is holds **for this corpus** and has not been tested at 1080p.
- It says nothing about quality. `pos_err`, `halluc` and `sharp` are accuracy at a fixed configuration;
  no optimization moves them unless it changes the output, and an optimization that changes the output
  invalidates the canonical record (the mechanism of P-045).
- It is two runs per arm. The run-to-run spreads are printed beside every quantity that carries a claim.

*Made with my soul - Swately <3*
