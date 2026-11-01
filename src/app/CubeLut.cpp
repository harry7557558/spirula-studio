// CubeLut.cpp -- see CubeLut.h.

#include "app/CubeLut.h"

#include "app/LogLuts.h"
#include "core/HardwareThreads.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <thread>

namespace fs = std::filesystem;

namespace app {

namespace {

const char* skip_space(const char* p, const char* end) {
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\r')) ++p;
    return p;
}

// Up to `n` floats from [p, end); how many were read.
int read_floats(const char* p, const char* end, float* out, int n) {
    int k = 0;
    while (k < n) {
        p = skip_space(p, end);
        if (p >= end) break;
        char* parsed_end = nullptr;
        out[k] = std::strtof(p, &parsed_end);
        if (parsed_end == p || parsed_end > end)
            break;
        p = parsed_end;
        ++k;
    }
    return k;
}

bool keyword(const std::string& line, const char* word, const char*& rest) {
    const size_t n = std::char_traits<char>::length(word);
    if (line.compare(0, n, word) != 0) return false;
    if (line.size() > n && line[n] != ' ' && line[n] != '\t') return false;
    rest = line.c_str() + n;
    return true;
}

}  // namespace

bool is_builtin_lut(const std::string& spec) {
    return spec.rfind(kBuiltinLutPrefix, 0) == 0;
}

std::string builtin_lut_spec(const std::string& id) { return kBuiltinLutPrefix + id; }

std::string lut_display_name(const std::string& spec) {
    if (is_builtin_lut(spec)) {
        const BuiltinLut* b = find_builtin_lut(spec.substr(std::strlen(kBuiltinLutPrefix)));
        return b ? b->name : spec;
    }
    return fs::path(spec).filename().string();
}

bool load_cube_lut(const std::string& path, CubeLut& out, std::string& error) {
    out = CubeLut();
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        error = "cannot open " + path;
        return false;
    }
    bool has_1d = false;
    size_t want = 0;
    std::string line;
    while (std::getline(f, line)) {
        const size_t hash = line.find('#');
        if (hash != std::string::npos) line.resize(hash);
        const size_t a = line.find_first_not_of(" \t\r");
        if (a == std::string::npos) continue;
        line.erase(0, a);
        const char* end = line.c_str() + line.size();
        const char* rest = nullptr;
        if (keyword(line, "TITLE", rest)) {
            const size_t q0 = line.find('"'), q1 = line.rfind('"');
            if (q0 != std::string::npos && q1 > q0) out.title = line.substr(q0 + 1, q1 - q0 - 1);
        } else if (keyword(line, "LUT_3D_SIZE", rest)) {
            float n = 0;
            if (read_floats(rest, end, &n, 1) != 1 || n < 2 || n > 256) {
                error = "bad LUT_3D_SIZE in " + path;
                return false;
            }
            out.size = (int)n;
            want = (size_t)out.size * out.size * out.size;
            out.rgb.reserve(want * 3);
        } else if (keyword(line, "LUT_1D_SIZE", rest)) {
            has_1d = true;
        } else if (keyword(line, "DOMAIN_MIN", rest)) {
            read_floats(rest, end, out.domain_min, 3);
        } else if (keyword(line, "DOMAIN_MAX", rest)) {
            read_floats(rest, end, out.domain_max, 3);
        } else if (keyword(line, "LUT_3D_INPUT_RANGE", rest)) {
            float r[2] = {0, 1};
            if (read_floats(rest, end, r, 2) == 2)
                for (int c = 0; c < 3; c++) {
                    out.domain_min[c] = r[0];
                    out.domain_max[c] = r[1];
                }
        } else if (keyword(line, "LUT_1D_INPUT_RANGE", rest)) {
        } else {
            float v[3];
            if (read_floats(line.c_str(), end, v, 3) != 3) {
                error = "unreadable line in " + path + ": " + line.substr(0, 40);
                return false;
            }
            if (out.size == 0) {
                error = (has_1d ? "a 1D LUT, not a 3D one: " : "no LUT_3D_SIZE before the data: ") + path;
                return false;
            }
            // Resolve writes the shaper's samples first; read as 3D they would
            // only surface as a wrong count.
            if (has_1d) {
                error = "a 1D shaper in front of the 3D LUT is not supported: " + path;
                return false;
            }
            out.rgb.insert(out.rgb.end(), v, v + 3);
        }
    }
    if (out.size == 0) {
        error = (has_1d ? "a 1D LUT, not a 3D one: " : "no LUT_3D_SIZE in ") + path;
        return false;
    }
    if (out.rgb.size() != want * 3) {
        error = path + ": " + std::to_string(out.rgb.size() / 3) + " samples, expected " +
                std::to_string(want);
        return false;
    }
    for (int c = 0; c < 3; c++)
        if (!(out.domain_max[c] > out.domain_min[c])) {
            error = "bad DOMAIN_MIN/DOMAIN_MAX in " + path;
            return false;
        }
    return true;
}

