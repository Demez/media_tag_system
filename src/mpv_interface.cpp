#include "main.h"
#include "util.h"

// some reference here
// https://github.com/mpv-player/mpv-examples/blob/master/libmpv/sdl/main.c

void*                             g_mpv_module  = nullptr;
mpv_handle*                       g_mpv         = nullptr;
mpv_render_context*               g_mpv_gl      = nullptr;
GLuint                            g_mpv_fbo     = 0;
GLuint                            g_mpv_fbo_tex = 0;
GLuint                            g_mpv_rbo     = 0;
static ivec2                      g_mpv_framebuffer_size{};

u32                               g_wakeup_on_mpv_render_update, g_wakeup_on_mpv_events;
static bool                       g_mpv_redraw    = false;

static char*                      g_current_video = nullptr;

s64                               g_video_width = 0, g_video_height = 0;

bool                              g_scale_up_video = true;


static std::vector< std::string > g_mpv_exts;


namespace image_draw
{
	extern bool  flip_v;
	extern bool  flip_h;
	extern float rot;
}

#define FUNC_PTR( func ) func##_t p_##func = nullptr

// function pointers
// client.h
FUNC_PTR( mpv_client_api_version );
FUNC_PTR( mpv_error_string );
FUNC_PTR( mpv_free );
FUNC_PTR( mpv_client_name );
FUNC_PTR( mpv_client_id );
FUNC_PTR( mpv_create );
FUNC_PTR( mpv_initialize );
FUNC_PTR( mpv_destroy );
FUNC_PTR( mpv_terminate_destroy );
FUNC_PTR( mpv_create_client );
FUNC_PTR( mpv_create_weak_client );
FUNC_PTR( mpv_load_config_file );
FUNC_PTR( mpv_get_time_ns );
FUNC_PTR( mpv_get_time_us );
FUNC_PTR( mpv_free_node_contents );
FUNC_PTR( mpv_set_option );
FUNC_PTR( mpv_set_option_string );
FUNC_PTR( mpv_command );
FUNC_PTR( mpv_command_node );
FUNC_PTR( mpv_command_ret );
FUNC_PTR( mpv_command_string );
FUNC_PTR( mpv_command_async );
FUNC_PTR( mpv_command_node_async );
FUNC_PTR( mpv_abort_async_command );
FUNC_PTR( mpv_set_property );
FUNC_PTR( mpv_set_property_string );
FUNC_PTR( mpv_del_property );
FUNC_PTR( mpv_set_property_async );
FUNC_PTR( mpv_get_property );
FUNC_PTR( mpv_get_property_string );
FUNC_PTR( mpv_get_property_osd_string );
FUNC_PTR( mpv_get_property_async );
FUNC_PTR( mpv_observe_property );
FUNC_PTR( mpv_unobserve_property );
FUNC_PTR( mpv_event_name );
FUNC_PTR( mpv_event_to_node );
FUNC_PTR( mpv_request_event );
FUNC_PTR( mpv_request_log_messages );
FUNC_PTR( mpv_wait_event );
FUNC_PTR( mpv_wakeup );
FUNC_PTR( mpv_set_wakeup_callback );
FUNC_PTR( mpv_wait_async_requests );
FUNC_PTR( mpv_hook_add );
FUNC_PTR( mpv_hook_continue );
FUNC_PTR( mpv_get_wakeup_pipe );

// render.h
FUNC_PTR( mpv_render_context_create );
FUNC_PTR( mpv_render_context_set_parameter );
FUNC_PTR( mpv_render_context_get_info );
FUNC_PTR( mpv_render_context_set_update_callback );
FUNC_PTR( mpv_render_context_update );
FUNC_PTR( mpv_render_context_render );
FUNC_PTR( mpv_render_context_report_swap );
FUNC_PTR( mpv_render_context_free );


