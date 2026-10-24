#include "main.h"
#include "util.h"

// some reference here
// https://github.com/mpv-player/mpv-examples/blob/master/libmpv/sdl/main.c

SDL_SharedObject*                 g_mpv_module  = nullptr;
mpv_handle*                       g_mpv         = nullptr;
mpv_render_context*               g_mpv_gl      = nullptr;
GLuint                            g_mpv_fbo     = 0;
GLuint                            g_mpv_fbo_tex = 0;
GLuint                            g_mpv_rbo     = 0;
static ivec2                      g_mpv_framebuffer_size{};

mpv_data_t                        g_mpv_data;

static bool                       g_mpv_redraw    = false;

static char*                      g_current_video = nullptr;

//s64                               g_mpv_data.dwidth = 0, g_mpv_data.dheight = 0;

bool                              g_scale_up_video             = true;
bool                              g_mpv_video_ready            = false;
bool                              g_mpv_event_sending          = false;

bool                              g_mpv_texture_updated_needed = false;


static std::vector< std::string > g_mpv_exts;


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
	p_##func = (func##_t)SDL_LoadFunction( g_mpv_module, #func ); \
	if ( p_##func == nullptr )                                 \
	{                                                          \
		char* sys_error = sys_get_error();                     \
		printf( "sys_load_func failed: %s\n", sys_error );     \
		ch_free_str( sys_error );                              \
		return false;                                          \
	}


static void* mpv_get_proc( void* ctx, const char* name )
{
	return (void*)SDL_GL_GetProcAddress( name );
}


bool load_mpv_dll()
{
	if ( app::config.no_video )
		return false;

#if 1
  #if _WIN32
	g_mpv_module = SDL_LoadObject( "libmpv-2.dll" );
  #else
	g_mpv_module = SDL_LoadObject( "libmpv.so" );
  #endif
#else
  #if _WIN32
	g_mpv_module = sys_load_library( L"libmpv-2.dll" );
  #else
	g_mpv_module = sys_load_library( "libmpv.so" );
  #endif
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


void mpv_create_texture();


static void on_mpv_render_update( void* ctx )
{
	//g_mpv_event_sending = true;
	send_user_event( e_user_event_mpv_render, nullptr, nullptr );
	//g_mpv_event_sending = false;
}


static void on_mpv_events( void* ctx )
{
	g_mpv_event_sending = true;
	send_user_event( e_user_event_mpv_event, nullptr, nullptr );
	g_mpv_event_sending = false;
}


bool mpv_init_render_context()
{
	// create render context
	mpv_opengl_init_params gl_init = {
		.get_proc_address = mpv_get_proc,  // e.g. SDL_GL_GetProcAddress
	};

	int              enabled  = 1;

	mpv_render_param params[] = {
		{ MPV_RENDER_PARAM_API_TYPE, (void*)MPV_RENDER_API_TYPE_OPENGL },
		{ MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &gl_init },
		{ MPV_RENDER_PARAM_INVALID, NULL },
	};

	//	 * @return error code, including but not limited to:
	//*      MPV_ERROR_UNSUPPORTED: the OpenGL version is not supported
	//*                             (or required extensions are missing)
	//*      MPV_ERROR_NOT_IMPLEMENTED: an unknown API type was provided, or
	//*                                 support for the requested API was not
	//*                                 built in the used libmpv binary.
	//*      MPV_ERROR_INVALID_PARAMETER: at least one of the provided parameters was
	//*                                   not valid.

	mpv_error ret = (mpv_error)p_mpv_render_context_create( &g_mpv_gl, g_mpv, params );

	if ( ret != MPV_ERROR_SUCCESS )
	{
		if ( ret == MPV_ERROR_UNSUPPORTED )
			printf( "Failed to Create MPV Render Context: MPV_ERROR_UNSUPPORTED" );
		else if ( ret == MPV_ERROR_NOT_IMPLEMENTED )
			printf( "Failed to Create MPV Render Context: MPV_ERROR_NOT_IMPLEMENTED" );
		else if ( ret == MPV_ERROR_INVALID_PARAMETER )
			printf( "Failed to Create MPV Render Context: MPV_ERROR_INVALID_PARAMETER" );

		return false;
	}

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

	g_mpv_data.gl = g_mpv_gl;

	mpv_create_texture();
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

	SDL_UnloadObject( g_mpv_module );
}


void mpv_update_frame()
{
	if ( !g_mpv )
		return;

	if ( !g_mpv_gl )
		mpv_init_render_context();

	//if ( !g_mpv_redraw )
	//	return;

	//mpv_opengl_fbo fbo{ static_cast< int >( g_mpv_fbo ), g_mpv_framebuffer_size[ 0 ], g_mpv_framebuffer_size[ 1 ], GL_RGBA16 };
	mpv_opengl_fbo fbo{ static_cast< int >( g_mpv_fbo ), g_mpv_framebuffer_size[ 0 ], g_mpv_framebuffer_size[ 1 ], GL_RGB };
	int            yes  = 1;

	mpv_render_param rp[] = {
		{ MPV_RENDER_PARAM_OPENGL_FBO, &fbo },
		//{ MPV_RENDER_PARAM_FLIP_Y, &yes },
		{ MPV_RENDER_PARAM_INVALID, NULL },
	};

	int ret = p_mpv_render_context_render( g_mpv_gl, rp );

	if ( ret != 0 )
	{
		printf("MPV Render Context Render Error: %d\n", ret );
	}

	g_mpv_redraw = false;
}


// IDEA: maybe for watching a video, you want to zoom into a video as if it was an image
// maybe integrate some of the panning and zoom controls into the video view
void mpv_draw_frame()
{
	if ( !g_mpv )
		return;

//	if ( !g_mpv_data.loaded_file )
//		return;

	mpv_update_frame();

	int width, height;
	SDL_GetWindowSize( app::window, &width, &height );

//	p_mpv_get_property( g_mpv, "width", MPV_FORMAT_INT64, &g_mpv_data.dwidth );
//	p_mpv_get_property( g_mpv, "height", MPV_FORMAT_INT64, &g_mpv_data.dheight );

	if ( g_mpv_data.dwidth == 0 || g_mpv_data.dheight == 0 )
		return;

	// Fit image in window size
	float factor[ 2 ] = { 1.f, 1.f };

	ivec2 area_size{ width, height };

	// printf( "FRAMEBUFFER %dx%d\n", g_mpv_framebuffer_size[ 0 ], g_mpv_framebuffer_size[ 1 ] );

	if ( g_scale_up_video || g_mpv_data.dwidth > area_size[ 0 ] )
		factor[ 0 ] = (float)area_size[ 0 ] / (float)g_mpv_data.dwidth;

	if ( g_scale_up_video || g_mpv_data.dheight > area_size[ 1 ] )
		factor[ 1 ] = (float)area_size[ 1 ] / (float)g_mpv_data.dheight;

	float     zoom_level        = std::min( factor[ 0 ], factor[ 1 ] );

	float     new_width         = static_cast< float >( g_mpv_data.dwidth ) * zoom_level;
	float     new_height        = static_cast< float >( g_mpv_data.dheight ) * zoom_level;

	float     pos_x             = static_cast< float >( area_size[ 0 ] ) / 2.f - ( new_width / 2.f );
	float     pos_y             = static_cast< float >( area_size[ 1 ] ) / 2.f - ( new_height / 2.f );

	int                   viewport_offset_x = ( area_size[ 0 ] - width ) / 2;
	int                   viewport_offset_y = ( area_size[ 1 ] - height ) / 2;

	int       clamped_width     = g_mpv_framebuffer_size[ 0 ];
	int       clamped_height    = g_mpv_framebuffer_size[ 1 ];

	render_draw_texture_t draw_info{};
	draw_info.width    = static_cast< float >( new_width );
	draw_info.height   = static_cast< float >( new_height );
	draw_info.x        = pos_x;
	draw_info.y        = pos_y;
	draw_info.rotation = 0;
	draw_info.texture  = g_mpv_fbo_tex;

	glEnable( GL_SCISSOR_TEST );
	glScissor( pos_x - viewport_offset_x, pos_y - viewport_offset_y, new_width, new_height );

	render_draw_texture( draw_info );

	glDisable( GL_SCISSOR_TEST );
}


void mpv_update_texture()
{
	if ( !g_mpv )
		return;

	if ( !g_mpv_gl )
	{
		mpv_init_render_context();
		return;
	}

	int width, height;
	SDL_GetWindowSize( app::window, &width, &height );

	float new_width  = width;
	float new_height = height;

	if ( g_mpv_data.dwidth != 0 && g_mpv_data.dheight != 0 )
	{
		// Fit image in window size
		float factor[ 2 ] = { 1.f, 1.f };

		ivec2 area_size{ width, height };

		// printf( "FRAMEBUFFER %dx%d\n", g_mpv_framebuffer_size[ 0 ], g_mpv_framebuffer_size[ 1 ] );

		if ( g_scale_up_video || g_mpv_data.dwidth > area_size[ 0 ] )
			factor[ 0 ] = (float)area_size[ 0 ] / (float)g_mpv_data.dwidth;

		if ( g_scale_up_video || g_mpv_data.dheight > area_size[ 1 ] )
			factor[ 1 ] = (float)area_size[ 1 ] / (float)g_mpv_data.dheight;

		float zoom_level = std::min( factor[ 0 ], factor[ 1 ] );

		new_width  = static_cast< float >( g_mpv_data.dwidth ) * zoom_level;
		new_height = static_cast< float >( g_mpv_data.dheight ) * zoom_level;
	}

	glBindFramebuffer( GL_FRAMEBUFFER, g_mpv_fbo );
	glBindTexture( GL_TEXTURE_2D, g_mpv_fbo_tex );
	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGB, new_width, new_height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr );
	//glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA16, width, height, 0, GL_RGBA16, GL_UNSIGNED_INT_10_10_10_2, nullptr );

	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_mpv_fbo_tex, 0 );

	if ( glCheckFramebufferStatus( GL_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE )
		printf( "FBO incomplete!\n" );

	glBindTexture( GL_TEXTURE_2D, 0 );
	glBindFramebuffer( GL_FRAMEBUFFER, 0 );

	g_mpv_framebuffer_size[ 0 ] = new_width;
	g_mpv_framebuffer_size[ 1 ] = new_height;

	set_frame_draw( 2 );
}


void mpv_create_texture()
{
	if ( !g_mpv )
		return;

	glGenTextures( 1, &g_mpv_fbo_tex );

//	mpv_update_texture();
#if 0
	//glDeleteTextures( 1, &g_mpv_fbo_tex );
	//g_mpv_fbo_tex = 0;

	int width, height;
	SDL_GetWindowSize( app::window, &width, &height );

	//glGenRenderbuffers( 1, &g_mpv_rbo );
	glBindTexture( GL_TEXTURE_2D, g_mpv_fbo_tex );

	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr );
	//glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_INT_10_10_10_2, nullptr );

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
#endif
}


