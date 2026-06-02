#include "main.h"

#include "stb_image_resize2.h"

#include <thread>
#include <mutex>
#include <queue>
#include <unordered_map>

constexpr int       MAX_THUMBNAILS    = 512;

std::atomic< bool > g_thumbnails_running;
std::thread**       g_thumbnail_worker;
std::thread**       g_thumbnail_save_worker;

extern void*        g_mpv_module;

constexpr bool      THUMBNAIL_DEBUG_PRINT = false;


enum e_thumbnail_thread_state
{
	e_thumbnail_thread_idle,
	e_thumbnail_thread_queued,
	e_thumbnail_thread_working,
	// e_thumbnail_thread_upload

	e_thumbnail_thread_exit,
};


struct thumbnail_thread_data_t
{
	// both set to nothing when free 
	h_thumbnail                             thumbnail;
	file_t                                  file;
	std::atomic< e_thumbnail_thread_state > state;
};


struct thumbnail_saver_entry_t
{
	h_thumbnail thumbnail;
	size_t      file_hash;
};


struct thumbnail_saver_queue_t
{
	std::forward_list< thumbnail_saver_entry_t > queue;
	std::mutex                                   mutex;
	size_t                                       count;
};


// buffer for thumbnails
struct thumbnail_cache_t
{
	thumbnail_t buffer[ MAX_THUMBNAILS ];
	u32         generation[ MAX_THUMBNAILS ];
	bool        used_this_frame[ MAX_THUMBNAILS ];
};


// array the size of the amount of threads used for thumbnails
// threads wait for data to populate it
thumbnail_thread_data_t* g_thumbnail_thread_data = nullptr;
thumbnail_cache_t        g_thumbnail_cache;
thumbnail_saver_queue_t  g_thumbnail_save;

extern bool              thumbnail_save( image_t& image, const std::string& output );

// debug printing
void thumbnail_printf( const char* format, ... )
{
	if ( !THUMBNAIL_DEBUG_PRINT )
		return;

	va_list args;
	va_start( args, format );
	#if _WIN32
	vprintf_s( format, args );
	#else
	vprintf( format, args );
	#endif
	va_end( args );
}


void thumbnail_loader_free_data( u32 index )
{
	thumbnail_t& thumbnail = g_thumbnail_cache.buffer[ index ];

	if ( thumbnail.image && thumbnail.image->frame.size() )
		image_free_alloc( *thumbnail.image );

	if ( thumbnail.image_scaled && thumbnail.image_scaled->frame.size() )
		image_free_alloc( *thumbnail.image_scaled );

	if ( thumbnail.textures.count )
	{
		thumbnail_printf( "FREED %d - %s\n", index, thumbnail.path );
		gl_free_textures( thumbnail.textures );
	}

	ch_free( e_mem_category_image, thumbnail.image );
	ch_free( e_mem_category_image, thumbnail.image_scaled );

	ch_free_str( thumbnail.path );

	memset( &thumbnail, 0, sizeof( thumbnail_t ) );

	thumbnail_printf( "[THUMBNAIL %d] FREED IMAGE DATA %s\n", index, thumbnail.path );
}


