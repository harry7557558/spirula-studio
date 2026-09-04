#pragma once

#ifdef __CUDACC__
#include "generated/slang.cuh"
namespace Slang3DGS {
#include "generated/set_namespace.cuh"
#include "generated/primitive_3dgs.cuh"
}
namespace SlangHarmonics {
#include "generated/set_namespace.cuh"
#include "generated/harmonics.cuh"
}
#endif

#include "primitives/PrimitiveBase3DGS.cuh"
#include "primitives/PrimitiveProjection.cuh"

template<int sh_degree>
struct Vanilla3DGUT : public _BasePrimitive3DGUT<sh_degree> {
    static constexpr RenderOutputType pixelType = RenderOutputType::RGB_D;

    using WorldBuffer = typename _BasePrimitive3DGUT<sh_degree>::WorldBuffer;
    using ScreenBuffer = typename _BasePrimitive3DGUT<sh_degree>::ScreenBuffer;

#ifdef __CUDACC__

    struct Screen;

    struct World : public _BasePrimitive3DGUT<sh_degree>::World{
        World() = default;
        __device__ World(const typename _BasePrimitive3DGS<sh_degree>::World& other) {
            _BasePrimitive3DGS<sh_degree>::World::operator=(other);
        }
        __device__ World& operator=(const typename _BasePrimitive3DGS<sh_degree>::World& other) {
            _BasePrimitive3DGS<sh_degree>::World::operator=(other);
            return *this;
        }
        __device__ World(const typename _BasePrimitive3DGUT<sh_degree>::World& other) {
            _BasePrimitive3DGUT<sh_degree>::World::operator=(other);
        }
        __device__ World& operator=(const typename _BasePrimitive3DGUT<sh_degree>::World& other) {
            _BasePrimitive3DGUT<sh_degree>::World::operator=(other);
            return *this;
        }

        // See Primitive3DGS::project() for VALUE_BITS semantics. Identical
        // contract; the 3dgut path uses a different projection geometry but
        // shares the SH eval pipeline.
        template<CameraModelType camera_model, CameraDistortionType distortion,
                 int VALUE_BITS = 32>
        inline __device__ void project(
            ProjCameraT<distortion> cam,
            Vanilla3DGUT<sh_degree>::Screen& proj, float4& aabb, float& sorting_depth, float& radius,
            uint8_t* sh_packed = nullptr,
            float2*  sh_bounds = nullptr,
            int64_t  sh_base   = 0,
            int64_t  sh_bounds_stride = 256,
            int64_t  sh_pair_pitch = 0
        ) {
            float2 xy;
            float depth;
            Slang3DGSProj<camera_model, distortion>::fwd_3d(
                false,
                this->mean, this->quat, this->scale, this->opacity,
                cam.R, cam.t, cam.fx, cam.fy, cam.cx, cam.cy,
                cam.dist_coeffs.v,
                cam.width, cam.height,
                &aabb, &sorting_depth, &radius, &xy, &depth, &proj.scale, &proj.opacity
            );
            if (aabb.z > aabb.x && aabb.w > aabb.y) {
                if constexpr (VALUE_BITS == 32) {
                    if constexpr (sh_degree == 0) proj.rgb = SlangHarmonics::sh0_to_color
                        (this->mean, cam.R, cam.t, this->features_dc, (float3*)this->features_sh);
                    else if constexpr (sh_degree == 1) proj.rgb = SlangHarmonics::sh1_to_color
                        (this->mean, cam.R, cam.t, this->features_dc, (float3*)this->features_sh);
                    else if constexpr (sh_degree == 2) proj.rgb = SlangHarmonics::sh2_to_color
                        (this->mean, cam.R, cam.t, this->features_dc, (float3*)this->features_sh);
                    else if constexpr (sh_degree == 3) proj.rgb = SlangHarmonics::sh3_to_color
                        (this->mean, cam.R, cam.t, this->features_dc, (float3*)this->features_sh);
                    else if constexpr (sh_degree == 4) proj.rgb = SlangHarmonics::sh4_to_color
                        (this->mean, cam.R, cam.t, this->features_dc, (float3*)this->features_sh);
                } else if constexpr (VALUE_BITS == 8) {
                    if constexpr (sh_degree == 0) proj.rgb = SlangHarmonics::sh0_to_color_q8
                        (this->mean, cam.R, cam.t, this->features_dc, sh_packed, sh_bounds, sh_base, sh_bounds_stride, sh_pair_pitch);
                    else if constexpr (sh_degree == 1) proj.rgb = SlangHarmonics::sh1_to_color_q8
                        (this->mean, cam.R, cam.t, this->features_dc, sh_packed, sh_bounds, sh_base, sh_bounds_stride, sh_pair_pitch);
                    else if constexpr (sh_degree == 2) proj.rgb = SlangHarmonics::sh2_to_color_q8
                        (this->mean, cam.R, cam.t, this->features_dc, sh_packed, sh_bounds, sh_base, sh_bounds_stride, sh_pair_pitch);
                    else if constexpr (sh_degree == 3) proj.rgb = SlangHarmonics::sh3_to_color_q8
                        (this->mean, cam.R, cam.t, this->features_dc, sh_packed, sh_bounds, sh_base, sh_bounds_stride, sh_pair_pitch);
                    else if constexpr (sh_degree == 4) proj.rgb = SlangHarmonics::sh4_to_color_q8
                        (this->mean, cam.R, cam.t, this->features_dc, sh_packed, sh_bounds, sh_base, sh_bounds_stride, sh_pair_pitch);
                } else if constexpr (VALUE_BITS == 16) {
                    if constexpr (sh_degree == 0) proj.rgb = SlangHarmonics::sh0_to_color_q16
                        (this->mean, cam.R, cam.t, this->features_dc, (uint16_t*)sh_packed, sh_bounds, sh_base, sh_bounds_stride, sh_pair_pitch);
                    else if constexpr (sh_degree == 1) proj.rgb = SlangHarmonics::sh1_to_color_q16
                        (this->mean, cam.R, cam.t, this->features_dc, (uint16_t*)sh_packed, sh_bounds, sh_base, sh_bounds_stride, sh_pair_pitch);
                    else if constexpr (sh_degree == 2) proj.rgb = SlangHarmonics::sh2_to_color_q16
                        (this->mean, cam.R, cam.t, this->features_dc, (uint16_t*)sh_packed, sh_bounds, sh_base, sh_bounds_stride, sh_pair_pitch);
                    else if constexpr (sh_degree == 3) proj.rgb = SlangHarmonics::sh3_to_color_q16
                        (this->mean, cam.R, cam.t, this->features_dc, (uint16_t*)sh_packed, sh_bounds, sh_base, sh_bounds_stride, sh_pair_pitch);
                    else if constexpr (sh_degree == 4) proj.rgb = SlangHarmonics::sh4_to_color_q16
                        (this->mean, cam.R, cam.t, this->features_dc, (uint16_t*)sh_packed, sh_bounds, sh_base, sh_bounds_stride, sh_pair_pitch);
                }
            }
        }

