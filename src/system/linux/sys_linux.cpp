#include "util.h"
#include "system/system.h"

#include <time.h>
#include <stdint.h>
#include <dlfcn.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>

#include <SDL3/SDL_video.h>


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


bool sys_get_file_times( const char* path, u64* creation, u64* access, u64* write )
{
	struct stat s{};
	if ( stat( path, &s ) != 0 )
		return false;

	if ( creation )
		*creation = s.st_ctime;

	if ( write )
		*write = s.st_mtime;

	if ( access )
		*access = s.st_atime;

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


// https://stackoverflow.com/a/35658917
bool sys_execute_read( const char* command, str_buf_t& output )
{
	return false;
}


// https://stackoverflow.com/a/35658917
bool sys_execute_read_callback( const char* command, str_buf_t& output, f_exec_callback* p_exec_callback )
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
	return font_data;
}


proc_mem_info_t sys_get_mem_info()
{
	proc_mem_info_t mem_info{};
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

