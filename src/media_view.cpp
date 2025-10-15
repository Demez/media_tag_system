#include "main.h"


// Image Draw Data
e_zoom_mode              g_image_zoom_mode;
double                   g_image_zoom = 1.f;
ImVec2                   g_image_pos{};
ImVec2                   g_image_size{};
bool                     g_image_flip_v = false;
bool                     g_image_flip_h = false;
float                    g_image_rot    = 0.f;


// Image Panning
bool                     g_image_pan    = false;


extern main_image_data_t g_image_data;
extern main_image_data_t g_image_data_free;


// TEMPORARY
extern ICodec*           g_test_codec;

constexpr double         ZOOM_AMOUNT = 0.1;
constexpr double         ZOOM_MIN    = 0.01;


void media_view_fit_in_view( bool adjust_zoom = true, bool center_image = true )
{
	// new image size
	int width, height;
	SDL_GetWindowSize( g_main_window, &width, &height );

	// Fit image in window size
	float factor[ 2 ] = { 1.f, 1.f };

	if ( g_image.width > width )
		factor[ 0 ] = (float)width / (float)g_image.width;

	if ( g_image.height > height )
		factor[ 1 ] = (float)height / (float)g_image.height;

	if ( adjust_zoom )
	{
		float zoom_level = std::min( factor[ 0 ], factor[ 1 ] );

		g_image_size.x   = g_image.width * zoom_level;
		g_image_size.y   = g_image.height * zoom_level;
	}

	// TODO: only adjust this if needed, check image zoom type
	// if image doesn't fit window size, keep locked to center

	//if ( factor[ 0 ] < 1.f )
		g_image_pos.x = width / 2 - ( g_image_size.x / 2 );

	//if ( factor[ 1 ] < 1.f )
		g_image_pos.y = height / 2 - ( g_image_size.y / 2 );
}


void media_view_zoom_reset()
{
	// keep where we are centered on in the image
	g_image_pos.x = 1 / g_image_zoom * ( g_image_pos.x );
	g_image_pos.y = 1 / g_image_zoom * ( g_image_pos.y );

	g_image_zoom  = 1.0;

	if ( !g_image.data )
		return;

	g_image_size.x = g_image.width;
	g_image_size.y = g_image.height;
}


void media_view_scroll_zoom( float scroll )
{
	if ( !g_image_data.texture || scroll == 0 )
		return;

	double factor = 1.0;

	// Zoom in if scrolling up
	if ( scroll > 0 )
	{
		// max zoom level
		if ( g_image_zoom >= 100.0 )
			return;

		factor += ( ZOOM_AMOUNT * scroll );
	}
	else
	{
		// min zoom level
		if ( g_image_zoom <= 0.01 )
			return;

		factor -= ( ZOOM_AMOUNT * abs( scroll ) );
	}

	// TODO: add zoom levels to snap to here
	// 100, 200, 400, 500, 50, 25, etc

	auto rounded_zoom = std::max( 0.01f, roundf( g_image_zoom * factor * 100 ) / 100 );

	if ( fmod( rounded_zoom, 1.0 ) == 0 )
		factor = rounded_zoom / g_image_zoom;

	double old_zoom = g_image_zoom;

	g_image_zoom = (double)( std::max( 1.f, g_image_size.x ) * factor ) / (double)g_image.width;

	// round it so we don't get something like 0.9999564598 or whatever instead of 1.0
	g_image_zoom   = std::max( ZOOM_MIN, round( g_image_zoom * 100 ) / 100 );

	// recalculate draw width and height
	g_image_size.x = (double)g_image.width * g_image_zoom;
	g_image_size.y = (double)g_image.height * g_image_zoom;

	// recalculate image position to keep image where cursor is

	// New Position = Scale Origin + ( Scale Point - Scale Origin ) * Scale Factor
	g_image_pos.x  = g_mouse_pos[ 0 ] + ( g_image_pos.x - g_mouse_pos[ 0 ] ) * factor;
	g_image_pos.y  = g_mouse_pos[ 1 ] + ( g_image_pos.y - g_mouse_pos[ 1 ] ) * factor;
}


