#include "glib.h"
#include "gst/gstbus.h"
#include "gst/gstclock.h"
#include "gst/gstelement.h"
#include "gst/gstmessage.h"
#include "gst/gststructure.h"
#include <gst/gst.h>
#include <algorithm>
#include <atomic>
#include <deque>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include <video_recording.hpp>
#include <global.hpp>

namespace fs = std::filesystem;

static std::mutex g_fragment_mutex;

static std::unordered_map<std::string, GstClockTime> g_open_fragment_starts;

static std::deque<ClosedFragment> g_closed_fragments;

static PendingEvent g_pending_event;

void trigger_event_recording(GstClockTime event_pts, const std::string &output_path)
{
	if (!GST_CLOCK_TIME_IS_VALID(event_pts))
	{
		g_printerr("Cannot trigger recording: invalid event PTS\n");
		return;
	}

	const GstClockTime start_pts = event_pts >= 5 * GST_SECOND ? event_pts - 5 * GST_SECOND : 0;
	const GstClockTime end_pts = event_pts + 5 * GST_SECOND;

	std::lock_guard<std::mutex> lock(g_fragment_mutex);

	if (g_pending_event.active)
	{
		g_pending_event.start_pts = std::min(g_pending_event.start_pts, start_pts);
		g_pending_event.end_pts = std::max(g_pending_event.end_pts, end_pts);
		return ;
	}

	g_pending_event.active = true;
	g_pending_event.start_pts = start_pts;
	g_pending_event.end_pts = end_pts;
	g_pending_event.output_path = output_path;

	//g_print("Event requested: window=%" GST_TIME_FORMAT " to %" GST_TIME_FORMAT "\n", GST_TIME_ARGS(start_pts), GST_TIME_ARGS(end_pts));
}

gboolean on_proc_bus_message(GstBus*, GstMessage *message, gpointer)
{
	if (GST_MESSAGE_TYPE(message) != GST_MESSAGE_ELEMENT)
		return G_SOURCE_CONTINUE;

	const GstStructure *structure = gst_message_get_structure(message);

	if (!structure)
		return G_SOURCE_CONTINUE;

	const gchar *message_name = gst_structure_get_name(structure);
	const gchar *location = gst_structure_get_string(structure, "location");

	guint64 running_time = GST_CLOCK_TIME_NONE;

	gst_structure_get_uint64(structure, "running-time", &running_time);

	if (!location || !GST_CLOCK_TIME_IS_VALID(running_time))
		return G_SOURCE_CONTINUE;

	if (g_strcmp0(message_name, "splitmuxsink-fragment-opened") == 0)
	{
		std::lock_guard<std::mutex> lock(g_fragment_mutex);

		g_open_fragment_starts[location] = running_time;

		return G_SOURCE_CONTINUE;
	}

	if (g_strcmp0(message_name, "splitmuxsink-fragment-closed") != 0)
		return G_SOURCE_CONTINUE;

	std::vector<ClosedFragment> event_fragments;
	std::string output_path;

	{
		std::lock_guard<std::mutex> lock(g_fragment_mutex);

		auto start_it = g_open_fragment_starts.find(location);

		if (start_it == g_open_fragment_starts.end())
		{
			g_printerr("No opening timestamp for fragment: %s\n", location);
			return G_SOURCE_CONTINUE;
		}

		const GstClockTime fragment_start = start_it->second;
		const GstClockTime fragment_end = running_time;

		g_open_fragment_starts.erase(start_it);

		ClosedFragment fragment{location, fragment_start, fragment_end};

		g_closed_fragments.push_back(fragment);

		while (g_closed_fragments.size() > 120)
			g_closed_fragments.pop_front();

		//g_print("Fragment closed: %s, %" GST_TIME_FORMAT " to %" GST_TIME_FORMAT "\n", location, GST_TIME_ARGS(fragment_start), GST_TIME_ARGS(fragment_end));

		if (!g_pending_event.active || fragment_end < g_pending_event.end_pts)
			return G_SOURCE_CONTINUE;

		for (const ClosedFragment &candidate : g_closed_fragments)
		{
			if (candidate.end_pts > g_pending_event.start_pts && candidate.start_pts < g_pending_event.end_pts)
				event_fragments.push_back(candidate);
		}

		output_path = g_pending_event.output_path;
		g_pending_event = PendingEvent{};
	}

	if (event_fragments.empty())
	{
		g_printerr("No fragments found for events\n");
		return G_SOURCE_CONTINUE;
	}

	std::thread([fragments = std::move(event_fragments), output = std::move(output_path)]()
				{
					if (!save_event_fragments(fragments, output))
						g_printerr("Failed to save event file: %s\n", output.c_str());
				}).detach();

	return G_SOURCE_CONTINUE;
}

