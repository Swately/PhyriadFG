#pragma once
// PhyriadFG - FgContext: the shared cross-thread state of main()'s FG pipeline, expressed as a
// struct of REFERENCES (and decayed array pointers) to main()'s locals. main() binds every reference
// at the ctx{...} aggregate-init; each thread aliases them back (auto& x = ctx.x).
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <cstdint>
#include <vulkan/vulkan.h>
#include <d3d11.h>                                  // ID3D11Texture2D (dxgi_stage)
#include "cli/cli.hpp"                              // Config
#include "core/vk_util.hpp"                         // Img, HBuf (+ now_ms/img_barrier/full_bic/submit_wait used by the body)
#include "core/device.hpp"                          // VDev
#include "core/globals.hpp"                         // g_quit (true global referenced by the body)
#include "capture/capture.hpp"                      // D3D, ConvPackPipe
#include "warp_blend/warp_blend.hpp"                // FieldPipe
#include "capture/wgc_ctx.hpp"                      // WgcCtx (MSVC/WGC)
#include <phyriad/topology/HardwareTopology.hpp>    // phyriad::hw (MMCSS/pin - used by the body)
#include <phyriad/render/vulkan/OpticalFlowPipeline.hpp>  // ofp/ofpA (the F-thread flow pipeline)

// The FLOW thread's pipeline structs (NvofaProvider/GmePipe/MvSmoothPipe) are DEFINED in flow/flow.hpp,
// which itself #includes THIS header — so they are forward-declared here (FgContext holds only
// references to them) to break the include cycle. main() and run_flow see the full definitions via
// flow/flow.hpp; capture.cpp never names them, so a forward declaration suffices.
struct NvofaProvider; struct GmePipe; struct MvSmoothPipe; struct MedianPipe;
// The PRESENT thread's upscale pipeline struct (UpPipe) is DEFINED in present/present.hpp, which
// #includes THIS header BEFORE the struct — so it is forward-declared here (FgContext holds only a
// reference). main()/run_present see the full definition via present/present.hpp.
struct UpPipe;

// The per-real-capture slot record (named by run_capture).
struct RealSlot { double t_cap_ms=0.0; };

// --ingest-async (default OFF): the RAW host-buffer ring between the acquire thread (run_capture's
// DDA acquire-only loop) and the convert worker (run_convert_worker). RAW_N clones of the Astage
// HBuf; the acquire writes the newest into raw[w%kRawSlots], the worker DROP-TO-NEWEST converts it.
// 4 = one slot of overwrite margin beyond the at-most-1 slot the drop-to-newest worker holds
// in-flight. The raw_busy guard is BEST-EFFORT by timing (memcpy ~1-2ms >> the sub-microsecond
// latch window + the 3-slot margin), NOT hard mutual-exclusion: worst case = a self-correcting
// torn frame, never a crash.
static constexpr int kRawSlots = 4;

struct FgContext {
    // -- CAPTURE thread (run_capture) -- references to main()'s locals; array members decay to pointers.
    Config& cfg;
    uint32_t& ra_core_c;
    std::atomic<uint64_t>& c_seq;
    int& cap_slots;
    std::atomic<bool>& g_quit_threads;   // a main() local (despite the g_ name): the clean-exit latch
#ifdef _MSC_VER
    WgcCtx*& wgc_ctx;
#endif
    D3D& d;
    std::atomic<uint64_t>& stat_mapfb;
    std::atomic<uint64_t>& stat_mapmiss;
    const uint32_t& NAT_W;
    const uint32_t& NAT_H;
    uint32_t& nat_bpp;
    HBuf& Astage;
    std::atomic<uint64_t>& lt_copy_us;
    std::atomic<uint64_t>& lt_compose_us;
    std::atomic<uint64_t>& dd_timeouts;
    ID3D11Texture2D*& dxgi_stage;
    std::atomic<uint64_t>& dd_arr_delta_us;
    std::atomic<uint64_t>& dd_arrived;
    std::atomic<uint64_t>& dd_lost;   // DDA re-arm events (ACCESS_LOST recoveries)
    std::atomic<uint64_t>& dd_present;   // suma de AccumulatedFrames = tasa de presents ENTREGADA (captura real)
    RealSlot* c_slots;
    std::atomic<uint64_t>& total_real;
    bool& use_igpu_convert;
    VkCommandBuffer& cmdA;
    Img& Anative;
    Img& Awork;
    VkPipeline& cvPipe;
    VkPipelineLayout& cvLayout;
    VkDescriptorSet& cvSet;
    const bool& IS_HDR;
    const uint32_t& WW;
    const uint32_t& WH;
    HBuf* hR_a;
    VDev& A;
    const bool& single_gpu;
    std::mutex& a_q2_mtx;
    VkFence& fA;
    std::atomic<uint64_t>& c_conv_us;
    VkCommandBuffer& cmdG;
    ConvPackPipe* cpPipe;
    FieldPipe* fpipe;
    VDev& G;
    VkFence& fG;
    std::mutex& g_q_mtx;
    void** hostFIELD;
    void** hostR;
    std::condition_variable& c_cv;
    // --ingest-async (default OFF): the acquire↔convert-worker decoupling state. ALL of these are inert
    // when cfg.ingest_async is false (the worker is never spawned, the async acquire branch is never
    // entered, the raw ring is never allocated → these references/pointers exist but are never touched).
    std::atomic<uint64_t>& raw_seq;      // monotone "newest published raw frame index + 1"; worker reads (raw_seq-1)%kRawSlots
    std::atomic<uint64_t>& dd_acq;       // TELEMETRY: successful ACQUIREs (serial + async)
    std::atomic<uint64_t>& dd_uniq;      // CAPTURE-DEDUP: frames ÚNICOS reales (no-duplicados de contenido); el `uniq=` readout = la tasa real del juego
    std::atomic<int>&      raw_busy;     // raw slot the worker is mid-converting (-1 = none); the acquire AVOIDS overwriting it (BEST-EFFORT torn-read guard: worst case = a self-correcting torn frame, not a crash)
    std::condition_variable& raw_cv;     // acquire→worker wakeup (also woken on quit)
    std::mutex&            raw_mtx;      // guards the raw_cv wait predicate
    HBuf*                  raw_astage_a; // kRawSlots A-imports (TRANSFER_SRC) — the A-path convert src
    HBuf*                  raw_astage_g; // kRawSlots G-imports (STORAGE)     — the iGPU-path convert src (only when use_igpu_convert)
    double*                raw_tcap;     // kRawSlots capture timestamps (ms) — carried to c_slots[s].t_cap_ms at publish (freshage parity)
    ID3D11Texture2D*&      dxgi_stage2;  // the 2nd DDA staging texture (readback double-buffer; null when async off)

