#include "main.h"

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_opengl3.h"
#include "imgui_freetype.h"
#include "imgui_internal.h"

#include <chrono>

// ImVec4                       g_clear_color = ImVec4( 0.15f, 0.15f, 0.15f, 1.00f );
ImVec4                       g_clear_color = ImVec4( 0.05f, 0.05f, 0.05f, 1.00f );

SDL_Window*                  g_main_window = nullptr;
SDL_GLContext                g_gl_context;

double                       g_total_time;
float                        g_frame_time;

bool                         g_running             = true;
bool                         g_window_focused      = false;
bool                         g_gallery_view        = false;
bool                         g_mpv_ready           = false;

bool                         g_mouse_scrolled_up   = false;
bool                         g_mouse_scrolled_down = false;
bool                         g_window_resized      = false;

ivec2                        g_mouse_delta{};
ivec2                        g_mouse_pos{};

// Main Image
image_t                      g_image;
image_t                      g_image_scaled;
main_image_data_t            g_image_data;
main_image_data_t            g_image_scaled_data;
size_t                       g_image_scaled_index = 0;
size_t                       g_media_index = 0;

// Previous Image to Free
main_image_data_t            g_image_data_free;

fs::path                     g_folder;
fs::path                     g_folder_queued;
std::vector< media_entry_t > g_folder_media_list;
std::vector< h_thumbnail >   g_folder_thumbnail_list;
size_t                       g_gallery_index = 0;

extern bool                  g_gallery_item_size_changed;
extern std::vector< ImVec2 > g_gallery_item_text_size;

ImFont*                      g_default_font        = nullptr;
ImFont*                      g_default_font_bold   = nullptr;
ImFont*                      g_default_font_italic = nullptr;

struct notification_t
{
	std::string msg;
	double      time_added;
	float       time_remain;
};

constexpr float               NOTIFICATION_DURATION     = 5;
constexpr float               NOTIFICATION_FADE_IN_TIME = 0.5;
constexpr size_t              NOTIFICATION_MAX_SHOWN    = 5;

std::vector< notification_t > g_notification_queue;


// Check the function FindHoveredWindowEx() in imgui.cpp to see if you need to update this when updating imgui
bool mouse_hovering_imgui_window()
{
	ImGuiContext& g = *ImGui::GetCurrentContext();

	ImVec2        imMousePos{ (float)g_mouse_pos[ 0 ], (float)g_mouse_pos[ 1 ] };

	ImGuiWindow*  hovered_window                     = NULL;
	ImGuiWindow*  hovered_window_under_moving_window = NULL;

	if ( g.MovingWindow && !( g.MovingWindow->Flags & ImGuiWindowFlags_NoMouseInputs ) )
		hovered_window = g.MovingWindow;

	ImVec2 padding_regular    = g.Style.TouchExtraPadding;
	ImVec2 padding_for_resize = ImMax( g.Style.TouchExtraPadding, ImVec2( g.Style.WindowBorderHoverPadding, g.Style.WindowBorderHoverPadding ) );
	for ( int i = g.Windows.Size - 1; i >= 0; i-- )
	{
		ImGuiWindow* window = g.Windows[ i ];
		IM_MSVC_WARNING_SUPPRESS( 28182 );  // [Static Analyzer] Dereferencing NULL pointer.
		if ( !window->WasActive || window->Hidden )
			continue;
		if ( window->Flags & ImGuiWindowFlags_NoMouseInputs )
			continue;

		// Using the clipped AABB, a child window will typically be clipped by its parent (not always)
		ImVec2 hit_padding = ( window->Flags & ( ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize ) ) ? padding_regular : padding_for_resize;
		if ( !window->OuterRectClipped.ContainsWithPad( imMousePos, hit_padding ) )
			continue;

		// Support for one rectangular hole in any given window
		// FIXME: Consider generalizing hit-testing override (with more generic frame, callback, etc.) (#1512)
		if ( window->HitTestHoleSize.x != 0 )
		{
			ImVec2 hole_pos( window->Pos.x + (float)window->HitTestHoleOffset.x, window->Pos.y + (float)window->HitTestHoleOffset.y );
			ImVec2 hole_size( (float)window->HitTestHoleSize.x, (float)window->HitTestHoleSize.y );
			if ( ImRect( hole_pos, hole_pos + hole_size ).Contains( imMousePos ) )
				continue;
		}

		//if ( find_first_and_in_any_viewport )
		//{
		//	hovered_window = window;
		//	break;
		//}
		//else
		{
			if ( hovered_window == NULL )
				hovered_window = window;
			IM_MSVC_WARNING_SUPPRESS( 28182 );  // [Static Analyzer] Dereferencing NULL pointer.
			if ( hovered_window_under_moving_window == NULL && ( !g.MovingWindow || window->RootWindow != g.MovingWindow->RootWindow ) )
				hovered_window_under_moving_window = window;
			if ( hovered_window && hovered_window_under_moving_window )
				break;
		}
	}

	return hovered_window;
}


