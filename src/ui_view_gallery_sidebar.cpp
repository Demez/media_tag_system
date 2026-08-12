#include "main.h"

#include "imgui_internal.h"


static char g_folder_buf[ 512 ]{};


static void draw_vertical_separator( ImDrawList* draw_list, ImGuiStyle& style, bool add_spacing = true )
{
	ImVec2 cursor_pos = ImGui::GetCursorPos();

	// TEMP DISABLE
	if ( style.WindowBorderSize > 0 )
	{
		ImColor border_col   = style.Colors[ ImGuiCol_Border ];
		ImVec2  region_avail = ImGui::GetContentRegionAvail();
		// float  window_height = ImGui::GetWindowHeight();
		float  window_height = ImGui::GetFrameHeight();
	
		ImVec2  line_start   = { cursor_pos.x, cursor_pos.y - style.WindowPadding.y };
		ImVec2  line_end     = cursor_pos;
	
		// line_start.y -= style.FramePadding.y;
		line_end.y += window_height + style.FramePadding.y;
	
		draw_list->AddLine( line_start, line_end, border_col, style.WindowBorderSize );
	
		// ImGui::SetCursorPosX( cursor_pos.x + style.ItemSpacing.x );
		if ( add_spacing )
			ImGui::SetCursorPosX( cursor_pos.x + style.WindowBorderSize + style.ItemSpacing.x );
	}
	else if ( add_spacing )
	{
		ImGui::SetCursorPosX( cursor_pos.x + style.ItemSpacing.x );
	}
}


void gallery_update_filter( e_gallery_filter filter )
{
	if ( gallery::filter & filter )
		gallery::filter &= ~filter;
	else
		gallery::filter |= filter;

	gallery_view_dir_change( true );
}


void gallery_header_draw_path_bar( bool& was_in_path_edit, float& bar_width, ImVec2 region_avail, float& right_side_space_needed )
{
	was_in_path_edit            = false;
	static bool path_edit_hover = false;

	ImGuiStyle& style = ImGui::GetStyle();
	//ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, { ImGui::GetFontSize() / 8.f, style.ItemSpacing.y } );
	ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, { 0.f, style.ItemSpacing.y } );
	//ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, { 0, style.FramePadding.y } );

	// ImVec2 bar_size = ImGui::CalcItemSize( path_text_size, 0, 0 );
	// ImGui::SetNextItemWidth( bar_size.x * 1.25 );

	// ImGui::SetNextWindowSizeConstraints( { 100, -1 }, { 600, -1 } );

	if ( path_edit_hover )
	{
		ImVec4 color = style.Colors[ ImGuiCol_FrameBg ];
		color.x *= 1.75;
		color.y *= 1.75;
		color.z *= 1.75;
		color.w *= 1.75;

		ImGui::PushStyleColor( ImGuiCol_FrameBg, color );
	}

	// ImGuiChildFlags_AutoResizeX
	if ( ImGui::BeginChild( "##breadcrumb_bar", {}, ImGuiChildFlags_FrameStyle | ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove ) )
	{
		int window_width = 0;
		SDL_GetWindowSize( app::window, &window_width, nullptr );

		bool   item_hovered = false;

		int    id           = 1;
		ImVec2 item_size{};
		for ( size_t i = 0; i < directory::path_chunks.size(); i++ )
		{
			ImGui::PushID( id++ );

			ImVec2 text_size = ImGui::CalcTextSize( directory::path_chunks[ i ].c_str() );
			item_size        = ImGui::CalcItemSize( text_size, 0, 0 );
			// item_size.x += style.ItemInnerSpacing.x * 2;
			// item_size.y      = ImGui::GetWindowHeight();

			if ( ImGui::Selectable( directory::path_chunks[ i ].c_str(), false, 0, item_size ) )
			{
				directory::queued.clear();

				// build new path where we are currently
				for ( size_t j = 0; j < i + 1; j++ )
				{
					directory::queued += directory::path_chunks[ j ];

					if ( j < i )
						directory::queued += SEP_S;
				}
			}

			if ( ImGui::IsItemHovered() )
				item_hovered |= true;

			ImGui::PopID();

			if ( i + 1 < directory::path_chunks.size() )
			{
				ImGui::SameLine();

				ImGui::PushID( id++ );
				ImGui::TextUnformatted( SEP_S );
				ImGui::PopID();
			}

			if ( ImGui::IsItemHovered() )
				item_hovered |= true;

			ImGui::SameLine();
		}

		// float  region_avail           = ImGui::GetContentRegionAvail().x;
		// float  region_avail_y         = ImGui::GetContentRegionAvail().y;

		ImVec2  region_avail_2    = ImGui::GetContentRegionAvail();

		ImVec2  cursor_pos        = ImGui::GetCursorPos();
		ImVec2  cursor_screen_pos = ImGui::GetCursorScreenPos();
		float   bar_size_min      = 100.f * style.FontScaleDpi;
		float   padding_extra     = ( ImGui::GetFontSize() * 2.f );
		float   nav_enter_width   = ImGui::GetFrameHeight();
		float   space_avail       = ( cursor_pos.x + style.FramePadding.x );

		float   free_space        = region_avail.x - cursor_screen_pos.x;
		float   space_avail2      = static_cast< float >( window_width ) - cursor_screen_pos.x;

		// float       bar_size                      = std::max( std::min( cursor_pos.x + padding_extra, bar_size_min + padding_extra ), free_space - space_needed );
		float   bar_size          = std::max( padding_extra, free_space - right_side_space_needed );

		//if ( right_side_space_needed == 0.f )
		//	bar_size = space_avail2 - ( nav_enter_width + style.ItemSpacing.y * 2.f + style.WindowPadding.x );

		// if ( bar_size == padding_extra )
		if ( free_space < right_side_space_needed / 2.f )
		{
			right_side_space_needed = 0.f;
			bar_size = space_avail2 - ( nav_enter_width + style.ItemSpacing.y * 2.f + style.WindowPadding.x );
		}

		ImVec2  window_pos        = ImGui::GetWindowPos();

		// ImVec2      window_cursor_pos( window_pos.x + cursor_base_pos.x, ( window_pos.y + cursor_base_pos.y ) );
		ImVec2  window_cursor_pos( window_pos.x + cursor_pos.x, cursor_pos.y );
		ImVec2  global_item_size = ImVec2( window_cursor_pos.x + bar_size + style.FramePadding.x, window_cursor_pos.y + ImGui::GetWindowHeight() );

		ImColor main_bg_color    = item_hovered ? style.Colors[ ImGuiCol_ButtonActive ] : style.Colors[ ImGuiCol_ButtonActive ];

		// draw_list->AddRectFilled( child_size_min, child_size_max, main_bg_color, style.ChildRounding, ImDrawFlags_RoundCornersAll );

		bool    rect_hovered     = ImGui::IsMouseHoveringRect( cursor_screen_pos, global_item_size, true );

		if ( path_edit_hover )
			ImGui::PopStyleColor();

		if ( rect_hovered && !item_hovered )
		{
			path_edit_hover = true;
			set_frame_draw( 1 );

			if ( ImGui::IsMouseClicked( ImGuiMouseButton_Left ) )
				directory::path_edit = true;
		}
		else
		{
			if ( path_edit_hover )
			{
				path_edit_hover = false;
				set_frame_draw( 1 );
			}
		}

		ImGui::Dummy( { bar_size, item_size.y } );
		ImGui::PopStyleVar();
	}

	bar_width = ImGui::GetWindowWidth();

	ImGui::EndChild();
}


