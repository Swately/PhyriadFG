// framework/render/present/src/PresentSurface.cpp
// PresentSurface — Windows-only implementation (FR-RENDER-1, FR_RENDER_IMPL.md §1).
//
// On non-Windows this file compiles to a small stub TU (the methods return
// ErrorCode::Unavailable) — the etw posture (D-19).
//
// The Win32 body lifts the recipe the STAGE-44a probe validated on the rig
// (framework/render/vulkan/bench/stage44a_present_probe/main.cpp):
//   - window recipe ........ main.cpp:215-223  (the WS_EX_* composition HWND)
//   - swapchain + DComp .... main.cpp:253-270  (CreateSwapChainForComposition +
//                                               IDCompositionDevice, Commit() once)
//   - WDA .................. main.cpp:277-285  (SetWindowDisplayAffinity)
// The hot-path bridge is FR_RENDER_IMPL.md §1.5.2 (keyed mutex + one CopyResource
// + Present(0)). Nothing here is new research — it is a disciplined lift.

#include <phyriad/render/present/PresentSurface.hpp>

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#  define NOMINMAX
#endif
// Target Win10+ so WS_EX_NOREDIRECTIONBITMAP (WINVER>=0x0602) and
// WDA_EXCLUDEFROMCAPTURE (Win10 2004+) are declared. Runtime-present on the rig.
#ifndef WINVER
#  define WINVER 0x0A00
#endif
#ifndef _WIN32_WINNT
#  define _WIN32_WINNT 0x0A00
#endif
#include <windows.h>
#include <d3d11.h>
#include <d3d11_1.h>
#include <dxgi1_2.h>
#include <dxgi1_3.h>   // IDXGISwapChain2 (waitable object) — FG_PRESENT_PACING_DESIGN option B
#include <dxgi1_4.h>   // IDXGISwapChain3 (SetColorSpace1) — A3-L1
#include <dcomp.h>

#include <atomic>
#include <cstdint>
#include <cstdio>      // swprintf — the per-instance class-name suffix (R-D2-5)
#include <cwchar>      // wcscpy / wchar_t
#include <exception>   // std::set_terminate / std::terminate_handler (OA-3 crash last-gasp)
#include <new>
#include <thread>

