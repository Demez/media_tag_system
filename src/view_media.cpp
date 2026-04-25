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
	float       rot              = 0.f;

	// Animated image playback information
	double      next_frame_timer = 0.0;
	size_t      frame            = 0;
	float       playback_speed   = 1.f;
	bool        pause            = false;
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

constexpr double         ZOOM_MIN    = 0.01;

// Image Scaling

enum e_scale_state
{
	e_scale_state_idle,
	e_scale_state_start,     // main thread sets state to this when it wants to scale the current image
	e_scale_state_working,   // main thread looks at this state when running, uses full image while waiting
	e_scale_state_upload,    // main thread needs to upload to gpu, after this, it's set back to idle
	e_scale_state_finished,  // main thread can use scaled image

	e_scale_state_count
};

const char* g_scale_state_str[] = 
{
	"scale_state_idle",
	"scale_state_start",
	"scale_state_working",
	"scale_state_upload",
	"scale_state_finished",
};

static_assert( ARR_SIZE( g_scale_state_str ) == e_scale_state_count );


constexpr float      SCALE_WAIT_TIME = 0.1f;
constexpr float      UPSCALE_LIMIT = 2.f;

static std::thread*  g_scale_thread;
static e_scale_state g_scale_state  = e_scale_state_idle;
static float         g_scale_timer  = -1.f;
static image_t       g_scale_src{};

std::mutex           g_scale_lock;


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

		g_scale_lock.lock();

		image_copy_data( g_scale_src, g_image_scaled_data.image );

		g_image_scaled_data.image.frame.clear();
		g_image_scaled_data.image.frame.resize( g_scale_src.frame.size() );

		image_copy_frame_data( g_scale_src.frame[ 0 ], g_image_scaled_data.image.frame[ 0 ] );

		// Downscale image if size is larger than target size
		if ( image_draw::size.x < ( g_scale_src.width * UPSCALE_LIMIT ) && image_draw::size.x != g_image_data.image.width )
		{
			if ( image_scale( &g_scale_src, &g_image_scaled_data.image, image_draw::size.x, image_draw::size.y ) )
				g_scale_state = e_scale_state_upload;

			// if ( g_scale_src.width != g_image_data.image.width )
			// 	printf( "scale source is different from currently displayed image!\n" );
		}
		else
		{
			g_scale_state = e_scale_state_idle;
		}

		g_scale_lock.unlock();
	}
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

	image_draw::next_frame_timer = g_image_data.image.frame[ image_draw::frame ].time;
}


void media_view_frame_set( size_t frame )
{
	if ( frame >= g_image_data.image.frame.size() )
		return;

	image_draw::frame            = frame;
	image_draw::next_frame_timer = g_image_data.image.frame[ image_draw::frame ].time;
}


void media_view_scale_check_timer( float frame_time )
{
	// Don't handle animated images for now
	if ( g_image_data.image.frame.size() > 1 )
		return;

	if ( g_scale_state == e_scale_state_finished )
	{
		// Are we drawing the image smaller than native size?
		if ( image_draw::size.x < ( g_image_data.image.width * UPSCALE_LIMIT ) || round( image_draw::size.x ) != g_image_data.image.width )
		{
			// Does the scaled image size match the size we draw it as?
			if ( int( image_draw::size.x ) != g_image_scaled_data.image.width )
			{
				g_scale_state = e_scale_state_idle;
				g_scale_timer = SCALE_WAIT_TIME;
			}
		}
	}

	if ( g_scale_state != e_scale_state_idle && g_scale_state != e_scale_state_finished )
		return;

	if ( g_scale_timer < 0.f )
		return;

	g_scale_timer -= frame_time;

	if ( g_scale_timer < 0.f && g_image_data.image.frame.size() && g_scale_state == e_scale_state_idle )
	{
		if ( image_draw::size.x >= ( g_image_data.image.width * UPSCALE_LIMIT ) && image_draw::size.x != g_image_data.image.width )
			return;

		// ????
		if ( !g_image_data.image.frame[ 0 ].data )
			return;

		g_scale_lock.lock();

		image_free( g_scale_src );

		image_copy_data( g_image_data.image, g_scale_src );

		g_scale_src.frame.clear();
		g_scale_src.frame.resize( 1 );

		image_copy_frame_data( g_image_data.image.frame[ 0 ], g_scale_src.frame[ 0 ] );

		// don't hold onto this
		g_scale_src.image_format = nullptr;

		size_t image_size           = (size_t)g_image_data.image.width * (size_t)g_image_data.image.height * (size_t)g_image_data.image.bytes_per_pixel;
		g_scale_src.frame[ 0 ].data = ch_calloc< u8 >( image_size, e_mem_category_image_data );
		memcpy( g_scale_src.frame[ 0 ].data, g_image_data.image.frame[ 0 ].data, image_size * sizeof( u8 ) );

		g_scale_src.frame[ 0 ].size = image_size;

		g_image_scaled_data.index = gallery::cursor;
		g_scale_state             = e_scale_state_start;

		g_scale_lock.unlock();
	}
}


