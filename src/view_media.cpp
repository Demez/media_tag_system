#include "main.h"
#include "imgui_internal.h"

#include "stb_image_resize2.h"

#include <thread>
#include <mutex>

// Image Draw Data
namespace image_draw
{
	e_zoom_mode zoom_mode = e_zoom_mode_fit;
	double      zoom      = 1.f;
	ImVec2      pos{};
	ImVec2      size{};
	bool        flip_v = false;
	bool        flip_h = false;
	float       rot    = 0.f;
}

// This doesn't let you move the image outside the window
// if zoomed in, and you move the image down and to the right, the top left corner of the image will be at the top left
// or if zoomed out, you can't pan the image, the image is in the middle of the window
//bool                     g_lock_image_panning_area = false;

// Image Panning
bool                     g_image_pan               = false;

// Draw Information
bool                     g_draw_media_info         = false;
bool                     g_draw_imgui_demo         = false;
bool                     g_draw_mem_stats          = false;
bool                     g_draw_zoom_level         = true;

constexpr double         ZOOM_AMOUNT = 0.1;
constexpr double         ZOOM_MIN    = 0.01;

// Image Scaling

enum e_scale_state
{
	e_scale_state_idle,
	e_scale_state_start,     // main thread sets state to this when it wants to scale the current image
	e_scale_state_working,   // main thread looks at this state when running, uses full image while waiting
	e_scale_state_upload,    // main thread needs to upload to gpu, after this, it's set back to idle
};

constexpr float      SCALE_WAIT_TIME = 0.1f;

static std::thread*  g_scale_thread;
static e_scale_state g_scale_state  = e_scale_state_idle;
static float         g_scale_timer  = -1.f;
static bool          g_scale_use    = false;  // use scaled down image
static image_t       g_scale_src{};

std::mutex           g_scale_lock;


void media_view_filter_image()
{
	g_scale_lock.lock();

	if ( g_image_scaled_data.image.frame.size() )
		ch_free( e_mem_category_stbi_resize, g_image_scaled_data.image.frame[ 0 ] );

	g_image_scaled_data.image = g_scale_src;

	g_image_scaled_data.image.frame.clear();
	g_image_scaled_data.image.frame.resize( g_scale_src.frame.size() );

	// Downscale image if size is larger than target size
	if ( image_draw::size.x < g_scale_src.width )
	{
		//u8*   result_image_data      = stbir_resize_uint8_linear(
		//  old_frame, thumbnail->image->width, thumbnail->image->height, 0,
		//  nullptr, new_width, new_height, 0, STBIR_RGBA );

		int new_width     = image_draw::size.x;
		int new_height    = image_draw::size.y;

		if ( image_downscale( &g_scale_src, &g_image_scaled_data.image, new_width, new_height ) )
			g_scale_state = e_scale_state_upload;

		if ( g_scale_src.width != g_image_scaled_data.image.width )
			printf( "lol knew it, different image being handled\n" );
	}
	else
	{
		g_scale_state = e_scale_state_idle;
	}

	g_scale_lock.unlock();
}


void media_view_scale_thread_run()
{
	while ( app::running )
	{
		if ( g_scale_state != e_scale_state_start )
		{
			SDL_Delay( 250 );
			continue;
		}

		g_scale_state = e_scale_state_working;

		media_view_filter_image();
	}
}


void media_view_scale_check_timer( float frame_time )
{
	if ( g_scale_timer < 0.f )
		return;

	g_scale_timer -= frame_time;

	if ( g_scale_timer < 0.f && g_image_scaled_data.image.frame.size() && g_scale_state == e_scale_state_idle )
	{
		g_scale_lock.lock();

		image_free( g_scale_src );

		g_scale_src = g_image_data.image;

		g_scale_src.frame.clear();
		g_scale_src.frame.resize( g_image_data.image.frame.size() );

		// don't hold onto this
		g_scale_src.image_format = nullptr;

		size_t image_size        = (size_t)g_image_data.image.width * (size_t)g_image_data.image.height * (size_t)g_image_data.image.bytes_per_pixel;
		g_scale_src.frame[ 0 ] = ch_calloc< u8 >( image_size, e_mem_category_image_data );
		memcpy( g_scale_src.frame[ 0 ], g_image_data.image.frame[ 0 ], image_size * sizeof( u8 ) );

		g_image_scaled_data.index = gallery::cursor;
		g_scale_state             = e_scale_state_start;

		g_scale_lock.unlock();
	}
}