#define LOAD_FUNC( func )                                            \
	p_##func = (func##_t)sys_load_func( g_mpv_module, #func ); \
	if ( p_##func == nullptr )                                 \
	{                                                          \
		char* sys_error = sys_get_error();                     \
		printf( "sys_load_func failed: %s\n", sys_error );     \
		ch_free_str( sys_error );                              \
		return false;                                          \
	}


bool load_mpv_dll()
{
	if ( app::config.no_video )
		return false;

#if _WIN32
	g_mpv_module = sys_load_library( L"libmpv-2.dll" );
#else
	g_mpv_module = sys_load_library( "libmpv.so" );
#endif

	if ( g_mpv_module == nullptr )
	{
		char* sys_error = sys_get_error(); 
		printf( "sys_load_library failed: %s\n", sys_error );
		ch_free_str( sys_error );
		return false;
	}

	// load mpv function pointers

	// client.h
	LOAD_FUNC( mpv_client_api_version );
	LOAD_FUNC( mpv_error_string );
	LOAD_FUNC( mpv_free );
	LOAD_FUNC( mpv_client_name );
	LOAD_FUNC( mpv_client_id );
	LOAD_FUNC( mpv_create );
	LOAD_FUNC( mpv_initialize );
	LOAD_FUNC( mpv_destroy );
	LOAD_FUNC( mpv_terminate_destroy );
	LOAD_FUNC( mpv_create_client );
	LOAD_FUNC( mpv_create_weak_client );
	LOAD_FUNC( mpv_load_config_file );
	LOAD_FUNC( mpv_get_time_ns );
	LOAD_FUNC( mpv_get_time_us );
	LOAD_FUNC( mpv_free_node_contents );
	LOAD_FUNC( mpv_set_option );
	LOAD_FUNC( mpv_set_option_string );
	LOAD_FUNC( mpv_command );
	LOAD_FUNC( mpv_command_node );
	LOAD_FUNC( mpv_command_ret );
	LOAD_FUNC( mpv_command_string );
	LOAD_FUNC( mpv_command_async );
	LOAD_FUNC( mpv_command_node_async );
	LOAD_FUNC( mpv_abort_async_command );
	LOAD_FUNC( mpv_set_property );
	LOAD_FUNC( mpv_set_property_string );
	LOAD_FUNC( mpv_del_property );
	LOAD_FUNC( mpv_set_property_async );
	LOAD_FUNC( mpv_get_property );
	LOAD_FUNC( mpv_get_property_string );
	LOAD_FUNC( mpv_get_property_osd_string );
	LOAD_FUNC( mpv_get_property_async );
	LOAD_FUNC( mpv_observe_property );
	LOAD_FUNC( mpv_unobserve_property );
	LOAD_FUNC( mpv_event_name );
	LOAD_FUNC( mpv_event_to_node );
	LOAD_FUNC( mpv_request_event );
	LOAD_FUNC( mpv_request_log_messages );
	LOAD_FUNC( mpv_wait_event );
	LOAD_FUNC( mpv_wakeup );
	LOAD_FUNC( mpv_set_wakeup_callback );
	LOAD_FUNC( mpv_wait_async_requests );
	LOAD_FUNC( mpv_hook_add );
	LOAD_FUNC( mpv_hook_continue );
	LOAD_FUNC( mpv_get_wakeup_pipe );

	// render.h
	LOAD_FUNC( mpv_render_context_create );
	LOAD_FUNC( mpv_render_context_set_parameter );
	LOAD_FUNC( mpv_render_context_get_info );
	LOAD_FUNC( mpv_render_context_set_update_callback );
	LOAD_FUNC( mpv_render_context_update );
	LOAD_FUNC( mpv_render_context_render );
	LOAD_FUNC( mpv_render_context_report_swap );
	LOAD_FUNC( mpv_render_context_free );

	return true;
}


void unload_mpv_dll()
{
	if ( !g_mpv )
		return;

	p_mpv_destroy( g_mpv );
	g_mpv                    = nullptr;

	// clear mpv function pointers
	p_mpv_create             = nullptr;
	p_mpv_client_api_version = nullptr;

	sys_close_library( g_mpv_module );
}


void mpv_update_frame()
{
	if ( !g_mpv )
		return;

	u64 start_time = sys_get_time_ms();

	mpv_opengl_fbo fbo{ g_mpv_fbo, g_mpv_framebuffer_size[ 0 ], g_mpv_framebuffer_size[ 1 ], GL_RGB };
	int            yes  = 1;

	mpv_render_param rp[] = {
		{ MPV_RENDER_PARAM_OPENGL_FBO, &fbo },
		{ MPV_RENDER_PARAM_FLIP_Y, &yes },
		{ MPV_RENDER_PARAM_INVALID, NULL },
	};

	int ret = p_mpv_render_context_render( g_mpv_gl, rp );

	if ( ret != 0 )
	{
		printf("MPV Render Context Render Error: %d\n", ret );
	}

	g_mpv_redraw = false;

	u64 end_time = sys_get_time_ms();
	//printf( "UPDATE TIME: %.4f\n", (float)(end_time - start_time) / 1000.f );
}


void mpv_draw_frame()
{
	if ( !g_mpv )
		return;

	//if ( g_mpv_redraw )
	mpv_update_frame();

	// called so mpv doesn't get flooded with too many events, and becomes unresponsive
//	mpv_handle_wait_event( g_mpv, 0.01f );

	int width, height;
	SDL_GetWindowSize( app::window, &width, &height );

	//if ( width < g_mpv_framebuffer_size[ 0 ] / 2 )
	//	printf( "lol\n" );

	// width /= 2;
	// height /= 2;

	// width  = g_mpv_framebuffer_size[ 0 ];
	// height = g_mpv_framebuffer_size[ 1 ];

	p_mpv_get_property( g_mpv, "dwidth", MPV_FORMAT_INT64, &g_video_width );
	p_mpv_get_property( g_mpv, "dheight", MPV_FORMAT_INT64, &g_video_height );

	// Fit image in window size
	float factor[ 2 ] = { 1.f, 1.f };

	if ( g_scale_up_video || g_video_width > g_mpv_framebuffer_size[ 0 ] )
		factor[ 0 ] = (float)g_mpv_framebuffer_size[ 0 ] / (float)g_video_width;

	if ( g_scale_up_video || g_video_height > g_mpv_framebuffer_size[ 1 ] )
		factor[ 1 ] = (float)g_mpv_framebuffer_size[ 1 ] / (float)g_video_height;

	float zoom_level = std::min( factor[ 0 ], factor[ 1 ] );

	int   new_width  = g_video_width * zoom_level;
	int   new_height = g_video_height * zoom_level;

	int   pos_x      = g_mpv_framebuffer_size[ 0 ] / 2 - ( new_width / 2 );
	int   pos_y      = g_mpv_framebuffer_size[ 1 ] / 2 - ( new_height / 2 );

	// pos_x *= 2;

	//int   offset_x   = g_mpv_framebuffer_size[ 0 ] - new_width;
	//int   offset_y   = g_mpv_framebuffer_size[ 1 ] - new_height;

	//if ( app::window_resized )
	//	printf( "MPV RESIZE\n" );

	// glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, g_mpv_rbo );

	//glBindFramebuffer( GL_FRAMEBUFFER, 0 );
	//glBindFramebuffer( GL_READ_BUFFER, g_mpv_fbo );
	//
	//glBlitFramebuffer( 0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_LINEAR );

	// int       viewport_offset_x = ( g_mpv_framebuffer_size[ 0 ] - width ) / 1;
	// int       viewport_offset_y = ( g_mpv_framebuffer_size[ 1 ] - height ) / 1;

	int viewport_offset_x = ( g_mpv_framebuffer_size[ 0 ] - width ) / 2;
	int viewport_offset_y = ( g_mpv_framebuffer_size[ 1 ] - height ) / 2;

	int clamped_width     = g_mpv_framebuffer_size[ 0 ];
	int clamped_height    = g_mpv_framebuffer_size[ 1 ];

	// TODO: keep the full video in frame when live resizing the window
	// scale it up or down when needed
	//if ( width < new_width )
	//	printf( "TEST\n" );
	
	SDL_FRect dst_area{};
	dst_area.w = 1;
	dst_area.h = 1;
	dst_area.x = 0;
	dst_area.y = 0;
	
	SDL_FRect dst_rect{};
	dst_rect.w = width;
	dst_rect.h = height;
	dst_rect.x = 0;
	dst_rect.y = 0;
	
	if ( image_draw::flip_h )
	{
		dst_rect.w = -1;
		dst_rect.x = 1;
	}

	if ( image_draw::flip_v )
	{
		dst_rect.h = -1;
		dst_rect.y = 1;
	}
	
	glBindFramebuffer( GL_FRAMEBUFFER, 0 );
	//glBindRenderbuffer( GL_RENDERBUFFER, 0 );

	//if ( image_draw::flip_h )
	//	glViewport( pos_x + width, pos_y, -width, height );
	//else
	//	glViewport( pos_x, pos_y, width, height );

	//if ( image_draw::flip_h )
	//	glViewport( pos_x - offset_x, pos_y, width, height );
	//else
	//	glViewport( pos_x, pos_y, width, height );

	if ( image_draw::flip_h )
		glViewport( -viewport_offset_x, -viewport_offset_y, g_mpv_framebuffer_size[ 0 ], g_mpv_framebuffer_size[ 1 ] );
	else
		// glViewport( 0, 0, width, height );
		// glViewport( -viewport_offset_x, 0, clamped_width, clamped_height );
		glViewport( -viewport_offset_x, -viewport_offset_y, g_mpv_framebuffer_size[ 0 ], g_mpv_framebuffer_size[ 1 ] );

	glEnable( GL_SCISSOR_TEST );
	glScissor( pos_x - viewport_offset_x, pos_y - viewport_offset_y, new_width, new_height );

	glClearColor( app::clear_color.x, app::clear_color.y, app::clear_color.z, app::clear_color.w );
	glClear( GL_COLOR_BUFFER_BIT );

	glEnable( GL_TEXTURE_2D );
	glBindTexture( GL_TEXTURE_2D, g_mpv_fbo_tex );

	glMatrixMode( GL_PROJECTION );
	glLoadIdentity();
	// glOrtho( 0, 1, 0, 1, -1, 1 );
	glOrtho( 0, dst_rect.w, 0, dst_rect.h, -1, 1 );
	// glOrtho( 0, 1, 1, 0, -1, 1 );

	glMatrixMode( GL_MODELVIEW );
	glLoadIdentity();

	// get the center of the video
	// float image_center_x = ( ( pos_x + new_width ) / (float)width ) * 0.5f;
	// float image_center_y = ( ( pos_y + new_height ) / (float)height ) * 0.5f;
	
//	float image_center_x = ( pos_x + new_width ) * 0.5f;
//	float image_center_y = ( pos_y + new_height ) * 0.5f;
//
//	glTranslatef( image_center_x, image_center_y, 0.0f );    // move pivot to center of the image
//	glRotatef( image_draw::rot, 0, 0, 1 );                       // rotate around the image
//	glTranslatef( -image_center_x, -image_center_y, 0.0f );  // move back

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

	glDisable( GL_SCISSOR_TEST );
	glDisable( GL_TEXTURE_2D );
}


void mpv_update_texture()
{
	if ( !g_mpv )
		return;

	int width, height;
	SDL_GetWindowSize( app::window, &width, &height );

	glBindTexture( GL_TEXTURE_2D, g_mpv_fbo_tex );
	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr );

	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_mpv_fbo_tex, 0 );

	if ( glCheckFramebufferStatus( GL_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE )
		printf( "FBO incomplete!\n" );

	glBindTexture( GL_TEXTURE_2D, 0 );
	glBindFramebuffer( GL_FRAMEBUFFER, 0 );

	g_mpv_framebuffer_size[ 0 ] = width;
	g_mpv_framebuffer_size[ 1 ] = height;
}


