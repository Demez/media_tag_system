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
	int         zoom_step = 0;  // 0 = 100% zoom

	ImVec2      pos{};
	ImVec2      size{};
	bool        flip_v             = false;
	bool        flip_h             = false;
	float       rot                = 0.f;

	// Animated image playback information
	u64         last_frame_time    = 0;  // time in system time
	u64         next_frame_time    = 0;  // time until next frame, add to last_frame_time
	size_t      frame              = 0;
	double      playback_speed     = 1.0;
	bool        pause              = false;
	bool        scaling            = true;
	float       volume             = 100.0;

	bool        show_raw_frames    = false;
	bool        hide_alpha         = false;
	bool        invert_color       = false;
	bool        show_channels[ 4 ] = { true, true, true, true };

	// index into gallery::sorted_media
	//size_t      media_index      = 0;
}


// This doesn't let you move the image outside the window
// if zoomed in, and you move the image down and to the right, the top left corner of the image will be at the top left
// or if zoomed out, you can't pan the image, the image is in the middle of the window
//bool                     g_lock_image_panning_area = false;

// Image Panning
bool                     g_image_pan               = false;

// Waiting for mouse movement to enter pan mode
bool                     g_image_pan_wait          = false;

// Draw Information
bool                     g_draw_media_info         = false;
bool                     g_draw_imgui_demo         = false;
bool                     g_draw_mem_stats          = false;
bool                     g_draw_zoom_level         = true;


void media_view_frame_update_timer()
{
	image_draw::last_frame_time = sys_get_time_ms();
	image_draw::next_frame_time = g_image_data.image.frame[ image_draw::frame ].time * 1000.f;
}


void media_view_frame_advance( bool backwards = false )
{
	if ( backwards )
	{
		if ( image_draw::frame == 0 )
			image_draw::frame = g_image_data.image.frame.size();

		image_draw::frame--;
	}
	else
	{
		image_draw::frame = ( image_draw::frame + 1 ) % g_image_data.image.frame.size();
	}

	media_view_frame_update_timer();
}


void media_view_frame_set( size_t frame )
{
	if ( frame >= g_image_data.image.frame.size() )
		return;

	image_draw::frame = frame;
	media_view_frame_update_timer();
}


void media_view_update()
{
	media_view_scale_check_timer();
	
	// Check if the animated image needs to advance a frame
	if ( g_image_data.image.frame.size() > 1 )
	{
		if ( !image_draw::pause )
		{
			u64 current_time = sys_get_time_ms();
			u64 next_time    = image_draw::last_frame_time + ( image_draw::next_frame_time * image_draw::playback_speed );

			if ( next_time < current_time )
			{
				media_view_frame_advance();
			}
		}
	}
	else
	{
		image_draw::last_frame_time = 0;
		image_draw::next_frame_time = 0;
	}
}


void media_view_init()
{
	if ( !media_view_render_init() )
		return;
	
	media_scale_init();
}


void media_view_shutdown()
{
	media_scale_shutdown();
}


e_media_type get_media_type()
{
	if ( gallery::sorted_media.size() <= g_image_data.index )
		return e_media_type_none;

	return gallery_item_get_media_entry( g_image_data.index ).type;
}


bool media_view_can_pan_image()
{
	int width, height;
	SDL_GetWindowSize( app::window, &width, &height );

	if ( width >= image_draw::size.x && height >= image_draw::size.y )
		return false;

	return true;
}


void media_view_draw_media_info()
{
	if ( gallery::sorted_media.empty() )
		return;

	if ( !ImGui::Begin( "##media_info", 0, ImGuiWindowFlags_NoTitleBar ) )
	{
		ImGui::End();
		return;
	}

	media_entry_t entry = gallery_item_get_media_entry( g_image_data.index );

	ImGui::TextUnformatted( entry.file.name.c_str() );

	ImGui::Separator();

	ImGui::Text( "Size: %.3f MB", (float)entry.file.size / ( STORAGE_SCALE * STORAGE_SCALE ) );

	char date_created[ DATE_TIME_BUFFER ]{};
	char date_mod[ DATE_TIME_BUFFER ]{};

	util_format_date_time( date_created, DATE_TIME_BUFFER, entry.file.date_created );
	util_format_date_time( date_mod, DATE_TIME_BUFFER, entry.file.date_mod );

	ImGui::Text( "Date Created: %s", date_created );
	ImGui::Text( "Date Modified: %s", date_mod );

	if ( get_media_type() == e_media_type_video )
	{
		ImGui::Separator();
	}
	else
	{
		// Scaling Info
		ImGui::SeparatorText( "Scaling" );

		ImGui::Text( "Scale Thread State: %s", media_get_scale_state_str() );
		ImGui::Text( "Scaled: %dx%d", g_image_scaled_data.image.width, g_image_scaled_data.image.height );
		ImGui::Text( "Render Size: %.0fx%.0f", image_draw::size.x, image_draw::size.y );
		ImGui::Text( "Render Pos: %.0fx%.0f", image_draw::pos.x, image_draw::pos.y );

		ImGui::SeparatorText( "Image Info" );

		// Image Type
		ImGui::Text( "Size: %dx%d", g_image_data.image.width, g_image_data.image.height );

		ImGui::Text( "Type: %s", g_image_data.image.image_format );
		ImGui::Text( "Frame Count: %zu", g_image_data.image.frame.size() );
		ImGui::Text( "Channels: %d", g_image_data.image.channels );

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
			case GL_BGR:
				ImGui::TextUnformatted( "GL Format: BGR" );
				break;
			case GL_BGRA:
				ImGui::TextUnformatted( "GL Format: BGRA" );
				break;
			case GL_RGBA16:
				ImGui::TextUnformatted( "GL Format: RGBA16" );
				break;
			case GL_R8:
				ImGui::TextUnformatted( "GL Format: R8" );
				break;
			case GL_ALPHA:
				ImGui::TextUnformatted( "GL Format: ALPHA" );
				break;
		}

		if ( ImGui::CollapsingHeader( "Frame Data" ) )
		{
			for ( size_t frame_i = 0; frame_i < g_image_data.image.frame.size(); frame_i++ )
			{
				ImGui::Separator();

				ImGui::Text( "Frame %zu", frame_i );
				ImGui::Text( "Duration: %.3f", g_image_data.image.frame[ frame_i ].time );
				ImGui::Text( "Size: %dx%d", g_image_data.image.frame[ frame_i ].width, g_image_data.image.frame[ frame_i ].height );
			}
		}
	}

	ImGui::End();
}


