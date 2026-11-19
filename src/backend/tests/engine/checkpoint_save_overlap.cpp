#include "app/TrainerCore.h"
#include "checkpoint/Resume.h"
#include "checkpoint/StagedFiles.h"
#include "engine/EngineInternal.h"
#include "engine/EngineState.h"
#include "external/stb_image_write.h"

#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
namespace fs = std::filesystem;
using Files = std::array<fs::path, 3>;
using Bytes = std::array<std::string, 3>;

void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}

Files products(const fs::path& root, const std::string& stem) {
    return {root / (stem + ".ply"), root / "background.webp", root / (stem + ".tar")};
}

fs::path temp_of(fs::path path) { return path.concat(".tmp"); }

Bytes read(const Files& paths) {
    Bytes bytes;
    for (size_t i = 0; i < paths.size(); ++i) {
        std::ifstream in(paths[i], std::ios::binary);
        require(bool(in), "could not open checkpoint artifact");
        bytes[i].assign(std::istreambuf_iterator<char>(in), {});
    }
    return bytes;
}

void assert_clean(const Files& paths) {
    for (const auto& p : paths) require(!fs::exists(temp_of(p)), "checkpoint left staging behind");
}

void write(const fs::path& path, const std::string& bytes) {
    std::ofstream out(path, std::ios::binary);
    out << bytes;
    out.close();
    require(bool(out), "could not write checkpoint test fixture");
}

template<class Action>
void fails(Action&& action, const std::string& detail) {
    try {
        action();
    } catch (const std::exception& e) {
        if (std::string(e.what()).find(detail) != std::string::npos) return;
        throw;
    }
    throw std::runtime_error("checkpoint did not report failure: " + detail);
}

void staging(const fs::path& root) {
    fs::create_directory(root);
    const Files paths = products(root, "run");
    const Bytes old{"old PLY", "old WebP", "old TAR"}, next{"new PLY", "new WebP", "new TAR"};
    for (size_t i = 0; i < paths.size(); ++i) write(paths[i], old[i]);
    for (size_t fault = 0; fault < paths.size(); ++fault) {
        fails([&] {
            ckpt::StagedFiles staged;
            for (size_t i = 0; i < paths.size(); ++i) {
                staged.write(paths[i], [&](std::ostream& out) {
                    out << next[i];
                    require(read(paths) == old, "reader saw staged bytes before publication");
                    if (i == fault) out.setstate(std::ios::badbit);
                });
            }
            staged.publish();
        }, temp_of(paths[fault]).filename().string());
        require(read(paths) == old, "failed write changed a final file");
        assert_clean(paths);
    }
    ckpt::StagedFiles staged;
    for (size_t i = 0; i < paths.size(); ++i) {
        write(temp_of(paths[i]), std::string(65536, 'x'));
        staged.write(paths[i], [&](std::ostream& out) { out << next[i]; });
    }
    require(read(paths) == old, "closing staging changed the previous checkpoint");
    staged.publish();
    require(read(paths) == next, "retry did not replace stale staging and publish exact bytes");
    assert_clean(paths);
}

TorchTensorView view(const void* data, std::vector<int64_t> shape) {
    return {(uint64_t)data, 4, std::move(shape)};
}

void set_background(float dc) {
    const std::array<float, 3> bg{dc, dc, dc};
    backend::memcpy_sync(engine().background.sh_coeffs.data_ptr(), bg.data(), sizeof(bg),
                         backend::MemcpyKind::HostToDevice);
}

