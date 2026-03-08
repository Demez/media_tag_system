#pragma once


#include <unordered_map>
#include <stacktrace>

#if USE_MIMALLOC
#include <mimalloc.h>
#endif

// --------------------------------------------------------------------------------------------------------


#include "imgui.h"

#include <cstdio>
#include <cstring>
#include <stdlib.h>
#include <vector>
#include <string>

#include <filesystem>

namespace fs   = std::filesystem;


// --------------------------------------------------------------------------------------------------------


using s8    = char;
using s16   = short;
using s32   = int;
using s64   = long long;

using u8    = unsigned char;
using u16   = unsigned short;
using u32   = unsigned int;
using u64   = unsigned long long;

using f32   = float;
using f64   = double;


// --------------------------------------------------------------------------------------------------------


#ifdef _WIN32
  #define SEP_S "\\"
  #define SEP '\\'

  #define PATH_SEP_STR "\\"
  #define PATH_SEP     '\\'

  #define strncasecmp _strnicmp
  #define strcasecmp  _stricmp
#else
  #define SEP_S        "/"
  #define SEP          '/'

  #define PATH_SEP_STR "/"
  #define PATH_SEP     '/'
#endif


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


using ivec2 = int[ 2 ];
using vec2  = float[ 2 ];


// struct vec2
// {
// 	float x, y;
// };


constexpr size_t STR_BUF_SIZE = 512;
constexpr size_t TIME_BUFFER  = 14;


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

	e_mem_category_image_data,
	e_mem_category_image,
	e_mem_category_string,
	e_mem_category_file_data,

	e_mem_category_imgui,
	e_mem_category_stbi_resize,

	e_mem_category_count,
};


extern const char* mem_category_str[];


struct mem_alloc_info_t
{
	void*            ptr;
	size_t           size;
	double           app_time;
	std::stacktrace* stack_trace;
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


void                 mem_add_item( e_mem_category category, void* memory, size_t bytes );
void                 mem_free_item( e_mem_category category, void* memory );

void*                imgui_mem_alloc( size_t sz, void* user_data );
void                 imgui_mem_free( void* ptr, void* user_data );

void                 mem_draw_debug_ui();

extern size_t        g_total_memory_allocated;

// extern mem_category_info_t g_mem_categories[ e_mem_category_count ];
mem_category_info_t* get_mem_categories();



// --------------------------------------------------------------------------------------------------------


template< typename T >
T CLAMP( T value, T low, T high )
{
	return ( value < low ) ? low : ( ( value > high ) ? high : value );
}


inline void ch_free( e_mem_category category, void* memory )
{
	if ( memory == nullptr )
		return;

	mem_free_item( category, memory );
	free( memory );
}


// shortcut function
inline void ch_free_str( void* memory )
{
	ch_free( e_mem_category_string, memory );
}


template< typename T >
T* ch_malloc( size_t count )
{
	T* data = (T*)malloc( count * sizeof( T ) );

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
	T* ptr = (T*)calloc( count, sizeof( T ) );

	if ( ptr )
		mem_add_item( category, ptr, count * sizeof( T ) );

	return ptr;
}


template< typename T >
T* ch_realloc( T* data, size_t count, e_mem_category category )
{
	T* ptr = (T*)realloc( data, count * sizeof( T ) );

	if ( ptr )
	{
		if ( data )
			mem_free_item( category, data );

		mem_add_item( category, ptr, count * sizeof( T ) );
	}

	return ptr;
}


template< typename T >
T* ch_recalloc( T* data, size_t count, size_t add_count, e_mem_category category )
{
	T* new_data = (T*)realloc( data, (count + add_count) * sizeof( T ) );

	if ( new_data )
	{
		mem_free_item( category, data );
		mem_add_item( category, new_data, ( count + add_count ) * sizeof( T ) );
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
// utility functions


bool        point_in_rect( ImVec2 point, ImVec2 min_size, ImVec2 max_size );
bool        mouse_in_rect( ImVec2 min_size, ImVec2 max_size );

#if _WIN32
char*       strcasestr( const char* s, const char* find );
#endif

char*       util_strdup( const char* string );
char*       util_strndup( const char* string, size_t len );

// takes in a pointer to realloc to
char*       util_strdup_r( char* data, const char* string );
char*       util_strndup_r( char* data, const char* string, size_t len );

bool        util_strncmp( const char* left, const char* right, size_t len );
bool        util_strncmp( const char* left, size_t left_len, const char* right, size_t right_len );

void        util_append_str( str_buf_t& buffer, const char* str, size_t len );
void        util_append_str( str_buf_t& buffer, const char* str, size_t len, size_t buffer_size );

// kinda lame lol
void        util_format_time( char* buffer, double time );  // expects at least TIME_BUFFER characters in buffer
void        util_format_time( char* buffer, size_t buffer_size, double time );


// --------------------------------------------------------------------------------------------------------
// file system functions


char*       fs_get_filename( const char* path );
char*       fs_get_filename_no_ext( const char* path );

char*       fs_get_filename( const char* path, size_t pathLen );
char*       fs_get_filename_no_ext( const char* path, size_t pathLen );

bool        fs_exists( const char* path );
bool        fs_make_dir( const char* path );
bool        fs_is_dir( const char* path );
bool        fs_is_file( const char* path );

// replace all backslash path separators with forward slashes
char*       fs_replace_path_seps_unix( const char* path );

// checks if it exists and if it's a file and not a directory
bool        fs_make_dir_check( const char* path );

// returns file size in bytes
u64         fs_file_size( const char* path );

// returns the file length in the len argument, optional
char*       fs_read_file( const char* path, size_t* len = nullptr );

// ensures no data loss happens and backs up the old file
bool        fs_save_file( const char* path, const char* data, size_t size );

// overrwites any existing file
bool        fs_write_file( const char* path, const char* data, size_t size );



