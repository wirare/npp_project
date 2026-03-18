#include <filter.hpp>
#include <global.hpp>

DEF_FILTER_FN(Gauss)
{
	if (init)
		return false;

	NppiSize roi = {g_w, g_h};

	nppiFilterGauss_8u_C4R_Ctx(in, inStep, g_rgba_out, g_rgba_outStep, roi, NPP_MASK_SIZE_3_X_3, g_nppStreamCtx);
	return true;
}
