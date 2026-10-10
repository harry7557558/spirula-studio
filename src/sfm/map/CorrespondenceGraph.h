// Correspondence graph: for every (image, feature), the set of features in
// other images it is matched to (COLMAP's scene/correspondence_graph).
// Built from the verified two-view matches; the incremental mapper
// queries it to find 2D-3D correspondences (register-next) and to grow tracks
// (triangulation).
//
// Stored per image as CSR -- one offsets array over the image's features plus
// one flat correspondence array -- rather than a vector per feature. An entry
// is one word, (image << feature_bits) | feature, whenever both fit in 32 bits
// (16k images of 256k features do), else two: 3.6 GB -> 1.8 GB on a
// 4000-image video's 225M matches.
#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <iterator>
#include <list>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <unordered_map>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "sfm/core/Matches.h"
#include "sfm/core/Spill.h"

namespace sfm {

struct Correspondence {
    uint32_t image_id;
    uint32_t feature_idx;
};

// Contiguous run of correspondences for one (image, feature), decoded as read.
class CorrespondenceView {
public:
    class iterator {
    public:
        iterator(const uint32_t* p, uint32_t stride, uint32_t shift)
            : p_(p), stride_(stride), shift_(shift) {}
        Correspondence operator*() const {
            if (stride_ == 2) return {p_[0], p_[1]};
            return {p_[0] >> shift_, p_[0] & ((1u << shift_) - 1u)};
        }
        iterator& operator++() {
            p_ += stride_;
            return *this;
        }
        bool operator!=(const iterator& o) const { return p_ != o.p_; }
        bool operator==(const iterator& o) const { return p_ == o.p_; }

    private:
        const uint32_t* p_;
        uint32_t stride_, shift_;
    };

    std::shared_ptr<const void> lease;
    CorrespondenceView() = default;
    CorrespondenceView(const Correspondence* first, const Correspondence* last,
                       std::shared_ptr<const void> owner)
        : lease(std::move(owner)), first_(reinterpret_cast<const uint32_t*>(first)),
          last_(reinterpret_cast<const uint32_t*>(last)), stride_(2), shift_(0) {}
    CorrespondenceView(const uint32_t* first, const uint32_t* last, uint32_t stride,
                       uint32_t shift, std::shared_ptr<const void> owner = {})
        : lease(std::move(owner)), first_(first), last_(last), stride_(stride), shift_(shift) {}
    iterator begin() const { return {first_, stride_, shift_}; }
    iterator end() const { return {last_, stride_, shift_}; }
    size_t size() const { return first_ == last_ ? 0 : (size_t)(last_ - first_) / stride_; }
    bool empty() const { return first_ == last_; }

private:
    const uint32_t* first_ = nullptr;
    const uint32_t* last_ = nullptr;
    uint32_t stride_ = 1, shift_ = 0;
};

class CorrespondenceGraph {
public:
    struct Options {
        // Zero keeps the RAM backend; a disk cache budget bounds heap CSR, not process RSS.
        size_t cache_bytes = 0;
        std::filesystem::path scratch_dir;
    };

    struct Stats {
        bool disk_backed = false;
        size_t resident_bytes = 0, peak_resident_bytes = 0;
        size_t spilled_bytes = 0, spilled_images = 0;
        size_t loads = 0, cache_hits = 0, evictions = 0, mapped_fallbacks = 0;
    };

    // `num_features[i]` = feature count of image i (matching db.images order).
    // `wide` takes two words per entry even where one would do (the tests).
    void build(const MatchesDatabase& db, const std::vector<uint32_t>& num_features,
               bool wide = false) {
        build(db, num_features, Options{}, wide);
    }

