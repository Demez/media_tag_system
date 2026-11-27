#include "main.h"

#include "stb_image_resize2.h"

#include <thread>
#include <mutex>
#include <forward_list>

constexpr int            MAX_THUMBNAILS = 512;

std::atomic< bool >      g_thumbnails_running;
std::thread**            g_thumbnail_worker;
std::thread**            g_thumbnail_save_worker;

// thumbnail save threads sleep, and wait on this signal before doing any work
std::atomic< bool >      g_thumbnail_save_wait = false;

extern SDL_SharedObject* g_mpv_module;

constexpr bool           THUMBNAIL_DEBUG_PRINT = false;


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

extern bool              thumbnail_save( image_t& image, const fs::path_str& output );

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

	image_free( thumbnail.image );
	image_free( thumbnail.image_scaled );

	ch_free_str( thumbnail.path );

	memset( &thumbnail, 0, sizeof( thumbnail_t ) );

	thumbnail_printf( "[THUMBNAIL %d] FREED IMAGE DATA %s\n", index, thumbnail.path );
}


h_thumbnail thumbnail_loader_queue_push( const media_entry_t& media_entry )
{
	if ( media_entry.file.name.empty() )
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

	fs::path_str path_str = directory::path;
	path_str += SEP;
	path_str += media_entry.file.path;

	g_thumbnail_cache.buffer[ cache_pos ].path        = util_strxndup( path_str.data(), path_str.size() );
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


fs::path_str build_cache_path( size_t file_hash )
{
	fs::path_str thumbnail_path = app::config.thumbnail_cache_path;
	thumbnail_path += SEP;

#if WIN32
	thumbnail_path += std::to_wstring( file_hash );
	thumbnail_path += L".jxl";
#else
	thumbnail_path += std::to_string( file_hash );
	thumbnail_path += ".jxl";
#endif

	return thumbnail_path;
}


void thumbnail_save_worker( int thread )
{
	while ( g_thumbnails_running.load( std::memory_order_acquire ) )
	{
		if ( g_thumbnail_save.queue.empty() )
		{
			g_thumbnail_save.mutex.lock();
			g_thumbnail_save_wait.store( false );
			g_thumbnail_save.mutex.unlock();
			g_thumbnail_save_wait.wait( false );
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

		fs::path_str thumbnail_path = build_cache_path( entry.file_hash );

		thumbnail_save( *thumbnail->image, thumbnail_path );

		//printf( "[THUMBNAIL %d] SAVED %s\n", entry.thumbnail.index, thumbnail->path );
		thumbnail->save_status = e_thumbnail_save_finished;

		// this is only useful when in stats view lol
		send_frame_draw_event();
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

	// wake up one thread
	g_thumbnail_save_wait.store( true );
	g_thumbnail_save_wait.notify_one();
}


bool thumbnail_loader_get_video_thumbnail_from_disk( thumbnail_t* thumbnail, int thread_id );


bool thumbnail_loader_load_source_from_disk( thumbnail_t* thumbnail, int thread_id )
{
	// No thumbnail was found on disk, load the source file and generate one
	switch ( thumbnail->type )
	{
		default:
			return false;

		case e_media_type_video:
		case e_media_type_audio:  // look for an embedded image, like album art
			return thumbnail_loader_get_video_thumbnail_from_disk( thumbnail, thread_id );
			
		case e_media_type_image:
		{
			// Load Image Normally
			thumbnail->image = image_create();

			image_load_info_t load_info{};
			load_info.image          = thumbnail->image;
			load_info.load_quick     = true;
			load_info.threaded_load  = true;
			load_info.thumbnail_load = true;
			load_info.single_frame   = true;
			load_info.target_size.x  = static_cast< float >( app::config.thumbnail_size );
			load_info.target_size.y  = static_cast< float >( app::config.thumbnail_size );

			if ( !image_load( thumbnail->path, load_info ) )
			{
				path_printf( "FAILED TO LOAD IMAGE: %s\n", thumbnail->path );
				thumbnail->status = e_thumbnail_status_failed;
				return false;
			}

			return true;
		}
	}

	// TODO: check thumbnail->loaded_type in case we have a correction, like a video file is actually just audio
	// but try to load it with the inverse method instead, in case of incorrect file extensions
	// if it loads, we need to update the type in media_entry_t properly, we can do that on the main thread later
	// note that it could change as well if the user tries to load it before or during thumbnail generation for the file
}


void thumbnail_loader_worker( u32 thread_id )
{
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

		thumbnail_t* thumbnail = &g_thumbnail_cache.buffer[ thread_data.thumbnail.index ];

		if ( !thumbnail )
			continue;

		if ( thumbnail->status == e_thumbnail_status_failed )
			continue;

		thumbnail->status.store( e_thumbnail_status_loading );

		thumbnail_printf( "[THUMBNAIL %d][THREAD %d] STARTING LOAD OF IMAGE: %s\n", thread_data.thumbnail.index, thread_id, thumbnail->path );

		size_t file_hash      = thumbnail_generate_hash( thread_data.file );
		u32    thumbnail_size = static_cast< u32 >( gallery::image_bounds.x );

		if ( app::config.thumbnail_use_fixed_size )
			thumbnail_size = app::config.thumbnail_size;

		bool              thumbnail_found_on_disk = false;
		image_load_info_t jxl_thumbnail{};

		if ( app::config.thumbnail_jxl_enable )
		{
			fs::path_str thumbnail_path = app::config.thumbnail_cache_path;
			thumbnail_path += SEP;

#if WIN32
			thumbnail_path += std::to_wstring( file_hash );
			thumbnail_path += L".jxl";
#else
			thumbnail_path += std::to_string( file_hash );
			thumbnail_path += ".jxl";
#endif

			if ( fs_is_file( thumbnail_path.c_str() ) )
			{
				// Load Image Normally
				jxl_thumbnail.image          = image_create();

				jxl_thumbnail.target_size.x  = static_cast< float >( thumbnail_size );
				jxl_thumbnail.target_size.y  = static_cast< float >( thumbnail_size );
				jxl_thumbnail.load_quick     = true;
				jxl_thumbnail.threaded_load  = true;
				jxl_thumbnail.thumbnail_load = true;
				jxl_thumbnail.quiet          = true;

				if ( image_load( thumbnail_path.c_str(), jxl_thumbnail ) )
				{
					thumbnail_found_on_disk = true;

					if ( thumbnail->image )
						image_free( thumbnail->image );

					thumbnail->image        = jxl_thumbnail.image;
					jxl_thumbnail.image     = nullptr;

					//if ( args_register_bool( "spew jxl loads", "--jxl-spew" ) )
					//	printf( "JXL THUMBNAIL LOADED: %s - %zu\n", thumbnail->path, file_hash );
				}
				else
				{
					image_free( jxl_thumbnail.image );
				}
			}
		}

		// ---------------------------------------------------------------------------------------------------------

		// if ( ( app::config.thumbnail_jxl_enable && !thumbnail_found_on_disk ) || ( !app::config.thumbnail_jxl_enable ) )
		if ( !thumbnail_found_on_disk )
		{
			// No thumbnail was found on disk, load the source file and generate one
			if ( !thumbnail_loader_load_source_from_disk( thumbnail, thread_id ) )
			{
				thumbnail->status = e_thumbnail_status_failed;
				send_frame_draw_event();
				continue;
			}
		}

		// ---------------------------------------------------------------------------------------------------------

		if ( !thumbnail->image )
		{
			printf( "THUMBNAIL IMAGE IS NULLPTR ?????\n" );
			thumbnail->status = e_thumbnail_status_failed;
			send_frame_draw_event();
			continue;
		}

		if ( thumbnail->image->frame.empty() || !thumbnail->image->frame[ 0 ].data )
		{
			printf( "data is nullptr in worker?\n" );
			thumbnail->status = e_thumbnail_status_failed;
			send_frame_draw_event();
			continue;
		}

		thumbnail_printf( "[THUMBNAIL %d] LOADED IMAGE: %s\n", thread_data.thumbnail.index, thumbnail->path );

		// ---------------------------------------------------------------------------------------------------------
		// Check if this loader wants us to save thumbnails for this image,
		// VTF's for example seem to be faster than loading a JPEG XL file, 
		// probably because of less effort decompressing, or no decompression at all
		// only probably with this idea is if the image is on slow storage, like a slow NAS server or SD card
		// but figuring out a way to handle that? i have no clue, i'll leave it enabled for certain loaders for now

		bool should_save_thumbnail = app::config.thumbnail_jxl_enable && !thumbnail_found_on_disk;

		if ( thumbnail->image->flags & e_image_flag_no_thumbnail_cache )
			should_save_thumbnail = false;
		
		// ---------------------------------------------------------------------------------------------------------
		// If we didn't find the thumbnail on disk, write it!

		bool  saving_thumbnail = false;

		if ( should_save_thumbnail )
		{
			// Make sure it's not in the cache folder
			fs::path     cleaned_path     = fs_path_clean( thumbnail->path );
			fs::path_str cleaned_path_str = cleaned_path.native();

			if ( !cleaned_path_str.starts_with( app::config.thumbnail_cache_path ) )
			{
				u64 max_image_size = static_cast< u64 >( std::max( thumbnail->image->width, thumbnail->image->height ) );

				// prescale the image instead, that way the save thread doesn't need to hold onto images nearly as long
				if ( max_image_size > static_cast< u64 >( app::config.thumbnail_size ) )
				{
					float factor[ 2 ]  = { 1.f, 1.f };

					factor[ 0 ]        = (float)app::config.thumbnail_size / (float)thumbnail->image->width;
					factor[ 1 ]        = (float)app::config.thumbnail_size / (float)thumbnail->image->height;

					float   scale      = std::min( factor[ 0 ], factor[ 1 ] );

					float   new_width  = thumbnail->image->width * scale;
					float   new_height = thumbnail->image->height * scale;

					u8*     old_frame  = thumbnail->image->frame[ 0 ].data;

					if ( image_scale( thumbnail->image, thumbnail->image, new_width, new_height ) )
					{
						ch_free( e_mem_category_image_data, old_frame );
						
						//printf( "SAVING JXL THUMBNAIL FOR %s\n", thumbnail->path );
						thumbnail->save_status = e_thumbnail_save_queued;
						thumbnail_save_push( thread_data.thumbnail, thumbnail, file_hash );
					}
					else
					{
						printf( "Failed to downscale image!\n" );
					}
				}
				else
				{
					//printf( "SAVING JXL THUMBNAIL FOR %s\n", thumbnail->path );
					thumbnail->save_status = e_thumbnail_save_queued;
					thumbnail_save_push( thread_data.thumbnail, thumbnail, file_hash );
				}
			}
		}

		// ---------------------------------------------------------------------------------------------------------
		// Scale image to give a nicer thumbnail preview

		float factor[ 2 ] = {
			(float)thumbnail_size / (float)thumbnail->image->width,
			(float)thumbnail_size / (float)thumbnail->image->height
		};

		float scale_amount = std::min( factor[ 0 ], factor[ 1 ] );

		if ( scale_amount != 0.f && scale_amount < 2.f && scale_amount != 1.f )
		{
			int new_width           = static_cast< int >( round( thumbnail->image->width * scale_amount ) );
			int new_height          = static_cast< int >( round( thumbnail->image->height * scale_amount ) );

			u8* old_frame           = thumbnail->image->frame[ 0 ].data;

			thumbnail->image_scaled = image_create();

			if ( image_scale( thumbnail->image, thumbnail->image_scaled, new_width, new_height ) )
			{
				// thumbnail->scaled = true;
				// ch_free( e_mem_category_image_data, old_frame );
			}
		}

		send_frame_draw_event();
		thumbnail->status.store( e_thumbnail_status_uploading, std::memory_order_release );
	}
}


bool thumbnail_libav_init();
void thumbnail_libav_shutdown();


bool thumbnail_loader_init()
{
	g_thumbnails_running.store( true );

	if ( !thumbnail_libav_init() )
		return false;

	g_thumbnail_worker      = ch_calloc< std::thread* >( app::config.thumbnail_threads, e_mem_category_general );
	g_thumbnail_save_worker = ch_calloc< std::thread* >( app::config.thumbnail_save_threads, e_mem_category_general );
	g_thumbnail_thread_data = new thumbnail_thread_data_t[ app::config.thumbnail_threads ];

	for ( u32 i = 0; i < app::config.thumbnail_save_threads; i++ )
	{
		g_thumbnail_save_worker[ i ] = new std::thread( thumbnail_save_worker, i );
	}

	for ( u32 i = 0; i < app::config.thumbnail_threads; i++ )
	{
		g_thumbnail_worker[ i ] = new std::thread( thumbnail_loader_worker, i );
	}

	return true;
}


void thumbnail_loader_shutdown( bool free_thumbnails )
{
	if ( !g_thumbnails_running )
		return;

	thumbnail_libav_shutdown();

	g_thumbnails_running.store( false );

	// wait for threads to shutdown
	for ( u32 i = 0; i < app::config.thumbnail_save_threads; i++ )
	{
		g_thumbnail_save_wait.store( true );
		g_thumbnail_save_wait.notify_all();
		g_thumbnail_save_worker[ i ]->join();
		delete g_thumbnail_save_worker[ i ];
	}

	for ( u32 i = 0; i < app::config.thumbnail_threads; i++ )
	{
		g_thumbnail_thread_data[ i ].state = e_thumbnail_thread_exit;
		g_thumbnail_thread_data[ i ].state.notify_all();
		g_thumbnail_worker[ i ]->join();
		delete g_thumbnail_worker[ i ];
	}

	ch_free( e_mem_category_general, g_thumbnail_save_worker );
	ch_free( e_mem_category_general, g_thumbnail_worker );

	delete[] g_thumbnail_thread_data;

	g_thumbnail_thread_data = nullptr;
	g_thumbnail_save_worker = nullptr;
	g_thumbnail_worker      = nullptr;

	if ( !free_thumbnails )
		return;

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
				// TODO: check if the thumbnail size changed while being generated, and invalidate it
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

