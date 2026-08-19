#include "main.h"
#include "util.h"
#include "kdl/kdl.h"

#include <type_traits>

constexpr const fs::path_char DEFAULT_THUMBNAIL_CACHE[]         = PATH_FMT( "thumbnail_cache" );
constexpr const fs::path_char DEFAULT_VIDEO_THUMBNAIL_CACHE[]   = PATH_FMT( "thumbnail_video_cache" );

constexpr size_t              DEFAULT_THUMBNAIL_CACHE_LEN       = sizeof( DEFAULT_THUMBNAIL_CACHE ) / sizeof( fs::path_char );
constexpr size_t              DEFAULT_VIDEO_THUMBNAIL_CACHE_LEN = sizeof( DEFAULT_VIDEO_THUMBNAIL_CACHE ) / sizeof( fs::path_char );


#define CFG_STR_EQUALS( my_str, kdl_str ) util_strncmp( my_str, sizeof( my_str ) - 1, kdl_str.data, kdl_str.len )
#define CFG_OP_EQUALS( option, kdl_str )  util_strncmp( option.name, option.name_len, kdl_str.data, kdl_str.len )
#define CFG_STR_AND_LEN( str )            str, sizeof( str ) - 1


static void config_close_and_free( kdl_parser*& parser, char*& buffer )
{
	if ( parser )
		kdl_destroy_parser( parser );

	parser = nullptr;
	ch_free( e_mem_category_file_data, buffer );
}


static kdl_parser* _config_open_file( const fs::path_str& config_path, char*& buffer )
{
	size_t len = 0;
	buffer     = fs_read_file( config_path.c_str(), &len );

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
	fs::path_str config_path = sys_get_exe_folder_native_str();
	config_path += SEP;
	config_path += PATH_FMT( "config.kdl" );

	if ( fs_is_file( config_path.c_str() ) )
		root = _config_open_file( config_path, buffer );

	if ( root )
		return true;

	printf( "Failed to open config.kdl, Trying to load config_default.kdl\n" );

	config_path = sys_get_exe_folder_native_str();
	config_path += SEP;
	config_path += PATH_FMT( "config_default.kdl" );

	if ( fs_is_file( config_path.c_str() ) )
		root = _config_open_file( config_path, buffer );

	if ( root )
		return true;

	printf( "Failed to open config_default.kdl!\n" );
	return false;
}


bool config_mkdir( const fs::path_str& path, const char* fail_str )
{
	if ( fs_make_dir_check( path.data() ) )
		return true;

	printf( fail_str );
	return false;
}


