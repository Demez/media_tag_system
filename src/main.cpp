#include "main.h"


class CoolSystemInterface : public SystemInterface_SDL
{
  public:

	CoolSystemInterface( SDL_Window* window ) :
		SystemInterface_SDL( window )
	{
	}

	~CoolSystemInterface() = default;
	virtual bool LogMessage( Rml::Log::Type type, const Rml::String& message ) override
	{
		printf( "RmlUi: %s\n", message.c_str() );

		return true;
	}
};


struct ApplicationData
{
	bool        show_text = true;
	Rml::String animal    = "dog";
} my_data;


// General App Data
namespace app
{
	bool                       running        = true;

	SDL_Window*                window         = nullptr;
	bool                       window_focused = false;
	bool                       window_resized = false;
	float                      dpi            = 1.0;

	TextInputMethodEditor_SDL* ime            = nullptr;
	SystemInterface_SDL*       system         = nullptr;
	RenderInterface_GL3*       render         = nullptr;
	Rml::Context*              context        = nullptr;

	// ImVec4                       clear_color = ImVec4( 0.15f, 0.15f, 0.15f, 1.00f );
	// ImVec4       clear_color    = ImVec4( 0.05f, 0.05f, 0.05f, 0.0f );
	// ImVec4       clear_color    = ImVec4( 0.f, 0.f, 0.f, 0.f );
	// ImVec4       clear_color    = ImVec4( 1.f, 1.f, 0.f, 0.f );

	ivec2                      mouse_delta;
	ivec2                      mouse_pos;
	int                        mouse_scroll     = 0;
	bool                       mouse_in_window  = false;

	u32                        draw_frame_count = 0;
	bool                       in_window_drag   = false;
	bool                       in_drag_drop     = false;

	app_config_t               config{};
}


// ImGui Fonts
namespace font
{
}


// Current Working Directory Information
namespace directory
{
	fs::path                     path;
	fs::path                     queued;  // will change to this folder start of next frame
	std::vector< media_entry_t > media_list;

	// the folder path split by path separators
	std::vector< std::string >   path_chunks;
	bool                         path_edit;

	// TODO: get rid of these "thumbnail handles", i don't think it's needed anymore, just use the index in media list
	// and make sure to clear the thumbnail cache when needed
	std::vector< h_thumbnail >   thumbnail_list;

	std::vector< std::string >   media_history;
	std::vector< fs::path >      folder_history;
	size_t                       folder_history_pos;

	bool                         folder_loading = false;
	bool                         folder_reload  = false;
	bool                         folder_changed = false;
	bool                         recursive      = false;
}


// =================================================================================

bool                         g_gallery_view = false;
std::vector< fs::path >      g_drag_drop_files;

// Main Image
main_image_data_t            g_image_data;
main_image_data_t            g_image_scaled_data;

SDL_GLContext                g_gl_context;
bool                         g_in_draw = false;

folder_scan_status_t*        g_main_dir_scan_status = nullptr;

bool                         g_mpv_resume_on_focus  = false;

void                         frame_draw_start();
void                         frame_draw_end();
void                         window_quick_draw();

// =================================================================================


bool delete_file_window( size_t count )
{
	SDL_MessageBoxButtonData buttons[ 2 ]{};
	buttons[ 1 ].buttonID = 1;
	buttons[ 1 ].text     = "Delete";
	buttons[ 1 ].flags    = SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT;

	buttons[ 0 ].buttonID = 2;
	buttons[ 0 ].text     = "Cancel";
	buttons[ 0 ].flags    = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;

	char message[ 512 ]{};

	if ( count > 1 )
	{
		snprintf( message, 512, "Delete %zu Files?", count );
	}
	else
	{
		snprintf( message, 512, "Delete 1 File?" );
	}
	
	SDL_MessageBoxData data{};
	data.flags      = SDL_MESSAGEBOX_WARNING;
	data.buttons    = buttons;
	data.numbuttons = 2;
	data.message    = message;
	data.title      = "Delete Files";
	data.window     = app::window;

	int  buttonid   = 0;
	bool ret        = SDL_ShowMessageBox( &data, &buttonid );

	// Keep them
	if ( !ret || buttonid != 1 )
		return false;

	// Delete the files!
	return true;
}