void gallery_header_draw_path_text_edit( bool& was_in_path_edit, float bar_width )
{
	ImGui::SetNextItemWidth( bar_width );

	if ( !was_in_path_edit )
	{
		ImGui::SetKeyboardFocusHere();
	}

	if ( ImGui::InputText( "##directory", g_folder_buf, 512, ImGuiInputTextFlags_EnterReturnsTrue ) )
	{
		if ( fs_is_dir( g_folder_buf ) )
		{
			directory::queued = sys_string_to_path( g_folder_buf );
		}
		else
		{
			std::string path_str = sys_path_to_string( directory::path );
			snprintf( g_folder_buf, 512, path_str.c_str() );
		}

		directory::path_edit = false;
	}

	if ( was_in_path_edit && !ImGui::IsItemActive() )
	{
		directory::path_edit = false;

		// reset on focus loss
		std::string path_str = sys_path_to_string( directory::path );
		snprintf( g_folder_buf, 512, path_str.c_str() );
	}

	if ( !was_in_path_edit )
	{
		was_in_path_edit = true;
	}
}


void gallery_header_draw_path( ImVec2 region_avail, float& right_side_space_needed )
{
	static bool  was_in_path_edit = false;
	static float bar_width        = 0.f;

	//std::string  path_str         = sys_path_to_string( directory::path );
	//ImVec2       path_text_size   = ImGui::CalcTextSize( path_str.c_str() );

	if ( !directory::path_edit )
	{
		gallery_header_draw_path_bar( was_in_path_edit, bar_width, region_avail, right_side_space_needed );
	}
	else
	{
		gallery_header_draw_path_text_edit( was_in_path_edit, bar_width );
	}
}


