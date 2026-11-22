#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <csignal>
#include <sys/resource.h>
#endif

#include "checkpoint/Adapt.h"
#include "checkpoint/BackgroundExport.h"
#include "checkpoint/Resume.h"
#include "core/CheckpointIO.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {
namespace fs = std::filesystem;

void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}

template<class F>
bool throws_io_error(F action) {
    try { action(); }
    catch (const std::runtime_error&) { return true; }
    return false;
}

std::string archive() {
    std::ostringstream out;
    const std::string state = R"({"step":17,"max_num_splats":2,"cur_num_splats":2})";
    ckpt::tar_write_bytes(out, "state.json", state.data(), state.size());
    const float opacity[]{-4.f, 2.f};
    auto npy = ckpt::npy_header("<f4", 2);
    npy.append(reinterpret_cast<const char*>(opacity), sizeof(opacity));
    ckpt::tar_write_bytes(out, "world.opacities.npy", npy.data(), npy.size());
    const float background[]{1.f, 2.f, 3.f};
    npy = ckpt::npy_header("<f4", 3);
    npy.append(reinterpret_cast<const char*>(background), sizeof(background));
    ckpt::tar_write_bytes(out, "eng.bg_sky.sh_coeffs.npy", npy.data(), npy.size());
    ckpt::tar_finish(out);
    return out.str();
}

void write_file(const fs::path& path, const std::string& data, size_t size) {
    std::ofstream out(path, std::ios::binary);
    out.write(data.data(), static_cast<std::streamsize>(size));
    out.close();
    require(bool(out), "could not write fixture");
}

void check_truncation(const fs::path& root, const std::string& data, size_t cut) {
    const auto src = root / "truncated.tar", out = root / ("truncated-" + std::to_string(cut));
    write_file(src, data, cut);
    ckpt::TargetLayout target;
    target.max_num_splats = 1;
    const bool adapt_rejected = throws_io_error([&] { ckpt::adapt_checkpoint(src, target, out); });
    const bool manifest_rejected = throws_io_error([&] { ckpt::read_state_json(src); });
    require(adapt_rejected && manifest_rejected, !adapt_rejected
        ? "adaptation accepted a truncated archive" : "manifest accepted a truncated archive");
    require(!fs::exists(out), "adaptation opened output for a truncated archive");
}

class WriteBlocker {
public:
    explicit WriteBlocker(const fs::path& path) {
#ifdef _WIN32
        file_ = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE,
                           FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        require(file_ != INVALID_HANDLE_VALUE, "could not create write fault target");
        OVERLAPPED all{};
        if (!LockFileEx(file_, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY,
                        0, MAXDWORD, MAXDWORD, &all)) {
            CloseHandle(file_);
            throw std::runtime_error("could not lock write fault target");
        }
#else
        (void)path;
        require(getrlimit(RLIMIT_FSIZE, &old_) == 0, "getrlimit failed");
        old_handler_ = std::signal(SIGXFSZ, SIG_IGN);
        require(old_handler_ != SIG_ERR, "could not ignore SIGXFSZ");
        rlimit limit = old_;
        limit.rlim_cur = 16;
        if (setrlimit(RLIMIT_FSIZE, &limit) != 0) {
            std::signal(SIGXFSZ, old_handler_);
            throw std::runtime_error("setrlimit failed");
        }
#endif
    }

    ~WriteBlocker() {
#ifdef _WIN32
        CloseHandle(file_);
#else
        setrlimit(RLIMIT_FSIZE, &old_);
        std::signal(SIGXFSZ, old_handler_);
#endif
    }

private:
#ifdef _WIN32
    HANDLE file_ = INVALID_HANDLE_VALUE;
#else
    rlimit old_{};
    void (*old_handler_)(int) = nullptr;
#endif
};

