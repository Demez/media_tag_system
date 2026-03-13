#include "main.h"

#include <jxl/codestream_header.h>
#include <jxl/decode.h>
#include <jxl/decode_cxx.h>
#include <jxl/resizable_parallel_runner.h>
#include <jxl/resizable_parallel_runner_cxx.h>
#include <jxl/types.h>


static void* jxl_mem_alloc( void* opaque, size_t sz )
{
	void* memory = malloc( sz );
	mem_add_item( e_mem_category_jxl, memory, sz );
	return memory;
}


static void jxl_mem_free( void* opaque, void* address )
{
	mem_free_item( e_mem_category_jxl, address );
	free( address );
}


struct LoaderJXL: public IImageLoader
{
	std::string match_ext  = ".jxl";

	LoaderJXL()
	{
		image_register_codec( this, false );
	}

	~LoaderJXL()
	{
	}

    bool check_extension( std::string_view ext ) override
    {
        // return ext == match_ext;
		return ext.ends_with( match_ext );
    }

	bool check_header( const fs::path& path ) override
	{
	#if 0
		// Read just 32 bytes of the file
		std::vector< char > header = fs_read_bytes( path, 32 );
		if ( header.size() < 12 )
		{
			return false;
		}

		JxlSignature signature = JxlSignatureCheck( (const uint8_t*)header.data(), header.size() );
		if ( signature == JXL_SIG_CODESTREAM || signature == JXL_SIG_CONTAINER )
		{
			return true;
		}

		return false;
	#endif

		return true;
	}

