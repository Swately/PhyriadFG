#pragma once
// phyriad/render/present/D3d11StampRing.hpp — a non-blocking ring of D3D11 GPU timestamp brackets (INSTRUMENT;
// header-only; no allocation after create()).
//
// One bracket = a D3D11_QUERY_TIMESTAMP_DISJOINT around `marks` D3D11_QUERY_TIMESTAMP queries:
//     const int s = ring.begin();          // Begin(disjoint) + End(ts[0])     (-1: every slot in flight -> skipped)
//     ... the GPU work to time ...
//     ring.mark(s, 1);                     // End(ts[1]) ... up to marks-1
//     ring.end(s);                         // End(disjoint); the slot is published to the reader
//   later, on the reading thread:
//     double iv[kMaxMarks - 1]; while (ring.take(iv) != Take::None) { ... iv[k] = ts[k+1] - ts[k] in ms ... }
//
// Rules:
//   - The ring never waits. take() reads with D3D11_ASYNC_GETDATA_DONOTFLUSH and returns None until the GPU has
//     written every query of the oldest published bracket; begin() skips (and counts) when the slot it would reuse
//     has not been read yet. A bracket whose disjoint query reports Disjoint (the GPU clock changed frequency or
//     was interrupted inside it) is dropped and counted, never returned as a number.
//   - One issuing thread (begin/mark/end) and one reading thread (take); they may be the same thread or two, and
//     the slot state is an atomic handed over at end() / take(). When they are two threads, the context MUST be
//     multithread-protected (ID3D11Multithread::SetMultithreadProtected(TRUE)): both threads call into the same
//     immediate context.
//   - Off = byte-identical: an unarmed ring (create() never called) records nothing; every call is a no-op.
// Made with my soul - Swately <3
#include <d3d11.h>
#include <atomic>
#include <cstdint>

namespace phyriad::render::present {

class D3d11StampRing {
public:
    static constexpr int kSlots    = 16;
    static constexpr int kMaxMarks = 4;
    enum class Take { None, Sample, Disjoint };

    D3d11StampRing() noexcept = default;
    D3d11StampRing(const D3d11StampRing&)            = delete;
    D3d11StampRing& operator=(const D3d11StampRing&) = delete;
    ~D3d11StampRing() noexcept { release(); }

    // Creates every query up front (kSlots x (1 + marks)). `ctx` is the immediate context the brackets are issued
    // on and read from; it is not AddRef'd — its owner keeps it alive until release(). false = nothing armed.
    bool create(ID3D11Device* dev, ID3D11DeviceContext* ctx, int marks) noexcept {
        release();
        if (!dev || !ctx || marks < 2 || marks > kMaxMarks) return false;
        const D3D11_QUERY_DESC dj{ D3D11_QUERY_TIMESTAMP_DISJOINT, 0 };
        const D3D11_QUERY_DESC ts{ D3D11_QUERY_TIMESTAMP, 0 };
        for (Slot& s : s_) {
            if (FAILED(dev->CreateQuery(&dj, &s.dj))) { release(); return false; }
            for (int i = 0; i < marks; ++i)
                if (FAILED(dev->CreateQuery(&ts, &s.ts[i]))) { release(); return false; }
        }
        ctx_ = ctx; marks_ = marks;
        return true;
    }

    // Releases every query. Call it before the device is released, and only when no thread can still issue.
    void release() noexcept {
        for (Slot& s : s_) {
            if (s.dj) { s.dj->Release(); s.dj = nullptr; }
            for (ID3D11Query*& q : s.ts) if (q) { q->Release(); q = nullptr; }
            s.state.store(0);
        }
        ctx_ = nullptr; marks_ = 0; w_ = 0; r_ = 0;
    }

    bool armed() const noexcept { return ctx_ != nullptr; }

    // ── the issuing thread ──
    int begin() noexcept {
        if (!ctx_) return -1;
        const int i = (int)(w_ % kSlots);
        if (s_[i].state.load() != 0) { skipped_.fetch_add(1); return -1; }   // not read yet: skip, never wait
        ctx_->Begin(s_[i].dj);
        ctx_->End(s_[i].ts[0]);
        return i;
    }
    void mark(int slot, int m) noexcept {
        if (slot >= 0 && m > 0 && m < marks_) ctx_->End(s_[slot].ts[m]);
    }
    void end(int slot) noexcept {
        if (slot < 0) return;
        ctx_->End(s_[slot].dj);
        s_[slot].state.store(1);   // published: the reader may now poll it
        ++w_;
    }

    // ── the reading thread ── one bracket, oldest first. Sample: intervals_ms[k] = ts[k+1] - ts[k] for
    // k < marks-1 (a negative interval is written as -1). slot_out (optional) = the slot, so a caller can pair it
    // with data it stored by slot at begin().
    Take take(double* intervals_ms, int* slot_out = nullptr) noexcept {
        if (!ctx_) return Take::None;
        const int i = (int)(r_ % kSlots);
        Slot& s = s_[i];
        if (s.state.load() != 1) return Take::None;
        D3D11_QUERY_DATA_TIMESTAMP_DISJOINT d{};
        if (ctx_->GetData(s.dj, &d, sizeof d, D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK) return Take::None;
        UINT64 t[kMaxMarks] = {};
        for (int m = 0; m < marks_; ++m)
            if (ctx_->GetData(s.ts[m], &t[m], sizeof t[m], D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK) return Take::None;
        if (slot_out) *slot_out = i;
        s.state.store(0);   // handed back to the issuing thread
        ++r_;
        if (d.Disjoint || d.Frequency == 0) { disjoint_.fetch_add(1); return Take::Disjoint; }
        for (int m = 0; m + 1 < marks_; ++m)
            intervals_ms[m] = t[m + 1] >= t[m] ? (double)(t[m + 1] - t[m]) * 1000.0 / (double)d.Frequency : -1.0;
        return Take::Sample;
    }

    uint64_t skipped() const noexcept { return skipped_.load(); }    // begin() found its slot still unread
    uint64_t disjoint() const noexcept { return disjoint_.load(); }  // brackets dropped as Disjoint

private:
    struct Slot {
        ID3D11Query*     dj = nullptr;
        ID3D11Query*     ts[kMaxMarks] = {};
        std::atomic<int> state{0};   // 0 free (issuer may reuse) | 1 published (reader may poll)
    };
    Slot                 s_[kSlots];
    ID3D11DeviceContext* ctx_   = nullptr;
    int                  marks_ = 0;
    uint64_t             w_ = 0;   // the issuing thread's cursor
    uint64_t             r_ = 0;   // the reading thread's cursor
    std::atomic<uint64_t> skipped_{0}, disjoint_{0};
};

}  // namespace phyriad::render::present
