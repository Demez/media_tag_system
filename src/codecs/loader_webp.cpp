#include "main.h"
#include <webp/decode.h>
#include <webp/demux.h>


struct LoaderWebP : public IImageLoader
{
	LoaderWebP()
	{
		image_register_codec( this, false );
	}

	~LoaderWebP()
	{
	}

	virtual void get_supported_extensions( std::vector< std::string >& extensions ) override
	{
		extensions.push_back( ".webp" );
	}

	bool check_header( const fs::path& path ) override
	{
		return false;
	}

	bool image_load( const fs::path& path, image_load_info_t& load_info, char* data, size_t data_len ) override
	{
		image_t*          image = load_info.image;

		// setup demuxer - see header comment in demux.h
		WebPData webp_data;
		WebPDataInit( &webp_data );

		webp_data.bytes    = reinterpret_cast< u8* >( data );
		webp_data.size     = data_len;

		WebPDemuxer* demux = WebPDemux( &webp_data );

		if ( !demux )
			return false;

		image->width  = WebPDemuxGetI( demux, WEBP_FF_CANVAS_WIDTH );
		image->height = WebPDemuxGetI( demux, WEBP_FF_CANVAS_HEIGHT );

		// ... (Get information about the features present in the WebP file).
		u32 flags     = WebPDemuxGetI( demux, WEBP_FF_FORMAT_FLAGS );

		if ( flags & ANIMATION_FLAG )
		{
			u32 loop_count  = WebPDemuxGetI( demux, WEBP_FF_LOOP_COUNT );
			u32 frame_count = WebPDemuxGetI( demux, WEBP_FF_FRAME_COUNT );
			image->frame.resize( frame_count );
			image->loop_count = loop_count;
		}
		else
		{
			image->frame.resize( 1 );
		}

		if ( flags & ALPHA_FLAG )
		{
			image->format   = GL_BGRA;
			image->channels = 4;
		}
		else
		{
			image->format   = GL_BGR;
			image->channels = 3;
		}

		// ... (Iterate over all frames).
		WebPIterator iter;
		if ( WebPDemuxGetFrame( demux, 1, &iter ) )
		{
			do
			{
				// ... (Consume 'iter'; e.g. Decode 'iter.fragment' with WebPDecode(),
				// ... and get other frame properties like width, height, offsets etc.
				// ... see 'struct WebPIterator' below for more info).

				WebPDecoderConfig config{};
				WebPInitDecoderConfig( &config );

				if ( flags & ALPHA_FLAG )
				{
					config.output.colorspace = MODE_BGRA;
				}
				else
				{
					config.output.colorspace = MODE_BGR;
				}

				config.options.use_threads = load_info.thumbnail_load ? 0 : 1;
				VP8StatusCode status = WebPDecode( iter.fragment.bytes, iter.fragment.size, &config );

				if ( status != VP8_STATUS_OK )
				{
					WebPDemuxDelete( demux );
					return false;
				}

				image_frame_t& frame = image->frame[ static_cast< size_t >( iter.frame_num - 1 ) ];

				frame.width          = iter.width;
				frame.height         = iter.height;
				frame.pos_x          = iter.x_offset;
				frame.pos_y          = iter.y_offset;
				frame.time           = static_cast< double >( iter.duration ) / 1000.0;

				switch (iter.dispose_method)
				{
					case WEBP_MUX_DISPOSE_NONE:
						frame.frame_disposal = e_frame_disposal_keep;
						break;

					case WEBP_MUX_DISPOSE_BACKGROUND:
						frame.frame_disposal = e_frame_disposal_background;
						break;
				}

				// TODO: handle blend mode

				frame.size           = config.output.u.RGBA.size;
				frame.pitch          = config.output.u.RGBA.stride;
				frame.data           = ch_calloc< u8 >( config.output.u.RGBA.size, e_mem_category_image_data );

				// VP8StatusCode status = WebPDecodeBGRA( iter.fragment.bytes, iter.fragment.size, &config );
				memcpy( frame.data, config.output.u.RGBA.rgba, frame.size );

			} while ( WebPDemuxNextFrame( &iter ) );
			WebPDemuxReleaseIterator( &iter );
		}

		// ... (Extract metadata).
		#if 0
		WebPChunkIterator chunk_iter;
		if ( flags & ICCP_FLAG ) WebPDemuxGetChunk( demux, "ICCP", 1, &chunk_iter );
		// ... (Consume the ICC profile in 'chunk_iter.chunk').
		WebPDemuxReleaseChunkIterator( &chunk_iter );
		if ( flags & EXIF_FLAG ) WebPDemuxGetChunk( demux, "EXIF", 1, &chunk_iter );
		// ... (Consume the EXIF metadata in 'chunk_iter.chunk').
		WebPDemuxReleaseChunkIterator( &chunk_iter );
		if ( flags & XMP_FLAG ) WebPDemuxGetChunk( demux, "XMP ", 1, &chunk_iter );
		// ... (Consume the XMP metadata in 'chunk_iter.chunk').
		WebPDemuxReleaseChunkIterator( &chunk_iter );
		#endif

		WebPDemuxDelete( demux );
		return true;
	}
};


static LoaderWebP gLoaderWebP;

