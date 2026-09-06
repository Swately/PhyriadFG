// PhyriadFG core/globals layer: the single definitions of the process-wide quit/device-lost/util/overlay
// state + vk_live + the console Ctrl handler (declared in core/globals.hpp).
#include "core/globals.hpp"
#include <cstdio>   // std::printf (vk_live one-shot message)

volatile bool g_quit=false;
std::atomic<int> g_gpu_a_util{-1};
std::atomic<int> g_gov_floor{0};   // control-word: P-published util→tier floor, F-read (advisory)
std::atomic<uint32_t> g_ov_in{0}, g_ov_out{0};
std::atomic<bool> g_device_lost{false};

bool vk_live(VkResult r) noexcept {
    if(r==VK_ERROR_DEVICE_LOST){
        if(!g_device_lost.exchange(true))
            std::printf("[ra] VK_ERROR_DEVICE_LOST -- graceful exit (the game keeps running; PhyriadFG is an external overlay)\n");
        g_quit=true;
        return false;
    }
    return true;
}
namespace {
    constexpr uint64_t kWaitSliceNs = 20ull * 1000ull * 1000ull;   // 20 ms per slice
    constexpr int      kQuitSlices  = 100;                          // 2 s past a quit request → abandon
    std::atomic<bool>  g_wait_abandon_said{false};
    template<class W> bool wait_live_impl(W wait) noexcept {
        int quit_slices = 0;
        for(;;){
            const VkResult r = wait(kWaitSliceNs);
            if(r==VK_SUCCESS) return true;
            if(r!=VK_TIMEOUT){ vk_live(r); return false; }     // VK_ERROR_DEVICE_LOST (or any error): latch + abandon
            if(g_device_lost.load()) return false;               // the loss was seen on another thread: this object may never signal
            if(g_quit && ++quit_slices>=kQuitSlices){
                if(!g_wait_abandon_said.exchange(true))
                    std::printf("[ra] vk_wait_live: an object stayed unsignalled 2 s after the quit request -- abandoning the wait (teardown proceeds)\n");
                return false;
            }
        }
    }
}
bool vk_wait_live(VkDevice dev, VkFence f) noexcept {
    return wait_live_impl([&](uint64_t ns){ return vkWaitForFences(dev,1,&f,VK_TRUE,ns); });
}
bool vk_wait_sem_live(VkDevice dev, const VkSemaphoreWaitInfo& wi) noexcept {
    return wait_live_impl([&](uint64_t ns){ return vkWaitSemaphores(dev,&wi,ns); });
}
BOOL WINAPI console_ctrl_handler(DWORD ctrl){
    if(ctrl==CTRL_C_EVENT||ctrl==CTRL_CLOSE_EVENT||ctrl==CTRL_BREAK_EVENT){ g_quit=true; return TRUE; }
    return FALSE;
}
// Made with my soul - Swately <3

VkDebugUtilsMessengerEXT g_dbg_messenger = VK_NULL_HANDLE;   // --validation (R2)
