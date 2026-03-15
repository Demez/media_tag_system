#include "main.h"
#include "util.h"


#ifdef _WIN32
  #include <direct.h>
  #include <io.h>

  // get rid of the dumb windows posix depreciation warnings
  #define mkdir  _mkdir
  #define chdir  _chdir
  #define access _access
  #define getcwd _getcwd

  #define stat   _stat
#else
  #include <dirent.h>
  #include <cstring>
  #include <unistd.h>
  #include <sys/stat.h>

// windows-specific mkdir() is used
  // #define mkdir( f ) mkdir( f, 666 )
  // owner read, write, exec
  // group read, exec
  // others read, exec
  #define mkdir( f ) mkdir( f, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH )
#endif


std::string fs_path_clean( const char* path, size_t path_len )
{
	if ( !path || path_len == 0 )
		return {};

	std::vector< std::string > path_segments;

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
			std::string segment( &path[ start_index ], end_index - start_index );
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


char* fs_get_filename( const char* path, size_t path_len )
{
	if ( !path || path_len == 0 )
		return nullptr;

	size_t i = path_len - 1;
	for ( ; i > 0; i-- )
	{
		if ( path[ i ] == '/' || path[ i ] == '\\' )
			break;
	}

	// No File Extension Found
	if ( i == path_len )
		return {};

	size_t start_index = i + 1;

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


bool fs_exists( const char* path )
{
	return access( path, 0 ) != -1;
}


bool fs_make_dir( const char* path )
{
	return mkdir( path ) == 0;
}


bool fs_is_dir( const char* path )
{
	struct stat s;

	if ( stat( path, &s ) == 0 )
		return ( s.st_mode & S_IFDIR );

	return false;
}


bool fs_is_file( const char* path )
{
	struct stat s;

	if ( stat( path, &s ) == 0 )
		return ( s.st_mode & S_IFREG );

	return false;
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


bool fs_make_dir_check( const char* path )
{
	if ( fs_exists( path ) )
	{
		if ( fs_is_file( path ) )
		{
			printf( "Error: Directory already exists as a file: \"%s\"\n", path );
			return false;
		}
	}
	else if ( !fs_make_dir( path ) )
	{
		printf( "Error: Failed to create directory: \"%s\"\n", path );
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
char* fs_read_file( const char* path, size_t* len )
{
	FILE* fp = fopen( path, "rb" );

	if ( !fp )
	{
		return nullptr;
	}

	fseek( fp, 0, SEEK_END );
	long size = ftell( fp );
	fseek( fp, 0, SEEK_SET );

	char* output = (char*)malloc( ( size + 1 ) * sizeof( char ) );

	if ( !output )
	{
		return nullptr;
	}

	mem_add_item( e_mem_category_file_data, output, ( size + 1 ) * sizeof( char ) );

	memset( output, 0, ( size + 1 ) * sizeof( char ) );
	fread( output, size, 1, fp );
	fclose( fp );

	output[ size ] = 0;

	if ( len )
		*len = size;

	return output;
}


static bool handle_rename( const char* path, const char* new_path )
{
	int code = rename( path, new_path );

	if ( code == 0 )
		return true;

	printf( "failed to rename old saved file \"%s\" - ", path );

	switch ( code )
	{
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


// TODO: THIS CURRENTLY IGNORES THE READ ONLY ATTRIBUTE, FIX THAT !!!!!!
bool fs_save_file( const char* path, const char* data, size_t size )
{
	// write to a temp file,
	// then rename to old saved file to name.bak,
	// then remove .temp from new file, and remove .bak file (or keep it until next save)
	// also check if a .temp file already exists just in case if a crash happened midway through this
	// basically this is all so if there is a crashe at any point during this, we dont lose any data

	char temp_path[ 4096 ] = { 0 };
	strcat( temp_path, path );
	strcat( temp_path, ".temp" );

	char bak_path[ 4096 ] = { 0 };
	strcat( bak_path, path );
	strcat( bak_path, ".bak" );

	// check if a .temp file exists already
	if ( access( temp_path, 0 ) != -1 )
	{
		if ( !sys_recycle_file( temp_path ) )
		{
			printf( "failed to delete old temp file for saving! - \"%s\"\n", temp_path );
			return false;
		}
	}

	FILE* fp = fopen( temp_path, "wb" );

	if ( fp == nullptr )
	{
		printf( "failed to open file handle to write file to\n - \"%s\"", temp_path );
		return false;
	}

	size_t amount_wrote = fwrite( data, size, 1, fp );

	fclose( fp );

	// check if a saved file exists already
	bool old_save_exists = access( path, 0 ) != -1;

	if ( old_save_exists )
	{
		// check if a .bak file exists already
		if ( access( bak_path, 0 ) != -1 )
		{
			if ( !sys_recycle_file( bak_path ) )
			{
				printf( "failed to delete old backup file for saving! - \"%s\"\n", bak_path );
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


bool fs_write_file( const char* path, const char* data, size_t size )
{
	FILE* fp = fopen( path, "wb" );

	if ( fp == nullptr )
	{
		printf( "failed to open file handle to write file to\n - \"%s\"", path );
		return false;
	}

	size_t amount_wrote = fwrite( data, size, 1, fp );

	fclose( fp );

	return true;
}
