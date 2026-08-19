#include "main.h"
#include "util.h"


#ifdef _WIN32
  #include <direct.h>
  #include <io.h>
#else
  #include <dirent.h>
  #include <cstring>
  #include <unistd.h>
  #include <sys/stat.h>
#endif


static std::string __empty_str{};


std::string fs_path_clean( const char* path, size_t path_len )
{
	if ( !path || path_len == 0 )
		return {};

	std::vector< std::string_view > path_segments;

#ifdef __unix__
	if ( fs_is_absolute( path, path_len ) )
	{
		path_segments.push_back( "/" );
	}
#endif

	size_t start_index = 0;
	size_t end_index   = 0;

	while ( end_index < path_len )
	{
		while ( end_index < path_len )
		{
			if ( path[ end_index ] == '/' || path[ end_index ] == '\\' )
				break;

			end_index++;
		}

		// this might occur on unix systems if the path starts with "/", an absolute path, where start_index and end_index are 0
		// or if we have a path like "C://test", with extra path separators for some reason
		if ( end_index == start_index )
		{
			start_index++;
			end_index++;
			continue;
		}

		// check if it's a "." segment and skip it
		if ( end_index - start_index == 1 && path[ start_index ] == '.' )
		{
			start_index = ++end_index;
			continue;
		}

		// check if this is a ".." segment and remove last path segment
		// if ( end_index - start_index == 2 && path[ start_index ] == '.' && path[ start_index + 1 ] == '.' )
		if ( !path_segments.empty() && ( end_index - start_index == 2 && path[ start_index ] == '.' && path[ start_index + 1 ] == '.' ) )
		{
			//if ( !path_segments.empty() )
			{
				// pop the last segment
				path_segments.pop_back();
			}
		}
		else if ( end_index - start_index > 1 )  // if it's not an empty segment
		{
			std::string_view segment( &path[ start_index ], end_index - start_index );

			// remove extra null terminators
			while ( segment.back() == '\0' )
				segment.remove_suffix( 1 );

			if ( segment.size() > 0 )
				path_segments.push_back( segment );
		}

		start_index = ++end_index;
	}

	// build the cleaned path
	std::string final_path{};

	for ( size_t i = 0; i < path_segments.size(); i++ )
	{
		final_path += path_segments[ i ];

		if ( i < path_segments.size() - 1 )
			final_path += SEP_S;
	}

	return final_path;
}


fs::path fs_path_clean( const fs::path& path )
{
	if ( path.empty() )
		return {};

	std::vector< fs::path::string_type > path_segments;

#ifdef __unix__
	if ( fs_is_absolute( path, path_len ) )
	{
		path_segments.push_back( "/" );
	}
#endif

	const auto& path_str    = path.native();
	size_t      path_len    = path_str.size();

	size_t      start_index = 0;
	size_t      end_index   = 0;

	while ( end_index < path.native().size() )
	{
		while ( end_index < path_len )
		{
			if ( path_str[ end_index ] == '/' || path_str[ end_index ] == '\\' )
				break;

			end_index++;
		}

		// this might occur on unix systems if the path starts with "/", an absolute path, where start_index and end_index are 0
		// or if we have a path like "C://test", with extra path separators for some reason
		if ( end_index == start_index )
		{
			start_index++;
			end_index++;
			continue;
		}

		// check if it's a "." segment and skip it
		if ( end_index - start_index == 1 && path_str[ start_index ] == '.' )
		{
			start_index = ++end_index;
			continue;
		}

		// check if this is a ".." segment and remove last path segment
		// if ( end_index - start_index == 2 && path[ start_index ] == '.' && path[ start_index + 1 ] == '.' )
		if ( !path_segments.empty() && ( end_index - start_index == 2 && path_str[ start_index ] == '.' && path_str[ start_index + 1 ] == '.' ) )
		{
			//if ( !path_segments.empty() )
			{
				// pop the last segment
				path_segments.pop_back();
			}
		}
		else if ( end_index - start_index > 1 )  // if it's not an empty segment
		{
			fs::path::string_type segment( &path_str[ start_index ], end_index - start_index );
			path_segments.push_back( segment );
		}

		start_index = ++end_index;
	}

	// build the cleaned path
	fs::path::string_type final_path{};

	for ( size_t i = 0; i < path_segments.size(); i++ )
	{
		final_path += path_segments[ i ];

		if ( i < path_segments.size() - 1 )
			final_path += SEP;
	}

	return final_path;
}


