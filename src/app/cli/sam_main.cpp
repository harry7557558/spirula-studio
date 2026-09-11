// spirula-sam -- segmentation, tracking and frame extraction from a shell.
//
//   spirula-sam devices
//   spirula-sam segment --model m.ggml --image cat.jpg --text "cat" --out out/
//   spirula-sam segment --model m.ggml --image cat.jpg --point 315,250 --out out/
//   spirula-sam track   --model m.ggml --frames frames/ --text "person" --out out/
//   spirula-sam video   --info clip.mp4
//   spirula-sam extract clip.mp4 --skip 30 --model m.ggml --text "person"
//
// The GUI drives the same library in-process (src/app/gui/DatasetPrep.cpp);
// this is the scriptable face of it, and how a masking or extraction problem
// gets reproduced without the GUI in the way.
//
// `video` and `extract` need the in-process decoder, which is only built with
// -DSS_ENABLE_PATENTED=ON (src/app/cli/sam_extract.cpp); without it they
// say so and point at ffmpeg.
//
// Diagnostics go to stderr, the result table to stdout, so a run pipes cleanly:
//   spirula-sam segment ... 2>/dev/null > detections.tsv

#include "app/Tools.h"
#include "i18n/catalog/Cli.h"
#include "i18n/catalog/SamHelp.h"
#include "app/FrameMask.h"
#include "app/WriterPool.h"
#include "nn/core/Log.h"
#include "nn/Device.h"
#include "nn/io/Image.h"
#include "sam/Masking.h"
#include "sam/Sam.h"
#ifdef SS_HAVE_VIDEO
#include "video/Video.h"
#endif

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <future>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;
namespace cmsg = spirula::i18n::msg::cli;
using spirula::i18n::format;

