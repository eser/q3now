// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#include "client.h"
#include "wired/ui/cl_wired_ui.h"
#include "wired/ui/cl_wired_msdf.h"
#include "wired/ui/cl_wired_fonts.h"
#include "wired/ui/cl_wired_text.h"
#include "../qcommon/wired/core/scripting/wired_scripting.h"
/* Phase 5: log channels */
LOG_DECLARE_CHANNEL( ch_client, "client" );

/*

key up events are sent even if in console mode

*/

field_t		g_consoleField;
field_t		chatField;
qboolean	chat_team;

int			chat_playerNum;

static void Field_CharEvent( field_t *edit, int ch );

/*
=============================================================================

EDIT FIELDS

=============================================================================
*/


/*
===================
Field_Draw

Handles horizontal scrolling and cursor blinking
x, y, and width are in pixels
===================
*/
/* ── Selection helpers ─────────────────────────────────────────────────── */

/* Returns qtrue + normalized [*start, *end) if a non-empty selection exists. */
static qboolean Field_GetSelRange( const field_t *edit, int *start, int *end ) {
	if ( !edit->selActive )
		return qfalse;
	*start = edit->selAnchor < edit->cursor ? edit->selAnchor : edit->cursor;
	*end   = edit->selAnchor < edit->cursor ? edit->cursor    : edit->selAnchor;
	return ( *start < *end ) ? qtrue : qfalse;
}

/* Delete selected region, place cursor at former selection start. */
static void Field_DeleteSel( field_t *edit ) {
	int start, end;
	if ( !Field_GetSelRange( edit, &start, &end ) ) {
		edit->selActive = qfalse;
		return;
	}
	int len = strlen( edit->buffer );
	memmove( edit->buffer + start, edit->buffer + end, len - end + 1 );
	edit->cursor = start;
	edit->selActive = qfalse;
	if ( edit->cursor < edit->scroll )
		edit->scroll = edit->cursor;
}

/* Copy selected text to the OS clipboard. */
static void Field_CopySel( const field_t *edit ) {
	int start, end;
	if ( !Field_GetSelRange( edit, &start, &end ) )
		return;
	char buf[MAX_EDIT_LINE];
	int len = end - start;
	memcpy( buf, edit->buffer + start, len );
	buf[len] = '\0';
	Sys_SetClipboardData( buf );
}

/* Select the entire buffer; cursor moves to end, anchor stays at 0. */
static void Field_SelectAll( field_t *edit ) {
	int len = strlen( edit->buffer );
	if ( len == 0 )
		return;
	edit->selAnchor = 0;
	edit->cursor    = len;
	edit->selActive = qtrue;
}

/* ── End selection helpers ─────────────────────────────────────────────── */

