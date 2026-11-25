#include "app/MeshPreviewFile.h"
#include "core/AtomicFile.h"

#include <climits>

#include <algorithm>
#include <array>
#include <fstream>
#include <unordered_set>

namespace spirula {
namespace {
using Mesh = meshing::MeshData;
using Vertex = std::array<float, 3>;
using Face = std::array<int, 3>;
constexpr int kGrid = 128;
using Cuts = std::array<std::array<float, kGrid - 1>, 3>;

std::filesystem::path file_path(const std::string& dir) {
    return std::filesystem::path(dir) / "surface.bin";
}

// Equal-population intervals keep a distant floater from consuming the grid resolution.
Cuts grid_cuts(const Mesh& mesh) {
    Cuts cuts;
    const size_t n = std::min<size_t>(8192, mesh.V.size());
    std::vector<float> sample(n);
    for (int axis = 0; axis < 3; ++axis) {
        uint32_t jitter = 1;
        for (size_t i = 0; i < n; ++i) {
            const size_t first = i * uint64_t(mesh.V.size()) / n;
            const size_t end = (i + 1) * uint64_t(mesh.V.size()) / n;
            jitter = 1664525u * jitter + 1013904223u;
            sample[i] = mesh.V[first + (uint64_t(jitter) * (end - first) >> 32)][axis];
        }
        std::sort(sample.begin(), sample.end());
        for (int i = 1; i < kGrid; ++i) cuts[axis][i-1] = sample[i * n / kGrid];
    }
    return cuts;
}

bool cluster_surface(const Mesh& src, const Cuts& cuts, int side, size_t max_faces, size_t cache_bytes, Mesh& dst) {
    auto cell = [&](const Vertex& p) {
        int id = 0;
        for (int a = 0; a < 3; ++a) {
            const auto& c = cuts[a];
            const int bin = int(std::upper_bound(c.begin(), c.end(), p[a]) - c.begin());
            id = id * side + bin / (kGrid / side);
        }
        return id;
    };
    struct Cluster { std::array<double, 3> sum{}; uint32_t count = 0; int vertex = -1; };
    std::vector<Cluster> cells(side * side * side);
    // Cache vertex lookups up to 8 MiB. Above that, recompute them while reading faces.
    std::vector<int> ids(src.V.size() <= cache_bytes / sizeof(int) ? src.V.size() : 0);
    for (size_t i = 0; i < src.V.size(); ++i) {
        const int id = cell(src.V[i]);
        if (!ids.empty()) ids[i] = id;
        auto& c = cells[id];
        ++c.count;
        for (int a = 0; a < 3; ++a) c.sum[a] += src.V[i][a];
    }
    dst.V.clear();
    dst.F.clear();
    for (auto& c : cells) {
        if (!c.count) continue;
        c.vertex = int(dst.V.size());
        dst.V.push_back({float(c.sum[0]/c.count), float(c.sum[1]/c.count), float(c.sum[2]/c.count)});
    }
    std::unordered_set<uint64_t> seen;
    for (Face f : src.F) {
        for (int& i : f) i = cells[ids.empty() ? cell(src.V[i]) : ids[i]].vertex;
        if (f[0] == f[1] || f[1] == f[2] || f[2] == f[0]) continue;
        Face key = f;
        std::sort(key.begin(), key.end());
        const uint64_t packed = (uint64_t(key[0]) << 42) | (uint64_t(key[1]) << 21) | key[2];
        if (seen.insert(packed).second) dst.F.push_back(f);
        if (dst.F.size() > max_faces) return false;
    }
    return true;
}

bool reduce_surface(const Mesh& mesh, MeshPreviewBudget budget, Mesh& coarse) {
    if (budget.host < (1u << 20)) return false;
    const size_t cache = std::min<uint64_t>(8u << 20, budget.host / 8);
    const size_t max_faces = std::min<uint64_t>({budget.host / 256, budget.device / 80, INT_MAX / 3});
    if (!max_faces) return false;
    const Cuts cuts = grid_cuts(mesh);
    int side = kGrid;
    while (uint64_t(side)*side*side*32 > budget.host/4) side /= 2;
    for (; side >= 2; side /= 2) {
        if (cluster_surface(mesh, cuts, side, max_faces, cache, coarse) &&
            budget.fits_surface(coarse.V.size(), coarse.F.size())) return !coarse.F.empty();
    }
    return false;
}

struct Header { uint32_t stage, vertices, faces; };
}  // namespace

void save_preview_device_free(const std::string& dir, uint64_t bytes) {
    if (bytes) write_file_atomic(std::filesystem::path(dir) / "preview-device.bin", &bytes, sizeof bytes);
}

uint64_t load_preview_device_free(const std::string& dir) {
    uint64_t bytes = 0;
    std::ifstream in(std::filesystem::path(dir) / "preview-device.bin", std::ios::binary);
    return in.read(reinterpret_cast<char*>(&bytes), sizeof bytes) ? bytes : 0;
}

void write_mesh_preview(const std::string& dir, const Mesh& mesh, int stage, MeshPreviewBudget budget,
                        bool allow_reduction) {
    if (mesh.V.empty() || mesh.F.empty()) return;
    try {
        Mesh coarse;
        const Mesh* surface = &mesh;
        if (!budget.fits_surface(mesh.V.size(), mesh.F.size())) {
            if (!allow_reduction || !reduce_surface(mesh, budget, coarse)) return;
            surface = &coarse;
        }
        if (surface->F.empty()) return;
        const Header h{uint32_t(stage), uint32_t(surface->V.size()), uint32_t(surface->F.size())};
        const size_t bytes = sizeof h + surface->V.size()*sizeof(Vertex) + surface->F.size()*sizeof(Face);
        AtomicOutputFile file(file_path(dir), std::min<uint64_t>({kFileBufferBytes, budget.host / 16, bytes}));
        auto& out = file.stream();
        out.write(reinterpret_cast<const char*>(&h), sizeof h);
        out.write(reinterpret_cast<const char*>(surface->V.data()), size_t(h.vertices) * sizeof(Vertex));
        out.write(reinterpret_cast<const char*>(surface->F.data()), size_t(h.faces) * sizeof(Face));
        file.close_checked();
        file.commit();
    } catch (const std::exception&) {
        // An optional preview must not interrupt the export.
    }
}

int read_mesh_preview(const std::string& dir, int after_stage, Mesh& mesh, MeshPreviewBudget budget) {
    std::ifstream in(file_path(dir), std::ios::binary | std::ios::ate);
    const auto bytes = in.tellg();
    in.seekg(0);
    Header h;
    if (!in.read(reinterpret_cast<char*>(&h), sizeof h) || h.stage > INT_MAX || int(h.stage) <= after_stage ||
        !h.vertices || !h.faces || h.vertices > INT_MAX || h.faces > INT_MAX / 3) return 0;
    const bool fits = budget.fits_surface(h.vertices, h.faces);
    const uint64_t vb = uint64_t(h.vertices) * sizeof(Vertex), fb = uint64_t(h.faces) * sizeof(Face);
    if (bytes != std::streamoff(sizeof h + vb + fb) || vb + fb > budget.host) return 0;
    Mesh result;
    result.V.resize(h.vertices);
    result.F.resize(h.faces);
    if (!in.read(reinterpret_cast<char*>(result.V.data()), vb) ||
        !in.read(reinterpret_cast<char*>(result.F.data()), fb)) return 0;
    for (const Face& face : result.F)
        for (int v : face) if (v < 0 || uint32_t(v) >= h.vertices) return 0;
    if (fits) mesh = std::move(result);
    else {
        // The OpenGL adapter or the worker's forecast may have changed since publication.
        budget.host -= vb + fb;
        Mesh coarse;
        if (!reduce_surface(result, budget, coarse)) return 0;
        mesh = std::move(coarse);
    }
    return int(h.stage);
}
}  // namespace spirula
