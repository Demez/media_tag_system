#include "main.h"

#include "stb_image_resize2.h"

#include <thread>
#include <mutex>
#include <queue>
#include <unordered_map>

constexpr int       JOB_QUEUE_SIZE    = 64;  // if this is too high, it can cause noticable hitches when uploading thumbnails
constexpr int       MAX_THUMBNAILS    = 512;

std::atomic< bool > g_thumbnails_running;
std::thread**       g_thumbnail_worker;
std::mutex          g_thumbnail_mutex;

extern void*        g_mpv_module;

constexpr bool      THUMBNAIL_DEBUG_PRINT = false;

enum e_job_state
{
	e_job_state_free,
	e_job_state_working,
	e_job_state_finished,
};


struct thumbnail_job_t
{
	h_thumbnail thumbnail;
	e_job_state state;
	file_t      file;
};


// ring buffer attempt
struct thumbnail_queue_t
{
	thumbnail_job_t    buffer[ JOB_QUEUE_SIZE ];
	std::atomic< u32 > write_pos;  // write position in buffer for adding new jobs from main thread
	std::atomic< u32 > read_pos;   // where to read the next job from a worker thread
};


// buffer for thumbnails
struct thumbnail_cache_t
{
	thumbnail_t buffer[ MAX_THUMBNAILS ];
	u32         generation[ MAX_THUMBNAILS ];
	bool        used_this_frame[ MAX_THUMBNAILS ];
};


thumbnail_queue_t g_thumbnail_queue;
thumbnail_cache_t g_thumbnail_cache;


extern bool       thumbnail_save( image_t& image, const std::string& output );


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


void thumbnail_free_host_image_data( size_t thumbnail_slot, thumbnail_t& thumbnail )
{
	if ( !thumbnail.image )
		return;

	if ( thumbnail.image->frame.size() )
	{
		if ( thumbnail.scaled )
			ch_free( e_mem_category_stbi_resize, thumbnail.image->frame[ 0 ] );
		else
			ch_free( e_mem_category_image_data, thumbnail.image->frame[ 0 ] );

		thumbnail.image->frame[ 0 ] = nullptr;
	}

	thumbnail.image->frame.clear();

	image_free_alloc( *thumbnail.image );

	thumbnail_printf( "[THUMBNAIL %d] FREED IMAGE DATA %s\n", thumbnail_slot, thumbnail.path );
}


void thumbnail_loader_free_data( u32 index )
{
	thumbnail_t& thumbnail = g_thumbnail_cache.buffer[ index ];

	thumbnail_free_host_image_data( index, thumbnail );

	if ( thumbnail.texture )
	{
		thumbnail_printf( "FREED %d - %s\n", index, thumbnail.path );
		gl_free_texture( thumbnail.texture );
		thumbnail.im_texture = nullptr;
	}

	ch_free( e_mem_category_image, thumbnail.image );
	ch_free_str( thumbnail.path );

	memset( &thumbnail, 0, sizeof( thumbnail_t ) );
}


