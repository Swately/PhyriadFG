// PhyriadFG warp_blend layer (STEP 4b — PURE RELOCATION from src/core/main.cpp; no logic
// change). Bodies of the WARP/BLEND/QUALITY factories + CPU stat helpers declared in
// warp_blend/warp_blend.hpp. All were file-static in main.cpp; they get external linkage here
// (main calls them). No *_spv.hpp includes (the factories take the SPIR-V as a parameter).
#include "warp_blend/warp_blend.hpp"
#include <cstddef>    // size_t (matte_mass_count / gme_dispct_from_mask)

// STAGE-59: the anchored matte mass — count of dissidence-mask blocks the shader would classify OBJECT.
// The mask stores min(255, round(16·r)); the shader reads v = byte/255 and classifies OBJECT when
// v > matte_thresh, i.e. byte > matte_thresh·255. We mirror that test on the CPU EXACTLY (the same
// integer comparison the shader's float comparison reduces to, with a guard against the boundary float).
// thr_byte = matte_thresh·255 (a float; a block counts iff its byte strictly exceeds it). Returns the
// count over all mvw·mvh blocks (the prev- or cur-anchored mass, in block-grid units).
uint32_t matte_mass_count(const uint8_t* mask, uint32_t mvw, uint32_t mvh, float matte_thresh){
    if(!mask) return 0u;
    const float thr_byte = matte_thresh * 255.0f;
    const size_t n = (size_t)mvw * mvh; uint32_t cnt = 0u;
    for(size_t i=0;i<n;++i) if((float)mask[i] > thr_byte) ++cnt;
    return cnt;
}
// ── IDEA-1 (gme-gpu): derive the dis% stat from a (GPU-produced) mask. The CPU gme_fit_affine returns
// dis% = the fraction of blocks with r>4px, counted from the raw residual BEFORE the change gate. The
// GPU path has only the QUANTIZED, gated mask byte = min(255, round(16·r)) (0 if gated out), so we
// approximate r>4px by byte>64 (16·4 = 64). The change gate zeros some byte>64 blocks → this slightly
// UNDER-counts vs the CPU's pre-gate r>4 when the gate is active. It is a STAT print only (the dis:NN%
// line + the inertia/HUD threshold display) — NOT load-bearing for the warp or object_repair (those read
// the mask bytes directly). Documented as an approximation; the mask itself is the verified artifact.
double gme_dispct_from_mask(const uint8_t* mask, uint32_t mvw, uint32_t mvh){
    if(!mask) return 0.0;
    const size_t n=(size_t)mvw*mvh; uint64_t dis=0;
    for(size_t i=0;i<n;++i) if(mask[i] > 64u) ++dis;
    return n ? 100.0*(double)dis/(double)n : 0.0;
}

