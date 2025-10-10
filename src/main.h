#pragma once

#include "args.h"
#include "util.h"

#include <filesystem>

namespace fs = std::filesystem;

#include <cstdio>
#include <vector>

#include "imgui.h"
#include "SDL3/SDL.h"


// we don't use a general u64, since this can be aligned by 4 bytes, and doesn't result in handle mismatches
#define HANDLE_GEN_32( name )                                      \
	struct name                                                    \
	{                                                              \
		u32  index;                                                \
		u32  generation;                                           \
                                                                   \
		bool operator!()                                           \
		{                                                          \
			return generation == 0;                                \
		}                                                          \
		operator bool() const                                      \
		{                                                          \
			return generation > 0;                                 \
		}                                                          \
	};                                                             \
                                                                   \
	namespace std                                                  \
	{                                                              \
	template<>                                                     \
	struct hash< name >                                            \
	{                                                              \
		size_t operator()( name const& handle ) const              \
		{                                                          \
			size_t value = ( hash< u32 >()( handle.index ) );      \
			value ^= ( hash< u32 >()( handle.generation ) );       \
			return value;                                          \
		}                                                          \
	};                                                             \
	}                                                              \
                                                                   \
	inline bool operator==( const name& a, const name& b )         \
	{                                                              \
		return a.index == b.index && a.generation == b.generation; \
	}


// validate a handle
template< typename HANDLE >
bool handle_list_valid( u32 capacity, u32* generation, HANDLE handle )
{
	if ( handle.index >= capacity )
		return false;

	return handle.generation != 0 && handle.generation == generation[ handle.index ];
}


// 32-bit handle list with generation support and ref counts
// TODO: should i have another layer of indirection with an index list so we can defragment the memory and reduce how much memory this takes up?
template< typename HANDLE, typename TYPE, u32 STEP_SIZE = 32 >
struct handle_list_32
{
	//u32   count;
	u32   capacity;
	TYPE* data;
	u32*  generation;
	bool* use_list;  // list of entries that are in use

	handle_list_32()
	{
	}

	~handle_list_32()
	{
		::free( data );
		::free( generation );
		::free( use_list );
	}

	bool allocate()
	{
		if ( util_array_extend( generation, capacity, STEP_SIZE ) )
		{
			::free( generation );
			return false;
		}

		if ( util_array_extend( data, capacity, STEP_SIZE ) )
		{
			::free( data );
			return false;
		}

		if ( util_array_extend( use_list, capacity, STEP_SIZE ) )
		{
			::free( use_list );
			return false;
		}

		capacity += STEP_SIZE;

		return true;
	}
	
	bool handle_valid( HANDLE s_handle )
	{
		return handle_list_valid( capacity, generation, s_handle );
	}

	bool create( HANDLE& s_handle, TYPE** s_type )
	{
		// Find a free handle
		u32 index = 0;
		for ( ; index < capacity; index++ )
		{
			// is this handle in use?
			if ( !use_list[ index ] )
				break;
		}

		if ( index == capacity && !allocate() )
			return false;

		use_list[ index ]   = true;

		s_handle.index      = index;
		s_handle.generation = ++generation[ index ];

		if ( s_type )
			*s_type = &data[ index ];

		return true;
	}

	void free( u32 index )
	{
		memset( &data[ index ], 0, sizeof( TYPE ) );
		use_list[ index ] = false;
	}

	void free( HANDLE& s_handle )
	{
		if ( !handle_valid( s_handle ) )
			return;

		memset( &data[ s_handle.index ], 0, sizeof( TYPE ) );
		use_list[ s_handle.index ] = false;
	}

	// use an existing handle, potentially useful for loading saves
	// though the generation index would be annoying
	// create_with_handle

	TYPE* get( HANDLE s_handle )
	{
		if ( !handle_valid( s_handle ) )
			return nullptr;

		return &data[ s_handle.index ];
	}
};


// 32-bit handle list with generation support
// TODO: should i have another layer of indirection with an index list so we can defragment the memory and reduce how much memory this takes up?
template< typename HANDLE, u32 STEP_SIZE = 32 >
struct handle_list_simple_32
{
	//u32   count;
	u32   capacity;
	u32*  generation;
	bool* use_list;  // list of entries that are in use

	handle_list_simple_32()
	{
	}

	~handle_list_simple_32()
	{
		free( generation );
		free( use_list );
	}

