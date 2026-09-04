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

#include "primitives/Primitive.cuh"
#include "primitives/PrimitiveBase3DGS.cuh"
#include "primitives/PrimitiveProjection.cuh"


template<int sh_degree, bool antialiased>
struct _Base3DGS : public _BasePrimitive3DGS<sh_degree> {
    static constexpr RenderOutputType pixelType = RenderOutputType::RGB_D;

    using WorldBuffer = typename _BasePrimitive3DGS<sh_degree>::WorldBuffer;
    using ScreenBuffer = typename _BasePrimitive3DGS<sh_degree>::ScreenBuffer;

#ifdef __CUDACC__

    struct World : public _BasePrimitive3DGS<sh_degree>::World{
        World() = default;
        __device__ World(const typename _BasePrimitive3DGS<sh_degree>::World& other) {
            _BasePrimitive3DGS<sh_degree>::World::operator=(other);
        }
        __device__ World& operator=(const typename _BasePrimitive3DGS<sh_degree>::World& other) {
            _BasePrimitive3DGS<sh_degree>::World::operator=(other);
            return *this;
        }

        // SH value-quantization parameters. When VALUE_BITS == 32 these are
        // unused (defaults pass through) and the legacy fp32 path is taken:
        // SH coefs are read from `this->features_sh` via the existing
        // sh{N}_to_color slang exports.
        // When VALUE_BITS in {8, 16}, callers pass:
        //   sh_packed         : uint8_t/uint16_t* canonical buffer
        //   sh_bounds         : float2* per-block (min, max) table
        //   sh_base           : splat_idx * 3 * num_sh_buffer
        //   sh_bounds_stride  : cells per bound. 256 for per-cell-block, OR
        //                        256 * 3 * num_sh_buffer for per-splat-block
        //                        (FPBO writeback layout). The slang helper
        //                        does bounds[cell / bounds_stride], so the
        //                        same code handles both layouts.
        // The codec read is dispatched via the matching sh{N}_to_color_q{8,16}
        // slang export, which decodes per-coef inside the SH eval loop.
        template<CameraModelType camera_model, CameraDistortionType distortion,
                 int VALUE_BITS = 32>
        inline __device__ void project(
            ProjCameraT<distortion> cam,
            typename _Base3DGS<sh_degree, antialiased>::Screen& screen, float4& aabb, float& sorting_depth, float& radius,
            uint8_t* sh_packed = nullptr,
            float2*  sh_bounds = nullptr,
            int64_t  sh_base   = 0,
            int64_t  sh_bounds_stride = 256,
            int64_t  sh_pair_pitch = 0
        ) {
            Slang3DGSProj<camera_model, distortion>::fwd_2d(
                antialiased,
                this->mean, this->quat, this->scale, this->opacity,
                cam.R, cam.t, cam.fx, cam.fy, cam.cx, cam.cy,
                cam.dist_coeffs.v,
                cam.width, cam.height,
                &aabb, &sorting_depth, &radius,
                &screen.xy, &screen.depth, &screen.conic, &screen.opac
            );
            if (aabb.z > aabb.x && aabb.w > aabb.y) {
                if constexpr (VALUE_BITS == 32) {
                    // screen.rgb = SlangHarmonics::sh_coeffs_to_color(
                    //     sh_degree, this->mean, cam.R, cam.t, this->features_dc, this->features_sh
                    // );
                    if constexpr (sh_degree == 0) screen.rgb = SlangHarmonics::sh0_to_color
                        (this->mean, cam.R, cam.t, this->features_dc, (float3*)this->features_sh);
                    else if constexpr (sh_degree == 1) screen.rgb = SlangHarmonics::sh1_to_color
                        (this->mean, cam.R, cam.t, this->features_dc, (float3*)this->features_sh);
                    else if constexpr (sh_degree == 2) screen.rgb = SlangHarmonics::sh2_to_color
                        (this->mean, cam.R, cam.t, this->features_dc, (float3*)this->features_sh);
                    else if constexpr (sh_degree == 3) screen.rgb = SlangHarmonics::sh3_to_color
                        (this->mean, cam.R, cam.t, this->features_dc, (float3*)this->features_sh);
                    else if constexpr (sh_degree == 4) screen.rgb = SlangHarmonics::sh4_to_color
                        (this->mean, cam.R, cam.t, this->features_dc, (float3*)this->features_sh);
                } else if constexpr (VALUE_BITS == 8) {
                    if constexpr (sh_degree == 0) screen.rgb = SlangHarmonics::sh0_to_color_q8
                        (this->mean, cam.R, cam.t, this->features_dc, sh_packed, sh_bounds, sh_base, sh_bounds_stride, sh_pair_pitch);
                    else if constexpr (sh_degree == 1) screen.rgb = SlangHarmonics::sh1_to_color_q8
                        (this->mean, cam.R, cam.t, this->features_dc, sh_packed, sh_bounds, sh_base, sh_bounds_stride, sh_pair_pitch);
                    else if constexpr (sh_degree == 2) screen.rgb = SlangHarmonics::sh2_to_color_q8
                        (this->mean, cam.R, cam.t, this->features_dc, sh_packed, sh_bounds, sh_base, sh_bounds_stride, sh_pair_pitch);
                    else if constexpr (sh_degree == 3) screen.rgb = SlangHarmonics::sh3_to_color_q8
                        (this->mean, cam.R, cam.t, this->features_dc, sh_packed, sh_bounds, sh_base, sh_bounds_stride, sh_pair_pitch);
                    else if constexpr (sh_degree == 4) screen.rgb = SlangHarmonics::sh4_to_color_q8
                        (this->mean, cam.R, cam.t, this->features_dc, sh_packed, sh_bounds, sh_base, sh_bounds_stride, sh_pair_pitch);
                } else if constexpr (VALUE_BITS == 16) {
                    if constexpr (sh_degree == 0) screen.rgb = SlangHarmonics::sh0_to_color_q16
                        (this->mean, cam.R, cam.t, this->features_dc, (uint16_t*)sh_packed, sh_bounds, sh_base, sh_bounds_stride, sh_pair_pitch);
                    else if constexpr (sh_degree == 1) screen.rgb = SlangHarmonics::sh1_to_color_q16
                        (this->mean, cam.R, cam.t, this->features_dc, (uint16_t*)sh_packed, sh_bounds, sh_base, sh_bounds_stride, sh_pair_pitch);
                    else if constexpr (sh_degree == 2) screen.rgb = SlangHarmonics::sh2_to_color_q16
                        (this->mean, cam.R, cam.t, this->features_dc, (uint16_t*)sh_packed, sh_bounds, sh_base, sh_bounds_stride, sh_pair_pitch);
                    else if constexpr (sh_degree == 3) screen.rgb = SlangHarmonics::sh3_to_color_q16
                        (this->mean, cam.R, cam.t, this->features_dc, (uint16_t*)sh_packed, sh_bounds, sh_base, sh_bounds_stride, sh_pair_pitch);
                    else if constexpr (sh_degree == 4) screen.rgb = SlangHarmonics::sh4_to_color_q16
                        (this->mean, cam.R, cam.t, this->features_dc, (uint16_t*)sh_packed, sh_bounds, sh_base, sh_bounds_stride, sh_pair_pitch);
                }
            }
        }

