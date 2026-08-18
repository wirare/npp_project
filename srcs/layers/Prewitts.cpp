#include <filter.hpp>
#include <global.hpp>
#include <memory.hpp>

std::vector<bufferRequest> PrewittHoriz::getRequestedBuffers()
{
	return {};
}

bool PrewittHoriz::init()
{
	return true;
}

void PrewittHoriz::activate()
{
}

void PrewittHoriz::process(Npp8u *in, Npp32s inStep)
{
	NppiSize roi = {g_w, g_h};
	nppiFilterPrewittHoriz_8u_AC4R_Ctx(in, inStep, g_rgba_out, g_rgba_outStep, roi, g_nppStreamCtx);
}

void PrewittHoriz::deactivate()
{
}

std::string PrewittHoriz::getName()
{
	return "PrewittHoriz";
}

std::vector<bufferRequest> PrewittVert::getRequestedBuffers()
{
	return {};
}

bool PrewittVert::init()
{
	return true;
}

void PrewittVert::activate()
{
}

void PrewittVert::process(Npp8u *in, Npp32s inStep)
{
	NppiSize roi = {g_w, g_h};
	nppiFilterPrewittVert_8u_AC4R_Ctx(in, inStep, g_rgba_out, g_rgba_outStep, roi, g_nppStreamCtx);
}

void PrewittVert::deactivate()
{
}

std::string PrewittVert::getName()
{
	return "PrewittVert";
}

std::vector<bufferRequest> PrewittFull::getRequestedBuffers()
{
	return {
		REQUEST(BUF_16s_C4, STATUS_PUBLIC, 3),
	};
}

bool PrewittFull::init()
{
	if (getBuffer(BUF_16s_C4, MEM_PUBLIC, 0, reinterpret_cast<void**>(&origTo16s), &origTo16sStep) != 0)
		return false;
	if (getBuffer(BUF_16s_C4, MEM_PUBLIC, 1, reinterpret_cast<void**>(&prewitt16x), &prewitt16xStep) != 0)
		return false;
	if (getBuffer(BUF_16s_C4, MEM_PUBLIC, 2, reinterpret_cast<void**>(&prewitt16y), &prewitt16yStep) != 0)
		return false;
	return true;
}

void PrewittFull::activate()
{
}

void PrewittFull::process(Npp8u *in, Npp32s inStep)
{
	NppiSize roi = {g_w, g_h};

	nppiConvert_8u16s_C4R_Ctx(in, inStep, origTo16s, origTo16sStep, roi, g_nppStreamCtx);
	nppiFilterPrewittHoriz_16s_AC4R_Ctx(origTo16s, origTo16sStep, prewitt16x, prewitt16xStep, roi, g_nppStreamCtx);
	nppiFilterPrewittVert_16s_AC4R_Ctx(origTo16s, origTo16sStep, prewitt16y, prewitt16yStep, roi, g_nppStreamCtx);
	nppiAbs_16s_AC4IR_Ctx(prewitt16x, prewitt16xStep, roi, g_nppStreamCtx);
	nppiAbs_16s_AC4IR_Ctx(prewitt16y, prewitt16yStep, roi, g_nppStreamCtx);
	nppiAdd_16s_AC4IRSfs_Ctx(prewitt16x, prewitt16xStep, prewitt16y, prewitt16yStep, roi, 0, g_nppStreamCtx);
	nppiConvert_16s8u_AC4R_Ctx(prewitt16y, prewitt16yStep, g_rgba_out, g_rgba_outStep, roi, g_nppStreamCtx);
	nppiSet_8u_C4CR_Ctx(255, g_rgba_out + 3, g_rgba_outStep, roi, g_nppStreamCtx);
}

void PrewittFull::deactivate()
{
}

std::string PrewittFull::getName()
{
	return "PrewittFull";
}
