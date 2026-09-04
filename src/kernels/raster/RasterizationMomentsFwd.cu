/*
 * RasterizationMomentsFwd.cu  -- see RasterizationMomentsFwd.cuh.
 *
 * Cloned from RasterizationEval3DFwd_kernel.cuh's front-to-back tile walk; the
 * only change is the per-pixel accumulator (occupancy moments instead of color).
 * Self-contained: kernel + camera-model dispatch + host entry in one TU.
 */

#include "kernels/raster/RasterizationMomentsFwd.cuh"

#include <cuda.h>
#include <cuda_runtime.h>
#include <cstdint>

#include <cooperative_groups.h>
namespace cg = cooperative_groups;

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
#include "primitives/Primitive3DGUT.cuh"


namespace {

using Prim = Vanilla3DGUT<0>;

inline constexpr uint32_t TILE_AREA_M = TILE_SIZE_X * TILE_SIZE_Y;

// One CUDA block per micro-tile (one thread per pixel); binning is at the
// macro-tile granularity. Mirrors rasterize_to_pixels_eval3d_fwd_kernel.
template<CameraModelType camera_model, CameraDistortionType distortion>
__global__ void moments_fwd_kernel(
    const uint32_t I,
    const uint32_t N,   // zero if packed
    const uint32_t n_isects,
    const uint32_t *__restrict__ gaussian_ids,  // [nnz], nullptr if not packed
    const Prim::WorldBuffer splat_wbuffer,
    const Prim::ScreenBuffer splat_sbuffer,
    const float *__restrict__ viewmats,        // [I, 4, 4]
    const float4 *__restrict__ intrins,        // [I, 4]
    const CameraDistortionCoeffsBuffer dist_coeffs_buffer,
    const uint2 *__restrict__ aabb,            // [..., N] packed, core/AabbQuant.cuh
    const uint32_t image_width,
    const uint32_t image_height,
    const uint32_t tile_width,
    const uint32_t tile_height,
    const int macro_log2,
    const int32_t *__restrict__ tile_offsets,  // [I, tile_h, tile_w]
    const int32_t *__restrict__ flatten_ids,   // [n_isects]
    float3 *__restrict__ render_moments,        // [I, H, W] (m0, mean, std)
    float3 *__restrict__ render_rgb             // [I, H, W] full-ray DC color, or null
) {
    auto block = cg::this_thread_block();
    uint32_t tid = threadIdx.x;
    int32_t image_id = blockIdx.x;

    uint32_t mt_y = blockIdx.y;
    uint32_t mt_x = blockIdx.z;
    int32_t tile_id =
        (int32_t)((mt_y >> macro_log2) * tile_width + (mt_x >> macro_log2));

    tile_offsets += image_id * tile_height * tile_width;
    render_moments += image_id * image_height * image_width;
    if (render_rgb) render_rgb += image_id * image_height * image_width;

    // Load camera
    viewmats += image_id * 16;
    float4 intrin = intrins[image_id];
    float3x3 R = {
        viewmats[0], viewmats[1], viewmats[2],
        viewmats[4], viewmats[5], viewmats[6],
        viewmats[8], viewmats[9], viewmats[10],
    };
    float3 cam_t = { viewmats[3], viewmats[7], viewmats[11] };
    float fx = intrin.x, fy = intrin.y, cx = intrin.z, cy = intrin.w;
    CameraDistortionCoeffsT<distortion> dist_coeffs = dist_coeffs_buffer.load<distortion>(image_id);

    float3 ray_o = SlangProjectionUtils::transform_ray_o(R, cam_t);

    uint32_t i = mt_y * TILE_SIZE_Y + tid / TILE_SIZE_X;
    uint32_t j = mt_x * TILE_SIZE_X + tid % TILE_SIZE_X;
    float px = (float)j + 0.5f;
    float py = (float)i + 0.5f;

    bool inside = (i < image_height && j < image_width);

    float3 raydir;
    inside &= SlangDistortion<distortion>::generate_ray(
        {(px - cx) / fx, (py - cy) / fy},
        (int)camera_model, dist_coeffs, &raydir);
    float3 ray_d = SlangProjectionUtils::transform_ray_d(R, raydir);

    bool done = !inside;
    int32_t pix_id = i * image_width + j;

    float T = 1.0f;
    float z1 = 0.0f, z2 = 0.0f, v1 = 0.0f, v2 = 0.0f, c0 = 0.0f, c1 = 0.0f;
    float3 crgb = make_float3(0.0f, 0.0f, 0.0f);

    int32_t range_start = tile_offsets[tile_id];
    int32_t range_end =
        (image_id == (int32_t)I - 1) && (tile_id == (int32_t)(tile_width * tile_height) - 1)
            ? (int32_t)n_isects
            : tile_offsets[tile_id + 1];
    uint32_t num_batches = (range_end - range_start + TILE_AREA_M - 1) / TILE_AREA_M;

    const float mt_bx0 = (float)(mt_x * TILE_SIZE_X);
    const float mt_by0 = (float)(mt_y * TILE_SIZE_Y);

    __shared__ Prim::FragmentFwd splat_batch[TILE_AREA_M];
    __shared__ bool splat_hit[TILE_AREA_M];

    for (uint32_t b = 0; b < num_batches; ++b) {
        if (__syncthreads_count(done) >= (int)TILE_AREA_M)
            break;

        uint32_t batch_start = range_start + TILE_AREA_M * b;
        uint32_t idx = batch_start + tid;
        bool hit = false;
        if (idx < (uint32_t)range_end) {
            int32_t g = flatten_ids[idx];
            uint32_t wi = gaussian_ids ? gaussian_ids[g] : g % N;
            float opac = splat_sbuffer.opacities(g);
            if (opac > ALPHA_THRESHOLD) {
                float3 conic = splat_sbuffer.scales(g);
                float2 ec = aabb16_center(aabb[g], image_width, image_height);
                float ecx = ec.x, ecy = ec.y;
                float kk = 0.5f / __logf(opac / ALPHA_THRESHOLD);
                float3 inv_cov = { conic.x * kk, conic.y * kk, conic.z * kk };
                hit = ellipse_box_overlap_test(inv_cov,
                    mt_bx0 - ecx, mt_bx0 + TILE_SIZE_X - ecx,
                    mt_by0 - ecy, mt_by0 + TILE_SIZE_Y - ecy);
            }
            if (hit)
                splat_batch[tid].load(splat_wbuffer, splat_sbuffer, wi, g);
        }
        splat_hit[tid] = hit;

        block.sync();

        uint32_t batch_size = min((uint32_t)TILE_AREA_M, (uint32_t)(range_end - batch_start));
        for (uint32_t k = 0; k < batch_size; ++k) {
            if (!splat_hit[k]) continue;
            if (done) continue;
            Prim::FragmentFwd splat = splat_batch[k];

            float alpha = splat.evaluate_alpha(ray_o, ray_d);
            if (alpha <= ALPHA_THRESHOLD) continue;

            RenderOutput col = splat.evaluate_color(ray_o, ray_d);
            float z = col.depth;
            if (z <= 0.0f) continue;

            const float next_T = T * (1.0f - alpha);
            if (next_T > 1e-4f) {
                const float vis = alpha * T;
                // const float v = 1.0f - next_T;
                const float v = sqrtf(fmaxf(-__logf(fmaxf(next_T, 1e-12f)), 1e-12f));
            #if 1
                if (z1 == 0.0f || (z2 == 0.0f && z <= z1))
                    z1 = z, v1 = v;
                else {
                    if (z2 == 0.0f)
                        z2 = z, v2 = v;
                    else if (v > c0 + c1 * z) {
                        if (z < z1) z1 = z, v1 = v;
                        else z2 = z, v2 = v;
                    }
                    // if (1.0f - next_T <= 0.95f) {
                    if (1.0f - T <= 0.9f) {  // TODO: too handy
                        c1 = (v2 - v1) / (z2 - z1);
                        c0 = v1 - c1 * z1;
                    }
                }
            #else
                if (z > z1)
                    z2 = z, v2 = v;
                if ((v1 - 0.5f) * (v2 - 0.5f) < 0.0f) {
                    // float u1 = __logf(1.0f - v1), u2 = __logf(1.0f - v2);
                    // c1 = (u2 - u1) / (z2 - z1);
                    // c0 = u1 - c1 * z1;
                    c1 = (v2 - v1) / (z2 - z1);
                    c0 = v1 - c1 * z1;
                }
                z1 = z2, v1 = v2;
            #endif
                if (render_rgb) {
                    crgb.x += vis * col.rgb.x;
                    crgb.y += vis * col.rgb.y;
                    crgb.z += vis * col.rgb.z;
                }
                T = next_T;
            } else done = true;
        }
    }

    if (i < image_height && j < image_width) {
        render_moments[pix_id] = make_float3(1.0f - T, c0, c1);                                               
        if (render_rgb) {
            float inv = (T < 1.0f) ? 1.0f / (1.0f - T) : 0.0f;  // normalize to full-ray color
            render_rgb[pix_id] = make_float3(crgb.x * inv, crgb.y * inv, crgb.z * inv);
        }
    }
}

template<CameraModelType camera_model, CameraDistortionType distortion>
void launch_moments(
    uint32_t I, uint32_t N, uint32_t n_isects,
    const uint32_t* gaussian_ids,
    const Prim::WorldBuffer& wbuffer, const Prim::ScreenBuffer& sbuffer,
    const float* viewmats, const float4* intrins,
    const CameraDistortionCoeffsBuffer& dist_buf, const uint2* aabb,
    uint32_t W, uint32_t H, uint32_t tw, uint32_t th, int macro_log2,
    const int32_t* tile_offsets, const int32_t* flatten_ids,
    float3* render_moments, float3* render_rgb
) {
    dim3 threads = { TILE_AREA_M, 1, 1 };
    dim3 grid = { I, th << macro_log2, tw << macro_log2 };
    moments_fwd_kernel<camera_model, distortion><<<grid, threads>>>(
        I, N, n_isects, gaussian_ids, wbuffer, sbuffer,
        viewmats, intrins, dist_buf, aabb,
        W, H, tw, th, macro_log2, tile_offsets, flatten_ids,
        render_moments, render_rgb);
}

}  // namespace