namespace phyriad::render::present {

namespace {

// ── small RAII/error helpers ────────────────────────────────────────────────
template <class T> void rel(T*& p) noexcept { if (p) { p->Release(); p = nullptr; } }

[[nodiscard]] std::unexpected<phyriad::Error> fail(phyriad::ErrorCode c) noexcept {
    return std::unexpected(phyriad::Error{c, 0u, 0u});
}

// Keyed-mutex acquire timeout on submit(): a bounded wait so a stalled producer
// degrades to a skipped present (D-20) rather than blocking the presenter thread.
constexpr DWORD kAcquireTimeoutMs = 8;  // < one 120 Hz interval; tune per consumer

// ── monitor pick (index N → primary-first), the probe's pick_monitor ────────
struct EnumCtx { int want = 0; int idx = 0; RECT rc{}; bool found = false; };
BOOL CALLBACK mon_enum(HMONITOR hm, HDC, LPRECT, LPARAM lp) noexcept {
    auto* c = reinterpret_cast<EnumCtx*>(lp);
    MONITORINFO mi{}; mi.cbSize = sizeof(mi); GetMonitorInfo(hm, &mi);
    const bool primary = (mi.dwFlags & MONITORINFOF_PRIMARY) != 0;
    int this_idx;
    if (c->want == 0) { if (!primary) return TRUE; this_idx = 0; }
    else              { c->idx++; this_idx = c->idx; if (primary) { c->idx--; return TRUE; } }
    if (this_idx == c->want) { c->rc = mi.rcMonitor; c->found = true; return FALSE; }
    return TRUE;
}
RECT pick_monitor(int n) noexcept {
    EnumCtx c; c.want = n;
    EnumDisplayMonitors(nullptr, nullptr, mon_enum, reinterpret_cast<LPARAM>(&c));
    if (!c.found)
        c.rc = { 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
    return c.rc;
}

// The overlay WndProc: stays non-activating; never owns input. (The DcompCt
// recipe's WS_EX_TRANSPARENT is what actually grants click-through; this proc
// just refuses to do anything that would re-acquire focus or the cursor.)
LRESULT CALLBACK overlay_proc(HWND h, UINT m, WPARAM w, LPARAM l) noexcept {
    if (m == WM_DESTROY) return 0;
    return DefWindowProc(h, m, w, l);
}

// The base overlay window-class name. Historically a fixed process-global string
// (`RegisterClassExW` is called unconditionally in create()) — which means a SECOND
// concurrent PresentSurface::create() re-registers the SAME class → RegisterClassExW
// returns 0 with ERROR_CLASS_ALREADY_EXISTS → the second surface cannot be created
// (the D2 parallel-instances blocker R-D2-5, FG_PARALLEL_INSTANCES_RISK_REGISTER §R-D2-5).
//
// Fix (S2, option 1 — per-instance unique class): each Impl derives its OWN class name
// with a process-wide atomic suffix, registers exactly that name, and destroy() unregisters
// exactly that name → create/destroy stay symmetric, no shared class lifetime. This is a
// COVERAGE unblocker: the operator runs ONE game, so the single-instance path below is
// preserved byte-for-byte — the FIRST surface in a process (suffix 0) keeps the EXACT
// historical name `phyriad_present_overlay`, so a single-instance run registers the same
// class string it always did. A class name is an internal Win32 registration token; it
// never enters the presented output byte stream (R-D2-6).
constexpr wchar_t kOverlayClass[] = L"phyriad_present_overlay";

// R-D2-5: process-wide surface counter → the per-instance class-name suffix. 0 (the first
// surface) maps to the bare historical name (single-instance byte-identical); 1,2,… get the
// `_<n>` suffix so N concurrent surfaces each register a distinct class.
std::atomic<unsigned> g_class_seq{0};

// Build a unique class name into `out` (capacity `cap` wchars). seq 0 → the bare historical
// name; seq>0 → `phyriad_present_overlay_<seq>`. Returns `out`. Allocation-free.
const wchar_t* make_class_name(wchar_t* out, size_t cap, unsigned seq) noexcept {
    if (seq == 0u) { wcscpy_s(out, cap, kOverlayClass); return out; }
    swprintf(out, cap, L"%ls_%u", kOverlayClass, seq);
    return out;
}

// FG_OPTION_A — the watchdog stall threshold (RISK OA-10): if the present thread
// stops bumping its heartbeat for this long WHILE the OwnWindow plane is displayed,
// the watchdog force-hides our window so a wedged FG can never hold the user's panel.
// A couple of frames at 60-240 Hz; conservative (the no-lock-out floor, not a pacing knob).
constexpr int64_t kWatchdogStallMs = 250;

// FG_OPTION_A device-loss classification (RISK OA-1) — the DXGI twin of main.cpp's vk_live.
// Returns: 0 = live (happy path / benign), 1 = recoverable (MODE_CHANGED/OCCLUDED → ResizeBuffers
// cold path, no exit), 2 = terminal (DEVICE_REMOVED/RESET → the consumer must exit to passthrough).
// On SUCCEEDED(hr) it is identity → byte-identical when no device event occurs.
enum class DxgiLiveness : int { Live = 0, Recoverable = 1, Terminal = 2, OtherFail = 3 };
[[nodiscard]] DxgiLiveness dxgi_live(HRESULT hr) noexcept {
    // The DXGI_STATUS_* codes are SUCCEEDED-class HRESULTs (e.g. DXGI_STATUS_OCCLUDED = 0x087A0001),
    // so they MUST be matched before the SUCCEEDED() fast-out — otherwise an occluded present would be
    // misread as a clean Live present.
    switch (hr) {
        case DXGI_STATUS_MODE_CHANGED:
        case DXGI_STATUS_OCCLUDED:
            return DxgiLiveness::Recoverable; // a cold-path ResizeBuffers re-seats the swapchain
        case DXGI_ERROR_DEVICE_REMOVED:
        case DXGI_ERROR_DEVICE_RESET:
        case DXGI_ERROR_DRIVER_INTERNAL_ERROR:
            return DxgiLiveness::Terminal;    // the game keeps rendering behind us → exit IS passthrough
        default: break;
    }
    if (SUCCEEDED(hr)) return DxgiLiveness::Live;
    return DxgiLiveness::OtherFail;           // any other FAILED → the existing SystemError path
}

// FG_OPTION_A crash last-gasp (RISK OA-3 layer 2). The OwnWindow plane is the DISPLAYED surface, so a
// crash (std::terminate from an uncaught exception / a noexcept violation) would otherwise leave a
// topmost stale frame holding the panel until process teardown. We register a best-effort terminate
// handler (chained to whatever was installed before) that hides every live OwnWindow HWND first. The
// handler does Win32 window calls ONLY — allocation-free, reentrancy-safe. A SIGKILL / power-loss /
// hard driver crash runs no handler; there the borderless-over-exclusive invariant (RISK OA-3 layer 1)
// is the protection — the dead process's window vanishes the instant the OS reclaims it.
std::atomic<HWND>             g_own_hwnd{nullptr};   // the single live OwnWindow HWND (one surface : one plane)
std::atomic<std::terminate_handler> g_prev_terminate{nullptr};
std::atomic<bool>            g_terminate_installed{false};

void own_window_last_gasp() noexcept {
    // Default (seq_cst) atomic ops throughout this file: these are cold control-plane flags (yield
    // transitions, the 50 ms watchdog, one heartbeat store/tick) — seq_cst's cost is negligible vs the
    // CopyResource+Present they bracket, and it keeps the pillar off an explicit-memory-order dependency
    // (lint_hal: no raw memory_order_* outside framework/hal; the present pillar deps only phyriad_schema).
    if (HWND h = g_own_hwnd.load()) {
        SetWindowPos(h, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        ShowWindow(h, SW_HIDE);
    }
    if (auto prev = g_prev_terminate.load()) prev();
}

} // namespace

// ── Impl: all platform state behind the opaque pointer in the header ────────
struct PresentSurface::Impl {
    PresentSurfaceDesc desc{};
    RECT  mon{};
    UINT  W = 0, H = 0;

    HINSTANCE hinst   = nullptr;
    ATOM      cls     = 0;
    // R-D2-5: this surface's OWN window-class name (filled by make_class_name in create()).
    // create() registers exactly this name and destroy() unregisters exactly this name →
    // a second concurrent surface registers a DISTINCT class instead of colliding. The first
    // surface in the process (suffix 0) holds the bare historical name → single-instance
    // byte-identical. 64 wchars is ample ("phyriad_present_overlay_" + a 32-bit decimal).
    wchar_t   cls_name[64] = {};
    HWND      hwnd    = nullptr;

    ID3D11Device*           dev = nullptr;
    ID3D11DeviceContext*    ctx = nullptr;
    IDXGISwapChain1*        sc  = nullptr;
    ID3D11Texture2D*        backbuffer = nullptr;
    HANDLE                  waitable_obj = nullptr;  // FG_PRESENT_PACING_DESIGN B (default-off); app-owned → CloseHandle

    IDCompositionDevice* dcdev = nullptr;
    IDCompositionTarget* dctgt = nullptr;
    IDCompositionVisual* dcvis = nullptr;

    // Cached import of the current shared frame (re-imported only when the NT
    // handle changes — steady-state with a stable producer texture re-imports 0×).
    void*               imported_handle = nullptr;
    ID3D11Texture2D*    imported = nullptr;
    IDXGIKeyedMutex*    keyed    = nullptr;

    bool wda_ok        = false;  // SetWindowDisplayAffinity succeeded
    bool click_through = false;  // DcompCt style applied
    // R-D2-3 (per-instance device-loss scoping — the D2 unblocker, coverage). Today a terminal
    // DXGI device loss surfaces from submit() as ErrorCode::ShuttingDown, and the consumer
    // (apps/render_assistant) maps that to the PROCESS-GLOBAL g_quit/g_device_lost — so a TDR
    // on ONE surface's device quits the WHOLE process (would kill a second instance's surface
    // on another device). This latch records the loss ON THIS SURFACE so a future multi-instance
    // orchestrator (FgInstance) can route the terminal status to the OWNING instance's quit
    // WITHOUT consulting a global — i.e. surface B's device loss is observable as B's loss alone.
    // Set true exactly once, inside submit(), when dxgi_live() classifies the present Terminal.
    // The single-instance path is unaffected: the bit is queried only by device_lost(), a new
    // diagnostic the legacy consumer never calls, so the existing ShuttingDown→g_quit flow (and
    // its byte stream) is unchanged. atomic: a future orchestrator may poll it off the present thread.
    std::atomic<bool> device_lost_flag{false};
    // A3-L4 (FG_HDR_PIPELINE): the SINGLE source of truth for whether the present is FP16 scRGB. Set true
    // only after an FP16 swapchain actually creates; CR-HDR-1's fallback CLEARS it so the consumer (which
    // reads present_is_fp16()) keeps its producer bridge texture 8-bit and the one CopyResource stays
    // format-compatible (CR-HDR-2). Default false → the proven BGRA8 path → byte-identical (DR-HDR-1).
    bool present_is_fp16 = false;

    // ── FG_OPTION_A (Style::OwnWindow) — the displayed-plane + no-lock-out state ──
    bool own_window    = false;  // OwnWindow mode active (the opaque displayed flip plane)
    HWND game_hwnd     = nullptr; // the captured game's HWND (foreground-yield reference; may be null)
    std::atomic<bool> yielded{false};       // OA-2: we have hidden + dropped topmost → submit() is a no-op
    std::atomic<int64_t> heartbeat_ms{0};   // OA-10: bumped each submit(); the watchdog reads it
    std::atomic<bool> wd_run{false};        // watchdog thread alive flag
    std::thread       wd_thread;            // OA-10 present-thread watchdog (own_window only)

    // OA-2: drop the displayed plane so the desktop/game beneath is reachable. Win32-only,
    // allocation-free, reentrancy-safe (the crash last-gasp + the watchdog both call it).
    void yield_plane() noexcept {
        if (!hwnd) return;
        SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        ShowWindow(hwnd, SW_HIDE);
    }
    // OA-2: re-assert the displayed plane when the game returns to the foreground.
    void reassert_plane() noexcept {
        if (!hwnd) return;
        ShowWindow(hwnd, SW_SHOWNOACTIVATE);
        SetWindowPos(hwnd, HWND_TOPMOST, mon.left, mon.top,
                     (int)W, (int)H, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }

    // OA-1 (recoverable): re-seat the flip swapchain after MODE_CHANGED/OCCLUDED. COLD PATH —
    // releases + re-acquires the cached backbuffer; never called on the steady present path (D-2).
    bool resize_swapchain() noexcept {
        if (!sc) return false;
        rel(backbuffer);
        if (FAILED(sc->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0))) return false;
        if (FAILED(sc->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backbuffer)) || !backbuffer)
            return false;
        return true;
    }

    // Re-import the producer's shared texture when its NT handle changes.
    // Cold relative to steady state (D-2): a stable producer keeps the same
    // handle frame-to-frame, so this body is skipped on the hot path.
    bool ensure_imported(const SharedFrameHandle& s) noexcept {
        if (s.nt_handle == imported_handle && imported) return true;
        rel(keyed); rel(imported); imported_handle = nullptr;

        ID3D11Device1* dev1 = nullptr;
        if (FAILED(dev->QueryInterface(__uuidof(ID3D11Device1), (void**)&dev1)) || !dev1)
            return false;
        ID3D11Texture2D* tex = nullptr;
        HRESULT hr = dev1->OpenSharedResource1(
            reinterpret_cast<HANDLE>(s.nt_handle), __uuidof(ID3D11Texture2D), (void**)&tex);
        rel(dev1);
        if (FAILED(hr) || !tex) { rel(tex); return false; }

        IDXGIKeyedMutex* km = nullptr;
        tex->QueryInterface(__uuidof(IDXGIKeyedMutex), (void**)&km);
        imported = tex;
        keyed = km;                 // may be null if the producer chose no keyed mutex
        imported_handle = s.nt_handle;
        return true;
    }

    void destroy() noexcept {
        if (waitable_obj) { CloseHandle(waitable_obj); waitable_obj = nullptr; }
        // OA-10: stop the watchdog before tearing down the window it guards.
        wd_run.store(false);
        if (wd_thread.joinable()) wd_thread.join();
        // OA-3: hide the displayed plane FIRST so the panel returns the instant we begin teardown
        // (best-effort; the DestroyWindow below is the real give-back). No-op for the overlay styles.
        if (own_window) {
            // Clear the crash last-gasp target before DestroyWindow so the handler never touches a
            // destroyed HWND (only if WE are the published one — guards a possible second surface).
            HWND expected = hwnd;
            g_own_hwnd.compare_exchange_strong(expected, nullptr);
            yield_plane();
        }
        rel(keyed); rel(imported);
        rel(dcvis); rel(dctgt); rel(dcdev);
        rel(backbuffer); rel(sc); rel(ctx); rel(dev);
        if (hwnd) { DestroyWindow(hwnd); hwnd = nullptr; }
        // Pump so the window actually disappears before the class is unregistered.
        MSG m; for (int i = 0; i < 20; ++i) {
            while (PeekMessage(&m, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&m); DispatchMessage(&m); }
        }
        // R-D2-5: unregister THIS surface's own class (cls_name), not the shared base constant —
        // so two concurrent surfaces unregister their distinct classes independently (create/destroy
        // balanced per instance, no leak across repeated create/destroy). cls_name[0]==0 ⇒ never
        // registered (a failed/early create) ⇒ nothing to unregister.
        if (cls && hinst && cls_name[0]) { UnregisterClassW(cls_name, hinst); cls = 0; }
    }
};

// ─── create() — the cold path ───────────────────────────────────────────────
std::expected<PresentSurface, phyriad::Error>
PresentSurface::create(const PresentSurfaceDesc& desc) noexcept {
    auto* impl = new (std::nothrow) Impl();
    if (!impl) return fail(phyriad::ErrorCode::OutOfMemory);
    impl->desc  = desc;
    impl->hinst = GetModuleHandle(nullptr);
    impl->mon   = pick_monitor(desc.monitor_index);
    impl->W = desc.width  ? desc.width  : static_cast<UINT>(impl->mon.right  - impl->mon.left);
    impl->H = desc.height ? desc.height : static_cast<UINT>(impl->mon.bottom - impl->mon.top);

    auto bail = [&](phyriad::ErrorCode c) noexcept {
        impl->destroy(); delete impl; return fail(c);
    };

    // ── window class + the recipe HWND (probe main.cpp:211-223) ──────────────
    // R-D2-5: derive THIS surface's unique class name. The first surface in the process (suffix 0)
    // gets the bare historical name → single-instance byte-identical; a concurrent second surface
    // gets `phyriad_present_overlay_1`, so RegisterClassExW no longer collides with ERROR_CLASS_
    // ALREADY_EXISTS. fetch_add is the only added process-global op and it is cold-path (create()).
    make_class_name(impl->cls_name, 64, g_class_seq.fetch_add(1u));
    WNDCLASSEXW wc{}; wc.cbSize = sizeof(wc); wc.lpfnWndProc = overlay_proc;
    wc.hInstance = impl->hinst; wc.lpszClassName = impl->cls_name;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    impl->cls = RegisterClassExW(&wc);
    if (!impl->cls) return bail(phyriad::ErrorCode::SystemError);

    DWORD ex = WS_EX_TOPMOST | WS_EX_NOACTIVATE;
    if (desc.style == Style::Dcomp || desc.style == Style::DcompCt)
        ex |= WS_EX_NOREDIRECTIONBITMAP;
    if (desc.style == Style::DcompCt) {
        ex |= (WS_EX_LAYERED | WS_EX_TRANSPARENT);   // the click-through grant
        impl->click_through = true;
    }
    // FG_OPTION_A (Style::OwnWindow): an OPAQUE displayed flip plane that is STILL click-through.
    // Click-through is granted by WS_EX_TRANSPARENT *alone* — it removes the window from hit-testing
    // (mouse/keyboard fall through to the game beneath), and it is INDEPENDENT of compositing. We must
    // NOT add WS_EX_LAYERED here: a layered window is composited through DWM's redirection/UpdateLayered
    // path, which is incompatible with a flip-model (CreateSwapChainForHwnd) present — the flip present
    // would be ignored/fail and we'd never reach Independent Flip. So: TRANSPARENT yes, LAYERED no →
    // both the opaque flip present AND input pass-through. NON-ACTIVATING (WS_EX_NOACTIVATE kept) so we
    // never steal the game's input focus (RISK OA-2). is_click_through() stays DcompCt-only (it reports
    // the LAYERED+TRANSPARENT overlay recipe; OwnWindow is a different, opaque kind of click-through).
    if (desc.style == Style::OwnWindow) {
        ex |= WS_EX_TRANSPARENT;       // hit-test pass-through, NO WS_EX_LAYERED (keeps the flip present)
        impl->own_window = true;
        impl->game_hwnd  = reinterpret_cast<HWND>(desc.game_hwnd);   // may be null → yield only on !our-window
    }
    impl->hwnd = CreateWindowExW(ex, impl->cls_name, impl->cls_name, WS_POPUP,
        impl->mon.left, impl->mon.top, (int)impl->W, (int)impl->H,
        nullptr, nullptr, impl->hinst, nullptr);
    if (!impl->hwnd) return bail(phyriad::ErrorCode::SystemError);
    ShowWindow(impl->hwnd, SW_SHOWNOACTIVATE);
    SetWindowPos(impl->hwnd, HWND_TOPMOST, impl->mon.left, impl->mon.top,
        (int)impl->W, (int)impl->H, SWP_NOACTIVATE | SWP_SHOWWINDOW);

    // ── D3D11 device ─────────────────────────────────────────────────────────
    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    D3D_FEATURE_LEVEL fl;
    if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
            nullptr, 0, D3D11_SDK_VERSION, &impl->dev, &fl, &impl->ctx)))
        return bail(phyriad::ErrorCode::SystemError);

    IDXGIDevice*  dxgiDev = nullptr; impl->dev->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxgiDev);
    IDXGIAdapter* ad      = nullptr; if (dxgiDev) dxgiDev->GetAdapter(&ad);
    IDXGIFactory2* fac    = nullptr; if (ad) ad->GetParent(__uuidof(IDXGIFactory2), (void**)&fac);
    if (!fac) { rel(ad); rel(dxgiDev); return bail(phyriad::ErrorCode::SystemError); }

    DXGI_SWAP_CHAIN_DESC1 sd{};
    sd.Width = impl->W; sd.Height = impl->H;
    // A3-L4 (FG_HDR_PIPELINE, Tier-2): the FP16 scRGB present format. present_format==1 → request
    // R16G16B16A16_FLOAT (the ONLY HDR present path for an alpha-premult overlay; 10bpc HDR10 is wall-blocked
    // for blended composition — MASTER_PLAN §1). present_format==0 → the literal BGRA8 line unentered →
    // BYTE-IDENTICAL (DR-HDR-1). The create below is SOFT: on an FP16 create-failure it retries at BGRA8 and
    // CLEARS impl->present_is_fp16 (CR-HDR-1) so the consumer keeps its producer bridge 8-bit (CR-HDR-2).
    impl->present_is_fp16 = (desc.present_format == 1);
    sd.Format = impl->present_is_fp16 ? DXGI_FORMAT_R16G16B16A16_FLOAT
                                      : DXGI_FORMAT_B8G8R8A8_UNORM;
    sd.SampleDesc.Count = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount = 2;                       // fixed-capacity backbuffers (D-4)
    if (desc.waitable)                        // FG_PRESENT_PACING_DESIGN B (default-off → flag absent → byte-identical)
        sd.Flags |= DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;

    HRESULT hr;
    if (desc.style == Style::Baseline || desc.style == Style::OwnWindow) {
        // classic flip-model HWND swapchain — the §10 fallback AND FG_OPTION_A's displayed plane.
        // OwnWindow uses the IDENTICAL opaque flip swapchain (DISCARD + ALPHA_IGNORE + SCALING_NONE);
        // only the window recipe (WS_EX_TRANSPARENT click-through) + the no-lock-out contract differ.
        // The DComp block below is skipped → no composition device/target/visual for OwnWindow.
        sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        sd.AlphaMode  = DXGI_ALPHA_MODE_IGNORE;
        sd.Scaling    = DXGI_SCALING_NONE;
        hr = fac->CreateSwapChainForHwnd(impl->dev, impl->hwnd, &sd, nullptr, nullptr, &impl->sc);
        // A3-L4 CR-HDR-1: an FP16 swapchain can be refused on a non-Advanced-Color path. Make it SOFT —
        // retry at BGRA8 and clear present_is_fp16 so the consumer keeps its bridge 8-bit (CR-HDR-2). Never
        // bail on the FP16 attempt alone; only a BGRA8 failure is terminal (the proven path is unavailable).
        if (FAILED(hr) && impl->present_is_fp16) {
            impl->present_is_fp16 = false;
            sd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            hr = fac->CreateSwapChainForHwnd(impl->dev, impl->hwnd, &sd, nullptr, nullptr, &impl->sc);
        }
        if (FAILED(hr)) { rel(fac); rel(ad); rel(dxgiDev); return bail(phyriad::ErrorCode::SystemError); }
    } else {
        // composition swapchain (probe main.cpp:255-269): FLIP_SEQUENTIAL + premult.
        sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        sd.AlphaMode  = DXGI_ALPHA_MODE_PREMULTIPLIED;
        sd.Scaling    = DXGI_SCALING_STRETCH;
        hr = fac->CreateSwapChainForComposition(impl->dev, &sd, nullptr, &impl->sc);
        // A3-L4 CR-HDR-1: SOFT FP16 — retry at BGRA8 + clear present_is_fp16 (CR-HDR-2) on an FP16-only
        // refusal; only a BGRA8 failure bails. FP16 scRGB is Option 1, the only HDR present path for this
        // alpha-premult composition surface (10bpc HDR10 is wall-blocked for blended composition).
        if (FAILED(hr) && impl->present_is_fp16) {
            impl->present_is_fp16 = false;
            sd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            hr = fac->CreateSwapChainForComposition(impl->dev, &sd, nullptr, &impl->sc);
        }
        if (FAILED(hr)) { rel(fac); rel(ad); rel(dxgiDev); return bail(phyriad::ErrorCode::SystemError); }

        if (FAILED(DCompositionCreateDevice(dxgiDev, __uuidof(IDCompositionDevice), (void**)&impl->dcdev))) {
            rel(fac); rel(ad); rel(dxgiDev); return bail(phyriad::ErrorCode::SystemError); }
        if (FAILED(impl->dcdev->CreateTargetForHwnd(impl->hwnd, TRUE, &impl->dctgt))) {
            rel(fac); rel(ad); rel(dxgiDev); return bail(phyriad::ErrorCode::SystemError); }
        if (FAILED(impl->dcdev->CreateVisual(&impl->dcvis))) {
            rel(fac); rel(ad); rel(dxgiDev); return bail(phyriad::ErrorCode::SystemError); }
        impl->dcvis->SetContent(impl->sc);
        impl->dctgt->SetRoot(impl->dcvis);
        impl->dcdev->Commit();    // once — per-frame delivery is per-Present, not Commit-gated
    }
    rel(fac); rel(ad); rel(dxgiDev);

    // ── cache the backbuffer once (reused every submit — zero hot-path alloc) ─
    if (FAILED(impl->sc->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&impl->backbuffer))
            || !impl->backbuffer)
        return bail(phyriad::ErrorCode::SystemError);

    // ── waitable swapchain (FG_PRESENT_PACING_DESIGN option B, default-off) ───
    // SetMaximumFrameLatency(1) + the latency waitable object so submit() can block until the
    // system is ready to accept a frame (MS-verified composed-path latency/jitter reduction).
    // QI/handle failure is SOFT: waitable_obj stays null → submit() takes the current no-wait path.
    if (desc.waitable) {
        IDXGISwapChain2* sc2 = nullptr;
        if (SUCCEEDED(impl->sc->QueryInterface(__uuidof(IDXGISwapChain2), (void**)&sc2)) && sc2) {
            sc2->SetMaximumFrameLatency(1);
            impl->waitable_obj = sc2->GetFrameLatencyWaitableObject();  // app-owned → CloseHandle in destroy()
            rel(sc2);
        }
    }

    // ── A3-L1/A3-L4: declare the overlay/present colorspace (default-off) ─
    // A3-L1: on an Advanced-Color (HDR) desktop, an SDR overlay that does not declare its colorspace
    // washes out; SetColorSpace1(sRGB) tells DWM to composite it as SDR sRGB.
    // A3-L4: when the present is FP16 (present_is_fp16 — the create above actually got the FP16 swapchain),
    // the correct treatment is LINEAR scRGB (G10_NONE_P709, dossier Option 1) so an HDR display receives HDR;
    // this overrides the sRGB map. Soft (skipped if IDXGISwapChain3/CheckColorSpaceSupport unsupported →
    // FR-HDR-2: on a display without scRGB-present support the FP16 buffer composes as sRGB = washed/wrong,
    // but NO crash; low-severity accepted residual). The whole block is entered when EITHER A3-L1's
    // present_colorspace is set OR the FP16 swapchain came up → byte-identical when both are 0/false.
    if (desc.present_colorspace != 0 || impl->present_is_fp16) {
        IDXGISwapChain3* sc3 = nullptr;
        if (SUCCEEDED(impl->sc->QueryInterface(__uuidof(IDXGISwapChain3), (void**)&sc3)) && sc3) {
            const DXGI_COLOR_SPACE_TYPE cs = impl->present_is_fp16
                ? DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709    // A3-L4: linear scRGB (FP16 HDR present, Option 1)
                : DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;   // A3-L1: sRGB (SDR-overlay washout fix)
            UINT support = 0;
            if (SUCCEEDED(sc3->CheckColorSpaceSupport(cs, &support)) &&
                (support & DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT))
                sc3->SetColorSpace1(cs);
            rel(sc3);
        }
    }

    // ── WDA (probe apply_wda) — failure is soft: overlay works, just capturable ─
    if (desc.capture == CaptureAffinity::ExcludeFromCapture) {
        SetLastError(0);
        impl->wda_ok = SetWindowDisplayAffinity(impl->hwnd, WDA_EXCLUDEFROMCAPTURE) != 0;
        // wda_ok==false surfaces via capture_excluded(); NOT a create() failure (§1.5.3).
    }

    // ── FG_OPTION_A (Style::OwnWindow) — the present-thread watchdog (RISK OA-10) ──
    // A separate thread reads the heartbeat submit() bumps each tick. If the present thread stalls
    // (crash/hang/wedge) WHILE the plane is displayed, force-hide our window so it can never hold the
    // user's panel on a stale frame. The watchdog calls ONLY Win32 window APIs (allocation-free,
    // reentrancy-safe). It runs ONLY in OwnWindow mode — the overlay styles are benign on crash, so
    // this whole machinery is absent (and thus byte-identical) when OwnWindow is not selected.
    if (impl->own_window) {
        // OA-3 layer 2: publish our HWND for the crash last-gasp + install the chained terminate
        // handler once (idempotent). Best-effort; the borderless-FSO invariant is the hard floor.
        g_own_hwnd.store(impl->hwnd);
        if (!g_terminate_installed.exchange(true)) {
            g_prev_terminate.store(std::set_terminate(own_window_last_gasp));
        }
        impl->heartbeat_ms.store(0);
        impl->wd_run.store(true);
        impl->wd_thread = std::thread([impl]() noexcept {
            int64_t last_seen = -1, last_change_at = 0;
            while (impl->wd_run.load()) {
                Sleep(50);
                if (impl->yielded.load()) continue; // already hidden → nothing to guard
                const int64_t hb  = impl->heartbeat_ms.load();
                const int64_t nowt = (int64_t)GetTickCount64();
                if (hb != last_seen) { last_seen = hb; last_change_at = nowt; continue; }
                if (hb != 0 && (nowt - last_change_at) > kWatchdogStallMs) {
                    // The present thread has not bumped the heartbeat for too long while displayed →
                    // a wedged FG. Force-hide so the game/desktop reclaims the plane. We do NOT set
                    // yielded (the foreground monitor owns that bit); the force-hide is the hard floor.
                    impl->yield_plane();
                    last_change_at = nowt;  // re-arm so we don't spin SetWindowPos every 50 ms
                }
            }
        });
    }

    return PresentSurface(impl);
}

