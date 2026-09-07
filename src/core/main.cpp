// PhyriadFG — continuous desktop capture → frame-gen → upscale → present.
//
// Loop: DXGI/WGC capture → format routing → host-staged cross-device transfer →
// OpticalFlowPipeline FG (pyramid + confidence + agreement) → optional 2×
// bilinear/Lanczos upscale → present. Paced to source. Runs until close.
//
// Two-GPU: GPU A (primary, LUID-match) = capture+convert+present;
//          GPU B (assist, first non-primary discrete or --assist-gpu) = frame-gen;
//          iGPU (optional) = upscale. Shared VK_EXT_external_memory_host bridge
//          (zero-copy cross-device).
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
// WDA_EXCLUDEFROMCAPTURE (Win10 2004+) — older MinGW winuser.h omits it.
#ifndef WDA_EXCLUDEFROMCAPTURE
#define WDA_EXCLUDEFROMCAPTURE 0x00000011
#endif
#include <d3d11.h>
#include <d3d11_4.h>   // ID3D11Multithread (capture ring: shared immediate-context protection)
#include <dxgi1_2.h>
#include <timeapi.h>   // timeBeginPeriod(1) — 1ms scheduler granularity for paced sleeps
#include <pdh.h>       // per-adapter GPU% via the "\GPU Engine(*)" perf counter
#include <pdhmsg.h>    // PDH_MORE_DATA / PDH_CSTATUS_VALID_DATA
#include <dwmapi.h>    // --pace-vblank: DwmGetCompositionTimingInfo — the true vblank phase (qpcVBlank/qpcRefreshPeriod) for the grid phase-lock. Cold/periodic query only.
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>

#include <phyriad/render/vulkan/OpticalFlowPipeline.hpp>
// The app is C++23; Phyriad pillars are directly consumable.
// hal::cpu_wait_for_ns = the pacing spin-finish.
#include <phyriad/hal/CpuWait.hpp>
// The hal fp16 codec: hal::f16_to_f32 / f32_to_f16 are the portable scalar codec (safe without
// -mavx2/-mf16c). The F16C batch decode is reached via ra::decode_f16 (ra_simd.cpp — the one
// flagged TU). main.cpp stays portable / dual-build (no arch flags on this TU).
#include <phyriad/hal/Simd.hpp>
#include "ra_simd.hpp"
#include "telemetry_csv.hpp"          // comprehensive per-frame telemetry CSV export (--csv)
#include "compat_reason.hpp"          // cold-path compatibility reason-code taxonomy (named-reason-on-every-failure-bail)
// The present pillar — PresentSurface owns the panel (dcomp-ct HWND + composition swapchain +
// WDA + the one CopyResource/frame bridge). The app produces the frame into a VK→D3D11 shared
// texture and hands the NT handle to submit().
#include <phyriad/render/present/PresentSurface.hpp>
// RT-thread pinning: the topology pillar supplies
// phyriad::hw::pin_current_thread / elevate_thread_rt / p_cores /
// optimal_producer_consumer_pair — used to bind the C/F/P real-time threads to fixed
// cores (P also RT-elevated) and kill the scheduler-migration present slip. Gated on
// cfg.pin_threads (--no-pin = bare-thread cadence, byte-identical).
#include <phyriad/topology/HardwareTopology.hpp>
// Router note: the gpu pillar's break_even_decide() for the FG offload decision —
// at FG's AI≈2.5 FLOP/byte (vs the ~596 crossover) it returns offload=false ALWAYS,
// a constant. Inlined as such.
#include "hdr_convert_spv.hpp"
#include "upscale_bilinear_spv.hpp"
#include "upscale_lanczos_spv.hpp"
#include "igpu_convert_pack_spv.hpp"  // iGPU fused convert+pack
#include "igpu_field_spv.hpp"         // iGPU image-derived contour field producer
#include "wap_fill_spv.hpp"            // A tints the iGPU contour field onto wapOutA (--afill)
#include "unpack_packed_spv.hpp"       // packed → RGBA8 unpack
#include "mv_smooth_spv.hpp"           // temporal MV EMA
#include "wap_warp_spv.hpp"            // warp-at-presenter (A re-warps per tick)
#include "mv_median_spv.hpp"           // 3x3 vector-median on the uploaded MV field
#include "nvofa_convert_spv.hpp"       // --nvofa: land the HW OFA output on the OFP contract
#include "gme_reduce_spv.hpp"          // gme-gpu normal-equation reduce (device B)
#include "gme_solve_spv.hpp"           // gme-gpu 3×3 Cramer solve (device B)
#include "gme_dissidence_spv.hpp"      // gme-gpu per-block dissidence R8 mask (device B)
#include "overlay_fps_spv.hpp"         // --fps-overlay: "in->out" fps overlay (compute RMW on Apresent)

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>   // std::ref for std::thread(run_capture, std::ref(ctx))
#include <malloc.h>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// --- WGC / WinRT (MSVC + Windows SDK only) ---------------------------------------------------
// The WinRT include block + namespace aliases + IDirect3DDxgiInterfaceAccess + the WgcCtx
// staging-ring struct live in capture/wgc_ctx.hpp so run_capture (capture/capture.cpp) can
// name the type. Included HERE (winrt include order matters); fg_context.hpp re-includes it
// (pragma-once no-op).
#include "capture/wgc_ctx.hpp"

// ─── Config / CLI (cli/cli.{hpp,cpp}) ───
#include "control/cli.hpp"
// The vk-utility + device + globals infra.
#include "core/vk_util.hpp"
#include "core/device.hpp"
#include "core/globals.hpp"
#include "flow/flow.hpp"          // FLOW factories/structs (gme_fit/NvofaProvider/MvSmoothPipe/GmePipe/MedianPipe)
#include "capture/capture.hpp"     // D3D11/DXGI interop (OutInfo/D3D/d3d_init/find_window/d3d_shutdown/staging) + iGPU ConvPackPipe/UnpackPipe
#include "generate/warp_blend.hpp" // WapPipe/FieldPipe/FillPipe factories + matte_mass_count/gme_dispct_from_mask CPU stat helpers
#include "present/present.hpp"      // present-side UpPipe upscale (bilinear/lanczos) factory
#include "instrument/instrument.hpp" // dump_bmp/dump_rgba diagnostic frame-dump helpers
// FgContext - the shared cross-thread state as references to main()'s locals;
// run_capture/F/P alias them so the bodies stay identical.
#include "core/fg_context.hpp"
// E1: the init-seq ownership structs (the former hoisted declaration block).
#include "core/app_init.hpp"
#include "flow/flow_set.hpp"      // R5: FlowRing / FlowSet (STAGE_CONTRACT §1)
#include "ingest/frames.hpp"    // R6: FrameRing / RawRing + RealFrame / RawFrame (STAGE_CONTRACT §1)
// ─── kGenRing lives in flow/flow.{hpp,cpp} ──

// The object-/scene-/shape-field holon constants live in flow/flow.hpp so run_flow can name
// them; main() sees them via the flow/flow.hpp include above. (kObjRimSpreadMin lives in
// cli/cli.hpp.)


// ─── now_ms() lives in core/vk_util.hpp ──
// ─── The frame-holon — half-float decode + global affine motion fit ─────────────
// hostMV[gen] is the RAW bytes of OFP's motion_image() copied via vkCmdCopyImageToBuffer: an
// RG16F image of mvw×mvh texels, tightly packed (the copy uses full_bic with no row padding,
// so the stride IS mvw·4 bytes). Each texel = 2× IEEE-754 binary16 (R=mv_x, G=mv_y) in FULL-RES
// PIXEL units (the OFP MV convention, A→B).
//
// ─── half_to_float/float_to_half forwarders live in core/vk_util.hpp ──

// ─── gme_fit_affine (+ kChangeGateSadZ + decode-once scratch) lives in flow/flow.{hpp,cpp} ──
// ─── matte_mass_count / gme_dispct_from_mask (CPU stat helpers) live in warp_blend/warp_blend.{hpp,cpp} ──
// ─── dump_bmp / dump_rgba (diagnostic frame dumps) live in instrument/instrument.{hpp,cpp} ──
// ─── rel<T> (COM release template) lives in core/vk_util.hpp ──

// ─── enum Route + route_for() live in core/vk_util.hpp ──

// ─── D3D11/DXGI interop (OutInfo / D3D / d3d_init / find_window_by_substr / d3d_shutdown / d3d_staging / d3d_staging_on) lives in capture/capture.{hpp,cpp} ──

// ─── pick_mem / VDev / vdev_create / vdev_destroy live in core/device.{hpp,cpp} ──

// ─── Img/HBuf + create/destroy + img_barrier/oneshot/submit_wait/submit_wait_q2/full_bic live in core/vk_util.{hpp,cpp}; vk_live fwd-decl is in core/globals.hpp ──

// ─── NvofaProvider (+ nvofa_img/create/write_set/destroy/alloc_cmds/nvofa_run) lives in flow/flow.{hpp,cpp} ──

// ─── UpPipe (present-side upscale bilinear/lanczos) lives in present/present.{hpp,cpp} ──
// ─── ConvPackPipe (iGPU convert+pack) lives in capture/capture.{hpp,cpp} ──
// ─── FieldPipe (iGPU contour field) + FillPipe (A-side field visualizer) live in warp_blend/warp_blend.{hpp,cpp} ──
// ─── UnpackPipe (packed→RGBA8) lives in capture/capture.{hpp,cpp} ──

// ─── MvSmoothPipe (+ mvsm_create/destroy) lives in flow/flow.{hpp,cpp} ──

// ─── GmePipe (+ gme_make_pipe/create/destroy/buf_barrier/gme_record) lives in flow/flow.{hpp,cpp} ──

// ─── WapPipe (+ wap_create/wap_destroy, the warp-at-presenter pipeline) lives in warp_blend/warp_blend.{hpp,cpp} ──

// ─── MedianPipe (+ med_create/destroy) lives in flow/flow.{hpp,cpp} ──

// ─── Quit signalling — g_quit / g_gpu_a_util / g_ov_in / g_ov_out live in core/globals.{hpp,cpp} ──
// FPS-OVERLAY: push constants — MUST match overlay_fps.comp's PushConsts block byte-for-byte (16 bytes).
// FPS-OVERLAY: OverlayFpsPC + kOverlayW/kOverlayH live in present/present.hpp
// so run_present (present.cpp) AND main() both see them.
// FPS-OVERLAY: the overlay dispatch rect (px), top-left-anchored. Sized to cover ORG(10) + the widest
// box ("9999->9999"); the shader bounds-guards every pixel against Apresent's extent + the real glyph
// count, so a slightly-larger rect only writes where text/box fall. Multiples of 8 (workgroup).
// ─── g_device_lost / vk_live / console_ctrl_handler live in core/globals.{hpp,cpp} ──

// ─────────────────────────────────────────────────────────────────────────────
// main — all resources declared up-front; single cleanup block at the end.
// The "goto fail" pattern is used for early exits; it is safe because every
// resource variable is declared (null-initialised) BEFORE any goto.
// ─────────────────────────────────────────────────────────────────────────────
int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
#ifdef _MSC_VER
    // FG high-DPI: make the process Per-Monitor-Aware-V2 BEFORE any DPI-dependent / WinRT / window call
    // (init_apartment + all WGC/present init are below). On a SCALED display a DPI-UNAWARE process gets
    // DPI-VIRTUALIZED geometry: GetClientRect returns the logical (downscaled) size, so the WGC pool is
    // sized to the logical corner → WGC delivers only that corner (a crop/zoom). PMv2 makes GetClientRect,
    // the DXGI DesktopCoordinates, and the present rcMonitor all PHYSICAL, so capture AND present land at
    // the true resolution. Byte-identical on a 96-DPI/100% display (scale 1.0 → no virtualization under
    // either awareness). Best-effort: on failure log + continue (the 100% path is a no-op; a scaled path
    // then stays cropped, surfaced by --dpi-probe).
    if(!SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2))
        std::printf("[ra] SetProcessDpiAwarenessContext(PMv2) failed (err %lu) — high-DPI capture may crop.\n",GetLastError());