static void Field_VariableSizeDraw( field_t *edit, int x, int y, int width, int size, qboolean showCursor,
		qboolean noColorEscape ) {
	char	str[MAX_STRING_CHARS];
	int		prestep;

	int drawLen = edit->widthInChars - 1; // - 1 so there is always a space for the cursor
	int len = strlen( edit->buffer );

	// guarantee that cursor will be visible
	if ( len <= drawLen ) {
		prestep = 0;
	} else {
		if ( edit->scroll + drawLen > len ) {
			edit->scroll = len - drawLen;
			if ( edit->scroll < 0 ) {
				edit->scroll = 0;
			}
		}
		prestep = edit->scroll;
	}

	if ( prestep + drawLen > len ) {
		drawLen = len - prestep;
	}

	// extract <drawLen> characters from the field at <prestep>
	if ( drawLen >= MAX_STRING_CHARS ) {
		Com_Terminate( TERM_CLIENT_DROP, "drawLen >= MAX_STRING_CHARS" );
	}

	memcpy( str, edit->buffer + prestep, drawLen );
	str[ drawLen ] = '\0';

	// color tracking
	int curColor = COLOR_WHITE;

	if ( prestep > 0 ) {
		// we need to track last actual color because we cut some text before
		char *s = edit->buffer;
		for ( int i = 0; i < prestep + 1; i++, s++ ) {
			if ( Q_IsColorString( s ) ) {
				curColor = (byte)*(s+1);
				s++;
			}
		}
		// scroll marker
		// FIXME: force white color?
		if ( str[0] ) {
			str[0] = '<';
		}
	}

	// draw it — Text_Draw expects real screen pixels, same as native coords
	{
		float vx = (float)x;
		float vy = (float)y;

		if ( size == smallchar_width ) {
			float vsize = (float)smallchar_height;
			float vsizeW = (float)size;
			Text_Draw( str, vx, vy, FONT_MONO, vsize,
				g_color_table[ ColorIndexFromChar( curColor ) ], TEXT_ALIGN_LEFT, 0 );
			if ( len > drawLen + prestep ) {
				Text_DrawChar( '>', vx + ( edit->widthInChars - 1 ) * vsizeW, vy,
					FONT_MONO, vsize, colorWhite );
			}
			// Selection highlight: background rect + dark text overdraw (terminal inversion).
			{
				int selStart, selEnd;
				if ( Field_GetSelRange( edit, &selStart, &selEnd ) ) {
					int localStart = selStart - prestep;
					int localEnd   = selEnd   - prestep;
					if ( localStart < 0 )        localStart = 0;
					if ( localEnd   > drawLen )   localEnd   = drawLen;
					if ( localStart < localEnd ) {
						char prefBuf[MAX_STRING_CHARS];
						char selBuf[MAX_STRING_CHARS];
						int  prefLen = localStart;
						int  selLen  = localEnd - localStart;
						int  strLen  = (int)strlen( str );
						if ( prefLen > strLen )           prefLen = strLen;
						if ( selLen  > strLen - prefLen ) selLen  = strLen - prefLen;
						Q_strncpyz( prefBuf, str, prefLen + 1 );
						Q_strncpyz( selBuf,  str + prefLen, selLen + 1 );
						float selX = Text_Measure( prefBuf, FONT_MONO, vsize );
						float selW = Text_Measure( selBuf,  FONT_MONO, vsize );
						static const vec4_t selBg   = { 0.85f, 0.85f, 0.85f, 0.85f };
						static const vec4_t selText = { 0.05f, 0.05f, 0.10f, 1.00f };
						re.SetColor( selBg );
						re.DrawStretchPic( vx + selX, vy, selW, vsize, 0, 0, 0, 0, cls.whiteShader );
						re.SetColor( NULL );
						Text_Draw( selBuf, vx + selX, vy, FONT_MONO, vsize, selText, TEXT_ALIGN_LEFT, 0 );
					}
				}
			}
		} else {
			float vsize = (float)bigchar_height;
			float vsizeW = (float)bigchar_width;
			if ( len > drawLen + prestep ) {
				Text_Draw( ">", vx + ( edit->widthInChars - 1 ) * vsizeW, vy,
					FONT_DISPLAY, vsize,
					g_color_table[ ColorIndex( COLOR_WHITE ) ], TEXT_ALIGN_LEFT, 0 );
			}
			Text_Draw( str, vx, vy, FONT_DISPLAY, vsize,
				g_color_table[ ColorIndexFromChar( curColor ) ], TEXT_ALIGN_LEFT, 0 );
		}
	}

	// draw the cursor — real screen pixel coordinates
	if ( showCursor ) {
		float vx = (float)x;
		float vy = (float)y;

		if ( cls.realtime & 256 ) {
			return;		// off blink
		}

		int cursorChar;
		if ( key_overstrikeMode ) {
			cursorChar = '|';
		} else {
			cursorChar = '_';
		}

		int i = drawLen - strlen( str );

		if ( size == smallchar_width ) {
			float vsize = (float)smallchar_height;
			int cursorPos = edit->cursor - prestep - i;
			char tmp[MAX_STRING_CHARS];
			int copyLen = cursorPos;
			float cursorX;

			if ( copyLen > (int)strlen( str ) ) copyLen = (int)strlen( str );
			if ( copyLen < 0 ) copyLen = 0;
			Q_strncpyz( tmp, str, copyLen + 1 );

			/* Text_Measure returns real screen pixels — use directly */
			cursorX = Text_Measure( tmp, FONT_MONO, vsize );
			Text_DrawChar( cursorChar, vx + cursorX, vy,
				FONT_MONO, vsize, colorWhite );
		} else {
			float vsize = (float)bigchar_height;
			float vsizeW = (float)bigchar_width;
			Text_DrawChar( cursorChar,
				vx + ( edit->cursor - prestep - i ) * vsizeW, vy,
				FONT_DISPLAY, vsize, colorWhite );
		}
	}
}


void Field_Draw( field_t *edit, int x, int y, int width, qboolean showCursor, qboolean noColorEscape )
{
	Field_VariableSizeDraw( edit, x, y, width, smallchar_width, showCursor, noColorEscape );
}


void Field_BigDraw( field_t *edit, int x, int y, int width, qboolean showCursor, qboolean noColorEscape )
{
	Field_VariableSizeDraw( edit, x, y, width, bigchar_width, showCursor, noColorEscape );
}


/*
================
Field_Paste
================
*/
static void Field_Paste( field_t *edit ) {
	if ( edit->selActive )
		Field_DeleteSel( edit );

	char *cbd = Sys_GetClipboardData();

	if ( !cbd ) {
		return;
	}

	// send as if typed, so insert / overstrike works properly
	int pasteLen = strlen( cbd );
	for ( int i = 0 ; i < pasteLen ; i++ ) {
		Field_CharEvent( edit, cbd[i] );
	}

	Z_Free( cbd );
}