void rotate_image( float rot )
{
	image_draw::rot += rot;

	if ( image_draw::rot < 0.f )
		image_draw::rot += 360.f;
	else if ( image_draw::rot > 360.f )
		image_draw::rot -= 360.f;

	image_draw::rot = CLAMP( image_draw::rot, 0.f, 360.f );
}


void media_view_context_menu()
{
	if ( !ImGui::BeginPopupContextVoid( "main ctx menu" ) )
		return;

	// hack lol
	// app::draw_frame          = true;

	ImGuiStyle& style        = ImGui::GetStyle();
	ImVec2      region_avail = ImGui::GetContentRegionAvail();

	float       button_width = ( region_avail.x / 2 ) + style.ItemSpacing.x;

	if ( ImGui::Button( "Fit", { 0, 0 } ) )
	{
		image_draw::zoom_mode = e_zoom_mode_fit;
		media_view_fit_in_view();
	}

	util_imgui_set_tooltip( "Fits the Image within the window up to 100%% zoom" );

	ImGui::SameLine();

	if ( ImGui::Button( "Fit Window", { 0, 0 } ) )
	{
		image_draw::zoom_mode = e_zoom_mode_fit_window;
		media_view_fit_in_view();
	}

	util_imgui_set_tooltip( "Image is as large as possible within the window" );

	ImGui::SameLine();

	if ( ImGui::Button( "100%", { 0, 0 } ) )
		media_view_zoom_reset();

	ImGui::SameLine();

	if ( ImGui::Button( "Center", { 0, 0 } ) )
		media_view_fit_in_view( false );

	ImGui::Separator();

	// ?????
	float saved_pos_y = ImGui::GetCursorPosY();
	ImGui::SetCursorPosY( saved_pos_y + style.FramePadding.y );

	ImGui::TextUnformatted( "Rotate" );
	ImGui::SameLine();
	ImGui::SetCursorPosY( saved_pos_y );

	if ( ImGui::Button( "L" ) )
		rotate_image( -90 );

	ImGui::SameLine();
	ImGui::SetCursorPosY( saved_pos_y );

	if ( ImGui::Button( "R" ) )
		rotate_image( 90 );

	ImGui::SameLine();
	ImGui::SetCursorPosY( saved_pos_y );

	if ( ImGui::Button( "Reset" ) )
		image_draw::rot = 0;

	ImGui::SameLine();
	ImGui::SetCursorPosY( saved_pos_y );
	ImGui::SeparatorEx( ImGuiSeparatorFlags_Vertical );
	ImGui::SameLine();
	ImGui::SetCursorPosY( saved_pos_y );

	ImGui::TextUnformatted( "Flip" );
	ImGui::SameLine();
	ImGui::SetCursorPosY( saved_pos_y );

	if ( ImGui::Button( "H" ) )
		image_draw::flip_h = !image_draw::flip_h;

	ImGui::SameLine();
	ImGui::SetCursorPosY( saved_pos_y );

	if ( ImGui::Button( "V" ) )
		image_draw::flip_v = !image_draw::flip_v;

	ImGui::Separator();

	ImGui::TextUnformatted( "Rotate" );

	ImGui::SameLine();

	ImGui::PushItemWidth( 150.f );
	ImGui::SliderFloat( "##rotation", &image_draw::rot, 0, 360 );
	ImGui::PopItemWidth();

	ImGui::Separator();

	if ( ImGui::BeginMenu( "Tools" ) )
	{
		// TODO: side menu to show information on the image or video overlayed next to the image in a window
		ImGui::MenuItem( "Media Info", nullptr, &g_draw_media_info, true );

		ImGui::Separator();

		ImGui::MenuItem( "Invert Colors", nullptr, &image_draw::invert_color, true );
		ImGui::MenuItem( "Draw Scaled Image", nullptr, &image_draw::scaling, true );
		ImGui::MenuItem( "Draw Raw Frames", nullptr, &image_draw::show_raw_frames, true );
		ImGui::MenuItem( "Hide Alpha Channel", nullptr, &image_draw::hide_alpha, true );

		ImGui::Separator();
		ImGui::Text( "Channels" );

		constexpr const char* channel_str[ 4 ] = { "Red", "Green", "Blue", "Alpha" };
		
		for ( u8 i = 0; i < 4; i++ )
		{
			ImVec2 text_size = ImGui::CalcTextSize( channel_str[ i ] );
			text_size.x += style.FramePadding.x;

			ImGui::Selectable( channel_str[ i ], &image_draw::show_channels[ i ], 0, text_size );

			if ( i < 3 )
			{
				ImGui::SameLine( 0, style.ItemInnerSpacing.x );
			}
		}

		ImGui::MenuItem( "Disable Transparency", nullptr, &image_draw::hide_alpha, true );

		if ( app::config.dev_mode )
		{
			ImGui::Separator();
			ImGui::MenuItem( "Memory Stats", nullptr, &g_draw_mem_stats, true );
			ImGui::MenuItem( "Demo Window", nullptr, &g_draw_imgui_demo, true );
		}

		ImGui::EndMenu();
	}

	ImGui::Separator();

	if ( ImGui::MenuItem( "Open File Location", nullptr, false, g_image_data.index != SIZE_MAX ) )
	{
		sys_browse_to_path( gallery_item_get_path( g_image_data.index ) );
	}

	if ( ImGui::BeginMenu( "Open With" ) )
	{
		if ( ImGui::MenuItem( "Open in New Window" ) )
		{
			util_open_in_new_window( gallery_item_get_path( g_image_data.index ).c_str() );
		}

		// TODO: list programs to open the file with, like fragment image viewer
		// how would this work on linux actually? hmm
		ImGui::MenuItem( "nothing lol", nullptr, false, false );
		ImGui::EndMenu();
	}

	if ( ImGui::MenuItem( "Copy File", nullptr, false ) )
	{
		fs::path path = gallery_item_get_path( g_image_data.index );

		if ( sys_copy_to_clipboard( { path } ) )
		{
			printf( "Copied to Clipboard\n" );
			push_notification( "Copied" );
		}
		else
		{
			printf( "Failed to Copy to Clipboard\n" );
			push_notification( "COPY FAILED" );
		}

		//sys_copy_to_clipboard( gallery_item_get_path_string( g_image_data.index ).data() );
		//push_notification( "Copied" );
	}

	if ( ImGui::BeginMenu( "Copy As" ) )
	{
		// what did this do again ????
		if ( ImGui::MenuItem( "Copy File Data", nullptr, false, false ) )
		{
		}

		// Enable once implemented
		// ImGui::BeginDisabled( gallery_item_get_media_entry( g_image_data.index ).type != e_media_type_image );
		ImGui::BeginDisabled( true );

		if ( ImGui::MenuItem( "JPEG", nullptr, false, 1 ) )
		{
		}

		if ( ImGui::MenuItem( "PNG", nullptr, false, 1 ) )
		{
		}

		ImGui::EndDisabled();

		ImGui::EndMenu();
	}

	if ( ImGui::MenuItem( "Set As Desktop Background", nullptr, false, false ) )
	{
	}

	if ( ImGui::MenuItem( "File Properties", nullptr, false ) )
	{
		// TODO: create our own imgui file properties for more info
		// Plat_OpenFileProperties( ImageView_GetImagePath() );

		sys_open_file_properties( { gallery_item_get_path( g_image_data.index ) } );
	}

	ImGui::Separator();

	if ( ImGui::MenuItem( "Undo", nullptr, false, 0 ) )
	{
		//UndoSys_Undo();
	}

	if ( ImGui::MenuItem( "Redo", nullptr, false, 0 ) )
	{
		//UndoSys_Redo();
	}

	if ( ImGui::MenuItem( "Delete" ) )
	{
		if ( delete_file_window( 1 ) )
		{
			// TODO: undo history
			fs::path path = gallery_item_get_path( g_image_data.index );
			sys_recycle_file( path.c_str() );
		}
	}

	ImGui::Separator();

	if ( ImGui::BeginMenu( "Sort By" ) )
	{

#define SORT_MENU_ITEM( type )                                                                     \
	if ( ImGui::MenuItem( g_gallery_sort_mode_str[ type ], nullptr, gallery::sort_mode == type ) ) \
	{                                                                                              \
		gallery::sort_mode        = type;                                                          \
		gallery::sort_mode_update = true;                                                          \
	}

		SORT_MENU_ITEM( e_gallery_sort_mode_name_a_z )
		SORT_MENU_ITEM( e_gallery_sort_mode_name_z_a )
		SORT_MENU_ITEM( e_gallery_sort_mode_date_mod_new_to_old )
		SORT_MENU_ITEM( e_gallery_sort_mode_date_mod_old_to_new )
		SORT_MENU_ITEM( e_gallery_sort_mode_date_created_new_to_old )
		SORT_MENU_ITEM( e_gallery_sort_mode_date_created_old_to_new )
		SORT_MENU_ITEM( e_gallery_sort_mode_size_large_to_small )
		SORT_MENU_ITEM( e_gallery_sort_mode_size_small_to_large )

#undef SORT_MENU_ITEM

		ImGui::EndMenu();
	}

	if ( ImGui::MenuItem( "Reload Folder", nullptr, false ) )
	{
		folder_load_media_list();
	}

	// 	if ( ImGui::MenuItem( "Show ImGui Demo", nullptr, gShowImGuiDemo ) )
	// 	{
	// 		gShowImGuiDemo = !gShowImGuiDemo;
	// 	}

	ImGui::EndPopup();
}


