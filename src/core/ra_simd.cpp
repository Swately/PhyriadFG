// apps/render_assistant/src/ra_simd.cpp
// STAGE-86b: the ONE flagged translation unit in render_assistant. Compiled with
// -mavx2 -mf16c (GCC/Clang) or /arch:AVX2 (MSVC) via set_source_files_properties in
// the app CMakeLists — mirroring framework/transport/src/SlotCopy.cpp + bench/copy_bench.
//
// Because __F16C__ is defined HERE (and only here), phyriad::hal::f16_to_f32_batch emits its
// F16C 8-wide _mm256_cvtph_ps path; it still runtime-gates on simd_caps().f16c so the produced
// binary remains correct on any x86_64 CPU. main.cpp stays portable (no arch flags) and calls
// ra::decode_f16. We do NOT re-implement the decode — the bit-math is the hal pillar primitive.
#include "ra_simd.hpp"
#include <phyriad/hal/Simd.hpp>   // FR-HAL-1: the pillar fp16 codec (f16_to_f32_batch)

namespace ra {

void decode_f16(const uint16_t* src, float* dst, std::size_t n) noexcept {
    phyriad::hal::f16_to_f32_batch(src, dst, n);
}

} // namespace ra
// Made with my soul - Swately <3
