// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include <SDL3/SDL.h>

#include "../client/client.h"
#include "sdl_glw.h"
/* Phase 5: log channels */
LOG_DECLARE_CHANNEL( ch_client, "client" );

static cvar_t *in_keyboardDebug;
static cvar_t *in_forceCharset;

#ifdef USE_JOYSTICK
// SDL3: SDL_GameController renamed to SDL_Gamepad
static SDL_Gamepad *gamepad;
static SDL_Joystick *stick = NULL;
#endif

static qboolean mouseAvailable = qfalse;
static qboolean mouseActive = qfalse;

static cvar_t *in_mouse;

#ifdef USE_JOYSTICK
static cvar_t *in_joystick;
static cvar_t *in_joystickThreshold;
static cvar_t *in_joystickNo;
static cvar_t *in_joystickUseAnalog;

static cvar_t *j_pitch;
static cvar_t *j_yaw;
static cvar_t *j_forward;
static cvar_t *j_side;
static cvar_t *j_up;
static cvar_t *j_pitch_axis;
static cvar_t *j_yaw_axis;
static cvar_t *j_forward_axis;
static cvar_t *j_side_axis;
static cvar_t *j_up_axis;
#endif

#define Com_QueueEvent Sys_QueEvent

static cvar_t *cl_consoleKeys;

static int in_eventTime = 0;
static qboolean mouse_focus;

#define CTRL(a) ((a)-'a'+1)

/*
===============
IN_PrintKey
===============
*/
// SDL3: SDL_Keysym struct removed; keyboard fields now directly on SDL_KeyboardEvent
static void IN_PrintKey( const SDL_KeyboardEvent *event, keyNum_t key, qboolean down )
{
	if( down )
		Com_Log( SEV_INFO, LOG_CH(ch_client), "+ " );
	else
		Com_Log( SEV_INFO, LOG_CH(ch_client), "  " );

	Com_Log( SEV_INFO, LOG_CH(ch_client), "Scancode: 0x%02x(%s) Sym: 0x%02x(%s)",
			event->scancode, SDL_GetScancodeName( event->scancode ),
			event->key, SDL_GetKeyName( event->key ) );

	if( event->mod & SDL_KMOD_LSHIFT ) Com_Log( SEV_INFO, LOG_CH(ch_client), " KMOD_LSHIFT" );
	if( event->mod & SDL_KMOD_RSHIFT ) Com_Log( SEV_INFO, LOG_CH(ch_client), " KMOD_RSHIFT" );
	if( event->mod & SDL_KMOD_LCTRL )  Com_Log( SEV_INFO, LOG_CH(ch_client), " KMOD_LCTRL" );
	if( event->mod & SDL_KMOD_RCTRL )  Com_Log( SEV_INFO, LOG_CH(ch_client), " KMOD_RCTRL" );
	if( event->mod & SDL_KMOD_LALT )   Com_Log( SEV_INFO, LOG_CH(ch_client), " KMOD_LALT" );
	if( event->mod & SDL_KMOD_RALT )   Com_Log( SEV_INFO, LOG_CH(ch_client), " KMOD_RALT" );
	if( event->mod & SDL_KMOD_LGUI )   Com_Log( SEV_INFO, LOG_CH(ch_client), " KMOD_LGUI" );
	if( event->mod & SDL_KMOD_RGUI )   Com_Log( SEV_INFO, LOG_CH(ch_client), " KMOD_RGUI" );
	if( event->mod & SDL_KMOD_NUM )    Com_Log( SEV_INFO, LOG_CH(ch_client), " KMOD_NUM" );
	if( event->mod & SDL_KMOD_CAPS )   Com_Log( SEV_INFO, LOG_CH(ch_client), " KMOD_CAPS" );
	if( event->mod & SDL_KMOD_MODE )   Com_Log( SEV_INFO, LOG_CH(ch_client), " KMOD_MODE" );
	// SDL3: KMOD_RESERVED removed

	Com_Log( SEV_INFO, LOG_CH(ch_client), " Q:0x%02x(%s)\n", key, Key_KeynumToString( key ) );
}


#define MAX_CONSOLE_KEYS 16

/*
===============
IN_IsConsoleKey

TODO: If the SDL_Scancode situation improves, use it instead of
      both of these methods
===============
*/
static qboolean IN_IsConsoleKey( keyNum_t key, int character )
{
	typedef struct consoleKey_s
	{
		enum
		{
			QUAKE_KEY,
			CHARACTER
		} type;

		union
		{
			keyNum_t key;
			int character;
		} u;
	} consoleKey_t;

	static consoleKey_t consoleKeys[ MAX_CONSOLE_KEYS ];
	static int numConsoleKeys = 0;
	int i;

	// Only parse the variable when it changes
	static int s_consolekeys_mod = -1;
	if ( cl_consoleKeys->modificationCount != s_consolekeys_mod )
	{
		const char *text_p, *token;
		ComParser parser = { 0 };

		s_consolekeys_mod = cl_consoleKeys->modificationCount;
		text_p = cl_consoleKeys->string;
		numConsoleKeys = 0;

		while( numConsoleKeys < MAX_CONSOLE_KEYS )
		{
			consoleKey_t *c = &consoleKeys[ numConsoleKeys ];
			int charCode = 0;

			token = COM_Parse( &parser, &text_p );
			if( !token[ 0 ] )
				break;

			charCode = Com_HexStrToInt( token );

			if( charCode > 0 )
			{
				c->type = CHARACTER;
				c->u.character = charCode;
			}
			else
			{
				c->type = QUAKE_KEY;
				c->u.key = Key_StringToKeynum( token );

				// 0 isn't a key
				if ( c->u.key <= 0 )
					continue;
			}

			numConsoleKeys++;
		}
	}

	// If the character is the same as the key, prefer the character
	if ( key == character )
		key = 0;

	for ( i = 0; i < numConsoleKeys; i++ )
	{
		consoleKey_t *c = &consoleKeys[ i ];

		switch ( c->type )
		{
			case QUAKE_KEY:
				if( key && c->u.key == key )
					return qtrue;
				break;

			case CHARACTER:
				if( c->u.character == character )
					return qtrue;
				break;
		}
	}

	return qfalse;
}


/*
===============
IN_GenericModifierKey

Phase 6.1: maps a side-specific modifier (e.g. K_LEFTSHIFT) to its generic
counterpart (K_SHIFT). Returns 0 if the key is not a side-specific modifier.

This lets HandleEvents emit BOTH events on the same physical key press,
preserving backward compatibility for bindings and engine code that checks
keys[K_ALT].down etc.
===============
*/
static keyNum_t IN_GenericModifierKey( keyNum_t key )
{
	switch ( key )
	{
		case K_LEFTSHIFT:
		case K_RIGHTSHIFT:
			return K_SHIFT;
		case K_LEFTCTRL:
		case K_RIGHTCTRL:
			return K_CTRL;
		case K_LEFTALT:
		case K_RIGHTALT:
			return K_ALT;
		case K_LEFTSUPER:
		case K_RIGHTSUPER:
			return K_SUPER;
		case K_LEFTCOMMAND:
		case K_RIGHTCOMMAND:
			return K_COMMAND;
		default:
			return 0;
	}
}


