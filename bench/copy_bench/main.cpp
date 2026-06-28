// apps/render_assistant/bench/copy_bench/main.cpp
// STAGE-37c — copy-path bench (1080p, 256 reps, median).
//
// Matrix: rows = {std::memcpy, SlotCopy auto, row-strided (WGC Map RowPitch)}
//         sizes = {8.3 MB BGRA, 6.2 MB RGB8 packed}
//         cols  = {_aligned_malloc 4KB, hal::aligned_alloc_hint hugepage}
//
// Prints a markdown table: GB/s | ms/frame | ms/s@125fps.
// If SlotCopy or hugepages gain <10% on the strided real-shape row: says do not integrate.
//
// Build (standalone):
//   MSVC:  cmake -B build -G "NMake Makefiles" && cmake --build build
//   MinGW: cmake -B build -G "MinGW Makefiles" && cmake --build build

#include <phyriad/hal/Allocator.hpp>
#include <phyriad/hal/Timestamp.hpp>
#include <phyriad/transport/SlotCopy.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>

#ifdef _WIN32
#  include <malloc.h>   // _aligned_malloc / _aligned_free
#else
#  include <cstdlib>    // posix_memalign / free
#endif

// ── Constants ─────────────────────────────────────────────────────────────────
static constexpr uint32_t W           = 1920u;
static constexpr uint32_t H           = 1080u;
static constexpr size_t   SZ_BGRA     = (size_t)W * H * 4u;    // 8,294,400 B
static constexpr size_t   SZ_RGB8     = (size_t)W * H * 3u;    // 6,220,800 B
static constexpr size_t   ROW_PITCH   = 2304u * 4u;             // 9,216 B — WGC Map RowPitch
static constexpr size_t   ROW_TIGHT   = (size_t)W * 4u;         // 7,680 B
static constexpr size_t   SZ_STRD_SRC = (size_t)H * ROW_PITCH; // 9,953,280 B
static constexpr int      REPS        = 256;  // ≥200

// ── Allocation helpers ─────────────────────────────────────────────────────────

static void* alloc_4k(size_t sz) {
#ifdef _WIN32
    return _aligned_malloc(sz, 4096u);
#else
    void* p = nullptr;
    if (posix_memalign(&p, 4096u, sz)) return nullptr;
    return p;
#endif
}

static void free_4k(void* p) {
#ifdef _WIN32
    _aligned_free(p);
#else
    std::free(p);
#endif
}

using phyriad::hal::AllocHint;

static void* alloc_huge(size_t sz) {
    return phyriad::hal::aligned_alloc_hint(sz, 4096u, AllocHint::Huge);
}

static void free_huge(void* p, size_t sz) {
    phyriad::hal::aligned_free_hint(p, sz, AllocHint::Huge);
}

// ── Touch to page in all pages before timing ──────────────────────────────────
static void touch(void* p, size_t sz) {
    volatile uint8_t* v = static_cast<uint8_t*>(p);
    for (size_t i = 0; i < sz; i += 4096u) v[i] = 0;
}

// ── Timing: median over REPS ──────────────────────────────────────────────────
template<typename F>
static uint64_t median_ns(F fn) {
    std::array<uint64_t, REPS> s{};
    for (int i = 0; i < REPS; ++i) {
        const uint64_t t0 = phyriad::hal::rdtsc();
        fn();
        s[static_cast<size_t>(i)] = phyriad::hal::tsc_to_ns(phyriad::hal::rdtsc() - t0);
    }
    std::sort(s.begin(), s.end());
    return s[REPS / 2];
}

// ── Result ────────────────────────────────────────────────────────────────────
struct Row {
    double gbs    = 0.0;
    double ms_f   = 0.0;
    bool   valid  = false;
};