void media_view_input()
{
	media_scale_check_upload();

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

		//if ( ImGui::IsMouseDoubleClicked( ImGuiMouseButton_Left ) )
		//{
		//	set_view_type_gallery();
		//}
	}

	// TODO: Test ImGui::Shortcut()
	if ( app::window_focused && ImGui::IsKeyDown( ImGuiKey_LeftCtrl ) && ImGui::IsKeyPressed( ImGuiKey_C, false ) )
	{
		fs::path path = gallery_item_get_path( g_image_data.index );

		if ( sys_copy_to_clipboard( { directory::path / path } ) )
		{
			printf( "Copied to Clipboard\n" );
			push_notification( "Copied" );
		}
		else
		{
			printf( "Failed to Copy to Clipboard\n" );
			push_notification( "COPY FAILED" );
		}
	}

	media_view_context_menu();

	// if ( !util_mouse_hovering_imgui_window() )
	// 	return;

	auto& io = ImGui::GetIO();

	// check if the mouse isn't hovering over any window and we didn't grab it already
	if ( io.WantCaptureMouseUnlessPopupClose && !g_image_pan )
		return;

	if ( !ImGui::GetIO().WantTextInput && ( ImGui::IsKeyPressed( ImGuiKey_Enter, false ) || ImGui::IsKeyPressed( ImGuiKey_Escape, false ) ) )
	{
		if ( !g_gallery_view )
			set_view_type_gallery();
	}

	if ( ImGui::IsKeyPressed( ImGuiKey_Delete ) )
	{
		if ( delete_file_window( 1 ) )
		{
			// TODO: undo history
			fs::path path = gallery_item_get_path( g_image_data.index );
			sys_recycle_file( path.c_str() );
		}
	}

	bool mouse_hover_imgui_window = util_mouse_hovering_imgui_window();

	if ( ImGui::IsKeyPressed( ImGuiKey_Space, false ) )
	{
		image_draw::pause = !image_draw::pause;
	}

	// Don't toggle playback if in an image pan
	if ( ( !g_image_pan && !mouse_hover_imgui_window && ImGui::IsKeyReleased( ImGuiKey_MouseLeft, false ) ) )
	{
		image_draw::pause = !image_draw::pause;
	}

	int                  window_width, window_height;
	float                mouse_x, mouse_y;
	SDL_MouseButtonFlags mouse_btns = SDL_GetMouseState( &mouse_x, &mouse_y );
	SDL_GetWindowSize( app::window, &window_width, &window_height );

	bool mouse_in_client_area = false;

	// TODO: this is more of a slight patch, need to figure out the root cause of this
	// after a cancelled drag drop into itself, the titlebar doesn't move, and acts as the client area
	if ( mouse_x >= 0 && mouse_y >= 0 )
	{
		if ( mouse_x <= window_width && mouse_y <= window_height )
		{
			mouse_in_client_area = true;
		}
	}

	// mouse down and not hovering an imgui window not in an image pan
	// bool        mouse_middle_down = ImGui::IsMouseDown( ImGuiMouseButton_Middle ) && !( mouse_hover_imgui_window );
	bool                 mouse_middle_down = mouse_btns & SDL_BUTTON_LMASK || mouse_btns & SDL_BUTTON_RMASK;

	static bool drag_cooldown     = false;

	// if ( mouse_middle_down && !mouse_hover_imgui_window && mouse_in_client_area )
	if ( mouse_middle_down && !mouse_hover_imgui_window )
	{
		if ( !drag_cooldown )
		{
			if ( app::mouse_delta[ 0 ] != 0.0 || app::mouse_delta[ 1 ] != 0.0 )
			{
				u32 button = 0;
				if ( mouse_btns & SDL_BUTTON_LMASK )
					button = SDL_BUTTON_LEFT;

				else if ( mouse_btns & SDL_BUTTON_RMASK )
					button = SDL_BUTTON_RIGHT;

				// if it's left click, make sure we can't pan the image around
				bool skip = button == SDL_BUTTON_LEFT && media_view_can_pan_image();

				if ( !skip )
				{
					std::vector< fs::path > files{ gallery_item_get_path( g_image_data.index ) };
					sys_do_drag_drop_files( files, button );
				}

				// this way we don't try to start another drag drop instantly after somehow
				drag_cooldown = true;
			}
		}
	}
	else
	{
		drag_cooldown = false;
	}

	// mouse down and not hovering an imgui window not in an image pan
	bool mouse_down = ImGui::IsMouseDown( ImGuiMouseButton_Left ) && !( mouse_hover_imgui_window && !g_image_pan );

	if ( mouse_down && !g_image_pan )
	{
		// Wait for mouse movement to determine if we are panning the image or not
		if ( app::mouse_delta[ 0 ] != 0.0 || app::mouse_delta[ 1 ] != 0.0 )
		{
			g_image_pan = media_view_can_pan_image();
		}
	}

	if ( g_image_pan )
	{
		//set_frame_draw();
		image_draw::pos.x += app::mouse_delta[ 0 ];
		image_draw::pos.y += app::mouse_delta[ 1 ];

		media_view_clamp_to_bounds();
	}

	if ( !mouse_down )
		g_image_pan = false;
}