bool write_cube_lut(const CubeLut& lut, const std::string& path, std::string& error) {
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) {
        error = "cannot write " + path;
        return false;
    }
    if (!lut.title.empty()) std::fprintf(f, "TITLE \"%s\"\n", lut.title.c_str());
    std::fprintf(f, "LUT_3D_SIZE %d\nDOMAIN_MIN %.9g %.9g %.9g\nDOMAIN_MAX %.9g %.9g %.9g\n",
                 lut.size, lut.domain_min[0], lut.domain_min[1], lut.domain_min[2],
                 lut.domain_max[0], lut.domain_max[1], lut.domain_max[2]);
    for (size_t i = 0; i + 2 < lut.rgb.size(); i += 3)
        std::fprintf(f, "%.7f %.7f %.7f\n", lut.rgb[i], lut.rgb[i + 1], lut.rgb[i + 2]);
    const bool ok = std::fclose(f) == 0;
    if (!ok) error = "cannot write " + path;
    return ok;
}

std::shared_ptr<const CubeLut> cube_lut_cached(const std::string& spec,
                                               std::string& error) {
    struct Entry {
        std::shared_ptr<const CubeLut> lut;
        fs::file_time_type mtime;
        uintmax_t bytes = 0;
    };
    static std::mutex mu;
    static std::map<std::string, Entry> cache;
    static uint64_t serial = 0;
    std::lock_guard<std::mutex> lock(mu);
    const bool builtin = is_builtin_lut(spec);
    Entry now;
    if (!builtin) {
        // A LUT re-exported from the grading tool keeps its name.
        std::error_code ec;
        now.mtime = fs::last_write_time(spec, ec);
        now.bytes = fs::file_size(spec, ec);
    }
    auto it = cache.find(spec);
    if (it != cache.end() && it->second.mtime == now.mtime && it->second.bytes == now.bytes)
        return it->second.lut;
    auto lut = std::make_shared<CubeLut>();
    const bool ok =
        builtin ? bake_builtin_lut(spec.substr(std::strlen(kBuiltinLutPrefix)), *lut, error)
                : load_cube_lut(spec, *lut, error);
    if (!ok) return nullptr;
    lut->serial = ++serial;
    now.lut = lut;
    cache[spec] = now;
    return lut;
}

bool cube_lut_file(const std::string& spec, std::string& path, std::string& error) {
    if (!is_builtin_lut(spec)) {
        path = spec;
        return true;
    }
    const std::shared_ptr<const CubeLut> lut = cube_lut_cached(spec, error);
    if (!lut) return false;
    uint64_t h = 1469598103934665603ull;   // FNV-1a over the table
    for (float v : lut->rgb) {
        uint32_t bits;
        std::memcpy(&bits, &v, sizeof bits);
        h = (h ^ bits) * 1099511628211ull;
    }
    char name[96];
    std::snprintf(name, sizeof name, "%s-%016llx.cube",
                  spec.substr(std::strlen(kBuiltinLutPrefix)).c_str(), (unsigned long long)h);
    std::error_code ec;
    const fs::path dir = fs::temp_directory_path(ec) / "spirula-luts";
    fs::create_directories(dir, ec);
    path = (dir / name).string();
    if (fs::is_regular_file(path, ec)) return true;
    // Written beside and renamed, so a second run never reads half a file.
    const std::string tmp = path + ".part" + std::to_string(lut->serial);
    if (!write_cube_lut(*lut, tmp, error)) return false;
    fs::rename(tmp, path, ec);
    if (ec && !fs::is_regular_file(path)) {
        error = "cannot write " + path;
        return false;
    }
    fs::remove(tmp, ec);
    return true;
}

namespace {

struct LutAxis {
    int base;
    float frac;
};

struct CubeSampler {
    const CubeLut& lut;
    const size_t sr = 3, sg, sb;

    explicit CubeSampler(const CubeLut& lut)
        : lut(lut), sg((size_t)lut.size * 3), sb((size_t)lut.size * lut.size * 3) {}

    LutAxis axis(float v, int c) const {
        const float t = (v - lut.domain_min[c]) /
                        (lut.domain_max[c] - lut.domain_min[c]);
        const float x = std::clamp(t, 0.0f, 1.0f) * (float)(lut.size - 1);
        const int i = std::min((int)x, lut.size - 2);
        return {i, x - (float)i};
    }

