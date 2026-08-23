#include "util.h"
#include "args.h"
#include "system/system.h"
#include "sys_win32.h"
#include "main.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>
#include <ole2.h>
#include <windowsx.h> // GET_X_LPARAM(), GET_Y_LPARAM()
#include <direct.h>
#include <shellapi.h>
#include <shlwapi.h> 
#include <shlobj.h>
#include <shlobj_core.h> 
#include <time.h>
#include <atlbase.h>
#include <psapi.h>
#include <dwmapi.h>
#include <strsafe.h>
#include <sys/stat.h>

#include <profileapi.h>
#include <stdint.h>
#include <thread>
#include <atomic>

#include <SDL3/SDL_system.h>
#include <SDL3/SDL_video.h>


// ----------------------------------------------------------------------------------------


static HANDLE                  g_singleton_pipe  = INVALID_HANDLE_VALUE;
static std::thread*            g_pipe_thread     = nullptr;
static std::atomic< wchar_t* > g_pipe_buffer     = nullptr;
static std::atomic< bool >     g_focus_window    = false;

HANDLE                         g_con_out         = INVALID_HANDLE_VALUE;
HWND                           g_main_hwnd       = 0;
LARGE_INTEGER                  g_win_perf_freq;

constexpr const wchar_t*       WINDOW_PIPE_PATH = L"\\\\.\\pipe\\media_tag_system";
constexpr const size_t         WINDOW_PIPE_SIZE = 1024 * sizeof( wchar_t );


// ----------------------------------------------------------------------------------------


// https://learn.microsoft.com/en-us/windows/win32/ipc/multithreaded-pipe-server
// https://learn.microsoft.com/en-us/windows/win32/ipc/named-pipe-client
static int open_pipe()
{
	g_singleton_pipe = CreateFile( WINDOW_PIPE_PATH, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL );

	// PIPE_UNLIMITED_INSTANCES

	if ( g_singleton_pipe == INVALID_HANDLE_VALUE )
	{
		// doesn't exist, create new one
		g_singleton_pipe = CreateNamedPipe(
		  WINDOW_PIPE_PATH,
		  PIPE_ACCESS_INBOUND,       // read/write access
		  PIPE_TYPE_MESSAGE |        // message type pipe
			PIPE_READMODE_MESSAGE |  // message-read mode
			PIPE_WAIT,             // blocking mode
		  1,  // max. instances
		  WINDOW_PIPE_SIZE,          // output buffer size
		  WINDOW_PIPE_SIZE,          // input buffer size
		  0,                         // client time-out
		  NULL );                    // default security attribute 

		if ( g_singleton_pipe == INVALID_HANDLE_VALUE )
		{
			printf( "Failed to create pipe for interprocess communication!\n" );
			sys_print_last_error();
			return 0;
		}

		return 1;  // new pipe created
	}

	return 2;  // pipe opened
}


void pipe_read_worker()
{
	while ( app::running )
	{
		// wait for any other instance to connect to this pipe
		BOOL connected = ConnectNamedPipe( g_singleton_pipe, NULL );

		if ( !connected )
		{
			SDL_Delay( 500 );
			break;
		}
		
		if ( !app::running )
		{
			DisconnectNamedPipe( g_singleton_pipe );
			break;
		}

		wchar_t buffer[ WINDOW_PIPE_SIZE ]{};
		DWORD   bytes_read    = 0;

		BOOL    read_file_ret = ReadFile(
          g_singleton_pipe,  // handle to pipe
          buffer,            // buffer to receive data
          WINDOW_PIPE_SIZE,  // size of buffer
          &bytes_read,       // number of bytes read
          NULL );            // not overlapped I/O

		if ( read_file_ret )
		{
			g_pipe_buffer.store( wcsdup( buffer ) );
		}
		else
		{
			// user didn't write anything, probably just called the main exe again
			g_focus_window = true;
		}

		// disconnect it, and wait for the next instance
		DisconnectNamedPipe( g_singleton_pipe );
	}
}


