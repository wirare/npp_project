#include <filter.hpp>
#include <global.hpp>
#include <memory.hpp>

std::vector<bufferRequest> SobelV::getRequestedBuffers()
{
	return {
		REQUEST(BUF_8u_C1, STATUS_PUBLIC, 2),
		REQUEST(BUF_16s_C1, STATUS_PUBLIC, 1),
	};
}

bool SobelV::init()
{
	if (getBuffer(BUF_8u_C1, MEM_PUBLIC, 0, reinterpret_cast<void**>(&gray), &grayStep) != 0)
		return false;
	if (getBuffer(BUF_8u_C1, MEM_PUBLIC, 1, reinterpret_cast<void**>(&sobel8), &sobel8Step) != 0)
		return false;
	if (getBuffer(BUF_16s_C1, MEM_PUBLIC, 0, reinterpret_cast<void**>(&sobel16), &sobel16Step) != 0)
		return false;
	return true;
}

void SobelV::activate()
{
}

void SobelV::process(Npp8u *in, Npp32s inStep)
{
	NppiSize roi = {g_w, g_h};

	nppiRGBToGray_8u_AC4C1R_Ctx(in, inStep, gray, grayStep, roi, g_nppStreamCtx);
	nppiFilterSobelVert_8u16s_C1R_Ctx(gray, grayStep, sobel16, sobel16Step, roi, NPP_MASK_SIZE_3_X_3, g_nppStreamCtx);
	nppiAbs_16s_C1IR_Ctx(sobel16, sobel16Step, roi, g_nppStreamCtx);
	nppiConvert_16s8u_C1R_Ctx(sobel16, sobel16Step, sobel8, sobel8Step, roi, g_nppStreamCtx);
	nppiCopy_8u_C1C4R_Ctx(sobel8, sobel8Step, g_rgba_out, g_rgba_outStep, roi, g_nppStreamCtx);
	nppiCopy_8u_C1C4R_Ctx(sobel8, sobel8Step, g_rgba_out + 1, g_rgba_outStep, roi, g_nppStreamCtx);
	nppiCopy_8u_C1C4R_Ctx(sobel8, sobel8Step, g_rgba_out + 2, g_rgba_outStep, roi, g_nppStreamCtx);
	nppiSet_8u_C4CR_Ctx(255, g_rgba_out + 3, g_rgba_outStep, roi, g_nppStreamCtx);
}

void SobelV::deactivate()
{
}

std::string SobelV::getName()
{
	return "SobelV";
}

std::vector<bufferRequest> SobelH::getRequestedBuffers()
{
	return {
		REQUEST(BUF_8u_C1, STATUS_PUBLIC, 2),
		REQUEST(BUF_16s_C1, STATUS_PUBLIC, 1),
	};
}

bool SobelH::init()
{
	if (getBuffer(BUF_8u_C1, MEM_PUBLIC, 0, reinterpret_cast<void**>(&gray), &grayStep) != 0)
		return false;
	if (getBuffer(BUF_8u_C1, MEM_PUBLIC, 1, reinterpret_cast<void**>(&sobel8), &sobel8Step) != 0)
		return false;
	if (getBuffer(BUF_16s_C1, MEM_PUBLIC, 0, reinterpret_cast<void**>(&sobel16), &sobel16Step) != 0)
		return false;
	return true;
}

void SobelH::activate()
{
}