void media_view_scale_reset_timer()
{
	g_scale_timer = SCALE_WAIT_TIME;
	g_scale_use   = false;
}


void media_view_init()
{
	g_scale_thread = new std::thread( media_view_scale_thread_run );
}


void media_view_shutdown()
{
	// wait for scale thread to shutdown
	g_scale_thread->join();

	delete g_scale_thread;
}


static e_media_type get_media_type()
{
	if ( gallery::items.size() <= gallery::cursor )
		return e_media_type_none;

	return gallery_item_get_media_entry( gallery::cursor ).type;
}


void media_view_fit_in_view( bool adjust_zoom, bool center_image )
{
	// new image size
	int width, height;
	SDL_GetWindowSize( app::window, &width, &height );

	// Fit image in window size
	float factor[ 2 ] = { 1.f, 1.f };

	if ( g_image_data.image.width > width )
		factor[ 0 ] = (float)width / (float)g_image_data.image.width;

	if ( g_image_data.image.height > height )
		factor[ 1 ] = (float)height / (float)g_image_data.image.height;

	if ( adjust_zoom )
	{
		float zoom_level = std::min( factor[ 0 ], factor[ 1 ] );

		image_draw::size.x    = g_image_data.image.width * zoom_level;
		image_draw::size.y    = g_image_data.image.height * zoom_level;

		image_draw::zoom     = zoom_level;

		image_draw::zoom_mode = e_zoom_mode_fit;
	}

	// TODO: only adjust this if needed, check image zoom type
	// if image doesn't fit window size, keep locked to center

	//if ( factor[ 0 ] < 1.f )
		image_draw::pos.x = width / 2 - ( image_draw::size.x / 2 );

	//if ( factor[ 1 ] < 1.f )
		image_draw::pos.y = height / 2 - ( image_draw::size.y / 2 );

	media_view_scale_reset_timer();
}


void media_view_zoom_reset()
{
	// keep where we are centered on in the image
	image_draw::pos.x = 1 / image_draw::zoom * ( image_draw::pos.x );
	image_draw::pos.y = 1 / image_draw::zoom * ( image_draw::pos.y );

	image_draw::zoom  = 1.0;

	image_draw::zoom_mode = e_zoom_mode_fixed;

	if ( !g_image_data.image.frame[ 0 ] )
		return;

	image_draw::size.x = g_image_data.image.width;
	image_draw::size.y = g_image_data.image.height;

	media_view_scale_reset_timer();
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
		if ( image_draw::zoom >= 100.0 )
			return;

		factor += ( ZOOM_AMOUNT * scroll );
	}
	else
	{
		// min zoom level
		if ( image_draw::zoom <= ZOOM_MIN )
			return;

		factor -= ( ZOOM_AMOUNT * abs( scroll ) );
	}

	// TODO: add zoom levels to snap to here
	// 100, 200, 400, 500, 50, 25, etc

	// auto rounded_zoom = std::max( 0.01f, roundf( image_draw::zoom * factor * 100 ) / 100 );
	// 
	// if ( fmod( rounded_zoom, 1.0 ) == 0 )
	// 	factor = rounded_zoom / image_draw::zoom;

	double old_zoom = image_draw::zoom;

	image_draw::zoom    = (double)( std::max( 1.f, image_draw::size.x ) * factor ) / (double)g_image_data.image.width;

	// round it so we don't get something like 0.9999564598 or whatever instead of 1.0
	//if ( image_draw::zoom < 0.01 )
	//{
	//	image_draw::zoom = std::max( ZOOM_MIN, round( image_draw::zoom * 10000 ) / 10000 );
	//}
	//else
	{
		image_draw::zoom = std::max( ZOOM_MIN, round( image_draw::zoom * 1000 ) / 1000 );
	}

	// recalculate draw width and height
	image_draw::size.x = (double)g_image_data.image.width * image_draw::zoom;
	image_draw::size.y = (double)g_image_data.image.height * image_draw::zoom;

	// recalculate image position to keep image where cursor is

	// New Position = Scale Origin + ( Scale Point - Scale Origin ) * Scale Factor
	image_draw::pos.x  = (double)app::mouse_pos[ 0 ] + ( image_draw::pos.x - (double)app::mouse_pos[ 0 ] ) * factor;
	image_draw::pos.y  = (double)app::mouse_pos[ 1 ] + ( image_draw::pos.y - (double)app::mouse_pos[ 1 ] ) * factor;

	media_view_scale_reset_timer();

	image_draw::zoom_mode = e_zoom_mode_fixed;
}


