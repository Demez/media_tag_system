#include "main.h"

#include "imgui_impl_sdl3.h"
#include "imgui_impl_opengl3.h"


extern bool          g_in_draw;
extern SDL_GLContext g_gl_context;

constexpr ImVec2     DEFAULT_WINDOW_SIZE( 1000, 600 );

// ================================================================================================


void frame_draw_start()
{
	g_in_draw = true;

	int width, height;
	SDL_GetWindowSize( app::window, &width, &height );

	ImGui::GetIO().DisplaySize.x = static_cast< float >( width );
	ImGui::GetIO().DisplaySize.y = static_cast< float >( height );

	ImGui_ImplSDL3_NewFrame();
	ImGui_ImplOpenGL3_NewFrame();
	ImGui::NewFrame();

	// reset framebuffer
	glBindFramebuffer( GL_FRAMEBUFFER, 0 );

	glViewport( 0, 0, width, height );

	if ( g_gallery_view )
		glClearColor( app::config.header_bg_color.x, app::config.header_bg_color.y, app::config.header_bg_color.z, app::config.header_bg_color.w );
	else
		glClearColor( app::config.media_bg_color.x, app::config.media_bg_color.y, app::config.media_bg_color.z, app::config.media_bg_color.w );

	glClear( GL_COLOR_BUFFER_BIT );
}


void frame_draw_end()
{
	if ( !g_gallery_view )
		media_view_draw();

	// reset framebuffer
	glBindFramebuffer( GL_FRAMEBUFFER, 0 );

	ImGui_ImplOpenGL3_RenderDrawData( ImGui::GetDrawData() );
	SDL_GL_SwapWindow( app::window );

	if ( g_mpv && g_mpv_gl )
		p_mpv_render_context_report_swap( g_mpv_gl );

	g_in_draw = false;
}


// called initially on startup and on window resize
void window_quick_draw()
{
	if ( g_in_draw )
		return;

	g_in_draw = true;

	set_frame_draw();

	frame_draw_start();

	imgui_draw( true );

	media_view_update();

	if ( app::window_resized )
	{
		media_view_window_resize();
		gallery_view_scroll_to_cursor();
		// mpv_window_resize();
	}

	frame_draw_end();

	set_frame_draw();

	g_in_draw = false;
}


// ================================================================================================
// Window Handling, pretty basic


bool render_window_prepare_for_creation()
{
	if ( !SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE ) )
		return false;

	if ( !SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, 4 ) )
		return false;

	if ( !SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, 3 ) )
		return false;

	// 10-bit testing...
	if ( app::config.high_bpc )
	{
		SDL_GL_SetAttribute( SDL_GL_RED_SIZE, 10 );
		SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE, 10 );
		SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE, 10 );
		SDL_GL_SetAttribute( SDL_GL_ALPHA_SIZE, 2 );

		// this does work, i imagine i need this for HDR later, but i need to adjust how things are rendered
		// SDL_GL_SetAttribute( SDL_GL_RED_SIZE, 16 );
		// SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE, 16 );
		// SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE, 16 );
		// SDL_GL_SetAttribute( SDL_GL_ALPHA_SIZE, 16 );

		// SDL_GL_SetAttribute( SDL_GL_FLOATBUFFERS, 1 );
	}

	return true;
}


bool render_window_test()
{
	g_gl_context = SDL_GL_CreateContext( app::window );

	if ( !g_gl_context )
	{
		printf( "Failed to create GL Context\n" );
		return false;
	}

	SDL_GL_MakeCurrent( app::window, g_gl_context );

	// make sure we don't get set to a bit depth of 0, this can happen with hdmi displays that aren't 2.1+, ugh
	int r = 0;
	SDL_GL_GetAttribute( SDL_GL_RED_SIZE, &r );
	return ( r != 0 );
}


void render_window_set_fallbacks()
{
	SDL_GL_SetAttribute( SDL_GL_RED_SIZE, 8 );
	SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE, 8 );
	SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE, 8 );
	SDL_GL_SetAttribute( SDL_GL_ALPHA_SIZE, 8 );
}