    void build(const MatchesDatabase& db, const std::vector<uint32_t>& num_features,
               const Options& opt, bool wide = false) {
        size_t estimate = 0;
        for (uint32_t count : num_features)
            estimate = checkedAdd(estimate, sizeof(DiskHeader) + (size_t(count) + 1) * 4);
        for (const TwoViewMatches& p : db.pairs) {
            if (p.image1 >= num_features.size() || p.image2 >= num_features.size())
                throw std::runtime_error("correspondence graph: image index out of range");
            estimate = checkedAdd(estimate, p.matches.size() * 2 * sizeof(Correspondence));
        }
        if (opt.cache_bytes && estimate > opt.cache_bytes) {
            auto disk = buildDisk(db, num_features, opt);
            starts_.clear();
            data_.clear();
            rows_.clear();
            spill_.reset();
            disk_ = std::move(disk);
            return;
        }
        disk_.reset();
        const size_t n = num_features.size();
        uint32_t max_features = 1;
        for (uint32_t c : num_features) max_features = std::max(max_features, c);
        auto bits = [](uint64_t v) {
            uint32_t b = 1;
            while (b < 32 && (v >> b) != 0) b++;
            return b;
        };
        shift_ = bits(max_features - 1);
        stride_ = !wide && shift_ + bits(n > 0 ? n - 1 : 0) <= 32 ? 1 : 2;
        spill_.reset();
        starts_.assign(n, {});
        data_.assign(n, {});
        for (size_t i = 0; i < n; i++) starts_[i].assign((size_t)num_features[i] + 1, 0);

        // Count, prefix-sum, scatter. The +1 offset in the counting pass makes
        // the prefix sum land directly in the final offsets array.
        for (const TwoViewMatches& p : db.pairs)
            for (const FeatureMatch& m : p.matches) {
                if (m.idx1 >= num_features[p.image1] ||
                    m.idx2 >= num_features[p.image2])
                    continue;
                starts_[p.image1][m.idx1 + 1]++;
                starts_[p.image2][m.idx2 + 1]++;
            }
        std::vector<std::vector<uint32_t>> fill(n);
        for (size_t i = 0; i < n; i++) {
            std::vector<uint32_t>& s = starts_[i];
            for (size_t f = 1; f < s.size(); f++) s[f] += s[f - 1];
            data_[i].resize(s.empty() ? 0 : (size_t)s.back() * stride_);
            fill[i].assign(s.begin(), s.end() - (s.empty() ? 0 : 1));
        }
        auto put = [&](uint32_t img, uint32_t feat, uint32_t other, uint32_t other_feat) {
            uint32_t* e = &data_[img][(size_t)fill[img][feat]++ * stride_];
            if (stride_ == 2) {
                e[0] = other;
                e[1] = other_feat;
            } else {
                e[0] = (other << shift_) | other_feat;
            }
        };
        for (const TwoViewMatches& p : db.pairs)
            for (const FeatureMatch& m : p.matches) {
                if (m.idx1 >= num_features[p.image1] ||
                    m.idx2 >= num_features[p.image2])
                    continue;
                put(p.image1, m.idx1, p.image2, m.idx2);
                put(p.image2, m.idx2, p.image1, m.idx1);
            }
        rows_.resize(n);
        for (size_t i = 0; i < n; i++)
            rows_[i] = {starts_[i].data(), data_[i].data(), num_features[i]};
    }

    // Both arrays into a SpillFile at `path`, read from the mapping from here
    // on: the graph is never written after build(). False keeps them in memory.
    bool spill(const std::string& path) {
        if (disk_ || spill_) return true;
        std::vector<std::pair<const void*, size_t>> chunks;
        for (size_t i = 0; i < rows_.size(); i++) {
            chunks.push_back({starts_[i].data(), starts_[i].size() * 4});
            if (!data_[i].empty()) chunks.push_back({data_[i].data(), data_[i].size() * 4});
        }
        std::shared_ptr<const SpillFile> f = SpillFile::write(path, chunks);
        if (!f) return false;
        const uint32_t* at = reinterpret_cast<const uint32_t*>(f->data());
        for (size_t i = 0; i < rows_.size(); i++) {
            rows_[i].starts = at;
            at += starts_[i].size();
            rows_[i].data = at;
            at += data_[i].size();
        }
        std::vector<std::vector<uint32_t>>().swap(starts_);
        std::vector<std::vector<uint32_t>>().swap(data_);
        spill_ = std::move(f);
        releaseFreedHeap();
        return true;
    }

    CorrespondenceView at(uint32_t image, uint32_t feature) const {
        if (disk_) return diskAt(image, feature);
        if (image >= numImages() || feature >= numFeatures(image))
            throw std::out_of_range("correspondence graph: feature index out of range");
        const Row& r = rows_[image];
        if (!r.data || r.starts[feature] == r.starts[feature + 1]) return {};
        return {r.data + (size_t)r.starts[feature] * stride_,
                r.data + (size_t)r.starts[feature + 1] * stride_, stride_, shift_, spill_};
    }

