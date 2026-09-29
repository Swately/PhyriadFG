// PhyriadFG — src/instrument/gdump.cpp : GdumpTap bodies — the Vulkan glue, the writer thread, the
// files (GDUMP_PLAN.md §1 mechanism, §2 the record, §4 RISK_REGISTER). GdumpBook (the pure-CPU
// bookkeeping this file drives) is implemented in gdump_book.cpp; this file touches it only through
// the public interface in gdump.hpp (S6: the two classes stay independently testable). Every method
// declared inline in the header (begin_tick, pair_pending, semaphore, next_signal, skip, tick, armed,
// book) already has its body there — nothing below re-defines them.
//
// Off = byte-identical (S7): every constructor failure path releases exactly what it allocated so far
// and returns with armed_ still false — a diagnostic never kills WAP (§1.6).
#include "instrument/gdump.hpp"
#include "core/globals.hpp"          // vk_wait_sem_live / g_quit / g_device_lost (CR3: the bounded, device-loss-aware wait)
#include <phyriad/hal/MemoryOrder.hpp>   // phyriad::hal::spin_hint — the backoff's PAUSE/YIELD rung
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <chrono>
#include <malloc.h>                  // _aligned_malloc / _aligned_free (the host-buffer discipline, warp_blend_init.cpp:238-266)

namespace pfg::instrument {

namespace {
// The writer's idle backoff (§1.4/CR3): the transport RingWaitBackoff shape, inlined because
// framework/transport/ is not vendored into this tree (S1's own note in the plan). 200 spins of
// nothing (the ring usually isn't empty long), then ~200 of spin_hint (PAUSE/YIELD, no syscall), then
// ~20 of a real yield, then 1 ms sleeps once the ring has been idle a while — never a busy spin that
// burns a core while `--gdump` sits behind a slow producer.
inline void backoff(uint32_t spins) noexcept {
    if (spins < 200u) return;
    if (spins < 400u) { phyriad::hal::spin_hint(); return; }
    if (spins < 420u) { std::this_thread::yield(); return; }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
}
// RR5: the STAGING allocation is rounded up to host_align (hfb import needs it), but the RECORD writes
// the logical size — never the rounded one. This is the rounding side only; every write below uses
// dims_ directly, never a buffer's allocated size.
inline uint64_t round_up_align(uint64_t n, uint64_t align) noexcept {
    return (align > 1u) ? (n + align - 1u) / align * align : n;
}
// Host visibility (P2-8; the --outdump precedent present.cpp:1258-1261): a transfer write into a host-imported
// buffer must be made AVAILABLE to the host (TRANSFER_WRITE -> HOST_READ) before the signal the writer waits
// on. HOST_COHERENT memory needs no invalidate, but the availability operation is still the spec's requirement.
// PR2 (measured 2026-09-09): a buffered stream of 0.9 MB frames at 240/s stalled the writer for 0.26–0.73 s at a time
// once the file grew past ~3 GB — the OS write cache flushing, not the disk (1.3–1.9 GB/s unbuffered, measured).
// The big planes (the generated frame, the pair reals) therefore go through FILE_FLAG_NO_BUFFERING when the spec's
// two conditions hold — the byte count is a sector multiple and the source pointer is sector-aligned (the staging
// buffers are _aligned_malloc'ed to host_align, 4096 on this device) — and fall back to stdio otherwise. The small
// planes, push.bin and the text indexes stay on stdio (a few KB per pair, sub-sector sizes).
constexpr uint64_t kSector = 4096u;
inline bool unbuffered_ok(const void* p, uint64_t n) noexcept {
    return (n % kSector) == 0u && n > 0u && ((uintptr_t)p % kSector) == 0u;
}
inline HANDLE open_raw(const char* path, bool append_stream) noexcept {
    const DWORD flags = FILE_ATTRIBUTE_NORMAL | FILE_FLAG_NO_BUFFERING | FILE_FLAG_SEQUENTIAL_SCAN | (append_stream ? 0u : FILE_FLAG_WRITE_THROUGH);
    return CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, flags, nullptr);
}
inline bool write_raw(HANDLE h, const void* p, uint64_t n) noexcept {
    DWORD got = 0;
    return WriteFile(h, p, (DWORD)n, &got, nullptr) && got == (DWORD)n;
}
inline void host_read_barrier(VkCommandBuffer c, VkBuffer buf) noexcept {
    VkBufferMemoryBarrier bb{}; bb.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    bb.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; bb.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    bb.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; bb.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bb.buffer = buf; bb.offset = 0; bb.size = VK_WHOLE_SIZE;
    vkCmdPipelineBarrier(c, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 0, nullptr, 1, &bb, 0, nullptr);
}
} // namespace

