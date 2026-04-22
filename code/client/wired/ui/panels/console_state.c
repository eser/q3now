/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// console_state.c

#include "../../../client.h"
#include "../../../../qcommon/arena.h"
#include "console_private.h"
#include "../cl_wired_text.h"

int bigchar_width;
int bigchar_height;
int smallchar_width;
int smallchar_height;

/* ── Text_Draw console metrics ────────────────────────────────────── */

cvar_t		*con_lineheight;

float con_textPointSize   = 0;
float con_lineAdvance     = SMALLCHAR_HEIGHT;
float con_textCharWidth   = 0;
float con_textNativeCharW = 0;

/*
================
Con_UpdateTextMetrics

Recompute point size and character width from the current
smallchar_height.  Uses the unified Text_Measure API.
All values are in real screen pixels.
================
*/
static void Con_UpdateTextMetrics( void ) {
	if ( cls.glconfig.vidHeight <= 0 ) return;

	/* smallchar_height is already in real screen pixels */
	con_textPointSize = (float)smallchar_height;
	con_lineAdvance   = con_textPointSize * ( con_lineheight ? con_lineheight->value : 1.0f );

	/* measure a monospace character for grid width (real pixels) */
	con_textCharWidth = Text_Measure( "M", FONT_MONO, con_textPointSize );
	if ( con_textCharWidth < 1.0f ) {
		con_textCharWidth = (float)smallchar_width;
	}

	/* native-pixel equivalent — same as con_textCharWidth now */
	con_textNativeCharW = con_textCharWidth;
	if ( con_textNativeCharW < 1.0f ) {
		con_textNativeCharW = (float)smallchar_width;
	}
}

// forward declarations
void Con_SearchClose( void );
void Con_MarkOpen( void );
void Con_MarkClose( void );
qboolean Con_IsMarkActive( void );
qboolean Con_MarkKey( int key, qboolean ctrlDown, qboolean shiftDown );
qboolean Con_MarkCellIsSelected( int row, int col );

/* Console state lives in a persistent arena so it survives Hunk_ClearLevel().
   All code uses `con.field` — the macro expands to (*s_con).field which is
   an lvalue, so assignments (con.field = ...) compile correctly.           */
static arena_t   *s_conArena = NULL;
console_t        *s_con      = NULL;

cvar_t		*con_conspeed;
cvar_t		*con_autoclear;
cvar_t		*con_notifytime;
cvar_t		*con_notifylines;
cvar_t		*con_scale;
cvar_t		*con_anim;
cvar_t		*con_clock;
cvar_t		*con_fade;
cvar_t		*con_fps;
cvar_t		*cl_consoleHeight;
cvar_t		*cl_consoleType;
cvar_t		*con_timestamp;

/* CNQ3 backport: per-element console colors.  Each cvar stores a hex
   string in the form "RRGGBB" or "RRGGBBAA"; we parse them lazily every
   frame (string comparison guards against rebuilding the vec4 when the
   value has not changed). */
cvar_t		*con_colBG;
cvar_t		*con_colBorder;
cvar_t		*con_colText;
cvar_t		*con_colCVar;
cvar_t		*con_colCmd;
cvar_t		*con_colValue;

vec4_t con_bgColor     = { 0.063f, 0.074f, 0.074f, 0.965f };
vec4_t con_borderColor = { 0.278f, 0.470f, 0.698f, 1.0f };
vec4_t con_textColor   = { 0.886f, 0.886f, 0.886f, 1.0f };
vec4_t con_cvarColor   = { 0.278f, 0.470f, 0.698f, 1.0f };
vec4_t con_cmdColor    = { 0.309f, 0.654f, 0.741f, 1.0f };
vec4_t con_valueColor  = { 0.898f, 0.737f, 0.223f, 1.0f };

static char con_bgString[16]     = "";
static char con_borderString[16] = "";
static char con_textString[16]   = "";
static char con_cvarString[16]   = "";
static char con_cmdString[16]    = "";
static char con_valueString[16]  = "";

int			g_console_field_width;

/*
================
Con_ToggleConsole_f
================
*/
void Con_ToggleConsole_f( void ) {
	// Can't toggle the console when it's the only thing available
    if ( cls.state == CA_DISCONNECTED && Key_GetCatcher() == KEYCATCH_CONSOLE ) {
		return;
	}

	if ( con_autoclear->integer ) {
		Field_Clear( &g_consoleField );
	}

	g_consoleField.widthInChars = g_console_field_width;

	Con_ClearNotify();
	Key_SetCatcher( Key_GetCatcher() ^ KEYCATCH_CONSOLE );
}


/*
================
Con_MessageMode_f
================
*/
static void Con_MessageMode_f( void ) {
	chat_playerNum = -1;
	chat_team = qfalse;
	Field_Clear( &chatField );
	chatField.widthInChars = 30;

	Key_SetCatcher( Key_GetCatcher() ^ KEYCATCH_MESSAGE );
}


/*
================
Con_MessageMode2_f
================
*/
static void Con_MessageMode2_f( void ) {
	chat_playerNum = -1;
	chat_team = qtrue;
	Field_Clear( &chatField );
	chatField.widthInChars = 25;
	Key_SetCatcher( Key_GetCatcher() ^ KEYCATCH_MESSAGE );
}


/*
================
Con_MessageMode3_f
================
*/
static void Con_MessageMode3_f( void ) {
	chat_playerNum = cgvm ? VM_Call( cgvm, 0, CG_CROSSHAIR_PLAYER ) : -1;
	if ( chat_playerNum < 0 || chat_playerNum >= MAX_CLIENTS ) {
		chat_playerNum = -1;
		return;
	}
	chat_team = qfalse;
	Field_Clear( &chatField );
	chatField.widthInChars = 30;
	Key_SetCatcher( Key_GetCatcher() ^ KEYCATCH_MESSAGE );
}


/*
================
Con_MessageMode4_f
================
*/
static void Con_MessageMode4_f( void ) {
	chat_playerNum = cgvm ? VM_Call( cgvm, 0, CG_LAST_ATTACKER ) : -1;
	if ( chat_playerNum < 0 || chat_playerNum >= MAX_CLIENTS ) {
		chat_playerNum = -1;
		return;
	}
	chat_team = qfalse;
	Field_Clear( &chatField );
	chatField.widthInChars = 30;
	Key_SetCatcher( Key_GetCatcher() ^ KEYCATCH_MESSAGE );
}