void seed(int64_t n = 1) {
    engine_reset();
    std::vector<float> means(3*n), quats(4*n), scales(3*n, -2), opacity(n, 1),
        dc(3*n), sh(9*n);
    for (int64_t i = 0; i < n; ++i) {
        means[3*i] = float(i % 73) * .01f;
        means[3*i+2] = 3;
        quats[4*i] = 1;
        for (int c = 0; c < 3; ++c) {
            dc[3*i+c] = .25f * (c+1);
            sh[9*i+c] = dc[3*i+c] + float(i % 17) * .01f;
            sh[9*i+3+c] = -dc[3*i+c];
        }
    }
    set_data_3dgs(n, view(means.data(), {n, 3}), view(quats.data(), {n, 4}),
        view(scales.data(), {n, 3}), view(opacity.data(), {n, 1}), view(dc.data(), {n, 3}),
        view(sh.data(), {n, 3, 3}), 0);
    engine_init_background_sh(0, 2, true);
    engine_init_color_space(true, 2, true, {.5f, 0, 0, 0, 1, 0, 0, 0, 2}, false, 0, false, {});
    set_background(-.25f);
}

std::future<void> save_async(const fs::path& root, bool full, int step) {
    std::mutex engine_mutex;
    return engine_try_save_checkpoint_async(engine_mutex, 0,
        root.string(), full, step, 16, "run", true, "").pending;
}

void snapshot(const fs::path& root) {
    for (bool full : {false, true}) {
        const auto reference = root / (full ? "full" : "inference");
        const auto dest = root / "snapshot";
        seed();
        engine().optim.use_fused_proj_bwd_optim = full;
        engine_ensure_optim_state(8, full ? 8 : 16, 32, false);
        engine_save_checkpoint(reference.string(), full, 10, 16, "run", true);
        auto save = save_async(dest, full, 10);
        require(save.valid(), "host memory observations did not admit the tiny checkpoint snapshot");
        set_background(.75f);
        const float3 position{1, 0, 3};
        backend::memcpy_sync(engine().world.means.data_ptr(), &position, sizeof(position),
                             backend::MemcpyKind::HostToDevice);
        save.get();
        require(read(products(reference, "run")) == read(products(dest, "run")),
                "snapshot and streaming serializers disagreed");
        assert_clean(products(dest, "run"));
    }
    std::mutex engine_mutex;
    const auto rejected = engine_try_save_checkpoint_async(engine_mutex, 1.e18, (root / "no-room").string(),
        true, 11, 16, "run", true, "");
    require(!rejected.pending.valid() && std::string(rejected.reason) == "insufficient_headroom",
            "capture ignored projected growth in the headroom check");
    require(!fs::exists(root / "no-room"), "headroom fallback started a writer");
}

void multi_chunk_ply(const fs::path& root) {
    constexpr int64_t n = 220003;
    seed(n);
    engine().optim.use_fused_proj_bwd_optim = true;
    engine_ensure_optim_state(8, 16, 32, false);
    const auto reference = root / "streaming", dest = root / "frozen";
    engine_save_checkpoint(reference.string(), false, 12, 16, "run", true);
    require(fs::file_size(reference / "run.ply") > spirula::kFileBufferBytes, "PLY fixture fit in one chunk");
    auto save = save_async(dest, false, 12);
    require(save.valid(), "multi-chunk PLY did not launch an async checkpoint");
    save.get();
    require(read(products(reference, "run")) == read(products(dest, "run")),
            "multi-chunk quantized snapshot and streaming bytes differed");
}

