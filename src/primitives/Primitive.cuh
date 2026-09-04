#pragma once

#include "core/Common.cuh"

#include <array>
#include <tuple>
#include <optional>
#include <vector>

#include <core/Tensor.h>


enum class RenderOutputType {
    RGB, RGB_D, RGB_DN
};

// Channel set for the distortion regularizer image D = W*S - C^2. Depth is
// always present when distortion is active (it is the primary 2DGS term); rgb
// and normal are independently optional. `None` disables distortion. DN /
// RGB_DN require a normal-rendering primitive: they are defined and handled in
// the kernels but not instantiated for RGB_D primitives (placeholders until a
// normal-rendering primitive exists). Channels must be a subset of the
// primitive's RenderOutputType.
enum class DistortionType {
    None, D, RGB_D, DN, RGB_DN
};
constexpr bool dist_any(DistortionType t)    { return t != DistortionType::None; }
constexpr bool dist_has_rgb(DistortionType t)
    { return t == DistortionType::RGB_D || t == DistortionType::RGB_DN; }
constexpr bool dist_has_depth(DistortionType t)  { return t != DistortionType::None; }
constexpr bool dist_has_normal(DistortionType t)
    { return t == DistortionType::DN || t == DistortionType::RGB_DN; }

// How the error map reduces to one score per splat (DensifyConfig::accum_mode).
// Avg's buffer is 2N planar -- numerator then denominator -- divided in place
// once every camera has landed.
enum class DensifyAccumMode : int {
    None = 0, Max = 1, Sum = 2, Avg = 3
};
constexpr bool accum_any(DensifyAccumMode m) { return m != DensifyAccumMode::None; }
constexpr int accum_lanes(DensifyAccumMode m)
    { return m == DensifyAccumMode::Avg ? 2 : 1; }

// Depth distortion operates on log depth (ln z). This floor keeps the log/exp
// and the 1/z gradient chain finite for non-positive depths (the eval3d path
// already guarantees z > 0; the 2D path does not). Must match in fwd + bwd.
constexpr float DEPTH_DIST_EPS = 1e-6f;

class RenderOutput {
    static constexpr float _default_depth = 0.0f;
    static constexpr float3 _default_normal = {0.0f, 0.0f, 0.0f};

public:
    static constexpr bool has_depth(RenderOutputType type)
        { return type == RenderOutputType::RGB_D || type == RenderOutputType::RGB_DN; }
    static constexpr bool has_normal(RenderOutputType type)
        { return type == RenderOutputType::RGB_DN; }

    float3 rgb;
    float depth;
    float3 normal;

    struct Buffer;

    typedef std::tuple<
        DeviceTensor3D<float3>,
        DeviceTensor3D<float>,
        DeviceTensor3D<float3>
    > TensorTuple;

    template<RenderOutputType type>
    static void resize(
        TensorTuple& tensors,
        int64_t batch, int64_t height, int64_t width,
        PoolSlot key
    ) {
        const VramCategory cat = slot_category(key);
        const std::string  base = slot_name(key);
        std::get<0>(tensors).resize_dynamic(cat, base + ".rgb", batch, height, width);
        if (has_depth(type))
            std::get<1>(tensors).resize_dynamic(cat, base + ".depth", batch, height, width);
        if (has_normal(type))
            std::get<2>(tensors).resize_dynamic(cat, base + ".normal", batch, height, width);
    }

    // Allocate only the distortion channels in `dt` (depth always when active;
    // rgb/normal optional). Tensors for absent channels stay empty.
    template<DistortionType dt>
    static void resizeDistortion(
        TensorTuple& tensors,
        int64_t batch, int64_t height, int64_t width,
        PoolSlot key
    ) {
        const VramCategory cat = slot_category(key);
        const std::string  base = slot_name(key);
        if (dist_has_rgb(dt))
            std::get<0>(tensors).resize_dynamic(cat, base + ".rgb", batch, height, width);
        if (dist_has_depth(dt))
            std::get<1>(tensors).resize_dynamic(cat, base + ".depth", batch, height, width);
        if (dist_has_normal(dt))
            std::get<2>(tensors).resize_dynamic(cat, base + ".normal", batch, height, width);
    }

    struct Tensor {
        DeviceTensor3D<float3> rgbs;
        DeviceTensor3D<float> depths;
        DeviceTensor3D<float3> normals;

        Tensor() {}

