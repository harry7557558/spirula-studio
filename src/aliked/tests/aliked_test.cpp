// ALIKED: the checkpoint reader, and (once it lands) the forward pass.
//
// The reader gate is deliberately strict about *shapes*, not about values: we
// do not own these weights and cannot embed a golden copy of them, so what
// this can check is that the file parses, that every tensor the forward pass
// will ask for exists at the width the rest of the model assumes, and that the
// two released variants agree everywhere except M.
//
// Needs a checkpoint. With none on disk and no --fetch it SKIPS rather than
// fails: a build machine without network access must still be able to run the
// test suite.
//
//   aliked_test                 # use whatever is already cached
//   aliked_test --fetch         # download from COLMAP's releases if missing
//   aliked_test path/to.onnx    # a specific file

#include "aliked/Aliked.h"
#include "aliked/Common.h"
#include "aliked/model/Fetch.h"
#include "aliked/model/LightGlue.h"
#include "aliked/model/Weights.h"
#include "nn/core/Log.h"
#include "nn/io/Image.h"
#include "nn/Tensor.h"
#include "nn/vk/Context.h"
#include "nn/vk/Memory.h"
#include "nn/vk/Pipelines.h"
#include "nn/vk/Stream.h"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>
#include "core/Env.h"

namespace fs = std::filesystem;
using namespace aliked;

