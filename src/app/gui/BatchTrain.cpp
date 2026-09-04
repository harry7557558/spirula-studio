// BatchTrain.cpp -- see BatchTrain.h.

#include "app/gui/BatchTrain.h"
#include "i18n/catalog/Log.h"

#include "app/TrainerCore.h"
#include "app/AppPaths.h"
#include "app/gui/DatasetPrep.h"
#include "app/gui/TrainPreset.h"
#include "backend/api/BackendRuntime.h"
#include "data/Json.h"
#include "i18n/catalog/Gui.h"

#include <cctype>
#include <cstdio>
#include <filesystem>
#include <set>
#include <stdexcept>

namespace fs = std::filesystem;

namespace gui {

namespace msg = spirula::i18n::msg::gui;

namespace {

BatchIssue error_of(const spirula::i18n::Msg& m, std::string arg = {}) {
    BatchIssue i;
    i.text = &m;
    i.arg = std::move(arg);
    i.fatal = true;
    return i;
}

BatchIssue warning_of(const spirula::i18n::Msg& m, std::string arg = {}) {
    BatchIssue i = error_of(m, std::move(arg));
    i.fatal = false;
    return i;
}

// The output folder a row resolves to, which is both what gets created and
// what two rows can collide on.
std::string resolved_output(const BatchJob& job) {
    if (!job.output_dir.empty()) return job.output_dir;
    if (job.dataset.empty()) return {};
    return (fs::path(job.dataset) / "outputs").string();
}

// The nearest ancestor of `p` that exists. "" when none does, which on every
// platform this runs on means the path was relative to nothing or the drive
// is not there.
fs::path existing_ancestor(fs::path p) {
    std::error_code ec;
    for (; !p.empty(); p = p.parent_path()) {
        if (fs::exists(p, ec)) return p;
        if (p.parent_path() == p) break;   // hit the root
    }
    return {};
}

std::string quote(const std::string& v) {
    std::string s = "\"";
    for (char ch : v) {
        if (ch == '"' || ch == '\\') { s += '\\'; s += ch; }
        else if (ch == '\n' || ch == '\r') continue;
        else s += ch;
    }
    return s + "\"";
}

std::string batch_list_path() {
    return (fs::path(app::config_dir()) / "batch.json").string();
}

// Parse one of the per-row overrides. Returns false only when the text is
// there and is not a whole number in [lo, hi] -- an empty field is "use the
// preset", which is a success with `out` left alone.
bool parse_override(const std::string& text, int lo, int hi, int* out) {
    if (text.empty()) return true;
    size_t i = 0;
    while (i < text.size() && std::isspace((unsigned char)text[i])) i++;
    if (i == text.size()) return true;   // whitespace only: still unset
    size_t end = 0;
    long long v = 0;
    try {
        v = std::stoll(text.substr(i), &end);
    } catch (const std::exception&) {
        return false;
    }
    // Trailing junk ("300px") is a typo, not a number.
    for (size_t j = i + end; j < text.size(); j++)
        if (!std::isspace((unsigned char)text[j])) return false;
    if (v < lo || v > hi) return false;
    *out = (int)v;
    return true;
}

// Two rows are duplicates when they would do the same work: the same dataset
// AND the same settings. The same dataset twice is ordinary -- comparing two
// presets on one capture, or sweeping the splat budget, is exactly what a
// batch is for.
bool same_work(const BatchJob& a, const BatchJob& b) {
    return a.dataset == b.dataset && a.preset_path == b.preset_path &&
           (!a.preset_path.empty() || a.preset_name == b.preset_name) &&
           a.cap_max_override == b.cap_max_override &&
           a.sh_degree_override == b.sh_degree_override &&
           a.iterations_override == b.iterations_override;
}

}  // namespace


std::string batch_issue_line(const BatchIssue& issue) {
    if (!issue.text) return issue.raw;
    if (issue.arg.empty()) return issue.text->get();
    return spirula::i18n::format(*issue.text, {issue.arg});
}


bool batch_has_error(const BatchJob& job) {
    for (const BatchIssue& i : job.issues)
        if (i.fatal) return true;
    return false;
}


std::vector<BatchIssue> batch_check(const BatchJob& job,
                                    const std::vector<BatchJob>& all,
                                    int index) {
    std::vector<BatchIssue> out;
    std::error_code ec;

    // ---- the dataset ----
    if (job.dataset.empty()) {
        out.push_back(error_of(msg::chk_dataset_empty));
    } else if (!fs::exists(job.dataset, ec)) {
        out.push_back(error_of(msg::chk_dataset_missing, job.dataset));
    } else if (!fs::is_directory(job.dataset, ec)) {
        out.push_back(error_of(msg::chk_dataset_not_a_dir, job.dataset));
    } else if (!folder_looks_like_dataset(job.dataset)) {
        // A folder of raw photos gets here. It is not a batch's job to
        // reconstruct one, so this is the row's mistake rather than a warning.
        out.push_back(error_of(msg::chk_dataset_unreadable, job.dataset));
    }

    for (int i = 0; i < (int)all.size(); i++) {
        if (i == index || all[i].dataset.empty()) continue;
        if (same_work(all[i], job)) {
            out.push_back(warning_of(msg::chk_dataset_duplicate, job.dataset));
            break;
        }
    }

    // ---- the per-row overrides ----
    int scratch = 0;
    if (!parse_override(job.cap_max_override, 1, 1000000000, &scratch))
        out.push_back(error_of(msg::chk_bad_max_splats, job.cap_max_override));
    if (!parse_override(job.sh_degree_override, 0, 4, &scratch))
        out.push_back(error_of(msg::chk_bad_sh_degree, job.sh_degree_override));
    if (!parse_override(job.iterations_override, 1, 1000000000, &scratch))
        out.push_back(error_of(msg::chk_bad_steps, job.iterations_override));

    // ---- the preset, and the config it comes to ----
    TrainConfig cfg;
    bool have_cfg = false;
    if (job.preset_path.empty()) {
        if (!train_apply_preset(cfg, job.preset_name))
            out.push_back(error_of(msg::chk_preset_unknown, job.preset_name));
        else
            have_cfg = true;
    } else if (!fs::is_regular_file(job.preset_path, ec)) {
        out.push_back(error_of(msg::chk_preset_missing, job.preset_path));
    } else {
        try {
            cfg = load_preset(job.preset_path).cfg;
            have_cfg = true;
        } catch (const std::exception& e) {
            BatchIssue i = error_of(msg::chk_preset_unreadable, job.preset_path);
            out.push_back(i);
            BatchIssue raw;
            raw.raw = e.what();
            raw.fatal = true;
            out.push_back(raw);
        }
    }

    if (have_cfg) {
        // Everything the trainer refuses outright, asked before the queue has
        // spent an hour getting to this row.
        if (std::string what = spirula::train_config_unsupported(cfg);
            !what.empty())
            out.push_back(error_of(msg::chk_unsupported, what));

        // A missing image folder is a warning, not an error: the COLMAP and
        // Nerfstudio parsers both index images by what the reconstruction
        // names, which can be somewhere else entirely.
        if (!job.dataset.empty() && !cfg.image_dir.empty() &&
            fs::is_directory(job.dataset, ec) &&
            !fs::exists(fs::path(job.dataset) / cfg.image_dir, ec))
            out.push_back(warning_of(msg::chk_images_missing, cfg.image_dir));
    }

    // ---- where it writes ----
    const std::string out_dir = resolved_output(job);
    if (!out_dir.empty()) {
        if (fs::exists(out_dir, ec) && !fs::is_directory(out_dir, ec))
            out.push_back(error_of(msg::chk_output_is_file, out_dir));
        else if (existing_ancestor(fs::absolute(out_dir, ec)).empty())
            out.push_back(error_of(msg::chk_output_unusable, out_dir));
    }

    // ---- the machine ----
    // Asked once per row so a row-by-row report is complete, but it is the
    // same answer every time and the cheap query the status strip already
    // polls at 2 Hz.
    if (backend::device_count() <= 0 || backend::device_current() < 0)
        out.push_back(error_of(msg::chk_no_device));

    return out;
}


bool batch_build_config(const BatchJob& job, TrainConfig& cfg,
                        std::string& preset_base, std::string& error) {
    std::set<std::string> touched;
    cfg = TrainConfig();
    preset_base = job.preset_path.empty() ? job.preset_name : "3dgs";

    if (job.preset_path.empty()) {
        if (!train_apply_preset(cfg, job.preset_name)) {
            error = spirula::i18n::format(
                spirula::i18n::msg::log::err_unknown_preset,
                {job.preset_name});
            return false;
        }
    } else {
        try {
            TrainPreset p = load_preset(job.preset_path);
            cfg = p.cfg;
            touched = p.touched;
            preset_base = p.base;
        } catch (const std::exception& e) {
            error = e.what();
            return false;
        }
    }

    cfg.data = job.dataset;
    cfg.output_dir_prefix = resolved_output(job);
    // Every run gets its own timestamped subfolder, so a batch re-run never
    // writes over what the last one produced and two rows may share a folder.
    cfg.output_dir_name.clear();
    // Unattended: nothing is going to open a browser, and binding a port per
    // job is one more thing that can fail between two datasets. The native
    // viewport does not go through the web viewer, so the run is still
    // watchable if anybody is there.
    cfg.disable_viewer = true;

    // The row's own overrides go on top of the preset, and count as set by
    // hand: --quality moves cap_max and num_iterations, and a number typed
    // into the row is not something a macro gets to overwrite.
    struct { const std::string& text; int lo, hi; int* field; const char* flag; }
    overrides[] = {
        {job.cap_max_override, 1, 1000000000, &cfg.cap_max, "cap_max"},
        {job.sh_degree_override, 0, 4, &cfg.sh_degree, "sh_degree"},
        {job.iterations_override, 1, 1000000000, &cfg.num_iterations,
         "num_iterations"},
    };
    for (const auto& o : overrides) {
        int v = 0;
        if (!parse_override(o.text, o.lo, o.hi, &v)) {
            error = spirula::i18n::format(
                spirula::i18n::msg::log::err_bad_flag_value, {o.flag, o.text});
            return false;
        }
        if (o.text.find_first_not_of(" \t") == std::string::npos) continue;
        *o.field = v;
        touched.insert(o.flag);
    }

    // The same resolution order the trainer screen uses, so a row trains with
    // the values the options editor would have shown for this preset.
    train_resolve_macros(cfg, touched);
    return true;
}


std::vector<BatchJob> load_batch_list() {
    std::vector<BatchJob> out;
    try {
        JsonValue root = json_parse_file(batch_list_path());
        const JsonValue* jobs = root.find("jobs");
        if (!jobs || !jobs->is_array()) return out;
        for (const JsonValue& j : jobs->arr) {
            BatchJob b;
            if (const JsonValue* v = j.find("dataset")) b.dataset = v->as_string();
            if (const JsonValue* v = j.find("preset_path")) b.preset_path = v->as_string();
            if (const JsonValue* v = j.find("preset_name")) b.preset_name = v->as_string();
            if (const JsonValue* v = j.find("output_dir")) b.output_dir = v->as_string();
            if (const JsonValue* v = j.find("cap_max")) b.cap_max_override = v->as_string();
            if (const JsonValue* v = j.find("sh_degree")) b.sh_degree_override = v->as_string();
            if (const JsonValue* v = j.find("num_iterations"))
                b.iterations_override = v->as_string();
            if (b.preset_name.empty()) b.preset_name = "3dgs";
            out.push_back(std::move(b));
        }
    } catch (const std::exception&) {
        // No list yet, or one a crash left half-written. Starting empty is the
        // only useful answer either way.
    }
    return out;
}


void save_batch_list(const std::vector<BatchJob>& jobs) {
    FILE* f = std::fopen(batch_list_path().c_str(), "w");
    if (!f) return;
    std::fprintf(f, "{\n    \"jobs\": [");
    for (size_t i = 0; i < jobs.size(); i++) {
        const BatchJob& b = jobs[i];
        std::fprintf(f,
                     "%s\n        {\"dataset\": %s, \"preset_path\": %s, "
                     "\"preset_name\": %s, \"output_dir\": %s, "
                     "\"cap_max\": %s, \"sh_degree\": %s, "
                     "\"num_iterations\": %s}",
                     i ? "," : "", quote(b.dataset).c_str(),
                     quote(b.preset_path).c_str(), quote(b.preset_name).c_str(),
                     quote(b.output_dir).c_str(),
                     quote(b.cap_max_override).c_str(),
                     quote(b.sh_degree_override).c_str(),
                     quote(b.iterations_override).c_str());
    }
    std::fprintf(f, "%s]\n}\n", jobs.empty() ? "" : "\n    ");
    std::fclose(f);
}

}  // namespace gui
