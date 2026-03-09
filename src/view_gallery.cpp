#include "main.h"

#include "imgui_internal.h"


namespace gallery
{
	// a sorted list of items
	std::vector< gallery_item_t > items{};

	// cursor position/index in items
	size_t                        cursor            = 0;

	e_gallery_sort_mode           sort_mode         = e_gallery_sort_mode_date_mod_new_to_old;
	bool                          sort_mode_update  = false;

	int                           row_count         = 0;
	int                           item_size         = 150;
	int                           item_size_min     = 50;
	int                           item_size_max     = 600;
	bool                          item_size_changed = true;
	std::vector< ImVec2 >         item_text_size;

	int                           image_size         = item_size;

	bool                          sidebar_draw       = true;

	bool                          scroll_to_cursor   = false;

	// Files selected in the gallery view
	std::vector< u32 >            selection{};
}


void gallery_view_draw_header();
void gallery_view_update_header_directory();

void gallery_view_draw_sidebar();
void sidebar_draw_filesystem();


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
			gallery::cursor = gallery::items.size();

		gallery::cursor--;
		gallery_view_scroll_to_cursor();
		gallery_view_input_update_multi_select();
	}
	else if ( ImGui::IsKeyPressed( ImGuiKey_RightArrow ) )
	{
		gallery::cursor = ( gallery::cursor + 1 ) % gallery::items.size();
		gallery_view_scroll_to_cursor();
		gallery_view_input_update_multi_select();
	}
	else if ( ImGui::IsKeyPressed( ImGuiKey_UpArrow ) )
	{
		if ( gallery::cursor < gallery::row_count )
		{
			size_t count_in_row   = gallery::items.size() % gallery::row_count;
			size_t missing_in_row = gallery::row_count - count_in_row;
			size_t row_diff       = gallery::row_count - gallery::cursor;

			// advance up a row
			if ( missing_in_row >= row_diff )
				row_diff += gallery::row_count;

			gallery::cursor = gallery::items.size() - ( row_diff - missing_in_row );
		}
		else
		{
			gallery::cursor = ( gallery::cursor - gallery::row_count ) % gallery::items.size();
		}

		gallery_view_scroll_to_cursor();
	}
	else if ( ImGui::IsKeyPressed( ImGuiKey_DownArrow ) )
	{
		if ( gallery::cursor + gallery::row_count >= gallery::items.size() )
		{
			size_t count_in_row  = gallery::items.size() % gallery::row_count;
			size_t row_pos      = gallery::cursor % gallery::row_count;
			gallery::cursor      = row_pos;
		}
		else
		{
			gallery::cursor = ( gallery::cursor + gallery::row_count ) % gallery::items.size();
		}

		gallery_view_scroll_to_cursor();
	}

	// shift adds to the selecction amount
	if ( ImGui::IsKeyDown( ImGuiKey_LeftShift ) )
	{
	}
}


std::string gallery_item_get_path_string( gallery_item_t& item )
{
	if ( item.file_index >= directory::media_list.size() )
		return {};

	return directory::media_list[ item.file_index ].path.string();
}


fs::path gallery_item_get_path( gallery_item_t& item )
{
	if ( item.file_index >= directory::media_list.size() )
		return {};

	return directory::media_list[ item.file_index ].path;
}


fs::path gallery_item_get_path( size_t index )
{
	if ( index >= gallery::items.size() )
		return {};

	return gallery_item_get_path( gallery::items[ index ] );
}


std::string gallery_item_get_path_string( size_t index )
{
	if ( index >= gallery::items.size() )
		return {};

	return gallery_item_get_path_string( gallery::items[ index ] );
}


media_entry_t gallery_item_get_media_entry( size_t index )
{
	if ( index >= gallery::items.size() )
		return {};

	return directory::media_list[ gallery::items[ index ].file_index ];
}


// =============================================================================================
// Sorting


