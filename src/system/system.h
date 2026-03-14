#pragma once

#include "util.h"

#include "SDL3/SDL.h"

// --------------------------------------------------------------------------------------------------------
// System Interface


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


using f_exec_callback = void( char* buf, size_t len );
using f_drag_drop_receive = bool( const std::vector< fs::path >& files );

// --------------------------------------------------------------------------------------------------------

bool                    sys_init();
void                    sys_shutdown();
void                    sys_update();

void                    sys_set_window( SDL_Window* window );

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
bool                    sys_get_file_times( const char* path, u64* creation, u64* access, u64* write );
bool                    sys_set_file_times( const char* path, u64* creation, u64* access, u64* write );

// Get list of drives mounted on this device
// Windows returns drive letters
std::vector< fs::path > sys_get_drives();

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

// --------------------------------------------------------------------------------------------------------
// Terminal

// execute a command and read it's output
bool                    sys_execute_read( const char* command, str_buf_t& output );

// execute a command and read it's output, with a callback function everytime more output is read from the file
bool                    sys_execute_read_callback( const char* command, str_buf_t& output, f_exec_callback* p_exec_callback );

// execute a command and return the commands return value
int                     sys_execute( const char* command );

// --------------------------------------------------------------------------------------------------------
// Drag and Drop Interface

// Start drag and drop of multiple files in the system shell, like dragging to another folder to copy, into discord, etc.
void                    sys_begin_drag_drop( const std::vector< fs::path >& files );

// files have been dragged into this program, the drag and drop system will call this function when it recieves it
void                    sys_set_receive_drag_drop_func( f_drag_drop_receive* callback );

// --------------------------------------------------------------------------------------------------------
// Other

proc_mem_info_t         sys_get_mem_info();

// get the default font to use for imgui
// FREE THIS AFTER USE
sys_font_data_t         sys_get_font();

u64                     sys_get_time_ms();
