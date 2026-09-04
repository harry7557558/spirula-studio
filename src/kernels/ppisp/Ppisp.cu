// Ppisp.cu -- PPISP (per-pixel image signal processing) forward, backward
// and regularization.
//
// Part of the PixelWise family -- see PixelWiseCommon.cuh.

#include "kernels/pixelwise/PixelWiseCommon.cuh"

// ================
// PPISP
// ================

static constexpr int kNumPPISPParams = 36;
static constexpr int kNumPPISPParamsRQS = 39;
static constexpr int kNumPPISPParamsNoCRF = 24;
static constexpr int kNumPPISPParamsNoCRFNoVig = 9;

template<PpispParamLayout layout>
static constexpr int kPPISPLayoutParams =
    (layout == PpispParamLayout::Original) ? kNumPPISPParams :
    (layout == PpispParamLayout::RQS)      ? kNumPPISPParamsRQS :
    (layout == PpispParamLayout::NoCRF)    ? kNumPPISPParamsNoCRF :
                                             kNumPPISPParamsNoCRFNoVig;

template<PpispParamLayout layout>
static constexpr int kPPISPLayoutRawLosses =
    (layout == PpispParamLayout::Original) ? (int)RawPPISPRegLossIndex::length :
    (layout == PpispParamLayout::RQS)      ? (int)RawPPISPRegLossIndexRQS::length :
    (layout == PpispParamLayout::NoCRF)    ? (int)RawPPISPRegLossIndexNoCRF::length :
                                             (int)RawPPISPRegLossIndexNoCRFNoVig::length;

// Turns the runtime layout into a compile-time one for the kernel launch, so
// each launcher below writes its <<<>>> once instead of per layout.
template<class F>
static void _with_ppisp_layout(PpispParamLayout layout, F&& f) {
    switch (layout) {
        case PpispParamLayout::Original:
            f(std::integral_constant<PpispParamLayout, PpispParamLayout::Original>{});
            break;
        case PpispParamLayout::RQS:
            f(std::integral_constant<PpispParamLayout, PpispParamLayout::RQS>{});
            break;
        case PpispParamLayout::NoCRF:
            f(std::integral_constant<PpispParamLayout, PpispParamLayout::NoCRF>{});
            break;
        case PpispParamLayout::NoCRFNoVig:
            f(std::integral_constant<PpispParamLayout, PpispParamLayout::NoCRFNoVig>{});
            break;
    }
}

template<PpispParamLayout layout>
__global__ void ppisp_forward_kernel(
    const TensorView<float, 4> in_image,  // [B, H, W, C]
    const float* __restrict__ ppisp_params,  // [N_cam or B, PPISP_NUM_PARAMS]
    const float4 *__restrict__ intrins,  // [B, 4]
    const float actual_image_width,
    const float actual_image_height,
    const int* __restrict__ cam_indices,  // [B], or nullptr -> identity
    const bool clamp_output,
    TensorView<float, 4> out_image  // [B, H, W, C]
) {
    unsigned gid = blockIdx.x * blockDim.x + threadIdx.x;
    unsigned bid = blockIdx.y * blockDim.y + threadIdx.y;
    unsigned B = in_image.shape[0], H = in_image.shape[1], W = in_image.shape[2];
    if (bid >= B || gid >= H*W)
        return;
    unsigned y = gid / W;
    unsigned x = gid % W;

    int p_id = cam_indices ? cam_indices[bid] : (int)bid;

    static constexpr int kNumParams = kPPISPLayoutParams<layout>;
    FixedArray<float, kNumParams> params;
    #pragma unroll
    for (int i = 0; i < kNumParams; i++) {
        params[i] = ppisp_params[p_id * kNumParams + i];
    }

    float3 pixel = in_image.load3(bid, y, x);
    const float2 pix_coord = make_float2((float)x, (float)y);
    const float2 image_center = make_float2(intrins[bid].z, intrins[bid].w);
    const float2 img_size = make_float2(actual_image_width, actual_image_height);

    float3 out_pixel;
    if constexpr (layout == PpispParamLayout::Original)
        out_pixel = SlangPPISP::apply_ppisp(
            pixel, pix_coord, image_center, img_size, params);
    else if constexpr (layout == PpispParamLayout::RQS)
        out_pixel = SlangPPISP::apply_ppisp_rqs(
            pixel, pix_coord, image_center, img_size, params);
    else if constexpr (layout == PpispParamLayout::NoCRF)
        out_pixel = SlangPPISP::apply_ppisp_no_crf(
            pixel, pix_coord, image_center, img_size, params, clamp_output);
    else
        out_pixel = SlangPPISP::apply_ppisp_no_crf_no_vig(
            pixel, pix_coord, image_center, img_size, params, clamp_output);

    out_image.store3(bid, y, x, out_pixel);
}