namespace {

// One row of `--help`: the flag with its value syntax on the left, the
// sentence for it on the right, wrapped where the language allows. Translated
// prose runs longer than the English it came from, and Chinese has no spaces
// to wrap at, so i18n::wrap does the breaking rather than a hand-placed \n.
void help_row(const char* flags, const std::string& text, int col = 24) {
    std::string left = std::string("        ") + flags;
    if (spirula::i18n::display_width(left) >= col + 8) {
        std::printf("%s\n", left.c_str());
        left.clear();
    }
    left = spirula::i18n::pad_to(left, col + 8);
    for (const std::string& line : spirula::i18n::wrap(text, 86 - col - 8)) {
        std::fprintf(stderr, "%s%s\n", left.c_str(), line.c_str());
        left.assign((size_t)col + 8, ' ');
    }
}
void help_row(const char* flags, const spirula::i18n::Msg& m, int col = 24) {
    help_row(flags, std::string(m.get()), col);
}

void usage() {
    namespace H = spirula::i18n::msg::samhelp;
    const std::string prog = app::program_name();
    auto line = [&](const char* rest) {
        std::fprintf(stderr, "  %s %s\n", prog.c_str(), rest);
    };
    auto para = [&](const spirula::i18n::Msg& m) {
        for (const std::string& l : spirula::i18n::wrap(m.get(), 76))
            std::fprintf(stderr, "        %s\n", l.c_str());
    };

    std::fprintf(stderr, "%s -- %s\n\n", prog.c_str(), H::tagline.get());

    line("devices");
    para(H::cmd_devices);
    std::fprintf(stderr, "\n");

    line("segment --model <file> --image <file> [options]");
    help_row("--text <phrase>", H::seg_text);
    help_row("--box x0,y0,x1,y1", H::seg_box);
    help_row("--neg-box x0,y0,x1,y1", H::seg_neg_box);
    help_row("--point x,y", H::seg_point);
    help_row("--neg-point x,y", H::seg_neg_point);
    help_row("--prompt-box x0,y0,x1,y1", H::seg_prompt_box);
    help_row("--multimask", H::seg_multimask);
    help_row("--threshold <f>", H::seg_threshold);
    help_row("--nms <f>", H::seg_nms);
    help_row("--out <dir>", H::seg_out);
    std::fprintf(stderr, "\n");

    line("track --model <file> --frames <dir> [options]");
    help_row("--text <phrase>", H::trk_text);
    help_row("--neg-text <phrase>", H::trk_neg_text);
    help_row("--point x,y", H::trk_point);
    help_row("--neg-point x,y", H::trk_neg_point);
    help_row("--object", H::trk_object);
    help_row("--at-frame <n>", H::trk_at_frame);
    std::fprintf(stderr,
                 "                                --point 640,360 --at-frame 90 "
                 "--point 700,300\n"
                 "                                --object --point 120,500\n");
    help_row("", H::trk_at_frame_example);
    help_row("--detect-every <n>", H::trk_detect_every);
    help_row("--memory-frames <n>", H::trk_memory_frames);
    help_row("--max-frames <n>", H::trk_max_frames);
    help_row("--out <dir>", H::trk_out);
    help_row("--keep-prompted", H::trk_keep_prompted);
    help_row("--dilate-ratio <f>", H::mask_dilate);
    help_row("--overlay", H::trk_overlay);
    std::fprintf(stderr, "\n");

    line("mask <images> [options]");
    para(H::cmd_mask);
    help_row("--out <dir>", H::mh_out);
    help_row("--shape <spec>", H::mh_shape);
    std::fprintf(stderr,
                 "                                ellipse cx,cy,rx,ry  |  "
                 "rect x0,y0,x1,y1\n"
                 "                                \"ellipse 0.5,0.5,0.49,0.49; "
                 "-rect 0,0.93,1,1\"\n");
    help_row("--shrink <f>", H::mh_shrink);
    help_row("--samples <n>", H::mh_samples);
    help_row("--dark <0..255>", H::mh_dark);
    help_row("--mask-image <file>", H::mh_image);
    help_row("--preview <file>", H::mh_preview);
    help_row("--print", H::mh_print);
    help_row("--replace", H::mh_replace);
    std::fprintf(stderr, "\n");

    line("video --info <file>");
    para(H::cmd_video);
    std::fprintf(stderr, "\n");

    line("extract <video> [options]");
    para(H::cmd_extract);
    for (const std::string& l : spirula::i18n::wrap(
             spirula::i18n::format(H::cmd_extract_more, {prog}), 76))
        std::fprintf(stderr, "        %s\n", l.c_str());
    std::fprintf(stderr, "\n");

    std::fprintf(stderr, "%s --device <index|name>  --vram  --profile  "
                         "--validate  --img-size <n>\n",
                 H::label_common.get());
    help_row("--max-size <n>", H::common_max_size);
    help_row("--image-gamut <name>", H::common_image_gamut);
    help_row("--image-linear / --no-image-linear", H::common_image_linear);
    std::fprintf(stderr,
                 "%s SS_NN_LOG=0..3  SS_VK_DEVICE  SS_PROFILE=1\n"
                 "             SS_VK_VALIDATION=1  SS_NN_DEBUG_SYNC=1\n",
                 H::label_environment.get());
}

bool parse_floats(const char* s, float* out, int n) {
    for (int i = 0; i < n; ++i) {
        char* end = nullptr;
        out[i] = std::strtof(s, &end);
        if (end == s) return false;
        s = end;
        if (i + 1 < n) {
            while (*s == ' ') ++s;
            if (*s != ',' && *s != 'x') return false;
            ++s;
        }
    }
    return true;
}

struct Options {
    std::string command;
    std::string model, image, frames, out_dir, text, neg_text, video_path;
    std::string device;
    std::vector<sam::Box> pos_boxes, neg_boxes;
    std::vector<sam::Point> pos_points, neg_points;
    sam::Box prompt_box{};
    bool use_prompt_box = false;
    // Clicks for `track`, grouped into objects and frames as they are parsed.
    // `segment` works on one still image and reads the flat vectors above.
    std::vector<sam::SeedPrompt> seeds;
    int     cur_object = 0;
    int64_t cur_frame = 0;
    bool multimask = false, show_vram = false, profile = false, validate = false;
    bool overlay = false, keep_prompted = false;
    float threshold = 0.5f, nms = 0.1f;
    float dilate_ratio = 0.05f;   // sam::MaskOptions, same default
    int max_frames = 0;
    int img_size = 0;
    int detect_every = 1, memory_frames = 0, max_size = 1600;

