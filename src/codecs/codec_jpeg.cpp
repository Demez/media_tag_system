#include "main.h"
#include "util.h"

#include <turbojpeg.h>

#include <chrono>

static tjhandle tjpg;

extern SDL_Window* g_main_window;


struct CodecJPEG: public IImageLoader
{
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

		image->data = ch_realloc( image->data, best_width * best_height * tjPixelSize[ pixelFmt ] );

		if ( !image->data )
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
          (unsigned char*)image->data,
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
			free( image->data );
			return false;
		}

		if ( load_info.threaded_load )
			tjDestroy( local_tjpg );

		if ( !image->data )
		{
			printf( "CODEC_JPEG: Decompress returned a nullptr?\n" );
			free( image->data );
			return false;
		}

		image->width     = best_width;
		image->height    = best_height;

		image->format    = GL_RGB;
		image->bit_depth = 4;  // uhhhh
		image->pitch     = pitch;

		return true;
	}

#if 0

	bool image_load_scaled( const fs::path& path, image_t* image, int area_width, int area_height ) override
	{
		tjhandle local_tjpg = tjInitDecompress();

		if ( local_tjpg == nullptr )
		{
			fprintf( stderr, "[FormatJpeg] Failed to allocate memory for jpeg decompressor.\n" );
			return false;
		}

		//auto          startTime   = std::chrono::high_resolution_clock::now();

		//auto          endTime     = std::chrono::high_resolution_clock::now();
		//float read_time                 = std::chrono::duration< float, std::chrono::seconds::period >( endTime - startTime ).count();
		//printf( "%f READ TIME\n", read_time );

		int subSamp, colorSpace;

		int           ret = tjDecompressHeader3(
          local_tjpg,
          (const unsigned char*)fileData,
          fileDataLen,
          &image->width,
          &image->height,
          &subSamp,
          &colorSpace );

        if ( ret != 0 )
		{
			fprintf( stderr, "[FormatJpeg] Failed to decompress header on image: %s\n%s\n", tjGetErrorStr2( local_tjpg ), path.string().c_str() );
			free( fileData );
			tjDestroy( local_tjpg );
			return false;
		}

		int pixelFmt     = TJPF_RGB;

		int                               scaling_factor_count;
		tjscalingfactor* scaling_factor = tjGetScalingFactors( &scaling_factor_count );

		// best fit
		int  best_width = image->width, best_height = image->height;

		bool                              scale_down = ( image->width > area_width || image->height > area_height );
		bool found      = false;

		if ( scale_down )
		{
			for ( int i = 0; i < scaling_factor_count; i++ )
			{
				int scaled_width  = TJSCALED( image->width, scaling_factor[ i ] );
				int scaled_height = TJSCALED( image->height, scaling_factor[ i ] );

				if ( scaled_width > image->width || scaled_height > image->height )
					continue;

				// if ( scaled_width <= best_width && scaled_height <= best_height )
				if ( scaled_width >= area_width && scaled_height >= area_height )
				{
					if ( !found || ( scaled_width < best_width && scaled_height < best_height ) )
					{
						best_width  = scaled_width;
						best_height = scaled_height;
						found       = true;
					}
				}
			}

			printf( "SCALED %d x %d -> %d x %d\n", image->width, image->height, best_width, best_height );
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

		image->data = ch_calloc< unsigned char >( best_width * best_height * tjPixelSize[ pixelFmt ] );

		if ( !image->data )
		{
			fprintf( stderr, "CODEC_JPEG: Failed to decompress image: %s\n%s\n", tjGetErrorStr2( local_tjpg ), path.string().c_str() );
			free( fileData );
			tjDestroy( local_tjpg );
			return false;
		}

		int pitch        = best_width * tjPixelSize[ pixelFmt ];

		ret         = tjDecompress2(
          local_tjpg,
          (const unsigned char*)fileData,
          fileDataLen,
          (unsigned char*)image->data,
          0,
          pitch,
          best_height,
          pixelFmt,
          // TJFLAG_FASTUPSAMPLE | TJFLAG_FASTDCT
          TJFLAG_ACCURATEDCT );

		free( fileData );

        if ( ret != 0 )
		{
			// fprintf( stderr, "[FormatJpeg] Failed to decompress image: %ws\n%s\n", path.c_str(), tjGetErrorStr2( tjpg ) );
			fprintf( stderr, "CODEC_JPEG: Failed to decompress image: %s\n%s\n", tjGetErrorStr2( local_tjpg ), path.string().c_str() );
			tjDestroy( local_tjpg );
			free( image->data );
			return false;
		}

		tjDestroy( local_tjpg );

		if ( !image->data )
		{
			printf( "Jpeg Decompress returned a nullptr?\n" );
			free( image->data );
			return false;
		}

		image->width = best_width;
		image->height = best_height;

		image->format = GL_RGB;
		image->bit_depth = 4;  // uhhhh
		image->pitch     = pitch;

		return true;
	}

	bool image_load( const fs::path& path, image_t* image ) override
	{
		//auto          startTime   = std::chrono::high_resolution_clock::now();

		size_t        fileDataLen = 0;
		char*         fileData    = fs_read_file( path.string().c_str(), &fileDataLen );

		//auto          endTime     = std::chrono::high_resolution_clock::now();
		//float read_time                 = std::chrono::duration< float, std::chrono::seconds::period >( endTime - startTime ).count();
		//printf( "%f READ TIME\n", read_time );

		int subSamp, colorSpace;

		int ret = tjDecompressHeader3(
			tjpg,
			(const unsigned char*)fileData,
			fileDataLen,
          &image->width,
          &image->height,
			&subSamp,
			&colorSpace
		);

        if ( ret != 0 )
		{
			fprintf( stderr, "[FormatJpeg] Failed to decompress header on image: %s\n%s\n", tjGetErrorStr2( tjpg ), path.string().c_str() );
			free( fileData );
			return false;
		}

		int pixelFmt     = TJPF_RGB;

		image->data      = ch_realloc< unsigned char >( image->data, image->width * image->height * tjPixelSize[ pixelFmt ] );

		int pitch        = image->width * tjPixelSize[ pixelFmt ];

		ret         = tjDecompress2(
          tjpg,
          (const unsigned char*)fileData,
          fileDataLen,
          (unsigned char*)image->data,
          0,
          pitch,
          0,
          pixelFmt,
          // TJFLAG_FASTUPSAMPLE | TJFLAG_FASTDCT
          TJFLAG_ACCURATEDCT );
		
		free( fileData );

        if ( ret != 0 )
		{
			// fprintf( stderr, "[FormatJpeg] Failed to decompress image: %ws\n%s\n", path.c_str(), tjGetErrorStr2( tjpg ) );
			fprintf( stderr, "[FormatJpeg] Failed to decompress image: %s\n%s\n", tjGetErrorStr2( tjpg ), path.string().c_str() );
			return false;
		}

		image->format = GL_RGB;
		// imageInfo->aFormat = FMT_RGBA8;
		image->bit_depth = 4;  // uhhhh
		image->pitch     = pitch;

		return image;
	}

	image_t* image_load( const fs::path& path ) override
	{
		size_t        fileDataLen = 0;
		char*         fileData    = fs_read_file( path.string().c_str(), &fileDataLen );

		image_t* image  = new image_t;

		int subSamp, colorSpace;

		int ret = tjDecompressHeader3(
			tjpg,
			(const unsigned char*)fileData,
			fileDataLen,
          &image->width,
          &image->height,
			&subSamp,
			&colorSpace
		);

        if ( ret != 0 )
		{
			fprintf( stderr, "[FormatJpeg] Failed to decompress header on image: %s\n%s\n", tjGetErrorStr2( tjpg ), path.string().c_str() );
			free( fileData );
			return nullptr;
		}

		int pixelFmt = TJPF_RGB;

		// srData.resize( imageInfo->aWidth * imageInfo->aHeight * tjPixelSize[ pixelFmt ] );

		image->data = ch_calloc< unsigned char >( image->width * image->height * tjPixelSize[ pixelFmt ] );

		ret = tjDecompress2(
		    tjpg,
		    (const unsigned char*)fileData,
			fileDataLen,
          (unsigned char*)image->data,
			0,
		  image->width * 4,
			0,
			pixelFmt,
			TJFLAG_FASTDCT
		);

		free( fileData );

        if ( ret != 0 )
		{
			// fprintf( stderr, "[FormatJpeg] Failed to decompress image: %ws\n%s\n", path.c_str(), tjGetErrorStr2( tjpg ) );
			fprintf( stderr, "[FormatJpeg] Failed to decompress image: %s\n%s\n", tjGetErrorStr2( tjpg ), path.string().c_str() );
			return nullptr;
		}

		image->format = GL_RGB;
		// imageInfo->aFormat = FMT_RGBA8;
		image->bit_depth = 4;  // uhhhh

		return image;
	}

#endif
};


static CodecJPEG gCodecJPEG;

