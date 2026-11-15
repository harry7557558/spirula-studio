// FlexGlue against the reference pairs `flexglue_ref.py fixtures` writes to
// tools/flexglue_ref/fixtures (PyTorch, fp32, COLMAP's configuration; README.md
// there has the format and the tolerance policy, whose fixed bars are used here).
//
//   flexglue_test [--fixtures ROOT] [--pair NAME] [--checkpoint PATH] [--bench N] [--require-model] [--warm-cache]
//
// Each pair runs twice, as the reference did: `nostop` (min covisibility 0, so
// the readout always runs) and `colmap` (COLMAP's defaults, early stops on).
// With SS_FLEXGLUE_DUMP=<dir> and one --pair, every stage is written there for
// tools/flexglue_ref/compare.py, which also applies the torch-vs-torch floors.
// No checkpoint, fixtures or images SKIPS (fails under --require-model);
// --warm-cache makes the dumped run take the layer-1 self-cache hit path.

#include "flexglue/FlexGlue.h"
#include "flexglue/model/Fetch.h"
#include "nn/io/Fetch.h"
#include "nn/io/Image.h"

#include "core/Env.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

int g_failures = 0;
int g_checks = 0;
// Pairs where the final covisibility gate removes matches nostop keeps: the
// count that says the gate check below could fail at all.
int g_gate_bites = 0;

void check(bool ok, const char* fmt, ...) {
    ++g_checks;
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    std::printf("  %s %s\n", ok ? "ok  " : "FAIL", buf);
    if (!ok) ++g_failures;
}

// A little-endian .npy of f4, f8, i4, i8 or u1, as doubles.
bool read_npy(const fs::path& p, std::vector<double>& v, std::vector<int64_t>* shape = nullptr) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return false;
    char magic[8];
    f.read(magic, 8);
    if (std::memcmp(magic, "\x93NUMPY", 6) != 0) return false;
    uint32_t hl = 0;
    if (magic[6] == 1) {
        uint16_t h = 0;
        f.read((char*)&h, 2);
        hl = h;
    } else {
        f.read((char*)&hl, 4);
    }
    std::string hdr(hl, ' ');
    f.read(&hdr[0], hl);
    const size_t d = hdr.find("'descr': '");
    if (d == std::string::npos || hdr.find("'fortran_order': False") == std::string::npos)
        return false;
    const std::string descr = hdr.substr(d + 10, 3);
    std::vector<int64_t> s;
    const size_t a = hdr.find('(', hdr.find("'shape'")), b = hdr.find(')', a);
    std::stringstream ss(hdr.substr(a + 1, b - a - 1));
    for (std::string tok; std::getline(ss, tok, ',');)
        if (tok.find_first_not_of(' ') != std::string::npos) s.push_back(std::stoll(tok));
    int64_t n = 1;
    for (int64_t x : s) n *= x;
    v.resize((size_t)n);
    auto take = [&](auto tag) {
        using T = decltype(tag);
        std::vector<T> raw((size_t)n);
        f.read((char*)raw.data(), (std::streamsize)(n * sizeof(T)));
        for (int64_t i = 0; i < n; ++i) v[(size_t)i] = (double)raw[(size_t)i];
    };
    if (descr == "<f4") take(float{});
    else if (descr == "<f8") take(double{});
    else if (descr == "<i4") take(int32_t{});
    else if (descr == "<i8") take(int64_t{});
    else if (descr == "|u1") take(uint8_t{});
    else return false;
    if (shape) *shape = s;
    return (bool)f;
}

std::vector<std::string> json_strings(const std::string& text, const char* key) {
    std::vector<std::string> out;
    size_t k = text.find(std::string("\"") + key + "\"");
    if (k == std::string::npos) return out;
    const size_t a = text.find('[', k), b = text.find(']', a);
    for (size_t q = text.find('"', a); q < b; q = text.find('"', q + 1)) {
        const size_t e = text.find('"', q + 1);
        out.push_back(text.substr(q + 1, e - q - 1));
        q = e;
    }
    return out;
}

