#include "main.h"

#define GIF_MGET( m, s, a, c )                               \
	if ( c )                                                 \
		m = ch_calloc< u8 >( s, e_mem_category_image_data ); \
	else                                                     \
		ch_free( e_mem_category_image_data, m );

#include "gif_load/gif_load.h"


struct ff_gif_data_t
{
	image_load_info_t& load_info;

	bool               incomplete_frame_count = false;
	bool               loaded_metadata        = false;
};


static void gif_write_palette( image_palette_t& palette, struct GIF_WHDR* gif_whdr )
{
	if ( gif_whdr->clrs <= 0 )
		return;

	palette.elem = ch_calloc< image_palette_elem_t >( palette.count, e_mem_category_image_data );

	//if ( gif_whdr->tran != -1 )
	//	frame.palette.trans_index = gif_whdr->tran;
	//else
	//	frame.palette.trans_index = frame.palette.count;

	for ( u32 p = 0; p < palette.count; p++ )
	{
		palette.elem[ p ].r = gif_whdr->cpal[ p ].R;
		palette.elem[ p ].g = gif_whdr->cpal[ p ].G;
		palette.elem[ p ].b = gif_whdr->cpal[ p ].B;
		palette.elem[ p ].a = 255;
	}
}


static void gif_setup_image( ff_gif_data_t* ff_gif_data, struct GIF_WHDR* gif_whdr )
{
	image_load_info_t& load_info = ff_gif_data->load_info;
	image_t*           image     = load_info.image;

	image->width                 = gif_whdr->xdim;
	image->height                = gif_whdr->ydim;
	image->format                = GL_R8;
	image->channels              = 1;

	image->palette.count         = gif_whdr->clrs;
	image->palette.bg_index      = gif_whdr->bkgd;
	image->palette.trans_index   = gif_whdr->tran;

	if ( gif_whdr->clrs > 0 )
	{
		image->flags |= e_image_flag_has_palette;
		gif_write_palette( image->palette, gif_whdr );
	}

	if ( gif_whdr->nfrm > 0 )
		image->frame.resize( load_info.single_frame ? 1 : gif_whdr->nfrm );
	else
		ff_gif_data->incomplete_frame_count = true;

	ff_gif_data->loaded_metadata = true;
}


static void gif_frame_writer( void* data, struct GIF_WHDR* gif_whdr )
{
	auto               ff_gif_data = static_cast< ff_gif_data_t* >( data );
	image_load_info_t& load_info   = ff_gif_data->load_info;
	image_t*           image       = load_info.image;

	size_t             frame_i     = gif_whdr->ifrm;

	// gif_load doesn't call the metadata function sometimes
	if ( frame_i == 0 && !ff_gif_data->loaded_metadata )
		gif_setup_image( ff_gif_data, gif_whdr );

	if ( frame_i > 0 && load_info.single_frame )
		return;

	if ( ff_gif_data->incomplete_frame_count )
		image->frame.resize( frame_i + 1 );

	image_frame_t& frame = image->frame[ frame_i ];
	image_reset_frame_data( frame );

	frame.time           = static_cast< float >( gif_whdr->time ) * 0.01;

	frame.pos_x          = gif_whdr->frxo;
	frame.pos_y          = gif_whdr->fryo;
	frame.width          = gif_whdr->frxd;
	frame.height         = gif_whdr->fryd;

	switch ( gif_whdr->mode )
	{
		default:
		case GIF_NONE:
		case GIF_CURR:
			frame.frame_disposal = e_frame_disposal_keep;
			break;

		case GIF_BKGD:
			frame.frame_disposal = e_frame_disposal_background;
			break;

		case GIF_PREV:
			frame.frame_disposal = e_frame_disposal_previous;
			break;
	}

	// in thumbnail load, apply the palette colors directly
	// in opengl land, since it's a global state machine, i can't really create an FBO for each thread
	// and then draw the image proper there, so i have to apply it here in software
	if ( load_info.thumbnail_load )
	{
		image->format   = GL_RGBA;
		image->channels = 4;

		size_t og_size = frame.width * frame.height;
		size_t size    = og_size * 4;  // 4 channels
		frame.data     = ch_calloc< u8 >( size, e_mem_category_image_data );
		frame.size     = size;

		memset( frame.data, 0, size * sizeof( u8 ) );

		u8* start = &frame.data[ 0 ];
		for ( int s = 0; s < og_size; s++ )
		{
			auto palette_index = gif_whdr->bptr[ s ];

			*start = gif_whdr->cpal[ palette_index ].R;
			start++;

			*start = gif_whdr->cpal[ palette_index ].G;
			start++;

			*start = gif_whdr->cpal[ palette_index ].B;
			start++;

			*start = 255;
			start++;
		}
	}
	else
	{
		size_t size = frame.width * frame.height;
		frame.data  = ch_calloc< u8 >( size, e_mem_category_image_data );
		frame.size  = size;

		memset( frame.data, 0, size * sizeof( u8 ) );
		memcpy( frame.data, gif_whdr->bptr, size );

		frame.color_type          = e_colortype_palette;
		frame.palette.count       = gif_whdr->clrs;
		frame.palette.bg_index    = gif_whdr->bkgd;
		frame.palette.trans_index = gif_whdr->tran;

		if ( gif_whdr->clrs > 0 )
		{
			image->flags |= e_image_flag_has_local_palettes;
			frame.flags |= e_image_frame_f_local_palette;

			gif_write_palette( frame.palette, gif_whdr );
		}
	}

	//frame->apInfo->aWidth  = spGifData->xdim;
	//frame->apInfo->aHeight = spGifData->ydim;
}


static void gif_metadata_reader( void* data, struct GIF_WHDR* gif_whdr )
{
	gif_setup_image( static_cast< ff_gif_data_t* >( data ), gif_whdr );
}


struct LoaderGIF : public IImageLoader
{
	LoaderGIF()
	{
		image_register_codec( this, false );
	}

	~LoaderGIF()
	{
	}

	virtual void get_supported_extensions( std::vector< std::string >& extensions ) override
	{
		extensions.push_back( ".gif" );
	}

	bool check_header( const fs::path& path ) override
	{
		return false;
	}

	bool image_load( const fs::path& path, image_load_info_t& load_info, char* data, size_t data_len ) override
	{
		ff_gif_data_t ff_gif_data{ load_info };

		long          ret = GIF_Load( data, data_len,
		                              &gif_frame_writer,
		                              &gif_metadata_reader,
		                              &ff_gif_data, 0 );

		return ret;
	}
};


static LoaderGIF gLoaderGIF;

