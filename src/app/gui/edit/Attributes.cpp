// Attributes.cpp -- see Attributes.h.

#include "app/gui/edit/Attributes.h"

#include "app/gui/edit/EditDoc.h"
#include "app/gui/edit/ElementGrid.h"
#include "checkpoint/SplatPly.h"
#include "data/CameraMath.h"
#include "data/DatasetParser.h"
#include "data/SparseEdit.h"
#include "mesh/MeshExport.h"
#include "i18n/catalog/EditAttributes.h"

#include <cstddef>
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <numeric>
#include <string>

namespace msg = spirula::i18n::msg::attr;

namespace gui {

namespace {

constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();
// The rasterizer's alpha cut (core/Common.cuh ALPHA_THRESHOLD), which is what
// decides how far from its centre a Gaussian is still drawn.
constexpr float kAlphaCut = 1.0f / 255.0f;

const AttrInfo kTable[(int)Attr::Count] = {
    {Attr::PosX, &msg::a_pos_x, &msg::a_pos_help, false, AttrTint::Red},
    {Attr::PosY, &msg::a_pos_y, &msg::a_pos_help, false, AttrTint::Green},
    {Attr::PosZ, &msg::a_pos_z, &msg::a_pos_help, false, AttrTint::Blue},
    {Attr::Opacity, &msg::a_opacity, &msg::a_opacity_help, false, AttrTint::None, 0.0, 1.0},
    {Attr::ScaleMax, &msg::a_scale_max, &msg::a_scale_help, true, AttrTint::None},
    {Attr::ScaleMin, &msg::a_scale_min, &msg::a_scale_help, true, AttrTint::None},
    {Attr::ScaleMean, &msg::a_scale_mean, &msg::a_scale_help, true, AttrTint::None},
    {Attr::ExtentMax, &msg::a_extent_max, &msg::a_extent_help, true, AttrTint::None},
    {Attr::ExtentMin, &msg::a_extent_min, &msg::a_extent_help, true, AttrTint::None},
    {Attr::ExtentMean, &msg::a_extent_mean, &msg::a_extent_help, true, AttrTint::None},
    {Attr::AnisoRatio, &msg::a_aniso_ratio, &msg::a_aniso_ratio_help, true, AttrTint::None},
    {Attr::Erank, &msg::a_erank, &msg::a_erank_help, false, AttrTint::None, 1.0, 3.0},
    {Attr::Red, &msg::a_red, &msg::a_colour_help, false, AttrTint::Red},
    {Attr::Green, &msg::a_green, &msg::a_colour_help, false, AttrTint::Green},
    {Attr::Blue, &msg::a_blue, &msg::a_colour_help, false, AttrTint::Blue},
    {Attr::Luma, &msg::a_luma, &msg::a_luma_help, false, AttrTint::Gray},
    {Attr::ChromaU, &msg::a_chroma_u, &msg::a_chroma_help, false, AttrTint::BlueYellow},
    {Attr::ChromaV, &msg::a_chroma_v, &msg::a_chroma_help, false, AttrTint::RedCyan},
    {Attr::Hue, &msg::a_hue, &msg::a_hue_help, false, AttrTint::Hue, 0.0, 360.0, false, true},
    {Attr::Saturation, &msg::a_saturation, &msg::a_saturation_help, false,
     AttrTint::Saturation},
    {Attr::CameraDistance, &msg::a_camera_distance, &msg::a_camera_distance_help, true,
     AttrTint::None},
    {Attr::OriginDistance, &msg::a_origin_distance, &msg::a_origin_distance_help, false, AttrTint::None},
    {Attr::Knn4, &msg::a_knn4, &msg::a_knn_help, true, AttrTint::None},
    {Attr::Knn16, &msg::a_knn16, &msg::a_knn_help, true, AttrTint::None},
    {Attr::Knn64, &msg::a_knn64, &msg::a_knn_help, true, AttrTint::None},
    {Attr::FaceAngleMin, &msg::a_face_angle_min, &msg::a_face_angle_help, false, AttrTint::None, 0.0, 60.0},
    {Attr::FaceAngleMax, &msg::a_face_angle_max, &msg::a_face_angle_help, false, AttrTint::None, 60.0, 180.0},
    {Attr::DihedralMax, &msg::a_dihedral, &msg::a_dihedral_help, false, AttrTint::None, 0.0, 180.0},
    {Attr::Valence, &msg::a_valence, &msg::a_valence_help, false, AttrTint::None, 0.0, 0.0, true},
    {Attr::EdgeFacesMin, &msg::a_edge_faces_min, &msg::a_edge_faces_help, false, AttrTint::None, 0.0, 0.0, true},
    {Attr::EdgeFacesMax, &msg::a_edge_faces_max, &msg::a_edge_faces_help, false, AttrTint::None, 0.0, 0.0, true},
    {Attr::FaceArea, &msg::a_face_area, &msg::a_face_area_help, true, AttrTint::None},
    {Attr::EdgeLength, &msg::a_edge_length, &msg::a_edge_length_help, true, AttrTint::None},
    {Attr::PieceSize, &msg::a_piece_size, &msg::a_piece_size_help, true, AttrTint::None},
    {Attr::TrackLength, &msg::a_track_length, &msg::a_track_length_help, false, AttrTint::None, 0.0, 0.0, true},
    {Attr::ReprojError, &msg::a_reproj_error, &msg::a_reproj_error_help, false, AttrTint::None},
    {Attr::TriangulationAngle, &msg::a_tri_angle, &msg::a_tri_angle_help, false, AttrTint::None},
    {Attr::InViewCount, &msg::a_in_view, &msg::a_in_view_help, false, AttrTint::None, 0.0, 0.0, true},
    {Attr::FocalLength, &msg::a_focal, &msg::a_focal_help, false, AttrTint::None},
    {Attr::FieldOfView, &msg::a_fov, &msg::a_fov_help, false, AttrTint::None},
    {Attr::Elevation, &msg::a_elevation, &msg::a_elevation_help, false, AttrTint::None, -90.0, 90.0},
    {Attr::Roll, &msg::a_roll, &msg::a_roll_help, false, AttrTint::None, -180.0, 180.0,
     false, true},
    {Attr::Heading, &msg::a_heading, &msg::a_heading_help, false, AttrTint::None, 0.0, 360.0,
     false, true},
    {Attr::LookAway, &msg::a_look_away, &msg::a_look_away_help, false, AttrTint::None, 0.0, 180.0},
    {Attr::NeighbourDistance, &msg::a_cam_neighbour, &msg::a_cam_neighbour_help, true, AttrTint::None},
    {Attr::PointsSeen, &msg::a_points_seen, &msg::a_points_seen_help, false, AttrTint::None, 0.0, 0.0, true},
};

constexpr double kDeg = 180.0 / 3.14159265358979323846;

double angle_deg(const double a[3], const double b[3]) {
    const double la = std::sqrt(a[0]*a[0] + a[1]*a[1] + a[2]*a[2]);
    const double lb = std::sqrt(b[0]*b[0] + b[1]*b[1] + b[2]*b[2]);
    if (!(la > 0) || !(lb > 0)) return 0.0;
    const double c = (a[0]*b[0] + a[1]*b[1] + a[2]*b[2]) / (la * lb);
    return std::acos(std::clamp(c, -1.0, 1.0)) * kDeg;
}

std::string leaf(const std::string& path) {
    const size_t at = path.find_last_of("/\\");
    return at == std::string::npos ? path : path.substr(at + 1);
}

bool is_colour(Attr a) { return a >= Attr::Red && a <= Attr::Saturation; }
bool is_shape(Attr a) { return a >= Attr::Opacity && a <= Attr::Erank; }

float colour_scalar(Attr a, const float* c) {
    const float r = c[0], g = c[1], b = c[2];
    // BT.709 luma and the colour differences that go with it.
    const float y = 0.2126f * r + 0.7152f * g + 0.0722f * b;
    switch (a) {
        case Attr::Red:   return r;
        case Attr::Green: return g;
        case Attr::Blue:  return b;
        case Attr::Luma:  return y;
        case Attr::ChromaU: return (b - y) / 1.8556f;
        case Attr::ChromaV: return (r - y) / 1.5748f;
        default: break;
    }
    const float mx = std::max(r, std::max(g, b)), mn = std::min(r, std::min(g, b));
    const float d = mx - mn;
    if (a == Attr::Saturation) return mx > 1e-6f ? d / mx : 0.0f;
    // Hue in degrees. A grey has none, and saying 0 would file every grey
    // under red.
    if (d < 1e-4f * std::max(mx, 1e-3f)) return kNaN;
    float h = mx == r ? (g - b) / d : mx == g ? 2.0f + (b - r) / d : 4.0f + (r - g) / d;
    h *= 60.0f;
    return h < 0 ? h + 360.0f : h;
}

void srgb_to_oklab(const float* c, float w_l, float out[3]) {
    float lin[3];
    for (int k = 0; k < 3; k++) {
        const float a = std::fabs(c[k]);
        const float v = a <= 0.04045f ? a / 12.92f : std::pow((a + 0.055f) / 1.055f, 2.4f);
        lin[k] = c[k] < 0 ? -v : v;
    }
    const float l = std::cbrt(0.4122214708f*lin[0] + 0.5363325363f*lin[1] + 0.0514459929f*lin[2]);
    const float m = std::cbrt(0.2119034982f*lin[0] + 0.6806995451f*lin[1] + 0.1073969566f*lin[2]);
    const float s = std::cbrt(0.0883024619f*lin[0] + 0.2817188376f*lin[1] + 0.6299787005f*lin[2]);
    out[0] = w_l * (0.2104542553f*l + 0.7936177850f*m - 0.0040720468f*s);
    out[1] = 1.9779984951f*l - 2.4285922050f*m + 0.4505937099f*s;
    out[2] = 0.0259040371f*l + 0.7827717662f*m - 0.8086757660f*s;
}

// ---------------------------------------------------------------------------
// A mesh vertex, by what is around it
// ---------------------------------------------------------------------------

// The atlas splits a vertex along every seam, and a seam copy on its own has
// half a fan of faces and an open edge. Every count here is over the WELDED
// vertex: copies share a position exactly, so sorting by position finds them.
std::vector<int32_t> weld_map(const meshing::MeshData& m) {
    const size_t n = m.V.size();
    std::vector<int32_t> order(n), weld(n);
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&m](int32_t a, int32_t b) {
        return m.V[(size_t)a] < m.V[(size_t)b];
    });
    for (size_t i = 0; i < n; i++)
        weld[(size_t)order[i]] =
            i && m.V[(size_t)order[i]] == m.V[(size_t)order[i - 1]]
                ? weld[(size_t)order[i - 1]] : order[i];
    return weld;
}