void media_view_draw_media_info()
{
	if ( gallery::items.empty() )
		return;

	if ( !ImGui::Begin( "##media_info", 0, ImGuiWindowFlags_NoTitleBar ) )
	{
		ImGui::End();
		return;
	}

	gallery_item_t& item = gallery::items[ gallery::cursor ];
	media_entry_t entry  = gallery_item_get_media_entry( gallery::cursor );

	ImGui::TextUnformatted( entry.filename.c_str() );

	ImGui::Separator();

	ImGui::Text( "Size: %.3f MB", (float)item.file_size / ( MEM_SCALE * MEM_SCALE ) );

	char date_created[ DATE_TIME_BUFFER ]{};
	char date_mod[ DATE_TIME_BUFFER ]{};

	util_format_date_time( date_created, DATE_TIME_BUFFER, item.date_created );
	util_format_date_time( date_mod, DATE_TIME_BUFFER, item.date_mod );

	ImGui::Text( "Date Created: %s", date_created );
	ImGui::Text( "Date Modified: %s", date_mod );

	ImGui::Separator();

	if ( get_media_type() == e_media_type_video )
	{
	}
	else
	{

		// Image Type
		ImGui::Text( "Size: %dx%d", g_image_data.image.width, g_image_data.image.height );
		ImGui::Text( "Type: %s", g_image_data.image.image_format );

		switch ( g_image_data.image.format )
		{
			default:
				ImGui::Text( "Format: Unhandled GL Format %d", g_image_data.image.format );
				break;
			case GL_RGB:
				ImGui::TextUnformatted( "GL Format: RGB" );
				break;
			case GL_RGBA:
				ImGui::TextUnformatted( "GL Format: RGBA" );
				break;
			case GL_RGBA16:
				ImGui::TextUnformatted( "GL Format: RGBA16" );
				break;
		}
	}

	ImGui::End();
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
		image_draw::rot -= 90;

	ImGui::SameLine();

	if ( ImGui::Button( "RR" ) )
		image_draw::rot += 90;

	ImGui::SameLine();

	if ( ImGui::Button( "R" ) )
		image_draw::rot = 0;

	ImGui::SameLine();

	if ( ImGui::Button( "H" ) )
		image_draw::flip_h = !image_draw::flip_h;

	ImGui::SameLine();

	if ( ImGui::Button( "V" ) )
		image_draw::flip_v = !image_draw::flip_v;

	ImGui::Separator();

	ImGui::SliderFloat( "rotate", &image_draw::rot, 0, 360 );

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
		image_draw::rot -= 90;
	}

	if ( ImGui::MenuItem( "Rotate Right", nullptr, false, g_image_data.texture ) )
	{
		image_draw::rot += 90;
	}

	// ImGui::SliderFloat( "Rotate Slider", &image_draw::rot, 0, 360 );

	if ( ImGui::MenuItem( "Reset Rotation", nullptr, false, g_image_data.texture ) )
	{
		image_draw::rot = 0;
	}

	if ( ImGui::MenuItem( "Flip Horizontally", nullptr, image_draw::flip_h, g_image_data.texture) )
	{
		image_draw::flip_h = !image_draw::flip_h;
	}

	if ( ImGui::MenuItem( "Flip Vertically", nullptr, image_draw::flip_v, g_image_data.texture ) )
	{
		image_draw::flip_v = !image_draw::flip_v;
	}

	ImGui::Separator();
