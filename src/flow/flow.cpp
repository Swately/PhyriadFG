// PhyriadFG flow layer (STEP 4a — PURE RELOCATION from src/core/main.cpp; no logic change).
// Bodies of the FLOW factories declared in flow/flow.hpp. The flow.cpp-internal helpers
// (nvofa_img / gme_make_pipe / gme_buf_barrier) stay `static`; the main-called factories get
// external linkage. nvofa_run is a TEMPLATE -> its definition lives in the header.
#include "flow/flow.hpp"
#include "core/ra_simd.hpp"        // ra::decode_f16 — the F16C batch decode for gme_fit_affine
#include "instrument/instrument.hpp"  // STEP 5.3: dump_bmp (the F-thread objdump grids)
#include <cstdint>
#include <cstdio>                   // std::printf (the nvofa auto-disable diagnostics)
#include <cstring>                  // STEP 5.3: std::memcpy (run_flow body)
#include <cmath>                    // std::sqrt / std::fabs (gme_fit_affine) + std::lround/std::ceil (run_flow)
#include <algorithm>                // STEP 5.3: std::max / std::min / std::fill (run_flow body)
#include <vector>

// ── gme_fit_affine (+ kChangeGateSadZ + the decode-once thread-local scratch) ──
// Fit mv(gx,gy) = (a + b·gx + c·gy, d + e·gx + f·gy) over the mv-grid by least squares with 2 IRLS
// reweight iterations (weight = 1/(1+(r/2px)²), r = residual length) to reject the moving objects as
// outliers. COORDINATE CONVENTION (must match the shader EXACTLY): gx,gy are GRID texel indices in
// [0,mvw)×[0,mvh). The shader evaluates at gx_s = uv.x·mvw − 0.5, gy_s = uv.y·mvh − 0.5 (the bilinear
// texel coordinate), which equals the integer (gx,gy) at every texel CENTRE uv=(gx+0.5)/mvw — so the
// model fitted over integer grid indices is evaluated at the same lattice the MV texture samples on.
// out6 receives {a,b,c,d,e,f}; returns the % of blocks with r>4px (the dissidence stat). The mask
// (uint8 = min(255, round(16·r))) is written into dis_out (mvw·mvh bytes) when non-null. sub2=true
// sub-samples the grid every 2nd block in BOTH axes for the fit (keeps the cost <2ms at large grids);
// the mask + stat ALWAYS cover all blocks. The two normal equations (x-model, y-model) are 3×3 each
// and share the SAME design weights, solved by Cramer's rule (a fixed 3×3 — no library).
// STAGE-70 (supervisor-built, objdump-diagnosed): the CHANGE GATE threshold. Units = the pillar's
// per-8×8-block SUM |A−B| (the sad_zero/stasis scale; 56's calibration: ≤8.0 ≈ visually identical).
// A block whose content did not change (sad_zero < this) cannot be dissident no matter what its MV
// says — the pyramid matcher paints a HALO of object MVs onto flat background tiles around/behind
// every mover (coarse-level motion flat refinement cannot undo; SEEN FIRST-HAND in the objdump
// grids: mask blobs 2-4× the object size — the root of the oldest trail/chunk artifact, upstream
// of every mechanism built on the mask). 0.5 = the stasis default scale.
static constexpr float kChangeGateSadZ = 0.5f;
// STAGE-86b: the DECODE-ONCE scratch. gme_fit_affine used to decode each fp16 MV block ~4× (3 IRLS
// passes + the dissidence pass) and each fp16 sad_zero once per dissident block. The --fsub field
// measure (HSR120 §6.7) showed this CPU tail dominates the per-pair serial cost; the gme_fit_bench
// proved that decoding the grid ONCE into float scratch (via the hal F16C batch) then reducing over
// the floats is byte-identical (same float values → same double accumulation order → identical out6
// + dis mask) and 1.48× (F16C) / 1.19× (scalar-dedup) faster. The scratch is F-THREAD-LOCAL and
// grown ONCE to the grid size (zero-alloc steady state, dogma D-2): gme_fit_affine runs only on the
// F thread (fwd @ have_prev_f, bwd @ do_bwd), so thread_local is the natural single-owner home and
// keeps the function reentrant-clean without passing buffers through the call sites. uv = decoded MV
// interleaved [u,v] per block (mvw·mvh·2 floats); szf = strided sad_zero per block (mvw·mvh floats),
// filled only when the change gate is active (sad_raw != null).
static thread_local std::vector<float> g_gme_uv;    // decoded MV grid, interleaved u,v
static thread_local std::vector<float> g_gme_sadf;  // decoded sad grid (full RG), interleaved
static thread_local std::vector<float> g_gme_szf;   // strided sad_zero per block
double gme_fit_affine(const void* mv_raw, const void* sad_raw, uint32_t mvw, uint32_t mvh,
                             float out6[6], uint8_t* dis_out, bool sub2, int irls_iters){   // STEP 4a: default arg (=3) lives on the flow.hpp declaration only
    const int step = sub2 ? 2 : 1;
    const size_t nblk = (size_t)mvw * mvh;
    // ── DECODE-ONCE (STAGE-86b): mv grid + (gated) sad grid into thread-local float scratch via the
    // hal F16C batch (ra::decode_f16). The IRLS passes + dissidence pass below read these floats —
    // byte-identical to the old per-block half_to_float (the hal batch is bit-exact to the scalar
    // decode, verified in gme_fit_bench). Buffers grow ONCE then are reused every pair.
    if (g_gme_uv.size() < nblk * 2u) g_gme_uv.resize(nblk * 2u);
    ra::decode_f16((const uint16_t*)mv_raw, g_gme_uv.data(), nblk * 2u);
    const float* uv = g_gme_uv.data();
    const float* szf = nullptr;
    if (sad_raw) {
        if (g_gme_sadf.size() < nblk * 2u) g_gme_sadf.resize(nblk * 2u);
        if (g_gme_szf.size()  < nblk)      g_gme_szf.resize(nblk);
        ra::decode_f16((const uint16_t*)sad_raw, g_gme_sadf.data(), nblk * 2u);
        for (size_t k = 0; k < nblk; ++k) g_gme_szf[k] = g_gme_sadf[k*2u + 1u];   // .g = sad_zero
        szf = g_gme_szf.data();
    }
    // IRLS state: start with all-equal weights (= ordinary least squares), then irls_iters-1 reweights.
    // STAGE-87: irls_iters defaults to 3 (OLS + 2 reweights = byte-identical to STAGE-86b); --gme-irls2
    // passes 2 (OLS + 1 reweight) to shave the marginal iter-2 reweight off the dominant CPU axis.
    double a=0,b=0,c=0,d=0,e=0,f=0;
    for (int iter = 0; iter < irls_iters; ++iter) {          // iter 0 = OLS, iters 1..n = IRLS reweight
        // Normal-equation accumulators for the shared 3×3 (Σw, Σw·gx, Σw·gy, Σw·gx², Σw·gx·gy, Σw·gy²)
        double Sw=0,Swx=0,Swy=0,Swxx=0,Swxy=0,Swyy=0;
        double bx0=0,bx1=0,bx2=0;                   // RHS for the x-model (Σw·u, Σw·u·gx, Σw·u·gy)
        double by0=0,by1=0,by2=0;                   // RHS for the y-model (Σw·v, …)
        for (uint32_t gy = 0; gy < mvh; gy += (uint32_t)step) {
            for (uint32_t gx = 0; gx < mvw; gx += (uint32_t)step) {
                const size_t idx = ((size_t)gy * mvw + gx) * 2u;
                const float u = uv[idx + 0];   // mv_x (px), pre-decoded
                const float v = uv[idx + 1];   // mv_y (px), pre-decoded
                double w = 1.0;
                if (iter > 0) {
                    const double rx = u - (a + b*gx + c*gy);
                    const double ry = v - (d + e*gx + f*gy);
                    const double r  = std::sqrt(rx*rx + ry*ry);
                    const double rr = r / 2.0;                // 2px scale
                    w = 1.0 / (1.0 + rr*rr);                  // Geman-McClure-style reweight
                }
                const double gxx=(double)gx, gyy=(double)gy;
                Sw+=w; Swx+=w*gxx; Swy+=w*gyy; Swxx+=w*gxx*gxx; Swxy+=w*gxx*gyy; Swyy+=w*gyy*gyy;
                bx0+=w*u; bx1+=w*u*gxx; bx2+=w*u*gyy;
                by0+=w*v; by1+=w*v*gxx; by2+=w*v*gyy;
            }
        }
        // Solve the shared 3×3 M·[p0,p1,p2]ᵀ = rhs (M symmetric) by Cramer's rule.
        // M = [[Sw,Swx,Swy],[Swx,Swxx,Swxy],[Swy,Swxy,Swyy]]
        const double det = Sw*(Swxx*Swyy - Swxy*Swxy)
                         - Swx*(Swx*Swyy - Swxy*Swy)
                         + Swy*(Swx*Swxy - Swxx*Swy);
        if (std::fabs(det) < 1e-9) { out6[0]=(float)a;out6[1]=(float)b;out6[2]=(float)c;out6[3]=(float)d;out6[4]=(float)e;out6[5]=(float)f; break; }
        const double invdet = 1.0 / det;
        // Cofactor solve for a generic RHS (r0,r1,r2) → returns the three params.
        auto solve3 = [&](double r0,double r1,double r2,double& p0,double& p1,double& p2){
            p0 = invdet * ( r0*(Swxx*Swyy - Swxy*Swxy) - Swx*(r1*Swyy - Swxy*r2) + Swy*(r1*Swxy - Swxx*r2) );
            p1 = invdet * ( Sw*(r1*Swyy - Swxy*r2) - r0*(Swx*Swyy - Swxy*Swy) + Swy*(Swx*r2 - r1*Swy) );
            p2 = invdet * ( Sw*(Swxx*r2 - r1*Swxy) - Swx*(Swx*r2 - r1*Swy) + r0*(Swx*Swxy - Swxx*Swy) );
        };
        solve3(bx0,bx1,bx2,a,b,c);
        solve3(by0,by1,by2,d,e,f);
    }
    out6[0]=(float)a;out6[1]=(float)b;out6[2]=(float)c;out6[3]=(float)d;out6[4]=(float)e;out6[5]=(float)f;
    // Dissidence mask + stat over ALL blocks (the mask is for STAGE-53; the stat is the dis:NN% print).
    uint64_t dissident = 0; const uint64_t total = (uint64_t)mvw * mvh;
    for (uint32_t gy = 0; gy < mvh; ++gy) {
        for (uint32_t gx = 0; gx < mvw; ++gx) {
            const size_t idx = ((size_t)gy * mvw + gx) * 2u;
            const float u = uv[idx + 0];
            const float v = uv[idx + 1];
            const double rx = u - (a + b*gx + c*gy);
            const double ry = v - (d + e*gx + f*gy);
            const double r  = std::sqrt(rx*rx + ry*ry);
            if (r > 4.0) ++dissident;
            if (dis_out) { int q = (int)(16.0*r + 0.5); if (q > 255) q = 255;
                // STAGE-70: THE CHANGE GATE — dissent requires CHANGED content. A halo block's big MV is
                // a lie (its pixels did not change, sad_zero≈0) → byte 0, the mask hugs the real mover.
                // The mover's flat interior also gates out (yellow-over-yellow): the cluster becomes the
                // RIM RING, which the STAGE-61 scanline fill + STAGE-63 shape-field handle by design.
                // sad_raw layout = RG16F per block (R=sad_best, G=sad_zero); nullable (--no-change-gate).
                // STAGE-86b: szf is the pre-decoded sad_zero (set iff sad_raw != null) — same value the
                // old per-block half_to_float(((sad)[..*2+1])) produced.
                if (szf && q > 0) {
                    const float sz = szf[(size_t)gy*mvw + gx];
                    if (sz < kChangeGateSadZ) q = 0;
                }
                dis_out[(size_t)gy*mvw + gx] = (uint8_t)q; }
        }
    }
    return total ? 100.0 * (double)dissident / (double)total : 0.0;
}

// ── NvofaProvider factories (nvofa_img stays internal/static) ──
// Create one OFA-bound image (chains the usage hint; CONCURRENT over fams[2]; MUTABLE if mutable_view fmt set).
static bool nvofa_img(VDev& d,uint32_t w,uint32_t h,VkFormat fmt,VkImageUsageFlags usage,VkOpticalFlowUsageFlagsNV ofu,
                      const uint32_t* fams,VkImage& img,VkDeviceMemory& mem,VkImageView& ofView,
                      VkFormat ofViewFmt,VkFormat mut1Fmt,VkImageView* mut1,VkFormat mut2Fmt,VkImageView* mut2){
    VkOpticalFlowImageFormatInfoNV ofi{}; ofi.sType=VK_STRUCTURE_TYPE_OPTICAL_FLOW_IMAGE_FORMAT_INFO_NV; ofi.usage=ofu;
    VkImageFormatListCreateInfo fl{}; VkFormat fmts[3]; uint32_t nfmt=0; fmts[nfmt++]=fmt;
    if(mut1Fmt!=VK_FORMAT_UNDEFINED) fmts[nfmt++]=mut1Fmt; if(mut2Fmt!=VK_FORMAT_UNDEFINED) fmts[nfmt++]=mut2Fmt;
    fl.sType=VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO; fl.viewFormatCount=nfmt; fl.pViewFormats=fmts; fl.pNext=&ofi;
    VkImageCreateInfo ci{}; ci.sType=VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO; ci.pNext=&fl; ci.imageType=VK_IMAGE_TYPE_2D; ci.format=fmt;
    ci.extent={w,h,1}; ci.mipLevels=1; ci.arrayLayers=1; ci.samples=VK_SAMPLE_COUNT_1_BIT; ci.tiling=VK_IMAGE_TILING_OPTIMAL; ci.usage=usage; ci.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
    if(nfmt>1) ci.flags|=VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;   // a view of a DIFFERENT (compatible) format
    ci.sharingMode=VK_SHARING_MODE_CONCURRENT; ci.queueFamilyIndexCount=2; ci.pQueueFamilyIndices=fams;
    if(vkCreateImage(d.dev,&ci,nullptr,&img)!=VK_SUCCESS) return false;
    VkMemoryRequirements mr; vkGetImageMemoryRequirements(d.dev,img,&mr); const uint32_t mt=pick_mem(d.mp,mr.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT); if(mt==UINT32_MAX) return false;
    VkMemoryAllocateInfo mai{}; mai.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO; mai.allocationSize=mr.size; mai.memoryTypeIndex=mt; if(vkAllocateMemory(d.dev,&mai,nullptr,&mem)!=VK_SUCCESS) return false; vkBindImageMemory(d.dev,img,mem,0);
    auto mkview=[&](VkFormat vf,VkImageView* out){ VkImageViewCreateInfo vi{}; vi.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO; vi.image=img; vi.viewType=VK_IMAGE_VIEW_TYPE_2D; vi.format=vf; vi.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1}; return vkCreateImageView(d.dev,&vi,nullptr,out)==VK_SUCCESS; };
    if(!mkview(ofViewFmt,&ofView)) return false;
    if(mut1Fmt!=VK_FORMAT_UNDEFINED && mut1 && !mkview(mut1Fmt,mut1)) return false;
    if(mut2Fmt!=VK_FORMAT_UNDEFINED && mut2 && !mkview(mut2Fmt,mut2)) return false;
    return true;
}

// Build the provider on device d (== A == FD under single_gpu). mvw/mvh = ofp.motion_width()/height().
bool nvofa_create(VDev& d,NvofaProvider& n,uint32_t mvw,uint32_t mvh,uint32_t WW_flow,bool bidir,
                         float cost_scale,float sadz_scale,const std::vector<uint32_t>& spv){
    if(d.ofaQueue==VK_NULL_HANDLE||d.pfnCreateOFSession==nullptr) return false;
    n.mvw=mvw; n.mvh=mvh; n.bidir=bidir; n.cost_scale=cost_scale; n.sadz_scale=sadz_scale;
    n.in_w=mvw*4u; n.in_h=mvh*4u; n.blk=(int)(n.in_w/mvw);   // = 4
    n.mv_scale=(float)WW_flow/(float)n.in_w;                 // OFA-input px -> WW_flow px (classical units)
    const uint32_t fams_in[2]={d.qfam,d.ofaQfam};            // written FD.q2, read OFA
    const uint32_t fams_out[2]={d.ofaQfam,d.qfam};           // written OFA, read convert (FD.q2)
    const VkFormat flowFmt=VK_FORMAT_R16G16_SFIXED5_NV, costFmt=VK_FORMAT_R8_UINT;
    // input images (BGRA8 — the OFA input; reads from the convert side too, sampled): SAMPLED|TRANSFER_DST.
    if(!nvofa_img(d,n.in_w,n.in_h,VK_FORMAT_B8G8R8A8_UNORM,VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,VK_OPTICAL_FLOW_USAGE_INPUT_BIT_NV,fams_in,n.in0,n.in0m,n.in0v,VK_FORMAT_B8G8R8A8_UNORM,VK_FORMAT_UNDEFINED,&n.in0cv,VK_FORMAT_UNDEFINED,nullptr)) return false;
    n.in0cv=n.in0v;   // the convert samples the same BGRA8 view
    if(!nvofa_img(d,n.in_w,n.in_h,VK_FORMAT_B8G8R8A8_UNORM,VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,VK_OPTICAL_FLOW_USAGE_INPUT_BIT_NV,fams_in,n.in1,n.in1m,n.in1v,VK_FORMAT_B8G8R8A8_UNORM,VK_FORMAT_UNDEFINED,nullptr,VK_FORMAT_UNDEFINED,nullptr)) return false;
    n.in1cv=n.in1v;
    // OFA flow output: the OF-bind view is the SFIXED5 format; the convert reads a MUTABLE rg16i view (same
    // 32-bit class — compatible) so imageLoad returns the raw int16 components (raw/32 = px).
    if(!nvofa_img(d,mvw,mvh,flowFmt,VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,VK_OPTICAL_FLOW_USAGE_OUTPUT_BIT_NV,fams_out,n.mvF,n.mvFm,n.mvFofv,flowFmt,VK_FORMAT_R16G16_SINT,&n.mvFiv,VK_FORMAT_UNDEFINED,nullptr)) return false;
    if(!nvofa_img(d,mvw,mvh,costFmt,VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,VK_OPTICAL_FLOW_USAGE_COST_BIT_NV,fams_out,n.costF,n.costFm,n.costFofv,costFmt,VK_FORMAT_R8_UINT,&n.costFuv,VK_FORMAT_UNDEFINED,nullptr)) return false;
    n.costFuv=n.costFofv;   // R8_UINT already — the OF view IS the r8ui view
    if(bidir){
        if(!nvofa_img(d,mvw,mvh,flowFmt,VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,VK_OPTICAL_FLOW_USAGE_OUTPUT_BIT_NV,fams_out,n.mvB,n.mvBm,n.mvBofv,flowFmt,VK_FORMAT_R16G16_SINT,&n.mvBiv,VK_FORMAT_UNDEFINED,nullptr)) return false;
        if(!nvofa_img(d,mvw,mvh,costFmt,VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,VK_OPTICAL_FLOW_USAGE_COST_BIT_NV,fams_out,n.costB,n.costBm,n.costBofv,costFmt,VK_FORMAT_R8_UINT,&n.costBuv,VK_FORMAT_UNDEFINED,nullptr)) return false;
        n.costBuv=n.costBofv;
    }
    // OFA session: input BGRA8, 4x4 grid -> (in_w+3)/4 = mvw grid exactly. FAST, +cost, +bidir.
    VkOpticalFlowSessionCreateInfoNV sci{}; sci.sType=VK_STRUCTURE_TYPE_OPTICAL_FLOW_SESSION_CREATE_INFO_NV;
    sci.width=n.in_w; sci.height=n.in_h; sci.imageFormat=VK_FORMAT_B8G8R8A8_UNORM; sci.flowVectorFormat=flowFmt; sci.costFormat=costFmt;
    sci.outputGridSize=VK_OPTICAL_FLOW_GRID_SIZE_4X4_BIT_NV; sci.hintGridSize=VK_OPTICAL_FLOW_GRID_SIZE_UNKNOWN_NV;
    sci.performanceLevel=VK_OPTICAL_FLOW_PERFORMANCE_LEVEL_FAST_NV; sci.flags=VK_OPTICAL_FLOW_SESSION_CREATE_ENABLE_COST_BIT_NV;
    if(bidir) sci.flags|=VK_OPTICAL_FLOW_SESSION_CREATE_BOTH_DIRECTIONS_BIT_NV;
    if(d.pfnCreateOFSession(d.dev,&sci,nullptr,&n.sess)!=VK_SUCCESS){ std::printf("[ra] --nvofa: vkCreateOpticalFlowSessionNV(%ux%u) failed — disabling (classical OFP)\n",n.in_w,n.in_h); return false; }
    // bind the session images (GENERAL layout — set below at first use; bind takes the OF views).
    auto bind=[&](VkOpticalFlowSessionBindingPointNV bp,VkImageView v)->bool{ return d.pfnBindOFImage(d.dev,n.sess,bp,v,VK_IMAGE_LAYOUT_GENERAL)==VK_SUCCESS; };
    if(!bind(VK_OPTICAL_FLOW_SESSION_BINDING_POINT_INPUT_NV,n.in0v)||!bind(VK_OPTICAL_FLOW_SESSION_BINDING_POINT_REFERENCE_NV,n.in1v)||
       !bind(VK_OPTICAL_FLOW_SESSION_BINDING_POINT_FLOW_VECTOR_NV,n.mvFofv)||!bind(VK_OPTICAL_FLOW_SESSION_BINDING_POINT_COST_NV,n.costFofv)){ std::printf("[ra] --nvofa: OF image bind failed — disabling\n"); return false; }
    if(bidir && (!bind(VK_OPTICAL_FLOW_SESSION_BINDING_POINT_BACKWARD_FLOW_VECTOR_NV,n.mvBofv)||!bind(VK_OPTICAL_FLOW_SESSION_BINDING_POINT_BACKWARD_COST_NV,n.costBofv))){ std::printf("[ra] --nvofa: bwd OF image bind failed — disabling\n"); return false; }
    // convert pipeline: sampler + 6-binding DSL + the SPV; 2 sets (fwd/bwd) drawn from one pool.
    VkSamplerCreateInfo ss{}; ss.sType=VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO; ss.magFilter=ss.minFilter=VK_FILTER_NEAREST; ss.addressModeU=ss.addressModeV=ss.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE; vkCreateSampler(d.dev,&ss,nullptr,&n.samp);
    VkDescriptorSetLayoutBinding lb[6]{};
    lb[0].binding=0; lb[0].descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_IMAGE; lb[0].descriptorCount=1; lb[0].stageFlags=VK_SHADER_STAGE_COMPUTE_BIT;   // OFA flow (rg16i)
    lb[1].binding=1; lb[1].descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_IMAGE; lb[1].descriptorCount=1; lb[1].stageFlags=VK_SHADER_STAGE_COMPUTE_BIT;   // OFA cost (r8ui)
    lb[2].binding=2; lb[2].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; lb[2].descriptorCount=1; lb[2].stageFlags=VK_SHADER_STAGE_COMPUTE_BIT;   // input A
    lb[3].binding=3; lb[3].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; lb[3].descriptorCount=1; lb[3].stageFlags=VK_SHADER_STAGE_COMPUTE_BIT;   // input B
    lb[4].binding=4; lb[4].descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_IMAGE; lb[4].descriptorCount=1; lb[4].stageFlags=VK_SHADER_STAGE_COMPUTE_BIT;   // OFP MV out
    lb[5].binding=5; lb[5].descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_IMAGE; lb[5].descriptorCount=1; lb[5].stageFlags=VK_SHADER_STAGE_COMPUTE_BIT;   // OFP SAD out
    VkDescriptorSetLayoutCreateInfo dlc{}; dlc.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO; dlc.bindingCount=6; dlc.pBindings=lb; if(vkCreateDescriptorSetLayout(d.dev,&dlc,nullptr,&n.dsl)!=VK_SUCCESS) return false;
    VkPushConstantRange pcr{}; pcr.stageFlags=VK_SHADER_STAGE_COMPUTE_BIT; pcr.size=24;   // mv_scale,cost_scale,sadz_scale,blk,mvw,mvh
    VkPipelineLayoutCreateInfo plc{}; plc.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO; plc.setLayoutCount=1; plc.pSetLayouts=&n.dsl; plc.pushConstantRangeCount=1; plc.pPushConstantRanges=&pcr; if(vkCreatePipelineLayout(d.dev,&plc,nullptr,&n.layout)!=VK_SUCCESS) return false;
    VkShaderModuleCreateInfo smci{}; smci.sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO; smci.codeSize=spv.size()*sizeof(uint32_t); smci.pCode=spv.data(); VkShaderModule sm; if(vkCreateShaderModule(d.dev,&smci,nullptr,&sm)!=VK_SUCCESS) return false;
    VkPipelineShaderStageCreateInfo st{}; st.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; st.stage=VK_SHADER_STAGE_COMPUTE_BIT; st.module=sm; st.pName="main";
    VkComputePipelineCreateInfo cp{}; cp.sType=VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO; cp.stage=st; cp.layout=n.layout; bool pok=(vkCreateComputePipelines(d.dev,VK_NULL_HANDLE,1,&cp,nullptr,&n.pipe)==VK_SUCCESS); vkDestroyShaderModule(d.dev,sm,nullptr); if(!pok) return false;
    const VkDescriptorPoolSize psz[2]={{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,4*2},{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,2*2}};
    VkDescriptorPoolCreateInfo dpc{}; dpc.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO; dpc.maxSets=2; dpc.poolSizeCount=2; dpc.pPoolSizes=psz; if(vkCreateDescriptorPool(d.dev,&dpc,nullptr,&n.dp)!=VK_SUCCESS) return false;
    return true;
}

