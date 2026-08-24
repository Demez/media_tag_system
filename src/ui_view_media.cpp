#include "main.h"

#include "stb_image_resize2.h"

#include <thread>
#include <mutex>


// Image Draw Data
namespace image_draw
{
	e_zoom_mode zoom_mode     = e_zoom_mode_fit;
	double      zoom          = 1.f;
	int         zoom_step     = 0;  // 0 = 100% zoom

	vec2        pos{};
	vec2        size{};
	bool        flip_v           = false;
	bool        flip_h = false;
	float       rot              = 0.f;

	// Animated image playback information
	u64         last_frame_time  = 0;  // time in system time
	u64         next_frame_time  = 0;  // time until next frame, add to last_frame_time
	size_t      frame            = 0;
	double      playback_speed   = 1.0;
	bool        pause            = false;
	bool        scaling          = true;

	bool        hide_alpha       = false;

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

constexpr double         ZOOM_MIN    = 0.01;
constexpr double         ZOOM_MAX    = 1000.0;

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


// TODO: put these in the config
// constexpr float                     SCALE_WAIT_TIME = 0.1f;
constexpr u32              SCALE_WAIT_TIME = 100;  // in ms
constexpr float            UPSCALE_LIMIT   = 2.f;

static std::thread*        g_scale_thread;
static e_scale_state       g_scale_state  = e_scale_state_idle;
static std::atomic< bool > g_scale_signal = false;
static u64                 g_scale_time   = UINT64_MAX;
static image_t             g_scale_src{};

std::mutex                 g_scale_lock;


// wait X amount of time before scaling the image, the user may be in the middle of changing zoom level quickly, like currently scrolling the mouse wheel
// so it's wasteful to constantly keep scaling it if the scaled image isn't even what the user zoom level is at now
// trying to make sure the user stops changing the zoom level for a certain amount of time before scaling
void media_view_scale_thread_wait_timer()
{
	//int recheck_time = 0;
timer_wait:
	g_scale_lock.lock();
	u64 cur_time = sys_get_time_ms();

	if ( cur_time < g_scale_time )
	{
		u32 diff = static_cast< u32 >( g_scale_time - cur_time );
		g_scale_lock.unlock();
		//printf( "SCALE WAIT %d\n", ++recheck_time );

		SDL_Delay( std::min( SCALE_WAIT_TIME, diff ) );

		// I use this goto here in case the main thread reset the scale timer once again in between this state
		goto timer_wait;
	}

	g_scale_lock.unlock();
}


void media_view_scale_thread_run()
{
	while ( app::running )
	{
		// wait for queued data here
		g_scale_signal.wait( false );

		if ( !app::running )
			return;

		media_view_scale_thread_wait_timer();

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
			{
				g_scale_state = e_scale_state_upload;
				send_frame_draw_event();
			}

			// if ( g_scale_src.width != g_image_data.image.width )
			// 	printf( "scale source is different from currently displayed image!\n" );
		}
		else
		{
			g_scale_state = e_scale_state_idle;
		}

		g_scale_signal.store( false );
		g_scale_lock.unlock();
	}
}


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


void media_view_scale_fire_thread()
{
	g_scale_time  = sys_get_time_ms() + SCALE_WAIT_TIME;
	g_scale_state = e_scale_state_start;

	g_scale_signal = true;
	g_scale_signal.notify_one();
}


void media_view_scale_reset_timer()
{
	g_scale_time = sys_get_time_ms() + SCALE_WAIT_TIME;

	if ( g_scale_state == e_scale_state_finished )
		g_scale_state = e_scale_state_idle;

	set_frame_draw( 2 );
}


// return true if scale again
bool media_view_scale_handle_finished()
{
	g_scale_time = UINT64_MAX;

	// Are we drawing the image smaller than native size?
	if ( image_draw::size.x < ( g_image_data.image.width * UPSCALE_LIMIT ) || round( image_draw::size.x ) != g_image_data.image.width )
	{
		// Does the scaled image size match the size we draw it as?
		if ( int( image_draw::size.x ) == g_image_scaled_data.image.width )
			return false;

		// it does not, scale again
		media_view_scale_reset_timer();
		return true;
	}
	
	return false;
}


