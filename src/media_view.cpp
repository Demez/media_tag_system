#include "main.h"
#include "imgui_internal.h"


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

constexpr double         ZOOM_AMOUNT = 0.1;
constexpr double         ZOOM_MIN    = 0.01;


static e_media_type get_media_type()
{
	if ( g_folder_media_list.size() <= g_gallery_index )
		return e_media_type_none;

	return g_folder_media_list[ g_gallery_index ].type;
}


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

	if ( !g_image.frame[ 0 ] )
		return;

	g_image_size.x = g_image.width;
	g_image_size.y = g_image.height;
}


void media_view_scroll_zoom( float scroll )
{
	if ( !g_image_data.texture || scroll == 0 )
		return;

	if ( mouse_hovering_imgui_window() )
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

	ImGui::SliderFloat( "rotate", &g_image_rot, 0, 360 );

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
		sys_browse_to_file( g_folder_media_list[ g_gallery_index ].path.string().c_str() );
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

	// if ( !mouse_hovering_imgui_window() )
	// 	return;

	auto& io = ImGui::GetIO();

	// check if the mouse isn't hovering over any window and we didn't grab it already
	if ( io.WantCaptureMouseUnlessPopupClose && !g_image_pan )
		return;

	g_image_pan = ImGui::IsMouseDown( ImGuiMouseButton_Left ) && !(mouse_hovering_imgui_window() && !g_image_pan);

	if ( g_image_pan )
	{
		g_image_pos.x += g_mouse_delta[ 0 ];
		g_image_pos.y += g_mouse_delta[ 1 ];
	}
}


void media_view_window_resize()
{
	if ( g_image_zoom_mode == e_zoom_mode_fit || g_image_zoom_mode == e_zoom_mode_fit_width )
	{
		media_view_fit_in_view( false );
	}
}


void media_view_load()
{
	if ( g_folder_media_list.empty() )
		return;

	if ( g_gallery_index >= g_folder_media_list.size() )
		return;

	float          load_time = 0.f;
	media_entry_t& entry = g_folder_media_list[ g_gallery_index ];

	image_load_info_t image_load_info{};
	image_load_info.image = &g_image;

	{
		auto startTime = std::chrono::high_resolution_clock::now();

		if ( entry.type == e_media_type_image )
		{
			// g_image_view.image = g_test_codec->image_load( g_folder_media_list[ g_folder_index ] );
			image_load( entry.path, image_load_info );
		}
		else
		{
			mpv_cmd_loadfile( entry.path.string().c_str() );
		}

		auto currentTime = std::chrono::high_resolution_clock::now();

		load_time        = std::chrono::duration< float, std::chrono::seconds::period >( currentTime - startTime ).count();

		// auto startTime       = std::chrono::high_resolution_clock::now();

		
		if ( entry.type == e_media_type_image )
		{
			if ( image_load_info.image->frame.size() > 0 )
			{
				gl_update_texture( g_image_data.texture, &g_image );
			}
			else
			{
				printf( "%f FAILED Load - %s\n", load_time, g_folder_media_list[ g_gallery_index ].path.string().c_str() );
			}
		}

	//	g_image_data.surface = SDL_CreateSurfaceFrom( g_image.width, g_image.height, g_image.format, g_image.frame, g_image.pitch );
	//	g_image_data.texture = SDL_CreateTextureFromSurface( g_main_renderer, g_image_data.surface );

		// auto  currentTime    = std::chrono::high_resolution_clock::now();
		// float up_time        = std::chrono::duration< float, std::chrono::seconds::period >( currentTime - startTime ).count();
		//printf( "%f Load - %f Up - %s\n", load_time, up_time, g_folder_media_list[ g_folder_index ].string().c_str() );
		printf( "%f Load - %s\n", load_time, g_folder_media_list[ g_gallery_index ].path.string().c_str() );
	}

	g_media_index = g_gallery_index;

	media_view_fit_in_view();

	update_window_title();
}


void media_view_advance( bool prev )
{
	if ( g_folder_media_list.size() <= 1 )
		return;

	if ( get_media_type() == e_media_type_video )
		mpv_cmd_loadfile( "" );
	//else
	//	g_image_data_free = g_image_data;

advance:
	if ( prev )
	{
		if ( g_gallery_index == 0 )
			g_gallery_index = g_folder_media_list.size();

		g_gallery_index--;
	}
	else
	{
		g_gallery_index++;

		if ( g_gallery_index == g_folder_media_list.size() )
			g_gallery_index = 0;
	}

	if ( g_folder_media_list[ g_gallery_index ].type == e_media_type_directory )
		goto advance;

	media_view_load();
}


