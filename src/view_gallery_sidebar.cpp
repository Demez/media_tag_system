#include "main.h"

#include "imgui_internal.h"


static char g_folder_buf[ 512 ]{};
bool        g_do_search = false;


#if 0
bool        SliderStepInt( const char* label, int* value, const int step_size, const int min_size, const int max_size, const char* format, ImGuiSliderFlags flags )
{
	ImGuiWindow* window = ImGui::GetCurrentWindow();
	if ( window->SkipItems )
		return false;

	ImGuiContext&     g          = *GImGui;
	const ImGuiStyle& style      = g.Style;
	const ImGuiID     id         = window->GetID( label );
	const float       w          = ImGui::CalcItemWidth();

	const ImVec2      label_size = ImGui::CalcTextSize( label, NULL, true );
	const ImRect      frame_bb( window->DC.CursorPos, window->DC.CursorPos + ImVec2( w, label_size.y + style.FramePadding.y * 2.0f ) );
	const ImRect      total_bb( frame_bb.Min, frame_bb.Max + ImVec2( label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f, 0.0f ) );

	const bool        temp_input_allowed = ( flags & ImGuiSliderFlags_NoInput ) == 0;
	ImGui::ItemSize( total_bb, style.FramePadding.y );
	if ( !ImGui::ItemAdd( total_bb, id, &frame_bb, temp_input_allowed ? ImGuiItemFlags_Inputable : 0 ) )
		return false;

	// Default format string when passing NULL
	if ( format == NULL )
		format = ImGui::DataTypeGetInfo( data_type )->PrintFmt;

	const bool hovered              = ImGui::ItemHoverable( frame_bb, id, g.LastItemData.ItemFlags );
	bool       temp_input_is_active = temp_input_allowed && ImGui::TempInputIsActive( id );
	if ( !temp_input_is_active )
	{
		// Tabbing or CTRL+click on Slider turns it into an input box
		const bool clicked     = hovered && ImGui::IsMouseClicked( 0, ImGuiInputFlags_None, id );
		const bool make_active = ( clicked || g.NavActivateId == id );
		if ( make_active && clicked )
			ImGui::SetKeyOwner( ImGuiKey_MouseLeft, id );
		if ( make_active && temp_input_allowed )
			if ( ( clicked && g.IO.KeyCtrl ) || ( g.NavActivateId == id && ( g.NavActivateFlags & ImGuiActivateFlags_PreferInput ) ) )
				temp_input_is_active = true;

		// Store initial value (not used by main lib but available as a convenience but some mods e.g. to revert)
		if ( make_active )
			memcpy( &g.ActiveIdValueOnActivation, p_data, ImGui::DataTypeGetInfo( data_type )->Size );

		if ( make_active && !temp_input_is_active )
		{
			ImGui::SetActiveID( id, window );
			ImGui::SetFocusID( id, window );
			ImGui::FocusWindow( window );
			g.ActiveIdUsingNavDirMask |= ( 1 << ImGuiDir_Left ) | ( 1 << ImGuiDir_Right );
		}
	}

	if ( temp_input_is_active )
	{
		// Only clamp CTRL+Click input when ImGuiSliderFlags_ClampOnInput is set (generally via ImGuiSliderFlags_AlwaysClamp)
		const bool clamp_enabled = ( flags & ImGuiSliderFlags_ClampOnInput ) != 0;
		return ImGui::TempInputScalar( frame_bb, id, label, data_type, p_data, format, clamp_enabled ? p_min : NULL, clamp_enabled ? p_max : NULL );
	}

	// Draw frame
	const ImU32 frame_col = ImGui::GetColorU32( g.ActiveId == id ? ImGuiCol_FrameBgActive : hovered ? ImGuiCol_FrameBgHovered
	                                                                                         : ImGuiCol_FrameBg );
	ImGui::RenderNavCursor( frame_bb, id );
	ImGui::RenderFrame( frame_bb.Min, frame_bb.Max, frame_col, true, g.Style.FrameRounding );

	// Slider behavior
	ImRect     grab_bb;
	const bool value_changed = ImGui::SliderBehavior( frame_bb, id, data_type, p_data, p_min, p_max, format, flags, &grab_bb );
	if ( value_changed )
		ImGui::MarkItemEdited( id );

	// Render grab
	if ( grab_bb.Max.x > grab_bb.Min.x )
		window->DrawList->AddRectFilled( grab_bb.Min, grab_bb.Max, ImGui::GetColorU32( g.ActiveId == id ? ImGuiCol_SliderGrabActive : ImGuiCol_SliderGrab ), style.GrabRounding );

	// Display value using user-provided display format so user can add prefix/suffix/decorations to the value.
	char        value_buf[ 64 ];
	const char* value_buf_end = value_buf + ImGui::DataTypeFormatString( value_buf, IM_ARRAYSIZE( value_buf ), data_type, p_data, format );
	if ( g.LogEnabled )
		ImGui::LogSetNextTextDecoration( "{", "}" );
	ImGui::RenderTextClipped( frame_bb.Min, frame_bb.Max, value_buf, value_buf_end, NULL, ImVec2( 0.5f, 0.5f ) );

	if ( label_size.x > 0.0f )
		ImGui::RenderText( ImVec2( frame_bb.Max.x + style.ItemInnerSpacing.x, frame_bb.Min.y + style.FramePadding.y ), label );

	IMGUI_TEST_ENGINE_ITEM_INFO( id, label, g.LastItemData.StatusFlags | ( temp_input_allowed ? ImGuiItemStatusFlags_Inputable : 0 ) );
	return value_changed;
}
#endif


