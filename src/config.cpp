#include "main.h"
#include "util.h"
#include "kdl/kdl.h"

#include <type_traits>

constexpr const char DEFAULT_THUMBNAIL_CACHE[]         = "thumbnail_cache";
constexpr const char DEFAULT_VIDEO_THUMBNAIL_CACHE[]   = "thumbnail_video_cache";

constexpr size_t     DEFAULT_THUMBNAIL_CACHE_LEN       = sizeof( DEFAULT_THUMBNAIL_CACHE );
constexpr size_t     DEFAULT_VIDEO_THUMBNAIL_CACHE_LEN = sizeof( DEFAULT_VIDEO_THUMBNAIL_CACHE );


#define CFG_STR_EQUALS( my_str, kdl_str ) util_strncmp( my_str, sizeof( my_str ) - 1, kdl_str.data, kdl_str.len )
#define CFG_OP_EQUALS( option, kdl_str )  util_strncmp( option.name, option.name_len, kdl_str.data, kdl_str.len )



static void config_close_and_free( kdl_parser*& parser, char*& buffer )
{
	if ( parser )
		kdl_destroy_parser( parser );

	parser = nullptr;
	ch_free( e_mem_category_file_data, buffer );
}


static kdl_parser* _config_open_file( const std::string& config_path, char*& buffer )
{
	size_t len = 0;
	buffer = fs_read_file( config_path.c_str(), &len );

	if ( !buffer )
		return nullptr;

	// kdl_str kdl_buffer = kdl_str_from_cstr( buffer );
	kdl_str     kdl_buffer{ buffer, len };
	kdl_parser* parser = kdl_create_string_parser( kdl_buffer, KDL_EMIT_COMMENTS );

	if ( !parser )
	{
		config_close_and_free( parser, buffer );
		return nullptr;
	}

	return parser;
}


static bool config_open( bool saving, kdl_parser*& root, char*& buffer )
{
	const char* app_dir     = sys_get_exe_folder();

	std::string config_path = app_dir;
	config_path += SEP_S;
	config_path += "config.kdl";

	if ( fs_is_file( config_path.c_str() ) )
		root = _config_open_file( config_path, buffer );

	if ( root )
		return true;

	printf( "Failed to open config.kdl, Trying to load config_default.kdl\n" );

	config_path = app_dir;
	config_path += SEP_S;
	config_path += "config_default.kdl";

	if ( fs_is_file( config_path.c_str() ) )
		root = _config_open_file( config_path, buffer );

	if ( root )
		return true;

	printf( "Failed to open config_default.kdl!\n" );
	return false;
}


void config_parse_path( const char* user_path, size_t user_path_len, std::string& result )
{
	result = fs_path_clean( user_path, user_path_len );
}


bool config_mkdir( std::string_view path, const char* fail_str )
{
	if ( fs_make_dir_check( path.data() ) )
		return true;

	printf( fail_str );
	return false;
}


void config_read_bookmarks( kdl_parser* parser )
{
	app::config.bookmark.clear();

	kdl_event_data* event = kdl_parser_next_event( parser );
	while ( event->event != KDL_EVENT_EOF )
	{
		if ( event->event != KDL_EVENT_START_NODE )
			break;

		bookmark_t bookmark{};
		bookmark.path.assign( event->name.data, event->name.len );

		if ( fs_is_file( bookmark.path.c_str() ) )
		{
			printf( "config: bookmark points to file, not a directory: \"%s\"\n", event->name.data );
			continue;
		}

		bookmark.valid = fs_is_dir( bookmark.path.c_str() );

		if ( !bookmark.valid )
			printf( "config: bookmark does not exist! \"%s\"\n", event->name.data );

		const char* folder_name = fs_get_filename_ptr( event->name.data, event->name.len );
		bookmark.name.assign( folder_name );

		app::config.bookmark.push_back( bookmark );

		event = kdl_parser_next_event( parser );

		if ( event->event == KDL_EVENT_END_NODE )
			event = kdl_parser_next_event( parser );
		else
			break;
	}
}


void config_finish_node( kdl_parser* parser )
{
	kdl_event_data* event = nullptr;
	do
	{
		event = kdl_parser_next_event( parser );
	} while ( event->event != KDL_EVENT_END_NODE );
}


void config_handle_path( kdl_parser* parser, std::string& result )
{
	kdl_event_data* event = kdl_parser_next_event( parser );

	if ( event->event != KDL_EVENT_ARGUMENT )
		return;

	kdl_str& str = event->value.string;
	config_parse_path( str.data, str.len, result );

	config_finish_node( parser );
}


