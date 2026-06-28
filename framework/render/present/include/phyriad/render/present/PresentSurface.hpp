// framework/render/present/include/phyriad/render/present/PresentSurface.hpp
// External presentation-composition primitive (FR-RENDER-1).
//
// The *presenter that owns the panel*: a capture-proof, input-invisible,
// flip/composition-grade alpha overlay over a running game, built in one
// create() call instead of re-deriving the unproven
// `NOREDIRECTIONBITMAP + LAYERED|TRANSPARENT` window + composition swapchain +
// WDA + VK→D3D11 bridge by hand (the render_assistant STAGE-33b/34c/39h history).
//
// What this primitive owns (and ONLY this):
//   - the borderless composition HWND with the measured trilemma-resolving recipe
//   - the composition swapchain (CreateSwapChainForComposition, premult alpha)
//   - the DirectComposition target/visual
//   - capture exclusion (SetWindowDisplayAffinity(WDA_EXCLUDEFROMCAPTURE))
//   - the Vulkan/D3D11 shared-texture bridge (one CopyResource per submit)
//
// What stays in the consumer (Ayama): overlay assembly, UX policy, output-clock
// integration, tier routing. The primitive owns *how* a frame reaches the panel,
// never *which* frame or *when* (PRESENTATION_LAYER_PRIOR_ART.md §9.4).
//
// Public header — intentionally free of <windows.h>/<d3d11.h>/<dcomp.h> to avoid
// macro pollution at call-sites (the Stream.hpp opaque-handle discipline). The
// Win32/D3D11/DComp state lives behind an opaque Impl pointer; the .cpp owns it.
//
// Platform: Windows-only capability (DComp / composition swapchain / WDA are
// Win32). The non-Windows build declares the SAME surface with bodies returning
// std::unexpected(Error{ErrorCode::Unavailable}) — the etw no-op-stub posture
// (D-19). Compiles and links cleanly everywhere; never works off Windows.
//
// Errors via std::expected (D-7). Descriptor + frame handle are POD (D-8).
// create() is the declared cold path (window/device); submit() is the hot path
// with zero steady-state allocation (D-2). Single presenter thread (§1.7).

#pragma once

#include <phyriad/schema/Error.hpp>

#include <cstdint>
#include <expected>
#include <optional>
#include <type_traits>

