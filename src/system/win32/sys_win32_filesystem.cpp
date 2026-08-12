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
#include <strsafe.h>
#include <sys/stat.h>

#include <profileapi.h>
#include <stdint.h>
#include <thread>
#include <atomic>


// ----------------------------------------------------------------------------------------


typedef _Return_type_success_( return >= 0 ) LONG NTSTATUS;
typedef NTSTATUS* PNTSTATUS;

typedef enum _FILE_INFORMATION_CLASS
{
	FileDirectoryInformation                     = 1,
	FileFullDirectoryInformation                 = 2,
	FileBothDirectoryInformation                 = 3,
	FileBasicInformation                         = 4,
	FileStandardInformation                      = 5,
	FileInternalInformation                      = 6,
	FileEaInformation                            = 7,
	FileAccessInformation                        = 8,
	FileNameInformation                          = 9,
	FileRenameInformation                        = 10,
	FileLinkInformation                          = 11,
	FileNamesInformation                         = 12,
	FileDispositionInformation                   = 13,
	FilePositionInformation                      = 14,
	FileFullEaInformation                        = 15,
	FileModeInformation                          = 16,
	FileAlignmentInformation                     = 17,
	FileAllInformation                           = 18,
	FileAllocationInformation                    = 19,
	FileEndOfFileInformation                     = 20,
	FileAlternateNameInformation                 = 21,
	FileStreamInformation                        = 22,
	FilePipeInformation                          = 23,
	FilePipeLocalInformation                     = 24,
	FilePipeRemoteInformation                    = 25,
	FileMailslotQueryInformation                 = 26,
	FileMailslotSetInformation                   = 27,
	FileCompressionInformation                   = 28,
	FileObjectIdInformation                      = 29,
	FileCompletionInformation                    = 30,
	FileMoveClusterInformation                   = 31,
	FileQuotaInformation                         = 32,
	FileReparsePointInformation                  = 33,
	FileNetworkOpenInformation                   = 34,
	FileAttributeTagInformation                  = 35,
	FileTrackingInformation                      = 36,
	FileIdBothDirectoryInformation               = 37,
	FileIdFullDirectoryInformation               = 38,
	FileValidDataLengthInformation               = 39,
	FileShortNameInformation                     = 40,
	FileIoCompletionNotificationInformation      = 41,
	FileIoStatusBlockRangeInformation            = 42,
	FileIoPriorityHintInformation                = 43,
	FileSfioReserveInformation                   = 44,
	FileSfioVolumeInformation                    = 45,
	FileHardLinkInformation                      = 46,
	FileProcessIdsUsingFileInformation           = 47,
	FileNormalizedNameInformation                = 48,
	FileNetworkPhysicalNameInformation           = 49,
	FileIdGlobalTxDirectoryInformation           = 50,
	FileIsRemoteDeviceInformation                = 51,
	FileUnusedInformation                        = 52,
	FileNumaNodeInformation                      = 53,
	FileStandardLinkInformation                  = 54,
	FileRemoteProtocolInformation                = 55,
	FileRenameInformationBypassAccessCheck       = 56,
	FileLinkInformationBypassAccessCheck         = 57,
	FileVolumeNameInformation                    = 58,
	FileIdInformation                            = 59,
	FileIdExtdDirectoryInformation               = 60,
	FileReplaceCompletionInformation             = 61,
	FileHardLinkFullIdInformation                = 62,
	FileIdExtdBothDirectoryInformation           = 63,
	FileDispositionInformationEx                 = 64,
	FileRenameInformationEx                      = 65,
	FileRenameInformationExBypassAccessCheck     = 66,
	FileDesiredStorageClassInformation           = 67,
	FileStatInformation                          = 68,
	FileMemoryPartitionInformation               = 69,
	FileStatLxInformation                        = 70,
	FileCaseSensitiveInformation                 = 71,
	FileLinkInformationEx                        = 72,
	FileLinkInformationExBypassAccessCheck       = 73,
	FileStorageReserveIdInformation              = 74,
	FileCaseSensitiveInformationForceAccessCheck = 75,
	FileKnownFolderInformation                   = 76,
	FileStatBasicInformation                     = 77,
	FileId64ExtdDirectoryInformation             = 78,
	FileId64ExtdBothDirectoryInformation         = 79,
	FileIdAllExtdDirectoryInformation            = 80,
	FileIdAllExtdBothDirectoryInformation        = 81,
	FileStreamReservationInformation,
	FileMupProviderInfo,
	FileMaximumInformation
} FILE_INFORMATION_CLASS, *PFILE_INFORMATION_CLASS;

