// BatchProcess.cpp -- see BatchProcess.h.

#include "app/gui/BatchProcess.h"

#include "app/gui/StencilPreset.h"

#include "app/AppPaths.h"
#include "app/TrainerCore.h"
#include "app/gui/SourceList.h"
#include "app/gui/TrainPreset.h"
#include "checkpoint/SplatPly.h"
#include "data/Json.h"
#include "data/JsonWrite.h"
#include "i18n/catalog/Gui.h"
#include "i18n/catalog/Log.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <set>
#include <stdexcept>

namespace fs = std::filesystem;

namespace gui {

namespace msg = spirula::i18n::msg::gui;

namespace {

BatchIssue issue_of(const spirula::i18n::Msg& m, BatchStage stage, bool fatal,
                    std::string arg = {}) {
    BatchIssue i;
    i.text = &m;
    i.arg = std::move(arg);
    i.fatal = fatal;
    i.stage = stage;
    return i;
}

std::string batch_list_path() {
    return (fs::path(app::config_dir()) / "batch.json").string();
}

// The nearest ancestor of `p` that exists. "" when none does, which on every
// platform this runs on means the drive is not there.
fs::path existing_ancestor(fs::path p) {
    std::error_code ec;
    for (; !p.empty(); p = p.parent_path()) {
        if (fs::exists(p, ec)) return p;
        if (p.parent_path() == p) break;   // hit the root
    }
    return {};
}

// Parse one of the per-run overrides. Returns false only when the text is
// there and is not a whole number in [lo, hi] -- an empty field is "use the
// preset", which is a success with `out` left alone.
bool parse_override(const std::string& text, int lo, int hi, int* out) {
    size_t i = 0;
    while (i < text.size() && std::isspace((unsigned char)text[i])) i++;
    if (i == text.size()) return true;   // empty or whitespace: still unset
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

bool is_set(const std::string& text) {
    return text.find_first_not_of(" \t") != std::string::npos;
}

// Which built-in a row's Dataset stage runs on. A row written before there
// were any names none, and the base one is what it always had.
std::string builtin_dataset_name(const BatchRow& row) {
    return row.dataset_preset.name.empty() ? std::string("general")
                                           : row.dataset_preset.name;
}

// Does this model carry its own dataset? The same resolution the mesh child
// does: `data` out of the run's config.json, relative to the run folder. A
// path that is not a checkpoint at all simply has none.
bool model_records_dataset(const std::string& model) {
    if (model.empty()) return false;
    std::error_code ec;
    try {
        auto [ply, run_dir] = spirula::find_splat_ply(model);
        (void)ply;
        const fs::path cfg = fs::path(run_dir) / "config.json";
        if (!fs::is_regular_file(cfg, ec)) return false;
        const JsonValue run_cfg = json_parse_file(cfg.string());
        const JsonValue* d = run_cfg.find("data");
        if (!d || d->is_null()) return false;
        fs::path cand = d->as_string();
        if (cand.is_relative()) cand = fs::path(run_dir) / cand;
        return fs::exists(cand, ec);
    } catch (const std::exception&) {
        return false;
    }
}

// The row's own colours and formats over whatever the preset said. An empty
// set is not a choice to write nothing; it is "leave the preset alone".
void apply_mesh_overrides(const BatchMeshOptions& opt, MeshJob& job) {
    if (opt.colors)
        for (int i = 0; i < kNumMeshColorModes; i++)
            job.colors[i] = (opt.colors & (1 << i)) != 0;
    if (opt.formats)
        for (int i = 0; i < kNumMeshFormats; i++)
            job.formats[i] = (opt.formats & (1 << i)) != 0;
}

// The three overrides, in one place so the check and the builder cannot
// disagree about what a legal value is.
struct OverrideSpec {
    const std::string& text;
    int lo, hi;
    const char* flag;
    int TrainConfig::* field;
};
std::vector<OverrideSpec> overrides_of(const BatchRun& run) {
    return {{run.cap_max, 1, 1000000000, "cap_max", &TrainConfig::cap_max},
            {run.sh_degree, 0, 4, "sh_degree", &TrainConfig::sh_degree},
            {run.iterations, 1, 1000000000, "num_iterations",
             &TrainConfig::num_iterations}};
}

// The training config a preset reference resolves to, plus the built-in name
// to record in the run's config.json.
bool config_of(const BatchPreset& p, TrainConfig& cfg,
               std::set<std::string>& touched, std::string& base,
               std::string& error) {
    cfg = TrainConfig();
    if (p.path.empty()) {
        base = p.name.empty() ? "3dgs" : p.name;
        if (!train_apply_preset(cfg, base)) {
            error = spirula::i18n::format(
                spirula::i18n::msg::log::err_unknown_preset, {base});
            return false;
        }
        return true;
    }
    try {
        TrainPreset tp = load_preset(p.path);
        cfg = tp.cfg;
        touched = tp.touched;
        base = tp.base;
    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }
    return true;
}

// Two rows do the same work when they train the same dataset with the same
// settings. The same dataset twice is ordinary -- comparing two presets on one
// capture is exactly what a batch is for.
bool same_run(const BatchRun& a, const BatchRun& b) {
    if (a.preset.path != b.preset.path) return false;
    if (a.preset.path.empty() && a.preset.name != b.preset.name) return false;
    return a.cap_max == b.cap_max && a.sh_degree == b.sh_degree &&
           a.iterations == b.iterations;
}

bool same_train_work(const BatchRow& a, const BatchRow& b) {
    if (a.dataset != b.dataset || a.dataset.empty()) return false;
    if (a.runs.size() != b.runs.size()) return false;
    for (size_t i = 0; i < a.runs.size(); i++)
        if (!same_run(a.runs[i], b.runs[i])) return false;
    return true;
}

// ---- the JSON shape of one row -------------------------------------------

void write_preset(JsonWriter& w, const char* key, const BatchPreset& p) {
    w.key(key).object();
    w.field("path", p.path);
    w.field("name", p.name);
    w.end();
}

BatchPreset read_preset(const JsonValue* v) {
    BatchPreset p;
    if (!v || !v->is_object()) return p;
    if (const JsonValue* s = v->find("path")) p.path = s->as_string();
    if (const JsonValue* s = v->find("name")) p.name = s->as_string();
    return p;
}

// The two override sets, as the tokens the mesher spells rather than as bits:
// a queue file is meant to be readable, and a bit that shifted would quietly
// mean a different format.
void write_mask(JsonWriter& w, const char* key, int mask,
                const char* const* names, int n) {
    if (!mask) return;
    w.key(key).array();
    for (int i = 0; i < n; i++)
        if (mask & (1 << i)) w.value(names[i]);
    w.end();
}

int read_mask(const JsonValue* v, const char* const* names, int n) {
    int mask = 0;
    if (!v || !v->is_array()) return 0;
    for (const JsonValue& e : v->arr)
        for (int i = 0; i < n; i++)
            if (e.as_string() == names[i]) mask |= 1 << i;
    return mask;
}

}  // namespace


std::string batch_issue_line(const BatchIssue& issue) {
    if (!issue.text) return issue.raw;
    if (issue.arg.empty()) return issue.text->get();
    return spirula::i18n::format(*issue.text, {issue.arg});
}


bool batch_has_error(const std::vector<BatchIssue>& issues) {
    for (const BatchIssue& i : issues)
        if (i.fatal) return true;
    return false;
}


int batch_num_runs(const BatchRow& row) {
    return row.runs.empty() ? 1 : (int)row.runs.size();
}


BatchRun batch_run_of(const BatchRow& row, int variant) {
    if (variant >= 0 && variant < (int)row.runs.size())
        return row.runs[(size_t)variant];
    return BatchRun{};
}


int batch_num_meshes(const BatchRow& row) {
    if (!row.does(BatchStage::Train)) return 1;   // the model it was given
    int n = 0;
    for (int k = 0; k < batch_num_runs(row); k++)
        n += batch_run_of(row, k).mesh ? 1 : 0;
    return n;
}


std::vector<BatchTask> batch_plan(const std::vector<BatchRow>& rows) {
    std::vector<BatchTask> out;
    for (int i = 0; i < (int)rows.size(); i++) {
        const BatchRow& r = rows[i];
        if (!r.enabled) continue;
        if (r.does(BatchStage::Dataset))
            out.push_back({i, BatchStage::Dataset, 0});
        const bool trains = r.does(BatchStage::Train);
        const int runs = trains ? batch_num_runs(r) : 0;
        for (int k = 0; k < runs; k++)
            out.push_back({i, BatchStage::Train, k});
        // One mesh per run the row marked for it -- the big appearance run and
        // the cheap one trained beside it are not both worth meshing -- and
        // one for a row that was handed a model instead.
        if (r.does(BatchStage::Mesh)) {
            if (!trains) {
                out.push_back({i, BatchStage::Mesh, 0});
            } else {
                for (int k = 0; k < runs; k++)
                    if (batch_run_of(r, k).mesh)
                        out.push_back({i, BatchStage::Mesh, k});
            }
        }
    }
    return out;
}


BatchProgress batch_progress(const std::vector<BatchTask>& tasks, int current,
                             double frac, double elapsed) {
    BatchProgress p;
    p.total = (int)tasks.size();
    // What a finished task of each stage took, which is the only honest thing
    // to estimate an unstarted one of the same stage with.
    double stage_sum[kNumBatchStages] = {0, 0, 0}, all_sum = 0;
    int stage_n[kNumBatchStages] = {0, 0, 0}, all_n = 0;
    for (const BatchTask& t : tasks) {
        if (t.status == BatchStatus::Running) p.running++;
        if (t.status != BatchStatus::Pending &&
            t.status != BatchStatus::Running)
            p.done++;
        if (t.status != BatchStatus::Done || t.seconds <= 0.0) continue;
        stage_sum[(int)t.stage] += t.seconds;
        stage_n[(int)t.stage]++;
        all_sum += t.seconds;
        all_n++;
    }
    auto guess = [&](BatchStage s) -> double {
        if (stage_n[(int)s] > 0) return stage_sum[(int)s] / stage_n[(int)s];
        return all_n > 0 ? all_sum / all_n : -1.0;
    };

    double total = 0.0;
    bool known = true;
    for (int i = 0; i < (int)tasks.size(); i++) {
        const BatchTask& t = tasks[(size_t)i];
        if (i == current) {
            // Its own runner knows best; a stage average is the fallback, and
            // a task cannot be estimated to finish before now.
            double rem = -1.0;
            if (frac > 0.02 && frac < 1.0) rem = elapsed * (1.0 - frac) / frac;
            else if (const double g = guess(t.stage); g > 0.0)
                rem = std::max(g - elapsed, 0.0);
            p.task_remaining = rem;
            if (rem < 0.0) known = false;
            else total += rem;
            continue;
        }
        if (t.status != BatchStatus::Pending) continue;
        const double g = guess(t.stage);
        if (g <= 0.0) known = false;
        else total += g;
    }
    p.remaining = known ? total : -1.0;
    return p;
}


std::string batch_dataset_workspace(const BatchRow& row) {
    if (!row.dataset.empty()) return row.dataset;
    if (row.sources.empty()) return {};
    std::vector<PrepInput> probe;
    for (const std::string& p : row.sources) probe.push_back(make_source(p, true));
    return default_workspace(probe);
}


// ---------------------------------------------------------------------------
// The pre-flight
// ---------------------------------------------------------------------------

namespace {

void check_dataset_stage(const BatchRow& row, const BatchCapabilities& caps,
                         std::vector<BatchIssue>& out) {
    constexpr BatchStage kSt = BatchStage::Dataset;
    std::error_code ec;

    if (row.sources.empty()) {
        out.push_back(issue_of(msg::chk_sources_empty, kSt, true));
    } else {
        for (const std::string& p : row.sources) {
            if (p.empty() || !fs::exists(p, ec)) {
                out.push_back(issue_of(msg::chk_source_missing, kSt, true, p));
            } else if (fs::is_directory(p, ec)) {
                if (!folder_has_images(p))
                    out.push_back(issue_of(msg::chk_source_no_images, kSt, true, p));
            } else if (!is_video_path(p)) {
                out.push_back(issue_of(msg::chk_source_unsupported, kSt, true, p));
            }
        }
    }

    DatasetSettings s;
    if (!row.dataset_preset.path.empty()) {
        if (!fs::is_regular_file(row.dataset_preset.path, ec)) {
            out.push_back(issue_of(msg::chk_preset_missing, kSt, true,
                                   row.dataset_preset.path));
            return;
        }
        try {
            s = load_dataset_preset(row.dataset_preset.path).s;
        } catch (const std::exception& e) {
            out.push_back(issue_of(msg::chk_preset_unreadable, kSt, true,
                                   row.dataset_preset.path));
            BatchIssue raw;
            raw.raw = e.what();
            raw.fatal = true;
            raw.stage = kSt;
            out.push_back(raw);
            return;
        }
    } else if (!dataset_apply_preset(s, builtin_dataset_name(row))) {
        out.push_back(issue_of(msg::chk_preset_unknown, kSt, true,
                               row.dataset_preset.name));
        return;
    }

    const bool have_engine = s.colmap_engine ? caps.colmap : caps.builtin_sfm;
    if (!have_engine) out.push_back(issue_of(msg::chk_engine_unavailable, kSt, true));

    if (s.sfm.prep.mask_enable) {
        // A batch has no clicks -- they belong to the frames they were drawn
        // on and a preset cannot carry them -- so the text prompt is the only
        // prompt there is.
        const bool prompted = !caps.mask_model_prompted || caps.mask_model_prompted(s.mask_model_id);
        if (prompted && s.mask.prompt.empty())
            out.push_back(issue_of(msg::chk_mask_no_prompt, kSt, true));
        if (s.sfm.prep.force_external_masking) {
            // mask.py resolves its own model by name; nothing here can say
            // whether it is there.
        } else if (!caps.masking) {
            out.push_back(issue_of(msg::chk_masking_unavailable, kSt, true));
        } else if (caps.mask_model_ready &&
                   !caps.mask_model_ready(s.mask_model_id, s.mask_detector_id)) {
            out.push_back(issue_of(msg::chk_mask_model_missing, kSt, true,
                                   s.mask_model_id));
        }
    }

    if (s.sfm.geometry.enable) {
        if (!caps.geometry)
            out.push_back(issue_of(msg::chk_geometry_unavailable, kSt, true));
        else if (caps.geometry_model_ready &&
                 !caps.geometry_model_ready(s.sfm.geometry.model))
            out.push_back(issue_of(msg::chk_geometry_model_missing, kSt, true,
                                   s.sfm.geometry.model));
    }

    // A preset made for photographs, pointed at a video (or the other way
    // round): it still runs, and it picks the wrong pairing strategy.
    if (!row.sources.empty() && !row.dataset_preset.path.empty()) {
        bool any_video = false, any_photos = false;
        for (const std::string& p : row.sources) {
            const bool vid = !fs::is_directory(p, ec) && is_video_path(p);
            any_video = any_video || vid;
            any_photos = any_photos || !vid;
        }
        if (s.sfm.data_type == 1 && !any_video && any_photos)
            out.push_back(issue_of(msg::chk_capture_is_photos, kSt, false));
        if (s.sfm.data_type != 1 && any_video && !any_photos)
            out.push_back(issue_of(msg::chk_capture_is_video, kSt, false));
    }

    const std::string ws = batch_dataset_workspace(row);
    if (ws.empty()) {
        out.push_back(issue_of(msg::chk_workspace_empty, kSt, true));
    } else if (fs::exists(ws, ec) && !fs::is_directory(ws, ec)) {
        out.push_back(issue_of(msg::chk_output_is_file, kSt, true, ws));
    } else if (existing_ancestor(fs::absolute(ws, ec)).empty()) {
        out.push_back(issue_of(msg::chk_output_unusable, kSt, true, ws));
    } else if (folder_looks_like_dataset(ws)) {
        // A run left to itself REUSES a model rather than spending an hour
        // rebuilding one, which is right for adding masks and wrong for
        // someone who expected a fresh reconstruction.
        out.push_back(issue_of(msg::chk_dataset_has_model, kSt, false, ws));
    }
}

void check_train_stage(const BatchRow& row, const BatchCapabilities& caps,
                       bool made_here, std::vector<BatchIssue>& out) {
    constexpr BatchStage kSt = BatchStage::Train;
    std::error_code ec;

    if (!made_here) {
        if (row.dataset.empty())
            out.push_back(issue_of(msg::chk_dataset_empty, kSt, true));
        else if (!fs::exists(row.dataset, ec))
            out.push_back(issue_of(msg::chk_dataset_missing, kSt, true, row.dataset));
        else if (!fs::is_directory(row.dataset, ec))
            out.push_back(issue_of(msg::chk_dataset_not_a_dir, kSt, true, row.dataset));
        else if (!folder_looks_like_dataset(row.dataset))
            out.push_back(issue_of(msg::chk_dataset_unreadable, kSt, true, row.dataset));
    }

    const spirula::i18n::Msg* bad[] = {&msg::chk_bad_max_splats,
                                       &msg::chk_bad_sh_degree,
                                       &msg::chk_bad_steps};
    for (int k = 0; k < batch_num_runs(row); k++) {
        const BatchRun run = batch_run_of(row, k);
        int scratch = 0;
        const std::vector<OverrideSpec> specs = overrides_of(run);
        for (size_t i = 0; i < specs.size(); i++)
            if (!parse_override(specs[i].text, specs[i].lo, specs[i].hi, &scratch))
                out.push_back(issue_of(*bad[i], kSt, true, specs[i].text));

        const BatchPreset& p = run.preset;
        TrainConfig cfg;
        std::set<std::string> touched;
        std::string base, error;
        if (!p.path.empty() && !fs::is_regular_file(p.path, ec)) {
            out.push_back(issue_of(msg::chk_preset_missing, kSt, true, p.path));
            continue;
        }
        if (!config_of(p, cfg, touched, base, error)) {
            out.push_back(issue_of(p.path.empty() ? msg::chk_preset_unknown
                                                  : msg::chk_preset_unreadable,
                                   kSt, true, p.path.empty() ? p.name : p.path));
            continue;
        }
        // Everything the trainer refuses outright, asked before the queue has
        // spent an hour getting to this row.
        if (std::string what = spirula::train_config_unsupported(cfg); !what.empty())
            out.push_back(issue_of(msg::chk_unsupported, kSt, true, what));
        // A missing image folder is a warning, not an error: the COLMAP and
        // Nerfstudio parsers both index images by what the reconstruction
        // names, which can be somewhere else entirely.
        if (!made_here && !row.dataset.empty() && !cfg.image_dir.empty() &&
            fs::is_directory(row.dataset, ec) &&
            !fs::exists(fs::path(row.dataset) / cfg.image_dir, ec))
            out.push_back(issue_of(msg::chk_images_missing, kSt, false, cfg.image_dir));
    }

    const std::string dir = row.output_dir.empty() && !row.dataset.empty()
                                ? (fs::path(row.dataset) / "outputs").string()
                                : row.output_dir;
    if (!dir.empty()) {
        if (fs::exists(dir, ec) && !fs::is_directory(dir, ec))
            out.push_back(issue_of(msg::chk_output_is_file, kSt, true, dir));
        else if (!made_here && existing_ancestor(fs::absolute(dir, ec)).empty())
            out.push_back(issue_of(msg::chk_output_unusable, kSt, true, dir));
    }

    if (!caps.device) out.push_back(issue_of(msg::chk_no_device, kSt, true));
}

void check_mesh_stage(const BatchRow& row, std::vector<BatchIssue>& out) {
    constexpr BatchStage kSt = BatchStage::Mesh;
    std::error_code ec;

    if (row.model.empty() && !row.does(BatchStage::Train)) {
        out.push_back(issue_of(msg::chk_mesh_no_model, kSt, true));
    } else if (!row.model.empty() && !fs::exists(row.model, ec)) {
        out.push_back(issue_of(msg::chk_mesh_model_missing, kSt, true, row.model));
    }
    // Meshing is on, and every run was unticked for it: the stage would run
    // over nothing at all.
    if (row.model.empty() && row.does(BatchStage::Train) &&
        batch_num_meshes(row) == 0)
        out.push_back(issue_of(msg::chk_mesh_no_runs, kSt, false));

    MeshJob job;
    if (!row.mesh.preset.path.empty()) {
        if (!fs::is_regular_file(row.mesh.preset.path, ec)) {
            out.push_back(issue_of(msg::chk_preset_missing, kSt, true,
                                   row.mesh.preset.path));
            return;
        }
        try {
            job = load_mesh_preset(row.mesh.preset.path).job;
        } catch (const std::exception& e) {
            out.push_back(issue_of(msg::chk_preset_unreadable, kSt, true,
                                   row.mesh.preset.path));
            BatchIssue raw;
            raw.raw = e.what();
            raw.fatal = true;
            raw.stage = kSt;
            out.push_back(raw);
            return;
        }
    }
    // A colour and a format that cannot travel together, chosen on the row
    // rather than in the preset: the run would write nothing at all.
    apply_mesh_overrides(row.mesh, job);
    if (mesh_job_writes_nothing(job))
        out.push_back(issue_of(msg::chk_mesh_no_output, kSt, true));
    // Meshing without cameras is a much rougher mesh, and a row that neither
    // names a dataset nor builds one gets exactly that -- unless the model it
    // was handed records one itself, which is what a run folder does.
    if (job.use_data && row.dataset.empty() && !row.does(BatchStage::Dataset) &&
        !row.does(BatchStage::Train) && !model_records_dataset(row.model))
        out.push_back(issue_of(msg::chk_mesh_no_dataset, kSt, false));
}

}  // namespace


std::vector<BatchIssue> batch_check_row(const BatchRow& row,
                                        const std::vector<BatchRow>& all,
                                        int index,
                                        const BatchCapabilities& caps) {
    std::vector<BatchIssue> out;
    if (!row.enabled) return out;

    const bool anything = row.does(BatchStage::Dataset) ||
                          row.does(BatchStage::Train) || row.does(BatchStage::Mesh);
    if (!anything) {
        out.push_back(issue_of(msg::chk_nothing_to_do, BatchStage::Dataset, false));
        return out;
    }

    if (row.does(BatchStage::Dataset)) check_dataset_stage(row, caps, out);
    if (row.does(BatchStage::Train))
        check_train_stage(row, caps, row.does(BatchStage::Dataset), out);
    if (row.does(BatchStage::Mesh)) check_mesh_stage(row, out);

    // ---- what the LIST says, rather than the row ----
    const std::string ws = row.does(BatchStage::Dataset)
                               ? batch_dataset_workspace(row) : std::string();
    for (int i = 0; i < (int)all.size(); i++) {
        if (i == index || !all[i].enabled) continue;
        const BatchRow& other = all[i];
        // Two rows building a dataset into one folder would fight over it.
        if (!ws.empty() && other.does(BatchStage::Dataset) &&
            batch_dataset_workspace(other) == ws) {
            out.push_back(issue_of(msg::chk_dataset_collision,
                                   BatchStage::Dataset, true, ws));
            break;
        }
    }
    for (int i = 0; i < (int)all.size(); i++) {
        if (i == index || !all[i].enabled) continue;
        if (row.does(BatchStage::Train) && !row.does(BatchStage::Dataset) &&
            same_train_work(row, all[i]) && all[i].does(BatchStage::Train)) {
            out.push_back(issue_of(msg::chk_dataset_duplicate, BatchStage::Train,
                                   false, row.dataset));
            break;
        }
    }
    // A row training a dataset a LATER row builds runs before it exists.
    if (row.does(BatchStage::Train) && !row.does(BatchStage::Dataset) &&
        !row.dataset.empty()) {
        for (int i = index + 1; i < (int)all.size(); i++) {
            if (!all[i].enabled || !all[i].does(BatchStage::Dataset)) continue;
            if (batch_dataset_workspace(all[i]) != row.dataset) continue;
            out.push_back(issue_of(msg::chk_dataset_made_later, BatchStage::Train,
                                   false, row.dataset));
            break;
        }
    }
    return out;
}


// ---------------------------------------------------------------------------
// What each stage runs with
// ---------------------------------------------------------------------------

bool batch_build_dataset_job(const BatchRow& row, const std::string& ffmpeg_exe,
                             std::vector<PrepInput>& sources,
                             DatasetSettings& settings, std::string& workspace,
                             std::string& error) {
    sources.clear();
    settings = DatasetSettings{};
    const bool from_file = !row.dataset_preset.path.empty();
    if (from_file) {
        try {
            settings = load_dataset_preset(row.dataset_preset.path).s;
        } catch (const std::exception& e) {
            error = e.what();
            return false;
        }
    }
    for (const std::string& p : row.sources)
        sources.push_back(make_source(p, settings.use_found_masks));
    probe_sources(sources, ffmpeg_exe);

    if (!from_file) {
        // The capture's own answers first and the built-in over them, so a
        // preset decides the "how" and the capture keeps the parts only it
        // knows -- then the built-in gets to ask the frames themselves.
        const std::string name = builtin_dataset_name(row);
        apply_capture_defaults(sources, settings.sfm, settings.colmap);
        if (!dataset_apply_preset(settings, name)) {
            error = "unknown dataset preset: " + name;
            return false;
        }
        dataset_adapt_preset(name, sources, settings.sfm, settings.colmap,
                             ffmpeg_exe);
    } else {
        // A preset decides everything the capture does not, and what the
        // capture does decide is the sphere to warp and the lens of its
        // views: a rectilinear model on a fisheye clip reconstructs nothing.
        DatasetSettings capture;
        apply_capture_defaults(sources, capture.sfm, capture.colmap);
        app::Pano360Options& pano = settings.sfm.prep.pano;
        if (!any_pano360(sources)) {
            pano.mode = app::Pano360Mode::Off;
        } else {
            if (pano.mode == app::Pano360Mode::Off)
                pano.mode = capture.sfm.prep.pano.mode;
            if (pano.size <= 0) reset_pano_size(sources, pano);
            settings.sfm.camera_mode = capture.sfm.camera_mode;
            settings.colmap.camera_mode = capture.colmap.camera_mode;
        }
    }
    // The screen draws these on each input; a batch row has only the preset.
    if (settings.border_enable) {
        std::vector<app::MaskShape> shapes;
        if (!settings.frame_shapes.empty() &&
            !load_stencil_preset(settings.frame_shapes, shapes, error)) {
            error = "drawn areas not found: " + error;
            return false;
        }
        for (PrepInput& in : sources) {
            in.stencil.detect_border = true;
            in.stencil.mask.shapes = shapes;
        }
    }
    resolve_source_lenses(sources, settings.sfm, settings.colmap);
    assign_source_subdirs(sources);
    refresh_subcameras(sources);
    guess_source_rigs(sources, /*force=*/false);
    normalize_source_lenses(sources, settings.sfm.camera_model);

    workspace = batch_dataset_workspace(row);
    if (workspace.empty()) {
        error = "no output folder for this row";
        return false;
    }
    return true;
}


bool batch_build_train_config(const BatchRow& row, int variant,
                              const std::string& dataset,
                              const std::string& image_dir,
                              const std::string& mask_dir, bool mask_flipped,
                              TrainConfig& cfg, std::string& preset_base,
                              std::string& error) {
    const BatchRun run = batch_run_of(row, variant);

    std::set<std::string> touched;
    if (!config_of(run.preset, cfg, touched, preset_base, error)) return false;

    cfg.data = dataset;
    if (!image_dir.empty()) cfg.image_dir = image_dir;
    if (!mask_dir.empty()) cfg.mask_dir = mask_dir;
    if (!mask_dir.empty()) cfg.flip_mask = mask_flipped;
    cfg.output_dir_prefix = row.output_dir.empty()
                                ? (fs::path(dataset) / "outputs").string()
                                : row.output_dir;
    // Every run gets its own timestamped subfolder, so a batch re-run never
    // writes over what the last one produced and two rows may share a folder.
    cfg.output_dir_name.clear();
    // Unattended: nothing will open a browser, and binding a port per run is
    // one more thing that can fail between two datasets. The native viewport
    // does not go through the web viewer, so a run is still watchable.
    cfg.disable_viewer = true;

    // The run's own overrides go on top of the preset, and count as set by
    // hand: --quality moves cap_max and num_iterations, and a number typed
    // into the row is not something a macro gets to overwrite.
    for (const OverrideSpec& o : overrides_of(run)) {
        int v = 0;
        if (!parse_override(o.text, o.lo, o.hi, &v)) {
            error = spirula::i18n::format(
                spirula::i18n::msg::log::err_bad_flag_value, {o.flag, o.text});
            return false;
        }
        if (!is_set(o.text)) continue;
        cfg.*o.field = v;
        touched.insert(o.flag);
    }

    // The same resolution order the trainer screen uses, so a run trains with
    // the values the options editor would have shown for this preset.
    train_resolve_macros(cfg, touched);
    return true;
}


bool batch_build_mesh_job(const BatchRow& row, const std::string& model,
                          const std::string& dataset, MeshJob& job,
                          std::string& error) {
    job = MeshJob{};
    if (!row.mesh.preset.path.empty()) {
        try {
            job = load_mesh_preset(row.mesh.preset.path).job;
        } catch (const std::exception& e) {
            error = e.what();
            return false;
        }
    }
    apply_mesh_overrides(row.mesh, job);
    if (model.empty()) {
        error = "no model to mesh";
        return false;
    }
    job.checkpoint = model;
    job.output = default_mesh_output(model);
    // A row that names or builds a dataset says so; otherwise the child reads
    // it out of the run's own config.json, which is what a run folder carries.
    job.data_dir = job.use_data ? dataset : std::string();
    sanitize_mesh_job(job);
    return true;
}


// ---------------------------------------------------------------------------
// The list on disk
// ---------------------------------------------------------------------------

namespace {

// The three overrides, wherever a file spells them. They sat on the row
// before they sat on the run, and a queue written then still loads.
void read_overrides(const JsonValue& j, BatchRun& run) {
    if (const JsonValue* v = j.find("cap_max")) run.cap_max = v->as_string();
    if (const JsonValue* v = j.find("sh_degree")) run.sh_degree = v->as_string();
    if (const JsonValue* v = j.find("num_iterations"))
        run.iterations = v->as_string();
}

BatchRow read_row(const JsonValue& j) {
    BatchRow r;
    if (const JsonValue* v = j.find("sources"); v && v->is_array())
        for (const JsonValue& e : v->arr)
            if (!e.as_string().empty()) r.sources.push_back(e.as_string());
    if (const JsonValue* v = j.find("dataset")) r.dataset = v->as_string();
    if (const JsonValue* v = j.find("model")) r.model = v->as_string();
    if (const JsonValue* v = j.find("output_dir")) r.output_dir = v->as_string();
    r.dataset_preset = read_preset(j.find("dataset_preset"));
    if (r.dataset_preset.path.empty() && r.dataset_preset.name.empty())
        r.dataset_preset.name = "general";

    if (const JsonValue* m = j.find("mesh"); m && m->is_object()) {
        r.mesh.preset = read_preset(m->find("preset"));
        r.mesh.colors = read_mask(m->find("colors"), kMeshColorModes,
                                  kNumMeshColorModes);
        r.mesh.formats = read_mask(m->find("formats"), kMeshFormats,
                                   kNumMeshFormats);
    } else {
        r.mesh.preset = read_preset(j.find("mesh_preset"));
    }

    // A row whose runs were a list of presets with one set of overrides
    // between them: every run keeps the numbers the row used to carry.
    BatchRun shared;
    read_overrides(j, shared);
    if (const JsonValue* v = j.find("runs"); v && v->is_array()) {
        for (const JsonValue& e : v->arr) {
            BatchRun run;
            run.preset = read_preset(e.find("preset"));
            read_overrides(e, run);
            if (const JsonValue* b = e.find("mesh")) run.mesh = b->as_bool(true);
            r.runs.push_back(std::move(run));
        }
    } else if (const JsonValue* v = j.find("train_presets"); v && v->is_array()) {
        for (const JsonValue& e : v->arr) {
            BatchRun run = shared;
            run.preset = read_preset(&e);
            r.runs.push_back(std::move(run));
        }
    }
    if (const JsonValue* v = j.find("stages"); v && v->is_array())
        for (int i = 0; i < kNumBatchStages && i < (int)v->arr.size(); i++)
            r.stages[i] = v->arr[(size_t)i].as_bool();
    if (const JsonValue* v = j.find("enabled")) r.enabled = v->as_bool(true);
    return r;
}

}  // namespace


std::vector<BatchRow> load_batch_list() {
    std::vector<BatchRow> out;
    try {
        const JsonValue root = json_parse_file(batch_list_path());
        if (const JsonValue* rows = root.find("rows"); rows && rows->is_array()) {
            for (const JsonValue& j : rows->arr) out.push_back(read_row(j));
            return out;
        }
        // A queue written before rows could do more than train.
        if (const JsonValue* jobs = root.find("jobs"); jobs && jobs->is_array()) {
            for (const JsonValue& j : jobs->arr) {
                BatchRow r;
                r.stages[(int)BatchStage::Train] = true;
                if (const JsonValue* v = j.find("dataset")) r.dataset = v->as_string();
                if (const JsonValue* v = j.find("output_dir"))
                    r.output_dir = v->as_string();
                BatchRun run;
                read_overrides(j, run);
                if (const JsonValue* v = j.find("preset_path"))
                    run.preset.path = v->as_string();
                if (const JsonValue* v = j.find("preset_name"))
                    run.preset.name = v->as_string();
                if (run.preset.name.empty()) run.preset.name = "3dgs";
                r.runs.push_back(std::move(run));
                out.push_back(std::move(r));
            }
        }
    } catch (const std::exception&) {
        // No list yet, or one a crash left half-written. Starting empty is the
        // only useful answer either way.
    }
    return out;
}


void save_batch_list(const std::vector<BatchRow>& rows) {
    JsonWriter w;
    w.object();
    w.key("rows").array();
    for (const BatchRow& r : rows) {
        w.object();
        w.key("sources").array();
        for (const std::string& s : r.sources) w.value(s);
        w.end();
        w.field("dataset", r.dataset);
        w.field("model", r.model);
        w.field("output_dir", r.output_dir);
        write_preset(w, "dataset_preset", r.dataset_preset);
        w.key("mesh").object();
        write_preset(w, "preset", r.mesh.preset);
        write_mask(w, "colors", r.mesh.colors, kMeshColorModes,
                   kNumMeshColorModes);
        write_mask(w, "formats", r.mesh.formats, kMeshFormats, kNumMeshFormats);
        w.end();
        w.key("runs").array();
        for (const BatchRun& run : r.runs) {
            w.object();
            write_preset(w, "preset", run.preset);
            w.field("cap_max", run.cap_max);
            w.field("sh_degree", run.sh_degree);
            w.field("num_iterations", run.iterations);
            w.field("mesh", run.mesh);
            w.end();
        }
        w.end();
        w.key("stages").array();
        for (int i = 0; i < kNumBatchStages; i++) w.value(r.stages[i]);
        w.end();
        w.field("enabled", r.enabled);
        w.end();
    }
    w.end();
    w.end();

    FILE* f = std::fopen(batch_list_path().c_str(), "wb");
    if (!f) return;
    const std::string text = w.str();
    std::fwrite(text.data(), 1, text.size(), f);
    std::fclose(f);
}

}  // namespace gui
