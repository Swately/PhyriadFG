// PhyriadFG warp_blend layer. Bodies of the WARP/BLEND/QUALITY factories + CPU stat helpers
// declared in warp_blend/warp_blend.hpp. The factories take the SPIR-V as a parameter, so this
// layer pulls no *_spv.hpp shader headers.
#include "warp_blend/warp_blend.hpp"
#include <cstddef>    // size_t (matte_mass_count / gme_dispct_from_mask)

// Anchored matte mass — count of dissidence-mask blocks the shader would classify OBJECT.
// The mask stores min(255, round(16·r)); the shader reads v = byte/255 and classifies OBJECT when
// v > matte_thresh, i.e. byte > matte_thresh·255. Mirrors that test on the CPU exactly (thr_byte =
// matte_thresh·255, a float; a block counts iff its byte strictly exceeds it). Returns the count
// over all mvw·mvh blocks, in block-grid units.
uint32_t matte_mass_count(const uint8_t* mask, uint32_t mvw, uint32_t mvh, float matte_thresh){
    if(!mask) return 0u;
    const float thr_byte = matte_thresh * 255.0f;
    const size_t n = (size_t)mvw * mvh; uint32_t cnt = 0u;
    for(size_t i=0;i<n;++i) if((float)mask[i] > thr_byte) ++cnt;
    return cnt;
}
// Derive the dis% stat from a GPU-produced mask. The mask byte = min(255, round(16·r)) (0 if gated
// out), so this approximates the fraction of blocks with r>4px by byte>64 (16·4 = 64). A stat print
// only (the dis:NN% line + the inertia/HUD threshold display) — NOT load-bearing for the warp or
// object_repair, which read the mask bytes directly. It is an approximation: the change gate zeros some
// byte>64 blocks, so it can slightly under-count when the gate is active.
double gme_dispct_from_mask(const uint8_t* mask, uint32_t mvw, uint32_t mvh){
    if(!mask) return 0.0;
    const size_t n=(size_t)mvw*mvh; uint64_t dis=0;
    for(size_t i=0;i<n;++i) if(mask[i] > 64u) ++dis;
    return n ? 100.0*(double)dis/(double)n : 0.0;
}

// iGPU contour-field pipeline (2 SSBO bindings: src=hostR RGBA8, dst=hostFIELD). The dispatch runs on
// G.q2 after igpu_convert_pack. Push constants {w,h,edge_thr,pad}.
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

// A-side field VISUALIZER pipeline (3 bindings: 0=u_warp rgba8 wapOutA read-write in-place [STORAGE],
// 1=u_field r32ui wapFIELDA [STORAGE], 2=u_mv rg16f wapMVA [SAMPLED] — for advecting the field to the
// present phase). Image-bound; the dispatch runs between the warp dispatch and the present blit on the
// same A.q submit. Push {w,h,strength,edge_norm,t,pad}.
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

