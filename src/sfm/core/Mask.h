// Keypoint masking: the parts of an image SfM is allowed to use.
//
// A mask is a single-channel image; a pixel is *keep* where it is nonzero and
// *ignore* where it is zero -- COLMAP's ImageReader.mask_path convention and
// the trainer's. Masking happens once, at extraction: keypoints under a zero
// pixel are dropped before `features.bin` is written, so every stage downstream
// (matching, verification, mapping, BA) is masked for free and no stage has to
// carry mask state.
//
// Two things here are deliberately *not* COLMAP's behavior (D39):
//
// 1. **Masks are sampled in uv, not in pixels.** COLMAP indexes the mask with
//    the integer keypoint coordinate (`mask.GetPixel((int)kp.x, (int)kp.y)`,
//    controllers/feature_extraction.cc MaskFeatures) after rescaling keypoints
//    back to the *original* image size, so the mask has to match the source
//    resolution exactly. It routinely does not: masks come out of a
//    segmentation model at whatever size the model works at (one capture here
//    ships 1600x1600 masks over 1920x1920 images). COLMAP's out-of-range GetPixel
//    returns nullopt, which MaskFeatures treats as "masked out", so a mask
//    smaller than the image silently deletes every keypoint past its extent --
//    69% of the frame survives in that example and nothing says so. Sampling
//    normalized coordinates makes the resolutions independent, which is also
//    what our own --max-image-size downscale needs (our keypoints live in the
//    decoded frame, not the source frame).
//
// 2. **Mask files are found by convention, not by one fixed name.** COLMAP
//    accepts `<image_name>.png` and `<stem>.png` only. Masks in the wild are
//    named by whatever produced them; MaskIndex below probes the common
//    conventions and, failing that, falls back to an index of the mask
//    directory keyed on progressively-stripped names.
#pragma once

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "sfm/core/Features.h"

namespace sfm {

// Binary mask, row-major, 1 = keep / 0 = ignore.
struct Mask {
    int width = 0, height = 0;
    std::vector<uint8_t> bits;  // width*height, strictly 0 or 1

    bool empty() const { return bits.empty(); }
    size_t pixels() const { return (size_t)width * height; }

    // Sample at normalized image coordinates: u,v in [0,1) over the full image
    // extent, (0,0) at the top-left *corner*. Nearest neighbour -- a mask is
    // categorical, interpolating it would invent boundary values. Out-of-range
    // coordinates clamp to the border rather than counting as masked-out: a
    // keypoint at x = width - 0.2 is inside the image, and COLMAP's habit of
    // deleting anything its indexing cannot reach is the bug being avoided.
    bool atUV(float u, float v) const {
        if (empty()) return true;
        int mx = (int)std::floor(u * width);
        int my = (int)std::floor(v * height);
        mx = std::max(0, std::min(width - 1, mx));
        my = std::max(0, std::min(height - 1, my));
        return bits[(size_t)my * width + mx] != 0;
    }

    // Swap keep and ignore, for the exporters that paint the region to REMOVE.
    void invert() {
        for (uint8_t& b : bits) b = (uint8_t)!b;
    }

