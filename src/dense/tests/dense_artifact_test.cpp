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
        ("spirula-generations-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    ~Fixture() { std::error_code error; fs::remove_all(spirula::long_path(root), error); }

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
        require(stopped && current.generation.rfind("legacy-", 0) == 0 && artifact_available(current),
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
            require(stopped && current.generation == first.generation && artifact_available(current),
                    "publication interruption exposed an inconsistent generation");
            require(!fs::exists(fixture.root / "dense" / "generations" / next.generation),
                    "publication interruption retained an abandoned generation");
            require(automatic_seed(fixture.root.string()) == first.cloud.string(),
                    "training seed resolved a partially replaced compatibility cloud");
        }
        const auto second = fixture.publish(fixture.stage("second", "two"));
        {
            WorkingDirectory working(fixture.root.parent_path());
            const auto relative = fixture.root.filename();
            require(artifact_files(relative.string()).cloud == second.cloud &&
                    resolve_seed_path(relative.string(), "dense/roma.ply") == (fixture.root / "dense/roma.ply").string() &&
                    resolve_dense_file((relative / "dense" / "roma.ply").string()) == (fixture.root / "dense/roma.ply").string() &&
                    resolve_seed_path(relative.string(), "dense/generations/first/roma.ply") == first.cloud.string(),
                    "relative dataset or seed did not resolve its explicit file");
        }
        const auto abandoned = fixture.root / "dense" / "generations" / "abandoned";
        fs::create_directories(abandoned); generation_marker(abandoned,".pending");
        const auto unowned = fixture.root / "dense" / "generations" / "unowned";
        fs::create_directories(unowned);
        generation_marker(first.cloud.parent_path(),".pending");
        cleanup_pending_generations(fixture.root.string());
        require(!fs::exists(abandoned) && fs::exists(unowned) && artifact_available(first) && artifact_available(second),
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
        {
            // Issue 178: a prediction nests two 64-digit names past MAX_PATH. Test executables carry no
            // long-path manifest, so this process reaches such a path only in extended-length form.
            auto deep = fixture.root;
            while (deep.native().size() < 200) deep /= "a-long-dataset-folder-name";
            const auto cache = spirula::long_path(deep / "dense" / "cache");
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
        require(artifact_files(fixture.root.string()).generation == "second" && artifact_complete(fixture.root.string()),
                "successful generation did not become current");
        require(is_dense_seed(fixture.root.string(), second.cloud.string()) &&
                resolve_dense_file(second.cloud.string()) == second.cloud.string() &&
                resolve_dense_file(legacy.cloud.string()) == legacy.cloud.string(),
                "explicit cloud paths were redirected");
        require(resolve_seed_path(fixture.root.string(), first.cloud.string()) == first.cloud.string(),
                "an explicitly selected published generation was replaced by the current one");
        { std::ofstream output(legacy.cloud); output << "edited compatibility cloud"; }
        require(artifact_available(second) && resolve_dense_file(legacy.cloud.string()) == legacy.cloud.string(),
                "compatibility export edits changed published generation contents");
        { std::ofstream output(second.cloud); output << "bad"; }
        stopped = false;
        try { resolve_dense_file(second.cloud.string()); } catch (const std::exception&) { stopped = true; }
        require(!stopped && artifact_complete(fixture.root.string()), "external dense cloud edit was refused");
        { std::ofstream output(second.cloud); output << "two"; }
        { std::ofstream output(second.manifest, std::ios::app); output << ' '; }
        stopped = false;
        try { artifact_files(fixture.root.string()); } catch (const std::exception&) { stopped = true; }
        require(!stopped && artifact_complete(fixture.root.string()), "an unchanged manifest with external whitespace was refused");
        const auto recovered = fixture.publish(fixture.stage("recovered", "three"));
        require(artifact_available(recovered), "a new successful run could not replace a corrupt generation record");
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
                require(artifact_available(published) && !fs::exists(pending) && artifact_complete(link.string()),
                        "a dataset reached through a linked parent folder could not publish or clean up");
            }
        }
        std::printf("PASS dense writer lock, owned cleanup, published generations, interrupted publication, legacy migration, external edits, seed resolution, linked datasets and recovery\n");
        return 0;
    } catch (const std::exception& error) { std::fprintf(stderr, "FAIL: %s\n", error.what()); return 1; }
}
