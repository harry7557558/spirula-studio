// `spirula-sam extract` -- pull the sharpest frames out of a video, and
// optionally mask them.
//
// The native replacement for reference/scripts/extract_frames.py, with the
// OpenCV/ffmpeg/PyTorch stack replaced by this tree: decode is Vulkan Video,
// the motion-blur metric is a compute shader, and masking is SAM on the same
// device. Only the JPEG/PNG encode stays on the CPU, on a pool of worker
// threads, because that is what it is good at.
//
// The frame-selection arithmetic is deliberately identical to the Python's
// (a window of `keep` frames, one written every `skip`), so a dataset
// extracted either way has the same file names for the same source frames --
// which is also what lets the GUI fall back to ffmpeg + FrameSelect without
// the output changing.
//
// This file is compiled only with -DSS_ENABLE_PATENTED=ON; the dispatcher
// in sam_main.cpp explains the fallback when it is not.
//
//   spirula-sam extract clip.mp4 --skip 10
//   spirula-sam extract clip.mp4 --skip 10 --model sam3-f16.ggml --text "person; car"
//   spirula-sam extract dish.mov --model sam3-f16.ggml --text food \
//                      --neg-text "cooked food" --mask-keep subject

#include "app/FrameExtract.h"
#include "app/Tools.h"
#include "i18n/catalog/Cli.h"
#include "i18n/catalog/Data.h"
#include "i18n/catalog/SamHelp.h"
#include "nn/core/Log.h"
#include "sam/Masking.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

namespace {

void set_env(const char* key, const char* value) {
#ifdef _WIN32
    _putenv_s(key, value);
#else
    setenv(key, value, 1);
#endif
}

// ---------------------------------------------------------------------------
// Options
// ---------------------------------------------------------------------------

// A row of `--help`: identifier on the left, translated sentence on the right,
// wrapped by i18n::wrap so a language without spaces still breaks.
void help_row(const char* flags, const spirula::i18n::Msg& m) {
    constexpr int kCol = 25;
    std::string left = std::string("  ") + flags;
    if (spirula::i18n::display_width(left) >= kCol) {
        std::fprintf(stderr, "%s\n", left.c_str());
        left.clear();
    }
    left = spirula::i18n::pad_to(left, kCol);
    for (const std::string& line : spirula::i18n::wrap(m.get(), 86 - kCol)) {
        std::fprintf(stderr, "%s%s\n", left.c_str(), line.c_str());
        left.assign(kCol, ' ');
    }
}

void usage() {
    namespace H = spirula::i18n::msg::samhelp;
    std::fprintf(stderr, "%s extract <video> [options]\n\n",
                 app::program_name().c_str());

    std::fprintf(stderr, "%s\n", H::xh_frame_selection.get());
    help_row("-o, --out <dir>", H::xh_out);
    help_row("-s, --skip <n>", H::xh_skip);
    help_row("-k, --keep <n>", H::xh_keep);
    help_row("-n, --max-frames <n>", H::xh_max_frames);
    help_row("-q, --quality <0..100>", H::xh_quality);
    help_row("-r, --rotate <deg>", H::xh_rotate);
    help_row("    --scale <f>", H::xh_scale);
    help_row("    --track <i>", H::xh_track);
    help_row("    --threads <n>", H::xh_threads);

    std::fprintf(stderr, "\n%s\n", H::xh_360_section.get());
    help_row("    --360 <mode>", H::xh_360);
    help_row("    --360-size <n>", H::xh_360_size);
    help_row("    --360-orient <y,p,r>", H::xh_360_orient);

    std::fprintf(stderr, "\n%s\n", H::xh_masking.get());
    help_row("    --model <file>", H::xh_model);
    help_row("    --text <phrases>", H::xh_text);
    help_row("    --neg-text <ph>", H::xh_neg_text);
    help_row("    --mask-mode <m>", H::xh_mask_mode);
    help_row("    --mask-keep <w>", H::xh_mask_keep);
    help_row("    --mask-out <dir>", H::xh_mask_out);
    help_row("    --detect-every <n>", H::xh_detect_every);
    help_row("    --memory-frames <n>", H::xh_memory_frames);
    help_row("    --max-size <n>", H::xh_max_size);
    help_row("    --threshold <f>", H::xh_threshold);
    help_row("    --nms <f>", H::xh_nms);
    help_row("    --dilate-ratio <f>", H::mask_dilate);
    help_row("    --overlay", H::xh_overlay);

    std::fprintf(stderr, "\n%s --device <index|name>  --profile  --validate\n",
                 H::label_common.get());
}

struct Options {
    std::string input;
    std::string out_dir, mask_dir;
    int    skip = 1, keep = -1, max_frames = 0;
    int    quality = 95, rotate = 0;
    float  scale = 1.0f;
    int    track = -1;
    int    threads = 0;

