#include "args.h"
#include "system/system.h"


enum e_arg_type : u8
{
	e_arg_type_bool,
	e_arg_type_string,
	e_arg_type_float,
	e_arg_type_count,
};


struct arg_t
{
	const fs::path_char* cmd_switch;
	e_arg_type           type;
	
	union
	{
		const char* default_val_str;
		bool        default_val_bool;
	};

	union
	{
		const char* val_str;
		bool        val_bool;
	};
};


int             g_argc                  = 0;
fs::path_char** g_argv                  = nullptr;

arg_t*          g_registered_args       = nullptr;
u32             g_registered_args_count = 0;


void args_init( int argc, fs::path_char* argv[] )
{
#if 0 // _WIN32
	sys_get_args( g_argc, &g_argv );
#else
	g_argc = argc;
	g_argv = argv;
#endif
}


void args_free()
{
	if ( g_registered_args )
		free( g_registered_args );

	g_registered_args = nullptr;
}


void args_print_help()
{
	printf( "%d registered arguments:\n\n", g_registered_args_count );

#if 0
	for ( u32 i = 0; i < g_registered_args_count; i++ )
	{
		printf( g_registered_args[ i ].cmd_switch );

		switch ( g_registered_args[ i ].type )
		{
			case e_arg_type_bool:
			{
				printf( " %s", g_registered_args[ i ].val_bool ? "true" : "false" );
				break;
			}

			case e_arg_type_string:
			{
				printf( " %s", g_registered_args[ i ].val_str );

				// yes we are intentionally doing a pointer check here, by default these are the same pointer
				if ( g_registered_args[ i ].val_str != g_registered_args[ i ].default_val_str )
				{
					if ( strlen( g_registered_args[ i ].default_val_str ) > 0 )
						printf( " (default %s)", g_registered_args[ i ].default_val_str );
				}

				break;
			}
		}

		printf( "\n" );
	}
#endif
}


arg_t* args_add_registered_arg()
{
	arg_t* new_data = ch_realloc( g_registered_args, g_registered_args_count + 1, e_mem_category_general );

	if ( !new_data )
		return nullptr;

	g_registered_args = new_data;
	memset( &g_registered_args[ g_registered_args_count ], 0, sizeof( arg_t ) );
	return &g_registered_args[ g_registered_args_count++ ];
}


#if 0
const char* args_register_str( const char* default_val, const char* desc, const char* cmd_switch )
{
	arg_t* arg           = args_add_registered_arg();

	arg->cmd_switch      = cmd_switch;
	arg->type            = e_arg_type_string;
	arg->default_val_str = default_val;
	arg->val_str         = default_val;

	for ( int i = 0; i < g_argc; i++ )
	{
		if ( strcmp( g_argv[ i ], cmd_switch ) == 0 )
		{
			if ( g_argc > i + 1 )
			{
				arg->val_str = g_argv[ i + 1 ];
				return g_argv[ i + 1 ];
			}
		}
	}

	return default_val;
}
#endif


bool args_register_bool( const char* desc, const fs::path_char* cmd_switch )
{
	arg_t* arg        = args_add_registered_arg();

	arg->cmd_switch   = cmd_switch;
	arg->type         = e_arg_type_bool;
	arg->val_bool     = false;

	for ( int i = 0; i < g_argc; i++ )
	{
		if ( util_strcmp( g_argv[ i ], cmd_switch ) )
		{
			arg->val_bool = true;
			return true;
		}
	}

	return false;
}

