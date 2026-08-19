#pragma once

// #include "array.hpp"

#define MEM_TRACK_STACK_TRACE 0

#if MEM_TRACK_STACK_TRACE
  #include <stacktrace>
#endif

#include <type_traits>
#include <unordered_map>
#include <utility>

#if USE_MIMALLOC
  #include <mimalloc.h>
#endif

// --------------------------------------------------------------------------------------------------------


#include "imgui.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <stdlib.h>
#include <string>
#include <vector>

namespace fs
{
	using namespace std::filesystem;

	using path_str  = path::string_type;
	using path_char = path::string_type::value_type;
}

// --------------------------------------------------------------------------------------------------------


using s8     = char;
using s16    = short;
using s32    = int;
using s64    = long long;

using u8     = unsigned char;
using u16    = unsigned short;
using u32    = unsigned int;
using u64    = unsigned long long;

using f32    = float;
using f64    = double;


// --------------------------------------------------------------------------------------------------------


#ifdef _WIN32
  #define SEP_S                     "\\"
  #define SEP                       '\\'

  #define PATH_SEP_STR              "\\"
  #define PATH_SEP                  '\\'

  #define path_printf( str, ... )   wprintf( L##str, __VA_ARGS__ )
  #define path_print( str )         wprintf( L##str )
  #define PATH_FMT( str )           L##str

  #define pathcat( buf, str )       wcscat( buf, str )
  #define pathcat_const( buf, str ) wcscat( buf, L##str )
  #define path_access               _waccess
  #define path_open( path, mode )   _wfopen( path, L##mode )

  #define strncasecmp               _strnicmp
  #define strcasecmp                _stricmp

  #define Win32()                   true
  #define Linux()                   false

namespace fs
{
	using path_view = std::wstring_view;
}

constexpr float STORAGE_SCALE = 1024.f;

#else
  #define SEP_S                     "/"
  #define SEP                       '/'

  #define PATH_SEP_STR              "/"
  #define PATH_SEP                  '/'

  #define path_printf( str, ... )   printf( str, __VA_ARGS__ )
  #define path_print( str )         printf( str )

  #define PATH_FMT( str )     str
  #define fs_path_len         strlen

  #define pathcat( buf, str )       strcat( buf, str )
  #define pathcat_const( buf, str ) strcat( buf, str )
  #define path_access               access
  #define path_open( path, mode )   fopen( path, mode )

  #define Win32()             false
  #define Linux()             true

namespace fs
{
	using path_view = std::string_view;
}

constexpr float STORAGE_SCALE = 1000.f;

#endif

constexpr float MEM_SCALE = 1000.f;

// --------------------------------------------------------------------------------------------------------

#define ARR_SIZE( arr ) ( sizeof( arr ) / sizeof( arr[ 0 ] ) )
#define MIN( a, b )     ( ( ( a ) < ( b ) ) ? ( a ) : ( b ) )
#define MAX( a, b )     ( ( ( a ) > ( b ) ) ? ( a ) : ( b ) )

#define SET_INT2( var, x, y ) \
	( var )[ 0 ] = x;         \
	( var )[ 1 ] = y;


// struct ivec2
// {
// 	int x, y;
// };


using ivec2                       = int[ 2 ];
using vec2                        = float[ 2 ];


// struct vec2
// {
// 	float x, y;
// };


constexpr size_t STR_BUF_SIZE     = 512;
constexpr size_t TIME_BUFFER      = 14;
constexpr size_t DATE_TIME_BUFFER = TIME_BUFFER + 11;


struct str_buf_t
{
	char*  data;
	size_t capacity;
	size_t size;
};


// --------------------------------------------------------------------------------------------------------
// Memory Tracking

enum e_mem_category : u8
{
	e_mem_category_general,

	e_mem_category_thread_data,
	e_mem_category_image_data,
	e_mem_category_image,
	e_mem_category_gl_texture_data,
	e_mem_category_string,
	e_mem_category_file_data,

	e_mem_category_imgui,

	// image formats
	e_mem_category_stbi_resize,
	e_mem_category_jxl,
	e_mem_category_jxl_thumbnail,
	e_mem_category_thumbnail_cache,

	e_mem_category_count,
};


extern const char* mem_category_str[];


struct mem_alloc_info_t
{
	void*            ptr;
	size_t           size;
	u64              app_time;

#if MEM_TRACK_STACK_TRACE
	std::stacktrace* stack_trace;
#endif