h_thumbnail thumbnail_loader_queue_push( const media_entry_t& media_entry )
{
	if ( media_entry.filename.empty() )
		return {};

	// find a free thread
	u32 thread_id = 0;
	for ( ; thread_id < app::config.thumbnail_threads; thread_id++ )
	{
		if ( g_thumbnail_thread_data[ thread_id ].state == e_thumbnail_thread_idle )
			break;
	}

	// no free threads found, all busy
	if ( thread_id == app::config.thumbnail_threads )
		return {};
	
	// find a thumbnail not used this frame, it's probably off screen and we can unload it
search:
	u32  cache_pos         = 0;
	bool found_best_fit    = false;
	u32  best_fit          = 0;
	u32  best_fit_distance = 0;

	for ( ; cache_pos < MAX_THUMBNAILS; cache_pos++ )
	{
		thumbnail_t& thumbnail = g_thumbnail_cache.buffer[ cache_pos ];

		e_thumbnail_save_status save_status = thumbnail.save_status.load( std::memory_order_acquire );

		if ( save_status == e_thumbnail_save_saving )
			continue;

		// must be after save status check, thumbnail may be marked free, but image data is still in use
		if ( g_thumbnail_cache.buffer[ cache_pos ].status == e_thumbnail_status_free )
		{
			found_best_fit = false;
			break;
		}

		if ( g_thumbnail_cache.used_this_frame[ cache_pos ] )
			continue;

		e_thumbnail_status status = thumbnail.status.load( std::memory_order_acquire );
		
		if ( status == e_thumbnail_status_queued || status == e_thumbnail_status_loading || status == e_thumbnail_status_uploading || status == e_thumbnail_status_save_waiting )
		// if ( status == e_thumbnail_status_loading || status == e_thumbnail_status_uploading )
			continue;
		
		// distance only applied after thumbnail is created, thumbnail can't be free
		// not needed since best_fit_distance is already 0?
		// if ( thumbnail.distance == 0 )
		// 	continue;
		
		// if ( status == e_thumbnail_status_queued || status == e_thumbnail_status_loading )
		// 	continue;

		if ( thumbnail.distance > best_fit_distance )
		{
			found_best_fit    = true;
			best_fit          = cache_pos;
			best_fit_distance = thumbnail.distance;
		}
	}

	if ( found_best_fit )
		cache_pos = best_fit;

	if ( cache_pos == MAX_THUMBNAILS )
	{
		printf( "THUMBNAIL CACHE FULL\n" );
		return {};
	}

	if ( g_thumbnail_cache.buffer[ cache_pos ].save_status == e_thumbnail_save_saving )
	{
		//printf( "WHAT\n" );
		goto search;
	}

	g_thumbnail_cache.buffer[ cache_pos ].save_status = e_thumbnail_save_cancel;

	if ( g_thumbnail_cache.buffer[ cache_pos ].status == e_thumbnail_status_queued )
	{
		//printf( "REPLACING QUEUED THUMBNAIL - %d\n", cache_pos );
	}

	thumbnail_loader_free_data( cache_pos );
	//printf( "THUMBNAIL %d USED\n", cache_pos );

	try
	{
		g_thumbnail_cache.buffer[ cache_pos ].path        = util_strdup( media_entry.file.path.string().c_str() );
		g_thumbnail_cache.buffer[ cache_pos ].status      = e_thumbnail_status_queued;
		g_thumbnail_cache.buffer[ cache_pos ].save_status = e_thumbnail_save_idle;
		g_thumbnail_cache.buffer[ cache_pos ].type        = media_entry.type;
		g_thumbnail_cache.used_this_frame[ cache_pos ]    = true;

		h_thumbnail handle;
		handle.index      = cache_pos;
		handle.generation = ++g_thumbnail_cache.generation[ cache_pos ];

		g_thumbnail_thread_data[ thread_id ].thumbnail = handle;
		g_thumbnail_thread_data[ thread_id ].file      = media_entry.file;

		g_thumbnail_thread_data[ thread_id ].state.store( e_thumbnail_thread_queued );
		g_thumbnail_thread_data[ thread_id ].state.notify_one();

		return handle;
	}
	// fs::path.string() conversion error
	catch ( std::system_error )
	{
		g_thumbnail_cache.buffer[ cache_pos ].status = e_thumbnail_status_failed;
		return {};
	}

	return {};
}


