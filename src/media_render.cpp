#include "main.h"


static GLuint g_main_image_anim_fbo = 0;
static GLuint g_main_image_anim_tex = 0;


bool media_view_render_init()
{
	glGenFramebuffers( 1, &g_main_image_anim_fbo );
	glGenTextures( 1, &g_main_image_anim_tex );

	return true;
}


// creates framebuffer object to draw animated images onto before scaling it down
// https://www.opengl-tutorial.org/intermediate-tutorials/tutorial-14-render-to-texture/
void media_view_create_animated_image_fbo( GLenum format )
{
	glBindFramebuffer( GL_FRAMEBUFFER, g_main_image_anim_fbo );
	glBindTexture( GL_TEXTURE_2D, g_main_image_anim_tex );

	// create an empty image
	glTexImage2D( GL_TEXTURE_2D, 0, format, g_image_data.image.width, g_image_data.image.height, 0, format, GL_UNSIGNED_BYTE, 0 );

	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );

	glFramebufferTexture( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, g_main_image_anim_tex, 0 );

	// Set the list of draw buffers.
	GLenum DrawBuffers[ 1 ] = { GL_COLOR_ATTACHMENT0 };
	glDrawBuffers( 1, DrawBuffers );  // "1" is the size of DrawBuffers

	if ( glCheckFramebufferStatus( GL_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE )
		printf( "FAILED TO MAKE ANIMATED IMAGE FBO!\n" );
}


static void media_view_draw_icon( e_icon icon_value, int window_width, int window_height, bool center = true )
{
	image_t*              icon = icon_get_image( icon_value );
	render_draw_texture_t draw_info{};

	if ( center )
	{
		draw_info.x = window_width / 2 - ( icon->width / 2 );
		draw_info.y = window_height / 2 - ( icon->height / 2 );
	}
	else
	{
		ImGuiStyle& style   = ImGui::GetStyle();
		float       padding = ( style.WindowPadding.x + style.FramePadding.x ) * 3.f + ( style.ItemSpacing.x * 2.f ) + ImGui::GetFontSize();

		draw_info.x         = window_width - ( padding + icon->width );
		draw_info.y         = style.WindowPadding.y * 2.f + style.FramePadding.y;
	}

	draw_info.width    = icon->width;
	draw_info.height   = icon->height;
	draw_info.rotation = 0.f;
	draw_info.texture  = icon_get_texture( icon_value );
	draw_info.blend    = e_frame_blend_alpha;

	render_set_fbo( 0, 0, window_width, window_height );
	render_draw_texture( draw_info );
}


// this is fucking stupid
static void media_view_set_palette_bg( image_palette_t& palette, size_t frame_i )
{
	if ( !image_draw::hide_alpha )
		return;

	if ( !palette.elem || palette.bg_index == IMAGE_PALETTE_INVALID )
	{
		// render_clear( { 0.f, 0.f, 0.f, 0.f } );
		return;
	}

	image_palette_elem_t& elem = palette.elem[ palette.bg_index ];

	// render_clear( { elem.r / 255.f, elem.g / 255.f, elem.b / 255.f, 1.f } );

	bool                  animated = g_image_data.textures.count > 1;
	image_frame_t&        frame    = g_image_data.image.frame[ frame_i ];

	render_draw_texture_t draw_info{};
	//draw_info.target_width  = g_image_data.image.width;
	//draw_info.target_height = g_image_data.image.height;

	draw_info.shader         = e_shader_color;
	draw_info.clear_bg       = true;

	if ( image_draw::disable_palette )
	{
		draw_info.clear_bg_color = { palette.bg_index / 255.f, palette.bg_index / 255.f, palette.bg_index / 255.f, 1.f };
	}
	else
	{
		draw_info.clear_bg_color = { elem.r / 255.f, elem.g / 255.f, elem.b / 255.f, 1.f };
	}

	// draw_info.width   = frame.width;
	// draw_info.height  = frame.height;
	// draw_info.x       = frame.pos_x;
	// draw_info.y       = frame.pos_y;
	draw_info.texture        = 0;

	if ( animated )
	{
		draw_info.width  = g_image_data.image.width;
		draw_info.height = g_image_data.image.height;
		draw_info.x      = 0;
		draw_info.y      = 0;
		draw_info.blend  = image_draw::hide_alpha ? e_frame_blend_none : frame.blend;

		// draw_info.width  = frame.width;
		// draw_info.height = frame.height;
		// draw_info.x      = frame.pos_x;
		// draw_info.y      = frame.pos_y;
	}
	else
	{
		draw_info.width      = static_cast< int >( round( std::max( 0.01, frame.width * image_draw::zoom ) ) );
		draw_info.height     = static_cast< int >( round( std::max( 0.01, frame.height * image_draw::zoom ) ) );
		draw_info.x          = static_cast< int >( round( image_draw::pos.x + ( frame.pos_x * image_draw::zoom ) ) );
		draw_info.y          = static_cast< int >( round( image_draw::pos.y + ( frame.pos_y * image_draw::zoom ) ) );
		draw_info.rotation   = image_draw::rot;
		draw_info.blend      = image_draw::hide_alpha ? e_frame_blend_none : frame.blend;
		draw_info.flip_h     = image_draw::flip_h;
		draw_info.flip_v     = image_draw::flip_v;
	}

	render_draw_texture( draw_info );
}


