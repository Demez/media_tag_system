#include "main.h"
#include "system/system.h"
#include "util.h"

#include <SDL3/SDL_video.h>
#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <stdint.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <ftw.h>


// ----------------------------------------------------------------------------------------


bool sys_init()
{
	return true;
}


void sys_shutdown()
{
}


void sys_update()
{
}


void sys_set_window( SDL_Window* window )
{
}


// ----------------------------------------------------------------------------------------
// Library Loading


module_t sys_load_library( const char* path )
{
	return (module_t)dlopen( path, RTLD_NOW | RTLD_GLOBAL );
}


void sys_close_library( module_t mod )
{
	dlclose( mod );
}


void* sys_load_func( module_t mod, const char* name )
{
	return dlsym( mod, name );
}


// ----------------------------------------------------------------------------------------
// System Errors


char* sys_get_error_internal()
{
	static char output[ 256 ];
	memset( output, 0,256 ) ;
	snprintf( output, 256, "Error: %d - %s", errno, strerror( errno ) );

	return output;
}


char* sys_get_error()
{
	return util_strdup( sys_get_error_internal() );
}


void sys_print_last_error()
{
	fprintf( stderr, "Error: %s\n", sys_get_error_internal() );
}


// --------------------------------------------------------------------------------------------------------
// Filesystem


char* sys_get_exe_folder( size_t* len )
{
	char output[ 4096 ];
	size_t ret = readlink( "/proc/self/exe", output, 4096 );

	if ( ret <= 0 )
	{
		printf("Failed to get exe folder!\n" );
		sys_print_last_error();
		return util_strdup( "" );
	}

	// find index of last path separator
	char*  sep    = strrchr( output, '/' );
	size_t path_i = sep - output;

	if ( len )
		*len = path_i;

	return util_strndup( output, path_i );
}


char* sys_get_exe_path( size_t* len )
{
	char output[ 4096 ];
	size_t ret = readlink( "/proc/self/exe", output, 4096 );

	if ( ret <= 0 )
	{
		printf("Failed to get exe folder!\n" );
		sys_print_last_error();
		return util_strdup( "" );
	}

	if ( len )
		*len = strlen( output );

	return util_strdup( output );
}


char* sys_get_cwd()
{
	return getcwd( nullptr, 0 );
}

bool sys_get_file_times_and_size( const char* path, u64* creation, u64* access, u64* write, u64* size )
{
	if ( !path )
		return false;

	struct stat s{};
	if ( stat( path, &s ) != 0 )
		return false;

	if ( creation )
		*creation = s.st_ctime;

	if ( write )
		*write = s.st_mtime;

	if ( access )
		*access = s.st_atime;

	if ( size )
		*size = s.st_size;

	return true;
}


bool sys_set_file_times( const char* path, u64* creation, u64* access, u64* write )
{
	return false;
}


std::vector< fs::path > sys_get_drives()
{
	std::vector< fs::path > drives{};
	return drives;
}


struct nftw_data_t
{
	const char*            root      = nullptr;
	e_scandir_flags        flags     = e_scandir_none;
	std::vector< file_t >* files_ptr = nullptr;
	bool found_root                  = false;
};


static nftw_data_t nftw_data{};


int ftw_callback( const char* filename, const struct stat64* status, int __flag, struct FTW* ftwbuf )
{
	// hack lol
	if ( !nftw_data.found_root )
	{
		if ( strcmp( filename, nftw_data.root ) == 0 )
		{
			nftw_data.found_root = true;
			return FTW_CONTINUE;
		}
	}

	if ( nftw_data.files_ptr )
	{
		file_t file{};
		file.path = filename;

		if ( status->st_mode & S_IFREG )
		{
			file.type |= e_file_type_file;
			file.size = status->st_size;
		}
		else if ( status->st_mode & S_IFDIR )
		{
			file.type |= e_file_type_directory;
			file.size = 0;
		}

		file.date_created = 0;
		file.date_mod     = status->st_mtime;

		nftw_data.files_ptr->push_back( file );
	}

	//if ( ftwbuf->level > 1 )
	//	return FTW_SKIP_SIBLINGS;

	if ( ftwbuf->level > 0 && status->st_mode & S_IFDIR )
		return FTW_SKIP_SUBTREE;

	return FTW_CONTINUE;
}