// ── Bench: flat copy (memcpy or SlotCopy) ─────────────────────────────────────
static Row bench_flat(size_t sz, bool huge, bool slotcopy) {
    void* src = huge ? alloc_huge(sz) : alloc_4k(sz);
    void* dst = huge ? alloc_huge(sz) : alloc_4k(sz);
    if (!src || !dst) {
        if (huge) { free_huge(src, sz); free_huge(dst, sz); }
        else      { free_4k(src);       free_4k(dst);       }
        return {};
    }
    touch(src, sz);
    touch(dst, sz);

    uint64_t ns;
    if (slotcopy) {
        const phyriad::transport::SlotCopyFn fn =
            phyriad::transport::pick_slot_copy(
                phyriad::transport::SlotCopyMode::Auto,
                static_cast<uint32_t>(sz));
        ns = median_ns([&]{ fn(dst, src, static_cast<uint32_t>(sz)); });
    } else {
        ns = median_ns([&]{ std::memcpy(dst, src, sz); });
    }

    if (huge) { free_huge(src, sz); free_huge(dst, sz); }
    else      { free_4k(src);       free_4k(dst);       }

    return { (double)sz / 1e9 / (ns / 1e9), ns / 1e6, true };
}

// ── Bench: row-strided (real WGC Map shape) ────────────────────────────────────
// src stride = ROW_PITCH (9216 B), dst tight = ROW_TIGHT (7680 B), 1080 rows.
// Bytes reported = tight output (H * ROW_TIGHT = 8,294,400 B = SZ_BGRA).
static Row bench_strided(bool huge) {
    void* src = huge ? alloc_huge(SZ_STRD_SRC) : alloc_4k(SZ_STRD_SRC);
    void* dst = huge ? alloc_huge(SZ_BGRA)     : alloc_4k(SZ_BGRA);
    if (!src || !dst) {
        if (huge) { free_huge(src, SZ_STRD_SRC); free_huge(dst, SZ_BGRA); }
        else      { free_4k(src);                 free_4k(dst);            }
        return {};
    }
    touch(src, SZ_STRD_SRC);
    touch(dst, SZ_BGRA);

    const uint8_t* s8 = static_cast<const uint8_t*>(src);
    uint8_t*       d8 = static_cast<uint8_t*>(dst);
    uint64_t ns = median_ns([&]{
        for (uint32_t row = 0u; row < H; ++row)
            std::memcpy(d8 + row * ROW_TIGHT,
                        s8 + row * ROW_PITCH, ROW_TIGHT);
    });

    if (huge) { free_huge(src, SZ_STRD_SRC); free_huge(dst, SZ_BGRA); }
    else      { free_4k(src);                 free_4k(dst);            }

    const size_t bytes = (size_t)H * ROW_TIGHT; // tight data transferred
    return { (double)bytes / 1e9 / (ns / 1e9), ns / 1e6, true };
}

// ── Table printing ─────────────────────────────────────────────────────────────
static void print_header() {
    std::printf("| %-24s | %-7s | %-18s | %7s | %9s | %12s |\n",
        "Method", "Size", "Allocator", "GB/s", "ms/frame", "ms/s@125fps");
    std::printf("|:-------------------------|:--------|:-------------------|-------:|----------:|-------------:|\n");
}

static void print_row(const char* method, const char* sz, const char* alloc,
                      const Row& r) {
    if (!r.valid) {
        std::printf("| %-24s | %-7s | %-18s | %7s | %9s | %12s |\n",
            method, sz, alloc, "ERR", "ERR", "ERR");
        return;
    }
    std::printf("| %-24s | %-7s | %-18s | %7.2f | %9.3f | %12.1f |\n",
        method, sz, alloc, r.gbs, r.ms_f, r.ms_f * 125.0);
}

static void print_na_row(const char* method, const char* sz) {
    std::printf("| %-24s | %-7s | %-18s | %7s | %9s | %12s |\n",
        method, sz, "hugepage (n/a)", "n/a", "n/a", "n/a");
}