void mpv_sdl_event( SDL_Event& sdl_event )
{
#if 1
	if ( sdl_event.user.code == e_user_event_mpv_render )
	{
		u64 flags = p_mpv_render_context_update( g_mpv_gl );
		if ( flags & MPV_RENDER_UPDATE_FRAME )
			g_mpv_redraw = true;

		// mpv_update();
	}
#endif
#if 1
	if ( sdl_event.user.code == e_user_event_mpv_event )
	{
		if ( g_mpv_event_sending )
			return;

		mpv_update();
	}
#endif
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

	// disabled for 10-bit testing
	p_mpv_set_option_string( g_mpv, "dither-depth", "no" );

	//p_mpv_set_option_string( g_mpv, "cache-secs", "60" );
	//p_mpv_set_option_string( g_mpv, "cache-pause-initial", "no" );

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

	p_mpv_set_property_string( g_mpv, "keep-open", "always" );
	p_mpv_set_property_string( g_mpv, "loop", "inf" );

	int observe_ret = p_mpv_observe_property( g_mpv, e_mpv_cmd_observe, "time-pos/full", MPV_FORMAT_DOUBLE );
	observe_ret     = p_mpv_observe_property( g_mpv, e_mpv_cmd_observe, "duration/full", MPV_FORMAT_DOUBLE );
	observe_ret     = p_mpv_observe_property( g_mpv, e_mpv_cmd_observe, "pause", MPV_FORMAT_FLAG );
	observe_ret     = p_mpv_observe_property( g_mpv, e_mpv_cmd_observe, "mute", MPV_FORMAT_FLAG );

	observe_ret     = p_mpv_observe_property( g_mpv, e_mpv_cmd_observe, "dwidth", MPV_FORMAT_INT64 );
	observe_ret     = p_mpv_observe_property( g_mpv, e_mpv_cmd_observe, "dheight", MPV_FORMAT_INT64 );

	observe_ret     = p_mpv_observe_property( g_mpv, e_mpv_cmd_observe, "volume", MPV_FORMAT_DOUBLE );

	observe_ret     = p_mpv_observe_property( g_mpv, e_mpv_cmd_observe, "audio", MPV_FORMAT_STRING );
	observe_ret     = p_mpv_observe_property( g_mpv, e_mpv_cmd_observe, "current-tracks/audio/title", MPV_FORMAT_STRING );

	p_mpv_observe_property( g_mpv, e_mpv_cmd_observe, "track-list/count", MPV_FORMAT_INT64 );

	// When normal mpv events are available.
	p_mpv_set_wakeup_callback( g_mpv, on_mpv_events, nullptr );

	// let mpv finish startup
	// mpv_handle_wait_event( g_mpv, 0.1 );

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

	g_mpv_data.mpv = g_mpv;
	g_mpv_data.gl  = g_mpv_gl;

	return true;
}


