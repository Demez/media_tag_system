#include "main.h"

// =================================================================================

extern bool g_mpv_resume_on_focus;

// =================================================================================

struct notification_t
{
	std::string msg;
	u64         time_added;
};

constexpr u64                 NOTIFICATION_DURATION  = 5000;
constexpr double              NOTIFICATION_FADE_TIME = 0.5;
constexpr size_t              NOTIFICATION_MAX_SHOWN = 5;

std::vector< notification_t > g_notification_queue;

// =================================================================================


void push_notification( const char* msg )
{
	g_notification_queue.emplace_back( msg, sys_get_time_ms() );
	printf( "NOTIFICATION: %.4f - %s\n", sys_get_time_ms() * 1000.f, msg );
}


void notification_draw()
{
	static bool fade_in = true;

	if ( g_notification_queue.empty() )
	{
		fade_in = true;
		return;
	}

	u64 current_time = sys_get_time_ms();

	// find expired ones first
	for ( size_t i = 0; i < g_notification_queue.size(); )
	{
		notification_t& notif = g_notification_queue[ i ];

		if ( notif.time_added + NOTIFICATION_DURATION > current_time )
		{
			i++;
			continue;
		}

		g_notification_queue.erase( g_notification_queue.begin() + i );
	}

	// check if empty again
	if ( g_notification_queue.empty() )
	{
		fade_in = true;
		return;
	}

	// draw last few notifications
	// set_frame_draw();
}


void imgui_draw( bool render )
{
	if ( g_gallery_view )
	{
		gallery_view_draw();
	}
	else
	{
		media_view_draw_imgui();
	}

	notification_draw();

	//if ( render )
	//	ImGui::Render();
	//else
	//	ImGui::EndFrame();
}


void view_type_toggle()
{
	if ( g_gallery_view )
	{
		set_view_type_media();
	}
	else
	{
		set_view_type_gallery();
	}
}


void set_view_type_gallery()
{
	if ( g_gallery_view )
		return;

	// pause video if playing one back
	if ( mpv_get_current_video() )
	{
		s32 paused = 0;
		p_mpv_get_property( g_mpv, "pause", MPV_FORMAT_FLAG, &paused );
		g_mpv_resume_on_focus = !paused;

		const char* cmd[]     = { "set", "pause", "yes", NULL };
		int         cmd_ret   = p_mpv_command_async( g_mpv, 0, cmd );
	}

	gallery_view_scroll_to_cursor();

	g_gallery_view = true;

	update_window_title();
	set_frame_draw( 3 );
}


void set_view_type_media( bool force_load_media )
{
	//if ( !g_gallery_view )
	//	return;

	u32 selected = gallery_view_get_last_selected_index();

	if ( directory::folder_loading )
		g_image_data.index = selected;

	bool reload_image = force_load_media;
	reload_image |= g_image_data.index != selected;
	reload_image |= directory::folder_reload;

	if ( reload_image )
	{
		g_image_data.index = selected;
		media_view_load();
	}

	if ( g_mpv_resume_on_focus )
	{
		const char* cmd[]   = { "set", "pause", "no", NULL };
		int         cmd_ret = p_mpv_command_async( g_mpv, 0, cmd );
	}

	g_gallery_view = false;

	update_window_title();
	set_frame_draw( 3 );
}

