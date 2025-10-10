#include "main.h"

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"

#define DATABASE_FILE "tag_database.txt"

#include <chrono>
#include <thread>


SDL_Window*                g_main_window        = nullptr;
SDL_Renderer*              g_main_renderer      = nullptr;


bool                       g_running            = true;
bool                       g_gallery_view       = false;

ICodec*                    g_test_codec         = nullptr;

fs::path                   TEST_IMAGE           = "D:\\demez_archive\\media\\downloads\\art\\[bsky] camothetiger.bsky.social—2025.04.14—3lmqydllpak2a—bafkreidjzm36t5zwx7drrmioalhswola36t3hngwmtqbolfriogffzclbe.jpg";
// fs::path TEST_IMAGE      = "D:\\demez_archive\\media\\downloads\\art\\WsWLHtGJ_400x400.jpg";

fs::path                   TEST_FOLDER          = "D:\\demez_archive\\media\\downloads\\art";


image_t*                   g_jpeg               = new image_t;
SDL_Texture*               g_focused_image      = nullptr;
SDL_Surface*               g_focused_image_surf = nullptr;

// to free next frame

image_t*                   g_free_jpeg          = nullptr;
SDL_Texture*               g_free_image         = nullptr;
SDL_Surface*               g_free_image_surf    = nullptr;


std::vector< fs::path >    g_folder_media_list;
std::vector< h_thumbnail > g_folder_thumbnail_list;
size_t                     g_folder_index = 0;


void register_codec( ICodec* codec )
{
	g_test_codec = codec;
}


void gallery_draw_header()
{
	int window_width, window_height;
	SDL_GetWindowSize( g_main_window, &window_width, &window_height );


	ImGui::SetNextWindowPos( { 0, 0 } );
	ImGui::SetNextWindowSize( { (float)window_width, 32.f } );

	if ( ImGui::BeginChild( "##gallery_header", {}, ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_FrameStyle ) )
	{
		ImGui::Text( TEST_FOLDER.string().c_str() );

		ImGui::EndChild();
	}
}


