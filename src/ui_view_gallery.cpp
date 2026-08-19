#include "main.h"

#include "imgui_internal.h"


namespace gallery
{
	e_gallery_scan                       scan_state;

	// a sorted list of media entries, each item is an index to an entry in directory::media_list
	std::vector< size_t >                sorted_media{};

	char                                 search[ 512 ];

	// cursor position/index in items
	// size_t                        cursor            = 0;

	e_gallery_sort_mode                  sort_mode          = e_gallery_sort_mode_date_mod_new_to_old;
	bool                                 sort_mode_update   = false;

	u32                                  row_count          = 0;
	u32                                  item_size          = 150;
	u32                                  item_size_min      = 70;
	u32                                  item_size_max      = 600;
	bool                                 item_size_changed  = true;
	bool                                 item_size_changing = false;
	std::vector< ImVec2 >                item_text_size;

	std::vector< gallery_item_draw_t >   item_layout;
	gallery_item_draw_t**                visible_item       = nullptr;
	size_t                               visible_item_count = 0;

	// area the image can fit within the item
	ImVec2                               image_bounds{ static_cast< float >( item_size ), static_cast< float >( item_size ) };

	bool                                 sidebar_draw         = true;
	bool                                 content_area_resized = false;

	// RENAME: scroll to last selected item
	// TODO: this is sometimes used as a way to keep the scroll position
	bool                                 scroll_to_cursor     = false;
	bool                                 keep_scroll_pos      = false;  // keeps the scroll position when resizing
	int                                  refresh_layout       = 0;      // refresh the layout X amount of times, imgui may need another frame to recalc properly

	u32                                  drawn_image_count    = 0;
	u32                                  first_visible_item   = 0;

	// Quick Filter
	e_gallery_filter                     filter{};

	// Files selected in the gallery view
	std::vector< selection_t >           selection{};

	// used for memory with media advancing with arrow keys
	selection_t                          last_selection{};

	bool                                 always_recalc_item_sizes = false;
	bool                                 always_recalc_layout     = false;
}


const char* g_gallery_sort_mode_str[] = {
	"A to Z",
	"Z to A",
	"Date Modified - New to Old",
	"Date Modified - Old to New",
	"Date Created - New to Old",
	"Date Created - Old to New",
	"File Size - Large to Small",
	"File Size - Small to Large"
};

static_assert( ARR_SIZE( g_gallery_sort_mode_str ) == e_gallery_sort_mode_count );


static const media_entry_t __media_entry_empty{};


float                      gallery_view_draw_header();
void                       gallery_view_update_header_directory();

void                       gallery_view_draw_sidebar();


void                       gallery_draw_extra_refresh( int count )
{
	if ( count > gallery::refresh_layout )
		gallery::refresh_layout = count;

	set_frame_draw( count );
}

// =============================================================================================


const media_entry_t& gallery_item_get_media_entry( size_t index )
{
	if ( index >= gallery::sorted_media.size() )
		return __media_entry_empty;

	return directory::media_list[ gallery::sorted_media[ index ] ];
}


const file_t& gallery_item_get_file( size_t index )
{
	const media_entry_t& entry = gallery_item_get_media_entry( index );
	return entry.file;
}


fs::path gallery_item_get_path( size_t index )
{
	const media_entry_t& entry = gallery_item_get_media_entry( index );
	return directory::path / entry.file.path;
}


std::string gallery_item_get_path_string( size_t index )
{
	const media_entry_t& entry = gallery_item_get_media_entry( index );
	return sys_path_to_string( directory::path / entry.file.path );
}


// =============================================================================================
// Selection System


bool gallery_view_input_do_multi_select()
{
	return ImGui::IsKeyDown( ImGuiKey_LeftShift ) || ImGui::IsKeyDown( ImGuiKey_LeftCtrl );
}


void gallery_view_input_check_clear_multi_select()
{
	if ( !ImGui::IsKeyDown( ImGuiKey_LeftShift ) && !ImGui::IsKeyDown( ImGuiKey_LeftCtrl ) )
	// if ( !gallery_view_input_do_multi_select() )
	{
		if ( gallery::selection.size() )
		{
			gallery::selection.clear();
		}
	}
}