// Date Modified
int qsort_date_mod_newest( const void* left, const void* right )
{
	const gallery_item_t* item_left  = static_cast< const gallery_item_t* >( left );
	const gallery_item_t* item_right = static_cast< const gallery_item_t* >( right );

	if ( item_left->date_mod > item_right->date_mod )
		return -1;
	else if ( item_left->date_mod < item_right->date_mod )
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
	const gallery_item_t* item_left  = static_cast< const gallery_item_t* >( left );
	const gallery_item_t* item_right = static_cast< const gallery_item_t* >( right );

	if ( item_left->date_created > item_right->date_created )
		return -1;
	else if ( item_left->date_created < item_right->date_created )
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
	const gallery_item_t* item_left  = static_cast< const gallery_item_t* >( left );
	const gallery_item_t* item_right = static_cast< const gallery_item_t* >( right );

	if ( item_left->file_size > item_right->file_size )
		return -1;
	else if ( item_left->file_size < item_right->file_size )
		return 1;

	return 0;
}


int qsort_size_small_to_large( const void* left, const void* right )
{
	return qsort_size_large_to_small( left, right ) * -1;
}


void gallery_view_sort_list( std::vector< gallery_item_t >& gallery_list )
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
			break;
		}

		case e_gallery_sort_mode_date_mod_new_to_old:
		{
			std::qsort( gallery_list.data(), gallery_list.size(), sizeof( gallery_item_t ), qsort_date_mod_newest );
			break;
		}

		case e_gallery_sort_mode_date_mod_old_to_new:
		{
			std::qsort( gallery_list.data(), gallery_list.size(), sizeof( gallery_item_t ), qsort_date_mod_oldest );
			break;
		}

		case e_gallery_sort_mode_date_created_new_to_old:
		{
			std::qsort( gallery_list.data(), gallery_list.size(), sizeof( gallery_item_t ), qsort_date_created_newest );
			break;
		}

		case e_gallery_sort_mode_date_created_old_to_new:
		{
			std::qsort( gallery_list.data(), gallery_list.size(), sizeof( gallery_item_t ), qsort_date_created_oldest );
			break;
		}

		case e_gallery_sort_mode_size_large_to_small:
		{
			std::qsort( gallery_list.data(), gallery_list.size(), sizeof( gallery_item_t ), qsort_size_large_to_small );
			break;
		}

		case e_gallery_sort_mode_size_small_to_large:
		{
			std::qsort( gallery_list.data(), gallery_list.size(), sizeof( gallery_item_t ), qsort_size_small_to_large );
			break;
		}
	}
}


void gallery_view_sort_dir()
{
	static std::vector< gallery_item_t > folders;
	static std::vector< gallery_item_t > files;

	folders.clear();
	files.clear();

	folders.reserve( directory::media_list.size() );
	files.reserve( directory::media_list.size() );

	size_t         selected_item_idx = gallery::cursor;
	media_entry_t  selected_item{};
	gallery_item_t selected_item_gallery{};
	bool           selected_folder = false;

	if ( gallery::items.size() )
	{
		selected_item_gallery = gallery::items[ selected_item_idx ];
		selected_item         = gallery_item_get_media_entry( selected_item_idx );
		selected_folder       = selected_item.type == e_media_type_directory;
	}

	// Fill lists with information
	for ( size_t i = 0; i < directory::media_list.size(); i++ )
	{
		gallery_item_t gallery_item{};
		gallery_item.file_index = i;

		fs::path&      path     = directory::media_list[ i ].path;
		std::string    str_path = directory::media_list[ i ].path.string();

		if ( directory::media_list[ i ].type == e_media_type_directory )
		{
			//if ( selected_folder && selected_item.path == path )
			//	gallery::cursor = i;

			folders.push_back( gallery_item );
		}
		else
		{
			//if ( !selected_folder && selected_item.path == path )
			//	gallery::cursor = i;

			if ( !sys_get_file_times( str_path.data(), &gallery_item.date_created, nullptr, &gallery_item.date_mod ) )
			{
				printf( "Failed to get file date created and modified: %s\n", str_path.data() );
			}

			gallery_item.file_size = fs::file_size( path );
			files.push_back( gallery_item );
		}
	}

	// Sort data
	if ( gallery::sort_mode != e_gallery_sort_mode_size_large_to_small && gallery::sort_mode != gallery::sort_mode )
		gallery_view_sort_list( folders );

	gallery_view_sort_list( files );

	gallery::items.clear();
	gallery::items.resize( directory::media_list.size() );

	// Add Folders First
	std::copy( folders.begin(), folders.end(), gallery::items.begin() );

	// Add Files next
	std::copy( files.begin(), files.end(), gallery::items.begin() + folders.size() );

	// Find Selected File
	if ( !selected_item.path.empty() )
	{
		for ( size_t i = 0; i < gallery::items.size(); i++ )
		{
			gallery_item_t& item = gallery::items[ i ];

			if ( selected_item_gallery.file_size != item.file_size )
				continue;

			if ( selected_item_gallery.date_created != item.date_created )
				continue;

			if ( selected_item_gallery.date_mod != item.date_mod )
				continue;

			fs::path& path = directory::media_list[ item.file_index ].path;

			if ( path != selected_item.path )
				continue;

			gallery::cursor = i;
			gallery_view_scroll_to_cursor();
			break;
		}
	}
}