void stop_mpv()
{
	if ( !g_mpv )
		return;

	mpv_cmd_close_video();

	mpv_update();

	mpv_event* event = p_mpv_wait_event( g_mpv, 0 );

	while ( event->event_id != MPV_EVENT_NONE )
	{
		event = p_mpv_wait_event( g_mpv, 0 );
	}

	// causes a crash?
	//p_mpv_destroy( g_mpv );
	//g_mpv = nullptr;
}


bool mpv_supports_ext( std::string_view ext )
{
	for ( const std::string& supported_ext : g_mpv_exts )
		if ( ext.ends_with( supported_ext ) )
			return true;

	return false;
}



// ----------------------------------------------------


template< typename T >
T get_mpv_value( void* data, T fallback )
{
	if ( data == nullptr )
		return fallback;
	else
		return *(T*)data;
}


char* get_mpv_string( char*& old, mpv_event_property* property, char* fallback )
{
	if ( old )
		free( old );

	old = nullptr;

	if ( property->format != MPV_FORMAT_STRING )
		return fallback;

	if ( property->data == nullptr )
		return fallback;

	// do you not free async string returns? can't find info on it in documentation
	// this crashes the program if i do this, either on first call, or a little later
	// p_mpv_free( temp_str );

	return util_strdup( *(char**)property->data );
}


