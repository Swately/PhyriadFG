// framework/render/vulkan/bench/fg_quality_scorer/main.cpp
// FG-QUALITY SCORER — the "number producer" for the render_assistant (Ayama)
// FG test-field (HSR120_THROUGHPUT_DIAGNOSIS.md §11.4 layer 2).
//
// Turns eyeballed FG artifacts into objective metrics on HELD-OUT real triples.
// Given a triple (anchor_prev = real N, truth_mid = real N+1 @ t=0.5,
// anchor_next = real N+2), it scores two candidate midpoints against the
// held-out ground truth truth_mid (§11.3 held-out method):
//
//   Mode A (flow+warp, offline-deterministic): run the COMMITTED
//     OpticalFlowPipeline flow+warp at t=0.5 on (anchor_prev, anchor_next) →
//     candidate. Measures OUR flow+warp quality. GPU-independent (integer-exact
//     SAD + deterministic shaders — see stage11 README).
//
//   Mode B (full-pipeline, only if the manifest carries a live_output): score
//     the live app's dumped wapOutA .rgba vs truth_mid directly (no re-run).
//     Measures the FULL shipped pipeline incl. the holonic layers
//     (object_repair / scene-memory / WAP).
//
//   Mode T (TRUTH-LESS, §11.4 layer 1): the in-app `--qdump` form — a manifest
//     `triple` with prev/next/live but NO mid= (live FG has no held-out ground
//     truth). Only the truth-INDEPENDENT metrics run: crossfade_residual (+alpha)
//     and double_edge_energy of `live` vs (prev, next). PSNR/SSIM/obj_IoU/nonrigid
//     are reported N/A. This is how OUR live FG output is scored for crossfade
//     without perturbing the pipeline (no held-out real needed).
//
// METRICS (all vs truth_mid, 16 px border crop):
//   PSNR + SSIM    — reused VERBATIM from stage11_blockmatch_quality/main.cpp
//                    (psnr_rgb lines 96-110, ssim_luma lines 113-141 of that file).
//   crossfade_residual + double_edge_energy — the §11 crossfade/seam artifact.
//   obj_iou + nonrigid_deform               — the §11 gravity/object-deform artifact.
//   LPIPS — DELIBERATELY NOT implemented (needs a learned ONNX model). See README.
//
// Standalone, raw Vulkan (headless compute — no WSI). REUSES, read-only, the
// committed OpticalFlowPipeline.cpp + its shaders + spv_to_header.cmake. NOT
// wired into the render pillar's CMake / CI (mirrors stage11/stage31). §13:
// every Vulkan object is released. Does NOT touch the live app or stage11/31's
// files — it COPIES their proven pattern.
//
// HONESTY (see README "Caveats"): Mode A drives the COMMITTED warp shader
// (optical_flow_warp.comp), which stage11 documents as carrying a sign error
// (it doubles the displacement → a ghost that can sit BELOW the naive blend on
// pure translation). The scorer reports the committed shader's true number; it
// does NOT silently substitute the local sign-corrected warp. The crossfade and
// gravity metrics are MODEL-FIT / THRESHOLD heuristics — proxies, not truth —
// each documented with its assumptions in the README and below.

#include <phyriad/render/vulkan/OpticalFlowPipeline.hpp>
#include <vulkan/vulkan.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>   // std::getenv / std::atoi — ARC-A LEVER-1b env-toggled arming
#include <cstring>
#include <string>
#include <vector>

#ifndef FGQ_DEFAULT_MANIFEST
#define FGQ_DEFAULT_MANIFEST "frames/manifest.txt"
#endif

namespace {

// ── frame I/O ────────────────────────────────────────────────────────────────
// (same raw .rgba reader as stage11/main.cpp:59-68 — RGBA8, row-major, no header)
bool read_file(const std::string& path, std::vector<uint8_t>& out) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END); long n = std::ftell(f); std::fseek(f, 0, SEEK_SET);
    if (n <= 0) { std::fclose(f); return false; }
    out.resize(static_cast<size_t>(n));
    const size_t got = std::fread(out.data(), 1, out.size(), f);
    std::fclose(f);
    return got == out.size();
}

// ── manifest ─────────────────────────────────────────────────────────────────
// A triple = the three held-out reals + an optional dumped live output.
//   anchor_prev = real N      truth_mid = real N+1 (held out, the ground truth)
//   anchor_next = real N+2    live = the live wapOutA for this midpoint (Mode B)
// Paths are resolved relative to the manifest's directory.
struct Triple {
    std::string name;
    std::string prev_path, mid_path, next_path;  // resolved (dir-prefixed)
    std::string live_path;                        // empty ⇒ no Mode B
    bool        has_live = false;
    bool        truthless = false;                // §11.4 layer 1: a `triple` with prev/next/live but NO
                                                  // mid= (the in-app --qdump form — live FG has no held-out
                                                  // ground truth). Truth-DEPENDENT metrics (PSNR/SSIM/obj_*/
                                                  // nonrigid) are reported N/A; only crossfade + double-edge run.
};
struct Manifest {
    uint32_t            w = 0, h = 0;
    std::vector<Triple> triples;
};

// Trim leading/trailing ASCII whitespace.
std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r')) ++a;
    while (b > a && (s[b-1] == ' ' || s[b-1] == '\t' || s[b-1] == '\r')) --b;
    return s.substr(a, b - a);
}

std::string dir_of(const std::string& path) {
    const size_t s1 = path.find_last_of('/');
    const size_t s2 = path.find_last_of('\\');
    size_t s = std::string::npos;
    if (s1 != std::string::npos) s = s1;
    if (s2 != std::string::npos) s = (s == std::string::npos) ? s2 : std::max(s, s2);
    return (s == std::string::npos) ? std::string(".") : path.substr(0, s);
}

// FORMAT (documented in README.md — this is the contract the in-app --qdump emits):
//   '#' line                     → comment
//   size <W> <H>                 → frame dims (RGBA8); ONE per file, applies to all
//   triple <name> prev=<p> mid=<m> next=<n> [live=<l>]
//                                → explicit-path triple. Paths relative to manifest dir.
//   triple <name> prev=<p> next=<n> live=<l>   (NO mid=)
//                                → TRUTH-LESS triple (the in-app --qdump form). No held-out
//                                  ground truth → only crossfade + double-edge run (mode T).
//   preset <name> [...]          → BACK-COMPAT with stage11/stage31 manifests:
//                                  maps to <name>_N.rgba / <name>_M.rgba / <name>_P.rgba
//                                  (stage11 triple naming). Trailing tokens (D=, tx=…)
//                                  are ignored. For stage31 (A/B/M) use the explicit
//                                  'triple' form (see README self-test).
bool load_manifest(const std::string& manifest_path, Manifest& m) {
    std::vector<uint8_t> raw;
    if (!read_file(manifest_path, raw)) return false;
    const std::string dir = dir_of(manifest_path);
    std::string s(raw.begin(), raw.end());
    size_t pos = 0;
    while (pos < s.size()) {
        size_t eol = s.find('\n', pos);
        if (eol == std::string::npos) eol = s.size();
        std::string line = trim(s.substr(pos, eol - pos));
        pos = eol + 1;
        if (line.empty() || line[0] == '#') continue;

        if (line.rfind("size ", 0) == 0) {
            std::sscanf(line.c_str(), "size %u %u", &m.w, &m.h);

        } else if (line.rfind("triple ", 0) == 0) {
            // triple <name> prev=.. mid=.. next=.. [live=..]
            char nm[256] = {};
            std::sscanf(line.c_str(), "triple %255s", nm);
            Triple t; t.name = nm;
            auto field = [&](const char* key, std::string& dst) {
                const std::string k = std::string(" ") + key + "=";
                size_t kp = line.find(k);
                if (kp == std::string::npos) return false;
                kp += k.size();
                size_t ep = line.find_first_of(" \t", kp);
                if (ep == std::string::npos) ep = line.size();
                dst = dir + "/" + line.substr(kp, ep - kp);
                return true;
            };
            const bool ok_prev = field("prev", t.prev_path);
            const bool ok_mid  = field("mid",  t.mid_path);
            const bool ok_next = field("next", t.next_path);
            t.has_live = field("live", t.live_path);
            // §11.4 layer 1: a TRUTH-LESS triple has prev+next+live but NO mid= (the in-app --qdump form).
            // It carries no held-out ground truth, so only the truth-independent metrics run (crossfade +
            // double-edge of live vs the two anchors). A full triple keeps mid= (the held-out N+1).
            t.truthless = (ok_prev && ok_next && !ok_mid && t.has_live);
            if (ok_prev && ok_mid && ok_next) m.triples.push_back(t);          // full held-out triple
            else if (t.truthless)             m.triples.push_back(t);          // truth-less (--qdump) triple

        } else if (line.rfind("sequence ", 0) == 0) {
            // sequence <prefix> <count> — expand a CONTINUOUS recording/synthetic sequence
            //   <prefix>000000.rgba .. <prefix>{count-1}.rgba   (6-digit zero-pad, capture_dump/zoo naming)
            // into HELD-OUT triples (the canonical VFI protocol over a continuous segment, §11.3):
            //   triple j  = prev=<prefix>{2j}  mid=<prefix>{2j+1} (HELD OUT, ground truth)  next=<prefix>{2j+2}
            // The interpolator reconstructs the dropped ODD frame from its EVEN neighbours; mid is the real
            // frame we withheld. This is the operator's "extensión de tiempo" — n frames recorded digitally,
            // decomposed into continuous held-out triples, scored per frame. Mode A (our flow+warp) runs on
            // each; Mode B too if a sibling <prefix>{2j+1}_live.rgba exists (the full-pipeline replay output).
            char pfx[200] = {}; int count = 0;
            if (std::sscanf(line.c_str(), "sequence %199s %d", pfx, &count) == 2 && count >= 3) {
                // E-9: these three names used to be built in char[64], while the sscanf above accepts a
                // prefix of up to 199 characters. A prefix over 47 chars (lv), 52 (fn) or 57 (nm) was
                // truncated by snprintf and the triple then surfaced as the misleading
                // "[skip] missing one of prev/mid/next .rgba" line rather than as an error about the
                // prefix. Build the names in std::string and leave only the 6-digit index in a buffer,
                // which cannot overflow. Output is byte-identical for every prefix that works today.
                auto idx6 = [](int v) { char d[16]; std::snprintf(d, sizeof d, "%06d", v); return std::string(d); };
                const std::string pfx_s(pfx);
                for (int j = 0; 2*j + 2 <= count - 1; ++j) {
                    auto frame = [&](int idx) { return dir + "/" + pfx_s + idx6(idx) + ".rgba"; };
                    Triple t;
                    t.name = pfx_s + idx6(2*j+1);                                     // named by the held-out frame
                    t.prev_path = frame(2*j);
                    t.mid_path  = frame(2*j+1);
                    t.next_path = frame(2*j+2);
                    // optional full-pipeline replay output for the held-out midpoint (Mode B), if present.
                    t.live_path = dir + "/" + pfx_s + idx6(2*j+1) + "_live.rgba";
                    t.has_live  = false;  // resolved at score time: read_file fails silently if absent
                    m.triples.push_back(t);
                }
            }

        } else if (line.rfind("preset ", 0) == 0) {
            // stage11/stage31 back-compat: <name>_{N,M,P}.rgba
            char nm[256] = {};
            std::sscanf(line.c_str(), "preset %255s", nm);
            Triple t; t.name = nm;
            t.prev_path = dir + "/" + nm + "_N.rgba";
            t.mid_path  = dir + "/" + nm + "_M.rgba";
            t.next_path = dir + "/" + nm + "_P.rgba";
            t.has_live  = false;
            m.triples.push_back(t);
        }
    }
    return m.w > 0 && m.h > 0 && !m.triples.empty();
}