	bool             freed;
};

//struct mem_alloc_info_t
//{
//	void*            ptr;
//	size_t           size;
//	std::stacktrace* stack_trace;
//};


struct mem_category_info_t
{
	size_t                                        total;
	std::unordered_map< void*, mem_alloc_info_t > sizes;
	//mem_alloc_info_t*                             alloc;
	//size_t                                        alloc_count;
};


void                 mem_add_item( e_mem_category category, void* memory, size_t bytes, size_t stack_skip = 1, size_t stack_depth = 1 );
void                 mem_free_item( e_mem_category category, void* memory );

void*                imgui_mem_alloc( size_t sz, void* user_data );
void                 imgui_mem_free( void* ptr, void* user_data );

void                 mem_draw_debug_ui();

extern size_t        g_total_memory_allocated;

// extern mem_category_info_t g_mem_categories[ e_mem_category_count ];
mem_category_info_t* get_mem_categories();


// --------------------------------------------------------------------------------------------------------


template< typename T >
// requires std::is_arithmetic_v< T >
T CLAMP( T value, T low, T high )
{
	return ( value < low ) ? low : ( ( value > high ) ? high : value );
}


template< typename T >
inline void ch_free( e_mem_category category, T*& memory )
{
	if ( memory == nullptr )
		return;

	mem_free_item( category, memory );
	free( memory );

	memory = nullptr;
}


// shortcut function
template< typename T >
inline void ch_free_str( T*& memory )
{
	ch_free( e_mem_category_string, memory );
}

#if 0
template< typename T, typename... Args >
requires std::constructible_from< T, Args... >
T* ch_new( e_mem_category category, Args&&... args )
{
	T* data = static_cast< T* >( malloc( sizeof( T ) ) );

	if ( !data )
		return data;

	mem_add_item( category, data, sizeof( T ), 1 );

	*data = T( std::forward< Args >( args )... );
	return data;
}

template< typename T, typename... Args >
requires std::constructible_from< T, Args... >
T* ch_new_multiple( e_mem_category category, size_t count, Args&&... args )
{
	T* data = static_cast< T* >( malloc( count * sizeof( T ) ) );

	if ( !data )
		return data;

	mem_add_item( category, data, count * sizeof( T ), 1 );

	for ( u32 i = 0; i < count; i++ )
	{
		data[ i ] = T( std::forward< Args >( args )... );
	}

	return data;
}
#endif

template< typename T >
T* ch_malloc( size_t count )
{
	T* data = static_cast< T* >( malloc( count * sizeof( T ) ) );

	if ( data == nullptr )
	{
		printf( "malloc failed\n" );
		return nullptr;
	}

	memset( data, 0, count * sizeof( T ) );
	return data;

	// return (T*)malloc( count * sizeof( T ) );
}


template< typename T >
T* ch_calloc( size_t count, e_mem_category category )
{
	T* ptr = static_cast< T* >( calloc( count, sizeof( T ) ) );

	if ( ptr )
		mem_add_item( category, ptr, count * sizeof( T ), 1 );

	return ptr;
}


template< typename T >
T* ch_realloc( T* data, size_t count, e_mem_category category, size_t stack_skip = 1, size_t stack_depth = 1 )
{
	T* ptr = static_cast< T* >( realloc( data, count * sizeof( T ) ) );

	if ( ptr )
	{
		if ( data )
			mem_free_item( category, data );

		mem_add_item( category, ptr, count * sizeof( T ), stack_skip, stack_depth );
	}

	return ptr;
}


template< typename T >
T* ch_recalloc( T* data, size_t count, size_t add_count, e_mem_category category )
{
	T* new_data = static_cast< T* >( realloc( data, ( count + add_count ) * sizeof( T ) ) );

	if ( new_data )
	{
		mem_free_item( category, data );
		mem_add_item( category, new_data, ( count + add_count ) * sizeof( T ), 1 );
		memset( &new_data[ count ], 0, add_count * sizeof( T ) );
	}

	return new_data;
}


// --------------------------------------------------------------------------------------------------------


// removes the element and shifts everything back, and memsets the last item with 0
template< typename T, typename COUNT_TYPE >
void util_array_remove_element( T* data, COUNT_TYPE& count, COUNT_TYPE index )
{
	if ( index >= count )
		return;

	memcpy( &data[ index ], &data[ index + 1 ], sizeof( T ) * ( count - index ) );
	count--;

	if ( count == 0 )
		return;

	memset( &data[ count ], 0, sizeof( T ) );
}


