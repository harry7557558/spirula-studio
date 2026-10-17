// `spirula focus` merges per-pixel focus weights into the dataset's masks/.

#include "app/Tools.h"

#include "app/FocusWeight.h"
#include "app/FocusMaskMerge.h"
#include "app/FocusOverlay.h"
#include "core/FileLock.h"
#include "data/DatasetParser.h"
#include "i18n/Message.h"
#include "i18n/catalog/Focus.h"
#include "i18n/catalog/Geometry.h"
#include "nn/io/Image.h"

#include "external/stb_image.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <numeric>
#include <string>
#include <set>
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
    std::string out_dir = "masks";
    std::string overlay_dir = "focus_overlay";
    app::FocusSettings focus;
    bool overwrite = false;
    bool overlay_only = false;
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
    help_row("--overlay-dir <dir>", F::opt_overlay_dir);
    help_row("--overlay-only", F::opt_overlay_only);
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
        else if (a == "--overlay-dir") o.overlay_dir = next();
        else if (a == "--overlay-only") o.overlay_only = true;
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
        const fs::path overlay_root = fs::weakly_canonical(root / o.overlay_dir);
        auto overlaps = [&](const fs::path& other) {
            const fs::path canonical = fs::weakly_canonical(other);
            const auto a = canonical.lexically_relative(overlay_root);
            const auto b = overlay_root.lexically_relative(canonical);
            return (!a.empty() && *a.begin() != "..") || (!b.empty() && *b.begin() != "..");
        };
        if (overlaps(root / cfg.image_dir) || overlaps(root / o.out_dir) || overlaps(root / o.depth_dir) ||
            overlaps(root / ".spirula-focus"))
            throw std::runtime_error("overlay directory must be separate from images, masks, depths and focus backups");
        const fs::path image_root = root / cfg.image_dir;
        std::vector<app::FocusOverlayEntry> previews;
        if (o.overlay_only) {
            int64_t written = 0;
            for (size_t i = 0; i < ds.image_filenames.size(); ++i) {
                const auto& image = ds.image_filenames[i];
                const std::string relative = dsparse::relative_under(image, image_root.string());
                if (relative.empty()) throw std::runtime_error("image is outside the image directory");
                const std::string mask = dsparse::find_aux_file((root / o.out_dir).string(), relative, "mask");
                const nn::Image img = nn::load_image(image);
                if (img.empty()) throw std::runtime_error("cannot read image: " + image);
                if (mask.empty()) std::fprintf(stderr, "%s\n", format(F::log_overlay_unmasked, {relative}).c_str());
                const auto weight = mask.empty() ? std::vector<uint8_t>((size_t)img.width * img.height, 255)
                                                : app::read_focus_overlay_mask(mask, img.width, img.height);
                app::write_focus_overlay(img, weight, overlay_root / (relative + ".jpg"));
                const double mean = std::accumulate(weight.begin(), weight.end(), 0.0) / (255.0 * weight.size());
                previews.push_back({relative, mean});
                std::printf("[%zu/%zu] %s\n", i + 1, ds.image_filenames.size(), relative.c_str());
                std::fflush(stdout);
                ++written;
            }
            app::write_focus_overlay_gallery(previews, overlay_root);
            std::printf("%s\n", format(F::log_overlays, {(long long)written, overlay_root.string()}).c_str());
            return 0;
        }
        fs::create_directories(root / ".spirula-focus");
        const spirula::FileLock lock(root / ".spirula-focus" / "run.lock");
        std::error_code ec;
        auto rel_of = [&](const std::string& image, const char* ext) {
            fs::path rel = fs::relative(fs::path(image), image_root, ec);
            if (ec || rel.empty() || rel.native()[0] == '.') rel = fs::path(image).filename();
            return rel.replace_extension(ext);
        };

        std::vector<app::FocusMaskTarget> targets;
        std::set<fs::path> outputs;
        for (const auto& image : ds.image_filenames) {
            const std::string rel = dsparse::relative_under(image, image_root.string());
            if (rel.empty()) throw std::runtime_error("image is outside the image directory");
            auto target = app::prepare_focus_mask(root, root / o.out_dir, rel);
            if (!outputs.insert(target.output).second) throw std::runtime_error("images share a mask filename");
            targets.push_back(std::move(target));
        }
        int64_t written = 0, skipped = 0, no_depth = 0;
        for (size_t i = 0; i < ds.image_filenames.size(); ++i) {
            const std::string& image = ds.image_filenames[i];
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
            if (img.empty()) throw std::runtime_error("cannot read image: " + image);
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
            app::write_focus_mask(targets[i], weight, img.width, img.height);
            const std::string relative = dsparse::relative_under(image, image_root.string());
            app::write_focus_overlay(img, weight, overlay_root / (relative + ".jpg"));
            double mean = 0;
            for (uint8_t v : weight) mean += v;
            mean /= 255.0 * (double)weight.size();
            previews.push_back({relative, mean});
            char m[16], r[16];
            std::snprintf(m, sizeof m, "%.3f", mean);
            std::snprintf(r, sizeof r, "%.2f", curve.rms);
            std::printf("[%zu/%zu] %s\n", i + 1, ds.image_filenames.size(),
                        format(F::log_image, {rel_of(image, fs::path(image).extension().string().c_str())
                                                  .generic_string(), std::string(m), std::string(r)}).c_str());
            std::fflush(stdout);
            ++written;
        }
        app::write_focus_overlay_gallery(previews, overlay_root);
        std::printf("%s\n", format(F::log_done, {(long long)written, (long long)skipped, (long long)no_depth}).c_str());
        std::printf("%s\n", format(F::log_overlays, {(long long)written, overlay_root.string()}).c_str());
        return no_depth > 0 ? 1 : 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "\nerror: %s\n", e.what());
        return 1;
    }
}