// =============================================================================================


void gallery_view_dir_change()
{
	gallery_view_update_header_directory();

	gallery::items.clear();

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
	static bool ctx_open = false;
	if ( !ctx_open  && !ImGui::IsMouseClicked( ImGuiMouseButton_Right ) )
		return;

	ctx_open = true;

	if ( !ImGui::BeginPopup( "##gallery ctx menu" ) )
		return;

	ImGuiStyle& style        = ImGui::GetStyle();
	ImVec2      region_avail = ImGui::GetContentRegionAvail();

	if ( ImGui::MenuItem( "Open File Location", nullptr, false, g_image_data.texture ) )
	{
		sys_browse_to_file( gallery_item_get_path_string( gallery::cursor ).c_str() );
		ctx_open = false;
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
		ctx_open = false;
	}

	ImGui::Separator();

	if ( ImGui::MenuItem( "Settings", nullptr, false, false ) )
	{
	}

	ImGui::EndPopup();
}


void gallery_view_draw_image( image_t* image, ImTextureRef im_texture, ImVec2 image_bounds, bool upscale )
{
	// Fit image in window size, scaling up if needed
	float factor[ 2 ] = { 1.f, 1.f };

	if ( upscale || image->width > image_bounds.x )
		factor[ 0 ] = (float)image_bounds.x / (float)image->width;

	if ( upscale || image->height > image_bounds.y )
		factor[ 1 ] = (float)image_bounds.y / (float)image->height;

	float  zoom_level = std::min( factor[ 0 ], factor[ 1 ] );

	ImVec2 scaled_image_size;
	scaled_image_size.x = image->width * zoom_level;
	scaled_image_size.y = image->height * zoom_level;

	// center the image
	ImVec2 image_offset = ImGui::GetCursorPos();
	image_offset.x += ( image_bounds.x - scaled_image_size.x ) / 2;
	image_offset.y += ( image_bounds.y - scaled_image_size.y ) / 2;

	ImGui::SetCursorPos( image_offset );

	ImGui::Image( im_texture, scaled_image_size );
}


