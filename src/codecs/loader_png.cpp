#include "main.h"

#include <spng.h>

#define APNG_SUPPORTED 1

#if 1

static void png_write_palette( image_palette_t& palette, struct spng_plte& png_palette )
{
	if ( png_palette.n_entries == 0 )
		return;

	palette.count = png_palette.n_entries;
	palette.elem  = ch_calloc< image_palette_elem_t >( palette.count, e_mem_category_image_data );

	for ( u32 p = 0; p < palette.count; p++ )
	{
		palette.elem[ p ].r = png_palette.entries[ p ].red;
		palette.elem[ p ].g = png_palette.entries[ p ].green;
		palette.elem[ p ].b = png_palette.entries[ p ].blue;
	}
}

struct LoaderPNG: public IImageLoader
{
	std::string match_ext  = ".png";

	LoaderPNG()
	{
		image_register_codec( this, false );
	}

	~LoaderPNG()
	{
	}

	void get_supported_formats( std::vector< image_format_info_t >& formats ) override
	{
		formats.emplace_back( "PNG", match_ext );
		formats.emplace_back( "Animated PNG", ".apng" );
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
		image_t*  image = load_info.image;

		spng_ctx* ctx   = spng_ctx_new( 0 );
        if( !ctx )
        {
            fprintf( stderr, "LOADER_PNG: Failed to allocate memory for png context.\n" );
            return false;
        }

        struct spng_ihdr ihdr{};
		struct spng_trns trns{};
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
			printf( "LOADER_PNG: Failed to get header information: %s\n", spng_strerror( err ) );
			spng_ctx_free( ctx );
			return false;
		}

		spng_format pngFmt = SPNG_FMT_RGB8;

		// https://libspng.org/docs/migrate-libpng/

		// palette image
		//if ( ihdr.color_type == SPNG_COLOR_TYPE_INDEXED )
		//	pngFmt = SPNG_FMT_PNG;

		//else if ( ihdr.color_type == SPNG_COLOR_TYPE_GRAYSCALE && ihdr.bit_depth < 8 )
		//	pngFmt = SPNG_FMT_G8;

		int have_trns = spng_get_trns( ctx, &trns );

		if ( have_trns == 0 )
		{
			// if ( ihdr.color_type == SPNG_COLOR_TYPE_TRUECOLOR )
			if ( ihdr.color_type == SPNG_COLOR_TYPE_TRUECOLOR || ihdr.color_type == SPNG_COLOR_TYPE_GRAYSCALE )
			{
				if ( ihdr.bit_depth == 16 )
					pngFmt = SPNG_FMT_RGBA16;
				else
					pngFmt = SPNG_FMT_RGBA8;
			}
			//else if ( ihdr.color_type == SPNG_COLOR_TYPE_GRAYSCALE )
			//{
			//	if ( ihdr.bit_depth == 16 )
			//		pngFmt = SPNG_FMT_GA16;
			//	else
			//		pngFmt = SPNG_FMT_GA8;
			//}
			else if ( ihdr.color_type == SPNG_COLOR_TYPE_INDEXED )
			{
				pngFmt = SPNG_FMT_RGBA8;
			}
		}

		struct spng_bkgd bkgd{};
		int              has_bg = spng_get_bkgd( ctx, &bkgd );

		// look into RGBA16?
        //spng_format pngFmt = ihdr.bit_depth == 16 ? SPNG_FMT_RGBA16 : SPNG_FMT_RGB8;
		// spng_format pngFmt = SPNG_FMT_RGBA8;
		int         decode_flags{};
		decode_flags |= SPNG_DECODE_TRNS;

        if ( ihdr.color_type == SPNG_COLOR_TYPE_TRUECOLOR_ALPHA || ihdr.color_type == SPNG_COLOR_TYPE_GRAYSCALE_ALPHA )
		{
			pngFmt = ihdr.bit_depth == 16 ? SPNG_FMT_RGBA16 : SPNG_FMT_RGBA8;
			pngFmt = SPNG_FMT_RGBA8;
		}

