#include "dense/Artifact.h"
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
void set_age(const fs::path& path, int seconds_ago) {
    fs::last_write_time(path, fs::file_time_type::clock::now() - std::chrono::seconds(seconds_ago));
}

void edits(const fs::path& root) {
    const auto dataset = (root / "edits").string();
    const auto dense = fs::path(dataset) / "dense";
    fs::create_directories(dense);
    write(dense / "run.ply", ply(3));
    write(dense / "run.json", "{\"complete\":true,\"statistics\":{\"exported\":3}}");
    set_age(dense / "run.ply", 100);
    const auto part = dense / ".edit.part";
    write(part, ply(2));
    commit_edit(dense / "run.ply", part);
    check(read(dense / "run.original.ply") == ply(3) && read(dense / "run.ply") == ply(2),
          "the first edit keeps its preceding cloud as <stem>.original.ply");
    write(part, ply(1));
    commit_edit(dense / "run.ply", part);
    check(read(dense / "run.original.ply") == ply(3), "later saves preserve the original backup");
    check(default_cloud(dataset).relative == "run.ply" && default_cloud(dataset).manifest == dense / "run.json" &&
          artifact_complete(dataset), "the edited cloud is the newest and keeps its manifest");
    write(dense / "run.ply", ply(5));
    check(automatic_seed(dataset) == (dense / "run.ply").string() &&
          dsparse::read_seed_pointcloud(dataset, "dense/run.ply").num() == 5,
          "an external edit with a changed byte size remains the automatic seed");

    write(dense / "copy.ply", ply(4));
    check(default_cloud(dataset).relative == "copy.ply" && clouds(dataset).size() == 3,
          "a saved copy becomes the newest version");
    const auto unrelated = fs::path(dataset) / "outside.ply";
    write(unrelated, ply(9));
    bool refused = false;
    try { delete_cloud(dataset, "../outside.ply"); } catch (const std::exception&) { refused = true; }
    check(refused && fs::exists(unrelated), "deletion refuses paths outside the version list");
    std::error_code link_error;
    fs::create_symlink(unrelated, dense / "linked.ply", link_error);
    if (!link_error) {
        delete_cloud(dataset, "linked.ply");
        check(fs::exists(unrelated) && !fs::exists(dense / "linked.ply"),
              "deleting a linked version removes the link, not its target");
    }
    delete_cloud(dataset, "copy.ply");
    check(!fs::exists(dense / "copy.ply") && default_cloud(dataset).relative == "run.ply",
          "deleting the newest version falls back to the next one");
    delete_cloud(dataset, "run.ply");
    check(!fs::exists(dense / "run.json"), "deleting a version removes its manifest");
    for (auto remaining = clouds(dataset); !remaining.empty(); remaining = clouds(dataset))
        delete_cloud(dataset, remaining.front().relative);
    check(clouds(dataset).empty() && automatic_seed(dataset).empty(),
          "deleting the last version removes the automatic dense seed");
}
}

int main() {
    const auto root = fs::temp_directory_path() / ("ss-dense-versions-" + std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count()));
    try { edits(root); } catch (const std::exception& e) { check(false, e.what()); }
    std::error_code ec;
    fs::remove_all(root, ec);
    std::printf("dense_edit_test: %s\n", failures ? "FAILED" : "OK");
    return failures ? 1 : 0;
}
