#include "dense/Reconstruction.h"

#include "core/HardwareThreads.h"
#include "data/SparseEdit.h"
#include "data/Knn.h"
#include "dense/ReferenceSampling.h"
#include "dense/DiskArray.h"
#include "dense/ExternalSort.h"
#include "dense/Fusion.h"
#include "dense/DiskTable.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <chrono>
#include <deque>
#include <filesystem>
#include <fstream>
#include <future>
#include <thread>
#include <map>
#include <set>
#include <tuple>

namespace spirula::dense {
namespace {
namespace fs = std::filesystem;
constexpr int kRows = 16;
struct Record {
    uint32_t pixel = 0, target = 0;
    double anchor[2]{}, target_pixel[2]{}, point[3]{};
    float overlap = 0, precision[3]{}, color[6]{};
};

struct RefinedSurface {
    Surface surface;
    double spacing = 0;
    uint32_t pixel = 0, track_count = 0;
    double error = 0;
};


struct SourceRecord {
    int64_t source;
    Record record;
};

bool sample(const float* data, int w, int h, int c, double px, double py, float* out,
            bool periodic_input = false, bool periodic_output = false) {
    if (!std::isfinite(px) || !std::isfinite(py) || py < 0.5 || py > h - 0.5 ||
        (!periodic_input && (px < 0.5 || px > w - 0.5))) return false;
    if (periodic_input) { px = std::fmod(px,w); if (px < 0) px += w; }
    const double x = px - 0.5, y = py - 0.5;
    const int left = (int)std::floor(x), y0 = (int)y;
    const int x0 = periodic_input ? (left + w) % w : left;
    const int x1 = periodic_input ? (x0 + 1) % w : std::min(x0 + 1,w - 1), y1 = std::min(y0 + 1,h - 1);
    const double dx = x - left, dy = y - y0;
    for (int k = 0; k < c; ++k) {
        double values[4] = {data[((size_t)y0 * w + x0) * c + k],data[((size_t)y0 * w + x1) * c + k],
                            data[((size_t)y1 * w + x0) * c + k],data[((size_t)y1 * w + x1) * c + k]};
        if (periodic_output && k == 0)
            for (int i = 1; i < 4; ++i) values[i] = values[0] + std::remainder(values[i] - values[0],2.0);
        out[k] = (float)((1 - dy) * ((1 - dx) * values[0] + dx * values[1]) +
                           dy * ((1 - dx) * values[2] + dx * values[3]));
        if (!std::isfinite(out[k])) return false;
    }
    return true;
}

bool kept(const ViewPixels& pixels, double x, double y, bool periodic = false) {
    if (!std::isfinite(x) || !std::isfinite(y) || y < 0.5 || y > pixels.height - 0.5 ||
        (!periodic && (x < 0.5 || x > pixels.width - 0.5))) return false;
    if (periodic) { x = std::fmod(x,pixels.width); if (x < 0) x += pixels.width; }
    if (pixels.keep.empty()) return true;
    const int left = (int)std::floor(x - 0.5), y0 = (int)(y - 0.5);
    const int x0 = periodic ? (left + pixels.width) % pixels.width : left;
    const int x1 = periodic ? (x0 + 1) % pixels.width : std::min(x0 + 1,pixels.width - 1);
    for (int j = y0; j <= std::min(y0 + 1, pixels.height - 1); ++j)
        if (!pixels.keep[(size_t)j * pixels.width + x0] || !pixels.keep[(size_t)j * pixels.width + x1]) return false;
    return true;
}

sfm::Vec3 vector(const double p[3]) { return {p[0], p[1], p[2]}; }
void store(double out[3], const sfm::Vec3& p) { out[0] = p.x; out[1] = p.y; out[2] = p.z; }
}  // namespace

struct Reconstruction::Impl {
    fs::path work;
    const std::vector<View>& views;
    DenseConfig config;
    const std::atomic<bool>* cancel;
    ReconstructionStatistics stats;
    bool finished = false;
    int width, height;
    std::map<std::pair<uint32_t, int>, std::vector<Record>> pending;
    uint64_t pending_bytes = 0;
    fs::path preview_dir;
    std::ofstream preview_stream;
    PreviewCheckpoint preview;
    std::ofstream surface_output;
    std::vector<bool> completed;
    std::vector<double> spacings;
    uint64_t run_records = 1, pending_limit = 1;
    struct ReferenceResult { fs::path file, tracks; ReconstructionStatistics statistics; double seconds = 0; };
    FilterPlan plan;
    GeometryOptions support_geometry;
    bool two_image_rule = false, keep_tracks = false;
    std::unique_ptr<FreeSpaceGrid> free_space;
    uint64_t long_tracks = 0, kept_points = 0, observation_count = 0;
    std::ofstream two_image_output, observation_output, range_output, error_output;
    unsigned reference_workers = 1;
    uint64_t reference_budget = 1, published_references = 0;
    std::deque<std::future<ReferenceResult>> references;

