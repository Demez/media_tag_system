#include "main.h"


double g_zoom_level = 1.f;


extern main_image_data_t g_image_data;
extern main_image_data_t g_image_data_free;


// TEMPORARY
extern ICodec*           g_test_codec;


void   media_view_context_menu()
{
	if ( !ImGui::BeginPopupContextVoid( "main ctx menu" ) )
		return;

	if ( ImGui::MenuItem( "Fit In View", nullptr, false, g_image_data.texture ) )
	{
		//ImageView_FitInView();
	}

	if ( ImGui::MenuItem( "Reset Zoom to 100%", nullptr, false, g_image_data.texture ) )
	{
		//ImageView_ResetZoom();
	}

	if ( ImGui::MenuItem( "Rotate Left", nullptr, false, g_image_data.texture ) )
	{
		//float rot = ImageView_GetRotation();
		//ImageView_SetRotation( rot - gRot90DegInRad );
	}

	if ( ImGui::MenuItem( "Rotate Right", nullptr, false, g_image_data.texture ) )
	{
		//float rot = ImageView_GetRotation();
		//ImageView_SetRotation( rot + gRot90DegInRad );
	}

	if ( ImGui::MenuItem( "Reset Rotation", nullptr, false, g_image_data.texture ) )
	{
		//ImageView_ResetRotation();
	}

	if ( ImGui::MenuItem( "Flip Horizontally", nullptr, false, 0 ) )
	{
	}

	if ( ImGui::MenuItem( "Flip Vertically", nullptr, false, 0 ) )
	{
	}

	ImGui::Separator();

	if ( ImGui::MenuItem( "Open File Location", nullptr, false, g_image_data.texture ) )
	{
		//Plat_BrowseToFile( ImageView_GetImagePath() );
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
}


void media_view_advance( bool prev = false )
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
}


void media_view_draw()
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
	float     factor[ 2 ] = { 1.f, 1.f };

	if ( g_image.width > width )
		factor[ 0 ] = (float)width / (float)g_image.width;

	if ( g_image.height > height )
		factor[ 1 ] = (float)height / (float)g_image.height;

	float zoom_level = std::min( factor[ 0 ], factor[ 1 ] );

	dst_rect.w       = g_image.width * zoom_level;
	dst_rect.h       = g_image.height * zoom_level;
	dst_rect.x       = width / 2 - ( dst_rect.w / 2 );
	dst_rect.y       = height / 2 - ( dst_rect.h / 2 );

	SDL_RenderTexture( g_main_renderer, g_image_data.texture, NULL, &dst_rect );
}

