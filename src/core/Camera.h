#pragma once

// Cameras -- device-resident table of N cameras.
//
// Storage is non-owning throughout: the device members reference memory owned
// by DevicePool, or by an external owner when shallow-copied from existing
// device tensors. Copy and move are therefore shallow.
//
// Two constraints are not visible from the declarations:
//
//   - The model AND the distortion tier are ONE enum each for the whole batch,
//     not per camera, because that is how the projection / rasterization
//     kernels dispatch. Mixed cameras have to be split into separate tables.
//
//   - Width / height live host-side. Only host code reads them (launch
//     dimensions, image-size bookkeeping), so keeping them off device avoids
//     an H->D->H round trip per query. A uniform-size batch is N copies of
//     the same value.

#include "core/Common.cuh"
#include "core/Tensor.h"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>


// COLMAP / NerfStudio camera-model string -> CameraModelType.
// Returns CameraModelType(-1) for unknown / unsupported models; callers
// validate and raise.
inline CameraModelType camera_model_from_name(const std::string& name) {
    if (name == "PINHOLE" ||
        name == "SIMPLE_PINHOLE" ||
        name == "SIMPLE_RADIAL" ||
        name == "RADIAL" ||
        name == "OPENCV")             return CameraModelType::PINHOLE;
    if (name == "FISHEYE" ||
        name == "SIMPLE_FISHEYE" ||
        name == "SIMPLE_RADIAL_FISHEYE" ||
        name == "RADIAL_FISHEYE" ||
        name == "OPENCV_FISHEYE" ||
        name == "THIN_PRISM_FISHEYE") return CameraModelType::FISHEYE;
    if (name == "ORTHOGRAPHIC") return CameraModelType::ORTHOGRAPHIC;
    if (name == "EQUISOLID")          return CameraModelType::EQUISOLID;
    if (name == "EQUIRECTANGULAR")    return CameraModelType::EQUIRECTANGULAR;
    return (CameraModelType)-1;
}

inline const char* camera_distortion_to_string(CameraDistortionType d) {
    switch (d) {
        case CameraDistortionType::None:      return "NONE";
        case CameraDistortionType::OpenCV:    return "OPENCV";
        case CameraDistortionType::ThinPrism: return "THIN_PRISM";
        default:                              return "UNKNOWN";
    }
}

inline const char* camera_model_to_string(CameraModelType m) {
    switch (m) {
        case CameraModelType::ORTHOGRAPHIC:    return "ORTHOGRAPHIC";
        case CameraModelType::PINHOLE:         return "PINHOLE";
        case CameraModelType::FISHEYE:         return "FISHEYE";
        case CameraModelType::EQUISOLID:       return "EQUISOLID";
        case CameraModelType::EQUIRECTANGULAR: return "EQUIRECTANGULAR";
        default:                               return "UNKNOWN";
    }
}


class Cameras {
public:

    // ---- Construction ------------------------------------------------------

    Cameras() = default;

    // Uniform-size upload. The views may be host or device; width / height
    // are broadcast.
    Cameras(const std::string& key_prefix,
            int64_t num,
            int32_t width,
            int32_t height,
            CameraModelType model,
            CameraDistortionType distortion,
            const TorchTensorView& viewmats,     // host or device, [N, 4, 4]
            const TorchTensorView& intrins,      // host or device, [N, 4]
            const TorchTensorView& dist_coeffs)  // host or device, [N, 8]
        : _num(num), _model(model), _distortion(distortion)
    {
        _validate_shape("viewmats",   viewmats,    {num, 4, 4});
        _validate_shape("intrins",    intrins,     {num, 4});
        _validate_shape("dist_coeffs", dist_coeffs, {num, kCameraDistortionParams});

        (void)key_prefix;  // camera table always uses the cam.* pool slots
        _viewmats    = _upload_viewmats(PoolSlot::CamViewmats, viewmats);
        _intrins     = _upload_dv<float4>(PoolSlot::CamIntrins, intrins);
        _dist_coeffs = _upload_dt2d<float>(PoolSlot::CamDistCoeffs, dist_coeffs);

        _widths.assign((size_t)num, width);
        _heights.assign((size_t)num, height);
    }

