// gme_fit_bench — microbenchmark for the CPU global-motion-estimation tail of the frame generator.
//
// Breaks the per-pair CPU cost of gme_fit_affine into its fp16 decode, its FMA reduction, and its
// dissidence pass, and measures the cost of the every-2nd-block subsample (sub2). Runs on a
// synthetic 240x135 (1080p/8) RG16F motion-vector grid. Console program, no overlay.
//
// gme_fit_affine, half_to_float, and float_to_half are scalar copies of the shipping frame-generator
// path so the measured cost matches it exactly. Build:
//   g++ -O2 -std=c++23 main.cpp -o gme_fit_bench         (MinGW)
//   cl  /O2 /std:c++latest /EHsc main.cpp                 (MSVC)

#include <cstdint>
#include <cstring>
#include <cmath>
#include <cstdio>
#include <vector>
#include <chrono>
#include <algorithm>
#include <phyriad/hal/Simd.hpp>   // the batch fp16->fp32 primitive under test (hal::f16_to_f32_batch)

// ── Portable (F16C-free) half->float decode ──────────────────────────────────
static inline float half_to_float(uint16_t h){
    const uint32_t sign = (uint32_t)(h & 0x8000u) << 16;
    const uint32_t exp  = (h >> 10) & 0x1Fu;
    const uint32_t mant = h & 0x3FFu;
    uint32_t bits;
    if (exp == 0u) {
        if (mant == 0u) { bits = sign; }
        else {
            uint32_t m = mant; int e = -1;
            do { m <<= 1; ++e; } while ((m & 0x400u) == 0u);
            m &= 0x3FFu;
            bits = sign | (uint32_t)((127 - 15 - e) << 23) | (m << 13);
        }
    } else if (exp == 0x1Fu) {
        bits = sign | 0x7F800000u | (mant << 13);
    } else {
        bits = sign | (uint32_t)((int)exp - 15 + 127) << 23 | (mant << 13);
    }
    float out; std::memcpy(&out, &bits, sizeof(out)); return out;
}
// ── float->half encode (used only to build the synthetic grid) ───────────────
static inline uint16_t float_to_half(float f){
    uint32_t x; std::memcpy(&x, &f, sizeof(x));
    const uint32_t sign = (x >> 16) & 0x8000u;
    int32_t  exp  = (int32_t)((x >> 23) & 0xFFu) - 127 + 15;
    uint32_t mant = x & 0x7FFFFFu;
    if (((x >> 23) & 0xFFu) == 0xFFu) return (uint16_t)(sign | 0x7C00u | (mant ? 0x200u : 0u));
    if (exp >= 0x1F) return (uint16_t)(sign | 0x7C00u);
    if (exp <= 0) {
        if (exp < -10) return (uint16_t)sign;
        mant |= 0x800000u;
        const int shift = 14 - exp;
        const uint32_t halfmant = mant >> shift;
        const uint32_t rem = mant & ((1u << shift) - 1u);
        const uint32_t halfway = 1u << (shift - 1);
        uint32_t r = halfmant;
        if (rem > halfway || (rem == halfway && (halfmant & 1u))) ++r;
        return (uint16_t)(sign | r);
    }
    const uint32_t halfmant = mant >> 13;
    const uint32_t rem = mant & 0x1FFFu;
    uint32_t out = (uint32_t)(exp << 10) | halfmant;
    if (rem > 0x1000u || (rem == 0x1000u && (halfmant & 1u))) ++out;
    return (uint16_t)(sign | out);
}

