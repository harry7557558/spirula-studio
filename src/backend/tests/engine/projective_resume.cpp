#include "engine/Engine.h"
#include "engine/EngineState.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <vector>

namespace {
void seed() {
    std::vector<float> means{0,0,3,1,0,4}, quats{1,0,0,0,1,0,0,0};
    std::vector<float> scales(6,-2), opacity(2,0), color(6,.5f), sh(18,0);
    auto view = [](std::vector<float>& data, std::vector<int64_t> shape) {
        return TorchTensorView{(uint64_t)data.data(),4,std::move(shape)};
    };
    set_data_3dgs(2,view(means,{2,3}),view(quats,{2,4}),view(scales,{2,3}),
                  view(opacity,{2,1}),view(color,{2,3}),view(sh,{2,3,3}),0);
    backend::device_synchronize();
}
}

int main() {
    namespace fs = std::filesystem;
    auto root = fs::temp_directory_path()/
        ("ss-projective-resume-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const std::vector<float> expected{1,.25f,.5f,17,2,-.25f,1,23}, information{7,11};
    try {
        engine_reset(); seed();
        auto& state = engine();
        state.projective_state.resize(PoolSlot::EngProjectiveState,2);
        state.projective_information.resize(PoolSlot::EngProjectiveInformation,2);
        backend::memcpy_sync(state.projective_state.data_ptr(),expected.data(),expected.size()*4,backend::MemcpyKind::HostToDevice);
        backend::memcpy_sync(state.projective_information.data_ptr(),information.data(),information.size()*4,backend::MemcpyKind::HostToDevice);
        engine_save_checkpoint(root.string(),true,123);
        engine_reset(); seed();
        if (engine_load_checkpoint(root.string()) != 123)
            throw std::runtime_error("projective checkpoint lost the training step");
        std::vector<float> actual(expected.size()), actual_information(information.size());
        backend::memcpy_sync(actual.data(),engine().projective_state.data_ptr(),actual.size()*4,backend::MemcpyKind::DeviceToHost);
        backend::memcpy_sync(actual_information.data(),engine().projective_information.data_ptr(),actual_information.size()*4,backend::MemcpyKind::DeviceToHost);
        if (actual != expected || actual_information != information)
            throw std::runtime_error("projective checkpoint lost anchors, moments, or bearing information");
        engine_reset();
    } catch (const std::exception& error) {
        std::fprintf(stderr,"%s\n",error.what()); fs::remove_all(root); return 1;
    }
    fs::remove_all(root);
    std::puts("projective_resume passed");
}