/*
===============
IN_TranslateSDLToQ3Key
===============
*/
// SDL3: SDL_Keysym struct removed; takes SDL_KeyboardEvent directly
// Fields: event->scancode (was keysym->scancode), event->key (was keysym->sym),
//         event->mod (was keysym->mod)
static keyNum_t IN_TranslateSDLToQ3Key( const SDL_KeyboardEvent *event, qboolean down )
{
	keyNum_t key = 0;

	if ( event->scancode >= SDL_SCANCODE_1 && event->scancode <= SDL_SCANCODE_0 )
	{
		// Always map the number keys as such even if they actually map
		// to other characters (eg, "1" is "&" on an AZERTY keyboard).
		if( event->scancode == SDL_SCANCODE_0 )
			key = '0';
		else
			key = '1' + event->scancode - SDL_SCANCODE_1;
	}
	else if ( in_forceCharset->integer > 0 )
	{
		if ( event->scancode >= SDL_SCANCODE_A && event->scancode <= SDL_SCANCODE_Z )
		{
			key = 'a' + event->scancode - SDL_SCANCODE_A;
		}
		else
		{
			switch ( event->scancode )
			{
				case SDL_SCANCODE_MINUS:        key = '-';  break;
				case SDL_SCANCODE_EQUALS:       key = '=';  break;
				case SDL_SCANCODE_LEFTBRACKET:  key = '[';  break;
				case SDL_SCANCODE_RIGHTBRACKET: key = ']';  break;
				case SDL_SCANCODE_NONUSBACKSLASH:
				case SDL_SCANCODE_BACKSLASH:    key = '\\'; break;
				case SDL_SCANCODE_SEMICOLON:    key = ';';  break;
				case SDL_SCANCODE_APOSTROPHE:   key = '\''; break;
				case SDL_SCANCODE_COMMA:        key = ',';  break;
				case SDL_SCANCODE_PERIOD:       key = '.';  break;
				case SDL_SCANCODE_SLASH:        key = '/';  break;
				default:
					/* key = 0 */
					break;
			}
		}
	}

	if( !key && event->key >= SDLK_SPACE && event->key < SDLK_DELETE )
	{
		// These happen to match the ASCII chars
		key = (int)event->key;
	}
	else if( !key )
	{
		switch( event->key )
		{
			case SDLK_PAGEUP:       key = K_PGUP;          break;
			case SDLK_KP_9:         key = K_KP_PGUP;       break;
			case SDLK_PAGEDOWN:     key = K_PGDN;          break;
			case SDLK_KP_3:         key = K_KP_PGDN;       break;
			case SDLK_KP_7:         key = K_KP_HOME;       break;
			case SDLK_HOME:         key = K_HOME;          break;
			case SDLK_KP_1:         key = K_KP_END;        break;
			case SDLK_END:          key = K_END;           break;
			case SDLK_KP_4:         key = K_KP_LEFTARROW;  break;
			case SDLK_LEFT:         key = K_LEFTARROW;     break;
			case SDLK_KP_6:         key = K_KP_RIGHTARROW; break;
			case SDLK_RIGHT:        key = K_RIGHTARROW;    break;
			case SDLK_KP_2:         key = K_KP_DOWNARROW;  break;
			case SDLK_DOWN:         key = K_DOWNARROW;     break;
			case SDLK_KP_8:         key = K_KP_UPARROW;    break;
			case SDLK_UP:           key = K_UPARROW;       break;
			case SDLK_ESCAPE:       key = K_ESCAPE;        break;
			case SDLK_KP_ENTER:     key = K_KP_ENTER;      break;
			case SDLK_RETURN:       key = K_ENTER;         break;
			case SDLK_TAB:          key = K_TAB;           break;
			case SDLK_F1:           key = K_F1;            break;
			case SDLK_F2:           key = K_F2;            break;
			case SDLK_F3:           key = K_F3;            break;
			case SDLK_F4:           key = K_F4;            break;
			case SDLK_F5:           key = K_F5;            break;
			case SDLK_F6:           key = K_F6;            break;
			case SDLK_F7:           key = K_F7;            break;
			case SDLK_F8:           key = K_F8;            break;
			case SDLK_F9:           key = K_F9;            break;
			case SDLK_F10:          key = K_F10;           break;
			case SDLK_F11:          key = K_F11;           break;
			case SDLK_F12:          key = K_F12;           break;
			case SDLK_F13:          key = K_F13;           break;
			case SDLK_F14:          key = K_F14;           break;
			case SDLK_F15:          key = K_F15;           break;

			case SDLK_BACKSPACE:    key = K_BACKSPACE;     break;
			case SDLK_KP_PERIOD:    key = K_KP_DEL;        break;
			case SDLK_DELETE:       key = K_DEL;           break;
			case SDLK_PAUSE:        key = K_PAUSE;         break;

			// Phase 6.1: side-specific modifier keys.
			// Translate to the LEFT/RIGHT variant; HandleEvents emits the
			// generic K_SHIFT/K_CTRL/K_ALT/K_COMMAND/K_SUPER alongside for
			// backward compatibility with bindings and engine code.
			case SDLK_LSHIFT:       key = K_LEFTSHIFT;     break;
			case SDLK_RSHIFT:       key = K_RIGHTSHIFT;    break;

			case SDLK_LCTRL:        key = K_LEFTCTRL;      break;
			case SDLK_RCTRL:        key = K_RIGHTCTRL;     break;

#ifdef __APPLE__
			case SDLK_LGUI:         key = K_LEFTCOMMAND;   break;
			case SDLK_RGUI:         key = K_RIGHTCOMMAND;  break;
#else
			case SDLK_LGUI:         key = K_LEFTSUPER;     break;
			case SDLK_RGUI:         key = K_RIGHTSUPER;    break;
#endif

			case SDLK_LALT:         key = K_LEFTALT;       break;
			case SDLK_RALT:         key = K_RIGHTALT;      break;

			case SDLK_KP_5:         key = K_KP_5;          break;
			case SDLK_INSERT:       key = K_INS;           break;
			case SDLK_KP_0:         key = K_KP_INS;        break;
			case SDLK_KP_MULTIPLY:  key = '*'; /*K_KP_STAR;*/ break;
			case SDLK_KP_PLUS:      key = K_KP_PLUS;       break;
			case SDLK_KP_MINUS:     key = K_KP_MINUS;      break;
			case SDLK_KP_DIVIDE:    key = K_KP_SLASH;      break;

			case SDLK_MODE:         key = K_MODE;          break;
			case SDLK_HELP:         key = K_HELP;          break;
			case SDLK_PRINTSCREEN:  key = K_PRINT;         break;
			case SDLK_SYSREQ:       key = K_SYSREQ;        break;
			case SDLK_MENU:         key = K_MENU;          break;
			case SDLK_APPLICATION:	key = K_MENU;          break;
			case SDLK_POWER:        key = K_POWER;         break;
			case SDLK_UNDO:         key = K_UNDO;          break;
			case SDLK_SCROLLLOCK:   key = K_SCROLLOCK;     break;
			case SDLK_NUMLOCKCLEAR: key = K_KP_NUMLOCK;    break;
			case SDLK_CAPSLOCK:     key = K_CAPSLOCK;      break;

			default:
				key = 0;
				break;
		}
	}

	if ( in_keyboardDebug->integer )
		IN_PrintKey( event, key, down );

	if ( event->scancode == SDL_SCANCODE_GRAVE )
	{
		// Console keys can't be bound or generate characters
		key = K_CONSOLE;
	}
	else if ( IN_IsConsoleKey( key, 0 ) )
	{
		// Console keys can't be bound or generate characters
		key = K_CONSOLE;
	}

	return key;
}


/*
===============
IN_GobbleMotionEvents
===============
*/
static void IN_GobbleMouseEvents( void )
{
	SDL_Event dummy[ 1 ];
	int val = 0;

	// Gobble any mouse events
	SDL_PumpEvents();

	// SDL3: SDL_MOUSEMOTION/SDL_MOUSEWHEEL renamed to SDL_EVENT_*
	while( ( val = SDL_PeepEvents( dummy, ARRAY_LEN( dummy ), SDL_GETEVENT,
		SDL_EVENT_MOUSE_MOTION, SDL_EVENT_MOUSE_WHEEL ) ) > 0 ) { }

	if ( val < 0 )
		Com_Log( SEV_INFO, LOG_CH(ch_client), "%s failed: %s\n", __func__, SDL_GetError() );
}