void media_view_scale_set_image()
{
	if ( g_image_data.image.frame.empty() || g_scale_state != e_scale_state_idle )
		return;

	// if ( image_draw::size.x >= ( g_image_data.image.width * UPSCALE_LIMIT ) && image_draw::size.x != g_image_data.image.width )
	if ( image_draw::zoom > UPSCALE_LIMIT && image_draw::size.x != g_image_data.image.width )
		return;

	// ????
	if ( !g_image_data.image.frame[ 0 ].data )
		return;

	g_scale_lock.lock();

	// copy a new image
	// TODO: ref count image_t, so we can avoid copying
	// so when the user switches images during scaling, it still holds on to the previous one thanks to the ref count
	// plus it uses less memory, unless image switching is happening during scaling, but that's a short time frame
	image_free( g_scale_src );

	image_copy_data( g_image_data.image, g_scale_src );

	g_scale_src.frame.clear();
	g_scale_src.frame.resize( 1 );

	image_copy_frame_data( g_image_data.image.frame[ 0 ], g_scale_src.frame[ 0 ] );

	// don't hold onto this
	g_scale_src.image_format    = nullptr;

	size_t image_size           = (size_t)g_image_data.image.width * (size_t)g_image_data.image.height * (size_t)g_image_data.image.bytes_per_pixel;
	g_scale_src.frame[ 0 ].data = ch_calloc< u8 >( image_size, e_mem_category_image_data );
	memcpy( g_scale_src.frame[ 0 ].data, g_image_data.image.frame[ 0 ].data, image_size * sizeof( u8 ) );

	g_scale_src.frame[ 0 ].size = image_size;
	g_image_scaled_data.index   = g_image_data.index;

	media_view_scale_fire_thread();

	g_scale_lock.unlock();
}


void media_view_scale_check_timer()
{
	if ( g_scale_state != e_scale_state_idle && g_scale_state != e_scale_state_finished )
		return;

	const media_entry_t& entry = gallery_item_get_media_entry( g_image_data.index );

	if ( entry.type == e_media_type_video )
		return;

	// Don't handle animated images for now
	if ( g_image_data.image.frame.size() > 1 )
		return;

	if ( g_scale_state == e_scale_state_finished )
		if ( !media_view_scale_handle_finished() )
			return;

	if ( g_scale_time == UINT64_MAX )
		return;

	// g_scale_timer -= frame_time;
	media_view_scale_set_image();
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
	g_scale_thread = new std::thread( media_view_scale_thread_run );
}


void media_view_shutdown()
{
	if ( !g_scale_thread )
		return;

	// wait for scale thread to shutdown
	g_scale_signal = true;
	g_scale_signal.notify_all();
	g_scale_thread->join();

	delete g_scale_thread;

	image_free( g_scale_src );
}


e_media_type get_media_type()
{
	if ( gallery::sorted_media.size() <= g_image_data.index )
		return e_media_type_none;

	return gallery_item_get_media_entry( g_image_data.index ).type;
}


// New Position = Scale Origin + ( Scale Point - Scale Origin ) * Scale Factor
double scale_point_from_origin( double origin, double point, double factor )
{
	return origin + ( point - origin ) * factor;
}


bool media_view_can_pan_image()
{
	int width, height;
	SDL_GetWindowSize( app::window, &width, &height );

	if ( width >= image_draw::size.x && height >= image_draw::size.y )
		return false;

	return true;
}


void media_view_clamp_to_bounds()
{
	vec2 min_bounds{};
	vec2 max_bounds{};

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
		image_draw::pos.x = width / 2 - ( image_draw::size.x / 2 );
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
		image_draw::pos.y = height / 2 - ( image_draw::size.y / 2 );
	}
}


// or DBL_EPSILON ?
constexpr double             ZOOM_EPSILON = 0.01;

static size_t                g_zoom_snap_0_index;
static std::vector< double > g_zoom_snap_values;


int qsort_zoom_values( const void* left, const void* right )
{
	const double& zoom_left  = *static_cast< const double* >( left );
	const double& zoom_right = *static_cast< const double* >( right );

	if ( zoom_left < zoom_right )
		return -1;

	if ( zoom_left > zoom_right )
		return 1;

	return 0;
}


