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
HANDLE_GEN_32( h_job );


// ---------------------------------------------------------


enum e_media_type : u8
{
	e_media_type_none,
	e_media_type_directory,
	e_media_type_image,
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
	e_icon_image,

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


enum e_gallery_filter_ : u8
{
	e_gallery_filter_none,
	e_gallery_filter_folders = 1 << 0,
	e_gallery_filter_images  = 1 << 1,
	e_gallery_filter_videos  = 1 << 2,

	e_gallery_filter_media   = e_gallery_filter_images | e_gallery_filter_videos,
	e_gallery_filter_all     = e_gallery_filter_folders | e_gallery_filter_images | e_gallery_filter_videos,
	e_gallery_filter_count   = 4,
};


using e_gallery_filter = u8;


enum e_gallery_scan : u8
{
	e_gallery_scan_idle,
	e_gallery_scan_filesystem,
	e_gallery_scan_building,
	e_gallery_scan_sorting,

	e_gallery_scan_count,
};


extern const char* g_gallery_sort_mode_str[];


struct directory_entry_t
{
	fs::path              path;
	std::vector< file_t > folders;

	// hmmmm
	bool                  used_this_frame = false;
	bool                  valid           = false;
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
	u32                       thumbnail_save_threads      = 2;
	u32                       thumbnail_uploads_per_frame = 4;

	// size in kilobytes
	u32                       thumbnail_mem_cache_size    = 20000;

	// resoultion of thumbnail
	u32                       thumbnail_size              = 600;

	bool                      thumbnail_use_fixed_size    = false;

	bool                      thumbnail_enable            = true;
	bool                      thumbnail_jxl_enable        = true;
	float                     thumbnail_jxl_distance      = 4;
	u32                       thumbnail_jxl_effort        = 6;

	std::string               thumbnail_cache_path{};
	std::string               thumbnail_video_cache_path{};

	int                       vsync                          = 1;
	bool                      high_bpc                       = false;
	bool                      hdr                            = false;

	u32                       job_threads                    = 3;

	u32                       sleep_time_no_focus            = 5;
	u32                       sleep_time_focus               = 1;
	u32                       sleep_time_idle                = 15;
	double                    apply_sleep_time_threshold     = 0.005;

	u32                       font_size                      = 17;

	u32                       gallery_zoom_default           = 200;
	float                     media_zoom_scale               = 0.1f;

	bool                      no_video                       = false;
	bool                      gallery_show_filenames         = true;
	bool                      always_draw                    = false;
	bool                      single_instance                = false;
	bool                      dev_mode                       = false;

	bool                      directory_tree_auto_expand     = true;
	bool                      directory_tree_expand_on_click = true;
	bool                      directory_tree_simple          = false;

	// Theming
	bool                      dwm_extend                     = false;
	bool                      use_custom_colors              = false;

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
	file_t       file{};
	std::string  filename{};
	e_media_type type{};
};


struct selection_t
{
	u32           index = 0;
	media_entry_t entry{};
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
		data = nullptr;
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

	// Waiting for the save function to finish
	e_thumbnail_status_save_waiting,

	// Thumbnail is uploaded and ready for use
	e_thumbnail_status_finished,

	// Failed to load thumbnail
	e_thumbnail_status_failed,
};


enum e_thumbnail_save_status
{
	e_thumbnail_save_idle,
	e_thumbnail_save_queued,
	e_thumbnail_save_saving,
	e_thumbnail_save_finished,
	e_thumbnail_save_cancel,
};


struct thumbnail_t
{
	std::atomic< e_thumbnail_status >      status;
	std::atomic< e_thumbnail_save_status > save_status;
	char*                                  path;      // mainly for debugging
	image_t*                               image;
	image_t*                               image_scaled;
	uploaded_textures_t                    textures{};
	u32                                    distance;  // higher distances get freed first for other thumbnails
	e_media_type                           type;
	// bool                                   scaled;
};


// internal draw info for gallery for each item
struct gallery_item_draw_t
{
	// position in grid
	u32            grid_pos_x;
	u32            grid_pos_y;

	// current gallery index
	size_t         i             = 0;
	size_t         gallery_index = 0;

	// current media entry
	media_entry_t* media         = nullptr;

	ImVec2         text_size{};

	float          item_size_y = 0.f;

	ImVec2         cursor_screen_pos{};
	ImVec2         item_rect_min{};
	ImVec2         item_rect_max{};

	bool           selected_item = false;
	bool           item_hovered  = false;
	bool           visible       = false;
};


// -------------------------------------------------------------------------------------------


// General App Data
namespace app
{
	extern bool         running;

	extern SDL_Window*  window;
	extern bool         window_focused;
	extern bool         window_resized;
	extern float        dpi;

	extern u64          total_time;
	extern double       frame_time;

	extern ImVec2       mouse_delta;
	extern ImVec2       mouse_pos;
	extern int          mouse_scroll;
	extern bool         mouse_in_window;

