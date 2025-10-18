#include "main.h"

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_opengl3.h"

#define DATABASE_FILE "tag_database.txt"

// TEMPORARY
ICodec*                      g_test_codec  = nullptr;

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
size_t                       g_image_index = 0;

// Previous Image to Free
main_image_data_t            g_image_data_free;

fs::path                     g_folder;
fs::path                     g_folder_queued;
std::vector< media_entry_t > g_folder_media_list;
std::vector< h_thumbnail >   g_folder_thumbnail_list;
size_t                       g_folder_index = 0;


void register_codec( ICodec* codec )
{
	g_test_codec = codec;
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
		if ( g_folder_media_list.size() > g_folder_index )
			snprintf( buf, 512, "Media Tag System - %s", g_folder_media_list[ g_folder_index ].path.string().c_str() );
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
		bool            valid_ext = false;

		// Image Formats
		valid_ext |= ext == ".jpg";
		valid_ext |= ext == ".jpeg";

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
		if ( g_image_index != g_folder_index )
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
	glPixelStorei( GL_UNPACK_ROW_LENGTH, 0 );
	glTexImage2D( GL_TEXTURE_2D, 0, image->format, image->width, image->height, 0, image->format, GL_UNSIGNED_BYTE, image->data );

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
			g_folder_index = i;
			break;
		}
	}

	// probably not a supported file
	g_folder_index = 0;
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
		ImGui::GetIO().Fonts->AddFontFromFileTTF( font_data.font_path, font_data.height, nullptr );
	
		ImGui_ImplOpenGL3_CreateDeviceObjects();
	}

	ImGuiIO& io = ImGui::GetIO();

	int      width, height;
	SDL_GetWindowSize( g_main_window, &width, &height );
	io.DisplaySize.x   = width;
	io.DisplaySize.y   = height;

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

	// ----------------------------------------------------------------

	g_folder_queued = sys_get_cwd();

	if ( argc > 1 )
	{
		// take the first path here
		for ( int i = 0; i < argc; i++ )
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
		// don't go full speed lol
		// SDL_Delay( 5 );
		SDL_Delay( 1 );

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

		ImGui::ShowDemoWindow();
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

	SDL_GL_DestroyContext( g_gl_context );

	args_free();
	sys_shutdown();
	SDL_Quit();
	return 0;
}