static constexpr float kChangeGateSadZ = 0.5f;
// ── gme_fit_affine: IRLS-weighted least-squares affine fit of the MV grid (scalar path) ──
static double gme_fit_affine(const void* mv_raw, const void* sad_raw, uint32_t mvw, uint32_t mvh,
                             float out6[6], uint8_t* dis_out, bool sub2){
    const uint16_t* h = (const uint16_t*)mv_raw;
    const int step = sub2 ? 2 : 1;
    double a=0,b=0,c=0,d=0,e=0,f=0;
    for (int iter = 0; iter < 3; ++iter) {
        double Sw=0,Swx=0,Swy=0,Swxx=0,Swxy=0,Swyy=0;
        double bx0=0,bx1=0,bx2=0;
        double by0=0,by1=0,by2=0;
        for (uint32_t gy = 0; gy < mvh; gy += (uint32_t)step) {
            for (uint32_t gx = 0; gx < mvw; gx += (uint32_t)step) {
                const size_t idx = ((size_t)gy * mvw + gx) * 2u;
                const float u = half_to_float(h[idx + 0]);
                const float v = half_to_float(h[idx + 1]);
                double w = 1.0;
                if (iter > 0) {
                    const double rx = u - (a + b*gx + c*gy);
                    const double ry = v - (d + e*gx + f*gy);
                    const double r  = std::sqrt(rx*rx + ry*ry);
                    const double rr = r / 2.0;
                    w = 1.0 / (1.0 + rr*rr);
                }
                const double gxx=(double)gx, gyy=(double)gy;
                Sw+=w; Swx+=w*gxx; Swy+=w*gyy; Swxx+=w*gxx*gxx; Swxy+=w*gxx*gyy; Swyy+=w*gyy*gyy;
                bx0+=w*u; bx1+=w*u*gxx; bx2+=w*u*gyy;
                by0+=w*v; by1+=w*v*gxx; by2+=w*v*gyy;
            }
        }
        const double det = Sw*(Swxx*Swyy - Swxy*Swxy)
                         - Swx*(Swx*Swyy - Swxy*Swy)
                         + Swy*(Swx*Swxy - Swxx*Swy);
        if (std::fabs(det) < 1e-9) { out6[0]=(float)a;out6[1]=(float)b;out6[2]=(float)c;out6[3]=(float)d;out6[4]=(float)e;out6[5]=(float)f; break; }
        const double invdet = 1.0 / det;
        auto solve3 = [&](double r0,double r1,double r2,double& p0,double& p1,double& p2){
            p0 = invdet * ( r0*(Swxx*Swyy - Swxy*Swxy) - Swx*(r1*Swyy - Swxy*r2) + Swy*(r1*Swxy - Swxx*r2) );
            p1 = invdet * ( Sw*(r1*Swyy - Swxy*r2) - r0*(Swx*Swyy - Swxy*Swy) + Swy*(Swx*r2 - r1*Swy) );
            p2 = invdet * ( Sw*(Swxx*r2 - r1*Swxy) - Swx*(Swx*r2 - r1*Swy) + r0*(Swx*Swxy - Swxx*Swy) );
        };
        solve3(bx0,bx1,bx2,a,b,c);
        solve3(by0,by1,by2,d,e,f);
    }
    out6[0]=(float)a;out6[1]=(float)b;out6[2]=(float)c;out6[3]=(float)d;out6[4]=(float)e;out6[5]=(float)f;
    uint64_t dissident = 0; const uint64_t total = (uint64_t)mvw * mvh;
    for (uint32_t gy = 0; gy < mvh; ++gy) {
        for (uint32_t gx = 0; gx < mvw; ++gx) {
            const size_t idx = ((size_t)gy * mvw + gx) * 2u;
            const float u = half_to_float(h[idx + 0]);
            const float v = half_to_float(h[idx + 1]);
            const double rx = u - (a + b*gx + c*gy);
            const double ry = v - (d + e*gx + f*gy);
            const double r  = std::sqrt(rx*rx + ry*ry);
            if (r > 4.0) ++dissident;
            if (dis_out) { int q = (int)(16.0*r + 0.5); if (q > 255) q = 255;
                if (sad_raw && q > 0) {
                    const float sz = half_to_float(((const uint16_t*)sad_raw)[((size_t)gy*mvw + gx)*2u + 1u]);
                    if (sz < kChangeGateSadZ) q = 0;
                }
                dis_out[(size_t)gy*mvw + gx] = (uint8_t)q; }
        }
    }
    return total ? 100.0 * (double)dissident / (double)total : 0.0;
}

// The batch fp16->fp32 primitive under test is hal::f16_to_f32_batch (framework/hal/Simd.hpp) —
// runtime-dispatched F16C/scalar. The bench calls it directly so the comparison measures the real
// primitive, not a bench-local copy.

