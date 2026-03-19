#pragma once

#include "filter.hpp"
#include "glib.h"
#include "gst/app/gstappsrc.h"
#include "gst/gstelement.h"
#include "gtk/gtk.h"
#include "nppdefs.h"
#include <cstdint>
#include <vector>

const int g_w = 1024;
const int g_h = 1024;

extern NppStreamContext g_nppStreamCtx;
extern cudaStream_t g_stream;

extern Npp8u*	g_rgba_out;
extern Npp32s   g_rgba_outStep;

extern std::vector<void*> g_cuda_buf_to_free;

extern GstElement* g_pipeline_capture;
extern GstElement* g_pipeline_proc;

extern GstAppSrc* g_proc_src;

extern GtkWidget* g_stack;
extern gboolean g_show_processed;
extern std::vector<uint8_t> g_host_rgba;
extern guint64 g_proc_frame_id;

extern cudaEvent_t evStart, evStop;
extern float ms;

extern image_processing_fn current_processing_function;
extern int current_processing_fn_index;

extern bool g_input_toggle[0xffff];
