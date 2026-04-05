#include "main.h"

#include "imgui_internal.h"


namespace gallery
{
	// a sorted list of media entries, each item is an index to an entry in directory::media_list
	std::vector< size_t >         sorted_media{};

	char                          search[ 512 ];

	// cursor position/index in items
	size_t                        cursor            = 0;

	e_gallery_sort_mode           sort_mode         = e_gallery_sort_mode_date_mod_new_to_old;
	bool                          sort_mode_update  = false;

	u32                           row_count         = 0;
	u32                           item_size         = 150;
	u32                           item_size_min     = 70;
	u32                           item_size_max     = 500;
	bool                          item_size_changed = true;
	std::vector< ImVec2 >         item_text_size;

	u32                           image_size         = item_size;

	bool                          sidebar_draw       = true;

	bool                          scroll_to_cursor   = false;

	// Files selected in the gallery view
	std::vector< u32 >            selection{};
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


const media_entry_t& gallery_item_get_media_entry( size_t index )
{
	if ( index >= gallery::sorted_media.size() )
		return {};

	return directory::media_list[ gallery::sorted_media[ index ] ];
}


const file_t& gallery_item_get_file( size_t index )
{
	return gallery_item_get_media_entry( index ).file;
}


const fs::path& gallery_item_get_path( size_t index )
{
	return gallery_item_get_media_entry( index ).file.path;
}


std::string gallery_item_get_path_string( size_t index )
{
	return gallery_item_get_media_entry( index ).file.path.string();
}


// =============================================================================================


void gallery_view_input_check_clear_multi_select()
{
	if ( !ImGui::IsKeyDown( ImGuiKey_LeftShift ) && !ImGui::IsKeyDown( ImGuiKey_LeftCtrl ) )
	{
		if ( gallery::selection.size() )
		{
			gallery::selection.clear();
		}
	}
}


void gallery_view_input_update_multi_select()
{
	if ( ImGui::IsKeyDown( ImGuiKey_LeftShift ) )
	{
		gallery::selection.push_back( gallery::cursor );
	}
}


void gallery_view_input()
{
	// ctrl moves the cursor position, but not what is currently selected

	if ( ImGui::IsKeyDown( ImGuiKey_LeftShift ) )
	{
	}

	if ( ImGui::IsKeyPressed( ImGuiKey_LeftArrow ) )
	{
		gallery_view_input_check_clear_multi_select();

		if ( gallery::cursor == 0 )
			gallery::cursor = gallery::sorted_media.size();

		gallery::cursor--;
		gallery_view_scroll_to_cursor();
		gallery_view_input_update_multi_select();
	}
	else if ( ImGui::IsKeyPressed( ImGuiKey_RightArrow ) )
	{
		gallery::cursor = ( gallery::cursor + 1 ) % gallery::sorted_media.size();
		gallery_view_scroll_to_cursor();
		gallery_view_input_update_multi_select();
	}
	else if ( ImGui::IsKeyPressed( ImGuiKey_UpArrow ) )
	{
		if ( gallery::cursor < gallery::row_count )
		{
			size_t count_in_row   = gallery::sorted_media.size() % gallery::row_count;
			size_t missing_in_row = gallery::row_count - count_in_row;
			size_t row_diff       = gallery::row_count - gallery::cursor;

			// advance up a row
			if ( missing_in_row >= row_diff )
				row_diff += gallery::row_count;

			gallery::cursor = gallery::sorted_media.size() - ( row_diff - missing_in_row );
		}
		else
		{
			gallery::cursor = ( gallery::cursor - gallery::row_count ) % gallery::sorted_media.size();
		}

		gallery_view_scroll_to_cursor();
	}
	else if ( ImGui::IsKeyPressed( ImGuiKey_DownArrow ) )
	{
		if ( gallery::cursor + gallery::row_count >= gallery::sorted_media.size() )
		{
			size_t count_in_row  = gallery::sorted_media.size() % gallery::row_count;
			size_t row_pos      = gallery::cursor % gallery::row_count;
			gallery::cursor      = row_pos;
		}
		else
		{
			gallery::cursor = ( gallery::cursor + gallery::row_count ) % gallery::sorted_media.size();
		}

		gallery_view_scroll_to_cursor();
	}

	// shift adds to the selecction amount
	if ( ImGui::IsKeyDown( ImGuiKey_LeftShift ) )
	{
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

	size_t         selected_item_idx = gallery::cursor;
	media_entry_t  selected_item{};
	size_t         selected_item_gallery{};
	bool           selected_folder = false;

	if ( gallery::sorted_media.size() )
	{
		selected_item_gallery = gallery::sorted_media[ selected_item_idx ];
		selected_item         = gallery_item_get_media_entry( selected_item_idx );
		selected_folder       = selected_item.type == e_media_type_directory;
	}

	size_t search_len = strlen( gallery::search );

	// Split up lists
	for ( size_t i = 0; i < directory::media_list.size(); i++ )
	{
		//if ( g_do_search )
		if ( search_len )
		{
			media_entry_t& entry = directory::media_list[ i ];
			if ( entry.filename.find( gallery::search, search_len ) == std::string::npos )
				continue;
		}

		if ( directory::media_list[ i ].type == e_media_type_directory )
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

	// Find Selected File
	if ( !selected_item.file.path.empty() )
	{
		for ( size_t i = 0; i < gallery::sorted_media.size(); i++ )
		{
			const file_t& file = gallery_item_get_file( i );

			if ( selected_item.file != file )
				continue;

			gallery::cursor = i;
			gallery_view_scroll_to_cursor();
			break;
		}
	}

	//if ( g_do_search )
	//	g_do_search = false;

	update_window_title();
}


// =============================================================================================


void gallery_view_dir_change()
{
	gallery_view_update_header_directory();

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

	if ( !ImGui::BeginPopupContextVoid( "##gallery ctx menu", ImGuiPopupFlags_AnyPopup ) )
	 	return;

	ImGuiStyle& style        = ImGui::GetStyle();
	ImVec2      region_avail = ImGui::GetContentRegionAvail();

	if ( ImGui::MenuItem( "Open File Location", nullptr, false, g_image_data.textures.count ) )
	{
		sys_browse_to_file( gallery_item_get_path_string( gallery::cursor ).c_str() );
		//ctx_open = false;
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

	if ( ImGui::MenuItem( "File Info", nullptr, false, false ) )
	{
	}

	if ( ImGui::MenuItem( "File Properties", nullptr, false, 0 ) )
	{
		// TODO: create our own imgui file properties for more info
		// Plat_OpenFileProperties( ImageView_GetImagePath() );
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
	gallery::item_size_changed = true;
	gallery_view_scroll_to_cursor();

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
	image_size.x    = image->width * zoom_level;
	image_size.y    = image->height * zoom_level;

	if ( upscale )
		out_image_size = image_size;

	// center the image
	ImVec2 image_offset = ImGui::GetCursorPos();
	image_offset.x += ( image_bounds.x - image_size.x ) / 2;
	image_offset.y += ( image_bounds.y - image_size.y ) / 2;

	ImGui::SetCursorPos( image_offset );

	ImGui::Image( im_texture, image_size );
}


// called when file is double clicked or enter is pressed on it
void gallery_selected_item_action( const media_entry_t& media )
{
	if ( media.type == e_media_type_directory )
	{
		directory::queued = media.file.path;
	}
	else
	{
		set_view_type_media();
	}
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

	bool   content_area_hovered = false;

	{
		ImVec2 cursor_screen_pos = ImGui::GetCursorScreenPos();
		content_area_hovered     = ImGui::IsMouseHoveringRect(
			cursor_screen_pos,
			{ cursor_screen_pos.x + region_avail.x + style.WindowPadding.x,
			region_avail.y + style.WindowPadding.y } );

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

	gallery::row_count = (int)( region_avail.x - style.ScrollbarSize ) / ( gallery::item_size );

	float item_size_x  = gallery::item_size - style.ItemSpacing.x;
	float item_size_y  = item_size_x;

	if ( app::config.gallery_show_filenames )
		item_size_y += ImGui::GetFontSize() + style.ItemSpacing.y;

	int    grid_pos_x          = 0;
	size_t i                   = 0;

	int    image_visible_count = 0;

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
			app::draw_frame      = true;
			app::draw_next_frame = true;
		}

		if ( app::window_resized )
		{
			float scroll_diff = fmod( scroll, scroll_amount );

			if ( scroll_diff > 0 )
				scroll -= scroll_diff;
		}

		ImGui::SetScrollY( scroll );
	}

	// ----------------------------------------------------------------------------------------------------------

	ImVec2 image_bounds                 = { item_size_x - ( style.WindowPadding.x * 2 ), item_size_x - ( style.WindowPadding.x * 2 ) };
	gallery::image_size                 = image_bounds.x;

	ImVec2        image_icon_bounds     = { image_bounds.x / 4.f, image_bounds.y / 4.f };

	ImVec2        last_cursor_pos       = ImGui::GetCursorPos();
	float         last_grid_row_y       = ImGui::GetCursorPos().y;

	ImDrawList*   draw_list             = ImGui::GetWindowDrawList();

	float         last_max_item_height  = item_size_y;

	ImVec2        controlled_cursor_pos = ImGui::GetCursorPos();

	static size_t last_hovered          = SIZE_MAX;
	static size_t last_selected         = SIZE_MAX;
	bool          any_item_hovered      = false;

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
			ImGui::SetCursorPosX( ImGui::GetCursorPosX() );
			ImGui::SetCursorPosY( last_grid_row_y + last_max_item_height + style.ItemSpacing.y );

			grid_pos_x           = 0;
			last_max_item_height = item_size_y;
		}
		else if ( grid_pos_x > 0 )
		{
			ImGui::SameLine();
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
		if ( gallery::cursor == i && gallery::scroll_to_cursor )
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

			app::draw_frame      = true;
			app::draw_next_frame = true;
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

		ImVec2 window_pos = ImGui::GetWindowPos();

		ImVec2 window_cursor_pos( window_pos.x + cursor_pos.x, ( window_pos.y + cursor_pos.y ) - ImGui::GetScrollY() );
		ImVec2 global_item_size = ImVec2( window_cursor_pos.x + item_size_x, window_cursor_pos.y + current_item_size_y );

		bool   item_hovered     = false;
		bool   selected_item    = gallery::cursor == i;

		if ( content_area_hovered )
			item_hovered = ImGui::IsMouseHoveringRect( cursor_screen_pos, { cursor_screen_pos.x + item_size_x, cursor_screen_pos.y + current_item_size_y } );

		any_item_hovered |= item_hovered;

		if ( selected_item && !item_hovered && i == last_hovered )
		{
			app::draw_frame = true;
		}

		if ( item_hovered && i != last_hovered )
		{
			last_hovered = i;
			app::draw_frame = true;
		}

		if ( selected_item && i != last_selected )
		{
			last_selected   = i;
			app::draw_frame = true;
		}

		// Draw a background if needed
		if ( selected_item || item_hovered )
		{
			// why is this not using Active color?
			ImColor main_bg_color     = item_hovered ? style.Colors[ ImGuiCol_FrameBgHovered ] : style.Colors[ ImGuiCol_FrameBg ];
			ImColor main_border_color = style.Colors[ ImGuiCol_Border ];

			draw_list->AddRectFilled( window_cursor_pos, global_item_size, main_bg_color, style.ChildRounding, ImDrawFlags_RoundCornersAll );

			// if ( style.FrameBorderSize )
				draw_list->AddRect( window_cursor_pos, global_item_size, main_border_color, style.ChildRounding, ImDrawFlags_RoundCornersAll );
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

		if ( media.type == e_media_type_directory )
		{
			gallery_view_draw_image( icon_get_image( e_icon_folder ), icon_get_imtexture( e_icon_folder ), image_bounds, false, scaled_image_size );
		}
		// videos don't have thumbnail generation yet
		// else if ( media.type == e_media_type_video )
		// {
		// 	gallery_view_draw_image( icon_get_image( e_icon_video ), icon_get_imtexture( e_icon_video ), image_bounds, false );
		// }
		else
		{
			thumbnail_t* thumbnail = thumbnail_get_data( directory::thumbnail_list[ gallery_index ] );

			if ( thumbnail )
			{
				if ( thumbnail->status == e_thumbnail_status_finished )
				{
					gallery_view_draw_image( thumbnail->image, thumbnail->im_texture, image_bounds, true, scaled_image_size );
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

					ImGui::Dummy( image_bounds );
				}
				else  // if ( thumbnail->status == e_thumbnail_status_free )
				{
					ImGui::Dummy( image_bounds );
				}
			}
			else
			{
				if ( !thumbnail && media.type != e_media_type_directory )
					thumbnail_requests.emplace_back( media, gallery_index );
				// directory::thumbnail_list[ i ] = thumbnail_queue_image( entry );

				ImGui::Dummy( image_bounds );
			}
		}

		// ----------------------------------------------------------------------------------------------------------
		// Draw icon on top of it in the bottom right corner

		if ( media.type == e_media_type_video )
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

		if ( item_hovered && ImGui::IsMouseClicked( ImGuiMouseButton_Left ) )
		{
			gallery::cursor = i;

			if ( ImGui::IsMouseDoubleClicked( ImGuiMouseButton_Left ) )
			{
				gallery_selected_item_action( media );
			}
		}

		if ( selected_item && !ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed( ImGuiKey_Enter, false ) )
		{
			gallery_selected_item_action( media );
		}

		grid_pos_x++;
	}

	ImGui::EndChild();

	if ( !any_item_hovered && last_hovered != SIZE_MAX )
	{
		last_hovered    = SIZE_MAX;
		app::draw_frame = true;
	}

	// ----------------------------------------------------------------------------------------------------------
	
	// printf( "IMAGE COUNT: %d\n", image_visible_count );

	for ( size_t i = 0; i < thumbnail_requests.size(); i++ )
		directory::thumbnail_list[ thumbnail_requests[ i ].index ] = thumbnail_loader_queue_push( thumbnail_requests[ i ].media );

	gallery::scroll_to_cursor        = false;
	gallery::item_size_changed = false;

	// ImGui::PopStyleColor();
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

	// gallery_view_context_menu();

	ImGui::End();

	if ( ImGui::GetIO().WantTextInput )
		return;

	// TODO: Test ImGui::Shortcut()
	if ( app::window_focused && ImGui::IsKeyDown( ImGuiKey_LeftCtrl ) && ImGui::IsKeyPressed( ImGuiKey_C, false ) )
	{
		sys_copy_to_clipboard( gallery_item_get_path_string( gallery::cursor ).data() );
		printf( "Copied to Clipboard\n" );
		push_notification( "Copied" );
	}
}

