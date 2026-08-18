#include <filter.hpp>
#include <cuda_global.hpp>

std::vector<bufferRequest> Empty::getRequestedBuffers()
{
	return {};
}

bool Empty::init()
{
	return true;
}

void Empty::activate()
{
}

void Empty::process(Npp8u *in, Npp32s inStep)
{
	NppiSize roi = {g_w, g_h};
	nppiCopy_8u_C4R_Ctx(in, inStep, g_rgba_out, g_rgba_outStep, roi, g_nppStreamCtx);
}

void Empty::deactivate()
{
}

std::string Empty::getName()
{
	return "Empty";
}