    // The frames' colour space; they convert to sRGB before the model sees them.
    std::string image_gamut;
    std::optional<bool> image_is_linear;   // unset: an EXR's own header decides

    // `mask`
    std::string shape_spec, mask_image, preview;
    bool print_only = false, replace = false;
    app::BorderDetectOptions border;
};

// The seed the next click goes into: one per (object, frame), created on
// demand so `--point a --point b` is two clicks on one object and not two
// objects, which is the distinction that matters to the model.
sam::SeedPrompt& current_seed(Options& o) {
    for (sam::SeedPrompt& s : o.seeds)
        if (s.object == o.cur_object && s.frame == o.cur_frame) return s;
    sam::SeedPrompt s;
    s.object = o.cur_object;
    s.frame = o.cur_frame;
    o.seeds.push_back(s);
    return o.seeds.back();
}

bool parse_args(int argc, char** argv, Options& o) {
    if (argc < 2) return false;
    o.command = argv[1];
    for (int i = 2; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&](const char* what) -> const char* {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "%s\n",
                             format(cmsg::sam_flag_needs_value, {what}).c_str());
                std::exit(2);
            }
            return argv[++i];
        };
        float v[4];
        if (a == "--model") o.model = next("--model");
        else if (a == "--image") o.image = next("--image");
        else if (a == "--image-gamut") o.image_gamut = next("--image-gamut");
        else if (a == "--image-linear") o.image_is_linear = true;
        else if (a == "--no-image-linear") o.image_is_linear = false;
        else if (a == "--frames") o.frames = next("--frames");
        else if (a == "--out") o.out_dir = next("--out");
        else if (a == "--text") o.text = next("--text");
        else if (a == "--neg-text") o.neg_text = next("--neg-text");
        else if (a == "--detect-every") o.detect_every = std::atoi(next("--detect-every"));
        else if (a == "--memory-frames") o.memory_frames = std::atoi(next("--memory-frames"));
        else if (a == "--max-size") o.max_size = std::atoi(next("--max-size"));
        else if (a == "--keep-prompted") o.keep_prompted = true;
        else if (a == "--info") o.video_path = next("--info");
        else if (a == "--device") o.device = next("--device");
        else if (a == "--threshold") o.threshold = std::strtof(next("--threshold"), nullptr);
        else if (a == "--nms") o.nms = std::strtof(next("--nms"), nullptr);
        else if (a == "--dilate-ratio")
            o.dilate_ratio = std::strtof(next("--dilate-ratio"), nullptr);
        else if (a == "--max-frames") o.max_frames = std::atoi(next("--max-frames"));
        else if (a == "--img-size") o.img_size = std::atoi(next("--img-size"));
        else if (a == "--shape") o.shape_spec = next("--shape");
        else if (a == "--mask-image") o.mask_image = next("--mask-image");
        else if (a == "--preview") o.preview = next("--preview");
        else if (a == "--print") o.print_only = true;
        else if (a == "--replace") o.replace = true;
        else if (a == "--shrink") o.border.shrink = std::strtof(next("--shrink"), nullptr);
        else if (a == "--samples") o.border.samples = std::atoi(next("--samples"));
        else if (a == "--dark") o.border.dark = std::atoi(next("--dark"));
        else if (a == "--multimask") o.multimask = true;
        else if (a == "--overlay") o.overlay = true;
        else if (a == "--vram") o.show_vram = true;
        else if (a == "--profile") o.profile = true;
        else if (a == "--validate") o.validate = true;
        else if (a == "--box" && parse_floats(next("--box"), v, 4))
            o.pos_boxes.push_back({v[0], v[1], v[2], v[3]});
        else if (a == "--neg-box" && parse_floats(next("--neg-box"), v, 4))
            o.neg_boxes.push_back({v[0], v[1], v[2], v[3]});
        else if (a == "--object") {
            // Objects are numbered from 0, so the first --object opens the
            // SECOND one and a command line that never says it still works.
            ++o.cur_object;
            o.cur_frame = 0;
        } else if (a == "--at-frame") o.cur_frame = std::atoll(next("--at-frame"));
        else if (a == "--prompt-box" && parse_floats(next("--prompt-box"), v, 4)) {
            o.prompt_box = {v[0], v[1], v[2], v[3]};
            o.use_prompt_box = true;
            sam::SeedPrompt& s = current_seed(o);
            s.prompt.box = o.prompt_box;
            s.prompt.use_box = true;
        } else if (a == "--point" && parse_floats(next("--point"), v, 2)) {
            o.pos_points.push_back({v[0], v[1]});
            current_seed(o).prompt.pos_points.push_back({v[0], v[1]});
        } else if (a == "--neg-point" && parse_floats(next("--neg-point"), v, 2)) {
            o.neg_points.push_back({v[0], v[1]});
            current_seed(o).prompt.neg_points.push_back({v[0], v[1]});
        } else if (a[0] != '-' && o.command == "mask" && o.frames.empty()) {
            o.frames = a;   // `mask <images>`, the one positional this tool has
        } else {
            std::fprintf(stderr, "%s\n",
                         format(cmsg::sam_unknown_option, {a}).c_str());
            return false;
        }
    }
    return true;
}