// Optional bindings + push flags. Discipline: every optional binding is ALWAYS created, bound, and
// written — when its feature is off the caller binds a valid placeholder view/buffer (typically the
// forward MV view), so the descriptor set is complete regardless of the flags. The shader reads each
// only when its push gate is set, and every feature flag is byte-identical to off when 0/≤0.5 (the host
// sets each gate live only when its feature is armed). The gated bindings:
//   5  MV_bwd (RG16F)            — read when occl_thresh>0 (bidir)
//   6/7 dissidence fwd/bwd (R8)  — read when matte_on>0.5 (matte requires bidir, so the real bwd mask is
//                                  always bound when matte runs)
//   8  inertia persistence (R8)  — read when inertia_thresh>0.5
//   9  mass counter (host-visible 4-byte SSBO) — the warp atomicAdds the per-workgroup OBJECT-pixel
//      count (one atomicAdd/workgroup) only when matte runs; P zeroes it before the dispatch and reads
//      it after the fBridge fence wait
//   10 second-best candidate field (RGBA16F: xy=runner-up MV, z=runner-up SAD) — read when ambig_on>0.5
//      to arbitrate SAD ties on background/texture-interior (periodic-texture aliasing) pixels
// Push feature flags include: occl_thresh (fill-div via div_eps), rescue_on, mv_guided (color-guided MV
// upsample; host encodes the membership band as 1.0+sim so >0.5 gates AND carries it), gme_on + the 6
// affine params (global affine rescue candidate + fill-div background direction), matte_on/matte_thresh,
// crescent_on, appear_on/appear_band, travel_on, contour_on, ambig_on, member_commit_on, and
// commit_default_on (floors the warp-vs-blend selection at the warp, cross-fading only evidenced garbage).
bool wap_create(VDev& d,VkImageView prev_v,VkImageView cur_v,VkImageView mv_v,VkImageView sad_v,VkImageView out_v,VkImageView mvb_v,VkImageView dis_v,VkImageView disb_v,VkImageView per_v,VkBuffer mass_buf,VkImageView c2_v,VkImageView field_v,VkImageView mvt_v,VkImageView prev_out_v,const std::vector<uint32_t>& spv,WapPipe& p){
    VkSamplerCreateInfo s{}; s.sType=VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO; s.magFilter=VK_FILTER_LINEAR; s.minFilter=VK_FILTER_LINEAR; s.addressModeU=s.addressModeV=s.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE; vkCreateSampler(d.dev,&s,nullptr,&p.samp);
    const VkDescriptorSetLayoutBinding bd[14]={
        {0,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_COMPUTE_BIT,&p.samp},
        {1,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_COMPUTE_BIT,&p.samp},
        {2,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_COMPUTE_BIT,&p.samp},
        {3,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_COMPUTE_BIT,&p.samp},
        {4,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr},
        {5,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_COMPUTE_BIT,&p.samp},   // MV_bwd
        {6,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_COMPUTE_BIT,&p.samp},   // dissidence_fwd (R8)
        {7,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_COMPUTE_BIT,&p.samp},   // dissidence_bwd (R8)
        {8,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_COMPUTE_BIT,&p.samp},   // persistence (R8)
        {9,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr},           // mass counter
        {10,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_COMPUTE_BIT,&p.samp},  // candidate (RGBA16F)
        {11,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr},           // iGPU contour field (R32_UINT)
        {12,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_COMPUTE_BIT,&p.samp},  // next-pair MV target (RG16F, --vblend)
        {13,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_COMPUTE_BIT,&p.samp}}; // prev-output history (RGBA8, --ts-smooth)
    VkDescriptorSetLayoutCreateInfo dl{}; dl.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO; dl.bindingCount=14; dl.pBindings=bd; vkCreateDescriptorSetLayout(d.dev,&dl,nullptr,&p.dsl);
    VkPushConstantRange pcr{}; pcr.stageFlags=VK_SHADER_STAGE_COMPUTE_BIT; pcr.size=212; // {res_ceil,conf_improv,agreement,t,soft_gate,commit_thresh,commit_real,occl_thresh,div_eps,rescue_on,mv_guided,gme_on,gme_a..gme_f,matte_on,matte_thresh,stasis_thresh,inertia_thresh,crescent_on,appear_on,appear_band,travel_on,contour_on,obj_crescent_on,phase_anchor_on,ambig_on,member_commit_on,commit_default_on,onepos_on,onepos_band,disoccl_commit_on,bg_snap_on,bg_snap_strength,bg_snap_norm,extrap,vblend_on,vblend_t0,vblend_strength,band_xfade,vblend_exact,ts_smooth,mc_on,mc_nperturb,mc_perturb,mc_disp,mc_edge,cam_lead.x,cam_lead.y,disoccl_hardpick} = 53 floats / 212B. cam_lead is a GLSL vec2 std430-aligned to 8B; the preceding 50 floats put it at offset 200 (8-aligned → no pad), and disoccl_hardpick lands at offset 208 (no pad). Host-side these mirror as trailing floats {clx,cly,...}. maxPushConstantsSize is 256B on both target GPUs, so 212B fits within the device limit; the 128B figure is the Vulkan spec MINIMUM guarantee, not a project cap.
    VkPipelineLayoutCreateInfo pl{}; pl.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO; pl.setLayoutCount=1; pl.pSetLayouts=&p.dsl; pl.pushConstantRangeCount=1; pl.pPushConstantRanges=&pcr; vkCreatePipelineLayout(d.dev,&pl,nullptr,&p.layout);
    VkShaderModuleCreateInfo mci{}; mci.sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO; mci.codeSize=spv.size()*sizeof(uint32_t); mci.pCode=spv.data(); VkShaderModule mod; vkCreateShaderModule(d.dev,&mci,nullptr,&mod);
    VkPipelineShaderStageCreateInfo stg{}; stg.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; stg.stage=VK_SHADER_STAGE_COMPUTE_BIT; stg.module=mod; stg.pName="main";
    VkComputePipelineCreateInfo cp{}; cp.sType=VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO; cp.stage=stg; cp.layout=p.layout; vkCreateComputePipelines(d.dev,VK_NULL_HANDLE,1,&cp,nullptr,&p.pipe); vkDestroyShaderModule(d.dev,mod,nullptr);
    const VkDescriptorPoolSize psz[3]={{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,11},{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,2},{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1}};  // 11 samplers (incl. candidate binding 10, vblend MV-target binding 12, ts-smooth prev-output binding 13) + 2 storage images (binding 4 wOut, binding 11 field) + the mass counter SSBO
    VkDescriptorPoolCreateInfo pi{}; pi.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO; pi.maxSets=1; pi.poolSizeCount=3; pi.pPoolSizes=psz; vkCreateDescriptorPool(d.dev,&pi,nullptr,&p.pool);
    VkDescriptorSetAllocateInfo dai{}; dai.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO; dai.descriptorPool=p.pool; dai.descriptorSetCount=1; dai.pSetLayouts=&p.dsl; vkAllocateDescriptorSets(d.dev,&dai,&p.set);
    VkDescriptorImageInfo iprev{}; iprev.sampler=p.samp; iprev.imageView=prev_v; iprev.imageLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkDescriptorImageInfo icur{};  icur.sampler=p.samp;  icur.imageView=cur_v;   icur.imageLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkDescriptorImageInfo imv{};   imv.sampler=p.samp;   imv.imageView=mv_v;     imv.imageLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkDescriptorImageInfo isad{};  isad.sampler=p.samp;  isad.imageView=sad_v;   isad.imageLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkDescriptorImageInfo iout{};  iout.imageView=out_v; iout.imageLayout=VK_IMAGE_LAYOUT_GENERAL;
    VkDescriptorImageInfo imvb{};  imvb.sampler=p.samp;  imvb.imageView=mvb_v;   imvb.imageLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkDescriptorImageInfo idis{};  idis.sampler=p.samp;  idis.imageView=dis_v;   idis.imageLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;  // dissidence_fwd
    VkDescriptorImageInfo idisb{}; idisb.sampler=p.samp; idisb.imageView=disb_v; idisb.imageLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; // dissidence_bwd
    VkDescriptorImageInfo iper{};  iper.sampler=p.samp;  iper.imageView=per_v;   iper.imageLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;  // persistence (R8)
    VkDescriptorBufferInfo imass{}; imass.buffer=mass_buf; imass.offset=0; imass.range=VK_WHOLE_SIZE;  // mass counter SSBO
    VkDescriptorImageInfo ic2{};   ic2.sampler=p.samp;   ic2.imageView=c2_v;     ic2.imageLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;   // candidate (RGBA16F)
    VkDescriptorImageInfo ifld{};  ifld.imageView=field_v; ifld.imageLayout=VK_IMAGE_LAYOUT_GENERAL;   // iGPU contour field (R32_UINT, imageLoad → GENERAL)
    VkDescriptorImageInfo imvt{};  imvt.sampler=p.samp;  imvt.imageView=mvt_v;   imvt.imageLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;  // next-pair MV target (RG16F, --vblend)
    VkDescriptorImageInfo ipout{}; ipout.sampler=p.samp; ipout.imageView=prev_out_v; ipout.imageLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;  // prev-output history (RGBA8, --ts-smooth)
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
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,nullptr,p.set,10,0,1,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,&ic2,nullptr,nullptr},   // candidate
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,nullptr,p.set,11,0,1,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,&ifld,nullptr,nullptr},  // contour field
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,nullptr,p.set,12,0,1,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,&imvt,nullptr,nullptr},   // MV target (--vblend)
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,nullptr,p.set,13,0,1,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,&ipout,nullptr,nullptr}};  // prev-output history (--ts-smooth)
    vkUpdateDescriptorSets(d.dev,14,wds,0,nullptr); return p.pipe!=VK_NULL_HANDLE;
}
void wap_destroy(VDev& d,WapPipe& p){ if(p.pool)vkDestroyDescriptorPool(d.dev,p.pool,nullptr); if(p.pipe)vkDestroyPipeline(d.dev,p.pipe,nullptr); if(p.layout)vkDestroyPipelineLayout(d.dev,p.layout,nullptr); if(p.dsl)vkDestroyDescriptorSetLayout(d.dev,p.dsl,nullptr); if(p.samp)vkDestroySampler(d.dev,p.samp,nullptr); p=WapPipe{}; }