// replace all backslash path separators with forward slashes
char* fs_replace_path_seps_unix( const char* path )
{
	if ( !path )
		return nullptr;

	size_t path_len = strlen( path );
	char*  out      = ch_calloc< char >( path_len + 1, e_mem_category_general );

	if ( !out )
		return nullptr;

	// TODO: maybe use strchr later? not sure if that's faster
	for ( size_t i = 0; i < path_len; i++ )
	{
		if ( path[ i ] == '\\' )
			out[ i ] = '/';
		else
			out[ i ] = path[ i ];
	}

	return out;
}


void fs_get_extension( std::string_view path, std::string& output )
{
	output.clear();

	if ( path.empty() )
		return;

	const char* path_c = path.data();
	const char* dot    = strrchr( path_c, '.' );

	if ( !dot || dot == path_c )
		return;

	output.assign( dot, ( path_c + path.size() ) - dot );
}


std::string fs_get_extension( std::string_view path )
{
	std::string output;
	fs_get_extension( path, output );
	return output;
}


#if 0
const fs::path_char* fs_get_filename_ptr( fs::path_view path )
{
	if ( path.size() == 0 )
		return nullptr;

	size_t i = path.size() - 1;
	for ( ; i > 0; i-- )
	{
		if ( ( path[ i ] == '/' || path[ i ] == '\\' ) && i != path.size() - 1 )
			break;
	}

	// No File Extension Found
	if ( i == path.size() )
		return {};

	size_t start_index = i + 1;

	if ( i == 0 )
		start_index = 0;

	if ( start_index == path.size() )
		return {};

	return path.data() + start_index;
}
#endif


char* fs_get_filename( const char* path, size_t path_len )
{
	if ( !path || path_len == 0 )
		return nullptr;

	size_t i = path_len - 1;
	for ( ; i > 0; i-- )
	{
		if  ( ( path[ i ] == '/' || path[ i ] == '\\' ) && i != path_len - 1 )
			break;
	}

	// No File Extension Found
	if ( i == path_len )
		return {};

	size_t start_index = i + 1;

	if ( i == 0 )
		start_index = 0;

	if ( start_index == path_len )
		return {};

	return util_strndup( &path[ start_index ], path_len - start_index );
}


char* fs_get_filename_no_ext( const char* path, size_t path_len )
{
	if ( !path || path_len == 0 )
		return nullptr;

	char* name = fs_get_filename( path, path_len );

	if ( !name )
		return nullptr;

	char* dot = strrchr( name, '.' );

	if ( !dot || dot == name )
		return name;

	char* output = util_strndup( name, dot - name );
	ch_free_str( name );
	return output;
}


char* fs_get_filename( const char* path )
{
	if ( !path )
		return nullptr;

	return fs_get_filename( path, strlen( path ) );
}


char* fs_get_filename_no_ext( const char* path )
{
	if ( !path )
		return nullptr;

	return fs_get_filename_no_ext( path, strlen( path ) );
}


bool fs_is_absolute( const char* path, size_t path_len )
{
#ifdef _WIN32
	// NOTE: this doesn't work for paths like C:test.txt,
	// as that is relative to the current directory on that drive, weird windows stuff
	// https://devblogs.microsoft.com/oldnewthing/20101011-00/?p=12563
	if ( path_len > 2 )
		return ( path[ 1 ] == ':' );

	return false;
	// return !PathIsRelativeA( spPath );
#elif __unix__
	if ( path_len == 0 )
		return false;
	return path[ 0 ] == '/';
#else
	return fs::path( path ).is_absolute();
#endif
}


bool fs_is_relative( const char* path, size_t path_len )
{
	return !fs_is_absolute( path, path_len );
}


