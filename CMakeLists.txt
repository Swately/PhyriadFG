# PhyriadFG/CMakeLists.txt
# Standalone single-target build of phyriad_fg.exe.
#
# One SHIPPING target, one build-dir, one command (build.bat). No add_subdirectory, no libraries:
# (corrected 2026-09-04: there are four add_executable targets now — phyriad_fg plus the two test
#  binaries and the layer generator. There is still no enable_testing()/add_test(), so nothing runs
#  the tests automatically.)
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

# PFG_NOCONTRACT (EXPERIMENT, default OFF, never shipped): after glslc, tools/spv_nocontract.py decorates every
# float result in the module NoContraction, so the driver compiler may not fuse a*b+c into FMA. Used by R3 to
# attribute the 1-level --fg-core-ab residual: with BOTH kernels contraction-free the residual is either gone
# (= contraction) or not (= something else). It runs INSIDE the glslc rules so Ninja's depfile log cannot
# silently recompile one module without it (that happened on the first attempt, 2026-09-05).
option(PFG_NOCONTRACT "EXPERIMENT: NoContraction on every float op of every .spv (R3 M4 attribution); never ship" OFF)
find_program(PFG_PYTHON NAMES python python3)
set(_nocontract "${CMAKE_CURRENT_SOURCE_DIR}/tools/spv_nocontract.py")
if(PFG_NOCONTRACT)
    message(STATUS "phyriad_fg: PFG_NOCONTRACT=ON -- EXPERIMENT build, every .spv decorated NoContraction; do not ship")
endif()

# .comp -> .spv -> constexpr-array header.
function(pfg_spv _src _hdr _var)
    get_filename_component(_stem "${_src}" NAME_WE)
    set(_spv "${_gen}/${_stem}.spv")
    if(PFG_NOCONTRACT)
        set(_nc_cmd COMMAND "${PFG_PYTHON}" "${_nocontract}" "${_spv}" "${_spv}")
        set(_nc_dep "${_nocontract}")
        set(_nc_tag " +NoContraction")
    else()
        set(_nc_cmd)
        set(_nc_dep)
        set(_nc_tag "")
    endif()
    add_custom_command(OUTPUT "${_spv}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${_gen}"
        COMMAND "${PFG_GLSLC}" --target-env=vulkan1.2 -O "${_src}" -o "${_spv}"
        ${_nc_cmd}
        DEPENDS "${_src}" ${_nc_dep} COMMENT "glslc ${_stem}${_nc_tag}" VERBATIM)
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
    src/clock/phase_clock.cpp                   # R1: STAGE 4 — the content clock (NCO+PLL, selection, phase, order)
    src/layers/layer_registry.cpp              # R0: the layer registry (shadow parser, parity, help, UI model, dump, hash)
    src/core/vk_util.cpp                       # now_ms / route_for / Img+HBuf factories / barrier+submit helpers
    src/core/device.cpp                        # VDev + vdev_create/vdev_destroy
    src/core/globals.cpp                       # g_quit/device-lost/util+overlay atomics + vk_live + console Ctrl handler
    src/flow/flow.cpp                          # the FLOW factories (gme_fit_affine / NvofaProvider / MvSmoothPipe / GmePipe+gme_record / MedianPipe)
    src/flow/flow_init.cpp                     # E1: flow-side init sections (gme-gpu pipes + use_* re-finalization), verbatim from main.cpp
    src/flow/holons.cpp                        # R5 step 3a: the holon leaves (object_repair, mem_advect/merge/refresh), moved by anchor from run_flow
    src/flow/flow_consume.cpp                   # R5 step 3c: the FLOW stage per-pair CPU tail + F->P publish, moved by anchor from run_flow
    src/capture/capture.cpp                    # D3D11/DXGI interop (D3D/d3d_init/find_window/staging) + iGPU ConvPackPipe/UnpackPipe
    src/capture/capture_init.cpp               # E1: capture-side init sections (convert pipe + iGPU pipes + WGC backend), verbatim from main.cpp
    src/warp_blend/warp_blend.cpp              # WapPipe/FieldPipe/FillPipe factories + matte_mass_count/gme_dispct_from_mask
    src/warp_blend/warp_blend_init.cpp         # E1: the WAP init section, verbatim from main.cpp
    src/present/present.cpp                     # the present-side UpPipe upscale (bilinear/lanczos) factory
    src/present/present_init.cpp               # E1: the present-side init sections (bridge slots), verbatim from main.cpp
    src/present/present_stage.cpp              # R4: STAGE 6 -- the present stage (surface, slots, submit, present, FlipStats, --tdr-test)
    src/instrument/instrument.cpp              # the dump_bmp/dump_rgba diagnostic frame-dump helpers
    src/core/ra_simd.cpp                       # F16C batch — /arch:AVX2 below; main.cpp stays portable
    src/core/core_init.cpp                     # E1: core-side init sections (cmd buffers + fences), verbatim from main.cpp
    "${_rv}/src/OpticalFlowPipeline.cpp"       # render/vulkan optical-flow pipeline
    "${_fw}/render/present/src/PresentSurface.cpp"
    "${_fw}/topology/src/HardwareTopology.cpp"
    "${_fw}/topology/src/CacheInfo.cpp"
    "${_fw}/topology/src/VCacheDetector.cpp"
    "${_fw}/topology/src/HybridCoreDetector.cpp"
    "${_fw}/topology/src/PCIeAffinityProbe.cpp")
