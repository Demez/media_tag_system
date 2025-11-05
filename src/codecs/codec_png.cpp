#include "main.h"

#include <spng.h>

#if 1

struct LoaderPNG: public IImageLoader
{
	std::string match_ext  = ".png";

	LoaderPNG()
	{
		image_register_codec( this );
	}

	~LoaderPNG()
	{
	}

    bool check_extension( std::string_view ext ) override
    {
        return ext == match_ext;
    }

	bool check_header( const fs::path& path ) override
	{
#if 0
		spng_ctx* ctx = spng_ctx_new( 0 );
		if ( !ctx )
		{
			fprintf( stderr, "LOADER_PNG Failed to allocate memory for png context.\n" );
			return false;
		}

		FILE* pFile = _wfopen( path.c_str(), L"rb" );
		if ( !pFile )
			return false;

		struct spng_ihdr ihdr;
		int              err = 0;

		err                  = spng_set_png_file( ctx, pFile );
		if ( err != 0 )
		{
			spng_ctx_free( ctx );
			fclose( pFile );
			return false;
		}

		err = spng_get_ihdr( ctx, &ihdr );
		if ( err != 0 )
		{
			spng_ctx_free( ctx );
			fclose( pFile );
			return false;
		}

		spng_ctx_free( ctx );
		fclose( pFile );
#endif

		return true;
	}

	bool image_load( const fs::path& path, image_load_info_t& load_info, char* data, size_t data_len ) override
	{
        spng_ctx *ctx = spng_ctx_new( 0 );
        if( !ctx )
        {
            fprintf( stderr, "LOADER_PNG: Failed to allocate memory for png context.\n" );
            return false;
        }

        struct spng_ihdr ihdr;
		int              err = 0;

        err = spng_set_png_buffer( ctx, data, data_len );
		if ( err != 0 )
		{
			printf( "LOADER_PNG: Failed to set image: %s\n", spng_strerror( err ) );
			spng_ctx_free( ctx );
			return false;
		}

        err = spng_get_ihdr( ctx, &ihdr );
		if ( err != 0 )
		{
			printf( "LOADER_PNG: Failed to get ihdr: %s\n", spng_strerror( err ) );
			spng_ctx_free( ctx );
			return false;
		}

		// look into RGBA16?
        //spng_format pngFmt = ihdr.bit_depth == 16 ? SPNG_FMT_RGBA16 : SPNG_FMT_RGB8;
        spng_format pngFmt = SPNG_FMT_RGB8;
		int decode_flags{};

        if ( ihdr.color_type == SPNG_COLOR_TYPE_TRUECOLOR_ALPHA || ihdr.color_type == SPNG_COLOR_TYPE_GRAYSCALE_ALPHA )
		{
			//pngFmt = ihdr.bit_depth == 16 ? SPNG_FMT_RGBA16 : SPNG_FMT_RGBA8;
			decode_flags |= SPNG_DECODE_TRNS;
		}

        size_t size;
		err = spng_decoded_image_size( ctx, pngFmt, &size );
		if ( err != 0 )
		{
			printf( "LOADER_PNG Failed to decode image size: %s\n", spng_strerror( err ) );
			spng_ctx_free( ctx );
			return false;
		}

		// one frame
		// load_info.image->frame      = ch_realloc( load_info.image->frame, 1 );
		load_info.image->frame.resize( 1 );
		load_info.image->frame[ 0 ] = ch_realloc( load_info.image->frame[ 0 ], size );

		err                         = spng_decode_image( ctx, load_info.image->frame[ 0 ], size, pngFmt, decode_flags );
		if ( err != 0 )
		{
			printf( "LOADER_PNG: Failed to decode image: %s\n", spng_strerror( err ) );
			spng_ctx_free( ctx );
			return false;
		}
		
		load_info.image->width     = ihdr.width;
		load_info.image->height    = ihdr.height;
		// load_info.image->format    = GL_RGBA;
		load_info.image->bit_depth = ihdr.bit_depth;
		load_info.image->pitch     = 8;

		switch ( pngFmt )
		{
			case SPNG_FMT_RGB8:
				load_info.image->format = GL_RGB;
				break;

			case SPNG_FMT_RGBA8:
				load_info.image->format = GL_RGBA;
				break;

			case SPNG_FMT_RGBA16:
				load_info.image->format = GL_RGBA16;
				break;
		}

        spng_ctx_free( ctx );

		return true;
	}
};


//static LoaderPNG gPNG;

#endif