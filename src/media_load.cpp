#include "main.h"


constexpr float            UPSCALE_LIMIT = 2.f;


job_status_t* g_media_load_status = nullptr;


struct media_load_data_t
{
	double            load_time      = 0.0;
	bool              image_load_ret = false;
	u32               file_index     = 0;
	fs::path_str      full_path{};
	media_entry_t     entry{};
	image_load_info_t load_info{};
};


void media_load_on_finish()
{
	g_image_data.index        = g_media_load_index;
	g_media_load_start_time   = 0;
	g_media_load_index        = UINT32_MAX;
	g_media_load_from_gallery = false;

	gallery_view_set_selection( g_image_data.index );

	if ( get_media_type() == e_media_type_video )
	{
		image_free_data( g_image_data.image );
		image_free_data( g_image_scaled_data.image );
	}
}


void media_load_job_finish( job_status_t* status, bool in_main_thread )
{
	g_media_load_status = nullptr;

	set_frame_draw();

	auto load_data = static_cast< media_load_data_t* >( status->userdata );

	// g_image_data.index = load_data->file_index;
	// gallery_view_set_selection( g_image_data.index );

	path_printf( "%f Load - %s\n", load_data->load_time, load_data->full_path.c_str() );

	if ( load_data->entry.type == e_media_type_image )
	{
		media_load_on_finish();
		mpv_cmd_close_video();

		if ( !load_data->image_load_ret )
		{
			path_printf( "%f FAILED Load - %s\n", load_data->load_time, load_data->full_path.c_str() );
			gl_free_textures( g_image_data.textures );
			return;
		}

		// wait for any image scale to finish before freeing this data
		media_scale_lock();
		media_view_scale_clear();

		image_free_data( g_image_data.image );

		media_scale_unlock();

		// move over data to main image pointer
		g_image_data.image = std::move( *load_data->load_info.image );

		// free the original image data
		image_free( load_data->load_info.image );

		if ( g_image_data.image.frame.size() > 0 )
		{
			gl_update_textures( g_image_data.textures, &g_image_data.image, g_image_data.image.frame.size() );
			media_view_fit_in_view();

			image_draw::frame = 0;
			media_view_frame_update_timer();

			if ( g_image_data.image.frame.size() > 1 )
			{
				if ( g_image_data.image.flags & e_image_flag_has_palette_trans )
					media_view_create_animated_image_fbo( GL_RGBA );
				else
					media_view_create_animated_image_fbo( GL_RGB );
			}
		}
	}
	else if ( load_data->entry.type == e_media_type_video )
	{
		//	image_free_data( g_image_data.image );
		//	image_free_data( g_image_scaled_data.image );
	}

	update_window_title();

	media_view_scale_reset_timer();
	media_view_scale_set_image();
}


void media_load_job_run( job_status_t* status )
{
	auto load_data  = static_cast< media_load_data_t* >( status->userdata );
	u64  start_time = sys_get_time_ms();

	// TODO: what if this is not an image and a video?
	// we need to correct the media type

	if ( load_data->entry.type == e_media_type_image )
	{
		load_data->image_load_ret = image_load( load_data->full_path, load_data->load_info );
	}
	else
	{
		std::string path_str = sys_path_to_string( load_data->full_path );
		mpv_cmd_loadfile( path_str.c_str() );
	}

	auto current_time    = sys_get_time_ms();
	load_data->load_time = ( current_time / 1000.0 ) - ( start_time / 1000.0 );
}


void media_load_job_free( job_status_t* status )
{
	if ( !status )
		return;

	auto load_data = static_cast< media_load_data_t* >( status->userdata );
	delete load_data;
}


void media_view_load( u32 index )
{
	if ( g_media_load_status || g_media_load_index != UINT32_MAX )
		return;

	if ( gallery::sorted_media.empty() )
		return;

	if ( index >= gallery::sorted_media.size() )
		return;

	g_media_load_start_time = sys_get_time_ms();
	g_media_load_index      = index;

	auto load_data          = new media_load_data_t;
	load_data->file_index   = index;
	load_data->entry        = gallery_item_get_media_entry( index );
	load_data->full_path    = directory::path / load_data->entry.file.path;

	g_media_load_status     = job_push( media_load_job_finish, media_load_job_run, media_load_job_free, load_data );
}

