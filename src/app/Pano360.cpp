// Pano360.cpp -- see app/Pano360.h.

#include "app/Pano360.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <thread>

namespace app {

namespace {

constexpr double kPi = 3.14159265358979323846;

struct Mat3 {
    float m[9];   // row major
    float operator()(int r, int c) const { return m[3 * r + c]; }
};

Mat3 mul(const Mat3& a, const Mat3& b) {
    Mat3 o{};
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 3; c++) {
            float s = 0.0f;
            for (int k = 0; k < 3; k++) s += a(r, k) * b(k, c);
            o.m[3 * r + c] = s;
        }
    return o;
}

// About the down (y), right (x) and forward (z) axes. Signs are fixed by the
// EAC face table below, which is ffmpeg's: they were checked against
// `v360=eac:flat` over five rotations, agreeing to the resampling floor.
Mat3 rotY(double a) {
    const float c = (float)std::cos(a), s = (float)std::sin(a);
    return {{c, 0, s, 0, 1, 0, -s, 0, c}};
}
Mat3 rotX(double a) {
    const float c = (float)std::cos(a), s = (float)std::sin(a);
    return {{1, 0, 0, 0, c, s, 0, -s, c}};
}
Mat3 rotZ(double a) {
    const float c = (float)std::cos(a), s = (float)std::sin(a);
    return {{c, -s, 0, s, c, 0, 0, 0, 1}};
}

double rad(double deg) { return deg * kPi / 180.0; }

// Positive pitch tilts the view up, as everyone else spells it.
Mat3 orientation(const Pano360Options& o) {
    return mul(mul(rotY(rad(o.yaw)), rotX(-rad(o.pitch))), rotZ(rad(o.roll)));
}

// A direction to its EAC cell and the position within it, in [-1, 1]. The cell
// order is [left front right / down back up] with the bottom row rotated,
// which is what makes both rows continuous across their cells.
void dir_to_cell(float x, float y, float z, int& cell, float& u, float& v) {
    const float ax = std::fabs(x), ay = std::fabs(y), az = std::fabs(z);
    auto safe = [](float d) { return d == 0.0f ? 1e-12f : d; };
    if (ax >= ay && ax >= az) {
        const float d = safe(x);
        u = -z / d;
        v = (x > 0.0f ? y : -y) / d;
        cell = x > 0.0f ? 2 : 0;
    } else if (ay >= az) {
        const float d = safe(y);
        // Both poles are stored turned a quarter: (u, v) -> (v, -u).
        const float a = (y > 0.0f ? x : -x) / d, b = -z / d;
        u = b;
        v = -a;
        cell = y > 0.0f ? 3 : 5;
    } else {
        const float d = safe(z);
        if (z > 0.0f) {
            u = x / d;
            v = y / d;
            cell = 1;
        } else {
            const float a = x / d, b = -y / d;
            u = -b;   // a quarter the other way
            v = a;
            cell = 4;
        }
    }
}

// dir_to_cell run backwards, cell by cell: the face axis is +-1 and the two
// in-cell coordinates are what the table above put where.
void cell_to_dir(int cell, float u, float v, float d[3]) {
    switch (cell) {
        case 0: d[0] = -1; d[1] = v;  d[2] = u;  break;
        case 1: d[0] = u;  d[1] = v;  d[2] = 1;  break;
        case 2: d[0] = 1;  d[1] = v;  d[2] = -u; break;
        case 3: d[0] = -v; d[1] = 1;  d[2] = -u; break;
        case 4: d[0] = -v; d[1] = -u; d[2] = -1; break;
        default: d[0] = -v; d[1] = -1; d[2] = u; break;
    }
    const float n = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    d[0] /= n;
    d[1] /= n;
    d[2] /= n;
}

}  // namespace