namespace phyriad::render::present {

// ─── Descriptor enums ──────────────────────────────────────────────────────────
// Style mirrors the STAGE-44a probe's --style. DcompCt is the measured
// trilemma-resolving recipe (click-through + capture-proof + flip-grade).
enum class Style : uint8_t {
    Baseline,   // classic flip HWND swapchain (no click-through) — the §10 fallback
    Dcomp,      // NOREDIRECTIONBITMAP + composition swapchain (solid hit-test)
    DcompCt,    // + LAYERED|TRANSPARENT → click-through; the measured config
    OwnWindow,  // FG_OPTION_A: an OPAQUE flip-model HWND swapchain (Baseline's swapchain) so we
                //   BECOME the DISPLAYED Independent-Flip plane → demote the game to Composed → pace
                //   it down (the LSFG present topology). CLICK-THROUGH via WS_EX_TRANSPARENT *without*
                //   WS_EX_LAYERED (layering breaks the flip-model present — see PresentSurface.cpp),
                //   so the user's keyboard/mouse still reach the game beneath us; NON-ACTIVATING (we
                //   never steal the game's input focus). Carries the no-lock-out contract:
                //   a foreground-window yield + a present-thread watchdog (RISK OA-2/OA-10) and a
                //   device-loss exit (RISK OA-1). DEFAULT-OFF mode; reachable only via the consumer
                //   flag. NOT click-through-reported as is_click_through() (that flag stays DcompCt-only).
};

// Immediate now; PresentationManager is the §6.1 timestamp-pacing upgrade path.
enum class Pacing : uint8_t {
    Immediate,            // Present(0) — the v1 path
    PresentationManager,  // IPresentationManager SetTargetTime (DESIGNED, not shipped)
};

// WDA_EXCLUDEFROMCAPTURE — the §10 capture-proof result.
enum class CaptureAffinity : uint8_t {
    Normal,
    ExcludeFromCapture,
};

// ─── PresentSurfaceDesc (POD, D-8) ─────────────────────────────────────────────
// What window/swapchain/affinity to build. Cold-path input to create().
struct PresentSurfaceDesc {
    int32_t  monitor_index = 0;   // primary-first enumeration (the probe's pick_monitor)
    uint32_t width         = 0;   // 0,0 = the monitor's full extent
    uint32_t height        = 0;
    Style           style   = Style::DcompCt;                // the trilemma-resolving default
    CaptureAffinity capture = CaptureAffinity::ExcludeFromCapture; // §10 capture-proof default
    Pacing          pacing  = Pacing::Immediate;             // PresentationManager when WDDM 3.0
    // FG_PRESENT_PACING_DESIGN option B — composed-overlay jitter reduction. Both default-off →
    // BYTE-IDENTICAL to the prior present path. They reduce jitter/latency WITHIN DWM composition;
    // they do NOT promote the overlay to Independent Flip (that needs option A, the own-window mode).
    bool     waitable      = false; // true → create with DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT
                                    //   + SetMaximumFrameLatency(1) + WaitForSingleObject before present
                                    //   (MS-verified: render on current data, queue when the system is ready).
    uint8_t  sync_interval = 0;     // Present(sync_interval, 0). 0 = present-immediately (current; over-presents
                                    //   past refresh). 1 = pace to the compositor (stops over-presenting). Eye-tune.
    // A3-L1 (HDR/format completeness, FG_HDR_FORMAT_PRIOR_ART): declare the overlay's colorspace so DWM
    // composites it correctly on an Advanced-Color (HDR) desktop (else the SDR overlay washes out). 0 =
    // no-op (current — never call SetColorSpace1). 1 = sRGB (DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709).
    // Soft: if IDXGISwapChain3/CheckColorSpaceSupport is unavailable the call is skipped (byte-identical).
    uint8_t  present_colorspace = 0;
    // A3-L4 (FG_HDR_PIPELINE, the FP16 scRGB HDR present — Tier-2). 0 = the current 8-bit BGRA8 swapchain
    // (DXGI_FORMAT_B8G8R8A8_UNORM, the only present path today). 1 = an FP16 scRGB swapchain
    // (DXGI_FORMAT_R16G16B16A16_FLOAT + DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709) so an HDR display receives
    // HDR. This is the SINGLE present-side format decision; the consumer MUST widen its producer bridge
    // texture to a matching FP16 format (CR-HDR-2) so the one CopyResource into the backbuffer stays
    // format-compatible. SOFT (CR-HDR-1): if the FP16 swapchain create fails, create() falls back to the
    // proven BGRA8 path AND reports it via present_is_fp16()==false so the consumer keeps its bridge 8-bit.
    // 0 = the literal BGRA8 line unentered → BYTE-IDENTICAL (DR-HDR-1). HDR is inert without an HDR display +
    // HDR content (coverage/correctness, not a default-path win).
    uint8_t  present_format = 0;
    uint8_t  _pad[5] = {0, 0, 0, 0, 0};                     // align game_hwnd to 8 (offset 19→24)
    // FG_OPTION_A (Style::OwnWindow only): the captured GAME's top-level HWND (the consumer's
    // wgc_target_hwnd). USED ONLY by OwnWindow's no-lock-out contract — the foreground-yield monitor
    // compares GetForegroundWindow() against {our window, this game window}: foreground is NEITHER
    // → the user left both → yield the displayed plane (hide + drop topmost) so the desktop is
    // reachable (RISK OA-2). Also picks our monitor via MonitorFromWindow(game_hwnd) when it is the
    // game's monitor (RISK OA-8). nullptr (the default, and for every other Style) → no game binding:
    // OwnWindow then yields only on foreground != our window (still never locks out), and binds by
    // monitor_index. Opaque void* (kept <windows.h>-free; reinterpret_cast<HWND> in the .cpp).
    void*    game_hwnd = nullptr;
};
static_assert(std::is_standard_layout_v<PresentSurfaceDesc>);
static_assert(std::is_trivially_copyable_v<PresentSurfaceDesc>);

// ─── SharedFrameHandle (POD, D-8) ──────────────────────────────────────────────
// A frame the consumer produced on some GPU. Carries the shared NT handle to a
// D3D11/VK shared texture (the §7 bridge source) + its keyed-mutex key + dims.
// No ownership — the consumer owns the texture's lifetime. Layout-compatible with
// composite::SharedFrame's win32_handle export (vkGetMemoryWin32HandleKHR).
struct SharedFrameHandle {
    void*    nt_handle       = nullptr; // shared NT handle (vkGetMemoryWin32HandleKHR / CreateSharedHandle)
    uint64_t keyed_mutex_key = 0;       // key to AcquireSync on the producer's shared texture
    uint32_t width           = 0;
    uint32_t height          = 0;
};
static_assert(std::is_standard_layout_v<SharedFrameHandle>);
static_assert(std::is_trivially_copyable_v<SharedFrameHandle>);

// ─── PresentSurface ────────────────────────────────────────────────────────────
// Move-only owner of the HWND + D3D11 device + composition swapchain + DComp +
// WDA + bridge resources. NOT a GLFWwindow / IRenderBackend — it owns its own
// borderless composition HWND with a distinct lifecycle.
class PresentSurface {
public:
    // Cold path (D-2 exception): creates the borderless composition HWND + D3D11
    // device + composition swapchain + DComp target/visual + applies WDA.
    // noexcept; all failure via std::expected (D-7). On a DComp/WDA refusal the
    // consumer falls back to Style::Baseline (D-20, §1.5.3).
    [[nodiscard]] static std::expected<PresentSurface, phyriad::Error>
    create(const PresentSurfaceDesc& desc) noexcept;

