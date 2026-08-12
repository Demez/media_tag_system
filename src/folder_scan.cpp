#include "main.h"

#include <thread>
#include <mutex>
#include <atomic>


std::thread*                         g_folder_scan_thread = nullptr;
std::vector< folder_scan_status_t* > g_folder_scan_queue;
std::atomic< size_t >                g_folder_scan_queue_size;
std::mutex                           g_folder_scan_lock;

SDL_Event                            g_event_folder_scan_finish{};

typedef void( job_finish_t )( job_status_t* status, bool in_main_thread );
typedef void( job_function_t )( job_status_t* status );


void folder_scan_job_finish( job_status_t* status, bool in_main_thread )
{
	auto scan = static_cast< folder_scan_status_t* >( status->userdata );
	scan->callback( scan, in_main_thread );
}


void folder_scan_job_run( job_status_t* status )
{
	auto scan   = static_cast< folder_scan_status_t* >( status->userdata );

	bool result = sys_scandir( scan->root, scan->files, scan->flags, &status->cancel );

	if ( !result )
		printf( "Failed to scan directory: %s\n", scan->root );

	// should we mutex lock this by storing a mutex in the status of it?
	scan->result     = result;
	//status->finished = true;

	if ( !status->cancel && scan->thread_func )
	{
		// send draw event
		send_frame_draw_event();

		// call user function
		scan->thread_userdata = scan->thread_func( scan );
	}
}


void folder_scan_job_free( job_status_t* status )
{
	if ( !status )
		return;

	auto scan = static_cast< folder_scan_status_t* >( status->userdata );

	ch_free_str( scan->root );
	scan->files.clear();

	delete scan;
}


// non-blocking folder scanning
folder_scan_status_t* folder_scan_push( const char* root, e_scandir_flags flags, folder_scan_callback_t* callback, folder_scan_thread_func_t* thread_func )
{
	if ( !fs_is_dir( root ) )
		return nullptr;

	auto status = new folder_scan_status_t;

	if ( !status )
		return nullptr;

	status->flags       = flags;
	status->root        = util_strdup( root );
	status->callback    = callback;
	status->thread_func = thread_func;

	status->job         = job_push( folder_scan_job_finish, folder_scan_job_run, folder_scan_job_free, status );

	return status;
}

