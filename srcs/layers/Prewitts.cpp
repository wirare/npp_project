#include <filter.hpp>
#include <global.hpp>
#include <memory.hpp>

DEF_FILTER_FN(PrewittHoriz)
{
	if (init)
		return false;

	NppiSize roi = {g_w, g_h};

	nppiFilterPrewittHoriz_8u_AC4R_Ctx(in, inStep, g_rgba_out, g_rgba_outStep, roi, g_nppStreamCtx);
	return true;
}

DEF_FILTER_FN(PrewittVert)
{
	if (init)
		return false;

	NppiSize roi = {g_w, g_h};

	nppiFilterPrewittVert_8u_AC4R_Ctx(in, inStep, g_rgba_out, g_rgba_outStep, roi, g_nppStreamCtx);
	return true;
}

DEF_FILTER_FN(PrewittFull)
{
	static Npp16s* orig_to_16s = nullptr;
	static Npp32s orig_to_16sStep = 0;

	static Npp16s* prewitt16x = nullptr;
	static Npp32s prewitt16xStep = 0;

	static Npp16s* prewitt16y = nullptr;
	static Npp32s prewitt16yStep = 0;

	if (init)
	{
		bool ret;
		ret = getBuffer(BUF_16s_C4, MEM_PUBLIC, 0, (void **)&orig_to_16s, &orig_to_16sStep);
		ret |= getBuffer(BUF_16s_C4, MEM_PUBLIC, 1, (void **)&prewitt16x, &prewitt16xStep);
		ret |= getBuffer(BUF_16s_C4, MEM_PUBLIC, 2, (void **)&prewitt16y, &prewitt16yStep);
		return ret;
	}

	NppiSize roi = {g_w, g_h};

	nppiConvert_8u16s_C4R_Ctx(in, inStep, orig_to_16s, orig_to_16sStep, roi, g_nppStreamCtx);

	nppiFilterPrewittHoriz_16s_AC4R_Ctx(orig_to_16s, orig_to_16sStep, prewitt16x, prewitt16xStep, roi, g_nppStreamCtx);
	nppiFilterPrewittVert_16s_AC4R_Ctx(orig_to_16s, orig_to_16sStep, prewitt16y, prewitt16yStep, roi, g_nppStreamCtx);

	nppiAbs_16s_AC4IR_Ctx(prewitt16x, prewitt16xStep, roi, g_nppStreamCtx);
	nppiAbs_16s_AC4IR_Ctx(prewitt16y, prewitt16yStep, roi, g_nppStreamCtx);
	nppiAdd_16s_AC4IRSfs_Ctx(prewitt16x, prewitt16xStep, prewitt16y, prewitt16yStep, roi, 0, g_nppStreamCtx);

	nppiConvert_16s8u_AC4R_Ctx(prewitt16y, prewitt16yStep, g_rgba_out, g_rgba_outStep, roi, g_nppStreamCtx);
	nppiSet_8u_C4CR_Ctx(255, g_rgba_out + 3, g_rgba_outStep, roi, g_nppStreamCtx);
	return true;
}

