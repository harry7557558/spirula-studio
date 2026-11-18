// nn::save_image / save_gray_png and sam::save_mask_png must fail when the
// file write fails, and must write the same bytes stb encodes when it does not.
// Writes fail through a named pipe whose server closes unread on Windows, and
// RLIMIT_FSIZE on POSIX; both let fopen succeed, as a full disk or dropped share does.

#include "external/stb_image_write.h"
#include "nn/io/Image.h"
#include "sam/Sam.h"

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

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

namespace fs = std::filesystem;

namespace {

void require(bool ok, const std::string& what) {
    if (!ok) throw std::runtime_error(what);
}

std::vector<uint8_t> read_all(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

void append(void* context, void* data, int size) {
    auto* out = static_cast<std::vector<uint8_t>*>(context);
    out->insert(out->end(), (uint8_t*)data, (uint8_t*)data + size);
}

// Every write to path() fails while this is alive. On Windows the writer's open
// succeeds and its writes block until the pipe server closes without reading.
class WriteBlocker {
public:
    WriteBlocker(const fs::path& dir, const std::string& name) {
#ifdef _WIN32
        (void)dir;
        path_ = "\\\\.\\pipe\\ss_image_write_test_" + std::to_string(GetCurrentProcessId()) + "_" + name;
        server_ = CreateNamedPipeA(path_.c_str(), PIPE_ACCESS_INBOUND, PIPE_TYPE_BYTE | PIPE_WAIT,
                                   1, 0, 0, 0, nullptr);
        require(server_ != INVALID_HANDLE_VALUE, "could not create the write target pipe");
        closer_ = std::thread([this] {
            ConnectNamedPipe(server_, nullptr);
            Sleep(100);
            CloseHandle(server_);
        });
#else
        path_ = (dir / name).string();
        std::signal(SIGXFSZ, SIG_IGN);
        require(getrlimit(RLIMIT_FSIZE, &old_) == 0, "getrlimit");
        rlimit tiny = old_;
        tiny.rlim_cur = 16;
        require(setrlimit(RLIMIT_FSIZE, &tiny) == 0, "setrlimit");
#endif
    }
    ~WriteBlocker() {
#ifdef _WIN32
        // Releases ConnectNamedPipe when the writer failed before opening.
        HANDLE late = CreateFileA(path_.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (late != INVALID_HANDLE_VALUE) CloseHandle(late);
        closer_.join();
#else
        setrlimit(RLIMIT_FSIZE, &old_);
#endif
    }
    const std::string& path() const { return path_; }

private:
    std::string path_;
#ifdef _WIN32
    HANDLE server_ = INVALID_HANDLE_VALUE;
    std::thread closer_;
#else
    rlimit old_{};
#endif
};

}  // namespace

int main() {
    try {
        const fs::path dir = fs::temp_directory_path() / "ss_image_write_test";
        fs::remove_all(dir);
        fs::create_directories(dir);

        // Small enough to sit in the stdio buffer, so only fclose sees the
        // error; large enough that fwrite itself fails.
        for (int side : {16, 1024}) {
            sam::Mask mask;
            mask.width = side;
            mask.height = side;
            mask.data.resize((size_t)side * side);
            uint32_t state = 12345;
            for (auto& v : mask.data) v = (uint8_t)((state = state * 1664525u + 1013904223u) >> 24);

            nn::Image rgb;
            rgb.width = side;
            rgb.height = side;
            rgb.data.resize((size_t)side * side * 3);
            for (size_t i = 0; i < rgb.data.size(); ++i) rgb.data[i] = mask.data[i / 3] ^ (uint8_t)i;

            std::vector<uint8_t> want_gray, want_png, want_jpg;
            stbi_write_png_to_func(append, &want_gray, side, side, 1, mask.data.data(), side);
            stbi_write_png_to_func(append, &want_png, side, side, 3, rgb.data.data(), side * 3);
            stbi_write_jpg_to_func(append, &want_jpg, side, side, 3, rgb.data.data(), 90);

            const fs::path gray = dir / "gray.png", png = dir / "rgb.png", jpg = dir / "rgb.jpg";
            const std::string at = " at " + std::to_string(side) + " px";
            require(sam::save_mask_png(mask, gray.string()) && read_all(gray) == want_gray,
                    "mask PNG bytes" + at);
            require(nn::save_image(rgb, png.string(), -1) && read_all(png) == want_png,
                    "RGB PNG bytes" + at);
            require(nn::save_image(rgb, jpg.string(), 90) && read_all(jpg) == want_jpg,
                    "JPEG bytes" + at);

            const std::pair<const char*, std::function<bool(const std::string&)>> failing_saves[] = {
                {"mask", [&](const std::string& p) { return sam::save_mask_png(mask, p); }},
                {"PNG", [&](const std::string& p) { return nn::save_image(rgb, p, -1); }},
                {"JPEG", [&](const std::string& p) { return nn::save_image(rgb, p, 90); }},
            };
            for (const auto& [kind, save] : failing_saves) {
                std::string target;
                {
                    WriteBlocker block(dir, kind + std::to_string(side));
                    target = block.path();
                    require(!save(target), std::string("failed ") + kind + " write reported success" + at);
                }
                std::error_code ec;
                require(!fs::exists(target, ec), std::string("failed ") + kind + " write left a file behind" + at);
            }
            std::printf("PASS checked image writes at %d px\n", side);
        }
        fs::remove_all(dir);
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "FAIL %s\n", e.what());
        return 1;
    }
}
