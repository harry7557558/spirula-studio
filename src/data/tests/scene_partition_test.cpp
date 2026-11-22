// scene_partition_test -- data/ScenePartition.h over a synthetic corridor:
// cameras along a line looking down at a strip of points. The cut must fall
// across the corridor, the rings must sit at the cut, the file must round-trip,
// applying a part must keep exactly its frames, and the merge must keep each
// model's splats on its own side (checkpoint/SplatMerge.h).

#include "app/TrainerCore.h"
#include "config/TrainConfigJson.h"
#include "checkpoint/SplatMerge.h"
#include "checkpoint/RegionCameras.h"
#include "checkpoint/SplatTransform.h"
#include "core/BufferedFile.h"
#include "core/Sha256.h"
#include "checkpoint/SplatPly.h"
#include "core/SphericalBeta.h"
#include "data/JsonWrite.h"
#include "data/DatasetParser.h"
#include "data/SparseEdit.h"
#include "data/Json.h"
#include "data/RegionProgram.h"
#include "data/ScenePartition.h"
#include "data/SceneTransform.h"
#include "external/stb_image.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <exception>
#include <limits>
#include <random>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

namespace {

int g_failures = 0;

void check(bool ok, const std::string& what) {
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what.c_str());
    if (!ok) g_failures++;
}

// `n_cam` cameras at x = 0..n_cam-1, z = 5, looking straight down (-z in the
// OpenGL convention is the identity rotation), over points on z = 0.
struct Corridor {
    ParsedDataset ds;
    spirula::SparseStats tracks;
};

// `reach`: how far along x a camera sees; `keep`: the chance it tracks a
// point it sees, which makes per-point observer counts noisy.
Corridor make_corridor(int n_cam, int pts_per_cam, unsigned seed, double reach = 2.5,
                       double keep = 1.0) {
    Corridor c;
    ParsedDataset& d = c.ds;
    d.num_cameras = n_cam;
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> u(-1.0, 1.0);
    for (int i = 0; i < n_cam; i++) {
        d.camera_models.push_back(0);
        d.camera_distortions.push_back(0);
        d.image_filenames.push_back("/nowhere/img_" + std::to_string(1000 + i) + ".jpg");
        d.widths.push_back(500);
        d.heights.push_back(500);
        float m[12] = {1, 0, 0, (float)i, 0, 1, 0, 0, 0, 0, 1, 5};
        d.c2w.insert(d.c2w.end(), m, m + 12);
        // f = 500 over a 500 px frame: half-angle 26.6 degrees, so a camera
        // at height 5 sees x within +-2.5 of itself.
        const float k[4] = {500, 500, 250, 250};
        d.intrins.insert(d.intrins.end(), k, k + 4);
        for (int j = 0; j < 8; j++) d.dist_coeffs.push_back(0.0f);
        d.train_indices.push_back(i);
        c.tracks.image_names.push_back("img_" + std::to_string(1000 + i) + ".jpg");
    }
    c.tracks.track_beg.push_back(0);
    for (int i = 0; i < n_cam; i++)
        for (int j = 0; j < pts_per_cam; j++) {
            const double x = i + u(rng), y = 1.5 * u(rng), z = 0.2 * u(rng);
            d.points.xyz.insert(d.points.xyz.end(), {x, y, z});
            d.points.rgb.insert(d.points.rgb.end(), {128, 128, 128});
            for (int k = 0; k < n_cam; k++)
                if (std::fabs(k - x) <= reach && (keep >= 1.0 || 0.5 * (u(rng) + 1.0) < keep))
                    c.tracks.track_image.push_back(k);
            c.tracks.track_beg.push_back((int64_t)c.tracks.track_image.size());
            c.tracks.error.push_back(0.5f);
        }
    return c;
}

void check_voronoi_graph_free() {
    using namespace spirula;
    auto compare = [&](const ParsedDataset& ds, const SparseStats* tracks, PartitionOptions opt,
                       CovisibilitySource expected, const std::string& name) {
        opt.method = PartitionMethod::Voronoi;
        opt.parts = 2;
        opt.ring_min_points = 1;
        const auto fast = build_covisibility(ds, tracks, opt);
        auto full_opt = opt;
        full_opt.method = PartitionMethod::ViewGraph;
        const auto full = build_covisibility(ds, tracks, full_opt);
        const auto a = partition_scene(ds, full, opt), b = partition_scene(ds, fast, opt);
        std::string af, bf;
        a.field->write(af); b.field->write(bf);
        check(full.camera_graph_built && fast.camera_graph_built == (expected == CovisibilitySource::Proximity),
              name + ": graph is omitted only for observer-based Voronoi");
        check(full.source == expected && fast.source == expected && full.beg == fast.beg &&
                  full.frame == fast.frame && full.point_row == fast.point_row,
              name + ": observation rows and source fallback are exact");
        check(a.num_parts == b.num_parts && a.frame_names == b.frame_names && a.frame_centers == b.frame_centers &&
                  a.frame_label == b.frame_label && a.point_label == b.point_label && a.ring == b.ring &&
                  a.part_points == b.part_points && af == bf && a.core_pieces == b.core_pieces &&
                  a.cut_fraction == b.cut_fraction && a.view_share == b.view_share,
              name + ": field, ownership, rings, seeds and topology match the full graph exactly");
        if (!fast.camera_graph_built) {
            bool refused = false;
            try { partition_scene(ds, fast, full_opt); } catch (const std::runtime_error&) { refused = true; }
            check(refused, name + ": graph-free cache cannot enter a graph partition method");
        }
        return b;
    };
    auto tracks = make_corridor(100, 1, 735);
    tracks.tracks.track_beg.assign(1, 0);
    tracks.tracks.track_image.clear();
    const int lengths[] = {0, 1, 2, 48, 49, 96, 97};
    for (int i = 0; i < tracks.ds.points.num(); i++) {
        for (int j = 0; j < lengths[i % 7]; j++)
            tracks.tracks.track_image.push_back((j / 2 * 7 + i) % 99);
        tracks.tracks.track_beg.push_back((int64_t)tracks.tracks.track_image.size());
    }
    PartitionOptions opt;
    opt.source = CovisibilitySource::Tracks;
    compare(tracks.ds, &tracks.tracks, opt, CovisibilitySource::Tracks, "strided duplicate tracks with isolated camera");
    tracks.tracks.track_beg.assign(1, 0);
    tracks.tracks.track_image.clear();
    for (int i = 0; i < tracks.ds.points.num(); i++) {
        tracks.tracks.track_image.push_back(i % 100);
        tracks.tracks.track_beg.push_back((int64_t)tracks.tracks.track_image.size());
    }
    compare(tracks.ds, &tracks.tracks, opt, CovisibilitySource::Tracks, "tracks without camera edges");
    auto projection = make_corridor(10, 2, 736);
    opt.source = CovisibilitySource::Projection;
    opt.max_projected_points = 1;
    const auto shared = compare(projection.ds, nullptr, opt, CovisibilitySource::Projection, "shared projection row");
    check(*std::min_element(shared.point_label.begin(), shared.point_label.end()) !=
              *std::max_element(shared.point_label.begin(), shared.point_label.end()),
          "shared observer row includes points with different Voronoi owners");
    for (size_t i = 0; i < projection.ds.intrins.size(); i += 4) {
        projection.ds.intrins[i] = 1e8f;
        projection.ds.intrins[i + 1] = 1e8f;
    }
    compare(projection.ds, nullptr, opt, CovisibilitySource::Proximity, "projection without camera edges");
    opt.source = CovisibilitySource::Proximity;
    compare(projection.ds, nullptr, opt, CovisibilitySource::Proximity, "explicit proximity");
}

