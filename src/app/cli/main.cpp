// main.cpp -- the trainer, `spirula train`.
//
//     spirula train [<preset>] --data <dir> [--flag value ...]
//
// where <preset> is one of 3dgs (the default), 360-camera, in-the-wild,
// linear-color, synthetic, meshing, academic-baseline. Flags are the
// FLATTENED training config fields
// (--sh-degree, not --model.sh-degree); '-' and '_' are interchangeable;
// booleans take a value (--warp-to-pinhole 1). The config struct, flag
// table, and preset appliers all come from config/TrainConfig.h.
//
// The engine plumbing (dataset -> seeding -> per-step configs -> train loop)
// lives in TrainerCore.{h,cpp}, shared with the native GUI (gui/). This file
// adds only CLI parsing, --help, stdout progress printing, and the web
// viewer wiring.

#include "app/Tools.h"
#include "app/TrainerCore.h"
#include "app/webviewer/Viewer.h"
#include "checkpoint/Resume.h"
#include "i18n/catalog/Cli.h"
#include "i18n/catalog/Train.h"
#include "i18n/catalog/TrainFields.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <set>
#include <string>
#include <thread>
#include <vector>
#include "core/Env.h"

namespace fs = std::filesystem;

using namespace spirula;

namespace cmsg = spirula::i18n::msg::cli;
using spirula::i18n::format;


// ===========================================================================
// Generic flag parsing over the generated SS_CONFIG_FIELDS table
// ===========================================================================