// ── Construction (§1.1, §1.4, CR1, CR3, PR2) ────────────────────────────────────────────────────────
GdumpTap::GdumpTap(VDev& A, const char* dir, uint32_t ring_n, uint32_t pair_sets, GdumpDims dims, GdumpInfo info)
    : A_(A), book_(ring_n, pair_sets), dims_(dims), info_(info), dir_(dir ? dir : "")
{
    // S7 byte-identical-off: the flag is OFF (no dir) — return now, silently, before any check that could print.
    // The object always exists in run_present; unarmed, every method is an inline no-op.
    if (dir_.empty()) return;
    // CR1: the staging import needs external host memory; the writer's wait needs the timeline
    // feature. Neither exists yet without it — disarm now, before touching the filesystem.
    if (!A_.has_emh) {
        std::printf("[ra] gdump: device has no external host memory -- DISARMED\n");
        return;
    }
    if (!A_.has_timeline) {
        std::printf("[ra] gdump: timeline semaphores unavailable -- DISARMED\n");
        return;
    }

    // §1.1(2): the directory tree. Both calls are idempotent — an ERROR_ALREADY_EXISTS is not checked,
    // exactly like the qdump precedent (warp_blend_init.cpp:259/287's CreateDirectoryA).
    CreateDirectoryA(dir_.c_str(), nullptr);
    const std::string pairs_dir = dir_ + "\\pairs";
    CreateDirectoryA(pairs_dir.c_str(), nullptr);

    // §1.1(3)/RR1: GdumpBook rounds ring_n DOWN to a power of two (kRingMax-clamped) in its own
    // constructor above — book_.ring_n() is the value actually in force, never the raw parameter.
    const uint32_t N = book_.ring_n();
    const uint32_t M = book_.pair_sets();
    const uint64_t align = A_.host_align ? A_.host_align : 1u;

    // Shared failure-path teardown: every early return below (files not yet opened, so nothing there
    // to release) calls this with whatever slots_/sets_/sem_ were built so far. The destructor has its
    // own copy of the buffer-freeing halves (gdump.hpp declares no shared private helper to call).
    auto release_all = [&]() {
        for (auto& s : slots_) { if (s.buf.buf) hbuf_destroy(A_, s.buf); if (s.ptr) _aligned_free(s.ptr); }
        slots_.clear();
        for (auto& s : sets_) {
            if (s.bprev.buf) hbuf_destroy(A_, s.bprev); if (s.prev) _aligned_free(s.prev);
            if (s.bcur.buf)  hbuf_destroy(A_, s.bcur);  if (s.cur)  _aligned_free(s.cur);
            if (s.bmv1.buf)  hbuf_destroy(A_, s.bmv1);  if (s.mv1)  _aligned_free(s.mv1);
            if (s.bmvb1.buf) hbuf_destroy(A_, s.bmvb1); if (s.mvb1) _aligned_free(s.mvb1);
        }
        sets_.clear();
        if (sem_) { vkDestroySemaphore(A_.dev, sem_, nullptr); sem_ = VK_NULL_HANDLE; }
        if (live_raw_ && live_raw_ != INVALID_HANDLE_VALUE) { CloseHandle(live_raw_); }
        live_raw_ = INVALID_HANDLE_VALUE;
        if (f_live_)  { std::fclose(f_live_);  f_live_  = nullptr; }
        if (f_push_)  { std::fclose(f_push_);  f_push_  = nullptr; }
        if (f_ticks_) { std::fclose(f_ticks_); f_ticks_ = nullptr; }
        if (f_pairs_) { std::fclose(f_pairs_); f_pairs_ = nullptr; }
        if (f_xref_)  { std::fclose(f_xref_);  f_xref_  = nullptr; }
    };

    // §1.1(3): N frame staging slots — _aligned_malloc + hbuf_import, the warp_blend_init.cpp:238-266
    // host-buffer discipline. The buffer only needs TRANSFER_DST (the tick's cmd buffer copies INTO it,
    // §1.2); the writer thread reads it back on the CPU side of the same mapping (hbuf_import's
    // out.mapped == ptr, vk_util.cpp:53), never through Vulkan again.
    const uint64_t frame_bytes = round_up_align((uint64_t)dims_.WW_warp * dims_.WH_warp * 4u, align);
    slots_.resize(N);
    for (uint32_t i = 0; i < N; ++i) {
        Slot& s = slots_[i];
        if (!info_.live) { s.push.resize(dims_.push_bytes); continue; }   // --gdump-live 0: no frame staging (no live copy, no live write)
        s.ptr = _aligned_malloc((size_t)frame_bytes, (size_t)align);
        if (!s.ptr || !hbuf_import(A_, s.ptr, frame_bytes, s.buf, VK_BUFFER_USAGE_TRANSFER_DST_BIT)) {
            std::printf("[ra] gdump: staging slot %u/%u alloc/import failed (%.1f MB) -- DISARMED\n",
                        i, N, frame_bytes / 1048576.0);
            release_all(); return;
        }
        s.push.resize(dims_.push_bytes);
    }

    // §1.1(4): M pair sets. prev/cur are the WW×WH pair-real anchors; mv1 is the post-consensus MV the
    // warp sampled; mvb1 mirrors it ONLY under info.bidir (the mvb1 host-visible import is skipped
    // entirely off-bidir — no cost for a config that never fills it, S7's spirit extended to this
    // sub-allocation). The eight HOST planes (sad/c2/dis/disb/per/mvt/mv/mvb) are sized unconditionally
    // per §1.3's list; on_pair_upload leaves the ones this config never fills at their has_*=false
    // default — a null source memcpy's nothing (RR3).
    const uint64_t real_bytes = round_up_align((uint64_t)dims_.WW * dims_.WH * 4u, align);
    const uint64_t mv_bytes   = round_up_align((uint64_t)dims_.mvw * dims_.mvh * 4u, align);
    sets_.resize(M);
    for (uint32_t i = 0; i < M; ++i) {
        PairSet& s = sets_[i];
        s.prev = _aligned_malloc((size_t)real_bytes, (size_t)align);
        s.cur  = _aligned_malloc((size_t)real_bytes, (size_t)align);
        s.mv1  = _aligned_malloc((size_t)mv_bytes,   (size_t)align);
        bool ok = s.prev && s.cur && s.mv1
            && hbuf_import(A_, s.prev, real_bytes, s.bprev, VK_BUFFER_USAGE_TRANSFER_DST_BIT)
            && hbuf_import(A_, s.cur,  real_bytes, s.bcur,  VK_BUFFER_USAGE_TRANSFER_DST_BIT)
            && hbuf_import(A_, s.mv1,  mv_bytes,   s.bmv1,  VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        if (ok && info_.bidir) {
            s.mvb1 = _aligned_malloc((size_t)mv_bytes, (size_t)align);
            ok = s.mvb1 && hbuf_import(A_, s.mvb1, mv_bytes, s.bmvb1, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        }
        if (!ok) {
            std::printf("[ra] gdump: pair set %u/%u alloc/import failed -- DISARMED\n", i, M);
            release_all(); return;
        }
        const size_t plane  = (size_t)dims_.mvw * dims_.mvh;
        s.sad.resize(plane * 4u); s.c2.resize(plane * 8u);
        s.dis.resize(plane); s.disb.resize(plane); s.per.resize(plane);
        s.mvt.resize(plane * 4u); s.mv.resize(plane * 4u); s.mvb.resize(plane * 4u);
    }

    // §1.1(5): the second timeline semaphore (semCapTL) the writer waits on — device.cpp:238-240's
    // TIMELINE-semaphore pattern, initialValue 0 (cap_seq_ also starts at 0; the first signal is 1).
    {
        VkSemaphoreTypeCreateInfo stci{}; stci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
        stci.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE; stci.initialValue = 0;
        VkSemaphoreCreateInfo sci{}; sci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO; sci.pNext = &stci;
        if (vkCreateSemaphore(A_.dev, &sci, nullptr, &sem_) != VK_SUCCESS) {
            std::printf("[ra] gdump: vkCreateSemaphore(TIMELINE) failed -- DISARMED\n");
            release_all(); return;
        }
    }

    // §1.1(6)/§2: the five persistent streams. live.rgba/push.bin get a 4 MB buffer (setvbuf(NULL,...)
    // lets the CRT own the storage — no lifetime to track) since the writer appends to them every
    // captured tick; ticks.tsv/pairs.tsv/qdump_xref.tsv stay default-buffered (line-rate, not frame-rate).
    // live.rgba: unbuffered when every frame is a sector multiple from a sector-aligned slot (PR2); the fallback
    // is the stdio stream. The choice is made ONCE here from the first slot (all slots share the alignment).
    live_raw_ = INVALID_HANDLE_VALUE;
    if (info_.live && N > 0 && unbuffered_ok(slots_[0].ptr, (uint64_t)dims_.WW_warp * dims_.WH_warp * 4u)) {
        live_raw_ = open_raw((dir_ + "\\live.rgba").c_str(), /*append_stream=*/true);
        if (live_raw_ == INVALID_HANDLE_VALUE) std::printf("[ra] gdump: live.rgba unbuffered open failed (%lu) -- falling back to stdio\n", (unsigned long)GetLastError());
    }
    f_live_  = (info_.live && live_raw_ == INVALID_HANDLE_VALUE) ? std::fopen((dir_ + "\\live.rgba").c_str(), "wb") : nullptr;
    f_push_  = std::fopen((dir_ + "\\push.bin").c_str(),        "wb");
    f_ticks_ = std::fopen((dir_ + "\\ticks.tsv").c_str(),       "wb");
    f_pairs_ = std::fopen((dir_ + "\\pairs.tsv").c_str(),       "wb");
    f_xref_  = std::fopen((dir_ + "\\qdump_xref.tsv").c_str(),  "wb");
    if ((info_.live && !f_live_ && live_raw_ == INVALID_HANDLE_VALUE) || !f_push_ || !f_ticks_ || !f_pairs_ || !f_xref_) {
        std::printf("[ra] gdump: failed to open live.rgba/push.bin/ticks.tsv/pairs.tsv/qdump_xref.tsv under '%s' -- DISARMED\n",
                    dir_.c_str());
        if (live_raw_ != INVALID_HANDLE_VALUE) { CloseHandle(live_raw_); live_raw_ = INVALID_HANDLE_VALUE; }
        release_all(); return;
    }
    if (f_live_) std::setvbuf(f_live_, nullptr, _IOFBF, 4u * 1024u * 1024u);
    std::setvbuf(f_push_, nullptr, _IOFBF, 4u * 1024u * 1024u);

    // gdump.hdr — §2's exact token set, one line each, written once and closed (not one of the five
    // persistent streams above).
    {
        FILE* h = std::fopen((dir_ + "\\gdump.hdr").c_str(), "wb");
        if (!h) {
            std::printf("[ra] gdump: failed to open gdump.hdr under '%s' -- DISARMED\n", dir_.c_str());
            release_all(); return;
        }
        std::fprintf(h, "gdump 1\n");
        std::fprintf(h, "size %u %u\n", dims_.WW, dims_.WH);
        std::fprintf(h, "live_div %u %u %u\n", dims_.warp_div, dims_.WW_warp, dims_.WH_warp);   // ALWAYS written, even D=1 (§2)
        std::fprintf(h, "mv %u %u\n", dims_.mvw, dims_.mvh);
        std::fprintf(h, "push_bytes %u\n", dims_.push_bytes);
        std::fprintf(h, "kernel %s\n", info_.kernel ? info_.kernel : "?");
        std::fprintf(h, "contract 0x%016llX\n", (unsigned long long)info_.contract);
        std::fprintf(h, "bidir %d\n", info_.bidir ? 1 : 0);
        std::fprintf(h, "xfer %d\n", info_.xfer ? 1 : 0);
        std::fprintf(h, "async %d\n", info_.async_present ? 1 : 0);
        std::fprintf(h, "ring %u pairs %u\n", N, M);
        if (!info_.live) std::fprintf(h, "live 0\n");   // --gdump-live 0 (absent = 1, so a default header is unchanged)
        if (info_.eco_anchor > 0) std::fprintf(h, "eco_anchor %d %.9g\n", info_.eco_anchor, (double)info_.eco_hyst);   // absent = off (default header unchanged)
        std::fprintf(h, "qpc_hz %.6f\n", info_.qpc_hz);
        // §1.6: the three refused flags, recorded as 0 BY CONSTRUCTION — resolve_config() (S2) never
        // lets the tap arm with any of them on, so this file has no other truth to record.
        std::fprintf(h, "afill 0\n");
        std::fprintf(h, "fps_overlay 0\n");
        std::fprintf(h, "ts_smooth 0\n");
        std::fclose(h);
    }
    std::fputs("# seq slot tick t gen tgen pair decision flags qpc live_off push_off pairset\n", f_ticks_);
    std::fputs("# pair gen tgen seq_recorded gme_valid g0 g1 g2 g3 g4 g5 prev next mv1 mvb1 sad c2 dis disb per mvt mv mvb\n", f_pairs_);

    // §1.4: the writer thread. Everything above must be live before it starts (it reads slots_/sets_/
    // sem_/f_*_ with no further synchronisation — P never touches them again except through the public
    // on_*/record_*/note_qdump/stop methods).
    writer_ = std::thread(&GdumpTap::writer_main, this);
    armed_ = true;
    if (info_.live)
        std::printf("[ra] gdump: ARMED -> %s (ring %u x %.1f MB, pairs %u, %s)\n",
                    dir_.c_str(), N, frame_bytes / 1048576.0, M, info_.kernel ? info_.kernel : "?");
    else
        std::printf("[ra] gdump: ARMED -> %s (ring %u, LIVE FRAMES OFF (--gdump-live 0): index + pairs only, pairs %u, %s)\n",
                    dir_.c_str(), N, M, info_.kernel ? info_.kernel : "?");
}

GdumpTap::~GdumpTap() {
    // CR3: stop() joins the writer BEFORE anything below is torn down; the caller normally already
    // called stop() with the real total_presents, so this is the safety net for an early-return path
    // (main.cpp exception-free early exit, or an unarmed tap where stop() is a no-op past stopped_=true).
    if (!stopped_) stop(0);
    if (sem_) { vkDestroySemaphore(A_.dev, sem_, nullptr); sem_ = VK_NULL_HANDLE; }
    for (auto& s : slots_) { if (s.buf.buf) hbuf_destroy(A_, s.buf); if (s.ptr) _aligned_free(s.ptr); }
    for (auto& s : sets_) {
        if (s.bprev.buf) hbuf_destroy(A_, s.bprev); if (s.prev) _aligned_free(s.prev);
        if (s.bcur.buf)  hbuf_destroy(A_, s.bcur);  if (s.cur)  _aligned_free(s.cur);
        if (s.bmv1.buf)  hbuf_destroy(A_, s.bmv1);  if (s.mv1)  _aligned_free(s.mv1);
        if (s.bmvb1.buf) hbuf_destroy(A_, s.bmvb1); if (s.mvb1) _aligned_free(s.mvb1);
    }
}

// ── P side (GDUMP_PLAN §3 S5) ───────────────────────────────────────────────────────────────────────
// (b) the frame copy: `out` is already TRANSFER_SRC_OPTIMAL at the call site (inside the blit pass /
// between present.cpp's blit and back-barrier) — no barrier here, the caller guarantees the layout (§1.2).
void GdumpTap::record_frame_copy(VkCommandBuffer c, VkImage out, int slot) noexcept {
    if (!armed_ || slot < 0 || !info_.live) return;   // --gdump-live 0: no live copy (the pair copies are separate)
    VkBufferImageCopy cp = full_bic(dims_.WW_warp, dims_.WH_warp);
    vkCmdCopyImageToBuffer(c, out, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, slots_[(size_t)slot].buf.buf, 1, &cp);
    host_read_barrier(c, slots_[(size_t)slot].buf.buf);
}

// (b) the pair copies: prev/cur (WW×WH) + mv1 (mvw×mvh) unconditionally, mvb1 only when the caller
// passed a real image AND this config is bidir (the mvb1 staging import above is skipped off-bidir, so
// copying into it then would write past nothing — never call bmvb1.buf when it was never created).
// Barrier pairs mirror present.cpp:1418-1437's qdump-oneshot pattern exactly (RO -> TRANSFER_SRC -> RO;
// these images stay SHADER_READ_ONLY_OPTIMAL between warps — CR6).
int GdumpTap::record_pair_copies(VkCommandBuffer c, VkImage prev, VkImage cur, VkImage mv, VkImage mvb) noexcept {
    if (!armed_) return -1;
    const int idx = book_.claim_pending_pair();
    if (idx < 0) return -1;
    PairSet& s = sets_[(size_t)idx];

    img_barrier(c, prev, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_READ_BIT);
    { VkBufferImageCopy cp = full_bic(dims_.WW, dims_.WH); vkCmdCopyImageToBuffer(c, prev, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, s.bprev.buf, 1, &cp); }
    img_barrier(c, prev, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT);

    img_barrier(c, cur, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_READ_BIT);
    { VkBufferImageCopy cp = full_bic(dims_.WW, dims_.WH); vkCmdCopyImageToBuffer(c, cur, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, s.bcur.buf, 1, &cp); }
    img_barrier(c, cur, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT);

    img_barrier(c, mv, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_READ_BIT);
    { VkBufferImageCopy cp = full_bic(dims_.mvw, dims_.mvh); vkCmdCopyImageToBuffer(c, mv, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, s.bmv1.buf, 1, &cp); }
    img_barrier(c, mv, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT);

    s.has_mvb1 = false;
    if (mvb != VK_NULL_HANDLE && info_.bidir) {
        img_barrier(c, mvb, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_READ_BIT);
        { VkBufferImageCopy cp = full_bic(dims_.mvw, dims_.mvh); vkCmdCopyImageToBuffer(c, mvb, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, s.bmvb1.buf, 1, &cp); }
        img_barrier(c, mvb, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT);
        s.has_mvb1 = true;
        host_read_barrier(c, s.bmvb1.buf);
    }
    host_read_barrier(c, s.bprev.buf); host_read_barrier(c, s.bcur.buf); host_read_barrier(c, s.bmv1.buf);
    return idx;
}

// (d) after pres.submit: shadow the push block into this tick's slot (RR3-adjacent — a copy, never a
// deferred pointer) and hand the descriptor to the book.
void GdumpTap::on_submitted(GdumpDesc d, const void* push, uint32_t push_bytes) noexcept {
    if (!armed_) return;
    Slot& s = slots_[d.slot];
    const size_t n = std::min<size_t>(s.push.size(), (size_t)push_bytes);
    if (push && n) std::memcpy(s.push.data(), push, n);
    last_seq_ = d.seq;
    book_.on_submitted(d);
}

// (e) the pair-advance site, right after wap_upload returns: RR3 — memcpy NOW, never deferred, because
// nothing guards F's next write into these same host slots. A null source plane means this config
// never produced it (bidir off, ambig off, …) — has_* stays false and pairs.tsv names it '-' (§2).
void GdumpTap::on_pair_upload(uint64_t pair, int gen, int tgen, const GdumpPairHost& h) noexcept {
    if (!armed_) return;
    const int idx = book_.on_pair_upload(pair, gen, tgen);
    if (idx < 0) return;
    PairSet& s = sets_[(size_t)idx];
    s.pair = pair; s.gen = gen; s.tgen = tgen;
    auto plane = [](const void* src, std::vector<uint8_t>& dst, bool& has) noexcept {
        if (src) { std::memcpy(dst.data(), src, dst.size()); has = true; } else has = false;
    };
    plane(h.sad, s.sad, s.has_sad);   plane(h.c2,  s.c2,  s.has_c2);
    plane(h.dis, s.dis, s.has_dis);   plane(h.disb, s.disb, s.has_disb);
    plane(h.per, s.per, s.has_per);   plane(h.mvt, s.mvt, s.has_mvt);
    plane(h.mv,  s.mv,  s.has_mv);    plane(h.mvb, s.mvb, s.has_mvb);
    s.gme_valid = h.gme_valid;
    if (h.gme6) std::memcpy(s.gme6, h.gme6, sizeof(s.gme6)); else std::memset(s.gme6, 0, sizeof(s.gme6));
}

// (g) --qdump dumped the tick whose descriptor was last pushed (sync path only — P7-6's cross-check key).
void GdumpTap::note_qdump(int qdump_idx) noexcept {
    if (!armed_ || !f_xref_) return;
    std::fprintf(f_xref_, "%llu %d\n", (unsigned long long)last_seq_, qdump_idx);
    std::fflush(f_xref_);
}

// ── The writer thread (§1.4, CR3) ───────────────────────────────────────────────────────────────────
void GdumpTap::writer_main() noexcept {
    uint32_t spins = 0;
    GdumpDesc d{};
    for (;;) {
        if (!book_.peek(d)) {
            // Re-check under stop_ before sleeping into a break: a descriptor pushed between the two
            // peeks must still be drained, not lost at shutdown.
            if (stop_ && !book_.peek(d)) break;
            backoff(spins++);
            continue;
        }
        spins = 0;
        // CR3: bounded, device-loss-aware — never an unbounded vkWaitSemaphores on a lost device.
        VkSemaphoreWaitInfo wi{}; wi.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
        wi.semaphoreCount = 1; wi.pSemaphores = &sem_; wi.pValues = &d.seq;
        if (!vk_wait_sem_live(A_.dev, wi)) {
            ++book_.counters().writer_timeouts;
            if (g_device_lost || stop_) break;
            continue;   // a live-but-slow tick: retry the same descriptor next lap (still at the ring head)
        }

        // RR5: the LOGICAL frame size, never the rounded staging allocation. --gdump-live 0: 0 bytes, nothing written.
        const uint64_t frame_bytes = GdumpBook::live_frame_bytes(info_.live, (uint64_t)dims_.WW_warp * dims_.WH_warp * 4u);
        if (frame_bytes == 0) {
        } else if (live_raw_ != INVALID_HANDLE_VALUE) {
            if (!write_raw(live_raw_, slots_[d.slot].ptr, frame_bytes)) ++book_.counters().writer_timeouts;   // counted under the same "the disk did not take it" bucket
        } else {
            std::fwrite(slots_[d.slot].ptr, 1, (size_t)frame_bytes, f_live_);
        }
        const size_t push_n = slots_[d.slot].push.size();
        if (push_n) std::fwrite(slots_[d.slot].push.data(), 1, push_n, f_push_);
        std::fputs(GdumpBook::index_line(d, live_off_, push_off_).c_str(), f_ticks_);
        live_off_ += frame_bytes;
        push_off_ += push_n;
        book_.counters().bytes += frame_bytes + push_n;

        if (d.flags & kFlagPair) {
            bool written = false;
            if (d.pairset >= 0 && (size_t)d.pairset < sets_.size()) written = write_pair(sets_[(size_t)d.pairset], d.seq);
            book_.release_pair_set(d.pairset, written);   // the set is free again either way; pairs_written counts only the disk
        }
        ++book_.counters().written;
        book_.pop();
    }
}

// The pair files + the pairs.tsv line. A pair with no SAD plane is refused unconditionally
// (ref_warp.py:438 requires it, RR4) — counted nowhere new (the header is fixed) but named once.
bool GdumpTap::write_pair(const PairSet& s, uint64_t seq_recorded) {
    if (!s.has_sad) {
        std::printf("[ra] gdump: pair %llu has no SAD plane -- not written\n", (unsigned long long)s.pair);
        return false;
    }
    const uint64_t real_bytes = (uint64_t)dims_.WW * dims_.WH * 4u;
    const uint64_t mv_bytes   = (uint64_t)dims_.mvw * dims_.mvh * 4u;
    char path[512];
    auto write_file = [&](const char* suffix, const void* data, size_t n) {
        std::snprintf(path, sizeof(path), "%s\\pairs\\p%llu_%s", dir_.c_str(), (unsigned long long)s.pair, suffix);
        if (unbuffered_ok(data, n)) {   // the two pair reals (sector-multiple, sector-aligned staging): bypass the cache (PR2)
            HANDLE h = open_raw(path, /*append_stream=*/false);
            if (h != INVALID_HANDLE_VALUE) { write_raw(h, data, n); CloseHandle(h); return; }
        }
        if (FILE* f = std::fopen(path, "wb")) { std::fwrite(data, 1, n, f); std::fclose(f); }
    };
    write_file("prev.rgba", s.prev, (size_t)real_bytes);
    write_file("next.rgba", s.cur,  (size_t)real_bytes);
    write_file("mv1.rg16f", s.mv1,  (size_t)mv_bytes);
    if (s.has_mvb1) write_file("mvb1.rg16f", s.mvb1, (size_t)mv_bytes);
    if (s.has_sad)  write_file("sad.rg16f",  s.sad.data(),  s.sad.size());
    if (s.has_c2)   write_file("c2.rgba16f", s.c2.data(),   s.c2.size());
    if (s.has_dis)  write_file("dis.r8",     s.dis.data(),  s.dis.size());
    if (s.has_disb) write_file("disb.r8",    s.disb.data(), s.disb.size());
    if (s.has_per)  write_file("per.r8",     s.per.data(),  s.per.size());
    if (s.has_mvt)  write_file("mvt.rg16f",  s.mvt.data(),  s.mvt.size());
    if (s.has_mv)   write_file("mv.rg16f",   s.mv.data(),   s.mv.size());
    if (s.has_mvb)  write_file("mvb.rg16f",  s.mvb.data(),  s.mvb.size());

    // §2's pairs.tsv line: bare file names (the adapter renames, gap 5), '-' for an absent plane.
    char b_prev[64], b_next[64], b_mv1[64], b_mvb1[64], b_sad[64], b_c2[64], b_dis[64], b_disb[64], b_per[64], b_mvt[64], b_mv[64], b_mvb[64];
    std::snprintf(b_prev, sizeof(b_prev), "p%llu_prev.rgba", (unsigned long long)s.pair);
    std::snprintf(b_next, sizeof(b_next), "p%llu_next.rgba", (unsigned long long)s.pair);
    std::snprintf(b_mv1,  sizeof(b_mv1),  "p%llu_mv1.rg16f", (unsigned long long)s.pair);
    auto nm = [&](bool have, char* buf, size_t bufsz, const char* suffix) -> const char* {
        if (!have) { buf[0] = '-'; buf[1] = '\0'; return buf; }
        std::snprintf(buf, bufsz, "p%llu_%s", (unsigned long long)s.pair, suffix); return buf;
    };
    nm(s.has_mvb1, b_mvb1, sizeof(b_mvb1), "mvb1.rg16f");
    nm(s.has_sad,  b_sad,  sizeof(b_sad),  "sad.rg16f");
    nm(s.has_c2,   b_c2,   sizeof(b_c2),   "c2.rgba16f");
    nm(s.has_dis,  b_dis,  sizeof(b_dis),  "dis.r8");
    nm(s.has_disb, b_disb, sizeof(b_disb), "disb.r8");
    nm(s.has_per,  b_per,  sizeof(b_per),  "per.r8");
    nm(s.has_mvt,  b_mvt,  sizeof(b_mvt),  "mvt.rg16f");
    nm(s.has_mv,   b_mv,   sizeof(b_mv),   "mv.rg16f");
    nm(s.has_mvb,  b_mvb,  sizeof(b_mvb),  "mvb.rg16f");
    std::fprintf(f_pairs_,
        "%llu %d %d %llu %d %.9g %.9g %.9g %.9g %.9g %.9g %s %s %s %s %s %s %s %s %s %s %s %s\n",
        (unsigned long long)s.pair, s.gen, s.tgen, (unsigned long long)seq_recorded, s.gme_valid,
        (double)s.gme6[0], (double)s.gme6[1], (double)s.gme6[2], (double)s.gme6[3], (double)s.gme6[4], (double)s.gme6[5],
        b_prev, b_next, b_mv1, b_mvb1, b_sad, b_c2, b_dis, b_disb, b_per, b_mvt, b_mv, b_mvb);
    return true;   // pairs_written is counted ONCE, by release_pair_set(idx, written) in writer_main — never here
}

// ── stop (idempotent) ───────────────────────────────────────────────────────────────────────────────
void GdumpTap::stop(uint64_t total_presents) noexcept {
    if (stopped_) return;
    stopped_ = true;
    if (!armed_) return;   // never opened anything (S7's spirit: a disarmed tap stays silent beyond its one DISARMED line)
    stop_ = true;
    if (writer_.joinable()) writer_.join();   // CR3: joined BEFORE the semaphore/buffers die (destructor)
    // CR3, the second half (SEEN 2026-09-09: observer run r2 hung at exit with the tap's own summary already
    // printed). A recorded tick whose ring was full still signals semCapTL (the signal rides every recorded submit
    // so the timeline stays monotone) but pushes no descriptor, so nobody waits for it; if such ticks are the LAST
    // ones, vkDestroySemaphore runs with a signal operation still pending — out of spec, and the exit wedged in the
    // driver. Wait the final value here, bounded and device-loss-aware, before anything is destroyed. This is P at
    // the loop EXIT, never per tick.
    if (sem_ && cap_seq_) {
        VkSemaphoreWaitInfo wi{}; wi.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
        wi.semaphoreCount = 1; wi.pSemaphores = &sem_; wi.pValues = &cap_seq_;
        if (!vk_wait_sem_live(A_.dev, wi)) std::printf("[ra] gdump: the last timeline value (%llu) did not signal before teardown -- device lost or quit timeout\n", (unsigned long long)cap_seq_);
    }

    if (FILE* f = std::fopen((dir_ + "\\summary.txt").c_str(), "wb")) {
        const std::string txt = GdumpBook::summary_text(book_.counters(), total_presents);
        std::fwrite(txt.data(), 1, txt.size(), f);
        std::fclose(f);
    }
    if (live_raw_ != INVALID_HANDLE_VALUE) { CloseHandle(live_raw_); live_raw_ = INVALID_HANDLE_VALUE; }
    if (f_live_)  { std::fclose(f_live_);  f_live_  = nullptr; }
    if (f_push_)  { std::fclose(f_push_);  f_push_  = nullptr; }
    if (f_ticks_) { std::fclose(f_ticks_); f_ticks_ = nullptr; }
    if (f_pairs_) { std::fclose(f_pairs_); f_pairs_ = nullptr; }
    if (f_xref_)  { std::fclose(f_xref_);  f_xref_  = nullptr; }

    const GdumpBook::Counters& c = book_.counters();
    std::printf("[ra] gdump: %llu captured / %llu recorded ticks, %llu ring-full, %llu pairs written, %.1f MB\n",
                (unsigned long long)c.captured, (unsigned long long)c.recorded, (unsigned long long)c.ring_full,
                (unsigned long long)c.pairs_written, c.bytes / 1048576.0);
}

}  // namespace pfg::instrument
// Made with my soul - Swately <3