// Write a convert descriptor set: ofaFlowView (rg16i), ofaCostView (r8ui), inAview, inBview, mvOut, sadOut.
void nvofa_write_set(VDev& d,NvofaProvider& n,VkDescriptorSet set,VkImageView flowiv,VkImageView costuv,VkImageView inA,VkImageView inB,VkImageView mvOut,VkImageView sadOut){
    VkDescriptorImageInfo i0{}; i0.imageView=flowiv; i0.imageLayout=VK_IMAGE_LAYOUT_GENERAL;
    VkDescriptorImageInfo i1{}; i1.imageView=costuv; i1.imageLayout=VK_IMAGE_LAYOUT_GENERAL;
    VkDescriptorImageInfo i2{}; i2.sampler=n.samp; i2.imageView=inA; i2.imageLayout=VK_IMAGE_LAYOUT_GENERAL;
    VkDescriptorImageInfo i3{}; i3.sampler=n.samp; i3.imageView=inB; i3.imageLayout=VK_IMAGE_LAYOUT_GENERAL;
    VkDescriptorImageInfo i4{}; i4.imageView=mvOut; i4.imageLayout=VK_IMAGE_LAYOUT_GENERAL;
    VkDescriptorImageInfo i5{}; i5.imageView=sadOut; i5.imageLayout=VK_IMAGE_LAYOUT_GENERAL;
    VkWriteDescriptorSet w[6]={};
    for(int i=0;i<6;++i){ w[i].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET; w[i].dstSet=set; w[i].dstBinding=(uint32_t)i; w[i].descriptorCount=1; }
    w[0].descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_IMAGE; w[0].pImageInfo=&i0;
    w[1].descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_IMAGE; w[1].pImageInfo=&i1;
    w[2].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; w[2].pImageInfo=&i2;
    w[3].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; w[3].pImageInfo=&i3;
    w[4].descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_IMAGE; w[4].pImageInfo=&i4;
    w[5].descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_IMAGE; w[5].pImageInfo=&i5;
    vkUpdateDescriptorSets(d.dev,6,w,0,nullptr);
}

void nvofa_destroy(VDev& d,NvofaProvider& n){
    if(n.sess) d.pfnDestroyOFSession(d.dev,n.sess,nullptr);
    if(n.pipe) vkDestroyPipeline(d.dev,n.pipe,nullptr); if(n.layout) vkDestroyPipelineLayout(d.dev,n.layout,nullptr); if(n.dsl) vkDestroyDescriptorSetLayout(d.dev,n.dsl,nullptr); if(n.dp) vkDestroyDescriptorPool(d.dev,n.dp,nullptr); if(n.samp) vkDestroySampler(d.dev,n.samp,nullptr);
    if(n.pool) vkDestroyCommandPool(d.dev,n.pool,nullptr);   // STAGE-115 fix: the F-thread-private prep/convert pool
    auto dv=[&](VkImageView v){ if(v) vkDestroyImageView(d.dev,v,nullptr); };
    dv(n.in0v); dv(n.in1v); dv(n.mvFofv); dv(n.mvFiv); dv(n.mvBofv); dv(n.mvBiv);   // costFuv==costFofv / costBuv==costBofv (R8 self-view) — freed once below
    dv(n.costFofv); dv(n.costBofv);
    auto di=[&](VkImage im,VkDeviceMemory m){ if(im) vkDestroyImage(d.dev,im,nullptr); if(m) vkFreeMemory(d.dev,m,nullptr); };
    di(n.in0,n.in0m); di(n.in1,n.in1m); di(n.mvF,n.mvFm); di(n.costF,n.costFm); di(n.mvB,n.mvBm); di(n.costB,n.costBm);
    if(n.fPrep) vkDestroyFence(d.dev,n.fPrep,nullptr); if(n.fOfa) vkDestroyFence(d.dev,n.fOfa,nullptr); if(n.fConv) vkDestroyFence(d.dev,n.fConv,nullptr);
    if(n.semPrep) vkDestroySemaphore(d.dev,n.semPrep,nullptr); if(n.semOfa) vkDestroySemaphore(d.dev,n.semOfa,nullptr);   // STAGE-115b: the chain semaphores
    n=NvofaProvider{};
}

// Allocate the per-pair cmd buffers + fences once (cmdPrep/cmdConv on FD.pool→FD.q2; cmdOfa on FD.ofaPool→
// FD.ofaQueue). Called after nvofa_create succeeds. Returns false on any alloc failure (→ disable).
bool nvofa_alloc_cmds(VDev& d,NvofaProvider& n){
    // STAGE-115 (CRASH-FIX, validation MultipleThreads-Write): cmdPrep/cmdConv run on the F-thread; recording them
    // from d.pool (the P-thread's present pool) races P (command pools are externally-synchronized). Give the OFA
    // prep/convert their OWN pool on d.qfam (the graphics+compute family — they submit to A.q2, same family). cmdOfa
    // stays on d.ofaPool (the OFA queue family).
    { VkCommandPoolCreateInfo cp{}; cp.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO; cp.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; cp.queueFamilyIndex=d.qfam; if(vkCreateCommandPool(d.dev,&cp,nullptr,&n.pool)!=VK_SUCCESS) return false; }
    VkCommandBufferAllocateInfo a{}; a.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO; a.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY; a.commandBufferCount=1;
    a.commandPool=n.pool; if(vkAllocateCommandBuffers(d.dev,&a,&n.cmdPrep)!=VK_SUCCESS) return false;
    a.commandPool=n.pool; if(vkAllocateCommandBuffers(d.dev,&a,&n.cmdConv)!=VK_SUCCESS) return false;
    a.commandPool=d.ofaPool; if(vkAllocateCommandBuffers(d.dev,&a,&n.cmdOfa)!=VK_SUCCESS) return false;
    VkFenceCreateInfo f{}; f.sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    if(vkCreateFence(d.dev,&f,nullptr,&n.fPrep)!=VK_SUCCESS||vkCreateFence(d.dev,&f,nullptr,&n.fOfa)!=VK_SUCCESS||vkCreateFence(d.dev,&f,nullptr,&n.fConv)!=VK_SUCCESS) return false;
    // STAGE-115b: the two binary semaphores that chain prep→OFA→convert on the GPU (1 CPU wait per pair).
    VkSemaphoreCreateInfo sc{}; sc.sType=VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    if(vkCreateSemaphore(d.dev,&sc,nullptr,&n.semPrep)!=VK_SUCCESS||vkCreateSemaphore(d.dev,&sc,nullptr,&n.semOfa)!=VK_SUCCESS) return false;
    n.ok=true; return true;
}

// ── MvSmoothPipe factories ──
bool mvsm_create(VDev& d,VkImage mv_img,VkImageView prev_view,const std::vector<uint32_t>& spv,MvSmoothPipe& p){
    VkImageViewCreateInfo vi{}; vi.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO; vi.image=mv_img; vi.viewType=VK_IMAGE_VIEW_TYPE_2D; vi.format=VK_FORMAT_R16G16_SFLOAT; vi.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
    if(vkCreateImageView(d.dev,&vi,nullptr,&p.mv_store)!=VK_SUCCESS) return false;
    const VkDescriptorSetLayoutBinding bd[2]={{0,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr},{1,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr}};
    VkDescriptorSetLayoutCreateInfo dl{}; dl.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO; dl.bindingCount=2; dl.pBindings=bd; vkCreateDescriptorSetLayout(d.dev,&dl,nullptr,&p.dsl);
    VkPushConstantRange pcr{}; pcr.stageFlags=VK_SHADER_STAGE_COMPUTE_BIT; pcr.size=20; // {alpha,cut,w,h,span} (35-R2b)
    VkPipelineLayoutCreateInfo pl{}; pl.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO; pl.setLayoutCount=1; pl.pSetLayouts=&p.dsl; pl.pushConstantRangeCount=1; pl.pPushConstantRanges=&pcr; vkCreatePipelineLayout(d.dev,&pl,nullptr,&p.layout);
    VkShaderModuleCreateInfo mci{}; mci.sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO; mci.codeSize=spv.size()*sizeof(uint32_t); mci.pCode=spv.data(); VkShaderModule mod; vkCreateShaderModule(d.dev,&mci,nullptr,&mod);
    VkPipelineShaderStageCreateInfo stg{}; stg.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; stg.stage=VK_SHADER_STAGE_COMPUTE_BIT; stg.module=mod; stg.pName="main";
    VkComputePipelineCreateInfo cp{}; cp.sType=VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO; cp.stage=stg; cp.layout=p.layout; vkCreateComputePipelines(d.dev,VK_NULL_HANDLE,1,&cp,nullptr,&p.pipe); vkDestroyShaderModule(d.dev,mod,nullptr);
    const VkDescriptorPoolSize psz[1]={{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,2}};
    VkDescriptorPoolCreateInfo pi{}; pi.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO; pi.maxSets=1; pi.poolSizeCount=1; pi.pPoolSizes=psz; vkCreateDescriptorPool(d.dev,&pi,nullptr,&p.pool);
    VkDescriptorSetAllocateInfo dai{}; dai.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO; dai.descriptorPool=p.pool; dai.descriptorSetCount=1; dai.pSetLayouts=&p.dsl; vkAllocateDescriptorSets(d.dev,&dai,&p.set);
    VkDescriptorImageInfo i0{}; i0.imageView=p.mv_store; i0.imageLayout=VK_IMAGE_LAYOUT_GENERAL;
    VkDescriptorImageInfo i1{}; i1.imageView=prev_view; i1.imageLayout=VK_IMAGE_LAYOUT_GENERAL;
    const VkWriteDescriptorSet wds[2]={
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,nullptr,p.set,0,0,1,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,&i0,nullptr,nullptr},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,nullptr,p.set,1,0,1,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,&i1,nullptr,nullptr}};
    vkUpdateDescriptorSets(d.dev,2,wds,0,nullptr); return p.pipe!=VK_NULL_HANDLE;
}
void mvsm_destroy(VDev& d,MvSmoothPipe& p){ if(p.mv_store)vkDestroyImageView(d.dev,p.mv_store,nullptr); if(p.pool)vkDestroyDescriptorPool(d.dev,p.pool,nullptr); if(p.pipe)vkDestroyPipeline(d.dev,p.pipe,nullptr); if(p.layout)vkDestroyPipelineLayout(d.dev,p.layout,nullptr); if(p.dsl)vkDestroyDescriptorSetLayout(d.dev,p.dsl,nullptr); p=MvSmoothPipe{}; }

// ── GmePipe factories (gme_make_pipe / gme_buf_barrier stay internal/static) ──
static VkPipeline gme_make_pipe(VDev& d,VkPipelineLayout lay,const std::vector<uint32_t>& spv){
    VkShaderModuleCreateInfo mci{}; mci.sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO; mci.codeSize=spv.size()*sizeof(uint32_t); mci.pCode=spv.data();
    VkShaderModule mod; if(vkCreateShaderModule(d.dev,&mci,nullptr,&mod)!=VK_SUCCESS) return VK_NULL_HANDLE;
    VkPipelineShaderStageCreateInfo stg{}; stg.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; stg.stage=VK_SHADER_STAGE_COMPUTE_BIT; stg.module=mod; stg.pName="main";
    VkComputePipelineCreateInfo cp{}; cp.sType=VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO; cp.stage=stg; cp.layout=lay;
    VkPipeline pipe=VK_NULL_HANDLE; vkCreateComputePipelines(d.dev,VK_NULL_HANDLE,1,&cp,nullptr,&pipe); vkDestroyShaderModule(d.dev,mod,nullptr); return pipe;
}
// mv_img/sad_img = ofp.motion_image()/sad_field_image(). dis_fwd[gen]/dis_bwd[gen] = the per-gen hostDIS
// bridge BUFFERS imported on B (hDIS_b[gen].buf / hDISB_b[gen].buf). dis_bytes = mvw*mvh (the packed
// mask length). want_bwd binds the bwd anchor sets (bidir). A null dis_bwd[gen] leaves that set unbuilt.
bool gme_create(VDev& d,VkImage mv_img,VkImage sad_img,
                       VkBuffer dis_fwd[kGenRing],VkBuffer dis_bwd[kGenRing],VkDeviceSize dis_bytes,
                       bool want_bwd,
                       const std::vector<uint32_t>& rspv,const std::vector<uint32_t>& sspv,
                       const std::vector<uint32_t>& dspv,GmePipe& g){
    // Storage views aliasing the OFP MV/SAD (RG16F).
    VkImageViewCreateInfo vi{}; vi.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO; vi.viewType=VK_IMAGE_VIEW_TYPE_2D; vi.format=VK_FORMAT_R16G16_SFLOAT; vi.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
    vi.image=mv_img;  if(vkCreateImageView(d.dev,&vi,nullptr,&g.mv_store )!=VK_SUCCESS) return false;
    vi.image=sad_img; if(vkCreateImageView(d.dev,&vi,nullptr,&g.sad_store)!=VK_SUCCESS) return false;
    // Small device-local SSBOs (Accum 12 floats, Model 6 floats). STORAGE + TRANSFER (model copy-out;
    // accum is also fill-reset on GPU between passes — TRANSFER_DST covers it; harmless on model).
    if(!dbuf_create(d,12u*sizeof(float),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT|VK_BUFFER_USAGE_TRANSFER_SRC_BIT,g.accum)) return false;
    if(!dbuf_create(d, 6u*sizeof(float),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT|VK_BUFFER_USAGE_TRANSFER_SRC_BIT,g.model)) return false;
    // Layouts.
    const VkDescriptorSetLayoutBinding rb[3]={{0,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr},{1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr},{2,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr}};
    const VkDescriptorSetLayoutBinding sb[2]={{0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr},{1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr}};
    const VkDescriptorSetLayoutBinding db[4]={{0,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr},{1,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr},{2,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr},{3,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr}};
    VkDescriptorSetLayoutCreateInfo dl{}; dl.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dl.bindingCount=3; dl.pBindings=rb; if(vkCreateDescriptorSetLayout(d.dev,&dl,nullptr,&g.rdsl)!=VK_SUCCESS) return false;
    dl.bindingCount=2; dl.pBindings=sb; if(vkCreateDescriptorSetLayout(d.dev,&dl,nullptr,&g.sdsl)!=VK_SUCCESS) return false;
    dl.bindingCount=4; dl.pBindings=db; if(vkCreateDescriptorSetLayout(d.dev,&dl,nullptr,&g.ddsl)!=VK_SUCCESS) return false;
    // Pipeline layouts + push ranges.
    VkPushConstantRange rpc{}; rpc.stageFlags=VK_SHADER_STAGE_COMPUTE_BIT; rpc.size=16; // {mvw,mvh,step,iter}
    VkPushConstantRange dpc{}; dpc.stageFlags=VK_SHADER_STAGE_COMPUTE_BIT; dpc.size=16; // {mvw,mvh,use_gate,sz}
    VkPipelineLayoutCreateInfo pl{}; pl.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO; pl.setLayoutCount=1;
    pl.pSetLayouts=&g.rdsl; pl.pushConstantRangeCount=1; pl.pPushConstantRanges=&rpc; if(vkCreatePipelineLayout(d.dev,&pl,nullptr,&g.rlay)!=VK_SUCCESS) return false;
    pl.pSetLayouts=&g.sdsl; pl.pushConstantRangeCount=0; pl.pPushConstantRanges=nullptr; if(vkCreatePipelineLayout(d.dev,&pl,nullptr,&g.slay)!=VK_SUCCESS) return false;
    pl.pSetLayouts=&g.ddsl; pl.pushConstantRangeCount=1; pl.pPushConstantRanges=&dpc; if(vkCreatePipelineLayout(d.dev,&pl,nullptr,&g.dlay)!=VK_SUCCESS) return false;
    g.rpipe=gme_make_pipe(d,g.rlay,rspv); g.spipe=gme_make_pipe(d,g.slay,sspv); g.dpipe=gme_make_pipe(d,g.dlay,dspv);
    if(!g.rpipe||!g.spipe||!g.dpipe) return false;
    // Descriptor pool: reduce(1 img+2 ssbo) + solve(2 ssbo) + dis sets ((kGenRing×ndis)×(2 img+2 ssbo)).
    const uint32_t ndis=want_bwd?2u:1u;
    const uint32_t ndset=(uint32_t)kGenRing*ndis;
    const VkDescriptorPoolSize psz[2]={{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1u+2u*ndset},{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,2u+2u+2u*ndset}};
    VkDescriptorPoolCreateInfo pi{}; pi.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO; pi.maxSets=2u+ndset; pi.poolSizeCount=2; pi.pPoolSizes=psz; if(vkCreateDescriptorPool(d.dev,&pi,nullptr,&g.pool)!=VK_SUCCESS) return false;
    auto alloc=[&](VkDescriptorSetLayout l,VkDescriptorSet& s)->bool{ VkDescriptorSetAllocateInfo a{}; a.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO; a.descriptorPool=g.pool; a.descriptorSetCount=1; a.pSetLayouts=&l; return vkAllocateDescriptorSets(d.dev,&a,&s)==VK_SUCCESS; };
    if(!alloc(g.rdsl,g.rset)||!alloc(g.sdsl,g.sset)) return false;
    for(int gen=0; gen<kGenRing; ++gen){ if(!alloc(g.ddsl,g.dset[gen][0])) return false; if(want_bwd && !alloc(g.ddsl,g.dset[gen][1])) return false; }
    // Shared buffer/image infos.
    VkDescriptorBufferInfo ba{}; ba.buffer=g.accum.buf; ba.offset=0; ba.range=VK_WHOLE_SIZE;
    VkDescriptorBufferInfo bm{}; bm.buffer=g.model.buf; bm.offset=0; bm.range=VK_WHOLE_SIZE;
    VkDescriptorImageInfo  imv{}; imv.imageView=g.mv_store;  imv.imageLayout=VK_IMAGE_LAYOUT_GENERAL;
    VkDescriptorImageInfo  isa{}; isa.imageView=g.sad_store; isa.imageLayout=VK_IMAGE_LAYOUT_GENERAL;
    // reduce set: 0=MV, 1=Accum, 2=Model
    const VkWriteDescriptorSet rw[3]={
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,nullptr,g.rset,0,0,1,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,&imv,nullptr,nullptr},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,nullptr,g.rset,1,0,1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,nullptr,&ba,nullptr},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,nullptr,g.rset,2,0,1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,nullptr,&bm,nullptr}};
    vkUpdateDescriptorSets(d.dev,3,rw,0,nullptr);
    // solve set: 0=Accum, 1=Model
    const VkWriteDescriptorSet sw[2]={
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,nullptr,g.sset,0,0,1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,nullptr,&ba,nullptr},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,nullptr,g.sset,1,0,1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,nullptr,&bm,nullptr}};
    vkUpdateDescriptorSets(d.dev,2,sw,0,nullptr);
    // dissidence sets per (gen, anchor): 0=MV, 1=SAD, 2=DisMask(gen,anchor), 3=Model
    for(int gen=0; gen<kGenRing; ++gen){
        for(uint32_t an=0; an<ndis; ++an){
            VkBuffer disbuf = (an==0u) ? dis_fwd[gen] : (dis_bwd? dis_bwd[gen] : VK_NULL_HANDLE);
            if(disbuf==VK_NULL_HANDLE) continue;
            VkDescriptorBufferInfo bd2{}; bd2.buffer=disbuf; bd2.offset=0; bd2.range=dis_bytes;
            const VkWriteDescriptorSet dw[4]={
                {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,nullptr,g.dset[gen][an],0,0,1,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,&imv,nullptr,nullptr},
                {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,nullptr,g.dset[gen][an],1,0,1,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,&isa,nullptr,nullptr},
                {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,nullptr,g.dset[gen][an],2,0,1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,nullptr,&bd2,nullptr},
                {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,nullptr,g.dset[gen][an],3,0,1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,nullptr,&bm,nullptr}};
            vkUpdateDescriptorSets(d.dev,4,dw,0,nullptr);
        }
    }
    return true;
}
void gme_destroy(VDev& d,GmePipe& g){
    if(g.pool)vkDestroyDescriptorPool(d.dev,g.pool,nullptr);
    if(g.rpipe)vkDestroyPipeline(d.dev,g.rpipe,nullptr); if(g.spipe)vkDestroyPipeline(d.dev,g.spipe,nullptr); if(g.dpipe)vkDestroyPipeline(d.dev,g.dpipe,nullptr);
    if(g.rlay)vkDestroyPipelineLayout(d.dev,g.rlay,nullptr); if(g.slay)vkDestroyPipelineLayout(d.dev,g.slay,nullptr); if(g.dlay)vkDestroyPipelineLayout(d.dev,g.dlay,nullptr);
    if(g.rdsl)vkDestroyDescriptorSetLayout(d.dev,g.rdsl,nullptr); if(g.sdsl)vkDestroyDescriptorSetLayout(d.dev,g.sdsl,nullptr); if(g.ddsl)vkDestroyDescriptorSetLayout(d.dev,g.ddsl,nullptr);
    if(g.mv_store)vkDestroyImageView(d.dev,g.mv_store,nullptr); if(g.sad_store)vkDestroyImageView(d.dev,g.sad_store,nullptr);
    hbuf_destroy(d,g.accum); hbuf_destroy(d,g.model);
    g=GmePipe{};
}
// A small compute-shader buffer barrier (shader-write → shader-read, or transfer-write → shader-read).
static void gme_buf_barrier(VkCommandBuffer c,VkBuffer b,VkAccessFlags sa,VkAccessFlags da,
                            VkPipelineStageFlags ss,VkPipelineStageFlags ds){
    VkBufferMemoryBarrier mb{}; mb.sType=VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER; mb.srcAccessMask=sa; mb.dstAccessMask=da;
    mb.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED; mb.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED; mb.buffer=b; mb.offset=0; mb.size=VK_WHOLE_SIZE;
    vkCmdPipelineBarrier(c,ss,ds,0,0,nullptr,1,&mb,0,nullptr);
}
// ── IDEA-1 (gme-gpu): record the full affine fit for ONE anchor into `cmd`, run ON DEVICE B. ────────
// Records, AFTER record_optical_flow has left mv_img/sad_img in SHADER_READ_ONLY_OPTIMAL:
//   (a) MV+SAD RO → GENERAL (storage-image access for the compute passes)
//   (b) irls_iters × [reduce(iter) → accum barrier → solve → model barrier]  (the IRLS loop)
//   (c) fill the dis host bridge to 0 → barrier → dissidence dispatch → host-read barrier
//   (d) copy the 6-float Model SSBO → the host readback buffer (hGmeM) → host-read barrier
//   (e) MV+SAD GENERAL → RO (restore the layout the downstream MV/SAD copy-out expects)
// gen/anchor select the dis descriptor set; dis_buf/dis_bytes the per-(gen,anchor) host bridge; model_dst
// the per-gen 6-float readback. step = the CPU sub2 stride (1 or 2); use_gate = cfg.change_gate.
void gme_record(VkCommandBuffer cmd,GmePipe& g,VkImage mv_img,VkImage sad_img,
                       uint32_t mvw,uint32_t mvh,uint32_t step,bool use_gate,
                       int gen,int anchor,VkBuffer dis_buf,VkDeviceSize dis_bytes,
                       VkBuffer model_dst,int irls_iters){
    // (a) MV+SAD → GENERAL.
    img_barrier(cmd,mv_img, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_IMAGE_LAYOUT_GENERAL,VK_ACCESS_SHADER_READ_BIT,VK_ACCESS_SHADER_READ_BIT);
    img_barrier(cmd,sad_img,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_IMAGE_LAYOUT_GENERAL,VK_ACCESS_SHADER_READ_BIT,VK_ACCESS_SHADER_READ_BIT);
    // (b) IRLS loop: reduce(iter) → solve, chained through the accum + model SSBOs.
    for(int iter=0; iter<irls_iters; ++iter){
        struct{uint32_t mvw,mvh,step,iter;}rpc{mvw,mvh,step,(uint32_t)iter};
        vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_COMPUTE,g.rpipe);
        vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_COMPUTE,g.rlay,0,1,&g.rset,0,nullptr);
        vkCmdPushConstants(cmd,g.rlay,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(rpc),&rpc);
        vkCmdDispatch(cmd,1,1,1);   // ONE workgroup grid-strides the whole grid
        gme_buf_barrier(cmd,g.accum.buf,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_COMPUTE,g.spipe);
        vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_COMPUTE,g.slay,0,1,&g.sset,0,nullptr);
        vkCmdDispatch(cmd,1,1,1);
        // The next reduce reads model (iter≥1); the dissidence reads model. Make the solve's write visible.
        gme_buf_barrier(cmd,g.model.buf,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    }
    // (c) clear the dis bridge to 0, then the dissidence dispatch atomicOrs the dissident bytes in.
    vkCmdFillBuffer(cmd,dis_buf,0,VK_WHOLE_SIZE,0u);
    gme_buf_barrier(cmd,dis_buf,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    {
        struct{uint32_t mvw,mvh,use_gate;float sz;}dpc{mvw,mvh,use_gate?1u:0u,0.5f};   // sz = kChangeGateSadZ
        vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_COMPUTE,g.dpipe);
        vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_COMPUTE,g.dlay,0,1,&g.dset[gen][anchor],0,nullptr);
        vkCmdPushConstants(cmd,g.dlay,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(dpc),&dpc);
        vkCmdDispatch(cmd,(mvw+7)/8,(mvh+7)/8,1);
    }
    // Make the GPU dis-mask write visible to the HOST (F reads hostDIS after the fence).
    gme_buf_barrier(cmd,dis_buf,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_HOST_BIT);
    // (d) copy the 6-float model into the host readback bridge → host-visible.
    gme_buf_barrier(cmd,g.model.buf,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);
    { VkBufferCopy bc{0,0,6u*sizeof(float)}; vkCmdCopyBuffer(cmd,g.model.buf,model_dst,1,&bc); }
    gme_buf_barrier(cmd,model_dst,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT);
    // (e) MV+SAD GENERAL → RO (the downstream copy-out + the next match expect RO).
    img_barrier(cmd,mv_img, VK_IMAGE_LAYOUT_GENERAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_ACCESS_SHADER_READ_BIT,VK_ACCESS_SHADER_READ_BIT);
    img_barrier(cmd,sad_img,VK_IMAGE_LAYOUT_GENERAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_ACCESS_SHADER_READ_BIT,VK_ACCESS_SHADER_READ_BIT);
    (void)dis_bytes;
}