double rel_l2(const std::vector<double>& got, const std::vector<double>& want) {
    double e = 0, n = 0;
    for (size_t i = 0; i < want.size(); ++i) {
        e += (got[i] - want[i]) * (got[i] - want[i]);
        n += want[i] * want[i];
    }
    return std::sqrt(e / std::max(n, 1e-30));
}

double now_s() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

std::vector<double> to_d(const std::vector<float>& v) { return {v.begin(), v.end()}; }

// The fraction of entries that differ, and the counts of non-negative entries.
double frac_diff(const std::vector<double>& a, const std::vector<double>& b) {
    size_t d = 0;
    for (size_t i = 0; i < a.size(); ++i) d += a[i] != b[i];
    return (double)d / (double)std::max<size_t>(a.size(), 1);
}

size_t count_matched(const std::vector<double>& m) {
    return (size_t)std::count_if(m.begin(), m.end(), [](double x) { return x >= 0; });
}

bool load_fixture_image(const fs::path& root, const std::string& stem, nn::Image& im) {
    const fs::path id = root / "images" / stem;
    std::vector<double> u8;
    std::vector<int64_t> ushape;
    if (fs::exists(id / "image.png")) {
        im = nn::load_image((id / "image.png").string());
    } else if (read_npy(id / "u8.npy", u8, &ushape) && ushape.size() == 3 && ushape[2] == 3) {
        im.height = (int)ushape[0];   // the full tier's decoded RGB
        im.width = (int)ushape[1];
        im.data.assign(u8.begin(), u8.end());
    }
    return !im.empty();
}

bool same_bits(const std::vector<float>& a, const std::vector<float>& b) {
    return a.size() == b.size() && std::memcmp(a.data(), b.data(), a.size() * 4) == 0;
}

bool same_result(const flexglue::MatchResult& a, const flexglue::MatchResult& b) {
    return a.matches0 == b.matches0 && same_bits(a.scores0, b.scores0) &&
           std::memcmp(&a.covisibility, &b.covisibility, 4) == 0 &&
           a.stop_layer == b.stop_layer && same_bits(a.p2d[0], b.p2d[0]) &&
           same_bits(a.p2d[1], b.p2d[1]) && same_bits(a.conf[0], b.conf[0]) &&
           same_bits(a.conf[1], b.conf[1]);
}