        // SH value-quant params follow the same pattern as project(): when
        // VALUE_BITS == 32, sh_packed/sh_bounds are unused. The OUTPUT gradient
        // buffer (`v_world.features_sh`) stays fp32 regardless of VALUE_BITS
        // -- gradients always live in fp32 inside engine().grad.features_sh.
        // Only the INPUT SH read (used for v_dir grads) goes through the codec.
        template<CameraModelType camera_model, CameraDistortionType distortion,
                 bool atomic=true, int VALUE_BITS = 32>
        inline __device__ void project_vjp(
            ProjCameraT<distortion> cam,
            typename _Base3DGS<sh_degree, antialiased>::Screen v_screen,
            typename _Base3DGS<sh_degree, antialiased>::World& v_world, float3x3 &v_R, float3 &v_t,
            uint8_t* sh_packed = nullptr,
            float2*  sh_bounds = nullptr,
            int64_t  sh_base   = 0,
            int64_t  sh_bounds_stride = 256,
            int64_t  sh_pair_pitch = 0
        ) {
            Slang3DGSProj<camera_model, distortion>::vjp_2d(
                antialiased,
                this->mean, this->quat, this->scale, this->opacity,
                cam.R, cam.t, cam.fx, cam.fy, cam.cx, cam.cy,
                cam.dist_coeffs.v,
                cam.width, cam.height,
                v_screen.xy, v_screen.depth, v_screen.conic, v_screen.opac,
                &v_world.mean, &v_world.quat, &v_world.scale, &v_world.opacity,
                &v_R, &v_t
            );
            // SH: atomic for global memory, add for local/shared memory
            #define _ARGS_F32 ( \
                this->mean, cam.R, cam.t, this->features_dc, (float3*)this->features_sh, \
                v_screen.rgb, &v_world.features_dc, (float3*)v_world.features_sh, \
                &v_world.mean, &v_R, &v_t \
            );
            #define _ARGS_Q8 ( \
                this->mean, cam.R, cam.t, this->features_dc, sh_packed, sh_bounds, sh_base, sh_bounds_stride, sh_pair_pitch, \
                v_screen.rgb, &v_world.features_dc, (float3*)v_world.features_sh, \
                &v_world.mean, &v_R, &v_t \
            );
            #define _ARGS_Q16 ( \
                this->mean, cam.R, cam.t, this->features_dc, (uint16_t*)sh_packed, sh_bounds, sh_base, sh_bounds_stride, sh_pair_pitch, \
                v_screen.rgb, &v_world.features_dc, (float3*)v_world.features_sh, \
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

