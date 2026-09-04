# S2.T1c — gate record: the replay record made complete, and audited against the push · 2026-09-03

> Not a planned phase. It exists because scoping T6 began with decoding a real `q*_push.bin` instead of
> trusting the plan's premise, and the premise did not survive that. **Verdict: PASSED** — the record now
> carries every plane the shipping default reads, and the checker proves it per record rather than by
> assertion.

## 1 · The finding that opened it

The motion-truth plan (§2.3) says a replay record is `(prev, next, MV, gme, push, t)`, and T1 built
exactly that. Before writing T6's reference warp I decoded the 58 floats of a live push block from an
actual run, and matched each armed field against the shader's bindings. **The premise is false for the
shipping default.**

| Push field, shipping default | Makes the warp read | In the T1 record? |
|---|---|---|
| `occl_thresh = 1.5`, `phase_anchor_on = 1` | `u_motion_vectors_bwd` (binding 5) | **no** |
| `inertia_thresh = 0.5` | `u_persistence` (8) | **no** |
| `ambig_on = 1`, `gme_on = 1` | `u_candidates` (10) | **no** |
| `vblend_on = 1` | `u_mv_target` (12) | **no** |

Four planes. A reference warp fed the T1 record could not have reproduced a default tick at all, and the
failure would have looked like a bug in the reference rather than a hole in the record.

**The table above is the CORRECTED one.** My first pass wrote six, adding both dissidence masks on the
strength of `gme_on = 1`. Re-reading every `texture(u_dissidence` site and its enclosing gate showed that
claim is false: all the ordinary sites are gated on `matte_on > 0.5` (0 here), the edge-snap G1 variant
guides on the forward mask only when `mv_edge_snap` is armed (0 here), and the reveal-fill site needs
`bg_snap_on` or `band_xfade` (both 0). The masks are dumped anyway — they cost 14,400 B each and a run
that arms matte needs them — but the checker no longer *requires* them for a push that does not read
them, and the corrected gate lives in the table it belongs in.

Three bindings are genuinely NOT read under this default, verified from the same push block rather than
assumed: `u_field` (11), gated on `bg_snap_on` / `disoccl_hardpick` / `mc_on`, all zero; `u_prev_out`
(13), gated on `ts_smooth = 0`; and the two dissidence masks (6, 7) as just described. The shader takes
no backward gme model; the backward fit reaches it only through the backward dissidence mask.

## 2 · What was added

Every remaining per-generation host plane, written on the same synchronous tap as T1's, each only if its
host pointer exists (each is allocated with its feature), and named in the manifest:

| Token | File | Format | Measured |
|---|---|---|---|
| `mvb=` | `q*_mvb.rg16f` | RG16F | 57,600 B = 160 × 90 × 4 |
| `mvt=` | `q*_mvt.rg16f` | RG16F | 57,600 B |
| `c2=` | `q*_c2.rgba16f` | RGBA16F | 115,200 B |
| `dis=` `disb=` `per=` | `q*_{dis,disb,per}.r8` | R8 | 14,400 B each |
| `tgen=` | — | int | the generation bound as `u_mv_target` |

`tgen` is recorded at the `wap_upload()` call site next to `gen`, for the same reason `gen` is: the record
must state which plane binding 12 held, never make the replay infer it. An absent plane is named `-`, so
a record states its own scope instead of being silently partial.

A 16-triple record is 193 files: twelve per triple plus the manifest.

## 3 · The check that now decides whether a record is a corpus

`tools/check_qdump_plus.py` gained a **replayability audit**. It decodes the push block against the
58-float contract, and for each armed feature asserts the plane that feature makes the warp read is in
the record. Run against the pre-T1c record:

