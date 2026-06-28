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
#include "cli/cli.hpp"
// The vk-utility + device + globals infra.
#include "core/vk_util.hpp"
#include "core/device.hpp"
#include "core/globals.hpp"
#include "flow/flow.hpp"          // FLOW factories/structs (gme_fit/NvofaProvider/MvSmoothPipe/GmePipe/MedianPipe)
#include "capture/capture.hpp"     // D3D11/DXGI interop (OutInfo/D3D/d3d_init/find_window/d3d_shutdown/staging) + iGPU ConvPackPipe/UnpackPipe
#include "warp_blend/warp_blend.hpp" // WapPipe/FieldPipe/FillPipe factories + matte_mass_count/gme_dispct_from_mask CPU stat helpers
#include "present/present.hpp"      // present-side UpPipe upscale (bilinear/lanczos) factory
#include "instrument/instrument.hpp" // dump_bmp/dump_rgba diagnostic frame-dump helpers
// FgContext - the shared cross-thread state as references to main()'s locals;
// run_capture/F/P alias them so the bodies stay identical.
#include "core/fg_context.hpp"
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
    Config cfg; if (!parse_args(argc, argv, cfg)) return 0;
    // Ctrl+C / console-close exits CLEANLY (sets g_quit → the main loop signals the workers and
    // joins them) — the only quit path. Returns TRUE so the default terminator (a hard process
    // kill) never runs.
    SetConsoleCtrlHandler(console_ctrl_handler, TRUE);
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

    // ── WGC state ─────────────────────────────────────────────────────────────
    // Declared here (before any goto) to satisfy C++ goto rules under MSVC.
#ifdef _MSC_VER
    WgcCtx* wgc_ctx=nullptr;
    HWND    wgc_target_hwnd=nullptr; // set when --window is used (window-capture mode)
#endif

    // ── Capture init ─────────────────────────────────────────────────────────
    D3D d{};
#ifdef _MSC_VER
    // DDA-primary: --window (sin --capture-api wgc explícito) captura el MONITOR donde está la ventana,
    // vía DDA (llega al refresh del panel; WGC-ventana se auto-capa ~125 por el anti-duplicate-flood).
    if (cfg.capture_api==CA_DD && cfg.window_substr[0]) {
        // --present-own-window con DDA: persistimos el HWND del juego en wgc_target_hwnd para que
        // present.cpp lo pase como psd.game_hwnd → el yield del plano OwnWindow lo reconoce como "frente
        // válido" y NO se esconde cuando el juego está al frente. Da el pid correcto al CSV. El watchdog de
        // muerte-de-ventana es seguro en DDA (la captura del escritorio no se estanca al cerrar la ventana).
        wgc_target_hwnd=find_window_by_substr(cfg.window_substr);
        if (wgc_target_hwnd) {
            int _oi=d3d_output_index_for_monitor(MonitorFromWindow(wgc_target_hwnd,MONITOR_DEFAULTTONEAREST));
            if (_oi>=0) { cfg.cap_mon=_oi; std::printf("[ra] --window '%s' -> DDA en su monitor (salida %d)\n",cfg.window_substr,_oi); }
            else std::printf("[ra] --window '%s': no mapea a una salida DXGI del adaptador de captura (¿otra GPU?) — usando --monitor %d (DDA)\n",cfg.window_substr,cfg.cap_mon);
        } else std::printf("[ra] --window '%s': ninguna ventana visible coincide — usando --monitor %d (DDA)\n",cfg.window_substr,cfg.cap_mon);
    }
#endif
    const bool want_dd = (cfg.capture_api == CA_DD);
    if (!d3d_init(d, cfg.cap_mon, want_dd)) {
        std::printf("[ra] capture output %d unavailable (api=%s)\n",cfg.cap_mon,want_dd?"dd":"wgc");
        // The generic capture-init failure carries a NAMED reason. On a Microsoft-Hybrid laptop dGPU the
        // DD path fails by design (DuplicateOutput → UNSUPPORTED); surface that floor specifically so the
        // user knows to switch to WGC, else the generic reason.
        ra::compat::emit(want_dd && d.dev /*device made, duplication failed*/
                         ? ra::compat::ReasonCode::HYBRID_DD_WRONG_GPU
                         : ra::compat::ReasonCode::CAPTURE_INIT_FAILED);
        d3d_shutdown(d); return 1;
    }
    // Enumerate ALL DXGI adapters to build pres_outputs (present candidates from every GPU).
    std::vector<OutInfo> pres_outputs;
    { IDXGIFactory1* fac=nullptr;
      if(SUCCEEDED(CreateDXGIFactory1(__uuidof(IDXGIFactory1),(void**)&fac))){
        for(UINT _ai=0;;++_ai){ IDXGIAdapter* _oad=nullptr; if(fac->EnumAdapters(_ai,&_oad)!=S_OK) break;
            DXGI_ADAPTER_DESC _ad2{}; _oad->GetDesc(&_ad2);
            char _aname[128]={}; WideCharToMultiByte(CP_ACP,0,_ad2.Description,-1,_aname,sizeof(_aname),nullptr,nullptr);
            for(UINT _oi=0;;++_oi){ IDXGIOutput* _o=nullptr; if(_oad->EnumOutputs(_oi,&_o)!=S_OK) break;
                DXGI_OUTPUT_DESC _od{}; _o->GetDesc(&_od); OutInfo _info;
                WideCharToMultiByte(CP_ACP,0,_od.DeviceName,-1,_info.name,sizeof(_info.name),nullptr,nullptr);
                _info.coords=_od.DesktopCoordinates; _info.attached=(_od.AttachedToDesktop!=0); _info.hmon=_od.Monitor;
                std::snprintf(_info.adapter_name,sizeof(_info.adapter_name),"%s",_aname);
                { DEVMODEA _dm{}; _dm.dmSize=sizeof(_dm); if(EnumDisplaySettingsExA(_info.name,ENUM_CURRENT_SETTINGS,&_dm,0)) _info.hz=(int)_dm.dmDisplayFrequency; }
                pres_outputs.push_back(_info); rel(_o); }
            rel(_oad); }
        rel(fac); }
      if(pres_outputs.empty()) pres_outputs=d.outputs; }
    std::printf("[render_assistant] adapter: %s | present candidates:\n",d.adapter);
    for (size_t i=0;i<pres_outputs.size();++i)
        std::printf("  [%zu] %-14s %4ldx%-4ld @ (%5ld,%4ld) %dHz  [%s]%s\n",i,pres_outputs[i].name,
            pres_outputs[i].coords.right-pres_outputs[i].coords.left,pres_outputs[i].coords.bottom-pres_outputs[i].coords.top,
            pres_outputs[i].coords.left,pres_outputs[i].coords.top,pres_outputs[i].hz,
            pres_outputs[i].adapter_name,pres_outputs[i].attached?"":" [detached]");
    if (cfg.list_only) { d3d_shutdown(d); return 0; }
    if (cfg.pres_mon<0||cfg.pres_mon>=(int)pres_outputs.size()){
        // Present ON the capture/game monitor — the assistant's whole point. The surface is
        // capture-proof BY CONSTRUCTION (the pillar's WDA). --present-monitor still overrides.
        cfg.pres_mon=cfg.cap_mon;
    }
    // WDA_EXCLUDEFROMCAPTURE on the pillar's HWND makes capture+present on one output
    // feedback-free unconditionally.

    // WGC window capture: adjust d.w/d.h from target window's client rect before route_for.
#ifdef _MSC_VER
    if(cfg.capture_api==CA_WGC && cfg.window_substr[0]){
        wgc_target_hwnd=find_window_by_substr(cfg.window_substr);
        if(!wgc_target_hwnd){std::printf("[ra] --window: no visible window matching '%s'\n",cfg.window_substr);ra::compat::emit(ra::compat::ReasonCode::WINDOW_NOT_FOUND);d3d_shutdown(d);return 1;}   // named reason on the no-match bail
        RECT cr{}; GetClientRect(wgc_target_hwnd,&cr);
        if(cr.right>cr.left&&cr.bottom>cr.top){d.w=(uint32_t)(cr.right-cr.left);d.h=(uint32_t)(cr.bottom-cr.top);}
        d.fmt=DXGI_FORMAT_B8G8R8A8_UNORM; // WGC always delivers BGRA8
        if(cfg.dpi_probe){ UINT _dpi=GetDpiForWindow(wgc_target_hwnd); std::printf("[ra] --dpi-probe: GetClientRect=%ldx%ld  GetDpiForWindow=%u (scale %.2fx) — this feeds NAT_W/NAT_H + the WGC pool today\n",cr.right-cr.left,cr.bottom-cr.top,_dpi,_dpi/96.0); }
    }