float gallery_view_draw_header()
{
	int window_width, window_height;
	SDL_GetWindowSize( app::window, &window_width, &window_height );

	ImGuiStyle& style = ImGui::GetStyle();

	ImGui::SetNextWindowPos( { 0, 0 } );
	//ImGui::SetCursorPos( { 0, 0 } );
	//ImGui::SetNextWindowSize( { (float)window_width, 0.f } );
	ImGui::SetNextWindowSizeConstraints( { (float)window_width, 0.f }, { (float)window_width, -1.f } );

	ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, app::config.gallery_header_padding * app::dpi );

	if ( app::config.use_custom_colors )
	{
		ImGui::PushStyleColor( ImGuiCol_FrameBg, style.Colors[ ImGuiCol_WindowBg ] );
		ImGui::PushStyleColor( ImGuiCol_FrameBgHovered, style.Colors[ ImGuiCol_WindowBg ] );
		ImGui::PushStyleColor( ImGuiCol_FrameBgActive, style.Colors[ ImGuiCol_WindowBg ] );

		ImGui::PushStyleColor( ImGuiCol_WindowBg, app::config.header_bg_color );
		ImGui::PushStyleVar( ImGuiStyleVar_WindowBorderSize, 0 );
	}

	// if ( !ImGui::Begin( "##gallery_header", { (float)window_width, 0.f }, ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_FrameStyle | ImGuiChildFlags_AlwaysUseWindowPadding ) )
	if ( !ImGui::Begin( "##gallery_header", 0, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav ) )
	{
		ImGui::End();

		if ( app::config.use_custom_colors )
		{
			ImGui::PopStyleColor( 4 );
			ImGui::PopStyleVar( 2 );
		}
		else
		{
			ImGui::PopStyleVar( 1 );
		}

		return 0;
	}

	ImDrawList* draw_list = ImGui::GetWindowDrawList();

	ImGui::BeginDisabled( !(directory::folder_history.size() && directory::folder_history_pos > 1) );
	if ( ImGui::ArrowButton( "##nav_history_back", ImGuiDir_Left ) )
	{
		folder_history_nav_prev();
	}

	ImGui::EndDisabled();

	ImGui::SameLine();

	ImGui::BeginDisabled( !( directory::folder_history.size() && directory::folder_history.size() > directory::folder_history_pos ) );
	if ( ImGui::ArrowButton( "##nav_history_next", ImGuiDir_Right ) )
	{
		folder_history_nav_next();
	}
	ImGui::EndDisabled();

	ImGui::SameLine();

	if ( ImGui::ArrowButton( "##nav_parent_path", ImGuiDir_Up ) )
	{
		directory::queued = directory::path.parent_path();
	}

	ImGui::SameLine();

	if ( ImGui::Button( "Refresh" ) )
	{
		directory::queued = directory::path;
	}

	ImGui::SameLine();

	draw_vertical_separator( draw_list, style );

	// ---------------------------------------------------------------------------------
	// Center Spacing, rest is aligned to the right

	ImVec2       region_avail     = ImGui::GetContentRegionAvail();

	float        space_needed     = 0;
	int          search_box_size  = 150;

	ImVec2       search_size      = ImGui::CalcTextSize( "Search" );
	ImVec2       recursive_size   = ImGui::CalcTextSize( "Search Subfolders" );

	space_needed += search_size.x + recursive_size.x;
	space_needed += style.WindowBorderSize * 2;  // Separator
	space_needed += style.ItemSpacing.x * 2;     // Search Text
	space_needed += style.ItemSpacing.x * 2;     // Recursive
	space_needed += style.ItemSpacing.x * 6;     // Padding
	space_needed += search_box_size;

	int sep_space_needed = 0;

	if ( style.WindowBorderSize )
		sep_space_needed = style.ItemSpacing.x + style.WindowBorderSize;

	// estimate
	float arrow_size = style.FontSizeBase;

	// ??
	// space_needed += 60.f;
	//space_needed += style.ItemSpacing.x;
	
	// ---------------------------------------------------------------------------------

	gallery_header_draw_path( region_avail, space_needed );

	ImGui::SameLine();

	// Enter returns true doesn't work because of gallery view hooking that input currently, need to add a check later for if focused in text input
	if ( ImGui::ArrowButton( "##nav_enter_path", ImGuiDir_Right ) )
	{
		directory::queued    = g_folder_buf;
		directory::path_edit = false;
	}

	if ( space_needed > 0 )
	{
		ImGui::SameLine();
		//draw_vertical_separator( draw_list, style, false );
		draw_vertical_separator( draw_list, style, true );

		ImVec2 region_avail_2 = ImGui::GetContentRegionAvail();

		ImVec2 spacing_size( region_avail_2.x - space_needed, ImGui::GetFrameHeight() );
		spacing_size.x           = std::max( spacing_size.x, 0.f );

		float search_space_avail = region_avail_2.x - ( space_needed - search_box_size );

		if ( search_space_avail < 17 )
		{
			ImGui::Dummy( spacing_size );
		}
		else
		{
			ImGui::TextUnformatted( "Search" );

			ImGui::SameLine();

			// Press Ctrl+F to focus the search text input
			if ( !ImGui::GetIO().WantTextInput )
			{
				if ( ImGui::IsKeyChordPressed( ImGuiMod_Ctrl | ImGuiKey_F ) )
					ImGui::SetKeyboardFocusHere();
			}

			ImGui::SetNextItemWidth( search_space_avail );

			if ( ImGui::InputText( "##search", gallery::search, 512 ) )
			{
				gallery_view_dir_change( true );
			}

			ImGui::SameLine();
			draw_vertical_separator( draw_list, style );

			if ( ImGui::Checkbox( "Search Subfolders", &directory::recursive ) )
			{
				directory::queued = g_folder_buf;

				if ( !directory::recursive )
				{
					gallery_view_reset();
					gallery_view_clear_selection();
					//gallery::cursor = 0;
				}
			}
		}
	}

	//ImGui::Dummy( spacing_size );
	//ImGui::SameLine( 0, 0 );
	//draw_vertical_separator( draw_list, style );

	//ImGui::SameLine();
	//draw_vertical_separator( draw_list, style );

	// ---------------------------------------------------------------------------------
	// Center Spacing, rest is aligned to the right

	ImGui::Separator();

	if ( ImGui::Button( "Sidebar" ) )
	{
		gallery::sidebar_draw = !gallery::sidebar_draw;
		gallery_view_scroll_to_cursor();
	}

	// ImGui::Selectable( "Sidebar", &gallery::sidebar_draw );
	ImGui::SameLine();
	draw_vertical_separator( draw_list, style );

	//ImGui::Spacing();
	//ImGui::SameLine();

	ImVec2 filter_size       = ImGui::CalcTextSize( "Quick Filter" );
	ImVec2 sort_size         = ImGui::CalcTextSize( "Sort By" );
	ImVec2 sort_entry_size   = ImGui::CalcTextSize( "Date Modified - New to Old" );
	ImVec2 filter_entry_size = ImGui::CalcTextSize( "Quick Filter" );

	float  sort_width   = sort_entry_size.x + arrow_size + ( style.FramePadding.x * 3 ) + ( style.FramePadding.y * 2 );
	float  filter_width = filter_entry_size.x + arrow_size + ( style.FramePadding.x * 3 ) + ( style.FramePadding.y * 2 );

	// ---------------------------------------------------------------------------------

	ImGui::TextUnformatted( "Zoom" );
	ImGui::SameLine();

	ImGui::SetNextItemWidth( 120 );

	SDL_MouseButtonFlags mouse_state      = SDL_GetMouseState( 0, 0 );
	static bool          check_left_mouse = false;

	bool                 zoom_changed     = false;

	// if ( ImGui::SliderInt( "Zoom", &gallery::item_size, gallery::item_size_min, gallery::item_size_max ) )
	// if ( ImGui::DragInt( "##zoom", &gallery::item_size, 10, gallery::item_size_min, gallery::item_size_max, "Zoom - %d px" ) )
	//zoom_changed                          = ImGui::SliderScalar( "##zoom", ImGuiDataType_U32, &gallery::item_size, &gallery::item_size_min, &gallery::item_size_max, "%d px" );
	//u32                  step             = 30;
	//// zoom_changed                          = ImGui::InputScalar( "##zoom", ImGuiDataType_U32, &gallery::item_size, &step, &step, "%d px" );
	//zoom_changed                          = ImGui::InputText( "##zoom", ImGuiDataType_U32, &gallery::item_size, &step, &step, "%d px" );
	//
	//ImGui::SameLine();

