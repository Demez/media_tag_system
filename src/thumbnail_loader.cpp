#include "main.h"

#include <thread>
#include <mutex>
#include <queue>
#include <atomic>

constexpr int         JOB_QUEUE_SIZE    = 32;
constexpr int         THUMBNAIL_THREADS = 1;
constexpr int         MAX_THUMBNAILS    = 256;

constexpr const char* FAILED_IMAGE_PATH = "super_missing_texture.jpg";
image_t*              FAILED_IMAGE      = nullptr;

std::atomic< bool >   g_thumbnails_running;
std::thread*          g_thumbnail_worker[ THUMBNAIL_THREADS ];

extern SDL_Renderer*  g_main_renderer;
extern ICodec* g_test_codec;


struct thumbnail_job_t
{
	char*       path;
	h_thumbnail thumbnail;
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


thumbnail_queue_t g_thumbnail_queue;
thumbnail_cache_t g_thumbnail_cache;
// handle_list_32< h_thumbnail, thumbnail_t, MAX_THUMBNAILS > g_thumbnail_cache;
// u32                                                        g_thumbnail_cache_index = 0;


void thumbnail_loader_free_data( u32 index )
{
	thumbnail_t& thumbnail = g_thumbnail_cache.buffer[ index ];

	printf( "FREED %d - %s\n", index, thumbnail.path );

	thumbnail.im_texture = nullptr;

	SDL_DestroyTexture( thumbnail.sdl_texture );
	SDL_DestroySurface( thumbnail.sdl_surface );

	free( thumbnail.data );
	free( thumbnail.path );

	memset( &thumbnail, 0, sizeof( thumbnail_t ) );
}


h_thumbnail thumbnail_loader_queue_push( const char* path )
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
		printf( "THUMBNAIL QUEUE FULL\n" );
		return {};
	}

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

		if ( g_thumbnail_cache.buffer[ cache_pos ].status == e_thumbnail_status_loading )
			continue;

		if ( g_thumbnail_cache.buffer[ cache_pos ].status == e_thumbnail_status_uploading )
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
		printf( "THUMBNAIL CACHE FULL\n" );
		return {};
	}

	if ( g_thumbnail_cache.buffer[ cache_pos ].sdl_texture )
	{
		thumbnail_loader_free_data( cache_pos );
	}

	h_thumbnail handle;
	handle.index         = cache_pos;
	handle.generation    = ++g_thumbnail_cache.generation[ cache_pos ];

	// the queue is not full, so create a new job for it
	thumbnail_job_t& job = g_thumbnail_queue.buffer[ current_pos ];

	if ( job.path )
		free( job.path );

	job.path                                       = strdup( path );
	job.thumbnail                                  = handle;
	g_thumbnail_cache.buffer[ cache_pos ].status   = e_thumbnail_status_queued;
	g_thumbnail_cache.buffer[ cache_pos ].path     = strdup( path );
	g_thumbnail_cache.used_this_frame[ cache_pos ] = true;

	// update the write position in the queue, use release to wait for all reads to finish before updating this
	g_thumbnail_queue.write_pos.store( next_pos, std::memory_order_release );

	return handle;
}


thumbnail_job_t* thumbnail_loader_queue_pop()
{
	// wait for reads/writes before loading it
	u32 current_pos = g_thumbnail_queue.read_pos.load( std::memory_order_acquire );

	// make sure we aren't at the write position, nothing new added yet then
	if ( current_pos == g_thumbnail_queue.write_pos.load( std::memory_order_acquire ) )
	{
		return nullptr;
	}

	thumbnail_job_t* job = &g_thumbnail_queue.buffer[ current_pos ];
	g_thumbnail_queue.read_pos.store( ( current_pos + 1 ) % JOB_QUEUE_SIZE, std::memory_order_release );

	return job;
}


void thumbnail_loader_worker()
{
	while ( g_thumbnails_running.load( std::memory_order_acquire ) )
	{
		thumbnail_job_t* job = thumbnail_loader_queue_pop();

		// no work for us to do right now
		if ( !job )
		{
			SDL_Delay( 250 );
			continue;
		}

		thumbnail_t* thumbnail = &g_thumbnail_cache.buffer[ job->thumbnail.index ];

		if ( !thumbnail )
			continue;

		thumbnail->status = e_thumbnail_status_loading;
		thumbnail->data   = ch_calloc< image_t >( 1 );

		if ( thumbnail->data == nullptr )
		{
			printf( "????\n" );
		}

		if ( !g_test_codec->image_load_scaled( job->path, thumbnail->data, 512, 512 ) )
		{
			printf( "FAILED TO LOAD IMAGE: %s\n", job->path );
			thumbnail->status = e_thumbnail_status_failed;
			continue;
		}

		if ( thumbnail->data == nullptr )
		{
			printf( "????\n" );
		}

		thumbnail->status = e_thumbnail_status_uploading;
		printf( "LOADED IMAGE: %s\n", job->path );
	}
}


bool thumbnail_loader_init()
{
	g_thumbnail_queue.write_pos = 0;
	g_thumbnail_queue.read_pos  = 0;

	// load fail image
	FAILED_IMAGE                = ch_calloc< image_t >( 1 );
	if ( !g_test_codec->image_load_scaled( FAILED_IMAGE_PATH, FAILED_IMAGE, 512, 512 ) )
	{
		return false;
	}

	g_thumbnails_running.store( true );

	for ( int i = 0; i < THUMBNAIL_THREADS; i++ )
	{
		g_thumbnail_worker[ i ] = new std::thread( thumbnail_loader_worker );
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
	for ( u32 i = 0; i < MAX_THUMBNAILS; i++ )
	{
		// reset all of these
		g_thumbnail_cache.used_this_frame[ i ] = false;

		if ( g_thumbnail_cache.generation[ i ] == 0 )
			continue;

		// thumbnail_t& thumbnail = g_thumbnail_cache.buffer[ g_thumbnail_cache.read_pos ];
		thumbnail_t& thumbnail = g_thumbnail_cache.buffer[ i ];

		// if ( thumbnail.status != e_thumbnail_status_uploading )
		// 	break;
		// 
		// if ( ++g_thumbnail_cache.read_pos == MAX_THUMBNAILS )
		// 	g_thumbnail_cache.read_pos = 0;

		if ( thumbnail.status != e_thumbnail_status_uploading )
			continue;

		thumbnail.sdl_surface = SDL_CreateSurfaceFrom( thumbnail.data->width, thumbnail.data->height, SDL_PIXELFORMAT_BGR24, thumbnail.data->data, thumbnail.data->pitch );
		thumbnail.sdl_texture = SDL_CreateTextureFromSurface( g_main_renderer, thumbnail.sdl_surface );
		thumbnail.im_texture  = thumbnail.sdl_texture;

		free( thumbnail.data->data );

		if ( thumbnail.sdl_texture )
			thumbnail.status = e_thumbnail_status_finished;
		else
			thumbnail.status = e_thumbnail_status_failed;
	}
}


void thumbnail_loader_update_after_render()
{
//	for ( u32 i = 0; i < MAX_THUMBNAILS; i++ )
//	{
//		// reset all of these
//		g_thumbnail_cache.used_this_frame[ i ] = false;
//	}
}


h_thumbnail thumbnail_queue_image( const fs::path& path )
{
	return thumbnail_loader_queue_push( path.string().c_str() );
}


thumbnail_t* thumbnail_get_data( h_thumbnail handle )
{
	if ( !handle_list_valid( MAX_THUMBNAILS, g_thumbnail_cache.generation, handle ) )
	{
		// printf( "Requesting Invalid Thumbnail!\n" );
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

