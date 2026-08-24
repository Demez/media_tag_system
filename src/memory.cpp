#include "main.h"

#include <mutex>

// ====================================================================================================

static thread_local bool MEM_TRACK_PAUSE    = false;

#define MEM_TRACK_ENABLE      1
#define MEM_TRACK_KEEP_FREE   0


// ====================================================================================================

size_t               g_total_memory_allocated = 0;

mem_category_info_t* get_mem_categories()
{
	static mem_category_info_t mem_categories[ e_mem_category_count ];
	return mem_categories;
}


const char* mem_category_str[] = {
    "general",

    "thread_data",
    "image_data",
    "image",
    "gl_texture_data",
    "string",
    "file_data",

	"stbi_resize",
	"jxl",
	"jxl_thumbnail",
	"thumbnail_cache",
};


static_assert( ARR_SIZE( mem_category_str ) == e_mem_category_count );


static std::mutex alloc_lock;


void mem_add_item( e_mem_category category, void* memory, size_t size, size_t stack_skip, size_t stack_depth )
{
#if MEM_TRACK_ENABLE
	if ( MEM_TRACK_PAUSE )
		return;

	if ( category >= e_mem_category_count )
		return;

	alloc_lock.lock();
	MEM_TRACK_PAUSE = true;
	
	mem_category_info_t& info = get_mem_categories()[ category ];

#if MEM_TRACK_STACK_TRACE
	std::stacktrace* stack = new std::stacktrace( std::stacktrace::current( stack_skip + 1, stack_depth ) );
	#endif

	info.sizes[ memory ] = {
		memory, size, sys_get_time_ms(),
  #if MEM_TRACK_STACK_TRACE
		stack,
  #endif
		false
	};

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

	#if !MEM_TRACK_KEEP_FREE
	#if MEM_TRACK_STACK_TRACE
		delete it->second.stack_trace;
	#endif

		info.sizes.erase( it );
	#else
		it->second.freed = true;
	#endif
	}

	alloc_lock.unlock();
#endif
}


int qsort_memory_newest( const void* left, const void* right )
{
	const mem_alloc_info_t* item_left  = static_cast< const mem_alloc_info_t* >( left );
	const mem_alloc_info_t* item_right = static_cast< const mem_alloc_info_t* >( right );

	if ( item_left->app_time > item_right->app_time )
		return -1;
	else if ( item_left->app_time < item_right->app_time )
		return 1;

	return 0;
}


static bool g_mem_allocation_show[ e_mem_category_count ]{};


void mem_draw_debug_ui()
{
}