bool mesh_values(const EditDoc& doc, Attr a, std::vector<float>& out) {
    const meshing::MeshData* mp = doc.mesh();
    if (!mp || (int64_t)mp->V.size() != doc.count()) return false;
    const meshing::MeshData& m = *mp;
    const uint8_t* alive = doc.alive();
    const size_t n = m.V.size();
    const float unit = (float)doc.file_placement().s;

    if (a == Attr::PieceSize) {
        int64_t pairs = 0;
        const int32_t* topo = doc.topology(pairs);
        if (!topo) return false;
        std::vector<int32_t> label;
        std::vector<int64_t> sizes;
        components_from_pairs(topo, pairs, alive, (int64_t)n, label, sizes);
        for (size_t i = 0; i < n; i++)
            if (label[i] >= 0) out[i] = (float)sizes[(size_t)label[i]];
        return true;
    }

    const std::vector<int32_t> weld = weld_map(m);
    // Accumulated per welded vertex, handed back to every copy at the end.
    std::vector<float> acc(n, 0.0f), cnt(n, 0.0f);
    const bool want_min = a == Attr::FaceAngleMin || a == Attr::EdgeFacesMin;
    if (want_min) std::fill(acc.begin(), acc.end(), std::numeric_limits<float>::max());
    auto put = [&](int32_t v, float x) {
        const size_t w = (size_t)weld[(size_t)v];
        if (want_min) acc[w] = std::min(acc[w], x);
        else if (a == Attr::FaceArea || a == Attr::EdgeLength) acc[w] += x;
        else acc[w] = std::max(acc[w], x);
        cnt[w] += 1.0f;
    };
    auto live = [&](const std::array<int, 3>& f) {
        return alive[f[0]] && alive[f[1]] && alive[f[2]];
    };

    const bool by_edge = a == Attr::DihedralMax || a == Attr::EdgeFacesMin ||
                         a == Attr::EdgeFacesMax;
    if (!by_edge) {
        for (const auto& f : m.F) {
            if (!live(f)) continue;
            double e[3][3], len[3];
            for (int k = 0; k < 3; k++) {
                const auto& p = m.V[(size_t)f[k]];
                const auto& q = m.V[(size_t)f[(k + 1) % 3]];
                for (int d = 0; d < 3; d++) e[k][d] = (double)q[d] - p[d];
                len[k] = std::sqrt(e[k][0]*e[k][0] + e[k][1]*e[k][1] + e[k][2]*e[k][2]);
            }
            if (a == Attr::Valence) {
                for (int k = 0; k < 3; k++) put(f[k], cnt[(size_t)weld[(size_t)f[k]]] + 1.0f);
            } else if (a == Attr::EdgeLength) {
                for (int k = 0; k < 3; k++) {
                    put(f[k], (float)len[k] * unit);
                    put(f[(k + 1) % 3], (float)len[k] * unit);
                }
            } else if (a == Attr::FaceArea) {
                const double cx = e[0][1]*e[1][2] - e[0][2]*e[1][1];
                const double cy = e[0][2]*e[1][0] - e[0][0]*e[1][2];
                const double cz = e[0][0]*e[1][1] - e[0][1]*e[1][0];
                const float area = (float)(0.5 * std::sqrt(cx*cx + cy*cy + cz*cz)) * unit * unit;
                for (int k = 0; k < 3; k++) put(f[k], area);
            } else {
                // The face's own worst corner, charged to all three: a sliver
                // is unhealthy whichever of its vertices is asked.
                double lo = 180.0, hi = 0.0;
                for (int k = 0; k < 3; k++) {
                    const double back[3] = {-e[(k + 2) % 3][0], -e[(k + 2) % 3][1],
                                            -e[(k + 2) % 3][2]};
                    const double ang = angle_deg(e[k], back);
                    lo = std::min(lo, ang);
                    hi = std::max(hi, ang);
                }
                for (int k = 0; k < 3; k++)
                    put(f[k], (float)(a == Attr::FaceAngleMin ? lo : hi));
            }
        }
    } else {
        // Every face edge under its welded endpoints, sorted so that the
        // faces sharing an edge come out next to each other.
        struct Half { uint64_t key; int32_t face; bool swapped; };
        std::vector<Half> halves;
        halves.reserve(m.F.size() * 3);
        for (size_t fi = 0; fi < m.F.size(); fi++) {
            const auto& f = m.F[fi];
            if (!live(f)) continue;
            for (int k = 0; k < 3; k++) {
                uint64_t u = (uint64_t)(uint32_t)weld[(size_t)f[k]];
                uint64_t v = (uint64_t)(uint32_t)weld[(size_t)f[(k + 1) % 3]];
                const bool swapped = u > v;
                if (swapped) std::swap(u, v);
                halves.push_back({(u << 32) | v, (int32_t)fi, swapped});
            }
        }
        std::sort(halves.begin(), halves.end(),
                  [](const Half& x, const Half& y) { return x.key < y.key; });
        auto normal = [&](int32_t fi, double nrm[3]) {
            const auto& f = m.F[(size_t)fi];
            const auto& p = m.V[(size_t)f[0]];
            const auto& q = m.V[(size_t)f[1]];
            const auto& r = m.V[(size_t)f[2]];
            const double u[3] = {(double)q[0]-p[0], (double)q[1]-p[1], (double)q[2]-p[2]};
            const double v[3] = {(double)r[0]-p[0], (double)r[1]-p[1], (double)r[2]-p[2]};
            nrm[0] = u[1]*v[2] - u[2]*v[1];
            nrm[1] = u[2]*v[0] - u[0]*v[2];
            nrm[2] = u[0]*v[1] - u[1]*v[0];
        };
        for (size_t i = 0; i < halves.size();) {
            size_t j = i;
            while (j < halves.size() && halves[j].key == halves[i].key) j++;
            const int32_t u = (int32_t)(halves[i].key >> 32);
            const int32_t v = (int32_t)(halves[i].key & 0xffffffffu);
            float x = (float)(j - i);
            if (a == Attr::DihedralMax) {
                // 0 where two faces lie flat against each other, 180 where
                // one is folded back onto the other. An open or a crowded
                // edge has no dihedral and says nothing.
                x = 0.0f;
                if (j - i == 2) {
                    double n0[3], n1[3];
                    normal(halves[i].face, n0);
                    normal(halves[i + 1].face, n1);
                    // Two faces wound the same way run the edge in opposite
                    // directions; where they do not, one normal is inside out.
                    if (halves[i].swapped == halves[i + 1].swapped)
                        for (double& c : n1) c = -c;
                    x = (float)angle_deg(n0, n1);
                }
            }
            put(u, x);
            put(v, x);
            i = j;
        }
    }

    for (size_t i = 0; i < n; i++) {
        if (!alive[i]) continue;
        const size_t w = (size_t)weld[i];
        if (!(cnt[w] > 0.0f)) continue;
        out[i] = a == Attr::FaceArea || a == Attr::EdgeLength ? acc[w] / cnt[w] : acc[w];
    }
    return true;
}


