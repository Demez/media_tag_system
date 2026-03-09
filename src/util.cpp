#include "main.h"
#include "util.h"

#include "imgui.h"
#include "imgui_internal.h"

#include <sys/stat.h>
#include <time.h>
#include <math.h>
#include <ctype.h>
#include <locale>


// Check the function FindHoveredWindowEx() in imgui.cpp to see if you need to update this when updating imgui
bool util_mouse_hovering_imgui_window()
{
	ImGuiContext& g = *ImGui::GetCurrentContext();

	ImVec2        imMousePos{ (float)app::mouse_pos[ 0 ], (float)app::mouse_pos[ 1 ] };

	ImGuiWindow*  hovered_window                     = NULL;
	ImGuiWindow*  hovered_window_under_moving_window = NULL;

	if ( g.MovingWindow && !( g.MovingWindow->Flags & ImGuiWindowFlags_NoMouseInputs ) )
		hovered_window = g.MovingWindow;

	ImVec2 padding_regular    = g.Style.TouchExtraPadding;
	ImVec2 padding_for_resize = ImMax( g.Style.TouchExtraPadding, ImVec2( g.Style.WindowBorderHoverPadding, g.Style.WindowBorderHoverPadding ) );
	for ( int i = g.Windows.Size - 1; i >= 0; i-- )
	{
		ImGuiWindow* window = g.Windows[ i ];
		IM_MSVC_WARNING_SUPPRESS( 28182 );  // [Static Analyzer] Dereferencing NULL pointer.
		if ( !window->WasActive || window->Hidden )
			continue;
		if ( window->Flags & ImGuiWindowFlags_NoMouseInputs )
			continue;

		// Using the clipped AABB, a child window will typically be clipped by its parent (not always)
		ImVec2 hit_padding = ( window->Flags & ( ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize ) ) ? padding_regular : padding_for_resize;
		if ( !window->OuterRectClipped.ContainsWithPad( imMousePos, hit_padding ) )
			continue;

		// Support for one rectangular hole in any given window
		// FIXME: Consider generalizing hit-testing override (with more generic frame, callback, etc.) (#1512)
		if ( window->HitTestHoleSize.x != 0 )
		{
			ImVec2 hole_pos( window->Pos.x + (float)window->HitTestHoleOffset.x, window->Pos.y + (float)window->HitTestHoleOffset.y );
			ImVec2 hole_size( (float)window->HitTestHoleSize.x, (float)window->HitTestHoleSize.y );
			if ( ImRect( hole_pos, hole_pos + hole_size ).Contains( imMousePos ) )
				continue;
		}

		//if ( find_first_and_in_any_viewport )
		//{
		//	hovered_window = window;
		//	break;
		//}
		//else
		{
			if ( hovered_window == NULL )
				hovered_window = window;
			IM_MSVC_WARNING_SUPPRESS( 28182 );  // [Static Analyzer] Dereferencing NULL pointer.
			if ( hovered_window_under_moving_window == NULL && ( !g.MovingWindow || window->RootWindow != g.MovingWindow->RootWindow ) )
				hovered_window_under_moving_window = window;
			if ( hovered_window && hovered_window_under_moving_window )
				break;
		}
	}

	return hovered_window;
}


bool point_in_rect( ImVec2 point, ImVec2 min_size, ImVec2 max_size )
{
	return point[ 0 ] >= min_size[ 0 ] && point[ 0 ] <= max_size[ 0 ] && point[ 1 ] <= max_size[ 1 ] && point[ 1 ] >= min_size[ 1 ];
}


bool mouse_in_rect( ImVec2 min_size, ImVec2 max_size )
{
	return point_in_rect( ImVec2( app::mouse_pos[ 0 ], app::mouse_pos[ 1 ] ), min_size, max_size );
}


#ifdef _WIN32
// Find the first occurrence of find in s while ignoring case
char* strcasestr( const char* s, const char* find )
{
	char c, sc;

	if ( ( c = *find++ ) == 0 )
		return ( (char*)s );

	// convert to lower case character
	c          = tolower( (unsigned char)c );
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