#endif
    // 1ms scheduler granularity — without it, every paced sleep_until
    // quantizes to the 15.6ms (or 1.4ms) system quantum and the quantization lands
    // directly in present slip. Process-wide; released at exit.
    timeBeginPeriod(1);
    struct TimePeriodGuard{~TimePeriodGuard(){timeEndPeriod(1);}} _tpg;
#ifdef _MSC_VER
    winrt::init_apartment();  // WinRT MTA init; required before any WinRT API calls
#endif
    // DISABLE console QuickEdit Mode on our OWN console. With QuickEdit on (the Windows default),
    // a stray CLICK + drag in the terminal enters mark/selection mode and SUSPENDS the process at
    // the next stdout write — the real-time FG freezes until the user presses a key. Clearing
    // ENABLE_QUICK_EDIT_MODE on STDIN removes that pause vector; ENABLE_EXTENDED_FLAGS must be set
    // for the quick-edit/insert-mode bits to take effect (per SetConsoleMode docs). Windows-only and
    // best-effort: a no-op if there is no console (handle invalid) or the call fails (e.g. redirected/
    // piped stdin) — never fatal, never changes pacing.
#ifdef _WIN32
    {
        HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
        DWORD inMode = 0;
        if (hIn != INVALID_HANDLE_VALUE && hIn != nullptr && GetConsoleMode(hIn, &inMode)) {
            const DWORD want = (inMode & ~(DWORD)ENABLE_QUICK_EDIT_MODE) | ENABLE_EXTENDED_FLAGS;
            if (want != inMode) SetConsoleMode(hIn, want);   // ignore failure (best-effort)
        }
    }
#endif
    // 4.3: a parse ERROR (unknown option / missing value) exits 2; --help and the other informational stops keep 0.
    Config cfg; if (!parse_args(argc, argv, cfg)) return cfg.parse_failed ? 2 : 0;
    // R0 (CONVERGENCE): the registry SHADOW must agree with the hand parser on every effective value —
    // a mismatch is a loud abort (exit 3), never a log line (risk XR2). Then the three CONTROL-plane
    // diagnostics act and exit before any device exists.
    if (!pfg::layers::layer_config_parity(cfg)) { std::printf("[layertab] PARITY FAIL: the layer registry disagrees with the hand parser -- refusing to run (exit 3)\n"); return 3; }
    if (cfg.dump_config_flag)  { pfg::layers::dump_config(cfg); return 0; }
    if (cfg.layer_dump)        { pfg::layers::layer_dump(cfg); return 0; }
    if (cfg.layer_model_json)  { pfg::layers::emit_layer_model_json(cfg); return 0; }
    // Ctrl+C / console-close exits CLEANLY (sets g_quit → the main loop signals the workers and
    // joins them) — the only quit path. Returns TRUE so the default terminator (a hard process
    // kill) never runs.
    SetConsoleCtrlHandler(console_ctrl_handler, TRUE);
    // ── --gpu-priority LEVER 1: process GPU scheduling priority (D3DKMT) ──────────────────────
    // D3DKMTSetProcessSchedulingPriorityClass — the OBS "GPU priority" mechanism: raises this
    // process's priority in the WDDM GPU scheduler so our submissions preempt/queue ahead of the
    // saturated game's. Called at startup BEFORE any device creation (this is the earliest point
    // after parse). Exported from gdi32.dll; prototype + enum verified FIRST-HAND from the WDK
    // header "C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\shared\d3dkmthk.h"
    // (enum at :4534-4542, prototype at :5954):
    //   typedef enum { IDLE=0, BELOW_NORMAL=1, NORMAL=2, ABOVE_NORMAL=3, HIGH=4, REALTIME=5 }
    //     D3DKMT_SCHEDULINGPRIORITYCLASS;
    //   NTSTATUS APIENTRY D3DKMTSetProcessSchedulingPriorityClass(HANDLE, D3DKMT_SCHEDULINGPRIORITYCLASS);
    // REALTIME typically requires elevation (expect STATUS_PRIVILEGE_NOT_HELD 0xC0000061 unelevated)
    // — print the NTSTATUS honestly and CONTINUE on failure (never abort; the other levers still run).
#ifdef _WIN32
    if (cfg.gpu_priority) {
        typedef LONG (APIENTRY *PFN_D3DKMTSetProcSchedPrioClass)(HANDLE, int);
        HMODULE hGdi = LoadLibraryA("gdi32.dll");
        PFN_D3DKMTSetProcSchedPrioClass pSet = hGdi
            ? (PFN_D3DKMTSetProcSchedPrioClass)GetProcAddress(hGdi, "D3DKMTSetProcessSchedulingPriorityClass")
            : nullptr;
        if (pSet) {
            const int  cls  = (cfg.gpu_priority == 2) ? 5 /*REALTIME*/ : 4 /*HIGH*/;
            const LONG st   = pSet(GetCurrentProcess(), cls);
            if (st == 0)
                std::printf("[ra] --gpu-priority: D3DKMTSetProcessSchedulingPriorityClass(%s) OK (NTSTATUS 0x00000000)\n",
                            cls == 5 ? "REALTIME=5" : "HIGH=4");
            else
                std::printf("[ra] --gpu-priority: D3DKMTSetProcessSchedulingPriorityClass(%s) FAILED — NTSTATUS 0x%08lX%s — continuing (lever 1 inactive)\n",
                            cls == 5 ? "REALTIME=5" : "HIGH=4", (unsigned long)st,
                            (unsigned long)st == 0xC0000061ul ? " (STATUS_PRIVILEGE_NOT_HELD: needs elevation)" : "");
        } else {
            std::printf("[ra] --gpu-priority: D3DKMTSetProcessSchedulingPriorityClass not exported by this gdi32.dll — lever 1 unavailable, continuing\n");
        }
        // hGdi intentionally NOT freed: gdi32 is a permanent dependency of this process anyway.
    }
#endif
    // The output clock is always the timer and PresentSurface is the only present path — the
    // support-matrix gates (warp-at-presenter needs a clock; surface needs timer; igpu-present is
    // unsupported under surface) are satisfied by construction. WGC (and so --window, which implies
    // it) is MSVC-only (WinRT). Without this guard the MinGW build would skip the #ifdef'd WGC init
    // SILENTLY and run with no capture at all.
#ifndef _MSC_VER
    if (cfg.capture_api==CA_WGC) {
        std::printf("[ra] ERROR: --capture-api wgc / --window need the MSVC build (WGC is C++/WinRT; "
                    "this is the MinGW DD-only build)\n");
        return 1;
    }
#endif
    if (cfg.dump_n>0) CreateDirectoryA("frames",nullptr);   // frames/ is gitignored
    // Whether to try primary FG on A (4090): true unless the user forced assist-only.
    // Finalised (pfg_enabled) after ofpA.init() succeeds; governed by --fg-gpu.
    // Non-const so single_gpu can force it off after device selection (below) — else ofpA + AframeA +
    // cmdA_fg + fA_fg double-allocate a 2nd full-res OFP on A (the pfg path is dead under single-GPU).
    // The single_gpu override is applied right after single_gpu is derived.
    bool want_pfg = (cfg.fg_gpu != FG_ASSIST);

    // ── Capture source (DDA/WGC route + dims + monitor refresh) (E1 → capture/capture_init.cpp) ──
    CaptureSrcInit o_cap{};
    { const int _rc=init_capture_source(cfg,o_cap); if(_rc>=0) return _rc; }
    // Rebind the section's exported names (const copies/aliases — the downstream code is verbatim).
    D3D& d=o_cap.d;
    auto& pres_outputs=o_cap.pres_outputs;
#ifdef _MSC_VER
    auto& wgc_ctx=o_cap.wgc_ctx;
    auto& wgc_target_hwnd=o_cap.wgc_target_hwnd;