// ---------------------------------------------------------------------------
// A reconstruction's points and cameras
// ---------------------------------------------------------------------------

// Camera index by the leaf of its image name, which is the part the parser's
// absolute path and the model's relative one agree on.
std::vector<int32_t> camera_of_image(const ParsedDataset& ds,
                                     const spirula::SparseStats& st) {
    std::map<std::string, int32_t> by_leaf;
    for (size_t i = 0; i < ds.image_filenames.size(); i++)
        by_leaf[leaf(ds.image_filenames[i])] = (int32_t)i;
    std::vector<int32_t> out(st.image_names.size(), -1);
    for (size_t i = 0; i < st.image_names.size(); i++) {
        const auto it = by_leaf.find(leaf(st.image_names[i]));
        if (it != by_leaf.end()) out[i] = it->second;
    }
    return out;
}

bool point_values(const EditDoc& doc, Attr a, std::vector<float>& out,
                  const std::atomic<bool>* cancel) {
    const ParsedDataset* ds = doc.dataset();
    if (!ds || doc.layer() != 0) return false;
    const int64_t n = doc.count();
    const std::vector<uint8_t>& cam_alive =
        doc.layer_count() > 1 ? doc.alive_of(1) : std::vector<uint8_t>();

    if (a == Attr::InViewCount) {
        const int64_t nc = ds->num_cameras;
        std::vector<camhost::Camera> cams((size_t)nc);
        for (int64_t c = 0; c < nc; c++) {
            camhost::Camera& k = cams[(size_t)c];
            k.model = ds->camera_models[(size_t)c];
            k.width = ds->widths[(size_t)c];
            k.height = ds->heights[(size_t)c];
            k.fx = ds->intrins[(size_t)c*4];     k.fy = ds->intrins[(size_t)c*4+1];
            k.cx = ds->intrins[(size_t)c*4+2];   k.cy = ds->intrins[(size_t)c*4+3];
        }
#pragma omp parallel for schedule(dynamic, 256)
        for (int64_t i = 0; i < n; i++) {
            if (cancel && cancel->load(std::memory_order_relaxed)) continue;
            const double* p = &ds->points.xyz[(size_t)i * 3];
            int seen = 0;
            for (int64_t c = 0; c < nc; c++) {
                if ((size_t)c < cam_alive.size() && !cam_alive[(size_t)c]) continue;
                const float* m = &ds->c2w[(size_t)c * 12];
                const double d[3] = {p[0] - m[3], p[1] - m[7], p[2] - m[11]};
                // OpenGL camera-to-world: the camera looks down its own -z,
                // and the lens maths wants +z forward and +y down.
                const double ray[3] = {
                    m[0]*d[0] + m[4]*d[1] + m[8]*d[2],
                    -(m[1]*d[0] + m[5]*d[1] + m[9]*d[2]),
                    -(m[2]*d[0] + m[6]*d[1] + m[10]*d[2])};
                double px[2];
                if (camhost::ray_in_frame(cams[(size_t)c], ray, px)) seen++;
            }
            out[(size_t)i] = (float)seen;
        }
        return true;
    }

    const spirula::SparseStats* st = doc.sparse_stats();
    if (!st || (int64_t)st->track_beg.size() != n + 1) return false;
    if (a == Attr::ReprojError) {
        for (int64_t i = 0; i < n; i++) out[(size_t)i] = st->error[(size_t)i];
        return true;
    }
    const std::vector<int32_t> cam_of = camera_of_image(*ds, *st);
#pragma omp parallel for schedule(dynamic, 1024)
    for (int64_t i = 0; i < n; i++) {
        const int64_t t0 = st->track_beg[(size_t)i], t1 = st->track_beg[(size_t)i + 1];
        // Only cameras still in the reconstruction count as having seen it.
        int32_t seen[32];
        int m = 0, total = 0;
        for (int64_t t = t0; t < t1; t++) {
            const int32_t c = cam_of[(size_t)st->track_image[(size_t)t]];
            if (c < 0 || ((size_t)c < cam_alive.size() && !cam_alive[(size_t)c])) continue;
            total++;
            if (m < 32) seen[m++] = c;
        }
        if (a == Attr::TrackLength) {
            out[(size_t)i] = (float)total;
            continue;
        }
        // The widest angle any two of its cameras subtend at the point: what
        // decides how well its depth is known. A long track from one spot is
        // still a point at the end of a needle.
        const double* p = &ds->points.xyz[(size_t)i * 3];
        double widest = 0.0;
        for (int x = 0; x < m; x++)
            for (int y = x + 1; y < m; y++) {
                const float* ca = &ds->c2w[(size_t)seen[x] * 12];
                const float* cb = &ds->c2w[(size_t)seen[y] * 12];
                const double ra[3] = {ca[3] - p[0], ca[7] - p[1], ca[11] - p[2]};
                const double rb[3] = {cb[3] - p[0], cb[7] - p[1], cb[11] - p[2]};
                widest = std::max(widest, angle_deg(ra, rb));
            }
        out[(size_t)i] = (float)widest;
    }
    return true;
}

