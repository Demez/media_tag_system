#include "main.h"


void gl_check_error();


// =================================================================
// Palette Creation


void render_upload_palette( GLuint& palette_texture, GLuint& palette_trans, image_palette_t& palette )
{
	palette_trans = palette.trans_index;

	glBindTexture( GL_TEXTURE_1D, palette_texture );

	// disable wrapping
	glTexParameteri( GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_1D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );

	//glTexImage1D( GL_TEXTURE_1D, 0, GL_RGBA8, palette.count, 0, GL_RGBA, GL_UNSIGNED_BYTE, palette.elem );
	//glTexImage1D( GL_TEXTURE_1D, 0, GL_RGBA8UI, palette.count, 0, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, palette.elem );
	glTexImage1D( GL_TEXTURE_1D, 0, GL_RGB8UI, palette.count, 0, GL_RGB_INTEGER, GL_UNSIGNED_BYTE, palette.elem );

	glTexParameteri( GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
	glTexParameteri( GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
}


size_t get_palette_count( image_t* image, size_t frame_count )
{
	size_t palette_count = 0;

	if ( image->flags & e_image_flag_has_palette )
		palette_count += 1;

	if ( image->flags & e_image_flag_has_local_palettes )
		palette_count += frame_count;

	return palette_count;
}


void render_upload_palettes( uploaded_textures_t& textures, image_t* image, size_t frame_count )
{
	size_t palette_count = get_palette_count( image, frame_count );

	textures.palette       = ch_calloc< GLuint >( palette_count, e_mem_category_gl_texture_data );
	textures.palette_trans = ch_calloc< GLuint >( palette_count, e_mem_category_gl_texture_data );

	// Create 1D Palette Texture
	glGenTextures( palette_count, textures.palette );

	if ( image->flags & e_image_flag_has_palette )
	{
		render_upload_palette( textures.palette[ 0 ], textures.palette_trans[ 0 ], image->palette );
		glObjectLabel( GL_TEXTURE, textures.palette[ 0 ], GL_DBG_NAME( "Palette Texture" ) );
	}

	if ( image->flags & e_image_flag_has_local_palettes )
	{
		size_t j = 0;
		if ( image->flags & e_image_flag_has_palette )
			j = 1;

		for ( size_t i = 0; i < frame_count; i++ )
		{
			render_upload_palette( textures.palette[ i + j ], textures.palette_trans[ i + j ], image->frame[ i ].palette );
			glObjectLabel( GL_TEXTURE, textures.palette[ i + j ], GL_DBG_NAME( "Palette Local Texture" ) );
		}
	}
}


// =================================================================
// Texture Uploading


void gl_update_texture( GLuint texture, image_t* image, size_t frame_i )
{
	glBindTexture( GL_TEXTURE_2D, texture );
	glObjectLabel( GL_TEXTURE, texture, GL_DBG_NAME( "Image Frame" ) );

	// disable wrapping
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );

	// Setup filtering parameters for display
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );  // downscaling image
	// glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );  // upscaling image

	// ------------------------------------------------------------------------------------------------------
	// https://stackoverflow.com/a/58927549
	//
	// The jpg image consists of 3 color channels (GL_RGB) and stbi_load returns a tightly packed image.
	// The number of bytes of the of the image buffer (img) is w * h * 3.
	//
	// By default OpenGL assumes that the start of each row of an image is aligned 4 bytes.
	// This is because the GL_UNPACK_ALIGNMENT parameter by default is 4.
	// Since the image has 3 color channels, and is tightly packed the start of a row is possibly misaligned.
	// So the size of the image buffer is assumed to by aligne(w*3, 4) * h.
	//
	// The crash is caused, because glTexImage2D. access the buffer out of bounds.
	//
	// Change the the GL_UNPACK_ALIGNMENT parameter to 1, before specifying the two-dimensional texture image (glTexImage2D):
	//
	// ------------------------------------------------------------------------------------------------------
	// https://registry.khronos.org/OpenGL-Refpages/es3.0/html/glPixelStorei.xhtml

	int width  = image->frame[ frame_i ].width;
	int height = image->frame[ frame_i ].height;

	if ( width == 0 )
		width = image->width;

	if ( height == 0 )
		height = image->height;


	// number of pixels in a row
	//glPixelStorei( GL_UNPACK_ROW_LENGTH, width );
	if ( image->frame[ frame_i ].pitch > 0 )
	{
		glPixelStorei( GL_UNPACK_ROW_LENGTH, image->frame[ frame_i ].pitch / image->channels );
		glPixelStorei( GL_UNPACK_ALIGNMENT, 4 );
	}
	else
	{
		glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
	}

	GLint  internal_format = image->format;
	GLint  format          = image->format;
	GLenum type            = GL_UNSIGNED_BYTE;

	bool   use_unsigned    = image->flags & e_image_flag_has_palette || image->flags & e_image_flag_has_local_palettes;

	// GL_RGB  - 6407
	// GL_RGBA - 6408

	switch ( image->format )
	{
		default:
			break;

		case GL_RGBA16:
			internal_format = format = GL_RGBA16;
			break;

		case GL_R16UI:
			internal_format = GL_RGB16;
			format          = GL_RED;
			break;

		case GL_R16I:
			internal_format = GL_RGB16;
			format          = GL_RED;
			break;

		case GL_RGB32F:
			internal_format = GL_RGB32F;
			format          = GL_RGB;
			break;

		case GL_RGBA32F:
			internal_format = GL_RGBA32F;
			format          = GL_RGBA;
			break;

		case GL_R8:
		{
			if ( use_unsigned )
			{
				internal_format = GL_R8UI;
				format = GL_RED_INTEGER;
			}
			else
			{
				internal_format = GL_R8;
				format          = GL_RED;
			}

			break;
		}
	}

	switch ( image->format )
	{
		default:
			type = GL_UNSIGNED_BYTE;
			break;

		case GL_RGBA16:
		case GL_R16UI:
			type = GL_UNSIGNED_SHORT;
			break;

		case GL_R16I:
			type = GL_SHORT;
			break;

		case GL_RGB32F:
		case GL_RGBA32F:
			type = GL_FLOAT;
			break;
	}

	glTexImage2D( GL_TEXTURE_2D, 0, internal_format, width, height, 0, format, type, image->frame[ frame_i ].data );

	auto err = glGetError();

	if ( err != 0 )
		printf( "GL Error: %d\n", err );

	glPixelStorei( GL_UNPACK_ALIGNMENT, 4 );

	if ( image->frame[ frame_i ].pitch > 0 )
		glPixelStorei( GL_UNPACK_ROW_LENGTH, 0 );
}


