#pragma once

#include "mpv_interface.h"
#include "args.h"
#include "util.h"
#include "handles.h"

#include "imgui.h"
#include "glad/glad.h"
#include "SDL3/SDL.h"

#include <cstdio>
#include <vector>
#include <atomic>

#include <filesystem>

namespace fs = std::filesystem;


extern SDL_Window*   g_main_window;
// extern SDL_Renderer* g_main_renderer;


HANDLE_GEN_32( h_thumbnail );


// ---------------------------------------------------------
// codec handler


struct image_t
{
	int             width;
	int             height;
	int             bit_depth;
	int             pitch;
	GLint           format;
	unsigned char*  data;
};


struct image_load_info_t
{
	// Image data, this will be reused if valid data, result is also stored in here
	image_t* image;

	// When not 0, The codec will load the smallest version of an image that's larger than this resolution
	ImVec2   target_size;

	// leads to a lower quality image if the codec has options for this, otherwise load it in max quality
	bool     load_quick;

	// Is this being loaded from a thread?
	bool     threaded_load;
};


struct IImageLoader
{
	virtual bool     check_extension( std::string_view ext )                                                       = 0;
	virtual bool     check_header( const fs::path& path )                                                          = 0;

	// Load the smallest version of an image that's larger than the inputted size
	//virtual bool     image_load_scaled( const fs::path& path, image_t* image_info, int area_width, int area_height ) = 0;

	virtual bool     image_load( const fs::path& path, image_load_info_t& load_info, char* data, size_t data_len ) = 0;
	//virtual image_t* image_load( const fs::path& path )                                                              = 0;
};


void image_register_codec( IImageLoader* codec );
bool image_load( const fs::path& path, image_load_info_t& load_info );


// TODO: add image load functions here
// - add animated image support
// - add color profile support (PAIN)
// - split it into reading the file first, passing it into each codec to check the header, if valid, load the rest of the image


// ---------------------------------------------------------


enum e_media_type
{
	e_media_type_none,
	e_media_type_directory,
	e_media_type_image,
	e_media_type_image_animated,
	e_media_type_video,

	e_media_type_count,
};


enum e_icon : u8
{
	e_icon_none,
	e_icon_invalid,
	e_icon_folder,
	e_icon_loading,
	e_icon_video,

	e_icon_count,
};


enum e_zoom_mode
{
	e_zoom_mode_fit,
	e_zoom_mode_fit_width,
	e_zoom_mode_fixed,
};


struct media_entry_t
{
	fs::path     path;
	std::string  filename;
	e_media_type type;
};


struct main_image_data_t
{
	// TODO: add multiple frames here
	GLuint texture;
};


// imgui scroll hack lol
extern bool                         g_mouse_scrolled_up;
extern bool                         g_mouse_scrolled_down;
extern bool                         g_window_resized;

extern ivec2                        g_mouse_delta;
extern ivec2                        g_mouse_pos;

extern fs::path                     g_folder;
extern fs::path                     g_folder_queued;  // will change to this folder start of next frame
extern std::vector< media_entry_t > g_folder_media_list;
extern std::vector< h_thumbnail >   g_folder_thumbnail_list;
extern size_t                       g_gallery_index;


// Main Image
extern e_zoom_mode                  g_image_zoom_mode;
extern image_t                      g_image;
extern main_image_data_t            g_image_data;
extern size_t                       g_media_index;

// Previous Image to Free
extern main_image_data_t            g_image_data_free;


void                                media_view_load();
void                                media_view_input();
void                                media_view_draw_imgui();
void                                media_view_draw();
void                                media_view_scroll_zoom( float amount );
void                                media_view_advance( bool prev = false );
void                                media_view_window_resize();

void                                gallery_view_scroll_to_selected();
void                                gallery_view_input();
void                                gallery_view_draw();
void                                gallery_view_dir_change();

void                                gallery_view_toggle();

void                                update_window_title();
void                                folder_load_media_list();

bool                                icon_preload();
void                                icon_free();
image_t*                            icon_get_image( e_icon icon_type );
ImTextureRef                        icon_get_imtexture( e_icon icon_type );

GLuint                              gl_upload_texture( image_t* image );
void                                gl_update_texture( GLuint texture, image_t* image );
void                                gl_free_texture( GLuint texture );

bool                                mouse_hovering_imgui_window();


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
};


struct thumbnail_t
{
	std::atomic< e_thumbnail_status > status;
	u32                               distance;  // higher distances get freed first for other thumbnails
	char*                             path;      // mainly for debugging
	image_t*                          data;
	GLuint                            texture;
	ImTextureRef                      im_texture;
};


bool          thumbnail_loader_init();
void          thumbnail_loader_shutdown();
void          thumbnail_loader_update();

h_thumbnail   thumbnail_queue_image( const fs::path& path );
thumbnail_t*  thumbnail_get_data( h_thumbnail handle );
// void          thumbnail_free( const fs::path& path, h_thumbnail handle );

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


