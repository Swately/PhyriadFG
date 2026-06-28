// apps/render_assistant/bench/nvofa_bench.cpp
// STANDALONE NVOFA (NVIDIA Optical Flow Accelerator) microbenchmark.
//
// LOAD-BEARING measurement: does the VK_NV_optical_flow hardware path beat the
// PhyriadFG classical block-match flow (~7.5 ms, which saturates the 4090 compute
// cores), at our resolutions, fwd+bwd + cost, and run UNCONTENDED (on the
// dedicated OFA engine, off the compute cores)?
//
// No Phyriad dependencies, no external libs beyond vulkan-1. Every ms is a real
// VkQueryPool timestamp around vkCmdOpticalFlowExecuteNV.
//
// Build (Git Bash invoking cmd via vcvars64):
//   cmd //c '"C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" && \
//     cl /std:c++17 /EHsc /O2 /I "%VULKAN_SDK%\Include" \
//        G:\phyriad\apps\render_assistant\bench\nvofa_bench.cpp \
//        /Fe:G:\phyriad\apps\render_assistant\bench\nvofa_bench.exe \
//        /link "%VULKAN_SDK%\Lib\vulkan-1.lib"'
//
// Run:   nvofa_bench.exe [--validate]

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <vulkan/vulkan.h>
#include <string>     // std::string for device-name match

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

// ── tiny error helper ────────────────────────────────────────────────────────
static const char* vkstr(VkResult r) {
    switch (r) {
        case VK_SUCCESS: return "VK_SUCCESS";
        case VK_NOT_READY: return "VK_NOT_READY";
        case VK_TIMEOUT: return "VK_TIMEOUT";
        case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
        case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
        case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
        case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
        case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
        case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
        case VK_ERROR_VALIDATION_FAILED_EXT: return "VK_ERROR_VALIDATION_FAILED_EXT";
        default: break;
    }
    static char buf[32];
    std::snprintf(buf, sizeof(buf), "VkResult(%d)", (int)r);
    return buf;
}

#define VKCHK(call) do { \
    VkResult vk_rc_ = (call); \
    if (vk_rc_ != VK_SUCCESS) { \
        std::printf("[FATAL] %s -> %s  (at %s:%d)\n", #call, vkstr(vk_rc_), __FILE__, __LINE__); \
        std::fflush(stdout); \
        return false; \
    } \
} while (0)

// ── extension PFNs ───────────────────────────────────────────────────────────
static PFN_vkGetPhysicalDeviceOpticalFlowImageFormatsNV pfnGetOFFormats = nullptr;
static PFN_vkCreateOpticalFlowSessionNV   pfnCreateOFSession  = nullptr;
static PFN_vkDestroyOpticalFlowSessionNV  pfnDestroyOFSession = nullptr;
static PFN_vkBindOpticalFlowSessionImageNV pfnBindOFImage     = nullptr;
static PFN_vkCmdOpticalFlowExecuteNV      pfnCmdOFExecute     = nullptr;

// ── global vk objects ────────────────────────────────────────────────────────
static VkInstance       g_inst = VK_NULL_HANDLE;
static VkPhysicalDevice g_phys = VK_NULL_HANDLE;
static VkDevice         g_dev  = VK_NULL_HANDLE;
static uint32_t         g_ofQF = ~0u;    // optical-flow queue family
static uint32_t         g_gfxQF = ~0u;   // graphics+compute queue family
static VkQueue          g_ofQueue = VK_NULL_HANDLE;
static VkQueue          g_gfxQueue = VK_NULL_HANDLE;
static float            g_tsPeriod = 1.0f;
static VkPhysicalDeviceMemoryProperties g_memProps{};

static uint32_t findMemType(uint32_t typeBits, VkMemoryPropertyFlags want) {
    for (uint32_t i = 0; i < g_memProps.memoryTypeCount; ++i)
        if ((typeBits & (1u << i)) &&
            (g_memProps.memoryTypes[i].propertyFlags & want) == want)
            return i;
    return ~0u;
}

// ── an image + view + memory bundle ──────────────────────────────────────────
struct Img {
    VkImage img = VK_NULL_HANDLE;
    VkDeviceMemory mem = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    uint32_t w = 0, h = 0;
    VkFormat fmt = VK_FORMAT_UNDEFINED;
};

static bool createImg(Img& o, uint32_t w, uint32_t h, VkFormat fmt,
                      VkImageUsageFlags usage, VkOpticalFlowUsageFlagsNV ofUsage) {
    o.w = w; o.h = h; o.fmt = fmt;
    VkOpticalFlowImageFormatInfoNV ofInfo{};
    ofInfo.sType = VK_STRUCTURE_TYPE_OPTICAL_FLOW_IMAGE_FORMAT_INFO_NV;
    ofInfo.usage = ofUsage;

    VkImageCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ci.pNext = (ofUsage != 0) ? &ofInfo : nullptr;  // OF images MUST chain the usage hint
    ci.imageType = VK_IMAGE_TYPE_2D;
    ci.format = fmt;
    ci.extent = { w, h, 1 };
    ci.mipLevels = 1;
    ci.arrayLayers = 1;
    ci.samples = VK_SAMPLE_COUNT_1_BIT;
    ci.tiling = VK_IMAGE_TILING_OPTIMAL;
    ci.usage = usage;
    ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkResult rc = vkCreateImage(g_dev, &ci, nullptr, &o.img);
    if (rc != VK_SUCCESS) {
        std::printf("[FATAL] vkCreateImage(%ux%u fmt=%d) -> %s\n", w, h, (int)fmt, vkstr(rc));
        return false;
    }

    VkMemoryRequirements mr{};
    vkGetImageMemoryRequirements(g_dev, o.img, &mr);
    VkMemoryAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize = mr.size;
    ai.memoryTypeIndex = findMemType(mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (ai.memoryTypeIndex == ~0u) { std::printf("[FATAL] no device-local memtype\n"); return false; }
    rc = vkAllocateMemory(g_dev, &ai, nullptr, &o.mem);
    if (rc != VK_SUCCESS) { std::printf("[FATAL] vkAllocateMemory -> %s\n", vkstr(rc)); return false; }
    rc = vkBindImageMemory(g_dev, o.img, o.mem, 0);
    if (rc != VK_SUCCESS) { std::printf("[FATAL] vkBindImageMemory -> %s\n", vkstr(rc)); return false; }

    VkImageViewCreateInfo vi{};
    vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vi.image = o.img;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format = fmt;
    vi.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    rc = vkCreateImageView(g_dev, &vi, nullptr, &o.view);
    if (rc != VK_SUCCESS) { std::printf("[FATAL] vkCreateImageView -> %s\n", vkstr(rc)); return false; }
    return true;
}

static void destroyImg(Img& o) {
    if (o.view) vkDestroyImageView(g_dev, o.view, nullptr);
    if (o.img)  vkDestroyImage(g_dev, o.img, nullptr);
    if (o.mem)  vkFreeMemory(g_dev, o.mem, nullptr);
    o = {};
}

// ── one-shot command submit on a queue, blocking ─────────────────────────────
static bool submitBlocking(VkCommandBuffer cb, VkQueue q) {
    VkSubmitInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cb;
    VkFenceCreateInfo fci{}; fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence fence; VKCHK(vkCreateFence(g_dev, &fci, nullptr, &fence));
    VKCHK(vkQueueSubmit(q, 1, &si, fence));
    VkResult rc = vkWaitForFences(g_dev, 1, &fence, VK_TRUE, 5ull * 1000 * 1000 * 1000);
    vkDestroyFence(g_dev, fence, nullptr);
    if (rc != VK_SUCCESS) { std::printf("[FATAL] wait fence -> %s\n", vkstr(rc)); return false; }
    return true;
}

// NON-periodic deterministic value at an absolute texel coord. A hash-noise field
// (no spatial repetition) so a block-matcher has exactly ONE good match = the true
// shift. A periodic pattern would let the OFA lock onto an aliased shift with an
// equally-low cost, making the sanity check meaningless — so we avoid that.
static inline uint8_t noiseAt(int x, int y, int chan) {
    uint32_t h = (uint32_t)(x * 374761393 + y * 668265263 + chan * 2147483647);
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= h >> 16;
    // blend the high-freq hash with a smooth low-freq ramp so there is large-scale
    // structure too (helps the matcher converge) but still globally unique
    float ramp = 0.5f + 0.25f * std::sin(x * 0.0123f) + 0.25f * std::cos(y * 0.0117f);
    uint8_t hi = (uint8_t)(h & 0xFF);
    return (uint8_t)std::min(255.0f, std::max(0.0f, 0.5f * hi + 0.5f * ramp * 255.0f));
}

// fill a host-staged BGRA pattern, shifted by (dx,dy) for the second frame.
// frame1(x,y) = field(x-dx, y-dy)  => content moved by (+dx,+dy); OFA forward
// flow INPUT->REFERENCE should report ~(+dx,+dy). Border clamps (no wrap).
static void fillPattern(std::vector<uint8_t>& buf, uint32_t w, uint32_t h, int dx, int dy) {
    buf.resize((size_t)w * h * 4);
    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            int sx = (int)x - dx, sy = (int)y - dy;     // clamp, do NOT wrap
            if (sx < 0) sx = 0; if (sx >= (int)w) sx = (int)w - 1;
            if (sy < 0) sy = 0; if (sy >= (int)h) sy = (int)h - 1;
            size_t o = ((size_t)y * w + x) * 4;
            buf[o + 0] = noiseAt(sx, sy, 0);   // B
            buf[o + 1] = noiseAt(sx, sy, 1);   // G
            buf[o + 2] = noiseAt(sx, sy, 2);   // R
            buf[o + 3] = 255;                  // A
        }
    }
}

