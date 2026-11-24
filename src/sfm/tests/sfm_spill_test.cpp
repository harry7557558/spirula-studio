// Spilled pair lists and correspondence graph (sfm/core/Spill.h) read back the
// same as the heap copies they replace, and a write to a spilled list throws.
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#include "sfm/core/Matches.h"
#include "sfm/map/CorrespondenceGraph.h"
#include "sfm/tests/TestMain.h"

#if defined(_WIN32)
#include <future>
#include <thread>
#include <windows.h>
#include <sddl.h>
#else
#include <csignal>
#include <sys/resource.h>
#endif

using namespace sfm;
namespace fs = std::filesystem;

static int fails = 0;

static void check(bool ok, const char* what) {
    std::printf("  %-58s %s\n", what, ok ? "ok" : "BAD");
    if (!ok) fails++;
}

static MatchesDatabase database(uint32_t images, uint32_t features) {
    MatchesDatabase db;
    for (uint32_t i = 0; i < images; i++)
        db.images.push_back({"img" + std::to_string(i), features});
    for (uint32_t a = 0; a < images; a++)
        for (uint32_t b = a + 1; b < images && b <= a + 3; b++) {
            TwoViewMatches p{a, b, 2, {}};
            for (uint32_t f = (a + b) % 3; f < features; f += 3)
                p.matches.push_back({f, (f * 7 + a) % features});
            db.pairs.push_back(std::move(p));
        }
    db.pairs.push_back({0, images - 1, 0, {}});  // a pair with nothing in it
    return db;
}

static std::vector<std::vector<Correspondence>> dump(const CorrespondenceGraph& g) {
    std::vector<std::vector<Correspondence>> out;
    for (uint32_t i = 0; i < g.numImages(); i++)
        for (uint32_t f = 0; f < g.numFeatures(i); f++) {
            out.emplace_back();
            for (const Correspondence& c : g.at(i, f)) out.back().push_back(c);
        }
    return out;
}

static bool same(const std::vector<std::vector<Correspondence>>& a,
                 const std::vector<std::vector<Correspondence>>& b) {
    if (a.size() != b.size()) return false;
    for (size_t k = 0; k < a.size(); k++) {
        if (a[k].size() != b[k].size()) return false;
        for (size_t j = 0; j < a[k].size(); j++)
            if (a[k][j].image_id != b[k][j].image_id ||
                a[k][j].feature_idx != b[k][j].feature_idx)
                return false;
    }
    return true;
}

static void checkChunkBoundaries(const fs::path& dir) {
    const size_t b = spirula::kFileBufferBytes;
    std::vector<uint8_t> bytes(2 * b + 30);
    for (size_t i = 0; i < bytes.size(); ++i)
        bytes[i] = static_cast<uint8_t>(i ^ (i >> 14) ^ (i >> 24));
    const auto spill = SpillFile::write((dir / "chunks.spill").string(), {
        {bytes.data(), b - 3}, {bytes.data() + b - 3, 17},
        {bytes.data() + b + 14, b + 5}, {bytes.data() + 2 * b + 19, 11}});
    check(spill && spill->size() == bytes.size() &&
              std::memcmp(spill->data(), bytes.data(), bytes.size()) == 0,
          "spill bytes match across 16 MiB boundaries and final flush");
}

#if defined(_WIN32)
class PipeListener {
public:
    bool listen(const std::string& path, SECURITY_ATTRIBUTES* security = nullptr) {
        connect_.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!connect_.hEvent) return false;
        pipe_ = CreateNamedPipeA(path.c_str(), PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
                                 PIPE_TYPE_BYTE | PIPE_WAIT, 2, 4096, 4096, 0, security);
        return pipe_ != INVALID_HANDLE_VALUE &&
               !ConnectNamedPipe(pipe_, &connect_) && GetLastError() == ERROR_IO_PENDING;
    }
    ~PipeListener() {
        if (pipe_ != INVALID_HANDLE_VALUE) {
            cancel();
            connected(TRUE);
            CloseHandle(pipe_);
        }
        if (connect_.hEvent) CloseHandle(connect_.hEvent);
    }
    void cancel() { CancelIoEx(pipe_, &connect_); }
    bool connected(BOOL wait) {
        DWORD transferred;
        return GetOverlappedResult(pipe_, &connect_, &transferred, wait) != FALSE;
    }
    bool disconnect() { return DisconnectNamedPipe(pipe_) != FALSE; }