void get_media_info( mpv_data_t& mpv )
{
	if ( !g_current_video )
		return;

	//mpv.track_count       = 0;
	mpv.track_count_audio = 0;
	mpv.track_count_video = 0;

	//p_mpv_get_property_async( mpv.mpv, "track-list/count", MPV_FORMAT_INT64, &mpv.track_count );

	// mpv_error ret          = (mpv_error)p_mpv_get_property( mpv.mpv, "track-list/count", MPV_FORMAT_INT64, &mpv.track_count );
	//
	for ( s32 i = 0; i < mpv.track_count; i++ )
	{
		char cmd[ 64 ] = { 0 };
		snprintf( cmd, 64, "track-list/%d/type", i );

		//char* type = nullptr;
		p_mpv_get_property_async( mpv.mpv, e_mpv_cmd_track_type, cmd, MPV_FORMAT_STRING );

		//if ( !type )
		//	continue;
		//
		//if ( strcmp( type, "video" ) == 0 )
		//	mpv.track_count_video++;
		//
		//else if ( strcmp( type, "audio" ) == 0 )
		//	mpv.track_count_audio++;
	}
}


void mpv_update( mpv_data_t& data )
{
	if ( !data.mpv )
		return;

	mpv_event* mpv_event = p_mpv_wait_event( data.mpv, 0 );

	while ( mpv_event && mpv_event->event_id != MPV_EVENT_NONE )
	{
		if ( mpv_event->event_id == MPV_EVENT_NONE )
			return;

		if ( mpv_event->event_id == MPV_EVENT_LOG_MESSAGE )
		{
			struct mpv_event_log_message* msg = (struct mpv_event_log_message*)mpv_event->data;
			printf( "MPV: [%s] %s: %s", msg->prefix, msg->level, msg->text );
		}
		else if ( mpv_event->event_id == MPV_EVENT_PROPERTY_CHANGE )
		{
			struct mpv_event_property* property = (struct mpv_event_property*)mpv_event->data;

			if ( property->name )
			{
				if ( strcmp( "time-pos/full", property->name ) == 0 )
				{
					// what the fuck is this?
					// if ( property->data == nullptr )
					// 	data.time_pos = 0.0;
					// else
					// 	data.time_pos = *(double*)property->data;
					data.time_pos = get_mpv_value( property->data, 0.0 );
				}
				else if ( strcmp( "duration/full", property->name ) == 0 )
				{
					data.duration = get_mpv_value( property->data, 0.0 );
				}
				else if ( strcmp( "pause", property->name ) == 0 )
				{
					data.pause = get_mpv_value< s32 >( property->data, 0 );
				}
				else if ( strcmp( "mute", property->name ) == 0 )
				{
					data.mute = get_mpv_value< s32 >( property->data, 0 );
				}
				else if ( strcmp( "dwidth", property->name ) == 0 )
				{
					data.dwidth = get_mpv_value< s64 >( property->data, 0 );
					mpv_update_texture();
				}
				else if ( strcmp( "dheight", property->name ) == 0 )
				{
					data.dheight = get_mpv_value< s64 >( property->data, 0 );
					mpv_update_texture();
				}
				else if ( strcmp( "volume", property->name ) == 0 )
				{
					data.volume = get_mpv_value< double >( property->data, 0 );
				}
				else if ( strcmp( "audio", property->name ) == 0 )
				{
					data.audio_track = get_mpv_string( data.audio_track, property, nullptr );
				}
				else if ( strcmp( "current-tracks/audio/title", property->name ) == 0 )
				{
					data.audio_track_title = get_mpv_string( data.audio_track_title, property, nullptr );
				}
				else if ( strcmp( "track-list/count", property->name ) == 0 )
				{
					data.track_count = get_mpv_value< s64 >( property->data, 0 );
					get_media_info( data );
				}
			}
		}
		else if ( mpv_event->event_id == MPV_EVENT_GET_PROPERTY_REPLY )
		{
			struct mpv_event_property* property = (struct mpv_event_property*)mpv_event->data;

			if ( mpv_event->reply_userdata == e_mpv_cmd_track_type )
			{
				char* track_type = get_mpv_string( data.audio_track_title, property, nullptr );

				if ( !track_type )
					continue;

				if ( strcmp( track_type, "video" ) == 0 )
					data.track_count_video++;

				else if ( strcmp( track_type, "audio" ) == 0 )
					data.track_count_audio++;

				free( track_type );
			}

			if ( property->name )
			{
				//if ( strcmp( "audio", property->name ) == 0 )
				//{
				//	data.audio_track = get_mpv_value< char* >( property->data, nullptr );
				//}
				//if ( strcmp( "current-tracks/audio/title", property->name ) == 0 )
				//{
				//	//if ( data.audio_track_title )
				//	//	p_mpv_free( data.audio_track_title );
				//
				//	//data.audio_track_title = get_mpv_value< char* >( property->data, nullptr );
				//}

				//if ( strncmp( "track-list/", property->name, 11 ) == 0 )
				//{
				//	char* track_type = get_mpv_string( data.audio_track_title, property, nullptr );
				//}
			}
		}
		else if ( mpv_event->event_id == MPV_EVENT_PLAYBACK_RESTART )
		{
			data.loaded_file = true;
			// get_media_info( data );

			// this may be called on new video creation to set to specific time
			// p_mpv_set_option_string( data.mpv, "start", "0" );

			if ( g_media_load_index < UINT32_MAX )
				media_load_on_finish();

			send_frame_draw_event();
		}
		else if ( mpv_event->event_id == MPV_EVENT_COMMAND_REPLY )
		{
			struct mpv_event_command* cmd_reply = (struct mpv_event_command*)mpv_event->data;

			if ( mpv_event->reply_userdata == e_mpv_cmd_loadfile )
			{
				if ( mpv_event->error != 0 )
				{
					free( g_current_video );
					g_current_video  = nullptr;
					data.loaded_file   = false;
				}
				else
				{
					char volume_str[ 16 ];
					snprintf( volume_str, 16, "%.4f", image_draw::volume );

					const char* cmd[]   = { "set", "volume", volume_str, NULL };
					int         cmd_ret = p_mpv_command_async( data.mpv, 0, cmd );
				}
			}
			else if ( mpv_event->reply_userdata == e_mpv_cmd_seek )
			{
				//printf( "FINISH SEEK - %u\n", sys_get_time_ms(), data.seek_queued_time );
				// if ( data.seek_queued )
				data.seek_queued = false;
			}
		}

		mpv_event = p_mpv_wait_event( data.mpv, 0 );
	}
}


