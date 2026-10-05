// Keypoint masking: uv sampling, mask decode, file discovery (host only).
//
// Prints PASS/FAIL and returns 0/1. See docs/testing.md.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <vector>

#include "sfm/core/Features.h"
#include "sfm/core/Image.h"
#include "sfm/core/Mask.h"
#include "sfm/tests/TestMain.h"

namespace fs = std::filesystem;

// Do two paths name the same file? Not a string comparison: MaskIndex::find
// returns a path it composed, and on Windows that legitimately differs from the
// one the test wrote -- `dir / "x/dup.png"` keeps the embedded '/' next to the
// native '\', and a case-insensitive filesystem answers `exists()` for
// "e.jpg.jpeg" when what is on disk is "e.jpg.JPEG". Both open the intended
// file, which is what the resolver promises.
bool samePath(const std::string& a, const std::string& b) {
    if (a == b) return true;
    std::error_code ec;
    return !a.empty() && !b.empty() && fs::equivalent(a, b, ec);
}
using namespace sfm;

// -----------------------------------------------------------------------
// mask-selftest: keypoint masking (host only, no GPU)
// -----------------------------------------------------------------------
//
// The three things that can silently go wrong (D39): sampling a mask whose
// resolution differs from the image's, compacting the parallel keypoint /
// descriptor / color arrays out of step, and picking the wrong file for an
// image. One case per section, all on data written to a scratch directory.

// Binary PGM, the one mask format we can *write* without another vendored
// header. stb_image sniffs content rather than extension, so these stand in
// for the .png masks a real dataset ships.
static void writePgm(const fs::path& p, int w, int h, const std::vector<uint8_t>& px) {
    fs::create_directories(p.parent_path());
    std::ofstream f(p, std::ios::binary);
    f << "P5\n" << w << " " << h << "\n255\n";
    f.write((const char*)px.data(), (std::streamsize)px.size());
}

// A mask whose left `keep_frac` of the width is keep (255) and the rest ignore.
static std::vector<uint8_t> leftHalfMask(int w, int h, double keep_frac) {
    std::vector<uint8_t> px((size_t)w * h, 0);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            px[(size_t)y * w + x] = (x < keep_frac * w) ? 255 : 0;
    return px;
}

