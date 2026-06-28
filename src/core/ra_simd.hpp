// apps/render_assistant/src/ra_simd.hpp
// STAGE-86b: the app's ONE flagged-dispatch seam onto the hal fp16 codec (FR-HAL-1).
//
// WHY a separate TU: main.cpp must stay portable / dual-build (MinGW g++ + MSVC cl) and is
// compiled WITHOUT -mavx2/-mf16c (so the binary runs on any x86_64). But the F16C 8-wide
// decode path in phyriad::hal::f16_to_f32_batch is only EMITTED when the calling TU defines
// __F16C__ (i.e. was compiled with -mf16c / /arch:AVX2). So the batch call lives in ra_simd.cpp,
// the ONE translation unit the CMake build flags with the SIMD arch (the established
// framework/transport/src/SlotCopy.cpp + bench/copy_bench precedent). At runtime the hal batch
// STILL gates on simd_caps().f16c, so this is safe on CPUs without F16C — it just takes the
// byte-identical scalar tail there.
//
// main.cpp (no flags) calls ra::decode_f16 for the gme_fit decode-once; the actual bit-math
// is the hal pillar primitive (no hand-rolled second decode anywhere in the app).
#pragma once
#include <cstddef>
#include <cstdint>

namespace ra {

// Batch decode n binary16 (interleaved or planar — caller's choice) → n float, into dst.
// Thin forwarder to phyriad::hal::f16_to_f32_batch; the F16C path is reached because THIS
// declaration's definition (ra_simd.cpp) is the flagged TU. dst/src may not alias.
void decode_f16(const uint16_t* src, float* dst, std::size_t n) noexcept;

} // namespace ra
