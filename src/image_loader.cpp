#include "main.h"

void* malloc_stbi( size_t size )
{
	void* mem = malloc( size );
	mem_add_item( e_mem_category_stbi_resize, mem, size );
	return mem;
}

#define STBIR_MALLOC( size, user_data ) ( (void)( user_data ), malloc_stbi( size ) )
#define STBIR_FREE( ptr, user_data )    ( (void)( user_data ), ch_free( e_mem_category_stbi_resize, ptr ) )

#define STB_IMAGE_RESIZE2_IMPLEMENTATION
#include "stb_image_resize2.h"

std::vector< IImageLoader* > g_codecs;


// =================================================================
// Texture Uploading


void gl_update_texture( GLuint texture, image_t* image )
{
	glBindTexture( GL_TEXTURE_2D, texture );

	// disable wrapping
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );

	// Setup filtering parameters for display
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );  // downscaling image
	// glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );  // upscaling image

	// Upload pixels into texture
	// glPixelStorei( GL_UNPACK_ROW_LENGTH, 0 );

	//if ( image->bytes_per_pixel > 1 )
	//glPixelStorei( GL_UNPACK_ROW_LENGTH, image->pitch / image->bytes_per_pixel );
	//glPixelStorei( GL_UNPACK_ROW_LENGTH, image->width );

	//glPixelStorei( GL_UNPACK_ROW_LENGTH, 0 );
	//glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );

	if ( image->format == GL_RGBA16 )
	{
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA16, image->width, image->height, 0, GL_RGBA, GL_UNSIGNED_SHORT, (u16*)image->frame[ 0 ] );
	}
	else if ( image->format == GL_R16UI )
	{
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGB16, image->width, image->height, 0, GL_LUMINANCE, GL_UNSIGNED_SHORT, (u16*)image->frame[ 0 ] );
	}
	else if ( image->format == GL_R16I )
	{
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGB16, image->width, image->height, 0, GL_LUMINANCE, GL_SHORT, (s16*)image->frame[ 0 ] );
	}
	else if ( image->format == GL_R8 )
	{
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGB, image->width, image->height, 0, GL_RED, GL_UNSIGNED_BYTE, image->frame[ 0 ] );
	}
	else if ( image->format == GL_RGBA32F )
	{
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA32F, image->width, image->height, 0, GL_RGBA, GL_FLOAT, image->frame[ 0 ] );
	}
	else
	{
		glTexImage2D( GL_TEXTURE_2D, 0, image->format, image->width, image->height, 0, image->format, GL_UNSIGNED_BYTE, image->frame[ 0 ] );
	}

	auto err = glGetError();

	if ( err != 0 )
		printf( "FUCK: %d\n", err );

	glBindTexture( GL_TEXTURE_2D, 0 );
}


GLuint gl_upload_texture( image_t* image )
{
	GLuint image_texture;
	glGenTextures( 1, &image_texture );

	gl_update_texture( image_texture, image );

	return image_texture;
}


void gl_free_texture( GLuint texture )
{
	glDeleteTextures( 1, &texture );
}


// =================================================================
// Image Loading


void image_register_codec( IImageLoader* codec )
{
	if ( codec )
		g_codecs.push_back( codec );
}


bool image_load( const fs::path& path, image_load_info_t& load_info )
{
	std::string path_std_string = path.string();
	const char* path_str        = path_std_string.c_str();
	std::string ext_str         = path.extension().string();

	if ( !fs_is_file( path_str ) || fs_file_size( path_str ) == 0 )
	{
		printf( "File is Empty or Doesn't exist: %s\n", path_str );
		return false;
	}

	bool allocated_image = false;

	if ( !load_info.image )
	{
		load_info.image = ch_calloc< image_t >( 1, e_mem_category_image );

		if ( !load_info.image )
		{
			printf( "Failed to allocate image data!\n" );
			return false;
		}

		allocated_image = true;
	}

	size_t file_len  = 0;
	char*  file_data = fs_read_file( path.string().c_str(), &file_len );

	if ( !file_data )
	{
		printf( "Failed to read file: %s\n", path_str );
		return false;
	}

	bool loaded_image = false;

	for ( IImageLoader* codec : g_codecs )
	{
		// if ( !codec->check_extension( ext_str ) )
		if ( !codec->check_extension( path_str ) )
			continue;

		loaded_image = codec->image_load( path, load_info, file_data, file_len );

		if ( loaded_image )
			break;
	}

	ch_free( e_mem_category_file_data, file_data );

	if ( !loaded_image && allocated_image )
	{
		ch_free( e_mem_category_image, load_info.image );
		load_info.image = nullptr;
	}

	return loaded_image;
}


