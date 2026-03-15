#include "main.h"

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_opengl3.h"
#include "imgui_freetype.h"
#include "imgui_internal.h"

#include <chrono>


// General App Data
namespace app
{
	bool         running        = true;

	SDL_Window*  window         = nullptr;
	bool         window_focused = false;
	bool         window_resized = false;

	// ImVec4                       clear_color = ImVec4( 0.15f, 0.15f, 0.15f, 1.00f );
	ImVec4       clear_color    = ImVec4( 0.05f, 0.05f, 0.05f, 1.00f );

	u64          total_time     = 0;
	float        frame_time     = 0.f;

	ImVec2       mouse_delta;
	ImVec2       mouse_pos;

	// imgui scroll hack lol
	bool         mouse_scrolled_up;
	bool         mouse_scrolled_down;

	app_config_t config{};
}


// ImGui Fonts
namespace font
{
	ImFont* normal        = nullptr;
	ImFont* normal_bold   = nullptr;
	ImFont* normal_italic = nullptr;
}


// Current Working Directory Information
namespace directory
{
	fs::path                     path;
	fs::path                     queued;  // will change to this folder start of next frame
	std::vector< media_entry_t > media_list;

	// TODO: get rid of these "thumbnail handles", i don't think it's needed anymore, just use the index in media list
	// and make sure to clear the thumbnail cache when needed
	std::vector< h_thumbnail >   thumbnail_list;
}

// =================================================================================

bool                         g_gallery_view = false;
std::vector< fs::path >      g_drag_drop_files;

// Main Image
main_image_data_t            g_image_data;
main_image_data_t            g_image_scaled_data;

static SDL_GLContext         g_gl_context;

// =================================================================================

struct notification_t
{
	std::string msg;
	u64         time_added;
	float       time_remain;
};

constexpr float               NOTIFICATION_DURATION     = 5;
constexpr float               NOTIFICATION_FADE_IN_TIME = 0.5;
constexpr size_t              NOTIFICATION_MAX_SHOWN    = 5;

std::vector< notification_t > g_notification_queue;

// =================================================================================


void update_window_title()
{
	char buf[ 512 ];

	if ( g_gallery_view )
	{
		snprintf( buf, 512, "Media Tag System [%zd] - %s", directory::media_list.size(), directory::path.string().c_str() );
	}
	else
	{
		if ( gallery::sorted_media.size() >= gallery::cursor )
			snprintf( buf, 512, "Media Tag System [%zd / %zd] - %s", gallery::cursor, gallery::sorted_media.size(), gallery_item_get_path_string( gallery::cursor ).c_str() );
		else
			snprintf( buf, 512, "Media Tag System [%zd]", gallery::sorted_media.size() );
	}

	SDL_SetWindowTitle( app::window, buf );
}


void folder_load_media_list()
{
	thumbnail_clear_cache();

	directory::media_list.clear();
	directory::thumbnail_list.clear();

	directory::media_list.reserve( 5000 );
	directory::thumbnail_list.reserve( 5000 );

	gallery::item_size_changed = true;
	gallery::item_text_size.clear();

	std::string root = directory::path.string();
	std::vector< file_t > files{};

	if ( !sys_scandir( root.c_str(), nullptr, files, e_scandir_abs_paths ) )
	{
		printf( "Failed to scan directory\n" );
		return;
	}

	directory::media_list.reserve( files.size() );

	for ( const file_t& entry : files )
	{
		media_entry_t media_entry{};
		media_entry.file     = entry;
		media_entry.filename = entry.path.filename().string();

		// if ( fs_is_dir( entry.data() ) )
		if ( entry.type & e_file_type_directory )
		{
			media_entry.type = e_media_type_directory;
			directory::media_list.push_back( media_entry );
			continue;
		}

		const fs::path& ext       = entry.path.extension();
		// bool            valid_ext = image_check_extension( ext.string() );
		bool            valid_ext = image_check_extension( media_entry.filename );

		if ( valid_ext )
		{
			media_entry.type = e_media_type_image;
		}
		else if ( g_mpv )
		{
			if ( mpv_supports_ext( ext.string() ) )
			{
				valid_ext        = true;
				media_entry.type = e_media_type_video;
			}
		}

		if ( !valid_ext )
			continue;

		// if ( !sys_get_file_times_and_size( entry.data(), &file.date_created, nullptr, &file.date_mod, &file.file_size ) )
		// {
		// 	printf( "Failed to get file date created and modified, and size: %s\n", entry.data() );
		// }

		directory::media_list.push_back( media_entry );
	}

	directory::thumbnail_list.resize( directory::media_list.size() );

	gallery_view_dir_change();
	
	gallery::item_text_size.resize( directory::media_list.size() );
}


