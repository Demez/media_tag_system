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


extern const char* g_gallery_sort_mode_str[];


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
	u32                       thumbnail_size              = 600;

	bool                      thumbnail_use_fixed_size    = false;

	bool                      thumbnail_jxl_enable        = true;
	float                     thumbnail_jxl_distance      = 4;
	u32                       thumbnail_jxl_effort        = 6;

	std::string               thumbnail_cache_path{};
	std::string               thumbnail_video_cache_path{};

	int                       vsync                  = 1;

	u32                       sleep_time_no_focus    = 5;
	u32                       sleep_time_focus       = 1;
	u32                       sleep_time_idle        = 15;

	u32                       font_size              = 17;

	u32                       gallery_zoom_default   = 200;
	float                     media_zoom_scale       = 0.1;

	bool                      no_video               = false;
	bool                      gallery_show_filenames = false;
	bool                      always_draw            = false;
	
	// Theming
	bool                      dwm_extend             = true;
	bool                      use_custom_colors      = true;

	ImVec2                    gallery_header_padding{};
	ImVec4                    header_bg_color{};
	ImVec4                    sidebar_bg_color{};
	ImVec4                    content_bg_color{};

	ImVec4                    media_bg_color{};
};


// add this to the thumbnail cache system
// saves metadata on the image or video here
// useful for more file info in the gallery
// or maybe if you do more background loading of thumbnails
// not sure if i do want background loading for the whole folder though, may eat cpu on large folders or searches
// but, then you could sort media by some info here
struct cached_media_info_t
{
	// maybe add a file path here?
	// this may be saved and never removed in the program unless a directory change happens
	// so then we can store more of these than thumbnails

	int width;
	int height;

	// time in miliseconds
	u64 video_duration;
};


struct media_entry_t
{
	file_t       file;
	std::string  filename;
	e_media_type type;
};


// -------------------------------------------------------------------------------------------
// Image Data


// https://www.theimage.com/animation/pages/disposal.html
// https://www.theimage.com/animation/pages/disposal2.html
// GIF
enum e_frame_disposal
{
	e_frame_disposal_keep,        // leave rendered image on canvas and draw over it
	e_frame_disposal_background,  // restore to background color or transparency before drawing
	e_frame_disposal_previous,    // only keep the previous frame and draw on top of that

	e_frame_disposal_count,
};


// JPEG XL, can i join the above into this somehow? or is this wrong, i haven't touched this yet still
enum e_frame_blend_mode
{
	e_frame_blend_mode_none,

	e_frame_blend_mode_replace,
	e_frame_blend_mode_add,
	e_frame_blend_mode_blend,
	e_frame_blend_mode_multiply_add,
	e_frame_blend_mode_multiply,

	e_frame_blend_mode_count,
};


// TODO: use shaders for drawing images
// also apply palette's in the shader itself, maybe it will have a faster load time?
struct image_frame_t
{
	// image data
	u8*              data;

	// size
	size_t           size;

	// time to spend on frame
	double           time;

	// frame width and height
	int              width;
	int              height;

	// frame draw position relative to image draw position
	int              pos_x;
	int              pos_y;

	e_frame_disposal frame_disposal;

	image_frame_t()
	{
		data           = nullptr;
		size           = 0;
		time           = 0.0;
		width          = 0;
		height         = 0;
		pos_x          = 0;
		pos_y          = 0;
		frame_disposal = e_frame_disposal_keep;
	}

	~image_frame_t()
	{
		ch_free( e_mem_category_image_data, data );
	}
};


struct image_t
{
	int                          width;
	int                          height;

	int                          bit_depth;
	int                          pitch;
	int                          bytes_per_pixel;
	int                          channels;
	GLint                        format;

	int                          loop_count;

	std::vector< image_frame_t > frame;

	char*                        image_format;
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


struct uploaded_textures_t
{
	GLuint* frame = nullptr;
	size_t  count = 0;
};


struct main_image_data_t
{
	// source image
	image_t             image{};

	// index in sorted file list
	size_t              index = 0;

	uploaded_textures_t textures{};
};


// -------------------------------------------------------------------------------------------
// Thumbnail Data


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
	uploaded_textures_t               textures{};
	e_media_type                      type;
	ImTextureRef                      im_texture;
	bool                              scaled;
};


// -------------------------------------------------------------------------------------------


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
	extern int          mouse_scroll;
	extern bool         mouse_middle_press;

	// extern ImVec4       clear_color;

	extern app_config_t config;

	extern bool         draw_frame;
	extern bool         draw_next_frame;
	extern bool         in_window_drag;
	extern bool         in_drag_drop;
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

	// the folder path split by path separators
	extern std::vector< std::string >    path_chunks;
	extern bool                          path_edit;

	extern std::vector< std::string >    media_history;
	extern std::vector< fs::path >       folder_history;
	extern size_t                        folder_history_pos;

	extern bool                          folder_reload;
	extern bool                          recursive;
}


// Gallery View
namespace gallery
{
	// a sorted list of media entries, each item is an index to an entry in directory::media_list
	extern std::vector< size_t >         sorted_media;

	extern char                          search[ 512 ];

	// cursor position/index in items
	extern size_t                        cursor;

	extern e_gallery_sort_mode           sort_mode;
	extern bool                          sort_mode_update;

	extern u32                           row_count;
	extern u32                           item_size;
	extern u32                           item_size_min;
	extern u32                           item_size_max;
	extern bool                          item_size_changed;
	extern std::vector< ImVec2 >         item_text_size;

