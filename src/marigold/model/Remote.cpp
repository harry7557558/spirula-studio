#include "marigold/model/Remote.h"

#include "core/Env.h"
#include "marigold/Common.h"

#include <chrono>
#include <cstdio>
#include <cstring>

// glibc's popen takes "r" and refuses "rb"; Windows' needs the b.
#ifdef _WIN32
#define SS_POPEN(cmd) _popen(cmd, "rb")
#define SS_PCLOSE _pclose
#else
#define SS_POPEN(cmd) popen(cmd, "r")
#define SS_PCLOSE pclose
#endif

namespace marigold {
namespace {

// Reconnections per range before giving up, alternating hosts.
constexpr int kMaxAttempts = 8;
constexpr size_t kChunk = 4u << 20;
// Read ahead of the converter: ~1 s of a fast connection.
constexpr uint64_t kAhead = 64ull << 20;

// SS_HF_ENDPOINT stands in for https://huggingface.co, as HF_ENDPOINT does for
// huggingface_hub: a mirror such as hf-mirror.com, or a local copy (file://).
std::string endpoint(const char* url) {
    static const std::string hf = "https://huggingface.co";
    const char* e = spirula::env("HF_ENDPOINT");
    const std::string u = url;
    return e && *e && u.rfind(hf, 0) == 0 ? std::string(e) + u.substr(hf.size()) : u;
}

std::FILE* open_curl(const char* url, uint64_t a, uint64_t b) {
    // -f turns an HTTP error into an exit code instead of an error page in the
    // stream; the timeouts turn a stalled host into a switch to the other.
    const std::string cmd = "curl -L -f -s -S --connect-timeout 30 --speed-limit 1024 "
                            "--speed-time 60 -r " + std::to_string(a) + "-" +
                            std::to_string(b - 1) + " \"" + endpoint(url) + "\"";
    std::FILE* p = SS_POPEN(cmd.c_str());
    NN_CHECK(p, "cannot run curl, which is how Marigold v2 is fetched");
    return p;
}

}  // namespace

RemoteRange::RemoteRange(const RemoteFile& f, uint64_t begin, uint64_t end,
                         const std::atomic<bool>* cancel)
    : f_(f), pos_(begin), end_(end), cancel_(cancel) {
    if (begin < end) worker_ = std::thread([this] { fetch(); });
    else done_ = true;
}

RemoteRange::~RemoteRange() {
    {
        std::lock_guard<std::mutex> lk(mu_);
        stop_ = true;
    }
    cv_.notify_all();
    if (worker_.joinable()) worker_.join();
}

void RemoteRange::fetch() {
    uint64_t at = pos_;
    int host = 0, attempts = 0;
    std::string why;
    try {
        while (at < end_ && !stop_) {
            NN_CHECK(attempts < kMaxAttempts,
                     "downloading %s failed: %s\n  tried %s\n  and %s", f_.name, why.c_str(),
                     f_.url, f_.mirror);
            ++attempts;
            std::FILE* p = open_curl(host == 0 ? f_.url : f_.mirror, at, end_);
            std::vector<uint8_t> chunk(kChunk);
            size_t fill = 0;
            while (at < end_ && !stop_) {
                const size_t got =
                    std::fread(chunk.data() + fill, 1,
                               (size_t)std::min<uint64_t>(kChunk - fill, end_ - at), p);
                if (got == 0) break;
                fill += got;
                at += got;
                if (fill == kChunk || at == end_) {
                    std::unique_lock<std::mutex> lk(mu_);
                    cv_.wait(lk, [&] { return stop_ || buffered_ < kAhead; });
                    buffered_ += fill;
                    chunk.resize(fill);
                    chunks_.push_back(std::move(chunk));
                    cv_.notify_all();
                    chunk.assign(kChunk, 0);
                    fill = 0;
                }
            }
            const int rc = SS_PCLOSE(p);
            if (fill) {
                std::lock_guard<std::mutex> lk(mu_);
                buffered_ += fill;
                chunk.resize(fill);
                chunks_.push_back(std::move(chunk));
                cv_.notify_all();
            }
            if (at < end_) {
                why = rc ? "curl exited with status " + std::to_string(rc)
                         : std::string("the connection closed early");
                host ^= 1;
            }
        }
    } catch (...) {
        std::lock_guard<std::mutex> lk(mu_);
        error_ = std::current_exception();
    }
    std::lock_guard<std::mutex> lk(mu_);
    done_ = true;
    cv_.notify_all();
}

void RemoteRange::read(void* dst, size_t n) {
    NN_CHECK(pos_ + n <= end_, "%s: read past the requested range", f_.name);
    uint8_t* out = static_cast<uint8_t*>(dst);
    std::unique_lock<std::mutex> lk(mu_);
    while (n) {
        cv_.wait_for(lk, std::chrono::milliseconds(200),
                     [&] { return !chunks_.empty() || done_; });
        if (cancel_ && cancel_->load()) throw Cancelled();
        if (chunks_.empty()) {
            if (!done_) continue;
            if (error_) std::rethrow_exception(error_);
            fail("%s: the stream ended %llu bytes early", f_.name, (unsigned long long)n);
        }
        std::vector<uint8_t>& c = chunks_.front();
        const size_t take = std::min(n, c.size() - head_);
        std::memcpy(out, c.data() + head_, take);
        out += take;
        n -= take;
        pos_ += take;
        head_ += take;
        buffered_ -= take;
        if (head_ == c.size()) {
            chunks_.pop_front();
            head_ = 0;
        }
        cv_.notify_all();
    }
}

std::string remote_safetensors_header(const RemoteFile& f, uint64_t* data_offset,
                                      const std::atomic<bool>* cancel) {
    uint8_t len_le[8];
    RemoteRange(f, 0, 8, cancel).read(len_le, 8);
    uint64_t n = 0;
    for (int i = 7; i >= 0; --i) n = (n << 8) | len_le[i];
    NN_CHECK(n > 1 && n < (100ull << 20) && 8 + n <= f.bytes,
             "%s: a header length of %llu does not fit the file", f.name,
             (unsigned long long)n);
    std::string json((size_t)n, '\0');
    RemoteRange(f, 8, 8 + n, cancel).read(&json[0], (size_t)n);
    *data_offset = 8 + n;
    return json;
}

}  // namespace marigold