    Impl(const std::string& dir, const std::vector<View>& v, const DenseConfig& c, const std::atomic<bool>* stop,
         const std::string& live_dir)
        : work(dir), views(v), config(c), cancel(stop), preview_dir(live_dir),
          width(c.match.high_width ? c.match.high_width : c.match.low_width),
          height(c.match.high_height ? c.match.high_height : c.match.low_height) {
        config.validate();
        config.image_cache_bytes = c.resolved_image_cache_bytes();
        config.geometry = c.resolved_geometry();
        // Without the id, so the spacing sample, and with it the automatic voxel, match a run before ids.
        run_records = std::max<uint64_t>(1, config.image_cache_bytes / (8 * (sizeof(Surface) - sizeof(Surface::id))));
        pending_limit = std::max<uint64_t>(sizeof(Record), config.image_cache_bytes / 8);
        const unsigned hardware = std::max(1u, spirula::hardware_threads());
        const uint64_t requested = c.matching_space == "source" && c.samples_per_reference ?
            std::min<uint64_t>(c.samples_per_reference, (uint64_t)width * height) : (uint64_t)width * kRows;
        const uint64_t working_bytes = std::max<uint64_t>(sizeof(Record), requested * 128);
        reference_workers = (unsigned)std::min<uint64_t>(std::min(c.cpu_workers ? (unsigned)c.cpu_workers : hardware, hardware),
            std::max<uint64_t>(1, config.image_cache_bytes / 4 / working_bytes));
        reference_workers = (unsigned)std::min<uint64_t>(reference_workers, std::max<size_t>(1, views.size()));
        reference_budget = std::max<uint64_t>(sizeof(Record), config.image_cache_bytes / 4 / reference_workers);
        stats.refinement_workers = reference_workers;
        completed.resize(views.size());
        for (const auto& view : views) view.validate();
        fs::create_directories(work);
        two_image_rule = config.two_image_active();
        keep_tracks = two_image_rule || config.reprojection_check;
        support_geometry = config.geometry;
        if (two_image_rule) support_geometry.min_source_images = 2;
        // Sized by the views, never by the budget: a budget must not change the cloud.
        if (two_image_rule && config.free_space_test) {
            free_space = std::make_unique<FreeSpaceGrid>(views);
            stats.free_space_bytes = free_space->bytes();
        }
        if (two_image_rule) {
            two_image_output.open(work / "two-image.bin", std::ios::binary | std::ios::trunc);
            error_output.open(work / "long-track-errors.bin", std::ios::binary | std::ios::trunc);
        }
        if (config.reprojection_check) {
            observation_output.open(work / "observations.bin", std::ios::binary | std::ios::trunc);
            range_output.open(work / "observation-ranges.bin", std::ios::binary | std::ios::trunc);
        }
        if ((two_image_rule && (!two_image_output || !error_output)) ||
            (config.reprojection_check && (!observation_output || !range_output)))
            throw std::runtime_error("cannot write dense point observations");
        surface_output.open(work / "surfaces.bin", std::ios::binary | std::ios::trunc);
        if (!surface_output) throw std::runtime_error("cannot write dense surfaces");
        open_preview("filtered");
    }

    void check() const {
        if (cancel && cancel->load()) throw std::runtime_error("dense processing cancelled");
    }

    fs::path tile(uint32_t view, int row) const {
        return work / ("observations-" + std::to_string(view) + "-" + std::to_string(row) + ".bin");
    }

    void append(uint32_t view, const Record& r) {
        if (completed.at(view)) throw std::runtime_error("dense reference was completed before its last pair");
        pending[std::make_pair(view, (int)(r.pixel / width) / kRows)].push_back(r);
        pending_bytes += sizeof r;
        if (pending_bytes >= pending_limit) flush();
    }

    void flush() {
        for (auto& [key, records] : pending) {
            if (records.empty()) continue;
            std::ofstream stream(tile(key.first, key.second), std::ios::binary | std::ios::app);
            stream.write(reinterpret_cast<const char*>(records.data()), (std::streamsize)(records.size() * sizeof(Record)));
            if (!stream) throw std::runtime_error("cannot write dense observation tile");
        }
        pending.clear();
        pending_bytes = 0;
    }

    void open_preview(const char* phase) {
        if (preview_dir.empty() || !preview.error.empty()) return;
        preview_stream.close();
        preview_stream.clear();
        preview.file = work.filename().string() + "-" + phase + ".points";
        preview.points = 0;
        preview.provisional = std::string(phase) == "observations";
        preview.filtered = std::string(phase) == "filtered";
        std::error_code error;
        fs::create_directories(preview_dir, error);
        preview_stream.open(preview_dir / preview.file, std::ios::binary | std::ios::trunc);
        if (!preview_stream) preview.error = "cannot open dense point checkpoint";
    }

    void remember(const double point[3], const float color[3]) {
        if (preview_dir.empty() || !preview.error.empty()) return;
        float xyz[3]; uint8_t rgb[3];
        for (int c = 0; c < 3; ++c) {
            xyz[c] = (float)point[c];
            rgb[c] = (uint8_t)std::lround(std::clamp(color[c], 0.f, 1.f) * 255);
        }
        preview_stream.write(reinterpret_cast<const char*>(xyz), sizeof xyz);
        preview_stream.write(reinterpret_cast<const char*>(rgb), sizeof rgb);
        if (preview_stream) ++preview.points;
        else preview.error = "cannot write dense point checkpoint";
    }

    void reset_preview() { open_preview("final"); }