    // Per-camera upload — sizes are passed host-side.
    Cameras(const std::string& key_prefix,
            int64_t num,
            CameraModelType model,
            CameraDistortionType distortion,
            const TorchTensorView& viewmats,
            const TorchTensorView& intrins,
            const TorchTensorView& dist_coeffs,
            std::vector<int32_t> widths,
            std::vector<int32_t> heights)
        : _num(num), _model(model), _distortion(distortion),
          _widths(std::move(widths)), _heights(std::move(heights))
    {
        _validate_shape("viewmats",   viewmats,    {num, 4, 4});
        _validate_shape("intrins",    intrins,     {num, 4});
        _validate_shape("dist_coeffs", dist_coeffs, {num, kCameraDistortionParams});
        if ((int64_t)_widths.size()  != num)
            throw std::runtime_error("Cameras: widths size mismatch");
        if ((int64_t)_heights.size() != num)
            throw std::runtime_error("Cameras: heights size mismatch");

        (void)key_prefix;  // camera table always uses the cam.* pool slots
        _viewmats    = _upload_viewmats(PoolSlot::CamViewmats, viewmats);
        _intrins     = _upload_dv<float4>(PoolSlot::CamIntrins, intrins);
        _dist_coeffs = _upload_dt2d<float>(PoolSlot::CamDistCoeffs, dist_coeffs);
    }

    // View over existing device tensors: no allocation, and their lifetime
    // is the caller's responsibility.
    Cameras(int64_t num,
            CameraModelType model,
            CameraDistortionType distortion,
            DeviceTensor2D<float4> viewmats,
            DeviceVector<float4>   intrins,
            DeviceTensor2D<float>  dist_coeffs,
            std::vector<int32_t>   widths,
            std::vector<int32_t>   heights)
        : _num(num), _model(model), _distortion(distortion),
          _viewmats(viewmats), _intrins(intrins), _dist_coeffs(dist_coeffs),
          _widths(std::move(widths)), _heights(std::move(heights))
    {
        if ((int64_t)_widths.size()  != num)
            throw std::runtime_error("Cameras: widths size mismatch");
        if ((int64_t)_heights.size() != num)
            throw std::runtime_error("Cameras: heights size mismatch");
    }

    // Default copy / move: shallow, since member views are non-owning.
    Cameras(const Cameras&)            = default;
    Cameras(Cameras&&)                 = default;
    Cameras& operator=(const Cameras&) = default;
    Cameras& operator=(Cameras&&)      = default;

    // ---- Read-only accessors ----------------------------------------------

    int64_t              num()        const { return _num; }
    CameraModelType      model()      const { return _model; }
    CameraDistortionType distortion() const { return _distortion; }
    bool                 empty()      const { return _num == 0; }

    const DeviceTensor2D<float4>& viewmats()    const { return _viewmats; }
    const DeviceVector<float4>&   intrins()     const { return _intrins; }
    const DeviceTensor2D<float>&  dist_coeffs() const { return _dist_coeffs; }
    const std::vector<int32_t>&   widths()      const { return _widths; }
    const std::vector<int32_t>&   heights()     const { return _heights; }

    // ---- Static factories / mapping --------------------------------------

    static CameraModelType model_from_colmap_name(const std::string& name) {
        return camera_model_from_name(name);
    }
    static const char* model_to_string(CameraModelType m) {
        return camera_model_to_string(m);
    }

private:

    int64_t              _num        = 0;
    CameraModelType      _model      = (CameraModelType)-1;
    CameraDistortionType _distortion = CameraDistortionType::None;

    DeviceTensor2D<float4> _viewmats;     // [N, 4]    -- each row is a float4
    DeviceVector<float4>   _intrins;      // [N]       -- (fx, fy, cx, cy)
    DeviceTensor2D<float>  _dist_coeffs;  // [N, 8]
    std::vector<int32_t>   _widths;       // size N, host-side
    std::vector<int32_t>   _heights;      // size N, host-side

    // ---- Helpers ----------------------------------------------------------

    // Source pointer location: device vs. pageable-host.
    static bool _is_device_ptr(const void* ptr) {
        return backend::is_device_pointer(ptr);
    }

