#include <cstdio>
#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include <cuda_runtime.h>
#include <cuda_egl_interop.h>
#include <EGL/egl.h>

#include "driver_types.h"
#include "gst/gstclock.h"
#include "gst/gstobject.h"
#include "gst/gstpad.h"
#include "nppdefs.h"
#include "nvbufsurface.h"
#include "video_recording.hpp"

#include <npp.h>
#include <nppi.h>

#include <string>
#include <vector>
#include <cstring>
#include <utility>
#include <iostream>

#include <filter.hpp>
#include <global.hpp>
#include <memory.hpp>
#include <gtk_implementation.hpp>
#include <build_pipeline.hpp>

static GstClockTime g_first_capture_pts = GST_CLOCK_TIME_NONE;
static GstClockTime g_last_proc_pts     = GST_CLOCK_TIME_NONE;

static bool push_processed_rgba_to_appsrc(const uint8_t* data, size_t size, GstClockTime capture_pts, GstClockTime capture_duration, int fps, GstClockTime *pushed_pts)
{
    if (!g_proc_src || !data || size == 0 || fps <= 0)
        return false;

    GstBuffer* out = gst_buffer_new_allocate(nullptr, size, nullptr);

    if (!out)
        return false;

    GstMapInfo map = GST_MAP_INFO_INIT;

    if (!gst_buffer_map(out, &map, GST_MAP_WRITE))
    {
        gst_buffer_unref(out);
        return false;
    }

    memcpy(map.data, data, size);
    gst_buffer_unmap(out, &map);

    const GstClockTime fallback_duration = gst_util_uint64_scale(1, GST_SECOND, static_cast<guint64>(fps));
    GstClockTime output_pts;

    if (GST_CLOCK_TIME_IS_VALID(capture_pts))
    {
        if (!GST_CLOCK_TIME_IS_VALID(g_first_capture_pts))
            g_first_capture_pts = capture_pts;

        if (capture_pts < g_first_capture_pts)
        {
            g_printerr("Invalid capture PTS ordering\n");
            gst_buffer_unref(out);
            return false;
        }

        output_pts = capture_pts - g_first_capture_pts;
    }
    else
        output_pts = gst_util_uint64_scale(g_proc_frame_id, GST_SECOND, static_cast<guint64>(fps));

    GstClockTime output_duration = GST_CLOCK_TIME_IS_VALID(capture_duration) ? capture_duration : fallback_duration;

    if (GST_CLOCK_TIME_IS_VALID(g_last_proc_pts) && output_pts <= g_last_proc_pts)
    {
        g_printerr(
            "Non-monotonic processed PTS: "
            "previous=%" GST_TIME_FORMAT
            ", current=%" GST_TIME_FORMAT "\n",
            GST_TIME_ARGS(g_last_proc_pts),
            GST_TIME_ARGS(output_pts));

        gst_buffer_unref(out);
        return false;
    }

    GST_BUFFER_PTS(out) = output_pts;

    GST_BUFFER_DTS(out) = GST_CLOCK_TIME_NONE;

    GST_BUFFER_DURATION(out) = output_duration;

	if (pushed_pts)
		*pushed_pts = output_pts;

    GST_BUFFER_OFFSET(out) = g_proc_frame_id;
    GST_BUFFER_OFFSET_END(out) = g_proc_frame_id + 1;

    const GstFlowReturn flow = gst_app_src_push_buffer(g_proc_src, out);

    if (flow != GST_FLOW_OK)
    {
        g_printerr(
            "Failed to push processed buffer: %s\n",
            gst_flow_get_name(flow));

        return false;
    }

    g_last_proc_pts = output_pts;
    ++g_proc_frame_id;

    return true;
}