    void direction(uint32_t a, uint32_t b, const ViewPixels& pa, const ViewPixels& pb,
                   const roma::Prediction& prediction, const roma::Prediction& reverse, bool reverse_anchor) {
        if (prediction.width != width || prediction.height != height ||
            prediction.warp.size() != (size_t)width * height * 2 ||
            prediction.overlap.size() != (size_t)width * height || prediction.precision.size() != (size_t)width * height * 3)
            throw std::runtime_error("dense prediction dimensions do not match resolved settings");
        const View& va = views.at(a), &vb = views.at(b);
        const bool periodic_a = camhost::full_longitude(va.camera), periodic_b = camhost::full_longitude(vb.camera);
        for (int y = 0; y < height; y += config.stride) {
            check();
            for (int x = 0; x < width; x += config.stride) {
                const size_t i = (size_t)y * width + x;
                ++stats.tested;
                const float overlap = prediction.overlap[i];
                if (!std::isfinite(overlap) || overlap < config.min_overlap || overlap <= 0 || overlap > 1) { ++stats.low_overlap; continue; }
                const double ax = (x + 0.5) * pa.width / width, ay = (y + 0.5) * pa.height / height;
                double tx = (prediction.warp[i * 2] + 1.0) * 0.5 * width;
                const double ty = (prediction.warp[i * 2 + 1] + 1.0) * 0.5 * height;
                double target[2] = {tx * pb.width / width,ty * pb.height / height};
                if (!camhost::normalize_pixel(vb.camera,target)) { ++stats.masked; continue; }
                const double bx = target[0], by = target[1]; tx = bx * width / pb.width;
                if (!kept(pa,ax,ay,periodic_a) || !kept(pb,bx,by,periodic_b)) { ++stats.masked; continue; }
                if (config.effective_cycle_check()) {
                    float warp[2], probability;
                    if (reverse.width != width || reverse.height != height ||
                        !sample(reverse.warp.data(),width,height,2,tx,ty,warp,periodic_b,periodic_a) ||
                        !sample(reverse.overlap.data(),width,height,1,tx,ty,&probability,periodic_b) || probability < config.min_overlap) {
                        ++stats.cycle; continue;
                    }
                    const auto delta = pixel_residual(va,{(warp[0] + 1) * 0.5 * pa.width,(warp[1] + 1) * 0.5 * pa.height},{ax,ay});
                    if (std::hypot(delta.x * width / pa.width,delta.y * height / pa.height) > config.max_cycle_error) {
                        ++stats.cycle; continue;
                    }
                }
                const double sx = (double)width / pb.width, sy = (double)height / pb.height;
                const float* p = &prediction.precision[i * 3];
                Observation oa{&va, {ax, ay}, overlap}, ob{&vb, {bx, by}, overlap, p[0] * sx * sx, p[1] * sx * sy, p[2] * sy * sy};
                if (config.matching_space == "source" && (!valid_observation(oa) || !valid_observation(ob))) {
                    ++stats.geometry; continue;
                }
                sfm::Vec3 point;
                if (config.matching_space != "source" && !triangulate(oa, ob, config.geometry, point)) { ++stats.geometry; continue; }
                Record r;
                r.pixel = (uint32_t)i; r.target = b;
                r.anchor[0] = ax; r.anchor[1] = ay; r.target_pixel[0] = bx; r.target_pixel[1] = by;
                store(r.point, point); r.overlap = overlap;
                r.precision[0] = (float)ob.precision_xx; r.precision[1] = (float)ob.precision_xy; r.precision[2] = (float)ob.precision_yy;
                if (!sample(pa.rgb.data(),pa.width,pa.height,3,ax,ay,r.color,periodic_a) ||
                    !sample(pb.rgb.data(),pb.width,pb.height,3,bx,by,r.color + 3,periodic_b)) { ++stats.masked; continue; }
                append(a, r);
                if (config.matching_space != "source") ++stats.triangulated;
                if (reverse_anchor) {
                    r.pixel = (uint32_t)((int)std::floor(ty) * width + (int)std::floor(tx)); r.target = a;
                    std::swap(r.anchor[0], r.target_pixel[0]); std::swap(r.anchor[1], r.target_pixel[1]);
                    r.precision[0] = r.precision[2] = 1; r.precision[1] = 0;
                    for (int k = 0; k < 3; ++k) std::swap(r.color[k], r.color[k + 3]);
                    append(b, r);
                }
            }
        }
    }

