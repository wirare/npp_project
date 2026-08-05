#pragma once

#include "glib.h"
#include "gtk/gtk.h"

gboolean on_key_press(GtkWidget*, GdkEventKey* event, gpointer);
gboolean on_key_release(GtkWidget*, GdkEventKey* event, gpointer);
gboolean draw_info(GtkWidget*, cairo_t* cr, gpointer);
