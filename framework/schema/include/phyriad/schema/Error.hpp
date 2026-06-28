// framework/schema/include/phyriad/schema/Error.hpp
// Hot-path error type — 16 bytes, alignas(16), trivially copyable.
//
// Layout matches the wire error format used by all pillars.
// transport, node, graph, runtime, and orchestration all use this type.
//
// The Error struct lives in schema (not runtime) because:
//   - It is a pure POD with zero dependencies above <cstdint>/<type_traits>
//   - transport needs it before runtime is initialized
//   - schema is the earliest pillar all others depend on
//
// Invariants (static_asserts below):
//   sizeof(Error)  == 16  — fits in two xmm registers, register-passable on x86-64
//   alignof(Error) == 16  — ABI-safe passing via std::expected on ARM / MSVC
//   trivially_copyable    — safe for memcpy across SHM or ring slots
//
// Usage:
//   return std::unexpected(phyriad::Error{ErrorCode::RingFull, node_id, 0});
#pragma once
#include <cstdint>
#include <type_traits>

namespace phyriad {

using NodeId = uint32_t;

enum class ErrorCode : uint32_t {
    None              = 0,
    RingFull          = 1,
    RingEmpty         = 2,
    SchemaMismatch    = 3,
    NodePaused        = 4,
    DeadConsumer      = 5,
    QuiescenceTimeout = 6,
    PrivilegeRequired = 7,
    ShmOpenFailed     = 8,
    Timeout           = 9,
    InvalidHandle     = 10,
    BufferTooSmall    = 11,
    PayloadTooLarge   = 12,
    IoError           = 13,
    InvalidNodeId     = 14,
    ShuttingDown      = 15,
    WindowClosed      = 16,
    ResourceInitFailed = 17,
    // ── Phase 5 additions (daemon pillar) ─────────────────────────────────────
    PermissionDenied   = 18,  // insufficient OS privileges (ETW, affinity)
    InvalidArgument    = 19,  // caller passed an invalid argument
    Unavailable        = 20,  // service not available (daemon not running)
    AlreadyConnected   = 21,  // client already has an active connection
    ResourceExhausted  = 22,  // no free slots / capacity exhausted
    // ── FR-8: extended error codes (added for downstream consumer needs) ──
    SystemError        = 23,  // OS/Win32/POSIX call returned an unexpected error
    BufferFull         = 24,  // fixed-capacity buffer is at capacity
    OutOfMemory        = 25,  // operator new / malloc returned null
};

struct alignas(16) Error {
    ErrorCode code{ErrorCode::None};
    NodeId    source_node_id{0};
    uint64_t  timestamp_ns{0};
};
static_assert(sizeof(Error)  == 16, "Error must be exactly 16 bytes");
static_assert(alignof(Error) == 16, "Error must be aligned to 16 bytes");
static_assert(std::is_trivially_copyable_v<Error>,
    "Error must be trivially copyable for SHM/ring slot transfer");

} // namespace phyriad
// Made with my soul - Swately <3