#endif

	if ( ImGui::MenuItem( "Open File Location", nullptr, false, g_image_data.texture ) )
	{
		sys_browse_to_file( gallery_item_get_path_string( gallery::cursor ).c_str() );
	}

	if ( ImGui::BeginMenu( "Open With" ) )
	{
		// TODO: list programs to open the file with, like fragment image viewer
		// how would this work on linux actually? hmm
		ImGui::MenuItem( "nothing lol", nullptr, false, false );
		ImGui::EndMenu();
	}

	if ( ImGui::MenuItem( "Copy File", nullptr, false ) )
	{
		sys_copy_to_clipboard( gallery_item_get_path_string( gallery::cursor ).data() );
		push_notification( "Copied" );
	}

	if ( ImGui::MenuItem( "Copy File Data", nullptr, false, false ) )
	{
	}

	if ( ImGui::MenuItem( "Set As Desktop Background", nullptr, false, false ) )
	{
	}

	if ( ImGui::MenuItem( "File Properties", nullptr, false ) )
	{
		// TODO: create our own imgui file properties for more info
		// Plat_OpenFileProperties( ImageView_GetImagePath() );

		sys_open_file_properties( gallery_item_get_path_string( gallery::cursor ).c_str() );
	}

	// TODO: side menu to show information on the image or video overlayed next to the image in a window
	ImGui::MenuItem( "Media Info", nullptr, &g_draw_media_info, true );

	ImGui::Separator();

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

	ImGui::Separator();

	ImGui::MenuItem( "Demo Window", nullptr, &g_draw_imgui_demo, true );
	ImGui::MenuItem( "Memory Stats", nullptr, &g_draw_mem_stats, true );

	// 	if ( ImGui::MenuItem( "Show ImGui Demo", nullptr, gShowImGuiDemo ) )
	// 	{
	// 		gShowImGuiDemo = !gShowImGuiDemo;
	// 	}

	ImGui::EndPopup();
}


void media_view_input()
{
	if ( g_scale_state == e_scale_state_upload )
	{
		if ( g_image_scaled_data.index == gallery::cursor )
		{
			gl_update_texture( g_image_scaled_data.texture, &g_image_scaled_data.image );
			g_scale_use = true;
			printf( "Scaled Main Image\n" );
		}
		else
		{
			printf( "SCALE MISMATCH\n" );
		}

		g_scale_state  = e_scale_state_idle;
	}

	// for video view
	if ( !ImGui::IsKeyDown( ImGuiKey_RightCtrl ) )
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

	// TODO: Test ImGui::Shortcut()
	if ( app::window_focused && ImGui::IsKeyDown( ImGuiKey_LeftCtrl ) && ImGui::IsKeyPressed( ImGuiKey_C, false ) )
	{
		sys_copy_to_clipboard( gallery_item_get_path_string( gallery::cursor ).data() );
		printf( "Copied to Clipboard\n" );
		push_notification( "Copied" );
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
		image_draw::pos.x += app::mouse_delta[ 0 ];
		image_draw::pos.y += app::mouse_delta[ 1 ];
	}
}


void media_view_window_resize()
{
	if ( image_draw::zoom_mode == e_zoom_mode_fit || image_draw::zoom_mode == e_zoom_mode_fit_width )
	{
		media_view_fit_in_view();
	}
}


