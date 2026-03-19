#pragma once

#include <gst/gst.h>

GstElement* build_pipeline_capture(int width, int height);
GstElement* build_pipeline_proc(int width, int height);
