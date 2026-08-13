// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "wired_profile_imgui_sdl3.h"

#include "imgui.h"
#include <SDL3/SDL_events.h>

static ImGuiKey WiredProfileImGuiSdl3_Key( SDL_Scancode scancode ) {
	if ( scancode >= SDL_SCANCODE_A && scancode <= SDL_SCANCODE_Z ) {
		return (ImGuiKey)( ImGuiKey_A + scancode - SDL_SCANCODE_A );
	}
	if ( scancode >= SDL_SCANCODE_1 && scancode <= SDL_SCANCODE_9 ) {
		return (ImGuiKey)( ImGuiKey_1 + scancode - SDL_SCANCODE_1 );
	}
	if ( scancode >= SDL_SCANCODE_F1 && scancode <= SDL_SCANCODE_F12 ) {
		return (ImGuiKey)( ImGuiKey_F1 + scancode - SDL_SCANCODE_F1 );
	}
	if ( scancode >= SDL_SCANCODE_F13 && scancode <= SDL_SCANCODE_F24 ) {
		return (ImGuiKey)( ImGuiKey_F13 + scancode - SDL_SCANCODE_F13 );
	}
	switch ( scancode ) {
		case SDL_SCANCODE_0: return ImGuiKey_0;
		case SDL_SCANCODE_TAB: return ImGuiKey_Tab;
		case SDL_SCANCODE_LEFT: return ImGuiKey_LeftArrow;
		case SDL_SCANCODE_RIGHT: return ImGuiKey_RightArrow;
		case SDL_SCANCODE_UP: return ImGuiKey_UpArrow;
		case SDL_SCANCODE_DOWN: return ImGuiKey_DownArrow;
		case SDL_SCANCODE_PAGEUP: return ImGuiKey_PageUp;
		case SDL_SCANCODE_PAGEDOWN: return ImGuiKey_PageDown;
		case SDL_SCANCODE_HOME: return ImGuiKey_Home;
		case SDL_SCANCODE_END: return ImGuiKey_End;
		case SDL_SCANCODE_INSERT: return ImGuiKey_Insert;
		case SDL_SCANCODE_DELETE: return ImGuiKey_Delete;
		case SDL_SCANCODE_BACKSPACE: return ImGuiKey_Backspace;
		case SDL_SCANCODE_SPACE: return ImGuiKey_Space;
		case SDL_SCANCODE_RETURN: return ImGuiKey_Enter;
		case SDL_SCANCODE_ESCAPE: return ImGuiKey_Escape;
		case SDL_SCANCODE_LCTRL: return ImGuiKey_LeftCtrl;
		case SDL_SCANCODE_LSHIFT: return ImGuiKey_LeftShift;
		case SDL_SCANCODE_LALT: return ImGuiKey_LeftAlt;
		case SDL_SCANCODE_LGUI: return ImGuiKey_LeftSuper;
		case SDL_SCANCODE_RCTRL: return ImGuiKey_RightCtrl;
		case SDL_SCANCODE_RSHIFT: return ImGuiKey_RightShift;
		case SDL_SCANCODE_RALT: return ImGuiKey_RightAlt;
		case SDL_SCANCODE_RGUI: return ImGuiKey_RightSuper;
		case SDL_SCANCODE_APPLICATION: return ImGuiKey_Menu;
		case SDL_SCANCODE_APOSTROPHE: return ImGuiKey_Apostrophe;
		case SDL_SCANCODE_COMMA: return ImGuiKey_Comma;
		case SDL_SCANCODE_MINUS: return ImGuiKey_Minus;
		case SDL_SCANCODE_PERIOD: return ImGuiKey_Period;
		case SDL_SCANCODE_SLASH: return ImGuiKey_Slash;
		case SDL_SCANCODE_SEMICOLON: return ImGuiKey_Semicolon;
		case SDL_SCANCODE_EQUALS: return ImGuiKey_Equal;
		case SDL_SCANCODE_LEFTBRACKET: return ImGuiKey_LeftBracket;
		case SDL_SCANCODE_BACKSLASH: return ImGuiKey_Backslash;
		case SDL_SCANCODE_RIGHTBRACKET: return ImGuiKey_RightBracket;
		case SDL_SCANCODE_GRAVE: return ImGuiKey_GraveAccent;
		case SDL_SCANCODE_CAPSLOCK: return ImGuiKey_CapsLock;
		case SDL_SCANCODE_SCROLLLOCK: return ImGuiKey_ScrollLock;
		case SDL_SCANCODE_NUMLOCKCLEAR: return ImGuiKey_NumLock;
		case SDL_SCANCODE_PRINTSCREEN: return ImGuiKey_PrintScreen;
		case SDL_SCANCODE_PAUSE: return ImGuiKey_Pause;
		case SDL_SCANCODE_KP_0: return ImGuiKey_Keypad0;
		case SDL_SCANCODE_KP_1: return ImGuiKey_Keypad1;
		case SDL_SCANCODE_KP_2: return ImGuiKey_Keypad2;
		case SDL_SCANCODE_KP_3: return ImGuiKey_Keypad3;
		case SDL_SCANCODE_KP_4: return ImGuiKey_Keypad4;
		case SDL_SCANCODE_KP_5: return ImGuiKey_Keypad5;
		case SDL_SCANCODE_KP_6: return ImGuiKey_Keypad6;
		case SDL_SCANCODE_KP_7: return ImGuiKey_Keypad7;
		case SDL_SCANCODE_KP_8: return ImGuiKey_Keypad8;
		case SDL_SCANCODE_KP_9: return ImGuiKey_Keypad9;
		case SDL_SCANCODE_KP_DECIMAL: return ImGuiKey_KeypadDecimal;
		case SDL_SCANCODE_KP_DIVIDE: return ImGuiKey_KeypadDivide;
		case SDL_SCANCODE_KP_MULTIPLY: return ImGuiKey_KeypadMultiply;
		case SDL_SCANCODE_KP_MINUS: return ImGuiKey_KeypadSubtract;
		case SDL_SCANCODE_KP_PLUS: return ImGuiKey_KeypadAdd;
		case SDL_SCANCODE_KP_ENTER: return ImGuiKey_KeypadEnter;
		case SDL_SCANCODE_KP_EQUALS: return ImGuiKey_KeypadEqual;
		default: return ImGuiKey_None;
	}
}