    void map(LutAxis r, LutAxis g, LutAxis b, uint8_t* out) const {
        const float fr = r.frac, fg = g.frac, fb = b.frac;
        const float* c000 = lut.rgb.data() + r.base * sr + g.base * sg + b.base * sb;
        const float* c111 = c000 + sr + sg + sb;
        // Sorted fractions select one of the cube's six tetrahedra.
        const float *c1, *c2;
        float w0, w1, w2, w3;
        if (fr > fg) {
            if (fg > fb)      { c1 = c000 + sr; c2 = c000 + sr + sg; w0 = 1 - fr; w1 = fr - fg; w2 = fg - fb; w3 = fb; }
            else if (fr > fb) { c1 = c000 + sr; c2 = c000 + sr + sb; w0 = 1 - fr; w1 = fr - fb; w2 = fb - fg; w3 = fg; }
            else              { c1 = c000 + sb; c2 = c000 + sr + sb; w0 = 1 - fb; w1 = fb - fr; w2 = fr - fg; w3 = fg; }
        } else {
            if (fb > fg)      { c1 = c000 + sb; c2 = c000 + sg + sb; w0 = 1 - fb; w1 = fb - fg; w2 = fg - fr; w3 = fr; }
            else if (fb > fr) { c1 = c000 + sg; c2 = c000 + sg + sb; w0 = 1 - fg; w1 = fg - fb; w2 = fb - fr; w3 = fr; }
            else              { c1 = c000 + sg; c2 = c000 + sr + sg; w0 = 1 - fg; w1 = fg - fr; w2 = fr - fb; w3 = fb; }
        }
        for (int c = 0; c < 3; c++) {
            const float v = w0 * c000[c] + w1 * c1[c] + w2 * c2[c] + w3 * c111[c];
            out[c] = (uint8_t)std::lround(std::clamp(v, 0.0f, 1.0f) * 255.0f);
        }
    }
};

template <typename Run>
void map_pixels(size_t pixels, int threads, const Run& run) {
    int t = threads > 0 ? threads : (int)std::max(1u, spirula::hardware_threads());
    t = (int)std::min<size_t>((size_t)t, std::max<size_t>(1, pixels / 65536));
    if (t <= 1) {
        run(0, pixels);
        return;
    }
    std::vector<std::thread> pool;
    const size_t chunk = (pixels + t - 1) / t;
    for (int k = 0; k < t; k++) {
        const size_t a = (size_t)k * chunk, b = std::min(pixels, a + chunk);
        if (a < b) pool.emplace_back(run, a, b);
    }
    for (std::thread& th : pool) th.join();
}

}  // namespace

void apply_cube_lut(const CubeLut& lut, uint8_t* rgb, size_t pixels, int threads) {
    if (lut.empty() || pixels == 0) return;
    const CubeSampler sampler(lut);
    // RGB8 has only 256 positions per axis; RGB16 avoids a 65536-entry table.
    LutAxis axis[3][256];
    for (int c = 0; c < 3; c++)
        for (int v = 0; v < 256; v++) axis[c][v] = sampler.axis(v / 255.0f, c);
    map_pixels(pixels, threads, [&](size_t from, size_t to) {
        for (size_t p = from; p < to; p++) {
            uint8_t* px = rgb + p * 3;
            sampler.map(axis[0][px[0]], axis[1][px[1]], axis[2][px[2]], px);
        }
    });
}

void apply_cube_lut_rgb48be(const CubeLut& lut, const uint8_t* rgb48, uint8_t* rgb8,
                           size_t pixels, int threads) {
    if (lut.empty() || pixels == 0) return;
    const CubeSampler sampler(lut);
    map_pixels(pixels, threads, [&](size_t from, size_t to) {
        for (size_t p = from; p < to; p++) {
            const uint8_t* px = rgb48 + p * 6;
            const auto axis = [&](int c) {
                const unsigned v = (unsigned)px[c * 2] * 256 + px[c * 2 + 1];
                return sampler.axis(v / 65535.0f, c);
            };
            sampler.map(axis(0), axis(1), axis(2), rgb8 + p * 3);
        }
    });
}

std::string ffmpeg_lut3d_filter(const std::string& path) {
    auto escape = [](const std::string& s, const char* special) {
        std::string o;
        for (char ch : s) {
            if (std::strchr(special, ch)) o += '\\';
            o += ch;
        }
        return o;
    };
    std::string p = path;
    std::replace(p.begin(), p.end(), '\\', '/');
    // The option parser first (':' separates options), then the graph parser.
    const std::string value = escape(escape(p, "\\':"), "\\'[],;");
    return "lut3d=file=" + value + ":interp=tetrahedral";
}

}  // namespace app