void media_view_draw_video_controls()
{
	if ( ImGui::IsKeyPressed( ImGuiKey_Space, false ) || ( !mouse_hovering_imgui_window() && ImGui::IsKeyPressed( ImGuiKey_MouseLeft, false ) ) )
	{
		const char* cmd[]   = { "cycle", "pause", NULL };
		int         cmd_ret = p_mpv_command_async( g_mpv, 0, cmd );
	}

	int width, height;
	SDL_GetWindowSize( g_main_window, &width, &height );

	ImVec2 playback_control_pos{};
	playback_control_pos.x = width / 2;
	//playback_control_pos.y = ( height - 75.f );
	playback_control_pos.y = ( height - 40.f );

	// check if mouse in rectangle

	static float controls_height = 50.f;
	if ( !mouse_in_rect( { 0.f, height - (80.f + (controls_height * 2)) }, { (float)width, (float)height } ) )
		return;

	// ----------------------------------------

	// pivot aligns it to the center and the bottom of the window
	ImGui::SetNextWindowPos( playback_control_pos, 0, ImVec2( 0.5f, 1.0f ) );

	if ( !ImGui::Begin( "##video_controls", 0, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing ) )
	{
		ImGui::End();
		return;
	}

	double time_pos = 0;
	double duration = 0;
	s32    paused   = 0;
	p_mpv_get_property( g_mpv, "time-pos", MPV_FORMAT_DOUBLE, &time_pos );
	p_mpv_get_property( g_mpv, "duration", MPV_FORMAT_DOUBLE, &duration );
	p_mpv_get_property( g_mpv, "pause", MPV_FORMAT_FLAG, &paused );

	ImGuiStyle&  style         = ImGui::GetStyle();

	const ImVec2 label_size    = ImGui::CalcTextSize( "Pause", NULL, true );
	ImVec2       play_btn_size = ImGui::CalcItemSize( { 0, 0 }, label_size.x + style.FramePadding.x * 2.0f, label_size.y + style.FramePadding.y * 2.0f );

	if ( paused )
	{
		if ( ImGui::Button( "Play", play_btn_size ) )
		{
			const char* cmd[]   = { "set", "pause", "no", NULL };
			int         cmd_ret = p_mpv_command_async( g_mpv, 0, cmd );
		}
	}
	else
	{
		if ( ImGui::Button( "Pause", play_btn_size ) )
		{
			const char* cmd[]   = { "set", "pause", "yes", NULL };
			int         cmd_ret = p_mpv_command_async( g_mpv, 0, cmd );
		}
	}

#if 0
	ImGui::SameLine();
	ImGui::Spacing();

	ImGui::SameLine();
	if ( ImGui::Button( "<|" ) )
	{
		const char* cmd[]   = { "seek", "0", "absolute", NULL };
		int         cmd_ret = p_mpv_command_async( g_mpv, 0, cmd );
	}

	ImGui::SameLine();
	if ( ImGui::Button( "|>" ) )
	{
		char duration_str[ 16 ];
		gcvt( duration, 4, duration_str );

		// const char* cmd[]   = { "seek", duration_str, "absolute", NULL };
		const char* cmd[]   = { "seek", "100", "absolute-percent+exact", NULL };
		int         cmd_ret = p_mpv_command_async( g_mpv, 0, cmd );
	}
#endif

	ImGui::SameLine();
	//ImGui::Spacing();

	ImGui::SameLine();
	if ( ImGui::Button( "<" ) )
	{
		const char* cmd[]   = { "frame-back-step", NULL };
		int         cmd_ret = p_mpv_command_async( g_mpv, 0, cmd );
	}

	ImGui::SameLine();
	if ( ImGui::Button( ">" ) )
	{
		const char* cmd[]   = { "frame-step", NULL };
		int         cmd_ret = p_mpv_command_async( g_mpv, 0, cmd );
	}
	
	ImGui::SameLine();

	// https://stackoverflow.com/questions/3673226/how-to-print-time-in-format-2009-08-10-181754-811

	char str_time_pos[ TIME_BUFFER ]{ 0 };
	char str_duration[ TIME_BUFFER ]{ 0 };

	util_format_time( str_time_pos, time_pos );
	util_format_time( str_duration, duration );

	char         str_time[ TIME_BUFFER + TIME_BUFFER + 4 ]{};

	snprintf( str_time, TIME_BUFFER + TIME_BUFFER + 4, "%s / %s", str_time_pos, str_duration );

	// const ImVec2 time_size       = ImGui::CalcTextSize( str_time, NULL, true );

	// float        avaliable_width = ImGui::GetContentRegionAvail()[ 0 ] - ( style.ItemSpacing.x * 2 );
	// float        avaliable_width = 500.f - ( style.ItemSpacing.x * 2 );
	float        vol_bar_width   = 96.f;

	// float        seek_bar_width  = avaliable_width;
	// seek_bar_width -= ( vol_bar_width + time_size.x + ( style.ItemSpacing.x * 2 ) );

	ImGui::SetNextItemWidth( 200.f );

	float time_pos_f = (float)time_pos;
	if ( ImGui::SliderFloat( "##seek", &time_pos_f, 0.f, (float)duration ) )
	{
		// convert float to string in c
		char time_pos_str[ 16 ];
		gcvt( time_pos_f, 4, time_pos_str );

		const char* cmd[]   = { "seek", time_pos_str, "absolute", NULL };
		int         cmd_ret = p_mpv_command_async( g_mpv, 0, cmd );
	}

	ImGui::SameLine();

	int muted = 0;
	p_mpv_get_property( g_mpv, "mute", MPV_FORMAT_FLAG, &muted );

	if ( muted )
	{
		ImGui::PushStyleColor( ImGuiCol_Button, ImGui::GetStyleColorVec4( ImGuiCol_ButtonActive ) );
	}

	if ( ImGui::Button( "M" ) )
	{
		const char* cmd[]   = { "cycle", "mute", NULL };
		int         cmd_ret = p_mpv_command_async( g_mpv, 0, cmd );
	}

	if ( muted )
	{
		ImGui::PopStyleColor();
	}

	double volume = 0;
	p_mpv_get_property( g_mpv, "volume", MPV_FORMAT_DOUBLE, &volume );

	ImGui::SameLine();
	ImGui::SetNextItemWidth( vol_bar_width );

	float volume_f = volume;
	if ( ImGui::SliderFloat( "##Volume", &volume_f, 0.f, 130.f ) )
	{
		// convert float to string in c
		char volume_str[ 16 ];
		gcvt( volume_f, 4, volume_str );

		const char* cmd[]   = { "set", "volume", volume_str, NULL };
		int         cmd_ret = p_mpv_command_async( g_mpv, 0, cmd );
	}

	ImGui::SameLine();

	// ImGui::Text( "%s / %s", str_time_pos, str_duration );
	ImGui::TextUnformatted( str_time );
	// ImGui::ProgressBar( time_pos / duration );

	controls_height = ImGui::GetWindowContentRegionMax().y;

	ImGui::End();
}


