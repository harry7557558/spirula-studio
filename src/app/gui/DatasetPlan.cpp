#include "app/gui/DatasetPlan.h"
#include "dense/ConfigFields.h"
#include "dense/Artifact.h"
#include "i18n/catalog/Dense.h"

#include "app/FrameMask.h"
#include "app/gui/ColmapRunner.h"
#include "app/gui/SfmRunner.h"
#include "data/Yaml.h"
#include "i18n/catalog/Log.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iterator>
#include <map>
#include <set>

namespace fs = std::filesystem;

namespace gui {

namespace {

std::string num(double v) {
    char b[32];
    std::snprintf(b, sizeof b, "%g", v);
    return b;
}

std::string onoff(bool b) { return b ? "on" : "off"; }

std::string norm_path(const std::string& p) {
    if (p.empty()) return p;
    std::error_code ec;
    std::string s = fs::absolute(p, ec).lexically_normal().generic_string();
    while (s.size() > 1 && s.back() == '/') s.pop_back();
    return s;
}

bool same_dir(const std::string& a, const fs::path& b) {
    std::error_code ec;
    return !a.empty() && fs::is_directory(b, ec) && fs::equivalent(a, b, ec) && !ec;
}

std::string join(const std::string& a, const std::string& b) {
    return a.empty() ? b : b.empty() ? a : a + "/" + b;
}

std::string scope_of(const std::string& rel) { return rel.empty() ? "." : rel; }

std::string joined(std::vector<std::string> v) {
    std::sort(v.begin(), v.end());
    std::string s;
    for (const std::string& x : v) s += (s.empty() ? "" : ",") + scope_of(x);
    return s;
}

// FNV-1a, for settings too long to show (clicks, drawn shapes): only whether
// they moved is ever reported.
std::string digest(const std::string& s) {
    uint64_t h = 1469598103934665603ull;
    for (unsigned char c : s) {
        h ^= c;
        h *= 1099511628211ull;
    }
    char b[24];
    std::snprintf(b, sizeof b, "#%016llx", (unsigned long long)h);
    return b;
}

void add(StepFields& f, const char* key, const std::string& scope,
         const std::string& value) {
    f.push_back({key, scope, value});
}

bool input_in_dataset(const PrepJob& job, const PrepInput& in) {
    if (in.is_video) return false;
    if (reads_photos_in_place(job.inputs, job.photo_import)) return true;
    return same_dir(in.path, fs::path(job.workspace) / "images");
}

bool input_masks_in_dataset(const PrepJob& job, const PrepInput& in) {
    if (in.mask_dir.empty()) return false;
    if (reads_photos_in_place(job.inputs, job.photo_import)) return true;
    return same_dir(in.mask_dir, fs::path(job.workspace) / "masks");
}

bool masks_wanted(const PrepJob& job) {
    if (job.mask_enable) return true;
    for (const PrepInput& in : job.inputs)
        if (!in.mask_dir.empty() || !in.stencil.empty()) return true;
    return false;
}

// Whether SfM will be handed a masks/ to keep feature points out of.
bool masks_reach_features(const PrepJob& job, bool mask_features) {
    return mask_features && masks_wanted(job);
}

std::string stencil_digest(const app::FrameStencil& st) {
    std::string s;
    auto one = [&](const std::string& cam, const app::CameraStencil& c) {
        s += cam + "|" + (c.detect_border ? "border" : "-") + "|" + num(c.shrink) +
             "|" + app::format_mask_shapes(c.mask.shapes) + "|" + c.mask.image + "\n";
    };
    one("", st);
    for (const auto& [cam, c] : st.cameras) one(cam, c);
    return digest(s);
}

std::string clicks_digest(const std::vector<MaskClick>& clicks) {
    std::string s;
    for (const MaskClick& c : clicks)
        s += num(c.x) + "," + num(c.y) + "," + (c.positive ? "+" : "-") + "," +
             std::to_string(c.object) + "," + std::to_string(c.frame) + "," +
             num(c.position) + "," + c.source + "," + c.camera + "\n";
    return digest(s);
}

std::string file_name(const std::string& p) {
    return p.empty() ? std::string() : fs::path(p).filename().string();
}

// The folders under images/ holding one camera's frames each, as the run
// will lay them out.
std::vector<std::string> leaf_folders(const PrepJob& job, const PrepInput& in) {
    std::vector<std::string> out;
    for (const SubCamera& sc : in.subcameras) out.push_back(join(in.subdir, sc.rel));
    if (!out.empty()) return out;
    for (const std::string& l : lens_dirs(job, in)) out.push_back(join(in.subdir, l));
    if (out.empty()) out.push_back(in.subdir);
    return out;
}

// Each camera folder's lens, whichever row of the panel it came from: a
// video's one row covers its cam0/ and cam1/, the same capture dropped back as
// a folder has a row each.
void add_lenses(StepFields& f, const PrepJob& p, const std::string& camera_model) {
    const std::vector<CameraGroup> rows = camera_groups(p.inputs);
    const std::vector<std::string> models =
        camera_group_models(p.inputs, rows, camera_model);
    for (const PrepInput& in : p.inputs)
        for (const std::string& leaf : leaf_folders(p, in)) {
            int best = -1;
            for (size_t r = 0; r < rows.size(); r++) {
                const std::string& rel = rows[r].rel;
                const bool under =
                    rel.empty() || leaf == rel ||
                    (leaf.size() > rel.size() && leaf.compare(0, rel.size(), rel) == 0 &&
                     leaf[rel.size()] == '/');
                if (under && (best < 0 || rel.size() >= rows[(size_t)best].rel.size()))
                    best = (int)r;
            }
            std::string model = camera_model;
            float focal = 0.0f;
            if (best >= 0) {
                const CameraGroup& g = rows[(size_t)best];
                if (!g.rel.empty() && !models[(size_t)best].empty())
                    model = models[(size_t)best];
                focal = group_focal(p.inputs, g);
            }
            add(f, "lens", scope_of(leaf), model);
            if (focal > 0) add(f, "focal", scope_of(leaf), num(focal));
        }
}

// The rigs build_rigs() will make, as their member folders and what is known
// of them. A 360 file's views and a dual-fisheye file's lenses are only known
// when one decode took them at one instant.
void add_rigs(StepFields& f, const PrepJob& p) {
    const bool builtin = backends().builtin_video && !p.force_external_decode;
    auto video_kind = [&](const PrepInput& in) -> std::string {
        const bool lockstep = in.pano360.valid() ? p.pano.mode != app::Pano360Mode::Off
                              : in.packed_lenses >= 2 ? true
                                                      : builtin && p.sync_tracks;
        if (!lockstep) return "free";
        if (in.pano360.valid()) return "360";
        return is_dual_lens(in) ? "dual-fisheye" : "free";
    };
    std::map<int, std::pair<std::vector<std::string>, bool>> shared;
    for (const PrepInput& in : p.inputs) {
        std::vector<std::string> own;
        bool own_dual = false;
        if (in.subcameras.empty()) {
            std::vector<std::string> lenses;
            for (const std::string& l : lens_dirs(p, in)) lenses.push_back(join(in.subdir, l));
            if (in.rig == kRigOwn) {
                if (lenses.size() >= 2) add(f, "rig", joined(lenses), video_kind(in));
            } else if (in.rig >= kRigFirstShared) {
                auto& [members, dual] = shared[in.rig];
                if (lenses.empty()) lenses.push_back(in.subdir);
                members.insert(members.end(), lenses.begin(), lenses.end());
                dual = dual || in.rig_dual_fisheye;
            }
            continue;
        }
        for (const SubCamera& sc : in.subcameras) {
            const std::string m = join(in.subdir, sc.rel);
            if (sc.rig == kRigOwn) {
                own.push_back(m);
                own_dual = own_dual || sc.rig_dual_fisheye;
            } else if (sc.rig >= kRigFirstShared) {
                auto& [members, dual] = shared[sc.rig];
                members.push_back(m);
                dual = dual || sc.rig_dual_fisheye;
            }
        }
        if (own.size() >= 2)
            add(f, "rig", joined(own), own_dual && own.size() == 2 ? "dual-fisheye" : "free");
    }
    for (const auto& [letter, rig] : shared)
        if (rig.first.size() >= 2)
            add(f, "rig", joined(rig.first),
                rig.second && rig.first.size() == 2 ? "dual-fisheye" : "free");
}

void add_sequences(StepFields& f, const PrepJob& p, bool use_sequence) {
    if (!use_sequence) return;
    for (const PrepInput& in : p.inputs)
        for (const std::vector<std::string>& members : input_sequences(p, in))
            add(f, "sequence", joined(members), "on");
}

std::string image_dir_field(const PrepJob& p) {
    return reads_photos_in_place(p.inputs, p.photo_import) ? norm_path(p.inputs[0].path)
                                                          : std::string("images");
}

int effective_camera_mode(const PrepJob& p, int mode) {
    size_t folders = 0;
    for (const PrepInput& in : p.inputs) folders += leaf_folders(p, in).size();
    return mode == 0 && folders > 1 ? 1 : mode;
}

// Model fields that describe the masks it was given rather than how it was
// solved: a difference there leaves the model standing (docs/notes/
// dataset-rerun.md, "masks").
bool soft_model_key(const std::string& key) {
    return key == "masks_for_features" || key == "feature_only_masks";
}

// The first `spirula sfm` stage whose signature (sfm::stageSignature) carries
// what a model field becomes on its command line. Unlisted counts as
// extraction: overstating the work beats hiding it.
ModelPart field_part(const std::string& key) {
    static const char* const matching[] = {
        "lens",        "focal",        "camera_mode", "data_type", "matcher",
        "pairs",       "overlap",      "loop_closure", "prefilter_sequential",
        "block_size", "block_neighbours", "block_radius", "block_cache_mb",
        "focal_px",    "distortion",   "sequence",    "rig",        "scan_views",
    };
    static const char* const mapping[] = {
        "mapper",         "distortion_refine", "final_per_image_intrinsics",
        "final_free_rig", "metric_gps",        "sensor_gauge",
        "exif_attitude", "map_memory_mb",
    };
    for (const char* k : matching)
        if (key == k) return ModelPart::Matching;
    for (const char* k : mapping)
        if (key == k) return ModelPart::Mapping;
    return ModelPart::Features;
}

std::vector<FieldChange> diff(const StepFields& was, const StepFields& now,
                              bool only_now = false) {
    auto key = [](const StepField& f) { return f.key + '\x1f' + f.scope; };
    std::map<std::string, const StepField*> before, after;
    for (const StepField& f : was) before[key(f)] = &f;
    for (const StepField& f : now) after[key(f)] = &f;
    std::vector<FieldChange> out;
    for (const StepField& f : now) {
        const auto it = before.find(key(f));
        if (it == before.end()) out.push_back({f.key, f.scope, "", f.value});
        else if (it->second->value != f.value)
            out.push_back({f.key, f.scope, it->second->value, f.value});
    }
    if (!only_now)
        for (const StepField& f : was)
            if (!after.count(key(f))) out.push_back({f.key, f.scope, f.value, ""});
    return out;
}

void set(StepPlan& s, Act a, Why w = Why::None) {
    s.act = a;
    s.why = w;
}

// A record that a step of this kind has started but never finished: finish
// it if the settings still read the same, otherwise start it over.
void compare(StepPlan& s, const StepRecord& r, std::vector<FieldChange> changes) {
    s.changes = std::move(changes);
    if (!r.complete) set(s, s.changes.empty() ? Act::Run : Act::Redo,
                         s.changes.empty() ? Why::Resume : Why::Settings);
    else if (s.changes.empty()) set(s, Act::Reuse);
    else set(s, Act::Redo, Why::Settings);
}

// What `spirula sfm` and `spirula lidar` will make of the workspace: each
// reuses by its own signature, predicted here from the fields that feed it,
// then what the user chose where the stage's lock allows.
void plan_parts(DatasetPlan& p, const PlanJob& job, const WorkspaceState& ws,
                const DatasetRecord& rec, const PlanRequest& req, bool masks_feed) {
    const StepPlan& fr = p[Step::Frames];
    const StepPlan& mk = p[Step::Masks];
    const StepPlan& md = p[Step::Model];
    const StepRecord& rr = rec.step(Step::Model);
    StepPlan& fe = p[ModelPart::Features];
    StepPlan& ma = p[ModelPart::Matching];
    StepPlan& mp = p[ModelPart::Mapping];
    StepPlan& al = p[ModelPart::Align];
    if (!job.reconstruct) return;

    auto choose = [&](ModelPart part, StepPlan& s) {
        if (s.lock != Lock::None) return;
        const PartChoice c = req.parts[(int)part];
        if (c == PartChoice::Keep && makes(s.act)) s.act = Act::Keep;
        else if (c == PartChoice::Run && !makes(s.act)) set(s, Act::Redo, Why::Requested);
    };
    auto changed = [](const StepPlan& s, std::initializer_list<const char*> keys) {
        for (const FieldChange& c : s.changes)
            for (const char* k : keys)
                if (c.key == k) return true;
        return false;
    };

    if (job.staged && makes(md.act)) {
        if (rr.present)
            for (const FieldChange& c : diff(rr.fields, job.model))
                p[field_part(c.key)].changes.push_back(c);

        const std::string dir = image_dir_field(job.prep);
        const fs::path images = dir == "images" ? fs::path(job.prep.workspace) / dir : fs::path(dir);
        const bool moved = !ws.extracted_images.empty() && !same_dir(ws.extracted_images, images);

        // The runner deletes features/ and matches.bin under new frames.
        if (makes(fr.act)) {
            set(fe, ws.features ? Act::Redo : Act::Run, ws.features ? Why::Frames : Why::None);
            fe.lock = ws.features ? Lock::Frames : Lock::Nothing;
        } else if (!ws.extracted) {
            set(fe, Act::Run);
            fe.lock = Lock::Nothing;
        } else if (moved) {
            set(fe, Act::Redo, Why::Moved);
        } else if (!rr.present) {
            set(fe, Act::Reuse, Why::Unrecorded);
        } else if (!fe.changes.empty()) {
            set(fe, Act::Redo, Why::Settings);
        } else if (masks_feed && (makes(mk.act) || rr.masks_id != rec.step(Step::Masks).id)) {
            // An image is extracted again when its mask file is newer.
            set(fe, Act::Redo, Why::Masks);
        } else if (!rr.complete && !ws.matched) {
            set(fe, Act::Run, Why::Resume);
            fe.lock = Lock::Nothing;
        } else {
            set(fe, Act::Reuse);
        }
        // A learned matcher reads only the descriptors it was trained on.
        if (fe.lock == Lock::None && changed(fe, {"features", "engine"})) fe.lock = Lock::Frontend;
        choose(ModelPart::Features, fe);

        // Its signature covers every feature file's size, write time and path.
        if (!ws.matched) {
            set(ma, Act::Run, ws.matching_part && !makes(fe.act) ? Why::Resume : Why::None);
            ma.lock = Lock::Nothing;
        } else if (makes(fe.act)) {
            set(ma, Act::Redo, Why::Features);
            ma.lock = Lock::Before;
        } else if (moved) {
            set(ma, Act::Redo, Why::Moved);
        } else if (!rr.present) {
            set(ma, Act::Reuse, Why::Unrecorded);
        } else if (!ma.changes.empty()) {
            set(ma, Act::Redo, Why::Settings);
        } else {
            set(ma, Act::Reuse);
        }
        if (ma.lock == Lock::None && changed(ma, {"lens", "focal", "camera_mode", "focal_px",
                                                  "distortion", "data_type", "scan_views"}))
            ma.lock = Lock::Lens;
        choose(ModelPart::Matching, ma);

        if (!ws.model) set(mp, Act::Run);
        else if (makes(ma.act)) set(mp, Act::Redo, Why::Matches);
        else if (!mp.changes.empty()) set(mp, Act::Redo, Why::Settings);
        else if (md.why == Why::Resume) set(mp, Act::Run);
        else set(mp, Act::Redo, md.why);
        mp.lock = Lock::Always;
    }

    if (job.lidar_clouds.empty()) return;
    if (makes(md.act)) {
        set(al, ws.aligned ? Act::Redo : Act::Run, ws.aligned ? Why::Model : Why::None);
        al.lock = ws.aligned ? Lock::Before : Lock::Nothing;
    } else if (!ws.aligned) {
        set(al, Act::Run);
        al.lock = Lock::Nothing;
    } else if (ws.aligned_clouds == job.lidar_clouds &&
               ws.aligned_kept_frame == job.lidar_in_frame) {
        set(al, Act::Reuse);
    } else {
        set(al, Act::Redo, Why::Settings);
    }
    choose(ModelPart::Align, al);
}

float to_float(const std::string& s) { return (float)std::strtod(s.c_str(), nullptr); }

// `.spirula-frames` onto `j`: the decoder on its first line, then flags, and
// `--input path subdir fps` per input. An input `now` also lists keeps what
// probing it found.
bool apply_legacy_frames(const std::vector<std::string>& a, const PrepJob& now,
                         PrepJob& j) {
    if (a.empty()) return false;
    j.force_external_decode = a[0] == "ffmpeg";
    j.inputs.clear();
    for (size_t k = 1; k + 1 < a.size(); k++) {
        const std::string& flag = a[k];
        if (flag == "--input") {
            if (k + 3 >= a.size()) break;
            PrepInput in;
            for (const PrepInput& n : now.inputs)
                if (n.path == a[k + 1]) in = n;
            in.path = a[k + 1];
            if (in.video_tracks == 0) in.is_video = is_video_path(in.path);
            in.subdir = a[k + 2];
            in.fps = to_float(a[k + 3]);
            j.inputs.push_back(in);
            k += 3;
            continue;
        }
        const std::string& v = a[++k];
        if (flag == "--fps") j.video_fps = to_float(v);
        else if (flag == "--adaptive") j.adaptive_fps = v == "1";
        else if (flag == "--range") j.adaptive_range = to_float(v);
        else if (flag == "--sharp") j.sharp_window = (int)to_float(v);
        else if (flag == "--sync") j.sync_tracks = v == "1";
        else if (flag == "--max-frames") j.max_frames = (int)to_float(v);
        else if (flag == "--rotate") j.auto_rotate = v == "1";
        else if (flag == "--photos") j.photo_import = (PhotoImport)(int)to_float(v);
        else if (flag == "--360") j.pano.mode = (app::Pano360Mode)(int)to_float(v);
        else if (flag == "--360-size") j.pano.size = (int)to_float(v);
        else if (flag == "--360-orient")
            std::sscanf(v.c_str(), "%f,%f,%f", &j.pano.yaw, &j.pano.pitch, &j.pano.roll);
    }
    return true;
}

StepRecord legacy_frames(const std::string& workspace, const PrepJob& now) {
    StepRecord r;
    PrepJob j = now;
    if (!apply_legacy_frames(read_legacy_stamp(workspace, ".spirula-frames"), now, j))
        return r;
    r.present = r.complete = true;
    r.fields = frames_fields(j);
    return r;
}

template <int N>
int index_in(const char* const (&table)[N], const std::string& v, int fallback) {
    for (int i = 0; i < N; i++)
        if (v == table[i]) return i;
    return fallback;
}

// The built-in engine's `.spirula-recon` flags (SfmRunner::recon_args) onto
// `job`; what the manifest says per folder comes back as model fields.
void apply_legacy_recon(const std::vector<std::string>& a, const std::string& images,
                        SfmJob& job, StepFields& fields) {
    std::string manifest;
    // What a flag the run left out stood for.
    job.pairs = 0;
    job.partition_mode = job.non_partition_pairs = 0;
    job.partition_resolved = false;
    job.block_size_auto = job.block_cache_auto = true;
    job.block_size = 2000;
    job.block_cache_mb = 512;
    job.map_memory_mb = 0;
    job.block_neighbours = 40;
    job.block_radius = 0;
    job.overlap = 10;
    job.prefilter_sequential = true;
    job.loop_closure = true;
    job.metric_gps = 0;
    job.sensor_gauge = job.exif_attitude = 2;
    job.distortion_refine = 0;
    job.final_per_image_intrinsics = job.final_free_rig = false;
    job.init_focal_px = 0.0f;
    job.init_distortion.clear();
    job.max_features = job.max_image_size = 0;
    job.image_gamut.clear();
    job.image_is_linear.reset();
    job.image_exposure.clear();
    job.point_color_in_image_space = false;
    for (size_t k = 1; k < a.size(); k++) {
        const std::string& flag = a[k];
        const std::string v = k + 1 < a.size() ? a[k + 1] : std::string();
        auto take = [&]() { k++; return v; };
        if (flag == "--quality") job.quality = index_in(kSfmQuality, take(), 2);
        else if (flag == "--data-type") job.data_type = index_in(kSfmDataType, take(), 0);
        else if (flag == "--camera-model") job.camera_model = take();
        else if (flag == "--camera-mode") job.camera_mode = index_in(kSfmCameraMode, take(), 1);
        else if (flag == "--mapper") job.mapper = index_in(kSfmMapper, take(), 0);
        else if (flag == "--features") job.features = index_in(kSfmFeatures, take(), 0);
        else if (flag == "--matcher") job.matcher = take() == "bruteforce" ? 0 : 1;
        else if (flag == "--pairs") {
            job.pairs = index_in(kSfmPairs, take(), 0);
            if (job.pairs == 4) job.partition_mode = 2;
            else job.non_partition_pairs = job.pairs;
        }
        else if (flag == "--block-size") {
            job.block_size = (int)to_float(take());
            job.block_size_auto = false;
        }
        else if (flag == "--block-neighbours") job.block_neighbours = (int)to_float(take());
        else if (flag == "--block-radius") job.block_radius = to_float(take());
        else if (flag == "--block-cache-mb") {
            job.block_cache_mb = (int)to_float(take());
            job.block_cache_auto = false;
        }
        else if (flag == "--overlap") job.overlap = (int)to_float(take());
        else if (flag == "--map-memory-mb") job.map_memory_mb = (int)to_float(take());
        else if (flag == "--no-loop-closure") job.loop_closure = false;
        else if (flag == "--no-prefilter-sequential") job.prefilter_sequential = false;
        else if (flag == "--focal") job.init_focal_px = to_float(take());
        else if (flag == "--distortion") job.init_distortion = take();
        else if (flag == "--no-refine-extra-params")
            job.distortion_refine = std::max(job.distortion_refine, 1);
        else if (flag == "--no-final-extra-params") job.distortion_refine = 2;
        else if (flag == "--final-per-image-intrinsics") job.final_per_image_intrinsics = true;
        else if (flag == "--final-free-rig") job.final_free_rig = true;
        else if (flag == "--manifest") manifest = take();
        else if (flag == "--max-features" || flag == "--aliked-max-features" ||
                 flag == "--loma-max-features")
            job.max_features = (int)to_float(take());
        else if (flag == "--max-image-size") job.max_image_size = (int)to_float(take());
        else if (flag == "--metric-gps") job.metric_gps = index_in(kSfmMetricGps, take(), 0);
        else if (flag == "--sensor-gauge") job.sensor_gauge = index_in(kSfmSensorGauge, take(), 2);
        else if (flag == "--exif-attitude")
            job.exif_attitude = index_in(kSfmExifAttitude, take(), 2);
        else if (flag == "--image-gamut") job.image_gamut = take();
        else if (flag == "--image-linear") job.image_is_linear = true;
        else if (flag == "--no-image-linear") job.image_is_linear = false;
        else if (flag == "--image-exposure") job.image_exposure = take();
        else if (flag == "--point-color") job.point_color_in_image_space = take() == "image";
        else if (flag == "--masks") { take(); job.mask_features = true; }
        else if (flag == "--no-masks") job.mask_features = false;
    }
    JsonValue m;
    try {
        if (!manifest.empty()) m = yaml_parse(manifest);
    } catch (const std::exception&) {
    }
    std::vector<std::pair<std::string, std::string>> cameras;   // prefix, model
    if (const JsonValue* cs = m.find("cameras"); cs && cs->is_array())
        for (const JsonValue& c : cs->arr)
            if (const JsonValue* model = c.find("model"))
                if (const JsonValue* prefix = c.find("prefix"))
                    cameras.push_back({prefix->as_string(), model->as_string()});
    for (const std::string& leaf : camera_subfolders(images)) {
        std::string model = job.camera_model;
        size_t best = 0;
        for (const auto& [prefix, mdl] : cameras)
            if ((leaf == prefix || leaf.rfind(prefix + "/", 0) == 0) && prefix.size() >= best) {
                model = mdl;
                best = prefix.size();
            }
        add(fields, "lens", scope_of(leaf), model);
    }
    if (const JsonValue* rs = m.find("rigs"); rs && rs->is_array())
        for (const JsonValue& r : rs->arr) {
            std::vector<std::string> members;
            if (const JsonValue* ms = r.find("members"); ms && ms->is_array())
                for (const JsonValue& mm : ms->arr)
                    if (const JsonValue* prefix = mm.find("prefix"))
                        members.push_back(prefix->as_string());
            const JsonValue* kind = r.find("kind");
            if (members.size() >= 2)
                add(fields, "rig", joined(members),
                    kind && kind->as_string() == "dual-fisheye" ? "dual-fisheye" : "free");
        }
    if (const JsonValue* ss = m.find("sequences"); ss && ss->is_array())
        for (const JsonValue& sq : ss->arr) {
            std::vector<std::string> members;
            if (const JsonValue* ms = sq.find("members"); ms && ms->is_array())
                for (const JsonValue& mm : ms->arr)
                    members.push_back(mm.as_string() == "." ? std::string() : mm.as_string());
            if (!members.empty()) add(fields, "sequence", joined(members), "on");
        }
}

}  // namespace

// ---------------------------------------------------------------------------
// What each step is made with
// ---------------------------------------------------------------------------

bool frames_in_dataset(const PrepJob& job) {
    if (job.inputs.empty()) return false;
    for (const PrepInput& in : job.inputs)
        if (!input_in_dataset(job, in)) return false;
    return true;
}

bool masks_in_dataset(const PrepJob& job) {
    if (job.inputs.empty()) return false;
    for (const PrepInput& in : job.inputs)
        if (!input_masks_in_dataset(job, in)) return false;
    return true;
}

StepFields frames_fields(const PrepJob& job) {
    StepFields f;
    bool video = false, rate = false, multi = false, pano = false;
    for (const PrepInput& in : job.inputs) {
        if (input_in_dataset(job, in)) continue;
        const std::string scope = scope_of(in.subdir);
        add(f, "input", scope, norm_path(in.path));
        if (!in.is_video) continue;
        video = true;
        const bool every = every_frame(job, in);
        rate = rate || !every;
        add(f, "fps", scope, every ? "every" : num(input_fps(job, in)));
        multi = multi || (!in.pano360.valid() && in.video_tracks >= 2);
        pano = pano || in.pano360.valid();
    }
    // Photos already gathered are kept whatever the import mode says, so a
    // folder of photos has nothing else here that its images depend on.
    if (!video) return f;
    add(f, "decoder", "", job.force_external_decode ? "ffmpeg" : "builtin");
    add(f, "max_frames", "", num(job.max_frames));
    add(f, "auto_rotate", "", onoff(job.auto_rotate));
    if (rate) {
        add(f, "sharp_window", "", num(job.sharp_window));
        add(f, "adaptive_fps", "", onoff(job.adaptive_fps));
        if (job.adaptive_fps) add(f, "adaptive_range", "", num(job.adaptive_range));
    }
    if (multi) add(f, "sync_tracks", "", onoff(job.sync_tracks));
    if (pano) {
        add(f, "pano_mode", "", num((int)job.pano.mode));
        add(f, "pano_size", "", num(job.pano.size));
        add(f, "pano_orient", "",
            num(job.pano.yaw) + "," + num(job.pano.pitch) + "," + num(job.pano.roll));
    }
    return f;
}

StepFields masks_fields(const PrepJob& job) {
    StepFields f;
    // Some input's masks come from segmentation rather than from the input.
    bool segment = false;
    for (const PrepInput& in : job.inputs) {
        const std::string scope = scope_of(in.subdir);
        if (in.mask_dir.empty()) segment = true;
        else if (!input_masks_in_dataset(job, in))
            add(f, "found_masks", scope, norm_path(in.mask_dir));
        if (!in.mask_dir.empty() && job.flip_found_masks) add(f, "flip_masks", scope, "on");
        if (!in.stencil.empty()) add(f, "stencil", scope, stencil_digest(in.stencil));
    }
    if (!job.mask_enable) return f;
    const bool feature = !job.mask_feature_prompt.empty();
    if (!segment && !feature) return f;
    add(f, "mask_model", "", file_name(job.mask_model_path));
    if (!job.mask_detector_path.empty()) {
        add(f, "mask_detector", "", file_name(job.mask_detector_path));
        add(f, "mask_box_threshold", "", num(job.mask_detector_threshold));
    }
    add(f, "mask_max_size", "", num(job.mask_max_image_size));
    if (!job.image_exposure.empty()) add(f, "image_exposure", "", job.image_exposure);
    add(f, "mask_threshold", "", num(job.mask_threshold));
    add(f, "mask_nms", "", num(job.mask_nms));
    if (segment) {
        add(f, "mask_prompt", "", job.mask_prompt);
        add(f, "mask_negative_prompt", "", job.mask_negative_prompt);
        add(f, "mask_keep_subject", "", onoff(job.mask_keep_subject));
        add(f, "mask_dilate", "", num(job.mask_dilate_ratio));
        const bool memory = job.mask_memory || !job.mask_clicks.empty();
        add(f, "mask_memory", "", onoff(memory));
        if (memory) {
            add(f, "mask_detect_every", "", num(job.mask_detect_every));
            add(f, "mask_memory_frames", "", num(job.mask_memory_frames));
        }
        if (!job.mask_clicks.empty())
            add(f, "mask_clicks", "", clicks_digest(job.mask_clicks));
    }
    if (feature) add(f, "feature_prompt", "", job.mask_feature_prompt);
    return f;
}

StepFields model_fields(const SfmJob& job) {
    const PrepJob& p = job.prep;
    StepFields f;
    add(f, "engine", "", "builtin");
    add(f, "image_dir", "", image_dir_field(p));
    add_lenses(f, p, job.camera_model);
    add(f, "camera_mode", "",
        sfm_pick(kSfmCameraMode, effective_camera_mode(p, job.camera_mode), 1));
    add(f, "quality", "", sfm_pick(kSfmQuality, job.quality, 2));
    add(f, "data_type", "", sfm_pick(kSfmDataType, job.data_type));
    add(f, "mapper", "", sfm_pick(kSfmMapper, job.mapper));
    add(f, "map_memory_mb", "", num(job.map_memory_mb));
    add(f, "features", "", sfm_pick(kSfmFeatures, job.features));
    add(f, "matcher", "", sfm_matcher_for(job.features, job.matcher));
    add(f, "pairs", "", sfm_pick(kSfmPairs, job.pairs));
    if (job.pairs == 4) {
        add(f, "block_size", "", num(job.block_size));
        add(f, "block_neighbours", "", num(job.block_neighbours));
        add(f, "block_radius", "", num(job.block_radius));
        add(f, "block_cache_mb", "", num(job.block_cache_mb));
    }
    if (sequential_window_applies(job)) add(f, "overlap", "", num(job.overlap));
    add(f, "loop_closure", "", onoff(job.loop_closure));
    add(f, "prefilter_sequential", "", onoff(job.prefilter_sequential));
    if (job.init_focal_px > 0) add(f, "focal_px", "", num(job.init_focal_px));
    if (!job.init_distortion.empty()) add(f, "distortion", "", job.init_distortion);
    add(f, "distortion_refine", "", num(job.distortion_refine));
    add(f, "final_per_image_intrinsics", "", onoff(job.final_per_image_intrinsics));
    add(f, "final_free_rig", "", onoff(job.final_free_rig));
    if (job.max_features > 0) add(f, "max_features", "", num(job.max_features));
    if (job.max_image_size > 0) add(f, "max_image_size", "", num(job.max_image_size));
    add(f, "metric_gps", "", sfm_pick(kSfmMetricGps, job.metric_gps));
    add(f, "sensor_gauge", "", sfm_pick(kSfmSensorGauge, job.sensor_gauge, 2));
    add(f, "exif_attitude", "", sfm_pick(kSfmExifAttitude, job.exif_attitude, 2));
    if (!job.image_gamut.empty()) add(f, "image_gamut", "", job.image_gamut);
    if (job.image_is_linear) add(f, "image_linear", "", onoff(*job.image_is_linear));
    if (!job.image_exposure.empty()) add(f, "image_exposure", "", job.image_exposure);
    add(f, "point_color", "", job.point_color_in_image_space ? "image" : "srgb");
    if (!job.extra_args.empty()) add(f, "extra_args", "", job.extra_args);
    add_rigs(f, p);
    add_sequences(f, p, job.use_sequence);
    add(f, "masks_for_features", "", onoff(masks_reach_features(p, job.mask_features)));
    add(f, "feature_only_masks", "",
        onoff(p.mask_enable && !p.mask_feature_prompt.empty()));
    return f;
}

StepFields model_fields(const ColmapJob& job, const PrepJob& p) {
    StepFields f;
    auto flag = [](bool v) { return std::string(v ? "on" : "off"); };
    add(f, "engine", "", "colmap");
    add(f, "image_dir", "", image_dir_field(p));
    add(f, "lens", "", job.camera_model);
    add(f, "camera_mode", "", num(effective_camera_mode(p, job.camera_mode)));
    if (!job.camera_params.empty()) add(f, "camera_params", "", job.camera_params);
    if (job.init_focal_factor > 0) add(f, "focal", "", num(job.init_focal_factor));
    add(f, "features", "", job.feature_type == 1 ? "aliked" : "sift");
    add(f, "lightglue", "", flag(job.lightglue));
    add(f, "quality", "", num(job.quality));
    add(f, "matcher", "", num(job.matcher));
    if (job.matcher == 2) {
        add(f, "loop_closure", "", flag(job.seq_loop_closure));
        add(f, "overlap", "", num(job.seq_overlap));
        add(f, "quadratic_overlap", "", flag(job.seq_quadratic_overlap));
    }
    if (job.matcher == 3) add(f, "vocab_tree", "", job.vocab_tree_path);
    add(f, "max_features", "", num(job.max_num_features));
    add(f, "max_image_size", "", num(job.max_image_size));
    add(f, "affine_shape", "", flag(job.estimate_affine_shape));
    add(f, "extra_params", "", num(job.mapper_extra_params));
    add(f, "min_matches", "", num(job.min_num_matches));
    add(f, "max_ratio", "", num(job.match_max_ratio));
    add(f, "min_inliers", "", num(job.min_inliers_per_pair));
    add(f, "abs_pose_inliers", "", num(job.abs_pose_min_num_inliers));
    add(f, "abs_pose_inlier_ratio", "", num(job.abs_pose_min_inlier_ratio));
    add(f, "abs_pose_error", "", num(job.abs_pose_max_error));
    add(f, "merge_models", "", flag(job.merge_models));
    add(f, "final_ba", "", flag(job.final_bundle_adjust));
    if (!job.image_gamut.empty()) add(f, "image_gamut", "", job.image_gamut);
    if (job.image_is_linear) add(f, "image_linear", "", onoff(*job.image_is_linear));
    add(f, "masks_for_features", "", onoff(masks_reach_features(p, job.mask_features)));
    add(f, "feature_only_masks", "",
        onoff(p.mask_enable && !p.mask_feature_prompt.empty()));
    return f;
}

StepFields geometry_fields(const GeometryJob& g) {
    StepFields f;
    add(f, "geometry_model", "", g.model);
    add(f, "geometry_max_size", "", num(g.max_size));
    if (g.model.rfind("moge", 0) == 0) add(f, "geometry_tokens", "", num(g.num_tokens));
    add(f, "ray_depth", "", num(g.ray_depth));
    add(f, "geometry_split", "", num(g.split));
    add(f, "face_res", "", num(g.face_res));
    add(f, "depth_mm", "", onoff(g.depth_mm));
    add(f, "normal_jpg", "", onoff(g.normal_jpg));
    if (g.normal_jpg) add(f, "jpeg_quality", "", num(g.jpeg_quality));
    if (!g.image_gamut.empty()) add(f, "image_gamut", "", g.image_gamut);
    if (g.image_is_linear) add(f, "image_linear", "", onoff(*g.image_is_linear));
    if (!g.image_exposure.empty()) add(f, "image_exposure", "", g.image_exposure);
    return f;
}

void verify_dense_reuse(DatasetPlan& plan, const std::string& dataset, const std::atomic<bool>& cancel) {
    auto& step = plan[Step::Dense];
    if (step.act != Act::Reuse) return;
    auto check = [&] { if (cancel.load()) throw std::runtime_error("dense verification cancelled"); };
    if (!spirula::dense::artifact_inputs_verified(dataset,check)) {
        step.act = Act::Redo; step.why = Why::Stale; step.ask = false;
    }
}

StepFields dense_fields(const DenseJob& job) {
    StepFields fields;
    const auto object = json_parse(spirula::dense::config_json(job.config));
    for (const auto& [key, value] : object.obj) {
        if (key == "preset" || key == "device" || key == "memory_budget_bytes" || key == "image_cache_bytes" ||
            key == "cpu_workers" || key == "resume" || key == "rebuild" || key == "keep_cache") continue;
        // A record from before reference spacing existed means 0, so such a cloud stays fresh.
        if (key == "reference_coverage" && value.num == 0) continue;
        JsonWriter writer; json_write(writer, value);
        fields.push_back({key, "", writer.str()});
    }
    return fields;
}

std::vector<std::string> geometry_kinds(const GeometryJob& g) {
    std::vector<std::string> k;
    if (g.want_normal) k.push_back("normal");
    if (g.want_depth) k.push_back("depth");
    return k;
}

PlanJob plan_job(const SfmJob& job) {
    PlanJob p;
    p.prep = job.prep;
    p.reconstruct = !job.lidar.scanner_only;
    p.model = model_fields(job);
    p.mask_features = job.mask_features;
    p.geometry = job.geometry;
    // The scans give the run its depth and normals.
    if (job.lidar.enabled()) p.geometry.enable = false;
    p.staged = true;
    p.lidar_clouds = job.lidar.clouds;
    p.lidar_in_frame = job.lidar.in_frame;
    p.dense = job.dense;
    return p;
}

PlanJob plan_job(const ColmapJob& job, const PrepJob& prep) {
    PlanJob p;
    p.prep = prep;
    p.model = model_fields(job, prep);
    p.mask_features = job.mask_features;
    p.geometry = job.geometry;
    p.dense = job.dense;
    return p;
}

// ---------------------------------------------------------------------------
// The plan
// ---------------------------------------------------------------------------

bool DatasetPlan::ask() const {
    for (const StepPlan& s : steps)
        if (s.ask) return true;
    return false;
}

DatasetRecord read_plan_record(const std::string& workspace, const PrepJob& job) {
    DatasetRecord rec = read_dataset_record(workspace);
    if (!rec.steps[(int)Step::Frames].present)
        rec.steps[(int)Step::Frames] = legacy_frames(workspace, job);
    return rec;
}

std::vector<PrepCapture> recorded_captures(const std::string& workspace, const PrepJob& job) {
    const StepRecord fr = read_plan_record(workspace, job).step(Step::Frames);
    if (!fr.present || !fr.captures.empty()) return fr.captures;
    // frames_fields back onto a job, as far as the stems depend on it.
    PrepJob j = job;
    j.inputs.clear();
    for (const StepField& f : fr.fields)
        if (f.key == "input" && is_video_path(f.value)) {
            PrepInput in;
            in.path = f.value;
            in.is_video = true;
            in.subdir = f.scope == "." ? "" : f.scope;
            for (const StepField& r : fr.fields)
                if (r.key == "fps" && r.scope == f.scope)
                    in.fps = r.value == "every" ? kFpsEveryFrame : to_float(r.value);
            j.inputs.push_back(std::move(in));
        }
    for (const StepField& f : fr.fields) {
        if (f.key == "decoder") j.force_external_decode = f.value == "ffmpeg";
        else if (f.key == "sharp_window") j.sharp_window = (int)to_float(f.value);
        else if (f.key == "adaptive_fps") j.adaptive_fps = f.value == "on";
        else if (f.key == "adaptive_range") j.adaptive_range = to_float(f.value);
        else if (f.key == "sync_tracks") j.sync_tracks = f.value == "on";
    }
    std::vector<PrepCapture> out;
    for (const PrepInput& in : j.inputs) out.push_back(video_capture(j, in));
    return out;
}

std::vector<PrepCapture> captures_behind(const PrepJob& job) {
    std::vector<PrepCapture> out;
    for (const PrepInput& in : job.inputs) {
        if (in.is_video) continue;
        std::error_code ec;
        const fs::path dir = fs::absolute(in.path, ec).lexically_normal();
        const fs::path images = dir.has_filename() ? dir : dir.parent_path();
        if (images.filename() != "images") continue;
        for (PrepCapture c : recorded_captures(images.parent_path().string(), job)) {
            if (!fs::is_regular_file(c.path, ec)) continue;
            c.subdir = in.subdir.empty() ? c.subdir
                       : c.subdir.empty() ? in.subdir
                                          : in.subdir + "/" + c.subdir;
            out.push_back(std::move(c));
        }
    }
    return out;
}

DatasetPlan plan_dataset(const PlanJob& job, const WorkspaceState& ws,
                         const DatasetRecord& rec, const PlanRequest& req,
                         const DatasetPlan* done, Step from) {
    DatasetPlan p;
    auto fixed = [&](Step s) { return done && (int)s < (int)from; };
    const StepRecord& rf = rec.step(Step::Frames);
    const StepRecord& rm = rec.step(Step::Masks);
    const StepRecord& rr = rec.step(Step::Model);
    const StepRecord& rg = rec.step(Step::Geometry);

    StepPlan& fr = p[Step::Frames];
    if (fixed(Step::Frames)) {
        fr = (*done)[Step::Frames];
    } else if (frames_in_dataset(job.prep)) {
        set(fr, Act::Reuse, Why::InDataset);
    } else if (!ws.frames) {
        set(fr, Act::Run);
    } else if (req.redo_frames) {
        set(fr, Act::Redo, Why::Requested);
    } else if (!rf.present) {
        set(fr, Act::Reuse, Why::Unrecorded);
    } else {
        compare(fr, rf, diff(rf.fields, frames_fields(job.prep)));
        if (fr.act == Act::Redo && rf.complete) {
            if (req.keep_built) set(fr, Act::Keep, Why::Settings);
            else fr.ask = true;
        }
    }

    StepPlan& mk = p[Step::Masks];
    const bool masks_adopted = masks_in_dataset(job.prep);
    if (fixed(Step::Masks)) {
        mk = (*done)[Step::Masks];
    } else if (!masks_wanted(job.prep)) {
    } else if (fr.act == Act::Redo || (fr.act == Act::Run && ws.masks)) {
        set(mk, Act::Redo, Why::Frames);
    } else if (fr.act == Act::Run) {
        set(mk, Act::Run, Why::Frames);
    } else if (req.redo_masks) {
        set(mk, Act::Redo, Why::Requested);
    } else {
        const StepFields now = masks_fields(job.prep);
        // Masks that came with the input are there already; only a feature
        // prompt still has something to make.
        const bool features = job.prep.mask_enable && !job.prep.mask_feature_prompt.empty();
        const bool have = masks_adopted ? (!features || ws.masks) : ws.masks;
        if (!have) {
            set(mk, Act::Run);
        } else if (!rm.present) {
            set(mk, Act::Reuse,
                masks_adopted && now.empty() ? Why::InDataset : Why::Unrecorded);
        } else if (rm.frames_id != rf.id) {
            set(mk, Act::Redo, Why::Stale);
        } else {
            compare(mk, rm, diff(rm.fields, now, masks_adopted));
            if (mk.act == Act::Reuse && masks_adopted) mk.why = Why::InDataset;
        }
    }

    StepPlan& md = p[Step::Model];
    const bool masks_feed = masks_reach_features(job.prep, job.mask_features);
    if (fixed(Step::Model)) {
        md = (*done)[Step::Model];
    } else if (!job.reconstruct) {
    } else if (!ws.model) {
        set(md, Act::Run);
    } else if (req.redo_model) {
        set(md, Act::Redo, Why::Requested);
    } else if (makes(fr.act)) {
        // Frames re-extracted for their settings were confirmed as a pair
        // with this; frames made where there were none never were.
        set(md, Act::Redo, Why::Frames);
        md.ask = fr.ask || (fr.act == Act::Run && !req.redo_frames);
    } else if (!rr.present) {
        set(md, Act::Reuse, Why::Unrecorded);
        md.masks_changed = masks_feed && makes(mk.act);
    } else {
        std::vector<FieldChange> all = diff(rr.fields, job.model);
        std::vector<FieldChange> hard;
        for (const FieldChange& c : all)
            if (!soft_model_key(c.key)) hard.push_back(c);
        const bool stale = rr.frames_id != rf.id;
        if (!rr.complete) {
            set(md, hard.empty() && !stale ? Act::Run : Act::Redo,
                hard.empty() && !stale ? Why::Resume : Why::Settings);
        } else if (!hard.empty() || stale) {
            const Why why = hard.empty() ? Why::Stale : Why::Settings;
            if (req.keep_built) {
                set(md, Act::Keep, why);
            } else {
                set(md, Act::Redo, why);
                md.ask = true;
            }
        } else {
            set(md, Act::Reuse);
            md.masks_changed = !all.empty() ||
                               (masks_feed && (makes(mk.act) || rr.masks_id != rm.id));
        }
        md.changes = std::move(all);
    }
    if (fixed(Step::Model))
        std::copy(std::begin(done->parts), std::end(done->parts), std::begin(p.parts));
    else
        plan_parts(p, job, ws, rec, req, masks_feed);

    StepPlan& dense = p[Step::Dense];
    const auto& rd = rec.step(Step::Dense);
    const auto& dc = job.dense.config;
    const bool dense_masks = dc.use_masks && (dc.training_masks || dc.alpha_masks || dc.feature_masks);
    if (fixed(Step::Dense)) dense = (*done)[Step::Dense];
    else if (!job.dense.enable) {}
    else if (makes(fr.act) && ws.dense) set(dense, Act::Redo, Why::Frames);
    else if (makes(md.act) && ws.dense) set(dense, Act::Redo, Why::Model);
    else if (dense_masks && makes(mk.act) && ws.dense) set(dense, Act::Redo, Why::Stale);
    else if (!ws.dense) set(dense, Act::Run);
    else if (req.redo_dense || dc.rebuild) set(dense, Act::Redo, Why::Requested);
    else if (!rd.present) set(dense, Act::Run, Why::Unrecorded);
    else if (rd.frames_id != rf.id || rd.model_id != rr.id || (dense_masks && rd.masks_id != rm.id)) set(dense, Act::Redo, Why::Stale);
    else compare(dense, rd, diff(rd.fields, dense_fields(job.dense)));

    StepPlan& g = p[Step::Geometry];
    if (fixed(Step::Geometry)) {
        g = (*done)[Step::Geometry];
        return p;
    }
    g.kinds = geometry_kinds(job.geometry);
    if (!job.geometry.enable || g.kinds.empty()) {
        g.kinds.clear();
    } else if (makes(fr.act) && ws.geometry) {
        set(g, Act::Redo, Why::Frames);
    } else if (makes(md.act) && ws.geometry) {
        set(g, Act::Redo, Why::Model);
    } else if (!ws.geometry) {
        set(g, Act::Run);
    } else if (req.redo_geometry || job.geometry.overwrite) {
        set(g, Act::Redo, Why::Requested);
    } else if (!rg.present) {
        // Whatever is missing; the maps already there are kept.
        set(g, Act::Run, Why::Unrecorded);
    } else if (rg.frames_id != rf.id || rg.model_id != rr.id) {
        set(g, Act::Redo, Why::Stale);
    } else {
        compare(g, rg, diff(rg.fields, geometry_fields(job.geometry)));
        if (g.act == Act::Reuse) {
            std::vector<std::string> missing;
            for (const std::string& k : g.kinds)
                if (std::find(rg.made.begin(), rg.made.end(), k) == rg.made.end())
                    missing.push_back(k);
            if (!missing.empty()) {
                set(g, Act::Run);
                g.kinds = missing;
                g.adds = true;
            }
        }
    }
    return p;
}

DatasetRecord read_legacy_settings(const std::string& workspace, SfmJob& job,
                                   bool& colmap) {
    DatasetRecord rec;
    const std::vector<std::string> frames = read_legacy_stamp(workspace, ".spirula-frames");
    const std::vector<std::string> recon = read_legacy_stamp(workspace, ".spirula-recon");
    if (frames.empty() && recon.empty()) return rec;
    rec.present = true;
    PrepJob framed = job.prep;
    if (apply_legacy_frames(frames, job.prep, framed)) {
        framed.inputs = job.prep.inputs;
        job.prep = framed;
    }
    colmap = !recon.empty() && recon[0] == "colmap";
    if (!recon.empty() && recon[0] == "builtin") {
        StepRecord& m = rec.steps[(int)Step::Model];
        m.present = m.complete = true;
        apply_legacy_recon(recon, (fs::path(workspace) / "images").string(), job, m.fields);
    }
    return rec;
}

void restore_record_inputs(const DatasetRecord& rec, PrepJob& job,
                           std::string& camera_model) {
    // Each half of a row as the run that made its step left it.
    auto made_by = [&](Step s) {
        const JsonValue& v = rec.step_inputs[(int)s];
        return decode_record_inputs(v.is_object() ? v : rec.inputs);
    };
    const RecordInputs lenses = made_by(Step::Model), rates = made_by(Step::Frames);
    const RecordInputs clicks = made_by(Step::Masks);
    std::vector<PrepInput>& rows = job.inputs;
    auto same_rows = [&](const RecordInputs& was) {
        bool same = !was.rows.empty() && was.rows.size() == rows.size();
        for (size_t i = 0; same && i < rows.size(); i++) same = was.rows[i].path == rows[i].path;
        return same;
    };
    if (same_rows(lenses)) {
        for (size_t i = 0; i < rows.size(); i++) {
            PrepInput& in = rows[i];
            const PrepInput& r = lenses.rows[i];
            in.camera_model = r.camera_model;
            in.focal_factor = r.focal_factor;
            in.rig = r.rig;
            in.rig_dual_fisheye = r.rig_dual_fisheye;
            in.sequential = r.sequential;
            for (SubCamera& sc : in.subcameras)
                for (const SubCamera& o : r.subcameras)
                    if (o.rel == sc.rel) sc = o;
        }
        if (same_rows(rates))
            for (size_t i = 0; i < rows.size(); i++) rows[i].fps = rates.rows[i].fps;
        if (same_rows(clicks)) job.mask_clicks = clicks.clicks;
        if (!rows.empty()) camera_model = rows[0].camera_model;
        return;
    }
    if (rows.size() != 1 || !frames_in_dataset(job)) return;

    std::map<std::string, std::string> lens, focal;
    std::vector<std::pair<std::vector<std::string>, std::string>> rigs;
    std::vector<std::vector<std::string>> sequences;
    auto split = [](const std::string& list) {
        std::vector<std::string> out;
        size_t at = 0;
        while (at <= list.size()) {
            const size_t comma = std::min(list.find(',', at), list.size());
            out.push_back(list.substr(at, comma - at));
            at = comma + 1;
        }
        return out;
    };
    for (const StepField& f : rec.step(Step::Model).fields) {
        if (f.key == "lens") lens[f.scope] = f.value;
        else if (f.key == "focal") focal[f.scope] = f.value;
        else if (f.key == "rig") rigs.push_back({split(f.scope), f.value});
        else if (f.key == "sequence") sequences.push_back(split(f.scope));
    }
    if (lens.empty()) return;
    PrepInput& in = rows[0];
    auto apply_lens = [&](const std::string& folder, std::string& model, float& f) {
        if (const auto it = lens.find(folder); it != lens.end()) model = it->second;
        const auto it = focal.find(folder);
        f = it == focal.end() ? 0.0f : (float)std::strtod(it->second.c_str(), nullptr);
    };
    std::vector<std::string> folders;
    if (in.subcameras.empty()) {
        apply_lens(".", in.camera_model, in.focal_factor);
        folders.push_back(".");
    } else {
        for (SubCamera& sc : in.subcameras) {
            const std::string folder = scope_of(sc.rel);
            apply_lens(folder, sc.camera_model, sc.focal_factor);
            sc.rig = kRigNone;
            sc.rig_dual_fisheye = false;
            folders.push_back(folder);
        }
        in.camera_model = in.subcameras[0].camera_model;
    }
    camera_model = in.camera_model;
    int letter = 0;
    for (const auto& [members, kind] : rigs) {
        std::vector<SubCamera*> on;
        for (const std::string& m : members)
            for (SubCamera& sc : in.subcameras)
                if (scope_of(sc.rel) == m) on.push_back(&sc);
        if (on.size() < 2 || on.size() != members.size()) continue;
        const bool whole = on.size() == in.subcameras.size();
        for (SubCamera* sc : on) {
            sc->rig = whole ? kRigOwn : kRigFirstShared + letter;
            sc->rig_dual_fisheye = kind == "dual-fisheye";
        }
        if (!whole) letter++;
    }
    in.sequential = false;
    for (const std::vector<std::string>& seq : sequences) {
        bool mine = !seq.empty();
        for (const std::string& m : seq)
            mine = mine && std::find(folders.begin(), folders.end(), m) != folders.end();
        in.sequential = in.sequential || mine;
    }
}

// ---------------------------------------------------------------------------
// Acting on it
// ---------------------------------------------------------------------------

void apply_masks_plan(const StepPlan& s, PrepJob& job) {
    job.redo_masks = s.act == Act::Redo;
    job.keep_masks = s.act == Act::Reuse || s.act == Act::Keep;
}

GeometryJob geometry_for_plan(GeometryJob g, const StepPlan& s) {
    auto has = [&](const char* k) {
        return std::find(s.kinds.begin(), s.kinds.end(), k) != s.kinds.end();
    };
    g.overwrite = s.act == Act::Redo;
    g.want_normal = has("normal");
    g.want_depth = has("depth");
    return g;
}

std::vector<std::string> geometry_made(const StepPlan& s, const DatasetRecord& rec) {
    std::vector<std::string> made = s.kinds;
    const StepRecord& r = rec.step(Step::Geometry);
    if (r.present && (s.adds || s.why == Why::Resume))
        for (const std::string& k : r.made)
            if (std::find(made.begin(), made.end(), k) == made.end()) made.push_back(k);
    return made;
}

std::string describe_changes(const std::vector<FieldChange>& changes) {
    auto shown = [](const std::string& v) {
        if (v.empty()) return std::string("-");
        return v[0] == '#' ? v.substr(0, 7) : v;
    };
    std::string out;
    size_t n = 0;
    for (const FieldChange& c : changes) {
        if (n++ == 6) {
            out += "; ...";
            break;
        }
        if (!out.empty()) out += "; ";
        out += c.key;
        if (!c.scope.empty() && c.scope != ".") out += "[" + c.scope + "]";
        out += ": " + shown(c.was) + " -> " + shown(c.now);
    }
    return out;
}

std::vector<std::string> plan_log_lines(Step step, const StepPlan& s,
                                        const std::string& workspace) {
    namespace L = spirula::i18n::msg::log;
    using spirula::i18n::format;
    const std::string what = describe_changes(s.changes);
    const bool settings = s.why == Why::Settings;
    switch (step) {
        case Step::Frames:
            if (s.act == Act::Redo && settings) return {format(L::frames_settings_changed, {what})};
            if (s.act == Act::Keep) return {format(L::plan_frames_kept, {what})};
            break;
        case Step::Masks:
            if (s.act == Act::Redo && settings) return {format(L::plan_masks_changed, {what})};
            if (s.act == Act::Redo && s.why == Why::Stale) return {L::plan_masks_stale.get()};
            break;
        case Step::Model:
            if (s.act == Act::Reuse) {
                std::vector<std::string> out{format(L::sfm_reusing_model, {workspace})};
                if (s.masks_changed) out.push_back(L::plan_model_masks_changed.get());
                return out;
            }
            if (s.act == Act::Keep) return {format(L::plan_model_kept, {what})};
            if (s.act == Act::Redo && settings) return {format(L::sfm_settings_changed, {what})};
            if (s.act == Act::Redo && s.why == Why::Stale) return {L::plan_model_stale.get()};
            break;
        case Step::Geometry:
            if (s.act == Act::Reuse) return {L::plan_geometry_current.get()};
            if (s.act == Act::Redo && settings) return {format(L::plan_geometry_changed, {what})};
            if (s.act == Act::Redo && (s.why == Why::Frames || s.why == Why::Model ||
                                       s.why == Why::Stale))
                return {L::plan_geometry_stale.get()};
            break;
        case Step::Dense:
            return {spirula::i18n::msg::dense::title.get()};
    }
    return {};
}

// ---------------------------------------------------------------------------
// Writing it down
// ---------------------------------------------------------------------------

StepRecorder::StepRecorder(std::string workspace, const DatasetRecord& rec)
    : _ws(std::move(workspace)) {
    for (int k = 0; k < kNumSteps; k++) _ids[k] = rec.steps[k].id;
}

void StepRecorder::begin(Step s, StepFields fields, std::vector<std::string> made) {
    StepRecord r;
    r.present = true;
    r.id = new_step_id();
    r.fields = std::move(fields);
    r.made = std::move(made);
    if (s != Step::Frames) r.frames_id = _ids[(int)Step::Frames];
    if (s == Step::Model) r.masks_id = _ids[(int)Step::Masks];
    if (s == Step::Geometry) r.model_id = _ids[(int)Step::Model];
    if (s == Step::Dense) { r.model_id = _ids[(int)Step::Model]; r.masks_id = _ids[(int)Step::Masks]; }
    _ids[(int)s] = r.id;
    _open[(int)s] = r;
    write_step_record(_ws, s, r);
}

void StepRecorder::finish(Step s, std::vector<PrepCapture> captures) {
    StepRecord& r = _open[(int)s];
    if (!r.present) return;
    r.complete = true;
    r.captures = std::move(captures);
    write_step_record(_ws, s, r);
    r = StepRecord{};
}

}  // namespace gui