// https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/ns-wdm-_io_status_block?redirectedfrom=MSDN
typedef struct _IO_STATUS_BLOCK
{
	union
	{
		NTSTATUS Status;
		PVOID    Pointer;
	};
	ULONG_PTR Information;
} IO_STATUS_BLOCK, *PIO_STATUS_BLOCK;

// https://learn.microsoft.com/en-us/windows/win32/api/ntdef/ns-ntdef-_unicode_string
typedef struct _UNICODE_STRING
{
	USHORT Length;
	USHORT MaximumLength;
	PWSTR  Buffer;
} UNICODE_STRING, *PUNICODE_STRING;

// See the handy table linked on the page below to learn where these values comes from.
// https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/using-ntstatus-values
constexpr NTSTATUS STATUS_NO_MORE_FILES = 0x80000006;
constexpr NTSTATUS STATUS_NO_SUCH_FILE  = 0xC000000F;

typedef struct _FILE_DIRECTORY_INFORMATION
{
	ULONG         NextEntryOffset;
	ULONG         FileIndex;
	LARGE_INTEGER CreationTime;
	LARGE_INTEGER LastAccessTime;
	LARGE_INTEGER LastWriteTime;
	LARGE_INTEGER ChangeTime;
	LARGE_INTEGER EndOfFile;
	LARGE_INTEGER AllocationSize;
	ULONG         FileAttributes;
	ULONG         FileNameLength;
	WCHAR         FileName[ 1 ];
} FILE_DIRECTORY_INFORMATION;

// useful if i want to backport to XP?
// https://stackoverflow.com/questions/10075514/calling-nt-function-from-ntdll-dll-in-win32-environment-c

extern "C"
{
	// extracted from ntdll
	NTSYSCALLAPI NTSTATUS NtQueryDirectoryFile(
	  HANDLE                 FileHandle,
	  HANDLE                 Event,
	  // This here is PIO_APC_ROUTINE, but we don't use APCs and just set it to null.
	  PVOID                  ApcRoutine,
	  PVOID                  ApcContext,
	  PIO_STATUS_BLOCK       IoStatusBlock,
	  PVOID                  FileInformation,
	  ULONG                  Length,
	  FILE_INFORMATION_CLASS FileInformationClass,
	  BOOLEAN                ReturnSingleEntry,
	  PUNICODE_STRING        FileName,
	  BOOLEAN                RestartScan );
}


// --------------------------------------------------------------------------------------------------------


static char*                   g_exe_path       = nullptr;
static size_t                  g_exe_path_len   = 0;

static char*                   g_exe_folder     = nullptr;
static size_t                  g_exe_folder_len = 0;

static wchar_t*                g_exe_path_w     = nullptr;
static wchar_t*                g_exe_folder_w   = nullptr;


// --------------------------------------------------------------------------------------------------------
// Filesystem


// TODO: we need versions of these with fs::path support
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

	return ret == 0;
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
		// printf( "Failed to get file attributes: %s\n", path );
		// sys_print_last_error();
		return false;
	}

	// if ( attributes == INVALID_FILE_ATTRIBUTES )
	// 	return false;

	if ( !( data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) )
		return true;

	return false;
}


