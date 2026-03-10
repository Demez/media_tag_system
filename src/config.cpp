#include "main.h"
#include "util.h"

#include "libfyaml.h"


constexpr const char* DEFAULT_THUMBNAIL_CACHE = "$app_path$/thumbnail_cache";
constexpr const char* DEFAULT_VIDEO_THUMBNAIL_CACHE = "$app_path$/thumbnail_video_cache";


static fy_document* config_open( char* app_dir )
{
	std::string config_path = app_dir;
	config_path += SEP_S;
	config_path += "config.yaml";

	fy_parse_cfg cfg{};
	cfg.flags        = (fy_parse_cfg_flags)( FYPCF_PARSE_COMMENTS | FYPCF_SLOPPY_FLOW_INDENTATION );

	fy_document* fyd = fy_document_build_from_file( &cfg, config_path.c_str() );

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


void config_reset()
{
	app::config.bookmark.clear();

	app::config.thumbnail_threads           = 8;
	app::config.thumbnail_uploads_per_frame = 4;
}


bool config_check_path( char* app_dir, const char* user_path, std::string& result, const char* fail_str )
{
	std::string output{};

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
			output += app_dir;
		}
		else
		{
			std::string tmp( last, dist );
			output += tmp;
		}

		if ( !find )
			break;

		last = ++find;
		find = strchr( last, '$' );
	}

	// std::string path = app_dir;
	// path += SEP_S;
	// path += user_path;

	if ( fs_make_dir_check( output.c_str() ) )
	{
		result = output;
		return true;
	}

	printf( fail_str );
	return false;
}


bool config_load()
{
	char*        app_dir = sys_get_exe_folder();

	fy_document* fyd = config_open( app_dir );

	if ( !fyd )
	{
		printf( "Failed to open config.yaml!\n" );
		free( app_dir );
		return false;
	}

	app::config.bookmark.clear();

	fy_node* bookmark_node_list = fy_node_by_path( fy_document_root( fyd ), "/bookmarks", FY_NT, FYNWF_DONT_FOLLOW );

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
					printf( "bookmark points to file, not a directory: \"%s\"\n", string );
					continue;
				}

				if ( fs_is_dir( string ) )
				{
					std::string bookmark_str( string, len );
					char*       folder_name = fs_get_filename( string, len );
					std::string bookmark_name = folder_name;
					free( folder_name );

					app::config.bookmark.emplace_back( bookmark_name, bookmark_str );
				}
				else
				{
					printf( "bookmark does not exist! \"%s\"\n", string );
				}
			}
			else
			{
				printf( "bookmark not a string?\n" );
			}
		}
	}

	char         cache_dir[ 256 ]{};
	char         cache_video_dir[ 256 ]{};

	int          count = fy_document_scanf( fyd,
	                                        "/thumbnail-threads %u "
	                                                 "/thumbnail-uploads-per-frame %u "
	                                                 "/thumbnail-memory-cache-size %u "
	                                                 "/vsync %d "
	                                                 "/no-focus-sleep-time %u "
	                                                 "/thumbnail-cache-path %255s "
	                                                 "/thumbnail-video-cache-path %255s",
	                                        &app::config.thumbnail_threads,
	                                        &app::config.thumbnail_uploads_per_frame,
	                                        &app::config.thumbnail_mem_cache_size,
	                                        &app::config.vsync,
	                                        &app::config.no_focus_sleep_time,
	                                        cache_dir,
	                                        cache_video_dir );

	if ( app::config.thumbnail_threads == 0 )
	{
		printf( "Can't have 0 thumbnail threads!\n" );
		app::config.thumbnail_threads = 1;
	}
	else if ( app::config.thumbnail_threads > 32 )
	{
		printf( "Not allowing over 32 thumbnail threads! Only 64 thumbnails can be waiting to be loaded in the queue!\n" );
		app::config.thumbnail_threads = 32;
	}

	if ( app::config.thumbnail_uploads_per_frame == 0 )
	{
		printf( "Can't have 0 thumbnail uploads per frame!\n" );
		app::config.thumbnail_uploads_per_frame = 1;
	}
	else if ( app::config.thumbnail_uploads_per_frame > 64 )
	{
		printf( "Not allowing over 64 thumbnail uploads per frame, it can really lock up the program a lot!\n" );
		app::config.thumbnail_threads = 64;
	}

	app::config.vsync = std::clamp( app::config.vsync, -1, 1 );

	if ( cache_dir[ 0 ] )
	{
		config_check_path( app_dir, cache_dir, app::config.thumbnail_cache_path, "Invalid thumbnail-cache-path!\n" );
	}

	if ( cache_video_dir[ 0 ] )
	{
		config_check_path( app_dir, cache_video_dir, app::config.thumbnail_video_cache_path, "Invalid thumbnail-video-cache-path!\n" );
	}

	// Defaults

	if ( app::config.thumbnail_cache_path.empty() )
	{
		if ( !config_check_path( app_dir, DEFAULT_THUMBNAIL_CACHE, app::config.thumbnail_cache_path, "Can't use fallback thumbnail-cache-path!\n" ) )
		{
			free( app_dir );
			fy_document_destroy( fyd );
			return false;
		}
	}

	if ( app::config.thumbnail_video_cache_path.empty() )
	{
		if ( !config_check_path( app_dir, DEFAULT_VIDEO_THUMBNAIL_CACHE, app::config.thumbnail_video_cache_path, "Can't use fallback thumbnail-video-cache-path!\n" ) )
		{
			free( app_dir );
			fy_document_destroy( fyd );
			return false;
		}
	}

	config_write_internal( fyd );

	free( app_dir );
	fy_document_destroy( fyd );

	return true;
}


bool config_save()
{
	return false;
}

