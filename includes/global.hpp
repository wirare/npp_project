#pragma once

#ifdef __CUDACC__
#pragma push_macro("__noinline__")
#undef __noinline__
#endif
#include "gst/app/gstappsrc.h"
#include "gst/gstelement.h"
#include "gtk/gtk.h"
#ifdef __CUDACC__
#pragma pop_macro("__noinline__")
#endif

#include "filter.hpp"
#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>
#include "cuda_global.hpp"

extern GstElement* g_pipeline_capture;
extern GstElement* g_pipeline_proc;
extern GstElement* g_camera_source;

extern GstAppSrc* g_proc_src;

extern GtkWidget* g_stack;
extern gboolean g_show_processed;
extern std::vector<uint8_t> g_host_rgba;
extern guint64 g_proc_frame_id;

extern std::vector<std::unique_ptr<AFilter>> g_processing_filters;
extern AFilter* g_current_filter;
extern int g_current_filter_index;

extern std::atomic_uint64_t g_event_sequence;
extern bool g_trigger_record;
extern bool g_set_bg;
