# RESTRUCTURE E0 — raw delegate capture tables (I2 / I3)

**Provenance banner (read first):** this file is the verbatim row-level output of two delegated
read-only analyses (SUBAGENT_DELEGATION_PROTOCOL R6: load-bearing exchanges captured as
artifacts). Model: sonnet-class, 2026-08-31, supervised by the session that wrote
[`RESTRUCTURE_INVENTORY.md`](RESTRUCTURE_INVENTORY.md). Status of every row: **claim,
spot-verified only at the §0 samples recorded there** — a convenience starting point, NOT an
authority. Rule DR1 stands: each row consumed by stage E2/E3 is re-verified against the source
when the binding struct is written. Line numbers reference commit `c043780`.
I1/I4 raw outputs are NOT kept here: I1 was corrected first-hand (its raw table has known
omissions/misattributions — see INVENTORY §0/§1), I4 is fully integrated in INVENTORY §4.

---

## I2 raw — `present.cpp` `run_present` (alias preamble 56–235)

### bridge_present (466–507)

| Identifier | Class | Declared at line |
|---|---|---|
| `cfg` | ALIAS | 58 |
| `ph_tgt` | LOOP-LOCAL | 459 |
| `ph_w_ema` | LOOP-LOCAL | 459 |
| `ph_held` | LOOP-LOCAL | 460 |
| `ph_overshoot` | LOOP-LOCAL | 460 |
| `bridge_nt` | ALIAS | 142 |
| `bridge_w` | ALIAS | 147 |
| `bridge_h` | ALIAS | 137 |
| `ps_account` | LOOP-LOCAL (lambda variable) | 440 |
| `ra_surface` | LOOP-LOCAL | 382 |
| `surface_ready` | LOOP-LOCAL | 383 |

### bridge_present_src (654–693)

| Identifier | Class | Declared at line |
|---|---|---|
| `surface_ready` | LOOP-LOCAL | 383 |
| `cmdBridge` | ALIAS | 222 |
| `Apresent` | ALIAS | 130 |
| `pres_w` | ALIAS | 185 |
| `pres_h` | ALIAS | 184 |
| `cfg` | ALIAS | 58 |
| `ov_ready` | LOOP-LOCAL | 609 |
| `ov_pipeline` | LOOP-LOCAL | 602 |
| `ov_pl_layout` | LOOP-LOCAL | 601 |
| `ov_set` | LOOP-LOCAL | 602 |
| `kOverlayW` | namespace constexpr (`present.hpp:25`) | no capture |
| `kOverlayH` | namespace constexpr (`present.hpp:26`) | no capture |
| `bridge_img` | ALIAS | 220 |
| `badge_buf` | LOOP-LOCAL | 574 |
| `badge_dim` | LOOP-LOCAL | 575 |
| `badge_margin` | LOOP-LOCAL | 575 |
| `fBridge` | ALIAS | 223 |
| `A` | ALIAS | 80 |
| `bridge_use_km` | ALIAS | 146 |
| `bridge_mem` | ALIAS | 221 |
| `bridge_present` | OTHER-LAMBDA (called at 692) | 466 |

### wap_upload (713–836)

Delegate note (spot-verified): line 724 reads the ALIAS `cmdBridge` (preamble 222) to pick
`ucmd`; line 735 declares a local `VkCommandBuffer cmdBridge = ucmd;` that shadows the alias for
the rest of the body ("shadow: keep the large record body below textually unchanged").

