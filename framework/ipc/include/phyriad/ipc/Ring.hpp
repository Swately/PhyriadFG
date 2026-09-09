// framework/ipc/include/phyriad/ipc/Ring.hpp
// SPSC lock-free ring buffer.
//
// Provides:
//   Ring<T, Capacity> — single-producer / single-consumer bounded ring.
//
// Guarantees:
//   - T must be trivially_copyable (memcpy-safe).
//   - Capacity must be a power of 2 (compile-time checked).
//   - Zero heap allocation — all storage is embedded in the struct.
//   - Cache-line separated producer/consumer cursors → no false sharing.
//   - try_push / try_pop are lock-free and wait-free.
//   - drain() uses at most 2 memcpy calls (handles ring wrap).
//
// Threading:
//   Exactly ONE producer thread and ONE consumer thread.
//   All other combinations are UB.
//
// Memory orders:
//   - Load own cursor: relaxed (only this side writes it).
//   - Load other cursor: acquire.
//   - Store own cursor: release.
//

#pragma once

#include <phyriad/hal/Cacheline.hpp>

#include <array>
#include <atomic>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <span>
#include <type_traits>
#include <phyriad/hal/MemoryOrder.hpp>

namespace phyriad::ipc {

/// SPSC lock-free ring buffer.
///
/// `T`        — element type; must be trivially_copyable.
/// `Capacity` — maximum elements; must be a non-zero power of 2.
template <typename T, uint32_t Capacity>
class alignas(phyriad::hal::kDestructivePad) Ring {
    static_assert(std::is_trivially_copyable_v<T>,
        "Ring<T>: T must be trivially_copyable");
    static_assert(Capacity > 0u,
        "Ring<T>: Capacity must be > 0");
    static_assert((Capacity & (Capacity - 1u)) == 0u,
        "Ring<T>: Capacity must be a power of 2");

public:
    Ring() noexcept = default;

    // Non-copyable, non-movable (contains atomics + large storage).
    Ring(const Ring&)            = delete;
    Ring& operator=(const Ring&) = delete;
    Ring(Ring&&)                 = delete;
    Ring& operator=(Ring&&)      = delete;

    // ── Producer API ─────────────────────────────────────────────────────────

    /// Try to push one element. Returns false if the ring is full.
    /// Increments dropped() counter on failure.
    /// Thread: producer only.
    [[nodiscard]] bool try_push(const T& v) noexcept {
        const uint64_t w = hal::stat_load_relaxed(write_seq_);
        const uint64_t r = hal::seq_load_acquire(read_seq_);
        if (w - r >= Capacity) {
            hal::stat_fetch_add_relaxed(dropped_, 1u);
            return false;
        }
        slots_[w & kMask] = v;
        hal::seq_store_release(write_seq_, w + 1u);
        return true;
    }

    /// Push without overflow check. Asserts in Debug if ring is full.
    /// Use only when the caller guarantees the ring will not overflow.
    /// Thread: producer only.
    uint64_t push_unchecked(const T& v) noexcept {
        const uint64_t w = hal::stat_load_relaxed(write_seq_);
#if !defined(NDEBUG)
        const uint64_t r = hal::seq_load_acquire(read_seq_);
        // In Debug: detect overflow but still push (overwrite oldest slot).
        if (w - r >= Capacity) {
            hal::stat_fetch_add_relaxed(dropped_, 1u);
        }
#endif
        slots_[w & kMask] = v;
        hal::seq_store_release(write_seq_, w + 1u);
        return w + 1u;
    }

    // ── Consumer API ─────────────────────────────────────────────────────────

    /// Try to pop one element. Returns false if the ring is empty.
    /// Thread: consumer only.
    [[nodiscard]] bool try_pop(T& out) noexcept {
        const uint64_t r = hal::stat_load_relaxed(read_seq_);
        const uint64_t w = hal::seq_load_acquire(write_seq_);
        if (r == w) return false;
        out = slots_[r & kMask];
        hal::seq_store_release(read_seq_, r + 1u);
        return true;
    }

    /// Drain up to `max` elements into `out[0..max)`.
    /// Returns the number of elements actually copied (may be 0).
    ///
    /// Uses at most 2 memcpy calls (handles ring wrap efficiently).
    /// Thread: consumer only.
    [[nodiscard]] uint32_t drain(T* out, uint32_t max) noexcept {
        const uint64_t r = hal::stat_load_relaxed(read_seq_);
        const uint64_t w = hal::seq_load_acquire(write_seq_);

        const uint64_t available = w - r;
        const uint32_t take = static_cast<uint32_t>(
            std::min<uint64_t>(available, static_cast<uint64_t>(max)));
        if (take == 0u) return 0u;

        const uint32_t r_idx = static_cast<uint32_t>(r & kMask);
        const uint32_t first_chunk = std::min(take, Capacity - r_idx);

        std::memcpy(out, &slots_[r_idx], first_chunk * sizeof(T));
        if (first_chunk < take) {
            std::memcpy(out + first_chunk, &slots_[0],
                        (take - first_chunk) * sizeof(T));
        }

        hal::seq_store_release(read_seq_, r + take);
        return take;
    }

