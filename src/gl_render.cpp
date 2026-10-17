#include "main.h"

#include "imgui_impl_sdl3.h"
#include "imgui_impl_opengl3.h"


constexpr float      M_PI   = 3.14159265358979323846f;
constexpr float      TO_RAD = M_PI / 180.f;

constexpr ImVec2     DEFAULT_WINDOW_SIZE( 1000, 600 );

extern SDL_GLContext g_gl_context;
extern bool          g_in_draw;

// vertex buffer object
static GLuint        g_image_vbo      = 0;

// vertex array object
static GLuint        g_image_vao      = 0;

// element buffer object
static GLuint        g_image_index_buffer = 0;

//static float         vertices[] = {
//	// positions          // texture coords
//	 1.0f,  1.0f,   1.0f, 0.0f,  // top right
//	 1.0f, -1.0f,   1.0f, 1.0f,  // bottom right
//	-1.0f, -1.0f,   0.0f, 1.0f,  // bottom left
//	-1.0f,  1.0f,   0.0f, 0.0f,  // top left
//};

//static float         vertices[] = {
//	// positions          // texture coords
//	 1.0f,  1.0f,   0.0f, 0.0f,  // top right
//	 1.0f, -1.0f,   1.0f, 0.0f,  // bottom right
//	-1.0f, -1.0f,   1.0f, 1.0f,  // bottom left
//	-1.0f,  1.0f,   0.0f, 1.0f,  // top left
//};

static float         vertices[] = {
	// texture coords
	0.0f, 0.0f,  // top right
	1.0f, 0.0f,  // bottom right
	1.0f, 1.0f,  // bottom left
	0.0f, 1.0f,  // top left
};


static u32 indices[] = {
	0, 1, 3,  // first triangle
	1, 2, 3   // second triangle
};


const char*          shader_str[]              = {
	"Main",
	"Palette",
};


static_assert( ARR_SIZE( shader_str ) == e_shader_count );

// Shader Info
struct shader_base_t
{
	const char*          name             = nullptr;
	const fs::path_char* vert_path        = nullptr;
	const fs::path_char* frag_path        = nullptr;

	GLuint               program          = 0;

	GLint                projection       = 0;
	GLint                view             = 0;

	GLint                window_size      = 0;
	GLint                image_size       = 0;
	GLint                image_pos        = 0;
	GLint                image_rot        = 0;
	GLint                visible_channels = 0;

	virtual bool         init()
	{
		return true;
	}

	virtual void draw( const render_draw_texture_t& draw_info )
	{

	}
};


shader_base_t* shader_list[ e_shader_count ] = {};


//using shader_list_t = std::vector< shader_base_t& >;
//
//
//shader_list_t& get_shader_list()
//{
//	static shader_list_t shader_list;
//	return shader_list;
//}


struct shader_main_t : public shader_base_t
{
};


// Shader Info - Palette
struct shader_palette_t : public shader_base_t
{
	GLint        in_palette       = 0;
	GLint        in_palette_trans = 0;

	virtual bool init()
	{
		in_palette       = glGetUniformLocation( program, "in_palette" );
		in_palette_trans = glGetUniformLocation( program, "in_palette_trans" );
		return true;
	}

	virtual void draw( const render_draw_texture_t& draw_info )
	{
		glActiveTexture( GL_TEXTURE1 );
		glBindTexture( GL_TEXTURE_1D, draw_info.palette );
		glUniform1i( in_palette, 1 );
		glUniform1i( in_palette_trans, draw_info.palette_trans );
	}
};


struct register_shader_t
{
	register_shader_t( e_shader shader_enum, shader_base_t* shader_base, const fs::path_char* vert_path, const fs::path_char* frag_path )
	{
		shader_base->name          = shader_str[ shader_enum ];
		shader_base->vert_path     = vert_path;
		shader_base->frag_path     = frag_path;

		shader_list[ shader_enum ] = shader_base;
	}
};

#define REGISTER_SHADER( shader_enum, type, vert_path, frag_path ) \
	register_shader_t __reg_##type( shader_enum, new type, PATH_FMT( vert_path ), PATH_FMT( frag_path ) )


REGISTER_SHADER( e_shader_main, shader_main_t, "shaders/image.vert", "shaders/image.frag" );
REGISTER_SHADER( e_shader_palette, shader_palette_t, "shaders/image.vert", "shaders/image_palette.frag" );