bool camera_values(const EditDoc& doc, Attr a, std::vector<float>& out) {
    const ParsedDataset* ds = doc.dataset();
    if (!ds || doc.layer() != 1 || ds->num_cameras != doc.count()) return false;
    const int64_t n = doc.count();
    const uint8_t* alive = doc.alive();
    const spirula::Sim3 moved = doc.file_placement();

    if (a == Attr::PointsSeen) {
        const spirula::SparseStats* st = doc.sparse_stats();
        if (!st) return false;
        const std::vector<int32_t> cam_of = camera_of_image(*ds, *st);
        for (size_t i = 0; i < cam_of.size(); i++)
            if (cam_of[i] >= 0) out[(size_t)cam_of[i]] = (float)st->image_points[i];
        return true;
    }
    if (a == Attr::NeighbourDistance) {
        const float* p = doc.positions();
        const float unit = (float)(doc.placement().s / doc.view_frame().s);
        for (int64_t i = 0; i < n; i++) {
            if (!alive[i]) continue;
            float best = std::numeric_limits<float>::max();
            for (int64_t j = 0; j < n; j++) {
                if (j == i || !alive[j]) continue;
                const float x = p[j*3]-p[i*3], y = p[j*3+1]-p[i*3+1], z = p[j*3+2]-p[i*3+2];
                best = std::min(best, x*x + y*y + z*z);
            }
            if (best < std::numeric_limits<float>::max()) out[(size_t)i] = std::sqrt(best) * unit;
        }
        return true;
    }

    // Where the scene is, for "is this camera even looking at it": the live
    // points' median, in the frame the poses are in. A mean would follow the
    // very floaters this attribute is for finding the cameras of.
    double centre[3] = {0, 0, 0};
    if (a == Attr::LookAway) {
        const std::vector<uint8_t>& pa = doc.alive_of(0);
        const int64_t np = ds->points.num();
        const int64_t step = std::max<int64_t>(1, np / 200000);
        std::vector<double> axis;
        for (int d = 0; d < 3; d++) {
            axis.clear();
            for (int64_t i = 0; i < np; i += step)
                if ((size_t)i < pa.size() && pa[(size_t)i])
                    axis.push_back(ds->points.xyz[(size_t)i * 3 + d]);
            if (axis.empty()) return false;
            std::nth_element(axis.begin(), axis.begin() + axis.size() / 2, axis.end());
            centre[d] = axis[axis.size() / 2];
        }
    }
    for (int64_t i = 0; i < n; i++) {
        const float* m = &ds->c2w[(size_t)i * 12];
        const double fwd[3] = {-m[2], -m[6], -m[10]};
        const double right[3] = {m[0], m[4], m[8]};
        const double up[3] = {m[1], m[5], m[9]};
        double f[3], r[3], u[3];
        moved.rotate(fwd, f);
        moved.rotate(right, r);
        moved.rotate(up, u);
        const double w = ds->widths[(size_t)i], fx = ds->intrins[(size_t)i * 4];
        float v = kNaN;
        switch (a) {
            case Attr::FocalLength: v = (float)fx; break;
            case Attr::FieldOfView: {
                const int model = ds->camera_models[(size_t)i];
                const double half = 0.5 * w / std::max(fx, 1e-9);
                const double fov = model == 1 ? 2.0 * half
                                 : model == 2 ? 4.0 * std::asin(std::min(0.5 * half, 1.0))
                                 : model == 3 ? 2.0 * 3.14159265358979
                                              : 2.0 * std::atan(half);
                v = (float)(fov * kDeg);
                break;
            }
            case Attr::Elevation:
                v = (float)(std::asin(std::clamp(f[2], -1.0, 1.0)) * kDeg);
                break;
            case Attr::Heading: {
                double h = std::atan2(f[1], f[0]) * kDeg;
                if (h < 0) h += 360.0;
                // Straight up or down has no heading to speak of.
                if (std::hypot(f[0], f[1]) > 0.05) v = (float)h;
                break;
            }
            case Attr::Roll:
                // How far the image's own horizon is from level; looking
                // along the vertical there is no horizon to be off.
                if (std::hypot(r[2], u[2]) > 0.05)
                    v = (float)(std::atan2(r[2], u[2]) * kDeg);
                break;
            default: {
                const double to[3] = {centre[0] - m[3], centre[1] - m[7], centre[2] - m[11]};
                v = (float)angle_deg(fwd, to);
            }
        }
        out[(size_t)i] = v;
    }
    return true;
}

}  // namespace


const AttrInfo& attr_info(Attr a) { return kTable[(int)a]; }

bool attr_is_slow(Attr a) {
    switch (a) {
        case Attr::Knn4: case Attr::Knn16: case Attr::Knn64:
        case Attr::CameraDistance: case Attr::InViewCount:
        case Attr::TrackLength: case Attr::ReprojError:
        case Attr::TriangulationAngle: case Attr::PointsSeen:
        case Attr::FaceAngleMin: case Attr::FaceAngleMax: case Attr::DihedralMax:
        case Attr::Valence: case Attr::EdgeFacesMin: case Attr::EdgeFacesMax:
        case Attr::FaceArea: case Attr::EdgeLength: case Attr::PieceSize:
            return true;
        default:
            return false;
    }
}

bool attr_follows_alive(Attr a) {
    return (a >= Attr::Knn4 && a <= Attr::PieceSize) || a == Attr::CameraDistance ||
           a == Attr::InViewCount || a == Attr::NeighbourDistance;
}

