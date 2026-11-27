// SplatMerge.cpp -- see SplatMerge.h.

#include "checkpoint/SplatMerge.h"

#include "checkpoint/Resume.h"
#include "checkpoint/RegionCameras.h"
#include "checkpoint/SplatTransform.h"
#include "core/SphericalBeta.h"
#include "core/BufferedFile.h"
#include "core/AtomicFile.h"
#include "data/RegionProgram.h"
#include "data/SceneTransform.h"

#include <algorithm>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <tuple>

namespace fs = std::filesystem;

namespace spirula {

namespace {

// Raises the cloud to `to`'s bands (zero coefficients) and lobes (dark ones),
// so parts trained with different colour bases still share one file.
void pad_color(SplatCloud& c, const ColorBasis& to) {
    if (c.basis() == to) return;
    const ColorBasis from = c.basis();
    if (from.sh_degree > to.sh_degree || from.sb_lobes > to.sb_lobes)
        throw std::runtime_error("partition inputs changed during merge");
    const int64_t old_k = from.coeffs(), new_k = to.coeffs();
    std::vector<float> rows((size_t)(c.num * new_k * 3), 0.0f);
    for (int64_t i = 0; i < c.num; i++) {
        const float* src = c.features_sh.data() + i * old_k * 3;
        float* dst = rows.data() + i * new_k * 3;
        std::copy(src, src + 3 * from.sh_coeffs(), dst);
        dst += 3 * to.sh_coeffs();
        src += 3 * from.sh_coeffs();
        std::copy(src, src + 3 * from.sb_coeffs(), dst);
        for (int l = from.sb_lobes; l < to.sb_lobes; l++)
            sb::dark_lobe(dst + sb::kLobeFloats * l);
    }
    c.features_sh.swap(rows);
    c.sh_degree = to.sh_degree;
    c.sb_lobes = to.sb_lobes;
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
    if (in.coeffs() > 0) push(out.features_sh, in.features_sh, in.coeffs() * 3);
    for (uint8_t k : keep) out.num += k;
}

struct PartFrame {
    Sim3 dataset_from_train;
    std::optional<RegionCameras> cameras;
};

PartFrame part_frame(const std::string& ply, const std::string& run_dir, const std::string& hash) {
    PartFrame frame;
    SceneTransform transform;
    if (!hash.empty()) {
        frame.cameras = read_region_cameras(ply, hash);
        transform = frame.cameras->train_from_dataset;
    } else if (!run_dir.empty()) {
        SceneTransform parsed;
        if (read_scene_transform_json((fs::path(run_dir) / "scene_transform.json").string(), parsed))
            transform = parsed;
    }
    const SceneTransform inverse = transform.inverse();
    frame.dataset_from_train.s = inverse.scale;
    std::copy(inverse.R, inverse.R + 9, frame.dataset_from_train.R);
    std::copy(inverse.t, inverse.t + 3, frame.dataset_from_train.t);
    return frame;
}

bool same_camera_metadata(const MergeStats& a, const MergeStats& b) {
    return a.parts.size() == b.parts.size() && std::equal(a.parts.begin(), a.parts.end(), b.parts.begin(),
        [](const MergePartStats& x, const MergePartStats& y) { return x.region_camera_hash == y.region_camera_hash; });
}

using MergeRows = std::function<void(SplatCloud&, const std::vector<uint8_t>&)>;

void visit_partition_splats(const ScenePartition& p, const std::vector<std::string>& paths,
                            MergeStats& stats, const MergeRows& rows,
                            const std::function<void(const std::string&)>& log) {
    stats = MergeStats{};
    stats.parts.resize((size_t)p.num_parts);
    std::optional<bool> mip;
    for (int k = 0; k < p.num_parts; k++) {
        if (k >= (int)paths.size() || paths[(size_t)k].empty()) continue;
        std::string ply, run_dir;  // C++17 lambdas cannot capture structured bindings
        std::tie(ply, run_dir) = find_splat_ply(paths[(size_t)k]);
        auto& st = stats.parts[(size_t)k];
        st.ply = ply;
        const auto shape = splat_ply_shape(ply);
        st.read = shape.num;
        st.sh_degree = shape.sh_degree;
        stats.sh_degree = std::max(stats.sh_degree, shape.sh_degree);
        stats.sb_lobes = std::max(stats.sb_lobes, shape.sb_lobes);
        st.region_camera_hash = shape.region_camera_hash;
        const auto frame = part_frame(ply, run_dir, shape.region_camera_hash);
        const Sim3& S = frame.dataset_from_train;
        LabelField fallback_cameras;
        if (!frame.cameras) {
            std::vector<float> cameras;
            for (int32_t f : p.frames_of(k))
                cameras.insert(cameras.end(), p.frame_centers.begin() + (size_t)f * 3,
                               p.frame_centers.begin() + (size_t)f * 3 + 3);
            const std::vector<int32_t> labels(cameras.size() / 3, 0);
            fallback_cameras = LabelField::build(cameras.data(), labels.data(), nullptr, (int64_t)labels.size(), 4);
        }
        const LabelField& camera_field = frame.cameras ? frame.cameras->field : fallback_cameras;
        SplatCloud c;
        std::vector<uint8_t> keep;
        read_splat_ply_chunks(ply, 65536, [&](const SplatCloud& input, int64_t, int64_t total) {
            if (total != shape.num || input.basis() != shape.basis() || input.primitive != shape.primitive ||
                input.region_camera_hash != shape.region_camera_hash)
                throw std::runtime_error("partition inputs changed during merge: " + ply);
            c = input;
            if (!S.is_identity()) transform_splats(c, S);
            keep.resize((size_t)c.num);
#pragma omp parallel for schedule(static)
            for (int64_t i = 0; i < c.num; i++) {
                const float* qf = c.means.data() + (size_t)i * 3;
                const double q[3] = {qf[0], qf[1], qf[2]};
                double normal[3];
                const double* np = nullptr;
                const float* query_point = frame.cameras ? input.means.data() + (size_t)i * 3 : qf;
                const double query[3] = {query_point[0], query_point[1], query_point[2]};
                const int64_t camera = p.field && !p.field->euclidean ? camera_field.nearest_seed(query) : -1;
                if (camera >= 0) {
                    const float* seed = camera_field.seeds.data() + (size_t)camera * 4;
                    const double point[3] = {seed[0], seed[1], seed[2]};
                    double cc[3];
                    if (frame.cameras) S.apply(point, cc);
                    else std::copy(point, point + 3, cc);
                    const double to[3] = {cc[0] - q[0], cc[1] - q[1], cc[2] - q[2]};
                    splat_normal(&c.quats[(size_t)i * 4], &c.scales[(size_t)i * 3], to, normal);
                    np = normal;
                }
                keep[(size_t)i] = p.field && p.field->label(q, np) == k;
            }
            const int64_t kept = std::count(keep.begin(), keep.end(), (uint8_t)1);
            st.kept += kept;
            stats.total += kept;
            if (kept) {
                const bool part_mip = c.primitive == "mip";
                if (mip && *mip != part_mip)
                    throw std::runtime_error("cannot merge Mip-trained and classic or untagged splats in one PLY: " + ply);
                mip = part_mip;
                if (rows) rows(c, keep);
            }
        });
        if (log) {
            char buf[256];
            std::snprintf(buf, sizeof buf, "part %d: %lld splats read, %lld inside its region (%s)", k,
                          (long long)st.read, (long long)st.kept, ply.c_str());
            log(buf);
        }
    }
    if (mip.value_or(false)) stats.primitive = "mip";
}

}  // namespace

SplatCloud merge_partition_splats(const ScenePartition& p, const std::vector<std::string>& paths,
                                  MergeStats& stats,
                                  const std::function<void(const std::string&)>& log) {
    visit_partition_splats(p, paths, stats, {}, log);
    SplatCloud out;
    out.sh_degree = stats.sh_degree;
    out.sb_lobes = stats.sb_lobes;
    out.primitive = stats.primitive;
    out.means.reserve((size_t)stats.total * 3);
    out.quats.reserve((size_t)stats.total * 4);
    out.scales.reserve((size_t)stats.total * 3);
    out.opacities.reserve((size_t)stats.total);
    out.features_dc.reserve((size_t)stats.total * 3);
    out.features_sh.reserve((size_t)stats.total * out.coeffs() * 3);
    MergeStats checked;
    visit_partition_splats(p, paths, checked, [&](SplatCloud& c, const std::vector<uint8_t>& keep) {
        pad_color(c, out.basis());
        append(out, c, keep);
    }, {});
    if (out.num != stats.total || checked.sh_degree != stats.sh_degree || checked.sb_lobes != stats.sb_lobes ||
        checked.primitive != stats.primitive || !same_camera_metadata(checked, stats))
        throw std::runtime_error("partition inputs changed during merge");
    return out;
}

void merge_partition_splats_file(const ScenePartition& p, const std::vector<std::string>& paths,
                                 const std::string& output, MergeStats& stats,
                                 const std::function<void(const std::string&)>& log) {
    visit_partition_splats(p, paths, stats, {}, log);
    std::error_code ec;
    for (const auto& part : stats.parts) {
        if (part.ply.empty()) continue;
        if (fs::weakly_canonical(part.ply) == fs::weakly_canonical(output))
            throw std::runtime_error("partition merge requires a separate output");
    }
    fs::create_directories(fs::path(output).parent_path(), ec);
    const auto directory = reserve_file_directory(output);
    const auto temporary = directory / "model.ply";
    try {
        BufferedOutputFile file(temporary, std::ios::binary);
        if (!file) throw std::runtime_error("cannot open for writing: " + temporary.string());
        const ColorBasis basis{stats.sh_degree, stats.sb_lobes};
        file << splat_ply_header(stats.total, basis, stats.primitive);
        MergeStats checked;
        std::vector<float> buffer;
        visit_partition_splats(p, paths, checked, [&](SplatCloud& c, const std::vector<uint8_t>& keep) {
            pad_color(c, basis);
            write_splat_ply_rows(c, file, buffer, keep.data());
        }, {});
        file.close();
        if (!file) throw std::runtime_error("write failed: " + temporary.string());
        if (checked.total != stats.total || checked.sh_degree != stats.sh_degree || checked.sb_lobes != stats.sb_lobes ||
            checked.primitive != stats.primitive || !same_camera_metadata(checked, stats))
            throw std::runtime_error("partition inputs changed during merge");
        replace_file(temporary, output);
        fs::remove(directory, ec);
    } catch (...) {
        fs::remove(temporary, ec);
        fs::remove(directory, ec);
        throw;
    }
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