void set_frame_draw( u32 count )
{
	if ( count > app::draw_frame_count )
		app::draw_frame_count = count;
}


static bool      g_pushed_draw_event = false;
static SDL_Event g_event_draw{};


void send_frame_draw_event()
{
	if ( g_pushed_draw_event )
		return;

	if ( !SDL_PushEvent( &g_event_draw ) )
		printf( "FAILED TO PUSH DRAW EVENT\n" );

	set_frame_draw();
}


void update_window_title()
{
	char buf[ 512 ];

	if ( g_gallery_view )
	{
		snprintf( buf, 512, "Media Tag System [%zu] - %s", gallery::sorted_media.size(), directory::path.string().c_str() );
	}
	else
	{
		if ( gallery::sorted_media.size() >= g_image_data.index )
			snprintf( buf, 512, "Media Tag System [%zu / %zu] - %s", g_image_data.index + 1, gallery::sorted_media.size(), gallery_item_get_path_string( g_image_data.index ).c_str() );
		else
			snprintf( buf, 512, "Media Tag System [%zu]", gallery::sorted_media.size() );
	}

	SDL_SetWindowTitle( app::window, buf );
}


void select_image_in_folder( bool force_load_media )
{
	for ( size_t i = 0; i < gallery::sorted_media.size(); i++ )
	{
		const media_entry_t& entry = gallery_item_get_media_entry( i );

		if ( entry.file.type & e_file_type_directory )
			continue;

		fs::path abs_path = directory::path / entry.file.path;

		if ( abs_path != directory::queued )
			continue;

		gallery_view_set_selection( i );
		g_image_data.index = i;
		break;
	}
}


void folder_media_list_reset()
{
	directory::media_list.clear();
	directory::thumbnail_list.clear();

	gallery_view_reset();
}


void* folder_load_media_list_thread_finish( folder_scan_status_t* status )
{
	if ( !status )
		return nullptr;

	// was this cancelled?
	if ( status->job->cancel )
		return nullptr;

	gallery::scan_state = e_gallery_scan_building;

	send_frame_draw_event();
	set_frame_draw( 2 );

	u64  start_time = sys_get_time_ms();

	auto media_list = new std::vector< media_entry_t >;
	media_list->resize( status->files.size() );

	std::string ext;

	size_t      i = 0;
	for ( file_t& file : status->files )
	{
		//if ( status->job->cancel )
		//{
		//	delete media_list;
		//	return nullptr;
		//}

		e_media_type type = e_media_type_none;
		if ( ( file.type & e_file_type_directory ) )
		{
			type = e_media_type_directory;
		}

		media_entry_t& media_entry = media_list->at( i );
		media_entry.file           = std::move( file );
		media_entry.type           = type;

		//const fs::path_char* filename_ptr = fs_get_filename_ptr( media_entry.file.path.native() );

		//sys_path_to_string( filename_ptr, media_entry.filename );

		if ( !( file.type & e_file_type_directory ) )
		{
			fs_get_extension< char >( media_entry.file.name, ext );
			if ( !media_check_extension_fast( ext, media_entry.type ) )
				continue;
		}

		i++;
	}

	media_list->resize( i );

	u64 end_time = sys_get_time_ms();

	printf( "FILE LIST CHECK TIME: %.4f\n", (float)( end_time - start_time ) / 1000.f );

	return media_list;
}


void folder_load_media_list_finish( folder_scan_status_t* status, bool in_main_thread )
{
	// if ( !in_main_thread )
	//	return;

	if ( !status )
		return;

	// was this cancelled?
	if ( status->job->cancel )
		return;

	// already handled earlier?
	if ( !status->thread_userdata )
		return;

	folder_media_list_reset();

	//media_history_add( status->root );
	folder_history_add( directory::path );

	auto media_entry_list = static_cast< std::vector< media_entry_t >* >( status->thread_userdata );

	// move this list over we created in the worker thread
	directory::media_list = *media_entry_list;
	//directory::media_list = std::move( *media_entry_list );

	delete media_entry_list;
	status->thread_userdata = nullptr;

	directory::thumbnail_list.resize( directory::media_list.size() );

	// select_image_in_folder( false );
	directory::folder_loading = false;

	gallery_view_dir_change( false );

	gallery::item_text_size.resize( directory::media_list.size() );

	dir_tree_add_folder( directory::path );

	g_main_dir_scan_status = nullptr;

	// gallery::scan_state    = e_gallery_scan_idle;
}