#endif
    const bool IS_HDR=o_cap.IS_HDR;
    const VkFormat nat_vkfmt=o_cap.nat_vkfmt; auto& nat_bpp=o_cap.nat_bpp;
    const Route route=o_cap.route;
    const uint32_t NAT_W=o_cap.NAT_W, NAT_H=o_cap.NAT_H;
    const uint32_t WW=o_cap.WW, WH=o_cap.WH, UP_W=o_cap.UP_W, UP_H=o_cap.UP_H;
    auto& flow_div=o_cap.flow_div; const uint32_t WW_flow=o_cap.WW_flow, WH_flow=o_cap.WH_flow;
    auto& warp_div=o_cap.warp_div; const uint32_t WW_warp=o_cap.WW_warp, WH_warp=o_cap.WH_warp;
    const int cap_mon_hz=o_cap.cap_mon_hz;

    // ── Vulkan instance + physical-device selection (E1 → core/core_init.cpp) ──
    VkPickInit o_pick{};
    { const int _rc=init_vk_pick(cfg,d,o_pick); if(_rc>=0) return _rc; }
    VkInstance& inst=o_pick.inst;
    VkPhysicalDevice pA=o_pick.pA, pB=o_pick.pB, pG=o_pick.pG;
    auto& luidA=o_pick.luidA; auto& luidB=o_pick.luidB; auto& luidG=o_pick.luidG;
    auto& luidA_ok=o_pick.luidA_ok; auto& luidB_ok=o_pick.luidB_ok; auto& luidG_ok=o_pick.luidG_ok;
    auto& nvNameA=o_pick.nvNameA; auto& nvNameB=o_pick.nvNameB;
    const bool single_gpu=o_pick.single_gpu;
    const RECT& pc=pres_outputs[cfg.pres_mon].coords;
    // PresentSurface OWNS the panel (its own dcomp-ct HWND + WDA + message pump inside submit()) —
    // the only present path. The frame reaches the panel through the A-side bridge →
    // PresentSurface::submit(). The present-source extent fills the monitor exactly (the bridge blit,
    // FILTER_LINEAR, scales work→monitor). No HWND/VkSurface here.
    // ──────────────────────────────────────────────────────────────────────────
    // From here: ALL resource variables declared BEFORE any goto. The cleanup
    // block at `done:` releases everything that was initialised (checks != null).
    // ──────────────────────────────────────────────────────────────────────────
    // E1 (docs/planning/RESTRUCTURE_PLAN.md): the hoisted declaration block now lives as
    // per-module ownership structs (core/app_init.hpp) — zero-initialized, so the `done:`
    // cleanup still null-checks every member. The aliases below rebind every original name
    // to its struct member; the init sections, the main loop, the FgContext aggregate and
    // the teardown stay textually unchanged. All declarations (structs + aliases) precede
    // the first `goto done`, preserving the C++ goto rule this block has always encoded.
    DevicesInit    o_dev{};
    HostBridgeInit o_host{};
    GmeGpuInit     o_gme{};
    ImagesInit     o_img{};
    ConvertInit    o_cv{};
    FlowPipesInit  o_flow{};
    IgpuPipesInit  o_igpu{};
    CmdSyncInit    o_cs{};
    BridgeInit     o_br{};
    WapInit        o_wap{};

    // Devices.
    VDev& A=o_dev.A; VDev& B=o_dev.B; VDev& G=o_dev.G;
    // The flow/gme PRODUCER device. Multi-GPU → B (the 1080 Ti). single_gpu → A (the 4090 alone). FD
    // threads through EVERY device-B resource creation, the consume-side waits, and the teardown (the
    // destroys deref FD.dev — a null B.dev on an A-created handle would be UB). Declared BEFORE the
    // first `goto done` so no jump crosses its initializer (C++ goto rule).
    VDev& FD = single_gpu ? A : B;
    // Now that single_gpu is known, force the primary-FG (pfg) path off — its ofpA/AframeA/cmdA_fg/
    // fA_fg would otherwise double-allocate a 2nd full-res OFP on A. This is BEFORE the want_pfg-gated
    // allocations.
    if(single_gpu) want_pfg=false;
    auto& have_igpu=o_dev.have_igpu; auto& use_upscale=o_dev.use_upscale; auto& use_igpu_convert=o_dev.use_igpu_convert;
    auto& use_fwd_prestage=o_dev.use_fwd_prestage;
    auto& use_wap=o_dev.use_wap;
    auto& use_bidir=o_dev.use_bidir;
    auto& use_fill_div=o_dev.use_fill_div;
    auto& use_rescue=o_dev.use_rescue;
    auto& use_mv_median=o_dev.use_mv_median;
    auto& use_mv_guided=o_dev.use_mv_guided;
    auto& use_gme=o_dev.use_gme;
    auto& use_matte=o_dev.use_matte;
    auto& use_inertia=o_dev.use_inertia;
    auto& use_objects=o_dev.use_objects;
    auto& use_ambig=o_dev.use_ambig;
    auto& use_commit_default=o_dev.use_commit_default;
    auto& use_onepos=o_dev.use_onepos;
    auto& use_memory=o_dev.use_memory;
    auto& bframe_use=o_dev.bframe_use; auto& gsrc_use=o_dev.gsrc_use;

    // Host bridge (the full comment block lives with the fields in core/app_init.hpp).
    auto& cap_slots=o_host.cap_slots;
    auto& hostR=o_host.hostR; auto& hostI=o_host.hostI; auto& hostG=o_host.hostG;
    auto& hostA=o_host.hostA; auto& hostRP=o_host.hostRP;
    auto& hostFIELD=o_host.hostFIELD;
    auto& hR_a=o_host.hR_a; auto& hR_b=o_host.hR_b; auto& hR_g=o_host.hR_g;
    auto& hI_a=o_host.hI_a; auto& hI_b=o_host.hI_b; auto& hI_g=o_host.hI_g;
    auto& hGout=o_host.hGout; auto& hApres=o_host.hApres;
    auto& hRP_g=o_host.hRP_g; auto& hRP_b=o_host.hRP_b;
    auto& hFIELD_g=o_host.hFIELD_g;
    auto& hFIELD_a=o_host.hFIELD_a;
    auto& hRP_b_dev=o_host.hRP_b_dev;
    auto& Astage_g=o_host.Astage_g;
    auto& raw_host=o_host.raw_host;
    auto& raw_astage_a=o_host.raw_astage_a;
    auto& raw_astage_g=o_host.raw_astage_g;
    auto& raw_tcap=o_host.raw_tcap;
    auto& raw_lt_submit=o_host.raw_lt_submit;
    auto& raw_lt_compose=o_host.raw_lt_compose;
    auto& hostMV=o_host.hostMV; auto& hostSAD=o_host.hostSAD;
    auto& hMV_b=o_host.hMV_b; auto& hSAD_b=o_host.hSAD_b;
    auto& hostC2=o_host.hostC2;
    auto& hC2_b=o_host.hC2_b;
    auto& hostMVB=o_host.hostMVB;
    auto& hMVB_b=o_host.hMVB_b;
    auto& hostDIS=o_host.hostDIS;
    auto& hDIS_a=o_host.hDIS_a;
    auto& hostDISB=o_host.hostDISB;
    auto& hDISB_a=o_host.hDISB_a;
    auto& hostPER=o_host.hostPER;
    auto& hPER_a=o_host.hPER_a;
    auto& hMV_a=o_host.hMV_a; auto& hSAD_a=o_host.hSAD_a;
    auto& hMVB_a=o_host.hMVB_a;
    auto& hC2_a=o_host.hC2_a;

    // gme-gpu: the device-B side of the dissidence bridges + the model readback.
    auto& gmePipe=o_gme.gmePipe;
    auto& use_gme_gpu=o_gme.use_gme_gpu;
    auto& hDIS_b=o_gme.hDIS_b; auto& hDISB_b=o_gme.hDISB_b;
    auto& hostGmeM=o_gme.hostGmeM; auto& hostGmeMB=o_gme.hostGmeMB;
    auto& hGmeM_b=o_gme.hGmeM_b; auto& hGmeMB_b=o_gme.hGmeMB_b;

    // Per-interp B-VRAM staging + the overlapped copy-out cmd/fences.
    auto& sbI=o_img.sbI;
    auto& cmdB2=o_cs.cmdB2;
    auto& tfb=o_cs.tfb;
    auto& b_q2_split=o_img.b_q2_split;

    // Images.
    auto& dxgi_stage=o_img.dxgi_stage;
    auto& dxgi_stage2=o_img.dxgi_stage2;
    auto& Astage=o_host.Astage;
    auto& Anative=o_img.Anative; auto& Awork=o_img.Awork; auto& Bframe=o_img.Bframe;
    auto& Cinterp=o_img.Cinterp; auto& Gsrc=o_img.Gsrc; auto& Gdst=o_img.Gdst;
    auto& Bflow=o_img.Bflow;
    auto& pres_w=o_host.pres_w; auto& pres_h=o_host.pres_h;
    pres_w=WW; pres_h=WH;     // finalised once use_upscale is known (host bridge section)
    auto& Apresent=o_img.Apresent;

    // Convert pipeline (A).
    auto& cvSamp=o_cv.cvSamp; auto& cvDsl=o_cv.cvDsl;
    auto& cvLayout=o_cv.cvLayout; auto& cvPipe=o_cv.cvPipe;
    auto& cvPool=o_cv.cvPool; auto& cvSet=o_cv.cvSet;

    // Temporal MV smoothing (B-path only; app-local EMA on ofp's MV field).
    auto& mvsm=o_flow.mvsm; auto& mv_prev=o_flow.mv_prev; auto& use_mv_smooth=o_flow.use_mv_smooth;

    // OpticalFlowPipeline (B) + NVOFA + the primary-FG OFP (A).
    auto& ofp=o_flow.ofp;
    auto& nvofa=o_flow.nvofa;
    auto& use_nvofa=o_flow.use_nvofa;
    auto& ofpA=o_flow.ofpA;
    auto& AframeA=o_flow.AframeA; auto& CinterpA=o_flow.CinterpA;
    auto& pfg_enabled=o_flow.pfg_enabled;

    // Upscale pipeline (G) + iGPU-convert pipelines (one set per real slot).
    auto& upPipe=o_igpu.upPipe;
    auto& cpPipe=o_igpu.cpPipe;
    auto& fpipe=o_igpu.fpipe;
    auto& ubPipe=o_igpu.ubPipe;
    auto& ugPipe=o_igpu.ugPipe;

    // Command buffers + fences. No present semaphores (no swapchain present).
    auto& cmdA=o_cs.cmdA; auto& cmdB=o_cs.cmdB; auto& cmdG=o_cs.cmdG;
    auto& a_cpool_sg=o_cs.a_cpool_sg; auto& a_fpool_sg=o_cs.a_fpool_sg;
    auto& cmdGP=o_cs.cmdGP;
    auto& cmdA_fg=o_cs.cmdA_fg;
    auto& cmdB_bwd=o_cs.cmdB_bwd;
    auto& cmdB_fwd=o_cs.cmdB_fwd;
    auto& fB_fwd=o_cs.fB_fwd;
    auto& cmdF_pre=o_cs.cmdF_pre;
    auto& fF_pre=o_cs.fF_pre;
    auto& fA=o_cs.fA; auto& fB=o_cs.fB; auto& fG=o_cs.fG;
    auto& fGP=o_cs.fGP;
    auto& fA_fg=o_cs.fA_fg;
    auto& fB2=o_cs.fB2;

    // PresentSurface bridge (the producer side; full comments in core/app_init.hpp).
    namespace pp = phyriad::render::present;
    auto& bridge_w=o_br.bridge_w; auto& bridge_h=o_br.bridge_h;
    auto& bridge_tex=o_br.bridge_tex;
    auto& bridge_km_d3d=o_br.bridge_km_d3d;
    auto& bridge_nt=o_br.bridge_nt;
    auto& bridge_img=o_br.bridge_img;
    auto& bridge_mem=o_br.bridge_mem;
    auto& cmdBridge=o_br.cmdBridge;
    auto& fBridge=o_br.fBridge;
    auto& bridge_use_km=o_br.bridge_use_km;
    auto& bridge_tex1=o_br.bridge_tex1;
    auto& bridge_km_d3d1=o_br.bridge_km_d3d1;
    auto& bridge_nt1=o_br.bridge_nt1;
    auto& bridge_img1=o_br.bridge_img1;
    auto& bridge_mem1=o_br.bridge_mem1;
    auto& cmdBridge1=o_br.cmdBridge1;
    auto& fBridge1=o_br.fBridge1;
    auto& cmdBridgeA0=o_br.cmdBridgeA0;
    auto& fBridgeA0=o_br.fBridgeA0;

    // --upload-xfer machinery + WAP-on-A (full comments in core/app_init.hpp).
    auto& cmdUpload=o_wap.cmdUpload;
    auto& uslot_val=o_wap.uslot_val; auto& uslot=o_wap.uslot;
    auto& xfer_U=o_wap.xfer_U; auto& xfer_W=o_wap.xfer_W;
    auto& xfer_on=o_wap.xfer_on; auto& xfer_fams=o_wap.xfer_fams;
    auto& wapPipeA=o_wap.wapPipeA;
    auto& fgPipeA=o_wap.fgPipeA; auto& abPipeA=o_wap.abPipeA; auto& fgOutA=o_wap.fgOutA;   // R3
    auto& hostLP=o_wap.hostLP; auto& hLP_a=o_wap.hLP_a;
    auto& devAb=o_wap.devAb; auto& hostAb=o_wap.hostAb; auto& hAb_a=o_wap.hAb_a;
    auto& fillPipeA=o_wap.fillPipeA;
    auto& wapPrevA=o_wap.wapPrevA; auto& wapCurA=o_wap.wapCurA; auto& wapMVA=o_wap.wapMVA;
    auto& wapSADA=o_wap.wapSADA; auto& wapOutA=o_wap.wapOutA;
    auto& wapFIELDA=o_wap.wapFIELDA;
    auto& wapFIELDph=o_wap.wapFIELDph;
    auto& wapMVBA=o_wap.wapMVBA;
    auto& wapC2A=o_wap.wapC2A;
    auto& wapMVTA=o_wap.wapMVTA;
    auto& wapPrevOutA=o_wap.wapPrevOutA;
    auto& medPipe=o_wap.medPipe;
    auto& wapMVScratchA=o_wap.wapMVScratchA;
    auto& wapDISA=o_wap.wapDISA;
    auto& wapDISBA=o_wap.wapDISBA;
    auto& wapPERA=o_wap.wapPERA;
    auto& hostMassPtr=o_wap.hostMassPtr;
    auto& hMass_a=o_wap.hMass_a;
    auto& devMass=o_wap.devMass;
    auto& hostOutD=o_wap.hostOutD; auto& hOutD_a=o_wap.hOutD_a;
    auto& hostPrevD=o_wap.hostPrevD; auto& hPrevD_a=o_wap.hPrevD_a;
    auto& hostCurD=o_wap.hostCurD; auto& hCurD_a=o_wap.hCurD_a;
    auto& hostMV1=o_wap.hostMV1; auto& hMV1_a=o_wap.hMV1_a;   // --qdump+ post-consensus MV readback
    auto& hostMVB1=o_wap.hostMVB1; auto& hMVB1_a=o_wap.hMVB1_a;   // ... and the backward field

    // ── Devices + the derived feature gates (E1 → core/core_init.cpp) ──────
    if(!init_devices(cfg,pA,pB,pG,single_gpu,want_pfg,IS_HDR,NAT_W,NAT_H,WW,WH,route,o_dev)){ ra::compat::emit_fatal(ra::compat::ReasonCode::DEVICE_INIT_FAILED,"devices"); goto done; }

    // ── Host bridge (E1 → core/core_init.cpp) ─────────────────────
    if(!init_host_bridge(cfg,d,single_gpu,want_pfg,NAT_W,NAT_H,nat_bpp,WW,WH,UP_W,UP_H,WW_flow,WH_flow,cap_mon_hz,o_dev,FD,o_gme,o_img,o_host)){ ra::compat::emit_fatal(ra::compat::ReasonCode::DEVICE_INIT_FAILED,"host-bridge"); goto done; }

    // ── Images (E1 → core/core_init.cpp) ───────────────────────
    if(!init_images(cfg,d,NAT_W,NAT_H,nat_vkfmt,WW,WH,WW_flow,WH_flow,flow_div,UP_W,UP_H,want_pfg,o_dev,FD,o_host,o_flow,o_img)){ ra::compat::emit_fatal(ra::compat::ReasonCode::DEVICE_INIT_FAILED,"images"); goto done; }

    // ── WGC backend init (E1 → capture/capture_init.cpp) ────────────