bool image_check_extension( std::string_view ext )
{
	for ( IImageLoader* codec : g_codecs )
	{
		if ( codec->check_extension( ext ) )
			return true;
	}

	return false;
}


bool image_downscale( image_t* old_image, image_t* new_image, int new_width, int new_height )
{
	stbir_pixel_layout pixel_layout  = STBIR_RGBA;
	stbir_datatype     datatype      = STBIR_TYPE_UINT8;

	switch ( old_image->format )
	{
		case GL_LUMINANCE:
			pixel_layout = STBIR_1CHANNEL;
			break;

		case GL_R16UI:
			pixel_layout = STBIR_1CHANNEL;
			datatype     = STBIR_TYPE_UINT16;
			break;

		case GL_RGB:
			pixel_layout = STBIR_RGB;
			break;

		default:
		case GL_RGBA:
			pixel_layout = STBIR_RGBA;
			break;

		case GL_RGBA16:
			pixel_layout = STBIR_RGBA;
			datatype     = STBIR_TYPE_UINT16;
			break;

		case GL_RGBA32F:
			pixel_layout = STBIR_RGBA;
			datatype     = STBIR_TYPE_FLOAT;
			break;
	}

	u8* old_frame = old_image->frame[ 0 ];

	if ( old_frame == nullptr )
	{
		printf( "nullptr frame?????\n" );
		return false;
	}

	u8* resized_frame = (u8*)stbir_resize(
	  old_frame, old_image->width, old_image->height, 0,
	  nullptr, new_width, new_height, 0,
      pixel_layout, datatype, STBIR_EDGE_WRAP, STBIR_FILTER_DEFAULT );

	if ( !resized_frame )
		return false;

	new_image->frame[ 0 ] = resized_frame;
	new_image->width      = new_width;
	new_image->height     = new_height;
	new_image->pitch      = new_width * 4;
	// new_image->format     = GL_RGBA;

	// mem_add_item( e_mem_category_image_data, resized_frame, );

	return true;
}


// =================================================================
// Icon Loading


static const char* g_icon_names[] = {
	"none",
	"invalid",
	"folder",
	"loading",
	"video",
};


static const char* g_icon_paths[] = {
	"icons/none.png",
	"icons/invalid.png",
	"icons/folder.png",
	"icons/loading.png",
	"icons/video.png",
};


static image_t g_icon_image[ e_icon_count ]{};
static GLuint  g_icon_texture[ e_icon_count ]{};

static_assert( ARR_SIZE( g_icon_names ) == e_icon_count );
static_assert( ARR_SIZE( g_icon_paths ) == e_icon_count );


bool icon_preload()
{
	char*    exe_dir  = sys_get_exe_folder();
	fs::path exe_path = exe_dir;
	ch_free( e_mem_category_string, exe_dir );

	for ( u8 i = 0; i < e_icon_count; i++ )
	{
		image_load_info_t load_info{};
		load_info.image = &g_icon_image[ i ];

		if ( !image_load( exe_path / g_icon_paths[ i ], load_info ) )
		{
			printf( "Failed to load %s icon \"%s\"\n", g_icon_names[ i ], g_icon_paths[ i ] );
			continue;
		}

		g_icon_texture[ i ] = gl_upload_texture( &g_icon_image[ i ] );

		if ( !g_icon_texture[ i ] )
		{
			printf( "Failed to upload %s icon \"%s\"\n", g_icon_names[ i ], g_icon_paths[ i ] );
			continue;
		}

		ch_free( e_mem_category_image_data, g_icon_image[ i ].frame[ 0 ] );
		g_icon_image[ i ].frame.clear();

		printf( "Loaded icon %s\n", g_icon_names[ i ] );
	}

	return true;
}


void icon_free()
{
	for ( u8 i = 0; i < e_icon_count; i++ )
	{
		gl_free_texture( g_icon_texture[ i ] );
	}
}


image_t* icon_get_image( e_icon icon_type )
{
	if ( icon_type >= e_icon_count )
		return {};

	return &g_icon_image[ icon_type ];
}


ImTextureRef icon_get_imtexture( e_icon icon_type )
{
	if ( icon_type >= e_icon_count )
		return {};

	return static_cast< ImTextureRef >( g_icon_texture[ icon_type ] );
}


