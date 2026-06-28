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
BOOL WINAPI console_ctrl_handler(DWORD ctrl){
    if(ctrl==CTRL_C_EVENT||ctrl==CTRL_CLOSE_EVENT||ctrl==CTRL_BREAK_EVENT){ g_quit=true; return TRUE; }
    return FALSE;
}
// Made with my soul - Swately <3