bool attr_follows_placement(Attr a) {
    switch (a) {
        case Attr::Opacity: case Attr::AnisoRatio: case Attr::Erank:
        case Attr::Red: case Attr::Green: case Attr::Blue: case Attr::Luma:
        case Attr::ChromaU: case Attr::ChromaV: case Attr::Hue:
        case Attr::Saturation: case Attr::FaceAngleMin: case Attr::FaceAngleMax:
        case Attr::DihedralMax: case Attr::Valence: case Attr::EdgeFacesMin:
        case Attr::EdgeFacesMax: case Attr::PieceSize: case Attr::TrackLength:
        case Attr::ReprojError: case Attr::TriangulationAngle:
        case Attr::InViewCount: case Attr::FocalLength: case Attr::FieldOfView:
        case Attr::LookAway: case Attr::PointsSeen:
            return false;
        default:
            return true;
    }
}

unsigned attr_tint_colour(AttrTint tint, double v, float frac) {
    auto rgb = [](double r, double g, double b) -> unsigned {
        auto u8 = [](double x) { return (unsigned)std::clamp(x * 255.0, 0.0, 255.0); };
        return 0xff000000u | (u8(b) << 16) | (u8(g) << 8) | u8(r);
    };
    // A colour difference, shown as the colour it is a difference TOWARD:
    // grey in the middle, the two opponents at +-0.3, which is about as far as
    // anything but a saturated primary gets.
    auto opponent = [&](const double lo[3], const double hi[3]) {
        const double t = std::clamp(v / 0.3, -1.0, 1.0);
        const double* to = t < 0 ? lo : hi;
        const double k = std::fabs(t);
        return rgb(0.5 + (to[0] - 0.5) * k, 0.5 + (to[1] - 0.5) * k,
                   0.5 + (to[2] - 0.5) * k);
    };
    switch (tint) {
        case AttrTint::Red:   return rgb(0.78, 0.35, 0.39);
        case AttrTint::Green: return rgb(0.43, 0.71, 0.31);
        case AttrTint::Blue:  return rgb(0.31, 0.55, 0.86);
        case AttrTint::Gray: {
            const double g = 0.24 + 0.7 * frac;
            return rgb(g, g, g);
        }
        case AttrTint::Hue: {
            const double h = (double)frac * 6.0, x = 1.0 - std::fabs(std::fmod(h, 2.0) - 1.0);
            const double c[6][3] = {{1,x,0},{x,1,0},{0,1,x},{0,x,1},{x,0,1},{1,0,x}};
            const int i = std::clamp((int)h, 0, 5);
            return rgb(0.2 + 0.7 * c[i][0], 0.2 + 0.7 * c[i][1], 0.2 + 0.7 * c[i][2]);
        }
        case AttrTint::Saturation: {
            // No hue to show it in, so red stands in for one.
            const double s = std::clamp(v, 0.0, 1.0);
            return rgb(0.9, 0.9 * (1.0 - s), 0.9 * (1.0 - s));
        }
        case AttrTint::BlueYellow: {
            const double yellow[3] = {0.95, 0.85, 0.15}, blue[3] = {0.2, 0.4, 1.0};
            return opponent(yellow, blue);
        }
        case AttrTint::RedCyan: {
            const double cyan[3] = {0.1, 0.85, 0.9}, red[3] = {1.0, 0.25, 0.2};
            return opponent(cyan, red);
        }
        default: return rgb(0.51, 0.55, 0.61);
    }
}

// ---------------------------------------------------------------------------
// Two colour attributes, read as one colour
// ---------------------------------------------------------------------------

namespace {

unsigned pack_rgb(const double c[3]) {
    auto u8 = [](double x) { return (unsigned)std::clamp(x * 255.0, 0.0, 255.0); };
    return 0xff000000u | (u8(c[2]) << 16) | (u8(c[1]) << 8) | u8(c[0]);
}

void hsv_rgb(double h_deg, double s, double v, double out[3]) {
    const double h = std::fmod(std::fmod(h_deg, 360.0) + 360.0, 360.0) / 60.0;
    const double x = 1.0 - std::fabs(std::fmod(h, 2.0) - 1.0);
    const double c[6][3] = {{1,x,0},{x,1,0},{0,1,x},{0,x,1},{x,0,1},{1,0,x}};
    const int i = std::clamp((int)h, 0, 5);
    for (int k = 0; k < 3; k++) out[k] = v * (1.0 - s + s * c[i][k]);
}

double dot3(const double a[3], const double b[3]) {
    return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
}

// The attributes that are LINEAR in the colour, as the row they are
// (colour_scalar above). Hue and saturation are not, and get no row.
bool colour_row(Attr a, double row[3]) {
    const double y[3] = {0.2126, 0.7152, 0.0722};
    for (int k = 0; k < 3; k++) {
        switch (a) {
            case Attr::Red:   row[k] = k == 0; break;
            case Attr::Green: row[k] = k == 1; break;
            case Attr::Blue:  row[k] = k == 2; break;
            case Attr::Luma:  row[k] = y[k]; break;
            case Attr::ChromaU: row[k] = ((k == 2) - y[k]) / 1.8556; break;
            case Attr::ChromaV: row[k] = ((k == 0) - y[k]) / 1.5748; break;
            default: return false;
        }
    }
    return true;
}

// The primary or secondary a row points at most strongly, in degrees.
double hue_toward(const double row[3], double sign) {
    double best = 0.0, best_d = -1e9;
    for (int k = 0; k < 6; k++) {
        double c[3];
        hsv_rgb(60.0 * k, 1.0, 1.0, c);
        const double d = sign * dot3(row, c);
        if (d > best_d) { best_d = d; best = 60.0 * k; }
    }
    return best;
}

// A hue with one linear attribute: a colour DIFFERENCE fixes how vivid that
// hue is (and cannot be had at all from a hue on its far side -- grey); a
// channel or the luma fixes how bright.
void hue_with_row(double hue, const double row[3], double value, double out[3]) {
    const double white = row[0] + row[1] + row[2];
    if (std::fabs(white) < 1e-6) {
        double pure[3];
        hsv_rgb(hue, 1.0, 1.0, pure);
        const double d = dot3(row, pure);
        const bool same_side = std::fabs(d) > 0.02 && d * value > 0.0;
        hsv_rgb(hue, same_side ? std::pow(std::min(std::fabs(value) / 0.3, 1.0), 0.7) : 0.0,
                0.95, out);
        return;
    }
    double c[3];
    hsv_rgb(hue, 0.8, 1.0, c);
    const double d = std::max(dot3(row, c), 1e-3);
    if (value <= d) {
        for (int k = 0; k < 3; k++) out[k] = c[k] * std::max(value, 0.0) / d;
    } else {
        const double t = std::clamp((value - d) / std::max(white - d, 1e-6), 0.0, 1.0);
        for (int k = 0; k < 3; k++) out[k] = c[k] + (1.0 - c[k]) * t;
    }
}

// Two linear attributes: the colour nearest mid grey that has both values.
bool rows_rgb(const double a[3], double va, const double b[3], double vb, double out[3]) {
    const double g[3] = {0.55, 0.55, 0.55};
    const double ra = va - dot3(a, g), rb = vb - dot3(b, g);
    const double aa = dot3(a, a), ab = dot3(a, b), bb = dot3(b, b);
    const double det = aa * bb - ab * ab;
    if (std::fabs(det) < 1e-9) return false;
    const double x = (ra * bb - rb * ab) / det, y = (rb * aa - ra * ab) / det;
    for (int k = 0; k < 3; k++) out[k] = std::clamp(g[k] + x * a[k] + y * b[k], 0.0, 1.0);
    return true;
}

bool pair_rgb(Attr a, double va, Attr b, double vb, double out[3]) {
    if (a == Attr::Saturation || (b == Attr::Hue && a != Attr::Hue)) {
        std::swap(a, b);
        std::swap(va, vb);
    }
    double ra[3], rb[3];
    const bool la = colour_row(a, ra), lb = colour_row(b, rb);
    if (la && lb) return rows_rgb(ra, va, rb, vb, out);
    if (a == Attr::Hue) {
        if (!std::isfinite(va)) return false;
        if (b == Attr::Saturation) hsv_rgb(va, std::clamp(vb, 0.0, 1.0), 0.95, out);
        else if (lb) hue_with_row(va, rb, vb, out);
        else return false;
        return true;
    }
    if (!la || b != Attr::Saturation) return false;
    // Saturation with no hue beside it: a colour difference still says which
    // way the colour leans, a channel is its own hue, and luma borrows red.
    const double s = std::clamp(vb, 0.0, 1.0);
    const double white = ra[0] + ra[1] + ra[2];
    if (std::fabs(white) < 1e-6)
        hsv_rgb(hue_toward(ra, va < 0 ? -1.0 : 1.0),
                s * std::min(std::fabs(va) / 0.02, 1.0), 0.95, out);
    else if (a == Attr::Luma)
        hsv_rgb(0.0, s, std::clamp(va, 0.0, 1.0), out);   // the stand-in red
    else
        hsv_rgb(hue_toward(ra, 1.0), s, std::clamp(va, 0.0, 1.0), out);
    return true;
}

}  // namespace