	// extern ImVec4       clear_color;

	extern app_config_t config;

	extern u32          draw_frame_count;
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

	extern bool                          folder_loading;  // folder is currently loading in the background
	extern bool                          folder_reload;   // folder has been reloaded, same directory
	extern bool                          folder_changed;  // folder has been changed
	extern bool                          recursive;
}


// Gallery View
namespace gallery
{
	extern e_gallery_scan                     scan_state;

	// a sorted list of media entries, each item is an index to an entry in directory::media_list
	extern std::vector< size_t >              sorted_media;

	extern char                               search[ 512 ];

	// cursor position/index in items
	// extern size_t                        cursor;

	extern e_gallery_sort_mode                sort_mode;
	extern bool                               sort_mode_update;

	extern u32                                row_count;
	extern u32                                item_size;
	extern u32                                item_size_min;
	extern u32                                item_size_max;
	extern bool                               item_size_changed;
	extern bool                               item_size_changing;
	extern std::vector< ImVec2 >              item_text_size;

	extern std::vector< gallery_item_draw_t > item_layout;
	extern gallery_item_draw_t**              visible_item;
	extern size_t                             visible_item_count;

	extern ImVec2                             image_bounds;

	extern bool                               sidebar_draw;
	extern bool                               content_area_resized;

	extern bool                               scroll_to_cursor;
	extern bool                               keep_scroll_pos;
	extern int                                refresh_layout;

	extern u32                                drawn_image_count;
	extern u32                                first_visible_item;

	// Quick Filter
	extern e_gallery_filter                   filter;

	// Files selected in the gallery view
	extern std::vector< selection_t >         selection;

	// used for memory with media advancing with arrow keys
	extern selection_t                        last_selection;

	extern bool                               always_recalc_item_sizes;
	extern bool                               always_recalc_layout;
}


// Media View
namespace image_draw
{
	extern e_zoom_mode zoom_mode;
	extern double      zoom;
	extern int         zoom_step;  // 0 = 100% zoom
	extern ImVec2      pos;
	extern ImVec2      size;
	extern bool        flip_v;
	extern bool        flip_h;
	extern float       rot;

	// Animated image playback information
	extern double      next_frame_timer;
	extern size_t      frame;
	extern double      playback_speed;
	extern bool        pause;
	extern bool        scaling;

	// index into gallery::sorted_media
	//extern size_t media_index;
}


struct folder_scan_status_t;
struct job_status_t;


// if in_main_thread is false, this is being called from the SDL event watch function, and can be in a different thread
// some tasks may be ok with calling that from the thread
typedef void ( folder_scan_callback_t )( folder_scan_status_t* status, bool in_main_thread );
typedef void* ( folder_scan_thread_func_t )( folder_scan_status_t* status );


// For running the scan directory in a background thread
struct folder_scan_status_t
{
	// function to call on the main thread when finsished
	folder_scan_callback_t*    callback        = nullptr;

	// function to call in the worker thread if we need additional slow processing done
	// returns a void* to store in thread_userdata, this is your own allocated memory
	// you need to free it later on your own
	folder_scan_thread_func_t* thread_func     = nullptr;
	void*                      thread_userdata = nullptr;

	std::vector< file_t >      files{};

	char*                      root     = nullptr;
	e_scandir_flags            flags    = 0;

	// set to true to cancel the scan
	bool                       cancel   = false;

	// check to see if it finished
	bool                       finished = false;

	// the return value of sys_scandir
	bool                       result   = false;
};


extern SDL_Event g_event_folder_scan_finish;


struct render_draw_texture_t
{
	int    width;
	int    height;
	int    x;
	int    y;
	float  rotation;

	GLuint texture;

	// draw settings
	bool   flip_v     = false;
	bool   flip_h     = false;

