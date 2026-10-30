#include "main.h"


constexpr double ZOOM_MIN = 0.01;
constexpr double ZOOM_MAX = 1000.0;


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


constexpr double             ZOOM_EPSILON   = DBL_EPSILON;
// constexpr double             ZOOM_TOO_CLOSE = 0.1;  // media_zoom_scale * 0.5 ?  // 0.05
constexpr double             ZOOM_TOO_CLOSE = 0.5;

static size_t                g_zoom_snap_0_index;
static std::vector< double > g_zoom_snap_values;


int                          qsort_zoom_values( const void* left, const void* right )
{
	const double& zoom_left  = *static_cast< const double* >( left );
	const double& zoom_right = *static_cast< const double* >( right );

	if ( zoom_left < zoom_right )
		return -1;

	if ( zoom_left > zoom_right )
		return 1;

	return 0;
}


bool media_view_same_zoom_level( double zoom_a, double zoom_b )
{
	if ( zoom_a > zoom_b + ZOOM_EPSILON )
		return false;

	if ( zoom_a < zoom_b - ZOOM_EPSILON )
		return false;

	return true;
}


int media_view_find_closest_zoom_step( double zoom )
{
	int zoom_step = -static_cast< int >( g_zoom_snap_0_index );

	for ( size_t zoom_i = 0; zoom_i < g_zoom_snap_values.size(); zoom_i++, zoom_step++ )
	{
		double zoom_level = g_zoom_snap_values[ zoom_i ];

		if ( media_view_same_zoom_level( zoom, zoom_level ) )
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

	double zoom_min              = ZOOM_MIN;

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

	// keep a list of zoom values we don't want filtered out
	const std::vector< double > special_zoom_levels = g_zoom_snap_values;

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
	for ( size_t zoom_i = 1; zoom_i < g_zoom_snap_values.size() - 1; )
	{
		double zoom_level_prev = g_zoom_snap_values[ zoom_i - 1 ];
		double zoom_level      = g_zoom_snap_values[ zoom_i ];

		// duplicate entry
		if ( zoom_level == zoom_level_prev )
		{
			vec_remove_index( g_zoom_snap_values, zoom_i );
			continue;
		}

		bool prev_cant_remove = false;
		bool cur_cant_remove  = false;

		for ( const double keep_zoom_level : special_zoom_levels )
		{
			prev_cant_remove |= media_view_same_zoom_level( zoom_level_prev, keep_zoom_level );
			cur_cant_remove |= media_view_same_zoom_level( zoom_level, keep_zoom_level );
		}

		// don't touch these
		if ( prev_cant_remove && cur_cant_remove )
		{
			zoom_i++;
			continue;
		}

		double zoom_diff_a = fabs( zoom_level - zoom_level_prev );

		if ( zoom_diff_a < ( ZOOM_TOO_CLOSE * app::config.media_zoom_scale ) * zoom_level )
		{
			// if we can't remove the current zoom step, remove the previous one
			if ( cur_cant_remove )
			{
				vec_remove_index( g_zoom_snap_values, zoom_i - 1 );
			}
			// otherwise, remove the current one
			else
			{
				vec_remove_index( g_zoom_snap_values, zoom_i );
			}
		}
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

		image_draw::size.x = g_image_data.image.width * image_draw::zoom;
		image_draw::size.y = g_image_data.image.height * image_draw::zoom;

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

	if ( util_mouse_hovering_imgui_window() )
		return;

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
	if ( media_view_same_zoom_level( fit_zoom, image_draw::zoom ) )
	{
		image_draw::zoom_mode = e_zoom_mode_fit;
		media_view_fit_in_view();
		return;
	}

	if ( media_view_same_zoom_level( fit_scale_up_zoom, image_draw::zoom ) )
	{
		image_draw::zoom_mode = e_zoom_mode_fit_window;
		media_view_fit_in_view();
		return;
	}

	image_draw::zoom_mode = e_zoom_mode_fixed;

	// round it so we don't get something like 0.9999564598 or whatever instead of 1.0
	image_draw::zoom      = std::max( ZOOM_MIN, round( image_draw::zoom * 1000 ) / 1000 );

	// get new factor
	factor                = image_draw::zoom / old_zoom;

	// recalculate draw width and height
	image_draw::size.x    = (double)g_image_data.image.width * image_draw::zoom;
	image_draw::size.y    = (double)g_image_data.image.height * image_draw::zoom;

	// recalculate image position to keep image where cursor is
	image_draw::pos.x     = scale_point_from_origin( app::mouse_pos[ 0 ], image_draw::pos.x, factor );
	image_draw::pos.y     = scale_point_from_origin( app::mouse_pos[ 1 ], image_draw::pos.y, factor );

	media_view_scale_reset_timer();
	media_view_clamp_to_bounds();

	set_frame_draw( 2 );
}

