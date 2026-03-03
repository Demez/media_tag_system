#include "main.h"

#include <unordered_map>


size_t                              g_total_memory_allocated = 0;

size_t                              g_imgui_memory_allocated = 0;
std::unordered_map< void*, size_t > g_imgui_alloc_sizes;


// SetAllocatorFunctions

typedef void* ( *ImGuiMemAllocFunc )( size_t sz, void* user_data );  // Function signature for ImGui::SetAllocatorFunctions()
typedef void ( *ImGuiMemFreeFunc )( void* ptr, void* user_data );    // Function signature for ImGui::SetAllocatorFunctions()


void* imgui_mem_alloc( size_t sz, void* user_data )
{
	g_imgui_memory_allocated += sz;
	void* memory = malloc( sz );
	g_imgui_alloc_sizes[ memory ] = sz;
	return memory;
}


void imgui_mem_free( void* ptr, void* user_data )
{
	auto it = g_imgui_alloc_sizes.find( ptr );

	if ( it == g_imgui_alloc_sizes.end() )
	{
		printf( "IMGUI FREE NOT FOUND\n" );
	}
	else
	{
		if ( g_imgui_memory_allocated < it->second )
		{
			printf( "IMGUI FREE MORE THAN ALLOCATED\n" );
		}
		else
		{
			g_imgui_memory_allocated -= it->second;
		}

		g_imgui_alloc_sizes.erase( it );
	}

	free( ptr );
}

