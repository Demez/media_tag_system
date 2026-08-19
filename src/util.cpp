#include "main.h"
#include "util.h"

#include "imgui.h"
#include "imgui_internal.h"

#include <sys/stat.h>
#include <time.h>
#include <math.h>
#include <ctype.h>
#include <locale>


bool point_in_rect( ImVec2 point, ImVec2 min_size, ImVec2 max_size )
{
	return point[ 0 ] >= min_size[ 0 ] && point[ 0 ] <= max_size[ 0 ] && point[ 1 ] <= max_size[ 1 ] && point[ 1 ] >= min_size[ 1 ];
}


bool mouse_in_rect( ImVec2 min_size, ImVec2 max_size )
{
	if ( app::mouse_in_window )
		return point_in_rect( ImVec2( app::mouse_pos[ 0 ], app::mouse_pos[ 1 ] ), min_size, max_size );

	return false;
}


bool mouse_moving()
{
	return app::mouse_delta[ 0 ] != 0 || app::mouse_delta[ 1 ] != 0;
}


#ifdef _WIN32
// Find the first occurrence of find in s while ignoring case
char* strcasestr( const char* s, const char* find )
{
	char c, sc;

	if ( ( c = *find++ ) == 0 )
		return ( (char*)s );

	// convert to lower case character
	c          = tolower( static_cast< int >( c ) );
	size_t len = strlen( find );
	do
	{
		// compare lower case character
		do
		{
			if ( ( sc = *s++ ) == 0 )
				return nullptr;

		} while ( (char)tolower( (unsigned char)sc ) != c );
	} while ( _strnicmp( s, find, len ) != 0 );
	s--;

	return ( (char*)s );
}
#endif


char* util_strdup( const char* string )
{
	return util_strdup_r( nullptr, string );
}


char* util_strndup( const char* string, size_t len )
{
	return util_strndup_r( nullptr, string, len );
}


char* util_strdup_r( char* data, const char* string )
{
	if ( !string )
		return nullptr;

	size_t len = strlen( string );

	if ( len == 0 )
		return nullptr;

	char* new_data = ch_realloc( data, len + 1, e_mem_category_string );

	if ( !new_data )
		return nullptr;

	memcpy( new_data, string, len * sizeof( char ) );
	new_data[ len ] = '\0';
	return new_data;
}


char* util_strndup_r( char* data, const char* string, size_t len )
{
	if ( !string )
		return nullptr;

	if ( len == 0 )
		return nullptr;

	char* new_data = ch_realloc( data, len + 1, e_mem_category_string );

	if ( !new_data )
		return nullptr;

	memcpy( new_data, string, len * sizeof( char ) );
	new_data[ len ] = '\0';
	return new_data;
}


#if 0
bool util_strncmp( const char* left, const char* right, size_t len )
{
	const char*       cur1 = left;
	const char*       cur2 = right;
	const char* const end  = len + left;

	for ( ; cur1 < end; ++cur1, ++cur2 )
	{
		if ( *cur1 != *cur2 )
			return false;
	}

	return true;
}


bool util_strncmp( const char* left, size_t left_len, const char* right, size_t right_len )
{
	if ( left_len != right_len )
		return false;

	return util_strncmp( left, right, left_len );
}
#endif


void util_append_str( str_buf_t& buffer, const char* str, size_t len, size_t buffer_size )
{
	if ( ( len + buffer.size ) > buffer.capacity )
	{
		size_t increase = MAX( len, buffer_size );
		char*  new_data = ch_realloc( buffer.data, buffer.capacity + increase, e_mem_category_string );

		if ( !new_data )
		{
			printf( "util_append_str: failed to increase string buffer size!\n" );
			return;
		}

		buffer.capacity += increase;
		buffer.data = new_data;
	}

	memcpy( &buffer.data[ buffer.size ], str, len * sizeof( char ) );
	buffer.size += len;
}


void util_append_str( str_buf_t& buffer, const char* str, size_t len )
{
	util_append_str( buffer, str, len, STR_BUF_SIZE );
}


void util_format_time( char* buffer, size_t buffer_size, double time )
{
	if ( buffer_size < 9 )
		return;

	time_t     time_time_pos = (time_t)time;

	struct tm* tm_info;

	tm_info = gmtime( &time_time_pos );
	strftime( buffer, 9, "%H:%M:%S", tm_info );

	if ( buffer_size == 9 )
		return;

	// add miliseconds
	snprintf( buffer + 8, buffer_size - 8, "%.8f", fmod( time, 1 ) );

	// move it back to get rid of the 0 lol
	memcpy( buffer + 8, buffer + 9, buffer_size - 9 );
	buffer[ buffer_size - 1 ] = '0';
}


void util_format_time( char* buffer, double time )
{
	return util_format_time( buffer, TIME_BUFFER, time );
}


// TODO: This should use system locale for formatting time
void util_format_date_time( char* buffer, size_t buffer_size, u64 time, bool apply_time_zone )
{
	if ( !buffer )
		return;

	time_t     time_pos = (time_t)time;
	struct tm* tm_info{};

	if ( apply_time_zone )
		tm_info = localtime( &time_pos );
	else
		tm_info = gmtime( &time_pos );

	if ( !tm_info )
	{
		memset( buffer, '\0', buffer_size );
		return;
	}

	// YYYY-MM-DD HH:MM:SS
	strftime( buffer, buffer_size, "%Y-%m-%d %H:%M:%S", tm_info );

	if ( buffer_size <= 19 )
		return;

	// add miliseconds
	snprintf( buffer + 19, buffer_size - 19, "%.8f", fmod( time, 1 ) );

	// move it back to get rid of the 0 lol
	memcpy( buffer + 19, buffer + 20, buffer_size - 20 );
	buffer[ buffer_size - 1 ] = '0';
}