e_sys_init sys_init()
{
	// https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/setlocale-wsetlocale?view=msvc-170#utf-8-support
	// Allows using utf8 in the C runtime in windows 10 1803 or newer
	// also allows utf-16/utf-8 to be printed to the console, does this work on windows vista?
	setlocale( LC_ALL, ".utf8" );
	setlocale( LC_NUMERIC, "C" );

	// https://stackoverflow.com/questions/46512441/how-do-i-print-unicode-to-the-output-console-in-c-with-visual-studio

	// https://learn.microsoft.com/en-us/windows/win32/intl/code-page-identifiers
	// Set console to UTF-8 output
	// BOOL con_ret = SetConsoleOutputCP( 65001 );
	// SetConsoleOutputCP( 1200 );

	if ( app::config.single_instance )
	{
		// NOTE: Using pipes here since WM_COPYDATA didn't want to work at all for me
		
		// try to open a pipe
		int pipe_state = open_pipe();

		if ( pipe_state == 2 )
		{
			// opened existing pipe, write to it and close
			fs::path_char* path = nullptr;

			// take the first path here
			for ( int i = 1; i < g_argc; i++ )
			{
				if ( !fs_exists( g_argv[ i ] ) )
					continue;

				path = g_argv[ i ];
				break;
			}

			// optional path to write, still focuses the window either way and keeps it as one program
			if ( path )
			{
				size_t len = ( wcslen( path ) + 1 ) * sizeof( wchar_t );

				if ( len > WINDOW_PIPE_SIZE )
				{
					printf( "PATH TOO LONG!!!\n" );
					return e_sys_init_fail;
				}

				// Write to the pipe
				DWORD bytes_written = 0;
				BOOL  pipe_write    = WriteFile( g_singleton_pipe, path, static_cast< DWORD >( len ), &bytes_written, NULL );
			}
			
			CloseHandle( g_singleton_pipe );
			g_singleton_pipe = INVALID_HANDLE_VALUE;

			// exit out of program
			return e_sys_init_single_instance;
		}
		else if ( pipe_state == 1 )
		{
			g_pipe_thread = new std::thread( pipe_read_worker );
		}
		else
		{
			return e_sys_init_fail;
		}
	}

	g_con_out = GetStdHandle( STD_OUTPUT_HANDLE );

	if ( g_con_out == INVALID_HANDLE_VALUE )
	{
		printf( "Failed to get console output handle\n" );
		sys_print_last_error();
		return e_sys_init_fail;
	}

	if ( !SUCCEEDED( OleInitialize( NULL ) ) )
	{
		printf( "Failed to init OLE\n" );
		sys_print_last_error();
		return e_sys_init_fail;
	}

	if ( !SUCCEEDED( CoInitialize( NULL ) ) )
	{
		printf( "Failed to init COM\n" );
		sys_print_last_error();
		return e_sys_init_fail;
	}

	return e_sys_init_success;
}


void sys_shutdown()
{
	if ( app::config.single_instance )
	{
		// ffs, stop the pipe from waiting so the loop knows we are closing
		HANDLE pipe_temp = CreateFile( WINDOW_PIPE_PATH, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL );
		CloseHandle( pipe_temp );

		if ( g_pipe_thread )
		{
			g_pipe_thread->join();
			g_pipe_thread = nullptr;
		}

		if ( g_singleton_pipe != INVALID_HANDLE_VALUE )
		{
			CloseHandle( g_singleton_pipe );
			g_singleton_pipe = INVALID_HANDLE_VALUE;
		}
	}

	OleUninitialize();
	// drag_drop_remove( g_main_hwnd );

	sys_free_exe_path_vars();
}


void sys_update()
{
	if ( app::config.single_instance )
	{
		wchar_t* buffer = g_pipe_buffer.load();

		if ( buffer )
		{
			on_new_file( buffer );
			free( buffer );
			g_pipe_buffer.store( nullptr );

			SDL_RaiseWindow( app::window );
		}
		else if ( g_focus_window )
		{
			SDL_RaiseWindow( app::window );
			g_focus_window = false;
		}
	}
}


bool sys_set_window( SDL_Window* window )
{
	SDL_PropertiesID props = SDL_GetWindowProperties( window );
	void*            hwnd  = SDL_GetPointerProperty( props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr );

	if ( !hwnd )
	{
		printf( "Failed to get HWND from Window: %s\n", SDL_GetError() );
		return false;
	}

	g_main_hwnd = (HWND)hwnd;
	drag_drop_register( g_main_hwnd );

	if ( app::config.dwm_extend )
	{
		MARGINS margins{ -1 };
		HRESULT res = DwmExtendFrameIntoClientArea( g_main_hwnd, &margins );

		if ( res != S_OK )
		{
			printf( "Failed to extend frame into client area\n" );
			sys_print_last_error();
		}
	}

	return true;
}