void gallery_view_input_update_multi_select( u32 index, bool readd )
{
	// check if this exists in the list already
	// if it does, remove it so we can add it to the end
	// instead, just remove it and move on
	for ( u32 i = 0; i < gallery::selection.size(); i++ )
	{
		if ( gallery::selection[ i ].index == index )
		{
			gallery::selection.erase( gallery::selection.begin() + i );

			if ( !readd )
				return;
		}
	}

	selection_t selection{
		.index = index,
		.entry = gallery_item_get_media_entry( index ),
	};

	gallery::selection.push_back( selection );
	gallery::last_selection = selection;
}


selection_t gallery_view_get_last_selected()
{
	if ( gallery::selection.empty() )
		return {};

	return gallery::selection.back();
}


u32 gallery_view_get_last_selected_index( u32 empty_return )
{
	if ( gallery::selection.empty() )
		return empty_return;

	return gallery::selection.back().index;
}


bool gallery_view_selection_cleared()
{
	return ( gallery::selection.empty() && gallery::last_selection.entry.type == e_media_type_none );
}


media_entry_t gallery_view_get_last_selected_entry()
{
	if ( gallery::selection.empty() )
		return {};

	return gallery::selection.back().entry;
}


void gallery_view_set_selection( size_t gallery_item_index )
{
	if ( directory::media_list.empty() )
		return;

	gallery::selection.clear();

	selection_t selection{
		.index = (u32)gallery_item_index,
		.entry = gallery_item_get_media_entry( gallery_item_index ),
	};

	gallery::selection.push_back( selection );
	gallery::last_selection = selection;

#if 0
	if ( gallery_item_index >= gallery::sorted_media.size() )
	{
		g_selected_item_cache.clear();

		g_selected_item_cache.file = {};
		g_selected_item_cache.type = e_media_type_none;
		g_selected_item_cache.filename.clear();
		return;
	}

	g_selected_item_cache = gallery_item_get_media_entry( gallery_item_index );
#endif
}


void gallery_view_clear_selection()
{
	gallery::selection.clear();

	gallery::last_selection.index                   = 0;
	gallery::last_selection.entry.type              = e_media_type_none;
	gallery::last_selection.entry.file.size         = 0;
	gallery::last_selection.entry.file.date_mod     = 0;
	gallery::last_selection.entry.file.date_created = 0;
	gallery::last_selection.entry.file.type         = e_file_type_invalid;
	gallery::last_selection.entry.file.path.clear();
	gallery::last_selection.entry.file.name.clear();
}


void gallery_view_delete_selection()
{
	if ( !delete_file_window( gallery::selection.size() ) )
		return;

	for ( selection_t& selection : gallery::selection )
	{
		// TODO: undo history
		fs::path path = directory::path / selection.entry.file.path;
		sys_recycle_file( path.c_str() );
	}
}