h_thumbnail thumbnail_loader_queue_push( const media_entry_t& media_entry )
{
	if ( media_entry.filename.empty() )
		return {};

	// TODO: some jobs here may be invalidated quickly if the user is scrolling very fast
	// maybe we could use the same distance value to determine if a queued job could be thrown out,
	// and instead replaced with an image just requested now?

	// don't care about load order since this is called from the main thread, just get current value
	u32 current_pos = g_thumbnail_queue.write_pos.load( std::memory_order_relaxed );
	u32 next_pos    = ( current_pos + 1 ) % JOB_QUEUE_SIZE;

	// make sure the queue isn't full, this slot might be used by a worker
	// also use acquire to make sure reads/writes happen after
	if ( next_pos == g_thumbnail_queue.read_pos.load( std::memory_order_acquire ) )
	{
		thumbnail_printf( "THUMBNAIL QUEUE FULL\n" );
		return {};
	}

	// the queue is not full, so create a new job for it
	thumbnail_job_t& job = g_thumbnail_queue.buffer[ current_pos ];
	
	// find a thumbnail not used this frame, it's probably off screen and we can unload it
	u32  cache_pos         = 0;
	bool found_best_fit    = false;
	u32  best_fit          = 0;
	u32  best_fit_distance = 0;
	for ( ; cache_pos < MAX_THUMBNAILS; cache_pos++ )
	{
		if ( g_thumbnail_cache.buffer[ cache_pos ].status == e_thumbnail_status_free )
		{
			found_best_fit = false;
			break;
		}

		e_thumbnail_status status = g_thumbnail_cache.buffer[ cache_pos ].status.load( std::memory_order_acquire );
		
		if ( status == e_thumbnail_status_queued || status == e_thumbnail_status_loading || status == e_thumbnail_status_uploading )
			continue;
		
		// if ( status == e_thumbnail_status_queued || status == e_thumbnail_status_loading )
		// 	continue;

		if ( g_thumbnail_cache.used_this_frame[ cache_pos ] )
			continue;

		thumbnail_t& thumbnail = g_thumbnail_cache.buffer[ cache_pos ];

		if ( thumbnail.distance == 0 )
			continue;

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
		thumbnail_printf( "THUMBNAIL CACHE FULL\n" );
		return {};
	}

	thumbnail_loader_free_data( cache_pos );
	//printf( "THUMBNAIL %d USED\n", cache_pos );
	//printf( "ADDED JOB %d\n", current_pos );

	h_thumbnail handle;
	handle.index         = cache_pos;
	handle.generation    = ++g_thumbnail_cache.generation[ cache_pos ];

	job.thumbnail                                  = handle;
	job.state                                      = e_job_state_working;
	job.file                                       = media_entry.file;

	g_thumbnail_cache.buffer[ cache_pos ].status   = e_thumbnail_status_queued;
	g_thumbnail_cache.buffer[ cache_pos ].path     = util_strdup( media_entry.file.path.string().c_str() );
	g_thumbnail_cache.buffer[ cache_pos ].type     = media_entry.type;
	g_thumbnail_cache.used_this_frame[ cache_pos ] = true;

	// update the write position in the queue, use release to wait for all reads to finish before updating this
	g_thumbnail_queue.write_pos.store( next_pos, std::memory_order_release );

	return handle;
}


