#include <gst/gst.h>
#include <global.hpp>
#include <camera_control.hpp>

void setCameraAutoControlsLocked(bool locked)
{
	if (!g_camera_source)
		return;

	g_object_set(
        G_OBJECT(g_camera_source),
        "aelock", locked ? TRUE : FALSE,
        "awblock", locked ? TRUE : FALSE,
        nullptr
    );
}