void folder_load_media_list()
{
	if ( g_main_dir_scan_status )
	{
		g_main_dir_scan_status->job->cancel = true;
	}

	thumbnail_clear_cache();

	if ( directory::folder_reload )
	{
		//gallery_view_set_selection( gallery::cursor );
	}
	else
	{
		gallery_view_clear_selection();
	}

	folder_media_list_reset();

	directory::media_list.reserve( 5000 );
	directory::thumbnail_list.reserve( 5000 );

	// split into chunks
	directory::path_chunks.clear();

	size_t path_i = 0;
	for ( fs::path::iterator it = directory::path.begin(); it != directory::path.end(); it++ )
	{
		#if _WIN32
		if ( path_i != 1 )
		#endif

		directory::path_chunks.push_back( sys_path_to_string( *it ) );
		path_i++;
	}

	e_scandir_flags scan_flags = 0;

	if ( directory::recursive )
		scan_flags |= e_scandir_recursive | e_scandir_no_dirs;

	set_frame_draw( 2 );

	// queue this directory change
	gallery::scan_state    = e_gallery_scan_filesystem;
	g_main_dir_scan_status = folder_scan_push( directory::path.c_str(), scan_flags, folder_load_media_list_finish, folder_load_media_list_thread_finish );
}

constexpr int MAX_HISTORY = 32;


void media_history_add( const std::string& entry )
{
	if ( directory::media_history.size() > 0 && directory::media_history[ directory::media_history.size() - 1 ] == entry )
		return;

	if ( directory::media_history.size() == MAX_HISTORY )
		directory::media_history.erase( directory::media_history.begin() );

	directory::media_history.push_back( entry );
}


void folder_history_add( const fs::path& entry )
{
	if ( directory::folder_history.size() > 0 && directory::folder_history[ directory::folder_history_pos - 1 ] == entry )
		return;

	if ( directory::folder_history.size() == MAX_HISTORY )
		directory::folder_history.erase( directory::folder_history.begin() );

	// we went back a bit in the history, clear everything after this pos
	if ( directory::folder_history.size() > 0 && directory::folder_history_pos < directory::folder_history.size() )
	{
		directory::folder_history.resize( directory::folder_history_pos );
	}

	directory::folder_history.push_back( entry );
	directory::folder_history_pos++;

	if ( directory::folder_history_pos > directory::folder_history.size() )
		directory::folder_history_pos = directory::folder_history.size();
}


fs::path folder_history_get_prev()
{
	if ( directory::folder_history.empty() || directory::folder_history_pos <= 1 )
		return {};

	return directory::folder_history[ --directory::folder_history_pos ];
}


fs::path folder_history_get_next()
{
	if ( directory::folder_history.empty() || directory::folder_history_pos == directory::folder_history.size() )
		return {};

	return directory::folder_history[ ++directory::folder_history_pos ];
}


bool folder_history_nav_prev()
{
	if ( directory::folder_history.empty() || directory::folder_history_pos <= 1 )
		return false;

	directory::queued = directory::folder_history[ --directory::folder_history_pos - 1 ];
	return true;
}


bool folder_history_nav_next()
{
	if ( directory::folder_history.empty() || directory::folder_history_pos == directory::folder_history.size() )
		return false;

	directory::queued = directory::folder_history[ ++directory::folder_history_pos - 1 ];
	return true;
}


bool on_new_file( const fs::path& file_path )
{
	fs::path    clean_path = fs_path_clean( file_path );
	std::string path_str   = sys_path_to_string( clean_path );
	bool        is_file    = fs_is_file( file_path.c_str() );

	if ( is_file )
	{
		// can we open this file?
		e_media_type type = e_media_type_none;

		if ( !media_check_extension( fs_get_extension( path_str ), type ) )
			return false;

		directory::queued = clean_path;
		return true;
	}
	else if ( fs_is_dir( file_path.c_str() ) )
	{
		directory::queued = clean_path;
		return true;
	}

	return false;
}


bool drag_drop_recieve_func( const std::vector< fs::path >& files )
{
	if ( files.empty() )
		return false;

	if ( !on_new_file( files[ 0 ] ) )
		return false;

	SDL_RaiseWindow( app::window );
	return true;
}