void media_view_window_resize()
{
	if ( image_draw::zoom_mode == e_zoom_mode_fixed )
	{
		media_view_clamp_to_bounds();
		return;
	}

	media_view_fit_in_view();
}


void media_view_advance( bool prev )
{
	if ( gallery::sorted_media.size() <= 1 )
		return;

	u32 next_image = g_image_data.index;

advance:
	if ( prev )
	{
		if ( next_image == 0 )
			next_image = gallery::sorted_media.size();

		next_image--;
	}
	else
	{
		next_image++;

		if ( next_image == gallery::sorted_media.size() )
			next_image = 0;
	}

	if ( gallery_item_get_media_entry( next_image ).type == e_media_type_directory )
		goto advance;

	media_view_load( next_image );
}


//constexpr float MEDIA_CONTROLS_PADDING = 20.f;


// this should be elsewhere
void media_view_handle_inputs( bool mouse_hover_imgui_window, bool video )
{
	if ( ImGui::IsKeyReleased( ImGuiKey_Space, false ) || ( !mouse_hover_imgui_window && ImGui::IsKeyReleased( ImGuiKey_MouseLeft, false ) ) )
	{
		if ( video )
		{
			const char* cmd[]   = { "cycle", "pause", NULL };
			int         cmd_ret = p_mpv_command_async( g_mpv, 0, cmd );

			set_frame_draw();
		}
	}

	// Seeking
	if ( !mouse_hover_imgui_window )
	{
		if ( ImGui::IsKeyDown( ImGuiKey_RightCtrl ) && ImGui::IsKeyPressed( ImGuiKey_LeftArrow, true ) )
		{
			if ( video )
			{
				const char* cmd[]   = { "seek", "-5", NULL };
				int         cmd_ret = p_mpv_command_async( g_mpv, 0, cmd );
			}
			else
			{
				media_view_frame_advance();
			}
		}
		if ( ImGui::IsKeyDown( ImGuiKey_RightCtrl ) && ImGui::IsKeyPressed( ImGuiKey_RightArrow, true ) )
		{
			if ( video )
			{
				const char* cmd[]   = { "seek", "5", NULL };
				int         cmd_ret = p_mpv_command_async( g_mpv, 0, cmd );
			}
			else
			{
				media_view_frame_advance( true );
			}
		}
	}
}


