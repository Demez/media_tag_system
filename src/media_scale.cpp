#include "main.h"

#include <mutex>


// Image Scaling

const char* g_scale_state_str[] = {
	"scale_state_idle",
	"scale_state_start",
	"scale_state_working",
	"scale_state_upload",
	"scale_state_finished",
};

static_assert( ARR_SIZE( g_scale_state_str ) == e_scale_state_count );


// TODO: put these in the config
constexpr float            UPSCALE_LIMIT = 2.f;

static std::thread*        g_scale_thread;
static e_scale_state       g_scale_state  = e_scale_state_idle;
static std::atomic< bool > g_scale_signal = false;
static u64                 g_scale_time   = UINT64_MAX;
static image_t             g_scale_src{};

static std::mutex          g_scale_lock;


// wait X amount of time before scaling the image, the user may be in the middle of changing zoom level quickly, like currently scrolling the mouse wheel
// so it's wasteful to constantly keep scaling it if the scaled image isn't even what the user zoom level is at now
// trying to make sure the user stops changing the zoom level for a certain amount of time before scaling
void                       media_view_scale_thread_wait_timer()
{
	//int recheck_time = 0;
timer_wait:
	g_scale_lock.lock();
	u64 cur_time = sys_get_time_ms();

	if ( cur_time < g_scale_time )
	{
		u64 diff = g_scale_time - cur_time;
		g_scale_lock.unlock();
		//printf( "SCALE WAIT %d\n", ++recheck_time );

		SDL_Delay( std::min( SCALE_WAIT_TIME, diff ) );

		// I use this goto here in case the main thread reset the scale timer once again in between this state
		goto timer_wait;
	}

	g_scale_lock.unlock();
}


void media_view_scale_thread_run()
{
	while ( app::running )
	{
		// wait for queued data here
		g_scale_signal.wait( false );

		if ( !app::running )
			return;

		media_view_scale_thread_wait_timer();

		g_scale_state = e_scale_state_working;

		g_scale_lock.lock();

		// don't scale animated images
		if ( g_scale_src.frame.size() != 1 )
		{
			g_scale_state = e_scale_state_idle;
			g_scale_signal.store( false );
			g_scale_lock.unlock();
			continue;
		}

		image_copy_data( g_scale_src, g_image_scaled_data.image );

		g_image_scaled_data.image.frame.clear();
		g_image_scaled_data.image.frame.resize( g_scale_src.frame.size() );

		image_copy_frame_data( g_scale_src.frame[ 0 ], g_image_scaled_data.image.frame[ 0 ] );

		int draw_width  = static_cast< int >( roundf( image_draw::size.x ) );
		int draw_height = static_cast< int >( roundf( image_draw::size.y ) );

		// Downscale image if size is larger than target size
		if ( image_draw::size.x < ( g_scale_src.width * UPSCALE_LIMIT ) && image_draw::size.x != g_image_data.image.width )
		{
			if ( image_scale( &g_scale_src, &g_image_scaled_data.image, draw_width, draw_height ) )
			{
				g_scale_state = e_scale_state_upload;
				send_frame_draw_event();
			}

			// if ( g_scale_src.width != g_image_data.image.width )
			// 	printf( "scale source is different from currently displayed image!\n" );
		}
		else
		{
			g_scale_state = e_scale_state_idle;
		}

		g_scale_time = UINT64_MAX;
		g_scale_signal.store( false );
		g_scale_lock.unlock();
	}
}


void media_scale_init()
{
	g_scale_thread = new std::thread( media_view_scale_thread_run );
}


void media_scale_shutdown()
{
	if ( !g_scale_thread )
		return;

	// wait for scale thread to shutdown
	g_scale_signal = true;
	g_scale_signal.notify_all();
	g_scale_thread->join();

	delete g_scale_thread;

	image_free_data( g_scale_src );
}


void media_view_scale_fire_thread()
{
	g_scale_time   = sys_get_time_ms() + SCALE_WAIT_TIME;
	g_scale_state  = e_scale_state_start;

	g_scale_signal = true;
	g_scale_signal.notify_one();
}


void media_view_scale_reset_timer()
{
	g_scale_time = sys_get_time_ms() + SCALE_WAIT_TIME;

	if ( g_scale_state == e_scale_state_finished )
		g_scale_state = e_scale_state_idle;

	set_frame_draw( 2 );
}