void media_view_load()
{
	if ( gallery::items.empty() )
		return;

	if ( gallery::cursor >= gallery::items.size() )
		return;

	float             load_time    = 0.f;
	// gallery_item_t&   gallery_item = gallery::items[ gallery::cursor ];
	media_entry_t     entry        = gallery_item_get_media_entry( gallery::cursor );

	image_load_info_t image_load_info{};
	image_load_info.image = &g_image_data.image;

	{
		auto startTime = std::chrono::high_resolution_clock::now();

		if ( entry.type == e_media_type_image )
		{
			// g_image_view.image = g_test_codec->image_load( directory::media_list[ g_folder_index ] );
			image_load( entry.path, image_load_info );
			mpv_cmd_close_video();
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
			if ( image_load_info.image->frame.size() > 0 && image_load_info.image->bytes_per_pixel > 0 )
			{
				gl_update_texture( g_image_data.texture, &g_image_data.image );
				media_view_fit_in_view();
			}
			else
			{
				printf( "%f FAILED Load - %s\n", load_time, entry.path.string().c_str() );
			}
		}

		// auto  currentTime    = std::chrono::high_resolution_clock::now();
		// float up_time        = std::chrono::duration< float, std::chrono::seconds::period >( currentTime - startTime ).count();
		//printf( "%f Load - %f Up - %s\n", load_time, up_time, directory::media_list[ g_folder_index ].string().c_str() );
		printf( "%f Load - %s\n", load_time, entry.path.string().c_str() );
	}

	g_image_data.index = gallery::cursor;

	update_window_title();

	media_view_scale_reset_timer();
}


void media_view_advance( bool prev )
{
	if ( gallery::items.size() <= 1 )
		return;

	if ( get_media_type() == e_media_type_video )
		mpv_cmd_close_video();

advance:
	if ( prev )
	{
		if ( gallery::cursor == 0 )
			gallery::cursor = gallery::items.size();

		gallery::cursor--;
	}
	else
	{
		gallery::cursor++;

		if ( gallery::cursor == gallery::items.size() )
			gallery::cursor = 0;
	}

	if ( gallery_item_get_media_entry( gallery::cursor ).type == e_media_type_directory )
		goto advance;

	media_view_load();
}


void media_view_draw_video_controls()
{
	if ( !g_mpv )
		return;

	if ( ImGui::IsKeyPressed( ImGuiKey_Space, false ) || ( !mouse_hovering_imgui_window() && ImGui::IsKeyPressed( ImGuiKey_MouseLeft, false ) ) )
	{
		const char* cmd[]   = { "cycle", "pause", NULL };
		int         cmd_ret = p_mpv_command_async( g_mpv, 0, cmd );
	}

	// Seeking
	if ( !mouse_hovering_imgui_window() )
	{
		if ( ImGui::IsKeyDown( ImGuiKey_RightCtrl ) && ImGui::IsKeyPressed( ImGuiKey_LeftArrow, true ) )
		{
			const char* cmd[]   = { "seek", "-5", NULL };
			int         cmd_ret = p_mpv_command_async( g_mpv, 0, cmd );
		}
		if ( ImGui::IsKeyDown( ImGuiKey_RightCtrl ) && ImGui::IsKeyPressed( ImGuiKey_RightArrow, true ) )
		{
			const char* cmd[]   = { "seek", "5", NULL };
			int         cmd_ret = p_mpv_command_async( g_mpv, 0, cmd );
		}
	}

	int width, height;
	SDL_GetWindowSize( app::window, &width, &height );

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

	ImGui::SetNextWindowSizeConstraints( { width - 80.f, -1.f }, { width - 80.f, -1.f } );

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

	const ImVec2 time_size       = ImGui::CalcTextSize( str_time, NULL, true );
	const ImVec2 other_text_size       = ImGui::CalcTextSize( "M", NULL, true );

	float        avaliable_width = ImGui::GetContentRegionAvail()[ 0 ] - ( style.ItemSpacing.x * 2 );
	// float        avaliable_width = 500.f - ( style.ItemSpacing.x * 2 );
	float        vol_bar_width         = 96.f;
	ImVec2       other_text_area       = ImGui::CalcItemSize( { 0, 0 }, other_text_size.x + style.FramePadding.x * 2.0f, other_text_size.y + style.FramePadding.y * 2.0f );

	float        seek_bar_width        = width - 80.f;
	seek_bar_width -= ( play_btn_size.x + vol_bar_width + time_size.x + ( other_text_area.x * 3 ) + ( style.ItemSpacing.x * 7 ) );
	// seek_bar_width -= ( play_btn_size.x + vol_bar_width + time_size.x + ( style.ItemSpacing.x * 2 ) );

	ImGui::SetNextItemWidth( seek_bar_width );

	float time_pos_f = (float)time_pos;
	if ( ImGui::SliderFloat( "##seek", &time_pos_f, 0.f, (float)duration, "" ) )
	{
		// convert float to string in c
		char time_pos_str[ 16 ];
		gcvt( time_pos_f, 4, time_pos_str );

		const char* cmd[]   = { "seek", time_pos_str, "absolute", NULL };
		int         cmd_ret = p_mpv_command_async( g_mpv, 0, cmd );
	}

	ImGui::SameLine();
	ImGui::TextUnformatted( str_time );

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

	int volume_value = volume;
	if ( ImGui::SliderInt( "##Volume", &volume_value, 0, 130 ) )
	{
		char volume_str[ 16 ];
		snprintf( volume_str, 16, "%d", volume_value );

		const char* cmd[]   = { "set", "volume", volume_str, NULL };
		int         cmd_ret = p_mpv_command_async( g_mpv, 0, cmd );
	}

	// ImGui::Text( "%s / %s", str_time_pos, str_duration );
	// ImGui::ProgressBar( time_pos / duration );

	controls_height = ImGui::GetWindowContentRegionMax().y;

	ImGui::End();
}