void media_view_update( float frame_time )
{
	media_view_scale_check_timer( frame_time );

	// Add frame draw timer here for animated images
	if ( g_image_data.image.frame.size() > 1 )
	{
		if ( !image_draw::pause )
		{
			image_draw::next_frame_timer -= frame_time * image_draw::playback_speed;

			if ( image_draw::next_frame_timer < 0.f )
			{
				media_view_frame_advance();
			}
		}
	}
	else
	{
		image_draw::next_frame_timer = 0.0;
	}
}


void media_view_scale_reset_timer()
{
	g_scale_timer = SCALE_WAIT_TIME;

	if ( g_scale_state == e_scale_state_finished )
		g_scale_state = e_scale_state_idle;
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


e_media_type get_media_type()
{
	if ( gallery::sorted_media.size() <= gallery::cursor )
		return e_media_type_none;

	return gallery_item_get_media_entry( gallery::cursor ).type;
}


// New Position = Scale Origin + ( Scale Point - Scale Origin ) * Scale Factor
double scale_point_from_origin( double origin, double point, double factor )
{
	return origin + ( point - origin ) * factor;
}


void media_view_clamp_to_bounds()
{
	ImVec2 min_bounds{};
	ImVec2 max_bounds{};

	int    width, height;
	SDL_GetWindowSize( app::window, &width, &height );

	// If image is larger than width or height, we need to change the bounds a bit, to allow most of image itself to go past it
	// so the side of the window can be half filled with part of the image
	if ( width < image_draw::size.x )
	{
		float out_of_bounds = width / 2.f;
		min_bounds.x        = -image_draw::size.x + out_of_bounds;
		max_bounds.x        = out_of_bounds;

		image_draw::pos.x   = CLAMP( image_draw::pos.x, min_bounds.x, max_bounds.x );
	}
	else
	{
		// centers the image, this works ok, but feels a bit off when zooming out
		// image_draw::pos.x = width / 2 - ( image_draw::size.x / 2 );

		// this is similar to above, but only allows half the image to go out of bounds instead of most
		min_bounds.x        = -image_draw::size.x / 2.f;
		max_bounds.x        = width - ( image_draw::size.x / 2.f );

		// min_bounds.x        = 0;
		// max_bounds.x        = width - image_draw::size.x ;

		image_draw::pos.x   = CLAMP( image_draw::pos.x, min_bounds.x, max_bounds.x );
	}

	if ( height < image_draw::size.y )
	{
		float out_of_bounds = height / 2.f;
		min_bounds.y        = -image_draw::size.y + out_of_bounds;
		max_bounds.y        = out_of_bounds;

		image_draw::pos.y   = CLAMP( image_draw::pos.y, min_bounds.y, max_bounds.y );
	}
	else
	{
		// image_draw::pos.y = height / 2 - ( image_draw::size.y / 2 );

		min_bounds.y      = -image_draw::size.y / 2.f;
		max_bounds.y      = height - ( image_draw::size.y / 2.f );

		// min_bounds.y      = 0;
		// max_bounds.y      = height - image_draw::size.y;

		image_draw::pos.y = CLAMP( image_draw::pos.y, min_bounds.y, max_bounds.y );
	}
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
		image_draw::zoom      = std::min( factor[ 0 ], factor[ 1 ] );
		image_draw::zoom_mode = e_zoom_mode_fit;

		image_draw::size.x    = g_image_data.image.width * image_draw::zoom;
		image_draw::size.y    = g_image_data.image.height * image_draw::zoom;

		media_view_scale_reset_timer();
	}

	// TODO: only adjust this if needed, check image zoom type
	// if image doesn't fit window size, keep locked to center

	if ( center_image )
	{
		image_draw::pos.x = width / 2 - ( image_draw::size.x / 2 );
		image_draw::pos.y = height / 2 - ( image_draw::size.y / 2 );
	}

	media_view_clamp_to_bounds();
}


