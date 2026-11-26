// The CUDA copy of backend/vulkan/shaders/rasterize_stochastic.slang: kernel,
// camera dispatch and host entry in one TU. Fragments are the regular
// rasterizers' (FragmentFwd), so alpha, colour, depth and normal match them.

#include "kernels/raster/RasterizationStochastic.cuh"

#include <cuda.h>
#include <cuda_runtime.h>
#include <cstdint>

#include "generated/slang.cuh"
namespace SlangProjectionUtils {
#include "generated/set_namespace.cuh"
#include "generated/projection_utils.cuh"
}

#include "core/CameraDistortion.cuh"
#include "kernels/projection/CameraVariants.cuh"

#include <core/Common.cuh>
#include "core/AabbQuant.cuh"
#include "primitives/Primitive.cuh"
#include "primitives/Primitive3DGS.cuh"
#include "primitives/Primitive3DGUT.cuh"
#include "primitives/Primitive2DGS.cuh"


namespace {

inline constexpr uint32_t TILE_AREA_S = TILE_SIZE_X * TILE_SIZE_Y;

// The regular sort's order: |depth| bits, then the flatten id (its write order).
__device__ __forceinline__ bool st_before(uint32_t ka, uint32_t ia, uint32_t kb, uint32_t ib) {
    return ka < kb || (ka == kb && ia < ib);
}

template<typename Prim, bool eval3d, CameraModelType camera_model, CameraDistortionType distortion>
__global__ void rasterize_stochastic_kernel(
    const uint32_t I,
    const uint32_t N,   // zero if packed
    const uint32_t n_isects,
    const uint32_t *__restrict__ gaussian_ids,
    const typename Prim::WorldBuffer splat_wbuffer,
    const typename Prim::ScreenBuffer splat_sbuffer,
    const float *__restrict__ depths,          // the sort depths
    const float *__restrict__ viewmats,        // [I, 4, 4]
    const float4 *__restrict__ intrins,        // [I, 4]
    const CameraDistortionCoeffsBuffer dist_coeffs_buffer,
    const uint2 *__restrict__ aabb,
    const uint32_t image_width,
    const uint32_t image_height,
    const uint32_t tile_width,
    const uint32_t tile_height,
    const int macro_log2,
    const int32_t *__restrict__ tile_offsets,
    const int32_t *__restrict__ flatten_ids,
    float *__restrict__ sum,                   // [I, H, W, 8]
    float3 *__restrict__ out_rgb,
    float *__restrict__ out_depth,             // or null
    float3 *__restrict__ out_normal,           // or null
    float *__restrict__ out_T,
    const uint32_t frame,
    const uint32_t spp,
    const bool winner,
    const bool depth_sorted,
    const bool stratified,
    const float kernel_cut
) {
    const uint32_t tid = threadIdx.x;
    const uint32_t image_id = blockIdx.x;
    const uint32_t mt_y = blockIdx.y;
    const uint32_t mt_x = blockIdx.z;
    const int32_t tile_id =
        (int32_t)((mt_y >> macro_log2) * tile_width + (mt_x >> macro_log2));
    tile_offsets += image_id * tile_height * tile_width;

    const uint32_t i = mt_y * TILE_SIZE_Y + tid / TILE_SIZE_X;
    const uint32_t j = mt_x * TILE_SIZE_X + tid % TILE_SIZE_X;
    const float px = (float)j + 0.5f;
    const float py = (float)i + 0.5f;
    bool inside = (i < image_height && j < image_width);

    [[maybe_unused]] float3x3 R = {1.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 1.f};
    float3 ray_o = make_float3(0.0f), ray_d = make_float3(0.0f, 0.0f, 1.0f);
    if constexpr (eval3d) {
        const float* vm = viewmats + image_id * 16;
        const float4 intrin = intrins[image_id];
        R = { vm[0], vm[1], vm[2], vm[4], vm[5], vm[6], vm[8], vm[9], vm[10] };
        const float3 cam_t = { vm[3], vm[7], vm[11] };
        const float fx = intrin.x, fy = intrin.y, cx = intrin.z, cy = intrin.w;
        CameraDistortionCoeffsT<distortion> dist_coeffs =
            dist_coeffs_buffer.load<distortion>(image_id);
        ray_o = SlangProjectionUtils::transform_ray_o(R, cam_t);
        float3 raydir;
        inside &= SlangDistortion<distortion>::generate_ray(
            {(px - cx) / fx, (py - cy) / fy}, (int)camera_model, dist_coeffs, &raydir);
        if constexpr (camera_model == CameraModelType::ORTHOGRAPHIC)
            ray_o += SlangProjectionUtils::transform_ray_d(R, {(px - cx) / fx, (py - cy) / fy, 0.0f});
        ray_d = SlangProjectionUtils::transform_ray_d(R, raydir);
    }
    const uint32_t pix = image_id * image_height * image_width + i * image_width + j;
    const uint32_t n_spp = min(spp, kStochasticMaxSpp);

    uint32_t key[kStochasticMaxSpp], id[kStochasticMaxSpp];
#pragma unroll
    for (uint32_t s = 0; s < kStochasticMaxSpp; ++s) key[s] = id[s] = 0xffffffffu;
    uint32_t zmax = inside ? 0xffffffffu : 0u;

    const int32_t range_start = tile_offsets[tile_id];
    const int32_t range_end =
        (image_id == I - 1) && (tile_id == (int32_t)(tile_width * tile_height) - 1)
            ? (int32_t)n_isects
            : tile_offsets[tile_id + 1];
    const uint32_t num_batches =
        (uint32_t)(max(range_end - range_start, 0) + (int32_t)TILE_AREA_S - 1) / TILE_AREA_S;

    const float mt_bx0 = (float)(mt_x * TILE_SIZE_X);
    const float mt_by0 = (float)(mt_y * TILE_SIZE_Y);
    const float2 mt_half = {0.5f * (TILE_SIZE_X - 1), 0.5f * (TILE_SIZE_Y - 1)};
    [[maybe_unused]] const float2 mt_centre = {mt_bx0 + 0.5f + mt_half.x, mt_by0 + 0.5f + mt_half.y};

    __shared__ typename Prim::FragmentFwd batch[TILE_AREA_S];
    __shared__ uint32_t s_hit[TILE_AREA_S], s_key[TILE_AREA_S], s_id[TILE_AREA_S], s_wi[TILE_AREA_S];
    __shared__ uint32_t s_zmax;

    float3 sum_rgb = make_float3(0.0f), sum_normal = make_float3(0.0f);
    float sum_depth = 0.0f, sum_alpha = 0.0f;
    uint32_t tile_zmax = 0xffffffffu;
    auto reduce_zmax = [&]() {
        __syncthreads();
        if (tid == 0) s_zmax = 0u;
        __syncthreads();
        atomicMax(&s_zmax, zmax);
        __syncthreads();
        return s_zmax;
    };

    for (int pass = 0; pass < 2; ++pass) {
        if (pass == 1) tile_zmax = reduce_zmax();
        for (uint32_t b = 0; b < num_batches; ++b) {
            // Pass 1's samples fill as it goes; pass 2's are final.
            if (pass == 0) tile_zmax = reduce_zmax();
            else __syncthreads();

            const int32_t batch_start = range_start + (int32_t)(TILE_AREA_S * b);
            // In key order, nothing after a splat behind every sample can count.
            if (depth_sorted &&
                __float_as_uint(fabsf(depths[flatten_ids[batch_start]])) > tile_zmax)
                break;
            const int32_t idx = batch_start + (int32_t)tid;
            bool hit = false;
            if (idx < range_end) {
                const int32_t g = flatten_ids[idx];
                const uint32_t wi = gaussian_ids ? gaussian_ids[g] : (uint32_t)g % max(N, 1u);
                const uint32_t dk = __float_as_uint(fabsf(depths[g]));
                s_key[tid] = dk;
                s_id[tid] = (uint32_t)g;
                s_wi[tid] = wi;
                if (dk <= tile_zmax) {
                    if constexpr (eval3d) {
                        hit = splat_sbuffer.opacities(g) > ALPHA_THRESHOLD &&
                            SlangProjectionUtils::aabb_reaches_micro_tile(
                                aabb16_decode(aabb[g], image_width, image_height),
                                mt_bx0, mt_by0, (float)TILE_SIZE_X) &&
                            !SlangProjectionUtils::slab_misses_box(
                                splat_sbuffer.scales(g), mt_centre, mt_half);
                        if (hit) batch[tid].load(splat_wbuffer, splat_sbuffer, wi, g);
                    } else {
                        batch[tid].load(splat_wbuffer, splat_sbuffer, wi, g);
                        const typename Prim::FragmentFwd& sp = batch[tid];
                        if (sp.opac > ALPHA_THRESHOLD) {
                            const float kk = 0.5f / kernel_reach(sp.opac, kernel_cut);
                            const float3 inv_cov = { sp.conic.x * kk, sp.conic.y * kk, sp.conic.z * kk };
                            hit = ellipse_box_overlap_test(inv_cov,
                                mt_bx0 - sp.xy.x, mt_bx0 + TILE_SIZE_X - sp.xy.x,
                                mt_by0 - sp.xy.y, mt_by0 + TILE_SIZE_Y - sp.xy.y);
                        }
                    }
                }
            }
            s_hit[tid] = hit;
            __syncthreads();

            const uint32_t batch_size = min(TILE_AREA_S, (uint32_t)(range_end - batch_start));
            for (uint32_t t = 0; t < batch_size; ++t) {
                if (!s_hit[t]) continue;
                const uint32_t dk = s_key[t];
                if (dk > zmax) continue;
                const typename Prim::FragmentFwd splat = batch[t];
                float alpha;
                RenderOutput c;
                if constexpr (eval3d) {
                    alpha = splat.evaluate_alpha(ray_o, ray_d);
                    if (alpha <= ALPHA_THRESHOLD) continue;
                    c = splat.evaluate_color(ray_o, ray_d);
                    if (c.depth <= 0.0f) continue;
                } else {
                    alpha = splat.evaluate_alpha(px, py, kernel_cut);
                    if (alpha <= ALPHA_THRESHOLD) continue;
                    c = splat.evaluate_color(px, py);
                }
                const uint32_t g = s_id[t];
                if (pass == 0) {
                    const uint32_t h = SlangProjectionUtils::stochastic_hash(pix, s_wi[t], frame);
                    const float u0 = SlangProjectionUtils::stochastic_uniform(h, 0u);
                    uint32_t farthest = 0u;
#pragma unroll
                    for (uint32_t s = 0; s < kStochasticMaxSpp; ++s) {
                        if (s < n_spp) {
                            // Stratified, u0 + s/S: each sample's draw is still
                            // uniform and independent across splats.
                            float u;
                            if (stratified) {
                                u = u0 + (float)s / (float)n_spp;
                                if (u >= 1.0f) u -= 1.0f;
                            } else {
                                u = SlangProjectionUtils::stochastic_uniform(h, s);
                            }
                            if (u < alpha && st_before(dk, g, key[s], id[s])) {
                                key[s] = dk;
                                id[s] = g;
                            }
                            farthest = max(farthest, key[s]);
                        }
                    }
                    zmax = farthest;
                } else {
                    // Each sample shows the splats at or in front of its own:
                    // P(nothing nearer accepted) is the transmittance there.
                    uint32_t n = 0u;
#pragma unroll
                    for (uint32_t s = 0; s < kStochasticMaxSpp; ++s)
                        if (s < n_spp)
                            n += winner ? (g == id[s]) : !st_before(key[s], id[s], dk, g);
                    if (n == 0u) continue;
                    float w = (float)n / (float)n_spp;
                    if (!winner) w *= alpha;
                    sum_rgb += w * c.rgb;
                    sum_depth += w * c.depth;
                    sum_normal += w * c.normal;
                    sum_alpha += w;
                }
            }
        }
    }

    if (inside) {
        if constexpr (eval3d)
            sum_normal = SlangProjectionUtils::undo_transform_ray_d(R, sum_normal);
        uint32_t filled = 0u;
#pragma unroll
        for (uint32_t s = 0; s < kStochasticMaxSpp; ++s)
            filled += (s < n_spp && key[s] != 0xffffffffu) ? 1u : 0u;
        float* acc = sum + kStochasticSum * (size_t)pix;
        float v[kStochasticSum] = { sum_rgb.x, sum_rgb.y, sum_rgb.z, sum_depth,
                                    sum_normal.x, sum_normal.y, sum_normal.z, sum_alpha,
                                    (float)filled / (float)n_spp };
#pragma unroll
        for (int k = 0; k < kStochasticSum; ++k) {
            if (frame != 0u) v[k] += acc[k];
            acc[k] = v[k];
        }
        // The weighted sums' average, scaled to the coverage: what a frame
        // averages either way, and in the limit the sorted composite.
        const float coverage = v[8] / (float)(frame + 1u);
        const float scale = v[7] > 0.0f ? coverage / v[7] : 0.0f;
        out_rgb[pix] = make_float3(v[0] * scale, v[1] * scale, v[2] * scale);
        if (out_depth) out_depth[pix] = v[7] > 0.0f ? v[3] / v[7] : 0.0f;
        if (out_normal) out_normal[pix] = make_float3(v[4] * scale, v[5] * scale, v[6] * scale);
        out_T[pix] = 1.0f - coverage;
    }
}

template<typename Prim, bool eval3d, CameraModelType camera_model, CameraDistortionType distortion>
void launch_stochastic(
    uint32_t I, uint32_t N, uint32_t n_isects, const uint32_t* gaussian_ids,
    const std::vector<DeviceTensorFloatND>& splats_w, const std::vector<DeviceTensorFloatND>& splats_s,
    const float* depths, const float* viewmats, const float4* intrins,
    const CameraDistortionCoeffsBuffer& dist_buf, const uint2* aabb,
    uint32_t W, uint32_t H, uint32_t tw, uint32_t th, int macro_log2,
    const int32_t* tile_offsets, const int32_t* flatten_ids,
    float3* rgb, float* depth, float3* normal, float* T,
    const StochasticFrame& f, float kernel_cut
) {
    typename Prim::WorldBuffer wbuffer(splats_w);
    typename Prim::ScreenBuffer sbuffer(splats_s);
    dim3 threads = {TILE_AREA_S, 1, 1};
    dim3 grid = {I, th << macro_log2, tw << macro_log2};
    rasterize_stochastic_kernel<Prim, eval3d, camera_model, distortion><<<grid, threads>>>(
        I, N, n_isects, gaussian_ids, wbuffer, sbuffer, depths, viewmats, intrins,
        dist_buf, aabb, W, H, tw, th, macro_log2, tile_offsets, flatten_ids,
        f.sum, rgb, depth, normal, T, f.frame, f.spp, f.winner, f.depth_sorted, f.stratified, kernel_cut);
}

}  // namespace


