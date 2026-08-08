#include "main.h"

#include <thread>
#include <mutex>
#include <atomic>


std::thread*                         g_folder_scan_thread = nullptr;
std::vector< folder_scan_status_t* > g_folder_scan_queue;
std::atomic< size_t >                g_folder_scan_queue_size;
std::mutex                           g_folder_scan_lock;

SDL_Event                            g_event_folder_scan_finish{};


void folder_scan_thread()
{
	while ( app::running )
	{
		g_folder_scan_queue_size.wait( 0 );

		g_folder_scan_lock.lock();

		folder_scan_status_t* status = g_folder_scan_queue.back();
		g_folder_scan_queue.pop_back();
		g_folder_scan_queue_size.store( g_folder_scan_queue.size() );

		g_folder_scan_lock.unlock();

		if ( !status )
			continue;

		bool result = sys_scandir( status->root, status->files, status->flags, &status->cancel );

		if ( !result )
			printf( "Failed to scan directory: %s\n", status->root );

		// should we mutex lock this by storing a mutex in the status of it?
		status->result   = result;
		status->finished = true;

		if ( !status->cancel && status->thread_func )
		{
			// send draw event
			send_frame_draw_event();

			// call user function
			status->thread_userdata = status->thread_func( status );
		}

		// base event
		SDL_Event event  = g_event_folder_scan_finish;
		event.user.data1 = status;

		SDL_PushEvent( &event );
	}
}


// non-blocking folder scanning
folder_scan_status_t* folder_scan_push( const char* root, e_scandir_flags flags, folder_scan_callback_t* callback, folder_scan_thread_func_t* thread_func )
{
	if ( !fs_is_dir( root ) )
		return nullptr;

	// auto status = ch_new< folder_scan_status_t >( e_mem_category_thread_data );
	auto status = new folder_scan_status_t;

	if ( !status )
		return nullptr;

	status->flags       = flags;
	status->root        = util_strdup( root );
	status->callback    = callback;
	status->thread_func = thread_func;

	g_folder_scan_lock.lock();

	g_folder_scan_queue.push_back( status );
	g_folder_scan_queue_size.store( g_folder_scan_queue.size() );
	g_folder_scan_queue_size.notify_one();

	g_folder_scan_lock.unlock();

	return status;
}


void folder_scan_free( folder_scan_status_t* status )
{
	if ( !status )
		return;

	ch_free_str( status->root );
	status->files.clear();

	//ch_free( e_mem_category_thread_data, status );
	delete status;
}


bool folder_scan_init()
{
	g_folder_scan_thread = new std::thread( folder_scan_thread );

	if ( !g_folder_scan_thread )
		return false;

	g_event_folder_scan_finish.type      = SDL_EVENT_USER;
	g_event_folder_scan_finish.user.code = SDL_RegisterEvents( 1 );

	return true;
}


void folder_scan_shutdown()
{
	g_folder_scan_queue_size.store( 0 );
	g_folder_scan_queue_size.notify_all();

	if ( g_folder_scan_thread )
		g_folder_scan_thread->join();

	//ch_free( e_mem_category_thread_data, g_folder_scan_thread );
	delete g_folder_scan_thread;
	g_folder_scan_thread = nullptr;
}