// A colour difference, stretched so the END of its axis is the +-0.3 the
// tints call full: a real scene spans a fraction of that, and unstretched the
// whole plot is pastel.
static double plot_value(const AttrInfo& info, const AttrHistogram& h, float f) {
    const double v = h.value_at(f);
    if (info.id != Attr::ChromaU && info.id != Attr::ChromaV) return v;
    const double top = std::max(std::fabs(h.value_at(0.0)), std::fabs(h.value_at(1.0)));
    return top > 1e-6 && top < 0.3 ? v * 0.3 / top : v;
}

unsigned attr_pair_colour(const AttrInfo& a, const AttrHistogram& ha, float fa,
                          const AttrInfo& b, const AttrHistogram& hb, float fb) {
    const double va = plot_value(a, ha, fa), vb = plot_value(b, hb, fb);
    const bool ca = is_colour(a.id), cb = is_colour(b.id);
    if (ca && cb && a.id != b.id) {
        double c[3];
        if (pair_rgb(a.id, va, b.id, vb, c)) return pack_rgb(c);
    }
    // One axis with something to say: a colour attribute before a position's
    // flat axis colour, and two flat ones say nothing about a cell.
    const bool use_a = ca || (!cb && a.tint != AttrTint::None && b.tint == AttrTint::None);
    const bool use_b = !ca && (cb || (b.tint != AttrTint::None && a.tint == AttrTint::None));
    if (use_a && a.tint != AttrTint::None) return attr_tint_colour(a.tint, va, fa);
    if (use_b && b.tint != AttrTint::None) return attr_tint_colour(b.tint, vb, fb);
    return 0;  // nothing to say: the caller's neutral
}

int attr_group(Attr a) {
    switch (a) {
        case Attr::Opacity: return 1;
        case Attr::AnisoRatio: case Attr::Erank: return 3;
        case Attr::Knn4: case Attr::Knn16: case Attr::Knn64: return 4;
        case Attr::PosX: case Attr::PosY: case Attr::PosZ:
        case Attr::OriginDistance: return 6;
        case Attr::PieceSize: case Attr::EdgeLength: case Attr::FaceArea: return 7;
        case Attr::FaceAngleMin: case Attr::FaceAngleMax: case Attr::DihedralMax: return 8;
        case Attr::EdgeFacesMin: case Attr::EdgeFacesMax: case Attr::Valence: return 9;
        case Attr::TrackLength: case Attr::ReprojError: case Attr::TriangulationAngle:
        case Attr::InViewCount: case Attr::CameraDistance: return 10;
        case Attr::Elevation: case Attr::Heading: case Attr::Roll:
        case Attr::LookAway: return 11;
        case Attr::NeighbourDistance: case Attr::PointsSeen: return 12;
        case Attr::FocalLength: case Attr::FieldOfView: return 13;
        default: break;
    }
    return is_colour(a) ? 5 : 2;
}

// What people come here for first goes first, and what is alike stays
// together (attr_group): the first entry is also what the panel opens on.
std::vector<Attr> attributes_of(const EditDoc& doc) {
    std::vector<Attr> out;
    const bool splats = doc.splats() != nullptr;
    const bool mesh = doc.mesh() != nullptr;
    const ParsedDataset* ds = doc.dataset();
    const bool cameras = ds && doc.layer() == 1;
    const bool points = ds && doc.layer() == 0;
    auto add = [&out](std::initializer_list<Attr> list) {
        out.insert(out.end(), list);
    };
    if (cameras) {
        add({Attr::Elevation, Attr::Heading, Attr::Roll, Attr::LookAway,
             Attr::NeighbourDistance});
        if (doc.has_sparse_stats()) add({Attr::PointsSeen});
        add({Attr::FocalLength, Attr::FieldOfView});
    }
    if (splats)
        add({Attr::Opacity, Attr::ExtentMax, Attr::ExtentMean, Attr::ExtentMin,
             Attr::ScaleMax, Attr::ScaleMean, Attr::ScaleMin});
    // A mesh is cleaned piece by piece, then by the stretched faces bridging
    // what should be a gap, and only then by what is wrong with a corner.
    if (mesh) add({Attr::PieceSize, Attr::EdgeLength, Attr::FaceArea});
    if (points) {
        if (doc.has_sparse_stats())
            add({Attr::TrackLength, Attr::ReprojError, Attr::TriangulationAngle});
        if (ds->num_cameras > 0) add({Attr::InViewCount, Attr::CameraDistance});
    }
    // Spacing is how a floater is told from a surface in every kind of cloud;
    // a mesh has its pieces for that, so there it waits further down.
    if (!cameras && !mesh) add({Attr::Knn4, Attr::Knn16, Attr::Knn64});
    if (doc.colours_available())
        add({Attr::Hue, Attr::Saturation, Attr::Luma, Attr::ChromaU, Attr::ChromaV,
             Attr::Red, Attr::Green, Attr::Blue});
    if (splats) add({Attr::AnisoRatio, Attr::Erank});
    if (mesh)
        add({Attr::FaceAngleMin, Attr::FaceAngleMax, Attr::DihedralMax,
             Attr::EdgeFacesMin, Attr::EdgeFacesMax, Attr::Valence,
             Attr::Knn4, Attr::Knn16, Attr::Knn64});
    add({Attr::PosZ, Attr::OriginDistance, Attr::PosX, Attr::PosY});
    return out;
}

