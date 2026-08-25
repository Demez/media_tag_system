#include "main.h"
#include "ui_main.h"

// =================================================================================

extern bool g_mpv_resume_on_focus;

// =================================================================================

struct notification_t
{
	std::string msg;
	u64         time_added;
};

constexpr u64                 NOTIFICATION_DURATION  = 5000;
constexpr double              NOTIFICATION_FADE_TIME = 0.5;
constexpr size_t              NOTIFICATION_MAX_SHOWN = 5;

std::vector< notification_t > g_notification_queue;

// =================================================================================

class UiSystemInterface : public SystemInterface_SDL
{
  public:
	UiSystemInterface( SDL_Window* window ) :
		SystemInterface_SDL( window )
	{
	}

	~UiSystemInterface() = default;
	virtual bool LogMessage( Rml::Log::Type type, const Rml::String& message ) override
	{
		printf( "RmlUi: %s\n", message.c_str() );

		return true;
	}
};

// =================================================================================
// File Interface
// overridden to load all files from the app folder


class UiFileInterface final : public Rml::FileInterface
{
	/// Opens a file.
	/// @param path The path to the file to open.
	/// @return A valid file handle, or nullptr on failure
	virtual Rml::FileHandle Open( const Rml::String& path ) override
	{
		if ( fs_is_relative( path.c_str(), path.size() ) )
		{
			// everything for rmlui is local to the ui folder in the app directory
			std::string full_path = sys_get_exe_folder();
			full_path += SEP_S "ui" SEP_S;
			full_path += path;

			return (Rml::FileHandle)fopen( full_path.c_str(), "rb" );
		}

		return (Rml::FileHandle)fopen( path.c_str(), "rb" );
	}

	/// Closes a previously opened file.
	/// @param file The file handle previously opened through Open().
	virtual void            Close( Rml::FileHandle file ) override
	{
		fclose( (FILE*)file );
	}

	/// Reads data from a previously opened file.
	/// @param buffer The buffer to be read into.
	/// @param size The number of bytes to read into the buffer.
	/// @param file The handle of the file.
	/// @return The total number of bytes read into the buffer.
	virtual size_t Read( void* buffer, size_t size, Rml::FileHandle file ) override
	{
		return fread( buffer, 1, size, (FILE*)file );
	}

	/// Seeks to a point in a previously opened file.
	/// @param file The handle of the file to seek.
	/// @param offset The number of bytes to seek.
	/// @param origin One of either SEEK_SET (seek from the beginning of the file), SEEK_END (seek from the end of the file) or SEEK_CUR (seek from
	/// the current file position).
	/// @return True if the operation completed successfully, false otherwise.
	virtual bool Seek( Rml::FileHandle file, long offset, int origin ) override
	{
		return fseek( (FILE*)file, offset, origin ) == 0;
	}

	/// Returns the current position of the file pointer.
	/// @param file The handle of the file to be queried.
	/// @return The number of bytes from the origin of the file.
	virtual size_t Tell( Rml::FileHandle file ) override
	{
		return ftell( (FILE*)file );
	}
};


// =================================================================================
// Base Data Models

struct ApplicationData
{
	bool        show_text  = true;
	Rml::String animal     = "dog";
	Rml::String file_count = "0 Files";
} my_data;


// =================================================================================

void push_notification( const char* msg )
{
	g_notification_queue.emplace_back( msg, sys_get_time_ms() );
	printf( "NOTIFICATION: %.4f - %s\n", sys_get_time_ms() * 1000.f, msg );
}


void notification_draw()
{
	static bool fade_in = true;

	if ( g_notification_queue.empty() )
	{
		fade_in = true;
		return;
	}

	u64 current_time = sys_get_time_ms();

	// find expired ones first
	for ( size_t i = 0; i < g_notification_queue.size(); )
	{
		notification_t& notif = g_notification_queue[ i ];

		if ( notif.time_added + NOTIFICATION_DURATION > current_time )
		{
			i++;
			continue;
		}

		g_notification_queue.erase( g_notification_queue.begin() + i );
	}

	// check if empty again
	if ( g_notification_queue.empty() )
	{
		fade_in = true;
		return;
	}

	// draw last few notifications
	// set_frame_draw();
}


void imgui_draw( bool render )
{
	if ( g_gallery_view )
	{
		char buf[ 256 ]{};
		snprintf( buf, 256, "%zu File%s", gallery::sorted_media.size(), gallery::sorted_media.size() == 1 ? "" : "s" );
		my_data.file_count = buf;

		gallery_view_draw();
	}
	else
	{
		media_view_draw_imgui();
	}

	notification_draw();

	//if ( render )
	//	ImGui::Render();
	//else
	//	ImGui::EndFrame();
}


void view_type_toggle()
{
	if ( g_gallery_view )
	{
		set_view_type_media();
	}
	else
	{
		set_view_type_gallery();
	}
}


