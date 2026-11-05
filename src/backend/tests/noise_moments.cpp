#include "kernels/densify/Densify.cuh"

#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace {
template<class T> DeviceVector<T> upload(const std::vector<T>& host) {
    auto ptr = static_cast<T*>(backend::device_malloc(host.size()*sizeof(T)));
    backend::memcpy_sync(ptr,host.data(),host.size()*sizeof(T),backend::MemcpyKind::HostToDevice);
    return DeviceVector<T>(TorchTensorView{(uint64_t)ptr,sizeof(T),{(int64_t)host.size(),1}});
}
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
std::vector<float> test(bool revised) {
    constexpr int n = 131072;
    auto means = upload(std::vector<float3>(n,{0,0,0}));
    auto scales = upload(std::vector<float3>(n,{0,0,0}));
    auto quats = upload(std::vector<float4>(n,{1,0,0,0}));
    auto opacs = upload(std::vector<float>(n,revised ? 0.f : -100.f));
    std::vector<float> visibility(n,1.f); visibility.back() = 0;
    auto radii = upload(visibility);
    auto weights = upload(std::vector<float>(n,0.f));
    auto sample = [&](uint32_t seed) {
        backend::memset_async(means.data_ptr(),0,n*sizeof(float3));
        if (revised)
            revised_add_noise_tensor(n,10.f,radii,means,scales,quats,opacs,weights,1.f,seed);
        else
            mcmc_add_noise_tensor(n,1.f+std::exp(-.5f),means,scales,quats,opacs,seed);
        std::vector<float3> host(n);
        backend::memcpy_sync(host.data(),means.data_ptr(),n*sizeof(float3),backend::MemcpyKind::DeviceToHost);
        return host;
    };
    auto first = sample(71), repeat = sample(71), second = sample(72);
    require(std::memcmp(first.data(),repeat.data(),n*sizeof(float3)) == 0,"noise seed was not reproducible");
    if (revised)
        require(first.back().x == 0 && first.back().y == 0 && first.back().z == 0,
                "an invisible Gaussian received exploration noise");
    std::array<double,3> sum{}, other{};
    std::array<double,9> products{}, temporal{};
    for (int i = 0; i < n-1; ++i) {
        std::array<float,3> a{first[i].x,first[i].y,first[i].z};
        std::array<float,3> b{second[i].x,second[i].y,second[i].z};
        for (int j = 0; j < 3; ++j) {
            require(std::isfinite(a[j]) && std::isfinite(b[j]),"nonfinite exploration sample");
            sum[j] += a[j]; other[j] += b[j];
            for (int k = 0; k < 3; ++k) {
                products[3*j+k] += double(a[j])*a[k];
                temporal[3*j+k] += double(a[j])*b[k];
            }
        }
    }
    for (int j = 0; j < 3; ++j) {
        sum[j] /= n-1; other[j] /= n-1;
        require(std::abs(sum[j]) < .02 && std::abs(other[j]) < .02,"exploration mean exceeds seven standard errors");
    }
    for (int j = 0; j < 3; ++j) for (int k = 0; k < 3; ++k) {
        double covariance = products[3*j+k]/(n-1)-sum[j]*sum[k];
        require(std::abs(covariance-(j == k ? 1. : 0.)) < .03,"exploration covariance is not identity");
        require(std::abs(temporal[3*j+k]/(n-1)-sum[j]*other[k]) < .025,
                "exploration samples are correlated across training steps");
    }
    std::vector<float> result;
    for (int i = 0; i < 256; ++i) for (float v : {first[i].x,first[i].y,first[i].z}) result.push_back(v);
    backend::device_free(means.data_ptr()); backend::device_free(scales.data_ptr());
    backend::device_free(quats.data_ptr()); backend::device_free(opacs.data_ptr());
    backend::device_free(radii.data_ptr()); backend::device_free(weights.data_ptr());
    return result;
}
}

int main(int argc, char** argv) {
    try {
        auto result = test(false), revised = test(true);
        result.insert(result.end(),revised.begin(),revised.end());
        if (argc == 3 && std::strcmp(argv[1],"dump") == 0) {
            std::ofstream file(argv[2],std::ios::binary);
            file.write(reinterpret_cast<const char*>(result.data()),result.size()*sizeof(float));
            require(bool(file),"could not write noise reference");
        } else if (argc == 3 && std::strcmp(argv[1],"compare") == 0) {
            std::vector<float> reference(result.size());
            std::ifstream file(argv[2],std::ios::binary);
            file.read(reinterpret_cast<char*>(reference.data()),reference.size()*sizeof(float));
            require(bool(file),"could not read noise reference");
            for (size_t i = 0; i < result.size(); ++i)
                require(std::abs(result[i]-reference[i]) < 2e-5f,"exploration noise differs between backends");
        } else require(argc == 1,"usage: noise_moments [dump|compare reference]");
    } catch (const std::exception& e) { std::fprintf(stderr,"%s\n",e.what()); return 1; }
    std::puts("noise_moments passed");
}