    size_t numImages() const { return disk_ ? disk_->images.size() : rows_.size(); }
    uint32_t numFeatures(uint32_t image) const { return disk_ ? disk_->images.at(image).features : rows_.at(image).features; }

    Stats stats() const {
        if (disk_) {
            std::lock_guard<std::mutex> lock(disk_->mutex);
            return disk_->stats;
        }
        Stats out;
        for (const auto& s : starts_) out.resident_bytes += s.capacity() * sizeof(uint32_t);
        for (const auto& d : data_) out.resident_bytes += d.capacity() * sizeof(uint32_t);
        out.peak_resident_bytes = out.resident_bytes;
        out.disk_backed = bool(spill_);
        return out;
    }

    std::filesystem::path cacheDirectory() const { return disk_ ? disk_->directory : std::filesystem::path{}; }

private:
    struct DiskHeader {
        uint32_t magic = 0x53474352, features = 0;
        uint64_t correspondences = 0;
    };

    struct Mapping {
        const uint8_t* data = nullptr;
        size_t bytes = 0;
#if defined(_WIN32)
        HANDLE file = INVALID_HANDLE_VALUE, mapping = nullptr;
#endif
        ~Mapping() {
#if defined(_WIN32)
            if (data) UnmapViewOfFile(data);
            if (mapping) CloseHandle(mapping);
            if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
#else
            if (data) munmap(const_cast<uint8_t*>(data), bytes);
#endif
        }

        void open(const std::filesystem::path& path, size_t expected) {
#if defined(_WIN32)
            file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE,
                               nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            LARGE_INTEGER length{};
            if (file == INVALID_HANDLE_VALUE || !GetFileSizeEx(file, &length) ||
                length.QuadPart < 0 || uint64_t(length.QuadPart) != expected)
                throw std::runtime_error("correspondence graph: invalid cache file");
            mapping = CreateFileMappingW(file, nullptr, PAGE_READONLY, 0, 0, nullptr);
            if (mapping) data = static_cast<const uint8_t*>(MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0));
            if (!data) throw std::runtime_error("correspondence graph: cannot map cache file");
#else
            const int fd = ::open(path.c_str(), O_RDONLY);
            struct stat info{};
            if (fd < 0) throw std::runtime_error("correspondence graph: cannot open cache file");
            if (fstat(fd, &info) || info.st_size < 0 || uint64_t(info.st_size) != expected) {
                ::close(fd);
                throw std::runtime_error("correspondence graph: invalid cache file");
            }
            void* view = mmap(nullptr, expected, PROT_READ, MAP_SHARED, fd, 0);
            ::close(fd);
            if (view == MAP_FAILED) throw std::runtime_error("correspondence graph: cannot map cache file");
            data = static_cast<const uint8_t*>(view);
#endif
            bytes = expected;
        }
    };

    struct Rows {
        std::vector<uint8_t> heap;
        Mapping mapped;
        const uint8_t* base() const { return heap.empty() ? mapped.data : heap.data(); }
        const uint32_t* starts() const { return reinterpret_cast<const uint32_t*>(base() + sizeof(DiskHeader)); }
        const Correspondence* values(uint32_t features) const {
            return reinterpret_cast<const Correspondence*>(base() + sizeof(DiskHeader) + (size_t(features) + 1) * 4);
        }
    };

    struct ImageInfo {
        uint32_t features = 0;
        size_t bytes = 0;
        std::weak_ptr<Rows> mapped;
    };

    struct CacheEntry {
        std::shared_ptr<Rows> rows;
        std::list<uint32_t>::iterator order;
    };

    struct DiskBackend {
        std::filesystem::path directory;
        size_t budget = 0;
        std::vector<ImageInfo> images;
        std::unordered_map<uint32_t, CacheEntry> cache;
        std::list<uint32_t> lru;
        std::mutex mutex;
        Stats stats;
        ~DiskBackend() {
            cache.clear();
            std::error_code ec;
            if (!directory.empty()) std::filesystem::remove_all(directory, ec);
        }
        std::filesystem::path file(uint32_t image) const { return directory / (std::to_string(image) + ".csr"); }
    };

