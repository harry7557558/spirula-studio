#include "app/DenseProcessing.h"
#include "dense/Artifact.h"

#include "app/FrameLook.h"
#include "app/SystemRecorder.h"
#include "app/GeometryWarp.h"
#include "core/AtomicFile.h"
#include "core/FileLock.h"
#include "core/OwnedDirectory.h"
#include "core/DistanceTransform.h"
#include "core/Env.h"
#include "core/ImageFile.h"
#include "core/Sha256.h"
#include "data/ImageProbe.h"
#include "data/SparseEdit.h"
#include "dense/ConfigFields.h"
#include "dense/DiskTable.h"
#include "dense/MemoryReport.h"
#include "external/stb_image.h"
#include "nn/Device.h"
#include "nn/io/Fetch.h"
#include "nn/io/Image.h"
#include "nn/vk/Context.h"
#include "nn/vk/Memory.h"
#include "roma/model/Fetch.h"
#include "core/HostMemory.h"

#include <chrono>
#include <algorithm>
#include <array>
#include <cmath>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <fstream>
#include <functional>
#include <future>
#include <iomanip>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <thread>
#include <unordered_map>

namespace app {
namespace {
namespace fs = std::filesystem;
using namespace spirula;
using namespace spirula::dense;

void check_cancel(const std::atomic<bool>* cancel) {
    if (cancel && cancel->load()) throw std::runtime_error("dense processing cancelled");
}

std::string digest(const std::string& bytes) {
    Sha256 hash;
    hash.update(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size());
    return hash.hex();
}

void write_text(const fs::path& path, const std::string& text) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << text; out.flush();
    if (!out) throw std::runtime_error("cannot write " + path.string());
}

void write_diagnostic_array(const fs::path& target, const void* data, size_t bytes) {
    const auto part = fs::path(target.string() + ".part");
    std::ofstream output(part,std::ios::binary | std::ios::trunc);
    output.write(static_cast<const char*>(data),(std::streamsize)bytes);
    output.flush();
    if (!output) throw std::runtime_error("cannot write dense diagnostic array");
    output.close(); replace_file(part,target);
}

void dump_source_pixels(size_t id, const std::string& image, const ViewPixels& pixels) {
    const char* directory = spirula::env("DENSE_DUMP_VIEWS");
    if (!directory || !*directory) return;
    static std::mutex mutex;
    std::lock_guard<std::mutex> lock(mutex);
    const fs::path root = fs::u8path(directory);
    fs::create_directories(root);
    const std::string stem = "source-" + std::to_string(id);
    write_diagnostic_array(root / (stem + "-rgb.f32"),pixels.rgb.data(),pixels.rgb.size() * sizeof(float));
    write_diagnostic_array(root / (stem + "-keep.u8"),pixels.keep.data(),pixels.keep.size());
    JsonWriter metadata;
    metadata.object().field("source_image",(long long)id).field("image",image);
    metadata.field("width",pixels.width).field("height",pixels.height);
    metadata.field("rgb",stem + "-rgb.f32").field("keep",stem + "-keep.u8").end();
    const auto target = root / (stem + ".json"), part = fs::path(target.string() + ".part");
    write_text(part,metadata.str()); replace_file(part,target);
}

void dump_pair_prediction(uint32_t a, uint32_t b, const roma::PairPrediction& prediction) {
    const char* directory = spirula::env("DENSE_DUMP_PAIRS");
    if (!directory || !*directory) return;
    const fs::path root = fs::u8path(directory) / ("pair-" + std::to_string(a) + "-" + std::to_string(b));
    fs::create_directories(root);
    for (const auto& direction : {std::make_pair(&prediction.forward,"AB"),std::make_pair(&prediction.backward,"BA")}) {
        const auto& p = *direction.first;
        if (p.warp.empty()) continue;
        const std::string stem = std::string("profile_") + direction.second + "_";
        write_diagnostic_array(root / (stem + "warp.f32"),p.warp.data(),p.warp.size() * sizeof(float));
        write_diagnostic_array(root / (stem + "overlap.f32"),p.overlap.data(),p.overlap.size() * sizeof(float));
        write_diagnostic_array(root / (stem + "precision.f32"),p.precision.data(),p.precision.size() * sizeof(float));
    }
    JsonWriter metadata;
    metadata.object().field("a",(long long)a).field("b",(long long)b).field("width",prediction.forward.width).field("height",prediction.forward.height);
    metadata.field("bidirectional",!prediction.backward.warp.empty()).end();
    const auto target = root / "pair.json", part = fs::path(target.string() + ".part");
    write_text(part,metadata.str()); replace_file(part,target);
}

unsigned worker_count(const DenseConfig& config) {
    const unsigned hardware = std::max(1u, std::thread::hardware_concurrency());
    return std::min(config.cpu_workers > 0 ? (unsigned)config.cpu_workers : hardware, hardware);
}

void parallel_for(size_t n, unsigned workers, const std::function<void(size_t)>& body) {
    std::atomic<size_t> next{0};
    std::mutex mutex;
    std::exception_ptr failure;
    auto run = [&] {
        for (size_t i; (i = next.fetch_add(1)) < n;) {
            try { body(i); }
            catch (...) {
                std::lock_guard<std::mutex> lock(mutex);
                if (!failure) failure = std::current_exception();
                next.store(n);
            }
        }
    };
    std::vector<std::thread> threads;
    for (size_t t = 1; t < std::min<size_t>(workers, n); ++t) threads.emplace_back(run);
    run();
    for (auto& thread : threads) thread.join();
    if (failure) std::rethrow_exception(failure);
}

class Fingerprints {
public:
    explicit Fingerprints(fs::path file) : file_(std::move(file)) {
        std::ifstream in(file_);
        std::string line;
        while (std::getline(in, line)) {
            std::istringstream row(line);
            Entry entry; std::string path;
            if (row >> entry.sha >> entry.size >> entry.mtime && std::getline(row >> std::ws, path) && entry.sha.size() == 64)
                entries_[path] = entry;
        }
    }

    std::string get(const std::string& file) {
        const auto path = fs::absolute(file).lexically_normal().string();
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (verified_this_run_.count(path)) return entries_.at(path).sha;
        }
        std::error_code error;
        const uint64_t size = fs::file_size(path, error);
        if (error) return {};
        const int64_t mtime = (int64_t)fs::last_write_time(path, error).time_since_epoch().count();
        if (error) return {};
        const auto sha = spirula::sha256_file(path);
        if (sha.empty()) return {};
        std::lock_guard<std::mutex> lock(mutex_);
        entries_[path] = {sha, size, mtime};
        verified_this_run_.insert(path);
        dirty_ = true;
        return sha;
    }

    void save() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!dirty_) return;
        std::ostringstream out;
        for (const auto& [path, entry] : entries_) out << entry.sha << ' ' << entry.size << ' ' << entry.mtime << ' ' << path << '\n';
        try {
            const fs::path part = file_.string() + ".part";
            write_text(part, out.str());
            replace_file(part, file_);
            dirty_ = false;
        } catch (const std::exception&) {}
    }

private:
    struct Entry { std::string sha; uint64_t size = 0; int64_t mtime = 0; };
    fs::path file_;
    std::mutex mutex_;
    std::map<std::string, Entry> entries_;
    std::set<std::string> verified_this_run_;
    bool dirty_ = false;
};

std::string checkpoint_path(const std::string& name, Fingerprints& fingerprints, std::string& hash) {
    if (name != "romav2.0.1") {
        if (!fs::is_regular_file(name)) throw std::runtime_error("RoMa checkpoint does not exist: " + name);
        hash = fingerprints.get(name);
        return fs::absolute(name).string();
    }
    const auto path = fs::path(nn::model_cache_dir()) / roma::kOfficialCheckpoint.file;
    if (!fs::is_regular_file(path))
        throw std::runtime_error("RoMa weights are missing; provide --checkpoint PATH or install romav2.0.1.pt in the model cache after reviewing the RoMa and DINOv3 terms");
    hash = fingerprints.get(path.string());
    if (hash != roma::kOfficialCheckpoint.sha256) throw std::runtime_error("cached RoMa checkpoint SHA-256 mismatch");
    return path.string();
}

GeometryCamera camera(const ParsedDataset& ds, size_t i) {
    GeometryCamera c;
    c.model = ds.camera_models[i]; c.distortion = ds.camera_distortions[i];
    c.width = ds.widths[i]; c.height = ds.heights[i];
    c.fx = ds.intrins[i * 4]; c.fy = ds.intrins[i * 4 + 1];
    c.cx = ds.intrins[i * 4 + 2]; c.cy = ds.intrins[i * 4 + 3];
    std::copy_n(ds.dist_coeffs.data() + i * 8, 8, c.dist);
    if (!ds.redistort.empty()) {
        c.source_model = ds.redistort[i].source_model;
        std::copy_n(ds.redistort[i].params, 16, c.source_params);
    }
    return c;
}

