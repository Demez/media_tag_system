#include "main.h"

// FFMpeg libraries need to linked as extern C, otherwise you get a bunch of linker errors as a result
// I think there was some other library i got the same error with, hmm
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/mem.h>
#include <libavutil/opt.h>
#include <libavutil/time.h>
#include <libavutil/imgutils.h>
}


#define INBUF_SIZE 4096


struct libav_data_t
{
	AVFormatContext* fmt_ctx;
	AVCodecContext*  dec_ctx;
	AVFilterContext* buffersink_ctx;
	AVFilterContext* buffersrc_ctx;
	AVFilterGraph*   filter_graph;
	int              video_stream_index = -1;
	int64_t          last_pts           = AV_NOPTS_VALUE;

	AVPixelFormat    dst_format;
};


constexpr AVPixelFormat get_dst_av_format( AVPixelFormat src_format )
{
	switch ( src_format )
	{
		default:
			// return AV_PIX_FMT_RGB24;
			return AV_PIX_FMT_BGR24;
			// return AV_PIX_FMT_BGRA;

		case AV_PIX_FMT_YA16BE:
		case AV_PIX_FMT_YA16LE:
		case AV_PIX_FMT_YUVA422P12BE:
		case AV_PIX_FMT_YUVA422P12LE:
		case AV_PIX_FMT_YUVA444P12BE:
		case AV_PIX_FMT_YUVA444P12LE:
		case AV_PIX_FMT_YAF32BE:
		case AV_PIX_FMT_YAF32LE:
		case AV_PIX_FMT_YAF16BE:
		case AV_PIX_FMT_YAF16LE:
			return AV_PIX_FMT_BGRA;
	}
}


constexpr GLenum get_dst_gl_format( AVPixelFormat src_format )
{
	switch ( src_format )
	{
		default:
			printf( "UNHANDLED AVPixelFormat TO GL FORMAT CONVERSION!\n" );
			return GL_RGB;

		case AV_PIX_FMT_BGR24:
			return GL_BGR;

		case AV_PIX_FMT_RGB24:
			return GL_RGB;

		case AV_PIX_FMT_BGRA:
			return GL_BGRA;

		case AV_PIX_FMT_RGBA:
			return GL_RGBA;
	}
}

constexpr int get_dst_channels( GLenum src_format )
{
	switch ( src_format )
	{
		default:
		case GL_BGR:
		case GL_RGB:
			return 3;

		case GL_BGRA:
		case GL_RGBA:
			return 4;
	}
}


bool thumbnail_libav_init()
{
	return true;
}


void thumbnail_libav_shutdown()
{
}

#if 1


static int open_input_file( libav_data_t& avdata, const fs::path_char* filename )
{
	const AVCodec* dec{};
	int            ret = 0;

	std::string    filename_utf8{};
	sys_path_to_string( filename, filename_utf8 );

	if ( ( ret = avformat_open_input( &avdata.fmt_ctx, filename_utf8.c_str(), NULL, NULL ) ) < 0 )
	{
		av_log( NULL, AV_LOG_ERROR, "Cannot open input file\n" );
		return ret;
	}

	if ( ( ret = avformat_find_stream_info( avdata.fmt_ctx, NULL ) ) < 0 )
	{
		av_log( NULL, AV_LOG_ERROR, "Cannot find stream information\n" );
		return ret;
	}

	//const AVDictionaryEntry* tag = NULL;
	//while ( ( tag = av_dict_iterate( avdata.fmt_ctx->metadata, tag ) ) )
	//	printf( "%s=%s\n", tag->key, tag->value );

	/* dump input information to stderr */
	//av_dump_format( avdata.fmt_ctx, 0, filename_utf8.c_str(), 0 );

	/* select the video stream */
	ret = av_find_best_stream( avdata.fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &dec, 0 );
	if ( ret < 0 )
	{
		av_log( NULL, AV_LOG_ERROR, "Cannot find a video stream in the input file\n" );
		return ret;
	}

	avdata.video_stream_index = ret;

	/* create decoding context */
	avdata.dec_ctx            = avcodec_alloc_context3( dec );
	if ( !avdata.dec_ctx )
	{
		printf( "FFMPEG: Failed to allocate decoding context\n" );
		return AVERROR( ENOMEM );
	}

	avcodec_parameters_to_context( avdata.dec_ctx, avdata.fmt_ctx->streams[ avdata.video_stream_index ]->codecpar );

	/* init the video decoder */
	if ( ( ret = avcodec_open2( avdata.dec_ctx, dec, NULL ) ) < 0 )
	{
		printf( "FFMPEG: Failed to open decoder\n" );
		av_log( NULL, AV_LOG_ERROR, "Cannot open video decoder\n" );
		return ret;
	}

	return 0;
}


