#include "checkpoint/SplatPly.h"
#include "checkpoint/SplatTransform.h"
#include "checkpoint/SplatYUp.h"
#include "checkpoint/Resume.h"
#include "app/TrainerCore.h"
#include <fstream>
#include "engine/Engine.h"
#include "engine/EngineInternal.h"
#include "engine/EngineState.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
namespace fs = std::filesystem;

void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}

TorchTensorView view(std::vector<float>& data, std::vector<int64_t> shape) {
    return {(uint64_t)data.data(), 4, std::move(shape)};
}

void run(const fs::path& dir, int64_t n, ColorBasis basis, int bits, bool fused, bool verify, bool y_up = false) {
    engine_reset();
    const int k = basis.coeffs();
    std::vector<float> means(n * 3), quats(n * 4), scales(n * 3, -2), opacity(n, 0), dc(n * 3), sh(n * k * 3);
    for (int64_t i = 0; i < n; ++i) {
        means[i * 3] = float(i);
        means[i * 3 + 1] = float(i % 73) * .125f;
        quats[i * 4] = 1;
        dc[i * 3] = float(i % 19) * .0625f;
    }
    for (size_t i = 0; i < sh.size(); ++i) sh[i] = float(int(i % 257) - 128) / 128;
    std::vector<uint8_t> keep(n, 1);
    if (verify && n > 0) {
        for (int64_t i = 0; i < n; ++i) {
            if (i % 31 == 0) { opacity[i] = -6; keep[i] = 0; }
            if (i % 37 == 0) {
                for (int a = 0; a < 3; ++a) scales[i * 3 + a] = -40;
                keep[i] = 0;
            }
            if (i % 41 == 0) { means[i * 3 + 2] = std::numeric_limits<float>::infinity(); keep[i] = 0; }
        }
        if (bits == 32 && k > 0) { sh[3 * k] = std::numeric_limits<float>::quiet_NaN(); keep[1] = 0; }
    }
    set_data_3dgs(n, view(means, {n, 3}), view(quats, {n, 4}), view(scales, {n, 3}),
                  view(opacity, {n, 1}), view(dc, {n, 3}), view(sh, {n, k, 3}), basis.sb_lobes);
    if (bits != 32) {
        engine().optim.use_fused_proj_bwd_optim = fused;
        engine_ensure_optim_state(8, bits, 32, false);
    }
    const auto start = std::chrono::steady_clock::now();
    engine_save_checkpoint(dir.string(), false, 123, 512, "", y_up);
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    std::printf("n=%lld degree=%d lobes=%d bits=%d fused=%d y_up=%d: %.3fs, %.1f MiB/s\n",
                (long long)n, basis.sh_degree, basis.sb_lobes, bits, fused, y_up, seconds,
                double(fs::file_size(dir / "splat.ply")) / (1 << 20) / seconds);
    require(fs::exists(dir / "state.tar"), "state.tar was not published");
    for (const auto& entry : fs::directory_iterator(dir))
        require(entry.path().extension() != ".tmp", "checkpoint left a temporary file");
    if (verify) {
        const auto saved = spirula::read_splat_ply((dir / "splat.ply").string());
        const auto sample = spirula::read_splat_ply_sample((dir / "splat.ply").string(), 75);
        require(sample.num == std::min<int64_t>(saved.num, 75), "wrong preview sample size");
        require(sample.features_sh.empty(), "preview loaded view-dependent color");
        const auto colored = spirula::read_splat_ply_sample((dir / "splat.ply").string(), 75, true);
        require(colored.basis() == saved.basis() && colored.num == sample.num, "preview color basis mismatch");
        for (int64_t i = 0; i < sample.num; ++i) {
            const auto block = (i / 32) * 32;
            const auto source = block * saved.num / sample.num + i - block;
            for (int64_t k = 0; k < saved.coeffs()*3; ++k)
                require(colored.features_sh[i*saved.coeffs()*3+k] == saved.features_sh[source*saved.coeffs()*3+k],
                        "preview coefficient mismatch");
            for (int axis = 0; axis < 3; ++axis) {
                require(sample.means[i*3+axis] == saved.means[source*3+axis], "preview position mismatch");
                require(sample.features_dc[i*3+axis] == saved.features_dc[source*3+axis], "preview color mismatch");
            }
        }
        require(saved.basis() == basis, "checkpoint changed the colour basis");
        int64_t row = 0;
        for (int64_t i = 0; i < n; ++i) {
            if (!keep[i]) continue;
            require(row < saved.num, "checkpoint dropped a live splat");
            for (int a = 0; a < 3; ++a) {
                require(saved.means[row * 3 + a] == means[i * 3 + a], "positions or row order changed");
                require(saved.scales[row * 3 + a] == scales[i * 3 + a], "scales changed");
                require(saved.features_dc[row * 3 + a] == dc[i * 3 + a], "DC changed");
            }
            for (int a = 0; a < 4; ++a)
                require(std::abs(saved.quats[row * 4 + a] - quats[i * 4 + a]) < 2e-6f, "quaternions changed");
            require(saved.opacities[row] == opacity[i], "opacity changed");
            const float tolerance = (y_up ? 2e-6f : 0.f) +
                (bits == 32 ? 0 : bits == 8 ? 1.0f / 255 + 1e-6f : 1.0f / 65535 + 1e-6f);
            for (int a = 0; a < k * 3; ++a)
                require(std::fabs(saved.features_sh[row * k * 3 + a] - sh[i * k * 3 + a]) <= tolerance,
                        "SH or lobe values changed beyond quantization error");
            ++row;
        }
        require(row == saved.num, "checkpoint retained a filtered splat");
        require(fs::file_size(dir / "splat.ply") ==
                spirula::splat_ply_header(row, basis, engine().primitive, y_up).size() +
                size_t(row) * spirula::splat_ply_row_floats(basis) * sizeof(float),
                "checkpoint has extra or missing bytes");
    }
    fs::remove_all(dir);
}

