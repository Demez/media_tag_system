#include "main.h"

#include "imgui_impl_sdl3.h"
#include "imgui_impl_opengl3.h"


constexpr float      M_PI   = 3.14159265358979323846f;
constexpr float      TO_RAD = M_PI / 180.f;

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

	GLint                in_texture       = 0;

	virtual bool         init()
	{
		return true;
	}

	virtual void draw( const render_draw_texture_t& draw_info )
	{

	}
};


shader_base_t* shader_list[ e_shader_count ] = {};


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


#define GL_DEBUG_TYPE_STR( find, str ) if ( type == find ) return str


const char* gl_debug_type_str( GLenum type )
{
	GL_DEBUG_TYPE_STR( GL_DEBUG_TYPE_ERROR, "Error" );
	GL_DEBUG_TYPE_STR( GL_DEBUG_TYPE_PORTABILITY, "Portability" );
	GL_DEBUG_TYPE_STR( GL_DEBUG_TYPE_PERFORMANCE, "Perf" );
	GL_DEBUG_TYPE_STR( GL_DEBUG_TYPE_OTHER, "Other" );
	GL_DEBUG_TYPE_STR( GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR, "Deprecated" );
	GL_DEBUG_TYPE_STR( GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR, "Undefined" );

	return "Unknown";
}


void gl_msg_callback( GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam )
{
	if ( type == GL_DEBUG_TYPE_OTHER ) return;

	if ( type == GL_DEBUG_TYPE_ERROR )
		(void)0;  // breakpoint lol

	printf( "[GL - %s] - %s\n", gl_debug_type_str( type ), message );
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

	shader->in_texture       = glGetUniformLocation( shader->program, "in_texture" );

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


bool render_window_create();


bool render_init()
{
	if ( !render_window_create() )
		return false;

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

	glEnable( GL_DEBUG_OUTPUT );
	glEnable( GL_DEBUG_OUTPUT_SYNCHRONOUS );
	gl_check_error();

	glDebugMessageCallback( gl_msg_callback, nullptr );

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
	glUniform1i( shader->in_texture, 0 );

	shader->draw( draw_info );

	glDrawElements( GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0 );

	if ( !draw_info.hide_alpha )
	{
		glDisable( GL_BLEND );
	}
}

