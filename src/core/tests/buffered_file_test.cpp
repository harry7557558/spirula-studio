#include "core/BufferedFile.h"
#include "dense/ExternalSort.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using spirula::BufferedInputFile;
using spirula::BufferedOutputFile;

namespace {
void check(bool ok, const char* what) {
    if (!ok) throw std::runtime_error(what);
}

struct Scratch {
    fs::path dir = fs::temp_directory_path() / ("spirula-buffered-file-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Scratch() { fs::create_directories(dir); }
    ~Scratch() { std::error_code ec; fs::remove_all(dir, ec); }
};

void verify(const fs::path& path, const std::string& expected) {
    std::ifstream input(path, std::ios::binary);
    std::string actual(std::istreambuf_iterator<char>{input}, {});
    check(actual == expected, "saved bytes differ");
}
}

int main() {
    try {
        Scratch scratch;
        const auto path = scratch.dir / "records.bin", other = scratch.dir / "other.bin";
        std::string expected(2 * spirula::kFileBufferBytes + 137, '\0');
        for (size_t i = 0; i < expected.size(); ++i) expected[i] = (char)((i * 37 + i / 251) & 255);
        {
            BufferedOutputFile output(path, std::ios::binary);
            for (size_t at = 0; at < expected.size();) {
                const auto n = std::min<size_t>(131, expected.size() - at);
                output.write(expected.data() + at, (std::streamsize)n);
                at += n;
            }
            BufferedOutputFile moved(std::move(output));
            moved.seekp(spirula::kFileBufferBytes - 2);
            moved.write("patch", 5);
            expected.replace(spirula::kFileBufferBytes - 2, 5, "patch");
            output.open(other, std::ios::binary);
            output << "old tail";
            output = std::move(moved);
            output.close();
            check((bool)output, "output close failed");
        }
        verify(path, expected);
        verify(other, "old tail");
        {
            BufferedInputFile input(path, std::ios::binary);
            char prefix[17]; input.read(prefix, sizeof prefix);
            check(std::string(prefix, sizeof prefix) == expected.substr(0, sizeof prefix), "prefix mismatch");
            BufferedInputFile moved(std::move(input));
            std::vector<char> block(8191);
            for (size_t at = sizeof prefix; at < expected.size();) {
                const auto n = std::min(block.size(), expected.size() - at);
                moved.read(block.data(), (std::streamsize)n);
                check(moved.gcount() == (std::streamsize)n &&
                    std::equal(block.begin(), block.begin() + n, expected.begin() + at), "read boundary mismatch");
                at += n;
            }
            check(moved.get() == std::char_traits<char>::eof() && moved.eof(), "EOF missing");
            moved.clear(); moved.seekg(spirula::kFileBufferBytes - 2);
            moved.read(block.data(), 5);
            check(std::string(block.data(), 5) == "patch", "seek after EOF failed");
            input.open(other, std::ios::binary);
            input.swap(moved);
            check(input.get() == (unsigned char)expected[spirula::kFileBufferBytes + 3], "swap lost buffered position");
            moved.close(); moved.open(other, std::ios::binary);
            std::string line; std::getline(moved, line);
            check(line == "old tail", "reopen failed");
        }
        {
            BufferedOutputFile output(other, std::ios::binary | std::ios::app);
            output << " appended";
        }
        verify(other, "old tail appended");
        {
            BufferedOutputFile output(other, std::ios::binary | std::ios::trunc);
            output << "short";
        }
        verify(other, "short");
        {
            BufferedInputFile input(scratch.dir / "missing");
            check(!input && !input.is_open(), "missing file opened");
            input.open(other);
            check((bool)input && input.get() == 's', "failed open poisoned reuse");
        }
#ifndef _WIN32
        if (fs::exists("/dev/full")) {
            BufferedOutputFile output("/dev/full", std::ios::binary);
            output << "deferred failure";
            output.close();
            check(output.fail(), "final write error was lost");
        }
#endif
        using spirula::dense::disk_io_buffer_bytes;
        check(disk_io_buffer_bytes(1024) == 0, "small budget overallocated");
        check(disk_io_buffer_bytes(256ull << 20) == (8u << 20), "8 MiB budget tier");
        check(disk_io_buffer_bytes(512ull << 20) == (16u << 20), "16 MiB budget tier");
        std::printf("PASS buffered file boundaries, lifetime, seeks and failures\n");
    } catch (const std::exception& e) {
        std::fprintf(stderr, "FAIL %s\n", e.what());
        return 1;
    }
}