private:
    HANDLE pipe_ = INVALID_HANDLE_VALUE;
    OVERLAPPED connect_{};
};
#endif

class WriteFailure {
public:
#if defined(_WIN32)
    explicit WriteFailure(const fs::path& path)
        : path_("\\\\.\\pipe\\ss_sfm_spill_" + std::to_string(GetCurrentProcessId()) +
                "_" + path.filename().string()) {
        // Deny DELETE so failed spill cleanup cannot connect to the map probe.
        SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, FALSE};
        if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
                L"D:(D;;SD;;;WD)(A;;GRGW;;;OW)", SDDL_REVISION_1,
                &security.lpSecurityDescriptor, nullptr))
            throw std::runtime_error("make spill pipe security descriptor");
        const bool listening = writer_.listen(path_, &security);
        LocalFree(security.lpSecurityDescriptor);
        if (!listening) throw std::runtime_error("listen for spill writer");
        closer_ = std::thread([this] {
            bool broken = false;
            if (writer_.connected(TRUE)) {
                const bool armed = probe_.listen(path_);
                broken = writer_.disconnect() && armed;
            }
            armed_.set_value(broken);
            // The issuing thread must stay alive until the probe is inspected.
            checked_.get_future().wait();
        });
    }
#else
    explicit WriteFailure(const fs::path& path) : path_(path.string()) {
        if (getrlimit(RLIMIT_FSIZE, &limit_) != 0) throw std::runtime_error("getrlimit");
        signal_ = std::signal(SIGXFSZ, SIG_IGN);
        if (signal_ == SIG_ERR) throw std::runtime_error("ignore SIGXFSZ");
        rlimit blocked = limit_;
        blocked.rlim_cur = 0;
        if (setrlimit(RLIMIT_FSIZE, &blocked) != 0) {
            std::signal(SIGXFSZ, signal_);
            throw std::runtime_error("setrlimit");
        }
    }
#endif
    ~WriteFailure() {
#if defined(_WIN32)
        writer_.cancel();
        checked_.set_value();
        closer_.join();
#else
        setrlimit(RLIMIT_FSIZE, &limit_);
        std::signal(SIGXFSZ, signal_);
#endif
    }
    const std::string& path() const { return path_; }

#if defined(_WIN32)
    bool expectMap(bool expected) {
        writer_.cancel();
        if (!armed_.get_future().get()) return false;
        const bool connected = probe_.connected(FALSE);
        return expected ? connected : !connected && GetLastError() == ERROR_IO_INCOMPLETE;
    }
#endif

private:
    std::string path_;
#if defined(_WIN32)
    PipeListener writer_, probe_;
    std::promise<bool> armed_;
    std::promise<void> checked_;
    std::thread closer_;
#else
    rlimit limit_{};
    void (*signal_)(int) = nullptr;
#endif
};

