#include "cuda_global.hpp"
#include <filter.hpp>
#include <npp.h>

DEF_FILTER_FN(Empty)
{
	(void)inStep;
	(void)in;

	if (init)
		return false;

	NppiSize roi = {g_w, g_h};

	nppiCopy_8u_AC4R_Ctx(in, inStep, g_rgba_out, g_rgba_outStep, roi, g_nppStreamCtx);
	return true;
}