void update_dpi( float dpi_override )
{
	float scale = 0.f;

	if ( dpi_override == 0.f )
	{
		scale = std::max( 0.25f, SDL_GetWindowDisplayScale( app::window ) );
	}
	else
	{
		scale = CLAMP( dpi_override, 0.25f, 5.f );
	}

	app::dpi = scale;

	gallery_view_reset_text_size();
	set_frame_draw();
}


void handle_user_event( SDL_Event& event, bool in_main_thread )
{
	set_frame_draw();

	// just a draw event, something finished
	if ( event.user.code == g_event_draw.user.code )
	{
		return;
	}
	else if ( event.user.code == g_event_job_finish.user.code )
	{
		auto status = static_cast< job_status_t* >( event.user.data1 );

		if ( !status )
			return;

		//printf( "JOB FINISH EVENT - %p\n", status );

		if ( !status->cancel )
		{
			if ( status->callback )
				status->callback( status, in_main_thread );
			else
				printf( "JOB DOES NOT HAVE CALLBACK?\n" );
		}

		if ( in_main_thread )
			job_free( status );
	}
}


bool sdl_window_resize_watcher( void* userdata, SDL_Event* event )
{
	if ( app::in_drag_drop )
		return true;

	if ( SDL_GetWindowFlags( app::window ) & SDL_WINDOW_MINIMIZED )
		return true;

	switch ( event->type )
	{
		case SDL_EVENT_WINDOW_MINIMIZED:
			return true;

		case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
			update_dpi();
			break;

		// Redraw window - Window is being resized
		// NOTE: this is also called when dragging the window around
#ifdef _WIN32
		case SDL_EVENT_WINDOW_EXPOSED:
		{
			app::in_window_drag = true;
			thumbnail_loader_update();
			window_quick_draw();
			app::in_window_drag = false;
			break;
		}
#endif
		case SDL_EVENT_WINDOW_RESIZED:
		{
			app::window_resized = true;
			thumbnail_loader_update();
			window_quick_draw();
			break;
		}

		case SDL_EVENT_USER:
			// handle_user_event( *event, false );
			break;

		default:
			//mpv_sdl_event( *event );
			break;
	}

	return true;
}