mpv_handle* thumbnail_mpv_ctx_create( u32 thread_id )
{
	// MPV Init
	mpv_handle* local_mpv = nullptr;

	if ( g_mpv_module )
	{
		local_mpv = p_mpv_create();

		if ( local_mpv == nullptr )
		{
			printf( "mpv_create failed on thread %d!\n", thread_id );
		}
		else
		{
			int ret = 0;

			// Disable Video - So no window pops up when playing back a video
			p_mpv_set_option_string( local_mpv, "vo", "null" );

			// Disable Audio
			ret = p_mpv_set_option_string( local_mpv, "ao", "null" );

			// Low Latency Mode
			ret = p_mpv_set_option_string( local_mpv, "profile", "low-latency" );

			ret = p_mpv_set_option_string( local_mpv, "demuxer-max-bytes", "6M" );
			// ret     = p_mpv_set_option_string( local_mpv, "demuxer-max-bytes", "0" );
			ret = p_mpv_set_option_string( local_mpv, "demuxer-max-back-bytes", "0" );
			ret = p_mpv_set_option_string( local_mpv, "demuxer-donate-buffer", "no" );

			// hopefully helps? since it's just for a single screenshot
			ret = p_mpv_set_option_string( local_mpv, "demuxer-thread", "no" );

			ret = p_mpv_set_option_string( local_mpv, "video-reversal-buffer", "0" );
			ret = p_mpv_set_option_string( local_mpv, "audio-reversal-buffer", "0" );
			ret = p_mpv_set_option_string( local_mpv, "cache", "no" );
			ret = p_mpv_set_option_string( local_mpv, "cache-secs", "0" );

			ret = p_mpv_set_option_string( local_mpv, "aid", "no" );
			ret = p_mpv_set_option_string( local_mpv, "sid", "no" );

			// Only allow 1 frame used
			ret = p_mpv_set_option_string( local_mpv, "frames", "1" );

			// Start Paused
			ret = p_mpv_set_option_string( local_mpv, "pause", "" );

			// Always start at 30% of the way in the video
			ret = p_mpv_set_option_string( local_mpv, "start", "30%" );

			// Fast PNG files
			ret = p_mpv_set_option_string( local_mpv, "screenshot-high-bit-depth", "no" );
			ret = p_mpv_set_option_string( local_mpv, "screenshot-png-compression", "1" );
			ret = p_mpv_set_option_string( local_mpv, "screenshot-png-filter", "0" );

			if ( p_mpv_initialize( local_mpv ) < 0 )
			{
				printf( "mpv_initialize failed!\n" );
				p_mpv_destroy( local_mpv );
				local_mpv = nullptr;
			}
			else
			{
				// p_mpv_request_log_messages( local_mpv, "debug" );
				// p_mpv_request_log_messages( local_mpv, "warn" );
			}
		}
	}

	// let mpv startup
	// mpv_handle_wait_event( local_mpv, 0.1, mpv_thread_name );

	char mpv_thread_name[ 64 ];
	snprintf( mpv_thread_name, 64, "MPV THREAD %d", thread_id );

	mpv_event* event = p_mpv_wait_event( local_mpv, -1 );

	while ( event->event_id != MPV_EVENT_NONE )
	{
		if ( event->event_id == MPV_EVENT_LOG_MESSAGE )
		{
			struct mpv_event_log_message* msg = (struct mpv_event_log_message*)event->data;
			printf( "%s: [%s] %s: %s", mpv_thread_name, msg->prefix, msg->level, msg->text );
		}
		else if ( event->event_id == MPV_EVENT_IDLE )
		{
			break;
		}

		event = p_mpv_wait_event( local_mpv, -1 );
	}

	return local_mpv;
}


void thumbnail_mpv_ctx_free( mpv_handle*& ctx )
{
	p_mpv_destroy( ctx );
	ctx = nullptr;
}


size_t thumbnail_generate_hash( file_t& file )
{
	// take full file path + date modified + file size
	size_t hash = 0;

	hash ^= std::hash< fs::path >{}( file.path );
	hash ^= std::hash< size_t >{}( file.size );
	hash ^= std::hash< size_t >{}( file.date_mod );
	hash ^= std::hash< size_t >{}( file.date_created );

	hash ^= std::hash< float >{}( app::config.thumbnail_jxl_distance );
	hash ^= std::hash< u32 >{}( app::config.thumbnail_jxl_effort );
	hash ^= std::hash< u32 >{}( app::config.thumbnail_size );

	return hash;
}


