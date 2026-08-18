#include <filter.hpp>
#include <global.hpp>

std::vector<bufferRequest> Gauss::getRequestedBuffers()
{
	return {};
}

bool Gauss::init()
{
	return true;
}

void Gauss::activate()
{
}

void Gauss::process(Npp8u *in, Npp32s inStep)
{
	NppiSize roi = {g_w, g_h};
	nppiFilterGauss_8u_C4R_Ctx(in, inStep, g_rgba_out, g_rgba_outStep, roi, NPP_MASK_SIZE_3_X_3, g_nppStreamCtx);
}

void Gauss::deactivate()
{
}

std::string Gauss::getName()
{
	return "Gauss";
}
