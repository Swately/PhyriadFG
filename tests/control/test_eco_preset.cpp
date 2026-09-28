// PhyriadFG — tests/control/test_eco_preset.cpp (2026-09-28).
//
// WHAT THIS PINS. `--eco` is the operator's opt-in power preset: EXACTLY `--frame-vram --no-bidir`, with no-bidir's
// cascade, and NOT `--gme-sub2-force` (a diagnostic pin the power arms carried only for equivalence). The claim is about
// the parsed Config BEFORE the cascade runs, so a missing assignment cannot hide behind the cascade re-setting a field:
// parse_args(..., resolve=false) stops right after capture_layer_old, the pre-cascade snapshot.
//
// It runs the REAL hand parser (cli.cpp) and the REAL registry shadow parser (layer_registry.cpp) over three argv:
//   A  = { --eco }                       the preset
//   B  = { --frame-vram --no-bidir }     the flags it names
//   B2 = { --no-bidir --frame-vram }     the same, the other order
// and checks, in order:
//   1. PRE-CASCADE  — the hand fields the two flags set (frame_vram, bidir, occl_thresh, no_bidir), the fields the
//                     cascade will act on (fill_div, matte, phase_anchor), gme_sub2_force (false in all), the whole
//                     registry store (on[], val[]) byte for byte, BIDIR off in the registry, and the R0 parity oracle
//                     (layer_config_parity) true for A and B. The oracle is the check that fails without the
//                     layer_shadow_parse alias: bidir.on old=0 new=1.
//   2. POST-CASCADE — apply_cascades on each: the same fields plus div_eps, matte_thresh and the derived predicate;
//                     the resolved rows (layer_resolve_effective, nothing unavailable) with BIDIR clear; equal contract
//                     hashes.
//   3. THE CASCADE ROUTE — { --eco --matte } against { --frame-vram --no-bidir --matte }: matte is off after the cascade
//                     in both (the no-bidir cascade runs off c.no_bidir, which the preset sets).
//   4. SEEN RED     — the default argv differs from A on frame_vram and bidir (read directly, not through same_fields).
//                     The registry half of same_fields was seen red by removing the layer_shadow_parse alias; the hand
//                     half is pinned by the direct expects in section 1.
//
// WHAT IT DOES NOT COVER, named: the init-time prints (the frame-vram ARMED line, and "flow rows resolved ... bidir=0"
// with its eff mask). They depend on the devices a run finds, so they need a live run (the operator's call).
//
// main() returns non-zero if ANY check fails.
#include "control/cli.hpp"
#include "control/layer_config.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

using pfg::layers::LayerId;

static int g_checks = 0, g_fail = 0;
static void expect(bool ok, const char* what, const char* detail) {
    ++g_checks;
    if (!ok) { ++g_fail; std::fprintf(stderr, "FAIL %-34s %s\n", what, detail); }
}
static bool feq(float a, float b) { return std::memcmp(&a, &b, sizeof a) == 0; }   // bit for bit

static Config parse(std::vector<const char*> args, bool resolve) {
    std::vector<char*> argv;
    argv.push_back(const_cast<char*>("phyriad_fg"));
    for (const char* a : args) argv.push_back(const_cast<char*>(a));
    Config c;
    const bool ok = parse_args((int)argv.size(), argv.data(), c, resolve);
    expect(ok && !c.parse_failed, "parse_args accepts", args.empty() ? "(default)" : args[0]);
    return c;
}

