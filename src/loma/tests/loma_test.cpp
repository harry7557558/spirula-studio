// LoMa: the checkpoint readers, the two forward passes and the matcher.
//
// The reader gate is strict about SHAPES, not values: we do not own these
// weights and cannot embed a golden copy, so what it checks is that the file
// parses and that every tensor the forward pass asks for exists at the width
// the rest of the model assumes. The gate that matters is parity against
// onnxruntime on the same bytes -- tools/loma/compare_ort.py.
//
// With no checkpoint on disk and no --fetch it SKIPS rather than fails: a
// build machine without network access must still run the suite.

#include "loma/Loma.h"
#include "loma/Common.h"
#include "loma/model/Fetch.h"
#include "loma/model/Weights.h"
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
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include "core/Env.h"

namespace fs = std::filesystem;
using namespace loma;

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

// Where a cached checkpoint is, or "" when it is absent and --fetch was not
// given. An explicit path is used as-is.
std::string locate(const char* id, bool fetch) {
    const ModelSource* src = find_model_source(id);
    if (!src) return "";
    if (fetch) return ensure_model(*src);
    const std::string p = model_cache_path(*src);
    std::error_code ec;
    return fs::exists(p, ec) ? p : std::string();
}

// Every shape the pyramid's forward pass derives, checked against what the
// cascade needs: each level's input is its own tap plus the level above's
// context, and every level implies the same head width.
void check_pyramid(const std::string& path, const char* prefix) {
    std::printf("\n%s (%s)\n", path.c_str(), prefix);
    PyramidWeights w;
    try {
        w.load(path, prefix);
    } catch (const std::exception& e) {
        check(false, "load: %s", e.what());
        return;
    }
    const PyramidHparams& hp = w.hparams();
    check(hp.tap_at.size() == hp.refiners.size() ||
              hp.tap_at.size() + 1 == hp.refiners.size(),
          "%zu taps for %zu refiners", hp.tap_at.size(), hp.refiners.size());
    check(hp.head > 0, "head width is %lld", (long long)hp.head);

    const size_t n = hp.refiners.size();
    for (size_t i = 0; i < n; ++i) {
        const RefinerSpec& r = hp.refiners[i];
        check(r.blocks > 0 && r.hidden > 0 && r.in > 0,
              "level %d is in=%lld hidden=%lld blocks=%d", r.scale, (long long)r.in,
              (long long)r.hidden, r.blocks);
        check(r.out == hp.head + r.ctx, "level %d emits %lld, expected head %lld + ctx %lld",
              r.scale, (long long)r.out, (long long)hp.head, (long long)r.ctx);
        if (i + 1 < n)
            check(hp.refiners[i + 1].in == hp.tap_width[n - 2 - i] + r.ctx,
                  "level %d takes %lld, expected tap %lld + ctx %lld",
                  hp.refiners[i + 1].scale, (long long)hp.refiners[i + 1].in,
                  (long long)hp.tap_width[n - 2 - i], (long long)r.ctx);
        // Every weight the refiner will look up, at the width it assumes.
        char q[96];
        std::snprintf(q, sizeof q, "%s.decoder.layers.%d.", prefix, r.scale);
        const std::string b = q;
        try {
            check(w.get(b + "block1.0.weight").shape[0] == r.hidden, "%sblock1.0", b.c_str());
            check(w.get(b + "out_conv.bias").shape[0] == r.out, "%sout_conv", b.c_str());
            for (int k = 0; k < r.blocks; ++k) {
                const std::string h = b + "hidden_blocks." + std::to_string(k) + ".";
                check(w.get(h + "0.weight").shape[0] == r.hidden, "%s0", h.c_str());
                check(w.get(h + "3.weight").shape[1] == r.hidden, "%s3", h.c_str());
            }
        } catch (const std::exception& e) {
            check(false, "%s", e.what());
        }
    }
    std::printf("  %zu convs, %zu taps, %zu levels, head %lld, input %lld\n",
                hp.conv_index.size(), hp.tap_width.size(), n, (long long)hp.head,
                (long long)hp.input_size);
}