    // Zero-copy view if `tv` is already on device; otherwise pool-acquire
    // and H2D-copy. Mirrors EngineCommon::_hv_to_dv but is local to Cameras
    // so this header has no Engine* dependency.
    template<typename T>
    static DeviceVector<T> _upload_dv(
        PoolSlot key, const TorchTensorView& tv)
    {
        if (std::get<0>(tv) == 0) return DeviceVector<T>();
        int64_t n = std::get<2>(tv)[0];
        uint64_t src_ptr = std::get<0>(tv);
        T* ptr;
        if (_is_device_ptr((void*)src_ptr)) {
            ptr = (T*)src_ptr;
        } else {
            ptr = DevicePool::global().acquire<T>(key, (size_t)n);
            backend::memcpy_sync(ptr, (void*)src_ptr, n * sizeof(T),
                       backend::MemcpyKind::HostToDevice);
        }
        constexpr uint32_t elem = (sizeof(T) % 4 == 0) ? 4 : 1;
        constexpr int64_t  dim1 = (sizeof(T) % 4 == 0) ? (int64_t)sizeof(T)/4
                                                       : (int64_t)sizeof(T);
        TorchTensorView dv_tv((uint64_t)ptr, elem, {n, dim1});
        return DeviceVector<T>(dv_tv);
    }

    template<typename T>
    static DeviceTensor2D<T> _upload_dt2d(
        PoolSlot key, const TorchTensorView& tv)
    {
        if (std::get<0>(tv) == 0) return DeviceTensor2D<T>();
        const auto& shape = std::get<2>(tv);
        int64_t n0 = shape[0], n1 = shape[1];
        uint64_t src_ptr = std::get<0>(tv);
        T* ptr;
        if (_is_device_ptr((void*)src_ptr)) {
            ptr = (T*)src_ptr;
        } else {
            ptr = DevicePool::global().acquire<T>(key, (size_t)(n0 * n1));
            backend::memcpy_sync(ptr, (void*)src_ptr, n0 * n1 * sizeof(T),
                       backend::MemcpyKind::HostToDevice);
        }
        constexpr uint32_t elem = (sizeof(T) % 4 == 0) ? 4 : 1;
        constexpr int64_t  dim2 = (sizeof(T) % 4 == 0) ? (int64_t)sizeof(T)/4
                                                       : (int64_t)sizeof(T);
        TorchTensorView dv_tv((uint64_t)ptr, elem, {n0, n1, dim2});
        return DeviceTensor2D<T>(dv_tv);
    }

    // Viewmats arrive as a [N, 4, 4] tensor view. Treat each row of 4
    // floats as a float4 and store as DeviceTensor2D<float4>[N, 4].
    static DeviceTensor2D<float4> _upload_viewmats(
        PoolSlot key, const TorchTensorView& tv)
    {
        if (std::get<0>(tv) == 0) return DeviceTensor2D<float4>();
        const auto& shape = std::get<2>(tv);
        int64_t N = shape[0];
        uint64_t src_ptr = std::get<0>(tv);
        float4* ptr;
        if (_is_device_ptr((void*)src_ptr)) {
            ptr = (float4*)src_ptr;
        } else {
            ptr = DevicePool::global().acquire<float4>(
                key, (size_t)(N * 4));
            backend::memcpy_sync(ptr, (void*)src_ptr, N * 4 * sizeof(float4),
                       backend::MemcpyKind::HostToDevice);
        }
        TorchTensorView dv_tv((uint64_t)ptr, 4, {N, 4LL, 4LL});
        return DeviceTensor2D<float4>(dv_tv);
    }

    static void _validate_shape(const char* name,
                                const TorchTensorView& tv,
                                const std::vector<int64_t>& expected)
    {
        const auto& shape = std::get<2>(tv);
        if ((int)shape.size() < (int)expected.size())
            throw std::runtime_error(
                std::string("Cameras: ") + name + " rank mismatch");
        for (size_t i = 0; i < expected.size(); ++i) {
            if (shape[i] != expected[i])
                throw std::runtime_error(
                    std::string("Cameras: ") + name + " shape mismatch on dim "
                    + std::to_string(i));
        }
    }
};