void gallery_draw_content()
{
	int window_width, window_height;
	SDL_GetWindowSize( g_main_window, &window_width, &window_height );

	ImVec2      region_avail = ImGui::GetContentRegionAvail();

	ImGuiStyle& style        = ImGui::GetStyle();

	ImGui::SetNextWindowPos( { 0, 32.f + style.ItemSpacing.y } );
	ImGui::SetNextWindowSize( region_avail );

	if ( !ImGui::BeginChild( "##gallery_content" ) )
	{
		ImGui::EndChild();
		return;
	}

	float       text_height = ImGui::CalcTextSize( "TEST" ).y;

	// ScrollToBringRectIntoView

	int         grid_count_x      = 6;
	int         grid_item_padding = style.ItemSpacing.x;

	// float       item_size_x       = ( region_avail.x / grid_count_x ) - ( grid_count_x * grid_item_padding ) - ( grid_item_padding * 2 );
	float       item_size_x       = ( region_avail.x / grid_count_x ) - ( ( grid_count_x - 1 ) * grid_item_padding );
	float       item_size_y       = item_size_x + text_height + style.ItemInnerSpacing.y;

	int         grid_pos_x        = 0;
	size_t      i                 = 0;

	int         image_visible_count = 0;

	struct delayed_load_t
	{
		fs::path path;
		size_t   index;
	};

	static std::vector< delayed_load_t > thumbnail_requests;
	thumbnail_requests.clear();

	for ( size_t i = 0; i < g_folder_media_list.size(); i++ )
	{
		const fs::path& entry  = g_folder_media_list[ i ];

		float           scroll = ImGui::GetScrollY();

		ImGui::SetNextWindowSize( { item_size_x, item_size_y } );

		if ( grid_pos_x == grid_count_x )
		{
			grid_pos_x = 0;
			ImGui::SetCursorPosX( ImGui::GetCursorPosX() + style.ItemSpacing.x );
		}
		else
		{
			// ImGui::SetCursorPos( { cursor_pos.x + item_size_x + grid_item_padding, cursor_pos.y } );
			ImGui::SameLine();
		}

		ImVec2 cursor_pos = ImGui::GetCursorPos();

		if ( g_folder_index == i )
		{
			ImGui::PushStyleColor( ImGuiCol_ChildBg, style.Colors[ ImGuiCol_FrameBg ] );
		}

		// Calculate Distance
		{
			ImVec2 window_pos     = ImGui::GetWindowPos();
			float  visible_top    = scroll;
			float  visible_bottom = visible_top + ImGui::GetWindowHeight();

			float  cursor_y       = ImGui::GetCursorPosY();
			//float  item_y         = cursor_y + item_size_y;

			u32    distance       = 0;
			if ( cursor_y + item_size_y < visible_top )
				distance = visible_top - ( cursor_y + item_size_y );  // above view
			else if ( cursor_y > visible_bottom )
				distance = cursor_y - visible_bottom;  // below view
			else
				distance = 0;  // visible

			thumbnail_update_distance( g_folder_thumbnail_list[ i ], distance );
		}

		ImGui::PushID( *entry.string().c_str() );

		if ( ImGui::BeginChild( entry.string().c_str(), {}, ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoScrollbar ) )
		{
			image_visible_count++;

			float image_size = item_size_x - ( style.ItemInnerSpacing.y * 4 );
			ImVec2 current_pos = ImGui::GetCursorPos();
			// ImGui::SetNextWindowPos( { current_pos.x + style.ItemInnerSpacing.x, current_pos.y + style.ItemInnerSpacing.y } );
			ImGui::SetNextWindowSize( { image_size, image_size } );

			// if ( visible )
			 	ImGui::PushStyleColor( ImGuiCol_ChildBg, { 0.25f, 0.25f, 0.25f, 1.f } );

			thumbnail_t* thumbnail = thumbnail_get_data( g_folder_thumbnail_list[ i ] );

			if ( thumbnail )
			{
				if ( thumbnail->path != entry )
				{
					printf( "how did we get here?\n" );
				}

				if ( thumbnail->status == e_thumbnail_status_finished )
				{
					// Fit image in window size, scaling up if needed
					float     factor[ 2 ] = { 1.f, 1.f };

					factor[ 0 ] = (float)image_size / (float)thumbnail->data->width;
					factor[ 1 ] = (float)image_size / (float)thumbnail->data->height;

					float zoom_level = std::min( factor[ 0 ], factor[ 1 ] );

					ImVec2 scaled_image_size;
					scaled_image_size.x = thumbnail->data->width * zoom_level;
					scaled_image_size.y = thumbnail->data->height * zoom_level;
					
					// center the image
					ImVec2 image_offset   = ImGui::GetCursorPos();
					image_offset.x += ( image_size - scaled_image_size.x ) / 2;
					image_offset.y += ( image_size - scaled_image_size.y ) / 2;

					ImGui::SetCursorPos( image_offset );

					ImGui::Image( thumbnail->im_texture, scaled_image_size );
				}
				else
				{
					ImGui::Dummy( { image_size, image_size } );
				}
			}
			else if ( !thumbnail || thumbnail->status == e_thumbnail_status_failed )
			{
				if ( !thumbnail )
					thumbnail_requests.emplace_back( entry, i );
					// g_folder_thumbnail_list[ i ] = thumbnail_queue_image( entry );

				ImGui::Dummy( { image_size, image_size } );

				// if ( ImGui::BeginChild( "##image", {} ) )
				// {
				// }
				// else
				// {
				// 	// printf( "HIDDEN\n" );
				// }
				// 
				// ImGui::EndChild();
			}

			// if ( visible )
				ImGui::PopStyleColor();
		}

		ImGui::TextUnformatted( entry.string().c_str() );

		ImGui::EndChild();

		ImGui::PopID();

		if ( g_folder_index == i )
		{
			ImGui::PopStyleColor();
		}

		grid_pos_x++;

	}

	ImGui::EndChild();

	// printf( "IMAGE COUNT: %d\n", image_visible_count );

	for ( size_t i = 0; i < thumbnail_requests.size(); i++ )
		g_folder_thumbnail_list[ thumbnail_requests[ i ].index ] = thumbnail_queue_image( thumbnail_requests[ i ].path );
}


void gallery_draw()
{
	int window_width, window_height;
	SDL_GetWindowSize( g_main_window, &window_width, &window_height );

	ImGui::SetNextWindowPos( { 0, 0 } );
	ImGui::SetNextWindowSize( { (float)window_width, (float)window_height } );

	if ( !ImGui::Begin( "##gallery_main", 0, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings ) )
	{
		ImGui::End();
		return;
	}

	// Header
	gallery_draw_header();

	// Sidebar
	
	// Gallery View
	gallery_draw_content();

	thumbnail_cache_debug_draw();

	ImGui::End();
}