// Three images in three pairs, each image in two of them and in both slots:
// cached output must equal uncached bit for bit, and the cache must be hit.
void test_self_cache(flexglue::Model& model, const fs::path& root) {
    std::printf("\nself-block cache: 3 fixture images in 3 pairs\n");
    const char* stems[3] = {"berlin1", "berlin2", "lamar_cab_1446"};
    std::shared_ptr<flexglue::DenseFeatures> f[3];
    for (int i = 0; i < 3; ++i) {
        nn::Image im;
        if (!load_fixture_image(root, stems[i], im)) {
            check(false, "self cache: cannot read %s", stems[i]);
            return;
        }
        model.setDump("cache" + std::to_string(i) + ".", "", false);
        f[i] = model.describe(im.data.data(), im.width, im.height);
    }
    // (image a, its kpts file), (image b, its kpts file)
    struct Side {
        int img;
        const char* pair;
        int slot;
    };
    const Side pairs[3][2] = {{{0, "berlin", 0}, {1, "berlin", 1}},
                              {{0, "berlin_x_lamar", 0}, {2, "berlin_x_lamar", 1}},
                              {{2, "berlin_x_lamar", 1}, {1, "berlin", 1}}};
    std::vector<float> kp[3][2];
    flexglue::Keypoints k[3][2];
    for (int p = 0; p < 3; ++p)
        for (int s = 0; s < 2; ++s) {
            const Side& sd = pairs[p][s];
            std::vector<double> v;
            if (!read_npy(root / "pairs" / sd.pair / ("kpts" + std::to_string(sd.slot) + ".npy"),
                          v) || v.empty()) {
                check(false, "self cache: no kpts in %s", sd.pair);
                return;
            }
            kp[p][s].assign(v.begin(), v.end());
            k[p][s].xy = kp[p][s].data();
            k[p][s].n = (uint32_t)(kp[p][s].size() / 2);
            k[p][s].width = k[p][s].height = 512;
        }
    flexglue::MatchOptions o;
    o.min_covisibility = 0.0f;
    o.keep_correspondences = true;
    model.setDump("", "", false);
    auto run = [&](int p) {
        return model.match(*f[pairs[p][0].img], k[p][0], *f[pairs[p][1].img], k[p][1], o);
    };
    model.setSelfCache(0);
    flexglue::MatchResult ref[3];
    for (int p = 0; p < 3; ++p) ref[p] = run(p);
    check(model.selfCacheStats().entries == 0, "cache off holds nothing");

    model.setSelfCache(64);
    const flexglue::Model::SelfCacheStats s0 = model.selfCacheStats();
    for (int p = 0; p < 3; ++p) {
        const flexglue::MatchResult r = run(p);
        check(same_result(r, ref[p]), "pair %d (%s x %s): cached == uncached, bit for bit, %zu "
              "matches", p, stems[pairs[p][0].img], stems[pairs[p][1].img],
              (size_t)std::count_if(r.matches0.begin(), r.matches0.end(),
                                    [](int32_t m) { return m >= 0; }));
    }
    const flexglue::Model::SelfCacheStats s1 = model.selfCacheStats();
    check(s1.hits - s0.hits == 3 && s1.misses - s0.misses == 3 && s1.entries == 3,
          "3 hits, 3 misses, 3 entries (got %llu, %llu, %d)",
          (unsigned long long)(s1.hits - s0.hits), (unsigned long long)(s1.misses - s0.misses),
          s1.entries);

    model.setSelfCache(1);
    const flexglue::Model::SelfCacheStats s2 = model.selfCacheStats();
    check(s2.entries == 1 && s2.evictions - s1.evictions == 2,
          "capacity 1 evicts down to one entry (%d entries, %llu evicted)", s2.entries,
          (unsigned long long)(s2.evictions - s1.evictions));
    // pair 2 used berlin2 last, so it is the survivor: pair 0 hits once.
    const flexglue::MatchResult r0 = run(0);
    const flexglue::Model::SelfCacheStats s3 = model.selfCacheStats();
    check(same_result(r0, ref[0]) && s3.hits - s2.hits == 1 && s3.misses - s2.misses == 1 &&
              s3.entries == 1,
          "capacity 1: LRU survivor hits, output unchanged (%llu hit, %d entries)",
          (unsigned long long)(s3.hits - s2.hits), s3.entries);
    f[0].reset();
    check(model.selfCacheStats().entries == 0, "an image's entry dies with its features");
    model.setSelfCache(64);
}