static GstFlowReturn on_new_sample(GstAppSink* appsink, gpointer)
{
	if (!g_show_processed)
		return GST_FLOW_OK;

	if (evStart == nullptr)
	{
		cudaEventCreate(&evStart);
		cudaEventCreate(&evStop);
	}

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

	GstClockTime capture_pts = GST_CLOCK_TIME_NONE;
	GstClockTime capture_duration = GST_CLOCK_TIME_NONE;

	if (doProfile && g_rgba_out)
		cudaEventRecord(evStart, g_stream);

	sample = gst_app_sink_pull_sample(appsink);
	if (!sample)
		goto cleanup;

	buffer = gst_sample_get_buffer(sample);
	if (!buffer)
		goto cleanup;

	capture_pts = GST_BUFFER_PTS(buffer);
	capture_duration = GST_BUFFER_DURATION(buffer);

	if (!gst_buffer_map(buffer, &map, GST_MAP_READ))
		goto cleanup;

	gst_mapped = true;

	surf = (NvBufSurface*)map.data;
	if (!surf)
		goto cleanup;

	cudaSetDevice(0);
	cudaFree(0);
	nppSetStream(g_stream);

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

	if (!g_current_filter)
		goto cleanup;

	g_current_filter->process(d_in, step);
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

		GstClockTime processed_pts = GST_CLOCK_TIME_NONE;

        if (ce == cudaSuccess)
		{
            bool pushed = push_processed_rgba_to_appsrc(g_host_rgba.data(), g_host_rgba.size(), capture_pts, capture_duration, 30, &processed_pts);
			if (pushed && g_trigger_record)
			{
				g_trigger_record = false;

				const uint64_t event_number = g_event_sequence.load();

				const std::string output = "/mnt/persistent/event/event_" + std::to_string(event_number) + ".mkv";

				printf("RECORD EVENT\n");
				trigger_event_recording(processed_pts, output);
			}
		}
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

int main(int argc, char** argv)
{
	if (cudaSetDevice(0) != cudaSuccess || cudaFree(0) != cudaSuccess)
	{
		std::cerr << "Failed to initialize CUDA\n";
		return 1;
	}

	nppSetStream(g_stream);
	if (nppGetStreamContext(&g_nppStreamCtx) != NPP_SUCCESS)
	{
		std::cerr << "Failed to initialize NPP stream context\n";
		return 1;
	}

	g_processing_filters = createProcessingFilters();
	if (g_processing_filters.empty())
	{
		std::cerr << "No processing filters registered\n";
		return 1;
	}

	std::vector<std::vector<bufferRequest>> requestSets;
	requestSets.reserve(g_processing_filters.size() + 1);

	// The processed RGBA output is pipeline-owned persistent memory, not a
	// filter scratch buffer, so it participates as its own private request set.
	requestSets.push_back({REQUEST(BUF_8u_C4, STATUS_PRIVATE, 1)});
	for (auto& filter : g_processing_filters)
		requestSets.push_back(filter->getRequestedBuffers());

	const std::vector<bufferRequest> mergedRequests = mergeBufferRequests(requestSets);
	if (!initBuffers(mergedRequests))
	{
		std::cerr << "Error while allocating merged NPP buffer pool, abort\n";
		return 1;
	}

	if (getBuffer(BUF_8u_C4, MEM_PRIVATE, 0, reinterpret_cast<void**>(&g_rgba_out), &g_rgba_outStep) != 0 ||
		!initFilters(g_processing_filters))
	{
		freeBuffers();
		for (auto &buf : g_cuda_buf_to_free)
			cudaFree(buf);
		g_cuda_buf_to_free.clear();
		std::cerr << "Error while initializing filters, abort\n";
		return 1;
	}

	g_current_filter_index = 0;
	g_current_filter = g_processing_filters[0].get();
	g_current_filter->activate();
	g_host_rgba.resize(static_cast<size_t>(g_w) * static_cast<size_t>(g_h) * 4);

    gst_init(&argc, &argv);
    gtk_init(&argc, &argv);

	g_mkdir_with_parents("/dev/shm/processed_ring", 0755);

    g_pipeline_capture = build_pipeline_capture(g_w, g_h);
    if (!g_pipeline_capture)
        return 1;

    g_pipeline_proc = build_pipeline_proc(g_w, g_h);
    if (!g_pipeline_proc)
    {
        gst_object_unref(g_pipeline_capture);
        return 2;
    }

	g_camera_source = gst_bin_get_by_name(GST_BIN(g_pipeline_capture), "camera_source");
	if (!g_camera_source)
	{
		gst_object_unref(g_pipeline_capture);
		gst_object_unref(g_pipeline_proc);
		return 3;
	}

	GstBus *bus = gst_element_get_bus(g_pipeline_proc);

	gst_bus_add_watch(bus, on_proc_bus_message, nullptr);

	gst_object_unref(bus);

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
		gst_object_unref(g_camera_source);
        return 4;
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
		gst_object_unref(g_camera_source);
        return 5;
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
	g_signal_connect_after(window, "draw", G_CALLBACK(draw_info), nullptr);

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
	gst_object_unref(g_camera_source);

	g_object_unref(raw_widget);
	g_object_unref(proc_widget);

	if (g_current_filter)
	{
		g_current_filter->deactivate();
		g_current_filter = nullptr;
	}
	g_processing_filters.clear();

	freeBuffers();
	for (auto &buf : g_cuda_buf_to_free)
		cudaFree(buf);
	g_cuda_buf_to_free.clear();

    return 0;
}
