#include "main.h"


struct gallery_item_t
{
	
};


extern bool                       g_gallery_view;

std::vector< gallery_item_t >     g_gallery_items;

const int                         GALLERY_GRID_X_COUNT = 12;

const int                         MOUSE_SCROLL_AMOUNT  = 150;


void gallery_view_input()
{
	if ( ImGui::IsKeyPressed( ImGuiKey_LeftArrow ) )
	{
		if ( g_gallery_index == 0 )
			g_gallery_index = g_folder_media_list.size();

		g_gallery_index--;
		gallery_view_scroll_to_selected();
	}
	else if ( ImGui::IsKeyPressed( ImGuiKey_RightArrow ) )
	{
		g_gallery_index = ( g_gallery_index + 1 ) % g_folder_media_list.size();
		gallery_view_scroll_to_selected();
	}
	else if ( ImGui::IsKeyPressed( ImGuiKey_UpArrow ) )
	{
		if ( g_gallery_index < GALLERY_GRID_X_COUNT )
		{
			size_t count_in_row   = g_folder_media_list.size() % GALLERY_GRID_X_COUNT;
			size_t missing_in_row = GALLERY_GRID_X_COUNT - count_in_row;
			size_t row_diff       = GALLERY_GRID_X_COUNT - g_gallery_index;

			// advance up a row
			if ( missing_in_row >= row_diff )
				row_diff += GALLERY_GRID_X_COUNT;

			g_gallery_index = g_folder_media_list.size() - ( row_diff - missing_in_row );
		}
		else
		{
			g_gallery_index = ( g_gallery_index - GALLERY_GRID_X_COUNT ) % g_folder_media_list.size();
		}

		gallery_view_scroll_to_selected();
	}
	else if ( ImGui::IsKeyPressed( ImGuiKey_DownArrow ) )
	{
		if ( g_gallery_index + GALLERY_GRID_X_COUNT >= g_folder_media_list.size() )
		{
			size_t count_in_row = g_folder_media_list.size() % GALLERY_GRID_X_COUNT;
			size_t row_pos      = g_gallery_index % GALLERY_GRID_X_COUNT;
			g_gallery_index      = row_pos;
		}
		else
		{
			g_gallery_index = ( g_gallery_index + GALLERY_GRID_X_COUNT ) % g_folder_media_list.size();
		}

		gallery_view_scroll_to_selected();
	}
}


static char g_folder_buf[ 512 ]{};

void gallery_view_draw_header()
{
	int window_width, window_height;
	SDL_GetWindowSize( g_main_window, &window_width, &window_height );

	ImGui::SetNextWindowPos( { 0, 0 } );
	ImGui::SetNextWindowSize( { (float)window_width, 32.f } );

	if ( ImGui::BeginChild( "##gallery_header", {}, ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_FrameStyle | ImGuiChildFlags_AlwaysUseWindowPadding ) )
	{
		ImGui::Text( "%.1f FPS (%.3f ms/frame)", ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate );

		ImGui::SameLine();

		if ( ImGui::Button( "^" ) )
		{
			g_folder_queued = g_folder.parent_path();
		}

		ImGui::SameLine();

		// ImGui::TextUnformatted( g_folder.string().c_str() );

		if ( ImGui::InputText( "Directory", g_folder_buf, 512, ImGuiInputTextFlags_EnterReturnsTrue ) )
		{
			// g_folder_queued = g_folder_buf;
		}

		ImGui::SameLine();

		// Enter returns true doesn't work because of gallery view hooking that input currently, need to add a check later for if focused in text input
		if ( ImGui::Button( "->" ) )
		{
			g_folder_queued = g_folder_buf;
		}
	}

	ImGui::EndChild();
}


void gallery_view_dir_change()
{
	snprintf( g_folder_buf, 512, "%s", g_folder.string().c_str() );
}


bool g_scroll_to_selected = false;

void gallery_view_scroll_to_selected()
{
	g_scroll_to_selected = true;
}


void gallery_view_draw_image( image_t* image, ImTextureRef im_texture, float image_bounds, bool upscale )
{
	// Fit image in window size, scaling up if needed
	float factor[ 2 ] = { 1.f, 1.f };

	if ( upscale || image->width > image_bounds )
		factor[ 0 ] = (float)image_bounds / (float)image->width;

	if ( upscale || image->height > image_bounds )
		factor[ 1 ] = (float)image_bounds / (float)image->height;

	float  zoom_level = std::min( factor[ 0 ], factor[ 1 ] );

	ImVec2 scaled_image_size;
	scaled_image_size.x = image->width * zoom_level;
	scaled_image_size.y = image->height * zoom_level;

	// center the image
	ImVec2 image_offset = ImGui::GetCursorPos();
	image_offset.x += ( image_bounds - scaled_image_size.x ) / 2;
	image_offset.y += ( image_bounds - scaled_image_size.y ) / 2;

	ImGui::SetCursorPos( image_offset );

	ImGui::Image( im_texture, scaled_image_size );
}