bool sys_setup_exe_path_vars()
{
	// this SUCKS, just in case if we are using a STUPID long path
	std::wstring buffer;
	buffer.resize( MAX_PATH_EXT );

	//wchar_t output_w[ 4096 ];
	DWORD copied = GetModuleFileName( NULL, buffer.data(), MAX_PATH_EXT );
	buffer.resize( copied );

	size_t   len_w   = wcslen( buffer.data() );

	// find index of last path separator
	wchar_t* sep     = wcsrchr( buffer.data(), '\\' );
	size_t   path_i  = sep - buffer.data();

	g_exe_path       = sys_to_utf8( buffer.data(), len_w );
	g_exe_folder     = sys_to_utf8( buffer.data(), path_i );

	if ( !g_exe_path || !g_exe_folder )
		return false;

	g_exe_path_len   = strlen( g_exe_path );
	g_exe_folder_len = strlen( g_exe_folder );

	g_exe_path_w     = (wchar_t*)malloc( ( len_w + 1 ) * sizeof( wchar_t ) );
	g_exe_folder_w   = (wchar_t*)malloc( ( path_i + 1 ) * sizeof( wchar_t ) );

	if ( !g_exe_path_w || !g_exe_folder_w )
		return false;

	memcpy( g_exe_path_w, buffer.data(), len_w * sizeof( wchar_t ) );
	memcpy( g_exe_folder_w, buffer.data(), path_i * sizeof( wchar_t ) );

	g_exe_path_w[ len_w ]    = '\0';
	g_exe_folder_w[ path_i ] = '\0';

	mem_add_item( e_mem_category_string, g_exe_path_w, ( len_w + 1 ) * sizeof( wchar_t ) );
	mem_add_item( e_mem_category_string, g_exe_folder_w, ( path_i + 1 ) * sizeof( wchar_t ) );

	return true;
}


void sys_free_exe_path_vars()
{
	ch_free_str( g_exe_path );
	ch_free_str( g_exe_folder );

	ch_free_str( g_exe_path_w );
	ch_free_str( g_exe_folder_w );
}


const char* sys_get_exe_folder( size_t* len )
{
	if ( len )
		*len = g_exe_folder_len;

	return g_exe_folder;
}


fs::path sys_get_exe_folder_fspath()
{
	return g_exe_folder_w;
}


fs::path::string_type sys_get_exe_folder_native_str()
{
	return g_exe_folder_w;
}


const char* sys_get_exe_path( size_t* len )
{
	if ( len )
		*len = g_exe_path_len;

	return g_exe_path;
}


char* sys_get_cwd()
{
	return _getcwd( 0, 0 );
}


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

		std::string drive( &drive_str[ i ], &drive_str[ i + 2 ] );
		drives.push_back( drive );
	}

	return true;
}


struct _recursive_depth_t
{
	wchar_t* name;
	size_t   name_len;

	wchar_t* root;
	size_t   root_len;
};

// https://blog.s-schoener.com/2024-06-24-find-files-internals/

static bool _open_dir( std::wstring& path, HANDLE& dir_handle, IO_STATUS_BLOCK& statusBlock, void* buffer, ULONG buffer_size )
{
	dir_handle = CreateFileW( path.c_str(),
	                          FILE_LIST_DIRECTORY,
	                          FILE_SHARE_READ,
	                          nullptr,
	                          OPEN_EXISTING,
	                          FILE_FLAG_BACKUP_SEMANTICS,
	                          0 );

	if ( dir_handle == INVALID_HANDLE_VALUE )
		return false;

	NTSTATUS status = NtQueryDirectoryFile(
	  dir_handle, 0, nullptr, nullptr,
	  &statusBlock, buffer, buffer_size,
	  FileDirectoryInformation,
	  FALSE,
	  nullptr,
	  TRUE );

	const size_t bytesWritten = (size_t)statusBlock.Information;

	if ( bytesWritten == 0 || status == STATUS_NO_SUCH_FILE )
	{
		// No file entries found -- this is impossible in this case because we did not
		// specifiy a search string, so we'll find '.' and '..' at the very least.
		CloseHandle( dir_handle );
		return false;
	}

	return true;
}


static size_t __get_scan_count( FILE_DIRECTORY_INFORMATION* tmp_file_dir_info )
{
	size_t files_scanned     = 0;

	while ( tmp_file_dir_info->NextEntryOffset != 0 )
	{
		tmp_file_dir_info = (FILE_DIRECTORY_INFORMATION*)( ( (uint8_t*)tmp_file_dir_info ) + tmp_file_dir_info->NextEntryOffset );
		files_scanned++;
	}

	return files_scanned;
}


constexpr size_t TEMP_PATH_SIZE    = 1024;
constexpr size_t BUFFER_CHUNK_SIZE = 1024 * 6;


static size_t preallocate_vector( FILE_DIRECTORY_INFORMATION* file_dir_info, size_t file_index, std::vector< file_t >& files )
{
	size_t files_scanned = __get_scan_count( file_dir_info );

	if ( files.size() < file_index + files_scanned )
		files.resize( file_index + std::max( files_scanned, BUFFER_CHUNK_SIZE ) );

	return files_scanned;
}