    // Hot path: drain the window's pending messages (keeps Windows from
    // ghosting the window as "not responding" — sub-µs when the queue is
    // empty), import src via its keyed mutex, one CopyResource into the
    // backbuffer (§7 — exactly one full-frame copy, no zero-copy exists),
    // Present(0). Zero steady-state allocation (D-2). A keyed-mutex acquire
    // timeout degrades by skipping the present (returns the error), never blocks.
    //
    // THREADING CONTRACT: call submit() on the SAME thread that called
    // create() — the window's message queue belongs to the creating thread,
    // so a pump from any other thread drains nothing and the ghosting
    // protection silently stops working. Single presenter thread (§1.7).
    [[nodiscard]] std::expected<void, phyriad::Error>
    submit(const SharedFrameHandle& src) noexcept;

    // Hot path, Pacing::PresentationManager only: queue a present for target QPC
    // time (§6.1 — IPresentationManager SetTargetTime). The call-based v1 ships
    // Pacing::Immediate; this returns std::unexpected(Unavailable) until the
    // displayable-surface path is built (the §1.5.4 follow-up).
    [[nodiscard]] std::expected<void, phyriad::Error>
    submit_at(const SharedFrameHandle& src, uint64_t target_qpc) noexcept;

    // Diagnostics — the probe's verdict fields, queryable by the consumer.
    [[nodiscard]] bool capture_excluded() const noexcept; // WDA call succeeded
    [[nodiscard]] bool is_click_through() const noexcept;  // DcompCt style
    // A3-L4 (FG_HDR_PIPELINE): true iff the swapchain was actually created at FP16 scRGB
    // (DXGI_FORMAT_R16G16B16A16_FLOAT). When desc.present_format==1 but the FP16 create failed (CR-HDR-1),
    // create() falls back to BGRA8 and this returns false — the consumer reads it to decide whether to widen
    // its producer bridge texture to FP16 (the matched-format invariant, CR-HDR-2). false for every other
    // path (default BGRA8). The single source of truth for the producer/consumer format agreement.
    [[nodiscard]] bool present_is_fp16() const noexcept;

