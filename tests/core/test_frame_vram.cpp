// PhyriadFG - tests/core/test_frame_vram.cpp : the arming decision of --frame-vram (src/core/frame_vram.hpp), the lever
// of docs/planning/O0_FREEZE_LEVER1.md, tested without a GPU. It checks that the lever arms on the shipping
// single-GPU route, stays silent when not requested, and refuses, with a reason, every configuration in which
// something still reads the host ring. Exit 0 = every case as expected.
// Made with my soul - Swately <3
#include "core/frame_vram.hpp"

#include <cstdio>
#include <cstring>

namespace {

int g_fail = 0;

void expect(bool cond, const char* what) {
    if (!cond) { std::printf("FAIL: %s\n", what); ++g_fail; }
    else       { std::printf("ok:   %s\n", what); }
}

pfg::core::FrameVramGate shipping() {   // the released configuration on this rig: single GPU, WAP, nothing else
    pfg::core::FrameVramGate g{};
    g.requested = true; g.single_gpu = true; g.use_wap = true;
    return g;
}

void refused(pfg::core::FrameVramGate g, const char* needle, const char* what) {
    const char* why = nullptr;
    const bool armed = pfg::core::frame_vram_arm(g, &why);
    expect(!armed && why != nullptr && std::strstr(why, needle) != nullptr, what);
    if (why) std::printf("      reason: %s\n", why);
}

}  // namespace

int main() {
    {   // F1: not requested -> off, no reason (byte-identical off)
        pfg::core::FrameVramGate g = shipping(); g.requested = false;
        const char* why = reinterpret_cast<const char*>(1);
        const bool armed = pfg::core::frame_vram_arm(g, &why);
        expect(!armed && why == nullptr, "F1 not requested: off and silent");
    }
    {   // F2: the shipping route arms
        const char* why = nullptr;
        const bool armed = pfg::core::frame_vram_arm(shipping(), &why);
        expect(armed && why == nullptr, "F2 shipping single-GPU WAP route: armed");
    }
    // F3..F11: each host-ring reader, or a route the mirror cannot serve, refuses with its own reason
    { auto g = shipping(); g.single_gpu = false;      refused(g, "single-GPU",        "F3 multi-GPU route refused"); }
    { auto g = shipping(); g.use_igpu_convert = true; refused(g, "iGPU convert",      "F4 iGPU convert refused"); }
    { auto g = shipping(); g.use_wap = false;         refused(g, "warp-at-presenter", "F5 non-WAP presenter refused"); }
    { auto g = shipping(); g.use_upscale = true;      refused(g, "upscale",           "F6 upscale refused"); }
    { auto g = shipping(); g.upload_xfer = true;      refused(g, "--upload-xfer",     "F7 --upload-xfer refused"); }
    { auto g = shipping(); g.real_fast_path = true;   refused(g, "--real-fast-path",  "F8 --real-fast-path refused"); }
    { auto g = shipping(); g.rfp_fresh = true;        refused(g, "--rfp-fresh",       "F9 --rfp-fresh refused"); }
    { auto g = shipping(); g.dump_n = 3;              refused(g, "--dump",            "F10 --dump refused"); }
    { auto g = shipping(); g.pairdump_n = 2;          refused(g, "--pairdump",        "F11 --pairdump refused"); }
    if (g_fail) std::printf("FRAME_VRAM TEST: %d FAILED\n", g_fail);
    else        std::printf("FRAME_VRAM TEST: all passed\n");
    return g_fail ? 1 : 0;
}