// gme_fit over PRE-DECODED floats: identical math to gme_fit_affine, but reads uv[]/szf[] instead
// of decoding fp16 per block per pass. The "decode once, reuse 4x" design. uv = interleaved [u,v]
// per block (nblk*2 floats); szf = sad_zero per block (nblk floats). Produces byte-identical out6
// + dis to the scalar path (same float values → same double accumulation order).
static double gme_fit_affine_pre(const float* uv, const float* szf, uint32_t mvw, uint32_t mvh,
                                 float out6[6], uint8_t* dis_out, bool sub2){
    const int step = sub2 ? 2 : 1;
    double a=0,b=0,c=0,d=0,e=0,f=0;
    for (int iter = 0; iter < 3; ++iter) {
        double Sw=0,Swx=0,Swy=0,Swxx=0,Swxy=0,Swyy=0, bx0=0,bx1=0,bx2=0, by0=0,by1=0,by2=0;
        for (uint32_t gy = 0; gy < mvh; gy += (uint32_t)step)
            for (uint32_t gx = 0; gx < mvw; gx += (uint32_t)step) {
                const size_t idx = ((size_t)gy * mvw + gx) * 2u;
                const float u = uv[idx + 0], v = uv[idx + 1];
                double w = 1.0;
                if (iter > 0) {
                    const double rx = u - (a + b*gx + c*gy), ry = v - (d + e*gx + f*gy);
                    const double r = std::sqrt(rx*rx + ry*ry), rr = r / 2.0; w = 1.0 / (1.0 + rr*rr);
                }
                const double gxx=(double)gx, gyy=(double)gy;
                Sw+=w; Swx+=w*gxx; Swy+=w*gyy; Swxx+=w*gxx*gxx; Swxy+=w*gxx*gyy; Swyy+=w*gyy*gyy;
                bx0+=w*u; bx1+=w*u*gxx; bx2+=w*u*gyy; by0+=w*v; by1+=w*v*gxx; by2+=w*v*gyy;
            }
        const double det = Sw*(Swxx*Swyy - Swxy*Swxy) - Swx*(Swx*Swyy - Swxy*Swy) + Swy*(Swx*Swxy - Swxx*Swy);
        if (std::fabs(det) < 1e-9) { out6[0]=(float)a;out6[1]=(float)b;out6[2]=(float)c;out6[3]=(float)d;out6[4]=(float)e;out6[5]=(float)f; break; }
        const double invdet = 1.0 / det;
        auto solve3 = [&](double r0,double r1,double r2,double& p0,double& p1,double& p2){
            p0 = invdet * ( r0*(Swxx*Swyy - Swxy*Swxy) - Swx*(r1*Swyy - Swxy*r2) + Swy*(r1*Swxy - Swxx*r2) );
            p1 = invdet * ( Sw*(r1*Swyy - Swxy*r2) - r0*(Swx*Swyy - Swxy*Swy) + Swy*(Swx*r2 - r1*Swy) );
            p2 = invdet * ( Sw*(Swxx*r2 - r1*Swxy) - Swx*(Swx*r2 - r1*Swy) + r0*(Swx*Swxy - Swxx*Swy) );
        };
        solve3(bx0,bx1,bx2,a,b,c); solve3(by0,by1,by2,d,e,f);
    }
    out6[0]=(float)a;out6[1]=(float)b;out6[2]=(float)c;out6[3]=(float)d;out6[4]=(float)e;out6[5]=(float)f;
    uint64_t dissident = 0; const uint64_t total = (uint64_t)mvw * mvh;
    for (uint32_t gy = 0; gy < mvh; ++gy) for (uint32_t gx = 0; gx < mvw; ++gx) {
        const size_t idx = ((size_t)gy * mvw + gx) * 2u;
        const float u = uv[idx + 0], v = uv[idx + 1];
        const double rx = u - (a + b*gx + c*gy), ry = v - (d + e*gx + f*gy);
        const double r = std::sqrt(rx*rx + ry*ry);
        if (r > 4.0) ++dissident;
        if (dis_out) { int q = (int)(16.0*r + 0.5); if (q > 255) q = 255;
            if (q > 0) { const float sz = szf[(size_t)gy*mvw + gx]; if (sz < kChangeGateSadZ) q = 0; }
            dis_out[(size_t)gy*mvw + gx] = (uint8_t)q; }
    }
    return total ? 100.0 * (double)dissident / (double)total : 0.0;
}

// ════════════════════════════════════════════════════════════════════════════
// GPU-fp32 SIMULATION — the precision gate for offloading gme_fit to a GPU.
//
// A GPU path accumulates in fp32 with a PARALLEL reduction order (workgroup-partials-then-combine),
// not the CPU's fp64 sequential running sum, so it is not byte-identical (and fp64 is impractical on
// Pascal-class GPUs at a 1/32 rate). This models the fp32 + reorder drift against the fp64 reference
// gme_fit_affine, so the precision cost of an offload is known before committing to it.
//
// What is simulated faithfully:
//   • ALL accumulation in float (the 12 reduction sums Sw..by2, the residual/weight math, the
//     3×3 Cramer solve, the dissidence residual). NOT a single double anywhere.
//   • A TWO-LEVEL reduction: blocks are walked in the same row-major grid order the CPU uses, but
//     partitioned into fixed CHUNKS ("workgroups" of `chunk` blocks). Each chunk sums its own 12
//     partials in fp32 (sequential WITHIN the chunk — a workgroup's local reduction), then the
//     chunk-partials are combined in fp32 (the cross-workgroup combine). This is the GPU's
//     partial-then-combine tree, swapping the fp64 one-long-running-sum for a fp32 two-level sum.
//   • The IRLS reweight (Geman-McClure w = 1/(1+(r/2)²)) and the Cramer 3×3 in fp32.
//   • The dissidence pass: same r>4 count + min(255,round(16·r)) quantization + change-gate, fp32.
//
// The reference gme_fit_affine already casts its fp64 result to float for out6, so the comparison is
// (fp32-rounded fp64 model) vs (fp32-native model) — exactly what an adopted GPU path produces.