bool media_view_has_video()
{
	if ( get_media_type() != e_media_type_video )
		return false;

	return g_mpv_data.loaded_file;
}


void media_view_set_playback_pause( bool pause )
{
	image_draw::pause = pause;

	if ( media_view_has_video() )
	{
		const char* cmd[]   = { "set", "pause", pause ? "yes" : "no", NULL };
		int         cmd_ret = p_mpv_command_async( g_mpv, 0, cmd );
	}
}


void media_view_draw_video_controls( bool mouse_hover_imgui_window, float right_side_area, ImVec2& play_btn_size, int window_width, int window_height )
{
	ImGuiStyle& style = ImGui::GetStyle();

	//ImGui::Spacing();
	//ImGui::SameLine();

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

	util_format_time( str_time_pos, g_mpv_data.time_pos );
	util_format_time( str_duration, g_mpv_data.duration );

	char str_time[ TIME_BUFFER + TIME_BUFFER + 4 ]{};

	snprintf( str_time, TIME_BUFFER + TIME_BUFFER + 4, "%s / %s", str_time_pos, str_duration );

	const ImVec2 time_size       = ImGui::CalcTextSize( str_time, NULL, true );
	const ImVec2 other_text_size = ImGui::CalcTextSize( "Mute", NULL, true );

	float        avaliable_width = ImGui::GetContentRegionAvail()[ 0 ] - ( style.ItemSpacing.x * 0 + right_side_area );
	// float        avaliable_width = 500.f - ( style.ItemSpacing.x * 2 );
	float        vol_bar_width   = 96.f;
	ImVec2       other_text_area = ImGui::CalcItemSize( { 0, 0 }, other_text_size.x + style.FramePadding.x * 2.0f, other_text_size.y + style.FramePadding.y * 2.0f );

	float        seek_bar_width  = avaliable_width - vol_bar_width;
	// seek_bar_width -= ( play_btn_size.x + vol_bar_width + time_size.x + ( other_text_area.x * 3 ) + ( style.ItemSpacing.x * 6 ) );
	seek_bar_width -= ( time_size.x + ( other_text_area.x * 1 ) + ( style.ItemSpacing.x * 4 ) );
	// seek_bar_width -= ( play_btn_size.x + vol_bar_width + time_size.x + ( style.ItemSpacing.x * 2 ) );

	ImGui::SetNextItemWidth( seek_bar_width );

	float time_pos_f = (float)g_mpv_data.time_pos;
	if ( ImGui::SliderFloat( "##seek", &time_pos_f, 0.f, (float)g_mpv_data.duration, "" ) )
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

	//	int muted = 0;
	//	p_mpv_get_property( g_mpv, "mute", MPV_FORMAT_FLAG, &muted );

	if ( g_mpv_data.mute )
	{
		ImGui::PushStyleColor( ImGuiCol_Button, ImGui::GetStyleColorVec4( ImGuiCol_ButtonActive ) );
	}

	if ( ImGui::Button( "Mute" ) )
	{
		const char* cmd[]   = { "cycle", "mute", NULL };
		int         cmd_ret = p_mpv_command_async( g_mpv, 0, cmd );
	}

	if ( g_mpv_data.mute )
	{
		ImGui::PopStyleColor();
	}

	//	double volume = 0;
	//	p_mpv_get_property( g_mpv, "volume", MPV_FORMAT_DOUBLE, &volume );

	ImGui::SameLine();
	ImGui::SetNextItemWidth( vol_bar_width );

	int volume_value = static_cast< int >( round( g_mpv_data.volume ) );
	if ( ImGui::SliderInt( "##Volume", &volume_value, 0, 130 ) )
	{
		char volume_str[ 16 ];
		snprintf( volume_str, 16, "%d", volume_value );

		const char* cmd[]   = { "set", "volume", volume_str, NULL };
		int         cmd_ret = p_mpv_command_async( g_mpv, 0, cmd );

		image_draw::volume  = volume_value;
	}

	// ImGui::Text( "%s / %s", str_time_pos, str_duration );
	// ImGui::ProgressBar( time_pos / duration );
}


