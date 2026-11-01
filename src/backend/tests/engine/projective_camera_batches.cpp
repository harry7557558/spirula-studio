#include "engine/Engine.h"
#include "engine/EngineInternal.h"
#include "engine/EngineState.h"

#include <cstdio>
#include <stdexcept>
#include <vector>

namespace {
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
void seed() {
    std::vector<float> means{0,0,3,1,0,4}, quats{1,0,0,0,1,0,0,0};
    std::vector<float> scales(6,-2), opacity(2,0), color(6,.5f), sh(18,0);
    auto tensor = [](std::vector<float>& data, std::vector<int64_t> shape) {
        return TorchTensorView{(uint64_t)data.data(),4,std::move(shape)};
    };
    set_data_3dgs(2,tensor(means,{2,3}),tensor(quats,{2,4}),tensor(scales,{2,3}),
        tensor(opacity,{2,1}),tensor(color,{2,3}),tensor(sh,{2,3,3}),0);
    backend::device_synchronize();
}
}

int main() {
    try {
        engine_reset(); seed();
        auto& s = engine();
        s.projective_centers_host = {{0,0,0,1},{0,0,0,1},{1,0,0,1}};
        s.projective_centers.resize(PoolSlot::EngProjectiveCenters,3);
        backend::memcpy_sync(s.projective_centers.data_ptr(),s.projective_centers_host.data(),
            3*sizeof(float4),backend::MemcpyKind::HostToDevice);
        s.optim.zero_grad_in_optim = false;
        OptimConfig cfg; cfg.lr_projective_depth = .01f; cfg.lr_means = .001f;
        auto select = [&](std::vector<int32_t> indices) {
            _set_cur_cam_indices({(uint64_t)indices.data(),4,{(int64_t)indices.size(),1}});
            engine_prepare_projective_depth(cfg);
            return s.optim.observe_projective;
        };
        auto depth = select({0});
        require(depth.single_photometric && depth.single_observation,
                "single-view gradient and observer shortcuts were lost");
        depth = select({0,1});
        require(depth.single_photometric && !depth.single_observation,
                "coincident views need the stable derivative and atomic observations");
        TorchTensorView device{(uint64_t)s.bilagrid_cur_cam_indices.data_ptr(),4,{2,1}};
        _set_cur_cam_indices(device); engine_prepare_projective_depth(cfg);
        require(s.optim.observe_projective.single_photometric,
                "device camera indices lost their coincident-center metadata");
        depth = select({0,2});
        require(!depth.single_photometric && !depth.single_observation,
                "different camera centers incorrectly used the single-center derivative");
        for (bool coincident : {true,false}) {
            s.optim.skip_grad_zero = false; s.optim.zero_grad_in_optim = false;
            select({0});
            s.optim.skip_grad_zero = true;
            select({coincident ? 1 : 2});
            s.optim.zero_grad_in_optim = true;
            engine_prepare_projective_depth(cfg);
            require(s.optim.observe_projective.single_photometric == coincident,
                    "accumulated face passes lost their common-center invariant");
        }
        s.optim.zero_grad_in_optim = false; s.optim.skip_grad_zero = false;
        cfg.projective_photometric = false; select({0});
        s.optim.skip_grad_zero = true; cfg.projective_photometric = true; select({1});
        s.optim.zero_grad_in_optim = true; engine_prepare_projective_depth(cfg);
        require(!s.optim.observe_projective.single_photometric,
                "accumulated geometry supervision used a photometric identity");
        s.optim.zero_grad_in_optim = false; s.optim.skip_grad_zero = false;
        cfg.projective_photometric = false; depth = select({0,1});
        require(!depth.single_photometric,"geometry supervision used a photometric identity");
        int32_t unused = 0;
        _set_cur_cam_indices({(uint64_t)&unused,4,{0,1}}); engine_prepare_projective_depth(cfg);
        require(!s.optim.observe_projective.states,"zero-sized camera metadata retained an active descriptor");
        _set_cur_cam_indices({0,0,{}}); engine_prepare_projective_depth(cfg);
        require(!s.optim.observe_projective.states && !s.optim.projective_one_center,
                "empty camera metadata retained a projective descriptor");
        engine_reset();
        require(engine().projective_centers_host.empty(),"engine reset retained camera-center metadata");
    } catch (const std::exception& e) { std::fprintf(stderr,"%s\n",e.what()); return 1; }
    std::puts("projective_camera_batches passed");
}