#if 0
	if ( ImGui::InputScalar( "##zoom", ImGuiDataType_U32, &gallery::item_size, &step, &step, "%d px" ) )
	{
		gallery::item_size = CLAMP( gallery::item_size, gallery::item_size_min, gallery::item_size_max );

		gallery_view_reset_text_size();
		gallery_view_scroll_to_cursor();
		check_left_mouse            = true;
		gallery::item_size_changing = true;

		if ( !app::config.thumbnail_use_fixed_size )
			thumbnail_clear_cache();
	}
#endif

	ImGui::PushFont( font::normal_bold );

	ImVec2 xl_size      = ImGui::CalcTextSize( "XL" );
	ImVec2 button_size{
		xl_size.y + style.FramePadding.x * 2.0f,
		xl_size.y + style.FramePadding.y * 2.0f
	};

	if ( ImGui::Button( "S", button_size ) )
	{
		zoom_changed = true;
		gallery::item_size = gallery::item_size_min;
	}

	ImGui::SameLine();

	if ( ImGui::Button( "M", button_size ) )
	{
		zoom_changed       = true;
		gallery::item_size = 150;
	}

	ImGui::SameLine();

	if ( ImGui::Button( "L", button_size ) )
	{
		zoom_changed       = true;
		gallery::item_size = 300;
	}

	ImGui::SameLine();

	if ( ImGui::Button( "XL", button_size ) )
	{
		zoom_changed       = true;
		gallery::item_size = gallery::item_size_max;
	}

	ImGui::PopFont();

	if ( zoom_changed )
	{
		gallery::item_size = CLAMP( gallery::item_size, gallery::item_size_min, gallery::item_size_max );

		gallery_view_reset_text_size();
		gallery_view_scroll_to_cursor();
		check_left_mouse            = true;
		gallery::item_size_changing = true;

		if ( !app::config.thumbnail_use_fixed_size )
			thumbnail_clear_cache();
	}

	if ( check_left_mouse && gallery::item_size_changing )
	{
		if ( !( mouse_state & SDL_BUTTON_LMASK ) )
		{
			gallery::item_size_changing = false;
			check_left_mouse            = false;
		}
	}

	ImGui::SameLine();
	draw_vertical_separator( draw_list, style );

	//ImGui::TextUnformatted( "Quick Filter" );
	//ImGui::SameLine();

	ImGui::SetNextItemWidth( filter_width );

	// this could be a check box thing for toggling what you want to view, all enabled by default, but that is slower
	if ( ImGui::BeginCombo( "##quick_filter", "Quick Filter", 0 ) )
	{
		if ( ImGui::MenuItem( "None", 0, gallery::filter == 0 ) )
		{
			gallery::filter = 0;
			gallery_update_filter( 0 );
			//gallery::sort_mode_update = true;
		}

		// No folders in recursive mode
		ImGui::BeginDisabled( directory::recursive );
		if ( ImGui::MenuItem( "Folders", 0, gallery::filter & e_gallery_filter_folders ) )
		{
			gallery_update_filter( e_gallery_filter_folders );
		}
		ImGui::EndDisabled();

		if ( ImGui::MenuItem( "Images", 0, gallery::filter & e_gallery_filter_images ) )
		{
			gallery_update_filter( e_gallery_filter_images );
		}

		ImGui::BeginDisabled( !g_mpv );
		if ( ImGui::MenuItem( "Videos", 0, gallery::filter & e_gallery_filter_videos ) )
		{
			gallery_update_filter( e_gallery_filter_videos );
		}
		ImGui::EndDisabled();

		ImGui::EndCombo();
	}

	ImGui::SameLine();
	draw_vertical_separator( draw_list, style );

	ImGui::TextUnformatted( "Sort By" );
	ImGui::SameLine();

	const char* combo_preview_value = g_gallery_sort_mode_str[ gallery::sort_mode ];

	ImGui::SetNextItemWidth( sort_width );

	if ( ImGui::BeginCombo( "##sort_by", combo_preview_value, 0 ) )
	{
		for ( int n = 0; n < e_gallery_sort_mode_count; n++ )
		{
			const bool is_selected = ( gallery::sort_mode == n );

			if ( ImGui::Selectable( g_gallery_sort_mode_str[ n ], is_selected ) )
			{
				gallery::sort_mode        = (e_gallery_sort_mode)n;
				gallery::sort_mode_update = true;
				gallery_view_dir_change( true );
			}

			// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
			if ( is_selected )
			{
				ImGui::SetItemDefaultFocus();
			}
		}

		ImGui::EndCombo();
	}

	ImVec2 dummy_size;

	//if ( gallery::selection.size() )
	//{
		ImGui::SameLine( 0, 0 );

		char buf_selected[ 256 ]{};
		if ( gallery::selection.size() )
		{
			snprintf( buf_selected, 256, "%zu Selected", gallery::selection.size() );

			dummy_size = ImGui::CalcTextSize( buf_selected );
			dummy_size.x += 1 + ( style.ItemSpacing.x * 2 );
		}

		ImVec2 region_avail_tmp = ImGui::GetContentRegionAvail();


		//ImVec2 spacing_size( region_avail_tmp.x - ( dummy_size.x + style.ItemSpacing.x ), dummy_size.y );
		//spacing_size.x = std::max( spacing_size.x, style.ItemSpacing.x );
	//}

	// if ( gallery::selection.size() )
	{
		//if ( !gallery::selection.size() )
			ImGui::SameLine( 0, 0 );

		char   buf[ 256 ]{};
		// snprintf( buf, 256, "%zu File%s Selected", gallery::selection.size(), gallery::selection.size() == 1 ? "" : "s" );
		//if ( gallery::selection.size() )
		//{
		//	snprintf( buf, 256, "%zu Selected | %zu File%s", gallery::selection.size(), gallery::sorted_media.size(), gallery::sorted_media.size() == 1 ? "" : "s" );
		//}
		//else
		{
			snprintf( buf, 256, "%zu File%s", gallery::sorted_media.size(), gallery::sorted_media.size() == 1 ? "" : "s" );
		}

		dummy_size.x += ImGui::CalcTextSize( buf ).x;

		ImVec2 spacing_size( region_avail_tmp.x - ( dummy_size.x + style.ItemSpacing.x ), dummy_size.y );
		spacing_size.x = std::max( spacing_size.x, style.ItemSpacing.x );

		ImGui::Dummy( spacing_size );
		ImGui::SameLine( 0, 0 );

		if ( gallery::selection.size() )
		{
			ImGui::TextUnformatted( buf_selected );

			ImGui::SameLine();
			draw_vertical_separator( draw_list, style );
		}

		ImGui::TextUnformatted( buf );
		ImGui::SameLine( 0, 0 );
	}

	float im_window_height = ImGui::GetWindowHeight();

	if ( ImGui::IsMouseHoveringRect( { 0, 0 }, { (float)window_width, im_window_height } ) && !ImGui::IsAnyItemHovered() )
	{
		/*if ( !app::in_window_drag )
		{
			app::in_window_drag = ImGui::IsKeyPressed( ImGuiKey_MouseLeft );
		}
		else*/ if ( ImGui::IsKeyDown( ImGuiKey_MouseLeft ) )
		{
			app::in_window_drag = true;
		}
	}

	ImGui::End();

	if ( app::config.use_custom_colors )
	{
		ImGui::PopStyleColor( 4 );
		ImGui::PopStyleVar( 2 );
	}
	else
	{
		ImGui::PopStyleVar( 1 );
	}

	return im_window_height;
}