    // -- FLOW thread (run_flow) -- references to main()'s locals; array members decay to pointers (the
    //    C-thread convention above). Locals SHARED with capture (cfg, c_seq, cap_slots, c_slots, c_cv,
    //    g_quit_threads, use_igpu_convert, WW, WH, A, single_gpu, a_q2_mtx) are reused from the block
    //    above and NOT re-listed. run_flow aliases each of these back (auto& x = ctx.x).
    VDev& B;
    VDev& FD;
    bool& pfg_enabled;
    const uint32_t& mvw_f;
    const uint32_t& mvh_f;
    const uint32_t& WW_flow;
    const uint32_t& WH_flow;
    uint32_t& flow_div;
    const uint32_t& ra_core_f;
    bool& use_inertia;
    bool& use_objects;
    bool& use_memory;
    bool& use_bidir;
    bool& use_gme;
    bool& use_gme_gpu;
    bool& use_wap;
    bool& use_nvofa;
    bool& use_ambig;
    bool& use_mv_smooth;
    bool& use_fwd_prestage;
    bool& b_q2_split;
    phyriad::render::vulkan::OpticalFlowPipeline& ofp;
    phyriad::render::vulkan::OpticalFlowPipeline& ofpA;
    Img& Cinterp;
    Img& CinterpA;
    NvofaProvider& nvofa;
    GmePipe& gmePipe;
    MvSmoothPipe& mvsm;
    Img* Bframe;
    Img* Bflow;
    Img* AframeA;
    VkCommandBuffer& cmdB;
    VkCommandBuffer& cmdB_bwd;
    VkCommandBuffer* cmdB_fwd;
    VkCommandBuffer* cmdF_pre;
    VkCommandBuffer* cmdB2;
    VkCommandBuffer& cmdA_fg;
    VkFence& fB;
    VkFence& fB2;
    VkFence* fB_fwd;
    VkFence* fF_pre;
    VkFence& fA_fg;
    VkFence* tfb;
    void** hostMV;
    void** hostSAD;
    void** hostPER;
    void** hostDIS;
    void** hostDISB;
    void** hostMVB;
    void** hostGmeM;
    void** hostGmeMB;
    void** hostC2;
    void* (*hostI)[kMaxInterp];
    HBuf* hR_b;
    HBuf* hRP_b;
    HBuf* hRP_b_dev;
    HBuf* hMV_b;
    HBuf* hSAD_b;
    HBuf* hMVB_b;
    HBuf* hDIS_b;
    HBuf* hDISB_b;
    HBuf* hGmeM_b;
    HBuf* hGmeMB_b;
    HBuf* hC2_b;
    HBuf* sbI;
    HBuf (*hI_b)[kMaxInterp];
    HBuf (*hI_a)[kMaxInterp];
    UnpackPipe* ubPipe;
    uint64_t* f_pair_cseq_a;
    int* f_pair_slot_a;
    double* f_pair_tcap_a;
    uint64_t* f_pair_span_a;
    int* f_pair_n_a;
    float (*f_pair_gme_a)[6];
    int* f_pair_gme_valid_a;
    float (*f_pair_gme_bwd_a)[6];
    float* f_pair_mfwd_a;
    float* f_pair_mbwd_a;
    float* f_pair_disp_a;
    int* f_pair_bwd_valid_a;
    std::atomic<uint64_t>& f_seq;
    std::condition_variable& f_cv;
    std::mutex& c_mtx;
    std::atomic<uint64_t>& p_presenting;
    std::atomic<uint64_t>& src_interval_us;
    std::atomic<uint64_t>& present_cost_us;
    std::atomic<uint64_t>& stat_tier;
    std::atomic<uint64_t>& stat_bwd_skips;
    std::atomic<uint64_t>& stat_skips;
    std::atomic<uint64_t>& stat_lap;
    std::atomic<uint64_t>& stat_cons;
    std::atomic<uint64_t>& stat_flow_us;
    std::atomic<uint64_t>& stat_pair_us;
    std::atomic<uint64_t>& gme_fit_us;
    std::atomic<uint64_t>& gme_dis_x100;
    std::atomic<uint64_t>& obj_live;
    std::atomic<uint64_t>& obj_rep_x10;
    std::atomic<int>& live_n_atomic;
    std::atomic<uint64_t>& lt_pickup_us;
    std::atomic<uint64_t>& lt_preflow_us;
    std::atomic<uint64_t>& lt_spin_us;
    std::atomic<uint64_t>& lt_fpub_us;

