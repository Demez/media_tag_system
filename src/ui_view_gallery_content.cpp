#include "main.h"

#include "imgui.h"
#include "imgui_internal.h"


struct delayed_load_t
{
	media_entry_t media;
	size_t        index;
};


// internal persistent draw info across frames
namespace gallery_draw
{
	float                         scroll      = 0.f;
	float                         scroll_prev = 0.f;

	// Item size with text line height and spacing
	float                         item_size_y;

	// flexible horizontal spacing between items
	float                         item_spacing_x;

	// keep the item visible on screen
	bool                          lock_visible_item;  // ?

	// States of last item or draw
	bool                          scrollbar_active_last_frame;
	bool                          scrollbar_active;

	size_t                        last_hovered       = SIZE_MAX;
	size_t                        last_selected      = SIZE_MAX;
	size_t                        first_visible_item = 0;

	// thumbnails we want loaded this frame
	std::vector< delayed_load_t > thumbnail_requests;

	ImVec2                        dummy_area;
	ImVec2                        region_size;

	// Input states
	bool                          content_area_hovered;
	bool                          any_item_hovered;
	bool                          scroll_changed;
}


// =============================================================================================
// Gallery Content Helpers


bool is_content_area_hovered( float area_width, float area_height )
{
	ImGuiStyle& style                = ImGui::GetStyle();
	ImVec2      cursor_screen_pos    = ImGui::GetCursorScreenPos();

	bool        content_area_hovered = ImGui::IsMouseHoveringRect(
	  cursor_screen_pos,
	  { cursor_screen_pos.x + area_width + style.WindowPadding.x,
	    area_height + style.WindowPadding.y } );

	if ( !content_area_hovered )
		return false;

	//if ( !ImGui::IsPopupOpen( "", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel ) )
	//	return true;

	for ( int i = 0; i < ImGui::GetCurrentContext()->OpenPopupStack.Size; i++ )
	{
		ImGuiPopupData& data   = ImGui::GetCurrentContext()->OpenPopupStack[ i ];
		ImGuiWindow*    window = data.Window;

		if ( ImGui::IsMouseHoveringRect( window->Pos, { window->Pos.x + window->Size.x, window->Pos.y + window->Size.y } ) )
			return false;
	}

	return true;
}


// =============================================================================================
// Gallery Item Actions/Behavior


// called when file is double clicked or enter is pressed on it
void gallery_selected_item_action( const media_entry_t& media, u32 index )
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


