#ifdef _KERNEL_CUH_INC
#error "Rasterization forward kernel must be included no more than once"
#endif

#define _KERNEL_CUH_INC

#ifndef NO_TORCH
#define NO_TORCH
#endif


// Modified from https://github.com/nerfstudio-project/gsplat/blob/main/gsplat/cuda/csrc/RasterizeToPixels3DGSFwd.cu

#include <cuda.h>
#include <cuda_runtime.h>
#include <cstdint>

#include <cooperative_groups.h>
namespace cg = cooperative_groups;

#ifdef __CUDACC__
#include "generated/slang.cuh"
namespace SlangProjectionUtils {
#include "generated/set_namespace.cuh"
#include "generated/projection_utils.cuh"
}
#endif

#include "core/CameraDistortion.cuh"

#include <core/Common.cuh>
#include "core/AabbQuant.cuh"

#include "primitives/Primitive.cuh"


#ifndef IS_EVAL3D
#define IS_EVAL3D 1
#endif

// One CUDA block per micro-tile (TILE_SIZE_X x TILE_SIZE_Y pixels, one thread
// per pixel). Binning is still done at the coarser macro-tile granularity, so a
// block reads its macro tile's gaussian range and culls the ones that miss its
// micro-tile. Per-micro-tile blocks let the scheduler spread a dense macro
// tile's work across many SMs (a single huge macro tile no longer stalls the
// whole launch on one SM) and keep shared-memory tiny for high occupancy.
inline constexpr uint32_t TILE_AREA = TILE_SIZE_X * TILE_SIZE_Y;

template<
    typename SplatPrimitive,
#if IS_EVAL3D
    CameraModelType camera_model,
    CameraDistortionType distortion,
#endif
    DistortionType dist_type,
    bool output_median
