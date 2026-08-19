#pragma once

#include "util.h"


struct bookmark_t
{
	std::string  name{};
	fs::path_str path{};
	bool         valid = false;
};


struct app_config_t
{
	std::vector< bookmark_t > bookmark{};

	u32                       thumbnail_threads           = 8;
	u32                       thumbnail_save_threads      = 2;
	u32                       thumbnail_uploads_per_frame = 4;

	// size in kilobytes
	u32                       thumbnail_mem_cache_size    = 20000;

	// resolution of thumbnail
	u32                       thumbnail_size              = 600;

	bool                      thumbnail_use_fixed_size    = false;

	bool                      thumbnail_enable            = true;
	bool                      thumbnail_jxl_enable        = true;
	float                     thumbnail_jxl_distance      = 4;
	u32                       thumbnail_jxl_effort        = 6;

	fs::path_str              thumbnail_cache_path{};
	fs::path_str              thumbnail_video_cache_path{};

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
	bool                      zoom_under_window_size         = true;
	bool                      media_vertical_nav_buttons     = false;

	bool                      directory_tree_auto_expand     = true;
	bool                      directory_tree_expand_on_click = false;
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

// i don't really like this too much, keeping it for a while, might rework later if i can a better idea on how to handle this

enum e_config_opt_type
{
	e_cfg_stdstring,
	e_cfg_path,
	e_cfg_bool,
	e_cfg_u32,
	e_cfg_s32,
	e_cfg_float,
	e_cfg_vec2,
	e_cfg_vec4,
	e_cfg_color,
};


struct config_opt_t
{
	const char*       name;
	size_t            name_len;
	size_t            offset;  // offset in app::config
	size_t            size;
	e_config_opt_type type;
	bool              ranged;

	const char*       desc;
	// limits here

	union
	{
		float min_float;
		s32   min_s32;
		u32   min_u32;
	};

	union
	{
		float max_float;
		s32   max_s32;
		u32   max_u32;
	};

	constexpr ~config_opt_t() {}

	template< typename VAL >
	constexpr static config_opt_t create_option( const char* name, size_t name_len, size_t offset, e_config_opt_type type, const char* desc )
	{
		return config_opt_t{
			name,
			name_len,
			offset,
			sizeof( VAL ),
			type,
			false,
			desc
		};
	}

