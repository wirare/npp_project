#include <global.hpp>

Npp8u*	g_rgba_out = nullptr;
Npp32s   g_rgba_outStep = 0;

NppStreamContext g_nppStreamCtx;

cudaStream_t g_stream = 0;

std::vector<void*> g_cuda_buf_to_free;

GstElement* g_pipeline_capture = nullptr;
GstElement* g_pipeline_proc = nullptr;

GstAppSrc* g_proc_src = nullptr;

GtkWidget* g_stack = nullptr;
gboolean g_show_processed = FALSE;
std::vector<uint8_t> g_host_rgba;
guint64 g_proc_frame_id = 0;

cudaEvent_t evStart = nullptr, evStop = nullptr;
float ms = 0.0f;

image_processing_fn current_processing_function = processing_functions[0].fn;
int current_processing_fn_index = 0;

bool g_input_toggle[0xffff];