bool media_view_is_zoom_level( double snap_level, double new_zoom )
{
	// is this zoom close enough to the snap level?
	if ( new_zoom > snap_level + ZOOM_EPSILON )
		return false;

	if ( new_zoom < snap_level - ZOOM_EPSILON )
		return false;

	return true;
}


int media_view_find_closest_zoom_step( double zoom )
{
	int    zoom_step = -static_cast< int >( g_zoom_snap_0_index );

	for ( size_t zoom_i = 0; zoom_i < g_zoom_snap_values.size(); zoom_i++, zoom_step++ )
	{
		double zoom_level = g_zoom_snap_values[ zoom_i ];

		if ( media_view_is_zoom_level( zoom, zoom_level ) )
			return zoom_step;
	}

	// fallback, should not reach here ideally
	return 0;
}


void media_view_build_zoom_steps( double fit_zoom, double fit_scale_up_zoom )
{
	// int zoom_step_fit = media_view_find_closest_zoom_step( fit_zoom );

	g_zoom_snap_values.clear();
		
	app::config.media_zoom_scale = std::max( 0.01f, app::config.media_zoom_scale );

	double zoom_min = ZOOM_MIN;

	if ( app::config.zoom_under_window_size )
	{
		g_zoom_snap_values.push_back( ZOOM_MIN );
		g_zoom_snap_values.push_back( 1.0 );
		g_zoom_snap_values.push_back( 2.0 );
	}
	else
	{
		if ( fit_zoom + ZOOM_EPSILON <= ZOOM_MIN )
			g_zoom_snap_values.push_back( ZOOM_MIN );

		if ( fit_zoom + ZOOM_EPSILON <= 1.0 )
			g_zoom_snap_values.push_back( 1.0 );

		if ( fit_zoom + ZOOM_EPSILON <= 2.0 )
			g_zoom_snap_values.push_back( 2.0 );

		if ( g_zoom_snap_values.size() )
			zoom_min = g_zoom_snap_values.front();
		else
			zoom_min = fit_zoom;
	}

	g_zoom_snap_values.push_back( fit_zoom );

	if ( fit_scale_up_zoom != fit_zoom )
		g_zoom_snap_values.push_back( fit_scale_up_zoom );

	g_zoom_snap_values.push_back( ZOOM_MAX );

	// build the standard zoom levels now

	// Zooming under 100%
	for ( double zoom = 1.0;; )
	{
		zoom *= 1.0 - app::config.media_zoom_scale;

		if ( zoom <= zoom_min )
			break;

		g_zoom_snap_values.push_back( zoom );
	}

	// Zooming over 100%
	for ( double zoom = 1.0;; )
	{
		zoom *= 1.0 + app::config.media_zoom_scale;

		if ( zoom >= ZOOM_MAX )
			break;

		g_zoom_snap_values.push_back( zoom );
	}

	std::qsort( g_zoom_snap_values.data(), g_zoom_snap_values.size(), sizeof( double ), qsort_zoom_values );

	// remove values that are too close to each other
	for ( size_t zoom_i = 1; zoom_i < g_zoom_snap_values.size() - 1;  )
	{
		double zoom_level_prev = g_zoom_snap_values[ zoom_i - 1 ];
		double zoom_level      = g_zoom_snap_values[ zoom_i ];

		// duplicate entry
		if ( zoom_level == zoom_level_prev )
		{
			vec_remove_index( g_zoom_snap_values, zoom_i );
			continue;
		}

		// don't touch these
		if ( zoom_level == fit_zoom || zoom_level == fit_scale_up_zoom )
		{
			zoom_i++;
			continue;
		}

		double zoom_diff_a = fabs( zoom_level - zoom_level_prev );

		//double zoom_threshold = 1.0;
		//
		//if ( zoom_level > 1.0 )
		//	zoom_threshold += app::config.media_zoom_scale;
		//else
		//	zoom_threshold -= app::config.media_zoom_scale;

		if ( zoom_diff_a < 0.05 * zoom_level )
		{
			vec_remove_index( g_zoom_snap_values, zoom_i );
		}
		//else if ( zoom_diff_b < 0.05 * zoom_level )
		//{
		//	vec_remove_index( g_zoom_snap_values, zoom_i );
		//}
		else
		{
			zoom_i++;
		}
	}

	g_zoom_snap_0_index = vec_index( g_zoom_snap_values, 1.0, 0 );
}


