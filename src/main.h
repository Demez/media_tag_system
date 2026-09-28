#pragma once

#include "core.h"
#include "config_options.h"

#include "mpv_interface.h"
#include "args.h"
#include "system/system.h"
#include "input.h"
#include "image.h"
#include "ui_main.h"
#include "job_system.h"

#include "imgui.h"
#include "glad/glad.h"

#include <cstdio>
#include <vector>
#include <atomic>


// ---------------------------------------------------------


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
	e_zoom_mode_fit,            // image is as large as possible in the window without being upscaled
	e_zoom_mode_fit_window,     // image is as large as possible in the window
	e_zoom_mode_fit_width,      // image is as large as possible in the window, but instead is cropped vertically, so the edges of the image touch the sides of the window
	e_zoom_mode_fit_height,     // image is as large as possible in the window, but instead is cropped vertically, so the edges of the image touch the sides of the window
	e_zoom_mode_fixed,          // user specified zoom level
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
	e_media_type type{};
};


struct selection_t
{
	u32           index = 0;
	media_entry_t entry{};
};


// internal draw info for gallery for each item
struct gallery_item_draw_t
{
	// current gallery index
	size_t         i             = 0;
	size_t         gallery_index = 0;

	// current media entry
	media_entry_t* media         = nullptr;

	ImVec2         text_size{};
	ImVec2         image_size{};
	ImVec2         image_bounds{};

	// float          item_size_y = 0.f;

	ImVec2         cursor_screen_pos{};
	ImVec2         item_rect_min{};
	ImVec2         item_rect_max{};

	bool           selected_item = false;
	bool           item_hovered  = false;
	bool           visible       = false;

	float          get_height( ImGuiStyle& style );
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
	extern u64         last_frame_time;  // time in system time
	extern u64         next_frame_time;  // time until next frame, add to last_frame_time
	extern size_t      frame;
	extern double      playback_speed;
	extern bool        pause;
	extern bool        scaling;

	// index into gallery::sorted_media
	//extern size_t media_index;
}


struct folder_scan_status_t;


// if in_main_thread is false, this is being called from the SDL event watch function, and can be in a different thread
// some tasks may be ok with calling that from the thread
typedef void ( folder_scan_callback_t )( folder_scan_status_t* status, bool in_main_thread );
typedef void* ( folder_scan_thread_func_t )( folder_scan_status_t* status );


// For running the scan directory in a background thread
// TODO: GET RID OF THIS !!!
struct folder_scan_status_t
{
	job_status_t*              job             = nullptr;

	// function to call on the main thread when finsished
	folder_scan_callback_t*    callback        = nullptr;

	// function to call in the worker thread if we need additional slow processing done
	// returns a void* to store in thread_userdata, this is your own allocated memory
	// you need to free it later on your own
	folder_scan_thread_func_t* thread_func     = nullptr;
	void*                      thread_userdata = nullptr;

	std::vector< file_t >      files{};

	fs::path_char*             root     = nullptr;
	e_scandir_flags            flags    = 0;

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
	bool   hide_channel[ 4 ];
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

void                                 imgui_draw( bool render );

// Handle new file or path from external source
bool                                 on_new_file( const fs::path& file_path );

// non-blocking folder scanning
folder_scan_status_t*                folder_scan_push( const fs::path_char* root, e_scandir_flags flags, folder_scan_callback_t* callback, folder_scan_thread_func_t* thread_func = nullptr );

void                                 image_copy_data( image_t& src, image_t& dst );
void                                 image_copy_frame_data( image_frame_t& src, image_frame_t& dst );
bool                                 image_copy_frame_data( image_t& src, image_t& dst, size_t frame_i );

void                                 media_view_init();
void                                 media_view_shutdown();
void                                 media_view_update();
e_media_type                         get_media_type();

// Load currently selected file, does not change view type though
void                                 media_view_load();
void                                 media_view_input();
void                                 media_view_draw_imgui();
void                                 media_view_draw();
void                                 media_view_scroll_zoom( int amount );
void                                 media_view_advance( bool prev = false );
void                                 media_view_window_resize();
void                                 media_view_fit_in_view( bool adjust_zoom = true, bool center_image = true, bool adjust_zoom_step = true );
void                                 media_view_zoom_reset();
void                                 media_view_scale_reset_timer();

// media_entry_t                        gallery_item_get_media_entry( size_t index );
const media_entry_t&                 gallery_item_get_media_entry( size_t index );
const file_t&                        gallery_item_get_file( size_t index );

// returns an absolute path to the file
fs::path                             gallery_item_get_path( size_t index );
std::string                          gallery_item_get_path_string( size_t index );

void                                 gallery_view_scroll_to_cursor();

void                                 gallery_view_handle_scroll_event( float mouse_y );
void                                 gallery_view_input();
void                                 gallery_view_draw();
void                                 gallery_view_dir_change( bool keep_selection );
void                                 gallery_view_sort_dir();
void                                 gallery_view_reset_text_size();
void                                 gallery_view_reset();

void                                 gallery_view_draw_content();
void                                 gallery_draw_extra_refresh( int count = 1 );

void                                 gallery_view_set_selection( size_t gallery_item_index );
void                                 gallery_view_clear_selection();
void                                 gallery_view_delete_selection();
selection_t                          gallery_view_get_last_selected();
u32                                  gallery_view_get_last_selected_index( u32 empty_return = 0 );  // returns empty_return if selection is empty
media_entry_t                        gallery_view_get_last_selected_entry();

bool                                 gallery_view_input_do_multi_select();
void                                 gallery_view_input_check_clear_multi_select();
void                                 gallery_view_input_update_multi_select( u32 index, bool readd = true );

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

bool                                 render_window_prepare_for_creation();
bool                                 render_window_test();
void                                 render_window_set_fallbacks();

bool                                 render_init();
void                                 render_shutdown();
void                                 render_window_resize();
void                                 render_draw_texture( render_draw_texture_t draw_info );

bool                                 config_init();
void                                 config_free();

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

