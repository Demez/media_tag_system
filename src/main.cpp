#include "main.h"

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_opengl3.h"
#include "imgui_freetype.h"
#include "imgui_internal.h"

#define DATABASE_FILE "tag_database.txt"

std::vector< IImageLoader* >       g_codecs;

SDL_Window*                  g_main_window = nullptr;
// SDL_Renderer*              g_main_renderer       = nullptr;
SDL_GLContext                g_gl_context;

bool                         g_running             = true;
bool                         g_gallery_view        = false;
bool                         g_mpv_ready           = false;

bool                         g_mouse_scrolled_up   = false;
bool                         g_mouse_scrolled_down = false;
bool                         g_window_resized      = false;

ivec2                        g_mouse_delta{};
ivec2                        g_mouse_pos{};

// Main Image
image_t                      g_image;
main_image_data_t            g_image_data;
size_t                       g_media_index = 0;

// Previous Image to Free
main_image_data_t            g_image_data_free;

fs::path                     g_folder;
fs::path                     g_folder_queued;
std::vector< media_entry_t > g_folder_media_list;
std::vector< h_thumbnail >   g_folder_thumbnail_list;
size_t                       g_gallery_index = 0;


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


void image_register_codec( IImageLoader* codec )
{
	if ( codec )
		g_codecs.push_back( codec );
}


bool image_load( const fs::path& path, image_load_info_t& load_info )
{
	std::string path_std_string = path.string();
	const char* path_str        = path_std_string.c_str();
	std::string ext_str         = path.extension().string();

	if ( !fs_is_file( path_str ) || fs_file_size( path_str ) == 0 )
	{
		printf( "File is Empty or Doesn't exist: %s\n", path_str );
		return false;
	}

	bool allocated_image = false;

	if ( !load_info.image )
	{
		load_info.image = ch_calloc< image_t >( 1 );

		if ( !load_info.image )
		{
			printf( "Failed to allocate image data!\n" );
			return false;
		}

		allocated_image = true;
	}

	size_t file_len  = 0;
	char*  file_data = fs_read_file( path.string().c_str(), &file_len );

	if ( !file_data )
	{
		printf( "Failed to read file: %s\n", path_str );
		return false;
	}

	bool loaded_image = false;

	for ( IImageLoader* codec : g_codecs )
	{
		// if ( !codec->check_extension( ext_str ) )
		if ( !codec->check_extension( path_str ) )
			continue;

		loaded_image = codec->image_load( path, load_info, file_data, file_len );

		if ( loaded_image )
			break;
	}

	free( file_data );

	if ( !loaded_image && allocated_image )
	{
		free( load_info.image );
		load_info.image = nullptr;
	}

	return loaded_image;
}


bool image_check_extension( std::string_view ext )
{
	for ( IImageLoader* codec : g_codecs )
	{
		if ( codec->check_extension( ext ) )
			return true;
	}

	return false;
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
			snprintf( buf, 512, "Media Tag System - %s", g_folder_media_list[ g_gallery_index ].path.string().c_str() );
		else
			snprintf( buf, 512, "Media Tag System" );
	}

	SDL_SetWindowTitle( g_main_window, buf );
}


void folder_load_media_list()
{
	g_folder_media_list.clear();
	g_folder_thumbnail_list.clear();

	g_folder_media_list.reserve( 5000 );
	g_folder_thumbnail_list.reserve( 5000 );

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
		else
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
}


void imgui_draw()
{
	if ( g_gallery_view )
	{
		gallery_view_draw();
	}
	else
	{
		media_view_draw_imgui();
	}
}


void gallery_view_toggle()
{
	if ( g_gallery_view )
	{
		if ( g_media_index != g_gallery_index )
			media_view_load();
	}
	else
	{
		// clear mpv
		mpv_cmd_loadfile( "" );
		gallery_view_scroll_to_selected();
	}

	g_gallery_view = !g_gallery_view;

	update_window_title();
}