void conversion() {
    const spirula::SplatYUp fast;
    const spirula::Sim3 rotation{1, {-1,0,0, 0,0,-1, 0,-1,0}, {0,0,0}};
    for (int degree = 0; degree <= 4; ++degree) {
        ColorBasis basis{degree, 3};
        const spirula::SplatTransform reference(rotation, basis);
        for (int sample = 0; sample < 50; ++sample) {
            float p[3] = {.3f, -.5f, 1.2f}, q[4] = {.7f, .2f, -.4f, .1f};
            float pr[3], qr[4], scales[3] = {-2,-1,-3};
            std::copy(p, p+3, pr); std::copy(q, q+4, qr);
            std::vector<float> rest(3*basis.coeffs());
            for (size_t i = 0; i < rest.size(); ++i) rest[i] = std::sin(float(i+sample));
            auto original = rest, expected = rest;
            reference.apply(pr, qr, scales, expected.data());
            fast.apply(p, q, rest.data(), basis);
            for (int a = 0; a < 3; ++a) require(p[a] == pr[a], "Y-up position convention");
            float sign = q[0]*qr[0]+q[1]*qr[1]+q[2]*qr[2]+q[3]*qr[3] < 0 ? -1.f : 1.f;
            for (int a = 0; a < 4; ++a) require(std::abs(q[a]-sign*qr[a]) < 2e-6f, "Y-up quaternion convention");
            for (size_t a = 0; a < rest.size(); ++a)
                require(std::abs(rest[a]-expected[a]) < 2e-6f, "Y-up SH/lobe reference mismatch");
            fast.apply(p, q, rest.data(), basis, true);
            for (int a = 0; a < 3*basis.sh_coeffs(); ++a)
                require(std::abs(rest[a]-original[a]) < 2e-6f, "SH round trip");
            for (int l = 0; l < basis.sb_lobes; ++l) {
                const int at = 3*basis.sh_coeffs()+6*l;
                double a[3], b[3];
                spirula::sb::axis(rest[at+3], rest[at+4], a);
                spirula::sb::axis(original[at+3], original[at+4], b);
                for (int k = 0; k < 3; ++k) require(std::abs(a[k]-b[k]) < 2e-6, "lobe axis round trip");
            }
        }
    }
}