void update_window_title()
{
	char buf[ 512 ];

	if ( g_gallery_view )
	{
		snprintf( buf, 512, "Media Tag System - %s", g_folder.string().c_str() );
	}
	else
	{
		if ( g_folder_media_list.size() > g_gallery_index )
			snprintf( buf, 512, "Media Tag System [%d / %d] - %s", g_gallery_index, g_gallery_items.size(), gallery_item_get_path_string( g_gallery_index ).c_str() );
		else
			snprintf( buf, 512, "Media Tag System" );
	}

	SDL_SetWindowTitle( g_main_window, buf );
}


void folder_load_media_list()
{
	thumbnail_clear_cache();

	g_folder_media_list.clear();
	g_folder_thumbnail_list.clear();

	g_folder_media_list.reserve( 5000 );
	g_folder_thumbnail_list.reserve( 5000 );

	g_gallery_item_size_changed = true;
	g_gallery_item_text_size.clear();

	for ( const auto& entry : fs::directory_iterator( g_folder ) )
	{
		const fs::path& path = entry.path();

		if ( entry.is_directory() )
		{
			g_folder_media_list.emplace_back( path, path.filename().string(), e_media_type_directory );
			continue;
		}

		const fs::path& ext       = path.extension();
		e_media_type    type      = e_media_type_none;
		// bool            valid_ext = image_check_extension( ext.string() );
		bool            valid_ext = image_check_extension( path.filename().string() );

		// Image Formats
	//	valid_ext |= ext == ".jpg";
	//	valid_ext |= ext == ".jpeg";
	//	valid_ext |= ext == ".png";
	//	valid_ext |= ext == ".gif";

		if ( valid_ext )
		{
			type = e_media_type_image;
		}
		else if ( g_mpv )
		{
			// Video Formats
			valid_ext |= ext == ".mp4";
			valid_ext |= ext == ".mkv";
			valid_ext |= ext == ".webm";
			valid_ext |= ext == ".mov";
			valid_ext |= ext == ".3gp";
			valid_ext |= ext == ".avi";
	
			if ( valid_ext )
			{
				type = e_media_type_video;
			}
		}

		if ( !valid_ext )
			continue;

		g_folder_media_list.emplace_back( path, path.filename().string(), type );
		// g_folder_thumbnail_list.push_back( UINT32_MAX );
	}

	g_folder_thumbnail_list.resize( g_folder_media_list.size() );

	gallery_view_dir_change();
	
	g_gallery_item_text_size.resize( g_folder_media_list.size() );
}