| Identifier | Class | Declared at line |
|---|---|---|
| `xfer_on` | ALIAS | 219 |
| `uslot` | ALIAS | 198 |
| `cmdUpload` | ALIAS | 151 |
| `cmdBridge` | ALIAS | 222 (read once at 724, pre-shadow) |
| `cfg` | ALIAS | 58 |
| `wapFIELDA` | ALIAS | 204 |
| `hFIELD_a` | ALIAS | 161 |
| `WW` | ALIAS | 77 |
| `WH` | ALIAS | 78 |
| `wapPrevA` | ALIAS | 212 |
| `hR_a` | ALIAS | 79 |
| `wapCurA` | ALIAS | 201 |
| `wap_mvw` | LOOP-LOCAL | 706 |
| `wap_mvh` | LOOP-LOCAL | 706 |
| `wapMVA` | ALIAS | 205 |
| `hMV_a` | ALIAS | 164 |
| `wapMVTA` | ALIAS | 208 |
| `wapSADA` | ALIAS | 214 |
| `hSAD_a` | ALIAS | 170 |
| `use_bidir` | ALIAS | 92 |
| `wapMVBA` | ALIAS | 206 |
| `hMVB_a` | ALIAS | 163 |
| `use_ambig` | ALIAS | 95 |
| `wapC2A` | ALIAS | 200 |
| `hC2_a` | ALIAS | 157 |
| `use_gme` | ALIAS | 93 |
| `wapDISA` | ALIAS | 202 |
| `hDIS_a` | ALIAS | 160 |
| `wapDISBA` | ALIAS | 203 |
| `hDISB_a` | ALIAS | 159 |
| `use_inertia` | ALIAS | 90 |
| `wapPERA` | ALIAS | 210 |
| `hPER_a` | ALIAS | 167 |
| `use_mv_median` | ALIAS | 194 |
| `use_mv_guided` | ALIAS | 193 |
| `medPipe` | ALIAS | 181 |
| `wapMVScratchA` | ALIAS | 207 |
| `A` | ALIAS | 80 |
| `fBridge` | ALIAS | 223 |
| `xfer_U` | ALIAS | 217 |
| `xfer_W` | ALIAS | 218 |
| `uslot_val` | ALIAS | 199 |

### wap_warp_present (879–1402)

Delegate note (spot-verified): lines 927–930 declare LOCAL `cmdBridge/fBridge/bridge_img/
bridge_mem` derived from the captured `bslot` — not the same-named preamble aliases.

| Identifier | Class | Declared at line |
|---|---|---|
| `cfg` | ALIAS | 58 |
| `gov_self_distress` | LOOP-LOCAL | 390 |
| `async_inflight` | LOOP-LOCAL | 877 |
| `A` | ALIAS | 80 |
| `bslot` | LOOP-LOCAL | 867 |
| `hostMassPtr` | ALIAS | 172 |
| `async_front` | LOOP-LOCAL | 876 |
| `rdrop_ticks` | LOOP-LOCAL | 385 |
| `devMass` | ALIAS | 152 |
| `wapPipeA` | ALIAS | 211 |
| `use_bidir` | ALIAS | 92 |
| `use_matte` | ALIAS | 192 |
| `use_ambig` | ALIAS | 95 |
| `use_wap` | ALIAS | 94 |
| `use_commit_default` | ALIAS | 190 |
| `use_onepos` | ALIAS | 195 |
| `WW_warp` | ALIAS | 136 |
| `WH_warp` | ALIAS | 135 |
| `use_fill_div` | ALIAS | 191 |
| `use_rescue` | ALIAS | 196 |
| `use_mv_guided` | ALIAS | 193 |
| `use_inertia` | ALIAS | 90 |
| `fillPipeA` | ALIAS | 156 |
| `ov_ready_wap` | LOOP-LOCAL | 609 |
| `ov_pipeline` | LOOP-LOCAL | 602 |
| `ov_pl_layout` | LOOP-LOCAL | 601 |
| `ov_set_wap` | LOOP-LOCAL | 608 |
| `kOverlayW` / `kOverlayH` | namespace constexpr (`present.hpp:25–26`) | no capture |
| `hMass_a` | ALIAS | 165 |
| `wapOutA` | ALIAS | 209 |
| `bridge_w` | ALIAS | 147 |
| `bridge_h` | ALIAS | 137 |
| `bridge_use_km` | ALIAS | 146 |
| `xfer_on` | ALIAS | 219 |
| `xfer_U` | ALIAS | 217 |
| `xfer_W` | ALIAS | 218 |
| `wapPrevOutA` | ALIAS | 213 |
| `outdump_left` | LOOP-LOCAL | 841 |
| `outdump_idx` | LOOP-LOCAL | 841 |
| `hostOutD` | ALIAS | 173 |
| `hOutD_a` | ALIAS | 166 |
| `qdump_left` | LOOP-LOCAL | 848 |
| `qdump_tick` | LOOP-LOCAL | 848 |
| `qdump_idx` | LOOP-LOCAL | 848 |
| `qdump_man_open` | LOOP-LOCAL | 848 |
| `hostPrevD` | ALIAS | 174 |
| `hostCurD` | ALIAS | 171 |
| `wapPrevA` | ALIAS | 212 |
| `wapCurA` | ALIAS | 201 |
| `hPrevD_a` | ALIAS | 168 |
| `hCurD_a` | ALIAS | 158 |
| `WW` | ALIAS | 77 |
| `WH` | ALIAS | 78 |
| `warp_div` | ALIAS | 215 |
| `kQdumpStride` | LOOP-LOCAL | 849 |
| `sq_hits` | LOOP-LOCAL | 878 |
| `sq_misses` | LOOP-LOCAL | 878 |
| `ps_account` | LOOP-LOCAL (lambda variable) | 440 |
| `ra_surface` | LOOP-LOCAL | 382 |
| `surface_ready` | LOOP-LOCAL | 383 |
| `bridge_present` | OTHER-LAMBDA (called at 1389, non-async path) | 466 |
| `w_rec_ema` | LOOP-LOCAL | 859 |
| `w_gpu_ema` | LOOP-LOCAL | 859 |
| `w_prs_ema` | LOOP-LOCAL | 859 |