void run_pair(flexglue::Model& model, const fs::path& root, const std::string& pair, int bench,
              bool warm) {
    const fs::path pdir = root / "pairs" / pair;
    std::ifstream mf(pdir / "manifest.json");
    std::stringstream ms;
    ms << mf.rdbuf();
    const std::vector<std::string> stems = json_strings(ms.str(), "images");
    std::printf("\n%s (%s, %s)\n", pair.c_str(), stems.size() > 0 ? stems[0].c_str() : "?",
                stems.size() > 1 ? stems[1].c_str() : "?");
    if (stems.size() != 2) {
        check(false, "%s: the manifest names %zu images", pair.c_str(), stems.size());
        return;
    }

    std::shared_ptr<flexglue::DenseFeatures> f[2];
    std::vector<float> kp[2];
    for (int s = 0; s < 2; ++s) {
        const fs::path id = root / "images" / stems[s];
        nn::Image im;
        if (!load_fixture_image(root, stems[s], im)) {
            check(false, "%s: cannot read %s/image.png", pair.c_str(), stems[s].c_str());
            return;
        }
        model.setDump("img" + std::to_string(s) + ".");
        const double t0 = now_s();
        f[s] = model.describe(im.data.data(), im.width, im.height);
        const double dt = now_s() - t0;
        std::vector<double> idx, val;
        if (read_npy(id / "features.npy", val)) {   // the full tier
            idx.resize(val.size());
            for (size_t i = 0; i < idx.size(); ++i) idx[i] = (double)i;
        }
        if ((!idx.empty() || (read_npy(id / "features.probe_idx.npy", idx) &&
                              read_npy(id / "features.probe_val.npy", val))) &&
            idx.size() == val.size()) {
            const std::vector<float> got = model.download(*f[s]);
            std::vector<double> ours(idx.size());
            for (size_t i = 0; i < idx.size(); ++i) ours[i] = got[(size_t)idx[i]];
            const double e = rel_l2(ours, val);
            check(e < 1e-4, "img%d features rel L2 %.3g on %zu probes (bar 1e-4), %.0f ms", s, e,
                  idx.size(), dt * 1e3);
        } else {
            check(false, "%s: no features probes for %s", pair.c_str(), stems[s].c_str());
        }
        std::vector<double> k;
        if (!read_npy(pdir / ("kpts" + std::to_string(s) + ".npy"), k) || k.empty()) {
            check(false, "%s: no kpts%d.npy", pair.c_str(), s);
            return;
        }
        kp[s].assign(k.begin(), k.end());
    }
    // kpts{s} are in the 512 frame already, so the frame is 512 x 512 and the
    // rescale to it is exact.
    flexglue::Keypoints k[2];
    for (int s = 0; s < 2; ++s) {
        k[s].xy = kp[s].data();
        k[s].n = (uint32_t)(kp[s].size() / 2);
        k[s].width = k[s].height = 512;
    }

    // ---- nostop: the readout always runs ----
    flexglue::MatchOptions nostop;
    nostop.min_covisibility = 0.0f;
    nostop.keep_correspondences = true;
    if (warm) {
        model.setDump("", "warm.", false);
        model.match(*f[0], k[0], *f[1], k[1], nostop);
    }
    model.setDump("", "nostop.");
    double t0 = now_s();
    const flexglue::MatchResult r = model.match(*f[0], k[0], *f[1], k[1], nostop);
    const double t_nostop = now_s() - t0;
    std::vector<double> want, want2;
    for (int s = 0; s < 2; ++s) {
        if (read_npy(pdir / ("p2d" + std::to_string(s) + ".npy"), want) &&
            want.size() == r.p2d[s].size()) {
            std::vector<double> err;
            for (size_t i = 0; i < want.size(); i += 2)
                err.push_back(256.0 * std::hypot(r.p2d[s][i] - want[i],
                                                 r.p2d[s][i + 1] - want[i + 1]));
            std::sort(err.begin(), err.end());
            const double p50 = err[err.size() / 2], p99 = err[(size_t)(0.99 * (err.size() - 1))];
            check(p50 <= 0.01 && p99 <= 0.1,
                  "p2d%d EPE p50 %.3g px, p99 %.3g px, max %.3g px (bars 0.01 / 0.1)", s, p50,
                  p99, err.back());
        } else {
            check(false, "%s: p2d%d missing or a different size", pair.c_str(), s);
        }
        if (read_npy(pdir / ("conf" + std::to_string(s) + ".npy"), want) &&
            want.size() == r.conf[s].size()) {
            const double e = rel_l2(to_d(r.conf[s]), want);
            check(e <= 1e-3, "conf%d rel L2 %.3g (bar 1e-3)", s, e);
        } else {
            check(false, "%s: conf%d missing or a different size", pair.c_str(), s);
        }
    }
    if (read_npy(pdir / "cov_layer9.npy", want) && want.size() == 1)
        check(std::fabs(r.covisibility - want[0]) <= 1e-4, "covisibility %.6f vs %.6f (bar 1e-4)",
              r.covisibility, want[0]);
    if (read_npy(pdir / "nostop.matches0.npy", want) && want.size() == r.matches0.size() &&
        read_npy(pdir / "nostop.mscores0.npy", want2)) {
        const std::vector<double> ours(r.matches0.begin(), r.matches0.end());
        check(frac_diff(ours, want) <= 0.002,
              "nostop matches0: %zu ours, %zu reference, %.3f%% of entries differ (bar 0.2%%)",
              count_matched(ours), count_matched(want), 100 * frac_diff(ours, want));
        const double e = rel_l2(to_d(r.scores0), want2);
        check(e <= 1e-3, "nostop mscores0 rel L2 %.3g (bar 1e-3)", e);
    } else {
        check(false, "%s: nostop.matches0 missing or a different size", pair.c_str());
    }

    // ---- COLMAP's gate: its defaults, and decay 0.2, which separates checking
    // after layers 2/4/6/8 from checking after every layer ----
    flexglue::MatchResult c;
    double t_colmap = 0;
    for (const float decay : {0.5f, 0.2f}) {
        const std::string tag = decay == 0.5f ? "colmap." : "colmap_decay02.";
        flexglue::MatchOptions o;
        o.early_stop_decay = decay;
        model.setDump("", tag, false);
        t0 = now_s();
        const flexglue::MatchResult g = model.match(*f[0], k[0], *f[1], k[1], o);
        if (decay == 0.5f) {
            t_colmap = now_s() - t0;
            c = g;
        }
        const char* t = tag.c_str();
        if (read_npy(pdir / (tag + "stop_layer.npy"), want) && want.size() == 1)
            check(g.stop_layer == (int)want[0], "%sstop_layer %d vs %d", t, g.stop_layer,
                  (int)want[0]);
        else
            check(false, "%s: %sstop_layer missing", pair.c_str(), t);
        if (read_npy(pdir / (tag + "covisibility_score.npy"), want) && want.size() == 1)
            check(std::fabs(g.covisibility - want[0]) <= 1e-4,
                  "%scovisibility %.6f vs %.6f (bar 1e-4)", t, g.covisibility, want[0]);
        if (read_npy(pdir / (tag + "matches0.npy"), want) && want.size() == g.matches0.size()) {
            const std::vector<double> ours(g.matches0.begin(), g.matches0.end());
            check(frac_diff(ours, want) <= 0.002,
                  "%smatches0: %zu ours, %zu reference, %.3f%% of entries differ (bar 0.2%%)", t,
                  count_matched(ours), count_matched(want), 100 * frac_diff(ours, want));
        } else {
            check(false, "%s: %smatches0 missing or a different size", pair.c_str(), t);
        }
    }
    // ---- the final gate alone: no early stop, so every pair reaches it ----
    {
        flexglue::MatchOptions o;
        o.early_stop_decay = 1.0f;
        model.setDump("", "", false);
        const flexglue::MatchResult g = model.match(*f[0], k[0], *f[1], k[1], o);
        const bool pass = r.covisibility >= o.min_covisibility;
        std::vector<int32_t> expect = r.matches0;
        if (!pass) std::fill(expect.begin(), expect.end(), -1);
        if (!pass && count_matched({r.matches0.begin(), r.matches0.end()}) > 0) ++g_gate_bites;
        check(g.stop_layer == 9 && g.matches0 == expect,
              "final gate: covisibility %.4f %s 0.5, %zu matches (nostop %zu)", g.covisibility,
              pass ? ">=" : "<", count_matched({g.matches0.begin(), g.matches0.end()}),
              count_matched({r.matches0.begin(), r.matches0.end()}));
    }
    std::printf("  match %u x %u: nostop %.0f ms, colmap %.0f ms (stop layer %d)\n", k[0].n,
                k[1].n, t_nostop * 1e3, t_colmap * 1e3, c.stop_layer);

    if (bench > 0) {
        // Both images hit the self cache after the first run; capacity 0 is
        // the uncached arm. Interleaved so drift lands on both.
        const int cap = model.selfCacheStats().capacity;
        double tm[2] = {0, 0};
        for (int it = 0; it < bench; ++it)
            for (int arm = 0; arm < 2; ++arm) {
                model.setSelfCache(arm == 0 ? cap : 0);
                if (arm == 0) model.match(*f[0], k[0], *f[1], k[1], {});
                t0 = now_s();
                model.match(*f[0], k[0], *f[1], k[1], {});
                tm[arm] += now_s() - t0;
            }
        model.setSelfCache(cap);
        std::printf("  bench: %.1f ms/pair self-cached (capacity %d), %.1f ms/pair uncached, "
                    "over %d runs, COLMAP settings\n",
                    tm[0] / bench * 1e3, cap, tm[1] / bench * 1e3, bench);
    }
}

}  // namespace