void SobelH::process(Npp8u *in, Npp32s inStep)
{
	NppiSize roi = {g_w, g_h};

	nppiRGBToGray_8u_AC4C1R_Ctx(in, inStep, gray, grayStep, roi, g_nppStreamCtx);
	nppiFilterSobelHoriz_8u16s_C1R_Ctx(gray, grayStep, sobel16, sobel16Step, roi, NPP_MASK_SIZE_3_X_3, g_nppStreamCtx);
	nppiAbs_16s_C1IR_Ctx(sobel16, sobel16Step, roi, g_nppStreamCtx);
	nppiConvert_16s8u_C1R_Ctx(sobel16, sobel16Step, sobel8, sobel8Step, roi, g_nppStreamCtx);
	nppiCopy_8u_C1C4R_Ctx(sobel8, sobel8Step, g_rgba_out, g_rgba_outStep, roi, g_nppStreamCtx);
	nppiCopy_8u_C1C4R_Ctx(sobel8, sobel8Step, g_rgba_out + 1, g_rgba_outStep, roi, g_nppStreamCtx);
	nppiCopy_8u_C1C4R_Ctx(sobel8, sobel8Step, g_rgba_out + 2, g_rgba_outStep, roi, g_nppStreamCtx);
	nppiSet_8u_C4CR_Ctx(255, g_rgba_out + 3, g_rgba_outStep, roi, g_nppStreamCtx);
}

void SobelH::deactivate()
{
}

std::string SobelH::getName()
{
	return "SobelH";
}

std::vector<bufferRequest> SobelF::getRequestedBuffers()
{
	return {
		REQUEST(BUF_8u_C1, STATUS_PUBLIC, 2),
		REQUEST(BUF_16s_C1, STATUS_PUBLIC, 2),
	};
}

bool SobelF::init()
{
	if (getBuffer(BUF_8u_C1, MEM_PUBLIC, 0, reinterpret_cast<void**>(&gray), &grayStep) != 0)
		return false;
	if (getBuffer(BUF_8u_C1, MEM_PUBLIC, 1, reinterpret_cast<void**>(&sobel8), &sobel8Step) != 0)
		return false;
	if (getBuffer(BUF_16s_C1, MEM_PUBLIC, 0, reinterpret_cast<void**>(&sobel16x), &sobel16xStep) != 0)
		return false;
	if (getBuffer(BUF_16s_C1, MEM_PUBLIC, 1, reinterpret_cast<void**>(&sobel16y), &sobel16yStep) != 0)
		return false;
	return true;
}

void SobelF::activate()
{
}

void SobelF::process(Npp8u *in, Npp32s inStep)
{
	NppiSize roi = {g_w, g_h};

	nppiRGBToGray_8u_AC4C1R_Ctx(in, inStep, gray, grayStep, roi, g_nppStreamCtx);
	nppiFilterSobelVert_8u16s_C1R_Ctx(gray, grayStep, sobel16x, sobel16xStep, roi, NPP_MASK_SIZE_3_X_3, g_nppStreamCtx);
	nppiFilterSobelHoriz_8u16s_C1R_Ctx(gray, grayStep, sobel16y, sobel16yStep, roi, NPP_MASK_SIZE_3_X_3, g_nppStreamCtx);
	nppiAbs_16s_C1IR_Ctx(sobel16x, sobel16xStep, roi, g_nppStreamCtx);
	nppiAbs_16s_C1IR_Ctx(sobel16y, sobel16yStep, roi, g_nppStreamCtx);
	nppiAdd_16s_C1IRSfs_Ctx(sobel16y, sobel16yStep, sobel16x, sobel16xStep, roi, 0, g_nppStreamCtx);
	nppiConvert_16s8u_C1R_Ctx(sobel16x, sobel16xStep, sobel8, sobel8Step, roi, g_nppStreamCtx);
	nppiCopy_8u_C1C4R_Ctx(sobel8, sobel8Step, g_rgba_out, g_rgba_outStep, roi, g_nppStreamCtx);
	nppiCopy_8u_C1C4R_Ctx(sobel8, sobel8Step, g_rgba_out + 1, g_rgba_outStep, roi, g_nppStreamCtx);
	nppiCopy_8u_C1C4R_Ctx(sobel8, sobel8Step, g_rgba_out + 2, g_rgba_outStep, roi, g_nppStreamCtx);
	nppiSet_8u_C4CR_Ctx(255, g_rgba_out + 3, g_rgba_outStep, roi, g_nppStreamCtx);
}

void SobelF::deactivate()
{
}

std::string SobelF::getName()
{
	return "SobelF";
}
