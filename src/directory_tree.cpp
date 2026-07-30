#include "main.h"

#include <unordered_map>
#include <unordered_set>


static std::unordered_map< fs::path, directory_entry_t > g_directory_entries{};
// static std::vector< directory_entry_t >       g_directory_entries{};
// static std::unordered_map< fs::path, size_t > g_directory_entry_map{};
static std::unordered_set< fs::path >         g_failed_directories{};


void dir_tree_watch_changes()
{
}


void dir_tree_add_folder( fs::path& path )
{
	// split into chunks
	std::vector< std::string > dir_tree_path_chunks;

	std::vector< file_t >      dir_tree;

	std::string                current_scan{};

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
		dir_tree_path_chunks.push_back( filename );
		current_scan += filename;
		current_scan += SEP_S;

		auto dir_it = g_directory_entries.find( current_scan );

		if ( dir_it != g_directory_entries.end() )
		{
			path_i++;
			continue;
		}

		// scan for a new folder
		directory_entry_t directory_entry{};

		if ( sys_scandir( current_scan.c_str(), nullptr, directory_entry.folders, e_scandir_no_files ) )
		{
			g_directory_entries[ current_scan ] = directory_entry;
		}
		else
		{
			g_failed_directories.emplace( current_scan );
		}

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
//		if ( sys_scandir( current_scan.c_str(), nullptr, directory_entry.folders, e_scandir_no_files ) )
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


directory_entry_t* dir_tree_get( fs::path& path )
{
	auto dir_it = g_directory_entries.find( path );

	// TODO: keep failed folders
	if ( dir_it == g_directory_entries.end() )
	{
		auto fail_it = g_failed_directories.find( path );

		if ( fail_it != g_failed_directories.end() )
			return nullptr;

		dir_tree_add_folder( path );

		dir_it = g_directory_entries.find( path );

		if ( dir_it == g_directory_entries.end() )
			return nullptr;
	}

	return &dir_it->second;
}


//directory_entry_t* dir_tree_get( size_t entry_index, fs::path& path )
//{
//	if ( entry_index > g_directory_entries.size() )
//	{
//		auto fail_it = g_failed_directories.find( path );
//
//		if ( fail_it != g_failed_directories.end() )
//			return nullptr;
//
//		size_t index = dir_tree_add_folder( path );
//
//		if ( entry_index == SIZE_MAX )
//			return nullptr;
//	}
//
//	return &g_directory_entries.at( entry_index );
//}