        Tensor(const TensorTuple& images) {
            rgbs = std::get<0>(images);
            depths = std::get<1>(images);
            normals = std::get<2>(images);
        }

        TensorTuple tuple() const {
            return std::make_tuple(rgbs, depths, normals);
        }

        // True when ANY channel is allocated. Distortion tensors may carry
        // only a subset of channels (e.g. depth-only distortion has rgbs ==
        // nullptr but depths valid); keying solely on rgbs would wrongly drop
        // the whole buffer and null-deref the present channel in the kernel.
        // The render buffer always has rgb, so its behavior is unchanged.
        bool has_value() const {
            return rgbs.data_ptr() != nullptr ||
                   depths.data_ptr() != nullptr ||
                   normals.data_ptr() != nullptr;
        }

        long width() const {
            return rgbs.size<2>();
        }
        long height() const {
            return rgbs.size<1>();
        }
        long batchSize() const {
            return rgbs.size<0>();
        }

        Buffer buffer() { return Buffer(*this); }
    };

    struct Buffer {
        float3* __restrict__ rgbs;
        float* __restrict__ depths;
        float3* __restrict__ normals;

        Buffer() : rgbs(nullptr), depths(nullptr), normals(nullptr) {}

        Buffer(const Tensor& tensors) {
            rgbs = tensors.rgbs.data_ptr();
            depths = tensors.depths.data_ptr();
            normals = tensors.normals.data_ptr();
        }

#ifdef __CUDACC__

    template<RenderOutputType type>
    __device__ RenderOutput load(long idx) const {
        return {
            rgbs[idx],
            has_depth(type) ? depths[idx] : _default_depth,
            has_normal(type) ? normals[idx] : _default_normal,
        };
    }

    // Load only the distortion channels in `dt`; absent channels read as 0 so
    // they contribute nothing to the (channel-masked) gradient.
    template<DistortionType dt>
    __device__ RenderOutput loadDistortion(long idx) const {
        return {
            dist_has_rgb(dt) ? rgbs[idx] : float3{0.0f, 0.0f, 0.0f},
            dist_has_depth(dt) ? depths[idx] : _default_depth,
            dist_has_normal(dt) ? normals[idx] : _default_normal,
        };
    }

#endif

    };

#ifdef __CUDACC__

    static __device__ __forceinline__ RenderOutput zero() {
        return {
            {0.f, 0.f, 0.f},
            0.f,
            {0.f, 0.f, 0.f},
        };
    }

    __device__ __forceinline__ void operator+=(const RenderOutput &other) {
        rgb += other.rgb;
        depth += other.depth;
        normal += other.normal;
    }

    __device__ __forceinline__ RenderOutput operator*(float k) const {
        return {
            rgb * k,
            depth * k,
            normal * k,
        };
    }

    __device__ __forceinline__ RenderOutput operator+(const RenderOutput &other) const {
        return {
            rgb + other.rgb,
            depth + other.depth,
            normal + other.normal,
        };
    }

    __device__ __forceinline__ RenderOutput operator*(const RenderOutput &other) const {
        return {
            rgb * other.rgb,
            depth * other.depth,
            normal * other.normal,
        };
    }

    __device__ __forceinline__ float dot(const RenderOutput &other) const {
        float val = 0.0f;
        val += rgb.x * other.rgb.x + rgb.y * other.rgb.y + rgb.z * other.rgb.z;
        val += depth * other.depth;
        val += (normal.x * other.normal.x + normal.y * other.normal.y + normal.z * other.normal.z);
        return val;
    }

    template<RenderOutputType type>
    __device__ void saveParamsToBuffer(Buffer &buffer, long idx) {
        buffer.rgbs[idx] = rgb;
        if (has_depth(type)) buffer.depths[idx] = depth;
        if (has_normal(type)) buffer.normals[idx] = normal;
    }

    // Save only the distortion channels in `dt`. Channels absent from `dt` are
    // not written (their buffer pointer may be null / unallocated), and the
    // values feeding them are dead-code-eliminated upstream.
    template<DistortionType dt>
    __device__ void saveDistortion(Buffer &buffer, long idx) {
        if (dist_has_rgb(dt)) buffer.rgbs[idx] = rgb;
        if (dist_has_depth(dt)) buffer.depths[idx] = depth;
        if (dist_has_normal(dt)) buffer.normals[idx] = normal;
    }

#endif  // #ifdef __CUDACC__

};