void gl_update_texture( GLuint texture, image_t* image )
{
	glBindTexture( GL_TEXTURE_2D, texture );
	
	// disable wrapping
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );

	// Setup filtering parameters for display
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );

	// Upload pixels into texture
	// glPixelStorei( GL_UNPACK_ROW_LENGTH, 0 );

	//if ( image->bytes_per_pixel > 1 )
	glPixelStorei( GL_UNPACK_ROW_LENGTH, image->pitch / image->bytes_per_pixel );
	// glPixelStorei( GL_UNPACK_ROW_LENGTH, image->width );

	//glPixelStorei( GL_UNPACK_ROW_LENGTH, 0 );
	glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );

	if ( image->format == GL_RGBA16 )
	{
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA16, image->width, image->height, 0, GL_RGBA, GL_UNSIGNED_SHORT, (u16*)image->frame[ 0 ] );
	}
	else if ( image->format == GL_R16UI )
	{
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGB16, image->width, image->height, 0, GL_LUMINANCE, GL_UNSIGNED_SHORT, (u16*)image->frame[ 0 ] );
	}
	else if ( image->format == GL_R16I )
	{
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGB16, image->width, image->height, 0, GL_LUMINANCE, GL_SHORT, (s16*)image->frame[ 0 ] );
	}
	else if ( image->format == GL_R8 )
	{
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGB, image->width, image->height, 0, GL_RED, GL_UNSIGNED_BYTE, image->frame[ 0 ] );
	}
	else if ( image->format == GL_RGBA32F )
	{
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA32F, image->width, image->height, 0, GL_RGBA, GL_FLOAT, image->frame[ 0 ] );
	}
	else
	{
		glTexImage2D( GL_TEXTURE_2D, 0, image->format, image->width, image->height, 0, image->format, GL_UNSIGNED_BYTE, image->frame[ 0 ] );
	}

	auto err = glGetError();

	if ( err != 0 )
		printf( "FUCK: %d\n", err );

	glBindTexture( GL_TEXTURE_2D, 0 );
}


GLuint gl_upload_texture( image_t* image )
{
	GLuint image_texture;
	glGenTextures( 1, &image_texture );

	gl_update_texture( image_texture, image );

	return image_texture;
}


void gl_free_texture( GLuint texture )
{
	glDeleteTextures( 1, &texture );
}


void on_new_file( char* file )
{
	fs::path file_path = file;

	// TODO: CHECK IF WE CAN OPEN THIS FILE FIRST

	g_folder           = file_path.parent_path();

	folder_load_media_list();

	for ( size_t i = 0; i < g_folder_media_list.size(); i++ )
	{
		if ( g_folder_media_list[ i ].path == file_path )
		{
			g_gallery_index = i;
			g_folder_queued.clear();
			return;
		}
	}

	// probably not a supported file
	g_gallery_index = 0;
}



static const char* g_icon_names[] = {
	"none",
	"invalid",
	"folder",
	"loading",
	"video",
};



static const char* g_icon_paths[] = {
	"icons/none.png",
	"icons/invalid.png",
	"icons/folder.png",
	"icons/loading.png",
	"icons/video.png",
};


static image_t g_icon_image[ e_icon_count ]{};
static GLuint  g_icon_texture[ e_icon_count ]{};

static_assert( ARR_SIZE( g_icon_names ) == e_icon_count );
static_assert( ARR_SIZE( g_icon_paths ) == e_icon_count );


bool icon_preload()
{
	char* exe_dir = sys_get_exe_folder();
	fs::path exe_path = exe_dir;
	free( exe_dir );

	for ( u8 i = 0; i < e_icon_count; i++ )
	{
		image_load_info_t load_info{};
		load_info.image = &g_icon_image[ i ];

		if ( !image_load( exe_path / g_icon_paths[ i ], load_info ) )
		{
			printf( "Failed to load %s icon \"%s\"\n", g_icon_names[ i ], g_icon_paths[ i ] );
			continue;
		}

		g_icon_texture[ i ] = gl_upload_texture( &g_icon_image[ i ] );

		if ( !g_icon_texture[ i ] )
		{
			printf( "Failed to upload %s icon \"%s\"\n", g_icon_names[ i ], g_icon_paths[ i ] );
			continue;
		}

		free( g_icon_image[ i ].frame[ 0 ] );
		g_icon_image[ i ].frame.clear();

		printf( "Loaded icon %s\n", g_icon_names[ i ] );
	}

	return true;
}