template< typename T >
bool util_array_append( e_mem_category category, T*& data, size_t count )
{
#if 1
	T* new_data = ch_recalloc< T >( data, count, 1, category );

	if ( !new_data )
		return true;

	data = new_data;
#else
	T* new_data = ch_realloc< T >( data, count + 1 );

	if ( !new_data )
		return true;

	data = new_data;
	memset( &data[ count ], 0, sizeof( T ) );
#endif

	return false;
}


template< typename T >
bool util_array_append_err( e_mem_category category, T*& data, u32 count, const char* msg )
{
#if 1
	T* new_data = ch_recalloc< T >( data, count, 1, category );

	if ( !new_data )
	{
		fputs( msg, stdout );
		return true;
	}

	data = new_data;
#else
	T* new_data = ch_realloc< T >( data, count + 1 );

	if ( !new_data )
		return true;

	data = new_data;
	memset( &data[ count ], 0, sizeof( T ) );
#endif

	return false;
}


// Allocates X amount more space in the array
template< typename T >
bool util_array_extend( e_mem_category category, T*& data, size_t count, size_t extend_amount )
{
#if 1
	T* new_data = ch_recalloc< T >( data, count, extend_amount, category );

	if ( !new_data )
		return true;

	data = new_data;
#else
	T* new_data = ch_realloc< T >( data, count + extend_amount, category );

	if ( !new_data )
		return true;

	data = new_data;
	memset( &data[ count ], 0, sizeof( T ) );
#endif

	return false;
}


// --------------------------------------------------------------------------------------------------------
// Helper Functions for std::vector


template< class T >
constexpr size_t vec_index( const std::vector< T >& vec, T item, size_t fallback = SIZE_MAX )
{
	auto it = std::find( vec.begin(), vec.end(), item );
	if ( it != vec.end() )
		return it - vec.begin();

	return fallback;
}


template< class T >
constexpr void vec_remove( std::vector< T >& vec, T item )
{
	vec.erase( vec.begin() + vec_index( vec, item ) );
}


// Remove item if it exists
template< class T >
constexpr void vec_remove_if( std::vector< T >& vec, T item )
{
	size_t index = vec_index( vec, item );
	if ( index != SIZE_MAX )
		vec.erase( vec.begin() + index );
}


template< class T >
constexpr void vec_remove_index( std::vector< T >& vec, size_t index )
{
	vec.erase( vec.begin() + index );
}


template< class T >
constexpr bool vec_contains( const std::vector< T >& vec, T item )
{
	return ( std::find( vec.begin(), vec.end(), item ) != vec.end() );
}


// --------------------------------------------------------------------------------------------------------
// utility functions


bool point_in_rect( ImVec2 point, ImVec2 min_size, ImVec2 max_size );
bool mouse_in_rect( ImVec2 min_size, ImVec2 max_size );
bool mouse_moving();

#if _WIN32
char* strcasestr( const char* s, const char* find );
#endif

char* util_strdup( const char* string );
char* util_strndup( const char* string, size_t len );

// takes in a pointer to realloc to
char* util_strdup_r( char* data, const char* string );
char* util_strndup_r( char* data, const char* string, size_t len );

//bool  util_strncmp( const char* left, const char* right, size_t len );
//bool  util_strncmp( const char* left, size_t left_len, const char* right, size_t right_len );

void  util_append_str( str_buf_t& buffer, const char* str, size_t len );
void  util_append_str( str_buf_t& buffer, const char* str, size_t len, size_t buffer_size );

// kinda lame lol
void  util_format_time( char* buffer, double time );  // expects at least TIME_BUFFER characters in buffer
void  util_format_time( char* buffer, size_t buffer_size, double time );

void  util_format_date_time( char* buffer, size_t buffer_size, u64 time, bool apply_time_zone = true );

bool  util_mouse_hovering_imgui_window();
void  util_imgui_set_tooltip( const char* value );


template< typename CHAR >
CHAR* util_strxndup_r( CHAR* data, const CHAR* string, size_t len )
{
	if ( !string )
		return nullptr;

	if ( len == 0 )
		return nullptr;

	CHAR* new_data = ch_realloc( data, len + 1, e_mem_category_string );

	if ( !new_data )
		return nullptr;

	memcpy( new_data, string, len * sizeof( CHAR ) );
	new_data[ len ] = '\0';
	return new_data;
}


