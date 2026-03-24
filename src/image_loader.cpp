#include "main.h"

void* malloc_stbi( size_t size )
{
	void* mem = malloc( size );
	// mem_add_item( e_mem_category_stbi_resize, mem, size );
	mem_add_item( e_mem_category_image_data, mem, size, 1, 6 );
	return mem;
}

#define STBIR_MALLOC( size, user_data ) ( (void)( user_data ), malloc_stbi( size ) )
#define STBIR_FREE( ptr, user_data )    ( (void)( user_data ), ch_free( e_mem_category_image_data, ptr ) )

#define STB_IMAGE_RESIZE2_IMPLEMENTATION
#include "stb_image_resize2.h"

std::vector< IImageLoader* > g_codecs;
std::vector< IImageLoader* > g_codecs_backup;


// =================================================================
// Texture Uploading


void gl_update_texture( GLuint texture, image_t* image, size_t frame_i )
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

	int width  = image->frame[ frame_i ].width;
	int height = image->frame[ frame_i ].height;

	if ( width == 0 )
		width = image->width;

	if ( height == 0 )
		height = image->height;

	if ( image->format == GL_RGBA16 )
	{
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA16, width, height, 0, GL_RGBA, GL_UNSIGNED_SHORT, (u16*)image->frame[ frame_i ].data );
	}
	else if ( image->format == GL_R16UI )
	{
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGB16, width, height, 0, GL_LUMINANCE, GL_UNSIGNED_SHORT, (u16*)image->frame[ frame_i ].data );
	}
	else if ( image->format == GL_R16I )
	{
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGB16, width, height, 0, GL_LUMINANCE, GL_SHORT, (s16*)image->frame[ frame_i ].data );
	}
	else if ( image->format == GL_R8 )
	{
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, image->frame[ frame_i ].data );
	}
	else if ( image->format == GL_RGBA32F )
	{
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, image->frame[ frame_i ].data );
	}
	else
	{
		glTexImage2D( GL_TEXTURE_2D, 0, image->format, width, height, 0, image->format, GL_UNSIGNED_BYTE, image->frame[ frame_i ].data );
	}

	auto err = glGetError();

	if ( err != 0 )
		printf( "GL Error: %d\n", err );
}


void gl_update_textures( uploaded_textures_t& textures, image_t* image, size_t frame_count )
{
	if ( textures.count != frame_count )
	{
		if ( textures.frame )
			glDeleteTextures( textures.count, textures.frame );

		textures.frame = ch_realloc< GLuint >( textures.frame, frame_count, e_mem_category_gl_texture_data, 2 );
		memset( textures.frame, 0, sizeof( GLuint ) * frame_count );

		glGenTextures( frame_count, textures.frame );
		textures.count = frame_count;
	}

	for ( size_t i = 0; i < frame_count; i++ )
	{
		gl_update_texture( textures.frame[ i ], image, i );
	}
}


void gl_free_textures( uploaded_textures_t& textures )
{
	if ( textures.frame )
		glDeleteTextures( textures.count, textures.frame );

	ch_free( e_mem_category_gl_texture_data, textures.frame );

	textures.frame = nullptr;
	textures.count = 0;
}


// =================================================================
// Image Loading


static std::unordered_map< std::string, IImageLoader* > g_ext_loader_map{};


void image_register_codec( IImageLoader* codec, bool fallback )
{
	if ( !codec )
		return;

	if ( fallback )
	{
		codec->loader_id = g_codecs_backup.size();
		g_codecs_backup.push_back( codec );
	}
	else
	{
		codec->loader_id = g_codecs.size();
		g_codecs.push_back( codec );
	}

	std::vector< std::string > exts;
	codec->get_supported_extensions( exts );

	for ( const std::string& ext : exts )
	{
		auto it = g_ext_loader_map.find( ext );

		// Already here
		if ( it != g_ext_loader_map.end() )
		{
			if ( fallback )
				continue;
		}
		
		// Add it
		g_ext_loader_map[ ext ] = codec;
	}
}


bool image_load( const fs::path& path, image_load_info_t& load_info, char* file_data, size_t file_len )
{
	std::string   path_std_string = path.string();
	const char*   path_str        = path_std_string.c_str();
	std::string   ext             = fs_get_extension( path_std_string );

	IImageLoader* loader          = image_check_extension( ext );

	if ( !loader )
	{
		if ( !load_info.quiet )
			printf( "No matching loader for image format!\n" );

		return false;
	}

	if ( !fs_is_file( path_str ) /*|| fs_file_size( path_str ) == 0*/ )
	{
		if ( !load_info.quiet )
			printf( "File is Empty or Doesn't exist: %s\n", path_str );

		return false;
	}

	bool allocated_image = false;

	if ( !load_info.image )
	{
		load_info.image = ch_calloc< image_t >( 1, e_mem_category_image );

		if ( !load_info.image )
		{
			if ( !load_info.quiet )
				printf( "Failed to allocate image data!\n" );

			return false;
		}

		allocated_image = true;
	}

	if ( file_data && !file_len )
		return false;

	if ( !file_data && file_len > 0 )
		return false;

	bool internal_file_ptr = !file_data && !file_len;

	if ( internal_file_ptr )
		file_data = fs_read_file( path.string().c_str(), &file_len );

	if ( !file_data || file_len == 0 )
		return false;

	bool loaded_image = loader->image_load( path, load_info, file_data, file_len );

	if ( !loaded_image )
	{
		for ( IImageLoader* _loader : g_codecs )
		{
			if ( _loader == loader )
				continue;

			loaded_image = _loader->image_load( path, load_info, file_data, file_len );

			if ( loaded_image )
				break;
		}
	}

	if ( !loaded_image )
	{
		for ( IImageLoader* _loader : g_codecs_backup )
		{
			if ( _loader == loader )
				continue;

			loaded_image = _loader->image_load( path, load_info, file_data, file_len );

			if ( loaded_image )
				break;
		}
	}

	if ( !loaded_image && allocated_image )
	{
		ch_free( e_mem_category_image, load_info.image );
		load_info.image = nullptr;
		return false;
	}

	if ( internal_file_ptr )
		ch_free( e_mem_category_file_data, file_data );

	return loaded_image;
}