bool handle_event( SDL_Event& event )
{
	RmlSDL::InputEventHandler( app::context, app::window, event );

	switch ( event.type )
	{
		default:
			break;

		case SDL_EVENT_MOUSE_BUTTON_DOWN:
			if ( !g_gallery_view )
			{
				if ( event.button.button == SDL_BUTTON_X1 )
				{
					set_view_type_gallery();
				}
			}
			else
			{
				if ( event.button.button == SDL_BUTTON_X1 )
				{
					folder_history_nav_prev();
				}
				else if ( event.button.button == SDL_BUTTON_X2 )
				{
					folder_history_nav_next();
				}
			}

			set_frame_draw( 1 );
			break;

		case SDL_EVENT_MOUSE_BUTTON_UP:
			set_frame_draw( 1 );
			break;

		case SDL_EVENT_KEY_DOWN:
		case SDL_EVENT_KEY_UP:
			set_frame_draw( 1 );
			break;

		case SDL_EVENT_MOUSE_WHEEL:
			set_frame_draw( 2 );
			app::mouse_scroll += event.wheel.integer_y;

			media_view_scroll_zoom( event.wheel.integer_y );
			gallery_view_handle_scroll_event( event.wheel.y );
			break;

		case SDL_EVENT_MOUSE_MOTION:
			app::mouse_pos[ 0 ] = event.motion.x;
			app::mouse_pos[ 1 ] = event.motion.y;
			app::mouse_delta[ 0 ] += event.motion.xrel;
			app::mouse_delta[ 1 ] += event.motion.yrel;
			// set_frame_draw();
			break;

		case SDL_EVENT_WINDOW_MOUSE_ENTER:
			app::mouse_in_window = true;
			break;

		case SDL_EVENT_WINDOW_MOUSE_LEAVE:
			app::mouse_in_window = false;
			break;

#if __unix__
		case SDL_EVENT_WINDOW_EXPOSED:
#endif
		case SDL_EVENT_WINDOW_RESIZED:
			int width, height;
			SDL_GetWindowSize( app::window, &width, &height );
			//ImGui::GetIO().DisplaySize.x = static_cast< float >( width );
			//ImGui::GetIO().DisplaySize.y = static_cast< float >( height );
			//
			//// clear focusing of any windows
			//ImGui::SetNextFrameWantCaptureKeyboard( false );
			//ImGui::SetWindowFocus( nullptr );

			app::window_focused = true;
			app::window_resized = true;
			set_frame_draw();
			media_view_window_resize();
			gallery_view_scroll_to_cursor();
			mpv_window_resize();
			break;

		case SDL_EVENT_WINDOW_FOCUS_GAINED:
		case SDL_EVENT_WINDOW_RESTORED:
		case SDL_EVENT_WINDOW_MAXIMIZED:
			app::window_focused = true;
			set_frame_draw();
			break;

		case SDL_EVENT_WINDOW_FOCUS_LOST:
		case SDL_EVENT_WINDOW_MINIMIZED:
			app::window_focused = false;

			// clear focusing of any windows
			//ImGui::SetNextFrameWantCaptureKeyboard( false );
			//ImGui::SetWindowFocus( nullptr );
			break;

#if !_WIN32
		// The system requests a file open
		case SDL_EVENT_DROP_FILE:
		{
			g_drag_drop_files.push_back( event.drop.data );
			break;
		}

		// text/plain drag-and-drop event
		case SDL_EVENT_DROP_TEXT:
			break;

		// Current set of drops is now complete (NULL filename)
		case SDL_EVENT_DROP_COMPLETE:
		{
			if ( app::in_drag_drop )
				break;

			set_frame_draw();
			if ( drag_drop_recieve_func( g_drag_drop_files ) )
				SDL_RaiseWindow( app::window );

			break;
		}

		// Position while moving over the window
		case SDL_EVENT_DROP_POSITION:
			set_frame_draw();
			break;
#endif

		case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
			update_dpi();
			break;

		case SDL_EVENT_QUIT:
		case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
			set_frame_draw();
			app::running = false;
			return true;

		case SDL_EVENT_USER:
			handle_user_event( event, true );
			break;
		
	}

	return false;
}


// return true to exit main loop
// Handle SDL3 Events
bool handle_events()
{
	app::mouse_scroll     = 0;
	app::window_resized   = false;

	app::mouse_delta[ 0 ] = 0.f;
	app::mouse_delta[ 1 ] = 0.f;

	g_drag_drop_files.clear();

	SDL_Event event;

	SDL_PumpEvents();

	int max_events = SDL_PeepEvents( nullptr, 0, SDL_PEEKEVENT, 0, 0 );

	bool need_draw  = app::config.always_draw || app::draw_frame_count > 0;

	// slam it to false when minimized
	if ( SDL_GetWindowFlags( app::window ) & SDL_WINDOW_MINIMIZED )
		need_draw = false;

	// if there is NOTHING to do, then wait forever until we have a new event, uses basically nothing for cpu usage this way
	if ( max_events == 0 && !need_draw )
	{
		//printf( "WAITING %zu\n", app::total_time );
		if ( SDL_WaitEvent( &event ) )
		{
			//printf( "END WAITING %zu\n", app::total_time );
			if ( handle_event( event ) )
				return true;
		}
	}

	while ( SDL_PollEvent( &event ) )
	{
		if ( handle_event( event ) )
			return true;
	}

	app::in_drag_drop = false;

	return false;
}


void check_need_draw( bool playing_back_video )
{
	if ( SDL_GetWindowFlags( app::window ) & SDL_WINDOW_MINIMIZED )
	{
		app::window_focused   = false;
		app::draw_frame_count = 0;
		return;
	}

	if ( app::config.always_draw )
	{
		set_frame_draw();
		return;
	}

	// Always draw on video playback
	if ( playing_back_video )
		set_frame_draw();
}


static bool check_mpv_playback()
{
	if ( g_gallery_view || gallery::sorted_media.empty() )
		return false;

	media_entry_t entry = gallery_item_get_media_entry( g_image_data.index );

	// if ( entry.type == e_media_type_video /*&& g_mpv_video_ready*/ )
	if ( entry.type != e_media_type_video )
		return false;

	// check mpv state (SHOULD PROBABLY TRY USING OBSERVE PROPERTY)
	s32 paused = 0;
	p_mpv_get_property( g_mpv, "pause", MPV_FORMAT_FLAG, &paused );

	return !paused;
}