    bool refine(uint32_t reference, const DiskArray<Record>& input_records, Surface& surface, double& spacing,
                std::vector<TrackObservation>& track, double& error, ReconstructionStatistics& statistics, uint64_t budget) {
        const View& view = views[reference];
        sfm::Vec3 reference_ray;
        if (input_records.empty() || (view.source_camera &&
            !bearing(view, {input_records.front().anchor[0], input_records.front().anchor[1]}, reference_ray))) return false;
        // A wide lens spends few matcher cells per radian, so its depths scatter by more
        // than the relative band; whether the point explains the target pixel is model-independent.
        auto consistent = [&](const sfm::Vec3& point, const Record& r) {
            if (!view.source_camera)
                return depth_consistent(view, point, vector(r.point), config.geometry.max_relative_depth_error);
            const Observation target{&views[r.target], {r.target_pixel[0], r.target_pixel[1]}};
            return depth(view, point, &reference_ray) > 0 &&
                reprojection_error(target, point) <= 2 * config.geometry.max_reprojection_error;
        };
        DiskArray<Record> candidates(work / ("candidates-" + std::to_string(reference) + ".bin"), budget / 8);
        if (config.matching_space == "source") {
            for (auto record : input_records) {
                check();
                sfm::Vec3 point;
                const Observation a{&view, {record.anchor[0],record.anchor[1]}, record.overlap};
                const Observation b{&views[record.target], {record.target_pixel[0],record.target_pixel[1]}, record.overlap,
                                    record.precision[0],record.precision[1],record.precision[2]};
                if (!triangulate(a, b, config.geometry, point)) { ++statistics.geometry; continue; }
                store(record.point, point); candidates.push_back(record); ++statistics.triangulated;
            }
        }
        const auto& records = config.matching_space == "source" ? candidates : input_records;
        if (records.empty()) return false;
        size_t best = 0, best_count = 0;
        DiskArray<int64_t> sources(work / ("cluster-sources-" + std::to_string(reference) + ".bin"),budget / 16);
        for (size_t i = 0; i < records.size(); ++i) {
            check();
            sources.clear(); sources.push_back(view.source_image);
            const sfm::Vec3 point = vector(records[i].point);
            for (const auto& record : records) {
                check();
                if (consistent(point, record))
                    sources.push_back(views[record.target].source_image);
            }
            sources.sort(std::less<int64_t>{},budget / 8,[&] { check(); });
            size_t count = 0; int64_t previous = -1;
            for (const auto source : sources) if (source != previous) { ++count; previous = source; }
            if (count > best_count) { best_count = count; best = i; }
        }
        if (best_count < (size_t)support_geometry.min_source_images) { ++statistics.insufficient_support; return false; }
        const Record seed = records[best];
        DiskArray<SourceRecord> ranked(work / ("support-ranked-" + std::to_string(reference) + ".bin"),budget / 8);
        auto anchor = seed; anchor.target = reference;
        anchor.target_pixel[0] = seed.anchor[0]; anchor.target_pixel[1] = seed.anchor[1];
        anchor.precision[0] = anchor.precision[2] = 1; anchor.precision[1] = 0;
        for (int c = 0; c < 3; ++c) anchor.color[c + 3] = seed.color[c];
        ranked.push_back({view.source_image,anchor});
        for (const auto& r : records) {
            check();
            if (!consistent(vector(seed.point), r)) continue;
            sfm::Vec2 projected;
            if (!project(view, vector(r.point), projected)) continue;
            const auto residual = pixel_residual(view, projected, {seed.anchor[0],seed.anchor[1]});
            if (residual_length(view, residual) > config.geometry.max_reprojection_error) continue;
            const auto id = views[r.target].source_image;
            if (id != view.source_image) ranked.push_back({id,r});
        }
        ranked.sort([](const SourceRecord& a,const SourceRecord& b) {
            return a.source != b.source ? a.source < b.source : a.record.overlap > b.record.overlap;
        },budget / 8,[&] { check(); });
        DiskArray<Observation> observations(work / ("point-observations-" + std::to_string(reference) + ".bin"),budget / 8);
        DiskArray<std::array<float,3>> colors(work / ("point-colors-" + std::to_string(reference) + ".bin"),budget / 32);
        uint32_t reference_index = 0;
        sfm::Vec3 initial{}; double total = 0; int64_t previous = -1;
        for (const auto source_record : ranked) {
            check();
            const auto id = source_record.source; const auto& r = source_record.record;
            if (id == previous) continue;
            previous = id;
            if (id == view.source_image) reference_index = (uint32_t)observations.size();
            observations.push_back({&views[r.target], {r.target_pixel[0], r.target_pixel[1]}, r.overlap,
                                    r.precision[0], r.precision[1], r.precision[2]});
            colors.push_back({r.color[3], r.color[4], r.color[5]});
            if (id == view.source_image) continue;
            const auto* ptr = &r;
            sfm::Vec2 a, b;
            if (!project(view, vector(ptr->point), a) || !project(views[ptr->target], vector(ptr->point), b)) continue;
            const auto da = pixel_residual(view, a, {ptr->anchor[0],ptr->anchor[1]});
            const auto db = pixel_residual(views[ptr->target], b, {ptr->target_pixel[0],ptr->target_pixel[1]});
            const double weight = 1 / std::max(1e-8, std::max(residual_length(view, da), residual_length(views[ptr->target], db)));
            initial = initial + vector(ptr->point) * weight; total += weight;
        }
        PointEstimate result;
        DiskArray<uint32_t> retained(work / ("point-retained-" + std::to_string(reference) + ".bin"),budget / 32);
        if (!refine_unique_point(config.matching_space == "source" && total > 0 ? initial * (1 / total) : vector(seed.point),
                                 observations,support_geometry,retained,result,[&] { check(); })) {
            ++statistics.insufficient_support; return false;
        }
        bool retained_reference = false;
        for (const auto index : retained) retained_reference |= index == reference_index;
        if (!retained_reference) { ++statistics.insufficient_support; return false; }
        track.clear();
        std::vector<const View*> others;
        for (const auto index : retained) {
            const auto o = observations[index];
            track.push_back({(uint32_t)(o.view - views.data()), {(float)o.pixel.x, (float)o.pixel.y}});
            if (o.view->source_image != view.source_image) others.push_back(o.view);
        }
        double bar = plan.depth_precision;
        if (two_image_rule && plan.two_image_depth_precision > 0) {
            std::vector<int64_t> sources{view.source_image};
            for (const View* other : others) sources.push_back(other->source_image);
            std::sort(sources.begin(),sources.end());
            if (std::unique(sources.begin(),sources.end()) - sources.begin() == 2) bar = plan.two_image_depth_precision;
        }
        if (bar > 0 && !(best_depth_per_cell(result.position, view, others, width, height) <= bar)) {
            ++statistics.imprecise; return false;
        }
        error = result.max_reprojection_error;
        store(surface.point, result.position);
        surface.support = (uint32_t)retained.size();
        double weight = 0;
        for (uint32_t i : retained) {
            weight += observations[i].overlap;
            for (int c = 0; c < 3; ++c) surface.color[c] += (float)(observations[i].overlap * colors[i][c]);
        }
        for (float& c : surface.color) c /= (float)weight;
        const double z = depth(view, result.position, view.source_camera ? &reference_ray : nullptr);
        spacing = z * config.stride * std::sqrt((double)view.camera.width * view.camera.height /
                  ((double)width * height * view.camera.fx * view.camera.fy));
        if (view.source_camera) {
            sfm::Vec3 ray, rx, ry;
            const sfm::Vec2 px{seed.anchor[0],seed.anchor[1]};
            const double dx = (double)view.camera.width * config.stride / width;
            const double dy = (double)view.camera.height * config.stride / height;
            if (!bearing(view, px, ray)) return false;
            const bool has_x = bearing(view, {px.x + dx <= view.camera.width ? px.x + dx : px.x - dx, px.y}, rx);
            const bool has_y = bearing(view, {px.x, px.y + dy <= view.camera.height ? px.y + dy : px.y - dy}, ry);
            spacing = z * (has_x && has_y ? std::sqrt((ray - rx).norm() * (ray - ry).norm()) :
                            has_x ? (ray - rx).norm() : has_y ? (ray - ry).norm() : 0);
        }
        surface.radius = z * config.geometry.max_relative_depth_error;
        ++statistics.refined;
        return true;
    }

    void visit_tile(uint32_t reference, int row, uint64_t budget,
                    const std::function<void(uint32_t,const DiskArray<Record>&)>& consume) {
        const auto source = tile(reference, row);
        if (!fs::exists(source)) return;
        const auto ordered = source.string() + ".ordered";
        if (!fs::exists(ordered)) {
            auto compare = [](const Record& a, const Record& b) {
                return std::tie(a.pixel,a.target,a.anchor[0],a.anchor[1]) < std::tie(b.pixel,b.target,b.anchor[0],b.anchor[1]);
            };
            external_sort<Record>(source, ordered, budget / 2, compare, [&] { check(); });
        }
        std::ifstream input(ordered,std::ios::binary);
        Record record; DiskArray<Record> records(work / ("support-" + std::to_string(reference) + ".bin"), budget / 4);
        auto publish = [&] { if (!records.empty()) { check(); consume(records.front().pixel,records); records.clear(); } };
        while (read_disk_record(input,record)) {
            if (!records.empty() && records.front().pixel != record.pixel) publish();
            records.push_back(record);
        }
        publish();
    }

