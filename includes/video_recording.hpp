#pragma once

#include <gst/gst.h>
#include <string>
#include <vector>

struct ClosedFragment
{
	std::string path;
	GstClockTime start_pts;
	GstClockTime end_pts;
};

struct PendingEvent
{
	bool active = false;

	GstClockTime start_pts = GST_CLOCK_TIME_NONE;
	GstClockTime end_pts = GST_CLOCK_TIME_NONE;

	std::string output_path;
};

void trigger_event_recording(GstClockTime event_pts, const std::string &output_path);
gboolean on_proc_bus_message(GstBus*, GstMessage *message, gpointer);
bool save_event_fragments(const std::vector<ClosedFragment> &fragments, const std::string &output_path);
