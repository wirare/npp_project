#pragma once

#include "filter.hpp"
#include <vector>

const int g_w = 1024;
const int g_h = 1024;

extern NppStreamContext g_nppStreamCtx;
extern cudaStream_t g_stream;

extern Npp8u*	g_rgba_out;
extern Npp32s   g_rgba_outStep;

extern std::vector<void*> g_cuda_buf_to_free;

extern cudaEvent_t evStart, evStop;
extern float ms;