    ReferenceResult refine_reference(uint32_t v) {
        const auto started = std::chrono::steady_clock::now();
        ReferenceResult result{work / ("reference-" + std::to_string(v) + ".bin"), work / ("reference-tracks-" + std::to_string(v) + ".bin")};
        const uint64_t requested = std::min<uint64_t>(config.samples_per_reference, (uint64_t)width * height);
        std::set<uint32_t> selected;
        std::ifstream disk_selection;
        uint32_t selection_pixel = 0; bool selection_available = false;
        if (config.matching_space == "source" && requested) {
            auto visit = [&](const std::function<void(uint32_t,float)>& add) {
                for (int t = 0; t * kRows < height; ++t)
                    visit_tile(v,t,reference_budget,[&](uint32_t pixel,const auto& records) {
                        float score = 0; for (const auto& r : records) score = std::max(score,r.overlap);
                        add(pixel,score);
                    });
            };
            const auto seed = sampling_hash(config.sampling_seed ^ views[v].source_image);
            if (requested <= reference_budget / 128) {
                ReferenceSampling sampling(width,height,requested,seed);
                visit([&](auto pixel,auto score) { sampling.add(pixel,score); }); selected = sampling.selected();
            } else {
                ExternalReferenceSampling sampling(work / ("sampling-" + std::to_string(v)),width,height,requested,seed,reference_budget / 2);
                visit([&](auto pixel,auto score) { sampling.add(pixel,score); });
                disk_selection.open(sampling.selected([&] { check(); }),std::ios::binary);
                if (!disk_selection) throw std::runtime_error("cannot read dense reference selection");
                selection_available = read_disk_record(disk_selection,selection_pixel);
            }
        }
        DiskArray<RefinedSurface> accepted(work / ("accepted-" + std::to_string(v) + ".bin"), reference_budget / 4);
        DiskArray<TrackObservation> tracks(work / ("tracks-" + std::to_string(v) + ".bin"), reference_budget / 16);
        std::vector<TrackObservation> track;
        for (int t = 0; t * kRows < height; ++t) {
            visit_tile(v,t,reference_budget,[&](uint32_t pixel,const auto& records) {
                if (config.matching_space == "source" && requested) {
                    if (disk_selection.is_open()) {
                        while (selection_available && selection_pixel < pixel)
                            selection_available = read_disk_record(disk_selection,selection_pixel);
                        if (!selection_available || selection_pixel != pixel) return;
                    } else if (!selected.count(pixel)) return;
                }
                RefinedSurface p; p.pixel = pixel;
                if (refine(v,records,p.surface,p.spacing,track,p.error,result.statistics,reference_budget)) {
                    p.track_count = keep_tracks ? (uint32_t)track.size() : 0;
                    if (keep_tracks) for (const auto& o : track) tracks.push_back(o);
                    accepted.push_back(p);
                }
            });
        }
        auto find = [&](uint32_t pixel, Surface& surface) {
            uint64_t lo = 0, hi = accepted.size();
            while (lo < hi) {
                const auto mid = lo + (hi - lo) / 2;
                if (accepted[mid].pixel < pixel) lo = mid + 1; else hi = mid;
            }
            if (lo == accepted.size() || accepted[lo].pixel != pixel) return false;
            surface = accepted[lo].surface; return true;
        };
        std::ofstream output(result.file,std::ios::binary | std::ios::trunc);
        for (auto entry : accepted) {
            check(); auto& p = entry.surface; Surface px, py;
            const auto pixel = entry.pixel;
            if (pixel % width + config.stride < (uint32_t)width && pixel / width + config.stride < (uint32_t)height &&
                find(pixel + config.stride,px) && find(pixel + width * config.stride,py) &&
                depth_consistent(views[v],vector(p.point),vector(px.point),config.geometry.max_relative_depth_error) &&
                depth_consistent(views[v],vector(p.point),vector(py.point),config.geometry.max_relative_depth_error)) {
                const auto dx = vector(px.point) - vector(p.point), dy = vector(py.point) - vector(p.point);
                const sfm::Vec3 n{dx.y*dy.z-dx.z*dy.y,dx.z*dy.x-dx.x*dy.z,dx.x*dy.y-dx.y*dy.x};
                if (n.norm() > 1e-20) store(p.normal,n.normalized());
            }
            write_disk_record(output,entry);
        }
        output.flush(); if (!output) throw std::runtime_error("cannot write refined dense reference");
        if (keep_tracks) {
            std::ofstream track_output(result.tracks,std::ios::binary | std::ios::trunc);
            for (const auto o : tracks) write_disk_record(track_output,o);
            track_output.flush(); if (!track_output) throw std::runtime_error("cannot write dense point tracks");
        }
        // A completed reference's tiles are never read again, and they hold every observation of its pairs.
        for (int t = 0; t * kRows < height; ++t) {
            std::error_code error;
            const auto source = tile(v, t);
            fs::remove(source, error); fs::remove(source.string() + ".ordered", error);
        }
        result.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
        return result;
    }

    void publish_reference(const ReferenceResult& result) {
        stats.geometry += result.statistics.geometry;
        stats.triangulated += result.statistics.triangulated;
        stats.insufficient_support += result.statistics.insufficient_support;
        stats.reference_work_seconds += result.seconds;
        stats.imprecise += result.statistics.imprecise;
        {
            std::ifstream input(result.file,std::ios::binary), track_input;
            if (keep_tracks) track_input.open(result.tracks,std::ios::binary);
            if (!input || (keep_tracks && !track_input)) throw std::runtime_error("cannot read refined dense reference");
            RefinedSurface entry; std::vector<TrackObservation> track;
            while (read_disk_record(input,entry)) {
                check();
                read_track(track_input,entry,track);
                const auto sources = track_sources(track);
                if (two_image_rule && sources.size() == 2) {
                    write_disk_record(two_image_output,entry);
                    for (const auto& o : track) write_disk_record(two_image_output,o);
                    ++stats.two_image_candidates;
                    continue;
                }
                if (sources.size() >= 3 && two_image_rule) {
                    write_disk_record(error_output,entry.error); ++long_tracks;
                    if (free_space) free_space->observe(vector(entry.surface.point),sources);
                }
                emit(entry,track);
            }
        }
        fs::remove(result.file); if (keep_tracks) fs::remove(result.tracks); ++published_references;
    }