### rfp_present (1416–1458)

| Identifier | Class | Declared at line |
|---|---|---|
| `surface_ready` | LOOP-LOCAL | 383 |
| `cfg` | ALIAS | 58 |
| `async_inflight` | LOOP-LOCAL | 877 |
| `A` | ALIAS | 80 |
| `bslot` | LOOP-LOCAL | 867 |
| `async_front` | LOOP-LOCAL | 876 |
| `Apresent` | ALIAS | 130 |
| `pres_w` | ALIAS | 185 |
| `pres_h` | ALIAS | 184 |
| `hR_a` | ALIAS | 79 |
| `bridge_w` | ALIAS | 147 |
| `bridge_h` | ALIAS | 137 |
| `bridge_use_km` | ALIAS | 146 |
| `ps_account` | LOOP-LOCAL (lambda variable) | 440 |
| `ra_surface` | LOOP-LOCAL | 382 |
| `bridge_present` | OTHER-LAMBDA (called at 1453, non-async path) | 466 |

### I2 dependency summary

`bridge_present` = shared sink (called by `bridge_present_src` 692, `wap_warp_present` 1389,
`rfp_present` 1453). `wap_upload` depends on no sibling.

---

## I3 raw — `flow.cpp` `run_flow` (alias preamble 476–590)

Delegate scope notes (spot-verified): `kObj*/kChamf*` constants + `kPriorDecay` are
`static constexpr` in `flow.hpp` (33/54/68…) — no capture. `kTier4DwellPairs` is a run_flow
LOCAL (694). Struct definitions `ObjCluster` (791), `ObjSlot` (807), `WakeRec` (826) live inside
`run_flow` and must move with the extraction.

### object_repair (885–1245)

Parameters excluded: `mv_field dis_mask model6 slots persist_p persist_mut span_local adv_sign
out_live out_rep out_infill wake_out wake_n_out`.

| Identifier | Class | Declared at line |
|---|---|---|
| `cfg` | ALIAS | 476 |
| `mvw_f` | ALIAS | 492 |
| `mvh_f` | ALIAS | 493 |
| `obj_label` | FLOW-LOCAL | 787 |
| `obj_bfs` | FLOW-LOCAL | 788 |
| `obj_clusters` | FLOW-LOCAL | 792 |
| `obj_used` | FLOW-LOCAL | 793 |
| `obj_rowmin` | FLOW-LOCAL | 795 |
| `obj_rowmax` | FLOW-LOCAL | 795 |
| `obj_chamf` | FLOW-LOCAL | 803 |
| `obj_feat` | FLOW-LOCAL | 804 |