//#define DEBUG_EVENTS

/*
===============
IN_ActivateMouse
===============
*/
static void IN_ActivateMouse( void )
{
	if ( !mouseAvailable )
		return;

	if ( !mouseActive )
	{
		IN_GobbleMouseEvents();

		// SDL3: SDL_SetRelativeMouseMode requires window parameter
		SDL_SetWindowRelativeMouseMode( SDL_window, in_mouse->integer == 1 ? true : false );
		// SDL3: SDL_SetWindowGrab renamed to SDL_SetWindowMouseGrab
		SDL_SetWindowMouseGrab( SDL_window, true );

		if ( glw_state.isFullscreen )
			SDL_HideCursor(); // SDL3: SDL_ShowCursor(SDL_FALSE) → SDL_HideCursor()

		// NOLINTBEGIN(bugprone-integer-division) — pixel-aligned window-center coordinates; integer math intentional
		SDL_WarpMouseInWindow( SDL_window,
			(float)(glw_state.window_width / 2), (float)(glw_state.window_height / 2) );
		// NOLINTEND(bugprone-integer-division)

#ifdef DEBUG_EVENTS
		Com_Log( SEV_INFO, LOG_CH(ch_client), "%4i %s\n", Sys_Milliseconds(), __func__ );
#endif
	}

	// in_nograb makes no sense in fullscreen mode
	if ( !glw_state.isFullscreen )
	{
		{
			static int s_nograb_mod = -1;
			if ( in_nograb->modificationCount != s_nograb_mod || !mouseActive )
			{
				if ( in_nograb->integer ) {
					SDL_SetWindowRelativeMouseMode( SDL_window, false );
					SDL_SetWindowMouseGrab( SDL_window, false );
				} else {
					SDL_SetWindowRelativeMouseMode( SDL_window, in_mouse->integer == 1 ? true : false );
					SDL_SetWindowMouseGrab( SDL_window, true );
				}

				s_nograb_mod = in_nograb->modificationCount;
			}
		}
	}

	mouseActive = qtrue;
}


/*
===============
IN_DeactivateMouse
===============
*/
static void IN_DeactivateMouse( void )
{
	const char* drv = SDL_GetCurrentVideoDriver();

	if ( !mouseAvailable )
		return;

	if ( mouseActive )
	{
#ifdef DEBUG_EVENTS
		Com_Log( SEV_INFO, LOG_CH(ch_client), "%4i %s\n", Sys_Milliseconds(), __func__ );
#endif
		IN_GobbleMouseEvents();

		SDL_SetWindowMouseGrab( SDL_window, false );
		SDL_SetWindowRelativeMouseMode( SDL_window, false );

		// NOLINTBEGIN(bugprone-integer-division) — pixel-aligned screen/window-center coordinates; integer math intentional
		if ( gw_active )
			SDL_WarpMouseInWindow( SDL_window,
				(float)(glw_state.window_width / 2), (float)(glw_state.window_height / 2) );
		else
		{
			if ( glw_state.isFullscreen )
				SDL_ShowCursor(); // SDL3: SDL_ShowCursor(SDL_TRUE) → SDL_ShowCursor()

			if ( drv && strcmp( drv, "x11" ) == 0 ) {
				SDL_WarpMouseGlobal(
					(float)(glw_state.desktop_width / 2),
					(float)(glw_state.desktop_height / 2) );
			}
		}
		// NOLINTEND(bugprone-integer-division)

		mouseActive = qfalse;
	}

	// Always show the cursor when the mouse is disabled,
	// but not when fullscreen
	if ( !glw_state.isFullscreen )
		SDL_ShowCursor();
}


#ifdef USE_JOYSTICK
// We translate axes movement into keypresses
static const int joy_keys[16] = {
	K_LEFTARROW, K_RIGHTARROW,
	K_UPARROW, K_DOWNARROW,
	K_JOY17, K_JOY18,
	K_JOY19, K_JOY20,
	K_JOY21, K_JOY22,
	K_JOY23, K_JOY24,
	K_JOY25, K_JOY26,
	K_JOY27, K_JOY28
};

// translate hat events into keypresses
// the 4 highest buttons are used for the first hat ...
static const int hat_keys[16] = {
	K_JOY29, K_JOY30,
	K_JOY31, K_JOY32,
	K_JOY25, K_JOY26,
	K_JOY27, K_JOY28,
	K_JOY21, K_JOY22,
	K_JOY23, K_JOY24,
	K_JOY17, K_JOY18,
	K_JOY19, K_JOY20
};


struct
{
	// SDL3: SDL_CONTROLLER_BUTTON_MAX renamed to SDL_GAMEPAD_BUTTON_COUNT
	qboolean buttons[SDL_GAMEPAD_BUTTON_COUNT + 1];
	unsigned int oldaxes;
	int oldaaxes[MAX_JOYSTICK_AXIS];
	unsigned int oldhats;
} stick_state;