void config_handle_string( kdl_parser* parser, std::string& result )
{
	kdl_event_data* event = kdl_parser_next_event( parser );

	if ( event->event != KDL_EVENT_ARGUMENT )
		return;

	kdl_str& str = event->value.string;
	result.assign( str.data, str.len );

	config_finish_node( parser );
}


template< typename NUM >
void config_get_number_arg( kdl_parser* parser, NUM& result )
{
	static_assert( std::is_arithmetic_v< NUM >, "Template argument NUM must be an arithmetic type!" );
	static_assert( !std::is_same_v< bool, NUM >, "Template argument NUM must not be a bool!" );

	kdl_event_data* event = kdl_parser_next_event( parser );

	if ( event->event != KDL_EVENT_ARGUMENT )
	{
		config_finish_node( parser );
		return;
	}

	if ( event->value.type != KDL_TYPE_NUMBER )
	{
		config_finish_node( parser );
		return;
	}

	kdl_number& num = event->value.number;

	switch ( num.type )
	{
		case KDL_NUMBER_TYPE_INTEGER:
			result = static_cast< NUM >( num.integer );
			break;
		case KDL_NUMBER_TYPE_FLOATING_POINT:
			result = static_cast< NUM >( num.floating_point );
			break;
		case KDL_NUMBER_TYPE_STRING_ENCODED:
			if constexpr ( std::is_unsigned_v< NUM > )
			{
				result = static_cast< NUM >( std::stoul( num.string.data ) );
			}
			else
			{
				result = static_cast< NUM >( std::stol( num.string.data ) );
			}
			break;
	}

	config_finish_node( parser );
}



template< typename NUM >
void config_get_vector_arg( kdl_parser* parser, NUM& result, u32 count )
{
	kdl_event_data* event = kdl_parser_next_event( parser );

	for ( u32 i = 0; i < count; i++ )
	{
		if ( event->value.type != KDL_TYPE_NUMBER )
		{
			config_finish_node( parser );
			return;
		}

		kdl_number& num = event->value.number;

		switch ( num.type )
		{
			case KDL_NUMBER_TYPE_INTEGER:
				result[ i ] = static_cast< float >( num.integer );
				break;
			case KDL_NUMBER_TYPE_FLOATING_POINT:
				result[ i ] = static_cast< float >( num.floating_point );
				break;
			case KDL_NUMBER_TYPE_STRING_ENCODED:
				result[ i ] = static_cast< float >( std::stof( num.string.data ) );
				break;
		}

		event = kdl_parser_next_event( parser );
	}
}


void config_get_bool_arg( kdl_parser* parser, bool& result )
{
	kdl_event_data* event = kdl_parser_next_event( parser );

	if ( event->event != KDL_EVENT_ARGUMENT )
	{
		config_finish_node( parser );
		return;
	}

	if ( event->value.type == KDL_TYPE_BOOLEAN )
		result = event->value.boolean;

	config_finish_node( parser );
}