/*
=================
Field_NextWord
=================
*/
static void Field_SeekWord( field_t *edit, int direction )
{
	if ( direction > 0 ) {
		while ( edit->buffer[ edit->cursor ] == ' ' )
			edit->cursor++;
		while ( edit->buffer[ edit->cursor ] != '\0' && edit->buffer[ edit->cursor ] != ' ' )
			edit->cursor++;
		while ( edit->buffer[ edit->cursor ] == ' ' )
			edit->cursor++;
	} else {
		while ( edit->cursor > 0 && edit->buffer[ edit->cursor-1 ] == ' ' )
			edit->cursor--;
		while ( edit->cursor > 0 && edit->buffer[ edit->cursor-1 ] != ' ' )
			edit->cursor--;
		if ( edit->cursor == 0 && ( edit->buffer[ 0 ] == '/' || edit->buffer[ 0 ] == '\\' ) )
			edit->cursor++;
	}
}


/*
=================
Field_KeyDownEvent

Performs the basic line editing functions for the console,
in-game talk, and menu fields

Key events are used for non-printable characters, others are gotten from char events.
=================
*/
static void Field_KeyDownEvent( field_t *edit, int key ) {
	// shift-insert is paste
	if ( ( ( key == K_INS ) || ( key == K_KP_INS ) ) && keys[K_SHIFT].down ) {
		Field_Paste( edit );
		return;
	}

	int len = strlen( edit->buffer );

	switch ( key ) {
		case K_DEL:
			if ( edit->selActive ) {
				Field_DeleteSel( edit );
			} else if ( edit->cursor < len ) {
				memmove( edit->buffer + edit->cursor,
					edit->buffer + edit->cursor + 1, len - edit->cursor );
			}
			break;

		case K_RIGHTARROW:
			// Shift extends selection; no Shift clears it.
			if ( keys[K_SHIFT].down ) {
				if ( !edit->selActive ) { edit->selActive = qtrue; edit->selAnchor = edit->cursor; }
			} else {
				edit->selActive = qfalse;
			}
			if ( edit->cursor < len ) {
#ifdef __APPLE__
				if ( keys[K_COMMAND].down ) {
					edit->cursor = len;
					edit->scroll = len - ( edit->widthInChars - 1 );
					if ( edit->scroll < 0 ) edit->scroll = 0;
				} else if ( keys[K_ALT].down ) {
					Field_SeekWord( edit, 1 );
					if ( edit->cursor > edit->scroll + edit->widthInChars - 1 )
						edit->scroll = edit->cursor - ( edit->widthInChars - 1 );
				} else if ( !keys[K_CTRL].down ) {
					edit->cursor++;
				}
#else
				if ( keys[ K_CTRL ].down ) {
					Field_SeekWord( edit, 1 );
					if ( edit->cursor > edit->scroll + edit->widthInChars - 1 )
						edit->scroll = edit->cursor - ( edit->widthInChars - 1 );
				} else {
					edit->cursor++;
				}
#endif
			}
			break;

		case K_LEFTARROW:
			// Shift extends selection; no Shift clears it.
			if ( keys[K_SHIFT].down ) {
				if ( !edit->selActive ) { edit->selActive = qtrue; edit->selAnchor = edit->cursor; }
			} else {
				edit->selActive = qfalse;
			}
			if ( edit->cursor > 0 ) {
#ifdef __APPLE__
				if ( keys[K_COMMAND].down ) {
					edit->cursor = 0;
					edit->scroll = 0;
				} else if ( keys[K_ALT].down ) {
					Field_SeekWord( edit, -1 );
					if ( edit->cursor < edit->scroll )
						edit->scroll = edit->cursor;
				} else if ( !keys[K_CTRL].down ) {
					edit->cursor--;
				}
#else
				if ( keys[ K_CTRL ].down ) {
					Field_SeekWord( edit, -1 );
					if ( edit->cursor < edit->scroll )
						edit->scroll = edit->cursor;
				} else {
					edit->cursor--;
				}
#endif
			}
			break;

		case K_HOME:
			if ( keys[K_SHIFT].down ) {
				if ( !edit->selActive ) { edit->selActive = qtrue; edit->selAnchor = edit->cursor; }
			} else {
				edit->selActive = qfalse;
			}
			edit->cursor = 0;
			edit->scroll = 0;
			break;

		case K_END:
			if ( keys[K_SHIFT].down ) {
				if ( !edit->selActive ) { edit->selActive = qtrue; edit->selAnchor = edit->cursor; }
			} else {
				edit->selActive = qfalse;
			}
			edit->cursor = len;
			edit->scroll = len - ( edit->widthInChars - 1 );
			if ( edit->scroll < 0 ) edit->scroll = 0;
			break;

		case K_INS:
			key_overstrikeMode = !key_overstrikeMode;
			break;

		default:
			break;
	}

	// Change scroll if cursor is no longer visible
	if ( edit->cursor < edit->scroll ) {
		edit->scroll = edit->cursor;
	} else if ( edit->cursor >= edit->scroll + edit->widthInChars && edit->cursor <= len ) {
		edit->scroll = edit->cursor - edit->widthInChars + 1;
	}
}