// upload host BGRA into a device image, leaving it in VK_IMAGE_LAYOUT_GENERAL
static bool uploadBGRA(Img& dst, const std::vector<uint8_t>& host, VkCommandPool pool) {
    VkBuffer stage; VkDeviceMemory stageMem;
    VkBufferCreateInfo bci{}; bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size = host.size(); bci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VKCHK(vkCreateBuffer(g_dev, &bci, nullptr, &stage));
    VkMemoryRequirements mr{}; vkGetBufferMemoryRequirements(g_dev, stage, &mr);
    VkMemoryAllocateInfo ai{}; ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize = mr.size;
    ai.memoryTypeIndex = findMemType(mr.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    VKCHK(vkAllocateMemory(g_dev, &ai, nullptr, &stageMem));
    VKCHK(vkBindBufferMemory(g_dev, stage, stageMem, 0));
    void* mp; VKCHK(vkMapMemory(g_dev, stageMem, 0, host.size(), 0, &mp));
    std::memcpy(mp, host.data(), host.size());
    vkUnmapMemory(g_dev, stageMem);

    VkCommandBufferAllocateInfo cba{}; cba.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cba.commandPool = pool; cba.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; cba.commandBufferCount = 1;
    VkCommandBuffer cb; VKCHK(vkAllocateCommandBuffers(g_dev, &cba, &cb));
    VkCommandBufferBeginInfo bi{}; bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VKCHK(vkBeginCommandBuffer(cb, &bi));

    VkImageMemoryBarrier b{}; b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED; b.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    b.srcAccessMask = 0; b.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    b.image = dst.img; b.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &b);
    VkBufferImageCopy cp{};
    cp.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    cp.imageExtent = { dst.w, dst.h, 1 };
    vkCmdCopyBufferToImage(cb, stage, dst.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &cp);
    // -> GENERAL (the layout the OF session requires for its bound images)
    b.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; b.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    b.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; b.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &b);
    VKCHK(vkEndCommandBuffer(cb));
    bool ok = submitBlocking(cb, g_gfxQueue);
    vkFreeCommandBuffers(g_dev, pool, 1, &cb);
    vkDestroyBuffer(g_dev, stage, nullptr);
    vkFreeMemory(g_dev, stageMem, nullptr);
    return ok;
}

// transition an OF output image UNDEFINED->GENERAL
static bool toGeneral(Img& im, VkCommandPool pool) {
    VkCommandBufferAllocateInfo cba{}; cba.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cba.commandPool = pool; cba.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; cba.commandBufferCount = 1;
    VkCommandBuffer cb; VKCHK(vkAllocateCommandBuffers(g_dev, &cba, &cb));
    VkCommandBufferBeginInfo bi{}; bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VKCHK(vkBeginCommandBuffer(cb, &bi));
    VkImageMemoryBarrier b{}; b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED; b.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    b.srcAccessMask = 0; b.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    b.image = im.img; b.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &b);
    VKCHK(vkEndCommandBuffer(cb));
    bool ok = submitBlocking(cb, g_gfxQueue);
    vkFreeCommandBuffers(g_dev, pool, 1, &cb);
    return ok;
}

// ── result row ────────────────────────────────────────────────────────────────
struct Stat { double mean = 0, med = 0, p99 = 0; bool ok = false; };

static Stat reduce(std::vector<double>& v) {
    Stat s;
    if (v.empty()) return s;
    std::sort(v.begin(), v.end());
    double sum = 0; for (double x : v) sum += x;
    s.mean = sum / v.size();
    s.med = v[v.size() / 2];
    s.p99 = v[(size_t)std::min(v.size() - 1, (size_t)std::llround(0.99 * (v.size() - 1)))];
    s.ok = true;
    return s;
}

