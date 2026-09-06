#pragma once
// PhyriadFG — src/flow/holons.hpp : the holon family's LEAVES as functions over the F thread's scratch (R5 step 3a).
//
// object_repair (the object-holon: connected-component cluster + 16-slot temporal identity + contour shape-field
// motion-inheritance repair + the HUD shield), mem_advect / mem_merge / mem_refresh (the scene-holon: the persistent
// CUR-anchored silhouette prior — advected, merged into the fresh masks, refreshed with wake evaporation). They were
// lambdas inside run_flow (flow.cpp:885–1380 pre-extraction); their bodies are MOVED VERBATIM into holons.cpp, their
// captured scratch is this struct, their captured cfg / grid size are parameters. run_flow owns one HolonScratch and
// keeps the former names as aliases; the lambdas survive as thin wrappers with the SAME parameter lists, so
// consume_wap (step 3c) calls exactly what it called. The registry's FLOW rows objects / objects_bwd / mem_fwd /
// mem_bwd / mem_refresh (layer_table.def) are these functions; step 3b binds their arms.
//
// The scratch (all sized ONCE by run_flow to the full mvw_f×mvh_f grid — zero per-pair heap; the comments on the
// alias lines in run_flow carry each field's rationale):
//   obj_label / obj_bfs / obj_clusters / obj_used / obj_rowmin / obj_rowmax — the cluster walk + top-K selection +
//     per-row fill bounds; obj_chamf / obj_feat — the contour shape-field (chamfer + feature transforms);
//   obj_slots_fwd / obj_slots_bwd — the temporal identity tables, one per anchor, PERSISTING across pairs;
//   mem_prior / mem_adv — the scene-holon's belief (persisting) and its per-pair advected copy;
//   wake_rec / wake_n — the ARMED bwd clusters of this pair, recorded by the bwd repair for mem_refresh.
// Made with my soul - Swately <3
#include <cstdint>
#include <cstddef>
#include <vector>
struct Config;   // cli/cli.hpp

namespace pfg::flow {

struct ObjCluster{ uint32_t mass; double cx,cy; double sx,sy; int minx,maxx,miny,maxy; double mvx,mvy; };
struct ObjSlot{ int active; double cx,cy; double mvx,mvy; uint32_t mass; int age; int miss; };
struct WakeRec{ int32_t cid; int minx,maxx,miny,maxy; double mvx,mvy; };

struct HolonScratch {
    size_t obj_nblk = 0;
    std::vector<int32_t>  obj_label;
    std::vector<uint32_t> obj_bfs;
    std::vector<ObjCluster> obj_clusters;
    std::vector<uint8_t>    obj_used;
    std::vector<int32_t> obj_rowmin, obj_rowmax;
    std::vector<int32_t>  obj_chamf;
    std::vector<uint32_t> obj_feat;
    std::vector<ObjSlot> obj_slots_fwd, obj_slots_bwd;
    std::vector<uint8_t> mem_prior;
    std::vector<uint8_t> mem_adv;
    std::vector<WakeRec> wake_rec;
    int wake_n = 0;
};

void object_repair(pfg::flow::HolonScratch& S, const Config& cfg, uint32_t mvw_f, uint32_t mvh_f, void* mv_field, uint8_t* dis_mask, const float model6[6], std::vector<ObjSlot>& slots, const uint8_t* persist_p, uint8_t* persist_mut, uint64_t span_local, double adv_sign, uint32_t* out_live, uint32_t* out_rep, uint32_t* out_infill, WakeRec* wake_out, int* wake_n_out);
void mem_advect(pfg::flow::HolonScratch& S, uint32_t mvw_f, uint32_t mvh_f, const void* fwd_field);
void mem_merge(const Config& cfg, uint32_t mvw_f, uint32_t mvh_f, uint8_t* dis_mask, const void* anchor_field, const uint8_t* prior_src);
void mem_refresh(pfg::flow::HolonScratch& S, uint32_t mvw_f, uint32_t mvh_f, const uint8_t* dis_b_post, bool bwd_ran, const WakeRec* wake_recs, int wake_count);

}  // namespace pfg::flow