void mpv_create_texture()
{
	if ( !g_mpv )
		return;

	//glDeleteTextures( 1, &g_mpv_fbo_tex );
	//g_mpv_fbo_tex = 0;

	int width, height;
	SDL_GetWindowSize( app::window, &width, &height );

	//glGenRenderbuffers( 1, &g_mpv_rbo );
	glGenTextures( 1, &g_mpv_fbo_tex );
	glBindTexture( GL_TEXTURE_2D, g_mpv_fbo_tex );

	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr );

	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );

	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_mpv_fbo_tex, 0 );

	if ( glCheckFramebufferStatus( GL_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE )
		printf( "FBO incomplete!\n" );

	glBindTexture( GL_TEXTURE_2D, 0 );
	glBindFramebuffer( GL_FRAMEBUFFER, 0 );

	g_mpv_framebuffer_size[ 0 ] = width;
	g_mpv_framebuffer_size[ 1 ] = height;
}


static void on_mpv_render_update( void* ctx )
{
	SDL_Event event = {.type = g_wakeup_on_mpv_render_update};
	SDL_PushEvent(&event);
}

static void on_mpv_events( void* ctx )
{
	SDL_Event event = {.type = g_wakeup_on_mpv_events};
	SDL_PushEvent(&event);
}


void mpv_sdl_event( SDL_Event& event )
{
#if 0
	if ( event.type == g_wakeup_on_mpv_render_update )
	{
		u64 flags = p_mpv_render_context_update( g_mpv_gl );
		if ( flags & MPV_RENDER_UPDATE_FRAME )
			g_mpv_redraw = true;
	}
	else if ( event.type == g_wakeup_on_mpv_events )
	{
		// Handle all remaining mpv events.
		while ( true )
		{
			mpv_event *mp_event = p_mpv_wait_event( g_mpv, 0 );

			if (mp_event->event_id == MPV_EVENT_NONE)
				break;

			if (mp_event->event_id == MPV_EVENT_LOG_MESSAGE)
			{
				mpv_event_log_message* msg = (mpv_event_log_message*)mp_event->data;
				// Print log messages about DR allocations, just to
				// test whether it works. If there is more than 1 of
				// these, it works. (The log message can actually change
				// any time, so it's possible this logging stops working
				// in the future.)
				if (strstr(msg->text, "DR image"))
					printf("MPV: %s", msg->text);
				continue;
			}

			printf("MPV EVENT: %s\n", p_mpv_event_name(mp_event->event_id));
		}
	}
#endif
}


