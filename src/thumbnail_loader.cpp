#include "main.h"

#include "stb_image_resize2.h"

#include <thread>
#include <mutex>
#include <queue>
#include <unordered_map>

constexpr int       JOB_QUEUE_SIZE    = 64;
constexpr int       THUMBNAIL_THREADS = 8;
constexpr int       MAX_THUMBNAILS    = 256;

std::atomic< bool > g_thumbnails_running;
std::thread*        g_thumbnail_worker[ THUMBNAIL_THREADS ];
std::mutex          g_thumbnail_mutex;

extern int          g_gallery_image_size;
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
	char*       path;
	h_thumbnail thumbnail;
	e_job_state state;
};


// ring buffer attempt
struct thumbnail_queue_t
{
	thumbnail_job_t    buffer[ JOB_QUEUE_SIZE ];
	std::atomic< u32 > write_pos;  // write position in buffer for adding new jobs from main thread
	std::atomic< u32 > read_pos;   // where to read the next job from a worker thread
};


// ring buffer for thumbnails
struct thumbnail_cache_t
{
	thumbnail_t buffer[ MAX_THUMBNAILS ];
	// u32         index[ MAX_THUMBNAILS ];      // index in buffer array
	u32         generation[ MAX_THUMBNAILS ];
	bool        used_this_frame[ MAX_THUMBNAILS ];
	// u32         write_pos;
};


// list of thumbnails to try to load
struct thumbnail_list_value_t
{
	u32 distance;         // determines when the thumbnail is loaded distances get loaded first for other thumbnails
	u32 thumbnail_index;  // index into thumbnail_cache_t::buffer
};


// std::vector< thumbnail_list_entry_t > g_thumbnail_list;
// value is distance, lower values determine whent he thumbnail is loaded
std::unordered_map< fs::path, thumbnail_list_value_t > g_thumbnail_list;
thumbnail_queue_t                   g_thumbnail_queue;
thumbnail_cache_t                   g_thumbnail_cache;

// handle_list_32< h_thumbnail, thumbnail_t, MAX_THUMBNAILS > g_thumbnail_cache;
// u32                                                        g_thumbnail_cache_index = 0;


// debug printing
void thumbnail_printf( const char* format, ... )
{
	if ( !THUMBNAIL_DEBUG_PRINT )
		return;

	va_list args;
	va_start( args, format );
	vprintf_s( format, args );
	va_end( args );
}


void thumbnail_loader_free_data( u32 index )
{
	thumbnail_t& thumbnail = g_thumbnail_cache.buffer[ index ];

	thumbnail_printf( "FREED %d - %s\n", index, thumbnail.path );

	thumbnail.im_texture = nullptr;

	gl_free_texture( thumbnail.texture );

	free( thumbnail.image );
	free( thumbnail.path );

	memset( &thumbnail, 0, sizeof( thumbnail_t ) );
}