// TODO: increase stack size, and increase size of temp_path_buffer to 32k characters
static bool sys_scandir_internal( const wchar_t* root, std::vector< file_t >& files, e_scandir_flags flags, bool& cancel )
{
	std::wstring scan_dir = root, scan_dir_wildcard{};

	if ( !scan_dir.ends_with( L"\\" ) )
		scan_dir += L"\\";

	scan_dir_wildcard += L"\\\\?\\";
	scan_dir_wildcard += scan_dir;

	size_t                                  recursive_path_count = 0;
	std::forward_list< _recursive_depth_t > recursive_paths{};
	std::wstring                            current_depth{};

	constexpr ULONG             buffer_count = 4096;
	constexpr ULONG             buffer_size  = sizeof( FILE_DIRECTORY_INFORMATION ) * buffer_count;
	FILE_DIRECTORY_INFORMATION* buffer       = ch_calloc< FILE_DIRECTORY_INFORMATION >( buffer_count, e_mem_category_general );

	IO_STATUS_BLOCK statusBlock;
	ZeroMemory( &statusBlock, sizeof( statusBlock ) );

	HANDLE dirHandle{};

	wchar_t                      temp_path_buffer[ 1024 ]{};
	size_t                       file_count = 0;
	//size_t                       file_index = 0;
	size_t file_index = files.size();

	if ( !_open_dir( scan_dir_wildcard, dirHandle, statusBlock, buffer, buffer_size ) )
	{
		wprintf( L"Failed to search directory: %s\n", scan_dir.c_str() );
		ch_free( e_mem_category_general, buffer );
		return false;
	}

	if ( cancel )
		return true;

	// allocate more memory
	preallocate_vector( buffer, file_index, files );

	// file dir info pointer gets offset as this loop goes on, so keep the starting memory pointer to free it later
	FILE_DIRECTORY_INFORMATION* file_dir_info = buffer;

	while ( true )
	{
		if ( file_dir_info->NextEntryOffset == 0 )
		{
			// The state of the search is implictly tied
			// to the handle we are using for the directory.
			NTSTATUS status = NtQueryDirectoryFile(
			  dirHandle, 0, nullptr, nullptr,
			  &statusBlock, buffer, buffer_size,
			  FileDirectoryInformation,
			  FALSE,
			  nullptr,
			  FALSE );

			if ( status == STATUS_NO_MORE_FILES )
			{
				CloseHandle( dirHandle );

				if ( !(flags & e_scandir_recursive) )
					break;

open_dir_recurse_fail:
				if ( recursive_path_count == 0 )
					break;

				if ( cancel )
					return true;

				recursive_path_count--;
				_recursive_depth_t new_path = recursive_paths.front();
				recursive_paths.pop_front();

				scan_dir_wildcard = L"\\\\?\\";
				scan_dir_wildcard += scan_dir;

				if ( new_path.root )
				{
					scan_dir_wildcard.append( new_path.root, new_path.root_len );
					scan_dir_wildcard += '\\';

					current_depth.assign( new_path.root, new_path.root_len );
					current_depth += '\\';
					current_depth.append( new_path.name, new_path.name_len );
				}
				else
				{
					current_depth.assign( new_path.name, new_path.name_len );
				}

				scan_dir_wildcard.append( new_path.name, new_path.name_len );

				ch_free_str( new_path.name );
				ch_free_str( new_path.root );

				if ( !_open_dir( scan_dir_wildcard, dirHandle, statusBlock, buffer, buffer_size ) )
				{
					wprintf( L"Failed to search directory: %s\n", scan_dir_wildcard.c_str() );

					// try again with the next directory to search
					goto open_dir_recurse_fail;
				}

				if ( cancel )
					return true;
			}

			file_dir_info = buffer;
			preallocate_vector( file_dir_info, file_index, files );

			if ( cancel )
				return true;
		}

		// Do something with the file here!
		file_dir_info = (FILE_DIRECTORY_INFORMATION*)( ( (uint8_t*)file_dir_info ) + file_dir_info->NextEntryOffset );

		bool is_dir   = file_dir_info->FileAttributes & FILE_ATTRIBUTE_DIRECTORY;

		size_t character_count = file_dir_info->FileNameLength / sizeof( wchar_t );

		if ( is_dir && character_count == 2 && wcsncmp( file_dir_info->FileName, L"..", 2 ) == 0 )
			continue;

		if ( ( flags & e_scandir_recursive ) && is_dir )
		{
			// sys_scandir_internal( root, relative_path.c_str(), files, flags );
			_recursive_depth_t depth{
				.name     = util_strxndup_r< wchar_t >( nullptr, file_dir_info->FileName, character_count ),
				.name_len = character_count,
				.root     = util_strxndup_r< wchar_t >( nullptr, current_depth.data(), current_depth.size() ),
				.root_len = current_depth.size(),
			};

			recursive_paths.push_front( depth );
			recursive_path_count++;
		}

		if ( ( flags & e_scandir_no_dirs ) && is_dir )
			continue;

		// if ( ( flags & e_scandir_no_files ) && fs_is_file( relative_path.data() ) )
		if ( ( flags & e_scandir_no_files ) && !is_dir )
			continue;

		// https://stackoverflow.com/a/46024468
		constexpr s64 UNIX_TIME_START  = 0x019DB1DED53E8000;  //January 1, 1970 (start of Unix epoch) in "ticks"
		constexpr s64 TICKS_PER_SECOND = 10000000;            //a tick is 100ns

		if ( file_index == files.capacity() )
		{
			// ???
			files.reserve( files.size() + 1024 );
		}

		file_count++;
		//file_t& file      = file_buffers.back()[ file_index++ ];
		file_t& file      = files[ file_index++ ];
		file.date_created = ( file_dir_info->CreationTime.QuadPart - UNIX_TIME_START ) / TICKS_PER_SECOND;
		file.date_mod     = ( file_dir_info->LastWriteTime.QuadPart - UNIX_TIME_START ) / TICKS_PER_SECOND;

		if ( !( flags & e_scandir_no_paths ) )
		{
			size_t path_len = character_count;
			size_t offset   = 0;

			if ( current_depth.size() )
				path_len += current_depth.size() + 1;  // add path sep size

			if ( flags & e_scandir_abs_paths )
			{
				path_len += scan_dir.size();

				memcpy( temp_path_buffer, scan_dir.c_str(), sizeof( wchar_t ) * ( scan_dir.size() ) );
				offset += ( scan_dir.size() );
			}

			if ( current_depth.size() )
			{
				memcpy( temp_path_buffer + offset, current_depth.c_str(), sizeof( wchar_t ) * ( current_depth.size() ) );
				offset += ( current_depth.size() );

				memcpy( temp_path_buffer + offset++, L"\\", sizeof( wchar_t ) * 1 );
			}

			memcpy( temp_path_buffer + offset, file_dir_info->FileName, sizeof( wchar_t ) * character_count );

			file.path.assign( temp_path_buffer, temp_path_buffer + path_len );
		}

		// assign filename
		if ( !( flags & e_scandir_no_filenames ) )
		{
			int name_size = WideCharToMultiByte( CP_UTF8, 0, file_dir_info->FileName, character_count, NULL, 0, NULL, NULL );
			file.name.resize( name_size );
			WideCharToMultiByte( CP_UTF8, 0, file_dir_info->FileName, character_count, file.name.data(), name_size, NULL, NULL );
		}

		if ( is_dir )
		{
			file.type |= e_file_type_directory;
			file.size = 0;
		}
		else
		{
			file.type |= e_file_type_file;
			file.size = file_dir_info->EndOfFile.QuadPart;
		}
	}

	files.resize( file_count );

	// not really needed since this will be freed shortly later anyway
	// files.shrink_to_fit();

	ch_free( e_mem_category_general, buffer );
	return true;
}