void media_view_draw_animated_image_controls( float right_side_area )
{
	ImGuiStyle& style = ImGui::GetStyle();

	//ImGui::Spacing();
	//ImGui::SameLine();

	if ( ImGui::Button( "<|" ) )
	{
		media_view_frame_set( 0 );
	}

	//ImGui::SameLine();
	//if ( ImGui::Button( "|>" ) )
	//{
	//	media_view_frame_set( g_image_data.image.frame.size() - 1 );
	//}

	ImGui::SameLine();
	//ImGui::Spacing();

	ImGui::SameLine();
	if ( ImGui::Button( "<" ) )
	{
		image_draw::pause = true;
		media_view_frame_advance( true );
	}

	ImGui::SameLine();
	if ( ImGui::Button( ">" ) )
	{
		image_draw::pause = true;
		media_view_frame_advance();
	}

	ImGui::SameLine();

	// https://stackoverflow.com/questions/3673226/how-to-print-time-in-format-2009-08-10-181754-811

	char str_position[ 256 ]{};

	snprintf( str_position, 256, "%zu / %zu", image_draw::frame + 1, g_image_data.image.frame.size() );

	const ImVec2 time_size       = ImGui::CalcTextSize( str_position, NULL, true );

	float        avaliable_width = ImGui::GetContentRegionAvail()[ 0 ] - ( style.ItemSpacing.x * 1 + right_side_area );
	// float        avaliable_width = 500.f - ( style.ItemSpacing.x * 2 );

	float        seek_bar_width  = avaliable_width;
	seek_bar_width -= ( time_size.x + ( style.ItemSpacing.x * 1 ) );
	// seek_bar_width -= ( play_btn_size.x + vol_bar_width + time_size.x + ( style.ItemSpacing.x * 2 ) );

	ImGui::SetNextItemWidth( seek_bar_width );

	int desired_frame = image_draw::frame + 1;
	if ( ImGui::SliderInt( "##seek", &desired_frame, 1, g_image_data.image.frame.size(), "" ) )
	{
		media_view_frame_set( desired_frame - 1 );
	}

	ImGui::SameLine();
	ImGui::TextUnformatted( str_position );
}