// ── METRICS reused VERBATIM from stage11_blockmatch_quality/main.cpp ──────────
// psnr_rgb: stage11/main.cpp lines 96-110 (mean-squared-error PSNR over RGB,
//   interior with a `border`-pixel crop). Identical body.
double psnr_rgb(const uint8_t* a, const uint8_t* b, uint32_t w, uint32_t h, uint32_t border) {
    double mse = 0.0; uint64_t n = 0;
    for (uint32_t y = border; y < h - border; ++y)
        for (uint32_t x = border; x < w - border; ++x) {
            const size_t i = (static_cast<size_t>(y) * w + x) * 4;
            for (int c = 0; c < 3; ++c) {
                const double d = double(a[i + c]) - double(b[i + c]);
                mse += d * d; ++n;
            }
        }
    if (n == 0) return 0.0;
    mse /= double(n);
    if (mse <= 1e-9) return 99.0;
    return 10.0 * std::log10(255.0 * 255.0 / mse);
}

// ssim_luma: stage11/main.cpp lines 113-141 (windowed SSIM on luma, win 8 step 4,
//   8-bit constants C1=6.5025 C2=58.5225). Identical body.
double ssim_luma(const uint8_t* a, const uint8_t* b, uint32_t w, uint32_t h, uint32_t border) {
    auto luma = [](const uint8_t* p, size_t i) {
        return 0.299 * p[i] + 0.587 * p[i + 1] + 0.114 * p[i + 2];
    };
    const double C1 = 6.5025, C2 = 58.5225;
    const int win = 8, step = 4;
    double acc = 0.0; uint64_t cnt = 0;
    for (uint32_t y = border; y + win <= h - border; y += step)
        for (uint32_t x = border; x + win <= w - border; x += step) {
            double ma = 0, mb = 0;
            for (int j = 0; j < win; ++j) for (int k = 0; k < win; ++k) {
                const size_t i = (static_cast<size_t>(y + j) * w + (x + k)) * 4;
                ma += luma(a, i); mb += luma(b, i);
            }
            const double inv = 1.0 / (win * win);
            ma *= inv; mb *= inv;
            double va = 0, vb = 0, cov = 0;
            for (int j = 0; j < win; ++j) for (int k = 0; k < win; ++k) {
                const size_t i = (static_cast<size_t>(y + j) * w + (x + k)) * 4;
                const double da = luma(a, i) - ma, db = luma(b, i) - mb;
                va += da * da; vb += db * db; cov += da * db;
            }
            va *= inv; vb *= inv; cov *= inv;
            const double s = ((2 * ma * mb + C1) * (2 * cov + C2)) /
                             ((ma * ma + mb * mb + C1) * (va + vb + C2));
            acc += s; ++cnt;
        }
    return cnt ? acc / double(cnt) : 0.0;
}

// ── CROSSFADE metric (the §11.5 crossfade/seam artifact) ──────────────────────
// crossfade_residual: the candidate is fit to the BEST pure crossfade model
//   cand ≈ α·prev + (1-α)·next, minimising MSE over α∈[0,1]. The residual is the
//   per-channel RMSE (0..255) of the best fit. The α that minimises
//   Σ‖cand − (α·prev + (1-α)·next)‖² has the closed form (per channel, summed):
//     let p=prev, q=next, c=cand, d=p-q ; α* = Σ(c-q)·d / Σ d·d  (clamped [0,1]).
//   INTERPRETATION (documented honestly): a LOW residual ⇒ the candidate IS
//   ~a linear blend of the two anchors = the crossfade artifact. A clean
//   motion-compensated warp moves content to NEW positions that the blend cannot
//   reproduce → HIGH residual. So here LOW = BAD (crossfading), HIGH = GOOD
//   (genuine motion comp). This is a MODEL-FIT PROXY, not a direct artifact
//   detector — a frame that is mostly static (no motion) also fits the blend with
//   a low residual, which is correct (static ⇒ blend ≈ truth ⇒ no artifact to
//   call out). Read it together with PSNR: low residual + low PSNR = a damaging
//   crossfade; low residual + high PSNR = harmless (little motion).
double crossfade_residual(const uint8_t* cand, const uint8_t* prev, const uint8_t* next,
                          uint32_t w, uint32_t h, uint32_t border, double& alpha_out) {
    double num = 0.0, den = 0.0;
    for (uint32_t y = border; y < h - border; ++y)
        for (uint32_t x = border; x < w - border; ++x) {
            const size_t i = (static_cast<size_t>(y) * w + x) * 4;
            for (int c = 0; c < 3; ++c) {
                const double d  = double(prev[i + c]) - double(next[i + c]);
                const double cq = double(cand[i + c]) - double(next[i + c]);
                num += cq * d; den += d * d;
            }
        }
    double alpha = (den > 1e-9) ? (num / den) : 0.5;
    alpha = std::min(1.0, std::max(0.0, alpha));
    alpha_out = alpha;
    // residual RMSE of the best blend vs the candidate.
    double sse = 0.0; uint64_t n = 0;
    for (uint32_t y = border; y < h - border; ++y)
        for (uint32_t x = border; x < w - border; ++x) {
            const size_t i = (static_cast<size_t>(y) * w + x) * 4;
            for (int c = 0; c < 3; ++c) {
                const double model = alpha * double(prev[i + c]) + (1.0 - alpha) * double(next[i + c]);
                const double e = double(cand[i + c]) - model;
                sse += e * e; ++n;
            }
        }
    return n ? std::sqrt(sse / double(n)) : 0.0;
}

// double_edge_energy: sum of luma gradient-magnitude in the candidate at pixels
//   where NEITHER anchor NOR the truth has a strong edge — the spurious crescent
//   seam the crossfade introduces. We compute a forward-difference luma gradient
//   |∇L| at each interior pixel for cand, prev, next, truth; a pixel is a
//   "spurious edge" iff cand's |∇L| exceeds `edge_thr` AND each of prev/next/truth
//   is below `edge_thr` there (no real edge to justify it). We sum cand's |∇L|
//   over those pixels and NORMALISE by the interior pixel count (per-pixel mean,
//   units = luma/px). Higher ⇒ more seam energy not present in any real frame.
//   HONEST LIMIT: it is gradient-, not structure-aware (a sub-pixel-shifted real
//   edge can register as "spurious"); the per-pixel AND-mask against all three
//   real frames suppresses most of that. edge_thr default 12 (luma/px).
double double_edge_energy(const uint8_t* cand, const uint8_t* prev, const uint8_t* next,
                          const uint8_t* truth, uint32_t w, uint32_t h, uint32_t border,
                          double edge_thr) {
    auto lum = [](const uint8_t* p, size_t i) {
        return 0.299 * p[i] + 0.587 * p[i + 1] + 0.114 * p[i + 2];
    };
    auto grad = [&](const uint8_t* p, uint32_t x, uint32_t y) {
        const size_t i  = (static_cast<size_t>(y) * w + x) * 4;
        const size_t ir = (static_cast<size_t>(y) * w + (x + 1)) * 4;
        const size_t id = (static_cast<size_t>(y + 1) * w + x) * 4;
        const double gx = lum(p, ir) - lum(p, i);
        const double gy = lum(p, id) - lum(p, i);
        return std::sqrt(gx * gx + gy * gy);
    };
    double acc = 0.0; uint64_t n = 0;
    for (uint32_t y = border; y < h - border - 1; ++y)
        for (uint32_t x = border; x < w - border - 1; ++x) {
            const double gc = grad(cand, x, y);
            ++n;
            if (gc <= edge_thr) continue;
            if (grad(prev,  x, y) > edge_thr) continue;
            if (grad(next,  x, y) > edge_thr) continue;
            if (grad(truth, x, y) > edge_thr) continue;
            acc += gc;  // spurious edge: present in cand, absent in all real frames
        }
    return n ? acc / double(n) : 0.0;
}

