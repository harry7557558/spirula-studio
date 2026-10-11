#pragma once

#include "sfm/map/RegionalStore.h"
#include "sfm/ba/CpuParallel.h"
#include <exception>
#include <unordered_map>

namespace sfm::regional {
struct Quality { double cost = 0; uint64_t observations = 0; };
struct ValidationObservation { uint64_t point = 0; uint32_t image = 0, reserved = 0; Vec2 xy; };

inline void writeValidation(const fs::path& path, const Reconstruction& rec, uint64_t model) {
    uint64_t count = 0;
    for (const auto& p : rec.points3D) count += p.second.track.size();
    model_store_detail::Writer out(path.string() + ".part");
    out.scalar<uint64_t>(0x314c415653535353ull); out.scalar(model); out.scalar(count);
    for (const auto& p : rec.points3D) {
        cancel::check();
        for (const auto& e : p.second.track) {
            const auto& im = rec.images.at(e.image_id);
            if (!im.registered || e.point2D_idx >= im.points2D.size()) throw std::runtime_error("invalid regional validation track");
            out.scalar(ValidationObservation{p.first, e.image_id, 0, im.points2D[e.point2D_idx]});
        }
    }
    out.finish(); replaceFile(path.string() + ".part", path);
}

inline Quality validationCost(const fs::path& path, const Reconstruction& local,
                              const Reconstruction& committed, uint64_t model, double gate, int threads = 0) {
    if (!(gate > 0) || !std::isfinite(gate)) throw std::invalid_argument("invalid regional validation gate");
    model_store_detail::Reader in(path);
    if (in.scalar<uint64_t>() != 0x314c415653535353ull || in.scalar<uint64_t>() != model)
        throw std::runtime_error("regional validation identity changed");
    uint64_t count = in.scalar<uint64_t>();
    if (count > (UINT64_MAX - 24) / sizeof(ValidationObservation) ||
        fs::file_size(path) != 24 + count * sizeof(ValidationObservation))
        throw std::runtime_error("regional validation file size mismatch");
    struct View { const Image* image; const Camera* camera; };
    std::unordered_map<uint32_t, View> views;
    for (const auto& item : committed.images) {
        const auto camera = committed.cameras.find(item.second.camera_id);
        if (camera != committed.cameras.end()) views.emplace(item.first, View{&item.second, &camera->second});
    }
    const auto& prepared = views;
    constexpr size_t batch = 262144, grain = 4096;
    std::vector<ValidationObservation> rows(size_t(std::min<uint64_t>(count, batch)));
    auto& pool = bacpu::Pool::get();
    const int workers = threads > 0 ? std::min(threads, pool.size()) : pool.size();
    Quality result;
    for (uint64_t offset = 0; offset < count;) {
        cancel::check(); const size_t size = size_t(std::min<uint64_t>(count - offset, batch));
        in.read(rows.data(), size);
        const int tasks = int((size + grain - 1) / grain);
        std::vector<double> costs(tasks);
        std::mutex failure_mutex; std::exception_ptr failure; std::atomic<bool> failed{false};
        pool.run(tasks, workers, [&](int task, int) {
            if (failed.load(std::memory_order_relaxed)) return;
            try {
                cancel::check(); uint64_t point = 0; bool has_point = false; Vec3 xyz{};
                const size_t end = std::min(size, (size_t(task) + 1) * grain);
                for (size_t o = size_t(task) * grain; o < end; ++o) {
                    const auto& row = rows[o]; const auto& view = prepared.at(row.image);
                    if (!view.image->registered || row.reserved || !(view.camera->pixel_scale > 0))
                        throw std::runtime_error("invalid committed validation camera");
                    if (!has_point || point != row.point) { xyz = local.points3D.at(row.point).xyz; point = row.point; has_point = true; }
                    double e = reprojErrorAt(*view.camera, view.image->pose, row.xy, xyz) / view.camera->pixel_scale;
                    if (!std::isfinite(e)) throw std::runtime_error("nonfinite committed validation residual");
                    costs[task] += e <= gate ? e * e : gate * (2 * e - gate);
                }
            } catch (...) {
                std::lock_guard<std::mutex> lock(failure_mutex);
                if (!failure) failure = std::current_exception();
                failed.store(true, std::memory_order_relaxed);
            }
        });
        if (failure) std::rethrow_exception(failure);
        for (double cost : costs) result.cost += cost;
        result.observations += size; offset += size;
    }
    in.finish();
    if (!std::isfinite(result.cost)) throw std::runtime_error("regional validation cost overflow");
    return result;
}

inline uint32_t qualityStop(Quality candidate, Quality accepted, double baseline, double tolerance, bool last) {
    if (!candidate.observations || candidate.observations != accepted.observations ||
        !std::isfinite(candidate.cost) || candidate.cost < 0 || !std::isfinite(baseline) || baseline < 0)
        throw std::runtime_error("regional validation observations or cost changed");
    if (candidate.cost > accepted.cost * (1 + 1e-8) + 1e-8) return 2;
    return last || std::abs(accepted.cost - candidate.cost) <= baseline * tolerance ? 1u : 0u;
}
} // namespace sfm::regional