void config_read_thumbnail_settings( kdl_parser* parser )
{
	kdl_event_data* event = kdl_parser_next_event( parser );
	while ( event->event != KDL_EVENT_EOF )
	{
		if ( event->event == KDL_EVENT_END_NODE )
		{
			event = kdl_parser_next_event( parser );
			break;
		}

		if ( event->event != KDL_EVENT_START_NODE )
		{
			event = kdl_parser_next_event( parser );
			continue;
		}

		if ( CFG_STR_EQUALS( "cache_path", event->name ) )
		{
			config_handle_path( parser, app::config.thumbnail_cache_path );
		}
		else if ( CFG_STR_EQUALS( "cache_path_video", event->name ) )
		{
			config_handle_path( parser, app::config.thumbnail_video_cache_path );
		}
		else if ( CFG_STR_EQUALS( "jxl_distance", event->name ) )
		{
			config_get_number_arg( parser, app::config.thumbnail_jxl_distance );
		}
		else if ( CFG_STR_EQUALS( "jxl_effort", event->name ) )
		{
			config_get_number_arg( parser, app::config.thumbnail_jxl_effort );
		}
		else if ( CFG_STR_EQUALS( "jxl_enable", event->name ) )
		{
			config_get_bool_arg( parser, app::config.thumbnail_jxl_enable );
		}
		else if ( CFG_STR_EQUALS( "memory_cache_size", event->name ) )
		{
			config_get_number_arg( parser, app::config.thumbnail_mem_cache_size );
		}
		else if ( CFG_STR_EQUALS( "size", event->name ) )
		{
			config_get_number_arg( parser, app::config.thumbnail_size );
		}
		else if ( CFG_STR_EQUALS( "threads", event->name ) )
		{
			config_get_number_arg( parser, app::config.thumbnail_threads );
		}
		else if ( CFG_STR_EQUALS( "threads_save", event->name ) )
		{
			config_get_number_arg( parser, app::config.thumbnail_save_threads );
		}
		else if ( CFG_STR_EQUALS( "uploads_per_frame", event->name ) )
		{
			config_get_number_arg( parser, app::config.thumbnail_uploads_per_frame );
		}
		else if ( CFG_STR_EQUALS( "use_fixed_size", event->name ) )
		{
			config_get_bool_arg( parser, app::config.thumbnail_use_fixed_size );
		}

		// advance to the next node
		event = kdl_parser_next_event( parser );
	}

	if ( app::config.thumbnail_threads == 0 )
	{
		printf( "config: Can't have 0 thumbnail threads!\n" );
		app::config.thumbnail_threads = 1;
	}
	else if ( app::config.thumbnail_threads > 32 )
	{
		printf( "config: Not allowing over 32 thumbnail threads! Only 64 thumbnails can be waiting to be loaded in the queue!\n" );
		app::config.thumbnail_threads = 32;
	}

	if ( app::config.thumbnail_uploads_per_frame == 0 )
	{
		printf( "config: an't have 0 thumbnail uploads per frame!\n" );
		app::config.thumbnail_uploads_per_frame = 1;
	}
	else if ( app::config.thumbnail_uploads_per_frame > 64 )
	{
		printf( "config: Not allowing over 64 thumbnail uploads per frame, it can really lock up the program a lot!\n" );
		app::config.thumbnail_threads = 64;
	}

	app::config.thumbnail_jxl_distance = std::clamp( app::config.thumbnail_jxl_distance, -1.f, 25.f );
	app::config.thumbnail_jxl_effort   = std::clamp( app::config.thumbnail_jxl_effort, 0U, 11U );
}


void config_handle_registered_option( const config_opt_t* opt_list, const size_t opt_list_len, kdl_parser* parser, kdl_event_data* event )
{
	for ( size_t i = 0; i < opt_list_len; i++ )
	{
		const config_opt_t& cfg_op = opt_list[ i ];

		if ( !CFG_OP_EQUALS( cfg_op, event->name ) )
			continue;

		// cast to raw byte pointer before applying byte offset
		auto member_ptr = reinterpret_cast< u8* >( &app::config ) + cfg_op.offset;

		switch ( cfg_op.type )
		{
			case e_cfg_bool:
			{
				auto value = reinterpret_cast< bool* >( member_ptr );
				config_get_bool_arg( parser, *value );
				break;
			}
			case e_cfg_u32:
			{
				auto value = reinterpret_cast< u32* >( member_ptr );
				config_get_number_arg( parser, *value );
				break;
			}
			case e_cfg_s32:
			{
				auto value = reinterpret_cast< s32* >( member_ptr );
				config_get_number_arg( parser, *value );
				break;
			}
			case e_cfg_float:
			{
				auto value = reinterpret_cast< float* >( member_ptr );
				config_get_number_arg( parser, *value );
				break;
			}
			case e_cfg_vec2:
			{
				auto value = reinterpret_cast< ImVec2* >( member_ptr );
				config_get_vector_arg( parser, *value, 2 );
				break;
			}
			case e_cfg_vec4:
			{
				auto value = reinterpret_cast< ImVec4* >( member_ptr );
				config_get_vector_arg( parser, *value, 4 );
				break;
			}
			case e_cfg_stdstring:
			{
				auto value = reinterpret_cast< std::string* >( member_ptr );
				config_handle_string( parser, *value );
				break;
			}
		}

		return;
	}

	// not found
	config_finish_node( parser );
}


void config_read_settings_group( kdl_parser* parser, const config_opt_t* opt_list, const size_t opt_list_len )
{
	kdl_event_data* event = kdl_parser_next_event( parser );
	while ( event->event != KDL_EVENT_EOF )
	{
		if ( event->event == KDL_EVENT_END_NODE )
		{
			event = kdl_parser_next_event( parser );
			break;
		}

		if ( event->event != KDL_EVENT_START_NODE )
		{
			event = kdl_parser_next_event( parser );
			continue;
		}

		config_handle_registered_option( opt_list, opt_list_len, parser, event );

		// advance to the next node
		event = kdl_parser_next_event( parser );
	}
}