#endif
    VkFormat nat_vkfmt=VK_FORMAT_UNDEFINED; uint32_t nat_bpp=0; Route route=RT_NONE; const char* rdesc="";
    if (!route_for(d.fmt,nat_vkfmt,nat_bpp,route,rdesc)) { std::printf("[ra] unsupported capture format DXGI=%d\n",(int)d.fmt); ra::compat::emit(ra::compat::ReasonCode::UNSUPPORTED_FORMAT); d3d_shutdown(d); return 1; }   // named reason on the unsupported-format bail
    const bool IS_HDR=(route==RT_HDR);
    const uint32_t NAT_W=d.w, NAT_H=d.h;
    const uint32_t WW=std::min(NAT_W,1920u), WH=std::min(NAT_H,1080u);
    const uint32_t UP_W=WW*2, UP_H=WH*2;
    // The flow-resolution DRS divisor. The OFP pyramid keeps the coarsest level ≥ kMinCoarse=32px
    // by halving (n_levels capped at kLevels=4 → coarsest ≈ WW_flow/8), so WW_flow ≥ 256 keeps a full
    // 4-level pyramid valid. Clamp the divisor DOWN (toward 1) if WW/N or WH/N would fall below 256
    // on a small capture (e.g. a 640×360 window) — the dial degrades, never breaks the matcher. At
    // flow_scale==1 (default) flow_div==1 and WW_flow==WW (byte-identical).
    // --flow-scale auto (the adaptive quality<->latency knob): pick the LOWEST flow_div (best quality) whose flow-work
    // pixels fit the --flow-scale-target-mp budget. Resolution-adaptive (the flow cost is ~pixel-count-bound). Picked at
    // STARTUP (the flow pipeline is init-sized → no runtime re-init). WAP-only (the non-WAP/primary-FG paths consume
    // full-res). The clamp below still applies (under-feeding the OFP pyramid). OFF (flow_scale_auto false) → byte-identical.
    if(cfg.flow_scale_auto && cfg.warp_at_presenter){
        const double tgt=(double)cfg.flow_scale_target_mp*1.0e6;
        uint32_t d=4u; for(uint32_t cand:{1u,2u,4u}){ if((double)(WW/cand)*(double)(WH/cand)<=tgt){ d=cand; break; } }
        cfg.flow_scale=(int)d;
        std::printf("[ra] --flow-scale auto -> %u (target %.2f MP; work %ux%u -> flow %ux%u). Adaptive quality<->latency; override --flow-scale N.\n",d,cfg.flow_scale_target_mp,WW,WH,WW/d,WH/d);
    } else if(cfg.flow_scale_auto){
        std::printf("[ra] --flow-scale auto ignored — requires warp-at-presenter (the default). Reverting to flow-scale 1.\n");
        cfg.flow_scale=1;
    }
    uint32_t flow_div=(uint32_t)cfg.flow_scale;
    while(flow_div>1u && (WW/flow_div<256u || WH/flow_div<256u)) flow_div>>=1u;   // 4→2→1
    if(flow_div!=(uint32_t)cfg.flow_scale)
        std::printf("[ra] --flow-scale %d clamped to %u — WW/N or WH/N < 256 would under-feed the OFP pyramid (capture %ux%u, work %ux%u)\n",cfg.flow_scale,flow_div,NAT_W,NAT_H,WW,WH);
    const uint32_t WW_flow=WW/flow_div, WH_flow=WH/flow_div;   // the flow's (down)sampled work size
    // The warp-output divisor (fixed at init, like flow_div). Threads into the wapOutA EXTENT, the
    // warp dispatch dims, the --afill in-place dispatch, the wapOutA→bridge upscale-blit src, the
    // --ts-smooth history (wapPrevOutA + its copy extent), and the --outdump/--qdump readback. At
    // warp_scale==1 (default) warp_div==1 ⇒ WW_warp==WW, WH_warp==WH ⇒ every site byte-identical.
    // Clamp DOWN (toward 1) if WW/N or WH/N would be degenerate (<8 — below one 8×8 workgroup tile the
    // dispatch+blit lose meaning), mirroring the flow_div pyramid-floor clamp above. WAP-only (the
    // parse-time guard already reverted N>1 when WAP is off).
    uint32_t warp_div=(uint32_t)cfg.warp_scale;
    while(warp_div>1u && (WW/warp_div<8u || WH/warp_div<8u)) warp_div>>=1u;   // 4→2→1
    if(warp_div!=(uint32_t)cfg.warp_scale)
        std::printf("[ra] --warp-scale %d clamped to %u — WW/N or WH/N < 8 (below one 8x8 workgroup) on a tiny capture (work %ux%u)\n",cfg.warp_scale,warp_div,WW,WH);
    const uint32_t WW_warp=WW/warp_div, WH_warp=WH/warp_div;   // OUR warp's (down)scaled output/dispatch size

    std::printf("[ra] capture [%d]: %ux%u DXGI=%d → %s → work %ux%u\n",cfg.cap_mon,NAT_W,NAT_H,(int)d.fmt,rdesc,WW,WH);
    // The commit marker gains the appearance band when appearance is live (commit governs):
    //   commit:0.080(real appear:0.10) / commit:0.080(real) / commit:0.080(appear:0.10) / commit:0.080
    char commit_buf[56]={};
    if(cfg.commit_thresh>0.f){
        char appear_sub[24]={};   // "appear:0.10" prefixed with a space iff (real) also present
        if(cfg.appearance) std::snprintf(appear_sub,sizeof(appear_sub),"%sappear:%.2f",cfg.commit_real?" ":"",cfg.appear_band);
        const bool paren = cfg.commit_real || cfg.appearance;   // (real appear:0.10) / (real) / (appear:0.10)
        std::snprintf(commit_buf,sizeof(commit_buf)," commit:%.3f%s%s%s%s",
                      cfg.commit_thresh,
                      paren?"(":"", cfg.commit_real?"real":"", appear_sub, paren?")":"");
    }
    char bidir_buf[40]={}; if(cfg.bidir) std::snprintf(bidir_buf,sizeof(bidir_buf)," bidir(occl:%.2fpx%s)",cfg.occl_thresh,cfg.phase_anchor?"+pa":"");   // +pa = phase-anchored primary MV
    char filldiv_buf[40]={}; if(cfg.fill_div) std::snprintf(filldiv_buf,sizeof(filldiv_buf)," fill-div(eps:%.3f)",cfg.div_eps);
    char mvguided_buf[40]={}; if(cfg.mv_guided) std::snprintf(mvguided_buf,sizeof(mvguided_buf)," mv-guided(sim:%.2f)",cfg.mv_sim);
    char matte_buf[112]={}; if(cfg.matte) std::snprintf(matte_buf,sizeof(matte_buf)," matte(thr:%.2f occ-lerp mass-k:%.2f%s%s%s%s%s)",cfg.matte_thresh,cfg.mass_k,cfg.travel?" travel":"",cfg.crescent?" crescent":"",cfg.contour?" contour":"",cfg.obj_crescent?" objcres":"",cfg.member_commit?" mbb":"");
    char object_buf[80]={}; if(cfg.objects) std::snprintf(object_buf,sizeof(object_buf)," objects(k:%d min:%d inh:%.0fpx shield:%s%s%s)",kObjSlots,kObjMinMass,kObjInhMin,cfg.persist_reset?"hud-only":"per",cfg.shapefield?" shape":"",cfg.scene_memory?" mem:0.55":"");
    char stasis_buf[40]={}; if(cfg.stasis) std::snprintf(stasis_buf,sizeof(stasis_buf)," stasis(thr:%.2f)",cfg.stasis_thresh);
    char inertia_buf[40]={}; if(cfg.inertia) std::snprintf(inertia_buf,sizeof(inertia_buf)," inertia(thr:%.2f)",cfg.inertia_thresh);
    // --mv-guided supersedes --mv-median's blind pass, so suppress the mv-median marker when guided
    // is on (the consensus pass runs color-weighted, not blind — one marker, no contradiction).
    char gme_buf[32]={}; if(cfg.gme) std::snprintf(gme_buf,sizeof(gme_buf)," gme%s%s",cfg.change_gate?"+chg":"",cfg.ambig?"+ambig":"");   // +chg = the change gate, +ambig = the second-best SAD-tie arbitration (gme child)
    // The commit-default marker ` cdef` rides NEXT TO the commit marker (the warp-vs-blend selection
    // rule, sibling to the commit's averaging-vs-commit). WARP-LEVEL (matte-independent), gated on
    // wap → printed when cfg.commit_default (` cdef` present ⇔ the flip is live this run).
    // The one-position marker ` 1pos` rides NEXT TO ` cdef` (sibling warp-vs-blend cure — the
    // warp_result composition collapse). WARP-LEVEL (matte-independent), gated on wap → printed when
    // cfg.onepos (` 1pos` present ⇔ the collapse is live this run).
    // The 1pos marker carries the band when ≠1 (the dial visible in every log).
    char onepos_buf[24]={}; if(cfg.onepos){ if(cfg.onepos_band!=1.0f) std::snprintf(onepos_buf,sizeof(onepos_buf)," 1pos:%.2f",cfg.onepos_band); else std::snprintf(onepos_buf,sizeof(onepos_buf)," 1pos"); }
    std::printf("[ra] FG: res_ceil=%.1f conf_improv=%.2f agreement=%.2f%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s\n",cfg.res_ceil,cfg.conf_improv,cfg.agreement,cfg.warp_at_presenter?" wap":"",cfg.soft_gate?" soft":"",cfg.mv_prior?" mv-prior":"",commit_buf,cfg.commit_default?" cdef":"",onepos_buf,bidir_buf,filldiv_buf,cfg.rescue?" rescue":"",(cfg.mv_median&&!cfg.mv_guided)?" mv-median":"",mvguided_buf,gme_buf,matte_buf,object_buf,stasis_buf,inertia_buf,cfg.expire?" expire":"");   // FG markers

    // ── Vulkan instance ───────────────────────────────────────────────────────
    // The WSI surface extensions are still requested (harmless, no swapchain is created — the bridge
    // presents through PresentSurface's DComp path, not a VkSwapchainKHR).
    // The device ext VK_KHR_format_feature_flags2 (a prereq of VK_NV_optical_flow) itself depends on
    // the INSTANCE ext VK_KHR_get_physical_device_properties2. Under --nvofa we enable it here so the
    // dependency chain is satisfied. Default path: 2 exts (byte-identical).
    const char* inst_exts[]={VK_KHR_SURFACE_EXTENSION_NAME,VK_KHR_WIN32_SURFACE_EXTENSION_NAME,VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME};
    VkApplicationInfo ai{}; ai.sType=VK_STRUCTURE_TYPE_APPLICATION_INFO; ai.pApplicationName="render_assistant"; ai.apiVersion=VK_API_VERSION_1_2;
    VkInstanceCreateInfo ici2{}; ici2.sType=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO; ici2.pApplicationInfo=&ai; ici2.enabledExtensionCount=cfg.nvofa?3u:2u; ici2.ppEnabledExtensionNames=inst_exts;
    VkInstance inst=VK_NULL_HANDLE; if (vkCreateInstance(&ici2,nullptr,&inst)!=VK_SUCCESS) { std::printf("[ra] instance failed\n"); d3d_shutdown(d); return 1; }

    const RECT& pc=pres_outputs[cfg.pres_mon].coords;
    // PresentSurface OWNS the panel (its own dcomp-ct HWND + WDA + message pump inside submit()) —
    // the only present path. The frame reaches the panel through the A-side bridge →
    // PresentSurface::submit(). The present-source extent fills the monitor exactly (the bridge blit,
    // FILTER_LINEAR, scales work→monitor). No HWND/VkSurface here.

    // ── Physical device selection ─────────────────────────────────────────────
    uint32_t nd=0; vkEnumeratePhysicalDevices(inst,&nd,nullptr); std::vector<VkPhysicalDevice> pds(nd); vkEnumeratePhysicalDevices(inst,&nd,pds.data());
    VkPhysicalDevice pA=VK_NULL_HANDLE,pB=VK_NULL_HANDLE,pG=VK_NULL_HANDLE;
    // Capture each selected device's deviceLUID for the per-adapter GPU% stat. The PDH
    // "\GPU Engine(*)\Utilization Percentage" instance names embed luid_0x{High}_0x{Low}; we format
    // these LUIDs the same way and sum each engine's utilization into the matching adapter.
    // deviceLUID is the 8-byte little-endian image of a Windows LUID (LowPart[0..3], HighPart[4..7]).
    LUID luidA{},luidB{},luidG{}; bool luidA_ok=false,luidB_ok=false,luidG_ok=false;
    std::string nvNameA,nvNameB;   // A/B device names → NVML handle match (the PDH GPU-Engine LUID
                                   // space does NOT equal the Vulkan/DXGI deviceLUID on some rigs, so
                                   // A/B utilization comes from NVML — nvidia-smi's own source — by name.
    std::string afrag(cfg.assist_gpu); for(auto& c:afrag) c=(char)std::tolower((unsigned char)c);
    for(auto pd:pds){ VkPhysicalDeviceIDProperties idp{}; idp.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES; VkPhysicalDeviceProperties2 p2{}; p2.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2; p2.pNext=&idp; vkGetPhysicalDeviceProperties2(pd,&p2);
        const bool primary=idp.deviceLUIDValid&&std::memcmp(idp.deviceLUID,&d.luid,sizeof(LUID))==0;
        if(primary){pA=pd; if(idp.deviceLUIDValid){std::memcpy(&luidA,idp.deviceLUID,sizeof(LUID)); luidA_ok=true;} continue;}
        if(p2.properties.deviceType==VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU){pG=pd; if(idp.deviceLUIDValid){std::memcpy(&luidG,idp.deviceLUID,sizeof(LUID)); luidG_ok=true;} continue;}
        if(p2.properties.deviceType==VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU){ if(!afrag.empty()){ std::string nm(p2.properties.deviceName); for(auto& c:nm) c=(char)std::tolower((unsigned char)c); if(nm.find(afrag)!=std::string::npos&&!pB){ pB=pd; if(idp.deviceLUIDValid){std::memcpy(&luidB,idp.deviceLUID,sizeof(LUID)); luidB_ok=true;} } } else if(!pB){ pB=pd; if(idp.deviceLUIDValid){std::memcpy(&luidB,idp.deviceLUID,sizeof(LUID)); luidB_ok=true;} } }
    }
    // Capture A/B device names for the NVML utilization match (re-query, no selection-logic change).
    if(pA){ VkPhysicalDeviceProperties pr{}; vkGetPhysicalDeviceProperties(pA,&pr); nvNameA=pr.deviceName; }
    if(pB){ VkPhysicalDeviceProperties pr{}; vkGetPhysicalDeviceProperties(pB,&pr); nvNameB=pr.deviceName; }
    // Require ONLY device A. single_gpu derives true with --force-single-gpu OR when no 2nd discrete
    // GPU physically exists (pB null). Under --force-single-gpu null out pB AND pG so the degenerate
    // path is DRIVEN (no aliasing) — every device-B/G creation gate below sees a null phys handle and
    // routes onto A (FD) instead.
    if(!pA){ std::printf("[ra] need primary GPU (LUID). A=%s\n",pA?"ok":"MISSING"); vkDestroyInstance(inst,nullptr); d3d_shutdown(d); return 1; }
    {   // The honest VRR/G-Sync vendor statement at startup. A non-hooking external overlay cannot
        // drive the captured game's VRR; we NEVER hook the game swapchain (= injection = ban).
        // Informational (always-on, no behavior change). The is_nv/is_amd substrings below select
        // only WHICH sentence to PRINT — they gate NO FG behaviour.
        std::string vlo=nvNameA; for(auto& c2:vlo) c2=(char)std::tolower((unsigned char)c2);
        const bool is_nv=(vlo.find("nvidia")!=std::string::npos||vlo.find("geforce")!=std::string::npos);
        const bool is_amd=(vlo.find("amd")!=std::string::npos||vlo.find("radeon")!=std::string::npos);
        if(is_nv)      std::printf("[ra] F2/VRR (honest): NVIDIA — G-Sync stays OFF for the captured game while PhyriadFG presents (a non-hooking overlay can't drive the game's VRR; a permanent class floor shared with LSFG). Windowed-G-Sync = the partial mitigation. We never hook the game swapchain.\n");
        else if(is_amd)std::printf("[ra] F2/VRR (honest): AMD — FreeSync MAY persist with the overlay (verify the monitor OSD); on NVIDIA it would not. We never degrade the baseline + never hook the game swapchain.\n");
        else           std::printf("[ra] F2/VRR (honest): a non-hooking external overlay cannot drive the captured game's VRR (G-Sync off on NVIDIA; AMD/Intel FreeSync may persist — verify the OSD). We never hook the game swapchain.\n");
    }
    const bool single_gpu = cfg.force_single_gpu || (pA && !pB);   // the single-GPU switch
    if(cfg.force_single_gpu){ pB=VK_NULL_HANDLE; pG=VK_NULL_HANDLE; nvNameB.clear(); luidB_ok=false; }   // drive the TRUE degenerate (no B/G aliasing)
    if(!single_gpu && !pB){ std::printf("[ra] need primary GPU (LUID) + assist discrete (or --force-single-gpu). B=MISSING\n"); vkDestroyInstance(inst,nullptr); d3d_shutdown(d); return 1; }

    // ──────────────────────────────────────────────────────────────────────────
    // From here: ALL resource variables declared BEFORE any goto. The cleanup
    // block at `done:` releases everything that was initialised (checks != null).
    // ──────────────────────────────────────────────────────────────────────────

    // Devices.
    VDev A{},B{},G{};
    // The flow/gme PRODUCER device. Multi-GPU → B (the 1080 Ti). single_gpu → A (the 4090 alone). FD
    // threads through EVERY device-B resource creation, the consume-side waits, and the teardown (the
    // destroys deref FD.dev — a null B.dev on an A-created handle would be UB). Declared BEFORE the
    // first `goto done` so no jump crosses its initializer (C++ goto rule).
    VDev& FD = single_gpu ? A : B;
    // Now that single_gpu is known, force the primary-FG (pfg) path off — its ofpA/AframeA/cmdA_fg/
    // fA_fg would otherwise double-allocate a 2nd full-res OFP on A. This is BEFORE the want_pfg-gated
    // allocations.
    if(single_gpu) want_pfg=false;
    // use_upscale stays declared (the upscale pipes are still COMPILED) but is forced false at runtime
    // by the output-clock guard — the present is ALWAYS the surface bridge, and the bridge blit
    // (FILTER_LINEAR, work→monitor extent) already scales. The pipes are deliberately not deleted.
    bool have_igpu=false, use_upscale=false, use_igpu_convert=false;
    bool use_fwd_prestage=false; // --fwd-prestage: derived gate (cfg.fwd_prestage && use_wap && use_igpu_convert); assigned after use_wap. Declared here (non-const, NOT skipped by goto done) so its init is never bypassed.
    bool use_wap=false;          // warp-at-presenter active (re-warps on A, the bridge owner)
    bool use_bidir=false;        // bidirectional flow + occlusion classification (WAP-only; needs use_wap)
    bool use_fill_div=false;     // divergence-directed disocclusion pick (bidir-only; needs use_bidir)
    bool use_rescue=false;       // candidate-rescue (WAP-only; needs use_wap + commit armed)
    bool use_mv_median=false;    // 3x3 MV vector-median in wap_upload (WAP-only; needs use_wap)
    bool use_mv_guided=false;    // color-guided MV (consensus + bilateral fetch; WAP-only; needs use_wap)
    bool use_gme=false;          // frame-holon global affine model (fit on F + rescue/fill-div in warp; WAP-only)
    bool use_matte=false;        // fluid-matte boundary-first compositing (WAP-only; needs use_gme → wapDISA + model)
    bool use_inertia=false;      // inertia prior (F-side persistence counter + 3 WAP warp gates; WAP-only; needs use_wap)
    bool use_objects=false;      // object-holon clustering + motion-inheritance repair (F-side; needs use_gme for the dissidence mask)
    bool use_ambig=false;        // ambiguity channel — second-best candidate arbitration of SAD ties (WAP warp; needs use_gme for the model referee)
    bool use_commit_default=false; // the commit-default flip — WARP IS THE DEFAULT in the warp-vs-blend selection (WAP-level, matte-independent; needs use_wap)
    bool use_onepos=false;       // the one-position collapse — warp_result is single-position where the two samples disagree (WAP-level, matte-independent; needs use_wap)
    bool use_memory=false;       // scene-holon silhouette memory (advect+merge+refresh the dissidence prior; needs use_objects)
    VkImageUsageFlags bframe_use=0, gsrc_use=0;   // assigned after use_igpu_convert is known

    // Host bridge — three disjoint aligned allocations (hostR/hostI de-aliased so the
    // real frame survives the interp FG write):
    //   hostR (WW×WH×4)      : the REAL converted frame. A writes it (hR_a, cmdA);
    //                          B reads it (hR_b) to upload Bframe[cur]; G reads it
    //                          (hR_g) to upscale the real present; A reads it (hR_a,
    //                          no-upscale path) to present the real frame.
    //   hostI (WW×WH×4)      : the INTERP frame. B writes Cinterp into it (hI_b);
    //                          G reads it (hI_g) to upscale an interp present; A reads
    //                          it (hI_a, no-upscale path) to present an interp frame.
    //   hostG (UP_W×UP_H×4)  : G writes upscale output (hGout); A reads (hApres) for present.
    // iGPU-convert additions:
    //   hostA (NAT_W×NAT_H×bpp): Astage capture buffer, aligned for EMH import on G.
    //   hostRP (WW×WH×3)     : packed RGB8 output. G writes (hRP_g) after convert+pack;
    //                          B reads (hRP_b) to ingest packed frame; G also reads (hRP_g)
    //                          for real-frame unpack before upscale.
    // Ring-buffered real slots; 2 interp generations × up to kMaxInterp interps. The capture ring
    // lets F consume captures IN ORDER (span 1) instead of jumping to the newest — jumping turns a
    // long F iteration into a source skip → span-2 pair → 2× displacement → a walking tremble. The
    // depth keeps spare overwrite margin beyond the in-order read window. The F→P interp-generation
    // ring stays at 2, independent of the capture ring.
    static constexpr int kCapSlots=32;   // compile-time MAX (the static handle-array size, ~negligible memory); the ACTIVE ring depth is the RUNTIME `cap_slots` (auto-sized below from the source/flow/resolution regime, NOT the CPU). A too-shallow ring LAPS the reals at high source (prev_slot_safe = cur_c-prev_cseq < cap_slots-1 fails → freeze-at-cur every tick); a deep ring widens the real-validity window (the present's read-side gates real_valid/prev_slot_safe use cap_slots-1) so WAP interpolates B's pairs instead of degrading to show-reals+repeat. The present still selects the freshest pair.
    // The in-order INGEST backlog cap is DECOUPLED from the ring depth. It governs LATENCY (how far behind newest F drains in order before jumping), NOT real-persistence — keep it small so latency stays low while the deep ring keeps the reals valid.
    static constexpr int kIngestBacklog=3;
    static constexpr int kCapSlotsMin=4;   // the minimum viable ring (kGenRing=3 + 1) AND the torn-read floor.
    static_assert(kCapSlotsMin >= kIngestBacklog + 1, "torn-read safety (STAGE-84/92b): the in-order ingest write head and the present read slot must never collide; the non-collision margin (cap_slots - kIngestBacklog) >= 1 is guaranteed by cap_slots >= kCapSlotsMin >= kIngestBacklog+1. Enforced against the live constants, not a frozen mod-4 example.");
    int cap_slots=kCapSlotsMin;   // the ACTIVE ring depth (<= kCapSlots max). Auto-sized at init (just before the host-bridge alloc) from the source/flow/resolution regime, or --cap-slots N override. All ring LOOPS + INDEX MATH + validity gates use this runtime value; the static arrays are sized to kCapSlots (the max).
    void* hostR[kCapSlots]={}, *hostI[kGenRing][kMaxInterp]={}, *hostG=nullptr;
    void* hostA=nullptr, *hostRP[kCapSlots]={};          // hostRP × kCapSlots
    void* hostFIELD[kCapSlots]={};                       // iGPU contour-field host bridge (CPU-readable for the verify gate)
    HBuf hR_a[kCapSlots]{},hR_b[kCapSlots]{},hR_g[kCapSlots]{}; // real-slot imports (A/B/G)
    HBuf hI_a[kGenRing][kMaxInterp]{},hI_b[kGenRing][kMaxInterp]{},hI_g[kGenRing][kMaxInterp]{};  // interp gen×k imports (A/B/G)
    HBuf hGout{},hApres{};
    HBuf hRP_g[kCapSlots]{},hRP_b[kCapSlots]{};   // packed (× kCapSlots)
    HBuf hFIELD_g[kCapSlots]{};                   // iGPU contour-field import (G writer)
    HBuf hFIELD_a[kCapSlots]{};                   // iGPU contour-field import (A reader — uploads it to wapFIELDA; --afill only)
    HBuf hRP_b_dev[kCapSlots]{};          // device-local packed copy (× kCapSlots)
    HBuf Astage_g{};              // Astage on G as SSBO
    // INGEST-ASYNC (--ingest-async, default OFF): the RAW host-buffer ring between the acquire thread
    // and the convert worker. kRawSlots clones of the Astage host buffer, each imported on A
    // (TRANSFER_SRC = the A-path convert src) AND, when use_igpu_convert, on G (STORAGE = the iGPU-path
    // convert src). Allocated ONLY when cfg.ingest_async; null/inert otherwise (byte-identical off).
    void* raw_host[kRawSlots]={};
    HBuf  raw_astage_a[kRawSlots]{};   // A imports (TRANSFER_SRC)
    HBuf  raw_astage_g[kRawSlots]{};   // G imports (STORAGE) — only when use_igpu_convert
    double raw_tcap[kRawSlots]={};     // per-slot capture timestamp (ms), carried to c_slots[s].t_cap_ms
    // Warp-at-presenter host bridges — per generation, F copies B's MV+SAD field out to these
    // (~130KB each at 1080p, RG16F at WW/8×WH/8); A imports them (hMV_a/hSAD_a, below) to upload
    // into its sampled MV/SAD images per pair-advance. Only allocated when WAP is active.
    void* hostMV[kGenRing]={}, *hostSAD[kGenRing]={};
    HBuf hMV_b[kGenRing]{},hSAD_b[kGenRing]{};
    // The second-best CANDIDATE field host bridge (RGBA16F = 8 bytes/texel, same mvw×mvh as hostMV —
    // NOT 4: the candidate carries xy=runner-up MV, z=runner-up SAD, w=0). Only allocated when
    // use_ambig (cfg.ambig && use_gme). F copies B's cand_image() out into hC2_b (TRANSFER_DST, same
    // FWD-pass copy as hMV_b/hSAD_b); A uploads it into wapC2A (binding 10) per pair. Same hfb
    // align-rounded EMH discipline as hostMV (POINTER and SIZE host_align-rounded), but the per-texel
    // size is 8 not 4 so its own field byte-count (cfb) is mvw×mvh×8.
    void* hostC2[kGenRing]={};
    HBuf hC2_b[kGenRing]{};
    // Backward-flow MV host bridges (RG16F, same mvw×mvh as hostMV). Only allocated when --bidir is
    // active (use_bidir). F copies B's backward MV into hMVB_b (TRANSFER_DST); A uploads it into
    // wapMVBA (TRANSFER_SRC). SAD-bwd is not shipped.
    void* hostMVB[kGenRing]={};
    HBuf hMVB_b[kGenRing]{};
    // The frame-holon's per-block dissidence mask (uint8 = min(255, round(16·r)) where r = |mv_block
    // − model(x,y)| in px). Computed on the CPU in F (single-threaded, the fit owns it), so the bridge
    // is a plain _aligned_malloc host buffer (NO B-side GPU write — F writes it via memcpy semantics)
    // imported on A for the per-pair upload into wapDISA. mvw×mvh bytes, hfb align-rounded (the EMH
    // discipline). Only allocated when use_gme.
    void* hostDIS[kGenRing]={};
    HBuf hDIS_a[kGenRing]{};
    // The BACKWARD (cur-anchored) dissidence mask — the SAME uint8 = min(255, round(16·r)) quantization,
    // but r = |mv_bwd − model_bwd| computed by the SAME gme_fit_affine run on the bwd MV field (hostMVB).
    // It carries the cur-anchored object silhouette; advected backward in the warp it covers the LEADING
    // edge the prev-anchored mask arrives late at. Same dfb discipline as hostDIS: a plain _aligned_malloc
    // host buffer (F writes it on the CPU after the bwd fit), imported on A for the per-pair upload into
    // wapDISBA. Only allocated when use_gme (the matte gate already forbids --matte without --bidir, so
    // the bwd field is always present when gme is on with matte).
    void* hostDISB[kGenRing]={};
    HBuf hDISB_a[kGenRing]{};
    // ── gme-gpu: the device-B side of the dissidence bridges + the model readback. ──────────
    // When --gme-gpu the dis-mask is GPU-PRODUCED on B (gme_dissidence.comp atomicOrs into the host
    // bridge directly) instead of CPU-written. So hostDIS/hostDISB get a B-side STORAGE import (hDIS_b/
    // hDISB_b) — the dissidence shader's binding-2 target. The A-side imports (hDIS_a/hDISB_a) stay (A
    // still uploads the mask into wapDISA/wapDISBA). hostGmeM[gen] is the 6-float model readback: B
    // copies the device-local Model SSBO into it (TRANSFER_DST) after the solve passes; F reads it in
    // consume_wap. All only allocated when use_gme_gpu.
    GmePipe gmePipe{};
    bool use_gme_gpu=false;
    HBuf hDIS_b[kGenRing]{}, hDISB_b[kGenRing]{};       // B-side STORAGE imports of hostDIS/hostDISB
    void* hostGmeM[kGenRing]={}, *hostGmeMB[kGenRing]={};// 6-float model readback (host-coherent): fwd / bwd
    HBuf hGmeM_b[kGenRing]{}, hGmeMB_b[kGenRing]{};      // B imports (vkCmdCopyBuffer dest): fwd / bwd
    // NOTE fwd vs bwd model use SEPARATE readback buffers: cmdB_bwd is submitted async (fB2) BEFORE the CPU
    // reads the fwd model, so a shared buffer would race (bwd model could clobber the fwd before F reads it).
    // The inertia prior's per-block persistence bridge — R8 (1 byte/MV-block, the uint8 counter mapped
    // straight to the [0,255] R8 store). The SOURCE state is a SINGLE F-local array accumulated across
    // pairs (NOT per-gen — it integrates motion history over time); F memcpy's that continuous array into
    // hostPER[f_gen] right after the persistence update, and A imports it (read-side, TRANSFER_SRC upload
    // into wapPERA). Same R8 dfb discipline + A-side-only import as hostDIS (F writes it on the CPU; no
    // B-side GPU write). Only allocated when cfg.inertia. The per-gen bridge is the upload vehicle; the
    // continuity lives in the F-local persist[] array (declared in thr_f_fn).
    void* hostPER[kGenRing]={};
    HBuf hPER_a[kGenRing]{};
    // Per-interp B-VRAM staging buffers. The set build copies Cinterp→sbI[k] VRAM→VRAM on B.q
    // (fast, ~0.5ms), then the slow x4 leg sbI[k]→hI_b[gen][k] runs on B.q2 (the second queue) with
    // its OWN fence tfb[k], NOT waited inline — overlapped with the next warp. CPU fence-submit
    // ordering (q2 submit AFTER the B.q fB wait) makes sbI[k] visible to q2 without semaphores. Only
    // active when B.q2 is a real second queue; B.q2==B.q falls back to the serial Cinterp→hI_b path.
    HBuf sbI[kMaxInterp]{};
    VkCommandBuffer cmdB2[kMaxInterp]={};
    VkFence tfb[kMaxInterp]={};
    bool b_q2_split=false;

    // Images.
    ID3D11Texture2D* dxgi_stage=nullptr;
    ID3D11Texture2D* dxgi_stage2=nullptr;   // INGEST-ASYNC: 2nd DDA staging texture (readback double-buffer); created only when cfg.ingest_async
    HBuf Astage{};
    Img Anative{},Awork{},Bframe[2]{},Cinterp{},Gsrc{},Gdst{};
    // Per-pair flow-input downscale scratch (only allocated when flow_div>1). Two WW_flow×WH_flow
    // RGBA8 DEVICE_LOCAL images (SAMPLED so the OFP can read them as a_view/b_view; TRANSFER_DST as the
    // vkCmdBlitImage destination). The F thread blits Bframe[prv]/Bframe[cur] (full-res) into these with a
    // LINEAR filter before record_optical_flow, so the level-0 match input matches the smaller MV grid.
    Img Bflow[2]{};
    uint32_t pres_w=WW, pres_h=WH;     // finalised once use_upscale is known (host bridge section)
    Img Apresent{};                      // present source on A: pres_w×pres_h RGBA8

    // Convert pipeline (A).
    VkSampler cvSamp=VK_NULL_HANDLE; VkDescriptorSetLayout cvDsl=VK_NULL_HANDLE;
    VkPipelineLayout cvLayout=VK_NULL_HANDLE; VkPipeline cvPipe=VK_NULL_HANDLE;
    VkDescriptorPool cvPool=VK_NULL_HANDLE; VkDescriptorSet cvSet=VK_NULL_HANDLE;

    // Temporal MV smoothing (B-path only; app-local EMA on ofp's MV field).
    MvSmoothPipe mvsm{}; Img mv_prev{}; bool use_mv_smooth=false;

    // OpticalFlowPipeline (B).
    phyriad::render::vulkan::OpticalFlowPipeline ofp{};
    // --nvofa: the hardware OFA flow provider (replaces ofp.record_optical_flow's match when armed).
    // use_nvofa is set TRUE only after a SUCCESSFUL nvofa_create (gated on --nvofa && single_gpu &&
    // A.ofaQueue). When false EVERYTHING below uses the classical ofp path (byte-identical).
    NvofaProvider nvofa{};
    bool use_nvofa=false;
    // Primary-FG path: optional OFP on A when A has spare cycles.
    // Zero PCIe transfer cost: Awork is already on A's VRAM after the convert pass.
    phyriad::render::vulkan::OpticalFlowPipeline ofpA{};
    Img AframeA[2]{}, CinterpA{};   // ping-pong frames + interp output on A
    bool pfg_enabled=false;         // true once ofpA.init() succeeds

    // Upscale pipeline (G) — kept compiled but forced off (the bridge blit scales).
    UpPipe upPipe{};
    // (The warp-at-presenter pipeline lives on A — wapPipeA / wap*A, declared in the bridge section
    // below.)
    // iGPU-convert pipelines (one set per real slot).
    ConvPackPipe cpPipe[kCapSlots]{};  // G: igpu_convert_pack per real slot (Astage_g → hRP_g[s])
    FieldPipe    fpipe[kCapSlots]{};    // iGPU contour-field pipe per real slot (hR_g → hFIELD_g)
    UnpackPipe   ubPipe[kCapSlots]{};  // B: unpack_packed per real slot (hRP_b[s] → Bframe[0/1])
    UnpackPipe   ugPipe[kCapSlots]{};  // G: unpack_packed per real slot (hRP_g[s] → Gsrc)

    // Command buffers + fences. No present semaphores (no swapchain present).
    VkCommandBuffer cmdA=VK_NULL_HANDLE,cmdB=VK_NULL_HANDLE,cmdG=VK_NULL_HANDLE;
    VkCommandPool a_cpool_sg=VK_NULL_HANDLE,a_fpool_sg=VK_NULL_HANDLE;   // single-GPU: dedicated C(convert)/F(flow) command pools on A — command pools are externally-synchronized (one recording thread each); avoids the validation MultipleThreads-Write/segfault.
    VkCommandBuffer cmdGP=VK_NULL_HANDLE;    // P-thread G cmd buffer (upscale path — forced off)
    VkCommandBuffer cmdA_fg=VK_NULL_HANDLE;  // F-thread pfg OFP on A
    // A SECOND B cmd buffer for the bwd pass — the overlap submits bwd (cmdB_bwd/fB2) and then runs
    // the fwd CPU fit while it matches on the GPU. cmdB cannot be re-recorded while its fwd submit is
    // conceptually still the "owner" of fB; a distinct buffer+fence lets the bwd record proceed in
    // flight without resetting an in-use buffer. Allocated from B.pool when bidir is live (the only
    // consumer); shares B.q (externally synchronized — F is its only submitter).
    VkCommandBuffer cmdB_bwd=VK_NULL_HANDLE;  // F-thread bwd-pass cmd buffer (B.pool)
    // The FORWARD-pass ping-pong. Two cmd buffers + two fences (depth-2 frames-in-flight) let pair N's
    // fwd flow be submitted NO-WAIT to cmdB_fwd[N&1]/fB_fwd[N&1] while pair N-1's fwd (cmdB_fwd[(N-1)&1]/
    // fB_fwd[(N-1)&1]) is consumed by the CPU tail — a distinct buffer+fence per parity so neither resets
    // the other while it is conceptually in flight. Allocated from B.pool (F is B.q's only submitter,
    // externally synchronized) ONLY when use_wap && cfg.fwd_pipeline (the opt-in path; idle otherwise →
    // the serial path uses cmdB/fB, byte-unchanged).
    VkCommandBuffer cmdB_fwd[2]={VK_NULL_HANDLE,VK_NULL_HANDLE};
    VkFence         fB_fwd[2]={VK_NULL_HANDLE,VK_NULL_HANDLE};
    // --fwd-prestage: the prestage ping-pong. Two cmd buffers + two fences (depth-2 frames-in-flight)
    // hold ONLY the hRP_b[s]->hRP_b_dev[s] device copy, submitted NO-WAIT on the F flow lane (A.q2)
    // BEFORE cmdF so the copy overlaps the flow record + the ring-guard spin instead of sitting in
    // front of the blocking flow submit. Distinct buffer+fence per g_seq parity so pair N's prestage
    // does not reset the buffer/fence pair N-1's prestage may still be draining. Allocated from the
    // SAME F flow pool as cmdB (a_fpool_sg under single_gpu, else FD.pool) ONLY when cfg.fwd_prestage
    // && use_wap && use_igpu_convert (the opt-in path; idle otherwise → the serial path records the
    // copy INLINE in cmdF, byte-unchanged).
    VkCommandBuffer cmdF_pre[2]={VK_NULL_HANDLE,VK_NULL_HANDLE};
    VkFence         fF_pre[2]={VK_NULL_HANDLE,VK_NULL_HANDLE};
    VkFence fA=VK_NULL_HANDLE,fB=VK_NULL_HANDLE,fG=VK_NULL_HANDLE;
    VkFence fGP=VK_NULL_HANDLE;              // upscale fence (forced-off path)
    VkFence fA_fg=VK_NULL_HANDLE;            // F-thread pfg fence
    VkFence fB2=VK_NULL_HANDLE;             // bwd-submit fence (must not reset fB in flight)

    // ── PresentSurface bridge ────────────────────
    // THE present path. The present pillar owns the panel (dcomp-ct + WDA + its message pump). The app
    // produces each frame into a D3D11 SHARED texture (BGRA8, present-monitor extent) on adapter A —
    // the panel owner / 4090 — and hands the NT handle to surface.submit() (one CopyResource +
    // Present(0)). VK-A imports that texture as a B8G8R8A8 image (the present-source blit dst → a channel
    // reinterpretation). Sync: VK_KHR_win32_keyed_mutex if A exposes it (NVIDIA does), else a CPU fence +
    // no-KM texture. The surface itself is created IN P's setup (the threading contract: create()+submit()
    // on the SAME thread). bridge_* are the producer side (D3D11+VK), allocated here (before goto).
    namespace pp = phyriad::render::present;
    uint32_t bridge_w=0, bridge_h=0;             // present-monitor extent (= surface backbuffer)
    ID3D11Texture2D* bridge_tex=nullptr;         // D3D11 shared producer texture (on d.dev = A/4090)
    IDXGIKeyedMutex* bridge_km_d3d=nullptr;      // its keyed mutex (D3D11 side; null if no-KM path)
    HANDLE           bridge_nt=nullptr;          // CreateSharedHandle NT handle → surface.submit()
    Img   bridge_img{};                          // VK-A image aliasing bridge_tex (B8G8R8A8)
    VkDeviceMemory bridge_mem=VK_NULL_HANDLE;    // imported memory backing bridge_img
    VkCommandBuffer cmdBridge=VK_NULL_HANDLE;    // A's bridge-blit/warp cmd buffer (P thread)
    VkFence fBridge=VK_NULL_HANDLE;              // bridge-blit fence (CPU ordering before submit())
    bool   bridge_use_km=false;                  // keyed-mutex sync path shipped (vs CPU fence)
    // --async-present: the SECOND bridge slot. Mirrors slot-0 EXACTLY (same extent / desc / shared NT
    // handle / VK import / cmd buffer / fence). Allocated ONLY when cfg.async_present (created below,
    // right after slot-0); these declarations init to null/VK_NULL_HANDLE so they always exist for the
    // teardown guards regardless of the flag. With async off they stay null and are never touched →
    // byte-identical. bridge_w/bridge_h are shared (the second slot is the SAME present-monitor extent).
    ID3D11Texture2D* bridge_tex1=nullptr;        // slot-1 D3D11 shared producer texture (on d.dev = A/4090)
    IDXGIKeyedMutex* bridge_km_d3d1=nullptr;     // slot-1 keyed mutex (null if no-KM path)
    HANDLE           bridge_nt1=nullptr;         // slot-1 CreateSharedHandle NT handle → surface.submit()
    Img   bridge_img1{};                         // slot-1 VK-A image aliasing bridge_tex1 (B8G8R8A8)
    VkDeviceMemory bridge_mem1=VK_NULL_HANDLE;   // slot-1 imported memory backing bridge_img1
    VkCommandBuffer cmdBridge1=VK_NULL_HANDLE;   // slot-1 A's bridge-blit/warp cmd buffer (P thread)
    VkFence fBridge1=VK_NULL_HANDLE;             // slot-1 bridge-blit fence (non-blocking poll target)
    // Slot-0's async warp needs its OWN cmd buffer + fence — it must NOT record into the shared
    // cmdBridge/fBridge, because the per-pair wap_upload resets+resubmits THOSE, and the async
    // (non-blocking) warp submit leaves them in flight → resetting an in-flight cmd buffer/fence = a
    // GPU fault. Slot 0 keeps the shared present TEXTURE (bridge_img/nt/mem); only its cmd+fence become
    // dedicated.
    VkCommandBuffer cmdBridgeA0=VK_NULL_HANDLE;  // slot-0 DEDICATED async cmd buffer (allocated only when async on)
    VkFence fBridgeA0=VK_NULL_HANDLE;            // slot-0 DEDICATED async fence (non-blocking poll target)
    // --upload-xfer: PING-PONG upload command buffers on A.poolT (the transfer queue's pool) + the two
    // P-thread-local monotonic timeline counters. cmdUpload[2] lets a new pair-advance upload record into
    // the FREE slot while the prior upload may still execute on A.qT (never reset in flight). uslot =
    // round-robin index; uslot_val[s] = the semUpTL value signaled when slot s was last submitted (0 =
    // never used) — the non-blocking reuse guard polls semUpTL against it. xfer_U/xfer_W are the upload /
    // warp counters: each upload SIGNALS semUpTL=++xfer_U, each warp SIGNALS semWarpTL=++xfer_W; the warp
    // WAITS semUpTL>=xfer_U (RAW) and the next upload WAITS semWarpTL>=xfer_W (WAR back-edge). Both waits
    // are BACKWARD-looking (prior submits) → no cycle → no deadlock. Allocated once at init from A.poolT
    // (zero-alloc hot path); never touched when xfer_on is false (byte-identical).
    VkCommandBuffer cmdUpload[2]={VK_NULL_HANDLE,VK_NULL_HANDLE};
    uint64_t uslot_val[2]={0,0}; int uslot=0;
    uint64_t xfer_U=0, xfer_W=0;
    // --upload-xfer: the SINGLE runtime gate + the CONCURRENT family pair, declared at this main-local
    // scope so they are visible to BOTH the wap-image creation block AND the wap_upload /
    // wap_warp_present lambdas in the present loop (the if(use_wap) image block closes before the loop).
    // ASSIGNED below once device A exists (its A.qT/A.poolT come from vdev_create). OFF here → all the
    // xfer paths are inert and the code is byte-identical.
    bool xfer_on=false; uint32_t xfer_fams[2]={0,0};
    // WAP-on-A: the warp runs on A. A-local WAP pipeline + its sampled inputs (two pair reals, MV, SAD)
    // and the rgba8 warp output, plus A-side MV/SAD host bridges (EMH align-rounded size). Warp writes
    // wapOutA; a blit (rgba8→bgra8 channel reinterpretation) lands it in bridge_img.
    WapPipe wapPipeA{};
    FillPipe fillPipeA{};   // the field VISUALIZER pipe (wapOutA + wapFIELDA storage images; --afill only)
    Img wapPrevA{},wapCurA{},wapMVA{},wapSADA{},wapOutA{};
    Img wapFIELDA{};        // A-side iGPU contour field image (R32_UINT, full-res; --afill OR --bg-snap)
    Img wapFIELDph{};       // 1×1 r32ui binding-11 placeholder when neither --afill nor --bg-snap owns wapFIELDA (never sampled)
    Img wapMVBA{};                                // A-side backward-MV sampled image (binding 5)
    Img wapC2A{};                                 // A-side second-best CANDIDATE sampled image (RGBA16F,
                                                  // mvw×mvh). Created with use_ambig; uploaded from hC2_a per pair;
                                                  // bound as warp binding 10 (u_candidates). Placeholder-bound to
                                                  // wMV.view when ambig is off (the completeness trick) — never sampled.
    Img wapMVTA{};                                // --vblend: A-side NEXT-pair forward-MV TARGET image
                                                  // (RG16F, mvw×mvh — SAME format/size as wapMVA). Created with
                                                  // cfg.vblend; uploaded from hMV_a[target_gen] (the next-fresher
                                                  // generation) per pair; bound as warp binding 12 (u_mv_target).
                                                  // Placeholder-bound to wMV.view when vblend is off (the binding-5/10
                                                  // completeness trick) — never sampled (vblend_on=0 gates the read).
    Img wapPrevOutA{};                            // --ts-smooth: A-side PREVIOUS final-output history image
                                                  // (RGBA8, full-res WW×WH — SAME format/size as wapOutA, mirrored).
                                                  // Created with cfg.ts_smooth>0; the host copies wapOutA → wapPrevOutA
                                                  // after each tick's blit; bound as warp binding 13 (u_prev_out).
                                                  // Placeholder-bound to wCur.view when ts_smooth is off (the binding-
                                                  // 5/10/12 completeness trick) — never sampled (ts_smooth=0 gates the read).
    HBuf hMV_a[kGenRing]{}, hSAD_a[kGenRing]{};   // A-side imports of hostMV/hostSAD (surface WAP)
    HBuf hMVB_a[kGenRing]{};                       // A-side import of hostMVB (backward MV)
    HBuf hC2_a[kGenRing]{};                        // A-side import of hostC2 (second-best candidate, RGBA16F)
    MedianPipe medPipe{};                          // 3x3 vector-median on the WAP MV field(s)
    Img wapMVScratchA{};                           // ONE RG16F scratch (mvw×mvh) — median dst
    Img wapDISA{};                                  // A-side R8 dissidence mask (mvw×mvh). Uploaded
                                                    // from hDIS_a per pair; bound as warp binding 6.
    Img wapDISBA{};                                 // A-side R8 BACKWARD (cur-anchored) dissidence
                                                    // mask (mvw×mvh). Uploaded from hDISB_a per pair; bound as
                                                    // warp binding 7 (u_dissidence_bwd) for the dual-anchored matte.
    Img wapPERA{};                                  // A-side R8 inertia persistence mask (mvw×mvh).
                                                    // Created unconditionally with WAP (cheap; binding 8 must
                                                    // always have a valid R8 view); uploaded from hPER_a per
                                                    // pair only when use_inertia. Bound as warp binding 8
                                                    // (u_persistence); read only when inertia_thresh>0.
    // The presented-matte-mass counter — a host-visible 4-byte STORAGE buffer bound as warp binding 9.
    // The warp atomicAdds the per-workgroup OBJECT-pixel count into it (gated on matte_on>0.5). The SSBO
    // the shader atomicAdds into is a DEVICE-LOCAL buffer (devMass, VRAM) so the GPU atomic does not
    // cross PCIe per workgroup. The per-dispatch result is reset on-GPU (vkCmdFillBuffer) and copied to
    // the host-imported buffer (vkCmdCopyBuffer) inside cmdBridge after the dispatch, so P reads
    // hostMassPtr AFTER the fBridge fence wait — no extra submit, no stall. hMass_a is the COPY DEST.
    void* hostMassPtr=nullptr;                       // the aligned CPU alloc (the host-coherent COPY dest)
    HBuf  hMass_a{};                                 // the A-device host import (vkCmdCopyBuffer destination)
    HBuf  devMass{};                                 // the device-local SSBO (binding 9; VRAM atomicAdd target)
    // The warp-output readback buffer (--outdump N). hfb discipline.
    void* hostOutD=nullptr; HBuf hOutD_a{};
    // --qdump <dir> N: TWO MORE readback buffers — the two warp anchors (wapPrevA=real N, wapCurA=real
    // N+2). The live wapOutA reuses the --outdump buffers (hostOutD/hOutD_a, allocated below when EITHER
    // --outdump OR --qdump is set). All gated on qdump_n>0 → zero cost (no alloc) when OFF.
    void* hostPrevD=nullptr; HBuf hPrevD_a{};
    void* hostCurD =nullptr; HBuf hCurD_a{};

    // ── Devices ───────────────────────────────────────────────────────────────
    // The present is ALWAYS the A-side bridge → PresentSurface (no VkSwapchain on any device). G never
    // presents — its only niche is igpu-convert (fused convert+pack, CPU-proximity). A enables
    // VK_KHR_external_memory_win32 + VK_KHR_win32_keyed_mutex for the bridge unconditionally.
    // Skip B's vdev_create under single_gpu (B stays a null VDev{}); the flow/gme producer rides A (FD).
    // A still needs EMH for the host bridges (unconditional). Gate the B half.
    // --nvofa: request the OFA queue on device A (the 4090 — the only OFA-capable device; the 1080 Ti has
    // no OFA). want_ofa=cfg.nvofa; vdev_create auto-disables (ofaQueue stays null) if A lacks
    // VK_NV_optical_flow. The runtime use is FURTHER gated on single_gpu (FD==A) below — under multi-GPU the
    // flow rides B (no OFA) so NVOFA cannot apply (NVOFA is a single-GPU lever).
    if(!vdev_create(pA,A,true,/*want_extmem_win32=*/true,/*prefer_same_family_q2=*/true,/*want_xfer_q=*/cfg.upload_xfer,/*want_ofa=*/cfg.nvofa)
       || (!single_gpu && !vdev_create(pB,B,false))){ std::printf("[ra] device creation failed\n"); goto done; }
    // NOTA: el 4090 soporta OFA por HW pero en multi-GPU el flow corre en el 1080 Ti (sin OFA),
    // así que --nvofa no aplica aquí. Avisar la opción (NO auto-activar: la salida OFA está sin calibrar y
    // usarla obliga a single-GPU, perdiendo el offload de flow del 1080 Ti).
    if(A.has_optical_flow && !single_gpu && !cfg.nvofa)
        std::printf("[ra] NVOFA: el 4090 soporta optical-flow por HW; para usarlo --nvofa --force-single-gpu "
                    "(el flow vuelve al 4090; se pierde el offload de flow del 1080 Ti — medir antes de adoptar).\n");
    // On a single-graphics-queue GPU A.q2==A.q (q2mode falls back to shared). The flow submit would then
    // land on A.q and RACE the present thread P (external-sync violation → device-lost). The 4090 has
    // queueCount>1 so A.q2!=A.q and this passes. Hard-refuse otherwise (the safe first cut; the CPU-mutex-
    // serialize-on-A.q degraded mode is not shipped) — NEVER a silent A.q2→A.q fallthrough.
    if(single_gpu && A.q2==A.q){
        std::printf("[ra] --force-single-gpu: device A has a single graphics queue (A.q2==A.q) — the flow submit "
                    "would race the present thread on A.q (device-lost). REFUSING (the safe first cut). This GPU "
                    "needs the serialize-on-A.q degraded mode (not yet shipped). Use a GPU with >1 graphics queue.\n");
        goto done;   // the standard cleanup (A created; nothing else allocated yet) — same as the hard-exit's intent
    }
    if(pG) have_igpu=vdev_create(pG,G,/*want_swap=*/false);  // G is convert-only (never presents); pG is null under --force-single-gpu so this is skipped
    // Force convert onto A (CG_PRIMARY) so the C-thread's convert runs on A.q2, serialized vs the
    // F-thread flow via a_q2_mtx (below). pG was nulled → have_igpu is false → the iGPU convert path is
    // unavailable anyway; this makes the intent explicit.
    if(single_gpu) cfg.convert_gpu=CG_PRIMARY;
    if(have_igpu){   // report G's queue split (convert on q2)
        const char* qmode=(G.q2==G.q)?"shared":(G.qfam2==G.qfam?"split-queue":"split-family");
        const uint32_t cqi=(G.q2==G.q)?0u:(G.qfam2==G.qfam?1u:0u);
        std::printf("[ra] G queues: convert fam=%u q=%u (%s)\n",G.qfam2,cqi,qmode);
    }
    if(single_gpu){   // single-GPU: the flow/gme producer rides A.q2 (the same-family split-queue), serialized vs C's convert by a_q2_mtx.
        std::printf("[ra] SINGLE-GPU: PhyriadFG collapsed onto device A alone (flow+gme+convert on A.q2, present on A.q). B/G suppressed.\n");
    } else {   // report B's queue split — the interp copy-outs overlap on B.q2 when split.
        const char* bqmode=(B.q2==B.q)?"shared (serial copy-out)":(B.qfam2==B.qfam?"split-queue (overlap)":"split-family (overlap)");
        std::printf("[ra] B queues: interp copy-out fam=%u (%s)\n",B.qfam2,bqmode);
    }
    // --cap-route-probe: a break-even-style CAPABILITY routing decision over the FG's app-local VDevs +
    // a vendor-NAMED capability manifest. MEASUREMENT-ONLY (no behaviour change → byte-identical off AND
    // on; it only std::printf's). INERT by design: the FG's A/B/G roles are FIXED by architecture
    // (A=present/warp, B=flow/gme, G=convert) and the FG's arithmetic intensity (~2.5 FLOP/byte) is far
    // below the break-even crossover (~596 on the author's link), so a capability-driven offload decision
    // returns offload=false ALWAYS — the const-inlined invariant this app encodes. This probe does NOT
    // route; it reports what such a decision WOULD say, so the routing is verifiable rather than hidden.
    // The arithmetic mirrors framework/gpu break_even_decide — replicated inline because this app does
    // NOT link phyriad_gpu. Speculative + inert-by-default on NVIDIA / fixed-role FG.
    if(cfg.cap_route_probe){
        // The vendor-NAMED capability manifest — vendor strings confined HERE (named messaging, not a
        // decision path). No vendor_id/name gates ANY behaviour: the routing decision below keys ONLY on
        // capability fields.
        auto vendor_name=[&](const VDev& v)->const char*{
            std::string lo(v.name); for(auto& ch:lo) ch=(char)std::tolower((unsigned char)ch);
            if(lo.find("nvidia")!=std::string::npos||lo.find("geforce")!=std::string::npos) return "NVIDIA";
            if(lo.find("amd")!=std::string::npos||lo.find("radeon")!=std::string::npos)     return "AMD";
            if(lo.find("intel")!=std::string::npos||lo.find("arc")!=std::string::npos)      return "Intel";
            return "other";   // messaging only, not a decision path
        };
        auto cap_line=[&](const char* role,const VDev& v){
            if(v.dev==VK_NULL_HANDLE){ std::printf("[ra] cap-probe %s: (absent)\n",role); return; }
            std::printf("[ra] cap-probe %s: vendor=%s '%s' has_fp16=%d has_dp4a=%d fp16_storage=%d fp16_gflops=%.1f(0=uncharacterized)\n",
                        role,vendor_name(v),v.name,(int)v.has_fp16,(int)v.has_dp4a,(int)v.fp16_storage,v.fp16_gflops);
        };
        cap_line("A(present/warp)",A);
        if(!single_gpu) cap_line("B(flow/gme)",B);
        if(have_igpu)   cap_line("G(convert)",G);
        // The break-even-style decision (mirrors framework/gpu break_even_decide). Inputs are the MEASURED
        // capability fields when characterized, else degenerate → the honest decline. FG AI ≈ 2.5 FLOP/byte.
        // fp16_gflops==0 (not yet characterized — we do not measure it on this path) makes the inputs
        // degenerate ⇒ offload=false by the same guard the pillar uses. Even with characterized FLOPS the FG's
        // AI ≪ crossover keeps offload=false; the FG's A/B/G roles are not chosen by this number anyway.
        auto break_even_offload=[&](double sup_flops,double ass_flops,double link_bps,double ai,double thresh=0.02)->bool{
            if(sup_flops<=0.0||ass_flops<=0.0||link_bps<=0.0||ai<=0.0) return false;   // not-characterized → honest decline
            const double invs=1.0/sup_flops+1.0/ass_flops+1.0/(ai*link_bps);
            const double f=(1.0/sup_flops)/invs; const double speedup=1.0/(1.0-f);
            return (speedup-1.0)>thresh;
        };
        const double fg_ai=2.5;   // FG's arithmetic intensity (FLOP per result byte) — the const this app encodes
        const double sup=(double)A.fp16_gflops*1.0e9;                 // A's measured fp16 FLOP/s (0 ⇒ degenerate)
        const double ass=single_gpu?0.0:(double)B.fp16_gflops*1.0e9;  // B's measured fp16 FLOP/s (0 ⇒ degenerate)
        const double link=0.0;   // cross-GPU link BW not measured on this path (0 ⇒ degenerate → honest decline)
        const bool off=break_even_offload(sup,ass,link,fg_ai);
        std::printf("[ra] cap-probe DECISION: offload=%s (FG AI~%.1f; link/fp16-gflops uncharacterized on this path => degenerate => honest decline). INERT BY DESIGN: the FG's A/B/G roles are architecture-fixed; this decision ROUTES NOTHING -- it reports what a capability-driven gate WOULD say. Coverage/speculative, not a default win.\n", off?"true":"false", fg_ai);
    }
    // Upscale machinery stays COMPILED but is forced off — the output clock is always the timer, and the
    // per-tick upscale path is not supported under it. The bridge blit (FILTER_LINEAR, work→monitor)
    // already scales. So use_upscale is structurally false at runtime.
    use_upscale=(!cfg.no_upscale&&have_igpu);
    if(use_upscale){
        use_upscale=false;
        std::printf("[ra] upscale: unsupported under the surface present path (v1) — forcing --no-upscale (the bridge blit scales)\n");
    }
    // --real-fast-path — upscale-parity refusal. The real-fast-path blits hR_a[rs] (WW×WH) into the
    // bridge via the bslot record; if use_upscale were live, the present source would be the UP_W×UP_H
    // upscaled buffer and the rfp record would NOT run do_upscale_real_P → a wrong-size/garbage present.
    // Rather than replicate the upscale pre-pass on the fast path, REFUSE --rfp under use_upscale.
    // use_upscale is structurally false here (forced off above), so this is belt-and-suspenders —
    // correct-by-construction if upscale is ever re-enabled through the bridge.
    if(cfg.real_fast_path && use_upscale){
        cfg.real_fast_path=false;
        std::printf("[ra] --real-fast-path: --upscale is active → DISABLING --rfp (the fast path presents the native-res real; an upscaled present needs the do_upscale_real_P pre-pass not replicated on this path).\n");
    }
    // use_igpu_convert: iGPU does fused convert+pack in sysRAM (zero PCIe).
    // Requires: iGPU + SDR 8bpc (RT_PASS) or HDR (RT_HDR) + no scaling (NAT==WW).
    // When active: 4090 freed of ALL compute except capture+present; FG stays on B; primary-FG disabled.
    // HDR (FP16 scRGB, DD on an HDR desktop) is an iGPU-convert format — the pack shader has the is_hdr
    // tone-map. Still 1:1 (NAT==WW), so a >1080p HDR desktop falls back to primary.
    // --fg-gpu primary + --convert-gpu primary would make F's flow AND C's convert both want A.q2 (only
    // ONE non-present submitter per queue handle is race-free; A has exactly 2 queues, P owns A.q).
    // Resolve in favour of FG: force convert back to the iGPU. This runs BEFORE the use_igpu_convert=
    // line so the iGPU path re-evaluates correctly (and the "unavailable -> primary" note still fires if
    // the iGPU/format conditions do not hold — in which case primary-FG is disabled later anyway).
    if(cfg.fg_gpu==FG_PRIMARY && cfg.convert_gpu==CG_PRIMARY){
        std::printf("[ra] --fg-gpu primary + --convert-gpu primary both target A.q2 (only one non-present submitter per queue) — forcing convert to iGPU; flow keeps A.q2.\n");
        cfg.convert_gpu=CG_IGPU;
    }
    use_igpu_convert=(cfg.convert_gpu==CG_IGPU&&have_igpu&&NAT_W==WW&&NAT_H==WH&&(route==RT_PASS||route==RT_HDR));
    if(cfg.convert_gpu==CG_IGPU&&!use_igpu_convert)
        std::printf("[ra] igpu-convert: unavailable (no iGPU or NAT!=WW) -> primary\n");
    if(use_igpu_convert)
        std::printf("[ra] igpu-convert: ACTIVE — iGPU fused convert+pack%s, B ingests packed\n",
            IS_HDR?" (HDR tone-map)":"");
    // WAP rides the surface path on A (the bridge owner) — always available.
    use_wap=cfg.warp_at_presenter;
    // --fwd-prestage: the prestage only has a copy to collapse on the iGPU-convert path (the only one
    // with the inline hRP_b[s]->hRP_b_dev[s] copy at the F-build top) AND only matters on the WAP path
    // (the serial WAP build is the one whose blocking flow submit fronts the copy). Force-OFF otherwise
    // so the flag is a clean no-op and the alloc/runtime/destroy all key on this single derived bool. The
    // serial-vs-pipeline split is decided at the per-pair use site (the fwd_pipeline path already overlaps
    // the same copy, so the prestage stands down there). Off → byte-identical (the copy stays inline).
    use_fwd_prestage = cfg.fwd_prestage && use_wap && use_igpu_convert;
    if(cfg.fwd_prestage && !use_fwd_prestage)
        std::printf("[ra] --fwd-prestage: requires --warp-at-presenter + the iGPU-convert ingest path (the only build with the inline hRP_b->dev copy) — disabled (no-op, byte-identical)\n");
    if(use_fwd_prestage)
        std::printf("[ra] --fwd-prestage: ACTIVE — the hRP_b->hRP_b_dev device copy splits to the prestage ping-pong (A.q2 no-wait, overlaps the flow record); cmdF keeps the hRP_b_dev barrier (same-queue cross-submission edge)\n");
    // Bidir is WAP-path only (the classification lives in A's warp). Without --warp-at-presenter there
    // is no MV field shipped to classify against, so --bidir without WAP is a no-op with a note.
    use_bidir=cfg.bidir&&use_wap;
    if(cfg.bidir&&!use_wap) std::printf("[ra] --bidir requires --warp-at-presenter (no WAP MV field to classify) — bidir disabled\n");
    // Fill-div directs the bidir neither-class sliver — alive only when bidir is (parse_args already
    // errored out on --fill-div without --bidir; this also covers --bidir disabled by no-WAP).
    use_fill_div=cfg.fill_div&&use_bidir;
    if(cfg.fill_div&&!use_bidir) std::printf("[ra] --fill-div needs an active bidir classification (disabled with bidir) — fill-div disabled\n");
    // Candidate-rescue runs in A's warp on the d_ab commit trigger — WAP-only (no commit path off WAP)
    // and only when commit is armed (parse_args already errored on --rescue without --commit-warp; this
    // also covers WAP disabled). The shader gate is rescue_on>0.5 AND commit_thresh>0.
    use_rescue=cfg.rescue&&use_wap&&cfg.commit_thresh>0.f;
    if(cfg.rescue&&!use_wap) std::printf("[ra] --rescue requires --warp-at-presenter (the commit/rescue path is the WAP warp) — rescue disabled\n");
    // The MV vector-median filters the WAP MV image(s) in wap_upload — WAP-only (no WAP MV image off
    // the WAP path). Independent of commit/bidir (it's a field-consensus prefilter); the bidir field
    // (wapMVBA) is median-filtered too when bidir is active.
    use_mv_median=cfg.mv_median&&use_wap;
    if(cfg.mv_median&&!use_wap) std::printf("[ra] --mv-median requires --warp-at-presenter (no WAP MV field to filter) — mv-median disabled\n");
    // Color-guided MV — WAP-only (the consensus pass filters the WAP MV image; the bilateral fetch is
    // in the WAP warp). parse_args already HARD-errored on --mv-guided without --warp-at-presenter, so
    // this is belt-and-suspenders for a non-flag WAP disable.
    use_mv_guided=cfg.mv_guided&&use_wap;
    if(cfg.mv_guided&&!use_wap) std::printf("[ra] --mv-guided requires --warp-at-presenter (no WAP MV field to guide) — mv-guided disabled\n");
    // The frame-holon (global affine fit + dissidence mask + model rescue + fill-div assist) is WAP-only
    // — the fit reads the shipped fwd MV grid and the model is consumed in A's warp. parse_args already
    // HARD-errored on --gme without --warp-at-presenter; this is belt-and-suspenders for a non-flag WAP
    // disable (e.g. WAP bridge/image alloc failure flipped use_wap off above).
    use_gme=cfg.gme&&use_wap;
    if(cfg.gme&&!use_wap) std::printf("[ra] --gme requires --warp-at-presenter (no WAP MV field to fit) — gme disabled\n");
    // The ambiguity channel — second-best candidate arbitration of SAD ties. Needs use_gme (the warp
    // referee is the gme background model gme_model_mv AND the dissidence masks gate the OBJECT
    // exemption; gme already requires WAP). Resolved HERE (before ofp.init) so emit_second_best can be
    // armed on B's pipeline. The cascades already cleared cfg.ambig with cfg.gme; this is the live gate.
    use_ambig=cfg.ambig&&use_gme;
    if(cfg.ambig&&!use_gme) std::printf("[ra] --ambig requires gme (the referee is the gme background model) — ambiguity disabled\n");
    // The inertia prior — F-side per-block persistence counter + the three WAP-warp motion-restriction
    // gates. WAP-only (the persistence rides the shipped MV grid; the gates live in A's warp). parse_args
    // already HARD-errored on --inertia without --warp-at-presenter; this is belt-and-suspenders for a
    // non-flag WAP disable (e.g. a WAP bridge/image alloc failure flipped use_wap off above).
    use_inertia=cfg.inertia&&use_wap;
    if(cfg.inertia&&!use_wap) std::printf("[ra] --inertia requires --warp-at-presenter (no WAP MV field to track) — inertia disabled\n");
    // The commit-default flip is a WARP-LEVEL rule — gated ONLY on use_wap, MATTE-INDEPENDENT (it floors
    // the warp-vs-blend selection for ANY low-confidence warp pixel, inside or outside a matte; the
    // fringe/sub-mask-text lives outside the silhouette). Belt-and-suspenders for a non-flag WAP disable
    // (a WAP image-alloc failure that flipped use_wap off after parse_args).
    use_commit_default=cfg.commit_default&&use_wap;
    if(cfg.commit_default&&!use_wap) std::printf("[ra] --commit-default requires --warp-at-presenter (no warp-vs-blend selection without WAP) — commit-default disabled\n");
    // The one-position collapse is a WARP-LEVEL rule — gated ONLY on use_wap, MATTE-INDEPENDENT (it
    // collapses the warp_result composition for ANY warp pixel whose two displaced samples disagree —
    // the nameplate TEXT, mv=0, lives outside any matte). Belt-and-suspenders for a non-flag WAP disable
    // (a WAP image-alloc failure that flipped use_wap off after parse_args).
    use_onepos=cfg.onepos&&use_wap;
    if(cfg.onepos&&!use_wap) std::printf("[ra] --onepos requires --warp-at-presenter (no warp_result composition without WAP) — one-position disabled\n");
    // The object-holon — F-side connected-component clustering of the gme dissidence mask, a 16-slot
    // temporal-identity table, and the conservative motion-inheritance repair. Needs use_gme (it clusters
    // hostDIS/hostDISB, the gme masks, and reuses the gme half-float decode). The HUD shield reads
    // persist[] (the inertia array) — only populated under use_inertia; when inertia is off the shield
    // simply does not fire (persist treated as 0), which is acceptable (a degenerate phase still fades by
    // the dissidence recompute, and the shield is an exemption, not a requirement). parse_args cascades
    // --no-gme/--no-wap onto objects; this is belt-and-suspenders for a non-flag gme disable (e.g. a WAP
    // image-alloc failure flipped use_wap → use_gme off above).
    use_objects=cfg.objects&&use_gme;
    if(cfg.objects&&!use_gme) std::printf("[ra] --objects requires --gme (no dissidence mask to cluster) — object-holon disabled\n");
    // --mv-guided SUPERSEDES --mv-median's blind pass. The same median compute pass runs (it reads
    // cur_real + a sim_thresh push), but in CONSENSUS mode (sim_thresh>0): color-membership-weighted
    // votes instead of the blind component-wise median. --mv-median alone (no guided) keeps sim_thresh=0
    // → the blind McGuire path. So the pass is enabled for EITHER flag; guided just changes the mode.
    if(use_mv_guided&&use_mv_median) std::printf("[ra] --mv-guided supersedes --mv-median (the consensus pass runs color-weighted, not blind)\n");
    // NOTE: the pipeline is CREATED when (use_mv_median||use_mv_guided) and the per-pair dispatch
    // re-checks the SAME live bools (see wap_upload). No named local is introduced here — the
    // goto done/fail pattern forbids crossing a non-trivial initializer with the jumps above.
    std::printf("[ra] A(primary): %-28s | B(FG): %s%s\n",A.name,B.name,
        use_igpu_convert?" | G(convert)":(!have_igpu&&!cfg.no_upscale?" (no iGPU)":""));
    std::printf("[ra] fg-gpu: %s (want-primary=%d)\n",
        cfg.fg_gpu==FG_AUTO?"auto":cfg.fg_gpu==FG_PRIMARY?"primary":"assist",
        (int)want_pfg);

    // ── Host bridge ───────────────────────────────────────────────────────────
    // Use one alignment granule that satisfies every active device so both
    // allocations can be imported by any combination of A / B / G.
    {
        const uint64_t al=std::max<uint64_t>({A.host_align,B.host_align,have_igpu?G.host_align:1u,1u});
        const VkDeviceSize wwork=VkDeviceSize(WW)*WH*4u;
        const VkDeviceSize hwork=(wwork+al-1)/al*al;
        // ── Auto-size the capture ring from the RUNTIME regime (NOT the CPU) ──────────────
        // The ring is RAM frame-slots; the depth must cover how many reals advance while B flows the pair
        // + P consumes it = the source/flow gap. Heuristic: ~1 slot per ~55 source-fps (the source ceiling
        // = --cap-fps, or ~125 default) + a base of 4, then clamped to [4, kCapSlots(max)] AND to a host-
        // memory budget (~768MB; each slot ~ W*H*4 * 2.7 for R + packed + field). --cap-slots N overrides
        // (0 = auto). Deliberately NOT a function of CPU cores/topology: that pillar (HardwareTopology /
        // Scheduler) governs THREAD-core placement, a separate concern; the ring depth is source/flow/res-
        // bound. (This is an init-time heuristic from the cap CEILING, not a measured per-run optimum; the
        // override + the frz/uniq stats are the tuning path.)
        {
            const int src_ceiling = (cfg.cap_fps>0) ? cfg.cap_fps : 125;
            int want = (cfg.cap_slots>0) ? cfg.cap_slots : (src_ceiling/10 + 4);
            const double slot_bytes = (double)WW*(double)WH*4.0*2.7;            // R + packed + field, approx
            const int mem_cap = (int)(768.0*1024.0*1024.0 / slot_bytes);
            if(want>mem_cap) want=mem_cap;
            if(want<kCapSlotsMin) want=kCapSlotsMin;                            // floor = the torn-read invariant (>= kIngestBacklog+1, static_asserted)
            if(want>kCapSlots) want=kCapSlots;                                  // the compile-time MAX (handle-array size)
            cap_slots = want;
            std::printf("[ra] capture-ring: %d slots (%s; source ceiling %d fps; ~%.0f MB) — auto-sized to the source/flow/res gap, NOT CPU topology\n",
                        cap_slots, cfg.cap_slots>0?"--cap-slots override":"auto", src_ceiling, (double)cap_slots*slot_bytes/1048576.0);
        }
        // kCapSlots real slots; each slot has independent R / RP host bridges.
        // hostR[s]: REAL frame slot s — A writes (hR_a[s]), B reads (hR_b[s]);
        // G reads/writes (hR_g[s]) when upscale / igpu_convert.
        {
            const VkBufferUsageFlags hRg_use=VK_BUFFER_USAGE_TRANSFER_SRC_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT
                                            |(use_igpu_convert?(VkBufferUsageFlags)VK_BUFFER_USAGE_STORAGE_BUFFER_BIT:0u);
            for(int _s=0;_s<cap_slots;++_s){
                hostR[_s]=_aligned_malloc((size_t)hwork,(size_t)al);
                if(!hostR[_s]||!hbuf_import(A,hostR[_s],hwork,hR_a[_s])||!hbuf_import(FD,hostR[_s],hwork,hR_b[_s])||   // FD-route (B→A under single_gpu) so the on-A copy-out writes a VALID hR_b handle (not a silent early-return)
                   ((use_upscale||use_igpu_convert)&&!hbuf_import(G,hostR[_s],hwork,hR_g[_s],hRg_use,/*q2_shared=*/use_igpu_convert)))
                    { std::printf("[ra] host bridge R[%d] failed\n",_s); goto done; }
            }
            // iGPU contour-field host bridge. G writes hFIELD_g via SSBO; the CPU reads hostFIELD for the
            // --igpu-field-verify byte gate. Size = hwork (W*H*4 = 1 uint/px). Needs the iGPU convert path
            // (the field reads hR_g, the convert's RGBA output).
            if(cfg.igpu_field && !use_igpu_convert){
                // No iGPU convert path → the field can't be produced, so the field AND EVERY consumer must
                // go off. Set igpu_field=false then RE-RESOLVE via the central apply_cascades (cli.cpp) so
                // afill/bg-snap/band-xfade/disoccl-hardpick cascade off AND cfg.d.field_to_warp is re-derived
                // in ONE place. announce=false: the cascade detail is suppressed; the one-line summary below
                // covers it.
                std::printf("[ra] --igpu-field ignored: needs the iGPU convert path (no iGPU or NAT!=WW); cascading off the field consumers (afill/bg-snap/band-xfade/disoccl-hardpick).\n");
                cfg.igpu_field=false;
                apply_cascades(cfg, /*announce=*/false);
            }
            if(cfg.igpu_field){
                for(int _s=0;_s<cap_slots;++_s){
                    hostFIELD[_s]=_aligned_malloc((size_t)hwork,(size_t)al);
                    if(!hostFIELD[_s]||!hbuf_import(G,hostFIELD[_s],hwork,hFIELD_g[_s],VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,/*q2_shared=*/true))
                        { std::printf("[ra] host bridge FIELD[%d] failed\n",_s); goto done; }
                    // A imports the SAME hostFIELD pointer as TRANSFER_SRC — A uploads the field (which G
                    // wrote) into wapFIELDA each pair; the fill OR the warp reads it (--afill OR --bg-snap).
                    if(cfg.d.field_to_warp && !hbuf_import(A,hostFIELD[_s],hwork,hFIELD_a[_s],VK_BUFFER_USAGE_TRANSFER_SRC_BIT))   // field_to_warp = ALL consumers (afill||bg_snap||band_xfade||disoccl_hardpick||mc_on), not just afill||bg_snap
                        { std::printf("[ra] host bridge FIELD_a[%d] failed\n",_s); goto done; }
                }
            }
        }
        // 2 interp generations × NI interps per generation.
        // hostI[g][k]: B writes (hI_b[g][k]); A reads (hI_a[g][k]) for the grid present (the only
        // present path — the surface bridge reads hostI). G reads (hI_g[g][k]) only on the (forced-off)
        // upscale path.
        {
            const int NI_alloc=std::min(std::max(1,cfg.fg_factor-1),kMaxInterp);  // belt vs overrun
            for(int _g=0;_g<kGenRing;++_g) for(int _k=0;_k<NI_alloc;++_k){
                hostI[_g][_k]=_aligned_malloc((size_t)hwork,(size_t)al);
                if(!hostI[_g][_k]||!hbuf_import(FD,hostI[_g][_k],hwork,hI_b[_g][_k])||   // FD-route hI_b (B→A under single_gpu); the warp copy-out targets it
                   (use_upscale ? !hbuf_import(G,hostI[_g][_k],hwork,hI_g[_g][_k])
                                : !hbuf_import(A,hostI[_g][_k],hwork,hI_a[_g][_k])))
                    { std::printf("[ra] host bridge I[%d][%d] failed\n",_g,_k); goto done; }
                if(want_pfg&&!hI_a[_g][_k].buf&&!hbuf_import(A,hostI[_g][_k],hwork,hI_a[_g][_k]))
                    { std::printf("[ra] host bridge I(pfg)[%d][%d] failed\n",_g,_k); goto done; }
            }
        }
        // hostG: UP_W×UP_H×4 — G writes upscale output (hGout); A reads for present (hApres).
        if(use_upscale){
            const VkDeviceSize wg=VkDeviceSize(UP_W)*UP_H*4u;
            const VkDeviceSize hg=(wg+al-1)/al*al;
            hostG=_aligned_malloc((size_t)hg,(size_t)al);
            if(!hostG||!hbuf_import(G,hostG,hg,hGout)||!hbuf_import(A,hostG,hg,hApres))
                { std::printf("[ra] host bridge G failed\n"); goto done; }
            pres_w=UP_W; pres_h=UP_H;
        }
        // hostRP[s] = packed RGB8 per real slot (× kCapSlots).
        if(use_igpu_convert){
            const VkDeviceSize wrp=VkDeviceSize(WW)*WH*3u;
            const VkDeviceSize hrp=(wrp+al-1)/al*al;
            for(int _s=0;_s<cap_slots;++_s){
                hostRP[_s]=_aligned_malloc((size_t)hrp,(size_t)al);
                if(!hostRP[_s]
                   ||!hbuf_import(G,hostRP[_s],hrp,hRP_g[_s],VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,/*q2_shared=*/true)
                   ||!hbuf_import(B,hostRP[_s],hrp,hRP_b[_s],VK_BUFFER_USAGE_TRANSFER_SRC_BIT)
                   ||!dbuf_create(B,wrp,VK_BUFFER_USAGE_TRANSFER_DST_BIT|VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,hRP_b_dev[_s]))
                    { std::printf("[ra] host bridge RP[%d] failed\n",_s); goto done; }
            }
        }
    }
    // Astage host buffer — aligned for EMH import on both A and G (same CPU pointer, importable as
    // SSBO on G).
    {
        const uint64_t al=std::max<uint64_t>({A.host_align,use_igpu_convert?G.host_align:1u,1u});
        const VkDeviceSize ab=VkDeviceSize(NAT_W)*NAT_H*nat_bpp;
        const VkDeviceSize hab=(ab+al-1)/al*al;
        hostA=_aligned_malloc((size_t)hab,(size_t)al);
        // hab (alignment-rounded), NOT ab: EMH vkAllocateMemory needs the size to be a multiple of
        // minImportedHostPointerAlignment. 1920x1080x4 happens to be 4096-aligned; odd window sizes
        // (1264x681) are not. Every hbuf_import site rounds the same way.
        if(!hostA
           ||!hbuf_import(A,hostA,hab,Astage,VK_BUFFER_USAGE_TRANSFER_SRC_BIT)
           ||(use_igpu_convert&&!hbuf_import(G,hostA,hab,Astage_g,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)))
            { std::printf("[ra] Astage import failed\n"); goto done; }
        // ── --ingest-async: the RAW host-buffer ring + the 2nd staging texture ─────────
        // DDA-only: WGC ingests via its own callback ring (the convert is already off the acquire path),
        // so force the flag off for WGC (byte-identical). On any raw-ring alloc failure we free what we
        // got and fall back to serial (never abort an opt-in perf flag).
        if(cfg.ingest_async && cfg.capture_api!=CA_DD){
            std::printf("[ra] --ingest-async: DDA-only — capture-api is WGC; forcing ingest-async OFF (serial path unchanged)\n");
            cfg.ingest_async=false;
        }
        if(cfg.ingest_async){
            bool ok=true;
            for(int _k=0;_k<kRawSlots && ok;++_k){
                raw_host[_k]=_aligned_malloc((size_t)hab,(size_t)al);   // same hab/al as Astage → same importability on A+G
                ok = raw_host[_k]
                   && hbuf_import(A,raw_host[_k],hab,raw_astage_a[_k],VK_BUFFER_USAGE_TRANSFER_SRC_BIT)
                   && (!use_igpu_convert || hbuf_import(G,raw_host[_k],hab,raw_astage_g[_k],VK_BUFFER_USAGE_STORAGE_BUFFER_BIT));
            }
            // The readback double-buffer: a 2nd DDA staging texture (slot-1; dxgi_stage is slot-0).
            if(ok){ dxgi_stage2=d3d_staging(d,NAT_W,NAT_H); ok=(dxgi_stage2!=nullptr); }
            if(!ok){
                std::printf("[ra] --ingest-async: raw-ring alloc failed — falling back to the serial capture path\n");
                cfg.ingest_async=false;
                for(int _k=0;_k<kRawSlots;++_k){ hbuf_destroy(A,raw_astage_a[_k]); if(use_igpu_convert) hbuf_destroy(G,raw_astage_g[_k]); if(raw_host[_k]){ _aligned_free(raw_host[_k]); raw_host[_k]=nullptr; } }
                if(dxgi_stage2){ rel(dxgi_stage2); dxgi_stage2=nullptr; }
            } else {
                std::printf("[ra] --ingest-async: ARMED — %d-slot raw ring + readback double-buffer; convert worker thread will spawn\n",kRawSlots);
            }
        }
    }
    // Warp-at-presenter MV+SAD host bridges (per generation). The MV/SAD grid is (WW/8)×(WH/8) RG16F
    // (=4 bytes/texel; matches OFP's kBlockSize=8). F copies B's MV+SAD images into hMV_b/hSAD_b
    // (TRANSFER_DST); the presenter uploads them into its sampled images from hMV_*/hSAD_* (TRANSFER_SRC).
    // hbuf_import's default usage (TRANSFER_SRC|DST) covers both. The presenter is A (the bridge owner),
    // so the upload-side import is on A (hMV_a/hSAD_a) — taken with the SAME align-rounded size hfb (EMH
    // vkAllocateMemory needs size %% minImportedHostPointerAlignment == 0).
    if(use_wap){
        const uint64_t al=std::max<uint64_t>({B.host_align,A.host_align,1u});
        // This is the ONLY MV-grid site that runs BEFORE ofp.init (the host bridges are allocated here),
        // so it cannot read ofp.motion_width() yet. It derives the grid from WW_flow/WH_flow — THE inputs
        // about to be passed to ofp.init — so it equals ofp.motion_width() by construction
        // (create_mv_image: mv_w_=(width+7)/8). The post-init sites below read the accessor directly; a
        // runtime assert after init confirms the two never diverge. At flow_div==1, WW_flow==WW → (WW+7)/8.
        const uint32_t mvw=(WW_flow+7u)/8u, mvh=(WH_flow+7u)/8u;
        const VkDeviceSize fb=VkDeviceSize(mvw)*mvh*4u;     // RG16F
        const VkDeviceSize hfb=(fb+al-1)/al*al;
        for(int _g=0;_g<kGenRing;++_g){
            hostMV[_g]=_aligned_malloc((size_t)hfb,(size_t)al);
            hostSAD[_g]=_aligned_malloc((size_t)hfb,(size_t)al);
            // hfb (alignment-rounded), NOT fb: the EMH import's vkAllocateMemory requires the
            // size to be a multiple of minImportedHostPointerAlignment (the gate-found failure).
            bool ok = hostMV[_g] && hostSAD[_g]
                && hbuf_import(FD,hostMV[_g],hfb,hMV_b[_g]) && hbuf_import(FD,hostSAD[_g],hfb,hSAD_b[_g])   // FD-route the write-side MV/SAD bridges (B→A under single_gpu); the on-A copy-out writes them, the CPU gme fit + A warp read the same host pointer
                && hbuf_import(A,hostMV[_g],hfb,hMV_a[_g]) && hbuf_import(A,hostSAD[_g],hfb,hSAD_a[_g]);
            if(ok && use_bidir){
                // The backward-MV bridge — same RG16F dims/hfb discipline. Imported on B (write-side,
                // TRANSFER_DST for the copy-out) and A (read-side, TRANSFER_SRC upload).
                hostMVB[_g]=_aligned_malloc((size_t)hfb,(size_t)al);
                ok = hostMVB[_g]
                  && hbuf_import(FD,hostMVB[_g],hfb,hMVB_b[_g])   // FD-route so single_gpu keeps bidir ON (not silently disabled by a null-B early-return)
                  && hbuf_import(A,hostMVB[_g],hfb,hMVB_a[_g]);
                if(!ok){ std::printf("[ra] WAP MV_bwd bridge[%d] failed — disabling bidir\n",_g); use_bidir=false; ok=true; }
            }
            // The second-best CANDIDATE bridge — RGBA16F (8 bytes/texel, NOT 4), so its OWN
            // align-rounded size cfb (mvw×mvh×8 rounded UP to al, the same EMH discipline hostMV uses —
            // POINTER aligned to al via _aligned_malloc AND SIZE a multiple of al via the round). Imported
            // on B (write-side, TRANSFER_DST for the FWD-pass copy-out from cand_image()) AND A (read-side,
            // TRANSFER_SRC upload into wapC2A). Only allocated under use_ambig (cfg.ambig && use_gme). On
            // failure we clear use_ambig (the warp falls to byte-identical off — push 0, placeholder binding)
            // and keep WAP alive (the candidate is additive). cand_image() was already created by ofp.init
            // (emit_second_best=use_ambig); if use_ambig were off here the whole block is skipped.
            if(ok && use_ambig){
                const VkDeviceSize cfb_raw=VkDeviceSize(mvw)*mvh*8u;   // RGBA16F: 8 bytes/texel
                const VkDeviceSize cfb=(cfb_raw+al-1)/al*al;
                hostC2[_g]=_aligned_malloc((size_t)cfb,(size_t)al);
                bool okc = hostC2[_g]
                  && hbuf_import(FD,hostC2[_g],cfb,hC2_b[_g])   // FD-route so single_gpu keeps ambiguity ON
                  && hbuf_import(A,hostC2[_g],cfb,hC2_a[_g]);
                if(!okc){ std::printf("[ra] WAP candidate bridge[%d] failed — disabling ambiguity\n",_g); use_ambig=false; }
            }
            // The dissidence mask bridge — R8 (1 byte/texel), so its own align-rounded size dfb (NOT hfb,
            // which is the RG16F 4-byte size). F fills hostDIS[_g] by CPU memcpy after the fit; A imports
            // it (read-side, TRANSFER_SRC upload into wapDISA). No B-side import (F writes it on the CPU).
            // Disabling gme on failure keeps WAP alive (the mask is additive).
            if(ok && cfg.gme){
                const VkDeviceSize db=VkDeviceSize(mvw)*mvh;       // R8: 1 byte/texel
                const VkDeviceSize dfb=(db+al-1)/al*al;
                hostDIS[_g]=_aligned_malloc((size_t)dfb,(size_t)al);
                bool okd = hostDIS[_g] && hbuf_import(A,hostDIS[_g],dfb,hDIS_a[_g]);
                // The backward (cur-anchored) dissidence bridge — SAME R8 dfb discipline as hostDIS, same
                // A-side-only import (F writes it on the CPU after the bwd fit, A uploads into wapDISBA).
                // Allocated whenever gme is on AND bidir is active (the bwd field exists). The --matte gate
                // already requires --bidir, so for the matte case this is always taken.
                if(okd && use_bidir){
                    hostDISB[_g]=_aligned_malloc((size_t)dfb,(size_t)al);
                    okd = hostDISB[_g] && hbuf_import(A,hostDISB[_g],dfb,hDISB_a[_g]);
                }
                // ── gme-gpu: the B-side of the dissidence bridges + the model readback. The GPU dissidence
                // pass WRITES the mask into hostDIS (atomicOr) — so it needs a B import as a STORAGE buffer
                // (the dissidence set's binding-2 target). And the 6-float model SSBO is copied into a
                // host-coherent readback buffer (hostGmeM[_g]) that F reads in consume_wap. Only allocated
                // when cfg.gme_gpu; any failure clears cfg.gme_gpu (falls back to the CPU gme_fit_affine —
                // the dis bridges + A-side imports are identical either way). hostDIS is imported on A above
                // (read-side); here we ADD the B write-side + bwd + model. Under single_gpu the GPU gme pass
                // is forced OFF at the B.dev==null guard (→ CPU gme_fit_affine, which reads/writes the host
                // pointers hostDIS/hostMV directly — no device handle needed). So SKIP these imports under
                // single_gpu (gating them, NOT routing dead-but-valid aliases): cfg.gme_gpu stays set here
                // and the guard routes to CPU with its own LOUD "device B unavailable -> CPU gme fallback"
                // message. gme itself (the CPU fit) stays ON (the stat prints gme(dis:NN%)).
                if(okd && cfg.gme_gpu && !single_gpu){
                    // STORAGE (the dissidence atomicOr target) + TRANSFER_DST (the per-pass vkCmdFillBuffer
                    // 0-clear before the dissidence dispatch — non-dissident blocks stay 0, matching the CPU).
                    const VkBufferUsageFlags du=VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT;
                    bool okg = hbuf_import(B,hostDIS[_g],dfb,hDIS_b[_g],du);
                    if(okg && use_bidir) okg = hbuf_import(B,hostDISB[_g],dfb,hDISB_b[_g],du);
                    if(okg){
                        const VkDeviceSize mb_raw=6u*sizeof(float);
                        const VkDeviceSize mb=(mb_raw+al-1)/al*al;
                        hostGmeM[_g]=_aligned_malloc((size_t)mb,(size_t)al);
                        okg = hostGmeM[_g] && hbuf_import(B,hostGmeM[_g],mb,hGmeM_b[_g],VK_BUFFER_USAGE_TRANSFER_DST_BIT);
                        if(okg && use_bidir){
                            hostGmeMB[_g]=_aligned_malloc((size_t)mb,(size_t)al);
                            okg = hostGmeMB[_g] && hbuf_import(B,hostGmeMB[_g],mb,hGmeMB_b[_g],VK_BUFFER_USAGE_TRANSFER_DST_BIT);
                        }
                    }
                    if(!okg){ std::printf("[ra] WAP gme-gpu bridge[%d] failed — falling back to CPU gme\n",_g); cfg.gme_gpu=false; }
                }
                if(!okd){ std::printf("[ra] WAP DIS bridge[%d] failed — disabling gme\n",_g); cfg.gme=false; }
            }
            // The inertia persistence bridge — SAME R8 dfb discipline as hostDIS (1 byte/MV-block), SAME
            // A-side-only import (F memcpy's its continuous persist[] into hostPER[_g], A uploads into
            // wapPERA). Independent of gme — gated only on cfg.inertia. On failure we clear cfg.inertia (the
            // gates fall to byte-identical-off) and keep WAP alive (the prior is additive).
            if(ok && cfg.inertia){
                const VkDeviceSize db=VkDeviceSize(mvw)*mvh;       // R8: 1 byte/MV-block
                const VkDeviceSize dfb=(db+al-1)/al*al;
                hostPER[_g]=_aligned_malloc((size_t)dfb,(size_t)al);
                bool okp = hostPER[_g] && hbuf_import(A,hostPER[_g],dfb,hPER_a[_g]);
                if(!okp){ std::printf("[ra] WAP PER bridge[%d] failed — disabling inertia\n",_g); cfg.inertia=false; }
            }
            if(!ok){ std::printf("[ra] WAP MV/SAD bridge[%d] failed — disabling warp-at-presenter\n",_g); use_wap=false; break; }
        }
    }

    // ── Images ────────────────────────────────────────────────────────────────
    // Astage is now hostA (aligned _aligned_malloc) imported via hbuf_import above.
    // dxgi_stage is still needed for D3D11 staging texture; Astage.buf already created.
    dxgi_stage=d3d_staging(d,NAT_W,NAT_H);
    // Bframe needs STORAGE_BIT for packed unpack output (compute write).
    // Gsrc needs STORAGE_BIT for iGPU real-frame unpack (compute write before upscale).
    // Assigned (not declared-const) here: earlier goto-done error paths would otherwise
    // jump across the initialization — gcc rejects that; the vars live with the top block.
    bframe_use=VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT
               |(use_igpu_convert?(VkImageUsageFlags)VK_IMAGE_USAGE_STORAGE_BIT:0u)
               |(cfg.nvofa?(VkImageUsageFlags)VK_IMAGE_USAGE_TRANSFER_SRC_BIT:0u)   // the OFA blits Bframe -> its downscaled input, so Bframe needs TRANSFER_SRC (flow_div==1 doesn't add it)
               // At flow_div>1 the F thread blits Bframe → Bflow (downscale) before the flow, so Bframe
               // must be a valid blit SOURCE. Added ONLY when downscaling → bytes unchanged at N=1.
               |((flow_div>1u)?(VkImageUsageFlags)VK_IMAGE_USAGE_TRANSFER_SRC_BIT:0u);
    gsrc_use  =VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT
               |(use_igpu_convert?(VkImageUsageFlags)VK_IMAGE_USAGE_STORAGE_BIT:0u);
    if(!dxgi_stage||
       !img_create(A,NAT_W,NAT_H,nat_vkfmt,VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,Anative)||
       !img_create(A,WW,WH,VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,Awork)||
       // --fps-overlay: add STORAGE + a view ONLY when the overlay is enabled, so the overlay compute
       // pass can RMW Apresent (rgba8). DEFAULT OFF → created as TRANSFER_DST|TRANSFER_SRC, no view →
       // byte-identical-off.
       !img_create(A,pres_w,pres_h,VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|(cfg.fps_overlay?(VkImageUsageFlags)VK_IMAGE_USAGE_STORAGE_BIT:0u),Apresent,/*want_view=*/cfg.fps_overlay)||
       !img_create(FD,WW,WH,VK_FORMAT_R8G8B8A8_UNORM,bframe_use,Bframe[0])||   // FD-route Bframe/Cinterp/Bflow (B→A under single_gpu) — the flow producer's frame buffers
       !img_create(FD,WW,WH,VK_FORMAT_R8G8B8A8_UNORM,bframe_use,Bframe[1])||
       !img_create(FD,WW,WH,VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT,Cinterp)||
       // The two flow-input downscale targets (WW_flow×WH_flow). SAMPLED (OFP reads them) +
       // TRANSFER_DST (blit dest). Only when downscaling — zero extra VRAM at flow_div==1.
       (flow_div>1u&&!img_create(FD,WW_flow,WH_flow,VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,Bflow[0]))||
       (flow_div>1u&&!img_create(FD,WW_flow,WH_flow,VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,Bflow[1]))||
       ((use_upscale||use_igpu_convert)&&!img_create(G,WW,WH,VK_FORMAT_R8G8B8A8_UNORM,gsrc_use,Gsrc))||
       (use_upscale&&!img_create(G,UP_W,UP_H,VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT,Gdst)))
        { std::printf("[ra] image allocation failed\n"); goto done; }

    oneshot(FD,[&](VkCommandBuffer c){ for(int i=0;i<2;++i) img_barrier(c,Bframe[i].img,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,0,VK_ACCESS_SHADER_READ_BIT); img_barrier(c,Cinterp.img,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_GENERAL,0,VK_ACCESS_SHADER_WRITE_BIT); });   // FD-route the layout-seed oneshot (uses FD.q/FD.pool internally)
    // Seed the downscale scratch to SHADER_READ_ONLY_OPTIMAL so the first per-pair blit's
    // RO→TRANSFER_DST barrier (in the F record sites) is a valid transition (matches Bframe's seed layout).
    if(flow_div>1u) oneshot(FD,[&](VkCommandBuffer c){ for(int i=0;i<2;++i) img_barrier(c,Bflow[i].img,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,0,VK_ACCESS_SHADER_READ_BIT); });   // FD-route
    // B-VRAM staging buffers for the overlapped interp copy-outs. Only when B.q2 is a real second
    // queue (split-family/split-queue); shared fallback keeps serial.
    b_q2_split = (B.q2!=B.q);
    if(b_q2_split){
        const VkDeviceSize ibytes=VkDeviceSize(WW)*WH*4u;   // R8G8B8A8 interp frame ≈ 8.3MB
        for(int _k=0;_k<kMaxInterp;++_k){
            if(!dbuf_create(B,ibytes,VK_BUFFER_USAGE_TRANSFER_DST_BIT|VK_BUFFER_USAGE_TRANSFER_SRC_BIT,sbI[_k]))
                { std::printf("[ra] sbI[%d] alloc failed — reverting to serial copy-out\n",_k); b_q2_split=false; break; }
        }
    }
    if(use_upscale) oneshot(G,[&](VkCommandBuffer c){ img_barrier(c,Gdst.img,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_GENERAL,0,VK_ACCESS_SHADER_WRITE_BIT); });
    // Primary-FG images on A.
    if(want_pfg){
        if(!img_create(A,WW,WH,VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,AframeA[0])||
           !img_create(A,WW,WH,VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,AframeA[1])||
           !img_create(A,WW,WH,VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT,CinterpA))
            { std::printf("[ra] pfg image allocation failed — primary-FG disabled\n"); }
        else oneshot(A,[&](VkCommandBuffer c){
            for(int i=0;i<2;++i) img_barrier(c,AframeA[i].img,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,0,VK_ACCESS_SHADER_READ_BIT);
            img_barrier(c,CinterpA.img,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_GENERAL,0,VK_ACCESS_SHADER_WRITE_BIT);
        });
    }

    // ── WGC backend init ─────────────────────────────────────────────────────