// TODO: look at getdents64()?
bool sys_scandir( const char* root, const char* path, std::vector< file_t >& files, e_scandir_flags flags )
{
	std::string scan_dir = root;

	if ( path )
	{
		scan_dir += SEP_S;
		scan_dir += path;
	}

	scan_dir += SEP_S;

	// Failed experiment lol
#if 0

	nftw_data.root       = root;
	nftw_data.files_ptr  = &files;
	nftw_data.flags      = flags;
	nftw_data.found_root = false;

	if ( nftw64( scan_dir.c_str(), ftw_callback, 100, FTW_ACTIONRETVAL | FTW_PHYS ) != 0)
	{
		perror("ftw");
	}

	nftw_data.root       = nullptr;
	nftw_data.files_ptr  = nullptr;
	nftw_data.flags      = e_scandir_none;
	nftw_data.found_root = false;

#else
	DIR* dir = opendir( scan_dir.c_str() );

	if ( !dir )
	{
		printf( "Failed to open directory: \"%s\"\n", scan_dir.c_str() );
		return false;
	}

	dirent64* ent;
	while ( ( ent = readdir64( dir ) ) != nullptr )
	{
		if ( ent->d_type == DT_DIR )
		{
			if ( strcmp( ent->d_name, "." ) == 0 || strcmp( ent->d_name, ".." ) == 0 )
				continue;
		}

		std::string relative_path;

		if ( path )
		{
			relative_path += path;
			relative_path += SEP_S;
		}

		relative_path += ent->d_name;

		if ( ent->d_type == DT_DIR )
		{
			if ( flags & e_scandir_recursive )
				sys_scandir( root, relative_path.data(), files, flags );

			if ( flags & e_scandir_no_dirs )
				continue;
		}

		if ( ( ent->d_type == DT_REG ) && flags & e_scandir_no_files )
		{
			continue;
		}

		file_t file{};

		std::string abs_path = scan_dir + ent->d_name;

		if ( flags & e_scandir_abs_paths )
			file.path = abs_path;
		else
			file.path = relative_path;

		// TODO: handle system links

		if ( ent->d_type == DT_REG )
		{
			file.type |= e_file_type_file;
			file.size = ent->d_off;
		}
		else
		{
			file.type |= e_file_type_directory;
			file.size = 0;
		}

		struct stat64 s{};
		if ( stat64( abs_path.c_str(), &s ) == 0 )
		{
			// file.size = s.st_size;
			file.date_created = 0;
			file.date_mod     = s.st_mtime;
		}

		files.push_back( file );
	}

	closedir( dir );
#endif
	return true;
}


// --------------------------------------------------------------------------------------------------------
// Shell Functions


bool sys_recycle_file( const char* path )
{
	// TODO: move file to ~/.local/share/trash

	// Check if we have gio
	int gio_check = sys_execute( "gio" );

	if ( gio_check == 0 )
	{
		char buffer[ 1024 ]{};
		snprintf( buffer, 1024, "gio trash %s", path );
		sys_execute( buffer );
		return true;
	}

	return false;
}


void sys_open_file_properties( const char* file )
{
	// not possible to implement on linux
	// dolphin doesn't expose a way to open file properties
}


// --------------------------------------------------------------------------------------------------------


const void* sdl_clipboard_callback( void *userdata, const char *mime_type, size_t *size )
{
	char* data = (char*)userdata;
	*size      = strlen( data );
	return SDL_strdup( data );
}


bool sys_copy_to_clipboard( const char* path )
{
	if ( !path )
		return false;

	const char* mime_type = "text/uri-list";

	static char buffer[ 1024 ]{};
	memset( buffer, 0, 1024 );
	snprintf( buffer, 1024, "file://%s", path );

	if ( SDL_SetClipboardData( sdl_clipboard_callback, nullptr, buffer, &mime_type, 1 ) )
		return true;

	return false;
}


