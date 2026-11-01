#include "kernels/optim/ProjectiveDepth.cuh"
#include "kernels/optim/Optimizer.cuh"

#include <cmath>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <cstdio>
#include <stdexcept>
#include <vector>

namespace {
template<class T> DeviceVector<T> upload(const std::vector<T>& host) {
    auto ptr = static_cast<T*>(backend::device_malloc(host.size()*sizeof(T)));
    backend::memcpy_sync(ptr,host.data(),host.size()*sizeof(T),backend::MemcpyKind::HostToDevice);
    return DeviceVector<T>(TorchTensorView{(uint64_t)ptr,sizeof(T),{(int64_t)host.size(),1}});
}
template<class T> std::vector<T> read(DeviceVector<T> device) {
    std::vector<T> host(device.size());
    backend::memcpy_sync(host.data(),device.data_ptr(),host.size()*sizeof(T),backend::MemcpyKind::DeviceToHost);
    return host;
}
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
std::vector<float> test() {
    auto means = upload(std::vector<float3>{{0,0,2},{0,0,2},{0,0,2},{0,0,2},{0,0,2}});
    std::vector<float3> shape(5,{-1,-2,-3}); shape[4] = {-2,-1,-3};
    auto scales = upload(shape);
    auto grad = upload(std::vector<float3>(5,{-.2f,0,-.1f}));
    auto gs = upload(std::vector<float3>(5,{0,0,0}));
    auto centers = upload(std::vector<float4>{{0,0,0,1},{1,0,0,1}});
    auto radii = upload(std::vector<float>{1,0,1,1,1});
    auto map = upload(std::vector<int32_t>{1});
    auto states = upload(std::vector<float4>{{1,0,0,0},{1,0,0,0},{0,0,0,0},{1,0,0,0},{1,0,0,0}});
    std::vector<float4> rotations(5,{1,0,0,0});
    rotations[4] = {std::sqrt(.5f),0,0,std::sqrt(.5f)};
    auto quats = upload(rotations);
    auto information = upload(std::vector<float>{0,0,0,1e8f,0});
    ProjectiveDepthOptim depth{states.data_ptr(),information.data_ptr(),centers.data_ptr(),map.data_ptr(),
                              .01f,.01f,true,true,true};
    auto screen = upload(std::vector<float>{1,0,1,1,1});
    projective_depth_observe_tensor(5,5,false,{},{},DeviceTensorFloatND(screen),1,0,means,scales,quats,depth);
    auto dc = upload(std::vector<float3>(5,{.5f,.5f,.5f}));
    std::vector<DeviceVector<float3>> buffers;
    for (int i = 0; i < 5; ++i) buffers.push_back(upload(std::vector<float3>(5,{0,0,0})));
    auto qgrad = upload(std::vector<float4>(5,{0,0,0,0}));
    auto q1 = upload(std::vector<float4>(5,{0,0,0,0})), q2 = upload(std::vector<float4>(5,{0,0,0,0}));
    auto opac = upload(std::vector<float>(5,0)), og = upload(std::vector<float>(5,0));
    auto o1 = upload(std::vector<float>(5,0)), o2 = upload(std::vector<float>(5,0));
    auto update = [&](int n) {
        fused_optim_3dgs_geometry(n,means,grad,buffers[0],buffers[1],
            quats,qgrad,q1,q2,scales,gs,buffers[2],buffers[3],opac,og,o1,o2,
            dc,buffers[4],radii,{},0,0,0,0,0,1000000.f,0,0,0,0,0,0,0,0,.3f,0,
            true,{},{},{},{},depth,1,{},1,false);
    };
    update(5);
    auto m = read(means); auto s = read(scales); auto state = read(states);
    require(m[0].z > 2 && m[0].x == 0 && m[0].y == 0,"log-depth step did not preserve the anchor ray");
    require(m[3].z-2 < (m[0].z-2)*.01f, "well-covered geometry did not suppress the extra depth update");
    require(std::abs(m[4].z-m[0].z)<1e-6f,"rotating a covariance changed its bearing information");
    double error = -.1+1/m[0].z;
    require(error*error < .16,"log-depth step did not reduce the other camera's reprojection error");
    double ratio = std::log(m[0].z/2);
    require(std::abs(s[0].x+1-ratio)<1e-6 && std::abs(s[0].y+2-ratio)<1e-6 && std::abs(s[0].z+3-ratio)<1e-6,
            "log-depth step changed the anchor's projected covariance");
    require(m[1].z == 2 && state[1].w == 0,"an invisible point received a projective update");
    require(m[2].z == 2 && state[2].x == 2 && state[2].w == 0,"first observation must only choose an anchor");
    int32_t anchor = 0;
    backend::memcpy_sync(map.data_ptr(),&anchor,4,backend::MemcpyKind::HostToDevice);
    update(1);
    auto same = read(means);
    require(same[0].z == m[0].z,"the anchor camera changed its own projective gauge");
    float3 physical_gradient{0,0,1};
    backend::memcpy_sync(grad.data_ptr(),&physical_gradient,sizeof(physical_gradient),backend::MemcpyKind::HostToDevice);
    depth.single_photometric = false;
    update(1);
    require(read(means)[0].z < m[0].z,"the full derivative did not optimize supervised depth");
    float3 trust_mean{0,0,2}, trust_gradient{-.2f,0,-.1f};
    float4 trust_state{1,0,0,0}; float zero_information = 0;
    int32_t trust_camera = 1;
    auto write = [](auto device, const auto& host) {
        backend::memcpy_sync(device.data_ptr(),&host,sizeof(host),backend::MemcpyKind::HostToDevice);
    };
    write(map,trust_camera); write(grad,trust_gradient);
    depth.single_photometric = true; depth.lr = 1000; depth.angular_lr = .001f;
    for (bool relative : {true,false}) {
        write(means,trust_mean); write(states,trust_state); write(information,zero_information);
        depth.angular_relative = relative;
        update(1);
        float delta = std::log(read(means)[0].z/trust_mean.z);
        float displacement = .4f*std::abs(delta);
        float limit = .001f/(relative ? 1.f : std::sqrt(5.f));
        require(displacement <= limit*1.001f && displacement >= limit*.99f,
                "the log-range step violated its angular trust bound");
    }
    depth.lr = .01f; depth.angular_lr = .01f; depth.angular_relative = true;
    std::vector<float> result;
    for (int i = 0; i < 5; ++i) {
        for (float value : {m[i].x,m[i].y,m[i].z,s[i].x,s[i].y,s[i].z,
                             state[i].x,state[i].y,state[i].z,state[i].w})
            result.push_back(value);
    }
    auto camera_map = upload(std::vector<int32_t>{0,1});
    auto camera_ids = upload(std::vector<int32_t>{1,0});
    auto gaussian_ids = upload(std::vector<int32_t>{0,1});
    auto unpacked_flags = upload(std::vector<float>{0,1,0,0,0,1,0,0,0,0});
    auto packed_flags = upload(std::vector<float>{1,1});
    std::vector<float4> anchored(5,{1,0,0,0}); anchored[1].x = 2;
    std::vector<float> zero(5,0);
    depth.camera_indices = camera_map.data_ptr();
    depth.single_observation = false;
    depth.single_photometric = false;
    auto reset = [&] {
        backend::memcpy_sync(states.data_ptr(),anchored.data(),5*sizeof(float4),backend::MemcpyKind::HostToDevice);
        backend::memcpy_sync(information.data_ptr(),zero.data(),5*sizeof(float),backend::MemcpyKind::HostToDevice);
    };
    reset();
    projective_depth_observe_tensor(10,5,false,{},{},DeviceTensorFloatND(unpacked_flags),1,0,means,scales,quats,depth);
    auto unpacked_information = read(information);
    require(unpacked_information[0] > 0 && unpacked_information[1] > 0,
            "multi-camera observations lost their camera mapping");
    reset();
    projective_depth_observe_tensor(2,5,true,camera_ids,gaussian_ids,DeviceTensorFloatND(packed_flags),1,0,means,scales,quats,depth);
    auto packed_information = read(information);
    for (int i = 0; i < 5; ++i) {
        require(std::abs(packed_information[i]-unpacked_information[i]) <= 1e-5f*std::max(1.f,unpacked_information[i]),
                "packed and unpacked bearing observations differ");
        result.push_back(packed_information[i]);
    }
    for (auto b : {camera_map,camera_ids,gaussian_ids}) backend::device_free(b.data_ptr());
    for (auto b : {unpacked_flags,packed_flags}) backend::device_free(b.data_ptr());
    for (auto b : buffers) backend::device_free(b.data_ptr());
    for (auto b : {qgrad,q1,q2}) backend::device_free(b.data_ptr());
    for (auto b : {opac,og,o1,o2,screen}) backend::device_free(b.data_ptr());
    backend::device_free(dc.data_ptr());
    for (auto b : {means,scales,grad,gs}) backend::device_free(b.data_ptr());
    for (auto b : {centers,states,quats}) backend::device_free(b.data_ptr());
    backend::device_free(radii.data_ptr()); backend::device_free(map.data_ptr()); backend::device_free(information.data_ptr());
    return result;
}
}

int main(int argc, char** argv) {
    try {
        auto result = test();
        if (argc == 3 && std::strcmp(argv[1],"dump") == 0) {
            std::ofstream file(argv[2],std::ios::binary);
            file.write(reinterpret_cast<const char*>(result.data()),result.size()*sizeof(float));
            if (!file) throw std::runtime_error("could not write projective reference");
        } else if (argc == 3 && std::strcmp(argv[1],"compare") == 0) {
            std::vector<float> reference(result.size());
            std::ifstream file(argv[2],std::ios::binary);
            file.read(reinterpret_cast<char*>(reference.data()),reference.size()*sizeof(float));
            if (!file) throw std::runtime_error("could not read projective reference");
            for (size_t i = 0; i < result.size(); ++i)
                require(std::abs(result[i]-reference[i]) <= 2e-5f*std::max(std::abs(reference[i]),1.f),
                        "projective update differs between backends");
        } else require(argc == 1,"usage: projective_depth_test [dump|compare reference]");
    }
    catch (const std::exception& e) { std::fprintf(stderr,"%s\n",e.what()); return 1; }
    std::puts("projective_depth_test passed");
}