void gallery_view_update_header_directory()
{
	snprintf( g_folder_buf, 512, "%s", directory::path.string().c_str() );
}


void sidebar_draw_bookmarks( ImGuiStyle& style )
{
	//ImGui::PushItemWidth( 40.f );
	ImGui::PushFont( font::normal_bold, style.FontSizeBase + 2.f );
	//ImGui::PushStyleColor( ImGuiCol_Header, header_bg );

	bool opened = ImGui::CollapsingHeader( "Bookmarks", ImGuiTreeNodeFlags_DefaultOpen );

	ImGui::PopFont();
	//ImGui::PopStyleColor();
	//ImGui::PopItemWidth();

	if ( !opened )
	{
		//ImGui::EndChild();
		//ImGui::PopStyleVar();
		return;
	}

	ImDrawList* draw_list     = ImGui::GetWindowDrawList();

	//ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, { 0.f, 0.f } );
	//ImVec4 color_bg      = style.Colors[ ImGuiCol_FrameBg ];
	ImVec4 header_bg     = style.Colors[ ImGuiCol_Header ];
	//ImVec4 header_active = style.Colors[ ImGuiCol_HeaderActive ];
	//ImVec4 header_hover  = style.Colors[ ImGuiCol_HeaderHovered ];
	//
	header_bg.w          = 0.f;
	//ImGui::PushStyleColor( ImGuiCol_FrameBg, header_bg );
	//ImGui::PushStyleColor( ImGuiCol_Header, header_bg );
	//ImGui::PushStyleColor( ImGuiCol_HeaderActive, header_active );
	//ImGui::PushStyleColor( ImGuiCol_HeaderHovered, header_hover );

	ImVec2 start_cursor = ImGui::GetCursorPos();

	if ( !ImGui::BeginChild( "##bookmarks", {}, ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_FrameStyle ) )
	{
		ImGui::EndChild();
		return;
	}

	if ( ImGui::Selectable( "Add Folder" ) )
	{
		app::config.bookmark.emplace_back( directory::path.filename().string(), directory::path.string(), true );
		config_save();
	}

	if ( app::config.bookmark.size() )
		ImGui::Separator();

	ImVec2 remove_text_size = ImGui::CalcTextSize( "X" );
	ImVec2 region_avail     = ImGui::GetContentRegionAvail();

	float  bookmark_width   = region_avail.x - ( ImGui::GetTextLineHeightWithSpacing() );
	ImVec2 bookmark_size( bookmark_width, ImGui::GetTextLineHeight() );
	ImVec2 remove_size( ImGui::GetTextLineHeight(), bookmark_size.y );

	ImVec2 screen_cursor_pos = ImGui::GetCursorScreenPos();

	size_t remove_bookmark   = SIZE_MAX;
	int id = 1;
	size_t bookmark_i        = 0;
	for ( const bookmark_t& bookmark : app::config.bookmark )
	{
		ImGui::PushID( id++ );
		ImGui::BeginDisabled( !bookmark.valid );

		if ( ImGui::Selectable( bookmark.name.data(), false, 0, bookmark_size ) )
		{
			directory::queued = bookmark.path;
		}

		ImGui::EndDisabled();
		ImGui::PopID();

		//if ( mouse_in_rect( ) || ImGui::IsItemFocused() )
		{
			ImGui::PushID( id++ );
			ImGui::SameLine();
			ImGui::PushStyleVar( ImGuiStyleVar_SelectableTextAlign, { 0.5f, 0.5f } );

			if ( ImGui::Selectable( "X", false, 0, remove_size ) )
			{
				remove_bookmark = bookmark_i;
			}

			ImGui::PopStyleVar();
			ImGui::PopID();
		}

		screen_cursor_pos.y += bookmark_size.y;
		bookmark_i++;
	}

	ImGui::EndChild();

	if ( remove_bookmark != SIZE_MAX )
	{
		app::config.bookmark.erase( app::config.bookmark.begin() + remove_bookmark );
		config_save();
	}
}


