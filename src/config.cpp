#include "main.h"
#include "util.h"

#include "libfyaml.h"


#define DEFAULT_THUMBNAIL_CACHE       "$app_path$/thumbnail_cache"
#define DEFAULT_VIDEO_THUMBNAIL_CACHE "$app_path$/thumbnail_video_cache"


static fy_document* config_open( char* app_dir )
{
	std::string config_path = app_dir;
	config_path += SEP_S;
	config_path += "config.yaml";

	fy_parse_cfg cfg{};
	cfg.flags        = (fy_parse_cfg_flags)( FYPCF_PARSE_COMMENTS | FYPCF_SLOPPY_FLOW_INDENTATION );
	fy_document* fyd = nullptr;

	if ( fs_is_file( config_path.c_str() ) )
	{
		fyd = fy_document_build_from_file( &cfg, config_path.c_str() );
	}

	if ( !fyd )
	{
		printf( "Failed to open config.yaml, Trying to load config_default.yaml\n" );

		std::string config_path2 = app_dir;
		config_path2 += SEP_S;
		config_path2 += "config_default.yaml";

		if ( fs_is_file( config_path2.c_str() ) )
		{
			fyd = fy_document_build_from_file( &cfg, config_path2.c_str() );
		}

		if ( !fyd )
		{
			printf( "Failed to open config_default.yaml!\n" );
			return nullptr;
		}
	}

	return fyd;
}


static bool config_write_internal( fy_document* fyd )
{
	printf( "Config:\n" );
	auto flags = FYECF_OUTPUT_COMMENTS | FYECF_DEFAULT | FYECF_MODE_PRETTY;
	// auto flags = FYECF_OUTPUT_COMMENTS | FYECF_WIDTH_INF | FYECF_INDENT_DEFAULT | FYECF_MODE_ORIGINAL | FYECF_MODE_PRETTY;
	// auto flags = FYECF_MODE_PRETTY;
	fy_emit_document_to_file( fyd, (fy_emitter_cfg_flags)flags, NULL );
	printf( "\n\n" );

	return true;
}


void config_parse_path( char* app_dir, const char* user_path, std::string& result )
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


void config_reset()
{
	char* app_dir = sys_get_exe_folder();

	app::config.bookmark.clear();

	app::config.thumbnail_threads           = 8;
	app::config.thumbnail_uploads_per_frame = 16;

	config_parse_path( app_dir, DEFAULT_THUMBNAIL_CACHE, app::config.thumbnail_cache_path );
	config_parse_path( app_dir, DEFAULT_VIDEO_THUMBNAIL_CACHE, app::config.thumbnail_video_cache_path );

	free( app_dir );
}