namespace {

int g_failures = 0;
int g_checks = 0;

void check(bool ok, const char* fmt, ...) {
    ++g_checks;
    if (ok) return;
    ++g_failures;
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    std::printf("  FAIL %s\n", buf);
}

void expect_shape(const AlikedWeights& w, const char* name,
                  std::vector<int64_t> want) {
    nn::Tensor t;
    try {
        t = w.get(name);
    } catch (const std::exception& e) {
        check(false, "%s: %s", name, e.what());
        return;
    }
    bool ok = t.ndim == (int32_t)want.size();
    for (size_t i = 0; ok && i < want.size(); ++i)
        if (want[i] >= 0 && t.shape[i] != want[i]) ok = false;
    if (!ok) {
        std::string got = "[";
        for (int i = 0; i < t.ndim; ++i)
            got += (i ? ", " : "") + std::to_string(t.shape[i]);
        got += "]";
        std::string ws = "[";
        for (size_t i = 0; i < want.size(); ++i)
            ws += (i ? ", " : "") + (want[i] < 0 ? std::string("*") : std::to_string(want[i]));
        ws += "]";
        check(false, "%s is %s, expected %s", name, got.c_str(), ws.c_str());
    } else {
        ++g_checks;
    }
}

// Everything AlikedModel's forward pass will look up, at the widths it assumes.
void check_checkpoint(const std::string& path) {
    std::printf("\n%s\n", path.c_str());

    // Whenever the file we were handed is recognisably one of the released
    // artifacts, hash it. This is the only place SHA-256 gets exercised on
    // real data -- everywhere else it runs on the download path, where a bug
    // would show up as "the file is always corrupt" long after the fact.
    for (const char* id : {"aliked-n16rot", "aliked-n32", "aliked-lightglue"}) {
        const ModelSource* src = find_model_source(id);
        if (!src || path.find(src->onnx.file) == std::string::npos) continue;
        const std::string got = sha256_file(path);
        check(got == src->onnx.sha256, "%s hashes to %s, expected %s",
              src->onnx.file, got.c_str(), src->onnx.sha256);
        break;
    }

    AlikedWeights w;
    w.load(path);
    const AlikedHparams& h = w.hparams();
    std::printf("  c=(%d,%d,%d,%d) dim=%d dim4=%d K=%d M=%d desc=%d  %.2f MB\n", h.c1,
                h.c2, h.c3, h.c4, h.dim, h.dim4, h.K, h.M, h.desc_dim,
                (double)w.deviceBytes() / 1e6);

    // The published variants: n16 / n16rot / n32 all share these.
    check(h.c1 == 16 && h.c2 == 32 && h.c3 == 64 && h.c4 == 128,
          "unexpected block widths (%d,%d,%d,%d)", h.c1, h.c2, h.c3, h.c4);
    check(h.dim == 128 && h.dim4 == 32, "unexpected aggregated width %d", h.dim);
    check(h.desc_dim == 128, "unexpected descriptor width %d", h.desc_dim);
    check(h.K == 3, "unexpected SDDH patch %d", h.K);
    check(h.M == 16 || h.M == 32, "unexpected SDDH position count %d", h.M);

    // Encoder. block1 is a ConvBlock (no shortcut projection); 2..4 are
    // ResBlocks; 3 and 4 are deformable.
    expect_shape(w, "block1.conv1.weight", {h.c1, 3, 3, 3});
    expect_shape(w, "block1.conv1.bias", {h.c1});
    expect_shape(w, "block1.conv2.weight", {h.c1, h.c1, 3, 3});
    expect_shape(w, "block1.conv2.bias", {h.c1});
    check(!w.has("block1.downsample.weight"),
          "block1 has a downsample; it should be a ConvBlock");

    const int cin[5] = {0, 0, 16, 32, 64};
    const int cout[5] = {0, 0, 32, 64, 128};
    for (int b = 2; b <= 4; ++b) {
        char n[64];
        const bool deform = b >= 3;
        const char* suffix = deform ? ".regular_conv" : "";
        std::snprintf(n, sizeof n, "block%d.conv1%s.weight", b, suffix);
        expect_shape(w, n, {cout[b], cin[b], 3, 3});
        std::snprintf(n, sizeof n, "block%d.conv1%s.bias", b, suffix);
        expect_shape(w, n, {cout[b]});
        std::snprintf(n, sizeof n, "block%d.conv2%s.weight", b, suffix);
        expect_shape(w, n, {cout[b], cout[b], 3, 3});
        std::snprintf(n, sizeof n, "block%d.downsample.weight", b);
        expect_shape(w, n, {cout[b], cin[b], 1, 1});
        std::snprintf(n, sizeof n, "block%d.downsample.bias", b);
        expect_shape(w, n, {cout[b]});
        if (deform) {
            // 2 offsets per tap, no modulation mask -- Weights.cpp refuses the
            // 3*k*k form, and this is the assertion that says why.
            std::snprintf(n, sizeof n, "block%d.conv1.offset_conv.weight", b);
            expect_shape(w, n, {18, cin[b], 3, 3});
            std::snprintf(n, sizeof n, "block%d.conv2.offset_conv.weight", b);
            expect_shape(w, n, {18, cout[b], 3, 3});
        }
    }

    // Aggregation and heads.
    const int ci[5] = {0, 16, 32, 64, 128};
    for (int i = 1; i <= 4; ++i) {
        char n[32];
        std::snprintf(n, sizeof n, "conv%d.weight", i);
        expect_shape(w, n, {h.dim4, ci[i], 1, 1});
    }
    expect_shape(w, "score_head.0.weight", {8, h.dim, 1, 1});
    expect_shape(w, "score_head.2.weight", {4, 8, 3, 3});
    expect_shape(w, "score_head.4.weight", {4, 4, 3, 3});
    expect_shape(w, "score_head.6.weight", {1, 4, 3, 3});

    expect_shape(w, "desc_head.offset_conv.0.weight", {2 * h.M, h.dim, h.K, h.K});
    expect_shape(w, "desc_head.offset_conv.0.bias", {2 * h.M});
    expect_shape(w, "desc_head.offset_conv.2.weight", {2 * h.M, 2 * h.M, 1, 1});
    expect_shape(w, "desc_head.offset_conv.2.bias", {2 * h.M});
    expect_shape(w, "desc_head.sf_conv.weight", {h.desc_dim, h.dim, 1, 1});
    // Stored transposed at load: the aggregation runs as M matmuls on the
    // tuned GEMM, which wants [out_features, in_features].
    expect_shape(w, "desc_head.agg_weights_t", {h.M, h.desc_dim, h.dim});

    // A name that is not there must say so, and say what is nearby.
    bool threw = false;
    try {
        (void)w.get("block1.conv3.weight");
    } catch (const std::exception&) {
        threw = true;
    }
    check(threw, "a missing weight name did not throw");
}

// Kernels index the full-resolution aggregate from one pointer, so a working
// resolution it cannot address is refused rather than detecting in part of the
// frame. Checked at a lowered span, which costs no VRAM.
void check_span_ceiling(const std::string& path) {
    Extractor ex;
    ex.load(path);
    const std::vector<uint8_t> rgb(64 * 64 * 3, 128);
    std::string what;
    nn::set_max_span_bytes(64 * 64 * 128 * 4 - 1);
    nn::set_wide_index(nn::WideIndex::Off);   // as a device without shaderInt64
    try {
        (void)ex.extract(rgb.data(), 64, 64);
    } catch (const std::exception& e) {
        what = e.what();
    }
    nn::set_wide_index(nn::WideIndex::Auto);
    nn::set_max_span_bytes(0);
    check(what.find("--max-image-size") != std::string::npos,
          "an unaddressable working resolution did not name the ceiling: %s",
          what.empty() ? "no error" : what.c_str());
}

// The whole forward pass on the 64-bit-addressing kernels, which a frame past
// 2880 px square would take. Forced at a small size: the keypoints must be the
// ones the narrow kernels find, to the bit.
void check_wide_extraction(const std::string& path) {
    if (!vk::Context::get().hasInt64()) {
        std::printf("  SKIP wide addressing (no shaderInt64)\n");
        return;
    }
    Extractor ex;
    ex.load(path);
    std::vector<uint8_t> rgb(192 * 192 * 3);
    for (size_t i = 0; i < rgb.size(); ++i)
        rgb[i] = (uint8_t)((i * 2654435761u) >> 24);

    ExtractOptions o;
    o.max_num_features = 128;
    o.min_score = 0.0f;
    const Features narrow = ex.extract(rgb.data(), 192, 192, o);
    nn::set_wide_index(nn::WideIndex::Force);
    const Features wide = ex.extract(rgb.data(), 192, 192, o);
    nn::set_wide_index(nn::WideIndex::Auto);

    bool same = narrow.keypoints.size() == wide.keypoints.size() &&
                narrow.descriptors.size() == wide.descriptors.size();
    for (size_t i = 0; same && i < narrow.keypoints.size(); ++i)
        same = narrow.keypoints[i].x == wide.keypoints[i].x &&
               narrow.keypoints[i].y == wide.keypoints[i].y &&
               narrow.keypoints[i].score == wide.keypoints[i].score;
    for (size_t i = 0; same && i < narrow.descriptors.size(); ++i)
        same = narrow.descriptors[i] == wide.descriptors[i];
    check(same && !narrow.keypoints.empty(),
          "the 64-bit kernels return different features (%zu vs %zu keypoints)",
          narrow.keypoints.size(), wide.keypoints.size());
}

// Extract from one image and write the result where a comparison script can
// read it. The format is deliberately trivial -- a header and two float
// blocks -- because its only consumer is tools/aliked/compare_colmap.py, which
// reads COLMAP's SQLite database on the other side.
//
//   char[8] "ALIKEDFT"  u32 version=1  i32 width, height  u32 count, dim
//   count * { f32 x, y, score }
//   count * dim f32 descriptors
void run_extraction(const std::string& model, const std::string& image_path,
                    const std::string& out_path, const ExtractOptions& opts,
                    int max_image_size) {
    nn::Image img = nn::load_image(image_path);
    check(!img.empty(), "cannot read image %s", image_path.c_str());
    if (img.empty()) return;

    // COLMAP downscales to EffMaxImageSize() before extracting -- 1600 for
    // ALIKED. Comparing against its output means doing the same, and doing it
    // the same way: a box-filtered halving is not what it does, but at the
    // integer ratios these tests use the difference is not what is under test.
    if (max_image_size > 0 &&
        std::max(img.width, img.height) > max_image_size) {
        const double s = (double)max_image_size / std::max(img.width, img.height);
        const int nw = std::max(1, (int)(img.width * s));
        const int nh = std::max(1, (int)(img.height * s));
        std::vector<uint8_t> dst((size_t)nw * nh * 3);
        for (int y = 0; y < nh; ++y)
            for (int x = 0; x < nw; ++x) {
                const int sy = std::min(img.height - 1, (int)((y + 0.5) / s));
                const int sx = std::min(img.width - 1, (int)((x + 0.5) / s));
                std::memcpy(&dst[((size_t)y * nw + x) * 3],
                            &img.data[((size_t)sy * img.width + sx) * 3], 3);
            }
        img.data.swap(dst);
        img.width = nw;
        img.height = nh;
    }

    Extractor ex;
    ex.load(model);
    const double t0 = nn::now_ms();
    Features f = ex.extract(img.data.data(), img.width, img.height, opts);
    const double ms = nn::now_ms() - t0;

    double min_s = 1e9, max_s = -1e9, mean_norm = 0;
    for (const Keypoint& k : f.keypoints) {
        min_s = std::min(min_s, (double)k.score);
        max_s = std::max(max_s, (double)k.score);
    }
    for (size_t i = 0; i < f.keypoints.size(); ++i) {
        double sq = 0;
        for (int c = 0; c < f.desc_dim; ++c) {
            const double v = f.descriptors[i * f.desc_dim + c];
            sq += v * v;
        }
        mean_norm += std::sqrt(sq);
    }
    if (!f.keypoints.empty()) mean_norm /= (double)f.keypoints.size();

    std::printf("%s  %dx%d -> %zu keypoints in %.0f ms\n", image_path.c_str(), img.width,
                img.height, f.keypoints.size(), ms);
    std::printf("  score %.3f .. %.3f, mean |descriptor| %.5f\n", min_s, max_s, mean_norm);

    // Descriptors are L2-normalized by construction; if they are not, nothing
    // downstream that treats a dot product as a cosine is meaningful.
    check(f.keypoints.empty() || std::fabs(mean_norm - 1.0) < 1e-3,
          "descriptors are not unit norm (mean %.6f)", mean_norm);
    check(min_s >= opts.min_score || f.keypoints.empty(),
          "a keypoint scored %.4f, below min_score %.4f", min_s, opts.min_score);

    if (out_path.empty()) return;
    std::FILE* fp = std::fopen(out_path.c_str(), "wb");
    check(fp != nullptr, "cannot write %s", out_path.c_str());
    if (!fp) return;
    const uint32_t version = 1, count = (uint32_t)f.keypoints.size(),
                   dim = (uint32_t)f.desc_dim;
    std::fwrite("ALIKEDFT", 1, 8, fp);
    std::fwrite(&version, 4, 1, fp);
    std::fwrite(&f.width, 4, 1, fp);
    std::fwrite(&f.height, 4, 1, fp);
    std::fwrite(&count, 4, 1, fp);
    std::fwrite(&dim, 4, 1, fp);
    for (const Keypoint& k : f.keypoints) {
        const float v[3] = {k.x, k.y, k.score};
        std::fwrite(v, 4, 3, fp);
    }
    std::fwrite(f.descriptors.data(), 4, f.descriptors.size(), fp);
    std::fclose(fp);
    std::printf("  wrote %s\n", out_path.c_str());
}

// Match two dumps written by --image/--out, and append the correspondences to
// a third file so tools/aliked/compare_colmap.py can score them against ORT.
//
//   u32 count, then count * { u32 i, u32 j, f32 score }
void run_matching(const std::string& model, const std::string& a_path,
                  const std::string& b_path, const std::string& out_path,
                  float min_score) {
    struct Dump {
        int w = 0, h = 0, dim = 0;
        std::vector<float> xy, score, desc;
    };
    auto read = [&](const std::string& p) {
        Dump d;
        std::FILE* f = std::fopen(p.c_str(), "rb");
        check(f != nullptr, "cannot read %s", p.c_str());
        if (!f) return d;
        bool ok = true;
        auto rd = [&](void* dst, size_t sz, size_t n) {
            ok = ok && std::fread(dst, sz, n, f) == n;
        };
        char magic[8];
        uint32_t version = 0, count = 0, dim = 0;
        rd(magic, 1, 8);
        rd(&version, 4, 1);
        rd(&d.w, 4, 1);
        rd(&d.h, 4, 1);
        rd(&count, 4, 1);
        rd(&dim, 4, 1);
        d.dim = (int)dim;
        d.xy.resize((size_t)count * 2);
        d.score.resize(count);
        for (uint32_t i = 0; i < count && ok; i++) {
            float v[3];
            rd(v, 4, 3);
            d.xy[(size_t)i * 2] = v[0];
            d.xy[(size_t)i * 2 + 1] = v[1];
            d.score[i] = v[2];
        }
        d.desc.resize((size_t)count * dim);
        if (!d.desc.empty()) rd(d.desc.data(), 4, d.desc.size());
        std::fclose(f);
        check(ok, "truncated dump %s", p.c_str());
        if (!ok) return Dump{};
        return d;
    };

    const Dump A = read(a_path), B = read(b_path);
    if (A.desc.empty() || B.desc.empty()) return;

    Matcher lg;
    lg.load(model);
    aliked::MatchInput ia, ib;
    ia.keypoints = A.xy.data();
    ia.descriptors = A.desc.data();
    ia.n = (uint32_t)(A.xy.size() / 2);
    ia.width = A.w;
    ia.height = A.h;
    ib.keypoints = B.xy.data();
    ib.descriptors = B.desc.data();
    ib.n = (uint32_t)(B.xy.size() / 2);
    ib.width = B.w;
    ib.height = B.h;

    aliked::MatchOptions mo;
    mo.min_score = min_score;
    const double t0 = nn::now_ms();
    std::vector<aliked::Match> m = lg.match(ia, ib, mo);
    const double ms = nn::now_ms() - t0;
    std::printf("lightglue: %u x %u -> %zu matches in %.0f ms\n", ia.n, ib.n, m.size(),
                ms);
    check(!m.empty(), "LightGlue returned no matches at all");

    // Every index in range, every pair used once on each side: a mutual-nearest
    // assignment cannot repeat either.
    std::vector<char> seen_i(ia.n, 0), seen_j(ib.n, 0);
    bool ok = true;
    for (const aliked::Match& x : m) {
        if (x.i >= ia.n || x.j >= ib.n) { ok = false; break; }
        if (seen_i[x.i] || seen_j[x.j]) { ok = false; break; }
        seen_i[x.i] = seen_j[x.j] = 1;
        if (!(x.score >= min_score && x.score <= 1.0f + 1e-3f)) { ok = false; break; }
    }
    check(ok, "LightGlue produced a duplicate, out-of-range or invalid-score match");

    if (out_path.empty()) return;
    std::FILE* f = std::fopen(out_path.c_str(), "wb");
    check(f != nullptr, "cannot write %s", out_path.c_str());
    if (!f) return;
    const uint32_t n = (uint32_t)m.size();
    std::fwrite(&n, 4, 1, f);
    for (const aliked::Match& x : m) {
        std::fwrite(&x.i, 4, 1, f);
        std::fwrite(&x.j, 4, 1, f);
        std::fwrite(&x.score, 4, 1, f);
    }
    std::fclose(f);
    std::printf("  wrote %s\n", out_path.c_str());
}

}  // namespace

