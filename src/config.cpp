#include "main.h"
#include "util.h"

#include "libfyaml.h"


//const char* __default_config = {
//#include "config_default.yaml"
//};


#define DEFAULT_THUMBNAIL_CACHE       "$app_path$/thumbnail_cache"
#define DEFAULT_VIDEO_THUMBNAIL_CACHE "$app_path$/thumbnail_video_cache"


static fy_document* config_open( bool saving )
{
	const char* app_dir     = sys_get_exe_folder();

	std::string config_path = app_dir;
	config_path += SEP_S;
	config_path += "config.yaml";

	fy_parse_cfg cfg{};
	cfg.flags        = (fy_parse_cfg_flags)( FYPCF_SLOPPY_FLOW_INDENTATION );

	if ( saving )
	{
		cfg.flags = (fy_parse_cfg_flags)( cfg.flags | FYPCF_PARSE_COMMENTS );
	}

	fy_document* fyd = nullptr;

	if ( fs_is_file( config_path.c_str() ) )
	{
		size_t len    = 0;
		char* buffer = fs_read_file( config_path.c_str(), &len );
		fyd          = fy_document_build_from_string( &cfg, buffer, len );
	}

	if ( !fyd )
	{
		printf( "Failed to open config.yaml, Trying to load config_default.yaml\n" );

		std::string config_path2 = app_dir;
		config_path2 += SEP_S;
		config_path2 += "config_default.yaml";

		if ( fs_is_file( config_path2.c_str() ) )
		{
			size_t len    = 0;
			char*  buffer = fs_read_file( config_path.c_str(), &len );
			fyd           = fy_document_build_from_string( &cfg, buffer, len );
		}

		if ( !fyd )
		{
			printf( "Failed to open config_default.yaml!\n" );
			return nullptr;
		}
	}

	return fyd;
}


void config_parse_path( const char* app_dir, const char* user_path, std::string& result )
{
	if ( !app_dir )
	{
		printf("app_dir is nullptr!\n" );
		return;
	}

	result.clear();

	const char* last = user_path;
	const char* find = strchr( user_path, '$' );
	size_t      path_len = strlen( user_path );

	while ( last )
	{
		// at a macro
		if ( find == last )
		{
			find = strchr( last + 1, '$' );
		}

		size_t dist = 0;
		if ( find )
			dist = ( find - last ) + 1;
		else
			dist = path_len - ( last - user_path );

		if ( dist == 0 )
			break;

		if ( dist == 10 && strncmp( last, "$app_path$", dist ) == 0 )
		{
			result += app_dir;
		}
		else
		{
			std::string tmp( last, dist );
			result += tmp;
		}

		if ( !find )
			break;

		last = ++find;
		find = strchr( last, '$' );
	}

	result = fs_path_clean( result.data(), result.size() );
}


bool config_mkdir( std::string_view path, const char* fail_str )
{
	if ( fs_make_dir_check( path.data() ) )
		return true;

	printf( fail_str );
	return false;
}


static void config_get_bool_value( fy_document* doc, const char* fmt, bool& value )
{
	u32 number = 0;
	int count  = fy_document_scanf( doc, fmt, &number );

	if ( count <= 0 )
		printf( "config: Failed to get value of \"%s\"\n", fmt );
	else
		value = number > 0;
}


template< typename T >
static void config_get_doc_value( fy_document* doc, const char* fmt, T& value )
{
	int count = fy_document_scanf( doc, fmt, &value );

	if ( count <= 0 )
		printf( "config: Failed to get value of \"%s\"\n", fmt );
}


template< typename T >
static void config_get_node_value( fy_node* node, const char* fmt, T& value )
{
	int count = fy_node_scanf( node, fmt, &value );

	if ( count <= 0 )
		printf( "config: Failed to get value of \"%s\"\n", fmt );
}


static bool config_get_node_value_base( fy_node* node, const char* path, const char*& output )
{
	fy_node* node_value = fy_node_by_path( node, path, FY_NT, FYNWF_PTR_DEFAULT );

	if ( !node_value )
	{
		printf( "config: Failed to find \"%s\"\n", path );
		return false;
	}

	if ( !fy_node_is_scalar( node_value ) )
	{
		printf( "config: \"%s\" is not a value!\n", path );
		return false;
	}

	size_t value_len = 0;
	output = fy_node_get_scalar0( node_value );

	return output != nullptr;
}