namespace {

// A MAX 2 track is a whole panorama, a few rows short of the 2:1 its own pixel
// scale asks for and padded at the sides to fill the frame. Neither follows
// from the frame size, so PMOD states both -- a fourth entry is a MAX 2.
bool sphere_layout(int width, int height, const std::vector<uint32_t>& mode,
                   Pano360Layout& out) {
    if (mode.size() < 4 || mode[1] == 0 || (int)mode[1] >= width) return false;
    const int face = (width - (int)mode[1]) / 4;
    if (face <= 0 || 2 * face - height != (int)mode[0]) return false;
    out.packing = Pano360Packing::Sphere;
    out.track_w = width;
    out.track_h = height;
    out.face = face;
    out.margin = (width - 4 * face) / 2;
    return out.margin > 0;
}

bool eac_layout(int width, int height, Pano360Layout& out) {
    const int strips = width - 3 * height;
    // 64 px in both of a MAX's recording modes. A generous ceiling still
    // rejects every other two-track file: an .insv is two square fisheyes.
    if (strips < 0 || strips % 2 != 0 || strips > 128) return false;
    out.packing = Pano360Packing::Eac;
    out.track_w = width;
    out.track_h = height;
    out.face = height;
    out.strip = strips / 2;
    return true;
}

}  // namespace

bool pano360_detect(int tracks, int width, int height, const Pano360Meta& meta,
                    Pano360Layout& out) {
    out = Pano360Layout{};
    if (tracks != 2 || width <= 0 || height <= 0) return false;
    if (!meta.projection.empty() && meta.projection != "EACO") return false;
    // The cube map is read off the shape, which every MAX mode fits and nothing
    // else does; the panorama pair needs numbers only the camera has.
    if (eac_layout(width, height, out)) return true;
    return sphere_layout(width, height, meta.mode, out);
}

bool pano360_unsupported(int tracks, int width, int height,
                         const Pano360Meta& meta) {
    Pano360Layout l;
    // Only a .360 names a projection at all, so naming one and not being placed
    // is a packing this build has not met -- not an ordinary two-lens file.
    return tracks == 2 && !meta.projection.empty() &&
           !pano360_detect(tracks, width, height, meta, l);
}

std::vector<Eac360Slice> eac360_slices(const Pano360Layout& l) {
    const int half = l.face / 2;
    return {{0, 0, half},
            {half + l.strip, half, 2 * l.face},
            {2 * l.face + half + 2 * l.strip, 2 * l.face + half, half}};
}

namespace {

// Rows the panorama is short of the 2:1 its own scale asks for, per edge.
int sphere_vpad(const Pano360Layout& l) { return (2 * l.face - l.track_h) / 2; }

void sphere_dir(const Pano360Layout& l, float cx, float cy, float dir[3]) {
    const double az = ((double)cx / l.canvasW() - 0.5) * 2 * kPi;
    const double el = ((cy + sphere_vpad(l)) / (2.0 * l.face) - 0.5) * kPi;
    dir[0] = (float)(std::cos(el) * std::sin(az));
    dir[1] = (float)std::sin(el);
    dir[2] = (float)(std::cos(el) * std::cos(az));
}

}  // namespace

bool pano360_direction(const Pano360Layout& l, int row, float x, float y,
                       float dir[3]) {
    if (!l.valid() || row < 0 || row > 1 || y < 0 || y >= (float)l.track_h)
        return false;
    if (l.sphere()) {
        // Only the first track: the second holds the same sphere on its side,
        // and nothing here knows which way round.
        if (row != 0 || x < (float)l.margin ||
            x >= (float)(l.track_w - l.margin))
            return false;
        sphere_dir(l, x - l.margin, y, dir);
        return true;
    }
    int dst_x = -1;
    for (const Eac360Slice& s : eac360_slices(l))
        if (x >= (float)s.src_x && x < (float)(s.src_x + s.width))
            dst_x = (int)(s.dst_x + (x - (float)s.src_x));
    if (dst_x < 0) return false;

    const float face = (float)l.face;
    const int col = std::min(2, (int)(dst_x / l.face));
    const float uf = 2.0f * ((float)dst_x - col * face) / face - 1.0f;
    const float vf = 2.0f * y / face - 1.0f;
    cell_to_dir(row * 3 + col, (float)std::tan(kPi / 4 * uf),
                (float)std::tan(kPi / 4 * vf), dir);
    return true;
}

