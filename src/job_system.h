#pragma once
#include "core.h"


// -------------------------------------------------------------------------------------------
// Job System


struct job_status_t;


typedef void( job_finish_t )( job_status_t* status, bool in_main_thread );
typedef void( job_function_t )( job_status_t* status );


// For running the scan directory in a background thread
struct job_status_t
{
	// function to call when job is finished on the main thread
	job_finish_t*   callback = nullptr;

	// function to call internally
	job_function_t* function = nullptr;

	// function to call for freeing userdata
	job_function_t* free     = nullptr;

	// store information you need here
	void*           userdata = nullptr;

	// set to true to cancel the job
	bool            cancel   = false;

	// check to see if it finished
	bool            finished = false;
};


extern SDL_Event g_event_job_finish;


job_status_t*    job_push( job_finish_t* finish_callback, job_function_t* function, job_function_t* free_func, void* userdata );

// call this when finished doing work
void             job_free( job_status_t* status );

// cancel a job and free it later
void             job_cancel_and_free( job_status_t* status );

bool             job_init();
void             job_shutdown();