thumbnail_job_t* thumbnail_loader_queue_pop( u32& job_id )
{
	g_thumbnail_mutex.lock();

	u32 current_pos = g_thumbnail_queue.read_pos.load( std::memory_order_relaxed );

	// make sure we aren't at the write position, nothing new added yet then
	if ( current_pos == g_thumbnail_queue.write_pos.load( std::memory_order_acquire ) )
	{
		g_thumbnail_mutex.unlock();
		return nullptr;
	}

	thumbnail_job_t* job = &g_thumbnail_queue.buffer[ current_pos ];
	job_id               = current_pos;

	g_thumbnail_queue.read_pos.store( ( current_pos + 1 ) % JOB_QUEUE_SIZE, std::memory_order_release );

	g_thumbnail_mutex.unlock();
	return job;
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


void thumbnail_loader_worker( u32 thread_id )
{
	char  video_thumbnail_path[ 512 ];
	snprintf( video_thumbnail_path, 512, "%s" SEP_S "video_thumbnail_thread_%d.png", app::config.thumbnail_video_cache_path.c_str(), thread_id );

	char mpv_thread_name[ 64 ];
	snprintf( mpv_thread_name, 64, "MPV THREAD %d", thread_id );

	mpv_handle* local_mpv = nullptr;

	// Enter Loop
	while ( g_thumbnails_running.load( std::memory_order_acquire ) )
	{
		u32              job_id = 0;
		thumbnail_job_t* job = thumbnail_loader_queue_pop( job_id );

		if ( !job )
		{
			if ( local_mpv )
				thumbnail_mpv_ctx_free( local_mpv );

			SDL_Delay( 250 );
			continue;
		}

		thumbnail_t* thumbnail = &g_thumbnail_cache.buffer[ job->thumbnail.index ];

		if ( !thumbnail )
			continue;

		thumbnail_printf( "[THUMBNAIL %d][JOB %d][THREAD %d] STARTING LOAD OF IMAGE: %s\n", job->thumbnail.index, job_id, thread_id, thumbnail->path );

		thumbnail->status.store( e_thumbnail_status_loading, std::memory_order_release );

		size_t            file_hash      = thumbnail_generate_hash( job->file );

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

		if ( app::config.thumbnail_jxl_enable && !thumbnail_found_on_disk )
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
					continue;
				}

				bool        failed       = false;

				// mpv_handle_wait_event( local_mpv, 0.1, mpv_thread_name );

				const char* cmd[]        = { "loadfile", thumbnail->path, NULL };
				int         cmd_ret      = p_mpv_command_async( local_mpv, NULL, cmd );

				mpv_event*  event        = p_mpv_wait_event( local_mpv, -1 );

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
					continue;
				}

				// TODO: USE screenshot-raw
				const char* cmd3[]       = { "screenshot-to-file", video_thumbnail_path, NULL };
				cmd_ret                  = p_mpv_command_async( local_mpv, NULL, cmd3 );

				event         = p_mpv_wait_event( local_mpv, -1 );

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
					continue;
				}

				// Clear Video from MPV
				const char* cmd_clear[]  = { "stop", NULL };
				cmd_ret                 = p_mpv_command_async( local_mpv, NULL, cmd_clear );

				// Load Image Normally
				thumbnail->image         = ch_calloc< image_t >( 1, e_mem_category_image );

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
					continue;
				}
			
				// TODO: Delete Image? it might just slow this down a bit, since it always just gets overwritten later
			}
			// ---------------------------------------------------------------------------------------------------------
			else
			{
				// Load Image Normally
				thumbnail->image         = ch_calloc< image_t >( 1, e_mem_category_image );

				image_load_info_t load_info{};
				load_info.image          = thumbnail->image;
				load_info.load_quick     = true;
				load_info.threaded_load  = true;
				load_info.thumbnail_load = true;
				load_info.target_size.x  = app::config.thumbnail_size;
				load_info.target_size.y  = app::config.thumbnail_size;

				if ( !image_load( thumbnail->path, load_info ) )
				{
					printf( "FAILED TO LOAD IMAGE: %s\n", thumbnail->path );
					thumbnail->status = e_thumbnail_status_failed;
					continue;
				}
			}
		}

		// ---------------------------------------------------------------------------------------------------------

		if ( thumbnail->image->frame.empty() || !thumbnail->image->frame[ 0 ] )
		{
			printf( "data is nullptr in worker?\n" );
			thumbnail->status = e_thumbnail_status_failed;
			continue;
		}

		thumbnail_printf( "[THUMBNAIL %d] LOADED IMAGE: %s\n", job->thumbnail.index, thumbnail->path );

		float max_image_size = std::max( thumbnail->image->width, thumbnail->image->height );

		// ---------------------------------------------------------------------------------------------------------
		// If we didn't find the thumbnail on disk, write it!

		if ( app::config.thumbnail_jxl_enable && !thumbnail_found_on_disk )
		{
			// Make sure it's not in the cache folder
			std::string cleaned_path = fs_path_clean( thumbnail->path, strlen( thumbnail->path ) );

			if ( !cleaned_path.starts_with( app::config.thumbnail_cache_path ) )
			{
				// Downscale first
				if ( max_image_size > app::config.thumbnail_size )
				{
					float factor[ 2 ]  = { 1.f, 1.f };

					factor[ 0 ]        = (float)app::config.thumbnail_size / (float)thumbnail->image->width;
					factor[ 1 ]        = (float)app::config.thumbnail_size / (float)thumbnail->image->height;

					float   scale      = std::min( factor[ 0 ], factor[ 1 ] );

					float   new_width  = thumbnail->image->width * scale;
					float   new_height = thumbnail->image->height * scale;

					image_t new_image{};

					if ( image_scale( thumbnail->image, &new_image, new_width, new_height ) )
					{
						std::string thumbnail_path = app::config.thumbnail_cache_path;
						thumbnail_path += SEP_S;
						thumbnail_path += std::to_string( file_hash );
						thumbnail_path += ".jxl";

						thumbnail_save( new_image, thumbnail_path );

						ch_free( e_mem_category_stbi_resize, new_image.frame[ 0 ] );
					}
					else
					{
						printf( "Failed to downscale image for thumbnail cache!\n" );
					}
				}
				else
				{
					std::string thumbnail_path = app::config.thumbnail_cache_path;
					thumbnail_path += SEP_S;
					thumbnail_path += std::to_string( file_hash );
					thumbnail_path += ".jxl";

					thumbnail_save( *thumbnail->image, thumbnail_path );
				}
			}
		}

		// ---------------------------------------------------------------------------------------------------------
		// Downscale image if size is larger than target 

		if ( max_image_size > thumbnail_size )
		// if ( 0 )
		{
			float factor[ 2 ]      = { 1.f, 1.f };

			factor[ 0 ]            = (float)thumbnail_size / (float)thumbnail->image->width;
			factor[ 1 ]            = (float)thumbnail_size / (float)thumbnail->image->height;

			float downscale_amount = std::min( factor[ 0 ], factor[ 1 ] );
			// float downscale_amount       = 0.5f;

			float new_width        = thumbnail->image->width * downscale_amount;
			float new_height       = thumbnail->image->height * downscale_amount;

			u8*   old_frame        = thumbnail->image->frame[ 0 ];

			if ( image_scale( thumbnail->image, thumbnail->image, new_width, new_height ) )
			{
				thumbnail->scaled = true;
				ch_free( e_mem_category_image_data, old_frame );
			}
		}

		job->state = e_job_state_free;

		thumbnail->status.store( e_thumbnail_status_uploading, std::memory_order_release );
	}
}