// ─── submit() — the hot path (§1.5.2) ───────────────────────────────────────
std::expected<void, phyriad::Error>
PresentSurface::submit(const SharedFrameHandle& s) noexcept {
    if (!impl_) return fail(phyriad::ErrorCode::InvalidHandle);
    if (!s.nt_handle) return fail(phyriad::ErrorCode::InvalidArgument);

    // Drain this window's pending messages so Windows never flags it "not
    // responding" (~5 s without dispatch → DWM ghosts the window: white +
    // dialog — observed in the gap-3 session). Empty-queue PeekMessage is
    // sub-µs, so this stays D-2-clean. hwnd-filtered: only OUR window's
    // messages, never the consumer thread's other windows. Requires submit()
    // on the create() thread (the window's queue lives there — header contract).
    MSG m;
    while (PeekMessage(&m, impl_->hwnd, 0, 0, PM_REMOVE)) {
        TranslateMessage(&m); DispatchMessage(&m);
    }

    // FG_PRESENT_PACING_DESIGN option B: block until the system is ready to accept a frame
    // (the waitable swapchain — minimizes the composed-path wait). Bounded 1 s so a stalled
    // compositor degrades rather than hangs. Default-off → waitable_obj null → no wait (byte-identical).
    if (impl_->waitable_obj) WaitForSingleObjectEx(impl_->waitable_obj, 1000, FALSE);

    // ── FG_OPTION_A (Style::OwnWindow) — foreground-yield + heartbeat (RISK OA-2/OA-10) ──
    // OwnWindow is NON-ACTIVATING, so it never gets its own WM_KILLFOCUS — the yield is driven by
    // polling the foreground window each present tick (cheap: one GetForegroundWindow + compares).
    // When the foreground is NEITHER our window NOR the captured game → the user alt-tabbed/minimized/
    // switched apps → hide + drop topmost so the desktop is reachable, and EARLY-RETURN (present
    // nothing → the game holds the scanout plane → byte-identical to passthrough). When the game (or
    // our window) is back in front → re-assert the plane and resume. This whole block is OwnWindow-only;
    // the overlay styles skip it entirely → byte-identical when OwnWindow is not selected.
    if (impl_->own_window) {
        impl_->heartbeat_ms.store((int64_t)GetTickCount64());  // OA-10 liveness (default seq_cst, see own_window_last_gasp)
        const HWND fg = GetForegroundWindow();
        const bool ours = (fg == impl_->hwnd);
        const bool game = (impl_->game_hwnd && fg == impl_->game_hwnd);
        // game_hwnd set → display only while the game or our window is in front (yield to any other app).
        // game_hwnd==nullptr (no game binding) → we can only key off our own window: a null/own
        // foreground keeps us displayed, anything else yields (still never a permanent lock-out).
        const bool want_yield = impl_->game_hwnd ? !(ours || game) : (fg != nullptr && fg != impl_->hwnd);
        const bool was_yielded = impl_->yielded.load();
        if (want_yield && !was_yielded) {
            impl_->yielded.store(true);
            impl_->yield_plane();
        } else if (!want_yield && was_yielded) {
            impl_->yielded.store(false);
            impl_->reassert_plane();
        }
        if (impl_->yielded.load()) return {};  // present nothing → passthrough
    }

    if (!impl_->ensure_imported(s)) return fail(phyriad::ErrorCode::SystemError);

    // keyed-mutex acquire (the platform cross-device sync). A timeout degrades to
    // a skipped present (D-20) — return the error, never block the presenter.
    if (impl_->keyed) {
        HRESULT a = impl_->keyed->AcquireSync(s.keyed_mutex_key, kAcquireTimeoutMs);
        if (a == static_cast<HRESULT>(WAIT_TIMEOUT)) return fail(phyriad::ErrorCode::Timeout);
        if (FAILED(a)) return fail(phyriad::ErrorCode::SystemError);
    }

    impl_->ctx->CopyResource(impl_->backbuffer, impl_->imported);  // §7: the one copy/frame

    if (impl_->keyed) impl_->keyed->ReleaseSync(s.keyed_mutex_key);

    // FG_OPTION_A device-loss hardening (RISK OA-1) — dxgi_live is the DXGI twin of main.cpp's vk_live:
    // ALWAYS-ON but IDENTITY-ON-SUCCESS (the happy path returns Live → {} → byte-identical to before;
    // only an actual DEVICE_REMOVED/RESET or MODE_CHANGED/OCCLUDED trips a new branch). We now OWN the
    // displayed DXGI present (OwnWindow), so this present-site takes the device-loss hit directly.
    // Present uses the FG_PRESENT_PACING_DESIGN B sync_interval knob (defaults 0 = current) for EVERY
    // style — the OwnWindow handling is purely in the result classification below.
    const HRESULT hr = impl_->sc->Present(impl_->desc.sync_interval, 0);
    // Overlay styles (DcompCt/Dcomp/Baseline): the EXACT original mapping — SUCCEEDED → {}, FAILED →
    // SystemError. The device-loss recovery/terminal handling is gated to OwnWindow so the overlay path
    // is byte-identical to before (it is a benign composition surface; it does not own the panel).
    if (!impl_->own_window)
        return SUCCEEDED(hr) ? std::expected<void, phyriad::Error>{} : fail(phyriad::ErrorCode::SystemError);
    switch (dxgi_live(hr)) {
        case DxgiLiveness::Live:
            return {};
        case DxgiLiveness::Recoverable:
            // MODE_CHANGED / OCCLUDED → re-seat the flip swapchain on the COLD path (no hot-path alloc,
            // D-2). The frame just presented is dropped; the next submit() presents into the fresh
            // backbuffer. A ResizeBuffers failure escalates to a skipped present (never a crash).
            if (!impl_->resize_swapchain()) return fail(phyriad::ErrorCode::SystemError);
            return {};
        case DxgiLiveness::Terminal:
            // DEVICE_REMOVED / RESET / DRIVER_INTERNAL_ERROR → terminal. Surface it to the consumer as
            // ShuttingDown so it can drive the proven exit-to-passthrough (the game keeps rendering
            // behind us; main.cpp maps this to g_quit/g_device_lost — the same Ctrl-C unwind).
            // R-D2-3: ALSO latch the loss ON THIS SURFACE so a future multi-instance orchestrator can
            // attribute the loss to THIS instance (and quit only it) without the process-global g_quit.
            // The legacy single-instance consumer never reads device_lost(), so its ShuttingDown→g_quit
            // flow + byte stream are unchanged; this store is pure additive instrumentation.
            impl_->device_lost_flag.store(true);
            return fail(phyriad::ErrorCode::ShuttingDown);
        case DxgiLiveness::OtherFail:
        default:
            return fail(phyriad::ErrorCode::SystemError);
    }
}