constexpr int SC_DRAGMOVE = SC_MOVE | HTCAPTION;


void sys_do_window_drag( ImVec2 last_mouse_pos, ImVec2 new_mouse_pos )
{
	// INSTANT WINDOW DRAGGING
	// https://stackoverflow.com/a/66919909/12778316
	SendMessage( g_main_hwnd, WM_SYSCOMMAND, SC_DRAGMOVE, 0 );
}


// ----------------------------------------------------------------------------------------
// System Errors


const wchar_t* sys_get_error_w()
{
	DWORD errorID = GetLastError();

	if ( errorID == 0 )
		return L"";  // No error message

	// LPTSTR strErrorMessage = NULL;
	WCHAR strErrorMessage[ 1024 ];

	DWORD ret = FormatMessageW(
	  FORMAT_MESSAGE_FROM_SYSTEM,
	  NULL,
	  errorID,
	  0,
	  strErrorMessage,
	  1024,
	  NULL );

	static wchar_t message[ 1100 ];
	memset( message, 0, sizeof( wchar_t ) * 1100 );

	if ( ret == 0 )
	{
		printf( "smh FormatMessageW failed with %d\n", GetLastError() );
		_snwprintf( message, 1100, L"Win32 API Error %ud", errorID );
		return message;
	}

	_snwprintf( message, 1100, L"Win32 API Error %u: %s", errorID, strErrorMessage );

	// Free the Win32 string buffer.
	// LocalFree( strErrorMessage );

	return message;
}


char* sys_get_error()
{
	const wchar_t* error = sys_get_error_w();

	if ( !error )
		return util_strdup( "" );

	return sys_to_utf8( error );
}


void sys_print_last_error()
{
	fwprintf( stderr, L"Error: %s\n", sys_get_error_w() );
}


// --------------------------------------------------------------------------------------------------------
// Terminal


// https://stackoverflow.com/a/35658917
bool sys_execute_read( const char* command, std::string& output )
{
	HANDLE              hPipeRead, hPipeWrite;

	SECURITY_ATTRIBUTES saAttr  = { sizeof( SECURITY_ATTRIBUTES ) };
	saAttr.bInheritHandle       = TRUE;  // Pipe handles are inherited by child process.
	saAttr.lpSecurityDescriptor = NULL;

	// Create a pipe to get results from child's stdout.
	if ( !CreatePipe( &hPipeRead, &hPipeWrite, &saAttr, 0 ) )
		return false;

	STARTUPINFOW si               = { sizeof( STARTUPINFOW ) };
	si.dwFlags                    = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
	si.hStdOutput                 = hPipeWrite;
	si.hStdError                  = hPipeWrite;
	si.wShowWindow                = SW_HIDE;  // Prevents cmd window from flashing.
											  // Requires STARTF_USESHOWWINDOW in dwFlags.

	PROCESS_INFORMATION pi        = { 0 };

	wchar_t*            command_w = sys_to_wchar( command );

	BOOL                fSuccess  = CreateProcessW( NULL, command_w, NULL, NULL, TRUE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi );

	ch_free_str( command_w );

	if ( !fSuccess )
	{
		CloseHandle( hPipeWrite );
		CloseHandle( hPipeRead );
		return false;
	}

	memset( &output, 0, sizeof( str_buf_t ) );

	bool bProcessEnded = false;
	while ( !bProcessEnded )
	{
		// Give some timeslice (50 ms), so we won't waste 100% CPU.
		bProcessEnded = WaitForSingleObject( pi.hProcess, 50 ) == WAIT_OBJECT_0;

		// Even if process exited - we continue reading, if
		// there is some data available over pipe.
		for ( ;; )
		{
			char  buf[ 1024 ];
			DWORD dwRead  = 0;
			DWORD dwAvail = 0;

			if ( !::PeekNamedPipe( hPipeRead, NULL, 0, NULL, &dwAvail, NULL ) )
				break;

			if ( !dwAvail )  // No data available, return
				break;

			if ( !::ReadFile( hPipeRead, buf, MIN( sizeof( buf ) - 1, dwAvail ), &dwRead, NULL ) || !dwRead )
				// Error, the child process might ended
				break;

			buf[ dwRead ] = 0;
			output.append( buf );
		}
	}

	CloseHandle( hPipeWrite );
	CloseHandle( hPipeRead );
	CloseHandle( pi.hProcess );
	CloseHandle( pi.hThread );
	return true;
}


