#include "main.h"


struct gallery_item_t
{
	
};


extern fs::path TEST_FOLDER;

extern std::vector< fs::path >    g_folder_media_list;
extern std::vector< h_thumbnail > g_folder_thumbnail_list;
extern size_t                     g_folder_index;

std::vector< gallery_item_t >     g_gallery_items;

const int                         GALLERY_GRID_X_COUNT = 6;


void gallery_view_input()
{
}


void gallery_view_draw_header()
{
	int window_width, window_height;
	SDL_GetWindowSize( g_main_window, &window_width, &window_height );


	ImGui::SetNextWindowPos( { 0, 0 } );
	ImGui::SetNextWindowSize( { (float)window_width, 32.f } );

	if ( ImGui::BeginChild( "##gallery_header", {}, ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_FrameStyle ) )
	{
		ImGui::Text( TEST_FOLDER.string().c_str() );

		ImGui::EndChild();
	}
}


void gallery_view_draw_content()
{
	int window_width, window_height;
	SDL_GetWindowSize( g_main_window, &window_width, &window_height );

	ImVec2      region_avail = ImGui::GetContentRegionAvail();

	ImGuiStyle& style        = ImGui::GetStyle();

	ImGui::SetNextWindowPos( { 0, 32.f + style.ItemSpacing.y } );
	ImGui::SetNextWindowSize( region_avail );

	if ( !ImGui::BeginChild( "##gallery_content" ) )
	{
		ImGui::EndChild();
		return;
	}

	float  text_height         = ImGui::CalcTextSize( "TEST" ).y;

	// ScrollToBringRectIntoView

	int    grid_item_padding   = style.ItemSpacing.x;

	// float       item_size_x       = ( region_avail.x / GALLERY_GRID_X_COUNT ) - ( GALLERY_GRID_X_COUNT * grid_item_padding ) - ( grid_item_padding * 2 );
	float  item_size_x         = ( region_avail.x / GALLERY_GRID_X_COUNT ) - ( ( GALLERY_GRID_X_COUNT - 1 ) * grid_item_padding );
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

	for ( size_t i = 0; i < g_folder_media_list.size(); i++ )
	{
		const fs::path& entry  = g_folder_media_list[ i ];

		float           scroll = ImGui::GetScrollY();

		ImGui::SetNextWindowSize( { item_size_x, item_size_y } );

		if ( grid_pos_x == GALLERY_GRID_X_COUNT )
		{
			grid_pos_x = 0;
			ImGui::SetCursorPosX( ImGui::GetCursorPosX() + style.ItemSpacing.x );
		}
		else
		{
			// ImGui::SetCursorPos( { cursor_pos.x + item_size_x + grid_item_padding, cursor_pos.y } );
			ImGui::SameLine();
		}

		ImVec2 cursor_pos = ImGui::GetCursorPos();

		if ( g_folder_index == i )
		{
			ImGui::PushStyleColor( ImGuiCol_ChildBg, style.Colors[ ImGuiCol_FrameBg ] );
		}

		// Calculate Distance
		{
			ImVec2 window_pos     = ImGui::GetWindowPos();
			float  visible_top    = scroll;
			float  visible_bottom = visible_top + ImGui::GetWindowHeight();

			float  cursor_y       = ImGui::GetCursorPosY();
			//float  item_y         = cursor_y + item_size_y;

			u32    distance       = 0;
			if ( cursor_y + item_size_y < visible_top )
				distance = visible_top - ( cursor_y + item_size_y );  // above view
			else if ( cursor_y > visible_bottom )
				distance = cursor_y - visible_bottom;  // below view
			else
				distance = 0;  // visible

			thumbnail_update_distance( g_folder_thumbnail_list[ i ], distance );
		}

		ImGui::PushID( *entry.string().c_str() );

		if ( ImGui::BeginChild( entry.string().c_str(), {}, ImGuiChildFlags_AlwaysUseWindowPadding | ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar ) )
		{
#if 1
			image_visible_count++;

			float  image_bounds  = item_size_x - ( style.ItemInnerSpacing.y * 4 );
			ImVec2 current_pos = ImGui::GetCursorPos();
			// ImGui::SetNextWindowPos( { current_pos.x + style.ItemInnerSpacing.x, current_pos.y + style.ItemInnerSpacing.y } );
			ImGui::SetNextWindowSize( { image_bounds, image_bounds } );

			// if ( visible )
			ImGui::PushStyleColor( ImGuiCol_ChildBg, { 0.25f, 0.25f, 0.25f, 1.f } );

			thumbnail_t* thumbnail = thumbnail_get_data( g_folder_thumbnail_list[ i ] );

			if ( thumbnail )
			{
				if ( thumbnail->status == e_thumbnail_status_finished )
				{
					// Fit image in window size, scaling up if needed
					float factor[ 2 ] = { 1.f, 1.f };

					factor[ 0 ]       = (float)image_bounds / (float)thumbnail->data->width;
					factor[ 1 ]       = (float)image_bounds / (float)thumbnail->data->height;

					float  zoom_level = std::min( factor[ 0 ], factor[ 1 ] );

					ImVec2 scaled_image_size;
					scaled_image_size.x = thumbnail->data->width * zoom_level;
					scaled_image_size.y = thumbnail->data->height * zoom_level;

					// center the image
					ImVec2 image_offset = ImGui::GetCursorPos();
					image_offset.x += ( image_bounds - scaled_image_size.x ) / 2;
					image_offset.y += ( image_bounds - scaled_image_size.y ) / 2;

					ImGui::SetCursorPos( image_offset );

					ImGui::Image( thumbnail->im_texture, scaled_image_size );
				}
				else
				{
					ImGui::Dummy( { image_bounds, image_bounds } );
				}
			}
			else
			{
				if ( !thumbnail )
					thumbnail_requests.emplace_back( entry, i );
				// g_folder_thumbnail_list[ i ] = thumbnail_queue_image( entry );

				ImGui::Dummy( { image_bounds, image_bounds } );
			}

			// if ( visible )
			ImGui::PopStyleColor();
#endif
		}

		ImGui::TextUnformatted( entry.string().c_str() );

		ImGui::EndChild();

		ImGui::PopID();

		if ( g_folder_index == i )
		{
			ImGui::PopStyleColor();
		}

		grid_pos_x++;
	}

	ImGui::EndChild();

	// printf( "IMAGE COUNT: %d\n", image_visible_count );

	for ( size_t i = 0; i < thumbnail_requests.size(); i++ )
		g_folder_thumbnail_list[ thumbnail_requests[ i ].index ] = thumbnail_queue_image( thumbnail_requests[ i ].path );
}


void gallery_view_draw()
{
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