void check_adaptation(const fs::path& root, const std::string& data) {
    const auto src = root / "original.tar", out = root / ".resume_adapt";
    const auto dst = out / "state.tar";
    write_file(src, data, data.size());
    ckpt::TargetLayout target;
    target.max_num_splats = 2;
    require(!ckpt::adapt_checkpoint(src, target, out), "unchanged layout requested adaptation");
    require(!fs::exists(out), "unchanged layout wrote output");

    target.max_num_splats = 1;
    fs::create_directory(out);
    write_file(out / "neighbor.txt", "keep", 4);
    bool rejected_write = false;
    {
        WriteBlocker blocked(dst);
        rejected_write = throws_io_error([&] { ckpt::adapt_checkpoint(src, target, out); });
    }
    require(rejected_write, "failed adaptation output was not reported");
    require(!fs::exists(dst), "failed adaptation left partial state.tar");
    require(fs::file_size(out / "neighbor.txt") == 4, "adaptation removed a neighboring file");
    {
        std::ifstream original(src, std::ios::binary);
        const std::string bytes{std::istreambuf_iterator<char>(original), {}};
        require(bytes == data, "failed adaptation changed source archive");
    }

    require(ckpt::adapt_checkpoint(src, target, out), "changed layout did not adapt");
    const auto state = ckpt::read_state_json(dst);
    require(state.get_double("step", -1) == 17 && state.get_double("cur_num_splats", -1) == 1 &&
            state.get_double("max_num_splats", -1) == 1, "adapted manifest has wrong counts or step");
}
}  // namespace

int main() {
    const auto root = fs::temp_directory_path() / ("ss-checkpoint-io-" + std::to_string(
        static_cast<long long>(std::chrono::steady_clock::now().time_since_epoch().count())));
    if (!fs::create_directory(root)) return 1;
    int failures = 0;
    auto check = [&](const char* name, auto action) {
        try { action(); }
        catch (const std::exception& e) { std::fprintf(stderr, "%s: %s\n", name, e.what()); ++failures; }
    };
    const auto data = archive();
    check("header boundary", [&] { check_truncation(root, data, 2048); });
    check("short header", [&] { check_truncation(root, data, 2048 + 17); });
    check("short payload", [&] { check_truncation(root, data, 2048 + 512 + 129); });
    check("nonzero first footer block", [&] {
        auto invalid = data;
        invalid[invalid.size() - 1023] = 'x';
        check_truncation(root, invalid, invalid.size());
    });
    check("missing footer", [&] { check_truncation(root, data, data.size() - 1024); });
    check("one footer block", [&] { check_truncation(root, data, data.size() - 512); });
    check("short footer", [&] { check_truncation(root, data, data.size() - 1); });
    check("nonzero footer", [&] {
        auto invalid = data;
        invalid.back() = 'x';
        check_truncation(root, invalid, invalid.size());
    });
    check("small member exact read", [] {
        std::istringstream in("abc");
        require(throws_io_error([&] { ckpt::tar_read_member(in, {"state.json", 1, 3}); }),
                "short member read succeeded");
        require(throws_io_error([&] { ckpt::tar_read_member(in, {"state.json", 4, 0}); }),
                "failed member seek succeeded");
    });
    check("background and empty TAR", [&] {
        std::istringstream empty(std::string(1024, '\0'));
        require(ckpt::tar_index(empty).empty(), "valid empty TAR did not index");
        const auto src = root / "background.tar", output = root / "background.webp";
        write_file(src, data, data.size() - 1024);
        require(!ckpt::export_background_from_checkpoint(src, output, 8, 4), "background accepted a missing footer");
        require(!fs::exists(output), "background wrote output from truncated TAR");
        write_file(src, data, data.size());
        require(ckpt::export_background_from_checkpoint(src, output, 8, 4), "background rejected a valid TAR");
    });
    check("adaptation output", [&] { check_adaptation(root, data); });
    fs::remove_all(root);
    if (failures) return 1;
    std::puts("checkpoint_io_test passed");
}