// Reduce 12 fp32 partials over a [first,last) block range walked in row-major grid order, summing
// SEQUENTIALLY within the range (a single workgroup's local reduction). `iter`>0 applies the IRLS
// reweight using the current fp32 model (a..f). Returns the 12 partials into acc[12].
struct GmeAcc12 { float v[12]; };  // Sw,Swx,Swy,Swxx,Swxy,Swyy, bx0,bx1,bx2, by0,by1,by2
static inline void gme_chunk_partials_fp32(const float* uv, uint32_t mvw, int step,
                                           uint32_t first_blk, uint32_t last_blk,
                                           int iter, float a,float b,float c,float d,float e,float f,
                                           bool kahan, GmeAcc12& out){
    float s[12]   = {0,0,0,0,0,0, 0,0,0,0,0,0};
    float comp[12]= {0,0,0,0,0,0, 0,0,0,0,0,0};   // Kahan compensation per accumulator
    auto add = [&](int k, float x){
        if (kahan){ float y = x - comp[k]; float t = s[k] + y; comp[k] = (t - s[k]) - y; s[k] = t; }
        else      { s[k] += x; }
    };
    // step>1 (sub2) skips odd rows/cols; the linear block index space here is the FULL grid index
    // (gy*mvw+gx), and a block contributes only if both gx,gy are on the step lattice.
    for (uint32_t blk = first_blk; blk < last_blk; ++blk){
        const uint32_t gx = blk % mvw, gy = blk / mvw;
        if (step > 1 && ((gx % (uint32_t)step) || (gy % (uint32_t)step))) continue;
        const size_t idx = (size_t)blk * 2u;
        const float u = uv[idx + 0], v = uv[idx + 1];
        const float gxx = (float)gx, gyy = (float)gy;
        float w = 1.0f;
        if (iter > 0){
            const float rx = u - (a + b*gxx + c*gyy);
            const float ry = v - (d + e*gxx + f*gyy);
            const float r  = std::sqrt(rx*rx + ry*ry);
            const float rr = r * 0.5f;
            w = 1.0f / (1.0f + rr*rr);
        }
        add(0,  w);          add(1,  w*gxx);      add(2,  w*gyy);
        add(3,  w*gxx*gxx);  add(4,  w*gxx*gyy);  add(5,  w*gyy*gyy);
        add(6,  w*u);        add(7,  w*u*gxx);    add(8,  w*u*gyy);
        add(9,  w*v);        add(10, w*v*gxx);    add(11, w*v*gyy);
    }
    for (int k=0;k<12;++k) out.v[k] = s[k];
}