double media_view_get_zoom_level( int& zoom_step )
{
	// if under 100% zoom, and is an out of index range, give them the minimum zoom level
	if ( zoom_step < 0 && -zoom_step > g_zoom_snap_0_index )
	{
		zoom_step = -1 * static_cast< int >( g_zoom_snap_0_index );
		return g_zoom_snap_values.front();
	}

	size_t offset = static_cast< size_t >( zoom_step ) + g_zoom_snap_0_index;

	// if the offset is out of range, give them the max zoom level
	if ( offset >= g_zoom_snap_values.size() )
	{
		zoom_step = static_cast< int >( g_zoom_snap_values.size() - g_zoom_snap_0_index );
		return g_zoom_snap_values.back();
	}

	return g_zoom_snap_values.at( offset );
}


void media_view_fit_in_view( bool adjust_zoom, bool center_image, bool adjust_zoom_step )
{
	// new image size
	int width, height;
	SDL_GetWindowSize( app::window, &width, &height );

	// Fit image in window size
	double factor[ 2 ] = {
		(double)width / (double)g_image_data.image.width,
		(double)height / (double)g_image_data.image.height,
	};

	double fit_scale_up_zoom = std::min( factor[ 0 ], factor[ 1 ] );
	double fit_zoom          = std::min( fit_scale_up_zoom, 1.0 );

	media_view_build_zoom_steps( fit_zoom, fit_scale_up_zoom );

	if ( adjust_zoom )
	{
		if ( image_draw::zoom_mode == e_zoom_mode_fit_window )
		{
			image_draw::zoom = fit_scale_up_zoom;
		}
		else
		{
			image_draw::zoom      = fit_zoom;
			image_draw::zoom_mode = e_zoom_mode_fit;
		}

		if ( adjust_zoom_step )
			image_draw::zoom_step = media_view_find_closest_zoom_step( image_draw::zoom );

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
	image_draw::zoom_step = 0;

	image_draw::zoom_mode = e_zoom_mode_fixed;

	if ( !g_image_data.image.frame.size() )
		return;

	image_draw::size.x = g_image_data.image.width;
	image_draw::size.y = g_image_data.image.height;

	media_view_scale_reset_timer();
	media_view_clamp_to_bounds();
}


void media_view_scroll_zoom( int scroll )
{
	if ( directory::folder_loading )
		return;

	if ( !g_image_data.textures.count || scroll == 0 )
		return;

	//if ( util_mouse_hovering_imgui_window() )
	//	return;

	// Check zoom limits
	if ( scroll > 0 )
	{
		// max zoom level
		if ( image_draw::zoom >= ZOOM_MAX )
			return;
	}
	else
	{
		// min zoom level
		if ( image_draw::zoom <= ZOOM_MIN )
			return;
	}

	double factor = 1.0;

	int    width, height;
	SDL_GetWindowSize( app::window, &width, &height );

	double fit_factor[ 2 ] = {
		(double)width / (double)g_image_data.image.width,
		(double)height / (double)g_image_data.image.height,
	};

	double fit_scale_up_zoom = std::min( fit_factor[ 0 ], fit_factor[ 1 ] );
	double fit_zoom          = std::min( fit_scale_up_zoom, 1.0 );

	media_view_build_zoom_steps( fit_zoom, fit_scale_up_zoom );

	// Zoom in if scrolling up
	image_draw::zoom_step += scroll;

	double old_zoom  = image_draw::zoom;
	image_draw::zoom = media_view_get_zoom_level( image_draw::zoom_step );

	// Special case for fit zoom levels
	if ( media_view_is_zoom_level( fit_zoom, image_draw::zoom ) )
	{
		image_draw::zoom_mode = e_zoom_mode_fit;
		media_view_fit_in_view();
		return;
	}

	if ( media_view_is_zoom_level( fit_scale_up_zoom, image_draw::zoom ) )
	{
		image_draw::zoom_mode = e_zoom_mode_fit_window;
		media_view_fit_in_view();
		return;
	}

	image_draw::zoom_mode = e_zoom_mode_fixed;

	// round it so we don't get something like 0.9999564598 or whatever instead of 1.0
	image_draw::zoom   = std::max( ZOOM_MIN, round( image_draw::zoom * 1000 ) / 1000 );

	// get new factor
	factor             = image_draw::zoom / old_zoom;

	// recalculate draw width and height
	image_draw::size.x = (double)g_image_data.image.width * image_draw::zoom;
	image_draw::size.y = (double)g_image_data.image.height * image_draw::zoom;

	// recalculate image position to keep image where cursor is
	image_draw::pos.x  = scale_point_from_origin( app::mouse_pos[ 0 ], image_draw::pos.x, factor );
	image_draw::pos.y  = scale_point_from_origin( app::mouse_pos[ 1 ], image_draw::pos.y, factor );

	media_view_scale_reset_timer();
	media_view_clamp_to_bounds();

	set_frame_draw( 2 );
}


void media_view_draw_media_info()
{
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
}


void media_view_input()
{
	if ( g_scale_state == e_scale_state_upload )
	{
		g_scale_src.frame.clear();

		if ( g_image_scaled_data.index == g_image_data.index )
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
//	if ( !ImGui::IsKeyDown( ImGuiKey_RightCtrl ) )
//	{
//		if ( ImGui::IsKeyPressed( ImGuiKey_RightArrow, true ) )
//		{
//			media_view_advance();
//		}
//		else if ( ImGui::IsKeyPressed( ImGuiKey_LeftArrow, true ) )
//		{
//			media_view_advance( true );
//		}
//
//		//if ( ImGui::IsMouseDoubleClicked( ImGuiMouseButton_Left ) )
//		//{
//		//	set_view_type_gallery();
//		//}
//	}

	// TODO: Test ImGui::Shortcut()
//	if ( app::window_focused && ImGui::IsKeyDown( ImGuiKey_LeftCtrl ) && ImGui::IsKeyPressed( ImGuiKey_C, false ) )
//	{
//		fs::path path = gallery_item_get_path( g_image_data.index );
//
//		if ( sys_copy_to_clipboard( { directory::path / path } ) )
//		{
//			printf( "Copied to Clipboard\n" );
//			push_notification( "Copied" );
//		}
//		else
//		{
//			printf( "Failed to Copy to Clipboard\n" );
//			push_notification( "COPY FAILED" );
//		}
//	}

	media_view_context_menu();

	// if ( !util_mouse_hovering_imgui_window() )
	// 	return;

#if 0
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

	if ( mouse_middle_down && !mouse_hover_imgui_window && mouse_in_client_area )
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
#endif
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


void media_view_load()
{
	if ( gallery::sorted_media.empty() )
		return;

	if ( g_image_data.index >= gallery::sorted_media.size() )
		return;

	double            load_time = 0.0;
	media_entry_t     entry     = gallery_item_get_media_entry( g_image_data.index );
	fs::path          full_path = directory::path / entry.file.path;
	std::string       path_str  = sys_path_to_string( full_path );

	image_load_info_t image_load_info{};
	image_load_info.image = &g_image_data.image;

	u64 start_time = sys_get_time_ms();

	if ( entry.type == e_media_type_image )
	{
		mpv_cmd_close_video();
		image_load( full_path, image_load_info );
	}
	else
	{
		image_free( g_image_data.image );
		mpv_cmd_loadfile( path_str.c_str() );
	}
		
	media_history_add( path_str );

	auto current_time = sys_get_time_ms();
	load_time         = ( current_time / 1000.0 ) - ( start_time / 1000.0 );

	// auto startTime       = std::chrono::high_resolution_clock::now();

	if ( entry.type == e_media_type_image )
	{
		if ( image_load_info.image->frame.size() > 0 && image_load_info.image->bytes_per_pixel > 0 )
		{
			gl_update_textures( g_image_data.textures, &g_image_data.image, g_image_data.image.frame.size() );
			media_view_fit_in_view();

			image_draw::frame = 0;
			media_view_frame_update_timer();
		}
		else
		{
			path_printf( "%f FAILED Load - %s\n", load_time, full_path.c_str() );
		}
	}

	// auto  currentTime    = std::chrono::high_resolution_clock::now();
	// float up_time        = std::chrono::duration< float, std::chrono::seconds::period >( currentTime - startTime ).count();
	//printf( "%f Load - %f Up - %s\n", load_time, up_time, directory::media_list[ g_folder_index ].string().c_str() );
	path_printf( "%f Load - %s\n", load_time, full_path.c_str() );

	// g_image_data.index = image_draw::media_index;

	update_window_title();
	
	media_view_scale_reset_timer();
	media_view_scale_set_image();

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
		if ( g_image_data.index == 0 )
			g_image_data.index = gallery::sorted_media.size();

		g_image_data.index--;
	}
	else
	{
		g_image_data.index++;

		if ( g_image_data.index == gallery::sorted_media.size() )
			g_image_data.index = 0;
	}

	gallery_view_set_selection( g_image_data.index );

	if ( gallery_item_get_media_entry( g_image_data.index ).type == e_media_type_directory )
		goto advance;

	media_view_load();
}


void media_view_draw_video_controls( bool mouse_hover_imgui_window )
{
	if ( !g_mpv )
		return;
}


void media_view_draw_animated_image_controls( bool mouse_hover_imgui_window )
{
}


void media_view_draw_close_button()
{
}


void media_view_draw_nav_buttons()
{
}


void media_view_draw_imgui()
{
	media_view_input();

	media_view_draw_close_button();
	media_view_draw_nav_buttons();

	if ( g_draw_media_info )
		media_view_draw_media_info();

	if ( g_draw_mem_stats )
	{
	}

	if ( get_media_type() == e_media_type_video )
	{
		media_view_draw_video_controls( false );
	}
	else
	{
		if ( get_media_type() == e_media_type_image && g_image_data.image.frame.size() > 1 )
		{
			media_view_draw_animated_image_controls( false );
		}

		//if ( g_draw_zoom_level )
		//{
		//	ImGui::Begin( "##zoom_level", 0, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration );
		//	ImGui::Text( "%.1f%%", (float)( image_draw::zoom * 100 ) );
		//	ImGui::End();
		//}
	}
}


static void media_view_draw_frame( int width, int height, size_t frame_i )
{
	image_frame_t&        frame       = g_image_data.image.frame[ frame_i ];

	int                   draw_width  = frame.width * image_draw::zoom;
	int                   draw_height = frame.height * image_draw::zoom;
	int                   draw_x      = image_draw::pos.x + ( frame.pos_x * image_draw::zoom );
	int                   draw_y      = image_draw::pos.y + ( frame.pos_y * image_draw::zoom );

	render_draw_texture_t draw_info{};
	draw_info.width      = draw_width;
	draw_info.height     = draw_height;
	draw_info.x          = draw_x;
	draw_info.y          = draw_y;
	draw_info.rotation   = image_draw::rot;
	draw_info.hide_alpha = image_draw::hide_alpha;

	draw_info.flip_h     = image_draw::flip_h;
	draw_info.flip_v     = image_draw::flip_v;

	if ( g_scale_state == e_scale_state_finished && image_draw::scaling )
		draw_info.texture = g_image_scaled_data.textures.frame[ frame_i ];
	else
		draw_info.texture = g_image_data.textures.frame[ frame_i ];

	render_draw_texture( draw_info );
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

	e_frame_disposal prev_disposal = g_image_data.image.frame[ image_draw::frame ].frame_disposal;

	// frame disposal seems to be how to handle THIS frame for the next frame drawn
	// so here, the current frame will use the last frame's disposal method for how to draw it
	// if it's keep, look for all previous frames to draw, until we hit 0 or one that's not keep

	int              width, height;
	// SDL_GetWindowSize( app::window, &width, &height );
	SDL_GetWindowSizeInPixels( app::window, &width, &height );

	if ( image_draw::frame > 0 )
	{
		prev_disposal = g_image_data.image.frame[ image_draw::frame - 1 ].frame_disposal;
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