static void WiredProfileImGuiSdl3_Modifiers( ImGuiIO &io, SDL_Keymod mod ) {
	io.AddKeyEvent( ImGuiMod_Ctrl, ( mod & SDL_KMOD_CTRL ) != 0 );
	io.AddKeyEvent( ImGuiMod_Shift, ( mod & SDL_KMOD_SHIFT ) != 0 );
	io.AddKeyEvent( ImGuiMod_Alt, ( mod & SDL_KMOD_ALT ) != 0 );
	io.AddKeyEvent( ImGuiMod_Super, ( mod & SDL_KMOD_GUI ) != 0 );
}

int WiredProfileImGuiSdl3_ProcessEvent( const SDL_Event *event,
		wiredProfileImGuiSdl3Receipt_t *receipt ) {
	wiredProfileImGuiSdl3Receipt_t result;
	ImGuiIO *io;

	if ( !event || !receipt || !ImGui::GetCurrentContext() ) return 0;
	result = *receipt;
	io = &ImGui::GetIO();

	switch ( event->type ) {
		case SDL_EVENT_KEY_DOWN:
		case SDL_EVENT_KEY_UP: {
			const ImGuiKey key = WiredProfileImGuiSdl3_Key( event->key.scancode );
			if ( key == ImGuiKey_None ) return 0;
			WiredProfileImGuiSdl3_Modifiers( *io, event->key.mod );
			io->AddKeyEvent( key, event->type == SDL_EVENT_KEY_DOWN );
			result.keyboardEvents++;
			break;
		}
		case SDL_EVENT_TEXT_INPUT:
			if ( !event->text.text || !event->text.text[ 0 ] ) return 0;
			io->AddInputCharactersUTF8( event->text.text );
			result.textEvents++;
			break;
		case SDL_EVENT_MOUSE_MOTION:
			io->AddMousePosEvent( event->motion.x, event->motion.y );
			result.mouseEvents++;
			break;
		case SDL_EVENT_MOUSE_BUTTON_DOWN:
		case SDL_EVENT_MOUSE_BUTTON_UP: {
			int button;
			switch ( event->button.button ) {
				case SDL_BUTTON_LEFT: button = 0; break;
				case SDL_BUTTON_RIGHT: button = 1; break;
				case SDL_BUTTON_MIDDLE: button = 2; break;
				case SDL_BUTTON_X1: button = 3; break;
				case SDL_BUTTON_X2: button = 4; break;
				default: return 0;
			}
			io->AddMouseButtonEvent( button,
				event->type == SDL_EVENT_MOUSE_BUTTON_DOWN );
			result.mouseEvents++;
			break;
		}
		case SDL_EVENT_MOUSE_WHEEL: {
			const float direction = event->wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -1.0f : 1.0f;
			io->AddMouseWheelEvent( event->wheel.x * direction,
				event->wheel.y * direction );
			result.mouseEvents++;
			break;
		}
		case SDL_EVENT_WINDOW_FOCUS_GAINED:
		case SDL_EVENT_WINDOW_FOCUS_LOST:
			io->AddFocusEvent( event->type == SDL_EVENT_WINDOW_FOCUS_GAINED );
			result.focusEvents++;
			break;
		case SDL_EVENT_WINDOW_MOUSE_LEAVE:
			io->AddMousePosEvent( -FLT_MAX, -FLT_MAX );
			result.focusEvents++;
			break;
		default:
			return 0;
	}

	result.handledEvents++;
	*receipt = result;
	return 1;
}