void thumbnail_save_worker()
{
	while ( g_thumbnails_running.load( std::memory_order_acquire ) )
	{
		if ( g_thumbnail_save.queue.empty() )
		{
			SDL_Delay( 250 );
			continue;
		}

		g_thumbnail_save.mutex.lock();

		// already taken?
		if ( g_thumbnail_save.queue.empty() )
		{
			g_thumbnail_save.mutex.unlock();
			continue;
		}

		thumbnail_saver_entry_t entry = g_thumbnail_save.queue.front();
		g_thumbnail_save.queue.pop_front();
		g_thumbnail_save.count--;

		g_thumbnail_save.mutex.unlock();

		// is this invalid now? waited too long
		if ( !handle_list_valid( MAX_THUMBNAILS, g_thumbnail_cache.generation, entry.thumbnail ) )
			continue;

		thumbnail_t* thumbnail = &g_thumbnail_cache.buffer[ entry.thumbnail.index ];

		if ( thumbnail->save_status == e_thumbnail_save_cancel )
			continue;

		if ( thumbnail->save_status == e_thumbnail_save_idle )
			continue;

		thumbnail->save_status     = e_thumbnail_save_saving;

		std::string thumbnail_path = app::config.thumbnail_cache_path;
		thumbnail_path += SEP_S;
		thumbnail_path += std::to_string( entry.file_hash );
		thumbnail_path += ".jxl";

		float image_size = std::max( thumbnail->image->width, thumbnail->image->height );

		// Downscale first if needed
		if ( image_size > app::config.thumbnail_size )
		{
			float factor[ 2 ]  = { 1.f, 1.f };

			factor[ 0 ]        = (float)app::config.thumbnail_size / (float)thumbnail->image->width;
			factor[ 1 ]        = (float)app::config.thumbnail_size / (float)thumbnail->image->height;

			float   scale      = std::min( factor[ 0 ], factor[ 1 ] );

			float   new_width  = thumbnail->image->width * scale;
			float   new_height = thumbnail->image->height * scale;

			image_t new_image{};

			//printf( "[THUMBNAIL %d] SAVING %s\n", entry.thumbnail.index, thumbnail->path );

			if ( image_scale( thumbnail->image, &new_image, new_width, new_height ) )
			{
				thumbnail_save( new_image, thumbnail_path );
				image_free( new_image );
			}
			else
			{
				printf( "Failed to downscale image for thumbnail cache!\n" );
			}
		}
		else
		{
			thumbnail_save( *thumbnail->image, thumbnail_path );
		}

		//printf( "[THUMBNAIL %d] SAVED %s\n", entry.thumbnail.index, thumbnail->path );
		thumbnail->save_status = e_thumbnail_save_finished;
	}
}


void thumbnail_save_push( h_thumbnail thumbnail_handle, thumbnail_t* thumbnail, size_t file_hash )
{
	std::lock_guard lock( g_thumbnail_save.mutex );

	thumbnail_saver_entry_t entry{
		.thumbnail = thumbnail_handle,
		.file_hash = file_hash,
	};

	g_thumbnail_save.queue.emplace_front( entry );
	g_thumbnail_save.count++;
}


