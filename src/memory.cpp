#include "main.h"


#include <mutex>
#include <threads.h>

// ====================================================================================================

static thread_local bool MEM_TRACK_PAUSE    = false;

e_mem_category g_mem_redirect_src = e_mem_category_general;
e_mem_category g_mem_redirect_dst = e_mem_category_general;
bool           g_mem_redirect     = false;

#define MEM_TRACK_ENABLE      1
#define MEM_TRACK_STACK_TRACE 0


// ====================================================================================================

size_t                              g_total_memory_allocated = 0;

mem_category_info_t*                get_mem_categories()
{
	static mem_category_info_t mem_categories[ e_mem_category_count ];
	return mem_categories;
}


const char*                         mem_category_str[] = {
    "mem_category_general",

    "mem_category_image_data",
    "mem_category_image",
    "mem_category_string",
    "mem_category_file_data",

    "mem_category_imgui",
    "mem_category_stbi_resize",
};


static_assert( ARR_SIZE( mem_category_str ) == e_mem_category_count );


void* imgui_mem_alloc( size_t sz, void* user_data )
{
	void* memory = malloc( sz );
	mem_add_item( e_mem_category_imgui, memory, sz );
	return memory;
}


void imgui_mem_free( void* ptr, void* user_data )
{
	mem_free_item( e_mem_category_imgui, ptr );
	free( ptr );
}


static std::mutex alloc_lock;


void mem_add_item( e_mem_category category, void* memory, size_t size )
{
#if MEM_TRACK_ENABLE
	if ( MEM_TRACK_PAUSE )
		return;

	if ( category >= e_mem_category_count )
		return;

	alloc_lock.lock();
	MEM_TRACK_PAUSE = true;
	
	mem_category_info_t& info = get_mem_categories()[ category ];

	std::stacktrace* stack = nullptr;

	#if MEM_TRACK_STACK_TRACE
	stack = new std::stacktrace( std::stacktrace::current() );
	#endif

	info.sizes[ memory ] = { memory, size, g_total_time, stack };

	info.total += size;
	g_total_memory_allocated += size;

	MEM_TRACK_PAUSE = false;
	alloc_lock.unlock();
#endif
}


void mem_free_item( e_mem_category category, void* memory )
{
#if MEM_TRACK_ENABLE
	if ( !memory )
		return;

	if ( category >= e_mem_category_count )
		return;

	alloc_lock.lock();

	mem_category_info_t& info = get_mem_categories()[ category ];

	auto it = info.sizes.find( memory );

	if ( it == info.sizes.end() )
	{
		printf( "%s - FREE NOT FOUND\n", mem_category_str[ category ] );
	}
	else
	{
		if ( info.total < it->second.size )
		{
			printf( "%s - FREE MORE THAN ALLOCATED\n", mem_category_str[ category ] );
		}
		else
		{
			info.total -= it->second.size;
			g_total_memory_allocated -= it->second.size;
		}

	#if MEM_TRACK_STACK_TRACE
		delete it->second.stack_trace;
	#endif

		info.sizes.erase( it );
	}

	alloc_lock.unlock();
#endif
}