template< typename CHAR >
CHAR* util_strxndup( const CHAR* string, size_t len )
{
	return util_strxndup_r( (CHAR*)nullptr, string, len );
}


template< typename CHAR >
bool util_strncmp( const CHAR* left, const CHAR* right, size_t len )
{
	const CHAR*       cur1 = left;
	const CHAR*       cur2 = right;
	const CHAR* const end  = len + left;

	for ( ; cur1 < end; ++cur1, ++cur2 )
	{
		if ( *cur1 != *cur2 )
			return false;
	}

	return true;
}


template< typename CHAR >
bool util_strncmp( const CHAR* left, size_t left_len, const CHAR* right, size_t right_len )
{
	if ( left_len != right_len )
		return false;

	return util_strncmp( left, right, left_len );
}


template< typename CHAR >
bool util_strcmp( const CHAR* left, const CHAR* right )
{
	if constexpr ( std::is_same_v< wchar_t, CHAR > )
	{
		return wcscmp( left, right ) == 0;
	}
	else
	{
		return strcmp( left, right ) == 0;
	}
}


// --------------------------------------------------------------------------------------------------------
// file system functions

size_t      fs_path_len( const fs::path_char* path );

std::string fs_path_clean( const char* path, size_t path_len );
fs::path    fs_path_clean( const fs::path& path );

char*       fs_get_filename( const char* path );
char*       fs_get_filename_no_ext( const char* path );

char*       fs_get_filename( const char* path, size_t pathLen );
char*       fs_get_filename_no_ext( const char* path, size_t pathLen );

//const char*          fs_get_filename_ptr( std::string_view path );
//const fs::path_char* fs_get_filename_ptr( fs::path_view path );

std::string fs_get_extension( std::string_view path );
// void        fs_get_extension( std::string_view path, std::string& output );

bool        fs_exists( const fs::path_char* path );
bool        fs_make_dir( const fs::path_char* path );
bool        fs_is_dir( const fs::path_char* path );
bool        fs_is_file( const fs::path_char* path );

bool        fs_is_absolute( const char* path, size_t path_len );
bool        fs_is_relative( const char* path, size_t path_len );

// replace all backslash path separators with forward slashes
char*       fs_replace_path_seps_unix( const char* path );

// checks if it exists and if it's a file and not a directory
bool        fs_make_dir_check( const fs::path_char* path );

// returns file size in bytes
u64         fs_file_size( const char* path );

// returns the file length in the len argument, optional
char*       fs_read_file( const fs::path_char* path, size_t* len = nullptr );

// reads a file relative to the app directory
// returns the file length in the len argument, optional
char*       fs_read_file_app_dir( const fs::path_char* path, size_t* len = nullptr );

// ensures no data loss happens and backs up the old file
bool        fs_save_file( const fs::path_char* path, const char* data, size_t size );

//struct save_file_t
//{
//	void* file;
//	char* temp_path;
//	char* bak_path;
//};
//
//save_file_t fs_save_file_open( const char* path );
//void        fs_save_file_close( save_file_t& save, const char* path );

// overrwites any existing file
bool        fs_write_file( const fs::path_char* path, const char* data, size_t size );


template< typename CHAR >
inline const CHAR* fs_get_filename_ptr( CHAR* path, size_t len )
{
	if ( !path || len == 0 )
		return nullptr;

	size_t i = len - 1;
	for ( ; i > 0; i-- )
	{
		if ( ( path[ i ] == '/' || path[ i ] == '\\' ) && i != len - 1 )
			break;
	}

	// No File Extension Found
	if ( i == len )
		return {};

	size_t start_index = i + 1;

	if ( i == 0 )
		start_index = 0;

	if ( start_index == len )
		return {};

	return path + start_index;
}


template< typename CHAR, typename STRING_TYPE, typename STRING_VIEW >
void fs_get_extension( STRING_VIEW path, STRING_TYPE& output )
{
	output.clear();

	if ( path.empty() )
		return;

	const CHAR* dot = nullptr;

	if constexpr ( std::is_same_v< CHAR, wchar_t > )
	{
		dot = wcsrchr( path.data(), L'.' );
	}
	else
	{
		dot = strrchr( path.data(), '.' );
	}

	if ( !dot || dot == path.data() )
		return;

	output.assign( dot, ( path.data() + path.size() ) - dot );
}

