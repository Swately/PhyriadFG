// PhyriadFG - src/core/frame_vram.hpp : --frame-vram, the lever of docs/planning/O0_FREEZE_LEVER1.md (default OFF).
// The lever keeps the CONVERTED frame on the device. A full-ring mirror (one R8G8B8A8_UNORM WW x WH image per ACTIVE
// capture-ring slot, the same slot index as the host ring hostR[s]) is filled by a device copy right after the
// convert, and the flow's and the presenter's frame inputs are device copies from it: the host crossings C.download,
// F.upload and P.upload_frames become device-to-device copies. Awork, the mirror, Bframe and wapPrevA / wapCurA are
// all R8G8B8A8_UNORM, so every copy on the new route is same-format. The mirror uses the capture ring's own slot
// indices, so the ring's torn-read guard (kCapSlotsMin >= kIngestBacklog + 1) protects it exactly as it protects the
// host ring. The host ring stays allocated; the lever refuses to arm wherever something still READS it.
// This header holds only the arming decision, kept out of init_devices so a CPU test can check every refusal.
// Made with my soul - Swately <3
#pragma once

namespace pfg::core {

struct FrameVramGate {
    bool requested = false;         // --frame-vram
    bool single_gpu = false;        // the mirror, the convert, the flow and the presenter must share device A
    bool use_igpu_convert = false;  // the iGPU convert writes the host ring itself (no A-side convert to mirror)
    bool use_wap = false;           // the presenter upload the lever replaces is the WAP one
    bool use_upscale = false;       // the upscale path reads the host ring
    bool upload_xfer = false;       // --upload-xfer records the presenter upload on another queue family (A.qT)
    bool real_fast_path = false;    // --real-fast-path blits a real from the host ring to the present
    bool rfp_fresh = false;         // --rfp-fresh presents the freshest captured real from the host ring
    bool motion_fallback = false;   // --motion-fallback presents a real from the host ring (rfp_present, present.cpp)
    int  dump_n = 0;                // --dump reads the host ring
    int  pairdump_n = 0;            // --pairdump reads the host ring
};

// true = arm. false with *why set = requested and refused: the caller prints the reason and FAILS the run (fatal,
// init_devices returns false), so a run that asked for the lever never runs without it.
// false with *why == nullptr = not requested: off, the host round trip, byte-identical.
inline bool frame_vram_arm(const FrameVramGate& g, const char** why) {
    *why = nullptr;
    if (!g.requested) return false;
    if (!g.single_gpu)      { *why = "needs the single-GPU route (the convert, the flow and the presenter must share device A)"; return false; }
    if (g.use_igpu_convert) { *why = "the iGPU convert writes the host ring itself"; return false; }
    if (!g.use_wap)         { *why = "needs --warp-at-presenter (the presenter upload it replaces is the WAP one)"; return false; }
    if (g.use_upscale)      { *why = "the upscale path reads the host ring"; return false; }
    if (g.upload_xfer)      { *why = "--upload-xfer records the presenter upload on another queue family"; return false; }
    if (g.real_fast_path)   { *why = "--real-fast-path reads the host ring"; return false; }
    if (g.rfp_fresh)        { *why = "--rfp-fresh reads the host ring"; return false; }
    if (g.motion_fallback)  { *why = "--motion-fallback presents a real from the host ring"; return false; }
    if (g.dump_n > 0)       { *why = "--dump reads the host ring"; return false; }
    if (g.pairdump_n > 0)   { *why = "--pairdump reads the host ring"; return false; }
    return true;
}

// After every init cascade: a route the gate armed can still lose warp-at-presenter to a later allocation failure
// (init_host_bridge, init_wap), and the non-WAP presenter reads the host ring. Returns the reason an armed lever must
// fail the run, or nullptr (not armed, or the route held).
inline const char* frame_vram_route_lost(bool armed, bool use_wap) {
    return (armed && !use_wap) ? "the warp-at-presenter route was lost after the gate (an allocation failure disabled it)" : nullptr;
}

}  // namespace pfg::core