void set_view_type_gallery()
{
	if ( g_gallery_view )
		return;

	// pause video if playing one back
	if ( mpv_get_current_video() )
	{
		s32 paused = 0;
		p_mpv_get_property( g_mpv, "pause", MPV_FORMAT_FLAG, &paused );
		g_mpv_resume_on_focus = !paused;

		const char* cmd[]     = { "set", "pause", "yes", NULL };
		int         cmd_ret   = p_mpv_command_async( g_mpv, 0, cmd );
	}

	gallery_view_scroll_to_cursor();

	g_gallery_view = true;

	update_window_title();
	set_frame_draw( 3 );
}


void set_view_type_media( bool force_load_media )
{
	//if ( !g_gallery_view )
	//	return;

	u32 selected = gallery_view_get_last_selected_index();

	if ( directory::folder_loading )
		g_image_data.index = selected;

	bool reload_image = force_load_media;
	reload_image |= g_image_data.index != selected;
	reload_image |= directory::folder_reload;

	if ( reload_image )
	{
		g_image_data.index = selected;
		media_view_load();
	}

	if ( g_mpv_resume_on_focus )
	{
		const char* cmd[]   = { "set", "pause", "no", NULL };
		int         cmd_ret = p_mpv_command_async( g_mpv, 0, cmd );
	}

	g_gallery_view = false;

	update_window_title();
	set_frame_draw( 3 );
}

// =================================================================================

const char* g_ui_document_paths[] = {
	"data" SEP_S "tutorial.rml",
};

constexpr size_t      g_ui_document_count = ARR_SIZE( g_ui_document_paths );
Rml::ElementDocument* g_ui_documents[ g_ui_document_count ];


bool ui_load_documents()
{
	for ( size_t i = 0; i < g_ui_document_count; i++ )
	{
		g_ui_documents[ i ] = ui::context->LoadDocument( g_ui_document_paths[ i ] );
		if ( !g_ui_documents[ i ] )
		{
			printf( "Failed to Load Document: %s\n", g_ui_document_paths[ i ] );
			return false;
		}

		g_ui_documents[ i ]->Show();
	}

	return true;
}


void ui_close_documents()
{
	for ( size_t i = 0; i < g_ui_document_count; i++ )
	{
		if ( g_ui_documents[ i ] )
			g_ui_documents[ i ]->Close();

		g_ui_documents[ i ] = nullptr;
	}
}


void ui_reload()
{
	ui_close_documents();

	Rml::Factory::ClearStyleSheetCache();
	Rml::Factory::ClearTemplateCache();

	if ( ui_load_documents() )
	{
		for ( size_t i = 0; i < g_ui_document_count; i++ )
		{
			// g_ui_documents[ i ]->ReloadStyleSheet();
		}
	}

	printf( "DOCUMENTS RELOADED\n" );
}


bool ui_load_datamodels()
{
	// Set up data bindings to synchronize application data.
	if ( Rml::DataModelConstructor constructor = ui::context->CreateDataModel( "animals" ) )
	{
		constructor.Bind( "show_text", &my_data.show_text );
		constructor.Bind( "animal", &my_data.animal );
		constructor.Bind( "file_count", &my_data.file_count );
	}

	if ( !ui_load_datamodels_gallery() )
		return false;

	return true;
}


void load_default_font( sys_font_data_t& font_data, bool load_symbols )
{
	bool font_loaded = Rml::LoadFontFace( font_data.font_path, "Default", Rml::Style::FontStyle::Normal, Rml::Style::FontWeight::Auto );

#ifdef _WIN32
	// Japanese Characters
	Rml::LoadFontFace( "C:\\Windows\\Fonts\\YuGothM.ttc", true );

	// Symbols/Emoji's
	if ( load_symbols )
	{
		// Segoe UI Symbol
		Rml::LoadFontFace( "C:\\Windows\\Fonts\\seguisym.ttf", true );
		Rml::LoadFontFace( "C:\\Windows\\Fonts\\seguiemj.ttf", true );
	}
#endif
}


bool ui_init()
{
	ui::system  = new UiSystemInterface( app::window );
	ui::render  = new RenderInterface_GL3;
	ui::ime     = new TextInputMethodEditor_SDL;
	ui::filesys = new UiFileInterface;

	Rml::SetSystemInterface( ui::system );
	Rml::SetRenderInterface( ui::render );
	Rml::SetTextInputHandler( ui::ime );
	Rml::SetFileInterface( ui::filesys );

	// RmlUi initialisation.
	if ( !Rml::Initialise() )
	{
		printf( "Failed to startup RmlUi\n" );
		return false;
	}

	// Create the main RmlUi context.

	int width, height;
	SDL_GetWindowSize( app::window, &width, &height );

	ui::render->SetViewport( width, height );

	ui::context = Rml::CreateContext( "main", Rml::Vector2i( width, height ) );
	if ( !ui::context )
	{
		Rml::Shutdown();
		return false;
	}

	sys_font_data_t font_data = sys_get_font();

	if ( font_data.font_path )
	{
		font_data.height = static_cast< float >( app::config.font_size );
		load_default_font( font_data, true );
	}

	Rml::Debugger::Initialise( ui::context );

	if ( !ui_load_datamodels() )
	{
		printf( "Failed to load RmlUi data models!\n" );
		return false;
	}

	if ( !ui_load_documents() )
	{
		printf( "Failed to load RmlUi documents!\n" );
		return false;
	}

	return true;
}