    // R-D2-3 (parallel-instances device-loss scoping — FG_PARALLEL_INSTANCES_RISK_REGISTER §R-D2-3).
    // true iff a TERMINAL DXGI device loss (DEVICE_REMOVED/RESET/DRIVER_INTERNAL_ERROR) was observed
    // at THIS surface's present site (latched once, inside submit()). The same loss is ALSO surfaced
    // synchronously as the submit() result ErrorCode::ShuttingDown — that is the v1 single-instance
    // contract and is unchanged. This accessor is the PER-INSTANCE scoping primitive for the D2
    // parallel-FG objective: a future multi-instance orchestrator polls it to attribute the loss to
    // the OWNING instance and quit only that instance, instead of consulting the process-global quit
    // flag (so a TDR on instance B's device does NOT kill instance A). false for a default/moved-from
    // surface and on the happy path → a single-instance consumer that never calls it is byte-identical.
    // COVERAGE: inert on the default single-game path (the operator runs one game).
    [[nodiscard]] bool device_lost() const noexcept;

    // W4 (FG fluidity PACING axis — measurement-only): the LAST display-FLIP's statistics for the
    // OWN-WINDOW flip swapchain (DXGI_FRAME_STATISTICS via IDXGISwapChain::GetFrameStatistics). The
    // consumer differences SyncQPCTime across presents so it can score fluidity PACING on DISPLAY-FLIP
    // time (the SOTA's binding MUST) instead of present-CALL time (MsBetweenPresents — which the
    // --pace-hard pacer directly controls, making a pacing A/B on it partly CIRCULAR).
    //   - sync_qpc      = SyncQPCTime.QuadPart — the QPC tick of the vblank that scanned out the last
    //                     completed present (the display-flip instant).
    //   - present_count = PresentCount — # of presents that have reached the screen (advances per flip);
    //                     the consumer trusts a SyncQPCTime delta ONLY when this advanced since its last sample.
    // Returns nullopt (SOFT-FAIL — a per-config OUTCOME, never a defect) on: a non-own-window/composed
    // surface (CreateSwapChainForComposition does NOT reliably yield flip stats — the composed-path YIELD
    // is rig-pending), DXGI_ERROR_FRAME_STATISTICS_DISJOINT, or any other GetFrameStatistics HRESULT
    // failure (e.g. before the first flip). nullopt → the consumer leaves MsBetweenDisplayChange NA → the
    // fluidity scorer falls back to the present-time proxy (flagged). READ-ONLY (GetFrameStatistics changes
    // no present-path state) → byte-identical to a run that never calls it. nullopt for a default/moved-from surface.
    struct FlipStats {
        uint64_t sync_qpc      = 0;   // DXGI_FRAME_STATISTICS.SyncQPCTime.QuadPart (QPC ticks)
        uint32_t present_count = 0;   // DXGI_FRAME_STATISTICS.PresentCount
    };
    [[nodiscard]] std::optional<FlipStats> last_flip_qpc() const noexcept;

    PresentSurface() noexcept = default;
    PresentSurface(PresentSurface&&) noexcept;
    PresentSurface& operator=(PresentSurface&&) noexcept;
    ~PresentSurface() noexcept;                 // destroys HWND/device/swapchain
    PresentSurface(const PresentSurface&)            = delete;
    PresentSurface& operator=(const PresentSurface&) = delete;

private:
    // Opaque platform state — keeps <windows.h>/<d3d11.h>/<dcomp.h> out of this
    // header. nullptr ⇒ a moved-from / default-constructed (non-functional) surface.
    struct Impl;
    Impl* impl_ = nullptr;

    explicit PresentSurface(Impl* impl) noexcept : impl_(impl) {}
};

} // namespace phyriad::render::present

// Made with my soul - Swately <3