```
push contract: 58 floats; armed = residual_ceil=32 improvement_frac=0.2 agreement_threshold=0.05
  soft_gate=1 commit_thresh=0.08 commit_real=1 occl_thresh=1.5 rescue_on=1 mv_guided=1.1 gme_on=1
  ... stasis_thresh=0.5 inertia_thresh=0.5 appear_on=1 appear_band=0.1 phase_anchor_on=1 ambig_on=1
  commit_default_on=1 onepos_on=1 onepos_band=1 vblend_on=1 ... single_track=1 bg_reclaim=4
  NOT REPLAYABLE for this push:
    - backward MV (binding 5) is READ but the record has no `mvb=` plane
    - SAD candidates (binding 10) is READ but the record has no `c2=` plane
    - persistence (binding 8) is READ but the record has no `per=` plane
    - target-generation MV (binding 12) is READ but the record has no `mvt=` plane
```

and against the T1c record from the same source and settings:

```
  q000000: gen=1->tgen=0 mv 160x90 |MV| max=7.51 p99=3.61 px, moving=33.1% MVb max=24.50 MVt max=9.06
           | push 232 B rc=32 improv=0.2 agree=0.05 t=0.1280 (manifest 0.1280) OK
           | planes c2+dis+disb+mvb+mvt+per
  REPLAYABLE: every binding this push arms has its plane in the record.
coverage: 16 triples | t in [0.124,0.892] span=0.768 | distinct t-bins(1/8) = 5 | generations = ['0','1','2']
  phase histogram (bin/8 -> triples): 0:1  1:5  3:3  5:4  7:3
  note: 1 bin(s) seen once ([0]) - the clock acquisition transient, not part of the locked ladder.
```

The audit is per record, so a later run that arms `--ts-smooth` or `--bg-snap` is told, at check time,
that its record is out of scope — instead of that surfacing as an unexplained pixel diff inside M4.

**A second self-inflicted false alarm, fixed.** The balance test first counted the acquisition-transient
bins, which a coverage sampler can dump once and never again, so a record uniform over its locked ladder
read as lopsided. Singleton bins are now reported as the transient they are and excluded from the test.
This is the same class of mistake as T1b's binning mismatch: the instrument disagreeing with the thing it
measured. Both were found by reading the numbers rather than the intent.

## 4 · What this does to R3's M4, stated plainly

The good news is real but narrow. The record can now feed a reference warp for the default. It does not
make the reference warp exist, and T6 is still the gate's other half.

It also sharpens what T6 must implement. Under `single_track = 1.0` — the shipping default — the final
store is `mix(B_samp, cur[uv], w_s)`, with `w_s` from the screen-static evidence ratio and the block
stasis bool, and the whole commit / matte / onepos / blend cascade shadowed. So the reference's real work
is the **effective forward MV**: the guided fetch at `mv_guided = 1.1`, `bg_reclaim = 4`, the phase anchor
against the backward field, and the vblend tilt toward `mvt`. That is a much smaller surface than the
1,336-line shader suggests, and it is exactly the surface the new planes feed.

## 5 · Honesty ledger

- The armed-feature to binding map in the checker is **my reading of the shader's gates**, not a
  generated artefact, and its first version was WRONG about the dissidence masks (§1). Every entry has
  since been checked at each `texture(...)` call site against its enclosing `if`, not inferred from the
  push field's documentation. If R3 changes a gate this table must move with it, and nothing enforces
  that today — the map is the weakest link in the audit and should be treated as such.
- Byte-exactness of the new planes was checked by SIZE and, for the MV planes, by float16 decode and
  magnitude plausibility. The R8 masks and the RGBA16F candidate field were **not** semantically
  validated — only their sizes. A wrong-but-same-size plane would pass.
- Not measured: the added dump cost per tick, and the record's disk footprint pressure. A 16-triple
  720p record is 176 MB, dominated by the three full-res RGBA planes; the six new planes (four required,
  two carried for other feature sets) add about 330 KB per triple, which is noise against that.
- The default's own `mv_edge_snap = 0` means the edge-snap MV path is off in this corpus. A record that
  arms it is still replayable (it reads no new binding), but it exercises code this corpus does not.

*Made with my soul - Swately <3*