// Half the diagonal field of view; fisheye and equisolid focals are per radian, not per tangent.
double half_fov(const GeometryCamera& c) {
    constexpr double pi = 3.141592653589793;
    if (c.model == (int)CameraModelType::EQUIRECTANGULAR) return pi;
    const double r = 0.5 * std::hypot(c.width / (double)c.fx, c.height / (double)c.fy);
    const double theta = c.model == (int)CameraModelType::FISHEYE ? r :
        c.model == (int)CameraModelType::EQUISOLID ? 2 * std::asin(std::min(1.0, 0.5 * r)) : std::atan(r);
    return std::clamp(theta, 0.1, pi);
}

GeometryWarp warp_for(const GeometryCamera& c, const DenseConfig& config) {
    GeometryWarp warp;
    const double scale = std::min(1.0, (double)config.max_face_size / std::max(c.width, c.height));
    const int w = std::max(16, (int)(c.width * scale) / 16 * 16), h = std::max(16, (int)(c.height * scale) / 16 * 16);
    const bool split = config.split_views && camhost::splits_to_pinhole_faces(c.model, c.width, c.height, c.fx, c.fy);
    warp.plan(c, w, h, split, 16, config.max_face_size, FaceRes::Output, 0, true, FaceLayout::Cube);
    return warp;
}

// A plan costs ~57 ms at 1280-pixel faces and captures usually share one camera.
class WarpCache {
public:
    explicit WarpCache(const DenseConfig& config) : config_(config) {}

    std::shared_ptr<const GeometryWarp> get(const GeometryCamera& camera) {
        const std::string key(reinterpret_cast<const char*>(&camera), sizeof camera);
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto it = entries_.begin(); it != entries_.end(); ++it) if (it->first == key) {
            auto warp = it->second;
            entries_.erase(it); entries_.emplace_back(key, warp);
            return warp;
        }
        auto warp = std::make_shared<const GeometryWarp>(warp_for(camera, config_));
        while (!entries_.empty() && bytes_ + warp->bytes() > config_.image_cache_bytes) {
            bytes_ -= entries_.front().second->bytes();
            entries_.pop_front();
        }
        bytes_ += warp->bytes();
        entries_.emplace_back(key, warp);
        return warp;
    }

private:
    const DenseConfig& config_;
    std::mutex mutex_;
    std::deque<std::pair<std::string, std::shared_ptr<const GeometryWarp>>> entries_;
    uint64_t bytes_ = 0;
};

struct Prepared {
    View view;
    size_t image = 0;
    int face = 0;
    std::string identity;
    std::string cache_identity;
    std::vector<uint64_t> visible_points;
};

// Empty when the file has no alpha channel; a JPEG is not decoded twice to find that out.
std::vector<uint8_t> alpha(const std::string& path, int& w, int& h) {
    std::vector<uint8_t> rgba;
    int ch = 0;
    if (!imagefile::handles(path) && stbi_info(path.c_str(), &w, &h, &ch) && ch != 2 && ch != 4) return {};
    if (imagefile::handles(path)) {
        imagefile::Info info;
        imagefile::Options options; options.channels = 4;
        const auto error = imagefile::decode_srgb8(path, options, info, rgba);
        if (!error.empty()) throw std::runtime_error(error);
        w = info.width; h = info.height; ch = info.channels;
    } else {
        uint8_t* decoded = stbi_load(path.c_str(), &w, &h, &ch, 4);
        if (!decoded) throw std::runtime_error("cannot decode image alpha: " + path);
        rgba.assign(decoded, decoded + (size_t)w * h * 4);
        stbi_image_free(decoded);
    }
    if (ch != 2 && ch != 4) return {};
    std::vector<uint8_t> out((size_t)w * h);
    for (size_t i = 0; i < out.size(); ++i) out[i] = rgba[i * 4 + 3];
    return out;
}

std::vector<uint8_t> mask(const std::string& path, int& w, int& h) {
    if (imagefile::handles(path)) {
        imagefile::Info info;
        imagefile::Options options; options.channels = 1;
        std::vector<uint8_t> out;
        const auto error = imagefile::decode_srgb8(path, options, info, out, "", false);
        if (!error.empty()) throw std::runtime_error(error);
        w = info.width; h = info.height;
        return out;
    }
    int channels;
    uint8_t* decoded = stbi_load(path.c_str(), &w, &h, &channels, 1);
    if (!decoded) throw std::runtime_error("cannot decode dense mask: " + path);
    std::vector<uint8_t> out(decoded, decoded + (size_t)w * h);
    stbi_image_free(decoded);
    return out;
}

// Decodes views on worker threads ahead of the GPU, within image_cache_bytes.
class Pixels {
public:
    using Future = std::shared_future<std::shared_ptr<const ViewPixels>>;

    Pixels(const ParsedDataset& ds, const std::vector<Prepared>& prepared, const std::string& root,
           const DenseConfig& config, WarpCache& warps, unsigned workers)
        : ds_(ds), prepared_(prepared), root_(root), config_(config), warps_(warps) {
        if (!colorspace::parse_exposure(config.image_exposure, exposure_)) throw std::runtime_error("invalid dense image exposure");
        for (unsigned i = 0; i < std::max(1u, workers); ++i) threads_.emplace_back([this] { run(); });
    }

    ~Pixels() { release(); }

    void release() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            quit_ = true; tasks_.clear();
        }
        wake_.notify_all();
        for (auto& thread : threads_) if (thread.joinable()) thread.join();
        cache_.clear(); sources_.clear(); bytes_ = 0;
    }

    uint64_t bytes(uint32_t id) const {
        const auto& camera = prepared_.at(id).view.camera;
        return (uint64_t)camera.width * camera.height * (3 * sizeof(float) + 1);
    }

    uint64_t sourceBytes(uint32_t id) const {
        const auto c = camera(ds_, prepared_.at(id).image);
        if (config_.matching_space == "source") return 0;
        const auto warp = warps_.get(c);
        return (uint64_t)warp->sampleWidth() * warp->sampleHeight() * 3 * sizeof(float) + (uint64_t)c.width * c.height;
    }

    size_t sourceImage(uint32_t id) const { return prepared_.at(id).image; }

    // Schedules `ids` in order and drops other decoded views while over budget.
    void prefetch(const std::vector<uint32_t>& ids) {
        std::lock_guard<std::mutex> lock(mutex_);
        ++clock_;
        for (uint32_t id : ids) {
            const auto it = cache_.find(id);
            if (it == cache_.end()) schedule(id);
            else it->second.used = clock_;
        }
        const std::set<uint32_t> keep(ids.begin(), ids.end());
        std::set<size_t> keep_sources;
        for (uint32_t id : ids) keep_sources.insert(sourceImage(id));
        while (bytes_ > config_.image_cache_bytes) {
            auto victim = cache_.end();
            for (auto it = cache_.begin(); it != cache_.end(); ++it)
                if (!keep.count(it->first) && (victim == cache_.end() || it->second.used < victim->second.used)) victim = it;
            auto source_victim = sources_.end();
            for (auto it = sources_.begin(); it != sources_.end(); ++it)
                if (!keep_sources.count(it->first) && (source_victim == sources_.end() || it->second.used < source_victim->second.used))
                    source_victim = it;
            if (source_victim != sources_.end() && (victim == cache_.end() || source_victim->second.used <= victim->second.used)) {
                bytes_ -= source_victim->second.bytes;
                sources_.erase(source_victim);
                continue;
            }
            if (victim == cache_.end()) break;
            bytes_ -= victim->second.bytes;
            cache_.erase(victim);
        }
    }

    Future get(uint32_t id) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = cache_.find(id);
        if (it == cache_.end()) it = schedule(id);
        it->second.used = ++clock_;
        return it->second.future;
    }