    void read_track(std::ifstream& input, const RefinedSurface& entry, std::vector<TrackObservation>& track) {
        track.resize(entry.track_count);
        for (auto& o : track)
            if (!read_disk_record(input,o) || o.view >= views.size()) throw std::runtime_error("truncated dense point track");
    }

    std::vector<int64_t> track_sources(const std::vector<TrackObservation>& track) const {
        std::vector<int64_t> sources;
        for (const auto& o : track) sources.push_back(views[o.view].source_image);
        std::sort(sources.begin(),sources.end());
        sources.erase(std::unique(sources.begin(),sources.end()),sources.end());
        return sources;
    }

    // Quality statistics describe what is published, so a dropped two-image candidate never enters them.
    void emit(RefinedSurface entry, const std::vector<TrackObservation>& track) {
        entry.surface.id = kept_points++; ++stats.refined;
        if (config.reprojection_check) {
            write_disk_record(range_output,ObservationRange{observation_count,track.size()});
            for (const auto& o : track) write_disk_record(observation_output,o);
            observation_count += track.size();
        }
        stats.max_reference_reprojection_error = std::max(stats.max_reference_reprojection_error,entry.error);
        stats.sum_reference_max_reprojection_error += entry.error;
        const uint64_t support = entry.surface.support;
        stats.min_reference_support = stats.min_reference_support ? std::min(stats.min_reference_support,support) : support;
        stats.max_reference_support = std::max(stats.max_reference_support,support);
        stats.sum_reference_support += support;
        const auto bin = std::min<size_t>(stats.reference_reprojection_histogram.size() - 1,
            (size_t)(entry.error / config.geometry.max_reprojection_error * stats.reference_reprojection_histogram.size()));
        ++stats.reference_reprojection_histogram[bin];
        if (entry.spacing > 0 && std::isfinite(entry.spacing)) {
            if (spacings.size() < run_records) spacings.push_back(entry.spacing);
            else { const auto index = sampling_hash(kept_points) % kept_points; if (index < spacings.size()) spacings[index] = entry.spacing; }
        }
        write_disk_record(surface_output,entry.surface); remember(entry.surface.point,entry.surface.color);
    }

    // Exact, from disk: a sample sized by the budget would let the budget change the cloud.
    double long_track_median() {
        flush_disk_output(error_output); error_output.close();
        const double median = external_median(work / "long-track-errors.bin", long_tracks, config.image_cache_bytes / 8, [&] { check(); });
        fs::remove(work / "long-track-errors.bin");
        return median;
    }

    // The bar is the median error of the run's three-image points, so it waits for every reference.
    void admit_two_image_points() {
        two_image_output.flush(); if (!two_image_output) throw std::runtime_error("dense two-image point write failed");
        two_image_output.close();
        stats.two_image_bar = long_track_median();
        std::ifstream input(work / "two-image.bin",std::ios::binary);
        struct Candidate { RefinedSurface entry; std::vector<TrackObservation> track; std::vector<int64_t> sources; char verdict = 0; };
        std::vector<Candidate> block;
        const unsigned workers = std::max(1u,std::min(config.cpu_workers ? (unsigned)config.cpu_workers : spirula::hardware_threads(),
                                                      std::max(1u,spirula::hardware_threads())));
        auto judge = [&] {
            std::atomic<size_t> next{0};
            auto work_on = [&] {
                for (size_t i; (i = next.fetch_add(1)) < block.size();) {
                    auto& c = block[i];
                    if (!(c.entry.error <= stats.two_image_bar)) c.verdict = 'e';
                    else if (free_space && free_space->sees_through(vector(c.entry.surface.point),c.sources)) c.verdict = 's';
                    else c.verdict = 'k';
                }
            };
            std::vector<std::future<void>> tasks;
            for (unsigned w = 1; w < workers; ++w) tasks.push_back(std::async(std::launch::async,work_on));
            work_on(); for (auto& t : tasks) t.get();
            for (auto& c : block) {
                if (c.verdict == 'e') ++stats.two_image_over_bar;
                else if (c.verdict == 's') ++stats.seen_through;
                else { ++stats.two_image_admitted; emit(c.entry,c.track); }
            }
            block.clear();
        };
        Candidate c;
        while (read_disk_record(input,c.entry)) {
            check();
            read_track(input,c.entry,c.track);
            c.sources = track_sources(c.track);
            block.push_back(c);
            if (block.size() == 65536) judge();
        }
        judge();
        input.close(); fs::remove(work / "two-image.bin");
    }

    void drain_references(bool wait) {
        while (!references.empty()) {
            if (!wait && references.front().wait_for(std::chrono::seconds(0)) != std::future_status::ready) break;
            auto result = references.front().get(); references.pop_front(); publish_reference(result);
            if (wait) break;
        }
    }

    void complete_reference(uint32_t v, bool background = false) {
        if (v >= views.size() || completed[v]) throw std::runtime_error("invalid or repeated dense reference completion");
        flush(); completed[v] = true;
        if (!background) {
            while (!references.empty()) drain_references(true);
            publish_reference(refine_reference(v));
        } else {
            while (references.size() >= reference_workers) drain_references(true);
            references.push_back(std::async(std::launch::async,[this,v] { return refine_reference(v); }));
            drain_references(false);
        }
    }

    void surfaces(const DenseProgress& progress) {
        for (uint32_t v = 0; v < views.size(); ++v) {
            if (!completed[v]) complete_reference(v,true);
            if (progress) progress("refine",published_references,views.size());
        }
        while (!references.empty()) { drain_references(true); if (progress) progress("refine",published_references,views.size()); }
        if (two_image_rule) admit_two_image_points();
        if (config.reprojection_check) {
            observation_output.flush(); range_output.flush();
            if (!observation_output || !range_output) throw std::runtime_error("dense point observation write failed");
            observation_output.close(); range_output.close();
        }
        surface_output.flush(); if (!surface_output) throw std::runtime_error("dense surface write failed");
        surface_output.close();
        if (!kept_points) throw std::runtime_error("dense filtering produced no points; check overlap, baseline, masks and distinct-image support");
        if (config.voxel_size > 0) stats.resolved_voxel_size = config.voxel_size;
        else {
            if (spacings.empty()) throw std::runtime_error("cannot determine dense point spacing");
            const auto median = spacings.begin() + spacings.size() / 2;
            std::nth_element(spacings.begin(), median, spacings.end());
            stats.resolved_voxel_size = *median;
        }
        std::vector<double>().swap(spacings);
    }

