#include "main.h"
#include "sys_win32.h"


// --------------------------------------------------------------------------------------------------------
// Internal String conversion functions


wchar_t* sys_to_wchar( const char* spStr, int sSize )
{
	wchar_t* spDst = (wchar_t*)malloc( ( sSize + 1 ) * sizeof( wchar_t ) );
	
	if ( !spDst )
		return nullptr;

	memset( spDst, 0, ( sSize + 1 ) * sizeof( wchar_t ) );

	MultiByteToWideChar( CP_UTF8, 0, spStr, -1, spDst, sSize );

	mem_add_item( e_mem_category_string, spDst, ( sSize + 1 ) * sizeof( wchar_t ), 1 );

	return spDst;
}


wchar_t* sys_to_wchar( const char* spStr )
{
	int size = MultiByteToWideChar( CP_UTF8, 0, spStr, -1, NULL, 0 );

	if ( size )
		return sys_to_wchar( spStr, size );

	return nullptr;
}


// Allows you to bypass the MAX_PATH limit in windows
wchar_t* sys_to_wchar_extended( const char* spStr, int sSize )
{
	wchar_t* spDst = (wchar_t*)malloc( ( sSize + 5 ) * sizeof( wchar_t ) );

	if ( !spDst )
		return nullptr;

	memset( spDst, 0, ( sSize + 1 ) * sizeof( wchar_t ) );
	wcscat( spDst, L"\\\\?\\" );

	MultiByteToWideChar( CP_UTF8, 0, spStr, -1, spDst + 4, sSize );

	mem_add_item( e_mem_category_string, spDst, ( sSize + 5 ) * sizeof( wchar_t ), 1 );

	return spDst;
}


// Allows you to bypass the MAX_PATH limit in windows
wchar_t* sys_to_wchar_extended( const char* spStr )
{
	int size = MultiByteToWideChar( CP_UTF8, 0, spStr, -1, NULL, 0 );

	if ( size )
		return sys_to_wchar_extended( spStr, size );

	return nullptr;
}


char* sys_to_utf8( const wchar_t* spStr, int sSize )
{
	char* spDst = (char*)malloc( ( sSize + 1 ) * sizeof( char ) );

	if ( !spDst )
		return nullptr;

	memset( spDst, 0, ( sSize + 1 ) * sizeof( char ) );

	WideCharToMultiByte( CP_UTF8, 0, spStr, -1, spDst, sSize, NULL, NULL );

	mem_add_item( e_mem_category_string, spDst, ( sSize + 1 ) * sizeof( char ), 1 );

	return spDst;
}


char* sys_to_utf8( const wchar_t* spStr )
{
	int size = WideCharToMultiByte( CP_UTF8, 0, spStr, -1, NULL, 0, NULL, NULL );

	if ( size )
		return sys_to_utf8( spStr, size );

	return nullptr;
}
