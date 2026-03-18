#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include <cuda_runtime.h>
#include <cuda_egl_interop.h>
#include <EGL/egl.h>

#include "cairo.h"
#include "driver_types.h"
#include "gst/gstpad.h"
#include "nppdefs.h"
#include "nvbufsurface.h"

#include <npp.h>
#include <nppi.h>

#include <nppi_filtering_functions.h>
#include <nppi_arithmetic_and_logical_operations.h>
#include <nppi_data_exchange_and_initialization.h>
#include <nppi_color_conversion.h>

#include <string>
#include <vector>
#include <cstring>
#include <utility>
#include <iostream>

#include <filter.hpp>
#include <global.hpp>
#include <memory.hpp>

static GstElement* g_pipeline_capture = nullptr;
static GstElement* g_pipeline_proc = nullptr;

static GstAppSrc* g_proc_src = nullptr;

static GtkWidget* g_stack = nullptr;
static gboolean g_show_processed = FALSE;

Npp8u*	g_rgba_out = nullptr;
Npp32s   g_rgba_outStep = 0;

NppStreamContext g_nppStreamCtx;

cudaStream_t g_stream = 0;

static std::vector<uint8_t> g_host_rgba;
static guint64 g_proc_frame_id = 0;

static cudaEvent_t evStart = nullptr, evStop = nullptr;
static float ms = 0.0f;

static image_processing_fn current_processing_function = processing_functions[0].fn;
static int current_processing_fn_index = 0;

std::vector<void*> g_cuda_buf_to_free;
static bool g_input_toggle[0xffff];

static gboolean on_key_press(GtkWidget*, GdkEventKey* event, gpointer)
{
	g_input_toggle[event->keyval] = 1;
	switch (event->keyval)
	{
		case GDK_KEY_space:
			{
				g_show_processed = !g_show_processed;
				gtk_stack_set_visible_child_name(GTK_STACK(g_stack), g_show_processed ? "proc" : "raw");
				return TRUE;
			}
		case GDK_KEY_Right:
			{
				current_processing_fn_index++;
				if (current_processing_fn_index % processing_functions.size() == 0)
					current_processing_fn_index = 0;
				current_processing_function = processing_functions[current_processing_fn_index].fn;
				return TRUE;
			}
		case GDK_KEY_Left:
			{
				current_processing_fn_index--;
				if (current_processing_fn_index == -1)
					current_processing_fn_index = processing_functions.size()-1;
				current_processing_function = processing_functions[current_processing_fn_index].fn;
				return TRUE;
			}
		case GDK_KEY_Escape:
				gtk_main_quit();
	}
	return FALSE;
}

static gboolean on_key_release(GtkWidget*, GdkEventKey *event, gpointer)
{
	g_input_toggle[event->keyval] = 0;
	return FALSE;
}

static void push_processed_rgba_to_appsrc(const uint8_t* data, size_t size, int fps)
{
    if (!g_proc_src)
        return;

    GstBuffer* out = gst_buffer_new_allocate(nullptr, size, nullptr);
    if (!out)
        return;

    GstMapInfo m;
    memset(&m, 0, sizeof(m));

    if (!gst_buffer_map(out, &m, GST_MAP_WRITE))
    {
        gst_buffer_unref(out);
        return;
    }

    memcpy(m.data, data, size);
    gst_buffer_unmap(out, &m);

    GstClockTime pts = gst_util_uint64_scale(g_proc_frame_id, GST_SECOND, (guint64)fps);
    GstClockTime dur = gst_util_uint64_scale(1, GST_SECOND, (guint64)fps);

    GST_BUFFER_PTS(out) = pts;
    GST_BUFFER_DTS(out) = pts;
    GST_BUFFER_DURATION(out) = dur;

    g_proc_frame_id++;

    GstFlowReturn fr = gst_app_src_push_buffer(g_proc_src, out);
    if (fr != GST_FLOW_OK)
        return;
}

gboolean draw_ms(GtkWidget *widget, cairo_t *cr, gpointer data)
{
	(void)data;
	(void)widget;
	cairo_set_source_rgb(cr, 1, 1, 1);
	cairo_set_font_size(cr, 24);
	cairo_move_to(cr, 20, 60);
	std::string ms_text("ms/f: ");
	ms_text += std::to_string(ms);
	cairo_show_text(cr, ms_text.c_str());
	std::string fps_text("fps: ");
	fps_text += std::to_string(1000/ms);
	cairo_move_to(cr, 20, 90);
	cairo_show_text(cr, fps_text.c_str());
	cairo_move_to(cr, 20, 120);
	std::string proc_fn_text("Current processing function: ");
	proc_fn_text += processing_functions[current_processing_fn_index].name; 
	cairo_show_text(cr, proc_fn_text.c_str());
	return FALSE;
}