void media_view_draw_imgui()
{
	media_view_input();

	if ( g_draw_media_info )
		media_view_draw_media_info();

	if ( g_draw_imgui_demo )
		ImGui::ShowDemoWindow();

	if ( g_draw_mem_stats )
	{
		if ( ImGui::Begin( "Memory Stats" ) )
		{
			mem_draw_debug_ui();
			ImGui::End();
		}
	}

	if ( get_media_type() == e_media_type_video )
	{
		media_view_draw_video_controls();
	}
	else
	{
		if ( get_media_type() == e_media_type_image_animated )
		{
			// Draw frame controls
		}

		if ( g_draw_zoom_level )
		{
			ImGui::Begin( "##zoom_level", 0, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration );
			ImGui::Text( "%.1f%%", (float)( image_draw::zoom * 100 ) );
			ImGui::End();
		}
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
	dst_rect.w = image_draw::size.x;
	dst_rect.h = image_draw::size.y;
	dst_rect.x = image_draw::pos.x;
	dst_rect.y = image_draw::pos.y;

	if ( image_draw::flip_h )
	{
		dst_rect.w = -image_draw::size.x;
		dst_rect.x += image_draw::size.x;
	}

	if ( image_draw::flip_v )
	{
		dst_rect.h = -image_draw::size.y;
		dst_rect.y += image_draw::size.y;
	}

	int width, height;
	SDL_GetWindowSize( app::window, &width, &height );

	glEnable( GL_BLEND );
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

	glEnable( GL_TEXTURE_2D );

	if ( g_scale_use )
	{
		glBindTexture( GL_TEXTURE_2D, g_image_scaled_data.texture );
	}
	else
	{
		glBindTexture( GL_TEXTURE_2D, g_image_data.texture );
	}

 	glMatrixMode( GL_PROJECTION );
	glLoadIdentity();

	glOrtho( 0, width, height, 0, -1, 1 );

 	glMatrixMode( GL_MODELVIEW );
	glLoadIdentity();

	// get the center of the image
	float image_center_x = dst_rect.x + dst_rect.w * 0.5f;
	float image_center_y = dst_rect.y + dst_rect.h * 0.5f;

	glTranslatef( image_center_x, image_center_y, 0.0f );    // move pivot to center of the image
	glRotatef( image_draw::rot, 0, 0, 1 );                       // rotate around the image
	glTranslatef( -image_center_x, -image_center_y, 0.0f );  // move back
 
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