static void* mpv_get_proc( void* ctx, const char* name )
{
	return ( void* )SDL_GL_GetProcAddress( name );
}


bool start_mpv()
{
	if ( !g_mpv_module )
		return false;

	// get the mpv version
	unsigned long mpv_version = p_mpv_client_api_version();
	printf( "mpv version: %lu\n", mpv_version );

	g_mpv = p_mpv_create();

	if ( g_mpv == nullptr )
	{
		printf( "mpv_create failed!\n" );
		return false;
	}

	// Disable VO
	int ret = p_mpv_set_option_string( g_mpv, "vo", "libmpv" );
	//p_mpv_set_option_string( g_mpv, "vo", "null" );

	p_mpv_set_option_string( g_mpv, "demuxer-max-bytes", "10M" );

	// Stops the main thread from being blocked somehow
	// https://github.com/celluloid-player/celluloid/pull/982
	p_mpv_set_option_string( g_mpv, "video-timing-offset", "0" );

	if ( p_mpv_initialize( g_mpv ) < 0 )
	{
		printf( "mpv_initialize failed!\n" );
		return false;
	}

	// p_mpv_request_log_messages( g_mpv, "debug" );
	//p_mpv_request_log_messages( g_mpv, "warn" );
	p_mpv_request_log_messages( g_mpv, "info" );

	// create render context
	mpv_opengl_init_params gl_init = {
		.get_proc_address = mpv_get_proc,  // e.g. SDL_GL_GetProcAddress
	};

	int enabled = 1;

	mpv_render_param params[] = {
		{ MPV_RENDER_PARAM_API_TYPE, (void*)MPV_RENDER_API_TYPE_OPENGL },
		{ MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &gl_init },
		{ MPV_RENDER_PARAM_INVALID, NULL },
	};

	p_mpv_render_context_create( &g_mpv_gl, g_mpv, params );

	// We use events for thread-safe notification of the SDL main loop.
	// Generally, the wakeup callbacks (set further below) should do as least
	// work as possible, and merely wake up another thread to do actual work.
	// On SDL, waking up the mainloop is the ideal course of action. SDL's
	// SDL_PushEvent() is thread-safe, so we use that.
	//g_wakeup_on_mpv_render_update = SDL_RegisterEvents(1);
	//g_wakeup_on_mpv_events = SDL_RegisterEvents(1);
//
	//if (g_wakeup_on_mpv_render_update == (u32)-1 || g_wakeup_on_mpv_events == (u32)-1)
	//{
	//	printf( "Failed to Regsiter SDL Events for Video Player!\n" );
	//	return false;
	//}

	// When normal mpv events are available.
	p_mpv_set_wakeup_callback( g_mpv, on_mpv_events, nullptr );

	// When there is a need to call mpv_render_context_update(), which can
	// request a new frame to be rendered.
	// (Separate from the normal event handling mechanism for the sake of
	//  users which run OpenGL on a different thread.)
	//p_mpv_render_context_set_update_callback( g_mpv_gl, on_mpv_render_update, nullptr );

	//int64_t wid = (s64)g_mpv_window;
	//bool    yes = true;
	//
	//// attach to main window
	//p_mpv_set_property( g_mpv, "wid", MPV_FORMAT_INT64, &wid );
	//p_mpv_set_property( g_mpv, "keep-open", MPV_FORMAT_FLAG, &yes );

	int width, height;
	SDL_GetWindowSize( app::window, &width, &height );

	// Create Framebuffer to draw on
	glGenFramebuffers( 1, &g_mpv_fbo );
	glBindFramebuffer( GL_FRAMEBUFFER, g_mpv_fbo );

	mpv_create_texture();

	p_mpv_set_property_string( g_mpv, "keep-open", "always" );
	p_mpv_set_property_string( g_mpv, "loop", "inf" );

	// let mpv finish startup
	mpv_handle_wait_event( g_mpv, 0.1 );

	// Load supported extensions
	char* exts = p_mpv_get_property_string( g_mpv, "video-exts" );

	if ( !exts )
	{
		printf( "no supported extensions from mpv??\n" );
		return false;
	}

	char* ext_cur  = exts; 
	char* ext_next = strchr( ext_cur, ',' );

	while ( ext_next != nullptr )
	{
		std::string ext = ".";
		ext.append( ext_cur, ext_next - ext_cur );
		g_mpv_exts.push_back( ext );

		ext_cur  = ext_next + 1;
		ext_next = strchr( ext_cur, ',' );
	}

	return true;
}