private:
    struct Source : ViewPixels {};
    using SourceFuture = std::shared_future<std::shared_ptr<const Source>>;
    struct SourceEntry { SourceFuture future; uint64_t bytes = 0, used = 0; };
    struct Entry { Future future; uint64_t bytes = 0, used = 0; };

    std::shared_ptr<const Source> source(uint32_t id) {
        const size_t image = sourceImage(id);
        const uint64_t bytes = sourceBytes(id);
        auto promise = std::make_shared<std::promise<std::shared_ptr<const Source>>>();
        SourceFuture future;
        bool decode = false;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = sources_.find(image);
            if (it == sources_.end()) {
                it = sources_.emplace(image, SourceEntry{promise->get_future().share(), bytes, ++clock_}).first;
                bytes_ += bytes; decode = true;
            } else it->second.used = ++clock_;
            future = it->second.future;
        }
        if (decode) {
            try { promise->set_value(load_source(id)); }
            catch (...) { promise->set_exception(std::current_exception()); }
        }
        return future.get();
    }

    std::unordered_map<uint32_t, Entry>::iterator schedule(uint32_t id) {
        auto promise = std::make_shared<std::promise<std::shared_ptr<const ViewPixels>>>();
        Entry entry{promise->get_future().share(), bytes(id), clock_};
        bytes_ += entry.bytes;
        tasks_.push_back([this, id, promise] {
            try { promise->set_value(load(id)); }
            catch (...) { promise->set_exception(std::current_exception()); }
        });
        wake_.notify_one();
        return cache_.emplace(id, std::move(entry)).first;
    }

    void run() {
        for (;;) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                wake_.wait(lock, [&] { return quit_ || !tasks_.empty(); });
                if (quit_) return;
                task = std::move(tasks_.front());
                tasks_.pop_front();
            }
            task();
        }
    }

    std::shared_ptr<const Source> load_source(uint32_t id) {
        const Prepared& p = prepared_.at(id);
        nn::Image image = nn::load_image(ds_.image_filenames[p.image], config_.image_gamut, config_.image_is_linear, exposure_);
        if (image.empty()) throw std::runtime_error("cannot decode dense image: " + ds_.image_filenames[p.image]);
        const int turns = ds_.exif_quarter_turns.empty() ? 0 : ds_.exif_quarter_turns[p.image];
        if (turns) turn_pixels(sfm::ExifTransform{turns, false}, 3, image.data, image.width, image.height);
        const auto c = camera(ds_, p.image);
        if (image.width != c.width || image.height != c.height) throw std::runtime_error("dense image size no longer agrees with the camera");
        std::vector<uint8_t> keep((size_t)image.width * image.height, 1);
        auto intersect = [&](std::vector<uint8_t> values, int w, int h, bool invert) {
            if (turns) turn_pixels(sfm::ExifTransform{turns, false}, 1, values, w, h);
            uint8_t pass[256];
            for (int v = 0; v < 256; ++v) pass[v] = (uint8_t)((v / 255.0 >= config_.mask_threshold) != invert);
            std::vector<int> columns((size_t)image.width);
            for (int x = 0; x < image.width; ++x) columns[x] = std::min(w - 1, (int)((x + 0.5) * w / image.width));
            for (int y = 0; y < image.height; ++y) {
                const uint8_t* row = values.data() + (size_t)std::min(h - 1, (int)((y + 0.5) * h / image.height)) * w;
                uint8_t* out = keep.data() + (size_t)y * image.width;
                for (int x = 0; x < image.width; ++x) out[x] &= pass[row[columns[x]]];
            }
        };
        if (config_.use_masks && config_.alpha_masks) {
            int w, h; auto values = alpha(ds_.image_filenames[p.image], w, h);
            if (!values.empty()) intersect(std::move(values), w, h, false);
        }
        if (config_.use_masks && config_.training_masks && !ds_.mask_filenames.empty() && !ds_.mask_filenames[p.image].empty()) {
            int w, h; auto values = mask(ds_.mask_filenames[p.image], w, h); intersect(std::move(values), w, h, config_.invert_masks);
        }
        if (config_.use_masks && config_.feature_masks && !config_.feature_mask_dir.empty()) {
            const auto name = dsparse::relative_under(ds_.image_filenames[p.image], (root_ / config_.image_dir).string());
            const auto path = dsparse::find_aux_file((root_ / config_.feature_mask_dir).string(), name, "");
            if (!path.empty()) { int w, h; auto values = mask(path, w, h); intersect(std::move(values), w, h, false); }
        }
        if (config_.use_masks && config_.mask_boundary) edt::apply_mask_boundary_offset_in_place(keep.data(), image.height, image.width, (float)config_.mask_boundary);
        auto source = std::make_shared<Source>();
        source->width = image.width; source->height = image.height;
        if (config_.matching_space == "source")
            source->rgb = resize_area(image.data.data(),image.width,image.height,3,image.width,image.height);
        else {
            const auto plan = warps_.get(c);
            source->rgb = resize_area(image.data.data(), image.width, image.height, 3, plan->sampleWidth(), plan->sampleHeight());
        }
        std::vector<uint8_t> horizontal(keep.size());
        for (int y = 0; y < image.height; ++y) for (int x = 0; x < image.width; ++x) {
            const size_t i = (size_t)y * image.width + x;
            horizontal[i] = keep[i] & keep[(size_t)y * image.width + std::max(0, x - 1)] &
                keep[(size_t)y * image.width + std::min(image.width - 1, x + 1)];
        }
        source->keep.resize(keep.size());
        for (int y = 0; y < image.height; ++y) for (int x = 0; x < image.width; ++x) {
            const size_t i = (size_t)y * image.width + x;
            source->keep[i] = horizontal[i] & horizontal[(size_t)std::max(0, y - 1) * image.width + x] &
                horizontal[(size_t)std::min(image.height - 1, y + 1) * image.width + x];
        }
        if (config_.matching_space == "source") dump_source_pixels(p.image,ds_.image_filenames[p.image],*source);
        return source;
    }

    std::shared_ptr<const ViewPixels> load(uint32_t id) {
        const Prepared& p = prepared_.at(id);
        if (config_.matching_space == "source") return load_source(id);
        const auto decoded = source(id);
        const auto plan = warps_.get(camera(ds_, p.image));
        const auto& warp = *plan;
        auto pixels = std::make_shared<ViewPixels>();
        pixels->width = warp.faceWidth(p.face); pixels->height = warp.faceHeight(p.face);
        warp.sampleFace(p.face, decoded->rgb.data(), pixels->rgb);
        pixels->keep.assign((size_t)pixels->width * pixels->height, 0);
        const auto& indices = warp.faceSourceIndices(p.face);
        for (size_t i = 0; i < pixels->keep.size(); ++i) {
            if (indices[i] >= 0) pixels->keep[i] = decoded->keep[(size_t)indices[i]];
        }
        return pixels;
    }

    const ParsedDataset& ds_;
    const std::vector<Prepared>& prepared_;
    fs::path root_;
    DenseConfig config_;
    colorspace::Exposure exposure_;
    WarpCache& warps_;
    std::mutex mutex_;
    std::condition_variable wake_;
    std::deque<std::function<void()>> tasks_;
    std::unordered_map<uint32_t, Entry> cache_;
    std::unordered_map<size_t, SourceEntry> sources_;
    uint64_t bytes_ = 0, clock_ = 0;
    bool quit_ = false;
    std::vector<std::thread> threads_;
};

// The VKPM model.bin that app/gui/SfmProgress.h reads, so the dataset screen
// shows the cloud growing. Cameras are the dataset's, points the reconstruction preview.
class LiveSnapshot {
public:
    LiveSnapshot(const std::string& dir, const ParsedDataset& ds) : dir_(dir) {
        if (dir_.empty()) return;
        std::error_code error;
        fs::create_directories(dir_, error);
        fs::remove(dir_ / "model.bin", error);
        const uint32_t n = (uint32_t)ds.num_cameras;
        put_u32(header_, (ds.gauge_oriented ? 1u : 0u) | (ds.gauge_metric ? 2u : 0u));
        put_u32(header_, n); put_u32(header_, n);
        for (uint32_t i = 0; i < n; ++i) {
            put_u32(cameras_, i);
            for (int k = 0; k < 12; ++k) put(cameras_, &ds.c2w[(size_t)i * 12 + k], 4);
            put_u32(cameras_, (uint32_t)ds.widths[i]); put_u32(cameras_, (uint32_t)ds.heights[i]);
            put_u32(cameras_, (uint32_t)ds.camera_models[i]);
            put_u32(cameras_, (uint32_t)ds.camera_distortions[i]);
            put(cameras_, &ds.intrins[(size_t)i * 4], 4 * sizeof(float));
            put(cameras_, &ds.dist_coeffs[(size_t)i * kCameraDistortionParams], kCameraDistortionParams * sizeof(float));
        }
    }

    void write(Reconstruction& reconstruction, bool force) {
        if (dir_.empty()) return;
        const auto now = std::chrono::steady_clock::now();
        if (!force && written_ && now - last_ < std::chrono::milliseconds(1500)) return;
        written_ = true; last_ = now;
        const auto checkpoint = reconstruction.checkpoint();
        std::string b;
        b.reserve(48 + header_.size() + cameras_.size() + checkpoint.file.size() + checkpoint.error.size());
        put(b, "VKPM", 4); put_u32(b, 7);
        b += header_;
        if (!checkpoint.error.empty()) b[8] |= 8;
        if (checkpoint.provisional) b[8] |= 16;
        if (checkpoint.filtered) b[8] |= 32;
        put(b, &checkpoint.points, 8);
        b += cameras_;
        put_u32(b, (uint32_t)checkpoint.file.size()); b += checkpoint.file;
        put_u32(b, (uint32_t)checkpoint.error.size()); b += checkpoint.error;
        try {
            const fs::path part = dir_ / "model.bin.part";
            write_text(part, b);
            replace_file(part, dir_ / "model.bin");
        } catch (const std::exception&) {}  // a reader holding the old snapshot open; the next write retries
    }

private:
    static void put(std::string& b, const void* p, size_t n) { b.append((const char*)p, n); }
    static void put_u32(std::string& b, uint32_t v) { put(b, &v, 4); }
    fs::path dir_;
    std::string header_, cameras_;
    std::chrono::steady_clock::time_point last_{};
    bool written_ = false;
};

