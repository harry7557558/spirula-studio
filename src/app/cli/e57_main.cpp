// spirula e57 -- an E57 laser scan written out as a training dataset
// (app/E57Dataset.h). The GUI's "Create Dataset from E57" runs this as a child
// process and reads its progress lines back, so every line is a catalog Msg.

#include "app/E57Dataset.h"
#include "app/Tools.h"
#include "i18n/Message.h"
#include "i18n/catalog/Data.h"
#include "i18n/catalog/E57.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <string>

namespace {

namespace E = spirula::i18n::msg::e57;
using spirula::i18n::format;

// Half the default cap_max, which leaves the densifier room to add detail.
constexpr long long kDefaultPoints = 500000;

void help_row(const char* flags, const std::string& text, int col = 22) {
    std::string left = std::string("    ") + flags;
    if (spirula::i18n::display_width(left) >= col + 4) {
        std::fprintf(stderr, "%s\n", left.c_str());
        left.clear();
    }
    left = spirula::i18n::pad_to(left, col + 4);
    for (const std::string& line : spirula::i18n::wrap(text, 86 - col - 4)) {
        std::fprintf(stderr, "%s%s\n", left.c_str(), line.c_str());
        left.assign((size_t)col + 4, ' ');
    }
}

void usage() {
    const std::string prog = app::program_name();
    std::fprintf(stderr, "%s -- %s\n\n", prog.c_str(), E::tagline.get());
    std::fprintf(stderr, "    %s <scan.e57> [<dataset folder>] [options]\n\n", prog.c_str());
    for (const std::string& l : spirula::i18n::wrap(E::usage_about.get(), 80))
        std::fprintf(stderr, "    %s\n", l.c_str());
    std::fprintf(stderr, "\n%s\n", E::head_options.get());
    help_row("--points <n>|all", format(E::opt_points, {kDefaultPoints}));
    help_row("--no-depth", E::opt_no_depth.get());
    help_row("--no-pinhole", E::opt_no_pinhole.get());
    help_row("--no-panoramas", E::opt_no_panoramas.get());
    help_row("--overwrite", E::opt_overwrite.get());
    help_row("--info", E::opt_info.get());
}

void say(const std::string& line) {
    std::printf("%s\n", line.c_str());
    std::fflush(stdout);
}

int info(const std::string& path) {
    spirula::e57::Reader reader(path);
    const auto& scans = reader.scans();
    const auto& images = reader.images();
    long long pinhole = 0, spherical = 0;
    for (const auto& im : images) {
        pinhole += im.projection == spirula::e57::Projection::Pinhole;
        spherical += im.projection == spirula::e57::Projection::Spherical;
    }
    say(format(E::summary, {(long long)scans.size(), (long long)reader.total_points(),
                            (long long)images.size()}));
    say(format(E::summary_images, {pinhole, spherical,
                                   (long long)images.size() - pinhole - spherical}));
    for (size_t i = 0; i < scans.size(); i++)
        say(format(E::info_scan, {(long long)i, scans[i].name, (long long)scans[i].count}));
    return 0;
}

}  // namespace

int spirula_e57_main(int argc, char** argv) {
    app::set_program_name(argc > 0 ? argv[0] : nullptr, "spirula e57");
    app::E57DatasetOptions opt;
    opt.seed_points = kDefaultPoints;
    bool list_only = false;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--help" || a == "-h") { usage(); return 0; }
        else if (a == "--no-pinhole") opt.pinhole = false;
        else if (a == "--no-panoramas") opt.spherical = false;
        else if (a == "--no-depth") opt.depth_maps = false;
        else if (a == "--overwrite") opt.overwrite = true;
        else if (a == "--info") list_only = true;
        else if (a == "--points" && i + 1 < argc) {
            const char* v = argv[++i];
            if (std::strcmp(v, "all") == 0) {
                opt.all_points = true;
                continue;
            }
            char* end = nullptr;
            const long long n = std::strtoll(v, &end, 10);
            if (end == v || *end != '\0' || n < 0) {
                std::fprintf(stderr, "%s\n", format(E::err_bad_number, {a, v}).c_str());
                return 2;
            }
            opt.seed_points = n;
        } else if (!a.empty() && a[0] == '-') {
            std::fprintf(stderr, "%s\n\n", format(E::err_unknown_option, {a}).c_str());
            usage();
            return 2;
        } else if (opt.input.empty()) opt.input = a;
        else if (opt.output.empty()) opt.output = a;
        else { usage(); return 2; }
    }
    if (opt.input.empty()) {
        std::fprintf(stderr, "%s\n", format(E::err_no_input, {app::program_name()}).c_str());
        return 2;
    }
    const char* error_word = spirula::i18n::msg::data::word_error.get();
    try {
        if (list_only) return info(opt.input);
        if (opt.output.empty()) opt.output = app::default_e57_dataset_dir(opt.input);
        const app::E57DatasetResult r = app::write_e57_dataset(opt, say);
        if (r.cancelled) return 1;
        say(format(E::next_step, {"spirula train --data \"" + opt.output + "\""}));
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s %s\n", error_word, e.what());
        return 1;
    }
    return 0;
}