// Free all image data
void image_free( image_t& image )
{
	image_free_alloc( image );
	// memset( &image, 0, sizeof( image_t ) );
	
	image.width           = 0;
	image.height          = 0;
	image.bit_depth       = 0;
	image.pitch           = 0;
	image.bytes_per_pixel = 0;
	image.format          = 0;
	image.loop_count      = 0;
}


// Free only frames
void image_free_frames( image_t& image )
{
	if ( image.frame.empty() )
		return;

	//for ( image_frame_t& frame : image.frame )
	//	ch_free( e_mem_category_image_data, frame.data );

	image.frame.clear();
}


// Free only frames and allocations
void image_free_alloc( image_t& image )
{
	image_free_frames( image );

	if ( image.image_format )
		ch_free_str( image.image_format );

	image.image_format = nullptr;
}


void image_copy_data( image_t& src, image_t& dst )
{
	dst.width           = src.width;
	dst.height          = src.height;
	dst.bit_depth       = src.bit_depth;
	dst.pitch           = src.pitch;
	dst.bytes_per_pixel = src.bytes_per_pixel;
	dst.channels        = src.channels;
	dst.format          = src.format;
	dst.loop_count      = src.loop_count;
	dst.image_format    = src.image_format;
}


void image_copy_frame_data( image_frame_t& src, image_frame_t& dst )
{
	dst.width  = src.width;
	dst.height = src.height;
	dst.pos_x  = src.pos_x;
	dst.pos_x  = src.pos_y;
	dst.time   = src.time;
}


bool image_copy_frame_data( image_t& src, image_t& dst, size_t frame_i )
{
	if ( frame_i >= src.frame.size() || frame_i >= dst.frame.size() )
		return false;

	image_copy_frame_data( src.frame[ frame_i ], dst.frame[ frame_i ] );
	return true;
}


IImageLoader* image_check_extension( const std::string& ext )
{
	auto it = g_ext_loader_map.find( ext );

	if ( it == g_ext_loader_map.end() )
		return nullptr;

	return it->second;
}


bool media_check_extension( const std::string& ext, e_media_type& type )
{
	if ( image_check_extension( ext ) )
	{
		type = e_media_type_image;
		return true;
	}

	if ( g_mpv )
	{
		if ( mpv_supports_ext( ext ) )
		{
			type = e_media_type_video;
			return true;
		}
	}

	type = e_media_type_none;
	return false;
}


bool image_scale( image_t* old_image, image_t* new_image, int new_width, int new_height )
{
	stbir_pixel_layout pixel_layout = STBIR_RGBA;
	stbir_datatype     datatype     = STBIR_TYPE_UINT8;

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

	new_image->frame.resize( old_image->frame.size() );

	for ( size_t i = 0; i < old_image->frame.size(); i++ )
	{
		u8* old_frame = old_image->frame[ i ].data;

		if ( old_frame == nullptr )
		{
			printf( "nullptr frame?????\n" );
			return false;
		}

		// u8* new_frame     = ch_calloc< u8 >( new_width * new_height * old_image->channels, e_mem_category_image_data );

		u8* resized_frame = (u8*)stbir_resize(
		  old_frame, old_image->frame[ i ].width, old_image->frame[ i ].height, 0,
		  nullptr, new_width, new_height, 0,
		  pixel_layout, datatype, STBIR_EDGE_CLAMP, STBIR_FILTER_DEFAULT );

		if ( !resized_frame )
			return false;

		new_image->frame[ i ].data   = resized_frame;
		new_image->frame[ i ].size   = new_width * new_height * old_image->channels;
		new_image->frame[ i ].width  = new_width;
		new_image->frame[ i ].height = new_height;
	}

	new_image->width           = new_width;
	new_image->height          = new_height;
	new_image->pitch           = new_width * old_image->channels;
	new_image->bit_depth       = 4;
	new_image->bytes_per_pixel = 4;
	new_image->channels        = old_image->channels;

	switch ( old_image->channels )
	{
		default:
		case 4:
			new_image->format = GL_RGBA;
			break;

		case 3:
			new_image->format = GL_RGB;
			break;

		case 1:
			new_image->format = GL_LUMINANCE;
			break;
	}

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
	ch_free_str( exe_dir );

	for ( u8 i = 0; i < e_icon_count; i++ )
	{
		image_load_info_t load_info{};
		load_info.image = &g_icon_image[ i ];

		if ( !image_load( exe_path / g_icon_paths[ i ], load_info ) )
		{
			printf( "Failed to load %s icon \"%s\"\n", g_icon_names[ i ], g_icon_paths[ i ] );
			continue;
		}

		glGenTextures( 1, &g_icon_texture[ i ] );
		gl_update_texture( g_icon_texture[ i ], &g_icon_image[ i ] );

		if ( !g_icon_texture[ i ] )
		{
			printf( "Failed to upload %s icon \"%s\"\n", g_icon_names[ i ], g_icon_paths[ i ] );
			continue;
		}

		image_free_alloc( g_icon_image[ i ] );

		printf( "Loaded icon %s\n", g_icon_names[ i ] );
	}

	return true;
}


void icon_free()
{
	for ( u8 i = 0; i < e_icon_count; i++ )
	{
		glDeleteTextures( 1, &g_icon_texture[ i ] );
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