h_thumbnail thumbnail_loader_queue_push( const char* path, e_media_type type )
{
	if ( !path )
		return {};

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

	// next job isn't free?
	// if ( job.state != e_job_state_free )
	// 	return {};

	// u32 cache_pos               = g_thumbnail_cache.write_pos;
	// g_thumbnail_cache.write_pos = ( cache_pos + 1 ) % MAX_THUMBNAILS;

	// if ( ++g_thumbnail_cache.write_pos == MAX_THUMBNAILS )
	// 	g_thumbnail_cache.write_pos = 0;
	
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

		// if ( g_thumbnail_cache.buffer[ cache_pos ].status != e_thumbnail_status_free &&
		//      g_thumbnail_cache.buffer[ cache_pos ].status != e_thumbnail_status_finished &&
		//      g_thumbnail_cache.buffer[ cache_pos ].status != e_thumbnail_status_failed )
		// 	break;

		e_thumbnail_status status = g_thumbnail_cache.buffer[ cache_pos ].status.load( std::memory_order_acquire );
		
		if ( status == e_thumbnail_status_queued || status == e_thumbnail_status_loading || status == e_thumbnail_status_uploading )
			continue;

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

	if ( g_thumbnail_cache.buffer[ cache_pos ].texture )
	{
		thumbnail_loader_free_data( cache_pos );
	}

	thumbnail_printf( "THUMBNAIL %d USED\n", cache_pos );

	h_thumbnail handle;
	handle.index         = cache_pos;
	handle.generation    = ++g_thumbnail_cache.generation[ cache_pos ];

	if ( job.path )
		free( job.path );

	job.path                                       = strdup( path );
	job.thumbnail                                  = handle;
	job.state                                      = e_job_state_working;

	g_thumbnail_cache.buffer[ cache_pos ].status   = e_thumbnail_status_queued;
	g_thumbnail_cache.buffer[ cache_pos ].path     = strdup( path );
	g_thumbnail_cache.buffer[ cache_pos ].type     = type;
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


void thumbnail_loader_worker( u32 thread_id )
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
			// Disable Video - So no window pops up when playing back a video
			p_mpv_set_option_string( local_mpv, "vo", "null" );
			
			// Disable Audio
			p_mpv_set_option_string( local_mpv, "ao", "null" );

			if ( p_mpv_initialize( local_mpv ) < 0 )
			{
				printf( "mpv_initialize failed!\n" );
				p_mpv_destroy( local_mpv );
				local_mpv = nullptr;
			}
			else
			{
				// p_mpv_request_log_messages( local_mpv, "debug" );
				p_mpv_request_log_messages( local_mpv, "warn" );
			}
		}
	}

	char* app_path = sys_get_exe_folder();

	char video_thumbnail_path[ 512 ];
	snprintf( video_thumbnail_path, 512, "%s/video_thumbnail_thread_%d.png", app_path, thread_id );

	free( app_path );

	char mpv_thread_name[ 64 ];
	snprintf( mpv_thread_name, 64, "MPV THREAD %d", thread_id );

	// let mpv startup
	mpv_handle_wait_event( local_mpv, 0.1, mpv_thread_name );

	// Enter Loop
	while ( g_thumbnails_running.load( std::memory_order_acquire ) )
	{
		u32              job_id = 0;
		thumbnail_job_t* job = thumbnail_loader_queue_pop( job_id );

		if ( !job )
		{
			SDL_Delay( 250 );
			continue;
		}

		thumbnail_t* thumbnail = &g_thumbnail_cache.buffer[ job->thumbnail.index ];

		if ( !thumbnail )
			continue;

		thumbnail_printf( "[THUMBNAIL %d][JOB %d][THREAD %d] STARTING LOAD OF IMAGE: %s\n", job->thumbnail.index, job_id, thread_id, job->path );

		thumbnail->status.store( e_thumbnail_status_loading, std::memory_order_release );

		image_load_info_t load_info{};

		if ( thumbnail->type == e_media_type_video )
		{
			// Use the local mpv instance to capture a frame from the video
			if ( !local_mpv )
			{
				thumbnail->status = e_thumbnail_status_failed;
				continue;
			}

			bool        failed       = false;

			// mpv_handle_wait_event( local_mpv, 0.1, mpv_thread_name );

			const char* cmd[]        = { "loadfile", job->path, NULL };
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
				// this stage is loaded enough for seek to happen, a little quicker that playback restart
				else if ( event->event_id == MPV_EVENT_FILE_LOADED )
				{
					break;
				}
				else if ( event->event_id == MPV_EVENT_PLAYBACK_RESTART )
				{
					//break;
				}

				event = p_mpv_wait_event( local_mpv, -1 );
			}

			if ( failed )
			{
				thumbnail->status = e_thumbnail_status_failed;
				continue;
			}

			s64         percent_pos  = 30;
			//cmd_ret                  = p_mpv_set_property( local_mpv, "percent-pos", MPV_FORMAT_INT64, &percent_pos );
			cmd_ret                  = p_mpv_set_property_async( local_mpv, NULL, "percent-pos", MPV_FORMAT_INT64, &percent_pos );

			event                    = p_mpv_wait_event( local_mpv, -1 );
			
			while ( event->event_id != MPV_EVENT_NONE )
			{
				if ( event->event_id == MPV_EVENT_LOG_MESSAGE )
				{
					struct mpv_event_log_message* msg = (struct mpv_event_log_message*)event->data;
					printf( "%s: [%s] %s: %s", mpv_thread_name, msg->prefix, msg->level, msg->text );
				}
				else if ( event->event_id == MPV_EVENT_SET_PROPERTY_REPLY )
				{
					if ( event->error != 0 )
					{
						printf( "failed to seek into video for thumbnail - %d\n", event->error );
						failed = true;
						break;
					}
				}
				// wait for the seek to finish
				else if ( event->event_id == MPV_EVENT_PLAYBACK_RESTART )
				{
					if ( event->error != 0 )
					{
						printf( "failed to seek into video for thumbnail - %d\n", event->error );
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
			cmd_ret                  = p_mpv_command_async( local_mpv, NULL, cmd_clear );

			// Load Image Normally
			thumbnail->image         = ch_calloc< image_t >( 1 );

			load_info.image          = thumbnail->image;
			load_info.load_quick     = true;
			load_info.threaded_load  = true;
			load_info.thumbnail_load = true;
			load_info.target_size.x  = g_gallery_image_size;
			load_info.target_size.y  = g_gallery_image_size;

			if ( !image_load( video_thumbnail_path, load_info ) )
			{
				printf( "FAILED TO LOAD IMAGE: %s\n", video_thumbnail_path );
				thumbnail->status = e_thumbnail_status_failed;
				continue;
			}
			
			// TODO: Delete Image? it might just slow this down a bit, since it always just gets overwritten later
		}
		else
		{
			// Load Image Normally
			thumbnail->image   = ch_calloc< image_t >( 1 );

			load_info.image          = thumbnail->image;
			load_info.load_quick     = true;
			load_info.threaded_load  = true;
			load_info.thumbnail_load = true;
			load_info.target_size.x  = g_gallery_image_size;
			load_info.target_size.y  = g_gallery_image_size;

			if ( !image_load( job->path, load_info ) )
			{
				printf( "FAILED TO LOAD IMAGE: %s\n", job->path );
				thumbnail->status = e_thumbnail_status_failed;
				continue;
			}
		}

		if ( thumbnail->image->frame.empty() || !thumbnail->image->frame[ 0 ] )
		{
			printf( "data is nullptr in worker?\n" );
			thumbnail->status = e_thumbnail_status_failed;
			continue;
		}

		thumbnail_printf( "[THUMBNAIL %d] LOADED IMAGE: %s\n", job->thumbnail.index, job->path );
		
		float min_size = std::min( thumbnail->image->width, thumbnail->image->height );

		// Downscale image if size is larger than target size
		if ( min_size > load_info.target_size.x )
		{
			float factor[ 2 ]      = { 1.f, 1.f };

			factor[ 0 ]            = (float)load_info.target_size.x / (float)thumbnail->image->width;
			factor[ 1 ]            = (float)load_info.target_size.y / (float)thumbnail->image->height;

			float downscale_amount = std::min( factor[ 0 ], factor[ 1 ] );
			// float downscale_amount       = 0.5f;

			float new_width        = thumbnail->image->width * downscale_amount;
			float new_height       = thumbnail->image->height * downscale_amount;

			u8*   old_frame        = thumbnail->image->frame[ 0 ];

			image_downscale( thumbnail->image, thumbnail->image, new_width, new_height );

			free( old_frame );
		}

		job->state = e_job_state_free;

		// hopefully fixes race condition with writes
		// std::atomic_thread_fence( std::memory_order_seq_cst );

		thumbnail->status.store( e_thumbnail_status_uploading, std::memory_order_release );
	}
}


bool thumbnail_loader_init()
{
	g_thumbnail_queue.write_pos = 0;
	g_thumbnail_queue.read_pos  = 0;

	// load fail image
//	FAILED_IMAGE                = ch_calloc< image_t >( 1 );
//	if ( !g_test_codec->image_load_scaled( FAILED_IMAGE_PATH, FAILED_IMAGE, 512, 512 ) )
//	{
//		return false;
//	}

	g_thumbnails_running.store( true );

	for ( int i = 0; i < THUMBNAIL_THREADS; i++ )
	{
		g_thumbnail_worker[ i ] = new std::thread( thumbnail_loader_worker, i );
	}

	return true;
}


void thumbnail_loader_shutdown()
{
}


void thumbnail_loader_free( h_thumbnail handle )
{
}


void thumbnail_loader_free_oldest( h_thumbnail handle )
{
}


void thumbnail_loader_update()
{
	// int max = g_thumbnail_cache.write_pos;
	// 
	// if ( max <= g_thumbnail_cache.read_pos )
	// 	max += ( MAX_THUMBNAILS - g_thumbnail_cache.read_pos );

	// for ( u32 i = 0; i < max; i++ )
	u32 upload_count = 0;
	u32 upload_max   = 4;
	for ( u32 i = 0; i < MAX_THUMBNAILS; i++ )
	{
		if ( upload_count == upload_max )
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
			free( thumbnail.image->frame[ 0 ] );
			thumbnail.image->frame[ 0 ] = nullptr;
			thumbnail_printf( "[THUMBNAIL %d] FREED IMAGE DATA %s\n", i, thumbnail.path );
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


h_thumbnail thumbnail_queue_image( const fs::path& path, e_media_type type )
{
	return thumbnail_loader_queue_push( path.string().c_str(), type );
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


void thumbnail_free( const fs::path& path, u32 index )
{
}


void thumbnail_cache_debug_draw()
{
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

