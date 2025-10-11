#pragma once

#include "args.h"
#include "util.h"
#include "handles.h"

#include <filesystem>

namespace fs = std::filesystem;

#include <cstdio>
#include <vector>

#include "imgui.h"
#include "SDL3/SDL.h"


extern SDL_Window* g_main_window;


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


void register_codec( ICodec* codec );


// ---------------------------------------------------------
// Gallery View


void gallery_draw();


// ---------------------------------------------------------
// Thumbnail System


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