#ifdef _MSC_VER
    if(!init_wgc_backend(cfg,d,NAT_W,NAT_H,cap_mon_hz,wgc_target_hwnd,o_img,wgc_ctx)){ ra::compat::g_fatal_reason=true; goto done; }
#endif

    // ── Convert pipeline (A: native→RGBA8 work) (E1 → capture/capture_init.cpp) ──
    if(!init_convert_pipe(o_dev,o_img,o_cv)){ ra::compat::emit_fatal(ra::compat::ReasonCode::DEVICE_INIT_FAILED,"convert-pipe"); goto done; }

    // ── OpticalFlowPipeline (B) + NVOFA + MV-smooth + primary-FG OFP (E1 → flow/flow_init.cpp) ──
    if(!init_flow_pipes(cfg,single_gpu,want_pfg,WW,WH,WW_flow,WH_flow,flow_div,o_dev,FD,o_img,o_flow)){ ra::compat::emit_fatal(ra::compat::ReasonCode::DEVICE_INIT_FAILED,"flow-pipes"); goto done; }

    // ── Upscale pipeline (G) (E1 → present/present_init.cpp) ───────────────────
    if(!init_upscale(cfg,o_dev,o_img,o_igpu)){ ra::compat::emit_fatal(ra::compat::ReasonCode::DEVICE_INIT_FAILED,"upscale"); goto done; }
    // ── iGPU convert+pack + B/G unpack pipelines (E1 → capture/capture_init.cpp) ──
    if(!init_igpu_pipes(cfg,NAT_W,NAT_H,nat_bpp,WW,WH,o_dev,o_host,o_img,o_flow,o_igpu)){ ra::compat::emit_fatal(ra::compat::ReasonCode::DEVICE_INIT_FAILED,"igpu-pipes"); goto done; }
    // ── warp-at-presenter pipeline (A, the bridge owner) (E1 → warp_blend/warp_blend_init.cpp) ──
    init_wap(cfg,WW,WH,WW_warp,WH_warp,o_dev,o_flow,o_wap);
    // ── gme-gpu pipeline + use_* re-finalization (E1 → flow/flow_init.cpp) ─────
    init_gme_finalize(cfg,o_dev,o_flow,o_gme);
    // R5 step 3b: the registry's FLOW rows resolved against the init cascades (the loud abort of R0's parity, one
    // level later: after every create-time fact is known). F reads cfg.layers.eff / avail from here on.
    if(!pfg::layers::layer_flow_resolve(cfg,use_wap,use_gme,use_gme_gpu,use_objects,use_memory,use_bidir,use_ambig,use_inertia,use_mv_smooth)){
        std::printf("[layertab] FLOW PARITY FAIL: the registry's resolved rows disagree with the init cascades -- refusing to run (exit 3)\n");
        return 3;
    }
    // R5 step 4: what the 3->5 transport actually carries, and which half of it the CPU authored. The channels the
    // upload moves today (wap_upload, present.cpp) minus the ones a host row writes = what a device-resident FlowSet
    // could share without the round trip. Printed, not promised: the data-path change is a separate project.
    if(use_wap) pfg::layers::layer_transport_report(cfg.layers,
        pfg::layers::CH_PREV | pfg::layers::CH_CUR | pfg::layers::CH_MV_RAW_FWD | pfg::layers::CH_SAD |
        pfg::layers::CH_MV_TARGET | pfg::layers::CH_MV_BWD | pfg::layers::CH_CANDIDATES |
        pfg::layers::CH_DISSIDENCE | pfg::layers::CH_PERSIST);

    // ── Command buffers + fences + semaphores (E1 → core/core_init.cpp) ────────
    init_cmd_sync(cfg,single_gpu,o_dev,FD,o_img,o_flow,o_cs,o_br);
    // There is NO VkSwapchain on any device — PresentSurface presents through DirectComposition; the
    // producer-side bridge below IS the present path.
    // ── Producer side: D3D11 shared bridge texture + VK-A import + --async-present slot-1 +
    //    the --rfp/--motion-fallback co-arm guards (E1 → present/present_init.cpp) ──────────
    if(!init_bridge_slots(cfg,d,pc,o_dev,o_br)){ ra::compat::emit_fatal(ra::compat::ReasonCode::DEVICE_INIT_FAILED,"bridge-slots"); goto done; }

    // ── Main loop — 3 worker threads + main/pump ───────────────────
    {
        std::printf("[ra] running — Ctrl+C to quit.\n");
        // fg-factor is SUPERSEDED under WAP (the default). The warp re-renders the EXACT phase per panel
        // tick (continuous t_use), so the present rate is the PANEL'S (--refresh-hz), not N·src. The flag
        // now only feeds (a) the grid-path interp buffer DEPTH (the non-WAP fallback) and (b) the auto-N
        // cap — both inert while WAP is on. The honest print reflects that.
        if(cfg.warp_at_presenter)
            std::printf("[ra] fg-factor: superseded — WAP interpolates every tick at panel rate (%d Hz); the flag now only caps the grid-path buffer depth (=%d) [3-thread]\n",
                cfg.refresh_hz, cfg.fg_factor);
        else if(cfg.fg_auto)
            std::printf("[ra] fg-factor=auto (N<=%d by measured capacity) [3-thread]\n", cfg.fg_factor);
        else
            std::printf("[ra] fg-factor=%d (%dx output — %d interp per real frame) [grid path] [3-thread]\n",
                cfg.fg_factor, cfg.fg_factor, std::max(0, cfg.fg_factor-1));

        static constexpr int NS=kGenRing;   // F→P interp generation ring — see kGenRing rationale
        // Per-set interp count is N_use−1 (F) / N_set−1 (P), each carried with the set — no single NI
        // constant (auto varies it; the cap is cfg.fg_factor).

        RealSlot c_slots[kCapSlots]{};
        // R6 step 2 — STAGE_CONTRACT §1's FrameRing: the 2 → 3 / 2 → 4 crossing gets its type. The ring OWNS the
        // publish counter and BINDS the slot storage; `c_seq` and `c_slots` stay as the names everything already
        // uses (the same storage, so no consumer changes). `ring.at(seq)` is the declared RealFrame view.
        std::atomic<uint64_t> _c_seq{0};
        pfg::ingest::FrameRing frames{ _c_seq, c_slots, cap_slots };
        std::atomic<uint64_t>& c_seq = frames.seq;
        // R5 step 2 — the F→P generation ring is DECLARED: pfg::flow::FlowRing (flow/flow_set.hpp) owns the per-pair
        // scalars that were twelve locals here and the two counters of the ring contract (f_seq, p_presenting), and
        // binds the per-generation host bridges by reference (their allocation stays with o_host / core_init). The
        // field notes (cseq/slot/tcap, span/n, gme, gme_bwd, mfwd/mbwd, disp, bwd_valid — and the publish discipline)
        // moved with the fields. The loop keeps the former names as ALIASES: the same memory, the same code below.
        pfg::flow::FlowRing ring(hostMV, hostSAD, hostMVB, hostC2, hostDIS, hostDISB, hostGmeM, hostGmeMB, hostPER, hostI);
        std::atomic<uint64_t>& f_seq = ring.f_seq;
        uint64_t (&f_pair_cseq_a)[NS] = ring.cseq; int (&f_pair_slot_a)[NS] = ring.slot; double (&f_pair_tcap_a)[NS] = ring.tcap;
        uint64_t (&f_pair_span_a)[NS] = ring.span; int (&f_pair_n_a)[NS] = ring.n;
        float (&f_pair_gme_a)[NS][6] = ring.gme; int (&f_pair_gme_valid_a)[NS] = ring.gme_valid;
        float (&f_pair_gme_bwd_a)[NS][6] = ring.gme_bwd;
        float (&f_pair_mfwd_a)[NS] = ring.mfwd; float (&f_pair_mbwd_a)[NS] = ring.mbwd;
        float (&f_pair_disp_a)[NS] = ring.disp;
        int (&f_pair_bwd_valid_a)[NS] = ring.bwd_valid;   // initialized 1 by the ring (the off-bidir build behaves as before)
        std::mutex c_mtx, f_mtx, g_q_mtx;
        // Under single_gpu BOTH the C-thread's convert (submit_wait_q2) AND the F-thread's flow (the
        // WAP/non-WAP submits below) target A.q2 — two threads on one VkQueue handle = external-sync
        // violation → device-lost. This mutex serializes every A.q2 submit so only one thread touches the
        // queue at a time (the g_q_mtx precedent for G's shared queue). It is taken ONLY on the single_gpu
        // path; the default multi-GPU path (B.q + A.q2-by-the-sole-convert) never locks it → byte-identical
        // OFF. Captured by reference by both threads (declared in their shared scope).
        std::mutex a_q2_mtx;
        std::condition_variable c_cv, f_cv;
        std::atomic<uint64_t> total_frames{0}, total_real{0};
        std::atomic<uint64_t> stat_cons{0};
        // F pair-span>1 events (a real source skip in the CONTENT — the tremble signature). With in-order
        // ingest this should sit at ~0 except genuine stalls.
        std::atomic<uint64_t> stat_skips{0};
        // F skipped the bwd record+fit this pair (pressure throttle). P prints bwd-skip:NN% as a % of the
        // pairs CONSUMED in the stat window (stat_cons is the denominator the WAP stats line already
        // advances). F-incremented, P-read; the % is computed from windowed deltas the same way dis%/fit
        // ride gme_* atomics.
        std::atomic<uint64_t> stat_bwd_skips{0};
        // The LAP ESCAPE counter — incremented when F breaks the ring-overwrite spin guard after the
        // kRingGuardSpinMax (~64ms) bound because P is FROZEN pinned on the generation F is about to
        // overwrite (p_presenting stuck at f_seq+1-kGenRing). An unbounded wait there would deadlock
        // (cons→0, lat linear); the bounded escape keeps F live and lets P's own read-side gates cover the
        // overwrite. F-incremented, P-read; P prints ` lap:N/s` from windowed deltas (only when nonzero →
        // calm runs never show it → line unchanged).
        std::atomic<uint64_t> stat_lap{0};
        // The current pressure TIER F is escalated to (0 = no escalation; 1 = bwd-skip only; 2 =
        // objects/memory every 2nd pair; 3 = objects/memory every 4th pair). F-published, P-read for the
        // ` tier:N` stats indicator (printed only when >0). Single scalar, the dis%/fit plumbing.
        std::atomic<uint64_t> stat_tier{0};
        // --fsub: F publishes the fwd-fence-wait EMA and the full-pair EMA (both × 1000 = µs, the
        // gme_fit_us plumbing). flow_us = the blocking submit_wait (the 1080 Ti pyramid GPU leg F stalls
        // on); pair_us = t_pair_ema (tp0→both fits). P prints fsub(flow/pair/cpu) only under --fsub; cpu =
        // pair − flow = the serial CPU tail.
        std::atomic<uint64_t> stat_flow_us{0};
        std::atomic<uint64_t> stat_pair_us{0};
        // F publishes the latest fit's dissidence % (× 100, integer) so P's WAP stats line prints dis:NN%,
        // and the fit-cost EMA (× 1000 = µs) so P appends fit:N.NNms when it exceeds 1ms. Plain atomics —
        // single scalars, no ordering needs beyond the value itself.
        std::atomic<uint64_t> gme_dis_x100{0};
        std::atomic<uint64_t> gme_fit_us{0};
        // F publishes the object-holon's two live stats — obj_live = the count of tracked (non-retired)
        // object slots after the latest pair's identity match, and obj_rep_x10 = the percent of IN-FILL
        // blocks the inheritance repair rewrote this pair (× 10 for one decimal, though the stats line
        // prints whole percent). Single scalars, no ordering needs beyond the value. P reads them into the
        // gme stats segment (obj:K rep:N%); the line omits the segment when obj_live==0 (calm/off → line
        // unchanged).
        std::atomic<uint64_t> obj_live{0};
        std::atomic<uint64_t> obj_rep_x10{0};
        // C's iGPU convert round-trip (lock+submit-fence, µs EMA) — C and P share G's ONE queue under
        // g_q_mtx; at high src+present rates they contend and C silently drops WGC frames (invisible to skip).
        std::atomic<uint64_t> c_conv_us{0};
        // --latency-trace: per-stage latency EMAs (µs), each computed within ONE clock (no cross-clock
        // subtraction). compose+copy are PRE-tcap (INVISIBLE to freshage); convert (=c_conv_us) + pickup
        // + fpub are the freshage decomposition. detect is DERIVED in the stats (freshage − fpub). Read by
        // the stats thread when cfg.latency_trace; written by C (compose/copy) and F (pickup/fpub). Lock-free.
        std::atomic<uint64_t> lt_compose_us{0}, lt_copy_us{0}, lt_pickup_us{0}, lt_fpub_us{0}, lt_preflow_us{0}, lt_spin_us{0}, lt_fwake_us{0};   // lt_fwake_us: publish→consume wake = F's now − the consumed slot's t_pub_ms (a SUB-component of pickup)
        // P publishes the f_seq value it is currently presenting so F can detect a ring-overwrite hazard:
        // F must not build into the generation P still holds. With kGenRing=3 this only fires during
        // span≥kGenRing stalls but guards the remaining edge.
        std::atomic<uint64_t>& p_presenting = ring.p_presenting;   // R5 step 2: the ring owns it; the name stays
        // WGC ring Map outcomes under primary saturation — fb = fell back to the older unread slot
        // (newest's copy not executed yet); miss = both slots unmappable.
        std::atomic<uint64_t> stat_mapfb{0}, stat_mapmiss{0};
        std::atomic<bool> g_quit_threads{false};
        // DD arrival cadence — C timestamps each delivered frame; P reads the arrival-delta EMA for
        // pacing (the processed-cadence ratchets under load) + arr/drop.
        std::atomic<uint64_t> dd_arr_delta_us{0}, dd_arrived{0}, dd_timeouts{0}, dd_lost{0}, dd_present{0};   // dd_timeouts: DDA WAIT_TIMEOUT/s; dd_lost: re-arm events; dd_present: suma AccumulatedFrames = tasa entregada real (superseded por dd_acq en el readout [ra-cap])
        // INGEST-ASYNC: dd_acq = ACQUIRES/s (serial + async) = the reliable `acq=` readout (dd_present reads 0 on NVIDIA).
        // raw_seq/raw_busy/raw_cv/raw_mtx = the acquire↔worker decoupling state (inert unless cfg.ingest_async).
        std::atomic<uint64_t> dd_acq{0}, dd_uniq{0};   // dd_uniq: CAPTURE-DEDUP frames únicos reales/s (el `uniq=` readout)
        // R6 step 2 — the RawRing (1 → 2, armed only under --ingest-async): the acquirer publishes with `raw_seq`,
        // the worker takes `raw_seq - 1` and drops the rest. Same discipline as the FrameRing above; `raw_seq` stays
        // the name the acquirer and the worker use.
        std::atomic<uint64_t> _raw_seq{0};
        pfg::ingest::RawRing raws{ _raw_seq, raw_host, raw_astage_a, raw_astage_g, raw_tcap, raw_lt_submit, raw_lt_compose };
        std::atomic<uint64_t>& raw_seq = raws.seq;
        std::atomic<int> raw_busy{-1};
        std::condition_variable raw_cv;
        std::mutex raw_mtx;
        // Adaptive-N cross-thread state. F owns the decision (measures its own stage times); P publishes
        // the inputs F needs (src interval + per-present cost) and reads the live N for stats. live_n starts
        // at the cap and degrades by measured capacity.
        std::atomic<int> live_n_atomic{cfg.fg_factor};
        std::atomic<uint64_t> src_interval_us{16670}, present_cost_us{0};

        // ── Compute the 3 RT-thread core IDs ONCE ──────────
        // Declared in this cross-thread-state block so the C/F/P thread-fn lambdas (defined below) capture
        // them by reference via their existing [&] captures. F=producer + P=consumer from the optimal SPSC
        // pair on the P/V-cache CCD; C = a p_cores() id distinct from F/P. UINT32_MAX = unavailable → that
        // pin is skipped (never pin UINT32_MAX: 1<<UINT32_MAX is UB on Win32). Cold path.
        const std::pair<uint32_t,uint32_t> ra_pc_pair =
            phyriad::hw::optimal_producer_consumer_pair(true);
        const uint32_t ra_core_f = ra_pc_pair.first;   // flow (producer)
        const uint32_t ra_core_p = ra_pc_pair.second;  // present (consumer, the RT thread)
        uint32_t ra_core_c = UINT32_MAX;               // capture — a p_core distinct from F/P
        {
            const std::vector<uint32_t> ra_pcores = phyriad::hw::p_cores();
            for (uint32_t _pid : ra_pcores)
                if (_pid != ra_core_f && _pid != ra_core_p) { ra_core_c = _pid; break; }
            if (ra_core_c == UINT32_MAX && !ra_pcores.empty()) ra_core_c = ra_pcores.front();
        }
        if (cfg.pin_threads) {
            // --pin-test makes the per-lever summary honest per mode (read only inside the --pin guard).
            static const char* kS0Mode[6] = {
                "FULL(pin C/F/P + elevate P+F)", "NO-FLOW-RT(pin C/F/P + elevate P)",
                "PRIO-ONLY(elevate P+F, no pin)", "AFFINITY-ONLY(pin C/F/P, no elevate)",
                "NEITHER(no pin, no elevate)",
                "MMCSS-COMPOSITE(P hard-pin+ProAudio/CRIT; C/F soft-aff+Capture HIGH/NORMAL; elevate fallback)" };
            const int s0m = (cfg.pin_test>=0 && cfg.pin_test<=5) ? cfg.pin_test : 0;
            std::printf("[ra] thread pin (--pin-test %d=%s): C=%u F=%u P=%u\n",
                        s0m, kS0Mode[s0m], ra_core_c, ra_core_f, ra_core_p);
        }
        // ── --fg-protect: the GAME_FLOOR core-reservation = the NO-GAME-CAP dogma AS CODE ──
        // We NEVER call set_process_affinity/set_process_priority on the GAME; we only BOUND how many of OUR
        // own threads HARD-pin a core, so the game always keeps >= half the P-cores. The hard-pin count = 1
        // (only P hard-pins in mode-5; C/F are SOFT ideal-processor hints). GAME_FLOOR = max(4, p_cores/2).
        // If the hard-pin count would exceed (p_cores - GAME_FLOOR), demote P to a SOFT ideal-processor hint
        // too (the game can reclaim the core). 16-P-core rig: GAME_FLOOR=8, p_cores-GAME_FLOOR=8 >= 1 -> P
        // stays hard-pinned (INERT here). <= 4-P-core CPU: GAME_FLOOR=4, p_cores-GAME_FLOOR<=0 < 1 -> P
        // demotes to soft. Computed once on the cold path; the P thread lambda (captured by &) consumes
        // ra_fgprotect_demote_p. Gated on cfg.fg_protect so the bare `--pin --pin-test 5` ablation lever
        // stays byte-identical (it does NOT get the GAME_FLOOR clamp).
        bool ra_fgprotect_demote_p = false;
        if (cfg.fg_protect) {
            const int ra_pcount    = (int)phyriad::hw::p_cores().size();
            const int ra_gamefloor = (ra_pcount/2 > 4) ? ra_pcount/2 : 4;   // max(4, p_cores/2)
            const int ra_hardpins  = 1;                                     // only P hard-pins today (C/F are soft)
            ra_fgprotect_demote_p  = (ra_hardpins > (ra_pcount - ra_gamefloor));
            std::printf("[ra] --fg-protect GAME_FLOOR: p_cores=%d, GAME_FLOOR=max(4,p/2)=%d, hard_pins=%d (P only) -> %s (game keeps >= %d P-cores; the game's own affinity/priority is NEVER touched).\n",
                        ra_pcount, ra_gamefloor, ra_hardpins,
                        ra_fgprotect_demote_p ? "P DEMOTED to soft ideal-processor (small-CPU reservation)" : "P stays HARD-pinned",
                        ra_gamefloor);
        }

        // The locked G-queue submit helper (submit_wait_G_P) lives in run_present (present.cpp) with the P
        // body — it is used only by the present thread.

        // -- Thread C -----------------------------------------------------------------------
        // The CAPTURE thread body lives in capture/capture.cpp as run_capture(FgContext&). It is launched
        // below via std::thread(run_capture, std::ref(ctx)).

        // ── Thread F ──────────────────────────────────────────────────────────
        const uint32_t mvw_f=ofp.motion_width(), mvh_f=ofp.motion_height();   // MV grid
        // The FLOW thread body lives in flow/flow.cpp as run_flow(FgContext&). It is launched below via
        // std::thread(run_flow, std::ref(ctx)).

        // ── Thread P ──────────────────────────────────────────────────────────
        // The PRESENT thread body lives in present/present.cpp as run_present(FgContext&). It is launched
        // below via std::thread(run_present, std::ref(ctx)).

        // ── Launch threads + main pump ──────────────────────────────────────
        // Bind the worker threads' shared main()-locals into FgContext (references + array pointers). The
        // run_* bodies alias them back.
        FgContext ctx{
            .cfg = cfg,
            .ra_core_c = ra_core_c,
            .c_seq = c_seq,
            .cap_slots = cap_slots,
            .g_quit_threads = g_quit_threads,
#ifdef _MSC_VER
            .wgc_ctx = wgc_ctx,
#endif
            .d = d,
            .stat_mapfb = stat_mapfb,
            .stat_mapmiss = stat_mapmiss,
            .NAT_W = NAT_W,
            .NAT_H = NAT_H,
            .nat_bpp = nat_bpp,
            .Astage = Astage,
            .lt_copy_us = lt_copy_us,
            .lt_compose_us = lt_compose_us,
            .dd_timeouts = dd_timeouts,
            .dxgi_stage = dxgi_stage,
            .dd_arr_delta_us = dd_arr_delta_us,
            .dd_arrived = dd_arrived,
            .dd_lost = dd_lost,
            .dd_present = dd_present,
            .c_slots = c_slots,
            .total_real = total_real,
            .use_igpu_convert = use_igpu_convert,
            .cmdA = cmdA,
            .Anative = Anative,
            .Awork = Awork,
            .cvPipe = cvPipe,
            .cvLayout = cvLayout,
            .cvSet = cvSet,
            .IS_HDR = IS_HDR,
            .WW = WW,
            .WH = WH,
            .hR_a = hR_a,
            .A = A,
            .single_gpu = single_gpu,
            .a_q2_mtx = a_q2_mtx,
            .fA = fA,
            .c_conv_us = c_conv_us,
            .cmdG = cmdG,
            .cpPipe = cpPipe,
            .fpipe = fpipe,
            .G = G,
            .fG = fG,
            .g_q_mtx = g_q_mtx,
            .hostFIELD = hostFIELD,
            .hostR = hostR,
            .c_cv = c_cv,
            // INGEST-ASYNC: the acquire↔convert-worker decoupling state (inert unless cfg.ingest_async).
            .frames = frames,
            .raws = raws,                 // R7: the RawRing owns the publish rule and the drop-to-newest read
            .raw_seq = raw_seq,
            .dd_acq = dd_acq,
            .dd_uniq = dd_uniq,
            .raw_busy = raw_busy,
            .raw_cv = raw_cv,
            .raw_mtx = raw_mtx,
            .raw_astage_a = raw_astage_a,
            .raw_astage_g = raw_astage_g,
            .raw_tcap = raw_tcap,
            .raw_lt_submit = raw_lt_submit,
            .raw_lt_compose = raw_lt_compose,
            .dxgi_stage2 = dxgi_stage2,
            // The FLOW thread's shared main()-locals (bound in struct-declaration order).
            .B = B,
            .FD = FD,
            .pfg_enabled = pfg_enabled,
            .mvw_f = mvw_f,
            .mvh_f = mvh_f,
            .WW_flow = WW_flow,
            .WH_flow = WH_flow,
            .flow_div = flow_div,
            .ra_core_f = ra_core_f,
            .use_inertia = use_inertia,
            .use_objects = use_objects,
            .use_memory = use_memory,
            .use_bidir = use_bidir,
            .use_gme = use_gme,
            .use_gme_gpu = use_gme_gpu,
            .use_wap = use_wap,
            .use_nvofa = use_nvofa,
            .use_ambig = use_ambig,
            .use_mv_smooth = use_mv_smooth,
            .use_fwd_prestage = use_fwd_prestage,
            .b_q2_split = b_q2_split,
            .ofp = ofp,
            .ofpA = ofpA,
            .Cinterp = Cinterp,
            .CinterpA = CinterpA,
            .nvofa = nvofa,
            .gmePipe = gmePipe,
            .mvsm = mvsm,
            .Bframe = Bframe,
            .Bflow = Bflow,
            .AframeA = AframeA,
            .cmdB = cmdB,
            .cmdB_bwd = cmdB_bwd,
            .cmdB_fwd = cmdB_fwd,
            .cmdF_pre = cmdF_pre,
            .cmdB2 = cmdB2,
            .cmdA_fg = cmdA_fg,
            .fB = fB,
            .fB2 = fB2,
            .fB_fwd = fB_fwd,
            .fF_pre = fF_pre,
            .fA_fg = fA_fg,
            .tfb = tfb,
            .hostMV = hostMV,
            .hostSAD = hostSAD,
            .hostPER = hostPER,
            .hostDIS = hostDIS,
            .hostDISB = hostDISB,
            .hostMVB = hostMVB,
            .hostGmeM = hostGmeM,
            .hostGmeMB = hostGmeMB,
            .hostC2 = hostC2,
            .hostI = hostI,
            .hR_b = hR_b,
            .hRP_b = hRP_b,
            .hRP_b_dev = hRP_b_dev,
            .hMV_b = hMV_b,
            .hSAD_b = hSAD_b,
            .hMVB_b = hMVB_b,
            .hDIS_b = hDIS_b,
            .hDISB_b = hDISB_b,
            .hGmeM_b = hGmeM_b,
            .hGmeMB_b = hGmeMB_b,
            .hC2_b = hC2_b,
            .sbI = sbI,
            .hI_b = hI_b,
            .hI_a = hI_a,
            .ubPipe = ubPipe,
            .f_pair_cseq_a = f_pair_cseq_a,
            .f_pair_slot_a = f_pair_slot_a,
            .f_pair_tcap_a = f_pair_tcap_a,
            .f_pair_span_a = f_pair_span_a,
            .f_pair_n_a = f_pair_n_a,
            .f_pair_gme_a = f_pair_gme_a,
            .f_pair_gme_valid_a = f_pair_gme_valid_a,
            .f_pair_gme_bwd_a = f_pair_gme_bwd_a,
            .f_pair_mfwd_a = f_pair_mfwd_a,
            .f_pair_mbwd_a = f_pair_mbwd_a,
            .f_pair_disp_a = f_pair_disp_a,
            .f_pair_bwd_valid_a = f_pair_bwd_valid_a,
            .f_seq = f_seq,
            .f_cv = f_cv,
            .c_mtx = c_mtx,
            .p_presenting = p_presenting,
            .src_interval_us = src_interval_us,
            .present_cost_us = present_cost_us,
            .stat_tier = stat_tier,
            .stat_bwd_skips = stat_bwd_skips,
            .stat_skips = stat_skips,
            .stat_lap = stat_lap,
            .stat_cons = stat_cons,
            .stat_flow_us = stat_flow_us,
            .stat_pair_us = stat_pair_us,
            .gme_fit_us = gme_fit_us,
            .gme_dis_x100 = gme_dis_x100,
            .obj_live = obj_live,
            .obj_rep_x10 = obj_rep_x10,
            .live_n_atomic = live_n_atomic,
            .lt_pickup_us = lt_pickup_us,
            .lt_preflow_us = lt_preflow_us,
            .lt_spin_us = lt_spin_us,
            .lt_fpub_us = lt_fpub_us,
            .lt_fwake_us = lt_fwake_us,
            .Apresent = Apresent,
            .Gdst = Gdst,
            .Gsrc = Gsrc,
            .UP_H = UP_H,
            .UP_W = UP_W,
            .WH_warp = WH_warp,
            .WW_warp = WW_warp,
            .bridge_h = bridge_h,
            .bridge_img1 = bridge_img1,
            .bridge_km_d3d = bridge_km_d3d,
            .bridge_km_d3d1 = bridge_km_d3d1,
            .bridge_mem1 = bridge_mem1,
            .bridge_nt = bridge_nt,
            .bridge_nt1 = bridge_nt1,
            .bridge_tex = bridge_tex,
            .bridge_tex1 = bridge_tex1,
            .bridge_use_km = bridge_use_km,
            .bridge_w = bridge_w,
            .cmdBridge1 = cmdBridge1,
            .cmdBridgeA0 = cmdBridgeA0,
            .cmdGP = cmdGP,
            .cmdUpload = cmdUpload,
            .devMass = devMass,
            .fBridge1 = fBridge1,
            .fBridgeA0 = fBridgeA0,
            .fGP = fGP,
            .fillPipeA = fillPipeA,
            .hC2_a = hC2_a,
            .hCurD_a = hCurD_a,
            .hDISB_a = hDISB_a,
            .hDIS_a = hDIS_a,
            .hFIELD_a = hFIELD_a,
            .hGout = hGout,
            .hMV1_a = hMV1_a,
            .hMVB1_a = hMVB1_a,
            .hMVB_a = hMVB_a,
            .hMV_a = hMV_a,
            .hMass_a = hMass_a,
            .hOutD_a = hOutD_a,
            .hPER_a = hPER_a,
            .hPrevD_a = hPrevD_a,
            .hR_g = hR_g,
            .hSAD_a = hSAD_a,
            .hostCurD = hostCurD,
            .hostMV1 = hostMV1,
            .hostMVB1 = hostMVB1,
            .hostMassPtr = hostMassPtr,
            .hostOutD = hostOutD,
            .hostPrevD = hostPrevD,
            .luidA = luidA,
            .luidA_ok = luidA_ok,
            .luidB = luidB,
            .luidB_ok = luidB_ok,
            .luidG = luidG,
            .luidG_ok = luidG_ok,
            .medPipe = medPipe,
            .nvNameA = nvNameA,
            .nvNameB = nvNameB,
            .pres_h = pres_h,
            .pres_w = pres_w,
            .ra_core_p = ra_core_p,
            .total_frames = total_frames,
            .ugPipe = ugPipe,
            .upPipe = upPipe,
            .use_commit_default = use_commit_default,
            .use_fill_div = use_fill_div,
            .use_matte = use_matte,
            .use_mv_guided = use_mv_guided,
            .use_mv_median = use_mv_median,
            .use_onepos = use_onepos,
            .use_rescue = use_rescue,
            .use_upscale = use_upscale,
            .uslot = uslot,
            .uslot_val = uslot_val,
            .wapC2A = wapC2A,
            .wapCurA = wapCurA,
            .wapDISA = wapDISA,
            .wapDISBA = wapDISBA,
            .wapFIELDA = wapFIELDA,
            .wapMVA = wapMVA,
            .wapMVBA = wapMVBA,
            .wapMVScratchA = wapMVScratchA,
            .wapMVTA = wapMVTA,
            .wapOutA = wapOutA,
            .wapPERA = wapPERA,
            .wapPipeA = wapPipeA,
            .fgPipeA = fgPipeA,
            .abPipeA = abPipeA,
            .fgOutA = fgOutA,
            .hLP_a = hLP_a,
            .hostLP = hostLP,
            .devAb = devAb,
            .hAb_a = hAb_a,
            .hostAb = hostAb,
            .wapPrevA = wapPrevA,
            .wapPrevOutA = wapPrevOutA,
            .wapSADA = wapSADA,
            .warp_div = warp_div,
            .wgc_target_hwnd = wgc_target_hwnd,
            .xfer_U = xfer_U,
            .xfer_W = xfer_W,
            .xfer_on = xfer_on,
            .bridge_img = bridge_img,
            .bridge_mem = bridge_mem,
            .cmdBridge = cmdBridge,
            .fBridge = fBridge,
            .ra_fgprotect_demote_p = ra_fgprotect_demote_p,
        };
        std::thread thr_c(run_capture, std::ref(ctx));
        std::thread thr_f(run_flow, std::ref(ctx));
        std::thread thr_p(run_present, std::ref(ctx));
        // INGEST-ASYNC: the convert WORKER thread — spawned ONLY when --ingest-async armed (it owns the
        // convert state; thr_c then acquires-only). Joined below BEFORE any convert/Vulkan teardown.
        std::thread thr_cw;
        if(cfg.ingest_async) thr_cw=std::thread(run_convert_worker, std::ref(ctx));

        // PresentSurface owns its own HWND + message pump (drained inside submit() on the P thread). The
        // main thread just waits for the quit flag (set by the console Ctrl handler or by P on a
        // surface-create failure), then signals + joins the workers cleanly.
        while(!g_quit){ std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
        g_quit_threads.store(true);
        c_cv.notify_all(); f_cv.notify_all();
        raw_cv.notify_all();   // INGEST-ASYNC: wake the convert worker so it observes the quit latch and drains

        // R4c (2026-09-05): the joins get a DEADLINE once the device is LOST. The operator's --tdr-test runs showed
        // the P thread stuck inside a driver/DXGI call it entered in the tick that dispatched the hang (F: "P pinned
        // on gen" from that tick on): no code of ours runs on that thread again, its join never returns, and the
        // own-window plane it owns stays on the panel with the last frame — the pillar's OA-10 watchdog cannot hide
        // a window whose owning thread is wedged (ShowWindow/SetWindowPos from another thread wait on that thread's
        // message loop). The only give-back is the process ending: 3 s past the quit with the loss latched and a
        // worker still alive → name the survivors, flush, TerminateProcess (the OS reclaims the window; the device
        // is lost anyway; no CSV finalize on this path — said). One joiner per worker so the survivors can be named.
        // On a normal quit (no loss) the joins are the unbounded ones they were — byte-identical.
        std::atomic<bool> c_done{false}, cw_done{true}, f_done{false}, p_done{false};
        std::thread j_c([&]{ thr_c.join(); c_done.store(true); });
        std::thread j_cw; if(thr_cw.joinable()){ cw_done.store(false); j_cw=std::thread([&]{ thr_cw.join(); cw_done.store(true); }); }
        std::thread j_f([&]{ thr_f.join(); f_done.store(true); });
        std::thread j_p([&]{ thr_p.join(); p_done.store(true); });
        const auto t_join0=std::chrono::steady_clock::now();
        while(!(c_done.load()&&cw_done.load()&&f_done.load()&&p_done.load())){
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            if(g_device_lost.load() && (std::chrono::steady_clock::now()-t_join0)>std::chrono::seconds(3)){
                std::printf("[ra] device lost: worker(s) still alive 3 s after the quit --%s%s%s%s -- a driver/DXGI call never returned; terminating the process so the panel is released (no CSV finalize on this path)\n",
                    c_done.load()?"":" C(capture)", cw_done.load()?"":" CW(convert)", f_done.load()?"":" F(flow)", p_done.load()?"":" P(present)");
                std::fflush(stdout);
                TerminateProcess(GetCurrentProcess(), 3);
            }
        }
        j_c.join(); if(j_cw.joinable()) j_cw.join(); j_f.join(); j_p.join();   // INGEST-ASYNC: CW joined BEFORE the convert/Vulkan teardown (it touches cmdA/cmdG/fA/fG + the raw imports)

        const uint64_t total_interp=(total_frames.load()>total_real.load())?(total_frames.load()-total_real.load()):0;
        std::printf("[ra] done (real=%llu interp=%llu total_presents=%llu)\n",
            (unsigned long long)total_real.load(),(unsigned long long)total_interp,(unsigned long long)total_frames.load());
        // The host-side measurement instrument. descriptor_update_calls() is the LIFETIME count of
        // vkUpdateDescriptorSets issued by record_optical_flow. Read it as the OFF-vs-ON DELTA: run the SAME
        // workload once without --fg-prebake (the per-pair burst counts ~41/record) and once WITH it
        // (steady-state 0/record after the 2 cold prebake collections). The win is this host-CPU burst
        // eliminated — NOT a GPU-ms change (the GPU slice is identical).
        std::printf("[ra] fg-prebake instrument: vkUpdateDescriptorSets lifetime calls = %llu  (prebake %s).%s\n",
            (unsigned long long)ofp.descriptor_update_calls(),
            ofp.fg_prebake_active()?"ARMED":"OFF",
            ofp.fg_prebake_active()?"  Steady-state 0/record; the count is the ~41-call cold prebake per parity (~82 total) + any non-prebaked fallbacks."
                                   :"  Per-record burst counted — re-run with --fg-prebake to read the eliminated delta.");
        // ── Bounded-run clean-exit evidence line — GATED, so default runs are byte-identical ───────
        // Printed ONLY on a bounded run (cfg.run_max_ms || cfg.run_max_frames). The workers have already
        // cleanly joined (above) and the CSV drain was flushed + the -stats.csv written at tcsv scope-exit
        // inside run_present, so this is pure post-teardown evidence (it changes no teardown). The FG tracks
        // NO numeric live descriptor count (cvPool is a single VkDescriptorPool handle), so we print the
        // present count + the clean-exit marker rather than invent a counter; cvPool's alloc state is the one
        // honest descriptor signal. total_frames is in scope here (the thread-owning block); it is NOT visible
        // at the `done:` label, which is why this sits here, still BEFORE the device destroy in done:.
        if(cfg.run_max_ms||cfg.run_max_frames){
            std::printf("[ra] bounded-run clean exit: total_presents=%llu  (--duration %.4gs / --max-frames %llu)  cvDescriptorPool=%s\n",
                (unsigned long long)total_frames.load(), cfg.run_max_ms/1000.0,
                (unsigned long long)cfg.run_max_frames, cvPool?"alloc":"none");
        }
    }

done:
    // 2026-09-06: a latched VK_ERROR_DEVICE_LOST is NOT a clean quit. globals.cpp:14-17 prints its own
    // line and sets g_quit, and the unwind then fell through to `return 0` — so the launcher showed
    // "process finished (code 0)" for a run that died. Name it and latch it. (This is the second half of
    // the --fg-gpu primary defect: with convert wrongly on CG_IGPU under single-GPU, the C-thread
    // submitted on A.q against the present thread and lost the device inside the first frames.)
    if(g_device_lost.load()) ra::compat::emit_fatal(ra::compat::ReasonCode::DEVICE_LOST);
    // Teardown in reverse-init order (all checks guard against null).
    // On a LOST device, vkDeviceWaitIdle is unreliable (Khronos: it may itself return VK_ERROR_DEVICE_LOST)
    // -> skip the idle-wait when the loss is already known and go straight to the null-guarded destroys
    // (destroying on a lost device is defined/safe). On a normal quit (g_device_lost false) this waits idle.
    if(A.dev && !g_device_lost) vkDeviceWaitIdle(A.dev);
    if(B.dev && !g_device_lost) vkDeviceWaitIdle(B.dev);
    if((use_upscale||use_igpu_convert)&&G.dev && !g_device_lost) vkDeviceWaitIdle(G.dev);

    // Stop WGC BEFORE releasing D3D11 resources the callback uses (dxgi_stage, d.ctx).
#ifdef _MSC_VER
    if(wgc_ctx){
        wgc_ctx->running.store(false);
        wgc_ctx->session.Close();
        Sleep(100); // drain in-flight FrameArrived callbacks
        // --copy-fence: release the fence/ctx4/event AFTER running=false + the Sleep(100) callback drain —
        // no in-flight FrameArrived can touch a freed fence/ctx4 here. Null-guarded (off → all null).
        if(wgc_ctx->copyEvt){ CloseHandle(wgc_ctx->copyEvt); wgc_ctx->copyEvt=nullptr; }
        if(wgc_ctx->copyFence){ wgc_ctx->copyFence->Release(); wgc_ctx->copyFence=nullptr; }
        if(wgc_ctx->ctx4){ wgc_ctx->ctx4->Release(); wgc_ctx->ctx4=nullptr; }
        wgc_ctx->pool.Close();
        for(uint32_t i=1;i<WgcCtx::RING_N;++i) rel(wgc_ctx->ring[i]); // ring[0]==dxgi_stage released below (off path)
        // --copy-device: when armed, ring[0] is a FRESH 2nd-device staging texture (NOT dxgi_stage), so
        // release it here; then release the 2nd device + its context (AFTER the pool.Close() that dropped
        // d3d_winrt's ref + AFTER the fence/ctx4 release that held device-5 refs — no in-flight FrameArrived
        // after running=false+Sleep(100)). All null when off → this whole block is dead (byte-identical).
        if(wgc_ctx->cdev){
            rel(wgc_ctx->ring[0]);
            if(wgc_ctx->cctx){ wgc_ctx->cctx->Release(); wgc_ctx->cctx=nullptr; }
            wgc_ctx->cdev->Release(); wgc_ctx->cdev=nullptr;
        }
        delete wgc_ctx; wgc_ctx=nullptr;
    }
#endif

    // No swapchains, no present semaphores.
    if(fA) vkDestroyFence(A.dev,fA,nullptr);
    if(fA_fg) vkDestroyFence(A.dev,fA_fg,nullptr);
    if(fB) vkDestroyFence(FD.dev,fB,nullptr); if(fG&&(use_upscale||use_igpu_convert)) vkDestroyFence(G.dev,fG,nullptr);   // FD.dev — fB was created on FD (A under single_gpu); B.dev null would pass a null device with a valid A-handle (UB)
    if(fB2) vkDestroyFence(FD.dev,fB2,nullptr);   // bwd-submit fence (FD.dev)
    for(int _k=0;_k<2;++_k) if(fB_fwd[_k]) vkDestroyFence(FD.dev,fB_fwd[_k],nullptr);   // fwd-pipeline fences (vkDeviceWaitIdle(A) drained any in-flight fwd submit; FD.dev)
    for(int _k=0;_k<2;++_k) if(fF_pre[_k]) vkDestroyFence(FD.dev,fF_pre[_k],nullptr);   // fwd-prestage fences (vkDeviceWaitIdle(A) at shutdown drained any in-flight prestage copy; FD.dev; cmdF_pre freed with a_fpool_sg/FD.pool)
    if(fGP&&use_upscale) vkDestroyFence(G.dev,fGP,nullptr);  // upscale-only
    if(b_q2_split) for(int _k=0;_k<kMaxInterp;++_k) if(tfb[_k]) vkDestroyFence(FD.dev,tfb[_k],nullptr);  // overlapped interp copy-out fences (FD.dev; b_q2_split false under single_gpu — handles null anyway)
    rel(dxgi_stage);
    rel(dxgi_stage2);   // INGEST-ASYNC: the 2nd staging texture (null-safe rel when async off)

    if(use_upscale) up_destroy(G,upPipe);
    // Warp-at-presenter pipeline + presenter-local images on A (the bridge owner).
    if(use_wap){ wap_destroy(A,wapPipeA);
        // R3: the --fg-core-ab totals (read from the host copy AFTER the last submit completed), then the objects.
        if(cfg.fg_core_ab && hostAb){ const uint32_t* s=(const uint32_t*)hostAb;
            std::printf("[fg-core-ab] TOTAL compared=%u diff_px=%u max_delta=%u sum_delta=%u  (%s)\n",s[0],s[1],s[2],s[3],
                        s[1]==0u?"BYTE-IDENTICAL on every compared tick":"NOT identical"); }
        fgcore_destroy(A,fgPipeA); abdiff_destroy(A,abPipeA); img_destroy(A,fgOutA);
        hbuf_destroy(A,hLP_a);  if(hostLP) _aligned_free(hostLP);
        hbuf_destroy(A,devAb);  hbuf_destroy(A,hAb_a); if(hostAb) _aligned_free(hostAb);
        fillpipe_destroy(A,fillPipeA);   // the field VISUALIZER pipeline (created only with --afill; null-safe)
        img_destroy(A,wapFIELDA);        // A-side iGPU contour field image (created with --afill OR --bg-snap; null-safe)
        img_destroy(A,wapFIELDph);       // the 1×1 r32ui binding-11 placeholder (created when neither owns wapFIELDA; null-safe)
        img_destroy(A,wapPrevA); img_destroy(A,wapCurA); img_destroy(A,wapMVA); img_destroy(A,wapSADA); img_destroy(A,wapOutA);
        img_destroy(A,wapMVBA);    // backward-MV sampled image (created whenever use_wap)
        img_destroy(A,wapC2A);     // second-best candidate image (created only with use_ambig; null-safe)
        img_destroy(A,wapMVTA);    // next-pair MV-target image (--vblend; created only with cfg.vblend; null-safe)
        img_destroy(A,wapPrevOutA); // prev-output history image (--ts-smooth; created only with cfg.ts_smooth>0; null-safe)
        img_destroy(A,wapDISA);    // dissidence-mask image (created only with --gme; null-safe if not)
        img_destroy(A,wapDISBA);   // backward dissidence-mask image (created only with --gme+--bidir; null-safe)
        img_destroy(A,wapPERA); }  // inertia persistence image (created unconditionally with WAP; null-safe)
    // The mass counter SSBO import + its CPU backing. Destroyed UNCONDITIONALLY (the alloc/import live
    // inside the WAP setup block, which may flip use_wap false on a counter failure — so the pointer can be
    // non-null even when use_wap ended false). hbuf_destroy + _aligned_free are null-safe.
    hbuf_destroy(A,hMass_a); if(hostMassPtr) _aligned_free(hostMassPtr);
    hbuf_destroy(A,devMass);   // the device-local mass SSBO (VRAM; no host backing)
    hbuf_destroy(A,hOutD_a); if(hostOutD) _aligned_free(hostOutD);   // --outdump readback (null-safe)
    hbuf_destroy(A,hPrevD_a); if(hostPrevD) _aligned_free(hostPrevD);   // --qdump anchor (null-safe)
    hbuf_destroy(A,hCurD_a);  if(hostCurD)  _aligned_free(hostCurD);    // --qdump anchor (null-safe)
    hbuf_destroy(A,hMV1_a);   if(hostMV1)   _aligned_free(hostMV1);     // --qdump+ post-consensus MV (null-safe)
    hbuf_destroy(A,hMVB1_a);  if(hostMVB1)  _aligned_free(hostMVB1);    // --qdump+ post-consensus MVB (null-safe)
    // The MV consensus pipeline + scratch (created when mv-median OR mv-guided; both are cleared together
    // on a create failure, so either being set means the resources exist).
    if(use_mv_median||use_mv_guided){ med_destroy(A,medPipe); img_destroy(A,wapMVScratchA); }
    // The bridge — VK-A image/import, the D3D11 shared texture + its keyed mutex + NT handle, P's bridge
    // cmd buffer/fence, and the A-side WAP MV/SAD host bridges. (The PresentSurface itself is a P-thread
    // local — RAII-destroyed when run_present returned, on its creating thread, per the pillar threading
    // contract.) Destroyed AFTER vkDeviceWaitIdle(A) above.
    {
        for(int _g=0;_g<kGenRing;++_g){ hbuf_destroy(A,hMV_a[_g]); hbuf_destroy(A,hSAD_a[_g]); hbuf_destroy(A,hMVB_a[_g]); }   // +hMVB_a
        for(int _g=0;_g<kGenRing;++_g) hbuf_destroy(A,hC2_a[_g]);   // second-best candidate A-side import (null-safe)
        for(int _g=0;_g<kGenRing;++_g) hbuf_destroy(A,hDIS_a[_g]);   // dissidence-mask A-side import (null-safe)
        for(int _g=0;_g<kGenRing;++_g) hbuf_destroy(A,hDISB_a[_g]);  // backward dissidence-mask A-side import (null-safe)
        for(int _g=0;_g<kGenRing;++_g) hbuf_destroy(A,hPER_a[_g]);   // inertia persistence A-side import (null-safe)
        if(bridge_img.img) vkDestroyImage(A.dev,bridge_img.img,nullptr);
        if(bridge_mem) vkFreeMemory(A.dev,bridge_mem,nullptr);
        if(fBridge) vkDestroyFence(A.dev,fBridge,nullptr);
        rel(bridge_km_d3d); rel(bridge_tex);
        if(bridge_nt) CloseHandle(bridge_nt);
        // --async-present: slot-1 teardown — mirrors slot-0 exactly; every guard is null-safe so this is
        // inert when async-present was off (the slot-1 handles stay VK_NULL_HANDLE / null). Command buffers
        // are freed with the pool (slot-0's cmdBridge is not explicitly freed either — same here).
        if(bridge_img1.img) vkDestroyImage(A.dev,bridge_img1.img,nullptr);
        if(bridge_mem1) vkFreeMemory(A.dev,bridge_mem1,nullptr);
        if(fBridge1) vkDestroyFence(A.dev,fBridge1,nullptr);
        if(fBridgeA0) vkDestroyFence(A.dev,fBridgeA0,nullptr);   // slot-0 dedicated async fence (cmd buffer pool-freed)
        rel(bridge_km_d3d1); rel(bridge_tex1);
        if(bridge_nt1) CloseHandle(bridge_nt1);
    }
    // iGPU-convert pipeline teardown (guard: only created when use_igpu_convert).
    if(use_igpu_convert){ for(int _s=0;_s<cap_slots;++_s){ cpipe_destroy(G,cpPipe[_s]); unpipe_destroy(B,ubPipe[_s]); unpipe_destroy(G,ugPipe[_s]); if(cfg.igpu_field) fpipe_destroy(G,fpipe[_s]); } }
    // Destroy the smoothing pipeline + its mv storage view (aliases ofp's mv image) BEFORE ofp.shutdown,
    // then the app-owned prev image.
    if(use_mv_smooth){ mvsm_destroy(FD,mvsm); img_destroy(FD,mv_prev); }   // FD — created on FD (A under single_gpu)
    // gme-gpu: destroy the affine-fit pipeline set BEFORE ofp.shutdown (its mv/sad storage views alias
    // ofp's images). gme_destroy is null-safe (no-op when use_gme_gpu was never built — incl. single_gpu, where gme-gpu is forced off).
    gme_destroy(FD,gmePipe);   // FD (gmePipe null under single_gpu — no-op either way; FD for consistency)
    ofpA.shutdown(A.dev);  // primary-FG OFP; no-op if pfg_enabled=false
    // --nvofa: destroy the OFA provider BEFORE ofp.shutdown (its convert descriptor sets alias ofp's
    // mv/sad images). nvofa_destroy is null-safe (no-op when use_nvofa was never built). On A (==FD here).
    if(use_nvofa) nvofa_destroy(A,nvofa);
    ofp.shutdown(FD.dev);   // FD.dev — ofp was init'd on FD (A under single_gpu); B.dev null would be an invalid shutdown
    if(cvPool) vkDestroyDescriptorPool(A.dev,cvPool,nullptr);
    if(cvPipe) vkDestroyPipeline(A.dev,cvPipe,nullptr);
    if(cvLayout) vkDestroyPipelineLayout(A.dev,cvLayout,nullptr);
    if(cvDsl) vkDestroyDescriptorSetLayout(A.dev,cvDsl,nullptr);
    if(cvSamp) vkDestroySampler(A.dev,cvSamp,nullptr);

    img_destroy(A,Apresent);
    img_destroy(A,CinterpA); img_destroy(A,AframeA[1]); img_destroy(A,AframeA[0]);
    img_destroy(A,Anative); img_destroy(A,Awork); hbuf_destroy(A,Astage);
    if(use_igpu_convert) hbuf_destroy(G,Astage_g);  // G-side import of hostA; free host ptr below
    img_destroy(FD,Bframe[0]); img_destroy(FD,Bframe[1]); img_destroy(FD,Cinterp);   // FD — Bframe/Cinterp created on FD (A under single_gpu)
    img_destroy(FD,Bflow[0]); img_destroy(FD,Bflow[1]);   // VK_NULL_HANDLE no-op when flow_div==1 (FD)
    if(b_q2_split) for(int _k=0;_k<kMaxInterp;++_k) hbuf_destroy(FD,sbI[_k]);   // VRAM staging (FD; b_q2_split false under single_gpu)
    if(use_upscale||use_igpu_convert) img_destroy(G,Gsrc);
    if(use_upscale) img_destroy(G,Gdst);
    // Destroy VkBuffer/VkDeviceMemory BEFORE freeing the underlying host allocation.
    if(use_upscale) hbuf_destroy(A,hApres);
    if(use_upscale) hbuf_destroy(G,hGout);
    if(use_upscale){ for(int _g=0;_g<kGenRing;++_g) for(int _k=0;_k<kMaxInterp;++_k) hbuf_destroy(G,hI_g[_g][_k]); }
    if(use_upscale||use_igpu_convert){ for(int _s=0;_s<cap_slots;++_s) hbuf_destroy(G,hR_g[_s]); }
    for(int _g=0;_g<kGenRing;++_g) for(int _k=0;_k<kMaxInterp;++_k){ hbuf_destroy(A,hI_a[_g][_k]); hbuf_destroy(FD,hI_b[_g][_k]); }   // FD — hI_b imported on FD (A under single_gpu)
    for(int _s=0;_s<cap_slots;++_s){ hbuf_destroy(A,hR_a[_s]); hbuf_destroy(FD,hR_b[_s]); }   // FD — hR_b imported on FD
    for(int _s=0;_s<cap_slots;++_s) hbuf_destroy(A,hFIELD_a[_s]);   // A-side iGPU field import (the hostFIELD ptr is shared with G — NOT freed here; null-safe when --afill off)
    if(use_igpu_convert){ for(int _s=0;_s<cap_slots;++_s){ hbuf_destroy(G,hRP_g[_s]); hbuf_destroy(B,hRP_b[_s]); hbuf_destroy(B,hRP_b_dev[_s]); } }
    // WAP MV/SAD host bridges — the B-side imports (A-side imports freed in the bridge block above), then
    // the host allocations below.
    if(use_wap){ for(int _g=0;_g<kGenRing;++_g){ hbuf_destroy(FD,hMV_b[_g]); hbuf_destroy(FD,hSAD_b[_g]); hbuf_destroy(FD,hMVB_b[_g]); } }   // +hMVB_b (FD; imported on A under single_gpu)
    // gme-gpu: the B-side dissidence imports + model readback imports (all null-safe when off — under
    // single_gpu these were GATED OFF, so the handles are null; FD keeps the destroy device-correct regardless).
    for(int _g=0;_g<kGenRing;++_g){ hbuf_destroy(FD,hDIS_b[_g]); hbuf_destroy(FD,hDISB_b[_g]); hbuf_destroy(FD,hGmeM_b[_g]); hbuf_destroy(FD,hGmeMB_b[_g]); }   // FD
    for(int _g=0;_g<kGenRing;++_g) hbuf_destroy(FD,hC2_b[_g]);   // second-best candidate B-side import (null-safe; FD; imported on A under single_gpu)
    if(hostG) _aligned_free(hostG);
    for(int _g=0;_g<kGenRing;++_g) for(int _k=0;_k<kMaxInterp;++_k) if(hostI[_g][_k]) _aligned_free(hostI[_g][_k]);
    for(int _s=0;_s<cap_slots;++_s) if(hostR[_s]) _aligned_free(hostR[_s]);
    if(hostA) _aligned_free(hostA);
    // INGEST-ASYNC: the raw-ring imports + host allocations (all null-safe; the worker is already joined).
    for(int _k=0;_k<kRawSlots;++_k){ hbuf_destroy(A,raw_astage_a[_k]); hbuf_destroy(G,raw_astage_g[_k]); if(raw_host[_k]) _aligned_free(raw_host[_k]); }
    for(int _s=0;_s<cap_slots;++_s) if(hostRP[_s]) _aligned_free(hostRP[_s]);
    for(int _g=0;_g<kGenRing;++_g){ if(hostMV[_g]) _aligned_free(hostMV[_g]); if(hostSAD[_g]) _aligned_free(hostSAD[_g]); }   // MV/SAD host bridges
    for(int _g=0;_g<kGenRing;++_g){ if(hostMVB[_g]) _aligned_free(hostMVB[_g]); }   // backward-MV host bridge
    for(int _g=0;_g<kGenRing;++_g){ if(hostC2[_g]) _aligned_free(hostC2[_g]); }     // second-best candidate host bridge
    for(int _g=0;_g<kGenRing;++_g){ if(hostDIS[_g]) _aligned_free(hostDIS[_g]); }   // dissidence-mask host bridge
    for(int _g=0;_g<kGenRing;++_g){ if(hostDISB[_g]) _aligned_free(hostDISB[_g]); } // backward dissidence-mask host bridge
    for(int _g=0;_g<kGenRing;++_g){ if(hostPER[_g]) _aligned_free(hostPER[_g]); }   // inertia persistence host bridge
    for(int _g=0;_g<kGenRing;++_g){ if(hostGmeM[_g]) _aligned_free(hostGmeM[_g]); if(hostGmeMB[_g]) _aligned_free(hostGmeMB[_g]); }   // gme-gpu model readback bridges

    if(a_cpool_sg) vkDestroyCommandPool(A.dev,a_cpool_sg,nullptr);   // single-GPU dedicated pools (frees cmdA/cmdB with them)
    if(a_fpool_sg) vkDestroyCommandPool(A.dev,a_fpool_sg,nullptr);
    vdev_destroy(G); vdev_destroy(B); vdev_destroy(A);
    // --validation (R2): destroy the messenger BEFORE the instance, else the layer reports it as a leak.
    if(g_dbg_messenger!=VK_NULL_HANDLE){
        if(auto pDestroy=(PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(inst,"vkDestroyDebugUtilsMessengerEXT"))
            pDestroy(inst,g_dbg_messenger,nullptr);
        g_dbg_messenger=VK_NULL_HANDLE;
    }
    vkDestroyInstance(inst,nullptr);
    d3d_shutdown(d);
    // 2026-09-06: EVERY `goto done` init failure used to land on this single `return 0`, so a run that
    // never presented a frame was indistinguishable from a clean quit to the launcher (ui/src-tauri/
    // src/lib.rs reports the child's code verbatim). The exit-code contract is now: 0 clean, 1 fatal
    // (init bail or device loss), 2 argument-parse error, 3 layer-registry parity failure.
    return ra::compat::g_fatal_reason ? 1 : 0;
}
// Made with my soul - Swately <3