// ── heavy compute contention shader (SPIR-V) ─────────────────────────────────
// A tight FMA loop dispatched over a big grid so the COMPUTE engine stays pegged.
// GLSL (compiled with the SDK's glslangValidator -V; the bytes below are the
// real compiler output, NOT hand-assembled):
//   #version 450
//   layout(local_size_x=256) in;
//   layout(std430,binding=0) buffer B { float d[]; };
//   void main(){ uint i=gl_GlobalInvocationID.x; float a=d[i];
//     for(int k=0;k<4096;k++){ a=a*1.0000001+0.0000001; } d[i]=a; }
static const uint32_t g_compSpv[] = {
0x07230203,0x00010000,0x0008000b,0x00000036,0x00000000,0x00020011,0x00000001,0x0006000b,
0x00000001,0x4c534c47,0x6474732e,0x3035342e,0x00000000,0x0003000e,0x00000000,0x00000001,
0x0006000f,0x00000005,0x00000004,0x6e69616d,0x00000000,0x0000000b,0x00060010,0x00000004,
0x00000011,0x00000100,0x00000001,0x00000001,0x00030003,0x00000002,0x000001c2,0x00040005,
0x00000004,0x6e69616d,0x00000000,0x00030005,0x00000008,0x00000069,0x00080005,0x0000000b,
0x475f6c67,0x61626f6c,0x766e496c,0x7461636f,0x496e6f69,0x00000044,0x00030005,0x00000012,
0x00000061,0x00030005,0x00000014,0x00000042,0x00040006,0x00000014,0x00000000,0x00000064,
0x00030005,0x00000016,0x00000000,0x00030005,0x0000001e,0x0000006b,0x00040047,0x0000000b,
0x0000000b,0x0000001c,0x00040047,0x00000013,0x00000006,0x00000004,0x00030047,0x00000014,
0x00000003,0x00050048,0x00000014,0x00000000,0x00000023,0x00000000,0x00040047,0x00000016,
0x00000021,0x00000000,0x00040047,0x00000016,0x00000022,0x00000000,0x00040047,0x00000035,
0x0000000b,0x00000019,0x00020013,0x00000002,0x00030021,0x00000003,0x00000002,0x00040015,
0x00000006,0x00000020,0x00000000,0x00040020,0x00000007,0x00000007,0x00000006,0x00040017,
0x00000009,0x00000006,0x00000003,0x00040020,0x0000000a,0x00000001,0x00000009,0x0004003b,
0x0000000a,0x0000000b,0x00000001,0x0004002b,0x00000006,0x0000000c,0x00000000,0x00040020,
0x0000000d,0x00000001,0x00000006,0x00030016,0x00000010,0x00000020,0x00040020,0x00000011,
0x00000007,0x00000010,0x0003001d,0x00000013,0x00000010,0x0003001e,0x00000014,0x00000013,
0x00040020,0x00000015,0x00000002,0x00000014,0x0004003b,0x00000015,0x00000016,0x00000002,
0x00040015,0x00000017,0x00000020,0x00000001,0x0004002b,0x00000017,0x00000018,0x00000000,
0x00040020,0x0000001a,0x00000002,0x00000010,0x00040020,0x0000001d,0x00000007,0x00000017,
0x0004002b,0x00000017,0x00000025,0x00001000,0x00020014,0x00000026,0x0004002b,0x00000010,
0x00000029,0x3f800001,0x0004002b,0x00000010,0x0000002b,0x33d6bf95,0x0004002b,0x00000017,
0x0000002e,0x00000001,0x0004002b,0x00000006,0x00000033,0x00000100,0x0004002b,0x00000006,
0x00000034,0x00000001,0x0006002c,0x00000009,0x00000035,0x00000033,0x00000034,0x00000034,
0x00050036,0x00000002,0x00000004,0x00000000,0x00000003,0x000200f8,0x00000005,0x0004003b,
0x00000007,0x00000008,0x00000007,0x0004003b,0x00000011,0x00000012,0x00000007,0x0004003b,
0x0000001d,0x0000001e,0x00000007,0x00050041,0x0000000d,0x0000000e,0x0000000b,0x0000000c,
0x0004003d,0x00000006,0x0000000f,0x0000000e,0x0003003e,0x00000008,0x0000000f,0x0004003d,
0x00000006,0x00000019,0x00000008,0x00060041,0x0000001a,0x0000001b,0x00000016,0x00000018,
0x00000019,0x0004003d,0x00000010,0x0000001c,0x0000001b,0x0003003e,0x00000012,0x0000001c,
0x0003003e,0x0000001e,0x00000018,0x000200f9,0x0000001f,0x000200f8,0x0000001f,0x000400f6,
0x00000021,0x00000022,0x00000000,0x000200f9,0x00000023,0x000200f8,0x00000023,0x0004003d,
0x00000017,0x00000024,0x0000001e,0x000500b1,0x00000026,0x00000027,0x00000024,0x00000025,
0x000400fa,0x00000027,0x00000020,0x00000021,0x000200f8,0x00000020,0x0004003d,0x00000010,
0x00000028,0x00000012,0x00050085,0x00000010,0x0000002a,0x00000028,0x00000029,0x00050081,
0x00000010,0x0000002c,0x0000002a,0x0000002b,0x0003003e,0x00000012,0x0000002c,0x000200f9,
0x00000022,0x000200f8,0x00000022,0x0004003d,0x00000017,0x0000002d,0x0000001e,0x00050080,
0x00000017,0x0000002f,0x0000002d,0x0000002e,0x0003003e,0x0000001e,0x0000002f,0x000200f9,
0x0000001f,0x000200f8,0x00000021,0x0004003d,0x00000006,0x00000030,0x00000008,0x0004003d,
0x00000010,0x00000031,0x00000012,0x00060041,0x0000001a,0x00000032,0x00000016,0x00000018,
0x00000030,0x0003003e,0x00000032,0x00000031,0x000100fd,0x00010038
};

// ── globals for contention ───────────────────────────────────────────────────
struct ContentionRig {
    VkBuffer buf = VK_NULL_HANDLE; VkDeviceMemory mem = VK_NULL_HANDLE;
    VkShaderModule sm = VK_NULL_HANDLE;
    VkDescriptorSetLayout dsl = VK_NULL_HANDLE; VkPipelineLayout pl = VK_NULL_HANDLE;
    VkPipeline pipe = VK_NULL_HANDLE;
    VkDescriptorPool dp = VK_NULL_HANDLE; VkDescriptorSet ds = VK_NULL_HANDLE;
    VkCommandPool pool = VK_NULL_HANDLE; VkCommandBuffer cb = VK_NULL_HANDLE;
    uint32_t groups = 0;
    bool ok = false;
    std::vector<VkFence> inflight;   // all fences submitted, drained at teardown
};