/*[AutoHeaderGeneratorExport]*/
void ppisp_forward(
    DeviceTensor3D<float3> in_image,    // [B, H, W, 3]
    TorchTensorView ppisp_params,       // [N_cam or B, PPISP_NUM_PARAMS]
    TorchTensorView intrins,            // [B, 4]
    const float actual_image_width,
    const float actual_image_height,
    std::string param_type,
    TorchTensorView cam_indices,        // [B] int32, or null -> identity (ppisp_params is [B,P])
    DeviceTensor3D<float3> out_image    // [B, H, W, 3]
) {
    long b = in_image.size<0>(), h = in_image.size<1>(), w = in_image.size<2>();
    const int* cam_idx_ptr = (std::get<0>(cam_indices) == 0) ?
        nullptr : (const int*)std::get<0>(cam_indices);
    const PpispParamSpec spec = ppisp_param_spec(param_type);
    _with_ppisp_layout(spec.layout, [&](auto L) {
        ppisp_forward_kernel<L.value><<<_LAUNCH_ARGS_2D(h*w, b, 256, 1)>>>(
            _dt3d_to_tv4<float>(in_image),
            (float*)std::get<0>(ppisp_params),
            (float4*)std::get<0>(intrins),
            actual_image_width,
            actual_image_height,
            cam_idx_ptr,
            spec.clamp_output,
            _dt3d_to_tv4<float>(out_image)
        );
    });
    CHECK_DEVICE_ERROR(cudaGetLastError());
}

