#include "main.h"

#include "imgui_internal.h"

#include <unordered_map>
#include <unordered_set>


static std::unordered_map< fs::path, directory_entry_t >     g_directory_entries{};
static std::unordered_map< fs::path, folder_scan_status_t* > g_directory_entry_status{};
// static std::vector< directory_entry_t >       g_directory_entries{};
// static std::unordered_map< fs::path, size_t > g_directory_entry_map{};
static std::unordered_set< fs::path >                        g_failed_directories{};


static std::vector< std::string >                            g_filesystem_browser_path_chunks{};


void dir_tree_watch_changes()
{
}


void dir_tree_add_folder_callback( folder_scan_status_t* status, bool in_main_thread )
{
	if ( !status )
		return;

	fs::path path = status->root;

	// scan failed
	if ( !status->result )
	{
		g_failed_directories.emplace( path );
		return;
	}

	auto dir_it = g_directory_entries.find( path );

	// already handled earlier
	if ( dir_it != g_directory_entries.end() )
		return;

	directory_entry_t directory_entry{
		.path    = path,
		.folders = status->files,
		.valid   = true
	};

	g_directory_entries[ path ] = directory_entry;

	g_directory_entry_status.erase( path );
}


void dir_tree_add_folder( fs::path& path )
{
	fs::path_str current_scan{};

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

		const fs::path_str& filename = *it;

		// ??
		if ( filename.empty() )
		{
			path_i++;
			continue;
		}

		current_scan += filename;
		current_scan += SEP;

		auto dir_it = g_directory_entries.find( current_scan );

		if ( dir_it != g_directory_entries.end() )
		{
			path_i++;
			continue;
		}

		// scan for a new folder
		g_directory_entry_status[ current_scan ] = folder_scan_push( current_scan.c_str(), e_scandir_no_files | e_scandir_no_paths, dir_tree_add_folder_callback );
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


extern bool TreeNodeBehaviorStupid( ImGuiID id, ImGuiTreeNodeFlags flags, const char* label, const char* label_end, bool& label_pressed );


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
		if ( entry->folders.empty() )
		{
			ImGui::BeginDisabled();
			ImGui::TextUnformatted( "Empty" );
			ImGui::EndDisabled();
		}
		else
		{
			for ( const file_t& folder : entry->folders )
			{
				tmp_path = current_path;

				if ( !current_path.ends_with( SEP ) )
					tmp_path += SEP;

				tmp_path += folder.name;

				sidebar_draw_directory_recursive( depth + 1, tmp_path );
			}
		}
	}
	else
	{
		ImGui::BeginDisabled();

		// are we scanning the directory?
		folder_scan_status_t* status = dir_tree_get_scan_status( evil );

		if ( status )
			ImGui::TextUnformatted( "Scanning..." );
		else
			ImGui::TextUnformatted( "Failed to Search" );

		ImGui::EndDisabled();
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

