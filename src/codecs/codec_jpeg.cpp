#include "main.h"
#include "util.h"

#include <turbojpeg.h>

#include <chrono>


struct CodecJPEG: public IImageLoader
{
	tjhandle tjpg;

	CodecJPEG()
	{
		tjpg = tjInitDecompress();

		if ( tjpg == nullptr )
		{
			fprintf( stderr, "[FormatJpeg] Failed to allocate memory for jpeg decompressor.\n" );
			return;
		}

		image_register_codec( this );
	}

	~CodecJPEG()
	{
		tjDestroy( tjpg );
	}

	// TODO: have this just be a list of valid extensions on codec register
    bool check_extension( std::string_view ext ) override
    {
		return ext == ".jpg" || ext == ".jpeg";
	}

	bool check_header( const fs::path& path ) override
	{
		return true;
	}

	bool image_load( const fs::path& path, image_load_info_t& load_info, char* data, size_t data_len ) override
	{
		tjhandle local_tjpg = tjpg;

		if ( load_info.threaded_load )
		{
			local_tjpg = tjInitDecompress();

			if ( local_tjpg == nullptr )
			{
				fprintf( stderr, "CODEC_JPEG: Failed to allocate memory for jpeg decompressor.\n" );
				return false;
			}
		}

		image_t* image = load_info.image;

		int subSamp, colorSpace;

		int ret = tjDecompressHeader3(
		  local_tjpg,
		  (const unsigned char*)data,
		  data_len,
		  &image->width,
		  &image->height,
		  &subSamp,
		  &colorSpace );

		if ( ret != 0 )
		{
			fprintf( stderr, "CODEC_JPEG: Failed to decompress header on image: %s\n%s\n", tjGetErrorStr2( local_tjpg ), path.string().c_str() );
			if ( load_info.threaded_load )
				tjDestroy( local_tjpg );

			return false;
		}

		int              pixelFmt = TJPF_RGB;

		int              scaling_factor_count;
		tjscalingfactor* scaling_factor = tjGetScalingFactors( &scaling_factor_count );

		// best fit
		int              best_width = image->width, best_height = image->height;

		bool             scale_down = ( load_info.target_size.x > 0 && load_info.target_size.y > 0 ) && ( image->width > load_info.target_size.x || image->height > load_info.target_size.y );
		bool             found      = false;

		if ( scale_down )
		{
			for ( int i = 0; i < scaling_factor_count; i++ )
			{
				int scaled_width  = TJSCALED( image->width, scaling_factor[ i ] );
				int scaled_height = TJSCALED( image->height, scaling_factor[ i ] );

				if ( scaled_width > image->width || scaled_height > image->height )
					continue;

				// if ( scaled_width <= best_width && scaled_height <= best_height )
				if ( scaled_width >= load_info.target_size.x && scaled_height >= load_info.target_size.y )
				{
					if ( !found || ( scaled_width < best_width && scaled_height < best_height ) )
					{
						best_width  = scaled_width;
						best_height = scaled_height;
						found       = true;
					}
				}
			}

			printf( "CODEC_JPEG: SCALED %d x %d -> %d x %d\n", image->width, image->height, best_width, best_height );
		}
		else
		{
			best_width  = image->width;
			best_height = image->height;
		}

		if ( best_width == 0 )
			best_width = image->width;

		if ( best_height == 0 )
			best_height = image->height;

		// one frame
		// image->frame      = ch_realloc( image->frame, 1 );
		image->frame.resize( 1 );
		image->frame[ 0 ] = ch_realloc( image->frame[ 0 ], best_width * best_height * tjPixelSize[ pixelFmt ] );

		if ( !image->frame[ 0 ] )
		{
			fprintf( stderr, "CODEC_JPEG: Failed to decompress image: %s\n%s\n", tjGetErrorStr2( local_tjpg ), path.string().c_str() );
			if ( load_info.threaded_load )
				tjDestroy( local_tjpg );
			return false;
		}

		int pitch = best_width * tjPixelSize[ pixelFmt ];

		ret       = tjDecompress2(
          local_tjpg,
          (const unsigned char*)data,
          data_len,
          (unsigned char*)image->frame[ 0 ],
          0,
          pitch,
          best_height,
          pixelFmt,
          load_info.load_quick ? TJFLAG_FASTUPSAMPLE | TJFLAG_FASTDCT : TJFLAG_ACCURATEDCT
		);

		if ( ret != 0 )
		{
			fprintf( stderr, "CODEC_JPEG: Failed to decompress image: %s\n%s\n", tjGetErrorStr2( local_tjpg ), path.string().c_str() );
			if ( load_info.threaded_load )
				tjDestroy( local_tjpg );
			free( image->frame[ 0 ] );
			return false;
		}

		if ( load_info.threaded_load )
			tjDestroy( local_tjpg );

		if ( !image->frame[ 0 ] )
		{
			printf( "CODEC_JPEG: Decompress returned a nullptr?\n" );
			free( image->frame[ 0 ] );
			return false;
		}

		image->width     = best_width;
		image->height    = best_height;

		image->format    = GL_RGB;
		image->bit_depth = 4;  // uhhhh
		image->pitch     = pitch;

		return true;
	}
};


// static CodecJPEG gCodecJPEG;