static void media_view_draw_animated_fbo()
{
	render_draw_texture_t draw_info{};
	draw_info.width         = roundf( image_draw::size.x );
	draw_info.height        = roundf( image_draw::size.y );
	draw_info.x             = roundf( image_draw::pos.x );
	draw_info.y             = roundf( image_draw::pos.y );
	draw_info.rotation      = image_draw::rot;
	draw_info.blend         = image_draw::hide_alpha ? e_frame_blend_none : e_frame_blend_alpha;
	draw_info.flip_h        = image_draw::flip_h;
	draw_info.flip_v        = !image_draw::flip_v;
	draw_info.texture       = g_main_image_anim_tex;
	draw_info.invert_colors = image_draw::invert_color;

	for ( u8 i = 0; i < 4; i++ )
		draw_info.hide_channel[ i ] = !image_draw::show_channels[ i ];

	render_draw_texture( draw_info );
}


static void media_view_draw_frame( int width, int height, size_t frame_i )
{
	bool                  animated = g_image_data.textures.count > 1;
	image_frame_t&        frame    = g_image_data.image.frame[ frame_i ];

	render_draw_texture_t draw_info{};
	//draw_info.target_width  = g_image_data.image.width;
	//draw_info.target_height = g_image_data.image.height;

	if ( g_image_data.image.flags & e_image_flag_has_palette || g_image_data.image.flags & e_image_flag_has_local_palettes )
	{
		draw_info.shader          = e_shader_palette;
		draw_info.palette_enabled = !image_draw::disable_palette;

		// if ( image_draw::disable_palette )
		{
			if ( g_image_data.image.flags & e_image_flag_has_local_palettes )
			{
				draw_info.palette       = g_image_data.textures.palette[ frame_i ];
				draw_info.palette_trans = g_image_data.textures.palette_trans[ frame_i ];
				draw_info.palette_count = g_image_data.image.frame[ frame_i ].palette.count;
			}
		}
	}

	if ( frame.color_type == e_colortype_min_is_white )
		draw_info.shader = e_shader_min_is_white;

	else if ( frame.color_type == e_colortype_min_is_black )
		draw_info.shader = e_shader_min_is_black;

	if ( animated )
	{
		draw_info.width   = frame.width;
		draw_info.height  = frame.height;
		draw_info.x       = frame.pos_x;
		draw_info.y       = frame.pos_y;
		draw_info.texture = g_image_data.textures.frame[ frame_i ];
		draw_info.blend   = frame.blend;
		//draw_info.target  = g_main_image_anim_fbo;
	}
	else
	{
		draw_info.width         = static_cast< int >( round( std::max( 0.01, frame.width * image_draw::zoom ) ) );
		draw_info.height        = static_cast< int >( round( std::max( 0.01, frame.height * image_draw::zoom ) ) );
		draw_info.x             = static_cast< int >( round( image_draw::pos.x + ( frame.pos_x * image_draw::zoom ) ) );
		draw_info.y             = static_cast< int >( round( image_draw::pos.y + ( frame.pos_y * image_draw::zoom ) ) );
		draw_info.rotation      = image_draw::rot;
		draw_info.blend         = image_draw::hide_alpha ? e_frame_blend_none : frame.blend;
		draw_info.flip_h        = image_draw::flip_h;
		draw_info.flip_v        = image_draw::flip_v;
		draw_info.invert_colors = image_draw::invert_color;

		if ( media_get_scale_state() == e_scale_state_finished && image_draw::scaling )
			draw_info.texture = g_image_scaled_data.textures.frame[ frame_i ];
		else
			draw_info.texture = g_image_data.textures.frame[ frame_i ];

		for ( u8 i = 0; i < 4; i++ )
			draw_info.hide_channel[ i ] = !image_draw::show_channels[ i ];
	}

	render_draw_texture( draw_info );
}


