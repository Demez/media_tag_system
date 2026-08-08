#include "main.h"

#include <thread>
#include <mutex>
#include <atomic>


struct job_worker_data_t
{
};


static job_worker_data_t*           g_job_worker_data = nullptr;
static std::thread**                g_job_threads     = nullptr;

static std::vector< job_status_t* > g_job_queue;
static std::atomic< size_t >        g_job_queue_size;
static std::mutex                   g_job_lock;

SDL_Event                           g_event_job_finish{};


void job_worker( u32 thread )
{
	while ( app::running )
	{
		g_job_queue_size.wait( 0 );

		g_job_lock.lock();

		job_status_t* status = g_job_queue.back();
		g_job_queue.pop_back();
		g_job_queue_size.store( g_job_queue.size() );

		g_job_lock.unlock();

		if ( !status )
			continue;

		if ( status->cancel )
			continue;

		if ( !status->function )
			continue;
		
		status->function( status );
		status->finished = true;

		// base event
		SDL_Event event  = g_event_job_finish;
		event.user.data1 = status;

		SDL_PushEvent( &event );
	}
}


bool job_init()
{
	g_job_threads     = ch_calloc< std::thread* >( app::config.thumbnail_save_threads, e_mem_category_general );
	g_job_worker_data = new job_worker_data_t[ app::config.thumbnail_save_threads ];

	for ( u32 i = 0; i < app::config.thumbnail_save_threads; i++ )
	{
		g_job_threads[ i ] = new std::thread( job_worker, i );
	}

	g_event_job_finish.type      = SDL_EVENT_USER;
	g_event_job_finish.user.code = SDL_RegisterEvents( 1 );

	return true;
}


void job_shutdown()
{
	g_job_lock.lock();
	g_job_queue_size.store( 0 );
	g_job_lock.unlock();

	for ( u32 i = 0; i < app::config.thumbnail_save_threads; i++ )
	{
		g_job_queue_size.notify_all();
		g_job_threads[ i ]->join();
		delete g_job_threads[ i ];
	}

	if ( g_job_worker_data )
		delete[] g_job_worker_data;
}


job_status_t* job_push( job_function_t* callback, job_function_t* function, void* userdata )
{
	// auto status = ch_new< folder_scan_status_t >( e_mem_category_thread_data );
	auto status = new job_status_t;

	if ( !status )
		return nullptr;

	status->callback = callback;
	status->function = function;
	status->userdata = userdata;

	g_job_lock.lock();

	g_job_queue.push_back( status );
	g_job_queue_size.store( g_job_queue.size() );
	g_job_queue_size.notify_one();

	g_job_lock.unlock();

	return status;
}


// call this when finished doing work
void job_free( job_status_t* status )
{
	if ( status )
		delete status;
}