// ─── submit_at() — Pacing::PresentationManager (DESIGNED, not yet shipped) ───
std::expected<void, phyriad::Error>
PresentSurface::submit_at(const SharedFrameHandle& /*src*/, uint64_t /*target_qpc*/) noexcept {
    // The displayable-surface + IPresentationManager path is the §1.5.4 follow-up,
    // gated behind the maintainer's sub-pillar decision and a measured need (D-12).
    // The call-based v1 ships Pacing::Immediate; this is honestly Unavailable.
    return fail(phyriad::ErrorCode::Unavailable);
}

bool PresentSurface::capture_excluded() const noexcept { return impl_ && impl_->wda_ok; }
bool PresentSurface::is_click_through() const noexcept { return impl_ && impl_->click_through; }
// A3-L4 (FG_HDR_PIPELINE): the actual present format (post-CR-HDR-1-fallback). The consumer reads this to
// match its producer bridge texture format (CR-HDR-2): true → FP16 bridge, false → 8-bit bridge.
bool PresentSurface::present_is_fp16() const noexcept { return impl_ && impl_->present_is_fp16; }
// R-D2-3 (per-instance device-loss scoping — the D2 unblocker): true iff a TERMINAL DXGI device
// loss (DEVICE_REMOVED/RESET/DRIVER_INTERNAL_ERROR) was observed at THIS surface's present site.
// A multi-instance orchestrator polls this to attribute the loss to the owning instance and quit
// only it (instead of the process-global g_quit). false for a default/moved-from surface and on
// the happy path → the single-instance consumer that never calls it is byte-identical.
bool PresentSurface::device_lost() const noexcept { return impl_ && impl_->device_lost_flag.load(); }

