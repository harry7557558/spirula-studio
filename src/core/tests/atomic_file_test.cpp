// atomic_file_test -- replace_file publishes a temporary over its destination,
// including while another reader still has the destination open: Windows
// refuses that rename, and a GUI polling a progress file every frame did.

#include "core/AtomicFile.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>

namespace fs = std::filesystem;

namespace {

int failures = 0;

void check(bool ok, const char* what) {
    std::printf("%s  %s\n", ok ? "ok  " : "BAD ", what);
    if (!ok) failures++;
}

void write(const fs::path& p, const std::string& text) {
    std::ofstream(p, std::ios::binary | std::ios::trunc) << text;
}

std::string read(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    std::ostringstream s;
    s << f.rdbuf();
    return s.str();
}

}  // namespace

int main() {
    const fs::path dir = fs::temp_directory_path() / "spirula_atomic_file_test";
    fs::remove_all(dir);
    fs::create_directories(dir);
    const fs::path dst = dir / "state.progress", tmp = dir / "state.progress.tmp";

    write(dst, "old");
    write(tmp, "new");
    spirula::replace_file(tmp, dst);
    check(read(dst) == "new" && !fs::exists(tmp), "replaces a closed destination");

    write(tmp, "newer");
    auto reader = std::make_unique<std::ifstream>(dst, std::ios::binary);
    std::thread release([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        reader.reset();
    });
    bool threw = false;
    try {
        spirula::replace_file(tmp, dst);
    } catch (const std::exception&) {
        threw = true;
    }
    release.join();
    check(!threw && read(dst) == "newer", "replaces a destination a reader briefly holds open");

    threw = false;
    try {
        spirula::replace_file(dir / "missing.tmp", dst);
    } catch (const std::exception&) {
        threw = true;
    }
    check(threw && read(dst) == "newer", "a missing temporary still throws, at once");

    fs::remove_all(dir);
    std::printf("\n%d failures\n", failures);
    return failures ? 1 : 0;
}
