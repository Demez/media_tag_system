#include "main.h"

#include <cctype>
#include <unordered_map>
#include <unordered_set>

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
// Image Loading


static std::unordered_map< std::string, IImageLoader* >& get_ext_loader_map()
{
	static std::unordered_map< std::string, IImageLoader* > ext_loader_map;
	return ext_loader_map;
}

static std::unordered_set< std::string >& get_ext_loader_set()
{
	static std::unordered_set< std::string > ext_loader_set;
	return ext_loader_set;
}

//static std::unordered_map< std::string, IImageLoader* > g_ext_loader_map{};
//static std::unordered_set< std::string >                g_ext_loader_set{};

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
		auto it = get_ext_loader_map().find( ext );

		// Already here
		if ( it != get_ext_loader_map().end() )
		{
			if ( fallback )
				continue;
		}
		
		// Add it
		get_ext_loader_map()[ ext ] = codec;
		get_ext_loader_set().insert( ext );
	}
}


bool image_load( const fs::path& path, image_load_info_t& load_info, char* file_data, size_t file_len )
{
	std::string path_std_string = sys_path_to_string( path );
	std::string ext             = fs_get_extension( path_std_string );

	for ( size_t i = 0; i < ext.size(); i++ )
	{
		ext[ i ] = static_cast< char >( std::tolower( static_cast< int >( ext.data()[ i ] ) ) );
	}

	IImageLoader* loader = image_check_extension( ext );

	if ( !loader )
	{
		if ( !load_info.quiet )
			printf( "No matching loader for image format!\n" );

		return false;
	}

	if ( !fs_is_file( path.c_str() ) /*|| fs_file_size( path_str ) == 0*/ )
	{
		if ( !load_info.quiet )
			path_printf( "File is Empty or Doesn't exist: %s\n", path.c_str() );

		return false;
	}

	bool allocated_image = false;

	if ( !load_info.image )
	{
		load_info.image = image_create();

		if ( !load_info.image )
		{
			if ( !load_info.quiet )
				printf( "Failed to allocate image data!\n" );

			return false;
		}

		// construct in place
		load_info.image = new ( load_info.image ) image_t();

		allocated_image = true;
	}

	if ( file_data && !file_len )
		return false;

	if ( !file_data && file_len > 0 )
		return false;

	bool internal_file_ptr = !file_data && !file_len;

	if ( internal_file_ptr )
		file_data = fs_read_file( path.c_str(), &file_len );

	if ( !file_data || file_len == 0 )
		return false;

	image_free_data( *load_info.image );

	bool loaded_image = loader->image_load( path, load_info, file_data, file_len );

	if ( !loaded_image )
	{
		for ( IImageLoader* _loader : g_codecs )
		{
			if ( _loader == loader )
				continue;

			image_free_data( *load_info.image );
			loaded_image = _loader->image_load( path, load_info, file_data, file_len );

			if ( loaded_image )
				break;
		}
	}

	if ( !loaded_image )
	{
		printf( "*** Falling to backup codec\n" );

		for ( IImageLoader* _loader : g_codecs_backup )
		{
			if ( _loader == loader )
				continue;

			image_free_data( *load_info.image );
			loaded_image = _loader->image_load( path, load_info, file_data, file_len );

			if ( loaded_image )
				break;
		}
	}

	if ( internal_file_ptr )
		ch_free( e_mem_category_file_data, file_data );

	if ( !loaded_image && allocated_image )
	{
		image_free( load_info.image );
		return false;
	}

	if ( loaded_image && !image_verify( load_info.image ) )
	{
		printf( "Image is invalid !!!\n" );
		image_free( load_info.image );
		return false;
	}

	return loaded_image;
}


void image_free_palette( image_palette_t& palette )
{
	palette.count       = 0;
	palette.trans_index = 0;

	ch_free( e_mem_category_image_data, palette.elem );
}


void image_reset_frame_data( image_frame_t& frame )
{
	frame.time           = 0.f;
	frame.width          = 0;
	frame.height         = 0;
	frame.pitch          = 0;
	frame.pos_x          = 0;
	frame.pos_y          = 0;
	frame.flags          = 0;

	frame.frame_disposal = e_frame_disposal_keep;
	frame.color_type     = e_colortype_bitmap;

	image_free_palette( frame.palette );
}