    void fuse(const std::string& ply, const ParsedDataset& dataset, const DenseProgress& progress) {
        const auto fused = work / "fused.bin";
        const auto result = fuse_surfaces(work / "surfaces.bin",fused,work,stats.resolved_voxel_size,
            config.image_cache_bytes,(unsigned)config.cpu_workers,[&] { check(); },
            [&](uint64_t done,uint64_t total) { if (progress) progress("fuse",done,total); });
        stats.fused = result.surfaces;
        stats.fusion_workers = result.workers; stats.fusion_partitions = result.partitions;
        stats.fusion_boundary_candidates = result.boundary_candidates;
        const uint64_t records = drop_far_isolated(fused, dataset);
        std::ofstream exported_ids;
        if (config.reprojection_check) exported_ids.open(work / "exported-ids.bin", std::ios::binary | std::ios::trunc);
        PointCloudWriter writer(ply, PlyCoordinates::Float64);
        reset_preview();
        if (!config.remove_outliers) {
            std::ifstream input(fused,std::ios::binary); Surface p;
            uint64_t read = 0;
            while (read_disk_record(input,p)) {
                check();
                if (progress && ++read % 65536 == 0) progress("export",read,records);
                if (config.point_limit && writer.count() >= config.point_limit) continue;
                double raw[3], xyz[3];
                for (int c = 0; c < 3; ++c) raw[c] = p.point[c] + dataset.center[c];
                for (int c = 0; c < 3; ++c) {
                    xyz[c] = dataset.raw_to_file[c * 4 + 3];
                    for (int k = 0; k < 3; ++k) xyz[c] += dataset.raw_to_file[c * 4 + k] * raw[k];
                }
                uint8_t rgb[3];
                for (int c = 0; c < 3; ++c) rgb[c] = (uint8_t)std::lround(std::clamp(p.color[c], 0.f, 1.f) * 255);
                writer.append(xyz, rgb); if (exported_ids.is_open()) write_disk_record(exported_ids, p.id);
                remember(p.point, p.color);
            }
        }
        if (config.remove_outliers) {
            if (records > INT32_MAX) throw std::runtime_error("dense exact outlier index exceeds its int32 address range");
            const uint64_t index_bytes = records * (4 * sizeof(float) + sizeof(int32_t) + sizeof(uint8_t));
            const int neighbors = (int)std::min<uint64_t>(config.outlier_neighbors, records - 1);
            const uint64_t query_bytes = (uint64_t)neighbors * sizeof(float);
            const uint64_t required = index_bytes + query_bytes;
            if (required > config.image_cache_bytes)
                throw std::runtime_error("dense exact outlier filtering requires " + std::to_string(required) +
                    " bytes; host cache budget is " + std::to_string(config.image_cache_bytes) + " bytes");
            const unsigned hardware = std::max(1u, spirula::hardware_threads());
            const unsigned workers = (unsigned)std::min<uint64_t>(std::min(config.cpu_workers ? (unsigned)config.cpu_workers : hardware, hardware),
                std::min<uint64_t>((records + 255) / 256, std::max<uint64_t>(1,(config.image_cache_bytes - index_bytes) / std::max<uint64_t>(1,query_bytes))));
            std::ifstream input(work / "fused.bin", std::ios::binary);
            std::vector<float> xyz;
            xyz.reserve((size_t)records * 3);
            sfm::Vec3 origin;
            Surface p;
            while (read_disk_record(input, p)) {
                if (xyz.empty()) origin = vector(p.point);
                const auto delta = vector(p.point) - origin;
                for (double coordinate : {delta.x, delta.y, delta.z}) {
                    if (!std::isfinite((float)coordinate)) throw std::runtime_error("dense outlier query exceeds coordinate range");
                    xyz.push_back((float)coordinate);
                }
            }
            knn::KdTree3 tree(xyz.data(), (int64_t)records);
            std::vector<float> means((size_t)records);
            std::atomic<uint64_t> next{0};
            std::mutex report;
            auto query = [&] {
                if (!neighbors) return;
                std::vector<float> distances((size_t)neighbors);
                for (;;) {
                    const auto begin = next.fetch_add(256);
                    if (begin >= records) return;
                    if (progress && begin % 65536 == 0) {
                        std::lock_guard<std::mutex> lock(report);
                        progress("outliers",begin,records);
                    }
                    for (uint64_t i = begin; i < std::min(begin + 256, records); ++i) {
                        check();
                        const int count = tree.query(&xyz[(size_t)i * 3], (int32_t)i, neighbors, distances.data());
                        double sum = 0;
                        for (int j = 0; j < count; ++j) sum += std::sqrt(distances[j]);
                        means[(size_t)i] = (float)(count ? sum / count : 0);
                    }
                }
            };
            std::vector<std::future<void>> queries;
            for (unsigned i = 1; i < workers; ++i) queries.push_back(std::async(std::launch::async, query));
            query();
            for (auto& task : queries) task.get();
            double mean = 0, moment = 0;
            for (uint64_t i = 0; i < records; ++i) {
                const double value = means[(size_t)i];
                const double delta = value - mean; mean += delta / (i + 1); moment += delta * (value - mean);
            }
            const double threshold = mean + config.outlier_stddev * std::sqrt(moment / std::max<uint64_t>(1, records));
            input.clear(); input.seekg(0);
            uint64_t index = 0;
            while (read_disk_record(input, p)) {
                check();
                if (progress && (index + 1) % 65536 == 0) progress("export",index + 1,records);
                if (means[(size_t)index++] > threshold || (config.point_limit && writer.count() >= config.point_limit)) continue;
                double raw[3], xyz_source[3];
                for (int c = 0; c < 3; ++c) raw[c] = p.point[c] + dataset.center[c];
                for (int c = 0; c < 3; ++c) {
                    xyz_source[c] = dataset.raw_to_file[c * 4 + 3];
                    for (int k = 0; k < 3; ++k) xyz_source[c] += dataset.raw_to_file[c * 4 + k] * raw[k];
                }
                uint8_t rgb[3];
                for (int c = 0; c < 3; ++c) rgb[c] = (uint8_t)std::lround(std::clamp(p.color[c], 0.f, 1.f) * 255);
                writer.append(xyz_source, rgb); if (exported_ids.is_open()) write_disk_record(exported_ids, p.id);
                remember(p.point, p.color);
            }
        }
        writer.finish();
        if (exported_ids.is_open()) { flush_disk_output(exported_ids); exported_ids.close(); }
        stats.exported = writer.count();
        if (!stats.exported) throw std::runtime_error("dense fusion and outlier filtering produced an empty cloud");
        if (config.reprojection_check) stats.reprojection = check_written(ply, dataset);
        if (progress) progress("export", records, records);
    }

