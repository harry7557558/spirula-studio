#pragma once

// EngineCommon — small inline helpers shared across the Engine*.cpp split:
//   * Type conversion between TorchTensorView and DeviceVector/DeviceTensor.
//   * Pool acquire helpers that return a TorchTensorView.
//   * Source-pointer detection (device vs. host).
//   * The GT upload + on-device dtype conversion path.
//
// All helpers are header-only (`inline` / `inline static template`) so the
// section .cpp files can each `#include "engine/EngineCommon.h"` without ODR issues.

#include "engine/EngineInternal.h"
#include "core/Tensor.h"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <type_traits>


// --- Construct DeviceTensorFloatND from a TorchTensorView by appending a
//     trailing 1 dimension (for float-typed access). ---
inline DeviceTensorFloatND tv_to_fnd(const TorchTensorView& tv) {
    auto shape = std::get<2>(tv);
    shape.push_back(1LL);
    return DeviceTensorFloatND(TorchTensorView{std::get<0>(tv), std::get<1>(tv), shape});
}

// --- DeviceTensor2D<uint2> view over the packed AABB the packed projection
//     returns, which the backward takes as a 2D tensor. ---
inline DeviceTensor2D<uint2> vec_to_2d_aabb(const DeviceVector<uint2>& vec) {
    TorchTensorView tv{(uint64_t)vec.data_ptr(), (uint32_t)sizeof(unsigned),
                       {vec.size(), 1LL, 2LL}};
    return DeviceTensor2D<uint2>(tv);
}

// --- DeviceVector/DeviceTensor -> TorchTensorView. ---

template<typename T>
inline TorchTensorView _dv_tv(const DeviceVector<T>& dv) {
    static_assert(sizeof(T) % 4 == 0 || std::is_same<T, uint8_t>::value ||
                  std::is_same<T, bool>::value,
                  "Unsupported element type");
    if constexpr (sizeof(T) % 4 == 0) {
        return TorchTensorView((uint64_t)dv.data_ptr(), 4,
            {dv.size(), (int64_t)(sizeof(T) / 4)});
    } else {
        return TorchTensorView((uint64_t)dv.data_ptr(), 1,
            {dv.size(), (int64_t)sizeof(T)});
    }
}

template<typename T>
inline TorchTensorView _dt2d_tv(const DeviceTensor2D<T>& dt) {
    if constexpr (std::is_arithmetic_v<T>) {
        uint32_t es = (uint32_t)sizeof(T);
        return TorchTensorView((uint64_t)dt.data_ptr(), es,
            {dt.template size<0>(), dt.template size<1>()});
    } else if constexpr (sizeof(T) % 4 == 0) {
        return TorchTensorView((uint64_t)dt.data_ptr(), 4,
            {dt.template size<0>(), dt.template size<1>(),
             (int64_t)(sizeof(T) / 4)});
    } else {
        return TorchTensorView((uint64_t)dt.data_ptr(), 1,
            {dt.template size<0>(), dt.template size<1>(), (int64_t)sizeof(T)});
    }
}

template<typename T>
inline TorchTensorView _dt3d_tv(const DeviceTensor3D<T>& dt) {
    if constexpr (sizeof(T) % 4 == 0) {
        return TorchTensorView((uint64_t)dt.data_ptr(), 4,
            {dt.template size<0>(), dt.template size<1>(), dt.template size<2>(),
             (int64_t)(sizeof(T) / 4)});
    } else {
        return TorchTensorView((uint64_t)dt.data_ptr(), 1,
            {dt.template size<0>(), dt.template size<1>(), dt.template size<2>(),
             (int64_t)sizeof(T)});
    }
}

// --- Source pointer location: device vs. pageable-host. ---
inline bool _is_device_ptr(const void* ptr) {
    return backend::is_device_pointer(ptr);
}

// --- Pool-acquire + H->D copy from a host TorchTensorView, OR zero-copy view
//     of an already-device source. ---

template<typename T>
inline DeviceVector<T> _hv_to_dv(PoolSlot key, const TorchTensorView& src_tv) {
    if (std::get<0>(src_tv) == 0) return DeviceVector<T>();
    int64_t n = std::get<2>(src_tv)[0];
    uint64_t src_ptr = std::get<0>(src_tv);
    T* ptr;
    if (_is_device_ptr((void*)src_ptr)) {
        ptr = (T*)src_ptr;
    } else {
        ptr = DevicePool::global().acquire<T>(key, (size_t)n);
        backend::memcpy_sync(ptr, (void*)src_ptr, n * sizeof(T), backend::MemcpyKind::HostToDevice);
    }
    TorchTensorView dv_tv((uint64_t)ptr,
        (uint32_t)(sizeof(T) % 4 == 0 ? 4 : 1),
        {n, (int64_t)(sizeof(T) % 4 == 0 ? sizeof(T) / 4 : sizeof(T))});
    return DeviceVector<T>(dv_tv);
}

