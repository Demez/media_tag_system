#pragma once


#include "mpv/client.h"
#include "mpv/render.h"
#include "mpv/render_gl.h"


// --------------------------------------------------------------------------------------------------------
// MPV

bool                       load_mpv_dll();
void                       unload_mpv_dll();

bool                       start_mpv();
void                       stop_mpv();

void                       mpv_draw_frame();

void                       mpv_window_resize();
char*                      mpv_get_current_video();

void                       mpv_cmd_loadfile( const char* file );
void                       mpv_cmd_toggle_playback();
void                       mpv_cmd_seek_offset( double seconds );

void                       mpv_cmd_hook_window( void* window );
void                       mpv_cmd_hook_window_mpv();

void                       mpv_handle_error();

extern mpv_handle*         g_mpv;
extern mpv_render_context* g_mpv_gl;
extern bool                g_wakeup_on_mpv_render_update, g_wakeup_on_mpv_events;


// --------------------------------------------------------------------------------------------------------
// mpv function pointer typedefs, cause im not compiling the mpv source just to link to it

#define MPV_PTR( return_type, func, ... ) \
	typedef return_type ( *func##_t )( __VA_ARGS__ ); \
	extern func##_t p_##func

// client.h
MPV_PTR( unsigned long, mpv_client_api_version, void );
MPV_PTR( const char*,   mpv_error_string, int error );
MPV_PTR( void,          mpv_free, void* data );
MPV_PTR( const char*,   mpv_client_name, mpv_handle* ctx );
MPV_PTR( int64_t,       mpv_client_id, mpv_handle* ctx );
MPV_PTR( mpv_handle*,   mpv_create, void );
MPV_PTR( int,           mpv_initialize, mpv_handle* ctx );
MPV_PTR( void,          mpv_destroy, mpv_handle* ctx );
MPV_PTR( void,          mpv_terminate_destroy, mpv_handle* ctx );
MPV_PTR( mpv_handle*,   mpv_create_client, mpv_handle* ctx, const char* name );
MPV_PTR( mpv_handle*,   mpv_create_weak_client, mpv_handle* ctx, const char* name );
MPV_PTR( int,           mpv_load_config_file, mpv_handle* ctx, const char* filename );
MPV_PTR( int64_t,       mpv_get_time_ns, mpv_handle* ctx );
MPV_PTR( int64_t,       mpv_get_time_us, mpv_handle* ctx );
MPV_PTR( void,          mpv_free_node_contents, mpv_node* node );
MPV_PTR( int,           mpv_set_option, mpv_handle* ctx, const char* name, mpv_format format, void* data );
MPV_PTR( int,           mpv_set_option_string, mpv_handle* ctx, const char* name, const char* data );
MPV_PTR( int,           mpv_command, mpv_handle *ctx, const char **args );
MPV_PTR( int,           mpv_command_node, mpv_handle *ctx, mpv_node *args, mpv_node *result );
MPV_PTR( int,           mpv_command_ret, mpv_handle *ctx, const char **args, mpv_node *result );
MPV_PTR( int,           mpv_command_string, mpv_handle *ctx, const char *args );
MPV_PTR( int,           mpv_command_async, mpv_handle *ctx, uint64_t reply_userdata, const char **args );
MPV_PTR( int,           mpv_command_node_async, mpv_handle *ctx, uint64_t reply_userdata, mpv_node *args );
MPV_PTR( int,           mpv_abort_async_command, mpv_handle *ctx, uint64_t reply_userdata );
MPV_PTR( int,           mpv_set_property, mpv_handle *ctx, const char *name, mpv_format format, void *data );
MPV_PTR( int,           mpv_set_property_string, mpv_handle *ctx, const char *name, const char *data );
MPV_PTR( int,           mpv_del_property, mpv_handle *ctx, const char *name );
MPV_PTR( int,           mpv_set_property_async, mpv_handle *ctx, uint64_t reply_userdata, const char *name, mpv_format format, void *data );
MPV_PTR( int,           mpv_get_property, mpv_handle *ctx, const char *name, mpv_format format, void *data );
MPV_PTR( char*,         mpv_get_property_string, mpv_handle *ctx, const char *name );
MPV_PTR( char*,         mpv_get_property_osd_string, mpv_handle *ctx, const char *name );
MPV_PTR( int,           mpv_get_property_async, mpv_handle *ctx, uint64_t reply_userdata, const char *name, mpv_format format );
MPV_PTR( int,           mpv_observe_property, mpv_handle *mpv, uint64_t reply_userdata, const char *name, mpv_format format );
MPV_PTR( int,           mpv_unobserve_property, mpv_handle *mpv, uint64_t registered_reply_userdata );
MPV_PTR( const char*,   mpv_event_name, mpv_event_id event );
MPV_PTR( int,           mpv_event_to_node, mpv_node *dst, mpv_event *src );
MPV_PTR( int,           mpv_request_event, mpv_handle *ctx, mpv_event_id event, int enable );
MPV_PTR( int,           mpv_request_log_messages, mpv_handle *ctx, const char *min_level );
MPV_PTR( mpv_event*,    mpv_wait_event, mpv_handle* ctx, double timeout );
MPV_PTR( void,          mpv_wakeup, mpv_handle *ctx );
MPV_PTR( void,          mpv_set_wakeup_callback, mpv_handle *ctx, void (*cb)(void *d), void *d );
MPV_PTR( void,          mpv_wait_async_requests, mpv_handle *ctx );
MPV_PTR( int,           mpv_hook_add, mpv_handle *ctx, uint64_t reply_userdata, const char *name, int priority );
MPV_PTR( int,           mpv_hook_continue, mpv_handle *ctx, uint64_t id );
MPV_PTR( int,           mpv_get_wakeup_pipe, mpv_handle *ctx );

// render.h
MPV_PTR( int,           mpv_render_context_create, mpv_render_context** res, mpv_handle* mpv, mpv_render_param* params );
MPV_PTR( int,           mpv_render_context_set_parameter, mpv_render_context* ctx, mpv_render_param param );
MPV_PTR( int,           mpv_render_context_get_info, mpv_render_context* ctx, mpv_render_param param );
MPV_PTR( void,          mpv_render_context_set_update_callback, mpv_render_context* ctx, mpv_render_update_fn callback, void* callback_ctx );
MPV_PTR( uint64_t,      mpv_render_context_update, mpv_render_context* ctx );
MPV_PTR( int,           mpv_render_context_render, mpv_render_context* ctx, mpv_render_param *params );
MPV_PTR( void,          mpv_render_context_report_swap, mpv_render_context* ctx );
MPV_PTR( void,          mpv_render_context_free, mpv_render_context* ctx );

#undef MPV_PTR