// return true if error occurred
bool config_check_error( kdl_event_data* event )
{
	if ( event->event != KDL_EVENT_PARSE_ERROR )
		return false;

	printf( "CONFIG ERROR: Error Occurered parsing value!\n" );
	return true;
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
		bookmark.path = sys_string_to_path_str( fs_path_clean( event->name.data, event->name.len ) );

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

		app::config.bookmark.push_back( std::move( bookmark ) );

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


void config_handle_path( kdl_parser* parser, fs::path_str& result )
{
	kdl_event_data* event = kdl_parser_next_event( parser );

	if ( event->event != KDL_EVENT_ARGUMENT )
		return;

	kdl_str&    str  = event->value.string;
	std::string path = fs_path_clean( str.data, str.len );
	result           = sys_string_to_path_str( path );

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
			else if constexpr ( std::is_floating_point_v< NUM > )
			{
				result = static_cast< NUM >( std::stof( num.string.data ) );
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

	config_check_error( event );

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
		config_check_error( event );
	}
}


void config_get_bool_arg( kdl_parser* parser, bool& result )
{
	kdl_event_data* event = kdl_parser_next_event( parser );

	config_check_error( event );

	if ( event->event != KDL_EVENT_ARGUMENT )
	{
		config_finish_node( parser );
		return;
	}

	if ( event->value.type == KDL_TYPE_BOOLEAN )
		result = event->value.boolean;

	config_finish_node( parser );
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
			case e_cfg_color:
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
			case e_cfg_path:
			{
				auto value = reinterpret_cast< fs::path_str* >( member_ptr );
				config_handle_path( parser, *value );
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


void config_reset()
{
	app::config.bookmark.clear();

	app_config_t reset_config{};
	app::config       = reset_config;

	app::config.thumbnail_cache_path = sys_get_exe_folder_native_char();
	app::config.thumbnail_cache_path += SEP;
	app::config.thumbnail_cache_path.append( DEFAULT_THUMBNAIL_CACHE, DEFAULT_THUMBNAIL_CACHE_LEN );

	app::config.thumbnail_video_cache_path = sys_get_exe_folder_native_char();
	app::config.thumbnail_video_cache_path += SEP;
	app::config.thumbnail_video_cache_path.append( DEFAULT_VIDEO_THUMBNAIL_CACHE, DEFAULT_VIDEO_THUMBNAIL_CACHE_LEN );

	app::config.gallery_header_padding.x   = 6;
	app::config.gallery_header_padding.y   = 6;
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
			config_read_settings_group( parser, g_cfg_opt_thumbnail, g_cfg_opt_thumbnail_len );
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

			for ( size_t depth = 1; depth > 0; )
			{
				event = kdl_parser_next_event( parser );

				if ( event->event == KDL_EVENT_END_NODE )
					depth--;
				else if ( event->event == KDL_EVENT_START_NODE )
					depth++;
			}
		}
	}
}


bool config_load()
{
	config_reset();

	char*       buffer = nullptr;
	kdl_parser* parser = nullptr;

	if ( config_open( false, parser, buffer ) )
		config_read_document( parser );

	config_close_and_free( parser, buffer );

	if ( args_register_bool( "Disable Video Support", PATH_FMT( "--no-video" ) ) )
		app::config.no_video = true;

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

	// Make Directories
	if ( !config_mkdir( app::config.thumbnail_cache_path, "config: Invalid path for thumbnail/cache-path!\n" ) )
		return false;

	if ( !config_mkdir( app::config.thumbnail_video_cache_path, "config: Invalid path for thumbnail/cache-path-video!\n" ) )
		return false;

	app::config.gallery_zoom_default = std::clamp( app::config.gallery_zoom_default, gallery::item_size_min, gallery::item_size_max );
	gallery::item_size               = app::config.gallery_zoom_default;
	gallery::image_bounds.x          = gallery::item_size;
	gallery::image_bounds.y          = gallery::item_size;

	app::config.vsync                = std::clamp( app::config.vsync, -1, 1 );
	app::config.sleep_time_no_focus  = std::min( app::config.sleep_time_no_focus, 1000U );
	app::config.sleep_time_focus     = std::min( app::config.sleep_time_focus, 1000U );
	app::config.sleep_time_idle      = std::min( app::config.sleep_time_idle, 1000U );
	app::config.job_threads          = std::clamp( app::config.job_threads, 1U, 8U );

	app::config.sleep_time_idle      = std::min( app::config.job_threads, 1000U );

	app::config.media_zoom_scale     = std::max( 0.01f, app::config.media_zoom_scale );
	app::config.font_size            = std::max( 1U, app::config.font_size );

	return true;
}


// =====================================================================================================================


void config_emit_str( kdl_emitter* emitter, const char* group_name, size_t group_name_len )
{
	kdl_str name( group_name, group_name_len );
	
	if ( !kdl_emit_node( emitter, name ) )
		printf( "Failed to emit string - %s\n", group_name );
}


void config_emit_group( kdl_emitter* emitter, const char* group_name, size_t group_name_len )
{
	config_emit_str( emitter, group_name, group_name_len );
	kdl_start_emitting_children( emitter );
}


void config_emit_registered_option_list( kdl_emitter* emitter, const config_opt_t* opt_list, const size_t opt_list_len )
{
	for ( size_t i = 0; i < opt_list_len; i++ )
	{
		const config_opt_t& cfg_op = opt_list[ i ];
		kdl_str             name( cfg_op.name, cfg_op.name_len );

		kdl_emit_node( emitter, name );

		// cast to raw byte pointer before applying byte offset
		auto member_ptr = reinterpret_cast< u8* >( &app::config ) + cfg_op.offset;
		kdl_value value{};

		switch ( cfg_op.type )
		{
			case e_cfg_bool:
			{
				const auto& var = *reinterpret_cast< bool* >( member_ptr );
				value.type      = KDL_TYPE_BOOLEAN;
				value.boolean   = var;

				kdl_emit_arg( emitter, &value );
				break;
			}
			case e_cfg_u32:
			{
				const auto& var      = *reinterpret_cast< u32* >( member_ptr );
				value.type           = KDL_TYPE_NUMBER;
				value.number.type    = KDL_NUMBER_TYPE_INTEGER;
				value.number.integer = var;

				kdl_emit_arg( emitter, &value );
				break;
			}
			case e_cfg_s32:
			{
				const auto& var      = *reinterpret_cast< s32* >( member_ptr );
				value.type           = KDL_TYPE_NUMBER;
				value.number.type    = KDL_NUMBER_TYPE_INTEGER;
				value.number.integer = var;

				kdl_emit_arg( emitter, &value );
				break;
			}
			case e_cfg_float:
			{
				const auto& var             = *reinterpret_cast< float* >( member_ptr );
				value.type                  = KDL_TYPE_NUMBER;
				value.number.type           = KDL_NUMBER_TYPE_FLOATING_POINT;
				value.number.floating_point = var;

				kdl_emit_arg( emitter, &value );
				break;
			}
			case e_cfg_vec2:
			{
				const auto& var             = *reinterpret_cast< ImVec2* >( member_ptr );
				value.type                  = KDL_TYPE_NUMBER;
				value.number.type           = KDL_NUMBER_TYPE_FLOATING_POINT;

				value.number.floating_point = var.x;
				kdl_emit_arg( emitter, &value );

				value.number.floating_point = var.y;
				kdl_emit_arg( emitter, &value );
				break;
			}
			case e_cfg_vec4:
			case e_cfg_color:
			{
				const auto& var   = *reinterpret_cast< ImVec4* >( member_ptr );
				value.type        = KDL_TYPE_NUMBER;
				value.number.type = KDL_NUMBER_TYPE_FLOATING_POINT;

				for ( u8 v = 0; v < 4; v++ )
				{
					value.number.floating_point = var[ v ];
					kdl_emit_arg( emitter, &value );
				}

				break;
			}
			case e_cfg_stdstring:
			{
				const auto& var   = *reinterpret_cast< std::string* >( member_ptr );
				value.type        = KDL_TYPE_STRING;
				value.string.data = var.data();
				value.string.len  = var.size();

				kdl_emit_arg( emitter, &value );
				break;
			}
			case e_cfg_path:
			{
				const auto& var         = *reinterpret_cast< fs::path_str* >( member_ptr );
				value.type              = KDL_TYPE_STRING;

				fs::path_str clean      = fs_path_clean( var );
				std::string  clean_utf8 = sys_path_to_string( clean );

				value.string.data       = clean_utf8.data();
				value.string.len        = clean_utf8.size();

				kdl_emit_arg( emitter, &value );
				break;
			}
		}
	}
}


void config_save()
{
	kdl_emitter* emitter = kdl_create_buffering_emitter( &KDL_DEFAULT_EMITTER_OPTIONS );

	if ( !emitter )
	{
		printf( "Failed to create kdl emitter\n" );
		return;
	}

	// write bookmarks
	config_emit_group( emitter, CFG_STR_AND_LEN( "bookmarks" ) );

	// rebuild list
	for ( const bookmark_t& bookmark : app::config.bookmark )
	{
		fs::path_str clean      = fs_path_clean( bookmark.path );
		std::string  clean_utf8 = sys_path_to_string( bookmark.path.data() );
		kdl_str      name( clean_utf8.data(), clean_utf8.size() );

		if ( !kdl_emit_node( emitter, name ) )
			printf( "Failed to emit string - %s\n", clean_utf8.data() );
	}

	kdl_finish_emitting_children( emitter );

	config_emit_group( emitter, CFG_STR_AND_LEN( "thumbnail" ) );
	config_emit_registered_option_list( emitter, g_cfg_opt_thumbnail, g_cfg_opt_thumbnail_len );
	kdl_finish_emitting_children( emitter );

	config_emit_group( emitter, CFG_STR_AND_LEN( "theme" ) );
	config_emit_registered_option_list( emitter, g_cfg_opt_theme, g_cfg_opt_theme_len );
	kdl_finish_emitting_children( emitter );

	config_emit_group( emitter, CFG_STR_AND_LEN( "gallery" ) );
	config_emit_registered_option_list( emitter, g_cfg_opt_gallery, g_cfg_opt_gallery_len );
	kdl_finish_emitting_children( emitter );

	config_emit_group( emitter, CFG_STR_AND_LEN( "general" ) );
	config_emit_registered_option_list( emitter, g_cfg_opt_general, g_cfg_opt_general_len );
	kdl_finish_emitting_children( emitter );

	kdl_emit_end( emitter );

	kdl_str      buffer      = kdl_get_emitter_buffer( emitter );
	fs::path_str config_path = sys_get_exe_folder_native_str();
	config_path += SEP;
	config_path += PATH_FMT( "config.kdl" );

	if ( fs_save_file( config_path.c_str(), buffer.data, buffer.len ) )
	{
		printf( "Saved config!\n" );
	}

	kdl_destroy_emitter( emitter );
}

