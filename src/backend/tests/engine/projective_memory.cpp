#include "engine/Engine.h"
#include "engine/EngineInternal.h"
#include "engine/EngineState.h"
#include "external/stb_image_write.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <vector>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
void require_no_projective_memory() {
    auto& s = engine();
    require(!s.projective_state.data_ptr() && !s.projective_information.data_ptr()
        && !s.projective_centers.data_ptr(),"disabled projective depth retained a device view");
    for (auto slot : {PoolSlot::EngProjectiveState,PoolSlot::EngProjectiveInformation,PoolSlot::EngProjectiveCenters}) {
        void* ptr = nullptr; size_t bytes = 0;
        require(!DevicePool::global().lookup(slot,0,ptr,bytes),"disabled projective depth retained a pool allocation");
    }
}
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
        ("ss-projective-memory-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        fs::create_directories(root); engine_reset(); seed();
        std::vector<uint8_t> rgb(16*16*3,127);
        auto image = root/"input.png";
        require(stbi_write_png(image.string().c_str(),16,16,3,rgb.data(),48) != 0,"could not write memory fixture");
        std::vector<float> viewmat(16,0); for (int i = 0; i < 4; ++i) viewmat[5*i] = 1;
        DataManagerConfig dm; dm.cache_mode = CacheMode::CPU;
        dm.load_masks = dm.load_depths = dm.load_normals = false;
        engine_setup_data_manager(dm,{0},{0},{image.string()},{},{},{},{16},{16},{},{},
            viewmat,{20,20,8,8},std::vector<float>(8,0),{},{},{},{},{},{},{},{0},{});
        require_no_projective_memory();
        require(engine().projective_centers_host.size() == 1,"missing lazy camera source");
        int32_t camera = 0;
        _set_cur_cam_indices({(uint64_t)&camera,4,{1,1}});
        OptimConfig cfg; cfg.lr_means = .001f; cfg.lr_projective_depth = 0;
        const size_t before = DevicePool::global().totalAllocBytes();
        engine_prepare_projective_depth(cfg); require_no_projective_memory();
        require(DevicePool::global().totalAllocBytes() == before,"disabled depth increased pool capacity");
        cfg.lr_projective_depth = .01f; engine_prepare_projective_depth(cfg);
        require(engine().optim.observe_projective.states && engine().projective_centers.data_ptr(),
                "enabling projective depth did not allocate its lazy buffers");
        cfg.lr_projective_depth = 0; engine_prepare_projective_depth(cfg);
        require_no_projective_memory();
        require(DevicePool::global().totalAllocBytes() == before,"disabling depth did not release VRAM capacity");
        cfg.lr_projective_depth = .01f; engine_prepare_projective_depth(cfg);
        float4 fresh;
        backend::memcpy_sync(&fresh,engine().projective_state.data_ptr(),sizeof(fresh),backend::MemcpyKind::DeviceToHost);
        require(fresh.x == 0 && fresh.y == 0 && fresh.z == 0 && fresh.w == 0,"re-enabling depth retained discarded moments");
        float4 expected{1,.25f,.5f,17}; float information = 7;
        backend::memcpy_sync(engine().projective_state.data_ptr(),&expected,sizeof(expected),backend::MemcpyKind::HostToDevice);
        backend::memcpy_sync(engine().projective_information.data_ptr(),&information,4,backend::MemcpyKind::HostToDevice);
        engine_save_checkpoint((root/"checkpoint").string(),true,123);
        require(engine_load_checkpoint((root/"checkpoint").string(),false) == 123,"disabled resume lost its step");
        require_no_projective_memory();
        require(engine_load_checkpoint((root/"checkpoint").string(),true) == 123,"enabled resume lost its step");
        engine_prepare_projective_depth(cfg);
        float4 actual; float actual_information;
        backend::memcpy_sync(&actual,engine().projective_state.data_ptr(),sizeof(actual),backend::MemcpyKind::DeviceToHost);
        backend::memcpy_sync(&actual_information,engine().projective_information.data_ptr(),4,backend::MemcpyKind::DeviceToHost);
        require(actual.x == expected.x && actual.y == expected.y && actual.z == expected.z
            && actual.w == expected.w && actual_information == information,"lazy camera upload erased restored moments");
        cfg.lr_projective_depth = 0; engine_prepare_projective_depth(cfg); require_no_projective_memory();
        engine_reset(); fs::remove_all(root);
    } catch (const std::exception& e) {
        std::fprintf(stderr,"%s\n",e.what()); engine_reset(); fs::remove_all(root); return 1;
    }
    std::puts("projective_memory passed");
}