        template<CameraModelType camera_model, CameraDistortionType distortion,
                 bool atomic=true, int VALUE_BITS = 32>
        inline __device__ void project_vjp(
            ProjCameraT<distortion> cam,
            Vanilla3DGUT<sh_degree>::Screen v_proj,
            Vanilla3DGUT<sh_degree>::World& v_world, float3x3 &v_R, float3 &v_t,
            uint8_t* sh_packed = nullptr,
            float2*  sh_bounds = nullptr,
            int64_t  sh_base   = 0,
            int64_t  sh_bounds_stride = 256,
            int64_t  sh_pair_pitch = 0
        ) {
            Slang3DGSProj<camera_model, distortion>::vjp_3d(
                false,
                this->mean, this->quat, this->scale, this->opacity,
                cam.R, cam.t, cam.fx, cam.fy, cam.cx, cam.cy,
                cam.dist_coeffs.v,
                cam.width, cam.height,
                make_float2(0), 0.0f, v_proj.scale, v_proj.opacity,
                &v_world.mean, &v_world.quat, &v_world.scale, &v_world.opacity,
                &v_R, &v_t
            );
            // SH: atomic for global memory, add for local/shared memory
            #define _ARGS_F32 ( \
                this->mean, cam.R, cam.t, this->features_dc, (float3*)this->features_sh, \
                v_proj.rgb, &v_world.features_dc, (float3*)v_world.features_sh, \
                &v_world.mean, &v_R, &v_t \
            );
            #define _ARGS_Q8 ( \
                this->mean, cam.R, cam.t, this->features_dc, sh_packed, sh_bounds, sh_base, sh_bounds_stride, sh_pair_pitch, \
                v_proj.rgb, &v_world.features_dc, (float3*)v_world.features_sh, \
                &v_world.mean, &v_R, &v_t \
            );
            #define _ARGS_Q16 ( \
                this->mean, cam.R, cam.t, this->features_dc, (uint16_t*)sh_packed, sh_bounds, sh_base, sh_bounds_stride, sh_pair_pitch, \
                v_proj.rgb, &v_world.features_dc, (float3*)v_world.features_sh, \
                &v_world.mean, &v_R, &v_t \
            );
            if constexpr (atomic) {
                if constexpr (VALUE_BITS == 32) {
                    if constexpr (sh_degree == 0) SlangHarmonics::sh0_to_color_vjp_atomic _ARGS_F32
                    else if constexpr (sh_degree == 1) SlangHarmonics::sh1_to_color_vjp_atomic _ARGS_F32
                    else if constexpr (sh_degree == 2) SlangHarmonics::sh2_to_color_vjp_atomic _ARGS_F32
                    else if constexpr (sh_degree == 3) SlangHarmonics::sh3_to_color_vjp_atomic _ARGS_F32
                    else if constexpr (sh_degree == 4) SlangHarmonics::sh4_to_color_vjp_atomic _ARGS_F32
                } else if constexpr (VALUE_BITS == 8) {
                    if constexpr (sh_degree == 0) SlangHarmonics::sh0_to_color_q8_vjp_atomic _ARGS_Q8
                    else if constexpr (sh_degree == 1) SlangHarmonics::sh1_to_color_q8_vjp_atomic _ARGS_Q8
                    else if constexpr (sh_degree == 2) SlangHarmonics::sh2_to_color_q8_vjp_atomic _ARGS_Q8
                    else if constexpr (sh_degree == 3) SlangHarmonics::sh3_to_color_q8_vjp_atomic _ARGS_Q8
                    else if constexpr (sh_degree == 4) SlangHarmonics::sh4_to_color_q8_vjp_atomic _ARGS_Q8
                } else if constexpr (VALUE_BITS == 16) {
                    if constexpr (sh_degree == 0) SlangHarmonics::sh0_to_color_q16_vjp_atomic _ARGS_Q16
                    else if constexpr (sh_degree == 1) SlangHarmonics::sh1_to_color_q16_vjp_atomic _ARGS_Q16
                    else if constexpr (sh_degree == 2) SlangHarmonics::sh2_to_color_q16_vjp_atomic _ARGS_Q16
                    else if constexpr (sh_degree == 3) SlangHarmonics::sh3_to_color_q16_vjp_atomic _ARGS_Q16
                    else if constexpr (sh_degree == 4) SlangHarmonics::sh4_to_color_q16_vjp_atomic _ARGS_Q16
                }
            }
            else {
                if constexpr (VALUE_BITS == 32) {
                    if constexpr (sh_degree == 0) SlangHarmonics::sh0_to_color_vjp_inplace _ARGS_F32
                    else if constexpr (sh_degree == 1) SlangHarmonics::sh1_to_color_vjp_inplace _ARGS_F32
                    else if constexpr (sh_degree == 2) SlangHarmonics::sh2_to_color_vjp_inplace _ARGS_F32
                    else if constexpr (sh_degree == 3) SlangHarmonics::sh3_to_color_vjp_inplace _ARGS_F32
                    else if constexpr (sh_degree == 4) SlangHarmonics::sh4_to_color_vjp_inplace _ARGS_F32
                } else if constexpr (VALUE_BITS == 8) {
                    if constexpr (sh_degree == 0) SlangHarmonics::sh0_to_color_q8_vjp_inplace _ARGS_Q8
                    else if constexpr (sh_degree == 1) SlangHarmonics::sh1_to_color_q8_vjp_inplace _ARGS_Q8
                    else if constexpr (sh_degree == 2) SlangHarmonics::sh2_to_color_q8_vjp_inplace _ARGS_Q8
                    else if constexpr (sh_degree == 3) SlangHarmonics::sh3_to_color_q8_vjp_inplace _ARGS_Q8
                    else if constexpr (sh_degree == 4) SlangHarmonics::sh4_to_color_q8_vjp_inplace _ARGS_Q8
                } else if constexpr (VALUE_BITS == 16) {
                    if constexpr (sh_degree == 0) SlangHarmonics::sh0_to_color_q16_vjp_inplace _ARGS_Q16
                    else if constexpr (sh_degree == 1) SlangHarmonics::sh1_to_color_q16_vjp_inplace _ARGS_Q16
                    else if constexpr (sh_degree == 2) SlangHarmonics::sh2_to_color_q16_vjp_inplace _ARGS_Q16
                    else if constexpr (sh_degree == 3) SlangHarmonics::sh3_to_color_q16_vjp_inplace _ARGS_Q16
                    else if constexpr (sh_degree == 4) SlangHarmonics::sh4_to_color_q16_vjp_inplace _ARGS_Q16
                }
            }
            #undef _ARGS_F32
            #undef _ARGS_Q8
            #undef _ARGS_Q16
        }

    };

    struct Screen : public _BasePrimitive3DGUT<sh_degree>::Screen{
        Screen() = default;
        __device__ Screen(const typename _BasePrimitive3DGUT<sh_degree>::Screen& other) {
            _BasePrimitive3DGUT<sh_degree>::Screen::operator=(other);
        }
        __device__ Screen& operator=(const typename _BasePrimitive3DGUT<sh_degree>::Screen& other) {
            _BasePrimitive3DGUT<sh_degree>::Screen::operator=(other);
            return *this;
        }
    };

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
            // iscl_rot = SlangProjectionUtils::compute_3dgut_iscl_rot(wbuffer.quats(wi), sbuffer.scales(si));
            iscl_rot = SlangProjectionUtils::compute_3dgut_iscl_rot(wbuffer.quats(wi), wbuffer.scales(wi));
            opacity = sbuffer.opacities(si);
            rgb = sbuffer.colors(si);
        }

        __device__ __forceinline__ float evaluate_alpha(
            float3 ray_o, float3 ray_d
        ) const {
            return SlangProjectionUtils::evaluate_alpha_3dgs(
                mean, iscl_rot, opacity,
                ray_o, ray_d
            );
        }

        __device__ __forceinline__ RenderOutput evaluate_color(
            float3 ray_o, float3 ray_d
        ) const {
            float3 out_rgb; float out_depth;
            SlangProjectionUtils::evaluate_color_3dgs(
                mean, iscl_rot, opacity, rgb,
                ray_o, ray_d, &out_rgb, &out_depth
            );
            return { out_rgb, out_depth, make_float3(0.0f) };
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
            // scale = sbuffer.scales(si);
            scale = wbuffer.scales(wi);
            iscl_rot = SlangProjectionUtils::compute_3dgut_iscl_rot(quat, scale);
            opacity = sbuffer.opacities(si);
            rgb = sbuffer.colors(si);
        }

        __device__ __forceinline__ void atomicStore(
            WorldBuffer &wbuffer,
            ScreenBuffer &sbuffer,
            int64_t wi, int64_t si
        ) const {
            float4 v_quat; float3 v_scale;
            SlangProjectionUtils::compute_3dgut_iscl_rot_vjp(quat, scale, iscl_rot, &v_quat, &v_scale);
            if (&wbuffer.means(0)) atomicAddFVec(&wbuffer.means(wi), mean);
            if (&wbuffer.quats(0)) atomicAddFVec(&wbuffer.quats(wi), v_quat);
            // if (&sbuffer.scales(0)) atomicAddFVec(&sbuffer.scales(si), v_scale);
            if (&wbuffer.scales(0)) atomicAddFVec(&wbuffer.scales(wi), v_scale);
            if (sbuffer.allocated()) {
                atomicAddFVec(&sbuffer.opacities(si), opacity);
                atomicAddFVec(&sbuffer.colors(si), rgb);
            }
        }

        __device__ __forceinline__ float evaluate_alpha(
            float3 ray_o, float3 ray_d
        ) const {
            return SlangProjectionUtils::evaluate_alpha_3dgs(
                this->mean, this->iscl_rot, this->opacity,
                ray_o, ray_d
            );
        }

        __device__ __forceinline__ void evaluate_alpha_vjp(
            float3 ray_o, float3 ray_d, float v_alpha,
            FragmentBwd &v_frag, float3 &v_ray_o, float3 &v_ray_d
        ) const {
            float3 v_mean; float3x3 v_iscl_rot; float v_opacity;
            float3 v_ray_o_t, v_ray_d_t;
            SlangProjectionUtils::evaluate_alpha_3dgs_vjp(
                this->mean, this->iscl_rot, this->opacity,
                ray_o, ray_d, v_alpha,
                &v_mean, &v_iscl_rot, &v_opacity,
                &v_ray_o_t, &v_ray_d_t
            );
            v_frag.mean += v_mean;
            v_frag.iscl_rot = v_frag.iscl_rot + v_iscl_rot;
            v_frag.opacity += v_opacity;
            v_ray_o += v_ray_o_t;
            v_ray_d += v_ray_d_t;
        }

        __device__ __forceinline__ RenderOutput evaluate_color(
            float3 ray_o, float3 ray_d
        ) const {
            float3 out_rgb; float out_depth;
            SlangProjectionUtils::evaluate_color_3dgs(
                this->mean, this->iscl_rot, this->opacity, this->rgb,
                ray_o, ray_d, &out_rgb, &out_depth
            );
            return { out_rgb, out_depth, make_float3(0.0f) };
        }

        __device__ __forceinline__ void evaluate_color_vjp(
            float3 ray_o, float3 ray_d, RenderOutput v_render,
            FragmentBwd &v_frag, float3 &v_ray_o, float3 &v_ray_d
        ) const {
            float3 v_mean; float3x3 v_iscl_rot; float v_opacity; float3 v_rgb;
            float3 v_ray_o_t, v_ray_d_t;
            SlangProjectionUtils::evaluate_color_3dgs_vjp(
                this->mean, this->iscl_rot, this->opacity, this->rgb,
                ray_o, ray_d,
                v_render.rgb, v_render.depth,
                &v_mean, &v_iscl_rot, &v_opacity, &v_rgb,
                &v_ray_o_t, &v_ray_d_t
            );
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
