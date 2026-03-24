#include "main.h"

#include "imgui_internal.h"


static char g_folder_buf[ 512 ]{};
bool        g_do_search = false;

void        gallery_view_reset_text_size();


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

	ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, app::config.gallery_header_padding );
	ImGui::PushStyleVar( ImGuiStyleVar_WindowBorderSize, 0 );

	if ( app::config.use_custom_colors )
	{
		ImGui::PushStyleColor( ImGuiCol_FrameBg, style.Colors[ ImGuiCol_WindowBg ] );
		ImGui::PushStyleColor( ImGuiCol_FrameBgHovered, style.Colors[ ImGuiCol_WindowBg ] );
		ImGui::PushStyleColor( ImGuiCol_FrameBgActive, style.Colors[ ImGuiCol_WindowBg ] );

		ImGui::PushStyleColor( ImGuiCol_WindowBg, app::config.header_bg_color );
	}

	// if ( !ImGui::Begin( "##gallery_header", { (float)window_width, 0.f }, ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_FrameStyle | ImGuiChildFlags_AlwaysUseWindowPadding ) )
	if ( !ImGui::Begin( "##gallery_header", 0, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav ) )
	{
		ImGui::End();
		if ( app::config.use_custom_colors )
			ImGui::PopStyleColor( 4 );
		ImGui::PopStyleVar( 2 );
		return 0;
	}

	ImDrawList* draw_list = ImGui::GetWindowDrawList();

	if ( ImGui::Button( "Toggle Sidebar" ) )
	{
		gallery::sidebar_draw = !gallery::sidebar_draw;
	}

	// ImGui::Selectable( "Sidebar", &gallery::sidebar_draw );
	ImGui::SameLine();
	//ImGui::Spacing();
	//ImGui::SameLine();

	draw_vertical_separator( draw_list, style );

	// TODO: add in navigation history
	ImGui::BeginDisabled();

	if ( ImGui::Button( "<" ) )
	{
	}

	ImGui::SameLine();
	if ( ImGui::Button( ">" ) )
	{
	}

	ImGui::SameLine();
	ImGui::EndDisabled();

	if ( ImGui::Button( "^" ) )
	{
		directory::queued = directory::path.parent_path();
	}

	ImGui::SameLine();

	// ImGui::TextUnformatted( directory::path.string().c_str() );
	ImGui::SetNextItemWidth( 400 );

	if ( ImGui::InputText( "##directory", g_folder_buf, 512, ImGuiInputTextFlags_EnterReturnsTrue ) )
	{
		if ( fs_is_dir( g_folder_buf ) )
			directory::queued = g_folder_buf;
		else
			snprintf( g_folder_buf, 512, directory::path.string().c_str() );
	}

	ImGui::SameLine();

	// Enter returns true doesn't work because of gallery view hooking that input currently, need to add a check later for if focused in text input
	if ( ImGui::Button( "->" ) )
	{
		directory::queued = g_folder_buf;
	}

	ImGui::SameLine();
	draw_vertical_separator( draw_list, style );

	ImGui::TextUnformatted( "Search" );

	ImGui::SameLine();

	if ( ImGui::InputText( "##search", gallery::search, 512, ImGuiInputTextFlags_EnterReturnsTrue ) )
	{
		g_do_search = true;
		gallery_view_dir_change();
		gallery_view_reset_text_size();
		printf( "SEARCH\n" );
	}

	ImGui::SameLine();
	draw_vertical_separator( draw_list, style );

	// ---------------------------------------------------------------------------------
	// Center Spacing, rest is aligned to the right

	ImVec2 region_avail    = ImGui::GetContentRegionAvail();

	int    space_needed    = 100;

	ImVec2 zoom_size       = ImGui::CalcTextSize( "Zoom" );
	ImVec2 sort_size       = ImGui::CalcTextSize( "Sort Mode" );
	ImVec2 sort_entry_size = ImGui::CalcTextSize( "Date Modified - New to Old" );

	space_needed += zoom_size.x + sort_size.x;
	space_needed += style.ItemSpacing.x * 2;  // Zoom Text
	space_needed += style.WindowBorderSize;   // Separator
	space_needed += style.ItemSpacing.x * 2;  // Sort Mode Text
	space_needed += style.ItemSpacing.x;      // Padding
	
	int sep_space_needed = 0;

	if ( style.WindowBorderSize )
		sep_space_needed = style.ItemSpacing.x + style.WindowBorderSize;

	// estimate
	int arrow_size = style.FontSizeBase;

	int sort_width = sort_entry_size.x + arrow_size + ( style.FramePadding.x * 3 ) + ( style.FramePadding.y * 2 );

	space_needed += sort_width;

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

	ImGui::TextUnformatted( "Sort Mode" );
	ImGui::SameLine();

	const char* sort_names[] = {
		"A to Z",
		"Z to A",
		"Date Modified - New to Old",
		"Date Modified - Old to New",
		"Date Created - New to Old",
		"Date Created - Old to New",
		"File Size - Large to Small",
		"File Size - Small to Large"
	};

	static_assert( ARR_SIZE( sort_names ) == e_gallery_sort_mode_count );

	const char* combo_preview_value = sort_names[ gallery::sort_mode ];

	ImGui::SetNextItemWidth( sort_width );

	if ( ImGui::BeginCombo( "##sort_mode", combo_preview_value, 0 ) )
	{
		for ( int n = 0; n < e_gallery_sort_mode_count; n++ )
		{
			const bool is_selected = ( gallery::sort_mode == n );

			if ( ImGui::Selectable( sort_names[ n ], is_selected ) )
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
		ImGui::PopStyleColor( 4 );

	ImGui::PopStyleVar( 2 );

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
					app::config.bookmark.emplace_back( directory::path.filename().string(), directory::path.string() );
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
				}

				ImGui::EndListBox();
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
			}

			ImGui::EndListBox();

			ImGui::EndTabItem();
		}

		if ( ImGui::BeginTabItem( "Style Editor" ) )
		{
			ImGui::ShowStyleEditor();
			ImGui::EndTabItem();
		}

		if ( ImGui::BeginTabItem( "Settings" ) )
		{
			if ( ImGui::Checkbox( "Gallery - Show Filenames", &app::config.gallery_show_filenames ) )
			{
				gallery_view_reset_text_size();
			}

			ImGui::Checkbox( "Always Draw", &app::config.always_draw );

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