float g_file_info_height = 0;


void sidebar_draw_file_information( ImGuiStyle& style )
{
	ImGui::PushFont( font::normal_bold, style.FontSizeBase + 2.f );
	g_file_info_height = ImGui::GetFrameHeightWithSpacing();

	if ( !ImGui::CollapsingHeader( "File Information", ImGuiTreeNodeFlags_DefaultOpen ) )
	{
		ImGui::PopFont();
		return;
	}

	ImGui::PopFont();

	// if ( gallery::sorted_media.size() && gallery::last_selection.entry.type != e_media_type_none )
	if ( gallery::sorted_media.empty() )
		return;

	//ImGui::PushFont( font::normal_bold, style.FontSizeBase + 2.f );
	//ImGui::TextUnformatted( "File Information\n" );
	//ImGui::Separator();
	//ImGui::PopFont();

	// const media_entry_t& entry = gallery_item_get_media_entry( gallery_view_get_last_selected() );
	media_entry_t entry{};

	if ( gallery::selection.size() )
	{
		entry = gallery::selection.back().entry;
	}
	else
	{
		// find a hovered item
		for ( size_t i = 0; i < gallery::visible_item_count; i++ )
		{
			gallery_item_draw_t* item_draw = gallery::visible_item[ i ];

			if ( item_draw->item_hovered )
			{
				entry = directory::media_list[ item_draw->gallery_index ];
				break;
			}
		}
	}

	// none found, try last item that was selected?
	if ( entry.type == e_media_type_none )
		entry = gallery::last_selection.entry;

	// none found
	if ( entry.type == e_media_type_none )
		return;
	
	if ( !ImGui::BeginChild( "##file_info", {}, ImGuiChildFlags_FrameStyle | ImGuiChildFlags_AutoResizeY, 0 ) )
	{
		ImGui::EndChild();
		return;
	}

	ImGui::PushTextWrapPos();

	if ( entry.file.type & e_file_type_file )
	{
		fs::path dir = entry.file.path.parent_path();

		if ( !dir.empty() )
		{
			std::string dir_str = sys_path_to_string( dir );
			ImGui::TextUnformatted( dir_str.c_str() );
			ImGui::Separator();
		}
	}

	ImGui::TextUnformatted( entry.file.name.c_str() );

	ImGui::PopTextWrapPos();

	ImGui::Separator();

	// CONFIG_TODO
	if ( entry.file.type & e_file_type_file )
	{
		if ( entry.file.size < 1000000 )
		{
			ImGui::Text( "Size: %.3f KB", (float)entry.file.size / ( STORAGE_SCALE ) );
		}
		else
		{
			ImGui::Text( "Size: %.3f MB", (float)entry.file.size / ( STORAGE_SCALE * STORAGE_SCALE ) );
		}
	}

	char date_created[ DATE_TIME_BUFFER ]{};
	char date_mod[ DATE_TIME_BUFFER ]{};

	util_format_date_time( date_created, DATE_TIME_BUFFER, entry.file.date_created );
	util_format_date_time( date_mod, DATE_TIME_BUFFER, entry.file.date_mod );

	ImGui::Text( "Date Created: %s", date_created );
	ImGui::Text( "Date Modified: %s", date_mod );

	if ( entry.type == e_media_type_directory )
	{
		ImGui::TextUnformatted( "Type: Folder" );
	}
	else if ( entry.type == e_media_type_image )
	{
		ImGui::TextUnformatted( "Type: Image" );
	}
	else if ( entry.type == e_media_type_video )
	{
		ImGui::TextUnformatted( "Type: Video" );
	}

	// unsure what i want to do with this for the long term, but for now, im gonna have this be here
	if ( entry.file.type & e_file_type_file && entry.file.name.starts_with( "[twitter]" ) )
	{
		// if it's a twitter url, construct the original url from the post
		const char* start = entry.file.name.c_str();

		// offset past the start, skipping "[twitter] "
		start += 10;
		const char* last    = start;

		size_t      sep_len = strlen( "—" );

		// find the end of the artist name
		const char* find    = strchr( start, '—' );

		if ( find )
		{
			// new style, user id added, skip it
			// check if this is a date
			if ( ( find + 1 )[ 0 ] != '2' )
			{
				start = find + 1;  // offset the — character

				// find the end of the url string
				find  = strchr( start, '—' );
			}

			const char* check = find + 11;

			std::string artist_name( start, ( find - 2 ) - start );

			start = find + 14;  // offset the date and — character

			// find the end of the url string
			find  = strchr( start, '—' );

			// std::string artist_name( start, ( find - 2 ) - start );
			//
			// start = find + 14; // offset the date and — character
			//
			// // find the end of the url string
			// find  = strchr( start, '—' );

			if ( find )
			{
				// length of 19, is it always like that?
				std::string post_id( start, ( find - 2 ) - start );

				char        post_url[ 512 ]{};
				snprintf( post_url, 512, "https://x.com/%s/status/%s", artist_name.c_str(), post_id.c_str() );

				ImGui::Separator();

				ImGui::TextLinkOpenURL( post_url, post_url );

				if ( ImGui::Button( "Copy URL" ) )
				{
					ImGui::SetClipboardText( post_url );
					push_notification( "URL Copied" );
				}
			}
		}
	}

	g_file_info_height += ImGui::GetWindowHeight() + style.ItemSpacing.y;

	ImGui::EndChild();
}