void stop_mpv()
{
}


bool mpv_supports_ext( std::string_view ext )
{
	for ( const std::string& supported_ext : g_mpv_exts )
		if ( ext.ends_with( supported_ext ) )
			return true;

	return false;
}


void mpv_window_resize()
{
	mpv_update_texture();
}


char* mpv_get_current_video()
{
	return g_current_video;
}


void mpv_handle_wait_event( mpv_handle* mpv, double timeout, const char* prefix )
{
	if ( !mpv )
		return;

	mpv_event* event = p_mpv_wait_event( mpv, timeout );

	if ( !event )
		return;

	while ( event->event_id != MPV_EVENT_NONE )
	{
		if ( event->event_id == MPV_EVENT_LOG_MESSAGE )
		{
			struct mpv_event_log_message* msg = (struct mpv_event_log_message*)event->data;

			if ( prefix )
			{
				printf( "%s: [%s] %s: %s", prefix, msg->prefix, msg->level, msg->text );
			}
			else
			{
				printf( "MPV: [%s] %s: %s", msg->prefix, msg->level, msg->text );
			}
		}

		event = p_mpv_wait_event( mpv, timeout );
	}
}


void mpv_handle_wait_event_2( mpv_handle* mpv, double timeout, const char* prefix )
{
	if ( !mpv )
		return;

	mpv_event* event = p_mpv_wait_event( mpv, timeout );

	if ( !event )
		return;

	while ( event->event_id != MPV_EVENT_NONE )
	{
		if ( event->event_id == MPV_EVENT_LOG_MESSAGE )
		{
			struct mpv_event_log_message* msg = (struct mpv_event_log_message*)event->data;

			if ( prefix )
			{
				printf( "%s: [%s] %s: %s", prefix, msg->prefix, msg->level, msg->text );
			}
			else
			{
				printf( "MPV: [%s] %s: %s", msg->prefix, msg->level, msg->text );
			}
		}

		event = p_mpv_wait_event( mpv, timeout );
	}
}