static GstFlowReturn on_new_sample(GstAppSink* appsink, gpointer)
{
	if (evStart == nullptr)
	{
		cudaEventCreate(&evStart);
		cudaEventCreate(&evStop);
	}

	static bool init = false;

    GstSample* sample = nullptr;
    GstBuffer* buffer = nullptr;
    GstMapInfo map;
    memset(&map, 0, sizeof(map));
    NvBufSurface* surf = nullptr;

	EGLImageKHR eglImage = nullptr;

	cudaGraphicsResource_t cudaResource = nullptr;
	cudaEglFrame eglFrame;
	cudaPitchedPtr pp;
	Npp8u* d_in = nullptr;
	int step = 0;

	bool gst_mapped = false;
	bool egl_mapped = false;
	bool cuda_registered = false;

	cudaError_t ce = cudaSuccess;

	static int counter = 0;
	counter++;

	bool doProfile = ((counter % 30) == 0);

	if (doProfile && g_rgba_out)
		cudaEventRecord(evStart, g_stream);

	sample = gst_app_sink_pull_sample(appsink);
	if (!sample)
		goto cleanup;

	buffer = gst_sample_get_buffer(sample);
	if (!buffer)
		goto cleanup;

	if (!gst_buffer_map(buffer, &map, GST_MAP_READ))
		goto cleanup;

	gst_mapped = true;

	surf = (NvBufSurface*)map.data;
	if (!surf)
		goto cleanup;

	cudaSetDevice(0);
	cudaFree(0);
	nppSetStream(g_stream);

	if (!init)
	{
		nppGetStreamContext(&g_nppStreamCtx);

		g_host_rgba.resize((size_t)g_w * (size_t)g_h * 4);
		init = true;
	}

	if (NvBufSurfaceMapEglImage(surf, 0) != 0)
		goto cleanup;

	egl_mapped = true;

	eglImage = (EGLImageKHR)surf->surfaceList[0].mappedAddr.eglImage;
	if (!eglImage)
		goto cleanup;

	ce = cudaGraphicsEGLRegisterImage(&cudaResource, eglImage, cudaGraphicsRegisterFlagsReadOnly);
	if (ce != cudaSuccess)
		goto cleanup;

	cuda_registered = true;

	ce = cudaGraphicsResourceGetMappedEglFrame(&eglFrame, cudaResource, 0, 0);
	if (ce != cudaSuccess)
		goto cleanup;

	if (eglFrame.frameType != cudaEglFrameTypePitch)
		goto cleanup;

    pp = eglFrame.frame.pPitch[0];
    d_in = (Npp8u*)pp.ptr;
    step = (int)pp.pitch;

	current_processing_function(d_in, step, false);
	nppiMirror_8u_C4IR_Ctx(g_rgba_out, g_rgba_outStep, (NppiSize){g_w, g_h}, NPP_BOTH_AXIS, g_nppStreamCtx);
	{
        size_t rowBytes = (size_t)g_w * 4;
        ce = cudaMemcpy2D(g_host_rgba.data(), rowBytes, g_rgba_out, (size_t)g_rgba_outStep, rowBytes, (size_t)g_h, cudaMemcpyDeviceToHost);
		if (doProfile)
		{
			cudaEventRecord(evStop, g_stream);
			cudaEventSynchronize(evStop);
			cudaEventElapsedTime(&ms, evStart, evStop);
		}
        if (ce == cudaSuccess)
            push_processed_rgba_to_appsrc(g_host_rgba.data(), g_host_rgba.size(), 30);
    }

cleanup:
    if (cuda_registered)
        cudaGraphicsUnregisterResource(cudaResource);
    if (egl_mapped)
        NvBufSurfaceUnMapEglImage(surf, 0);
    if (gst_mapped)
        gst_buffer_unmap(buffer, &map);
    if (sample)
        gst_sample_unref(sample);
    return GST_FLOW_OK;
}

static GstElement* build_pipeline_capture(int width, int height)
{
	std::string pipe =
		"nvarguscamerasrc ! " 
		"video/x-raw(memory:NVMM),width=" + std::to_string(width) + ",height=" + std::to_string(height) + ",framerate=30/1,format=NV12 ! " 
		"tee name=t " 
		"t. ! queue ! nvvidconv ! gtksink name=raw_sink sync=false " 
		"t. ! queue leaky=downstream max-size-buffers=1 ! " 
		"nvvidconv ! video/x-raw(memory:NVMM),format=RGBA ! " 
		"appsink name=proc_sink emit-signals=true max-buffers=1 drop=true sync=false";

    GError* err = nullptr;
    GstElement* pipeline = gst_parse_launch(pipe.c_str(), &err);
	if (err)
	{
		g_printerr("gst_parse_launch capture failed: %s\n", err->message);
		g_error_free(err);
		return nullptr;
	}
    return pipeline;
}

static GstElement* build_pipeline_proc(int width, int height)
{
    std::string pipe =
        "appsrc name=proc_src is-live=true do-timestamp=false format=time block=false ! "
        "video/x-raw,format=RGBA,width=" + std::to_string(width) + ",height=" + std::to_string(height) + ",framerate=30/1 ! "
        "queue ! "
		"videoconvert ! "
        "gtksink name=proc_sink sync=false";

    GError* err = nullptr;
    GstElement* pipeline = gst_parse_launch(pipe.c_str(), &err);
	if (err)
	{
		g_printerr("gst_parse_launch proc failed: %s\n", err->message);
		g_error_free(err);
		return nullptr;
	}
    return pipeline;
}