    std::string pano_mode = "faces";
    app::Pano360Options pano;

    std::string model, text, neg_text, device;
    std::string mask_mode = "video";
    bool   keep_subject = false;
    int    detect_every = 1, memory_frames = 0, max_size = 1600;
    float  threshold = 0.5f, nms = 0.1f;
    float  dilate_ratio = 0.05f;   // sam::MaskOptions, same default
    bool   overlay = false, profile = false, validate = false;
};

bool parse_args(int argc, char** argv, Options& o) {
    if (argc < 2) return false;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&](const char* what) -> const char* {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "%s\n",
                             spirula::i18n::format(
                                 spirula::i18n::msg::cli::sam_flag_needs_value,
                                 {what}).c_str());
                std::exit(2);
            }
            return argv[++i];
        };
        if (a == "-o" || a == "--out") o.out_dir = next("--out");
        else if (a == "-s" || a == "--skip") o.skip = std::atoi(next("--skip"));
        else if (a == "-k" || a == "--keep") o.keep = std::atoi(next("--keep"));
        else if (a == "-n" || a == "--max-frames") o.max_frames = std::atoi(next("--max-frames"));
        else if (a == "-q" || a == "--quality") o.quality = std::atoi(next("--quality"));
        else if (a == "-r" || a == "--rotate") o.rotate = std::atoi(next("--rotate"));
        else if (a == "--scale") o.scale = std::strtof(next("--scale"), nullptr);
        else if (a == "--track") o.track = std::atoi(next("--track"));
        else if (a == "--threads") o.threads = std::atoi(next("--threads"));
        else if (a == "--360") o.pano_mode = next("--360");
        else if (a == "--360-size") o.pano.size = std::atoi(next("--360-size"));
        else if (a == "--360-orient") {
            const char* v = next("--360-orient");
            if (std::sscanf(v, "%f,%f,%f", &o.pano.yaw, &o.pano.pitch,
                            &o.pano.roll) != 3) {
                std::fprintf(stderr, "%s\n",
                             spirula::i18n::format(
                                 spirula::i18n::msg::cli::sam_flag_needs_value,
                                 {"--360-orient"}).c_str());
                return false;
            }
        }
        else if (a == "--model") o.model = next("--model");
        else if (a == "--text") o.text = next("--text");
        else if (a == "--neg-text") o.neg_text = next("--neg-text");
        else if (a == "--mask-mode") o.mask_mode = next("--mask-mode");
        else if (a == "--mask-keep") o.keep_subject = std::strcmp(next("--mask-keep"), "subject") == 0;
        else if (a == "--mask-out") o.mask_dir = next("--mask-out");
        else if (a == "--detect-every") o.detect_every = std::atoi(next("--detect-every"));
        else if (a == "--memory-frames") o.memory_frames = std::atoi(next("--memory-frames"));
        else if (a == "--max-size") o.max_size = std::atoi(next("--max-size"));
        else if (a == "--threshold") o.threshold = std::strtof(next("--threshold"), nullptr);
        else if (a == "--nms") o.nms = std::strtof(next("--nms"), nullptr);
        else if (a == "--dilate-ratio")
            o.dilate_ratio = std::strtof(next("--dilate-ratio"), nullptr);
        else if (a == "--overlay") o.overlay = true;
        else if (a == "--device") o.device = next("--device");
        else if (a == "--profile") o.profile = true;
        else if (a == "--validate") o.validate = true;
        else if (a == "-h" || a == "--help") return false;
        else if (a[0] == '-') {
            std::fprintf(stderr, "%s\n",
                         spirula::i18n::format(
                             spirula::i18n::msg::cli::sam_unknown_option,
                             {a}).c_str());
            return false;
        } else if (o.input.empty()) {
            o.input = a;
        } else {
            std::fprintf(stderr, "%s\n",
                         spirula::i18n::format(
                             spirula::i18n::msg::cli::sam_unexpected_argument,
                             {a}).c_str());
            return false;
        }
    }
    if (o.input.empty()) return false;
    if (o.skip < 1) o.skip = 1;
    if (o.keep < 0) o.keep = (int)(0.5 * o.skip + 0.5);
    if (o.rotate % 90 != 0) {
        std::fprintf(stderr, "%s\n",
                     spirula::i18n::msg::cli::sam_rotate_multiple_of_90.get());
        return false;
    }
    return true;
}

}  // namespace