void gallery_view_input()
{
	u32  selection = gallery_view_get_last_selected_index();
	bool empty     = gallery::selection.empty();

	if ( empty && gallery::last_selection.entry.type != e_media_type_none )
	{
		selection = gallery::last_selection.index;
		empty     = false;
	}

	if ( ImGui::IsKeyPressed( ImGuiKey_Home ) )
	{
		gallery_view_scroll_to_cursor();
		gallery_view_input_update_multi_select( 0 );
	}
	else if ( ImGui::IsKeyPressed( ImGuiKey_End ) )
	{
		if ( gallery::sorted_media.size() )
			selection = static_cast< u32 >( gallery::sorted_media.size() - 1 );

		gallery_view_scroll_to_cursor();
		gallery_view_input_update_multi_select( selection );
	}
	else if ( ImGui::IsKeyPressed( ImGuiKey_LeftArrow ) )
	{
		gallery_view_input_check_clear_multi_select();

		if ( !empty )
		{
			if ( selection == 0 )
				selection = static_cast< u32 >( gallery::sorted_media.size() );

			selection--;
		}

		gallery_view_scroll_to_cursor();
		gallery_view_input_update_multi_select( selection );
	}
	else if ( ImGui::IsKeyPressed( ImGuiKey_RightArrow ) )
	{
		gallery_view_input_check_clear_multi_select();

		// TODO: CRASH HERE ON EMPTY FOLDER
		if ( !empty )
			selection = ( selection + 1 ) % static_cast< u32 >( gallery::sorted_media.size() );

		gallery_view_scroll_to_cursor();
		gallery_view_input_update_multi_select( selection );
	}
	else if ( ImGui::IsKeyPressed( ImGuiKey_UpArrow ) )
	{
		gallery_view_input_check_clear_multi_select();

		if ( !empty )
		{
			if ( selection < gallery::row_count )
			{
				size_t count_in_row   = gallery::sorted_media.size() % static_cast< size_t >( gallery::row_count );
				size_t missing_in_row = gallery::row_count - count_in_row;
				size_t row_diff       = gallery::row_count - selection;

				// advance up a row
				if ( missing_in_row >= row_diff )
					row_diff += gallery::row_count;

				selection = gallery::sorted_media.size() - ( row_diff - missing_in_row );
			}
			else
			{
				selection = ( selection - gallery::row_count ) % gallery::sorted_media.size();
			}
		}

		gallery_view_scroll_to_cursor();
		gallery_view_input_update_multi_select( selection );
	}
	else if ( ImGui::IsKeyPressed( ImGuiKey_DownArrow ) )
	{
		gallery_view_input_check_clear_multi_select();
		
		if ( !empty )
		{
			if ( selection + gallery::row_count >= gallery::sorted_media.size() )
			{
				size_t count_in_row = gallery::sorted_media.size() % gallery::row_count;
				size_t row_pos      = selection % gallery::row_count;
				selection           = row_pos;
			}
			else
			{
				selection = ( selection + gallery::row_count ) % gallery::sorted_media.size();
			}
		}

		gallery_view_scroll_to_cursor();
		gallery_view_input_update_multi_select( selection );
	}
	else if ( ImGui::IsKeyPressed( ImGuiKey_Delete ) )
	{
		gallery_view_delete_selection();
	}
}


// =============================================================================================
// Sorting


// Date Modified
int qsort_date_mod_newest( const void* left, const void* right )
{
	const file_t& file_left  = directory::media_list[ *static_cast< const size_t* >( left ) ].file;
	const file_t& file_right = directory::media_list[ *static_cast< const size_t* >( right ) ].file;

	if ( file_left.date_mod > file_right.date_mod )
		return -1;
	else if ( file_left.date_mod < file_right.date_mod )
		return 1;

	return 0;
}


int qsort_date_mod_oldest( const void* left, const void* right )
{
	return qsort_date_mod_newest( left, right ) * -1;
}


// Date Created
int qsort_date_created_newest( const void* left, const void* right )
{
	const file_t& file_left  = directory::media_list[ *static_cast< const size_t* >( left ) ].file;
	const file_t& file_right = directory::media_list[ *static_cast< const size_t* >( right ) ].file;

	if ( file_left.date_created > file_right.date_created )
		return -1;
	else if ( file_left.date_created < file_right.date_created )
		return 1;

	return 0;
}


int qsort_date_created_oldest( const void* left, const void* right )
{
	return qsort_date_created_newest( left, right ) * -1;
}


// File Size
int qsort_size_large_to_small( const void* left, const void* right )
{
	const file_t& file_left  = directory::media_list[ *static_cast< const size_t* >( left ) ].file;
	const file_t& file_right = directory::media_list[ *static_cast< const size_t* >( right ) ].file;

	if ( file_left.size > file_right.size )
		return -1;
	else if ( file_left.size < file_right.size )
		return 1;

	return 0;
}


int qsort_size_small_to_large( const void* left, const void* right )
{
	return qsort_size_large_to_small( left, right ) * -1;
}