    // -- PRESENT thread (run_present) -- references to main()'s locals; array members decay to
    //    pointers (the C/F convention above). Locals SHARED with capture/flow (cfg, the rings,
    //    c_seq/cap_slots/c_slots, the seq atomics f_seq/p_presenting, devices A/G, the F->P publish
    //    arrays, WW/WH, the stat_* atomics, ...) are reused from the blocks above and NOT re-listed.
    //    run_present aliases each of these back (auto& x = ctx.x).
    Img& Apresent;
    Img& Gdst;
    Img& Gsrc;
    const uint32_t& UP_H;
    const uint32_t& UP_W;
    const uint32_t& WH_warp;
    const uint32_t& WW_warp;
    uint32_t& bridge_h;
    Img& bridge_img1;
    IDXGIKeyedMutex*& bridge_km_d3d;
    IDXGIKeyedMutex*& bridge_km_d3d1;
    VkDeviceMemory& bridge_mem1;
    HANDLE& bridge_nt;
    HANDLE& bridge_nt1;
    ID3D11Texture2D*& bridge_tex;
    ID3D11Texture2D*& bridge_tex1;
    bool& bridge_use_km;
    uint32_t& bridge_w;
    VkCommandBuffer& cmdBridge1;
    VkCommandBuffer& cmdBridgeA0;
    VkCommandBuffer& cmdGP;
    VkCommandBuffer* cmdUpload;
    HBuf& devMass;
    VkFence& fBridge1;
    VkFence& fBridgeA0;
    VkFence& fGP;
    FillPipe& fillPipeA;
    HBuf* hC2_a;
    HBuf& hCurD_a;
    HBuf* hDISB_a;
    HBuf* hDIS_a;
    HBuf* hFIELD_a;
    HBuf& hGout;
    HBuf* hMVB_a;
    HBuf* hMV_a;
    HBuf& hMass_a;
    HBuf& hOutD_a;
    HBuf* hPER_a;
    HBuf& hPrevD_a;
    HBuf* hR_g;
    HBuf* hSAD_a;
    void*& hostCurD;
    void*& hostMassPtr;
    void*& hostOutD;
    void*& hostPrevD;
    LUID& luidA;
    bool& luidA_ok;
    LUID& luidB;
    bool& luidB_ok;
    LUID& luidG;
    bool& luidG_ok;
    MedianPipe& medPipe;
    std::string& nvNameA;
    std::string& nvNameB;
    uint32_t& pres_h;
    uint32_t& pres_w;
    const uint32_t& ra_core_p;
    std::atomic<uint64_t>& total_frames;
    UnpackPipe* ugPipe;
    UpPipe& upPipe;
    bool& use_commit_default;
    bool& use_fill_div;
    bool& use_matte;
    bool& use_mv_guided;
    bool& use_mv_median;
    bool& use_onepos;
    bool& use_rescue;
    bool& use_upscale;
    int& uslot;
    uint64_t* uslot_val;
    Img& wapC2A;
    Img& wapCurA;
    Img& wapDISA;
    Img& wapDISBA;
    Img& wapFIELDA;
    Img& wapMVA;
    Img& wapMVBA;
    Img& wapMVScratchA;
    Img& wapMVTA;
    Img& wapOutA;
    Img& wapPERA;
    WapPipe& wapPipeA;
    Img& wapPrevA;
    Img& wapPrevOutA;
    Img& wapSADA;
    uint32_t& warp_div;
    HWND& wgc_target_hwnd;
    uint64_t& xfer_U;
    uint64_t& xfer_W;
    bool& xfer_on;
    // slot-0 present bridge + the fg-protect demote flag: these names are ALSO re-declared (shadowed)
    // inside nested lambdas of the P body, so they need the OUTER-scope alias (run_present's nested
    // lambdas shadow them locally).
    Img& bridge_img;
    VkDeviceMemory& bridge_mem;
    VkCommandBuffer& cmdBridge;
    VkFence& fBridge;
    bool& ra_fgprotect_demote_p;
};