// W4 (FG fluidity PACING axis — measurement-only, byte-identical-off): the last display-FLIP's
// statistics for the own-window flip swapchain. GetFrameStatistics is READ-ONLY (it changes no
// present-path state), so a run that calls this presents the SAME bytes as one that does not.
// Gated to the OWN-WINDOW flip path (CreateSwapChainForHwnd + DXGI_SWAP_EFFECT_FLIP_DISCARD), which
// yields per-flip statistics; the DComp/composed path (CreateSwapChainForComposition) does NOT
// reliably yield flip stats, so we return nullopt WITHOUT calling GetFrameStatistics on it (the
// composed-path API YIELD is rig-pending). SOFT on every HRESULT failure
// (DXGI_ERROR_FRAME_STATISTICS_DISJOINT, no present yet, device-loss) → nullopt, so the consumer
// leaves MsBetweenDisplayChange NA and the scorer falls back to the present-time proxy (flagged).
std::optional<PresentSurface::FlipStats> PresentSurface::last_flip_qpc() const noexcept {
    if (!impl_ || !impl_->own_window || !impl_->sc) return std::nullopt;
    DXGI_FRAME_STATISTICS fs{};
    if (FAILED(impl_->sc->GetFrameStatistics(&fs))) return std::nullopt;
    return FlipStats{ static_cast<uint64_t>(fs.SyncQPCTime.QuadPart),
                      static_cast<uint32_t>(fs.PresentCount) };
}