/*
==================
Field_CharEvent
==================
*/
static void Field_CharEvent( field_t *edit, int ch ) {
	/* Any character event ends a cycling-completion sequence so the
	 * next Tab starts a fresh match list. */
	Field_ResetCompletionCycle( edit );

#ifndef __APPLE__
	if ( ch == 'v' - 'a' + 1 ) {	// ctrl-v is paste (non-macOS; macOS uses Cmd+V in Console_Key)
		Field_Paste( edit );		// Field_Paste handles active selection
		return;
	}

	if ( ch == 'c' - 'a' + 1 ) {	// ctrl-c is copy (non-macOS; macOS uses Cmd+C in Console_Key)
		Field_CopySel( edit );
		return;
	}

	if ( ch == 'x' - 'a' + 1 ) {	// ctrl-x is cut (non-macOS; macOS uses Cmd+X in Console_Key)
		Field_CopySel( edit );
		Field_DeleteSel( edit );
		return;
	}

	// NOLINTNEXTLINE(misc-redundant-expression) — kept as `'a' - 'a' + 1` to match the ctrl-key formula style of nearby checks (ctrl-letter = letter - 'a' + 1)
	if ( ch == 'a' - 'a' + 1 ) {	// ctrl-a is select all (non-macOS; macOS uses Cmd+A in Console_Key)
		Field_SelectAll( edit );
		return;
	}
#endif

	int len = strlen( edit->buffer );

	if ( ch == 'h' - 'a' + 1 ) {	// ctrl-h is backspace
		if ( edit->selActive ) {
			Field_DeleteSel( edit );
		} else if ( edit->cursor > 0 ) {
			memmove( edit->buffer + edit->cursor - 1,
				edit->buffer + edit->cursor, len + 1 - edit->cursor );
			edit->cursor--;
			if ( edit->cursor < edit->scroll )
				edit->scroll--;
		}
		return;
	}

	if ( ch == 'e' - 'a' + 1 ) {	// ctrl-e is end (clears selection)
		edit->selActive = qfalse;
		edit->cursor = len;
		edit->scroll = edit->cursor - edit->widthInChars;
		return;
	}

	//
	// ignore any other non-printable chars
	//
	if ( ch < ' ' ) {
		return;
	}

	// Typing replaces an active selection.
	if ( edit->selActive )
		Field_DeleteSel( edit );

	len = strlen( edit->buffer );

	if ( key_overstrikeMode ) {
		// - 2 to leave room for the leading slash and trailing \0
		if ( edit->cursor == MAX_EDIT_LINE - 2 )
			return;
		edit->buffer[edit->cursor] = ch;
		edit->cursor++;
	} else {	// insert mode
		// - 2 to leave room for the leading slash and trailing \0
		if ( len == MAX_EDIT_LINE - 2 ) {
			return; // all full
		}
		memmove( edit->buffer + edit->cursor + 1,
			edit->buffer + edit->cursor, len + 1 - edit->cursor );
		edit->buffer[edit->cursor] = ch;
		edit->cursor++;
	}

	if ( edit->cursor >= edit->widthInChars ) {
		edit->scroll++;
	}

	if ( edit->cursor == len + 1) {
		edit->buffer[edit->cursor] = '\0';
	}
}


/*
=============================================================================

CONSOLE LINE EDITING

==============================================================================
*/