void mpv_update()
{
	mpv_update( g_mpv_data );
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
		if ( event->event_id == MPV_EVENT_PLAYBACK_RESTART )
		{
			// Video Loaded
			send_frame_draw_event();
			g_mpv_video_ready = true;
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
	ch_free_str( g_current_video );

	if ( !g_mpv )
		return;

	if ( !g_mpv_gl )
		mpv_init_render_context();

	printf( "MPV: Loading File: %s\n", file );

	g_mpv_texture_updated_needed = true;
	g_mpv_data.loaded_file       = false;

	const char* cmd[]            = { "loadfile", file, NULL };
	int         cmd_ret          = p_mpv_command_async( g_mpv, e_mpv_cmd_loadfile, cmd );

	g_mpv_video_ready   = false;

	// mpv_event*  event   = p_mpv_wait_event( g_mpv, 0.1f );
	// mpv_handle_wait_event( g_mpv, 0.1f );

	g_current_video = util_strdup( file );

	set_frame_draw();

	// mpv_update_texture();

	// or use video-params?
//	p_mpv_get_property( g_mpv, "width", MPV_FORMAT_INT64, &g_mpv_data.dwidth );
//	p_mpv_get_property( g_mpv, "height", MPV_FORMAT_INT64, &g_mpv_data.dheight );
//
//	p_mpv_get_property( g_mpv, "height", MPV_FORMAT_INT64, &g_mpv_data.dheight );
}


void mpv_cmd_close_video()
{
	if ( !g_mpv )
		return;

	const char* cmd[]   = { "stop", NULL };
	int         cmd_ret = p_mpv_command_async( g_mpv, NULL, cmd );

	ch_free_str( g_current_video );
	g_current_video   = nullptr;

	g_mpv_video_ready = false;
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

	// wait for current seek to finish
	if ( g_mpv_data.seek_queued )
		return;

	g_mpv_data.seek_queued      = true;
	g_mpv_data.seek_queued_time = sys_get_time_ms();

	double new_time = g_mpv_data.time_pos + seconds;
	new_time        = std::max( 0.0, std::min( g_mpv_data.duration, new_time ) );

	char time_pos_str[ 16 ];
	snprintf( time_pos_str, 16, "%.4f", seconds );

	const char* cmd[]   = { "seek", time_pos_str, "absolute", NULL };
	int         cmd_ret = p_mpv_command_async( g_mpv, e_mpv_cmd_seek, cmd );
}

