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

	// gallery::sort_mode_update = true;
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

		size_t id           = 1;
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
		float   space_avail2      = window_width - cursor_screen_pos.x;

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
		directory::queued        = directory::path;
		directory::folder_reload = true;
	}

	ImGui::SameLine();

	draw_vertical_separator( draw_list, style );

	// ---------------------------------------------------------------------------------
	// Center Spacing, rest is aligned to the right

	ImVec2       region_avail     = ImGui::GetContentRegionAvail();

	float        space_needed     = 0;
	int          search_box_size  = 150;

	ImVec2       search_size      = ImGui::CalcTextSize( "Search" );
	ImVec2       recursive_size   = ImGui::CalcTextSize( "Recursive" );

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
	int arrow_size = style.FontSizeBase;

	// ??
	// space_needed += 60.f;
	//space_needed += style.ItemSpacing.x;
	
	// ---------------------------------------------------------------------------------

	gallery_header_draw_path( region_avail, space_needed );

	ImGui::SameLine();

	// Enter returns true doesn't work because of gallery view hooking that input currently, need to add a check later for if focused in text input
	if ( ImGui::ArrowButton( "##nav_enter_path", ImGuiDir_Right ) )
	{
		directory::queued        = g_folder_buf;
		directory::folder_reload = true;
		directory::path_edit     = false;
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

			if ( strlen( gallery::search ) == 0 )
			{
				// gallery_view_set_selection( gallery::cursor );
			}

			ImGui::SetNextItemWidth( search_space_avail );

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
		gallery::content_area_resized = true;
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

	int    sort_width   = sort_entry_size.x + arrow_size + ( style.FramePadding.x * 3 ) + ( style.FramePadding.y * 2 );
	int    filter_width = filter_entry_size.x + arrow_size + ( style.FramePadding.x * 3 ) + ( style.FramePadding.y * 2 );

#if 0
	ImVec2 region_avail    = ImGui::GetContentRegionAvail();

	int    space_needed    = 100;

	ImVec2 zoom_size         = ImGui::CalcTextSize( "Zoom" );

	space_needed += zoom_size.x + sort_size.x + filter_size.x;
	space_needed += style.ItemSpacing.x * 2;  // Zoom Text
	space_needed += style.WindowBorderSize;   // Separator
	space_needed += style.ItemSpacing.x * 2;  // Sort By Text
	space_needed += style.ItemSpacing.x * 2;  // Fiter Size Text
	space_needed += style.ItemSpacing.x;      // Padding
	
	int sep_space_needed = 0;

	if ( style.WindowBorderSize )
		sep_space_needed = style.ItemSpacing.x + style.WindowBorderSize;

	// estimate
	int arrow_size = style.FontSizeBase;


	space_needed += sort_width;
	space_needed += filter_width;

	// ??
	// space_needed += 60.f;
	space_needed += style.ItemSpacing.x;

	if ( ( space_needed + sep_space_needed ) < region_avail.x )
	{
		//ImGui::SetCursorPosX( ImGui::GetCursorPosX() + ( region_avail.x - space_needed ) );

		//draw_vertical_separator( draw_list, style );

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
#endif

	// ---------------------------------------------------------------------------------

	ImGui::TextUnformatted( "Zoom" );
	ImGui::SameLine();

	ImGui::SetNextItemWidth( 190 );

	SDL_MouseButtonFlags mouse_state     = SDL_GetMouseState( 0, 0 );
	static bool          check_left_mouse = false;

	// if ( ImGui::SliderInt( "Zoom", &gallery::item_size, gallery::item_size_min, gallery::item_size_max ) )
	// if ( ImGui::DragInt( "##zoom", &gallery::item_size, 10, gallery::item_size_min, gallery::item_size_max, "Zoom - %d px" ) )
	if ( ImGui::SliderScalar( "##zoom", ImGuiDataType_U32, &gallery::item_size, &gallery::item_size_min, &gallery::item_size_max, "%d px" ) )
	{
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
			gallery::filter           = 0;
			gallery::sort_mode_update = true;
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

	// if ( ImGui::Combo( "Sort Mode", &item_current, items, IM_ARRAYSIZE( items ) ) )
	// {
	// }

	// ImGui::SameLine();

	//ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, { 0, 0 } );

	// if ( gallery::selection.size() )
	{
		ImGui::SameLine( 0, 0 );
		ImVec2 region_avail = ImGui::GetContentRegionAvail();

		char   buf[ 256 ]{};
		// snprintf( buf, 256, "%zu File%s Selected", gallery::selection.size(), gallery::selection.size() == 1 ? "" : "s" );
		if ( gallery::selection.size() )
		{
			snprintf( buf, 256, "%zu Selected | %zu File%s", gallery::selection.size(), gallery::sorted_media.size(), gallery::sorted_media.size() == 1 ? "" : "s" );
		}
		else
		{
			snprintf( buf, 256, "%zu File%s", gallery::sorted_media.size(), gallery::sorted_media.size() == 1 ? "" : "s" );
		}

		ImVec2 text_size = ImGui::CalcTextSize( buf );

		ImVec2 spacing_size( region_avail.x - ( text_size.x + style.ItemSpacing.x ), text_size.y );
		spacing_size.x = std::max( spacing_size.x, style.ItemSpacing.x );

		ImGui::Dummy( spacing_size );
		ImGui::SameLine( 0, 0 );

		ImGui::TextUnformatted( buf );
		ImGui::SameLine( 0, 0 );
	}

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


static bool g_dir_change_from_dir_tree = false;


extern void TreeNodeStoreStackData( ImGuiTreeNodeFlags flags, float x1 );


// oh my god bruh
bool TreeNodeBehaviorStupid( ImGuiID id, ImGuiTreeNodeFlags flags, const char* label, const char* label_end, bool& label_pressed )
{
	using namespace ImGui;

	ImGuiWindow* window = GetCurrentWindow();
	if ( window->SkipItems )
		return false;

	ImGuiContext&     g             = *GImGui;
	const ImGuiStyle& style         = g.Style;
	const bool        display_frame = ( flags & ImGuiTreeNodeFlags_Framed ) != 0;
	const ImVec2      padding       = ( display_frame || ( flags & ImGuiTreeNodeFlags_FramePadding ) ) ? style.FramePadding : ImVec2( style.FramePadding.x, ImMin( window->DC.CurrLineTextBaseOffset, style.FramePadding.y ) );

	if ( !label_end )
		label_end = FindRenderedTextEnd( label );

	const ImVec2 label_size             = CalcTextSize( label, label_end, false );

	const float  text_offset_x          = g.FontSize + ( display_frame ? padding.x * 3 : padding.x * 2 );  // Collapsing arrow width + Spacing
	const float  text_offset_y          = ImMax( padding.y, window->DC.CurrLineTextBaseOffset );           // Latch before ItemSize changes it
	const float  text_width             = g.FontSize + label_size.x + padding.x * 2;                       // Include collapsing arrow

	// We vertically grow up to current line height up the typical widget height.
	const float  frame_height           = ImMax( ImMin( window->DC.CurrLineSize.y, g.FontSize + style.FramePadding.y * 2 ), label_size.y + padding.y * 2 );
	const bool   span_all_columns       = ( flags & ImGuiTreeNodeFlags_SpanAllColumns ) != 0 && ( g.CurrentTable != NULL );
	const bool   span_all_columns_label = ( flags & ImGuiTreeNodeFlags_LabelSpanAllColumns ) != 0 && ( g.CurrentTable != NULL );
	ImRect       frame_bb;
	frame_bb.Min.x = span_all_columns ? window->ParentWorkRect.Min.x : ( flags & ImGuiTreeNodeFlags_SpanFullWidth ) ? window->WorkRect.Min.x
	                                                                                                                : window->DC.CursorPos.x;
	frame_bb.Min.y = window->DC.CursorPos.y;
	frame_bb.Max.x = span_all_columns ? window->ParentWorkRect.Max.x : ( flags & ImGuiTreeNodeFlags_SpanLabelWidth ) ? window->DC.CursorPos.x + text_width + padding.x
	                                                                                                                 : window->WorkRect.Max.x;
	frame_bb.Max.y = window->DC.CursorPos.y + frame_height;
	if ( display_frame )
	{
		const float outer_extend = IM_TRUNC( window->WindowPadding.x * 0.5f );  // Framed header expand a little outside of current limits
		frame_bb.Min.x -= outer_extend;
		frame_bb.Max.x += outer_extend;
	}

	ImVec2 text_pos( window->DC.CursorPos.x + text_offset_x, window->DC.CursorPos.y + text_offset_y );
	ItemSize( ImVec2( text_width, frame_height ), padding.y );

	// For regular tree nodes, we arbitrary allow to click past 2 worth of ItemSpacing
	ImRect interact_bb = frame_bb;
	if ( ( flags & ( ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_SpanLabelWidth | ImGuiTreeNodeFlags_SpanAllColumns ) ) == 0 )
		interact_bb.Max.x = frame_bb.Min.x + text_width + ( label_size.x > 0.0f ? style.ItemSpacing.x * 2.0f : 0.0f );

	// Compute open and multi-select states before ItemAdd() as it clear NextItem data.
	ImGuiID storage_id = ( g.NextItemData.HasFlags & ImGuiNextItemDataFlags_HasStorageID ) ? g.NextItemData.StorageId : id;
	bool    is_open    = TreeNodeUpdateNextOpen( storage_id, flags );

	bool    is_visible;
	if ( span_all_columns || span_all_columns_label )
	{
		// Modify ClipRect for the ItemAdd(), faster than doing a PushColumnsBackground/PushTableBackgroundChannel for every Selectable..
		const float backup_clip_rect_min_x = window->ClipRect.Min.x;
		const float backup_clip_rect_max_x = window->ClipRect.Max.x;
		window->ClipRect.Min.x             = window->ParentWorkRect.Min.x;
		window->ClipRect.Max.x             = window->ParentWorkRect.Max.x;
		is_visible                         = ItemAdd( interact_bb, id );
		window->ClipRect.Min.x             = backup_clip_rect_min_x;
		window->ClipRect.Max.x             = backup_clip_rect_max_x;
	}
	else
	{
		is_visible = ItemAdd( interact_bb, id );
	}
	g.LastItemData.StatusFlags |= ImGuiItemStatusFlags_HasDisplayRect;
	g.LastItemData.DisplayRect      = frame_bb;

	// If a NavLeft request is happening and ImGuiTreeNodeFlags_NavLeftJumpsToParent enabled:
	// Store data for the current depth to allow returning to this node from any child item.
	// For this purpose we essentially compare if g.NavIdIsAlive went from 0 to 1 between TreeNode() and TreePop().
	// It will become tempting to enable ImGuiTreeNodeFlags_NavLeftJumpsToParent by default or move it to ImGuiStyle.
	bool store_tree_node_stack_data = false;
	if ( ( flags & ImGuiTreeNodeFlags_DrawLinesMask_ ) == 0 )
		flags |= g.Style.TreeLinesFlags;
	const bool draw_tree_lines = ( flags & ( ImGuiTreeNodeFlags_DrawLinesFull | ImGuiTreeNodeFlags_DrawLinesToNodes ) ) && ( frame_bb.Min.y < window->ClipRect.Max.y ) && ( g.Style.TreeLinesSize > 0.0f );
	if ( !( flags & ImGuiTreeNodeFlags_NoTreePushOnOpen ) )
	{
		store_tree_node_stack_data = draw_tree_lines;
		if ( ( flags & ImGuiTreeNodeFlags_NavLeftJumpsToParent ) && !g.NavIdIsAlive )
			if ( g.NavMoveDir == ImGuiDir_Left && g.NavWindow == window && NavMoveRequestButNoResultYet() )
				store_tree_node_stack_data = true;
	}

	const bool is_leaf = ( flags & ImGuiTreeNodeFlags_Leaf ) != 0;
	if ( !is_visible )
	{
		if ( ( flags & ImGuiTreeNodeFlags_DrawLinesToNodes ) && ( window->DC.TreeRecordsClippedNodesY2Mask & ( 1 << ( window->DC.TreeDepth - 1 ) ) ) )
		{
			ImGuiTreeNodeStackData* parent_data = &g.TreeNodeStack.Data[ g.TreeNodeStack.Size - 1 ];
			parent_data->DrawLinesToNodesY2     = ImMax( parent_data->DrawLinesToNodesY2, window->DC.CursorPos.y );  // Don't need to aim to mid Y position as we are clipped anyway.
			if ( frame_bb.Min.y >= window->ClipRect.Max.y )
				window->DC.TreeRecordsClippedNodesY2Mask &= ~( 1 << ( window->DC.TreeDepth - 1 ) );  // Done
		}
		if ( is_open && store_tree_node_stack_data )
			TreeNodeStoreStackData( flags, text_pos.x - text_offset_x );  // Call before TreePushOverrideID()
		if ( is_open && !( flags & ImGuiTreeNodeFlags_NoTreePushOnOpen ) )
			TreePushOverrideID( id );
		IMGUI_TEST_ENGINE_ITEM_INFO( g.LastItemData.ID, label, g.LastItemData.StatusFlags | ( is_leaf ? 0 : ImGuiItemStatusFlags_Openable ) | ( is_open ? ImGuiItemStatusFlags_Opened : 0 ) );
		return is_open;
	}

	if ( span_all_columns || span_all_columns_label )
	{
		TablePushBackgroundChannel();
		g.LastItemData.StatusFlags |= ImGuiItemStatusFlags_HasClipRect;
		g.LastItemData.ClipRect = window->ClipRect;
	}

	ImGuiButtonFlags button_flags = ImGuiTreeNodeFlags_None;
	if ( ( flags & ImGuiTreeNodeFlags_AllowOverlap ) || ( g.LastItemData.ItemFlags & ImGuiItemFlags_AllowOverlap ) )
		button_flags |= ImGuiButtonFlags_AllowOverlap;
	if ( !is_leaf )
		button_flags |= ImGuiButtonFlags_PressedOnDragDropHold;

	// We allow clicking on the arrow section with keyboard modifiers held, in order to easily
	// allow browsing a tree while preserving selection with code implementing multi-selection patterns.
	// When clicking on the rest of the tree node we always disallow keyboard modifiers.
	const float arrow_hit_x1          = ( text_pos.x - text_offset_x ) - style.TouchExtraPadding.x;
	const float arrow_hit_x2          = ( text_pos.x - text_offset_x ) + ( g.FontSize + padding.x * 2.0f ) + style.TouchExtraPadding.x;
	const bool  is_mouse_x_over_arrow = ( g.IO.MousePos.x >= arrow_hit_x1 && g.IO.MousePos.x < arrow_hit_x2 );

	const bool  is_multi_select       = ( g.LastItemData.ItemFlags & ImGuiItemFlags_IsMultiSelect ) != 0;
	if ( is_multi_select )  // We absolutely need to distinguish open vs select so _OpenOnArrow comes by default
		flags |= ( flags & ImGuiTreeNodeFlags_OpenOnMask_ ) == 0 ? ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick : ImGuiTreeNodeFlags_OpenOnArrow;

	// Open behaviors can be altered with the _OpenOnArrow and _OnOnDoubleClick flags.
	// Some alteration have subtle effects (e.g. toggle on MouseUp vs MouseDown events) due to requirements for multi-selection and drag and drop support.
	// - Single-click on label = Toggle on MouseUp (default, when _OpenOnArrow=0)
	// - Single-click on arrow = Toggle on MouseDown (when _OpenOnArrow=0)
	// - Single-click on arrow = Toggle on MouseDown (when _OpenOnArrow=1)
	// - Double-click on label = Toggle on MouseDoubleClick (when _OpenOnDoubleClick=1)
	// - Double-click on arrow = Toggle on MouseDoubleClick (when _OpenOnDoubleClick=1 and _OpenOnArrow=0)
	// It is rather standard that arrow click react on Down rather than Up.
	// We set ImGuiButtonFlags_PressedOnClickRelease on OpenOnDoubleClick because we want the item to be active on the initial MouseDown in order for drag and drop to work.
	if ( is_mouse_x_over_arrow )
		button_flags |= ImGuiButtonFlags_PressedOnClick;
	else if ( flags & ImGuiTreeNodeFlags_OpenOnDoubleClick )
		button_flags |= ImGuiButtonFlags_PressedOnClickRelease | ImGuiButtonFlags_PressedOnDoubleClick;
	else
		button_flags |= ImGuiButtonFlags_PressedOnClickRelease;
	if ( flags & ImGuiTreeNodeFlags_NoNavFocus )
		button_flags |= ImGuiButtonFlags_NoNavFocus;

	bool       selected     = ( flags & ImGuiTreeNodeFlags_Selected ) != 0;
	const bool was_selected = selected;

	// Multi-selection support (header)
	if ( is_multi_select )
	{
		// Handle multi-select + alter button flags for it
		MultiSelectItemHeader( id, &selected, &button_flags );
		if ( is_mouse_x_over_arrow )
			button_flags = ( button_flags | ImGuiButtonFlags_PressedOnClick ) & ~ImGuiButtonFlags_PressedOnClickRelease;
	}
	else
	{
		if ( window != g.HoveredWindow || !is_mouse_x_over_arrow )
			button_flags |= ImGuiButtonFlags_NoKeyModsAllowed;
	}

	bool hovered, held;
	bool pressed = ButtonBehavior( interact_bb, id, &hovered, &held, button_flags );
	bool toggled = false;
	if ( !is_leaf )
	{
		if ( pressed && g.DragDropHoldJustPressedId != id )
		{
			if ( ( flags & ImGuiTreeNodeFlags_OpenOnMask_ ) == 0 || ( g.NavActivateId == id && !is_multi_select ) )
			{
				toggled       = true;  // Single click
				label_pressed = toggled;
			}
			if ( flags & ImGuiTreeNodeFlags_OpenOnArrow )
			{
				toggled |= is_mouse_x_over_arrow && !g.NavHighlightItemUnderNav;  // Lightweight equivalent of IsMouseHoveringRect() since ButtonBehavior() already did the job
				label_pressed = !toggled;
			}
			if ( ( flags & ImGuiTreeNodeFlags_OpenOnDoubleClick ) && g.IO.MouseClickedCount[ 0 ] == 2 )
				toggled = true;  // Double click
		}
		else if ( pressed && g.DragDropHoldJustPressedId == id )
		{
			IM_ASSERT( button_flags & ImGuiButtonFlags_PressedOnDragDropHold );
			if ( !is_open )  // When using Drag and Drop "hold to open" we keep the node highlighted after opening, but never close it again.
				toggled = true;
			else
				pressed = false;  // Cancel press so it doesn't trigger selection.
		}

		if ( g.NavId == id && g.NavMoveDir == ImGuiDir_Left && is_open )
		{
			toggled = true;
			NavClearPreferredPosForAxis( ImGuiAxis_X );
			NavMoveRequestCancel();
		}
		if ( g.NavId == id && g.NavMoveDir == ImGuiDir_Right && !is_open )  // If there's something upcoming on the line we may want to give it the priority?
		{
			toggled = true;
			NavClearPreferredPosForAxis( ImGuiAxis_X );
			NavMoveRequestCancel();
		}

		if ( toggled )
		{
			is_open = !is_open;
			window->DC.StateStorage->SetInt( storage_id, is_open );
			g.LastItemData.StatusFlags |= ImGuiItemStatusFlags_ToggledOpen;
		}
	}

	// Multi-selection support (footer)
	if ( is_multi_select )
	{
		bool pressed_copy = pressed && !toggled;
		MultiSelectItemFooter( id, &selected, &pressed_copy );
		if ( pressed )
			SetNavID( id, window->DC.NavLayerCurrent, g.CurrentFocusScopeId, interact_bb );
	}

	if ( selected != was_selected )
		g.LastItemData.StatusFlags |= ImGuiItemStatusFlags_ToggledSelection;

	// Render
	{
		const ImU32               text_col                = GetColorU32( ImGuiCol_Text );
		ImGuiNavRenderCursorFlags nav_render_cursor_flags = ImGuiNavRenderCursorFlags_Compact;
		if ( is_multi_select )
			nav_render_cursor_flags |= ImGuiNavRenderCursorFlags_AlwaysDraw;  // Always show the nav rectangle
		if ( display_frame )
		{
			// Framed type
			const ImU32 bg_col = GetColorU32( ( held && hovered ) ? ImGuiCol_HeaderActive : hovered ? ImGuiCol_HeaderHovered
			                                                                                        : ImGuiCol_Header );
			RenderFrame( frame_bb.Min, frame_bb.Max, bg_col, true, style.FrameRounding );
			RenderNavCursor( frame_bb, id, nav_render_cursor_flags );
			if ( span_all_columns && !span_all_columns_label )
				TablePopBackgroundChannel();
			if ( flags & ImGuiTreeNodeFlags_Bullet )
				RenderBullet( window->DrawList, ImVec2( text_pos.x - text_offset_x * 0.60f, text_pos.y + g.FontSize * 0.5f ), text_col );
			else if ( !is_leaf )
				RenderArrow( window->DrawList, ImVec2( text_pos.x - text_offset_x + padding.x, text_pos.y ), text_col, is_open ? ( ( flags & ImGuiTreeNodeFlags_UpsideDownArrow ) ? ImGuiDir_Up : ImGuiDir_Down ) : ImGuiDir_Right, 1.0f );
			else  // Leaf without bullet, left-adjusted text
				text_pos.x -= text_offset_x - padding.x;
			if ( flags & ImGuiTreeNodeFlags_ClipLabelForTrailingButton )
				frame_bb.Max.x -= g.FontSize + style.FramePadding.x;
			if ( g.LogEnabled )
				LogSetNextTextDecoration( "###", "###" );
		}
		else
		{
			// Unframed typed for tree nodes
			if ( hovered || selected )
			{
				const ImU32 bg_col = GetColorU32( ( held && hovered ) ? ImGuiCol_HeaderActive : hovered ? ImGuiCol_HeaderHovered
				                                                                                        : ImGuiCol_Header );
				RenderFrame( frame_bb.Min, frame_bb.Max, bg_col, false );
			}
			RenderNavCursor( frame_bb, id, nav_render_cursor_flags );
			if ( span_all_columns && !span_all_columns_label )
				TablePopBackgroundChannel();
			if ( flags & ImGuiTreeNodeFlags_Bullet )
				RenderBullet( window->DrawList, ImVec2( text_pos.x - text_offset_x * 0.5f, text_pos.y + g.FontSize * 0.5f ), text_col );
			else if ( !is_leaf )
				RenderArrow( window->DrawList, ImVec2( text_pos.x - text_offset_x + padding.x, text_pos.y + g.FontSize * 0.15f ), text_col, is_open ? ( ( flags & ImGuiTreeNodeFlags_UpsideDownArrow ) ? ImGuiDir_Up : ImGuiDir_Down ) : ImGuiDir_Right, 0.70f );
			if ( g.LogEnabled )
				LogSetNextTextDecoration( ">", NULL );
		}

		if ( draw_tree_lines )
			TreeNodeDrawLineToChildNode( ImVec2( text_pos.x - text_offset_x + padding.x, text_pos.y + g.FontSize * 0.5f ) );

		// Label
		if ( display_frame )
			RenderTextClipped( text_pos, frame_bb.Max, label, label_end, &label_size );
		else
			RenderText( text_pos, label, label_end, false );

		if ( span_all_columns_label )
			TablePopBackgroundChannel();
	}

	if ( is_open && store_tree_node_stack_data )
		TreeNodeStoreStackData( flags, text_pos.x - text_offset_x );  // Call before TreePushOverrideID()
	if ( is_open && !( flags & ImGuiTreeNodeFlags_NoTreePushOnOpen ) )
		TreePushOverrideID( id );  // Could use TreePush(label) but this avoid computing twice

	IMGUI_TEST_ENGINE_ITEM_INFO( id, label, g.LastItemData.StatusFlags | ( is_leaf ? 0 : ImGuiItemStatusFlags_Openable ) | ( is_open ? ImGuiItemStatusFlags_Opened : 0 ) );
	return is_open;
}


bool is_path_part_of_current_dir( u32 depth, const std::string& current_path, char* folder_name )
{
	if ( depth >= directory::path_chunks.size() )
		return false;

	std::string path_str = sys_path_to_string( directory::path );

	if ( !path_str.starts_with( current_path ) )
		return false;

	std::string current_path_part = directory::path_chunks[ depth ];
	
	if ( folder_name != current_path_part )
		return false;

	return true;
}


static std::vector< std::string > g_filesystem_browser_path_chunks{};


// this is shit
void sidebar_draw_directory_recursive( u32 depth, const std::string& current_path )
{
	ImGuiTreeNodeFlags node_flags  = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DrawLinesFull;
	char*              folder_name = fs_get_filename( current_path.c_str(), current_path.size() );

	if ( !app::config.directory_tree_expand_on_click )
		node_flags |= ImGuiTreeNodeFlags_OpenOnArrow;

	if ( is_path_part_of_current_dir( depth, current_path, folder_name ) )
	{
		node_flags |= ImGuiTreeNodeFlags_Selected;

		if ( directory::folder_changed && app::config.directory_tree_auto_expand && !g_dir_change_from_dir_tree )
		{
			ImGui::SetScrollHereY();
			ImGui::SetNextItemOpen( true );
		}
	}

	bool   label_pressed = false;
	size_t hash          = std::hash< std::string >{}( current_path );
	
	bool   tree_opened   = TreeNodeBehaviorStupid( hash, node_flags, folder_name ? folder_name : current_path.c_str(), nullptr, label_pressed );

	if ( label_pressed )
	{
		//printf( "CLICKED\n" );
		g_dir_change_from_dir_tree = true;
		directory::queued = current_path;
	}

	ch_free_str( folder_name );

	if ( !tree_opened )
		return;

	std::string tmp_path = current_path;
	tmp_path += SEP;
	fs::path           evil  = sys_string_to_path( tmp_path );
	directory_entry_t* entry = dir_tree_get( evil );

	if ( entry )
	{
		for ( const file_t& folder : entry->folders )
		{
			fs::path filename = folder.path.filename();
			tmp_path          = current_path;

			if ( !current_path.ends_with( SEP ) )
				tmp_path += SEP;

			tmp_path += sys_path_to_string( filename );

			sidebar_draw_directory_recursive( depth + 1, tmp_path );
		}
	}

	ImGui::TreePop();
}


void sidebar_draw_filesystem_folder_item( const std::string& folder_name )
{


	if ( ImGui::Selectable( folder_name.c_str() ) )
	{
		g_filesystem_browser_path_chunks.push_back( folder_name );
	}
}


void sidebar_draw_filesystem( ImGuiStyle& style )
{
	ImGui::PushFont( font::normal_bold, style.FontSizeBase + 2.f );

	if ( !ImGui::CollapsingHeader( "Folders", ImGuiTreeNodeFlags_DefaultOpen ) )
	{
		ImGui::PopFont();
		return;
	}

	ImGui::PopFont();

	if ( !ImGui::BeginChild( "##directory_tree", {}, ImGuiChildFlags_ResizeY | ImGuiChildFlags_FrameStyle ) )
	{
		ImGui::EndChild();
		//ImGui::PopStyleVar();
		//ImGui::PopStyleColor( 4 );
		return;
	}

	//ImGui::TextUnformatted( "Files" );
	//ImGui::Separator();

	// get mounted drives
	// TODO: MOVE ME TO STARTUP, AND CHECK FOR NEW DRIVES BEING MOUNTED/UNMOUNTED ONCE IN A WHILE
	static bool                       first_run = true;
	static std::vector< std::string > drives;

	if ( first_run )
	{
		sys_get_drives( drives );
		first_run = false;
	}

	bool reset_dir_change_state = g_dir_change_from_dir_tree;

	if ( app::config.directory_tree_simple )
	{
		//std::string built_path;
		//
		//for ( size_t i = 0; i < g_filesystem_browser_path_chunks.size(); i++ )
		//{
		//	built_path += g_filesystem_browser_path_chunks[ i ];
		//	built_path += SEP;
		//}

		//fs::path           evil  = sys_string_to_path( built_path );
		fs::path evil = directory::path;
		evil += SEP_S;
		directory_entry_t* entry         = dir_tree_get( evil );

		g_filesystem_browser_path_chunks = directory::path_chunks;

		//ImGui::BeginDisabled( g_filesystem_browser_path_chunks.empty() );
		//
		//if ( ImGui::Selectable( "Back" ) )
		//{
		//	g_filesystem_browser_path_chunks.pop_back();
		//
		//	std::string new_path{};
		//
		//	for ( size_t i = 0; i < g_filesystem_browser_path_chunks.size(); i++ )
		//	{
		//		new_path += g_filesystem_browser_path_chunks[ i ];
		//		new_path += SEP;
		//	}
		//
		//	directory::queued = new_path;
		//}
		//
		//ImGui::EndDisabled();
		//
		//ImGui::Separator();
		ImGui::PushID( "##folders" );

		bool navigtate = false;

		if ( g_filesystem_browser_path_chunks.empty() )
		{
			for ( const std::string& drive : drives )
			{
				if ( ImGui::Selectable( drive.c_str() ) )
				{
					g_filesystem_browser_path_chunks.push_back( drive );
					navigtate = true;
				}
			}
		}
		else if ( entry )
		{
			for ( const file_t& folder : entry->folders )
			{
				std::string folder_name = sys_path_to_string( folder.path );
				if ( ImGui::Selectable( folder_name.c_str() ) )
				{
					g_filesystem_browser_path_chunks.push_back( folder_name );
					navigtate = true;
				}
				//sidebar_draw_filesystem_folder_item( folder.path );
			}
		}
		ImGui::PopID();
		// else - background scanning folder ?

		if ( navigtate )
		{
			std::string built_path;

			for ( size_t i = 0; i < g_filesystem_browser_path_chunks.size(); i++ )
			{
				built_path += g_filesystem_browser_path_chunks[ i ];
				built_path += SEP;
			}

			directory::queued = fs_path_clean( built_path.c_str(), built_path.size() );
		}
	}
	else
	{
		for ( const std::string& drive : drives )
		{
			sidebar_draw_directory_recursive( 0, drive );
		}
	}

	ImGui::EndChild();

	if ( reset_dir_change_state )
		g_dir_change_from_dir_tree = false;
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
		//ImGui::PopStyleVar();
		//ImGui::PopStyleColor( 4 );
		return;
	}

	//ImGui::PopStyleVar();
	//ImGui::PopStyleColor( 4 );


	//ImGui::SetCursorPos( { 0.f, 0.f } );

	//if ( !opened )
	//{
	//	ImGui::EndChild();
	//	//ImGui::PopStyleVar();
	//	return;
	//}

	ImVec2 remove_text_size = ImGui::CalcTextSize( "X" );
	ImVec2 region_avail     = ImGui::GetContentRegionAvail();

	float  bookmark_width   = region_avail.x - ( ImGui::GetTextLineHeightWithSpacing() );
	ImVec2 bookmark_size( bookmark_width, ImGui::GetTextLineHeight() );
	ImVec2 remove_size( ImGui::GetTextLineHeight(), bookmark_size.y );

	//ImGui::PushItemWidth( region_avail.x - remove_text_size.x );

	ImVec2 screen_cursor_pos = ImGui::GetCursorScreenPos();

	u32 id = 1;
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
			ImGui::PushStyleVar( ImGuiStyleVar_SelectableTextAlign, { 0.5, 0.5 } );

			if ( ImGui::Selectable( "X", false, 0, remove_size ) )
			{
			}

			ImGui::PopStyleVar();
			ImGui::PopID();
		}

		screen_cursor_pos.y += bookmark_size.y;
	}

	ImGui::Separator();

	if ( ImGui::Button( "Add Current Directory" ) )
	{
		app::config.bookmark.emplace_back( directory::path.filename().string(), directory::path.string(), true );
	}

	//ImGui::PopItemWidth();

	ImGui::EndChild();
}


void sidebar_draw_file_information( ImGuiStyle& style )
{
	// if ( gallery::sorted_media.size() && gallery::last_selection.entry.type != e_media_type_none )
	if ( gallery::sorted_media.empty() )
		return;

	ImGui::PushFont( font::normal_bold, style.FontSizeBase + 2.f );
	ImGui::TextUnformatted( "File Information\n" );
	ImGui::Separator();
	ImGui::PopFont();

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
		fs::path    dir     = entry.file.path.parent_path();
		std::string dir_str = sys_path_to_string( dir );

		ImGui::TextUnformatted( dir_str.c_str() );
		ImGui::Separator();
	}

	ImGui::TextUnformatted( entry.filename.c_str() );

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
	if ( entry.file.type & e_file_type_file && entry.filename.starts_with( "[twitter]" ) )
	{
		// if it's a twitter url, construct the original url from the post
		const char* start = entry.filename.c_str();

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

	ImGui::EndChild();
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
			sidebar_draw_bookmarks( style );
			sidebar_draw_filesystem( style );
			sidebar_draw_file_information( style );

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
			settings_draw();
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