// ── Recommendation logic ───────────────────────────────────────────────────────
static void recommend(const char* label, double base_gbs, double cand_gbs,
                       bool candidate_available) {
    if (!candidate_available) {
        std::printf("- **%s**: n/a on this system\n", label);
        return;
    }
    const double pct = 100.0 * (cand_gbs - base_gbs) / base_gbs;
    if (pct < 10.0) {
        std::printf("- **%s**: %.2f → %.2f GB/s, gain = %.1f%% — "
                    "**do not integrate** (< 10%%)\n",
                    label, base_gbs, cand_gbs, pct);
    } else {
        std::printf("- **%s**: %.2f → %.2f GB/s, gain = %.1f%% — "
                    "integration viable (>= 10%%)\n",
                    label, base_gbs, cand_gbs, pct);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
int main() {
    (void)phyriad::hal::tsc_to_ns(0u); // trigger static freq init before bench

    const bool have_huge = phyriad::hal::hugepages_available();
    std::printf("hugepages: %s\n\n",
        have_huge ? "available" : "n/a (SeLockMemoryPrivilege not granted)");

    // ── Run all benches ───────────────────────────────────────────────────────
    Row mc_bgra_4k  = bench_flat(SZ_BGRA, false, false);
    Row mc_bgra_hp  = have_huge ? bench_flat(SZ_BGRA, true,  false) : Row{};
    Row mc_rgb8_4k  = bench_flat(SZ_RGB8, false, false);
    Row mc_rgb8_hp  = have_huge ? bench_flat(SZ_RGB8, true,  false) : Row{};

    Row sc_bgra_4k  = bench_flat(SZ_BGRA, false, true);
    Row sc_bgra_hp  = have_huge ? bench_flat(SZ_BGRA, true,  true)  : Row{};
    Row sc_rgb8_4k  = bench_flat(SZ_RGB8, false, true);
    Row sc_rgb8_hp  = have_huge ? bench_flat(SZ_RGB8, true,  true)  : Row{};

    Row st_4k       = bench_strided(false);
    Row st_hp       = have_huge ? bench_strided(true) : Row{};

    // ── Table ─────────────────────────────────────────────────────────────────
    std::puts("## STAGE-37c copy-path bench — 1080p, 256 reps, median\n");
    print_header();
    print_row("std::memcpy",       "8.3MB", "_aligned_malloc", mc_bgra_4k);
    if (have_huge) print_row("std::memcpy",    "8.3MB", "hugepage",       mc_bgra_hp);
    else           print_na_row("std::memcpy",  "8.3MB");
    print_row("std::memcpy",       "6.2MB", "_aligned_malloc", mc_rgb8_4k);
    if (have_huge) print_row("std::memcpy",    "6.2MB", "hugepage",       mc_rgb8_hp);
    else           print_na_row("std::memcpy",  "6.2MB");
    print_row("SlotCopy auto",     "8.3MB", "_aligned_malloc", sc_bgra_4k);
    if (have_huge) print_row("SlotCopy auto",  "8.3MB", "hugepage",       sc_bgra_hp);
    else           print_na_row("SlotCopy auto","8.3MB");
    print_row("SlotCopy auto",     "6.2MB", "_aligned_malloc", sc_rgb8_4k);
    if (have_huge) print_row("SlotCopy auto",  "6.2MB", "hugepage",       sc_rgb8_hp);
    else           print_na_row("SlotCopy auto","6.2MB");
    print_row("row-strided (WGC)", "8.3MB", "_aligned_malloc", st_4k);
    if (have_huge) print_row("row-strided (WGC)","8.3MB","hugepage",      st_hp);
    else           print_na_row("row-strided (WGC)","8.3MB");

    // ── Recommendation ────────────────────────────────────────────────────────
    std::puts("\n## Recommendation (strided real-shape row)\n");
    std::puts("Gate: strided row-strided (WGC) 8.3MB with _aligned_malloc as baseline.\n");

    // SlotCopy on flat: compare slotcopy vs memcpy on 8.3MB (same alloc, 4KB)
    if (sc_bgra_4k.valid && mc_bgra_4k.valid)
        recommend("SlotCopy auto vs memcpy on flat 8.3MB (4KB)",
                  mc_bgra_4k.gbs, sc_bgra_4k.gbs, true);

    // hugepage on strided: compare strided+hp vs strided+4k
    if (st_4k.valid)
        recommend("hugepage vs 4KB on row-strided (WGC) 8.3MB",
                  st_4k.gbs, st_hp.valid ? st_hp.gbs : 0.0, have_huge && st_hp.valid);

    std::puts("");
    return 0;
}
