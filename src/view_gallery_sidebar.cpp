#include "main.h"


static char g_folder_buf[ 512 ]{};

void        gallery_view_reset_text_size();

void gallery_view_draw_header()
{
	int window_width, window_height;
	SDL_GetWindowSize( app::window, &window_width, &window_height );

	ImGuiStyle& style = ImGui::GetStyle();

	ImGui::SetNextWindowPos( { 0, 0 } );
	// ImGui::SetNextWindowSize( { (float)window_width, 32.f } );
	// ImGui::SetNextWindowSizeConstraints( { (float)window_width, 0.f }, { (float)window_width, 64.f } );

	ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, { 6, 6 } );

	if ( !ImGui::BeginChild( "##gallery_header", { (float)window_width, 0.f }, ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_FrameStyle | ImGuiChildFlags_AlwaysUseWindowPadding ) )
	{
		ImGui::EndChild();
		ImGui::PopStyleVar();
		return;
	}

	if ( ImGui::Button( "Toggle Sidebar" ) )
	{
		gallery::sidebar_draw = !gallery::sidebar_draw;
	}

	// ImGui::Selectable( "Sidebar", &gallery::sidebar_draw );
	ImGui::SameLine();
	ImGui::Spacing();
	ImGui::SameLine();

	if ( ImGui::Button( "^" ) )
	{
		directory::queued = directory::path.parent_path();
	}

	ImGui::SameLine();

	// ImGui::TextUnformatted( directory::path.string().c_str() );

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
	ImGui::Spacing();
	ImGui::SameLine();

	if ( ImGui::SliderInt( "Zoom", &gallery::item_size, gallery::item_size_min, gallery::item_size_max ) )
	{
		gallery_view_reset_text_size();

		if ( !app::config.thumbnail_use_fixed_size )
			thumbnail_clear_cache();
	}

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

	if ( ImGui::BeginCombo( "Sort Mode", combo_preview_value, 0 ) )
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

	ImGui::SameLine();

	ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, { 0, 0 } );

	ImGui::EndChild();

	ImGui::PopStyleVar();

	ImGui::PopStyleVar();
}


void gallery_view_update_header_directory()
{
	snprintf( g_folder_buf, 512, "%s", directory::path.string().c_str() );
}


void sidebar_draw_filesystem()
{
	// get mounted drives
	// TODO: MOVE ME TO STARTUP, AND CHECK FOR NEW DRIVES BEING MOUNTED/UNMOUNTED ONCE IN A WHILE
	static bool                    first_run = true;
	static std::vector< fs::path > drives;

	if ( first_run )
	{
		drives    = sys_get_drives();
		first_run = false;
	}

	u32 drive_i = 0;
	for ( const fs::path& drive : drives )
	{
		std::string drive_str = drive.string();
		if ( ImGui::CollapsingHeader( drive_str.data() ) )
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
	ImGui::SetCursorPosX( 0.f );

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

