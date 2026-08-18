#include <filter.hpp>
#include <global.hpp>

std::vector<bufferRequest> Sharpen::getRequestedBuffers()
{
	return {};
}

bool Sharpen::init()
{
	return true;
}

void Sharpen::activate()
{
}

void Sharpen::process(Npp8u *in, Npp32s inStep)
{
	NppiSize roi = {g_w, g_h};
	nppiFilterSharpen_8u_C4R_Ctx(in, inStep, g_rgba_out, g_rgba_outStep, roi, g_nppStreamCtx);
}

void Sharpen::deactivate()
{
}

std::string Sharpen::getName()
{
	return "Sharpen";
}