// fp32 gme_fit with a GPU-style two-level (chunked partial → combine) reduction. `chunk` = blocks
// per workgroup. `kahan`=true uses compensated summation in BOTH the within-chunk accumulate and the
// cross-chunk combine. Produces out6 (fp32) + dis-mask + dis% identically structured to the fp64 ref.
static float gme_fit_affine_fp32sim(const float* uv, const float* szf, uint32_t mvw, uint32_t mvh,
                                    float out6[6], uint8_t* dis_out, bool sub2,
                                    uint32_t chunk, bool kahan){
    const int step = sub2 ? 2 : 1;
    const uint32_t nblk = mvw * mvh;
    const uint32_t nchunks = (nblk + chunk - 1) / chunk;
    float a=0,b=0,c=0,d=0,e=0,f=0;
    std::vector<GmeAcc12> parts(nchunks);
    for (int iter = 0; iter < 3; ++iter){
        // Level 1: each chunk reduces its own range sequentially in fp32.
        for (uint32_t ci = 0; ci < nchunks; ++ci){
            const uint32_t first = ci * chunk;
            const uint32_t last  = std::min(first + chunk, nblk);
            gme_chunk_partials_fp32(uv, mvw, step, first, last, iter, a,b,c,d,e,f, kahan, parts[ci]);
        }
        // Level 2: combine chunk-partials in fp32 (the cross-workgroup combine).
        float S[12] = {0,0,0,0,0,0, 0,0,0,0,0,0};
        float comp[12] = {0,0,0,0,0,0, 0,0,0,0,0,0};
        for (uint32_t ci = 0; ci < nchunks; ++ci)
            for (int k=0;k<12;++k){
                if (kahan){ float y = parts[ci].v[k] - comp[k]; float t = S[k] + y; comp[k] = (t - S[k]) - y; S[k] = t; }
                else      { S[k] += parts[ci].v[k]; }
            }
        const float Sw=S[0],Swx=S[1],Swy=S[2],Swxx=S[3],Swxy=S[4],Swyy=S[5];
        const float bx0=S[6],bx1=S[7],bx2=S[8], by0=S[9],by1=S[10],by2=S[11];
        const float det = Sw*(Swxx*Swyy - Swxy*Swxy) - Swx*(Swx*Swyy - Swxy*Swy) + Swy*(Swx*Swxy - Swxx*Swy);
        if (std::fabs(det) < 1e-9f){ out6[0]=a;out6[1]=b;out6[2]=c;out6[3]=d;out6[4]=e;out6[5]=f; break; }
        const float invdet = 1.0f / det;
        auto solve3 = [&](float r0,float r1,float r2,float& p0,float& p1,float& p2){
            p0 = invdet * ( r0*(Swxx*Swyy - Swxy*Swxy) - Swx*(r1*Swyy - Swxy*r2) + Swy*(r1*Swxy - Swxx*r2) );
            p1 = invdet * ( Sw*(r1*Swyy - Swxy*r2) - r0*(Swx*Swyy - Swxy*Swy) + Swy*(Swx*r2 - r1*Swy) );
            p2 = invdet * ( Sw*(Swxx*r2 - r1*Swxy) - Swx*(Swx*r2 - r1*Swy) + r0*(Swx*Swxy - Swxx*Swy) );
        };
        solve3(bx0,bx1,bx2,a,b,c); solve3(by0,by1,by2,d,e,f);
    }
    out6[0]=a;out6[1]=b;out6[2]=c;out6[3]=d;out6[4]=e;out6[5]=f;
    uint64_t dissident = 0; const uint64_t total = (uint64_t)mvw * mvh;
    for (uint32_t gy = 0; gy < mvh; ++gy) for (uint32_t gx = 0; gx < mvw; ++gx){
        const size_t idx = ((size_t)gy * mvw + gx) * 2u;
        const float u = uv[idx + 0], v = uv[idx + 1];
        const float rx = u - (a + b*gx + c*gy), ry = v - (d + e*gx + f*gy);
        const float r = std::sqrt(rx*rx + ry*ry);
        if (r > 4.0f) ++dissident;
        if (dis_out){ int q = (int)(16.0f*r + 0.5f); if (q > 255) q = 255;
            if (q > 0){ const float sz = szf[(size_t)gy*mvw + gx]; if (sz < kChangeGateSadZ) q = 0; }
            dis_out[(size_t)gy*mvw + gx] = (uint8_t)q; }
    }
    return total ? 100.0f * (float)dissident / (float)total : 0.0f;
}

// ── decode-only microbench: how much of the cost is JUST the fp16 decode ──────
static double decode_only_sum(const uint16_t* h, uint32_t mvw, uint32_t mvh, int passes){
    double acc = 0;
    for (int p = 0; p < passes; ++p)
        for (uint32_t i = 0; i < mvw*mvh*2u; ++i) acc += half_to_float(h[i]);
    return acc;
}

template<class F> static double bench_ms(int iters, F&& fn){
    using clk = std::chrono::steady_clock;
    // warmup
    for (int i = 0; i < 3; ++i) fn();
    std::vector<double> samples; samples.reserve(iters);
    for (int i = 0; i < iters; ++i){
        const auto t0 = clk::now(); fn(); const auto t1 = clk::now();
        samples.push_back(std::chrono::duration<double,std::milli>(t1-t0).count());
    }
    std::sort(samples.begin(), samples.end());
    return samples[samples.size()/2];   // median
}