#ifdef _MSC_VER
    if(cfg.capture_api==CA_WGC){
        // Pre-flight WGC-availability probe. GraphicsCaptureSession::IsSupported() is false on builds <
        // 1803 (and in some locked-down policy states). There is no auto-descent ladder, so on an
        // unsupported OS we surface the NAMED reason and exit cleanly via the `goto done` path instead of
        // throwing deep in WGC init. Best-effort: any projection/throw here is treated as "could not
        // confirm support" and also surfaced (never silent).
        {
            bool wgc_supported=false;
            try { wgc_supported = wgc::GraphicsCaptureSession::IsSupported(); } catch(...) { wgc_supported=false; }
            if(!wgc_supported){
                ra::compat::emit(ra::compat::ReasonCode::WGC_UNSUPPORTED_OS);
                goto done;   // clean cold-path exit (no descent ladder)
            }
        }
        // --copy-device: optionally create a SEPARATE D3D11 device on the SAME adapter as d.dev for the
        // WGC copy queue. cap_dev/cap_ctx alias d.dev/d.ctx when OFF (every WGC-path device reference below
        // is then the d.dev/d.ctx code → byte-identical-off). When ON, they point at the 2nd device so the
        // CopyResource runs on its OWN command queue, decoupled from the game's saturated d.dev queue.
        // SAME-adapter is REQUIRED (a cross-adapter copy of the WGC surface would need shared-resource
        // keyed-mutex machinery — out of scope, crash-class); we create on d.luid's adapter.
        ID3D11Device*        cap_dev=d.dev;
        ID3D11DeviceContext* cap_ctx=d.ctx;
        if(cfg.copy_device){
            IDXGIFactory1* _cfac=nullptr; IDXGIAdapter1* _cad=nullptr;
            if(SUCCEEDED(CreateDXGIFactory1(__uuidof(IDXGIFactory1),(void**)&_cfac)) && _cfac){
                for(UINT _i=0;_cfac->EnumAdapters1(_i,&_cad)==S_OK;++_i){
                    DXGI_ADAPTER_DESC1 _ad{}; _cad->GetDesc1(&_ad);
                    if(_ad.AdapterLuid.LowPart==d.luid.LowPart && _ad.AdapterLuid.HighPart==d.luid.HighPart) break;
                    rel(_cad); _cad=nullptr;
                }
            }
            ID3D11Device* _cdev=nullptr; ID3D11DeviceContext* _cctx=nullptr; D3D_FEATURE_LEVEL _cfl{};
            // DRIVER_TYPE_UNKNOWN when an explicit adapter is passed (D3D11CreateDevice contract); BGRA_SUPPORT
            // to match d3d_init (WGC delivers BGRA8). Same-adapter guarantees CopyResource(ring,tex) is intra-device.
            HRESULT _chr = D3D11CreateDevice(_cad, _cad?D3D_DRIVER_TYPE_UNKNOWN:D3D_DRIVER_TYPE_HARDWARE,
                nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION, &_cdev, &_cfl, &_cctx);
            rel(_cad); rel(_cfac);
            if(SUCCEEDED(_chr) && _cdev && _cctx){
                // Enable D3D11Multithread so the FrameArrived callback (CopyResource/Signal) and the
                // C-thread (Map/Unmap) can share this immediate context safely.
                { ID3D11Multithread* _mt=nullptr; if(SUCCEEDED(_cctx->QueryInterface(__uuidof(ID3D11Multithread),(void**)&_mt))&&_mt){ _mt->SetMultithreadProtected(TRUE); _mt->Release(); } }
                cap_dev=_cdev; cap_ctx=_cctx; // recorded on wgc_ctx (cdev/cctx) after the new WgcCtx() below

                std::printf("[ra] --copy-device: separate D3D11 device ARMED on the capture adapter — the WGC CopyResource queue is decoupled from d.dev.\n");
            } else {
                if(_cctx){ _cctx->Release(); } if(_cdev){ _cdev->Release(); }
                cfg.copy_device=false;   // FORCED OFF → cap_dev/cap_ctx stay d.dev/d.ctx → byte-identical
                std::printf("[ra] --copy-device: 2nd D3D11 device on the capture adapter unavailable (hr=0x%08lX) — FORCED OFF, the copy runs on d.dev (byte-identical).\n",(unsigned long)_chr);
            }
        }
        winrt::com_ptr<IDXGIDevice> dxgi_dev_wgc;
        cap_dev->QueryInterface(dxgi_dev_wgc.put());
        winrt::com_ptr<IInspectable> d3d_insp;
        winrt::check_hresult(CreateDirect3D11DeviceFromDXGIDevice(dxgi_dev_wgc.get(),d3d_insp.put()));
        auto d3d_winrt=d3d_insp.as<wd3d::IDirect3DDevice>();

        auto interop=winrt::get_activation_factory<wgc::GraphicsCaptureItem,IGraphicsCaptureItemInterop>();
        wgc::GraphicsCaptureItem cap_item{nullptr};
        // CreateForWindow/CreateForMonitor throw winrt::hresult_error on failure (e.g. a window destroyed
        // between match and create, a monitor that vanished, or an OS-policy refusal). main() has no
        // top-level catch, so an escape here would abort with a generic crash — a silent failure with NO
        // named reason. Catch it on this cold path, surface CAPTURE_INIT_FAILED, and exit cleanly via the
        // `goto done` cleanup (forward jump out of this scope runs the locals' destructors).
        try {
            if(cfg.window_substr[0]){
                winrt::check_hresult(interop->CreateForWindow(wgc_target_hwnd,
                    winrt::guid_of<wgc::GraphicsCaptureItem>(),winrt::put_abi(cap_item)));
                std::printf("[ra] WGC: capturing window '%s'\n",cfg.window_substr);
                if(cfg.dpi_probe){ auto _sz=cap_item.Size(); std::printf("[ra] --dpi-probe: cap_item.Size()=%dx%d (WGC's OWN physical window size — the DPI-independent source of truth)\n",_sz.Width,_sz.Height); }
            } else {
                winrt::check_hresult(interop->CreateForMonitor(d.cap_hmon,
                    winrt::guid_of<wgc::GraphicsCaptureItem>(),winrt::put_abi(cap_item)));
                std::printf("[ra] WGC: capturing monitor %d\n",cfg.cap_mon);
            }
        } catch(const winrt::hresult_error& e){
            std::printf("[ra] WGC GraphicsCaptureItem create FAILED (hr=0x%08lX)\n",(unsigned long)e.code().value);
            ra::compat::emit(ra::compat::ReasonCode::CAPTURE_INIT_FAILED);
            goto done;
        }

        wgc_ctx=new WgcCtx();
        // --copy-device: record the 2nd device on the ctx (null when off). cap_dev/cap_ctx already alias
        // d.dev/d.ctx when off → the ring-alloc + teardown below are byte-identical in the off case.
        if(cfg.copy_device){ wgc_ctx->cdev=cap_dev; wgc_ctx->cctx=cap_ctx; }
        // Fill the staging ring; ring[0] reuses the already-created dxgi_stage (off path). With --copy-device
        // ON the ring lives on the 2nd device (CopyResource must be intra-device with the WGC surface that
        // device owns), so ALL slots — including ring[0] — are fresh d3d_staging_on(cap_dev) textures;
        // dxgi_stage (on d.dev) stays allocated but unused by WGC (released at teardown). The off path is
        // unchanged.
        if(cfg.copy_device){
            for(uint32_t _i=0;_i<WgcCtx::RING_N;++_i) wgc_ctx->ring[_i]=d3d_staging_on(cap_dev,d.fmt,NAT_W,NAT_H);
        } else {
            wgc_ctx->ring[0]=dxgi_stage;
            for(uint32_t _i=1;_i<WgcCtx::RING_N;++_i) wgc_ctx->ring[_i]=d3d_staging(d,NAT_W,NAT_H);
        }
        winrt::Windows::Graphics::SizeInt32 pool_sz{(int32_t)NAT_W,(int32_t)NAT_H};
        // bufferCount 6. Under a saturated 4090, WGC's GPU copy takes ~25 ms; with only 2 pool slots and
        // frames every ~11 ms, slot #3 finds the pool full → WGC drops it silently (FrameArrived never
        // fires) → arr ceiling ~57/90 fps. 6 slots absorb up to ~66 ms copy backlog before any WGC-level
        // drop — lifting the ceiling to the source fps. (When that still binds, --copy-device gives the
        // CopyResource its own queue, separate from WGC's internal copies.)
        wgc_ctx->pool=wgc::Direct3D11CaptureFramePool::CreateFreeThreaded(
            d3d_winrt,wgdx::DirectXPixelFormat::B8G8R8A8UIntNormalized,6,pool_sz);

        // --copy-fence init: the D3D11.4 capability probe. Query ID3D11Device5 (CreateFence) +
        // ID3D11DeviceContext4 (Signal) + an auto-reset event from the CAPTURE device/context (cap_dev/cap_ctx
        // — == d.dev/d.ctx when --copy-device off, the 2nd device when on; the fence MUST be created+signaled
        // on the SAME device that issues the CopyResource). If ANY step fails → release partials and FORCE
        // cfg.copy_fence=false → the C-thread + callback both take the byte-identical Map-retry path.
        if(cfg.copy_fence){
            bool cf_ok=false;
            ID3D11Device5* dev5=nullptr;
            if(SUCCEEDED(cap_dev->QueryInterface(__uuidof(ID3D11Device5),(void**)&dev5)) && dev5){
                if(SUCCEEDED(dev5->CreateFence(0,D3D11_FENCE_FLAG_NONE,__uuidof(ID3D11Fence),(void**)&wgc_ctx->copyFence)) && wgc_ctx->copyFence){
                    if(SUCCEEDED(cap_ctx->QueryInterface(__uuidof(ID3D11DeviceContext4),(void**)&wgc_ctx->ctx4)) && wgc_ctx->ctx4){
                        wgc_ctx->copyEvt=CreateEventEx(nullptr,nullptr,0,EVENT_ALL_ACCESS); // auto-reset (no MANUAL_RESET flag)
                        if(wgc_ctx->copyEvt) cf_ok=true;
                    }
                }
            }
            dev5 && (dev5->Release(),true);   // release the temporary dev5 query ref (the fence keeps the device alive)
            if(cf_ok){
                std::printf("[ra] --copy-fence: event-driven WGC copy-completion pickup ARMED (D3D11.4 fence+event).\n");
            } else {
                // Fallback: release whatever was created, force OFF → byte-identical Map-retry path.
                if(wgc_ctx->copyEvt){ CloseHandle(wgc_ctx->copyEvt); wgc_ctx->copyEvt=nullptr; }
                if(wgc_ctx->ctx4){ wgc_ctx->ctx4->Release(); wgc_ctx->ctx4=nullptr; }
                if(wgc_ctx->copyFence){ wgc_ctx->copyFence->Release(); wgc_ctx->copyFence=nullptr; }
                cfg.copy_fence=false;
                std::printf("[ra] --copy-fence: D3D11.4 (ID3D11Device5/Context4) or event unavailable — FORCED OFF, the Map-retry busy-poll path runs (byte-identical).\n");
            }
        }

        // FrameArrived callback — CopyResource to next ring slot only. Map/memcpy run in the main loop;
        // D3D11Multithread (d3d_init) makes both sides safe. Captures pointers by value to survive teardown
        // ordering.
        // --copy-device: raw_ctx is the CAPTURE context — cap_ctx (== d.ctx when off, the 2nd device when
        // on). The callback's CopyResource + the C-thread Map (below) both go through the SAME device, so the
        // 2nd device's copy queue is self-consistent. When off this is exactly d.ctx → byte-identical callback.
        ID3D11DeviceContext* raw_ctx=cap_ctx;
        WgcCtx* raw_wctx=wgc_ctx;
        const bool lt_on=cfg.latency_trace;   // captured once → byte-identical callback when off
        const bool cf_on=cfg.copy_fence;      // captured once (post-probe) → zero work in the callback when off
        const bool dp_on=cfg.dpi_probe;       // --dpi-probe: one-shot first-frame ContentSize log (default off → dead in the callback → byte-identical)
        wgc_ctx->pool.FrameArrived([raw_wctx,raw_ctx,lt_on,cf_on,dp_on](auto& p,auto&){
            if(!raw_wctx->running.load()) return;
            auto frame=p.TryGetNextFrame(); if(!frame) return;
            if(dp_on){ static bool _dp_once=false; if(!_dp_once){ _dp_once=true; auto _cs=frame.ContentSize(); std::printf("[ra] --dpi-probe: first frame.ContentSize()=%dx%d (what WGC ACTUALLY delivers — the non-circular truth vs the pool size)\n",_cs.Width,_cs.Height); } }
            auto surface=frame.Surface();
            auto acc=surface.try_as<IDirect3DDxgiInterfaceAccess>(); if(!acc) return;
            winrt::com_ptr<ID3D11Texture2D> tex;
            acc->GetInterface(IID_PPV_ARGS(tex.put())); if(!tex) return;
            // Plain seq_cst atomics throughout (≤240 ops/s — ordering cost is nil; explicit
            // memory_order_* lives in framework/hal/ only, per the lint_hal rule).
            const uint32_t w=raw_wctx->ring_write.load();
            const uint32_t r=raw_wctx->ring_read.load();
            if(w-r>=WgcCtx::RING_N){ ++raw_wctx->arrived; return; }  // ring full; drop frame
            raw_ctx->CopyResource(raw_wctx->ring[w%WgcCtx::RING_N],tex.get());
            ++raw_wctx->ring_write;
            // --copy-fence: enqueue a GPU-timeline Signal AFTER the CopyResource for slot w%N. The value is
            // w+1 (== ring_write post-increment) so the C-thread, seeing ring_write==W, can wait fence>=W for
            // the newest slot (W-1)%N whose copy was signaled w+1==W. One fire-and-return enqueue on the
            // already-multithread-protected context (≤125/s) — NEVER blocks the winrt thread.
            if(cf_on && raw_wctx->ctx4 && raw_wctx->copyFence)
                raw_wctx->ctx4->Signal(raw_wctx->copyFence,(UINT64)(w+1u));
            ++raw_wctx->arrived;
            // --latency-trace: stamp the WGC-ring slot just written (w%RING_N). submit = steady now (C
            // computes copy-exec = tcap−submit); compose = the QPC delta WGC-compose→this-callback (both
            // QPC → clock-safe). All in DOUBLE ms (no int overflow / no __int128). If the QPC↔
            // SystemRelativeTime epochs do NOT match, the delta is out-of-band → stored 0 (= invalid, the
            // stats then show compose=0, flagging the assumption failed). Off (lt_on=false) → zero work.
            if(lt_on){
                raw_wctx->ring_submit_us[w%WgcCtx::RING_N].store((uint64_t)(now_ms()*1000.0));
                static LARGE_INTEGER s_qf=[]{ LARGE_INTEGER f{}; QueryPerformanceFrequency(&f); return f; }();
                LARGE_INTEGER qpc{}; QueryPerformanceCounter(&qpc);
                const double qpc_ms = s_qf.QuadPart>0 ? (double)qpc.QuadPart*1000.0/(double)s_qf.QuadPart : 0.0;
                const double srt_ms = (double)frame.SystemRelativeTime().count()/10000.0;   // 100ns → ms
                const double dcm = qpc_ms - srt_ms;
                raw_wctx->ring_compose_us[w%WgcCtx::RING_N].store((dcm>0.0&&dcm<200.0)?(uint64_t)(dcm*1000.0):0);
            }
            { const uint64_t t_us=(uint64_t)(now_ms()*1000.0);
              const uint64_t prev=raw_wctx->last_arr_us.exchange(t_us);
              if(prev&&t_us>prev) raw_wctx->arr_delta_us.store(t_us-prev); }
            raw_wctx->frame_ready.store(true);
        });

        wgc_ctx->session=wgc_ctx->pool.CreateCaptureSession(cap_item);
        try { wgc_ctx->session.IsBorderRequired(false); } catch(...) {}  // Win11 22621+ only
        // Do NOT bake the cursor into captured frames — a captured (delayed) cursor would float
        // mid-screen even while the game hides the real one. With capture off, the REAL hardware cursor
        // draws above the overlay and shows/hides exactly as the game commands.
        try { wgc_ctx->session.IsCursorCaptureEnabled(false); } catch(...) {}
        // WGC's default MinUpdateInterval (~16.6 ms) caps delivery at ~60/s; Win11 24H2+ exposes the
        // knob. Requesting 1 ms makes WGC deliver DUPLICATE frames in micro-bursts up to the compose
        // rate — the burst deltas POISON the arrival-cadence EMA (T_ema collapses → the paced presents
        // fire back-to-back → MAILBOX latest-wins keeps only the real frame → the interps never reach the
        // panel). 8 ms caps capture at ~125/s — above any game source we serve, below the duplicate flood.
        try {
            // --cap-fps N throttles the source to N fps (MinUpdateInterval = 1/N, in 100ns units);
            // default (cap_fps==0) keeps the 8ms (~125/s) anti-duplicate-flood cap.
            const long long mui_100ns = (cfg.cap_fps > 0) ? (10000000LL / (long long)cfg.cap_fps) : 80000LL;
            wgc_ctx->session.MinUpdateInterval(winrt::Windows::Foundation::TimeSpan{mui_100ns});
            if (cfg.cap_fps > 0)
                std::printf("[ra] WGC MinUpdateInterval: %.1f ms (--cap-fps %d) — source throttled so the FG interpolates more frames/pair (crescent easier to eyeball)\n", (double)mui_100ns/10000.0, cfg.cap_fps);
            else
                std::printf("[ra] WGC MinUpdateInterval: 8 ms requested (~125/s cap; anti-duplicate-flood)\n");
        } catch(...) { std::printf("[ra] WGC MinUpdateInterval: unavailable (pre-24H2) — default ~60/s cap\n"); }
        wgc_ctx->session.StartCapture();
        std::printf("[ra] WGC session started — frames arriving via free-threaded callback\n");
    }