// Declared in sam_main.cpp's dispatcher.
int sam_cli_extract(int argc, char** argv);

int sam_cli_extract(int argc, char** argv) {
    Options o;
    if (!parse_args(argc, argv, o)) {
        usage();
        return 2;
    }
    // The decoder and the masker each reach vk::Context::get() on their own,
    // so the device and the diagnostic switches are handed over the way the
    // context reads them rather than plumbed through two option structs.
    if (o.validate) set_env("SS_VK_VALIDATION", "1");
    if (o.profile) set_env("SS_PROFILE", "1");
    if (!o.device.empty()) set_env("SS_VK_DEVICE", o.device.c_str());

    const fs::path input(o.input);
    const fs::path base = o.out_dir.empty()
                              ? (input.parent_path() / input.stem() / "images")
                              : fs::path(o.out_dir);

    app::FrameExtractJob job;
    job.input = o.input;
    job.image_dir = base.string();
    job.mask_dir = o.mask_dir;
    job.skip = o.skip;
    job.keep = o.keep;
    job.max_frames = o.max_frames;
    job.quality = o.quality;
    job.rotate = o.rotate;
    job.scale = o.scale;
    job.track = o.track;
    job.threads = o.threads;
    job.write_overlay = o.overlay;
    // A 360 file is recognised by its packing, not by its name, and only then
    // is there anything for --360 to select.
    if (o.pano_mode != "off") {
        std::string err;
        const std::vector<std::pair<int, int>> tracks =
            app::video_track_sizes(o.input, err);
        if (tracks.size() == 2 && tracks[0] == tracks[1] &&
            app::eac360_detect(2, tracks[0].first, tracks[0].second, job.eac)) {
            o.pano.mode = o.pano_mode == "equirect" ? app::Pano360Mode::Equirect
                                                    : app::Pano360Mode::Faces;
            job.views = app::pano360_views(job.eac, o.pano);
            std::fprintf(stderr, "360: %d x %d tracks -> %zu view(s) of %dx%d\n",
                         job.eac.track_w, job.eac.track_h, job.views.size(),
                         job.views[0].width, job.views[0].height);
        } else {
            job.eac = app::Eac360Layout{};
        }
    }
    if (!o.model.empty()) {
        job.mask.model = o.model;
        job.mask.device = o.device;
        job.mask.text = o.text;
        job.mask.neg_text = o.neg_text;
        job.mask.video = o.mask_mode != "image";
        job.mask.keep_prompted = o.keep_subject;
        job.mask.threshold = o.threshold;
        job.mask.nms = o.nms;
        job.mask.dilate_ratio = o.dilate_ratio;
        job.mask.detect_every = o.detect_every;
        job.mask.memory_frames = o.memory_frames;
        job.mask.max_size = o.max_size;
        job.mask.validate = o.validate;
    }

    app::FrameExtractSinks sinks;
    sinks.log = [](const std::string& l) { std::fprintf(stderr, "%s\n", l.c_str()); };

    app::FrameExtractStats stats;
    std::string error;
    int rc = 0;
    if (!app::extract_frames(job, sinks, stats, error)) {
        std::fprintf(stderr, "%s\n",
                     spirula::i18n::format(spirula::i18n::msg::cli::error_line,
                                           {error}).c_str());
        rc = 1;
    }
    std::printf("%s", app::format_extract_stats(stats, base.string(),
                                                !o.model.empty()).c_str());
    if (stats.write_failures) {
        std::fprintf(stderr, "%s %s\n",
                     spirula::i18n::msg::data::word_warning.get(),
                     spirula::i18n::format(
                         spirula::i18n::msg::data::write_failures,
                         {stats.write_failures}).c_str());
        rc = 1;
    }
    return rc;
}