bool thumbnail_loader_load_source_from_disk( thumbnail_t* thumbnail, mpv_handle*& local_mpv, int thread_id, char* mpv_thread_name, char* video_thumbnail_path )
{
	// No thumbnail was found on disk, load the source file and generate one
	if ( thumbnail->type == e_media_type_video )
	{
		if ( !local_mpv )
		{
			local_mpv = thumbnail_mpv_ctx_create( thread_id );
		}

		// Use the local mpv instance to capture a frame from the video
		if ( !local_mpv )
		{
			thumbnail->status = e_thumbnail_status_failed;
			return false;
		}

		bool        failed  = false;

		// mpv_handle_wait_event( local_mpv, 0.1, mpv_thread_name );

		const char* cmd[]   = { "loadfile", thumbnail->path, NULL };
		int         cmd_ret = p_mpv_command_async( local_mpv, NULL, cmd );

		mpv_event*  event   = p_mpv_wait_event( local_mpv, -1 );

		while ( event->event_id != MPV_EVENT_NONE )
		{
			if ( event->event_id == MPV_EVENT_LOG_MESSAGE )
			{
				struct mpv_event_log_message* msg = (struct mpv_event_log_message*)event->data;
				printf( "%s: [%s] %s: %s", mpv_thread_name, msg->prefix, msg->level, msg->text );
			}
			else if ( event->event_id == MPV_EVENT_COMMAND_REPLY )
			{
				if ( event->error != 0 )
				{
					printf( "failed to load video for thumbnail - %d\n", event->error );
					failed = true;
					break;
				}
			}
			else if ( event->event_id == MPV_EVENT_PLAYBACK_RESTART )
			{
				break;
			}

			event = p_mpv_wait_event( local_mpv, -1 );
		}

		if ( failed )
		{
			thumbnail->status = e_thumbnail_status_failed;
			return false;
		}

		// TODO: USE screenshot-raw
		const char* cmd3[] = { "screenshot-to-file", video_thumbnail_path, NULL };
		cmd_ret            = p_mpv_command_async( local_mpv, NULL, cmd3 );

		event              = p_mpv_wait_event( local_mpv, -1 );

		while ( event->event_id != MPV_EVENT_NONE )
		{
			if ( event->event_id == MPV_EVENT_LOG_MESSAGE )
			{
				struct mpv_event_log_message* msg = (struct mpv_event_log_message*)event->data;

				if ( msg->log_level == MPV_LOG_LEVEL_ERROR )
				{
					printf( "ERROR: %s: [%s] %s: %s - %d", mpv_thread_name, msg->prefix, msg->level, msg->text, event->error );
				}
				else
				{
					printf( "%s: [%s] %s: %s", mpv_thread_name, msg->prefix, msg->level, msg->text );
				}
			}
			else if ( event->event_id == MPV_EVENT_COMMAND_REPLY )
			{
				if ( event->error != 0 )
				{
					printf( "failed to write screenshot for thumbnail - %d\n", event->error );
					failed = true;
					break;
				}

				break;
			}

			event = p_mpv_wait_event( local_mpv, -1 );
		}

		if ( failed )
		{
			// Clear Video from MPV
			const char* cmd_clear[] = { "stop", NULL };
			cmd_ret                 = p_mpv_command_async( local_mpv, NULL, cmd_clear );

			thumbnail->status       = e_thumbnail_status_failed;
			return false;
		}

		// Clear Video from MPV
		const char* cmd_clear[] = { "stop", NULL };
		cmd_ret                 = p_mpv_command_async( local_mpv, NULL, cmd_clear );

		// Load Image Normally
		thumbnail->image        = ch_calloc< image_t >( 1, e_mem_category_image );

		image_load_info_t load_info{};
		load_info.image          = thumbnail->image;
		load_info.load_quick     = true;
		load_info.threaded_load  = true;
		load_info.thumbnail_load = true;
		load_info.target_size.x  = app::config.thumbnail_size;
		load_info.target_size.y  = app::config.thumbnail_size;

		if ( !image_load( video_thumbnail_path, load_info ) )
		{
			printf( "FAILED TO LOAD IMAGE: %s\n", video_thumbnail_path );
			thumbnail->status = e_thumbnail_status_failed;
			return false;
		}

		// TODO: Delete Image? it might just slow this down a bit, since it always just gets overwritten later
	}
	// ---------------------------------------------------------------------------------------------------------
	else
	{
		// Load Image Normally
		thumbnail->image = ch_calloc< image_t >( 1, e_mem_category_image );

		image_load_info_t load_info{};
		load_info.image          = thumbnail->image;
		load_info.load_quick     = true;
		load_info.threaded_load  = true;
		load_info.thumbnail_load = true;
		load_info.single_frame   = true;
		load_info.target_size.x  = app::config.thumbnail_size;
		load_info.target_size.y  = app::config.thumbnail_size;

		if ( !image_load( thumbnail->path, load_info ) )
		{
			printf( "FAILED TO LOAD IMAGE: %s\n", thumbnail->path );
			thumbnail->status = e_thumbnail_status_failed;
			return false;
		}
	}

	return true;
}