bool sys_scandir( const char* root, std::vector< file_t >& files, e_scandir_flags flags, bool* cancel )
{
	wchar_t* root_w     = sys_to_wchar( root );
	u64      start_time = sys_get_time_ms();

	bool     tmp_cancel = false;

	if ( !cancel )
		cancel = &tmp_cancel;

	bool     ret        = sys_scandir_internal( root_w, files, flags, *cancel );

	u64      end_time   = sys_get_time_ms();
	printf( "SCANDIR TIME: %.4f\n", (float)( end_time - start_time ) / 1000.f );

	ch_free_str( root_w );
	return ret;
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


void sys_open_file_properties( const std::vector< fs::path >& files )
{
	if ( files.empty() )
		return;

	if ( files.size() == 1 )
	{
		if ( !SHObjectProperties( 0, SHOP_FILEPATH, files[ 0 ].c_str(), NULL ) )
		{
			wprintf( L"Failed to open File Properties for file: %s\n", files[ 0 ].c_str() );
			sys_print_last_error();
		}
	}
	else
	{
		IDataObject* data_obj = nullptr;
		if ( !sys_get_data_obj_for_files( files, data_obj ) )
		{
			wprintf( L"Failed to open File Properties for files\n" );
			sys_print_last_error();
			return;
		}

		if ( FAILED( SHMultiFileProperties( data_obj, 0 ) ) )
		{
			wprintf( L"Failed to open File Properties for files\n" );
			sys_print_last_error();
			return;
		}
	}
}


bool sys_copy_to_clipboard( const std::vector< fs::path >& files )
{
	if ( files.empty() )
		return false;

	IDataObject* file_obj = nullptr;
	if ( !sys_get_data_obj_for_files( files, file_obj ) )
		return false;

	if ( !SUCCEEDED( OleSetClipboard( file_obj ) ) )
		return false;

	if ( !SUCCEEDED( OleFlushClipboard() ) )
		return false;

	return true;
}


void sys_browse_to_file( const char* path )
{
	wchar_t*         path_w = sys_to_wchar( path );

	PIDLIST_ABSOLUTE pidl   = ILCreateFromPathW( path_w );
	if ( pidl )
	{
		SHOpenFolderAndSelectItems( pidl, 0, 0, 0 );
		ILFree( pidl );
	}

	ch_free_str( path_w );
}


void sys_browse_to_path( const fs::path& path )
{
	if ( path.empty() )
		return;

	PIDLIST_ABSOLUTE root_folder_pidl{};

	// NOTE: windows documentation says this function should be called in a background thread, oops lol
	HRESULT          hr = SHParseDisplayName( path.c_str(), nullptr, &root_folder_pidl, 0, nullptr );

	if ( FAILED( hr ) )
		return;

	SHOpenFolderAndSelectItems( root_folder_pidl, 0, 0, 0 );
	CoTaskMemFree( root_folder_pidl );
}


void sys_browse_to_files( const fs::path& root, const std::vector< fs::path > paths )
{
	if ( paths.empty() )
		return;

	// NOTE: App should only submit ones in the same folder, it does a check before hand

	//std::vector< PIDLIST_ABSOLUTE > absolute_pidls{};
	std::vector< PCUITEMID_CHILD > child_pidls{};

	//absolute_pidls.reserve( paths.size() );
	child_pidls.reserve( paths.size() );

	PIDLIST_ABSOLUTE root_folder_pidl{};

	// NOTE: windows documentation says this function should be called in a background thread, oops lol
	HRESULT          hr = SHParseDisplayName( root.c_str(), nullptr, &root_folder_pidl, 0, nullptr );

	if ( FAILED( hr ) )
		goto end;

	for ( const auto& path : paths )
	{
		PIDLIST_ABSOLUTE pidl = nullptr;
		hr                    = SHParseDisplayName( path.c_str(), nullptr, &pidl, 0, nullptr );

		if ( SUCCEEDED( hr ) && pidl )
		{
			// store this to free it later
			//absolute_pidls.push_back( pidl );

			// same as absolute since desktop is root here
			child_pidls.push_back( reinterpret_cast< PCUITEMID_CHILD >( pidl ) );
		}
		else
		{
			goto end;
		}
	}

	if ( !child_pidls.empty() )
	{
		hr = SHOpenFolderAndSelectItems( root_folder_pidl, static_cast< UINT >( child_pidls.size() ), (PCUITEMID_CHILD_ARRAY)child_pidls.data(), 0 );
	}

end:
	for ( auto pidl : child_pidls )
		CoTaskMemFree( (PIDLIST_ABSOLUTE)pidl );

	// absolute_pidls.clear();
	child_pidls.clear();
	CoTaskMemFree( root_folder_pidl );
}

