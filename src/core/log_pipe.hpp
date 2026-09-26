// PhyriadFG - src/core/log_pipe.hpp : the log pipe. stdout goes through an in-memory anonymous pipe that ONE background
// thread drains into the real stdout target, so no FG thread ever waits on the disk to print.
//
// Why (docs/planning/records/LEVER1_RECORD.md s4.8): stdout is unbuffered (main.cpp), and when it is redirected to a file
// on a disk another writer saturates (the --gdump tap streaming 15+ GB), each printf blocked its thread for 0.1-0.8 s. On
// the present thread that stopped the ticks, tripped the plane watchdog, and froze the pair. Through the pipe, a printf
// returns as soon as its bytes are in memory. Only the drainer waits on the disk. Order is kept (one pipe, one reader),
// and the output stays live, because the drainer writes each chunk as soon as it reads it.
//
// Scope: it is armed only when stdout is a FILE or a PIPE. An interactive console is left exactly as it was: a console
// write does not queue behind disk I/O, and the CRT writes consoles by its own route. A normal exit drains the pipe
// before the process ends (atexit). A crash (std::terminate, an unhandled SEH exception) or the forced exit waits at
// most 200 ms for the pipe to empty, without taking any lock. Anything still in the pipe past that is lost, and the
// header says so.
// Made with my soul - Swately <3
#pragma once

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>

namespace pfg::core {

// One pipe, one drainer thread. Every byte written to write_handle() reaches `sink` in order. A writer waits only
// when the pipe's buffer is full, which a log never fills at the default 1 MB.
class LogPipe {
public:
    using Sink = void (*)(void* ctx, const char* p, DWORD n);
    static constexpr DWORD kDefaultBytes = 1u << 20;

    bool start(Sink sink, void* ctx, DWORD pipe_bytes = kDefaultBytes) noexcept {
        if (thread_) return false;
        sink_ = sink; ctx_ = ctx;
        if (!CreatePipe(&rd_, &wr_, nullptr, pipe_bytes)) { rd_ = wr_ = nullptr; return false; }
        thread_ = CreateThread(nullptr, 0, &LogPipe::drain_main, this, 0, nullptr);
        if (!thread_) { CloseHandle(rd_); CloseHandle(wr_); rd_ = wr_ = nullptr; return false; }
        return true;
    }
    HANDLE write_handle() const noexcept { return wr_; }
    // The write handle was handed to someone who will close it (the CRT, via _open_osfhandle).
    void release_write_handle() noexcept { wr_ = nullptr; }
    // Close the write side (if still ours), then wait for the drainer to reach EOF. Bounded; true = drained and exited.
    bool close_and_join(DWORD timeout_ms) noexcept {
        if (wr_) { CloseHandle(wr_); wr_ = nullptr; }
        if (!thread_) return true;
        const bool ok = WaitForSingleObject(thread_, timeout_ms) == WAIT_OBJECT_0;
        if (ok) { CloseHandle(thread_); thread_ = nullptr; CloseHandle(rd_); rd_ = nullptr; }
        return ok;
    }
    // Wait until the drainer has written everything it was given: it sits in ReadFile, is not mid-write, and its drained
    // count has not moved for 3 consecutive 1 ms samples (a pending synchronous ReadFile returns within microseconds once
    // bytes arrive). It does NOT peek the pipe: a PeekNamedPipe on the read handle would queue behind the drainer's
    // pending synchronous ReadFile and hang exactly when the pipe is empty. Bounded, no locks: safe on a crash path.
    bool drain(DWORD timeout_ms) const noexcept {
        if (!thread_) return true;
        const ULONGLONG t0 = GetTickCount64();
        uint64_t last = drained_.load();
        int stable = 0;
        for (;;) {
            if (done_.load()) return true;
            if (waiting_.load() && !busy_.load()) {
                const uint64_t now = drained_.load();
                if (now == last) { if (++stable >= 3) return true; }
                else { stable = 0; last = now; }
            } else { stable = 0; last = drained_.load(); }
            if (GetTickCount64() - t0 >= timeout_ms) return false;
            Sleep(1);
        }
    }
    uint64_t drained_bytes() const noexcept { return drained_.load(); }

private:
    static DWORD WINAPI drain_main(LPVOID self) noexcept {
        auto* lp = static_cast<LogPipe*>(self);
        char buf[64 * 1024];
        for (;;) {
            DWORD n = 0;
            lp->waiting_.store(true);
            const BOOL ok = ReadFile(lp->rd_, buf, sizeof(buf), &n, nullptr);
            lp->waiting_.store(false);
            if (!ok || n == 0) break;   // EOF: every write handle closed
            lp->busy_.store(true);
            lp->sink_(lp->ctx_, buf, n);
            lp->drained_.fetch_add(n);
            lp->busy_.store(false);
        }
        lp->done_.store(true);
        return 0;
    }
    Sink   sink_ = nullptr;
    void*  ctx_  = nullptr;
    HANDLE rd_ = nullptr, wr_ = nullptr, thread_ = nullptr;
    std::atomic<bool>     waiting_{false};   // the drainer is inside ReadFile
    std::atomic<bool>     busy_{false};      // the drainer is inside the sink
    std::atomic<bool>     done_{false};
    std::atomic<uint64_t> drained_{0};
};

namespace log_pipe_detail {
struct StdoutRoute {
    LogPipe pipe;
    HANDLE  target = nullptr;   // our duplicate of the original stdout handle (the drainer's sink)
    int     mode   = _O_TEXT;   // the original fd 1 translation mode, kept for the pipe
    bool    armed  = false;
    std::terminate_handler       prev_terminate = nullptr;
    LPTOP_LEVEL_EXCEPTION_FILTER prev_filter    = nullptr;
};
inline StdoutRoute& route() noexcept { static StdoutRoute r; return r; }

inline void write_all(void* ctx, const char* p, DWORD n) noexcept {
    const HANDLE h = static_cast<HANDLE>(ctx);
    while (n > 0) {
        DWORD w = 0;
        if (!WriteFile(h, p, n, &w, nullptr) || w == 0) return;   // the target is gone: nothing more can be done
        p += w; n -= w;
    }
}

inline void at_exit() noexcept {
    StdoutRoute& r = route();
    if (!r.armed) return;
    r.armed = false;
    std::fflush(stdout);
    // Point fd 1 back at the original target. _dup2 closes fd 1's pipe handle, the pipe's only write handle, so the
    // drainer reads the rest and reaches EOF. The workers are joined by now, so nothing prints while it drains.
    HANDLE back = nullptr;
    if (DuplicateHandle(GetCurrentProcess(), r.target, GetCurrentProcess(), &back, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
        const int ofd = _open_osfhandle(reinterpret_cast<intptr_t>(back), r.mode);
        if (ofd >= 0) { _dup2(ofd, _fileno(stdout)); _close(ofd); }
        else CloseHandle(back);
    }
    SetStdHandle(STD_OUTPUT_HANDLE, reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(stdout))));
    (void)r.pipe.close_and_join(5000);
}

inline void on_terminate() noexcept {
    StdoutRoute& r = route();
    if (r.armed) (void)r.pipe.drain(200);
    if (r.prev_terminate) r.prev_terminate();
    std::abort();
}

inline LONG WINAPI on_unhandled(EXCEPTION_POINTERS* ep) noexcept {
    StdoutRoute& r = route();
    if (r.armed) (void)r.pipe.drain(200);
    return r.prev_filter ? r.prev_filter(ep) : EXCEPTION_CONTINUE_SEARCH;
}
}  // namespace log_pipe_detail