static bool config_get_node_u32( fy_node* node, const char* path, u32& output )
{
	size_t      value_len  = 0;
	const char* value      = nullptr;

	if ( !config_get_node_value_base( node, path, value ) )
		return false;

	char* end_ptr = nullptr;
	output = strtoul( value, &end_ptr, 10 );

	return true;
}


static bool config_get_node_string( fy_node* node, const char* fmt, char* buffer )
{
	int count = fy_node_scanf( node, fmt, buffer );

	if ( count <= 0 )
	{
		printf( "config: Failed to get value of \"%s\"\n", fmt );
		return false;
	}

	return true;
}


static bool config_get_color( fy_node* node, const char* path, ImVec4& output )
{
	fy_node* color_node = fy_node_by_path( node, path, FY_NT, FYNWF_PTR_DEFAULT );

	if ( !color_node )
	{
		printf( "config: Failed to find \"%s\"\n", path );
		return false;
	}

	fy_node_type color_node_type = fy_node_get_type( color_node );

	if ( color_node_type != FYNT_SEQUENCE )
	{
		printf( "config: Expected Sequence like [0.1, 0.2, 0.5, 1.0] or 0 to 255 values in \"%s\"\n", path );
		return false;
	}

	int item_count = fy_node_sequence_item_count( color_node );

	for ( int item_i = 0; item_i < item_count; item_i++ )
	{
		fy_node*     node_entry = fy_node_sequence_get_by_index( color_node, item_i );
		fy_node_type node_type  = fy_node_get_type( node_entry );

		if ( node_type != FYNT_SCALAR )
			continue;

		const char* string = fy_node_get_scalar0( node_entry );

		if ( strchr( string, '.' ) )
		{
			// Float
			char* end    = nullptr;
			float result = static_cast< float >( strtod( string, &end ) );

			if ( end )
			{
				*( &output.x + item_i ) = result;
			}
		}
		else
		{
			// RGB 0 to 255
			char* end    = nullptr;
			float result = static_cast< float >( strtol( string, &end, 10 ) );

			if ( end )
			{
				*( &output.x + item_i ) = result / 255.f;
			}
		}
	}

	return true;
}


//static void config_get_node_path( fy_node* node, char* app_dir, const char* fmt, std::string& value )
//{
//	char buffer[ 256 ]{};
//
//	int count = fy_node_scanf( node, fmt, buffer );
//
//	if ( count <= 0 )
//	{
//		printf( "config: Failed to get value of \"%s\"\n", fmt );
//		return;
//	}
//
//	config_check_path( app_dir, buffer, value, "Invalid \"%s\"!\n", fmt );
//}


void config_read_bookmarks( fy_document* fyd )
{
	app::config.bookmark.clear();

	fy_node* bookmark_node_list = fy_node_by_path( fy_document_root( fyd ), "/bookmarks", FY_NT, FYNWF_PTR_DEFAULT );

	if ( !bookmark_node_list )
		return;

	int item_count = fy_node_sequence_item_count( bookmark_node_list );

	for ( int item_i = 0; item_i < item_count; item_i++ )
	{
		fy_node*    bookmark_node = fy_node_sequence_get_by_index( bookmark_node_list, item_i );

		size_t      len           = 0;
		const char* string        = fy_node_get_scalar( bookmark_node, &len );

		if ( string )
		{
			bookmark_t bookmark{};
			bookmark.path.assign( string, len );

			if ( fs_is_file( bookmark.path.c_str() ) )
			{
				printf( "config: bookmark points to file, not a directory: \"%s\"\n", string );
				continue;
			}

			bookmark.valid = fs_is_dir( bookmark.path.c_str() );

			if ( !bookmark.valid )
				printf( "config: bookmark does not exist! \"%s\"\n", string );

			char* folder_name = fs_get_filename( string, len );
			bookmark.name.assign( folder_name );
			free( folder_name );

			app::config.bookmark.push_back( bookmark );
		}
		else
		{
			printf( "config: bookmark not a string?\n" );
		}
	}
}