add_dependencies(phyriad_fg pfg_shaders)

# ── R0: the layer registry generator (tools/layer_gen.cpp) — a host tool built from the SAME X-macro tables,
#    emitting the GLSL chains / spec constants / std140 UBO / offsets into ${_gen}/shaders. Nothing includes
#    them yet (shaders/fg_core.comp is stage R3); they are generated so the mechanism is proven at M-R0 and
#    so a .def change rebuilds them (the DEPENDS discipline of risk XR13 starts here).
# R2: the adopted SEAM engine's GPU-free unit test (the 122 derivation checks). Vulkan is consumed for
# its synchronization2 TYPE definitions only; execute() is never called, so no device is needed.
add_executable(pfg_seam_test tests/seam/test_seam_graph.cpp)
target_compile_features(pfg_seam_test PRIVATE cxx_std_23)
target_include_directories(pfg_seam_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(pfg_seam_test PRIVATE Vulkan::Vulkan)

# R1: the STAGE-4 CLOCK's CPU test (GPU-free, Vulkan-free). Built with the tree so the extraction keeps a
# standing regression harness; pass it a --arrival-log recording to assert bit-parity of the replay.
add_executable(pfg_clock_test tests/clock/test_phase_clock.cpp src/clock/phase_clock.cpp)
target_compile_features(pfg_clock_test PRIVATE cxx_std_23)
target_include_directories(pfg_clock_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

# R3: the row bodies are inputs of the generator (it checks them) and of fg_core.spv (it includes them).
file(GLOB _layer_bodies CONFIGURE_DEPENDS "${_sh}/layers/*.glsl")
add_executable(pfg_layer_gen tools/layer_gen.cpp)
target_compile_features(pfg_layer_gen PRIVATE cxx_std_23)
target_include_directories(pfg_layer_gen PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(_layer_gen_out
    "${_gen}/shaders/layer_specconst.glsl" "${_gen}/shaders/layer_params.glsl" "${_gen}/shaders/layer_includes.glsl"
    "${_gen}/shaders/chain_mvcond.glsl" "${_gen}/shaders/chain_sample.glsl" "${_gen}/shaders/chain_weight.glsl" "${_gen}/shaders/chain_compose.glsl"
    "${_gen}/shaders/layer_offsets.hpp" "${_gen}/shaders/layer_manifest.txt")
add_custom_command(OUTPUT ${_layer_gen_out}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${_gen}/shaders"
    COMMAND pfg_layer_gen "${_gen}/shaders" "${_sh}"
    DEPENDS pfg_layer_gen "${CMAKE_CURRENT_SOURCE_DIR}/src/layers/layer_table.def" ${_layer_bodies}
            "${CMAKE_CURRENT_SOURCE_DIR}/src/layers/layer_table.hpp" "${CMAKE_CURRENT_SOURCE_DIR}/src/layers/layer_abi.hpp"
    COMMENT "layer_gen: registry -> generated GLSL/offsets" VERBATIM)
# ── 4.3 — THE TESTS ARE WIRED (2026-09-06) ────────────────────────────────────────────────────────
# Before this block `enable_testing()`/`add_test()` appeared ZERO times: R2's seam checks and R1's clock oracle were
# built by every build and run by nothing. `ctest --test-dir build-release` runs them now.
#
# The discipline this block follows (EMPIRICAL_TEST §3.3 — a gate must be seen red): every positive test that CAN
# have a negative twin has one, and each twin was confirmed to fail before being committed. A test whose failure
# mode has never been observed is a claim, not a gate.
enable_testing()

# 1. R2's seam graph — 148 self-contained checks (mock handles; no device).
add_test(NAME seam_graph COMMAND pfg_seam_test)
set_tests_properties(seam_graph PROPERTIES PASS_REGULAR_EXPRESSION "OK: all [0-9]+ checks passed")

# 2. R1's PhaseClock bit-parity oracle, over a RECORDED arrival log (tests/clock/fixtures/, produced by
#    `phyriad_fg --arrival-log FILE` on the ball zoo). This is the check that had never run: the binary existed,
#    the oracle worked, and nothing ever handed it a log.
add_test(NAME clock_replay_oracle COMMAND pfg_clock_test "${CMAKE_CURRENT_SOURCE_DIR}/tests/clock/fixtures/ball_zoo_60fps_240hz.alog")
# ASCII only in the regex: the RESULT line carries an em-dash, which no single-byte regex atom matches.
set_tests_properties(clock_replay_oracle PROPERTIES PASS_REGULAR_EXPRESSION "replay oracle over [0-9]+ ticks")

# 2b. The same oracle over the two dimensions the first fixture cannot reach (4.3's honesty ledger named this gap):
#     a different SOURCE RATE (120 fps -> a per-pair budget of 8.3 ms instead of 16.7) and a run under real GPU
#     pressure where the governor engaged tier 5, i.e. with the bwd-skip and holon-shedding branches live and
#     span > 1. Both reproduce bit-exactly; if the clock ever stops being deterministic under pressure, THIS is the
#     test that says so, and the idle 60 fps one will not.
add_test(NAME clock_replay_oracle_120fps COMMAND pfg_clock_test "${CMAKE_CURRENT_SOURCE_DIR}/tests/clock/fixtures/ball_zoo_120fps_240hz.alog")
set_tests_properties(clock_replay_oracle_120fps PROPERTIES PASS_REGULAR_EXPRESSION "replay oracle over [0-9]+ ticks")
add_test(NAME clock_replay_oracle_pressured COMMAND pfg_clock_test "${CMAKE_CURRENT_SOURCE_DIR}/tests/clock/fixtures/ball_zoo_120fps_pressured.alog")
set_tests_properties(clock_replay_oracle_pressured PROPERTIES PASS_REGULAR_EXPRESSION "replay oracle over [0-9]+ ticks")

# 3. The synthetic half alone — legitimate, and it must SAY that the oracle did not run (the pre-4.3 build printed
#    "all checks passed (0)" here, which is the false green this step removed).
add_test(NAME clock_synthetic COMMAND pfg_clock_test)
set_tests_properties(clock_synthetic PROPERTIES PASS_REGULAR_EXPRESSION "THE REPLAY ORACLE DID NOT RUN")

# 4. NEGATIVE: told to replay a log it cannot open, the clock test must FAIL. Pre-4.3 it printed SKIP and exited 0 —
#    a missing or renamed fixture would have reported success forever.
add_test(NAME clock_replay_missing_log_fails COMMAND pfg_clock_test "${CMAKE_CURRENT_BINARY_DIR}/no_such_arrival.log")
set_tests_properties(clock_replay_missing_log_fails PROPERTIES WILL_FAIL TRUE)

# 5. The registry's parity oracle over a token corpus. `--dump-config` parses, runs layer_config_parity() and exits
#    (exit 3 on a mismatch) before any device is created — a pure CPU test. These are the combinations that broke
#    the shadow parser during R5 step 1 (the --gme-gpu-verify ordering) plus one eight-token line.
set(_parity_cases
    "" "--no-gme" "--no-objects" "--no-memory" "--no-bidir" "--no-ambig" "--no-inertia"
    "--mv-median" "--no-mv-guided|--mv-median" "--mv-smooth|0.6" "--mv-smooth|2" "--mv-smooth|-1"
    "--nvofa" "--gme-irls2" "--no-gme-gpu" "--gme-gpu-verify"
    "--gme-gpu-verify|--no-gme-gpu" "--no-gme-gpu|--gme-gpu-verify"
    "--no-shapefield" "--no-obj-fill-rim" "--obj-fill-rim" "--no-expire" "--no-persist-reset"
    "--no-mv-consensus" "--mv-consensus" "--no-mv-consensus|--mv-consensus"
    "--no-inertia|--inertia" "--no-ambig|--ambig" "--no-objects|--objects" "--no-memory|--memory"
    "--no-bidir|--bidir" "--no-gme|--gme" "--no-shapefield|--shapefield"
    "--no-gme|--no-objects|--no-memory|--no-bidir|--no-ambig|--no-inertia|--no-mv-consensus|--mv-smooth|0.3")
set(_pc 0)
foreach(_case IN LISTS _parity_cases)
    math(EXPR _pc "${_pc}+1")
    string(REPLACE "|" ";" _argv "${_case}")   # '|' separates tokens INSIDE a case; ';' would end the case (CMake list semantics)
    add_test(NAME "layer_parity_${_pc}" COMMAND phyriad_fg --dump-config ${_argv})
    set_tests_properties("layer_parity_${_pc}" PROPERTIES FAIL_REGULAR_EXPRESSION "PARITY FAIL")
endforeach()

# 6. The contract hash of the DEFAULT resolved chain. Pinned deliberately: any edit to layer_table.def / the ABI
#    that changes the chain fails this test, and updating the pin in the same commit is the point (touchpoints move
#    together). The value below was read from this build's own `--layer-dump`.
add_test(NAME layer_contract_hash COMMAND phyriad_fg --layer-dump)
set_tests_properties(layer_contract_hash PROPERTIES PASS_REGULAR_EXPRESSION "contract=0x9517AE73A530EAFE")

# 7. The CLI's exit codes (4.3's own fix): an unknown option and a missing value are ERRORS (2); --help is not.
add_test(NAME cli_unknown_option_exits_2 COMMAND phyriad_fg --no-such-flag)
set_tests_properties(cli_unknown_option_exits_2 PROPERTIES WILL_FAIL TRUE)
add_test(NAME cli_missing_value_exits_2 COMMAND phyriad_fg --mv-smooth)
set_tests_properties(cli_missing_value_exits_2 PROPERTIES WILL_FAIL TRUE)
add_test(NAME cli_help_exits_0 COMMAND phyriad_fg --help)
set_tests_properties(cli_help_exits_0 PROPERTIES PASS_REGULAR_EXPRESSION "LAYERS \\(the registry")

# 8. NEGATIVE: the layer generator's body checks (R3) must REFUSE to generate from a malformed row body. The script
#    copies the shader tree, breaks one body, and asserts the generator exits 3 — the check R3 saw red by hand.
add_test(NAME layer_gen_rejects_bad_body COMMAND ${CMAKE_COMMAND}
    "-DGEN=$<TARGET_FILE:pfg_layer_gen>" "-DSHADERS=${_sh}" "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/test_layergen"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/layers/negative_body.cmake")

add_custom_target(pfg_layer_glsl DEPENDS ${_layer_gen_out})
add_dependencies(phyriad_fg pfg_layer_glsl)

# ── R3: shaders/fg_core.comp (the LAYERTAB kernel, stage 5) + shaders/fg_ab_diff.comp (the M4 byte-diff
#    instrument). fg_core #includes the generated GLSL and the row bodies, so its .spv carries a DEPFILE
#    (glslc -MD): a body or .def edit rebuilds the kernel — risk XR13's discipline, now on the shader side.
#    -I order: the generated dir first, then shaders/ (fg_core_math / fg_helpers / layers/<name>).
set(_fgc_src "${_sh}/fg_core.comp")
set(_fgc_spv "${_gen}/fg_core.spv")
if(PFG_NOCONTRACT)
    set(_fgc_nc_cmd COMMAND "${PFG_PYTHON}" "${_nocontract}" "${_fgc_spv}" "${_fgc_spv}")
    set(_fgc_nc_dep "${_nocontract}")
    set(_fgc_nc_tag " +NoContraction")
else()
    set(_fgc_nc_cmd)
    set(_fgc_nc_dep)
    set(_fgc_nc_tag "")
endif()
add_custom_command(OUTPUT "${_fgc_spv}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${_gen}"
    COMMAND "${PFG_GLSLC}" --target-env=vulkan1.2 -O -I "${_gen}/shaders" -I "${_sh}"
            -MD -MF "${_fgc_spv}.d" -MT "${_fgc_spv}" "${_fgc_src}" -o "${_fgc_spv}"
    ${_fgc_nc_cmd}
    DEPENDS "${_fgc_src}" ${_layer_gen_out} "${_sh}/fg_core_math.glsl" "${_sh}/fg_helpers.glsl" ${_layer_bodies} ${_fgc_nc_dep}
    DEPFILE "${_fgc_spv}.d"
    COMMENT "glslc fg_core (+depfile)${_fgc_nc_tag}" VERBATIM)
add_custom_command(OUTPUT "${_gen}/fg_core_spv.hpp"
    COMMAND ${CMAKE_COMMAND} -D "INPUT=${_fgc_spv}" -D "OUTPUT=${_gen}/fg_core_spv.hpp"
            -D "VARNAME=kFgCoreSpv" -P "${_spv2hdr}"
    DEPENDS "${_fgc_spv}" "${_spv2hdr}" COMMENT "spv->hdr: kFgCoreSpv" VERBATIM)
pfg_spv("${_sh}/fg_ab_diff.comp" "${_gen}/fg_ab_diff_spv.hpp" "kFgAbDiffSpv")
pfg_spv("${_sh}/tdr_hang.comp"   "${_gen}/tdr_hang_spv.hpp"   "kTdrHangSpv")     # R4: --tdr-test (G-R4)
add_custom_target(pfg_shaders_r3 DEPENDS "${_gen}/fg_core_spv.hpp" "${_gen}/fg_ab_diff_spv.hpp" "${_gen}/tdr_hang_spv.hpp")
add_dependencies(phyriad_fg pfg_shaders_r3)

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