// The cut of a corridor must be a single change of label along x.
int label_changes(const std::vector<int32_t>& labels) {
    int n = 0;
    for (size_t i = 1; i < labels.size(); i++) n += labels[i] != labels[i - 1];
    return n;
}

spirula::SplatCloud splats_along_x(double x0, double x1, int n, int sh_degree, float tag) {
    spirula::SplatCloud c;
    c.num = n;
    c.sh_degree = sh_degree;
    for (int i = 0; i < n; i++) {
        const double x = x0 + (x1 - x0) * (i + 0.5) / n;
        c.means.insert(c.means.end(), {(float)x, tag, 0.0f});
        c.quats.insert(c.quats.end(), {1, 0, 0, 0});
        c.scales.insert(c.scales.end(), {-2, -2, -2});
        c.opacities.push_back(2.0f);
        c.features_dc.insert(c.features_dc.end(), {0.5f, 0.2f, 0.1f});
        const int64_t k = c.dim_sh() - 1;
        for (int64_t j = 0; j < k * 3; j++) c.features_sh.push_back(0.01f * (float)j);
    }
    return c;
}

void write_text(const fs::path& p, const std::string& s) {
    std::ofstream f(p);
    f << s;
}

}  // namespace

int main(int argc, char** argv) {
    using namespace spirula;
    if (argc == 4 && std::string(argv[1]) == "--camera-metadata") {
        try {
            const JsonValue config = json_parse_file(argv[2]);
            if (!train_config_json_has_fields(config)) throw std::runtime_error("not a training config");
            TrainerSession session;
            train_config_from_json(config, session.cfg);
            if (session.cfg.partition.empty() || session.cfg.partition_part < 0)
                throw std::runtime_error("camera metadata control requires a partitioned run");
            session.load_dataset();
            if (!session.roi) throw std::runtime_error("partitioned run has no ownership region");
            const RegionCameras cameras = session.build_region_cameras();
            fs::create_directories(argv[3]);
            const std::string hash = save_region_cameras(argv[3], cameras);
            std::printf("camera metadata: %s, cameras: %lld\n", hash.c_str(),
                        (long long)cameras.field.num_seeds());
            return 0;
        } catch (const std::exception& error) {
            std::fprintf(stderr, "%s\n", error.what());
            return 1;
        }
    }
    if (argc == 5 && std::string(argv[1]) == "--overlap-seeds") {
        try {
            const ScenePartition original = read_partition(argv[3]);
            DatasetParserConfig config;
            config.require_image_files = false;
            const ParsedDataset dataset = parse_dataset(argv[2], config, "");
            const SparseStats tracks = read_sparse_stats(argv[2]);
            PartitionOptions options = original.options;
            options.outside_seed_fraction = 1.0f;
            const Covisibility visibility = build_covisibility(dataset, &tracks, options);
            const ScenePartition overlap = partition_scene(dataset, visibility, options);
            if (!original.field || !overlap.field) throw std::runtime_error("missing ownership field");
            std::string original_field, overlap_field;
            original.field->write(original_field);
            overlap.field->write(overlap_field);
            if (original.frame_names != overlap.frame_names || original.frame_label != overlap.frame_label ||
                original.frame_centers != overlap.frame_centers || original.ring != overlap.ring ||
                original.point_label != overlap.point_label || original_field != overlap_field)
                throw std::runtime_error("outside seed control changed camera or ownership assignments");
            for (int k = 0; k < original.num_parts; k++) {
                const auto& before = original.part_points[(size_t)k];
                const auto& after = overlap.part_points[(size_t)k];
                if (!std::includes(after.begin(), after.end(), before.begin(), before.end()))
                    throw std::runtime_error("outside seed control lost existing seeds");
                std::printf("part %d seed points: %zu -> %zu\n", k, before.size(), after.size());
            }
            write_partition(overlap, argv[4], fs::absolute(argv[2]).lexically_normal().string());
            std::printf("Camera assignments, rings and ownership field are unchanged.\n");
            return 0;
        } catch (const std::exception& error) {
            std::fprintf(stderr, "%s\n", error.what());
            return 1;
        }
    }
    if (argc != 1) {
        std::fprintf(stderr, "usage: scene_partition_test [--overlap-seeds DATASET PARTITION OUTPUT | --camera-metadata CONFIG DIRECTORY]\n");
        return 2;
    }
    check_voronoi_graph_free();
    const Corridor corr = make_corridor(60, 40, 5);
    const ParsedDataset& ds = corr.ds;

    // ---- tracks -> two parts ----
    PartitionOptions opt;
    opt.parts = 2;
    Covisibility cov = build_covisibility(ds, &corr.tracks, opt);
    check(cov.source == CovisibilitySource::Tracks, "auto takes the tracks when they exist");
    check(cov.num_points() == ds.points.num() && cov.has_tracks(), "one track row per point");
    check(cov.cameras.n() == 60 && cov.cameras.adj.size() > 0, "camera graph built");

    ScenePartition p = partition_scene(ds, cov, opt);
    check(p.num_parts == 2, "two parts asked, two made");
    check(label_changes(p.frame_label) == 1, "the cut is one boundary along the corridor");
    check(p.core_count(0) >= p.core_count(1), "largest part is labelled 0");
    // Which label ended up on which end of the corridor.
    const int L = p.frame_label.front(), R = 1 - L;
    {
        // Where the cut is, and that the rings sit right at it.
        int cut = 0;
        for (size_t i = 1; i < p.frame_label.size(); i++)
            if (p.frame_label[i] != p.frame_label[i - 1]) cut = (int)i;
        check(cut >= 24 && cut <= 36, "cut near the middle (at camera " + std::to_string(cut) + ")");
        bool rings_ok = !p.ring[0].empty() && !p.ring[1].empty();
        for (int32_t f : p.ring[(size_t)L]) rings_ok = rings_ok && f >= cut && f < cut + 6;
        for (int32_t f : p.ring[(size_t)R]) rings_ok = rings_ok && f < cut && f >= cut - 6;
        check(rings_ok, "rings are the few cameras just across the cut (" +
                            std::to_string(p.ring[0].size()) + " / " +
                            std::to_string(p.ring[1].size()) + ")");
        // Points follow: one boundary along x, near the cut.
        int wrong = 0;
        for (int64_t i = 0; i < ds.points.num(); i++) {
            const double x = ds.points.xyz[(size_t)i * 3];
            const int want = x < cut - 1.5 ? L : (x > cut + 1.5 ? R : -1);
            if (want >= 0 && p.point_label[(size_t)i] != want) wrong++;
        }
        check(wrong == 0, "points away from the cut belong to the side they are on (" +
                              std::to_string(wrong) + " wrong)");
        // The field: one boundary along the corridor axis, and the nearer
        // part far away.
        std::vector<int32_t> along;
        for (double x = 0; x < 60; x += 0.5) along.push_back(p.field->label(x, 0.0, 0.0));
        check(label_changes(along) == 1, "ownership field has one boundary along the corridor");
        check(p.field->label(-1000.0, 0.0, 0.0) == L && p.field->label(1000.0, 0.0, 0.0) == R,
              "far away, the nearer part owns the point");
        check(p.frame_centers.size() == (size_t)ds.num_cameras * 3, "camera centres recorded");
        check(p.cut_fraction > 0 && p.cut_fraction < 0.2,
              "cut fraction is small (" + std::to_string(p.cut_fraction) + ")");
        // Seeds: part 0 seeds from everything it owns plus what its ring sees.
        const std::vector<int64_t>& s0 = p.part_points[0];
        bool has_own = false, has_borrowed = false;
        for (int64_t i : s0) {
            has_own = has_own || p.point_label[(size_t)i] == 0;
            has_borrowed = has_borrowed || p.point_label[(size_t)i] == 1;
        }
        check(has_own && has_borrowed && s0.size() < (size_t)ds.points.num(),
              "part 0 seeds from its own points and its ring's, not the whole cloud");
    }

    {
        PartitionOptions vo = opt;
        vo.method = PartitionMethod::Voronoi;
        const ScenePartition v = partition_scene(ds, cov, vo);
        check(v.num_parts == 2 && label_changes(v.frame_label) == 1,
              "Voronoi cells divide the corridor once");
        check(v.field->num_seeds() == 2, "Voronoi ownership stores only its centroids");
        bool consistent = true;
        for (int64_t i = 0; i < ds.num_cameras; i++) {
            const float* c = v.frame_centers.data() + i * 3;
            consistent &= v.field->label(c[0], c[1], c[2]) == v.frame_label[(size_t)i];
        }
        for (int64_t i = 0; i < ds.points.num(); i++)
            consistent &= v.field->label(ds.points.xyz.data() + i * 3) == v.point_label[(size_t)i];
        check(consistent, "Voronoi camera and point owners agree with the merge field");
        const double q[3] = {10, 1, 0}, forward[3] = {1, 0, 0}, backward[3] = {-1, 0, 0};
        check(v.field->label(q, forward) == v.field->label(q, backward) &&
                  v.field->label(q, forward) == v.field->label(q), "Voronoi ownership is independent of splat orientation");
        check(partition_scene(ds, cov, vo).frame_label == v.frame_label, "Voronoi split is deterministic");
        auto isolated_camera = ds;
        isolated_camera.c2w[3] = 10000;
        const auto robust = partition_scene(isolated_camera, cov, vo);
        check(robust.num_parts == 2 && robust.core_count(0) >= 20 && robust.core_count(1) >= 20 &&
                  robust.frame_label.size() == (size_t)ds.num_cameras &&
                  robust.field->label(10000, 0, 5) == robust.frame_label[0],
              "an isolated pose does not consume a centroid and still receives an owner");
        vo.parts = 0;
        vo.max_images = 15;
        const ScenePartition limited = partition_scene(ds, cov, vo);
        bool capped = true;
        for (int k = 0; k < limited.num_parts; k++) capped &= limited.core_count(k) <= 15;
        check(capped, "Voronoi automatic count respects the core image cap");
        auto crowded = ds;
        for (int64_t i = 0; i < crowded.num_cameras; i++)
            crowded.c2w[(size_t)i * 12 + 3] = i == 59 ? 10000.0f : (float)((i + 1) / 2);
        auto tight = vo;
        tight.max_images = 2;
        const auto restored = partition_scene(crowded, cov, tight);
        bool tight_cap = restored.num_parts == 31;
        for (int k = 0; k < restored.num_parts; k++) tight_cap &= restored.core_count(k) <= 2;
        check(tight_cap, "automatic Voronoi restores isolated fitting poses when more distinct cells are required");
        PartitionMethod parsed;
        check(partition_method_from_name("voronoi", parsed) && parsed == PartitionMethod::Voronoi,
              "Voronoi CLI method parses");
        const fs::path path = fs::temp_directory_path() / "spirula_voronoi_test.json";
        write_partition(v, path.string(), "synthetic");
        const auto roundtrip = read_partition(path.string());
        check(roundtrip.options.method == PartitionMethod::Voronoi && roundtrip.point_label == v.point_label &&
                  roundtrip.field->euclidean,
              "Voronoi method and ownership round-trip");
        std::string error;
        const auto roi = region_from_json(json_parse(region_to_json(LabelRegion(v.field, 0))), "", error);
        RegionProgram roi_program;
        check(roi && compile_region(*roi, roi_program, error) && roi_program.field->euclidean &&
                  program_contains(roi_program, q, forward) == program_contains(roi_program, q, backward),
              "Voronoi mode survives standalone ROI JSON and compiled program");
        fs::remove(path);
        fs::path binary = path;
        binary.replace_extension(".bin");
        fs::remove(binary);
        std::atomic<bool> cancel{true};
        bool cancelled = false;
        try { partition_scene(ds, cov, vo, {}, &cancel); } catch (const PartitionCancelled&) { cancelled = true; }
        check(cancelled, "Voronoi cancellation is honored");
        auto invalid = ds;
        invalid.c2w[3] = std::numeric_limits<float>::quiet_NaN();
        bool rejected = false;
        try { partition_scene(invalid, cov, vo); } catch (const std::runtime_error&) { rejected = true; }
        check(rejected, "non-finite Voronoi camera positions are refused before field indexing");
        vo.parts = 2;
        for (int64_t i = 0; i < invalid.num_cameras; i++)
            for (int r = 0; r < 3; r++) invalid.c2w[(size_t)i * 12 + r * 4 + 3] = 0;
        rejected = false;
        try { partition_scene(invalid, cov, vo); } catch (const std::runtime_error&) { rejected = true; }
        check(rejected, "coincident camera positions cannot silently create empty Voronoi parts");
        for (double coordinate : {std::numeric_limits<double>::quiet_NaN(),
                                  std::numeric_limits<double>::infinity(), 1e20}) {
            auto malformed = ds;
            malformed.points.xyz[0] = coordinate;
            rejected = false;
            try { partition_scene(malformed, cov, vo); }
            catch (const std::runtime_error& e) {
                rejected = std::string(e.what()).find("cannot assign sparse point 0") != std::string::npos;
            }
            check(rejected, "tracked unsupported sparse coordinates fail before ownership indexing");
        }
    }

    {
        auto disconnected = cov;
        std::vector<graph::Edge> edges;
        for (uint32_t i = 0; i < cov.cameras.n(); i++)
            for (uint32_t e = cov.cameras.offs[i]; e < cov.cameras.offs[i + 1]; e++)
                if (i < 55 && cov.cameras.adj[e] < 55 && i < cov.cameras.adj[e])
                    edges.push_back({i, cov.cameras.adj[e], cov.cameras.w[e]});
        disconnected.cameras = graph::build_graph(60, edges);
        auto requested = opt;
        requested.parts = 4;
        const auto split = partition_scene(ds, disconnected, requested);
        check(split.num_parts == 4, "isolated cameras do not consume the requested view-graph cuts");
        disconnected.beg.assign(1, 0);
        disconnected.frame.clear();
        for (int64_t i = 0; i < cov.num_points(); i++) {
            for (int64_t j = cov.begin(i); j < cov.end(i); j++)
                if (cov.frame[(size_t)j] < 55) disconnected.frame.push_back(cov.frame[(size_t)j]);
            disconnected.beg.push_back((int64_t)disconnected.frame.size());
        }
        requested.method = PartitionMethod::Graph;
        const auto joint_split = partition_scene(ds, disconnected, requested);
        check(joint_split.num_parts == 4, "isolated cameras do not consume the requested joint-graph cuts");
        requested.method = PartitionMethod::ViewGraph;
        edges.clear();
        for (uint32_t i = 1; i < 60; i++) if (i != 30) edges.push_back({i - 1, i, 1});
        disconnected.cameras = graph::build_graph(60, edges);
        requested.parts = 1;
        bool refused = false;
        try { partition_scene(ds, disconnected, requested); }
        catch (const std::runtime_error&) { refused = true; }
        check(refused, "too few requested parts for disconnected groups is explicit");
    }

    // ---- a surface both parts see: owners must not interleave ----
    {
        Corridor room = make_corridor(40, 60, 11, 30.0, 0.1);
        const Covisibility rc = build_covisibility(room.ds, &room.tracks, PartitionOptions{});
        PartitionOptions ro;
        ro.method = PartitionMethod::ViewGraph;
        ro.parts = 2;
        const ScenePartition rp = partition_scene(room.ds, rc, ro);
        // Interleaving is what blurs a merge: the share of each point's ten
        // nearest points owned by another part.
        const int64_t n = room.ds.points.num();
        const double* X = room.ds.points.xyz.data();
        double foreign = 0;
        for (int64_t i = 0; i < n; i++) {
            std::vector<std::pair<double, int64_t>> d;
            for (int64_t j = 0; j < n; j++) {
                if (j == i) continue;
                double e = 0;
                for (int r = 0; r < 3; r++) e += (X[i * 3 + r] - X[j * 3 + r]) * (X[i * 3 + r] - X[j * 3 + r]);
                d.push_back({e, j});
            }
            std::partial_sort(d.begin(), d.begin() + 10, d.end());
            for (int k = 0; k < 10; k++)
                foreign += rp.point_label[(size_t)d[(size_t)k].second] != rp.point_label[(size_t)i];
        }
        foreign /= 10.0 * (double)n;
        char buf[64];
        std::snprintf(buf, sizeof buf, "%.3f", foreign);
        check(rp.num_parts == 2 && foreign < 0.03,
              std::string("every camera sees the floor: owners do not interleave (") + buf +
                  " of neighbours foreign)");
    }

    // ---- region masks: a camera over the inside keeps its pixels ----
    {
        Corridor c = make_corridor(40, 60, 5);
        const int64_t n = c.ds.points.num();
        std::vector<uint8_t> inside((size_t)n);
        for (int64_t i = 0; i < n; i++) inside[(size_t)i] = c.ds.points.xyz[(size_t)i * 3] < 20.0;
        const fs::path dir = fs::temp_directory_path() / "spirula_region_masks_test";
        const std::vector<std::string> files =
            write_region_masks(c.ds, c.ds.points.xyz.data(), n, inside.data(), dir.string(), false);
        auto kept_in = [&](const std::vector<std::string>& set, int cam) {
            int w = 0, h = 0, ch = 0;
            stbi_uc* img = stbi_load(set[(size_t)cam].c_str(), &w, &h, &ch, 1);
            if (!img) return -1.0;
            double k = 0;
            for (int i = 0; i < w * h; i++) k += img[i] != 0;
            stbi_image_free(img);
            return k / (w * h);
        };
        auto kept = [&](int cam) { return kept_in(files, cam); };
        check(files.size() == 40 && kept(5) > 0.99, "region masks: a camera over the inside keeps everything");
        // Pixels no point falls near stay kept: here the strip's two sides.
        check(kept(35) < 0.35, "region masks: one over the outside keeps little but what nothing covers");
        check(kept(20) > 0.3 && kept(20) < 0.9, "region masks: one over the boundary keeps its inside half and a margin");

        // Given the region, the sides are rays that miss it.
        BoxRegion box;
        box.center[0] = 9.5;
        box.half[0] = 10.5;
        box.half[1] = 1.5;
        box.half[2] = 0.2;
        const std::vector<std::string> boxed = write_region_masks(
            c.ds, c.ds.points.xyz.data(), n, inside.data(), (dir / "boxed").string(), false, nullptr, &box);
        // The strip is 300 of the 500 px across, plus a 12 px margin a side.
        const double k5 = kept_in(boxed, 5);
        check(k5 > 0.6 && k5 < 0.75, "region masks: a camera over the inside keeps the region and a margin, not "
                                     "the sides (" + std::to_string(k5) + ")");
        check(kept_in(boxed, 35) == 0.0, "region masks: one wholly past the region keeps nothing");
        std::error_code ec;
        fs::remove_all(dir, ec);
    }

    // ---- max-images mode and the other sources ----
    {
        PartitionOptions o2;
        o2.max_images = 15;
        ScenePartition p2 = partition_scene(ds, cov, o2);
        bool sizes_ok = p2.num_parts >= 4;
        for (int k = 0; k < p2.num_parts; k++) sizes_ok = sizes_ok && p2.core_count(k) <= 15;
        check(sizes_ok, "max-images mode: " + std::to_string(p2.num_parts) + " parts, all <= 15");
        check(label_changes(p2.frame_label) == p2.num_parts - 1,
              "every part is one contiguous run of the corridor");

        PartitionOptions o3 = opt;
        o3.source = CovisibilitySource::Projection;
        Covisibility c3 = build_covisibility(ds, &corr.tracks, o3);
        check(c3.source == CovisibilitySource::Projection && c3.has_tracks(),
              "projection covisibility from the seed cloud");
        o3.max_projected_points = 101;
        const auto shared = build_covisibility(ds, nullptr, o3);
        check(shared.num_points() == ds.points.num() && shared.beg.size() <= 102 &&
                  shared.point_row.size() == (size_t)ds.points.num(), "projection observer rows are shared within the requested sample budget");
        check(shared.begin(0) == shared.begin(1) && shared.end(0) == shared.end(1),
              "unsampled points reuse their sampled point's observer span");
        Covisibility expanded = shared;
        expanded.point_row.clear();
        expanded.beg.assign(1, 0);
        expanded.frame.clear();
        for (int64_t i = 0; i < ds.points.num(); i++) {
            expanded.frame.insert(expanded.frame.end(), shared.frame.begin() + shared.begin(i),
                                  shared.frame.begin() + shared.end(i));
            expanded.beg.push_back((int64_t)expanded.frame.size());
        }
        const auto compact_partition = partition_scene(ds, shared, o3);
        const auto expanded_partition = partition_scene(ds, expanded, o3);
        check(compact_partition.point_label == expanded_partition.point_label &&
                  compact_partition.ring == expanded_partition.ring,
              "shared projection rows preserve partition ownership and camera rings");
        ScenePartition p3 = partition_scene(ds, c3, o3);
        check(p3.num_parts == 2 && label_changes(p3.frame_label) == 1,
              "projection source cuts the corridor once too");

        PartitionOptions o4 = opt;
        o4.source = CovisibilitySource::Proximity;
        Covisibility c4 = build_covisibility(ds, nullptr, o4);
        check(c4.source == CovisibilitySource::Proximity && !c4.has_tracks(),
              "proximity covisibility needs no points");
        ScenePartition p4 = partition_scene(ds, c4, o4);
        check(p4.num_parts == 2 && label_changes(p4.frame_label) == 1,
              "proximity source cuts the corridor once");
        check(!p4.ring[0].empty() && !p4.ring[1].empty(), "proximity rings from graph edges");

        ParsedDataset bare = ds;
        bare.points = ColmapPoints3D{};
        Covisibility c5 = build_covisibility(bare, nullptr, opt);
        check(c5.source == CovisibilitySource::Proximity, "auto falls through to proximity");
    }

    {
        auto layered = make_corridor(2, 1, 2).ds;
        layered.c2w[15] = 0;
        layered.points.xyz.clear();
        for (int i = 0; i < 4; i++) {
            const double x = -0.3 + 0.2 * i;
            layered.points.xyz.insert(layered.points.xyz.end(), {x, 0, 3, x + 0.001, 0, 3, 2 * x, 0, 1});
        }
        layered.points.xyz.insert(layered.points.xyz.end(), {0, 0.3, 3, 0.002, 0.6, 1});
        layered.points.rgb.resize(layered.points.xyz.size());
        PartitionOptions depth_options;
        depth_options.source = CovisibilitySource::Projection;
        const auto frustum = build_covisibility(layered, nullptr, depth_options);
        depth_options.projection_depth_slack = 0.1f;
        const auto occluded = build_covisibility(layered, nullptr, depth_options);
        bool walls = occluded.source == CovisibilitySource::Projection;
        for (int i = 0; i < 4; i++) {
            walls &= frustum.end(i * 3 + 2) - frustum.begin(i * 3 + 2) == 2;
            walls &= occluded.end(i * 3) - occluded.begin(i * 3) == 2;
            walls &= occluded.end(i * 3 + 2) == occluded.begin(i * 3 + 2);
        }
        check(walls, "projected depth rejects a back wall while retaining supported foreground");
        check(occluded.end(12) - occluded.begin(12) == 2 && occluded.end(13) - occluded.begin(13) == 2,
              "one sparse foreground point does not erase nearby background in its coarse cell");
        depth_options.source = CovisibilitySource::Auto;
        const auto tracked = build_covisibility(ds, &corr.tracks, depth_options);
        check(tracked.source == CovisibilitySource::Tracks && tracked.frame == cov.frame,
              "projected depth filtering leaves real observation tracks unchanged");
        const auto depth_partition = partition_scene(layered, occluded, depth_options);
        fs::path depth_path = fs::temp_directory_path() / "spirula_projection_depth_test.json";
        write_partition(depth_partition, depth_path.string(), "synthetic");
        check(read_partition(depth_path.string()).options.projection_depth_slack == 0.1f,
              "projected depth option round-trips with the partition");
        fs::remove(depth_path);
        depth_path.replace_extension(".bin");
        fs::remove(depth_path);
        depth_options.projection_depth_slack = -1;
        bool invalid = false;
        try { build_covisibility(layered, nullptr, depth_options); }
        catch (const std::runtime_error&) { invalid = true; }
        check(invalid, "invalid projected depth slack is rejected");
    }

    // ---- files, and applying a part ----
    const fs::path tmp = fs::temp_directory_path() / "spirula_scene_partition_test";
    auto no_partials = [](const fs::path& output) {
        const auto prefix = output.filename().string() + ".partial-";
        for (const auto& entry : fs::directory_iterator(output.parent_path()))
            if (entry.path().filename().string().rfind(prefix, 0) == 0) return false;
        return true;
    };
    std::error_code ec;
    fs::remove_all(tmp, ec);
    fs::create_directories(tmp / "dataset", ec);
    const std::string json = (tmp / "dataset" / "partition.json").string();
    write_partition(p, json, (tmp / "dataset").string());
    {
        std::string dataset;
        ScenePartition back = read_partition(json, &dataset);
        check(dataset == (tmp / "dataset").string(), "dataset path recorded");
        check(back.num_parts == p.num_parts && back.frame_label == p.frame_label &&
                  back.frame_names == p.frame_names && back.ring == p.ring,
              "frames round-trip through json");
        check(back.point_label == p.point_label && back.part_points == p.part_points,
              "point tables round-trip through bin");
        auto same = [](const std::vector<float>& a, const std::vector<float>& b) {
            return a.size() == b.size() &&
                   std::memcmp(a.data(), b.data(), a.size() * sizeof(float)) == 0;
        };
        check(back.field && same(back.field->seeds, p.field->seeds) &&
                  same(back.field->nodes, p.field->nodes),
              "field round-trips through bin");
        check(back.frame_centers == p.frame_centers, "camera centres round-trip through bin");
        check(back.options.parts == 2 && back.source == CovisibilitySource::Tracks,
              "options and source round-trip");

        // The part holding camera 0, so a renamed camera 0 below is its loss.
        ParsedDataset part0 = ds;
        PartitionApplied a;
        check(apply_partition(part0, back, L, a), "apply the first camera's part");
        const std::vector<int32_t> want = back.frames_of(L);
        check(a.frames_after == (int64_t)want.size() && a.core == back.core_count(L) &&
                  a.ring == (int64_t)back.ring[(size_t)L].size() && a.missing == 0,
              "a part keeps exactly core + ring frames");
        bool names_ok = part0.num_cameras == (int64_t)want.size();
        for (size_t i = 0; names_ok && i < want.size(); i++)
            names_ok = fs::path(part0.image_filenames[i]).filename().string() ==
                       back.frame_names[(size_t)want[i]];
        check(names_ok, "kept frames are the part's, in order");
        check(part0.c2w.size() == (size_t)part0.num_cameras * 12 &&
                  part0.intrins.size() == (size_t)part0.num_cameras * 4 &&
                  part0.train_indices.size() == (size_t)part0.num_cameras,
              "per-frame arrays and train indices follow");
        check(part0.points.num() == (int64_t)back.part_points[(size_t)L].size() &&
                  part0.points.rgb.size() == part0.points.xyz.size(),
              "seed points are the part's");
        // A frame the dataset lacks is counted, not fatal.
        ParsedDataset fewer = ds;
        fewer.image_filenames[0] = "/nowhere/renamed.jpg";
        PartitionApplied b;
        apply_partition(fewer, back, L, b);
        check(b.missing == 1 && b.frames_after == a.frames_after - 1, "a renamed frame is reported missing");
        check(!apply_partition(fewer, back, 7, b), "out-of-range part is refused");
    }

    // ---- merge ----
    {
        // Part 0's model covers the whole corridor (as a ring-trained model
        // would), part 1's too; each keeps only its own side. Part 1 was
        // trained centred at x=30 (scene_transform.json), degree 1 vs 0.
        const fs::path r0 = tmp / "outputs" / "run_a" / "step-000000200.ckpt";
        const fs::path r1 = tmp / "outputs" / "run_b" / "step-000000300.ckpt";
        fs::create_directories(r0, ec);
        fs::create_directories(r1, ec);
        SplatCloud c0 = splats_along_x(-2, 62, 640, 0, 0.0f);
        SplatCloud c1 = splats_along_x(-2 - 30, 62 - 30, 640, 1, 0.5f);
        write_splat_ply(c0, (r0 / "splat.ply").string());
        write_splat_ply(c1, (r1 / "splat.ply").string());
        write_text(tmp / "outputs" / "run_a" / "config.json",
                   "{\"partition\": " + json_quote(json) + ", \"partition_part\": 0}");
        write_text(tmp / "outputs" / "run_b" / "config.json",
                   "{\"partition\": " + json_quote(json) + ", \"partition_part\": 1}");
        write_text(tmp / "outputs" / "run_stale" / "config.json",
                   "{\"partition\": " + json_quote(json) + ", \"partition_part\": 1}");
        SceneTransform T;
        T.t[0] = -30.0;
        const double centre[3] = {30, 0, 0};
        write_text(tmp / "outputs" / "run_b" / "scene_transform.json",
                   scene_transform_json(T, "median", centre));
        SceneTransform back_T;
        check(read_scene_transform_json((tmp / "outputs" / "run_b" / "scene_transform.json").string(), back_T) &&
                  std::fabs(back_T.t[0] + 30.0) < 1e-9 && std::fabs(back_T.scale - 1.0) < 1e-12,
              "scene_transform.json reads back");

        std::vector<std::string> runs = find_partition_runs((tmp / "outputs").string(), json, 2);
        check(runs.size() == 2 && fs::path(runs[0]).filename() == "run_a" &&
                  fs::path(runs[1]).filename() == "run_b",
              "runs found by their config.json; the one without a model is skipped");

        MergeStats st;
        SplatCloud merged = merge_partition_splats(p, runs, st);
        const auto streamed_path = (tmp / "streamed.ply").string();
        MergeStats streamed_stats;
        merge_partition_splats_file(p, runs, streamed_path, streamed_stats);
        const auto streamed = read_splat_ply(streamed_path);
        check(streamed.means == merged.means && streamed.features_sh == merged.features_sh &&
                  streamed.quats == merged.quats && streamed.scales == merged.scales &&
                  streamed.opacities == merged.opacities && streamed.features_dc == merged.features_dc &&
                  streamed_stats.total == merged.num, "streamed merge matches the in-memory merge");
        write_text(streamed_path + ".partial", "unrelated temporary file");
        std::atomic<int> ready{0};
        std::array<std::exception_ptr, 2> errors;
        std::vector<std::thread> workers;
        for (int worker = 0; worker < 2; worker++) workers.emplace_back([&, worker] {
            int logged_parts = 0;
            MergeStats local;
            try {
                merge_partition_splats_file(p, runs, streamed_path, local, [&](const std::string&) {
                    if (++logged_parts != p.num_parts) return;
                    ready.fetch_add(1);
                    while (ready.load() < 2) std::this_thread::yield();
                });
            } catch (...) { errors[(size_t)worker] = std::current_exception(); }
        });
        for (auto& worker : workers) worker.join();
        std::ifstream sentinel(streamed_path + ".partial");
        std::string sentinel_text;
        std::getline(sentinel, sentinel_text);
        check(!errors[0] && !errors[1] && read_splat_ply(streamed_path).features_sh == merged.features_sh &&
                  sentinel_text == "unrelated temporary file" && no_partials(streamed_path),
              "concurrent merges publish complete files and preserve unrelated temporary files");
        sentinel.close();
        fs::remove(streamed_path + ".partial");
        SplatCloud empty;
        empty.sh_degree = 2;
        empty.sb_lobes = 1;
        const auto empty_path = (tmp / "empty.ply").string();
        const auto empty_output = (tmp / "empty-merged.ply").string();
        write_splat_ply(empty, empty_path);
        MergeStats empty_stats;
        const auto empty_memory = merge_partition_splats(p, {empty_path}, empty_stats);
        merge_partition_splats_file(p, {empty_path}, empty_output, empty_stats);
        const auto empty_file = splat_ply_shape(empty_output);
        check(empty_memory.num == 0 && empty_file.num == 0 && empty_memory.basis() == empty.basis() &&
                  empty_file.basis() == empty.basis(), "empty input PLYs preserve their SH and spherical-beta basis");
        int logged = 0;
        bool changed_rejected = false;
        try {
            merge_partition_splats_file(p, runs, streamed_path, streamed_stats, [&](const std::string&) {
                if (++logged != 2) return;
                auto changed0 = c0, changed1 = c1;
                changed0.primitive = changed1.primitive = "mip";
                write_splat_ply(changed0, (r0 / "splat.ply").string());
                write_splat_ply(changed1, (r1 / "splat.ply").string());
            });
        } catch (const std::exception&) { changed_rejected = true; }
        check(changed_rejected && read_splat_ply(streamed_path).primitive.empty() &&
                  no_partials(streamed_path),
              "primitive changes between merge passes preserve the previous output");
        write_splat_ply(c0, (r0 / "splat.ply").string());
        write_splat_ply(c1, (r1 / "splat.ply").string());
        logged = 0;
        changed_rejected = false;
        try {
            merge_partition_splats_file(p, runs, streamed_path, streamed_stats, [&](const std::string&) {
                if (++logged != 2) return;
                auto changed = c0;
                changed.sh_degree = 2;
                changed.features_sh.assign((size_t)changed.num * changed.coeffs() * 3, 0);
                write_splat_ply(changed, (r0 / "splat.ply").string());
            });
        } catch (const std::exception&) { changed_rejected = true; }
        check(changed_rejected && read_splat_ply(streamed_path).features_sh == streamed.features_sh &&
                  no_partials(streamed_path),
              "basis growth between merge passes is rejected before padding or publication");
        write_splat_ply(c0, (r0 / "splat.ply").string());
        auto lobed = c1;
        lobed.sb_lobes = 1;
        lobed.features_sh.resize((size_t)lobed.num * 15);
        for (int64_t i = 0; i < lobed.num; i++) {
            std::copy_n(c1.features_sh.data() + i * 9, 9, lobed.features_sh.data() + i * 15);
            sb::dark_lobe(lobed.features_sh.data() + i * 15 + 9);
        }
        write_splat_ply(lobed, (r1 / "splat.ply").string());
        const auto lobed_merge = merge_partition_splats(p, runs, streamed_stats);
        merge_partition_splats_file(p, runs, streamed_path, streamed_stats);
        const auto lobed_stream = read_splat_ply(streamed_path);
        check(lobed_stream.sb_lobes == 1 && lobed_stream.features_sh == lobed_merge.features_sh,
              "streamed mixed SH and spherical-beta bases preserve rows and dark padding");
        write_splat_ply(c1, (r1 / "splat.ply").string());
        bool refused = false;
        try { merge_partition_splats_file(p, runs, (r0 / "splat.ply").string(), streamed_stats); }
        catch (const std::exception&) { refused = true; }
        check(refused && read_splat_ply((r0 / "splat.ply").string()).num == c0.num,
              "streamed merge refuses to overwrite an input");
        check(st.parts[0].read == 640 && st.parts[1].read == 640, "both models read");
        check(st.parts[0].kept + st.parts[1].kept == merged.num && merged.num > 0,
              "merged count is the sum of what each part kept");
        check(st.parts[0].kept > 200 && st.parts[1].kept > 200 && merged.num < 700,
              "each side kept roughly half (" + std::to_string(st.parts[0].kept) + " + " +
                  std::to_string(st.parts[1].kept) + ")");
        // The merge orients each splat toward the nearest camera of its part
        // (data/RegionProgram.h splat_normal); the same query here.
        bool sides_ok = true;
        int64_t from1 = 0;
        for (int64_t i = 0; i < merged.num; i++) {
            const float x = merged.means[(size_t)i * 3], tag = merged.means[(size_t)i * 3 + 1];
            const int from = tag > 0.25f ? 1 : 0;
            double best = 1e300, to[3] = {0, 0, 1};
            for (int32_t f : p.frames_of(from)) {
                const float* c = &p.frame_centers[(size_t)f * 3];
                const double d[3] = {c[0] - x, c[1] - tag, c[2]};
                const double d2 = d[0] * d[0] + d[1] * d[1] + d[2] * d[2];
                if (d2 < best) { best = d2; for (int k = 0; k < 3; k++) to[k] = d[k]; }
            }
            double n[3];
            splat_normal(&merged.quats[(size_t)i * 4], &merged.scales[(size_t)i * 3], to, n);
            const double q[3] = {x, tag, 0.0};
            const int owner = p.field->label(q, n);
            from1 += from;
            sides_ok = sides_ok && owner == from;
        }
        check(sides_ok && from1 == st.parts[1].kept,
              "every merged splat lies in the region of the part that made it, "
              "part 1 moved back by its scene transform");
        check(merged.sh_degree == 1 &&
                  merged.features_sh.size() == (size_t)merged.num * 3 * 3,
              "SH padded to the highest degree present");
        // Part 0's padded coefficients are zero, part 1's are its own.
        bool pad_ok = true;
        for (int64_t i = 0; i < merged.num; i++) {
            const bool from = merged.means[(size_t)i * 3 + 1] > 0.25f;
            const float v = merged.features_sh[(size_t)i * 9 + 4];
            pad_ok = pad_ok && (from ? std::fabs(v - 0.04f) < 1e-6f : v == 0.0f);
        }
        check(pad_ok, "padding is zeros, real coefficients survive");
        std::vector<std::string> one = {runs[0], ""};
        SplatCloud half = merge_partition_splats(p, one, st);
        check(half.num == st.parts[0].kept && st.parts[1].ply.empty(), "a missing part is skipped");

        c1.primitive = "mip";
        write_splat_ply(c1, (r1 / "splat.ply").string());
        bool mixed_rejected = false;
        try {
            merge_partition_splats(p, runs, st);
        } catch (const std::runtime_error& e) {
            mixed_rejected = std::string(e.what()).find("cannot merge Mip-trained") != std::string::npos;
        }
        check(mixed_rejected, "parts that retain both render modes cannot write one PLY");

        c0.primitive = "mip";
        write_splat_ply(c0, (r0 / "splat.ply").string());
        const SplatCloud all_mip = merge_partition_splats(p, runs, st);
        check(all_mip.num == merged.num && all_mip.primitive == "mip",
              "a merge of Mip-trained parts retains their render mode");

        const auto part1_frames = p.frames_of(1);
        const float other_x = p.frame_centers[(size_t)part1_frames[part1_frames.size() / 2] * 3];
        SplatCloud clipped = splats_along_x(other_x - 1, other_x + 1, 64, 0, 0.0f);
        clipped.primitive = "mip";
        c1.primitive.clear();
        write_splat_ply(clipped, (r0 / "splat.ply").string());
        write_splat_ply(c1, (r1 / "splat.ply").string());
        const SplatCloud retained_classic = merge_partition_splats(p, runs, st);
        check(st.parts[0].read > 0 && st.parts[0].kept == 0 &&
                  retained_classic.num > 0 && retained_classic.primitive.empty(),
              "a clipped Mip part does not change the retained classic render mode");
    }
    {
        ScenePartition oriented;
        oriented.num_parts = 1;
        oriented.ring.resize(1);
        oriented.frame_label.assign(300, 0);
        for (int i = 0; i < 299; i++)
            oriented.frame_centers.insert(oriented.frame_centers.end(), {(float)(300 - i), 0, 5});
        oriented.frame_centers.insert(oriented.frame_centers.end(), {0, 0, -1});
        const float xyz[6] = {0, 0, 0, 0, 0, 0};
        const float dirs[6] = {0, 0, 1, 0, 0, -1};
        const int32_t labels[2] = {0, 1};
        oriented.field = std::make_shared<LabelField>(LabelField::build(xyz, labels, dirs, 2));
        auto c = splats_along_x(-1, 1, 1, 0, 0);
        c.scales[2] = -3;
        const auto input = (tmp / "oriented.ply").string();
        write_splat_ply(c, input);
        MergeStats stats;
        const auto merged = merge_partition_splats(oriented, {input}, stats);
        check(merged.num == 0, "merge orientation finds nearest camera beyond index 255 after spatial reordering");
        const auto blocked = tmp / "blocked.ply";
        fs::create_directory(blocked);
        bool failed = false;
        try { merge_partition_splats_file(oriented, {input}, blocked.string(), stats); }
        catch (const std::exception&) { failed = true; }
        check(failed && fs::is_directory(blocked) && no_partials(blocked),
              "failed merge publication removes its partial file and preserves the destination");
    }
    {
        ScenePartition tied;
        tied.num_parts = 2;
        tied.ring.resize(2);
        tied.frame_label = {0, 0};
        tied.frame_centers = {-1, 0, 0, 1, 0, 0};
        const float xyz[6] = {0, 0, 0, 0, 0, 0};
        const float dirs[6] = {-1, 0, 0, 1, 0, 0};
        const int32_t labels[2] = {0, 1};
        tied.field = std::make_shared<LabelField>(LabelField::build(xyz, labels, dirs, 2));
        auto c = splats_along_x(-1, 1, 1, 0, 0);
        c.scales[0] = -4;
        const auto input = (tmp / "camera-tie.ply").string();
        const auto output = (tmp / "camera-tie-merged.ply").string();
        write_splat_ply(c, input);
        for (int reverse = 0; reverse < 2; reverse++) {
            MergeStats stats;
            const auto merged = merge_partition_splats(tied, {input}, stats);
            merge_partition_splats_file(tied, {input}, output, stats);
            check(merged.num == 1 - reverse && read_splat_ply(output).num == merged.num,
                  "symmetric camera ties use training BVH order in both merge paths");
            std::swap(tied.frame_centers[0], tied.frame_centers[3]);
        }
    }
    {
        const auto directory = tmp / "camera-metadata";
        fs::create_directory(directory);
        ScenePartition p;
        p.num_parts = 2;
        p.ring.resize(2);
        p.frame_label = {0, 0};
        p.frame_centers = {-1, 0, 0, 1, 0, 0};
        const float xyz[6] = {0, 0, 0, 0, 0, 0};
        const float dirs[6] = {-1, 0, 0, 1, 0, 0};
        const int32_t labels[2] = {0, 1};
        p.field = std::make_shared<LabelField>(LabelField::build(xyz, labels, dirs, 2));
        auto c = splats_along_x(-1, 1, 1, 0, 0);
        c.scales[0] = -4;
        const auto old_ply = (directory / "old.ply").string();
        const auto new_ply = (directory / "new.ply").string();
        const auto output = (directory / "merged.ply").string();
        auto marked = [&](const std::string& path, const SplatCloud& cloud, const std::string& hash) {
            BufferedOutputFile file(path, std::ios::binary);
            file << splat_ply_header(cloud.num, cloud.basis(), cloud.primitive, false, hash);
            std::vector<float> buffer;
            write_splat_ply_rows(cloud, file, buffer);
            file.close();
            if (!file) throw std::runtime_error("cannot write camera metadata fixture");
        };
        auto rejected = [&](const auto& action) {
            try { action(); } catch (const std::exception&) { return true; }
            return false;
        };
        RegionCameras left, right;
        const int32_t camera_label = 0;
        left.field = LabelField::build(p.frame_centers.data(), &camera_label, nullptr, 1, 4);
        right.field = LabelField::build(p.frame_centers.data() + 3, &camera_label, nullptr, 1, 4);
        const auto left_hash = save_region_cameras(directory.string(), left);
        const auto right_hash = save_region_cameras(directory.string(), right);
        marked(old_ply, c, left_hash);
        marked(new_ply, c, right_hash);
        MergeStats stats;
        check(left_hash != right_hash && save_region_cameras(directory.string(), left) == left_hash &&
                  merge_partition_splats(p, {old_ply}, stats).num == 1 &&
                  merge_partition_splats(p, {new_ply}, stats).num == 0,
              "checkpoint camera hashes retain their own uploaded camera set across later saves");
        merge_partition_splats_file(p, {old_ply}, output, stats);
        check(read_splat_ply(output).region_camera_hash.empty(), "merged exports discard checkpoint camera references");
        write_splat_ply(read_splat_ply(old_ply), output);
        check(read_splat_ply(output).region_camera_hash.empty(), "edited exports discard checkpoint camera references");
        auto moved = left;
        moved.train_from_dataset.scale = 2;
        const double rotation[9] = {0, -1, 0, 1, 0, 0, 0, 0, 1};
        std::copy(rotation, rotation + 9, moved.train_from_dataset.R);
        std::copy_n(std::array<double, 3>{5, 6, 7}.data(), 3, moved.train_from_dataset.t);
        const float camera[3] = {5, 4, 7};
        moved.field = LabelField::build(camera, &camera_label, nullptr, 1, 4);
        Sim3 transform;
        transform.s = 2;
        std::copy(rotation, rotation + 9, transform.R);
        std::copy(moved.train_from_dataset.t, moved.train_from_dataset.t + 3, transform.t);
        auto transformed = c;
        transform_splats(transformed, transform);
        marked(new_ply, transformed, save_region_cameras(directory.string(), moved));
        SceneTransform conflicting;
        conflicting.scale = 17;
        const double center[3] = {0, 0, 0};
        write_text(directory / "scene_transform.json", scene_transform_json(conflicting, "none", center));
        const auto restored = merge_partition_splats(p, {new_ply}, stats);
        merge_partition_splats_file(p, {new_ply}, output, stats);
        check(restored.num == 1 && restored.means == c.means && read_splat_ply(output).means == c.means,
              "checkpoint metadata restores its own scene frame instead of mutable run metadata");
        std::vector<float> many;
        for (int i = 300; i >= 1; i--) many.insert(many.end(), {(float)i, 0, 0});
        many.insert(many.end(), {-0.5f, 0, 0});
        std::vector<int32_t> many_labels(301, 0);
        RegionCameras large;
        large.field = LabelField::build(many.data(), many_labels.data(), nullptr, 301, 4);
        const auto large_hash = save_region_cameras(directory.string(), large);
        const auto loaded = read_region_cameras(old_ply, large_hash);
        const double origin[3] = {0, 0, 0};
        const auto nearest = loaded.field.nearest_seed(origin);
        check(nearest >= 0 && loaded.field.seeds[(size_t)nearest * 4] == -0.5f &&
                  loaded.field.seeds.size() == large.field.seeds.size() &&
                  std::memcmp(loaded.field.seeds.data(), large.field.seeds.data(), large.field.seeds.size() * sizeof(float)) == 0 &&
                  loaded.field.nodes == large.field.nodes,
              "camera metadata preserves reordered BVH indices beyond 255 cameras");
        marked(new_ply, c, left_hash);
        const auto blob = directory / ("roi-cameras-" + left_hash + ".bin");
        fs::rename(blob, blob.string() + ".saved");
        check(read_splat_ply(new_ply).num == 1 && rejected([&] {
                  merge_partition_splats_file(p, {new_ply}, output, stats);
              }) && read_splat_ply(output).num == 1,
              "missing companion metadata rejects merging but leaves ordinary viewing and published output intact");
        fs::rename(blob.string() + ".saved", blob);
        BufferedInputFile source(blob, std::ios::binary);
        const std::string bytes((std::istreambuf_iterator<char>(source)), {});
        source.close();
        write_text(blob, "tampered");
        check(rejected([&] { merge_partition_splats(p, {new_ply}, stats); }) &&
                  rejected([&] { save_region_cameras(directory.string(), left); }),
              "tampered camera metadata never silently falls back or overwrites a hash collision");
        write_text(blob, bytes);
        std::string malformed = bytes;
        const uint64_t huge = UINT64_MAX;
        std::memcpy(malformed.data() + 120, &huge, sizeof(huge));
        Sha256 digest;
        digest.update(reinterpret_cast<const uint8_t*>(malformed.data()), malformed.size());
        const auto malformed_hash = digest.hex();
        write_text(directory / ("roi-cameras-" + malformed_hash + ".bin"), malformed);
        check(rejected([&] { read_region_cameras(new_ply, malformed_hash); }),
              "camera metadata rejects overflowing serialized counts before allocating");
        BufferedInputFile marked_source(new_ply, std::ios::binary);
        std::string invalid_ply((std::istreambuf_iterator<char>(marked_source)), {});
        marked_source.close();
        invalid_ply.replace(invalid_ply.find(left_hash), left_hash.size(), "../escape");
        write_text(new_ply, invalid_ply);
        check(read_splat_ply(new_ply).num == 1 && rejected([&] {
                  merge_partition_splats(p, {new_ply}, stats);
              }), "malformed PLY camera references are ignored by viewing but rejected by merging");
        auto invalid = left;
        invalid.field.seeds[0] = std::numeric_limits<float>::quiet_NaN();
        check(rejected([&] { save_region_cameras(directory.string(), invalid); }) &&
                  rejected([&] { read_region_cameras(new_ply, "../escape"); }),
              "invalid camera coordinates and arbitrary metadata paths are rejected");
        invalid = large;
        const uint32_t root = 0;
        std::memcpy(&invalid.field.nodes[3], &root, sizeof(root));
        check(rejected([&] { save_region_cameras(directory.string(), invalid); }),
              "camera metadata rejects cyclic BVHs");
        bool changed = false;
        check(rejected([&] {
                  merge_partition_splats_file(p, {old_ply}, output, stats, [&](const std::string&) {
                      if (!changed) { marked(old_ply, c, right_hash); changed = true; }
                  });
              }) && read_splat_ply(output).num == 1 && no_partials(output),
              "changing a checkpoint camera reference between merge passes preserves the published output");
    }
    fs::remove_all(tmp, ec);

    if (g_failures == 0) std::printf("scene_partition_test: OK\n");
    return g_failures ? 1 : 0;
}