bool attribute_values(const EditDoc& doc, Attr a, std::vector<float>& out,
                      const std::atomic<bool>* cancel) {
    const int64_t n = doc.count();
    out.assign((size_t)n, kNaN);

    if (a <= Attr::PosZ) {
        // positions() frame -> the file's, with the placement on top.
        const spirula::Sim3 to_saved = doc.view_frame().inverse() * doc.placement();
        const float* p = doc.positions();
        const int axis = (int)a;
#pragma omp parallel for schedule(static)
        for (int64_t i = 0; i < n; i++) {
            const double q[3] = {p[i*3], p[i*3+1], p[i*3+2]};
            double w[3];
            to_saved.apply(q, w);
            out[(size_t)i] = (float)w[axis];
        }
        return true;
    }

    if (is_shape(a)) {
        const spirula::SplatCloud* c = doc.splats();
        if (!c || c->num != n) return false;
        const float grow = (float)std::log(doc.file_placement().s);
#pragma omp parallel for schedule(static)
        for (int64_t i = 0; i < n; i++) {
            const float* s = &c->scales[(size_t)i * 3];
            const float hi = std::max(s[0], std::max(s[1], s[2])) + grow;
            const float lo = std::min(s[0], std::min(s[1], s[2])) + grow;
            const float mean = (s[0] + s[1] + s[2]) / 3.0f + grow;
            const float op = 1.0f / (1.0f + std::exp(-c->opacities[(size_t)i]));
            // Zero for a Gaussian the rasterizer never draws at all.
            const float reach = op > kAlphaCut
                ? std::sqrt(2.0f * std::log(op / kAlphaCut)) : 0.0f;
            float v = kNaN;
            switch (a) {
                case Attr::Opacity:    v = op; break;
                case Attr::ScaleMax:   v = std::exp(hi); break;
                case Attr::ScaleMin:   v = std::exp(lo); break;
                case Attr::ScaleMean:  v = std::exp(mean); break;
                case Attr::ExtentMax:  v = std::exp(hi) * reach; break;
                case Attr::ExtentMin:  v = std::exp(lo) * reach; break;
                case Attr::ExtentMean: v = std::exp(mean) * reach; break;
                case Attr::AnisoRatio: v = std::exp(std::min(hi - lo, 60.0f)); break;
                default: {
                    // The effective rank of the covariance, as the erank
                    // regularizer has it (shaders/per_splat_losses.slang).
                    double e[3], sum = 0.0;
                    for (int k = 0; k < 3; k++) {
                        e[k] = std::exp(2.0 * (double)(s[k] + grow - hi));
                        sum += e[k];
                    }
                    double h = 0.0;
                    for (int k = 0; k < 3; k++) {
                        const double q = std::max(e[k] / sum, 1e-30);
                        h -= q * std::log(q);
                    }
                    v = (float)std::exp(h);
                }
            }
            out[(size_t)i] = v;
        }
        return true;
    }

    if (is_colour(a)) {
        std::vector<float> rgb;
        if (!doc.colours(rgb) || (int64_t)rgb.size() != n * 3) return false;
#pragma omp parallel for schedule(static)
        for (int64_t i = 0; i < n; i++)
            out[(size_t)i] = colour_scalar(a, &rgb[(size_t)i * 3]);
        return true;
    }

    if (a == Attr::OriginDistance) {
        const spirula::Sim3 to_saved = doc.view_frame().inverse() * doc.placement();
        const float* p = doc.positions();
#pragma omp parallel for schedule(static)
        for (int64_t i = 0; i < n; i++) {
            const double q[3] = {p[i*3], p[i*3+1], p[i*3+2]};
            double w[3];
            to_saved.apply(q, w);
            out[(size_t)i] = (float)std::sqrt(w[0]*w[0] + w[1]*w[1] + w[2]*w[2]);
        }
        return true;
    }

    if (a == Attr::Knn4 || a == Attr::Knn16 || a == Attr::Knn64) {
        const int k = a == Attr::Knn4 ? 4 : a == Attr::Knn16 ? 16 : 64;
        // Elements sit on surfaces, so k of them fill a DISC: pi r^2 of the
        // surface's share of the count. The cell is a little over that
        // radius, and the search widens by shells where the cloud thins out.
        const double live = (double)std::max<int64_t>(doc.alive_count(), 1);
        const double r = (double)doc.extent() * std::sqrt((double)k / (3.14159265 * live));
        ElementGrid grid;
        grid.build(doc, (float)std::max(1.3 * r, (double)doc.extent() * 1e-6));
        grid.knn_median(k, doc.alive(), out, cancel);
        const float unit = (float)(doc.placement().s / doc.view_frame().s);
        for (float& v : out) v *= unit;
        return true;
    }

    if (a >= Attr::FaceAngleMin && a <= Attr::PieceSize) return mesh_values(doc, a, out);
    if (a >= Attr::TrackLength && a <= Attr::InViewCount)
        return point_values(doc, a, out, cancel);
    if (a >= Attr::FocalLength && a <= Attr::PointsSeen) return camera_values(doc, a, out);

    if (a == Attr::CameraDistance) {
        const std::vector<float> cams = doc.camera_centres();
        const int64_t nc = (int64_t)cams.size() / 3;
        if (nc == 0) return false;
        const float* p = doc.positions();
        const float unit = (float)(doc.placement().s / doc.view_frame().s);
#pragma omp parallel for schedule(static)
        for (int64_t i = 0; i < n; i++) {
            float best = std::numeric_limits<float>::max();
            for (int64_t c = 0; c < nc; c++) {
                const float dx = p[i*3] - cams[(size_t)c*3];
                const float dy = p[i*3+1] - cams[(size_t)c*3+1];
                const float dz = p[i*3+2] - cams[(size_t)c*3+2];
                best = std::min(best, dx*dx + dy*dy + dz*dz);
            }
            out[(size_t)i] = std::sqrt(best) * unit;
        }
        return true;
    }
    return false;
}


// ---------------------------------------------------------------------------
// Histogram
// ---------------------------------------------------------------------------