    // Fraction of the mask that is "keep". Reported so a run that masks away
    // almost everything (an inverted mask, the classic mistake) is visible.
    double keepFraction() const {
        if (empty()) return 1.0;
        size_t n = 0;
        for (uint8_t b : bits) n += b;
        return (double)n / (double)pixels();
    }
};

// Decode a mask image (any format stb_image reads) to strict 0/1. Returns an
// empty Mask if the file cannot be decoded. Defined in image.cpp, the single
// translation unit that instantiates stb_image (D5).
Mask loadMask(const std::string& path);

// Drop every keypoint of `fs` that falls on a zero mask pixel, compacting
// keypoints, descriptors and colors in place. Returns the number removed.
//
// Keypoints are in `fs`'s own (post-downscale) pixel frame, so the pixel-center
// convention -- pixel i covers [i, i+1), center i + 0.5 -- makes uv = (x + 0.5)
// / width. That is the same convention resizeGray() samples with, so a mask at
// the image's own resolution masks exactly the pixels a human would expect.
inline uint32_t applyMask(FeatureSet& fs, const Mask& m) {
    if (m.empty() || fs.width <= 0 || fs.height <= 0 || fs.keypoints.empty()) return 0;
    const uint32_t n = fs.count();
    // 0 when the set carries keypoints but no descriptors (synthetic sets built
    // by the self-tests do this); compacting then has nothing to move.
    const size_t dbytes = fs.descriptors.size() >= (size_t)n * fs.dim * dtypeSize(fs.dtype)
                              ? (size_t)fs.dim * dtypeSize(fs.dtype)
                              : 0;
    const bool colors = fs.hasColors();
    uint32_t out = 0;
    for (uint32_t i = 0; i < n; i++) {
        const Keypoint& k = fs.keypoints[i];
        if (!m.atUV((k.x + 0.5f) / fs.width, (k.y + 0.5f) / fs.height)) continue;
        if (out != i) {
            fs.keypoints[out] = fs.keypoints[i];
            std::copy_n(fs.descriptors.begin() + (size_t)i * dbytes, dbytes,
                        fs.descriptors.begin() + (size_t)out * dbytes);
            if (colors)
                std::copy_n(fs.colors.begin() + (size_t)i * 3, (size_t)3,
                            fs.colors.begin() + (size_t)out * 3);
        }
        out++;
    }
    fs.keypoints.resize(out);
    if (dbytes) fs.descriptors.resize((size_t)out * dbytes);
    if (colors) fs.colors.resize((size_t)out * 3);
    return n - out;
}

// ---- finding the mask that belongs to an image --------------------------

namespace detail {

inline std::string lowerStr(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

// Drop one trailing extension, if any ("a/b.jpg.png" -> "a/b.jpg").
inline std::string stripExt(const std::string& s) {
    size_t dot = s.rfind('.');
    size_t slash = s.find_last_of("/\\");
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) return s;
    return s.substr(0, dot);
}

// Drop a conventional mask marker from the end of a stem
// ("img_mask" -> "img"). Returns s unchanged when there is none.
inline std::string stripMaskTag(const std::string& s) {
    static const char* kTags[] = {"_mask", "-mask", ".mask", "_masks", "_alpha", "_seg"};
    for (const char* t : kTags) {
        size_t n = std::strlen(t);
        if (s.size() > n && s.compare(s.size() - n, n, t) == 0) return s.substr(0, s.size() - n);
    }
    return s;
}

inline std::string baseName(const std::string& s) {
    size_t slash = s.find_last_of("/\\");
    return slash == std::string::npos ? s : s.substr(slash + 1);
}

}  // namespace detail

// Maps an image's name (its path relative to the image directory, the same
// string that ends up in COLMAP's images.bin) to a mask file under `mask_dir`.
//
// Resolution is two-tier. First the explicit conventions are probed by
// filename, cheapest and least surprising first:
//
//   masks/<name>.png        "00023.jpg.png"   COLMAP, Spirula Studio, SAM tools
//   masks/<stem>.png        "00023.png"       COLMAP's alternate, nerfstudio
//   masks/<stem>_mask.png   "00023_mask.png"  Spirula Studio's suffix form
//
// each over {.png,.jpg,.jpeg} in either case, which also covers a mask sharing
// the image's exact filename. If none exists, a lazily-built index of the mask
// directory is consulted, keyed on the name with extensions and mask markers
// progressively stripped -- this is what catches a mask directory that is flat
// while the images are in sub-folders, or one whose files are `.jpeg` against
// `.jpg` images. Index hits require the key to be *unique* in the mask
// directory, so two `00023.png` under different sub-folders never silently pick
// one for the other.
//
// The index is built lazily on the first miss, so `find` is const but not
// thread-safe. Callers resolve every image's mask up front on one thread (the
// decode pool then only ever opens the resolved paths), which is also where a
// mask directory that matches nothing gets reported.
class MaskIndex {
public:
    MaskIndex() = default;
    explicit MaskIndex(const std::string& mask_dir) : dir_(mask_dir) {
        std::error_code ec;
        valid_ = !mask_dir.empty() && std::filesystem::is_directory(dir_, ec);
    }

