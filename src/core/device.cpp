// PhyriadFG core/device layer (STEP 3 — PURE RELOCATION from src/core/main.cpp; no logic change).
// vdev_create / vdev_destroy bodies (declared in core/device.hpp). `static` dropped → external
// linkage so main.cpp links to them; behaviour byte-identical (verbatim relocation).
#include "core/device.hpp"
#include <cstdio>    // std::printf / std::snprintf
#include <cstring>   // std::strcmp
#include <vector>

bool vdev_create(VkPhysicalDevice phys,VDev& d,bool want_swap,bool want_extmem_win32,bool prefer_same_family_q2,bool want_xfer_q,bool want_ofa){
    vkGetPhysicalDeviceMemoryProperties(phys,&d.mp); VkPhysicalDeviceProperties props; vkGetPhysicalDeviceProperties(phys,&props); std::snprintf(d.name,sizeof(d.name),"%s",props.deviceName); d.phys=phys; d.type=props.deviceType;
    uint32_t ec=0; vkEnumerateDeviceExtensionProperties(phys,nullptr,&ec,nullptr); std::vector<VkExtensionProperties> ex(ec); vkEnumerateDeviceExtensionProperties(phys,nullptr,&ec,ex.data()); bool has_sc=false;
    // STAGE-45: the VK→D3D11 bridge (--present-surface) imports a D3D11 shared texture as a VK
    // image (VK_KHR_external_memory_win32) synced by a keyed mutex (VK_KHR_win32_keyed_mutex).
    // Both are queried here and enabled ONLY when want_extmem_win32 (surface mode) — when OFF the
    // device's enabled-extension set is byte-identical to before (the kickoff's default-OFF gate).
    bool has_extmem_win32=false, has_keyed_mutex=false;
    bool has_optical_flow=false;   // STAGE-115 (--nvofa): VK_NV_optical_flow exposed?
    for(auto& e:ex){if(!std::strcmp(e.extensionName,VK_EXT_EXTERNAL_MEMORY_HOST_EXTENSION_NAME))d.has_emh=true; if(!std::strcmp(e.extensionName,VK_KHR_SWAPCHAIN_EXTENSION_NAME))has_sc=true;
        if(!std::strcmp(e.extensionName,VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME))has_extmem_win32=true;
        if(!std::strcmp(e.extensionName,VK_KHR_WIN32_KEYED_MUTEX_EXTENSION_NAME))has_keyed_mutex=true;
        if(!std::strcmp(e.extensionName,VK_NV_OPTICAL_FLOW_EXTENSION_NAME))has_optical_flow=true;}
    d.has_optical_flow=has_optical_flow;
    d.has_extmem_win32=has_extmem_win32; d.has_keyed_mutex=has_keyed_mutex;
    // FG_VENDOR_AGNOSTIC E1 (CR1 mitigation = QUERY-ONLY, never ENABLE): read the vendor-agnostic
    // capability bits (fp16-packed-math + DP4a int8-dot + 16-bit-storage) first-hand from the device via a
    // VkPhysicalDeviceFeatures2 *query*. This chain is LOCAL to the read — it is NOT chained onto the
    // vkCreateDevice pNext below and these features are NOT added to the enabled set, so the default
    // device-creation path is byte-for-byte unchanged (the default render is byte-identical; the fields are
    // populated but the default A/B/G role-routing ignores them — see the VDev field comment). The feature
    // structs are core promotions (Vulkan 1.2 / 1.3) with the KHR aliases, queried unconditionally here so a
    // single arbitrary-vendor GPU is correctly NAMED even on the flag-free path. Inert-by-default on NVIDIA
    // / the fixed-role FG (coverage, not a default win).
    {
        VkPhysicalDeviceShaderFloat16Int8Features f16i8{}; f16i8.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES;
        VkPhysicalDeviceShaderIntegerDotProductFeatures dotp{}; dotp.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_INTEGER_DOT_PRODUCT_FEATURES;
        VkPhysicalDevice16BitStorageFeatures st16{}; st16.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES;
        f16i8.pNext=&dotp; dotp.pNext=&st16;
        VkPhysicalDeviceFeatures2 cf2{}; cf2.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2; cf2.pNext=&f16i8;
        vkGetPhysicalDeviceFeatures2(phys,&cf2);   // query only — no enablement, no pipeline/device-create change
        d.has_fp16=(f16i8.shaderFloat16==VK_TRUE);
        d.has_dp4a=(dotp.shaderIntegerDotProduct==VK_TRUE);
        d.fp16_storage=(st16.storageBuffer16BitAccess==VK_TRUE);
    }
    if(d.has_emh){VkPhysicalDeviceExternalMemoryHostPropertiesEXT emh{}; emh.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_MEMORY_HOST_PROPERTIES_EXT; VkPhysicalDeviceProperties2 p2{}; p2.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2; p2.pNext=&emh; vkGetPhysicalDeviceProperties2(phys,&p2); d.host_align=emh.minImportedHostPointerAlignment?emh.minImportedHostPointerAlignment:1u;}
    uint32_t qfc=0; vkGetPhysicalDeviceQueueFamilyProperties(phys,&qfc,nullptr); std::vector<VkQueueFamilyProperties> qfs(qfc); vkGetPhysicalDeviceQueueFamilyProperties(phys,&qfc,qfs.data());
    for(uint32_t i=0;i<qfc;++i) if(qfs[i].queueFlags&(VK_QUEUE_GRAPHICS_BIT|VK_QUEUE_COMPUTE_BIT)){d.qfam=i;break;} if(d.qfam==UINT32_MAX) return false;
    std::vector<const char*> exts; if(d.has_emh) exts.push_back(VK_EXT_EXTERNAL_MEMORY_HOST_EXTENSION_NAME); if(want_swap&&has_sc) exts.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    // STAGE-45: enable the win32 external-memory + keyed-mutex pair for the A-side bridge. The
    // win32 import needs VK_KHR_external_memory (the base, core in 1.1) + its win32 child; the
    // keyed mutex is the cross-device sync chained on the blit submit.
    bool use_extmem_win32=false;
    if(want_extmem_win32&&has_extmem_win32&&has_keyed_mutex){
        exts.push_back(VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME);
        exts.push_back(VK_KHR_WIN32_KEYED_MUTEX_EXTENSION_NAME);
        use_extmem_win32=true;
    }
    d.extmem_win32_enabled=use_extmem_win32;
    // STAGE-37a: pick a second queue (used by G to split convert off the present queue; created on
    // A/B too but unused there). Prefer (a) a DISTINCT compute-only family (AMD async compute), else
    // (b) a second queue in qfam (queueCount>1), else (c) shared fallback (q2=q — identical to before).
    uint32_t qfam2=d.qfam; int q2mode=2;   // 0=split-family, 1=split-queue, 2=shared
    // CRASH-FIX (A.q2 lock-free partition): when prefer_same_family_q2 is set (device A only) the
    // split-FAMILY search is SKIPPED so the mode falls through to the SAME-family split-queue (q2mode=1
    // when queueCount>1). A.pool cmd buffers can then submit to A.q2 with NO family trap / NO ownership
    // transfer — the non-present A-work (F's flow, C's convert) routes off A.q while P keeps A.q
    // exclusively. Two distinct VkQueue handles, same family → per-handle external sync is satisfied.
    if(!prefer_same_family_q2){
      for(uint32_t i=0;i<qfc;++i){ if(i==d.qfam) continue;
          if((qfs[i].queueFlags&VK_QUEUE_COMPUTE_BIT)&&!(qfs[i].queueFlags&VK_QUEUE_GRAPHICS_BIT)){ qfam2=i; q2mode=0; break; } }
    }
    if(q2mode==2&&qfs[d.qfam].queueCount>1) q2mode=1;   // qfam2 stays d.qfam
    d.qfam2=qfam2;
    // STAGE-106 (--upload-xfer): find a TRANSFER queue family DISTINCT from qfam/qfam2 — prefer a
    // COMPUTE&&!GRAPHICS family (a true parallel engine that can ALSO run the in-upload median COMPUTE
    // dispatch), else a transfer-only family. UINT32_MAX → no transfer engine → the feature force-disables
    // downstream (the upload stays on the graphics queue, byte-identical).
    if(want_xfer_q){
        uint32_t pickTC=UINT32_MAX, pickTonly=UINT32_MAX;
        for(uint32_t i=0;i<qfc;++i){ if(i==d.qfam||i==qfam2) continue;
            const VkQueueFlags f=qfs[i].queueFlags;
            if((f&VK_QUEUE_COMPUTE_BIT)&&!(f&VK_QUEUE_GRAPHICS_BIT)){ if(pickTC==UINT32_MAX) pickTC=i; }
            else if((f&VK_QUEUE_TRANSFER_BIT)&&!(f&(VK_QUEUE_GRAPHICS_BIT|VK_QUEUE_COMPUTE_BIT))){ if(pickTonly==UINT32_MAX) pickTonly=i; } }
        d.qfamT = (pickTC!=UINT32_MAX)?pickTC:pickTonly;
    }
    // STAGE-115 (--nvofa): pick the OPTICAL-FLOW queue family (its OWN family on the 4090). Gated on
    // want_ofa AND the extension being exposed; if no OFA family exists the feature force-disables (the
    // ofaQueue stays null → the runtime falls back to the classical OFP, byte-identical). The OFA family
    // is distinct from qfam/qfam2/qfamT (TRANSFER+OPTICAL_FLOW only) — it carries no graphics/compute, so
    // uploads + the convert dispatch + layout transitions all stay on qfam (q); ofaQueue runs only the
    // vkCmdOpticalFlowExecuteNV.
    const bool use_ofa = want_ofa && has_optical_flow;
    if(use_ofa){
        for(uint32_t i=0;i<qfc;++i) if((qfs[i].queueFlags&VK_QUEUE_OPTICAL_FLOW_BIT_NV) && d.ofaQfam==UINT32_MAX){ d.ofaQfam=i; break; }
    }
    float prio[4]={1.f,1.f,1.f,1.f};
    VkDeviceQueueCreateInfo qcis[4]{};
    qcis[0].sType=VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO; qcis[0].queueFamilyIndex=d.qfam; qcis[0].queueCount=1; qcis[0].pQueuePriorities=prio;
    uint32_t nqci=1;
    if(q2mode==0){ qcis[1].sType=VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO; qcis[1].queueFamilyIndex=qfam2; qcis[1].queueCount=1; qcis[1].pQueuePriorities=prio; nqci=2; }
    else if(q2mode==1){ qcis[0].queueCount=2; }
    // STAGE-106: append the transfer-queue family at the index-relative next slot (never a hardcoded index).
    if(want_xfer_q && d.qfamT!=UINT32_MAX){ qcis[nqci].sType=VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO; qcis[nqci].queueFamilyIndex=d.qfamT; qcis[nqci].queueCount=1; qcis[nqci].pQueuePriorities=prio; ++nqci; }
    // STAGE-115 (--nvofa): append the OPTICAL-FLOW queue family (index-relative next slot). Only when an OFA
    // family was found AND it is DISTINCT from every family already requested (it is its own family on the
    // 4090; the distinct-guard keeps a duplicate-family create-info — illegal — out if a future GPU folds
    // OFA into an existing family). If it coincides, ofaQueue reuses that family's queue 0 below.
    bool ofa_own_qci=false;
    if(use_ofa && d.ofaQfam!=UINT32_MAX){
        bool dup=false; for(uint32_t i=0;i<nqci;++i) if(qcis[i].queueFamilyIndex==d.ofaQfam){ dup=true; break; }
        if(!dup){ qcis[nqci].sType=VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO; qcis[nqci].queueFamilyIndex=d.ofaQfam; qcis[nqci].queueCount=1; qcis[nqci].pQueuePriorities=prio; ++nqci; ofa_own_qci=true; }
    }
    // STAGE-106: enable the timeline-semaphore feature (Vulkan 1.2 core) ONLY when the transfer queue is live.
    // tsf must outlive the vkCreateDevice call below (dci.pNext); it shares this scope.
    VkPhysicalDeviceTimelineSemaphoreFeatures tsf{}; tsf.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES; tsf.timelineSemaphore=VK_TRUE;
    // STAGE-115 (--nvofa): the OFA extension + feature. The feature struct (opticalFlow=VK_TRUE) chains on
    // dci.pNext; it must outlive the vkCreateDevice call (this scope). When also enabling timeline semaphores
    // we chain ofFeat -> tsf so BOTH features are requested. Default path (no OFA): byte-identical (exts/pNext
    // untouched).
    VkPhysicalDeviceOpticalFlowFeaturesNV ofFeat{}; ofFeat.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_OPTICAL_FLOW_FEATURES_NV; ofFeat.opticalFlow=VK_TRUE;
    if(use_ofa){ exts.push_back(VK_NV_OPTICAL_FLOW_EXTENSION_NAME); exts.push_back(VK_KHR_FORMAT_FEATURE_FLAGS_2_EXTENSION_NAME); exts.push_back(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME); }   // STAGE-115 fix: VK_NV_optical_flow REQUIRES both VK_KHR_format_feature_flags2 AND VK_KHR_synchronization2
    // STAGE-115 fix (VUID-vkCmdDraw-None-09600): the nvofa_convert pass binds the OFA flow/cost as rg16i + r8ui
    // STORAGE images (mutable views). R16G16_SINT and R8_UINT carry the STORAGE_IMAGE format feature only when
    // shaderStorageImageExtendedFormats is enabled — without it the storage-image accesses are invalid. Enable
    // it (only on the OFA path; dci.pEnabledFeatures stays null otherwise → byte-identical). enFeat must outlive
    // the vkCreateDevice call (this scope).
    VkPhysicalDeviceFeatures enFeat{}; if(use_ofa){ enFeat.shaderStorageImageExtendedFormats=VK_TRUE; }
    VkDeviceCreateInfo dci{}; dci.sType=VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO; dci.queueCreateInfoCount=nqci; dci.pQueueCreateInfos=qcis; dci.enabledExtensionCount=(uint32_t)exts.size(); dci.ppEnabledExtensionNames=exts.data();
    if(use_ofa) dci.pEnabledFeatures=&enFeat;
    const bool want_ts = (want_xfer_q && d.qfamT!=UINT32_MAX);
    if(use_ofa){ ofFeat.pNext = want_ts ? (void*)&tsf : nullptr; dci.pNext=&ofFeat; }
    else if(want_ts) dci.pNext=&tsf;
    if(vkCreateDevice(phys,&dci,nullptr,&d.dev)!=VK_SUCCESS) return false;
    vkGetDeviceQueue(d.dev,d.qfam,0,&d.q);
    if(q2mode==0) vkGetDeviceQueue(d.dev,qfam2,0,&d.q2);
    else if(q2mode==1) vkGetDeviceQueue(d.dev,d.qfam,1,&d.q2);
    else d.q2=d.q;
    VkCommandPoolCreateInfo cpci{}; cpci.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO; cpci.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; cpci.queueFamilyIndex=d.qfam; vkCreateCommandPool(d.dev,&cpci,nullptr,&d.pool);
    // pool2: own pool bound to qfam2 when q2 is a real second queue (the family trap); else shared.
    if(q2mode!=2){ VkCommandPoolCreateInfo cpci2{}; cpci2.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO; cpci2.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; cpci2.queueFamilyIndex=qfam2; vkCreateCommandPool(d.dev,&cpci2,nullptr,&d.pool2); }
    else d.pool2=d.pool;
    // STAGE-106 (--upload-xfer): the transfer queue + its pool + the two TIMELINE semaphores. Gated on a
    // real qfamT; enumerates ALL families (the first-hand close on the [wf] vulkaninfo premise) + logs the pick.
    if(want_xfer_q){
        for(uint32_t i=0;i<qfc;++i) std::printf("[ra]   qfam[%u]: flags=0x%X count=%u\n", i, qfs[i].queueFlags, qfs[i].queueCount);
        if(d.qfamT!=UINT32_MAX){
            vkGetDeviceQueue(d.dev,d.qfamT,0,&d.qT);
            VkCommandPoolCreateInfo cpciT{}; cpciT.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO; cpciT.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; cpciT.queueFamilyIndex=d.qfamT; vkCreateCommandPool(d.dev,&cpciT,nullptr,&d.poolT);
            VkSemaphoreTypeCreateInfo stci{}; stci.sType=VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO; stci.semaphoreType=VK_SEMAPHORE_TYPE_TIMELINE; stci.initialValue=0;
            VkSemaphoreCreateInfo sci{}; sci.sType=VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO; sci.pNext=&stci;
            vkCreateSemaphore(d.dev,&sci,nullptr,&d.semUpTL); vkCreateSemaphore(d.dev,&sci,nullptr,&d.semWarpTL);
            std::printf("[ra] --upload-xfer: A.qT transfer queue armed (qfamT=%u flags=0x%X queues=%u) + timeline semaphores up/warp\n", d.qfamT, qfs[d.qfamT].queueFlags, qfs[d.qfamT].queueCount);
        } else {
            std::printf("[ra] --upload-xfer: NO transfer/async-compute family distinct from qfam(%u)/qfam2(%u) on '%s' — upload stays on the graphics queue (feature off, byte-identical)\n", d.qfam, d.qfam2, d.name);
        }
    }
    // STAGE-115 (--nvofa): the OFA queue + its command pool + the device OF PFNs. Gated on a real OFA
    // family; the queue is from queue 0 of ofaQfam (its own family on the 4090). The pool is family-bound
    // to ofaQfam so a cmd buffer recorded from it submits to ofaQueue with no family trap. If ANY PFN fails
    // to load (a driver that exposes the ext but not the entry points — never on the 4090) the feature
    // force-disables (ofaQueue cleared → the runtime falls back to the classical OFP, byte-identical).
    if(use_ofa && d.ofaQfam!=UINT32_MAX){
        (void)ofa_own_qci;   // the queue is at index 0 of ofaQfam whether its create-info was own or shared
        vkGetDeviceQueue(d.dev,d.ofaQfam,0,&d.ofaQueue);
        VkCommandPoolCreateInfo cpciO{}; cpciO.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO; cpciO.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; cpciO.queueFamilyIndex=d.ofaQfam; vkCreateCommandPool(d.dev,&cpciO,nullptr,&d.ofaPool);
        d.pfnCreateOFSession =(PFN_vkCreateOpticalFlowSessionNV)vkGetDeviceProcAddr(d.dev,"vkCreateOpticalFlowSessionNV");
        d.pfnDestroyOFSession=(PFN_vkDestroyOpticalFlowSessionNV)vkGetDeviceProcAddr(d.dev,"vkDestroyOpticalFlowSessionNV");
        d.pfnBindOFImage =(PFN_vkBindOpticalFlowSessionImageNV)vkGetDeviceProcAddr(d.dev,"vkBindOpticalFlowSessionImageNV");
        d.pfnCmdOFExecute =(PFN_vkCmdOpticalFlowExecuteNV)vkGetDeviceProcAddr(d.dev,"vkCmdOpticalFlowExecuteNV");
        if(!d.pfnCreateOFSession||!d.pfnDestroyOFSession||!d.pfnBindOFImage||!d.pfnCmdOFExecute||!d.ofaQueue){
            std::printf("[ra] --nvofa: OF PFN/queue load failed on '%s' — disabling (classical OFP, byte-identical)\n", d.name);
            if(d.ofaPool) vkDestroyCommandPool(d.dev,d.ofaPool,nullptr);
            d.ofaPool=VK_NULL_HANDLE; d.ofaQueue=VK_NULL_HANDLE; d.ofaQfam=UINT32_MAX;
        } else {
            std::printf("[ra] --nvofa: OFA queue armed on '%s' (ofaQfam=%u flags=0x%X)\n", d.name, d.ofaQfam, qfs[d.ofaQfam].queueFlags);
        }
    }
    if(d.has_emh) d.pfnHostPtr=(PFN_vkGetMemoryHostPointerPropertiesEXT)vkGetDeviceProcAddr(d.dev,"vkGetMemoryHostPointerPropertiesEXT");
    if(d.extmem_win32_enabled) d.pfnGetMemWin32=(PFN_vkGetMemoryWin32HandlePropertiesKHR)vkGetDeviceProcAddr(d.dev,"vkGetMemoryWin32HandlePropertiesKHR");
    return true;
}
void vdev_destroy(VDev& d){ if(d.semUpTL) vkDestroySemaphore(d.dev,d.semUpTL,nullptr); if(d.semWarpTL) vkDestroySemaphore(d.dev,d.semWarpTL,nullptr); if(d.ofaPool) vkDestroyCommandPool(d.dev,d.ofaPool,nullptr); if(d.poolT) vkDestroyCommandPool(d.dev,d.poolT,nullptr); if(d.pool2&&d.pool2!=d.pool) vkDestroyCommandPool(d.dev,d.pool2,nullptr); if(d.pool) vkDestroyCommandPool(d.dev,d.pool,nullptr); if(d.dev) vkDestroyDevice(d.dev,nullptr); d=VDev{}; }
