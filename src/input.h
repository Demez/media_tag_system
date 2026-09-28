#pragma once


// -------------------------------------------------------------------------------------------
// Binding System


enum e_act_gallery
{
	e_act_gallery_invalid,

	e_act_gallery_file_action,
	e_act_gallery_file_select,
	e_act_gallery_file_delete,
	e_act_gallery_file_copy,

	e_act_gallery_nav_left,
	e_act_gallery_nav_right,
	e_act_gallery_nav_up,
	e_act_gallery_nav_down,
	e_act_gallery_nav_scroll_up,
	e_act_gallery_nav_scroll_down,
	e_act_gallery_history_nav_back,
	e_act_gallery_history_nav_forward,
};


enum e_act_media
{
	e_act_media_invalid,
	e_act_media_enter_gallery_view,

	e_act_media_file_action,
	e_act_media_file_select,
	e_act_media_file_delete,
	e_act_media_file_copy,

	e_act_media_nav_left,
	e_act_media_nav_right,
	e_act_media_nav_up,
	e_act_media_nav_down,
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

#if 0

struct bind_t
{
	enum type
	{
		type_key,
		type_joy,

		type_count,
	};

	union
	{

	};
};

#endif

