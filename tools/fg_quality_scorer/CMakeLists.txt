# catalog/cpp/render/vulkan/bench/fg_quality_scorer/CMakeLists.txt
# FG-QUALITY SCORER — the HSR120 §11.4 layer-2 "number producer" — STANDALONE
# (not wired into the render pillar's CMake / CI / root; a run-on-the-rig
# measurement like stage11/stage31). Headless compute (no WSI). REUSES,
# read-only: the committed optical-flow shaders + OpticalFlowPipeline.cpp +
# spv_to_header.cmake. Mirrors stage11_blockmatch_quality/CMakeLists.txt.
# Build (needs VULKAN_SDK):
#   cmake -S tools/fg_quality_scorer -B build-fgq -G Ninja
#   cmake --build build-fgq
#   ./build-fgq/fg_quality_scorer <manifest.txt> [gpu_index] [out.csv]
cmake_minimum_required(VERSION 3.20)
project(fg_quality_scorer LANGUAGES CXX)

# The scorer hard-codes /O2 below. With no build type CMake defaults to Debug on MSVC, which adds
# /RTC1 /Od, and cl refuses "/RTC1 with /O2" (D8016) — the port hit this immediately. Default to
# Release: it is a MEASUREMENT tool, an unoptimised build would be the wrong thing to measure with.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

find_package(Vulkan REQUIRED)

find_program(FGQ_GLSLC glslc
    HINTS
        "${Vulkan_INCLUDE_DIRS}/../Bin" "${Vulkan_INCLUDE_DIRS}/../bin"
        "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/bin"
    DOC "Path to glslc (GLSL → SPIR-V) from the Vulkan SDK")
if(NOT FGQ_GLSLC)
    message(FATAL_ERROR "fg_quality_scorer: glslc not found (need VULKAN_SDK).")
endif()
message(STATUS "fg_quality_scorer: glslc = ${FGQ_GLSLC}")

# PORTED INTO PhyriadFG 2026-09-03 (S2.T0). The catalog copy pointed at ITS OWN render/vulkan; this
# one points at the repo's VENDORED pipeline, because the two lineages diverged (427/146 diff lines,
# measured 2026-09-02) and a scorer built against the catalog does not score THIS product's flow.
set(_render_vulkan "${CMAKE_CURRENT_SOURCE_DIR}/../../framework/render/vulkan")
set(_spv2hdr       "${_render_vulkan}/cmake/spv_to_header.cmake")
set(_gen_dir       "${CMAKE_CURRENT_BINARY_DIR}/gen")

function(fgq_compile_spv _src _hdr _var)
    get_filename_component(_stem "${_src}" NAME_WE)
    set(_spv "${_gen_dir}/${_stem}.spv")
    add_custom_command(OUTPUT "${_spv}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${_gen_dir}"
        COMMAND "${FGQ_GLSLC}" --target-env=vulkan1.2 -O "${_src}" -o "${_spv}"
        DEPENDS "${_src}" COMMENT "glslc ${_stem}" VERBATIM)
    add_custom_command(OUTPUT "${_hdr}"
        COMMAND ${CMAKE_COMMAND} -D "INPUT=${_spv}" -D "OUTPUT=${_hdr}"
                -D "VARNAME=${_var}" -P "${_spv2hdr}"
        DEPENDS "${_spv}" "${_spv2hdr}" COMMENT "spv -> header: ${_var}" VERBATIM)
endfunction()

# The three committed shaders OpticalFlowPipeline.cpp #includes (verified
# first-hand: src/OpticalFlowPipeline.cpp:63-65). No local warp shader — the
# scorer drives the COMMITTED warp via the pipeline, by design (honesty note in
# main.cpp / README). Generated headers land in _gen_dir, added to the include
# path so OpticalFlowPipeline.cpp finds them exactly as the pillar build does.
fgq_compile_spv("${_render_vulkan}/shaders/optical_flow_pyr_down.comp"
    "${_gen_dir}/optical_flow_pyr_down_spv.hpp" "kOpticalFlowPyrDownSpv")
fgq_compile_spv("${_render_vulkan}/shaders/optical_flow_hier_match.comp"
    "${_gen_dir}/optical_flow_hier_match_spv.hpp" "kOpticalFlowHierMatchSpv")
# CANON #12 fork (the FG-variant matcher): OpticalFlowPipeline.cpp #includes its header
# (src/OpticalFlowPipeline.cpp:85), so this standalone build MUST generate it too — else the
# scorer (which compiles OpticalFlowPipeline.cpp) fails. Mirrors the pillar CMake (:131-136).
fgq_compile_spv("${_render_vulkan}/shaders/optical_flow_hier_match_fg.comp"
    "${_gen_dir}/optical_flow_hier_match_fg_spv.hpp" "kOpticalFlowHierMatchFgSpv")
fgq_compile_spv("${_render_vulkan}/shaders/optical_flow_warp.comp"
    "${_gen_dir}/optical_flow_warp_spv.hpp" "kOpticalFlowWarpSpv")
# ARC-A LEVER-1: the per-tile affine-fit post-pass shader the pipeline #includes when armed.
fgq_compile_spv("${_render_vulkan}/shaders/optical_flow_affine_fit.comp"
    "${_gen_dir}/optical_flow_affine_fit_spv.hpp" "kOpticalFlowAffineFitSpv")
add_custom_target(fgq_shaders DEPENDS
    "${_gen_dir}/optical_flow_pyr_down_spv.hpp"
    "${_gen_dir}/optical_flow_hier_match_spv.hpp"
    "${_gen_dir}/optical_flow_hier_match_fg_spv.hpp"
    "${_gen_dir}/optical_flow_warp_spv.hpp"
    "${_gen_dir}/optical_flow_affine_fit_spv.hpp")

add_executable(fg_quality_scorer
    main.cpp
    "${_render_vulkan}/src/OpticalFlowPipeline.cpp")
add_dependencies(fg_quality_scorer fgq_shaders)
target_compile_features(fg_quality_scorer PRIVATE cxx_std_17)
target_include_directories(fg_quality_scorer PRIVATE
    "${_render_vulkan}/include" "${_gen_dir}")
target_compile_definitions(fg_quality_scorer PRIVATE
    "FGQ_DEFAULT_MANIFEST=\"${CMAKE_CURRENT_SOURCE_DIR}/frames/manifest.txt\"")
target_link_libraries(fg_quality_scorer PRIVATE Vulkan::Vulkan)
if(MSVC)
    # /O2 must NOT be hard-coded: with a Debug configuration MSVC also passes /RTC1, and cl refuses
    # the pair (D8016). Let the build type carry the optimisation; keep the warning level always.
    target_compile_options(fg_quality_scorer PRIVATE /W4)
else()
    target_compile_options(fg_quality_scorer PRIVATE -Wall -Wextra -O2)
endif()
# Made with my soul - Swately <3