void media_view_draw_imgui()
{
	media_view_input();

	if ( get_media_type() == e_media_type_video )
	{
		media_view_draw_video_controls();
	}
	else if ( get_media_type() == e_media_type_image_animated )
	{
		// Draw frame controls
	}
}


float vertices[] = {
	// positions          // colors           // texture coords
	0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f,    // top right
	0.5f, -0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,   // bottom right
	-0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,  // bottom left
	-0.5f, 0.5f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f    // top left
};


static void media_view_draw_image()
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

	int width, height;
	SDL_GetWindowSize( g_main_window, &width, &height );

	glEnable( GL_BLEND );
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

	glEnable( GL_TEXTURE_2D );
	glBindTexture( GL_TEXTURE_2D, g_image_data.texture );

 	glMatrixMode( GL_PROJECTION );
	glLoadIdentity();

 	//glOrtho( g_image_pos.x, g_image_pos.x + g_image_size.x, g_image_pos.y, g_image_pos.y + g_image_size.y, -1, 1 );
	glOrtho( 0, width, height, 0, -1, 1 );

	glTranslatef( dst_rect.w / 2, dst_rect.h / 2, 0 );  // M1 - 2nd translation
	glRotatef( g_image_rot, 0.0f, 0.0f, 1.0f );
	glTranslatef( -dst_rect.w / 2, -dst_rect.h / 2, 0 );  // M3 - 1st translation

 	glMatrixMode( GL_MODELVIEW );
	glLoadIdentity();
 
 	glBegin( GL_QUADS );
 	glTexCoord2f( 0, 0 );
	glVertex2f( dst_rect.x, dst_rect.y );
 	glTexCoord2f( 1, 0 );
	glVertex2f( dst_rect.x + dst_rect.w, dst_rect.y );
 	glTexCoord2f( 1, 1 );
	glVertex2f( dst_rect.x + dst_rect.w, dst_rect.y + dst_rect.h );
 	glTexCoord2f( 0, 1 );
	glVertex2f( dst_rect.x, dst_rect.y + dst_rect.h );
 	glEnd();

	glDisable( GL_TEXTURE_2D );
	glDisable( GL_BLEND );

	// SDL_RenderTexture( g_main_renderer, g_image_data.texture, NULL, &dst_rect );

//	SDL_RenderTextureRotated( g_main_renderer, g_image_data.texture, NULL, &dst_rect, g_image_rot, nullptr, SDL_FLIP_NONE );
}


void media_view_draw()
{
	if ( get_media_type() == e_media_type_video )
	{
		mpv_draw_frame();
	}
	else
	{
		media_view_draw_image();
	}
}