void gallery_view_sort_list( std::vector< size_t >& gallery_list, e_gallery_sort_mode sort_mode )
{
	// Sort data
	switch ( sort_mode )
	{
		default:
		case e_gallery_sort_mode_name_a_z:
		{
			break;
		}

		case e_gallery_sort_mode_name_z_a:
		{
			// this only works since fs::directory_iterator is sorted A to Z by default
			std::reverse( gallery_list.begin(), gallery_list.end() );
			break;
		}

		case e_gallery_sort_mode_date_mod_new_to_old:
		{
			std::qsort( gallery_list.data(), gallery_list.size(), sizeof( size_t ), qsort_date_mod_newest );
			break;
		}

		case e_gallery_sort_mode_date_mod_old_to_new:
		{
			std::qsort( gallery_list.data(), gallery_list.size(), sizeof( size_t ), qsort_date_mod_oldest );
			break;
		}

		case e_gallery_sort_mode_date_created_new_to_old:
		{
			std::qsort( gallery_list.data(), gallery_list.size(), sizeof( size_t ), qsort_date_created_newest );
			break;
		}

		case e_gallery_sort_mode_date_created_old_to_new:
		{
			std::qsort( gallery_list.data(), gallery_list.size(), sizeof( size_t ), qsort_date_created_oldest );
			break;
		}

		case e_gallery_sort_mode_size_large_to_small:
		{
			std::qsort( gallery_list.data(), gallery_list.size(), sizeof( size_t ), qsort_size_large_to_small );
			break;
		}

		case e_gallery_sort_mode_size_small_to_large:
		{
			std::qsort( gallery_list.data(), gallery_list.size(), sizeof( size_t ), qsort_size_small_to_large );
			break;
		}
	}
}


void gallery_find_selected_file()
{
	// make a copy of this
	std::vector< selection_t > selection_copy( gallery::selection );

	gallery_view_clear_selection();

	size_t i = 0;
	for ( ; i < gallery::sorted_media.size(); i++ )
	{
		if ( selection_copy.empty() )
			break;

		const file_t& file = gallery_item_get_file( i );

		for ( size_t s = 0; s < selection_copy.size(); s++ )
		{
			if ( selection_copy[ s ].entry.file != file )
				continue;

			selection_t selection{
				.index = (u32)i,
				.entry = gallery_item_get_media_entry( i ),
			};

			gallery::selection.push_back( selection );
			gallery::last_selection = selection;

			selection_copy.erase( selection_copy.begin() + s );
			gallery_view_scroll_to_cursor();
			break;
		}
	}
}


struct gallery_sort_dir_data_t
{
	std::vector< size_t > sorted_media;
	char*                 search;
	size_t                search_len = 0;
	e_gallery_sort_mode   sort_mode;
	e_gallery_filter      filter;
	bool                  resort;  // true to do another resort, used for live user searching
};

// check if we are cancelled every X items
constexpr size_t     SORT_CHECK_CANCEL_ITEM_COUNT  = 3000;

static job_status_t* g_item_size_calc_job = nullptr;
static job_status_t* g_gallery_sort_job   = nullptr;


// this is actually such a lazy way to do this, it is not good lol
static bool          g_gallery_sort_block_new_jobs = false;


void gallery_view_item_size_calc( ImGuiStyle& style, size_t count );


bool gallery_sort_check_if_canceled( job_status_t* status, gallery_sort_dir_data_t* sort_data )
{
	size_t new_search_len = strlen( gallery::search );

	// in case the user changed these while starting the task
	sort_data->resort |= sort_data->search_len != new_search_len;
	sort_data->resort |= sort_data->filter != gallery::filter;
	sort_data->resort |= sort_data->sort_mode != gallery::sort_mode;

	if ( sort_data->resort )
	{
		printf( "SORT JOB NEED RESORT - %p\n", status );
		job_cancel_and_free( status );
	}

	if ( status->cancel )
	{
		printf( "SORT JOB CANCELLED - %p\n", status );
		return true;
	}

	return sort_data->resort;
}