static void handle_queued_file()
{
	fs::path path = directory::queued.parent_path();

	if ( path != directory::path || directory::folder_reload )
	{
		directory::path = path;

		if ( !directory::folder_reload )
			gallery_view_clear_selection();

		// create a temporary media entry for showing the image first
		// then, we can scan the directory next frame
		media_entry_t entry{};
		entry.file.path = directory::queued.filename();
		entry.file.name = sys_path_to_string( entry.file.path );

		if ( media_check_extension( fs_get_extension( entry.file.name ), entry.type ) )
		{
			folder_media_list_reset();

			directory::media_list.push_back( entry );
			gallery::sorted_media.push_back( 0 );

			g_image_data.index        = 0;
			g_image_scaled_data.index = 0;

			set_view_type_media( true );

			directory::folder_loading = true;

			// draw the window to show the image NOW
			window_quick_draw();
		}

		// now we can load the files in the directory
		folder_load_media_list();
	}
	// if still running a folder change, wait for the thread to
	else if ( !directory::folder_changed && gallery::scan_state == e_gallery_scan_idle )
	{
		select_image_in_folder( true );
		set_view_type_media( true );

		directory::folder_loading = false;
		directory::folder_reload = false;
		directory::queued.clear();
	}
}


static void handle_queued_folder()
{
	if ( directory::queued != directory::path )
	{
		if ( directory::folder_reload )
		{
			directory::folder_reload = false;
			printf( "NOTE: this is not a folder reload!\n" );
		}

		directory::folder_changed = true;
		// gallery_view_clear_selection();
		directory::path           = directory::queued;

		folder_load_media_list();
	}
	else
	{
		directory::folder_reload = true;
		// gallery::keep_scroll_pos = true;
		// gallery_view_scroll_to_cursor();
		folder_load_media_list();
	}

	// gallery_view_scroll_to_cursor();
	set_view_type_gallery();

	directory::queued.clear();
}


static void check_queued_path()
{
	bool is_file = fs_is_file( directory::queued.c_str() );

	// Reset search string
	memset( gallery::search, 0, 512 * sizeof( char ) );

	if ( is_file )
		handle_queued_file();
	else
		handle_queued_folder();

	update_window_title();
}


void main_loop()
{
	bool   run_after_first_loop_hack = true;

	u64    start_time                = sys_get_time_ms();
	u64    current_time              = start_time;
	double frame_time                = 0.0;

	while ( app::running )
	{
		bool playing_back_video = check_mpv_playback();

		// -----------------------------------------------------------------------------------
		// Update Frame Time

		current_time            = sys_get_time_ms();
		frame_time              = ( current_time / 1000.0 ) - ( start_time / 1000.0 );

		sys_update();

		// -----------------------------------------------------------------------------------
		// Queued Directory/File to Change to/Load

		if ( sys_folder_mon_changed() )
		{
			directory::queued        = directory::path;
			directory::folder_reload = true;
		}

		if ( !directory::queued.empty() )
			check_queued_path();

		thumbnail_loader_update();

		// -----------------------------------------------------------------------------------
		// Window Events

		check_need_draw( playing_back_video );

		if ( handle_events() )
			break;

		if ( SDL_GetWindowFlags( app::window ) & SDL_WINDOW_MINIMIZED )
		{
			start_time = current_time;
			continue;
		}

		if ( !app::window_focused && !playing_back_video )
		{
			if ( app::config.sleep_time_no_focus > 0 )
				SDL_Delay( app::config.sleep_time_no_focus );
		}

		// never called?
		// if ( SDL_GetWindowFlags( app::window ) & SDL_WINDOW_OCCLUDED )
		// {
		// 	printf( "OCCLUDED\n" );
		// 	SDL_Delay( 8 );
		// }

		// -----------------------------------------------------------------------------------

		u32 draw_frame_count = app::draw_frame_count;

		if ( app::draw_frame_count > 0 )
			app::draw_frame_count--;

		frame_draw_start();

		imgui_draw( draw_frame_count );

		media_view_update();

		bool want_text_input = false;

		// if ( draw_frame_count )
			frame_draw_end();

		g_in_draw = false;

		// slow down the app if running really fast, don't need to use all this cpu for rendering
		if ( app::config.vsync == 0 && app::config.sleep_time_focus && frame_time < 0.003 )
		{
			if ( want_text_input || app::config.always_draw )
			{
				SDL_Delay( app::config.sleep_time_focus );
			}
		}

		if ( app::in_window_drag )
			sys_do_window_drag();

		app::in_window_drag = false;

		// -----------------------------------------------------------------------------------

		if ( gallery::scan_state == e_gallery_scan_idle )
		{
			directory::folder_changed = false;
			directory::folder_reload  = false;
		}

		// -----------------------------------------------------------------------------------

		// delayed startup, stuff that can be loaded after initial draw
		// like if we open an image or video from file explorer, we want this program to open and show it near instantly
		// at least the first frame of it, then we can do this after
		if ( run_after_first_loop_hack )
		{
			icon_preload();
			run_after_first_loop_hack = false;
			set_frame_draw();
		}

		start_time = current_time;
	}
}