int cmd_devices() {
    auto devices = nn::list_devices();
    if (devices.empty()) {
        std::fprintf(stderr, "%s\n", cmsg::sam_no_devices.get());
        return 1;
    }
    // Padded by display width rather than by byte count -- a heading of two Han
    // characters is four bytes and two columns wide -- and each column is at
    // least as wide as its own heading, since a translated one can be longer
    // than the values under it.
    using spirula::i18n::display_width;
    using spirula::i18n::pad_to;
    const int w_idx = std::max(3, display_width(cmsg::sam_col_index.get()));
    const int w_name = std::max(42, display_width(cmsg::sam_col_name.get()));
    const int w_type = std::max(11, display_width(cmsg::sam_col_type.get()));
    const int w_vram = std::max(8, display_width(cmsg::sam_col_vram.get()));
    std::printf("%s %s %s %s  %s\n",
                pad_to(cmsg::sam_col_index.get(), w_idx).c_str(),
                pad_to(cmsg::sam_col_name.get(), w_name).c_str(),
                pad_to(cmsg::sam_col_type.get(), w_type).c_str(),
                pad_to(cmsg::sam_col_vram.get(), w_vram).c_str(),
                cmsg::sam_col_status.get());
    for (const auto& d : devices) {
        char vram[32];
        std::snprintf(vram, sizeof vram, "%6.1f G", d.vram_bytes / 1073741824.0);
        std::printf("%s %s %s %s  %s\n",
                    pad_to(std::to_string(d.index), w_idx).c_str(),
                    pad_to(d.name, w_name).c_str(),
                    pad_to(d.type, w_type).c_str(),
                    pad_to(vram, w_vram).c_str(),
                    d.usable ? cmsg::sam_status_ok.get()
                             : d.unusable_reason.c_str());
    }
    return 0;
}

void write_results(const Options& o, const nn::Image& image, const sam::Result& r,
                   const std::string& tag) {
    std::printf("#\tid\tscore\tiou\tx0\ty0\tx1\ty1\tarea\n");
    for (size_t i = 0; i < r.detections.size(); ++i) {
        const auto& d = r.detections[i];
        size_t area = 0;
        for (uint8_t m : d.mask.data) area += (m > 127) ? 1 : 0;
        std::printf("%zu\t%d\t%.4f\t%.4f\t%.1f\t%.1f\t%.1f\t%.1f\t%zu\n", i,
                    d.instance_id, d.score, d.iou_score, d.box.x0, d.box.y0, d.box.x1,
                    d.box.y1, area);
    }
    if (o.out_dir.empty()) return;
    std::error_code ec;
    fs::create_directories(o.out_dir, ec);
    for (size_t i = 0; i < r.detections.size(); ++i) {
        char name[512];
        std::snprintf(name, sizeof name, "%s/%s_mask_%02zu.png", o.out_dir.c_str(),
                      tag.c_str(), i);
        sam::save_mask_png(r.detections[i].mask, name);
    }
    char overlay[512];
    std::snprintf(overlay, sizeof overlay, "%s/%s_overlay.png", o.out_dir.c_str(),
                  tag.c_str());
    sam::save_overlay_png(image, r, overlay);
    std::fprintf(stderr, "%s\n",
                 format(cmsg::sam_wrote_masks,
                        {(long long)r.detections.size(), o.out_dir}).c_str());
}