void icon_free()
{
	for ( u8 i = 0; i < e_icon_count; i++ )
	{
		gl_free_texture( g_icon_texture[ i ] );
	}
}


image_t* icon_get_image( e_icon icon_type )
{
	if ( icon_type > e_icon_count )
		return {};

	return &g_icon_image[ icon_type ];
}


ImTextureRef icon_get_imtexture( e_icon icon_type )
{
	if ( icon_type > e_icon_count )
		return {};

	return static_cast< ImTextureRef >( g_icon_texture[ icon_type ] );
}


void style_imgui()
{
	ImGuiStyle& style        = ImGui::GetStyle();

	style.WindowPadding.x    = 6;
	style.WindowPadding.y    = 6;
	style.ItemSpacing.x      = 6;
	style.ItemSpacing.y      = 6;
	style.ItemInnerSpacing.x = 6;
	style.ItemInnerSpacing.y = 6;

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
}


int main( int argc, char* argv[] )
{
	args_init( argc, argv );
	sys_init();

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

		ImFontConfig font_cfg{};
		font_cfg.FontLoaderFlags |= ImGuiFreeTypeLoaderFlags_LoadColor;

		// Main Font
		ImGui::GetIO().Fonts->AddFontFromFileTTF( font_data.font_path, font_data.height, &font_cfg );

		// All fonts will be merged into this one above
		font_cfg.MergeMode = true;

		// Japanese Characters
		ImGui::GetIO().Fonts->AddFontFromFileTTF( "C:\\Windows\\Fonts\\YuGothM.ttc", font_data.height, &font_cfg );

		// Symbols/Emoji's
		font_cfg.FontLoaderFlags |= ImGuiFreeTypeLoaderFlags_LoadColor | ImGuiFreeTypeLoaderFlags_Bitmap;

		// Segoe UI Symbol
		ImGui::GetIO().Fonts->AddFontFromFileTTF( "C:\\Windows\\Fonts\\seguisym.ttf", font_data.height, &font_cfg );

		//char font_path[ 512 ]{};
		//snprintf( font_path, 512, "%s/seguiemj.ttf", exe_path );

		// ImGui::GetIO().Fonts->AddFontFromFileTTF( font_path, font_data.height, &font_cfg );
		ImGui::GetIO().Fonts->AddFontFromFileTTF( "C:\\Windows\\Fonts\\seguiemj.ttf", font_data.height, &font_cfg );
	
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
	
	glGenTextures( 1, &g_image_data.texture );

	icon_preload();

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
				}
				else
				{
					on_new_file( arg );
				}

				break;
			}
		}
	}

	if ( !g_folder_queued.empty() )
	{
		g_folder = g_folder_queued;

		folder_load_media_list();
		g_folder_queued.clear();
	}

	media_view_load();

	// ----------------------------------------------------------------

	ImVec4 clear_color = ImVec4( 0.15f, 0.15f, 0.15f, 1.00f );

	while ( g_running )
	{
		// called so mpv doesn't get flooded with too many events, and becomes unresponsive
		mpv_event* video_event = p_mpv_wait_event( g_mpv, 0.01f );

		// don't go full speed lol
		// SDL_Delay( 5 );
		// SDL_Delay( 1 );

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
			SDL_Delay( 10 );
			continue;
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
		glClearColor( clear_color.x, clear_color.y, clear_color.z, clear_color.w );
		glClear( GL_COLOR_BUFFER_BIT );

		imgui_draw();

		ImGui::Render();

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
	}

	icon_free();

	SDL_GL_DestroyContext( g_gl_context );

	args_free();
	sys_shutdown();
	SDL_Quit();
	return 0;
}