/*
================
Con_Clear_f
================
*/
static void Con_Clear_f( void ) {
	for ( int i = 0 ; i < con.linewidth ; i++ ) {
		con.text[i] = ( ColorIndex( COLOR_WHITE ) << 8 ) | ' ';
	}

	con.x = 0;
	con.current = 0;
	con.newline = qtrue;

	Con_Bottom();		// go to end
}


/*
================
Con_Dump_f

Save the console contents out to a file
================
*/
static void Con_Dump_f( void )
{
	if ( Cmd_Argc() != 2 )
	{
		Com_Printf( "usage: condump <filename>\n" );
		return;
	}

	char	filename[ MAX_OSPATH ];
	Q_strncpyz( filename, Cmd_Argv( 1 ), sizeof( filename ) );
	COM_DefaultExtension( filename, sizeof( filename ), ".txt" );

	const char *ext;
	if ( !FS_AllowedExtension( filename, qfalse, &ext ) ) {
		Com_Printf( "%s: Invalid filename extension '%s'.\n", __func__, ext );
		return;
	}

	fileHandle_t f = FS_FOpenFileWrite( filename );
	if ( f == FS_INVALID_HANDLE )
	{
		Com_Printf( "ERROR: couldn't open %s.\n", filename );
		return;
	}

	Com_Printf( "Dumped console text to %s.\n", filename );

	int n, l;
	if ( con.current >= con.totallines ) {
		n = con.totallines;
		l = con.current + 1;
	} else {
		n = con.current + 1;
		l = 0;
	}

	int bufferlen = con.linewidth + ARRAY_LEN( Q_NEWLINE ) * sizeof( char );
	char *buffer = Hunk_AllocateTempMemory( bufferlen );

	// write the remaining lines
	buffer[ bufferlen - 1 ] = '\0';

	for ( int i = 0; i < n ; i++, l++ )
	{
		short *line = con.text + (l % con.totallines) * con.linewidth;
		// store line
		for( int x = 0; x < con.linewidth; x++ )
			buffer[ x ] = line[ x ] & 0xff;
		buffer[ con.linewidth ] = '\0';
		// terminate on ending space characters
		for ( int x = con.linewidth - 1 ; x >= 0 ; x-- ) {
			if ( buffer[ x ] == ' ' )
				buffer[ x ] = '\0';
			else
				break;
		}
		{ qstring_t _b_qs = QS_WrapExisting( buffer, bufferlen ); QS_Append( &_b_qs, Q_NEWLINE ); }
		FS_Write( buffer, strlen( buffer ), f );
	}

	Hunk_FreeTempMemory( buffer );
	FS_FCloseFile( f );
}


/*
================
Con_ClearNotify
================
*/
void Con_ClearNotify( void ) {
	for ( int i = 0 ; i < NUM_CON_TIMES ; i++ ) {
		con.times[i] = 0;
	}
}


/*
================
Con_CheckResize

If the line width has changed, reformat the buffer.
================
*/
void Con_CheckResize( void )
{
	short	tbuf[CON_TEXTSIZE];
	static int old_width, old_vispage;

	if ( con.viswidth == cls.glconfig.vidWidth && !con_scale->modified && !con_lineheight->modified ) {
		return;
	}

	float scale = con_scale->value;

	con.viswidth = cls.glconfig.vidWidth;

	smallchar_width = SMALLCHAR_WIDTH * scale * cls.con_factor;
	smallchar_height = SMALLCHAR_HEIGHT * scale * cls.con_factor;
	bigchar_width = BIGCHAR_WIDTH * scale * cls.con_factor;
	bigchar_height = BIGCHAR_HEIGHT * scale * cls.con_factor;

	Con_UpdateTextMetrics();

	if ( cls.glconfig.vidWidth == 0 ) // video hasn't been initialized yet
	{
		int width = DEFAULT_CONSOLE_WIDTH * scale;
		if ( width < 40 ) width = 40;
		g_console_field_width = DEFAULT_CONSOLE_WIDTH;
		con.linewidth = width;
		con.totallines = CON_TEXTSIZE / con.linewidth;
		con.vispage = 4;

		Con_Clear_f();
	}
	else
	{
		int width;
		if ( con_textCharWidth > 0 ) {
			width = (int)( (float)cls.glconfig.vidWidth / con_textNativeCharW ) - 2;
		} else {
			/* Text system not ready yet (early init) — use a reasonable default */
			width = DEFAULT_CONSOLE_WIDTH;
		}

		if ( width < 40 ) width = 40;

		g_console_field_width = width;
		g_consoleField.widthInChars = g_console_field_width;

		int vispage = cls.glconfig.vidHeight / ( (int)con_lineAdvance * 2 ) - 1;

		if ( old_vispage == vispage && old_width == width )
			return;

		int oldwidth = con.linewidth;
		int oldtotallines = con.totallines;
		int oldcurrent = con.current;

		con.linewidth = width;
		con.totallines = CON_TEXTSIZE / con.linewidth;
		con.vispage = vispage;

		old_vispage = vispage;
		old_width = width;

		int numchars = oldwidth;
		if ( numchars > con.linewidth )
			numchars = con.linewidth;

		int numlines;
		if ( oldcurrent > oldtotallines )
			numlines = oldtotallines;
		else
			numlines = oldcurrent + 1;

		if ( numlines > con.totallines )
			numlines = con.totallines;

		memcpy( tbuf, con.text, CON_TEXTSIZE * sizeof( short ) );

		for ( int i = 0; i < CON_TEXTSIZE; i++ )
			con.text[i] = (ColorIndex(COLOR_WHITE)<<8) | ' ';

		for ( int i = 0; i < numlines; i++ )
		{
			short *src = &tbuf[ ((oldcurrent - i + oldtotallines) % oldtotallines) * oldwidth ];
			short *dst = &con.text[ (numlines - 1 - i) * con.linewidth ];
			for ( int j = 0; j < numchars; j++ )
				*dst++ = *src++;
		}

		Con_ClearNotify();

		con.current = numlines - 1;
	}

	con.display = con.current;

	con_scale->modified = qfalse;
	con_lineheight->modified = qfalse;
}


