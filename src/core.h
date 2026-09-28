#pragma once

#include "util.h"
#include "handles.h"
#include "SDL3/SDL.h"


HANDLE_GEN_32( h_thumbnail );
HANDLE_GEN_32( h_job );


enum e_media_type : u8
{
	e_media_type_none,
	e_media_type_directory,
	e_media_type_image,
	e_media_type_video,

	e_media_type_count,
};

