#include <cuda_runtime.h>
#include <npp.h>
#include <filter.hpp>
#include <memory.hpp>
#include <cuda_global.hpp>

static __global__ void compute_row_scales_kernel(
    const Npp32f* src,
    int srcStepBytes,
    int width,
    int height,
    float targetMean,
    float eps,
    float* rowScales)
{
    const int y = blockIdx.x;
    if (y >= height)
        return;

    const int tid = threadIdx.x;
    const Npp32f* row = (const Npp32f*)((const unsigned char*)src + (size_t)y * (size_t)srcStepBytes);

    __shared__ float sh[256];

    float local = 0.0f;
    for (int x = tid; x < width; x += blockDim.x)
        local += row[x];

    sh[tid] = local;
    __syncthreads();

    for (int stride = blockDim.x / 2; stride > 0; stride >>= 1)
    {
        if (tid < stride)
            sh[tid] += sh[tid + stride];
        __syncthreads();
    }

    if (tid == 0)
    {
        const float sum = sh[0];
        const float target = targetMean * (float)width;
        rowScales[y] = (sum > eps) ? (target / sum) : 1.0f;
    }
}

static __global__ void apply_row_scales_kernel(
    Npp32f* srcDst,
    int srcDstStepBytes,
    int width,
    int height,
    const float* rowScales)
{
    const int x = blockIdx.x * blockDim.x + threadIdx.x;
    const int y = blockIdx.y;

    if (x >= width || y >= height)
        return;

    Npp32f* row = (Npp32f*)((unsigned char*)srcDst + (size_t)y * (size_t)srcDstStepBytes);
    row[x] *= rowScales[y];
}

DEF_FILTER_FN(RowNormalization)
{
    static Npp8u* gray = nullptr;
    static Npp32s grayStep = 0;

    static Npp32f* convertBuffer = nullptr;
    static Npp32s convertBufferStep = 0;

    static float* rowScales = nullptr;

    NppiSize roi = {g_w, g_h};

    if (init)
    {
		bool ret;
        ret = getBuffer(BUF_8u_C1,  MEM_PUBLIC, 0, (void**)&gray, &grayStep);
        ret |= getBuffer(BUF_32f_C1, MEM_PUBLIC, 0, (void**)&convertBuffer, &convertBufferStep);
		if (ret)
			return true;

        cudaMalloc(&rowScales, (size_t)g_h * sizeof(float));
        g_cuda_buf_to_free.push_back(static_cast<void*>(rowScales));

        return false;
    }

    nppiRGBToGray_8u_AC4C1R_Ctx(
        in, inStep,
        gray, grayStep,
        roi,
        g_nppStreamCtx);

    nppiConvert_8u32f_C1R_Ctx(
        gray, grayStep,
        convertBuffer, convertBufferStep,
        roi,
        g_nppStreamCtx);

    {
        constexpr int threads = 256;

        compute_row_scales_kernel<<<g_h, threads, 0, g_stream>>>(
            convertBuffer,
            convertBufferStep,
            g_w,
            g_h,
            128.0f,
            1e-6f,
            rowScales);

        dim3 block(256, 1, 1);
        dim3 grid((unsigned int)((g_w + block.x - 1) / block.x), (unsigned int)g_h, 1);

        apply_row_scales_kernel<<<grid, block, 0, g_stream>>>(
            convertBuffer,
            convertBufferStep,
            g_w,
            g_h,
            rowScales);
    }

    nppiConvert_32f8u_C1R_Ctx(
        convertBuffer, convertBufferStep,
        gray, grayStep,
        roi,
        NPP_ROUND_NEAREST_TIES_AWAY_FROM_ZERO,
        g_nppStreamCtx);

    nppiCopy_8u_C1C4R_Ctx(gray, grayStep, g_rgba_out,   g_rgba_outStep, roi, g_nppStreamCtx);
    nppiCopy_8u_C1C4R_Ctx(gray, grayStep, g_rgba_out+1, g_rgba_outStep, roi, g_nppStreamCtx);
    nppiCopy_8u_C1C4R_Ctx(gray, grayStep, g_rgba_out+2, g_rgba_outStep, roi, g_nppStreamCtx);

    nppiSet_8u_C4CR_Ctx(255, g_rgba_out + 3, g_rgba_outStep, roi, g_nppStreamCtx);
	return true;
}
