#include "main.h"


constexpr int SCALAR_OPTION_WIDTH = 192;


void setting_desc( const char* desc )
{
	if ( !desc )
		return;

	//set_frame_draw( ImGui::IsItemHovered() && mouse_moving() );
	set_frame_draw( ImGui::IsItemHovered() );
	ImGui::SetItemTooltip( desc );
}


template< typename NUM_TYPE >
NUM_TYPE setting_scalar_option_base( ImGuiDataType type, const char* format, const char* name, const char* desc, NUM_TYPE base_option, NUM_TYPE range_min, NUM_TYPE range_max, NUM_TYPE step, NUM_TYPE step_fast )
{
	if ( ImGui::InputScalar( name, type, &base_option, (void*)( step > 0 ? &step : NULL ), (void*)( step_fast > 0 ? &step_fast : NULL ), format, ImGuiInputTextFlags_CharsDecimal ) )
	{
		base_option = CLAMP( base_option, range_min, range_max );
	}

	setting_desc( desc );
	return base_option;
}


void setting_u32_option( const char* name, const char* desc, u32& base_option, u32 range_min, u32 range_max, u32 step )
{
	u32 value   = base_option;
	base_option = setting_scalar_option_base( ImGuiDataType_U32, "%d", name, desc, value, range_min, range_max, step, 100U );
}


void setting_float_option( const char* name, const char* desc, float& base_option, float range_min, float range_max, float step )
{
	float value   = base_option;
	base_option = setting_scalar_option_base( ImGuiDataType_Float, "%.6f", name, desc, value, range_min, range_max, step, 100.f );
}


bool setting_bool( const char* name, bool& base_option, const char* desc )
{
	bool ret = ImGui::Checkbox( name, &base_option );
	setting_desc( desc );
	return ret;
}


void settings_draw_vsync()
{
	SDL_GL_GetSwapInterval( &app::config.vsync );

	ImGui::PushItemWidth( SCALAR_OPTION_WIDTH );

	if ( ImGui::Selectable( "VSync OFF", app::config.vsync == 0, 0, { 64.f, ImGui::GetFontSize() } ) )
	{
		if ( app::config.vsync != 0 )
			SDL_GL_SetSwapInterval( 0 );
	}

	ImGui::SameLine();
	ImGui::Spacing();
	ImGui::SameLine();

	if ( ImGui::Selectable( "VSync ON", app::config.vsync == 1, 0, { 64.f, ImGui::GetFontSize() } ) )
	{
		if ( app::config.vsync != 1 )
			SDL_GL_SetSwapInterval( 1 );
	}

	ImGui::SameLine();
	ImGui::Spacing();
	ImGui::SameLine();

	if ( ImGui::Selectable( "VSync Adaptive", app::config.vsync == -1, 0, { 96.f, ImGui::GetFontSize() } ) )
	{
		if ( app::config.vsync != -1 )
			SDL_GL_SetSwapInterval( -1 );
	}

	ImGui::PopItemWidth();
}


void settings_draw_general()
{
	ImGui::Checkbox( "Always Draw", &app::config.always_draw );
	ImGui::Checkbox( "Single Instance Mode", &app::config.single_instance );

	// RESTART NEEDED
	ImGui::Checkbox( "Disable Video Support", &app::config.no_video );
	setting_desc( "RESTART NEEDED: Disables the built in MPV Video Player" );

#if _WIN32
	ImGui::Checkbox( "Windows: DWM Extend", &app::config.dwm_extend );
	setting_desc( "WINDOWS ONLY - RESTART NEEDED: Enabled DWM to extend into the window, useful for mods like DWMBlurGlass" );
#endif

	// CRASH ON CHANGE
	// ImGui::Checkbox( "Use Custom Colors", &app::config.use_custom_colors );

	ImGui::PushItemWidth( SCALAR_OPTION_WIDTH );

	setting_u32_option(
	  "Focused Sleep Time",
	  "Sleep time when app is focused, and running really fast, and vsync is off",
	  app::config.sleep_time_focus,
	  0, 1000, 1 );

	setting_u32_option(
	  "Unfocused Sleep Time",
	  "Sleep time when app is not focused",
	  app::config.sleep_time_no_focus,
	  0, 1000, 1 );

	ImGui::PopItemWidth();

	settings_draw_vsync();
}