// Twice a second, this process's memory for the GUI's bar (dense/MemoryReport.h).
class MemoryReporter {
public:
    MemoryReporter(const std::string& dir, uint64_t planned) : dir_(dir) {
        if (dir_.empty()) return;
        std::error_code error;
        fs::create_directories(dir_, error);
        fs::remove(memory_report_path(dir_), error);
        report_.host_planned = spirula::processRamBytes() + planned;
        thread_ = std::thread([this] { run(); });
    }
    ~MemoryReporter() { stop(); }

    void phase(const char* name) {
        std::lock_guard<std::mutex> lock(mutex_);
        report_.phase = name;
    }

    // `ours` counts the driver's view of this process; `others` is the device less its budget for us.
    void device(uint64_t ours, uint64_t others, uint64_t capacity, uint64_t planned) {
        std::lock_guard<std::mutex> lock(mutex_);
        report_.device_known = true;
        report_.device_ours = ours; report_.device_others = others;
        report_.device_capacity = capacity; report_.device_planned = std::max(planned, ours);
    }

    void stop() {
        if (!thread_.joinable()) return;
        { std::lock_guard<std::mutex> lock(mutex_); quit_ = true; }
        wake_.notify_all();
        thread_.join();
    }

private:
    void run() {
        std::unique_lock<std::mutex> lock(mutex_);
        for (;;) {
            report_.host_process = spirula::processRamBytes();
            report_.host_available = spirula::availableRamBytes();
            report_.host_total = spirula::physicalRamBytes();
            report_.host_planned = std::max(report_.host_planned, report_.host_process);
            ++report_.sequence;
            write_memory_report(dir_, report_);
            if (wake_.wait_for(lock, std::chrono::milliseconds(500), [&] { return quit_; })) return;
        }
    }

    fs::path dir_;
    MemoryReport report_;
    std::mutex mutex_;
    std::condition_variable wake_;
    bool quit_ = false;
    std::thread thread_;
};

// --perf-dir: once a second, the phase and its progress beside the machine's
// counters (app/SystemRecorder.h), for tools/perf/perf_report.py.
class DensePerfLog {
public:
    explicit DensePerfLog(const fs::path& dir) {
        if (dir.empty()) return;
        std::error_code error;
        fs::create_directories(dir, error);
        file_ = std::fopen((dir / "dense_perf.csv").string().c_str(), "w");
        if (!file_) return;
        std::fputs("unix_ms,phase,done,total,interval_s,done_per_s,rss_bytes\n", file_);
        system_.start(dir);
        thread_ = std::thread([this] { run(); });
    }
    ~DensePerfLog() {
        if (thread_.joinable()) {
            { std::lock_guard<std::mutex> lock(mutex_); quit_ = true; }
            wake_.notify_all();
            thread_.join();
        }
        system_.stop();
        if (file_) std::fclose(file_);
    }
    // A phase change writes its row at once, so a phase shorter than a second still shows.
    void update(const char* phase, uint64_t done, uint64_t total) {
        if (!file_) return;
        bool switched = false;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (phase_ != phase) {
                closing_ = phase_ + "," + std::to_string(done_) + "," + std::to_string(total_);
                last_done_ = 0; switched = switched_ = true;
            }
            phase_ = phase; done_ = done; total_ = total;
        }
        if (switched) wake_.notify_all();
    }

private:
    void run() {
        std::unique_lock<std::mutex> lock(mutex_);
        auto last = std::chrono::steady_clock::now();
        for (;;) {
            wake_.wait_for(lock, std::chrono::seconds(1), [&] { return quit_ || switched_; });
            if (quit_) return;
            switched_ = false;
            const auto now = std::chrono::steady_clock::now();
            const double interval = std::chrono::duration<double>(now - last).count();
            last = now;
            const long long ms = (long long)std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            // The phase just left, at its last count: it can end between two samples.
            if (!closing_.empty()) {
                std::fprintf(file_, "%lld,%s,0.0000,0.000,%llu\n", ms - 1, closing_.c_str(),
                             (unsigned long long)spirula::processRamBytes());
                closing_.clear();
            }
            const double rate = done_ >= last_done_ ? (double)(done_ - last_done_) / std::max(interval, 1e-3) : 0.0;
            last_done_ = done_;
            std::fprintf(file_, "%lld,%s,%llu,%llu,%.4f,%.3f,%llu\n", ms, phase_.c_str(),
                         (unsigned long long)done_, (unsigned long long)total_, interval, rate,
                         (unsigned long long)spirula::processRamBytes());
            std::fflush(file_);
        }
    }

    std::FILE* file_ = nullptr;
    spirula::SystemRecorder system_;
    std::thread thread_;
    std::mutex mutex_;
    std::condition_variable wake_;
    bool quit_ = false, switched_ = false;
    std::string phase_ = "prepare", closing_;
    uint64_t done_ = 0, total_ = 0, last_done_ = 0;
};

void report_device(MemoryReporter& memory, const roma::Session& session) {
    const auto& context = nn::vk::Context::get();
    const auto budget = context.memoryBudget();
    const uint64_t allocated = nn::vk::Allocator::get().totalBytes();
    const uint64_t ours = std::max<uint64_t>(budget.usage_bytes, allocated);
    const uint64_t capacity = context.info().vram_bytes ? context.info().vram_bytes : budget.budget_bytes;
    const uint64_t others = budget.budget_bytes && capacity > budget.budget_bytes ? capacity - budget.budget_bytes : 0;
    const auto cache = session.featureCacheStatistics();
    memory.device(ours, others, capacity, ours + (cache.limit_bytes > cache.bytes ? cache.limit_bytes - cache.bytes : 0));
}

void write_prediction(std::ostream& out, const roma::Prediction& p) {
    const int32_t dims[2] = {p.width, p.height};
    out.write(reinterpret_cast<const char*>(dims), sizeof dims);
    for (const auto* data : {&p.warp, &p.overlap, &p.precision})
        out.write(reinterpret_cast<const char*>(data->data()), (std::streamsize)(data->size() * sizeof(float)));
}

bool read_prediction(std::istream& in, roma::Prediction& p, int w, int h, bool present) {
    int32_t dims[2];
    in.read(reinterpret_cast<char*>(dims), sizeof dims);
    if (!in || dims[0] != (present ? w : 0) || dims[1] != (present ? h : 0)) return false;
    p.width = dims[0]; p.height = dims[1];
    const size_t n = present ? (size_t)w * h : 0;
    p.warp.resize(n * 2); p.overlap.resize(n); p.precision.resize(n * 3);
    for (auto* data : {&p.warp, &p.overlap, &p.precision}) {
        in.read(reinterpret_cast<char*>(data->data()), (std::streamsize)(data->size() * sizeof(float)));
        if (!in) return false;
        for (float value : *data) if (!std::isfinite(value)) return false;
    }
    return true;
}

bool cached_prediction(const fs::path& path, const DenseConfig& config, roma::PairPrediction& p) {
    if (!config.resume || config.rebuild || !fs::is_regular_file(path)) return false;
    std::ifstream checksum(path.string() + ".sha256"); std::string expected;
    checksum >> expected;
    if (expected.size() != 64 || spirula::sha256_file(path.string()) != expected) return false;
    std::ifstream in(path, std::ios::binary);
    char magic[8]; in.read(magic, sizeof magic);
    if (!in || std::string(magic, sizeof magic) != "SSROMA03") return false;
    const int w = config.match.high_width ? config.match.high_width : config.match.low_width;
    const int h = config.match.high_height ? config.match.high_height : config.match.low_height;
    return read_prediction(in, p.forward, w, h, true) && read_prediction(in, p.backward, w, h, config.match.bidirectional) && in.peek() == EOF;
}

void cache_prediction(const fs::path& path, const roma::PairPrediction& p) {
    const fs::path part = path.string() + ".part";
    std::ofstream out(part, std::ios::binary | std::ios::trunc);
    out.write("SSROMA03", 8); write_prediction(out, p.forward); write_prediction(out, p.backward);
    out.flush();
    if (!out) throw std::runtime_error("cannot cache RoMa pair prediction");
    out.close();
    const auto hash = spirula::sha256_file(part.string());
    const fs::path hash_part = path.string() + ".sha256.part";
    write_text(hash_part, hash + "\n");
    replace_file(part, path); replace_file(hash_part, path.string() + ".sha256");
}

}  // namespace

