#include "util.h"
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
#include <sys/stat.h>

#include <profileapi.h>
#include <stdint.h>

#include <SDL3/SDL_system.h>
#include <SDL3/SDL_video.h>


// ----------------------------------------------------------------------------------------


HANDLE               g_con_out   = INVALID_HANDLE_VALUE;
HWND                 g_main_hwnd = 0;
static LARGE_INTEGER g_win_perf_freq;


// ----------------------------------------------------------------------------------------



bool fs_exists( const char* path )
{
	DWORD attributes = GetFileAttributesA( path );
	return attributes != INVALID_FILE_ATTRIBUTES;
}


bool fs_make_dir( const char* path )
{
	int ret = SHCreateDirectoryExA( g_main_hwnd, path, nullptr );

	if ( ret != 0 )
	{
		printf( "Failed to create directory %d\n", ret );
		sys_print_last_error();
	}

	return ret;
}


bool fs_is_dir( const char* path )
{
	DWORD attributes = GetFileAttributesA( path );

	if ( attributes == INVALID_FILE_ATTRIBUTES )
		return false;

	if ( attributes & FILE_ATTRIBUTE_DIRECTORY )
		return true;

	return false;
}


bool fs_is_file( const char* path )
{
	wchar_t*                  path_w = sys_to_wchar_extended( path );

	WIN32_FILE_ATTRIBUTE_DATA data{};
	BOOL                      ret = GetFileAttributesEx( path_w, GetFileExInfoStandard, &data );

	// DWORD    attributes = GetFileAttributesEx( path_w,  );

	ch_free_str( path_w );

	if ( !ret )
	{
		printf( "Failed to get file attributes: %s\n", path );
		sys_print_last_error();
		return false;
	}

	// if ( attributes == INVALID_FILE_ATTRIBUTES )
	// 	return false;

	if ( !( data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) )
		return true;

	return false;
}


// ----------------------------------------------------------------------------------------


bool sys_init()
{
	QueryPerformanceFrequency( &g_win_perf_freq );

	g_con_out = GetStdHandle( STD_OUTPUT_HANDLE );

	if ( g_con_out == INVALID_HANDLE_VALUE )
	{
		printf( "Failed to get console output handle\n" );
		sys_print_last_error();
		return false;
	}

	if ( !SUCCEEDED( OleInitialize( NULL ) ) )
	{
		printf( "Failed to init OLE\n" );
		sys_print_last_error();
		return false;
	}

	

	return true;
}


void sys_shutdown()
{
	OleUninitialize();
	// drag_drop_remove( g_main_hwnd );
}


void sys_update()
{
}


void sys_set_window( SDL_Window* window )
{
	SDL_PropertiesID props = SDL_GetWindowProperties( window );
	void*            hwnd  = SDL_GetPointerProperty( props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr );

	if ( hwnd )
	{
		g_main_hwnd = (HWND)hwnd;
		// drag_drop_register( g_main_hwnd );

		if ( app::config.dwm_extend )
		{
			MARGINS margins{ -1 };
			// margins.cxLeftWidth = 0;
			// margins.cxRightWidth = 800;
			// margins.cyBottomHeight = 400;
			// margins.cyTopHeight = 0;

			HRESULT res = DwmExtendFrameIntoClientArea( g_main_hwnd, &margins );

			if ( res != S_OK )
			{
				printf( "Failed to extend frame into client area\n" );
				sys_print_last_error();
			}
		}
	}
	else
	{
		printf( "Failed to get HWND from Window: %s\n", SDL_GetError() );
	}
}


constexpr int SC_DRAGMOVE = SC_MOVE | HTCAPTION;


void sys_do_window_drag( ImVec2 last_mouse_pos, ImVec2 new_mouse_pos )
{
	// POINT cursor_pos;
	// GetCursorPos( &cursor_pos );
	// 
	// ImVec2 new_pos;
	// new_pos[ 0 ] = cursor_pos.x - last_pos.x;
	// new_pos[ 1 ] = cursor_pos.y - last_pos.y;

	// MoveWindow( g_main_hwnd, new_pos[ 0 ], new_pos[ 1 ], WINDOW_SIZE[ 0 ], WINDOW_SIZE[ 1 ], false );

	// INSTANT WINDOW DRAGGING
	// https://stackoverflow.com/a/66919909/12778316
	SendMessage( g_main_hwnd, WM_SYSCOMMAND, SC_DRAGMOVE, 0 );
}