bool gallery_begin_tab_content()
{
	return ImGui::BeginChild( "##sidebar_tab_content", {}, 0, ImGuiWindowFlags_NoScrollbar );
}


void gallery_view_draw_sidebar()
{
	int window_width, window_height;
	SDL_GetWindowSize( app::window, &window_width, &window_height );

	ImVec2      region_avail = ImGui::GetContentRegionAvail();
	ImGuiStyle& style        = ImGui::GetStyle();

	ImGui::SetNextWindowSizeConstraints(
	  { 40.f, region_avail.y + style.WindowPadding.y },
	  { (float)window_width / 2.f, region_avail.y + style.WindowPadding.y } );

	if ( !ImGui::BeginChild( "##gallery_sidebar", {}, ImGuiChildFlags_ResizeX | ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar ) )
	{
		//ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, { 0, 0 } );
		ImGui::EndChild();
		//ImGui::PopStyleVar();
		return;
	}

	if ( ImGui::BeginTabBar( "##sidebar_tabs" ) )
	{
		if ( ImGui::BeginTabItem( "Filesystem" ) )
		{
			if ( gallery_begin_tab_content() )
			{
				sidebar_draw_bookmarks( style );
				dir_tree_draw( style );
				sidebar_draw_file_information( style );
			}

			ImGui::EndChild();
			ImGui::EndTabItem();
		}

		if ( app::config.dev_mode )
		{
			if ( ImGui::BeginTabItem( "Tags" ) )
			{
				if ( gallery_begin_tab_content() )
				{
					ImGui::PushFont( font::normal_bold, style.FontSizeBase + 2.f );

					ImGui::TextUnformatted( "Tag Databases" );
					ImGui::Separator();

					ImGui::PopFont();

					if ( ImGui::BeginListBox( "##TagDatabases" ) )
					{
						ImGui::EndListBox();
					}
				}

				ImGui::EndChild();
				ImGui::EndTabItem();
			}
		}

		if ( ImGui::BeginTabItem( "Settings" ) )
		{
			if ( gallery_begin_tab_content() )
			{
				settings_draw();
			}

			ImGui::EndChild();
			ImGui::EndTabItem();
		}

		if ( app::config.dev_mode )
		{
			if ( ImGui::BeginTabItem( "Style Editor" ) )
			{
				if ( gallery_begin_tab_content() )
				{
					ImGui::ShowStyleEditor();
				}

				ImGui::EndChild();
				ImGui::EndTabItem();
			}

			if ( ImGui::BeginTabItem( "Stats" ) )
			{
				if ( gallery_begin_tab_content() )
				{
					thumbnail_cache_debug_draw();
					ImGui::Separator();
					mem_draw_debug_ui();
				}

				ImGui::EndChild();
				ImGui::EndTabItem();
			}
		}
	}

	ImGui::EndTabBar();

	//ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, { 0, 0 } );
	ImGui::EndChild();
	//ImGui::PopStyleVar();
}

