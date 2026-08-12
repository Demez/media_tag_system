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
	e_scandir_no_filenames  = 1 << 4,  // Don't fill out the name value of file_t
	e_scandir_no_paths      = 1 << 5,  // Don't fill out the path value of file_t

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

enum e_sys_init : u8
{
	e_sys_init_fail,
	e_sys_init_success,
	e_sys_init_single_instance,
};

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


struct scandir_status_t
{
	//size_t search_count = 0;      // scandir updates this per file
	bool   cancel       = false;  // set this to true to cancel the search
};


struct file_t
{
	fs::path    path{};
	std::string name{};
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

		if ( name != other.name )
			return true;

		return false;
	}

	bool operator==( const file_t& other ) const
	{
		return !operator!=( other );
	}

	file_t()
	{
	};

	~file_t()
	{
	};

	// copying
	void assign( const file_t& other ) noexcept
	{
		path         = other.path;
		name         = other.name;
		date_mod     = other.date_mod;
		date_created = other.date_created;
		size         = other.size;
		type         = other.type;
	}

	// moving
	void assign( file_t&& other ) noexcept
	{
		path         = std::move( other.path );
		name         = std::move( other.name );
		date_mod     = other.date_mod;
		date_created = other.date_created;
		size         = other.size;
		type         = other.type;
	}

	file_t& operator=( const file_t& other ) noexcept
	{
		assign( other );
		return *this;
	}

	file_t& operator=( file_t&& other ) noexcept
	{
		assign( std::move( other ) );
		return *this;
	}

	// copying
	file_t( const file_t& other ) noexcept
	{
		assign( other );
	}

	// moving
	file_t( file_t&& other ) noexcept
	{
		assign( std::move( other ) );
	}
};


using f_exec_callback = void( char* buf, size_t len );
using f_drag_drop_receive = bool( const std::vector< fs::path >& files );

// --------------------------------------------------------------------------------------------------------

// call this before sys_init
bool                    sys_setup_exe_path_vars();
void                    sys_free_exe_path_vars();

e_sys_init              sys_init( int argc, char* argv[] );
void                    sys_shutdown();
void                    sys_update();

bool                    sys_set_window( SDL_Window* window );
void                    sys_do_window_drag( ImVec2 last_mouse_pos, ImVec2 new_mouse_pos );

// system error, make sure to free this string!
char*                   sys_get_error();
void                    sys_print_last_error();

// --------------------------------------------------------------------------------------------------------
// Filesystem

// get folder exe is stored in
// pass in a ref to a size_t to get the length of the folder
const char*             sys_get_exe_folder( size_t* len = nullptr );
fs::path                sys_get_exe_folder_fspath();
fs::path::string_type   sys_get_exe_folder_native_str();

// get the full path of the exe
// pass in a ref to a size_t to get the length of the path
const char*             sys_get_exe_path( size_t* len = nullptr );

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
void                    sys_open_file_properties( const std::vector< fs::path >& files );

bool                    sys_copy_to_clipboard( const std::vector< fs::path >& files );

void                    sys_browse_to_file( const char* path );

// simpiler version of sys_browse_to_files, one file or folder
void                    sys_browse_to_path( const fs::path& path );

void                    sys_browse_to_files( const fs::path& root, const std::vector< fs::path > paths );

// print color with \aFFF escape codes for color values
//void        sys_print_color( const char* string );

// Search a directory
// pass in a pointer to a boolean if you want to be able to cancel the search at any point in time
bool                    sys_scandir( const char* root, std::vector< file_t >& files, e_scandir_flags flags, bool* cancel = nullptr );

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
void                    sys_do_drag_drop_files( const std::vector< fs::path >& files, u32 sdl_mouse_btn );

// files have been dragged into this program, the drag and drop system will call this function when it recieves it
void                    sys_set_receive_drag_drop_func( f_drag_drop_receive* callback );

// --------------------------------------------------------------------------------------------------------
// Folder Monitor

// Return true if something in the folder changed, indicating we need a refresh
bool                    sys_folder_mon_changed();
void                    sys_folder_mon_shutdown();

// --------------------------------------------------------------------------------------------------------
// Other

proc_mem_info_t         sys_get_mem_info();

// get the default font to use for imgui
// FREE THIS AFTER USE
sys_font_data_t         sys_get_font();

u64                     sys_get_time_ms();

// non-exception based path conversion
std::string             sys_path_to_string( const fs::path& path );
void                    sys_path_to_string( fs::path&& path, std::string& output );
void                    sys_path_to_string( const fs::path_char* path, std::string& output );

fs::path                sys_string_to_path( const std::string& path_str );