// double_edge_energy_masked (§11.5 + VFI-QA SOTA: motion-masked ROI metric).
//   The global double_edge_energy above NORMALISES by the whole interior, so a
//   LOCALIZED ghost (the q1 double-disc) is DILUTED against the static background —
//   the exact failure mode the operator's eye caught AND that the peer-reviewed
//   VFI-quality literature proves: per-pixel/global metrics correlate poorly with
//   perceived interpolation artifacts because the artifact is localized to the
//   moving region (FloLPIPS, arXiv 2207.08119, weights LPIPS by the optical-flow
//   discrepancy map; BVI-VFI/VFIPS, ECCV'22, on object-deform ghosting). The fix is
//   to restrict the metric to the MOVING region. Here we build a coarse motion mask
//   from the two ANCHORS — a pixel where |luma(prev) − luma(next)| > motion_thr is
//   where content MOVED between N and N+2, i.e. where ghosting/crossfade lives —
//   accumulate spurious-edge energy ONLY there, and normalise by the motion-pixel
//   count (per-MOVING-pixel mean, not per-frame). `coverage_out` = motion px /
//   interior px, so a high score on TINY coverage reads as a localized ghost (q1),
//   not a frame-wide fault.
//   HONEST SCOPE: this is motion-PRESENCE masking (where content moved) — the
//   truth-less analogue of FloLPIPS flow-DISCREPANCY weighting (where OUR motion is
//   WRONG). The discrepancy form needs the held-out truth + the flow field (FR mode,
//   §11 upgrade); the research flagged that warp-error motion-decoupling is NOT
//   provably independent of motion magnitude, so this stays a localizer/flag, read
//   together with coverage + PSNR, never a standalone verdict.
double double_edge_energy_masked(const uint8_t* cand, const uint8_t* prev, const uint8_t* next,
                                 const uint8_t* truth, uint32_t w, uint32_t h, uint32_t border,
                                 double edge_thr, double motion_thr, double& coverage_out) {
    auto lum = [](const uint8_t* p, size_t i) {
        return 0.299 * p[i] + 0.587 * p[i + 1] + 0.114 * p[i + 2];
    };
    auto grad = [&](const uint8_t* p, uint32_t x, uint32_t y) {
        const size_t i  = (static_cast<size_t>(y) * w + x) * 4;
        const size_t ir = (static_cast<size_t>(y) * w + (x + 1)) * 4;
        const size_t id = (static_cast<size_t>(y + 1) * w + x) * 4;
        const double gx = lum(p, ir) - lum(p, i);
        const double gy = lum(p, id) - lum(p, i);
        return std::sqrt(gx * gx + gy * gy);
    };
    double acc = 0.0; uint64_t motion_px = 0, interior = 0;
    for (uint32_t y = border; y < h - border - 1; ++y)
        for (uint32_t x = border; x < w - border - 1; ++x) {
            const size_t i = (static_cast<size_t>(y) * w + x) * 4;
            ++interior;
            if (std::fabs(lum(prev, i) - lum(next, i)) <= motion_thr) continue;  // static → not ROI
            ++motion_px;
            const double gc = grad(cand, x, y);
            if (gc <= edge_thr) continue;
            if (grad(prev,  x, y) > edge_thr) continue;
            if (grad(next,  x, y) > edge_thr) continue;
            if (grad(truth, x, y) > edge_thr) continue;
            acc += gc;  // spurious edge inside the moving region
        }
    coverage_out = interior ? double(motion_px) / double(interior) : 0.0;
    return motion_px ? acc / double(motion_px) : 0.0;
}

// ── GRAVITY / OBJECT-DEFORMATION metric (the §11.5 gravity/object-warp artifact) ─
// HEURISTIC, THRESHOLD-BASED — a PROXY, documented limits below. Segments the
// dominant moving object by a high-saturation OR high-luma-deviation mask, takes
// its largest connected component, and compares the candidate's object silhouette
// to the truth's:
//   obj_iou        = |maskC ∩ maskT| / |maskC ∪ maskT|   (silhouette overlap; 1 = identical)
//   nonrigid_deform = after translating maskC so its centroid coincides with maskT's,
//                     the symmetric-difference area / mean object area. A RIGID object
//                     that merely moved aligns to ~0; an object that changed SHAPE
//                     (the LSFG "gravity"/squish artifact) leaves residual mismatch > 0.
// HONEST ASSUMPTIONS / LIMITS (also in README):
//   - "dominant object" = the largest connected component of a colour/luma mask;
//     it assumes ONE salient object distinguishable from the background by saturation
//     or luma. On busy / multi-object scenes it segments whatever component is
//     largest, which may not be the artifact-bearing one. It is a synthetic-/
//     simple-scene proxy, NOT a general segmenter.
//   - the threshold (sat_thr / luma_dev_thr) is fixed; a low-contrast object can
//     be missed entirely → obj_iou reported as -1 (N/A) when either mask is empty.
//   - centroid-only alignment models pure translation; it does NOT correct
//     rotation/scale, so a rotating rigid object scores some nonrigid_deform.
struct ObjMetrics { double iou = -1.0; double nonrigid = -1.0; int area_c = 0, area_t = 0; };

static void object_mask(const uint8_t* p, uint32_t w, uint32_t h, uint32_t border,
                        double sat_thr, double luma_dev_thr, std::vector<uint8_t>& mask,
                        double& mean_luma) {
    mask.assign(static_cast<size_t>(w) * h, 0u);
    // pass 1: interior mean luma (background reference).
    double sum = 0.0; uint64_t n = 0;
    for (uint32_t y = border; y < h - border; ++y)
        for (uint32_t x = border; x < w - border; ++x) {
            const size_t i = (static_cast<size_t>(y) * w + x) * 4;
            sum += 0.299 * p[i] + 0.587 * p[i+1] + 0.114 * p[i+2]; ++n;
        }
    mean_luma = n ? sum / double(n) : 0.0;
    // pass 2: mark salient pixels (high saturation OR strong luma deviation from bg).
    for (uint32_t y = border; y < h - border; ++y)
        for (uint32_t x = border; x < w - border; ++x) {
            const size_t i = (static_cast<size_t>(y) * w + x) * 4;
            const double r = p[i], g = p[i+1], b = p[i+2];
            const double mx = std::max(r, std::max(g, b));
            const double mn = std::min(r, std::min(g, b));
            const double sat = (mx > 1e-6) ? (mx - mn) / mx : 0.0;  // HSV saturation
            const double lum = 0.299 * r + 0.587 * g + 0.114 * b;
            if (sat >= sat_thr || std::fabs(lum - mean_luma) >= luma_dev_thr)
                mask[static_cast<size_t>(y) * w + x] = 1u;
        }
}

// crescent_band_energy — the crescent-SPECIFIC detector (automation enabler).
//   The disocclusion crescent / estela lives in the SWEPT BAND: the symmetric difference of the
//   object silhouette between the two anchors, object_mask(prev) XOR object_mask(next) — exactly
//   where the object's leading/trailing edge sweeps. dbl_edg_m is too coarse (the WHOLE moving
//   region, so it tracks total artifact energy, not the crescent — it moved on the interior fix but
//   the operator's eye saw the crescent steady). This restricts the spurious-edge energy (live's
//   |∇L| where NEITHER anchor has an edge — the half-moon seam) to that band, normalised by the band
//   pixel count, so the number tracks the VISIBLE crescent. coverage_out = band px / interior px.
//   Truth-less (prev/next/live) — runs on the in-app --qdump triples. HONEST LIMIT: object_mask is the
//   simple-scene saturation/luma proxy (same caveats as the gravity metric); a low-contrast object or a
//   busy multi-object scene degrades the band. Validate it tracks the eye before trusting it (§automation).
double crescent_band_energy(const uint8_t* live, const uint8_t* prev, const uint8_t* next,
                            uint32_t w, uint32_t h, uint32_t border, double edge_thr,
                            double sat_thr, double luma_dev_thr, double& coverage_out) {
    std::vector<uint8_t> mp, mn; double mlp = 0, mln = 0;
    object_mask(prev, w, h, border, sat_thr, luma_dev_thr, mp, mlp);
    object_mask(next, w, h, border, sat_thr, luma_dev_thr, mn, mln);
    auto lum = [](const uint8_t* p, size_t i) {
        return 0.299 * p[i] + 0.587 * p[i + 1] + 0.114 * p[i + 2];
    };
    auto grad = [&](const uint8_t* p, uint32_t x, uint32_t y) {
        const size_t i  = (static_cast<size_t>(y) * w + x) * 4;
        const size_t ir = (static_cast<size_t>(y) * w + (x + 1)) * 4;
        const size_t id = (static_cast<size_t>(y + 1) * w + x) * 4;
        const double gx = lum(p, ir) - lum(p, i);
        const double gy = lum(p, id) - lum(p, i);
        return std::sqrt(gx * gx + gy * gy);
    };
    double acc = 0.0; uint64_t band_px = 0, interior = 0;
    for (uint32_t y = border; y < h - border - 1; ++y)
        for (uint32_t x = border; x < w - border - 1; ++x) {
            const size_t m = static_cast<size_t>(y) * w + x;
            ++interior;
            if ((mp[m] != 0) == (mn[m] != 0)) continue;   // NOT the swept band (silhouette agrees) → skip
            ++band_px;
            const double gc = grad(live, x, y);
            if (gc <= edge_thr) continue;
            if (grad(prev, x, y) > edge_thr) continue;     // a real edge in an anchor → not spurious
            if (grad(next, x, y) > edge_thr) continue;
            acc += gc;                                     // spurious seam INSIDE the disocclusion band
        }
    coverage_out = interior ? double(band_px) / double(interior) : 0.0;
    return band_px ? acc / double(band_px) : 0.0;
}

