#include <cstdio>
#include <SDL3/SDL.h>


#if _WIN32
  #define MODULE_EXT ".dll"
typedef int ( *fn_entry_t )( int argc, wchar_t* argv[] );
#elif __unix__
  #define MODULE_EXT ".so"
typedef int ( *fn_entry_t )( int argc, char* argv[] );
#else
#error unsupported platform
#endif

#define MODULE_NAME "media_tag_system" MODULE_EXT


void handle_module_error( const char* msg )
{
#if CONSOLE_APP
	SDL_ShowSimpleMessageBox( SDL_MESSAGEBOX_ERROR, "Media Tag System Error!", msg, nullptr );
#else
	fputs( msg, stdout );
#endif
}


#if _WIN32
int wmain( int argc, wchar_t* argv[] )
#else
int main( int argc, char* argv[] )
#endif
{
	SDL_SharedObject* module = SDL_LoadObject( MODULE_NAME );

	if ( !module )
	{
		handle_module_error( "Failed to find " MODULE_NAME );
		return -1;
	}

	SDL_FunctionPointer entry_func = SDL_LoadFunction( module, "entry" );

	if ( !entry_func )
	{
		handle_module_error( "Failed to find entry function!" );
		return -1;
	}

	auto func_ptr = ( fn_entry_t )( entry_func );
	return func_ptr( argc, argv );
}


