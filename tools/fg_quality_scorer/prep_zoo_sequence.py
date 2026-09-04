#!/usr/bin/env python3
# prep_zoo_sequence — the CONTINUOUS held-out test-field source for fg_quality_scorer.
#
# HSR120_THROUGHPUT_DIAGNOSIS.md §11 + the VFI-QA SOTA recipe (held-out-frame
# protocol — Middlebury / Vimeo90K / Adobe240 / MSU VFI Benchmark): the rigorous,
# automated, camera-free way to measure frame-interpolation quality is to take a
# DETERMINISTIC high-rate source, DECIMATE it (drop every other frame), have the
# interpolator reconstruct the dropped frame from its two neighbours, and compare
# against the REAL frame you withheld. The withheld real IS the ground truth — for
# free, no camera, no manual slow-mo decomposition. This script manufactures that
# source as a CONTINUOUS sequence (the operator's "extensión de tiempo": n frames
# recorded digitally, decomposed into many held-out triples) over SEVERAL motion
# types AT ONCE (the "varios ángulos de forma simultánea").
#
# WHY synthetic + procedural (not a captured clip): held-out needs EXACT ground
# truth at the temporal midpoint. A procedural scene under a KNOWN continuous
# camera path gives a genuine render at every t — frame 2j+1 is the true midpoint
# of frames 2j and 2j+2 (the path is sampled uniformly in t), so it is a valid
# held-out target. It is also deterministic (byte-identical every run → no
# nondeterminism confound) and resolution-agnostic (the budget north star:
# 1280x720 → 1920x1080 → DSR all use the SAME instrument). The CONTENT is
# synthetic but the MOTION is controlled, which is exactly what isolates "how good
# is OUR flow+warp" from "what was on screen".
#
# Each preset is a CONTINUOUS sequence  <name>_000000.rgba .. <name>_{T-1}.rgba
# (raw RGBA8, row-major, W*H*4, NO header — the scorer's reader). The manifest uses
# the scorer's `sequence <name> <T>` directive, which expands it into the held-out
# triples (prev=2j, mid=2j+1 HELD OUT, next=2j+2).
#
# Motion presets (the "zoo" of motion the interpolator must survive):
#   pan      — steady translation (the EASY case; a translational warp should ace it)
#   pan_diag — diagonal translation (both axes)
#   accel    — ACCELERATING translation: per-step motion GROWS across the segment
#              (the budget-relevant case — motion magnitude is the load variable)
#   zoom     — continuous zoom: NON-translational, a per-tile translational model
#              cannot fully reconstruct → the crossfade/ghost-prone case
#   orbit    — rotation about centre: non-translational
#   mixed    — pan + zoom + rotation together (the HARDEST case)
#   occlude  — THE CROSSFADE-CAUSE: a static textured bg with an OPAQUE, faint-textured
#              disc translating across it → aperture (low-gradient interior the block-
#              match can't lock) + occlusion/disocclusion (uncovered bg has no match) →
#              the symmetric-blend-of-misaligned ghost (dossier §3 F8). The camera presets
#              cannot show this (value-noise = well-posed flow everywhere); measure it with
#              dbl_edg_m + PSNR (the localized-artifact detectors), not flowdsc alone.
#
# Usage:
#   python prep_zoo_sequence.py [--width 1280] [--height 720] [--frames 40] [--out DIR]
#   ./build-fgq/fg_quality_scorer <DIR>/manifest.txt -1 zoo.csv
#
# Writes into ./zoo_seq/ by default (git-ignored). Disk: ~W*H*4 * frames * presets.

import argparse
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))


