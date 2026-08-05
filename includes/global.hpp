#pragma once

#include "filter.hpp"
#include "glib.h"
#include "gst/app/gstappsrc.h"
#include "gst/gstelement.h"
#include "gtk/gtk.h"
#include <cstdint>
#include <vector>
#include "cuda_global.hpp"

extern GstElement* g_pipeline_capture;
extern GstElement* g_pipeline_proc;

extern GstAppSrc* g_proc_src;

extern GtkWidget* g_stack;
extern gboolean g_show_processed;
extern std::vector<uint8_t> g_host_rgba;
extern guint64 g_proc_frame_id;

extern image_processing_fn current_processing_function;
extern int current_processing_fn_index;

extern bool g_input_toggle[0xffff];