void push_notification( const char* msg )
{
	g_notification_queue.emplace_back( msg, g_total_time, NOTIFICATION_DURATION );
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
	SDL_GetWindowSize( g_main_window, &width, &height );

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

	ImGui::SetNextWindowFocus();

	if ( ImGui::Begin( "##notif", 0, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing ) )
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


void gallery_view_toggle()
{
	static bool   mpv_resume_on_focus = false;
	media_entry_t entry      = gallery_item_get_media_entry( g_gallery_index );

	if ( g_gallery_view )
	{
		if ( g_media_index != g_gallery_index )
			media_view_load();

		if ( mpv_resume_on_focus )
		{
			const char* cmd[]   = { "set", "pause", "no", NULL };
			int         cmd_ret = p_mpv_command_async( g_mpv, 0, cmd );
		}

		media_view_fit_in_view();
	}
	else
	{
		// clear mpv
		// mpv_cmd_loadfile( "" );

		if ( entry.type == e_media_type_video )
		{
			s32 paused = 0;
			p_mpv_get_property( g_mpv, "pause", MPV_FORMAT_FLAG, &paused );
			mpv_resume_on_focus = !paused;

			const char* cmd[]   = { "set", "pause", "yes", NULL };
			int         cmd_ret = p_mpv_command_async( g_mpv, 0, cmd );
		}

		gallery_view_scroll_to_selected();
	}

	g_gallery_view = !g_gallery_view;

	update_window_title();
}


void on_new_file( char* file )
{
	fs::path file_path = file;

	// TODO: CHECK IF WE CAN OPEN THIS FILE FIRST

	g_folder           = file_path.parent_path();

	folder_load_media_list();

	for ( size_t i = 0; i < g_gallery_items.size(); i++ )
	{
		if ( gallery_item_get_path( i ) == file_path )
		{
			g_gallery_index = i;
			g_folder_queued.clear();
			return;
		}
	}

	// probably not a supported file
	g_gallery_index = 0;
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
}


int main( int argc, char* argv[] )
{
	// printf( "Using mimalloc version %d\n", mi_version() );

	args_init( argc, argv );

	if ( !sys_init() )
	{
		printf( "Failed to init system backend!\n" );
		return 1;
	}

	if ( !SDL_Init( SDL_INIT_EVENTS | SDL_INIT_VIDEO ) )
	{
		printf( "Failed to init SDL\n" );
		return 1;
	}

	g_main_window = SDL_CreateWindow( "Media Tag System", 1600, 900, SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL );

	if ( !g_main_window )
	{
		printf( "Failed to create SDL window\n" );
		return 1;
	}

	SDL_SetWindowMinimumSize( g_main_window, 640, 480 );

	g_gl_context = SDL_GL_CreateContext( g_main_window );
	
	if ( !g_gl_context )
	{
		printf( "Failed to create GL Context\n" );
		return 1;
	}
	
	SDL_GL_MakeCurrent( g_main_window, g_gl_context );

	if ( !gladLoadGL() )
	{
		printf( "Failed to load GL\n" );
		return 1;
	}

	IMGUI_CHECKVERSION();

	ImGui::SetAllocatorFunctions( imgui_mem_alloc, imgui_mem_free );

	ImGui::CreateContext();

	if ( !ImGui_ImplSDL3_InitForOpenGL( g_main_window, g_gl_context ) )
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
			load_default_font( font_data, g_default_font, font_cfg, false );
		}

		{
			ImFontConfig font_cfg{};
			snprintf( font_cfg.Name, 40, "Default - Bold" );
			font_cfg.FontLoaderFlags |= ImGuiFreeTypeLoaderFlags_Bold;
			load_default_font( font_data, g_default_font_bold, font_cfg, false );
		}
	
		{
			ImFontConfig font_cfg{};
			snprintf( font_cfg.Name, 40, "Default - Oblique" );
			font_cfg.FontLoaderFlags |= ImGuiFreeTypeLoaderFlags_Oblique;
			load_default_font( font_data, g_default_font_italic, font_cfg, false );
		}

		ImGui_ImplOpenGL3_CreateDeviceObjects();

		//free( exe_path );
	}

	ImGuiIO& io = ImGui::GetIO();

	int      width, height;
	SDL_GetWindowSize( g_main_window, &width, &height );
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
		else
			g_mpv_ready = true;
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

	g_folder_queued = sys_get_cwd();

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
					g_folder_queued = arg;
					g_gallery_view  = true;
				}
				else
				{
					on_new_file( arg );
					g_gallery_view = false;
				}

				break;
			}
		}
	}

