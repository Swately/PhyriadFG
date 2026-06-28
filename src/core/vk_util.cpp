// PhyriadFG core/vk_util layer: the out-of-line factory + submit helper bodies declared in
// core/vk_util.hpp.
#include "core/vk_util.hpp"
#include "core/globals.hpp"   // vk_live (the submit helpers report device-loss through it)

bool route_for(DXGI_FORMAT f,VkFormat& vk,uint32_t& bpp,Route& rt,const char*& desc){
    switch(f){
    case DXGI_FORMAT_R8G8B8A8_UNORM:     vk=VK_FORMAT_R8G8B8A8_UNORM;           bpp=4;rt=RT_PASS;desc="RGBA8 SDR"; return true;
    case DXGI_FORMAT_B8G8R8A8_UNORM:     vk=VK_FORMAT_B8G8R8A8_UNORM;           bpp=4;rt=RT_PASS;desc="BGRA8 SDR"; return true;
    case DXGI_FORMAT_R16G16B16A16_FLOAT: vk=VK_FORMAT_R16G16B16A16_SFLOAT;      bpp=8;rt=RT_HDR; desc="FP16 HDR scRGB"; return true;
    case DXGI_FORMAT_R10G10B10A2_UNORM:  vk=VK_FORMAT_A2B10G10R10_UNORM_PACK32; bpp=4;rt=RT_10;  desc="10bpc SDR"; return true;
    default: vk=VK_FORMAT_UNDEFINED;bpp=0;rt=RT_NONE;desc="UNSUPPORTED"; return false;
    }
}

