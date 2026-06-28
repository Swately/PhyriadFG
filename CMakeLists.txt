# PhyriadFG/CMakeLists.txt
# Standalone single-target build of phyriad_fg.exe.
#
# One target, one build-dir, one command (build.bat). No add_subdirectory, no libraries:
# the framework .cpp sources are compiled by direct inclusion, so there is no build explosion.
#
# Build (MSVC + WGC):
#   build.bat
#   build\phyriad_fg.exe --help
cmake_minimum_required(VERSION 3.20)
project(phyriad_fg LANGUAGES CXX)

find_package(Vulkan REQUIRED)

find_program(PFG_GLSLC glslc
    HINTS
        "${Vulkan_INCLUDE_DIRS}/../Bin" "${Vulkan_INCLUDE_DIRS}/../bin"
        "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/bin"
    DOC "glslc (GLSL -> SPIR-V) from the Vulkan SDK")
if(NOT PFG_GLSLC)
    message(FATAL_ERROR "phyriad_fg: glslc not found (set VULKAN_SDK).")
endif()
message(STATUS "phyriad_fg: glslc = ${PFG_GLSLC}")

set(_fw  "${CMAKE_CURRENT_SOURCE_DIR}/framework")
set(_rv  "${_fw}/render/vulkan")
set(_spv2hdr "${_rv}/cmake/spv_to_header.cmake")
set(_gen "${CMAKE_CURRENT_BINARY_DIR}/gen")

# .comp -> .spv -> constexpr-array header.
function(pfg_spv _src _hdr _var)
    get_filename_component(_stem "${_src}" NAME_WE)
    set(_spv "${_gen}/${_stem}.spv")
    add_custom_command(OUTPUT "${_spv}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${_gen}"
        COMMAND "${PFG_GLSLC}" --target-env=vulkan1.2 -O "${_src}" -o "${_spv}"
        DEPENDS "${_src}" COMMENT "glslc ${_stem}" VERBATIM)
    add_custom_command(OUTPUT "${_hdr}"
        COMMAND ${CMAKE_COMMAND} -D "INPUT=${_spv}" -D "OUTPUT=${_hdr}"
                -D "VARNAME=${_var}" -P "${_spv2hdr}"
        DEPENDS "${_spv}" "${_spv2hdr}" COMMENT "spv->hdr: ${_var}" VERBATIM)
endfunction()

# --- render/vulkan shaders (consumed by OpticalFlowPipeline.cpp) ---
pfg_spv("${_rv}/shaders/optical_flow_pyr_down.comp"     "${_gen}/optical_flow_pyr_down_spv.hpp"     "kOpticalFlowPyrDownSpv")
pfg_spv("${_rv}/shaders/optical_flow_hier_match.comp"   "${_gen}/optical_flow_hier_match_spv.hpp"   "kOpticalFlowHierMatchSpv")
pfg_spv("${_rv}/shaders/optical_flow_hier_match_fg.comp" "${_gen}/optical_flow_hier_match_fg_spv.hpp" "kOpticalFlowHierMatchFgSpv")
pfg_spv("${_rv}/shaders/optical_flow_warp.comp"         "${_gen}/optical_flow_warp_spv.hpp"         "kOpticalFlowWarpSpv")
pfg_spv("${_rv}/shaders/optical_flow_affine_fit.comp"   "${_gen}/optical_flow_affine_fit_spv.hpp"   "kOpticalFlowAffineFitSpv")

