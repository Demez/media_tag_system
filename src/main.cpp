#include "main.h"

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"

#define DATABASE_FILE "tag_database.txt"

// TEMPORARY
ICodec*                    g_test_codec         = nullptr;

fs::path                   TEST_IMAGE           = "D:\\demez_archive\\media\\downloads\\art\\[bsky] camothetiger.bsky.social—2025.04.14—3lmqydllpak2a—bafkreidjzm36t5zwx7drrmioalhswola36t3hngwmtqbolfriogffzclbe.jpg";
// fs::path TEST_IMAGE      = "D:\\demez_archive\\media\\downloads\\art\\WsWLHtGJ_400x400.jpg";
fs::path                   TEST_FOLDER          = "D:\\demez_archive\\media\\downloads\\art";
// fs::path                   TEST_FOLDER          = "D:\\demez_archive\\media\\downloads\\_unsorted";
// fs::path                   TEST_FOLDER          = "D:\\demez_archive\\media\\downloads\\photos_furry";


SDL_Window*                g_main_window        = nullptr;
SDL_Renderer*              g_main_renderer      = nullptr;

bool                       g_running            = true;
bool                       g_gallery_view       = false;

bool                       g_mouse_scrolled_up   = false;
bool                       g_mouse_scrolled_down = false;

// Main Image
image_t                    g_image;
main_image_data_t          g_image_data;
size_t                     g_image_index = 0;

// Previous Image to Free
main_image_data_t          g_image_data_free;

std::vector< fs::path >    g_folder_media_list;
std::vector< h_thumbnail > g_folder_thumbnail_list;
size_t                     g_folder_index = 0;


void register_codec( ICodec* codec )
{
	g_test_codec = codec;
}


void folder_load_media_list()
{
	g_folder_media_list.clear();
	g_folder_thumbnail_list.clear();

	g_folder_media_list.reserve( 5000 );
	g_folder_thumbnail_list.reserve( 5000 );

	for ( const auto& entry : fs::directory_iterator( TEST_FOLDER ) )
	{
		if ( !entry.is_regular_file() )
			continue;

		const fs::path& path = entry.path();

		if ( path.extension() == ".jpg" || path.extension() == ".jpeg" )
		{
			g_folder_media_list.push_back( path );
			// g_folder_thumbnail_list.push_back( UINT32_MAX );
		}
	}

	g_folder_thumbnail_list.resize( g_folder_media_list.size() );
}


void imgui_draw()
{
	if ( g_gallery_view )
	{
		gallery_view_draw();
	}
	else
	{
		media_view_input();
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
		gallery_view_scroll_to_selected();
	}

	g_gallery_view = !g_gallery_view;
}


int main( int argc, char* argv[] )
{
	if ( !SDL_Init( SDL_INIT_EVENTS | SDL_INIT_VIDEO ) )
	{
		printf( "Failed to init SDL\n" );
		return 1;
	}

	args_init( argc, argv );
	sys_init();

	if ( !SDL_CreateWindowAndRenderer( "Image Tag System", 1600, 900, SDL_WINDOW_RESIZABLE, &g_main_window, &g_main_renderer ) )
	{
		printf( "Failed to create SDL window and renderer\n" );
		return 1;
	}

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	if ( !ImGui_ImplSDL3_InitForSDLRenderer( g_main_window, g_main_renderer ) )
	{
		printf( "Failed to init imgui for sdl3\n" );
		return 1;
	}
	
	if ( !ImGui_ImplSDLRenderer3_Init( g_main_renderer ) )
	{
		printf( "Failed to init imgui for sdl3 renderer\n" );
		return 1;
	}

	ImGuiIO& io      = ImGui::GetIO();

	io.DisplaySize.x = 1600;
	io.DisplaySize.y = 900;

	ImVec4 clear_color = ImVec4( 0.15f, 0.15f, 0.15f, 1.00f );

	// image_t* jpeg        = g_test_codec->image_load( TEST_IMAGE );
	// 
	// if ( jpeg )
	// {
	// 	g_focused_image_surf = SDL_CreateSurfaceFrom( jpeg->width, jpeg->height, SDL_PIXELFORMAT_XBGR8888, jpeg->data, jpeg->width * 4 );
	// 	g_focused_image      = SDL_CreateTextureFromSurface( g_main_renderer, g_focused_image_surf );
	// }

	if ( !thumbnail_loader_init() )
	{
		printf( "Failed to init thumbnail loader\n" );
		return 1;
	}

	folder_load_media_list();
	media_view_load();

	while ( g_running )
	{
		// don't go full speed lol
		// SDL_Delay( 5 );
		SDL_Delay( 2 );
		
		thumbnail_loader_update();

		g_mouse_scrolled_up   = false;
		g_mouse_scrolled_down = false;

		//auto      startTime = std::chrono::high_resolution_clock::now();

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
					break;

				case SDL_EVENT_WINDOW_RESIZED:
					int width, height;
					SDL_GetWindowSize( g_main_window, &width, &height );
					io.DisplaySize.x = width;
					io.DisplaySize.y = height;
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

		bool show_frame_time = false;

		if ( ImGui::IsKeyPressed( ImGuiKey_Enter, false ) )
		{
			gallery_view_toggle();
		}

		ImGui::ShowDemoWindow();
		imgui_draw();

		ImGui::Render();

		SDL_SetRenderScale( g_main_renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y );
		SDL_SetRenderDrawColorFloat( g_main_renderer, clear_color.x, clear_color.y, clear_color.z, clear_color.w );
		SDL_RenderClear( g_main_renderer );

		if ( !g_gallery_view && g_image_data.texture )
		{
			media_view_draw();
		}

		ImGui_ImplSDLRenderer3_RenderDrawData( ImGui::GetDrawData(), g_main_renderer );

		SDL_RenderPresent( g_main_renderer );

		if ( g_image_data_free.texture )
		{
			SDL_DestroyTexture( g_image_data_free.texture );
			SDL_DestroySurface( g_image_data_free.surface );

			g_image_data_free.texture = nullptr;
			g_image_data_free.surface = nullptr;
		}

		//auto  currentTime = std::chrono::high_resolution_clock::now();
		//float time     = std::chrono::duration< float, std::chrono::seconds::period >( currentTime - startTime ).count();
		//if ( show_frame_time )
		// printf( "%f FRAMETIME\n", time );
	}

	args_free();
	sys_shutdown();
	SDL_Quit();
	return 0;
}