int pano360_default_size(const Pano360Layout& l, const Pano360Options& o) {
    if (!l.valid()) return 0;
    // Four faces around a panorama, so a pixel spans a source pixel's angle. A
    // rectilinear face would need 4/pi to hold the density at its centre and
    // spends the extra on its corners; 1.125 splits the difference.
    if (o.mode == Pano360Mode::Equirect) return 4 * l.face;
    return 32 * (int)std::lround(l.face * 1.125 / 32.0);
}

std::vector<Pano360View> pano360_views(const Pano360Layout& l,
                                       const Pano360Options& o) {
    std::vector<Pano360View> views;
    if (!l.valid() || o.mode == Pano360Mode::Off) return views;
    const Mat3 base = orientation(o);

    if (o.mode == Pano360Mode::Equirect) {
        Pano360View v;
        v.width = o.size > 0 ? o.size : pano360_default_size(l, o);
        v.height = v.width / 2;
        v.fov = 0.0f;
        std::memcpy(v.rot, base.m, sizeof v.rot);
        views.push_back(std::move(v));
        return views;
    }

    const int side = o.size > 0 ? o.size : pano360_default_size(l, o);
    if (l.sphere()) {
        // A cube stood on a corner: its six faces still cover the sphere, but
        // none is centred on a pole, where a panorama's own rows fan out into a
        // starburst -- and every one of them holds some horizon.
        const double tilt = std::asin(1.0 / std::sqrt(3.0));   // 35.26 degrees
        for (int k = 0; k < 6; k++) {
            const double up = (k % 2) ? -tilt : tilt;
            const double az = rad(60.0 * k);
            // z forward, y down: the axis this looks along, then a level right
            // and the down that follows, so all six agree on which way gravity is.
            const float ez[3] = {(float)(std::cos(up) * std::sin(az)),
                                 (float)-std::sin(up),
                                 (float)(std::cos(up) * std::cos(az))};
            const float n = std::sqrt(ez[0] * ez[0] + ez[2] * ez[2]);
            const float ex[3] = {ez[2] / n, 0.0f, -ez[0] / n};
            const float ey[3] = {ez[1] * ex[2] - ez[2] * ex[1],
                                 ez[2] * ex[0] - ez[0] * ex[2],
                                 ez[0] * ex[1] - ez[1] * ex[0]};
            const Mat3 r{{ex[0], ey[0], ez[0], ex[1], ey[1], ez[1],
                          ex[2], ey[2], ez[2]}};
            Pano360View v;
            v.dir = "cam" + std::to_string(views.size());
            v.width = v.height = side;
            // Wider than the 90 a cube face needs, so neighbours share the
            // features along their edge rather than meeting exactly on it.
            v.fov = 100.0f;
            const Mat3 m = mul(base, r);
            std::memcpy(v.rot, m.m, sizeof v.rot);
            views.push_back(std::move(v));
        }
        return views;
    }
    const double focal = side * 0.5;
    // Half of a side face is 45 of its 90 degrees. Rounded DOWN and to an even
    // count, so a bilinear tap at the outer edge cannot reach past the seam
    // into the other lens.
    const int half = 2 * (int)std::floor(side * std::tan(rad(22.5)) / 2.0);
    const double av = std::atan(half / (double)side);   // half its height / focal
    // A tilted view narrows in azimuth as it nears the seam, so 90 degrees
    // would leave a wedge uncovered between neighbours; this circumscribes the
    // half face exactly, and widening is free -- that axis parallels the seam.
    const int wide = 2 * (int)std::ceil(side / std::cos(av) / 2.0);

    auto push = [&](const Mat3& r, int w, int h) {
        Pano360View v;
        v.dir = "cam" + std::to_string(views.size());
        v.width = w;
        v.height = h;
        v.fov = (float)(2.0 * std::atan(w * 0.5 / focal) * 180.0 / kPi);
        const Mat3 f = mul(base, r);
        std::memcpy(v.rot, f.m, sizeof v.rot);
        views.push_back(std::move(v));
    };
    for (int lens = 0; lens < 2; lens++) {
        // The two lenses look along +z and -z, and the seam between them is the
        // z = 0 plane -- the centre line of the side faces (Pano360Layout).
        const Mat3 axis = lens == 0 ? Mat3{{1, 0, 0, 0, 1, 0, 0, 0, 1}}
                                    : rotY(rad(180.0));
        push(axis, side, side);
        // Tilted so the INNER edge meets the axial face's exactly, leaving the
        // rounding as a sliver at the seam and not a gap where there is scene.
        // Each keeps the sphere's up, so all ten agree on which way gravity is.
        const double tilt = rad(45.0) + av;
        push(mul(axis, rotX(-tilt)), wide, half);   // above the lens axis
        push(mul(axis, rotX(tilt)), wide, half);    // below
        push(mul(axis, rotY(tilt)), half, wide);    // beside, and so portrait
        push(mul(axis, rotY(-tilt)), half, wide);
    }
    return views;
}