static void draw_vertical_separator( ImDrawList* draw_list, ImGuiStyle& style )
{
	ImVec2 cursor_pos = ImGui::GetCursorPos();

	if ( style.WindowBorderSize > 0 )
	{
		ImColor border_col   = style.Colors[ ImGuiCol_Border ];
		ImVec2  region_avail = ImGui::GetContentRegionAvail();
		float  window_height = ImGui::GetWindowHeight();

		ImVec2  line_start   = { cursor_pos.x, 0 };
		ImVec2  line_end     = cursor_pos;

		// line_start.y -= style.FramePadding.y;
		line_end.y += window_height + style.FramePadding.y;

		draw_list->AddLine( line_start, line_end, border_col, style.WindowBorderSize );

		// ImGui::SetCursorPosX( cursor_pos.x + style.ItemSpacing.x );
		ImGui::SetCursorPosX( cursor_pos.x + style.WindowBorderSize + style.ItemSpacing.x );
	}
	else
	{
		ImGui::SetCursorPosX( cursor_pos.x + style.ItemSpacing.x );
	}
}


int gallery_view_draw_header()
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

	if ( ImGui::Button( "Sidebar" ) )
	{
		gallery::sidebar_draw = !gallery::sidebar_draw;
	}

	// ImGui::Selectable( "Sidebar", &gallery::sidebar_draw );
	ImGui::SameLine();
	//ImGui::Spacing();
	//ImGui::SameLine();

	draw_vertical_separator( draw_list, style );

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

	static bool  was_in_path_edit = false;
	static bool  path_edit_hover  = false;
	static float bar_width        = 0.f;

	if ( !directory::path_edit )
	{
		was_in_path_edit  = false;

		ImGuiStyle& style = ImGui::GetStyle();
		ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, { ImGui::GetFontSize() / 8.f, style.ItemSpacing.y } );
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
			bool   item_hovered      = false;

			size_t id = 1;
			ImVec2 item_size{};
			for ( size_t i = 0; i < directory::path_chunks.size(); i++ )
			{
				ImGui::PushID( id++ );

				ImVec2 text_size = ImGui::CalcTextSize( directory::path_chunks[ i ].c_str() );
				item_size = ImGui::CalcItemSize( text_size, 0, 0 );
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

			float  region_avail           = ImGui::GetContentRegionAvail().x;
			float  region_avail_y         = ImGui::GetContentRegionAvail().y;

			ImVec2 cursor_pos             = ImGui::GetCursorPos();
			region_avail                  = 500 - ( cursor_pos.x + style.FramePadding.x );

			region_avail                  = std::max( ImGui::GetFontSize() * 3.f, region_avail );

			ImVec2      cursor_screen_pos = ImGui::GetCursorScreenPos();
			ImVec2      window_pos        = ImGui::GetWindowPos();

			// ImVec2      window_cursor_pos( window_pos.x + cursor_base_pos.x, ( window_pos.y + cursor_base_pos.y ) );
			ImVec2      window_cursor_pos( window_pos.x + cursor_pos.x, cursor_pos.y );
			ImVec2      global_item_size = ImVec2( window_cursor_pos.x + region_avail + style.FramePadding.x, window_cursor_pos.y + ImGui::GetWindowHeight() );

			ImColor     main_bg_color     = item_hovered ? style.Colors[ ImGuiCol_ButtonActive ] : style.Colors[ ImGuiCol_ButtonActive ];

			// draw_list->AddRectFilled( child_size_min, child_size_max, main_bg_color, style.ChildRounding, ImDrawFlags_RoundCornersAll );

			bool rect_hovered = ImGui::IsMouseHoveringRect( cursor_screen_pos, global_item_size, true );

			if ( path_edit_hover )
				ImGui::PopStyleColor();

			if ( rect_hovered && !item_hovered )
			{
				path_edit_hover = true;
				set_frame_draw( 2 );

				if ( ImGui::IsMouseClicked( ImGuiMouseButton_Left ) )
					directory::path_edit = true;
			}
			else
			{
				if ( path_edit_hover )
				{
					path_edit_hover = false;
					set_frame_draw( 2 );
				}
			}

			ImGui::Dummy( { region_avail, item_size.y } );
			ImGui::PopStyleVar();
		}

		bar_width = ImGui::GetWindowWidth();

		ImGui::EndChild();
	}
	else
	{
		ImGui::SetNextItemWidth( bar_width );

		if ( !was_in_path_edit )
		{
			ImGui::SetKeyboardFocusHere();
		}

		if ( ImGui::InputText( "##directory", g_folder_buf, 512, ImGuiInputTextFlags_EnterReturnsTrue ) )
		{
			if ( fs_is_dir( g_folder_buf ) )
				directory::queued = g_folder_buf;
			else
				snprintf( g_folder_buf, 512, directory::path.string().c_str() );

			directory::path_edit = false;
		}

		if ( was_in_path_edit && !ImGui::IsItemFocused() )
		{
			directory::path_edit = false;
		}

		if ( !was_in_path_edit )
		{
			was_in_path_edit = true;
		}
	}

	ImGui::SameLine();

	// Enter returns true doesn't work because of gallery view hooking that input currently, need to add a check later for if focused in text input
	if ( ImGui::ArrowButton( "##nav_enter_path", ImGuiDir_Right ) )
	{
		directory::queued        = g_folder_buf;
		directory::folder_reload = true;
		directory::path_edit     = false;
	}

	ImGui::SameLine();
	draw_vertical_separator( draw_list, style );

	ImGui::TextUnformatted( "Search" );

	ImGui::SameLine();

	// Press Ctrl+F to focus the search text input
	if ( !ImGui::GetIO().WantTextInput )
	{
		if ( ImGui::IsKeyChordPressed( ImGuiMod_Ctrl | ImGuiKey_F ) )
			ImGui::SetKeyboardFocusHere();
	}

	if ( strlen( gallery::search ) == 0 )
	{
		gallery_view_set_selection( gallery::cursor );
	}

	// if ( ImGui::InputText( "##search", gallery::search, 512, ImGuiInputTextFlags_EnterReturnsTrue ) )
	if ( ImGui::InputText( "##search", gallery::search, 512 ) )
	{
		g_do_search = true;
		gallery_view_dir_change( true );
	}

	ImGui::SameLine();
	draw_vertical_separator( draw_list, style );

	if ( ImGui::Checkbox( "Recursive", &directory::recursive ) )
	{
		directory::queued        = g_folder_buf;
		directory::folder_reload = true;

		if ( !directory::recursive )
		{
			gallery::sorted_media.clear();
			gallery::cursor = 0;
		}
	}

	ImGui::SameLine();
	draw_vertical_separator( draw_list, style );

	// ---------------------------------------------------------------------------------
	// Center Spacing, rest is aligned to the right

	ImVec2 region_avail    = ImGui::GetContentRegionAvail();

	int    space_needed    = 100;

	ImVec2 zoom_size         = ImGui::CalcTextSize( "Zoom" );
	ImVec2 sort_size         = ImGui::CalcTextSize( "Sort Mode" );
	ImVec2 filter_size       = ImGui::CalcTextSize( "Quick Filter" );
	ImVec2 sort_entry_size   = ImGui::CalcTextSize( "Date Modified - New to Old" );
	ImVec2 filter_entry_size = ImGui::CalcTextSize( "Folders" );

	space_needed += zoom_size.x + sort_size.x + filter_size.x;
	space_needed += style.ItemSpacing.x * 2;  // Zoom Text
	space_needed += style.WindowBorderSize;   // Separator
	space_needed += style.ItemSpacing.x * 2;  // Sort Mode Text
	space_needed += style.ItemSpacing.x * 2;  // Fiter Size Text
	space_needed += style.ItemSpacing.x;      // Padding
	
	int sep_space_needed = 0;

	if ( style.WindowBorderSize )
		sep_space_needed = style.ItemSpacing.x + style.WindowBorderSize;

	// estimate
	int arrow_size = style.FontSizeBase;

	int sort_width   = sort_entry_size.x + arrow_size + ( style.FramePadding.x * 3 ) + ( style.FramePadding.y * 2 );
	int filter_width = filter_entry_size.x + arrow_size + ( style.FramePadding.x * 3 ) + ( style.FramePadding.y * 2 );

	space_needed += sort_width;
	space_needed += filter_width;

	// ??
	// space_needed += 60.f;
	space_needed += style.ItemSpacing.x;

	if ( ( space_needed + sep_space_needed ) < region_avail.x )
	{
		ImGui::SetCursorPosX( ImGui::GetCursorPosX() + ( region_avail.x - space_needed ) );

		draw_vertical_separator( draw_list, style );

		// ImColor border_col   = ImVec4( 1, 0, 0, 1 );
		// ImVec2  cursor_pos   = ImGui::GetCursorPos();
		// ImVec2  region_avail = ImGui::GetContentRegionAvail();
		// 
		// cursor_pos.x += space_needed;
		// 
		// ImVec2  line_start   = cursor_pos;
		// ImVec2  line_end     = cursor_pos;
		// 
		// line_start.y -= style.FramePadding.y;
		// line_end.y += region_avail.y + style.FramePadding.y;
		// 
		// draw_list->AddLine( line_start, line_end, border_col, style.WindowBorderSize );
	}

	// ---------------------------------------------------------------------------------

	ImGui::TextUnformatted( "Zoom" );
	ImGui::SameLine();

	ImGui::SetNextItemWidth( 100 );

	// if ( ImGui::SliderInt( "Zoom", &gallery::item_size, gallery::item_size_min, gallery::item_size_max ) )
	// if ( ImGui::DragInt( "##zoom", &gallery::item_size, 10, gallery::item_size_min, gallery::item_size_max, "Zoom - %d px" ) )
	if ( ImGui::SliderScalar( "##zoom", ImGuiDataType_U32, &gallery::item_size, &gallery::item_size_min, &gallery::item_size_max, "%d px" ) )
	{
		gallery_view_reset_text_size();

		if ( !app::config.thumbnail_use_fixed_size )
			thumbnail_clear_cache();
	}

	ImGui::SameLine();
	draw_vertical_separator( draw_list, style );

	ImGui::TextUnformatted( "Quick Filter" );
	ImGui::SameLine();

	ImGui::SetNextItemWidth( filter_width );

	ImGui::BeginDisabled();

	// this could be a check box thing for toggling what you want to view, all enabled by default, but that is slower
	if ( ImGui::BeginCombo( "##quick_filter", "None", 0 ) )
	{
		if ( ImGui::Selectable( "None", false ) )
		{
		}

		if ( ImGui::Selectable( "Images", false ) )
		{
		}

		if ( ImGui::Selectable( "Videos", false ) )
		{
		}

		if ( ImGui::Selectable( "Folders", false ) )
		{
		}

		ImGui::EndCombo();
	}

	ImGui::EndDisabled();

	ImGui::SameLine();
	draw_vertical_separator( draw_list, style );

	ImGui::TextUnformatted( "Sort Mode" );
	ImGui::SameLine();

	const char* combo_preview_value = g_gallery_sort_mode_str[ gallery::sort_mode ];

	ImGui::SetNextItemWidth( sort_width );

	if ( ImGui::BeginCombo( "##sort_mode", combo_preview_value, 0 ) )
	{
		for ( int n = 0; n < e_gallery_sort_mode_count; n++ )
		{
			const bool is_selected = ( gallery::sort_mode == n );

			if ( ImGui::Selectable( g_gallery_sort_mode_str[ n ], is_selected ) )
			{
				gallery::sort_mode        = (e_gallery_sort_mode)n;
				gallery::sort_mode_update = true;
			}

			// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
			if ( is_selected )
			{
				ImGui::SetItemDefaultFocus();
			}
		}

		ImGui::EndCombo();
	}

	// if ( ImGui::Combo( "Sort Mode", &item_current, items, IM_ARRAYSIZE( items ) ) )
	// {
	// }

	// ImGui::SameLine();

	//ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, { 0, 0 } );

	int im_window_height = ImGui::GetWindowHeight();

	if ( ImGui::IsMouseHoveringRect( { 0, 0 }, { (float)window_width, (float)im_window_height } ) && !ImGui::IsAnyItemHovered() )
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