/*
===============
IN_InitJoystick
===============
*/
static void IN_InitJoystick( void )
{
	cvar_t *cv;
	int i = 0;
	int total = 0;
	QS_LOCAL(buf, 16384);
	SDL_JoystickID *joysticks = NULL;

	if (gamepad)
		SDL_CloseGamepad(gamepad); // SDL3: SDL_GameControllerClose → SDL_CloseGamepad

	if (stick != NULL)
		SDL_CloseJoystick(stick); // SDL3: SDL_JoystickClose → SDL_CloseJoystick

	stick = NULL;
	gamepad = NULL;
	memset(&stick_state, '\0', sizeof (stick_state));

	if (!SDL_WasInit(SDL_INIT_JOYSTICK))
	{
		Com_Log( SEV_DEBUG, LOG_CH(ch_client), "Calling SDL_Init(SDL_INIT_JOYSTICK)...\n");
		// SDL3: SDL_Init returns bool (true = success)
		if (!SDL_Init(SDL_INIT_JOYSTICK))
		{
			Com_Log( SEV_DEBUG, LOG_CH(ch_client), "SDL_Init(SDL_INIT_JOYSTICK) failed: %s\n", SDL_GetError());
			return;
		}
		Com_Log( SEV_DEBUG, LOG_CH(ch_client), "SDL_Init(SDL_INIT_JOYSTICK) passed.\n");
	}

	// SDL3: SDL_INIT_GAMECONTROLLER renamed to SDL_INIT_GAMEPAD
	if (!SDL_WasInit(SDL_INIT_GAMEPAD))
	{
		Com_Log( SEV_DEBUG, LOG_CH(ch_client), "Calling SDL_Init(SDL_INIT_GAMEPAD)...\n");
		if (!SDL_Init(SDL_INIT_GAMEPAD))
		{
			Com_Log( SEV_DEBUG, LOG_CH(ch_client), "SDL_Init(SDL_INIT_GAMEPAD) failed: %s\n", SDL_GetError());
			return;
		}
		Com_Log( SEV_DEBUG, LOG_CH(ch_client), "SDL_Init(SDL_INIT_GAMEPAD) passed.\n");
	}

	// SDL3: SDL_NumJoysticks() replaced by SDL_GetJoysticks(&count) returning ID array
	joysticks = SDL_GetJoysticks(&total);
	Com_Log( SEV_DEBUG, LOG_CH(ch_client), "%d possible joysticks\n", total);

	// Print list and build cvar to allow ui to select joystick.
	for (i = 0; i < total; i++)
	{
		QS_Append(&buf, SDL_GetJoystickNameForID(joysticks[i]));
		QS_AppendChar(&buf, '\n');
	}

	cv = Cvar_Get( "in_availableJoysticks", QS_CStr(&buf), CVAR_ROM );
	Cvar_SetDescription( cv, "List of available joysticks." );

	if( !in_joystick->integer ) {
		Com_Log( SEV_DEBUG, LOG_CH(ch_client), "Joystick is not active.\n" );
		SDL_free(joysticks);
		SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
		return;
	}

	{
		static const cvarDesc_t d = CVAR_INT( "in_joystickNo", "0", CVAR_ARCHIVE,
			"Select which joystick to use.", 0, 0 );
		in_joystickNo = Cvar_Register( &d );
	}
	if( in_joystickNo->integer < 0 || in_joystickNo->integer >= total )
		Cvar_Set( "in_joystickNo", "0" );

	{
		static const cvarDesc_t d = CVAR_BOOL( "in_joystickUseAnalog", "0", CVAR_ARCHIVE,
			"Do not translate joystick axis events to keyboard commands." );
		in_joystickUseAnalog = Cvar_Register( &d );
	}

	// SDL3: SDL_JoystickOpen(index) → SDL_OpenJoystick(instance_id)
	{
		SDL_JoystickID joystickID = joysticks[in_joystickNo->integer];

		stick = SDL_OpenJoystick( joystickID );

		if (stick == NULL) {
			Com_Log( SEV_DEBUG, LOG_CH(ch_client), "No joystick opened: %s\n", SDL_GetError() );
			SDL_free(joysticks);
			return;
		}

		// SDL3: SDL_IsGameController(index) → SDL_IsGamepad(instance_id)
		if (SDL_IsGamepad(joystickID))
			gamepad = SDL_OpenGamepad(joystickID); // SDL3: SDL_GameControllerOpen → SDL_OpenGamepad

		Com_Log( SEV_DEBUG, LOG_CH(ch_client), "Joystick %d opened\n", in_joystickNo->integer );
		Com_Log( SEV_DEBUG, LOG_CH(ch_client), "Name:       %s\n", SDL_GetJoystickNameForID(joystickID) );
		// SDL3: SDL_JoystickNum* renamed to SDL_GetNumJoystick*
		Com_Log( SEV_DEBUG, LOG_CH(ch_client), "Axes:       %d\n", SDL_GetNumJoystickAxes(stick) );
		Com_Log( SEV_DEBUG, LOG_CH(ch_client), "Hats:       %d\n", SDL_GetNumJoystickHats(stick) );
		Com_Log( SEV_DEBUG, LOG_CH(ch_client), "Buttons:    %d\n", SDL_GetNumJoystickButtons(stick) );
		// SDL3: SDL_JoystickNumBalls removed (balls emulated as axes)
		Com_Log( SEV_DEBUG, LOG_CH(ch_client), "Use Analog: %s\n", in_joystickUseAnalog->integer ? "Yes" : "No" );
		Com_Log( SEV_DEBUG, LOG_CH(ch_client), "Is gamepad: %s\n", gamepad ? "Yes" : "No" );
		// SDL3: SDL_JoystickEventState and SDL_GameControllerEventState removed (always enabled)
	}

	SDL_free(joysticks);
}


/*
===============
IN_ShutdownJoystick
===============
*/
static void IN_ShutdownJoystick( void )
{
	// SDL3: SDL_INIT_GAMECONTROLLER renamed to SDL_INIT_GAMEPAD
	if ( !SDL_WasInit( SDL_INIT_GAMEPAD ) )
		return;

	if ( !SDL_WasInit( SDL_INIT_JOYSTICK ) )
		return;

	if (gamepad)
	{
		SDL_CloseGamepad(gamepad); // SDL3: SDL_GameControllerClose → SDL_CloseGamepad
		gamepad = NULL;
	}

	if (stick)
	{
		SDL_CloseJoystick(stick); // SDL3: SDL_JoystickClose → SDL_CloseJoystick
		stick = NULL;
	}

	SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
	SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
}


static qboolean KeyToAxisAndSign(int keynum, int *outAxis, int *outSign)
{
	const char *bind;

	if (!keynum)
		return qfalse;

	bind = Key_GetBinding(keynum);

	if (!bind || *bind != '+')
		return qfalse;

	*outSign = 0;

	if (Q_stricmp(bind, "+forward") == 0)
	{
		*outAxis = j_forward_axis->integer;
		*outSign = j_forward->value > 0.0f ? 1 : -1;
	}
	else if (Q_stricmp(bind, "+back") == 0)
	{
		*outAxis = j_forward_axis->integer;
		*outSign = j_forward->value > 0.0f ? -1 : 1;
	}
	else if (Q_stricmp(bind, "+moveleft") == 0)
	{
		*outAxis = j_side_axis->integer;
		*outSign = j_side->value > 0.0f ? -1 : 1;
	}
	else if (Q_stricmp(bind, "+moveright") == 0)
	{
		*outAxis = j_side_axis->integer;
		*outSign = j_side->value > 0.0f ? 1 : -1;
	}
	else if (Q_stricmp(bind, "+left") == 0)
	{
		*outAxis = j_yaw_axis->integer;
		*outSign = j_yaw->value > 0.0f ? 1 : -1;
	}
	else if (Q_stricmp(bind, "+right") == 0)
	{
		*outAxis = j_yaw_axis->integer;
		*outSign = j_yaw->value > 0.0f ? -1 : 1;
	}
	else if (Q_stricmp(bind, "+moveup") == 0)
	{
		*outAxis = j_up_axis->integer;
		*outSign = j_up->value > 0.0f ? 1 : -1;
	}
	else if (Q_stricmp(bind, "+movedown") == 0)
	{
		*outAxis = j_up_axis->integer;
		*outSign = j_up->value > 0.0f ? -1 : 1;
	}

	return *outSign != 0;
}