/*
====================
Console_Key

Handles history and console scrollback
====================
*/
static void Console_Key( int key ) {
	// ctrl-M toggles mark (selection) mode.  When active, the mark
	// handler consumes navigation keys; it also swallows everything else
	// except Ctrl+M / Esc / Ctrl+C / Enter to prevent accidental command
	// execution while selecting.
	if ( Con_IsMarkActive() ||
	     ( keys[K_CTRL].down && tolower( key ) == 'm' ) ) {
		if ( Con_MarkKey( tolower( key ), keys[K_CTRL].down, keys[K_SHIFT].down ) ) {
			return;
		}
	}

#ifdef __APPLE__
	// Cmd+F opens/closes search on macOS
	if ( tolower(key) == 'f' && keys[K_COMMAND].down ) {
		if ( Con_IsSearchActive() )
			Con_SearchClose();
		else
			Con_SearchOpen();
		return;
	}
#else
	// Ctrl+F opens/closes search on non-macOS platforms
	if ( tolower(key) == 'f' && keys[K_CTRL].down ) {
		if ( Con_IsSearchActive() )
			Con_SearchClose();
		else
			Con_SearchOpen();
		return;
	}
#endif

	// when search is active, intercept keys
	if ( Con_IsSearchActive() ) {
		if ( key == K_ESCAPE ) {
			Con_SearchClose();
			return;
		}
		if ( key == K_ENTER || key == K_KP_ENTER ) {
			// Enter = next older match; Shift+Enter = next newer match.
			Con_SearchNext( keys[K_SHIFT].down ? qtrue : qfalse );
			return;
		}
#ifndef __APPLE__
		if ( key == K_F3 ) {
			// F3 = next older match; Shift+F3 = next newer match.
			Con_SearchNext( keys[K_SHIFT].down ? qtrue : qfalse );
			return;
		}
#endif
#ifdef __APPLE__
		// Cmd+G = next older match; Shift+Cmd+G = next newer match.
		if ( tolower(key) == 'g' && keys[K_COMMAND].down ) {
			Con_SearchNext( keys[K_SHIFT].down ? qtrue : qfalse );
			return;
		}
#endif
		if ( key == K_BACKSPACE ) {
			Con_SearchChar( '\b' );
			return;
		}
		// ignore most special keys while searching
		if ( key == K_PGUP || key == K_MWHEELUP ) {
			Con_SearchNext( qfalse );
			return;
		}
		if ( key == K_PGDN || key == K_MWHEELDOWN ) {
			Con_SearchNext( qtrue );
			return;
		}
		// printable chars handled in CL_CharEvent
		return;
	}

	// ctrl-L clears screen
	if ( key == 'l' && keys[K_CTRL].down ) {
		Cbuf_AddText( "clear\n" );
		return;
	}

#ifdef __APPLE__
	// Cmd+Backspace: delete to start of line. Option+Backspace: delete previous word.
	// If a selection is active, Cmd+Backspace / Option+Backspace delete the selection (via the
	// early-return branches below). Plain backspace with selection is handled by the SE_CHAR 8
	// path in Field_CharEvent (ctrl-h), so no early-return here for the plain-backspace case.
	if ( key == K_BACKSPACE ) {
		if ( keys[K_COMMAND].down ) {
			if ( g_consoleField.selActive ) {
				Field_DeleteSel( &g_consoleField );
				return;
			}
			int cursor = g_consoleField.cursor;
			if ( cursor > 0 ) {
				int len = strlen( g_consoleField.buffer );
				memmove( g_consoleField.buffer,
					g_consoleField.buffer + cursor,
					len - cursor + 1 );
				g_consoleField.cursor = 0;
				g_consoleField.scroll = 0;
			}
			return;
		}
		if ( keys[K_ALT].down ) {
			if ( g_consoleField.selActive ) {
				Field_DeleteSel( &g_consoleField );
				return;
			}
			int cursor = g_consoleField.cursor;
			if ( cursor > 0 ) {
				int len = strlen( g_consoleField.buffer );
				int wordStart = cursor;
				if ( g_consoleField.buffer[wordStart - 1] != ' ' && g_consoleField.buffer[wordStart - 1] != '\t' ) {
					while ( wordStart > 0 && g_consoleField.buffer[wordStart - 1] != ' ' && g_consoleField.buffer[wordStart - 1] != '\t' )
						wordStart--;
				} else {
					while ( wordStart > 0 && ( g_consoleField.buffer[wordStart - 1] == ' ' || g_consoleField.buffer[wordStart - 1] == '\t' ) )
						wordStart--;
					while ( wordStart > 0 && g_consoleField.buffer[wordStart - 1] != ' ' && g_consoleField.buffer[wordStart - 1] != '\t' )
						wordStart--;
				}
				memmove( g_consoleField.buffer + wordStart,
					g_consoleField.buffer + cursor,
					len - cursor + 1 );
				g_consoleField.cursor = wordStart;
				if ( g_consoleField.cursor < g_consoleField.scroll )
					g_consoleField.scroll = g_consoleField.cursor;
			}
			return;
		}
		// plain backspace: fall through to the SE_CHAR path in Field_CharEvent
	}

	// Cmd+A/C/V/X: macOS text editing shortcuts for the console input field.
	if ( keys[K_COMMAND].down ) {
		switch ( tolower(key) ) {
			case 'a':	// Cmd+A = select all
				Field_SelectAll( &g_consoleField );
				return;
			case 'c':	// Cmd+C = copy selection
				Field_CopySel( &g_consoleField );
				return;
			case 'v':	// Cmd+V = paste (replaces selection)
				Field_Paste( &g_consoleField );
				return;
			case 'x':	// Cmd+X = cut selection
				Field_CopySel( &g_consoleField );
				Field_DeleteSel( &g_consoleField );
				return;
		}
	}
#endif

	// enter finishes the line
	if ( key == K_ENTER || key == K_KP_ENTER ) {
		// if not in the game explicitly prepend a slash if needed
		if ( cls.state != CA_ACTIVE
			&& g_consoleField.buffer[0] != '\0'
			&& g_consoleField.buffer[0] != '\\'
			&& g_consoleField.buffer[0] != '/' ) {
			char	temp[MAX_EDIT_LINE-1];

			Q_strncpyz( temp, g_consoleField.buffer, sizeof( temp ) );
			Com_sprintf( g_consoleField.buffer, sizeof( g_consoleField.buffer ), "\\%s", temp );
			g_consoleField.cursor++;
		}

		Com_Log( SEV_INFO, LOG_CH(ch_client), "]%s\n", g_consoleField.buffer );

		// leading slash is an explicit command
		if ( g_consoleField.buffer[0] == '\\' || g_consoleField.buffer[0] == '/' ) {
			Cbuf_AddText( g_consoleField.buffer+1 );	// valid command
			Cbuf_AddText( "\n" );
		} else {
			// other text will be chat messages
			if ( !g_consoleField.buffer[0] ) {
				return;	// empty lines just scroll the console without adding to history
			}
			if ( WiredScript_TryEval( g_consoleField.buffer ) ) {
				/* Lua handled it -- skip chat dispatch */
			} else {
				Cbuf_AddText( "cmd say " );
				Cbuf_AddText( g_consoleField.buffer );
				Cbuf_AddText( "\n" );
			}
		}

		// copy line to history buffer
		Con_SaveField( &g_consoleField );

		Field_Clear( &g_consoleField );
		g_consoleField.widthInChars = g_console_field_width;

		if ( cls.state == CA_DISCONNECTED ) {
			SCR_UpdateScreen ();	// force an update, because the command
		}							// may take some time
		return;
	}

	// command completion

	if (key == K_TAB) {
		Field_AutoComplete(&g_consoleField);
		return;
	}

	/* any non-Tab key ends a cycling-completion sequence so the next
	 * Tab starts a fresh match list. */
	Field_ResetCompletionCycle( &g_consoleField );

	// command history (ctrl-p ctrl-n for unix style)

	if ( (key == K_MWHEELUP && keys[K_SHIFT].down) || ( key == K_UPARROW ) || ( key == K_KP_UPARROW ) ||
		 ( ( tolower(key) == 'p' ) && keys[K_CTRL].down ) ) {
		Con_HistoryGetPrev( &g_consoleField );
		g_consoleField.widthInChars = g_console_field_width;
		g_consoleField.selActive = qfalse;
		return;
	}

	if ( (key == K_MWHEELDOWN && keys[K_SHIFT].down) || ( key == K_DOWNARROW ) || ( key == K_KP_DOWNARROW ) ||
		 ( ( tolower(key) == 'n' ) && keys[K_CTRL].down ) ) {
		Con_HistoryGetNext( &g_consoleField );
		g_consoleField.widthInChars = g_console_field_width;
		g_consoleField.selActive = qfalse;
		return;
	}

	// console scrolling
	if ( key == K_PGUP || key == K_MWHEELUP ) {
		if ( keys[K_SHIFT].down ) {	// hold <shift> to granular scrolling
			Con_PageUp( 1 );
		} else {
			Con_PageUp( 0 );		// by one visible page
		}
		return;
	}

	if ( key == K_PGDN || key == K_MWHEELDOWN ) {
		if ( keys[K_SHIFT].down ) {	// hold <shift> to granular scrolling
			Con_PageDown( 1 );
		} else {
			Con_PageDown( 0 );		// by one visible page
		}
		return;
	}

	// ctrl-home = top of console
	if ( key == K_HOME && keys[K_CTRL].down ) {
		Con_Top();
		return;
	}

	// ctrl-end = bottom of console
	if ( key == K_END && keys[K_CTRL].down ) {
		Con_Bottom();
		return;
	}

	// pass to the normal editline routine
	Field_KeyDownEvent( &g_consoleField, key );
}