template<typename T>
inline DeviceTensor2D<T> _hv_to_dt2d(PoolSlot key, const TorchTensorView& src_tv) {
    if (std::get<0>(src_tv) == 0) return DeviceTensor2D<T>();
    auto& shape = std::get<2>(src_tv);
    int64_t n0 = shape[0], n1 = shape[1];
    uint64_t src_ptr = std::get<0>(src_tv);
    T* ptr;
    if (_is_device_ptr((void*)src_ptr)) {
        ptr = (T*)src_ptr;
    } else {
        ptr = DevicePool::global().acquire<T>(key, (size_t)(n0 * n1));
        backend::memcpy_sync(ptr, (void*)src_ptr, n0 * n1 * sizeof(T), backend::MemcpyKind::HostToDevice);
    }
    TorchTensorView tv((uint64_t)ptr,
        (uint32_t)(sizeof(T) % 4 == 0 ? 4 : 1),
        {n0, n1, (int64_t)(sizeof(T) % 4 == 0 ? sizeof(T) / 4 : sizeof(T))});
    return DeviceTensor2D<T>(tv);
}

template<typename T>
inline DeviceTensor3D<T> _hv_to_dt3d(PoolSlot key, const TorchTensorView& src_tv) {
    if (std::get<0>(src_tv) == 0) return DeviceTensor3D<T>();
    auto& shape = std::get<2>(src_tv);
    int64_t n0 = shape[0], n1 = shape[1], n2 = shape[2];
    uint64_t src_ptr = std::get<0>(src_tv);
    T* ptr;
    if (_is_device_ptr((void*)src_ptr)) {
        ptr = (T*)src_ptr;
    } else {
        ptr = DevicePool::global().acquire<T>(key, (size_t)(n0 * n1 * n2));
        backend::memcpy_sync(ptr, (void*)src_ptr, n0 * n1 * n2 * sizeof(T), backend::MemcpyKind::HostToDevice);
    }
    TorchTensorView tv((uint64_t)ptr,
        (uint32_t)(sizeof(T) % 4 == 0 ? 4 : 1),
        {n0, n1, n2, (int64_t)(sizeof(T) % 4 == 0 ? sizeof(T) / 4 : sizeof(T))});
    return DeviceTensor3D<T>(tv);
}

// --- D->H copies for the engine_copy_*_to_host APIs. ---

template<typename T>
inline void _dv_to_host(const DeviceVector<T>& dv, const TorchTensorView& host_tv) {
    if (dv.data_ptr() == nullptr || std::get<0>(host_tv) == 0) return;
    int64_t n = dv.size();
    backend::memcpy_sync((void*)std::get<0>(host_tv), dv.data_ptr(),
               n * sizeof(T), backend::MemcpyKind::DeviceToHost);
}

template<typename T>
inline void _dt3d_to_host(const DeviceTensor3D<T>& dt, const TorchTensorView& host_tv) {
    if (dt.data_ptr() == nullptr || std::get<0>(host_tv) == 0) return;
    backend::memcpy_sync((void*)std::get<0>(host_tv), dt.data_ptr(),
               dt.numel() * sizeof(T), backend::MemcpyKind::DeviceToHost);
}


