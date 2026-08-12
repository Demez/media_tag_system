#include "main.h"

#include <thread>
#include <mutex>
#include <atomic>
#include <unordered_set>


struct job_worker_data_t
{
};


static job_worker_data_t*                  g_job_worker_data = nullptr;
static std::thread**                       g_job_threads     = nullptr;

static std::vector< job_status_t* >        g_job_queue;
static std::atomic< size_t >               g_job_queue_size;
static std::mutex                          g_job_lock;

static std::unordered_set< job_status_t* > g_job_free_queue;

SDL_Event                                  g_event_job_finish{};


bool job_check_cancel_and_free( job_status_t* status )
{
	if ( !status->cancel )
		return false;

	// is this job freed?
	auto it = g_job_free_queue.find( status );

	if ( it != g_job_free_queue.end() )
	{
		// erase it and free from the queue
		g_job_free_queue.erase( it );
		job_free( status );
	}

	return true;
}


void job_worker( u32 thread )
{
	while ( app::running )
	{
		g_job_queue_size.wait( 0 );

		if ( !app::running )
			return;

		g_job_lock.lock();

		job_status_t* status = g_job_queue.back();
		g_job_queue.pop_back();
		g_job_queue_size.store( g_job_queue.size() );

		g_job_lock.unlock();

		if ( !status )
			continue;

		if ( job_check_cancel_and_free( status ) )
			continue;

		if ( !status->function )
			continue;
		
		status->function( status );

		//if ( job_check_cancel_and_free( status ) )
		//	continue;

		// base event
		SDL_Event event  = g_event_job_finish;
		event.user.data1 = status;

		SDL_PushEvent( &event );

		status->finished = true;
	}
}


bool job_init()
{
	// TODO: maybe use a "thread pool" kind of system?
	// that way you can have dedicated threads for certain tasks?

	g_job_threads     = ch_calloc< std::thread* >( app::config.job_threads, e_mem_category_general );
	g_job_worker_data = new job_worker_data_t[ app::config.job_threads ];

	for ( u32 i = 0; i < app::config.job_threads; i++ )
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
	g_job_queue_size.store( 64 );
	g_job_lock.unlock();

	for ( u32 i = 0; i < app::config.job_threads; i++ )
	{
		g_job_queue_size.notify_all();
		g_job_threads[ i ]->join();
		delete g_job_threads[ i ];
	}

	if ( g_job_worker_data )
		delete[] g_job_worker_data;
}


job_status_t* job_push( job_finish_t* finish_callback, job_function_t* function, job_function_t* free_func, void* userdata )
{
	// auto status = ch_new< folder_scan_status_t >( e_mem_category_thread_data );
	auto status = new job_status_t;

	if ( !status )
		return nullptr;

	status->callback = finish_callback;
	status->function = function;
	status->userdata = userdata;
	status->free     = free_func;

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
	if ( !status )
		return;

	if ( status->free )
		status->free( status );
	
	status->userdata = nullptr;
	delete status;
}


// cancel a job and free it later
// NOTE: i think this is flawed and can memory leak in race conditions here
// finished may be get updated in the thread right as it passes in the check to it i assume
// TODO: make job_status_t ref counted and just free it like that, way easier management
void job_cancel_and_free( job_status_t* status )
{
	if ( !status )
		return;

	// if it's already finished, free it now
	if ( status->finished )
	{
		// free later in event loop, easier to manage frees
		// job_free( status );
		return;
	}

	// free it in the thread when we can
	status->cancel = true;
	g_job_free_queue.insert( status );
}