//============================================================================


/*
================
Message_Key

In game talk message
================
*/
static void Message_Key( int key ) {

	char	buffer[MAX_STRING_CHARS];

	if (key == K_ESCAPE) {
		Key_SetCatcher( Key_GetCatcher( ) & ~KEYCATCH_MESSAGE );
		Field_Clear( &chatField );
		return;
	}

	if ( key == K_ENTER || key == K_KP_ENTER )
	{
		if ( chatField.buffer[0] && cls.state == CA_ACTIVE ) {
			if (chat_playerNum != -1 )

				Com_sprintf( buffer, sizeof( buffer ), "tell %i \"%s\"\n", chat_playerNum, chatField.buffer );

			else if (chat_team)

				Com_sprintf( buffer, sizeof( buffer ), "say_team \"%s\"\n", chatField.buffer );
			else
				Com_sprintf( buffer, sizeof( buffer ), "say \"%s\"\n", chatField.buffer );

			CL_AddReliableCommand( buffer, qfalse );
		}
		Key_SetCatcher( Key_GetCatcher( ) & ~KEYCATCH_MESSAGE );
		Field_Clear( &chatField );
		return;
	}

	Field_KeyDownEvent( &chatField, key );
}

//============================================================================


/*
===================
CL_KeyDownEvent

Called by CL_KeyEvent to handle a keypress
===================
*/
static void CL_KeyDownEvent( int key, unsigned time )
{
	keys[key].down = qtrue;
	keys[key].bound = qfalse;
	keys[key].repeats++;

	if ( keys[key].repeats == 1 ) {
		anykeydown++;
	}

#ifndef _WIN32
	if ( keys[K_ALT].down && key == K_ENTER )
	{
		Cvar_SetValue( "r_fullscreen", !Cvar_VariableIntegerValue( "r_fullscreen" ) );
		Cbuf_ExecuteText( EXEC_APPEND, "vid_restart\n" );
		return;
	}
#endif

	// console key is hardcoded, so the user can never unbind it
	if ( key == K_CONSOLE || ( keys[K_SHIFT].down && key == K_ESCAPE ) ) {
		Con_ToggleConsole_f();
		Key_ClearStates();
		return;
	}

	// hardcoded screenshot key
	if ( key == K_PRINT ) {
		if ( keys[K_SHIFT].down ) {
			Cbuf_ExecuteText( EXEC_APPEND, "screenshotBMP\n" );
		} else {
			Cbuf_ExecuteText( EXEC_APPEND, "screenshotBMP clipboard\n" );
		}
		return;
	}

	// keys can still be used for bound actions
	if ( ( key < 128 || key == K_MOUSE1 ) && cls.state == CA_CINEMATIC && Key_GetCatcher() == 0 ) {
		if ( Cvar_VariableIntegerValue( "com_cameraMode" ) == 0 ) {
			Cvar_Set ("nextdemo","");
			key = K_ESCAPE;
		}
	}

	// escape is always handled special
	if ( key == K_ESCAPE ) {
		// Recovery: if WiredUI is dead and the user is in the fullscreen fallback
		// console (no KEYCATCH_UI/CGAME, no intentional ~ console open), try to
		// bring WiredUI back. Mirrors the Con_DrawConsole fullscreen-fallback gate
		// at cl_console.c:1386 — triggers only when the user is looking at that screen.
		if ( cls.state == CA_DISCONNECTED
		     && !( Key_GetCatcher() & KEYCATCH_CONSOLE )
		     && !( Key_GetCatcher() & ( KEYCATCH_UI | KEYCATCH_CGAME ) )
		     && !WiredUI_IsHealthy() ) {
			WiredUI_Activate();
			return;
		}

#ifdef USE_CURL
		if ( Com_DL_InProgress( &download ) && download.mapAutoDownload ) {
			Com_DL_Cleanup( &download );
		}
#endif
		if ( Key_GetCatcher() & KEYCATCH_CONSOLE ) {
			if ( Con_IsSearchActive() ) {
				// escape closes search first, not console
				Con_SearchClose();
				return;
			}
			// escape always closes console
			Con_ToggleConsole_f();
			Key_ClearStates();
		}

		if ( Key_GetCatcher( ) & KEYCATCH_MESSAGE ) {
			// clear message mode
			Message_Key( key );
			return;
		}

		// escape always gets out of CGAME stuff
		if (Key_GetCatcher( ) & KEYCATCH_CGAME) {
			Key_SetCatcher( Key_GetCatcher( ) & ~KEYCATCH_CGAME );
			VM_Call( cgvm, 1, CG_EVENT_HANDLING, CGAME_EVENT_NONE );
			return;
		}

		if ( !( Key_GetCatcher( ) & KEYCATCH_UI ) ) {
			if ( cls.state == CA_ACTIVE && !clc.demoplaying ) {
				UI_CALL_SET_ACTIVE( UIMENU_INGAME );
			}
			else if ( cls.state != CA_DISCONNECTED ) {
#if 0
				CL_Disconnect_f();
				S_StopAllSounds();
#else
				Cmd_Clear();
				Com_ClearLastError();
				if ( cls.state == CA_CINEMATIC ) {
					SCR_StopCinematic();
				} else if ( !CL_Disconnect( qfalse ) ) { // restart client if not done already
					CL_FlushMemory();
				}
#endif
				UI_CALL_SET_ACTIVE( UIMENU_MAIN );
			}
			return;
		}

		UI_CALL_KEY_EVENT( key, qtrue );
		return;
	}

	// distribute the key down event to the appropriate handler
	if ( Key_GetCatcher( ) & KEYCATCH_CONSOLE ) {
		Console_Key( key );
	} else if ( Key_GetCatcher( ) & KEYCATCH_UI ) {
		if ( UI_VM_ACTIVE ) {
			UI_CALL_KEY_EVENT( key, qtrue );
		}
	} else if ( Key_GetCatcher( ) & KEYCATCH_CGAME ) {
		if ( cgvm ) {
			VM_Call( cgvm, 2, CG_KEY_EVENT, key, qtrue );
		}
	} else if ( Key_GetCatcher( ) & KEYCATCH_MESSAGE ) {
		Message_Key( key );
	} else if ( cls.state == CA_DISCONNECTED ) {
		Console_Key( key );
	} else {
		// send the bound action
		Key_ParseBinding( key, qtrue, time );
	}
}


