#include "main.h"


#include <mutex>
#include <threads.h>

// ====================================================================================================

static thread_local bool MEM_TRACK_PAUSE    = false;

e_mem_category           g_mem_redirect_src = e_mem_category_general;
e_mem_category           g_mem_redirect_dst = e_mem_category_general;
bool                     g_mem_redirect     = false;

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

	info.sizes[ memory ] = { memory, size, app::total_time, stack };

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


void mem_draw_debug_ui()
{
	ImGui::Text( "%.1f FPS (%.3f ms/frame)", ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate );

	// ImGui::Text( "App Time: %.3f Sec", app::total_time );
	ImGui::Text( "App Time: %.3f Sec", app::total_time / 1000.f );

	ImGui::SeparatorText( "Memory" );

	proc_mem_info_t proc_mem_info = sys_get_mem_info();

	// ImGui::Text( "Process Memory: %.3f MB", (float)( proc_mem_info.working_set + proc_mem_info.page_file ) / ( 1000.f * 1000.f ) );
	size_t          untracked_mem = proc_mem_info.working_set - g_total_memory_allocated;

	ImGui::Text( "Process Total: %.3f MB", (float)( proc_mem_info.working_set ) / ( MEM_SCALE * MEM_SCALE ) );
	ImGui::Text( "Tracked: %.3f MB", (float)g_total_memory_allocated / ( MEM_SCALE * MEM_SCALE ) );
	ImGui::Text( "Untracked: %.3f MB", (float)untracked_mem / ( MEM_SCALE * MEM_SCALE ) );
	ImGui::Text( "Page File: %.3f MB", (float)( proc_mem_info.page_file ) / ( MEM_SCALE * MEM_SCALE ) );

#if USE_MIMALLOC
	if ( ImGui::Button( "mimalloc print" ) )
	{
		mi_options_print();

		mi_collect( true );
		mi_stats_merge();
		mi_stats_print( nullptr );
	}
#endif

	// show memory usage
	static std::vector< mem_alloc_info_t > sorted_mem_infos[ e_mem_category_count ];

	for ( u8 i = 0; i < e_mem_category_count; i++ )
	{
		mem_category_info_t& info = get_mem_categories()[ i ];

		ImGui::PushID( i + 1 );

		ImGui::Separator();
		//ImGui::TextUnformatted( mem_category_str[ i ] );
		//ImGui::Spacing();

		ImGui::Text( "%s: %.3f KB", mem_category_str[ i ], (float)info.total / MEM_SCALE );


		ImGui::Text( "Allocations: %zd", info.sizes.size() );
		// ImGui::Text( "Allocations: %zd", info.alloc_count );

		//char header_name[ 64 ];
		//snprintf( header_name, 64, "Allocations: %zd", info.sizes.size() );

		if ( ImGui::CollapsingHeader( "Allocations" ) )
		{
			ImGui::PushItemWidth( -1 );

			ImGui::SetNextWindowSizeConstraints( ImVec2( 0.0f, ImGui::GetTextLineHeightWithSpacing() * 1 ), ImVec2( FLT_MAX, ImGui::GetTextLineHeightWithSpacing() * 10 ) );

			if ( ImGui::BeginChild( "##alloc", ImVec2( -FLT_MIN, 0.0f ), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY ) )
			{
#if 1
				sorted_mem_infos[ i ].clear();
				sorted_mem_infos[ i ].reserve( info.sizes.size() );

				for ( const auto& [ ptr, ptr_info ] : info.sizes )
				{
					sorted_mem_infos[ i ].push_back( ptr_info );
				}

				std::qsort( sorted_mem_infos[ i ].data(), sorted_mem_infos[ i ].size(), sizeof( mem_alloc_info_t ), qsort_memory_newest );

				// 	for ( const auto& [ ptr, ptr_info ] : info.sizes )
				for ( size_t mem_i = 0; mem_i < sorted_mem_infos[ i ].size(); mem_i++ )
				{
					const mem_alloc_info_t& ptr_info = sorted_mem_infos[ i ][ mem_i ];

					ImGui::Text( "%.3f Sec - Ptr: %p - %.3f KB", ptr_info.app_time / ( 1000.f ), ptr_info.ptr, (float)ptr_info.size / MEM_SCALE );

					if ( ptr_info.stack_trace )
					{
						ImGui::PushID( ptr_info.ptr );

						if ( ImGui::TreeNodeEx( "Stack Trace", ImGuiTreeNodeFlags_SpanFullWidth ) )
						{
							ImGui::TextUnformatted( std::to_string( *ptr_info.stack_trace ).c_str() );
							ImGui::TreePop();
						}

						ImGui::PopID();
					}
				}
#else
				for ( size_t mem_i = 0; mem_i < info.alloc_count; mem_i++ )
				{
					mem_alloc_info_t& alloc = info.alloc[ mem_i ];

					ImGui::Text( "Ptr: %p - %.3f KB", alloc.ptr, (float)alloc.size / MEM_SCALE );

					if ( alloc.stack_trace )
					{
						ImGui::PushID( alloc.ptr );

						if ( ImGui::TreeNodeEx( "Stack Trace", ImGuiTreeNodeFlags_SpanFullWidth ) )
						{
							ImGui::TextUnformatted( std::to_string( *alloc.stack_trace ).c_str() );
							ImGui::TreePop();
						}

						ImGui::PopID();
					}
				}
#endif
			}

			ImGui::EndChild();

			//		if ( ImGui::BeginListBox( "##alloc" ) )
			//		{
			//			for ( const auto& [ ptr, ptr_info ] : info.sizes )
			//			{
			//				ImGui::Text( "Ptr: %p - %.3f KB", ptr, (float)ptr_info.size / 1000.f );
			//
			//				ImGui::PushID( ptr );
			//
			//				if ( ImGui::TreeNodeEx( "Stack Trace", ImGuiTreeNodeFlags_SpanFullWidth ) )
			//				{
			//					ImGui::TextUnformatted( std::to_string( ptr_info.stack_trace ).c_str() );
			//					ImGui::TreePop();
			//				}
			//
			//				ImGui::PopID();
			//			}
			//
			//			ImGui::EndListBox();
			//		}

			ImGui::PopItemWidth();
		}

		ImGui::PopID();
	}
}