// Build the synthetic field (same ground-truth affine + 6 mover rectangles + light noise) for an
// ARBITRARY grid size. Production is 240×135; flow-scale 2/4 halve/quarter each axis. The mover
// rectangles + the per-block model are scaled PROPORTIONALLY so the field is structurally the same
// outlier pattern at every grid size (the same fraction of the grid is occluded by movers). The
// ground-truth coefficients b,c,e,f are per-BLOCK gradients, so they are scaled by the axis ratio
// (a coarser grid spans the same scene with fewer blocks ⇒ steeper per-block gradient). The decoded
// float grid (uv[], szf[]) is returned so the drift harness compares fp32sim vs fp64 on identical
// inputs — both consume the SAME fp16-roundtripped floats the shipping path sees.
static void build_field(uint32_t mvw, uint32_t mvh,
                        std::vector<uint16_t>& mv, std::vector<uint16_t>& sad,
                        std::vector<float>& uv, std::vector<float>& szf){
    const size_t nblk = (size_t)mvw*mvh;
    mv.assign(nblk*2,0); sad.assign(nblk*2,0); uv.assign(nblk*2,0); szf.assign(nblk,0);
    uint32_t rng = 0x9e3779b9u;
    auto nextf = [&](float lo, float hi){ rng = rng*1664525u + 1013904223u; return lo + (hi-lo)*((rng>>8)*(1.0f/16777216.0f)); };
    // Ground-truth global model: b,c,e,f are per-block gradients → scale to the grid so the END-to-END
    // displacement across the frame is invariant to grid resolution (240/mvw, 135/mvh ratios).
    const float sx = 240.0f / (float)mvw, sy = 135.0f / (float)mvh;
    const float A=2.3f, B=0.012f*sx, C=-0.004f*sy, D=1.1f, E=0.003f*sx, F=0.015f*sy;
    struct Mv{float fx0,fy0,fx1,fy1; float u,v;};   // mover rects as FRACTIONS of the grid
    const Mv movers[6] = {
        {20.f/240,15.f/135,55.f/240,45.f/135,-7.0f,3.0f}, {120.f/240,30.f/135,160.f/240,70.f/135,9.0f,-4.0f},
        {60.f/240,90.f/135,95.f/240,120.f/135,-2.0f,8.0f}, {180.f/240,80.f/135,215.f/240,115.f/135,5.5f,5.5f},
        {95.f/240,55.f/135,115.f/240,75.f/135,-9.0f,-9.0f}, {200.f/240,10.f/135,230.f/240,35.f/135,3.0f,-7.0f}};
    for (uint32_t gy=0; gy<mvh; ++gy) for (uint32_t gx=0; gx<mvw; ++gx){
        float u = A + B*gx + C*gy + nextf(-0.15f,0.15f);
        float v = D + E*gx + F*gy + nextf(-0.15f,0.15f);
        float sz = 12.0f + nextf(-3.0f,3.0f);
        for (auto& m : movers){
            const int x0=(int)(m.fx0*mvw), x1=(int)(m.fx1*mvw), y0=(int)(m.fy0*mvh), y1=(int)(m.fy1*mvh);
            if ((int)gx>=x0&&(int)gx<x1&&(int)gy>=y0&&(int)gy<y1){ u=m.u; v=m.v; sz=40.0f; }
        }
        const size_t i = ((size_t)gy*mvw+gx)*2u;
        mv[i]=float_to_half(u); mv[i+1]=float_to_half(v);
        sad[i]=float_to_half(2.0f); sad[i+1]=float_to_half(sz);
    }
    // Decode the fp16 grid into float scratch ONCE (the shipping decode-once path); fp32sim + the
    // fp64 ref both read these floats so the only variable is the accumulation precision/order.
    for (size_t k=0;k<nblk;++k){ uv[k*2]=half_to_float(mv[k*2]); uv[k*2+1]=half_to_float(mv[k*2+1]); szf[k]=half_to_float(sad[k*2+1]); }
}