// STAGE-G1 (P0): iGPU contour-field pipeline (2 SSBO bindings: src=hostR RGBA8, dst=hostFIELD). Mirrors
// cpipe_create with 2 bindings; the 2nd G.q2 dispatch after igpu_convert_pack. Push constants {w,h,edge_thr,pad}.
bool fpipe_create(VDev& d,VkBuffer src,VkDeviceSize src_b,VkBuffer dst,VkDeviceSize dst_b,const std::vector<uint32_t>& spv,FieldPipe& p){
    const VkDescriptorSetLayoutBinding bd[2]={{0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr},{1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr}};
    VkDescriptorSetLayoutCreateInfo dl{}; dl.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO; dl.bindingCount=2; dl.pBindings=bd; vkCreateDescriptorSetLayout(d.dev,&dl,nullptr,&p.dsl);
    VkPushConstantRange pcr{}; pcr.stageFlags=VK_SHADER_STAGE_COMPUTE_BIT; pcr.size=16; // {w,h,edge_thr,pad}
    VkPipelineLayoutCreateInfo pl{}; pl.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO; pl.setLayoutCount=1; pl.pSetLayouts=&p.dsl; pl.pushConstantRangeCount=1; pl.pPushConstantRanges=&pcr; vkCreatePipelineLayout(d.dev,&pl,nullptr,&p.layout);
    VkShaderModuleCreateInfo mci{}; mci.sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO; mci.codeSize=spv.size()*sizeof(uint32_t); mci.pCode=spv.data(); VkShaderModule mod; vkCreateShaderModule(d.dev,&mci,nullptr,&mod);
    VkPipelineShaderStageCreateInfo stg{}; stg.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; stg.stage=VK_SHADER_STAGE_COMPUTE_BIT; stg.module=mod; stg.pName="main";
    VkComputePipelineCreateInfo cp{}; cp.sType=VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO; cp.stage=stg; cp.layout=p.layout; vkCreateComputePipelines(d.dev,VK_NULL_HANDLE,1,&cp,nullptr,&p.pipe); vkDestroyShaderModule(d.dev,mod,nullptr);
    const VkDescriptorPoolSize psz[1]={{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,2}};
    VkDescriptorPoolCreateInfo pi{}; pi.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO; pi.maxSets=1; pi.poolSizeCount=1; pi.pPoolSizes=psz; vkCreateDescriptorPool(d.dev,&pi,nullptr,&p.pool);
    VkDescriptorSetAllocateInfo dai{}; dai.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO; dai.descriptorPool=p.pool; dai.descriptorSetCount=1; dai.pSetLayouts=&p.dsl; vkAllocateDescriptorSets(d.dev,&dai,&p.set);
    VkDescriptorBufferInfo bi0{}; bi0.buffer=src; bi0.offset=0; bi0.range=src_b;
    VkDescriptorBufferInfo bi1{}; bi1.buffer=dst; bi1.offset=0; bi1.range=dst_b;
    const VkWriteDescriptorSet wds[2]={
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,nullptr,p.set,0,0,1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,nullptr,&bi0,nullptr},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,nullptr,p.set,1,0,1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,nullptr,&bi1,nullptr}};
    vkUpdateDescriptorSets(d.dev,2,wds,0,nullptr); return p.pipe!=VK_NULL_HANDLE;
}
void fpipe_destroy(VDev& d,FieldPipe& p){ if(p.pool)vkDestroyDescriptorPool(d.dev,p.pool,nullptr); if(p.pipe)vkDestroyPipeline(d.dev,p.pipe,nullptr); if(p.layout)vkDestroyPipelineLayout(d.dev,p.layout,nullptr); if(p.dsl)vkDestroyDescriptorSetLayout(d.dev,p.dsl,nullptr); p=FieldPipe{}; }