void thumbnail_loader_worker( u32 thread_id )
{
	char  video_thumbnail_path[ 512 ];
	snprintf( video_thumbnail_path, 512, "%s" SEP_S "video_thumbnail_thread_%d.png", app::config.thumbnail_video_cache_path.c_str(), thread_id );

	char mpv_thread_name[ 64 ];
	snprintf( mpv_thread_name, 64, "MPV THREAD %d", thread_id );

	mpv_handle* local_mpv = nullptr;

	thumbnail_thread_data_t& thread_data = g_thumbnail_thread_data[ thread_id ];

	// Enter Loop
	while ( g_thumbnails_running.load( std::memory_order_acquire ) )
	{
		// set to idle
		thread_data.state.store( e_thumbnail_thread_idle );

		// wait for queued data here
		thread_data.state.wait( e_thumbnail_thread_idle );

		if ( thread_data.state == e_thumbnail_thread_exit )
			break;

		// if (  == e_thumbnail_thread_idle )
		// {
		// 	if ( local_mpv )
		// 		thumbnail_mpv_ctx_free( local_mpv );
		// 
		// 	SDL_Delay( 250 );
		// 	continue;
		// }

		thumbnail_t* thumbnail = &g_thumbnail_cache.buffer[ thread_data.thumbnail.index ];

		if ( !thumbnail )
			continue;

		if ( thumbnail->status == e_thumbnail_status_failed )
			continue;

		thumbnail->status.store( e_thumbnail_status_loading );

		thumbnail_printf( "[THUMBNAIL %d][THREAD %d] STARTING LOAD OF IMAGE: %s\n", thread_data.thumbnail.index, thread_id, thumbnail->path );

		size_t            file_hash               = thumbnail_generate_hash( thread_data.file );

		u32               thumbnail_size = gallery::image_size;

		//if ( app::config.thumbnail_use_fixed_size || app::config.thumbnail_jxl_enable )
		//	thumbnail_size = app::config.thumbnail_size;

		bool              thumbnail_found_on_disk = false;
		image_load_info_t jxl_thumbnail{};

		if ( app::config.thumbnail_jxl_enable )
		{
			std::string thumbnail_path = app::config.thumbnail_cache_path;
			thumbnail_path += SEP_S;
			thumbnail_path += std::to_string( file_hash );
			thumbnail_path += ".jxl";

			if ( fs_is_file( thumbnail_path.c_str() ) )
			{
				// Load Image Normally
				jxl_thumbnail.image          = ch_calloc< image_t >( 1, e_mem_category_image );
				jxl_thumbnail.target_size.x  = thumbnail_size;
				jxl_thumbnail.target_size.y  = thumbnail_size;
				jxl_thumbnail.load_quick     = true;
				jxl_thumbnail.threaded_load  = true;
				jxl_thumbnail.thumbnail_load = true;
				jxl_thumbnail.quiet          = true;

				if ( image_load( thumbnail_path.c_str(), jxl_thumbnail ) )
				{
					thumbnail_found_on_disk = true;

					if ( thumbnail->image )
						ch_free( e_mem_category_image, thumbnail->image );

					thumbnail->image        = jxl_thumbnail.image;
					jxl_thumbnail.image     = nullptr;

					//if ( args_register_bool( "spew jxl loads", "--jxl-spew" ) )
					//	printf( "JXL THUMBNAIL LOADED: %s - %zu\n", thumbnail->path, file_hash );
				}
				else
				{
					ch_free( e_mem_category_image, jxl_thumbnail.image );
				}
			}
		}

		// ---------------------------------------------------------------------------------------------------------

		// if ( ( app::config.thumbnail_jxl_enable && !thumbnail_found_on_disk ) || ( !app::config.thumbnail_jxl_enable ) )
		if ( !thumbnail_found_on_disk )
		{
			// No thumbnail was found on disk, load the source file and generate one
			if ( !thumbnail_loader_load_source_from_disk( thumbnail, local_mpv, thread_id, mpv_thread_name, video_thumbnail_path ) )
			{
				thumbnail->status = e_thumbnail_status_failed;
				continue;
			}
		}

		// ---------------------------------------------------------------------------------------------------------

		if ( !thumbnail->image )
		{
			printf( "THUMBNAIL IMAGE IS NULLPTR ?????\n" );
			thumbnail->status = e_thumbnail_status_failed;
			continue;
		}

		if ( thumbnail->image->frame.empty() || !thumbnail->image->frame[ 0 ].data )
		{
			printf( "data is nullptr in worker?\n" );
			thumbnail->status = e_thumbnail_status_failed;
			continue;
		}

		thumbnail_printf( "[THUMBNAIL %d] LOADED IMAGE: %s\n", thread_data.thumbnail.index, thumbnail->path );

		float max_image_size = std::max( thumbnail->image->width, thumbnail->image->height );

		// ---------------------------------------------------------------------------------------------------------
		// If we didn't find the thumbnail on disk, write it!

		bool  saving_thumbnail = false;

		if ( app::config.thumbnail_jxl_enable && !thumbnail_found_on_disk )
		{
			// Make sure it's not in the cache folder
			std::string cleaned_path = fs_path_clean( thumbnail->path, strlen( thumbnail->path ) );

			if ( !cleaned_path.starts_with( app::config.thumbnail_cache_path ) )
			{
				//printf( "SAVING JXL THUMBNAIL FOR %s\n", thumbnail->path );
				thumbnail->save_status = e_thumbnail_save_queued;
				thumbnail_save_push( thread_data.thumbnail, thumbnail, file_hash );

				// Downscale first
			//	if ( max_image_size > app::config.thumbnail_size )
			//	{
			//		float factor[ 2 ]  = { 1.f, 1.f };
			//
			//		factor[ 0 ]        = (float)app::config.thumbnail_size / (float)thumbnail->image->width;
			//		factor[ 1 ]        = (float)app::config.thumbnail_size / (float)thumbnail->image->height;
			//
			//		float   scale      = std::min( factor[ 0 ], factor[ 1 ] );
			//
			//		float   new_width  = thumbnail->image->width * scale;
			//		float   new_height = thumbnail->image->height * scale;
			//
			//		image_t new_image{};
			//
			//		if ( image_scale( thumbnail->image, &new_image, new_width, new_height ) )
			//		{
			//			std::string thumbnail_path = app::config.thumbnail_cache_path;
			//			thumbnail_path += SEP_S;
			//			thumbnail_path += std::to_string( file_hash );
			//			thumbnail_path += ".jxl";
			//
			//			thumbnail_save( new_image, thumbnail_path );
			//
			//			// ch_free( e_mem_category_image_data, new_image.frame[ 0 ].data );
			//		}
			//		else
			//		{
			//			printf( "Failed to downscale image for thumbnail cache!\n" );
			//		}
			//	}
			//	else
			//	{
			//		std::string thumbnail_path = app::config.thumbnail_cache_path;
			//		thumbnail_path += SEP_S;
			//		thumbnail_path += std::to_string( file_hash );
			//		thumbnail_path += ".jxl";
			//
			//		thumbnail_save( *thumbnail->image, thumbnail_path );
			//	}
			}
		}

		// ---------------------------------------------------------------------------------------------------------
		// Scale image to give a nicer thumbnail preview

		{
			float factor[ 2 ] = {
				(float)thumbnail_size / (float)thumbnail->image->width,
				(float)thumbnail_size / (float)thumbnail->image->height
			};

			float scale_amount = std::min( factor[ 0 ], factor[ 1 ] );

			if ( scale_amount != 0.f && scale_amount < 2.f && scale_amount != 1.f )
			{
				float new_width         = thumbnail->image->width * scale_amount;
				float new_height        = thumbnail->image->height * scale_amount;

				u8*   old_frame         = thumbnail->image->frame[ 0 ].data;

				thumbnail->image_scaled = ch_calloc< image_t >( 1, e_mem_category_image );

				if ( image_scale( thumbnail->image, thumbnail->image_scaled, new_width, new_height ) )
				{
					// thumbnail->scaled = true;
					// ch_free( e_mem_category_image_data, old_frame );
				}
			}
		}

		thumbnail->status.store( e_thumbnail_status_uploading, std::memory_order_release );
	}
}


