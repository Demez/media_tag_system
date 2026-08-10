#include "main.h"

// =================================================================================

extern bool g_mpv_resume_on_focus;

// =================================================================================

struct notification_t
{
	std::string msg;
	u64         time_added;
	double      time_remain;
};

constexpr double              NOTIFICATION_DURATION  = 5;
constexpr double              NOTIFICATION_FADE_TIME = 0.5;
constexpr size_t              NOTIFICATION_MAX_SHOWN = 5;

std::vector< notification_t > g_notification_queue;

// =================================================================================


void push_notification( const char* msg )
{
	g_notification_queue.emplace_back( msg, app::total_time, NOTIFICATION_DURATION );
	printf( "NOTIFICATION: %.4f - %s\n", app::total_time * 1000.f, msg );
}


void notification_draw( double frame_time )
{
	static double time_drawn = 0.f;
	static bool   fade_in    = true;

	if ( g_notification_queue.empty() )
	{
		time_drawn = 0.f;
		fade_in    = true;
		return;
	}

	// find expired ones first
	for ( size_t i = 0; i < g_notification_queue.size(); )
	{
		notification_t& notif = g_notification_queue[ i ];

		notif.time_remain -= frame_time;

		if ( notif.time_remain > 0.f )
		{
			i++;
			continue;
		}

		g_notification_queue.erase( g_notification_queue.begin() + i );
	}

	// check if empty again
	if ( g_notification_queue.empty() )
	{
		time_drawn = 0.f;
		fade_in    = true;
		return;
	}

	// draw last few notifications
	set_frame_draw();

	int width, height;
	SDL_GetWindowSize( app::window, &width, &height );

	ImVec2 notif_pos{};
	notif_pos.x = static_cast< float >( width ) / 2.f;
	notif_pos.y = 40.f;

	// ----------------------------------------

	// pivot aligns it to the center and the bottom of the window
	// ImGui::SetNextWindowPos( notif_pos, 0, ImVec2( 0.5f, 1.0f ) );
	ImGui::SetNextWindowPos( notif_pos, 0, ImVec2( 0.5f, 0.0f ) );

	ImGuiStyle& style        = ImGui::GetStyle();

	ImVec4      bg_color     = style.Colors[ ImGuiCol_FrameBg ];
	ImVec4      border_color = style.Colors[ ImGuiCol_Border ];
	bg_color.w               = 0.75;

	double max_notif_time    = -1.0;
	// get fadeout time
	size_t count             = std::min( NOTIFICATION_MAX_SHOWN, g_notification_queue.size() );

	//float  fade_in_amount    = std::min( 1.f, time_drawn / NOTIFICATION_FADE_IN_TIME );
	// float  fade_amount    = std::min( 1.f, time_drawn / NOTIFICATION_FADE_IN_TIME );
	double fade_amount       = 1.0;

	for ( size_t j = 0, i = g_notification_queue.size() - 1;; i--, j++ )
	{
		notification_t& notif = g_notification_queue[ i ];
		max_notif_time        = std::max( max_notif_time, notif.time_remain );

		if ( i == 0 || j == count )
			break;
	}

	if ( max_notif_time < NOTIFICATION_FADE_TIME )
	{
		fade_amount = max_notif_time / NOTIFICATION_FADE_TIME;

		//border_color.w = max_notif_time * border_color.w;
		//bg_color.w     = max_notif_time;
	}
	//else // if ( max_notif_time > NOTIFICATION_DURATION - NOTIFICATION_FADE_IN_TIME )
	{
		border_color.w *= fade_amount;
		bg_color.w *= fade_amount;
	}

	ImGui::PushStyleColor( ImGuiCol_WindowBg, bg_color );
	ImGui::PushStyleColor( ImGuiCol_Border, border_color );

	// ImGui::SetNextWindowSizeConstraints( { width - 80.f, -1.f }, { width - 80.f, -1.f } );

	//if ( !ImGui::GetIO().WantTextInput )
	//	ImGui::SetNextWindowFocus();

	if ( ImGui::Begin( "##notif", 0, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing ) )
	{
		for ( size_t j = 0, i = g_notification_queue.size() - 1;; i--, j++ )
		{
			notification_t& notif      = g_notification_queue[ i ];
			ImVec4          text_color = style.Colors[ ImGuiCol_Text ];

			// nice fade out effect
			if ( notif.time_remain < NOTIFICATION_FADE_TIME )
				text_color.w *= notif.time_remain;

			ImGui::PushStyleColor( ImGuiCol_Text, text_color );

			ImGui::TextUnformatted( g_notification_queue[ i ].msg.c_str() );
			// ImGui::Text( "%.f - %s", g_notification_queue[ i ].time_added, g_notification_queue[ i ].msg.c_str() );

			ImGui::PopStyleColor();

			if ( i == 0 || j == count )
				break;
		}

		ImGui::End();
	}

	ImGui::PopStyleColor();
	ImGui::PopStyleColor();

	time_drawn += frame_time;
}


void imgui_draw( double frame_time, bool render )
{
	if ( gallery::sort_mode_update )
	{
		//gallery_view_set_selection( gallery::cursor );
		gallery_view_sort_dir();
		gallery::sort_mode_update = false;
	}

	if ( g_gallery_view )
	{
		gallery_view_draw();
	}
	else
	{
		media_view_draw_imgui();
	}

	notification_draw( frame_time );

	if ( render )
		ImGui::Render();
	else
		ImGui::EndFrame();
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