// One drift row: fp32sim (a given chunk size + kahan flag) vs the fp64 reference, at one grid size.
static void measure_drift(const char* label, uint32_t mvw, uint32_t mvh, uint32_t chunk, bool kahan){
    std::vector<uint16_t> mv, sad; std::vector<float> uv, szf;
    build_field(mvw, mvh, mv, sad, uv, szf);
    const size_t nblk = (size_t)mvw*mvh;
    float o6_ref[6], o6_f32[6];
    std::vector<uint8_t> dis_ref(nblk), dis_f32(nblk);
    // fp64 reference — read the SAME decoded floats (gme_fit_affine_pre is the fp64 decode-once path,
    // byte-identical to the scalar gme_fit_affine). This isolates precision/order from decode.
    const double pct_ref = gme_fit_affine_pre(uv.data(), szf.data(), mvw, mvh, o6_ref, dis_ref.data(), false);
    const float  pct_f32 = gme_fit_affine_fp32sim(uv.data(), szf.data(), mvw, mvh, o6_f32, dis_f32.data(), false, chunk, kahan);
    // Model drift: per-component max abs-diff + max rel-diff.
    double max_abs = 0.0, max_rel = 0.0; int rel_idx = 0;
    for (int i=0;i<6;++i){ const double ad = std::fabs((double)o6_f32[i] - (double)o6_ref[i]);
        const double rd = std::fabs((double)o6_ref[i]) > 1e-12 ? ad/std::fabs((double)o6_ref[i]) : ad;
        if (ad>max_abs) max_abs=ad;
        if (rd>max_rel){ max_rel=rd; rel_idx=i; } }
    // dis-mask drift: bytes that differ + max byte-delta.
    size_t flips = 0; int max_byte_delta = 0;
    for (size_t k=0;k<nblk;++k){ const int dd = (int)dis_f32[k] - (int)dis_ref[k];
        if (dd) ++flips;
        if (std::abs(dd) > max_byte_delta) max_byte_delta = std::abs(dd); }
    std::printf("%-22s grid %3ux%-3u chunk %-4u kahan=%d | maxAbs %.3e  maxRel %.3e (p%d) | dis%% %.4f vs %.4f (|d|=%.4f) | mask flips %zu/%zu (%.3f%%) maxByteΔ %d\n",
                label, mvw, mvh, chunk, (int)kahan, max_abs, max_rel, rel_idx,
                (double)pct_f32, pct_ref, std::fabs((double)pct_f32 - pct_ref),
                flips, nblk, 100.0*(double)flips/(double)nblk, max_byte_delta);
}