/*
===============
IN_GamepadMove
===============
*/
static void IN_GamepadMove( void )
{
	int i;
	int translatedAxes[MAX_JOYSTICK_AXIS];
	qboolean translatedAxesSet[MAX_JOYSTICK_AXIS];

	SDL_UpdateGamepads(); // SDL3: SDL_GameControllerUpdate → SDL_UpdateGamepads

	// check buttons — SDL3: SDL_CONTROLLER_BUTTON_MAX → SDL_GAMEPAD_BUTTON_COUNT
	for (i = 0; i < SDL_GAMEPAD_BUTTON_COUNT; i++)
	{
		// SDL3: SDL_GetGamepadButton returns bool; SDL_GAMEPAD_BUTTON_SOUTH = 0 (was A)
		qboolean pressed = SDL_GetGamepadButton(gamepad, (SDL_GamepadButton)(SDL_GAMEPAD_BUTTON_SOUTH + i));
		if (pressed != stick_state.buttons[i])
		{
			if ( i >= SDL_GAMEPAD_BUTTON_MISC1 ) {
				Com_QueueEvent(in_eventTime, SE_KEY, K_PAD0_MISC1 + i - SDL_GAMEPAD_BUTTON_MISC1, pressed, 0, NULL);
			} else
			{
				Com_QueueEvent(in_eventTime, SE_KEY, K_PAD0_A + i, pressed, 0, NULL);
			}
			stick_state.buttons[i] = pressed;
		}
	}

	// must defer translated axes until all real axes are processed
	if (in_joystickUseAnalog->integer)
	{
		for (i = 0; i < MAX_JOYSTICK_AXIS; i++)
		{
			translatedAxes[i] = 0;
			translatedAxesSet[i] = qfalse;
		}
	}

	// check axes — SDL3: SDL_CONTROLLER_AXIS_MAX → SDL_GAMEPAD_AXIS_COUNT
	for (i = 0; i < SDL_GAMEPAD_AXIS_COUNT; i++)
	{
		// SDL3: SDL_GetGamepadAxis; SDL_GAMEPAD_AXIS_LEFTX = 0 (was SDL_CONTROLLER_AXIS_LEFTX)
		int axis = SDL_GetGamepadAxis(gamepad, (SDL_GamepadAxis)(SDL_GAMEPAD_AXIS_LEFTX + i));
		int oldAxis = stick_state.oldaaxes[i];

		// Smoothly ramp from dead zone to maximum value
		float f = ((float)abs(axis) / 32767.0f - in_joystickThreshold->value) / (1.0f - in_joystickThreshold->value);

		if (f < 0.0f)
			f = 0.0f;

		axis = (int)(32767 * ((axis < 0) ? -f : f));

		if (axis != oldAxis)
		{
			const int negMap[SDL_GAMEPAD_AXIS_COUNT] = { K_PAD0_LEFTSTICK_LEFT,  K_PAD0_LEFTSTICK_UP,   K_PAD0_RIGHTSTICK_LEFT,  K_PAD0_RIGHTSTICK_UP, 0, 0 };
			const int posMap[SDL_GAMEPAD_AXIS_COUNT] = { K_PAD0_LEFTSTICK_RIGHT, K_PAD0_LEFTSTICK_DOWN, K_PAD0_RIGHTSTICK_RIGHT, K_PAD0_RIGHTSTICK_DOWN, K_PAD0_LEFTTRIGGER, K_PAD0_RIGHTTRIGGER };

			qboolean posAnalog = qfalse, negAnalog = qfalse;
			int negKey = negMap[i];
			int posKey = posMap[i];

			if (in_joystickUseAnalog->integer)
			{
				int posAxis = 0, posSign = 0, negAxis = 0, negSign = 0;

				posAnalog = KeyToAxisAndSign(posKey, &posAxis, &posSign);
				negAnalog = KeyToAxisAndSign(negKey, &negAxis, &negSign);

				if (posAnalog && !translatedAxesSet[posAxis] && oldAxis > 0 && axis <= 0)
				{
					translatedAxes[posAxis] = 0;
					translatedAxesSet[posAxis] = qtrue;
				}

				if (negAnalog && !translatedAxesSet[negAxis] && oldAxis < 0 && axis >= 0)
				{
					translatedAxes[negAxis] = 0;
					translatedAxesSet[negAxis] = qtrue;
				}

				if (posAnalog && axis > 0)
				{
					translatedAxes[posAxis] = axis * posSign;
					translatedAxesSet[posAxis] = qtrue;
				}

				if (negAnalog && axis < 0)
				{
					translatedAxes[negAxis] = -axis * negSign;
					translatedAxesSet[negAxis] = qtrue;
				}
			}

			if (!posAnalog && posKey && oldAxis > 0 && axis <= 0)
				Com_QueueEvent(in_eventTime, SE_KEY, posKey, qfalse, 0, NULL);

			if (!negAnalog && negKey && oldAxis < 0 && axis >= 0)
				Com_QueueEvent(in_eventTime, SE_KEY, negKey, qfalse, 0, NULL);

			if (!posAnalog && posKey && oldAxis <= 0 && axis > 0)
				Com_QueueEvent(in_eventTime, SE_KEY, posKey, qtrue, 0, NULL);

			if (!negAnalog && negKey && oldAxis >= 0 && axis < 0)
				Com_QueueEvent(in_eventTime, SE_KEY, negKey, qtrue, 0, NULL);

			stick_state.oldaaxes[i] = axis;
		}
	}

	if (in_joystickUseAnalog->integer)
	{
		for (i = 0; i < MAX_JOYSTICK_AXIS; i++)
		{
			if (translatedAxesSet[i])
				Com_QueueEvent(in_eventTime, SE_JOYSTICK_AXIS, i, translatedAxes[i], 0, NULL);
		}
	}
}


/*
===============
IN_JoyMove
===============
*/
static void IN_JoyMove( void )
{
	unsigned int axes = 0;
	unsigned int hats = 0;
	int total = 0;
	int i = 0;

	in_eventTime = Sys_Milliseconds();

	if (gamepad)
	{
		IN_GamepadMove();
		return;
	}

	if (!stick)
		return;

	SDL_UpdateJoysticks(); // SDL3: SDL_JoystickUpdate → SDL_UpdateJoysticks

	// SDL3: SDL_JoystickNumBalls removed (balls emulated as axes) — ball code removed

	// now query the stick buttons...
	// SDL3: SDL_JoystickNumButtons → SDL_GetNumJoystickButtons
	total = SDL_GetNumJoystickButtons(stick);
	if (total > 0)
	{
		if (total > ARRAY_LEN(stick_state.buttons))
			total = ARRAY_LEN(stick_state.buttons);
		for (i = 0; i < total; i++)
		{
			// SDL3: SDL_JoystickGetButton → SDL_GetJoystickButton
			qboolean pressed = (SDL_GetJoystickButton(stick, i) != 0);
			if (pressed != stick_state.buttons[i])
			{
				Com_QueueEvent( in_eventTime, SE_KEY, K_JOY1 + i, pressed, 0, NULL );
				stick_state.buttons[i] = pressed;
			}
		}
	}

	// look at the hats...
	// SDL3: SDL_JoystickNumHats → SDL_GetNumJoystickHats
	total = SDL_GetNumJoystickHats(stick);
	if (total > 0)
	{
		if (total > 4) total = 4;
		for (i = 0; i < total; i++)
		{
			// SDL3: SDL_JoystickGetHat → SDL_GetJoystickHat
			((Uint8 *)&hats)[i] = SDL_GetJoystickHat(stick, i);
		}
	}

	// update hat state
	if (hats != stick_state.oldhats)
	{
		for( i = 0; i < 4; i++ ) {
			if( ((Uint8 *)&hats)[i] != ((Uint8 *)&stick_state.oldhats)[i] ) {
				// release event
				switch( ((Uint8 *)&stick_state.oldhats)[i] ) {
					case SDL_HAT_UP:
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 0], qfalse, 0, NULL );
						break;
					case SDL_HAT_RIGHT:
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 1], qfalse, 0, NULL );
						break;
					case SDL_HAT_DOWN:
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 2], qfalse, 0, NULL );
						break;
					case SDL_HAT_LEFT:
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 3], qfalse, 0, NULL );
						break;
					case SDL_HAT_RIGHTUP:
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 0], qfalse, 0, NULL );
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 1], qfalse, 0, NULL );
						break;
					case SDL_HAT_RIGHTDOWN:
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 2], qfalse, 0, NULL );
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 1], qfalse, 0, NULL );
						break;
					case SDL_HAT_LEFTUP:
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 0], qfalse, 0, NULL );
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 3], qfalse, 0, NULL );
						break;
					case SDL_HAT_LEFTDOWN:
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 2], qfalse, 0, NULL );
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 3], qfalse, 0, NULL );
						break;
					default:
						break;
				}
				// press event
				switch( ((Uint8 *)&hats)[i] ) {
					case SDL_HAT_UP:
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 0], qtrue, 0, NULL );
						break;
					case SDL_HAT_RIGHT:
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 1], qtrue, 0, NULL );
						break;
					case SDL_HAT_DOWN:
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 2], qtrue, 0, NULL );
						break;
					case SDL_HAT_LEFT:
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 3], qtrue, 0, NULL );
						break;
					case SDL_HAT_RIGHTUP:
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 0], qtrue, 0, NULL );
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 1], qtrue, 0, NULL );
						break;
					case SDL_HAT_RIGHTDOWN:
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 2], qtrue, 0, NULL );
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 1], qtrue, 0, NULL );
						break;
					case SDL_HAT_LEFTUP:
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 0], qtrue, 0, NULL );
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 3], qtrue, 0, NULL );
						break;
					case SDL_HAT_LEFTDOWN:
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 2], qtrue, 0, NULL );
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 3], qtrue, 0, NULL );
						break;
					default:
						break;
				}
			}
		}
	}

	// save hat state
	stick_state.oldhats = hats;

	// finally, look at the axes...
	// SDL3: SDL_JoystickNumAxes → SDL_GetNumJoystickAxes
	total = SDL_GetNumJoystickAxes(stick);
	if (total > 0)
	{
		if (in_joystickUseAnalog->integer)
		{
			if (total > MAX_JOYSTICK_AXIS) total = MAX_JOYSTICK_AXIS;
			for (i = 0; i < total; i++)
			{
				// SDL3: SDL_JoystickGetAxis → SDL_GetJoystickAxis
				Sint16 axis = SDL_GetJoystickAxis(stick, i);
				float f = ( (float) abs(axis) ) / 32767.0f;

				if( f < in_joystickThreshold->value ) axis = 0;

				if ( axis != stick_state.oldaaxes[i] )
				{
					Com_QueueEvent( in_eventTime, SE_JOYSTICK_AXIS, i, axis, 0, NULL );
					stick_state.oldaaxes[i] = axis;
				}
			}
		}
		else
		{
			if (total > 16) total = 16;
			for (i = 0; i < total; i++)
			{
				Sint16 axis = SDL_GetJoystickAxis(stick, i);
				float f = ( (float) axis ) / 32767.0f;
				if( f < -in_joystickThreshold->value ) {
					axes |= ( 1 << ( i * 2 ) );
				} else if( f > in_joystickThreshold->value ) {
					axes |= ( 1 << ( ( i * 2 ) + 1 ) );
				}
			}
		}
	}

	/* Time to update axes state based on old vs. new. */
	if (axes != stick_state.oldaxes)
	{
		for( i = 0; i < 16; i++ ) {
			if( ( axes & ( 1 << i ) ) && !( stick_state.oldaxes & ( 1 << i ) ) ) {
				Com_QueueEvent( in_eventTime, SE_KEY, joy_keys[i], qtrue, 0, NULL );
			}

			if( !( axes & ( 1 << i ) ) && ( stick_state.oldaxes & ( 1 << i ) ) ) {
				Com_QueueEvent( in_eventTime, SE_KEY, joy_keys[i], qfalse, 0, NULL );
			}
		}
	}

	/* Save for future generations. */
	stick_state.oldaxes = axes;
}
#endif  // USE_JOYSTICK