void pano360_remap(const Pano360Layout& l, const Pano360View& v,
                   Pano360Remap& out) {
    out.width = v.width;
    out.height = v.height;
    out.x.resize((size_t)v.width * v.height);
    out.y.resize((size_t)v.width * v.height);
    const Mat3 r{{v.rot[0], v.rot[1], v.rot[2], v.rot[3], v.rot[4], v.rot[5],
                  v.rot[6], v.rot[7], v.rot[8]}};
    const float face = (float)l.face;
    const double focal = v.fov > 0.0f
                             ? (v.width * 0.5) / std::tan(rad(v.fov) * 0.5)
                             : 0.0;

    for (int j = 0; j < v.height; j++) {
        for (int i = 0; i < v.width; i++) {
            float dx, dy, dz;
            if (v.fov > 0.0f) {
                dx = (float)(i + 0.5 - v.width * 0.5);
                dy = (float)(j + 0.5 - v.height * 0.5);
                dz = (float)focal;
            } else {
                const double phi = (2.0 * (i + 0.5) / v.width - 1.0) * kPi;
                const double th = (2.0 * (j + 0.5) / v.height - 1.0) * kPi / 2;
                dx = (float)(std::cos(th) * std::sin(phi));
                dy = (float)std::sin(th);
                dz = (float)(std::cos(th) * std::cos(phi));
            }
            const float x = r(0, 0) * dx + r(0, 1) * dy + r(0, 2) * dz;
            const float y = r(1, 0) * dx + r(1, 1) * dy + r(1, 2) * dz;
            const float z = r(2, 0) * dx + r(2, 1) * dy + r(2, 2) * dz;

            if (l.sphere()) {
                const double n = std::sqrt((double)x * x + (double)y * y + (double)z * z);
                const double az = std::atan2((double)x, (double)z);
                const double el = std::asin(std::min(1.0, std::max(-1.0, y / n)));
                const size_t k = (size_t)j * v.width + i;
                // Wrapped in azimuth, clamped in elevation: the panorama is a
                // ring, and the rows the format cut off it are not there.
                out.x[k] = (float)((az / (2 * kPi) + 0.5) * l.canvasW());
                out.y[k] = std::min(
                    std::max((float)((el / kPi + 0.5) * 2 * l.face - sphere_vpad(l)),
                             0.0f),
                    (float)(l.track_h - 1));
                continue;
            }
            int cell;
            float u, w;
            dir_to_cell(x, y, z, cell, u, w);
            const float cu = (float)(2.0 / kPi * std::atan(u) + 0.5);
            const float cv = (float)(2.0 / kPi * std::atan(w) + 0.5);
            const float row = (float)(cell / 3);
            float sx = (cu + (cell % 3)) * face;
            float sy = (cv + row) * face;
            // The two rows are unrelated cube faces, so a bilinear tap must not
            // cross between them; within a row the cells are sphere-adjacent
            // and a tap across is the right pixel.
            sx = std::min(std::max(sx, 0.0f), 3 * face - 1.0f);
            sy = std::min(std::max(sy, row * face), (row + 1) * face - 1.0f);
            const size_t k = (size_t)j * v.width + i;
            out.x[k] = sx;
            out.y[k] = sy;
        }
    }
}

