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

		image_register_codec( this, true );
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
		FIMEMORY* memory = FreeImage_OpenMemory( (u8*)data, (u32)data_len );

		if ( memory == nullptr )
		{
			printf( "LOADER_FREEIMAGE: Failed to open memory\n" );
			return false;
		}

		// make sure it's the right type, like an incorrect file extension
		FREE_IMAGE_FORMAT format = FreeImage_GetFileTypeFromMemory( memory );

		if ( format == FIF_UNKNOWN )
		{
			FreeImage_CloseMemory( memory );
			return false;
		}

		int load_flags = 0;

		switch ( format )
		{
			case FIF_GIF:
			{
				// 'Play' the GIF to generate each frame (as 32bpp) instead of returning raw frame data when loading 
				// load_flags = GIF_PLAYBACK;
				break;
			}
			case FIF_JPEG:
			{
				if ( load_info.load_quick )
					load_flags = JPEG_FAST;
				else
					load_flags = JPEG_ACCURATE | JPEG_EXIFROTATE;

				break;
			}
		}

		FIBITMAP* bitmap = FreeImage_LoadFromMemory( format, memory, load_flags );
		// FIMULTIBITMAP* bitmap = FreeImage_LoadMultiBitmapFromMemory( format, memory );

		if ( bitmap == nullptr )
		{
			printf( "LOADER_FREEIMAGE: Failed to load image from memory\n" );
			FreeImage_CloseMemory( memory );
			return false;
		}

		// Convert non-32 bit or 24-bit images
		u32 bpp = FreeImage_GetBPP( bitmap );
		// if ( bpp != 32 && bpp != 24 )

	//	// TEMP
	//	if ( bpp == 32 )
	//	{
	//		FreeImage_Unload( bitmap );
	//		FreeImage_CloseMemory( memory );
	//		return false;
	//	}

		// HACK, some 24 bit images appear diagonal and i don't know why yet
		if ( bpp == 24 )
		{
			//u32       channel_num = FreeImage_GetChannelsNumber( bitmap );
		
			FIBITMAP* old_bitmap = bitmap;
			bitmap               = FreeImage_ConvertTo32Bits( old_bitmap );
			FreeImage_Unload( old_bitmap );
	
			bpp = FreeImage_GetBPP( bitmap );
		}
		// else
	//	{
	//	 	u32       channel_num = FreeImage_GetChannelsNumber( bitmap );
	//	 
	//	 	FIBITMAP* old_bitmap = bitmap;
	//	 	bitmap               = FreeImage_ConvertTo24Bits( old_bitmap );
	//	 	FreeImage_Unload( old_bitmap );
	//	 
	//	 	bpp = FreeImage_GetBPP( bitmap );
	//	}

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

		if ( bitmap == nullptr )
		{
			printf( "LOADER_FREEIMAGE: Failed to load image from memory\n" );
			FreeImage_CloseMemory( memory );
			return false;
		}

		FREE_IMAGE_COLOR_TYPE color_type = FreeImage_GetColorType( bitmap );
		FREE_IMAGE_TYPE       image_type = FreeImage_GetImageType( bitmap );

		load_info.image->width           = FreeImage_GetWidth( bitmap );
		load_info.image->height          = FreeImage_GetHeight( bitmap );

		// ----------------------------------------------------------------------------------------

		switch ( color_type )
		{
			default:
			case FIC_RGB:
			case FIC_RGBALPHA:
				break;

			case FIC_PALETTE:
			{
				//u32       channel_num = FreeImage_GetChannelsNumber( bitmap );
				FIRGBA8*  palette     = FreeImage_GetPalette( bitmap );

				FIBOOL    trans       = FreeImage_IsTransparent( bitmap );

				// FreeImage_ApplyPaletteIndexMapping
				// FIBITMAP* tmp         = FreeImage_Allocate( load_info.image->width, load_info.image->height, trans ? 32 : 24 );
				FIBITMAP* tmp         = FreeImage_Allocate( load_info.image->width, load_info.image->height, 32 );
				u8*       tmp_bits    = FreeImage_GetBits( tmp );
				u8*       image_bits  = FreeImage_GetBits( bitmap );

				#if 01
				// https://stackoverflow.com/questions/42084727/reading-gif-color-image-with-freeimage-library-and-convert-it-to-opencv-mat
				for ( int x = 0; x < load_info.image->width; x++ )
				{
					for ( int y = 0; y < load_info.image->height; y++ )
					{
						u8     palette_index = 0;
						FIBOOL ret           = FreeImage_GetPixelIndex( bitmap, x, y, &palette_index );

						if ( !ret )
						{
							printf( "failed to get pixel index for palette\n" );
						}

						FIRGBA8 color = palette[ palette_index ];

						// FIRGBA8 color{};
						// color.red = palette_index;
						// color.green = palette_index;
						// color.blue = palette_index;
						//if ( trans )
						//{
						//	if ( color.alpha > 0 )
						//		color.alpha = 255;
						//}
						//else
						{
							color.alpha = 255;
						}

						FreeImage_SetPixelColor( tmp, x, y, &color );
					}
				}
				#endif

				FreeImage_Unload( bitmap );
				bitmap = tmp;

				break;
			}
		}

		FreeImage_FlipVertical( bitmap );

		// ----------------------------------------------------------------------------------------
		// Copy Image Data

		load_info.image->pitch           = FreeImage_GetPitch( bitmap );

		load_info.image->bit_depth       = FreeImage_GetBPP( bitmap );
		load_info.image->image_format    = util_strdup_r( load_info.image->image_format, FreeImage_GetFormatFromFIF( format ) );
		u32 channel_num                  = FreeImage_GetChannelsNumber( bitmap );

		//load_info.image->pitch           = load_info.image->width * channel_num;

		int bit_depth                    = load_info.image->pitch / load_info.image->width;
		int bit_depth2                   = bpp / channel_num;
		int bit_depth3                   = load_info.image->pitch / bpp;

		// load_info.image->bytes_per_pixel = bpp / bit_depth2;
		load_info.image->bytes_per_pixel = bit_depth;

		if ( load_info.image->bytes_per_pixel == 0 )
		{
			// huh????
			FreeImage_Unload( bitmap );
			FreeImage_CloseMemory( memory );

			return false;
		}

		u8* image_bits = FreeImage_GetBits( bitmap );

		load_info.image->frame.resize( 1 );
		size_t image_size           = (size_t)load_info.image->pitch * (size_t)load_info.image->height;
		// size_t image_size           = load_info.image->width * load_info.image->height * load_info.image->bytes_per_pixel;

		load_info.image->frame[ 0 ] = ch_realloc< u8 >( load_info.image->frame[ 0 ], image_size, e_mem_category_image_data );
		memset( load_info.image->frame[ 0 ], 0, image_size * sizeof( u8 ) );

		memcpy( load_info.image->frame[ 0 ], image_bits, image_size );

		// TODO: HANDLE ANIM - LINE FROM GIF PLUGIN
		//setup frame time
		// FreeImage_SetMetadataEx( FIMD_ANIMATION, dib.get(), "FrameTime", ANIMTAG_FRAMETIME, FIDT_LONG, 1, 4, &delay_time );

		// ----------------------------------------------------------------------------------------

		if ( image_type == FIT_BITMAP )
		{
			switch ( channel_num )
			{
				case 4:
					if ( load_info.image->bit_depth != 32 )
						printf( "different bit depth?\n" );

					load_info.image->format = GL_RGBA;
					break;

				default:
				case 3:
					if ( load_info.image->bit_depth != 24 )
						printf( "different bit depth?\n" );

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