    struct Screen : public _BasePrimitive3DGS<sh_degree>::Screen {
        Screen() = default;
        __device__ Screen(const typename _BasePrimitive3DGS<sh_degree>::Screen& other) {
            _BasePrimitive3DGS<sh_degree>::Screen::operator=(other);
        }
        __device__ Screen& operator=(const typename _BasePrimitive3DGS<sh_degree>::Screen& other) {
            _BasePrimitive3DGS<sh_degree>::Screen::operator=(other);
            return *this;
        }
    };

    struct FragmentFwd : public _BasePrimitive3DGS<sh_degree>::Screen {
        FragmentFwd() = default;
        __device__ FragmentFwd(const typename _BasePrimitive3DGS<sh_degree>::Screen& other) {
            _BasePrimitive3DGS<sh_degree>::Screen::operator=(other);
        }
        __device__ FragmentFwd& operator=(const typename _BasePrimitive3DGS<sh_degree>::Screen& other) {
            _BasePrimitive3DGS<sh_degree>::Screen::operator=(other);
            return *this;
        }

        __device__ __forceinline__ void load(
            const WorldBuffer &wbuffer,
            const ScreenBuffer &sbuffer,
            int64_t wi, int64_t si
        ) {
            _BasePrimitive3DGS<sh_degree>::Screen::load(sbuffer, si);
        }

        __device__ __forceinline__ void store(
            WorldBuffer &wbuffer,
            ScreenBuffer &sbuffer,
            int64_t wi, int64_t si
        ) const {
            _BasePrimitive3DGS<sh_degree>::Screen::store(sbuffer, si);
        }

        __device__ __forceinline__ void atomicStore(
            WorldBuffer &wbuffer,
            ScreenBuffer &sbuffer,
            int64_t wi, int64_t si
        ) const {
            _BasePrimitive3DGS<sh_degree>::Screen::atomicStore(sbuffer, si);
        }

        __device__ __forceinline__ float evaluate_alpha(
            float px, float py
        ) const {
            float2 delta = {this->xy.x - px, this->xy.y - py};
            float sigma = 0.5f * (this->conic.x * delta.x * delta.x +
                                    this->conic.z * delta.y * delta.y) +
                            this->conic.y * delta.x * delta.y;
            float vis = __expf(-sigma);
            float alpha = min(0.999f, this->opac * vis);
            return sigma < 0.f ? 0.f : alpha;
        }

        __device__ __forceinline__ void evaluate_alpha_vjp(
            float px, float py, float v_alpha,
            FragmentFwd& v_frag
        ) const {
            float2 delta = {this->xy.x - px, this->xy.y - py};
            float sigma = 0.5f * (this->conic.x * delta.x * delta.x +
                                    this->conic.z * delta.y * delta.y) +
                            this->conic.y * delta.x * delta.y;
            float vis = __expf(-sigma);
            float alpha = min(0.999f, this->opac * vis);

            if (sigma >= 0.f && this->opac * vis <= 0.999f) {
                const float v_sigma = -this->opac * vis * v_alpha;
                v_frag.conic += float3{
                    0.5f * v_sigma * delta.x * delta.x,
                    v_sigma * delta.x * delta.y,
                    0.5f * v_sigma * delta.y * delta.y
                };
                v_frag.xy += float2{
                    v_sigma * (this->conic.x * delta.x + this->conic.y * delta.y),
                    v_sigma * (this->conic.y * delta.x + this->conic.z * delta.y)
                };
                v_frag.opac += vis * v_alpha;
            }
        }

        __device__ __forceinline__ RenderOutput evaluate_color(
            float px, float py
        ) const {
            return { this->rgb, this->depth, make_float3(0.0f) };
        }

        __device__ __forceinline__ void evaluate_color_vjp(
            float px, float py, RenderOutput v_render,
            FragmentFwd& v_frag
        ) const {
            v_frag.rgb += v_render.rgb;
            v_frag.depth += v_render.depth;
        }

    };

    struct FragmentBwd : public FragmentFwd {
        FragmentBwd() = default;
        __device__ FragmentBwd(const typename _BasePrimitive3DGS<sh_degree>::Screen& other) {
            _BasePrimitive3DGS<sh_degree>::Screen::operator=(other);
        }
        __device__ FragmentBwd& operator=(const typename _BasePrimitive3DGS<sh_degree>::Screen& other) {
            _BasePrimitive3DGS<sh_degree>::Screen::operator=(other);
            return *this;
        }

        static __device__ __forceinline__ FragmentBwd zero(const FragmentBwd& bwd) {
            return _BasePrimitive3DGS<sh_degree>::Screen::zero();
        }
    };

#endif  // #ifdef __CUDACC__

};

template<int sh_degree>
using Vanilla3DGS = _Base3DGS<sh_degree, false>;

template<int sh_degree>
using MipSplatting = _Base3DGS<sh_degree, true>;
