#include "dense/Artifact.h"
#include "core/FileLock.h"
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
        ("spirula-generations-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    ~Fixture() { std::error_code error; fs::remove_all(root, error); }

    ArtifactFiles stage(const std::string& name, const std::string& text) {
        const auto directory = root / name;
        fs::create_directories(directory);
        ArtifactFiles files{directory / "roma.ply", directory / "manifest.json", name};
        { std::ofstream output(files.cloud, std::ios::binary); output << text; }
        JsonWriter manifest;
        manifest.object().field("complete", true).field("cloud_sha256", spirula::sha256_file(files.cloud.string()))
            .field("cloud_bytes", (long long)text.size()).key("statistics").object().field("exported", 1).end().end();
        { std::ofstream output(files.manifest); output << manifest.str(); }
        return files;
    }

    ArtifactFiles publish(const ArtifactFiles& files, const std::function<void(const char*)>& hook = {}) {
        return publish_generation(root.string(), files.generation, files.cloud, files.manifest, hook);
    }
};

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
        require(automatic_seed(fixture.root.string()).empty(), "a dataset without a dense cloud got a seed");
        const auto legacy = fixture.stage("dense", "old");
        require(automatic_seed(fixture.root.string()) == artifact_files(fixture.root.string()).cloud.string(),
                "a finished dense cloud was not the automatic seed");
        bool stopped = false;
        {
            spirula::FileLock lock(fixture.root / "dense" / "run.lock");
            try { spirula::FileLock second(fixture.root / "dense" / "run.lock"); }
            catch (const std::exception&) { stopped = true; }
            require(stopped, "two writers acquired the same dense run lock");
        }
        { spirula::FileLock released(fixture.root / "dense" / "run.lock"); }
        const auto owned = fixture.stage("owned", "temporary");
        stopped = false;
        try { spirula::remove_owned_directory(owned.cloud.parent_path(), fixture.root, "dense"); }
        catch (const std::exception&) { stopped = true; }
        require(stopped && fs::exists(legacy.cloud), "cleanup accepted an unrelated directory");
        spirula::remove_owned_directory(owned.cloud.parent_path(), fixture.root, "owned");
        require(!fs::exists(owned.cloud), "owned work was not removed");
        require(artifact_complete(fixture.root.string()), "legacy artifact was not recognized");
        const auto interrupted = fixture.stage("interrupted", "new");
        stopped = false;
        try { fixture.publish(interrupted, [](const char*) { throw std::runtime_error("interrupted"); }); }
        catch (const std::exception&) { stopped = true; }
        auto current = artifact_files(fixture.root.string());
        require(stopped && current.generation.rfind("legacy-", 0) == 0 && artifact_checksum_valid(current),
                "interrupted migration lost the legacy cloud");
        require(spirula::sha256_file(current.cloud.string()) == spirula::sha256_file(legacy.cloud.string()),
                "legacy migration changed cloud bytes");

        const auto first = fixture.publish(fixture.stage("first", "one"));
        for (const char* phase : {"generation_cloud", "generation_manifest", "compatibility_cloud", "compatibility_manifest", "before_current"}) {
            const auto next = fixture.stage(std::string("failure-") + phase, "two");
            stopped = false;
            try { fixture.publish(next, [&](const char* observed) {
                if (std::string(observed) == phase) throw std::runtime_error("interrupted");
            }); } catch (const std::exception&) { stopped = true; }
            current = artifact_files(fixture.root.string());
            require(stopped && current.generation == first.generation && artifact_checksum_valid(current),
                    "publication interruption exposed an inconsistent generation");
            require(!fs::exists(fixture.root / "dense" / "generations" / next.generation),
                    "publication interruption retained an abandoned generation");
            require(verified_seed_path(fixture.root.string(), "dense/roma.ply") == first.cloud.string(),
                    "training seed resolved a partially replaced compatibility cloud");
        }
        const auto second = fixture.publish(fixture.stage("second", "two"));
        {
            WorkingDirectory working(fixture.root.parent_path());
            const auto relative = fixture.root.filename();
            require(artifact_files(relative.string()).cloud == second.cloud &&
                    verified_seed_path(relative.string(), "dense/roma.ply") == second.cloud.string() &&
                    verified_dense_file((relative / "dense" / "roma.ply").string()) == second.cloud.string() &&
                    verified_seed_path(relative.string(), "dense/generations/first/roma.ply") == first.cloud.string(),
                    "relative dataset or seed did not resolve an absolute immutable generation");
        }
        const auto abandoned = fixture.root / "dense" / "generations" / "abandoned";
        fs::create_directories(abandoned); generation_marker(abandoned,".pending");
        const auto unowned = fixture.root / "dense" / "generations" / "unowned";
        fs::create_directories(unowned);
        generation_marker(first.cloud.parent_path(),".pending");
        cleanup_pending_generations(fixture.root.string());
        require(!fs::exists(abandoned) && fs::exists(unowned) && artifact_checksum_valid(first) && artifact_checksum_valid(second),
                "recovery cleanup removed published/unowned data or retained abandoned work");
        {
            const auto cache = fixture.root / "dense" / "cache";
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
        require(artifact_files(fixture.root.string()).generation == "second" && artifact_complete(fixture.root.string()),
                "successful generation did not become current");
        require(is_dense_seed(fixture.root.string(), second.cloud.string()) &&
                verified_dense_file(second.cloud.string()) == second.cloud.string() &&
                verified_dense_file(legacy.cloud.string()) == second.cloud.string(),
                "preview and training did not resolve the same generation");
        require(verified_seed_path(fixture.root.string(), first.cloud.string()) == first.cloud.string(),
                "an explicitly selected immutable generation was replaced by the current one");
        { std::ofstream output(legacy.cloud); output << "edited compatibility cloud"; }
        require(artifact_checksum_valid(second) && verified_dense_file(legacy.cloud.string()) == second.cloud.string(),
                "compatibility export edits changed immutable generation contents");
        { std::ofstream output(second.cloud); output << "bad"; }
        stopped = false;
        try { verified_dense_file(second.cloud.string()); } catch (const std::exception&) { stopped = true; }
        require(stopped && !artifact_checksum_valid(second), "same-size dense cloud corruption reached a consumer");
        { std::ofstream output(second.cloud); output << "two"; }
        { std::ofstream output(second.manifest, std::ios::app); output << ' '; }
        stopped = false;
        try { artifact_files(fixture.root.string()); } catch (const std::exception&) { stopped = true; }
        require(stopped && !artifact_complete(fixture.root.string()), "corrupt current manifest silently fell back to an alias");
        const auto recovered = fixture.publish(fixture.stage("recovered", "three"));
        require(artifact_checksum_valid(recovered), "a new successful run could not replace a corrupt generation record");
        {
            std::ofstream output(fixture.root / "dense" / "current.json");
            output << "{\"version\":1,\"generation\":\"../outside\",\"manifest_sha256\":\""
                   << std::string(64, 'a') << "\"}";
        }
        require(!artifact_complete(fixture.root.string()), "generation traversal was accepted");
        Fixture initial;
        const auto first_failure = initial.stage("first-failure","first");
        stopped = false;
        try { initial.publish(first_failure,[](const char* phase) {
            if (std::string(phase) == "before_current") throw std::runtime_error("interrupted");
        }); } catch (const std::exception&) { stopped = true; }
        require(stopped && !artifact_complete(initial.root.string()),
                "interrupted first publication exposed compatibility aliases as a successful legacy generation");
        {
            Fixture linked;
            fs::create_directories(linked.root);
            const auto link = fs::path(linked.root.string() + "-link");
            std::error_code link_error;
            fs::create_directory_symlink(linked.root, link, link_error);
            if (!link_error) {
                struct RemoveLink { fs::path path; ~RemoveLink() { std::error_code e; fs::remove(path, e); } } remove_link{link};
                const auto staged = linked.stage("through-link", "linked");
                const auto published = publish_generation(link.string(), staged.generation, staged.cloud, staged.manifest);
                const auto pending = linked.root / "dense" / "generations" / "pending-through-link";
                fs::create_directories(pending); generation_marker(pending, ".pending");
                cleanup_pending_generations(link.string());
                require(artifact_checksum_valid(published) && !fs::exists(pending) && artifact_complete(link.string()),
                        "a dataset reached through a linked parent folder could not publish or clean up");
            }
        }
        std::printf("PASS dense writer lock, owned cleanup, immutable generations, interrupted publication, legacy migration, checksums, seed resolution, linked datasets and recovery\n");
        return 0;
    } catch (const std::exception& error) { std::fprintf(stderr, "FAIL: %s\n", error.what()); return 1; }
}