// STAGE-A-FILL (P1): the A-side field VISUALIZER pipeline (3 bindings: 0=u_warp rgba8 wapOutA read-write
// in-place [STORAGE], 1=u_field r32ui wapFIELDA [STORAGE], 2=u_mv rg16f wapMVA [SAMPLED] — for advecting the
// field to the present phase). Mirrors fpipe_create but image-bound; the dispatch lives between the warp
// dispatch and the present blit (same A.q submit). Push {w,h,strength,edge_norm,t,pad}.
bool fillpipe_create(VDev& d,VkImageView warp_v,VkImageView field_v,VkImageView mv_v,const std::vector<uint32_t>& spv,FillPipe& p){
    VkSamplerCreateInfo si{}; si.sType=VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO; si.magFilter=VK_FILTER_LINEAR; si.minFilter=VK_FILTER_LINEAR; si.addressModeU=si.addressModeV=si.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE; vkCreateSampler(d.dev,&si,nullptr,&p.samp);
    const VkDescriptorSetLayoutBinding bd[3]={{0,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr},{1,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr},{2,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr}};
    VkDescriptorSetLayoutCreateInfo dl{}; dl.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO; dl.bindingCount=3; dl.pBindings=bd; vkCreateDescriptorSetLayout(d.dev,&dl,nullptr,&p.dsl);
    VkPushConstantRange pcr{}; pcr.stageFlags=VK_SHADER_STAGE_COMPUTE_BIT; pcr.size=24; // {w,h,fill_strength,edge_norm,t,pad}
    VkPipelineLayoutCreateInfo pl{}; pl.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO; pl.setLayoutCount=1; pl.pSetLayouts=&p.dsl; pl.pushConstantRangeCount=1; pl.pPushConstantRanges=&pcr; vkCreatePipelineLayout(d.dev,&pl,nullptr,&p.layout);
    VkShaderModuleCreateInfo mci{}; mci.sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO; mci.codeSize=spv.size()*sizeof(uint32_t); mci.pCode=spv.data(); VkShaderModule mod; vkCreateShaderModule(d.dev,&mci,nullptr,&mod);
    VkPipelineShaderStageCreateInfo stg{}; stg.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; stg.stage=VK_SHADER_STAGE_COMPUTE_BIT; stg.module=mod; stg.pName="main";
    VkComputePipelineCreateInfo cp{}; cp.sType=VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO; cp.stage=stg; cp.layout=p.layout; vkCreateComputePipelines(d.dev,VK_NULL_HANDLE,1,&cp,nullptr,&p.pipe); vkDestroyShaderModule(d.dev,mod,nullptr);
    const VkDescriptorPoolSize psz[2]={{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,2},{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1}};
    VkDescriptorPoolCreateInfo pi{}; pi.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO; pi.maxSets=1; pi.poolSizeCount=2; pi.pPoolSizes=psz; vkCreateDescriptorPool(d.dev,&pi,nullptr,&p.pool);
    VkDescriptorSetAllocateInfo dai{}; dai.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO; dai.descriptorPool=p.pool; dai.descriptorSetCount=1; dai.pSetLayouts=&p.dsl; vkAllocateDescriptorSets(d.dev,&dai,&p.set);
    VkDescriptorImageInfo ii0{}; ii0.imageView=warp_v;  ii0.imageLayout=VK_IMAGE_LAYOUT_GENERAL;
    VkDescriptorImageInfo ii1{}; ii1.imageView=field_v; ii1.imageLayout=VK_IMAGE_LAYOUT_GENERAL;
    VkDescriptorImageInfo ii2{}; ii2.sampler=p.samp; ii2.imageView=mv_v; ii2.imageLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    const VkWriteDescriptorSet wds[3]={
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,nullptr,p.set,0,0,1,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,&ii0,nullptr,nullptr},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,nullptr,p.set,1,0,1,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,&ii1,nullptr,nullptr},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,nullptr,p.set,2,0,1,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,&ii2,nullptr,nullptr}};
    vkUpdateDescriptorSets(d.dev,3,wds,0,nullptr); return p.pipe!=VK_NULL_HANDLE;
}
void fillpipe_destroy(VDev& d,FillPipe& p){ if(p.pool)vkDestroyDescriptorPool(d.dev,p.pool,nullptr); if(p.pipe)vkDestroyPipeline(d.dev,p.pipe,nullptr); if(p.layout)vkDestroyPipelineLayout(d.dev,p.layout,nullptr); if(p.dsl)vkDestroyDescriptorSetLayout(d.dev,p.dsl,nullptr); if(p.samp)vkDestroySampler(d.dev,p.samp,nullptr); p=FillPipe{}; }