int main(int argc, char** argv) {
    std::string root = std::string(SS_REPO_ROOT) + "/tools/flexglue_ref/fixtures";
    std::string only, ckpt;
    int bench = 0;
    bool require_model = false, warm = false;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--fixtures" && i + 1 < argc) root = argv[++i];
        else if (a == "--pair" && i + 1 < argc) only = argv[++i];
        else if (a == "--checkpoint" && i + 1 < argc) ckpt = argv[++i];
        else if (a == "--bench" && i + 1 < argc) bench = std::atoi(argv[++i]);
        else if (a == "--require-model") require_model = true;
        else if (a == "--warm-cache") warm = true;
        else {
            std::fprintf(stderr,
                         "usage: %s [--fixtures ROOT] [--pair NAME] [--checkpoint PATH] "
                         "[--bench N] [--require-model] [--warm-cache]\n",
                         argv[0]);
            return 2;
        }
    }
    if (ckpt.empty()) ckpt = nn::cached_path(flexglue::kCheckpoint);
    if (!fs::exists(ckpt)) {
        std::printf("SKIP: no checkpoint at %s\n", ckpt.c_str());
        if (!require_model) return 0;
        std::printf("FAIL --require-model and no checkpoint\n");
        return 1;
    }
    check(nn::sha256_file(ckpt) == flexglue::kCheckpoint.sha256,
          "checkpoint SHA-256 matches the pinned release");
    // Neither is committed: tools/flexglue_ref/README.md, "Tiers" and "Images".
    if (!fs::exists(fs::path(root) / "manifest.json")) {
        std::printf("%s: no reference fixtures under %s; "
                    "run uv run tools/flexglue_ref/flexglue_ref.py fixtures\n",
                    require_model ? "FAIL" : "SKIP", root.c_str());
        return require_model ? 1 : 0;
    }
    if (!fs::exists(fs::path(root) / "images" / "berlin1" / "image.png")) {
        std::printf("%s: no fixture images under %s/images; "
                    "run uv run tools/flexglue_ref/fetch_images.py\n",
                    require_model ? "FAIL" : "SKIP", root.c_str());
        return require_model ? 1 : 0;
    }
    const char* dump = spirula::env("FLEXGLUE_DUMP");
    const bool dumping = dump && *dump;
    if (dumping && only.empty()) {
        std::fprintf(stderr, "SS_FLEXGLUE_DUMP needs one --pair\n");
        return 2;
    }

    std::vector<std::string> pairs;
    std::error_code ec;
    for (const auto& e : fs::directory_iterator(fs::path(root) / "pairs", ec))
        if (e.is_directory() && (only.empty() || e.path().filename() == only))
            pairs.push_back(e.path().filename().string());
    std::sort(pairs.begin(), pairs.end());
    check(!pairs.empty(), "fixture pairs under %s", root.c_str());

    // This tests the model, not the consent flow (flexglue_license_test does);
    // a checkpoint on disk was accepted when it was fetched.
    nn::set_license_gate([](const char*) {});
    flexglue::Model model;
    model.load(ckpt);
    if (only.empty()) test_self_cache(model, root);
    for (const std::string& p : pairs) run_pair(model, root, p, bench, warm);
    {
        const flexglue::Model::SelfCacheStats st = model.selfCacheStats();
        std::printf("\nself cache: %llu hits, %llu misses, %llu evictions, %d of %d entries\n",
                    (unsigned long long)st.hits, (unsigned long long)st.misses,
                    (unsigned long long)st.evictions, st.entries, st.capacity);
    }

    if (only.empty())
        check(g_gate_bites > 0,
              "%d pair(s) where the covisibility gate removes matches (the gate check can fail)",
              g_gate_bites);
    if (dumping) {
        std::ofstream m(fs::path(dump) / "manifest.json");
        m << "{\"finished\": true, \"exit_status\": " << (g_failures ? 1 : 0) << ", \"files\": [";
        bool first = true;
        for (const auto& e : fs::directory_iterator(dump, ec))
            if (e.path().extension() == ".npy") {
                m << (first ? "" : ", ") << '"' << e.path().stem().string() << '"';
                first = false;
            }
        m << "]}\n";
    }
    std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