// ----------------------------------------------------


void mpv_cmd_set_video_zoom( float zoom )
{
	if ( !g_mpv )
		return;

	// convert float to string
	char zoom_str[ 16 ];
	gcvt( zoom, 4, zoom_str );

	const char* cmd[]   = { "set", "video-zoom", zoom_str, nullptr };
	int         cmd_ret = p_mpv_command_async( g_mpv, 0, cmd );
}


void mpv_cmd_add_video_zoom( float zoom )
{
	if ( !g_mpv )
		return;

	// convert float to string
	char zoom_str[ 16 ];
	gcvt( zoom, 4, zoom_str );

	const char* cmd[]   = { "set", "video-zoom", zoom_str, nullptr };
	int         cmd_ret = p_mpv_command_async( g_mpv, 0, cmd );
}


void mpv_cmd_loadfile( const char* file )
{
	if ( !g_mpv )
		return;

	printf( "loading file: %s\n", file );

	const char* cmd[]   = { "loadfile", file, NULL };
	int         cmd_ret = p_mpv_command( g_mpv, cmd );

	// mpv_event*  event   = p_mpv_wait_event( g_mpv, 0.1f );
	mpv_handle_wait_event( g_mpv, 0.1f );

	if ( g_current_video != nullptr )
		ch_free_str( g_current_video );

	g_current_video = util_strdup( file );

	// or use video-params?
//	p_mpv_get_property( g_mpv, "width", MPV_FORMAT_INT64, &g_video_width );
//	p_mpv_get_property( g_mpv, "height", MPV_FORMAT_INT64, &g_video_height );
//
//	p_mpv_get_property( g_mpv, "height", MPV_FORMAT_INT64, &g_video_height );
}