/*
==================
Cmd_CompleteTxtName
==================
*/
static void Cmd_CompleteTxtName(const char *args, int argNum ) {
	if ( argNum == 2 ) {
		Field_CompleteFilename( "", "txt", qfalse, FS_MATCH_EXTERN | FS_MATCH_STICK );
	}
}


/*
================
Con_HexCharToInt

Returns the 0..15 value of a hex character, or -1 for invalid input.
================
*/
static int Con_HexCharToInt( char c ) {
	if ( c >= '0' && c <= '9' ) return c - '0';
	if ( c >= 'a' && c <= 'f' ) return 10 + (c - 'a');
	if ( c >= 'A' && c <= 'F' ) return 10 + (c - 'A');
	return -1;
}


/*
================
Con_ParseHexColor

Parses a 6- or 8-character hex color string into a vec4.  Returns
qtrue on success.  Supports "RRGGBB" (alpha = 1) and "RRGGBBAA".
Whitespace and a leading '#' are tolerated.
================
*/
static qboolean Con_ParseHexColor( const char *str, vec4_t out ) {
	if ( str == NULL || out == NULL )
		return qfalse;

	char buf[16];
	int nyb[8];

	/* trim leading whitespace and '#' */
	while ( *str == ' ' || *str == '\t' )
		str++;
	if ( *str == '#' )
		str++;

	int len = 0;
	while ( str[len] && len < (int)sizeof(buf) - 1 ) {
		if ( str[len] == ' ' || str[len] == '\t' )
			break;
		buf[len] = str[len];
		len++;
	}
	buf[len] = '\0';

	if ( len != 6 && len != 8 )
		return qfalse;

	for ( int i = 0; i < len; i++ ) {
		nyb[i] = Con_HexCharToInt( buf[i] );
		if ( nyb[i] < 0 )
			return qfalse;
	}

	out[0] = (float)( (nyb[0] << 4) | nyb[1] ) / 255.0f;
	out[1] = (float)( (nyb[2] << 4) | nyb[3] ) / 255.0f;
	out[2] = (float)( (nyb[4] << 4) | nyb[5] ) / 255.0f;
	if ( len == 8 ) {
		out[3] = (float)( (nyb[6] << 4) | nyb[7] ) / 255.0f;
	} else {
		out[3] = 1.0f;
	}
	return qtrue;
}


/*
================
Con_UpdateColor

Re-parses one cvar into its backing vec4 when the string value changed.
If the cvar string is empty or invalid, the previous value is kept so
users can clear a cvar without losing the default color.
================
*/
static void Con_UpdateColor( cvar_t *cv, char *lastString, int lastSize,
                             vec4_t out, const vec4_t fallback )
{
	if ( cv == NULL )
		return;

	if ( strcmp( cv->string, lastString ) == 0 )
		return;

	Q_strncpyz( lastString, cv->string, lastSize );

	if ( cv->string[0] == '\0' ) {
		Vector4Copy( fallback, out );
		return;
	}

	if ( !Con_ParseHexColor( cv->string, out ) ) {
		Com_Printf( S_COLOR_YELLOW "WARNING: invalid hex color in %s: '%s'\n",
			cv->name, cv->string );
		Vector4Copy( fallback, out );
	}
}


/*
================
Con_UpdateColors

Refreshes every per-element cvar.  Cheap — short-circuits when the
cvar string has not changed since last call.
================
*/
void Con_UpdateColors( void ) {
	static const vec4_t defBG     = { 0.063f, 0.074f, 0.074f, 0.965f };
	static const vec4_t defBorder = { 0.278f, 0.470f, 0.698f, 1.0f };
	static const vec4_t defText   = { 0.886f, 0.886f, 0.886f, 1.0f };
	static const vec4_t defCVar   = { 0.278f, 0.470f, 0.698f, 1.0f };
	static const vec4_t defCmd    = { 0.309f, 0.654f, 0.741f, 1.0f };
	static const vec4_t defValue  = { 0.898f, 0.737f, 0.223f, 1.0f };

	Con_UpdateColor( con_colBG,     con_bgString,     sizeof(con_bgString),
	                 con_bgColor,     defBG );
	Con_UpdateColor( con_colBorder, con_borderString, sizeof(con_borderString),
	                 con_borderColor, defBorder );
	Con_UpdateColor( con_colText,   con_textString,   sizeof(con_textString),
	                 con_textColor,   defText );
	Con_UpdateColor( con_colCVar,   con_cvarString,   sizeof(con_cvarString),
	                 con_cvarColor,   defCVar );
	Con_UpdateColor( con_colCmd,    con_cmdString,    sizeof(con_cmdString),
	                 con_cmdColor,    defCmd );
	Con_UpdateColor( con_colValue,  con_valueString,  sizeof(con_valueString),
	                 con_valueColor,  defValue );
}