void gallery_view_do_selected_item_behavior( size_t i, gallery_item_draw_t& item_draw )
{
	if ( !( item_draw.selected_item && item_draw.item_hovered ) )
		return;

	SDL_MouseButtonFlags mouse_btns       = SDL_GetMouseState( nullptr, nullptr );

	// mouse down and not hovering an imgui window not in an image pan
	// bool        mouse_middle_down = ImGui::IsMouseDown( ImGuiMouseButton_Middle ) && !( mouse_hover_imgui_window );
	bool                 drag_button_down = ( mouse_btns & SDL_BUTTON_LMASK ) || ( mouse_btns & SDL_BUTTON_RMASK );

	static bool          drag_cooldown    = false;

	if ( drag_button_down )
	{
		// make sure we aren't in this cooldown state, and the mouse is moving
		if ( !drag_cooldown && ( app::mouse_delta[ 0 ] != 0.0 || app::mouse_delta[ 1 ] != 0.0 ) )
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
	else
	{
		drag_cooldown = false;
	}
}


void gallery_view_do_hovered_item_behavior( size_t i, gallery_item_draw_t& item_draw )
{
	bool mouse_release = ( ImGui::IsMouseReleased( ImGuiMouseButton_Left ) || ImGui::IsMouseReleased( ImGuiMouseButton_Middle ) );
	bool mouse_press   = ( ImGui::IsMouseClicked( ImGuiMouseButton_Left ) || ImGui::IsMouseClicked( ImGuiMouseButton_Middle ) );

	if ( !item_draw.item_hovered )
		return;

	// don't try to do any item actions when scrolling
	if ( gallery_draw::scrollbar_active || gallery_draw::scrollbar_active_last_frame )
		return;

	// if ( mouse_release && gallery::selection.size() > 1 )
	if ( mouse_release )
	{
		// the item may be a bit out of frame, scroll a little to have it fully in view
		//gallery_draw::scroll_queued = true;
		gallery::scroll_to_cursor = true;

		if ( gallery_view_input_do_multi_select() )
		{
			// if we want multi select, remove or add the item from selection list
			gallery_view_input_update_multi_select( i, false );
		}
		else if ( item_draw.selected_item )
		{
			// if the item is already selected, but we DONT want multi select, clear the selection list, add readd that the selected item
			gallery::selection.clear();
			gallery_view_input_update_multi_select( i, false );
		}
	}

	if ( mouse_press && !item_draw.selected_item )
	{
		if ( !gallery_view_input_do_multi_select() )
		{
			gallery::selection.clear();
			gallery_view_input_update_multi_select( i, false );

			// the item may be a bit out of frame, scroll a little to have it fully in view
			//gallery_draw::scroll_queued = true;
			//gallery::scroll_to_cursor = true;
		}
	}

	if ( ImGui::IsMouseDoubleClicked( ImGuiMouseButton_Left ) )
	{
		gallery_selected_item_action( *item_draw.media, i );
	}
}


void gallery_view_item_handle_scroll( ImGuiStyle& style, gallery_item_draw_t& item_draw, int window_height, u32 last_selected, size_t selection_count )
{
	// Calculate Distance the Item is from visible scroll area
	float visible_bottom = window_height;
	float visible_top    = window_height - gallery_draw::region_size.y;

	if ( gallery_draw::scroll_changed || directory::folder_changed
#if 1  // here so i can easily disable
	     || gallery::always_recalc_layout
#endif
	)
	{
		u32 distance = 0;

		// check if the bottom of the item is still visible at the top of the content window
		if ( item_draw.item_rect_max.y < visible_top )
			distance = visible_top - item_draw.item_rect_max.y;

		// check if the top of the item is still visible at the bottom of the content window
		else if ( item_draw.item_rect_min.y > visible_bottom )
			distance = item_draw.item_rect_min.y - visible_bottom;

		// if distance is still 0, this item is at least partially on-screen
		thumbnail_update_distance( directory::thumbnail_list[ item_draw.gallery_index ], distance );
	}

	if ( directory::folder_changed )
	{
		// scroll to top
		ImGui::SetScrollY( 0 );
		gallery_draw::scroll      = 0;
		gallery_draw::scroll_prev = 0;
		gallery_draw_extra_refresh();
		return;
	}

	// ----------------------------------------------------------------------------------------------------------
	// If we need to scroll to the selected item this frame
	// adjust the scroll position as needed to keep it on screen

	// bool selection_empty = gallery_view_selection_cleared();

	u32 scroll_to_index = UINT32_MAX;

	if ( selection_count )
	{
		scroll_to_index = last_selected;
	}
	else if ( gallery::keep_scroll_pos )
	{
		scroll_to_index           = gallery_draw::first_visible_item;
		gallery::scroll_to_cursor = true;
	}

	// if ( gallery::selection.size() && last_selected == i && gallery::scroll_to_cursor )
	if ( !gallery_draw::scrollbar_active_last_frame && scroll_to_index == item_draw.i && gallery::scroll_to_cursor )
	// if ( gallery::last_selection.entry.type != e_media_type_none && cache_last_selected == i && gallery::scroll_to_cursor )
	{
		bool scroll_needed = false;
		bool scroll_up     = false;

		// check if the bottom of the item is off-screen at the bottom of the content window
		if ( ( item_draw.item_rect_max.y + style.ItemSpacing.y ) > visible_bottom )
		{
			scroll_up     = false;
			scroll_needed = true;
		}

		// check if the top of the item is off-screen at the top of the content window
		else if ( ( item_draw.item_rect_min.y - style.ItemSpacing.y ) < visible_top )
		{
			scroll_up     = true;
			scroll_needed = true;
		}

		if ( scroll_needed )
		{
			// calculate how much to scroll up or down
			float scroll_offset = 0;

			if ( scroll_up )
				scroll_offset = ( item_draw.item_rect_min.y - style.ItemSpacing.y ) - visible_top;
			//scroll_offset = ( item_draw.item_rect_min.y ) - visible_top;
			else
				scroll_offset = ( item_draw.item_rect_max.y + style.ItemSpacing.y ) - visible_bottom;
			//scroll_offset = ( item_draw.item_rect_max.y ) - visible_bottom;

			// try to keep at the top of the window?
			if ( ( app::window_resized || gallery::content_area_resized ) && !scroll_up )
				scroll_offset = ( item_draw.item_rect_min.y - style.ItemSpacing.y ) - visible_top;

			gallery_draw::scroll += scroll_offset;
			ImGui::SetScrollY( gallery_draw::scroll );
		}

		gallery::scroll_to_cursor = false;
		gallery_draw_extra_refresh();
	}
}



// =============================================================================================
// Gallery Item Drawing


void gallery_view_draw_image( image_t* image, ImTextureRef im_texture, bool upscale, ImVec2& out_image_size )
{
	// Fit image in window size, scaling up if needed
	float factor[ 2 ] = { 1.f, 1.f };

	if ( upscale || image->width > gallery::image_bounds.x )
		factor[ 0 ] = (float)gallery::image_bounds.x / (float)image->width;

	if ( upscale || image->height > gallery::image_bounds.y )
		factor[ 1 ] = (float)gallery::image_bounds.y / (float)image->height;

	float  zoom_level = std::min( factor[ 0 ], factor[ 1 ] );

	ImVec2 image_size{};
	image_size.x = int( image->width * zoom_level );
	image_size.y = int( image->height * zoom_level );

	if ( upscale )
		out_image_size = image_size;

	// center the image
	ImVec2 image_offset = ImGui::GetCursorPos();
	image_offset.x += int( ( gallery::image_bounds.x - image_size.x ) / 2 );
	image_offset.y += int( ( gallery::image_bounds.y - image_size.y ) / 2 );

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


void gallery_view_draw_item_thumbnail( size_t i, gallery_item_draw_t& item_draw, ImVec2& scaled_image_size, bool& drew_base_icon )
{
	if ( item_draw.media->type == e_media_type_directory )
	{
		gallery_view_draw_image( icon_get_image( e_icon_folder ), icon_get_imtexture( e_icon_folder ), true, scaled_image_size );
		return;
	}

	thumbnail_t* thumbnail = nullptr;
	e_icon       base_icon = e_icon_none;

	if ( app::config.thumbnail_enable )
		thumbnail = thumbnail_get_data( directory::thumbnail_list[ item_draw.gallery_index ] );

	if ( item_draw.media->type == e_media_type_directory )
		base_icon = e_icon_folder;
	else if ( item_draw.media->type == e_media_type_image )
		base_icon = e_icon_image;
	else if ( item_draw.media->type == e_media_type_video )
		base_icon = e_icon_video;

	if ( !thumbnail )
	{
		if ( app::config.thumbnail_enable && !thumbnail && item_draw.media->type != e_media_type_directory )
			gallery_draw::thumbnail_requests.emplace_back( *item_draw.media, item_draw.gallery_index );
		// directory::thumbnail_list[ i ] = thumbnail_queue_image( entry );

		//ImGui::Dummy( image_bounds );
		gallery_view_draw_image( icon_get_image( base_icon ), icon_get_imtexture( base_icon ), true, scaled_image_size );
		drew_base_icon = true;
		return;
	}

	if ( thumbnail->status == e_thumbnail_status_finished )
	{
		if ( thumbnail->image_scaled )
			gallery_view_draw_image( thumbnail->image_scaled, thumbnail->textures.frame[ 0 ], true, scaled_image_size );
		else
			gallery_view_draw_image( thumbnail->image, thumbnail->textures.frame[ 0 ], true, scaled_image_size );

		gallery::drawn_image_count++;
	}
	else if ( thumbnail->status == e_thumbnail_status_failed )
	{
		gallery_view_draw_image( icon_get_image( e_icon_invalid ), icon_get_imtexture( e_icon_invalid ), false, scaled_image_size );
	}
	else if ( thumbnail->status == e_thumbnail_status_queued || thumbnail->status == e_thumbnail_status_loading || thumbnail->status == e_thumbnail_status_uploading )
	{
		gallery_view_draw_image( icon_get_image( e_icon_loading ), icon_get_imtexture( e_icon_loading ), false, scaled_image_size );
	}
	else if ( thumbnail->status == e_thumbnail_status_free )
	{
		if ( item_draw.media->type != e_media_type_directory )
			gallery_draw::thumbnail_requests.emplace_back( *item_draw.media, item_draw.gallery_index );

		// ImGui::Dummy( image_bounds );
		gallery_view_draw_image( icon_get_image( base_icon ), icon_get_imtexture( base_icon ), true, scaled_image_size );
		drew_base_icon = true;
	}
	else  // if ( thumbnail->status == e_thumbnail_status_free )
	{
		//ImGui::Dummy( image_bounds );
		gallery_view_draw_image( icon_get_image( base_icon ), icon_get_imtexture( base_icon ), true, scaled_image_size );
		drew_base_icon = true;
	}
}


extern void TextExFast( const char* text, const char* text_end, ImGuiTextFlags flags, const ImVec2& text_size );


void        gallery_view_draw_item_text( ImGuiStyle& style, size_t i, gallery_item_draw_t& item_draw, ImVec2 current_pos, ImVec2 saved_pos )
{
	ImVec2 media_text_size = gallery::item_text_size[ i ];

	// center align text
	ImGui::SetCursorPosX( current_pos.x + ( ( gallery::item_size - ( media_text_size.x + style.WindowPadding.x * 2 + style.ItemSpacing.x ) ) * 0.5f ) );
	ImGui::SetCursorPosY( current_pos.y + gallery::image_bounds.x + style.ItemSpacing.y );

	ImGui::PushTextWrapPos( saved_pos.x + gallery::image_bounds.x + style.ItemSpacing.x );

	// Text Clipping
	ImVec2 window_pos         = ImGui::GetWindowPos();
	ImVec2 current_screen_pos = ImGui::GetCursorScreenPos();

	// ImVec2 text_clip_min( window_pos.x + gallery_draw_info.start_cursor_pos.x, ( window_pos.y + gallery_draw_info.start_cursor_pos.y + gallery::image_bounds.x + ( style.ItemSpacing.y * 2 ) ) - ImGui::GetScrollY() );
	//ImVec2 text_clip_min = item_draw.item_rect_min;
	ImVec2 text_clip_min      = current_screen_pos;
	ImVec2 text_clip_max      = item_draw.item_rect_max;

	//text_clip_min.y += gallery::image_bounds.y + style.ItemSpacing.y;

	float  text_height        = text_clip_max.y - text_clip_min.y;
	float  font_height        = ImGui::GetFontSize();

	// clip off extra text getting cut off half way
	float  result             = fmod( text_height, font_height );
	//float  result        = floor( text_height / font_height );
	//text_clip_max.y      = text_clip_min.y + ( result * font_height );
	text_clip_max.y           = text_clip_max.y - result;

	ImGui::PushClipRect( text_clip_min, text_clip_max, true );

	// draw clipping box for debug if needed
	//ImDrawList* draw_list  = ImGui::GetWindowDrawList();
	//ImColor clip_color = style.Colors[ ImGuiCol_Border ];
	//draw_list->AddRect( text_clip_min, text_clip_max, clip_color, 0, ImDrawFlags_None, 2.f );

	//ImGui::TextUnformatted( item_draw.media.filename.c_str() );
	TextExFast( item_draw.media->filename.c_str(), nullptr, ImGuiTextFlags_NoWidthForLargeClippedText, media_text_size );

	ImGui::PopTextWrapPos();
	ImGui::PopClipRect();
}


void gallery_view_draw_item_content( ImGuiStyle& style, size_t i, gallery_item_draw_t& item_draw )
{
	ImDrawList* draw_list   = ImGui::GetWindowDrawList();
	ImVec2      window_pos  = ImGui::GetWindowPos();

	item_draw.selected_item = false;

	for ( selection_t& selection : gallery::selection )
	{
		if ( selection.index != i )
			continue;

		item_draw.selected_item = true;
		break;
	}

	if ( item_draw.selected_item && !item_draw.item_hovered && i == gallery_draw::last_hovered )
	{
		set_frame_draw( 2 );
	}

	if ( item_draw.item_hovered && i != gallery_draw::last_hovered )
	{
		gallery_draw::last_hovered = i;
		set_frame_draw( 2 );
	}

	if ( item_draw.selected_item && i != gallery_draw::last_selected )
	{
		gallery_draw::last_selected = i;
		set_frame_draw( 2 );
	}

	// Draw a background if needed
	if ( item_draw.selected_item || item_draw.item_hovered )
	{
		// why is this not using Active color?
		ImColor color_base   = style.Colors[ ImGuiCol_FrameBg ];
		ImColor color_hover  = style.Colors[ ImGuiCol_FrameBgHovered ];
		ImColor color_active = style.Colors[ ImGuiCol_FrameBgActive ];
		ImColor color_border = style.Colors[ ImGuiCol_Border ];

		ImColor color        = color_base;

		if ( item_draw.item_hovered )
			color = item_draw.selected_item ? color_active : color_hover;

		draw_list->AddRectFilled( item_draw.item_rect_min, item_draw.item_rect_max, color, style.ChildRounding, ImDrawFlags_RoundCornersAll );

		// if ( style.FrameBorderSize )
		draw_list->AddRect( item_draw.item_rect_min, item_draw.item_rect_max, color_border, style.ChildRounding, ImDrawFlags_RoundCornersAll );
	}

	// draw a border around it if it was last selected
	if ( !item_draw.selected_item && gallery::last_selection.entry.type != e_media_type_none && gallery::last_selection.index == i )
	{
		// why is this not using Active color?
		// ImColor main_bg_color     = item_hovered ? style.Colors[ ImGuiCol_FrameBgHovered ] : style.Colors[ ImGuiCol_FrameBg ];
		ImColor main_bg_color = style.Colors[ ImGuiCol_FrameBg ];
		draw_list->AddRect( item_draw.item_rect_min, item_draw.item_rect_max, main_bg_color, style.ChildRounding, ImDrawFlags_RoundCornersAll );
	}

	ImVec2 current_pos = ImGui::GetCursorPos();
	ImVec2 saved_pos   = ImGui::GetCursorPos();

	current_pos.x += style.WindowPadding.x;
	current_pos.y += style.WindowPadding.y;
	ImGui::SetCursorPos( current_pos );

	// ----------------------------------------------------------------------------------------------------------
	// Draw Thumbnail or Icon

	ImVec2 scaled_image_size{};  // size of image that was drawn
	bool   drew_icon = false;    // was the icon drawn instead of a thumbnail?

	// draw clipping box for debug if needed
	//ImGuiStyle&          style      = ImGui::GetStyle();
	//ImDrawList*          draw_list  = ImGui::GetWindowDrawList();
	//ImColor              clip_color = style.Colors[ ImGuiCol_Border ];
	//
	//ImVec2               image_min  = ImGui::GetCursorScreenPos();
	//ImVec2               image_max  = image_min;
	//image_max.x += gallery::image_bounds.x;
	//image_max.y += gallery::image_bounds.y;
	//
	//draw_list->AddRect( image_min, image_max, clip_color, 0, ImDrawFlags_None );

	gallery_view_draw_item_thumbnail( i, item_draw, scaled_image_size, drew_icon );

	// ----------------------------------------------------------------------------------------------------------
	// Draw icon on top of it in the bottom right corner

	// if we drew the media icon, no need to draw the icon overlay to indicate it's a video
	if ( item_draw.media->type == e_media_type_video && !drew_icon )
	{
		// Fit image in window size, scaling up if needed
		float    factor[ 2 ]       = { 1.f, 1.f };

		image_t* icon_video        = icon_get_image( e_icon_video );

		ImVec2   image_icon_bounds = { gallery::image_bounds.x / 4.f, gallery::image_bounds.y / 4.f };

		//if ( image->width > image_bounds.x )
		factor[ 0 ]                = (float)image_icon_bounds.x / (float)icon_video->width;

		//if ( image->height > image_bounds.y )
		factor[ 1 ]                = (float)image_icon_bounds.y / (float)icon_video->height;

		float  zoom_level          = std::min( factor[ 0 ], factor[ 1 ] );

		ImVec2 scaled_icon_size{};
		scaled_icon_size.x              = icon_video->width * zoom_level;
		scaled_icon_size.y              = icon_video->height * zoom_level;

		ImVec2 image_offset             = saved_pos;
		float  image_offset_from_side_x = 0.f;
		float  image_offset_from_side_y = 0.f;

		if ( scaled_image_size.x )
		{
			image_offset_from_side_x = ( gallery::image_bounds.x - scaled_image_size.x ) / 2.f;
			image_offset_from_side_y = ( gallery::image_bounds.y - scaled_image_size.y ) / 2.f;
		}

		// TODO: this doesn't work as well at different zoom levels
		image_offset.x += ( gallery::image_bounds.x - image_offset_from_side_x ) - ( scaled_icon_size.x / 1.25f );
		image_offset.y += ( gallery::image_bounds.y - image_offset_from_side_y ) - ( scaled_icon_size.y / 1.25f );

		ImGui::SetCursorPos( image_offset );

		ImGui::Image( icon_get_imtexture( e_icon_video ), scaled_icon_size );
	}

	// ----------------------------------------------------------------------------------------------------------
	// Draw Text

	gallery_view_draw_item_text( style, i, item_draw, current_pos, saved_pos );
}


// =============================================================================================
// Gallery Layout


void gallery_view_item_size_calc( ImGuiStyle& style, size_t count )
{
	// do an extra refresh next frame
	//gallery_draw_extra_refresh();

	gallery::image_bounds = { gallery::item_size - ( style.WindowPadding.x * 2 ), gallery::item_size - ( style.WindowPadding.x * 2 ) };

	for ( size_t i = 0; i < count; i++ )
	{
		gallery_item_draw_t& layout        = gallery::item_layout[ i ];
		size_t               gallery_index = gallery::sorted_media[ i ];
		const media_entry_t& media         = directory::media_list[ gallery_index ];
		gallery::item_text_size[ i ]       = ImGui::CalcTextSize( media.filename.c_str(), 0, false, gallery::item_size - ( style.WindowPadding.x * 2 ) );

		layout.i                           = i;
		layout.gallery_index               = gallery::sorted_media[ i ],
		layout.media                       = &directory::media_list[ layout.gallery_index ];

		if ( app::config.gallery_show_filenames )
		{
			layout.text_size = gallery::item_text_size[ i ];
		}

		// Calculate Current Item Height, and store tallest height for current row
		layout.item_size_y = gallery::image_bounds.y + ( style.WindowPadding.y * 2 );

		if ( app::config.gallery_show_filenames )
			layout.item_size_y += layout.text_size.y + style.ItemSpacing.y;

		layout.item_size_y = std::min( layout.item_size_y, gallery_draw::item_size_y * 1.75f );
	}
}


// the IsRectVisible Imgui function converts both ImVec2's to a ImRect structure,
// but that's slow in this loop with potentially over 100,000 items (i've tested on 400,000)
#define IsRectVisibleFast( window, rect_min, rect_max ) \
	( window->ClipRect.Min.y < rect_max.y && window->ClipRect.Max.y > rect_min.y && window->ClipRect.Min.x < rect_max.x && window->ClipRect.Max.x > rect_min.x )


void gallery_view_item_rect_calc( ImGuiWindow* window, ImGuiStyle& style, size_t count )
{
	int window_width, window_height;
	SDL_GetWindowSize( app::window, &window_width, &window_height );

	u32   row_x                   = 0;
	u32   row_y                   = 0;

	u32   last_visible_top_row    = UINT32_MAX;
	u32   last_visible_bottom_row = 0;
	float row_max_item_height     = 0.f;
	//float  last_grid_row_y         = 0.f;

	float visible_bottom          = window_height;
	float visible_top             = window_height - gallery_draw::region_size.y;

	visible_bottom -= gallery_draw::scroll;
	visible_top -= gallery_draw::scroll;

	visible_bottom += style.WindowPadding.y;
	visible_top += style.WindowPadding.y;

	ImVec2 fake_cursor_pos     = ImGui::GetCursorScreenPos();
	ImVec2 start_cursor_pos    = fake_cursor_pos;
	//ImVec2 cursor_screen_pos = ImGui::GetCursorScreenPos();

	gallery_draw::dummy_area.y = 0;

	// do an extra frame draw just in case
	set_frame_draw( 2 );

	// slow call, save the values
	u32                  last_selected   = gallery_view_get_last_selected_index();
	size_t               selection_count = gallery::selection.size();

	// slow converting from u32 to float a lot per frame?
	float                item_size_x     = gallery::item_size;

	// fast pointer offsetting
	gallery_item_draw_t* layout_ptr      = &gallery::item_layout[ 0 ];
	gallery::visible_item_count          = 0;

	memset( gallery::visible_item, 0, sizeof( void* ) * ( gallery::sorted_media.size() + 2 ) );

	for ( size_t i = 0; i < count; i++ )
	{
		gallery_item_draw_t& layout = *( layout_ptr + i );

		if ( row_x == gallery::row_count )
		{
			row_x = 0;
			row_y++;

			fake_cursor_pos.x = start_cursor_pos.x;
			fake_cursor_pos.y += row_max_item_height + style.ItemSpacing.y;

			gallery_draw::dummy_area.y += row_max_item_height + style.ItemSpacing.y;

			row_max_item_height = gallery_draw::item_size_y;
		}
		else if ( row_x > 0 )
		{
			fake_cursor_pos.x += gallery_draw::item_spacing_x + gallery::item_size;
		}

		row_x++;

		// layout.cursor_screen_pos = fake_cursor_pos;
		layout.cursor_screen_pos.x = fake_cursor_pos.x;
		layout.cursor_screen_pos.y = fake_cursor_pos.y;

		//layout.item_rect_min       = fake_cursor_pos;
		layout.item_rect_min.x     = fake_cursor_pos.x;
		layout.item_rect_min.y     = fake_cursor_pos.y;

		//layout.item_rect_max     = { fake_cursor_pos.x + gallery::item_size, fake_cursor_pos.y + layout.item_size_y };
		layout.item_rect_max.x     = fake_cursor_pos.x + item_size_x;
		layout.item_rect_max.y     = fake_cursor_pos.y + layout.item_size_y;

		if ( row_max_item_height < layout.item_size_y )
			row_max_item_height = layout.item_size_y;

		layout.visible = IsRectVisibleFast( window, layout.item_rect_min, layout.item_rect_max );

		if ( layout.visible )
			gallery::visible_item[ gallery::visible_item_count++ ] = &layout;

		gallery_view_item_handle_scroll( style, layout, window_height, last_selected, selection_count );
	}

	// add the final row
	gallery_draw::dummy_area.y += row_max_item_height + style.ItemSpacing.y;

	gallery::scroll_to_cursor = false;
}


// =============================================================================================
// Main Gallery Item Loop


void gallery_view_draw_item( ImGuiStyle& style, size_t i, u32& grid_pos_x, gallery_item_draw_t& item_draw )
{
	bool was_hovered = item_draw.item_hovered;

	if ( gallery_draw::content_area_hovered )
	{
		item_draw.item_hovered = ImGui::IsMouseHoveringRect( item_draw.item_rect_min, item_draw.item_rect_max );
		gallery_draw::any_item_hovered |= item_draw.item_hovered;

		// extra frame draw for the file information, so it can get an extra frame to resize it's content
		if ( item_draw.item_hovered != was_hovered )
			set_frame_draw( 4 );  // oh my god bruh
	}
	else
	{
		item_draw.item_hovered = false;
	}

	gallery_view_draw_item_content( style, i, item_draw );
	gallery_view_do_selected_item_behavior( i, item_draw );
	gallery_view_do_hovered_item_behavior( i, item_draw );
}


void gallery_view_draw_items( ImGuiWindow* window, ImGuiStyle& style, size_t count )
{
	u32                  grid_pos_x = 0;
	size_t               i          = 0;
	gallery_item_draw_t* item_draw  = gallery::visible_item[ 0 ];

	if ( !gallery_draw::lock_visible_item )
		gallery::first_visible_item = item_draw->i;

	//if ( !gallery_draw::lock_visible_item || gallery::first_visible_item == UINT32_MAX )
	//	gallery::first_visible_item = i;

	for ( ; i < gallery::visible_item_count; i++ )
	{
		item_draw            = gallery::visible_item[ i ];
		window->DC.CursorPos = item_draw->cursor_screen_pos;
		window->DC.IsSetPos  = true;

		gallery_view_draw_item( style, item_draw->i, grid_pos_x, *item_draw );
		grid_pos_x++;
	}
}


// =============================================================================================
// Gallery View Content Window


void gallery_view_handle_context_menu()
{
	if ( !ImGui::BeginPopupContextWindow( "##gallery ctx menu", ImGuiPopupFlags_AnyPopup | ImGuiPopupFlags_MouseButtonRight ) )
		return;

	ImGuiStyle&   style         = ImGui::GetStyle();
	ImVec2        region_avail  = ImGui::GetContentRegionAvail();

	u32           last_selected = gallery_view_get_last_selected_index( UINT32_MAX );
	media_entry_t media_entry   = gallery_view_get_last_selected_entry();

	bool          folder        = last_selected == UINT32_MAX;

	// make sure we have at least ONE image here, or this gets stuck and hangs forever lol
	bool          valid         = false;
	for ( size_t i : gallery::sorted_media )
	{
		const media_entry_t& entry = directory::media_list[ i ];

		if ( entry.type == e_media_type_image || entry.type == e_media_type_video )
		{
			valid = true;
			break;
		}
	}

	if ( ImGui::MenuItem( folder ? "Open" : "View", nullptr, false, valid ) )
	{
		gallery_selected_item_action( media_entry, folder ? 0 : last_selected );
	}

	ImGui::Separator();

	if ( ImGui::MenuItem( folder ? "Open in Explorer" : "Open File Location", nullptr, false, true ) )
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
						paths.push_back( directory::path / selection.entry.file.path );
				}
				else
				{
					paths.push_back( directory::path / selection.entry.file.path );
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

	if ( ImGui::MenuItem( "Delete", nullptr, false, !folder ) )
	{
		gallery_view_delete_selection();
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


void gallery_view_handle_scroll_event( float mouse_y )
{
	if ( !g_gallery_view )
		return;

	if ( !gallery_draw::content_area_hovered )
		return;

	ImGuiStyle& style       = ImGui::GetStyle();

	//int window_width, window_height;
	//SDL_GetWindowSize( app::window, &window_width, &window_height );

	// calculate the max scroll area
	// float scroll_size = MAX( 0.0f, ( gallery_draw::dummy_area.y + style.ItemSpacing.y ) - window_height );
	float       scroll_size = MAX( 0.0f, ( gallery_draw::dummy_area.y - gallery_draw::region_size.y ) + style.WindowPadding.y * 2 );

	if ( scroll_size == 0.f )
		return;

	// get the max item height of the 1st visible row
	// also account for each scroll step with fast scrolling
	float  max_item_height = gallery_draw::item_size_y;
	float  scroll_amount   = 0;
	size_t i               = gallery::first_visible_item;
	size_t row_i           = 0;
	s64    scroll_step_add = ( mouse_y < 0 ) ? 1i64 : -1i64;
	s64    scroll_step     = 0;
	s64    scroll_end      = -static_cast< s64 >( mouse_y );

	if ( scroll_step_add == -1 )
	{
		// if we can still see the first item, just snap to the top
		if ( gallery::first_visible_item == 0 )
		{
			gallery_draw::scroll_changed = true;
			gallery_draw::scroll         = 0;
			return;
		}

		// offset back a row
		i--;
	}

	for ( ; i < gallery::sorted_media.size(); )
	{
		gallery_item_draw_t& item_draw = gallery::item_layout[ i ];
		max_item_height                = std::max( max_item_height, item_draw.item_size_y );

		if ( ++row_i == gallery::row_count )
		{
			row_i = 0;

			scroll_amount += max_item_height + style.ItemSpacing.y;
			max_item_height = gallery_draw::item_size_y;

			scroll_step += scroll_step_add;

			if ( scroll_step == scroll_end )
				break;
		}

		if ( scroll_step_add == -1 && i == 0 )
			break;

		i += scroll_step_add;
	}

	// TODO: Factor in the different text sizes for each row
	//float scroll_amount          = max_item_height + style.ItemSpacing.y;

	gallery_draw::scroll_changed = true;
	//gallery_draw::scroll_queued  = true; // do another update next frame for rect layout
	//gallery_draw::scroll -= scroll_amount * mouse_y;
	gallery_draw::scroll += scroll_amount * scroll_step_add;

	// clamp it to the max scroll area
	gallery_draw::scroll = CLAMP( gallery_draw::scroll, 0.f, scroll_size );
}


void gallery_view_draw_content()
{
	int window_width, window_height;
	SDL_GetWindowSize( app::window, &window_width, &window_height );

	ImGuiStyle& style              = ImGui::GetStyle();
	ImVec2      content_cursor_pos = ImGui::GetCursorPos();
	ImVec2      mouse_pos          = ImGui::GetMousePos();

	ImGui::SetCursorPosX( std::max( 0.f, content_cursor_pos.x - style.ItemSpacing.x ) );

	ImVec2 region_avail        = ImGui::GetContentRegionAvail();
	gallery_draw::region_size  = { region_avail.x + style.WindowPadding.x, region_avail.y + style.WindowPadding.y };

	int region_x               = region_avail.x - ( style.ScrollbarSize + style.WindowPadding.x );
	gallery_draw::dummy_area.x = region_x;

	ImGui::SetNextWindowSize( gallery_draw::region_size );

	//float content_height = MAX( gallery_draw::region_size.y, gallery_draw::dummy_area.y + style.WindowPadding.y * 2 );
	//ImGui::SetNextWindowContentSize( { gallery_draw::region_size.x, content_height } );

	gallery_draw::content_area_hovered = is_content_area_hovered( region_avail.x, window_height );

	static size_t last_item_count      = 0;
	size_t        count                = gallery::sorted_media.size();
	bool          item_count_changed   = count != last_item_count;

	if ( gallery_draw::scroll_changed )
		ImGui::SetNextWindowScroll( { 0.f, gallery_draw::scroll } );

	else if ( directory::folder_changed )
	{
		gallery_draw::scroll_prev    = 0.f;
		gallery_draw::scroll         = 0.f;
		gallery_draw::scroll_changed = true;
		gallery::scroll_to_cursor    = false;

		ImGui::SetNextWindowScroll( { 0.f, 0.f } );
	}

	// if ( !ImGui::BeginChild( "##gallery_content", { region_avail.x + style.WindowPadding.x, region_avail.y }, ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollWithMouse ) )
	if ( !ImGui::BeginChild( "##gallery_content", {}, ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBringToFrontOnFocus ) )
	{
		ImGui::EndChild();
		return;
	}

	if ( gallery::sorted_media.empty() )
	{
		ImGui::EndChild();
		return;
	}

	ImGuiWindow* window            = ImGui::GetCurrentWindow();
	ImGuiID      active_id         = ImGui::GetActiveID();

	// bool         scrollbar_active             = active_id && ( active_id == ImGui::GetWindowScrollbarID( window, ImGuiAxis_X ) || active_id == ImGui::GetWindowScrollbarID( window, ImGuiAxis_Y ) );
	gallery_draw::scrollbar_active = active_id && active_id == ImGui::GetWindowScrollbarID( window, ImGuiAxis_Y );
	gallery_draw::scroll_prev      = gallery_draw::scroll;
	gallery_draw::scroll           = ImGui::GetScrollY();
	gallery_draw::any_item_hovered = false;

	gallery_draw::thumbnail_requests.clear();

	// ----------------------------------------------------------------------------------------------------------
	// Row Count

	static u32 last_row_count = 0;
	last_row_count            = gallery::row_count;
	gallery::row_count        = std::max( 1U, region_x / u32( gallery::item_size + style.ItemSpacing.x ) );

	// ----------------------------------------------------------------------------------------------------------

	gallery::keep_scroll_pos |= last_row_count != gallery::row_count;

	gallery_draw::scroll_changed |= gallery::scroll_to_cursor || gallery_draw::scrollbar_active || gallery_draw::scrollbar_active_last_frame;

	// float item_size_x  = gallery::item_size - style.ItemSpacing.x;
	//float item_size_x  = gallery::item_size;
	gallery_draw::item_size_y = gallery::item_size;

	if ( app::config.gallery_show_filenames )
		gallery_draw::item_size_y += ImGui::GetFontSize() + style.ItemSpacing.y;

	// ----------------------------------------------------------------------------------------------------------
	// Item Spacing

	float spacing_x_base = float( region_x ) - float( gallery::item_size * gallery::row_count );

	if ( gallery::row_count > 2 )
		gallery_draw::item_spacing_x = spacing_x_base / ( gallery::row_count - 1 );
	else
		gallery_draw::item_spacing_x = spacing_x_base / ( gallery::row_count + 1 );

	gallery_draw::item_spacing_x = std::max( 0.f, gallery_draw::item_spacing_x );

	// Offset the starting X position if we have a low row count, imo it looks better when at 1 or 2 items per row
	if ( gallery::row_count <= 2 )
		ImGui::SetCursorPosX( ImGui::GetCursorPosX() + gallery_draw::item_spacing_x );

	// ----------------------------------------------------------------------------------------------------------

	bool               row_count_changed    = last_row_count != gallery::row_count;
	static bool        filenames_shown_last = app::config.gallery_show_filenames;
	static ImVec2      last_region_avail    = region_avail;

	static h_thumbnail icons_scaled[ e_icon_count ]{};

	gallery::drawn_image_count       = 0;
	gallery_draw::first_visible_item = gallery::first_visible_item;
	gallery::content_area_resized |= app::window_resized || row_count_changed || last_region_avail != region_avail;

	gallery::keep_scroll_pos = gallery::item_size_changing;
	gallery::keep_scroll_pos |= filenames_shown_last != app::config.gallery_show_filenames;

	if ( !directory::folder_changed )
	{
		gallery::keep_scroll_pos |= gallery::scroll_to_cursor;
		gallery::keep_scroll_pos |= gallery::content_area_resized;
	}

	gallery_draw::lock_visible_item = gallery::keep_scroll_pos || gallery::content_area_resized;

	gallery_draw::scroll_changed |= gallery::keep_scroll_pos;

	bool no_extra_refresh = gallery::refresh_layout == 0;

	// ----------------------------------------------------------------------------------------------------------
	// Item Layout

	// TODO: for groups, change how sorted media is handled ?
	// maybe make gallery::grouped_media ? or just have inserts into groups here

	// if size changed, recalculate the text sizes
	//if ( gallery::item_size_changed || app::window_resized || region_avail != last_region_avail || gallery_draw::scroll_changed )
	if ( gallery::item_size_changed || directory::folder_changed || gallery::always_recalc_item_sizes )
	{
		gallery_view_item_size_calc( style, count );
	}

	bool recalc_item_rects = gallery::always_recalc_layout;
	recalc_item_rects |= gallery::item_size_changed;
	recalc_item_rects |= gallery::content_area_resized;
	recalc_item_rects |= gallery_draw::scroll_changed;
	recalc_item_rects |= directory::folder_changed;
	recalc_item_rects |= gallery::refresh_layout > 0;

	if ( recalc_item_rects )
	{
		gallery_view_item_rect_calc( window, style, count );
	}

	// ----------------------------------------------------------------------------------------------------------
	// Item Drawing

	ImVec2 dummy_start_pos        = ImGui::GetCursorPos();
	ImVec2 dummy_start_pos_screen = ImGui::GetCursorScreenPos();

	if ( gallery::visible_item_count > 0 )
		gallery_view_draw_items( window, style, count );

	// Dummy Widget to fill the entire content area
	ImGui::SetCursorPos( dummy_start_pos );
	ImGui::Dummy( gallery_draw::dummy_area );

	//ImDrawList* draw_list = ImGui::GetWindowDrawList();

	// why is this not using Active color?
	// ImColor main_bg_color     = item_hovered ? style.Colors[ ImGuiCol_FrameBgHovered ] : style.Colors[ ImGuiCol_FrameBg ];
	//ImColor main_bg_color = ImVec4( 128, 0, 0, 255 );
	//draw_list->AddRect( dummy_start_pos_screen, dummy_start_pos_screen + gallery_draw::dummy_area, main_bg_color, style.ChildRounding, ImDrawFlags_RoundCornersAll );

	gallery_view_handle_context_menu();

	ImGui::EndChild();

	// if no item was hovered this frame, then clear last hovered
	if ( !gallery_draw::any_item_hovered && gallery_draw::last_hovered != SIZE_MAX )
	{
		gallery_draw::last_hovered = SIZE_MAX;
		set_frame_draw();
	}

	// ----------------------------------------------------------------------------------------------------------
	// Handle global input behavior

	if ( !gallery_draw::any_item_hovered && gallery_draw::content_area_hovered )
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
		set_frame_draw();
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

	for ( size_t i = 0; i < gallery_draw::thumbnail_requests.size(); i++ )
		directory::thumbnail_list[ gallery_draw::thumbnail_requests[ i ].index ] = thumbnail_loader_queue_push( gallery_draw::thumbnail_requests[ i ].media );

	if ( gallery::sort_mode_update )
		gallery_draw_extra_refresh();

	// adjust saved vars
	gallery::sort_mode_update                 = false;
	gallery::content_area_resized             = false;
	gallery::item_size_changed                = false;
	gallery_draw::scroll_changed              = false;
	filenames_shown_last                      = app::config.gallery_show_filenames;
	gallery_draw::scrollbar_active_last_frame = gallery_draw::scrollbar_active;
	last_region_avail                         = region_avail;
	last_item_count                           = gallery::sorted_media.size();

	if ( !no_extra_refresh && gallery::refresh_layout > 0 )
		gallery::refresh_layout--;
}