// ── MedianPipe factories ──
bool med_create(VDev& d,VkImageView mv_v,VkImageView mvb_v,VkImageView cur_v,VkImageView out_v,bool with_mvb,const std::vector<uint32_t>& spv,MedianPipe& p){
    VkSamplerCreateInfo s{}; s.sType=VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO; s.magFilter=VK_FILTER_NEAREST; s.minFilter=VK_FILTER_NEAREST; s.addressModeU=s.addressModeV=s.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE; vkCreateSampler(d.dev,&s,nullptr,&p.samp);
    // STAGE-49c: binding 2 = cur_real (the membership color). NEAREST sampler is fine — the shader
    // samples block CENTRES at exact texel centres ((coord+0.5)/grid maps cleanly), no interpolation.
    const VkDescriptorSetLayoutBinding bd[3]={{0,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_COMPUTE_BIT,&p.samp},{1,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr},{2,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_COMPUTE_BIT,&p.samp}};
    VkDescriptorSetLayoutCreateInfo dl{}; dl.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO; dl.bindingCount=3; dl.pBindings=bd; vkCreateDescriptorSetLayout(d.dev,&dl,nullptr,&p.dsl);
    VkPushConstantRange pcr{}; pcr.stageFlags=VK_SHADER_STAGE_COMPUTE_BIT; pcr.size=4; // STAGE-49c: {sim_thresh}
    VkPipelineLayoutCreateInfo pl{}; pl.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO; pl.setLayoutCount=1; pl.pSetLayouts=&p.dsl; pl.pushConstantRangeCount=1; pl.pPushConstantRanges=&pcr; vkCreatePipelineLayout(d.dev,&pl,nullptr,&p.layout);
    VkShaderModuleCreateInfo mci{}; mci.sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO; mci.codeSize=spv.size()*sizeof(uint32_t); mci.pCode=spv.data(); VkShaderModule mod; vkCreateShaderModule(d.dev,&mci,nullptr,&mod);
    VkPipelineShaderStageCreateInfo stg{}; stg.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; stg.stage=VK_SHADER_STAGE_COMPUTE_BIT; stg.module=mod; stg.pName="main";
    VkComputePipelineCreateInfo cp{}; cp.sType=VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO; cp.stage=stg; cp.layout=p.layout; vkCreateComputePipelines(d.dev,VK_NULL_HANDLE,1,&cp,nullptr,&p.pipe); vkDestroyShaderModule(d.dev,mod,nullptr);
    const uint32_t nsets=with_mvb?2u:1u;
    // 2 samplers (MV in + cur_real) × nsets, 1 storage (scratch) × nsets.
    const VkDescriptorPoolSize psz[2]={{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,2u*nsets},{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,nsets}};
    VkDescriptorPoolCreateInfo pi{}; pi.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO; pi.maxSets=nsets; pi.poolSizeCount=2; pi.pPoolSizes=psz; vkCreateDescriptorPool(d.dev,&pi,nullptr,&p.pool);
    auto write_set=[&](VkDescriptorSet set,VkImageView in_v){
        VkDescriptorImageInfo si{}; si.sampler=p.samp; si.imageView=in_v; si.imageLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkDescriptorImageInfo di{}; di.imageView=out_v; di.imageLayout=VK_IMAGE_LAYOUT_GENERAL;
        VkDescriptorImageInfo ci{}; ci.sampler=p.samp; ci.imageView=cur_v; ci.imageLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        const VkWriteDescriptorSet wds[3]={{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,nullptr,set,0,0,1,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,&si,nullptr,nullptr},{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,nullptr,set,1,0,1,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,&di,nullptr,nullptr},{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,nullptr,set,2,0,1,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,&ci,nullptr,nullptr}};
        vkUpdateDescriptorSets(d.dev,3,wds,0,nullptr);
    };
    VkDescriptorSetAllocateInfo dai{}; dai.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO; dai.descriptorPool=p.pool; dai.descriptorSetCount=1; dai.pSetLayouts=&p.dsl;
    vkAllocateDescriptorSets(d.dev,&dai,&p.set_mv); write_set(p.set_mv,mv_v);
    if(with_mvb){ vkAllocateDescriptorSets(d.dev,&dai,&p.set_mvb); write_set(p.set_mvb,mvb_v); }
    return p.pipe!=VK_NULL_HANDLE;
}
void med_destroy(VDev& d,MedianPipe& p){ if(p.pool)vkDestroyDescriptorPool(d.dev,p.pool,nullptr); if(p.pipe)vkDestroyPipeline(d.dev,p.pipe,nullptr); if(p.layout)vkDestroyPipelineLayout(d.dev,p.layout,nullptr); if(p.dsl)vkDestroyDescriptorSetLayout(d.dev,p.dsl,nullptr); if(p.samp)vkDestroySampler(d.dev,p.samp,nullptr); p=MedianPipe{}; }

