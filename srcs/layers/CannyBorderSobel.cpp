#include <filter.hpp>
#include <global.hpp>
#include <memory.hpp>

DEF_FILTER_FN(CannyBorderSobel)
{
	static Npp8u* firstBuff = nullptr;
	static Npp32s firstBuffStep = 0;

	static Npp8u* secondBuff = nullptr;
	static Npp32s secondBuffStep = 0;

	static Npp8u* cannyBuffer = nullptr;
	static Npp8u* medianBuffer = nullptr;

	static NppiSize medianMask = {3, 3};
	static NppiPoint medianAnchor = {1, 1};

	NppiSize roi = {g_w, g_h};

	if (init)
	{
		bool ret;
		ret = getBuffer(BUF_8u_C1, MEM_PUBLIC, 0, (void**)&firstBuff, &firstBuffStep);
		ret |= getBuffer(BUF_8u_C1, MEM_PUBLIC, 1, (void**)&secondBuff, &secondBuffStep);
		if (ret)
			return true;

		int cannyBufferSize = 0;
		nppiFilterCannyBorderGetBufferSize(roi, &cannyBufferSize);
		cudaMalloc(&cannyBuffer, cannyBufferSize);
		g_cuda_buf_to_free.push_back(static_cast<void*>(cannyBuffer));
	
		unsigned int medianBufferSize = 0;
		nppiFilterMedianGetBufferSize_8u_C1R_Ctx(roi, medianMask, &medianBufferSize, g_nppStreamCtx);
		cudaMalloc(&medianBuffer, medianBufferSize);
		g_cuda_buf_to_free.push_back(static_cast<void*>(medianBuffer));

		return false;
	}

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
	nppiCopy_8u_C1C4R_Ctx(secondBuff, secondBuffStep, g_rgba_out+1, g_rgba_outStep, roi, g_nppStreamCtx);
    nppiCopy_8u_C1C4R_Ctx(secondBuff, secondBuffStep, g_rgba_out+2, g_rgba_outStep, roi, g_nppStreamCtx);

	nppiSet_8u_C4CR_Ctx(255, g_rgba_out + 3, g_rgba_outStep, roi, g_nppStreamCtx);
	return true;
}
