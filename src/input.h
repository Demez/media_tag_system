#pragma once


// -------------------------------------------------------------------------------------------
// Binding System


enum e_mouse_btn
{
	e_mouse_btn_left,
	e_mouse_btn_middle,
	e_mouse_btn_right,

	e_mouse_btn_x1,
	e_mouse_btn_x2,

	e_mouse_scroll_up,
	e_mouse_scroll_down,
	e_mouse_scroll_left,
	e_mouse_scroll_right,
};


enum e_act_list
{
	e_act_list_core,
	e_act_list_gallery,
	e_act_list_media,
};


using e_act_id = s32;

using e_act_core = e_act_id;
using e_act_gallery = e_act_id;
using e_act_media = e_act_id;


enum : e_act_core
{
	e_act_core_invalid = 0,
	e_act_core_toggle_view,

	e_act_core_select,
	e_act_core_delete,
	e_act_core_copy,

	e_act_core_nav_left,
	e_act_core_nav_right,
	e_act_core_nav_up,
	e_act_core_nav_down,
	e_act_core_nav_scroll_up,
	e_act_core_nav_scroll_down,
};


enum : e_act_gallery
{
	e_act_gallery_invalid = e_act_core_invalid,
	e_act_media_enter_media_view,

	e_act_gallery_history_nav_back,
	e_act_gallery_history_nav_forward,
};


enum : e_act_media
{
	e_act_media_invalid = e_act_core_invalid,
	e_act_media_enter_gallery_view,

	e_act_media_history_nav_back,
	e_act_media_history_nav_forward,

	e_act_media_image_zoom_in,
	e_act_media_image_zoom_out,
	e_act_media_image_zoom_reset,
	e_act_media_image_zoom_fit,
	e_act_media_image_zoom_fit_window,

	e_act_media_toggle_playback,
	e_act_media_video_volume_up,
	e_act_media_video_volume_down,
	e_act_media_video_volume_mute,
};


using e_keystate = s8;

enum : e_keystate
{
	e_keystate_just_released = -1,
	e_keystate_released      = 0,
	e_keystate_pressed       = 1,
	e_keystate_just_pressed  = 2,
};


#if 0

struct bind_t
{
	enum e_device_type
	{
		e_device_type_key,
		e_device_type_controller,

		e_device_type_count,
	};

	union
	{

	};
};

#endif


bool input_init();
void input_shutdown();

void input_handle_event( const SDL_Event& event );

void input_set_action_list( e_act_list list );