static bool buildContention(ContentionRig& r) {
    const uint32_t N = 1u << 20;          // 1M floats
    r.groups = N / 256;
    VkBufferCreateInfo bci{}; bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size = (VkDeviceSize)N * 4; bci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VKCHK(vkCreateBuffer(g_dev, &bci, nullptr, &r.buf));
    VkMemoryRequirements mr{}; vkGetBufferMemoryRequirements(g_dev, r.buf, &mr);
    VkMemoryAllocateInfo ai{}; ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize = mr.size;
    ai.memoryTypeIndex = findMemType(mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VKCHK(vkAllocateMemory(g_dev, &ai, nullptr, &r.mem));
    VKCHK(vkBindBufferMemory(g_dev, r.buf, r.mem, 0));

    VkShaderModuleCreateInfo smci{}; smci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    smci.codeSize = sizeof(g_compSpv); smci.pCode = g_compSpv;
    VKCHK(vkCreateShaderModule(g_dev, &smci, nullptr, &r.sm));

    VkDescriptorSetLayoutBinding lb{}; lb.binding = 0;
    lb.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; lb.descriptorCount = 1;
    lb.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    VkDescriptorSetLayoutCreateInfo dlc{}; dlc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dlc.bindingCount = 1; dlc.pBindings = &lb;
    VKCHK(vkCreateDescriptorSetLayout(g_dev, &dlc, nullptr, &r.dsl));
    VkPipelineLayoutCreateInfo plc{}; plc.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plc.setLayoutCount = 1; plc.pSetLayouts = &r.dsl;
    VKCHK(vkCreatePipelineLayout(g_dev, &plc, nullptr, &r.pl));

    VkComputePipelineCreateInfo cpc{}; cpc.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    cpc.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    cpc.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    cpc.stage.module = r.sm; cpc.stage.pName = "main";
    cpc.layout = r.pl;
    VKCHK(vkCreateComputePipelines(g_dev, VK_NULL_HANDLE, 1, &cpc, nullptr, &r.pipe));

    VkDescriptorPoolSize ps{}; ps.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; ps.descriptorCount = 1;
    VkDescriptorPoolCreateInfo dpc{}; dpc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    dpc.maxSets = 1; dpc.poolSizeCount = 1; dpc.pPoolSizes = &ps;
    VKCHK(vkCreateDescriptorPool(g_dev, &dpc, nullptr, &r.dp));
    VkDescriptorSetAllocateInfo dsa{}; dsa.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    dsa.descriptorPool = r.dp; dsa.descriptorSetCount = 1; dsa.pSetLayouts = &r.dsl;
    VKCHK(vkAllocateDescriptorSets(g_dev, &dsa, &r.ds));
    VkDescriptorBufferInfo dbi{}; dbi.buffer = r.buf; dbi.offset = 0; dbi.range = VK_WHOLE_SIZE;
    VkWriteDescriptorSet w{}; w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    w.dstSet = r.ds; w.dstBinding = 0; w.descriptorCount = 1;
    w.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; w.pBufferInfo = &dbi;
    vkUpdateDescriptorSets(g_dev, 1, &w, 0, nullptr);

    VkCommandPoolCreateInfo cpci{}; cpci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cpci.queueFamilyIndex = g_gfxQF;
    cpci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    VKCHK(vkCreateCommandPool(g_dev, &cpci, nullptr, &r.pool));
    VkCommandBufferAllocateInfo cba{}; cba.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cba.commandPool = r.pool; cba.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; cba.commandBufferCount = 1;
    VKCHK(vkAllocateCommandBuffers(g_dev, &cba, &r.cb));
    // record many back-to-back dispatches so one submit keeps the queue busy a while.
    // SIMULTANEOUS_USE: this command buffer is legally re-submitted while a prior
    // submission of it is still pending (that is exactly how we keep the queue pegged).
    VkCommandBufferBeginInfo bi{}; bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
    VKCHK(vkBeginCommandBuffer(r.cb, &bi));
    vkCmdBindPipeline(r.cb, VK_PIPELINE_BIND_POINT_COMPUTE, r.pipe);
    vkCmdBindDescriptorSets(r.cb, VK_PIPELINE_BIND_POINT_COMPUTE, r.pl, 0, 1, &r.ds, 0, nullptr);
    for (int i = 0; i < 64; ++i)
        vkCmdDispatch(r.cb, r.groups, 1, 1);
    VKCHK(vkEndCommandBuffer(r.cb));
    r.ok = true;
    return true;
}

// submit the contention load N times without blocking. Each submit gets its own
// fence (tracked in r.inflight); we lazily reap already-signaled ones so the list
// does not grow without bound, and never destroy a fence that is still pending
// (validation VUID-vkDestroyFence-fence-01120).
static void kickContention(ContentionRig& r, int submits) {
    // reap completed fences
    for (size_t i = 0; i < r.inflight.size();) {
        if (vkGetFenceStatus(g_dev, r.inflight[i]) == VK_SUCCESS) {
            vkDestroyFence(g_dev, r.inflight[i], nullptr);
            r.inflight[i] = r.inflight.back(); r.inflight.pop_back();
        } else ++i;
    }
    for (int i = 0; i < submits; ++i) {
        VkFenceCreateInfo fci{}; fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        VkFence f; vkCreateFence(g_dev, &fci, nullptr, &f);
        VkSubmitInfo si{}; si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        si.commandBufferCount = 1; si.pCommandBuffers = &r.cb;
        vkQueueSubmit(g_gfxQueue, 1, &si, f);
        r.inflight.push_back(f);
    }
}

// block until all submitted contention work is done, then destroy its fences
static void drainContention(ContentionRig& r) {
    if (!r.inflight.empty())
        vkWaitForFences(g_dev, (uint32_t)r.inflight.size(), r.inflight.data(),
                        VK_TRUE, 20ull*1000*1000*1000);
    for (VkFence f : r.inflight) vkDestroyFence(g_dev, f, nullptr);
    r.inflight.clear();
}

static void destroyContention(ContentionRig& r) {
    drainContention(r);   // ensure no fence is still pending before teardown
    if (r.pipe) vkDestroyPipeline(g_dev, r.pipe, nullptr);
    if (r.pl) vkDestroyPipelineLayout(g_dev, r.pl, nullptr);
    if (r.dsl) vkDestroyDescriptorSetLayout(g_dev, r.dsl, nullptr);
    if (r.dp) vkDestroyDescriptorPool(g_dev, r.dp, nullptr);
    if (r.sm) vkDestroyShaderModule(g_dev, r.sm, nullptr);
    if (r.pool) vkDestroyCommandPool(g_dev, r.pool, nullptr);
    if (r.buf) vkDestroyBuffer(g_dev, r.buf, nullptr);
    if (r.mem) vkFreeMemory(g_dev, r.mem, nullptr);
    r = {};
}

// ── the core: one OF config measured ─────────────────────────────────────────
struct Cfg {
    uint32_t w, h;
    VkOpticalFlowPerformanceLevelNV perf;
    bool bidir;
    const char* presetName;
};

// Run an OF config, time 100 iters. Optionally read back MV/cost sanity once.
// If `contendRig` non-null, keep the compute queue pegged during the timed loop.
static bool runConfig(const Cfg& c, VkFormat costFmt, VkFormat flowFmt,
                      VkCommandPool ofPool, VkCommandPool gfxPool,
                      const std::vector<uint8_t>& frame0,
                      const std::vector<uint8_t>& frame1,
                      Stat& out, bool sanity, ContentionRig* contendRig) {
    const uint32_t gw = (c.w + 3) / 4;   // 4x4 grid output
    const uint32_t gh = (c.h + 3) / 4;

    Img in0{}, in1{}, mvF{}, mvB{}, costF{}, costB{};
    if (!createImg(in0, c.w, c.h, VK_FORMAT_B8G8R8A8_UNORM,
                   VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                   VK_OPTICAL_FLOW_USAGE_INPUT_BIT_NV)) return false;
    if (!createImg(in1, c.w, c.h, VK_FORMAT_B8G8R8A8_UNORM,
                   VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                   VK_OPTICAL_FLOW_USAGE_INPUT_BIT_NV)) return false;
    if (!createImg(mvF, gw, gh, flowFmt,
                   VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                   VK_OPTICAL_FLOW_USAGE_OUTPUT_BIT_NV)) return false;
    if (!createImg(costF, gw, gh, costFmt,
                   VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                   VK_OPTICAL_FLOW_USAGE_COST_BIT_NV)) return false;
    if (c.bidir) {
        if (!createImg(mvB, gw, gh, flowFmt,
                       VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                       VK_OPTICAL_FLOW_USAGE_OUTPUT_BIT_NV)) return false;
        if (!createImg(costB, gw, gh, costFmt,
                       VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                       VK_OPTICAL_FLOW_USAGE_COST_BIT_NV)) return false;
    }

    if (!uploadBGRA(in0, frame0, gfxPool)) return false;
    if (!uploadBGRA(in1, frame1, gfxPool)) return false;
    if (!toGeneral(mvF, gfxPool)) return false;
    if (!toGeneral(costF, gfxPool)) return false;
    if (c.bidir) { if (!toGeneral(mvB, gfxPool)) return false; if (!toGeneral(costB, gfxPool)) return false; }

    // session
    VkOpticalFlowSessionCreateInfoNV sci{};
    sci.sType = VK_STRUCTURE_TYPE_OPTICAL_FLOW_SESSION_CREATE_INFO_NV;
    sci.width = c.w; sci.height = c.h;
    sci.imageFormat = VK_FORMAT_B8G8R8A8_UNORM;
    sci.flowVectorFormat = flowFmt;
    sci.costFormat = costFmt;
    sci.outputGridSize = VK_OPTICAL_FLOW_GRID_SIZE_4X4_BIT_NV;
    sci.hintGridSize = VK_OPTICAL_FLOW_GRID_SIZE_UNKNOWN_NV;
    sci.performanceLevel = c.perf;
    sci.flags = VK_OPTICAL_FLOW_SESSION_CREATE_ENABLE_COST_BIT_NV;
    if (c.bidir) sci.flags |= VK_OPTICAL_FLOW_SESSION_CREATE_BOTH_DIRECTIONS_BIT_NV;

    VkOpticalFlowSessionNV sess = VK_NULL_HANDLE;
    VkResult rc = pfnCreateOFSession(g_dev, &sci, nullptr, &sess);
    if (rc != VK_SUCCESS) {
        std::printf("[FATAL] vkCreateOpticalFlowSessionNV(%ux%u perf=%d bidir=%d) -> %s\n",
                    c.w, c.h, (int)c.perf, (int)c.bidir, vkstr(rc));
        return false;
    }

    rc = pfnBindOFImage(g_dev, sess, VK_OPTICAL_FLOW_SESSION_BINDING_POINT_INPUT_NV, in0.view, VK_IMAGE_LAYOUT_GENERAL);
    if (rc != VK_SUCCESS) { std::printf("[FATAL] bind INPUT -> %s\n", vkstr(rc)); return false; }
    rc = pfnBindOFImage(g_dev, sess, VK_OPTICAL_FLOW_SESSION_BINDING_POINT_REFERENCE_NV, in1.view, VK_IMAGE_LAYOUT_GENERAL);
    if (rc != VK_SUCCESS) { std::printf("[FATAL] bind REFERENCE -> %s\n", vkstr(rc)); return false; }
    rc = pfnBindOFImage(g_dev, sess, VK_OPTICAL_FLOW_SESSION_BINDING_POINT_FLOW_VECTOR_NV, mvF.view, VK_IMAGE_LAYOUT_GENERAL);
    if (rc != VK_SUCCESS) { std::printf("[FATAL] bind FLOW_VECTOR -> %s\n", vkstr(rc)); return false; }
    rc = pfnBindOFImage(g_dev, sess, VK_OPTICAL_FLOW_SESSION_BINDING_POINT_COST_NV, costF.view, VK_IMAGE_LAYOUT_GENERAL);
    if (rc != VK_SUCCESS) { std::printf("[FATAL] bind COST -> %s\n", vkstr(rc)); return false; }
    if (c.bidir) {
        rc = pfnBindOFImage(g_dev, sess, VK_OPTICAL_FLOW_SESSION_BINDING_POINT_BACKWARD_FLOW_VECTOR_NV, mvB.view, VK_IMAGE_LAYOUT_GENERAL);
        if (rc != VK_SUCCESS) { std::printf("[FATAL] bind BWD_FLOW -> %s\n", vkstr(rc)); return false; }
        rc = pfnBindOFImage(g_dev, sess, VK_OPTICAL_FLOW_SESSION_BINDING_POINT_BACKWARD_COST_NV, costB.view, VK_IMAGE_LAYOUT_GENERAL);
        if (rc != VK_SUCCESS) { std::printf("[FATAL] bind BWD_COST -> %s\n", vkstr(rc)); return false; }
    }

    // timestamp query pool on the OF queue
    VkQueryPoolCreateInfo qpc{}; qpc.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    qpc.queryType = VK_QUERY_TYPE_TIMESTAMP; qpc.queryCount = 2;
    VkQueryPool qp; VKCHK(vkCreateQueryPool(g_dev, &qpc, nullptr, &qp));

    VkCommandBufferAllocateInfo cba{}; cba.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cba.commandPool = ofPool; cba.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; cba.commandBufferCount = 1;
    VkCommandBuffer cb; VKCHK(vkAllocateCommandBuffers(g_dev, &cba, &cb));

    VkOpticalFlowExecuteInfoNV ei{};
    ei.sType = VK_STRUCTURE_TYPE_OPTICAL_FLOW_EXECUTE_INFO_NV;
    ei.regionCount = 0; ei.pRegions = nullptr; ei.flags = 0;

    auto recordAndTime = [&](double& ms) -> bool {
        VkCommandBufferBeginInfo bi{}; bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        if (vkBeginCommandBuffer(cb, &bi) != VK_SUCCESS) return false;
        vkCmdResetQueryPool(cb, qp, 0, 2);
        vkCmdWriteTimestamp(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, qp, 0);
        pfnCmdOFExecute(cb, sess, &ei);
        vkCmdWriteTimestamp(cb, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, qp, 1);
        if (vkEndCommandBuffer(cb) != VK_SUCCESS) return false;
        if (!submitBlocking(cb, g_ofQueue)) return false;
        uint64_t ts[2] = {0,0};
        VkResult qr = vkGetQueryPoolResults(g_dev, qp, 0, 2, sizeof(ts), ts, sizeof(uint64_t),
                                            VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
        if (qr != VK_SUCCESS) { std::printf("[FATAL] query results -> %s\n", vkstr(qr)); return false; }
        ms = (double)(ts[1] - ts[0]) * (double)g_tsPeriod / 1.0e6;
        return true;
    };

    // warm up
    for (int i = 0; i < 10; ++i) { double d; if (!recordAndTime(d)) return false; }

    // optional contention: keep the compute queue pegged with a steady stream
    if (contendRig) kickContention(*contendRig, 8);

    std::vector<double> samples; samples.reserve(100);
    for (int i = 0; i < 100; ++i) {
        // re-kick contention periodically so it never drains during the 100 iters
        if (contendRig && (i % 10) == 0) kickContention(*contendRig, 4);
        double d; if (!recordAndTime(d)) return false;
        samples.push_back(d);
    }
    if (contendRig) drainContention(*contendRig);
    out = reduce(samples);

    // sanity: read back MV near image center + a cost sample
    if (sanity) {
        // copy mvF to a host-visible buffer
        VkDeviceSize mvSize = (VkDeviceSize)gw * gh * 4; // R16G16 = 4 bytes
        VkBuffer rb; VkDeviceMemory rbMem;
        VkBufferCreateInfo bci{}; bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bci.size = mvSize; bci.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        VKCHK(vkCreateBuffer(g_dev, &bci, nullptr, &rb));
        VkMemoryRequirements mr{}; vkGetBufferMemoryRequirements(g_dev, rb, &mr);
        VkMemoryAllocateInfo ai{}; ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        ai.allocationSize = mr.size;
        ai.memoryTypeIndex = findMemType(mr.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        VKCHK(vkAllocateMemory(g_dev, &ai, nullptr, &rbMem));
        VKCHK(vkBindBufferMemory(g_dev, rb, rbMem, 0));
        // cost buffer
        VkDeviceSize costSize = (VkDeviceSize)gw * gh * 1; // R8 = 1 byte
        VkBuffer cb2; VkDeviceMemory cb2Mem;
        bci.size = costSize;
        VKCHK(vkCreateBuffer(g_dev, &bci, nullptr, &cb2));
        vkGetBufferMemoryRequirements(g_dev, cb2, &mr);
        ai.allocationSize = mr.size;
        ai.memoryTypeIndex = findMemType(mr.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        VKCHK(vkAllocateMemory(g_dev, &ai, nullptr, &cb2Mem));
        VKCHK(vkBindBufferMemory(g_dev, cb2, cb2Mem, 0));

        VkCommandBuffer rcb; VkCommandBufferAllocateInfo rca{}; rca.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        rca.commandPool = gfxPool; rca.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; rca.commandBufferCount = 1;
        VKCHK(vkAllocateCommandBuffers(g_dev, &rca, &rcb));
        VkCommandBufferBeginInfo bi{}; bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        VKCHK(vkBeginCommandBuffer(rcb, &bi));
        VkBufferImageCopy cp{}; cp.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT,0,0,1 };
        cp.imageExtent = { gw, gh, 1 };
        vkCmdCopyImageToBuffer(rcb, mvF.img, VK_IMAGE_LAYOUT_GENERAL, rb, 1, &cp);
        vkCmdCopyImageToBuffer(rcb, costF.img, VK_IMAGE_LAYOUT_GENERAL, cb2, 1, &cp);
        VKCHK(vkEndCommandBuffer(rcb));
        submitBlocking(rcb, g_gfxQueue);

        int16_t* mv; uint8_t* cost;
        VKCHK(vkMapMemory(g_dev, rbMem, 0, mvSize, 0, (void**)&mv));
        VKCHK(vkMapMemory(g_dev, cb2Mem, 0, costSize, 0, (void**)&cost));
        // sample center + a couple points (S10.5 fixed: value/32 = pixels)
        auto sampleAt = [&](uint32_t px, uint32_t py) {
            uint32_t gx = px / 4, gy = py / 4;
            size_t idx = (size_t)gy * gw + gx;
            float mx = mv[idx*2+0] / 32.0f, my = mv[idx*2+1] / 32.0f;
            std::printf("    @(%4u,%4u) MV=(%+6.2f,%+6.2f)px  cost=%3u\n", px, py, mx, my, cost[idx]);
        };
        std::printf("  [sanity %ux%u %s %s] known motion = (+6.00,+4.00)px:\n",
                    c.w, c.h, c.presetName, c.bidir ? "bidir" : "fwd");
        sampleAt(c.w/2, c.h/2);
        sampleAt(c.w/4, c.h/4);
        sampleAt(3*c.w/4, 3*c.h/4);
        vkUnmapMemory(g_dev, rbMem); vkUnmapMemory(g_dev, cb2Mem);
        vkFreeCommandBuffers(g_dev, gfxPool, 1, &rcb);
        vkDestroyBuffer(g_dev, rb, nullptr); vkFreeMemory(g_dev, rbMem, nullptr);
        vkDestroyBuffer(g_dev, cb2, nullptr); vkFreeMemory(g_dev, cb2Mem, nullptr);
    }

    vkFreeCommandBuffers(g_dev, ofPool, 1, &cb);
    vkDestroyQueryPool(g_dev, qp, nullptr);
    pfnDestroyOFSession(g_dev, sess, nullptr);
    destroyImg(in0); destroyImg(in1); destroyImg(mvF); destroyImg(costF);
    if (c.bidir) { destroyImg(mvB); destroyImg(costB); }
    return true;
}

// ── main ──────────────────────────────────────────────────────────────────────
static bool g_validate = false;

static bool run() {
    // 1. instance
    std::vector<const char*> instExt = { VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME };
    std::vector<const char*> instLayers;
    if (g_validate) instLayers.push_back("VK_LAYER_KHRONOS_validation");
    VkApplicationInfo app{}; app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName = "nvofa_bench"; app.apiVersion = VK_API_VERSION_1_3;
    VkInstanceCreateInfo ici{}; ici.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ici.pApplicationInfo = &app;
    ici.enabledExtensionCount = (uint32_t)instExt.size(); ici.ppEnabledExtensionNames = instExt.data();
    ici.enabledLayerCount = (uint32_t)instLayers.size(); ici.ppEnabledLayerNames = instLayers.data();
    VkResult rc = vkCreateInstance(&ici, nullptr, &g_inst);
    if (rc != VK_SUCCESS && g_validate) {
        std::printf("[warn] instance create with validation failed (%s); retrying without\n", vkstr(rc));
        ici.enabledLayerCount = 0;
        rc = vkCreateInstance(&ici, nullptr, &g_inst);
    }
    if (rc != VK_SUCCESS) { std::printf("[FATAL] vkCreateInstance -> %s\n", vkstr(rc)); return false; }

    // 2. pick the 4090
    uint32_t npd = 0; vkEnumeratePhysicalDevices(g_inst, &npd, nullptr);
    std::vector<VkPhysicalDevice> pds(npd); vkEnumeratePhysicalDevices(g_inst, &npd, pds.data());
    for (auto pd : pds) {
        VkPhysicalDeviceProperties pr{}; vkGetPhysicalDeviceProperties(pd, &pr);
        std::string nm = pr.deviceName;
        if (nm.find("4090") != std::string::npos) { g_phys = pd; break;
        }
    }
    if (!g_phys && npd) g_phys = pds[0];
    if (!g_phys) { std::printf("[FATAL] no physical device\n"); return false; }
    VkPhysicalDeviceProperties pr{}; vkGetPhysicalDeviceProperties(g_phys, &pr);
    std::printf("Device: %s  (driver %u)\n", pr.deviceName, pr.driverVersion);
    g_tsPeriod = pr.limits.timestampPeriod;
    std::printf("timestampPeriod: %.3f ns\n", g_tsPeriod);
    vkGetPhysicalDeviceMemoryProperties(g_phys, &g_memProps);

    // 3. check OF extension + feature + props
    uint32_t nExt = 0; vkEnumerateDeviceExtensionProperties(g_phys, nullptr, &nExt, nullptr);
    std::vector<VkExtensionProperties> exts(nExt);
    vkEnumerateDeviceExtensionProperties(g_phys, nullptr, &nExt, exts.data());
    bool hasOF = false, hasSync2 = false;
    for (auto& e : exts) {
        if (!std::strcmp(e.extensionName, VK_NV_OPTICAL_FLOW_EXTENSION_NAME)) hasOF = true;
        if (!std::strcmp(e.extensionName, VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME)) hasSync2 = true;
    }
    if (!hasOF) { std::printf("[FATAL] VK_NV_optical_flow NOT exposed on this device\n"); return false; }

    VkPhysicalDeviceOpticalFlowPropertiesNV ofProps{};
    ofProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_OPTICAL_FLOW_PROPERTIES_NV;
    VkPhysicalDeviceProperties2 p2{}; p2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    p2.pNext = &ofProps; vkGetPhysicalDeviceProperties2(g_phys, &p2);
    std::printf("OF props: cost=%d bidir=%d gridSizes=0x%x  range=[%ux%u .. %ux%u]\n",
                ofProps.costSupported, ofProps.bidirectionalFlowSupported,
                ofProps.supportedOutputGridSizes,
                ofProps.minWidth, ofProps.minHeight, ofProps.maxWidth, ofProps.maxHeight);

    // 4. queue families
    uint32_t nqf = 0; vkGetPhysicalDeviceQueueFamilyProperties(g_phys, &nqf, nullptr);
    std::vector<VkQueueFamilyProperties> qfs(nqf);
    vkGetPhysicalDeviceQueueFamilyProperties(g_phys, &nqf, qfs.data());
    for (uint32_t i = 0; i < nqf; ++i) {
        if ((qfs[i].queueFlags & VK_QUEUE_OPTICAL_FLOW_BIT_NV) && g_ofQF == ~0u) g_ofQF = i;
        if ((qfs[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && (qfs[i].queueFlags & VK_QUEUE_COMPUTE_BIT) && g_gfxQF == ~0u) g_gfxQF = i;
    }
    if (g_ofQF == ~0u) { std::printf("[FATAL] no VK_QUEUE_OPTICAL_FLOW_BIT_NV queue family\n"); return false; }
    if (g_gfxQF == ~0u) { std::printf("[FATAL] no graphics+compute queue family\n"); return false; }
    std::printf("queues: OF family=%u (count %u, tsValidBits %u)  GFX family=%u\n",
                g_ofQF, qfs[g_ofQF].queueCount, qfs[g_ofQF].timestampValidBits, g_gfxQF);
    if (qfs[g_ofQF].timestampValidBits == 0) {
        std::printf("[warn] OF queue has 0 timestampValidBits — timestamps on OF queue invalid!\n");
    }

    // 5. device with OF feature + extension
    float prio = 1.0f;
    VkDeviceQueueCreateInfo qci[2]{};
    qci[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO; qci[0].queueFamilyIndex = g_ofQF; qci[0].queueCount = 1; qci[0].pQueuePriorities = &prio;
    qci[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO; qci[1].queueFamilyIndex = g_gfxQF; qci[1].queueCount = 1; qci[1].pQueuePriorities = &prio;
    uint32_t nqci = (g_ofQF == g_gfxQF) ? 1 : 2;

    std::vector<const char*> devExt = { VK_NV_OPTICAL_FLOW_EXTENSION_NAME };
    if (hasSync2) devExt.push_back(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME);

    VkPhysicalDeviceOpticalFlowFeaturesNV ofFeat{};
    ofFeat.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_OPTICAL_FLOW_FEATURES_NV;
    ofFeat.opticalFlow = VK_TRUE;
    VkPhysicalDeviceSynchronization2Features sync2{};
    sync2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
    sync2.synchronization2 = VK_TRUE;
    if (hasSync2) ofFeat.pNext = &sync2;

    VkDeviceCreateInfo dci{}; dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dci.pNext = &ofFeat;
    dci.queueCreateInfoCount = nqci; dci.pQueueCreateInfos = qci;
    dci.enabledExtensionCount = (uint32_t)devExt.size(); dci.ppEnabledExtensionNames = devExt.data();
    rc = vkCreateDevice(g_phys, &dci, nullptr, &g_dev);
    if (rc != VK_SUCCESS) { std::printf("[FATAL] vkCreateDevice -> %s\n", vkstr(rc)); return false; }
    vkGetDeviceQueue(g_dev, g_ofQF, 0, &g_ofQueue);
    vkGetDeviceQueue(g_dev, g_gfxQF, 0, &g_gfxQueue);

    // 6. load OF PFNs
    pfnGetOFFormats = (PFN_vkGetPhysicalDeviceOpticalFlowImageFormatsNV)vkGetInstanceProcAddr(g_inst, "vkGetPhysicalDeviceOpticalFlowImageFormatsNV");
    pfnCreateOFSession = (PFN_vkCreateOpticalFlowSessionNV)vkGetDeviceProcAddr(g_dev, "vkCreateOpticalFlowSessionNV");
    pfnDestroyOFSession = (PFN_vkDestroyOpticalFlowSessionNV)vkGetDeviceProcAddr(g_dev, "vkDestroyOpticalFlowSessionNV");
    pfnBindOFImage = (PFN_vkBindOpticalFlowSessionImageNV)vkGetDeviceProcAddr(g_dev, "vkBindOpticalFlowSessionImageNV");
    pfnCmdOFExecute = (PFN_vkCmdOpticalFlowExecuteNV)vkGetDeviceProcAddr(g_dev, "vkCmdOpticalFlowExecuteNV");
    if (!pfnGetOFFormats || !pfnCreateOFSession || !pfnDestroyOFSession || !pfnBindOFImage || !pfnCmdOFExecute) {
        std::printf("[FATAL] failed to load OF PFNs (get=%p create=%p destroy=%p bind=%p exec=%p)\n",
                    (void*)pfnGetOFFormats,(void*)pfnCreateOFSession,(void*)pfnDestroyOFSession,(void*)pfnBindOFImage,(void*)pfnCmdOFExecute);
        return false;
    }

    // 7. query supported OF formats for each usage class
    auto dumpFormats = [&](VkOpticalFlowUsageFlagsNV usage, const char* label) {
        VkOpticalFlowImageFormatInfoNV fi{}; fi.sType = VK_STRUCTURE_TYPE_OPTICAL_FLOW_IMAGE_FORMAT_INFO_NV;
        fi.usage = usage;
        uint32_t n = 0; pfnGetOFFormats(g_phys, &fi, &n, nullptr);
        std::vector<VkOpticalFlowImageFormatPropertiesNV> props(n);
        for (auto& p : props) p.sType = VK_STRUCTURE_TYPE_OPTICAL_FLOW_IMAGE_FORMAT_PROPERTIES_NV;
        pfnGetOFFormats(g_phys, &fi, &n, props.data());
        std::printf("  OF formats [%s]: ", label);
        for (auto& p : props) std::printf("%d ", (int)p.format);
        std::printf("\n");
    };
    std::printf("Supported OF image formats:\n");
    dumpFormats(VK_OPTICAL_FLOW_USAGE_INPUT_BIT_NV,  "INPUT");
    dumpFormats(VK_OPTICAL_FLOW_USAGE_OUTPUT_BIT_NV, "OUTPUT(flow)");
    dumpFormats(VK_OPTICAL_FLOW_USAGE_COST_BIT_NV,   "COST");

    const VkFormat flowFmt = VK_FORMAT_R16G16_SFIXED5_NV;
    const VkFormat costFmt = VK_FORMAT_R8_UINT;

    // 8. command pools
    VkCommandPoolCreateInfo ofPoolCI{}; ofPoolCI.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    ofPoolCI.queueFamilyIndex = g_ofQF; ofPoolCI.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    VkCommandPool ofPool; VKCHK(vkCreateCommandPool(g_dev, &ofPoolCI, nullptr, &ofPool));
    VkCommandPoolCreateInfo gfxPoolCI = ofPoolCI; gfxPoolCI.queueFamilyIndex = g_gfxQF;
    VkCommandPool gfxPool; VKCHK(vkCreateCommandPool(g_dev, &gfxPoolCI, nullptr, &gfxPool));

    // 9. the matrix
    struct Res { uint32_t w, h; const char* name; };
    Res resList[] = { {1920,1080,"1080p"}, {2560,1440,"1440p"}, {3840,2160,"4K"} };
    struct Preset { VkOpticalFlowPerformanceLevelNV lvl; const char* name; };
    Preset presets[] = {
        { VK_OPTICAL_FLOW_PERFORMANCE_LEVEL_FAST_NV,   "FAST"   },
        { VK_OPTICAL_FLOW_PERFORMANCE_LEVEL_MEDIUM_NV, "MEDIUM" },
        { VK_OPTICAL_FLOW_PERFORMANCE_LEVEL_SLOW_NV,   "SLOW"   },
    };

    std::printf("\n================ NVOFA TIMING TABLE (100 iters, OF-queue timestamps) ================\n");
    std::printf("| %-6s | %-6s | %-5s | cost | %8s | %8s | %8s |\n",
                "res","preset","dir","mean_ms","med_ms","p99_ms");
    std::printf("|--------|--------|-------|------|----------|----------|----------|\n");

    bool firstSanity = true;
    for (auto& res : resList) {
        // pattern for this resolution: frame0 unshifted, frame1 shifted (+6,+4)
        std::vector<uint8_t> f0, f1;
        fillPattern(f0, res.w, res.h, 0, 0);
        fillPattern(f1, res.w, res.h, 6, 4);
        for (auto& pst : presets) {
            for (int dir = 0; dir < 2; ++dir) {
                Cfg c{ res.w, res.h, pst.lvl, dir == 1, pst.name };
                Stat s;
                bool sanity = firstSanity && (pst.lvl == VK_OPTICAL_FLOW_PERFORMANCE_LEVEL_FAST_NV);
                if (!runConfig(c, costFmt, flowFmt, ofPool, gfxPool, f0, f1, s, sanity, nullptr)) {
                    std::printf("| %-6s | %-6s | %-5s | yes  | %8s | %8s | %8s |\n",
                                res.name, pst.name, dir?"bidir":"fwd", "FAIL","FAIL","FAIL");
                    return false;  // an OF failure is load-bearing; stop and report exact call
                }
                std::printf("| %-6s | %-6s | %-5s | yes  | %8.3f | %8.3f | %8.3f |\n",
                            res.name, pst.name, dir?"bidir":"fwd", s.mean, s.med, s.p99);
                if (sanity && dir==1) firstSanity = false;
            }
        }
    }

    // 10. contention test: BOTH_DIRECTIONS + cost + FAST at each res, idle vs pegged compute
    std::printf("\n================ CONTENTION TEST (bidir+cost+FAST) ================\n");
    std::printf("OFA ms while a heavy compute load saturates the graphics/compute queue.\n");
    ContentionRig rig;
    bool rigOK = buildContention(rig);
    if (!rigOK) std::printf("[warn] contention rig build failed; skipping contention test\n");
    // Self-check: prove the dummy load is actually a HEAVY, sustained compute job.
    // Time how long ONE contention submit (64 dispatches x 1M threads x 4096 FMA)
    // takes to fully drain on the GPU. If this >> the OFA's ~4 ms window, the
    // compute engine is genuinely pegged for the entire timed loop.
    if (rigOK) {
        kickContention(rig, 1);
        auto t0 = std::chrono::steady_clock::now();
        vkQueueWaitIdle(g_gfxQueue);
        auto t1 = std::chrono::steady_clock::now();
        double oneMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
        drainContention(rig);
        std::printf("contention load self-check: 1 submit drains in %.2f ms of GPU compute "
                    "(we keep 8-12 queued => the compute engine stays pegged through the OFA loop)\n",
                    oneMs);
    }
    std::printf("| %-6s | %10s | %10s | %s |\n", "res", "idle_med", "contended", "slowdown");
    std::printf("|--------|------------|------------|----------|\n");
    for (auto& res : resList) {
        std::vector<uint8_t> f0, f1;
        fillPattern(f0, res.w, res.h, 0, 0);
        fillPattern(f1, res.w, res.h, 6, 4);
        Cfg c{ res.w, res.h, VK_OPTICAL_FLOW_PERFORMANCE_LEVEL_FAST_NV, true, "FAST" };
        Stat idle, cont;
        if (!runConfig(c, costFmt, flowFmt, ofPool, gfxPool, f0, f1, idle, false, nullptr)) return false;
        if (rigOK) {
            if (!runConfig(c, costFmt, flowFmt, ofPool, gfxPool, f0, f1, cont, false, &rig)) return false;
        }
        double slow = (rigOK && idle.med > 0) ? 100.0*(cont.med - idle.med)/idle.med : 0.0;
        if (rigOK)
            std::printf("| %-6s | %10.3f | %10.3f | %+6.1f%% |\n", res.name, idle.med, cont.med, slow);
        else
            std::printf("| %-6s | %10.3f | %10s | %8s |\n", res.name, idle.med, "n/a", "n/a");
    }
    if (rigOK) { vkQueueWaitIdle(g_gfxQueue); destroyContention(rig); }

    vkDestroyCommandPool(g_dev, ofPool, nullptr);
    vkDestroyCommandPool(g_dev, gfxPool, nullptr);
    return true;
}

int main(int argc, char** argv) {
    for (int i = 1; i < argc; ++i)
        if (!std::strcmp(argv[i], "--validate")) g_validate = true;
    std::printf("nvofa_bench  (validate=%d)\n\n", g_validate);
    bool ok = run();
    if (g_dev) { vkDeviceWaitIdle(g_dev); vkDestroyDevice(g_dev, nullptr); }
    if (g_inst) vkDestroyInstance(g_inst, nullptr);
    std::printf("\n%s\n", ok ? "[done]" : "[FAILED — see error above]");
    return ok ? 0 : 1;
}