// Largest connected component (4-neighbour BFS) kept; others zeroed. Returns area.
static int keep_largest_cc(std::vector<uint8_t>& mask, uint32_t w, uint32_t h) {
    std::vector<int> label(static_cast<size_t>(w) * h, 0);
    std::vector<uint32_t> stack;
    int best_label = 0, best_area = 0, cur = 0;
    for (uint32_t y = 0; y < h; ++y)
        for (uint32_t x = 0; x < w; ++x) {
            const size_t s = static_cast<size_t>(y) * w + x;
            if (!mask[s] || label[s]) continue;
            ++cur; int area = 0;
            stack.clear(); stack.push_back(static_cast<uint32_t>(s)); label[s] = cur;
            while (!stack.empty()) {
                const uint32_t q = stack.back(); stack.pop_back(); ++area;
                const uint32_t qx = q % w, qy = q / w;
                const uint32_t nx[4] = {qx ? qx - 1 : qx, qx + 1 < w ? qx + 1 : qx, qx, qx};
                const uint32_t ny[4] = {qy, qy, qy ? qy - 1 : qy, qy + 1 < h ? qy + 1 : qy};
                for (int k = 0; k < 4; ++k) {
                    const size_t t = static_cast<size_t>(ny[k]) * w + nx[k];
                    if (mask[t] && !label[t]) { label[t] = cur; stack.push_back(static_cast<uint32_t>(t)); }
                }
            }
            if (area > best_area) { best_area = area; best_label = cur; }
        }
    if (best_area == 0) { std::fill(mask.begin(), mask.end(), 0u); return 0; }
    for (size_t s = 0; s < mask.size(); ++s) mask[s] = (label[s] == best_label) ? 1u : 0u;
    return best_area;
}

static void centroid(const std::vector<uint8_t>& mask, uint32_t w, uint32_t h,
                     double& cx, double& cy) {
    double sx = 0, sy = 0; uint64_t n = 0;
    for (uint32_t y = 0; y < h; ++y)
        for (uint32_t x = 0; x < w; ++x)
            if (mask[static_cast<size_t>(y) * w + x]) { sx += x; sy += y; ++n; }
    cx = n ? sx / double(n) : 0; cy = n ? sy / double(n) : 0;
}

ObjMetrics gravity_metric(const uint8_t* cand, const uint8_t* truth, uint32_t w, uint32_t h,
                          uint32_t border, double sat_thr, double luma_dev_thr) {
    ObjMetrics out;
    std::vector<uint8_t> mc, mt; double ml_c = 0, ml_t = 0;
    object_mask(cand,  w, h, border, sat_thr, luma_dev_thr, mc, ml_c);
    object_mask(truth, w, h, border, sat_thr, luma_dev_thr, mt, ml_t);
    out.area_c = keep_largest_cc(mc, w, h);
    out.area_t = keep_largest_cc(mt, w, h);
    if (out.area_c == 0 || out.area_t == 0) return out;  // mask empty ⇒ N/A (-1)

    // IoU of the raw silhouettes.
    uint64_t inter = 0, uni = 0;
    for (size_t s = 0; s < mc.size(); ++s) {
        const bool a = mc[s] != 0, b = mt[s] != 0;
        if (a && b) ++inter;
        if (a || b) ++uni;
    }
    out.iou = uni ? double(inter) / double(uni) : -1.0;

    // Non-rigid deformation: translate cand's mask so its centroid lands on
    // truth's, then symmetric-difference area / mean area. Pure translation
    // cancels; residual ⇒ shape change.
    double ccx, ccy, tcx, tcy;
    centroid(mc, w, h, ccx, ccy);
    centroid(mt, w, h, tcx, tcy);
    const int dx = static_cast<int>(std::lround(tcx - ccx));
    const int dy = static_cast<int>(std::lround(tcy - ccy));
    uint64_t symdiff = 0;
    for (uint32_t y = 0; y < h; ++y)
        for (uint32_t x = 0; x < w; ++x) {
            const bool t = mt[static_cast<size_t>(y) * w + x] != 0;
            // candidate pixel that maps here after the centroid shift:
            const long sx = long(x) - dx, sy = long(y) - dy;
            bool c = false;
            if (sx >= 0 && sx < long(w) && sy >= 0 && sy < long(h))
                c = mc[static_cast<size_t>(sy) * w + sx] != 0;
            if (c != t) ++symdiff;
        }
    const double mean_area = 0.5 * (double(out.area_c) + double(out.area_t));
    out.nonrigid = mean_area > 0 ? double(symdiff) / mean_area : -1.0;
    return out;
}

// IEEE-754 half (RG16F MV field) → float. Standard bit decode (sign/exp/mantissa,
// subnormal + inf/nan paths). The pipeline's motion_image() stores (dx,dy) px as RG16F.
inline float half_to_float(uint16_t h) {
    const uint32_t sign = (uint32_t)(h & 0x8000u) << 16;
    const uint32_t exp  = (h >> 10) & 0x1Fu;
    const uint32_t mant = h & 0x3FFu;
    uint32_t f;
    if (exp == 0u) {
        if (mant == 0u) { f = sign; }
        else {                                   // subnormal → normalise
            int e = -1; uint32_t m = mant;
            do { m <<= 1; ++e; } while ((m & 0x400u) == 0u);
            m &= 0x3FFu;
            f = sign | ((uint32_t)(127 - 15 - e) << 23) | (m << 13);
        }
    } else if (exp == 0x1Fu) {                    // inf / nan
        f = sign | 0x7F800000u | (mant << 13);
    } else {                                      // normal
        f = sign | ((exp - 15u + 127u) << 23) | (mant << 13);
    }
    float out; std::memcpy(&out, &f, 4); return out;
}

// ── vulkan helpers (same pattern as stage11/main.cpp:144-213) ─────────────────
uint32_t find_mem(VkPhysicalDevice p, uint32_t bits, VkMemoryPropertyFlags want) {
    VkPhysicalDeviceMemoryProperties mp{}; vkGetPhysicalDeviceMemoryProperties(p, &mp);
    for (uint32_t i = 0; i < mp.memoryTypeCount; ++i)
        if ((bits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & want) == want) return i;
    return UINT32_MAX;
}

struct Image {
    VkImage img{VK_NULL_HANDLE}; VkImageView vw{VK_NULL_HANDLE}; VkDeviceMemory mem{VK_NULL_HANDLE};
    bool create(VkDevice d, VkPhysicalDevice p, uint32_t w, uint32_t h,
                VkFormat f, VkImageUsageFlags usage) {
        VkImageCreateInfo ci{}; ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        ci.imageType = VK_IMAGE_TYPE_2D; ci.format = f; ci.extent = {w, h, 1u};
        ci.mipLevels = 1u; ci.arrayLayers = 1u; ci.samples = VK_SAMPLE_COUNT_1_BIT;
        ci.tiling = VK_IMAGE_TILING_OPTIMAL; ci.usage = usage;
        ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        if (vkCreateImage(d, &ci, nullptr, &img) != VK_SUCCESS) return false;
        VkMemoryRequirements mr{}; vkGetImageMemoryRequirements(d, img, &mr);
        const uint32_t mt = find_mem(p, mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (mt == UINT32_MAX) return false;
        VkMemoryAllocateInfo ai{}; ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        ai.allocationSize = mr.size; ai.memoryTypeIndex = mt;
        if (vkAllocateMemory(d, &ai, nullptr, &mem) != VK_SUCCESS) return false;
        vkBindImageMemory(d, img, mem, 0u);
        VkImageViewCreateInfo vi{}; vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        vi.image = img; vi.viewType = VK_IMAGE_VIEW_TYPE_2D; vi.format = f;
        vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u};
        return vkCreateImageView(d, &vi, nullptr, &vw) == VK_SUCCESS;
    }
    void destroy(VkDevice d) {
        if (vw) vkDestroyImageView(d, vw, nullptr);
        if (img) vkDestroyImage(d, img, nullptr);
        if (mem) vkFreeMemory(d, mem, nullptr);
        vw = VK_NULL_HANDLE; img = VK_NULL_HANDLE; mem = VK_NULL_HANDLE;
    }
};

struct HostBuf {
    VkBuffer buf{VK_NULL_HANDLE}; VkDeviceMemory mem{VK_NULL_HANDLE}; void* mapped{nullptr};
    bool create(VkDevice d, VkPhysicalDevice p, VkDeviceSize bytes, VkBufferUsageFlags usage) {
        VkBufferCreateInfo bi{}; bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bi.size = bytes; bi.usage = usage;
        if (vkCreateBuffer(d, &bi, nullptr, &buf) != VK_SUCCESS) return false;
        VkMemoryRequirements mr{}; vkGetBufferMemoryRequirements(d, buf, &mr);
        const uint32_t mt = find_mem(p, mr.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (mt == UINT32_MAX) return false;
        VkMemoryAllocateInfo ai{}; ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        ai.allocationSize = mr.size; ai.memoryTypeIndex = mt;
        if (vkAllocateMemory(d, &ai, nullptr, &mem) != VK_SUCCESS) return false;
        vkBindBufferMemory(d, buf, mem, 0u);
        return vkMapMemory(d, mem, 0u, bytes, 0u, &mapped) == VK_SUCCESS;
    }
    void destroy(VkDevice d) {
        if (mapped) vkUnmapMemory(d, mem);
        if (buf) vkDestroyBuffer(d, buf, nullptr);
        if (mem) vkFreeMemory(d, mem, nullptr);
        buf = VK_NULL_HANDLE; mem = VK_NULL_HANDLE; mapped = nullptr;
    }
};

void barrier(VkCommandBuffer c, VkImage im, VkImageLayout o, VkImageLayout n,
             VkAccessFlags sa, VkAccessFlags da,
             VkPipelineStageFlags ss, VkPipelineStageFlags ds) {
    VkImageMemoryBarrier b{}; b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    b.oldLayout = o; b.newLayout = n; b.srcAccessMask = sa; b.dstAccessMask = da;
    b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image = im; b.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u};
    vkCmdPipelineBarrier(c, ss, ds, 0u, 0u, nullptr, 0u, nullptr, 1u, &b);
}

