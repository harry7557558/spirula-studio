// atomic_file_test -- replace_file publishes a temporary over its destination,
// including while another reader still has the destination open: Windows
// refuses that rename, and a GUI polling a progress file every frame did.

#include "core/AtomicFile.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <sstream>
#include <string>
#include <thread>

#ifndef _WIN32
#include <unistd.h>
#endif

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

template<class Fn>
bool throws(Fn&& fn) {
    try { fn(); }
    catch (const std::exception&) { return true; }
    return false;
}

void test_staged_pair(const fs::path& dir) {
    fs::create_directory(dir);
    const auto first = dir / "first.json";
    const auto second = dir / "second.json";
#ifdef _WIN32
    const auto pid = GetCurrentProcessId();
#else
    const auto pid = getpid();
#endif
    const auto collision = dir / ("first.json." + std::to_string(pid) + ".0.tmp");
    write(collision, "crash leftover");
    write(first, "old first");
    write(second, "old second");
    spirula::AtomicOutputFile a(first, 0), b(second, 0);
    a.stream() << "new first";
    b.stream() << "new second";
    check(throws([&] { a.commit(); }), "open stream cannot commit");
    a.close_checked();
    b.close_checked();
    check(!a.stream().is_open() && !b.stream().is_open() &&
          read(first) == "old first" && read(second) == "old second",
          "both staged streams closed before either publishes");
    a.commit();
    b.commit();
    check(read(first) == "new first" && read(second) == "new second",
          "explicit commits publish the closed streams");
    check(read(collision) == "crash leftover", "reservation never truncates an existing temporary");
}

void test_abandon_and_failed_close(const fs::path& dir) {
    fs::create_directory(dir);
    const auto dst = dir / "state.json";
    const auto staging = dir / "staging";
    fs::create_directory(staging);
    write(dst, "old");
    for (bool close : {false, true}) {
        {
            spirula::AtomicOutputFile file(dst, 0, staging);
            file.stream() << "abandoned";
            if (close) file.close_checked();
            const auto it = fs::directory_iterator(staging);
            check(it != fs::directory_iterator{} && it->path().extension() == ".tmp",
                  "explicit staging is used and does not look like JSON");
        }
        check(fs::is_empty(staging) && read(dst) == "old",
              "abandoning an uncommitted output removes only its staging");
    }
    {
        spirula::AtomicOutputFile file(dst, 64u << 10, staging);
#ifdef _WIN32
        const auto tmp = fs::directory_iterator(staging)->path();
        const HANDLE lock = CreateFileW(tmp.c_str(), GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0, nullptr);
        OVERLAPPED all{};
        check(lock != INVALID_HANDLE_VALUE &&
              LockFileEx(lock, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY,
                         0, MAXDWORD, MAXDWORD, &all), "inject a late write failure on staging");
        file.stream() << "new";
        check(bool(file.stream()), "small write stays buffered until checked close");
#else
        file.stream().setstate(std::ios::badbit);
#endif
        check(throws([&] { file.close_checked(); }) && !file.stream().is_open(),
              "checked close reports stream failure and closes the handle");
        check(throws([&] { file.commit(); }), "a failed close prevents commit");
#ifdef _WIN32
        if (lock != INVALID_HANDLE_VALUE) CloseHandle(lock);
#endif
    }
    check(fs::is_empty(staging) && read(dst) == "old",
          "failed close cleans staging without changing the destination");
    check(!spirula::write_file_atomic(dst, "new", 3, dst) && read(dst) == "old",
          "an invalid explicit staging parent cannot change the destination");
}

void test_denied_replacement(const fs::path& dir) {
    fs::create_directory(dir);
    const auto dst = dir / "state.json";
#ifdef _WIN32
    write(dst, "old");
    const HANDLE hold = CreateFileW(dst.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                    nullptr, OPEN_EXISTING, 0, nullptr);
    check(hold != INVALID_HANDLE_VALUE, "hold the destination without delete sharing");
    const auto original = dst;
#else
    fs::create_directory(dst);
    const auto original = dst / "previous";
    write(original, "old");
#endif
    {
        spirula::AtomicOutputFile file(dst, 0);
        file.stream() << "new";
        file.close_checked();
        check(throws([&] { file.commit(); }), "replacement denial is reported after a successful close");
    }
#ifdef _WIN32
    if (hold != INVALID_HANDLE_VALUE) CloseHandle(hold);
#endif
    check(read(original) == "old" && std::distance(fs::directory_iterator(dir),
          fs::directory_iterator{}) == 1, "replacement denial preserves the old data and cleans staging");
}

}  // namespace

int main() {
    const fs::path dir = fs::temp_directory_path() / "spirula_atomic_file_test";
    fs::remove_all(dir);
    fs::create_directories(dir);
    test_staged_pair(dir / "pair");
    test_abandon_and_failed_close(dir / "abandon");
    test_denied_replacement(dir / "denied");
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