	// draw a specific channel of the image
	int    channel    = -1;
	bool   hide_alpha = false;
};


extern bool                          g_gallery_view;

extern bool                          g_mpv_video_ready;

// Main Image
extern main_image_data_t             g_image_data;
extern main_image_data_t             g_image_scaled_data;

void                                 set_frame_draw( u32 count = 1 );
void                                 send_frame_draw_event();
void                                 update_dpi( float dpi_override = 0.f );

void                                 imgui_draw( double frame_time, bool render );

// Handle new file or path from external source
bool                                 on_new_file( const fs::path& file_path );

// non-blocking folder scanning
folder_scan_status_t*                folder_scan_push( const char* root, e_scandir_flags flags, folder_scan_callback_t* callback, folder_scan_thread_func_t* thread_func = nullptr );

// call this when finished doing work after scanning is complete
void                                 folder_scan_free( folder_scan_status_t* status );

bool                                 folder_scan_init();
void                                 folder_scan_shutdown();

void                                 image_copy_data( image_t& src, image_t& dst );
void                                 image_copy_frame_data( image_frame_t& src, image_frame_t& dst );
bool                                 image_copy_frame_data( image_t& src, image_t& dst, size_t frame_i );

void                                 media_view_init();
void                                 media_view_shutdown();
void                                 media_view_update( double frame_time );
e_media_type                         get_media_type();

// Load currently selected file, does not change view type though
void                                 media_view_load();
void                                 media_view_input();
void                                 media_view_draw_imgui();
void                                 media_view_draw();
void                                 media_view_scroll_zoom( int amount );
void                                 media_view_advance( bool prev = false );
void                                 media_view_window_resize();
void                                 media_view_fit_in_view( bool adjust_zoom = true, bool center_image = true );
void                                 media_view_zoom_reset();
void                                 media_view_scale_reset_timer();

// media_entry_t                        gallery_item_get_media_entry( size_t index );
const media_entry_t&                 gallery_item_get_media_entry( size_t index );
const file_t&                        gallery_item_get_file( size_t index );
const fs::path&                      gallery_item_get_path( size_t index );
std::string                          gallery_item_get_path_string( size_t index );

void                                 gallery_view_scroll_to_cursor();
void                                 gallery_view_keep_scroll_pos();

void                                 gallery_view_handle_scroll_event( float mouse_y );
void                                 gallery_view_input();
void                                 gallery_view_draw();
void                                 gallery_view_dir_change( bool keep_selection );
void                                 gallery_view_sort_dir();
void                                 gallery_view_reset_text_size();
void                                 gallery_view_reset();

void                                 gallery_view_set_selection( size_t gallery_item_index );
void                                 gallery_view_clear_selection();
selection_t                          gallery_view_get_last_selected();
u32                                  gallery_view_get_last_selected_index( u32 empty_return = 0 );  // returns empty_return if selection is empty
media_entry_t                        gallery_view_get_last_selected_entry();

void                                 media_history_add( const std::string& entry );
void                                 folder_history_add( const fs::path& entry );
fs::path                             folder_history_get_prev();
fs::path                             folder_history_get_next();
bool                                 folder_history_nav_prev();
bool                                 folder_history_nav_next();

void                                 set_view_type_gallery();
void                                 set_view_type_media( bool force_load_media = false );
void                                 view_type_toggle();

void                                 update_window_title();
void                                 folder_load_media_list();

void                                 push_notification( const char* msg );

// if returned true, delete files
bool                                 delete_file_window( size_t count );

bool                                 icon_preload();
void                                 icon_free();
image_t*                             icon_get_image( e_icon icon_type );
ImTextureRef                         icon_get_imtexture( e_icon icon_type );

// GLuint                               gl_upload_texture( image_t* image );
void                                 gl_update_textures( uploaded_textures_t& textures, image_t* image, size_t frame_count );
void                                 gl_update_texture( GLuint texture, image_t* image, size_t frame_i = 0 );
void                                 gl_free_textures( uploaded_textures_t& textures );

bool                                 render_init();
void                                 render_shutdown();
void                                 render_window_resize();
void                                 render_draw_texture( render_draw_texture_t draw_info );

void                                 config_reset();
bool                                 config_load();
void                                 config_save();

void                                 settings_draw();

void                                 dir_tree_watch_changes();
void                                 dir_tree_init();
void                                 dir_tree_shutdown();

void                                 dir_tree_add_folder( fs::path& path );
directory_entry_t*                   dir_tree_get( fs::path& path );

void                                 dir_tree_draw( ImGuiStyle& style );

// returns an index
//size_t                               dir_tree_add_folder( fs::path& path );
//directory_entry_t*                   dir_tree_get( size_t index, fs::path& path );


// -------------------------------------------------------------------------------------------
// Job System


struct job_status_t;


typedef void( job_finish_t )( job_status_t* status, bool in_main_thread );
typedef void( job_function_t )( job_status_t* status );


// For running the scan directory in a background thread
struct job_status_t
{
	// function to call when job is finished on the main thread
	job_finish_t*   callback = nullptr;

	// function to call internally
	job_function_t* function = nullptr;

	// store information you need here
	void*           userdata = nullptr;

	// set to true to cancel the job
	bool            cancel   = false;

	// check to see if it finished
	bool            finished = false;
};


extern SDL_Event g_event_job_finish;


job_status_t*    job_push( job_finish_t* finish_callback, job_function_t* function, void* userdata );

// call this when finished doing work
void             job_free( job_status_t* status );

// cancel a job and free it later
void             job_cancel_and_free( job_status_t* status );

bool             job_init();
void             job_shutdown();


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
void         thumbnail_loader_shutdown( bool free_thumbnails = true );
void         thumbnail_loader_update();

h_thumbnail  thumbnail_loader_queue_push( const media_entry_t& media_entry );
thumbnail_t* thumbnail_get_data( h_thumbnail handle );

void         thumbnail_clear_cache();

// distance based cache
void         thumbnail_update_distance( h_thumbnail handle, u32 distance );

void         thumbnail_cache_debug_draw();

