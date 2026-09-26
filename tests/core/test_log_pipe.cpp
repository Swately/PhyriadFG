// PhyriadFG - tests/core/test_log_pipe.cpp : the log pipe (src/core/log_pipe.hpp) on a real anonymous pipe, no GPU.
// Its core property is that a writer does not wait on a slow sink (the disk the --gdump tap saturates). Also pinned:
// every byte arrives, in order; nothing is lost at close; drain() is bounded both ways. Exit 0 = every case as expected.
// Made with my soul - Swately <3
#include "core/log_pipe.hpp"

#include <atomic>
#include <cstdio>
#include <string>
#include <thread>

namespace {

int g_fail = 0;

void expect(bool cond, const char* what) {
    if (!cond) { std::printf("FAIL: %s\n", what); ++g_fail; }
    else       { std::printf("ok:   %s\n", what); }
}

struct Collect {
    std::string s;
    DWORD delay_ms = 0;              // a slow sink: the disk behind a saturating writer
    std::atomic<bool> abort{false};  // set when a case gives up: the sink stops delaying, so everything drains
};

void sink(void* ctx, const char* p, DWORD n) {
    auto* c = static_cast<Collect*>(ctx);
    for (DWORD t = 0; t < c->delay_ms && !c->abort.load(); t += 10) Sleep(10);
    c->s.append(p, n);
}

double ms_since(const LARGE_INTEGER& t0) {
    LARGE_INTEGER t1, f; QueryPerformanceCounter(&t1); QueryPerformanceFrequency(&f);
    return (double)(t1.QuadPart - t0.QuadPart) * 1000.0 / (double)f.QuadPart;
}

// n lines of 80 bytes each, written to h; appends the same bytes to `want`.
bool write_lines(HANDLE h, int n, std::string& want) {
    char line[81];
    for (int i = 0; i < n; ++i) {
        std::snprintf(line, sizeof(line), "[ra] log line %06d -- the present thread prints this and must not wait: %04d\n", i, i % 9973);
        std::string l(line);
        l.resize(79, '.'); l += '\n';
        DWORD w = 0;
        if (!WriteFile(h, l.data(), (DWORD)l.size(), &w, nullptr) || w != l.size()) return false;
        want += l;
    }
    return true;
}

}  // namespace

int main() {
    using pfg::core::LogPipe;
    std::printf("LOG_PIPE TEST: stdout's in-memory pipe and its drainer (log_pipe.hpp)\n");

    {   // L1: 2 000 lines arrive complete and in order; close_and_join reaches EOF
        Collect c; LogPipe lp; std::string want;
        const bool st = lp.start(&sink, &c);
        const bool wr = st && write_lines(lp.write_handle(), 2000, want);
        const bool joined = lp.close_and_join(5000);
        expect(st && wr && joined, "L1a started, written, closed and joined");
        expect(c.s == want, "L1b 2 000 lines arrive complete and in order");
    }
    {   // L2: a sink that takes 300 ms per chunk does not slow the writer (64 KB, well under the 1 MB pipe)
        Collect c; c.delay_ms = 300; LogPipe lp; std::string want;
        (void)lp.start(&sink, &c);
        // The writer runs on its own thread with a 2 s deadline, so a writer that blocks makes this case FAIL instead
        // of hanging (with a no-capacity pipe, 64 000 bytes behind a 300 ms sink would take hours).
        std::atomic<bool> done{false}; bool wr = false; double took = -1.0;
        std::thread w([&] { LARGE_INTEGER t0; QueryPerformanceCounter(&t0);
                            wr = write_lines(lp.write_handle(), 800, want); took = ms_since(t0); done.store(true); });
        for (int i = 0; i < 200 && !done.load(); ++i) Sleep(10);
        const bool in_time = done.load();
        if (!in_time) c.abort.store(true);   // let it all drain so the case can end
        w.join();
        std::printf("      writing 64 000 bytes behind a 300 ms-per-chunk sink took %.2f ms%s\n", took, in_time ? "" : " (past the 2 s deadline)");
        expect(in_time && wr && took < 100.0, "L2a the writer returns in < 100 ms while the sink is slow");
        const bool joined = lp.close_and_join(30000);
        expect(joined && c.s == want, "L2b everything still arrives, in order");
    }
    {   // L3: drain() is bounded: false while the sink is mid-write, true once it has written everything
        Collect c; c.delay_ms = 300; LogPipe lp; std::string want;
        (void)lp.start(&sink, &c);
        (void)write_lines(lp.write_handle(), 1, want);
        Sleep(30);                                     // the drainer has picked it up and is inside the slow sink
        const bool early = lp.drain(50);
        const bool later = lp.drain(3000);
        expect(!early, "L3a drain(50) returns false while the sink is still writing");
        expect(later && c.s == want, "L3b drain(3000) returns true once the sink wrote it");
        (void)lp.close_and_join(5000);
    }
    {   // L4: an idle pipe drains at once, and it does not hang on the drainer's pending ReadFile
        Collect c; LogPipe lp;
        (void)lp.start(&sink, &c);
        LARGE_INTEGER t0; QueryPerformanceCounter(&t0);
        const bool d = lp.drain(1000);
        const double took = ms_since(t0);
        expect(d && took < 100.0, "L4 an idle pipe drains at once (no hang on the pending ReadFile)");
        (void)lp.close_and_join(5000);
    }

    if (g_fail == 0) { std::printf("LOG_PIPE TEST: all passed\n"); return 0; }
    std::printf("LOG_PIPE TEST: %d FAILED\n", g_fail);
    return 1;
}
