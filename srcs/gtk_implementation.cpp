#include <gtk_implementation.hpp>
#include <global.hpp>
#include <string>

static void select_filter(int newIndex)
{
	if (g_processing_filters.empty())
		return;

	if (g_current_filter)
		g_current_filter->deactivate();

	const int count = static_cast<int>(g_processing_filters.size());
	newIndex %= count;
	if (newIndex < 0)
		newIndex += count;

	g_current_filter_index = newIndex;
	g_current_filter = g_processing_filters[static_cast<size_t>(g_current_filter_index)].get();
	g_current_filter->activate();
}

gboolean on_key_press(GtkWidget*, GdkEventKey* event, gpointer)
{
	switch (event->keyval)
	{
		case GDK_KEY_space:
			g_show_processed = !g_show_processed;
			gtk_stack_set_visible_child_name(GTK_STACK(g_stack), g_show_processed ? "proc" : "raw");
			return TRUE;
		case GDK_KEY_Right:
			select_filter(g_current_filter_index + 1);
			return TRUE;
		case GDK_KEY_Left:
			select_filter(g_current_filter_index - 1);
			return TRUE;
		case GDK_KEY_Escape:
			gtk_main_quit();
			return TRUE;
		case GDK_KEY_r:
			g_trigger_record = true;
			return TRUE;
		case GDK_KEY_b:
			g_set_bg = true;
			return TRUE;
	}
	return FALSE;
}

gboolean on_key_release(GtkWidget*, GdkEventKey *event, gpointer)
{
	(void)event;
	return FALSE;
}

gboolean draw_info(GtkWidget*, cairo_t *cr, gpointer)
{
	cairo_set_source_rgb(cr, 1, 1, 1);
	cairo_set_font_size(cr, 24);
	cairo_move_to(cr, 20, 60);
	std::string ms_text("ms/f: ");
	ms_text += std::to_string(ms);
	cairo_show_text(cr, ms_text.c_str());
	std::string fps_text("fps: ");
	fps_text += std::to_string(1000 / ms);
	cairo_move_to(cr, 20, 90);
	cairo_show_text(cr, fps_text.c_str());
	cairo_move_to(cr, 20, 120);
	std::string proc_fn_text("Current processing function: ");
	proc_fn_text += g_current_filter ? g_current_filter->getName() : "None";
	cairo_show_text(cr, proc_fn_text.c_str());
	return FALSE;
}
