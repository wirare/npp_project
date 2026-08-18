#include <build_pipeline.hpp>
#include <string>

GstElement* build_pipeline_capture(int width, int height)
{
	std::string pipe =
		"nvarguscamerasrc name=camera_source ! " 
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
        "appsrc name=proc_src is-live=true do-timestamp=false format=time block=false max-buffers=2 max-bytes=0 max-time=0 leaky-type=downstream ! "
        "video/x-raw,format=RGBA,width=" + std::to_string(width) + ",height=" + std::to_string(height) + ",framerate=30/1 ! "
		"tee name=processed_t "
		"processed_t. ! queue leaky=downstream max-size-buffers=2 max-size-bytes=0 max-size-time=0 ! videoconvert ! gtksink name=proc_sink sync=false "
		"processed_t. ! queue leaky=downstream max-size-buffers=2 max-size-bytes=0 max-size-time=0 ! videoconvert ! video/x-raw,format=I420 ! "
		"x264enc bitrate=8000 speed-preset=ultrafast tune=zerolatency key-int-max=30 ! "
		"h264parse config-interval=-1 ! "
		"splitmuxsink name=rolling_sink location=/dev/shm/processed_ring/processed_%05d.mkv "
		"muxer-factory=matroskamux max-size-time=1000000000 max-size-bytes=0 max-files=30 async-finalize=true";

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