bool thumbnail_loader_init()
{
	g_thumbnail_queue.write_pos = 0;
	g_thumbnail_queue.read_pos  = 0;

	g_thumbnails_running.store( true );
	g_thumbnail_worker = ch_calloc< std::thread* >( app::config.thumbnail_threads, e_mem_category_general );

	for ( int i = 0; i < app::config.thumbnail_threads; i++ )
	{
		g_thumbnail_worker[ i ] = new std::thread( thumbnail_loader_worker, i );
	}

	return true;
}


void thumbnail_loader_shutdown()
{
	g_thumbnails_running.store( false );

	// wait for threads to shutdown
	for ( int i = 0; i < app::config.thumbnail_threads; i++ )
	{
		g_thumbnail_worker[ i ]->join();
		delete g_thumbnail_worker[ i ];
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

		// thumbnail_t& thumbnail = g_thumbnail_cache.buffer[ g_thumbnail_cache.read_pos ];
		thumbnail_t&       thumbnail = g_thumbnail_cache.buffer[ i ];

		// if ( thumbnail.status != e_thumbnail_status_uploading )
		// 	break;
		//
		// if ( ++g_thumbnail_cache.read_pos == MAX_THUMBNAILS )
		// 	g_thumbnail_cache.read_pos = 0;

		if ( thumbnail.status.load( std::memory_order_acquire ) != e_thumbnail_status_uploading )
			continue;

		// printf( "UPLOADING IMAGE: %s\n", thumbnail.path );

		if ( thumbnail.image->frame.empty() || !thumbnail.image->frame[ 0 ] )
		{
			printf( "thumbnail data is nullptr\n" );
		}

		thumbnail.texture    = gl_upload_texture( thumbnail.image );
		thumbnail.im_texture = thumbnail.texture;

		{
			if ( thumbnail.scaled )
				ch_free( e_mem_category_stbi_resize, thumbnail.image->frame[ 0 ] );
			else
				ch_free( e_mem_category_image_data, thumbnail.image->frame[ 0 ] );

			thumbnail.image->frame[ 0 ] = nullptr;
			thumbnail.image->frame.clear();

			image_free_alloc( *thumbnail.image );

			thumbnail_printf( "[THUMBNAIL %d] FREED IMAGE DATA %s\n", i, thumbnail.path );

			app::draw_frame = true;
		}

		if ( thumbnail.texture )
		{
			thumbnail.status = e_thumbnail_status_finished;
			//thumbnail_printf( "IMAGE FINISHED: %s\n", thumbnail.path );

			upload_count++;
		}
		else
			thumbnail.status = e_thumbnail_status_failed;
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
	g_thumbnail_mutex.lock();

	for ( u32 i = 0; i < MAX_THUMBNAILS; i++ )
	{
		g_thumbnail_cache.used_this_frame[ i ] = false;

		if ( g_thumbnail_cache.buffer[ i ].status == e_thumbnail_status_queued || g_thumbnail_cache.buffer[ i ].status == e_thumbnail_status_finished )
			g_thumbnail_cache.buffer[ i ].status = e_thumbnail_status_free;
	}

	g_thumbnail_queue.write_pos = 0;
	g_thumbnail_queue.read_pos  = 0;

	g_thumbnail_mutex.unlock();
}


void thumbnail_cache_debug_draw()
{
	ImGui::SeparatorText( "Thumbnail System" );
	ImGui::Text( "Thread Count: %d", app::config.thumbnail_threads );

	ImGui::Text( "Job Queue Write Pos: %d", g_thumbnail_queue.write_pos.load() );
	ImGui::Text( "Job Queue Read Pos: %d", g_thumbnail_queue.read_pos.load() );
}