int main(int argc, char** argv) {
    // Stage timings by default, but let $SS_NN_LOG win -- the tensor dumps
    // in the model layer are how a mismatch against the reference is bisected,
    // and forcing the level here would hide them.
    if (!spirula::env("NN_LOG")) nn::set_log_level(2);

    bool fetch = false;
    std::string image_path, out_path, model = "aliked-n16rot";
    int max_image_size = 1600;
    std::string match_a, match_b, lg_model = "aliked-lightglue";
    ExtractOptions opts;
    std::vector<std::string> explicit_paths;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&]() { return (i + 1 < argc) ? std::string(argv[++i]) : std::string(); };
        if (a == "--fetch") fetch = true;
        else if (a == "--image") image_path = next();
        else if (a == "--out") out_path = next();
        else if (a == "--model") model = next();
        else if (a == "--max-image-size") max_image_size = std::atoi(next().c_str());
        else if (a == "--max-features") opts.max_num_features = std::atoi(next().c_str());
        else if (a == "--min-score") opts.min_score = (float)std::atof(next().c_str());
        else if (a == "--match") { match_a = next(); match_b = next(); }
        else if (a == "--lightglue-model") lg_model = next();
        else explicit_paths.push_back(a);
    }

    try {
        vk::Context::get();

        std::vector<std::string> paths = explicit_paths;
        if (paths.empty()) {
            for (const char* id : {"aliked-n16rot", "aliked-n32"}) {
                const ModelSource* src = find_model_source(id);
                if (!src) continue;
                if (fetch) {
                    paths.push_back(ensure_model(*src));
                } else {
                    const std::string p = model_cache_path(*src);
                    std::error_code ec;
                    if (fs::exists(p, ec)) paths.push_back(p);
                }
            }
        }

        if (paths.empty()) {
            std::printf("SKIP: no ALIKED checkpoint cached. Run with --fetch to "
                        "download from COLMAP's releases.\n");
            vk::Stream::shutdown();
            vk::Pipelines::get().shutdown();
            vk::VramPool::get().releaseAll();
            return 0;
        }

        for (const std::string& p : paths) check_checkpoint(p);
        check_span_ceiling(paths[0]);
        check_wide_extraction(paths[0]);
        if (!match_a.empty()) {
            std::printf("\nMatching\n");
            run_matching(lg_model, match_a, match_b, out_path, 0.1f);
        }
        if (!image_path.empty()) {
            std::printf("\nExtraction\n");
            run_extraction(paths.empty() ? model : paths[0], image_path, out_path, opts,
                           max_image_size);
        }
        std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
    } catch (const std::exception& e) {
        std::printf("EXCEPTION: %s\n", e.what());
        return 1;
    }

    vk::Stream::shutdown();
    vk::Pipelines::get().shutdown();
    vk::VramPool::get().releaseAll();
    std::printf("%s\n", g_failures == 0 ? "PASS" : "FAIL");
    return g_failures == 0 ? 0 : 1;
}
