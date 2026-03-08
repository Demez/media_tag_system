#include "util.h"


// --------------------------------------------------------------------------------------------------------
// system functions


using module_t = void*;


struct sys_font_data_t
{
	char* font_path;
	float weight;
	float height;
};


struct proc_mem_info_t
{
	size_t working_set;
	size_t page_file;
};


using f_exec_callback = void( char* buf, size_t len );


// --------------------------------------------------------------------------------------------------------


int                     sys_init();
void                    sys_shutdown();
void                    sys_update();

// library loading
module_t                sys_load_library( const wchar_t* path );
void                    sys_close_library( module_t mod );
void*                   sys_load_func( module_t mod, const char* path );

// system error
const char*             sys_get_error();
const wchar_t*          sys_get_error_w();
void                    sys_print_last_error();

// --------------------------------------------------------------------------------------------------------
// string conversion functions, for windows primarily
// also known as "sys_to_utf16"
wchar_t*                sys_to_wchar( const char* spStr, int sSize );
wchar_t*                sys_to_wchar( const char* spStr );

// prepends "\\?\" on the string for windows
// https://learn.microsoft.com/en-us/windows/win32/fileio/naming-a-file
wchar_t*                sys_to_wchar_extended( const char* spStr, int sSize );
wchar_t*                sys_to_wchar_extended( const char* spStr );

wchar_t*                sys_to_wchar_short( const char* spStr, int sSize );
wchar_t*                sys_to_wchar_short( const char* spStr );

char*                   sys_to_utf8( const wchar_t* spStr, int sSize );
char*                   sys_to_utf8( const wchar_t* spStr );

// --------------------------------------------------------------------------------------------------------
// Filesystem

// get folder exe is stored in
char*                   sys_get_exe_folder( size_t* len = nullptr );

// get the full path of the exe
char*                   sys_get_exe_path( size_t* len = nullptr );

// get current working directory
char*                   sys_get_cwd();

// File Times
bool                    sys_get_file_times( const char* path, u64* creation, u64* access, u64* write );
bool                    sys_set_file_times( const char* path, u64* creation, u64* access, u64* write );
bool                    sys_copy_file_times( const char* src_path, const char* out_path, bool creation, bool access, bool write );

// Get list of drives mounted on this device
// Windows returns drive letters
std::vector< fs::path > sys_get_drives();


// --------------------------------------------------------------------------------------------------------
// Terminal


// execute a command and read it's output
bool                    sys_execute_read( const char* command, str_buf_t& output );

// execute a command and read it's output, with a callback function everytime more output is read from the file
bool                    sys_execute_read_callback( const char* command, str_buf_t& output, f_exec_callback* p_exec_callback );

// execute a command and return the commands return value
int                     sys_execute( const char* command );


// --------------------------------------------------------------------------------------------------------
// Shell Functions

// on windows, this sends the file to the recycle bin
// it does the equivalent on other platforms
bool                    sys_recycle_file( const char* path );

// on windows, this opens the file properties dialog
void                    sys_open_file_properties( const char* path );

bool                    sys_copy_to_clipboard( const char* path );

// hack for above
// void                    sys_set_main_hwnd( void* hwnd );

// NOTE: path cannot be over MAX_PATH (260 characters), thanks windows shell
void                    sys_browse_to_file( const char* path );

// print color with \aFFF escape codes for color values
//void        sys_print_color( const char* string );


// --------------------------------------------------------------------------------------------------------
// Other


proc_mem_info_t         sys_get_mem_info();

// get the default font to use for imgui
sys_font_data_t         sys_get_font();