// https://stackoverflow.com/a/35658917
bool sys_execute_read_callback( const char* command, std::string& output, f_exec_callback* p_exec_callback )
{
	if ( !p_exec_callback )
		return false;

	HANDLE              hPipeRead, hPipeWrite;

	SECURITY_ATTRIBUTES saAttr  = { sizeof( SECURITY_ATTRIBUTES ) };
	saAttr.bInheritHandle       = TRUE;  // Pipe handles are inherited by child process.
	saAttr.lpSecurityDescriptor = NULL;

	// Create a pipe to get results from child's stdout.
	if ( !CreatePipe( &hPipeRead, &hPipeWrite, &saAttr, 0 ) )
		return false;

	STARTUPINFOW si               = { sizeof( STARTUPINFOW ) };
	si.dwFlags                    = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
	si.hStdOutput                 = hPipeWrite;
	si.hStdError                  = hPipeWrite;
	si.wShowWindow                = SW_HIDE;  // Prevents cmd window from flashing.
											  // Requires STARTF_USESHOWWINDOW in dwFlags.

	PROCESS_INFORMATION pi        = { 0 };

	wchar_t*            command_w = sys_to_wchar( command );

	BOOL                fSuccess  = CreateProcessW( NULL, command_w, NULL, NULL, TRUE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi );

	ch_free_str( command_w );

	if ( !fSuccess )
	{
		CloseHandle( hPipeWrite );
		CloseHandle( hPipeRead );
		return false;
	}

	memset( &output, 0, sizeof( str_buf_t ) );

	bool bProcessEnded = false;
	while ( !bProcessEnded )
	{
		// Give some timeslice (50 ms), so we won't waste 100% CPU.
		bProcessEnded = WaitForSingleObject( pi.hProcess, 50 ) == WAIT_OBJECT_0;

		// Even if process exited - we continue reading, if
		// there is some data available over pipe.
		for ( ;; )
		{
			char  buf[ 1024 ];
			DWORD dwRead  = 0;
			DWORD dwAvail = 0;

			if ( !::PeekNamedPipe( hPipeRead, NULL, 0, NULL, &dwAvail, NULL ) )
				break;

			if ( !dwAvail )  // No data available, return
				break;

			if ( !::ReadFile( hPipeRead, buf, MIN( sizeof( buf ) - 1, dwAvail ), &dwRead, NULL ) || !dwRead )
				// Error, the child process might ended
				break;

			buf[ dwRead ]  = 0;
			size_t buf_len = strlen( buf );
			p_exec_callback( buf, buf_len );
			output.append( buf, buf_len );
		}
	}

	CloseHandle( hPipeWrite );
	CloseHandle( hPipeRead );
	CloseHandle( pi.hProcess );
	CloseHandle( pi.hThread );
	return true;
}


int sys_execute( const char* command )
{
	PROCESS_INFORMATION pi        = { 0 };
	wchar_t*            command_w = sys_to_wchar( command );
	STARTUPINFOW        si        = { sizeof( STARTUPINFOW ) };

	BOOL                success   = CreateProcessW( NULL, command_w, NULL, NULL, TRUE, BELOW_NORMAL_PRIORITY_CLASS, NULL, NULL, &si, &pi );

	ch_free_str( command_w );

	if ( !success )
	{
		sys_print_last_error();
		return false;
	}

	WaitForSingleObject( pi.hProcess, INFINITE );

	DWORD exit_code = 0;
	BOOL  ret       = GetExitCodeProcess( pi.hProcess, &exit_code );

	CloseHandle( pi.hProcess );
	CloseHandle( pi.hThread );

	return (int)exit_code;
}


// --------------------------------------------------------------------------------------------------------
// Other