void media_view_draw_media_controls( bool mouse_hover_imgui_window )
{
	int width, height;
	SDL_GetWindowSize( app::window, &width, &height );

	bool video      = get_media_type() == e_media_type_video;
	bool anim_image = ( get_media_type() == e_media_type_image && g_image_data.image.frame.size() > 1 );

	media_view_handle_inputs( mouse_hover_imgui_window, video );

	ImGui::PushFont( font::normal, app::config.font_size * 1.25f );

	// check if mouse in rectangle
	ImGuiStyle& style                = ImGui::GetStyle();

	//ImVec2      window_height        = ImGui::CalcItemSize( { 0, 0 }, ImGui::GetFontSize() + style.FramePadding.x * 2.0f, ImGui::GetFontSize() + style.FramePadding.y * 2.0f );
	ImVec2      content_height( ImGui::GetFontSize() + style.FramePadding.x * 2.0f, ImGui::GetFrameHeight() );
	ImVec2       window_height( ImGui::GetFontSize() + style.FramePadding.x * 2.0f + style.ItemSpacing.x, ImGui::GetFrameHeightWithSpacing() );

	const ImVec2 POS_OFFSET( style.WindowPadding.x * 3.f, style.WindowPadding.y * 3.f );

	ImVec2       playback_control_pos{};
	playback_control_pos.y            = ( height - POS_OFFSET.y );

	// check if mouse in rectangle
	static bool was_drawing_controls  = false;
	bool        mouse_in_area         = mouse_in_rect( { 0.f, height - ( POS_OFFSET.y + window_height.y * 3.f ) }, { (float)width, (float)height } );

	if ( mouse_in_area != was_drawing_controls )
		set_frame_draw();

	was_drawing_controls         = mouse_in_area;
	float nav_button_area        = content_height.x * 2 + style.ItemSpacing.x;

	// ----------------------------------------

	// float max_controls_width = width - ( ( ( POS_OFFSET.x * 1.f ) + window_height.x + style.ItemSpacing.x ) * 2.f + ( style.WindowPadding.x + style.ItemSpacing.x ) );
	float max_controls_width = width - ( POS_OFFSET.x * 2.f );

	bool  draw_playback_controls = mouse_in_area && ( video || anim_image );
	
	if ( draw_playback_controls )
	{
		nav_button_area  += style.ItemSpacing.x + 1.f;  // 1.f for separator
		playback_control_pos.x = POS_OFFSET.x;  // width / 2;
		//playback_control_pos.y = ( height - 75.f );
	//ImGui::SetNextWindowSizeConstraints( { width - 80.f, -1.f }, { width - 80.f, -1.f } );
		ImGui::SetNextWindowSizeConstraints( { max_controls_width, -1 }, { max_controls_width, -1 } );
	}
	else
	{
		playback_control_pos.x = width - POS_OFFSET.x;
		//playback_control_pos.y = ( height - 75.f );
		max_controls_width = nav_button_area;
	}

	// pivot aligns it to the center and the bottom of the window
	//ImGui::SetNextWindowPos( playback_control_pos, 0, ImVec2( 0.5f, 1.0f ) );
	if ( draw_playback_controls )
		ImGui::SetNextWindowPos( playback_control_pos, 0, ImVec2( 0.0f, 1.0f ) );
	else
		ImGui::SetNextWindowPos( playback_control_pos, 0, ImVec2( 1.0f, 1.0f ) );

	// ImGuiWindowFlags_NoBackground

	ImGuiWindowFlags win_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing;

	if ( !draw_playback_controls )
		win_flags |= ImGuiWindowFlags_NoBackground;

	if ( mouse_in_area )
	{
		ImGui::SetNextWindowBgAlpha( 0.5f );
	}
	else
	{
		ImGui::SetNextWindowBgAlpha( 0.0f );

		ImVec4 btn_color  = ImGui::GetStyleColorVec4( ImGuiCol_Button );
		ImVec4 text_color = ImGui::GetStyleColorVec4( ImGuiCol_Text );
		btn_color.w       = 0.25f;
		text_color.w      = 0.25f;

		ImGui::PushStyleColor( ImGuiCol_Button, btn_color );
		ImGui::PushStyleColor( ImGuiCol_Text, text_color );
	}

	if ( ImGui::Begin( "##media_controls", 0, win_flags ) )
	{
		const ImVec2 label_size    = ImGui::CalcTextSize( "Pause", NULL, true );
		ImVec2       play_btn_size = ImGui::CalcItemSize( { 0, 0 }, label_size.x + style.FramePadding.x * 2.0f, label_size.y + style.FramePadding.y * 2.0f );

		if ( draw_playback_controls )
		{
			bool is_paused = image_draw::pause;

			if ( video )
				is_paused = g_mpv_data.pause;

			if ( is_paused )
			{
				if ( ImGui::Button( "Play", play_btn_size ) )
				{
					media_view_set_playback_pause( false );
				}
			}
			else
			{
				if ( ImGui::Button( "Pause", play_btn_size ) )
				{
					media_view_set_playback_pause( true );
				}
			}
		}

		if ( draw_playback_controls )
		{
			ImGui::SameLine();

			if ( video )
				media_view_draw_video_controls( mouse_hover_imgui_window, nav_button_area, play_btn_size, width, height );
			else
				media_view_draw_animated_image_controls( nav_button_area );

			ImGui::SameLine();
			ImGui::SeparatorEx( ImGuiSeparatorFlags_Vertical );
			ImGui::SameLine();
		}

		if ( ImGui::ArrowButtonEx( "##view_nav_btn_prev", ImGuiDir_Left, content_height ) )
		{
			media_view_advance( true );
		}

		ImGui::SameLine();

		if ( ImGui::ArrowButtonEx( "##view_nav_btn_next", ImGuiDir_Right, content_height ) )
		{
			media_view_advance();
		}
	}

	ImGui::End();
	ImGui::PopFont();

	if ( !mouse_in_area )
	{
		ImGui::PopStyleColor( 2 );
	}
}