DenseResult run_dense(const std::string& dataset_path, const spirula::dense::DenseConfig& config,
                       const spirula::dense::DenseProgress& caller_progress, const std::atomic<bool>* cancel,
                       const std::string& progress_dir, const std::string& perf_dir) {
    config.validate_run(); check_cancel(cancel);
    DensePerfLog perf(perf_dir);
    const auto start = std::chrono::steady_clock::now();
    const fs::path root = resolved_dataset(dataset_path);
    fs::create_directories(root / "dense" / "cache");
    FileLock run_lock(root / "dense" / "run.lock");
    cleanup_pending_generations(root.string());
    cleanup_abandoned_runs(root / "dense" / "cache");
    Fingerprints fingerprints(root / "dense" / "cache" / "fingerprints.txt");
    std::string checkpoint_hash;
    const auto checkpoint = checkpoint_path(config.checkpoint, fingerprints, checkpoint_hash);
    if (checkpoint_hash.empty()) throw std::runtime_error("cannot read RoMa checkpoint");
    const unsigned workers = worker_count(config);
    DenseConfig resources = config;
    resources.image_cache_bytes = config.resolved_image_cache_bytes();
    MemoryReporter memory(progress_dir, planned_host_bytes(resources.image_cache_bytes, config.matching_space == "rectified"));
    memory.phase("prepare");
    const DenseProgress progress = [&](const char* stage, uint64_t done, uint64_t total) {
        memory.phase(stage);
        perf.update(stage, done, total);
        if (caller_progress) caller_progress(stage, done, total);
    };
    DatasetParserConfig parser;
    parser.image_dir = config.image_dir; parser.mask_dir = config.mask_dir; parser.recon_dir = config.recon_dir;
    parser.metashape_xml = config.metashape_xml; parser.metashape_psx = config.metashape_psx;
    parser.metashape_component = config.metashape_component; parser.exif_orientation = config.exif_orientation;
    parser.center_mode = "camera-mean"; parser.probe_image_size = probe_image_size;
    const ParsedDataset ds = parse_dataset(root.string(), parser, "");
    if (ds.num_cameras < config.geometry.min_source_images) throw std::runtime_error("dense dataset has fewer registered source images than the required support");
    std::vector<Prepared> prepared;
    std::vector<View> views;
    std::vector<PairImage> images;
    std::vector<std::vector<uint32_t>> faces((size_t)ds.num_cameras);
    JsonWriter identity; identity.object();
    nn::configure_device(config.device);
    const auto resolved_precision = roma::Session::resolvePrecision(config.match.precision);
    identity.field("revision", 3).field("checkpoint_sha256", checkpoint_hash);
    if (config.matching_space == "source") identity.field("matching_space","source");
    if (resolved_precision == roma::InferencePrecision::Mixed) identity.field("precision", "mixed-v1");
    identity.field("color", config.image_gamut).field("exposure", config.image_exposure);
    identity.field_raw("linear", json_field::emit(config.image_is_linear));
    identity.field("split", config.split_views).field("face_size", config.max_face_size).field("exif", config.exif_orientation);
    identity.field("low_width", config.match.low_width).field("low_height", config.match.low_height);
    identity.field("high_width", config.match.high_width).field("high_height", config.match.high_height);
    identity.field("bidirectional", config.match.bidirectional).field("saturation", config.match.overlap_saturation);
    JsonWriter inference;
    inference.object().field("revision",4).field("checkpoint",checkpoint_hash).field("precision",(int)resolved_precision);
    inference.field("color",config.image_gamut).field("exposure",config.image_exposure).field_raw("linear",json_field::emit(config.image_is_linear));
    inference.field("low_width",config.match.low_width).field("low_height",config.match.low_height);
    inference.field("high_width",config.match.high_width).field("high_height",config.match.high_height);
    inference.field("bidirectional",config.match.bidirectional).field("saturation",config.match.overlap_saturation).end();
    const auto inference_identity = digest(inference.str());
    identity.key("images").array();
    struct FacePlan { std::vector<std::array<double, 6>> faces; std::vector<double> axes; };
    std::vector<std::string> image_hashes((size_t)ds.num_cameras);
    std::vector<FacePlan> plans(image_hashes.size());
    WarpCache warps(resources);
    std::atomic<uint64_t> hashed{0};
    std::mutex print;
    parallel_for(image_hashes.size(), workers, [&](size_t i) {
        check_cancel(cancel);
        if (config.matching_space == "source") {
            const auto c = camera(ds, i);
            plans[i].faces.push_back({(double)c.width, (double)c.height, c.fx, c.fy, c.cx, c.cy});
        } else {
            const auto warp = warps.get(camera(ds, i));
            for (int f = 0; f < warp->faces(); ++f)
                plans[i].faces.push_back({(double)warp->faceWidth(f), (double)warp->faceHeight(f), warp->faceFocal(f),
                                          warp->faceFocalY(f), warp->faceCx(f), warp->faceCy(f)});
            if (const double* axes = warp->faceAxes()) plans[i].axes.assign(axes, axes + (size_t)warp->faces() * 9);
        }
        image_hashes[i] = fingerprints.get(ds.image_filenames[i]);
        if (image_hashes[i].empty()) throw std::runtime_error("cannot fingerprint dense image");
        const uint64_t done = ++hashed;
        if (progress && (done % 16 == 0 || done == image_hashes.size())) {
            std::lock_guard<std::mutex> lock(print);
            progress("prepare", done, image_hashes.size());
        }
    });
    fingerprints.save();
    for (size_t i = 0; i < (size_t)ds.num_cameras; ++i) {
        check_cancel(cancel);
        const FacePlan& plan = plans[i];
        const float* pose = ds.c2w.data() + i * 12;
        sfm::Mat3 rotation;
        for (int r = 0; r < 3; ++r) for (int c = 0; c < 3; ++c) rotation[r * 3 + c] = pose[c * 4 + r] * (r == 0 ? 1 : -1);
        const sfm::Vec3 center{pose[3], pose[7], pose[11]};
        const auto name = dsparse::relative_under(ds.image_filenames[i], (root / config.image_dir).string());
        PairImage pair;
        pair.source_image = i; pair.name = name.empty() ? fs::path(ds.image_filenames[i]).filename().generic_string() : name;
        pair.center = center; pair.forward = {rotation[6], rotation[7], rotation[8]};
        pair.forward = pair.forward.normalized();
        pair.up = {rotation[3], rotation[4], rotation[5]};
        pair.half_fov_radians = half_fov(camera(ds, i));
        images.push_back(pair);
        const auto& image_hash = image_hashes[i];
        identity.object().field("name", pair.name).field("sha256", image_hash);
        identity.key("faces").array();
        for (int f = 0; f < (int)plan.faces.size(); ++f) {
            Prepared p; p.image = i; p.face = f; p.view.source_image = i; p.view.center = center;
            p.view.camera.model = (int)CameraModelType::PINHOLE;
            const auto& intrinsics = plan.faces[(size_t)f];
            p.view.camera.width = (int)intrinsics[0]; p.view.camera.height = (int)intrinsics[1];
            p.view.camera.fx = intrinsics[2]; p.view.camera.fy = intrinsics[3];
            p.view.camera.cx = intrinsics[4]; p.view.camera.cy = intrinsics[5];
            if (config.matching_space == "source") {
                const auto c = camera(ds, i);
                p.view.source_camera = true;
                p.view.camera.model = c.model; p.view.camera.tier = c.distortion;
                std::copy_n(c.dist, 8, p.view.camera.dist);
                p.view.camera.source_model = c.source_model;
                std::copy_n(c.source_params, 16, p.view.camera.source_params);
                p.view.grid_scale[0] = (double)(config.match.high_width ? config.match.high_width : config.match.low_width) / c.width;
                p.view.grid_scale[1] = (double)(config.match.high_height ? config.match.high_height : config.match.low_height) / c.height;
            }
            p.view.world_to_camera = rotation;
            if (!plan.axes.empty()) {
                const double* axes = plan.axes.data();
                sfm::Mat3 face;
                for (int r = 0; r < 3; ++r) {
                    sfm::Vec3 axis{axes[f * 9 + r * 3], axes[f * 9 + r * 3 + 1], axes[f * 9 + r * 3 + 2]};
                    axis = axis.normalized();
                    face[r * 3] = axis.x; face[r * 3 + 1] = axis.y; face[r * 3 + 2] = axis.z;
                }
                for (int r = 0; r < 3; ++r) for (int c = 0; c < 3; ++c) {
                    p.view.world_to_camera[r * 3 + c] = 0;
                    for (int k = 0; k < 3; ++k) p.view.world_to_camera[r * 3 + c] += face[r * 3 + k] * rotation[k * 3 + c];
                }
            }
            p.view.validate();
            JsonWriter id; id.object().field("image", image_hash).field("face", f);
            id.key("camera").array();
            for (double value : {p.view.camera.fx, p.view.camera.fy, p.view.camera.cx, p.view.camera.cy,
                                  (double)p.view.camera.width, (double)p.view.camera.height, center.x, center.y, center.z}) id.raw(json_number_exact(value));
            for (double value : p.view.world_to_camera) id.raw(json_number_exact(value));
            const auto source = camera(ds, i);
            for (float value : source.dist) id.raw(json_number_exact(value));
            id.raw(std::to_string(source.model)).raw(std::to_string(source.distortion)).raw(std::to_string(source.source_model));
            for (float value : source.source_params) id.raw(json_number_exact(value));
            id.end().end(); p.identity = digest(id.str());
            JsonWriter pixels;
            pixels.object().field("image",image_hash).field("space",config.matching_space).field("exif",config.exif_orientation);
            const int applied_turns = ds.exif_quarter_turns.empty() ? 0 : ds.exif_quarter_turns[i];
            if (applied_turns) pixels.field("exif_turns",applied_turns);
            if (config.matching_space == "rectified") {
                pixels.field("face",f).field("split",config.split_views).field("face_size",config.max_face_size);
                pixels.key("calibration").array();
                for (double value : intrinsics) pixels.raw(json_number_exact(value));
                for (double value : {(double)source.fx,(double)source.fy,(double)source.cx,(double)source.cy,
                                     (double)source.width,(double)source.height}) pixels.raw(json_number_exact(value));
                for (float value : source.dist) pixels.raw(json_number_exact(value));
                pixels.raw(std::to_string(source.model)).raw(std::to_string(source.distortion)).raw(std::to_string(source.source_model));
                for (float value : source.source_params) pixels.raw(json_number_exact(value));
                for (double value : plan.axes) pixels.raw(json_number_exact(value));
                pixels.end();
            }
            pixels.end(); p.cache_identity = digest(pixels.str());
            identity.value(p.identity);
            faces[i].push_back((uint32_t)prepared.size()); views.push_back(p.view); prepared.push_back(std::move(p));
        }
        identity.end().end();
    }
    identity.end().end();
    const auto match_identity = digest(identity.str());
    const auto model_dir = find_colmap_model(root.string(), config.recon_dir);
    std::vector<std::string> input_paths = ds.image_filenames;
    input_paths.push_back(checkpoint);
    for (const auto& path : ds.mask_filenames) if (!path.empty()) input_paths.push_back(path);
    for (const auto& path : {config.mask_dir, config.feature_mask_dir, config.pair_list, config.metashape_xml, config.metashape_psx})
        if (!path.empty()) input_paths.push_back((root / path).string());
    if (!model_dir.empty()) input_paths.push_back(model_dir);
    for (const auto& entry : fs::directory_iterator(root)) {
        const auto extension = entry.path().extension().string();
        const auto name = entry.path().filename().string();
        if (entry.is_regular_file() && ((extension == ".json" && name.rfind("transforms", 0) == 0) ||
                                      extension == ".xml" || extension == ".psx"))
            input_paths.push_back(entry.path().string());
    }
    const auto initial_input_stamp = input_stamp(input_paths);
    const auto initial_input_content = input_content_stamp(input_paths,[&] { check_cancel(cancel); },
                                                           [&](const auto& path) { return fingerprints.get(path); });
    bool face_tracks = false, tracks_aligned = false;
    if (!model_dir.empty()) {
        const auto tracks = read_sparse_stats(model_dir);
        tracks_aligned = tracks.error.size() == (size_t)ds.points.num() && !tracks.empty();
        face_tracks = config.sparse_face_pairs && tracks_aligned;
        std::map<std::string, uint32_t> by_name;
        for (uint32_t i = 0; i < images.size(); ++i) by_name[images[i].name] = i;
        for (uint64_t p = 0; p + 1 < tracks.track_beg.size(); ++p)
            for (int64_t o = tracks.track_beg[p]; o < tracks.track_beg[p + 1]; ++o) {
                const auto it = by_name.find(tracks.image_names[tracks.track_image[o]]);
                if (it != by_name.end()) {
                    const uint32_t image = it->second;
                    images[image].visible_points.push_back(p);
                }
            }
        for (auto& image : images) { std::sort(image.visible_points.begin(), image.visible_points.end()); image.visible_points.erase(std::unique(image.visible_points.begin(), image.visible_points.end()), image.visible_points.end()); }
    }
    std::vector<sfm::Vec3> point_positions;
    if (tracks_aligned)
        for (int64_t p = 0; p < ds.points.num(); ++p)
            point_positions.push_back({ds.points.xyz[p * 3], ds.points.xyz[p * 3 + 1], ds.points.xyz[p * 3 + 2]});
    PairOptions pair_options = config.pairs;
    if (config.matching_space == "source" && pair_options.mode == PairMode::Automatic) {
        pair_options.directed = true;
        pair_options.references = select_references(images, config.reference_fraction, config.sampling_seed, [&] { check_cancel(cancel); });
    }
    if (pair_options.mode == PairMode::Explicit && !config.pair_list.empty()) {
        std::map<std::string, uint32_t> by_name;
        for (uint32_t i = 0; i < images.size(); ++i) by_name[images[i].name] = i;
        std::ifstream file(root / config.pair_list);
        if (!file) throw std::runtime_error("cannot read dense pair list");
        std::string line;
        while (std::getline(file, line)) {
            if (line.empty() || line[0] == '#') continue;
            std::istringstream row(line); std::string a, b, extra;
            if (!(row >> std::quoted(a) >> std::quoted(b)) || row >> extra || !by_name.count(a) || !by_name.count(b))
                throw std::runtime_error("dense pair list must contain two registered image names per line");
            pair_options.explicit_pairs.emplace_back(by_name[a], by_name[b]);
        }
    }
    const fs::path dense = root / "dense", cache = dense / "cache" / match_identity;
    const fs::path predictions = dense / "cache" / "pairs-v4" / inference_identity;
    fs::create_directories(predictions);
    fs::create_directories(cache);
    const fs::path work = cache / ("run-" + std::to_string(start.time_since_epoch().count()));
    struct WorkCleanup {
        fs::path work, parent;
        ~WorkCleanup() { try { remove_owned_directory(work, parent, work.filename().string()); } catch (const std::exception&) {} }
    } cleanup{work, cache};
    Reconstruction reconstruction(work.string(), views, config, cancel, progress_dir);
    DenseResult result;
    struct PairIndex { uint32_t a = 0, b = 0; };
    struct PairJob { uint32_t a = 0, b = 0; fs::path file, legacy, reverse; };
    const auto unordered_jobs = work / "jobs.bin", ordered_jobs = work / "jobs-ordered.bin";
    std::ofstream job_output(unordered_jobs,std::ios::binary | std::ios::trunc);
    uint64_t rejected_face_pairs = 0;
    std::vector<bool> tracks_ready(images.size());
    auto prepare_face_tracks = [&](uint32_t image) {
        if (tracks_ready[image]) return;
        check_cancel(cancel);
        for (uint64_t p : images[image].visible_points) {
            const sfm::Vec3 point{ds.points.xyz[p * 3], ds.points.xyz[p * 3 + 1], ds.points.xyz[p * 3 + 2]};
            for (uint32_t face : faces[image]) {
                sfm::Vec2 pixel;
                if (project(views[face], point, pixel) && pixel.x >= 0 && pixel.y >= 0 &&
                    pixel.x < views[face].camera.width && pixel.y < views[face].camera.height)
                    prepared[face].visible_points.push_back(p);
            }
        }
        tracks_ready[image] = true;
    };
    PairStatistics pair_stats;
    if (pair_options.mode == PairMode::Automatic && config.matching_space != "source") {
        // Faces are paired as views of their own, so a wide lens adds only the face pairs that share points.
        std::vector<PairImage> view_images;
        for (uint32_t i = 0; i < images.size(); ++i) {
            if (tracks_aligned && faces[i].size() > 1) prepare_face_tracks(i);
            for (size_t f = 0; f < faces[i].size(); ++f) {
                const View& view = views[faces[i][f]];
                PairImage v;
                v.source_image = images[i].source_image; v.face = (int)f; v.name = images[i].name; v.center = view.center;
                v.forward = sfm::Vec3{view.world_to_camera[6], view.world_to_camera[7], view.world_to_camera[8]}.normalized();
                v.up = sfm::Vec3{view.world_to_camera[3], view.world_to_camera[4], view.world_to_camera[5]}.normalized();
                v.half_fov_radians = std::clamp(std::atan(0.5 * std::hypot(view.camera.width / view.camera.fx, view.camera.height / view.camera.fy)), 0.1, 3.14159);
                v.visible_points = faces[i].size() > 1 ? prepared[faces[i][f]].visible_points : images[i].visible_points;
                v.shared_points_only = faces[i].size() > 1 && face_tracks;
                view_images.push_back(std::move(v));
            }
        }
        pair_stats = select_pairs(view_images, pair_options, [&](ImagePair pair) {
            check_cancel(cancel);
            write_disk_record(job_output,PairIndex{pair.first,pair.second});
        }, [&] { check_cancel(cancel); }, point_positions);
    } else {
        pair_stats = select_pairs(images, pair_options, [&](ImagePair pair) {
            check_cancel(cancel);
            const bool guided = face_tracks && (faces[pair.first].size() > 1 || faces[pair.second].size() > 1) &&
                shares_points(images[pair.first].visible_points, images[pair.second].visible_points);
            if (guided) { prepare_face_tracks(pair.first); prepare_face_tracks(pair.second); }
            for (uint32_t a : faces[pair.first]) for (uint32_t b : faces[pair.second]) {
                const View& va = views[a], &vb = views[b];
                const sfm::Vec3 fa{va.world_to_camera[6], va.world_to_camera[7], va.world_to_camera[8]};
                const sfm::Vec3 fb{vb.world_to_camera[6], vb.world_to_camera[7], vb.world_to_camera[8]};
                const double fova = std::atan(std::hypot(va.camera.width / va.camera.fx, va.camera.height / va.camera.fy) * 0.5);
                const double fovb = std::atan(std::hypot(vb.camera.width / vb.camera.fx, vb.camera.height / vb.camera.fy) * 0.5);
                const auto baseline = vb.center - va.center;
                const bool facing = fa.dot(baseline) > 0 && fb.dot(baseline) < 0;
                if (config.matching_space != "source" && !facing && std::acos(std::clamp(fa.dot(fb), -1.0, 1.0)) > fova + fovb) continue;
                if (guided && !shares_points(prepared[a].visible_points, prepared[b].visible_points)) {
                    ++rejected_face_pairs; continue;
                }
                write_disk_record(job_output,PairIndex{a,b});
                if (config.matching_space == "source" && pair_options.mode != PairMode::Automatic)
                    write_disk_record(job_output,PairIndex{b,a});
            }
        }, [&] { check_cancel(cancel); }, point_positions);
    }
    flush_disk_output(job_output); job_output.close();
    auto order_jobs = [](const PairIndex& a,const PairIndex& b) { return std::tie(a.a,a.b) < std::tie(b.a,b.b); };
    external_sort<PairIndex>(unordered_jobs,ordered_jobs,resources.image_cache_bytes / 8,order_jobs,[&] { check_cancel(cancel); });
    fs::remove(unordered_jobs);
    auto jobs = std::make_unique<DiskTable<PairIndex>>(ordered_jobs,resources.image_cache_bytes / 32);
    const uint64_t job_count = jobs->size();
    if (!job_count) throw std::runtime_error("no dense pairs have a usable baseline and overlapping views");
    auto job_at = [&](uint64_t index) {
        const auto pair = jobs->get(index); const auto a = pair.a, b = pair.b;
        return PairJob{a,b,predictions / (digest(prepared[a].cache_identity + prepared[b].cache_identity) + ".roma"),
            config.matching_space == "rectified" ? cache / (digest(prepared[a].identity + prepared[b].identity) + ".roma") : fs::path{},
            predictions / (digest(prepared[b].cache_identity + prepared[a].cache_identity) + ".roma")};
    };
    std::vector<uint64_t> remaining(views.size());
    std::vector<uint64_t> neighbors(views.size());
    std::vector<int64_t> last_neighbor(views.size(),-1), last_reverse_neighbor(views.size(),-1);
    const auto unordered_unique = work / "unique-pairs.bin", ordered_unique = work / "unique-pairs-ordered.bin";
    {
        std::ofstream output(unordered_unique,std::ios::binary | std::ios::trunc);
        for (uint64_t i = 0; i < job_count; ++i) {
            check_cancel(cancel); const auto job = jobs->get(i);
            ++remaining[job.a];
            if (last_neighbor[job.a] != views[job.b].source_image) { ++neighbors[job.a]; last_neighbor[job.a] = views[job.b].source_image; }
            if (config.matching_space != "source") {
                ++remaining[job.b];
                if (last_reverse_neighbor[job.b] != views[job.a].source_image) {
                    ++neighbors[job.b]; last_reverse_neighbor[job.b] = views[job.a].source_image;
                }
            }
            write_disk_record(output,PairIndex{std::min(job.a,job.b),std::max(job.a,job.b)});
        }
        flush_disk_output(output);
    }
    if (std::none_of(neighbors.begin(),neighbors.end(),[&](auto count) { return count + 1 >= (uint64_t)config.geometry.min_source_images; }))
        throw std::runtime_error("planned dense pairs cannot supply the requested distinct-image support");
    const auto reference_count = (uint64_t)std::count_if(remaining.begin(),remaining.end(),[](auto count) { return count > 0; });
    external_sort<PairIndex>(unordered_unique,ordered_unique,resources.image_cache_bytes / 8,order_jobs,[&] { check_cancel(cancel); });
    uint64_t unique_pair_count = 0;
    {
        std::ifstream input(ordered_unique,std::ios::binary); PairIndex previous, value;
        while (read_disk_record(input,value)) {
            if (!unique_pair_count || value.a != previous.a || value.b != previous.b) ++unique_pair_count;
            previous = value;
        }
    }
    fs::remove(unordered_unique); fs::remove(ordered_unique);
    const double prepare_seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    if (progress) progress("match", 0, job_count);   // model loading is matching's, not preparation's

    // Decoding runs ahead on worker threads and reconstruction trails on its own,
    // so the GPU waits only for its own previous pair.
    nn::configure_device(config.device);
    roma::Session session;
    uint64_t decode_bytes = 0;
    for (size_t i = 0; i < (size_t)ds.num_cameras; ++i)
        decode_bytes = std::max(decode_bytes, (uint64_t)ds.widths[i] * ds.heights[i] * (3 * sizeof(float) + 6));
    const unsigned decode_workers = (unsigned)std::min<uint64_t>(workers, std::max<uint64_t>(1, resources.image_cache_bytes / std::max<uint64_t>(1, decode_bytes)));
    Pixels pixels(ds, prepared, root.string(), resources, warps, decode_workers);
    LiveSnapshot live(progress_dir, ds);
    struct Matched { PairJob job; Pixels::Future a, b; roma::PairPrediction prediction; bool store = false; };
    std::mutex mutex;
    std::condition_variable changed;
    std::deque<Matched> queue;
    bool closed = false, discard = false;
    std::exception_ptr failure;
    // Cached predictions only serve resume and reuse, so a nearly full disk stops collecting them.
    auto disk_has_room = [&](const roma::PairPrediction& p) {
        std::error_code error;
        const auto space = fs::space(predictions, error);
        const uint64_t bytes = ((uint64_t)p.forward.warp.size() + p.forward.overlap.size() + p.forward.precision.size() +
                                p.backward.warp.size() + p.backward.overlap.size() + p.backward.precision.size()) * sizeof(float);
        return !error && space.available > bytes + std::max<uint64_t>(4ull << 30, space.capacity / 20);
    };
    std::thread consumer([&] {
        for (;;) {
            Matched item;
            {
                std::unique_lock<std::mutex> lock(mutex);
                changed.wait(lock, [&] { return closed || discard || !queue.empty(); });
                if (discard || queue.empty()) return;
                item = std::move(queue.front());
                queue.pop_front();
            }
            changed.notify_all();
            try {
                if (item.store && disk_has_room(item.prediction)) cache_prediction(item.job.file,item.prediction);
                const auto pa = item.a.get(), pb = item.b.get();
                reconstruction.add_pair(item.job.a,item.job.b,*pa,*pb,item.prediction);
                if (--remaining[item.job.a] == 0) reconstruction.complete_reference(item.job.a,true);
                if (config.matching_space != "source" && --remaining[item.job.b] == 0) reconstruction.complete_reference(item.job.b,true);
                ++result.pairs;
                if (progress) progress("match",result.pairs,job_count);
                live.write(reconstruction, false);
            } catch (...) {
                std::lock_guard<std::mutex> lock(mutex);
                failure = std::current_exception();
                discard = true;
                queue.clear();
                changed.notify_all();
                return;
            }
        }
    });
    auto stop = [&](bool drop) {
        {
            std::lock_guard<std::mutex> lock(mutex);
            closed = true; discard = discard || drop;
        }
        changed.notify_all();
        if (consumer.joinable()) consumer.join();
    };
    struct StopGuard { std::function<void(bool)> stop; ~StopGuard() { if (stop) stop(true); } } guard{stop};
    double model_load_seconds = 0, inference_seconds = 0;
    for (uint64_t k = 0; k < job_count; ++k) {
        check_cancel(cancel);
        std::vector<uint32_t> window;
        std::set<size_t> window_sources;
        uint64_t window_bytes = 0;
        const size_t lookahead = (size_t)decode_workers * 2;
        for (uint64_t j = k; j < job_count && j - k < lookahead; ++j) {
            bool full = false;
            const auto pair = jobs->get(j);
            for (uint32_t id : {pair.a,pair.b}) {
                if (std::find(window.begin(), window.end(), id) != window.end()) continue;
                const bool new_source = !window_sources.count(pixels.sourceImage(id));
                const uint64_t bytes = pixels.bytes(id) + (new_source ? pixels.sourceBytes(id) : 0);
                if (j > k && window_bytes + bytes > resources.image_cache_bytes) { full = true; break; }
                window.push_back(id); window_bytes += bytes;
                window_sources.insert(pixels.sourceImage(id));
            }
            if (full) break;
        }
        pixels.prefetch(window);
        Matched item;
        const auto job = job_at(k); item.job = job; item.a = pixels.get(job.a); item.b = pixels.get(job.b);
        bool cached = cached_prediction(job.file,config,item.prediction);
        if (!cached && config.matching_space == "rectified" && cached_prediction(job.legacy,config,item.prediction))
            cached = item.store = true;
        if (!cached && config.match.bidirectional && cached_prediction(job.reverse,config,item.prediction)) {
            std::swap(item.prediction.forward,item.prediction.backward); cached = item.store = true;
        }
        if (cached) ++result.cached_pairs;
        else {
            const auto pa = item.a.get(), pb = item.b.get();
            if (!session.loaded()) {
                const auto load_start = std::chrono::steady_clock::now();
                session.load(checkpoint, resolved_precision, [&](uint64_t done, uint64_t total) {
                    check_cancel(cancel);
                    if (progress) progress("load", done, total);
                });
                model_load_seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - load_start).count();
            }
            auto options = config.match;
            const auto previous = options.progress;
            options.progress = [&](const char* stage) { check_cancel(cancel); if (previous) previous(stage); };
            const auto match_start = std::chrono::steady_clock::now();
            item.prediction = session.matchCached(job.a,pa->rgb.data(),pa->width,pa->height,
                                                  job.b,pb->rgb.data(),pb->width,pb->height,options);
            inference_seconds += std::chrono::duration<double>(std::chrono::steady_clock::now() - match_start).count();
            item.store = true;
            report_device(memory, session);
        }
        dump_pair_prediction(job.a,job.b,item.prediction);
        std::unique_lock<std::mutex> lock(mutex);
        const uint64_t prediction_bytes = (uint64_t)item.prediction.forward.width * item.prediction.forward.height *
                                          6 * sizeof(float) * (config.match.bidirectional ? 2 : 1);
        const uint64_t held_bytes = prediction_bytes + pixels.bytes(job.a) + pixels.bytes(job.b);
        const uint64_t queue_capacity = std::max<uint64_t>(1,resources.image_cache_bytes / std::max<uint64_t>(1,held_bytes));
        changed.wait(lock, [&] { return queue.size() < queue_capacity || discard; });
        if (discard) break;
        queue.push_back(std::move(item));
        lock.unlock();
        changed.notify_all();
    }
    guard.stop = nullptr;
    stop(false);
    if (failure) std::rethrow_exception(failure);
    check_cancel(cancel);
    const auto feature_cache = session.featureCacheStatistics();
    const auto peak_vulkan_buffer_bytes = nn::vk::Allocator::get().peakBytes();
    const auto peak_scratch_bytes = session.peakScratchBytes();
    session.unload();
    if (feature_cache.limit_bytes || peak_vulkan_buffer_bytes) report_device(memory, session);
    pixels.release();
    live.write(reconstruction, true);
    const double match_seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() - prepare_seconds;
    const fs::path temporary_ply = work / "roma.ply", temporary_manifest = work / "manifest.json";
    result.statistics = reconstruction.finish(temporary_ply.string(), ds, progress);
    check_cancel(cancel);
    result.cloud = (dense / "roma.ply").string(); result.manifest = (dense / "manifest.json").string();
    result.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    const double finish_seconds = result.seconds - prepare_seconds - match_seconds;
    const auto verification_start = std::chrono::steady_clock::now();
    JsonWriter manifest; manifest.object();
    manifest.field("version", 2).field("complete", true).field("generation", work.filename().string());
    manifest.field("reconstruction_revision",reconstruction_revision);
    manifest.field("coordinate_frame", "source file frame; seed_pointcloud compatible");
    manifest.field("cloud", "roma.ply").field("cloud_sha256", spirula::sha256_file(temporary_ply.string()));
    manifest.field("cloud_bytes", (long long)fs::file_size(temporary_ply));
    if (initial_input_stamp != input_stamp(input_paths)) throw std::runtime_error("dense inputs changed during processing; rerun with stable inputs");
    if (initial_input_content != input_content_stamp(input_paths,[&] { check_cancel(cancel); }))
        throw std::runtime_error("dense input contents changed during processing; rerun with stable inputs");
    const double verification_seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - verification_start).count();
    result.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    manifest.field("input_content_stamp",initial_input_content);
    manifest.field("input_stamp", initial_input_stamp).key("input_paths").array();
    for (const auto& path : input_paths) manifest.value(fs::absolute(path).lexically_normal().string());
    manifest.end();
    manifest.field("match_identity", match_identity).field("checkpoint_sha256", checkpoint_hash);
    manifest.field_raw("settings", config_json(config));
    manifest.field("source_images", (long long)images.size()).field("views", (long long)views.size());
    manifest.field("source_pairs", (long long)pair_stats.emitted).field("matched_pairs", (long long)result.pairs).field("cached_pairs", (long long)result.cached_pairs);
    manifest.field("reference_images",(long long)reference_count).field("directed_jobs",(long long)job_count);
    manifest.field("selected_reference_views",(long long)pair_stats.references);
    manifest.field("unique_image_pairs",(long long)unique_pair_count).field("inference_calls",(long long)(result.pairs-result.cached_pairs));
    manifest.field("seconds", result.seconds).field("prepare_seconds", prepare_seconds).field("match_seconds", match_seconds);
    manifest.field("finish_seconds", finish_seconds).field("verification_seconds", verification_seconds);
    manifest.field("model_load_seconds", model_load_seconds).field("inference_seconds", inference_seconds);
    manifest.field("peak_vulkan_buffer_bytes", (long long)peak_vulkan_buffer_bytes);
    manifest.field("peak_matcher_scratch_bytes", (long long)peak_scratch_bytes);
    manifest.field("inference_precision", resolved_precision == roma::InferencePrecision::Mixed ? "mixed" : "float32");
    manifest.field("sparse_face_pairing", face_tracks).field("rejected_face_pairs", (long long)rejected_face_pairs);
    manifest.field("image_cache_bytes", (long long)resources.image_cache_bytes).field("decode_workers", (long long)decode_workers);
    manifest.field("live_checkpoint_error", reconstruction.checkpoint().error);
    manifest.field("refinement_workers", (long long)result.statistics.refinement_workers);
    manifest.field("reference_work_seconds", result.statistics.reference_work_seconds);
    manifest.field("fusion_workers", (long long)result.statistics.fusion_workers);
    manifest.field("fusion_partitions", (long long)result.statistics.fusion_partitions);
    manifest.field("fusion_boundary_candidates", (long long)result.statistics.fusion_boundary_candidates);
    manifest.key("feature_cache").object();
    manifest.field("hits", (long long)feature_cache.hits).field("misses", (long long)feature_cache.misses);
    manifest.field("evictions", (long long)feature_cache.evictions).field("bytes", (long long)feature_cache.bytes);
    manifest.field("peak_bytes", (long long)feature_cache.peak_bytes).field("limit_bytes", (long long)feature_cache.limit_bytes).end();
    manifest.field("voxel_size", result.statistics.resolved_voxel_size);
    manifest.key("statistics").object();
