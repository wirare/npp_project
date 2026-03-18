#include <filter.hpp>
#include <global.hpp>

DEF_FILTER_FN(Sharpen)
{
	if (init)
		return false;

	NppiSize roi = {g_w, g_h};

	nppiFilterSharpen_8u_C4R_Ctx(in, inStep, g_rgba_out, g_rgba_outStep, roi, g_nppStreamCtx);
	return true;
}