void publication(const fs::path& root) {
    seed();
    const Files paths = products(root, "run");
    engine_save_checkpoint(root.string(), true, 20, 16, "run", true);
    const Bytes old = read(paths);
    set_background(.5f);
    const float3 changed{1, 0, 3};
    backend::memcpy_sync(engine().world.means.data_ptr(), &changed, sizeof(changed),
                         backend::MemcpyKind::HostToDevice);
    for (const auto& path : paths) {
        const auto blocked = temp_of(path);
        fs::create_directory(blocked);
        auto save = save_async(root, true, 21);
        require(save.valid(), "fault test did not launch an async checkpoint");
        fails([&] { save.get(); }, blocked.filename().string());
        require(fs::is_directory(blocked), "failed save removed an unowned blocking directory");
        fs::remove(blocked);
        require(read(paths) == old, "staging fault changed the previous generation");
        assert_clean(paths);
    }
#ifdef _WIN32
    for (size_t i = 0; i < paths.size(); ++i) {
        engine_save_checkpoint(root.string(), true, 20, 16, "run", true);
        const Bytes prior = read(paths);
        const float3 next{float(i + 2), 0, 3};
        backend::memcpy_sync(engine().world.means.data_ptr(), &next, sizeof(next), backend::MemcpyKind::HostToDevice);
        set_background(float(i) * .3f);
        struct HeldFile {
            HANDLE handle;
            ~HeldFile() { if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle); }
        } reader{CreateFileW(paths[i].c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                             nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr)};
        require(reader.handle != INVALID_HANDLE_VALUE, "could not open test reader without delete sharing");
        auto save = save_async(root, true, 21);
        require(save.valid(), "held-reader test did not launch an async checkpoint");
        fails([&] { save.get(); }, paths[i].filename().string());
        const Bytes current = read(paths);
        for (size_t j = 0; j < paths.size(); ++j)
            require((current[j] == prior[j]) == (j >= i), "wrong publish order after replace failure");
        assert_clean(paths);
    }
#endif
    auto retry = save_async(root, true, 21);
    require(retry.valid(), "retry did not use async save");
    retry.get();
    require(ckpt::read_state_json(paths.back()).find("step")->as_int() == 21, "same-path retry did not publish TAR");
    assert_clean(paths);
}

void setup_data(const fs::path& root, bool missing = false) {
    const auto image = root / "input.png";
    if (!missing) {
        std::vector<uint8_t> rgb(16 * 16 * 3, 127);
        require(stbi_write_png(image.string().c_str(), 16, 16, 3, rgb.data(), 48) != 0,
                "could not write training image");
    }
    std::vector<float> viewmat(16, 0);
    for (int i = 0; i < 4; ++i) viewmat[5*i] = 1;
    DataManagerConfig dm;
    dm.cache_mode = missing ? CacheMode::DISK : CacheMode::CPU;
    dm.load_masks = dm.load_depths = dm.load_normals = false;
    engine_setup_data_manager(dm, {0}, {0}, {image.string()}, {}, {}, {}, {16}, {16}, {}, {},
        viewmat, {20,20,8,8}, std::vector<float>(8,0), {}, {}, {}, {}, {}, {}, {}, {0}, {});
}

void drain(const fs::path& root, bool fail, bool final_save, bool stop = true) {
    seed();
    fs::create_directory(root);
    setup_data(root);
    const auto stem = root.filename().string();
    const auto paths = products(root, stem);

    spirula::TrainerSession session;
    session.out_dir = root;
    session.cfg.num_iterations = stop ? 16 : 5;
    session.cfg.steps_per_save = 4;
    session.cfg.save_only_latest_checkpoint = true;
    session.cfg.background_export_height = 16;
    session.cfg.sh_degree = 1;
    session.cfg.use_fused_proj_bwd_optim = false;
    session.cfg.background_mode = "sh";
    session.cfg.background_sh_degree = 0;
    session.save_checkpoint(0, true);
    const Bytes old = read(paths);
    const auto stale = root / (stem + "_step-000000000.tar");
    fs::copy_file(paths.back(), stale);
    if (fail) fs::create_directory(temp_of(paths.back()));
    spirula::TrainerCallbacks callbacks;
    std::promise<void> writer_entered, release_writer;
    auto entered = writer_entered.get_future();
    auto released = release_writer.get_future();
    bool advanced = false;
    callbacks.checkpoint_writer_gate = [&] {
        writer_entered.set_value();
        if (released.wait_for(std::chrono::seconds(30)) != std::future_status::ready)
            throw std::runtime_error("writer did not overlap the training step");
    };
    callbacks.on_step = [&](const spirula::TrainerProgress& p) {
        if (p.step != 4) return;
        const bool writer_pending = entered.wait_for(std::chrono::seconds(10)) == std::future_status::ready;
        const bool retained = fs::exists(stale) && read(paths) == old;
        advanced = true;
        session.save_on_stop = final_save;
        session.stop_requested = stop;
        release_writer.set_value();
        require(writer_pending, "training advanced without entering the async writer");
        require(retained, "publication or retention ran before the pending writer was released");
    };
    if (fail) {
        fails([&] { session.train(callbacks); }, temp_of(paths.back()).filename().string());
        require(fs::exists(stale) && read(paths) == old, "failed drain changed retention or finals");
        fs::remove(temp_of(paths.back()));
    } else {
        session.train(callbacks);
        const auto state = ckpt::read_state_json(paths.back());
        require(state.find("step")->as_int() == (final_save ? 5 : 4), "drain published the wrong generation");
        require(state.find("full_resume")->as_int() == int(stop), "unexpected checkpoint resume mode");
        require(!fs::exists(stale), "successful drain did not prune the old checkpoint");
    }
    require(advanced && session.cur_step.load() == 5, "did not train the save-boundary step");
    assert_clean(paths);
}