void sidebar_draw_filesystem()
{
	// get mounted drives
	// TODO: MOVE ME TO STARTUP, AND CHECK FOR NEW DRIVES BEING MOUNTED/UNMOUNTED ONCE IN A WHILE
	static bool                       first_run = true;
	static std::vector< std::string > drives;

	if ( first_run )
	{
		sys_get_drives( drives );
		first_run = false;
	}

	u32 drive_i = 0;
	for ( const std::string& drive : drives )
	{
		if ( ImGui::CollapsingHeader( drive.c_str() ) )
		{
			ImGui::PushID( drive_i + 1 );

			// ImGuiTreeNodeFlags_SpanAvailWidth
			if ( ImGui::TreeNodeEx( "test", ImGuiTreeNodeFlags_SpanAvailWidth ) )
			{
				if ( ImGui::TreeNodeEx( "test", ImGuiTreeNodeFlags_SpanAvailWidth ) )
				{
					if ( ImGui::TreeNodeEx( "test", ImGuiTreeNodeFlags_SpanAvailWidth ) )
					{
						if ( ImGui::TreeNodeEx( "test", ImGuiTreeNodeFlags_SpanAvailWidth ) )
						{
							ImGui::TreePop();
						}
						ImGui::TreePop();
					}

					ImGui::TreePop();
				}

				if ( ImGui::TreeNodeEx( "test2", ImGuiTreeNodeFlags_SpanAvailWidth ) )
				{
					ImGui::TreePop();
				}

				ImGui::TreePop();
			}

			ImGui::PopID();
		}

		drive_i++;
	}
}