void media_view_scale_clear()
{
	media_view_scale_reset_timer();
	g_scale_time = SCALE_INVALID_TIME;

	image_free_data( g_scale_src );
	image_free_data( g_image_scaled_data.image );
}


// return true if scale again
bool media_view_scale_handle_finished()
{
	g_scale_time = UINT64_MAX;

	// Are we drawing the image smaller than native size?
	if ( image_draw::size.x < ( g_image_data.image.width * UPSCALE_LIMIT ) || roundf( image_draw::size.x ) != g_image_data.image.width )
	{
		// Does the scaled image size match the size we draw it as?
		if ( roundf( image_draw::size.x ) == g_image_scaled_data.image.width )
			return false;

		// it does not, scale again
		media_view_scale_reset_timer();
		return true;
	}

	return false;
}


void media_view_scale_set_image()
{
	if ( g_image_data.image.frame.empty() || g_scale_state != e_scale_state_idle )
		return;

	// if ( image_draw::size.x >= ( g_image_data.image.width * UPSCALE_LIMIT ) && image_draw::size.x != g_image_data.image.width )
	if ( image_draw::zoom > UPSCALE_LIMIT && image_draw::size.x != g_image_data.image.width )
		return;

	// ????
	if ( !g_image_data.image.frame[ 0 ].data )
		return;

	// don't scale animated images
	if ( g_image_data.image.frame.size() > 1 )
		return;

	g_scale_lock.lock();

	// copy a new image
	// TODO: ref count image_t, so we can avoid copying
	// so when the user switches images during scaling, it still holds on to the previous one thanks to the ref count
	// plus it uses less memory, unless image switching is happening during scaling, but that's a short time frame
	image_free_data( g_scale_src );

	image_copy_data( g_image_data.image, g_scale_src );

	g_scale_src.frame.clear();
	g_scale_src.frame.resize( 1 );

	image_copy_frame_data( g_image_data.image.frame[ 0 ], g_scale_src.frame[ 0 ] );

	// don't hold onto this
	g_scale_src.image_format    = nullptr;

	size_t image_size           = (size_t)g_image_data.image.width * (size_t)g_image_data.image.height * (size_t)g_image_data.image.channels;
	g_scale_src.frame[ 0 ].data = ch_calloc< u8 >( image_size, e_mem_category_image_data );
	memcpy( g_scale_src.frame[ 0 ].data, g_image_data.image.frame[ 0 ].data, image_size * sizeof( u8 ) );

	g_scale_src.frame[ 0 ].size = image_size;
	g_image_scaled_data.index   = g_image_data.index;

	media_view_scale_fire_thread();

	g_scale_lock.unlock();
}


void media_view_scale_check_timer()
{
	if ( g_scale_state != e_scale_state_idle && g_scale_state != e_scale_state_finished )
		return;

	const media_entry_t& entry = gallery_item_get_media_entry( g_image_data.index );

	if ( entry.type == e_media_type_video )
		return;

	// Don't handle animated images for now
	if ( g_image_data.image.frame.size() > 1 )
		return;

	if ( g_scale_state == e_scale_state_finished )
		if ( !media_view_scale_handle_finished() )
			return;

	if ( g_scale_time == UINT64_MAX )
		return;

	// g_scale_timer -= frame_time;
	media_view_scale_set_image();
}


e_scale_state media_get_scale_state()
{
	return g_scale_state;
}


const char* media_get_scale_state_str()
{
	return g_scale_state_str[ g_scale_state ];
}


void media_scale_check_upload()
{
	if ( g_scale_state != e_scale_state_upload )
		return;

	g_scale_src.frame.clear();
	set_frame_draw();

	if ( g_image_scaled_data.index == g_image_data.index )
	{
		gl_update_textures( g_image_scaled_data.textures, &g_image_scaled_data.image, 1 );
		g_scale_state = e_scale_state_finished;
	}
	else
	{
		printf( "SCALE MISMATCH\n" );
		g_scale_state = e_scale_state_idle;
	}
}


void media_scale_lock()
{
	g_scale_lock.lock();
}


void media_scale_unlock()
{
	g_scale_lock.unlock();
}
