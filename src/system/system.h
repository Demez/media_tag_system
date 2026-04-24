#pragma once

#include "util.h"

#include "SDL3/SDL.h"

// --------------------------------------------------------------------------------------------------------
// System Interface


enum e_scandir_flags_ : u8
{
	e_scandir_none          = 0,
	e_scandir_abs_paths     = 1 << 0,  // All paths must be absolute
	e_scandir_no_dirs       = 1 << 1,  // Don't include any directories
	e_scandir_no_files      = 1 << 2,  // Don't include any files
	e_scandir_recursive     = 1 << 3,  // Recursively scan a directory, works with e_scandir_no_dirs flag

	e_scandir_abs_recursive = e_scandir_abs_paths | e_scandir_recursive,
};

using e_scandir_flags = u8;


enum e_file_type_ : u8
{
	e_file_type_invalid     = 0,
	e_file_type_file        = 1 << 0,
	e_file_type_directory   = 1 << 1,
	e_file_type_system_link = 1 << 2,
};

using e_file_type = u8;

using module_t = void*;


struct sys_font_data_t
{
	char* font_path;
	float weight;
	float height;
};


// Process Memory Info
struct proc_mem_info_t
{
	size_t working_set;
	size_t page_file;
};


struct file_t
{
	fs::path    path{};
	u64         size         = 0;
	u64         date_mod     = 0;
	u64         date_created = 0;
	e_file_type type         = e_file_type_invalid;

	bool operator!=( const file_t& other ) const
	{
		if ( size != other.size )
			return true;

		if ( date_mod != other.date_mod )
			return true;

		if ( date_created != other.date_created )
			return true;

		if ( type != other.type )
			return true;

		if ( path != other.path )
			return true;

		return false;
	}

	bool operator==( const file_t& other ) const
	{
		return !operator!=( other );
	}
};


using f_exec_callback = void( char* buf, size_t len );
using f_drag_drop_receive = bool( const std::vector< fs::path >& files );

// --------------------------------------------------------------------------------------------------------

bool                    sys_init();
void                    sys_shutdown();
void                    sys_update();

void                    sys_set_window( SDL_Window* window );
void                    sys_do_window_drag( ImVec2 last_mouse_pos, ImVec2 new_mouse_pos );

// library loading
#ifdef _WIN32
module_t                sys_load_library( const wchar_t* path );
#else
module_t                sys_load_library( const char* path );
#endif

void                    sys_close_library( module_t mod );
void*                   sys_load_func( module_t mod, const char* path );

// system error, make sure to free this string!
char*                   sys_get_error();
void                    sys_print_last_error();

// --------------------------------------------------------------------------------------------------------
// Filesystem

// get folder exe is stored in
// pass in a ref to a size_t to get the length of the folder
// FREE THIS AFTER USE
char*                   sys_get_exe_folder( size_t* len = nullptr );

// get the full path of the exe
// pass in a ref to a size_t to get the length of the path
// FREE THIS AFTER USE
char*                   sys_get_exe_path( size_t* len = nullptr );

// get current working directory
char*                   sys_get_cwd();

// File Times - In Unix Time
bool                    sys_get_file_times_and_size( const char* path, u64* creation, u64* access, u64* write, u64* size );
bool                    sys_set_file_times( const char* path, u64* creation, u64* access, u64* write );

// Get list of drives mounted on this device
// Windows returns drive letters
bool                    sys_get_drives( std::vector< std::string >& drives );

// --------------------------------------------------------------------------------------------------------
// Shell Functions

// on windows, this sends the file to the recycle bin
// it does the equivalent on other platforms
bool                    sys_recycle_file( const char* path );

// on windows, this opens the file properties dialog
void                    sys_open_file_properties( const char* path );

bool                    sys_copy_to_clipboard( const char* path );

// NOTE: path cannot be over MAX_PATH (260 characters), thanks windows shell
void                    sys_browse_to_file( const char* path );

// print color with \aFFF escape codes for color values
//void        sys_print_color( const char* string );

bool                    sys_scandir( const char* root, const char* path, std::vector< file_t >& files, e_scandir_flags flags );

// --------------------------------------------------------------------------------------------------------
// Terminal

// execute a command and read it's output
bool                    sys_execute_read( const char* command, std::string& output );

// execute a command and read it's output, with a callback function everytime more output is read from the file
bool                    sys_execute_read_callback( const char* command, std::string& output, f_exec_callback* p_exec_callback );

// execute a command and return the commands return value
int                     sys_execute( const char* command );

// --------------------------------------------------------------------------------------------------------
// Drag and Drop Interface

// Start drag and drop of multiple files in the system shell, like dragging to another folder to copy, into discord, etc.
void                    sys_do_drag_drop_files( const std::vector< fs::path >& files );

// files have been dragged into this program, the drag and drop system will call this function when it recieves it
void                    sys_set_receive_drag_drop_func( f_drag_drop_receive* callback );

// --------------------------------------------------------------------------------------------------------
// Other

proc_mem_info_t         sys_get_mem_info();

// get the default font to use for imgui
// FREE THIS AFTER USE
sys_font_data_t         sys_get_font();

u64                     sys_get_time_ms();