void gallery_view_draw_sidebar()
{
	int window_width, window_height;
	SDL_GetWindowSize( app::window, &window_width, &window_height );

	ImVec2      region_avail = ImGui::GetContentRegionAvail();
	ImGuiStyle& style        = ImGui::GetStyle();

	ImVec2      cursor_pos   = ImGui::GetCursorPos();
	// ImGui::SetCursorPosX( 0.f );

	// weirdly sized still
	// ImGui::SetNextWindowPos( { 0, 32.f } );
	ImGui::SetNextWindowSizeConstraints(
	  { 40.f, region_avail.y + style.ItemSpacing.y + style.ItemSpacing.y },
	  { (float)window_width / 2.f, region_avail.y + style.ItemSpacing.y + style.ItemSpacing.y } );

	if ( !ImGui::BeginChild( "##gallery_sidebar", {}, ImGuiChildFlags_ResizeX | ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar ) )
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
			ImGui::PushFont( font::normal_bold, style.FontSizeBase + 2.f );

			if ( ImGui::CollapsingHeader( "Bookmarks", ImGuiTreeNodeFlags_DefaultOpen ) )
			{
				ImGui::PopFont();

				if ( ImGui::Button( "Add Current Directory" ) )
				{
					app::config.bookmark.emplace_back( directory::path.filename().string(), directory::path.string(), true );
				}

				ImGui::PushItemWidth( -1 );

				if ( ImGui::BeginListBox( "##Bookmarks" ) )
				{
					// TODO: format like this?
					//
					// =================================================
					// | drawings                                    X |
					// =================================================
					//

					u32 id = 1;
					for ( const bookmark_t& bookmark : app::config.bookmark )
					{
						ImGui::PushID( id++ );
						ImGui::BeginDisabled( !bookmark.valid );

						if ( ImGui::Selectable( bookmark.name.data() ) )
						{
							directory::queued = bookmark.path;
						}

						ImGui::EndDisabled();
						ImGui::PopID();
					}

					ImGui::EndListBox();
				}

				ImGui::PopItemWidth();
			}
			else
			{
				ImGui::PopFont();
			}

			ImGui::PushFont( font::normal_bold, style.FontSizeBase + 2.f );

			ImGui::TextUnformatted( "Files" );
			ImGui::Separator();

			ImGui::PopFont();

			sidebar_draw_filesystem();

			ImGui::PushFont( font::normal_bold, style.FontSizeBase + 2.f );
			ImGui::TextUnformatted( "File Information\n" );
			ImGui::Separator();
			ImGui::PopFont();

			{
				if ( gallery::sorted_media.size() )
				{
					if ( ImGui::BeginChild( "##file_info", {}, ImGuiChildFlags_FrameStyle | ImGuiChildFlags_AutoResizeY, 0 ) )
					{
						const media_entry_t& entry = gallery_item_get_media_entry( gallery::cursor );

						ImGui::PushTextWrapPos();
						ImGui::TextUnformatted( entry.filename.c_str() );
						ImGui::PopTextWrapPos();

						ImGui::Separator();

						// CONFIG_TODO
						if ( entry.file.size < 1000000 )
						{
							ImGui::Text( "Size: %.3f KB", (float)entry.file.size / ( STORAGE_SCALE ) );
						}
						else
						{
							ImGui::Text( "Size: %.3f MB", (float)entry.file.size / ( STORAGE_SCALE * STORAGE_SCALE ) );
						}

						char date_created[ DATE_TIME_BUFFER ]{};
						char date_mod[ DATE_TIME_BUFFER ]{};

						util_format_date_time( date_created, DATE_TIME_BUFFER, entry.file.date_created );
						util_format_date_time( date_mod, DATE_TIME_BUFFER, entry.file.date_mod );

						ImGui::Text( "Date Created: %s", date_created );
						ImGui::Text( "Date Modified: %s", date_mod );

						if ( get_media_type() == e_media_type_directory )
						{
							ImGui::TextUnformatted( "Type: Folder" );
						}
						else if ( get_media_type() == e_media_type_image )
						{
							ImGui::TextUnformatted( "Type: Image" );
						}
						else if ( get_media_type() == e_media_type_video )
						{
							ImGui::TextUnformatted( "Type: Video" );
						}

						// unsure what i want to do with this for the long term, but for now, im gonna have this be here
						if ( entry.filename.starts_with( "[twitter]" ) )
						{
							// if it's a twitter url, construct the original url from the post
							const char* start = entry.filename.c_str();
							
							// offset past the start, skipping "[twitter] "
							start += 10;
							const char* last  = start;

							size_t      sep_len = strlen( "—" );

							// find the end of the artist name
							const char* find = strchr( start, '—' );

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

									char post_url[ 512 ]{};
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
					}

					ImGui::EndChild();
				}
			}

			#if 0
			ImGui::PushFont( font::normal_bold, style.FontSizeBase + 2.f );
			ImGui::TextUnformatted( "History\n" );
			ImGui::Separator();
			ImGui::PopFont();

			u32 id = 1;
			for ( size_t i = directory::media_history.size(); i > 0; i-- )
			{
				const std::string& entry = directory::media_history[ i - 1 ];

				ImGui::PushID( id++ );

				if ( fs_is_file( entry.c_str() ) )
				{
					char* filename = fs_get_filename( entry.c_str(), entry.size() );

					if ( ImGui::Selectable( filename ) )
					{
						directory::queued = entry;
					}

					ch_free_str( filename );
				}
				else
				{
					if ( ImGui::Selectable( entry.c_str() ) )
					{
						directory::queued = entry;
					}
				}
				

				ImGui::PopID();
			}
			#endif

			ImGui::EndTabItem();
		}

		if ( ImGui::BeginTabItem( "Tags" ) )
		{
			ImGui::PushFont( font::normal_bold, style.FontSizeBase + 2.f );

			ImGui::TextUnformatted( "Tag Databases" );
			ImGui::Separator();

			ImGui::PopFont();

			if ( ImGui::BeginListBox( "##TagDatabases" ) )
			{
				ImGui::EndListBox();
			}

			ImGui::EndTabItem();
		}

		if ( ImGui::BeginTabItem( "Style Editor" ) )
		{
			ImGui::ShowStyleEditor();
			ImGui::EndTabItem();
		}

		if ( ImGui::BeginTabItem( "Settings" ) )
		{
			if ( ImGui::Checkbox( "Gallery - Fixed Thumbnail Sizes", &app::config.thumbnail_use_fixed_size ) )
			{
				thumbnail_clear_cache();
			}

			if ( ImGui::Checkbox( "Gallery - Show Filenames", &app::config.gallery_show_filenames ) )
			{
				gallery_view_reset_text_size();
			}

			ImGui::Checkbox( "Always Draw", &app::config.always_draw );

			static float dpi_scale = 0.f;
			dpi_scale              = app::dpi;
			if ( ImGui::InputFloat( "DPI Override", &dpi_scale, 0.25, 0.5, "%.3f" ) )
			{
				update_dpi( dpi_scale );
			}

			ImGui::EndTabItem();
		}

		if ( ImGui::BeginTabItem( "Stats" ) )
		{
			thumbnail_cache_debug_draw();

			ImGui::Separator();

			mem_draw_debug_ui();

			ImGui::EndTabItem();
		}
	}

	ImGui::EndTabBar();

	//ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, { 0, 0 } );
	ImGui::EndChild();
	//ImGui::PopStyleVar();
}