int main(){
    const uint32_t mvw = 240, mvh = 135;   // 1920/8 x 1080/8
    const size_t   nblk = (size_t)mvw*mvh;
    std::vector<uint16_t> mv(nblk*2), sad(nblk*2);
    std::vector<uint8_t>  dis(nblk);

    // Synthetic field: a global affine camera motion + ~6 mover rectangles (outliers) + light noise.
    // Deterministic LCG (no Date/rand) so the bench is reproducible.
    uint32_t rng = 0x9e3779b9u;
    auto nextf = [&](float lo, float hi){ rng = rng*1664525u + 1013904223u; return lo + (hi-lo)*((rng>>8)*(1.0f/16777216.0f)); };
    const float A=2.3f,B=0.012f,C=-0.004f, D=1.1f,E=0.003f,F=0.015f;   // ground-truth global model
    struct Mv{int x0,y0,x1,y1; float u,v;};
    Mv movers[6] = {{20,15,55,45,-7.0f,3.0f},{120,30,160,70,9.0f,-4.0f},{60,90,95,120,-2.0f,8.0f},
                    {180,80,215,115,5.5f,5.5f},{95,55,115,75,-9.0f,-9.0f},{200,10,230,35,3.0f,-7.0f}};
    for (uint32_t gy=0; gy<mvh; ++gy) for (uint32_t gx=0; gx<mvw; ++gx){
        float u = A + B*gx + C*gy + nextf(-0.15f,0.15f);
        float v = D + E*gx + F*gy + nextf(-0.15f,0.15f);
        float sz = 12.0f + nextf(-3.0f,3.0f);   // changed content (passes the change gate)
        for (auto& m : movers) if ((int)gx>=m.x0&&(int)gx<m.x1&&(int)gy>=m.y0&&(int)gy<m.y1){ u=m.u; v=m.v; sz=40.0f; }
        const size_t i = ((size_t)gy*mvw+gx)*2u;
        mv[i]=float_to_half(u); mv[i+1]=float_to_half(v);
        sad[i]=float_to_half(2.0f); sad[i+1]=float_to_half(sz);
    }

    const int iters = 400;
    std::vector<float>   uv(nblk*2), sad_f(nblk*2), szf(nblk);
    std::vector<uint8_t> dis_ref(nblk), dis_simd(nblk);
    float out6_ref[6], out6_simd[6];

    // Scalar reference.
    const double pct_ref = gme_fit_affine(mv.data(), sad.data(), mvw, mvh, out6_ref, dis_ref.data(), false);
    const double t_full  = bench_ms(iters, [&]{ float o[6]; gme_fit_affine(mv.data(), sad.data(), mvw, mvh, o, dis.data(), false); });
    const double t_sub2  = bench_ms(iters, [&]{ float o[6]; gme_fit_affine(mv.data(), sad.data(), mvw, mvh, o, dis.data(), true ); });
    const double t_dec4  = bench_ms(iters, [&]{ volatile double s = decode_only_sum(mv.data(), mvw, mvh, 4); (void)s; });

    std::printf("=== gme_fit_bench (grid %ux%u = %zu blocks, median of %d iters) ===\n", mvw, mvh, nblk, iters);
    std::printf("model fit: a=%.4f b=%.5f c=%.5f d=%.4f e=%.5f f=%.5f  dis=%.1f%%\n",
                out6_ref[0],out6_ref[1],out6_ref[2],out6_ref[3],out6_ref[4],out6_ref[5], pct_ref);
    std::printf("[SCALAR] gme_fit_affine sub2=false : %.3f ms/call\n", t_full);
    std::printf("[SCALAR] gme_fit_affine sub2=true  : %.3f ms/call\n", t_sub2);
    std::printf("[SCALAR] decode-only (4 passes)    : %.3f ms  (%.0f%% of sub2=false)\n", t_dec4, 100.0*t_dec4/t_full);

    // hal::f16_to_f32_batch decode-once parity check: every value must bit-match scalar half_to_float.
    namespace hal = phyriad::hal;
    hal::f16_to_f32_batch(mv.data(), uv.data(), nblk*2);
    size_t decode_mismatch = 0;
    for (size_t i = 0; i < nblk*2; ++i){ float s = half_to_float(mv[i]);
        uint32_t a; uint32_t b2; std::memcpy(&a,&uv[i],4); std::memcpy(&b2,&s,4); if (a!=b2) ++decode_mismatch; }

    // The decode-once path: hal-decode mv + sad once, stride out sad_zero, then the reduction over floats.
    const double pct_simd = ([&]{
        hal::f16_to_f32_batch(mv.data(),  uv.data(),    nblk*2);
        hal::f16_to_f32_batch(sad.data(), sad_f.data(), nblk*2);
        for (size_t k=0;k<nblk;++k) szf[k]=sad_f[k*2+1];
        return gme_fit_affine_pre(uv.data(), szf.data(), mvw, mvh, out6_simd, dis_simd.data(), false);
    })();

    int o6_mismatch = 0; for (int i=0;i<6;++i){ uint32_t a,b2; std::memcpy(&a,&out6_ref[i],4); std::memcpy(&b2,&out6_simd[i],4); if(a!=b2) ++o6_mismatch; }
    const int dis_mismatch = std::memcmp(dis_ref.data(), dis_simd.data(), nblk) == 0 ? 0 : 1;

    const double t_simd = bench_ms(iters, [&]{
        hal::f16_to_f32_batch(mv.data(),  uv.data(),    nblk*2);
        hal::f16_to_f32_batch(sad.data(), sad_f.data(), nblk*2);
        for (size_t k=0;k<nblk;++k) szf[k]=sad_f[k*2+1];
        float o[6]; gme_fit_affine_pre(uv.data(), szf.data(), mvw, mvh, o, dis.data(), false);
    });

    std::printf("--- hal::f16_to_f32_batch (decode-once + reduce), caps.f16c=%d ---\n", (int)hal::simd_caps().f16c);
    std::printf("[HAL]    decode parity (must=0)    : %zu mismatches / %zu values\n", decode_mismatch, nblk*2);
    std::printf("[HAL]    out6 parity   (must=0)    : %d/6   dis-mask parity (must=0): %d\n", o6_mismatch, dis_mismatch);
    std::printf("[HAL]    pct match     : scalar %.3f%% vs hal %.3f%%\n", pct_ref, pct_simd);
    std::printf("[HAL]    gme_fit (decode-once) sub2=false : %.3f ms/call\n", t_simd);
    std::printf("[HAL]    SPEEDUP vs scalar : %.2fx  (%.3f -> %.3f ms)\n", t_full/t_simd, t_full, t_simd);

    // ════════════════════════════════════════════════════════════════════════
    // GPU-fp32 PRECISION GATE: drift of the fp32 two-level reduction vs the fp64 reference.
    // Three grid sizes (production + flow-scale 2 + flow-scale 4) × two chunk sizes (64, 256).
    std::printf("\n=== GPU-fp32 PRECISION GATE (fp32sim vs fp64 ref) — model rel-diff / dis%% / dis-mask flips ===\n");
    const struct { uint32_t w,h; const char* name; } grids[3] = {
        {240,135,"production"}, {120,68,"flow-scale 2"}, {60,34,"flow-scale 4"} };
    const uint32_t chunks[2] = {64, 256};
    std::printf("--- plain fp32 ---\n");
    for (auto& g : grids) for (uint32_t ch : chunks)
        measure_drift("fp32-plain", g.w, g.h, ch, /*kahan=*/false);
    std::printf("--- Kahan (compensated) fp32 ---\n");
    for (auto& g : grids) for (uint32_t ch : chunks)
        measure_drift("fp32-kahan", g.w, g.h, ch, /*kahan=*/true);
    return 0;
}
// Made with my soul - Swately <3
