#include <build_pipeline.hpp>
#include <string>

GstElement* build_pipeline_capture(int width, int height)
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

GstElement* build_pipeline_proc(int width, int height)
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