bool fs_make_dir_check( const fs::path_char* path )
{
	if ( fs_exists( path ) )
	{
		if ( fs_is_file( path ) )
		{
			path_printf( "Error: Directory already exists as a file: \"%s\"\n", path );
			return false;
		}
	}
	else if ( !fs_make_dir( path ) )
	{
		path_printf( "Error: Failed to create directory: \"%s\"\n", path );
		return false;
	}

	return true;
}


u64 fs_file_size( const char* path )
{
	struct stat s;

	if ( stat( path, &s ) == 0 )
		return s.st_size;

	return 0;
}


// returns the file length in the len argument
char* fs_read_file( const fs::path_char* path, size_t* len )
{
	if ( !path )
		return nullptr;

	// TODO: use the windows api instead of posix version, think it gives you better permission handling? maybe faster?
	FILE* fp = path_open( path, "rb" );

	if ( !fp )
	{
		return nullptr;
	}

	fseek( fp, 0, SEEK_END );
	long size = ftell( fp );
	fseek( fp, 0, SEEK_SET );

	char* output = static_cast< char* >( malloc( ( size + 1 ) * sizeof( char ) ) );

	if ( !output )
	{
		return nullptr;
	}

	mem_add_item( e_mem_category_file_data, output, ( size + 1 ) * sizeof( char ), 1 );

	memset( output, 0, ( size + 1 ) * sizeof( char ) );
	fread( output, size, 1, fp );
	fclose( fp );

	output[ size ] = 0;

	if ( len )
		*len = size;

	return output;
}


char* fs_read_file_app_dir( const fs::path_char* path, size_t* len )
{
	if ( !path )
		return nullptr;

	size_t path_len = fs_path_len( path );

	if ( path_len == 0 )
		return nullptr;

	fs::path_str path_full = sys_get_exe_folder_native_str();
	path_full += SEP;
	path_full += path;

	return fs_read_file( path_full.c_str(), len );
}


static bool handle_rename( const fs::path_char* path, const fs::path_char* new_path )
{
#if WIN32
	int code = _wrename( path, new_path );
#else
	int code = rename( path, new_path );
#endif

	if ( code == 0 )
		return true;

	path_printf( "failed to rename old saved file \"%s\" - \"%s\"\n", path, new_path );

	switch ( code )
	{
		default:
			printf( "Unknown Error Code: %d\n", code );
			break;
		case EACCES:
			printf( "Permission denied\n" );
			break;
		case ENOENT:
			printf( "Source file does not exist\n" );
			break;
		case EEXIST:
			printf( "A file with the new filename already exists\n" );
			break;
		case EINVAL:
			printf( "The names specified are invalid\n" );
			break;
	}

	return false;
}


// TODO: look into atomic file operations?
// TODO: THIS CURRENTLY IGNORES THE READ ONLY ATTRIBUTE, FIX THAT !!!!!!
bool fs_save_file( const fs::path_char* path, const char* data, size_t size )
{
	// write to a temp file,
	// then rename to old saved file to name.bak,
	// then remove .temp from new file, and remove .bak file (or keep it until next save)
	// also check if a .temp file already exists just in case if a crash happened midway through this
	// basically this is all so if there is a crashe at any point during this, we dont lose any data

	// TODO: new idea - make a copy of the file on disk, then try to overwrite it
	// this should respect read only, and other attributes

	fs::path_char temp_path[ 2048 ] = { 0 };
	pathcat( temp_path, path );
	pathcat_const( temp_path, ".temp" );

	fs::path_char bak_path[ 2048 ] = { 0 };
	pathcat( bak_path, path );
	pathcat_const( bak_path, ".bak" );

	// check if a .temp file exists already
	if ( path_access( temp_path, 0 ) != -1 )
	{
		if ( !sys_recycle_file( temp_path ) )
		{
			path_printf( "failed to delete old temp file for saving! - \"%s\"\n", temp_path );
			return false;
		}
	}

	FILE* fp = path_open( temp_path, "wb" );

	if ( fp == nullptr )
	{
		path_printf( "failed to open file handle to save file to\n - \"%s\"\n", temp_path );
		return false;
	}

	size_t amount_wrote = fwrite( data, size, 1, fp );

	fclose( fp );

	// check if a saved file exists already
	bool old_save_exists = path_access( path, 0 ) != -1;

	if ( old_save_exists )
	{
		// check if a .bak file exists already
		if ( path_access( bak_path, 0 ) != -1 )
		{
			if ( !sys_recycle_file( bak_path ) )
			{
				path_printf( "failed to delete old backup file for saving! - \"%s\"\n", bak_path );
				return false;
			}
		}

		if ( !handle_rename( path, bak_path ) )
			return false;
	}

	if ( !handle_rename( temp_path, path ) )
		return false;

	// copy file creation date
	u64 create_date = 0;

	if ( old_save_exists && sys_get_file_times_and_size( bak_path, &create_date, nullptr, nullptr, nullptr ) )
	{
		sys_set_file_times( path, &create_date, nullptr, nullptr );
	}

	return true;
}