void check_matcher(const std::string& path) {
    std::printf("\n%s (matcher)\n", path.c_str());
    MatcherWeights w;
    try {
        w.load(path);
    } catch (const std::exception& e) {
        check(false, "load: %s", e.what());
        return;
    }
    const MatcherHparams& hp = w.hparams();
    check(hp.n_layers == 9, "%d layers, expected 9", hp.n_layers);
    check(hp.embed_dim == 64 * hp.n_heads, "embed %lld for %d heads",
          (long long)hp.embed_dim, hp.n_heads);
    check(hp.input_dim == 128 || hp.input_dim == 256, "input dim %lld",
          (long long)hp.input_dim);
    check(hp.assign_layer == hp.n_layers - 1, "assignment on layer %d of %d",
          hp.assign_layer, hp.n_layers);
    try {
        check(w.get("posenc.Wr.weight").shape[1] == 2, "posenc is not [F, 2]");
        for (int l = 0; l < hp.n_layers; ++l) {
            char q[96];
            std::snprintf(q, sizeof q, "model.transformers.%d.", l);
            const std::string b = q;
            check(w.get(b + "self_attn.Wqkv.weight").shape[0] == 3 * hp.embed_dim,
                  "%sself_attn.Wqkv", b.c_str());
            check(w.get(b + "cross_attn.ffn.3.weight").shape[0] == hp.embed_dim,
                  "%scross_attn.ffn.3", b.c_str());
        }
    } catch (const std::exception& e) {
        check(false, "%s", e.what());
    }
    std::printf("  %d layers x %d heads, embed %lld, input %lld\n", hp.n_layers,
                hp.n_heads, (long long)hp.embed_dim, (long long)hp.input_dim);
}

// A synthetic image with structure a corner detector can find: a grid of
// squares over a smooth gradient, which gives real peaks without needing a
// file on disk.
nn::Image synthetic(int w, int h, int shift) {
    nn::Image img;
    img.width = w;
    img.height = h;
    img.channels = 3;
    img.data.resize((size_t)w * h * 3);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const int u = x + shift;
            const bool sq = ((u / 37) % 2) == ((y / 41) % 2);
            const int base = 40 + (u * 120) / std::max(w, 1);
            const uint8_t v = (uint8_t)std::min(255, sq ? base + 90 : base);
            uint8_t* p = &img.data[((size_t)y * w + x) * 3];
            p[0] = v;
            p[1] = (uint8_t)std::min(255, v + 12);
            p[2] = (uint8_t)(255 - v);
        }
    return img;
}

struct Dump {
    int w = 0, h = 0, dim = 0;
    std::vector<float> xy, score, desc;
};

Dump to_dump(const Features& f) {
    Dump d;
    d.w = f.width;
    d.h = f.height;
    d.dim = f.desc_dim;
    d.xy.resize(f.keypoints.size() * 2);
    d.score.resize(f.keypoints.size());
    for (size_t i = 0; i < f.keypoints.size(); ++i) {
        d.xy[i * 2] = f.keypoints[i].x;
        d.xy[i * 2 + 1] = f.keypoints[i].y;
        d.score[i] = f.keypoints[i].score;
    }
    d.desc = f.descriptors;
    return d;
}

void write_dump(const Dump& d, const std::string& path) {
    std::FILE* fp = std::fopen(path.c_str(), "wb");
    check(fp != nullptr, "cannot write %s", path.c_str());
    if (!fp) return;
    const uint32_t version = 1, count = (uint32_t)d.score.size(), dim = (uint32_t)d.dim;
    std::fwrite("LOMAFEAT", 1, 8, fp);
    std::fwrite(&version, 4, 1, fp);
    std::fwrite(&d.w, 4, 1, fp);
    std::fwrite(&d.h, 4, 1, fp);
    std::fwrite(&count, 4, 1, fp);
    std::fwrite(&dim, 4, 1, fp);
    for (uint32_t i = 0; i < count; ++i) {
        const float v[3] = {d.xy[i * 2], d.xy[i * 2 + 1], d.score[i]};
        std::fwrite(v, 4, 3, fp);
    }
    std::fwrite(d.desc.data(), 4, d.desc.size(), fp);
    std::fclose(fp);
    std::printf("  wrote %s\n", path.c_str());
}

Dump read_dump(const std::string& path) {
    Dump d;
    std::FILE* f = std::fopen(path.c_str(), "rb");
    check(f != nullptr, "cannot read %s", path.c_str());
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
    for (uint32_t i = 0; i < count && ok; ++i) {
        float v[3];
        rd(v, 4, 3);
        d.xy[(size_t)i * 2] = v[0];
        d.xy[(size_t)i * 2 + 1] = v[1];
        d.score[i] = v[2];
    }
    d.desc.resize((size_t)count * dim);
    if (!d.desc.empty()) rd(d.desc.data(), 4, d.desc.size());
    std::fclose(f);
    check(ok, "truncated dump %s", path.c_str());
    return ok ? d : Dump{};
}