bool create_event_stage(const std::vector<ClosedFragment> &fragments, fs::path &stage_directory)
{
	const uint64_t event_id = g_event_sequence.fetch_add(1);

	stage_directory = fs::path("/dev/shm") / ("event_stage_" + std::to_string(event_id));

	std::error_code error;

	fs::create_directories(stage_directory, error);

	if (error)
	{
		g_printerr("Could not create event stage: %s\n", error.message().c_str());
		return false;
	}

	for (size_t index = 0; index < fragments.size(); index++)
	{
		char filename[64];

		g_snprintf(filename, sizeof(filename), "part_%05zu.mkv", index);

		const fs::path destination = stage_directory / filename;

		error.clear();

		fs::create_hard_link(fragments[index].path, destination, error);

		if (error)
        {
            error.clear();

            fs::copy_file(fragments[index].path, destination, fs::copy_options::overwrite_existing, error);

            if (error)
            {
                g_printerr("Could not stage %s: %s\n", fragments[index].path.c_str(), error.message().c_str());
                fs::remove_all(stage_directory);
                return false;
            }
        }
	}

	return true;
}

static bool remux_stage_to_mkv(const fs::path &stage_directory, const std::string &output_path)
{
	GError *error = nullptr;

	GstElement *pipeline = gst_parse_launch(
			"splitmuxsrc name=event_src ! "
			"h264parse ! "
			"matroskamux ! "
			"filesink name=event_output",
			&error);

	if (error)
	{
		g_printerr("Could not construct event remux pipeline: %s\n", error->message);
		g_error_free(error);

		if (pipeline)
			gst_object_unref(pipeline);

		return false;
	}

	GstElement *source = gst_bin_get_by_name(GST_BIN(pipeline), "event_src");
	GstElement *output = gst_bin_get_by_name(GST_BIN(pipeline), "event_output");

	if (!source || !output)
	{
		g_printerr("Could not find remux elements\n");

		if (source)
			gst_object_unref(source);

		if (output)
			gst_object_unref(output);

		gst_object_unref(pipeline);

		return false;
	}

	const std::string input_pattern = (stage_directory / "part_*.mkv").string();

	g_object_set(source, "location", input_pattern.c_str(), nullptr);
	g_object_set(output, "location", output_path.c_str(), nullptr);

	gst_object_unref(source);
	gst_object_unref(output);

	const GstStateChangeReturn state_result = gst_element_set_state(pipeline, GST_STATE_PLAYING);

	if (state_result == GST_STATE_CHANGE_FAILURE)
	{
		g_printerr("Could not start event remux pipeline\n");

		gst_element_set_state(pipeline, GST_STATE_NULL);
		gst_object_unref(pipeline);

		return false;
	}

	GstBus *bus = gst_element_get_bus(pipeline);

	GstMessage *message = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, static_cast<GstMessageType>(GST_MESSAGE_EOS | GST_MESSAGE_ERROR));

	bool success = false;

	if (message && GST_MESSAGE_TYPE(message) == GST_MESSAGE_EOS)
		success = true;

	else if (message && GST_MESSAGE_TYPE(message) == GST_MESSAGE_ERROR)
	{
		GError *pipeline_error = nullptr;
		gchar *debug = nullptr;

		gst_message_parse_error(message, &pipeline_error, &debug);

		g_printerr("Event remux failed: %s\n", pipeline_error ? pipeline_error->message : "unknown error");

		g_clear_error(&pipeline_error);
		g_free(debug);
	}

	if (message)
		gst_message_unref(message);

	gst_object_unref(bus);

	gst_element_set_state(pipeline, GST_STATE_NULL);

	gst_object_unref(pipeline);

	return success;
}

bool save_event_fragments(const std::vector<ClosedFragment> &fragments, const std::string &output_path)
{
	fs::path stage_directory;

	if (!create_event_stage(fragments, stage_directory))
		return false;

	const fs::path output_parent = fs::path(output_path).parent_path();

	std::error_code error;

	if (!output_parent.empty())
		fs::create_directories(output_parent, error);

	const bool success = remux_stage_to_mkv(stage_directory, output_path);

	fs::remove_all(stage_directory, error);

	if (success)
		g_print("Event saved: %s, fragments=%zu\n", output_path.c_str(), fragments.size());

	return success;
}
