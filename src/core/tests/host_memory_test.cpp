// cgroupRamHeadroomBytes against fake /proc/self/cgroup files and cgroup trees.

#include "core/HostMemory.h"

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

#ifdef __linux__
namespace {
namespace fs = std::filesystem;

int failures = 0;

void put(const fs::path& path, const std::string& text) {
    fs::create_directories(path.parent_path());
    std::ofstream(path) << text;
}

void expect(const char* name, size_t got, size_t want) {
    if (got == want) return;
    std::fprintf(stderr, "FAIL %s: got %zu, want %zu\n", name, got, want);
    ++failures;
}

size_t headroom(const fs::path& root) {
    return spirula::cgroupRamHeadroomBytes((root / "cgroup").c_str(), (root / "sys").c_str());
}
}  // namespace

int main() {
    const fs::path base = fs::temp_directory_path() / ("ss-host-memory-" + std::to_string(::getpid()));
    fs::remove_all(base);

    // v2: the parent's limit binds; its inactive file cache is reclaimable.
    fs::path r = base / "v2";
    put(r / "cgroup", "0::/job/step\n");
    put(r / "sys/job/memory.max", "1000\n");
    put(r / "sys/job/memory.current", "600\n");
    put(r / "sys/job/memory.stat", "anon 400\ninactive_file 100\nactive_file 100\n");
    put(r / "sys/job/step/memory.max", "max\n");
    put(r / "sys/job/step/memory.current", "550\n");
    expect("v2 parent limit", headroom(r), 500);

    // v2: the tighter child wins.
    put(r / "sys/job/step/memory.max", "700\n");
    put(r / "sys/job/step/memory.stat", "inactive_file 0\n");
    expect("v2 child limit", headroom(r), 150);

    // v2 with no limit anywhere.
    r = base / "v2-unlimited";
    put(r / "cgroup", "0::/user.slice/app.scope\n");
    put(r / "sys/user.slice/memory.max", "max\n");
    put(r / "sys/user.slice/memory.current", "123\n");
    put(r / "sys/user.slice/app.scope/memory.max", "max\n");
    put(r / "sys/user.slice/app.scope/memory.current", "45\n");
    expect("v2 unlimited", headroom(r), SIZE_MAX);

    // A container without a cgroup namespace: the listed path is not under the mount.
    r = base / "v2-no-namespace";
    put(r / "cgroup", "0::/system.slice/docker-abc.scope\n");
    put(r / "sys/memory.max", "4000\n");
    put(r / "sys/memory.current", "1000\n");
    expect("v2 mount root", headroom(r), 3000);

    // Usage past the limit leaves nothing.
    put(r / "sys/memory.current", "5000\n");
    expect("v2 over limit", headroom(r), 0);

    // Hybrid v1 (a Slurm job): the memory controller's hierarchy, its "no limit" sentinel at the root.
    r = base / "v1";
    put(r / "cgroup", "12:cpu,cpuacct:/slurm/job1\n9:memory:/slurm/job1\n0::/user.slice\n");
    put(r / "sys/memory/memory.limit_in_bytes", "9223372036854771712\n");
    put(r / "sys/memory/memory.usage_in_bytes", "99999\n");
    put(r / "sys/memory/slurm/job1/memory.limit_in_bytes", "2000\n");
    put(r / "sys/memory/slurm/job1/memory.usage_in_bytes", "1500\n");
    put(r / "sys/memory/slurm/job1/memory.stat", "inactive_file 50\ntotal_inactive_file 300\n");
    expect("v1 job limit", headroom(r), 800);

    // No cgroup file at all.
    expect("missing", spirula::cgroupRamHeadroomBytes((base / "none").c_str(), "/nonexistent"), SIZE_MAX);

    fs::remove_all(base);
    const size_t here = spirula::cgroupRamHeadroomBytes();
    if (here == SIZE_MAX) std::printf("this process: no cgroup memory limit\n");
    else std::printf("this process: %.2f GiB under its cgroup memory limit\n", here / 1073741824.0);
    std::printf(failures ? "host_memory_test: %d failures\n" : "host_memory_test: passed\n", failures);
    return failures ? 1 : 0;
}
#else
int main() {
    std::puts("host_memory_test: cgroups are Linux-only; nothing to test");
    return 0;
}
#endif