/*
===============
Sys_GetCapsLockMode
===============
*/
int Sys_GetCapsLockMode( void ) {
    return (SDL_GetModState() & SDL_KMOD_CAPS) != 0;
}


/*
===============
Sys_GetNumLockMode
===============
*/
int Sys_GetNumLockMode( void ) {
    return (SDL_GetModState() & SDL_KMOD_NUM) != 0;
}


/*
===============
IN_SyncModifiers
===============
*/
static void IN_SyncModifiers( void ) {
    SDL_Keymod mod = SDL_GetModState();

    keys[K_CTRL].down  = (mod & SDL_KMOD_CTRL)  ? qtrue : qfalse;
    keys[K_SHIFT].down = (mod & SDL_KMOD_SHIFT) ? qtrue : qfalse;
    keys[K_ALT].down   = (mod & SDL_KMOD_ALT)   ? qtrue : qfalse;
}


/*
===============
HandleEvents

SDL3 key changes summary:
  - SDL_Keysym struct removed: e.key.keysym.{scancode,sym,mod} → e.key.{scancode,key,mod}
  - Window events flattened: case SDL_WINDOWEVENT: switch(e.window.event) →
    individual top-level cases (SDL_EVENT_WINDOW_MOVED, etc.)
  - Mouse motion xrel/yrel: int → float; cast to int for SE_MOUSE
  - Mouse wheel y: int → float; cast to int for comparison
  - All event type constants renamed: SDL_KEYDOWN → SDL_EVENT_KEY_DOWN, etc.
===============
*/
void HandleEvents( void )
{
	SDL_Event e;
	keyNum_t key = 0;
	static keyNum_t lastKeyDown = 0;

	if ( !SDL_WasInit( SDL_INIT_VIDEO ) )
			return;

	in_eventTime = Sys_Milliseconds();

	IN_SyncModifiers();

	while ( SDL_PollEvent( &e ) )
	{
		switch( e.type )
		{
			// SDL3: SDL_KEYDOWN → SDL_EVENT_KEY_DOWN
			case SDL_EVENT_KEY_DOWN:
				if ( e.key.repeat && Key_GetCatcher() == 0 )
					break;
				// SDL3: pass &e.key directly (SDL_Keysym struct removed)
				key = IN_TranslateSDLToQ3Key( &e.key, qtrue );

				if ( key == K_ENTER && keys[K_ALT].down ) {
					Cvar_SetIntegerValue( "r_fullscreen", glw_state.isFullscreen ? 0 : 1 );
					Cbuf_AddText( "vid_restart\n" );
					break;
				}

				if ( key ) {
					keyNum_t generic;

					Com_QueueEvent( in_eventTime, SE_KEY, key, qtrue, 0, NULL );

					// Phase 6.1: also emit the generic modifier (K_ALT/K_CTRL/...)
					// for backward compatibility with bindings and engine code.
					generic = IN_GenericModifierKey( key );
					if ( generic )
						Com_QueueEvent( in_eventTime, SE_KEY, generic, qtrue, 0, NULL );

					if ( key == K_BACKSPACE
#ifdef __APPLE__
						// Cmd+Backspace and Option+Backspace are handled as SE_KEY
						// by Console_Key; suppress the duplicate SE_CHAR to avoid
						// an extra single-char delete after the modifier action.
						&& !keys[K_COMMAND].down && !keys[K_ALT].down
#endif
						)
						Com_QueueEvent( in_eventTime, SE_CHAR, CTRL('h'), 0, 0, NULL );
					else if ( key == K_ESCAPE )
						Com_QueueEvent( in_eventTime, SE_CHAR, key, 0, 0, NULL );
					else if( keys[K_CTRL].down && key >= 'a' && key <= 'z' )
						Com_QueueEvent( in_eventTime, SE_CHAR, CTRL(key), 0, 0, NULL );
#ifdef MACOS_X
					else if( keys[K_COMMAND].down && key == 'v' )
						Com_QueueEvent( in_eventTime, SE_CHAR, CTRL(key), 0, 0, NULL );
#endif
				}

				lastKeyDown = key;
				break;

			// SDL3: SDL_KEYUP → SDL_EVENT_KEY_UP
			case SDL_EVENT_KEY_UP:
				if( ( key = IN_TranslateSDLToQ3Key( &e.key, qfalse ) ) ) {
					keyNum_t generic;

					Com_QueueEvent( in_eventTime, SE_KEY, key, qfalse, 0, NULL );

					// Phase 6.1: release the generic modifier alongside the
					// side-specific one. We always emit it on key-up (even if
					// the opposite-side key is still held) — this is OK because
					// IN_SyncModifiers re-asserts keys[K_ALT].down etc. from
					// SDL_GetModState() at the top of every HandleEvents pass.
					generic = IN_GenericModifierKey( key );
					if ( generic )
						Com_QueueEvent( in_eventTime, SE_KEY, generic, qfalse, 0, NULL );
				}

				lastKeyDown = 0;
				break;

			// SDL3: SDL_TEXTINPUT → SDL_EVENT_TEXT_INPUT
			case SDL_EVENT_TEXT_INPUT:
				if( lastKeyDown != K_CONSOLE )
				{
					const char *c = e.text.text; // SDL3: text field is now const char *

					// Quick and dirty UTF-8 to UTF-32 conversion
					while ( *c )
					{
						int utf32 = 0;

						if( ( *c & 0x80 ) == 0 )
							utf32 = (byte)*c++;
						else if( ( *c & 0xE0 ) == 0xC0 ) // 110x xxxx
						{
							utf32 |= ( *c++ & 0x1F ) << 6;
							utf32 |= ( *c++ & 0x3F );
						}
						else if( ( *c & 0xF0 ) == 0xE0 ) // 1110 xxxx
						{
							utf32 |= ( *c++ & 0x0F ) << 12;
							utf32 |= ( *c++ & 0x3F ) << 6;
							utf32 |= ( *c++ & 0x3F );
						}
						else if( ( *c & 0xF8 ) == 0xF0 ) // 1111 0xxx
						{
							utf32 |= ( *c++ & 0x07 ) << 18;
							utf32 |= ( *c++ & 0x3F ) << 12;
							utf32 |= ( *c++ & 0x3F ) << 6;
							utf32 |= ( *c++ & 0x3F );
						}
						else
						{
							Com_Log( SEV_DEBUG, LOG_CH(ch_client), "Unrecognised UTF-8 lead byte: 0x%x\n", (unsigned int)*c );
							c++;
						}

						if( utf32 != 0 )
						{
							if ( IN_IsConsoleKey( 0, utf32 ) )
							{
								Com_QueueEvent( in_eventTime, SE_KEY, K_CONSOLE, qtrue, 0, NULL );
								Com_QueueEvent( in_eventTime, SE_KEY, K_CONSOLE, qfalse, 0, NULL );
							}
							else
								Com_QueueEvent( in_eventTime, SE_CHAR, utf32, 0, 0, NULL );
						}
					}
				}
				break;

			// SDL3: SDL_MOUSEMOTION → SDL_EVENT_MOUSE_MOTION; xrel/yrel are now float
			case SDL_EVENT_MOUSE_MOTION:
				if( mouseActive )
				{
					if( !e.motion.xrel && !e.motion.yrel )
						break;
					// Cast float→int: preserves SDL2 behavior; sub-pixel precision lost
					Com_QueueEvent( in_eventTime, SE_MOUSE, (int)e.motion.xrel, (int)e.motion.yrel, 0, NULL );
				}
				break;

			// SDL3: SDL_MOUSEBUTTONDOWN/UP → SDL_EVENT_MOUSE_BUTTON_DOWN/UP
			case SDL_EVENT_MOUSE_BUTTON_DOWN:
			case SDL_EVENT_MOUSE_BUTTON_UP:
				{
					int b;
					switch( e.button.button )
					{
						case SDL_BUTTON_LEFT:   b = K_MOUSE1;     break;
						case SDL_BUTTON_MIDDLE: b = K_MOUSE3;     break;
						case SDL_BUTTON_RIGHT:  b = K_MOUSE2;     break;
						case SDL_BUTTON_X1:     b = K_MOUSE4;     break;
						case SDL_BUTTON_X2:     b = K_MOUSE5;     break;
						default:                b = K_AUX1 + ( e.button.button - SDL_BUTTON_X2 + 1 ) % 16; break;
					}
					Com_QueueEvent( in_eventTime, SE_KEY, b,
						( e.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? qtrue : qfalse ), 0, NULL );
				}
				break;

			// SDL3: SDL_MOUSEWHEEL → SDL_EVENT_MOUSE_WHEEL; x and y are now float
			case SDL_EVENT_MOUSE_WHEEL:
				if( (int)e.wheel.y > 0 )
				{
					Com_QueueEvent( in_eventTime, SE_KEY, K_MWHEELUP, qtrue, 0, NULL );
					Com_QueueEvent( in_eventTime, SE_KEY, K_MWHEELUP, qfalse, 0, NULL );
				}
				else if( (int)e.wheel.y < 0 )
				{
					Com_QueueEvent( in_eventTime, SE_KEY, K_MWHEELDOWN, qtrue, 0, NULL );
					Com_QueueEvent( in_eventTime, SE_KEY, K_MWHEELDOWN, qfalse, 0, NULL );
				}
				// Phase 6.1: horizontal mouse wheel (touchpad gestures, tilt wheels)
				if( (int)e.wheel.x > 0 )
				{
					Com_QueueEvent( in_eventTime, SE_KEY, K_MWHEELRIGHT, qtrue, 0, NULL );
					Com_QueueEvent( in_eventTime, SE_KEY, K_MWHEELRIGHT, qfalse, 0, NULL );
				}
				else if( (int)e.wheel.x < 0 )
				{
					Com_QueueEvent( in_eventTime, SE_KEY, K_MWHEELLEFT, qtrue, 0, NULL );
					Com_QueueEvent( in_eventTime, SE_KEY, K_MWHEELLEFT, qfalse, 0, NULL );
				}
				break;

#ifdef USE_JOYSTICK
			// SDL3: SDL_CONTROLLERDEVICEADDED/REMOVED → SDL_EVENT_GAMEPAD_ADDED/REMOVED
			case SDL_EVENT_GAMEPAD_ADDED:
			case SDL_EVENT_GAMEPAD_REMOVED:
				if ( in_joystick->integer )
					IN_InitJoystick();
				break;
#endif

			// SDL3: SDL_QUIT → SDL_EVENT_QUIT
			case SDL_EVENT_QUIT:
				Cbuf_ExecuteText( EXEC_NOW, "quit Closed window\n" );
				break;

			//
			// SDL3: Window events are now top-level (no nested SDL_WINDOWEVENT).
			// SDL_WINDOWEVENT + switch(e.window.event) becomes individual cases.
			// e.window.data1/data2 semantics preserved for MOVED event.
			//

			case SDL_EVENT_WINDOW_MOVED:
				if ( gw_active && !gw_minimized && !glw_state.isFullscreen ) {
					Cvar_SetIntegerValue( "vid_xpos", e.window.data1 );
					Cvar_SetIntegerValue( "vid_ypos", e.window.data2 );
				}
				break;

			// window states:
			case SDL_EVENT_WINDOW_HIDDEN:
			case SDL_EVENT_WINDOW_MINIMIZED:
				gw_active = qfalse; gw_minimized = qtrue;
				break;

			case SDL_EVENT_WINDOW_SHOWN:
			case SDL_EVENT_WINDOW_RESTORED:
			case SDL_EVENT_WINDOW_MAXIMIZED:
				gw_minimized = qfalse;
				break;

			// keyboard focus:
			case SDL_EVENT_WINDOW_FOCUS_LOST:
				lastKeyDown = 0; Key_ClearStates(); IN_SyncModifiers(); gw_active = qfalse;
				break;

			case SDL_EVENT_WINDOW_FOCUS_GAINED:
				lastKeyDown = 0; Key_ClearStates(); IN_SyncModifiers();
				gw_active = qtrue; gw_minimized = qfalse;
				// Re-upload our gamma LUT — another app may have clobbered
				// hardware state while we were unfocused. Still required
				// post-Phase 6B3'-a: under r_fbo 0 the LUT is the only
				// gamma path, and under r_fbo 1 we still need to restore
				// the identity LUT so the shader is the sole transform.
				if ( re.SetColorMappings ) {
					re.SetColorMappings();
				}
				break;

			// mouse focus (SDL_EVENT_WINDOW_ENTER/LEAVE → MOUSE_ENTER/MOUSE_LEAVE):
			case SDL_EVENT_WINDOW_MOUSE_ENTER:
				mouse_focus = qtrue;
				break;

			case SDL_EVENT_WINDOW_MOUSE_LEAVE:
				if ( glw_state.isFullscreen ) mouse_focus = qfalse;
				break;

			default:
				break;
		}
	}
}