// ── per-metric aggregate (mean ± σ over the triples that produced a value) ────
struct Agg {
    double sum = 0, sumsq = 0; int n = 0;
    void add(double v) { sum += v; sumsq += v * v; ++n; }
    double mean() const { return n ? sum / n : 0.0; }
    double sigma() const {
        if (n < 2) return 0.0;
        const double m = mean();
        return std::sqrt(std::max(0.0, sumsq / n - m * m));
    }
};

// Worst-frame surfacing (§11 + VFI-QA SOTA: a continuous segment must surface its WORST
// frames, not just an average — the manual slow-mo eyeballing replacement). One per scored
// row; sorted by the masked artifact metric so the operator sees the frames his eye would pick.
struct WorstRow { double dem; double mcov; double psnr; std::string name; std::string mode; };

} // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    const std::string manifest = (argc > 1) ? argv[1] : FGQ_DEFAULT_MANIFEST;
    const int gpu_index        = (argc > 2) ? std::atoi(argv[2]) : -1;   // -1 = auto
    const std::string csv_path = (argc > 3) ? argv[3] : "";              // optional CSV out

    // Heuristic thresholds (documented; overridable only at compile time for now).
    const double kEdgeThr     = 12.0;   // double_edge_energy luma/px edge threshold
    const double kMotionThr   = 12.0;   // motion-masked dbl_edge: |luma(prev)−luma(next)| ROI gate (8-bit)
    const double kSatThr      = 0.18;   // gravity mask: HSV saturation
    const double kLumaDevThr  = 40.0;   // gravity mask: |luma − bg| (8-bit)
    const uint32_t border     = 16u;    // §11 border crop (matches stage11/31)

    std::printf("[fg_quality_scorer] FG-quality test-field — HSR120 §11.4 layer 2\n");
    std::printf("=====================================================================================\n");
    std::printf("  manifest: %s\n", manifest.c_str());

    Manifest man;
    if (!load_manifest(manifest, man)) {
        std::printf("[FAIL] cannot load manifest '%s' (need: a 'size W H' line + >=1 'triple'/'preset').\n",
                    manifest.c_str());
        return 1;
    }
    const uint32_t W = man.w, H = man.h;
    std::printf("  frame size: %ux%u | triples: %zu | border crop: %u px\n",
                W, H, man.triples.size(), border);

    // ── Vulkan instance + device (stage11/main.cpp:234-282 pattern) ───────────
    VkInstance inst = VK_NULL_HANDLE;
    {
        VkApplicationInfo ai{}; ai.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        ai.pApplicationName = "fg_quality_scorer"; ai.apiVersion = VK_API_VERSION_1_2;
        VkInstanceCreateInfo ci{}; ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        ci.pApplicationInfo = &ai;
        if (vkCreateInstance(&ci, nullptr, &inst) != VK_SUCCESS) { std::printf("[FAIL] instance\n"); return 1; }
    }
    uint32_t nd = 0; vkEnumeratePhysicalDevices(inst, &nd, nullptr);
    std::vector<VkPhysicalDevice> pds(nd); vkEnumeratePhysicalDevices(inst, &nd, pds.data());
    if (nd == 0) { std::printf("[FAIL] no Vulkan devices\n"); vkDestroyInstance(inst, nullptr); return 1; }
    std::printf("  GPUs:\n");
    for (uint32_t i = 0; i < nd; ++i) {
        VkPhysicalDeviceProperties pr{}; vkGetPhysicalDeviceProperties(pds[i], &pr);
        std::printf("    [%u] %s%s\n", i, pr.deviceName,
                    pr.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? " (discrete)" : "");
    }
    VkPhysicalDevice phys = VK_NULL_HANDLE;
    if (gpu_index >= 0 && gpu_index < int(nd)) phys = pds[gpu_index];
    else { for (auto pd : pds) { VkPhysicalDeviceProperties pr{}; vkGetPhysicalDeviceProperties(pd, &pr);
            if (pr.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) { phys = pd; break; } }
           if (!phys) phys = pds[0]; }

    VkPhysicalDeviceProperties props{}; vkGetPhysicalDeviceProperties(phys, &props);

    uint32_t qf = UINT32_MAX;
    {
        uint32_t qn = 0; vkGetPhysicalDeviceQueueFamilyProperties(phys, &qn, nullptr);
        std::vector<VkQueueFamilyProperties> qfs(qn);
        vkGetPhysicalDeviceQueueFamilyProperties(phys, &qn, qfs.data());
        for (uint32_t i = 0; i < qn; ++i)
            if (qfs[i].queueFlags & VK_QUEUE_COMPUTE_BIT) { qf = i; break; }
    }
    if (qf == UINT32_MAX) { std::printf("[FAIL] no compute queue\n"); return 1; }

    // ── environment block (minimal — no shared bench-env helper exists) ───────
    std::printf("\n  ── environment ──\n");
    std::printf("    selected GPU : %s%s\n", props.deviceName,
                props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? " (discrete)" : "");
    std::printf("    driver ver   : 0x%08x  vendor 0x%04x  device 0x%04x\n",
                props.driverVersion, props.vendorID, props.deviceID);
    std::printf("    vulkan API   : %u.%u.%u\n",
                VK_API_VERSION_MAJOR(props.apiVersion),
                VK_API_VERSION_MINOR(props.apiVersion),
                VK_API_VERSION_PATCH(props.apiVersion));
    std::printf("    flow         : committed OpticalFlowPipeline (search_radius=2, the shipping default)\n");
    std::printf("    mv flags     : PHYR_SUBPEL / PHYR_AFFINE / PHYR_COARSE_WIDE / PHYR_CANDSEL env-gated, default OFF (actual state on the [arm] line below)\n");
    std::printf("    NOTE         : Mode A drives the COMMITTED warp shader (see README sign caveat).\n\n");

    VkDevice dev = VK_NULL_HANDLE; VkQueue q = VK_NULL_HANDLE;
    {
        const float prio = 1.0f;
        VkDeviceQueueCreateInfo qci{}; qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qci.queueFamilyIndex = qf; qci.queueCount = 1u; qci.pQueuePriorities = &prio;
        VkDeviceCreateInfo dci{}; dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        dci.queueCreateInfoCount = 1u; dci.pQueueCreateInfos = &qci;
        if (vkCreateDevice(phys, &dci, nullptr, &dev) != VK_SUCCESS) { std::printf("[FAIL] device\n"); return 1; }
        vkGetDeviceQueue(dev, qf, 0u, &q);
    }

    // ── shared resources ──────────────────────────────────────────────────────
    const VkDeviceSize img_bytes = VkDeviceSize(W) * H * 4u;
    Image A, B, C;
    A.create(dev, phys, W, H, VK_FORMAT_R8G8B8A8_UNORM,
             VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    B.create(dev, phys, W, H, VK_FORMAT_R8G8B8A8_UNORM,
             VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    C.create(dev, phys, W, H, VK_FORMAT_R8G8B8A8_UNORM,
             VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    HostBuf upA, upB, down;
    upA.create(dev, phys, img_bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    upB.create(dev, phys, img_bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    down.create(dev, phys, img_bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT);

    VkCommandPool pool = VK_NULL_HANDLE;
    { VkCommandPoolCreateInfo pi{}; pi.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
      pi.queueFamilyIndex = qf; pi.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
      vkCreateCommandPool(dev, &pi, nullptr, &pool); }
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    { VkCommandBufferAllocateInfo ai{}; ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
      ai.commandPool = pool; ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; ai.commandBufferCount = 1u;
      vkAllocateCommandBuffers(dev, &ai, &cmd); }
    VkFence fence = VK_NULL_HANDLE;
    { VkFenceCreateInfo fi{}; fi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
      vkCreateFence(dev, &fi, nullptr, &fence); }

    auto submit_wait = [&]() {
        vkEndCommandBuffer(cmd);
        VkSubmitInfo si{}; si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        si.commandBufferCount = 1u; si.pCommandBuffers = &cmd;
        vkQueueSubmit(q, 1u, &si, fence);
        vkWaitForFences(dev, 1u, &fence, VK_TRUE, UINT64_MAX);
        vkResetFences(dev, 1u, &fence);
        vkResetCommandBuffer(cmd, 0);
    };
    auto begin = [&]() {
        VkCommandBufferBeginInfo bi{}; bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        vkBeginCommandBuffer(cmd, &bi);
    };
    auto full_copy = []() { VkBufferImageCopy c{};
        c.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}; return c; };

    // Put C (warp output) into GENERAL once and keep it there (record_optical_flow
    // pre-condition: c_view in GENERAL — OpticalFlowPipeline.hpp:140).
    begin();
    barrier(cmd, C.img, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
            0, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    submit_wait();

    // The committed flow pipeline (shipping default search_radius=2). ARC-A LEVER-1b: the three matcher
    // arms are ENV-TOGGLED, ALL default OFF (an unset/zero var = the byte-identical baseline), so the
    // scorer measures the OFF baseline by default and each lever in isolation on demand:
    //   PHYR_SUBPEL=1      → mv_subpel   (finest-level sub-pixel parabolic MV refinement)
    //   PHYR_AFFINE=1      → mv_affine   (per-tile affine companion field — needs the aff field for flow_disc)
    //   PHYR_COARSE_WIDE=1 → coarse_wide (decoupled coarsest-radius rider 6→8; was net-negative riding affine)
    //   PHYR_CANDSEL=1     → mv_candsel  (holonic candidate-selection: ambiguous interior tiles ADOPT the
    //                                     coarse region-predictor MV — attacks the textureless-interior crossfade)
    // The translational MV/SAD outputs are byte-identical with subpel/affine/coarse_wide/candsel OFF.
    auto env_on = [](const char* name) -> bool {
        const char* v = std::getenv(name);
        return v && std::atoi(v) != 0;
    };
    const bool arm_subpel      = env_on("PHYR_SUBPEL");
    const bool arm_affine      = env_on("PHYR_AFFINE");
    const bool arm_coarse_wide = env_on("PHYR_COARSE_WIDE");
    const bool arm_candsel     = env_on("PHYR_CANDSEL");
    std::printf("[arm] PHYR_SUBPEL=%d  PHYR_AFFINE=%d  PHYR_COARSE_WIDE=%d  PHYR_CANDSEL=%d  (all default OFF)\n",
                arm_subpel ? 1 : 0, arm_affine ? 1 : 0, arm_coarse_wide ? 1 : 0, arm_candsel ? 1 : 0);
    phyriad::render::vulkan::OpticalFlowPipeline ofp;
    if (!ofp.init(phys, dev, W, H, 2u /*max_in_flight*/,
                  2 /*search_radius*/, 32.0f /*residual_ceil*/, 0.5f /*improvement_frac*/,
                  0.20f /*agreement_thresh*/, false /*emit_second_best*/, arm_affine /*mv_affine*/,
                  arm_subpel /*mv_subpel*/, arm_coarse_wide /*coarse_wide*/, arm_candsel /*mv_candsel*/)) {
        std::printf("[FAIL] OpticalFlowPipeline::init\n"); return 1;
    }
    const bool kHaveAffine = ofp.mv_affine() && (ofp.aff_image() != VK_NULL_HANDLE);

    // ── flow-discrepancy (the FloLPIPS-analogue, §11 + arXiv 2207.08119) ──
    // FloLPIPS weights the artifact map by the optical-flow DISCREPANCY between reference and
    // output. We adopt the mechanism honestly with what the pipeline gives us: its MV field
    // (motion_image(), RG16F at W/8×H/8) AND — ARC-A LEVER-1 — the per-tile affine companion field
    // (aff_image(), RGBA16F, the 2x2 linear part M). For a held-out triple we run flow(prev→next)
    // [the candidate's motion] and flow(prev→truth) [the TRUE midpoint motion], read BOTH MV fields
    // and BOTH affine fields.
    //
    // CRITICAL (the gate-precondition the brief mandates): the per-tile model must be evaluated
    // PER-PIXEL (at several SUB-TILE points), NOT at the tile centre — at the centre the affine ≈ the
    // translation (M·0 = 0), so a centre-only flow_disc would show a FALSE NULL and the affine's whole
    // benefit (the intra-tile divergence/curl) would be INVISIBLE to the gate. We sample a 3×3 grid of
    // sub-tile offsets o (tile units, in [−0.375,+0.375] → spanning ~6 of the 8 px tile) and at each:
    //     candidate per-pixel flow at the midpoint = 0.5·(mv_cand + M_cand·o)
    //     truth     per-pixel flow                 =      mv_ref  + M_ref ·o      (the held-out truth's own affine fit)
    //     contribution = |candidate − truth|, averaged over the 9 sub-points and the confident tiles.
    // The translational baseline is the o=(0,0) term (M·0=0); the OFF instrument (M absent) collapses
    // to exactly the old centre-only |0.5·mv_cand − mv_ref|. So when M is byte-identical-zero (pure
    // translation), the new metric equals the old one → no false delta on rigid pans; the reduction on
    // zoom/orbit/mixed is the genuine intra-tile model benefit. flow_disc reads the RAW matcher field
    // (the gate is visible). FR-only (needs the held-out truth); Mode B/T = N/A.
    const uint32_t MVW = ofp.motion_width(), MVH = ofp.motion_height();
    HostBuf mvdown;
    mvdown.create(dev, phys, VkDeviceSize(MVW) * MVH * 8u, VK_BUFFER_USAGE_TRANSFER_DST_BIT);  // RG16F=4B, RGBA16F=8B/texel
    std::vector<float> mvCand, mvRef, sadCand, sadRef;   // (dx,dy) and (sad_best,sad_zero) per tile
    std::vector<float> affCand, affRef;                  // ARC-A LEVER-1: per-tile M=(a,b,c,d) — 4 floats/tile
    // Read an RG16F field (motion_image / sad_field_image) back to host floats (2 components/tile). Both
    // are in SHADER_READ_ONLY_OPTIMAL post-call and both start the NEXT call from UNDEFINED
    // (OpticalFlowPipeline.cpp), so leaving them in TRANSFER_SRC after the copy is safe.
    auto read_field = [&](VkImage img, std::vector<float>& out) {
        begin();
        barrier(cmd, img, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
        { VkBufferImageCopy r = full_copy(); r.imageExtent = {MVW, MVH, 1};
          vkCmdCopyImageToBuffer(cmd, img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, mvdown.buf, 1, &r); }
        submit_wait();
        out.resize(size_t(MVW) * MVH * 2u);
        const uint16_t* p = static_cast<const uint16_t*>(mvdown.mapped);
        for (size_t i = 0; i < out.size(); ++i) out[i] = half_to_float(p[i]);
    };
    // ARC-A LEVER-1: read the RGBA16F affine companion field (4 components/tile = M). Same layout
    // discipline as read_field (SHADER_READ_ONLY post-call). Only valid when armed (kHaveAffine).
    auto read_affine = [&](VkImage img, std::vector<float>& out) {
        begin();
        barrier(cmd, img, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
        { VkBufferImageCopy r = full_copy(); r.imageExtent = {MVW, MVH, 1};
          vkCmdCopyImageToBuffer(cmd, img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, mvdown.buf, 1, &r); }
        submit_wait();
        out.resize(size_t(MVW) * MVH * 4u);
        const uint16_t* p = static_cast<const uint16_t*>(mvdown.mapped);
        for (size_t i = 0; i < out.size(); ++i) out[i] = half_to_float(p[i]);
    };
    // SAD-confidence gate: a tile contributes to flow_disc only if BOTH the candidate match
    // (prev→next) AND the reference match (prev→truth) are CONFIDENT — sad_best below the warp's
    // own residual_ceil (32.0). This removes the block-match's periodic-texture / aperture ambiguity
    // (a wrong-but-low... no: a wrong match has HIGH sad_best, so the gate drops it) that otherwise
    // inflates the raw discrepancy (the verified pan_diag finding). The MV field's R channel is dx,
    // the SAD field's R channel is sad_best — both at index 2*t.
    const double kSadConf = 32.0;   // = OpticalFlowPipeline residual_ceil default (tiles the warp itself trusts)
    // 3×3 sub-tile sample offsets in TILE units (the affine field's gradient is px-per-tile-step).
    // The MV-grid is 8px/tile, so o∈{−0.375,0,+0.375} covers ~6 of the 8 px around the tile centre.
    static const double kSubOff[3] = { -0.375, 0.0, 0.375 };
    auto flow_discrepancy = [&](double& gated_frac) -> double {
        const size_t n = std::min(mvCand.size(), mvRef.size()) / 2u;
        if (n == 0) { gated_frac = 0.0; return -1.0; }
        // M present only when armed AND both affine reads succeeded (4 floats/tile); else M=0 → the
        // metric reduces EXACTLY to the centre-only translational form (honest OFF fallback).
        const bool haveM = kHaveAffine && affCand.size() >= n * 4u && affRef.size() >= n * 4u;
        double acc = 0.0; size_t used = 0;
        for (size_t t = 0; t < n; ++t) {
            if (sadCand[2*t] >= kSadConf || sadRef[2*t] >= kSadConf) continue;  // ambiguous/failed match → skip
            // candidate (midpoint, t=0.5) and truth (held-out) per-tile translational MV.
            const double cu = mvCand[2*t],   cv = mvCand[2*t+1];
            const double ru = mvRef[2*t],    rv = mvRef[2*t+1];
            // 2x2 linear parts M = (a,b,c,d): u' = a·ox + b·oy, v' = c·ox + d·oy.
            double ca=0, cb=0, cc=0, cd=0, ra=0, rb=0, rc=0, rd=0;
            if (haveM) {
                ca = affCand[4*t]; cb = affCand[4*t+1]; cc = affCand[4*t+2]; cd = affCand[4*t+3];
                ra = affRef [4*t]; rb = affRef [4*t+1]; rc = affRef [4*t+2]; rd = affRef [4*t+3];
            }
            double tile_acc = 0.0; int sub = 0;
            for (int oy = 0; oy < 3; ++oy)
                for (int ox = 0; ox < 3; ++ox) {
                    const double sx = kSubOff[ox], sy = kSubOff[oy];
                    // candidate per-pixel flow at the midpoint = 0.5·(mv + M·o); truth = mv_ref + M_ref·o.
                    const double cdx = 0.5 * (cu + ca*sx + cb*sy);
                    const double cdy = 0.5 * (cv + cc*sx + cd*sy);
                    const double rdx = ru + ra*sx + rb*sy;
                    const double rdy = rv + rc*sx + rd*sy;
                    const double dx = cdx - rdx, dy = cdy - rdy;
                    tile_acc += std::sqrt(dx*dx + dy*dy); ++sub;
                }
            acc += tile_acc / double(sub); ++used;
        }
        gated_frac = double(used) / double(n);
        return used ? acc / double(used) : -1.0;   // no confident tiles → N/A
    };
    Agg aFD_A, aFDcov_A;   // flow-discrepancy + its confident-tile coverage (Mode A only)

    // CSV.
    FILE* csv = nullptr;
    if (!csv_path.empty()) {
        csv = std::fopen(csv_path.c_str(), "wb");
        if (csv) std::fprintf(csv,
            "triple,mode,psnr_db,ssim,crossfade_residual,crossfade_alpha,double_edge_energy,double_edge_masked,motion_coverage,obj_iou,nonrigid_deform,flow_discrepancy\n");
        else std::printf("  [warn] could not open CSV '%s' — stdout only.\n", csv_path.c_str());
    }

    std::printf("Method (§11.3 held-out): score each candidate midpoint vs the held-out real N+1.\n");
    std::printf("  Mode A = committed flow+warp on (N, N+2)@t=0.5 (OUR flow+warp).\n");
    std::printf("  Mode B = the live wapOutA for this midpoint, if 'live=' present (FULL pipeline).\n");
    std::printf("  Mode T = TRUTH-LESS (--qdump: prev/next/live, NO mid) — crossfade + double-edge only.\n");
    std::printf("  All truth-based metrics vs truth_mid, %u px border crop. See README for each metric.\n\n", border);

    std::printf("%-18s %-6s | %8s %7s | %9s %6s %9s %9s %5s | %7s %9s | %8s\n",
                "triple", "mode", "PSNR dB", "SSIM", "xfade_res", "alpha", "dbl_edge",
                "dbl_edg_m", "mcov", "obj_IoU", "nonrigid", "flowdsc");
    std::printf("%s\n", std::string(120, '-').c_str());

    Agg aPSNR_A, aSSIM_A, aXR_A, aDE_A, aIoU_A, aND_A;
    Agg aPSNR_B, aSSIM_B, aXR_B, aDE_B, aIoU_B, aND_B;
    Agg aXR_T, aDE_T;   // §11.4 layer 1: truth-less aggregates (only crossfade + double-edge are defined)
    // motion-masked dbl_edge + its coverage (per mode) — the localized-artifact metric.
    Agg aDEM_A, aMCOV_A, aDEM_B, aMCOV_B, aDEM_T, aMCOV_T;
    Agg aCRES_T, aCCOV_T;   // crescent-band energy + coverage (truth-less) — the crescent-SPECIFIC metric
    std::vector<WorstRow> worst;   // every scored row, for worst-frame surfacing over the segment

    std::vector<uint8_t> hN, hM, hP, hLive;

    auto print_row = [&](const std::string& nm, const char* mode, const uint8_t* cand, double flow_disc) {
        double alpha = 0.0, mcov = 0.0;
        const double psnr = psnr_rgb(cand, hM.data(), W, H, border);
        const double ssim = ssim_luma(cand, hM.data(), W, H, border);
        const double xr   = crossfade_residual(cand, hN.data(), hP.data(), W, H, border, alpha);
        const double de   = double_edge_energy(cand, hN.data(), hP.data(), hM.data(), W, H, border, kEdgeThr);
        const double dem  = double_edge_energy_masked(cand, hN.data(), hP.data(), hM.data(), W, H, border,
                                                       kEdgeThr, kMotionThr, mcov);
        const ObjMetrics om = gravity_metric(cand, hM.data(), W, H, border, kSatThr, kLumaDevThr);

        char iou_s[16], nd_s[16], fd_s[16];
        if (om.iou >= 0) std::snprintf(iou_s, sizeof iou_s, "%7.4f", om.iou);
        else             std::snprintf(iou_s, sizeof iou_s, "%7s", "N/A");
        if (om.nonrigid >= 0) std::snprintf(nd_s, sizeof nd_s, "%9.4f", om.nonrigid);
        else                  std::snprintf(nd_s, sizeof nd_s, "%9s", "N/A");
        if (flow_disc >= 0)   std::snprintf(fd_s, sizeof fd_s, "%8.4f", flow_disc);
        else                  std::snprintf(fd_s, sizeof fd_s, "%8s", "N/A");

        std::printf("%-18s %-6s | %8.3f %7.4f | %9.3f %6.3f %9.4f %9.4f %5.3f | %s %s | %s\n",
                    nm.c_str(), mode, psnr, ssim, xr, alpha, de, dem, mcov, iou_s, nd_s, fd_s);

        if (csv) std::fprintf(csv, "%s,%s,%.4f,%.5f,%.4f,%.4f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f\n",
                              nm.c_str(), mode, psnr, ssim, xr, alpha, de, dem, mcov,
                              om.iou, om.nonrigid, flow_disc);
        // (aFD_A is accumulated in the Mode-A orchestration, where fd coverage is also available)

        Agg &p = (mode[0] == 'A') ? aPSNR_A : aPSNR_B; p.add(psnr);
        Agg &s = (mode[0] == 'A') ? aSSIM_A : aSSIM_B; s.add(ssim);
        Agg &x = (mode[0] == 'A') ? aXR_A   : aXR_B;   x.add(xr);
        Agg &e = (mode[0] == 'A') ? aDE_A   : aDE_B;   e.add(de);
        Agg &dm = (mode[0] == 'A') ? aDEM_A  : aDEM_B;  dm.add(dem);
        Agg &mc = (mode[0] == 'A') ? aMCOV_A : aMCOV_B; mc.add(mcov);
        if (om.iou >= 0)      { Agg &i = (mode[0] == 'A') ? aIoU_A : aIoU_B; i.add(om.iou); }
        if (om.nonrigid >= 0) { Agg &n = (mode[0] == 'A') ? aND_A  : aND_B;  n.add(om.nonrigid); }
        worst.push_back({dem, mcov, psnr, nm, std::string(mode)});
    };

    // §11.4 layer 1: TRUTH-LESS row — the in-app --qdump form (prev/next/live, NO mid). There is no
    // held-out ground truth, so the truth-DEPENDENT metrics (PSNR/SSIM/obj_IoU/nonrigid) are N/A; only
    // the truth-INDEPENDENT crossfade_residual (+alpha) and double_edge_energy run. double_edge_energy
    // needs a 4th `truth` frame for its spurious-edge AND-mask; with no truth we pass `next` as the
    // mask-substitute (cand-edge must be absent in prev AND next), which is the honest truth-less
    // analogue — it cannot consult the held-out N+1, so a real edge that exists ONLY in N+1 is not
    // suppressed. Mode tag "T" (truth-less). Aggregates into aXR_T/aDE_T.
    auto print_row_truthless = [&](const std::string& nm, const uint8_t* cand) {
        double alpha = 0.0, mcov = 0.0, ccov = 0.0;
        const double xr = crossfade_residual(cand, hN.data(), hP.data(), W, H, border, alpha);
        const double de = double_edge_energy(cand, hN.data(), hP.data(), /*truth=*/hP.data(), W, H, border, kEdgeThr);
        const double dem = double_edge_energy_masked(cand, hN.data(), hP.data(), /*truth=*/hP.data(), W, H, border,
                                                      kEdgeThr, kMotionThr, mcov);
        const double cres = crescent_band_energy(cand, hN.data(), hP.data(), W, H, border,
                                                 kEdgeThr, kSatThr, kLumaDevThr, ccov);
        std::printf("%-18s %-6s | %8s %7s | %9.3f %6.3f %9.4f %9.4f %5.3f | %7s %9s | %8s  cres %8.4f ccov %5.3f  (truth-less)\n",
                    nm.c_str(), "T", "N/A", "N/A", xr, alpha, de, dem, mcov, "N/A", "N/A", "N/A", cres, ccov);
        if (csv) std::fprintf(csv, "%s,%s,,,%.4f,%.4f,%.5f,%.5f,%.5f,,,,%.5f,%.5f\n", nm.c_str(), "T", xr, alpha, de, dem, mcov, cres, ccov);
        aXR_T.add(xr); aDE_T.add(de); aDEM_T.add(dem); aMCOV_T.add(mcov); aCRES_T.add(cres); aCCOV_T.add(ccov);
        worst.push_back({dem, mcov, -1.0 /*no truth*/, nm, std::string("T")});
    };

    for (const auto& tr : man.triples) {
        // §11.4 layer 1: a truth-less triple (prev/next/live, no mid) takes a SEPARATE path — it never
        // reads mid (there is none) and runs only crossfade + double-edge of live vs (prev, next).
        if (tr.truthless) {
            if (!read_file(tr.prev_path, hN) || !read_file(tr.next_path, hP) ||
                !read_file(tr.live_path, hLive)) {
                std::printf("%-18s [skip] missing one of prev/next/live .rgba (truth-less)\n", tr.name.c_str());
                continue;
            }
            if (hN.size() != size_t(img_bytes) || hP.size() != size_t(img_bytes) ||
                hLive.size() != size_t(img_bytes)) {
                std::printf("%-18s [skip] truth-less .rgba size != %ux%u*4 (= %lld bytes)\n",
                            tr.name.c_str(), W, H, (long long)img_bytes);
                continue;
            }
            print_row_truthless(tr.name, hLive.data());
            continue;
        }
        if (!read_file(tr.prev_path, hN) || !read_file(tr.mid_path, hM) || !read_file(tr.next_path, hP)) {
            std::printf("%-18s [skip] missing one of prev/mid/next .rgba\n", tr.name.c_str());
            continue;
        }
        if (hN.size() != size_t(img_bytes) || hM.size() != size_t(img_bytes) || hP.size() != size_t(img_bytes)) {
            std::printf("%-18s [skip] .rgba size != %ux%u*4 (= %lld bytes)\n",
                        tr.name.c_str(), W, H, (long long)img_bytes);
            continue;
        }

        // ── Mode A: committed flow+warp on (N, N+2) @ t=0.5 → C → readback ──────
        std::memcpy(upA.mapped, hN.data(), size_t(img_bytes));
        std::memcpy(upB.mapped, hP.data(), size_t(img_bytes));
        begin();
        barrier(cmd, A.img, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
        barrier(cmd, B.img, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
        { VkBufferImageCopy r = full_copy(); r.imageExtent = {W, H, 1};
          vkCmdCopyBufferToImage(cmd, upA.buf, A.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &r);
          vkCmdCopyBufferToImage(cmd, upB.buf, B.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &r); }
        barrier(cmd, A.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        barrier(cmd, B.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        submit_wait();

        begin();
        if (!ofp.record_optical_flow(cmd, A.vw, B.vw, C.vw, 0.5f)) {
            std::printf("%-18s [skip] record_optical_flow failed\n", tr.name.c_str());
            submit_wait(); continue;
        }
        submit_wait();
        read_field(ofp.motion_image(),    mvCand);    // mv(prev→next) — candidate motion, BEFORE the ref pass
        read_field(ofp.sad_field_image(), sadCand);   // sad_best(prev→next) for the confidence gate
        if (kHaveAffine) read_affine(ofp.aff_image(), affCand);  // ARC-A LEVER-1: M(prev→next) per tile

        // readback C (GENERAL) → down
        begin();
        barrier(cmd, C.img, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
        { VkBufferImageCopy r = full_copy(); r.imageExtent = {W, H, 1};
          vkCmdCopyImageToBuffer(cmd, C.img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, down.buf, 1, &r); }
        barrier(cmd, C.img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
                VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        submit_wait();

        // ── reference flow: flow(prev → truth) → mv_ref, for the flow-discrepancy metric ──
        // Re-upload the held-out truth into B (was 'next'); A (prev) stays SHADER_READ. The warp
        // output C is recomputed but UNUSED (we only want the MV). C stays GENERAL (its post-readback
        // layout) so record_optical_flow's c_view precondition holds.
        std::memcpy(upB.mapped, hM.data(), size_t(img_bytes));
        begin();
        barrier(cmd, B.img, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
        { VkBufferImageCopy r = full_copy(); r.imageExtent = {W, H, 1};
          vkCmdCopyBufferToImage(cmd, upB.buf, B.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &r); }
        barrier(cmd, B.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        submit_wait();
        begin();
        double flow_disc = -1.0, fd_cov = 0.0;
        if (ofp.record_optical_flow(cmd, A.vw, B.vw, C.vw, 0.5f)) {
            submit_wait();
            read_field(ofp.motion_image(),    mvRef);     // mv(prev→truth) — the true midpoint motion
            read_field(ofp.sad_field_image(), sadRef);    // sad_best(prev→truth) for the gate
            if (kHaveAffine) read_affine(ofp.aff_image(), affRef);  // ARC-A LEVER-1: M(prev→truth) per tile
            flow_disc = flow_discrepancy(fd_cov);
            if (flow_disc >= 0) { aFD_A.add(flow_disc); aFDcov_A.add(fd_cov); }
        } else { submit_wait(); }

        print_row(tr.name, "A", static_cast<const uint8_t*>(down.mapped), flow_disc);

        // ── Mode B: score the dumped live wapOutA vs truth_mid (if present) ────
        // flow_disc is a flow+warp (Mode A) concept — the live output is the FULL pipeline, whose
        // internal flow we do not recompute here → N/A for Mode B.
        if (tr.has_live) {
            if (read_file(tr.live_path, hLive) && hLive.size() == size_t(img_bytes))
                print_row(tr.name, "B", hLive.data(), -1.0);
            else
                std::printf("%-18s [skip B] live .rgba missing or wrong size\n", tr.name.c_str());
        }
    }

    // ── aggregate (mean ± σ) ──────────────────────────────────────────────────
    auto print_agg = [](const char* mode, const Agg& p, const Agg& s, const Agg& x,
                        const Agg& e, const Agg& iou, const Agg& nd) {
        if (p.n == 0) return;
        std::printf("  mode %-2s (n=%d): PSNR %.3f±%.3f  SSIM %.4f±%.4f  xfade_res %.3f±%.3f  "
                    "dbl_edge %.4f±%.4f  IoU %.4f±%.4f(n=%d)  nonrigid %.4f±%.4f(n=%d)\n",
                    mode, p.n, p.mean(), p.sigma(), s.mean(), s.sigma(), x.mean(), x.sigma(),
                    e.mean(), e.sigma(), iou.mean(), iou.sigma(), iou.n, nd.mean(), nd.sigma(), nd.n);
    };
    std::printf("\n── aggregate (mean ± σ) ──\n");
    print_agg("A", aPSNR_A, aSSIM_A, aXR_A, aDE_A, aIoU_A, aND_A);
    print_agg("B", aPSNR_B, aSSIM_B, aXR_B, aDE_B, aIoU_B, aND_B);
    // §11.4 layer 1: truth-less aggregate — only crossfade + double-edge are defined (the rest are N/A).
    if (aXR_T.n > 0) {
        std::printf("  mode T  (n=%d): xfade_res %.3f±%.3f  dbl_edge %.4f±%.4f  "
                    "(truth-less: PSNR/SSIM/IoU/nonrigid N/A — no held-out ground truth)\n",
                    aXR_T.n, aXR_T.mean(), aXR_T.sigma(), aDE_T.mean(), aDE_T.sigma());
        std::printf("  mode T  crescent (n=%d): cres %.4f±%.4f  band_cov %.3f±%.3f  "
                    "(the crescent-SPECIFIC metric — spurious edge in the silhouette swept-band only)\n",
                    aCRES_T.n, aCRES_T.mean(), aCRES_T.sigma(), aCCOV_T.mean(), aCCOV_T.sigma());
    }
    // motion-masked dbl_edge aggregate (the localized-artifact metric — read with coverage).
    auto print_masked = [](const char* mode, const Agg& dm, const Agg& mc) {
        if (dm.n == 0) return;
        std::printf("  mode %-2s masked (n=%d): dbl_edge_m %.4f±%.4f  motion_cov %.3f±%.3f\n",
                    mode, dm.n, dm.mean(), dm.sigma(), mc.mean(), mc.sigma());
    };
    print_masked("A", aDEM_A, aMCOV_A);
    print_masked("B", aDEM_B, aMCOV_B);
    print_masked("T", aDEM_T, aMCOV_T);
    if (aFD_A.n > 0)
        std::printf("  mode A  flow-disc (n=%d): flow_disc %.4f±%.4f px  conf_tiles %.0f%%  "
                    "(|0.5*mv(prev->next) - mv(prev->truth)| over SAD-confident tiles only)\n",
                    aFD_A.n, aFD_A.mean(), aFD_A.sigma(), 100.0 * aFDcov_A.mean());

    // ── worst-frame surfacing — the N frames with the highest masked artifact energy ──
    // (the automated replacement for the operator's manual slow-mo hunt for the bad frame).
    if (worst.size() > 1) {
        std::sort(worst.begin(), worst.end(),
                  [](const WorstRow& a, const WorstRow& b) { return a.dem > b.dem; });
        const size_t k = std::min<size_t>(12, worst.size());
        std::printf("\n── worst %zu frames (by dbl_edg_m — the localized-artifact metric) ──\n", k);
        for (size_t i = 0; i < k; ++i) {
            const WorstRow& r = worst[i];
            if (r.psnr >= 0)
                std::printf("  %2zu. %-18s [%s]  dbl_edg_m %8.4f  mcov %5.3f  PSNR %6.3f dB\n",
                            i + 1, r.name.c_str(), r.mode.c_str(), r.dem, r.mcov, r.psnr);
            else
                std::printf("  %2zu. %-18s [%s]  dbl_edg_m %8.4f  mcov %5.3f  PSNR    N/A\n",
                            i + 1, r.name.c_str(), r.mode.c_str(), r.dem, r.mcov);
        }
    }

    std::printf("\nReading the metrics (full definitions in README.md):\n");
    std::printf("  PSNR/SSIM    : higher = closer to the held-out truth (reused from stage11).\n");
    std::printf("  xfade_res    : RMSE of the best α-blend fit. LOW + low PSNR = crossfading (BAD);\n");
    std::printf("                 LOW + high PSNR = harmless (little motion). HIGH = genuine motion comp.\n");
    std::printf("  dbl_edge     : per-px spurious-edge energy (seam present in cand, in NO real frame). Lower = better.\n");
    std::printf("  dbl_edg_m    : SAME spurious-edge energy but restricted to + normalised by the MOVING region\n");
    std::printf("                 (|luma(prev)-luma(next)|>thr) — undilutes a localized ghost. Lower=better. PRIMARY artifact metric.\n");
    std::printf("  mcov         : motion coverage = moving px / interior px. High dbl_edg_m + LOW mcov = a localized ghost (q1).\n");
    std::printf("  obj_IoU      : silhouette overlap of the dominant object, cand vs truth (1 = identical). HEURISTIC.\n");
    std::printf("  nonrigid     : centroid-aligned symmetric-difference / area (0 = rigid; >0 = shape changed). HEURISTIC.\n");
    std::printf("  flowdsc      : flow-discrepancy px = mean over confident tiles of the PER-PIXEL |candidate - truth|\n");
    std::printf("                 flow, evaluated at a 3x3 SUB-TILE grid: cand=0.5*(mv+M*o), truth=mv_ref+M_ref*o (M = the\n");
    std::printf("                 ARC-A per-tile affine 2x2). At the tile CENTRE M*0=0 (the affine would falsely null) —\n");
    std::printf("                 the sub-tile sampling exposes the intra-tile divergence/curl the single MV cannot model.\n");
    std::printf("                 ~0 = motion tracked; large = non-translational (zoom/rot) under-modelled. Mode A only.\n");
    std::printf("  LPIPS        : NOT implemented (needs a learned ONNX model) — deliberate future add (README).\n");

    if (csv) { std::fclose(csv); std::printf("\nCSV written: %s\n", csv_path.c_str()); }

    // ── teardown (§13) ────────────────────────────────────────────────────────
    ofp.shutdown(dev);
    vkDestroyFence(dev, fence, nullptr);
    vkDestroyCommandPool(dev, pool, nullptr);
    mvdown.destroy(dev); down.destroy(dev); upB.destroy(dev); upA.destroy(dev);
    C.destroy(dev); B.destroy(dev); A.destroy(dev);
    vkDestroyDevice(dev, nullptr);
    vkDestroyInstance(inst, nullptr);
    std::printf("=====================================================================================\n");
    return 0;
}
// Made with my soul - Swately <3
