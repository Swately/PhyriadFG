#pragma once
// PhyriadFG core/globals layer (STEP 3 of the layered separation — PURE RELOCATION from
// src/core/main.cpp; no logic change). Process-wide quit / device-lost signalling, the GPU-util
// + fps-overlay atomics, the non-throwing device-lost detector (vk_live), and the console Ctrl
// handler. Globals are `extern`-declared here; their single definition lives in globals.cpp.
// The submit helpers (core/vk_util) call vk_live, so this header is the one declaration point
// (it replaces the old in-main forward declaration of vk_live).
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

// STAGE-45b: quit is driven by the console Ctrl handler (sets g_quit); the main loop then signals
// g_quit_threads and joins the workers (a CLEAN exit).
extern volatile bool g_quit;
// STAGE-101 (--load-governor): the live measured 4090 (device A) utilization, published by the
// PRESENT thread (NVML reader), read by the F-thread's tier-5 escalation. -1 = no reading yet.
extern std::atomic<int> g_gpu_a_util;
// STAGE-104 control-word (2026-06-28 layer-sync): the util-driven GRADUATED tier FLOOR (0/3/4/5),
// the single authoritative governor decode. ONE producer (the PRESENT thread — it owns the util
// read + the dwell hysteresis) → ONE consumer (the F-thread applies it as max(cpu_ladder, floor)).
// Advisory, lock-free (same contract as g_gpu_a_util). 0 = no floor (load_governor off / util cool /
// NVML not armed) → F's max(...,0) is a no-op = byte-identical. Replaces the F-LOCAL gov_floor that
// the audit found gated behind bwd_skipping (the CPU-budget gate the STAGE-104 decoupling claimed to
// bypass): in combat the 4090 pegs while the F EMA stays under budget, so the F-local floor never
// fired. P decodes the util → F just reads this. The util LEADS; one decode site, never two.
extern std::atomic<int> g_gov_floor;

// The single governor util→floor decode (used by the PRESENT thread to compute g_gov_floor, and by
// the present-thread warp_light so both arms read ONE mapping, never two). Pure. -1 util → no floor.
// Mirrors the STAGE-104 bands exactly: >=gov_util → 5 (deep shed), >=gov_util-6 → 4 (deficit shed),
// >=gov_util-12 → 3 (period-4 holon); else 0. gov_util is cfg.gov_util (clamped [50,100] at parse).
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
// P0 (device-lost graceful exit; DEVICE_LOST_RECOVERY_RISK_REGISTER.md): a terminal device loss
// was detected. File-scope so the present threads + the WGC callback can set it.
extern std::atomic<bool> g_device_lost;

// Non-throwing detector. Returns true to CONTINUE, false to ABORT (callers may ignore the bool —
// setting g_quit is what unwinds the threads). Trips ONLY on VK_ERROR_DEVICE_LOST; identity on the
// happy path (VK_SUCCESS) → wrapped sites are byte-identical when no device is lost.
bool vk_live(VkResult r) noexcept;
BOOL WINAPI console_ctrl_handler(DWORD ctrl);
// Made with my soul - Swately <3