// STAGE-48: a 6th sampled binding (mvb_v = backward MV) and a 32-byte push (was 28: + occl_thresh).
// The binding is ALWAYS present + written (a valid RG16F image) so the descriptor set is complete
// regardless of --bidir; the shader reads it only when occl_thresh>0 (bidir on). When bidir is off
// the caller passes the forward MV view here as a harmless placeholder (unread).
// STAGE-50: the push grows to 36 bytes (+ div_eps); div_eps>0 (fill-div on, requires bidir) directs
// the bidir neither-class disocclusion sliver by the forward-MV divergence sign. 0 = byte-identical.
// STAGE-49: the push grows to 40 bytes (+ rescue_on); rescue_on>0.5 (rescue on, requires commit) tries
// the 8 neighbor block MVs before a pixel falls to commit. 0 = byte-identical.
// STAGE-49c: the push grows to 44 bytes (+ mv_guided); mv_guided>0.5 (color-guided on, requires WAP)
// makes the PRIMARY MV fetch a bilateral (per-corner color-membership) upsample. 0 = byte-identical
// LINEAR. The host encodes the membership band as mv_guided = 1.0 + sim (so >0.5 gates AND carries it).
// STAGE-52: the push grows to 72 bytes (+ gme_on + 6 affine params {a,b,c,d,e,f}); gme_on>0.5 (the
// frame-holon armed, requires WAP + a valid per-pair fit) adds the global affine model as a rescue
// candidate AND directs the fill-div revealed case to the background MV. 0 = byte-identical (the whole
// gme block — rescue candidate + fill-div assist — is gated on gme_on>0.5, a uniform push value).
// STAGE-53: a 7th sampled binding (dis_v = wapDISA, the R8 dissidence mask) and an 80-byte push (was
// 72: + matte_on + matte_thresh). The binding is ALWAYS present + written (a valid sampled view) so the
// descriptor set is complete regardless of --matte/--gme; the shader reads it only when matte_on>0.5.
// When gme is off the caller passes the forward MV view here as a harmless placeholder (unread) — the
// same pattern binding 5 uses for MV_bwd.
// STAGE-53b: an 8th sampled binding (disb_v = wapDISBA, the R8 backward/cur-anchored dissidence mask).
// SAME ALWAYS-present-+-written discipline (a valid sampled view always bound); the shader reads it only
// when matte_on>0.5 (and matte requires --bidir, so the real bwd mask is always bound when matte runs).
// The PUSH is UNCHANGED at 80 bytes — the bwd model is published in the F→P channel but NOT pushed to the
// shader this stage; the bwd mask (binding 7) carries the leading-edge information.
// STAGE-57: a 9th sampled binding (per_v = wapPERA, the R8 inertia persistence mask). SAME always-present-
// +-written discipline (a valid sampled view always bound — wapPERA is created unconditionally with WAP);
// the shader reads it only when inertia_thresh>0.5. The push grows 84→88B (+ inertia_thresh, the 22nd float).
// STAGE-59: a 10th binding (binding 9 = mass_buf, a host-visible 4-byte STORAGE buffer — the presented-
// matte-mass counter). The warp atomicAdds the per-workgroup OBJECT-pixel count into it (one atomicAdd
// per workgroup, gated on matte_on>0.5). ALWAYS bound (a valid buffer always exists — created with WAP);
// the shader writes it only when matte runs. P resets it to 0 before each dispatch and reads it after the
// existing fBridge fence wait. The push is UNCHANGED (88B) — the counter is a buffer, not a push value.
// STAGE-62: the push grows 88→92B (+ crescent_on, the 23rd float). No new binding (the crescent fetch reuses
// the dis_fwd/dis_bwd the matte block already computed); crescent_on>0.5 AND matte_on>0.5 enables the
// mask-weighted background side-fetch in the matte BACKGROUND branch → byte-identical when off (the push
// flag is 0, set 1.0 by the host only when use_matte AND cfg.crescent).
// STAGE-64: the push grows 92→100B (+ appear_on, appear_band — the 24th/25th floats). No new binding (the
// appearance re-blend's one extra tap reuses the already-bound prev/cur real samplers); appear_on>0.5 AND
// commit_thresh>0 enables the appearance-band temporal re-blend on the committed value in the commit soft
// band → byte-identical when off (the flag is 0, set 1.0 by the host only when commit live AND cfg.appearance).
// STAGE-67: the push grows 100→104B (+ travel_on, the 26th float). No new binding (the traveling-silhouette
// occupancy reuses the dis_self/dis_gather the matte block already samples); travel_on>0.5 AND matte_on>0.5
// fades the matte's self crutches with phase so the silhouette TRAVELS (the swept-band residue dies) → the
// dis_fwd/dis_bwd the crescent reads stay the FULL max(self,gather) testimonies, only the occupancy is faded
// → byte-identical when off (the flag is 0, set 1.0 by the host only when use_matte governs AND cfg.travel).
// STAGE-71: the push grows 104→108B (+ contour_on, the 27th float). No new binding (the contour marriage
// reuses the two already-built layer colors + ONE near_real tap on the existing prev/cur samplers, band
// pixels only); contour_on>0.5 AND matte_on>0.5 enables the per-pixel boundary-band color-affinity
// arbitration at the matte composition site → byte-identical when off (the flag is 0, set 1.0 by the host
// only when use_matte governs AND cfg.contour). No new barriers — all ALU inside the existing matte branch.
// STAGE-77: an 11th binding (binding 10 = c2_v = wapC2A, the RGBA16F second-best candidate field) and a
// 120-byte push (was 116: + ambig_on, the 30th float). SAME always-present-+-placeholder discipline (a valid
// sampled view always bound — wapC2A when use_ambig, else wMV.view); the shader reads it only when ambig_on>0.5.
// The candidate field carries xy=runner-up MV, z=runner-up SAD; the warp arbitrates SAD TIES on background/
// texture-interior pixels (the periodic-texture aliasing class). Byte-identical when off (ambig_on=0, the host
// sets it 1.0 only under use_ambig; the binding is a placeholder + never sampled).
// STAGE-79: NO new binding (the member-commit boost reuses the dissidence masks bindings 6/7 the matte already
// samples); the push grows 120→124B (+ member_commit_on, the 31st float). member_commit_on>0.5 AND matte_on>0.5
// scales the warp-vs-blend selection's blend-fallback pressure down on object-interior pixels (the cross-fade
// ghost-step fix); byte-identical when off (the flag is 0, set 1.0 by the host only when use_matte governs AND
// cfg.member_commit). No new barriers — all ALU at the existing soft/hard selection site.
// STAGE-80: NO new binding, NO new tap (the commit-default flip reuses d_pixel + agreement_threshold already in
// scope at the selection site); the push grows 124→128B (+ commit_default_on, the 32nd float) — AT the spec-min
// push budget (Vulkan guarantees maxPushConstantsSize ≥ 128B for every conformant implementation; both target
// GPUs report 256B → legal, exactly met). commit_default_on>0.5 FLOORS the warp-vs-blend selection at the warp
// (warp IS the default; the fixed cross-fade only for evidenced garbage where d_pixel is large) — the cross-fade
// endgame. WARP-LEVEL (matte-independent): the host sets 1.0 iff use_commit_default = cfg.commit_default && use_wap.
// Byte-identical when off (the flag is 0). No new barriers — all ALU at the existing soft/hard selection site.
bool wap_create(VDev& d,VkImageView prev_v,VkImageView cur_v,VkImageView mv_v,VkImageView sad_v,VkImageView out_v,VkImageView mvb_v,VkImageView dis_v,VkImageView disb_v,VkImageView per_v,VkBuffer mass_buf,VkImageView c2_v,VkImageView field_v,VkImageView mvt_v,VkImageView prev_out_v,const std::vector<uint32_t>& spv,WapPipe& p){
    VkSamplerCreateInfo s{}; s.sType=VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO; s.magFilter=VK_FILTER_LINEAR; s.minFilter=VK_FILTER_LINEAR; s.addressModeU=s.addressModeV=s.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE; vkCreateSampler(d.dev,&s,nullptr,&p.samp);
    const VkDescriptorSetLayoutBinding bd[14]={
        {0,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_COMPUTE_BIT,&p.samp},
        {1,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_COMPUTE_BIT,&p.samp},
        {2,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_COMPUTE_BIT,&p.samp},
        {3,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_COMPUTE_BIT,&p.samp},
        {4,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr},
        {5,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_COMPUTE_BIT,&p.samp},   // STAGE-48 MV_bwd
        {6,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_COMPUTE_BIT,&p.samp},   // STAGE-53 dissidence_fwd (R8)
        {7,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_COMPUTE_BIT,&p.samp},   // STAGE-53b dissidence_bwd (R8)
        {8,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_COMPUTE_BIT,&p.samp},   // STAGE-57 persistence (R8)
        {9,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr},           // STAGE-59 mass counter
        {10,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_COMPUTE_BIT,&p.samp},  // STAGE-77 candidate (RGBA16F)
        {11,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr},           // STAGE-90 (P2b) iGPU contour field (R32_UINT)
        {12,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_COMPUTE_BIT,&p.samp},  // STAGE-94 (--vblend) next-pair MV target (RG16F)
        {13,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_COMPUTE_BIT,&p.samp}}; // STAGE-97 (--ts-smooth) prev-output history (RGBA8)
    VkDescriptorSetLayoutCreateInfo dl{}; dl.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO; dl.bindingCount=14; dl.pBindings=bd; vkCreateDescriptorSetLayout(d.dev,&dl,nullptr,&p.dsl);
    VkPushConstantRange pcr{}; pcr.stageFlags=VK_SHADER_STAGE_COMPUTE_BIT; pcr.size=212; // {res_ceil,conf_improv,agreement,t,soft_gate,commit_thresh,commit_real,occl_thresh,div_eps,rescue_on,mv_guided,gme_on,gme_a..gme_f,matte_on,matte_thresh,stasis_thresh,inertia_thresh,crescent_on,appear_on,appear_band,travel_on,contour_on,obj_crescent_on,phase_anchor_on,ambig_on,member_commit_on,commit_default_on,onepos_on,onepos_band,disoccl_commit_on,bg_snap_on,bg_snap_strength,bg_snap_norm,extrap,vblend_on,vblend_t0,vblend_strength,band_xfade,vblend_exact,ts_smooth,mc_on,mc_nperturb,mc_perturb,mc_disp,mc_edge,cam_lead.x,cam_lead.y,disoccl_hardpick} — STAGE-114 (+vec2 cam_lead appended LAST → 52 floats/208B; was 200B/50f at 98); STAGE-116 (+disoccl_hardpick appended LAST after cam_lead → 53 floats/212B; a float after the 8-aligned vec2 lands at offset 208, no pad). cam_lead is a GLSL vec2 std430-aligned to 8B; the preceding 50 floats put it at offset 200 (8-aligned → no pad), mirrored host-side as the two trailing floats {clx,cly}. The rig's maxPushConstantsSize is 256B (both target GPUs report 256 — see the STAGE-80 budget comment), so 208B is well within the device limit; the 128B figure is the Vulkan SPEC MINIMUM guarantee, NOT a project cap.
    VkPipelineLayoutCreateInfo pl{}; pl.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO; pl.setLayoutCount=1; pl.pSetLayouts=&p.dsl; pl.pushConstantRangeCount=1; pl.pPushConstantRanges=&pcr; vkCreatePipelineLayout(d.dev,&pl,nullptr,&p.layout);
    VkShaderModuleCreateInfo mci{}; mci.sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO; mci.codeSize=spv.size()*sizeof(uint32_t); mci.pCode=spv.data(); VkShaderModule mod; vkCreateShaderModule(d.dev,&mci,nullptr,&mod);
    VkPipelineShaderStageCreateInfo stg{}; stg.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; stg.stage=VK_SHADER_STAGE_COMPUTE_BIT; stg.module=mod; stg.pName="main";
    VkComputePipelineCreateInfo cp{}; cp.sType=VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO; cp.stage=stg; cp.layout=p.layout; vkCreateComputePipelines(d.dev,VK_NULL_HANDLE,1,&cp,nullptr,&p.pipe); vkDestroyShaderModule(d.dev,mod,nullptr);
    const VkDescriptorPoolSize psz[3]={{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,11},{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,2},{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1}};  // STAGE-59/77/90/94/97: 11 samplers (+ candidate binding 10 + vblend MV-target binding 12 + ts-smooth prev-output binding 13) + 2 storage images (binding 4 wOut, binding 11 field) + the mass counter SSBO
    VkDescriptorPoolCreateInfo pi{}; pi.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO; pi.maxSets=1; pi.poolSizeCount=3; pi.pPoolSizes=psz; vkCreateDescriptorPool(d.dev,&pi,nullptr,&p.pool);
    VkDescriptorSetAllocateInfo dai{}; dai.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO; dai.descriptorPool=p.pool; dai.descriptorSetCount=1; dai.pSetLayouts=&p.dsl; vkAllocateDescriptorSets(d.dev,&dai,&p.set);
    VkDescriptorImageInfo iprev{}; iprev.sampler=p.samp; iprev.imageView=prev_v; iprev.imageLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkDescriptorImageInfo icur{};  icur.sampler=p.samp;  icur.imageView=cur_v;   icur.imageLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkDescriptorImageInfo imv{};   imv.sampler=p.samp;   imv.imageView=mv_v;     imv.imageLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkDescriptorImageInfo isad{};  isad.sampler=p.samp;  isad.imageView=sad_v;   isad.imageLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkDescriptorImageInfo iout{};  iout.imageView=out_v; iout.imageLayout=VK_IMAGE_LAYOUT_GENERAL;
    VkDescriptorImageInfo imvb{};  imvb.sampler=p.samp;  imvb.imageView=mvb_v;   imvb.imageLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkDescriptorImageInfo idis{};  idis.sampler=p.samp;  idis.imageView=dis_v;   idis.imageLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;  // STAGE-53 dissidence_fwd
    VkDescriptorImageInfo idisb{}; idisb.sampler=p.samp; idisb.imageView=disb_v; idisb.imageLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; // STAGE-53b dissidence_bwd
    VkDescriptorImageInfo iper{};  iper.sampler=p.samp;  iper.imageView=per_v;   iper.imageLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;  // STAGE-57 persistence (R8)
    VkDescriptorBufferInfo imass{}; imass.buffer=mass_buf; imass.offset=0; imass.range=VK_WHOLE_SIZE;  // STAGE-59 mass counter SSBO
    VkDescriptorImageInfo ic2{};   ic2.sampler=p.samp;   ic2.imageView=c2_v;     ic2.imageLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;   // STAGE-77 candidate (RGBA16F)
    VkDescriptorImageInfo ifld{};  ifld.imageView=field_v; ifld.imageLayout=VK_IMAGE_LAYOUT_GENERAL;   // STAGE-90 (P2b) iGPU contour field (R32_UINT, imageLoad → GENERAL)
    VkDescriptorImageInfo imvt{};  imvt.sampler=p.samp;  imvt.imageView=mvt_v;   imvt.imageLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;  // STAGE-94 (--vblend) next-pair MV target (RG16F)
    VkDescriptorImageInfo ipout{}; ipout.sampler=p.samp; ipout.imageView=prev_out_v; ipout.imageLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;  // STAGE-97 (--ts-smooth) prev-output history (RGBA8)
    const VkWriteDescriptorSet wds[14]={
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,nullptr,p.set,0,0,1,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,&iprev,nullptr,nullptr},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,nullptr,p.set,1,0,1,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,&icur,nullptr,nullptr},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,nullptr,p.set,2,0,1,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,&imv,nullptr,nullptr},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,nullptr,p.set,3,0,1,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,&isad,nullptr,nullptr},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,nullptr,p.set,4,0,1,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,&iout,nullptr,nullptr},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,nullptr,p.set,5,0,1,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,&imvb,nullptr,nullptr},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,nullptr,p.set,6,0,1,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,&idis,nullptr,nullptr},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,nullptr,p.set,7,0,1,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,&idisb,nullptr,nullptr},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,nullptr,p.set,8,0,1,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,&iper,nullptr,nullptr},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,nullptr,p.set,9,0,1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,nullptr,&imass,nullptr},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,nullptr,p.set,10,0,1,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,&ic2,nullptr,nullptr},   // STAGE-77 candidate
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,nullptr,p.set,11,0,1,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,&ifld,nullptr,nullptr},  // STAGE-90 (P2b) contour field
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,nullptr,p.set,12,0,1,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,&imvt,nullptr,nullptr},   // STAGE-94 (--vblend) MV target
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,nullptr,p.set,13,0,1,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,&ipout,nullptr,nullptr}};  // STAGE-97 (--ts-smooth) prev-output history
    vkUpdateDescriptorSets(d.dev,14,wds,0,nullptr); return p.pipe!=VK_NULL_HANDLE;
}
void wap_destroy(VDev& d,WapPipe& p){ if(p.pool)vkDestroyDescriptorPool(d.dev,p.pool,nullptr); if(p.pipe)vkDestroyPipeline(d.dev,p.pipe,nullptr); if(p.layout)vkDestroyPipelineLayout(d.dev,p.layout,nullptr); if(p.dsl)vkDestroyDescriptorSetLayout(d.dev,p.dsl,nullptr); if(p.samp)vkDestroySampler(d.dev,p.samp,nullptr); p=WapPipe{}; }
