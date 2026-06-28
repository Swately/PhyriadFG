#pragma once
// PhyriadFG - WGC/WinRT capture context. The WinRT include block + namespace aliases +
// IDirect3DDxgiInterfaceAccess + the WgcCtx staging-ring struct, shared between run_capture
// (capture/capture.cpp) and main.cpp. MSVC-only (WGC = C++/WinRT); the whole block is
// #ifdef _MSC_VER.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3d11_4.h>   // ID3D11Fence / ID3D11DeviceContext4 / ID3D11Texture2D (WgcCtx members)
#include <atomic>
#include <mutex>
#include <cstdint>

#ifdef _MSC_VER
#define WINRT_LEAN_AND_MEAN
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>
namespace wgc  = winrt::Windows::Graphics::Capture;
namespace wgdx = winrt::Windows::Graphics::DirectX;
namespace wd3d = winrt::Windows::Graphics::DirectX::Direct3D11;
// IDirect3DDxgiInterfaceAccess: the interop header does not surface this under the
// cppwinrt include order (C2065), so declare it inline.
struct __declspec(uuid("A9B3D012-3DF2-4EE3-B8D1-8695F457D3C1"))
IDirect3DDxgiInterfaceAccess : ::IUnknown {
    virtual HRESULT __stdcall GetInterface(GUID const& id, void** object) = 0;
};

// -- WGC staging-ring context -----------------------------------------------------------
    struct WgcCtx {
        wgc::Direct3D11CaptureFramePool pool{nullptr};
        wgc::GraphicsCaptureSession     session{nullptr};
        std::atomic<bool>   frame_ready{false};
        std::atomic<uint64_t> arrived{0};   // every FrameArrived — vs processed = the drop rate
        // True SOURCE cadence, measured at ARRIVAL (callback thread). Pacing must use this,
        // not the processed cadence: when the loop lags, processed intervals = loop time →
        // an EMA on them ratchets the schedule up in a feedback runaway. Arrival deltas
        // cannot inflate that way.
        std::atomic<uint64_t> last_arr_us{0};
        std::atomic<uint64_t> arr_delta_us{0};
        std::atomic<bool>   running{true};
        std::mutex          ctx_mtx;
        // N-slot staging ring.  Callback: CopyResource to ring[w%N] only.
        // Main loop: Map(DO_NOT_WAIT) on newest filled slot, memcpy, Unmap.
        // D3D11Multithread protection (enabled in d3d_init) makes concurrent access safe.
        enum : uint32_t { RING_N = 4u };   // (enum, not static constexpr: MSVC C2246 in a local class)
        ID3D11Texture2D*      ring[RING_N]{};
        std::atomic<uint32_t> ring_write{0};   // ever-increasing; callback owns slot (w-1)%N after write
        std::atomic<uint32_t> ring_read{0};    // ever-increasing; main advances to w after consuming newest
        // --latency-trace (measurement-only): per-WGC-ring-slot capture timestamps. The callback
        // writes them at slot w%RING_N; the C-thread reads them at the slot it consumes ((use_cnt-1)%RING_N).
        std::atomic<uint64_t> ring_submit_us[RING_N]{};   // now_ms()*1000 at CopyResource submit → C computes copy-exec = tcap−submit
        std::atomic<uint64_t> ring_compose_us[RING_N]{};  // (QPC_now − Frame.SystemRelativeTime) µs = WGC compose→delivery (INVISIBLE to freshage)
        // --copy-fence: event-driven CopyResource-completion pickup. The callback enqueues
        // ctx4->Signal(copyFence, ring_write_after_increment) AFTER the CopyResource; the C-thread waits the
        // copyEvt (off the context) for fence>=ring_write before its Map. All three created at the WGC
        // ring-alloc site (gated on cfg.copy_fence + a D3D11.4 capability probe), released at teardown
        // AFTER the running=false + Sleep(100) callback drain. All null when --copy-fence is off.
        ID3D11Fence*          copyFence=nullptr;   // GPU-timeline fence the callback Signals post-copy
        ID3D11DeviceContext4* ctx4=nullptr;        // the D3D11.4 context view used for ctx4->Signal
        HANDLE                copyEvt=nullptr;     // auto-reset event SetEventOnCompletion fires; C waits it
        // --copy-device: the SEPARATE D3D11 device (+ its immediate context) the WGC copy queue
        // runs on when --copy-device is armed. Created on the SAME adapter as d.dev (so WGC delivers surfaces
        // this device can CopyResource from), with D3D11Multithread protection like d3d_init. The pool's
        // d3d_winrt, the ring textures, the copy-fence machinery, the callback CopyResource, and the C-thread
        // Map all use cap_dev/cap_ctx (which alias d.dev/d.ctx when off). Both null when off; released at
        // teardown AFTER the running=false + Sleep(100) callback drain (same ordering as the fence).
        ID3D11Device*         cdev=nullptr;        // 2nd D3D11 device (null when --copy-device off)
        ID3D11DeviceContext*  cctx=nullptr;        // its immediate context (null when off)
    };
#endif // _MSC_VER
