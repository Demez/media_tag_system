#include "main.h"

#include "gif_load.h"


// uh
struct GifData
{
	//ImageInfo* apInfo;
	//std::vector< std::vector< char > > aData;
};


static void GifFrameWriter( void* spData, struct GIF_WHDR* spGifData )
{
	image_load_info_t* load_info = (image_load_info_t*)spData;
	image_t*           image     = load_info->image;

	size_t             frame_i   = image->frame.size();
	image->frame.resize( image->frame.size() + 1 );

	//frame->apInfo->aWidth  = spGifData->xdim;
	//frame->apInfo->aHeight = spGifData->ydim;

	printf( "what\n" );
}


static void GifMetadataReader( void* spData, struct GIF_WHDR* spGifData )
{
	GifData* data = (GifData*)spData;
	printf( "what2\n" );
}


struct FormatGIF : public IImageLoader
{
	std::string match_ext = ".gif";

	FormatGIF()
	{
		// image_register_codec( this );
	}

	~FormatGIF()
	{
	}
	bool check_extension( std::string_view ext ) override
	{
		return ext == match_ext;
	}

	bool check_header( const fs::path& path ) override
	{
		return false;
	}

	bool image_load( const fs::path& path, image_load_info_t& load_info, char* data, size_t data_len ) override
	{
		long ret = GIF_Load( data, data_len,
		                     &GifFrameWriter,
		                     &GifMetadataReader,
		                     &load_info, 0 );

		return load_info.image->frame.size();
	}
};


//FormatGIF gpFmtGif;