/*
===============
IN_Minimize

Minimize the game so that user is back at the desktop
===============
*/
static void IN_Minimize( void )
{
	SDL_MinimizeWindow( SDL_window );
}


/*
===============
IN_Frame
===============
*/
void IN_Frame( void )
{
#ifdef USE_JOYSTICK
	IN_JoyMove();
#endif

	if ( Key_GetCatcher() & KEYCATCH_CONSOLE ) {
		// temporarily deactivate if not in the game and
		// running on the desktop with multimonitor configuration
		if ( !glw_state.isFullscreen || glw_state.monitorCount > 1 ) {
			IN_DeactivateMouse();
			return;
		}
	}

	if ( !gw_active || !mouse_focus || in_nograb->integer ) {
		IN_DeactivateMouse();
		return;
	}

	IN_ActivateMouse();
}


/*
===============
IN_Restart
===============
*/
static void IN_Restart( void )
{
#ifdef USE_JOYSTICK
	IN_ShutdownJoystick();
#endif
	IN_Shutdown();
	IN_Init();
}


#ifdef USE_JOYSTICK
static const cvarDesc_t joystickDescs[] = {
	/* 0  */ CVAR_BOOL(  "in_joystick",         "0",      CVAR_ARCHIVE | CVAR_LATCH, "Whether or not joystick support is on." ),
	/* 1  */ CVAR_FLOAT( "joy_threshold",        "0.15",   CVAR_ARCHIVE,              "Threshold of joystick moving distance.", 0, 0 ),
	/* 2  */ CVAR_FLOAT( "j_pitch",              "0.022",  CVAR_ARCHIVE | CVAR_NODEFAULT,            "Joystick pitch rotation speed/direction.", 0, 0 ),
	/* 3  */ CVAR_FLOAT( "j_yaw",                "-0.022", CVAR_ARCHIVE | CVAR_NODEFAULT,            "Joystick yaw rotation speed/direction.", 0, 0 ),
	/* 4  */ CVAR_FLOAT( "j_forward",            "-0.25",  CVAR_ARCHIVE | CVAR_NODEFAULT,            "Joystick forward movement speed/direction.", 0, 0 ),
	/* 5  */ CVAR_FLOAT( "j_side",               "0.25",   CVAR_ARCHIVE | CVAR_NODEFAULT,            "Joystick side movement speed/direction.", 0, 0 ),
	/* 6  */ CVAR_FLOAT( "j_up",                 "0",      CVAR_ARCHIVE | CVAR_NODEFAULT,            "Joystick up movement speed/direction.", 0, 0 ),
	/* 7  */ CVAR_INT(   "j_pitch_axis",          "3",      CVAR_ARCHIVE | CVAR_NODEFAULT,            "Selects which joystick axis controls pitch.", 0, MAX_JOYSTICK_AXIS - 1 ),
	/* 8  */ CVAR_INT(   "j_yaw_axis",            "2",      CVAR_ARCHIVE | CVAR_NODEFAULT,            "Selects which joystick axis controls yaw.", 0, MAX_JOYSTICK_AXIS - 1 ),
	/* 9  */ CVAR_INT(   "j_forward_axis",        "1",      CVAR_ARCHIVE | CVAR_NODEFAULT,            "Selects which joystick axis controls forward/back.", 0, MAX_JOYSTICK_AXIS - 1 ),
	/* 10 */ CVAR_INT(   "j_side_axis",           "0",      CVAR_ARCHIVE | CVAR_NODEFAULT,            "Selects which joystick axis controls left/right.", 0, MAX_JOYSTICK_AXIS - 1 ),
	/* 11 */ CVAR_INT(   "j_up_axis",             "4",      CVAR_ARCHIVE | CVAR_NODEFAULT,            "Selects which joystick axis controls up/down.", 0, MAX_JOYSTICK_AXIS - 1 ),
};

