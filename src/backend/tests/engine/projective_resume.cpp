#include "checkpoint/Adapt.h"
#include "engine/Engine.h"
#include "engine/EngineState.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <vector>

namespace {
void seed(std::vector<float> opacity) {
    const int64_t n = (int64_t)opacity.size();
    std::vector<float> means(3*n,0), quats(4*n,0), scales(3*n,-2), color(3*n,.5f), sh(9*n,0);
    for (int64_t i = 0; i < n; i++) { means[3*i] = (float)i; means[3*i+2] = 3+(float)i; quats[4*i] = 1; }
    auto view = [](std::vector<float>& data, std::vector<int64_t> shape) {
        return TorchTensorView{(uint64_t)data.data(),4,std::move(shape)};
    };
    set_data_3dgs(n,view(means,{n,3}),view(quats,{n,4}),view(scales,{n,3}),
                  view(opacity,{n,1}),view(color,{n,3}),view(sh,{n,3,3}),0);
    backend::device_synchronize();
}

void put_projective(const std::vector<float>& states, const std::vector<float>& information) {
    auto& state = engine();
    state.projective_state.resize(PoolSlot::EngProjectiveState,(int64_t)information.size());
    state.projective_information.resize(PoolSlot::EngProjectiveInformation,(int64_t)information.size());
    backend::memcpy_sync(state.projective_state.data_ptr(),states.data(),states.size()*4,backend::MemcpyKind::HostToDevice);
    backend::memcpy_sync(state.projective_information.data_ptr(),information.data(),information.size()*4,backend::MemcpyKind::HostToDevice);
}

void check_projective(const std::vector<float>& states, const std::vector<float>& information, const char* what) {
    std::vector<float> actual(states.size()), actual_information(information.size());
    backend::memcpy_sync(actual.data(),engine().projective_state.data_ptr(),actual.size()*4,backend::MemcpyKind::DeviceToHost);
    backend::memcpy_sync(actual_information.data(),engine().projective_information.data_ptr(),actual_information.size()*4,backend::MemcpyKind::DeviceToHost);
    if (actual != states || actual_information != information) throw std::runtime_error(what);
}
}

int main() {
    namespace fs = std::filesystem;
    auto root = fs::temp_directory_path()/
        ("ss-projective-resume-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        const std::vector<float> expected{1,.25f,.5f,17,2,-.25f,1,23}, information{7,11};
        engine_reset(); seed({0,0});
        put_projective(expected,information);
        engine_save_checkpoint((root/"same").string(),true,123);
        engine_reset(); seed({0,0});
        if (engine_load_checkpoint((root/"same").string()) != 123)
            throw std::runtime_error("projective checkpoint lost the training step");
        check_projective(expected,information,"projective checkpoint lost anchors, moments, or bearing information");

        // Resuming under a lower cap drops the two faintest splats, rows 1 and 3.
        engine_reset(); seed({0,-3,1,-5});
        put_projective({1,.25f,.5f,17, 2,-.25f,1,23, 3,.75f,2,29, 4,-.5f,3,31},{7,11,13,19});
        engine_save_checkpoint((root/"cap4").string(),true,456);
        engine_reset(); seed({0,0});
        ckpt::TargetLayout target;
        target.max_num_splats = engine_get_max_num_splats();
        target.num_sh = engine_get_num_sh();
        target.sb_lobes = engine_get_sb_lobes();
        if (!ckpt::adapt_checkpoint(root/"cap4",target,root/"cap2"))
            throw std::runtime_error("a lower splat cap did not adapt the checkpoint");
        if (engine_load_checkpoint((root/"cap2").string()) != 456)
            throw std::runtime_error("adapted projective checkpoint lost the training step");
        check_projective({1,.25f,.5f,17, 3,.75f,2,29},{7,13},"adapted checkpoint kept the wrong projective rows");
        engine_reset();
    } catch (const std::exception& error) {
        std::fprintf(stderr,"%s\n",error.what()); fs::remove_all(root); return 1;
    }
    fs::remove_all(root);
    std::puts("projective_resume passed");
}
