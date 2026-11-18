#pragma once

// 2DGS surfels: 3DGUT's world and screen rows, projection backward and eval3d
// rasterizer, with a ray-disc fragment that also renders camera-space normals.
// scale.z is stored but ignored; it gets no gradient.

#include "primitives/Primitive3DGUT.cuh"

// Seeded scale.z: below any disc axis, so splits and max-scale tests never
// pick it, and above per_splat_losses.slang's kMinLogScale.
constexpr float kSurfelLogScaleZ = -20.0f;

template<int sh_degree>
struct Vanilla2DGS : public Eval3DPrimitive<sh_degree, true> {
    static constexpr RenderOutputType pixelType = RenderOutputType::RGB_DN;

    using WorldBuffer = typename Eval3DPrimitive<sh_degree, true>::WorldBuffer;
    using ScreenBuffer = typename Eval3DPrimitive<sh_degree, true>::ScreenBuffer;

#ifdef __CUDACC__

    // pix_size: see surfel_ray_hit in shaders/projection_utils.slang.
    struct FragmentFwd {
        float3 mean;
        float3x3 iscl_rot;
        float opacity;
        float3 rgb;

        static __device__ __forceinline__ FragmentFwd zero() {
            FragmentFwd f;
            f.mean = make_float3(0.f);
            f.iscl_rot = float3x3{0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f};
            f.opacity = 0.f;
            f.rgb = make_float3(0.f);
            return f;
        }

        __device__ __forceinline__ void load(
            const WorldBuffer &wbuffer,
            const ScreenBuffer &sbuffer,
            int64_t wi, int64_t si
        ) {
            mean = wbuffer.means(wi);
            iscl_rot = SlangProjectionUtils::compute_2dgs_iscl_rot(wbuffer.quats(wi), wbuffer.scales(wi));
            opacity = sbuffer.opacities(si);
            rgb = sbuffer.colors(si);
        }

        __device__ __forceinline__ float evaluate_alpha(
            float3 ray_o, float3 ray_d, float2 pix_size
        ) const {
            return SlangProjectionUtils::evaluate_alpha_2dgs(
                mean, iscl_rot, opacity, ray_o, ray_d, pix_size);
        }

        __device__ __forceinline__ RenderOutput evaluate_color(
            float3 ray_o, float3 ray_d, float2 pix_size
        ) const {
            float3 out_rgb, out_normal; float out_depth;
            SlangProjectionUtils::evaluate_color_2dgs(
                mean, iscl_rot, opacity, rgb, ray_o, ray_d, pix_size,
                &out_rgb, &out_depth, &out_normal);
            return { out_rgb, out_depth, out_normal };
        }
    };

    struct FragmentBwd {
        float3 mean;
        float4 quat;
        float3 scale;
        float3x3 iscl_rot;
        float opacity;
        float3 rgb;

        static __device__ __forceinline__ FragmentBwd zero(const FragmentBwd& bwd) {
            FragmentBwd f;
            f.mean = make_float3(0.f);
            f.quat = bwd.quat;
            f.scale = bwd.scale;
            f.iscl_rot = float3x3{0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f};
            f.opacity = 0.f;
            f.rgb = make_float3(0.f);
            return f;
        }

        __device__ __forceinline__ void load(
            const WorldBuffer &wbuffer,
            const ScreenBuffer &sbuffer,
            int64_t wi, int64_t si
        ) {
            mean = wbuffer.means(wi);
            quat = wbuffer.quats(wi);
            scale = wbuffer.scales(wi);
            iscl_rot = SlangProjectionUtils::compute_2dgs_iscl_rot(quat, scale);
            opacity = sbuffer.opacities(si);
            rgb = sbuffer.colors(si);
        }

        // On a gradient fragment, iscl_rot holds d/d(iscl_rot).
        __device__ __forceinline__ void atomicStore(
            WorldBuffer &wbuffer,
            ScreenBuffer &sbuffer,
            int64_t wi, int64_t si
        ) const {
            float4 v_quat; float3 v_scale;
            SlangProjectionUtils::compute_2dgs_iscl_rot_vjp(quat, scale, iscl_rot, &v_quat, &v_scale);
            if (&wbuffer.means(0)) atomicAddFVec(&wbuffer.means(wi), mean);
            if (&wbuffer.quats(0)) atomicAddFVec(&wbuffer.quats(wi), v_quat);
            if (&wbuffer.scales(0)) atomicAddFVec(&wbuffer.scales(wi), v_scale);
            if (sbuffer.allocated()) {
                atomicAddFVec(&sbuffer.opacities(si), opacity);
                atomicAddFVec(&sbuffer.colors(si), rgb);
            }
        }

        __device__ __forceinline__ float evaluate_alpha(
            float3 ray_o, float3 ray_d, float2 pix_size
        ) const {
            return SlangProjectionUtils::evaluate_alpha_2dgs(
                mean, iscl_rot, opacity, ray_o, ray_d, pix_size);
        }

        __device__ __forceinline__ RenderOutput evaluate_color(
            float3 ray_o, float3 ray_d, float2 pix_size
        ) const {
            float3 out_rgb, out_normal; float out_depth;
            SlangProjectionUtils::evaluate_color_2dgs(
                mean, iscl_rot, opacity, rgb, ray_o, ray_d, pix_size,
                &out_rgb, &out_depth, &out_normal);
            return { out_rgb, out_depth, out_normal };
        }

        __device__ __forceinline__ void evaluate_vjp(
            float3 ray_o, float3 ray_d, float2 pix_size,
            float v_alpha, RenderOutput v_render,
            FragmentBwd &v_frag, float3 &v_ray_o, float3 &v_ray_d
        ) const {
            float3 v_mean; float3x3 v_iscl_rot; float v_opacity; float3 v_rgb;
            float3 v_ray_o_t, v_ray_d_t;
            SlangProjectionUtils::evaluate_2dgs_vjp(
                mean, iscl_rot, opacity, rgb, ray_o, ray_d, pix_size,
                v_alpha, v_render.rgb, v_render.depth, v_render.normal,
                &v_mean, &v_iscl_rot, &v_opacity, &v_rgb, &v_ray_o_t, &v_ray_d_t);
            v_frag.mean += v_mean;
            v_frag.iscl_rot = v_frag.iscl_rot + v_iscl_rot;
            v_frag.opacity += v_opacity;
            v_frag.rgb += v_rgb;
            v_ray_o += v_ray_o_t;
            v_ray_d += v_ray_d_t;
        }
    };

#endif  // #ifdef __CUDACC__
};