void media_view_context_menu()
{
	if ( !ImGui::BeginPopupContextVoid( "main ctx menu" ) )
		return;

	ImGuiStyle& style        = ImGui::GetStyle();
	ImVec2      region_avail = ImGui::GetContentRegionAvail();

	float       button_width = ( region_avail.x / 2 ) + style.ItemSpacing.x;

	if ( ImGui::Button( "FIT", { 0, 0 } ) )
		media_view_fit_in_view();

	ImGui::SameLine();

	if ( ImGui::Button( "100%", { 0, 0 } ) )
		media_view_zoom_reset();

	ImGui::Separator();

	if ( ImGui::Button( "RL" ) )
		g_image_rot -= 90;

	ImGui::SameLine();

	if ( ImGui::Button( "RR" ) )
		g_image_rot += 90;

	ImGui::SameLine();

	if ( ImGui::Button( "R" ) )
		g_image_rot = 0;

	ImGui::SameLine();

	if ( ImGui::Button( "H" ) )
		g_image_flip_h = !g_image_flip_h;

	ImGui::SameLine();

	if ( ImGui::Button( "V" ) )
		g_image_flip_v = !g_image_flip_v;

	ImGui::Separator();

#if 0
	if ( ImGui::MenuItem( "Fit In View", nullptr, false, g_image_data.texture ) )
	{
		media_view_fit_in_view();
	}

	if ( ImGui::MenuItem( "Reset Zoom to 100%", nullptr, false, g_image_data.texture ) )
	{
		media_view_zoom_reset();
	}

	if ( ImGui::MenuItem( "Rotate Left", nullptr, false, g_image_data.texture ) )
	{
		g_image_rot -= 90;
	}

	if ( ImGui::MenuItem( "Rotate Right", nullptr, false, g_image_data.texture ) )
	{
		g_image_rot += 90;
	}

	// ImGui::SliderFloat( "Rotate Slider", &g_image_rot, 0, 360 );

	if ( ImGui::MenuItem( "Reset Rotation", nullptr, false, g_image_data.texture ) )
	{
		g_image_rot = 0;
	}

	if ( ImGui::MenuItem( "Flip Horizontally", nullptr, g_image_flip_h, g_image_data.texture) )
	{
		g_image_flip_h = !g_image_flip_h;
	}

	if ( ImGui::MenuItem( "Flip Vertically", nullptr, g_image_flip_v, g_image_data.texture ) )
	{
		g_image_flip_v = !g_image_flip_v;
	}

	ImGui::Separator();
#endif

	if ( ImGui::MenuItem( "Open File Location", nullptr, false, g_image_data.texture ) )
	{
		sys_browse_to_file( g_folder_media_list[ g_folder_index ].string().c_str() );
	}

	if ( ImGui::BeginMenu( "Open With" ) )
	{
		// TODO: list programs to open the file with, like fragment image viewer
		// how would this work on linux actually? hmm
		ImGui::MenuItem( "nothing lol", nullptr, false, false );
		ImGui::EndMenu();
	}

	if ( ImGui::MenuItem( "Copy Image", nullptr, false, false ) )
	{
	}

	if ( ImGui::MenuItem( "Copy Image Data", nullptr, false, false ) )
	{
	}

	if ( ImGui::MenuItem( "Set As Desktop Background", nullptr, false, false ) )
	{
	}

	if ( ImGui::MenuItem( "Undo", nullptr, false, 0 ) )
	{
		//UndoSys_Undo();
	}

	if ( ImGui::MenuItem( "Redo", nullptr, false, 0 ) )
	{
		//UndoSys_Redo();
	}

	if ( ImGui::MenuItem( "Delete", nullptr, false, 0 ) )
	{
		//ImageView_DeleteImage();
	}

	if ( ImGui::MenuItem( "File Info", nullptr, false, false ) )
	{
	}

	if ( ImGui::MenuItem( "File Properties", nullptr, false, 0 ) )
	{
		// TODO: create our own imgui file properties for more info
		// Plat_OpenFileProperties( ImageView_GetImagePath() );
	}

	ImGui::Separator();

	if ( ImGui::BeginMenu( "Sort Mode" ) )
	{
#if 0
		auto sortMode = ImageList_GetSortMode();

		if ( ImGui::MenuItem( "None", nullptr, sortMode == FileSort_None, ImageList_InFolder() ) )
			ImageList_SetSortMode( FileSort_None );

		if ( ImGui::MenuItem( "File Name - A to Z", nullptr, sortMode == FileSort_AZ, false ) )
			ImageList_SetSortMode( FileSort_AZ );

		if ( ImGui::MenuItem( "File Name - Z to A", nullptr, sortMode == FileSort_ZA, false ) )
			ImageList_SetSortMode( FileSort_ZA );

		if ( ImGui::MenuItem( "Date Modified - Newest First", nullptr, sortMode == FileSort_DateModNewest, ImageList_InFolder() ) )
			ImageList_SetSortMode( FileSort_DateModNewest );

		if ( ImGui::MenuItem( "Date Modified - Oldest First", nullptr, sortMode == FileSort_DateModOldest, ImageList_InFolder() ) )
			ImageList_SetSortMode( FileSort_DateModOldest );

		if ( ImGui::MenuItem( "Date Created - Newest First", nullptr, sortMode == FileSort_DateCreatedNewest, ImageList_InFolder() ) )
			ImageList_SetSortMode( FileSort_DateCreatedNewest );

		if ( ImGui::MenuItem( "Date Created - Oldest First", nullptr, sortMode == FileSort_DateCreatedOldest, ImageList_InFolder() ) )
			ImageList_SetSortMode( FileSort_DateCreatedOldest );

		if ( ImGui::MenuItem( "File Size - Largest First", nullptr, sortMode == FileSort_SizeLargest, false ) )
			ImageList_SetSortMode( FileSort_SizeLargest );

		if ( ImGui::MenuItem( "File Size - Smallest First", nullptr, sortMode == FileSort_SizeSmallest, false ) )
			ImageList_SetSortMode( FileSort_SizeSmallest );
#endif

		ImGui::EndMenu();
	}

	if ( ImGui::MenuItem( "Reload Folder", nullptr, false ) )
	{
		folder_load_media_list();
	}

	ImGui::Separator();

	if ( ImGui::MenuItem( "Settings", nullptr, false, false ) )
	{
	}

	// 	if ( ImGui::MenuItem( "Show ImGui Demo", nullptr, gShowImGuiDemo ) )
	// 	{
	// 		gShowImGuiDemo = !gShowImGuiDemo;
	// 	}

	ImGui::EndPopup();
}