	template< typename VAL >
	constexpr static config_opt_t create_option( const char* name, size_t name_len, size_t offset, e_config_opt_type type, const char* desc, VAL min_value, VAL max_value )
	{
		static_assert( std::is_arithmetic_v< VAL >, "Template argument VAL must be an arithmetic type!" );
		static_assert( !std::is_same_v< bool, VAL >, "Template argument VAL should not be a bool for a ranged option!" );
		static_assert( !std::is_same_v< std::string, VAL >, "Template argument VAL cannot be a string!" );
		static_assert( !std::is_same_v< ImVec2, VAL >, "Template argument VAL cannot be a an ImVec2!" );
		static_assert( !std::is_same_v< ImVec4, VAL >, "Template argument VAL cannot be a an ImVec4!" );

		if constexpr ( std::is_same_v< VAL, float > )
		{
			return config_opt_t{
				.name      = name,
				.name_len  = name_len,
				.offset    = offset,
				.size      = sizeof( VAL ),
				.type      = type,
				.ranged    = true,
				.desc      = desc,
				.min_float = min_value,
				.max_float = max_value,
			};
		}
		else if constexpr ( std::is_same_v< VAL, s32 > )
		{
			return config_opt_t{
				.name     = name,
				.name_len = name_len,
				.offset   = offset,
				.size     = sizeof( VAL ),
				.type     = type,
				.ranged   = true,
				.desc     = desc,
				.min_s32  = min_value,
				.max_s32  = max_value,
			};
		}
		else  // u32
		{
			return config_opt_t{
				.name     = name,
				.name_len = name_len,
				.offset   = offset,
				.size     = sizeof( VAL ),
				.type     = type,
				.ranged   = true,
				.desc     = desc,
				.min_u32  = min_value,
				.max_u32  = max_value,
			};
		}
	}
};


#define CONFIG_OPT( type, name, var, desc ) \
	config_opt_t::create_option< decltype( app_config_t::var ) >( name, sizeof( name ) - 1, offsetof( app_config_t, var ), type, desc )

#define CONFIG_OPT_RANGE( type, name, var, min_val, max_val, desc ) \
	config_opt_t::create_option< decltype( app_config_t::var ) >( name, sizeof( name ) - 1, offsetof( app_config_t, var ), type, desc, min_val, max_val )


constexpr config_opt_t g_cfg_opt_thumbnail[] = {
	// config_opt_t::create_option< decltype( app_config_t::thumbnail_cache_path ) >( "cache_path", sizeof( "cache_path" ) - 1, offsetof( app_config_t, thumbnail_cache_path ), sizeof( app_config_t::thumbnail_cache_path ) ),
	CONFIG_OPT( e_cfg_path, "cache_path", thumbnail_cache_path, "Folder for the thumbnail cache" ),
	CONFIG_OPT( e_cfg_path, "cache_path_video", thumbnail_video_cache_path, "Folder for temporary video thumbnails to generate to, before being added to the thumbnail cache" ),
	CONFIG_OPT_RANGE( e_cfg_float, "jxl_distance", thumbnail_jxl_distance, -1.f, 25.f, "" ),
	CONFIG_OPT_RANGE( e_cfg_u32, "jxl_effort", thumbnail_jxl_effort, 0U, 11U, "" ),
	CONFIG_OPT( e_cfg_bool, "jxl_enable", thumbnail_jxl_enable, "Enable the Thumbnail Cache" ),
	CONFIG_OPT( e_cfg_u32, "memory_cache_size", thumbnail_mem_cache_size, "UNUSED CURRENTLY" ),
	CONFIG_OPT( e_cfg_u32, "size", thumbnail_size, "Max size of the Thumbnail Images" ),
	CONFIG_OPT( e_cfg_u32, "threads", thumbnail_threads, "Threads to use for new thumbnail generation and loading" ),
	CONFIG_OPT( e_cfg_u32, "threads_save", thumbnail_save_threads, "Threads to use for saving generated thumbnails to disk" ),
	CONFIG_OPT( e_cfg_u32, "uploads_per_frame", thumbnail_uploads_per_frame, "Max amount of thumbnails you can upload at a time, this blocks the main thread for a second to upload, so don't set this number too high" ),
	CONFIG_OPT( e_cfg_bool, "use_fixed_size", thumbnail_use_fixed_size, "Don't scale thumbnails based on zoom level" ),
};


constexpr config_opt_t g_cfg_opt_theme[] = {
	CONFIG_OPT( e_cfg_bool, "dwm_extend", dwm_extend, "" ),
	CONFIG_OPT( e_cfg_bool, "use_custom_colors", use_custom_colors, "" ),
	CONFIG_OPT( e_cfg_u32, "font_size", font_size, "" ),

	CONFIG_OPT( e_cfg_color, "gallery_header_background_color", header_bg_color, "" ),
	CONFIG_OPT( e_cfg_color, "gallery_content_bg_color", content_bg_color, "" ),
	CONFIG_OPT( e_cfg_color, "gallery_sidebar_bg_color", sidebar_bg_color, "" ),
	CONFIG_OPT( e_cfg_color, "media_background_color", media_bg_color, "" ),

	CONFIG_OPT( e_cfg_vec2, "gallery_header_padding", gallery_header_padding, "" ),
};


constexpr config_opt_t g_cfg_opt_gallery[] = {
	CONFIG_OPT( e_cfg_bool, "directory_tree_auto_expand", directory_tree_auto_expand, "" ),
	CONFIG_OPT( e_cfg_bool, "directory_tree_expand_on_click", directory_tree_expand_on_click, "" ),
	CONFIG_OPT( e_cfg_bool, "directory_tree_simple", directory_tree_simple, "" ),
	CONFIG_OPT( e_cfg_bool, "show_filenames", gallery_show_filenames, "" ),
	CONFIG_OPT( e_cfg_u32, "zoom_default", gallery_zoom_default, "" ),
};


constexpr config_opt_t g_cfg_opt_general[] = {
	CONFIG_OPT( e_cfg_bool, "always_draw", always_draw, "" ),
	CONFIG_OPT( e_cfg_bool, "dev_mode", dev_mode, "" ),
	CONFIG_OPT( e_cfg_bool, "high_bpc", high_bpc, "" ),
	CONFIG_OPT( e_cfg_bool, "no_video", no_video, "" ),
	CONFIG_OPT( e_cfg_bool, "single_instance", single_instance, "" ),
	CONFIG_OPT( e_cfg_bool, "media_vertical_nav_buttons", media_vertical_nav_buttons, "" ),

	// CONFIG_OPT( "job_debug", job_debug, "" ),
	// CONFIG_OPT( "job_debug_delay", job_debug_delay, "" ),
	CONFIG_OPT( e_cfg_u32, "job_threads", job_threads, "" ),

	CONFIG_OPT_RANGE( e_cfg_s32, "vsync", vsync, -1, 1, "WHAT" ),

	CONFIG_OPT( e_cfg_float, "media_zoom_scale", media_zoom_scale, "" ),

	CONFIG_OPT( e_cfg_u32, "sleep_time_no_focus", sleep_time_no_focus, "" ),
	CONFIG_OPT( e_cfg_u32, "sleep_time_focus", sleep_time_focus, "" ),
	CONFIG_OPT( e_cfg_u32, "sleep_time_idle", sleep_time_idle, "" ),
};


constexpr size_t g_cfg_opt_thumbnail_len = sizeof( g_cfg_opt_thumbnail ) / sizeof( config_opt_t );
constexpr size_t g_cfg_opt_theme_len     = sizeof( g_cfg_opt_theme ) / sizeof( config_opt_t );
constexpr size_t g_cfg_opt_gallery_len   = sizeof( g_cfg_opt_gallery ) / sizeof( config_opt_t );
constexpr size_t g_cfg_opt_general_len   = sizeof( g_cfg_opt_general ) / sizeof( config_opt_t );