template<PpispParamLayout layout>
__global__ void ppisp_backward_kernel(
    const TensorView<float, 4> in_image,  // [B, H, W, C]
    const float* __restrict__ ppisp_params,  // [N_cam or B, PPISP_NUM_PARAMS]
    const float4 *__restrict__ intrins,  // [B, 4]
    const float actual_image_width,
    const float actual_image_height,
    const int* __restrict__ cam_indices,  // [B], or nullptr -> identity
    const bool clamp_output,
    const TensorView<float, 4> v_out_image,  // [B, H, W, C]
    TensorView<float, 4> v_in_image,  // [B, H, W, C]
    float* __restrict__ v_ppisp_params  // [N_cam or B, PPISP_NUM_PARAMS]
) {
    unsigned gid = blockIdx.x * blockDim.x + threadIdx.x;
    unsigned bid = blockIdx.y * blockDim.y + threadIdx.y;
    unsigned B = in_image.shape[0], H = in_image.shape[1], W = in_image.shape[2];
    bool inside = (bid < B) && (gid < H*W);
    unsigned y = inside ? gid / W : 0u;
    unsigned x = inside ? gid % W : 0u;

    int p_id = (bid < B) ? (cam_indices ? cam_indices[bid] : (int)bid) : 0;

    static constexpr int kNumParams = kPPISPLayoutParams<layout>;
#if 0
    FixedArray<float, kNumParams> params;
    #pragma unroll
    for (int i = 0; i < kNumParams; i++) {
        params[i] = ppisp_params[p_id * kNumParams + i];
    }
#else
    __shared__ float params_shared[kNumParams];
    if (threadIdx.x < kNumParams) {  // assume blockDim.x >= kNumParams
        float value = ppisp_params[p_id * kNumParams + threadIdx.x];
        params_shared[threadIdx.x] = value;
    }
    __syncthreads();
    FixedArray<float, kNumParams> params;
    #pragma unroll
    for (int i = 0; i < kNumParams; i++) {
        params[i] = params_shared[i];
        // int j = (i + threadIdx.x) % kNumParams;
        // params[j] = params_shared[j];
    }
#endif

    float3 v_pixel = make_float3(0.0f, 0.0f, 0.0f);
    FixedArray<float, kNumParams> v_params;
    #pragma unroll
    for (int i = 0; i < kNumParams; i++)
        v_params[i] = 0.0f;

    if (inside) {
        float3 v_out_pixel = v_out_image.load3(bid, y, x);
        // Both gradients are linear in the incoming one, so a zero one -- a
        // masked pixel, mostly -- has no transform to differentiate.
        if (v_out_pixel.x != 0.0f || v_out_pixel.y != 0.0f ||
            v_out_pixel.z != 0.0f) {
            float3 pixel = in_image.load3(bid, y, x);
            const float2 pix_coord = make_float2((float)x, (float)y);
            const float2 image_center = make_float2(intrins[bid].z, intrins[bid].w);
            const float2 img_size =
                make_float2(actual_image_width, actual_image_height);
            if constexpr (layout == PpispParamLayout::Original)
                SlangPPISP::apply_ppisp_vjp(
                    pixel, pix_coord, image_center, img_size, params,
                    v_out_pixel, &v_pixel, &v_params);
            else if constexpr (layout == PpispParamLayout::RQS)
                SlangPPISP::apply_ppisp_rqs_vjp(
                    pixel, pix_coord, image_center, img_size, params,
                    v_out_pixel, &v_pixel, &v_params);
            else if constexpr (layout == PpispParamLayout::NoCRF)
                SlangPPISP::apply_ppisp_no_crf_vjp(
                    pixel, pix_coord, image_center, img_size, params,
                    clamp_output, v_out_pixel, &v_pixel, &v_params);
            else
                SlangPPISP::apply_ppisp_no_crf_no_vig_vjp(
                    pixel, pix_coord, image_center, img_size, params,
                    clamp_output, v_out_pixel, &v_pixel, &v_params);
        }

        v_in_image.store3(bid, y, x, v_pixel);
    }

    auto block = cg::this_thread_block();
    cg::thread_block_tile<WARP_SIZE> warp = cg::tiled_partition<WARP_SIZE>(block);
    #pragma unroll
    for (int i = 0; i < kNumParams; i++) {
        float param = isfinite(v_params[i]) ? v_params[i] : 0.0f;
        param = cg::reduce(warp, param, cg::plus<float>());
        if (threadIdx.x % WARP_SIZE == 0 && param != 0.0f)
            atomicAdd(&v_ppisp_params[p_id * kNumParams + i], param);
    }
}

/*[AutoHeaderGeneratorExport]*/
void ppisp_backward(
    DeviceTensor3D<float3> in_image,    // [B, H, W, 3]
    TorchTensorView ppisp_params,       // [N_cam or B, PPISP_NUM_PARAMS]
    TorchTensorView intrins,            // [B, 4]
    const float actual_image_width,
    const float actual_image_height,
    DeviceTensor3D<float3> v_out_image, // [B, H, W, 3]
    std::string param_type,
    TorchTensorView cam_indices,        // [B] int32, or null -> identity
    DeviceTensor3D<float3> v_in_image,  // [B, H, W, 3]
    TorchTensorView v_ppisp_params      // [N_cam or B, PPISP_NUM_PARAMS] (must be pre-zeroed)
) {
    long b = in_image.size<0>(), h = in_image.size<1>(), w = in_image.size<2>();
    const int* cam_idx_ptr = (std::get<0>(cam_indices) == 0) ?
        nullptr : (const int*)std::get<0>(cam_indices);
    const PpispParamSpec spec = ppisp_param_spec(param_type);
    _with_ppisp_layout(spec.layout, [&](auto L) {
        ppisp_backward_kernel<L.value><<<_LAUNCH_ARGS_2D(h*w, b, 64, 1)>>>(
            _dt3d_to_tv4<float>(in_image),
            (float*)std::get<0>(ppisp_params),
            (float4*)std::get<0>(intrins),
            actual_image_width,
            actual_image_height,
            cam_idx_ptr,
            spec.clamp_output,
            _dt3d_to_tv4<float>(v_out_image),
            _dt3d_to_tv4<float>(v_in_image),
            (float*)std::get<0>(v_ppisp_params)
        );
    });
    CHECK_DEVICE_ERROR(cudaGetLastError());
}