#if 0
void fs_save_file_free( save_file_t& save )
{
	ch_free_str( save.temp_path );
	ch_free_str( save.bak_path );
}


// stupid
save_file_t fs_save_file_open( const char* path )
{
	// write to a temp file,
	// then rename to old saved file to name.bak,
	// then remove .temp from new file, and remove .bak file (or keep it until next save)
	// also check if a .temp file already exists just in case if a crash happened midway through this
	// basically this is all so if there is a crashe at any point during this, we dont lose any data

	save_file_t save{};
	size_t      path_len = strlen( path );
	FILE*       fp       = nullptr;

	save.temp_path       = ch_calloc< char >( path_len + 6, e_mem_category_string );
	save.bak_path        = ch_calloc< char >( path_len + 5, e_mem_category_string );

	strcat( save.temp_path, path );
	strcat( save.temp_path, ".temp" );

	strcat( save.bak_path, path );
	strcat( save.bak_path, ".bak" );

	// check if a .temp file exists already
	if ( access( save.temp_path, 0 ) != -1 )
	{
		if ( !sys_recycle_file( save.temp_path ) )
		{
			printf( "failed to delete old temp file for saving! - \"%s\"\n", save.temp_path );
			goto save_file_open_fail;
		}
	}

#if WIN32
	FILE* fp = _wfopen( path.data(), L"wb" );
#else
	FILE* fp = fopen( path.data(), "wb" );
#endif
	fp = fopen( save.temp_path, "wb" );

	if ( fp == nullptr )
	{
		printf( "failed to open file handle to save file to\n - \"%s\"\n", save.temp_path );
		goto save_file_open_fail;
	}

	save.file = fp;
	return save;

save_file_open_fail:
	fs_save_file_free( save );
	return {};
}


void fs_save_file_close( save_file_t& save, const char* path )
{
	FILE* fp = (FILE*)save.file;
	fclose( fp );

	// check if a saved file exists already
	bool old_save_exists = access( path, 0 ) != -1;

	if ( old_save_exists )
	{
		// check if a .bak file exists already
		if ( access( save.bak_path, 0 ) != -1 )
		{
			if ( !sys_recycle_file( save.bak_path ) )
			{
				printf( "failed to delete old backup file for saving! - \"%s\"\n", save.bak_path );
				return;
			}
		}

		if ( !handle_rename( path, save.bak_path ) )
		{
			fs_save_file_free( save );
			return;
		}
	}

	if ( !handle_rename( save.temp_path, path ) )
	{
		fs_save_file_free( save );
		return;
	}

	// copy file creation date
	u64 create_date = 0;

	if ( old_save_exists && sys_get_file_times_and_size( save.bak_path, &create_date, nullptr, nullptr, nullptr ) )
	{
		sys_set_file_times( path, &create_date, nullptr, nullptr );
	}
}
#endif


bool fs_write_file( const fs::path_char* path, const char* data, size_t size )
{
	FILE* fp = path_open( path, "wb" );

	if ( fp == nullptr )
	{
		path_printf( "failed to open file handle to write file to\n - \"%s\"\n", path );
		return false;
	}

	size_t amount_wrote = fwrite( data, size, 1, fp );

	fclose( fp );

	return true;
}