void mpv_cmd_close_video()
{
	if ( !g_mpv )
		return;

	const char* cmd[]   = { "stop", NULL };
	int         cmd_ret = p_mpv_command_async( g_mpv, NULL, cmd );
}


void mpv_cmd_toggle_playback()
{
	if ( !g_mpv )
		return;

	const char* cmd[]   = { "cycle", "pause", NULL };
	int         cmd_ret = p_mpv_command_async( g_mpv, 0, cmd );
}


void mpv_cmd_seek_offset( double seconds )
{
	if ( !g_mpv )
		return;

	double duration = 0;
	double time_pos = 0;
	p_mpv_get_property( g_mpv, "duration", MPV_FORMAT_DOUBLE, &duration );
	p_mpv_get_property( g_mpv, "time-pos", MPV_FORMAT_DOUBLE, &time_pos );

	double new_time = time_pos + seconds;
	new_time        = std::max( 0.0, std::min( duration, new_time ) );

	char time_pos_str[ 32 ];
	gcvt( new_time, 4, time_pos_str );

	const char* cmd[]   = { "seek", time_pos_str, "absolute", NULL };
	int         cmd_ret = p_mpv_command_async( g_mpv, 0, cmd );
}


void mpv_cmd_hook_window( void* window )
{
	if ( !g_mpv )
		return;

	int64_t wid = (s64)window;
	int ret = p_mpv_set_property( g_mpv, "wid", MPV_FORMAT_INT64, &wid );

	printf( "hooked window idk\n" );
}


void mpv_cmd_hook_window_mpv()
{
	//int64_t wid = (s64)g_mpv_window;
	//int     ret = p_mpv_set_property( g_mpv, "wid", MPV_FORMAT_INT64, &wid );
	//printf( "hooked mpv window idk\n" );
}


void mpv_handle_error( int )
{
	//p_mpv_error_string()
}