// called when file is double clicked or enter is pressed on it
void gallery_selected_item_action( const media_entry_t& media )
{
	if ( media.type == e_media_type_directory )
	{
		directory::queued = media.path;
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

	ImGui::PushStyleColor( ImGuiCol_ChildBg, app::clear_color );

	// if ( !ImGui::BeginChild( "##gallery_content", { region_avail.x + style.WindowPadding.x, region_avail.y }, ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollWithMouse ) )
	if ( !ImGui::BeginChild( "##gallery_content", {}, ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBringToFrontOnFocus ) )
	{
		ImGui::EndChild();
		ImGui::PopStyleColor();
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

	float  text_height         = ImGui::CalcTextSize( "TEST" ).y;

	// ScrollToBringRectIntoView

	int    grid_item_padding   = style.ItemSpacing.x;

	gallery::row_count        = (int)(region_avail.x - style.ScrollbarSize) / ( gallery::item_size );

	float  item_size_x         = gallery::item_size - ( grid_item_padding );
	float  item_size_y         = item_size_x + text_height + style.ItemSpacing.y;

	int    grid_pos_x          = 0;
	size_t i                   = 0;

	int    image_visible_count = 0;

	struct delayed_load_t
	{
		fs::path     path;
		size_t       index;
		e_media_type type;
	};

	static std::vector< delayed_load_t > thumbnail_requests;
	thumbnail_requests.clear();

	// scroll speed hack
	if ( content_area_hovered )
	{
		float scroll = ImGui::GetScrollY();
		float scroll_amount = item_size_y + style.ItemSpacing.y;
		
		if ( app::mouse_scrolled_up )
			scroll -= scroll_amount;

		else if ( app::mouse_scrolled_down )
			scroll += scroll_amount;

		if ( app::window_resized )
		{
			float scroll_diff = fmod( scroll, scroll_amount );

			if ( scroll_diff > 0 )
				scroll -= scroll_diff;
		}

		ImGui::SetScrollY( scroll );
	}

	ImVec2 image_bounds      = { item_size_x - ( style.WindowPadding.x * 2 ), item_size_x - ( style.WindowPadding.x * 2 ) };
	gallery::image_size     = image_bounds.x;

	ImVec2 image_icon_bounds = { image_bounds.x / 4.f, image_bounds.y / 4.f };

	for ( size_t i = 0; i < gallery::items.size(); i++ )
	{
		const gallery_item_t& gallery_item = gallery::items[ i ];
		const media_entry_t&  media        = directory::media_list[ gallery_item.file_index ];

		float                 scroll       = ImGui::GetScrollY();

		ImVec2                media_text_size{};

		if ( gallery::item_size_changed )
		{
			media_text_size = ImGui::CalcTextSize( media.filename.c_str(), 0, false, item_size_x - ( style.WindowPadding.x * 2 ) );
			gallery::item_text_size[ gallery_item.file_index ] = media_text_size;
		}
		else
		{
			media_text_size = gallery::item_text_size[ gallery_item.file_index ];
		}

		//ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, { 0.f, 0.f } );

		// float  image_bounds = item_size_x - ( style.ItemInnerSpacing.y * 4 );
		// float  image_bounds = item_size_x - ( style.ItemInnerSpacing.x * 2 );
		// float  image_bounds = item_size_x - ( style.WindowPadding.x * 2 );

		// float                current_item_size_y = std::min( item_size_x + media_text_size.y + style.ItemInnerSpacing.y, item_size_y * 1.75f );
		float                current_item_size_y = image_bounds.y + media_text_size.y + style.ItemSpacing.y + ( style.WindowPadding.y * 2 );

		current_item_size_y                      = std::min( current_item_size_y, item_size_y * 1.75f );

		ImGui::SetNextWindowSize( { item_size_x, current_item_size_y } );
		// ImGui::SetNextWindowSize( { item_size_x, 0 } );
		ImGui::SetNextWindowSizeConstraints( { item_size_x, item_size_x }, { -1.f, -1.f } );

		// check if i is 0, for some reason it adds like extra spacing for some reason on the first item
		if ( grid_pos_x == gallery::row_count || i == 0 )
		{
			grid_pos_x = 0;
			//float new_pos = std::min( region_avail.x, ImGui::GetCursorPosX() + style.ItemSpacing.x );
			//ImGui::SetCursorPosX( style.ItemSpacing.x );
			// ImGui::SetCursorPosX( ImGui::GetCursorPosX() + style.ItemSpacing.x );
			// ImGui::SetCursorPosX( ImGui::GetCursorPosX() + style.WindowPadding.x );
			ImGui::SetCursorPosX( ImGui::GetCursorPosX() );
			// ImGui::SetCursorPosX( ImGui::GetCursorPosX() );
		}
		else
		{
			// ImGui::SetCursorPos( { cursor_pos.x + item_size_x + grid_item_padding, cursor_pos.y } );
			ImGui::SameLine();
		}

		ImVec2 cursor_pos = ImGui::GetCursorPos();

		// Calculate Distance
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
			thumbnail_update_distance( directory::thumbnail_list[ i ], distance );
		}

		ImVec2 cursor_screen_pos = ImGui::GetCursorScreenPos();
		bool   item_hovered      = false;

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
		}

		// is this item even visible?
		if ( !ImGui::IsRectVisible( cursor_screen_pos, { cursor_screen_pos.x + item_size_x, cursor_screen_pos.y + current_item_size_y } ) )
		{
			// use a dummy instead of a full child window, cheaper
			ImGui::Dummy( { item_size_x, current_item_size_y } );
			grid_pos_x++;
			continue;
		}

		if ( content_area_hovered )
			item_hovered = ImGui::IsMouseHoveringRect( cursor_screen_pos, { cursor_screen_pos.x + item_size_x, cursor_screen_pos.y + current_item_size_y } );

		bool selected_item = gallery::cursor == i;

		if ( selected_item )
			ImGui::PushStyleColor( ImGuiCol_ChildBg, style.Colors[ ImGuiCol_FrameBg ] );
		else if ( item_hovered )
			ImGui::PushStyleColor( ImGuiCol_ChildBg, style.Colors[ ImGuiCol_FrameBgHovered ] );

		// if ( ImGui::BeginChild( i + 1, {}, ImGuiChildFlags_AlwaysUseWindowPadding | ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings ) )
		if ( ImGui::BeginChild( i + 1, {}, ImGuiChildFlags_AlwaysUseWindowPadding | ImGuiChildFlags_AutoResizeY, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings ) )
		{
			ImVec2 current_pos  = ImGui::GetCursorPos();
#if 1
			image_visible_count++;

			//ImGui::SetNextWindowSize( { image_bounds, image_bounds } );

			ImGui::PushStyleColor( ImGuiCol_ChildBg, { 0.25f, 0.25f, 0.25f, 1.f } );

			#if 0
			ImGui::Dummy( image_bounds, image_bounds );
			#else

			if ( media.type == e_media_type_directory )
			{
				gallery_view_draw_image( icon_get_image( e_icon_folder ), icon_get_imtexture( e_icon_folder ), image_bounds, false );
			}
			// videos don't have thumbnail generation yet
			// else if ( media.type == e_media_type_video )
			// {
			// 	gallery_view_draw_image( icon_get_image( e_icon_video ), icon_get_imtexture( e_icon_video ), image_bounds, false );
			// }
			else
			{
				thumbnail_t* thumbnail = thumbnail_get_data( directory::thumbnail_list[ gallery_item.file_index ] );

				if ( thumbnail )
				{
					if ( thumbnail->status == e_thumbnail_status_finished )
					{
						gallery_view_draw_image( thumbnail->image, thumbnail->im_texture, image_bounds, true );
					}
					else if ( thumbnail->status == e_thumbnail_status_failed )
					{
						gallery_view_draw_image( icon_get_image( e_icon_invalid ), icon_get_imtexture( e_icon_invalid ), image_bounds, false );
					}
					else if ( thumbnail->status == e_thumbnail_status_queued || thumbnail->status == e_thumbnail_status_loading || thumbnail->status == e_thumbnail_status_uploading )
					{
						gallery_view_draw_image( icon_get_image( e_icon_loading ), icon_get_imtexture( e_icon_loading ), image_bounds, false );
					}
					else if ( thumbnail->status == e_thumbnail_status_free )
					{
						if ( media.type != e_media_type_directory )
							thumbnail_requests.emplace_back( media.path, gallery_item.file_index, media.type );

						ImGui::Dummy( image_bounds );
					}
					else // if ( thumbnail->status == e_thumbnail_status_free )
					{
						ImGui::Dummy( image_bounds );
					}
				}
				else
				{
					if ( !thumbnail && media.type != e_media_type_directory )
						thumbnail_requests.emplace_back( media.path, gallery_item.file_index, media.type );
					// directory::thumbnail_list[ i ] = thumbnail_queue_image( entry );

					ImGui::Dummy( image_bounds );
				}
			}

			// draw icon on top of it in the corner
			if ( media.type == e_media_type_video )
			{
				// Fit image in window size, scaling up if needed
				float factor[ 2 ] = { 1.f, 1.f };

				image_t* icon_video = icon_get_image( e_icon_video );

				//if ( image->width > image_bounds.x )
				factor[ 0 ]          = (float)image_icon_bounds.x / (float)icon_video->width;

				//if ( image->height > image_bounds.y )
				factor[ 1 ]          = (float)image_icon_bounds.y / (float)icon_video->height;

				float  zoom_level = std::min( factor[ 0 ], factor[ 1 ] );

				ImVec2 scaled_image_size;
				scaled_image_size.x = icon_video->width * zoom_level;
				scaled_image_size.y = icon_video->height * zoom_level;

				// center the image
				ImVec2 image_offset = ImGui::GetCursorPos();
				//ImVec2 image_offset = current_pos;
				//image_offset.x += image_bounds.x;
				//image_offset.y += image_bounds.y;
				// image_offset.x += ( image_bounds.x - scaled_image_size.x ) / 2;
				// image_offset.y += ( image_bounds.y - scaled_image_size.y ) / 2;

				image_offset.x += image_bounds.x - ( scaled_image_size.x / 1.25 );
				image_offset.y -= ( scaled_image_size.y / 1.25 ) + style.ItemSpacing.y;
				//image_offset.y += ( image_bounds.y - scaled_image_size.y ) / 2;

				ImGui::SetCursorPos( image_offset );

				ImGui::Image( icon_get_imtexture( e_icon_video ), scaled_image_size );
			}

			#endif

			// if ( visible )
			ImGui::PopStyleColor();

			// center align text
			ImGui::SetCursorPosX( ImGui::GetCursorPosX() + ( ( gallery::item_size - ( media_text_size.x + style.WindowPadding.x * 2 + style.ItemSpacing.x ) ) * 0.5f ) );

			// ImGui::SetCursorPosY( current_pos.y + image_bounds + style.ItemSpacing.y );
			ImGui::SetCursorPosY( current_pos.y + image_bounds.x + style.ItemSpacing.y );
			// ImGui::SetCursorPosY( current_pos.y + item_size_x + style.ItemSpacing.y );

			ImGui::PushTextWrapPos();

			ImGui::TextUnformatted( media.filename.c_str() );

			ImGui::PopTextWrapPos();
#endif
		}


		ImVec2 test_window_size = ImGui::GetWindowSize();


		ImGui::EndChild();

		//ImGui::PopStyleVar();

		if ( selected_item || item_hovered )
			ImGui::PopStyleColor();

		if ( item_hovered && ImGui::IsMouseClicked( ImGuiMouseButton_Left ) )
		{
			gallery::cursor = i;

			if ( ImGui::IsMouseDoubleClicked( ImGuiMouseButton_Left ) )
			{
				gallery_selected_item_action( media );
			}
		}

		if ( !ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed( ImGuiKey_Enter, false ) )
		{
			gallery_selected_item_action( media );
		}

		grid_pos_x++;
	}

	ImGui::EndChild();

	// printf( "IMAGE COUNT: %d\n", image_visible_count );

	for ( size_t i = 0; i < thumbnail_requests.size(); i++ )
		directory::thumbnail_list[ thumbnail_requests[ i ].index ] = thumbnail_queue_image( thumbnail_requests[ i ].path, thumbnail_requests[ i ].type );

	gallery::scroll_to_cursor        = false;
	gallery::item_size_changed = false;

	ImGui::PopStyleColor();
}


