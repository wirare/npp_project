#include <filter.hpp>
#include <global.hpp>
#include <memory.hpp>

DEF_FILTER_FN(SobelV)
{
	static Npp8u*	gray = nullptr;
	static Npp32s	grayStep = 0;

	static Npp16s*	sobel16 = nullptr;
	static Npp32s   sobel16Step = 0;

	static Npp8u*	sobel8 = nullptr;
	static Npp32s   sobel8Step = 0;

	if (init)
	{
		bool ret;
		ret = getBuffer(BUF_8u_C1, MEM_PUBLIC, 0, (void**)&gray, &grayStep);
		ret |= getBuffer(BUF_8u_C1, MEM_PUBLIC, 1, (void**)&sobel8, &sobel8Step);
		ret |= getBuffer(BUF_16s_C1, MEM_PUBLIC, 0, (void**)&sobel16, &sobel16Step);
		return ret;
	}

	NppiSize roi = { g_w, g_h };

	nppiRGBToGray_8u_AC4C1R_Ctx(in, inStep, gray, grayStep, roi, g_nppStreamCtx);

	nppiFilterSobelVert_8u16s_C1R_Ctx(gray, grayStep, sobel16, sobel16Step, roi, NPP_MASK_SIZE_3_X_3, g_nppStreamCtx);

	nppiAbs_16s_C1IR_Ctx(sobel16, sobel16Step, roi, g_nppStreamCtx);

	nppiConvert_16s8u_C1R_Ctx(sobel16, sobel16Step, sobel8, sobel8Step, roi, g_nppStreamCtx);

	nppiCopy_8u_C1C4R_Ctx(sobel8, sobel8Step, g_rgba_out, g_rgba_outStep, roi, g_nppStreamCtx);
	nppiCopy_8u_C1C4R_Ctx(sobel8, sobel8Step, g_rgba_out+1, g_rgba_outStep, roi, g_nppStreamCtx);
    nppiCopy_8u_C1C4R_Ctx(sobel8, sobel8Step, g_rgba_out+2, g_rgba_outStep, roi, g_nppStreamCtx);

	nppiSet_8u_C4CR_Ctx(255, g_rgba_out + 3, g_rgba_outStep, roi, g_nppStreamCtx);
	return true;
}

DEF_FILTER_FN(SobelH)
{
	static Npp8u*	gray = nullptr;
	static Npp32s	grayStep = 0;

	static Npp16s*	sobel16 = nullptr;
	static Npp32s   sobel16Step = 0;

	static Npp8u*	sobel8 = nullptr;
	static Npp32s   sobel8Step = 0;

	if (init)
	{
		bool ret;
		ret = getBuffer(BUF_8u_C1, MEM_PUBLIC, 0, (void**)&gray, &grayStep);
		ret |= getBuffer(BUF_8u_C1, MEM_PUBLIC, 1, (void**)&sobel8, &sobel8Step);
		ret |= getBuffer(BUF_16s_C1, MEM_PUBLIC, 0, (void**)&sobel16, &sobel16Step);
		return ret;
	}

	NppiSize roi = { g_w, g_h };

	nppiRGBToGray_8u_AC4C1R_Ctx(in, inStep, gray, grayStep, roi, g_nppStreamCtx);

	nppiFilterSobelHoriz_8u16s_C1R_Ctx(gray, grayStep, sobel16, sobel16Step, roi, NPP_MASK_SIZE_3_X_3, g_nppStreamCtx);

	nppiAbs_16s_C1IR_Ctx(sobel16, sobel16Step, roi, g_nppStreamCtx);

	nppiConvert_16s8u_C1R_Ctx(sobel16, sobel16Step, sobel8, sobel8Step, roi, g_nppStreamCtx);

	nppiCopy_8u_C1C4R_Ctx(sobel8, sobel8Step, g_rgba_out, g_rgba_outStep, roi, g_nppStreamCtx);
	nppiCopy_8u_C1C4R_Ctx(sobel8, sobel8Step, g_rgba_out+1, g_rgba_outStep, roi, g_nppStreamCtx);
    nppiCopy_8u_C1C4R_Ctx(sobel8, sobel8Step, g_rgba_out+2, g_rgba_outStep, roi, g_nppStreamCtx);

	nppiSet_8u_C4CR_Ctx(255, g_rgba_out + 3, g_rgba_outStep, roi, g_nppStreamCtx);
	return true;
}

DEF_FILTER_FN(SobelF)
{
	static Npp8u*	gray = nullptr;
	static Npp32s	grayStep = 0;

	static Npp16s*	sobel16x = nullptr;
	static Npp32s   sobel16xStep = 0;
	static Npp16s*	sobel16y = nullptr;
	static Npp32s   sobel16yStep = 0;

	static Npp8u*	sobel8 = nullptr;
	static Npp32s   sobel8Step = 0;

	if (init)
	{
		bool ret;
		ret = getBuffer(BUF_8u_C1, MEM_PUBLIC, 0, (void**)&gray, &grayStep);
		ret |= getBuffer(BUF_8u_C1, MEM_PUBLIC, 1, (void**)&sobel8, &sobel8Step);
		ret |= getBuffer(BUF_16s_C1, MEM_PUBLIC, 0, (void**)&sobel16x, &sobel16xStep);
		ret |= getBuffer(BUF_16s_C1, MEM_PUBLIC, 1, (void**)&sobel16y, &sobel16yStep);
		return ret;
	}

	NppiSize roi = { g_w, g_h };

	nppiRGBToGray_8u_AC4C1R_Ctx(in, inStep, gray, grayStep, roi, g_nppStreamCtx);

	nppiFilterSobelVert_8u16s_C1R_Ctx(gray, grayStep, sobel16x, sobel16xStep, roi, NPP_MASK_SIZE_3_X_3, g_nppStreamCtx);
	nppiFilterSobelHoriz_8u16s_C1R_Ctx(gray, grayStep, sobel16y, sobel16yStep, roi, NPP_MASK_SIZE_3_X_3, g_nppStreamCtx);

	nppiAbs_16s_C1IR_Ctx(sobel16x, sobel16xStep, roi, g_nppStreamCtx);
	nppiAbs_16s_C1IR_Ctx(sobel16y, sobel16yStep, roi, g_nppStreamCtx);

	nppiAdd_16s_C1IRSfs_Ctx(sobel16y, sobel16yStep, sobel16x, sobel16xStep, roi, 0, g_nppStreamCtx);

	nppiConvert_16s8u_C1R_Ctx(sobel16x, sobel16xStep, sobel8, sobel8Step, roi, g_nppStreamCtx);

	nppiCopy_8u_C1C4R_Ctx(sobel8, sobel8Step, g_rgba_out, g_rgba_outStep, roi, g_nppStreamCtx);
	nppiCopy_8u_C1C4R_Ctx(sobel8, sobel8Step, g_rgba_out+1, g_rgba_outStep, roi, g_nppStreamCtx);
    nppiCopy_8u_C1C4R_Ctx(sobel8, sobel8Step, g_rgba_out+2, g_rgba_outStep, roi, g_nppStreamCtx);

	nppiSet_8u_C4CR_Ctx(255, g_rgba_out + 3, g_rgba_outStep, roi, g_nppStreamCtx);
	return true;
}