bool load_session(const Options& o, sam::Session& session) {
    sam::ModelParams mp;
    mp.model_path = o.model;
    mp.validation = o.validate;
    mp.profile = o.profile;
    mp.img_size = o.img_size;
    if (!o.device.empty()) {
        char* end = nullptr;
        long idx = std::strtol(o.device.c_str(), &end, 10);
        if (end && *end == '\0') mp.device_index = (int)idx;
        else mp.device_match = o.device;
    }
    if (!session.loadModel(mp)) {
        std::fprintf(stderr, "%s\n",
                     format(cmsg::error_line, {session.lastError()}).c_str());
        return false;
    }
    return true;
}

int cmd_segment(const Options& o) {
    if (o.model.empty() || o.image.empty()) {
        std::fprintf(stderr, "%s\n", cmsg::sam_segment_needs.get());
        return 2;
    }
    sam::Session session;
    if (!load_session(o, session)) return 1;

    nn::Image image = nn::load_image(o.image, o.image_gamut, o.image_is_linear);
    if (image.empty()) return 1;
    if (!session.encodeImage(image)) {
        std::fprintf(stderr, "%s\n",
                     format(cmsg::error_line, {session.lastError()}).c_str());
        return 1;
    }

    sam::Result r;
    const bool visual = !o.pos_points.empty() || !o.neg_points.empty() || o.use_prompt_box;
    if (visual) {
        sam::VisualPrompt vp;
        vp.pos_points = o.pos_points;
        vp.neg_points = o.neg_points;
        vp.box = o.prompt_box;
        vp.use_box = o.use_prompt_box;
        vp.multimask = o.multimask;
        r = session.segmentVisual(vp);
    } else {
        sam::ConceptPrompt cp;
        cp.text = o.text;
        cp.pos_exemplars = o.pos_boxes;
        cp.neg_exemplars = o.neg_boxes;
        cp.score_threshold = o.threshold;
        cp.nms_threshold = o.nms;
        r = session.segmentConcept(cp);
    }
    if (r.detections.empty() && !session.lastError().empty()) {
        std::fprintf(stderr, "%s\n",
                     format(cmsg::error_line, {session.lastError()}).c_str());
        return 1;
    }
    write_results(o, image, r, "seg");
    if (o.show_vram) std::fprintf(stderr, "VRAM:\n%s", session.vramReport().c_str());
    if (o.profile) session.printProfile();
    return 0;
}

