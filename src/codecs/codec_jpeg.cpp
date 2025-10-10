#include "main.h"
#include "util.h"

#include <turbojpeg.h>

#include <chrono>

static tjhandle tjpg;

extern SDL_Window* g_main_window;


class CodecJPEG: public ICodec
{
public:
	CodecJPEG()
	{
		tjpg = tjInitDecompress();

		if ( tjpg == nullptr )
		{
			fprintf( stderr, "[FormatJpeg] Failed to allocate memory for jpeg decompressor.\n" );
			return;
		}

		register_codec( this );
	}

	~CodecJPEG()
	{
	}

	// TODO: have this just be a list of valid extensions on codec register
    bool check_extension( const char* ext ) override
    {
		return strcmp( ext, "jpg" ) == 0 || strcmp( ext, "jpeg" ) == 0;
	}

	bool check_header( const fs::path& path ) override
	{
		return true;
	}

	bool image_load( const fs::path& path, image_info_t* image_info ) override
	{
		// FILE* pFile = fopen( path.c_str(), "rb" );
		// if ( !pFile )
		// 	return nullptr;

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
          &image_info->width,
          &image_info->height,
			&subSamp,
			&colorSpace
		);

        if ( ret != 0 )
		{
			fprintf( stderr, "[FormatJpeg] Failed to decompress header on image: %s\n%s\n", tjGetErrorStr2( tjpg ), path.string().c_str() );
			return false;
		}

		int pixelFmt     = TJPF_BGR;

		int                               scaling_factor_count;
		tjscalingfactor* scaling_factor = tjGetScalingFactors( &scaling_factor_count );

		// srData.resize( imageInfo->aWidth * imageInfo->aHeight * tjPixelSize[ pixelFmt ] );

		// lol

		int width, height;
		SDL_GetWindowSize( g_main_window, &width, &height );

		//	float max_size = std::max( width, height );
		//	new_width      = jpeg->width / max_size;

		// Fit image in window size
		// SDL_FRect dst_rect{};
		// float     factor[ 2 ] = { 1.f, 1.f };
		// 
		// if ( image_info->width > width )
		// 	factor[ 0 ] = (float)width / (float)image_info->width;
		// 
		// if ( image_info->height > height )
		// 	factor[ 1 ] = (float)height / (float)image_info->height;
		// 
		// float zoom_level = std::min( factor[ 0 ], factor[ 1 ] );
		// 
		// int   pitch      = ( image_info->width * zoom_level ) * tjPixelSize[ pixelFmt ];
		// 
		// float scaled_width  = image_info->width * zoom_level;
		// float scaled_height = image_info->width * zoom_level;

		// best fit
		int  best_width = image_info->width, best_height = image_info->height;

		bool scale_down = ( image_info->width > width || image_info->height > height );
		bool found      = false;

		if ( scale_down )
		{
			for ( int i = 0; i < scaling_factor_count; i++ )
			{
				int scaled_width  = TJSCALED( image_info->width, scaling_factor[ i ] );
				int scaled_height = TJSCALED( image_info->height, scaling_factor[ i ] );

				if ( scaled_width > image_info->width || scaled_height > image_info->height )
					continue;

				// if ( scaled_width <= best_width && scaled_height <= best_height )
				if ( scaled_width >= width && scaled_height >= height )
				{
					if ( !found || ( scaled_width < best_width && scaled_height < best_height ) )
					{
						best_width  = scaled_width;
						best_height = scaled_height;
						found       = true;
					}
				}

				// if ( scaled_width <= width && scaled_height <= height )
			//	if ( scaled_width >= width && scaled_height >= height )
			//	{
			//		// if ( scaled_width <= image_info->width && scaled_height <= image_info->height )
			//		{
			//			if ( scaled_width > best_width && scaled_height > best_height )
			//			{
			//				best_width  = scaled_width;
			//				best_height = scaled_height;
			//			}
			//		}
			//	}
			}

			printf( "SCALED %d x %d -> %d x %d\n", image_info->width, image_info->height, best_width, best_height );
		}
		else
		{
			best_width  = image_info->width;
			best_height = image_info->height;
		}

		if ( best_width == 0 )
			best_width = image_info->width;

		if ( best_height == 0 )
			best_height = image_info->height;

		image_info->data = ch_realloc< unsigned char >( image_info->data, best_width * best_height * tjPixelSize[ pixelFmt ] );

		int pitch        = best_width * tjPixelSize[ pixelFmt ];

		ret = tjDecompress2(
		    tjpg,
		    (const unsigned char*)fileData,
			fileDataLen,
          (unsigned char*)image_info->data,
          0,
          pitch,
          best_height,
			pixelFmt,
          // TJFLAG_FASTUPSAMPLE | TJFLAG_FASTDCT
          TJFLAG_ACCURATEDCT
		);

        if ( ret != 0 )
		{
			// fprintf( stderr, "[FormatJpeg] Failed to decompress image: %ws\n%s\n", path.c_str(), tjGetErrorStr2( tjpg ) );
			fprintf( stderr, "[FormatJpeg] Failed to decompress image: %s\n%s\n", tjGetErrorStr2( tjpg ), path.string().c_str() );
			return false;
		}

		image_info->width = best_width;
		image_info->height = best_height;

		image_info->format   = FMT_BGRA8;
		// imageInfo->aFormat = FMT_RGBA8;
		image_info->bit_depth = 4;  // uhhhh
		image_info->pitch     = pitch;

		return image_info;
	}

	image_info_t* image_load( const fs::path& path ) override
	{
		size_t        fileDataLen = 0;
		char*         fileData    = fs_read_file( path.string().c_str(), &fileDataLen );

		image_info_t* image_info  = new image_info_t;

		int subSamp, colorSpace;

		int ret = tjDecompressHeader3(
			tjpg,
			(const unsigned char*)fileData,
			fileDataLen,
          &image_info->width,
          &image_info->height,
			&subSamp,
			&colorSpace
		);

        if ( ret != 0 )
		{
			fprintf( stderr, "[FormatJpeg] Failed to decompress header on image: %s\n%s\n", tjGetErrorStr2( tjpg ), path.c_str() );
			return nullptr;
		}

		int pixelFmt = TJPF_RGBA;

		// srData.resize( imageInfo->aWidth * imageInfo->aHeight * tjPixelSize[ pixelFmt ] );

		image_info->data = ch_calloc< unsigned char >( image_info->width * image_info->height * tjPixelSize[ pixelFmt ] );

		ret = tjDecompress2(
		    tjpg,
		    (const unsigned char*)fileData,
			fileDataLen,
          (unsigned char*)image_info->data,
			0,
		  image_info->width * 4,
			0,
			pixelFmt,
			TJFLAG_FASTDCT
		);

        if ( ret != 0 )
		{
			// fprintf( stderr, "[FormatJpeg] Failed to decompress image: %ws\n%s\n", path.c_str(), tjGetErrorStr2( tjpg ) );
			fprintf( stderr, "[FormatJpeg] Failed to decompress image: %s\n%s\n", tjGetErrorStr2( tjpg ), path.c_str() );
			return nullptr;
		}

		image_info->format   = FMT_BGRA8;
		// imageInfo->aFormat = FMT_RGBA8;
		image_info->bit_depth = 4;  // uhhhh

		return image_info;
	}
};


static CodecJPEG gCodecJPEG;