>
#if IS_EVAL3D
__global__ void rasterize_to_pixels_eval3d_fwd_kernel(
#else
__global__ void rasterize_to_pixels_fwd_kernel(
#endif
    const uint32_t I,
    const uint32_t N,   // zero if packed
    const uint32_t n_isects,
    const uint32_t *__restrict__ gaussian_ids,  // [nnz], nullptr if not packed
    const typename SplatPrimitive::WorldBuffer splat_wbuffer,
    const typename SplatPrimitive::ScreenBuffer splat_sbuffer,
#if IS_EVAL3D
    const float *__restrict__ viewmats, // [B, C, 4, 4]
    const float4 *__restrict__ intrins,  // [B, C, 4], fx, fy, cx, cy
    const CameraDistortionCoeffsBuffer dist_coeffs_buffer,
    const uint2 *__restrict__ aabb,   // [..., N] packed, core/AabbQuant.cuh
#endif
    const uint32_t image_width,
    const uint32_t image_height,
    const uint32_t tile_width,
    const uint32_t tile_height,
    const int macro_log2,
    const int32_t *__restrict__ tile_offsets, // [I, tile_height, tile_width]
    const int32_t *__restrict__ flatten_ids,  // [n_isects]
    RenderOutput::Buffer render_colors, // [I, image_height, image_width, ...]
    float * render_Ts, // [I, image_height, image_width, 1]
    int32_t *__restrict__ last_ids, // [I, image_height, image_width]
    RenderOutput::Buffer render_distortions, // [I, image_height, image_width, ...]
    float * render_median // [I, image_height, image_width, 1], optional
) {
    auto block = cg::this_thread_block();
    uint32_t tid = threadIdx.x;  // 0 .. TILE_AREA-1, one per pixel
    int32_t image_id = blockIdx.x;

    // one block per micro-tile; recover the macro tile it belongs to for binning
    uint32_t mt_y = blockIdx.y;  // micro-tile row
    uint32_t mt_x = blockIdx.z;  // micro-tile col
    int32_t tile_id =
        (int32_t)((mt_y >> macro_log2) * tile_width + (mt_x >> macro_log2));

    tile_offsets += image_id * tile_height * tile_width;
    render_Ts += image_id * image_height * image_width;
    last_ids += image_id * image_height * image_width;
    if constexpr (output_median)
        render_median += image_id * image_height * image_width;

#if IS_EVAL3D
    // Load camera
    viewmats += image_id * 16;
    float4 intrin = intrins[image_id];
    float3x3 R = {
        viewmats[0], viewmats[1], viewmats[2],  // 1st row
        viewmats[4], viewmats[5], viewmats[6],  // 2nd row
        viewmats[8], viewmats[9], viewmats[10],  // 3rd row
    };
    float3 t = { viewmats[3], viewmats[7], viewmats[11] };
    float fx = intrin.x, fy = intrin.y, cx = intrin.z, cy = intrin.w;
    CameraDistortionCoeffsT<distortion> dist_coeffs = dist_coeffs_buffer.load<distortion>(image_id);

    float3 ray_o = SlangProjectionUtils::transform_ray_o(R, t);
#endif

    // this thread's pixel within the micro-tile
    uint32_t i = mt_y * TILE_SIZE_Y + tid / TILE_SIZE_X;
    uint32_t j = mt_x * TILE_SIZE_X + tid % TILE_SIZE_X;
    float px = (float)j + 0.5f;
    float py = (float)i + 0.5f;

    bool inside = (i < image_height && j < image_width);

#if IS_EVAL3D
    float3 raydir;
    inside &= SlangDistortion<distortion>::generate_ray(
        {(px-cx)/fx, (py-cy)/fy},
        (int)camera_model, dist_coeffs,
        &raydir
    );
    float3 ray_d = SlangProjectionUtils::transform_ray_d(R, raydir);
#endif

    bool done = !inside;

    // current visibility left to render
    int32_t pix_id = i * image_width + j;
    int32_t pix_id_global = image_id * image_height * image_width + pix_id;
    float T = 1.0f;
    // index of most recent gaussian to write to this thread's pixel
    uint32_t cur_idx = 0;

    // median depth: depth where post-splat transmittance crosses 1/2, with z
    // interpolated linearly in ln(T). Virtual nearest point is (z=0, T=1).
    // Stays 0 if T never drops below 1/2.
    [[maybe_unused]] float median_depth = 0.0f;
    [[maybe_unused]] float median_prev_z = 0.0f;
    [[maybe_unused]] float median_prev_T = 1.0f;

    RenderOutput pix_out = RenderOutput::zero();   // C = sum_j w_j c_j (raw)
    RenderOutput pix2_out = RenderOutput::zero();  // S = sum_j w_j c_j^2

    // gaussians overlapping this micro-tile's macro tile
    int32_t range_start = tile_offsets[tile_id];
    int32_t range_end =
        (image_id == I - 1) && (tile_id == tile_width * tile_height - 1)
            ? n_isects
            : tile_offsets[tile_id + 1];
    uint32_t num_batches =
        (range_end - range_start + TILE_AREA - 1) / TILE_AREA;

    // micro-tile pixel box, for the footprint cull
    const float mt_bx0 = (float)(mt_x * TILE_SIZE_X);
    const float mt_by0 = (float)(mt_y * TILE_SIZE_Y);

    __shared__ typename SplatPrimitive::FragmentFwd splat_batch[TILE_AREA];
    // whether the loaded gaussian's footprint reaches this micro-tile (uniform
    // across the whole block, so skipping it is divergence-free)
    __shared__ bool splat_hit[TILE_AREA];

    // collect and process batches of gaussians, one thread loads one gaussian
    for (uint32_t b = 0; b < num_batches; ++b) {
        // resync all threads before the next batch overwrites shared, and stop
        // once every pixel in the block is done
        if (__syncthreads_count(done) >= (int)TILE_AREA)
            break;

        uint32_t batch_start = range_start + TILE_AREA * b;
        uint32_t idx = batch_start + tid;
        bool hit = false;
        if (idx < (uint32_t)range_end) {
            int32_t g = flatten_ids[idx]; // flatten index in [I * N] or [nnz]
            uint32_t wi = gaussian_ids ? gaussian_ids[g] : g % N;
        #if IS_EVAL3D
            // 3dgut keeps the projected 2D conic in the screen "scale" channel
            // and the ellipse center is the AABB center. Cull here, before the
            // (expensive) 3D fragment load, matching the tile intersector.
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
        #else
            // 2D splatting: the fragment is just the screen data, so load it
            // (cheap) and cull from its own conic / opacity.
            splat_batch[tid].load(splat_wbuffer, splat_sbuffer, wi, g);
            const typename SplatPrimitive::FragmentFwd& sp = splat_batch[tid];
            if (sp.opac > ALPHA_THRESHOLD) {
                float kk = 0.5f / __logf(sp.opac / ALPHA_THRESHOLD);
                float3 inv_cov = { sp.conic.x * kk, sp.conic.y * kk, sp.conic.z * kk };
                hit = ellipse_box_overlap_test(inv_cov,
                    mt_bx0 - sp.xy.x, mt_bx0 + TILE_SIZE_X - sp.xy.x,
                    mt_by0 - sp.xy.y, mt_by0 + TILE_SIZE_Y - sp.xy.y);
            }
        #endif
        }
        splat_hit[tid] = hit;

        // wait for other threads to collect the gaussians in the batch
        block.sync();

        // process gaussians in the current batch for this pixel
        uint32_t batch_size = min((uint32_t)TILE_AREA, (uint32_t)(range_end - batch_start));
        for (uint32_t t = 0; t < batch_size; ++t) {
            if (!splat_hit[t])  // gaussian misses this micro-tile (uniform skip)
                continue;
            if (done)
                continue;
            typename SplatPrimitive::FragmentFwd splat = splat_batch[t];
        #if IS_EVAL3D
            float alpha = splat.evaluate_alpha(ray_o, ray_d);
        #else
            float alpha = splat.evaluate_alpha(px, py);
        #endif
            if (alpha <= ALPHA_THRESHOLD)
                continue;

        #if IS_EVAL3D
            const RenderOutput color = splat.evaluate_color(ray_o, ray_d);
            if (color.depth <= 0.0f)
                continue;
        #else
            const RenderOutput color = splat.evaluate_color(px, py);
        #endif

            const float next_T = T * (1.0f - alpha);

            // median depth: detect the (unique) post-T crossing of 1/2.
            // Done before the 1e-4 early-out so a splat that drops T past 1/2
            // in one step is still captured. z is interpolated in ln(T).
            if constexpr (output_median) {
                if (median_prev_T >= 0.5f && next_T < 0.5f) {
                    float a = __logf(median_prev_T);
                    float b = __logf(next_T);
                    float c = -0.6931471805599453f;  // ln(1/2)
                    float d = b - a;
                    float f = (fabsf(d) > 1e-20f) ? (c - a) / d : 0.0f;
                    median_depth = median_prev_z + (color.depth - median_prev_z) * f;
                }
                median_prev_T = next_T;
                median_prev_z = color.depth;
            }

            if (next_T > 1e-4f) {
                const float vis = alpha * T;
                // Depth distortion operates in log space: the depth channel is
                // accumulated as ln(z) (rgb/normal unchanged), so C = pix_out and
                // S = pix2_out are the log-depth moments. The rendered ray depth
                // is recovered as exp(expected log depth) after the loop, so no
                // extra global buffer is needed.
                RenderOutput acc = color;
                if constexpr (dist_has_depth(dist_type))
                    acc.depth = __logf(fmaxf(color.depth, DEPTH_DIST_EPS));
                // Distortion uses the closed form D = W*S - C^2 (computed once
                // after the loop), so here we only accumulate the second moment
                // S; the per-splat distortion increment is no longer needed.
                if constexpr (dist_any(dist_type))
                    pix2_out += acc * acc * vis;
                pix_out += acc * vis;
                cur_idx = batch_start + t;
                T = next_T;
            } else done = true;

        }  // for (uint32_t t = 0; t < batch_size; ++t)
    }

    if (i < image_height && j < image_width) {
        render_Ts[pix_id] = T;
        if constexpr (output_median)
            render_median[pix_id] = median_depth;
        // distortion: closed form D = W*S - C^2, with W = 1 - T_final and the
        // raw (un-normalized) accumulators C = pix_out, S = pix2_out. Must be
        // evaluated before the depth channel of C is normalized below.
        if constexpr (dist_any(dist_type)) {
            const float W = 1.0f - T;
            RenderOutput distortion_out = pix2_out * W + pix_out * pix_out * -1.0f;
            distortion_out.saveDistortion<dist_type>(render_distortions, pix_id_global);
        }
        if constexpr (RenderOutput::has_depth(SplatPrimitive::pixelType)) {
            const float invW = 1.0f / fmaxf(1.0f - T, 1e-10f);
            if constexpr (dist_has_depth(dist_type))
                // pix_out.depth holds C_logz = sum_j w_j ln z_j; render the
                // geometric-mean-style depth = exp(expected log depth).
                pix_out.depth = __expf(pix_out.depth * invW);
            else
                pix_out.depth *= invW;  // expected (arithmetic) ray depth
        }
        pix_out.saveParamsToBuffer<SplatPrimitive::pixelType>(render_colors, pix_id_global);
        // index in bin of last gaussian in this pixel
        last_ids[pix_id] = static_cast<int32_t>(cur_idx);
    }

}

template<
    typename SplatPrimitive,
#if IS_EVAL3D
    CameraModelType camera_model,
    CameraDistortionType distortion,
#endif
    DistortionType dist_type,
    bool output_median
>
#if IS_EVAL3D
void rasterize_to_pixels_eval3d_fwd_kernel_wrapper(
#else
void rasterize_to_pixels_fwd_kernel_wrapper(
#endif
    cudaStream_t stream,
    const uint32_t I,
    const uint32_t N,
    const uint32_t n_isects,
    const uint32_t *__restrict__ gaussian_ids,  // [nnz] optional, for packed mode
    const typename SplatPrimitive::WorldBuffer splat_wbuffer,
    const typename SplatPrimitive::ScreenBuffer splat_sbuffer,
#if IS_EVAL3D
    const float *__restrict__ viewmats, // [B, C, 4, 4]
    const float4 *__restrict__ intrins,  // [B, C, 4], fx, fy, cx, cy
    const CameraDistortionCoeffsBuffer dist_coeffs_buffer,
    const uint2 *__restrict__ aabb,   // [..., N] packed, core/AabbQuant.cuh
#endif
    const uint32_t image_width,
    const uint32_t image_height,
    const uint32_t tile_width,
    const uint32_t tile_height,
    const int macro_log2,
    const int32_t *__restrict__ tile_offsets, // [I, tile_height, tile_width]
    const int32_t *__restrict__ flatten_ids,  // [n_isects]
    RenderOutput::Buffer render_colors, // [I, image_height, image_width, ...]
    float *__restrict__ render_Ts, // [I, image_height, image_width, 1]
    int32_t *__restrict__ last_ids, // [I, image_height, image_width]
    RenderOutput::Buffer render_distortions, // [I, image_height, image_width, ...]
    float *__restrict__ render_median // [I, image_height, image_width, 1], optional
) {
    // One block per micro-tile. The macro tile spans 1 << macro_log2 micro
    // tiles per axis, so the grid is the macro-tile grid scaled up by that.
    dim3 threads = {TILE_AREA, 1, 1};
    dim3 grid = {I, tile_height << macro_log2, tile_width << macro_log2};

#if IS_EVAL3D
    rasterize_to_pixels_eval3d_fwd_kernel<
#else
    rasterize_to_pixels_fwd_kernel<
#endif
        SplatPrimitive,
    #if IS_EVAL3D
        camera_model, distortion,
    #endif
        dist_type,
        output_median
    ><<<grid, threads, 0, stream>>>(
        I, N, n_isects,
        gaussian_ids, splat_wbuffer, splat_sbuffer,
    #if IS_EVAL3D
        viewmats, intrins, dist_coeffs_buffer, aabb,
    #endif
        image_width, image_height, tile_width, tile_height, macro_log2,
        tile_offsets, flatten_ids,
        render_colors, render_Ts, last_ids,
        render_distortions,
        render_median
    );
}

#undef IS_EVAL3D