void gl_update_textures( uploaded_textures_t& textures, image_t* image, size_t frame_count )
{
	// free resources
	size_t palette_count = get_palette_count( image, frame_count );

	if ( palette_count > 0 != textures.palette_count && textures.palette )
		glDeleteTextures( static_cast< int >( textures.palette_count ), textures.palette );
	
	ch_free( e_mem_category_gl_texture_data, textures.palette );
	ch_free( e_mem_category_gl_texture_data, textures.palette_trans );

	if ( textures.count != frame_count )
	{
		if ( textures.frame )
			glDeleteTextures( static_cast< int >( textures.count ), textures.frame );
	}

	if ( !image )
		return;

	if ( textures.count != frame_count )
	{
		textures.frame = ch_realloc< GLuint >( textures.frame, frame_count, e_mem_category_gl_texture_data, 2 );
		memset( textures.frame, 0, sizeof( GLuint ) * frame_count );

		glGenTextures( static_cast< int >( frame_count ), textures.frame );
		textures.count = frame_count;
	}

	for ( size_t i = 0; i < frame_count; i++ )
	{
		gl_update_texture( textures.frame[ i ], image, i );
	}

	if ( image->flags & e_image_flag_has_palette || image->flags & e_image_flag_has_local_palettes )
		render_upload_palettes( textures, image, frame_count );
}


void gl_free_textures( uploaded_textures_t& textures )
{
	if ( textures.frame )
		glDeleteTextures( static_cast< int >( textures.count ), textures.frame );

	if ( textures.palette )
		glDeleteTextures( static_cast< int >( textures.count ), textures.palette );

	ch_free( e_mem_category_gl_texture_data, textures.frame );
	ch_free( e_mem_category_gl_texture_data, textures.palette );
	ch_free( e_mem_category_gl_texture_data, textures.palette_trans );

	textures.frame         = nullptr;
	textures.palette       = nullptr;
	textures.palette_trans = nullptr;
	textures.count         = 0;
}