    bool valid() const { return valid_; }
    const std::filesystem::path& dir() const { return dir_; }

    // Absolute-or-relative path of the mask for `rel_name`, or "" if there is
    // none. `rel_name` uses '/' separators (generic_string()).
    std::string find(const std::string& rel_name) const {
        if (!valid_) return "";
        namespace fs = std::filesystem;
        const std::string stem = detail::stripExt(rel_name);
        static const char* kExts[] = {".png", ".PNG", ".jpg", ".JPG", ".jpeg", ".JPEG"};
        std::error_code ec;
        for (const char* e : kExts) {
            fs::path p = dir_ / (rel_name + e);
            if (fs::exists(p, ec)) return p.string();
            p = dir_ / (stem + e);
            if (fs::exists(p, ec)) return p.string();
        }
        for (const char* e : kExts) {
            fs::path p = dir_ / (stem + "_mask" + e);
            if (fs::exists(p, ec)) return p.string();
        }
        // Fall back to the stripped-name index.
        if (!indexed_) buildIndex();
        for (const std::string& key : probeKeys(rel_name)) {
            auto it = index_.find(key);
            if (it != index_.end() && !it->second.ambiguous)
                return (dir_ / it->second.rel).string();
        }
        return "";
    }

private:
    // A key claimed by exactly one mask file (or flagged ambiguous). `level`
    // records how specific the claim was, so a file that matches by its full
    // name beats one that only matches after stripping.
    struct Entry {
        std::string rel;
        int level = 0;
        bool ambiguous = false;
    };

    // Keys an image name can be looked up by, most specific first.
    static std::vector<std::string> probeKeys(const std::string& rel_name) {
        using namespace detail;
        const std::string low = lowerStr(rel_name);
        const std::string base = baseName(low);
        std::vector<std::string> k;
        for (const std::string& c : {low, stripExt(low), base, stripExt(base)})
            if (std::find(k.begin(), k.end(), c) == k.end()) k.push_back(c);
        return k;
    }

    void claim(const std::string& key, const std::string& rel, int level) const {
        if (key.empty()) return;
        auto it = index_.find(key);
        if (it == index_.end()) {
            index_.emplace(key, Entry{rel, level, false});
            return;
        }
        if (it->second.rel == rel) return;             // same file, another alias
        if (level < it->second.level) it->second = Entry{rel, level, false};
        else if (level == it->second.level) it->second.ambiguous = true;
    }

    void buildIndex() const {
        using namespace detail;
        indexed_ = true;
        std::error_code ec;
        for (auto it = std::filesystem::recursive_directory_iterator(dir_, ec);
             it != std::filesystem::recursive_directory_iterator(); it.increment(ec)) {
            if (ec) break;
            if (!it->is_regular_file(ec)) continue;
            const std::string rel = it->path().lexically_relative(dir_).generic_string();
            const std::string low = lowerStr(rel);
            const std::string s1 = stripExt(low);       // "00023.jpg.png" -> "00023.jpg"
            const std::string s2 = stripExt(s1);        //                 -> "00023"
            // Levels: 0 = full name, 1 = one extension off, 2 = two off or a
            // mask marker off, +4 for the basename-only forms (which only ever
            // resolve a flat mask directory against nested images).
            claim(low, rel, 0);
            claim(s1, rel, 1);
            claim(s2, rel, 2);
            claim(stripMaskTag(s1), rel, 2);
            claim(stripMaskTag(s2), rel, 2);
            claim(baseName(low), rel, 4);
            claim(baseName(s1), rel, 5);
            claim(baseName(s2), rel, 6);
            claim(baseName(stripMaskTag(s1)), rel, 6);
            claim(baseName(stripMaskTag(s2)), rel, 6);
        }
    }

    std::filesystem::path dir_;
    bool valid_ = false;
    mutable bool indexed_ = false;
    mutable std::map<std::string, Entry> index_;
};

}  // namespace sfm
