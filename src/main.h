#pragma once

#include "args.h"
#include "util.h"

#include <filesystem>

namespace fs = std::filesystem;

#include <cstdio>
#include <vector>

#include "SDL3/SDL.h"

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


struct image_info_t
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
	virtual bool          check_extension( const char* ext )   = 0;
	virtual bool          check_header( const fs::path& path ) = 0;

	virtual bool          image_load( const fs::path& path, image_info_t* old_data ) = 0;
	virtual image_info_t* image_load( const fs::path& path )   = 0;
};


void          register_codec( ICodec* codec );



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