//	if ( !g_folder_queued.empty() )
//	{
//		g_folder = g_folder_queued;
//
//		folder_load_media_list();
//		g_folder_queued.clear();
//	}

	bool run_after_first_loop_hack = true;

	// ----------------------------------------------------------------


	auto   start_time                = std::chrono::high_resolution_clock::now();
	auto   current_time              = start_time;
	float  time                      = 0.f;

	while ( g_running )
	{
		sys_update();

		// Update Frame Time
		{
			current_time         = std::chrono::high_resolution_clock::now();
			time                 = std::chrono::duration< float, std::chrono::seconds::period >( current_time - start_time ).count();

			// don't let the time go too crazy, usually happens when in a breakpoint
			time                 = std::min( time, 0.1f );

			g_frame_time         = time;
			g_total_time += time;

			// TODO: GET MONITOR REFRESH RATE
			float fps_limit      = 144.f;
			float max_fps        = CLAMP( fps_limit, 10.f, 5000.f );

			// check if we still have more than 2ms till next frame and if so, wait for "1ms"
			float min_frame_time = 1.0f / max_fps;
			if ( ( min_frame_time - time ) > ( 2.0f / 1000.f ) )
				SDL_Delay( 1 );

			// framerate is above max
			if ( time < min_frame_time )
				continue;
		}

		if ( !g_folder_queued.empty() )
		{
			if ( fs_is_dir( g_folder_queued.string().c_str() ) )
			{
				g_folder = g_folder_queued;
				folder_load_media_list();
			}
			else
			{
				gallery_view_dir_change();
			}

			g_folder_queued.clear();
		}
		
		thumbnail_loader_update();

		g_mouse_scrolled_up   = false;
		g_mouse_scrolled_down = false;
		g_window_resized      = false;

		//auto      startTime = std::chrono::high_resolution_clock::now();

		g_mouse_delta[ 0 ]    = 0.f;
		g_mouse_delta[ 1 ]    = 0.f;

		// Handle Events
		SDL_Event event;
		while ( SDL_PollEvent( &event ) )
		{
			ImGui_ImplSDL3_ProcessEvent( &event );

			switch ( event.type )
			{
				case SDL_EVENT_MOUSE_WHEEL:
					if ( event.wheel.integer_y > 0 )
						g_mouse_scrolled_up = true;
					else
						g_mouse_scrolled_down = true;

					media_view_scroll_zoom( event.wheel.integer_y );
					break;

				case SDL_EVENT_MOUSE_MOTION:
					g_mouse_pos[ 0 ] = event.motion.x;
					g_mouse_pos[ 1 ] = event.motion.y;
					g_mouse_delta[ 0 ] += event.motion.xrel;
					g_mouse_delta[ 1 ] += event.motion.yrel;
					break;

				case SDL_EVENT_WINDOW_RESIZED:
					int width, height;
					SDL_GetWindowSize( g_main_window, &width, &height );
					io.DisplaySize.x = width;
					io.DisplaySize.y = height;

					g_window_resized = true;
					media_view_window_resize();
					gallery_view_scroll_to_selected();
					mpv_window_resize();
					break;

				case SDL_EVENT_QUIT:
				case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
					g_running = false;
					break;
			}
		}

		if ( SDL_GetWindowFlags( g_main_window ) & SDL_WINDOW_MINIMIZED )
		{
			g_window_focused = false;
			SDL_Delay( 10 );
			continue;
		}

		if ( SDL_GetWindowFlags( g_main_window ) & SDL_WINDOW_INPUT_FOCUS )
		{
			g_window_focused = true;
		}
		else
		{
			g_window_focused = false;
			SDL_Delay( 5 );
		}

		ImGui::NewFrame();
		ImGui_ImplSDL3_NewFrame();
		ImGui_ImplOpenGL3_NewFrame();

		bool show_frame_time = false;
		
		if ( ImGui::IsKeyPressed( ImGuiKey_Enter, false ) )
		{
			gallery_view_toggle();
		}

		int width, height;
		SDL_GetWindowSize( g_main_window, &width, &height );

		glViewport( 0, 0, width, height );
		glClearColor( g_clear_color.x, g_clear_color.y, g_clear_color.z, g_clear_color.w );
		glClear( GL_COLOR_BUFFER_BIT );

		imgui_draw( time );

		media_view_scale_check_timer( time );

		if ( !g_gallery_view )
		{
			media_view_draw();
		}

		ImGui_ImplOpenGL3_RenderDrawData( ImGui::GetDrawData() );

		SDL_GL_SwapWindow( g_main_window );

		//auto  currentTime = std::chrono::high_resolution_clock::now();
		//float time     = std::chrono::duration< float, std::chrono::seconds::period >( currentTime - startTime ).count();
		//if ( show_frame_time )
		// printf( "%f FRAMETIME\n", time );

		// startup hack
		if ( run_after_first_loop_hack )
		{
			icon_preload();

			if ( !g_gallery_view )
				media_view_load();

			run_after_first_loop_hack = false;
		}

		start_time = current_time;
	}

	media_view_shutdown();
	icon_free();

	SDL_GL_DestroyContext( g_gl_context );

	args_free();
	sys_shutdown();
	SDL_Quit();
	return 0;
}