	bool image_load( const fs::path& path, image_load_info_t& load_info, char* data, size_t data_len ) override
	{
		JxlMemoryManager jxl_mem{};
		jxl_mem.alloc     = jxl_mem_alloc;
		jxl_mem.free      = jxl_mem_free;

		JxlDecoderPtr dec = JxlDecoderMake( &jxl_mem );
		
		if ( !dec )
		{
			printf( "Failed to make JPEG XL Decoder!\n" );
			return false;
		}

		// if ( JXL_DEC_SUCCESS != JxlDecoderSubscribeEvents( dec.get(), JXL_DEC_BASIC_INFO | JXL_DEC_COLOR_ENCODING | JXL_DEC_FULL_IMAGE ) )
		if ( JXL_DEC_SUCCESS != JxlDecoderSubscribeEvents( dec.get(), JXL_DEC_BASIC_INFO | JXL_DEC_FULL_IMAGE ) )
		{
			fprintf( stderr, "JxlDecoderSubscribeEvents failed\n" );
			return false;
		}

		// Is this image being loaded from another thread
		// NOT telling us to use multithreading here
		// in-fact, maybe the opposite, since we may have a lot of thumbnail threads
		//if ( load_info.threaded_load )
		//{
			// Multi-threaded parallel runner.
			JxlResizableParallelRunnerPtr runner = JxlResizableParallelRunnerMake( nullptr );

			if ( JXL_DEC_SUCCESS != JxlDecoderSetParallelRunner( dec.get(),
																 JxlResizableParallelRunner,
																 runner.get() ) )
			{
				fprintf( stderr, "JxlDecoderSetParallelRunner failed\n" );
				return false;
			}
		//}

		JxlBasicInfo   info{};

		// TODO: at some point in the future, i will want to support wide gamut,
		// so eventually i will need to swap to floating point precision
		// unless i can support either or somehow
		JxlPixelFormat format = { 4, JXL_TYPE_UINT8, JXL_NATIVE_ENDIAN, 0 };

		JxlDecoderSetInput( dec.get(), (const u8*)data, data_len );
		JxlDecoderCloseInput( dec.get() );

		std::vector< u8 > icc_profile{};

		// Sourced from JPEG XL decode_oneshot
		for ( ;; )
		{
			JxlDecoderStatus status = JxlDecoderProcessInput( dec.get() );

			if ( status == JXL_DEC_ERROR )
			{
				fprintf( stderr, "Decoder error\n" );
				return false;
			}
			else if ( status == JXL_DEC_NEED_MORE_INPUT )
			{
				fprintf( stderr, "Error, already provided all input\n" );
				return false;
			}
			else if ( status == JXL_DEC_BASIC_INFO )
			{
				if ( JXL_DEC_SUCCESS != JxlDecoderGetBasicInfo( dec.get(), &info ) )
				{
					fprintf( stderr, "JxlDecoderGetBasicInfo failed\n" );
					return false;
				}
				load_info.image->width  = info.xsize;
				load_info.image->height = info.ysize;

				JxlResizableParallelRunnerSetThreads( runner.get(), JxlResizableParallelRunnerSuggestThreads( info.xsize, info.ysize ) );
			}
			else if ( status == JXL_DEC_COLOR_ENCODING )
			{
				// Get the ICC color profile of the pixel data
				size_t icc_size = 0;
				if ( JXL_DEC_SUCCESS != JxlDecoderGetICCProfileSize( dec.get(), JXL_COLOR_PROFILE_TARGET_DATA, &icc_size ) )
				{
					fprintf( stderr, "JxlDecoderGetICCProfileSize failed\n" );
					return false;
				}

				icc_profile.resize( icc_size );
				if ( JXL_DEC_SUCCESS != JxlDecoderGetColorAsICCProfile(
										  dec.get(), JXL_COLOR_PROFILE_TARGET_DATA,
										  icc_profile.data(), icc_profile.size() ) )
				{
					fprintf( stderr, "JxlDecoderGetColorAsICCProfile failed\n" );
					return false;
				}
			}
			else if ( status == JXL_DEC_NEED_IMAGE_OUT_BUFFER )
			{
				size_t buffer_size = 0;
				if ( JXL_DEC_SUCCESS != JxlDecoderImageOutBufferSize( dec.get(), &format, &buffer_size ) )
				{
					fprintf( stderr, "JxlDecoderImageOutBufferSize failed\n" );
					return false;
				}

				if ( load_info.image->frame.empty() )
					load_info.image->frame.resize( 1 );

				load_info.image->frame[ 0 ] = ch_realloc< u8 >( load_info.image->frame[ 0 ], buffer_size, e_mem_category_image_data );
				load_info.image->frame_size = buffer_size * sizeof( u8 );

				memset( load_info.image->frame[ 0 ], 0, buffer_size * sizeof( u8 ) );

				void*  pixels_buffer = static_cast< void* >( load_info.image->frame[ 0 ] );

				if ( JXL_DEC_SUCCESS != JxlDecoderSetImageOutBuffer( dec.get(), &format, pixels_buffer, buffer_size ) )
				{
					fprintf( stderr, "JxlDecoderSetImageOutBuffer failed\n" );
					return false;
				}
			}
			else if ( status == JXL_DEC_FULL_IMAGE )
			{
				// Nothing to do. Do not yet return. If the image is an animation, more
				// full frames may be decoded. This example only keeps the last one.
			}
			else if ( status == JXL_DEC_SUCCESS )
			{
				// All decoding successfully finished.
				// It's not required to call JxlDecoderReleaseInput(dec.get()) here since
				// the decoder will be destroyed.
				break;
			}
			else
			{
				fprintf( stderr, "Unknown decoder status\n" );
				return false;
			}
		}

		load_info.image->format          = GL_RGBA;
		load_info.image->bytes_per_pixel = 4;
		load_info.image->channels        = 4;

		// ?
		load_info.image->bit_depth       = 4;
		load_info.image->pitch           = load_info.image->width * load_info.image->bytes_per_pixel;

		// the strdup is stupid lol
		load_info.image->image_format = util_strndup_r( load_info.image->image_format, "JPEG XL", 8 );

		return true;
	}
};


static LoaderJXL g_loader_jxl;