bool config_load()
{
	char* app_dir = sys_get_exe_folder();

	// Set Defaults
	config_parse_path( app_dir, DEFAULT_THUMBNAIL_CACHE, app::config.thumbnail_cache_path );
	config_parse_path( app_dir, DEFAULT_VIDEO_THUMBNAIL_CACHE, app::config.thumbnail_video_cache_path );

	fy_document* fyd = config_open( app_dir );

	if ( !fyd )
	{
		free( app_dir );
		return false;
	}
	else
	{
		printf( "Reading config\n" );
	}

	// =====================================================================================================================
	// Bookmarks

	app::config.bookmark.clear();

	fy_node* bookmark_node_list = fy_node_by_path( fy_document_root( fyd ), "/bookmarks", FY_NT, FYNWF_PTR_DEFAULT );

	if ( bookmark_node_list )
	{
		int item_count = fy_node_sequence_item_count( bookmark_node_list );

		for ( int item_i = 0; item_i < item_count; item_i++ )
		{
			fy_node*    bookmark_node = fy_node_sequence_get_by_index( bookmark_node_list, item_i );

			size_t      len           = 0;
			const char* string        = fy_node_get_scalar( bookmark_node, &len );

			if ( string )
			{
				if ( fs_is_file( string ) )
				{
					printf( "config: bookmark points to file, not a directory: \"%s\"\n", string );
					continue;
				}

				bookmark_t bookmark{};
				bookmark.valid = fs_is_dir( string );

				if ( !bookmark.valid )
					printf( "config: bookmark does not exist! \"%s\"\n", string );

				bookmark.path.assign( string, len );

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

	// =====================================================================================================================
	// Thumbnail Settings

	fy_node* thumbnail = fy_node_by_path( fy_document_root( fyd ), "/thumbnail", FY_NT, FYNWF_PTR_DEFAULT );

	if ( thumbnail )
	{
		char cache_dir[ 256 ]{};
		char cache_video_dir[ 256 ]{};

		if ( config_get_node_string( thumbnail, "/cache-path %255s", cache_dir ) )
			config_parse_path( app_dir, cache_dir, app::config.thumbnail_cache_path );

		if ( config_get_node_string( thumbnail, "/cache-path-video %255s", cache_video_dir ) )
			config_parse_path( app_dir, cache_video_dir, app::config.thumbnail_video_cache_path );

		config_get_node_u32( thumbnail, "/threads", app::config.thumbnail_threads );

		//config_get_node_value( thumbnail, "/threads %u", app::config.thumbnail_threads );
		config_get_node_value( thumbnail, "/uploads-per-frame %u", app::config.thumbnail_uploads_per_frame );
		config_get_node_value( thumbnail, "/memory-cache-size %u", app::config.thumbnail_mem_cache_size );
		config_get_node_value( thumbnail, "/use-fixed-size %u", app::config.thumbnail_use_fixed_size );
		config_get_node_value( thumbnail, "/jxl-enable %u", app::config.thumbnail_jxl_enable );
		config_get_node_value( thumbnail, "/jxl-effort %u", app::config.thumbnail_jxl_effort );
		config_get_node_value( thumbnail, "/jxl-distance %f", app::config.thumbnail_jxl_distance );

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
			printf( "Cconfig: an't have 0 thumbnail uploads per frame!\n" );
			app::config.thumbnail_uploads_per_frame = 1;
		}
		else if ( app::config.thumbnail_uploads_per_frame > 64 )
		{
			printf( "config: Not allowing over 64 thumbnail uploads per frame, it can really lock up the program a lot!\n" );
			app::config.thumbnail_threads = 64;
		}

		app::config.thumbnail_jxl_distance = std::clamp( app::config.thumbnail_jxl_distance, -1.f, 25.f );
		app::config.thumbnail_jxl_effort   = std::clamp( app::config.thumbnail_jxl_effort, 0U, 11U );
		app::config.thumbnail_jxl_enable   = std::clamp( app::config.thumbnail_jxl_enable, 0U, 1U );
	}

	// =====================================================================================================================

	config_get_doc_value( fyd, "/vsync %d", app::config.vsync );
	config_get_doc_value( fyd, "/no-video %u", app::config.no_video );
	config_get_doc_value( fyd, "/no-focus-sleep-time %u", app::config.no_focus_sleep_time );
	config_get_doc_value( fyd, "/gallery-show-filenames %u", app::config.gallery_show_filenames );

	app::config.vsync = std::clamp( app::config.vsync, -1, 1 );

	config_write_internal( fyd );

	if ( args_register_bool( "Disable Video Support", "--no-video" ) )
		app::config.no_video = true;

	// "config: Invalid path for thumbnail/cache-path!\n"

	free( app_dir );
	fy_document_destroy( fyd );

	// Make Directories
	if ( !config_mkdir( app::config.thumbnail_cache_path, "config: Invalid path for thumbnail/cache-path!\n" ) )
		return false;

	if ( !config_mkdir( app::config.thumbnail_video_cache_path, "config: Invalid path for thumbnail/cache-path-video!\n" ) )
		return false;

	return true;
}


bool config_save()
{
	return false;
}