int cmdMaskSelftest(int, char**) {
    int fails = 0;
    const fs::path tmp = fs::temp_directory_path() / "spirula_sfm_mask_selftest";
    std::error_code ec;
    fs::remove_all(tmp, ec);

    // ---- 1. uv sampling is resolution-independent ----
    // The same geometric mask at four resolutions must classify the same
    // keypoints. This is the property COLMAP does not have: it indexes the mask
    // by integer pixel, so only the resolution that happens to match the source
    // image is correct and the others silently drop everything out of range.
    {
        const int IW = 400, IH = 300;
        const int res[][2] = {{400, 300}, {40, 30}, {1200, 900}, {97, 73}};
        // Keypoints straddling the boundary, plus the extreme corners (which a
        // bounds-checking sampler must keep, not delete).
        std::vector<Keypoint> kps;
        for (int i = 0; i < 40; i++)
            kps.push_back({(float)(i * 10 + 0.5f), (float)(IH / 2), 2, 0, 0});
        kps.push_back({0.0f, 0.0f, 2, 0, 0});
        kps.push_back({(float)(IW - 0.01f), (float)(IH - 0.01f), 2, 0, 0});

        std::vector<uint32_t> kept_per_res;
        for (const auto& r : res) {
            Mask m;
            m.width = r[0];
            m.height = r[1];
            m.bits.resize((size_t)r[0] * r[1]);
            std::vector<uint8_t> px = leftHalfMask(r[0], r[1], 0.5);
            for (size_t i = 0; i < m.bits.size(); i++) m.bits[i] = px[i] != 0;

            FeatureSet fs;
            fs.width = IW;
            fs.height = IH;
            fs.dim = 4;
            fs.keypoints = kps;
            fs.descriptors.resize(kps.size() * 4);
            for (size_t i = 0; i < kps.size(); i++)
                for (int d = 0; d < 4; d++) fs.descriptors[i * 4 + d] = (uint8_t)(i + d);
            fs.colors.assign(kps.size() * 3, 0);
            for (size_t i = 0; i < kps.size(); i++) fs.colors[i * 3] = (uint8_t)i;

            applyMask(fs, m);
            kept_per_res.push_back(fs.count());
            // Everything kept must be in the left half, and the descriptor /
            // color rows must still belong to their keypoint. The boundary is
            // only resolvable to one mask cell, so a coarse mask gets that much
            // slack (97 columns over 400 px puts its edge 0.7 px past centre).
            const float tol = (float)IW / r[0];
            for (uint32_t i = 0; i < fs.count(); i++) {
                if (fs.keypoints[i].x >= IW * 0.5f + tol) {
                    printf("  FAIL: mask %dx%d kept a right-half keypoint at x=%.1f\n", r[0], r[1],
                           fs.keypoints[i].x);
                    fails++;
                    break;
                }
                uint8_t tag = fs.colors[i * 3];
                if (fs.descriptors[i * 4] != tag || fs.descriptors[i * 4 + 3] != (uint8_t)(tag + 3)) {
                    printf("  FAIL: mask %dx%d desaligned descriptors/colors at %u\n", r[0], r[1], i);
                    fails++;
                    break;
                }
            }
        }
        printf("mask: uv sampling at %dx%d over masks 400x300/40x30/1200x900/97x73 kept "
               "%u/%u/%u/%u of %zu\n", IW, IH, kept_per_res[0], kept_per_res[1], kept_per_res[2],
               kept_per_res[3], kps.size());
        // The coarsest mask (40x30) quantizes the boundary to 10 image px, so
        // allow one keypoint of slack there; the rest must agree exactly.
        for (size_t i = 1; i < kept_per_res.size(); i++) {
            uint32_t d = kept_per_res[i] > kept_per_res[0] ? kept_per_res[i] - kept_per_res[0]
                                                           : kept_per_res[0] - kept_per_res[i];
            if (d > 1) {
                printf("  FAIL: resolution %d changed the kept count by %u\n", res[i][0], d);
                fails++;
            }
        }
        // Corners survive: a keypoint inside the image is never deleted for
        // being unreachable by the sampler.
        Mask allkeep;
        allkeep.width = allkeep.height = 3;
        allkeep.bits.assign(9, 1);
        FeatureSet fs2;
        fs2.width = IW; fs2.height = IH; fs2.dim = 1;
        fs2.keypoints = kps;
        fs2.descriptors.assign(kps.size(), 0);
        if (applyMask(fs2, allkeep) != 0) { printf("  FAIL: all-keep mask dropped keypoints\n"); fails++; }
    }

    // ---- 2. decode: formats, binarization, failure ----
    {
        std::vector<uint8_t> px = leftHalfMask(64, 48, 0.25);
        // A grayscale ramp on the keep side, to check that "nonzero = keep"
        // holds rather than some threshold at 128.
        for (int y = 0; y < 48; y++)
            for (int x = 0; x < 16; x++) px[(size_t)y * 64 + x] = (uint8_t)(1 + (x % 3));
        writePgm(tmp / "decode" / "m.pgm", 64, 48, px);
        Mask m = loadMask((tmp / "decode" / "m.pgm").string());
        double keep = m.keepFraction();
        printf("mask: decoded %dx%d, keep fraction %.3f (expect 0.250)\n", m.width, m.height, keep);
        if (m.width != 64 || m.height != 48 || std::fabs(keep - 0.25) > 1e-6) {
            printf("  FAIL: mask decode / binarization\n");
            fails++;
        }
        for (uint8_t b : m.bits)
            if (b > 1) { printf("  FAIL: mask not binarized to 0/1\n"); fails++; break; }
        // A file that is not an image yields an empty mask (which masks
        // nothing) rather than throwing -- one bad mask must not lose the run.
        {
            fs::create_directories(tmp / "decode");
            std::ofstream(tmp / "decode" / "junk.png", std::ios::binary) << "not an image";
        }
        Mask bad = loadMask((tmp / "decode" / "junk.png").string());
        if (!bad.empty()) { printf("  FAIL: undecodable mask did not come back empty\n"); fails++; }
        FeatureSet fs;
        fs.width = 100; fs.height = 100; fs.dim = 1;
        fs.keypoints.assign(5, Keypoint{50, 50, 2, 0, 0});
        fs.descriptors.assign(5, 0);
        if (applyMask(fs, bad) != 0 || fs.count() != 5) {
            printf("  FAIL: empty mask must keep every keypoint\n");
            fails++;
        }
    }

    // ---- 2b. --masks and --feature-masks intersect, at the finer grid ----
    {
        auto grid = [](int w, int h, bool left_half) {
            Mask m;
            m.width = w;
            m.height = h;
            m.bits.resize((size_t)w * h);
            for (int y = 0; y < h; y++)
                for (int x = 0; x < w; x++)
                    m.bits[(size_t)y * w + x] = left_half ? x < w / 2 : y < h / 2;
            return m;
        };
        Mask a = grid(40, 30, true);
        intersectMask(a, grid(120, 90, false));
        const double keep = a.keepFraction();
        printf("mask: 40x30 left half & 120x90 top half -> %dx%d, keep %.3f (expect 0.250)\n",
               a.width, a.height, keep);
        if (a.width != 120 || a.height != 90 || std::fabs(keep - 0.25) > 1e-6 ||
            !a.atUV(0.1f, 0.1f) || a.atUV(0.9f, 0.1f) || a.atUV(0.1f, 0.9f)) {
            printf("  FAIL: intersection of masks at two resolutions\n");
            fails++;
        }
        Mask none;
        intersectMask(none, grid(8, 8, true));
        Mask kept = grid(8, 8, true);
        intersectMask(kept, Mask{});
        if (none.keepFraction() != 0.5 || kept.keepFraction() != 0.5) {
            printf("  FAIL: an empty mask must leave the other as it is\n");
            fails++;
        }

        // Through the loader: both files apply, and a first mask that will not
        // decode is reported as such rather than hidden by the second.
        const fs::path d = tmp / "both";
        writePgm(d / "img.pgm", 64, 48, std::vector<uint8_t>((size_t)64 * 48, 128));
        writePgm(d / "a.pgm", 64, 48, leftHalfMask(64, 48, 0.5));
        std::vector<uint8_t> top((size_t)64 * 48, 0);
        std::fill(top.begin(), top.begin() + (size_t)64 * 24, (uint8_t)255);
        writePgm(d / "b.pgm", 64, 48, top);
        std::ofstream(d / "junk.png", std::ios::binary) << "not an image";
        const std::string img = (d / "img.pgm").string();
        GrayImage g = loadGrayImage(img, 0, false, (d / "a.pgm").string(), "", std::nullopt,
                                    false, false, (d / "b.pgm").string());
        GrayImage only = loadGrayImage(img, 0, false, "", "", std::nullopt, false, false,
                                       (d / "b.pgm").string());
        GrayImage broken = loadGrayImage(img, 0, false, (d / "junk.png").string(), "",
                                         std::nullopt, false, false, (d / "b.pgm").string());
        if (std::fabs(g.mask.keepFraction() - 0.25) > 1e-6 ||
            std::fabs(only.mask.keepFraction() - 0.5) > 1e-6 || !broken.mask.empty()) {
            printf("  FAIL: loader with a feature mask kept %.3f / %.3f, broken %s\n",
                   g.mask.keepFraction(), only.mask.keepFraction(),
                   broken.mask.empty() ? "empty" : "not empty");
            fails++;
        }
    }

    // ---- 3. discovery: the naming conventions in the wild ----
    {
        const fs::path md = tmp / "masks";
        std::vector<uint8_t> px = leftHalfMask(8, 8, 0.5);
        struct Case { const char* image; const char* mask; };
        const Case cases[] = {
            {"cam0/a.jpg", "cam0/a.jpg.png"},      // COLMAP / Spirula Studio / SAM
            {"cam0/b.jpg", "cam0/b.png"},          // COLMAP's alternate, nerfstudio
            {"cam0/c.jpg", "cam0/c_mask.png"},     // suffix form
            {"cam0/d.jpg", "cam0/d.jpg"},          // same name, same extension
            {"cam1/e.jpg", "cam1/e.jpg.JPEG"},     // upper-case, different container
            {"cam1/f.jpg", "f.png"},               // flat mask dir, nested images
            {"cam1/g.png", "cam1/g.png.png"},      // png image, png mask
        };
        for (const Case& c : cases) writePgm(md / c.mask, 8, 8, px);
        MaskIndex idx(md.string());
        for (const Case& c : cases) {
            std::string got = idx.find(c.image);
            std::string want = (md / c.mask).string();
            if (!samePath(got, want)) {
                printf("  FAIL: %s -> \"%s\", expected \"%s\"\n", c.image, got.c_str(), want.c_str());
                fails++;
            }
        }
        printf("mask: resolved %zu naming conventions\n", sizeof(cases) / sizeof(cases[0]));

        // An image with no mask resolves to nothing (rather than to some other
        // image's mask).
        if (!idx.find("cam0/nosuch.jpg").empty()) {
            printf("  FAIL: invented a mask for an unmasked image\n");
            fails++;
        }
        // Ambiguity: two sub-folders both holding `dup.png` must not resolve a
        // *flat* image name -- but each nested image still resolves its own.
        writePgm(md / "x" / "dup.png", 8, 8, px);
        writePgm(md / "y" / "dup.png", 8, 8, px);
        MaskIndex idx2(md.string());
        if (!idx2.find("dup.jpg").empty()) {
            printf("  FAIL: ambiguous basename resolved anyway\n");
            fails++;
        }
        if (!samePath(idx2.find("x/dup.jpg"), (md / "x" / "dup.png").string())) {
            printf("  FAIL: unambiguous nested name did not resolve\n");
            fails++;
        }
        // A missing mask directory is inert, not fatal.
        if (MaskIndex((tmp / "nope").string()).valid()) {
            printf("  FAIL: missing mask directory reported valid\n");
            fails++;
        }
    }

    fs::remove_all(tmp, ec);
    printf("%s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}

int main() { return sfmTestMain(0, nullptr, cmdMaskSelftest); }