# ── procedural scene (deterministic, rich edges + clear object silhouettes) ──────
def build_scene(w, h):
    """A deterministic RGBA8-ready float scene (h,w,4) in [0,255]. Rich high-
    frequency edges (where flow+warp ghosts) + a few high-contrast foreground
    shapes (clear silhouettes for the object/deformation metrics)."""
    yy, xx = np.meshgrid(np.arange(h, dtype=np.float32),
                         np.arange(w, dtype=np.float32), indexing="ij")
    cx, cy = (w - 1) * 0.5, (h - 1) * 0.5

    # background: DETERMINISTIC NON-REPEATING value-noise ⊕ a smooth gradient.
    # WHY non-repeating (not a checkerboard/sinusoid): a PERIODIC texture makes the
    # block-match genuinely AMBIGUOUS (aperture problem — equally-good matches one
    # period apart, each with LOW SAD), which the SAD-confidence gate cannot remove
    # and which confounds flow_disc as a motion-type-difficulty signal (the verified
    # pan_diag inflation). Value-noise gives every tile a UNIQUE match → the flow is
    # well-posed, so flow_disc reflects the MOTION's difficulty, not texture ambiguity.
    # (Periodic-content robustness is a separate axis — a future dedicated preset.)
    rng = np.random.default_rng(7)                       # fixed seed → byte-identical every run
    coarse = rng.random((max(2, h // 6), max(2, w // 6)), dtype=np.float32)
    # upsample (bilinear via np.kron-free interp) for smooth, trackable, non-repeating detail.
    ys = (np.linspace(0, coarse.shape[0] - 1, h)).astype(np.float32)
    xs = (np.linspace(0, coarse.shape[1] - 1, w)).astype(np.float32)
    y0 = np.floor(ys).astype(np.int64); x0 = np.floor(xs).astype(np.int64)
    y1 = np.clip(y0 + 1, 0, coarse.shape[0] - 1); x1 = np.clip(x0 + 1, 0, coarse.shape[1] - 1)
    fy = (ys - y0)[:, None]; fx = (xs - x0)[None, :]
    top = coarse[y0][:, x0] * (1 - fx) + coarse[y0][:, x1] * fx
    bot = coarse[y1][:, x0] * (1 - fx) + coarse[y1][:, x1] * fx
    noise = top * (1 - fy) + bot * fy                    # (h,w) in [0,1], smooth + non-repeating
    gx = xx / (w - 1); gy = yy / (h - 1)                 # gentle large-scale gradient
    r = 30.0 + 170.0 * noise + 25.0 * gx
    g = 35.0 + 160.0 * noise + 25.0 * gy
    b = 45.0 + 150.0 * (1.0 - noise) + 25.0 * (1.0 - gx)
    scene = np.stack([r, g, b, np.full_like(r, 255.0)], axis=-1)

    # foreground shapes (baked into the scene; the camera path moves them rigidly).
    # Each is a saturated solid → strong silhouette the gravity/IoU metrics track.
    def disc(px, py, rad, col):
        m = ((xx - px) ** 2 + (yy - py) ** 2) <= rad * rad
        scene[m] = col + [255.0]

    def rect(px, py, rw, rh, col):
        m = (np.abs(xx - px) <= rw) & (np.abs(yy - py) <= rh)
        scene[m] = col + [255.0]

    disc(w * 0.30, h * 0.40, min(w, h) * 0.075, [240.0, 30.0, 30.0])   # red disc
    disc(w * 0.68, h * 0.62, min(w, h) * 0.060, [30.0, 230.0, 90.0])   # green disc
    rect(w * 0.50, h * 0.30, w * 0.045, h * 0.060, [40.0, 90.0, 245.0])  # blue bar
    rect(w * 0.78, h * 0.30, w * 0.030, h * 0.090, [245.0, 220.0, 30.0]) # yellow bar
    disc(w * 0.22, h * 0.74, min(w, h) * 0.045, [235.0, 30.0, 230.0])  # magenta disc
    return scene


def sample_bilinear(src, xs, ys):
    """Bilinear sample (clamp-to-edge — matches the shader sampler)."""
    h, w = src.shape[0], src.shape[1]
    x0 = np.floor(xs).astype(np.int64); y0 = np.floor(ys).astype(np.int64)
    x1 = x0 + 1; y1 = y0 + 1
    fx = (xs - x0)[..., None]; fy = (ys - y0)[..., None]
    x0 = np.clip(x0, 0, w - 1); x1 = np.clip(x1, 0, w - 1)
    y0 = np.clip(y0, 0, h - 1); y1 = np.clip(y1, 0, h - 1)
    p00 = src[y0, x0]; p01 = src[y0, x1]; p10 = src[y1, x0]; p11 = src[y1, x1]
    top = p00 * (1 - fx) + p01 * fx
    bot = p10 * (1 - fx) + p11 * fx
    return top * (1 - fy) + bot * fy


def render_frame(src, tx, ty, zoom, theta_deg):
    """Render the scene under an ABSOLUTE camera transform (translation (tx,ty),
    zoom about centre, rotation theta_deg about centre). Inverse map output→source
    is identical to stage11/prep_frames.py::render_at (verified): subtract the
    translation, inverse-zoom about centre, inverse-rotate by -theta."""
    h, w = src.shape[0], src.shape[1]
    cx, cy = (w - 1) * 0.5, (h - 1) * 0.5
    yy, xx = np.meshgrid(np.arange(h, dtype=np.float32),
                         np.arange(w, dtype=np.float32), indexing="ij")
    sx = xx - tx; sy = yy - ty
    sx = cx + (sx - cx) / zoom; sy = cy + (sy - cy) / zoom
    theta = np.deg2rad(theta_deg)
    if abs(theta) > 1e-9:
        ct, st = np.cos(-theta), np.sin(-theta)
        rx = cx + (sx - cx) * ct - (sy - cy) * st
        ry = cy + (sx - cx) * st + (sy - cy) * ct
        sx, sy = rx, ry
    out = sample_bilinear(src, sx, sy)
    return np.clip(out + 0.5, 0, 255).astype(np.uint8)


def render_occlude(bg, k, T, step):
    """THE CROSSFADE-CAUSE preset (ARC-A, the failure the smooth camera presets cannot
    show). A STATIC textured background with ONE OPAQUE disc translating across it —
    so the held-out interpolator faces the two modes the dossier (FG_VFI_PRIOR_ART §3
    F8) names as the ghosting root cause, which value-noise rigid-camera motion never
    exhibits:
      • APERTURE — the disc carries a FAINT, disc-ATTACHED texture (moves with the disc
        → a coherent object motion exists) but LOW-contrast → the block-match starves
        for gradient in the interior → it cannot lock the disc's motion → blend.
      • OCCLUSION / DISOCCLUSION — the opaque disc covers background as it advances and
        UNCOVERS background behind its trailing edge; the uncovered band has NO match in
        the previous frame → the symmetric blend of misaligned samples = the crescent.
    The disc centre is LINEAR in k, so frame 2j+1 places it at the exact midpoint of
    2j and 2j+2 → a valid held-out ground-truth target (same contract as render_frame).
    `step` is unused — the disc sweep is a fixed fraction of the width (a large, clear
    motion that reliably triggers the failure)."""
    h, w = bg.shape[0], bg.shape[1]
    u = k / (T - 1)
    out = bg.copy()
    cx = w * 0.20 + (w * 0.60) * u    # sweep L→R across the middle 60% (independent of bg)
    cy = h * 0.52
    rad = min(w, h) * 0.13
    yy, xx = np.meshgrid(np.arange(h, dtype=np.float32),
                         np.arange(w, dtype=np.float32), indexing="ij")
    dxr = xx - cx; dyr = yy - cy
    m = (dxr * dxr + dyr * dyr) <= rad * rad
    tex = 16.0 * np.sin(0.08 * dxr) + 12.0 * np.cos(0.07 * dyr)   # faint, disc-attached
    base = (55.0, 170.0, 200.0)
    disc = np.empty((h, w, 4), dtype=np.float32)
    disc[..., 0] = np.clip(base[0] + tex, 0.0, 255.0)
    disc[..., 1] = np.clip(base[1] + tex, 0.0, 255.0)
    disc[..., 2] = np.clip(base[2] + tex, 0.0, 255.0)
    disc[..., 3] = 255.0
    out[m] = disc[m]
    return np.clip(out + 0.5, 0, 255).astype(np.uint8)


# Each preset maps frame index k∈[0,T-1] → absolute (tx, ty, zoom, theta_deg).
# `step` is the per-frame translation so a HELD-OUT step (2 frames) ≈ 2*step px.
def preset_params(name, k, T, step):
    u = k / (T - 1)               # normalized time in [0,1]
    if name == "pan":
        return (step * k, 0.0, 1.0, 0.0)
    if name == "pan_diag":
        return (0.75 * step * k, 0.5 * step * k, 1.0, 0.0)
    if name == "accel":           # tx ∝ u² → per-step motion grows across the segment
        total = step * (T - 1) * 1.6
        return (total * u * u, 0.0, 1.0, 0.0)
    if name == "zoom":
        return (0.0, 0.0, 1.0 + 0.18 * u, 0.0)
    if name == "orbit":
        return (0.0, 0.0, 1.0, 10.0 * u)
    if name == "mixed":
        return (0.6 * step * k, 0.4 * step * k, 1.0 + 0.10 * u, 6.0 * u)
    raise ValueError(name)


PRESETS = ["pan", "pan_diag", "accel", "zoom", "orbit", "mixed", "occlude"]


def main():
    ap = argparse.ArgumentParser(description="continuous held-out test-field source")
    ap.add_argument("--width", type=int, default=1280)
    ap.add_argument("--height", type=int, default=720)
    ap.add_argument("--frames", type=int, default=40, help="frames per preset (T)")
    ap.add_argument("--step", type=float, default=4.0,
                    help="per-frame translation px (held-out step ≈ 2*step)")
    ap.add_argument("--out", default=os.path.join(HERE, "zoo_seq"))
    ap.add_argument("--presets", nargs="*", default=PRESETS)
    args = ap.parse_args()

    W, H, T = args.width, args.height, args.frames
    if T < 3:
        sys.exit("prep_zoo_sequence: --frames must be ≥ 3")
    os.makedirs(args.out, exist_ok=True)
    scene = build_scene(W, H)
    print(f"prep_zoo_sequence: {W}x{H}  T={T}/preset  step={args.step}px  presets={args.presets}")
    print(f"  held-out step ~ {2*args.step:.1f}px  ->  {(T-1)//2} triples/preset")

    manifest = [f"# zoo continuous held-out sequences  {W}x{H}  T={T}  step={args.step}",
                f"size {W} {H}"]
    total_frames = 0
    for name in args.presets:
        for k in range(T):
            if name == "occlude":                       # the crossfade-cause preset (object occlusion + aperture)
                img = render_occlude(scene, k, T, args.step)
            else:                                        # the camera-transform presets (pan/zoom/orbit/…)
                tx, ty, zoom, theta = preset_params(name, k, T, args.step)
                img = render_frame(scene, tx, ty, zoom, theta)
            path = os.path.join(args.out, f"{name}_{k:06d}.rgba")
            with open(path, "wb") as f:
                f.write(np.ascontiguousarray(img).tobytes())
            total_frames += 1
        manifest.append(f"sequence {name}_ {T}")
        print(f"  wrote {name}: {T} frames")

    with open(os.path.join(args.out, "manifest.txt"), "w") as f:
        f.write("\n".join(manifest) + "\n")
    print(f"prep_zoo_sequence: wrote {total_frames} frames "
          f"({len(args.presets)} presets) into {args.out}")
    print(f"  score: ./build-fgq/fg_quality_scorer {os.path.join(args.out, 'manifest.txt')} -1 zoo.csv")


if __name__ == "__main__":
    main()
# Made with my soul - Swately <3