void gallery_view_draw_content()
{
	int window_width, window_height;
	SDL_GetWindowSize( g_main_window, &window_width, &window_height );

	ImVec2      region_avail = ImGui::GetContentRegionAvail();

	ImGuiStyle& style        = ImGui::GetStyle();

	// weirdly sized still
	ImGui::SetNextWindowPos( { 0, 32.f } );
	ImGui::SetNextWindowSize( { (float)window_width, region_avail.y + style.ItemSpacing.y } );

	if ( !ImGui::BeginChild( "##gallery_content", {}, ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollWithMouse ) )
	{
		ImGui::EndChild();
		return;
	}

	float  text_height         = ImGui::CalcTextSize( "TEST" ).y;

	// ScrollToBringRectIntoView

	int    grid_item_padding   = style.ItemSpacing.x + style.ScrollbarPadding;

	// float       item_size_x       = ( region_avail.x / GALLERY_GRID_X_COUNT ) - ( GALLERY_GRID_X_COUNT * grid_item_padding ) - ( grid_item_padding * 2 );
	// float  item_size_x         = ( region_avail.x / GALLERY_GRID_X_COUNT ) - ( ( GALLERY_GRID_X_COUNT - 1 ) * grid_item_padding );
	float  item_size_x         = ( window_width / GALLERY_GRID_X_COUNT ) - ( grid_item_padding );
	float  item_size_y         = item_size_x + text_height + style.ItemInnerSpacing.y;

	int    grid_pos_x          = 0;
	size_t i                   = 0;

	int    image_visible_count = 0;

	struct delayed_load_t
	{
		fs::path path;
		size_t   index;
	};

	static std::vector< delayed_load_t > thumbnail_requests;
	thumbnail_requests.clear();

	// scroll speed hack
	{
		float scroll = ImGui::GetScrollY();
		float scroll_amount = item_size_y + style.ItemSpacing.y;
		
		if ( g_mouse_scrolled_up )
			scroll -= scroll_amount;

		else if ( g_mouse_scrolled_down )
			scroll += scroll_amount;

		if ( g_window_resized )
		{
			float scroll_diff = fmod( scroll, scroll_amount );

			if ( scroll_diff > 0 )
				scroll -= scroll_diff;
		}

		ImGui::SetScrollY( scroll );
	}

	for ( size_t i = 0; i < g_folder_media_list.size(); i++ )
	{
		const media_entry_t& media  = g_folder_media_list[ i ];

		float                scroll = ImGui::GetScrollY();

		ImGui::SetNextWindowSize( { item_size_x, item_size_y } );

		// check if i is 0, for some reason it adds like extra spacing for some reason on the first item
		if ( grid_pos_x == GALLERY_GRID_X_COUNT || i == 0 )
		{
			grid_pos_x = 0;
			//float new_pos = std::min( region_avail.x, ImGui::GetCursorPosX() + style.ItemSpacing.x );
			//ImGui::SetCursorPosX( style.ItemSpacing.x );
			// ImGui::SetCursorPosX( ImGui::GetCursorPosX() + style.ItemSpacing.x );
			ImGui::SetCursorPosX( ImGui::GetCursorPosX() );
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
			if ( cursor_pos.y + item_size_y < visible_top )
				distance = visible_top - ( cursor_pos.y + item_size_y );

			// check if the top of the item is still visible at the bottom of the content window
			else if ( cursor_pos.y > visible_bottom )
				distance = cursor_pos.y - visible_bottom;

			// if distance is still 0, this item is at least partially on-screen
			thumbnail_update_distance( g_folder_thumbnail_list[ i ], distance );
		}

		ImVec2 cursor_screen_pos = ImGui::GetCursorScreenPos();
		ImVec2 mouse_pos         = ImGui::GetMousePos();
		bool   item_hovered      = false;

		if ( g_gallery_index == i && g_scroll_to_selected )
		{
			bool   scroll_needed  = false;
			bool   scroll_up      = false;
			float  visible_top    = scroll;
			float  visible_bottom = visible_top + ImGui::GetWindowHeight();

			// check if the bottom of the item is off-screen at the bottom of the content window
			if ( cursor_pos.y + item_size_y > visible_bottom )
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
					scroll_offset = ( cursor_pos.y + item_size_y + style.ItemSpacing.y ) - visible_bottom;

				ImGui::SetScrollY( ImGui::GetScrollY() + scroll_offset );
			}
		}

		// is this item even visible?
		if ( !ImGui::IsRectVisible( cursor_screen_pos, { cursor_screen_pos.x + item_size_x, cursor_screen_pos.y + item_size_y } ) )
		{
			// use a dummy instead of a full child window, cheaper
			ImGui::Dummy( { item_size_x, item_size_y } );
			grid_pos_x++;
			continue;
		}

		item_hovered = ImGui::IsMouseHoveringRect( cursor_screen_pos, { cursor_screen_pos.x + item_size_x, cursor_screen_pos.y + item_size_y } );

		if ( g_gallery_index == i )
			ImGui::PushStyleColor( ImGuiCol_ChildBg, style.Colors[ ImGuiCol_FrameBg ] );
		else if ( item_hovered )
			ImGui::PushStyleColor( ImGuiCol_ChildBg, style.Colors[ ImGuiCol_FrameBgHovered ] );

		if ( ImGui::BeginChild( i + 1, {}, ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoScrollbar ) )
		{
#if 1
			image_visible_count++;

			float  image_bounds = item_size_x - ( style.ItemInnerSpacing.y * 4 );
			ImVec2 current_pos  = ImGui::GetCursorPos();
			ImGui::SetNextWindowSize( { image_bounds, image_bounds } );

			ImGui::PushStyleColor( ImGuiCol_ChildBg, { 0.25f, 0.25f, 0.25f, 1.f } );

			if ( media.type == e_media_type_directory )
			{
				gallery_view_draw_image( icon_get_image( e_icon_folder ), icon_get_imtexture( e_icon_folder ), image_bounds, false );
			}
			// videos don't have thumbnail generation yet
			else if ( media.type == e_media_type_video )
			{
				gallery_view_draw_image( icon_get_image( e_icon_video ), icon_get_imtexture( e_icon_video ), image_bounds, false );
			}
			else
			{
				thumbnail_t* thumbnail = thumbnail_get_data( g_folder_thumbnail_list[ i ] );

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
					else // if ( thumbnail->status == e_thumbnail_status_free )
					{
						ImGui::Dummy( { image_bounds, image_bounds } );
					}
				}
				else
				{
					if ( !thumbnail && media.type != e_media_type_directory )
						thumbnail_requests.emplace_back( media.path, i );
					// g_folder_thumbnail_list[ i ] = thumbnail_queue_image( entry );

					ImGui::Dummy( { image_bounds, image_bounds } );
				}
			}

			// if ( visible )
			ImGui::PopStyleColor();
#endif
		}

		ImGui::TextUnformatted( media.filename.c_str() );

		ImGui::EndChild();

		if ( g_gallery_index == i || item_hovered )
			ImGui::PopStyleColor();

		if ( item_hovered && ImGui::IsMouseClicked( ImGuiMouseButton_Left ) )
		{
			g_gallery_index = i;

			if ( ImGui::IsMouseDoubleClicked( ImGuiMouseButton_Left ) )
			{
				if ( media.type == e_media_type_directory )
				{
					g_folder_queued = media.path;
				}
				else
				{
					gallery_view_toggle();
				}
			}
		}

		grid_pos_x++;
	}

	ImGui::EndChild();

	// printf( "IMAGE COUNT: %d\n", image_visible_count );

	for ( size_t i = 0; i < thumbnail_requests.size(); i++ )
		g_folder_thumbnail_list[ thumbnail_requests[ i ].index ] = thumbnail_queue_image( thumbnail_requests[ i ].path );

	g_scroll_to_selected = false;
}


void gallery_view_draw()
{
	gallery_view_input();

	int window_width, window_height;
	SDL_GetWindowSize( g_main_window, &window_width, &window_height );

	ImGui::SetNextWindowPos( { 0, 0 } );
	ImGui::SetNextWindowSize( { (float)window_width, (float)window_height } );

	if ( !ImGui::Begin( "##gallery_main", 0, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings ) )
	{
		ImGui::End();
		return;
	}

	// Header
	gallery_view_draw_header();

	// Sidebar

	// Gallery View
	gallery_view_draw_content();

	thumbnail_cache_debug_draw();

	ImGui::End();
}