template<PpispParamLayout layout>
__global__ void compute_raw_ppisp_regularization_forward_kernel(
    int B,  // number of images
    const float* __restrict__ ppisp_params,  // [B, PPISP_NUM_PARAMS]
    float* __restrict__ raw_losses  // [B+1, RawPPISPRegLossIndex::length]
) {
    unsigned bid = blockIdx.x * blockDim.x + threadIdx.x;
    bool inside = (bid < (unsigned)B);

    static constexpr int kNumParams = kPPISPLayoutParams<layout>;
    static constexpr int kNumRawLosses = kPPISPLayoutRawLosses<layout>;

    FixedArray<float, kNumRawLosses> losses;
    if (inside) {
        FixedArray<float, kNumParams> params;
        #pragma unroll
        for (int i = 0; i < kNumParams; i++) {
            params[i] = ppisp_params[bid * kNumParams + i];
        }

        if constexpr (layout == PpispParamLayout::Original)
            SlangPPISP::compute_raw_ppisp_regularization_loss(params, &losses);
        else if constexpr (layout == PpispParamLayout::RQS)
            SlangPPISP::compute_raw_ppisp_rqs_regularization_loss(params, &losses);
        else if constexpr (layout == PpispParamLayout::NoCRF)
            SlangPPISP::compute_raw_ppisp_no_crf_regularization_loss(params, &losses);
        else
            SlangPPISP::compute_raw_ppisp_no_crf_no_vig_regularization_loss(
                params, &losses);
    }

    auto block = cg::this_thread_block();
    cg::thread_block_tile<WARP_SIZE> warp = cg::tiled_partition<WARP_SIZE>(block);
    #pragma unroll
    for (int i = 0; i < kNumRawLosses; i++) {
        float loss = (inside && isfinite(losses[i])) ? losses[i] : 0.0f;
        if (inside)
            raw_losses[bid*kNumRawLosses + i] = loss;
        loss = cg::reduce(warp, loss, cg::plus<float>());
        if (threadIdx.x % WARP_SIZE == 0 && loss != 0.0f)
            atomicAdd(&raw_losses[B*kNumRawLosses + i], loss);
    }
}

template<PpispParamLayout layout>
__global__ void compute_ppisp_regularization_forward_kernel(
    int num_train_images,
    const float* __restrict__ raw_losses_buffer,  // [RawPPISPRegLossIndex::length]
    FixedArray<float, (int)PPISPRegLossIndex::length> loss_weights,  // [PPISPRegLossIndex::length]
    float* __restrict__ losses_buffer  // [PPISPRegLossIndex::length]
) {
    static constexpr int kNumRawLosses = kPPISPLayoutRawLosses<layout>;

    FixedArray<float, kNumRawLosses> raw_losses;
    #pragma unroll
    for (int i = 0; i < kNumRawLosses; i++) {
        raw_losses[i] = raw_losses_buffer[i];
    }

    FixedArray<float, (int)PPISPRegLossIndex::length> losses;

    if constexpr (layout == PpispParamLayout::Original)
        SlangPPISP::compute_ppisp_regularization_loss(
            raw_losses, num_train_images, loss_weights, &losses);
    else if constexpr (layout == PpispParamLayout::RQS)
        SlangPPISP::compute_ppisp_rqs_regularization_loss(
            raw_losses, num_train_images, loss_weights, &losses);
    else if constexpr (layout == PpispParamLayout::NoCRF)
        SlangPPISP::compute_ppisp_no_crf_regularization_loss(
            raw_losses, num_train_images, loss_weights, &losses);
    else
        SlangPPISP::compute_ppisp_no_crf_no_vig_regularization_loss(
            raw_losses, num_train_images, loss_weights, &losses);

    #pragma unroll
    for (int i = 0; i < (int)PPISPRegLossIndex::length; i++) {
        losses_buffer[i] = losses[i];
    }
}

