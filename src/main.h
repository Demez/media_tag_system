#pragma once

#include "util.h"

#include "mpv_interface.h"
#include "args.h"
#include "handles.h"
#include "system/system.h"

#include "imgui.h"
#include "glad/glad.h"
#include "SDL3/SDL.h"

#include <cstdio>
#include <vector>
#include <atomic>


HANDLE_GEN_32( h_thumbnail );


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
	e_zoom_mode_fit,        // image is as large as possible in the window without being cropped
	e_zoom_mode_fit_width,  // image is as large as possible in the window, but instead is cropped vertically, so the edges of the image touch the sides of the window
	e_zoom_mode_fixed,      // user specified zoom level
};


enum e_gallery_sort_mode
{
	e_gallery_sort_mode_name_a_z,
	e_gallery_sort_mode_name_z_a,

	e_gallery_sort_mode_date_mod_new_to_old,
	e_gallery_sort_mode_date_mod_old_to_new,

	e_gallery_sort_mode_date_created_new_to_old,
	e_gallery_sort_mode_date_created_old_to_new,

	e_gallery_sort_mode_size_large_to_small,
	e_gallery_sort_mode_size_small_to_large,

	// TODO: add resolution size, large to small

	e_gallery_sort_mode_count,
};


// ---------------------------------------------------------


struct image_frame_t
{
	// time to spend on frame
	double         time;

	// image data
	unsigned char* data;

	// size
	size_t         size;
};


struct image_t
{
	int                width;
	int                height;

	int                bit_depth;
	int                pitch;
	int                bytes_per_pixel;
	int                channels;
	GLint              format;

	int                loop_count;
	// std::vector< image_frame_t > frame;
	std::vector< u8* > frame;

	// TEMP until image_frame_t is used
	size_t             frame_size;

	char*              image_format;
};


struct image_load_info_t
{
	// Image frame, this will be reused if valid frame, result is also stored in here
	image_t* image;

	// When not 0, The codec will load the smallest version of an image that's larger than this resolution
	ImVec2   target_size;

	// leads to a lower quality image if the codec has options for this, otherwise load it in max quality
	bool     load_quick;

	// leads to a lower quality image if the codec has options for this, otherwise load it in max quality
	bool     thumbnail_load;

	// Is this being loaded from a thread?
	bool     threaded_load;

	// Only load the first frame of this image, usually for thumbnails
	bool     single_frame;

	// No error printing!
	bool     quiet;
};


struct IImageLoader
{
	virtual bool check_extension( std::string_view ext )                                                       = 0;
	virtual bool check_header( const fs::path& path )                                                          = 0;

	// Load the smallest version of an image that's larger than the inputted size
	//virtual bool     image_load_scaled( const fs::path& path, image_t* image_info, int area_width, int area_height ) = 0;

	virtual bool image_load( const fs::path& path, image_load_info_t& load_info, char* data, size_t data_len ) = 0;
	//virtual image_t* image_load( const fs::path& path )                                                              = 0;
};


struct bookmark_t
{
	std::string name{};
	std::string path{};
	bool        valid = false;
};


struct app_config_t
{
	std::vector< bookmark_t > bookmark{};

	u32                       thumbnail_threads           = 8;
	u32                       thumbnail_uploads_per_frame = 4;

	// size in kilobytes
	u32                       thumbnail_mem_cache_size    = 20000;

	// resoultion of thumbnail
	u32                       thumbnail_size              = 500;

	bool                      thumbnail_use_fixed_size    = false;

	u32                       thumbnail_jxl_enable        = 0;
	float                     thumbnail_jxl_distance      = 4;
	u32                       thumbnail_jxl_effort        = 6;

	std::string               thumbnail_cache_path{};
	std::string               thumbnail_video_cache_path{};

	int                       vsync                  = 1;
	u32                       no_focus_sleep_time    = 8;
	bool                      no_video               = 0;
	bool                      gallery_show_filenames = 0;
};


struct media_entry_t
{
	file_t       file;
	std::string  filename;
	e_media_type type;
};


struct main_image_data_t
{
	// source image
	image_t image{};

	// index in sorted file list
	size_t  index   = 0;

	// TODO: add multiple frames here
	GLuint  texture = 0;
};


// General App Data
namespace app
{
	extern bool         running;

	extern SDL_Window*  window;
	extern bool         window_focused;
	extern bool         window_resized;

	extern u64          total_time;
	extern float        frame_time;

	extern ImVec2       mouse_delta;
	extern ImVec2       mouse_pos;

	// imgui scroll hack lol
	extern bool         mouse_scrolled_up;
	extern bool         mouse_scrolled_down;

	extern ImVec4       clear_color;

	extern app_config_t config;
}