bool media_view_draw_load_status( int width, int height )
{
#if 0
	//if ( g_media_load_status )
	if ( g_media_load_start_time > 0 )
	{
		u64 cur_time = sys_get_time_ms();
		u64 diff = cur_time - g_media_load_start_time;

		if ( diff > IMAGE_LOAD_WAIT_TIME )
		{
			media_view_draw_icon( e_icon_loading, width, height );
			return true;
		}
	}
#endif

	// if ( g_image_data.index == UINT32_MAX || g_image_data.textures.frame == nullptr )
	if ( g_media_load_from_gallery )
	{
		// printf( "NULLPTR IMAGE\n" );
		if ( g_media_load_start_time > 0 )
		{
			media_view_draw_icon( e_icon_loading, width, height );
		}
		else
		{
			media_view_draw_icon( e_icon_invalid, width, height );
		}

		return true;
	}

	return false;
}


static size_t media_view_get_starting_frame()
{
	size_t starting_frame = image_draw::frame;

	if ( image_draw::show_raw_frames || starting_frame == 0 )
		return starting_frame;

	// no previous frames to draw at all
	if ( g_image_data.image.frame[ starting_frame ].blend == e_frame_blend_none )
		return starting_frame;
		
	starting_frame--;

	for ( ;; starting_frame-- )
	{
		image_frame_t& frame = g_image_data.image.frame[ starting_frame ];

		if ( frame.frame_disposal == e_frame_disposal_background )
		{
			// we don't render the background disposed frame, the rendered image is cleared after that was drawn
			// so just start with the frame after that
			starting_frame++;
			break;
		}

		if ( starting_frame == 0 )
			break;
	}

	return starting_frame;
}


static void media_view_draw_image( int width, int height )
{
	if ( g_image_data.textures.frame == nullptr )
	{
		if ( g_media_load_start_time > 0 )
			media_view_draw_icon( e_icon_loading, width, height );

		else
			media_view_draw_icon( e_icon_invalid, width, height );

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

	bool using_fbo = false;

	if ( g_image_data.image.frame.size() > 1 )
	{
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );

		render_set_fbo( g_main_image_anim_fbo, g_main_image_anim_tex, g_image_data.image.width, g_image_data.image.height );
		using_fbo = true;

		render_clear( { 0.f, 0.f, 0.f, 0.f } );
		media_view_set_palette_bg( g_image_data.image.palette, 0 );
	}
	else
	{
		render_set_fbo( 0, 0, width, height );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	}

	size_t starting_frame = media_view_get_starting_frame();

	media_view_set_palette_bg( g_image_data.image.frame[ starting_frame ].palette, starting_frame );

	for ( size_t frame_i = starting_frame; frame_i < image_draw::frame; frame_i++ )
	{
		image_frame_t& frame = g_image_data.image.frame[ frame_i ];

		// these frames don't stay on the canvas, so skip previous cases of these
		if ( frame.frame_disposal == e_frame_disposal_previous )
			continue;

		media_view_draw_frame( width, height, frame_i );
	}

	// finally, draw the current frame
	media_view_draw_frame( width, height, image_draw::frame );

	if ( g_image_data.image.frame.size() > 1 )
	{
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		render_set_fbo( 0, 0, width, height );
		media_view_draw_animated_fbo();
	}

	render_finish();
}


void media_view_draw()
{
	int width, height;
	SDL_GetWindowSizeInPixels( app::window, &width, &height );

	if ( media_view_draw_load_status( width, height ) )
		return;

	if ( get_media_type() == e_media_type_video )
	{
		mpv_draw_frame();
	}
	else
	{
		media_view_draw_image( width, height );
	}

	// show loading icon in corner
	if ( g_media_load_start_time > 0 || gallery::scan_state != e_gallery_scan_idle )
		media_view_draw_icon( e_icon_loading, width, height, false );
}