/*[AutoHeaderGeneratorExport]*/
void compute_ppsip_regularization_forward(
    TorchTensorView ppisp_params,       // [B, PPISP_NUM_PARAMS]
    const std::array<float, (int)PPISPRegLossIndex::length> loss_weights_0,
    std::string param_type,
    TorchTensorView losses,             // [PPISPRegLossIndex::length] (must be pre-zeroed)
    TorchTensorView raw_losses          // [B+1, RawPPISPRegLossIndex::length] (must be pre-zeroed)
) {
    FixedArray<float, (int)PPISPRegLossIndex::length> loss_weights =
        *reinterpret_cast<const FixedArray<float, (int)PPISPRegLossIndex::length>*>(loss_weights_0.data());

    long B = std::get<2>(ppisp_params)[0];
    const PpispParamSpec spec = ppisp_param_spec(param_type);

    _with_ppisp_layout(spec.layout, [&](auto L) {
        compute_raw_ppisp_regularization_forward_kernel<L.value>
        <<<_LAUNCH_ARGS_1D(B, WARP_SIZE)>>>(
            B,
            (float*)std::get<0>(ppisp_params),
            (float*)std::get<0>(raw_losses)
        );
        CHECK_DEVICE_ERROR(cudaGetLastError());

        compute_ppisp_regularization_forward_kernel<L.value>
        <<<1, 1>>>(
            B,
            (float*)std::get<0>(raw_losses) + B * spec.num_raw_losses,
            loss_weights,
            (float*)std::get<0>(losses)
        );
        CHECK_DEVICE_ERROR(cudaGetLastError());
    });
}

template<PpispParamLayout layout>
__global__ void compute_raw_ppisp_regularization_backward_kernel(
    int B,  // number of images
    const float* __restrict__ ppisp_params,  // [B, PPISP_NUM_PARAMS]
    const float* __restrict__ v_raw_losses,  // [RawPPISPRegLossIndex::length]
    float* __restrict__ v_ppisp_params  // [B, PPISP_NUM_PARAMS]
) {
    unsigned bid = blockIdx.x * blockDim.x + threadIdx.x;
    if (bid >= B)
        return;

    static constexpr int kNumParams = kPPISPLayoutParams<layout>;
    static constexpr int kNumRawLosses = kPPISPLayoutRawLosses<layout>;

    FixedArray<float, kNumParams> params;
    #pragma unroll
    for (int i = 0; i < kNumParams; i++) {
        params[i] = ppisp_params[bid * kNumParams + i];
    }

    FixedArray<float, kNumRawLosses> v_losses;
    #pragma unroll
    for (int i = 0; i < kNumRawLosses; i++) {
        v_losses[i] = v_raw_losses[i];
    }

    FixedArray<float, kNumParams> v_params;
    if constexpr (layout == PpispParamLayout::Original)
        SlangPPISP::compute_raw_ppisp_regularization_loss_vjp(
            params, v_losses, &v_params);
    else if constexpr (layout == PpispParamLayout::RQS)
        SlangPPISP::compute_raw_ppisp_rqs_regularization_loss_vjp(
            params, v_losses, &v_params);
    else if constexpr (layout == PpispParamLayout::NoCRF)
        SlangPPISP::compute_raw_ppisp_no_crf_regularization_loss_vjp(
            params, v_losses, &v_params);
    else
        SlangPPISP::compute_raw_ppisp_no_crf_no_vig_regularization_loss_vjp(
            params, v_losses, &v_params);

    #pragma unroll
    for (int i = 0; i < kNumParams; i++) {
        float param = isfinite(v_params[i]) ? v_params[i] : 0.0f;
        v_ppisp_params[bid * kNumParams + i] = param;
    }
}