void shutdown()
{
	config_save();

	if ( app::context )
		Rml::RemoveContext( "main" );

	Rml::Shutdown();

	delete app::system;
	delete app::render;
	delete app::ime;

	render_shutdown();

	if ( app::window )
		SDL_DestroyWindow( app::window );

	app::window = nullptr;

	stop_mpv();

	thumbnail_loader_shutdown();
	job_shutdown();
	sys_folder_mon_shutdown();

	media_view_shutdown();
	icon_free();

	image_free( g_image_data.image );
	image_free( g_image_scaled_data.image );

	args_free();
	sys_shutdown();

	SDL_Quit();
}


void test_hdr_state()
{
	bool HDR_enabled      = false;

	auto props            = SDL_GetWindowProperties( app::window );
	HDR_enabled           = SDL_GetBooleanProperty( props, SDL_PROP_WINDOW_HDR_ENABLED_BOOLEAN, false );

	float sdr_white_level = SDL_GetFloatProperty( props, SDL_PROP_WINDOW_SDR_WHITE_LEVEL_FLOAT, 1.0 );
	float hdr_headroom    = SDL_GetFloatProperty( props, SDL_PROP_WINDOW_HDR_HEADROOM_FLOAT, 1.0 );

	printf( "HDR %s\n", HDR_enabled ? "enabled" : "disabled" );
}