void sys_browse_to_file( const char* path )
{
	if ( !path )
		return;

	// Check if we have xdg-open
	int xdg_check = sys_execute( "xdg-open" );

	if ( xdg_check == 0 )
	{
		char buffer[ 1024 ]{};
		snprintf( buffer, 1024, "xdg-open %s", path );
		sys_execute( buffer );
		return;
	}

	// Check if we have gio
	int gio_check = sys_execute( "gio" );

	if ( gio_check == 0 )
	{
		char buffer[ 1024 ]{};
		snprintf( buffer, 1024, "gio open %s", path );
		sys_execute( buffer );
		return;
	}

	printf("Failed to copy to clipboard, could not find terminal tools xdg-open or gio!\n" );
}


// --------------------------------------------------------------------------------------------------------
// Terminal


// https://stackoverflow.com/a/646254
bool sys_execute_read( const char* command, std::string& output )
{
	FILE* fp = popen( command, "r" );

	if ( fp == nullptr )
	{
		printf("Failed to run command: %s\n", command );
		return false;
	}

	char buffer[ 4096 ]{};
	while (fgets( buffer, sizeof( buffer ), fp ) != nullptr )
	{
		output.append( buffer );
	}

	pclose( fp );
	return true;
}


bool sys_execute_read_callback( const char* command, std::string& output, f_exec_callback* p_exec_callback )
{
	if ( !p_exec_callback )
		return false;

	return false;
}


int sys_execute( const char* command )
{
	return std::system( command );
}


// --------------------------------------------------------------------------------------------------------
// Other


sys_font_data_t sys_get_font()
{
	sys_font_data_t font_data{};

	// TODO: use fontconfig.h instead
	// Check if we have fc-match
	int ret = sys_execute( "fc-match" );

	if ( ret != 0 )
	{
		printf("Failed to find fc-match for default font!\n" );
		return font_data;
	}

	std::string output;
	sys_execute_read( "fc-match -b", output );

	const char* file      = strstr( output.c_str(), "file: " );
	//const char* pixelsize = strstr( output.c_str(), "pixelsize: " );

	if ( file )
	{
		const char* file_end = strchr( file, '\n' );
		std::string file_string( file + 7, ( ( file_end - 4 ) - ( file + 7 ) ) );
		font_data.font_path = util_strndup( file_string.c_str(), file_string.size() );
	}

	//if ( pixelsize )
	//{
	//	const char* line_end = strchr( pixelsize, '\n' );
	//	std::string pixelsize_str( pixelsize + 11, ( ( line_end - 6 ) - ( pixelsize + 11 ) ) );
	//	// font_data.font_path = util_strndup( pixelsize.c_str(), pixelsize.size() );
	//	font_data.height = atof( pixelsize_str.c_str() );
	//}
	//else
	{
		font_data.height = 17;
	}

	return font_data;
}


proc_mem_info_t sys_get_mem_info()
{
	proc_mem_info_t mem_info{};

	rusage usage{};
	int ret = getrusage( RUSAGE_SELF, &usage );

	if ( ret == 0 )
	{
		mem_info.working_set = usage.ru_maxrss * (long)MEM_SCALE;
	}
	else
	{
		printf("Failed to get memory usage - %d!\n", ret );
		mem_info.working_set = 0;
	}

	mem_info.page_file = 0;

	return mem_info;
}


u64 sys_get_time_ms()
{
	struct timespec ts{};
	clock_gettime( CLOCK_MONOTONIC, &ts );
	return (uint64_t)( ts.tv_sec * 1000ULL + ts.tv_nsec / 1000000ULL );
}


// Start drag and drop of multiple files in the system shell, like dragging to another folder to copy, into discord, etc.
void sys_begin_drag_drop( const std::vector< fs::path >& files )
{
}


// files have been dragged into this program, the drag and drop system will call this function when it recieves it
void sys_set_receive_drag_drop_func( f_drag_drop_receive* callback )
{
}

