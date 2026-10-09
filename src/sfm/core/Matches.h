// Feature matches and their flat-file interchange format.
//
// A match is a pair of feature indices into two images' FeatureSets. A
// MatchesDatabase collects the two-view match lists for a whole dataset (the
// input to phase-3 verification and the phase-4 correspondence graph). Like
// features.bin this is our own format (D4), self-describing and trivial to
// parse; it is not COLMAP's SQLite database.
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <initializer_list>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "sfm/core/Camera.h"
#include "sfm/core/Spill.h"

namespace sfm {

// One putative correspondence: feature idx1 in image1 <-> idx2 in image2.
// Nothing past a matcher's own cap reads a descriptor distance, so none is
// kept: a 4000-image video holds 225M of these through mapping.
struct FeatureMatch {
    uint32_t idx1 = 0, idx2 = 0;
};

// Keep the `cap` matches of smallest `dist` (parallel to `m`), ordered as
// std::partial_sort leaves them.
inline void keepClosest(std::vector<FeatureMatch>& m, const std::vector<float>& dist,
                        size_t cap) {
    if (cap == 0 || m.size() <= cap) return;
    struct Scored {
        float d;
        FeatureMatch m;
    };
    std::vector<Scored> s(m.size());
    for (size_t k = 0; k < m.size(); k++) s[k] = {dist[k], m[k]};
    std::partial_sort(s.begin(), s.begin() + (std::ptrdiff_t)cap, s.end(),
                      [](const Scored& a, const Scored& b) { return a.d < b.d; });
    m.resize(cap);
    for (size_t k = 0; k < cap; k++) m[k] = s[k].m;
}

static_assert(sizeof(FeatureMatch) == 8, "matches.bin and spillMatches write it as is");

// A pair's matches: owned, or a read-only view into the file spillMatches
// mapped. Reading code sees one const array either way; writing to a view
// throws, so a writer that runs after the spill fails loudly, not by copying.
class MatchList {
public:
    MatchList() = default;
    MatchList(std::vector<FeatureMatch> v) : v_(std::move(v)) {}
    MatchList(std::initializer_list<FeatureMatch> l) : v_(l) {}

    size_t size() const { return view_ ? n_ : v_.size(); }
    bool empty() const { return size() == 0; }
    const FeatureMatch* data() const { return view_ ? view_ : v_.data(); }
    const FeatureMatch* begin() const { return data(); }
    const FeatureMatch* end() const { return data() + size(); }
    const FeatureMatch& operator[](size_t k) const { return data()[k]; }
    bool isView() const { return view_ != nullptr; }

    // The only ways to write, so that reading through a non-const reference
    // still reads the view.
    std::vector<FeatureMatch>& mut() {
        if (view_) throw std::logic_error("MatchList: writing to spilled matches");
        return v_;
    }
    void push_back(const FeatureMatch& m) { mut().push_back(m); }
    void pop_back() { mut().pop_back(); }
    void resize(size_t n) { mut().resize(n); }
    void reserve(size_t n) { mut().reserve(n); }
    std::vector<FeatureMatch> toVector() const { return {begin(), end()}; }