void settings_draw_gallery()
{
	if ( ImGui::Checkbox( "Fixed Thumbnail Sizes", &app::config.thumbnail_use_fixed_size ) )
	{
		thumbnail_clear_cache();
	}

	if ( ImGui::Checkbox( "Show Filenames", &app::config.gallery_show_filenames ) )
	{
		gallery_view_reset_text_size();
		gallery_view_scroll_to_cursor();
	}

	ImGui::Checkbox( "Auto Expand Directory Tree", &app::config.directory_tree_auto_expand );
	setting_desc( "Auto expand the directory tree on entering folders" );

	ImGui::Checkbox( "Directory Tree Expand on Click", &app::config.directory_tree_expand_on_click );
	setting_desc( "Auto expand the directory tree on clicking on folders in the tree, instead of clicking the arrow" );

	ImGui::Checkbox( "Simple Directory Tree", &app::config.directory_tree_simple );
	setting_desc( "Simple Directory Tree list, only shows folders in the current folder you're in" );
}


void settings_draw_thumbnails()
{
	setting_bool( "Enable Thumbnail Cache", app::config.thumbnail_jxl_enable, "Saves JPEG XL thumbnails to a folder on your computer, for quick thumbnail loading" );

	ImGui::PushItemWidth( SCALAR_OPTION_WIDTH );

	setting_float_option(
	  "JXL: Distance",
	  "How far away the thumbnail is from the original quality with values closer to 0 being larger but high quality (0 itself equals lossless)",
	  app::config.thumbnail_jxl_distance,
	  0, 1000, 1 );

	setting_u32_option(
	  "JXL: Effort",
	  "Amount of processing power to dedicate towards creating thumbnails",
	  app::config.thumbnail_jxl_effort,
	  0, 1000, 1 );

	setting_u32_option(
	  "Max Thumbnail Size",
	  nullptr,
	  app::config.thumbnail_size,
	  1, 4096, 1 );

	ImGui::Separator();

	// crashing still
	ImGui::BeginDisabled();

	ImGui::PushItemWidth( SCALAR_OPTION_WIDTH );

	setting_u32_option(
	  "Thread Count",
	  "Amount of Threads to use for loading, and generating new thumbnails",
	  app::config.thumbnail_threads,
	  1, 128, 1 );

	setting_u32_option(
	  "Cache Thread Count",
	  "Amount of Threads to use for saving thumbnails to the cache",
	  app::config.thumbnail_save_threads,
	  1, 32, 1 );

	ImGui::PopItemWidth();

	if ( ImGui::Button( "Restart Threads" ) )
	{
		thumbnail_loader_shutdown( false );
		thumbnail_loader_init();
	}

	ImGui::EndDisabled();

	ImGui::Separator();

	setting_u32_option(
	  "Uploads Per Frame",
	  "Amount of thumbnails allowed to upload to the GPU per frame on the main thread\nThis Blocks the main thread to upload, so don't set this number too high",
	  app::config.thumbnail_uploads_per_frame,
	  1, 128, 1 );

	ImGui::PopItemWidth();
}


void settings_draw_debug()
{
	ImGui::PushItemWidth( SCALAR_OPTION_WIDTH );

	static float dpi_scale = 0.f;
	dpi_scale              = app::dpi;
	if ( ImGui::InputFloat( "DPI Override", &dpi_scale, 0.25, 0.5, "%.3f" ) )
	{
		update_dpi( dpi_scale );
	}

	ImGui::PopItemWidth();

	ImGui::Checkbox( "Always Recalc Gallery Item Sizes", &gallery::always_recalc_item_sizes );
	ImGui::Checkbox( "Always Recalc Gallery Item Layout", &gallery::always_recalc_layout );
}


void settings_draw()
{
	ImGui::Text( "%.1f FPS (%.3f ms/frame)", ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate );
	ImGui::Text( "%.8lf Frametime", app::frame_time );

	// ImGui::Text( "App Time: %.3f Sec", app::total_time );
	ImGui::Text( "App Time: %.3f Sec", app::total_time / 1000.f );

	ImGui::SeparatorText( "General" );
	settings_draw_general();

	ImGui::SeparatorText( "Gallery" );
	settings_draw_gallery();

	ImGui::SeparatorText( "Thumbnails" );
	settings_draw_thumbnails();

	ImGui::SeparatorText( "Debug" );
	settings_draw_debug();

	ImGui::Separator();

	ImGui::BeginDisabled();

	if ( ImGui::Button( "Save" ) )
	{
		config_save();
	}

	ImGui::EndDisabled();

	ImGui::SameLine();

	if ( ImGui::Button( "Reload" ) )
	{
		config_reset();
		config_load();
	}

	ImGui::SameLine();

	if ( ImGui::Button( "Reset" ) )
	{
		config_reset();
	}

	// ??? for some reason i need this here
	ImGui::Spacing();
}