#endif

    // ── Convert pipeline (A: native→RGBA8 work) ───────────────────────────────
    {
        VkSamplerCreateInfo s{}; s.sType=VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO; s.magFilter=VK_FILTER_LINEAR; s.minFilter=VK_FILTER_LINEAR; s.addressModeU=s.addressModeV=s.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE; vkCreateSampler(A.dev,&s,nullptr,&cvSamp);
        const VkDescriptorSetLayoutBinding bd[2]={{0,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_COMPUTE_BIT,&cvSamp},{1,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr}};
        VkDescriptorSetLayoutCreateInfo dl{}; dl.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO; dl.bindingCount=2; dl.pBindings=bd; vkCreateDescriptorSetLayout(A.dev,&dl,nullptr,&cvDsl);
        VkPushConstantRange pcr{}; pcr.stageFlags=VK_SHADER_STAGE_COMPUTE_BIT; pcr.size=8;
        VkPipelineLayoutCreateInfo pl{}; pl.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO; pl.setLayoutCount=1; pl.pSetLayouts=&cvDsl; pl.pushConstantRangeCount=1; pl.pPushConstantRanges=&pcr; vkCreatePipelineLayout(A.dev,&pl,nullptr,&cvLayout);
        VkShaderModuleCreateInfo mci{}; mci.sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO; mci.codeSize=kHdrConvertSpv.size()*sizeof(uint32_t); mci.pCode=kHdrConvertSpv.data(); VkShaderModule mod; vkCreateShaderModule(A.dev,&mci,nullptr,&mod);
        VkPipelineShaderStageCreateInfo stg{}; stg.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; stg.stage=VK_SHADER_STAGE_COMPUTE_BIT; stg.module=mod; stg.pName="main";
        VkComputePipelineCreateInfo cp{}; cp.sType=VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO; cp.stage=stg; cp.layout=cvLayout; vkCreateComputePipelines(A.dev,VK_NULL_HANDLE,1,&cp,nullptr,&cvPipe); vkDestroyShaderModule(A.dev,mod,nullptr);
        const VkDescriptorPoolSize psz[2]={{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1},{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1}};
        VkDescriptorPoolCreateInfo pi{}; pi.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO; pi.maxSets=1; pi.poolSizeCount=2; pi.pPoolSizes=psz; vkCreateDescriptorPool(A.dev,&pi,nullptr,&cvPool);
        VkDescriptorSetAllocateInfo dai{}; dai.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO; dai.descriptorPool=cvPool; dai.descriptorSetCount=1; dai.pSetLayouts=&cvDsl; vkAllocateDescriptorSets(A.dev,&dai,&cvSet);
        VkDescriptorImageInfo si{}; si.sampler=cvSamp; si.imageView=Anative.view; si.imageLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkDescriptorImageInfo di{}; di.imageView=Awork.view; di.imageLayout=VK_IMAGE_LAYOUT_GENERAL;
        const VkWriteDescriptorSet wds[2]={{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,nullptr,cvSet,0,0,1,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,&si,nullptr,nullptr},{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,nullptr,cvSet,1,0,1,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,&di,nullptr,nullptr}};
        vkUpdateDescriptorSets(A.dev,2,wds,0,nullptr);
        if(!cvPipe){ std::printf("[ra] convert pipeline failed\n"); goto done; }
    }

    // ── OpticalFlowPipeline (B) ───────────────────────────────────────────────
    // emit_second_best (10th arg) = use_ambig — when ON the pipeline creates cand_image() (RGBA16F, same
    // mvw×mvh as motion_image()) and the finest match level writes the runner-up MV+SAD. When OFF (default)
    // cand_image() is VK_NULL_HANDLE and nothing extra is written (byte-identical).
    // Init the B-path flow at the (down)sampled work size WW_flow×WH_flow → motion_width()/motion_height()
    // = (WW_flow+7)/8 × (WH_flow+7)/8, so the MV grid (and the whole host-bridge / WAP / CPU-tail chain
    // that sizes off ofp.motion_width()) shrinks ~flow_div² with NO change to those sites. At flow_div==1,
    // WW_flow==WW → byte-identical init.
    // FD-route the flow pipeline (B→A under single_gpu). Under single_gpu ofp lives on A (the 4090); its
    // compute dispatches run on A.q2 (the F-thread's lane), serialized vs C's convert by a_q2_mtx.
    // fg_variant=true opts into the FG-LOCAL matcher with the runner-up tracking compiled out. INERT BY
    // DEFAULT — a --no-ambig artifact: the pipeline AND-folds !emit_second_best internally, and the FG
    // DEFAULT runs ambig=ON (cfg.ambig default true ⇒ emit_second_best = use_ambig = 1), so the variant
    // AUTO-FALLS-BACK to the canonical matcher (which produces the candidate field the ambig warp reads). It
    // short-circuits the runner-up tracking ONLY under --no-ambig (the matcher's raw pick), where the
    // runner-up was tracked unconditionally with no consumer. Kept: correct + byte-identical-off.
    if(!ofp.init(FD.phys,FD.dev,WW_flow,WH_flow,2u,2,cfg.res_ceil,cfg.conf_improv,cfg.agreement,use_ambig,/*mv_affine*/false,/*mv_subpel*/cfg.mv_subpel,/*coarse_wide*/false,/*mv_candsel*/cfg.mv_candsel,/*fg_variant*/true))
        { std::printf("[ra] OpticalFlowPipeline init failed\n"); goto done; }
    // Smoke print: the live MV grid after init. At flow_div>1 this confirms the halved/quartered
    // grid (== (WW_flow+7)/8 × (WH_flow+7)/8). Printed once at startup.
    std::printf("[ra] flow-scale %u: MV grid %ux%u (work %ux%u, flow %ux%u)%s\n",
                flow_div, ofp.motion_width(), ofp.motion_height(), WW, WH, WW_flow, WH_flow,
                flow_div>1u?" — DRS active (~flow_div^2 fewer tiles + CPU work/pair)":" (byte-identical default)");
    // The pre-init host-bridge grid ((WW_flow+7)/8) MUST equal the accessor the post-init sites
    // read (ofp.motion_width()). They are derived the same way, but a divergence (e.g. a future change to
    // create_mv_image's rounding) would corrupt the copy-out — assert it here, cold path, once at startup.
    if(ofp.motion_width()!=(WW_flow+7u)/8u || ofp.motion_height()!=(WH_flow+7u)/8u){
        std::printf("[ra] FATAL: MV-grid mismatch — ofp.motion_width()=%u expected=%u (host bridges would over/under-read). Aborting init.\n",
                    ofp.motion_width(),(WW_flow+7u)/8u);
        goto done;
    }
    if(cfg.mv_prior) ofp.set_temporal_prior(true);   // arm the dual-centre temporal prior (B-path)
    // --fg-prebake: pre-bake the two ping-pong descriptor collections so the F-thread's per-pair
    // record_optical_flow SKIPS the vkUpdateDescriptorSets burst. The level-0 inputs the F-thread feeds
    // are FIXED: at flow_div==1 the matcher reads the two Bframe views in BOTH orderings (fwd = (prv,cur),
    // bwd = (cur,prv), and prv/cur ping-pong over {0,1}); at flow_div>1 it always reads the two Bflow
    // scratch views (the downsample blits into Bflow[0]/Bflow[1] regardless of parity). c_view is Cinterp
    // every record. INERT BY DEFAULT — a --no-ambig artifact. The FG DEFAULT runs ambig=ON (the
    // periodic-texture killer, which USES the runner-up tracking to arbitrate SAD ties), so
    // prebake_fg_descriptors — which requires match_fg_active() (the variant matcher, itself gated OFF
    // whenever emit_second_best/ambig is on) AND no affine — returns FALSE on the default, the per-record
    // update path stays armed (byte-identical), and the print below reads INERT. It ELIMINATES the burst
    // ONLY under --no-ambig (the matcher's raw pick). Cold call (no GPU work).
    if(cfg.fg_prebake){
        const VkImageView pa0 = (flow_div>1u) ? Bflow[0].view : Bframe[0].view;
        const VkImageView pb0 = (flow_div>1u) ? Bflow[1].view : Bframe[1].view;
        const VkImageView pa1 = (flow_div>1u) ? Bflow[0].view : Bframe[1].view;   // swapped ordering (bwd / odd parity)
        const VkImageView pb1 = (flow_div>1u) ? Bflow[1].view : Bframe[0].view;
        const bool baked = ofp.prebake_fg_descriptors(pa0,pb0,pa1,pb1,Cinterp.view);
        std::printf("[ra] --fg-prebake: %s (fg_variant_active=%d, use_ambig=%d) — per-pair vkUpdateDescriptorSets burst %s.\n",
                    baked?"ARMED":"INERT (eligibility not met)", (int)ofp.match_fg_active(), (int)use_ambig,
                    baked?"ELIMINATED (host-CPU relief; read the call-count delta at F teardown)":"unchanged");
    }
    // ── --nvofa: build the hardware OFA flow provider ─────────────
    // Gated on --nvofa AND single_gpu (the OFA is 4090-only — FD==A; NVOFA is a single-GPU lever) AND A
    // actually armed the OFA queue (vdev_create auto-disabled if the device lacked VK_NV_optical_flow). On
    // ANY failure → print + fall back to the classical ofp (use_nvofa stays false → byte-identical). The
    // session is sized so its 4x4 grid == ofp.motion_width()/height() exactly (in_w=mvw*4). use_bidir picks
    // BOTH_DIRECTIONS. The convert lands the OFP contract (RG16F MV in WW_flow px, cost->sad_best, a
    // SEPARATE |A-B|->sad_zero), so every downstream consumer reads ofp.motion_image()/sad_field_image()
    // UNCHANGED.
    if(cfg.nvofa){
        if(!single_gpu){
            std::printf("[ra] --nvofa: multi-GPU rig — the flow rides device B (1080 Ti, no OFA); NVOFA is a single-GPU lever (use --force-single-gpu). Falling back to the classical OFP (byte-identical).\n");
        } else if(flow_div>1u){
            // LIMITATION: at flow_div>1 the classical OFP produces MV in WW_flow pixel units (it searches
            // the Bflow downscale); reconciling those units with the OFA path (which would need the Bflow
            // source, produced mid-cmdF) is unsupported. At flow_div==1 (the operating point — 1080p)
            // WW_flow==WW so mv_scale=WW/in_w is exact. Refuse nvofa under --flow-scale>1 (classical OFP).
            std::printf("[ra] --nvofa: --flow-scale %u (flow_div>1) not supported by the v1 OFA path (unit reconciliation is a follow-up) — falling back to the classical OFP (byte-identical).\n",flow_div);
        } else if(A.ofaQueue==VK_NULL_HANDLE || !A.has_optical_flow){
            std::printf("[ra] --nvofa: device A lacks VK_NV_optical_flow (or no OFA queue) — falling back to the classical OFP (byte-identical).\n");
        } else {
            const std::vector<uint32_t> spvnv(kNvofaConvertSpv.begin(),kNvofaConvertSpv.end());
            const uint32_t mvw=ofp.motion_width(), mvh=ofp.motion_height();
            if(nvofa_create(A,nvofa,mvw,mvh,WW_flow,use_bidir,cfg.nvofa_cost_scale,cfg.nvofa_sadz_scale,spvnv)
               && nvofa_alloc_cmds(A,nvofa)){
                VkDescriptorSetAllocateInfo dsa{}; dsa.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO; dsa.descriptorPool=nvofa.dp; dsa.descriptorSetCount=1; dsa.pSetLayouts=&nvofa.dsl;
                bool sok = (vkAllocateDescriptorSets(A.dev,&dsa,&nvofa.setF)==VK_SUCCESS);
                if(sok && use_bidir) sok = (vkAllocateDescriptorSets(A.dev,&dsa,&nvofa.setB)==VK_SUCCESS);
                if(sok){
                    // fwd set: OFA fwd flow/cost + inputs + ofp's OWN MV/SAD images.
                    nvofa_write_set(A,nvofa,nvofa.setF,nvofa.mvFiv,nvofa.costFuv,nvofa.in0cv,nvofa.in1cv,ofp.motion_view(),ofp.sad_field_view());
                    if(use_bidir) nvofa_write_set(A,nvofa,nvofa.setB,nvofa.mvBiv,nvofa.costBuv,nvofa.in0cv,nvofa.in1cv,ofp.motion_view(),ofp.sad_field_view());
                    use_nvofa=true;
                    // Residual (VUID-vkCmdDraw-None-09600, 2 occ at startup): on the OFA path ofp.execute()
                    // never runs, so an OFP-adjacent image consumed by a GPU command is reported UNDEFINED for
                    // ~the first 2 frames (deferred submit-time layout check). Benign — content is don't-care at
                    // startup, the path runs correctly (cap recovers, frz 0, slice drops, byte-identical off).
                    // Not motion/sad (a one-shot RO seed of those does NOT clear it) nor cand (null without --ambig).
                    std::printf("[ra] --nvofa: ACTIVE — HW OFA flow provider (in %ux%u, grid %ux%u, mv_scale %.3f, bidir=%d, cost_scale %.3f, sadz_scale %.3f). Replaces the classical block-match. sad_best=cost remap + sad_zero=separate |A-B| pass (BOTH need eye-calibration).\n",
                                nvofa.in_w,nvofa.in_h,mvw,mvh,nvofa.mv_scale,(int)use_bidir,cfg.nvofa_cost_scale,cfg.nvofa_sadz_scale);
                } else { std::printf("[ra] --nvofa: descriptor-set alloc failed — falling back to the classical OFP\n"); nvofa_destroy(A,nvofa); }
            } else { std::printf("[ra] --nvofa: provider/cmd init failed — falling back to the classical OFP\n"); nvofa_destroy(A,nvofa); }
        }
    }
    // ── Temporal MV smoothing resources (B-path, opt-in) ─────────
    if(cfg.mv_smooth>0.f){
        const uint32_t mvw=ofp.motion_width(), mvh=ofp.motion_height();
        if(img_create(FD,mvw,mvh,VK_FORMAT_R16G16_SFLOAT,VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,mv_prev)){   // FD-route mv_prev (B→A)
            // Zero-seed prev (the cut-bypass self-seeds it on the first real-motion pair) → GENERAL.
            oneshot(FD,[&](VkCommandBuffer c){   // FD-route the zero-seed oneshot
                img_barrier(c,mv_prev.img,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,0,VK_ACCESS_TRANSFER_WRITE_BIT);
                VkClearColorValue z{}; VkImageSubresourceRange r{VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
                vkCmdClearColorImage(c,mv_prev.img,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,&z,1,&r);
                img_barrier(c,mv_prev.img,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_GENERAL,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT);
            });
            const std::vector<uint32_t> spvmv(kMvSmoothSpv.begin(),kMvSmoothSpv.end());
            use_mv_smooth=mvsm_create(FD,ofp.motion_image(),mv_prev.view,spvmv,mvsm);   // FD-route the MV-smooth pipeline (B→A)
        }
        if(use_mv_smooth) std::printf("[ra] mv-smooth: ACTIVE (alpha=%.2f, B-path) — temporal MV EMA\n",cfg.mv_smooth);
        else std::printf("[ra] mv-smooth: requested but resource init failed — disabled\n");
    }
    // ── OpticalFlowPipeline (A) — primary-FG path ────────────────
    // OFP on A reads AframeA[prv/cur] (already on A's VRAM) → zero PCIe transfer.
    if(want_pfg&&AframeA[0].img){
        // The primary-FG OFP always runs with emit_second=false (it never feeds the ambig warp) → it is a
        // pure default-path consumer of the runner-up-free FG-variant matcher.
        pfg_enabled=ofpA.init(A.phys,A.dev,WW,WH,2u,2,cfg.res_ceil,cfg.conf_improv,cfg.agreement,/*emit_second*/false,/*mv_affine*/false,/*mv_subpel*/cfg.mv_subpel,/*coarse_wide*/false,/*mv_candsel*/cfg.mv_candsel,/*fg_variant*/true);
        if(!pfg_enabled) std::printf("[ra] ofpA (primary-FG) init failed — assist path only\n");
        else if(cfg.mv_prior) ofpA.set_temporal_prior(true);   // arm the prior on the A (primary-FG) OFP too
    }

    // ── Upscale pipeline (G) ──────────────────────────────────────────────────
    if(use_upscale){
        // .data()/.size() (raw uint32_t*, a common type) — NOT .begin()/.end(): the two SPV arrays
        // differ in size, so their std::array iterators are distinct class types that MSVC refuses to
        // unify in a ternary (mingw/gcc decayed them to a pointer; MSVC does not).
        const uint32_t* spv_ptr = cfg.lanczos ? kUpscaleLanczosSpv.data() : kUpscaleBilinearSpv.data();
        const size_t    spv_len = cfg.lanczos ? kUpscaleLanczosSpv.size() : kUpscaleBilinearSpv.size();
        const std::vector<uint32_t> spv(spv_ptr, spv_ptr + spv_len);
        if(!up_create(G,Gsrc,Gdst,spv,upPipe)){ std::printf("[ra] upscale pipeline failed\n"); goto done; }
    }
    // ── iGPU convert+pack + B/G unpack pipelines — × kCapSlots ─
    if(use_igpu_convert){
        const VkDeviceSize ab_g=VkDeviceSize(NAT_W)*NAT_H*nat_bpp;
        const VkDeviceSize rp_sz=VkDeviceSize(WW)*WH*3u;
        const VkDeviceSize hR_sz=VkDeviceSize(WW)*WH*4u;
        const std::vector<uint32_t> spvcp(kIgpuConvertPackSpv.begin(),kIgpuConvertPackSpv.end());
        const std::vector<uint32_t> spvup(kUnpackPackedSpv.begin(),kUnpackPackedSpv.end());
        VkImageView bviews[2]={Bframe[0].view,Bframe[1].view};
        VkImageView gv[1]={Gsrc.view};
        for(int _s=0;_s<cap_slots;++_s){
            if(!cpipe_create(G,Astage_g.buf,ab_g,hRP_g[_s].buf,rp_sz,hR_g[_s].buf,hR_sz,spvcp,cpPipe[_s]))
                { std::printf("[ra] igpu convpack[%d] failed\n",_s); goto done; }
            if(!unpipe_create(B,hRP_b_dev[_s].buf,rp_sz,bviews,2,spvup,ubPipe[_s]))
                { std::printf("[ra] B unpack[%d] failed\n",_s); goto done; }
            if(!unpipe_create(G,hRP_g[_s].buf,rp_sz,gv,1,spvup,ugPipe[_s]))
                { std::printf("[ra] G unpack[%d] failed\n",_s); goto done; }
            if(cfg.igpu_field){   // the contour-field pipe (src=hR_g RGBA8, dst=hFIELD_g)
                const std::vector<uint32_t> spvfld(kIgpuFieldSpv.begin(),kIgpuFieldSpv.end());
                if(!fpipe_create(G,hR_g[_s].buf,hR_sz,hFIELD_g[_s].buf,hR_sz,spvfld,fpipe[_s]))
                    { std::printf("[ra] igpu field[%d] failed\n",_s); goto done; }
            }
        }
        // Keep primary-FG when the user EXPLICITLY asked for it (--fg-gpu primary) AND device A has a real
        // 2nd queue (A.q2!=A.q, forced same-family via prefer_same_family_q2). F then routes its flow
        // submit to A.q2 (submit_wait_q2) → lock-free, no race with P on A.q; convert stays on the iGPU
        // (this block). Otherwise disable primary-FG.
        if(cfg.fg_gpu==FG_PRIMARY && A.q2!=A.q){
            std::printf("[ra] igpu-convert + --fg-gpu primary: primary-FG KEPT on A, flow routed to A.q2 (lock-free same-family partition; convert stays on iGPU)\n");
        } else {
            pfg_enabled=false;
            std::printf("[ra] igpu-convert: primary-FG disabled (4090 freed to capture+present only)\n");
        }
    }
    // ── warp-at-presenter pipeline (A, the bridge owner) ──────────
    // Presenter-local sampled inputs (two pair reals WW×WH RGBA8, MV+SAD grids RG16F) + the rgba8 warp
    // output, re-uploaded per pair-advance and re-warped per tick at the exact phase. The warper is A
    // (the bridge owner); the WD/WP aliases name the A objects (wapPipeA etc.).
    if(use_wap){
        VDev& WD = A;
        WapPipe& WP = wapPipeA;
        // --upload-xfer: the SINGLE runtime gate. ON only when the flag is set AND the device actually
        // armed a transfer queue (qT non-null ⇒ qfamT!=UINT32_MAX). OFF → EVERYTHING below stays exactly
        // as is: EXCLUSIVE images, the upload on A.q with its sync vkWaitForFences, no A.qT submit, no
        // timeline semaphores touched (byte-identical-off). qfamT is a COMPUTE-capable family on the 4090
        // (fam2, flags 0xE) so the in-upload median dispatch is valid on A.qT. The CONCURRENT family pair
        // {qfam,qfamT} is built once here for the wap-input images.
        xfer_on = cfg.upload_xfer && WD.qT != VK_NULL_HANDLE;
        xfer_fams[0] = WD.qfam; xfer_fams[1] = WD.qfamT;
        // The in-upload median pass (use_mv_median/use_mv_guided) is a COMPUTE dispatch that runs on A.qT
        // — so qfamT MUST carry the COMPUTE bit. The transfer-queue pick prefers a COMPUTE&&!GRAPHICS
        // family, but can fall back to a transfer-only family when none exists; running a dispatch there is
        // invalid → device-lost. Rather than fork the median back to A.q (a cross-queue cmd-buffer split),
        // force xfer OFF if qfamT lacks COMPUTE (degrade to the byte-identical A.q sync upload — never a
        // crash). On the 4090 fam2 is 0xE (has COMPUTE).
        if(xfer_on){
            uint32_t qfc=0; vkGetPhysicalDeviceQueueFamilyProperties(WD.phys,&qfc,nullptr);
            std::vector<VkQueueFamilyProperties> qfs(qfc); vkGetPhysicalDeviceQueueFamilyProperties(WD.phys,&qfc,qfs.data());
            if(WD.qfamT>=qfc || !(qfs[WD.qfamT].queueFlags&VK_QUEUE_COMPUTE_BIT)){
                std::printf("[ra] --upload-xfer: qfamT=%u lacks the COMPUTE bit — the in-upload median dispatch cannot run on A.qT; forcing the upload back onto A.q (byte-identical)\n", WD.qfamT);
                xfer_on=false;
            }
        }
        // --upload-xfer: allocate the PING-PONG upload command buffers from A.poolT ONCE here (init,
        // zero-alloc hot path). A.poolT is RESET_COMMAND_BUFFER_BIT + family-bound to qfamT, so a
        // cmdUpload[] buffer submits to A.qT with no family trap. On a failure we force xfer_on OFF
        // (degrade to the byte-identical A.q sync upload — never a crash). Both buffers must allocate or
        // neither is used.
        if(xfer_on){
            VkCommandBufferAllocateInfo cbiU{}; cbiU.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            cbiU.commandPool=WD.poolT; cbiU.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY; cbiU.commandBufferCount=2;
            if(vkAllocateCommandBuffers(WD.dev,&cbiU,cmdUpload)!=VK_SUCCESS){
                std::printf("[ra] --upload-xfer: ping-pong upload cmd buffer alloc on A.poolT FAILED — forcing the upload back onto A.q (byte-identical)\n");
                cmdUpload[0]=cmdUpload[1]=VK_NULL_HANDLE; xfer_on=false;
            }
        }
        if(xfer_on) std::printf("[ra] --upload-xfer: ACTIVE — wap-input images CONCURRENT {qfam=%u,qfamT=%u}, upload→A.qT with two timeline semaphores + ping-pong cmd buffers (no host wait, no QFOT, no CPU mutex)\n", WD.qfam, WD.qfamT);
        Img& wPrev = wapPrevA;
        Img& wCur  = wapCurA;
        Img& wMV   = wapMVA;
        Img& wSAD  = wapSADA;
        Img& wOut  = wapOutA;
        Img& wMVB  = wapMVBA;   // backward MV (binding 5) — created unconditionally so the descriptor set
                                // is complete; uploaded + read only when use_bidir.
        Img& wC2   = wapC2A;    // second-best candidate (binding 10) — created only with use_ambig
                                // (RGBA16F); when off the binding is placeholder-bound to wMV.view (never sampled).
        // The WAP sampled MV/SAD/MVB grids are sized off ofp.motion_width()/height() (the single source of
        // truth, valid here — after ofp.init). At flow_div>1 they shrink with the OFP grid; the pair-real
        // images (wPrev/wCur) stay full-res WW×WH (the warp samples the captured frames full-res, upsampling
        // the smaller MV field via textureSize() in the shader). At flow_div==1 == (WW+7)/8.
        const uint32_t mvw=ofp.motion_width(), mvh=ofp.motion_height();
        // wPrev/wCur/wMV/wSAD/wMVB are UPLOAD-WRITTEN (A.qT) + WARP-READ (A.q) → CONCURRENT {qfam,qfamT}
        // when xfer_on (no QFOT); EXCLUSIVE otherwise (byte-identical). wOut is the warp OUTPUT (written +
        // blitted both on A.q, same queue) → stays EXCLUSIVE (no cross-queue).
        if(!img_create(WD,WW,WH,VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,wPrev,true,xfer_on?xfer_fams:nullptr,xfer_on?2u:0u)||
           !img_create(WD,WW,WH,VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,wCur,true,xfer_on?xfer_fams:nullptr,xfer_on?2u:0u)||
           !img_create(WD,mvw,mvh,VK_FORMAT_R16G16_SFLOAT,VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,wMV,true,xfer_on?xfer_fams:nullptr,xfer_on?2u:0u)||
           !img_create(WD,mvw,mvh,VK_FORMAT_R16G16_SFLOAT,VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,wSAD,true,xfer_on?xfer_fams:nullptr,xfer_on?2u:0u)||
           !img_create(WD,mvw,mvh,VK_FORMAT_R16G16_SFLOAT,VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,wMVB,true,xfer_on?xfer_fams:nullptr,xfer_on?2u:0u)||
           !img_create(WD,WW_warp,WH_warp,VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT,wOut)){   // wOut(=wapOutA) at WW/N×WH/N (the descriptor binds this scaled view at wap_create; the shader's imageSize(u_output) drives the per-invocation work). warp_div==1 ⇒ WW_warp==WW (byte-identical). OURS only — the pair-reals wPrev/wCur above stay full-res (no game cap).
            std::printf("[ra] WAP image allocation failed — disabling warp-at-presenter\n"); use_wap=false;
        } else {
            // The inertia persistence image (R8, mvw×mvh) — created UNCONDITIONALLY with WAP (the SAME
            // completeness discipline wMVB uses for binding 5): binding 8 must hold a valid R8 view
            // regardless of --inertia, so wap_create's descriptor set is always complete. Uploaded per pair
            // from hPER_a ONLY when use_inertia (the upload transitions DST→RO); off-inertia it stays the
            // initial RO image and the shader never samples it (inertia_thresh=0 gates every read). On a
            // creation failure we clear cfg.inertia and bind the MV view as a harmless placeholder below.
            if(!img_create(WD,mvw,mvh,VK_FORMAT_R8_UNORM,VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,wapPERA,true,xfer_on?xfer_fams:nullptr,xfer_on?2u:0u)){   // upload-written + warp-read → CONCURRENT under xfer_on
                std::printf("[ra] WAP PER image failed — disabling inertia\n"); cfg.inertia=false;
            }
            // The A-side iGPU contour field image (R32_UINT, full-res WW×WH). STORAGE (the fill's
            // imageLoad/Store via fillPipeA AND the warp's binding-11 imageLoad) | TRANSFER_DST (the per-pair
            // upload from hFIELD_a). Created when EITHER --afill (the visualizer) OR --bg-snap (the warp
            // consumer) owns the field; on failure we clear BOTH (graceful degrade — the WAP path keeps
            // running without the field, not a crash). When NEITHER owns it, a 1×1 r32ui PLACEHOLDER
            // (wapFIELDph) is bound to binding 11 instead: binding 11 is statically used in the SPIR-V so it
            // must always hold a valid r32ui storage view, and the candidate-binding-10 float-view placeholder
            // trick does NOT work for an integer storage image (format must match) — a real 1×1 r32ui image is
            // the cheap placeholder (never read: bg_snap_on=0 when bg_snap is off → byte-identical).
            if(cfg.d.field_to_warp){   // ALL field consumers, not just afill||bg_snap
                if(!img_create(WD,WW,WH,VK_FORMAT_R32_UINT,VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,wapFIELDA,true,xfer_on?xfer_fams:nullptr,xfer_on?2u:0u)){   // upload-written + warp-read (binding 11) → CONCURRENT under xfer_on
                    std::printf("[ra] WAP FIELD image failed — disabling all field consumers (afill/bg-snap/band-xfade/disoccl-hardpick)\n"); cfg.afill=false; cfg.bg_snap=false; cfg.band_xfade=0.f; cfg.disoccl_hardpick=0.f; cfg.d.field_to_warp=false;
                }
            }
            if(!wapFIELDA.img){   // binding-11 r32ui placeholder (1×1, GENERAL, never sampled when bg_snap off)
                if(!img_create(WD,1,1,VK_FORMAT_R32_UINT,VK_IMAGE_USAGE_STORAGE_BIT,wapFIELDph)){
                    std::printf("[ra] WAP FIELD placeholder failed — disabling warp-at-presenter\n"); use_wap=false;
                }
            }
            // The second-best candidate image (RGBA16F, mvw×mvh) — created only with use_ambig. Bound as
            // warp binding 10 (u_candidates); uploaded per pair from hC2_a (DST→RO). On a creation failure
            // we clear use_ambig (the warp falls to byte-identical off) and bind wMV.view as a harmless
            // placeholder below (the same completeness trick wMVB/wapDISA use). When use_ambig was already
            // off (e.g. --no-ambig or the bridge alloc failed above) the image is simply not created.
            if(use_ambig){
                if(!img_create(WD,mvw,mvh,VK_FORMAT_R16G16B16A16_SFLOAT,VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,wC2,true,xfer_on?xfer_fams:nullptr,xfer_on?2u:0u)){   // upload-written + warp-read (binding 10) → CONCURRENT under xfer_on
                    std::printf("[ra] WAP candidate image failed — disabling ambiguity\n"); use_ambig=false;
                }
            }
            // --vblend: the NEXT-pair forward-MV TARGET image (RG16F, mvw×mvh — the SAME format/size as
            // wapMVA, mirrored exactly). Bound as warp binding 12 (u_mv_target); uploaded per pair from
            // hMV_a[target_gen] (the next-fresher published generation). Created only with cfg.vblend; on a
            // creation failure we clear cfg.vblend (the warp falls to byte-identical off — vblend_on=0 push,
            // the placeholder binding) and bind wMV.view as a harmless unread placeholder below (the same
            // completeness trick wMVB/wapC2A use). When vblend is off the image is simply not created.
            if(cfg.vblend){
                if(!img_create(WD,mvw,mvh,VK_FORMAT_R16G16_SFLOAT,VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,wapMVTA,true,xfer_on?xfer_fams:nullptr,xfer_on?2u:0u)){   // upload-written + warp-read (binding 12) → CONCURRENT under xfer_on
                    std::printf("[ra] WAP MV-target image failed — disabling vblend\n"); cfg.vblend=false;
                }
            }
            // --ts-smooth: the PREVIOUS final-output history image (RGBA8, full-res WW×WH — the SAME format/
            // size as wOut, mirrored exactly). Bound as warp binding 13 (u_prev_out, sampled); the copy-back
            // (after the per-tick blit) writes wOut → wapPrevOutA, so the next tick's warp samples THIS tick's
            // output. SAMPLED (the warp's binding-13 sample) | TRANSFER_DST (the per-tick copy target).
            // Created only with cfg.ts_smooth>0; on a creation failure we clear cfg.ts_smooth (the warp falls
            // to byte-identical off — ts_smooth=0 push, the placeholder binding) and bind wCur.view as a
            // harmless unread placeholder below (the same completeness trick wMVB/wapC2A/wapMVTA use).
            if(cfg.ts_smooth>0.0f){
                // The ts-smooth history MUST match wapOutA's extent (the per-tick wapOutA→wapPrevOutA copy
                // uses that extent, and the warp samples binding 13 over the same domain). Size it WW/N×WH/N
                // too. warp_div==1 ⇒ WW_warp==WW (byte-identical).
                if(!img_create(WD,WW_warp,WH_warp,VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,wapPrevOutA)){
                    std::printf("[ra] WAP prev-output image failed — disabling ts-smooth\n"); cfg.ts_smooth=0.f;
                }
            }
            // The dissidence-mask image (R8, mvw×mvh) — uploaded per pair from hDIS_a, held RO. Bound as
            // binding 6, so it must exist BEFORE wap_create writes the descriptor set. Created only with
            // --gme (the matte's two layers both come from gme; the parse gate already forbids --matte
            // without --gme). On any DIS-image failure we clear cfg.gme (no mask → no upload, no matte) and
            // bind the MV view as a harmless placeholder.
            if(cfg.gme){
                if(!img_create(WD,mvw,mvh,VK_FORMAT_R8_UNORM,VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,wapDISA,true,xfer_on?xfer_fams:nullptr,xfer_on?2u:0u)){   // upload-written + warp-read (binding 6) → CONCURRENT under xfer_on
                    std::printf("[ra] WAP DIS image failed — disabling gme\n"); cfg.gme=false;
                }
                // The backward (cur-anchored) dissidence image — same R8 mvw×mvh as wapDISA. Created only
                // when gme survived AND bidir is live (the bwd field exists). On failure we clear cfg.gme too
                // (no bwd mask → the dual-anchored matte can't run) — same degrade path.
                if(cfg.gme && use_bidir){
                    if(!img_create(WD,mvw,mvh,VK_FORMAT_R8_UNORM,VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,wapDISBA,true,xfer_on?xfer_fams:nullptr,xfer_on?2u:0u)){   // upload-written + warp-read (binding 7) → CONCURRENT under xfer_on
                        std::printf("[ra] WAP DIS_bwd image failed — disabling gme\n"); cfg.gme=false;
                    }
                }
            }
            const std::vector<uint32_t> spvw(kWapWarpSpv.begin(),kWapWarpSpv.end());
            // binding 6 = wapDISA when gme is live, else the forward MV view as an unread placeholder (the
            // same completeness trick binding 5 uses for MV_bwd off-bidir). matte_on=0 off-matte → the
            // shader never samples binding 6, so the placeholder is harmless.
            const VkImageView dis_view = cfg.gme ? wapDISA.view : wMV.view;
            // binding 7 = wapDISBA when both gme AND bidir are live (the bwd mask exists), else a
            // placeholder. Prefer wapDISA.view (so binding 7's R8 format matches binding 6 — the sampler
            // is shared) when gme is on but bidir is off; else wMV.view (the binding-5/6 trick). matte
            // requires --bidir, so when matte is on this is always the real wapDISBA; the placeholder only
            // matters for gme-without-bidir (matte off → binding 7 is never sampled).
            const VkImageView disb_view = (cfg.gme && use_bidir) ? wapDISBA.view
                                        : (cfg.gme ? wapDISA.view : wMV.view);
            // binding 8 = wapPERA (R8) when it was created (the normal case — it is created unconditionally
            // with WAP above), else the MV view as a harmless unread placeholder (the binding-5/6/7 trick).
            // The shader reads it only when inertia_thresh>0.5 (a uniform push), and inertia_thresh is 0
            // whenever cfg.inertia is false (incl. the create-failure clear) — so the placeholder is never
            // sampled. wapPERA.view==VK_NULL_HANDLE only on a create failure (which already cleared
            // cfg.inertia); the ternary keeps the descriptor write valid in that case.
            const VkImageView per_view = wapPERA.view ? wapPERA.view : wMV.view;
            // binding 10 = wapC2A (RGBA16F second-best candidate) when it was created (use_ambig), else the
            // MV view as a harmless unread placeholder (the binding-5/6/7/8 completeness trick). The shader
            // reads it only when ambig_on>0.5 (a uniform push), and ambig_on is 0 whenever use_ambig is
            // false (incl. any create/bridge-failure clear above) — so the placeholder is never sampled.
            const VkImageView cand_view = wapC2A.view ? wapC2A.view : wMV.view;
            // binding 11 = wapFIELDA (R32_UINT contour field) when --afill/--bg-snap created it, else the
            // 1×1 r32ui placeholder (wapFIELDph). The shader reads it only when bg_snap_on>0.5 (a uniform
            // push), which the host sets only under cfg.bg_snap (and wapFIELDA then exists) — the placeholder
            // is never sampled. An r32ui storage binding needs a format-matching view, so (unlike binding
            // 10's float placeholder) the placeholder is a real 1×1 r32ui image.
            const VkImageView field_view = wapFIELDA.view ? wapFIELDA.view : wapFIELDph.view;
            // --vblend: binding 12 = wapMVTA (RG16F next-pair MV target) when it was created (cfg.vblend),
            // else the forward MV view as a harmless unread placeholder (the binding-5/6/7/8/10 completeness
            // trick). The shader reads it only when vblend_on>0.5 (a uniform push), and vblend_on is 0
            // whenever cfg.vblend is false (incl. any create-failure clear above) — so the placeholder (an
            // rg16f view, format-matching binding 12) is never sampled.
            const VkImageView mvt_view = wapMVTA.view ? wapMVTA.view : wMV.view;
            // --ts-smooth: binding 13 = wapPrevOutA (RGBA8 prev-output history) when it was created
            // (cfg.ts_smooth>0), else wCur.view as a harmless unread placeholder (the binding-5/10/12
            // completeness trick; an RGBA8 sampled view, format-matching binding 13). The shader reads it
            // only when ts_smooth>0 (a uniform push), and ts_smooth is 0 whenever cfg.ts_smooth is off (incl.
            // any create-failure clear above) — so the placeholder is never sampled.
            const VkImageView prevout_view = wapPrevOutA.view ? wapPrevOutA.view : wCur.view;
            // The mass counter SSBO — a 4-byte host-coherent counter imported on the WAP device (WD == A)
            // as the binding-9 STORAGE buffer. hfb discipline: the EMH import needs the POINTER aligned to
            // minImportedHostPointerAlignment AND the size a multiple of it (hbuf_import passes both raw to
            // vkCreateBuffer/vkAllocateMemory) — so the alloc is host_align-aligned and host_align-rounded
            // (4096 on NV; a 4-byte/64-align alloc fails the import). Only the first 4 bytes are ever
            // read/written. Failure → disable WAP (binding 9 must be a valid buffer for a complete descriptor
            // set, exactly like the image bindings above). The pointer doubles as the CPU read/reset window.
            const uint64_t mass_al=std::max<uint64_t>(WD.host_align,1u);
            // The warp-output readback buffer (--outdump N). hfb-rounded; failure just disables the dump
            // (diagnostic only, never kills WAP). --qdump ALSO needs this buffer (it reads back the live
            // wapOutA), so allocate it when EITHER flag is set.
            if(cfg.outdump_n>0 || cfg.qdump_n>0){
                const VkDeviceSize ob=(VkDeviceSize)WW*WH*4u;
                const VkDeviceSize obr=(ob+mass_al-1)/mass_al*mass_al;
                hostOutD=_aligned_malloc((size_t)obr,(size_t)mass_al);
                if(!hostOutD||!hbuf_import(WD,hostOutD,obr,hOutD_a,VK_BUFFER_USAGE_TRANSFER_DST_BIT)){
                    std::printf("[ra] outdump/qdump: live-output readback alloc/import failed — disabling\n");
                    cfg.outdump_n=0; cfg.qdump_n=0; }
                CreateDirectoryA("frames",nullptr);
            }
            // The TWO anchor readback buffers (wapPrevA=real N, wapCurA=real N+2). Allocated ONLY when
            // --qdump is set (zero cost when off). Same hfb-rounded alloc + import as hOutD_a; failure
            // disables qdump (diagnostic only, never kills WAP). The live-output buffer (hOutD_a) was
            // allocated above; if that failed it already cleared qdump_n, so this block is skipped too.
            if(cfg.qdump_n>0){
                const VkDeviceSize ob=(VkDeviceSize)WW*WH*4u;
                const VkDeviceSize obr=(ob+mass_al-1)/mass_al*mass_al;
                hostPrevD=_aligned_malloc((size_t)obr,(size_t)mass_al);
                hostCurD =_aligned_malloc((size_t)obr,(size_t)mass_al);
                if(!hostPrevD||!hbuf_import(WD,hostPrevD,obr,hPrevD_a,VK_BUFFER_USAGE_TRANSFER_DST_BIT)||
                   !hostCurD ||!hbuf_import(WD,hostCurD ,obr,hCurD_a ,VK_BUFFER_USAGE_TRANSFER_DST_BIT)){
                    std::printf("[ra] qdump: anchor readback alloc/import failed — qdump disabled\n"); cfg.qdump_n=0; }
                if(cfg.qdump_n>0) CreateDirectoryA(cfg.qdump_dir,nullptr);
            }
            const VkDeviceSize mass_sz=(sizeof(uint32_t)+mass_al-1)/mass_al*mass_al;
            hostMassPtr=_aligned_malloc((size_t)mass_sz,(size_t)mass_al);
            if(hostMassPtr){ *(uint32_t*)hostMassPtr=0u;
                // hMass_a is the host-coherent COPY DESTINATION (TRANSFER_DST), and devMass is the
                // DEVICE-LOCAL SSBO the shader atomicAdds into (STORAGE + TRANSFER_SRC for the per-dispatch
                // copy + TRANSFER_DST for the on-GPU fill reset). Both must succeed; either failure disables
                // WAP (binding 9 must be valid).
                if(!hbuf_import(WD,hostMassPtr,mass_sz,hMass_a,VK_BUFFER_USAGE_TRANSFER_DST_BIT)){
                    std::printf("[ra] WAP mass-counter copy-dest import failed — disabling warp-at-presenter\n"); use_wap=false; }
                else if(!dbuf_create(WD,mass_sz,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_SRC_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT,devMass)){
                    std::printf("[ra] WAP mass-counter device-local alloc failed — disabling warp-at-presenter\n"); use_wap=false; }
            } else { std::printf("[ra] WAP mass-counter alloc failed — disabling warp-at-presenter\n"); use_wap=false; }
            if(use_wap && !wap_create(WD,wPrev.view,wCur.view,wMV.view,wSAD.view,wOut.view,wMVB.view,dis_view,disb_view,per_view,devMass.buf,cand_view,field_view,mvt_view,prevout_view,spvw,WP)){
                std::printf("[ra] WAP pipeline failed — disabling warp-at-presenter\n"); use_wap=false;
            } else if(use_wap){
                // Initial layouts: sampled inputs → SHADER_READ_ONLY (the per-pair upload
                // transitions DST→RO each time), output → GENERAL (the warp dispatch target).
                oneshot(WD,[&](VkCommandBuffer c){
                    img_barrier(c,wPrev.img,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,0,VK_ACCESS_SHADER_READ_BIT);
                    img_barrier(c,wCur.img, VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,0,VK_ACCESS_SHADER_READ_BIT);
                    img_barrier(c,wMV.img,  VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,0,VK_ACCESS_SHADER_READ_BIT);
                    img_barrier(c,wSAD.img, VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,0,VK_ACCESS_SHADER_READ_BIT);
                    // wMVB → RO too. When bidir is off it stays cleared/unread (occl_thresh=0 gates the
                    // shader); when on, wap_upload transitions it DST→RO per pair like wMV.
                    img_barrier(c,wMVB.img, VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,0,VK_ACCESS_SHADER_READ_BIT);
                    img_barrier(c,wOut.img, VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_GENERAL,0,VK_ACCESS_SHADER_WRITE_BIT);
                    // wapDISA → RO (the wap_upload transitions DST→RO per pair when gme is on; off-gme it is
                    // never created — this barrier only fires when the image exists).
                    if(cfg.gme) img_barrier(c,wapDISA.img,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,0,VK_ACCESS_SHADER_READ_BIT);
                    // wapDISBA → RO (same per-pair DST→RO upload contract; created only when gme AND bidir,
                    // so this barrier only fires when the bwd dissidence image exists).
                    if(cfg.gme&&use_bidir) img_barrier(c,wapDISBA.img,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,0,VK_ACCESS_SHADER_READ_BIT);
                    // wapPERA → RO. Created unconditionally with WAP (binding-8 completeness), so this fires
                    // whenever the image exists. The per-pair upload transitions DST→RO when use_inertia;
                    // off-inertia it stays this initial RO state and is never sampled.
                    if(wapPERA.img) img_barrier(c,wapPERA.img,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,0,VK_ACCESS_SHADER_READ_BIT);
                    // wapC2A → RO. Created only with use_ambig; the per-pair upload transitions DST→RO when
                    // use_ambig. When ambig is off it is never created — this fires only when the candidate
                    // image exists (and the binding is then placeholder-bound to wMV.view).
                    if(wapC2A.img) img_barrier(c,wapC2A.img,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,0,VK_ACCESS_SHADER_READ_BIT);
                    // wapMVTA → RO. Created only with cfg.vblend; the per-pair upload transitions DST→RO when
                    // cfg.vblend. When vblend is off it is never created — this fires only when the MV-target
                    // image exists (and the binding is then placeholder-bound to wMV.view).
                    if(wapMVTA.img) img_barrier(c,wapMVTA.img,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,0,VK_ACCESS_SHADER_READ_BIT);
                    // wapPrevOutA → RO (the warp samples binding 13 in SHADER_READ_ONLY). Created only with
                    // cfg.ts_smooth>0; the per-tick copy-back transitions it RO→TRANSFER_DST→RO each tick.
                    // When ts-smooth is off it is never created — this fires only when the prev-output history
                    // image exists (and the binding is then placeholder-bound to wCur.view). The first tick
                    // samples this UNDEFINED-content-but-RO image (garbage history) — harmless: the copy-back
                    // populates it from tick 1's output, and ts_smooth-gated garbage masking is perceptual anyway.
                    if(wapPrevOutA.img) img_barrier(c,wapPrevOutA.img,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,0,VK_ACCESS_SHADER_READ_BIT);
                    // wapFIELDA → GENERAL (the fill's AND the warp's imageLoad target; a STORAGE r32ui image
                    // read in GENERAL). The per-pair upload transitions DST→GENERAL each pair; this fires only
                    // when --afill/--bg-snap created the image. When neither did, the 1×1 r32ui placeholder (the
                    // bound binding-11 view) takes the GENERAL transition so the statically-used storage
                    // descriptor has a valid layout at dispatch (never sampled: bg_snap_on=0).
                    if(wapFIELDA.img)        img_barrier(c,wapFIELDA.img, VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_GENERAL,0,VK_ACCESS_SHADER_READ_BIT);
                    else if(wapFIELDph.img)  img_barrier(c,wapFIELDph.img,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_GENERAL,0,VK_ACCESS_SHADER_READ_BIT);
                });
                std::printf("[ra] warp-at-presenter: ACTIVE — F ships MV+SAD; A re-warps per tick at exact phase (warper=A)\n");
                // The field VISUALIZER pipeline (2 STORAGE images: wOut read-write in-place,
                // wapFIELDA readonly). Created after the warp pipeline; on failure clear cfg.afill (the per-pair
                // field upload + the fill dispatch both gate on it — graceful degrade, the WAP path keeps running
                // without the tint). cfg.afill is true here only if wapFIELDA + hFIELD_a both succeeded above.
                if(cfg.afill){
                    const std::vector<uint32_t> spvfill(kWapFillSpv.begin(),kWapFillSpv.end());
                    if(!fillpipe_create(WD,wOut.view,wapFIELDA.view,wMV.view,spvfill,fillPipeA)){
                        std::printf("[ra] WAP fill (--afill) pipeline failed — disabling the field visualizer\n");
                        fillpipe_destroy(WD,fillPipeA); cfg.afill=false;
                    } else {
                        std::printf("[ra] --afill: ACTIVE — A tints the iGPU contour band onto the present (strength=%.2f, edge_norm=%.3f, mv-gate=%.1fpx still-pixel)\n",cfg.afill_strength,cfg.afill_edge_norm,cfg.afill_mv_gate);
                    }
                }
                // One-time note — bidir doubles B's per-pair match+pyramid work (a second backward
                // record_optical_flow). At ≤60fps source the span machinery absorbs it; the fg-auto
                // per-pair EMA (t_fuse_ema) includes both passes (reported when --fg-factor auto).
                if(use_bidir) std::printf("[ra] bidir: ACTIVE — F records TWO flows/pair (fwd + bwd); ships MV_fwd+SAD+MV_bwd; occl=%.2f px (B pair match ~2x)\n",cfg.occl_thresh);
                // ── MV vector-median scratch + pipeline (A) ─────────────────────
                // ONE RG16F scratch (mvw×mvh), STORAGE (median write) | TRANSFER_SRC (copy back into
                // the WAP MV image after each pass). The pipeline samples a WAP MV image (RO) into the
                // scratch (GENERAL); two descriptor sets (fwd + bwd) share the one scratch. Disable on
                // any failure (the WAP path keeps running unfiltered — a graceful degrade, not a crash).
                // The pass runs for --mv-median OR --mv-guided (run_median_pass). The pipeline also
                // samples cur_real (wCur.view, binding 2) and carries a sim_thresh push: 0 = blind
                // median, >0 = color-weighted consensus. On any failure, BOTH flags fall back to
                // unfiltered (graceful degrade, not a crash).
                if(use_mv_median||use_mv_guided){
                    const std::vector<uint32_t> spvmd(kMvMedianSpv.begin(),kMvMedianSpv.end());
                    if(!img_create(WD,mvw,mvh,VK_FORMAT_R16G16_SFLOAT,VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT,wapMVScratchA)
                       || !med_create(WD,wMV.view,wMVB.view,wCur.view,wapMVScratchA.view,/*with_mvb=*/use_bidir,spvmd,medPipe)){
                        std::printf("[ra] mv-median/guided pipeline failed — disabling the consensus pass\n");
                        med_destroy(WD,medPipe); img_destroy(WD,wapMVScratchA); use_mv_median=false; use_mv_guided=false;
                    } else {
                        // Scratch → GENERAL once (the median dispatch's storage target; the per-pass
                        // chain returns it to GENERAL after the copy-back read, mirroring the upload barriers).
                        oneshot(WD,[&](VkCommandBuffer c){
                            img_barrier(c,wapMVScratchA.img,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_GENERAL,0,VK_ACCESS_SHADER_WRITE_BIT);
                        });
                        if(use_mv_guided) std::printf("[ra] mv-guided: ACTIVE — color-weighted 3x3 consensus on the WAP MV field%s (sim=%.2f, contour-membership) + bilateral primary fetch in warp\n",use_bidir?"(s incl. bwd)":"",cfg.mv_sim);
                        else              std::printf("[ra] mv-median: ACTIVE — 3x3 component-wise vector-median on the WAP MV field%s (flat-content consensus)\n",use_bidir?"(s incl. bwd)":"");
                    }
                }
                // --dump is the pre-generated-grid BMP diagnostic; WAP synthesises on the fly with no
                // hostI buffers to dump → unsupported.
                if(cfg.dump_n>0){ std::printf("[ra] dump unsupported in warp-at-presenter v1\n"); cfg.dump_n=0; }
            }
        }
    }
    // Re-finalize use_gme — cfg.gme may have been cleared by a DIS bridge/image failure above, and use_wap
    // by a WAP pipeline failure. The fit + mask + warp consumption all require both live.
    use_gme=cfg.gme&&use_wap;
    if(use_gme) std::printf("[ra] gme: ACTIVE — global affine model fitted per pair (F, IRLS 2-iter); dissidence mask → wapDISA; model rescue%s + fill-div assist%s\n",
                            use_rescue?"":" (off: needs --rescue)", use_fill_div?"":" (off: needs --fill-div)");
    // ── gme-gpu: build the device-B affine-fit pipeline set. Requires the FINAL use_gme (the dis bridges
    // + model readback only exist when gme is live) AND cfg.gme_gpu (the B-side bridges were imported
    // above only then). The OFP MV/SAD grid is ofp.motion_width()/height(). want_bwd = use_bidir (the bwd
    // anchor needs its own dis sets). On any create failure we clear use_gme_gpu → the CPU gme_fit_affine
    // path runs (byte-identical). Off = zero pipeline allocated.
    use_gme_gpu=use_gme&&cfg.gme_gpu;
    // gme-gpu is DEFAULT ON, so guard the device-B precondition explicitly: the B-side pipelines + dis
    // bridges + model readback all live on device B (the 1080 Ti). On a topology WITHOUT a usable device B
    // (B.dev null — the upstream A+B requirement at the LUID/vdev_create gate normally exits first, this is
    // the belt-and-suspenders guard so default-ON can never crash here), or if cfg.gme_gpu was already
    // cleared by a B-side bridge-import failure above, route to the CPU gme_fit_affine (byte-identical)
    // instead of building the B pipelines.
    if(use_gme&&cfg.gme_gpu&&B.dev==VK_NULL_HANDLE){
        std::printf("[ra] --gme-gpu: device B unavailable -> CPU gme fallback\n");
        use_gme_gpu=false; cfg.gme_gpu=false; cfg.gme_gpu_verify=false;
    }
    if(use_gme_gpu){
        const uint32_t gmvw=ofp.motion_width(), gmvh=ofp.motion_height();
        VkBuffer disF[kGenRing]={}, disB[kGenRing]={};
        for(int _g=0;_g<kGenRing;++_g){ disF[_g]=hDIS_b[_g].buf; disB[_g]=hDISB_b[_g].buf; }
        const std::vector<uint32_t> rspv(kGmeReduceSpv.begin(),kGmeReduceSpv.end());
        const std::vector<uint32_t> sspv(kGmeSolveSpv.begin(),kGmeSolveSpv.end());
        const std::vector<uint32_t> dspv(kGmeDissidenceSpv.begin(),kGmeDissidenceSpv.end());
        if(!gme_create(B,ofp.motion_image(),ofp.sad_field_image(),disF,use_bidir?disB:nullptr,
                       (VkDeviceSize)gmvw*gmvh,use_bidir,rspv,sspv,dspv,gmePipe)){
            std::printf("[ra] gme-gpu: pipeline create failed — falling back to CPU gme\n");
            gme_destroy(B,gmePipe); use_gme_gpu=false;
        } else {
            std::printf("[ra] gme-gpu: ACTIVE — gme_fit_affine offloaded onto device B (reduce->solve x3 + dissidence, ONE B cmd buffer); only the 6-float model reads back%s\n",
                        cfg.gme_gpu_verify?" (+ --gme-gpu-verify: CPU cross-check per pair)":"");
        }
    }
    // Re-finalize use_ambig — it requires the FINAL use_gme (the referee is gme_model_mv). If gme was
    // cleared above (a DIS bridge/image failure) the candidate field has no model to arbitrate against,
    // so ambiguity falls to OFF (byte-identical — the push flag is 0, the binding stays a valid placeholder).
    // use_ambig already folded its own bridge/image-alloc failures; ANDing the final use_gme closes the gap.
    use_ambig=use_ambig&&use_gme;
    if(use_ambig) std::printf("[ra] ambig: ACTIVE — B emits the second-best candidate per pair (cand_image → wapC2A binding 10); the warp arbitrates SAD ties on background/texture interiors vs the gme model referee\n");
    // Re-finalize use_matte — the matte consumes wapDISA (the object layer) + gme_model_mv() (the
    // background layer), both gated by use_gme. If gme was cleared above (DIS-image failure) the matte
    // falls back to OFF (byte-identical) with a printed note — never a half-built matte.
    use_matte=cfg.matte&&use_gme;
    if(cfg.matte&&!use_gme) std::printf("[ra] --matte disabled — its layers come from gme (the dissidence mask + the global model), which is not active\n");
    if(use_matte) std::printf("[ra] matte: ACTIVE — boundary-first two-layer compositing; advected-dissidence binary matte (thr=%.2f, r>%.1fpx); BACKGROUND → %s model layer%s\n",
                            cfg.matte_thresh, cfg.matte_thresh*255.0f/16.0f, cfg.crescent?"crescent-weighted":"pure", use_mv_guided?" (+color cross-check)":"");
    // Re-finalize use_objects — it clusters the gme dissidence mask + reuses the gme decode, so a gme
    // clear above (DIS bridge/image failure) takes objects with it (the repair has nothing to act on).
    // When live it prints its parameters; the shield note flags the inertia coupling.
    use_objects=cfg.objects&&use_gme;
    if(cfg.objects&&!use_gme) std::printf("[ra] --objects disabled — the inheritance repair acts on the gme dissidence mask, which is not active\n");
    if(use_objects) std::printf("[ra] objects: ACTIVE — connected-component clustering (k=%d, min-mass=%d) + 16-slot temporal identity + %s repair (inh≥%.0fpx, static≤%.0fpx)%s\n",
                            kObjSlots,kObjMinMass,cfg.shapefield?"contour shape-field motion-inheritance":"rigid single-MV motion-inheritance",kObjInhMin,kObjStaticMax,use_inertia?" + HUD shield (persist≥128)":" (no HUD shield — inertia off)");
    // Re-finalize use_memory — the scene-holon silhouette memory rides the object-holon (it advects +
    // merges the dissidence prior INTO the masks the repair then clusters), so a gme/objects clear above
    // takes the memory with it (nothing to advect, no holon to keep alive). When live it prints its decay
    // constant; --no-memory leaves the masks fresh-only.
    use_memory = use_objects && cfg.scene_memory;   // re-finalized after the gme/objects clears above
    if(cfg.scene_memory&&!use_objects) std::printf("[ra] --memory disabled — the silhouette memory rides the object-holon, which is not active\n");
    if(use_memory) std::printf("[ra] memory: ACTIVE — persistent CUR-anchored silhouette prior advected by the fwd MV field; merged max(fresh, %.2f·prior) before clustering; expiration guards confident background\n",kPriorDecay);

    // ── Command buffers + fences + semaphores ─────────────────────────────────
    {
        VkCommandBufferAllocateInfo ci2{}; ci2.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO; ci2.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY; ci2.commandBufferCount=1;
        // single-GPU: command pools are EXTERNALLY-SYNCHRONIZED (one thread records at a time). Under
        // single_gpu the 3 threads P(present)/C(convert)/F(flow) would all record from A.pool → pool race
        // + UB. Give C and F their OWN A-side pools (A.qfam = A.q2's family, the q2mode=1 guarantee); P
        // keeps A.pool. The a_q2_mtx still serializes the SHARED A.q2 submits (a queue and a pool are
        // distinct objects). OFF (single_gpu false) → pools stay null → byte-identical.
        if(single_gpu){ VkCommandPoolCreateInfo cpsg{}; cpsg.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO; cpsg.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; cpsg.queueFamilyIndex=A.qfam; vkCreateCommandPool(A.dev,&cpsg,nullptr,&a_cpool_sg); vkCreateCommandPool(A.dev,&cpsg,nullptr,&a_fpool_sg); }
        ci2.commandPool=single_gpu?a_cpool_sg:A.pool; vkAllocateCommandBuffers(A.dev,&ci2,&cmdA);
        // FD-route cmdB (B→A under single_gpu). Under single_gpu it submits to A.q2 — same FAMILY as A.q
        // (q2mode=1, the guaranteed split-queue), so FD.pool (=A.pool, family qfam) is the correct pool.
        ci2.commandPool=single_gpu?a_fpool_sg:FD.pool; vkAllocateCommandBuffers(FD.dev,&ci2,&cmdB);   // F's own A-pool under single_gpu (pool-race fix)
        // Second B cmd buffer for the bwd pass (overlap: submit bwd, fit fwd on CPU, wait bwd). B.pool/B.q
        // (same family as cmdB — F is B.q's only submitter, externally synchronized). Allocated only when
        // bidir is live (the sole bwd consumer).
        if(use_bidir){ci2.commandPool=single_gpu?a_fpool_sg:FD.pool; vkAllocateCommandBuffers(FD.dev,&ci2,&cmdB_bwd);}   // FD-route + pool-race fix
        // The fwd-pass ping-pong cmd buffers (opt-in, WAP only). B.pool/B.q (same family as cmdB — F is
        // B.q's only submitter). Allocated only on the opt-in path; the serial path is unchanged.
        if(use_wap&&cfg.fwd_pipeline){ci2.commandPool=single_gpu?a_fpool_sg:FD.pool; for(int _k=0;_k<2;++_k) vkAllocateCommandBuffers(FD.dev,&ci2,&cmdB_fwd[_k]);}   // FD-route + pool-race fix
        // --fwd-prestage: the prestage ping-pong cmd buffers — SAME F flow pool as cmdB (a_fpool_sg under
        // single_gpu, else FD.pool), allocated ONLY when use_fwd_prestage. Idle otherwise (serial path
        // records the copy inline in cmdF) → byte-unchanged.
        if(use_fwd_prestage){ci2.commandPool=single_gpu?a_fpool_sg:FD.pool; for(int _k=0;_k<2;++_k) vkAllocateCommandBuffers(FD.dev,&ci2,&cmdF_pre[_k]);}   // FD-route + pool-race fix (mirrors cmdB_fwd)
        // cmdG (thread C's convert) is recorded from G.pool2 and submits to G.q2 — family-bound to qfam2
        // (=G.pool/G.q in the shared fallback). Separates C from P's G.pool.
        // Only the (forced-off) upscale or the igpu-convert path needs G cmd buffers.
        if(use_upscale||use_igpu_convert){ci2.commandPool=G.pool2; vkAllocateCommandBuffers(G.dev,&ci2,&cmdG);}
        // cmdGP is P-thread's G cmd buffer — used only by the (forced-off) upscale path now.
        if(use_upscale){ci2.commandPool=G.pool; vkAllocateCommandBuffers(G.dev,&ci2,&cmdGP);}
        // F-thread pfg OFP on A (separate pool entry so C's cmdA is uncontested).
        if(pfg_enabled){ci2.commandPool=A.pool; vkAllocateCommandBuffers(A.dev,&ci2,&cmdA_fg);}
        // F's overlapped interp copy-outs run on B.q2 — one cmd buffer per interp slot, recorded from
        // B.pool2 (family-bound to qfam2; the family trap).
        if(b_q2_split) for(int _k=0;_k<kMaxInterp;++_k){ci2.commandPool=B.pool2; vkAllocateCommandBuffers(B.dev,&ci2,&cmdB2[_k]);}
        // P-thread's A-side bridge cmd buffer (blit/warp → bridge_img) — the present path. A.pool (P
        // submits on A.q — the present-source build, single-threaded on P).
        {ci2.commandPool=A.pool; vkAllocateCommandBuffers(A.dev,&ci2,&cmdBridge);}
    }
    {
        VkFenceCreateInfo fi{}; fi.sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        vkCreateFence(A.dev,&fi,nullptr,&fA);
        vkCreateFence(FD.dev,&fi,nullptr,&fB); if(use_upscale||use_igpu_convert) vkCreateFence(G.dev,&fi,nullptr,&fG);   // FD-route fB (B→A)
        if(use_bidir) vkCreateFence(FD.dev,&fi,nullptr,&fB2);   // bwd-submit fence (FD-route)
        if(use_wap&&cfg.fwd_pipeline) for(int _k=0;_k<2;++_k) vkCreateFence(FD.dev,&fi,nullptr,&fB_fwd[_k]);  // fwd-pipeline ping-pong fences (FD-route)
        if(use_fwd_prestage) for(int _k=0;_k<2;++_k) vkCreateFence(FD.dev,&fi,nullptr,&fF_pre[_k]);  // fwd-prestage ping-pong fences (FD-route, mirrors fB_fwd)
        if(use_upscale) vkCreateFence(G.dev,&fi,nullptr,&fGP);  // upscale-only
        if(pfg_enabled) vkCreateFence(A.dev,&fi,nullptr,&fA_fg);      // F-thread pfg
        if(b_q2_split) for(int _k=0;_k<kMaxInterp;++_k) vkCreateFence(B.dev,&fi,nullptr,&tfb[_k]);  // overlapped interp copy-out fences
        vkCreateFence(A.dev,&fi,nullptr,&fBridge);  // the present-path fence
    }
    // There is NO VkSwapchain on any device — PresentSurface presents through DirectComposition; the
    // producer-side bridge below IS the present path.
    {
        // ── Producer side: D3D11 shared bridge texture + VK-A import ──────────
        // One BGRA8 shared texture at the present-monitor extent (= the surface backbuffer), on d.dev (the
        // capture D3D11 device — LUID-matched to A / the 4090, the panel owner). The bridge VK image is
        // B8G8R8A8 so the present-source blit (rgba8→bgra8 channel reinterpretation) lands byte-identically,
        // and the pillar CopyResource (BGRA8→BGRA8) is a valid same-format full-resource copy.
        bridge_w=(uint32_t)(pc.right-pc.left); bridge_h=(uint32_t)(pc.bottom-pc.top);
        // Keyed-mutex path iff A's VK device exposes VK_KHR_win32_keyed_mutex (verified at device
        // create). If absent: a no-KM shared texture + CPU-fence ordering (P waits fBridge before
        // submit() — same thread, ordering holds). The pillar skips AcquireSync when the producer
        // texture carries no keyed mutex (ensure_imported leaves keyed==null).
        bridge_use_km = A.has_keyed_mutex && A.extmem_win32_enabled;
        // The SINGLE bridge-format decision driven by cfg.present_format. When the present is FP16
        // (--present-fp16/--hdr), the producer's shared bridge texture MUST be FP16 too — else the pillar's
        // one CopyResource(FP16 backbuffer, this texture) is a D3D11 format-mismatch (no-op/validation error
        // → black/garbage present). The wapOutA->bridge step is a vkCmdBlitImage (VK_FILTER_LINEAR, below),
        // which CONVERTS rgba8->fp16, so widening the bridge is sufficient (no FP16 warp math → the Pascal
        // 1/64 floor is honored; the warp output is sampled and the blit writes FP16 texels). present_format
        // ==0 → the literal BGRA8 line → BYTE-IDENTICAL. A post-create disagreement (FP16 swapchain refused
        // on this rig but the bridge built FP16) is caught at the surface create and degrades to a named
        // clean quit.
        const bool bridge_fp16 = (cfg.present_format==1);
        const DXGI_FORMAT bridge_dxgi_fmt = bridge_fp16 ? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_B8G8R8A8_UNORM;
        const VkFormat    bridge_vk_fmt   = bridge_fp16 ? VK_FORMAT_R16G16B16A16_SFLOAT  : VK_FORMAT_B8G8R8A8_UNORM;
        D3D11_TEXTURE2D_DESC btd{}; btd.Width=bridge_w; btd.Height=bridge_h; btd.MipLevels=1; btd.ArraySize=1;
        btd.Format=bridge_dxgi_fmt; btd.SampleDesc.Count=1; btd.Usage=D3D11_USAGE_DEFAULT;
        btd.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET;
        btd.MiscFlags=(UINT)D3D11_RESOURCE_MISC_SHARED_NTHANDLE|(bridge_use_km?(UINT)D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX:0u);
        if(FAILED(d.dev->CreateTexture2D(&btd,nullptr,&bridge_tex))||!bridge_tex){ std::printf("[ra] bridge: CreateTexture2D failed\n"); goto done; }
        { IDXGIResource1* dr1=nullptr;
          if(SUCCEEDED(bridge_tex->QueryInterface(__uuidof(IDXGIResource1),(void**)&dr1))&&dr1){
              dr1->CreateSharedHandle(nullptr,DXGI_SHARED_RESOURCE_READ|DXGI_SHARED_RESOURCE_WRITE,nullptr,&bridge_nt); dr1->Release(); }
          if(!bridge_nt){ std::printf("[ra] bridge: CreateSharedHandle failed\n"); goto done; } }
        if(bridge_use_km) bridge_tex->QueryInterface(__uuidof(IDXGIKeyedMutex),(void**)&bridge_km_d3d);
        // Import the D3D11 texture into VK-A as a B8G8R8A8 image (dedicated alloc; the win32 NT
        // handle import). vkGetImageMemoryRequirements after the external image gives the size; the
        // win32 handle-properties query gives the valid memory-type bits.
        {
            VkExternalMemoryImageCreateInfo emi{}; emi.sType=VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
            emi.handleTypes=VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT;
            VkImageCreateInfo ici{}; ici.sType=VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO; ici.pNext=&emi;
            ici.imageType=VK_IMAGE_TYPE_2D; ici.format=bridge_vk_fmt; ici.extent={bridge_w,bridge_h,1};   // match the D3D11 bridge format (FP16 when --present-fp16; BGRA8 default = byte-identical)
            ici.mipLevels=1; ici.arrayLayers=1; ici.samples=VK_SAMPLE_COUNT_1_BIT; ici.tiling=VK_IMAGE_TILING_OPTIMAL;
            ici.usage=VK_IMAGE_USAGE_TRANSFER_DST_BIT; ici.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
            if(vkCreateImage(A.dev,&ici,nullptr,&bridge_img.img)!=VK_SUCCESS){ std::printf("[ra] bridge: vkCreateImage failed\n"); goto done; }
            VkMemoryRequirements mr; vkGetImageMemoryRequirements(A.dev,bridge_img.img,&mr);
            VkMemoryWin32HandlePropertiesKHR wp{}; wp.sType=VK_STRUCTURE_TYPE_MEMORY_WIN32_HANDLE_PROPERTIES_KHR;
            if(!A.pfnGetMemWin32||A.pfnGetMemWin32(A.dev,VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT,bridge_nt,&wp)!=VK_SUCCESS)
                { std::printf("[ra] bridge: vkGetMemoryWin32HandleProperties failed\n"); goto done; }
            const uint32_t mt=pick_mem(A.mp,mr.memoryTypeBits&wp.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            if(mt==UINT32_MAX){ std::printf("[ra] bridge: no device-local memory type for the import\n"); goto done; }
            VkImportMemoryWin32HandleInfoKHR imp{}; imp.sType=VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR;
            imp.handleType=VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT; imp.handle=bridge_nt;
            VkMemoryDedicatedAllocateInfo ded{}; ded.sType=VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO; ded.image=bridge_img.img; imp.pNext=&ded;
            VkMemoryAllocateInfo mai{}; mai.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO; mai.pNext=&imp; mai.allocationSize=mr.size; mai.memoryTypeIndex=mt;
            if(vkAllocateMemory(A.dev,&mai,nullptr,&bridge_mem)!=VK_SUCCESS){ std::printf("[ra] bridge: vkAllocateMemory(import) failed\n"); goto done; }
            vkBindImageMemory(A.dev,bridge_img.img,bridge_mem,0);
        }
        std::printf("[ra] present-surface: bridge %ux%u %s on A (4090), keyed-mutex sync=%s\n",
            bridge_w,bridge_h,bridge_fp16?"FP16(R16G16B16A16_FLOAT, HDR)":"BGRA8",bridge_use_km?"VK_KHR_win32_keyed_mutex":"CPU-fence (no KM)");
        // ── --async-present: the SECOND bridge slot ──────────────────────────
        // Mirror slot-0 EXACTLY (same bridge_w/bridge_h, same D3D11_TEXTURE2D_DESC, same CreateSharedHandle,
        // same VK external-image import). Alternating the two NT handles makes PresentSurface re-import each
        // present (a µs-scale OpenSharedResource1 — ensure_imported re-imports only on handle change) which is
        // fine on the async path. Allocate the slot-1 cmd buffer + fence here too. On ANY failure: print,
        // release whatever was partially built, and FORCE cfg.async_present=false (the off path runs
        // byte-identically) — never goto done, never crash.
        if(cfg.async_present){
            bool ok1=true;
            D3D11_TEXTURE2D_DESC btd1{}; btd1.Width=bridge_w; btd1.Height=bridge_h; btd1.MipLevels=1; btd1.ArraySize=1;
            btd1.Format=bridge_dxgi_fmt; btd1.SampleDesc.Count=1; btd1.Usage=D3D11_USAGE_DEFAULT;   // slot-1 mirrors slot-0's format (FP16 when --present-fp16; BGRA8 default = byte-identical)
            btd1.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET;
            btd1.MiscFlags=(UINT)D3D11_RESOURCE_MISC_SHARED_NTHANDLE|(bridge_use_km?(UINT)D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX:0u);
            if(FAILED(d.dev->CreateTexture2D(&btd1,nullptr,&bridge_tex1))||!bridge_tex1){ std::printf("[ra] bridge[1]: CreateTexture2D failed — falling back to sync present\n"); ok1=false; }
            if(ok1){ IDXGIResource1* dr1=nullptr;
                if(SUCCEEDED(bridge_tex1->QueryInterface(__uuidof(IDXGIResource1),(void**)&dr1))&&dr1){
                    dr1->CreateSharedHandle(nullptr,DXGI_SHARED_RESOURCE_READ|DXGI_SHARED_RESOURCE_WRITE,nullptr,&bridge_nt1); dr1->Release(); }
                if(!bridge_nt1){ std::printf("[ra] bridge[1]: CreateSharedHandle failed — falling back to sync present\n"); ok1=false; } }
            if(ok1&&bridge_use_km) bridge_tex1->QueryInterface(__uuidof(IDXGIKeyedMutex),(void**)&bridge_km_d3d1);
            if(ok1){
                VkExternalMemoryImageCreateInfo emi1{}; emi1.sType=VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
                emi1.handleTypes=VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT;
                VkImageCreateInfo ici1{}; ici1.sType=VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO; ici1.pNext=&emi1;
                ici1.imageType=VK_IMAGE_TYPE_2D; ici1.format=bridge_vk_fmt; ici1.extent={bridge_w,bridge_h,1};   // slot-1 mirrors slot-0's VK format
                ici1.mipLevels=1; ici1.arrayLayers=1; ici1.samples=VK_SAMPLE_COUNT_1_BIT; ici1.tiling=VK_IMAGE_TILING_OPTIMAL;
                ici1.usage=VK_IMAGE_USAGE_TRANSFER_DST_BIT; ici1.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
                if(vkCreateImage(A.dev,&ici1,nullptr,&bridge_img1.img)!=VK_SUCCESS){ std::printf("[ra] bridge[1]: vkCreateImage failed — falling back to sync present\n"); ok1=false; }
                else {
                    VkMemoryRequirements mr1; vkGetImageMemoryRequirements(A.dev,bridge_img1.img,&mr1);
                    VkMemoryWin32HandlePropertiesKHR wp1{}; wp1.sType=VK_STRUCTURE_TYPE_MEMORY_WIN32_HANDLE_PROPERTIES_KHR;
                    if(!A.pfnGetMemWin32||A.pfnGetMemWin32(A.dev,VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT,bridge_nt1,&wp1)!=VK_SUCCESS)
                        { std::printf("[ra] bridge[1]: vkGetMemoryWin32HandleProperties failed — falling back to sync present\n"); ok1=false; }
                    else {
                        const uint32_t mt1=pick_mem(A.mp,mr1.memoryTypeBits&wp1.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
                        if(mt1==UINT32_MAX){ std::printf("[ra] bridge[1]: no device-local memory type — falling back to sync present\n"); ok1=false; }
                        else {
                            VkImportMemoryWin32HandleInfoKHR imp1{}; imp1.sType=VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR;
                            imp1.handleType=VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT; imp1.handle=bridge_nt1;
                            VkMemoryDedicatedAllocateInfo ded1{}; ded1.sType=VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO; ded1.image=bridge_img1.img; imp1.pNext=&ded1;
                            VkMemoryAllocateInfo mai1{}; mai1.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO; mai1.pNext=&imp1; mai1.allocationSize=mr1.size; mai1.memoryTypeIndex=mt1;
                            if(vkAllocateMemory(A.dev,&mai1,nullptr,&bridge_mem1)!=VK_SUCCESS){ std::printf("[ra] bridge[1]: vkAllocateMemory(import) failed — falling back to sync present\n"); ok1=false; }
                            else vkBindImageMemory(A.dev,bridge_img1.img,bridge_mem1,0);
                        }
                    }
                }
            }
            // slot-1 cmd buffer + fence on A.
            if(ok1){
                VkCommandBufferAllocateInfo cbi1{}; cbi1.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
                cbi1.commandPool=A.pool; cbi1.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY; cbi1.commandBufferCount=1;
                if(vkAllocateCommandBuffers(A.dev,&cbi1,&cmdBridge1)!=VK_SUCCESS){ std::printf("[ra] bridge[1]: vkAllocateCommandBuffers failed — falling back to sync present\n"); ok1=false; }
            }
            if(ok1){
                VkFenceCreateInfo fi1{}; fi1.sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
                if(vkCreateFence(A.dev,&fi1,nullptr,&fBridge1)!=VK_SUCCESS){ std::printf("[ra] bridge[1]: vkCreateFence failed — falling back to sync present\n"); ok1=false; }
            }
            // slot-0's DEDICATED async cmd buffer + fence (A.pool / A.dev). Without these the async warp
            // would record into the shared cmdBridge/fBridge that wap_upload resets per-pair → in-flight
            // reset → GPU fault. With them, the async warp slots {A0, 1} are disjoint from wap_upload's
            // cmdBridge/fBridge.
            if(ok1){
                VkCommandBufferAllocateInfo cbiA{}; cbiA.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
                cbiA.commandPool=A.pool; cbiA.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY; cbiA.commandBufferCount=1;
                if(vkAllocateCommandBuffers(A.dev,&cbiA,&cmdBridgeA0)!=VK_SUCCESS){ std::printf("[ra] bridge[A0]: vkAllocateCommandBuffers failed — falling back to sync present\n"); ok1=false; }
            }
            if(ok1){
                VkFenceCreateInfo fiA{}; fiA.sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
                if(vkCreateFence(A.dev,&fiA,nullptr,&fBridgeA0)!=VK_SUCCESS){ std::printf("[ra] bridge[A0]: vkCreateFence failed — falling back to sync present\n"); ok1=false; }
            }
            if(!ok1){
                // Tear down whatever slot-1 partially built; the teardown guards below also cover these, but
                // releasing now keeps the off-path state clean and avoids a stale half-slot living to teardown.
                if(bridge_img1.img){ vkDestroyImage(A.dev,bridge_img1.img,nullptr); bridge_img1.img=VK_NULL_HANDLE; }
                if(bridge_mem1){ vkFreeMemory(A.dev,bridge_mem1,nullptr); bridge_mem1=VK_NULL_HANDLE; }
                if(fBridge1){ vkDestroyFence(A.dev,fBridge1,nullptr); fBridge1=VK_NULL_HANDLE; }
                if(fBridgeA0){ vkDestroyFence(A.dev,fBridgeA0,nullptr); fBridgeA0=VK_NULL_HANDLE; }  // (cmdBridgeA0 is pool-freed)
                if(bridge_km_d3d1){ bridge_km_d3d1->Release(); bridge_km_d3d1=nullptr; }
                if(bridge_tex1){ bridge_tex1->Release(); bridge_tex1=nullptr; }
                if(bridge_nt1){ CloseHandle(bridge_nt1); bridge_nt1=nullptr; }
                cfg.async_present=false;   // force the off path → byte-identical to today
            } else {
                std::printf("[ra] present-surface: async-present slot-1 ready (2 bridge slots; non-blocking present + drop-interpolated)\n");
            }
        }
        // ── --real-fast-path co-arm guard ────────────────────────────────
        // --rfp presents the real through the DEDICATED async bslot[] path (the bslot[1] slot + cmdBridgeA0/
        // fBridgeA0), which exist ONLY when cfg.async_present is live. If async-present's slot-1 creation
        // FAILED above (cfg.async_present force-OFF at the failure path), the dedicated slot is gone and the
        // only present helper left is the SYNCHRONOUS do_present_P → resetting the SHARED cmdBridge/fBridge
        // would be a use-after-reset / device-lost. So REFUSE to arm --rfp when async is not live (mirror the
        // use_wap-requires disable pattern). The parse auto-enables async, so this only fires when the async
        // CREATE itself failed — a clean, printed refusal, never a crash.
        if(cfg.real_fast_path && !cfg.async_present){
            cfg.real_fast_path=false;
            std::printf("[ra] --real-fast-path: async-present is NOT live (slot-1 create failed) → DISABLING --rfp (the dedicated non-blocking bslot path is required; the synchronous present would risk a use-after-reset on the shared bridge buffers). Running the standard D-anchored present.\n");
        }
        // --motion-fallback co-arm guards — mirror the --rfp pattern: the fast-motion real fallback uses the
        // SAME dedicated non-blocking bslot path, so it needs async-present live (auto-enabled by the flag;
        // this only fires if the async CREATE failed) and the gme dispersion signal, and (like --rfp) it
        // cannot present an upscaled real on the bslot path. Disabling here is a clean printed refusal, never
        // a crash.
        if(cfg.motion_fallback && !cfg.async_present){
            cfg.motion_fallback=false;
            std::printf("[ra] --motion-fallback: async-present is NOT live (slot-1 create failed) -> DISABLING (the fast-motion real fallback needs the dedicated non-blocking bslot path).\n");
        }
        if(cfg.motion_fallback && !use_gme){
            cfg.motion_fallback=false;
            std::printf("[ra] --motion-fallback: needs the gme dispersion signal but --no-gme is set -> DISABLING.\n");
        }
        if(cfg.motion_fallback && use_upscale){
            cfg.motion_fallback=false;
            std::printf("[ra] --motion-fallback: --upscale is active -> DISABLING (the bslot real present is not upscaled, mirrors --rfp).\n");
        }
    }

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
        std::atomic<uint64_t> c_seq{0}, f_seq{0};
        // F publishes WHICH capture its set's pair-cur is (seq/slot/t_cap), written BEFORE the f_seq
        // publish (the seq_cst fetch_add/load pair orders them), so P can present THE MATCHING real:
        // interp(N-1→N) must be followed by real N — not by the newest capture N+1/N+2, which would make
        // displayed content NON-MONOTONIC.
        uint64_t f_pair_cseq_a[NS]{}; int f_pair_slot_a[NS]{}; double f_pair_tcap_a[NS]{};
        // span = how many source frames F's pair covers (drops → >1). The set's interps represent span×T
        // of motion; P paces them over span×T (kills the walking 2× pulse). per-set N actually generated
        // (auto may pick <cap); P paces with THIS N.
        uint64_t f_pair_span_a[NS]{}; int f_pair_n_a[NS]{};
        // The per-pair global affine model F fits on the CPU travels to P through THIS SAME publish channel
        // (written by f_gen index BEFORE the f_seq.fetch_add that publishes the set; P reads them after
        // observing the new f_seq — the seq_cst fetch_add/load ordering covers it, no new synchronization).
        // 6 params {a,b,c,d,e,f} + a validity flag (0 until the first fit, so P pushes gme_on=0 for the rare
        // unfitted generation). Plain arrays, F-written / P-read.
        float f_pair_gme_a[NS][6]{}; int f_pair_gme_valid_a[NS]{};
        // The BACKWARD (cur-anchored) affine model F fits on the bwd MV field rides the SAME publish
        // channel (F-written before the f_seq.fetch_add, P-read after — the seq_cst fetch_add/load ordering
        // covers it, no new synchronization, exactly the fwd pattern). NOT pushed to the shader (the bwd
        // DISSIDENCE MASK carries the leading-edge information; the bwd model itself is published for a
        // future cur-anchored background layer). Its validity tracks the fwd model's (both fits run in the
        // same iteration when gme+bidir are live).
        float f_pair_gme_bwd_a[NS][6]{};
        // The per-pair ANCHORED MATTE MASSES — the expected-mass terms. F counts, on the SAME CPU walk
        // that fills hostDIS/hostDISB, the number of MV blocks whose dissidence byte exceeds
        // matte_thresh-quantized (byte > matte_thresh·255 — the mask stores min(255,16·r), and the shader
        // classifies OBJECT when byte/255 > matte_thresh, so the byte test mirrors the shader EXACTLY).
        // m_fwd = prev-anchored mass, m_bwd = cur-anchored mass. They ride the SAME F→P publish channel
        // (F-written by f_gen index BEFORE the f_seq.fetch_add, P-read after — the seq_cst fetch_add/load
        // ordering covers it, no new sync, exactly the gme/validity pattern). P forms the PHASE-t expected
        // mass = lerp(m_fwd, m_bwd, t) and compares it against the GPU-measured presented mass to drive the
        // threshold feedback. Block-grid counts (mvw·mvh space) — the GPU counter is in PIXEL space, so the
        // comparison is RATIO-based (err is dimensionless), not an absolute match.
        float f_pair_mfwd_a[NS]{}; float f_pair_mbwd_a[NS]{};
        // --motion-fallback: the per-pair gme DISPERSION (dis% in [0,100]) rides the SAME F->P publish
        // channel (F-written by f_gen BEFORE the f_seq.fetch_add, P-read after — the seq_cst fetch_add/load
        // ordering covers it, no new sync, exactly the gme/matte pattern). P gates the fast-motion
        // real-fallback on it.
        float f_pair_disp_a[NS]{};
        // Per-generation BACKWARD validity. Rides the SAME F→P publish channel (F-written by f_gen index
        // BEFORE the f_seq.fetch_add, P-read after — the seq_cst fetch_add/load ordering covers it, exactly
        // the fwd-validity pattern). 1 when this pair recorded+fit the bwd flow; 0 when F SKIPPED the bwd
        // pass under pressure (the adaptive throttle) OR no bwd ran. When 0, P pushes occl_thresh=0 AND
        // matte_on=0 for that generation — the bidir classification and the dual-anchored matte degrade
        // gracefully for THAT pair only (the fwd warp + the safety net still run). Initialized 1 so the
        // off-bidir build (no skip discipline) behaves as before; P only consults it under use_bidir.
        int f_pair_bwd_valid_a[NS];
        for(int _g=0;_g<NS;++_g) f_pair_bwd_valid_a[_g]=1;
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
        std::atomic<uint64_t> lt_compose_us{0}, lt_copy_us{0}, lt_pickup_us{0}, lt_fpub_us{0}, lt_preflow_us{0}, lt_spin_us{0};
        // P publishes the f_seq value it is currently presenting so F can detect a ring-overwrite hazard:
        // F must not build into the generation P still holds. With kGenRing=3 this only fires during
        // span≥kGenRing stalls but guards the remaining edge.
        std::atomic<uint64_t> p_presenting{0};
        // WGC ring Map outcomes under primary saturation — fb = fell back to the older unread slot
        // (newest's copy not executed yet); miss = both slots unmappable.
        std::atomic<uint64_t> stat_mapfb{0}, stat_mapmiss{0};
        std::atomic<bool> g_quit_threads{false};
        // DD arrival cadence — C timestamps each delivered frame; P reads the arrival-delta EMA for
        // pacing (the processed-cadence ratchets under load) + arr/drop.
        std::atomic<uint64_t> dd_arr_delta_us{0}, dd_arrived{0}, dd_timeouts{0}, dd_lost{0}, dd_present{0};   // dd_timeouts: DDA WAIT_TIMEOUT/s; dd_lost: re-arm events; dd_present: suma AccumulatedFrames = tasa entregada real (superseded por dd_acq en el readout [ra-cap])
        // INGEST-ASYNC: dd_acq = ACQUIRES/s (serial + async) = the reliable `acq=` readout (dd_present reads 0 on NVIDIA).
        // raw_seq/raw_busy/raw_cv/raw_mtx = the acquire↔worker decoupling state (inert unless cfg.ingest_async).
        std::atomic<uint64_t> dd_acq{0}, dd_uniq{0}, raw_seq{0};   // dd_uniq: CAPTURE-DEDUP frames únicos reales/s (el `uniq=` readout)
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
            .raw_seq = raw_seq,
            .dd_acq = dd_acq,
            .dd_uniq = dd_uniq,
            .raw_busy = raw_busy,
            .raw_cv = raw_cv,
            .raw_mtx = raw_mtx,
            .raw_astage_a = raw_astage_a,
            .raw_astage_g = raw_astage_g,
            .raw_tcap = raw_tcap,
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
            .hMVB_a = hMVB_a,
            .hMV_a = hMV_a,
            .hMass_a = hMass_a,
            .hOutD_a = hOutD_a,
            .hPER_a = hPER_a,
            .hPrevD_a = hPrevD_a,
            .hR_g = hR_g,
            .hSAD_a = hSAD_a,
            .hostCurD = hostCurD,
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

        thr_c.join();
        if(thr_cw.joinable()) thr_cw.join();   // INGEST-ASYNC: join BEFORE the convert/Vulkan teardown (it touches cmdA/cmdG/fA/fG + the raw imports)
        thr_f.join(); thr_p.join();

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
    vkDestroyInstance(inst,nullptr);
    d3d_shutdown(d);
    return 0;
}
// Made with my soul - Swately <3