void config_register()
{
}


bool config_init()
{
	// REGISTER CONFIG OPTIONS

	return true;
}


void config_free()
{
}


void config_reset()
{
	const char* app_dir = sys_get_exe_folder();

	app::config.bookmark.clear();

	app_config_t reset_config{};
	app::config = reset_config;

	std::string path = app_dir;
	path += SEP;
	path.append( DEFAULT_THUMBNAIL_CACHE, DEFAULT_THUMBNAIL_CACHE_LEN );

	config_parse_path( path.c_str(), path.size(), app::config.thumbnail_cache_path );

	path = app_dir;
	path += SEP;
	path.append( DEFAULT_VIDEO_THUMBNAIL_CACHE, DEFAULT_VIDEO_THUMBNAIL_CACHE_LEN );

	config_parse_path( path.c_str(), path.size(), app::config.thumbnail_video_cache_path );

	app::config.gallery_header_padding.x = 6;
	app::config.gallery_header_padding.y = 6;
}


void config_read_document( kdl_parser* parser )
{
	printf( "Reading config\n" );

	kdl_event_data* event = kdl_parser_next_event( parser );

	while ( event->event != KDL_EVENT_EOF )
	{
		if ( event->event != KDL_EVENT_START_NODE )
		{
			event = kdl_parser_next_event( parser );
			continue;
		}

		if ( CFG_STR_EQUALS( "bookmarks", event->name ) )
		{
			config_read_bookmarks( parser );
		}
		else if ( CFG_STR_EQUALS( "thumbnail", event->name ) )
		{
			config_read_thumbnail_settings( parser );
		}
		else if ( CFG_STR_EQUALS( "theme", event->name ) )
		{
			config_read_settings_group( parser, g_cfg_opt_theme, g_cfg_opt_theme_len );
		}
		else if ( CFG_STR_EQUALS( "gallery", event->name ) )
		{
			config_read_settings_group( parser, g_cfg_opt_gallery, g_cfg_opt_gallery_len );
		}
		else if ( CFG_STR_EQUALS( "general", event->name ) )
		{
			config_read_settings_group( parser, g_cfg_opt_general, g_cfg_opt_general_len );
		}
		else
		{
			if ( event->name.data )
				printf( "Unknown Key: %s\n", event->name.data );
		}
	}

#if 0
	for ( size_t root_i = 0; root_i < root.aObjects.count; root_i++ )
	{
		json_object_t& object = root.aObjects.data[ root_i ];

		if ( JSON_STR_EQUALS( "bookmarks", object.aName ) )
		{
			config_read_bookmarks( object );
		}
		else if ( JSON_STR_EQUALS( "thumbnail", object.aName ) )
		{
			config_read_thumbnail_settings( object );
		}
		else if ( JSON_STR_EQUALS( "vsync", object.aName ) )
		{
		}
		else if ( JSON_STR_EQUALS( "no_video", object.aName ) )
		{
		}
		else if ( JSON_STR_EQUALS( "gallery_show_filenames", object.aName ) )
		{
		}
		else if ( JSON_STR_EQUALS( "always_draw", object.aName ) )
		{
		}
		else if ( JSON_STR_EQUALS( "dwm_extend", object.aName ) )
		{
		}
		else if ( JSON_STR_EQUALS( "use_custom_colors", object.aName ) )
		{
		}
		else if ( JSON_STR_EQUALS( "single_instance", object.aName ) )
		{
		}
		else if ( JSON_STR_EQUALS( "high_bpc", object.aName ) )
		{
		}
	}


	config_read_bookmarks( fyd );
	config_read_thumbnail_settings( fyd );

	// =====================================================================================================================
	// General Settings

	config_get_doc_value( fyd, "/vsync %d", app::config.vsync );

	config_get_bool_value( fyd, "/no-video %u", app::config.no_video );
	config_get_bool_value( fyd, "/gallery-show-filenames %u", app::config.gallery_show_filenames );
	config_get_bool_value( fyd, "/always-draw %u", app::config.always_draw );
	config_get_bool_value( fyd, "/dwm-extend %u", app::config.dwm_extend );
	config_get_bool_value( fyd, "/use-custom-colors %u", app::config.use_custom_colors );
	config_get_bool_value( fyd, "/single-instance %u", app::config.single_instance );
	config_get_bool_value( fyd, "/high-bpc %u", app::config.high_bpc );
	//config_get_bool_value( fyd, "/hdr %u", app::config.hdr );
	config_get_bool_value( fyd, "/dev-mode %u", app::config.dev_mode );
	config_get_bool_value( fyd, "/zoom-under-window-size %u", app::config.zoom_under_window_size );

	config_get_bool_value( fyd, "/directory-tree-auto-expand %u", app::config.directory_tree_auto_expand );
	config_get_bool_value( fyd, "/directory-tree-expand-on-click %u", app::config.directory_tree_expand_on_click );
	config_get_bool_value( fyd, "/directory-tree-simple %u", app::config.directory_tree_simple );

	config_get_doc_value( fyd, "/sleep-time-no-focus %u", app::config.sleep_time_no_focus );
	config_get_doc_value( fyd, "/sleep-time-focus %u", app::config.sleep_time_focus );
	config_get_doc_value( fyd, "/sleep-time-idle %u", app::config.sleep_time_idle );

	config_get_doc_value( fyd, "/font-size %u", app::config.font_size );
	config_get_doc_value( fyd, "/job-threads %u", app::config.job_threads );

	config_get_doc_value( fyd, "/gallery-zoom-default %u", app::config.gallery_zoom_default );
	config_get_doc_value( fyd, "/media-zoom-scale %f", app::config.media_zoom_scale );

	config_get_doc_value( fyd, "/gallery-header-padding-x %f", app::config.gallery_header_padding[ 0 ] );
	config_get_doc_value( fyd, "/gallery-header-padding-y %f", app::config.gallery_header_padding[ 1 ] );

	//int media_bg_color[ 4 ]{};
	//int color_count = fy_document_scanf( fyd, "/media-background-color %d %d %d %d", &media_bg_color[ 0 ], &media_bg_color[ 1 ], &media_bg_color[ 2 ], &media_bg_color[ 3 ] );

	config_get_color( fy_document_root( fyd ), "/media-background-color", app::config.media_bg_color );
	config_get_color( fy_document_root( fyd ), "/gallery-header-background-color", app::config.header_bg_color );
	config_get_color( fy_document_root( fyd ), "/gallery-sidebar-bg-color", app::config.sidebar_bg_color );
	config_get_color( fy_document_root( fyd ), "/gallery-content-bg-color", app::config.content_bg_color );

	app::config.vsync = std::clamp( app::config.vsync, -1, 1 );

	// config_write_internal( fyd );

	// "config: Invalid path for thumbnail/cache-path!\n"

	fy_document_destroy( fyd );
#endif
}