void pano360_apply(const Pano360Remap& m, const uint8_t* canvas, int canvas_w,
                   int canvas_h, int threads, uint8_t* out) {
    if (threads <= 0)
        threads = std::max(1, (int)std::thread::hardware_concurrency());
    threads = std::min(threads, std::max(1, m.height));

    auto rows = [&](int y0, int y1) {
        for (int j = y0; j < y1; j++) {
            for (int i = 0; i < m.width; i++) {
                const size_t k = (size_t)j * m.width + i;
                const float fx = m.x[k], fy = m.y[k];
                const int x0 = std::min((int)fx, canvas_w - 1);
                const int y0i = std::min((int)fy, canvas_h - 1);
                const int x1 = std::min(x0 + 1, canvas_w - 1);
                const int y1i = std::min(y0i + 1, canvas_h - 1);
                const float ax = fx - x0, ay = fy - y0i;
                const uint8_t* p00 = canvas + ((size_t)y0i * canvas_w + x0) * 3;
                const uint8_t* p01 = canvas + ((size_t)y0i * canvas_w + x1) * 3;
                const uint8_t* p10 = canvas + ((size_t)y1i * canvas_w + x0) * 3;
                const uint8_t* p11 = canvas + ((size_t)y1i * canvas_w + x1) * 3;
                uint8_t* d = out + k * 3;
                for (int c = 0; c < 3; c++) {
                    const float top = p00[c] + (p01[c] - p00[c]) * ax;
                    const float bot = p10[c] + (p11[c] - p10[c]) * ax;
                    d[c] = (uint8_t)(top + (bot - top) * ay + 0.5f);
                }
            }
        }
    };

    if (threads == 1) {
        rows(0, m.height);
        return;
    }
    std::vector<std::thread> pool;
    pool.reserve(threads);
    const int span = (m.height + threads - 1) / threads;
    for (int t = 0; t < threads; t++) {
        const int y0 = t * span, y1 = std::min(m.height, y0 + span);
        if (y0 >= y1) break;
        pool.emplace_back(rows, y0, y1);
    }
    for (std::thread& t : pool) t.join();
}

void pano360_canvas(const Pano360Layout& l, const uint8_t* track0,
                    const uint8_t* track1, uint8_t* out) {
    const int cw = l.canvasW();
    if (l.sphere()) {
        for (int y = 0; y < l.track_h; y++)
            std::memcpy(out + (size_t)y * cw * 3,
                        track0 + ((size_t)y * l.track_w + l.margin) * 3,
                        (size_t)cw * 3);
        return;
    }
    const std::vector<Eac360Slice> slices = eac360_slices(l);
    const uint8_t* src[2] = {track0, track1};
    for (int row = 0; row < 2; row++)
        for (const Eac360Slice& s : slices)
            for (int y = 0; y < l.track_h; y++)
                std::memcpy(out + ((size_t)(row * l.face + y) * cw + s.dst_x) * 3,
                            src[row] + ((size_t)y * l.track_w + s.src_x) * 3,
                            (size_t)s.width * 3);
}

int packed_lens_count(int width, int height, bool& exact) {
    exact = false;
    if (width <= 0 || height <= 0) return 1;
    const double r = (double)width / height;
    // Stacked only at exactly 1:2: a portrait phone clip is nearer that than 1:1.
    if (std::fabs(r * 2.0 - 1.0) <= 0.01) {
        exact = true;
        return 2;
    }
    // Nearer in log ratio, so the two meet at sqrt(2).
    const int lenses = r >= std::sqrt(2.0) ? 2 : 1;
    exact = std::fabs(r / lenses - 1.0) <= 0.01;
    return lenses;
}

void packed_lens_crop(const uint8_t* px, int width, int height, int channels,
                      int lenses, int lens, std::vector<uint8_t>& out,
                      int& out_width, int& out_height) {
    lenses = std::max(lenses, 1);
    lens = std::min(std::max(lens, 0), lenses - 1);
    const bool stacked = height > width;
    const int w = stacked ? width : width / lenses;
    const int h = stacked ? height / lenses : height;
    const size_t x0 = stacked ? 0 : (size_t)lens * w;
    const size_t y0 = stacked ? (size_t)lens * h : 0;
    const size_t row = (size_t)w * channels;
    out.resize(row * h);
    for (int y = 0; y < h; y++)
        std::memcpy(out.data() + (size_t)y * row,
                    px + ((y0 + y) * width + x0) * channels, row);
    out_width = w;
    out_height = h;
}

