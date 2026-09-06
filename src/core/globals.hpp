#pragma once
// PhyriadFG core/globals layer: process-wide quit / device-lost signalling, the GPU-util + fps-overlay
// atomics, the non-throwing device-lost detector (vk_live), and the console Ctrl handler. Globals are
// `extern`-declared here; their single definition lives in globals.cpp. The submit helpers
// (core/vk_util) call vk_live, so this header is the one declaration point.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>          // BOOL / WINAPI / DWORD (console_ctrl_handler)
#include <vulkan/vulkan.h>    // VkResult (vk_live)
#include <atomic>
#include <cstdint>

// Quit is driven by the console Ctrl handler (sets g_quit); the main loop then signals
// g_quit_threads and joins the workers (a clean exit).
extern volatile bool g_quit;
// --load-governor: the live measured 4090 (device A) utilization, published by the PRESENT thread
// (NVML reader), read by the F-thread's tier-5 escalation. -1 = no reading yet.
extern std::atomic<int> g_gpu_a_util;
// Control-word: the util-driven GRADUATED tier FLOOR (0/3/4/5), the single authoritative governor
// decode. ONE producer (the PRESENT thread — it owns the util read + the dwell hysteresis) → ONE
// consumer (the F-thread applies it as max(cpu_ladder, floor)). Advisory, lock-free (same contract as
// g_gpu_a_util). 0 = no floor (load_governor off / util cool / NVML not armed) → F's max(...,0) is a
// no-op. P decodes the util → F just reads this. The util LEADS; one decode site, never two.
extern std::atomic<int> g_gov_floor;

// The single governor util→floor decode (used by the PRESENT thread to compute g_gov_floor, and by
// the present-thread warp_light so both arms read ONE mapping, never two). Pure. -1 util → no floor.
// Bands: >=gov_util → 5 (deep shed), >=gov_util-6 → 4 (deficit shed), >=gov_util-12 → 3 (period-4
// holon); else 0. gov_util is cfg.gov_util (clamped [50,100] at parse).
inline int governor_floor_for_util(int util, float gov_util) noexcept {
    if (util < 0) return 0;
    const float u = (float)util;
    if (u >= gov_util)         return 5;
    if (u >= gov_util - 6.f)   return 4;
    if (u >= gov_util - 12.f)  return 3;
    return 0;
}
// FPS-OVERLAY (--fps-overlay): in = real-captured fps, out = presented fps. Published by the
// PRESENT thread's per-second stat blocks; read by the overlay dispatch. 0 = no reading yet.
extern std::atomic<uint32_t> g_ov_in, g_ov_out;
// Device-lost graceful exit: a terminal device loss was detected. File-scope so the present threads +
// the WGC callback can set it.
extern std::atomic<bool> g_device_lost;

// Non-throwing detector. Returns true to CONTINUE, false to ABORT (callers may ignore the bool —
// setting g_quit is what unwinds the threads). Trips ONLY on VK_ERROR_DEVICE_LOST; identity on the
// happy path (VK_SUCCESS) → wrapped sites are byte-identical when no device is lost.
bool vk_live(VkResult r) noexcept;
// vk_wait_live / vk_wait_sem_live: THE fence / timeline-semaphore wait for every worker thread. Bounded 20 ms
// slices, so a thread never sits in a driver call that cannot return: on a LOST device a fence whose submit was
// refused never signals, and vkWaitForFences(UINT64_MAX) there is the hang the --tdr-test exposed (2026-09-05:
// VK_ERROR_DEVICE_LOST latched and printed, then a worker join never returned). Returns true when the wait
// completed; false when the device is lost — seen here (vk_live latches it) or on ANY other thread meanwhile —
// or when the object is still unsignalled 2 s after a quit request (said once; the teardown proceeds). On a
// healthy device a signalled object returns on its slice: byte-identical behaviour to the unbounded wait.
bool vk_wait_live(VkDevice dev, VkFence f) noexcept;
bool vk_wait_sem_live(VkDevice dev, const VkSemaphoreWaitInfo& wi) noexcept;
BOOL WINAPI console_ctrl_handler(DWORD ctrl);
// Made with my soul - Swately <3

// --validation (R2): the debug-utils messenger handle, so teardown destroys it BEFORE the instance
// (the layer reports an undestroyed messenger as a leaked object — found first-hand 2026-09-03).
extern VkDebugUtilsMessengerEXT g_dbg_messenger;