static void same_fields(const Config& a, const Config& b, const char* tag, bool post) {
    char d[160];
    auto chk_b = [&](const char* f, bool x, bool y) { std::snprintf(d, sizeof d, "%s: %s a=%d b=%d", tag, f, (int)x, (int)y); expect(x == y, "field", d); };
    auto chk_f = [&](const char* f, float x, float y) { std::snprintf(d, sizeof d, "%s: %s a=%.9g b=%.9g", tag, f, (double)x, (double)y); expect(feq(x, y), "field", d); };
    chk_b("frame_vram", a.frame_vram, b.frame_vram);
    chk_b("bidir", a.bidir, b.bidir);
    chk_f("occl_thresh", a.occl_thresh, b.occl_thresh);
    chk_b("no_bidir", a.no_bidir, b.no_bidir);
    chk_b("gme_sub2_force", a.gme_sub2_force, b.gme_sub2_force);
    chk_b("fill_div", a.fill_div, b.fill_div);
    chk_b("matte", a.matte, b.matte);
    chk_b("phase_anchor", a.phase_anchor, b.phase_anchor);
    if (post) {
        chk_f("div_eps", a.div_eps, b.div_eps);
        chk_f("matte_thresh", a.matte_thresh, b.matte_thresh);
        chk_b("d.field_to_warp", a.d.field_to_warp, b.d.field_to_warp);
    }
    std::snprintf(d, sizeof d, "%s: registry on[] (%u rows)", tag, (unsigned)pfg::layers::kLayerCount);
    expect(std::memcmp(a.layers.on, b.layers.on, sizeof a.layers.on) == 0, "registry", d);
    std::snprintf(d, sizeof d, "%s: registry val[] (%u params)", tag, (unsigned)pfg::layers::kParamCount);
    expect(std::memcmp(a.layers.val, b.layers.val, sizeof a.layers.val) == 0, "registry", d);
}

int main() {
    std::fprintf(stderr, "eco preset — --eco against --frame-vram --no-bidir:\n");
    const auto BIDIR = (uint16_t)LayerId::BIDIR;

    // 1. PRE-CASCADE
    Config A  = parse({"--eco"}, false);
    Config B  = parse({"--frame-vram", "--no-bidir"}, false);
    Config B2 = parse({"--no-bidir", "--frame-vram"}, false);
    same_fields(A, B, "pre A vs B", false);
    same_fields(A, B2, "pre A vs B2", false);
    expect(A.frame_vram && !A.bidir && A.no_bidir, "preset sets its two flags", "pre A");
    expect(!A.gme_sub2_force, "preset does NOT pin sub2", "pre A");
    expect(!A.layers.on[BIDIR], "registry BIDIR off", "pre A (the layer_shadow_parse alias)");
    expect(A.layers_old.valid && !A.layers_old.bidir, "pre-cascade snapshot", "pre A: layers_old.bidir == false");
    expect(pfg::layers::layer_config_parity(A), "R0 parity oracle", "pre A");
    expect(pfg::layers::layer_config_parity(B), "R0 parity oracle", "pre B");

    // 2. POST-CASCADE
    apply_cascades(A, false); apply_cascades(B, false); apply_cascades(B2, false);
    same_fields(A, B, "post A vs B", true);
    same_fields(A, B2, "post A vs B2", true);
    expect(!A.phase_anchor, "cascade: phase_anchor off", "post A (the no-bidir cascade)");
    pfg::layers::layer_resolve_effective(A.layers, 0u);
    pfg::layers::layer_resolve_effective(B.layers, 0u);
    expect(A.layers.avail == B.layers.avail && A.layers.eff == B.layers.eff, "resolved rows equal", "post A vs B avail/eff");
    expect((A.layers.eff & (1u << BIDIR)) == 0u, "resolved BIDIR clear", "post A eff");
    expect(pfg::layers::layer_contract_hash(A) == pfg::layers::layer_contract_hash(B), "contract hash equal", "post A vs B");

    // 3. THE CASCADE ROUTE
    Config Am = parse({"--eco", "--matte"}, true);
    Config Bm = parse({"--frame-vram", "--no-bidir", "--matte"}, true);
    expect(!Am.matte && !Bm.matte, "cascade: --matte off under no-bidir", "--eco --matte / explicit");
    same_fields(Am, Bm, "matte route", true);

    // 4. SEEN RED
    Config D = parse({}, false);
    expect(D.frame_vram != A.frame_vram && D.bidir != A.bidir, "default differs (not vacuous)", "default vs --eco");
    expect(D.layers.on[BIDIR], "default registry BIDIR on", "default");

    if (g_fail == 0) { std::fprintf(stderr, "OK: all %d checks passed\n", g_checks); return 0; }
    std::fprintf(stderr, "FAILED: %d/%d checks failed\n", g_fail, g_checks);
    return 1;
}
// Made with my soul - Swately <3
