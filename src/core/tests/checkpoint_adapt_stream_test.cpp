#include "checkpoint/Adapt.h"
#include "core/CheckpointIO.h"

#include <algorithm>
#include <cstdio>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <streambuf>
#include <string>

namespace {

void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}

std::string archive() {
    std::ostringstream out;
    const std::string state = R"({"step":17,"max_num_splats":2,"cur_num_splats":2})";
    ckpt::tar_write_bytes(out, "state.json", state.data(), state.size());
    const float opacity[]{-4.f, 2.f};
    auto npy = ckpt::npy_header("<f4", 2);
    npy.append(reinterpret_cast<const char*>(opacity), sizeof(opacity));
    ckpt::tar_write_bytes(out, "world.opacities.npy", npy.data(), npy.size());
    ckpt::tar_finish(out);
    return out.str();
}

class ShortRead : public std::stringbuf {
public:
    ShortRead(const std::string& bytes, std::streamoff offset)
        : std::stringbuf(bytes, std::ios::in), offset_(offset) {}
    bool hit = false;

private:
    std::streamsize xsgetn(char* dst, std::streamsize n) override {
        if (gptr() - eback() == offset_ && n > 0) {
            hit = true;
            --n;
        }
        return std::stringbuf::xsgetn(dst, n);
    }
    std::streamoff offset_;
};

class FailingOutput : public std::streambuf {
public:
    FailingOutput(std::streamsize limit, bool fail_flush) : left_(limit), fail_flush_(fail_flush) {}
    bool hit = false;

private:
    std::streamsize xsputn(const char*, std::streamsize n) override {
        const auto written = std::min(n, left_);
        left_ -= written;
        hit |= written != n;
        return written;
    }
    int sync() override {
        hit |= fail_flush_;
        return fail_flush_ ? -1 : 0;
    }
    std::streamsize left_;
    bool fail_flush_;
};

void check_write_failure(const std::string& bytes, const ckpt::TargetLayout& target,
                         std::streamsize limit, bool fail_flush, const char* message) {
    std::istringstream source(bytes);
    FailingOutput buf(limit, fail_flush);
    std::ostream out(&buf);
    bool failed = false;
    try { ckpt::detail::adapt_checkpoint_stream(source, target, out); }
    catch (const std::runtime_error&) { failed = true; }
    require(buf.hit && failed, message);
}
}  // namespace

int main() try {
    const auto bytes = archive();
    ckpt::TargetLayout target;
    target.max_num_splats = 1;

    std::istringstream source(bytes);
    const auto members = ckpt::tar_index(source);
    require(members.size() == 2, "unexpected source archive layout");
    const auto slot = ckpt::npy_locate(source, members[1].data_offset, members[1].size);
    ShortRead short_read(bytes, static_cast<std::streamoff>(slot.data_offset));
    std::istream bad_source(&short_read);
    std::ostringstream dest;
    bool rejected = false;
    try { ckpt::detail::adapt_checkpoint_stream(bad_source, target, dest); }
    catch (const std::runtime_error& e) {
        rejected = std::string(e.what()).find("world.opacities.npy") != std::string::npos;
    }
    require(short_read.hit && rejected && dest.str().empty(), "adaptation accepted a short array read");

    require(ckpt::detail::adapt_checkpoint_stream(source, target, dest), "valid stream adaptation failed");
    std::istringstream adapted(dest.str());
    const auto adapted_members = ckpt::tar_index(adapted);
    require(adapted_members.size() == 2, "unexpected adapted archive layout");
    const auto retargeted = ckpt::npy_locate(adapted, adapted_members[1].data_offset, adapted_members[1].size);
    float retained = 0.f;
    adapted.read(reinterpret_cast<char*>(&retained), sizeof(retained));
    require(adapted && retargeted.data_bytes == sizeof(retained) && retained == 2.f,
            "adapted archive did not retain the highest-opacity row");

    check_write_failure(bytes, target, 512 + 2, false, "adaptation accepted a failed data write");
    check_write_failure(bytes, target, 2048 + 128, false, "adaptation accepted a failed footer write");
    check_write_failure(bytes, target, std::numeric_limits<std::streamsize>::max(), true,
                        "adaptation accepted a failed final flush");
    std::puts("checkpoint_adapt_stream_test passed");
    return 0;
} catch (const std::exception& e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
}