void final_collision(const fs::path& root, bool decode_error) {
    seed();
    fs::create_directory(root);
    if (decode_error) setup_data(root, true);
    spirula::TrainerSession session;
    session.out_dir = root;
    session.cfg.save_only_latest_checkpoint = false;
    session.cfg.steps_per_save = -1;
    session.cfg.background_export_height = 16;
    session.start_step = 4;
    session.cfg.num_iterations = decode_error ? 16 : session.start_step;
    const auto paths = products(root, root.filename().string() + "_step-000000004");
    session.save_checkpoint(session.start_step, false);
    const Bytes prior = read(paths);
    bool declined = false;
    spirula::TrainerCallbacks callbacks;
    callbacks.on_data_error = [&](const std::string&) { declined = true; return false; };
    session.cfg.save_full_checkpoint = !decode_error;
    session.train(callbacks);
    require(declined == decode_error, "unexpected data-error path");
    const auto state = ckpt::read_state_json(paths.back());
    require(state.find("step")->as_int() == session.start_step && state.find("full_resume")->as_int() == 1,
            "final save did not replace the existing stepped checkpoint with resumable state");
    require(read(paths) != prior, "same-step final save did not replace the prior generation");
    assert_clean(paths);
}
}  // namespace

int main() {
    const auto root = fs::temp_directory_path() / ("ss-checkpoint-overlap-" + std::to_string(
        static_cast<long long>(std::chrono::steady_clock::now().time_since_epoch().count())));
    if (!fs::create_directory(root)) return 1;
    int status = 0;
    try {
#ifdef _WIN32
        require(_putenv_s("SS_TRAIN_PERF", "1") == 0, "could not enable training perf log");
#else
        require(setenv("SS_TRAIN_PERF", "1", 1) == 0, "could not enable training perf log");
#endif
        staging(root / "staging");
        snapshot(root);
        multi_chunk_ply(root / "multi-chunk");
        publication(root / "publication");
        drain(root / "stop-without-save", false, false);
        {
            std::ifstream perf(root / "stop-without-save" / "checkpoint_saves.csv");
            std::string header, row;
            require(bool(std::getline(perf, header)) && header == "step,path,reason" &&
                    bool(std::getline(perf, row)) && row == "4,overlapped,snapshot",
                    "missing periodic save decision in the perf log");
        }
        drain(root / "stop-and-save", false, true);
        drain(root / "stop-with-failed-save", true, true);
        drain(root / "natural-end", false, true, false);
        final_collision(root / "no-step-resume", false);
        final_collision(root / "decline-retry", true);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "checkpoint_save_overlap: %s\n", e.what());
        status = 1;
    }
    engine_reset();
    fs::remove_all(root);
    if (!status) std::puts("checkpoint_save_overlap passed");
    return status;
}