int cmd_track(const Options& o) {
    if (o.model.empty() || o.frames.empty()) {
        std::fprintf(stderr, "%s\n", cmsg::sam_track_needs.get());
        return 2;
    }
    std::vector<std::string> files;
    std::error_code ec;
    for (const auto& e : fs::directory_iterator(o.frames, ec)) {
        if (!e.is_regular_file()) continue;
        std::string ext = e.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" ||
            ext == ".exr")
            files.push_back(e.path().string());
    }
    std::sort(files.begin(), files.end());
    if (files.empty()) {
        std::fprintf(stderr, "%s\n",
                     format(cmsg::sam_no_images_in, {o.frames}).c_str());
        return 1;
    }
    if (o.max_frames > 0 && (int)files.size() > o.max_frames)
        files.resize((size_t)o.max_frames);

    // The mask policy -- several concepts, negatives that carve back out, and
    // which side of the mask is white -- lives in sam/Masking.h, shared with
    // `extract` and with the GUI so none of the three can disagree.
    sam::MaskOptions mo;
    mo.model = o.model;
    mo.device = o.device;
    mo.text = o.text;
    mo.neg_text = o.neg_text;
    mo.keep_prompted = o.keep_prompted;
    mo.threshold = o.threshold;
    mo.nms = o.nms;
    mo.dilate_ratio = o.dilate_ratio;
    mo.detect_every = o.detect_every;
    mo.memory_frames = o.memory_frames;
    mo.max_size = o.max_size;
    mo.img_size = o.img_size;
    mo.validate = o.validate;
    mo.profile = o.profile;
    mo.seeds = o.seeds;

    sam::Masker masker;
    std::string error;
    if (!masker.init(mo, error)) {
        std::fprintf(stderr, "%s\n",
                     format(cmsg::error_line, {error}).c_str());
        return 1;
    }

    // Neither reading the next frame nor writing the last mask needs the GPU,
    // and together they are about a third of a frame -- see app/WriterPool.h.
    app::WriterPool writers;
    std::future<nn::Image> ahead;
    auto load_at = [&](size_t i) {
        return std::async(std::launch::async,
                          [p = files[i], g = o.image_gamut, l = o.image_is_linear] {
                              return nn::load_image(p, g, l);
                          });
    };
    if (!files.empty()) ahead = load_at(0);

    double t_load = 0, t_track = 0, t_write = 0;
    const double t_all = nn::now_ms();
    for (size_t f = 0; f < files.size(); ++f) {
        double t0 = nn::now_ms();
        nn::Image frame = ahead.get();
        if (f + 1 < files.size()) ahead = load_at(f + 1);
        t_load += nn::now_ms() - t0;
        if (frame.empty()) continue;

        t0 = nn::now_ms();
        sam::Mask mask;
        sam::Result r;
        if (!masker.run(frame, mask, o.overlay ? &r : nullptr, (int64_t)f)) {
            std::fprintf(stderr, "%s\n",
                         format(cmsg::error_line, {masker.lastError()}).c_str());
            return 1;
        }
        t_track += nn::now_ms() - t0;
        t0 = nn::now_ms();

        size_t white = 0;
        for (uint8_t v : mask.data) white += (v > 127) ? 1 : 0;
        {
            char pct[16];
            std::snprintf(pct, sizeof pct, "%.1f",
                          100.0 * (double)white /
                              (double)std::max<size_t>(mask.data.size(), 1));
            std::fprintf(stderr, "%s\n",
                         format(cmsg::sam_frame_line,
                                {(long long)f,
                                 fs::path(files[f]).filename().string(),
                                 pct}).c_str());
        }
        if (!o.out_dir.empty()) {
            std::error_code dec;
            fs::create_directories(o.out_dir, dec);
            char path[512];
            std::snprintf(path, sizeof path, "%s/frame_%05zu.png", o.out_dir.c_str(), f);
            if (o.overlay) {
                sam::save_overlay_png(frame, r, path);
            } else {
                app::WriteJob wj;
                wj.mask = std::move(mask);
                wj.path = path;
                writers.submit(std::move(wj));
            }
        }
        t_write += nn::now_ms() - t0;
    }
    writers.finish();
    const double total = nn::now_ms() - t_all;
    const double n = (double)files.size();
    {
        auto ms = [](double v) {
            char b[32];
            std::snprintf(b, sizeof b, "%.0f", v);
            return std::string(b);
        };
        char secs[32];
        std::snprintf(secs, sizeof secs, "%.1f", total / 1000.0);
        std::fprintf(stderr, "%s\n",
                     format(cmsg::sam_track_summary,
                            {(long long)files.size(), secs, ms(total / n),
                             ms(t_load / n), ms(t_track / n),
                             ms(t_write / n)}).c_str());
    }
    if (o.show_vram)
        std::fprintf(stderr, "VRAM:\n%s", masker.session().vramReport().c_str());
    if (o.profile) masker.session().printProfile();
    return 0;
}

// ---------------------------------------------------------------------------
// mask -- the stencil that needs no model
// ---------------------------------------------------------------------------