bool thumbnail_loader_init()
{
	g_thumbnails_running.store( true );

	g_thumbnail_worker      = ch_calloc< std::thread* >( app::config.thumbnail_threads, e_mem_category_general );
	g_thumbnail_save_worker = ch_calloc< std::thread* >( app::config.thumbnail_save_threads, e_mem_category_general );
	g_thumbnail_thread_data = new thumbnail_thread_data_t[ app::config.thumbnail_threads ];

	for ( int i = 0; i < app::config.thumbnail_save_threads; i++ )
	{
		g_thumbnail_save_worker[ i ] = new std::thread( thumbnail_save_worker );
	}

	for ( int i = 0; i < app::config.thumbnail_threads; i++ )
	{
		g_thumbnail_worker[ i ] = new std::thread( thumbnail_loader_worker, i );
	}

	return true;
}


void thumbnail_loader_shutdown()
{
	if ( !g_thumbnails_running )
		return;

	g_thumbnails_running.store( false );

	// wait for threads to shutdown
	for ( int i = 0; i < app::config.thumbnail_save_threads; i++ )
	{
		g_thumbnail_save_worker[ i ]->join();
		delete g_thumbnail_save_worker[ i ];
	}

	for ( int i = 0; i < app::config.thumbnail_threads; i++ )
	{
		g_thumbnail_thread_data[ i ].state = e_thumbnail_thread_exit;
		g_thumbnail_thread_data[ i ].state.notify_one();
		g_thumbnail_worker[ i ]->join();
		delete g_thumbnail_worker[ i ];
	}

	ch_free( e_mem_category_general, g_thumbnail_worker );

	delete[] g_thumbnail_thread_data;

	g_thumbnail_thread_data = nullptr;
	g_thumbnail_save_worker = nullptr;
	g_thumbnail_worker      = nullptr;

	// Free thumbnails
	for ( u32 i = 0; i < MAX_THUMBNAILS; i++ )
	{
		thumbnail_loader_free_data( i );
	}
}