void push_notification( const char* msg )
{
	g_notification_queue.emplace_back( msg, app::total_time, NOTIFICATION_DURATION );
}


void notification_draw( float frame_time )
{
	static float time_drawn = 0.f;
	static bool  fade_in    = true;

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

	int width, height;
	SDL_GetWindowSize( app::window, &width, &height );

	ImVec2 notif_pos{};
	notif_pos.x = width / 2;
	notif_pos.y = 40.f;

	// ----------------------------------------

	// pivot aligns it to the center and the bottom of the window
	// ImGui::SetNextWindowPos( notif_pos, 0, ImVec2( 0.5f, 1.0f ) );
	ImGui::SetNextWindowPos( notif_pos, 0, ImVec2( 0.5f, 0.0f ) );

	ImGuiStyle& style        = ImGui::GetStyle();

	ImVec4      bg_color     = style.Colors[ ImGuiCol_FrameBg ];
	ImVec4      border_color = style.Colors[ ImGuiCol_Border ];
	bg_color.w               = 0.75;

	float  max_notif_time    = -1.f;
	// get fadeout time
	size_t count             = std::min( NOTIFICATION_MAX_SHOWN, g_notification_queue.size() );

	//float  fade_in_amount    = std::min( 1.f, time_drawn / NOTIFICATION_FADE_IN_TIME );
	// float  fade_amount    = std::min( 1.f, time_drawn / NOTIFICATION_FADE_IN_TIME );
	float  fade_amount    = 1.f;

	for ( size_t j = 0, i = g_notification_queue.size() - 1;; i--, j++ )
	{
		notification_t& notif = g_notification_queue[ i ];
		max_notif_time        = std::max( max_notif_time, notif.time_remain );

		if ( i == 0 || j == count )
			break;
	}

	if ( max_notif_time < NOTIFICATION_FADE_IN_TIME )
	{
		fade_amount = max_notif_time / NOTIFICATION_FADE_IN_TIME;

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
			notification_t& notif = g_notification_queue[ i ];
			ImVec4          text_color = style.Colors[ ImGuiCol_Text ];

			// nice fade out effect
			if ( notif.time_remain < NOTIFICATION_FADE_IN_TIME )
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


void imgui_draw( float frame_time )
{
	if ( g_gallery_view )
	{
		gallery_view_draw();
	}
	else
	{
		media_view_draw_imgui();
	}

	notification_draw( frame_time );

	ImGui::Render();
}


static bool g_mpv_resume_on_focus = false;


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

	media_entry_t entry = gallery_item_get_media_entry( gallery::cursor );

	// clear mpv
	// mpv_cmd_loadfile( "" );

	if ( entry.type == e_media_type_video )
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
}


void set_view_type_media()
{
	//if ( !g_gallery_view )
	//	return;

	if ( g_image_data.index != gallery::cursor )
		media_view_load();

	if ( g_mpv_resume_on_focus )
	{
		const char* cmd[]   = { "set", "pause", "no", NULL };
		int         cmd_ret = p_mpv_command_async( g_mpv, 0, cmd );
	}

	media_view_fit_in_view();

	g_gallery_view = false;

	update_window_title();
}


bool on_new_file( const fs::path& file_path )
{
	bool is_file = fs_is_file( file_path.string().c_str() );

	if ( is_file )
	{
		// can we open this file?
		if ( !media_check_extension( file_path.string() ) )
			return false;

		directory::queued = file_path;
		return true;
	}
	else if ( fs_is_dir( file_path.string().c_str() ) )
	{
		directory::queued = file_path;
		return true;
	}

	return false;
}


bool drag_drop_recieve_func( const std::vector< fs::path >& files )
{
	if ( files.empty() )
		return false;

	return on_new_file( files[ 0 ] );
}


void style_imgui()
{
	ImGuiStyle& style        = ImGui::GetStyle();
	ImVec4*     colors       = style.Colors;

	style.WindowPadding.x    = 6;
	style.WindowPadding.y    = 6;
	style.ItemSpacing.x      = 6;
	style.ItemSpacing.y      = 6;
	style.ItemInnerSpacing.x = 6;
	style.ItemInnerSpacing.y = 6;

	style.FramePadding.x     = 4;
	style.FramePadding.y     = 4;

	style.ChildRounding      = 3;
	style.FrameRounding      = 3;
	style.GrabRounding       = 3;
	style.PopupRounding      = 3;
	// style.ScrollbarRounding = 3;


	// TEST
#if 0
	style.WindowPadding.x    = 0.f;
	style.WindowPadding.y    = 0.f;
	style.ItemSpacing.x      = 0.f;
	style.ItemSpacing.y      = 0.f;
	style.ItemInnerSpacing.x = 0.f;
	style.ItemInnerSpacing.y = 0.f;
#endif

	colors[ ImGuiCol_FrameBg ]              = ImVec4( 0.00f, 0.21f, 0.52f, 0.54f );
	// colors[ ImGuiCol_WindowBg ]             = ImVec4( 0.06f, 0.06f, 0.06f, 1.00f );
	colors[ ImGuiCol_WindowBg ]             = ImVec4( 0.08f, 0.08f, 0.08f, 1.00f );

	colors[ ImGuiCol_ScrollbarBg ]          = ImVec4( 0.02f, 0.02f, 0.02f, 1.00f );
	colors[ ImGuiCol_ScrollbarGrab ]        = ImVec4( 0.00f, 0.28f, 0.65f, 1.00f );
	colors[ ImGuiCol_ScrollbarGrabHovered ] = ImVec4( 0.00f, 0.43f, 1.00f, 1.00f );
	colors[ ImGuiCol_ScrollbarGrabActive ]  = ImVec4( 0.00f, 0.35f, 0.78f, 1.00f );

	colors[ ImGuiCol_TabHovered ]           = ImVec4( 0.00f, 0.43f, 1.00f, 1.00f );
	colors[ ImGuiCol_Tab ]                  = ImVec4( 0.00f, 0.28f, 0.65f, 1.00f );
	colors[ ImGuiCol_TabSelected ]          = ImVec4( 0.00f, 0.35f, 0.78f, 1.00f );
	colors[ ImGuiCol_TabSelectedOverline ]  = ImVec4( 0.00f, 0.35f, 0.78f, 1.00f );

	colors[ ImGuiCol_TabDimmed ]            = ImVec4( 0.00f, 0.07f, 0.16f, 0.97f );
	colors[ ImGuiCol_TabDimmedSelected ]    = ImVec4( 0.00f, 0.18f, 0.42f, 1.00f );

	colors[ ImGuiCol_Button ]               = ImVec4( 0.00f, 0.35f, 0.77f, 1.00f );
	colors[ ImGuiCol_ButtonHovered ]        = ImVec4( 0.15f, 0.54f, 1.00f, 1.00f );
	colors[ ImGuiCol_ButtonActive ]         = ImVec4( 0.00f, 0.24f, 0.55f, 1.00f );

	colors[ ImGuiCol_CheckMark ]            = ImVec4( 0.00f, 0.46f, 1.00f, 1.00f );
}


void load_default_font( sys_font_data_t& font_data, ImFont*& dst, ImFontConfig& font_cfg, bool load_symbols )
{
	font_cfg.FontLoaderFlags |= ImGuiFreeTypeLoaderFlags_LoadColor;

	// Main Font
	dst = ImGui::GetIO().Fonts->AddFontFromFileTTF( font_data.font_path, font_data.height, &font_cfg );

	#ifdef _WIN32
	// All fonts will be merged into this one above
	font_cfg.MergeMode = true;

	// Japanese Characters
	dst = ImGui::GetIO().Fonts->AddFontFromFileTTF( "C:\\Windows\\Fonts\\YuGothM.ttc", font_data.height, &font_cfg );

	// Symbols/Emoji's
	if ( load_symbols )
	{
		// font_cfg.FontLoaderFlags |= ImGuiFreeTypeLoaderFlags_LoadColor | ImGuiFreeTypeLoaderFlags_Bitmap;
		font_cfg.FontLoaderFlags |= ImGuiFreeTypeLoaderFlags_LoadColor;

		// Segoe UI Symbol
		dst = ImGui::GetIO().Fonts->AddFontFromFileTTF( "C:\\Windows\\Fonts\\seguisym.ttf", font_data.height, &font_cfg );

		//char font_path[ 512 ]{};
		//snprintf( font_path, 512, "%s/seguiemj.ttf", exe_path );

		// ImGui::GetIO().Fonts->AddFontFromFileTTF( font_path, font_data.height, &font_cfg );
		dst = ImGui::GetIO().Fonts->AddFontFromFileTTF( "C:\\Windows\\Fonts\\seguiemj.ttf", font_data.height, &font_cfg );
	}
	#endif
}


void frame_draw_start()
{
	int width, height;
	SDL_GetWindowSize( app::window, &width, &height );

	ImGui::GetIO().DisplaySize.x = width;
	ImGui::GetIO().DisplaySize.y = height;

	ImGui_ImplSDL3_NewFrame();
	ImGui_ImplOpenGL3_NewFrame();
	ImGui::NewFrame();

	glViewport( 0, 0, width, height );
	glClearColor( app::clear_color.x, app::clear_color.y, app::clear_color.z, app::clear_color.w );
	glClear( GL_COLOR_BUFFER_BIT );
}


void frame_draw_end()
{
	if ( !g_gallery_view )
		media_view_draw();

	ImGui_ImplOpenGL3_RenderDrawData( ImGui::GetDrawData() );
	SDL_GL_SwapWindow( app::window );

	if ( g_mpv )
		p_mpv_render_context_report_swap( g_mpv_gl );
}


// called initially on startup and on window resize
void window_quick_draw()
{
	frame_draw_start();

	imgui_draw( app::frame_time );

	app::window_resized = true;
	media_view_window_resize();
	gallery_view_scroll_to_cursor();
	// mpv_window_resize();

	frame_draw_end();
}


bool sdl_window_resize_watcher( void* userdata, SDL_Event* event )
{
	switch ( event->type )
	{
		// Redraw window - Window is being resized
		// NOTE: this is also called when dragging the window around
#ifdef _WIN32
		case SDL_EVENT_WINDOW_EXPOSED:
#endif
		case SDL_EVENT_WINDOW_RESIZED:
		{
			thumbnail_loader_update();
			window_quick_draw();
			break;
		}

		default:
			mpv_sdl_event( *event );
	}

	return true;
}


// return true to exit main loop
// Handle SDL3 Events
bool handle_events()
{
	app::mouse_scrolled_up   = false;
	app::mouse_scrolled_down = false;
	app::window_resized      = false;

	app::mouse_delta[ 0 ]    = 0.f;
	app::mouse_delta[ 1 ]    = 0.f;

	g_drag_drop_files.clear();

	SDL_Event event;
	while ( SDL_PollEvent( &event ) )
	{
		ImGui_ImplSDL3_ProcessEvent( &event );

		switch ( event.type )
		{
			default:
				mpv_sdl_event( event );
				break;

			case SDL_EVENT_MOUSE_WHEEL:
				if ( event.wheel.integer_y > 0 )
					app::mouse_scrolled_up = true;
				else
					app::mouse_scrolled_down = true;

				media_view_scroll_zoom( event.wheel.integer_y );
				break;

			case SDL_EVENT_MOUSE_MOTION:
				app::mouse_pos[ 0 ] = event.motion.x;
				app::mouse_pos[ 1 ] = event.motion.y;
				app::mouse_delta[ 0 ] += event.motion.xrel;
				app::mouse_delta[ 1 ] += event.motion.yrel;
				break;

			case SDL_EVENT_WINDOW_RESIZED:
				int width, height;
				SDL_GetWindowSize( app::window, &width, &height );
				ImGui::GetIO().DisplaySize.x    = width;
				ImGui::GetIO().DisplaySize.y    = height;

				app::window_resized = true;
				media_view_window_resize();
				gallery_view_scroll_to_cursor();
				mpv_window_resize();
				break;

			case SDL_EVENT_WINDOW_FOCUS_GAINED:
				app::window_focused = true;
				break;

			case SDL_EVENT_WINDOW_FOCUS_LOST:
				app::window_focused = false;
				break;

			// The system requests a file open
			case SDL_EVENT_DROP_FILE:
			{
				g_drag_drop_files.push_back( event.drop.data );
				break;
			}

			// text/plain drag-and-drop event
			case SDL_EVENT_DROP_TEXT:
				break;

			// Current set of drops is now complete (NULL filename)
			case SDL_EVENT_DROP_COMPLETE:
			{
				if ( drag_drop_recieve_func( g_drag_drop_files ) )
					SDL_RaiseWindow( app::window );

				break;
			}

			// Position while moving over the window
			case SDL_EVENT_DROP_POSITION:
				break;

			case SDL_EVENT_QUIT:
			case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
				app::running = false;
				return true;
		}
	}

	return false;
}


void main_loop()
{
	bool  run_after_first_loop_hack = true;

	u64   start_time   = sys_get_time_ms();
	u64   current_time = start_time;
	float time         = 0.f;

	while ( app::running )
	{
		// -----------------------------------------------------------------------------------
		// Check MPV Playback

		bool playing_back_video = false;

		if ( !g_gallery_view && gallery::sorted_media.size() )
		{
			media_entry_t entry = gallery_item_get_media_entry( gallery::cursor );

			if ( entry.type == e_media_type_video )
			{
				// check mpv state (SHOULD PROBABLY TRY USING OBSERVE PROPERTY)
				s32 paused = 0;
				p_mpv_get_property( g_mpv, "pause", MPV_FORMAT_FLAG, &paused );

				if ( !paused )
					playing_back_video = true;
			}
		}

		// -----------------------------------------------------------------------------------
		// Update Frame Time

		current_time  = sys_get_time_ms();
		time          = ( current_time / 1000.f ) - ( start_time / 1000.f );

		// don't let the time go too crazy, usually happens when in a breakpoint
		// time                 = std::min( real_time, 0.1f );

		// TODO: GET MONITOR REFRESH RATE
	//	float max_fps = 300.f;
	//
	//	if ( !playing_back_video )
	//	{
	//		// check if we still have more than 2ms till next frame and if so, wait for "1ms"
	//		float min_frame_time = 1.0f / max_fps;
	//		if ( ( min_frame_time - time ) > ( 2.0f / 1000.f ) )
	//			SDL_Delay( 1 );
	//
	//		// framerate is above max
	//		if ( time < min_frame_time )
	//			continue;
	//	}

		app::frame_time = time;
		app::total_time += ( time * 1000.f );

		sys_update();

		// -----------------------------------------------------------------------------------
		// Queued Directory/File to Change to/Load

		if ( !directory::queued.empty() )
		{
			bool is_file = fs_is_file( directory::queued.string().c_str() );

			if ( is_file )
			{
				directory::path = directory::queued.parent_path();
				folder_load_media_list();

				for ( size_t i = 0; i < gallery::sorted_media.size(); i++ )
				{
					if ( gallery_item_get_path( i ) == directory::queued )
					{
						gallery::cursor = i;
						directory::queued.clear();
						//media_view_load();
						set_view_type_media();
						break;
					}
				}
			}
			else
			{
				gallery::cursor = 0;
				directory::path = directory::queued;
				directory::queued.clear();
				gallery_view_scroll_to_cursor();
				folder_load_media_list();
				set_view_type_gallery();
			}

			update_window_title();
		}

		thumbnail_loader_update();

		// -----------------------------------------------------------------------------------
		// Window Events

		if ( handle_events() )
			break;

		if ( SDL_GetWindowFlags( app::window ) & SDL_WINDOW_MINIMIZED )
		{
			app::window_focused = false;
			SDL_Delay( 15 );
			start_time = current_time;
			continue;
		}

		if ( !app::window_focused && !playing_back_video )
		{
			if ( app::config.no_focus_sleep_time > 0 )
				SDL_Delay( app::config.no_focus_sleep_time );
		}

		// never called?
		// if ( SDL_GetWindowFlags( app::window ) & SDL_WINDOW_OCCLUDED )
		// {
		// 	printf( "OCCLUDED\n" );
		// 	SDL_Delay( 8 );
		// }

		// -----------------------------------------------------------------------------------

		frame_draw_start();

		imgui_draw( time );

		media_view_scale_check_timer( time );

		frame_draw_end();

		// -----------------------------------------------------------------------------------

		// delayed startup, stuff that can be loaded after initial draw
		// like if we open an image or video from file explorer, we want this program to open and show it near instantly
		// at least the first frame of it, then we can do this after
		if ( run_after_first_loop_hack )
		{
			icon_preload();
			run_after_first_loop_hack = false;
		}

		start_time = current_time;
	}
}


int main( int argc, char* argv[] )
{
	args_init( argc, argv );

	if ( !sys_init() )
	{
		printf( "Failed to init system backend!\n" );
		return 1;
	}

	u64   start_time   = sys_get_time_ms();
	u64   current_time = start_time;
	float time         = 0.f;

	if ( !SDL_Init( SDL_INIT_EVENTS | SDL_INIT_VIDEO ) )
	{
		printf( "Failed to init SDL\n" );
		return 1;
	}

	app::window = SDL_CreateWindow( "Media Tag System", 1000, 600, SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL );

	if ( !app::window )
	{
		printf( "Failed to create SDL window\n" );
		return 1;
	}

	SDL_SetWindowMinimumSize( app::window, 400, 400 );

	// SDL_SetEventEnabled( SDL_EVENT_DROP_FILE, false );
	// SDL_SetEventEnabled( SDL_EVENT_DROP_TEXT, false );
	// SDL_SetEventEnabled( SDL_EVENT_DROP_BEGIN, false );
	// SDL_SetEventEnabled( SDL_EVENT_DROP_COMPLETE, false );
	// SDL_SetEventEnabled( SDL_EVENT_DROP_POSITION, false );

	sys_set_window( app::window );
	sys_set_receive_drag_drop_func( drag_drop_recieve_func );

	g_gl_context = SDL_GL_CreateContext( app::window );
	
	if ( !g_gl_context )
	{
		printf( "Failed to create GL Context\n" );
		return 1;
	}
	
	SDL_GL_MakeCurrent( app::window, g_gl_context );

	if ( !gladLoadGL() )
	{
		printf( "Failed to load GL\n" );
		return 1;
	}

	IMGUI_CHECKVERSION();

	ImGui::SetAllocatorFunctions( imgui_mem_alloc, imgui_mem_free );

	ImGui::CreateContext();

	if ( !ImGui_ImplSDL3_InitForOpenGL( app::window, g_gl_context ) )
	{
		printf( "Failed to init imgui for sdl3\n" );
		return 1;
	}

	if ( !ImGui_ImplOpenGL3_Init() )
	{
		printf( "Failed to init imgui opengl\n" );
		return 1;
	}
	
	sys_font_data_t font_data = sys_get_font();
	
	if ( font_data.font_path )
	{
		//char* exe_path = sys_get_exe_folder();

		{
			ImFontConfig font_cfg{};
			load_default_font( font_data, font::normal, font_cfg, false );
		}

		{
			ImFontConfig font_cfg{};
			snprintf( font_cfg.Name, 40, "Default - Bold" );
			font_cfg.FontLoaderFlags |= ImGuiFreeTypeLoaderFlags_Bold;
			load_default_font( font_data, font::normal_bold, font_cfg, false );
		}
	
		{
			ImFontConfig font_cfg{};
			snprintf( font_cfg.Name, 40, "Default - Oblique" );
			font_cfg.FontLoaderFlags |= ImGuiFreeTypeLoaderFlags_Oblique;
			load_default_font( font_data, font::normal_italic, font_cfg, false );
		}

		ImGui_ImplOpenGL3_CreateDeviceObjects();

		//free( exe_path );
	}

	SDL_GL_GetSwapInterval( &app::config.vsync );

	if ( !config_load() )
	{
		printf( "Failed to load config, using defaults\n" );
	}

	SDL_GL_SetSwapInterval( app::config.vsync );

	// bool gl_ret = SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 0 );

	ImGuiIO& io = ImGui::GetIO();

	int      width, height;
	SDL_GetWindowSize( app::window, &width, &height );
	io.DisplaySize.x   = width;
	io.DisplaySize.y   = height;

	style_imgui();

	if ( !load_mpv_dll() )
	{
		printf( "Failed to load MPV\n" );
	}
	else
	{
		if ( !start_mpv() )
			printf( "Failed to start MPV\n" );
	}

	if ( !thumbnail_loader_init() )
	{
		printf( "Failed to init thumbnail loader\n" );
		return 1;
	}

	media_view_init();
	
	glGenTextures( 1, &g_image_data.texture );
	glGenTextures( 1, &g_image_scaled_data.texture );

	// ----------------------------------------------------------------

	directory::queued = sys_get_cwd();

	if ( argc > 1 )
	{
		// take the first path here
		for ( int i = 1; i < argc; i++ )
		{
			char* arg = argv[ i ];

			if ( fs_exists( arg ) )
			{
				if ( fs_is_dir( arg ) )
				{
					directory::queued = arg;
					//g_gallery_view  = true;
				}
				else
				{
					on_new_file( arg );
					//g_gallery_view = false;
				}

				break;
			}
		}
	}

//	if ( !directory::queued.empty() )
//	{
//		directory::path = directory::queued;
//
//		folder_load_media_list();
//		directory::queued.clear();
//	}

	// ----------------------------------------------------------------

	if ( !SDL_AddEventWatch( sdl_window_resize_watcher, nullptr ) )
	{
		printf( "Failed to add SDL Event Watch\n" );
	}

	window_quick_draw();

	current_time = sys_get_time_ms();
	time         = (current_time / 1000.f) - (start_time / 1000.f);
	start_time   = current_time;

	printf( "%.3f STARTUP TIME\n", time );

	// -----------------------------------------------------------------------------------

	main_loop();

	// -----------------------------------------------------------------------------------

	thumbnail_loader_shutdown();
	media_view_shutdown();
	icon_free();

	SDL_GL_DestroyContext( g_gl_context );

	args_free();
	sys_shutdown();
	SDL_Quit();
	return 0;
}

