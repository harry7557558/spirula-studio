// ForeignPhoto.cpp -- see ForeignPhoto.h. JPEG XL's header layout is ISO/IEC
// 18181-1 (SizeHeader, ImageMetadata) and 18181-2 (the container's boxes).

#include "app/gui/ForeignPhoto.h"

#include "app/FrameMask.h"
#include "app/gui/Subprocess.h"
#include "core/AvifImage.h"
#include "core/JxlImage.h"
#include "i18n/catalog/Log.h"
#ifdef SS_HAVE_VIDEO
#include "video/Video.h"
#endif

#include <algorithm>
#include <cctype>
#include <chrono>
#include <climits>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <mutex>
#include <thread>

namespace fs = std::filesystem;
namespace lmsg = spirula::i18n::msg::log;

namespace gui {

namespace {

std::string lower_ext(const std::string& path) {
    std::string e = fs::path(path).extension().string();
    for (char& c : e) c = (char)std::tolower((unsigned char)c);
    return e;
}

// "8.1.2-full_build" -> 801, "n7.1" -> 701. 0 for anything else, such as a
// build from git ("N-118000-g..."), which is newer than any release refused.
int version_number(const std::string& v) {
    size_t i = (!v.empty() && v[0] == 'n') ? 1 : 0;
    if (i >= v.size() || !std::isdigit((unsigned char)v[i])) return 0;
    const int major = std::atoi(v.c_str() + i);
    const size_t dot = v.find('.', i);
    const int minor = dot == std::string::npos ? 0 : std::atoi(v.c_str() + dot + 1);
    return major * 100 + minor;
}

// The word after "ffmpeg version", asked once per executable.
std::string ffmpeg_version(const std::string& exe, const std::atomic<bool>& cancel) {
    static std::mutex mu;
    static std::map<std::string, std::string> seen;
    {
        std::lock_guard<std::mutex> lk(mu);
        auto it = seen.find(exe);
        if (it != seen.end()) return it->second;
    }
    std::string first;
    const int rc = run_process({exe, "-version"}, "",
                               [&first](const std::string& line) {
                                   if (first.empty()) first = line;
                               },
                               cancel);
    static const std::string kHead = "ffmpeg version ";
    std::string v;
    if (first.rfind(kHead, 0) == 0) {
        v = first.substr(kHead.size());
        v = v.substr(0, v.find(' '));
    }
    // A cancelled or failed probe is asked again next time, not remembered.
    if (rc != 0) return v;
    std::lock_guard<std::mutex> lk(mu);
    seen[exe] = v;
    return v;
}

// ================
// JPEG XL
// ================

// Enough of a codestream to hold its SizeHeader and ImageMetadata up to the
// extra channels, names included, which is all that is read of it.
constexpr size_t kJxlHeadBytes = 4096;
constexpr uint64_t kMaxExifBox = 1u << 20;

uint32_t be32(const uint8_t* p) {
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

// A bare codestream (FF 0A) is its own head; a container is walked box by box
// for the 'Exif' and the codestream's start, which 'jxlp' parts can split
// anywhere (8 bytes in, in files exiftool rewrote). False when neither shape.
bool read_jxl(const std::string& path, std::vector<uint8_t>* head,
              std::vector<uint8_t>* exif, bool* exif_compressed = nullptr) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    f.seekg(0, std::ios::end);
    const uint64_t size = (uint64_t)f.tellg();
    f.seekg(0);
    uint8_t sig[12] = {};
    const size_t got = (size_t)std::min<uint64_t>(size, 12);
    if (got < 2 || !f.read((char*)sig, (std::streamsize)got)) return false;
    auto read_at = [&](uint64_t at, size_t n, std::vector<uint8_t>& out) {
        out.resize(n);
        f.seekg((std::streamoff)at);
        return n == 0 || (bool)f.read((char*)out.data(), (std::streamsize)n);
    };
    if (sig[0] == 0xFF && sig[1] == 0x0A)
        return !head || read_at(0, (size_t)std::min<uint64_t>(size, kJxlHeadBytes), *head);
    static const uint8_t kSig[12] = {0, 0, 0, 12, 'J', 'X', 'L', ' ', 0x0D, 0x0A, 0x87, 0x0A};
    if (size < 12 || !std::equal(kSig, kSig + 12, sig)) return false;

    bool any_head = false;
    bool head_done = false;
    for (uint64_t pos = 12; pos + 8 <= size;) {
        std::vector<uint8_t> h;
        if (!read_at(pos, 8, h)) break;
        uint64_t len = be32(h.data());
        const uint32_t type = be32(h.data() + 4);
        uint64_t hdr = 8;
        if (len == 1) {
            if (pos + 16 > size || !read_at(pos + 8, 8, h)) break;
            len = (uint64_t)be32(h.data()) << 32 | be32(h.data() + 4);
            hdr = 16;
        } else if (len == 0) {
            len = size - pos;
        }
        if (len < hdr || len > size - pos) break;
        const uint64_t body = pos + hdr, n = len - hdr;
        if (type == be32((const uint8_t*)"Exif") && exif && exif->empty() && n > 4 &&
            n <= kMaxExifBox) {
            std::vector<uint8_t> box;
            if (read_at(body, (size_t)n, box)) {
                // A big-endian offset from the box payload to the TIFF header.
                const uint64_t tiff = 4 + (uint64_t)be32(box.data());
                if (tiff + 8 <= n) {
                    static const uint8_t kApp1[6] = {'E', 'x', 'i', 'f', 0, 0};
                    exif->assign(kApp1, kApp1 + 6);
                    exif->insert(exif->end(), box.begin() + (ptrdiff_t)tiff, box.end());
                }
            }
        } else if (type == be32((const uint8_t*)"brob") && exif_compressed && n >= 4) {
            std::vector<uint8_t> inner;
            if (read_at(body, 4, inner) && be32(inner.data()) == be32((const uint8_t*)"Exif"))
                *exif_compressed = true;
        } else if (head && !head_done && (type == be32((const uint8_t*)"jxlc") ||
                                           type == be32((const uint8_t*)"jxlp"))) {
            // A 'jxlp' opens with its 4-byte part index.
            const uint64_t skip = type == be32((const uint8_t*)"jxlp") ? 4 : 0;
            std::vector<uint8_t> part;
            const size_t want = kJxlHeadBytes - head->size();
            if (n > skip &&
                read_at(body + skip, (size_t)std::min<uint64_t>(n - skip, want), part)) {
                head->insert(head->end(), part.begin(), part.end());
                any_head = true;
            }
            head_done = type == be32((const uint8_t*)"jxlc") || head->size() >= kJxlHeadBytes;
        }
        pos += len;
    }
    return any_head || (exif && !exif->empty());
}

// LSB-first, as the codestream is packed. Reads past the end set `bad`.
struct BitReader {
    const uint8_t* p;
    size_t n;
    size_t bit = 0;
    bool bad = false;
    uint32_t u(int k) {
        uint32_t v = 0;
        for (int i = 0; i < k; i++, bit++) {
            if (bit / 8 >= n) { bad = true; return 0; }
            v |= (uint32_t)((p[bit / 8] >> (bit % 8)) & 1) << i;
        }
        return v;
    }
};

struct Dist {
    int bits;
    uint32_t offset;
};

struct JxlHeader {
    int w = 0, h = 0;   // upright
    bool alpha = false;
};

// 18181-1's U32(): two selector bits pick one of four distributions.
uint32_t u32(BitReader& b, const Dist (&d)[4]) {
    const Dist& x = d[b.u(2)];
    return b.u(x.bits) + x.offset;
}

void skip_bit_depth(BitReader& b) {
    static const Dist kFloat[4] = {{0, 32}, {0, 16}, {0, 24}, {6, 1}};
    static const Dist kInt[4] = {{0, 8}, {0, 10}, {0, 12}, {6, 1}};
    if (b.u(1)) {
        u32(b, kFloat);
        b.u(4);
    } else {
        u32(b, kInt);
    }
}

// SizeHeader, then ImageMetadata as far as the first alpha channel (18181-1
// Annex A). Orientation 5..8 swaps the axes: the size is the upright one
// ffmpeg's decode has. Checked against libjxl's JxlBasicInfo.
bool jxl_header(const std::vector<uint8_t>& cs, JxlHeader& out) {
    if (cs.size() < 3 || cs[0] != 0xFF || cs[1] != 0x0A) return false;
    BitReader b{cs.data() + 2, cs.size() - 2};
    static const Dist kSize[4] = {{9, 1}, {13, 1}, {18, 1}, {30, 1}};
    static const uint32_t kNum[7] = {1, 12, 4, 3, 16, 5, 2};
    static const uint32_t kDen[7] = {1, 10, 3, 2, 9, 4, 1};
    auto size_header = [&](uint64_t& xs, uint64_t& ys) {
        const bool div8 = b.u(1) != 0;
        auto dim = [&]() -> uint64_t {
            return div8 ? (uint64_t)(b.u(5) + 1) * 8 : u32(b, kSize);
        };
        ys = dim();
        const uint32_t ratio = b.u(3);
        xs = ratio == 0 ? dim() : ys * kNum[ratio - 1] / kDen[ratio - 1];
    };
    uint64_t xs = 0, ys = 0;
    size_header(xs, ys);
    int orientation = 1;
    bool alpha = false;
    if (!b.u(1)) {        // all_default
        if (b.u(1)) {     // extra_fields
            orientation = 1 + (int)b.u(3);
            if (b.u(1)) {
                uint64_t ix = 0, iy = 0;
                size_header(ix, iy);
            }
            if (b.u(1)) {   // preview
                static const Dist kDiv8[4] = {{0, 16}, {0, 32}, {5, 1}, {9, 33}};
                static const Dist kPx[4] = {{6, 1}, {8, 65}, {10, 321}, {12, 1345}};
                const bool div8 = b.u(1) != 0;
                u32(b, div8 ? kDiv8 : kPx);
                if (b.u(3) == 0) u32(b, div8 ? kDiv8 : kPx);
            }
            if (b.u(1)) {   // animation
                static const Dist kNumer[4] = {{0, 100}, {0, 1000}, {10, 1}, {30, 1}};
                static const Dist kDenom[4] = {{0, 1}, {0, 1001}, {8, 1}, {10, 1}};
                static const Dist kLoops[4] = {{0, 0}, {3, 0}, {16, 0}, {32, 0}};
                u32(b, kNumer);
                u32(b, kDenom);
                u32(b, kLoops);
                b.u(1);
            }
        }
        skip_bit_depth(b);
        b.u(1);   // modular_16_bit_buffers
        static const Dist kCount[4] = {{0, 0}, {0, 1}, {4, 2}, {12, 1}};
        static const Dist kType[4] = {{0, 0}, {0, 1}, {4, 2}, {6, 18}};
        static const Dist kShift[4] = {{0, 0}, {0, 3}, {0, 4}, {3, 1}};
        static const Dist kName[4] = {{0, 0}, {4, 0}, {5, 16}, {10, 48}};
        static const Dist kCfa[4] = {{0, 1}, {2, 0}, {4, 3}, {8, 19}};
        const uint32_t channels = u32(b, kCount);
        for (uint32_t i = 0; i < channels && !alpha && !b.bad; i++) {
            // all_default describes an 8-bit alpha; type 0 is alpha too.
            if (b.u(1)) {
                alpha = true;
                break;
            }
            const uint32_t type = u32(b, kType);
            if (type == 0) {
                alpha = true;
                break;
            }
            skip_bit_depth(b);
            u32(b, kShift);
            const uint32_t name = u32(b, kName);
            for (uint32_t k = 0; k < name && !b.bad; k++) b.u(8);
            if (type == 2)
                for (int k = 0; k < 4; k++) b.u(16);   // spot colour
            if (type == 5) u32(b, kCfa);
        }
    }
    if (b.bad || xs == 0 || ys == 0 || xs > INT_MAX || ys > INT_MAX) return false;
    out.w = (int)xs;
    out.h = (int)ys;
    if (orientation > 4) std::swap(out.w, out.h);
    out.alpha = alpha;
    return true;
}

// An AVIF's alpha is an auxiliary image whose 'auxC' names this URN; it is
// searched for in the 'meta' box's bytes rather than the item graph walked.
bool avif_has_alpha(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    static const std::string kUrn = "urn:mpeg:mpegB:cicp:systems:auxiliary:alpha";
    for (uint64_t pos = 0;;) {
        uint8_t h[8];
        f.seekg((std::streamoff)pos);
        if (!f.read((char*)h, 8)) return false;
        const uint64_t len = be32(h);
        if (be32(h + 4) == be32((const uint8_t*)"meta")) {
            if (len < 8 || len > (16u << 20)) return false;
            std::string body((size_t)len - 8, ' ');
            return (bool)f.read(&body[0], (std::streamsize)body.size()) &&
                   body.find(kUrn) != std::string::npos;
        }
        if (len < 8) return false;
        pos += len;
    }
}

}  // namespace

bool is_heif_path(const std::string& path) {
    const std::string e = lower_ext(path);
    return e == ".heic" || e == ".heif" || e == ".hif";
}

bool is_foreign_photo_path(const std::string& path) {
    const std::string e = lower_ext(path);
    return e == ".jxl" || e == ".avif" || is_heif_path(path);
}

bool photo_needs_jpeg(const std::string& path) {
    const std::string e = lower_ext(path);
    if (e == ".jxl") return !jxl::available();
    if (e == ".avif") return !avif::available();
    return is_heif_path(path);
}

bool decode_heif_builtin(const std::string& path, int& w, int& h,
                         std::vector<uint8_t>& rgb, std::vector<uint8_t>* exif,
                         std::string& error) {
#ifdef SS_HAVE_VIDEO
    nn::Image im;
    if (!video::decode_heif(path, im, exif, error)) return false;
    w = im.width;
    h = im.height;
    rgb = std::move(im.data);
    return true;
#else
    (void)path; (void)w; (void)h; (void)rgb; (void)exif;
    error = "built without the in-process decoder (-DSS_ENABLE_PATENTED=OFF)";
    return false;
#endif
}

bool ffmpeg_photo_to_jpeg(const std::string& ffmpeg_exe, const std::string& photo,
                          const std::string& jpeg, const std::atomic<bool>& cancel,
                          std::string& error) {
    return ffmpeg_photo_convert(ffmpeg_exe, photo, jpeg, false, cancel, error);
}

bool ffmpeg_photo_convert(const std::string& ffmpeg_exe, const std::string& photo,
                          const std::string& out, bool rgba_png,
                          const std::atomic<bool>& cancel, std::string& error) {
    if (!command_exists(ffmpeg_exe)) {
        error = spirula::i18n::format(lmsg::err_ffmpeg_missing, {ffmpeg_exe});
        return false;
    }
    const bool heif = is_heif_path(photo);
    // Tiled HEIF arrived in 7.0; before it the grid is not put together, and a
    // lone tile is a size nothing downstream can tell from a real photo's.
    if (heif) {
        const std::string version = ffmpeg_version(ffmpeg_exe, cancel);
        const int n = version_number(version);
        if (n > 0 && n < 700) {
            error = spirula::i18n::format(lmsg::err_ffmpeg_heif_too_old,
                                          {ffmpeg_exe, version});
            return false;
        }
    }
    std::error_code ec;
    fs::remove(out, ec);
    std::string last;
    // HEIF one at a time: ffmpeg 8.1 commits 2.6 GB for a 24 MP iPhone grid,
    // and two at once failed with ENOMEM on 16 GB. A 33 MP JPEG XL peaks at
    // 0.35 GB and an AVIF at 0.25 GB, so those run beside each other.
    static std::mutex one_at_a_time;
    std::unique_lock<std::mutex> lk(one_at_a_time, std::defer_lock);
    if (heif) lk.lock();
    // The muxer is named, not guessed from `out`: callers write to a
    // temporary name ("a.jpg.part") that ffmpeg would refuse.
    std::vector<std::string> argv{ffmpeg_exe, "-nostdin", "-y", "-v", "error", "-i", photo};
    if (rgba_png && lower_ext(photo) == ".avif") {
        // ffmpeg reads an AVIF's alpha as a second stream and drops it unless asked.
        argv.insert(argv.end(), {"-filter_complex", "[0:v:0][0:v:1]alphamerge"});
    }
    if (rgba_png)
        argv.insert(argv.end(), {"-frames:v", "1", "-pix_fmt", "rgba", "-f", "image2",
                                 "-c:v", "png", out});
    else
        argv.insert(argv.end(), {"-frames:v", "1", "-q:v", "2", "-f", "mjpeg", out});
    const int rc = run_process(
        argv,
        "",
        [&last](const std::string& line) {
            if (!line.empty()) last = line;
        },
        cancel);
    if (rc == 0 && fs::exists(out, ec) && fs::file_size(out, ec) > 0) return true;
    fs::remove(out, ec);
    if (rc == kCancelled) {
        error = lmsg::err_cancelled.get();
    } else {
        if (last.empty()) last = "ffmpeg exited with " + std::to_string(rc);
        error = spirula::i18n::format(lmsg::err_photo_convert_failed, {photo, last});
    }
    return false;
}

bool foreign_photo_exif(const std::string& path, std::vector<uint8_t>& exif) {
    exif.clear();
    if (lower_ext(path) != ".jxl") return false;
    bool compressed = false;
    read_jxl(path, nullptr, &exif, &compressed);
    return !compressed || !exif.empty();
}

bool foreign_photo_size(const std::string& path, int& w, int& h) {
    if (lower_ext(path) == ".jxl") {
        std::vector<uint8_t> head;
        JxlHeader jh;
        if (!read_jxl(path, &head, nullptr) || !jxl_header(head, jh)) return false;
        w = jh.w;
        h = jh.h;
        return true;
    }
#ifdef SS_HAVE_VIDEO
    if (is_heif_path(path)) {
        std::string error;
        return video::probe_heif(path, w, h, error);
    }
#endif
    (void)w; (void)h;
    return false;
}

bool foreign_photo_has_alpha(const std::string& path) {
    const std::string e = lower_ext(path);
    if (e == ".avif") return avif_has_alpha(path);
    std::vector<uint8_t> head;
    JxlHeader jh;
    return e == ".jxl" && read_jxl(path, &head, nullptr) && jxl_header(head, jh) && jh.alpha;
}

bool load_foreign_photo(const std::string& path, bool builtin,
                        const std::string& ffmpeg_exe, int& w, int& h,
                        std::vector<uint8_t>& rgb, const std::atomic<bool>& cancel,
                        std::string& error) {
    if (builtin && is_heif_path(path) &&
        decode_heif_builtin(path, w, h, rgb, nullptr, error))
        return true;
    const fs::path tmp =
        fs::temp_directory_path() /
        ("spirula-photo-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
         "-" + std::to_string(std::hash<std::thread::id>()(std::this_thread::get_id())) +
         ".jpg");
    bool ok = ffmpeg_photo_to_jpeg(ffmpeg_exe, path, tmp.string(), cancel, error);
    if (ok && !app::load_rgb(tmp.string(), w, h, rgb)) {
        error = spirula::i18n::format(lmsg::err_photo_convert_failed,
                                      {path, "ffmpeg wrote no readable JPEG"});
        ok = false;
    }
    std::error_code ec;
    fs::remove(tmp, ec);
    return ok;
}

}  // namespace gui