enum {
	JOY_JOYSTICK, JOY_THRESHOLD,
	JOY_J_PITCH, JOY_J_YAW, JOY_J_FORWARD, JOY_J_SIDE, JOY_J_UP,
	JOY_PITCH_AXIS, JOY_YAW_AXIS, JOY_FORWARD_AXIS, JOY_SIDE_AXIS, JOY_UP_AXIS,
	JOY_CVAR_COUNT
};

_Static_assert( ARRAY_LEN( joystickDescs ) == JOY_CVAR_COUNT, "joystickDescs/enum mismatch" );
static cvar_t *joystickHandles[JOY_CVAR_COUNT];
#endif


/*
===============
IN_Init
===============
*/
void IN_Init( void )
{
	if ( !SDL_WasInit( SDL_INIT_VIDEO ) )
	{
		Com_Terminate( TERM_UNRECOVERABLE, "IN_Init called before SDL_Init( SDL_INIT_VIDEO )" );
		return;
	}

	Com_Log( SEV_DEBUG, LOG_CH(ch_client), "\n------- Input Initialization -------\n" );

	{
		static const cvarDesc_t d = CVAR_BOOL( "in_keyboardDebug", "0", CVAR_ARCHIVE,
			"Print keyboard debug info." );
		in_keyboardDebug = Cvar_Register( &d );
	}
	{
		static const cvarDesc_t d = CVAR_INT( "in_forceCharset", "1", CVAR_ARCHIVE | CVAR_NODEFAULT,
			"Try to translate non-ASCII chars in keyboard input or force EN/US keyboard layout.", 0, 0 );
		in_forceCharset = Cvar_Register( &d );
	}

	// mouse variables
	{
		static const cvarDesc_t d = CVAR_INT( "in_mouse", "1", CVAR_ARCHIVE,
			"Mouse data input source:\n"
			"  0 - disable mouse input\n"
			"  1 - di/raw mouse\n"
			" -1 - win32 mouse", -1, 1 );
		in_mouse = Cvar_Register( &d );
	}

#ifdef USE_JOYSTICK
	Cvar_RegisterTable( joystickDescs, ARRAY_LEN( joystickDescs ), joystickHandles );
	in_joystick          = joystickHandles[JOY_JOYSTICK];
	in_joystickThreshold = joystickHandles[JOY_THRESHOLD];
	j_pitch              = joystickHandles[JOY_J_PITCH];
	j_yaw                = joystickHandles[JOY_J_YAW];
	j_forward            = joystickHandles[JOY_J_FORWARD];
	j_side               = joystickHandles[JOY_J_SIDE];
	j_up                 = joystickHandles[JOY_J_UP];
	j_pitch_axis         = joystickHandles[JOY_PITCH_AXIS];
	j_yaw_axis           = joystickHandles[JOY_YAW_AXIS];
	j_forward_axis       = joystickHandles[JOY_FORWARD_AXIS];
	j_side_axis          = joystickHandles[JOY_SIDE_AXIS];
	j_up_axis            = joystickHandles[JOY_UP_AXIS];
#endif

	// ~ and `, as keys and characters
	{
		static const cvarDesc_t d = CVAR_STRING( "cl_consoleKeys", "~ ` 0x7e 0x60", CVAR_ARCHIVE,
			"Space delimited list of key names or characters that toggle the console." );
		cl_consoleKeys = Cvar_Register( &d );
	}

	mouseAvailable = ( in_mouse->value != 0 ) ? qtrue : qfalse;

	// SDL3: SDL_StartTextInput/StopTextInput require window parameter
	SDL_StartTextInput( SDL_window );

#ifdef USE_JOYSTICK
	IN_InitJoystick();
#endif

	Cmd_AddCommand( "minimize", IN_Minimize );
	Cmd_AddCommand( "in_restart", IN_Restart );

	Com_Log( SEV_DEBUG, LOG_CH(ch_client), "------------------------------------\n" );
}


/*
===============
IN_Shutdown
===============
*/
void IN_Shutdown( void )
{
	// SDL3: SDL_StopTextInput requires window parameter
	SDL_StopTextInput( SDL_window );

	IN_DeactivateMouse();

	mouseAvailable = qfalse;

#ifdef USE_JOYSTICK
	IN_ShutdownJoystick();
#endif

	Cmd_RemoveCommand( "minimize" );
	Cmd_RemoveCommand( "in_restart" );
}