#define SS_DENSE_STAT(name) manifest.field(#name, (long long)result.statistics.name);
    SS_DENSE_STAT(tested) SS_DENSE_STAT(masked) SS_DENSE_STAT(low_overlap) SS_DENSE_STAT(cycle) SS_DENSE_STAT(geometry)
    SS_DENSE_STAT(triangulated) SS_DENSE_STAT(insufficient_support) SS_DENSE_STAT(refined) SS_DENSE_STAT(fused) SS_DENSE_STAT(exported)
#undef SS_DENSE_STAT
    manifest.end();
    manifest.key("filtered_reference_quality").object();
    manifest.field("pixel_frame", config.matching_space == "source" ? "original image" : "working view");
    manifest.field("reprojection_limit",config.resolved_geometry().max_reprojection_error);
    manifest.field("maximum_point_residual",result.statistics.max_reference_reprojection_error);
    manifest.field("mean_point_maximum_residual",result.statistics.refined ? result.statistics.sum_reference_max_reprojection_error / result.statistics.refined : 0);
    manifest.field("minimum_support",(long long)result.statistics.min_reference_support);
    manifest.field("maximum_support",(long long)result.statistics.max_reference_support);
    manifest.field("mean_support",result.statistics.refined ? (double)result.statistics.sum_reference_support / result.statistics.refined : 0);
    manifest.key("residual_histogram_by_limit_fraction").array();
    for (const auto count : result.statistics.reference_reprojection_histogram) manifest.value((long long)count);
    manifest.end().end().end();
    write_text(temporary_manifest, manifest.str());
    const auto published = publish_generation(root.string(), work.filename().string(), temporary_ply, temporary_manifest,
                                              [&](const char* phase) {
        check_cancel(cancel);
        if (std::string(phase) == "before_current" && config.keep_cache) {
            jobs.reset(); remove_owned_directory(work,cache,work.filename().string());
        }
    });
    result.cloud = published.cloud.string(); result.manifest = published.manifest.string();
    live.write(reconstruction, true);
    if (!config.keep_cache) {
        const auto allowed = (dense / "cache").lexically_normal();
        if (cache.lexically_normal().parent_path() != allowed) throw std::runtime_error("invalid dense cache cleanup path");
        std::error_code error;
        for (uint64_t i = 0; i < job_count; ++i) {
            const auto job = job_at(i);
            fs::remove(job.file, error); error.clear();
            fs::remove(job.file.string() + ".sha256", error); error.clear();
        }
        jobs.reset();
        try { remove_owned_directory(cache, allowed, match_identity); } catch (const std::exception&) {}
    }
    result.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    if (progress) progress("complete", result.statistics.exported, result.statistics.exported);
    memory.stop();
    return result;
}

}  // namespace app