/*
================
Con_Init
================
*/
void Con_Init( void )
{
	/* Allocate console state from the persistent arena if not already done.
	   May have been lazily allocated by CL_ConsolePrint before this call.
	   The arena survives Hunk_ClearLevel() so text and scroll position are
	   preserved across map transitions.  On re-entry (renderer restart),
	   the arena already exists — just re-init cvars and commands below. */
	if ( !s_conArena ) {
		s_conArena = Arena_Create( "Console", CONSOLE_ARENA_SIZE );
		s_con = Arena_AllocType( s_conArena, console_t );
		memset( s_con, 0, sizeof( console_t ) );
	} else if ( !s_con ) {
		/* arena was created by lazy init, s_con was set there; shouldn't be NULL here */
		s_con = Arena_AllocType( s_conArena, console_t );
		memset( s_con, 0, sizeof( console_t ) );
	}

	con_notifytime = Cvar_Get( "con_notifytime", "5", CVAR_ARCHIVE );
	Cvar_SetDescription( con_notifytime, "Defines how long messages (from players or the system) are on the screen (in seconds)." );
	con_notifylines = Cvar_Get ( "con_notifylines", "3", CVAR_ARCHIVE );
	Cvar_CheckRange(con_notifylines, "1", va( "%i", NUM_CON_TIMES - 1), CV_INTEGER);
	Cvar_SetDescription( con_notifylines, "Defines the number of lines to display in the notify area." );
	con_conspeed = Cvar_Get( "scr_conspeed", "3", CVAR_ARCHIVE );
	Cvar_SetDescription( con_conspeed, "Console opening/closing scroll speed." );
	con_autoclear = Cvar_Get("con_autoclear", "1", CVAR_ARCHIVE_ND);
	Cvar_SetDescription( con_autoclear, "Enable/disable clearing console input text when console is closed." );
	con_scale = Cvar_Get( "con_scale", "1", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( con_scale, "0.5", "8", CV_FLOAT );
	Cvar_SetDescription( con_scale, "Console font size scale." );
	con_lineheight = Cvar_Get( "con_lineheight", "1", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( con_lineheight, "0.5", "4", CV_FLOAT );
	Cvar_SetDescription( con_lineheight, "Notify/console line height multiplier (1.0 = tight, 2.0 = double-spaced)." );

	con_anim  = Cvar_Get( "con_anim",  "1", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( con_anim,  "Animate console open/close. 0 = instant snap." );
	cl_consoleHeight = Cvar_Get( "cl_consoleHeight", "0.5", CVAR_ARCHIVE );
	Cvar_CheckRange( cl_consoleHeight, "0.1", "1", CV_FLOAT );
	Cvar_SetDescription( cl_consoleHeight, "Fraction of screen height covered by the console when open (0.1-1.0)." );
	cl_consoleType = Cvar_Get( "cl_consoleType", "0", CVAR_ARCHIVE );
	Cvar_CheckRange( cl_consoleType, "0", "1", CV_INTEGER );
	Cvar_SetDescription( cl_consoleType, "Console background style: 0=themed (con_colBG), 1=classic Q3 shader." );
	con_clock = Cvar_Get( "con_clock", "1", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( con_clock, "Draw wall-clock HH:MM:SS in the top-right corner of the console." );
	con_fade  = Cvar_Get( "con_fade",  "1", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( con_fade,  "Fade notify lines to transparent before they expire instead of popping." );
	con_fps   = Cvar_Get( "con_fps",   "1", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( con_fps,   "Draw current FPS in the top-right corner of the console." );
	con_timestamp = Cvar_Get( "con_timestamp", "1", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( con_timestamp, "Prefix each console line with a HH:MM:SS timestamp." );

	/* CNQ3 backport: per-element console colors */
	con_colBG     = Cvar_Get( "con_colBG",     "101013F6", CVAR_ARCHIVE );
	Cvar_SetDescription( con_colBG, "Console background color (hex RRGGBB or RRGGBBAA)." );
	con_colBorder = Cvar_Get( "con_colBorder", "4778B2FF", CVAR_ARCHIVE );
	Cvar_SetDescription( con_colBorder, "Console border color (hex RRGGBB or RRGGBBAA)." );
	con_colText   = Cvar_Get( "con_colText",   "E2E2E2",   CVAR_ARCHIVE );
	Cvar_SetDescription( con_colText, "Console text color (hex RRGGBB)." );
	con_colCVar   = Cvar_Get( "con_colCVar",   "4778B2",   CVAR_ARCHIVE );
	Cvar_SetDescription( con_colCVar, "Console input color for cvar names (hex RRGGBB)." );
	con_colCmd    = Cvar_Get( "con_colCmd",    "4FA7BD",   CVAR_ARCHIVE );
	Cvar_SetDescription( con_colCmd, "Console input color for command names (hex RRGGBB)." );
	con_colValue  = Cvar_Get( "con_colValue",  "E5BC39",   CVAR_ARCHIVE );
	Cvar_SetDescription( con_colValue, "Console input color for cvar values (hex RRGGBB)." );

	Con_UpdateColors();

	Field_Clear( &g_consoleField );
	g_consoleField.widthInChars = g_console_field_width;

	Cmd_AddCommand( "clear", Con_Clear_f );
	Cmd_AddCommand( "condump", Con_Dump_f );
	Cmd_SetCommandCompletionFunc( "condump", Cmd_CompleteTxtName );
	Cmd_AddCommand( "toggleconsole", Con_ToggleConsole_f );
	Cmd_AddCommand( "messagemode", Con_MessageMode_f );
	Cmd_AddCommand( "messagemode2", Con_MessageMode2_f );
	Cmd_AddCommand( "messagemode3", Con_MessageMode3_f );
	Cmd_AddCommand( "messagemode4", Con_MessageMode4_f );
}


/*
================
Con_Shutdown
================
*/
void Con_Shutdown( void )
{
	Cmd_RemoveCommand( "clear" );
	Cmd_RemoveCommand( "condump" );
	Cmd_RemoveCommand( "toggleconsole" );
	Cmd_RemoveCommand( "messagemode" );
	Cmd_RemoveCommand( "messagemode2" );
	Cmd_RemoveCommand( "messagemode3" );
	Cmd_RemoveCommand( "messagemode4" );
}


/*
===============
Con_Fixup
===============
*/
static void Con_Fixup( void )
{
	int filled;

	if ( con.current >= con.totallines ) {
		filled = con.totallines;
	} else {
		filled = con.current + 1;
	}

	if ( filled <= con.vispage ) {
		con.display = con.current;
	} else if ( con.current - con.display > filled - con.vispage ) {
		con.display = con.current - filled + con.vispage;
	} else if ( con.display > con.current ) {
		con.display = con.current;
	}
}


/*
===============
Con_Linefeed

Move to newline only when we _really_ need this
===============
*/
static void Con_NewLine( void )
{
	// follow last line
	if ( con.display == con.current )
		con.display++;
	con.current++;

	short *s = &con.text[ ( con.current % con.totallines ) * con.linewidth ];
	for ( int i = 0; i < con.linewidth ; i++ )
		*s++ = (ColorIndex(COLOR_WHITE)<<8) | ' ';

	con.x = 0;
}


/*
===============
Con_Linefeed
===============
*/
static void Con_Linefeed( qboolean skipnotify )
{
	// mark time for transparent overlay
	if ( con.current >= 0 )	{
		if ( skipnotify )
			con.times[ con.current % NUM_CON_TIMES ] = 0;
		else
			con.times[ con.current % NUM_CON_TIMES ] = cls.realtime;
	}

	if ( con.newline ) {
		Con_NewLine();
	} else {
		con.newline = qtrue;
		con.x = 0;
	}

	Con_Fixup();
}


/*
================
CL_ConsolePrint

Handles cursor positioning, line wrapping, etc
All console printing must go through this in order to be logged to disk
If no console is visible, the text will appear at the top of the game window
================
*/
void CL_ConsolePrint( const char *txt ) {
	qboolean skipnotify = qfalse;		// NERVE - SMF

	// TTimo - prefix for text that shows up in console but not in notify
	// backported from RTCW
	if ( !strncmp( txt, "[skipnotify]", 12 ) ) {
		skipnotify = qtrue;
		txt += 12;
	}

	// for some demos we don't want to ever show anything on the console
	if ( cl_noprint && cl_noprint->integer ) {
		return;
	}

	/* Lazily allocate the arena on first print (may happen before Con_Init
	   due to early Com_Printf calls at engine startup).  Con_Init completes
	   the setup (cvars, cvar-based colors, cmd registration) later. */
	if ( !s_con ) {
		if ( !s_conArena ) {
			s_conArena = Arena_Create( "Console", CONSOLE_ARENA_SIZE );
			s_con = Arena_AllocType( s_conArena, console_t );
			memset( s_con, 0, sizeof( console_t ) );
		} else {
			return;   /* arena exists but s_con is NULL — shouldn't happen */
		}
	}

	if ( !con.initialized ) {
		static cvar_t null_cvar = { 0 };
		con.color[0] =
		con.color[1] =
		con.color[2] =
		con.color[3] = 1.0f;
		con.viswidth = -9999;
		cls.con_factor = 1.0f;
		con_scale = &null_cvar;
		con_scale->value = 1.0f;
		con_scale->modified = qtrue;
		con_lineheight = &null_cvar;
		con_lineheight->value = 1.0f;
		con_lineheight->modified = qtrue;
		Con_CheckResize();
		con.initialized = qtrue;
	}

	int colorIndex = ColorIndex( COLOR_WHITE );

	int c;
	while ( (c = *txt) != 0 ) {
		if ( Q_IsColorString( txt ) && *(txt+1) != '\n' ) {
			colorIndex = ColorIndexFromChar( *(txt+1) );
			txt += 2;
			continue;
		}

		// count word length
		int l;
		for ( l = 0 ; l < con.linewidth ; l++ ) {
			if ( txt[l] <= ' ' ) {
				break;
			}
		}

		// word wrap — only if the word fits on a fresh line.
		// With timestamps (9 cols), a word of length l requires l + 9 <= linewidth.
		// If the word is too long to fit even on a fresh line, let overflow
		// handle it char-by-char instead of cascading single-char wraps.
		{
			int ts_cols = ( con_timestamp && con_timestamp->integer ) ? 9 : 0;
			if ( l < con.linewidth - ts_cols && ( con.x + l >= con.linewidth ) ) {
				Con_Linefeed( skipnotify );
			}
		}

		txt++;

		switch( c )
		{
		case '\n':
			Con_Linefeed( skipnotify );
			break;
		case '\r':
			con.x = 0;
			break;
		default:
			if ( con.newline ) {
				Con_NewLine();
				Con_Fixup();
				con.newline = qfalse;
			}
			// display character and advance
			{
				int y = con.current % con.totallines;
				con.text[y * con.linewidth + con.x ] = (colorIndex << 8) | (c & 255);
				con.x++;
				if ( con.x >= con.linewidth ) {
					Con_Linefeed( skipnotify );
				}
			}
			break;
		}
	}

	// mark time for transparent overlay
	if ( con.current >= 0 ) {
		if ( skipnotify ) {
			int prev = con.current % NUM_CON_TIMES - 1;
			if ( prev < 0 )
				prev = NUM_CON_TIMES - 1;
			con.times[ prev ] = 0;
		} else {
			con.times[ con.current % NUM_CON_TIMES ] = cls.realtime;
		}
	}
}

//================================================================

/*
==================
Con_RunConsole

Scroll it up or down
==================
*/
void Con_RunConsole( void )
{
	// decide on the destination height of the console
	if ( Key_GetCatcher( ) & KEYCATCH_CONSOLE )
		con.finalFrac = cl_consoleHeight->value;
	else
		con.finalFrac = 0.0;	// none visible

	// instant snap when animation is disabled
	if ( !con_anim->integer ) {
		con.displayFrac = con.finalFrac;
		return;
	}

	// scroll towards the destination height
	if ( con.finalFrac < con.displayFrac )
	{
		con.displayFrac -= con_conspeed->value * cls.realFrametime * 0.001;
		if ( con.finalFrac > con.displayFrac )
			con.displayFrac = con.finalFrac;

	}
	else if ( con.finalFrac > con.displayFrac )
	{
		con.displayFrac += con_conspeed->value * cls.realFrametime * 0.001;
		if ( con.finalFrac < con.displayFrac )
			con.displayFrac = con.finalFrac;
	}
}


void Con_PageUp( int lines )
{
	if ( lines == 0 )
		lines = con.vispage - 2;

	con.display -= lines;

	Con_Fixup();
}


void Con_PageDown( int lines )
{
	if ( lines == 0 )
		lines = con.vispage - 2;

	con.display += lines;

	Con_Fixup();
}


void Con_Top( void )
{
	// this is generally incorrect but will be adjusted in Con_Fixup()
	con.display = con.current - con.totallines;

	Con_Fixup();
}


void Con_Bottom( void )
{
	con.display = con.current;

	Con_Fixup();
}


void Con_Close( void )
{
	if ( !com_cl_running->integer )
		return;

	Con_SearchClose();
	Field_Clear( &g_consoleField );
	Con_ClearNotify();
	Key_SetCatcher( Key_GetCatcher( ) & ~KEYCATCH_CONSOLE );
	con.finalFrac = 0.0;			// none visible
	con.displayFrac = 0.0;
}


/*
================
Con_SoftClose

Collapses the console visually (displayFrac/finalFrac = 0) and clears
notify lines, but preserves the KEYCATCH_CONSOLE bit so the user does
not have to re-open the console after a map transition.  Use this in
place of Con_Close() on any code path that is NOT a deliberate "hide
the console from the user" action (e.g. map-change, cgame init).
================
*/
void Con_SoftClose( void )
{
	if ( !com_cl_running->integer )
		return;

	con.finalFrac   = 0.0f;
	con.displayFrac = 0.0f;
	Con_ClearNotify();
	/* KEYCATCH_CONSOLE deliberately preserved */
}


/*
==============================================================================

CONSOLE SEARCH

==============================================================================
*/

/*
================
Con_LineToString

Extract ASCII text from a console line into a buffer.
Strips Q3 color codes (high byte) and trailing spaces.
================
*/
static void Con_LineToString( int line, char *buf, int bufSize ) {
	short *text = con.text + ( line % con.totallines ) * con.linewidth;
	int len = con.linewidth;
	if ( len >= bufSize )
		len = bufSize - 1;

	for ( int x = 0; x < len; x++ )
		buf[x] = text[x] & 0xff;

	buf[len] = '\0';

	// trim trailing spaces
	for ( int x = len - 1; x >= 0 && buf[x] == ' '; x-- )
		buf[x] = '\0';
}


/*
================
Con_SearchFind

Search for the current pattern starting from startLine, going in direction.
Returns the matching line index, or -1 if not found.
direction: -1 = backward (toward older lines), +1 = forward (toward newer lines)
================
*/
static int Con_SearchFind( int startLine, int direction ) {
	if ( !con.searchPattern[0] )
		return -1;

	char lineBuf[CON_LINEBUF_SIZE];

	/* Valid line range: [oldest, con.current].
	   Use modular arithmetic so backward search wraps correctly even when
	   the ring buffer is full (oldest > 0). The old code used
	   "while (line < 0) line += numLines" which never triggered for a full
	   buffer since oldest is positive, causing backward wrap to silently
	   fail. */
	int numLines, oldest;
	if ( con.current >= con.totallines ) {
		numLines = con.totallines;
		oldest   = con.current - con.totallines + 1;
	} else {
		numLines = con.current + 1;
		oldest   = 0;
	}

	/* Clamp startLine into the valid window — it can go stale if new lines
	   arrived after the last search (con.current advanced). */
	if ( startLine < oldest )   startLine = oldest;
	if ( startLine > con.current ) startLine = con.current;

	/* pos is an offset within [0, numLines-1] where 0=oldest, numLines-1=newest.
	   Step by direction each iteration; modular wrap stays within valid range.
	   Loop numLines-1 times — visits every line except startLine itself. */
	int pos = startLine - oldest;
	for ( int i = 0; i < numLines - 1; i++ ) {
		pos = ( pos + direction + numLines ) % numLines;
		int line = oldest + pos;

		Con_LineToString( line, lineBuf, sizeof( lineBuf ) );
		if ( Q_stristr( lineBuf, con.searchPattern ) )
			return line;
	}

	return -1;
}


/*
================
Con_SearchCountMatches

Count total occurrences of the search pattern in the console buffer.
================
*/
static int Con_SearchCountMatches( void ) {
	if ( !con.searchPattern[0] )
		return 0;

	char	lineBuf[CON_LINEBUF_SIZE];

	int count = 0;

	int numLines;
	if ( con.current >= con.totallines )
		numLines = con.totallines;
	else
		numLines = con.current + 1;

	for ( int i = 0; i < numLines; i++ ) {
		int line;
		if ( con.current >= con.totallines )
			line = ( con.current + 1 + i ) % con.totallines;
		else
			line = i;

		Con_LineToString( line, lineBuf, sizeof( lineBuf ) );
		if ( Q_stristr( lineBuf, con.searchPattern ) )
			count++;
	}

	return count;
}


/*
================
Con_SearchUpdate

Re-run search from current match position after pattern changes.
================
*/
static int Con_VisibleRows( void ) {
	// Actual on-screen content rows: console pixel height / line advance, minus
	// the 4 reserved slots at the bottom (input, status, Find bar, padding)
	// and 1 more for the ^^^^ scroll indicator when scrolled.
	int lineH = (int)con_lineAdvance;
	if ( lineH < 1 )
		lineH = smallchar_width > 0 ? smallchar_width : 8;
	int vis = ( con.vislines > 0 ) ? ( con.vislines / lineH - 5 ) : ( con.vispage / 2 );
	return vis < 1 ? 1 : vis;
}

static void Con_ScrollToLine( int line ) {
	int half = Con_VisibleRows() / 2;
	con.display = line + 1 + half;
	if ( con.display > con.current )
		con.display = con.current;
	Con_Fixup();
}

static void Con_SearchUpdate( void ) {
	if ( !con.searchPattern[0] ) {
		con.searchLine = -1;
		con.searchMatchCount = 0;
		return;
	}

	con.searchMatchCount = Con_SearchCountMatches();

	// first check if current display line matches (don't jump away needlessly)
	if ( con.display >= 0 && con.current - con.display < con.totallines ) {
		char	lineBuf[CON_LINEBUF_SIZE];
		Con_LineToString( con.display, lineBuf, sizeof( lineBuf ) );
		if ( Q_stristr( lineBuf, con.searchPattern ) ) {
			con.searchLine = con.display;
			return;
		}
	}

	// search backward from current display position
	con.searchLine = Con_SearchFind( con.display, -1 );

	if ( con.searchLine >= 0 )
		Con_ScrollToLine( con.searchLine );
}


/*
================
Con_SearchOpen

Activate console search mode (Ctrl-F).
================
*/
void Con_SearchOpen( void ) {
	con.searchActive = qtrue;
	con.searchPattern[0] = '\0';
	con.searchCursor = 0;
	con.searchLine = -1;
	con.searchMatchCount = 0;
}


/*
================
Con_SearchClose

Deactivate console search mode.
================
*/
void Con_SearchClose( void ) {
	con.searchActive = qfalse;
	con.searchPattern[0] = '\0';
	con.searchCursor = 0;
	con.searchLine = -1;
	con.searchMatchCount = 0;
}


/*
================
Con_SearchNext

Jump to next (forward=qtrue) or previous (forward=qfalse) match.
================
*/
void Con_SearchNext( qboolean forward ) {
	if ( !con.searchActive || !con.searchPattern[0] )
		return;

	int startLine = ( con.searchLine >= 0 ) ? con.searchLine : con.display;
	int dir = forward ? 1 : -1;

	int found = Con_SearchFind( startLine, dir );
	if ( found >= 0 ) {
		con.searchLine = found;
		Con_ScrollToLine( found );
	}
}


/*
================
Con_SearchChar

Handle a character typed into the search bar.
================
*/
void Con_SearchChar( int ch ) {
	if ( !con.searchActive )
		return;

	if ( ch == '\b' || ch == 127 ) {
		// backspace
		if ( con.searchCursor > 0 ) {
			con.searchCursor--;
			con.searchPattern[con.searchCursor] = '\0';
			Con_SearchUpdate();
		} else {
			Con_SearchClose();
		}
		return;
	}

	if ( ch < 32 )
		return;

	if ( con.searchCursor < (int)sizeof( con.searchPattern ) - 1 ) {
		con.searchPattern[con.searchCursor] = ch;
		con.searchCursor++;
		con.searchPattern[con.searchCursor] = '\0';
		Con_SearchUpdate();
	}
}


/*
================
Con_IsSearchActive
================
*/
qboolean Con_IsSearchActive( void ) {
	return con.searchActive;
}


/*
================
Con_SearchLine

Returns the currently highlighted search line, or -1.
================
*/
int Con_SearchLine( void ) {
	return con.searchActive ? con.searchLine : -1;
}


/*
==============================================================================

CONSOLE MARK MODE (Ctrl+M text selection)

Provides a keyboard-only text selection interface for the console buffer:
  Ctrl+M        Toggle mark mode.  Initial selection anchors at the current
                bottom-most visible line.
  Arrows        Move the "end" marker (with Shift) or move and reset the
                anchor (without Shift).
  Home / End    Jump to line start / end of the current row.
  PgUp / PgDn   Move the end marker by one page.
  Ctrl+C / Enter  Copy selection to the system clipboard with color codes
                stripped, then exit mark mode.
  Esc           Exit mark mode without copying.

The selection rectangle is drawn with a distinct background color in
Con_DrawSolidConsole.  Lines are stored as absolute indices into the
ring buffer, matching con.current / con.display semantics.

==============================================================================
*/

/*
================
Con_MarkValidLine

Clamp an absolute line index to the portion of the scrollback buffer that
still has live data available.  Returns the (possibly clamped) index.
================
*/
static int Con_MarkValidLine( int line ) {
	int firstValid;

	firstValid = con.current - (con.totallines - 1);
	if ( firstValid < 0 ) {
		firstValid = 0;
	}

	if ( line < firstValid ) {
		line = firstValid;
	}
	if ( line > con.current ) {
		line = con.current;
	}
	return line;
}


/*
================
Con_MarkClampCol

Clamp a column index to [0, linewidth).
================
*/
static int Con_MarkClampCol( int col ) {
	if ( col < 0 ) {
		col = 0;
	}
	if ( col >= con.linewidth ) {
		col = con.linewidth - 1;
	}
	return col;
}


/*
================
Con_MarkEnsureVisible

Scroll the console display so that the current end-of-selection row is
within the visible region.
================
*/
static void Con_MarkEnsureVisible( void ) {
	int visibleRows = con.vispage;
	if ( visibleRows <= 0 ) {
		visibleRows = 1;
	}

	if ( con.markEndLine > con.display ) {
		con.display = con.markEndLine;
	} else if ( con.markEndLine < con.display - visibleRows + 1 ) {
		con.display = con.markEndLine + visibleRows - 1;
	}

	Con_Fixup();
}


/*
================
Con_MarkOpen

Activate mark mode.  The anchor and cursor both start at the bottom-most
line currently on screen, column 0.
================
*/
void Con_MarkOpen( void ) {
	con.markActive = qtrue;
	con.markStartLine = con.display;
	con.markStartCol = 0;
	con.markEndLine = con.display;
	con.markEndCol = 0;
}


/*
================
Con_MarkClose
================
*/
void Con_MarkClose( void ) {
	con.markActive = qfalse;
}


/*
================
Con_IsMarkActive
================
*/
qboolean Con_IsMarkActive( void ) {
	return con.markActive;
}


/*
================
Con_MarkGetRange

Returns the normalized selection rectangle:
  line1,col1 = top-left (earliest)
  line2,col2 = bottom-right (latest)
================
*/
static void Con_MarkGetRange( int *line1, int *col1, int *line2, int *col2 ) {
	int sl = con.markStartLine;
	int sc = con.markStartCol;
	int el = con.markEndLine;
	int ec = con.markEndCol;

	if ( sl < el || ( sl == el && sc <= ec ) ) {
		*line1 = sl; *col1 = sc;
		*line2 = el; *col2 = ec;
	} else {
		*line1 = el; *col1 = ec;
		*line2 = sl; *col2 = sc;
	}
}


/*
================
Con_MarkRowIsSelected

Returns qtrue if (row, col) falls inside the normalized selection.
================
*/
qboolean Con_MarkCellIsSelected( int row, int col ) {
	if ( !con.markActive ) {
		return qfalse;
	}

	int l1, c1, l2, c2;
	Con_MarkGetRange( &l1, &c1, &l2, &c2 );

	if ( row < l1 || row > l2 ) {
		return qfalse;
	}

	if ( l1 == l2 ) {
		return (col >= c1 && col <= c2) ? qtrue : qfalse;
	}

	if ( row == l1 ) {
		return (col >= c1) ? qtrue : qfalse;
	}
	if ( row == l2 ) {
		return (col <= c2) ? qtrue : qfalse;
	}
	return qtrue;
}


/*
================
Con_MarkCopySelection

Extract the currently selected text (stripping any embedded color bytes,
which live in the high byte of each short in con.text) and push it to the
system clipboard.
================
*/
static void Con_MarkCopySelection( void ) {
	if ( !con.markActive ) {
		return;
	}

	int l1, c1, l2, c2;
	char buf[CON_TEXTSIZE + 4];
	int bufPos = 0;
	Con_MarkGetRange( &l1, &c1, &l2, &c2 );

	for ( int line = l1; line <= l2; line++ ) {
		/* validate line is inside the live scrollback */
		if ( con.current - line >= con.totallines ) {
			continue;
		}

		short *text = con.text + (line % con.totallines) * con.linewidth;

		int startCol = (line == l1) ? c1 : 0;
		int endCol   = (line == l2) ? c2 : (con.linewidth - 1);

		for ( int col = startCol; col <= endCol; col++ ) {
			char ch = (char)(text[col] & 0xff);
			/* color codes are stored in the high byte, not inline — no need
			   to strip ^x sequences here, but leave the guard in place so
			   paste targets never see unrenderable bytes. */
			if ( ch == '\0' ) {
				ch = ' ';
			}
			if ( bufPos < (int)sizeof( buf ) - 1 ) {
				buf[bufPos++] = ch;
			}
		}

		if ( line != l2 && bufPos < (int)sizeof( buf ) - 2 ) {
			buf[bufPos++] = '\r';
			buf[bufPos++] = '\n';
		}
	}

	buf[bufPos] = '\0';

	Sys_SetClipboardData( buf );
}


/*
================
Con_MarkKey

Handles all keystrokes when the console is in mark mode.  Returns qtrue
if the key was consumed by the mark system.
================
*/
qboolean Con_MarkKey( int key, qboolean ctrlDown, qboolean shiftDown ) {
	/* Ctrl+M toggles — handled even if not currently active */
	if ( ctrlDown && (key == 'm' || key == 'M') ) {
		if ( con.markActive ) {
			Con_MarkClose();
		} else {
			Con_MarkOpen();
		}
		return qtrue;
	}

	if ( !con.markActive ) {
		return qfalse;
	}

	/* ESC exits without copying */
	if ( key == K_ESCAPE ) {
		Con_MarkClose();
		return qtrue;
	}

	/* Ctrl+C or Enter copies and exits */
	if ( (ctrlDown && (key == 'c' || key == 'C')) ||
	     key == K_ENTER || key == K_KP_ENTER ) {
		Con_MarkCopySelection();
		Con_MarkClose();
		return qtrue;
	}

	switch ( key ) {
	case K_LEFTARROW:
	case K_KP_LEFTARROW:
		if ( con.markEndCol > 0 ) {
			con.markEndCol--;
		} else if ( con.markEndLine > Con_MarkValidLine( 0 ) ) {
			con.markEndLine--;
			con.markEndCol = con.linewidth - 1;
		}
		if ( !shiftDown ) {
			con.markStartLine = con.markEndLine;
			con.markStartCol = con.markEndCol;
		}
		Con_MarkEnsureVisible();
		return qtrue;

	case K_RIGHTARROW:
	case K_KP_RIGHTARROW:
		if ( con.markEndCol < con.linewidth - 1 ) {
			con.markEndCol++;
		} else if ( con.markEndLine < con.current ) {
			con.markEndLine++;
			con.markEndCol = 0;
		}
		if ( !shiftDown ) {
			con.markStartLine = con.markEndLine;
			con.markStartCol = con.markEndCol;
		}
		Con_MarkEnsureVisible();
		return qtrue;

	case K_UPARROW:
	case K_KP_UPARROW:
		con.markEndLine = Con_MarkValidLine( con.markEndLine - 1 );
		con.markEndCol = Con_MarkClampCol( con.markEndCol );
		if ( !shiftDown ) {
			con.markStartLine = con.markEndLine;
			con.markStartCol = con.markEndCol;
		}
		Con_MarkEnsureVisible();
		return qtrue;

	case K_DOWNARROW:
	case K_KP_DOWNARROW:
		con.markEndLine = Con_MarkValidLine( con.markEndLine + 1 );
		con.markEndCol = Con_MarkClampCol( con.markEndCol );
		if ( !shiftDown ) {
			con.markStartLine = con.markEndLine;
			con.markStartCol = con.markEndCol;
		}
		Con_MarkEnsureVisible();
		return qtrue;

	case K_HOME:
	case K_KP_HOME:
		con.markEndCol = 0;
		if ( ctrlDown ) {
			con.markEndLine = Con_MarkValidLine( 0 );
		}
		if ( !shiftDown ) {
			con.markStartLine = con.markEndLine;
			con.markStartCol = con.markEndCol;
		}
		Con_MarkEnsureVisible();
		return qtrue;

	case K_END:
	case K_KP_END:
		con.markEndCol = con.linewidth - 1;
		if ( ctrlDown ) {
			con.markEndLine = con.current;
		}
		if ( !shiftDown ) {
			con.markStartLine = con.markEndLine;
			con.markStartCol = con.markEndCol;
		}
		Con_MarkEnsureVisible();
		return qtrue;

	case K_PGUP:
	case K_KP_PGUP:
		{
			int step = con.vispage > 0 ? con.vispage : 1;
			con.markEndLine = Con_MarkValidLine( con.markEndLine - step );
			con.markEndCol = Con_MarkClampCol( con.markEndCol );
			if ( !shiftDown ) {
				con.markStartLine = con.markEndLine;
				con.markStartCol = con.markEndCol;
			}
			Con_MarkEnsureVisible();
			return qtrue;
		}

	case K_PGDN:
	case K_KP_PGDN:
		{
			int step = con.vispage > 0 ? con.vispage : 1;
			con.markEndLine = Con_MarkValidLine( con.markEndLine + step );
			con.markEndCol = Con_MarkClampCol( con.markEndCol );
			if ( !shiftDown ) {
				con.markStartLine = con.markEndLine;
				con.markStartCol = con.markEndCol;
			}
			Con_MarkEnsureVisible();
			return qtrue;
		}
	}

	/* Block everything else while in mark mode */
	return qtrue;
}