// TODO: query the registry to get the font path
sys_font_data_t      sys_get_font()
{
	NONCLIENTMETRICS metrics{ sizeof( NONCLIENTMETRICS ) };

	BOOL             ret = SystemParametersInfo( SPI_GETNONCLIENTMETRICS, sizeof( NONCLIENTMETRICS ), &metrics, 0 );

	if ( ret == FALSE )
	{
		sys_print_last_error();
		printf( "Failed to get info for font paths\n" );
		return {};
	}

	sys_font_data_t font_data{};

	wchar_t         buf[ 512 ];
	// _snwprintf( buf, 512, L"C:\\Windows\\Fonts\\%s.ttf", metrics.lfCaptionFont.lfFaceName );
	_snwprintf( buf, 512, L"C:\\Windows\\Fonts\\%s.ttf", L"segoeui" );

	font_data.font_path = sys_to_utf8( buf );
	// font_data.height    = abs( metrics.lfCaptionFont.lfHeight );
	font_data.height    = 17;
	font_data.weight    = abs( metrics.lfCaptionFont.lfWeight );

	return font_data;
}


proc_mem_info_t sys_get_mem_info()
{
	proc_mem_info_t mem_info{};

	// Get a handle to the current process.
	HANDLE          hProcess = GetCurrentProcess();

	if ( NULL == hProcess )
	{
		printf( "failed to open current process\n" );
		sys_print_last_error();
		return mem_info;
	}

	PROCESS_MEMORY_COUNTERS pmc;
	// Set the size of the structure
	pmc.cb = sizeof( pmc );

	// Get the memory usage details
	if ( GetProcessMemoryInfo( hProcess, &pmc, sizeof( pmc ) ) )
	{
		mem_info.working_set = pmc.WorkingSetSize;
		mem_info.page_file   = pmc.PagefileUsage;

		// WorkingSetSize is the current physical RAM usage (in bytes)
		//std::cout << "  WorkingSetSize: " << std::dec << pmc.WorkingSetSize / 1024 << " KB" << std::endl;
		//// PagefileUsage is the current size in the system paging file (in bytes)
		//std::cout << "  PagefileUsage:  " << std::dec << pmc.PagefileUsage / 1024 << " KB" << std::endl;
	}
	else
	{
		printf( "failed to get memory usage\n" );
		sys_print_last_error();
	}

	// Close the process handle
	CloseHandle( hProcess );

	return mem_info;
}


u64 sys_get_time_ms()
{
	LARGE_INTEGER counter;
	QueryPerformanceCounter( &counter );
	return (uint64_t)( counter.QuadPart * 1000 / g_win_perf_freq.QuadPart );
}


// non-exception based path conversion
std::string sys_path_to_string( const fs::path& path )
{
	const std::wstring& wstring = path.native();
	int                 size    = WideCharToMultiByte( CP_UTF8, 0, wstring.c_str(), -1, NULL, 0, NULL, NULL );
	std::string         ret( size - 1, 0 );
	WideCharToMultiByte( CP_UTF8, 0, wstring.data(), -1, ret.data(), size - 1, NULL, NULL );
	return ret;
}


void sys_path_to_string( fs::path&& path, std::string& output )
{
	const std::wstring& wstring = path.native();
	int                 size    = WideCharToMultiByte( CP_UTF8, 0, wstring.c_str(), -1, NULL, 0, NULL, NULL );

	output.resize( size - 1 );
	WideCharToMultiByte( CP_UTF8, 0, wstring.data(), -1, output.data(), size - 1, NULL, NULL );
}


void sys_path_to_string( const fs::path_char* path, std::string& output )
{
	int size = WideCharToMultiByte( CP_UTF8, 0, path, -1, NULL, 0, NULL, NULL );

	output.resize( size - 1 );
	WideCharToMultiByte( CP_UTF8, 0, path, -1, output.data(), size - 1, NULL, NULL );
}


fs::path sys_string_to_path( const std::string& path_str )
{
	int          size = MultiByteToWideChar( CP_UTF8, 0, path_str.c_str(), -1, NULL, 0 );
	std::wstring ret( size - 1, 0 );
	MultiByteToWideChar( CP_UTF8, 0, path_str.c_str(), -1, ret.data(), size - 1 );
	fs::path path( ret );
	return path;
}


fs::path_str sys_string_to_path_str( const std::string& path_str )
{
	int          size = MultiByteToWideChar( CP_UTF8, 0, path_str.c_str(), -1, NULL, 0 );
	std::wstring ret( size - 1, 0 );
	MultiByteToWideChar( CP_UTF8, 0, path_str.c_str(), -1, ret.data(), size - 1 );
	return ret;
}