void media_view_draw_close_button()
{
	int width, height;
	SDL_GetWindowSize( app::window, &width, &height );

	ImGuiStyle& style = ImGui::GetStyle();

	ImGui::PushFont( font::normal, app::config.font_size * 1.25f );

	// ImVec2 button_pos{
	// 	width - ( ImGui::GetFrameHeight() + style.WindowPadding.x * 4 ),
	// 	( style.WindowPadding.x * 4 )
	// };

	ImVec2 button_pos{
		width - ( style.WindowPadding.x * 2 ),
		( style.WindowPadding.y * 2 )
	};

	// check if mouse in rectangle

	static bool was_drawing_controls = false;
	ImVec2      play_btn_size        = ImGui::CalcItemSize( { 0, 0 }, ImGui::GetFontSize() + style.FramePadding.x * 2.0f, ImGui::GetFontSize() + style.FramePadding.y * 2.0f );
	bool        area_focused         = true;

	//if ( !mouse_in_rect( { button_pos.x - ( ( style.WindowPadding.x * 4 ) + play_btn_size.x ), 0.f }, { (float)width, ( ( style.WindowPadding.y * 6 ) + play_btn_size.y ) } ) )
	if ( !mouse_in_rect( { button_pos.x - ( ( style.WindowPadding.x * 10 ) + play_btn_size.x ), 0.f }, { (float)width, ( ( style.WindowPadding.y * 12 ) + play_btn_size.y ) } ) )
	{
		area_focused = false;

		//if ( was_drawing_controls )
		//	set_frame_draw();
	
		//was_drawing_controls = false;
		//ImGui::PopFont();
		//return;
	}
	
	//if ( !was_drawing_controls )
	if ( was_drawing_controls != area_focused )
		set_frame_draw();
	
	was_drawing_controls = area_focused;

	// ----------------------------------------

	// ImGui::SetNextWindowPos( button_pos, 0, ImVec2( 0.0f, 1.0f ) );
	ImGui::SetNextWindowPos( button_pos, 0, ImVec2( 1.0f, 0.0f ) );
	// ImGui::SetNextWindowPos( button_pos );

	//ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, { 0, 0 } );
	//ImGui::PushStyleVar( ImGuiStyleVar_WindowBorderSize, 0.f );
	 
	if ( !area_focused )
	{
		ImVec4 btn_color  = ImGui::GetStyleColorVec4( ImGuiCol_Button );
		ImVec4 text_color = ImGui::GetStyleColorVec4( ImGuiCol_Text );
		btn_color.w       = 0.25f;
		text_color.w      = 0.25f;

		ImGui::PushStyleColor( ImGuiCol_Button, btn_color );
		ImGui::PushStyleColor( ImGuiCol_Text, text_color );
	}
		//ImGui::SetNextWindowBgAlpha( 0.25f );

	if ( ImGui::Begin( "##view_close_btn", 0, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBackground ) )
	{
		if ( ImGui::Button( "X", play_btn_size ) )
		{
			set_view_type_gallery();
		}
	}

	if ( !area_focused )
	{
		ImGui::PopStyleColor( 2 );
	}

	//ImGui::PopStyleVar();

	ImGui::End();
	ImGui::PopFont();
}


void media_view_draw_imgui_controls( bool mouse_hover_imgui_window )
{
	media_view_draw_close_button();
	media_view_draw_media_controls( mouse_hover_imgui_window );
}


void media_view_draw_imgui()
{
	media_view_input();

	bool mouse_hover_imgui_window = util_mouse_hovering_imgui_window();

	if ( g_draw_media_info )
		media_view_draw_media_info();

	if ( g_draw_imgui_demo )
		ImGui::ShowDemoWindow();

	if ( g_draw_mem_stats )
	{
		if ( ImGui::Begin( "Memory Stats" ) )
			mem_draw_debug_ui();

		ImGui::End();
	}

	media_view_draw_imgui_controls( mouse_hover_imgui_window );

	if ( get_media_type() == e_media_type_image )
	{
		if ( g_draw_zoom_level )
		{
			ImGui::Begin( "##zoom_level", 0, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration );
			ImGui::Text( "%.1f%%", (float)( image_draw::zoom * 100 ) );
			ImGui::End();
		}
	}
}

