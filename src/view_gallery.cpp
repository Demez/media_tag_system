#include "main.h"

#include "imgui_internal.h"


namespace gallery
{
	// a sorted list of media entries, each item is an index to an entry in directory::media_list
	std::vector< size_t >         sorted_media{};

	char                          search[ 512 ];

	// cursor position/index in items
	// size_t                        cursor            = 0;

	e_gallery_sort_mode           sort_mode         = e_gallery_sort_mode_date_mod_new_to_old;
	bool                          sort_mode_update  = false;

	u32                           row_count          = 0;
	u32                           item_size          = 150;
	u32                           item_size_min      = 70;
	u32                           item_size_max      = 600;
	bool                          item_size_changed  = true;
	bool                          item_size_changing = false;
	std::vector< ImVec2 >         item_text_size;

	u32                           image_size         = item_size;

	bool                          sidebar_draw       = true;

	bool                          scroll_to_cursor   = false;

	u32                           drawn_image_count  = 0;
	u32                           first_visible_item = 0;

	// Quick Filter
	e_gallery_filter              filter{};

	// Files selected in the gallery view
	std::vector< selection_t >    selection{};

	// used for memory with media advancing with arrow keys
	selection_t                   last_selection{};
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


extern bool          g_do_search;


int                  gallery_view_draw_header();
void                 gallery_view_update_header_directory();

void                 gallery_view_draw_sidebar();
void                 sidebar_draw_filesystem();


// =============================================================================================


static const media_entry_t __media_entry_empty{};


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


const fs::path& gallery_item_get_path( size_t index )
{
	const media_entry_t& entry = gallery_item_get_media_entry( index );
	return entry.file.path;
}


std::string gallery_item_get_path_string( size_t index )
{
	const media_entry_t& entry = gallery_item_get_media_entry( index );
	return sys_path_to_string( entry.file.path );
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


void gallery_view_input_update_multi_select( u32 index, bool readd = true )
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

	gallery::last_selection.index = 0;
	gallery::last_selection.entry.filename.clear();
	gallery::last_selection.entry.type              = e_media_type_none;
	gallery::last_selection.entry.file.size         = 0;
	gallery::last_selection.entry.file.date_mod     = 0;
	gallery::last_selection.entry.file.date_created = 0;
	gallery::last_selection.entry.file.type         = e_file_type_invalid;
	gallery::last_selection.entry.file.path.clear();
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
			selection = gallery::sorted_media.size() - 1;

		gallery_view_scroll_to_cursor();
		gallery_view_input_update_multi_select( selection );
	}
	else if ( ImGui::IsKeyPressed( ImGuiKey_LeftArrow ) )
	{
		gallery_view_input_check_clear_multi_select();

		if ( !empty )
		{
			if ( selection == 0 )
				selection = gallery::sorted_media.size();

			selection--;
		}

		gallery_view_scroll_to_cursor();
		gallery_view_input_update_multi_select( selection );
	}
	else if ( ImGui::IsKeyPressed( ImGuiKey_RightArrow ) )
	{
		gallery_view_input_check_clear_multi_select();

		if ( !empty )
			selection = ( selection + 1 ) % gallery::sorted_media.size();

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
				size_t count_in_row   = gallery::sorted_media.size() % gallery::row_count;
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


void gallery_view_sort_list( std::vector< size_t >& gallery_list )
{
	// Sort data
	switch ( gallery::sort_mode )
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


void gallery_view_sort_dir()
{
	static std::vector< size_t > folders;
	static std::vector< size_t > files;

	folders.clear();
	files.clear();

	folders.reserve( directory::media_list.size() );
	files.reserve( directory::media_list.size() );

	size_t search_len = strlen( gallery::search );

	// Split up lists
	for ( size_t i = 0; i < directory::media_list.size(); i++ )
	{
		e_media_type type = directory::media_list[ i ].type;

		if ( gallery::filter )
		{
			if ( type == e_media_type_directory && !( gallery::filter & e_gallery_filter_folders ) )
				continue;

			if ( type == e_media_type_image && !( gallery::filter & e_gallery_filter_images ) )
				continue;

			if ( type == e_media_type_video && !( gallery::filter & e_gallery_filter_videos ) )
				continue;
		}

		//if ( g_do_search )
		if ( search_len )
		{
			media_entry_t& entry = directory::media_list[ i ];
			char*          find  = SDL_strcasestr( entry.filename.c_str(), gallery::search );

			if ( !find )
				continue;
		}

		if ( type == e_media_type_directory )
			folders.push_back( i );
		else
			files.push_back( i );
	}

	// Sort data
	if ( gallery::sort_mode != e_gallery_sort_mode_size_large_to_small && gallery::sort_mode != gallery::sort_mode )
		gallery_view_sort_list( folders );

	gallery_view_sort_list( files );

	gallery::sorted_media.clear();
	gallery::sorted_media.resize( folders.size() + files.size() );

	// Add Folders First
	std::copy( folders.begin(), folders.end(), gallery::sorted_media.begin() );

	// Add Files next
	std::copy( files.begin(), files.end(), gallery::sorted_media.begin() + folders.size() );
	
#if 1
	// Find Selected File
	if ( !gallery::selection.empty() )
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
#endif

	gallery_view_scroll_to_cursor();

	gallery_view_reset_text_size();

	update_window_title();
}


// =============================================================================================


// called when file is double clicked or enter is pressed on it
void gallery_selected_item_action( const media_entry_t& media, u32 index )
{
	if ( media.type == e_media_type_directory )
	{
		directory::queued = media.file.path;
	}
	else
	{
		g_image_data.index = index;
		set_view_type_media();
	}
}


void gallery_view_dir_change( bool keep_selection )
{
	gallery_view_update_header_directory();

	//if ( keep_selection )
	//	gallery_view_set_selection( gallery::cursor );

	// TODO: make it work with recursive, so if the selected item is still within the results, use that to snap scroll view to
	if ( !directory::folder_reload || directory::recursive )
		gallery::sorted_media.clear();

	// SORT FILE LIST
	gallery_view_sort_dir();

	// Invalidate These
	g_image_data.index        = SIZE_MAX;
	g_image_scaled_data.index = SIZE_MAX;
}


void gallery_view_scroll_to_cursor()
{
	gallery::scroll_to_cursor = true;
}


// doesn't want to work
void gallery_view_context_menu()
{
	//static bool ctx_open = false;
	//if ( !ctx_open  && !ImGui::IsMouseClicked( ImGuiMouseButton_Right ) )
	//	return;

	//ctx_open = true;

	// if ( !ImGui::BeginPopup( "##gallery ctx menu" ) )
	// 	return;

	if ( !ImGui::BeginPopupContextWindow( "##gallery ctx menu", ImGuiPopupFlags_AnyPopup | ImGuiPopupFlags_MouseButtonRight ) )
	 	return;

	ImGuiStyle&   style               = ImGui::GetStyle();
	ImVec2        region_avail        = ImGui::GetContentRegionAvail();

	u32           last_selected       = gallery_view_get_last_selected_index( UINT32_MAX );
	media_entry_t media_entry         = gallery_view_get_last_selected_entry();

	bool          folder              = last_selected == UINT32_MAX;

	// make sure we have at least ONE image here, or this gets stuck and hangs forever lol
	bool          valid               = false;
	for ( size_t i : gallery::sorted_media )
	{
		const media_entry_t& entry = directory::media_list[ i ];

		if ( entry.type == e_media_type_image || entry.type == e_media_type_video )
		{
			valid = true;
			break;
		}
	}

	if ( ImGui::MenuItem( "View", nullptr, false, valid ) )
	{

		if ( valid )
			gallery_selected_item_action( media_entry, folder ? 0 : last_selected );
	}

	ImGui::Separator();

	if ( ImGui::MenuItem( folder ? "Open Folder" : "Open File Location", nullptr, false, true ) )
	{
		if ( folder )
		{
			// Open folder
			sys_browse_to_path( directory::path );
		}
		else
		{
			// Open folder and select files
			std::vector< fs::path > paths;
			fs::path                base_path = directory::path;

			if ( directory::recursive )
				base_path = media_entry.file.path.parent_path();

			for ( selection_t& selection : gallery::selection )
			{
				if ( directory::recursive )
				{
					fs::path::string_type base_path_str   = base_path.native();
					fs::path::string_type path_str        = selection.entry.file.path.native();
					fs::path::string_type path_parent_str = selection.entry.file.path.parent_path().native();

					if ( base_path == path_parent_str )
						paths.push_back( selection.entry.file.path );
				}
				else
				{
					paths.push_back( selection.entry.file.path );
				}
			}

			sys_browse_to_files( base_path, paths );
		}
	}

	if ( ImGui::BeginMenu( "Open With" ) )
	{
		// TODO: list programs to open the file with, like fragment image viewer
		// how would this work on linux actually? hmm
		ImGui::MenuItem( "nothing lol", nullptr, false, false );
		ImGui::EndMenu();
	}

	if ( ImGui::MenuItem( "Copy Image", nullptr, false, false ) )
	{
	}

	if ( ImGui::MenuItem( "Copy Image Data", nullptr, false, false ) )
	{
	}

	if ( ImGui::MenuItem( "Set As Desktop Background", nullptr, false, false ) )
	{
	}

	if ( ImGui::MenuItem( "Undo", nullptr, false, 0 ) )
	{
		//UndoSys_Undo();
	}

	if ( ImGui::MenuItem( "Redo", nullptr, false, 0 ) )
	{
		//UndoSys_Redo();
	}

	if ( ImGui::MenuItem( "Delete", nullptr, false, 0 ) )
	{
		//ImageView_DeleteImage();
	}

	if ( ImGui::MenuItem( folder ? "Folder Properties" : "File Properties" ) )
	{
		// TODO: create our own imgui file properties for more info
		if ( folder )
		{
			sys_open_file_properties( { directory::path } );
		}
		else
		{
			std::vector< fs::path > paths;

			for ( selection_t& selection : gallery::selection )
			{
				paths.push_back( selection.entry.file.path );
			}

			sys_open_file_properties( paths );
		}
	}

	if ( ImGui::MenuItem( "Reload Folder", nullptr, false ) )
	{
		folder_load_media_list();
		//ctx_open = false;
	}

	ImGui::Separator();

	if ( ImGui::MenuItem( "Settings", nullptr, false, false ) )
	{
	}

	ImGui::EndPopup();
}


void gallery_view_reset_text_size()
{
	gallery::item_size_changed  = true;
	// gallery_view_scroll_to_cursor();

	gallery::item_text_size.clear();
	gallery::item_text_size.resize( gallery::sorted_media.size() );
}


void gallery_view_draw_image( image_t* image, ImTextureRef im_texture, ImVec2 image_bounds, bool upscale, ImVec2& out_image_size )
{
	// Fit image in window size, scaling up if needed
	float factor[ 2 ] = { 1.f, 1.f };

	if ( upscale || image->width > image_bounds.x )
		factor[ 0 ] = (float)image_bounds.x / (float)image->width;

	if ( upscale || image->height > image_bounds.y )
		factor[ 1 ] = (float)image_bounds.y / (float)image->height;

	float zoom_level = std::min( factor[ 0 ], factor[ 1 ] );

	ImVec2 image_size{};
	image_size.x    = int( image->width * zoom_level );
	image_size.y    = int( image->height * zoom_level );

	if ( upscale )
		out_image_size = image_size;

	// center the image
	ImVec2 image_offset = ImGui::GetCursorPos();
	image_offset.x += int( ( image_bounds.x - image_size.x ) / 2 );
	image_offset.y += int( ( image_bounds.y - image_size.y ) / 2 );

	ImGui::SetCursorPos( image_offset );

	glBindTexture( GL_TEXTURE_2D, (GLuint)im_texture.GetTexID() );

	 // upscaling image
	if ( zoom_level > 2.f )
	{
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	}
	else
	{
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	}

	ImGui::Image( im_texture, image_size );
}


void gallery_view_draw_content()
{
	int window_width, window_height;
	SDL_GetWindowSize( app::window, &window_width, &window_height );

	ImGuiStyle& style              = ImGui::GetStyle();
	ImVec2      content_cursor_pos = ImGui::GetCursorPos();
	ImVec2      mouse_pos          = ImGui::GetMousePos();

	ImGui::SetCursorPosX( std::max( 0.f, content_cursor_pos.x - style.ItemSpacing.x ) );

	ImVec2      region_avail       = ImGui::GetContentRegionAvail();

	// weirdly sized still
	// ImGui::SetNextWindowPos( { window_width - region_avail_true, 32.f } );
	// ImGui::SetNextWindowSize( { (float)window_width, region_avail.y + style.ItemSpacing.y } );
	// ImGui::SetNextWindowSize( { region_avail.x + style.WindowPadding.x, region_avail.y + style.ItemSpacing.y } );
	ImGui::SetNextWindowSize( { region_avail.x + style.WindowPadding.x, region_avail.y + style.WindowPadding.y } );

	// ImVec4 bg_color = style.Colors[ ImGuiCol_ChildBg ];
	// bg_color.x      = 0.f;
	// bg_color.y      = 0.f;
	// bg_color.z      = 0.f;
	// bg_color.w      = 0.f;

	// ImGui::PushStyleColor( ImGuiCol_ChildBg, app::config.media_bg_color );

	// if ( !ImGui::BeginChild( "##gallery_content", { region_avail.x + style.WindowPadding.x, region_avail.y }, ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollWithMouse ) )
	if ( !ImGui::BeginChild( "##gallery_content", {}, ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBringToFrontOnFocus ) )
	{
		ImGui::EndChild();
		// ImGui::PopStyleColor();
		return;
	}

	if ( gallery::sorted_media.empty() )
	{
		ImGui::EndChild();
		return;
	}

	bool content_area_hovered = false;

	{
		ImVec2 cursor_screen_pos = ImGui::GetCursorScreenPos();
		content_area_hovered     = ImGui::IsMouseHoveringRect(
          cursor_screen_pos,
          { cursor_screen_pos.x + region_avail.x + style.WindowPadding.x,
		        window_height + style.WindowPadding.y } );

		if ( content_area_hovered && ImGui::IsPopupOpen( "", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel ) )
		{
			bool          hovered_popup = false;

			ImGuiContext& g = *GImGui;

			for ( int i = 0; i < g.OpenPopupStack.Size; i++ )
			{
				ImGuiPopupData& data   = g.OpenPopupStack[ i ];
				ImGuiWindow*    window = data.Window;

				hovered_popup          = ImGui::IsMouseHoveringRect( window->Pos, { window->Pos.x + window->Size.x, window->Pos.y + window->Size.y } );

				if ( hovered_popup )
				{
					content_area_hovered = false;
					break;
				}
			}
		}
	}
	
	//ImGui::SetCursorPosY( ImGui::GetCursorPosY() + style.ItemSpacing.y );

	// calc row count

	// ScrollToBringRectIntoView

	int        region_x       = region_avail.x - ( style.ScrollbarSize + style.WindowPadding.x );

	static u32 last_row_count = 0;
	last_row_count = gallery::row_count;

	gallery::row_count        = std::max( 1U, region_x / u32( gallery::item_size + style.ItemSpacing.x ) );

	if ( last_row_count != gallery::row_count )
		gallery_view_scroll_to_cursor();
		//printf( "row count change\n" );

	// float item_size_x  = gallery::item_size - style.ItemSpacing.x;
	float item_size_x  = gallery::item_size;
	float item_size_y  = item_size_x;

	if ( app::config.gallery_show_filenames )
		item_size_y += ImGui::GetFontSize() + style.ItemSpacing.y;

	float  item_spacing_x      = 0.f;

	if ( gallery::row_count > 2 )
	{
		item_spacing_x = ( ( region_x ) - ( item_size_x * gallery::row_count ) ) / ( gallery::row_count - 1 );
	}
	else
	{
		item_spacing_x = ( ( region_x ) - ( item_size_x * gallery::row_count ) ) / ( gallery::row_count + 1 );
	}

	// item_spacing_x             = std::max( style.ItemSpacing.x, item_spacing_x );

	int grid_pos_x          = 0;
	int image_visible_count = 0;

	struct delayed_load_t
	{
		media_entry_t media;
		size_t        index;
	};

	static std::vector< delayed_load_t > thumbnail_requests;
	thumbnail_requests.clear();

	// ----------------------------------------------------------------------------------------------------------
	// Do Scrolling

	if ( content_area_hovered )
	{
		float scroll = ImGui::GetScrollY();
		float scroll_amount = item_size_y + style.ItemSpacing.y;
		
		if ( app::mouse_scroll != 0 )
		{
			scroll -= scroll_amount * app::mouse_scroll;
			set_frame_draw( 2 );
		}

		ImGui::SetScrollY( scroll );
	}

	ImGuiWindow* window                       = ImGui::GetCurrentWindow();
	ImGuiID      active_id                    = ImGui::GetActiveID();
	// bool         scrollbar_active             = active_id && ( active_id == ImGui::GetWindowScrollbarID( window, ImGuiAxis_X ) || active_id == ImGui::GetWindowScrollbarID( window, ImGuiAxis_Y ) );
	bool         scrollbar_active             = active_id && active_id == ImGui::GetWindowScrollbarID( window, ImGuiAxis_Y );

	static bool  scrollbar_active_last_frame  = scrollbar_active;

	// ----------------------------------------------------------------------------------------------------------

	ImVec2 image_bounds                 = { item_size_x - ( style.WindowPadding.x * 2 ), item_size_x - ( style.WindowPadding.x * 2 ) };
	gallery::image_size                 = image_bounds.x;

	ImVec2        image_icon_bounds     = { image_bounds.x / 4.f, image_bounds.y / 4.f };

	ImVec2        last_cursor_pos       = ImGui::GetCursorPos();
	float         last_grid_row_y       = ImGui::GetCursorPos().y;

	ImDrawList*   draw_list             = ImGui::GetWindowDrawList();

	float         last_max_item_height  = item_size_y;

	static size_t last_hovered          = SIZE_MAX;
	static size_t last_selected         = SIZE_MAX;
	bool          any_item_hovered      = false;

	if ( gallery::row_count <= 2 )
		ImGui::SetCursorPosX( ImGui::GetCursorPosX() + item_spacing_x );

	bool               scroll_queued        = false;
	bool               row_count_changed    = last_row_count != gallery::row_count;
	static bool        filenames_shown_last = app::config.gallery_show_filenames;

	static h_thumbnail icons_scaled[ e_icon_count ]{};

	gallery::drawn_image_count = 0;
	u32 first_visible_item     = gallery::first_visible_item;

	bool keep_scroll_pos        = gallery::item_size_changing || ( filenames_shown_last != app::config.gallery_show_filenames );
	bool lock_visible_item      = keep_scroll_pos || app::window_resized;

	keep_scroll_pos |= ( app::window_resized && row_count_changed );

	// ----------------------------------------------------------------------------------------------------------

	for ( size_t i = 0; i < gallery::sorted_media.size(); i++ )
	{
		size_t               gallery_index = gallery::sorted_media[ i ];
		const media_entry_t& media         = directory::media_list[ gallery_index ];

		float                scroll        = ImGui::GetScrollY();

		ImVec2               media_text_size{};

		if ( app::config.gallery_show_filenames )
		{
			if ( gallery::item_size_changed )
			{
				media_text_size = ImGui::CalcTextSize( media.filename.c_str(), 0, false, item_size_x - ( style.WindowPadding.x * 2 ) );
				gallery::item_text_size[ i ] = media_text_size;
			}
			else
			{
				media_text_size = gallery::item_text_size[ i ];
			}
		}

		float current_item_size_y = image_bounds.y + ( style.WindowPadding.y * 2 );

		if ( app::config.gallery_show_filenames )
			current_item_size_y += media_text_size.y + style.ItemSpacing.y;

		current_item_size_y = std::min( current_item_size_y, item_size_y * 1.75f );

		if ( current_item_size_y > last_max_item_height )
			last_max_item_height = current_item_size_y;

		if ( grid_pos_x == gallery::row_count )
		{
			if ( gallery::row_count <= 2 )
				ImGui::SetCursorPosX( ImGui::GetCursorPosX() + item_spacing_x );
			else
				ImGui::SetCursorPosX( ImGui::GetCursorPosX() );

			ImGui::SetCursorPosY( last_grid_row_y + last_max_item_height + style.ItemSpacing.y );

			grid_pos_x           = 0;
			last_max_item_height = item_size_y;
		}
		else if ( grid_pos_x > 0 )
		{
			ImGui::SameLine( 0.f, 0.f );
			ImGui::SetCursorPosX( ImGui::GetCursorPosX() + item_spacing_x );
		}

		ImVec2 cursor_pos = ImGui::GetCursorPos();
		last_cursor_pos   = cursor_pos;
		last_grid_row_y   = cursor_pos.y;

		// ----------------------------------------------------------------------------------------------------------
		// Calculate Distance the Item is from visible scroll area
		{
			u32    distance       = 0;
			float  visible_top    = scroll;
			float  visible_bottom = visible_top + ImGui::GetWindowHeight();

			// check if the bottom of the item is still visible at the top of the content window
			if ( cursor_pos.y + current_item_size_y < visible_top )
				distance = visible_top - ( cursor_pos.y + current_item_size_y );

			// check if the top of the item is still visible at the bottom of the content window
			else if ( cursor_pos.y > visible_bottom )
				distance = cursor_pos.y - visible_bottom;

			// if distance is still 0, this item is at least partially on-screen
			thumbnail_update_distance( directory::thumbnail_list[ gallery_index ], distance );
		}

		// ----------------------------------------------------------------------------------------------------------
		// If we need to scroll to the selected item this frame
		// adjust the scroll position as needed to keep it on screen

		u32  last_selected     = gallery_view_get_last_selected_index();
		// bool selection_empty = gallery_view_selection_cleared();

		u32  scroll_to_index   = UINT32_MAX;

		if ( gallery::selection.size() )
		{
			scroll_to_index = last_selected;
		}
		else if ( keep_scroll_pos )
		{
			scroll_to_index = first_visible_item;
		}
		else if ( directory::folder_changed )
		{
			// scroll to top
			ImGui::SetScrollY( 0 );
			//gallery::scroll_to_cursor = false;
			set_frame_draw( 2 );
		}

		// if ( gallery::selection.size() && last_selected == i && gallery::scroll_to_cursor )
		if ( !scrollbar_active_last_frame && scroll_to_index == i && gallery::scroll_to_cursor )
		// if ( gallery::last_selection.entry.type != e_media_type_none && cache_last_selected == i && gallery::scroll_to_cursor )
		{
			bool   scroll_needed  = false;
			bool   scroll_up      = false;
			float  visible_top    = scroll;
			float  visible_bottom = visible_top + ImGui::GetWindowHeight();

			// check if the bottom of the item is off-screen at the bottom of the content window
			if ( cursor_pos.y + current_item_size_y > visible_bottom )
			{
				scroll_up     = false;
				scroll_needed = true;
			}

			// check if the top of the item is off-screen at the top of the content window
			else if ( cursor_pos.y < visible_top )
			{
				scroll_up     = true;
				scroll_needed = true;
			}

			if ( scroll_needed )
			{
				// calculate how much to scroll up or down
				float scroll_offset = 0;

				if ( scroll_up )
					scroll_offset = ( cursor_pos.y - style.ItemSpacing.y ) - visible_top;
				else
					scroll_offset = ( cursor_pos.y + current_item_size_y + style.ItemSpacing.y ) - visible_bottom;

				ImGui::SetScrollY( ImGui::GetScrollY() + scroll_offset );
			}

			gallery::scroll_to_cursor = false;
			set_frame_draw( 2 );
		}

		// ----------------------------------------------------------------------------------------------------------
		// is this item even visible?

		ImVec2 cursor_screen_pos = ImGui::GetCursorScreenPos();

		if ( !ImGui::IsRectVisible( cursor_screen_pos, { cursor_screen_pos.x + item_size_x, cursor_screen_pos.y + current_item_size_y } ) )
		{
			// use a dummy instead of a full child window, cheaper
			ImGui::Dummy( { item_size_x, current_item_size_y } );
			grid_pos_x++;
			continue;
		}

		// ----------------------------------------------------------------------------------------------------------
		// Item is Visible on screen, draw selection/hover background if needed

		// if ( gallery::first_visible_item == UINT32_MAX || last_row_count > gallery::row_count )
		// if ( gallery::first_visible_item == UINT32_MAX )
		if ( !lock_visible_item )
			gallery::first_visible_item = i;

		ImVec2 window_pos = ImGui::GetWindowPos();

		ImVec2 window_cursor_pos( window_pos.x + cursor_pos.x, ( window_pos.y + cursor_pos.y ) - ImGui::GetScrollY() );
		ImVec2 global_item_size = ImVec2( window_cursor_pos.x + item_size_x, window_cursor_pos.y + current_item_size_y );

		bool   item_hovered     = false;
		bool   selected_item    = false;

		for ( selection_t& selection : gallery::selection )
		{
			if ( selection.index != i )
				continue;

			selected_item = true;
			break;
		}

		if ( content_area_hovered )
			item_hovered = ImGui::IsMouseHoveringRect( cursor_screen_pos, { cursor_screen_pos.x + item_size_x, cursor_screen_pos.y + current_item_size_y } );

		any_item_hovered |= item_hovered;

		if ( selected_item && !item_hovered && i == last_hovered )
		{
			set_frame_draw();
		}

		if ( item_hovered && i != last_hovered )
		{
			last_hovered = i;
			set_frame_draw();
		}

		if ( selected_item && i != last_selected )
		{
			last_selected   = i;
			set_frame_draw();
		}

		// Draw a background if needed
		if ( selected_item || item_hovered )
		{
			// why is this not using Active color?
			ImColor color_base   = style.Colors[ ImGuiCol_FrameBg ];
			ImColor color_hover  = style.Colors[ ImGuiCol_FrameBgHovered ];
			ImColor color_active = style.Colors[ ImGuiCol_FrameBgActive ];
			ImColor color_border = style.Colors[ ImGuiCol_Border ];

			ImColor color        = color_base;

			if ( item_hovered )
				color = selected_item ? color_active : color_hover;

			draw_list->AddRectFilled( window_cursor_pos, global_item_size, color, style.ChildRounding, ImDrawFlags_RoundCornersAll );

			// if ( style.FrameBorderSize )
			draw_list->AddRect( window_cursor_pos, global_item_size, color_border, style.ChildRounding, ImDrawFlags_RoundCornersAll );
		}

		if ( !selected_item && gallery::last_selection.entry.type != e_media_type_none && gallery::last_selection.index == i )
		{
			// why is this not using Active color?
			// ImColor main_bg_color     = item_hovered ? style.Colors[ ImGuiCol_FrameBgHovered ] : style.Colors[ ImGuiCol_FrameBg ];
			ImColor main_bg_color     = style.Colors[ ImGuiCol_FrameBg ];
			draw_list->AddRect( window_cursor_pos, global_item_size, main_bg_color, style.ChildRounding, ImDrawFlags_RoundCornersAll );
		}

		ImVec2 current_pos = ImGui::GetCursorPos();
		ImVec2 saved_pos   = ImGui::GetCursorPos();

		current_pos.x += style.WindowPadding.x;
		current_pos.y += style.WindowPadding.y;
		ImGui::SetCursorPos( current_pos );

		image_visible_count++;

		// ----------------------------------------------------------------------------------------------------------
		// Draw Thumbnail or Icon

		ImVec2 scaled_image_size{};
		bool   drew_base_icon = false;

		if ( media.type == e_media_type_directory )
		{
			gallery_view_draw_image( icon_get_image( e_icon_folder ), icon_get_imtexture( e_icon_folder ), image_bounds, true, scaled_image_size );
		}
		else
		{
			thumbnail_t* thumbnail = thumbnail_get_data( directory::thumbnail_list[ gallery_index ] );

			e_icon       base_icon = e_icon_none;

			if ( media.type == e_media_type_directory )
				base_icon = e_icon_folder;
			else if ( media.type == e_media_type_image )
				base_icon = e_icon_image;
			else if ( media.type == e_media_type_video )
				base_icon = e_icon_video;

			if ( thumbnail )
			{
				if ( thumbnail->status == e_thumbnail_status_finished )
				{
					if ( thumbnail->image_scaled )
						gallery_view_draw_image( thumbnail->image_scaled, thumbnail->textures.frame[ 0 ], image_bounds, true, scaled_image_size );
					else
						gallery_view_draw_image( thumbnail->image, thumbnail->textures.frame[ 0 ], image_bounds, true, scaled_image_size );

					gallery::drawn_image_count++;
				}
				else if ( thumbnail->status == e_thumbnail_status_failed )
				{
					gallery_view_draw_image( icon_get_image( e_icon_invalid ), icon_get_imtexture( e_icon_invalid ), image_bounds, false, scaled_image_size );
				}
				else if ( thumbnail->status == e_thumbnail_status_queued || thumbnail->status == e_thumbnail_status_loading || thumbnail->status == e_thumbnail_status_uploading )
				{
					gallery_view_draw_image( icon_get_image( e_icon_loading ), icon_get_imtexture( e_icon_loading ), image_bounds, false, scaled_image_size );
				}
				else if ( thumbnail->status == e_thumbnail_status_free )
				{
					if ( media.type != e_media_type_directory )
						thumbnail_requests.emplace_back( media, gallery_index );

					// ImGui::Dummy( image_bounds );
					gallery_view_draw_image( icon_get_image( base_icon ), icon_get_imtexture( base_icon ), image_bounds, true, scaled_image_size );
					drew_base_icon = true;
				}
				else  // if ( thumbnail->status == e_thumbnail_status_free )
				{
					//ImGui::Dummy( image_bounds );
					gallery_view_draw_image( icon_get_image( base_icon ), icon_get_imtexture( base_icon ), image_bounds, true, scaled_image_size );
					drew_base_icon = true;
				}
			}
			else
			{
				if ( !thumbnail && media.type != e_media_type_directory )
					thumbnail_requests.emplace_back( media, gallery_index );
				// directory::thumbnail_list[ i ] = thumbnail_queue_image( entry );

				//ImGui::Dummy( image_bounds );
				gallery_view_draw_image( icon_get_image( base_icon ), icon_get_imtexture( base_icon ), image_bounds, true, scaled_image_size );
				drew_base_icon = true;
			}
		}

		// ----------------------------------------------------------------------------------------------------------
		// Draw icon on top of it in the bottom right corner

		if ( media.type == e_media_type_video && !drew_base_icon )
		{
			// Fit image in window size, scaling up if needed
			float    factor[ 2 ] = { 1.f, 1.f };

			image_t* icon_video  = icon_get_image( e_icon_video );

			//if ( image->width > image_bounds.x )
			factor[ 0 ]          = (float)image_icon_bounds.x / (float)icon_video->width;

			//if ( image->height > image_bounds.y )
			factor[ 1 ]          = (float)image_icon_bounds.y / (float)icon_video->height;

			float  zoom_level    = std::min( factor[ 0 ], factor[ 1 ] );

			ImVec2 scaled_icon_size;
			scaled_icon_size.x  = icon_video->width * zoom_level;
			scaled_icon_size.y  = icon_video->height * zoom_level;

			ImVec2 image_offset = saved_pos;
			float image_offset_from_side_x = 0.f;
			float image_offset_from_side_y = 0.f;

			if ( scaled_image_size.x )
			{
				image_offset_from_side_x = ( image_bounds.x - scaled_image_size.x ) / 2.f;
				image_offset_from_side_y = ( image_bounds.y - scaled_image_size.y ) / 2.f;
			}

#if 0
			image_offset.x += ( image_bounds.x - image_offset_from_side_x ) - ( scaled_icon_size.x / 2.f );
			image_offset.y += ( image_bounds.y - image_offset_from_side_y ) - ( scaled_icon_size.y / 2.f );
#else
			image_offset.x += ( image_bounds.x - image_offset_from_side_x ) - ( scaled_icon_size.x / 1.25f );
			image_offset.y += ( image_bounds.y - image_offset_from_side_y ) - ( scaled_icon_size.y / 1.25f );
#endif

			// ImVec2 image_offset = ImGui::GetCursorPos();
			// image_offset.x += image_bounds.x - ( scaled_icon_size.x / 1.25 );
			// image_offset.y -= ( scaled_icon_size.y / 1.25 ) + style.ItemSpacing.y;

			ImGui::SetCursorPos( image_offset );

			ImGui::Image( icon_get_imtexture( e_icon_video ), scaled_icon_size );
		}

		// ----------------------------------------------------------------------------------------------------------
		// Draw Text
		
		// center align text
		ImGui::SetCursorPosX( current_pos.x + ( ( gallery::item_size - ( media_text_size.x + style.WindowPadding.x * 2 + style.ItemSpacing.x ) ) * 0.5f ) );
		ImGui::SetCursorPosY( current_pos.y + image_bounds.x + style.ItemSpacing.y );

		ImGui::PushTextWrapPos( saved_pos.x + image_bounds.x + style.ItemSpacing.x );

		// Text Clipping
		{
			ImVec2  window_pos  = ImGui::GetWindowPos();
			ImVec2  current_screen_pos = ImGui::GetCursorScreenPos();

			ImVec2  text_clip_min( window_pos.x + cursor_pos.x, ( window_pos.y + cursor_pos.y + image_bounds.x + ( style.ItemSpacing.y * 2 ) ) - ImGui::GetScrollY() );
			ImVec2  text_clip_max = global_item_size;

			float   text_height   = text_clip_max.y - text_clip_min.y;
			float   font_height   = ImGui::GetFontSize();

			// float   result        = fmod( text_height, font_height );
			float   result        = floor( text_height / font_height );
			text_clip_max.y       = text_clip_min.y + ( result * font_height );

			ImGui::PushClipRect( text_clip_min, text_clip_max, true );

			// draw clipping box for debug if needed
			//ImColor clip_color = style.Colors[ ImGuiCol_Border ];
			//draw_list->AddRect( text_clip_min, text_clip_max, clip_color, 0, ImDrawFlags_None );
		}
		
		ImGui::TextUnformatted( media.filename.c_str() );
		
		ImGui::PopTextWrapPos();
		ImGui::PopClipRect();

		// ----------------------------------------------------------------------------------------------------------
		// Add Dummy Window
		
		//ImGui::SetCursorPos( post_dummy_pos );
		ImGui::SetCursorPos( saved_pos );
		ImGui::Dummy( { item_size_x, current_item_size_y } );
		
		// ----------------------------------------------------------------------------------------------------------
		// Handle Actions

		if ( selected_item && item_hovered )
		{
			SDL_MouseButtonFlags mouse_btns        = SDL_GetMouseState( nullptr, nullptr );

			// mouse down and not hovering an imgui window not in an image pan
			// bool        mouse_middle_down = ImGui::IsMouseDown( ImGuiMouseButton_Middle ) && !( mouse_hover_imgui_window );
			bool                 drag_button_down = ( mouse_btns & SDL_BUTTON_LMASK ) || ( mouse_btns & SDL_BUTTON_RMASK );

			static bool          drag_cooldown     = false;

			if ( drag_button_down )
			{
				if ( !drag_cooldown )
				{
					if ( app::mouse_delta[ 0 ] != 0.0 || app::mouse_delta[ 1 ] != 0.0 )
					{
						u32 button = 0;
						if ( mouse_btns & SDL_BUTTON_LMASK )
							button = SDL_BUTTON_LEFT;

						else if ( mouse_btns & SDL_BUTTON_RMASK )
							button = SDL_BUTTON_RIGHT;

						std::vector< fs::path > files{};

						for ( selection_t& selection : gallery::selection )
							files.push_back( selection.entry.file.path );

						sys_do_drag_drop_files( files, button );

						// this way we don't try to start another drag drop instantly after somehow
						drag_cooldown = true;
					}
				}
			}
			else
			{
				drag_cooldown = false;
			}
		}

		bool mouse_release = ( ImGui::IsMouseReleased( ImGuiMouseButton_Left ) || ImGui::IsMouseReleased( ImGuiMouseButton_Middle ) );
		bool mouse_press   = ( ImGui::IsMouseClicked( ImGuiMouseButton_Left ) || ImGui::IsMouseClicked( ImGuiMouseButton_Middle ) );

		if ( !(scrollbar_active || scrollbar_active_last_frame) && item_hovered )
		{
			// if ( mouse_release && gallery::selection.size() > 1 )
			if ( mouse_release )
			{
				// the item may be a bit out of frame, scroll a little to have it fully in view
				scroll_queued = true;

				if ( gallery_view_input_do_multi_select() )
				{
					// if we want multi select, remove or add the item from selection list
					gallery_view_input_update_multi_select( i, false );
				}
				else if ( selected_item )
				{
					// if the item is already selected, but we DONT want multi select, clear the selection list, add readd that the selected item
					gallery::selection.clear();
					gallery_view_input_update_multi_select( i, false );
				}
			}

			if ( mouse_press && !selected_item )
			{
				if ( !gallery_view_input_do_multi_select() )
				{
					gallery::selection.clear();
					gallery_view_input_update_multi_select( i, false );

					// the item may be a bit out of frame, scroll a little to have it fully in view
					scroll_queued = true;
				}
			}

			if ( ImGui::IsMouseDoubleClicked( ImGuiMouseButton_Left ) )
			{
				gallery_selected_item_action( media, i );
			}
		}

		grid_pos_x++;
	}

	ImVec2 end_window_pos      = ImGui::GetWindowPos();
	ImVec2 end_window_size     = ImGui::GetWindowSize();
	ImVec2 end_window_size_pos = { end_window_pos.x + end_window_size.x, end_window_pos.y + end_window_size.y };

	gallery_view_context_menu();

	ImGui::EndChild();

	if ( !any_item_hovered && last_hovered != SIZE_MAX )
	{
		last_hovered = SIZE_MAX;
		set_frame_draw();
	}

	// ----------------------------------------------------------------------------------------------------------

	// if ( ImGui::IsMouseHoveringRect( end_window_pos, end_window_size_pos ) && !any_item_hovered && content_area_hovered )
	if ( !any_item_hovered && content_area_hovered )
	// if ( ImGui::IsWindowHovered() && !any_item_hovered )
	{
		if ( ImGui::IsMouseClicked( ImGuiMouseButton_Left ) )
		{
			gallery_view_input_check_clear_multi_select();
		}
	}

	if ( ImGui::IsKeyDown( ImGuiKey_Escape ) )
	{
		// also clears last selection cache
		gallery_view_clear_selection();
	}

	if ( !ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed( ImGuiKey_Enter, false ) && gallery::sorted_media.size() )
	{
		if ( gallery::last_selection.entry.type != e_media_type_none )
		{
			gallery_selected_item_action( gallery::last_selection.entry, gallery::last_selection.index );
		}
		else
		{
			const media_entry_t& entry = gallery_item_get_media_entry( 0 );
			gallery_selected_item_action( entry, 0 );
		}
	}
	
	for ( size_t i = 0; i < thumbnail_requests.size(); i++ )
		directory::thumbnail_list[ thumbnail_requests[ i ].index ] = thumbnail_loader_queue_push( thumbnail_requests[ i ].media );

	gallery::scroll_to_cursor   = scroll_queued;
	gallery::item_size_changed  = false;
	filenames_shown_last        = app::config.gallery_show_filenames;
	scrollbar_active_last_frame = scrollbar_active;
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

	gallery_view_draw_content();

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
			files.push_back( selection.entry.file.path );

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