// Compile-time-sized storage form of RenderOutput, holding only the channels
// present in `type` (rgb always; depth/normal per has_depth/has_normal). Use it
// for __shared__ arrays / buffers so an RGB_D primitive doesn't reserve the
// unused normal float3 (saves shared memory -> better occupancy). Compute stays
// on the full RenderOutput; conversions are implicit (operator= from a
// RenderOutput, implicit cast back), so call sites need no .load()/.store().
template<RenderOutputType type> struct RenderOutputStore;

template<> struct RenderOutputStore<RenderOutputType::RGB> {
    float3 rgb;
#ifdef __CUDACC__
    __device__ __forceinline__ RenderOutputStore& operator=(const RenderOutput& o)
        { rgb = o.rgb; return *this; }
    __device__ __forceinline__ operator RenderOutput() const
        { return { rgb, 0.0f, {0.0f, 0.0f, 0.0f} }; }
#endif
};

template<> struct RenderOutputStore<RenderOutputType::RGB_D> {
    float3 rgb;
    float depth;
#ifdef __CUDACC__
    __device__ __forceinline__ RenderOutputStore& operator=(const RenderOutput& o)
        { rgb = o.rgb; depth = o.depth; return *this; }
    __device__ __forceinline__ operator RenderOutput() const
        { return { rgb, depth, {0.0f, 0.0f, 0.0f} }; }
#endif
};

template<> struct RenderOutputStore<RenderOutputType::RGB_DN> {
    float3 rgb;
    float depth;
    float3 normal;
#ifdef __CUDACC__
    __device__ __forceinline__ RenderOutputStore& operator=(const RenderOutput& o)
        { rgb = o.rgb; depth = o.depth; normal = o.normal; return *this; }
    __device__ __forceinline__ operator RenderOutput() const
        { return { rgb, depth, normal }; }
#endif
};


// Compile-time-sized storage for the distortion-path shared state (C, S, v_dist
// in the raster backward). Holds only the channels in `dt` (depth always when
// active; rgb/normal optional) — strictly the dist channels, NOT the
// primitive's pixelType — so e.g. depth-only distortion (rgb weight 0) stores
// just a float per pixel instead of a float3+float. Absent channels read as 0
// in the implicit RenderOutput conversion, so they contribute nothing to the
// channel-masked gradient.
template<DistortionType dt> struct DistortionStore;

template<> struct DistortionStore<DistortionType::None> {
#ifdef __CUDACC__
    // Never accessed (distortion blocks are if constexpr (dist_any) -> elided);
    // present so the sized-1 shared array is a complete, trivially-constructed type.
    __device__ __forceinline__ DistortionStore& operator=(const RenderOutput&) { return *this; }
    __device__ __forceinline__ operator RenderOutput() const { return RenderOutput::zero(); }
#endif
};

template<> struct DistortionStore<DistortionType::D> {
    float depth;
#ifdef __CUDACC__
    __device__ __forceinline__ DistortionStore& operator=(const RenderOutput& o)
        { depth = o.depth; return *this; }
    __device__ __forceinline__ operator RenderOutput() const
        { return { {0.0f, 0.0f, 0.0f}, depth, {0.0f, 0.0f, 0.0f} }; }
#endif
};

template<> struct DistortionStore<DistortionType::RGB_D> {
    float3 rgb;
    float depth;
#ifdef __CUDACC__
    __device__ __forceinline__ DistortionStore& operator=(const RenderOutput& o)
        { rgb = o.rgb; depth = o.depth; return *this; }
    __device__ __forceinline__ operator RenderOutput() const
        { return { rgb, depth, {0.0f, 0.0f, 0.0f} }; }
#endif
};

template<> struct DistortionStore<DistortionType::DN> {
    float depth;
    float3 normal;
#ifdef __CUDACC__
    __device__ __forceinline__ DistortionStore& operator=(const RenderOutput& o)
        { depth = o.depth; normal = o.normal; return *this; }
    __device__ __forceinline__ operator RenderOutput() const
        { return { {0.0f, 0.0f, 0.0f}, depth, normal }; }
#endif
};

template<> struct DistortionStore<DistortionType::RGB_DN> {
    float3 rgb;
    float depth;
    float3 normal;
#ifdef __CUDACC__
    __device__ __forceinline__ DistortionStore& operator=(const RenderOutput& o)
        { rgb = o.rgb; depth = o.depth; normal = o.normal; return *this; }
    __device__ __forceinline__ operator RenderOutput() const
        { return { rgb, depth, normal }; }
#endif
};