// ================================================================================================


#define CHECK_GL_ERROR( val )                      \
	if ( err == val ) \
		printf( "GL Error: %d - " #val "\n", err )


void gl_check_error()
{
	GLenum err = glGetError();

	if ( err == GL_NO_ERROR )
		return;

	CHECK_GL_ERROR( GL_INVALID_VALUE );
	else CHECK_GL_ERROR( GL_INVALID_OPERATION );
	else CHECK_GL_ERROR( GL_INVALID_FRAMEBUFFER_OPERATION );
	else CHECK_GL_ERROR( GL_OUT_OF_MEMORY );
	else printf( "Unknown GL Error - %d", err );
}

#undef CHECK_GL_ERROR


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
		return 1;
	}

	// Create Window
	SDL_PropertiesID props      = SDL_CreateProperties();

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


// ================================================================================================


bool render_load_shader( GLuint& shader, const fs::path_char* shader_path, GLenum shader_type )
{
	size_t shader_src_len = 0;
	char* shader_src = fs_read_file_app_dir( shader_path, &shader_src_len );

	if ( !shader_src )
	{
		path_printf(  "Failed to find shader: \"%s\"\n", shader_path );
		return false;
	}

	shader = glCreateShader( shader_type );
	glShaderSource( shader, 1, &shader_src, nullptr );
	glCompileShader( shader );

	ch_free( e_mem_category_file_data, shader_src );

	GLenum err = glGetError();

	if ( err != GL_NO_ERROR )
	{
		printf( "GL Error: %d\n", err );
		return false;
	}
	
	return true;
}


bool render_load_shader( shader_base_t* shader )
{
	GLuint vert, frag;

	if ( !render_load_shader( vert, shader->vert_path, GL_VERTEX_SHADER ) )
	{
		printf( "Failed to load vertex shader\n" );
		return false;
	}

	if ( !render_load_shader( frag, shader->frag_path, GL_FRAGMENT_SHADER ) )
	{
		printf( "Failed to load vertex shader\n" );
		return false;
	}

	// create a program and attach shaders
	shader->program = glCreateProgram();
	glAttachShader( shader->program, vert );
	glAttachShader( shader->program, frag );
	glLinkProgram( shader->program );

	GLint success = 0;
	glGetProgramiv( shader->program, GL_LINK_STATUS, &success );
	if ( !success )
	{
		printf( "Failed to compile shader program\n" );

		GLchar log_buf[ 512 ];
		glGetProgramInfoLog( shader->program, 512, NULL, log_buf );

		printf( "GL Log:\n%s\n", log_buf );
		return false;
	}

	glUseProgram( shader->program );

	// shaders are linked into program, can free these objects now
	glDeleteShader( vert );
	glDeleteShader( frag );

	shader->projection       = glGetUniformLocation( shader->program, "projection" );
	shader->view             = glGetUniformLocation( shader->program, "view" );

	shader->window_size      = glGetUniformLocation( shader->program, "window_size" );
	shader->image_size       = glGetUniformLocation( shader->program, "image_size" );
	shader->image_pos        = glGetUniformLocation( shader->program, "image_pos" );
	shader->image_rot        = glGetUniformLocation( shader->program, "image_rotation" );
	shader->visible_channels = glGetUniformLocation( shader->program, "in_visible_channels" );

	return shader->init();
}


bool render_load_shaders()
{
	for ( u8 i = 0; i < e_shader_count; i++ )
	{
		if ( render_load_shader( shader_list[ i ] ) )
			continue;

		printf( "Failed to load shader - %s\n", shader_str[ i ] );
		return false;
	}

	return true;
}


void render_bind_buffer_data()
{
	// vertex array object
	glBindVertexArray( g_image_vao );

	// vertex buffer object
	glBindBuffer( GL_ARRAY_BUFFER, g_image_vbo );
	glBufferData( GL_ARRAY_BUFFER, sizeof( vertices ), vertices, GL_STATIC_DRAW );

	glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, g_image_index_buffer );
	glBufferData( GL_ELEMENT_ARRAY_BUFFER, sizeof( indices ), indices, GL_STATIC_DRAW ); 
	
	glVertexAttribPointer( 0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof( float ), (void*)0 );
	//glVertexAttribPointer( 0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof( float ), (void*)0 );
	glEnableVertexAttribArray( 0 );

	//glVertexAttribPointer( 1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof( float ), (void*)( 2 * sizeof( float ) ) );
	//glEnableVertexAttribArray( 1 );
}