static bool write_image_data( libav_data_t& avdata, AVFrame* av_frame, const AVPacket* av_packet, image_t* image )
{
	AVPixelFormat dst_fmt    = get_dst_av_format( avdata.dec_ctx->pix_fmt );

	int           new_width  = avdata.dec_ctx->width;
	int           new_height = avdata.dec_ctx->height;

	util_downscale_resolution( avdata.dec_ctx->width, avdata.dec_ctx->height, app::config.thumbnail_size, new_width, new_height );

	// https://stackoverflow.com/questions/71422496/how-to-save-avframe-as-image-in-c-using-ffmpeg
	// create SWS context for converting from decode pixel format (like YUV420) to BGRA
	struct SwsContext* sws_ctx = NULL;
	sws_ctx                    = sws_getContext( avdata.dec_ctx->width,
	                                             avdata.dec_ctx->height,
	                                             avdata.dec_ctx->pix_fmt,
	                                             new_width,
	                                             new_height,
	                                             dst_fmt,
	                                             SWS_BICUBIC,
	                                             NULL,
	                                             NULL,
	                                             NULL );

	AVFrame* dst_frame         = av_frame_alloc();

	if ( !dst_frame )
	{
		sws_freeContext( sws_ctx );
		av_frame_unref( av_frame );

		printf( "FFMPEG: Failed to allocate converted frame!\n" );
		return false;
	}

	dst_frame->format = dst_fmt;
	dst_frame->width  = new_width;
	dst_frame->height = new_height;

	image->width      = dst_frame->width;
	image->height     = dst_frame->height;

	int sts           = av_frame_get_buffer( dst_frame, 0 );

	if ( sts < 0 )
	{
		sws_freeContext( sws_ctx );
		av_frame_free( &dst_frame );
		av_frame_unref( av_frame );

		printf( "FFMPEG: Failed to get frame status!\n" );
		return false;
	}

	// convert from input format (e.g YUV420) to BGRX
	sts = sws_scale( sws_ctx,
	                 av_frame->data,
	                 av_frame->linesize,
	                 0,
	                 av_frame->height,
	                 dst_frame->data,
	                 dst_frame->linesize );

	if ( sts < 0 )
	{
		sws_freeContext( sws_ctx );
		av_frame_free( &dst_frame );
		av_frame_unref( av_frame );

		printf( "FFMPEG: Failed to convert frame format!\n" );
		return false;
	}

	image->format   = get_dst_gl_format( dst_fmt );
	image->channels = get_dst_channels( image->format );

	image->frame.resize( 1 );
	image_frame_t& frame = image->frame[ 0 ];

	frame.width          = dst_frame->width;
	frame.height         = dst_frame->height;

	// frame.size           = image->width * image->height * image->channels;
	frame.size           = dst_frame->linesize[ 0 ] * frame.height;
	frame.data           = ch_calloc< u8 >( frame.size, e_mem_category_image_data );
	frame.pitch          = dst_frame->linesize[ 0 ];

	for ( size_t i = 0; i < dst_frame->height; i++ )
	{
		memcpy( frame.data + i * dst_frame->linesize[ 0 ], dst_frame->data[ 0 ] + i * dst_frame->linesize[ 0 ], dst_frame->linesize[ 0 ] );
	}

	sws_freeContext( sws_ctx );
	av_frame_free( &dst_frame );

	return true;
}


