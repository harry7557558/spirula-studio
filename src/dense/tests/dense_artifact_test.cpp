#include "dense/Artifact.h"
#include "core/AtomicFile.h"
#include "core/FileLock.h"
#include "core/LongPath.h"
#include "core/OwnedDirectory.h"
#include "core/Env.h"

#include <chrono>
#include <cstdio>

namespace fs = std::filesystem;
using namespace spirula::dense;

namespace {
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

struct Fixture {
    fs::path root = fs::canonical(fs::temp_directory_path()) /
        ("spirula-dense-clouds-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    ~Fixture() { std::error_code error; fs::remove_all(spirula::long_path(root), error); }

    // A staged run output: `count` binary float points and a finished manifest.
    std::pair<fs::path, fs::path> stage(const std::string& name, int count) {
        const auto directory = root / "staging" / name;
        fs::create_directories(directory);
        const auto cloud = directory / "roma.ply", manifest = directory / "manifest.json";
        write_binary(cloud, count);
        { std::ofstream output(manifest); output << "{\"complete\":true}"; }
        return {cloud, manifest};
    }

    Cloud publish(const std::string& name, int count, const std::string& label = "balanced") {
        const auto [cloud, manifest] = stage(name, count);
        return publish_cloud(root.string(), label, cloud, manifest);
    }

    static void write_binary(const fs::path& path, int count, int drop_bytes = 0) {
        std::ofstream output(path, std::ios::binary);
        output << "ply\nformat binary_little_endian 1.0\nelement vertex " << count
               << "\nproperty float x\nproperty float y\nproperty float z\n"
                  "property uchar red\nproperty uchar green\nproperty uchar blue\nend_header\n";
        output << std::string((size_t)count * 15 - (size_t)drop_bytes, '\0');
    }
};

void set_age(const fs::path& path, int seconds_ago) {
    fs::last_write_time(path, fs::file_time_type::clock::now() - std::chrono::seconds(seconds_ago));
}

struct WorkingDirectory {
    fs::path previous = fs::current_path();
    explicit WorkingDirectory(const fs::path& directory) { fs::current_path(directory); }
    ~WorkingDirectory() { fs::current_path(previous); }
};
}

int main() {
    try {
        if (const auto path = spirula::env("DENSE_REDIRECT_TEST")) {
            const fs::path directory(path), target = directory / "target" / "sentinel";
            require(fs::is_regular_file(target),"redirect fixture is missing its target sentinel");
            const auto digest = spirula::sha256_file(target.string());
            for (const auto& parent : {directory,directory / "parent-redirect"}) {
                for (const char* name : {"owned","redirect"}) {
                    if (!fs::exists(parent / name)) continue;
                    bool rejected = false;
                    try { spirula::remove_owned_directory(parent / name,parent,name); }
                    catch (const std::exception&) { rejected = true; }
                    require(rejected && fs::exists(parent / name) && spirula::sha256_file(target.string()) == digest,
                            "redirect cleanup followed a junction or changed target contents");
                }
            }
            std::printf("PASS cleanup rejects nested, root and parent redirects with target bytes preserved\n");
        }
        Fixture fixture;
        const auto dataset = fixture.root.string();
        const auto dense = fixture.root / "dense";
        require(automatic_seed(dataset).empty() && clouds(dataset).empty(), "a dataset without a dense cloud got a seed");
        bool stopped = false;
        {
            fs::create_directories(work_root(dataset));
            spirula::FileLock lock(work_root(dataset) / "run.lock");
            try { spirula::FileLock second(work_root(dataset) / "run.lock"); }
            catch (const std::exception&) { stopped = true; }
            require(stopped, "two writers acquired the same dense run lock");
        }
        const auto first = fixture.publish("first", 4);
        require(first.ply.parent_path() == dense && first.ply.extension() == ".ply" &&
                first.relative.find("_balanced") != std::string::npos && fs::is_regular_file(first.manifest) &&
                first.manifest.stem() == first.ply.stem(), "a published cloud is not a dated file with its manifest beside it");
        const auto again = fixture.publish("again", 5);
        require(again.ply != first.ply && fs::exists(first.ply), "a second publication in the same second overwrote the first");
        set_age(first.ply, 100); set_age(again.ply, 50);
        require(automatic_seed(dataset) == again.ply.string() && artifact_complete(dataset),
                "the newest finished cloud was not the automatic seed");
        set_age(again.ply, 200);
        require(default_cloud(dataset).ply == first.ply, "the default did not follow modification time");

        fs::create_directories(dense / "generations" / "run-1");
        Fixture::write_binary(dense / "generations" / "run-1" / "roma.ply", 3);
        { std::ofstream(dense / "generations" / "run-1" / "manifest.json") << "{\"complete\":true}"; }
        set_age(dense / "generations" / "run-1" / "roma.ply", 10);
        const auto legacy = default_cloud(dataset);
        require(legacy.relative == "generations/run-1/roma.ply" &&
                legacy.manifest == dense / "generations" / "run-1" / "manifest.json" && artifact_complete(dataset),
                "a cloud from the older generations layout was not found with its manifest");

        fs::create_directories(dense / "mine");
        Fixture::write_binary(dense / "mine" / "cleaned.ply", 2);
        require(default_cloud(dataset).relative == "mine/cleaned.ply" && artifact_complete(dataset) &&
                !artifact_complete(dataset, true), "a cloud without a manifest seeds training but must not count as reusable");
        Fixture::write_binary(dense / "truncated.ply", 6, 1);
        { std::ofstream(dense / "mesh.ply") << "ply\nformat ascii 1.0\nelement vertex 3\nproperty float x\n"
              "property float y\nproperty float z\nelement face 1\nproperty list uchar int vertex_indices\nend_header\n"
              "0 0 0\n1 0 0\n0 1 0\n3 0 1 2\n"; }
        { std::ofstream(dense / "notes.ply") << "not a ply"; }
        fs::create_directories(work_root(dataset) / "cache");
        Fixture::write_binary(work_root(dataset) / "scratch.ply", 8);
        for (const auto& c : clouds(dataset))
            require(c.relative != "truncated.ply" && c.relative != "mesh.ply" && c.relative != "notes.ply" &&
                    c.relative.rfind(".work", 0) != 0, "an implausible or scratch PLY was listed as a cloud");
        require(clouds(dataset).size() == 4, "the cloud list missed a version");
        require(dense_dataset_of(first.ply) == dataset && dense_dataset_of(dense / "mine" / "cleaned.ply") == dataset &&
                dense_dataset_of(work_root(dataset) / "scratch.ply").empty() && dense_dataset_of(fixture.root / "x.ply").empty(),
                "a cloud's dataset was not recovered from its path");
        {
            WorkingDirectory working(fixture.root.parent_path());
            const auto relative = fixture.root.filename().string();
            require(is_dense_seed(relative, "dense/mine/cleaned.ply") &&
                    resolve_seed_path(relative, "dense/mine/cleaned.ply") == (dense / "mine" / "cleaned.ply").string() &&
                    resolve_seed_path(relative, "elsewhere.ply") == "elsewhere.ply",
                    "relative dataset or seed did not resolve its explicit file");
        }
        {
            const auto cache = work_root(dataset) / "cache";
            const auto kept = cache / std::string(64, 'a'), emptied = cache / std::string(64, 'b');
            const auto pairs = cache / "pairs-v4" / "run-1", lookalike = cache / "notes" / "run-2";
            for (const auto& directory : {kept / "run-11" / "nested", emptied / "run-22", kept / "run-notdigits", pairs, lookalike})
                fs::create_directories(directory);
            { std::ofstream(kept / "run-11" / "nested" / "observations-0-0.bin") << "tile"; }
            { std::ofstream(kept / "pair.roma") << "prediction"; }
            { std::ofstream(cache / "fingerprints.txt") << "ledger"; }
            const auto outside = fixture.root / "outside";
            fs::create_directories(outside);
            { std::ofstream(outside / "keep.txt") << "user data"; }
            std::error_code link_error;
            fs::create_directory_symlink(outside, kept / "run-33", link_error);
            cleanup_abandoned_runs(cache);
            require(!fs::exists(kept / "run-11") && !fs::exists(emptied) && fs::exists(kept / "pair.roma") &&
                    fs::exists(kept / "run-notdigits") && fs::exists(pairs) && fs::exists(lookalike) &&
                    fs::exists(cache / "fingerprints.txt") && fs::exists(outside / "keep.txt"),
                    "abandoned run sweep removed predictions or unowned data, or kept abandoned work");
            cleanup_abandoned_runs(fixture.root / "missing-cache");
        }
        {
            // Issue 178: a prediction nests two 64-digit names past MAX_PATH. Test executables carry no
            // long-path manifest, so this process reaches such a path only in extended-length form.
            auto deep = fixture.root;
            while (deep.native().size() < 200) deep /= "a-long-dataset-folder-name";
            const auto cache = spirula::long_path(work_root(deep.string()) / "cache");
            require(spirula::long_path(cache) == cache, "long_path is not idempotent");
            const auto run = cache / std::string(64, 'c') / "run-7";
            const auto prediction = cache / "pairs-v4" / std::string(64, 'd') / (std::string(64, 'e') + ".roma");
            require(prediction.native().size() > 300, "the long-path fixture is not long enough");
            fs::create_directories(run / "nested"); fs::create_directories(prediction.parent_path());
            { std::ofstream(run / "nested" / "observations-0-0.bin") << "tile"; }
            auto part = prediction; part += ".part";
            { std::ofstream output(part, std::ios::binary); output << "prediction"; output.flush(); require((bool)output, "cannot write a prediction past MAX_PATH"); }
            spirula::replace_file(part, prediction);
            require(spirula::sha256_file(prediction.string()).size() == 64, "cannot hash a prediction past MAX_PATH");
            cleanup_abandoned_runs(cache);
            require(!fs::exists(run) && fs::exists(prediction), "abandoned-run cleanup failed past MAX_PATH");
            spirula::remove_owned_directory(deep / "dense", deep, "dense");
            require(!fs::exists(spirula::long_path(deep / "dense")), "owned cleanup failed past MAX_PATH");
        }
        {
            Fixture linked;
            fs::create_directories(linked.root);
            const auto link = fs::path(linked.root.string() + "-link");
            std::error_code link_error;
            fs::create_directory_symlink(linked.root, link, link_error);
            if (!link_error) {
                struct RemoveLink { fs::path path; ~RemoveLink() { std::error_code e; fs::remove(path, e); } } remove_link{link};
                const auto [cloud, manifest] = linked.stage("through-link", 2);
                const auto published = publish_cloud(link.string(), "fast", cloud, manifest);
                require(fs::is_regular_file(published.ply) && artifact_complete(link.string()) &&
                        is_dense_seed(link.string(), published.ply.string()),
                        "a dataset reached through a linked parent folder could not publish");
            }
        }
        std::printf("PASS dense writer lock, owned cleanup, named publication, newest default, legacy and user clouds, implausible and scratch PLYs, seed resolution and linked datasets\n");
        return 0;
    } catch (const std::exception& error) { std::fprintf(stderr, "FAIL: %s\n", error.what()); return 1; }
}