template<PpispParamLayout layout>
__global__ void compute_ppisp_regularization_backward_kernel(
    int num_train_images,
    const float* __restrict__ raw_losses_buffer,  // [RawPPISPRegLossIndex::length]
    FixedArray<float, (int)PPISPRegLossIndex::length> loss_weights,  // [PPISPRegLossIndex::length]
    const float* __restrict__ v_losses_buffer,  // [PPISPRegLossIndex::length]
    float* __restrict__ v_raw_losses_buffer  // [RawPPISPRegLossIndex::length]
) {
    static constexpr int kNumRawLosses = kPPISPLayoutRawLosses<layout>;

    FixedArray<float, kNumRawLosses> raw_losses;
    #pragma unroll
    for (int i = 0; i < kNumRawLosses; i++) {
        raw_losses[i] = raw_losses_buffer[i];
    }

    FixedArray<float, (int)PPISPRegLossIndex::length> v_losses;
    #pragma unroll
    for (int i = 0; i < (int)PPISPRegLossIndex::length; i++) {
        v_losses[i] = v_losses_buffer[i];
    }

    FixedArray<float, kNumRawLosses> v_raw_losses;
    if constexpr (layout == PpispParamLayout::Original)
        SlangPPISP::compute_ppisp_regularization_loss_vjp(
            raw_losses, num_train_images, loss_weights, v_losses, &v_raw_losses);
    else if constexpr (layout == PpispParamLayout::RQS)
        SlangPPISP::compute_ppisp_rqs_regularization_loss_vjp(
            raw_losses, num_train_images, loss_weights, v_losses, &v_raw_losses);
    else if constexpr (layout == PpispParamLayout::NoCRF)
        SlangPPISP::compute_ppisp_no_crf_regularization_loss_vjp(
            raw_losses, num_train_images, loss_weights, v_losses, &v_raw_losses);
    else
        SlangPPISP::compute_ppisp_no_crf_no_vig_regularization_loss_vjp(
            raw_losses, num_train_images, loss_weights, v_losses, &v_raw_losses);

    #pragma unroll
    for (int i = 0; i < kNumRawLosses; i++) {
        v_raw_losses_buffer[i] = v_raw_losses[i];
    }
}

/*[AutoHeaderGeneratorExport]*/
void compute_ppsip_regularization_backward(
    TorchTensorView ppisp_params,       // [B, PPISP_NUM_PARAMS]
    const std::array<float, (int)PPISPRegLossIndex::length> loss_weights_0,
    TorchTensorView raw_losses,         // [B+1, RawPPISPRegLossIndex::length]
    TorchTensorView v_losses,           // [PPISPRegLossIndex::length]
    std::string param_type,
    TorchTensorView v_ppisp_params      // [B, PPISP_NUM_PARAMS] (must be pre-zeroed)
) {
    FixedArray<float, (int)PPISPRegLossIndex::length> loss_weights =
        *reinterpret_cast<const FixedArray<float, (int)PPISPRegLossIndex::length>*>(loss_weights_0.data());

    long B = std::get<2>(ppisp_params)[0];
    const PpispParamSpec spec = ppisp_param_spec(param_type);

    // v_raw_losses is a small scratch buffer
    float* v_raw_losses = DevicePool::global().acquire<float>(
        PoolSlot::PpispVRawLosses, spec.num_raw_losses);
    cudaMemset(v_raw_losses, 0, spec.num_raw_losses * sizeof(float));

    _with_ppisp_layout(spec.layout, [&](auto L) {
        compute_ppisp_regularization_backward_kernel<L.value>
        <<<1, 1>>>(
            B,
            (float*)std::get<0>(raw_losses) + B * spec.num_raw_losses,
            loss_weights,
            (float*)std::get<0>(v_losses),
            v_raw_losses
        );
        CHECK_DEVICE_ERROR(cudaGetLastError());

        compute_raw_ppisp_regularization_backward_kernel<L.value>
        <<<_LAUNCH_ARGS_1D(B, WARP_SIZE)>>>(
            B,
            (float*)std::get<0>(ppisp_params),
            v_raw_losses,
            (float*)std::get<0>(v_ppisp_params)
        );
        CHECK_DEVICE_ERROR(cudaGetLastError());
    });
}