    // Rewrites `fused` without the far isolated points; returns the records left.
    uint64_t drop_far_isolated(const fs::path& fused, const ParsedDataset& dataset) {
        if (!config.far_isolated) return stats.fused;
        stats.far_filter = resolve_far_filter(dataset.points.xyz, dataset.gauge_metric);
        if (!stats.far_filter.active()) return stats.fused;
        // Only points beyond margin - radius can neighbour a far point, so only they are held.
        const double reach = stats.far_filter.margin - stats.far_filter.radius;
        std::vector<sfm::Vec3> outer; std::vector<uint64_t> index;
        {
            std::ifstream input(fused,std::ios::binary); Surface p; uint64_t i = 0;
            while (read_disk_record(input,p)) {
                check();
                if (box_excess(stats.far_filter,vector(p.point)) > reach) { outer.push_back(vector(p.point)); index.push_back(i); }
                ++i;
            }
        }
        std::vector<uint64_t> drop;
        for (const auto i : far_isolated(outer,stats.far_filter,&stats.far_beyond)) drop.push_back(index[i]);
        stats.far_isolated = drop.size();
        if (drop.empty()) return stats.fused;
        std::sort(drop.begin(),drop.end());
        const auto kept_path = work / "fused-kept.bin";
        {
            std::ifstream input(fused,std::ios::binary); std::ofstream output(kept_path,std::ios::binary | std::ios::trunc);
            Surface p; uint64_t i = 0; size_t next = 0;
            while (read_disk_record(input,p)) {
                check();
                if (next < drop.size() && drop[next] == i) ++next; else write_disk_record(output,p);
                ++i;
            }
            flush_disk_output(output);
        }
        fs::rename(kept_path,fused);
        return stats.fused - drop.size();
    }

    ReprojectionSummary check_written(const std::string& ply, const ParsedDataset& dataset) {
        return reproject_written(ply, work / "exported-ids.bin", work / "observation-ranges.bin", work / "observations.bin",
                                 views, dataset.raw_to_file, dataset.center, width, height, config.image_cache_bytes / 16,
                                 [&] { check(); });
    }
};

Reconstruction::Reconstruction(const std::string& dir, const std::vector<View>& views, const DenseConfig& config,
                               const std::atomic<bool>* cancel, const std::string& preview_dir)
    : impl_(std::make_unique<Impl>(dir, views, config, cancel, preview_dir)) {}
Reconstruction::~Reconstruction() = default;

void Reconstruction::add_pair(uint32_t a, uint32_t b, const ViewPixels& pa, const ViewPixels& pb, const roma::PairPrediction& prediction) {
    if (impl_->finished) throw std::runtime_error("dense reconstruction is already finished");
    if (a >= impl_->views.size() || b >= impl_->views.size() || impl_->views[a].source_image == impl_->views[b].source_image)
        throw std::runtime_error("dense pair must name two distinct source images");
    auto validate = [](const ViewPixels& p, const View& v) {
        if (p.width != v.camera.width || p.height != v.camera.height || p.rgb.size() != (size_t)p.width * p.height * 3 ||
            (!p.keep.empty() && p.keep.size() != (size_t)p.width * p.height)) throw std::runtime_error("invalid dense view pixels");
    };
    validate(pa, impl_->views[a]); validate(pb, impl_->views[b]);
    if (impl_->config.effective_cycle_check() &&
        (prediction.backward.warp.size() != prediction.forward.warp.size() || prediction.backward.overlap.size() != prediction.forward.overlap.size()))
        throw std::runtime_error("dense cycle checking requires a reverse prediction");
    impl_->direction(a, b, pa, pb, prediction.forward, prediction.backward, prediction.backward.width == 0 && impl_->config.matching_space != "source");
    if (prediction.backward.width && impl_->config.matching_space != "source") impl_->direction(b, a, pb, pa, prediction.backward, prediction.forward, false);
}

void Reconstruction::set_filter_plan(const FilterPlan& plan) {
    if (impl_->finished || impl_->stats.tested) throw std::runtime_error("dense filter plan must precede the first pair");
    impl_->plan = plan;
    impl_->stats.depth_precision = plan.depth_precision;
}

uint64_t Reconstruction::filter_bytes() const { return impl_->stats.free_space_bytes; }

void Reconstruction::complete_reference(uint32_t reference, bool background) {
    if (impl_->finished) throw std::runtime_error("dense reconstruction is already finished");
    impl_->complete_reference(reference, background);
}

PreviewCheckpoint Reconstruction::checkpoint() {
    impl_->drain_references(false);
    if (impl_->preview_stream.is_open() && impl_->preview.error.empty()) {
        impl_->preview_stream.flush();
        if (!impl_->preview_stream) impl_->preview.error = "cannot flush dense point checkpoint";
    }
    return impl_->preview;
}

ReconstructionStatistics Reconstruction::finish(const std::string& path, const ParsedDataset& dataset, const DenseProgress& progress) {
    if (impl_->finished) throw std::runtime_error("dense reconstruction is already finished");
    impl_->finished = true;
    impl_->surfaces(progress);
    impl_->fuse(path, dataset, progress);
    return impl_->stats;
}

}  // namespace spirula::dense