void image_reset_data( image_t& image )
{
	image.width      = 0;
	image.height     = 0;
	image.format     = 0;
	image.channels   = 0;
	image.loop_count = 0;
	image.flags      = 0;

	image_free_palette( image.palette );

	for ( auto& frame : image.frame )
	{
		image_reset_frame_data( frame );
	}
}


image_t* image_create()
{
	auto image = ch_calloc< image_t >( 1, e_mem_category_image );

	// construct in place
	image = new ( image ) image_t();
	return image;
}


void image_free( image_t*& image )
{
	if ( !image )
		return;

	image_reset_data( *image );
	image_free_alloc( *image );

	image->~image_t();

	ch_free( e_mem_category_image, image );
}


// Free all image data
void image_free_data( image_t& image )
{
	image_reset_data( image );
	image_free_alloc( image );
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


// set frame_count to 0 to copy all frames
#if 0
void image_copy_all( image_t& src, image_t& dst, size_t frame_count = 0 )
{
	image_copy_data( src, dst );

	if ( frame_count == 0 )
		frame_count = src.frame.size();

	// copy first frame
	dst.frame.resize( frame_count );

	for ( size_t i = 0; i < dst.)

	image_frame_t& src = thumbnail->image->frame[ 0 ];
	image_frame_t& dst = entry.image.frame[ 0 ];

	image_copy_frame_data( src, dst );

	dst.data = ch_calloc < = src.frame_disposal;
}
#endif


// all of this should be ref counted probably, ugh
void image_copy_palette( image_palette_t& src, image_palette_t& dst )
{
	dst.count        = src.count;
	dst.trans_index  = src.trans_index;
	dst.elem         = ch_calloc< image_palette_elem_t >( src.count, e_mem_category_image_data );

	for ( u32 i = 0; i < src.count; i++ )
	{
		dst.elem[ i ].r = src.elem[ i ].r;
		dst.elem[ i ].g = src.elem[ i ].g;
		dst.elem[ i ].b = src.elem[ i ].b;
		dst.elem[ i ].a = src.elem[ i ].a;
	}
}


void image_copy_data( image_t& src, image_t& dst )
{
	dst.width        = src.width;
	dst.height       = src.height;
	dst.channels     = src.channels;
	dst.format       = src.format;
	dst.loop_count   = src.loop_count;
	dst.image_format = src.image_format;

	image_copy_palette( src.palette, dst.palette );
}


void image_copy_frame_data( image_frame_t& src, image_frame_t& dst )
{
	dst.width          = src.width;
	dst.height         = src.height;
	dst.pitch          = src.pitch;
	dst.pos_x          = src.pos_x;
	dst.pos_x          = src.pos_y;
	dst.time           = src.time;
	dst.frame_disposal = src.frame_disposal;
	dst.flags          = src.flags;
	dst.color_type     = src.color_type;

	image_copy_palette( src.palette, dst.palette );
}


bool image_copy_frame_data( image_t& src, image_t& dst, size_t frame_i )
{
	if ( frame_i >= src.frame.size() || frame_i >= dst.frame.size() )
		return false;

	image_copy_frame_data( src.frame[ frame_i ], dst.frame[ frame_i ] );
	return true;
}


// verify an image is valid and can be uploaded
bool image_verify( image_t* image, bool check_pixel_data )
{
	if ( !image )
		return false;

	bool field_missing = false;

	field_missing |= image->width == 0;
	field_missing |= image->height == 0;
	field_missing |= image->format == 0;
	field_missing |= image->channels == 0;
	field_missing |= image->frame.empty();

	for ( auto& frame : image->frame )
	{
		field_missing |= frame.width == 0;
		field_missing |= frame.height == 0;
		field_missing |= frame.color_type == e_colortype_invalid;

		if ( check_pixel_data )
		{
			field_missing |= frame.data == nullptr;
			field_missing |= frame.size == 0;
		}

		if ( field_missing )
			break;
	}

	return !field_missing;
}


// ===================================================================================================


bool image_check_extension_fast( const std::string& ext )
{
	return get_ext_loader_set().contains( ext );
}


IImageLoader* image_check_extension( const std::string& ext )
{
	auto it = get_ext_loader_map().find( ext );

	if ( it == get_ext_loader_map().end() )
		return nullptr;

	return it->second;
}


bool media_check_extension_fast( std::string& ext, e_media_type& type )
{
	for ( size_t i = 0; i < ext.size(); i++ )
	{
		ext.data()[ i ] = std::tolower( static_cast< unsigned char >( ext.data()[ i ] ) );
	}

	if ( image_check_extension_fast( ext ) )
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


bool media_check_extension( std::string ext, e_media_type& type )
{
	return media_check_extension_fast( ext, type );
}


bool image_scale( image_t* old_image, image_t* new_image, int new_width, int new_height )
{
	stbir_pixel_layout pixel_layout = STBIR_RGBA;
	stbir_datatype     datatype     = STBIR_TYPE_UINT8;

	switch ( old_image->format )
	{
		case GL_R8:
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

	if ( old_image != new_image )
		image_reset_data( *new_image );

	for ( size_t i = 0; i < old_image->frame.size(); i++ )
	{
		u8* old_frame = old_image->frame[ i ].data;

		if ( old_frame == nullptr )
		{
			printf( "nullptr frame?????\n" );
			return false;
		}

		// u8* new_frame     = ch_calloc< u8 >( new_width * new_height * old_image->channels, e_mem_category_image_data );

		// what am i doing wrong here? is pitch not the same as stride? it works just fine for single channel images from freeimage
		// but then when it comes to webp, or probably just 24 bpp images, it crashes
		int input_stride_in_bytes = 0;

		if ( old_image->channels == 1 )
			input_stride_in_bytes = old_image->frame[ i ].pitch;

		u8* resized_frame = (u8*)stbir_resize(
		  old_frame, old_image->frame[ i ].width, old_image->frame[ i ].height, input_stride_in_bytes,
		  nullptr, new_width, new_height, 0,
		  pixel_layout, datatype, STBIR_EDGE_CLAMP, STBIR_FILTER_DEFAULT );

		if ( !resized_frame )
			return false;

		new_image->frame[ i ].data   = resized_frame;
		new_image->frame[ i ].size   = new_width * new_height * old_image->channels;
		new_image->frame[ i ].width  = new_width;
		new_image->frame[ i ].height = new_height;
		new_image->frame[ i ].pitch  = 0;
	}

	new_image->width    = new_width;
	new_image->height   = new_height;
	new_image->channels = old_image->channels;

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
			new_image->format = GL_R8;
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
	"image",
};


static const char* g_icon_paths[] = {
	"icons" SEP_S "none.png",
	"icons" SEP_S "invalid.png",
	"icons" SEP_S "folder.png",
	"icons" SEP_S "loading.png",
	"icons" SEP_S "video.png",
	"icons" SEP_S "image.png",
};


static image_t g_icon_image[ e_icon_count ]{};
static GLuint  g_icon_texture[ e_icon_count ]{};

static_assert( ARR_SIZE( g_icon_names ) == e_icon_count );
static_assert( ARR_SIZE( g_icon_paths ) == e_icon_count );


bool icon_preload()
{
	fs::path exe_path = sys_get_exe_folder_fspath();

	for ( u8 i = 0; i < e_icon_count; i++ )
	{
		image_load_info_t load_info{};
		load_info.image = &g_icon_image[ i ];

		std::string icon_path = exe_path.string();
		icon_path += SEP_S;
		icon_path += g_icon_paths[ i ];

		if ( !image_load( icon_path, load_info ) )
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
		if ( g_icon_texture[ i ] > 0 )
			glDeleteTextures( 1, &g_icon_texture[ i ] );
	}
}


image_t* icon_get_image( e_icon icon_type )
{
	if ( icon_type >= e_icon_count )
		return &g_icon_image[ e_icon_invalid ];

	return &g_icon_image[ icon_type ];
}


ImTextureRef icon_get_imtexture( e_icon icon_type )
{
	if ( icon_type >= e_icon_count )
		return static_cast< ImTextureRef >( g_icon_texture[ e_icon_invalid ] );

	return static_cast< ImTextureRef >( g_icon_texture[ icon_type ] );
}