bool render_window_create()
{
	if ( !render_window_prepare_for_creation() )
	{
		printf( "Failed to set OpenGL attributes\n" );
		return false;
	}

	// Create Window
	SDL_PropertiesID props = SDL_CreateProperties();

	SDL_SetNumberProperty( props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, DEFAULT_WINDOW_SIZE.x );
	SDL_SetNumberProperty( props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, DEFAULT_WINDOW_SIZE.y );

	bool auto_center = true;

#if _WIN32
	ImVec2 startup_pos = sys_win32_get_startup_pos();

	if ( startup_pos.x == FLT_MIN )
	{
		printf( "No startup pos from windows? defaulting to center on monitor mouse is on\n" );
	}
	else
	{
		auto_center = false;
		SDL_SetNumberProperty( props, SDL_PROP_WINDOW_CREATE_X_NUMBER, startup_pos.x - ( DEFAULT_WINDOW_SIZE.x / 2 ) );
		SDL_SetNumberProperty( props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, startup_pos.y - ( DEFAULT_WINDOW_SIZE.y / 2 ) );
	}
#endif

	// for some reason, SDL_WINDOWPOS_UNDEFINED is ALWAYS centering the window in the middle on the primary display
	// so im trying to make it feel better and hacking it to open the window on the monitor the mouse is currently on
	if ( auto_center )
	{
		// Get the global mouse pos
		float mouse_x, mouse_y;
		SDL_GetGlobalMouseState( &mouse_x, &mouse_y );

		SDL_Point mouse;
		mouse.x                  = static_cast< int >( mouse_x );
		mouse.y                  = static_cast< int >( mouse_y );
		SDL_DisplayID display_id = SDL_GetDisplayForPoint( &mouse );

		SDL_SetNumberProperty( props, SDL_PROP_WINDOW_CREATE_X_NUMBER, SDL_WINDOWPOS_UNDEFINED_DISPLAY( display_id ) );
		SDL_SetNumberProperty( props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, SDL_WINDOWPOS_UNDEFINED_DISPLAY( display_id ) );
	}

	SDL_SetBooleanProperty( props, SDL_PROP_WINDOW_CREATE_OPENGL_BOOLEAN, true );
	SDL_SetBooleanProperty( props, SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN, true );
	SDL_SetBooleanProperty( props, SDL_PROP_WINDOW_CREATE_HIGH_PIXEL_DENSITY_BOOLEAN, true );
	SDL_SetBooleanProperty( props, SDL_PROP_WINDOW_CREATE_HIDDEN_BOOLEAN, true );

	SDL_SetStringProperty( props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, "Media Tag System" );

	app::window = SDL_CreateWindowWithProperties( props );

	if ( !app::window )
	{
		printf( "Failed to create SDL window\n" );
		SDL_DestroyProperties( props );
		return false;
	}

	// test the window to see if it was created correctly
	// not a bit depth of 0 if an hdmi device is used
	if ( !render_window_test() )
	{
		if ( !app::config.high_bpc )
		{
			printf( "Failed to create OpenGL context!\n" );
			SDL_DestroyProperties( props );
			return false;
		}

		fputs( " *** Falling back to 8-bit color depth! This may occur if you have a display plugged into HDMI that older than HDMI 2.1\n", stderr );

		// fallback to 8 bpc and create the window again
		if ( g_gl_context )
			SDL_GL_DestroyContext( g_gl_context );

		SDL_DestroyWindow( app::window );

		render_window_set_fallbacks();
		app::window = SDL_CreateWindowWithProperties( props );

		if ( !app::window )
		{
			printf( "Failed to create SDL window\n" );
			SDL_DestroyProperties( props );
			return false;
		}

		// create the opengl context here
		if ( !render_window_test() )
		{
			printf( "Failed to create OpenGL context!\n" );
			SDL_DestroyProperties( props );
			return false;
		}
	}

	SDL_DestroyProperties( props );

	SDL_ShowWindow( app::window );
	SDL_RaiseWindow( app::window );

	SDL_SetWindowMinimumSize( app::window, 200, 200 );
	return true;
}