bool config_load()
{
	config_reset();

	char*       buffer = nullptr;
	kdl_parser* parser = nullptr;

	if ( config_open( false, parser, buffer ) )
		config_read_document( parser );

	config_close_and_free( parser, buffer );

	if ( args_register_bool( "Disable Video Support", "--no-video" ) )
		app::config.no_video = true;

	// Make Directories
	if ( !config_mkdir( app::config.thumbnail_cache_path, "config: Invalid path for thumbnail/cache-path!\n" ) )
		return false;

	if ( !config_mkdir( app::config.thumbnail_video_cache_path, "config: Invalid path for thumbnail/cache-path-video!\n" ) )
		return false;

	gallery::item_size      = std::clamp( app::config.gallery_zoom_default, gallery::item_size_min, gallery::item_size_max );
	gallery::image_bounds.x = gallery::item_size;
	gallery::image_bounds.y = gallery::item_size;

	return true;
}


// =====================================================================================================================


#if 1


static std::vector< char* > g_cfg_save_str_pool;
//static size_t g_cfg_save_str_pool_size;
//static size_t g_cfg_save_str_pool_capacity;


static void config_save_str_pool_free()
{
	for ( size_t i = 0; i < g_cfg_save_str_pool.size(); i++ )
	{
		ch_free_str( g_cfg_save_str_pool[ i ] );
	}

	g_cfg_save_str_pool.clear();
}


static char* config_save_str_alloc( size_t len_needed )
{
	char* str_num = ch_calloc< char >( len_needed + 1, e_mem_category_string );

	if ( !str_num )
	{
		printf( "Failed to allocate string for saving config file - %zu bytes!" - len_needed + 1 );
		return nullptr;
	}

	g_cfg_save_str_pool.push_back( str_num );
	return str_num;
}


#else


struct str_mem_chunk_t
{
	char*  ptr;
	size_t len;
	size_t remain;
};


static std::vector< str_mem_chunk_t > g_cfg_save_str_pool;
//static size_t g_cfg_save_str_pool_size;
//static size_t g_cfg_save_str_pool_capacity;

constexpr int               CHUNK_SIZE = 512;