void gallery_view_draw()
{
	if ( gallery::sort_mode_update )
	{
		gallery_view_sort_dir();
		gallery::sort_mode_update = false;
	}

	gallery_view_input();

	int window_width, window_height;
	SDL_GetWindowSize( app::window, &window_width, &window_height );

	ImGui::SetNextWindowPos( { 0, 0 } );
	ImGui::SetNextWindowSize( { (float)window_width, (float)window_height } );

	if ( !ImGui::Begin( "##gallery_main", 0, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar ) )
	{
		ImGui::End();
		return;
	}

	// Header
	gallery_view_draw_header();

	// Sidebar
	if ( gallery::sidebar_draw )
	{
		gallery_view_draw_sidebar();
		ImGui::SameLine();
	}

	// Gallery View
	gallery_view_draw_content();

	thumbnail_cache_debug_draw();

	gallery_view_context_menu();

	ImGui::End();

	// TODO: Test ImGui::Shortcut()
	if ( app::window_focused && ImGui::IsKeyDown( ImGuiKey_LeftCtrl ) && ImGui::IsKeyPressed( ImGuiKey_C, false ) )
	{
		sys_copy_to_clipboard( gallery_item_get_path_string( gallery::cursor ).data() );
		printf( "Copied to Clipboard\n" );
		push_notification( "Copied" );
	}
}