Dump run_extraction(Extractor& ex, const nn::Image& img, const ExtractOptions& opts,
                    const char* label) {
    const double t0 = nn::now_ms();
    Features f = ex.extract(img.data.data(), img.width, img.height, opts);
    const double ms = nn::now_ms() - t0;

    double lo = 1e9, hi = -1e9, sum = 0;
    for (const Keypoint& k : f.keypoints) {
        lo = std::min(lo, (double)k.score);
        hi = std::max(hi, (double)k.score);
        sum += k.score;
    }
    std::printf("%s  %dx%d -> %zu keypoints in %.0f ms, score %.3g .. %.3g (sum %.4f)\n",
                label, img.width, img.height, f.keypoints.size(), ms, lo, hi, sum);

    check(!f.keypoints.empty(), "%s: no keypoints", label);
    check(f.desc_dim > 0 && f.descriptors.size() == f.keypoints.size() * f.desc_dim,
          "%s: %zu descriptors for %zu keypoints at dim %d", label, f.descriptors.size(),
          f.keypoints.size(), f.desc_dim);
    bool in_frame = true, finite = true;
    for (const Keypoint& k : f.keypoints)
        if (!(k.x >= 0 && k.x <= img.width && k.y >= 0 && k.y <= img.height))
            in_frame = false;
    for (float v : f.descriptors)
        if (!std::isfinite(v)) finite = false;
    check(in_frame, "%s: a keypoint landed outside the image", label);
    check(finite, "%s: a descriptor is not finite", label);
    // The density is a softmax over the whole map, so every score is in (0, 1]
    // and their sum over ALL pixels is 1 -- a kept subset must not exceed it.
    check(lo > 0.0 && sum <= 1.0 + 1e-3, "%s: scores are not a sub-distribution", label);

    // The arena refuses to grow mid-pass, so an under-counting plan is a hard
    // failure at some image size and silence at every other. Assert it here
    // rather than wait for the size that trips it.
    const uint64_t plan = ex.plannedBytes(), peak = ex.peakBytes();
    std::printf("  arena: %.0f MB used of %.0f MB planned (%.0f%%)\n", peak / 1e6,
                plan / 1e6, 100.0 * (double)peak / (double)std::max<uint64_t>(plan, 1));
    check(peak <= plan, "%s: the arena used %llu bytes against a plan of %llu", label,
          (unsigned long long)peak, (unsigned long long)plan);
    return to_dump(f);
}

void run_matching(Matcher& m, const Dump& A, const Dump& B, float min_score,
                  const std::string& out_path) {
    if (A.desc.empty() || B.desc.empty()) return;
    check(A.dim == m.descriptorDim(),
          "features are %d-D but the matcher wants %d", A.dim, m.descriptorDim());
    if (A.dim != m.descriptorDim()) return;

    MatchInput ia, ib;
    ia.keypoints = A.xy.data();
    ia.descriptors = A.desc.data();
    ia.n = (uint32_t)A.score.size();
    ia.width = A.w;
    ia.height = A.h;
    ib.keypoints = B.xy.data();
    ib.descriptors = B.desc.data();
    ib.n = (uint32_t)B.score.size();
    ib.width = B.w;
    ib.height = B.h;

    MatchOptions mo;
    mo.min_score = min_score;
    const double t0 = nn::now_ms();
    std::vector<Match> mm = m.match(ia, ib, mo);
    const double ms = nn::now_ms() - t0;
    std::printf("matcher: %u x %u -> %zu matches in %.0f ms\n", ia.n, ib.n, mm.size(), ms);
    check(!mm.empty(), "the matcher returned nothing at all");

    // Every index in range and every keypoint used once on each side: a
    // mutual-nearest assignment cannot repeat either.
    std::vector<char> seen_i(ia.n, 0), seen_j(ib.n, 0);
    bool ok = true;
    for (const Match& x : mm) {
        if (x.i >= ia.n || x.j >= ib.n) { ok = false; break; }
        if (seen_i[x.i] || seen_j[x.j]) { ok = false; break; }
        seen_i[x.i] = seen_j[x.j] = 1;
        if (!(x.score > min_score && x.score <= 1.0f + 1e-3f)) { ok = false; break; }
    }
    check(ok, "a duplicate, out-of-range or invalid-score match");

    if (out_path.empty()) return;
    std::FILE* f = std::fopen(out_path.c_str(), "wb");
    check(f != nullptr, "cannot write %s", out_path.c_str());
    if (!f) return;
    const uint32_t n = (uint32_t)mm.size();
    std::fwrite(&n, 4, 1, f);
    for (const Match& x : mm) {
        std::fwrite(&x.i, 4, 1, f);
        std::fwrite(&x.j, 4, 1, f);
        std::fwrite(&x.score, 4, 1, f);
    }
    std::fclose(f);
    std::printf("  wrote %s\n", out_path.c_str());
}

}  // namespace