static void config_save_str_pool_free()
{
	for ( size_t i = 0; i < g_cfg_save_str_pool.size(); i++ )
	{
		ch_free_str( g_cfg_save_str_pool[ i ].ptr );
	}

	g_cfg_save_str_pool.clear();
}


static char* config_save_str_alloc( size_t len_needed )
{
	// find a valid chunk to add this too
	size_t i = 0;

	// is this a really long string we need?
	if ( len_needed >= CHUNK_SIZE )
		size_t i = g_cfg_save_str_pool.size();

	for (; i < g_cfg_save_str_pool.size(); i++ )
	{
		str_mem_chunk_t& chunk = g_cfg_save_str_pool[ i ];
		if ( chunk.remain > len_needed )
			break;
	}

	if ( i == g_cfg_save_str_pool.size() )
	{

	}

	// allocate in string chunks
	char* str_num = ch_calloc< char >( CHUNK_SIZE, e_mem_category_general );

}
#endif

#if 0
static void config_save_node_scalar_base( fy_document* doc, fy_node* root_node, const char* root, const char* path, const char* value, size_t value_len )
{
	if ( value == nullptr || value_len == 0 )
	{
		printf( "Failed to write value: %s - value is empty!!\n", path );
		return;
	}

	fy_node* node_base  = fy_node_by_path( root_node, path, FY_NT, FYNWF_PTR_DEFAULT );
	fy_node* value_node = fy_node_build_from_string( doc, value, value_len );

	if ( node_base )
	{
		int ret = fy_node_insert( node_base, value_node );

		if ( ret != 0 )
			printf( "failed to write value: %s\n", path );

		return;
	}

	// create a new one
	node_base = fy_node_buildf( doc, "%s: %s", path, value );

	char what_the_fuck[ 256 ]{};
	snprintf( what_the_fuck, 256, "/%s", root );
	int ret = fy_document_insert_at( doc, what_the_fuck, FY_NT, node_base );

	// doesn't work on brand new sequence node creation (/thumbnail:) ??
	//int ret         = fy_node_insert( root_node, node_base );

	if ( ret != 0 )
		printf( "failed to write value: %s\n", path );
}


static void config_save_node_bool( fy_document* doc, fy_node* root_node, const char* root, const char* path, bool value )
{
	//int   size    = 1;
	//char* str_num = config_save_str_alloc( size );
	//snprintf( str_num, 2, "%u", value );
	config_save_node_scalar_base( doc, root_node, root, path, value ? "1" : "0", 1 );
}


static void config_save_node_u32( fy_document* doc, fy_node* root_node, const char* root, const char* path, u32 value )
{
	int size = snprintf( nullptr, 0, "%u", value );
	char* str_num = config_save_str_alloc( size );
	snprintf( str_num, size + 1, "%u", value );

	config_save_node_scalar_base( doc, root_node, root, path, str_num, size );
}


static void config_save_node_s32( fy_document* doc, fy_node* root_node, const char* root, const char* path, s32 value )
{
	int   size    = snprintf( nullptr, 0, "%d", value );
	char* str_num = config_save_str_alloc( size );
	snprintf( str_num, size + 1, "%d", value );

	config_save_node_scalar_base( doc, root_node, root, path, str_num, size );
}


static void config_save_node_float( fy_document* doc, fy_node* root_node, const char* root, const char* path, float value )
{
	int   size    = snprintf( nullptr, 0, "%.6f", value );
	char* str_num = config_save_str_alloc( size );
	snprintf( str_num, size + 1, "%.6f", value );

	config_save_node_scalar_base( doc, root_node, root, path, str_num, size );
}