int startup()
{
	if ( !sys_setup_exe_path_vars() )
	{
		printf( "Failed to setup exe path variables!\n" );
		return 1;
	}

	if ( !config_load() )
	{
		printf( "Failed to load config, using defaults\n" );
	}

	e_sys_init sys_init_ret = sys_init();

	if ( sys_init_ret == e_sys_init_fail )
	{
		printf( "Failed to init system backend!\n" );
		return 1;
	}

	if ( sys_init_ret == e_sys_init_single_instance )
		return 0;

	u64 start_time = sys_get_time_ms();

	// RmlUi can handle IME
	SDL_SetHint( SDL_HINT_IME_IMPLEMENTED_UI, "composition" );

	if ( !SDL_Init( SDL_INIT_EVENTS | SDL_INIT_VIDEO ) )
	{
		printf( "Failed to init SDL\n" );
		return 1;
	}

	// Submit click events when focusing the window.
	SDL_SetHint( SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1" );

	// Touch events are handled natively, no need to generate synthetic mouse events for touch devices.
	SDL_SetHint( SDL_HINT_TOUCH_MOUSE_EVENTS, "0" );

	// ----------------------------------------------------------------

	// In windows land, we need our own drag and drop manager so SDL doesn't convert it to UTF-8
	// this can fail thanks to windows supporting evil and fucked up characters in filenames that cannot be converted to UTF-8 seemingly
	// basically characters that are split in half i think
#if _WIN32
	SDL_SetEventEnabled( SDL_EVENT_DROP_FILE, false );
	SDL_SetEventEnabled( SDL_EVENT_DROP_TEXT, false );
	SDL_SetEventEnabled( SDL_EVENT_DROP_BEGIN, false );
	SDL_SetEventEnabled( SDL_EVENT_DROP_COMPLETE, false );
	SDL_SetEventEnabled( SDL_EVENT_DROP_POSITION, false );
#endif

	if ( !render_init() )
	{
		printf( "Failed to setup renderer!\n" );
		return 1;
	}

	if ( !sys_set_window( app::window ) )
	{
		printf( "Failed to set window data on platform backend\n" );
		return 1;
	}

	sys_set_receive_drag_drop_func( drag_drop_recieve_func );

	// ----------------------------------------------------------------
	// RmlUi

	app::system = new CoolSystemInterface( app::window );
	app::render = new RenderInterface_GL3;
	app::ime    = new TextInputMethodEditor_SDL;

	Rml::SetSystemInterface( app::system );
	Rml::SetRenderInterface( app::render );
	Rml::SetTextInputHandler( app::ime );

	// RmlUi initialisation.
	if ( !Rml::Initialise() )
	{
		printf( "Failed to startup RmlUi\n" );
		return 1;
	}

	// Create the main RmlUi context.

	int width, height;
	SDL_GetWindowSize( app::window, &width, &height );

	app::render->SetViewport( width, height );

	app::context = Rml::CreateContext( "main", Rml::Vector2i( width, height ) );
	if ( !app::context )
	{
		Rml::Shutdown();
		return -1;
	}

	fs::path_str font_path = sys_get_exe_folder_native_str();
	font_path += SEP;
	font_path += PATH_FMT( "ui/assets/LatoLatin-Regular.ttf" );

	std::string font_path_str = sys_path_to_string( font_path );

	// Tell RmlUi to load the given fonts.
	bool        font_loaded   = Rml::LoadFontFace( font_path_str );

	Rml::Debugger::Initialise( app::context );

	// Set up data bindings to synchronize application data.
	if ( Rml::DataModelConstructor constructor = app::context->CreateDataModel( "animals" ) )
	{
		constructor.Bind( "show_text", &my_data.show_text );
		constructor.Bind( "animal", &my_data.animal );
	}

	fs::path_str doc_path = sys_get_exe_folder_native_str();
	doc_path += SEP;
	doc_path += PATH_FMT( "ui" SEP_S "data" SEP_S "tutorial.rml" );

	std::string doc_path_str = sys_path_to_string( doc_path );

	// Load and show the tutorial document.
	Rml::ElementDocument* document     = app::context->LoadDocument( doc_path_str );
	if ( document )
		document->Show();

	// ----------------------------------------------------------------

	if ( !load_mpv_dll() )
	{
		printf( "Failed to load MPV\n" );
	}
	else
	{
		if ( !start_mpv() )
			printf( "Failed to start MPV\n" );
	}

	// ----------------------------------------------------------------
	// thread creation

	if ( !thumbnail_loader_init() )
	{
		printf( "Failed to init thumbnail loader\n" );
		return 1;
	}

	if ( !job_init() )
	{
		printf( "Failed to init job threads!\n" );
		return 1;
	}

	media_view_init();

	// ----------------------------------------------------------------
	// Handle File or Directory on the command line

	directory::queued = sys_get_cwd();

	// take the first path here
	for ( int i = 1; i < g_argc; i++ )
	{
		if ( on_new_file( g_argv[ i ] ) )
			break;
	}

	// ----------------------------------------------------------------

//	if ( !SDL_AddEventWatch( sdl_window_resize_watcher, nullptr ) )
//	{
//		printf( "Failed to add SDL Event Watch\n" );
//	}
	
	g_event_draw.type      = SDL_EVENT_USER;
	g_event_draw.user.code = SDL_RegisterEvents( 1 );

	SDL_ShowWindow( app::window );

	window_quick_draw();

	u64    current_time = sys_get_time_ms();
	double time         = ( current_time / 1000.0 ) - ( start_time / 1000.0 );

	printf(
	  "=====================================================================\n"
	  "Image Viewer Started - %.3f Seconds to Start\n"
	  "=====================================================================\n",
	  time );

	return 0;
}


#if _WIN32
int wmain( int argc, wchar_t* argv[] )
#else
int main( int argc, char* argv[] )
#endif
{
	args_init( argc, argv );

	int ret = startup();

	if ( ret != 0 )
	{
		shutdown();
		return ret;
	}

	main_loop();
	shutdown();

	return 0;
}