int main(int argc, char** argv) {
    if (!spirula::env("NN_LOG")) nn::set_log_level(2);

    bool fetch = false;
    std::string image_a, image_b, out_path, match_a, match_b;
    std::string matcher_id = "loma-b128", detector_id = "loma-dad", descriptor_id;
    int max_image_size = 1600;
    ExtractOptions opts;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&]() { return (i + 1 < argc) ? std::string(argv[++i]) : std::string(); };
        if (a == "--fetch") fetch = true;
        else if (a == "--image") image_a = next();
        else if (a == "--image2") image_b = next();
        else if (a == "--out") out_path = next();
        else if (a == "--matcher") matcher_id = next();
        else if (a == "--detector") detector_id = next();
        else if (a == "--descriptor") descriptor_id = next();
        else if (a == "--max-image-size") max_image_size = std::atoi(next().c_str());
        else if (a == "--max-features") opts.max_num_features = std::atoi(next().c_str());
        else if (a == "--min-score") opts.min_score = (float)std::atof(next().c_str());
        else if (a == "--match") { match_a = next(); match_b = next(); }
        else { std::printf("unknown argument '%s'\n", a.c_str()); return 2; }
    }
    if (descriptor_id.empty()) descriptor_id = descriptor_for_matcher(matcher_id);
    if (descriptor_id.empty()) descriptor_id = "loma-dedode-b";

    try {
        vk::Context::get();

        const std::string det = locate(detector_id.c_str(), fetch);
        const std::string desc = locate(descriptor_id.c_str(), fetch);
        const std::string mat = locate(matcher_id.c_str(), fetch);
        if (det.empty() || desc.empty() || mat.empty()) {
            std::printf("SKIP: no LoMa checkpoint cached (%s / %s / %s). Run with "
                        "--fetch to download.\n",
                        detector_id.c_str(), descriptor_id.c_str(), matcher_id.c_str());
            vk::Stream::shutdown();
            vk::Pipelines::get().shutdown();
            vk::VramPool::get().releaseAll();
            return 0;
        }

        check_pyramid(det, "det");
        check_pyramid(desc, "desc");
        check_matcher(mat);

        std::printf("\nExtraction\n");
        Extractor ex;
        ex.load(det, desc);
        // With no image given, a synthetic pair: two shifted copies of the same
        // pattern, which is a real correspondence the matcher has to find.
        nn::Image a = image_a.empty() ? synthetic(640, 480, 0) : nn::load_image(image_a);
        nn::Image b;
        if (!image_b.empty()) b = nn::load_image(image_b);
        else if (image_a.empty()) b = synthetic(640, 480, 11);
        check(!a.empty(), "cannot read %s", image_a.c_str());
        if (max_image_size > 0 && !a.empty()) {
            // Nearest-neighbour, only to keep this test's working size bounded;
            // the pipeline's own downscale is the loader's (sfm/core/Image.h).
            auto shrink = [&](nn::Image& img) {
                const int m = std::max(img.width, img.height);
                if (m <= max_image_size) return;
                const double s = (double)max_image_size / m;
                const int nw = std::max(1, (int)(img.width * s));
                const int nh = std::max(1, (int)(img.height * s));
                std::vector<uint8_t> dst((size_t)nw * nh * 3);
                for (int y = 0; y < nh; ++y)
                    for (int x = 0; x < nw; ++x)
                        std::memcpy(&dst[((size_t)y * nw + x) * 3],
                                    &img.data[((size_t)std::min(img.height - 1,
                                                                (int)((y + 0.5) / s)) *
                                                   img.width +
                                               std::min(img.width - 1,
                                                        (int)((x + 0.5) / s))) *
                                              3],
                                    3);
                img.data.swap(dst);
                img.width = nw;
                img.height = nh;
            };
            shrink(a);
            if (!b.empty()) shrink(b);
        }

        Matcher m;
        m.load(mat);

        if (!match_a.empty()) {
            std::printf("\nMatching two dumps\n");
            run_matching(m, read_dump(match_a), read_dump(match_b), 0.1f, out_path);
        } else if (!a.empty()) {
            if (image_a.empty()) {
                // An odd, non-square size: three halvings floor, and the
                // cascade's resize targets have to land back on the taps.
                const nn::Image odd = synthetic(654, 401, 3);
                run_extraction(ex, odd, opts, "odd 654x401");
            }
            const Dump da = run_extraction(ex, a, opts, "A");
            if (!out_path.empty()) write_dump(da, out_path);
            if (!b.empty()) {
                const Dump db = run_extraction(ex, b, opts, "B");
                std::printf("\nMatching\n");
                run_matching(m, da, db, 0.1f, "");
            }
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