bool render_gen_buffers()
{
	// generate vertex array object
	glGenVertexArrays( 1, &g_image_vao );

	// generate vertex buffer object
	glGenBuffers( 1, &g_image_vbo );

	glGenBuffers( 1, &g_image_index_buffer );

	render_bind_buffer_data();
	return true;
}


bool render_init()
{
	render_window_create();

	int r, g, b, a, f;
	SDL_GL_GetAttribute( SDL_GL_RED_SIZE, &r );
	SDL_GL_GetAttribute( SDL_GL_GREEN_SIZE, &g );
	SDL_GL_GetAttribute( SDL_GL_BLUE_SIZE, &b );
	SDL_GL_GetAttribute( SDL_GL_ALPHA_SIZE, &a );
	SDL_GL_GetAttribute( SDL_GL_FLOATBUFFERS, &f );

	printf(
	  "\nWindow Texture:\n"
	  "RED   - %d\n"
	  "GREEN - %d\n"
	  "BLUE  - %d\n"
	  "ALPHA - %d\n"
	  "\n",
	  r, g, b, a );

	if ( !gladLoadGL() )
	{
		printf( "Failed to load GL\n" );
		return false;
	}

	if ( !render_gen_buffers() )
		return false;

	if ( !render_load_shaders() )
		return false;

	return true;
}


void render_shutdown()
{
	SDL_GL_DestroyContext( g_gl_context );
}


// ================================================================================================


void render_draw_texture( render_draw_texture_t draw_info )
{
	if ( draw_info.flip_h )
	{
		draw_info.x += draw_info.width;
		draw_info.width *= -1;
	}

	if ( draw_info.flip_v )
	{
		draw_info.y += draw_info.height;
		draw_info.height *= -1;
	}

	glBindVertexArray( g_image_vao );

	shader_base_t* shader = shader_list[ draw_info.shader ];
	
	glUseProgram( shader->program );

	if ( !draw_info.hide_alpha )
	{
		glEnable( GL_BLEND );
		glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	}

	int window_width, window_height;

	if ( draw_info.target )
	{
		window_width  = draw_info.target_width;
		window_height = draw_info.target_height;

		glBindFramebuffer( GL_FRAMEBUFFER, draw_info.target );
	}
	else
	{
		SDL_GetWindowSizeInPixels( app::window, &window_width, &window_height );
		glBindFramebuffer( GL_FRAMEBUFFER, 0 );
	}

	glViewport( 0, 0, window_width, window_height );

	// from imgui code
	float       L                              = 0.f;
	float       R                              = static_cast< float >( window_width );
	float       T                              = 0.f;
	float       B                              = static_cast< float >( window_height );

	const float ortho_projection[ 4 ][ 4 ]     = {
		{ 2.0f / ( R - L ), 0.0f, 0.0f, 0.0f },
		{ 0.0f, 2.0f / ( T - B ), 0.0f, 0.0f },
		{ 0.0f, 0.0f, -1.0f, 0.0f },
		{ ( R + L ) / ( L - R ), ( T + B ) / ( B - T ), 0.0f, 1.0f },
	};

	glUniformMatrix4fv( shader->projection, 1, GL_FALSE, &ortho_projection[ 0 ][ 0 ] );

	glUniform2i( shader->window_size, window_width, window_height );
	glUniform2i( shader->image_size, draw_info.width, draw_info.height );
	glUniform2i( shader->image_pos, draw_info.x, draw_info.y );
	glUniform1f( shader->image_rot, draw_info.rotation * TO_RAD );

	glUniform4i(
	  shader->visible_channels,
	  !draw_info.hide_channel[ 0 ],
	  !draw_info.hide_channel[ 1 ],
	  !draw_info.hide_channel[ 2 ],
	  !draw_info.hide_channel[ 3 ] );

	gl_check_error();

	glActiveTexture( GL_TEXTURE0 );
	glBindTexture( GL_TEXTURE_2D, draw_info.texture );
	glUniform1i( glGetUniformLocation( shader->program, "in_texture" ), 0 );

	shader->draw( draw_info );

	glDrawElements( GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0 );

	if ( !draw_info.hide_alpha )
	{
		glDisable( GL_BLEND );
	}
}

