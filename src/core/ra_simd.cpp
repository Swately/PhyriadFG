// src/core/ra_simd.cpp
// The one translation unit compiled with the SIMD arch flags (-mavx2 -mf16c on GCC/Clang,
// /arch:AVX2 on MSVC) via set_source_files_properties in the CMakeLists.
//
// Because __F16C__ is defined here (and only here), phyriad::hal::f16_to_f32_batch emits its
// F16C 8-wide _mm256_cvtph_ps path; it still runtime-gates on simd_caps().f16c so the binary
// stays correct on any x86_64 CPU. main.cpp stays portable (no arch flags) and calls
// ra::decode_f16. The decode is not re-implemented here — the bit-math is the hal pillar primitive.
#include "ra_simd.hpp"
#include <phyriad/hal/Simd.hpp>   // the pillar fp16 codec (f16_to_f32_batch)

namespace ra {

void decode_f16(const uint16_t* src, float* dst, std::size_t n) noexcept {
    phyriad::hal::f16_to_f32_batch(src, dst, n);
}

} // namespace ra
// Made with my soul - Swately <3