// -- Thread F - run_flow (STEP 5.3: VERBATIM relocation of main()'s thr_f_fn lambda; no logic
//    change). The alias preamble rebinds FgContext's references/array-pointers to the exact local
//    NAMES the [&] body used, so the body below is byte-identical to the lambda. NS + kMvCut were
//    static-constexpr in main() (not captured) -> redeclared here as the same constants. --
void run_flow(FgContext& ctx){
    static constexpr int NS=kGenRing;
    static constexpr float kMvCut=6.0f;   // R2: |dmv|>cut px -> bypass EMA (reversal/cut guard)
    auto& cfg = ctx.cfg;
    auto& c_seq = ctx.c_seq;
    auto& cap_slots = ctx.cap_slots;
    auto& c_slots = ctx.c_slots;
    auto& c_cv = ctx.c_cv;
    auto& c_mtx = ctx.c_mtx;
    auto& g_quit_threads = ctx.g_quit_threads;
    auto& use_igpu_convert = ctx.use_igpu_convert;
    auto& WW = ctx.WW;
    auto& WH = ctx.WH;
    auto& A = ctx.A;
    auto& single_gpu = ctx.single_gpu;
    auto& a_q2_mtx = ctx.a_q2_mtx;
    auto& B = ctx.B;
    auto& FD = ctx.FD;
    auto& pfg_enabled = ctx.pfg_enabled;
    auto& mvw_f = ctx.mvw_f;
    auto& mvh_f = ctx.mvh_f;
    auto& WW_flow = ctx.WW_flow;
    auto& WH_flow = ctx.WH_flow;
    auto& flow_div = ctx.flow_div;
    auto& ra_core_f = ctx.ra_core_f;
    auto& use_inertia = ctx.use_inertia;
    auto& use_objects = ctx.use_objects;
    auto& use_memory = ctx.use_memory;
    auto& use_bidir = ctx.use_bidir;
    auto& use_gme = ctx.use_gme;
    auto& use_gme_gpu = ctx.use_gme_gpu;
    auto& use_wap = ctx.use_wap;
    auto& use_nvofa = ctx.use_nvofa;
    auto& use_ambig = ctx.use_ambig;
    auto& use_mv_smooth = ctx.use_mv_smooth;
    auto& use_fwd_prestage = ctx.use_fwd_prestage;
    auto& b_q2_split = ctx.b_q2_split;
    auto& ofp = ctx.ofp;
    auto& ofpA = ctx.ofpA;
    auto& Cinterp = ctx.Cinterp;
    auto& CinterpA = ctx.CinterpA;
    auto& nvofa = ctx.nvofa;
    auto& gmePipe = ctx.gmePipe;
    auto& mvsm = ctx.mvsm;
    auto& Bframe = ctx.Bframe;
    auto& Bflow = ctx.Bflow;
    auto& AframeA = ctx.AframeA;
    auto& cmdB = ctx.cmdB;
    auto& cmdB_bwd = ctx.cmdB_bwd;
    auto& cmdB_fwd = ctx.cmdB_fwd;
    auto& cmdF_pre = ctx.cmdF_pre;
    auto& cmdB2 = ctx.cmdB2;
    auto& cmdA_fg = ctx.cmdA_fg;
    auto& fB = ctx.fB;
    auto& fB2 = ctx.fB2;
    auto& fB_fwd = ctx.fB_fwd;
    auto& fF_pre = ctx.fF_pre;
    auto& fA_fg = ctx.fA_fg;
    auto& tfb = ctx.tfb;
    auto& hostMV = ctx.hostMV;
    auto& hostSAD = ctx.hostSAD;
    auto& hostPER = ctx.hostPER;
    auto& hostDIS = ctx.hostDIS;
    auto& hostDISB = ctx.hostDISB;
    auto& hostMVB = ctx.hostMVB;
    auto& hostGmeM = ctx.hostGmeM;
    auto& hostGmeMB = ctx.hostGmeMB;
    auto& hostC2 = ctx.hostC2;
    auto& hostI = ctx.hostI;
    auto& hR_b = ctx.hR_b;
    auto& hRP_b = ctx.hRP_b;
    auto& hRP_b_dev = ctx.hRP_b_dev;
    auto& hMV_b = ctx.hMV_b;
    auto& hSAD_b = ctx.hSAD_b;
    auto& hMVB_b = ctx.hMVB_b;
    auto& hDIS_b = ctx.hDIS_b;
    auto& hDISB_b = ctx.hDISB_b;
    auto& hGmeM_b = ctx.hGmeM_b;
    auto& hGmeMB_b = ctx.hGmeMB_b;
    auto& hC2_b = ctx.hC2_b;
    auto& sbI = ctx.sbI;
    auto& hI_b = ctx.hI_b;
    auto& hI_a = ctx.hI_a;
    auto& ubPipe = ctx.ubPipe;
    auto& f_pair_cseq_a = ctx.f_pair_cseq_a;
    auto& f_pair_slot_a = ctx.f_pair_slot_a;
    auto& f_pair_tcap_a = ctx.f_pair_tcap_a;
    auto& f_pair_span_a = ctx.f_pair_span_a;
    auto& f_pair_n_a = ctx.f_pair_n_a;
    auto& f_pair_gme_a = ctx.f_pair_gme_a;
    auto& f_pair_gme_valid_a = ctx.f_pair_gme_valid_a;
    auto& f_pair_gme_bwd_a = ctx.f_pair_gme_bwd_a;
    auto& f_pair_mfwd_a = ctx.f_pair_mfwd_a;
    auto& f_pair_mbwd_a = ctx.f_pair_mbwd_a;
    auto& f_pair_disp_a = ctx.f_pair_disp_a;
    auto& f_pair_bwd_valid_a = ctx.f_pair_bwd_valid_a;
    auto& f_seq = ctx.f_seq;
    auto& f_cv = ctx.f_cv;
    auto& p_presenting = ctx.p_presenting;
    auto& src_interval_us = ctx.src_interval_us;
    auto& present_cost_us = ctx.present_cost_us;
    auto& stat_tier = ctx.stat_tier;
    auto& stat_bwd_skips = ctx.stat_bwd_skips;
    auto& stat_skips = ctx.stat_skips;
    auto& stat_lap = ctx.stat_lap;
    auto& stat_cons = ctx.stat_cons;
    auto& stat_flow_us = ctx.stat_flow_us;
    auto& stat_pair_us = ctx.stat_pair_us;
    auto& gme_fit_us = ctx.gme_fit_us;
    auto& gme_dis_x100 = ctx.gme_dis_x100;
    auto& obj_live = ctx.obj_live;
    auto& obj_rep_x10 = ctx.obj_rep_x10;
    auto& live_n_atomic = ctx.live_n_atomic;
    auto& lt_pickup_us = ctx.lt_pickup_us;
    auto& lt_preflow_us = ctx.lt_preflow_us;
    auto& lt_spin_us = ctx.lt_spin_us;
    auto& lt_fpub_us = ctx.lt_fpub_us;
            // S2 (--pin-test 5): MMCSS token held thread-local → RAII AvRevert at thread exit.
            // Inactive (no AVRT call) for modes 0-4 / no --pin (byte-identical-off).
            phyriad::hw::MmcssToken mmcss_f;
            // FIX #1 + S1 (THREAD_PROTECTION): pin the FLOW thread to the SPSC producer core AND
            // elevate it to HIGHEST (strictly below the present thread's TIME_CRITICAL). The measured
            // B(1080Ti)->5% collapse under COMBINED GPU+CPU saturation is the F-thread starved off the
            // scheduler (TB-C12 testbench, 2026-06-26). Gated on cfg.pin_threads; skip UINT32_MAX for the
            // pin. On failure log + continue (do NOT crash). DEFAULT-OFF -> byte-identical.
            if(cfg.pin_threads){
                // S0 ablation (--pin-test): affinity in modes 0/1/3; F HIGHEST-elevate in modes 0/2.
                const bool s0_pin_f   = (cfg.pin_test==0||cfg.pin_test==1||cfg.pin_test==3);
                const bool s0_elev_f  = (cfg.pin_test==0||cfg.pin_test==2);
                if(s0_pin_f && ra_core_f!=UINT32_MAX && !phyriad::hw::pin_current_thread(ra_core_f))
                    std::printf("[ra] WARN: flow-thread pin to core %u failed (continuing unpinned)\n", ra_core_f);
                if(s0_elev_f && !phyriad::hw::elevate_thread_rt(false))
                    std::printf("[ra] WARN: flow-thread priority elevation failed (continuing at normal priority)\n");
                // S2 mode 5 = MMCSS-COMPOSITE: F gets SOFT affinity (ideal-processor hint, migratable —
                // an unpinned elevated F migrates to a freer core, the affinity-trap R7 win) + MMCSS
                // 'Capture'/HIGH (kept below P's 'Pro Audio'/CRITICAL), with elevate_thread_rt(false)=
                // HIGHEST as the fallback when MMCSS is inactive. F-elevation is the B(1080Ti)->5% fix.
                if(cfg.pin_test==5){
                    if(ra_core_f!=UINT32_MAX
                       && !phyriad::hw::set_thread_ideal_processor(GetCurrentThreadId(), ra_core_f).has_value())
                        std::printf("[ra] WARN: flow-thread soft-affinity (ideal proc %u) failed (continuing unhinted)\n", ra_core_f);
                    mmcss_f = phyriad::hw::join_mmcss_task(phyriad::hw::MmcssTask::Capture, phyriad::hw::AvrtPriority::High);
                    if(mmcss_f.active())
                        std::printf("[ra] MMCSS-composite: F joined 'Capture'/HIGH (idx=%lu) + soft-affinity core %u\n", mmcss_f.task_index(), ra_core_f);
                    else
                        std::printf("[ra] MMCSS-composite: F 'Capture' join INACTIVE -> fallback elevate_thread_rt(HIGHEST)%s + soft-affinity core %u\n", phyriad::hw::elevate_thread_rt(false)?"":" FAILED", ra_core_f);
                }
            }
            int cur_f=0; bool have_prev_f=false;
            bool fg_on_prim_f=(pfg_enabled&&cfg.fg_gpu==FG_PRIMARY);
            // CR2/CR3 (SINGLE_GPU_COLLAPSE): the F-thread flow-submit lane. Multi-GPU → the original B.q path
            // (byte-identical). single_gpu → A.q2 (NOT submit_wait(FD)=A.q, which would collide with present),
            // serialized vs the C-thread convert by a_q2_mtx. Two forms: a wait variant (the serial WAP +
            // every non-WAP submit_wait(B,cmdB,fB)) and a no-wait variant (the pipelined WAP + the bidir bwd
            // raw vkQueueSubmit(B.q,…)). cmdB/cmdF/cmdB_bwd are FD.pool-bound (A.pool under single_gpu, same
            // family as A.q2). These wrappers are the ONLY change to the flow submit sites.
            auto flow_submit_wait = [&](VkCommandBuffer cmd, VkFence f){
                if(single_gpu){ std::lock_guard<std::mutex> lk(a_q2_mtx); submit_wait_q2(A,cmd,f); }
                else submit_wait(B,cmd,f);
            };
            auto flow_submit_nowait = [&](VkCommandBuffer cmd, VkFence f){
                // reset the fence then submit (no wait). Under single_gpu lock a_q2_mtx for the submit only.
                if(single_gpu){
                    vkResetFences(A.dev,1,&f);
                    std::lock_guard<std::mutex> lk(a_q2_mtx);
                    VkSubmitInfo si{}; si.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO; si.commandBufferCount=1; si.pCommandBuffers=&cmd;
                    vkQueueSubmit(A.q2,1,&si,f);
                } else {
                    vkResetFences(B.dev,1,&f);
                    VkSubmitInfo si{}; si.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO; si.commandBufferCount=1; si.pCommandBuffers=&cmd;
                    vkQueueSubmit(B.q,1,&si,f);
                }
            };
            // STAGE-115b (--nvofa semaphore chain): submit ONE cmd to A.q2 UNDER a_q2_mtx with optional
            // {waitSem,waitStage,signalSem,fence} — SUBMIT ONLY (no CPU wait under the lock). nvofa_run passes
            // this so the prep+convert stages chain on the GPU (the OFA-queue stage + the final fConv wait are
            // done inside nvofa_run). Only ever invoked under single_gpu (--nvofa is single-GPU-gated), where
            // the F-thread flow lane is A.q2; the lock matches flow_submit_wait/_nowait. fence reset before submit.
            auto flow_submit_q2_chain = [&](VkCommandBuffer cmd,VkSemaphore waitSem,VkPipelineStageFlags waitStage,VkSemaphore signalSem,VkFence fence){
                if(fence!=VK_NULL_HANDLE) vkResetFences(A.dev,1,&fence);
                VkSubmitInfo si{}; si.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO; si.commandBufferCount=1; si.pCommandBuffers=&cmd;
                if(waitSem!=VK_NULL_HANDLE){ si.waitSemaphoreCount=1; si.pWaitSemaphores=&waitSem; si.pWaitDstStageMask=&waitStage; }
                if(signalSem!=VK_NULL_HANDLE){ si.signalSemaphoreCount=1; si.pSignalSemaphores=&signalSem; }
                std::lock_guard<std::mutex> lk(a_q2_mtx);
                vkQueueSubmit(A.q2,1,&si,fence);
            };
            uint64_t last_c_seen=0;
            double   last_lap_log_ms=0.0;      // STAGE-84: rate-limit for the F lap-escape log line (~1/2s)
            uint64_t prev_ingest_cseq=0;       // R1: cseq of the PREVIOUS ingest (span source)
            int live_n_f=cfg.fg_factor;        // R4: F-owned live N (auto degrades from the cap)
            // R4b: the 1.5*Tsrc set-age freshness rule FLAPPED (operator-observed, log-confirmed
            // auto:3/auto:2 at ~1Hz): set_age at publish includes the NS=2 slot WAIT — pipeline
            // depth, not N's doing — so degrading N never shrank it and the 45-set upgrade
            // re-armed the trigger. Falling-behind is now read from span>1 persistence (F
            // actually skipping sources); feasibility gets a dead band (degrade >0.90*Tsrc,
            // upgrade needs <=0.65*Tsrc sustained) + a post-change dwell. Stable N beats a
            // higher-but-oscillating N (the mode change itself reads as periodic judder).
            int up_streak=0, deg_streak=0, dwell_sets=0;
            double span_ema=0.0;
            double t_fuse_ema=0.0, t_warp_ema=0.0;   // R4: measured set-build stage times (ms)
            // STAGE-52: frame-holon fit state (F-local). gme_fit_ema = the CPU fit cost EMA (ms);
            // gme_fits = the fit count (for the first-3-fits sanity print + the startup EMA print);
            // gme_sub2 = sub-sample the grid every 2nd block once we've seen the fit cost exceed the
            // <2ms budget (measured, not assumed). gme_fit_printed latches the one-time startup print.
            double gme_fit_ema=0.0; uint64_t gme_fits=0; bool gme_sub2=false; bool gme_fit_printed=false;
            // ── IDEA-1 (gme-gpu): F-local verify-mode scratch (--gme-gpu-verify only). The CPU
            // gme_fit_affine is re-run into these on each pair and compared to the GPU model + mask;
            // gme_vfy_n latches the count for the periodic print. Zero-alloc steady state (sized once).
            std::vector<uint8_t> gme_vfy_dis;   // CPU dis-mask scratch (mvw×mvh), grown once
            uint64_t gme_vfy_n=0;
            // STAGE-55: pressure-throttle state (F-local). t_pair_ema = the full-F-pair-iteration
            // EMA (ms) — measured inline over [WAP block start … both fits done], it MEASURES the
            // whole per-pair cost the deficit lives in (not just the post-fwd-record slice that
            // t_fuse_ema covers). bwd_skipping = the hysteresis latch: enter the skip regime when
            // t_pair_ema > 0.95*pair_budget, leave it when < 0.80*pair_budget (the R4b dead-band —
            // a single threshold flaps; two with a gap between them latch). pair_budget = the
            // arrival interval = src_interval_us/1000 (= 1000/arr_rate), P's honest source cadence.
            double t_pair_ema=0.0; bool bwd_skipping=false;
            double t_flow_ema=0.0;          // STAGE-85 instrument (--fsub): EMA of the fwd fence-wait (the 1080 Ti GPU leg)
            // STAGE-84: the pressure-TIER state (F-local). pressure_tier = the current escalation level
            // (0..3), latched with hysteresis off the SAME t_pair_ema vs pair_budget signal the bwd-skip
            // latch reads (no new EMA, no new clock). holon_pair_ctr = the monotone per-CONSUMED-pair
            // counter that gates "every Nth pair" for the object/memory work (tier 2 → period 2, tier 3 →
            // period 4). Both advance only on pairs F actually ingests (the WAP block), so the period is
            // measured in real consumed pairs. All zero at warm-up → tier 0 → no escalation.
            int pressure_tier=0; uint64_t holon_pair_ctr=0;
            uint64_t tier4_dwell=0; const uint64_t kTier4DwellPairs=90;   // STAGE-87b: shed min-hold (~0.85s @107 cons/s)
            // 2026-06-28 control-word: tier5_dwell + gov_floor MOVED to the PRESENT thread (it owns the util read
            // + the dwell hysteresis). F now READS the published g_gov_floor and applies it as max(cpu_ladder,
            // floor) AFTER the tier ladder (decoupled from bwd_skipping). One decode owner, never two.
            // coin-3: the flow-input downscale. When flow_div>1, blit Bframe[a]/Bframe[b] (full-res, RO)
            // into Bflow[0]/Bflow[1] (WW_flow×WH_flow) with a LINEAR filter, then return the Bflow views to
            // feed record_optical_flow — so the level-0 match input matches the smaller MV grid (the matcher
            // reads a_view/b_view at level 0 via UV normalized to frame_w_=WW_flow; a full-res input would
            // make each tile's block-centre offsets misalign with the input texel grid). When flow_div==1
            // this lambda is NEVER called (the call sites pass Bframe directly) → byte-identical. The blit
            // runs on the SAME cmd buffer as the record (cmdF for fwd, cmdB_bwd for bwd), so it is ordered
            // before the match by the RO→TRANSFER_DST→RO barriers; one ~half-res LINEAR blit per pair.
            auto flow_downsample=[&](VkCommandBuffer cmd,int a_slot,int b_slot,VkImageView& out_a,VkImageView& out_b){
                auto blit_one=[&](Img& src,Img& dst){
                    img_barrier(cmd,src.img,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_ACCESS_SHADER_READ_BIT,VK_ACCESS_TRANSFER_READ_BIT);
                    img_barrier(cmd,dst.img,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_ACCESS_SHADER_READ_BIT,VK_ACCESS_TRANSFER_WRITE_BIT);
                    VkImageBlit bl{}; bl.srcSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1}; bl.dstSubresource=bl.srcSubresource;
                    bl.srcOffsets[1]={(int)WW,(int)WH,1}; bl.dstOffsets[1]={(int)WW_flow,(int)WH_flow,1};
                    vkCmdBlitImage(cmd,src.img,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,dst.img,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&bl,VK_FILTER_LINEAR);
                    // src (a Bframe) back to RO — the match below reads the REAL frames? No: the match reads
                    // Bflow (the dst). Bframe RO restore keeps its layout contract clean for the next pair's
                    // upload + the WAP pair-real upload (which reads hR_a, not Bframe — but the restore is the
                    // safe, layout-honest choice). dst (Bflow) → RO so the OFP samples it as a_view/b_view.
                    img_barrier(cmd,src.img,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_ACCESS_TRANSFER_READ_BIT,VK_ACCESS_SHADER_READ_BIT);
                    img_barrier(cmd,dst.img,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT);
                };
                blit_one(Bframe[a_slot],Bflow[0]); blit_one(Bframe[b_slot],Bflow[1]);
                out_a=Bflow[0].view; out_b=Bflow[1].view;
            };
            // ── STAGE-85: forward-pass pipelining state (F-local; LIVE only under cfg.fwd_pipeline && use_wap). ──
            // g_seq = the GPU-RECORD counter (one per fwd submit). It is the generation source for the
            // GPU-write side: the gen F is about to overwrite = g_seq % NS. In the SERIAL path g_seq and
            // f_seq advance in lockstep (record + publish in the same iteration) so g_seq%NS == f_seq%NS —
            // the OFF path keeps deriving f_gen from f_seq exactly as before; g_seq is only CONSULTED when
            // the pipeline is on. ON, g_seq LEADS f_seq by exactly one (pair N recorded while pair N-1 is
            // consumed/published) → the three live gens {N gpu, N-1 cpu, N-2 P} are distinct mod NS=3.
            uint64_t g_seq=0;
            // The one-pair-deferred consume context. When the pipeline is on, a recorded pair's CPU tail +
            // publish are held here and run on the NEXT iteration (while the just-recorded pair's GPU match
            // executes). Every per-pair varying value the deferred consume needs is snapshotted (it must NOT
            // read the live loop locals, which have advanced to the next pair). gen indexes the per-gen host
            // buffers (durable after fwd_fence) + the F→P publish arrays; cur_f/prv_f are informational only
            // here (the ON consume forces do_bwd off — see the ON branch — so it never reads Bframe content,
            // which the next record has already clobbered: single ofp + 2 Bframe slots cannot host a second
            // match in flight, §6.1/§6.2). fwd_fence is the no-wait submit's fence to block on at consume.
            struct FwdPend{ bool valid=false; int f_gen=0; uint64_t cur_c=0; int s=0; uint64_t span=1; int N_use=1;
                            int prv_f=0; int cur_f=0; bool have_prev_f=false; double tp0=0.0; double tf0=0.0; VkFence fwd_fence=VK_NULL_HANDLE; };
            FwdPend pend{};
            // STAGE-57: the inertia prior's CONTINUOUS per-MV-block persistence counter. F-thread-local,
            // zero-initialised ONCE here, accumulated across ALL pairs (NOT per-gen — it integrates motion
            // HISTORY over time; the per-gen hostPER bridge is only the upload vehicle). Sized mvw_f×mvh_f
            // (== the WAP MV grid). Updated right after the fwd MV+SAD copy-out is host-visible: per block
            // static_now = (|mv| ≤ 1px) && (sad_zero ≤ 8.0 sum-units) → persist = min(255, persist+16);
            // else persist = 0 (INSTANT reset on confident motion — the asymmetry by construction). Then
            // memcpy'd into hostPER[f_gen] for A to upload. Allocated only when use_inertia (cheap, idle
            // otherwise). std::vector = a plain runtime-sized zero-init heap array (no per-frame alloc).
            std::vector<uint8_t> persist;
            if(use_inertia) persist.assign((size_t)mvw_f*(size_t)mvh_f, (uint8_t)0);
            // STAGE-69b: the objdump instrument — tiny per-pair BMP grids of the F data plane (post-repair
            // dissidence mask / |MV|·16 / persist) so the supervisor can SEE the fields first-hand instead of
            // theorizing. Scratch sized once; output to frames/ (gitignored); OFF unless --objdump N.
            std::vector<uint8_t> objdump_rgba; int objdump_left=cfg.objdump_n; uint64_t objdump_idx=0;
            if(cfg.objdump_n>0){ objdump_rgba.assign((size_t)mvw_f*(size_t)mvh_f*4u,(uint8_t)0); CreateDirectoryA("frames",nullptr); }
            auto objdump_grid=[&](const char* tag, auto getv){
                const size_t n=(size_t)mvw_f*(size_t)mvh_f;
                for(size_t bi=0;bi<n;++bi){ const uint8_t v=getv(bi);
                    objdump_rgba[bi*4+0]=v; objdump_rgba[bi*4+1]=v; objdump_rgba[bi*4+2]=v; objdump_rgba[bi*4+3]=255; }
                char p[160]; std::snprintf(p,sizeof(p),"frames\\objdump_%03llu_%s.bmp",(unsigned long long)objdump_idx,tag);
                dump_bmp(p,objdump_rgba.data(),mvw_f,mvh_f);
            };
            // ── STAGE-61: the object-holon — F-thread-local state + FIXED scratch (sized ONCE here, never
            // per-pair: the kickoff forbids any heap allocation inside the per-pair path). The scratch is
            // SHARED between the fwd and bwd repairs because they run sequentially within one pair (the
            // bwd repair starts only after the fwd one finished reading/writing the scratch). The two
            // identity TABLES are separate (fwd clusters ≠ bwd clusters) and persist across pairs. All
            // sized to the full mvw_f×mvh_f grid the WAP MV field uses. Allocated only when use_objects.
            const size_t obj_nblk=(size_t)mvw_f*(size_t)mvh_f;
            std::vector<int32_t>  obj_label;       // per-block cluster label (-1 = not object / unvisited)
            std::vector<uint32_t> obj_bfs;         // BFS frontier queue (block linear indices), cap = nblk
            // Per-cluster accumulators (index = compact cluster id, capacity = nblk worst case but we only
            // ever keep the kObjSlots heaviest; sized to a generous bound so the 4-conn walk never reallocs).
            struct ObjCluster{ uint32_t mass; double cx,cy; double sx,sy; int minx,maxx,miny,maxy; double mvx,mvy; };
            std::vector<ObjCluster> obj_clusters;  // one entry per discovered component this pair
            std::vector<uint8_t>    obj_used;      // top-K selection scratch (1 byte/cluster); sized to nblk (the worst-case component count) ONCE
            // Per-row scanline fill bounds within a cluster's bbox (min/max member column per grid row).
            std::vector<int32_t> obj_rowmin, obj_rowmax;   // sized mvh_f; reset per cluster over its bbox rows
            // STAGE-63: the contour shape-field scratch — the two fixed arrays the kickoff sanctions, each
            // sized to the full mvw_f×mvh_f grid ONCE here (the STAGE-61 pattern; no per-pair heap). They are
            // bbox-scoped per cluster (only the cluster's bbox window is touched/initialized each pair, so no
            // full-grid clear is needed), and SHARED across fwd/bwd + across clusters because each cluster's
            // chamfer fully overwrites its own bbox window before the fill reads it. obj_chamf = the CHAMFER
            // DISTANCE transform (integer 3/4 chamfer to the rim; INT32_MAX = unreached/non-silhouette).
            // obj_feat = the FEATURE transform: the linear block index of the NEAREST RIM block (the rim MV is
            // read from the field hmv[obj_feat·2] at write time — the rim's MEASURED instrument, no copy).
            std::vector<int32_t>  obj_chamf;       // chamfer distance to the contour (rim) per silhouette block
            std::vector<uint32_t> obj_feat;        // nearest-rim block linear index per silhouette block (feature transform)
            // The temporal-identity slot table — ONE per anchor (fwd, bwd). id 0 = empty slot. Persists
            // across pairs (frame-holon advection lives in the centroid+mv carried here).
            struct ObjSlot{ int active; double cx,cy; double mvx,mvy; uint32_t mass; int age; int miss; };
            std::vector<ObjSlot> obj_slots_fwd, obj_slots_bwd;
            // ── STAGE-66: the scene-holon's persistent silhouette PRIOR + advection scratch. The prior is
            // the holon's BELIEF: a byte array in CUR-anchored space (the presented scene's space), in the
            // SAME 16·r quantization the dissidence masks use, PERSISTING across pairs (NOT cleared per
            // pair). mem_adv is the per-pair advected copy (prev-anchored prior pushed into THIS pair's cur
            // space by the fwd MV field). Both sized ONCE here to the full mvw_f×mvh_f grid (the STAGE-61
            // scratch discipline — zero per-pair heap). Allocated only when use_memory (idle otherwise).
            // mem_prior starts all-zero (no belief before the first confirmed pair builds it).
            std::vector<uint8_t> mem_prior;   // persistent CUR-anchored silhouette belief (16·r bytes; survives pairs)
            std::vector<uint8_t> mem_adv;     // per-pair advected prior (cur-anchored = THIS pair's prev-prior pushed fwd)
            // ── STAGE-68: the wake-evaporation match scratch — "the object takes its silhouette with it".
            // mem_refresh runs in CUR-anchored space; the BWD repair is the LAST writer of obj_label, so at
            // refresh time obj_label holds THIS pair's BWD (cur-anchored) cluster labels — exactly the
            // geometry mem_prior/mem_adv live in. To expire the trailing wake we need, per ARMED bwd
            // cluster: the slot MV (sweep direction), the cluster id (= the label value in obj_label, which
            // is keep_idx[k]), and the bbox. We record THAT here (a fixed [kObjSlots] table, ZERO per-pair
            // heap — the STAGE-61 scratch discipline) during the BWD object_repair pass only; the per-row
            // fill bounds are RE-DERIVED from obj_label inside mem_refresh (the kickoff's "re-derive, don't
            // copy the rows" path). Fwd repair passes wake_out=null → records nothing.
            struct WakeRec{ int32_t cid; int minx,maxx,miny,maxy; double mvx,mvy; };
            std::vector<WakeRec> wake_rec;    // ARMED bwd clusters this pair (cap kObjSlots; fixed, sized once)
            int wake_n=0;                     // count valid entries in wake_rec this pair (reset by the bwd repair)
            if(use_objects){
                obj_label.assign(obj_nblk,-1);
                obj_bfs.assign(obj_nblk,0u);
                obj_clusters.reserve(obj_nblk);            // capacity = worst-case component count; never reallocs in-loop
                obj_used.assign(obj_nblk,(uint8_t)0);      // one byte per possible cluster; sized once (no per-pair alloc)
                obj_rowmin.assign((size_t)mvh_f,0);
                obj_rowmax.assign((size_t)mvh_f,0);
                obj_chamf.assign(obj_nblk,INT32_MAX);      // STAGE-63: chamfer distance scratch (full grid, sized once)
                obj_feat.assign(obj_nblk,0u);              // STAGE-63: nearest-rim feature scratch (full grid, sized once)
                obj_slots_fwd.assign((size_t)kObjSlots, ObjSlot{0,0,0,0,0,0,0,0});
                obj_slots_bwd.assign((size_t)kObjSlots, ObjSlot{0,0,0,0,0,0,0,0});
            }
            if(use_memory){
                mem_prior.assign(obj_nblk,(uint8_t)0);     // STAGE-66: silhouette prior (full grid, sized once; zero belief at start)
                mem_adv.assign(obj_nblk,(uint8_t)0);       // STAGE-66: advection scratch (full grid, sized once)
                wake_rec.assign((size_t)kObjSlots, WakeRec{-1,0,0,0,0,0.0,0.0});   // STAGE-68: armed-bwd-cluster table (fixed, sized once)
            }
            // The object-holon's own F-side cost EMA (cluster + identity + repair + dissidence recompute),
            // measured per pair across BOTH anchors, printed ONCE after settle if >0.3ms (mirrors the gme
            // fit-cost startup print). obj_settle_printed latches the one-time print; obj_pairs counts the
            // pairs the holon actually ran (warm-up guard, like gme_fits).
            double obj_cost_ema=0.0; uint64_t obj_pairs=0; bool obj_settle_printed=false;
            // ── STAGE-61: the repair routine. Runs the full object-holon for ONE anchor: connected-component
            // cluster the dissidence mask (dis byte > matte_thresh·255 — the EXACT shader OBJECT test the
            // matte uses), keep the kObjSlots heaviest clusters with mass ≥ kObjMinMass, match them to the
            // persistent slot table by advected centroid, then for every tracked object with |mv_obj| ≥
            // kObjInhMin: scanline-fill its silhouette and OVERWRITE near-static (|mv_block| ≤ kObjStaticMax)
            // interior blocks' MV with mv_obj (the cancellation-class kill) — EXEMPTING the HUD shield
            // (persist[]≥kObjPersistShield). Repaired blocks' dissidence bytes are RECOMPUTED against the
            // SAME affine model (the min(255,16·r) quantization the mask fill uses) so mask+field ship
            // consistent. Writes mv_half back into the RG16F field via float_to_half. Returns the repaired
            // block count via *out_rep and the in-fill (silhouette-interior) count via *out_infill; the live
            // tracked-slot count via *out_live. Pure CPU, single-threaded F, NO heap (all scratch above).
            //   mv_field  : the RG16F MV grid for this anchor (hostMV or hostMVB) — read AND written
            //   dis_mask  : the uint8 dissidence bytes for this anchor (hostDIS or hostDISB) — read AND rewritten
            //   model6    : the 6 affine params {a,b,c,d,e,f} the gme fit produced for THIS field
            //   slots     : the persistent identity table for THIS anchor (fwd or bwd)
            //   persist_p : the inertia persist[] array (null when use_inertia off → no shield, treated as 0)
            //   adv_sign  : the identity-advection sign (supervisor fix). The FWD field maps prev→cur (mv ≈
            //               +v for an object moving +v) → +1. The BWD field is recorded with SWAPPED views
            //               (cur→prev — see the do_bwd record: mv ≈ −v) → its centroids still move +v across
            //               pairs, so the advection must NEGATE the slot mv → −1. Only the table prediction
            //               uses the sign; the inherited mv written to the field stays in the anchor's own
            //               convention (the cluster mean IS that convention).
            // STAGE-68: wake_out/wake_n_out — when non-null (the BWD pass only), the repair records each
            // ARMED kept cluster's {cid, bbox, owner-slot mv} into the fixed wake_out[kObjSlots] table for
            // mem_refresh's wake evaporation. null (the FWD pass) → records nothing (byte-identical to pre-68).
            //   persist_mut: STAGE-69 (supervisor-built, "membership beats inertia") — non-null ONLY on the
            //                fwd call (persist[] is screen/prev-anchored) when use_inertia && cfg.persist_reset.
            //                Inside an ARMED cluster's filled silhouette every visited block's persistence is
            //                RESET: a block inside a tracked moving object is NOT a HUD — its static look is
            //                the FLAT-INTERIOR ILLUSION (immeasurable MV on flat content: |mv|≈0 AND sad_zero≈0,
            //                indistinguishable from a HUD by the STAGE-57 counter; an interior block stays
            //                inside a big object for 8+ pairs → persist latched → the shield blocked the
            //                inheritance → the interior stayed pinned at MV=0 → the double-image trail/bites
            //                carried since the beginning; the operator's rep:0-4% was exactly the fresh entry
            //                band ~15px/120px). The reset (a) unblocks the inheritance below and (b) releases
            //                the shader's inertia gates next pair (hostPER uploads persist[]). Real HUDs never
            //                sit inside an ARMED cluster (a mover passing over resets them transiently — the
            //                accepted trade, they re-latch in ~8 pairs).
            auto object_repair=[&](void* mv_field, uint8_t* dis_mask, const float model6[6],
                                   std::vector<ObjSlot>& slots, const uint8_t* persist_p, uint8_t* persist_mut,
                                   uint64_t span_local, double adv_sign,
                                   uint32_t* out_live, uint32_t* out_rep, uint32_t* out_infill,
                                   WakeRec* wake_out, int* wake_n_out){
                const uint32_t MW=mvw_f, MH=mvh_f;
                const float thr_byte=cfg.matte_thresh*255.0f;   // the EXACT shader OBJECT cutoff (mirror of matte_mass_count)
                uint16_t* hmv=(uint16_t*)mv_field;              // interleaved R(mv_x),G(mv_y) per block
                // (1) connected components — 4-conn BFS over OBJECT blocks (dis>thr_byte). No heap in the
                // loop: obj_label / obj_bfs / obj_clusters are the fixed scratch. Reset labels to -1 each pair.
                std::fill(obj_label.begin(),obj_label.end(),-1);
                obj_clusters.clear();
                for(uint32_t gy=0; gy<MH; ++gy){
                    for(uint32_t gx=0; gx<MW; ++gx){
                        const size_t i=(size_t)gy*MW+gx;
                        if(obj_label[i]!=-1) continue;                       // already labelled
                        if((float)dis_mask[i] <= thr_byte) continue;        // not an OBJECT block
                        // new component — BFS flood, accumulating mass / centroid / bbox / member-MV mean.
                        const int32_t cid=(int32_t)obj_clusters.size();
                        ObjCluster cl{0,0,0,0,0,(int)gx,(int)gx,(int)gy,(int)gy,0,0};
                        size_t head=0,tail=0;
                        obj_bfs[tail++]=(uint32_t)i; obj_label[i]=cid;
                        while(head<tail){
                            const uint32_t bi=obj_bfs[head++];
                            const uint32_t by=bi/MW, bx=bi%MW;
                            const float mvx=half_to_float(hmv[(size_t)bi*2u+0u]);
                            const float mvy=half_to_float(hmv[(size_t)bi*2u+1u]);
                            ++cl.mass; cl.sx+=bx; cl.sy+=by; cl.mvx+=mvx; cl.mvy+=mvy;
                            if((int)bx<cl.minx)cl.minx=(int)bx; if((int)bx>cl.maxx)cl.maxx=(int)bx;
                            if((int)by<cl.miny)cl.miny=(int)by; if((int)by>cl.maxy)cl.maxy=(int)by;
                            // 4-connected neighbours
                            if(bx>0){      const size_t n=bi-1;  if(obj_label[n]==-1 && (float)dis_mask[n]>thr_byte){ obj_label[n]=cid; obj_bfs[tail++]=(uint32_t)n; } }
                            if(bx+1<MW){   const size_t n=bi+1;  if(obj_label[n]==-1 && (float)dis_mask[n]>thr_byte){ obj_label[n]=cid; obj_bfs[tail++]=(uint32_t)n; } }
                            if(by>0){      const size_t n=bi-MW; if(obj_label[n]==-1 && (float)dis_mask[n]>thr_byte){ obj_label[n]=cid; obj_bfs[tail++]=(uint32_t)n; } }
                            if(by+1<MH){   const size_t n=bi+MW; if(obj_label[n]==-1 && (float)dis_mask[n]>thr_byte){ obj_label[n]=cid; obj_bfs[tail++]=(uint32_t)n; } }
                        }
                        if(cl.mass>0){ cl.cx=cl.sx/(double)cl.mass; cl.cy=cl.sy/(double)cl.mass; cl.mvx/=(double)cl.mass; cl.mvy/=(double)cl.mass; }
                        obj_clusters.push_back(cl);
                    }
                }
                // (1b) keep the top-K=kObjSlots clusters by mass with mass ≥ kObjMinMass. We don't sort the
                // whole vector (could be large) — we select the K heaviest indices by repeated max-scan into
                // a fixed-size index array (K is 16; K passes over N clusters, cheap vs the BFS).
                int keep_idx[kObjSlots]; int keep_n=0;
                {
                    // mark-and-pick into the fixed obj_used scratch (no heap in the loop). For K≤16 and the
                    // typically small cluster count, K linear scans are negligible vs the BFS.
                    if(!obj_clusters.empty()) std::fill(obj_used.begin(),obj_used.begin()+(std::ptrdiff_t)obj_clusters.size(),(uint8_t)0);
                    for(int k=0;k<kObjSlots;++k){
                        int best=-1; uint32_t bestmass=0;
                        for(size_t c=0;c<obj_clusters.size();++c){
                            if(obj_used[c]) continue;
                            if(obj_clusters[c].mass<(uint32_t)kObjMinMass) continue;
                            if(obj_clusters[c].mass>bestmass){ bestmass=obj_clusters[c].mass; best=(int)c; }
                        }
                        if(best<0) break;
                        obj_used[best]=1; keep_idx[keep_n++]=best;
                    }
                }
                // (2) temporal identity — advect each slot's centroid by its mv over this pair's span
                // (prev centroid + prev mv·span/8.0 in block units; mv is px, the grid is 8px/block, so
                // px/8 = block units), then match each kept cluster to the nearest unmatched slot within
                // kObjMatchRadius. Unmatched cluster → new slot (evict the oldest-missed). Unmatched slot
                // → miss++, retire after kObjMaxMiss. EMA the matched slot's mv (kObjMvEma) for stability.
                const double adv=adv_sign*(double)span_local/8.0;   // block-units advection per px of mv (signed per anchor)
                double pred_cx[kObjSlots], pred_cy[kObjSlots]; int slot_taken[kObjSlots];
                for(int s2=0;s2<kObjSlots;++s2){
                    slot_taken[s2]=0;
                    pred_cx[s2]=slots[s2].cx + slots[s2].mvx*adv;
                    pred_cy[s2]=slots[s2].cy + slots[s2].mvy*adv;
                }
                int cl_slot[kObjSlots];               // matched slot per kept cluster (-1 = none)
                for(int k=0;k<keep_n;++k){
                    const ObjCluster& cl=obj_clusters[keep_idx[k]];
                    int best=-1; double bestd=kObjMatchRadius*kObjMatchRadius;
                    for(int s2=0;s2<kObjSlots;++s2){
                        if(!slots[s2].active || slot_taken[s2]) continue;
                        const double dx=cl.cx-pred_cx[s2], dy=cl.cy-pred_cy[s2];
                        const double d2=dx*dx+dy*dy;
                        if(d2<bestd){ bestd=d2; best=s2; }
                    }
                    cl_slot[k]=best;
                    if(best>=0) slot_taken[best]=1;
                }
                // apply matches / spawns. cl_slot[k] is UPDATED to the owner slot for every kept cluster
                // (matched or freshly spawned) so the repair loop below reads it directly — no float-
                // equality refind.
                for(int k=0;k<keep_n;++k){
                    const ObjCluster& cl=obj_clusters[keep_idx[k]];
                    int s2=cl_slot[k];
                    if(s2<0){
                        // spawn — find a free slot, else evict the active slot with the highest miss count.
                        s2=-1; for(int t=0;t<kObjSlots;++t){ if(!slots[t].active){ s2=t; break; } }
                        if(s2<0){ int worst=0; for(int t=1;t<kObjSlots;++t) if(slots[t].miss>slots[worst].miss) worst=t; s2=worst; }
                        slots[s2].active=1; slots[s2].cx=cl.cx; slots[s2].cy=cl.cy;
                        slots[s2].mvx=cl.mvx; slots[s2].mvy=cl.mvy; slots[s2].age=0; slots[s2].miss=0;
                        slots[s2].mass=cl.mass; slot_taken[s2]=1;
                        cl_slot[k]=s2;          // record the spawned owner
                    } else {
                        slots[s2].cx=cl.cx; slots[s2].cy=cl.cy;
                        // STAGE-65 (the operator's stigmergy-expiration principle): the slot MV memory EXPIRES
                        // on CONTRADICTION instead of decaying through it. The plain EMA made the holon inherit
                        // a STALE direction for ~1-2 pairs after every bounce (the sign flip) — the operator's
                        // "the flags still hallucinate at the bounce" residual. Innovation gate: a direction
                        // REVERSAL (negative dot) or a large delta (> kObjMvExpirePx) means the old evidence is
                        // INVALID, not noisy → adopt the fresh measurement outright (the pheromone evaporates).
                        // Otherwise the EMA keeps smoothing measurement noise exactly as before. The rule:
                        // EMA for noise, expiration for contradiction. --no-expire reverts to the pure EMA.
                        const double exp_dot = slots[s2].mvx*cl.mvx + slots[s2].mvy*cl.mvy;
                        const double exp_dx  = cl.mvx-slots[s2].mvx, exp_dy = cl.mvy-slots[s2].mvy;
                        const bool contradicted = (exp_dot < 0.0)
                            || (exp_dx*exp_dx+exp_dy*exp_dy > (double)kObjMvExpirePx*(double)kObjMvExpirePx);
                        if(cfg.expire && contradicted){ slots[s2].mvx=cl.mvx; slots[s2].mvy=cl.mvy; }
                        else { slots[s2].mvx=slots[s2].mvx*(1.0-kObjMvEma)+cl.mvx*kObjMvEma;
                               slots[s2].mvy=slots[s2].mvy*(1.0-kObjMvEma)+cl.mvy*kObjMvEma; }
                        slots[s2].mass=cl.mass; slots[s2].miss=0; ++slots[s2].age;
                    }
                }
                // unmatched slots → miss++, retire after kObjMaxMiss
                uint32_t live=0;
                for(int s2=0;s2<kObjSlots;++s2){
                    if(!slots[s2].active) continue;
                    if(!slot_taken[s2]){ if(++slots[s2].miss>=kObjMaxMiss){ slots[s2].active=0; continue; } }
                    ++live;
                }
                // (3) the INHERITANCE repair — per kept cluster whose MATCHED slot moves ≥ kObjInhMin (rigid
                // path) OR whose rim MV field is alive (shape-field path), scanline-fill the silhouette (per-row
                // min..max member column within the bbox) and overwrite near-static interior blocks. The repair
                // uses the SLOT mv (the EMA-smoothed, temporally-stable object velocity), not the raw cluster
                // mean. Only blocks BELONGING to this cluster's label form the silhouette (the scanline runs over
                // label==cid blocks per row, the vessel filled row-wise).
                // STAGE-63: cfg.shapefield (the default) replaces the rigid single-MV write with the contour
                // distance+feature transform + rim-sector inheritance; --no-shapefield reverts to the STAGE-61
                // rigid path (the else-arm below — PRESERVED verbatim, not re-derived).
                const bool shapefield_on=cfg.shapefield;
                uint32_t infill_total=0, rep_total=0;
                int wake_nrec=0;   // STAGE-68: count of armed clusters recorded into wake_out this pass (bwd only)
                for(int k=0;k<keep_n;++k){
                    const int owner=cl_slot[k];   // owner slot recorded in the apply loop (match or spawn)
                    if(owner<0) continue;
                    const double omvx=slots[owner].mvx, omvy=slots[owner].mvy;
                    const double omv=std::sqrt(omvx*omvx+omvy*omvy);
                    const int32_t cid=keep_idx[k];
                    const ObjCluster& cl=obj_clusters[keep_idx[k]];
                  if(!shapefield_on){
                    // ── STAGE-61 RIGID PATH (the --no-shapefield else-arm — the clean A/B, byte-identical) ──
                    if(omv<(double)kObjInhMin) continue;                 // object not moving → no inheritance
                    // STAGE-68: armed (rigid arm = omv≥kObjInhMin here) → record for the wake evaporation.
                    if(wake_out && wake_nrec<kObjSlots){ wake_out[wake_nrec++]=WakeRec{cid,cl.minx,cl.maxx,cl.miny,cl.maxy,omvx,omvy}; }
                    // scanline fill bounds over the bbox rows: min/max member COLUMN (label==cid) per row.
                    for(int ry=cl.miny; ry<=cl.maxy; ++ry){ obj_rowmin[ry]=INT32_MAX; obj_rowmax[ry]=INT32_MIN; }
                    for(int ry=cl.miny; ry<=cl.maxy; ++ry){
                        const size_t base=(size_t)ry*MW;
                        for(int rx=cl.minx; rx<=cl.maxx; ++rx){
                            if(obj_label[base+rx]==cid){ if(rx<obj_rowmin[ry])obj_rowmin[ry]=rx; if(rx>obj_rowmax[ry])obj_rowmax[ry]=rx; }
                        }
                    }
                    const uint16_t mvx_h=float_to_half((float)omvx), mvy_h=float_to_half((float)omvy);
                    for(int ry=cl.miny; ry<=cl.maxy; ++ry){
                        if(obj_rowmin[ry]>obj_rowmax[ry]) continue;       // no member on this row
                        const size_t base=(size_t)ry*MW;
                        for(int rx=obj_rowmin[ry]; rx<=obj_rowmax[ry]; ++rx){
                            const size_t bi=base+rx;
                            ++infill_total;                               // a silhouette-interior (filled) block
                            // STAGE-69: MEMBERSHIP BEATS INERTIA — inside this ARMED silhouette the block is
                            // a mover's interior, not a HUD (the flat-interior illusion); reset its persistence
                            // so the shield below never blocks it and the shader's inertia gates release.
                            if(persist_mut) persist_mut[bi]=0;
                            // HUD shield: a long-static-history block is exempt — UNLESS STAGE-69 just reset it
                            // (armed membership wins; the shield still guards the bwd call + --no-persist-reset).
                            if(persist_p && persist_p[bi]>=(uint8_t)kObjPersistShield) continue;
                            const float bmvx=half_to_float(hmv[bi*2u+0u]);
                            const float bmvy=half_to_float(hmv[bi*2u+1u]);
                            // STAGE-88 (--obj-fill-rim, --no-shapefield rigid arm): flag OFF -> !cfg.obj_fill_rim==true
                            // -> the original near-static gate EXACTLY (byte-identical). ON -> the whole rigid silhouette
                            // inherits the slot MV (this arm already writes mvx_h/mvy_h everywhere it does not continue).
                            if(!cfg.obj_fill_rim && std::sqrt(bmvx*bmvx+bmvy*bmvy) > (double)kObjStaticMax) continue;  // not near-static → leave it
                            // the cancellation signature: static interior inside a moving silhouette → inherit.
                            hmv[bi*2u+0u]=mvx_h; hmv[bi*2u+1u]=mvy_h;
                            // (4) consistency re-walk: recompute THIS block's dissidence byte against the SAME
                            // affine model (r = |mv_rep − model(gx,gy)|, byte = min(255, round(16·r))) so the
                            // mask the matte/mass-count read stays consistent with the field we just wrote.
                            const double gx=(double)rx, gy=(double)ry;
                            const double rxr=omvx-((double)model6[0]+model6[1]*gx+model6[2]*gy);
                            const double ryr=omvy-((double)model6[3]+model6[4]*gx+model6[5]*gy);
                            const double r=std::sqrt(rxr*rxr+ryr*ryr);
                            int q=(int)(16.0*r+0.5); if(q>255) q=255;
                            dis_mask[bi]=(uint8_t)q;
                            ++rep_total;
                        }
                    }
                    continue;
                  }
                    // ── STAGE-63 SHAPE-FIELD PATH (default) ───────────────────────────────────────────────
                    // (3a) scanline fill bounds over the bbox rows: min/max member COLUMN (label==cid) per row.
                    // This defines the FILLED silhouette (the chamfer domain) exactly as the rigid path fills.
                    for(int ry=cl.miny; ry<=cl.maxy; ++ry){ obj_rowmin[ry]=INT32_MAX; obj_rowmax[ry]=INT32_MIN; }
                    for(int ry=cl.miny; ry<=cl.maxy; ++ry){
                        const size_t base=(size_t)ry*MW;
                        for(int rx=cl.minx; rx<=cl.maxx; ++rx){
                            if(obj_label[base+rx]==cid){ if(rx<obj_rowmin[ry])obj_rowmin[ry]=rx; if(rx>obj_rowmax[ry])obj_rowmax[ry]=rx; }
                        }
                    }
                    // in-silhouette test (a block is in the filled vessel iff its column ∈ its row's fill bounds).
                    auto in_sil=[&](int rx,int ry)->bool{
                        if(ry<cl.miny||ry>cl.maxy) return false;
                        if(obj_rowmin[ry]>obj_rowmax[ry]) return false;
                        return rx>=obj_rowmin[ry] && rx<=obj_rowmax[ry];
                    };
                    // (3b) rim extraction + rim MV SPREAD. A member (label==cid) block is RIM iff any 4-neighbour
                    // is non-member (label!=cid, OR off-grid → the grid edge counts as non-member). Initialize the
                    // chamfer/feature scratch over the bbox window: rim → dist 0, feat=self; other silhouette →
                    // INT32_MAX. We also SUBSAMPLE up to kObjRimSpreadSamples rim blocks (stride) to measure the
                    // rim MV spread = max pairwise |mv_rim_i − mv_rim_j| (bounded O(32²)) for the generalized arm.
                    float rim_mx[kObjRimSpreadSamples], rim_my[kObjRimSpreadSamples]; int rim_ns=0;
                    uint32_t rim_count=0;
                    for(int ry=cl.miny; ry<=cl.maxy; ++ry){
                        if(obj_rowmin[ry]>obj_rowmax[ry]) continue;
                        const size_t base=(size_t)ry*MW;
                        for(int rx=obj_rowmin[ry]; rx<=obj_rowmax[ry]; ++rx){
                            const size_t bi=base+rx;
                            obj_chamf[bi]=INT32_MAX;                       // default: silhouette but not yet reached
                            if(obj_label[bi]!=cid) continue;              // only label-member blocks can be rim
                            const bool wEdge=(rx==0)             || obj_label[bi-1]!=cid;
                            const bool eEdge=((uint32_t)rx+1>=MW) || obj_label[bi+1]!=cid;
                            const bool nEdge=(ry==0)             || obj_label[bi-MW]!=cid;
                            const bool sEdge=((uint32_t)ry+1>=MH) || obj_label[bi+MW]!=cid;
                            if(wEdge||eEdge||nEdge||sEdge){
                                obj_chamf[bi]=0; obj_feat[bi]=(uint32_t)bi;   // rim: distance 0, feature = self
                                ++rim_count;
                            }
                        }
                    }
                    // subsample the rim MVs for the spread scan (deterministic stride over the bbox-scan order).
                    {
                        const uint32_t stride = rim_count>(uint32_t)kObjRimSpreadSamples
                                              ? (rim_count/(uint32_t)kObjRimSpreadSamples) : 1u;
                        uint32_t seen=0;
                        for(int ry=cl.miny; ry<=cl.maxy && rim_ns<kObjRimSpreadSamples; ++ry){
                            if(obj_rowmin[ry]>obj_rowmax[ry]) continue;
                            const size_t base=(size_t)ry*MW;
                            for(int rx=obj_rowmin[ry]; rx<=obj_rowmax[ry] && rim_ns<kObjRimSpreadSamples; ++rx){
                                const size_t bi=base+rx;
                                if(obj_chamf[bi]!=0) continue;            // not a rim block
                                if((seen++ % stride)!=0u) continue;       // stride subsample
                                rim_mx[rim_ns]=half_to_float(hmv[bi*2u+0u]);
                                rim_my[rim_ns]=half_to_float(hmv[bi*2u+1u]);
                                ++rim_ns;
                            }
                        }
                    }
                    double rim_spread=0.0;
                    for(int a=0;a<rim_ns;++a) for(int b=a+1;b<rim_ns;++b){
                        const double dx=(double)rim_mx[a]-rim_mx[b], dy=(double)rim_my[a]-rim_my[b];
                        const double d=std::sqrt(dx*dx+dy*dy); if(d>rim_spread) rim_spread=d;
                    }
                    // (3c) GENERALIZED ARMING: a rigid mean ≥ kObjInhMin OR a live rim field (spread ≥ min) — a
                    // scaling/rotating object can have mean mv ≈ 0 yet a live rim. Which arm fired is NOT in stats.
                    const bool armed = (omv>=(double)kObjInhMin) || (rim_spread>=(double)kObjRimSpreadMin);
                    if(!armed) continue;
                    // STAGE-88 (--obj-fill-rim): a RIGID cluster (rim field already AGREES: rim_spread<min) may stamp
                    // its single slot MV across the WHOLE silhouette (incl. the protected rim band + the >static
                    // annulus) so the disc warps as one surface. Short-circuits on cfg.obj_fill_rim -> zero work when
                    // off. Stands down on a scaling/rotating/articulated rim (the graded chamfer blend rules there).
                    const bool rigid_fill = cfg.obj_fill_rim && (rim_spread < (double)kObjRimSpreadMin);
                    // STAGE-68: armed → record for the wake evaporation (the trailing sweep in mem_refresh
                    // uses the slot mv; a rim-only-armed cluster with omv≈0 yields a ~1-block sweep — harmless).
                    if(wake_out && wake_nrec<kObjSlots){ wake_out[wake_nrec++]=WakeRec{cid,cl.minx,cl.maxx,cl.miny,cl.maxy,omvx,omvy}; }
                    // (3d) CHAMFER distance + FEATURE transform (2-pass, bbox-bounded) over the filled silhouette.
                    // Integer chamfer weights 3 (orthogonal) / 4 (diagonal). Forward pass TL→BR relaxes from the
                    // already-swept W/N/NW/NE neighbours; backward pass BR→TL from E/S/SE/SW. When a neighbour
                    // relaxes the distance we COPY its nearest-rim feature (the feature transform). Only
                    // in-silhouette neighbours with a finite distance contribute.
                    auto relax=[&](size_t bi,int nrx,int nry,int w){
                        if(!in_sil(nrx,nry)) return;
                        const size_t ni=(size_t)nry*MW+nrx;
                        const int32_t nd=obj_chamf[ni];
                        if(nd==INT32_MAX) return;
                        const int32_t cand=nd+w;
                        if(cand<obj_chamf[bi]){ obj_chamf[bi]=cand; obj_feat[bi]=obj_feat[ni]; }
                    };
                    for(int ry=cl.miny; ry<=cl.maxy; ++ry){               // forward pass TL→BR
                        if(obj_rowmin[ry]>obj_rowmax[ry]) continue;
                        const size_t base=(size_t)ry*MW;
                        for(int rx=obj_rowmin[ry]; rx<=obj_rowmax[ry]; ++rx){
                            const size_t bi=base+rx;
                            if(obj_chamf[bi]==0) continue;                // rim seed — never relaxed
                            relax(bi,rx-1,ry,  kChamfOrtho);             // W
                            relax(bi,rx,  ry-1,kChamfOrtho);             // N
                            relax(bi,rx-1,ry-1,kChamfDiag);              // NW
                            relax(bi,rx+1,ry-1,kChamfDiag);              // NE
                        }
                    }
                    for(int ry=cl.maxy; ry>=cl.miny; --ry){              // backward pass BR→TL
                        if(obj_rowmin[ry]>obj_rowmax[ry]) continue;
                        const size_t base=(size_t)ry*MW;
                        for(int rx=obj_rowmax[ry]; rx>=obj_rowmin[ry]; --rx){
                            const size_t bi=base+rx;
                            if(obj_chamf[bi]==0) continue;                // rim seed — never relaxed
                            relax(bi,rx+1,ry,  kChamfOrtho);             // E
                            relax(bi,rx,  ry+1,kChamfOrtho);             // S
                            relax(bi,rx+1,ry+1,kChamfDiag);              // SE
                            relax(bi,rx-1,ry+1,kChamfDiag);              // SW
                        }
                    }
                    // (3e) RIM-SECTOR INHERITANCE fill with the DEPTH GATE. Only strictly-interior blocks
                    // (chamfer dist ≥ kObjShapeDepthMin) are eligible — the rim band itself is NEVER rewritten
                    // (the operator's stable contour is the instrument). The inherited MV is
                    // mix(nearest-rim MV, slot MV, w), w = clamp(dist/kChamfWMax, 0, 1): near the contour the
                    // nearest rim sector wins (rotation/scaling correct), deep interior relaxes to the slot mean.
                    for(int ry=cl.miny; ry<=cl.maxy; ++ry){
                        if(obj_rowmin[ry]>obj_rowmax[ry]) continue;       // no member on this row
                        const size_t base=(size_t)ry*MW;
                        for(int rx=obj_rowmin[ry]; rx<=obj_rowmax[ry]; ++rx){
                            const size_t bi=base+rx;
                            ++infill_total;                               // a silhouette-interior (filled) block
                            const int32_t d=obj_chamf[bi];
                            // STAGE-88 (--obj-fill-rim): under rigid_fill the rim band + unreached blocks DO inherit
                            // the slot MV (they are part of the one rigid surface). Flag off -> !rigid_fill==true ->
                            // the original gates EXACTLY.
                            if(!rigid_fill && d==INT32_MAX) continue;     // unreached silhouette block — cannot inherit
                            if(!rigid_fill && d<kObjShapeDepthMin) continue; // DEPTH GATE: rim band protected, never rewritten
                            // STAGE-69: MEMBERSHIP BEATS INERTIA — reset the mover-interior persistence (see the
                            // rigid arm + the lambda doc; the flat-interior illusion is not a HUD).
                            if(persist_mut) persist_mut[bi]=0;
                            // HUD shield: exempt — UNLESS STAGE-69 just reset it (armed membership wins).
                            if(persist_p && persist_p[bi]>=(uint8_t)kObjPersistShield) continue;
                            const float bmvx=half_to_float(hmv[bi*2u+0u]);
                            const float bmvy=half_to_float(hmv[bi*2u+1u]);
                            // STAGE-88 (--obj-fill-rim): under rigid_fill a >static "spurious" block (aperture-problem
                            // flow) is REWRITTEN to the slot MV instead of left as garbage. Flag off -> original gate.
                            if(!rigid_fill && std::sqrt(bmvx*bmvx+bmvy*bmvy) > (double)kObjStaticMax) continue;  // not near-static → leave it
                            // rim-sector inheritance: mix(nearest-rim MV, slot MV, w). The nearest-rim MV is the
                            // MEASURED rim instrument read straight from the field (feature transform payload).
                            double imvx, imvy;
                            if(rigid_fill){ imvx=omvx; imvy=omvy; }       // STAGE-88: stamp the SINGLE slot MV (no
                                                                          // obj_feat read -> safe for unreached/rim
                                                                          // blocks AND corrects a wrong rim block).
                            else {                                        // original graded rim-sector inheritance:
                                const uint32_t fb=obj_feat[bi];
                                const double rmvx=half_to_float(hmv[(size_t)fb*2u+0u]);
                                const double rmvy=half_to_float(hmv[(size_t)fb*2u+1u]);
                                double w=(double)d/(double)kChamfWMax; if(w<0.0)w=0.0; else if(w>1.0)w=1.0;
                                imvx=rmvx*(1.0-w)+omvx*w;                 // near rim → rim sector; deep → slot mean
                                imvy=rmvy*(1.0-w)+omvy*w;
                            }
                            hmv[bi*2u+0u]=float_to_half((float)imvx); hmv[bi*2u+1u]=float_to_half((float)imvy);
                            // (4) consistency re-walk: recompute THIS block's dissidence byte against the SAME
                            // affine model with the INHERITED (per-block) MV so mask+field ship consistent.
                            const double gx=(double)rx, gy=(double)ry;
                            const double rxr=imvx-((double)model6[0]+model6[1]*gx+model6[2]*gy);
                            const double ryr=imvy-((double)model6[3]+model6[4]*gx+model6[5]*gy);
                            const double r=std::sqrt(rxr*rxr+ryr*ryr);
                            int q=(int)(16.0*r+0.5); if(q>255) q=255;
                            dis_mask[bi]=(uint8_t)q;
                            ++rep_total;
                        }
                    }
                }
                if(out_live)   *out_live=live;
                if(out_rep)    *out_rep=rep_total;
                if(out_infill) *out_infill=infill_total;
                if(wake_n_out) *wake_n_out=wake_nrec;   // STAGE-68: armed-cluster count (bwd pass only; 0 when wake_out null)
            };
            // ── STAGE-66: the scene-holon's three operations — ADVECT, MERGE, REFRESH. All pure CPU,
            // single-threaded F, zero per-pair heap (mem_prior/mem_adv are the fixed scratch above). They
            // run only when use_memory, gated at the call sites. The half-float MV decode they need is the
            // SAME half_to_float the repair uses (no new codec — the kickoff's reuse mandate).
            //
            // (A) ADVECT — push the previous pair's CUR-anchored prior into THIS pair's CUR space. The
            // previous pair's cur space IS this pair's prev space, and the FWD MV field (hostMV: A→B,
            // prev→cur) maps prev→cur. Nearest-block GATHER with the DESTINATION block's own MV (valid for
            // smooth fields at block scale): for destination block b, src = round(b − mv(b)/8.0) (mv is the
            // full per-pair displacement in PX, /8 converts px→block units; NO span factor — the field
            // already spans the pair). out-of-grid or unmapped → 0. Reads mem_prior (prev-anchored, the
            // belief carried from last pair), writes mem_adv (cur-anchored). mem_prior is left UNTOUCHED so
            // the fwd merge can still read it pre-advection (the prev-anchored merge the kickoff pins).
            auto mem_advect=[&](const void* fwd_field){
                const uint32_t MW=mvw_f, MH=mvh_f;
                const uint16_t* hmv=(const uint16_t*)fwd_field;   // fwd field: R(mv_x),G(mv_y) px, prev→cur
                for(uint32_t by=0; by<MH; ++by){
                    for(uint32_t bx=0; bx<MW; ++bx){
                        const size_t bi=(size_t)by*MW+bx;
                        const float mvx=half_to_float(hmv[bi*2u+0u]);
                        const float mvy=half_to_float(hmv[bi*2u+1u]);
                        // src block = where THIS cur block came FROM in prev space (gather, dst-MV).
                        const long sx=std::lround((double)bx-(double)mvx/8.0);
                        const long sy=std::lround((double)by-(double)mvy/8.0);
                        if(sx<0||sy<0||(uint32_t)sx>=MW||(uint32_t)sy>=MH){ mem_adv[bi]=0; continue; }
                        mem_adv[bi]=mem_prior[(size_t)sy*MW+(size_t)sx];   // unmapped src defaults to its 0 byte
                    }
                }
            };
            // (B) MERGE — fold a silhouette prior into ONE anchor's fresh dissidence mask BEFORE clustering,
            // applying the STAGE-65 EXPIRATION rule per block first. prior_src is mem_prior for the fwd
            // (prev-anchored) merge and mem_adv for the bwd (cur-anchored) merge — the kickoff's pinned
            // anchor pairing (the fwd mask lives in prev space = the prior's own un-advected space; the bwd
            // mask lives in cur space = the advected prior's space). The anchor's OWN field supplies the
            // |mv| the expiration test reads (hostMV for fwd, hostMVB for bwd). Per block:
            //   fresh f = dis_mask[b]; thr_byte = matte_thresh·255 = the EXACT shader/cluster OBJECT cutoff.
            //   EXPIRATION: fresh LOW (f ≤ thr_byte → background/silent) AND |mv(b)| > kObjStaticMax (moves
            //     WITH the world model → confidently BACKGROUND) ⇒ the prior is CONTRADICTED → 0 (instant
            //     evaporation, no trailing ghost). A silent block (f low, |mv| ≤ kObjStaticMax) is
            //     cancellation-compatible → the prior PERSISTS, decayed. Fresh dissent (f ≥ decayed prior)
            //     wins the max() and REFRESHES the memory by construction.
            //   decay(p) = (uint8)(p·kPriorDecay) — the unconditional per-pair floor (unconfirmed → ~3 pairs; STAGE-67).
            //   dis_mask[b] = max(f, decay(expired_prior)).
            // Mutates dis_mask in place (the repair then clusters the MERGED mask). prior_src is NOT modified.
            auto mem_merge=[&](uint8_t* dis_mask, const void* anchor_field, const uint8_t* prior_src){
                const uint32_t MW=mvw_f, MH=mvh_f;
                const uint16_t* hmv=(const uint16_t*)anchor_field;
                const float thr_byte=cfg.matte_thresh*255.0f;       // the EXACT cluster/shader OBJECT cutoff
                const size_t nblk=(size_t)MW*(size_t)MH;
                for(size_t bi=0; bi<nblk; ++bi){
                    uint32_t p=(uint32_t)prior_src[bi];
                    if(p==0u) continue;                              // no belief here → mask unchanged (fast path)
                    const float f=(float)dis_mask[bi];
                    const bool fresh_low = (f<=thr_byte);            // fresh evidence does NOT call this OBJECT
                    if(fresh_low){
                        const float mvx=half_to_float(hmv[bi*2u+0u]);
                        const float mvy=half_to_float(hmv[bi*2u+1u]);
                        // confident BACKGROUND (moves with the model) contradicts the memory → evaporate now.
                        if(std::sqrt((double)mvx*mvx+(double)mvy*mvy) > (double)kObjStaticMax){ continue; }
                        // else: fresh low + near-static = cancellation-compatible → keep the prior, decayed.
                    }
                    uint32_t pd=(uint32_t)((double)p*kPriorDecay);   // unconditional decay floor
                    if((float)pd>f) dis_mask[bi]=(uint8_t)pd;        // max(fresh, decayed prior)
                }
            };
            // (C) REFRESH — at END of pair, fold the post-repair evidence back into the persistent prior,
            // ALL in CUR-anchored space (mem_adv = the advected belief, hostDISB = the post-repair cur mask):
            //   bwd ran : mem_prior := max(decay(mem_adv), hostDISB)   — the confirmed silhouette refreshes it.
            //   bwd skip: mem_prior := decay(mem_adv)                  — pure decay; a skipped pair never refreshes.
            // The decay here is the SAME unconditional floor; combined with the merge's max(), a remembered
            // silhouette can sustain itself through a LONG cancellation IFF the post-repair mask keeps
            // confirming it — see the self-reinforcement-brake note at the bwd refresh call site.
            // STAGE-68: wake_recs/wake_count carry THIS pair's ARMED BWD clusters (recorded in the bwd
            // object_repair). After the normal max() refresh, evaporate the TRAILING wake: "the object
            // takes its silhouette with it". obj_label still holds the BWD labels (the bwd repair ran last),
            // and the records are cur-anchored = mem_prior's space. Runs only when bwd_ran (the labels are
            // valid + the dis_b_post space is the confirmed cur mask); on a bwd skip mem_refresh pure-decays
            // and skips evaporation entirely (the throttle case — pure decay still bounds the wake).
            auto mem_refresh=[&](const uint8_t* dis_b_post, bool bwd_ran,
                                 const WakeRec* wake_recs, int wake_count){
                const uint32_t MW=mvw_f, MH=mvh_f;
                const size_t nblk=(size_t)MW*(size_t)MH;
                if(bwd_ran && dis_b_post){
                    for(size_t bi=0; bi<nblk; ++bi){
                        const uint32_t pd=(uint32_t)((double)mem_adv[bi]*kPriorDecay);
                        const uint32_t db=(uint32_t)dis_b_post[bi];
                        mem_prior[bi]=(uint8_t)(db>pd?db:pd);        // max(decay(advected prior), confirmed mask)
                    }
                } else {
                    for(size_t bi=0; bi<nblk; ++bi)
                        mem_prior[bi]=(uint8_t)((double)mem_adv[bi]*kPriorDecay);   // pure decay (no confirmation)
                }
                // ── STAGE-68: holonic wake evaporation (cur-anchored, AFTER the max above). For each armed
                // bwd cluster: zero mem_prior blocks that are (prior>0) AND NOT inside the cluster's
                // scanline-filled silhouette AND inside the cluster bbox EXPANDED opposite to the slot MV by
                // ceil(|mv|/8)+1 blocks (the trailing sweep — where a remembered static wake hides behind a
                // moving object). The fill bounds are RE-DERIVED from obj_label==cid (the kickoff's path);
                // obj_rowmin/obj_rowmax are free scratch here (the repair finished with them). The sweep is
                // the ONLY region touched, so a remembered silhouette AHEAD of / on the object is untouched.
                if(bwd_ran && wake_recs){
                    for(int w=0; w<wake_count; ++w){
                        const WakeRec& r=wake_recs[w];
                        if(r.cid<0) continue;
                        if(r.miny>r.maxy||r.minx>r.maxx) continue;
                        // re-derive the per-row fill bounds (min/max member column) from obj_label==cid.
                        for(int ry=r.miny; ry<=r.maxy; ++ry){ obj_rowmin[ry]=INT32_MAX; obj_rowmax[ry]=INT32_MIN; }
                        for(int ry=r.miny; ry<=r.maxy; ++ry){
                            const size_t base=(size_t)ry*MW;
                            for(int rx=r.minx; rx<=r.maxx; ++rx){
                                if(obj_label[base+rx]==r.cid){ if(rx<obj_rowmin[ry])obj_rowmin[ry]=rx; if(rx>obj_rowmax[ry])obj_rowmax[ry]=rx; }
                            }
                        }
                        // trailing-sweep bbox: expand OPPOSITE to the slot MV by e=ceil(|mv|/8)+1 blocks.
                        const double omv=std::sqrt(r.mvx*r.mvx+r.mvy*r.mvy);
                        const int e=(int)std::ceil(omv/8.0)+1;        // sweep depth in block units (px→block /8)
                        int sminx=r.minx, smaxx=r.maxx, sminy=r.miny, smaxy=r.maxy;
                        if(r.mvx>0.0) sminx-=e; else if(r.mvx<0.0) smaxx+=e;   // trailing side = behind the motion
                        if(r.mvy>0.0) sminy-=e; else if(r.mvy<0.0) smaxy+=e;
                        if(sminx<0)sminx=0; if(sminy<0)sminy=0;
                        if(smaxx>(int)MW-1)smaxx=(int)MW-1; if(smaxy>(int)MH-1)smaxy=(int)MH-1;
                        for(int ry=sminy; ry<=smaxy; ++ry){
                            const size_t base=(size_t)ry*MW;
                            const bool row_in_bbox=(ry>=r.miny && ry<=r.maxy);
                            for(int rx=sminx; rx<=smaxx; ++rx){
                                const size_t bi=base+rx;
                                if(mem_prior[bi]==0) continue;        // nothing to expire here
                                // inside the filled silhouette? (only within the cluster bbox rows can it be).
                                const bool in_sil = row_in_bbox && obj_rowmin[ry]<=obj_rowmax[ry]
                                                  && rx>=obj_rowmin[ry] && rx<=obj_rowmax[ry];
                                if(!in_sil) mem_prior[bi]=0;          // trailing-wake block outside the body → evaporate
                            }
                        }
                    }
                }
            };
            // ── STAGE-85: the WAP per-pair CPU tail + F→P publish, factored out so the SERIAL path and the
            // forward-PIPELINED path run the SAME code (no second copy = no §6.2 silent-divergence hazard).
            // It WAITS the pair's fwd fence (pc.fwd_fence), runs the holon/mem/object CPU work over the pair's
            // DURABLE per-gen host buffers (hostX[pc.f_gen]), then publishes. The body is byte-verbatim the
            // pre-85 serial tail; the ONLY parameterization is (a) the per-pair values come from `pc` (so the
            // pipeline can defer it one pair) and (b) `allow_bwd` gates the bwd pass. SERIAL calls it inline
            // with allow_bwd=true → identical to pre-85. PIPELINED calls it deferred with allow_bwd=false:
            // the bwd shares the single `ofp` + the 2 `Bframe` slots with the fwd match, which the NEXT pair's
            // in-flight fwd record has already reused, so a deferred bwd would read clobbered Bframe content
            // (§6.1/§6.2) — the pipeline therefore runs bwd-OFF (matching the field's bwd-skip:100% regime).
            // It does NOT toggle cur_f / have_prev_f / prev_ingest_cseq (those are loop-progression state the
            // caller advances per RECORD so the next match picks the right Bframe slot) nor collect the q2
            // copy-outs (WAP never uses them). The TORN-READ DISCIPLINE (§6.2): pc.f_gen is the gen of the
            // pair being consumed; in the pipeline g_seq leads f_seq by one so pc.f_gen ≠ the in-flight pair's
            // gen → this only ever reads hostX[pc.f_gen] (durable post-pc.fwd_fence), never the GPU's live gen;
            // the process-wide state (persist / mem_* / obj_*) is mutated here in strict pair order.
            auto consume_wap = [&](const FwdPend& pc, bool allow_bwd){
                const int f_gen=pc.f_gen; const uint64_t cur_c=pc.cur_c; const int s=pc.s;
                const uint64_t span=pc.span; const int N_use=pc.N_use; const int prv_f=pc.prv_f;
                const int cur_f=pc.cur_f; const bool have_prev_f=pc.have_prev_f; const double tp0=pc.tp0;
                const uint32_t mvw=mvw_f, mvh=mvh_f;   // (WW+7)/8 × (WH+7)/8 — the WAP MV grid (constants)
                (void)prv_f;(void)cur_f;   // referenced only by the bwd record (allow_bwd path)
                // tf0 anchors the GPU-wait leg (t_flow) AND the fuse EMA. SERIAL (fwd_fence==NULL): the caller
                // already ran submit_wait and passes pc.tf0 = the pre-submit_wait timestamp, so t_flow =
                // now−pc.tf0 == the submit_wait duration and t_fuse = now−pc.tf0 == submit_wait+CPU — BYTE-
                // IDENTICAL to the pre-85 serial tail (tf0 set right before submit_wait). PIPELINED: tf0 is set
                // HERE right before the deferred fence wait, so t_flow = the fence-wait leg of the overlapped
                // pair and t_fuse = wait+CPU. Either way t_flow is the blocking-GPU-leg measure --fsub prints.
                const double tf0 = (pc.fwd_fence!=VK_NULL_HANDLE) ? now_ms() : pc.tf0;
                if(pc.fwd_fence!=VK_NULL_HANDLE) vkWaitForFences(FD.dev,1,&pc.fwd_fence,VK_TRUE,UINT64_MAX);   // CR2: the MISSED consume-side wait — FD.dev (B.dev null under single_gpu would crash)
                { const double tflow=now_ms()-tf0; t_flow_ema=t_flow_ema>0.0?t_flow_ema*0.8+tflow*0.2:tflow; }
                if(use_inertia && have_prev_f){
                    const uint16_t* hmv =(const uint16_t*)hostMV[f_gen];
                    const uint16_t* hsad=(const uint16_t*)hostSAD[f_gen];
                    const size_t nblk=(size_t)mvw*(size_t)mvh;
                    for(size_t i=0;i<nblk;++i){
                        const float mvx=half_to_float(hmv[i*2u+0u]);
                        const float mvy=half_to_float(hmv[i*2u+1u]);
                        const float szero=half_to_float(hsad[i*2u+1u]);
                        const bool static_now=(std::sqrt(mvx*mvx+mvy*mvy)<=1.0f) && (szero<=8.0f);
                        const int next = static_now ? ((int)persist[i]+16) : 0;
                        persist[i]=(uint8_t)(next>255?255:next);
                    }
                    std::memcpy(hostPER[f_gen], persist.data(), nblk);
                } else if(use_inertia){
                    std::memcpy(hostPER[f_gen], persist.data(), (size_t)mvw*(size_t)mvh);
                }
                const double pair_budget_ms=(double)src_interval_us.load()/1000.0;
                if(t_pair_ema>0.0){
                    if(!bwd_skipping){ if(t_pair_ema>0.95*pair_budget_ms) bwd_skipping=true; }
                    else            { if(t_pair_ema<0.80*pair_budget_ms) bwd_skipping=false; }
                }
                if(cfg.tiers){
                    if(!bwd_skipping){ pressure_tier=0; }
                    else if(t_pair_ema>0.0){
                        if(pressure_tier<2){ if(t_pair_ema>1.10*pair_budget_ms) pressure_tier=2; else pressure_tier=1; }
                        else if(pressure_tier==2){
                            if(t_pair_ema>1.50*pair_budget_ms) pressure_tier=3;
                            else if(t_pair_ema<0.95*pair_budget_ms) pressure_tier=1;
                        } else if(pressure_tier==3){
                            if(t_pair_ema<1.20*pair_budget_ms) pressure_tier=2;
                        }
                        // (pressure_tier==4 is held/released by the STAGE-87 deficit override below.)
                        //
                        // STAGE-87 (CORRECTED 2026-06-13): the deficit shed-tier is a budget-relative
                        // OVERRIDE, not a rung climbed through the STAGE-84 sub-ladder. The original
                        // [1.50,1.85]×budget gates were calibrated for a ~1.5×-budget pair this regime
                        // never reaches: the measured HSR@120 deficit pair is only ~1.2×budget (~10-11ms
                        // vs ~8.5ms src interval), so 3→4 at 1.85× (=15.7ms) was UNREACHABLE — and once
                        // reached, 4→3 at <1.50× fired on the SHED pair (~7ms ≪ 12.75ms) every iteration,
                        // so tier-4 could never sustain. Corrected: ENGAGE when the pair sustains over
                        // budget (the cons<arr deficit, >1.10×); HOLD until the SHED pair (~0.8×budget)
                        // drops below 0.65×budget — i.e. only when the source eases enough that full
                        // quality refits. The [0.65,1.10]×budget dead-band brackets the shed pair, so
                        // tier-4 STICKS instead of flapping. Still gated on --deficit-tier (default OFF) →
                        // byte-identical ladder when the flag is off (tier 4 stays unreachable).
                        if(cfg.deficit_tier){
                            // STAGE-87b (2026-06-14): PROACTIVE + DWELLED shed. The 1.10×/0.65× binary left
                            // the shed pair (~0.93×budget) at a thin margin → cons stuck ~8% below arr + the
                            // 64ms lap-escapes (the operator's freezes). Field A/B (HSR@120) showed tier-4
                            // engaging only reactively + a brief calm window dropping it → full-quality →
                            // re-deficit flap. Fix: ENGAGE EARLIER (>0.92×budget — shed before the deficit
                            // bites, leaving jitter headroom) and HOLD with a dwell (≥kTier4DwellPairs after
                            // engaging) so a momentary calm cannot re-trigger the flap; RELEASE only when truly
                            // eased (<0.55×budget AND dwell expired). Still --deficit-tier-gated (default OFF →
                            // byte-identical when off).
                            if(pressure_tier<4){ if(t_pair_ema>0.92*pair_budget_ms){ pressure_tier=4; tier4_dwell=kTier4DwellPairs; } }
                            else { if(tier4_dwell>0) --tier4_dwell;
                                   else if(t_pair_ema<0.55*pair_budget_ms) pressure_tier=3; }
                        }
                        // STAGE-104 / 2026-06-28 control-word: the util-driven tier FLOOR is now computed+published
                        // by the PRESENT thread (g_gov_floor) and applied BELOW, AFTER this whole if(cfg.tiers)
                        // block — DECOUPLED from bwd_skipping (the R2 root fix). The audit found the old in-here
                        // block DEAD in combat: it sat inside the `else if(t_pair_ema>0.0)` reached only when
                        // bwd_skipping==true, and bwd_skipping latches only at t_pair_ema>0.95×budget — the very
                        // CPU-budget gate the decoupling claimed to bypass. In combat the 4090 pegs while the F EMA
                        // stays UNDER budget, so bwd_skipping never latched and the floor never rose (= the
                        // STAGE-101 inertness the redesign was supposed to cure). One decode owner now: P.
                    } else { pressure_tier=1; }
                } else { pressure_tier = bwd_skipping?1:0; }
                // 2026-06-28 control-word (R2 root fix): apply the PRESENT-published governor floor as a MAX over
                // the CPU ladder, DECOUPLED from bwd_skipping. g_gov_floor is 0 unless --load-governor + a hot
                // 4090 → byte-identical otherwise. Gated on cfg.tiers to preserve --no-tiers semantics.
                if(cfg.tiers && cfg.load_governor){ const int gf=g_gov_floor.load(); if(gf>pressure_tier) pressure_tier=gf; }
                ++holon_pair_ctr;
                const int holon_period = (pressure_tier>=3)?4:(pressure_tier==2?2:1);
                // STAGE-87: tier-4 sheds the holon REFINEMENT (object_repair + scene-memory) on EVERY pair,
                // not every-Nth — the deficit recovery. gme-fit + matte + warp + inertia still run (the
                // raw-flow WAP that worked at 120fps pre-STAGE-61). Only reachable under --deficit-tier, so
                // holon_skip_pair stays byte-identical when the recovery is off.
                const bool shed_holon = (pressure_tier>=4);
                const bool holon_skip_pair = shed_holon || (cfg.tiers && (holon_period>1) && (holon_pair_ctr%(uint64_t)holon_period!=0));
                stat_tier.store((uint64_t)pressure_tier);
                // STAGE-101 (--load-governor): tier-5 is the DEEP-shed. It is a strict SUPERSET of tier-4 —
                // shed_holon above is already true at tier≥4 (object_repair + scene-memory off every pair), and
                // tier-5 ADDS: (a) the BACKWARD optical-flow leg forced OFF (the bwd pyramid + bwd gme — see
                // the do_bwd AND below), and (b) the cheapest single-pass gme-fit (1 IRLS iter, the gme_iters
                // override below). Forward-flow + gme-fit + matte + warp + inertia (raw-flow WAP) still run, so
                // F STILL generates while the CPU tail drops enough to keep up under combat. tier5_active is
                // only ever true when pressure_tier>=5, which is reachable ONLY under --load-governor → the two
                // gates below are byte-identical to today when the flag is off. STAGE-101: the per-pair IRLS
                // iteration count — single pass at tier-5 (cheapest), else the existing 2/3 (irls2/default).
                const bool tier5_active = (pressure_tier>=5);
                const uint32_t gme_iters = tier5_active ? 1u : (cfg.gme_irls2?2u:3u);
                // STAGE-85: allow_bwd folds in the pipeline's bwd-off rule (single ofp / 2 Bframe slots).
                // STAGE-101: tier-5 ALSO forces it off (skip the bwd pyramid + bwd gme entirely).
                const bool do_bwd = allow_bwd && use_bidir && have_prev_f && !bwd_skipping && !tier5_active;
                if(do_bwd){
                    // STAGE-115 (--nvofa): the bwd direction (cur→prv). Run the OFA provider FIRST (it writes
                    // ofp.motion_image()=bwd MV, RO, via its own 3 blocking submits), THEN cmdB_bwd records the
                    // copy-out + gme reading that RO image — same shape as the classical record. The OFA reuses
                    // ofp's mv/sad images; the fwd match already consumed its fwd MV (copied out on cmdF before
                    // this) so overwriting is safe. nvofa is flow_div==1 → feed Bframe at WW (cur,prv swapped for bwd).
                    if(use_nvofa){
                        nvofa_run(A,nvofa,Bframe[cur_f].img,Bframe[prv_f].img,WW,WH,/*use_bwd=*/true,
                                  ofp.motion_image(),ofp.motion_view(),ofp.sad_field_image(),ofp.sad_field_view(),
                                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                  flow_submit_q2_chain);   // STAGE-115b: the A.q2 prep/convert submits chain on semPrep/semOfa (1 CPU wait inside nvofa_run)
                    }
                    vkResetCommandBuffer(cmdB_bwd,0);
                    VkCommandBufferBeginInfo bib{}; bib.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO; vkBeginCommandBuffer(cmdB_bwd,&bib);
                    // coin-3: bwd direction is cur→prv. At flow_div>1 downsample (Bflow reused — the bwd
                    // record runs on its OWN cmd buffer cmdB_bwd, submitted+fenced (fB2) independently of the
                    // fwd cmdF, so reusing Bflow[0/1] is safe: the fwd match already consumed the fwd Bflow
                    // contents on cmdF before this bwd blit overwrites them). At flow_div==1 feed Bframe.
                    VkImageView ba=Bframe[cur_f].view, bb=Bframe[prv_f].view;
                    if(!use_nvofa){
                        if(flow_div>1u) flow_downsample(cmdB_bwd,cur_f,prv_f,ba,bb);
                        (void)ofp.record_optical_flow(cmdB_bwd,ba,bb,Cinterp.view,0.5f);
                    } else { (void)ba;(void)bb; }
                    img_barrier(cmdB_bwd,ofp.motion_image(),VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_ACCESS_SHADER_READ_BIT,VK_ACCESS_TRANSFER_READ_BIT);
                    { VkBufferImageCopy cp=full_bic(mvw,mvh); vkCmdCopyImageToBuffer(cmdB_bwd,ofp.motion_image(),VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,hMVB_b[f_gen].buf,1,&cp); }
                    img_barrier(cmdB_bwd,ofp.motion_image(),VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_ACCESS_TRANSFER_READ_BIT,VK_ACCESS_SHADER_READ_BIT);
                    // ── IDEA-1 (gme-gpu): the BWD affine fit on B, in cmdB_bwd, against the bwd MV (just
                    // copied out, but the MV image still holds it) + the LIVE (bwd) SAD. NOTE the change-gate
                    // SAD source: the CPU bwd fit (gme_fit_affine call below) gates on the FWD sad (hostSAD)
                    // by historical quirk; the GPU here gates on the bwd match's own sad_zero (the live
                    // ofp.sad_field_image()). For a TRUE block (sad_zero≈0 either way) this agrees; the small
                    // divergence is bwd-mask-only (the matte leading edge) and is what --gme-gpu-verify would
                    // surface on the bwd anchor. Writes hostDISB[f_gen] (B import) + reuses hGmeM_b[f_gen] for
                    // the bwd model (read back into mb6 below). Recorded after the MV copy-out so MV stays RO.
                    if(use_gme_gpu){
                        gme_record(cmdB_bwd,gmePipe,ofp.motion_image(),ofp.sad_field_image(),mvw,mvh,
                                   /*step=*/1u,cfg.change_gate,f_gen,/*anchor=*/1,
                                   hDISB_b[f_gen].buf,(VkDeviceSize)mvw*mvh,hGmeMB_b[f_gen].buf,
                                   cfg.gme_irls2?2:3);
                    }
                    vkEndCommandBuffer(cmdB_bwd);
                    // CR2/CR3: the bidir bwd-flow submit. FD-route to A.q2 under single_gpu (no-wait, fenced by fB2).
                    flow_submit_nowait(cmdB_bwd, fB2);
                }
                double gme_fit_total_ms=0.0; bool gme_did_fit=false; bool gme_did_bwd=false; double gme_dis_pct_fwd=0.0; float gme_m6_fwd[6]={};
                double obj_cost_ms=0.0; uint32_t obj_live_pair=0; uint32_t obj_rep_pair=0; uint32_t obj_infill_pair=0;
                if(use_gme&&have_prev_f){
                    const double g0=now_ms();
                    float m6[6]={};
                    double dis_pct;
                    if(use_gme_gpu){
                        // ── IDEA-1: the GPU produced the model (hostGmeM) + dis-mask (hostDIS) in cmdF, already
                        // waited (fF). Read the 6 floats; dis% derives from the mask (a stat-only approximation,
                        // see gme_dispct_from_mask). NO CPU gme_fit_affine — the dominant ~2.7ms tail is gone.
                        std::memcpy(m6, hostGmeM[f_gen], 6u*sizeof(float));
                        dis_pct = gme_dispct_from_mask((const uint8_t*)hostDIS[f_gen],mvw,mvh);
                        if(cfg.gme_gpu_verify){
                            // Cross-check: run the CPU fit into scratch + compare model rel-diff + mask flips.
                            const size_t nblk=(size_t)mvw*mvh;
                            if(gme_vfy_dis.size()<nblk) gme_vfy_dis.resize(nblk);
                            float c6[6]={};
                            const double cdis=gme_fit_affine(hostMV[f_gen],cfg.change_gate?hostSAD[f_gen]:nullptr,
                                                             mvw,mvh,c6,gme_vfy_dis.data(),/*sub2=*/false,(int)gme_iters);   // STAGE-101: mirror tier-5's iter count for an honest GPU/CPU cross-check
                            double num=0.0,den=0.0; for(int _p=0;_p<6;++_p){ const double dd=(double)m6[_p]-(double)c6[_p]; num+=dd*dd; den+=(double)c6[_p]*(double)c6[_p]; }
                            const double reldiff = den>0.0 ? std::sqrt(num/den) : std::sqrt(num);
                            uint64_t flips=0; const uint8_t* gm=(const uint8_t*)hostDIS[f_gen];
                            for(size_t i=0;i<nblk;++i) if(gm[i]!=gme_vfy_dis[i]) ++flips;
                            ++gme_vfy_n;
                            if(gme_vfy_n<=5 || (gme_vfy_n%120u)==0u)
                                std::printf("[ra] gme-gpu-verify[%llu]: model rel-diff=%.3e dis-mask flips=%llu/%zu | GPU t=(%.2f,%.2f) CPU t=(%.2f,%.2f) | dis GPU~%.1f%% CPU%.1f%%\n",
                                    (unsigned long long)gme_vfy_n,reldiff,(unsigned long long)flips,nblk,
                                    m6[0],m6[3],c6[0],c6[3],dis_pct,cdis);
                        }
                    } else {
                        // STAGE-101 (--load-governor): tier-5 uses the cheapest single-pass CPU fit (gme_iters
                        // = 1 at tier≥5, else the existing 2/3). gme_iters is byte-identical to cfg.gme_irls2?2:3
                        // when load_governor is off (tier-5 unreachable).
                        dis_pct=gme_fit_affine(hostMV[f_gen],
                                               cfg.change_gate?hostSAD[f_gen]:nullptr,
                                               mvw,mvh,m6,
                                               (uint8_t*)hostDIS[f_gen],gme_sub2,(int)gme_iters);
                    }
                    const double gdt=now_ms()-g0;
                    gme_fit_total_ms+=gdt; gme_did_fit=true; gme_dis_pct_fwd=dis_pct;
                    for(int _p=0;_p<6;++_p){ f_pair_gme_a[f_gen][_p]=m6[_p]; gme_m6_fwd[_p]=m6[_p]; }
                    f_pair_gme_valid_a[f_gen]=1;
                    f_pair_disp_a[f_gen]=(float)gme_dis_pct_fwd;   // P4: publish the per-pair gme dispersion to P (same F-write-before-fetch_add ordering)
                    gme_dis_x100.store((uint64_t)(dis_pct*100.0+0.5));
                    if(use_memory && !holon_skip_pair){
                        const double mc0=now_ms();
                        mem_advect(hostMV[f_gen]);
                        mem_merge((uint8_t*)hostDIS[f_gen],hostMV[f_gen],mem_prior.data());
                        obj_cost_ms+=now_ms()-mc0;
                    }
                    if(use_objects && !holon_skip_pair){
                        const double o0=now_ms();
                        uint32_t live=0,rep=0,infill=0;
                        object_repair(hostMV[f_gen],(uint8_t*)hostDIS[f_gen],m6,obj_slots_fwd,
                                      use_inertia?persist.data():nullptr,
                                      (use_inertia&&cfg.persist_reset)?persist.data():nullptr,
                                      span,+1.0,&live,&rep,&infill,
                                      nullptr,nullptr);
                        obj_cost_ms+=now_ms()-o0;
                        obj_live_pair=live; obj_rep_pair+=rep; obj_infill_pair+=infill;
                    }
                    f_pair_mfwd_a[f_gen]=(float)matte_mass_count((const uint8_t*)hostDIS[f_gen],mvw,mvh,cfg.matte_thresh);
                    if(objdump_left>0){
                        objdump_grid("dis",[&](size_t bi){ return ((const uint8_t*)hostDIS[f_gen])[bi]; });
                        objdump_grid("mv", [&](size_t bi){ const uint16_t* hh=(const uint16_t*)hostMV[f_gen];
                            const float mx=half_to_float(hh[bi*2u]); const float my=half_to_float(hh[bi*2u+1u]);
                            const float m=std::sqrt(mx*mx+my*my)*16.f; return (uint8_t)(m>255.f?255u:(uint8_t)m); });
                        objdump_grid("per",[&](size_t bi){ return use_inertia?persist[bi]:(uint8_t)0; });
                        ++objdump_idx; --objdump_left;
                        if(objdump_left==0) std::printf("[ra] objdump: complete — %llu pair grids in frames\\\n",(unsigned long long)objdump_idx);
                    }
                    f_pair_mbwd_a[f_gen]=0.f;
                } else if(use_gme){
                    f_pair_gme_valid_a[f_gen]=0;
                    f_pair_mfwd_a[f_gen]=0.f; f_pair_mbwd_a[f_gen]=0.f;
                    f_pair_disp_a[f_gen]=0.f;   // P4: no fit this pair -> no dispersion signal (P4 gate also checks gme_valid)
                }
                f_pair_bwd_valid_a[f_gen] = do_bwd ? 1 : 0;
                if(use_bidir && !do_bwd && have_prev_f) stat_bwd_skips.fetch_add(1);
                if(do_bwd){
                    vk_live(vkWaitForFences(FD.dev,1,&fB2,VK_TRUE,UINT64_MAX));   // P0: catch a TDR on the bwd flow — CR2: the MISSED consume-side wait, FD.dev (B.dev null under single_gpu would crash)
                    if(use_gme){
                        const double gb0=now_ms();
                        float mb6[6]={};
                        if(use_gme_gpu){
                            // ── IDEA-1: the bwd model + mask were produced in cmdB_bwd on B (waited via fB2 just
                            // above). Read the 6 floats; the bwd mask is already in hostDISB. No CPU bwd fit.
                            std::memcpy(mb6, hostGmeMB[f_gen], 6u*sizeof(float));
                        } else {
                            const double dis_pct_b=gme_fit_affine(hostMVB[f_gen],
                                                                  cfg.change_gate?hostSAD[f_gen]:nullptr,
                                                                  mvw,mvh,mb6,
                                                                  (uint8_t*)hostDISB[f_gen],gme_sub2,cfg.gme_irls2?2:3);
                            (void)dis_pct_b;
                        }
                        gme_fit_total_ms+=now_ms()-gb0; gme_did_bwd=true;
                        if(use_memory){
                            const double mc0=now_ms();
                            mem_merge((uint8_t*)hostDISB[f_gen],hostMVB[f_gen],mem_adv.data());
                            obj_cost_ms+=now_ms()-mc0;
                        }
                        if(use_objects){
                            const double o0=now_ms();
                            uint32_t live_b=0,rep_b=0,infill_b=0;
                            wake_n=0;
                            object_repair(hostMVB[f_gen],(uint8_t*)hostDISB[f_gen],mb6,obj_slots_bwd,
                                          use_inertia?persist.data():nullptr,
                                          nullptr,
                                          span,-1.0,&live_b,&rep_b,&infill_b,
                                          use_memory?wake_rec.data():nullptr, use_memory?&wake_n:nullptr);
                            obj_cost_ms+=now_ms()-o0;
                            obj_rep_pair+=rep_b; obj_infill_pair+=infill_b;
                            (void)live_b;
                        }
                        f_pair_mbwd_a[f_gen]=(float)matte_mass_count((const uint8_t*)hostDISB[f_gen],mvw,mvh,cfg.matte_thresh);
                        for(int _p=0;_p<6;++_p) f_pair_gme_bwd_a[f_gen][_p]=mb6[_p];
                    }
                }
                if(use_memory && gme_did_fit && !holon_skip_pair){
                    const double mr0=now_ms();
                    mem_refresh(gme_did_bwd?(const uint8_t*)hostDISB[f_gen]:nullptr, gme_did_bwd,
                                wake_rec.data(), wake_n);
                    obj_cost_ms+=now_ms()-mr0;
                }
                if(use_gme&&gme_did_fit){
                    gme_fit_ema=gme_fit_ema>0.0?gme_fit_ema*0.8+gme_fit_total_ms*0.2:gme_fit_total_ms;
                    gme_fit_us.store((uint64_t)(gme_fit_ema*1000.0));
                    if(!gme_sub2&&gme_fit_ema>2.0) gme_sub2=true;
                    ++gme_fits;
                    if(gme_fits<=3)
                        std::printf("[ra] gme: t=(%.2f,%.2f)px b=%.4f c=%.4f e=%.4f f=%.4f dis:%.0f%% fit=%.2fms%s%s\n",
                            gme_m6_fwd[0],gme_m6_fwd[3],gme_m6_fwd[1],gme_m6_fwd[2],gme_m6_fwd[4],gme_m6_fwd[5],gme_dis_pct_fwd,gme_fit_total_ms,gme_did_bwd?" (fwd+bwd)":"",gme_sub2?" (sub2)":"");
                    if(!gme_fit_printed&&gme_fits>=1){ gme_fit_printed=true;
                        if(gme_fit_ema>1.0) std::printf("[ra] gme: fit cost EMA %.2fms%s (>1ms — shown in stats)\n",gme_fit_ema,gme_did_bwd?" (fwd+bwd)":""); }
                }
                if(use_objects&&gme_did_fit&&!holon_skip_pair){
                    obj_live.store((uint64_t)obj_live_pair);
                    const double rep_pct=obj_infill_pair?100.0*(double)obj_rep_pair/(double)obj_infill_pair:0.0;
                    obj_rep_x10.store((uint64_t)(rep_pct*10.0+0.5));
                    obj_cost_ema=obj_cost_ema>0.0?obj_cost_ema*0.8+obj_cost_ms*0.2:obj_cost_ms;
                    ++obj_pairs;
                    if(!obj_settle_printed&&obj_pairs>=1){ obj_settle_printed=true;
                        if(obj_cost_ema>0.3) std::printf("[ra] objects: F-side cost EMA %.2fms (>0.3ms — cluster+identity+repair, both anchors)\n",obj_cost_ema); }
                }
                if(cfg.fg_auto){ const double dt=now_ms()-tf0; t_fuse_ema=t_fuse_ema>0.0?t_fuse_ema*0.8+dt*0.2:dt; }
                // STAGE-85b: the per-pair F-COST that drives the bwd-skip + tier latches must be F's
                // BUSY time, not wall-clock-since-WAP-entry. SERIAL (fwd_fence==NULL): consume runs in
                // the same iteration, so now−tp0 == record+submit_wait+CPU = the busy time (UNCHANGED,
                // byte-identical to pre-85). PIPELINE (fwd_fence!=NULL): the deferred consume runs inside
                // the NEXT pair's iteration, so now−tp0 wrongly folds in that pair's ingest-wait + record
                // (~+8-13ms at arr~67 → tier:2/3 + bwd-skip over-fire under light load, the A/B bug). The
                // correct pipeline cost is now−tf0 = fence-wait + CPU ≈ max(GPU-leg, CPU) — the real
                // F-throughput cost (GPU hidden when fully overlapped → ≈CPU; GPU-bound → ≈GPU). The
                // ingest-wait (F idle, only present when NOT in deficit) is correctly excluded.
                { const double dtp=(pc.fwd_fence!=VK_NULL_HANDLE)?(now_ms()-tf0):(now_ms()-tp0);
                  t_pair_ema=t_pair_ema>0.0?t_pair_ema*0.8+dtp*0.2:dtp; }
                stat_flow_us.store((uint64_t)(t_flow_ema*1000.0));
                stat_pair_us.store((uint64_t)(t_pair_ema*1000.0));
                // ── publish (the shared-tail logic for WAP, moved here so the pipeline can DEFER it one pair).
                // f_pair_*[f_gen] written BEFORE the f_seq.fetch_add (the seq_cst pair orders them for P).
                f_pair_cseq_a[f_gen]=cur_c; f_pair_slot_a[f_gen]=s; f_pair_tcap_a[f_gen]=c_slots[s].t_cap_ms;
                f_pair_span_a[f_gen]=span; f_pair_n_a[f_gen]=N_use;
                stat_cons.fetch_add(1);
                f_seq.fetch_add(1);
                f_cv.notify_all();
                if(cfg.latency_trace){ const double tc=f_pair_tcap_a[f_gen];   // STAGE-112: tcap→publish = the full F-side freshage contribution (pickup+ingest+convert+build); detect derived in stats
                    if(tc>0.0){ const double fp=(now_ms()-tc)*1000.0; if(fp>0.0&&fp<2000000.0){
                        const uint64_t pv=lt_fpub_us.load(); lt_fpub_us.store(pv?(uint64_t)((double)pv*0.8+fp*0.2):(uint64_t)fp); } } }
                if(cfg.fg_auto){
                    const double Tsrc=(double)src_interval_us.load()/1000.0;
                    const double pres=(double)present_cost_us.load()/1000.0;
                    span_ema=span_ema*0.9+(double)(span>1?span-1:0)*0.1;
                    if(dwell_sets>0) --dwell_sets;
                    if(span_ema>0.25&&live_n_f>1&&dwell_sets==0){
                        live_n_f-=1; span_ema=0.0; up_streak=0; deg_streak=0; dwell_sets=90;
                    } else if(dwell_sets==0){
                        const double set_live=t_fuse_ema+(double)std::max(0,live_n_f-2)*t_warp_ema;
                        const double pres_live=(double)live_n_f*pres;
                        if(live_n_f>1&&(set_live>0.90*Tsrc||(pres>0.0&&pres_live>1.2*Tsrc))){
                            up_streak=0;
                            if(++deg_streak>=3){ live_n_f-=1; deg_streak=0; dwell_sets=90; }
                        } else {
                            deg_streak=0;
                            if(live_n_f<cfg.fg_factor){
                                const int n=live_n_f+1;
                                const double set_t=t_fuse_ema+(double)std::max(0,n-2)*t_warp_ema;
                                const double pres_t=(double)n*pres;
                                if(set_t<=0.65*Tsrc&&(pres<=0.0||pres_t<=Tsrc)){
                                    if(++up_streak>=45){ live_n_f=n; up_streak=0; dwell_sets=90; }
                                } else up_streak=0;
                            }
                        }
                    }
                    live_n_atomic.store(live_n_f);
                }
            };
            while(!g_quit&&!g_quit_threads.load()){
                {
                    std::unique_lock<std::mutex> lk(c_mtx);
                    c_cv.wait_for(lk,std::chrono::milliseconds(8),
                        [&]{ return c_seq.load()>last_c_seen||g_quit||g_quit_threads.load(); });
                }
                if(g_quit||g_quit_threads.load()) break;
                const uint64_t newest_c=c_seq.load();
                if(newest_c==last_c_seen) continue;
                // STAGE-36: in-order ingest. F used to jump to the NEWEST capture, so every
                // long F iteration (worst 13-16ms vs the ~12.8ms source gap) skipped a source
                // → span-2 pair → 2× displacement → degraded interps a few times per second
                // (the walking tremble; operator-isolated: fg1 clean, FG-on correlates with the
                // drop/slip-max log lines). Now F consumes last+1 (span 1) while the backlog
                // fits the ring's safe read window — it drains because F is faster on average —
                // and jumps to newest only on a real stall (R1 span pacing absorbs that one).
                //
                // STAGE-84 NOTE — an ingest-side "lap escape" was prototyped here and REMOVED as dead code
                // (verify-before-claim, supervisor pass). The line just above (`if(newest_c==last_c_seen)
                // continue;`) plus c_seq monotonicity make g = newest_c-last_c_seen >= 1 here; the in-order
                // branch's guard caps the in-order backlog at g <= kIngestBacklog. The read slot (cur_c-1)%cap_slots
                // and C's write head newest_c%cap_slots are then at most kIngestBacklog apart (mod cap_slots), and
                // the torn-read invariant cap_slots >= kIngestBacklog+1 (static_asserted at decl via kCapSlotsMin)
                // guarantees a >=1-slot non-collision margin → they do NOT alias; and g > kIngestBacklog already
                // falls to `newest_c` (a slot != C's write head). So the selector is torn-read-safe by the LIVE
                // constants (NOT the old frozen mod-4/kCapSlots=4 example) — the proposed escape condition is
                // unreachable under the g <= kIngestBacklog guard. The HSR
                // field DEATH was the UNBOUNDED spin guard below (a liveness deadlock, not a torn read);
                // STAGE-84 proper fixes that. Selector kept byte-identical to the pre-stage one-liner.
                // STAGE-111 (--ingest-backlog): the drain cap is the runtime cfg.ingest_backlog (default kIngestBacklog=3,
                // byte-identical). It only ever REDUCES the compile-time kIngestBacklog, so the torn-read invariant
                // cap_slots >= kIngestBacklog+1 (static_asserted) still bounds it (cfg.ingest_backlog <= kIngestBacklog
                // <= cap_slots-1). Lower = F jumps to newest sooner = fresher reals (less freshage) at the cost of more
                // span-2 pairs (the STAGE-36 walking tremble) — the input-lag-floor knob (freshage is ~this × T_src).
                const uint64_t cur_c=(last_c_seen>0&&(newest_c-last_c_seen)<=(uint64_t)cfg.ingest_backlog)
                                     ? last_c_seen+1 : newest_c;   // STAGE-92b/111: in-order drain cap, DECOUPLED from the 16-deep ring (real persistence)
                last_c_seen=cur_c;
                const int s=(int)((cur_c-1)%(uint64_t)cap_slots);
                // STAGE-112 (--latency-trace): tcap→F-pickup = the convert+ingest+backlog wait (the suspected
                // dominant non-F-compute freshage chunk). now − the picked slot's tcap. EMA µs. Off → no work.
                // ALSO capture the pickup wall time → the submit-wait sites compute pre_flow = tf0 − pickup =
                // the F per-pair NON-compute window (upload+unpack+flow-record+ring-guard spin, BEFORE the fsub
                // tf0/fence-wait) — the ~11ms `build − fsub(8.7)` gap this trace exists to localize.
                double lt_pickup_now=0.0;
                if(cfg.latency_trace){ lt_pickup_now=now_ms(); const double tc=c_slots[s].t_cap_ms;
                    if(tc>0.0){ const double pw=(lt_pickup_now-tc)*1000.0; if(pw>0.0&&pw<2000000.0){
                        const uint64_t pv=lt_pickup_us.load(); lt_pickup_us.store(pv?(uint64_t)((double)pv*0.8+pw*0.2):(uint64_t)pw); } } }
                // STAGE-85: the GPU-write gen. SERIAL derives it from f_seq (record+publish lockstep);
                // PIPELINED derives it from the GPU-record counter g_seq (which leads f_seq by one while
                // the just-recorded pair's CPU/publish are still deferred) so cur's GPU writes a DIFFERENT
                // gen slot than the pair being consumed reads (torn-read guard, §6.2). In serial both are
                // equal → f_gen is byte-identical to the pre-85 `f_seq.load()%NS`.
                const bool fwd_pipe = (use_wap && cfg.fwd_pipeline);
                const int f_gen=(int)((fwd_pipe?g_seq:f_seq.load())%(uint64_t)NS);
                // STAGE-39c: F must not build into the generation P is still presenting.
                // Condition: P is holding the f_seq slot that F is about to overwrite
                // (p_presenting == f_seq+1-kGenRing). With kGenRing=3 this only fires
                // when span≥3 (large stalls); the ring width covers the normal fg8/30fps case.
                //
                // STAGE-84 — THE BOUNDED ESCAPE (the deadlock site the analysis named). The pre-fix
                // loop is UNBOUNDED: F spins while p_presenting==fs-2 with no exit but g_quit. Under a
                // sustained HSR deficit P can FREEZE on the oldest generation (its real-slot lapped →
                // real_valid/prev_slot_safe fail → the monotone t_display clamp pins it) and re-store the
                // SAME p_presenting every tick. p_presenting then sits at exactly fs-2 forever, F spins
                // forever, f_seq never advances → P never advances → the circular liveness deadlock
                // (cons→0, lat linear, F's stats frozen — the operator's death signature). The escape is
                // bounded: spin at most ~kRingGuardSpinMax ms (re-reading p_presenting each Sleep so the
                // NORMAL transient — P advancing within a tick or two — still resolves by the wait). If P
                // is still pinned after the bound it is FROZEN, not transiently busy → stop waiting and
                // proceed. This is SAFE because P's own read-side gates (real_valid §3, prev_slot_safe §5)
                // already freeze-at-cur if F overwrites a generation P holds — the guard is a quality
                // optimization (avoid one freeze tick), never a hard correctness barrier. F advancing
                // un-pins P (a fresh f_seq) and breaks the cycle. Counted as a lap-escape (same recovery
                // class — F broke an unrecoverable wait to stay live).
                // STAGE-85: the guard protects the gen F is ABOUT TO OVERWRITE. SERIAL: that pair publishes
                // as f_seq → guard vs f_seq (unchanged). PIPELINED: it publishes as g_seq (g_seq leads f_seq
                // by one) → guard vs g_seq so the "kGenRing-deep" safety window stays anchored to the slot
                // actually being clobbered, not to the lagged publish counter. p_presenting holds an f_seq
                // VALUE; with g_seq=f_seq+1 the ON target fs-1 == the oldest of the 3 live gens P can hold.
                { const uint64_t fs=fwd_pipe?g_seq:f_seq.load();
                  static constexpr int kRingGuardSpinMax=64;   // ~64ms ceiling (≈4 source frames at 60fps)
                  int guard_spins=0;
                  const double lt_spin0 = cfg.latency_trace ? now_ms() : 0.0;   // STAGE-112: measure the ring-guard spin wall-time (the prime preflow suspect: F throttled to P's gen-consume rate)
                  while(!g_quit&&!g_quit_threads.load()&&
                        p_presenting.load()==fs+1u-(uint64_t)kGenRing){
                      if(++guard_spins>kRingGuardSpinMax){
                          // P is frozen on this generation — break the circular wait and proceed.
                          stat_lap.fetch_add(1);
                          const double tnow=now_ms();
                          if(tnow-last_lap_log_ms>2000.0){ last_lap_log_ms=tnow;
                              std::printf("[ra] F lap-escape: P pinned on gen (p_presenting=%llu, fs=%llu) %dms — proceeding (P read-side gates cover it)\n",
                                  (unsigned long long)p_presenting.load(),(unsigned long long)fs,kRingGuardSpinMax); }
                          break;
                      }
                      Sleep(1);
                  }
                  if(cfg.latency_trace && lt_spin0>0.0){ const double sp=(now_ms()-lt_spin0)*1000.0;   // STAGE-112: ring-guard spin wall-time → splits preflow into spin (F waits for P) vs record/upload
                      if(sp>=0.0&&sp<2000000.0){ const uint64_t pv=lt_spin_us.load(); lt_spin_us.store(pv?(uint64_t)((double)pv*0.8+sp*0.2):(uint64_t)sp); } } }
                const int prv_f=(cur_f+1)&1;
                // STAGE-85: the fwd-record cmd buffer. SERIAL (and every non-pipelined path) → cmdB/fB exactly
                // as before. PIPELINED WAP → the ping-pong cmdB_fwd[g_seq&1]/fB_fwd[g_seq&1] so pair N's fwd
                // submit (no-wait) does not reset the buffer/fence pair N-1's deferred consume is still waiting
                // on. cmdF==cmdB for everything except the opt-in WAP pipeline → OFF/non-WAP are byte-unchanged.
                VkCommandBuffer cmdF = fwd_pipe ? cmdB_fwd[(int)(g_seq&1u)] : cmdB;
                VkFence         fF   = fwd_pipe ? fB_fwd[(int)(g_seq&1u)]   : fB;
                // STAGE-120 (--fwd-prestage, L2): split the hRP_b[s]->hRP_b_dev[s] device copy OUT of the blocking
                // cmdF flow submit. do_prestage is live ONLY on the serial WAP + iGPU-convert path (use_fwd_prestage
                // already gates use_wap+use_igpu_convert at config time; !fwd_pipe excludes the STAGE-85 pipeline,
                // which already overlaps this copy via its own deferred-consume). When live: record JUST the copy
                // into the ping-pong cmdF_pre[g_seq&1] and submit it NO-WAIT on the F flow lane (A.q2, under
                // a_q2_mtx via flow_submit_nowait) BEFORE cmdF — same queue, so the copy is queued ahead and runs
                // while the flow record + (already-paid) ring-guard spin proceed on the CPU. cmdF then SKIPS the
                // inline copy but KEEPS the hRP_b_dev[s] TRANSFER_WRITE->SHADER_READ barrier below (the memory
                // dependency edge for the unpack's read; same-queue submission order gives the execution edge —
                // RFP-CR2). OFF (do_prestage false) → the copy records inline in cmdF exactly as before, byte-identical.
                const bool do_prestage = use_fwd_prestage && !fwd_pipe;
                if(do_prestage){
                    VkCommandBuffer cmdP=cmdF_pre[(int)(g_seq&1u)]; VkFence fP=fF_pre[(int)(g_seq&1u)];
                    vkResetCommandBuffer(cmdP,0);
                    VkCommandBufferBeginInfo bp{}; bp.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO; vkBeginCommandBuffer(cmdP,&bp);
                    {VkBufferCopy bc{0,0,VkDeviceSize(WW)*WH*3u}; vkCmdCopyBuffer(cmdP,hRP_b[s].buf,hRP_b_dev[s].buf,1,&bc);}
                    vkEndCommandBuffer(cmdP);
                    flow_submit_nowait(cmdP,fP);   // A.q2 no-wait (under a_q2_mtx) — the copy executes ahead of cmdF on the same queue; fP is drained by vkDeviceWaitIdle(A) at shutdown
                }
                vkResetCommandBuffer(cmdF,0);
                VkCommandBufferBeginInfo bi{}; bi.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO; vkBeginCommandBuffer(cmdF,&bi);
                if(use_igpu_convert){
                    if(!do_prestage){VkBufferCopy bc{0,0,VkDeviceSize(WW)*WH*3u}; vkCmdCopyBuffer(cmdF,hRP_b[s].buf,hRP_b_dev[s].buf,1,&bc);}   // STAGE-120: prestaged → copy already submitted on cmdF_pre (A.q2, ahead of cmdF); OFF → inline, byte-identical
                    {VkBufferMemoryBarrier bmb{}; bmb.sType=VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER; bmb.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT; bmb.dstAccessMask=VK_ACCESS_SHADER_READ_BIT; bmb.buffer=hRP_b_dev[s].buf; bmb.offset=0; bmb.size=VK_WHOLE_SIZE; vkCmdPipelineBarrier(cmdF,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,0,nullptr,1,&bmb,0,nullptr);}
                    img_barrier(cmdF,Bframe[cur_f].img,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_GENERAL,0,VK_ACCESS_SHADER_WRITE_BIT);
                    vkCmdBindPipeline(cmdF,VK_PIPELINE_BIND_POINT_COMPUTE,ubPipe[s].pipe);
                    vkCmdBindDescriptorSets(cmdF,VK_PIPELINE_BIND_POINT_COMPUTE,ubPipe[s].layout,0,1,&ubPipe[s].sets[(uint32_t)cur_f],0,nullptr);
                    struct{uint32_t W;uint32_t H;}pcub{WW,WH}; vkCmdPushConstants(cmdF,ubPipe[s].layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(pcub),&pcub);
                    vkCmdDispatch(cmdF,(WW+7)/8,(WH+7)/8,1);
                    img_barrier(cmdF,Bframe[cur_f].img,VK_IMAGE_LAYOUT_GENERAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT);
                } else {
                    img_barrier(cmdF,Bframe[cur_f].img,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,0,VK_ACCESS_TRANSFER_WRITE_BIT);
                    { VkBufferImageCopy cp=full_bic(WW,WH); vkCmdCopyBufferToImage(cmdF,hR_b[s].buf,Bframe[cur_f].img,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&cp); }
                    img_barrier(cmdF,Bframe[cur_f].img,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT);
                }
                // STAGE-35-R4: per-set N — auto uses the F-owned live_n (degrades by measured
                // capacity); explicit uses the cap. Built AND published with THIS set's N so P
                // paces it self-consistently. STAGE-35-R1: span = source frames this pair covers.
                const int N_use  = cfg.fg_auto ? std::max(1,live_n_f) : cfg.fg_factor;
                const int NI_use = std::max(0,N_use-1);
                const uint64_t span = (prev_ingest_cseq>0&&cur_c>prev_ingest_cseq)?(cur_c-prev_ingest_cseq):1u;
                if(span>1) stat_skips.fetch_add(1);   // STAGE-36: content skip (tremble signature)
                if(use_wap){
                    // STAGE-41: WAP mode — F ships the RECIPE (MV+SAD), not the cake (7 warps).
                    // After the match it copies B's MV+SAD images into the per-gen host bridges;
                    // the presenter (G) re-warps per tick at the exact phase. No interp copy-outs,
                    // no x4 58MB/set leg. N_use still travels (phase-grid semantics in stats only).
                    // The k=0.5 warp into Cinterp is a throwaway needed to PRODUCE the MV+SAD;
                    // its Cinterp output is discarded (WAP never reads hostI).
                    // coin-3 CORRUPTION VECTOR (the load-bearing collapse): this local sizes the copy-out
                    // full_bic(mvw,mvh) from ofp.motion_image()/sad_field_image()/cand_image() into the host
                    // bridges AND feeds the whole CPU tail (gme_fit_affine/object_repair/matte_mass_count).
                    // At flow_div>1 the source images shrank to ofp.motion_width()×height(); if this stayed
                    // (WW+7)/8 the copy would over-read the smaller source → validation error / garbage. So
                    // it MUST track the OFP grid. mvw_f/mvh_f (= ofp.motion_width()/height(), defined at the
                    // F-thread top) IS that grid → reuse it. At flow_div==1 == (WW+7)/8 (byte-identical).
                    const uint32_t mvw=mvw_f, mvh=mvh_f;
                    // STAGE-55: tp0 anchors the FULL pair-iteration time (record → copy-out → fwd
                    // submit_wait → fwd fit → bwd → bwd fit). t_pair_ema is built from it at the
                    // iteration end and drives the bwd-skip decision (read just after the fwd wait).
                    const double tp0=now_ms();
                    if(have_prev_f){
                        // coin-3: at flow_div>1 downsample the pair into Bflow and feed THOSE views (matches
                        // the smaller MV grid); at flow_div==1 feed Bframe directly (byte-identical path).
                        VkImageView fa=Bframe[prv_f].view, fb=Bframe[cur_f].view;
                        if(use_nvofa){
                            // STAGE-115 (--nvofa): the HW OFA flow provider REPLACES the classical match. cmdF so
                            // far holds ONLY the capture upload (Bframe[cur_f]) — flush it (so cur_f is on the GPU
                            // for the OFA blit), run the semaphore-chained prep→OFA→convert (it writes ofp.motion/
                            // sad_field in place, leaving them SHADER_READ_ONLY_OPTIMAL = the exact post-record
                            // contract), then RE-BEGIN cmdF for the gme + copy-out TAIL below (which records into
                            // cmdF exactly as the classical path does). nvofa is gated to flow_div==1, so fa/fb =
                            // Bframe at WW; the provider downscales WW→in_w internally (mv_scale=WW/in_w). The A.q2
                            // submits (prep/convert) hold a_q2_mtx for the SUBMIT only; the F-thread blocks ONCE on
                            // fConv inside nvofa_run (STAGE-115b — the 3-blocking-submit preflow spike is gone).
                            (void)fa;(void)fb;
                            vkEndCommandBuffer(cmdF);
                            flow_submit_wait(cmdF,fF);   // flush the upload (cur_f now GPU-resident + RO)
                            nvofa_run(A,nvofa,Bframe[prv_f].img,Bframe[cur_f].img,WW,WH,/*use_bwd=*/false,
                                      ofp.motion_image(),ofp.motion_view(),ofp.sad_field_image(),ofp.sad_field_view(),
                                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                      flow_submit_q2_chain);   // STAGE-115b: the A.q2 prep/convert submits chain on semPrep/semOfa (1 CPU wait inside nvofa_run)
                            // re-begin cmdF for the gme + copy-out tail (the upload was already flushed above).
                            vkResetCommandBuffer(cmdF,0);
                            VkCommandBufferBeginInfo bnv{}; bnv.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO; vkBeginCommandBuffer(cmdF,&bnv);
                        } else {
                        if(flow_div>1u) flow_downsample(cmdF,prv_f,cur_f,fa,fb);
                        (void)ofp.record_optical_flow(cmdF,fa,fb,Cinterp.view,0.5f);
                        }
                        if(use_mv_smooth){
                            // Smooth the MV in place before shipping (same pass the warp path uses;
                            // here it conditions the field G will warp from).
                            img_barrier(cmdF,ofp.motion_image(),VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_IMAGE_LAYOUT_GENERAL,VK_ACCESS_SHADER_READ_BIT,VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT);
                            vkCmdBindPipeline(cmdF,VK_PIPELINE_BIND_POINT_COMPUTE,mvsm.pipe);
                            vkCmdBindDescriptorSets(cmdF,VK_PIPELINE_BIND_POINT_COMPUTE,mvsm.layout,0,1,&mvsm.set,0,nullptr);
                            struct{float alpha;float cut;uint32_t w;uint32_t h;float span;}pcm{cfg.mv_smooth,kMvCut,mvw_f,mvh_f,(float)span};
                            vkCmdPushConstants(cmdF,mvsm.layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(pcm),&pcm);
                            vkCmdDispatch(cmdF,(mvw_f+7)/8,(mvh_f+7)/8,1);
                            img_barrier(cmdF,ofp.motion_image(),VK_IMAGE_LAYOUT_GENERAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT);
                        }
                        // ── IDEA-1 (gme-gpu): record the FWD affine fit on B, in cmdF, right after the match
                        // (MV/SAD are RO here) and BEFORE the MV/SAD copy-out (gme_record restores RO at the
                        // end, so the copy-out barriers below are valid). It writes the dis-mask into hostDIS
                        // (B import) + the 6-float model into hostGmeM; F reads both in consume_wap. step=1
                        // (full grid — the GPU reduce is cheap; no sub2 needed). use_gate = cfg.change_gate.
                        // Only when use_gme_gpu && have_prev_f (a real fit). Off → byte-identical (not recorded).
                        if(use_gme_gpu && have_prev_f){
                            // STAGE-101 (--load-governor): tier-5 runs the cheapest single-pass GPU gme-fit
                            // (1 IRLS iter) to shave the per-pair cost under combat. pressure_tier here is the
                            // PREVIOUS consumed pair's level (set by the last consume_wap) — a 1-pair lag that
                            // is immaterial given the tier is dwell-held. Reachable only at tier≥5 (= only under
                            // --load-governor) → byte-identical (cfg.gme_irls2?2:3) when the flag is off.
                            const int fwd_gme_iters = (cfg.load_governor && pressure_tier>=5) ? 1 : (cfg.gme_irls2?2:3);
                            gme_record(cmdF,gmePipe,ofp.motion_image(),ofp.sad_field_image(),mvw,mvh,
                                       /*step=*/1u,cfg.change_gate,f_gen,/*anchor=*/0,
                                       hDIS_b[f_gen].buf,(VkDeviceSize)mvw*mvh,hGmeM_b[f_gen].buf,
                                       fwd_gme_iters);
                        }
                        // MV + SAD are in SHADER_READ_ONLY after the match → TRANSFER_SRC → copy
                        // out to the per-gen host bridges → back to SHADER_READ_ONLY (the next
                        // match leaves them RO again; restoring keeps the layout contract clean).
                        img_barrier(cmdF,ofp.motion_image(),VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_ACCESS_SHADER_READ_BIT,VK_ACCESS_TRANSFER_READ_BIT);
                        img_barrier(cmdF,ofp.sad_field_image(),VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_ACCESS_SHADER_READ_BIT,VK_ACCESS_TRANSFER_READ_BIT);
                        { VkBufferImageCopy cp=full_bic(mvw,mvh); vkCmdCopyImageToBuffer(cmdF,ofp.motion_image(),VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,hMV_b[f_gen].buf,1,&cp); }
                        { VkBufferImageCopy cp=full_bic(mvw,mvh); vkCmdCopyImageToBuffer(cmdF,ofp.sad_field_image(),VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,hSAD_b[f_gen].buf,1,&cp); }
                        img_barrier(cmdF,ofp.motion_image(),VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_ACCESS_TRANSFER_READ_BIT,VK_ACCESS_SHADER_READ_BIT);
                        img_barrier(cmdF,ofp.sad_field_image(),VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_ACCESS_TRANSFER_READ_BIT,VK_ACCESS_SHADER_READ_BIT);
                        // STAGE-77: the second-best candidate copy-out — ONLY in the FWD pass (cand_image() is
                        // written at the finest level of THIS record_optical_flow; the bwd record below reuses
                        // ofp's images and would overwrite it, but this copy + the fwd-fence wait land it in
                        // hC2_b == hostC2[f_gen] before the bwd match runs). Same RO→TRANSFER_SRC→RO barrier
                        // discipline as MV/SAD; cand_image() is left SHADER_READ_ONLY_OPTIMAL post-call. Gated
                        // on use_ambig (cand_image() is VK_NULL_HANDLE otherwise — the copy must not be recorded).
                        if(use_ambig){
                            img_barrier(cmdF,ofp.cand_image(),VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_ACCESS_SHADER_READ_BIT,VK_ACCESS_TRANSFER_READ_BIT);
                            { VkBufferImageCopy cp=full_bic(mvw,mvh); vkCmdCopyImageToBuffer(cmdF,ofp.cand_image(),VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,hC2_b[f_gen].buf,1,&cp); }
                            img_barrier(cmdF,ofp.cand_image(),VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_ACCESS_TRANSFER_READ_BIT,VK_ACCESS_SHADER_READ_BIT);
                        }
                    }
                    // ── STAGE-85: snapshot this pair's context (the deferred consume must NOT read the live
                    // loop locals, which advance to the next pair before it runs). ───────────────────────
                    FwdPend cur_pc{}; cur_pc.valid=true; cur_pc.f_gen=f_gen; cur_pc.cur_c=cur_c; cur_pc.s=s;
                    cur_pc.span=span; cur_pc.N_use=N_use; cur_pc.prv_f=prv_f; cur_pc.cur_f=cur_f;
                    cur_pc.have_prev_f=have_prev_f; cur_pc.tp0=tp0;
                    vkEndCommandBuffer(cmdF);
                    if(!fwd_pipe){
                        // ── SERIAL (default): submit + block, then consume THIS pair immediately. The fence is
                        // already signalled by submit_wait, so consume_wap's wait is a no-op — byte-identical to
                        // pre-85 (record → submit_wait → CPU tail → publish, in order, allow_bwd=true).
                        cur_pc.tf0=now_ms();                     // pre-submit_wait anchor (the pre-85 tf0 position)
                        if(cfg.latency_trace && lt_pickup_now>0.0){ const double pf=(cur_pc.tf0-lt_pickup_now)*1000.0;   // STAGE-112: the WAP-serial pre_flow = pickup→flow-submit (upload+unpack+flow-record+ring-guard spin) = the build−fsub gap this trace localizes
                            if(pf>0.0&&pf<2000000.0){ const uint64_t pv=lt_preflow_us.load(); lt_preflow_us.store(pv?(uint64_t)((double)pv*0.8+pf*0.2):(uint64_t)pf); } }
                        flow_submit_wait(cmdF,fF);   // CR2/CR3: FD-route the WAP-serial flow submit to A.q2 under single_gpu (was submit_wait(B,…))
                        cur_pc.fwd_fence=VK_NULL_HANDLE;          // already waited → consume_wap skips the wait (uses pc.tf0)
                        // toggle loop-progression BEFORE the consume publishes (matches the pre-85 shared tail).
                        cur_f=prv_f; have_prev_f=true; prev_ingest_cseq=cur_c; ++g_seq;
                        consume_wap(cur_pc, /*allow_bwd=*/true);
                    } else {
                        // ── PIPELINED (--fwd-pipeline): submit NO-WAIT (cur's GPU match runs on B.q), then
                        // consume the PENDING pair (N-1) — its CPU tail overlaps cur's GPU. cur is held in
                        // `pend` for the NEXT iteration. allow_bwd=false: the deferred consume must not touch
                        // ofp/Bframe (cur's record already reused them) — the field runs bwd-skip:100% anyway.
                        flow_submit_nowait(cmdF,fF);   // CR2/CR3: FD-route the WAP-pipelined no-wait flow submit to A.q2 under single_gpu (was vkResetFences(B.dev,…)+vkQueueSubmit(B.q,…))
                        cur_pc.fwd_fence=fF;
                        // toggle loop-progression for the NEXT record (cur's frame becomes the next pair's prev).
                        cur_f=prv_f; have_prev_f=true; prev_ingest_cseq=cur_c; ++g_seq;
                        if(pend.valid) consume_wap(pend, /*allow_bwd=*/false);   // N-1 CPU ‖ N GPU
                        pend=cur_pc;
                    }
                } else if(have_prev_f&&NI_use>=1){
                    const float t1=1.0f/(float)N_use;
                    if(fg_on_prim_f&&pfg_enabled){
                        vkEndCommandBuffer(cmdB); flow_submit_wait(cmdB,fB);   // CR2/CR3: FD-route (this whole branch is DEAD under single_gpu — pfg_enabled false — but route for consistency)
                        vkResetCommandBuffer(cmdA_fg,0);
                        VkCommandBufferBeginInfo bi2{}; bi2.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO; vkBeginCommandBuffer(cmdA_fg,&bi2);
                        (void)ofpA.record_optical_flow(cmdA_fg,AframeA[prv_f].view,AframeA[cur_f].view,CinterpA.view,t1);
                        img_barrier(cmdA_fg,CinterpA.img,VK_IMAGE_LAYOUT_GENERAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT);
                        { VkBufferImageCopy cp=full_bic(WW,WH); vkCmdCopyImageToBuffer(cmdA_fg,CinterpA.img,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,hI_a[f_gen][0].buf,1,&cp); }
                        img_barrier(cmdA_fg,CinterpA.img,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_IMAGE_LAYOUT_GENERAL,VK_ACCESS_TRANSFER_READ_BIT,VK_ACCESS_SHADER_WRITE_BIT);
                        for(int k=1;k<NI_use;++k){
                            (void)ofpA.record_warp_only(cmdA_fg,float(k+1)/(float)N_use);
                            img_barrier(cmdA_fg,CinterpA.img,VK_IMAGE_LAYOUT_GENERAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT);
                            { VkBufferImageCopy cp=full_bic(WW,WH); vkCmdCopyImageToBuffer(cmdA_fg,CinterpA.img,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,hI_a[f_gen][k].buf,1,&cp); }
                            img_barrier(cmdA_fg,CinterpA.img,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_IMAGE_LAYOUT_GENERAL,VK_ACCESS_TRANSFER_READ_BIT,VK_ACCESS_SHADER_WRITE_BIT);
                        }
                        vkEndCommandBuffer(cmdA_fg); submit_wait_q2(A,cmdA_fg,fA_fg);   // CRASH-FIX: route F's primary-flow off A.q (P-exclusive) to A.q2 (same-family, lock-free; cmdA_fg is A.pool-bound). A.q2!=A.q guaranteed when pfg_enabled survived (Edit 5 gate).
                    } else {
                        (void)ofp.record_optical_flow(cmdB,Bframe[prv_f].view,Bframe[cur_f].view,Cinterp.view,t1);
                        // STAGE-40a: enqueue the slow x4 leg (sbI[k]→hI_b[gen][k]) on B.q2 WITHOUT
                        // waiting inline. The caller already waited fB (the B.q copy Cinterp→sbI[k]),
                        // so sbI[k]'s content is host-visible-ordered before this q2 submit — no
                        // semaphore needed (CPU fence-submit ordering). The fence tfb[k] is collected
                        // at SET END before the set is published to P.
                        auto q2_copy_out=[&](int k){
                            vkResetCommandBuffer(cmdB2[k],0);
                            VkCommandBufferBeginInfo bq{}; bq.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO; vkBeginCommandBuffer(cmdB2[k],&bq);
                            VkBufferCopy bc{0,0,VkDeviceSize(WW)*WH*4u}; vkCmdCopyBuffer(cmdB2[k],sbI[k].buf,hI_b[f_gen][k].buf,1,&bc);
                            vkEndCommandBuffer(cmdB2[k]);
                            vkResetFences(B.dev,1,&tfb[k]);
                            VkSubmitInfo si{}; si.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO; si.commandBufferCount=1; si.pCommandBuffers=&cmdB2[k];
                            vkQueueSubmit(B.q2,1,&si,tfb[k]);
                        };
                        if(use_mv_smooth){
                            // STAGE-35-R2: EMA-smooth the MV field in place, then RE-warp k=0 with
                            // the smoothed field (the record_optical_flow warp above used the raw
                            // field — discarded). All interps in the set then read one smoothed MV.
                            img_barrier(cmdB,ofp.motion_image(),VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_IMAGE_LAYOUT_GENERAL,VK_ACCESS_SHADER_READ_BIT,VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT);
                            vkCmdBindPipeline(cmdB,VK_PIPELINE_BIND_POINT_COMPUTE,mvsm.pipe);
                            vkCmdBindDescriptorSets(cmdB,VK_PIPELINE_BIND_POINT_COMPUTE,mvsm.layout,0,1,&mvsm.set,0,nullptr);
                            // 35-R2b: pass span — the shader EMAs in VELOCITY space (mv/span) so
                            // a span-2 pair's 2x-larger raw MV doesn't corrupt the running average.
                            struct{float alpha;float cut;uint32_t w;uint32_t h;float span;}pcm{cfg.mv_smooth,kMvCut,mvw_f,mvh_f,(float)span};
                            vkCmdPushConstants(cmdB,mvsm.layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(pcm),&pcm);
                            vkCmdDispatch(cmdB,(mvw_f+7)/8,(mvh_f+7)/8,1);
                            img_barrier(cmdB,ofp.motion_image(),VK_IMAGE_LAYOUT_GENERAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT);
                            img_barrier(cmdB,Cinterp.img,VK_IMAGE_LAYOUT_GENERAL,VK_IMAGE_LAYOUT_GENERAL,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_SHADER_WRITE_BIT);
                            (void)ofp.record_warp_only(cmdB,t1);
                        }
                        img_barrier(cmdB,Cinterp.img,VK_IMAGE_LAYOUT_GENERAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT);
                        // STAGE-40a: k=0 copy-out → VRAM staging (sbI[0]) when overlapping; else
                        // the original direct copy Cinterp→hI_b (serial fallback).
                        { VkBufferImageCopy cp=full_bic(WW,WH); vkCmdCopyImageToBuffer(cmdB,Cinterp.img,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,(b_q2_split?sbI[0].buf:hI_b[f_gen][0].buf),1,&cp); }
                        img_barrier(cmdB,Cinterp.img,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_IMAGE_LAYOUT_GENERAL,VK_ACCESS_TRANSFER_READ_BIT,VK_ACCESS_SHADER_WRITE_BIT);
                        const double tf0=now_ms(); vkEndCommandBuffer(cmdB); flow_submit_wait(cmdB,fB);   // CR2/CR3: FD-route the non-WAP k=0 flow submit (was submit_wait(B,…))
                        if(cfg.latency_trace && lt_pickup_now>0.0){ const double pf=(tf0-lt_pickup_now)*1000.0;   // STAGE-112: pre_flow = pickup→tf0 = the F non-compute window (upload+unpack+record+spin), the build−fsub gap
                            if(pf>0.0&&pf<2000000.0){ const uint64_t pv=lt_preflow_us.load(); lt_preflow_us.store(pv?(uint64_t)((double)pv*0.8+pf*0.2):(uint64_t)pf); } }
                        if(b_q2_split) q2_copy_out(0);   // launch the x4 leg on B.q2 (overlapped, no inline wait) — b_q2_split false under single_gpu (B null), so dead
                        if(cfg.fg_auto){ const double dt=now_ms()-tf0; t_fuse_ema=t_fuse_ema>0.0?t_fuse_ema*0.8+dt*0.2:dt; }
                        for(int k=1;k<NI_use;++k){
                            vkResetCommandBuffer(cmdB,0);
                            VkCommandBufferBeginInfo bi2{}; bi2.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO; vkBeginCommandBuffer(cmdB,&bi2);
                            (void)ofp.record_warp_only(cmdB,float(k+1)/(float)N_use);
                            img_barrier(cmdB,Cinterp.img,VK_IMAGE_LAYOUT_GENERAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT);
                            // STAGE-40a: copy-out → VRAM staging (sbI[k]) when overlapping; else direct.
                            { VkBufferImageCopy cp=full_bic(WW,WH); vkCmdCopyImageToBuffer(cmdB,Cinterp.img,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,(b_q2_split?sbI[k].buf:hI_b[f_gen][k].buf),1,&cp); }
                            img_barrier(cmdB,Cinterp.img,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_IMAGE_LAYOUT_GENERAL,VK_ACCESS_TRANSFER_READ_BIT,VK_ACCESS_SHADER_WRITE_BIT);
                            const double tw0=now_ms(); vkEndCommandBuffer(cmdB); flow_submit_wait(cmdB,fB);   // CR2/CR3: FD-route the non-WAP k>0 warp submit (was submit_wait(B,…))
                            if(b_q2_split) q2_copy_out(k);   // overlap the x4 leg on B.q2 with the next warp — dead under single_gpu
                            if(cfg.fg_auto){ const double dt=now_ms()-tw0; t_warp_ema=t_warp_ema>0.0?t_warp_ema*0.8+dt*0.2:dt; }
                            // STAGE-39b diagnostic (dump mode only): CPU-diff this copy vs the
                            // previous one right after the fence — distinct phases MUST differ;
                            // ~0 means the warp write was lost below the app.
                            if(cfg.dump_n>0){
                                // STAGE-40a: the q2 copy-outs are in flight in overlap mode — the
                                // dbg diff reads hostI directly, so settle this k and the previous
                                // one's transfers first (diagnostic path only; the production path
                                // collects these at SET END).
                                if(b_q2_split){ vkWaitForFences(FD.dev,1,&tfb[k],VK_TRUE,UINT64_MAX); vkWaitForFences(FD.dev,1,&tfb[k-1],VK_TRUE,UINT64_MAX); }   // CR2: FD.dev (b_q2_split false under single_gpu — defensive)
                                // dense diff over the operator's duck rect (x 460-800, y 180-560)
                                const uint8_t* a8=(const uint8_t*)hostI[f_gen][k-1];
                                const uint8_t* b8=(const uint8_t*)hostI[f_gen][k];
                                uint64_t acc=0;
                                for(uint32_t y=180;y<560&&y<WH;++y){
                                    const size_t ro=((size_t)y*WW+460u)*4u; const size_t n=(800u-460u)*4u;
                                    for(size_t i=0;i<n;++i){ const uint8_t av=a8[ro+i],bv=b8[ro+i]; acc+=(uint64_t)((av>bv)?(av-bv):(bv-av)); }
                                }
                                uint64_t sum=0;
                                for(uint32_t y=180;y<560&&y<WH;++y){
                                    const size_t ro=((size_t)y*WW+460u)*4u; const size_t n=(800u-460u)*4u;
                                    for(size_t i=0;i<n;++i) sum+=b8[ro+i];
                                }
                                std::printf("[ra] dbg F warp c%llu k=%d t=%.3f duckdiff=%.3f ducksum=%llu\n",
                                    (unsigned long long)cur_c,k,(double)(k+1)/(double)N_use,
                                    (double)acc/(380.0*340.0*4.0),(unsigned long long)sum);
                            }
                        }
                    }
                } else {
                    const double tf0=now_ms(); vkEndCommandBuffer(cmdB); flow_submit_wait(cmdB,fB);   // CR2/CR3: FD-route the non-WAP no-prev/NI<1 flow submit (was submit_wait(B,…))
                    if(cfg.latency_trace && lt_pickup_now>0.0){ const double pf=(tf0-lt_pickup_now)*1000.0;   // STAGE-112: pre_flow = pickup→tf0 = the F non-compute window (the build−fsub gap)
                        if(pf>0.0&&pf<2000000.0){ const uint64_t pv=lt_preflow_us.load(); lt_preflow_us.store(pv?(uint64_t)((double)pv*0.8+pf*0.2):(uint64_t)pf); } }
                    if(cfg.fg_auto){ const double dt=now_ms()-tf0; t_fuse_ema=t_fuse_ema>0.0?t_fuse_ema*0.8+dt*0.2:dt; }
                }
                // STAGE-40a: SET END — before publishing the set to P, collect ALL the
                // overlapped q2 copy-outs (tfb[0..NI_use-1]). P reads hostI right after
                // the f_seq bump below; nothing may still be in flight. Only the B-else
                // overlapped path submitted them (not pfg, not the NI_use<1 path); in dump
                // mode tfb[k]/tfb[k-1] were already settled inline but re-waiting is a no-op.
                if(b_q2_split&&have_prev_f&&NI_use>=1&&!(fg_on_prim_f&&pfg_enabled)&&!use_wap){
                    for(int k=0;k<NI_use;++k) vk_live(vkWaitForFences(FD.dev,1,&tfb[k],VK_TRUE,UINT64_MAX));   // P0: catch a TDR on the flow — CR2: FD.dev (b_q2_split false under single_gpu — defensive)
                }
                // STAGE-85: the WAP path advances its own loop-progression (cur_f/have_prev_f/prev_ingest_cseq
                // per RECORD) AND does its own publish + live_n in consume_wap (deferred one pair when
                // --fwd-pipeline). Skip this shared tail for WAP so it is never double-published; the non-WAP
                // (else-if / final-else) paths keep it byte-unchanged.
                if(!use_wap){
                cur_f=prv_f; have_prev_f=true;
                prev_ingest_cseq=cur_c;   // R1: this ingest becomes the next pair's previous
                // 34e/35-R1/R4: publish this set's pair identity + span + N BEFORE the f_seq
                // bump that makes the set visible to P (the seq_cst fetch_add/load orders them).
                f_pair_cseq_a[f_gen]=cur_c; f_pair_slot_a[f_gen]=s; f_pair_tcap_a[f_gen]=c_slots[s].t_cap_ms;
                f_pair_span_a[f_gen]=span; f_pair_n_a[f_gen]=N_use;
                stat_cons.fetch_add(1);
                f_seq.fetch_add(1);
                f_cv.notify_all();
                if(cfg.latency_trace){ const double tc=f_pair_tcap_a[f_gen];   // STAGE-112: tcap→publish = the full F-side freshage contribution (pickup+ingest+convert+build); detect derived in stats
                    if(tc>0.0){ const double fp=(now_ms()-tc)*1000.0; if(fp>0.0&&fp<2000000.0){
                        const uint64_t pv=lt_fpub_us.load(); lt_fpub_us.store(pv?(uint64_t)((double)pv*0.8+fp*0.2):(uint64_t)fp); } } }
                // STAGE-35-R4/R4b: re-evaluate live N from measured capacity (auto only).
                // Decision lags one set (this set already used N_use) — applies to the NEXT set.
                if(cfg.fg_auto){
                    const double Tsrc=(double)src_interval_us.load()/1000.0;
                    const double pres=(double)present_cost_us.load()/1000.0;
                    span_ema=span_ema*0.9+(double)(span>1?span-1:0)*0.1;
                    if(dwell_sets>0) --dwell_sets;
                    if(span_ema>0.25&&live_n_f>1&&dwell_sets==0){
                        // R4b falling-behind: F is persistently skipping sources — shed warp work.
                        live_n_f-=1; span_ema=0.0; up_streak=0; deg_streak=0; dwell_sets=90;
                    } else if(dwell_sets==0){
                        const double set_live=t_fuse_ema+(double)std::max(0,live_n_f-2)*t_warp_ema;
                        const double pres_live=(double)live_n_f*pres;
                        if(live_n_f>1&&(set_live>0.90*Tsrc||(pres>0.0&&pres_live>1.2*Tsrc))){
                            up_streak=0;
                            if(++deg_streak>=3){ live_n_f-=1; deg_streak=0; dwell_sets=90; }
                        } else {
                            deg_streak=0;
                            if(live_n_f<cfg.fg_factor){
                                const int n=live_n_f+1;
                                const double set_t=t_fuse_ema+(double)std::max(0,n-2)*t_warp_ema;
                                const double pres_t=(double)n*pres;
                                if(set_t<=0.65*Tsrc&&(pres<=0.0||pres_t<=Tsrc)){
                                    if(++up_streak>=45){ live_n_f=n; up_streak=0; dwell_sets=90; }
                                } else up_streak=0;
                            }
                        }
                    }
                    live_n_atomic.store(live_n_f);
                }
                }   // !use_wap (STAGE-85 shared-tail guard)
                (void)fg_on_prim_f;
            }
            // STAGE-85: drain the last deferred pair at loop exit so its CPU tail + publish still run (the
            // pipeline holds one pair back). vkDeviceWaitIdle(B) at shutdown will have drained the GPU; here
            // we only need the CPU consume + the final f_seq bump. allow_bwd=false (the pipeline never bwd'd).
            if(cfg.fwd_pipeline && use_wap && pend.valid){ consume_wap(pend, /*allow_bwd=*/false); pend.valid=false; }
}
// Made with my soul - Swately <3