    struct Lease {
        std::shared_ptr<DiskBackend> backend;
        std::shared_ptr<Rows> rows;
    };

    static size_t checkedAdd(size_t a, size_t b) {
        if (b > std::numeric_limits<size_t>::max() - a)
            throw std::runtime_error("correspondence graph: size overflow");
        return a + b;
    }

    static std::filesystem::path createDirectory(std::filesystem::path parent) {
        if (parent.empty()) parent = std::filesystem::temp_directory_path();
        std::filesystem::create_directories(parent);
        static std::atomic<uint64_t> serial{0};
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        for (int i = 0; i < 64; ++i) {
            const auto dir = parent / ("spirula-graph-" + std::to_string(stamp) + "-" + std::to_string(serial++));
            std::error_code ec;
            if (std::filesystem::create_directory(dir, ec)) return dir;
            if (ec) throw std::runtime_error("correspondence graph: cannot create cache directory");
        }
        throw std::runtime_error("correspondence graph: cannot reserve cache directory");
    }

    static std::shared_ptr<DiskBackend> buildDisk(const MatchesDatabase& db,
        const std::vector<uint32_t>& features, const Options& opt) {
        auto out = std::make_shared<DiskBackend>();
        out->directory = createDirectory(opt.scratch_dir);
        out->budget = opt.cache_bytes;
        out->images.resize(features.size());
        out->stats.disk_backed = true;
        std::vector<std::vector<size_t>> adjacency(features.size());
        for (size_t i = 0; i < db.pairs.size(); ++i) {
            const auto& p = db.pairs[i];
            adjacency[p.image1].push_back(i);
            if (p.image2 != p.image1) adjacency[p.image2].push_back(i);
        }
        for (uint32_t image = 0; image < features.size(); ++image) {
            std::vector<uint32_t> starts(size_t(features[image]) + 1, 0);
            auto visit = [&](auto emit) {
                for (size_t i : adjacency[image]) {
                    const auto& p = db.pairs[i];
                    for (const auto& m : p.matches) {
                        if (m.idx1 >= features[p.image1] || m.idx2 >= features[p.image2]) continue;
                        if (p.image1 == image) emit(m.idx1, Correspondence{p.image2, m.idx2});
                        if (p.image2 == image) emit(m.idx2, Correspondence{p.image1, m.idx1});
                    }
                }
            };
            visit([&](uint32_t f, Correspondence) {
                if (starts[size_t(f) + 1] == UINT32_MAX)
                    throw std::runtime_error("correspondence graph: image row overflow");
                ++starts[size_t(f) + 1];
            });
            for (size_t f = 1; f < starts.size(); ++f) {
                const uint64_t count = uint64_t(starts[f]) + starts[f - 1];
                if (count > UINT32_MAX) throw std::runtime_error("correspondence graph: image row overflow");
                starts[f] = uint32_t(count);
            }
            std::vector<Correspondence> data(starts.back());
            std::vector<uint32_t> fill(starts.begin(), starts.end() - 1);
            visit([&](uint32_t f, Correspondence c) { data[fill[f]++] = c; });
            DiskHeader header;
            header.features = features[image];
            header.correspondences = data.size();
            std::ofstream file(out->file(image), std::ios::binary | std::ios::trunc);
            file.write(reinterpret_cast<const char*>(&header), sizeof(header));
            file.write(reinterpret_cast<const char*>(starts.data()), std::streamsize(starts.size() * 4));
            if (!data.empty()) file.write(reinterpret_cast<const char*>(data.data()), std::streamsize(data.size() * sizeof(Correspondence)));
            file.close();
            if (!file) throw std::runtime_error("correspondence graph: cannot write cache file");
            const size_t bytes = sizeof(header) + starts.size() * 4 + data.size() * sizeof(Correspondence);
            out->images[image].features = features[image];
            out->images[image].bytes = bytes;
            out->stats.spilled_bytes = checkedAdd(out->stats.spilled_bytes, bytes);
            ++out->stats.spilled_images;
        }
        return out;
    }

