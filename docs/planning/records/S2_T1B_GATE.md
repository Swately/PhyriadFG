# S2.T1b — gate record: the dump sampler, replaced and measured · 2026-09-03

> Opened by [`S2_T0_T1_GATE.md`](S2_T0_T1_GATE.md) §4 as an **R3 precondition**: T1 produced a correct
> replay-record FORMAT, but the record it produced was not a representative corpus. **Verdict: PASSED —
> the precondition is lifted.** Two independent runs, both numbers reported (DI-3).

## 1 · The defect, and why the old fix could not have worked

The stride sampler dumped every `kQdumpStride = 11`th present tick. Its own comment argued 11 was coprime
with the phase steps, so successive dumps would land on different phases. Measured over 16 triples, they
did not: **two** eighth-of-a-pair bins (ten triples at ≈0.125, six at ≈0.375) and **one** ring slot.

The reason is structural, and it means no choice of stride would have fixed it. **The dump stalls its own
tick** — three full-frame image-to-buffer copies plus a fence. The content clock recovers from that stall
the same way every time, so the phase N ticks after a dump is a deterministic function of the *stall*, not
of *N*. The sampler was synchronised with its own perturbation. Changing 11 to any other constant moves
the pinned phase; it does not unpin it.

## 2 · What replaced it

A **coverage sampler**. On every candidate tick, classify the tick by its phase bin (eighths of a pair,
truncated) and its generation ring slot, then dump only if:

| Condition | Why |
|---|---|
| the tick's phase bin is the least-covered bin **that has actually been seen** | never waits for a bin this refresh ratio cannot emit |
| its ring slot is likewise least-covered, **or** 64 candidates have been skipped | spreads across the ring; the escape hatch means the two conditions cannot deadlock each other |
| at least 8 ticks since the last dump | the recorded phase is a settled phase, not the clock's recovery transient |

Dumping a bin makes that bin ineligible until the others catch up. That is the property the old sampler
lacked and the one pinning cannot survive. The checker bins with the same truncation the sampler uses, so
the two agree by construction rather than by coincidence.

The manifest now carries a `#` line naming the sampler and its three constants, so a record says how it
was collected. The block stays inside the existing `qdump_left > 0` guard: with `--qdump` absent, nothing
runs. Six stack variables are declared unconditionally, exactly as the old counters were.

## 3 · Measured — two independent runs, both reported

Ball zoo at 60 fps on a 240 Hz panel, `--qdump DIR 16 --exit-after 45`, checked with
`tools/check_qdump_plus.py`.

| | phase bins | per-bin spread | ring slots | `t` span |
|---|---|---|---|---|
| **before** (stride 11) | 2 of 8 | 10 / 6 | 1 of 3 | 0.255 |
| **run A** (coverage) | **8 of 8** | **2 each** | **3 of 3** | 0.900 |
| **run B** (coverage) | **4 of 4 reachable** | **4 each** | **3 of 3** | 0.780 |

```
run A: coverage: 16 triples | t in [0.092,0.992] span=0.900 | distinct t-bins(1/8) = 8 | generations = ['0', '1', '2']
       phase histogram (bin/8 -> triples): 0:2  1:2  2:2  3:2  4:2  5:2  6:2  7:2
run B: coverage: 16 triples | t in [0.126,0.906] span=0.780 | distinct t-bins(1/8) = 4 | generations = ['0', '1', '2']
       phase histogram (bin/8 -> triples): 1:4  3:4  5:4  7:4
```

**Run-to-run spread and what it is.** The bin count differs, 8 against 4. That is not sampler variance.
Run B's phases sit on the locked ladder — 0.126, 0.379, 0.632, 0.885 and repeats — because **a locked
ladder at panel/source = 4 emits exactly four phases**. Run A had a longer clock-acquisition transient,
whose irregular phases filled the other four bins. In both runs the sampler covered **every phase the
ladder produced, uniformly**, which is the strongest available result. The ceiling on phase resolution is
the refresh ratio, not the sampler; a finer sweep needs a different source rate, and that is a
corpus-design choice for R3, not a defect here.

**A self-inflicted false alarm, recorded.** The checker's first version binned with `round(t*8)` while the
sampler truncates. That made run A read as "lopsided, 1..4 per bin" and put a bin 8 in an eight-bin
histogram. Aligning the checker to the sampler's truncation showed the true 2-per-bin uniformity. The
lopsided reading was my measuring instrument disagreeing with the thing it measured.

## 4 · Honesty ledger

- Both runs are on the synthetic ball zoo, one ball on a static lattice. Coverage of *phase* and *ring
  slot* is what was measured. Coverage of **content** was not: whether the sampler now also spreads across
  scene conditions is untested and is a T2–T5 question.
- The 8-tick gap and the 64-skip escape are chosen, not derived. They are stated in the manifest so a
  record carries its own sampling parameters; no measurement here justifies those exact values.
- Not measured: the sampler's own cost. It is a handful of comparisons on a tick that already stalls for
  three full-frame copies, so it is not plausibly significant, but it was not timed.
- `--qdump` still forces the synchronous present path (`resolve_config` auto-disables `--async-present`),
  so this corpus describes the sync path. Unchanged by T1b, and still true.

*Made with my soul - Swately <3*