### mem_advect (1259–1274)

| Identifier | Class | Declared at line |
|---|---|---|
| `mvw_f` | ALIAS | 492 |
| `mvh_f` | ALIAS | 493 |
| `mem_adv` | FLOW-LOCAL | 817 |
| `mem_prior` | FLOW-LOCAL | 816 |

### mem_merge (1290–1310)

| Identifier | Class | Declared at line |
|---|---|---|
| `mvw_f` | ALIAS | 492 |
| `mvh_f` | ALIAS | 493 |
| `cfg` | ALIAS | 476 |

### mem_refresh (1324–1380)

| Identifier | Class | Declared at line |
|---|---|---|
| `mvw_f` | ALIAS | 492 |
| `mvh_f` | ALIAS | 493 |
| `mem_adv` | FLOW-LOCAL | 817 |
| `mem_prior` | FLOW-LOCAL | 816 |
| `obj_rowmin` | FLOW-LOCAL | 795 |
| `obj_rowmax` | FLOW-LOCAL | 795 |
| `obj_label` | FLOW-LOCAL | 787 |

### consume_wap (1396–1809)

Parameters excluded: `pc allow_bwd`.

| Identifier | Class | Declared at line |
|---|---|---|
| `mvw_f` | ALIAS | 492 |
| `mvh_f` | ALIAS | 493 |
| `use_inertia` | ALIAS | 498 |
| `hostMV` | ALIAS | 532 |
| `hostSAD` | ALIAS | 533 |
| `hostPER` | ALIAS | 534 |
| `hostDIS` | ALIAS | 535 |
| `hostDISB` | ALIAS | 536 |
| `hostMVB` | ALIAS | 537 |
| `hostGmeM` | ALIAS | 538 |
| `hostGmeMB` | ALIAS | 539 |
| `hMVB_b` | ALIAS | 547 |
| `hDISB_b` | ALIAS | 549 |
| `hGmeMB_b` | ALIAS | 551 |
| `f_pair_cseq_a` | ALIAS | 557 |
| `f_pair_slot_a` | ALIAS | 558 |
| `f_pair_tcap_a` | ALIAS | 559 |
| `f_pair_span_a` | ALIAS | 560 |
| `f_pair_n_a` | ALIAS | 561 |
| `f_pair_gme_a` | ALIAS | 562 |
| `f_pair_gme_valid_a` | ALIAS | 563 |
| `f_pair_gme_bwd_a` | ALIAS | 564 |
| `f_pair_mfwd_a` | ALIAS | 565 |
| `f_pair_mbwd_a` | ALIAS | 566 |
| `f_pair_disp_a` | ALIAS | 567 |
| `f_pair_bwd_valid_a` | ALIAS | 568 |
| `f_seq` | ALIAS | 569 |
| `f_cv` | ALIAS | 570 |
| `src_interval_us` | ALIAS | 572 |
| `present_cost_us` | ALIAS | 573 |
| `stat_tier` | ALIAS | 574 |
| `stat_bwd_skips` | ALIAS | 575 |
| `stat_cons` | ALIAS | 578 |
| `stat_flow_us` | ALIAS | 579 |
| `stat_pair_us` | ALIAS | 580 |
| `gme_fit_us` | ALIAS | 581 |
| `gme_dis_x100` | ALIAS | 582 |
| `obj_live` | ALIAS | 583 |
| `obj_rep_x10` | ALIAS | 584 |
| `live_n_atomic` | ALIAS | 585 |
| `lt_fpub_us` | ALIAS | 590 |
| `cfg` | ALIAS | 476 |
| `WW` | ALIAS | 484 |
| `WH` | ALIAS | 485 |
| `A` | ALIAS | 486 |
| `B` / `FD` | ALIAS | 490 |
| `flow_div` | ALIAS | 496 |
| `use_objects` | ALIAS | 499 |
| `use_memory` | ALIAS | 500 |
| `use_bidir` | ALIAS | 501 |
| `use_gme` | ALIAS | 502 |
| `use_gme_gpu` | ALIAS | 503 |
| `use_nvofa` | ALIAS | 505 |
| `ofp` | ALIAS | 510 |
| `Cinterp` | ALIAS | 512 |
| `nvofa` | ALIAS | 514 |
| `gmePipe` | ALIAS | 515 |
| `Bframe` | ALIAS | 517 |
| `cmdB_bwd` | ALIAS | 521 |
| `fB2` | ALIAS | 527 |
| `c_slots` | ALIAS | 479 |
| `persist` | FLOW-LOCAL | 749 |
| `t_pair_ema` | FLOW-LOCAL | 685 |
| `bwd_skipping` | FLOW-LOCAL | 685 |
| `pressure_tier` | FLOW-LOCAL | 693 |
| `holon_pair_ctr` | FLOW-LOCAL | 693 |
| `tier4_dwell` | FLOW-LOCAL | 694 |
| `kTier4DwellPairs` | FLOW-LOCAL | 694 |
| `flow_submit_q2_chain` (lambda) | FLOW-LOCAL | 650 |
| `flow_submit_nowait` (lambda) | FLOW-LOCAL | 632 |
| `flow_downsample` (lambda) | FLOW-LOCAL | 706 |
| `gme_vfy_dis` | FLOW-LOCAL | 677 |
| `gme_vfy_n` | FLOW-LOCAL | 678 |
| `mem_prior` | FLOW-LOCAL | 816 |
| `mem_adv` | FLOW-LOCAL | 817 |
| `mv_audit_left` | FLOW-LOCAL | 760 |
| `mv_audit_stat` (lambda) | FLOW-LOCAL | 761 |
| `obj_slots_fwd` | FLOW-LOCAL | 808 |
| `obj_slots_bwd` | FLOW-LOCAL | 808 |
| `objdump_left` | FLOW-LOCAL | 754 |
| `objdump_idx` | FLOW-LOCAL | 754 |
| `objdump_grid` (lambda) | FLOW-LOCAL | 773 |
| `wake_n` | FLOW-LOCAL | 828 |
| `wake_rec` | FLOW-LOCAL | 827 |
| `gme_fit_ema` | FLOW-LOCAL | 673 |
| `gme_sub2` | FLOW-LOCAL | 673 |
| `gme_fits` | FLOW-LOCAL | 673 |
| `gme_fit_printed` | FLOW-LOCAL | 673 |
| `obj_cost_ema` | FLOW-LOCAL | 850 |
| `obj_pairs` | FLOW-LOCAL | 850 |
| `obj_settle_printed` | FLOW-LOCAL | 850 |
| `t_fuse_ema` | FLOW-LOCAL | 668 |
| `t_flow_ema` | FLOW-LOCAL | 686 |
| `span_ema` | FLOW-LOCAL | 667 |
| `dwell_sets` | FLOW-LOCAL | 666 |
| `live_n_f` | FLOW-LOCAL | 661 |
| `up_streak` | FLOW-LOCAL | 666 |
| `deg_streak` | FLOW-LOCAL | 666 |
| `t_warp_ema` | FLOW-LOCAL | 668 |
| `object_repair` | OTHER-LAMBDA (called 1607, 1721) | 885 |
| `mem_advect` | OTHER-LAMBDA (called 1592) | 1259 |
| `mem_merge` | OTHER-LAMBDA (called 1593, 1714) | 1290 |
| `mem_refresh` | OTHER-LAMBDA (called 1736) | 1324 |

### I3 dependency summary

`consume_wap` calls all four leaves; none calls back. It additionally needs `flow_submit_nowait`
(1531), `flow_submit_q2_chain` (via `nvofa_run`, 1499), `flow_downsample` (1509).