    void view(const FeatureMatch* p, size_t n) {
        std::vector<FeatureMatch>().swap(v_);
        view_ = n ? p : nullptr;
        n_ = n;
    }

private:
    std::vector<FeatureMatch> v_;
    const FeatureMatch* view_ = nullptr;
    size_t n_ = 0;
};

// All matches between one ordered image pair. When geometric verification has
// run, `matches` holds only the inliers and `config` is the two-view config
// (0 = not verified / raw; otherwise the sfm/geometry/TwoView.h TwoViewConfig).
struct TwoViewMatches {
    uint32_t image1 = 0, image2 = 0;      // indices into MatchesDatabase::images
    int32_t config = 0;                   // 0 = unverified
    MatchList matches;
};

// One image's identity in the match database.
struct ImageEntry {
    std::string name;         // feature-file stem (== image name)
    uint32_t num_features = 0;
};

struct MatchesDatabase {
    std::vector<ImageEntry> images;
    std::vector<TwoViewMatches> pairs;
    // The camera setup verification actually used (D47): which images share
    // intrinsics, what those intrinsics were, and whether the focal was a prior
    // or a guess. Recorded so the mapper does not have to reconstruct it -- its
    // only other option is to re-search the focal on the *verified inliers*,
    // which are biased towards whatever focal produced them. Empty in a file
    // written before this, or by `--no-verify`.
    std::vector<Camera> cameras;         // one per distinct camera id
    std::vector<uint32_t> camera_ids;    // per image, parallel to `images`
    std::vector<uint8_t> focal_prior;    // parallel to `cameras`; 1 = not a guess
    // 1 where THIS stage measured the focal rather than being told it. Kept
    // apart from `focal_prior` because the mapper treats the two differently:
    // it re-refines a measured focal and leaves a given one alone (D45).
    std::vector<uint8_t> focal_measured;
    bool hasCameras() const {
        return !cameras.empty() && camera_ids.size() == images.size();
    }
    // What the pairs' views point into, once spillMatches has run.
    std::shared_ptr<const SpillFile> spill;
};

// ---------------------------------------------------------------------------
// matches.bin -- "VKMT", u32 version=4; the layout is writeMatches below, and
// an older file reads back as what it carried: v2 stops after the pairs and has
// no cameras, v3 has cameras but no per-camera `focal_measured` byte.
// ---------------------------------------------------------------------------

inline void writeMatches(const std::string& path, const MatchesDatabase& db) {
    std::ofstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("cannot write " + path);
    uint32_t version = 4, nimg = (uint32_t)db.images.size();
    f.write("VKMT", 4);
    f.write((const char*)&version, 4);
    f.write((const char*)&nimg, 4);
    for (const ImageEntry& im : db.images) {
        uint32_t len = (uint32_t)im.name.size();
        f.write((const char*)&len, 4);
        f.write(im.name.data(), len);
        f.write((const char*)&im.num_features, 4);
    }
    uint32_t npairs = (uint32_t)db.pairs.size();
    f.write((const char*)&npairs, 4);
    for (const TwoViewMatches& p : db.pairs) {
        uint32_t nm = (uint32_t)p.matches.size();
        f.write((const char*)&p.image1, 4);
        f.write((const char*)&p.image2, 4);
        f.write((const char*)&p.config, 4);
        f.write((const char*)&nm, 4);
        f.write((const char*)p.matches.data(), (std::streamsize)nm * 8);
    }
    uint32_t ncam = db.hasCameras() ? (uint32_t)db.cameras.size() : 0;
    f.write((const char*)&ncam, 4);
    for (uint32_t c = 0; c < ncam; c++) {
        const Camera& cam = db.cameras[c];
        int32_t model_id = camColmapId(cam.model);
        uint32_t np = (uint32_t)camColmapParams(cam.model);
        uint8_t prior = c < db.focal_prior.size() ? db.focal_prior[c] : 0;
        double params[16] = {0};
        packColmap(cam, params);
        f.write((const char*)&cam.id, 4);
        f.write((const char*)&cam.width, 4);
        f.write((const char*)&cam.height, 4);
        f.write((const char*)&model_id, 4);
        f.write((const char*)&prior, 1);
        f.write((const char*)&cam.pixel_scale, 8);
        f.write((const char*)&np, 4);
        f.write((const char*)params, np * 8);
    }
    if (ncam) {
        uint32_t nid = (uint32_t)db.camera_ids.size();
        f.write((const char*)&nid, 4);
        f.write((const char*)db.camera_ids.data(), (std::streamsize)nid * 4);
        for (uint32_t c = 0; c < ncam; c++) {
            uint8_t m = c < db.focal_measured.size() ? db.focal_measured[c] : 0;
            f.write((const char*)&m, 1);
        }
    }
}

inline void readMatchesCameras(std::istream& f, uint32_t version, MatchesDatabase& db,
                               const std::string& path) {
    if (version >= 3) {
        uint32_t ncam = 0;
        f.read((char*)&ncam, 4);
        if (f.gcount() != 4) ncam = 0;
        for (uint32_t c = 0; c < ncam; c++) {
            Camera cam;
            int32_t model_id = 0;
            uint32_t np = 0;
            uint8_t prior = 0;
            double params[16] = {0};
            f.read((char*)&cam.id, 4);
            f.read((char*)&cam.width, 4);
            f.read((char*)&cam.height, 4);
            f.read((char*)&model_id, 4);
            f.read((char*)&prior, 1);
            f.read((char*)&cam.pixel_scale, 8);
            f.read((char*)&np, 4);
            if (np > 16) throw std::runtime_error("bad camera in " + path);
            f.read((char*)params, (std::streamsize)np * 8);
            cam.model = camFromColmapId(model_id);
            unpackColmap(cam, params);
            db.cameras.push_back(cam);
            db.focal_prior.push_back(prior);
        }
        if (ncam) {
            uint32_t nid = 0;
            f.read((char*)&nid, 4);
            if (nid == db.images.size()) {
                db.camera_ids.resize(nid);
                f.read((char*)db.camera_ids.data(), (std::streamsize)nid * 4);
            }
        }
        if (ncam && version >= 4) {
            std::vector<uint8_t> measured(ncam);
            f.read((char*)measured.data(), (std::streamsize)ncam);
            if (f.gcount() == (std::streamsize)ncam) db.focal_measured = std::move(measured);
        }
        if (!db.hasCameras()) {   // truncated section: no cameras, not bad ones
            db.cameras.clear();
            db.camera_ids.clear();
            db.focal_prior.clear();
            db.focal_measured.clear();
        }
    }
}

inline MatchesDatabase readMatches(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("cannot read " + path);
    char magic[4];
    f.read(magic, 4);
    if (std::memcmp(magic, "VKMT", 4) != 0) throw std::runtime_error("bad magic in " + path);
    uint32_t version, nimg;
    f.read((char*)&version, 4);
    f.read((char*)&nimg, 4);
    MatchesDatabase db;
    db.images.resize(nimg);
    for (uint32_t i = 0; i < nimg; i++) {
        uint32_t len;
        f.read((char*)&len, 4);
        db.images[i].name.resize(len);
        f.read(&db.images[i].name[0], len);
        f.read((char*)&db.images[i].num_features, 4);
    }
    uint32_t npairs;
    f.read((char*)&npairs, 4);
    db.pairs.resize(npairs);
    for (uint32_t i = 0; i < npairs; i++) {
        TwoViewMatches& p = db.pairs[i];
        uint32_t nm;
        f.read((char*)&p.image1, 4);
        f.read((char*)&p.image2, 4);
        f.read((char*)&p.config, 4);
        f.read((char*)&nm, 4);
        auto& matches = p.matches.mut();
        matches.resize(nm);
        f.read((char*)matches.data(), (std::streamsize)nm * sizeof(FeatureMatch));
    }
    readMatchesCameras(f, version, db, path);
    return db;
}

// ---- one pair at a time -------------------------------------------------
//
// The pair table without the matches themselves, for a reader that wants to
// draw one pair out of a file whose match arrays are most of a gigabyte. The
// GUI's match map is the caller: it needs every pair's size to draw, and one
// pair's contents only when the cursor is over it.

// A pair count of this means "pairs until the end of the file": the writer was
// still appending when the file was made (sfm/core/Progress.h, live_matches.bin).
inline constexpr uint32_t kStreamingPairs = 0xFFFFFFFFu;

struct MatchesIndex {
    struct Entry {
        uint32_t image1 = 0, image2 = 0, count = 0;
        uint64_t offset = 0;      // first idx1 of this pair's array
        int32_t config = 0;
    };
    std::vector<ImageEntry> images;
    std::vector<Entry> pairs;
    MatchesDatabase metadata;
    bool complete = false;
};

inline bool indexMatches(const std::string& path, MatchesIndex& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    // A file still being written hands back nonsense counts, and this one is
    // read while a run is going. The smallest a pair can be on disk is 16
    // bytes, so the file's own size is what bounds them.
    f.seekg(0, std::ios::end);
    const uint64_t bytes = (uint64_t)f.tellg();
    f.seekg(0);
    char magic[4];
    f.read(magic, 4);
    uint32_t version = 0, nimg = 0;
    f.read((char*)&version, 4);
    f.read((char*)&nimg, 4);
    if (!f || std::memcmp(magic, "VKMT", 4) != 0 || version < 2 || version > 4 ||
        uint64_t(nimg) * 8 > bytes) return false;
    MatchesIndex idx;
    idx.images.resize(nimg);
    for (uint32_t i = 0; i < nimg; i++) {
        uint32_t len = 0;
        f.read((char*)&len, 4);
        if (!f || len > (1u << 20)) return false;
        idx.images[i].name.resize(len);
        f.read(&idx.images[i].name[0], len);
        f.read((char*)&idx.images[i].num_features, 4);
    }
    uint32_t npairs = 0;
    f.read((char*)&npairs, 4);
    const bool streaming = npairs == kStreamingPairs;
    if (!f || (!streaming && (uint64_t)npairs * 16 > bytes)) return false;
    if (!streaming) idx.pairs.reserve(npairs);
    for (uint32_t i = 0; streaming || i < npairs; i++) {
        MatchesIndex::Entry e;
        f.read((char*)&e.image1, 4);
        f.read((char*)&e.image2, 4);
        f.read((char*)&e.config, 4);
        f.read((char*)&e.count, 4);
        // Streaming stops at the tail the writer has not finished; a fixed
        // count that runs out is a truncated file and stays an error.
        if (!f) return streaming ? (out = std::move(idx), true) : false;
        e.offset = (uint64_t)f.tellg();
        // seekg past the end does not fail until something is read, so the
        // bound is checked here for both shapes: a short streaming file is a
        // tail the writer has not finished, a short fixed one is truncated.
        if (e.offset + (uint64_t)e.count * 8 > bytes)
            return streaming ? (out = std::move(idx), true) : false;
        if (e.image1 >= nimg || e.image2 >= nimg || e.image1 == e.image2) return false;
        idx.pairs.push_back(e);
        f.seekg((std::streamoff)e.count * 8, std::ios::cur);
        if (!f) return streaming ? (out = std::move(idx), true) : false;
    }
    idx.metadata.images = idx.images;
    readMatchesCameras(f, version, idx.metadata, path);
    idx.complete = true;
    out = std::move(idx);
    return true;
}

inline bool readPairMatches(std::istream& f,
                            const MatchesIndex::Entry& e,
                            std::vector<FeatureMatch>& out) {
    out.clear();
    if (!e.count) return true;
    f.clear();
    f.seekg((std::streamoff)e.offset);
    out.resize(e.count);
    for (uint32_t i = 0; i < e.count && f; i++) {
        f.read((char*)&out[i].idx1, 4);
        f.read((char*)&out[i].idx2, 4);
    }
    if (!f) { out.clear(); return false; }
    return true;
}

inline bool readPairMatches(const std::string& path, const MatchesIndex::Entry& e,
                            std::vector<FeatureMatch>& out) {
    std::ifstream f(path, std::ios::binary);
    return f && readPairMatches(f, e, out);
}

// Every pair's matches into a SpillFile at `path`, read back as MatchList
// views. False, with `db` untouched, on any failure.
inline bool spillMatches(MatchesDatabase& db, const std::string& path) {
    std::vector<std::pair<const void*, size_t>> chunks;
    for (const TwoViewMatches& p : db.pairs)
        if (!p.matches.empty()) chunks.push_back({p.matches.data(), p.matches.size() * 8});
    std::shared_ptr<const SpillFile> spill = SpillFile::write(path, chunks);
    if (!spill) return false;
    const FeatureMatch* at = reinterpret_cast<const FeatureMatch*>(spill->data());
    for (TwoViewMatches& p : db.pairs) {
        const size_t n = p.matches.size();
        p.matches.view(at, n);
        at += n;
    }
    db.spill = std::move(spill);
    releaseFreedHeap();
    return true;
}

}  // namespace sfm