void media_view_input()
{
	if ( ImGui::IsKeyPressed( ImGuiKey_RightArrow, true ) )
	{
		media_view_advance();
	}
	else if ( ImGui::IsKeyPressed( ImGuiKey_LeftArrow, true ) )
	{
		media_view_advance( true );
	}

	media_view_context_menu();

	auto& io = ImGui::GetIO();

	// check if the mouse isn't hovering over any window and we didn't grab it already
	if ( io.WantCaptureMouseUnlessPopupClose && !g_image_pan )
		return;

	g_image_pan = ImGui::IsMouseDown( ImGuiMouseButton_Left );

	if ( g_image_pan )
	{
		g_image_pos.x += g_mouse_delta[ 0 ];
		g_image_pos.y += g_mouse_delta[ 1 ];
	}
}


void media_view_window_resize()
{
	if ( g_image_zoom_mode == e_zoom_mode_fit || e_zoom_mode_fit_width )
	{
		media_view_fit_in_view( false );
	}
}


void media_view_load()
{
	float load_time = 0.f;

	{
		auto startTime = std::chrono::high_resolution_clock::now();

		// g_image_view.image = g_test_codec->image_load( g_folder_media_list[ g_folder_index ] );
		g_test_codec->image_load( g_folder_media_list[ g_folder_index ], &g_image );

		auto currentTime = std::chrono::high_resolution_clock::now();

		load_time        = std::chrono::duration< float, std::chrono::seconds::period >( currentTime - startTime ).count();
	}

	// auto startTime       = std::chrono::high_resolution_clock::now();

	g_image_data.surface = SDL_CreateSurfaceFrom( g_image.width, g_image.height, g_image.format, g_image.data, g_image.pitch );
	g_image_data.texture = SDL_CreateTextureFromSurface( g_main_renderer, g_image_data.surface );

	// auto  currentTime    = std::chrono::high_resolution_clock::now();
	// float up_time        = std::chrono::duration< float, std::chrono::seconds::period >( currentTime - startTime ).count();
	//printf( "%f Load - %f Up - %s\n", load_time, up_time, g_folder_media_list[ g_folder_index ].string().c_str() );
	printf( "%f Load - %s\n", load_time, g_folder_media_list[ g_folder_index ].string().c_str() );

	g_image_index = g_folder_index;

	media_view_fit_in_view();

	update_window_title();
}


void media_view_advance( bool prev )
{
	g_image_data_free = g_image_data;

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

	media_view_load();
}


void media_view_draw_imgui()
{
	media_view_input();
}


void media_view_draw_image()
{
	SDL_FRect dst_rect{};
	dst_rect.w = g_image_size.x;
	dst_rect.h = g_image_size.y;
	dst_rect.x = g_image_pos.x;
	dst_rect.y = g_image_pos.y;

	if ( g_image_flip_h )
	{
		dst_rect.w = -g_image_size.x;
		dst_rect.x += g_image_size.x;
	}

	if ( g_image_flip_v )
	{
		dst_rect.h = -g_image_size.y;
		dst_rect.y += g_image_size.y;
	}

	// SDL_RenderTexture( g_main_renderer, g_image_data.texture, NULL, &dst_rect );

	ImVec2 mouse_pos_image_coords( (float)g_mouse_pos[ 0 ] - g_image_pos.x, (float)g_mouse_pos[ 1 ] - g_image_pos.y );
	ImVec2 image_pos_end( g_image_pos.x + g_image_size.x, g_image_pos.y + g_image_size.y );

	SDL_FPoint image_center( g_image_pos.x + ( g_image_size.x / 2 ), g_image_pos.y + ( g_image_size.y / 2 ) );

	SDL_RenderTextureRotated( g_main_renderer, g_image_data.texture, NULL, &dst_rect, g_image_rot, nullptr, SDL_FLIP_NONE );
}