static int decode_packet( libav_data_t& avdata, AVFrame* av_frame, const AVPacket* av_packet, image_t* image )
{
	int ret = 0;

	// submit the packet to the decoder
	ret     = avcodec_send_packet( avdata.dec_ctx, av_packet );
	if ( ret < 0 )
	{
		if ( ret == AVERROR_EOF )
			return AVERROR_EOF;

		char averr_str[ AV_ERROR_MAX_STRING_SIZE ];
		fprintf( stderr, "Error submitting a packet for decoding (%s)\n", av_make_error_string( averr_str, AV_ERROR_MAX_STRING_SIZE, ret ) );
		return -1;
	}

	// get all the available frames from the decoder
	while ( ret >= 0 )
	{
		ret = avcodec_receive_frame( avdata.dec_ctx, av_frame );
		if ( ret < 0 )
		{
			// those two return values are special and mean there is no output
			// frame available, but there were no errors during decoding
			if ( ret == AVERROR_EOF )
				return AVERROR_EOF;

			if ( ret == AVERROR( EAGAIN ) )
				return 0;

			char averr_str[ AV_ERROR_MAX_STRING_SIZE ];
			fprintf( stderr, "Error during decoding (%s)\n",  av_make_error_string( averr_str, AV_ERROR_MAX_STRING_SIZE, ret ) );
			return -1;
		}

		// av_frame_unref( av_frame );

		return 1;
	}

	return -1;
}


#endif


// seek to closest keyframe to perecentage of file
static bool seek_to_percentage( libav_data_t& avdata, double ratio, int64_t& out_ts )
{
	AVStream* video_stream = avdata.fmt_ctx->streams[ avdata.video_stream_index ];

	int64_t   duration     = avdata.fmt_ctx->duration;
	int64_t   start_time   = avdata.fmt_ctx->start_time;
	int       stream_idx   = -1;

	// if ( avdata.fmt_ctx->duration <= 0 )
	{
		// no duration found
		if ( video_stream->duration <= 0 )
			return false;

		// fallback to video duration
		duration   = video_stream->duration;
		start_time = video_stream->start_time;
		stream_idx = avdata.video_stream_index;
	}

	int64_t start = ( start_time != AV_NOPTS_VALUE ) ? start_time : 0;
	out_ts        = start + static_cast< int64_t >( duration * ratio );

	// backwards flag allows to seek the nearest keyframe behind the timestamp we want
	// then we can properly decode forward to the frame we want
	// if ( av_seek_frame( avdata.fmt_ctx, stream_idx, seek_ts, AVSEEK_FLAG_ANY ) < 0 )
	if ( av_seek_frame( avdata.fmt_ctx, stream_idx, out_ts, AVSEEK_FLAG_BACKWARD ) < 0 )
		return false;

	// clears old frames out
	avcodec_flush_buffers( avdata.dec_ctx );
	return true;
}


int64_t frame_get_timestamp( const AVFrame* frame )
{
	if ( frame->best_effort_timestamp != AV_NOPTS_VALUE )
		return frame->best_effort_timestamp;

	return frame->pts;
}