void imgui_draw()
{
	if ( g_gallery_view )
		gallery_draw();
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


void folder_load_image()
{
	float load_time = 0.f;

	{
		auto startTime = std::chrono::high_resolution_clock::now();

		// g_jpeg = g_test_codec->image_load( g_folder_media_list[ g_folder_index ] );
		g_test_codec->image_load( g_folder_media_list[ g_folder_index ], g_jpeg );

		auto currentTime = std::chrono::high_resolution_clock::now();

		load_time        = std::chrono::duration< float, std::chrono::seconds::period >( currentTime - startTime ).count();
	}

	if ( g_jpeg )
	{
		// auto startTime       = std::chrono::high_resolution_clock::now();

		g_focused_image_surf = SDL_CreateSurfaceFrom( g_jpeg->width, g_jpeg->height, SDL_PIXELFORMAT_BGR24, g_jpeg->data, g_jpeg->pitch );
		// g_focused_image_surf = SDL_CreateSurfaceFrom( g_jpeg->width, g_jpeg->height, SDL_PIXELFORMAT_RGB24, g_jpeg->data, g_jpeg->width * g_jpeg->pitch );
		// g_focused_image_surf = SDL_CreateSurfaceFrom( g_jpeg->width, g_jpeg->height, SDL_PIXELFORMAT_XBGR8888, g_jpeg->data, g_jpeg->width * g_jpeg->pitch );
		g_focused_image      = SDL_CreateTextureFromSurface( g_main_renderer, g_focused_image_surf );

		// auto  currentTime    = std::chrono::high_resolution_clock::now();
		// float up_time        = std::chrono::duration< float, std::chrono::seconds::period >( currentTime - startTime ).count();
		//printf( "%f Load - %f Up - %s\n", load_time, up_time, g_folder_media_list[ g_folder_index ].string().c_str() );
		printf( "%f Load - %s\n", load_time, g_folder_media_list[ g_folder_index ].string().c_str() );
	}
}


void folder_load_next_image( bool prev = false )
{
	g_free_image      = g_focused_image;
	g_free_image_surf = g_focused_image_surf;
	g_free_jpeg       = g_jpeg;

	if ( prev )
	{
		if ( g_folder_index == 0 )
			g_folder_index = g_folder_media_list.size();
		
		g_folder_index--;
	}
	else
	{
		g_folder_index++;

		if ( g_folder_index == g_folder_media_list.size() )
			g_folder_index = 0;
	}

	folder_load_image();
}


int main( int argc, char* argv[] )
{
	memset( g_jpeg, 0, sizeof( image_t ) );

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
	folder_load_image();

	while ( g_running )
	{
		thumbnail_loader_update();

		//auto      startTime = std::chrono::high_resolution_clock::now();

		// Handle Events
		SDL_Event event;
		while ( SDL_PollEvent( &event ) )
		{
			ImGui_ImplSDL3_ProcessEvent( &event );

			switch ( event.type )
			{
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

		if ( ImGui::IsKeyPressed( ImGuiKey_RightArrow, true ) )
		{
			folder_load_next_image();
			show_frame_time = true;
		}
		else if ( ImGui::IsKeyPressed( ImGuiKey_LeftArrow, true ) )
		{
			folder_load_next_image( true );
			show_frame_time = true;
		}
		else if ( ImGui::IsKeyPressed( ImGuiKey_Enter ) )
		{
			g_gallery_view = !g_gallery_view;
		}

		ImGui::ShowDemoWindow();
		imgui_draw();

		ImGui::Render();

		SDL_SetRenderScale( g_main_renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y );
		SDL_SetRenderDrawColorFloat( g_main_renderer, clear_color.x, clear_color.y, clear_color.z, clear_color.w );
		SDL_RenderClear( g_main_renderer );

		if ( !g_gallery_view && g_jpeg )
		{
			// new image size
		//	int new_width, new_height;
			int width, height;
			SDL_GetWindowSize( g_main_window, &width, &height );
		//
		//	float max_size = std::max( width, height );
		//	new_width      = jpeg->width / max_size;
			
			// Fit image in window size
			SDL_FRect dst_rect{};
			float factor[ 2 ] = { 1.f, 1.f };

			if ( g_jpeg->width > width )
				factor[ 0 ] = (float)width / (float)g_jpeg->width;

			if ( g_jpeg->height > height )
				factor[ 1 ] = (float)height / (float)g_jpeg->height;

			float zoom_level = std::min( factor[ 0 ], factor[ 1 ] );

			dst_rect.w       = g_jpeg->width * zoom_level;
			dst_rect.h       = g_jpeg->height * zoom_level;
			dst_rect.x       = width / 2 - ( dst_rect.w / 2 );
			dst_rect.y       = height / 2 - ( dst_rect.h / 2 );

			SDL_RenderTexture( g_main_renderer, g_focused_image, NULL, &dst_rect );
		}

		ImGui_ImplSDLRenderer3_RenderDrawData( ImGui::GetDrawData(), g_main_renderer );

		SDL_RenderPresent( g_main_renderer );

		if ( g_free_image )
		{
			SDL_DestroyTexture( g_free_image );
			SDL_DestroySurface( g_free_image_surf );

			g_free_image      = nullptr;
			g_free_image_surf = nullptr;
		}

		if ( g_free_jpeg )
		{
			// free( g_free_jpeg->data );
			// delete g_free_jpeg;
			// 
			// g_free_jpeg = nullptr;
		}

		//auto  currentTime = std::chrono::high_resolution_clock::now();
		//float time     = std::chrono::duration< float, std::chrono::seconds::period >( currentTime - startTime ).count();
		//if ( show_frame_time )
		// printf( "%f FRAMETIME\n", time );

		thumbnail_loader_update_after_render();
	}

	args_free();
	sys_shutdown();
	SDL_Quit();
	return 0;
}

