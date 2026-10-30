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
	draw_info.hide_alpha    = image_draw::hide_alpha;
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
	draw_info.target_width  = g_image_data.image.width;
	draw_info.target_height = g_image_data.image.height;

	if ( g_image_data.image.flags & e_image_flag_has_palette || g_image_data.image.flags & e_image_flag_has_local_palettes )
	{
		draw_info.shader = e_shader_palette;

		if ( g_image_data.image.flags & e_image_flag_has_local_palettes )
		{
			draw_info.palette       = g_image_data.textures.palette[ frame_i ];
			draw_info.palette_trans = g_image_data.textures.palette_trans[ frame_i ];
			draw_info.palette_count = g_image_data.image.frame[ frame_i ].palette.count;
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
		draw_info.target  = g_main_image_anim_fbo;
	}
	else
	{
		draw_info.width         = static_cast< int >( round( std::max( 0.01, frame.width * image_draw::zoom ) ) );
		draw_info.height        = static_cast< int >( round( std::max( 0.01, frame.height * image_draw::zoom ) ) );
		draw_info.x             = static_cast< int >( round( image_draw::pos.x + ( frame.pos_x * image_draw::zoom ) ) );
		draw_info.y             = static_cast< int >( round( image_draw::pos.y + ( frame.pos_y * image_draw::zoom ) ) );
		draw_info.rotation      = image_draw::rot;
		draw_info.hide_alpha    = image_draw::hide_alpha;
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

	if ( g_image_data.image.frame.size() > 1 )
	{
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );

		glBindFramebuffer( GL_FRAMEBUFFER, g_main_image_anim_fbo );
		glClear( GL_COLOR_BUFFER_BIT );
	}
	else
	{
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	}

	e_frame_disposal prev_disposal = g_image_data.image.frame[ image_draw::frame ].frame_disposal;

	// frame disposal seems to be how to handle THIS frame for the next frame drawn
	// so here, the current frame will use the last frame's disposal method for how to draw it
	// if it's keep, look for all previous frames to draw, until we hit 0 or one that's not keep

	if ( image_draw::frame > 0 )
	{
		prev_disposal = g_image_data.image.frame[ image_draw::frame - 1 ].frame_disposal;
	}

	if ( prev_disposal == e_frame_disposal_keep )
	{
		size_t last_frame_to_keep = image_draw::frame;

		if ( !image_draw::show_raw_frames && image_draw::frame > 0 )
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
		else if ( image_draw::show_raw_frames )
		{
			last_frame_to_keep = image_draw::frame;
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
	else  // e_frame_disposal_background ?
	{
		media_view_draw_frame( width, height, image_draw::frame );
	}

	if ( g_image_data.image.frame.size() > 1 )
	{
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		media_view_draw_animated_fbo();
	}
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
	if ( g_media_load_start_time > 0 )
		media_view_draw_icon( e_icon_loading, width, height, false );
}


