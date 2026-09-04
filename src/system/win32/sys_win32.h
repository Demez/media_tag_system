#pragma once

#include "util.h"
#include "system/system.h"

// glad.h defines this
#ifdef APIENTRY
  #undef APIENTRY
#endif

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
#include <time.h>
#include <atlbase.h>
#include <psapi.h>
#include <dwmapi.h>
#include <strsafe.h>
#include <sys/stat.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

// extended path limit in windows
// TODO: use the manifest if possible as well to enable long path support
#define MAX_PATH_EXT 32767

extern HWND    g_main_hwnd;


// Internal functions only
const wchar_t* sys_get_error_w();

// --------------------------------------------------------------------------------------------------------
// string conversion functions
// also known as "sys_to_utf16"
wchar_t*       sys_to_wchar( const char* spStr, size_t sSize );
wchar_t*       sys_to_wchar( const char* spStr );

// prepends "\\?\" on the string for windows
// https://learn.microsoft.com/en-us/windows/win32/fileio/naming-a-file
wchar_t*       sys_to_wchar_extended( const char* spStr, size_t sSize );
wchar_t*       sys_to_wchar_extended( const char* spStr );

char*          sys_to_utf8( const wchar_t* spStr, size_t sSize );
char*          sys_to_utf8( const wchar_t* spStr );

wchar_t*       create_extended_path( const wchar_t* spStr, size_t strLen );


// --------------------------------------------------------------------------------------------------------
// Internal Drag and Drop functions

bool           drag_drop_register( HWND hwnd );
void           drag_drop_remove( HWND hwnd );

// --------------------------------------------------------------------------------------------------------
// Shell Helpers

HRESULT        GetUIObjectOfFile( HWND hwnd, LPCWSTR pszPath, REFIID riid, void** ppv );
bool           sys_get_data_obj_for_files( const std::vector< fs::path >& paths, IDataObject*& file_obj );

