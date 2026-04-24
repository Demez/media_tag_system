#pragma once

#include "util.h"
#include "system/system.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>


extern HWND    g_main_hwnd;


// Internal functions only
const wchar_t* sys_get_error_w();

// --------------------------------------------------------------------------------------------------------
// string conversion functions
// also known as "sys_to_utf16"
wchar_t*       sys_to_wchar( const char* spStr, int sSize );
wchar_t*       sys_to_wchar( const char* spStr );

// prepends "\\?\" on the string for windows
// https://learn.microsoft.com/en-us/windows/win32/fileio/naming-a-file
wchar_t*       sys_to_wchar_extended( const char* spStr, int sSize );
wchar_t*       sys_to_wchar_extended( const char* spStr );

char*          sys_to_utf8( const wchar_t* spStr, int sSize );
char*          sys_to_utf8( const wchar_t* spStr );


// --------------------------------------------------------------------------------------------------------
// Internal Drag and Drop functions

bool           drag_drop_register( HWND hwnd );
void           drag_drop_remove( HWND hwnd );


// --------------------------------------------------------------------------------------------------------

HRESULT        GetUIObjectOfFile( HWND hwnd, LPCWSTR pszPath, REFIID riid, void** ppv );