#ifdef __CUDACC__

template<CameraDistortionType D>
struct ProjCameraT {
    float3x3 R;
    float3 t;
    float fx, fy, cx, cy;
    uint width, height;
    CameraDistortionCoeffsT<D> dist_coeffs;
};

#endif



// One row of STRIDE floats per splat instead of one allocation per component.
// Splats are gathered by index, so the sector a scattered load pulls in for one
// component carries its neighbours: the tile cull reads one instead of three.

// STRIDE * sizeof(float) must be a multiple of the widest component's
// alignment, or odd rows land misaligned for float2/float4 accessors.
template<int STRIDE>
class PackedTensorArray {
protected:
    int64_t _size;
    float* _data;

    static_assert(STRIDE >= 1);

public:
    PackedTensorArray() : _size(0), _data(nullptr) {}
    PackedTensorArray(const PackedTensorArray& other) = default;
    PackedTensorArray& operator=(const PackedTensorArray& other) = default;

    int64_t size() const { return _size; }

    // Device-callable: Screen::store checks it before writing a row.
#ifdef __CUDACC__
    __host__ __device__
#endif
    bool allocated() const { return _data != nullptr; }

    // Raw access for backend launchers that flatten the buffer into a
    // kernel-params struct (backend/vulkan). Host-callable.
    float* raw_data() const { return _data; }
    static constexpr int32_t raw_stride() { return STRIDE; }

    // Same one-tensor-vector plumbing as TensorArray, so the pool-owned
    // storage still round-trips through the engine as DeviceTensorFloatND.
    PackedTensorArray(std::vector<DeviceTensorFloatND> tensors) {
        _size = 0;
        _data = nullptr;
        if (tensors.size() == 0) return;
        if (tensors.size() != 1)
            throw std::runtime_error("PackedTensorArray: expect 1 tensor, got "
                + std::to_string(tensors.size()));
        _data = tensors[0].data_ptr();
        if (_data == nullptr) return;
        _size = tensors[0].size(0);
        if (tensors[0].numel() != _size * (int64_t)STRIDE)
            throw std::runtime_error("PackedTensorArray: row stride mismatch");
    }

    // Keyed "<slot>.0" like TensorArray's sub-buffers: engine teardown frees
    // these by the "<slot>." prefix (engine_release_screen_buffers).
    static std::vector<DeviceTensorFloatND> empty_pool(
        int64_t size, PoolSlot key_prefix
    ) {
        DeviceTensor2D<float> b;
        b.resize_dynamic(slot_category(key_prefix),
                         std::string(slot_name(key_prefix)) + ".0",
                         size, (int64_t)STRIDE, slot_phase(key_prefix));
        return { DeviceTensorFloatND(b) };
    }

    static std::vector<DeviceTensorFloatND> zeros_pool(
        int64_t size, PoolSlot key_prefix
    ) {
        auto res = empty_pool(size, key_prefix);
        if (res[0].data_ptr() != nullptr)
            backend::memset_sync(res[0].data_ptr(), 0,
                                 (size_t)res[0].numel() * sizeof(float));
        return res;
    }

    static std::vector<DeviceTensorFloatND> zeros_pool(
        const std::vector<DeviceTensorFloatND>& tmpl_vec, PoolSlot key_prefix
    ) {
        return zeros_pool(PackedTensorArray<STRIDE>(tmpl_vec)._size, key_prefix);
    }

#ifdef __CUDACC__
    __forceinline__ __device__ float* row(int64_t i) {
        return _data + (int64_t)STRIDE * i;
    }
    __forceinline__ __device__ const float* row(int64_t i) const {
        return _data + (int64_t)STRIDE * i;
    }
#endif
};


template<int N>
class TensorArray {
protected:
    int64_t _size;
    float* _data[N];
    int32_t _strides[N];

    static_assert(N >= 1);

public:
    TensorArray() {
        _size = 0;
        for (int i = 0; i < N; ++i) {
            _data[i] = nullptr;
            _strides[i] = 0;
        }
    }

    TensorArray(const TensorArray& other) {
        _size = other._size;
        for (int i = 0; i < N; ++i) {
            _data[i] = other._data[i];
            _strides[i] = other._strides[i];
        }
    }

    int64_t size() const {
        return _size;
    }

