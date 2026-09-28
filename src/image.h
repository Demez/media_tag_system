#pragma once

#include "core.h"

#include "glad/glad.h"

// -------------------------------------------------------------------------------------------
// Images


enum e_pixfmt
{
	e_pixfmt_invalid,

	e_pixfmt_r8,
	e_pixfmt_r8g8,
	e_pixfmt_r8g8b8,
	e_pixfmt_r8g8b8a8,

	e_pixfmt_count,
};


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


struct animation_format_frame_data_base_t
{
	animation_format_frame_data_base_t() {};
	virtual ~animation_format_frame_data_base_t() = default;
};


struct animation_format_frame_data_gif_t : public animation_format_frame_data_base_t
{
	e_frame_disposal frame_disposal;
};


enum e_animation_format
{
	e_animation_format_none,

	e_animation_format_gif,

	// NOT SUPPORTED YET
	// e_animation_format_apng,
	// e_animation_format_webp,
	// e_animation_format_jxl,

	e_animation_format_count,
};


enum e_image_color_format
{
	// Standard RGB/RGBA Colors
	e_image_color_format_rgba,

	// Use the palette rendering shader
	e_image_color_format_palette,

	e_image_color_format_count,
};


enum e_image_frame_list
{
	// This is a standard animated image
	e_image_frame_list_animated,

	// Every image frame is a different layer, and this image should be treated differently
	e_image_frame_list_layers,
};


enum e_image_frame_f
{
	e_image_frame_f_none          = 0,
	e_image_frame_f_local_palette = 1 << 0,
};


struct image_palette_elem_t
{
	int r;
	int g;
	int b;
	int a;
};


// TODO: test applying palette's in the shader itself, maybe it will have a faster load time?
struct image_frame_t
{
	// image data
	u8*                  data;

	// size
	size_t               size;

	// time to spend on frame
	double               time;

	// frame width and height
	int                  width;
	int                  height;

	// frame draw position relative to image draw position
	int                  pos_x;
	int                  pos_y;

	e_frame_disposal     frame_disposal;

	e_image_frame_f      flags;

	image_palette_elem_t palette;

	// animation_format_frame_data_base_t* frame_data;

	image_frame_t()
	{
		data   = nullptr;
		size   = 0;
		time   = 0.0;
		width  = 0;
		height = 0;
		pos_x  = 0;
		pos_y  = 0;
		// frame_data     = nullptr;
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

	// TODO: probably remove these, bits per pixel you can get with the format option
	int                          bit_depth;
	int                          pitch;
	int                          bytes_per_pixel;  // actually bits per pixel i think lol

	int                          channels;
	GLint                        format;

	// add channels_source - for the source channel count, would be used for jxl thumbnails

	int                          loop_count;

	char*                        image_format;

	//e_animation_format           animation_format;

	std::vector< image_frame_t > frame;
	// image_frame_list_t frame;

	image_t() {
	};

	~image_t()
	{
	}

	//image_frame_t* get_frame( size_t i )
	//{
	//	if ( !frame.data )
	//		return nullptr;
	//
	//	if ( frame.data->frame.size() >= i )
	//		return nullptr;
	//
	//	return &frame.data->frame[ i ];
	//}

	// copying
	void assign( const image_t& other ) noexcept
	{
		frame           = other.frame;

		width           = other.width;
		height          = other.height;
		bit_depth       = other.bit_depth;
		pitch           = other.pitch;
		bytes_per_pixel = other.bytes_per_pixel;
		channels        = other.channels;
		format          = other.format;
		loop_count      = other.loop_count;

		if ( other.image_format )
			image_format = util_strdup( other.image_format );
	}

	// moving
	void assign( image_t&& other ) noexcept
	{
		frame              = std::move( other.frame );

		width              = other.width;
		height             = other.height;
		bit_depth          = other.bit_depth;
		pitch              = other.pitch;
		bytes_per_pixel    = other.bytes_per_pixel;
		channels           = other.channels;
		format             = other.format;
		loop_count         = other.loop_count;

		image_format       = other.image_format;
		other.image_format = nullptr;
	}

	image_t& operator=( image_t&& other ) noexcept
	{
		assign( other );
		return *this;
	}

	// moving
	image_t( image_t&& other ) noexcept
	{
		assign( std::move( other ) );
	}

	//private:
	// copying
	image_t& operator=( const image_t& other ) noexcept
	{
		assign( other );
		return *this;
	}

	// copying
	image_t( const image_t& other ) noexcept
	{
		assign( other );
	}
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


// internal image loader data
using image_loader_handle_t                          = void*;

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


void               image_register_codec( IImageLoader* codec, bool fallback );

// Load an image from disk or from memory
// If nothing is passed in for file_data and data_len, it loads the file internally
bool               image_load( const fs::path& path, image_load_info_t& load_info, char* file_data = nullptr, size_t data_len = 0 );

// Free all image data
void               image_free( image_t& image );

// Free only frames
void               image_free_frames( image_t& image );

// Free only frames and allocations
void               image_free_alloc( image_t& image );

bool               media_check_extension( std::string ext, e_media_type& type );
bool               media_check_extension_fast( std::string& ext, e_media_type& type );

IImageLoader*      image_check_extension( const std::string& ext );
bool               image_scale( image_t* old_image, image_t* new_image, int new_width, int new_height );


// TODO: add image load functions here
// - add animated image support
// - add color profile support (PAIN)
// - split it into reading the file first, passing it into each codec to check the header, if valid, load the rest of the image


// -------------------------------------------------------------------------------------------
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
	fs::path_char*                         path;  // mainly for debugging
	image_t*                               image;
	image_t*                               image_scaled;
	uploaded_textures_t                    textures{};
	u32                                    distance;  // higher distances get freed first for other thumbnails
	e_media_type                           type;
	// bool                                   scaled;
};

struct media_entry_t;

bool               thumbnail_loader_init();
void               thumbnail_loader_shutdown( bool free_thumbnails = true );
void               thumbnail_loader_update();

h_thumbnail        thumbnail_loader_queue_push( const media_entry_t& media_entry );
thumbnail_t*       thumbnail_get_data( h_thumbnail handle );

void               thumbnail_clear_cache();

// distance based cache
void               thumbnail_update_distance( h_thumbnail handle, u32 distance );

void               thumbnail_cache_debug_draw();