void AttrHistogram::build(const std::vector<float>& v, const uint8_t* alive,
                          const uint8_t* sel, const AttrInfo& info) {
    log = info.log;
    bins = kBins;
    whole = false;
    periodic = info.periodic && info.hi > info.lo;
    all.assign(kBins, 0);
    selected.assign(kBins, 0);
    peak = 0;
    live = 0;
    const int64_t n = (int64_t)v.size();
    auto axis = [&](float x) -> double {
        if (!log) return (double)x;
        return x > 0.0f ? std::log10((double)x) : -std::numeric_limits<double>::infinity();
    };

    // The range, from a sample: a percentile does not get better for being
    // taken over more of the same distribution.
    const int64_t step = std::max<int64_t>(1, n / 200000);
    std::vector<double> sample;
    for (int64_t i = 0; i < n; i += step) {
        if (alive && !alive[i]) continue;
        const double a = axis(v[(size_t)i]);
        if (std::isfinite(a)) sample.push_back(a);
    }
    if (sample.empty()) { lo = 0.0; hi = 1.0; return; }
    auto pct = [&](double q) {
        const size_t k = (size_t)std::clamp(q * (double)(sample.size() - 1), 0.0,
                                            (double)(sample.size() - 1));
        std::nth_element(sample.begin(), sample.begin() + (ptrdiff_t)k, sample.end());
        return sample[k];
    };
    lo = pct(0.002);
    hi = pct(0.998);
    if (info.integer && !log) {
        // One bin per whole number, centred on it. Past 256 of them the bins
        // go back to being ranges, which a count in the thousands is anyway.
        lo = std::floor(lo) - 0.5;
        hi = std::ceil(hi) + 0.5;
        if (hi - lo <= (double)kBins) {
            bins = std::max(1, (int)std::lround(hi - lo));
            whole = true;
        }
    } else {
        // A couple of hundred cameras in 256 bins is a comb, not a shape.
        bins = (int)std::clamp<int64_t>((int64_t)sample.size() * step / 6, 16, kBins);
        if (!(hi > lo)) {
            const double pad = std::max(std::fabs(lo) * 1e-3, 1e-6);
            lo -= pad;
            hi += pad;
        }
        const double pad = (hi - lo) * 0.02;
        lo -= pad;
        hi += pad;
    }
    if (info.hi > info.lo) {
        lo = info.lo;
        hi = info.hi;
    }

    const double k = bins / (hi - lo);
    for (int64_t i = 0; i < n; i++) {
        if (alive && !alive[i]) continue;
        const double a = axis(v[(size_t)i]);
        if (std::isnan(a)) continue;
        const int b = (int)std::clamp((a - lo) * k, 0.0, (double)(bins - 1));
        all[(size_t)b]++;
        if (sel && sel[i]) selected[(size_t)b]++;
        live++;
    }
    for (uint32_t c : all) peak = std::max(peak, c);
}

void AttrDensity::build(const std::vector<float>& x, const AttrHistogram& hx,
                        const std::vector<float>& y, const AttrHistogram& hy,
                        const uint8_t* alive, const uint8_t* sel, int nx_, int ny_,
                        std::vector<int32_t>* cell_of) {
    nx = std::max(1, nx_);
    ny = std::max(1, ny_);
    all.assign((size_t)nx * ny, 0);
    selected.assign((size_t)nx * ny, 0);
    peak = 0;
    const int64_t n = (int64_t)std::min(x.size(), y.size());
    if (cell_of) cell_of->assign((size_t)n, -1);
    for (int64_t i = 0; i < n; i++) {
        if (alive && !alive[i]) continue;
        if (std::isnan(x[(size_t)i]) || std::isnan(y[(size_t)i])) continue;
        // Off the end of an axis is drawn AT the end, as the 1D plot does.
        const double fx = std::clamp(hx.frac_of(x[(size_t)i]), 0.0, 1.0);
        const double fy = std::clamp(hy.frac_of(y[(size_t)i]), 0.0, 1.0);
        const int cx = std::min((int)(fx * nx), nx - 1), cy = std::min((int)(fy * ny), ny - 1);
        const size_t c = (size_t)cy * nx + cx;
        all[c]++;
        if (sel && sel[i]) selected[c]++;
        if (cell_of) (*cell_of)[(size_t)i] = (int32_t)c;
    }
    for (uint32_t c : all) peak = std::max(peak, c);
}

double AttrHistogram::value_at(double frac) const {
    const double a = lo + (hi - lo) * frac;
    return log ? std::pow(10.0, a) : a;
}

double AttrHistogram::frac_of(double value) const {
    const double a = log ? (value > 0 ? std::log10(value) : lo) : value;
    return hi > lo ? (a - lo) / (hi - lo) : 0.0;
}

void select_by_range(const std::vector<float>& v, const AttrHistogram& h,
                     double f0, double f1, bool outside, const uint8_t* alive,
                     std::vector<uint8_t>& out) {
    const int64_t n = (int64_t)v.size();
    out.assign((size_t)n, 0);
    if (f0 > f1) std::swap(f0, f1);
    if (h.periodic) {
        // How far UP the circle from f0, against how far the range goes.
        const double width = f1 - f0, span = h.hi - h.lo;
#pragma omp parallel for schedule(static)
        for (int64_t i = 0; i < n; i++) {
            if (alive && !alive[i]) continue;
            const float x = v[(size_t)i];
            if (std::isnan(x)) continue;
            double g = ((double)x - h.lo) / span - f0;
            g -= std::floor(g);
            if ((width >= 1.0 || g <= width) != outside) out[(size_t)i] = 255;
        }
        return;
    }
    const double inf = std::numeric_limits<double>::infinity();
    // An end dragged to the edge of the plot means "and everything past it".
    const double a0 = f0 <= 0.0 ? -inf : h.lo + (h.hi - h.lo) * f0;
    const double a1 = f1 >= 1.0 ? inf : h.lo + (h.hi - h.lo) * f1;
    const bool log = h.log;
#pragma omp parallel for schedule(static)
    for (int64_t i = 0; i < n; i++) {
        if (alive && !alive[i]) continue;
        const float x = v[(size_t)i];
        if (std::isnan(x)) continue;
        const double a = log ? (x > 0.0f ? std::log10((double)x) : -inf) : (double)x;
        if ((a >= a0 && a <= a1) != outside) out[(size_t)i] = 255;
    }
}

void select_by_colour(const std::vector<float>& rgb, const float* samples,
                      int k, float tolerance, float lightness_weight,
                      const uint8_t* alive, std::vector<uint8_t>& out) {
    const int64_t n = (int64_t)rgb.size() / 3;
    out.assign((size_t)n, 0);
    if (k <= 0) return;
    std::vector<float> lab((size_t)k * 3);
    for (int j = 0; j < k; j++) srgb_to_oklab(samples + j * 3, lightness_weight, &lab[(size_t)j * 3]);
    const float t2 = tolerance * tolerance;
#pragma omp parallel for schedule(static)
    for (int64_t i = 0; i < n; i++) {
        if (alive && !alive[i]) continue;
        float c[3];
        srgb_to_oklab(&rgb[(size_t)i * 3], lightness_weight, c);
        for (int j = 0; j < k; j++) {
            const float d0 = c[0] - lab[(size_t)j*3], d1 = c[1] - lab[(size_t)j*3+1],
                        d2 = c[2] - lab[(size_t)j*3+2];
            if (d0*d0 + d1*d1 + d2*d2 <= t2) {
                out[(size_t)i] = 255;
                break;
            }
        }
    }
}

}  // namespace gui
