// SplatMerge.cpp -- see SplatMerge.h.

#include "checkpoint/SplatMerge.h"

#include "checkpoint/Resume.h"
#include "checkpoint/SplatTransform.h"
#include "data/RegionProgram.h"
#include "data/SceneTransform.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <stdexcept>

namespace fs = std::filesystem;

namespace spirula {

namespace {

// Raises the cloud to `deg` bands with zero coefficients, so parts trained
// at different --sh-degree still share one file.
void pad_sh(SplatCloud& c, int deg) {
    if (c.sh_degree >= deg) return;
    const int64_t old_k = c.dim_sh() - 1;
    const int64_t new_k = (int64_t)(deg + 1) * (deg + 1) - 1;
    std::vector<float> sh((size_t)(c.num * new_k * 3), 0.0f);
    for (int64_t i = 0; i < c.num; i++)
        for (int64_t k = 0; k < old_k; k++)
            for (int ch = 0; ch < 3; ch++)
                sh[(size_t)((i * new_k + k) * 3 + ch)] = c.features_sh[(size_t)((i * old_k + k) * 3 + ch)];
    c.features_sh.swap(sh);
    c.sh_degree = deg;
}

void append(SplatCloud& out, const SplatCloud& in, const std::vector<uint8_t>& keep) {
    auto push = [&](std::vector<float>& dst, const std::vector<float>& src, int64_t stride) {
        for (int64_t i = 0; i < in.num; i++)
            if (keep[(size_t)i])
                dst.insert(dst.end(), src.begin() + i * stride, src.begin() + (i + 1) * stride);
    };
    push(out.means, in.means, 3);
    push(out.quats, in.quats, 4);
    push(out.scales, in.scales, 3);
    push(out.opacities, in.opacities, 1);
    push(out.features_dc, in.features_dc, 3);
    if (in.sh_degree > 0) push(out.features_sh, in.features_sh, (in.dim_sh() - 1) * 3);
    for (uint8_t k : keep) out.num += k;
}

}  // namespace

SplatCloud merge_partition_splats(const ScenePartition& p, const std::vector<std::string>& part_paths,
                                  MergeStats& stats,
                                  const std::function<void(const std::string&)>& log) {
    stats = MergeStats{};
    stats.parts.resize((size_t)p.num_parts);
    std::vector<SplatCloud> clouds((size_t)p.num_parts);
    std::vector<std::vector<uint8_t>> keeps((size_t)p.num_parts);
    int deg = 0;
    for (int k = 0; k < p.num_parts; k++) {
        if (k >= (int)part_paths.size() || part_paths[(size_t)k].empty()) continue;
        auto [ply, run_dir] = find_splat_ply(part_paths[(size_t)k]);
        SplatCloud& c = clouds[(size_t)k];
        c = read_splat_ply(ply);
        MergePartStats& st = stats.parts[(size_t)k];
        st.ply = ply;
        st.read = c.num;
        st.sh_degree = c.sh_degree;
        // Back into the dataset's frame, which is the frame the partition's
        // regions are in.
        SceneTransform T;
        if (!run_dir.empty() &&
            read_scene_transform_json((fs::path(run_dir) / "scene_transform.json").string(), T)) {
            const SceneTransform W = T.inverse();
            Sim3 S;
            S.s = W.scale;
            for (int i = 0; i < 9; i++) S.R[i] = W.R[i];
            for (int i = 0; i < 3; i++) S.t[i] = W.t[i];
            if (!S.is_identity()) transform_splats(c, S);
        }
        // Each splat's short axis, pointed at the nearest of the cameras that
        // trained it, decides which side of a surface it is: the wall's back,
        // seen only from the next room, stays with that room's model.
        std::vector<float> cam_xyz;
        std::vector<int32_t> cam_idx;
        for (int32_t f : p.frames_of(k)) {
            cam_xyz.insert(cam_xyz.end(), p.frame_centers.begin() + (size_t)f * 3,
                           p.frame_centers.begin() + (size_t)f * 3 + 3);
            cam_idx.push_back((int32_t)cam_idx.size());
        }
        LabelField cams;
        if (!cam_xyz.empty())
            cams = LabelField::build(cam_xyz.data(), cam_idx.data(), nullptr, (int64_t)cam_idx.size(), 4);
        std::vector<uint8_t>& keep = keeps[(size_t)k];
        keep.resize((size_t)c.num);
#pragma omp parallel for schedule(static)
        for (int64_t i = 0; i < c.num; i++) {
            const double q[3] = {c.means[(size_t)i * 3], c.means[(size_t)i * 3 + 1], c.means[(size_t)i * 3 + 2]};
            double n[3];
            const double* np = nullptr;
            if (!cams.empty()) {
                const int cam = cams.label(q);
                if (cam >= 0 && cam < (int)cam_idx.size()) {
                    const float* cc = &cams.seeds[(size_t)cam * 4];
                    const double to[3] = {cc[0] - q[0], cc[1] - q[1], cc[2] - q[2]};
                    splat_normal(&c.quats[(size_t)i * 4], &c.scales[(size_t)i * 3], to, n);
                    np = n;
                }
            }
            keep[(size_t)i] = p.field && p.field->label(q, np) == k;
        }
        st.kept = std::count(keep.begin(), keep.end(), (uint8_t)1);
        deg = std::max(deg, c.sh_degree);
        if (log) {
            char buf[256];
            std::snprintf(buf, sizeof buf, "part %d: %lld splats read, %lld inside its region (%s)", k,
                          (long long)st.read, (long long)st.kept, ply.c_str());
            log(buf);
        }
    }
    SplatCloud out;
    out.num = 0;
    out.sh_degree = deg;
    for (int k = 0; k < p.num_parts; k++) {
        if (clouds[(size_t)k].num == 0 && keeps[(size_t)k].empty()) continue;
        pad_sh(clouds[(size_t)k], deg);
        append(out, clouds[(size_t)k], keeps[(size_t)k]);
        clouds[(size_t)k] = SplatCloud{};
    }
    stats.total = out.num;
    stats.sh_degree = deg;
    return out;
}

std::vector<std::string> find_partition_runs(const std::string& outputs_dir,
                                             const std::string& partition_json, int num_parts) {
    std::vector<std::string> runs((size_t)std::max(0, num_parts));
    std::vector<fs::file_time_type> when((size_t)std::max(0, num_parts));
    std::error_code ec;
    const fs::path want = fs::weakly_canonical(partition_json, ec);
    if (ec || !fs::is_directory(outputs_dir, ec)) return runs;
    for (fs::directory_iterator it(outputs_dir, ec), end; !ec && it != end; it.increment(ec)) {
        if (!it->is_directory(ec)) continue;
        const fs::path cfg = it->path() / "config.json";
        if (!fs::is_regular_file(cfg, ec)) continue;
        TrainConfig c;
        try {
            c = ckpt::config_from_json(cfg);
        } catch (const std::exception&) {
            continue;
        }
        if (c.partition.empty() || c.partition_part < 0 || c.partition_part >= num_parts) continue;
        std::error_code ec2;
        if (fs::weakly_canonical(c.partition, ec2) != want) continue;
        // Only a run that got as far as writing a model.
        try {
            find_splat_ply(it->path().string());
        } catch (const std::exception&) {
            continue;
        }
        const fs::file_time_type t = fs::last_write_time(cfg, ec2);
        const size_t k = (size_t)c.partition_part;
        if (runs[k].empty() || t > when[k]) {
            runs[k] = it->path().string();
            when[k] = t;
        }
    }
    return runs;
}

}  // namespace spirula