# --- app shaders (included by src/core/main.cpp) ---
set(_sh "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
pfg_spv("${_sh}/hdr_convert.comp"        "${_gen}/hdr_convert_spv.hpp"        "kHdrConvertSpv")
pfg_spv("${_sh}/upscale_bilinear.comp"   "${_gen}/upscale_bilinear_spv.hpp"   "kUpscaleBilinearSpv")
pfg_spv("${_sh}/upscale_lanczos.comp"    "${_gen}/upscale_lanczos_spv.hpp"    "kUpscaleLanczosSpv")
pfg_spv("${_sh}/igpu_convert_pack.comp"  "${_gen}/igpu_convert_pack_spv.hpp"  "kIgpuConvertPackSpv")
pfg_spv("${_sh}/igpu_field.comp"         "${_gen}/igpu_field_spv.hpp"         "kIgpuFieldSpv")
pfg_spv("${_sh}/wap_fill.comp"           "${_gen}/wap_fill_spv.hpp"           "kWapFillSpv")
pfg_spv("${_sh}/unpack_packed.comp"      "${_gen}/unpack_packed_spv.hpp"      "kUnpackPackedSpv")
pfg_spv("${_sh}/mv_smooth.comp"          "${_gen}/mv_smooth_spv.hpp"          "kMvSmoothSpv")
pfg_spv("${_sh}/wap_warp.comp"           "${_gen}/wap_warp_spv.hpp"           "kWapWarpSpv")
pfg_spv("${_sh}/mv_median.comp"          "${_gen}/mv_median_spv.hpp"          "kMvMedianSpv")
pfg_spv("${_sh}/nvofa_convert.comp"      "${_gen}/nvofa_convert_spv.hpp"      "kNvofaConvertSpv")
pfg_spv("${_sh}/gme_reduce.comp"         "${_gen}/gme_reduce_spv.hpp"         "kGmeReduceSpv")
pfg_spv("${_sh}/gme_solve.comp"          "${_gen}/gme_solve_spv.hpp"          "kGmeSolveSpv")
pfg_spv("${_sh}/gme_dissidence.comp"     "${_gen}/gme_dissidence_spv.hpp"     "kGmeDissidenceSpv")
pfg_spv("${_sh}/overlay_fps.comp"        "${_gen}/overlay_fps_spv.hpp"        "kOverlayFpsSpv")

add_custom_target(pfg_shaders DEPENDS
    "${_gen}/optical_flow_pyr_down_spv.hpp" "${_gen}/optical_flow_hier_match_spv.hpp"
    "${_gen}/optical_flow_hier_match_fg_spv.hpp" "${_gen}/optical_flow_warp_spv.hpp"
    "${_gen}/optical_flow_affine_fit_spv.hpp"
    "${_gen}/hdr_convert_spv.hpp" "${_gen}/upscale_bilinear_spv.hpp" "${_gen}/upscale_lanczos_spv.hpp"
    "${_gen}/igpu_convert_pack_spv.hpp" "${_gen}/igpu_field_spv.hpp" "${_gen}/wap_fill_spv.hpp"
    "${_gen}/unpack_packed_spv.hpp" "${_gen}/mv_smooth_spv.hpp" "${_gen}/wap_warp_spv.hpp"
    "${_gen}/mv_median_spv.hpp" "${_gen}/nvofa_convert_spv.hpp" "${_gen}/gme_reduce_spv.hpp"
    "${_gen}/gme_solve_spv.hpp" "${_gen}/gme_dissidence_spv.hpp" "${_gen}/overlay_fps_spv.hpp")

add_executable(phyriad_fg
    src/core/main.cpp
    src/cli/cli.cpp                            # the CLI/config layer (Config + print_help + parse_args)
    src/core/vk_util.cpp                       # now_ms / route_for / Img+HBuf factories / barrier+submit helpers
    src/core/device.cpp                        # VDev + vdev_create/vdev_destroy
    src/core/globals.cpp                       # g_quit/device-lost/util+overlay atomics + vk_live + console Ctrl handler
    src/flow/flow.cpp                          # the FLOW factories (gme_fit_affine / NvofaProvider / MvSmoothPipe / GmePipe+gme_record / MedianPipe)
    src/capture/capture.cpp                    # D3D11/DXGI interop (D3D/d3d_init/find_window/staging) + iGPU ConvPackPipe/UnpackPipe
    src/warp_blend/warp_blend.cpp              # WapPipe/FieldPipe/FillPipe factories + matte_mass_count/gme_dispct_from_mask
    src/present/present.cpp                     # the present-side UpPipe upscale (bilinear/lanczos) factory
    src/instrument/instrument.cpp              # the dump_bmp/dump_rgba diagnostic frame-dump helpers
    src/core/ra_simd.cpp                       # F16C batch — /arch:AVX2 below; main.cpp stays portable
    "${_rv}/src/OpticalFlowPipeline.cpp"       # render/vulkan optical-flow pipeline
    "${_fw}/render/present/src/PresentSurface.cpp"
    "${_fw}/topology/src/HardwareTopology.cpp"
    "${_fw}/topology/src/CacheInfo.cpp"
    "${_fw}/topology/src/VCacheDetector.cpp"
    "${_fw}/topology/src/HybridCoreDetector.cpp"
    "${_fw}/topology/src/PCIeAffinityProbe.cpp")
add_dependencies(phyriad_fg pfg_shaders)

target_compile_features(phyriad_fg PRIVATE cxx_std_23)
target_include_directories(phyriad_fg PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"          # so #include "cli/cli.hpp" resolves (same-dir quoted includes still win for main.cpp's "telemetry_csv.hpp" etc.)
    "${_rv}/include" "${_gen}"
    "${_fw}/hal/include"
    "${_fw}/render/present/include"
    "${_fw}/schema/include"
    "${_fw}/topology/include")
target_compile_definitions(phyriad_fg PRIVATE WINVER=0x0A00 _WIN32_WINNT=0x0A00)
target_link_libraries(phyriad_fg PRIVATE Vulkan::Vulkan d3d11 dxgi dcomp gdi32 user32 winmm pdh dwmapi avrt)

# LTO / IPO — cross-TU inlining so the per-layer module split is performance-free (the compiler inlines
# across capture/flow/warp_blend/present/core/framework .cpp as if it were one TU, via whole-program
# optimization). MSVC: /GL + /LTCG.
include(CheckIPOSupported)
check_ipo_supported(RESULT _pfg_ipo OUTPUT _pfg_ipo_msg)
if(_pfg_ipo)
    set_property(TARGET phyriad_fg PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    message(STATUS "phyriad_fg: LTO/IPO enabled (module split is perf-free / whole-program optimized)")
else()
    message(STATUS "phyriad_fg: LTO/IPO unavailable -> ${_pfg_ipo_msg}")
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release)
endif()
if(MSVC)
    target_compile_options(phyriad_fg PRIVATE /W4 /EHsc)
    target_link_libraries(phyriad_fg PRIVATE windowsapp)   # C++/WinRT (WGC backend)
    set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/src/core/ra_simd.cpp"
        PROPERTIES COMPILE_OPTIONS "/arch:AVX2")
else()
    target_compile_options(phyriad_fg PRIVATE -Wall -Wextra -Wno-misleading-indentation -O2)
    set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/src/core/ra_simd.cpp"
        PROPERTIES COMPILE_OPTIONS "-mavx2;-mf16c")
endif()
# Made with my soul - Swately <3