void config_read_thumbnail_settings( fy_document* fyd )
{
	fy_node* thumbnail = fy_node_by_path( fy_document_root( fyd ), "/thumbnail", FY_NT, FYNWF_PTR_DEFAULT );

	if ( !thumbnail )
		return;

	char cache_dir[ 256 ]{};
	char cache_video_dir[ 256 ]{};

	const char* app_dir = sys_get_exe_folder();

	if ( config_get_node_string( thumbnail, "/cache-path %255s", cache_dir ) )
		config_parse_path( app_dir, cache_dir, app::config.thumbnail_cache_path );

	if ( config_get_node_string( thumbnail, "/cache-path-video %255s", cache_video_dir ) )
		config_parse_path( app_dir, cache_video_dir, app::config.thumbnail_video_cache_path );

	config_get_node_u32( thumbnail, "/threads", app::config.thumbnail_threads );
	config_get_node_u32( thumbnail, "/threads-save", app::config.thumbnail_save_threads );

	//config_get_node_value( thumbnail, "/threads %u", app::config.thumbnail_threads );
	config_get_node_value( thumbnail, "/uploads-per-frame %u", app::config.thumbnail_uploads_per_frame );
	config_get_node_value( thumbnail, "/memory-cache-size %u", app::config.thumbnail_mem_cache_size );
	config_get_node_value( thumbnail, "/use-fixed-size %u", app::config.thumbnail_use_fixed_size );
	config_get_node_value( thumbnail, "/jxl-enable %u", app::config.thumbnail_jxl_enable );
	config_get_node_value( thumbnail, "/jxl-effort %u", app::config.thumbnail_jxl_effort );
	config_get_node_value( thumbnail, "/jxl-distance %f", app::config.thumbnail_jxl_distance );
	config_get_node_value( thumbnail, "/size %u", app::config.thumbnail_size );

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


void config_reset()
{
	const char* app_dir = sys_get_exe_folder();

	app::config.bookmark.clear();

	app_config_t reset_config{};
	app::config = reset_config;

	config_parse_path( app_dir, DEFAULT_THUMBNAIL_CACHE, app::config.thumbnail_cache_path );
	config_parse_path( app_dir, DEFAULT_VIDEO_THUMBNAIL_CACHE, app::config.thumbnail_video_cache_path );

	app::config.gallery_header_padding.x = 6;
	app::config.gallery_header_padding.y = 6;
}


bool config_load()
{
	config_reset();

	fy_document* fyd = config_open( false );

	if ( !fyd )
	{
		return false;
	}

	printf( "Reading config\n" );

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
	config_get_bool_value( fyd, "/hdr %u", app::config.hdr );

	config_get_bool_value( fyd, "/directory-tree-auto-expand %u", app::config.directory_tree_auto_expand );
	config_get_bool_value( fyd, "/directory-tree-expand-on-click %u", app::config.directory_tree_expand_on_click );
	config_get_bool_value( fyd, "/directory-tree-simple %u", app::config.directory_tree_simple );

	config_get_doc_value( fyd, "/sleep-time-no-focus %u", app::config.sleep_time_no_focus );
	config_get_doc_value( fyd, "/sleep-time-focus %u", app::config.sleep_time_focus );
	config_get_doc_value( fyd, "/sleep-time-idle %u", app::config.sleep_time_idle );

	config_get_doc_value( fyd, "/font-size %u", app::config.font_size );

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

	if ( args_register_bool( "Disable Video Support", "--no-video" ) )
		app::config.no_video = true;

	// "config: Invalid path for thumbnail/cache-path!\n"

	fy_document_destroy( fyd );

	// Make Directories
	if ( !config_mkdir( app::config.thumbnail_cache_path, "config: Invalid path for thumbnail/cache-path!\n" ) )
		return false;

	if ( !config_mkdir( app::config.thumbnail_video_cache_path, "config: Invalid path for thumbnail/cache-path-video!\n" ) )
		return false;

	gallery::item_size  = std::clamp( app::config.gallery_zoom_default, gallery::item_size_min, gallery::item_size_max );
	gallery::image_size = gallery::item_size;

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


void config_save()
{
	fy_document* doc = config_open( true );

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

	config_save_node_bool( doc, doc_root, "", "single-instance", app::config.single_instance );
	config_save_node_bool( doc, doc_root, "", "no-video", app::config.no_video );
	config_save_node_bool( doc, doc_root, "", "always-draw", app::config.always_draw );
	config_save_node_bool( doc, doc_root, "", "high-bpc", app::config.high_bpc );
	config_save_node_bool( doc, doc_root, "", "hdr", app::config.hdr );
	config_save_node_s32( doc, doc_root, "", "vsync", app::config.vsync );
	config_save_node_u32( doc, doc_root, "", "font-size", app::config.font_size );

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
}