// Route this process's stdout through a LogPipe. Armed only for a FILE or PIPE stdout. Returns true when armed. On any
// failure, stdout stays exactly as it was.
inline bool log_pipe_stdout_start() noexcept {
    using namespace log_pipe_detail;
    StdoutRoute& r = route();
    if (r.armed) return true;
    std::fflush(stdout);
    const int fd1 = _fileno(stdout);
    const HANDLE orig = reinterpret_cast<HANDLE>(_get_osfhandle(fd1));
    if (orig == INVALID_HANDLE_VALUE || orig == nullptr) return false;
    const DWORD type = GetFileType(orig);
    if (type != FILE_TYPE_DISK && type != FILE_TYPE_PIPE) return false;   // a console (or unknown) stays direct
    if (!DuplicateHandle(GetCurrentProcess(), orig, GetCurrentProcess(), &r.target, 0, FALSE, DUPLICATE_SAME_ACCESS))
        return false;
    r.mode = _setmode(fd1, _O_BINARY);        // read the current translation mode ...
    if (r.mode != -1) _setmode(fd1, r.mode);  // ... and put it back: the pipe gets the same one, so the bytes are the same
    else r.mode = _O_TEXT;
    if (!r.pipe.start(&write_all, r.target)) { CloseHandle(r.target); r.target = nullptr; return false; }
    const int pfd = _open_osfhandle(reinterpret_cast<intptr_t>(r.pipe.write_handle()), r.mode);
    if (pfd < 0) { (void)r.pipe.close_and_join(1000); CloseHandle(r.target); r.target = nullptr; return false; }
    r.pipe.release_write_handle();            // the CRT owns it now
    if (_dup2(pfd, fd1) != 0) { _close(pfd); (void)r.pipe.close_and_join(1000); CloseHandle(r.target); r.target = nullptr; return false; }
    _close(pfd);                              // fd 1 holds its own duplicate: the pipe's only write handle
    SetStdHandle(STD_OUTPUT_HANDLE, reinterpret_cast<HANDLE>(_get_osfhandle(fd1)));
    r.armed = true;
    std::atexit(&at_exit);
    r.prev_terminate = std::set_terminate(&on_terminate);
    r.prev_filter    = SetUnhandledExceptionFilter(&on_unhandled);
    return true;
}

// Before a forced exit (TerminateProcess): wait at most timeout_ms for the pipe to empty. No locks.
inline void log_pipe_stdout_drain(DWORD timeout_ms) noexcept {
    log_pipe_detail::StdoutRoute& r = log_pipe_detail::route();
    if (r.armed) (void)r.pipe.drain(timeout_ms);
}

}  // namespace pfg::core

#endif  // _WIN32