/*
===================
CL_KeyUpEvent

Called by CL_KeyEvent to handle a keyrelease
===================
*/
static void CL_KeyUpEvent( int key, unsigned time )
{
	const qboolean bound = keys[key].bound;

	keys[key].repeats = 0;
	keys[key].down = qfalse;
	keys[key].bound = qfalse;

	if ( --anykeydown < 0 ) {
		anykeydown = 0;
	}

	// don't process key-up events for the console key
	if ( key == K_CONSOLE || ( key == K_ESCAPE && keys[K_SHIFT].down ) ) {
		return;
	}

	// hardcoded screenshot key
	if ( key == K_PRINT ) {
		return;
	}

	//
	// key up events only perform actions if the game key binding is
	// a button command (leading + sign).  These will be processed even in
	// console mode and menu mode, to keep the character from continuing
	// an action started before a mode switch.
	//
	if ( cls.state != CA_DISCONNECTED ) {
		if ( bound || ( Key_GetCatcher() & KEYCATCH_CGAME ) ) {
			Key_ParseBinding( key, qfalse, time );
		}
	}

	if ( Key_GetCatcher() & KEYCATCH_UI ) {
		if ( UI_VM_ACTIVE ) {
			UI_CALL_KEY_EVENT( key, qfalse );
		}
	} else if ( Key_GetCatcher() & KEYCATCH_CGAME ) {
		if ( cgvm ) {
			VM_Call( cgvm, 2, CG_KEY_EVENT, key, qfalse );
		}
	}
}