void gallery_view_sort_dir_func( job_status_t* status )
{
	if ( g_gallery_sort_block_new_jobs )
	{
		status->cancel = true;
		return;
	}

	std::vector< size_t > folders;
	std::vector< size_t > files;

	folders.reserve( directory::media_list.size() );
	files.reserve( directory::media_list.size() );

	auto sort_data = static_cast< gallery_sort_dir_data_t* >( status->userdata );

	if ( gallery_sort_check_if_canceled( status, sort_data ) )
		return;

	printf( "FILTERING - %p\n", status );

	// Split up lists
	for ( size_t i = 0; i < directory::media_list.size(); i++ )
	{
		// check every X entries
		if ( i % SORT_CHECK_CANCEL_ITEM_COUNT == 0 )
		{
			if ( gallery_sort_check_if_canceled( status, sort_data ) )
				return;
		}

		e_media_type type = directory::media_list[ i ].type;

		if ( sort_data->filter )
		{
			if ( type == e_media_type_directory && !( sort_data->filter & e_gallery_filter_folders ) )
				continue;

			if ( type == e_media_type_image && !( sort_data->filter & e_gallery_filter_images ) )
				continue;

			if ( type == e_media_type_video && !( sort_data->filter & e_gallery_filter_videos ) )
				continue;
		}

		if ( sort_data->search_len )
		{
			media_entry_t& entry = directory::media_list[ i ];
			char*          find  = SDL_strcasestr( entry.file.name.c_str(), sort_data->search );

			if ( !find )
				continue;
		}

		if ( type == e_media_type_directory )
			folders.push_back( i );
		else
			files.push_back( i );
	}

	printf( "SORTING - %p\n", status );

	// Sort data
	if ( sort_data->sort_mode != e_gallery_sort_mode_size_large_to_small && sort_data->sort_mode != e_gallery_sort_mode_size_small_to_large )
		gallery_view_sort_list( folders, sort_data->sort_mode );

	if ( gallery_sort_check_if_canceled( status, sort_data ) )
		return;

	gallery_view_sort_list( files, sort_data->sort_mode );

	if ( gallery_sort_check_if_canceled( status, sort_data ) )
		return;

	sort_data->sorted_media.resize( folders.size() + files.size() );

	// Add Folders First
	//std::copy( folders.begin(), folders.end(), sort_data->sorted_media.begin() );
	std::move( folders.begin(), folders.end(), sort_data->sorted_media.begin() );

	// Add Files next
	//std::copy( files.begin(), files.end(), sort_data->sorted_media.begin() + folders.size() );
	std::move( files.begin(), files.end(), sort_data->sorted_media.begin() + folders.size() );

	printf( "DONE SORTING - %p\n", status );
}


extern void select_image_in_folder( bool force_load_media );


void gallery_view_sort_dir_finish( job_status_t* status, bool in_main_thread )
{
	g_gallery_sort_job = nullptr;

	if ( gallery::scan_state == e_gallery_scan_idle )
		return;

	// TODO: it would be nice for this to not be on the main thread when in a resize or something, but that's too tricky atm
	if ( !in_main_thread )
		return;

	printf( "SORT FINISH FUNC - %p\n", status );

	//g_gallery_sort_block_new_jobs = false;

	auto sort_data        = static_cast< gallery_sort_dir_data_t* >( status->userdata );
	gallery::sorted_media = sort_data->sorted_media;

	if ( !directory::queued.empty() )
		select_image_in_folder( false );

	if ( !gallery::selection.empty() )
		gallery_find_selected_file();

	// gallery_view_scroll_to_cursor();
	gallery_draw_extra_refresh( 2 );
	gallery::sort_mode_update = true;

	gallery_view_reset_text_size();

	// reset vars here, since this is the very end of the folder loading path
	// directory::folder_reload = false;
	directory::queued.clear();

	update_window_title();

	set_frame_draw();

	gallery::scan_state = e_gallery_scan_idle;
}