		switch ( ihdr.color_type )
		{
			case SPNG_COLOR_TYPE_TRUECOLOR:
			default:
			{
				switch ( pngFmt )
				{
					case SPNG_FMT_RGB8:
						image->format   = GL_RGB;
						image->channels = 3;
						break;

					case SPNG_FMT_RGBA8:
						image->format   = GL_RGBA;
						image->channels = 4;
						break;

					case SPNG_FMT_RGBA16:
						image->format   = GL_RGBA16;
						image->channels = 4;
						break;
				}

				break;
			}
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
		// image->frame      = ch_realloc( image->frame, 1, e_mem_category_image_data  );
		image->frame.resize( 1 );

		image->frame[ 0 ].data = ch_calloc< u8 >( size, e_mem_category_image_data );
		image->frame[ 0 ].size = size;

		err                    = spng_decode_image( ctx, image->frame[ 0 ].data, size, pngFmt, decode_flags );
		if ( err != 0 )
		{
			printf( "LOADER_PNG: Failed to decode image: %s\n", spng_strerror( err ) );
			spng_ctx_free( ctx );
			return false;
		}

		#if APNG_SUPPORTED

		/* Decode frames from an APNG */
		struct spng_actl actl;
		int              has_apng = spng_get_actl( ctx, &actl ); /* num_frames, num_plays */

		if ( has_apng == 0 && !load_info.thumbnail_load )
		{
			image->loop_count         = actl.num_plays;
			image->frame.resize( actl.num_frames );
			size_t           frame_i = 1;

			struct spng_fctl fctl;
			//int              has_apng_fctl = spng_get_frame_fctl( ctx, &fctl ); /* num_frames, num_plays */

			err = spng_decoded_image_size( ctx, pngFmt, &size );
			if ( err != 0 )
			{
				printf( "LOADER_PNG Failed to decode image size: %s\n", spng_strerror( err ) );
				spng_ctx_free( ctx );
				return false;
			}

			//size_t      image_size = static_cast< size_t >( fctl.width ) * static_cast< size_t >( fctl.height ) * static_cast< size_t >( image->channels );

			image->frame[ frame_i ].data   = ch_calloc< u8 >( size, e_mem_category_image_data );
			image->frame[ frame_i ].size   = size;

			err                            = spng_decode_frame( ctx, image->frame[ frame_i ].data, image->frame[ frame_i ].size, pngFmt, decode_flags, &fctl );

			while ( err == 0 )
			{
				image_frame_t& frame = image->frame[ frame_i ];
				frame.width          = fctl.width;
				frame.height         = fctl.height;
				frame.pos_x          = fctl.x_offset;
				frame.pos_y          = fctl.y_offset;

				double tps_den       = static_cast< double >( fctl.delay_den );
				double tps_num       = static_cast< double >( fctl.delay_num );

				if ( tps_num > 0 )
					frame.time = tps_num / tps_den;
				else
					frame.time = 0.1;

				switch ( fctl.dispose_op )
				{
					default:
					case SPNG_DISPOSE_OP_NONE:
						frame.frame_disposal = e_frame_disposal_keep;
						break;

					case SPNG_DISPOSE_OP_BACKGROUND:
						frame.frame_disposal = e_frame_disposal_background;
						break;

					case SPNG_DISPOSE_OP_PREVIOUS:
						frame.frame_disposal = e_frame_disposal_previous;
						break;
				}

				// TODO: look into blend modes, kinda varrying with all formats
				switch ( fctl.blend_op )
				{
					default:
					case SPNG_BLEND_OP_SOURCE:
						frame.blend = e_frame_blend_all;
						break;

					case SPNG_BLEND_OP_OVER:
						frame.blend = e_frame_blend_alpha;
						break;
				}

				frame_i++;

				if ( frame_i == image->frame.size() )
					break;

				//has_apng_fctl            = spng_get_frame_fctl( ctx, &fctl ); /* num_frames, num_plays */

				// image_size                   = fctl.width * fctl.height * image->channels;

				image->frame[ frame_i ].data = ch_calloc< u8 >( size, e_mem_category_image_data );
				image->frame[ frame_i ].size = size;

				err                          = spng_decode_frame( ctx, image->frame[ frame_i ].data, image->frame[ frame_i ].size, pngFmt, decode_flags, &fctl );

				/* fctl.delay_num / fctl.delay_den = frame delay in seconds */
				// display_frame( buf, fctl.width, fctl.height,
				//                fctl.x_offset, fctl.y_offset,
				//                fctl.delay_num, fctl.delay_den );
			}

			if ( err != 0 )
			{
				printf( "LOADER_PNG Failed to decode image size: %s\n", spng_strerror( err ) );
				spng_ctx_free( ctx );
				return false;
			}
		}
		else
		{
  #endif


  #if APNG_SUPPORTED
		}
	#endif

  #if 0    // maybe one day i can get this working well for hopefully less memory used
		switch ( ihdr.color_type )
		{
			#if 0  // maybe one day i can get this working well for hopefully less memory used
			case SPNG_COLOR_TYPE_INDEXED:
			{
				spng_plte plte;
				err = spng_get_plte( ctx, &plte );

				if ( err == 0 )
				{
					image->flags |= e_image_flag_has_local_palettes;
					image->frame[ 0 ].flags      |= e_image_frame_f_local_palette;
					image->frame[ 0 ].color_type = e_colortype_palette;
					png_write_palette( image->frame[ 0 ].palette, plte );
				}
				else
				{
					image->frame[ 0 ].color_type = e_colortype_min_is_white;
				}

				if ( have_trns )
				{
					// image->frame[ 0 ].palette.trans_index = trns.gray;
				}

				image->format   = GL_R8;
				image->channels = 1;
				break;
			}
			#endif
		}
		#endif


		image->width             = ihdr.width;
		image->height            = ihdr.height;

		image->frame[ 0 ].width  = ihdr.width;
		image->frame[ 0 ].height = ihdr.height;

		// image->format    = GL_RGBA;
		//image->bit_depth         = ihdr.bit_depth;
		// image->pitch             = image->width * image->bytes_per_pixel;

		image->image_format      = image_get_format_index_from_ext( has_apng == 0 ? ".apng" : match_ext );

        spng_ctx_free( ctx );

		return true;
	}
};


static LoaderPNG g_loader_png;

#endif