/*
===================
CL_KeyEvent

Called by the system for both key up and key down events
===================
*/
void CL_KeyEvent( int key, qboolean down, unsigned time )
{
	if ( down )
		CL_KeyDownEvent( key, time );
	else
		CL_KeyUpEvent( key, time );
}


/*
===================
CL_CharEvent

Normal keyboard characters, already shifted / capslocked / etc
===================
*/
void CL_CharEvent( int key )
{
	// distribute the key down event to the appropriate handler
	if ( Key_GetCatcher( ) & KEYCATCH_CONSOLE )
	{
		if ( Con_IsSearchActive() ) {
			// Search mode: K_BACKSPACE etc. arrive via Console_Key (SE_KEY path).
			// SDL also fires SE_CHAR for those keys, so drop control chars here
			// to avoid double-dispatch.
			if ( key < 32 || key == 127 )
				return;
			Con_SearchChar( key );
			return;
		}
		Field_CharEvent( &g_consoleField, key );
	}
	else if ( Key_GetCatcher( ) & KEYCATCH_UI )
	{
		UI_CALL_KEY_EVENT( key | K_CHAR_FLAG, qtrue );
	}
	else if ( Key_GetCatcher( ) & KEYCATCH_MESSAGE )
	{
		Field_CharEvent( &chatField, key );
	}
	else if ( cls.state == CA_DISCONNECTED )
	{
		if ( Con_IsSearchActive() ) {
			if ( key < 32 || key == 127 )
				return;
			Con_SearchChar( key );
			return;
		}
		Field_CharEvent( &g_consoleField, key );
	}
}


/*
===================
Key_ClearStates
===================
*/
void Key_ClearStates( void )
{
	anykeydown = 0;

	for ( int i = 0 ; i < MAX_KEYS ; i++ )
	{
		if ( keys[i].down )
			CL_KeyEvent( i, qfalse, 0 );

		keys[i].down = qfalse;
		keys[i].repeats = 0;
	}
}


static int keyCatchers = 0;

/*
====================
Key_GetCatcher
====================
*/
int Key_GetCatcher( void )
{
	return keyCatchers;
}


/*
====================
Key_SetCatcher
====================
*/
void Key_SetCatcher( int catcher )
{
	// If the catcher state is changing, clear all key states
	if ( catcher != keyCatchers )
		Key_ClearStates();

	keyCatchers = catcher;
}