bool img_create(VDev& d,uint32_t w,uint32_t h,VkFormat fmt,VkImageUsageFlags usage,Img& out,bool want_view,
                const uint32_t* concurrent_fams,uint32_t n_fams){
    VkImageCreateInfo ici{}; ici.sType=VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO; ici.imageType=VK_IMAGE_TYPE_2D; ici.format=fmt; ici.extent={w,h,1}; ici.mipLevels=1; ici.arrayLayers=1; ici.samples=VK_SAMPLE_COUNT_1_BIT; ici.tiling=VK_IMAGE_TILING_OPTIMAL; ici.usage=usage; ici.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
    if(concurrent_fams&&n_fams>=2){ ici.sharingMode=VK_SHARING_MODE_CONCURRENT; ici.queueFamilyIndexCount=n_fams; ici.pQueueFamilyIndices=concurrent_fams; }
    if(vkCreateImage(d.dev,&ici,nullptr,&out.img)!=VK_SUCCESS) return false;
    VkMemoryRequirements mr; vkGetImageMemoryRequirements(d.dev,out.img,&mr); const uint32_t mt=pick_mem(d.mp,mr.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VkMemoryAllocateInfo mai{}; mai.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO; mai.allocationSize=mr.size; mai.memoryTypeIndex=mt;
    if(vkAllocateMemory(d.dev,&mai,nullptr,&out.mem)!=VK_SUCCESS) return false; vkBindImageMemory(d.dev,out.img,out.mem,0);
    // want_view=false: transfer-only images (TRANSFER_DST|TRANSFER_SRC, no SAMPLED/STORAGE) must
    // not get a view — vkCreateImageView requires at least one view-compatible usage bit
    // (VUID-VkImageViewCreateInfo-image-04441). Callers that bind no view keep out.view==VK_NULL_HANDLE.
    if(!want_view){ out.view=VK_NULL_HANDLE; return true; }
    VkImageViewCreateInfo vi{}; vi.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO; vi.image=out.img; vi.viewType=VK_IMAGE_VIEW_TYPE_2D; vi.format=fmt; vi.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
    return vkCreateImageView(d.dev,&vi,nullptr,&out.view)==VK_SUCCESS;
}
void img_destroy(VDev& d,Img& i){ if(i.view) vkDestroyImageView(d.dev,i.view,nullptr); if(i.img) vkDestroyImage(d.dev,i.img,nullptr); if(i.mem) vkFreeMemory(d.dev,i.mem,nullptr); i=Img{}; }

bool hbuf_import(VDev& d,void* ptr,VkDeviceSize bytes,HBuf& out,
                 VkBufferUsageFlags usage,
                 bool q2_shared){
    if(!d.has_emh||!d.pfnHostPtr) return false;
    VkMemoryHostPointerPropertiesEXT hp{}; hp.sType=VK_STRUCTURE_TYPE_MEMORY_HOST_POINTER_PROPERTIES_EXT;
    if(d.pfnHostPtr(d.dev,VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT,ptr,&hp)!=VK_SUCCESS) return false;
    const uint32_t mt=pick_mem(d.mp,hp.memoryTypeBits,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT); if(mt==UINT32_MAX) return false;
    VkExternalMemoryBufferCreateInfo ext{}; ext.sType=VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO; ext.handleTypes=VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT;
    VkBufferCreateInfo bci{}; bci.sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO; bci.pNext=&ext; bci.size=bytes; bci.usage=usage;
    // A buffer WRITTEN by C's convert on q2 (family qfam2) and READ by P on q (family qfam) under
    // EXCLUSIVE sharing needs a queue-family ownership transfer the pipeline doesn't perform — formal
    // UB even where drivers tolerate it. CONCURRENT across both families removes the requirement;
    // applied only to the buffers that cross (hR_g/hRP_g) and only when this device picked the
    // split-family mode.
    const uint32_t qfis[2]={d.qfam,d.qfam2};
    if(q2_shared&&d.qfam2!=UINT32_MAX&&d.qfam2!=d.qfam){
        bci.sharingMode=VK_SHARING_MODE_CONCURRENT; bci.queueFamilyIndexCount=2; bci.pQueueFamilyIndices=qfis; }
    if(vkCreateBuffer(d.dev,&bci,nullptr,&out.buf)!=VK_SUCCESS) return false;
    VkImportMemoryHostPointerInfoEXT imp{}; imp.sType=VK_STRUCTURE_TYPE_IMPORT_MEMORY_HOST_POINTER_INFO_EXT; imp.handleType=VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT; imp.pHostPointer=ptr;
    VkMemoryAllocateInfo mai{}; mai.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO; mai.pNext=&imp; mai.allocationSize=bytes; mai.memoryTypeIndex=mt;
    if(vkAllocateMemory(d.dev,&mai,nullptr,&out.mem)!=VK_SUCCESS) return false; vkBindBufferMemory(d.dev,out.buf,out.mem,0); out.mapped=ptr; return true;
}
void hbuf_destroy(VDev& d,HBuf& b){ if(b.buf) vkDestroyBuffer(d.dev,b.buf,nullptr); if(b.mem) vkFreeMemory(d.dev,b.mem,nullptr); b=HBuf{}; }
bool dbuf_create(VDev& d,VkDeviceSize bytes,VkBufferUsageFlags usage,HBuf& out){
    VkBufferCreateInfo bci{}; bci.sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO; bci.size=bytes; bci.usage=usage;
    if(vkCreateBuffer(d.dev,&bci,nullptr,&out.buf)!=VK_SUCCESS) return false;
    VkMemoryRequirements mr; vkGetBufferMemoryRequirements(d.dev,out.buf,&mr);
    const uint32_t mt=pick_mem(d.mp,mr.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VkMemoryAllocateInfo mai{}; mai.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO; mai.allocationSize=mr.size; mai.memoryTypeIndex=mt;
    if(vkAllocateMemory(d.dev,&mai,nullptr,&out.mem)!=VK_SUCCESS) return false;
    vkBindBufferMemory(d.dev,out.buf,out.mem,0); return true;
}

void submit_wait(VDev& d,VkCommandBuffer cmd,VkFence f){ vkResetFences(d.dev,1,&f); VkSubmitInfo si{}; si.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO; si.commandBufferCount=1; si.pCommandBuffers=&cmd; vkQueueSubmit(d.q,1,&si,f); vk_live(vkWaitForFences(d.dev,1,&f,VK_TRUE,UINT64_MAX)); }
void submit_wait_q2(VDev& d,VkCommandBuffer cmd,VkFence f){ vkResetFences(d.dev,1,&f); VkSubmitInfo si{}; si.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO; si.commandBufferCount=1; si.pCommandBuffers=&cmd; vkQueueSubmit(d.q2,1,&si,f); vk_live(vkWaitForFences(d.dev,1,&f,VK_TRUE,UINT64_MAX)); }