void gallery_view_sort_dir_free( job_status_t* status )
{
	auto sort_data = static_cast< gallery_sort_dir_data_t* >( status->userdata );
	ch_free_str( sort_data->search );

	// if a resort is needed, push a new job
	if ( sort_data->resort && status->cancel )
		gallery_view_sort_dir();

	delete sort_data;

	//if ( g_gallery_sort_job == status )
	//	printf( "FREEING CURRENT STATUS !!!!\n" );
	//else if ( g_gallery_sort_job )
	//	printf( "FREE SORT DATA - g_gallery_sort_job EXISTS\n" );
	//else
		printf( "FREE SORT DATA - %p\n", status );
}


void gallery_view_sort_dir()
{
	if ( g_gallery_sort_job )
	{
		auto sort_data = static_cast< gallery_sort_dir_data_t* >( g_gallery_sort_job->userdata );

		if ( !sort_data->resort )
			return;
	}
	else if ( gallery::scan_state == e_gallery_scan_sorting )
		return;

	//if ( g_gallery_sort_block_new_jobs )
	//	return;

	gallery::scan_state = e_gallery_scan_sorting;
	set_frame_draw();

	job_cancel_and_free( g_gallery_sort_job );

	auto sort_data        = new gallery_sort_dir_data_t;
	sort_data->search_len = strlen( gallery::search );
	sort_data->search     = util_strndup( gallery::search, sort_data->search_len );
	sort_data->filter     = gallery::filter;
	sort_data->sort_mode  = gallery::sort_mode;
	sort_data->resort     = false;

	g_gallery_sort_job    = job_push( gallery_view_sort_dir_finish, gallery_view_sort_dir_func, gallery_view_sort_dir_free, sort_data );

	printf( "SORT JOB PUSHED - %p\n", g_gallery_sort_job );
}


// =============================================================================================


void gallery_view_dir_change( bool keep_selection )
{
	gallery_view_update_header_directory();

	//if ( keep_selection )
	//	gallery_view_set_selection( gallery::cursor );

	// TODO: make it work with recursive, so if the selected item is still within the results, use that to snap scroll view to
	if ( !directory::folder_reload || directory::recursive )
		gallery_view_reset();

	gallery_view_sort_dir();

	// Invalidate These
	g_image_data.index        = SIZE_MAX;
	g_image_scaled_data.index = SIZE_MAX;
}


void gallery_view_scroll_to_cursor()
{
	gallery::scroll_to_cursor = true;
}


void gallery_view_reset()
{
	gallery::item_size_changed = true;

	gallery::sorted_media.clear();
	gallery::item_text_size.clear();
	gallery::item_layout.clear();

	gallery::visible_item_count = 0;
}


void gallery_view_reset_text_size()
{
	gallery::item_size_changed  = true;
	// gallery_view_scroll_to_cursor();

	gallery::item_text_size.clear();
	gallery::item_text_size.resize( gallery::sorted_media.size() );  // SLOW

	gallery::item_layout.clear();
	gallery::item_layout.resize( gallery::sorted_media.size() );  // SLOW

	// TODO: this could use less memory
	gallery::visible_item       = ch_realloc( gallery::visible_item, gallery::sorted_media.size() + 2, e_mem_category_general );
	gallery::visible_item_count = 0;

	if ( !gallery::visible_item )
	{
		printf( "CANT REALLOC MEMORY??\n" );
		exit( 400 );
	}

	memset( gallery::visible_item, 0, sizeof( void* ) * ( gallery::sorted_media.size() + 2 ) );
}


