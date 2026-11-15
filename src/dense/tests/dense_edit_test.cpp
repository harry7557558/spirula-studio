#include "dense/Artifact.h"
#include "dense/Versions.h"
#include "data/DatasetParser.h"

#include <chrono>
#include <cstdio>
#include <fstream>

namespace fs = std::filesystem;
using namespace spirula::dense;

namespace {
int failures = 0;
void check(bool ok, const char* message) {
    std::printf("%s %s\n", ok ? "ok" : "FAIL", message);
    if (!ok) ++failures;
}
void write(const fs::path& path, const std::string& text) {
    std::ofstream(path, std::ios::binary) << text;
}
std::string read(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}
std::string ply(int count) {
    std::string text = "ply\nformat ascii 1.0\nelement vertex " + std::to_string(count) +
        "\nproperty double x\nproperty double y\nproperty double z\n"
        "property uchar red\nproperty uchar green\nproperty uchar blue\nend_header\n";
    for (int i = 0; i < count; i++) text += std::to_string(i) + " 2 3 255 0 0\n";
    return text;
}
void artifact(const fs::path& dir, int count) {
    fs::create_directories(dir);
    write(dir / "roma.ply", ply(count));
    write(dir / "manifest.json", "{\"complete\":true,\"cloud_sha256\":\"obsolete\","
        "\"cloud_bytes\":1,\"statistics\":{\"exported\":99}}");
}
void edit_layout(const fs::path& root, bool generation) {
    const auto dataset = (root / (generation ? "generation" : "legacy")).string();
    const auto dense = fs::path(dataset) / "dense";
    artifact(dense, 3);
    if (generation) {
        artifact(fs::path(dataset) / "work", 3);
        publish_generation(dataset, "run-1", fs::path(dataset) / "work/roma.ply",
                           fs::path(dataset) / "work/manifest.json");
    }
    auto files = artifact_files(dataset);
    check(cloud_versions(dataset).size() == (generation ? 2u : 1u),
          "a single legacy cloud opens directly; published history is discoverable");
    const auto part = files.cloud.parent_path() / "edit.part";
    write(part, ply(2));
    commit_edit(dataset, files, part, 2);
    check(read(files.cloud.parent_path() / "roma.original.ply") == ply(3),
          "the first edit preserves its preceding cloud");
    write(part, ply(1));
    commit_edit(dataset, files, part, 1);
    check(read(files.cloud.parent_path() / "roma.original.ply") == ply(3),
          "later saves preserve the original backup");
    check(!json_parse_file(files.manifest.string()).has("cloud_sha256"),
          "saving no longer signs the dense output");
    write(files.cloud, ply(5));
    check(artifact_complete(dataset) && automatic_seed(dataset) == files.cloud.string(),
          "external edits with a changed byte size remain automatic training seeds");
    const auto relative = fs::relative(files.cloud, dataset).generic_string();
    check(dsparse::read_seed_pointcloud(dataset, relative).num() == 5,
          "training reads the externally edited point count");
    write(dense / "roma.ply", ply(7));
    check(resolve_seed_path(dataset, "dense/roma.ply") == (dense / "roma.ply").string() &&
          dsparse::read_seed_pointcloud(dataset, "dense/roma.ply").num() == 7,
          "an explicit compatibility-file edit is not redirected to a generation");
    if (generation) {
        write(dense / "current.json", "{\"version\":1,\"generation\":\"run-1\",\"manifest_sha256\":\"stale\"}");
        check(artifact_files(dataset).cloud == files.cloud && artifact_complete(dataset),
              "legacy manifest hashes are ignored");
    }
    write(dense / "edited.ply", ply(4));
    const auto versions = cloud_versions(dataset);
    check(std::any_of(versions.begin(), versions.end(), [](const auto& v) { return v.relative == "edited.ply"; }),
          "saved PLY versions are listed");
    const auto unrelated = fs::path(dataset) / "outside.ply";
    write(unrelated, ply(9));
    bool refused = false;
    try { delete_cloud_version(dataset, "../outside.ply"); } catch (const std::exception&) { refused = true; }
    check(refused && fs::exists(unrelated), "version deletion refuses paths outside the version list");
    std::error_code link_error;
    fs::create_symlink(unrelated, dense / "linked.ply", link_error);
    if (!link_error) {
        delete_cloud_version(dataset, "linked.ply");
        check(fs::exists(unrelated) && !fs::exists(dense / "linked.ply"),
              "deleting a linked version removes the link, not its target");
    }
    delete_cloud_version(dataset, "edited.ply");
    check(!fs::exists(dense / "edited.ply") && fs::exists(files.cloud),
          "deleting a saved version leaves the current cloud untouched");
    const auto active = fs::relative(files.cloud, dense).generic_string();
    delete_cloud_version(dataset, active);
    check(!fs::exists(files.cloud) && artifact_complete(dataset),
          "deleting the current version selects a remaining cloud");
    if (generation)
        check(read(dense / "roma.ply") == ply(7), "deleting a generation preserves an independently edited compatibility export");
    for (auto remaining = cloud_versions(dataset); !remaining.empty(); remaining = cloud_versions(dataset))
        delete_cloud_version(dataset, remaining.front().relative);
    check(cloud_versions(dataset).empty() && automatic_seed(dataset).empty(),
          "deleting the last version removes the automatic dense seed");
}
void generation_versions(const fs::path& root) {
    const auto dataset = (root / "history").string();
    artifact(fs::path(dataset) / "work", 2);
    const auto first = publish_generation(dataset, "first", fs::path(dataset) / "work/roma.ply",
                                           fs::path(dataset) / "work/manifest.json");
    check(cloud_versions(dataset).size() == 1, "one generation and its compatibility export count as one version");
    artifact(fs::path(dataset) / "work", 3);
    const auto second = publish_generation(dataset, "second", fs::path(dataset) / "work/roma.ply",
                                            fs::path(dataset) / "work/manifest.json");
    check(cloud_versions(dataset).size() == 2, "earlier reconstruction generations are selectable");
    delete_cloud_version(dataset, "generations/first/roma.ply");
    check(!fs::exists(first.cloud) && fs::exists(second.cloud), "an older generation can be deleted independently");
}
}

int main() {
    const auto root = fs::temp_directory_path() / ("ss-dense-versions-" + std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        edit_layout(root, false);
        edit_layout(root, true);
        generation_versions(root);
    } catch (const std::exception& e) { check(false, e.what()); }
    std::error_code ec;
    fs::remove_all(root, ec);
    std::printf("dense_edit_test: %s\n", failures ? "FAILED" : "OK");
    return failures ? 1 : 0;
}