namespace {

std::string normalize_key(std::string s) {
    while (!s.empty() && s[0] == '-') s.erase(s.begin());
    for (auto& c : s) if (c == '-') c = '_';
    return s;
}

[[noreturn]] void bad_value(const std::string& key, const std::string& val,
                            const std::string& why) {
    throw std::runtime_error("--" + key + ": invalid value '" + val + "' (" + why + ")");
}

// Single-token setters; overload resolution picks the right one per field.
void parse_token(bool& out, const std::string& key, const std::string& v) {
    if (v == "1" || v == "true" || v == "True")  { out = true;  return; }
    if (v == "0" || v == "false" || v == "False") { out = false; return; }
    bad_value(key, v, "expected 0/1/true/false");
}
void parse_token(int& out, const std::string& key, const std::string& v) {
    try { size_t p; out = std::stoi(v, &p); if (p == v.size()) return; } catch (...) {}
    bad_value(key, v, "expected an integer");
}
void parse_token(float& out, const std::string& key, const std::string& v) {
    if (v == "inf" || v == "Infinity") { out = std::numeric_limits<float>::infinity(); return; }
    try { size_t p; out = std::stof(v, &p); if (p == v.size()) return; } catch (...) {}
    bad_value(key, v, "expected a number");
}
void parse_token(std::string& out, const std::string&, const std::string& v) {
    out = (v == "none" || v == "None" || v == "null") ? "" : v;
}
template <typename T>
void parse_token(std::optional<T>& out, const std::string& key, const std::string& v) {
    if (v == "none" || v == "None" || v == "null") { out = std::nullopt; return; }
    T t{}; parse_token(t, key, v); out = t;
}

// Arity per field type (tuples consume N CLI tokens).
template <typename T> struct ArgArity { static constexpr int value = 1; };
template <typename T, size_t N> struct ArgArity<std::array<T, N>> {
    static constexpr int value = (int)N;
};

template <typename T>
void consume(T& out, const std::string& key, int argc, char** argv, int& i) {
    if (i + 1 >= argc) throw std::runtime_error("--" + key + ": missing value");
    parse_token(out, key, argv[++i]);
}
template <typename T, size_t N>
void consume(std::array<T, N>& out, const std::string& key, int argc, char** argv, int& i) {
    for (size_t k = 0; k < N; k++) {
        if (i + 1 >= argc)
            throw std::runtime_error("--" + key + ": expected " + std::to_string(N) + " values");
        parse_token(out[k], key, argv[++i]);
    }
}

// Choices validation for string fields. `choices` is "a|b|c" ('' free-form);
// a lone "none" marks an optional free-form string.
void check_choices(const std::string& value, const std::string& key, const char* choices) {
    std::string ch = choices;
    if (ch.empty() || ch == "none") return;
    std::string want = value.empty() ? "none" : value;
    size_t pos = 0;
    while (pos <= ch.size()) {
        size_t bar = ch.find('|', pos);
        std::string tok = ch.substr(pos, bar == std::string::npos ? std::string::npos : bar - pos);
        if (tok == want) return;
        if (bar == std::string::npos) break;
        pos = bar + 1;
    }
    bad_value(key, value, "expected one of: " + ch);
}
// non-string fields (template rather than C-variadic: clang rejects passing
// std::string through `...`)
template <typename T>
void check_choices(const T&, const std::string&, const char*) {}

// Returns false if the key is unknown. Records the key in `seen`, which
// --resume needs: a flag the user actually passed wins over the checkpoint's
// config, including when its value happens to equal the default.
bool set_config_field(TrainConfig& c, const std::string& key,
                      int argc, char** argv, int& i,
                      std::set<std::string>& seen) {
#define SS_TRY_SET(type, member, default_, section, tier, choices)            \
    if (key == #member) {                                                      \
        consume(c.member, key, argc, argv, i);                                 \
        check_choices(c.member, key, choices);                                 \
        seen.insert(#member);                                                  \
        return true;                                                           \
    }
    SS_CONFIG_FIELDS(SS_TRY_SET)
#undef SS_TRY_SET
    return false;
}

// ---- Help -------------------------------------------------------------------

std::string value_str(bool v)               { return v ? "1" : "0"; }
std::string value_str(int v)                { return std::to_string(v); }
std::string value_str(float v) {
    if (std::isinf(v)) return v > 0 ? "inf" : "-inf";
    char buf[32]; std::snprintf(buf, sizeof buf, "%g", v); return buf;
}
std::string value_str(const std::string& v) { return v.empty() ? "none" : v; }
template <typename T> std::string value_str(const std::optional<T>& v) {
    return v ? value_str(*v) : "none";
}
template <typename T, size_t N> std::string value_str(const std::array<T, N>& v) {
    std::string s;
    for (size_t i = 0; i < N; i++) s += (i ? " " : "") + value_str(v[i]);
    return s;
}

// ---- Help text shortening ---------------------------------------------------

// Display columns of one UTF-8 codepoint: the CJK blocks are drawn full-width
// in every terminal that can draw them at all, so a Japanese line fits half as
// many characters as an English one.
int char_columns(unsigned cp) {
    return (cp >= 0x1100 && cp <= 0x115F) ||    // Hangul jamo
           (cp >= 0x2E80 && cp <= 0xA4CF) ||    // radicals .. Yi
           (cp >= 0xAC00 && cp <= 0xD7A3) ||    // Hangul syllables
           (cp >= 0xF900 && cp <= 0xFAFF) ||    // compatibility ideographs
           (cp >= 0xFE30 && cp <= 0xFE6F) ||    // CJK compatibility forms
           (cp >= 0xFF00 && cp <= 0xFF60) ||    // full-width forms
           (cp >= 0xFFE0 && cp <= 0xFFE6) ||
           (cp >= 0x20000 && cp <= 0x3FFFD)     // ideographic planes
           ? 2 : 1;
}

// The first sentence of a help string, cut to one terminal line.
//
// Two things stop this from being substr(0, s.find(". ")): a Japanese or
// Chinese sentence ends in "。" with no space after it, and a width limit in
// bytes would both slice a multi-byte character in half and still overflow
// the line, since those characters are twice as wide as they are numerous.
std::string help_summary(const char* text, size_t max_columns = 110) {
    std::string s = text;

    struct Ender { const char* text; size_t keep; };
    static const Ender kEnders[] = {
        {". ", 1}, {"! ", 1}, {"? ", 1},        // space-separated
        {"\xe3\x80\x82", 3},                    // 。 ideographic full stop
        {"\xef\xbc\x81", 3}, {"\xef\xbc\x9f", 3},  // ！ ？ full-width
    };
    size_t at = std::string::npos, keep = 0;
    for (const Ender& e : kEnders) {
        size_t p = s.find(e.text);
        if (p != std::string::npos && (at == std::string::npos || p < at)) {
            at = p;
            keep = e.keep;
        }
    }
    if (at != std::string::npos) s.resize(at + keep);

    size_t cols = 0, i = 0;
    while (i < s.size()) {
        unsigned char c = (unsigned char)s[i];
        size_t len = c < 0x80 ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xE ? 3 : 4;
        if (i + len > s.size()) break;
        unsigned cp = c < 0x80 ? c : c & (0xFF >> (len + 1));
        for (size_t k = 1; k < len; k++) cp = (cp << 6) | (s[i + k] & 0x3F);
        size_t w = (size_t)char_columns(cp);
        if (cols + w > max_columns - 3) {
            s = s.substr(0, i) + "...";
            break;
        }
        cols += w;
        i += len;
    }
    return s;
}

// ---- Compute device listing / selection -----------------------------------

// Applies --device (index or case-sensitive name substring) through the
// backend-neutral device API, then prints the device table VkSplat-style
// (all visible devices, '*' on the one in use).
void select_and_print_devices(const std::string& requested) {
    int n = backend::device_count();
    if (n == 0) {
        if (!requested.empty())
            throw std::runtime_error("--device: no compute devices found");
        return;  // let the backend report its own error on first use
    }
    if (!requested.empty()) {
        char* end = nullptr;
        long idx = std::strtol(requested.c_str(), &end, 10);
        int want = -1;
        if (end && *end == '\0') {
            want = (int)idx;
        } else {
            for (int i = 0; i < n && want < 0; i++) {
                backend::DeviceInfo d = backend::device_info(i);
                if (d.usable &&
                    std::string(d.name).find(requested) != std::string::npos)
                    want = i;
            }
        }
        if (want < 0 || !backend::device_select(want))
            throw std::runtime_error(
                "--device " + requested +
                ": no usable device matches (see the device list printed by "
                "a run without --device)");
    }
    int cur = backend::device_current();
    std::printf("%s\n", cmsg::devices_header.get());
    for (int i = 0; i < n; i++) {
        backend::DeviceInfo d = backend::device_info(i);
        // The name and the type come from the driver; only the note after an
        // unusable row is ours to translate.
        const std::string note =
            d.usable ? std::string() : "  " + std::string(cmsg::device_unusable.get());
        std::printf("  %c [%d] %s (%s, %llu MB)%s\n", i == cur ? '*' : ' ',
                    i, d.name, d.type,
                    (unsigned long long)(d.vram_bytes >> 20), note.c_str());
    }
    std::fflush(stdout);
}

// `max_tier` is a rank into kTrainTiers: 0 lists only the flags a first run
// needs (--help), kTrainNumTiers-1 lists every one of them (--help-all).
void print_help(const char* argv0, const TrainConfig& c, int max_tier) {
    std::printf("%s\n\n", format(cmsg::train_usage, {argv0}).c_str());
    std::printf("%s\n", cmsg::train_presets_header.get());
    static_assert(sizeof(kTrainPresets) / sizeof(kTrainPresets[0]) ==
                      spirula::i18n::msg::train::kNumPresetText,
                  "config/TrainConfig.h and i18n/catalog/Train.h disagree "
                  "about how many presets there are");
    static_assert((size_t)kTrainNumSections ==
                      spirula::i18n::msg::train::kNumSectionText,
                  "config/TrainConfig.h and i18n/catalog/Train.h disagree "
                  "about how many section headings there are");
    for (const auto& p : kTrainPresets) {
        const auto* t = spirula::i18n::msg::train::preset_text(p.name);
        std::printf("  %-18s %s\n", p.name, t ? t->help->get() : "");
    }
    std::printf("\n%s\n", cmsg::train_app_flags_header.get());
    std::printf("  --device <index|name substring>\n      %s\n",
                cmsg::train_device_help.get());
    std::printf("\n%s\n", cmsg::train_flags_header.get());

    // Pass 1: how many flags each heading has left after the tier filter, so
    // a heading with nothing under it is not printed at all.
    int hidden = 0;
    int vis[kTrainNumSections] = {0};
#define SS_COUNT_HELP(type, member, default_, section, tier, choices)         \
    if (train_tier_rank(tier) <= max_tier) vis[train_section_index(section)]++; \
    else                                   hidden++;
    SS_CONFIG_FIELDS(SS_COUNT_HELP)
#undef SS_COUNT_HELP

    const char* cur_section = "";
#define SS_PRINT_HELP(type, member, default_, section, tier, choices)         \
    if (std::strcmp(cur_section, section) != 0) {                              \
        cur_section = section;                                                 \
        if (vis[train_section_index(section)]) {                               \
            const spirula::i18n::Msg* label =                                  \
                spirula::i18n::msg::train::section_label(section);              \
            std::printf("\n  [%s]\n", label ? label->get() : section);         \
        }                                                                      \
    }                                                                          \
    if (train_tier_rank(tier) <= max_tier) {                                   \
        std::string h =                                                        \
            help_summary(spirula::i18n::msg::field::member##_help.get());      \
        std::string ch = choices;                                              \
        std::string key_disp = #member;                                        \
        for (auto& ck : key_disp) if (ck == '_') ck = '-';                     \
        std::printf("  --%-38s [%s]%s%s\n      %s\n", key_disp.c_str(),        \
                    value_str(c.member).c_str(),                               \
                    (ch.empty() || ch == "none") ? "" : (" {" + ch + "}").c_str(), \
                    "", h.c_str());                                            \
    }
    SS_CONFIG_FIELDS(SS_PRINT_HELP)
#undef SS_PRINT_HELP

    if (hidden)
        std::printf("\n%s\n", format(cmsg::train_more_flags, {hidden}).c_str());
}


// Debug dump for numeric verification against the Python dataparser /
// trainer camera algebra (SS_DUMP_CAMERAS=<path> env). Full precision.
void dump_cameras_json(const char* path, const ParsedDataset& ds,
                       const PostSplitCameras& post) {
    FILE* f = std::fopen(path, "w");
    if (!f) throw std::runtime_error(std::string("cannot write ") + path);
    auto arr_f = [&](const char* k, const std::vector<float>& v) {
        std::fprintf(f, "\"%s\": [", k);
        for (size_t i = 0; i < v.size(); i++)
            std::fprintf(f, "%s%.9g", i ? "," : "", v[i]);
        std::fprintf(f, "]");
    };
    auto arr_i = [&](const char* k, const std::vector<int32_t>& v) {
        std::fprintf(f, "\"%s\": [", k);
        for (size_t i = 0; i < v.size(); i++)
            std::fprintf(f, "%s%d", i ? "," : "", v[i]);
        std::fprintf(f, "]");
    };
    std::fprintf(f, "{\n\"num_cameras\": %lld,\n\"n_post\": %lld,\n"
                 "\"train_frame_scale\": %.9g,\n\"num_points\": %lld,\n",
                 (long long)ds.num_cameras, (long long)post.n_post,
                 ds.train_frame_scale, (long long)ds.points.num());
    std::fprintf(f, "\"image_filenames\": [");
    for (size_t i = 0; i < ds.image_filenames.size(); i++)
        std::fprintf(f, "%s\"%s\"", i ? "," : "", ds.image_filenames[i].c_str());
    std::fprintf(f, "],\n");
    arr_i("camera_models", ds.camera_models);   std::fprintf(f, ",\n");
    arr_i("widths", ds.widths);                 std::fprintf(f, ",\n");
    arr_i("heights", ds.heights);               std::fprintf(f, ",\n");
    arr_f("c2w", ds.c2w);                       std::fprintf(f, ",\n");
    arr_i("K_per_camera", post.K_per_camera);   std::fprintf(f, ",\n");
    arr_i("post_offsets", post.post_offsets);   std::fprintf(f, ",\n");
    arr_f("viewmats", post.viewmats);           std::fprintf(f, ",\n");
    arr_f("intrins", post.intrins);             std::fprintf(f, ",\n");
    arr_f("dist_coeffs", post.dist_coeffs);     std::fprintf(f, ",\n");
    arr_f("input_intrins", post.input_intrins); std::fprintf(f, ",\n");
    arr_f("input_dist_coeffs", post.input_dist_coeffs); std::fprintf(f, ",\n");
    std::vector<float> t2n(ds.train_to_normalized.begin(), ds.train_to_normalized.end());
    arr_f("train_to_normalized", t2n);          std::fprintf(f, ",\n");
    std::vector<float> pts_head(ds.points.xyz.begin(),
        ds.points.xyz.begin() + std::min<size_t>(ds.points.xyz.size(), 30));
    arr_f("points_head", pts_head);
    std::fprintf(f, "\n}\n");
    std::fclose(f);
}

}  // namespace


// ===========================================================================
// main
// ===========================================================================

int spirula_train_main(int argc, char** argv) {
    try {
        // ---- Preset + flags ------------------------------------------------
        std::string preset = "3dgs";
        std::string preset_arg;        // "" unless the user named one
        int argi = 1;
        if (argi < argc && argv[argi][0] != '-') preset = preset_arg = argv[argi++];

        TrainConfig cfg;
        std::set<std::string> seen;
        if (!train_apply_preset(cfg, preset)) {
            std::string names;
            for (const auto& p : kTrainPresets) names += std::string(" ") + p.name;
            throw std::runtime_error("unknown preset '" + preset + "'; expected one of:" + names);
        }

        std::string device_flag;
        for (int i = argi; i < argc; i++) {
            std::string arg = argv[i];
            if (arg == "--help" || arg == "-h") {
                print_help(argv[0], cfg, 0);
                return 0;
            }
            if (arg == "--help-all" || arg == "--help_all") {
                print_help(argv[0], cfg, kTrainNumTiers - 1);
                return 0;
            }
            if (arg.rfind("--", 0) != 0)
                throw std::runtime_error("unexpected argument: " + arg + " (flags are --key value)");
            // App-level flag, not part of the generated training config.
            if (arg.rfind("--device=", 0) == 0) { device_flag = arg.substr(9); continue; }
            if (arg == "--device") {
                if (i + 1 >= argc) throw std::runtime_error("--device: missing value");
                device_flag = argv[++i];
                continue;
            }
            // --key=value form: re-parse via a 2-token mini-argv. Tuple
            // fields need N separate tokens, so = only supports arity 1.
            std::string key = arg;
            size_t eq = key.find('=');
            if (eq != std::string::npos) {
                static std::vector<std::string> keep;   // token storage
                keep.push_back(key.substr(eq + 1));
                key = key.substr(0, eq);
                char* mini[2] = {const_cast<char*>(""), keep.back().data()};
                int mi = 0;
                if (!set_config_field(cfg, normalize_key(key), 2, mini, mi, seen))
                    throw std::runtime_error("unknown flag: " + key);
                continue;
            }
            if (!set_config_field(cfg, normalize_key(key), argc, argv, i, seen))
                throw std::runtime_error("unknown flag: " + key +
                                         " (see --help for the full list)");
        }

        // ---- Macro options -------------------------------------------------
        // --quality and friends stand in for a handful of flags each; they
        // fill in only the ones this command line left alone. What they write
        // joins `seen`, so a macro passed here still beats a --resume base.
        train_resolve_macros(cfg, seen, &seen);

        // ---- Resume --------------------------------------------------------
        // The checkpoint's config.json becomes the base: it carries the
        // architecture and data config the saved engine state was built for.
        // Everything the user named on this command line is layered back on.
        if (!cfg.resume.empty()) {
            cfg = ckpt::build_resume_config(cfg, preset_arg, seen);
            std::printf("%s\n", format(cmsg::resume_target, {cfg.resume}).c_str());
        }

#define SS_CHECK_REQUIRED(member)                                          \
        if (cfg.member.empty())                                                \
            throw std::runtime_error("--" #member " is required");
        SS_CONFIG_REQUIRED_FIELDS(SS_CHECK_REQUIRED)
#undef SS_CHECK_REQUIRED

        select_and_print_devices(device_flag);

        // ---- Session -------------------------------------------------------
        TrainerSession session;
        session.cfg = cfg;
        session.preset = preset;

        session.check_config();
        session.load_dataset();

        // Hidden debug flag (set via env to avoid polluting the config):
        // dump parsed + post-split arrays as JSON and exit, for numeric
        // verification against the Python dataparser/trainer algebra.
        if (const char* dump = spirula::env("DUMP_CAMERAS")) {
            dump_cameras_json(dump, session.ds, session.post);
            std::printf("Dumped cameras to %s\n", dump);
            return 0;
        }

        session.setup_engine();

        // ---- Web viewer ------------------------------------------------------
        ViewerServer viewer;
        bool viewer_on = !cfg.disable_viewer;
        if (viewer_on) {
            viewer.start("0.0.0.0", cfg.viewer_port,
                         session.make_viewer_config(), session.make_viewer_hooks(),
                         session.post);
            std::printf("%s\n", format(cmsg::viewer_at,
                                       {cfg.viewer_port, cfg.viewer_port,
                                        cfg.viewer_port}).c_str());
        }

        // ---- Train loop ------------------------------------------------------
        TrainerCallbacks cb;
        cb.on_step = [&](const TrainerProgress& p) {
            if (p.step % 100 == 0 || p.step == p.total_steps - 1) {
                // step + 1 is steps completed, as in the GUI. Both numbers
                // are padded here, not by the message, so they keep their
                // column while the words around them change length.
                char step[16], pct[16];
                std::snprintf(step, sizeof step, "%6d", p.step + 1);
                std::snprintf(pct, sizeof pct, "%3d",
                              100 * (p.step + 1) / p.total_steps);
                std::printf("%s", format(cmsg::train_step_line,
                                         {step, p.total_steps, pct,
                                          (long long)p.num_splats,
                                          format_duration(session.elapsed_seconds()),
                                          format_duration(session.eta_seconds())}).c_str());
                for (const char* k : {"rgb_loss", "ssim", "psnr"}) {
                    auto it = p.losses.find(k);
                    if (it != p.losses.end()) std::printf("  %s=%.4g", k, it->second);
                }
                std::printf("\n");
                std::fflush(stdout);
            }
        };
        session.train(cb);
        // Held-out eval. Replaces the engine's DataManager, so nothing may
        // train afterwards -- and the viewer, which only reads splats, is
        // unaffected. TODO: early stopping on the validation split.
        session.eval();

        if (viewer_on && cfg.keep_viewer_alive) {
            std::printf("%s\n", cmsg::train_done_viewer.get());
            std::fflush(stdout);
            for (;;) std::this_thread::sleep_for(std::chrono::seconds(3600));
        }

    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", format(cmsg::error_line, {e.what()}).c_str());
        return 1;
    }
    return 0;
}