// ImGui Fonts
namespace font
{
	extern ImFont* normal;
	extern ImFont* normal_bold;
	extern ImFont* normal_italic;
}


// Current Working Directory Information
namespace directory
{
	extern fs::path                      path;
	extern fs::path                      queued;  // will change to this folder start of next frame
	extern std::vector< media_entry_t >  media_list;
	extern std::vector< h_thumbnail >    thumbnail_list;
}


// Gallery View
namespace gallery
{
	// a sorted list of media entries, each item is an index to an entry in directory::media_list
	extern std::vector< size_t >         sorted_media;

	// cursor position/index in items
	extern size_t                        cursor;

	extern e_gallery_sort_mode           sort_mode;
	extern bool                          sort_mode_update;

	extern int                           row_count;
	extern int                           item_size;
	extern int                           item_size_min;
	extern int                           item_size_max;
	extern bool                          item_size_changed;
	extern std::vector< ImVec2 >         item_text_size;

	extern int                           image_size;

	extern bool                          sidebar_draw;

	extern bool                          scroll_to_cursor;

	// Files selected in the gallery view
	extern std::vector< u32 >            selection;
}


// Media View
namespace media
{
}


extern bool                          g_gallery_view;

// Main Image
extern main_image_data_t             g_image_data;
extern main_image_data_t             g_image_scaled_data;


void                                 media_view_init();
void                                 media_view_shutdown();
void                                 media_view_scale_check_timer( float frame_time );

// Load currently selected file, does not change view type though
void                                 media_view_load();
void                                 media_view_input();
void                                 media_view_draw_imgui();
void                                 media_view_draw();
void                                 media_view_scroll_zoom( float amount );
void                                 media_view_advance( bool prev = false );
void                                 media_view_window_resize();
void                                 media_view_fit_in_view( bool adjust_zoom = true, bool center_image = true );
void                                 media_view_zoom_reset();
void                                 media_view_scale_reset_timer();

const media_entry_t&                 gallery_item_get_media_entry( size_t index );
const file_t&                        gallery_item_get_file( size_t index );
const fs::path&                      gallery_item_get_path( size_t index );
std::string                          gallery_item_get_path_string( size_t index );

void                                 gallery_view_scroll_to_cursor();
void                                 gallery_view_input();
void                                 gallery_view_draw();
void                                 gallery_view_dir_change();

void                                 set_view_type_gallery();
void                                 set_view_type_media();
void                                 view_type_toggle();

void                                 update_window_title();
void                                 folder_load_media_list();

void                                 push_notification( const char* msg );

bool                                 icon_preload();
void                                 icon_free();
image_t*                             icon_get_image( e_icon icon_type );
ImTextureRef                         icon_get_imtexture( e_icon icon_type );

GLuint                               gl_upload_texture( image_t* image );
void                                 gl_update_texture( GLuint texture, image_t* image );
void                                 gl_free_texture( GLuint texture );

void                                 config_reset();
bool                                 config_load();


// ---------------------------------------------------------
// codec handler


void image_register_codec( IImageLoader* codec, bool fallback );

// Load an image from disk or from memory
// If nothing is passed in for file_data and data_len, it loads the file internally
bool image_load( const fs::path& path, image_load_info_t& load_info, char* file_data = nullptr, size_t data_len = 0 );

// not seprate files to check the extension of path still
// bool image_load_from_memory( image_load_info_t& load_info, char* file_data, size_t file_len );
// bool image_load( image_load_info_t& load_info, const fs::path& path );

// Free all image data
void image_free( image_t& image );

// Free only frames
void image_free_frames( image_t& image );

// Free only frames and allocations
void image_free_alloc( image_t& image );

bool media_check_extension( std::string_view ext, e_media_type& type );
bool image_check_extension( std::string_view ext );
bool image_downscale( image_t* old_image, image_t* new_image, int new_width, int new_height );


// TODO: add image load functions here
// - add animated image support
// - add color profile support (PAIN)
// - split it into reading the file first, passing it into each codec to check the header, if valid, load the rest of the image


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
	image_t*                          image;
	GLuint                            texture;
	e_media_type                      type;
	ImTextureRef                      im_texture;
	bool                              scaled;
};


bool          thumbnail_loader_init();
void          thumbnail_loader_shutdown();
void          thumbnail_loader_update();

h_thumbnail   thumbnail_loader_queue_push( const media_entry_t& media_entry );
thumbnail_t*  thumbnail_get_data( h_thumbnail handle );
// void          thumbnail_free( const fs::path& path, h_thumbnail handle );

void          thumbnail_add( const fs::path& path );
void          thumbnail_remove( const fs::path& path );

void          thumbnail_clear_cache();

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


