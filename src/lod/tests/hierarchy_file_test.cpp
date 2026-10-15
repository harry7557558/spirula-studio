// hierarchy_file_test -- tests for HierarchyFile.h/cpp (.slod serialization,
// deserialization, metadata-only mode, and corruption detection).

#include "lod/HierarchyBuilder.h"
#include "lod/HierarchyFile.h"
#include "lod/HierarchyNode.h"
#include "checkpoint/SplatPly.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

int g_failures = 0;

void check(bool ok, const std::string& what) {
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what.c_str());
    if (!ok) g_failures++;
}

spirula::SplatCloud make_test_cloud(int64_t n, unsigned seed) {
    spirula::SplatCloud c;
    c.num = n;
    c.sh_degree = 1;
    c.sb_lobes = 0;

    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> dist(-10.0f, 10.0f);

    c.means.resize((size_t)(n * 3));
    c.quats.resize((size_t)(n * 4));
    c.scales.resize((size_t)(n * 3));
    c.opacities.resize((size_t)n);
    c.features_dc.resize((size_t)(n * 3));
    c.features_sh.resize((size_t)(n * c.coeffs() * 3));

    for (int64_t i = 0; i < n; i++) {
        for (int j = 0; j < 3; j++) {
            c.means[(size_t)(i * 3 + j)] = dist(rng);
            c.scales[(size_t)(i * 3 + j)] = dist(rng) * 0.1f;
            c.features_dc[(size_t)(i * 3 + j)] = dist(rng) * 0.5f;
        }
        c.quats[(size_t)(i * 4 + 0)] = 1.0f;
        c.quats[(size_t)(i * 4 + 1)] = 0.0f;
        c.quats[(size_t)(i * 4 + 2)] = 0.0f;
        c.quats[(size_t)(i * 4 + 3)] = 0.0f;
        c.opacities[(size_t)i] = dist(rng);

        for (int64_t k = 0; k < c.coeffs() * 3; k++) {
            c.features_sh[(size_t)(i * c.coeffs() * 3 + k)] = dist(rng) * 0.2f;
        }
    }
    return c;
}

void test_roundtrip() {
    std::printf("--- Test: SLOD Serialization & Deserialization Roundtrip ---\n");

    const int64_t N = 250;
    spirula::SplatCloud orig_cloud = make_test_cloud(N, 101);

    spirula::LodBuildConfig cfg;
    cfg.max_leaf_size = 16;
    cfg.max_parent_gaussians = 4;

    spirula::GaussianHierarchy orig_hier = spirula::build_gaussian_hierarchy(orig_cloud, cfg);

    std::string test_file = "/tmp/test_hierarchy_roundtrip.slod";
    spirula::write_slod(orig_hier, test_file);

    check(fs::exists(test_file), "SLOD file successfully created on disk");

    // ---- Validation ----
    std::string val_err = spirula::validate_slod(test_file);
    check(val_err.empty(), "SLOD file validation passes: " + val_err);

    // ---- Metadata-only read ----
    spirula::GaussianHierarchy meta_hier = spirula::read_slod(test_file, /*metadata_only=*/true);
    check(meta_hier.num_splats == orig_hier.num_splats, "metadata read has correct splat count");
    check(meta_hier.nodes.size() == orig_hier.nodes.size(), "metadata read has correct node count");
    check(meta_hier.means.empty(), "metadata-only read leaves splat payload unallocated");

    // ---- Full read ----
    spirula::GaussianHierarchy full_hier = spirula::read_slod(test_file, /*metadata_only=*/false);
    check(full_hier.num_splats == orig_hier.num_splats, "full read has correct splat count");
    check(full_hier.nodes.size() == orig_hier.nodes.size(), "full read has correct node count");
    check(full_hier.means.size() == orig_hier.means.size(), "means buffer size matches");
    check(full_hier.features_sh.size() == orig_hier.features_sh.size(), "features_sh buffer size matches");

    // Bit-exact comparison of splats
    bool splats_exact = true;
    for (size_t i = 0; i < orig_hier.means.size(); i++) {
        if (orig_hier.means[i] != full_hier.means[i]) splats_exact = false;
    }
    for (size_t i = 0; i < orig_hier.opacities.size(); i++) {
        if (orig_hier.opacities[i] != full_hier.opacities[i]) splats_exact = false;
    }
    check(splats_exact, "Gaussian leaf attributes roundtrip bit-exactly");

    // Bit-exact comparison of nodes
    bool nodes_exact = true;
    for (size_t i = 0; i < orig_hier.nodes.size(); i++) {
        const auto& n1 = orig_hier.nodes[i];
        const auto& n2 = full_hier.nodes[i];
        if (n1.error_radius != n2.error_radius) nodes_exact = false;
        if (n1.parent != n2.parent || n1.child_left != n2.child_left) nodes_exact = false;
        if (n1.summary.size() != n2.summary.size()) nodes_exact = false;
    }
    check(nodes_exact, "Hierarchy node topology and error radii roundtrip bit-exactly");

    fs::remove(test_file);
}

void test_corruption_detection() {
    std::printf("\n--- Test: File Corruption & Truncation Detection ---\n");

    const int64_t N = 100;
    spirula::SplatCloud orig_cloud = make_test_cloud(N, 202);
    spirula::GaussianHierarchy hier = spirula::build_gaussian_hierarchy(orig_cloud);

    std::string good_file = "/tmp/test_good.slod";
    spirula::write_slod(hier, good_file);

    // Test 1: Corrupted magic bytes
    {
        std::string bad_magic = "/tmp/test_bad_magic.slod";
        fs::copy_file(good_file, bad_magic, fs::copy_options::overwrite_existing);

        std::fstream f(bad_magic, std::ios::in | std::ios::out | std::ios::binary);
        char bad[4] = {'B', 'A', 'A', 'D'};
        f.write(bad, 4);
        f.close();

        std::string err = spirula::validate_slod(bad_magic);
        check(!err.empty() && err.find("magic") != std::string::npos,
              "corrupted magic bytes properly rejected: " + err);
        fs::remove(bad_magic);
    }

    // Test 2: File truncated to half size
    {
        std::string trunc_file = "/tmp/test_trunc.slod";
        std::ifstream src(good_file, std::ios::binary);
        std::ofstream dst(trunc_file, std::ios::binary);

        std::vector<char> buf(fs::file_size(good_file) / 2);
        src.read(buf.data(), (std::streamsize)buf.size());
        dst.write(buf.data(), (std::streamsize)buf.size());
        src.close();
        dst.close();

        std::string err = spirula::validate_slod(trunc_file);
        check(!err.empty() && err.find("EOF") != std::string::npos,
              "truncated file properly rejected: " + err);
        fs::remove(trunc_file);
    }

    fs::remove(good_file);
}

}  // namespace

int main() {
    test_roundtrip();
    test_corruption_detection();

    if (g_failures == 0) std::printf("hierarchy_file_test: OK\n");
    return g_failures ? 1 : 0;
}