    static void validateRows(const Rows& rows, const ImageInfo& info) {
        DiskHeader header;
        std::memcpy(&header, rows.base(), sizeof(header));
        const size_t offset = sizeof(header) + (size_t(info.features) + 1) * 4;
        if (header.magic != 0x53474352 || header.features != info.features ||
            header.correspondences != (info.bytes - offset) / sizeof(Correspondence) ||
            rows.starts()[0] != 0 || rows.starts()[info.features] != header.correspondences)
            throw std::runtime_error("correspondence graph: corrupt cache file");
    }

    CorrespondenceView diskAt(uint32_t image, uint32_t feature) const {
        const auto backend = disk_;
        if (image >= backend->images.size() || feature >= backend->images[image].features)
            throw std::out_of_range("correspondence graph: feature index out of range");
        std::lock_guard<std::mutex> lock(backend->mutex);
        auto cached = backend->cache.find(image);
        std::shared_ptr<Rows> rows;
        ImageInfo& info = backend->images[image];
        if (cached != backend->cache.end()) {
            rows = cached->second.rows;
            backend->lru.splice(backend->lru.begin(), backend->lru, cached->second.order);
            ++backend->stats.cache_hits;
        } else {
            for (auto it = backend->lru.end();
                 info.bytes <= backend->budget && backend->stats.resident_bytes > backend->budget - info.bytes &&
                 it != backend->lru.begin();) {
                --it;
                auto entry = backend->cache.find(*it);
                if (entry->second.rows.use_count() != 1) continue;
                backend->stats.resident_bytes -= backend->images[entry->first].bytes;
                backend->cache.erase(entry);
                it = backend->lru.erase(it);
                ++backend->stats.evictions;
            }
            if (info.bytes <= backend->budget && backend->stats.resident_bytes <= backend->budget - info.bytes) {
                rows = std::make_shared<Rows>();
                rows->heap.resize(info.bytes);
                std::ifstream file(backend->file(image), std::ios::binary);
                file.read(reinterpret_cast<char*>(rows->heap.data()), std::streamsize(info.bytes));
                if (!file || file.peek() != std::ifstream::traits_type::eof())
                    throw std::runtime_error("correspondence graph: cannot read cache file");
                validateRows(*rows, info);
                backend->lru.push_front(image);
                backend->cache.emplace(image, CacheEntry{rows, backend->lru.begin()});
                backend->stats.resident_bytes += info.bytes;
                backend->stats.peak_resident_bytes =
                    std::max(backend->stats.peak_resident_bytes, backend->stats.resident_bytes);
                ++backend->stats.loads;
            } else {
                // Pinned views cannot be evicted. File-backed leases keep nested queries
                // live without growing the heap cache; mapped pages are outside its budget.
                rows = info.mapped.lock();
                if (!rows) {
                    rows = std::make_shared<Rows>();
                    rows->mapped.open(backend->file(image), info.bytes);
                    validateRows(*rows, info);
                    info.mapped = rows;
                    ++backend->stats.mapped_fallbacks;
                }
            }
        }
        const uint32_t* starts = rows->starts();
        if (starts[feature] > starts[size_t(feature) + 1] ||
            size_t(starts[size_t(feature) + 1]) >
                (info.bytes - sizeof(DiskHeader) - (size_t(info.features) + 1) * 4) / sizeof(Correspondence))
            throw std::runtime_error("correspondence graph: corrupt cache offsets");
        const Correspondence* base = rows->values(info.features);
        auto lease = std::make_shared<Lease>(Lease{backend, rows});
        return {base + starts[feature], base + starts[size_t(feature) + 1], std::move(lease)};
    }

    // Per image: offsets over the image's features (features+1 entries) and the
    // correspondences they index into, `stride_` words each. `rows_` points at
    // the vectors below, or into spill_ once they are gone.
    struct Row {
        const uint32_t* starts = nullptr;
        const uint32_t* data = nullptr;
        uint32_t features = 0;
    };
    std::vector<Row> rows_;
    std::vector<std::vector<uint32_t>> starts_;
    std::vector<std::vector<uint32_t>> data_;
    std::shared_ptr<const SpillFile> spill_;
    std::shared_ptr<DiskBackend> disk_;
    uint32_t stride_ = 1, shift_ = 1;
};

}  // namespace sfm
