#include <filter.hpp>
#include <global.hpp>
#include <memory.hpp>

#include <cuda_runtime.h>

std::vector<bufferRequest> CannyBorderSobel::getRequestedBuffers()
{
	return {
		REQUEST(BUF_8u_C1, STATUS_PUBLIC, 2),
	};
}

bool CannyBorderSobel::init()
{
	if (getBuffer(BUF_8u_C1, MEM_PUBLIC, 0, reinterpret_cast<void**>(&firstBuff), &firstBuffStep) != 0)
		return false;
	if (getBuffer(BUF_8u_C1, MEM_PUBLIC, 1, reinterpret_cast<void**>(&secondBuff), &secondBuffStep) != 0)
		return false;

	NppiSize roi = {g_w, g_h};

	int cannyBufferSize = 0;
	if (nppiFilterCannyBorderGetBufferSize(roi, &cannyBufferSize) != NPP_SUCCESS)
		return false;
	if (cudaMalloc(&cannyBuffer, static_cast<size_t>(cannyBufferSize)) != cudaSuccess)
		return false;
	g_cuda_buf_to_free.push_back(static_cast<void*>(cannyBuffer));

	unsigned int medianBufferSize = 0;
	if (nppiFilterMedianGetBufferSize_8u_C1R_Ctx(roi, medianMask, &medianBufferSize, g_nppStreamCtx) != NPP_SUCCESS)
		return false;
	if (cudaMalloc(&medianBuffer, static_cast<size_t>(medianBufferSize)) != cudaSuccess)
		return false;
	g_cuda_buf_to_free.push_back(static_cast<void*>(medianBuffer));

	return true;
}

void CannyBorderSobel::activate()
{
}

void CannyBorderSobel::process(Npp8u *in, Npp32s inStep)
{
	NppiSize roi = {g_w, g_h};
	NppiSize buffSize = {g_w, g_h};
	NppiPoint offset = {0, 0};

	nppiRGBToGray_8u_AC4C1R_Ctx(in, inStep, firstBuff, firstBuffStep, roi, g_nppStreamCtx);
	nppiFilterMedian_8u_C1R_Ctx(
		firstBuff,
		firstBuffStep,
		secondBuff,
		secondBuffStep,
		roi,
		medianMask,
		medianAnchor,
		medianBuffer,
		g_nppStreamCtx);
	nppiFilterGauss_8u_C1R_Ctx(secondBuff, secondBuffStep, firstBuff, firstBuffStep, roi, NPP_MASK_SIZE_5_X_5, g_nppStreamCtx);
	nppiFilterCannyBorder_8u_C1R_Ctx(
		firstBuff,
		firstBuffStep,
		buffSize,
		offset,
		secondBuff,
		secondBuffStep,
		roi,
		NPP_FILTER_SOBEL,
		NPP_MASK_SIZE_3_X_3,
		20,
		50,
		nppiNormL2,
		NPP_BORDER_REPLICATE,
		cannyBuffer,
		g_nppStreamCtx);

	nppiCopy_8u_C1C4R_Ctx(secondBuff, secondBuffStep, g_rgba_out, g_rgba_outStep, roi, g_nppStreamCtx);
	nppiCopy_8u_C1C4R_Ctx(secondBuff, secondBuffStep, g_rgba_out + 1, g_rgba_outStep, roi, g_nppStreamCtx);
	nppiCopy_8u_C1C4R_Ctx(secondBuff, secondBuffStep, g_rgba_out + 2, g_rgba_outStep, roi, g_nppStreamCtx);
	nppiSet_8u_C4CR_Ctx(255, g_rgba_out + 3, g_rgba_outStep, roi, g_nppStreamCtx);
}

void CannyBorderSobel::deactivate()
{
}

std::string CannyBorderSobel::getName()
{
	return "CannyBorderSobel";
}