static void config_save_color( fy_document* doc, fy_node* root_node, const char* root, const char* path, ImVec4 value )
{
	//int num_len[ 4 ]{
	//	snprintf( nullptr, 0, "%.6f", value.x ),
	//	snprintf( nullptr, 0, "%.6f", value.y ),
	//	snprintf( nullptr, 0, "%.6f", value.z ),
	//	snprintf( nullptr, 0, "%.6f", value.w )
	//};

	int   size    = snprintf( nullptr, 0, "[%.6f, %.6f, %.6f, %.6f]", value.x, value.y, value.z, value.w );
	char* str_num = config_save_str_alloc( size );
	snprintf( str_num, size + 1, "[%.6f, %.6f, %.6f, %.6f]", value.x, value.y, value.z, value.w );

	fy_node* node_base  = fy_node_by_path( root_node, path, FY_NT, FYNWF_PTR_DEFAULT );
	fy_node* value_node = fy_node_build_from_string( doc, str_num, size );

	if ( node_base )
	{
		int item_count = fy_node_sequence_item_count( node_base );

		// clear sequence
		for ( int item_i = 0; item_i < item_count; item_i++ )
		{
			fy_node* remove_node = fy_node_sequence_get_by_index( node_base, 0 );
			fy_node_sequence_remove( node_base, remove_node );
		}

		int ret = fy_node_insert( node_base, value_node );

		if ( ret != 0 )
			printf( "failed to write value: %s\n", path );

		return;
	}

	// create a new one
	node_base = fy_node_buildf( doc, "%s: %s", path, value );

	char what_the_fuck[ 256 ]{};
	snprintf( what_the_fuck, 256, "/%s", root );
	int ret = fy_document_insert_at( doc, what_the_fuck, FY_NT, node_base );

	//int ret   = fy_node_insert( root_node, node_base );

	if ( ret != 0 )
		printf( "failed to write value: %s\n", path );
}


fy_node* config_save_get_list_node( fy_document* doc, fy_node* root, const char* path )
{
	fy_node* node = fy_node_by_path( root, path, FY_NT, FYNWF_PTR_DEFAULT );

	if ( node )
		return node;

	const char* name_no_path = path + 1;
	node                     = fy_node_buildf( doc, "%s:", name_no_path );
	// int         ret          = fy_document_insert_at( doc, "/", FY_NT, thumbnail );
	int ret                  = fy_node_insert( root, node );

	if ( ret != 0 )
		printf( "Failed to write \"%s\" node\n", path );

	return node;
}
#endif