int main(int argc, char** argv)
{
	initBuffers();
	bool ret = initFilterBuffers();
	ret |= getBuffer(BUF_8u_C4, MEM_PRIVATE, 0, (void **)&g_rgba_out, &g_rgba_outStep);
	if (ret)
	{
		freeBuffers();
		for (auto &buf : g_cuda_buf_to_free)
			cudaFree(buf);
		std::cerr << "Error while buffer allocation, abbort\n";
		return 1;
	}

    gst_init(&argc, &argv);
    gtk_init(&argc, &argv);

    g_pipeline_capture = build_pipeline_capture(g_w, g_h);
    if (!g_pipeline_capture)
        return 1;

    g_pipeline_proc = build_pipeline_proc(g_w, g_h);
    if (!g_pipeline_proc)
    {
        gst_object_unref(g_pipeline_capture);
        return 2;
    }

    GstElement* raw_sink = gst_bin_get_by_name(GST_BIN(g_pipeline_capture), "raw_sink");
    GstElement* proc_sink = gst_bin_get_by_name(GST_BIN(g_pipeline_proc), "proc_sink");
    GstElement* proc_appsrc = gst_bin_get_by_name(GST_BIN(g_pipeline_proc), "proc_src");
    GstElement* proc_appsink = gst_bin_get_by_name(GST_BIN(g_pipeline_capture), "proc_sink");

    if (!raw_sink || !proc_sink || !proc_appsrc || !proc_appsink)
    {
        if (raw_sink) gst_object_unref(raw_sink);
        if (proc_sink) gst_object_unref(proc_sink);
        if (proc_appsrc) gst_object_unref(proc_appsrc);
        if (proc_appsink) gst_object_unref(proc_appsink);
        gst_object_unref(g_pipeline_proc);
        gst_object_unref(g_pipeline_capture);
        return 3;
    }

    GtkWidget* raw_widget = nullptr;
    GtkWidget* proc_widget = nullptr;

    g_object_get(raw_sink, "widget", &raw_widget, nullptr);
    g_object_get(proc_sink, "widget", &proc_widget, nullptr);

    if (!raw_widget || !proc_widget)
    {
        gst_object_unref(raw_sink);
        gst_object_unref(proc_sink);
        gst_object_unref(proc_appsrc);
        gst_object_unref(proc_appsink);
        gst_object_unref(g_pipeline_proc);
        gst_object_unref(g_pipeline_capture);
        return 4;
    }

    g_proc_src = GST_APP_SRC(proc_appsrc);

    gst_app_sink_set_emit_signals(GST_APP_SINK(proc_appsink), TRUE);
    gulong new_sample_handler = g_signal_connect(proc_appsink, "new-sample", G_CALLBACK(on_new_sample), nullptr);

    g_stack = gtk_stack_new();
    gtk_stack_add_named(GTK_STACK(g_stack), raw_widget, "raw");
	gtk_stack_add_named(GTK_STACK(g_stack), proc_widget, "proc");

    gtk_stack_set_visible_child_name(GTK_STACK(g_stack), "raw");

    GtkWidget* window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(window), g_w, g_h);

    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), nullptr);
    g_signal_connect(window, "key-press-event", G_CALLBACK(on_key_press), nullptr);
	g_signal_connect(window, "key-release-event", G_CALLBACK(on_key_release), nullptr);
	g_signal_connect_after(window, "draw", G_CALLBACK(draw_ms), nullptr);

    gtk_container_add(GTK_CONTAINER(window), g_stack);
    gtk_widget_show_all(window);

    gst_element_set_state(g_pipeline_proc, GST_STATE_PLAYING);
    gst_element_set_state(g_pipeline_capture, GST_STATE_PLAYING);

	gtk_main();

	if (g_proc_src)
		gst_app_src_end_of_stream(g_proc_src);

	g_signal_handler_disconnect(proc_appsink, new_sample_handler);
	gst_app_sink_set_emit_signals(GST_APP_SINK(proc_appsink), FALSE);

    gst_element_set_state(g_pipeline_capture, GST_STATE_NULL);
	gst_element_get_state(g_pipeline_capture, NULL, NULL, GST_CLOCK_TIME_NONE);
    
	gst_element_set_state(g_pipeline_proc, GST_STATE_NULL);
	gst_element_get_state(g_pipeline_proc, NULL, NULL, GST_CLOCK_TIME_NONE);

    gst_object_unref(raw_sink);
    gst_object_unref(proc_sink);
    gst_object_unref(proc_appsrc);
    gst_object_unref(proc_appsink);

    gst_object_unref(g_pipeline_capture);
    gst_object_unref(g_pipeline_proc);

	g_object_unref(raw_widget);
	g_object_unref(proc_widget);

	freeBuffers();
	for (auto &buf : g_cuda_buf_to_free)
		cudaFree(buf);

    return 0;
}