void write_preview(const std::string& frame, const app::FrameMask& fm,
                   const std::string& path, const std::string& gamut,
                   std::optional<bool> is_linear) {
    nn::Image img = nn::load_image(frame, gamut, is_linear);
    if (img.empty()) return;
    std::vector<uint8_t> px;
    std::string err;
    if (!app::rasterize_frame_mask(fm, img.width, img.height, px, err)) return;
    for (size_t i = 0; i < px.size(); i++) {
        if (px[i]) continue;
        uint8_t* p = &img.data[i * 3];
        p[0] = (uint8_t)((p[0] * 45 + 230 * 55) / 100);
        p[1] = (uint8_t)((p[1] * 45 + 40 * 55) / 100);
        p[2] = (uint8_t)((p[2] * 45 + 40 * 55) / 100);
    }
    if (nn::save_image(img, path, -1))
        std::fprintf(stderr, "%s\n", format(cmsg::sam_wrote_path, {path}).c_str());
}

int cmd_mask(const Options& o) {
    std::error_code ec;
    const fs::path root(o.frames);
    if (o.frames.empty() || !fs::is_directory(root, ec)) {
        std::fprintf(stderr, "%s\n", cmsg::sam_mask_needs.get());
        return 2;
    }
    if (!o.mask_image.empty()) {
        int w = 0, h = 0;
        std::vector<uint8_t> probe;
        if (!app::load_stencil(o.mask_image, w, h, probe)) {
            std::fprintf(stderr, "%s\n",
                         format(cmsg::sam_mask_bad_image, {o.mask_image}).c_str());
            return 2;
        }
    }

    app::FrameStencilRun run;
    run.image_dir = o.frames;
    run.mask_dir = o.out_dir.empty() ? (root.parent_path() / "masks").string()
                                     : o.out_dir;
    run.skip_dir = run.mask_dir;
    run.replace = o.replace;
    run.dry_run = o.print_only;
    run.detect = o.border;
    run.stencil.shrink = o.border.shrink;
    run.stencil.mask.image = o.mask_image;
    // Named shapes are the whole answer; only an unnamed one asks for a fit.
    if (!o.shape_spec.empty()) {
        std::string bad;
        if (!app::parse_mask_shapes(o.shape_spec, run.stencil.mask.shapes, bad)) {
            std::fprintf(stderr, "%s\n",
                         format(cmsg::sam_mask_bad_shape, {bad}).c_str());
            return 2;
        }
    } else {
        run.stencil.detect_border = true;
    }

    app::FrameStencilSinks sinks;
    std::string preview_left = o.preview;
    sinks.camera = [&](const std::string& rel, int64_t n) {
        std::fprintf(stderr, "%s\n",
                     format(cmsg::sam_mask_camera,
                            {rel.empty() ? o.frames : rel, n}).c_str());
    };
    sinks.resolved = [&](const std::string& rel, const app::FrameMask& fm,
                         const app::BorderDetect& d) {
        const std::string name = rel.empty() ? o.frames : rel;
        if (run.stencil.detect_border && !d.found) {
            std::fprintf(stderr, "%s\n",
                         format(cmsg::sam_mask_no_border, {name}).c_str());
        } else if (run.stencil.detect_border) {
            char pct[16];
            std::snprintf(pct, sizeof pct, "%.1f", 100.0f * d.kept_fraction);
            std::fprintf(stderr, "%s\n",
                         format(cmsg::sam_mask_found,
                                {app::format_mask_shapes(fm.shapes), pct}).c_str());
        }
        // Machine-readable, so a script can feed it back through --shape.
        if (o.print_only) {
            std::printf("%s\t%s\n", name.c_str(),
                        app::format_mask_shapes(fm.shapes).c_str());
            return;
        }
        if (!preview_left.empty() && !fm.empty()) {
            const auto groups = app::group_frames_by_camera(run.image_dir, run.skip_dir);
            const auto it = groups.find(rel);
            if (it != groups.end() && !it->second.empty())
                write_preview(it->second.front(), fm, preview_left,
                              o.image_gamut, o.image_is_linear);
            preview_left.clear();
        }
    };

    std::string error;
    const int64_t written = app::apply_frame_stencil(run, sinks, error);
    if (written < 0) {
        std::fprintf(stderr, "%s\n", format(cmsg::error_line, {error}).c_str());
        return 1;
    }
    if (!o.print_only)
        std::fprintf(stderr, "%s\n",
                     format(cmsg::sam_mask_wrote, {written, run.mask_dir}).c_str());
    return 0;
}

