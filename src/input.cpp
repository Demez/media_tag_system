#include "main.h"


// scancode bindings

// map binding to list of devices used in this binding
// check each device input state

// SDL_SCANCODE
// SDL_GamepadButton


// should i save a list of gamepad guids to automatically open on connection to pc?


void test_gamepad_open()
{
	int             count     = 0;
	SDL_JoystickID* joysticks = SDL_GetGamepads( &count );

	if ( count > 0 )
		printf( "%d gamepads found!\n", count );
}


void input_set_default_binds()
{
}


void input_bind_scancode( SDL_Scancode scancode, e_act_list list, e_act_id id )
{
}


// state = input_get_key_state( action )
// vec2  = input_get_scroll( action ) - scroll steps
// value = input_get_analog1( action )  - 1d axis - analog trigger, scroll wheel
// vec2  = input_get_analog2( action )  - 2d axis - joystick, full scroll wheel
// touch input handling ????????


bool input_init()
{
	// test_gamepad_open();

	input_set_default_binds();

	return true;
}


void input_shutdown()
{
}


void input_handle_event( const SDL_Event& event )
{
	switch ( event.type )
	{
		default:
			break;

		// =================================================================
		// Keyboard

		case SDL_EVENT_KEY_DOWN:
			break;

		case SDL_EVENT_KEY_UP:
			break;

		// =================================================================
		// Mouse

		case SDL_EVENT_MOUSE_BUTTON_DOWN:
			break;

		case SDL_EVENT_MOUSE_BUTTON_UP:
			break;

		case SDL_EVENT_MOUSE_WHEEL:
			break;

		// =================================================================
		// Touch

		case SDL_EVENT_FINGER_DOWN:
			break;

		case SDL_EVENT_FINGER_UP:
			break;

		case SDL_EVENT_FINGER_MOTION:
			break;

		// =================================================================
		// Gamepad Add/Remove

        case SDL_EVENT_GAMEPAD_ADDED:
		{
			const SDL_GamepadDeviceEvent& gamepad_ev    = event.gdevice;

			//SDL_Gamepad*                  gamepad       = SDL_GetGamepadFromID( gamepad_ev.which );
	//		SDL_Gamepad*                  gamepad       = SDL_OpenGamepad( gamepad_ev.which );
	//
	//		int                           binding_count = 0;
	//		SDL_GamepadBinding**          bindings      = SDL_GetGamepadBindings( gamepad, &binding_count );
	//
	//		const char*                   name          = SDL_GetGamepadNameForID( gamepad_ev.which );
	//		SDL_GUID                      guid          = SDL_GetGamepadGUIDForID( gamepad_ev.which );
	//		char                          guid_str[ 34 ];
	//		SDL_GUIDToString( guid, guid_str, 34 );
	//
	//		printf( "gamepad added - %s - %s\n", name, guid_str );
			break;
		}
		case SDL_EVENT_GAMEPAD_REMOVED:
		{
			printf( "gamepad removed\n" );
			break;
		}

		// =================================================================
		// Gamepad Inputs

        case SDL_EVENT_GAMEPAD_AXIS_MOTION:
		{
			const SDL_GamepadAxisEvent& gamepad_ev = event.gaxis;
			printf( "GAMEPAD AXIS %d - %d\n", gamepad_ev.axis, gamepad_ev.value );
			// TODO: deadzone slider
			break;
		}
		case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
		case SDL_EVENT_GAMEPAD_BUTTON_UP:
		{
			const SDL_GamepadButtonEvent& gamepad_ev = event.gbutton;
			
			if ( gamepad_ev.down )
				printf( "GAMEPAD BTN %d DOWN\n", gamepad_ev.button );
			else
				printf( "GAMEPAD BTN %d UP\n", gamepad_ev.button );

			break;
		}
        case SDL_EVENT_GAMEPAD_REMAPPED:
		{
			break;
		}
        case SDL_EVENT_GAMEPAD_TOUCHPAD_DOWN:
		{
			break;
		}
        case SDL_EVENT_GAMEPAD_TOUCHPAD_MOTION:
		{
			break;
		}
        case SDL_EVENT_GAMEPAD_TOUCHPAD_UP:
		{
			break;
		}

		// =================================================================
		// Gamepad Other

        case SDL_EVENT_GAMEPAD_SENSOR_UPDATE:
		{
			break;
		}
        case SDL_EVENT_GAMEPAD_UPDATE_COMPLETE:
		{
			break;
		}
        case SDL_EVENT_GAMEPAD_STEAM_HANDLE_UPDATED:
		{
			break;
		}
	}
}


void input_set_action_list( e_act_list list )
{
}

