#include "main.h"
#include "util.h"

#include <jxl/codestream_header.h>
#include <jxl/color_encoding.h>
#include <jxl/encode.h>
#include <jxl/encode_cxx.h>
#include <jxl/thread_parallel_runner.h>
#include <jxl/thread_parallel_runner_cxx.h>
#include <jxl/types.h>


static void* jxl_thumbnail_mem_alloc( void* opaque, size_t sz )
{
	void* memory = malloc( sz );
	mem_add_item( e_mem_category_jxl_thumbnail, memory, sz );
	return memory;
}


static void jxl_thumbnail_mem_free( void* opaque, void* address )
{
	mem_free_item( e_mem_category_jxl_thumbnail, address );
	free( address );
}


bool thumbnail_save( image_t& image, const std::string& output )
{
	JxlMemoryManager jxl_mem{};
	jxl_mem.alloc = jxl_thumbnail_mem_alloc;
	jxl_mem.free  = jxl_thumbnail_mem_free;

	std::vector< u8 > jxl_file;

	auto              enc          = JxlEncoderMake( &jxl_mem );

	// yes, 1 thread, since this is being called from a multithreaded thumbnail system
	// auto runner = JxlThreadParallelRunnerMake( /*memory_manager=*/nullptr, 1 );
	// 
	// if ( JXL_ENC_SUCCESS != JxlEncoderSetParallelRunner( enc.get(),
	//                                                      JxlThreadParallelRunner,
	//                                                      runner.get() ) )
	// {
	// 	fprintf( stderr, "JxlEncoderSetParallelRunner failed\n" );
	// 	return false;
	// }

	JxlPixelFormat pixel_format = { image.channels, JXL_TYPE_UINT8, JXL_NATIVE_ENDIAN, 0 };

	JxlBasicInfo   basic_info;
	JxlEncoderInitBasicInfo( &basic_info );
	basic_info.xsize                    = image.width;
	basic_info.ysize                    = image.height;
	basic_info.bits_per_sample          = 4;
	basic_info.exponent_bits_per_sample = 0;
	basic_info.uses_original_profile    = JXL_FALSE;

	if ( image.channels == 4 )
	{
		basic_info.alpha_bits         = 1;
		basic_info.num_extra_channels = 1;
	}

	JxlEncoderStatus status = JxlEncoderSetBasicInfo( enc.get(), &basic_info );

	if ( status != JXL_ENC_SUCCESS )
	{
		JxlEncoderError error = JxlEncoderGetError( enc.get() );
		fprintf( stderr, "JxlEncoderSetBasicInfo failed\n" );
		return false;
	}

	JxlExtraChannelInfo extra_info{};
	JxlEncoderInitExtraChannelInfo( JXL_CHANNEL_ALPHA, &extra_info );
	extra_info.bits_per_sample = 4;

	// status = JxlEncoderSetExtraChannelInfo( enc.get(), 3, &extra_info );
	// 
	// if ( status != JXL_ENC_SUCCESS )
	// {
	// 	JxlEncoderError error = JxlEncoderGetError( enc.get() );
	// 	fprintf( stderr, "JxlEncoderSetExtraChannelInfo failed\n" );
	// 	return false;
	// }

	JxlColorEncoding color_encoding = {};
	JXL_BOOL         is_gray        = TO_JXL_BOOL( pixel_format.num_channels < 3 );
	JxlColorEncodingSetToSRGB( &color_encoding, is_gray );
	
	if ( JXL_ENC_SUCCESS != JxlEncoderSetColorEncoding( enc.get(), &color_encoding ) )
	{
		fprintf( stderr, "JxlEncoderSetColorEncoding failed\n" );
		return false;
	}

	JxlEncoderFrameSettings* frame_settings = JxlEncoderFrameSettingsCreate( enc.get(), nullptr );

	if ( !frame_settings )
	{
		fprintf( stderr, "failed to create JxlEncoderFrameSettings\n" );
		return false;
	}

	status = JxlEncoderSetFrameDistance( frame_settings, app::config.thumbnail_jxl_distance );
	status = JxlEncoderFrameSettingsSetOption( frame_settings, JXL_ENC_FRAME_SETTING_EFFORT, app::config.thumbnail_jxl_effort );

	status = JxlEncoderAddImageFrame( frame_settings, &pixel_format, image.frame[ 0 ].data, image.frame[ 0 ].size );

	if ( status != JXL_ENC_SUCCESS )
	{
		JxlEncoderError error = JxlEncoderGetError( enc.get() );
		fprintf( stderr, "JxlEncoderAddImageFrame failed\n" );
		return false;
	}

	JxlEncoderCloseInput( enc.get() );

	// jxl_file.reserve( 1024 * 1024 );
	jxl_file.resize( 1024 * 10 );
	uint8_t*         next_out       = jxl_file.data();
	size_t           avail_out      = jxl_file.size() - ( next_out - jxl_file.data() );

	JxlEncoderStatus process_result = JXL_ENC_NEED_MORE_OUTPUT;

	while ( process_result == JXL_ENC_NEED_MORE_OUTPUT )
	{
		process_result = JxlEncoderProcessOutput( enc.get(), &next_out, &avail_out );

		if ( process_result == JXL_ENC_NEED_MORE_OUTPUT )
		{
			size_t offset = next_out - jxl_file.data();
			jxl_file.resize( jxl_file.size() * 2 );
			next_out  = jxl_file.data() + offset;
			avail_out = jxl_file.size() - offset;
		}
	}
	jxl_file.resize( next_out - jxl_file.data() );
	if ( JXL_ENC_SUCCESS != process_result )
	{
		fprintf( stderr, "JxlEncoderProcessOutput failed\n" );
		return false;
	}

	fs_write_file( output.c_str(), (char*)jxl_file.data(), jxl_file.size() );

	return true;
}