bool thumbnail_loader_get_video_thumbnail_from_disk( thumbnail_t* thumbnail, int thread_id )
{
	libav_data_t avdata{};

	int          ret = open_input_file( avdata, thumbnail->path );

	if ( ret != 0 )
		return false;

	int64_t seek_ts = 0;
	if ( !seek_to_percentage( avdata, 0.3, seek_ts ) )
	{
		printf( "Failed to seek file\n" );
	}

	AVFrame*     av_frame  = av_frame_alloc();
	AVPacket*    av_packet = av_packet_alloc();

	if ( !av_frame || !av_packet )
	{
		avcodec_free_context( &avdata.dec_ctx );
		avformat_close_input( &avdata.fmt_ctx );

		printf( "FFMPEG: Failed to allocate frame or packet!\n" );
		return false;
	}

//	av_frame->format   = AV_PIX_FMT_BGRA;
//	av_frame->width    = avdata.dec_ctx->width;
//	av_frame->height   = avdata.dec_ctx->height;
//
//	int sts            = av_frame_get_buffer( av_frame, 0 );
//
//	if ( sts < 0 )
//	{
//		printf( "FFMPEG: Failed to get frame status!\n" );
//		return false;
//	}

	/* allocate image where the decoded image will be put */
	image_t*     image = image_create();
	thumbnail->image   = image;

	image->width       = avdata.dec_ctx->width;
	image->height      = avdata.dec_ctx->height;
	// image->channels    = 4;
	// image->format      = GL_BGRA;

	image->frame.resize( 1 );

	int  decode_ret = 0;
	bool hit_eof    = false;

	while ( av_read_frame( avdata.fmt_ctx, av_packet ) <= 0 )
	{
		if ( av_packet->stream_index != avdata.video_stream_index )
		{
			av_packet_unref( av_packet );
			continue;
		}

		// check the timestamp, make sure we are at the correct position
		avdata.dec_ctx;
		av_packet->dts;

		decode_ret = decode_packet( avdata, av_frame, av_packet, image );

		if ( decode_ret == -1 )
			break;

		if ( decode_ret == AVERROR_EOF )
		{
			hit_eof = true;
			// printf( "END OF VIDEO STREAM - SEEKING TO START\n" );

			// go to start of file instead
			if ( !seek_to_percentage( avdata, 0.0, seek_ts ) )
			// if ( av_seek_frame( avdata.fmt_ctx, -1, 0, AVSEEK_FLAG_BACKWARD ) < 0 )
				break;
			
			// clear out old frames
			// avcodec_flush_buffers( avdata.dec_ctx );
		}

		if ( decode_ret == 1 )
		{
			// check the timestamp, make sure we are at the correct position
			int64_t frame_ts = frame_get_timestamp( av_frame );
			if ( frame_ts < seek_ts && !hit_eof )
			{
				av_frame_unref( av_frame );
				continue;
			}

			// TODO: do blank frame detection here

			if ( !write_image_data( avdata, av_frame, av_packet, image ) )
			{
			}

			av_frame_unref( av_frame );
			break;
		}
	}

	av_frame_free( &av_frame );
	av_packet_free( &av_packet );
	//av_parser_close( avdata.parser );
	avcodec_free_context( &avdata.dec_ctx );
	avformat_close_input( &avdata.fmt_ctx );

	return decode_ret == 1;

#if 0
	const AVCodec*        codec;
	AVCodecParserContext* parser;
	AVCodecContext*       c = NULL;
	AVFrame*              frame;
	uint8_t               inbuf[ INBUF_SIZE + AV_INPUT_BUFFER_PADDING_SIZE ];
	uint8_t*              data;
	size_t                data_size;
	int                   ret;
	int                   eof;
	AVPacket*             pkt;

	pkt = av_packet_alloc();
	if ( !pkt )
		return false;

	/* set end of buffer to 0 (this ensures that no overreading happens for damaged MPEG streams) */
	memset( inbuf + INBUF_SIZE, 0, AV_INPUT_BUFFER_PADDING_SIZE );

	/* find the MPEG-1 video decoder */
	codec = avcodec_find_decoder( AV_CODEC_ID_MPEG1VIDEO );
	if ( !codec )
	{
		fprintf( stderr, "Codec not found\n" );
		return false;
	}

	parser = av_parser_init( codec->id );
	if ( !parser )
	{
		fprintf( stderr, "parser not found\n" );
		return false;
	}

	c = avcodec_alloc_context3( codec );
	if ( !c )
	{
		fprintf( stderr, "Could not allocate video codec context\n" );
		return false;
	}

	/* For some codecs, such as msmpeg4 and mpeg4, width and height
       MUST be initialized there because this information is not
       available in the bitstream. */

	/* open it */
	if ( avcodec_open2( c, codec, NULL ) < 0 )
	{
		fprintf( stderr, "Could not open codec\n" );
		return false;
	}

	FILE* file_handle = path_open( thumbnail->path, "rb" );

	if ( !file_handle )
	{
		path_printf( "Could not open %s\n", thumbnail->path );
		return false;
	}

	frame = av_frame_alloc();
	if ( !frame )
	{
		fprintf( stderr, "Could not allocate video frame\n" );
		exit( 1 );
	}

	do
	{
		/* read raw data from the input file */
		data_size = fread( inbuf, 1, INBUF_SIZE, file_handle );
		if ( ferror( file_handle ) )
			break;
		eof  = !data_size;

		/* use the parser to split the data into frames */
		data = inbuf;
		while ( data_size > 0 || eof )
		{
			ret = av_parser_parse2( parser, c, &pkt->data, &pkt->size,
			                        data, data_size, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0 );
			if ( ret < 0 )
			{
				fprintf( stderr, "Error while parsing\n" );
				exit( 1 );
			}
			data += ret;
			data_size -= ret;

			if ( pkt->size )
				decode( c, frame, pkt, thumbnail );
			else if ( eof )
				break;
		}
	} while ( !eof );

	/* flush the decoder */
	decode( c, frame, NULL, thumbnail );

	fclose( file_handle );

	av_parser_close( parser );
	avcodec_free_context( &c );
	av_frame_free( &frame );
	av_packet_free( &pkt );

#endif

	//return false;
}