void layout(const fs::path& dir) {
    engine_reset();
    const int64_t n = 300003;
    std::vector<float> means(n*3, 1), quats(n*4, .5f), scales(n*3, -2), dc(n*3, .1f), opacity(n, 0), sh(n*45, .2f);
    for (size_t i = 0; i < sh.size(); ++i) sh[i] = float(i % 100003) / 100003.f;
    set_data_3dgs(n, view(means, {n,3}), view(quats, {n,4}), view(scales, {n,3}),
                  view(opacity, {n,1}), view(dc, {n,3}), view(sh, {n,15,3}), 0);
    engine_ensure_optim_state(32, 32, 32, false);
    spirula::TrainerSession session;
    session.out_dir = dir;
    session.cfg.export_y_up = true;
    const std::string name = dir.filename().string();
    fs::create_directories(dir);
    { std::ofstream cfg(dir / "config.json"); cfg << "{}"; }
    session.save_checkpoint(12, true);
    const auto ply = dir/(name+".ply"), tar = dir/(name+".tar");
    require(fs::exists(ply) && fs::exists(tar), "flat checkpoint names");
    require(fs::hard_link_count(ply) == 1, "checkpoint uses a hard link");
    require(!fs::exists(dir/"splat.ply"), "duplicate PLY");
    require(ckpt::resolve_checkpoint(dir).tar_path == tar, "flat run resolution");
    require(ckpt::resolve_checkpoint(ply).tar_path == tar, "paired PLY resume");
    require(spirula::find_splat_ply(tar.string()).first == ply.string(), "archive to PLY resolution");
    backend::memset_sync(engine().world.means.data_ptr(), 0, means.size()*4);
    backend::memset_sync(engine().world.features_sh.data_ptr(), 0, sh.size()*4);
    require(engine_load_checkpoint(tar.string()) == 12, "flat archive resume");
    std::vector<float> restored(means.size());
    backend::memcpy_sync(restored.data(), engine().world.means.data_ptr(), restored.size()*4, backend::MemcpyKind::DeviceToHost);
    require(restored == means, "resume rotated native training coordinates");
    restored.resize(sh.size());
    backend::memcpy_sync(restored.data(), engine().world.features_sh.data_ptr(), restored.size()*4, backend::MemcpyKind::DeviceToHost);
    require(restored == sh, "streamed archive restore corrupted colour buffers");
    session.cfg.save_only_latest_checkpoint = false;
    session.save_checkpoint(123, true);
    const auto stepped = dir/(name+"_step-000000123.tar");
    require(ckpt::resolve_checkpoint(dir).tar_path == stepped, "latest retained checkpoint selection");
    require(spirula::find_splat_ply(dir.string()).first == fs::path(stepped).replace_extension(".ply").string(), "latest retained PLY selection");
    require(engine_load_checkpoint(stepped.string()) == 123, "retained archive resume");
    session.cfg.save_only_latest_checkpoint = true;
    session.save_checkpoint(124, true);
    require(!fs::exists(stepped), "latest-only did not prune old pair");
    require(ckpt::read_state_json(dir).find("step")->as_int() == 124, "latest-only replacement");
    fs::remove_all(dir);
}

}

int main(int argc, char** argv) {
    const fs::path parent = argc > 1 ? fs::path(argv[1]) : fs::temp_directory_path();
    const auto dir = parent / ("ss-checkpoint-ply-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        if (argc > 2) {
            const int64_t n = std::stoll(argv[2]);
            const int bits = argc > 3 ? std::stoi(argv[3]) : 8;
            const bool fused = argc > 4 ? std::stoi(argv[4]) != 0 : true;
            const bool y_up = argc > 5 && std::stoi(argv[5]) != 0;
            for (int repeat = 0; repeat < 3; ++repeat) run(dir, n, {3, 0}, bits, fused, false, y_up);
        } else {
            conversion();
            layout(dir);
            run(dir, 1, {0, 0}, 32, false, true);
            run(dir, 140003, {0, 0}, 32, false, true);
            run(dir, 70003, {3, 0}, 32, false, true);
            run(dir, 140003, {4, 0}, 32, false, true, true);
            run(dir, 140003, {3, 0}, 8, true, true, true);
            for (bool fused : {false, true})
                for (int bits : {8, 16}) run(dir, 70003, {3, 0}, bits, fused, true);
#ifdef SS_BACKEND_VULKAN
            run(dir, 70003, {2, 3}, 32, false, true);
            run(dir, 70003, {2, 3}, 8, true, true);
#endif
        }
        engine_reset();
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        fs::remove_all(dir);
        return 1;
    }
}