// ----------------------------------------------------------------------------------------
// Library Loading


module_t sys_load_library( const wchar_t* path )
{
	return (module_t)LoadLibrary( path );
}


void sys_close_library( module_t mod )
{
	FreeLibrary( (HMODULE)mod );
}


void* sys_load_func( module_t mod, const char* name )
{
	return GetProcAddress( (HMODULE)mod, name );
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
// Filesystem


char* sys_get_exe_folder( size_t* len )
{
	wchar_t output_w[ 4096 ];
	GetModuleFileName( NULL, output_w, 4096 );

	// find index of last path separator
	wchar_t* sep    = wcsrchr( output_w, '\\' );
	size_t   path_i = sep - output_w;

	char*    output = sys_to_utf8( output_w, path_i );

	if ( len )
		*len = path_i;

	return output;
}


char* sys_get_exe_path( size_t* len )
{
	wchar_t output_w[ 4096 ];
	GetModuleFileName( NULL, output_w, 4096 );

	size_t len_w = wcslen( output_w );

	char* output = sys_to_utf8( output_w, len_w );

	if ( len )
		*len = len_w;

	return output;
}


char* sys_get_cwd()
{
	return _getcwd( 0, 0 );
}


// static u64 file_time_to_u64( FILETIME& time )
// {
// 	return static_cast< u64 >( time.dwHighDateTime ) << 32 | time.dwLowDateTime;
// }


// this is weird
// https://stackoverflow.com/a/26416380
u64 file_time_to_unix( const FILETIME& filetime )
{
	FILETIME localFileTime;
	FileTimeToLocalFileTime( &filetime, &localFileTime );
	SYSTEMTIME sysTime;
	FileTimeToSystemTime( &localFileTime, &sysTime );
	struct tm tmtime = { 0 };
	tmtime.tm_year   = sysTime.wYear - 1900;
	tmtime.tm_mon    = sysTime.wMonth - 1;
	tmtime.tm_mday   = sysTime.wDay;
	tmtime.tm_hour   = sysTime.wHour;
	tmtime.tm_min    = sysTime.wMinute;
	tmtime.tm_sec    = sysTime.wSecond;
	tmtime.tm_wday   = 0;
	tmtime.tm_yday   = 0;
	tmtime.tm_isdst  = -1;
	time_t ret       = mktime( &tmtime );
	return ret;
}  


// ????
// https://support.microsoft.com/en-us/topic/bf03df72-96e4-59f3-1d02-b6781002dc7f
static FILETIME file_time_from_unix( u64 time )
{
	// Note that LONGLONG is a 64-bit value
	LONGLONG ll;
	FILETIME filetime;

	ll                      = Int32x32To64( time, 10000000 ) + 116444736000000000;
	filetime.dwLowDateTime  = (DWORD)ll;
	filetime.dwHighDateTime = ll >> 32;

	return filetime;
}


bool sys_get_file_times_and_size( const char* path, u64* creation, u64* access, u64* write, u64* size )
{
	wchar_t* path_w = sys_to_wchar( path );

	struct _stat s;
	if ( _wstat( path_w, &s ) != 0 )
	{
		ch_free_str( path_w );
		return false;
	}

	if ( creation )
		*creation = s.st_ctime;

	if ( write )
		*write = s.st_mtime;

	if ( access )
		*access = s.st_atime;

	if ( size )
		*size = s.st_size;

	ch_free_str( path_w );

	return true;
}


// unreliable as hell wtf
bool sys_set_file_times( const char* path, u64* creation, u64* access, u64* write )
{
	wchar_t* path_w = sys_to_wchar_extended( path );

	// FILE_WRITE_ATTRIBUTES

	// https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createfilew#caching-behavior
	// FILE_FLAG_RANDOM_ACCESS

	SECURITY_ATTRIBUTES attrib{};

	HANDLE   file   = CreateFile( path_w, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL );

	if ( file == INVALID_HANDLE_VALUE )
	{
		printf( "failed to open file handle for \"%s\"\n", path );
		sys_print_last_error();
		return false;
	}

	FILETIME file_create{}, file_access{}, file_write{};
	BOOL     ret = GetFileTime( file, &file_create, &file_access, &file_write );

	if ( ret == FALSE )
	{
		CloseHandle( file );
		sys_print_last_error();
		printf( "failed to get file time - \"%s\"\n", path );
		return false;
	}

	if ( creation )
		file_create = file_time_from_unix( *creation );

	if ( access )
		file_access = file_time_from_unix( *access );
	 
	if ( write )
		file_write = file_time_from_unix( *write );

	ret = SetFileTime( file, &file_create, &file_access, &file_write );

	CloseHandle( file );

	if ( ret == FALSE )
	{
		sys_print_last_error();
		printf( "failed to set file time - \"%s\"\n", path );
		return false;
	}

	ch_free_str( path_w );

	return true;
}


bool sys_get_drives( std::vector< std::string >& drives )
{
	wchar_t                 drive_str[ MAX_PATH ];
	memset( drive_str, 0, sizeof( drive_str ) );

	// doesn't see network drives? try GetDriveType?
	if ( !GetLogicalDriveStrings( MAX_PATH, drive_str ) )
	{
		sys_print_last_error();
		printf( "Failed to get logical drives!\n" );
		return false;
	}

	for ( u32 i = 0; i < MAX_PATH; i += 4 )
	{
		if ( drive_str[ i ] == L'\0' )
			break;

		std::string drive( &drive_str[ i ], &drive_str[ i + 4 ] );
		drives.push_back( drive );
	}

	return true;
}


bool sys_scandir( const char* root, const char* path, std::vector< file_t >& files, e_scandir_flags flags )
{
	std::string scan_dir = root, scan_dir_wildcard{};
	scan_dir += SEP_S;

	if ( path )
	{
		scan_dir += path;
		scan_dir += SEP_S;
	}
	
	scan_dir_wildcard += scan_dir;
	scan_dir_wildcard += "*";

	wchar_t*        scan_dir_wildcard_w = sys_to_wchar_extended( scan_dir_wildcard.c_str() );

	WIN32_FIND_DATA ffd;
	HANDLE          find = FindFirstFileEx( scan_dir_wildcard_w, FindExInfoBasic, &ffd, FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH );

	ch_free_str( scan_dir_wildcard_w );

	if ( INVALID_HANDLE_VALUE == find )
	{
		printf( "Failed to find first file in directory: \"%s\"\n", path );
		return false;
	}

	while ( FindNextFile( find, &ffd ) != 0 )
	{
		// NOTE FROM https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-findfirstfileexa#remarks
		// "In rare cases or on a heavily loaded system, file attribute information on NTFS file systems may not be current at the time this function is called.
		// To be assured of getting the current NTFS file system file attributes, call the GetFileInformationByHandle function."
		// maybe add a check for that here?

		if ( ffd.dwFileAttributes == INVALID_FILE_ATTRIBUTES )
		{
			wprintf( L"File Attributes is Invalid: %s?\n", ffd.cFileName );
		}

		bool is_dir = ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;

		if ( is_dir && wcsncmp( ffd.cFileName, L"..", 2 ) == 0 )
			continue;

		std::string relative_path;

		if ( path )
		{
			relative_path += path;
			relative_path += SEP_S;
		}

		char* filename = sys_to_utf8( ffd.cFileName );
		relative_path += filename;

		if ( ( flags & e_scandir_recursive ) && is_dir )
			sys_scandir( root, relative_path.data(), files, flags );

		if ( ( flags & e_scandir_no_dirs ) && is_dir )
		{
			ch_free_str( filename );
			continue;
		}

		// if ( ( flags & e_scandir_no_files ) && fs_is_file( relative_path.data() ) )
		if ( ( flags & e_scandir_no_files ) && !is_dir )
		{
			ch_free_str( filename );
			continue;
		}

		file_t file{};
		file.date_created = file_time_to_unix( ffd.ftCreationTime );
		file.date_mod = file_time_to_unix( ffd.ftLastWriteTime );

		if ( flags & e_scandir_abs_paths )
			file.path = scan_dir + filename;
		else
			file.path = relative_path;

		ch_free_str( filename );

		if ( is_dir )
		{
			file.type |= e_file_type_directory;
			file.size = 0;
		}
		else
		{
			file.type |= e_file_type_file;
			file.size = (ffd.nFileSizeHigh * (MAXDWORD+1)) + ffd.nFileSizeLow;
		}

		files.push_back( file );
	}

	FindClose( find );
	return true;
}


// --------------------------------------------------------------------------------------------------------
// Shell Functions


bool sys_recycle_file( const char* path )
{
	//if ( !hwnd )
	//{
	//	printf( "No HWND specified" );
	//	return false;
	//}

	TCHAR    Buffer[ 2048 + 4 ];

	wchar_t* path_w = sys_to_wchar( path );

	wcsncpy_s( Buffer, 2048 + 4, path_w, 2048 );
	Buffer[ wcslen( Buffer ) + 1 ] = 0;  //Double-Null-Termination

	SHFILEOPSTRUCT s;
	s.hwnd                  = NULL;
	s.wFunc                 = FO_DELETE;
	s.pFrom                 = Buffer;
	s.pTo                   = NULL;
	s.fFlags                = FOF_ALLOWUNDO;
	s.fAnyOperationsAborted = false;
	s.hNameMappings         = NULL;
	s.lpszProgressTitle     = NULL;

	//if ( !showConfirm )
	s.fFlags |= FOF_SILENT;

	int rc = SHFileOperation( &s );

	ch_free_str( path_w );

	if ( rc != 0 )
	{
		printf( "Failed To Delete File: %s\n", path );
		return false;
	}

	printf( "Deleted File: %s\n", path );
	return true;
}


void sys_open_file_properties( const char* file )
{
	wchar_t* path_w = sys_to_wchar( file );

	if ( !SHObjectProperties( 0, SHOP_FILEPATH, path_w, NULL ) )
	{
		wprintf( L"Failed to open File Properties for file: %s\n", path_w );
	}

	ch_free_str( path_w );
}


// GetUIObjectOfFile incorporated by reference
// https://web.archive.org/web/20140424230840/http://blogs.msdn.com/b/oldnewthing/archive/2004/09/21/231739.aspx
static HRESULT GetUIObjectOfFile( HWND hwnd, LPCWSTR pszPath, REFIID riid, void** ppv )
{
	*ppv = NULL;
	HRESULT      hr;
	LPITEMIDLIST pidl;
	SFGAOF       sfgao;

	if ( SUCCEEDED( hr = SHParseDisplayName( pszPath, NULL, &pidl, 0, &sfgao ) ) )
	{
		IShellFolder* psf;
		LPCITEMIDLIST pidlChild;

		if ( SUCCEEDED( hr = SHBindToParent( pidl, IID_IShellFolder, (void**)&psf, &pidlChild ) ) )
		{
			hr = psf->GetUIObjectOf( hwnd, 1, &pidlChild, riid, NULL, ppv );
			psf->Release();
		}

		CoTaskMemFree( pidl );
	}

	return hr;
}


bool sys_copy_to_clipboard( const char* path )
{
	wchar_t*               path_w = sys_to_wchar( path );
	bool                   ret    = false;

	CComPtr< IDataObject > spdto;

	if ( !SUCCEEDED( GetUIObjectOfFile( nullptr, path_w, IID_PPV_ARGS( &spdto ) ) ) )
		goto end;

	if ( !SUCCEEDED( OleSetClipboard( spdto ) ) )
		goto end;

	if ( !SUCCEEDED( OleFlushClipboard() ) )
		goto end;

	ret = true;

end:
	ch_free_str( path_w );
	return ret;
}


void sys_browse_to_file( const char* path )
{
	wchar_t*    path_w = sys_to_wchar( path );

	ITEMIDLIST* pidl   = ILCreateFromPath( path_w );
	if ( pidl )
	{
		SHOpenFolderAndSelectItems( pidl, 0, 0, 0 );
		ILFree( pidl );
	}

	ch_free_str( path_w );
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