    // Raw component access for backend launchers that flatten the buffer
    // into a kernel-params struct (backend/vulkan). Host-callable.
    float* raw_data(int i) const { return _data[i]; }
    int32_t raw_stride(int i) const { return _strides[i]; }

    TensorArray(std::vector<DeviceTensorFloatND> tensors) {
        if (tensors.size() == 0) {
            _size = 0;
            for (int i = 0; i < N; ++i) {
                _data[i] = nullptr;
                _strides[i] = 0;
            }
            return;
        }
        if (tensors.size() != N)
            throw std::runtime_error("Number of tensors mismatch: Expect "
                + std::to_string(N) + ", got " + std::to_string(tensors.size()));
        _size = 0;
        for (int i = 0; i < N; ++i)
            if (tensors[i].data_ptr() != nullptr) { _size = tensors[i].size(0); break; }
        for (int i = 0; i < N; ++i) {
            _data[i] = tensors[i].data_ptr();
            if (_data[i] != nullptr) {
                if (tensors[i].size(0) != _size)
                    throw std::runtime_error("Tensor size mismatch");
                _strides[i] = _size == 0 ? 0 : (int32_t)(tensors[i].numel() / _size);
            } else {
                // Shape-only descriptor (e.g. world.features_sh when SH value
                // quantization is on: the canonical store is the packed buffer
                // but consumers like sh_degree() still need the row stride).
                // Pointer arithmetic on a null _data[i] is fine as long as no
                // kernel actually dereferences it (the VALUE_BITS != 32 paths
                // read from the packed buffer instead). Skip default-
                // constructed tensors (ndim == 0) -- e.g. unused vr/h gradient
                // slots whose size(0) would throw "Invalid dimension".
                _strides[i] = (tensors[i].get_ndim() > 0 && tensors[i].size(0) > 0)
                    ? (int32_t)(tensors[i].numel() / tensors[i].size(0))
                    : 0;
            }
        }
    }

    // The N sub-buffers of an array are Never-class forward-cache scratch keyed
    // "<slot>.<i>" at runtime -> dynamic slots, categorized by the base slot.
    static std::vector<DeviceTensorFloatND> empty_pool(
        int64_t size, std::array<int32_t, N> strides, PoolSlot key_prefix
    ) {
        const VramCategory cat = slot_category(key_prefix);
        const PoolPhase    ph  = slot_phase(key_prefix);
        const std::string  base = slot_name(key_prefix);
        std::vector<DeviceTensorFloatND> res;
        for (int i = 0; i < N; ++i) {
            if (strides[i] <= 0) { res.push_back(DeviceTensorFloatND()); continue; }
            std::string key = base + "." + std::to_string(i);
            switch (strides[i]) {
                case 1: { DeviceVector<float>  b; b.resize_dynamic(cat, key, size, ph); res.push_back(DeviceTensorFloatND(b)); break; }
                case 2: { DeviceVector<float2> b; b.resize_dynamic(cat, key, size, ph); res.push_back(DeviceTensorFloatND(b)); break; }
                case 3: { DeviceVector<float3> b; b.resize_dynamic(cat, key, size, ph); res.push_back(DeviceTensorFloatND(b)); break; }
                case 4: { DeviceVector<float4> b; b.resize_dynamic(cat, key, size, ph); res.push_back(DeviceTensorFloatND(b)); break; }
                default: { DeviceTensor2D<float> b; b.resize_dynamic(cat, key, size, (int64_t)strides[i], ph); res.push_back(DeviceTensorFloatND(b)); break; }
            }
        }
        return res;
    }

    static std::vector<DeviceTensorFloatND> zeros_pool(
        int64_t size, std::array<int32_t, N> strides, PoolSlot key_prefix
    ) {
        auto res = empty_pool(size, strides, key_prefix);
        for (auto& t : res)
            if (t.data_ptr() != nullptr)
                backend::memset_sync(t.data_ptr(), 0,
                                     (size_t)t.numel() * sizeof(float));
        return res;
    }

    static std::vector<DeviceTensorFloatND> zeros_pool(
        const std::vector<DeviceTensorFloatND>& tmpl_vec, PoolSlot key_prefix
    ) {
        TensorArray<N> tmpl(tmpl_vec);
        std::array<int32_t, N> strides_arr;
        for (int i = 0; i < N; i++) strides_arr[i] = tmpl._strides[i];
        return zeros_pool(tmpl._size, strides_arr, key_prefix);
    }

};


