#include "main.h"

#include "imgui_internal.h"

#include <unordered_map>
#include <unordered_set>


static std::unordered_map< fs::path, directory_entry_t > g_directory_entries{};
static std::unordered_map< fs::path, folder_scan_status_t* > g_directory_entry_status{};
// static std::vector< directory_entry_t >       g_directory_entries{};
// static std::unordered_map< fs::path, size_t > g_directory_entry_map{};
static std::unordered_set< fs::path >         g_failed_directories{};


void dir_tree_watch_changes()
{
}


void dir_tree_add_folder_callback( folder_scan_status_t* status )
{
	if ( !status )
		return;

	// scan failed
	if ( !status->result )
	{
		g_failed_directories.emplace( status->root );
		return;
	}

	directory_entry_t directory_entry{
		.path    = status->root,
		.folders = status->files,
		.valid   = true
	};

	g_directory_entries[ status->root ] = directory_entry;

	g_directory_entry_status.erase( status->root );

	folder_scan_free( status );
}


void dir_tree_add_folder( fs::path& path )
{
	std::string current_scan{};

	size_t path_i = 0;
	for ( fs::path::iterator it = path.begin(); it != path.end(); it++ )
	{
#if _WIN32
		if ( path_i == 1 )
		{
			path_i++;
			continue;
		}
#endif

		std::string filename = sys_path_to_string( *it );

		// ??
		if ( filename.empty() )
		{
			path_i++;
			continue;
		}

		current_scan += filename;
		current_scan += SEP_S;

		auto dir_it = g_directory_entries.find( current_scan );

		if ( dir_it != g_directory_entries.end() )
		{
			path_i++;
			continue;
		}

		// scan for a new folder
		g_directory_entry_status[ current_scan ] = folder_scan_push( current_scan.c_str(), e_scandir_no_files, dir_tree_add_folder_callback );
		path_i++;
	}
}


//size_t dir_tree_add_folder( fs::path& path )
//{
//	// split into chunks
//	std::vector< std::string > dir_tree_path_chunks;
//
//	std::vector< file_t >      dir_tree;
//
//	std::string                current_scan{};
//
//	size_t path_i = 0;
//	for ( fs::path::iterator it = path.begin(); it != path.end(); it++ )
//	{
//#if _WIN32
//		if ( path_i == 1 )
//		{
//			path_i++;
//			continue;
//		}
//#endif
//
//		std::string filename = sys_path_to_string( *it );
//		dir_tree_path_chunks.push_back( filename );
//		current_scan += filename;
//		current_scan += SEP_S;
//
//		auto dir_it = g_directory_entry_map.find( current_scan );
//
//		if ( dir_it != g_directory_entry_map.end() )
//		{
//			path_i++;
//			continue;
//		}
//
//		// scan for a new folder
//		directory_entry_t directory_entry{};
//
//		if ( sys_scandir( current_scan.c_str(), directory_entry.folders, e_scandir_no_files ) )
//		{
//			size_t index = g_directory_entries.size();
//			g_directory_entries.push_back( directory_entry );
//			g_directory_entry_map[ current_scan ] = index;
//		}
//		else
//		{
//			g_failed_directories.emplace( current_scan );
//		}
//
//		path_i++;
//	}
//}


folder_scan_status_t* dir_tree_get_scan_status( fs::path& path )
{
	auto status_it = g_directory_entry_status.find( path );

	if ( status_it == g_directory_entry_status.end() )
		return nullptr;

	return status_it->second;
}


directory_entry_t* dir_tree_get( fs::path& path )
{
	auto dir_it = g_directory_entries.find( path );

	if ( dir_it != g_directory_entries.end() )
		return &dir_it->second;

	// is this a failed directory?
	auto fail_it = g_failed_directories.find( path );

	if ( fail_it != g_failed_directories.end() )
		return nullptr;

	// is this directory being scanned?
	auto status_it = g_directory_entry_status.find( path );

	if ( status_it != g_directory_entry_status.end() )
		return nullptr;

	// no results, scan it
	dir_tree_add_folder( path );
	return nullptr;
}


static bool g_dir_change_from_dir_tree = false;


extern void TreeNodeStoreStackData( ImGuiTreeNodeFlags flags, float x1 );


// oh my god bruh
bool        TreeNodeBehaviorStupid( ImGuiID id, ImGuiTreeNodeFlags flags, const char* label, const char* label_end, bool& label_pressed )
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
void                              sidebar_draw_directory_recursive( u32 depth, const std::string& current_path )
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

	bool   tree_opened   = TreeNodeBehaviorStupid( static_cast< ImGuiID >( hash ), node_flags, folder_name ? folder_name : current_path.c_str(), nullptr, label_pressed );

	if ( label_pressed )
	{
		//printf( "CLICKED\n" );
		g_dir_change_from_dir_tree = true;
		directory::queued          = current_path;
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
	else
	{
		// are we scanning the directory?
		folder_scan_status_t* status = dir_tree_get_scan_status( evil );

		if ( status )
		{
			ImGui::BeginDisabled();
			ImGui::TextUnformatted( "Scanning..." );
			ImGui::EndDisabled();
		}
	}

	ImGui::TreePop();
}


extern float g_file_info_height;


void dir_tree_draw( ImGuiStyle& style )
{
	ImGui::PushFont( font::normal_bold, style.FontSizeBase + 2.f );

	if ( !ImGui::CollapsingHeader( "Folders", ImGuiTreeNodeFlags_DefaultOpen ) )
	{
		ImGui::PopFont();
		return;
	}

	ImGui::PopFont();

	ImVec2 dir_tree_size = ImGui::GetContentRegionAvail();
	// dir_tree_size.y -= g_file_info_height + ( style.ItemSpacing.y * 2 );
	dir_tree_size.y -= g_file_info_height;

	if ( !ImGui::BeginChild( "##directory_tree", dir_tree_size, ImGuiChildFlags_FrameStyle ) )
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

