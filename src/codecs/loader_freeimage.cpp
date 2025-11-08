#include "main.h"

#include "Freeimage.h"


struct LoaderFreeImage : public IImageLoader
{
	LoaderFreeImage()
	{
		// On windows, this is called automatically on dll load
#if __unix__
		FreeImage_Initialise();
#endif

		image_register_codec( this );
	}

	~LoaderFreeImage()
	{
#if __unix__
		FreeImage_DeInitialise();
#endif
	}

	bool check_extension( std::string_view ext ) override
	{
		// fucking shit
		// return ( FreeImage_GetFileType( ext.data() ) != FIF_UNKNOWN );

		return ( FreeImage_GetFIFFromFilename( ext.data() ) != FIF_UNKNOWN );
		//return ext == ".png" || ext == ".jpg" || ext == ".jpeg";
		//return ext == ".png";
		// return ext == ".jpg" || ext == ".jpeg";
	}

	bool check_header( const fs::path& path ) override
	{
		return false;
	}
	 
	bool image_load( const fs::path& path, image_load_info_t& load_info, char* data, size_t data_len ) override
	{
		// attach the binary data to a memory stream
		FIMEMORY* memory = FreeImage_OpenMemory( (u8*)data, data_len );

		if ( memory == nullptr )
		{
			printf( "LOADER_FREEIMAGE: Failed to open memory\n" );
			return false;
		}

		// make sure it's the right type, like an incorrect file extension
		FREE_IMAGE_FORMAT format = FreeImage_GetFileTypeFromMemory( memory );

		if ( format == FIF_UNKNOWN )
		{
			return false;
		}

		FIBITMAP* bitmap = FreeImage_LoadFromMemory( format, memory );
		// FIMULTIBITMAP* bitmap = FreeImage_LoadMultiBitmapFromMemory( format, memory );

		if ( bitmap == nullptr )
		{
			printf( "LOADER_FREEIMAGE: Failed to load image from memory\n" );
			FreeImage_CloseMemory( memory );
			return false;
		}

		FreeImage_FlipVertical( bitmap );

		// Convert non-32 bit or 24-bit images
		u32 bpp = FreeImage_GetBPP( bitmap );
		// if ( bpp != 32 && bpp != 24 )

		// HACK, some 24 bit images appear diagonal and i don't know why yet
		if ( bpp == 24 )
		{
			u32       channel_num = FreeImage_GetChannelsNumber( bitmap );
		
			FIBITMAP* old_bitmap = bitmap;
			bitmap               = FreeImage_ConvertTo32Bits( old_bitmap );
			FreeImage_Unload( old_bitmap );

			bpp = FreeImage_GetBPP( bitmap );
		}
		// else
		// {
		// 	u32       channel_num = FreeImage_GetChannelsNumber( bitmap );
		// 
		// 	FIBITMAP* old_bitmap = bitmap;
		// 	bitmap               = FreeImage_ConvertTo24Bits( old_bitmap );
		// 	FreeImage_Unload( old_bitmap );
		// 
		// 	bpp = FreeImage_GetBPP( bitmap );
		// }

		// hdr image - TODO: this looks kinda shit
		if ( bpp == 96 || bpp == 128 )
		{
			FIBITMAP* old_bitmap = bitmap;
			bitmap               = FreeImage_ToneMapping( old_bitmap, FITMO_DRAGO03 );
			// bitmap               = FreeImage_ToneMapping( old_bitmap, FITMO_REINHARD05 );
			FreeImage_Unload( old_bitmap );

			//FreeImage_AdjustGamma( bitmap, 0.6 );

			bpp = FreeImage_GetBPP( bitmap );
		}


		FREE_IMAGE_COLOR_TYPE color_type = FreeImage_GetColorType( bitmap );
		FREE_IMAGE_TYPE       image_type = FreeImage_GetImageType( bitmap );

		if ( image_type != FIT_BITMAP )
		{
			// printf( "huh\n" );
			// return false;
		}

		load_info.image->width            = FreeImage_GetWidth( bitmap );
		load_info.image->height           = FreeImage_GetHeight( bitmap );
		load_info.image->pitch            = FreeImage_GetPitch( bitmap );
		u32                   channel_num = FreeImage_GetChannelsNumber( bitmap );

		int                   bit_depth   = load_info.image->pitch / load_info.image->width;
		int                   bit_depth2  = bpp / channel_num;
		int bit_depth3                   = load_info.image->pitch / bpp;

		// load_info.image->bytes_per_pixel = bpp / bit_depth2;
		load_info.image->bytes_per_pixel = bit_depth;
		
		if ( load_info.image->bytes_per_pixel == 0 )
		{
			// huh????
			return false;
		}

		u8* pixels              = FreeImage_GetBits( bitmap );

		load_info.image->frame.resize( 1 );
		size_t image_size           = load_info.image->pitch * load_info.image->height;

		load_info.image->frame[ 0 ] = ch_calloc< u8 >( image_size );

		memcpy( load_info.image->frame[ 0 ], pixels, image_size );

		if ( image_type == FIT_BITMAP )
		{
			switch ( channel_num )
			{
				case 4:
					load_info.image->format = GL_RGBA;
					break;

				default:
				case 3:
					load_info.image->format = GL_RGB;
					break;

				case 1:
					load_info.image->format = GL_LUMINANCE;
					break;
			}
		}
		else
		{
			switch ( image_type )
			{
				default:
					printf( "UNHANDLED\n" );
					load_info.image->format = GL_RGBA;
					break;

				case FIT_UINT16:
					load_info.image->format = GL_R16UI;
					break;

				case FIT_INT16:
					load_info.image->format = GL_R16I;
					break;

				case FIT_RGB16:
					load_info.image->format = GL_RGB16;
					break;

				case FIT_RGBA16:
					load_info.image->format = GL_RGBA16;
					break;

				case FIT_RGBAF:
					load_info.image->format = GL_RGBA32F;
					break;
			}
		}

		FreeImage_Unload( bitmap );
		FreeImage_CloseMemory( memory );

		return true;
	}
};


LoaderFreeImage gLoaderFreeImage;