/*[AutoHeaderGeneratorExport]*/
std::tuple<
    RenderOutput::TensorTuple,  // renders: rgb, depth, normal (3dgut on request, 2dgs)
    DeviceTensor3D<float>       // transmittances
> rasterize_stochastic_fwd(
    int primitive,            // 0: 3dgs / mip, 1: 3dgut, 2: 2dgs
    bool output_normal,       // 3dgut renders its normal too
    bool output_depth,
    int64_t num_splats,
    std::vector<DeviceTensorFloatND> splats_w,
    std::vector<DeviceTensorFloatND> splats_s,
    DeviceVector<int32_t> gaussian_ids,
    DeviceTensorFloatND depths,       // what the tile intersector sorted by
    TorchTensorView viewmats,         // [I, 4, 4]
    TorchTensorView intrins,          // [I, 4]
    const std::string camera_model,
    const std::string distortion,
    const TorchTensorView dist_coeffs,
    DeviceTensor2D<uint2> aabb,
    const uint32_t image_width,
    const uint32_t image_height,
    const DeviceTensor3D<int32_t> tile_offsets,
    const DeviceVector<int32_t> flatten_ids,
    int macro_log2,
    StochasticFrame frame,
    float kernel_cut
) {
    const int64_t batch = tile_offsets.size<0>();
    const bool normal = primitive == 2 || (primitive == 1 && output_normal);
    RenderOutput::TensorTuple renders;
    if (normal)
        RenderOutput::resize<RenderOutputType::RGB_DN>(renders, batch, image_height, image_width,
                                                      PoolSlot::Renders, output_depth);
    else
        RenderOutput::resize<RenderOutputType::RGB_D>(renders, batch, image_height, image_width,
                                                     PoolSlot::Renders, output_depth);
    DeviceTensor3D<float> render_Ts;
    render_Ts.resize(PoolSlot::RenderTs, batch, image_height, image_width);

    const uint32_t N = gaussian_ids.data_ptr() ? 0 : (uint32_t)num_splats;
    const uint32_t tile_height = (uint32_t)tile_offsets.size<1>();
    const uint32_t tile_width = (uint32_t)tile_offsets.size<2>();
    const uint32_t n_isects = (uint32_t)flatten_ids.size();
    CameraDistortionCoeffsBuffer dist_buf(dist_coeffs);
    const float* viewmats_ptr = (const float*)std::get<0>(viewmats);
    const float4* intrins_ptr = (const float4*)std::get<0>(intrins);
    const uint32_t* gids = (const uint32_t*)gaussian_ids.data_ptr();
    float3* rgb = std::get<0>(renders).data_ptr();
    float* depth = output_depth ? std::get<1>(renders).data_ptr() : nullptr;
    float3* nrm = normal ? std::get<2>(renders).data_ptr() : nullptr;

    #define _ARGS (uint32_t)batch, N, n_isects, gids, splats_w, splats_s, depths.data_ptr(), \
        viewmats_ptr, intrins_ptr, dist_buf, aabb.data_ptr(), image_width, image_height, \
        tile_width, tile_height, macro_log2, tile_offsets.data_ptr(), flatten_ids.data_ptr(), \
        rgb, depth, nrm, render_Ts.data_ptr(), frame, kernel_cut
    if (primitive == 0) {
        launch_stochastic<Vanilla3DGS<0>, false, CameraModelType::PINHOLE, CameraDistortionType::None>(_ARGS);
    } else {
        const CameraModelType cm = cmt(camera_model);
        const CameraDistortionType cd = cdt(distortion);
        #define _DISPATCH(M, D) \
            if (cm == CameraModelType::M && cd == CameraDistortionType::D) { \
                if (primitive == 2) \
                    launch_stochastic<Vanilla2DGS<0>, true, CameraModelType::M, CameraDistortionType::D>(_ARGS); \
                else if (normal) \
                    launch_stochastic<Vanilla3DGUTNormal<0>, true, CameraModelType::M, CameraDistortionType::D>(_ARGS); \
                else \
                    launch_stochastic<Vanilla3DGUT<0>, true, CameraModelType::M, CameraDistortionType::D>(_ARGS); \
            } else
        SS_FOR_EACH_FORWARD_CAMERA_VARIANT(_DISPATCH)
            throw std::runtime_error("rasterize_stochastic_fwd: unsupported camera model / distortion tier");
        #undef _DISPATCH
    }
    #undef _ARGS
    CHECK_DEVICE_ERROR(cudaGetLastError());
    return std::make_tuple(renders, render_Ts);
}