// --- Ground-truth upload + GPU-side type conversion.
//
// kind picks the per-element conversion:
//   "rgb"    : uint8 -> [0, 1] (/255); uint16 -> float (/65535)
//   "normal" : uint8 -> [-1, 1] (x/127.5 - 1)
//   "depth"  : uint16 -> float (cast only)
// For all kinds, elem_size == sizeof(float) goes through the regular zero-copy
// / H2D _hv_to_dt3d<T>() path unchanged.
template<typename T>
inline DeviceTensor3D<T> _hv_to_dt3d_gt(
    const TorchTensorView& src_tv,
    PoolSlot key,
    const std::string& kind)
{
    if (std::get<0>(src_tv) == 0) return DeviceTensor3D<T>();
    uint32_t elem_size = std::get<1>(src_tv);
    if (elem_size == 4) return _hv_to_dt3d<T>(key, src_tv);

    const std::string key_name = slot_name(key);
    auto& shape = std::get<2>(src_tv);
    if (shape.size() != 4)
        throw std::runtime_error(key_name + ": expected [B, H, W, C] shape");
    int64_t B = shape[0], H = shape[1], W = shape[2], C = shape[3];
    int64_t numel = B * H * W * C;
    uint64_t src_ptr = std::get<0>(src_tv);
    bool src_is_device = _is_device_ptr((void*)src_ptr);

    // Float output buffer (consumed downstream by loss / bilagrid kernels).
    float* d_f = DevicePool::global().acquire<float>(key, (size_t)numel);

    // Shared H2D staging slot, reused across gt_rgb / gt_normal / gt_depth.
    if (elem_size == 1) {
        const uint8_t* d_u8;
        if (src_is_device) {
            d_u8 = (const uint8_t*)src_ptr;
        } else {
            uint8_t* staging = DevicePool::global().acquire<uint8_t>(
                PoolSlot::GtStagingU8, (size_t)numel);
            backend::memcpy_sync(staging, (void*)src_ptr, numel * sizeof(uint8_t),
                       backend::MemcpyKind::HostToDevice);
            d_u8 = staging;
        }
        if (kind == "rgb") {
            uint8_image_to_float_raw(d_u8, d_f, (int)B, (int)H, (int)W, (int)C);
        } else if (kind == "normal") {
            uint8_normal_to_float_raw(d_u8, d_f, (int)B, (int)H, (int)W, (int)C);
        } else {
            throw std::runtime_error(key_name + ": uint8 not supported for kind '" + kind + "'");
        }
    } else if (elem_size == 2) {
        const uint16_t* d_u16;
        if (src_is_device) {
            d_u16 = (const uint16_t*)src_ptr;
        } else {
            uint16_t* staging = DevicePool::global().acquire<uint16_t>(
                PoolSlot::GtStagingU16, (size_t)numel);
            backend::memcpy_sync(staging, (void*)src_ptr, numel * sizeof(uint16_t),
                       backend::MemcpyKind::HostToDevice);
            d_u16 = staging;
        }
        if (kind == "rgb") {
            uint16_image_to_float_raw(d_u16, d_f, (int)B, (int)H, (int)W, (int)C);
        } else if (kind == "depth") {
            uint16_depth_to_float_raw(d_u16, d_f, (int)B, (int)H, (int)W, (int)C);
        } else {
            throw std::runtime_error(key_name + ": uint16 not supported for kind '" + kind + "'");
        }
    } else {
        throw std::runtime_error(key_name + ": unsupported element_size (expected 1, 2, or 4)");
    }

    TorchTensorView tv((uint64_t)d_f, 4, {B, H, W, C});
    return DeviceTensor3D<T>(tv);
}


// --- Pool-backed TorchTensorView helpers (consumed by EngineLoss.cpp). ---

inline TorchTensorView _tv_null() {
    return TorchTensorView(0, 4, {0});
}

inline bool _tv_valid(const TorchTensorView& tv) {
    return std::get<0>(tv) != 0;
}

inline TorchTensorView _pool_tv(PoolSlot key,
                                int64_t B, int64_t H, int64_t W, int64_t C) {
    float* p = DevicePool::global().acquire<float>(key, (size_t)(B * H * W * C));
    return TorchTensorView((uint64_t)p, 4, {B, H, W, C});
}

inline TorchTensorView _pool_tv_zero(PoolSlot key,
                                     int64_t B, int64_t H, int64_t W, int64_t C) {
    float* p = DevicePool::global().acquire<float>(key, (size_t)(B * H * W * C));
    backend::memset_sync(p, 0, B * H * W * C * sizeof(float));
    return TorchTensorView((uint64_t)p, 4, {B, H, W, C});
}

inline TorchTensorView _pool_tv_1d(PoolSlot key, int64_t N) {
    float* p = DevicePool::global().acquire<float>(key, (size_t)N);
    return TorchTensorView((uint64_t)p, 4, {N});
}

inline TorchTensorView _pool_tv_1d_zero(PoolSlot key, int64_t N) {
    float* p = DevicePool::global().acquire<float>(key, (size_t)N);
    backend::memset_sync(p, 0, N * sizeof(float));
    return TorchTensorView((uint64_t)p, 4, {N});
}

inline TorchTensorView _pool_tv_2d(PoolSlot key, int64_t N, int64_t C) {
    float* p = DevicePool::global().acquire<float>(key, (size_t)(N * C));
    return TorchTensorView((uint64_t)p, 4, {N, C});
}

inline TorchTensorView _pool_tv_3d_f(PoolSlot key,
                                     int64_t N, int64_t K, int64_t C) {
    float* p = DevicePool::global().acquire<float>(key, (size_t)(N * K * C));
    return TorchTensorView((uint64_t)p, 4, {N, K, C});
}

inline TorchTensorView _pool_tv_3d_u8(PoolSlot key,
                                      int64_t N, int64_t K, int64_t C) {
    uint8_t* p = DevicePool::global().acquire<uint8_t>(key, (size_t)(N * K * C));
    return TorchTensorView((uint64_t)p, 1, {N, K, C});
}

inline void _zero_tv(const TorchTensorView& tv) {
    if (std::get<0>(tv) == 0) return;
    int64_t bytes = std::get<1>(tv);
    for (auto s : std::get<2>(tv)) bytes *= s;
    backend::memset_sync((void*)std::get<0>(tv), 0, bytes);
}