int cmd_video(const Options& o) {
#ifndef SS_HAVE_VIDEO
    (void)o;
    std::fprintf(stderr, "%s\n", cmsg::sam_no_video_decoder.get());
    return 1;
#else
    const std::string why = video::VideoReader::availability();
    if (why.empty())
        std::fprintf(stderr, "%s\n", cmsg::sam_video_decode.get());
    else
        std::fprintf(stderr, "%s\n",
                     format(cmsg::sam_video_decode_why, {why}).c_str());

    if (o.video_path.empty()) return why.empty() ? 0 : 1;

    video::VideoReader reader;
    if (!reader.open(o.video_path)) {
        std::fprintf(stderr, "%s\n",
                     format(cmsg::error_line, {reader.lastError()}).c_str());
        return 1;
    }
    const auto& i = reader.info();
    std::printf("codec\t%s\nwidth\t%d\nheight\t%d\nframes\t%d\nfps\t%.3f\n",
                i.codec.c_str(), i.width, i.height, i.frame_count, i.fps);

    // A decode smoke test: --max-frames N decodes the first N frames, writing
    // them as PNGs if --out is given. This is how a codec path gets compared
    // against a reference decoder.
    if (o.max_frames > 0) {
        const double t0 = nn::now_ms();
        int n = 0;
        for (; n < o.max_frames; ++n) {
            nn::Image frame = reader.readFrame();
            if (frame.empty()) {
                if (!reader.lastError().empty())
                    std::fprintf(stderr, "%s\n",
                                 format(cmsg::error_line,
                                        {reader.lastError()}).c_str());
                break;
            }
            if (o.out_dir.empty()) continue;
            char path[512];
            std::snprintf(path, sizeof(path), "%s/frame_%05d.png", o.out_dir.c_str(), n);
            nn::save_image(frame, path, -1);
            std::printf("%s\n", format(cmsg::sam_wrote_path, {path}).c_str());
        }
        const double ms = nn::now_ms() - t0;
        {
            char ms_s[32], fps[32];
            std::snprintf(ms_s, sizeof ms_s, "%.0f", ms);
            std::snprintf(fps, sizeof fps, "%.1f", n > 0 ? 1000.0 * n / ms : 0.0);
            std::printf("%s\n",
                        format(cmsg::sam_decoded, {n, ms_s, fps}).c_str());
        }
    }
    return 0;
#endif
}

}  // namespace

// Defined in src/app/cli/sam_extract.cpp; it has its own option set, so it
// parses its own argv rather than sharing Options above.
#ifdef SS_HAVE_VIDEO
int sam_cli_extract(int argc, char** argv);
#endif

int spirula_sam_main(int argc, char** argv) {
    app::set_program_name(argc > 0 ? argv[0] : nullptr, "spirula sam");
    if (argc >= 2 && std::strcmp(argv[1], "extract") == 0) {
#ifdef SS_HAVE_VIDEO
        int rc = sam_cli_extract(argc - 1, argv + 1);
        nn::shutdown();
        return rc;
#else
        std::fprintf(stderr, "%s\n",
                     format(cmsg::sam_extract_needs_decoder,
                            {app::program_name()}).c_str());
        return 1;
#endif
    }

    Options o;
    if (!parse_args(argc, argv, o)) {
        usage();
        return 2;
    }
    int rc = 2;
    if (o.command == "devices")      rc = cmd_devices();
    else if (o.command == "segment") rc = cmd_segment(o);
    else if (o.command == "track")   rc = cmd_track(o);
    else if (o.command == "mask")    rc = cmd_mask(o);
    else if (o.command == "video")   rc = cmd_video(o);
    else usage();
    nn::shutdown();
    return rc;
}