void gallery_view_draw_scan_state()
{
	e_gallery_scan scan_state   = gallery::scan_state;

	ImGuiStyle&    style        = ImGui::GetStyle();

	// center it in the content area
	ImVec2         region_avail = ImGui::GetContentRegionAvail();
	ImVec2         region_size  = { region_avail.x + style.WindowPadding.x, region_avail.y + style.WindowPadding.y };

	ImVec2         pos          = region_size;
	pos.x *= 0.5f;
	pos.y *= 0.5f;
	pos += ImGui::GetCursorScreenPos();

	ImGui::SetNextWindowPos( pos, 0, { 0.5f, 0.5f } );

	if ( !ImGui::BeginChild( "##gallery_scan_status", {}, ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysAutoResize ) )
	{
		ImGui::EndChild();
		return;
	}

	if ( app::config.dev_mode )
	{
		ImGui::PushFont( font::normal_bold, app::config.font_size * 2 );
		switch ( scan_state )
		{
			default:
				break;

			case e_gallery_scan_filesystem:
				ImGui::TextUnformatted( "Reading Filesystem" );
				break;

			case e_gallery_scan_building:
				ImGui::TextUnformatted( "Building Media Entries" );
				break;

			case e_gallery_scan_sorting:
				ImGui::TextUnformatted( "Sorting" );
				break;
		}
		ImGui::PopFont();
	}
	else
	{
		image_t* image = icon_get_image( e_icon_loading );

		if ( image )
		{
			ImVec2 image_size( image->width, image->height );
			ImGui::Image( icon_get_imtexture( e_icon_loading ), image_size );
		}
	}

	ImGui::EndChild();
}


void gallery_view_draw()
{
	gallery_view_input();

	int window_width, window_height;
	SDL_GetWindowSize( app::window, &window_width, &window_height );

	//ImGui::SetNextWindowPos( { 0, 0 } );
	// ImGui::SetCursorPos( { 0, 0 } );

	// Header
	float header_height = gallery_view_draw_header();

	// ImVec2 region_avail = ImGui::GetWindowContentRegionMax();

	//ImGui::SetNextWindowPos( { 0, 0 } );

	// ImVec2 cursor_pos = ImGui::GetCursorPos();

	//ImGui::SetCursorPosX( 0.f );
	ImGui::SetNextWindowPos( { 0, header_height } );
	ImGui::SetNextWindowSize( { (float)window_width, (float)window_height - header_height } );

	if ( app::config.use_custom_colors )
		ImGui::SetNextWindowBgAlpha( app::config.header_bg_color.w );

	if ( !ImGui::Begin( "##gallery_main", 0, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar ) )
	{
		ImGui::End();
		return;
	}

	// Sidebar
	if ( gallery::sidebar_draw )
	{
		ImGui::SetCursorPosX( 0 );
		ImGui::SetCursorPosY( 0 );

		if ( app::config.use_custom_colors )
			ImGui::PushStyleColor( ImGuiCol_ChildBg, app::config.sidebar_bg_color );

		gallery_view_draw_sidebar();

		if ( app::config.use_custom_colors )
			ImGui::PopStyleColor();

		ImGui::SameLine();
	}

	// Gallery View
	ImGui::SetCursorPosY( 0 );

	if ( app::config.use_custom_colors )
		ImGui::PushStyleColor( ImGuiCol_ChildBg, app::config.content_bg_color );

	if ( gallery::scan_state == e_gallery_scan_idle )
	{
		gallery_view_draw_content();
	}
	else
	{
		gallery_view_draw_scan_state();
	}

	if ( app::config.use_custom_colors )
		ImGui::PopStyleColor();

	ImGui::End();

	if ( ImGui::GetIO().WantTextInput )
		return;

	// TODO: Test ImGui::Shortcut()
	if ( app::window_focused && ImGui::IsKeyDown( ImGuiKey_LeftCtrl ) && ImGui::IsKeyPressed( ImGuiKey_C, false ) )
	{
		std::vector< fs::path > files{};
		files.reserve( gallery::selection.size() );

		for ( const selection_t& selection : gallery::selection )
			files.push_back( directory::path / selection.entry.file.path );

		if ( sys_copy_to_clipboard( files ) )
		{
			printf( "Copied to Clipboard\n" );
			push_notification( "Copied" );
		}
		else
		{
			printf( "Failed to Copy to Clipboard\n" );
			push_notification( "COPY FAILED" );
		}
	}
}