    // ── Multi-reader cursor API ───────────────────────────────────────────────
    //
    // These methods allow multiple independent readers to maintain their own
    // external cursors without touching the internal read_seq_.  The producer
    // still uses try_push(); the SPSC drain() path is unaffected.
    //
    // Typical pattern:
    //   uint64_t my_cursor = 0;
    //   while (true) {
    //       const uint64_t head = ring.write_cursor();
    //       while (my_cursor < head) {
    //           if (head - my_cursor > Capacity) my_cursor = head - Capacity;
    //           T item;
    //           if (ring.peek_at(my_cursor, item)) process(item);
    //           ++my_cursor;
    //       }
    //   }

    /// Returns the current write head (absolute sequence number).
    /// Callers can diff this against a saved cursor to know how many new
    /// elements are available.  Uses acquire ordering for visibility.
    [[nodiscard]] uint64_t write_cursor() const noexcept {
        return hal::seq_load_acquire(write_seq_);
    }

    /// Peek at the element at absolute sequence number `seq` without
    /// advancing any cursor.  `seq` must be < write_cursor().
    ///
    /// Returns false only if `seq` has not yet been written (seq >= write_head).
    /// Does NOT check for slot reuse (overwrite by producer) — the caller must
    /// guard against this by checking `write_cursor() - seq < Capacity`.
    ///
    /// Thread: safe for any reader; does not race with producer writes because
    /// the write to the slot is released before write_seq_ is updated.
    [[nodiscard]] bool peek_at(uint64_t seq, T& out) const noexcept {
        const uint64_t w = hal::seq_load_acquire(write_seq_);
        if (seq >= w) return false;
        out = slots_[seq & kMask];
        return true;
    }

    // ── Status API (approximate — no synchronization guarantee) ──────────────

    /// Approximate number of elements currently in the ring.
    /// Safe for SPSC; may be slightly stale for cross-thread queries.
    [[nodiscard]] uint32_t size() const noexcept {
        const uint64_t w = hal::seq_load_acquire(write_seq_);
        const uint64_t r = hal::seq_load_acquire(read_seq_);
        return static_cast<uint32_t>(w - r);
    }

    [[nodiscard]] bool empty() const noexcept {
        return hal::seq_load_acquire(write_seq_) ==
               hal::seq_load_acquire(read_seq_);
    }

    [[nodiscard]] static constexpr uint32_t capacity() noexcept {
        return Capacity;
    }

    /// Read-only span over ALL capacity slots (independent of cursors).
    ///
    /// Useful for display-only UI panels that want to walk the ring without
    /// consuming entries.  Only slots in the live window
    ///   [write_cursor() - min(write_cursor(), Capacity), write_cursor())
    /// are guaranteed to contain valid data; the rest are zero-initialised
    /// or stale from a previous wrap-around.
    [[nodiscard]] std::span<const T> slots_view() const noexcept {
        return {slots_.data(), slots_.size()};
    }

    /// Number of failed try_push calls due to the ring being full.
    /// Incremented by the producer; read by anyone for diagnostics.
    [[nodiscard]] uint64_t dropped() const noexcept {
        return hal::stat_load_relaxed(dropped_);
    }

    /// Reset drop counter. Call from producer side only.
    void reset_dropped() noexcept {
        hal::stat_store_relaxed(dropped_, 0u);
    }

private:
    static constexpr uint32_t kMask = Capacity - 1u;

    // Separate atomic cursors onto their own cache lines to eliminate
    // false sharing between the producer (writes write_seq_) and
    // the consumer (writes read_seq_).
    alignas(phyriad::hal::kDestructivePad) std::atomic<uint64_t> write_seq_{0u};
    alignas(phyriad::hal::kDestructivePad) std::atomic<uint64_t> read_seq_{0u};
    alignas(phyriad::hal::kDestructivePad) std::atomic<uint64_t> dropped_{0u};

    // Slot storage — cache-aligned to reduce cross-cache-line reads in drain().
    alignas(phyriad::hal::kCachelineSize)  std::array<T, Capacity> slots_{};
};

} // namespace phyriad::ipc
// Made with my soul - Swately <3