void rasterize_moments_3dgut_fwd(
    int64_t num_splats,
    std::vector<DeviceTensorFloatND> splats_w,
    std::vector<DeviceTensorFloatND> splats_s,
    DeviceVector<int32_t> gaussian_ids,
    TorchTensorView viewmats,
    TorchTensorView intrins,
    const std::string& camera_model,
    const std::string& distortion,
    TorchTensorView dist_coeffs,
    DeviceTensor2D<uint2> aabb,
    uint32_t image_width,
    uint32_t image_height,
    const DeviceTensor3D<int32_t>& tile_offsets,
    const DeviceVector<int32_t>& flatten_ids,
    int macro_log2,
    float3* render_moments,
    float3* render_rgb
) {
    uint32_t N = gaussian_ids.data_ptr() ? 0 : (uint32_t)num_splats;
    uint32_t I = (uint32_t)tile_offsets.size<0>();
    uint32_t tile_height = (uint32_t)tile_offsets.size<1>();
    uint32_t tile_width = (uint32_t)tile_offsets.size<2>();
    uint32_t n_isects = (uint32_t)flatten_ids.size();

    Prim::WorldBuffer wbuffer(splats_w);
    Prim::ScreenBuffer sbuffer(splats_s);
    CameraDistortionCoeffsBuffer dist_buf(dist_coeffs);

    const float* viewmats_ptr = (const float*)std::get<0>(viewmats);
    const float4* intrins_ptr = (const float4*)std::get<0>(intrins);
    const uint2* aabb_ptr = aabb.data_ptr();
    const uint32_t* gids = (const uint32_t*)gaussian_ids.data_ptr();

    CameraModelType cm = cmt(camera_model);
    CameraDistortionType cd = cdt(distortion);

    #define _ARGS \
        I, N, n_isects, gids, wbuffer, sbuffer, \
        viewmats_ptr, intrins_ptr, dist_buf, aabb_ptr, \
        image_width, image_height, tile_width, tile_height, macro_log2, \
        tile_offsets.data_ptr(), flatten_ids.data_ptr(), \
        render_moments, render_rgb

    #define _DISPATCH(M, D) \
        if (cm == CameraModelType::M && cd == CameraDistortionType::D) \
            launch_moments<CameraModelType::M, CameraDistortionType::D>(_ARGS); else
    SS_FOR_EACH_CAMERA_VARIANT(_DISPATCH)
        throw std::runtime_error("rasterize_moments_3dgut_fwd: unsupported camera model / distortion tier");
    #undef _DISPATCH
    CHECK_DEVICE_ERROR(cudaGetLastError());

    #undef _ARGS
}