// ─── move + destruction ─────────────────────────────────────────────────────
PresentSurface::PresentSurface(PresentSurface&& o) noexcept : impl_(o.impl_) { o.impl_ = nullptr; }
PresentSurface& PresentSurface::operator=(PresentSurface&& o) noexcept {
    if (this != &o) {
        if (impl_) { impl_->destroy(); delete impl_; }
        impl_ = o.impl_; o.impl_ = nullptr;
    }
    return *this;
}
PresentSurface::~PresentSurface() noexcept {
    if (impl_) { impl_->destroy(); delete impl_; impl_ = nullptr; }
}

} // namespace phyriad::render::present

#else // ── Non-Windows: no-op stub (the etw posture, D-19) ────────────────────

namespace phyriad::render::present {

struct PresentSurface::Impl {};  // never instantiated off Windows

std::expected<PresentSurface, phyriad::Error>
PresentSurface::create(const PresentSurfaceDesc&) noexcept {
    return std::unexpected(phyriad::Error{phyriad::ErrorCode::Unavailable, 0u, 0u});
}
std::expected<void, phyriad::Error>
PresentSurface::submit(const SharedFrameHandle&) noexcept {
    return std::unexpected(phyriad::Error{phyriad::ErrorCode::Unavailable, 0u, 0u});
}
std::expected<void, phyriad::Error>
PresentSurface::submit_at(const SharedFrameHandle&, uint64_t) noexcept {
    return std::unexpected(phyriad::Error{phyriad::ErrorCode::Unavailable, 0u, 0u});
}
bool PresentSurface::capture_excluded() const noexcept { return false; }
bool PresentSurface::is_click_through() const noexcept { return false; }
bool PresentSurface::present_is_fp16() const noexcept { return false; }
bool PresentSurface::device_lost() const noexcept { return false; }  // R-D2-3 stub (D-19 posture)
std::optional<PresentSurface::FlipStats> PresentSurface::last_flip_qpc() const noexcept { return std::nullopt; }  // W4 stub (D-19 posture)

PresentSurface::PresentSurface(PresentSurface&& o) noexcept : impl_(o.impl_) { o.impl_ = nullptr; }
PresentSurface& PresentSurface::operator=(PresentSurface&& o) noexcept {
    if (this != &o) { impl_ = o.impl_; o.impl_ = nullptr; }
    return *this;
}
PresentSurface::~PresentSurface() noexcept = default;

} // namespace phyriad::render::present

#endif // _WIN32

// Made with my soul - Swately <3
