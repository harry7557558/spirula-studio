// `spirula focus` -- per-pixel focus weights for a dataset, written to focus/
// beside the images. Reads the depth `spirula geometry --depth` wrote; the
// method is app/FocusWeight.h, the numbers behind it docs/notes/focus-weights.md.

#include "app/Tools.h"

#include "app/FocusWeight.h"
#include "data/DatasetParser.h"
#include "i18n/Message.h"
#include "i18n/catalog/Focus.h"
#include "i18n/catalog/Geometry.h"
#include "nn/io/Image.h"

#include "external/stb_image.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;
namespace F = spirula::i18n::msg::focus;
namespace G = spirula::i18n::msg::geometry;

using spirula::i18n::format;

namespace {

struct Options {
    std::string dataset;
    std::string image_dir = "images";
    std::string depth_dir = "depths";
    std::string out_dir = "focus";
    app::FocusSettings focus;
    bool overwrite = false;
};

void help_row(const char* flags, const spirula::i18n::Msg& m, int col = 26) {
    std::string left = std::string("    ") + flags;
    if (spirula::i18n::display_width(left) >= col + 4) {
        std::fprintf(stderr, "%s\n", left.c_str());
        left.clear();
    }
    left = spirula::i18n::pad_to(left, col + 4);
    for (const std::string& line : spirula::i18n::wrap(m.get(), 86 - col - 4)) {
        std::fprintf(stderr, "%s%s\n", left.c_str(), line.c_str());
        left.assign((size_t)col + 4, ' ');
    }
}

void usage() {
    const std::string prog = app::program_name();
    std::fprintf(stderr, "%s -- %s\n\n", prog.c_str(), F::tagline.get());
    std::fprintf(stderr, "    %s <dataset> [options]\n\n", prog.c_str());
    for (const std::string& l : spirula::i18n::wrap(F::usage_target.get(), 80))
        std::fprintf(stderr, "    %s\n", l.c_str());
    std::fprintf(stderr, "\n%s\n", G::head_options.get());
    help_row("--image-dir <dir>", G::opt_image_dir);
    help_row("--depth-dir <dir>", F::opt_depth_dir);
    help_row("--out-dir <dir>", F::opt_out_dir);
    help_row("--allowed <px>", F::opt_allowed);
    help_row("--train-side <px>", F::opt_train_side);
    help_row("--overwrite", F::opt_overwrite);
    std::fprintf(stderr, "\n%s --lang <code>\n", G::label_common.get());
}

// Inverse depth with 0 where the geometry model had no answer. The stored
// scale is per image and cancels in the fit.
std::vector<float> read_inverse_depth(const std::string& path, int& w, int& h) {
    int ch = 0;
    stbi_us* d = stbi_load_16(path.c_str(), &w, &h, &ch, 1);
    if (!d) return {};
    std::vector<float> u((size_t)w * h);
    for (size_t i = 0; i < u.size(); ++i) u[i] = d[i] ? 65535.0f / (float)d[i] : 0.0f;
    stbi_image_free(d);
    return u;
}

}  // namespace

int spirula_focus_main(int argc, char** argv) {
    app::set_program_name(argc > 0 ? argv[0] : nullptr, "spirula focus");
    Options o;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&]() -> const char* {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "%s needs a value\n", a.c_str());
                std::exit(2);
            }
            return argv[++i];
        };
        if (a == "--help" || a == "-h") { usage(); return 0; }
        else if (a == "--image-dir") o.image_dir = next();
        else if (a == "--depth-dir") o.depth_dir = next();
        else if (a == "--out-dir") o.out_dir = next();
        else if (a == "--allowed") o.focus.allowed_px = (float)std::atof(next());
        else if (a == "--train-side") o.focus.train_side = std::atoi(next());
        else if (a == "--overwrite") o.overwrite = true;
        else if (!a.empty() && a[0] == '-') {
            std::fprintf(stderr, "unknown option '%s'\n\n", a.c_str());
            usage();
            return 2;
        } else if (o.dataset.empty()) o.dataset = a;
        else { usage(); return 2; }
    }
    if (o.dataset.empty()) {
        std::fprintf(stderr, "%s\n", format(G::err_no_dataset, {app::program_name()}).c_str());
        return 2;
    }

    try {
        DatasetParserConfig cfg;
        cfg.require_image_files = false;
        cfg.image_dir = o.image_dir;
        const ParsedDataset ds = parse_dataset(o.dataset, cfg, "");
        const fs::path root(o.dataset);
        const fs::path image_root = root / cfg.image_dir;
        std::error_code ec;
        auto rel_of = [&](const std::string& image, const char* ext) {
            fs::path rel = fs::relative(fs::path(image), image_root, ec);
            if (ec || rel.empty() || rel.native()[0] == '.') rel = fs::path(image).filename();
            return rel.replace_extension(ext);
        };

        int64_t written = 0, skipped = 0, no_depth = 0;
        for (size_t i = 0; i < ds.image_filenames.size(); ++i) {
            const std::string& image = ds.image_filenames[i];
            const fs::path out = root / o.out_dir / rel_of(image, ".png");
            if (!o.overwrite && fs::exists(out, ec)) { ++skipped; continue; }
            const fs::path dpath = root / o.depth_dir / rel_of(image, ".png");
            int dw = 0, dh = 0;
            const std::vector<float> u = read_inverse_depth(dpath.string(), dw, dh);
            if (u.empty()) {
                std::string exe = app::program_name();
                if (exe.size() > 6 && exe.compare(exe.size() - 6, 6, " focus") == 0) exe.resize(exe.size() - 6);
                std::fprintf(stderr, "%s\n", format(F::err_no_depth, {fs::path(image).filename().string(),
                                                        o.depth_dir, exe}).c_str());
                ++no_depth;
                continue;
            }
            const nn::Image img = nn::load_image(image);
            if (img.empty()) continue;
            const int f = app::focus_measure_factor(img.width, img.height);
            int mw = 0, mh = 0;
            const std::vector<float> gray = app::gray_downsampled(img.data.data(), img.width, img.height, f, mw, mh);
            const app::FocusEdges edges = app::measure_edge_blur(gray, mw, mh);
            const app::FocusCurve curve = app::fit_focus_curve(edges, mw, mh, u, dw, dh);
            std::vector<uint8_t> weight;
            if (curve.ok) {
                weight = app::render_focus_weight(curve, u, dw, dh, mw, mh, img.width, img.height, o.focus);
            } else {
                std::fprintf(stderr, "%s\n", format(F::warn_no_fit, {fs::path(image).filename().string()}).c_str());
                weight.assign((size_t)img.width * img.height, 255);
            }
            fs::create_directories(out.parent_path(), ec);
            if (!nn::save_gray_png(weight.data(), img.width, img.height, out.string())) continue;
            double mean = 0;
            for (uint8_t v : weight) mean += v;
            mean /= 255.0 * (double)weight.size();
            char m[16], r[16];
            std::snprintf(m, sizeof m, "%.3f", mean);
            std::snprintf(r, sizeof r, "%.2f", curve.rms);
            std::printf("[%zu/%zu] %s\n", i + 1, ds.image_filenames.size(),
                        format(F::log_image, {rel_of(image, fs::path(image).extension().string().c_str())
                                                  .generic_string(), std::string(m), std::string(r)}).c_str());
            std::fflush(stdout);
            ++written;
        }
        std::printf("%s\n", format(F::log_done, {(long long)written, (long long)skipped, (long long)no_depth}).c_str());
        return no_depth > 0 && written == 0 ? 1 : 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "\nerror: %s\n", e.what());
        return 1;
    }
}
