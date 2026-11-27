#include "main.h"

#ifdef _MSC_VER
  #pragma warning( push )
  #pragma warning( disable : 5311 )  // ugh
#endif

#include <vtfpp/vtfpp.h>


struct LoaderVTF : public IImageLoader
{
	std::string match_ext = ".vtf";

	LoaderVTF()
	{
		image_register_codec( this, false );
	}

	~LoaderVTF()
	{
	}

	void get_supported_formats( std::vector< image_format_info_t >& formats ) override
	{
		formats.emplace_back( "VTF", match_ext );
	}

	bool check_header( const fs::path& path ) override
	{
		return true;
	}

	bool image_load( const fs::path& path, image_load_info_t& load_info, char* data, size_t data_len ) override
	{
		std::span< const std::byte > vtf_data( reinterpret_cast< const std::byte* >( data ), data_len );
		vtfpp::VTF                   vtf_file( vtf_data );

		if ( !vtf_file.hasImageData() )
			return false;

		size_t             mip        = 0;

		image_t*           image      = load_info.image;

		vtfpp::ImageFormat target_fmt = vtfpp::ImageFormat::BGR888;
		s8                 alpha_bits = vtfpp::ImageFormatDetails::decompressedAlpha( vtf_file.getFormat() );

		if ( alpha_bits > 0 )
		{
			target_fmt      = vtfpp::ImageFormat::BGRA8888;
			image->format   = GL_BGRA;
			image->channels = 4;
		}
		else
		{
			image->format   = GL_BGR;
			image->channels = 3;
		}

		if ( load_info.target_size.x != 0 )
		{
			// start with the lowest quality, and work our way up to higher quality
			// we want one step higher than the target size so we can scale down a little
			mip = vtf_file.getMipCount();

			for ( ;; )
			{
				image->width  = vtf_file.getWidth( mip );
				image->height = vtf_file.getHeight( mip );

				if ( image->width >= load_info.target_size.x && image->height >= load_info.target_size.y )
					break;

				if ( mip == 0 )
					break;

				mip--;
			}
		}
		else
		{
			image->width  = vtf_file.getWidth();
			image->height = vtf_file.getHeight();
		}

		size_t frame_count = vtf_file.getFrameCount();

		if ( frame_count == 0 )
			return false;

		if ( load_info.single_frame )
			frame_count = 1;

		image->frame.resize( frame_count );

		for ( size_t frame_i = 0; frame_i < image->frame.size(); frame_i++ )
		{
			image_frame_t& frame          = image->frame[ frame_i ];
			auto           converted_data = vtf_file.getImageDataAs( target_fmt, mip, frame_i, 0, 0 );

			frame.allocate_data( converted_data.size() );
			memcpy( frame.data, converted_data.data(), frame.size );

			frame.width          = image->width;
			frame.height         = image->height;
			frame.time           = 0.03333;  // 30 fps
			frame.frame_disposal = e_frame_disposal_background;
		}

		image->image_format = image_get_format_index_from_ext( match_ext );
		return true;
	}
};


static LoaderVTF g_loader_vtf;


#ifdef _MSC_VER
  #pragma warning( pop )
#endif