void config_save()
{
#if 0
	char*        buffer = nullptr;
	fy_document* doc = config_open( true, buffer );

	if ( !doc )
	{
		// no document found, make a new one
		doc               = fy_document_create( NULL );
		// fy_node* doc_root = fy_node_create_sequence( doc );
		fy_node* doc_root = fy_node_buildf( doc, "version: 1" );
		int      ret      = fy_document_set_root( doc, doc_root );

		if ( ret != 0 )
		{
			printf( "Failed to create root of document for config!\n" );
			return;
		}
	}

	config_save_str_pool_free();

	fy_node* doc_root  = fy_document_root( doc );

	fy_node* bookmarks = config_save_get_list_node( doc, doc_root, "/bookmarks" );

	if ( bookmarks )
	{
		int item_count = fy_node_sequence_item_count( bookmarks );

		// clear bookmarks
		for ( int item_i = 0; item_i < item_count; item_i++ )
		{
			fy_node* bookmark_node = fy_node_sequence_get_by_index( bookmarks, 0 );
			fy_node_sequence_remove( bookmarks, bookmark_node );
		}

		// rebuild list
		for ( const bookmark_t& bookmark : app::config.bookmark )
		{
			fy_node* node = fy_node_buildf( doc, " - %s", bookmark.path.c_str() );
			int      ret  = fy_node_insert( bookmarks, node );

			if ( ret != 0 )
			{
				printf( "Failed to write bookmarks" );
				break;
			}
		}
	}

	fy_node* thumbnail = config_save_get_list_node( doc, doc_root, "/thumbnail" );

	if ( thumbnail )
	{
		config_save_node_u32( doc, thumbnail, "thumbnail", "threads", app::config.thumbnail_threads );
		config_save_node_u32( doc, thumbnail, "thumbnail", "threads-save", app::config.thumbnail_save_threads );
		config_save_node_u32( doc, thumbnail, "thumbnail", "uploads-per-frame", app::config.thumbnail_uploads_per_frame );
		config_save_node_u32( doc, thumbnail, "thumbnail", "memory-cache-size", app::config.thumbnail_mem_cache_size );

		config_save_node_u32( doc, thumbnail, "thumbnail", "size", app::config.thumbnail_size );
		config_save_node_bool( doc, thumbnail, "thumbnail", "use-fixed-size", app::config.thumbnail_use_fixed_size );

		config_save_node_bool( doc, thumbnail, "thumbnail", "jxl-enable", app::config.thumbnail_jxl_enable );
		config_save_node_float( doc, thumbnail, "thumbnail", "jxl-distance", app::config.thumbnail_jxl_distance );
		config_save_node_u32( doc, thumbnail, "thumbnail", "jxl-effort", app::config.thumbnail_jxl_effort );

		config_save_node_scalar_base( doc, thumbnail, "thumbnail", "cache-path", app::config.thumbnail_cache_path.c_str(), app::config.thumbnail_cache_path.size() );
		config_save_node_scalar_base( doc, thumbnail, "thumbnail", "cache-path-video", app::config.thumbnail_cache_path.c_str(), app::config.thumbnail_cache_path.size() );
	}

	//fy_emit_document_to_file( doc, (fy_emitter_cfg_flags)flags, NULL );
	//printf( "\n\n" );

	config_save_node_bool( doc, doc_root, "", "zoom-under-window-size", app::config.zoom_under_window_size );
	config_save_node_bool( doc, doc_root, "", "dev-mode", app::config.dev_mode );
	config_save_node_bool( doc, doc_root, "", "single-instance", app::config.single_instance );
	config_save_node_bool( doc, doc_root, "", "no-video", app::config.no_video );
	config_save_node_bool( doc, doc_root, "", "always-draw", app::config.always_draw );
	config_save_node_bool( doc, doc_root, "", "high-bpc", app::config.high_bpc );
	//config_save_node_bool( doc, doc_root, "", "hdr", app::config.hdr );
	config_save_node_s32( doc, doc_root, "", "vsync", app::config.vsync );
	config_save_node_u32( doc, doc_root, "", "font-size", app::config.font_size );
	config_save_node_u32( doc, doc_root, "", "job-threads", app::config.job_threads );

	config_save_node_bool( doc, doc_root, "", "directory-tree-auto-expand", app::config.directory_tree_auto_expand );
	config_save_node_bool( doc, doc_root, "", "directory-tree-expand-on-click", app::config.directory_tree_expand_on_click );
	config_save_node_bool( doc, doc_root, "", "directory-tree-simple", app::config.directory_tree_simple );

	config_save_node_u32( doc, doc_root, "", "sleep-time-no-focus", app::config.sleep_time_no_focus );
	config_save_node_u32( doc, doc_root, "", "sleep-time-focus", app::config.sleep_time_focus );
	config_save_node_u32( doc, doc_root, "", "sleep-time-idle", app::config.sleep_time_idle );

	config_save_node_float( doc, doc_root, "", "media-zoom-scale", app::config.media_zoom_scale );
	config_save_node_bool( doc, doc_root, "", "gallery-show-filenames", app::config.gallery_show_filenames );
	config_save_node_u32( doc, doc_root, "", "gallery-zoom-default", app::config.gallery_zoom_default );

	config_save_node_float( doc, doc_root, "", "gallery-header-padding-x", app::config.gallery_header_padding[ 0 ] );
	config_save_node_float( doc, doc_root, "", "gallery-header-padding-y", app::config.gallery_header_padding[ 1 ] );

	//int media_bg_color[ 4 ]{};
	//int color_count = fy_document_scanf( doc, "/media-background-color %d %d %d %d", &media_bg_color[ 0 ], &media_bg_color[ 1 ], &media_bg_color[ 2 ], &media_bg_color[ 3 ] );

	config_save_node_bool( doc, doc_root, "", "dwm-extend", app::config.dwm_extend );
	config_save_node_bool( doc, doc_root, "", "use-custom-colors", app::config.use_custom_colors );

	config_save_color( doc, doc_root, "", "media-background-color", app::config.media_bg_color );
	config_save_color( doc, doc_root, "", "gallery-header-background-color", app::config.header_bg_color );
	config_save_color( doc, doc_root, "", "gallery-sidebar-bg-color", app::config.sidebar_bg_color );
	config_save_color( doc, doc_root, "", "gallery-content-bg-color", app::config.content_bg_color );

	constexpr int buf_size         = 1024 * 1024 * 8;
	char*         my_stupid_buffer = ch_calloc< char >( buf_size, e_mem_category_file_data );

	// using anything else crashes this, what a shit library,
	// trying to free it's allocated string for output crashes, trying to directly write to a file or file pointer crashes
	auto          flags            = FYECF_OUTPUT_COMMENTS | FYECF_WIDTH_INF | FYECF_INDENT_DEFAULT | FYECF_MODE_PRETTY | FYECF_SORT_KEYS;
	fy_emit_document_to_buffer( doc, (fy_emitter_cfg_flags)flags, my_stupid_buffer, buf_size );

	fy_document_destroy( doc );

	config_save_str_pool_free();

	std::string config_path = sys_get_exe_folder();
	config_path += SEP;
	config_path += "config.yaml";

	if ( fs_save_file( config_path.c_str(), my_stupid_buffer, strlen( my_stupid_buffer ) ) )
	{
		printf( "Saved config!\n" );
	}

	ch_free( e_mem_category_file_data, my_stupid_buffer );
#endif
}