namespace {

uint32_t le32(const uint8_t* p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 |
           (uint32_t)p[3] << 24;
}

// The top-level MP4 box `type`, or empty. Kandao's are a few KB; `limit` keeps
// a damaged size field from asking for the whole file.
std::vector<uint8_t> top_level_box(const std::string& path, const char* type,
                                   size_t limit) {
    std::ifstream f(path, std::ios::binary);
    uint8_t head[16];
    for (uint64_t pos = 0; f.seekg((std::streamoff)pos) &&
                           f.read((char*)head, 8);) {
        uint64_t size = (uint64_t)head[0] << 24 | (uint64_t)head[1] << 16 |
                        (uint64_t)head[2] << 8 | head[3];
        uint64_t header = 8;
        if (size == 1) {
            if (!f.read((char*)head + 8, 8)) break;
            size = 0;
            for (int i = 8; i < 16; i++) size = size << 8 | head[i];
            header = 16;
        }
        // 0 = the last box, running to the end of the file.
        if (size != 0 && size < header) break;
        if (std::memcmp(head + 4, type, 4) == 0) {
            std::vector<uint8_t> body(
                size ? (size_t)std::min<uint64_t>(size - header, limit) : limit);
            f.read((char*)body.data(), (std::streamsize)body.size());
            body.resize((size_t)f.gcount());
            return body;
        }
        if (size == 0) break;
        pos += size;
    }
    return {};
}

}  // namespace

bool kandao_unstitched(const std::string& path) {
    // A count, then per entry: name[32], type[8], element count, the elements.
    // All little endian, unlike the box around them.
    const std::vector<uint8_t> b = top_level_box(path, "kfix", 1 << 16);
    if (b.size() < 4) return false;
    size_t p = 4;
    for (uint32_t n = le32(b.data()); n > 0 && p + 44 <= b.size(); n--) {
        const char* name = (const char*)&b[p];
        const char* type = (const char*)&b[p + 32];
        // CHAR, or a letter and a width in bits: U8, S32, U64.
        const int bits = std::strncmp(type, "CHAR", 8) == 0 ? 8 : std::atoi(type + 1);
        const uint64_t bytes = (uint64_t)le32(&b[p + 40]) * (bits / 8);
        p += 44;
        if (bits < 8 || bits % 8 != 0 || bytes > b.size() - p) return false;
        if (std::strncmp(name, "STITCHED", 32) == 0) return bytes >= 1 && b[p] == 0;
        p += (size_t)bytes;
    }
    return false;
}

std::string pano360_graph(const Pano360Layout& l, const std::string& pre) {
    if (!l.valid()) return std::string();
    char buf[192];
    if (l.sphere()) {
        std::snprintf(buf, sizeof buf, "[0:v:0]crop=%d:%d:%d:0[cv];[cv]",
                      l.canvasW(), l.track_h, l.margin);
        std::string f = buf;
        f += pre.empty() ? "null" : pre;
        f += "[";
        f += pano360_canvas_pad();
        f += "]";
        return f;
    }
    const std::vector<Eac360Slice> slices = eac360_slices(l);
    std::string f;
    for (int track = 0; track < 2; track++) {
        for (size_t s = 0; s < slices.size(); s++) {
            std::snprintf(buf, sizeof buf,
                          "[0:v:%d]crop=%d:%d:%d:0[p%d_%zu];", track,
                          slices[s].width, l.track_h, slices[s].src_x, track, s);
            f += buf;
        }
        for (size_t s = 0; s < slices.size(); s++) {
            std::snprintf(buf, sizeof buf, "[p%d_%zu]", track, s);
            f += buf;
        }
        std::snprintf(buf, sizeof buf, "hstack=inputs=%zu[row%d];",
                      slices.size(), track);
        f += buf;
    }
    f += "[row0][row1]vstack=inputs=2[cv];[cv]";
    // A chain has to hold at least one filter, and null is the cheapest.
    f += pre.empty() ? "null" : pre;
    f += "[";
    f += pano360_canvas_pad();
    f += "]";
    return f;
}

}  // namespace app