void media_view_zoom_reset()
{
	int width, height;
	SDL_GetWindowSize( app::window, &width, &height );

	// keep where we are centered on in the image
	// New Position = Scale Origin + ( Scale Point - Scale Origin ) * Scale Factor
	// image_draw::pos.x     = ( width / 2.0 ) + ( image_draw::pos.x - ( width / 2.0 ) ) * ( 1.0 / image_draw::zoom );
	// image_draw::pos.y     = ( height / 2.0 ) + ( image_draw::pos.y - ( height / 2.0 ) ) * ( 1.0 / image_draw::zoom );

	image_draw::pos.x     = scale_point_from_origin( width / 2.0, image_draw::pos.x, 1.0 / image_draw::zoom );
	image_draw::pos.y     = scale_point_from_origin( height / 2.0, image_draw::pos.y, 1.0 / image_draw::zoom );

	image_draw::zoom      = 1.0;

	image_draw::zoom_mode = e_zoom_mode_fixed;

	if ( !g_image_data.image.frame.size() )
		return;

	image_draw::size.x = g_image_data.image.width;
	image_draw::size.y = g_image_data.image.height;

	media_view_scale_reset_timer();
	media_view_clamp_to_bounds();
}


void media_view_scroll_zoom( float scroll )
{
	if ( !g_image_data.textures.count || scroll == 0 )
		return;

	if ( util_mouse_hovering_imgui_window() )
		return;

	double factor = 1.0;

	// Zoom in if scrolling up
	if ( scroll > 0 )
	{
		// max zoom level
		if ( image_draw::zoom >= 1000.0 )
			return;

		factor += ( app::config.media_zoom_scale * scroll );
	}
	else
	{
		// min zoom level
		if ( image_draw::zoom <= ZOOM_MIN )
			return;

		factor -= ( app::config.media_zoom_scale * abs( scroll ) );
	}

	// TODO: add zoom levels to snap to here
	// 100, 200, 400, 500, 50, 25, etc

	// auto rounded_zoom = std::max( 0.01f, roundf( image_draw::zoom * factor * 100 ) / 100 );
	// 
	// if ( fmod( rounded_zoom, 1.0 ) == 0 )
	// 	factor = rounded_zoom / image_draw::zoom;

	double old_zoom = image_draw::zoom;

	image_draw::zoom    = (double)( std::max( 1.f, image_draw::size.x ) * factor ) / (double)g_image_data.image.width;

	// Snap to 100% zoom level
	if ( old_zoom < 1.0 && image_draw::zoom >= 1.0 || old_zoom > 1.0 && image_draw::zoom <= 1.0 )
		image_draw::zoom = 1.0;

	// round it so we don't get something like 0.9999564598 or whatever instead of 1.0
	//if ( image_draw::zoom < 0.01 )
	//{
	//	image_draw::zoom = std::max( ZOOM_MIN, round( image_draw::zoom * 10000 ) / 10000 );
	//}
	//else
	{
		image_draw::zoom = std::max( ZOOM_MIN, round( image_draw::zoom * 1000 ) / 1000 );
	}

	// get new factor
	factor             = image_draw::zoom / old_zoom;

	// recalculate draw width and height
	image_draw::size.x = (double)g_image_data.image.width * image_draw::zoom;
	image_draw::size.y = (double)g_image_data.image.height * image_draw::zoom;

	// recalculate image position to keep image where cursor is

	// New Position = Scale Origin + ( Scale Point - Scale Origin ) * Scale Factor
	// image_draw::pos.x  = (double)app::mouse_pos[ 0 ] + ( image_draw::pos.x - (double)app::mouse_pos[ 0 ] ) * factor;
	// image_draw::pos.y  = (double)app::mouse_pos[ 1 ] + ( image_draw::pos.y - (double)app::mouse_pos[ 1 ] ) * factor;

	image_draw::pos.x  = scale_point_from_origin( app::mouse_pos[ 0 ], image_draw::pos.x, factor );
	image_draw::pos.y  = scale_point_from_origin( app::mouse_pos[ 1 ], image_draw::pos.y, factor );

	media_view_scale_reset_timer();
	media_view_clamp_to_bounds();

	image_draw::zoom_mode = e_zoom_mode_fixed;
	set_frame_draw( 2 );
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

	media_entry_t entry  = gallery_item_get_media_entry( gallery::cursor );

	ImGui::TextUnformatted( entry.filename.c_str() );

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

		ImGui::Text( "Scale Thread State: %d - %s", g_scale_state, g_scale_state_str[ g_scale_state ] );
		ImGui::Text( "Scale Thread Timer: %.3f", g_scale_timer );
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
			case GL_RGBA16:
				ImGui::TextUnformatted( "GL Format: RGBA16" );
				break;
			case GL_LUMINANCE:
				ImGui::TextUnformatted( "GL Format: LUMINANCE" );
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
		media_view_fit_in_view();

	ImGui::SameLine();

	if ( ImGui::Button( "Center", { 0, 0 } ) )
		media_view_fit_in_view( false );

	ImGui::SameLine();

	if ( ImGui::Button( "100%", { 0, 0 } ) )
	{
		media_view_zoom_reset();
	}

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

	ImGui::PushItemWidth( 125.f );
	ImGui::SliderFloat( "Rotation", &image_draw::rot, 0, 360 );
	ImGui::PopItemWidth();

	ImGui::Separator();

	if ( ImGui::MenuItem( "Invert Colors", nullptr, false, false ) )
	{
	}

	ImGui::Separator();

	if ( ImGui::MenuItem( "Open File Location", nullptr, false, g_image_data.textures.count ) )
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

	if ( ImGui::BeginMenu( "Copy As" ) )
	{
		// what did this do again ????
		if ( ImGui::MenuItem( "Copy File Data", nullptr, false, false ) )
		{
		}

		// Enable once implemented
		// ImGui::BeginDisabled( gallery_item_get_media_entry( gallery::cursor ).type != e_media_type_image );
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
		g_scale_src.frame.clear();

		if ( g_image_scaled_data.index == gallery::cursor )
		{
			gl_update_textures( g_image_scaled_data.textures, &g_image_scaled_data.image, 1 );
			printf( "Scaled Main Image\n" );
			g_scale_state   = e_scale_state_finished;
			set_frame_draw();

		}
		else
		{
			printf( "SCALE MISMATCH\n" );
			g_scale_state   = e_scale_state_idle;
			set_frame_draw();
		}
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

	// if ( !util_mouse_hovering_imgui_window() )
	// 	return;

	auto& io = ImGui::GetIO();

	// check if the mouse isn't hovering over any window and we didn't grab it already
	if ( io.WantCaptureMouseUnlessPopupClose && !g_image_pan )
		return;

	if ( !ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed( ImGuiKey_Enter, false ) )
	{
		if ( !g_gallery_view )
			set_view_type_gallery();
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

	SDL_MouseButtonFlags mouse_btns = SDL_GetMouseState( nullptr, nullptr );

	// mouse down and not hovering an imgui window not in an image pan
	// bool        mouse_middle_down = ImGui::IsMouseDown( ImGuiMouseButton_Middle ) && !( mouse_hover_imgui_window );
	bool                 mouse_middle_down = mouse_btns & SDL_BUTTON_MMASK && !( mouse_hover_imgui_window );

	static bool drag_cooldown     = false;

	if ( mouse_middle_down )
	{
		if ( !drag_cooldown )
		{
			if ( app::mouse_delta[ 0 ] != 0.0 || app::mouse_delta[ 1 ] != 0.0 )
			{
				std::vector< fs::path > files{ gallery_item_get_path( gallery::cursor ) };
				sys_do_drag_drop_files( files );

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
			g_image_pan = true;
		}
	}

	if ( g_image_pan )
	{
		set_frame_draw();
		image_draw::pos.x += app::mouse_delta[ 0 ];
		image_draw::pos.y += app::mouse_delta[ 1 ];

		media_view_clamp_to_bounds();
	}

	if ( !mouse_down )
		g_image_pan = false;
}


void media_view_window_resize()
{
	if ( image_draw::zoom_mode == e_zoom_mode_fit || image_draw::zoom_mode == e_zoom_mode_fit_width )
	{
		media_view_fit_in_view();
	}
	else
	{
		media_view_clamp_to_bounds();
	}
}


void media_view_load()
{
	if ( gallery::sorted_media.empty() )
		return;

	if ( gallery::cursor >= gallery::sorted_media.size() )
		return;

	float             load_time    = 0.f;
	// gallery_item_t&   gallery_item = gallery::sorted_media[ gallery::cursor ];
	media_entry_t     entry        = gallery_item_get_media_entry( gallery::cursor );

	image_load_info_t image_load_info{};
	image_load_info.image = &g_image_data.image;

	{
		auto startTime = std::chrono::high_resolution_clock::now();

		if ( entry.type == e_media_type_image )
		{
			if ( image_load( entry.file.path, image_load_info ) )
			{
				media_history_add( entry.file.path.string() );
			}

			mpv_cmd_close_video();
		}
		else
		{
			mpv_cmd_loadfile( entry.file.path.string().c_str() );
		}

		auto currentTime = std::chrono::high_resolution_clock::now();

		load_time        = std::chrono::duration< float, std::chrono::seconds::period >( currentTime - startTime ).count();

		// auto startTime       = std::chrono::high_resolution_clock::now();

		if ( entry.type == e_media_type_image )
		{
			if ( image_load_info.image->frame.size() > 0 && image_load_info.image->bytes_per_pixel > 0 )
			{
				gl_update_textures( g_image_data.textures, &g_image_data.image, g_image_data.image.frame.size() );
				media_view_fit_in_view();

				image_draw::frame            = 0;
				image_draw::next_frame_timer = g_image_data.image.frame[ image_draw::frame ].time;
			}
			else
			{
				printf( "%f FAILED Load - %s\n", load_time, entry.file.path.string().c_str() );
			}
		}

		// auto  currentTime    = std::chrono::high_resolution_clock::now();
		// float up_time        = std::chrono::duration< float, std::chrono::seconds::period >( currentTime - startTime ).count();
		//printf( "%f Load - %f Up - %s\n", load_time, up_time, directory::media_list[ g_folder_index ].string().c_str() );
		printf( "%f Load - %s\n", load_time, entry.file.path.string().c_str() );
	}

	g_image_data.index = gallery::cursor;

	update_window_title();

	media_view_scale_reset_timer();

	set_frame_draw();
}


void media_view_advance( bool prev )
{
	if ( gallery::sorted_media.size() <= 1 )
		return;

	if ( get_media_type() == e_media_type_video )
		mpv_cmd_close_video();

advance:
	if ( prev )
	{
		if ( gallery::cursor == 0 )
			gallery::cursor = gallery::sorted_media.size();

		gallery::cursor--;
	}
	else
	{
		gallery::cursor++;

		if ( gallery::cursor == gallery::sorted_media.size() )
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

	//if ( !g_mpv_video_ready )
	//	return;

	bool mouse_hover_imgui_window = util_mouse_hovering_imgui_window();

	if ( ImGui::IsKeyPressed( ImGuiKey_Space, false ) || ( !mouse_hover_imgui_window && ImGui::IsKeyPressed( ImGuiKey_MouseLeft, false ) ) )
	{
		const char* cmd[]   = { "cycle", "pause", NULL };
		int         cmd_ret = p_mpv_command_async( g_mpv, 0, cmd );
	}

	// Seeking
	if ( !mouse_hover_imgui_window )
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

	static bool  was_drawing_controls = false;

	static float controls_height = 50.f;
	if ( !mouse_in_rect( { 0.f, height - ( 80.f + ( controls_height * 2 ) ) }, { (float)width, (float)height } ) )
	{
		if ( was_drawing_controls )
			set_frame_draw();

		was_drawing_controls = false;
		return;
	}

	if ( !was_drawing_controls )
		set_frame_draw();

	was_drawing_controls = true;

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


void media_view_draw_animated_image_controls()
{
	bool mouse_hover_imgui_window = util_mouse_hovering_imgui_window();

	// Seeking
	if ( !mouse_hover_imgui_window )
	{
		if ( ImGui::IsKeyDown( ImGuiKey_RightCtrl ) && ImGui::IsKeyPressed( ImGuiKey_LeftArrow, true ) )
		{
			media_view_frame_advance();
		}
		if ( ImGui::IsKeyDown( ImGuiKey_RightCtrl ) && ImGui::IsKeyPressed( ImGuiKey_RightArrow, true ) )
		{
			media_view_frame_advance( true );
		}
	}

	int width, height;
	SDL_GetWindowSize( app::window, &width, &height );

	ImVec2 playback_control_pos{};
	playback_control_pos.x = width / 2;
	//playback_control_pos.y = ( height - 75.f );
	playback_control_pos.y = ( height - 40.f );

	// check if mouse in rectangle

	static bool  was_drawing_controls = false;

	static float controls_height = 50.f;
	if ( !mouse_in_rect( { 0.f, height - ( 80.f + ( controls_height * 2 ) ) }, { (float)width, (float)height } ) )
	{
		if ( was_drawing_controls )
			set_frame_draw();

		was_drawing_controls = false;
		return;
	}

	if ( !was_drawing_controls )
		set_frame_draw();

	was_drawing_controls = true;

	// ----------------------------------------

	// pivot aligns it to the center and the bottom of the window
	ImGui::SetNextWindowPos( playback_control_pos, 0, ImVec2( 0.5f, 1.0f ) );

	ImGui::SetNextWindowSizeConstraints( { width - 80.f, -1.f }, { width - 80.f, -1.f } );

	if ( !ImGui::Begin( "##image_controls", 0, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing ) )
	{
		ImGui::End();
		return;
	}

	ImGuiStyle&  style         = ImGui::GetStyle();

	const ImVec2 label_size    = ImGui::CalcTextSize( "Pause", NULL, true );
	ImVec2       play_btn_size = ImGui::CalcItemSize( { 0, 0 }, label_size.x + style.FramePadding.x * 2.0f, label_size.y + style.FramePadding.y * 2.0f );

	if ( image_draw::pause )
	{
		if ( ImGui::Button( "Play", play_btn_size ) )
		{
			image_draw::pause = false;
		}
	}
	else
	{
		if ( ImGui::Button( "Pause", play_btn_size ) )
		{
			image_draw::pause = true;
		}
	}

	ImGui::SameLine();
	ImGui::Spacing();

	ImGui::SameLine();
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

	const ImVec2 time_size             = ImGui::CalcTextSize( str_position, NULL, true );

	float        avaliable_width = ImGui::GetContentRegionAvail()[ 0 ] - ( style.ItemSpacing.x * 2 );
	// float        avaliable_width = 500.f - ( style.ItemSpacing.x * 2 );

	float        seek_bar_width        = avaliable_width;
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
			mem_draw_debug_ui();

		ImGui::End();
	}

	if ( get_media_type() == e_media_type_video )
	{
		media_view_draw_video_controls();
	}
	else
	{
		if ( get_media_type() == e_media_type_image && g_image_data.image.frame.size() > 1 )
		{
			media_view_draw_animated_image_controls();
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


static void media_view_draw_frame( int width, int height, size_t frame_i )
{
	image_frame_t& frame       = g_image_data.image.frame[ frame_i ];

	int            draw_width  = frame.width * image_draw::zoom;
	int            draw_height = frame.height * image_draw::zoom;
	int            draw_x      = image_draw::pos.x + ( frame.pos_x * image_draw::zoom );
	int            draw_y      = image_draw::pos.y + ( frame.pos_y * image_draw::zoom );

	if ( image_draw::flip_h )
	{
		draw_width *= -1;
		draw_x += -draw_width;
	}

	if ( image_draw::flip_v )
	{
		draw_height *= -1;
		draw_y += -draw_height;
	}

	// dst_rect.w = round( dst_rect.w );
	// dst_rect.h = round( dst_rect.h );
	// dst_rect.x = round( dst_rect.x );
	// dst_rect.y = round( dst_rect.y );

	glEnable( GL_BLEND );
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

	glEnable( GL_TEXTURE_2D );

	if ( g_scale_state == e_scale_state_finished )
	{
		glBindTexture( GL_TEXTURE_2D, g_image_scaled_data.textures.frame[ frame_i ] );
	}
	else
	{
		glBindTexture( GL_TEXTURE_2D, g_image_data.textures.frame[ frame_i ] );
	}

	glMatrixMode( GL_PROJECTION );
	glLoadIdentity();

	glOrtho( 0, width, height, 0, -1, 1 );

	glMatrixMode( GL_MODELVIEW );
	glLoadIdentity();

	// get the center of the image
	int image_center_x = draw_x + draw_width * 0.5;
	int image_center_y = draw_y + draw_height * 0.5;

	glTranslatef( image_center_x, image_center_y, 0.0f );    // move pivot to center of the image
	glRotatef( image_draw::rot, 0, 0, 1 );                   // rotate around the image
	glTranslatef( -image_center_x, -image_center_y, 0.0f );  // move back

	glBegin( GL_QUADS );

	glTexCoord2i( 0, 0 );
	glVertex2i( draw_x, draw_y );
	glTexCoord2i( 1, 0 );
	glVertex2i( draw_x + draw_width, draw_y );
	glTexCoord2i( 1, 1 );
	glVertex2i( draw_x + draw_width, draw_y + draw_height );
	glTexCoord2i( 0, 1 );
	glVertex2i( draw_x, draw_y + draw_height );

	glEnd();

	glDisable( GL_TEXTURE_2D );
	glDisable( GL_BLEND );
}


static void media_view_draw_image()
{
	if ( g_image_data.textures.frame == nullptr )
	{
		printf( "NULLPTR IMAGE\n" );
		return;
	}

	if ( g_image_data.textures.count > 1 )
	{
		set_frame_draw( 2 );
	}

	if ( g_image_data.image.frame.size() <= image_draw::frame )
	{
		image_draw::frame = 0;
		printf( "IMAGE FRAME OUT OF BOUNDS\n" );
		return;
	}

	if ( g_image_data.image.frame.size() > 1 )
	{
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
	}
	else
	{
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	}

	image_frame_t&   frame         = g_image_data.image.frame[ image_draw::frame ];
	e_frame_disposal prev_disposal = frame.frame_disposal;

	// frame disposal seems to be how to handle THIS frame for the next frame drawn
	// so here, the current frame will use the last frame's disposal method for how to draw it
	// if it's keep, look for all previous frames to draw, until we hit 0 or one that's not keep

	int              width, height;
	// SDL_GetWindowSize( app::window, &width, &height );
	SDL_GetWindowSizeInPixels( app::window, &width, &height );

	if ( image_draw::frame > 0 )
	{
		image_frame_t& frame = g_image_data.image.frame[ image_draw::frame - 1 ];
		prev_disposal        = frame.frame_disposal;
	}

	if ( prev_disposal == e_frame_disposal_keep )
	{
		size_t last_frame_to_keep = image_draw::frame;

		if ( image_draw::frame > 0 )
		{
			for ( last_frame_to_keep--;; last_frame_to_keep-- )
			{
				image_frame_t& frame = g_image_data.image.frame[ last_frame_to_keep ];

				if ( frame.frame_disposal != e_frame_disposal_keep )
					break;

				if ( last_frame_to_keep == 0 )
					break;
			}
		}
		else
		{
			last_frame_to_keep = 0;
		}

		/// mmmm overdraw hell?
		for ( u32 i = last_frame_to_keep; i < image_draw::frame + 1; i++ )
		{
			media_view_draw_frame( width, height, i );
		}
	}
	else if ( prev_disposal == e_frame_disposal_previous )
	{
		if ( image_draw::frame > 0 )
			media_view_draw_frame( width, height, image_draw::frame - 1 );

		media_view_draw_frame( width, height, image_draw::frame );
	}
	else // e_frame_disposal_background ?
	{
		media_view_draw_frame( width, height, image_draw::frame );
	}
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

