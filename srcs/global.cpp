#include <global.hpp>

Npp8u* g_rgba_out = nullptr;
Npp32s g_rgba_outStep = 0;

NppStreamContext g_nppStreamCtx;

cudaStream_t g_stream = 0;

std::vector<void*> g_cuda_buf_to_free;

GstElement* g_pipeline_capture = nullptr;
GstElement* g_pipeline_proc = nullptr;
GstElement* g_camera_source = nullptr;

GstAppSrc* g_proc_src = nullptr;

GtkWidget* g_stack = nullptr;
gboolean g_show_processed = FALSE;
std::vector<uint8_t> g_host_rgba;
guint64 g_proc_frame_id = 0;

cudaEvent_t evStart = nullptr, evStop = nullptr;
float ms = 0.0f;

std::vector<std::unique_ptr<AFilter>> g_processing_filters;
AFilter* g_current_filter = nullptr;
int g_current_filter_index = 0;

std::atomic_uint64_t g_event_sequence{0};

bool g_trigger_record = false;
bool g_set_bg = false;
