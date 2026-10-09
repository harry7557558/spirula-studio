// CubeLut.cpp -- see CubeLut.h.

#include "app/CubeLut.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstring>
#include <fstream>
#include <map>
#include <mutex>
#include <thread>

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
        const std::from_chars_result r = std::from_chars(p, end, out[k]);
        if (r.ec != std::errc()) break;
        p = r.ptr;
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

std::shared_ptr<const CubeLut> cube_lut_cached(const std::string& path,
                                               std::string& error) {
    static std::mutex mu;
    static std::map<std::string, std::shared_ptr<const CubeLut>> cache;
    std::lock_guard<std::mutex> lock(mu);
    auto it = cache.find(path);
    if (it != cache.end()) return it->second;
    auto lut = std::make_shared<CubeLut>();
    if (!load_cube_lut(path, *lut, error)) return nullptr;
    cache[path] = lut;
    return lut;
}

void apply_cube_lut(const CubeLut& lut, uint8_t* rgb, size_t pixels, int threads) {
    if (lut.empty() || pixels == 0) return;
    const int n = lut.size;
    // An 8-bit input has 256 positions per axis, so the cell and the weight
    // inside it are looked up rather than computed per pixel.
    int base[3][256];
    float frac[3][256];
    for (int c = 0; c < 3; c++)
        for (int v = 0; v < 256; v++) {
            const float t = (v / 255.0f - lut.domain_min[c]) /
                            (lut.domain_max[c] - lut.domain_min[c]);
            const float x = std::clamp(t, 0.0f, 1.0f) * (float)(n - 1);
            const int i = std::min((int)x, n - 2);
            base[c][v] = i;
            frac[c][v] = x - (float)i;
        }
    const float* L = lut.rgb.data();
    const size_t sr = 3, sg = (size_t)n * 3, sb = (size_t)n * n * 3;

    auto run = [&](size_t from, size_t to) {
        for (size_t p = from; p < to; p++) {
            uint8_t* px = rgb + p * 3;
            const float fr = frac[0][px[0]], fg = frac[1][px[1]], fb = frac[2][px[2]];
            const float* c000 = L + base[0][px[0]] * sr + base[1][px[1]] * sg +
                                base[2][px[2]] * sb;
            const float* c111 = c000 + sr + sg + sb;
            // Which of the cube's six tetrahedra holds the point decides the two
            // corners between c000 and c111; the weights are the sorted fractions.
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
                px[c] = (uint8_t)std::lround(std::clamp(v, 0.0f, 1.0f) * 255.0f);
            }
        }
    };
    int t = threads > 0 ? threads : (int)std::max(1u, std::thread::hardware_concurrency());
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