static void checkFailedSpill(const fs::path& path, bool atClose) {
    // Both payloads exceed the unread pipe's quota; only one fits the file buffer.
    const size_t n = (atClose ? spirula::kFileBufferBytes / 2
                             : spirula::kFileBufferBytes + sizeof(FeatureMatch)) /
                     sizeof(FeatureMatch);
    MatchesDatabase db;
    auto& matches = db.pairs.emplace_back().matches;
    matches.resize(n);
    for (uint32_t i = 0; i < n; ++i)
        matches.mut()[i] = {i, i ^ 0x12345678u};
    const auto* heap = matches.data();

    {
        WriteFailure target(path);
        spirula::BufferedOutputFile f(target.path(), std::ios::binary | std::ios::trunc);
        const bool opened = static_cast<bool>(f);
        const bool wrote = opened && static_cast<bool>(
            f.write(reinterpret_cast<const char*>(heap), n * sizeof(*heap)));
        f.close();
        check(opened && wrote == atClose && !f,
              atClose ? "injector fails at checked close" : "injector fails during write");
#if defined(_WIN32)
        spirula::MappedFile probe;
        probe.open(target.path());
        check(target.expectMap(true), "probe detects an attempted map open");
#endif
    }

    {
        WriteFailure target(path);
        check(!spillMatches(db, target.path()),
              atClose ? "spill reports close failure" : "spill reports write failure");
#if defined(_WIN32)
        check(target.expectMap(false), "failed spill did not attempt a map open");
#endif
    }
    bool intact = !db.spill && !matches.isView() && matches.data() == heap && matches.size() == n;
    for (uint32_t i = 0; intact && i < n; ++i)
        intact = matches[i].idx1 == i && matches[i].idx2 == (i ^ 0x12345678u);
    check(intact, "failed spill kept the original heap allocation and bytes");
    check(!fs::exists(path), "failed spill did not leave a file behind");
}

static void checkRoundTrips(const fs::path& dir) {
    MatchesDatabase db = database(12, 500);
    const MatchesDatabase heap = db;
    check(spillMatches(db, (dir / "matches.spill").string()), "pair lists spill");
    bool equal = db.pairs.size() == heap.pairs.size();
    for (size_t p = 0; equal && p < db.pairs.size(); p++) {
        equal = db.pairs[p].matches.size() == heap.pairs[p].matches.size() &&
                (db.pairs[p].matches.empty() || db.pairs[p].matches.isView());
        for (size_t k = 0; equal && k < db.pairs[p].matches.size(); k++)
            equal = db.pairs[p].matches[k].idx1 == heap.pairs[p].matches[k].idx1 &&
                    db.pairs[p].matches[k].idx2 == heap.pairs[p].matches[k].idx2;
    }
    check(equal, "... and read back as they were, from the mapping");
    bool threw = false;
    try {
        db.pairs[0].matches.push_back({0, 0});
    } catch (const std::logic_error&) {
        threw = true;
    }
    check(threw, "a write to a spilled list throws");
    const MatchesDatabase copy = db;
    check(copy.pairs[1].matches.data() == db.pairs[1].matches.data(),
          "a copy of the database shares the mapping");

    const std::vector<uint32_t> nf(db.images.size(), 500u);
    CorrespondenceGraph narrow, wide;
    narrow.build(heap, nf);
    wide.build(db, nf, /*wide=*/true);
    const auto before = dump(narrow);
    check(same(before, dump(wide)), "one- and two-word entries agree, spilled lists or not");
    check(narrow.spill((dir / "graph1.spill").string()) &&
              wide.spill((dir / "graph2.spill").string()),
          "graph spills");
    check(same(before, dump(narrow)) && same(before, dump(wide)), "... and answers as it did");
#if !defined(_WIN32)
    check(fs::is_empty(dir), "nothing is left in the directory while mapped");
#endif
}

static int run(int, char**) {
    const fs::path dir = fs::temp_directory_path() / "spirula_sfm_spill_test";
    fs::remove_all(dir);
    fs::create_directories(dir);
    checkRoundTrips(dir);
    checkChunkBoundaries(dir);
    checkFailedSpill(dir / "close-failure.spill", true);
    checkFailedSpill(dir / "write-failure.spill", false);
    check(fs::is_empty(dir), "last spill holder removes the backing file");
    fs::remove_all(dir);
    std::printf("%s\n", fails ? "FAIL" : "PASS");
    return fails ? 1 : 0;
}

int main(int argc, char** argv) { return sfmTestMain(argc, argv, run); }