	extern u32                           image_size;

	extern bool                          sidebar_draw;

	extern bool                          scroll_to_cursor;

	// Files selected in the gallery view
	extern std::vector< u32 >            selection;
}


// Media View
namespace image_draw
{
	// Animated image playback information
	extern double next_frame_timer;
	extern size_t frame;
	extern float  playback_speed;
	extern bool   pause;
}


extern bool                          g_gallery_view;

extern bool                          g_mpv_video_ready;

// Main Image
extern main_image_data_t             g_image_data;
extern main_image_data_t             g_image_scaled_data;

void                                 image_copy_data( image_t& src, image_t& dst );
void                                 image_copy_frame_data( image_frame_t& src, image_frame_t& dst );
bool                                 image_copy_frame_data( image_t& src, image_t& dst, size_t frame_i );

void                                 media_view_init();
void                                 media_view_shutdown();
void                                 media_view_update( float frame_time );
e_media_type                         get_media_type();

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
void                                 gallery_view_dir_change( bool keep_selection );
void                                 gallery_view_sort_dir();
void                                 gallery_view_set_selection( size_t gallery_item_index );
void                                 gallery_view_reset_text_size();

void                                 media_history_add( const std::string& entry );
void                                 folder_history_add( const fs::path& entry );
const fs::path&                      folder_history_get_prev();
const fs::path&                      folder_history_get_next();
bool                                 folder_history_nav_prev();
bool                                 folder_history_nav_next();

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

// GLuint                               gl_upload_texture( image_t* image );
void                                 gl_update_textures( uploaded_textures_t& textures, image_t* image, size_t frame_count );
void                                 gl_update_texture( GLuint texture, image_t* image, size_t frame_i = 0 );
void                                 gl_free_textures( uploaded_textures_t& textures );

void                                 config_reset();
bool                                 config_load();


// -------------------------------------------------------------------------------------------
// image loader


// internal image loader data
using image_loader_handle_t = void*;

constexpr image_loader_handle_t INVALID_IMAGE_HANDLE = nullptr;


struct image_handle_t
{
	size_t                loader_id       = 0;
	bool                  fallback_loader = false;
	image_loader_handle_t handle          = INVALID_IMAGE_HANDLE;

	/*bool             operator!()
	{
		return handle == INVALID_IMAGE_HANDLE;
	}*/

	operator bool()
	{
		return handle != INVALID_IMAGE_HANDLE;
	}
};


struct IImageLoader
{
	virtual void get_supported_extensions( std::vector< std::string >& extensions )                            = 0;

	virtual bool check_header( const fs::path& path )                                                          = 0;

	// Load the smallest version of an image that's larger than the inputted size
	//virtual bool     image_load_scaled( const fs::path& path, image_t* image_info, int area_width, int area_height ) = 0;

	// OLD INTERFACE
	virtual bool image_load( const fs::path& path, image_load_info_t& load_info, char* data, size_t data_len ) = 0;
	//virtual image_t* image_load( const fs::path& path )                                                              = 0;

	// NEW INTERFACE WIP
	// Allow for background image loading ideally and trying to stream in data
#if 0

	virtual image_loader_handle_t open( image_load_info_t& load_info, char* data, size_t data_len )                    = 0;
	virtual void                  close( image_loader_handle_t handle )                                                = 0;

	virtual size_t                get_frame_count( image_loader_handle_t handle )                                      = 0;
	virtual bool                  load_frames( image_loader_handle_t handle, size_t frame_offset, size_t frame_count ) = 0;
#endif

	size_t loader_id = 0;
};


// Image Loader Threads

enum e_image_queue_state
{
	e_image_queue_idle,
	e_image_queue_start,
	e_image_queue_open,
	e_image_queue_loading_frame_0,
	e_image_queue_loading_frames,
	e_image_queue_finished,

	e_image_queue_count,
};


struct image_queue_data_t
{
	image_load_info_t*  load_info;
	e_image_queue_state state;
};


image_queue_data_t image_load_queue( const std::string& path, image_load_info_t* load_info );
void               image_load_cancel( image_queue_data_t& queue_data );


void          image_register_codec( IImageLoader* codec, bool fallback );

// Load an image from disk or from memory
// If nothing is passed in for file_data and data_len, it loads the file internally
bool          image_load( const fs::path& path, image_load_info_t& load_info, char* file_data = nullptr, size_t data_len = 0 );

// Free all image data
void          image_free( image_t& image );

// Free only frames
void          image_free_frames( image_t& image );

// Free only frames and allocations
void          image_free_alloc( image_t& image );

bool          media_check_extension( const std::string& ext, e_media_type& type );
IImageLoader* image_check_extension( const std::string& ext );
bool          image_scale( image_t* old_image, image_t* new_image, int new_width, int new_height );


// TODO: add image load functions here
// - add animated image support
// - add color profile support (PAIN)
// - split it into reading the file first, passing it into each codec to check the header, if valid, load the rest of the image


// -------------------------------------------------------------------------------------------
// Thumbnail System


bool         thumbnail_loader_init();
void         thumbnail_loader_shutdown();
void         thumbnail_loader_update();

h_thumbnail  thumbnail_loader_queue_push( const media_entry_t& media_entry );
thumbnail_t* thumbnail_get_data( h_thumbnail handle );

void         thumbnail_clear_cache();

// distance based cache
void         thumbnail_update_distance( h_thumbnail handle, u32 distance );

void         thumbnail_cache_debug_draw();