  private:
	bool allocate()
	{
		u32*  new_generation = util_array_extend( generation, capacity, STEP_SIZE );
		bool* new_use        = util_array_extend( use_list, capacity, STEP_SIZE );

		if ( !new_generation || !new_use )
		{
			free( new_generation );
			free( new_use );
			return false;
		}

		generation = new_generation;
		use_list   = new_use;

		return true;
	}

  public:
	bool handle_valid( HANDLE s_handle )
	{
		return handle_list_valid( capacity, generation, s_handle );
	}

	HANDLE create()
	{
		// Find a free handle
		u32 index = 0;
		for ( ; index < capacity; index++ )
		{
			// is this handle in use?
			if ( !use_list[ index ] )
				break;
		}

		if ( index == capacity && !allocate() )
			return {};

		use_list[ index ] = true;

		HANDLE handle;
		handle.index      = index;
		handle.generation = ++generation[ index ];

		return handle;
	}

	void free( u32 index )
	{
		use_list[ index ] = false;
	}
};


// ---------------------------------------------------------
// codec handler


enum PixelFormat
{
	FMT_NONE,

	FMT_RGB8,
	FMT_RGBA8,

	FMT_BGR8,
	FMT_BGRA8,

	COUNT,
};


struct image_t
{
	int            width;
	int            height;
	int            bit_depth;
	int            pitch;
	PixelFormat    format = FMT_NONE;
	unsigned char* data;
};


class ICodec
{
  public:
	virtual bool     check_extension( const char* ext )                                                              = 0;
	virtual bool     check_header( const fs::path& path )                                                            = 0;

	// Load the smallest version of an image that's larger than the inputted size
	virtual bool     image_load_scaled( const fs::path& path, image_t* image_info, int area_width, int area_height ) = 0;

	virtual bool     image_load( const fs::path& path, image_t* old_data )                                           = 0;
	virtual image_t* image_load( const fs::path& path )                                                              = 0;
};


void          register_codec( ICodec* codec );


// ---------------------------------------------------------
// thumbnail system


enum e_thumbnail_status
{
	// This is not a valid thumbnail at all, but is a free slot for a thumbnail
	// The thumbnail can also go to this state if it's automatically freed
	e_thumbnail_status_free,

	// Waiting for processing
	e_thumbnail_status_queued,

	// Thumbnail is loading from disk
	e_thumbnail_status_loading,

	// Thumbnail is uploading to the GPU
	e_thumbnail_status_uploading,

	// Thumbnail is uploaded and ready for use
	e_thumbnail_status_finished,

	// Failed to load thumbnail
	e_thumbnail_status_failed,

	// This thumbnail is no longer valid, it has been freed to make room for other thumbnails
	// (is this useful at all? just use invalid instead)
	// e_thumbnail_status_freed,
};


HANDLE_GEN_32( h_thumbnail );


struct thumbnail_t
{
	e_thumbnail_status status;
	u32                distance;  // higher distances get freed first for other thumbnails
	char*              path;  // mainly for debugging
	image_t*           data;
	SDL_Surface*       sdl_surface;
	SDL_Texture*       sdl_texture;
	ImTextureRef       im_texture;
};


bool          thumbnail_loader_init();
void          thumbnail_loader_shutdown();
void          thumbnail_loader_update();
void          thumbnail_loader_update_after_render();

h_thumbnail   thumbnail_queue_image( const fs::path& path );
thumbnail_t*  thumbnail_get_data( h_thumbnail handle );
void          thumbnail_free( const fs::path& path, h_thumbnail handle );

// distance based cache
void          thumbnail_update_distance( h_thumbnail handle, u32 distance );
// void          thumbnail_update_region( ImVec2 scroll_area_size, float scroll_amount );

void          thumbnail_cache_debug_draw();


// ---------------------------------------------------------

constexpr u32 DATABASE_VERSION = 1;

using db_handle_t              = u32;
using media_handle_t           = u32;

// temporary structures, these are horrible for searching through
// only just for laying out what i want


struct tag_descriptor_t
{
	// strings encoded in utf-8
	char*          name;
	char*          description;
};


struct media_descriptor_t
{
	media_handle_t handle;

	// strings encoded in utf-8
	char*          name;
	char*          path;

	// description
	char*          description;

	db_handle_t*   tags;
	u32            tags_count;

	// authors
	char*          authors;
	u32            authors_count;

	// dates
	// urls
};

// ---------------------------------------------------------


// Media Tag Database System

bool load_database();
bool save_database();
void run_database();


// testing ideas

// Copies metadata from an array of items to another array of items
void db_copy_data( db_handle_t db, s32 count, media_handle_t* input, media_handle_t* output );