void thumbnail_loader_update()
{
	// int max = g_thumbnail_cache.write_pos;
	// 
	// if ( max <= g_thumbnail_cache.read_pos )
	// 	max += ( MAX_THUMBNAILS - g_thumbnail_cache.read_pos );

	// for ( u32 i = 0; i < max; i++ )
	u32 upload_count = 0;
	for ( u32 i = 0; i < MAX_THUMBNAILS; i++ )
	{
		if ( upload_count == app::config.thumbnail_uploads_per_frame )
			return;

		// reset all of these
		g_thumbnail_cache.used_this_frame[ i ] = false;

		if ( g_thumbnail_cache.generation[ i ] == 0 )
			continue;

		thumbnail_t&       thumbnail = g_thumbnail_cache.buffer[ i ];

		bool               uploaded_image = false;

		if ( thumbnail.status.load( std::memory_order_acquire ) == e_thumbnail_status_uploading )
		{
			if ( thumbnail.image->frame.empty() || !thumbnail.image->frame[ 0 ].data )
			{
				printf( "thumbnail data is nullptr\n" );
			}

			if ( thumbnail.image_scaled )
			{
				gl_update_textures( thumbnail.textures, thumbnail.image_scaled, 1 );
				image_free_alloc( *thumbnail.image_scaled );
			}
			else
			{
				gl_update_textures( thumbnail.textures, thumbnail.image, 1 );
			}

			thumbnail.status = e_thumbnail_status_finished;
			uploaded_image   = true;
			upload_count++;
			set_frame_draw();
		}

		if ( thumbnail.status == e_thumbnail_status_finished && thumbnail.save_status != e_thumbnail_save_saving && thumbnail.save_status != e_thumbnail_save_queued )
		{
			if ( thumbnail.image && thumbnail.image->frame.size() )
			{
				//printf( "[THUMBNAIL %d] FREED SRC DATA FOR %s\n", i, thumbnail.path );
				image_free_alloc( *thumbnail.image );
			}
		}
	}
}


thumbnail_t* thumbnail_get_data( h_thumbnail handle )
{
	if ( !handle_list_valid( MAX_THUMBNAILS, g_thumbnail_cache.generation, handle ) )
	{
		// thumbnail_printf( "Requesting Invalid Thumbnail!\n" );
		return nullptr;
	}

	g_thumbnail_cache.used_this_frame[ handle.index ] = true;
	return &g_thumbnail_cache.buffer[ handle.index ];
}


// distance based cache
void thumbnail_update_distance( h_thumbnail handle, u32 distance )
{
	if ( !handle_list_valid( MAX_THUMBNAILS, g_thumbnail_cache.generation, handle ) )
		return;

	g_thumbnail_cache.buffer[ handle.index ].distance = distance;
}


void thumbnail_clear_cache()
{
	for ( u32 i = 0; i < MAX_THUMBNAILS; i++ )
	{
		g_thumbnail_cache.used_this_frame[ i ] = false;

		if ( g_thumbnail_cache.buffer[ i ].status == e_thumbnail_status_queued || g_thumbnail_cache.buffer[ i ].status == e_thumbnail_status_finished )
			g_thumbnail_cache.buffer[ i ].status = e_thumbnail_status_free;
	}
}


void thumbnail_cache_debug_draw()
{
	ImGui::SeparatorText( "Thumbnail System" );
	ImGui::Text( "Thread Count: %u", app::config.thumbnail_threads );
	ImGui::Text( "Save Thread Count: %u", app::config.thumbnail_save_threads );

	ImGui::Text( "Drawn Image Count: %u", gallery::drawn_image_count );
	ImGui::Text( "Thumbnail Save Queue Size: %u", g_thumbnail_save.count );
}

