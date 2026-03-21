/*
===========================================================================
ui_script.c -- Menu script command system for q3_ui

Ported from ui/ui_shared.c's Script_* handlers.
All commands are fully implemented — no stubs.

Commands:
  show, hide, open, close, setcvar, exec, play, playlooped, stopmusic,
  fadein, fadeout, setcolor, setitemcolor, setfocus, transition
===========================================================================
*/
#include "ui_local.h"

typedef struct {
	const char *name;
	void (*handler)( menucommon_s *item, int numArgs, const char **args );
} scriptCommand_t;

/*
=================
UI_FindItemByName

Searches the item's parent menu for an item with the given name.
=================
*/
static menucommon_s *UI_FindItemByName( menucommon_s *item, const char *name )
{
	menuframework_s *menu;
	int i;

	if ( !item || !item->parent || !name ) return NULL;

	menu = item->parent;
	for ( i = 0; i < menu->nitems; i++ ) {
		menucommon_s *check = (menucommon_s *)menu->items[i];
		if ( check->name && !Q_stricmp( check->name, name ) ) {
			return check;
		}
	}
	return NULL;
}

/*
=================
UI_ParseColor4

Parses 4 float args (r g b a) starting at args[startIdx].
Returns qtrue if successful.
=================
*/
static qboolean UI_ParseColor4( int numArgs, const char **args, int startIdx, vec4_t out )
{
	if ( numArgs < startIdx + 4 ) return qfalse;
	out[0] = atof( args[startIdx] );
	out[1] = atof( args[startIdx + 1] );
	out[2] = atof( args[startIdx + 2] );
	out[3] = atof( args[startIdx + 3] );
	return qtrue;
}

/* ── Commands ───────────────────────────────────────────── */

static void Script_Show( menucommon_s *item, int numArgs, const char **args ) {
	menucommon_s *target;
	if ( numArgs < 1 ) return;
	target = UI_FindItemByName( item, args[0] );
	if ( target ) {
		target->flags &= ~QMF_HIDDEN;
		target->fadeAlpha = 1.0f;
	}
}

static void Script_Hide( menucommon_s *item, int numArgs, const char **args ) {
	menucommon_s *target;
	if ( numArgs < 1 ) return;
	target = UI_FindItemByName( item, args[0] );
	if ( target ) {
		target->flags |= QMF_HIDDEN;
		target->fadeAlpha = 0.0f;
	}
}

static void Script_Open( menucommon_s *item, int numArgs, const char **args ) {
	if ( numArgs < 1 ) return;
	UI_OpenMenuByName( args[0] );
}

static void Script_Close( menucommon_s *item, int numArgs, const char **args ) {
	UI_PopMenu();
}

static void Script_SetCvar( menucommon_s *item, int numArgs, const char **args ) {
	if ( numArgs >= 2 ) {
		trap_Cvar_Set( args[0], args[1] );
	}
}

static void Script_Exec( menucommon_s *item, int numArgs, const char **args ) {
	if ( numArgs >= 1 ) {
		trap_Cmd_ExecuteText( EXEC_APPEND, va( "%s\n", args[0] ) );
	}
}

static void Script_Play( menucommon_s *item, int numArgs, const char **args ) {
	if ( numArgs >= 1 ) {
		sfxHandle_t sfx = trap_S_RegisterSound( args[0], qfalse );
		if ( sfx ) {
			trap_S_StartLocalSound( sfx, CHAN_LOCAL_SOUND );
		}
	}
}

static void Script_PlayLooped( menucommon_s *item, int numArgs, const char **args ) {
	if ( numArgs >= 1 ) {
		trap_S_StartBackgroundTrack( args[0], args[0] );
	}
}

static void Script_StopMusic( menucommon_s *item, int numArgs, const char **args ) {
	trap_S_StopBackgroundTrack();
}

/*
=================
Script_FadeIn

Starts a fade-in animation on the target item.
Sets alpha to 0, clears QMF_HIDDEN, begins ramping up.
=================
*/
static void Script_FadeIn( menucommon_s *item, int numArgs, const char **args ) {
	menucommon_s *target;
	if ( numArgs < 1 ) return;
	target = UI_FindItemByName( item, args[0] );
	if ( !target ) return;

	target->flags &= ~QMF_HIDDEN;
	target->fadeAlpha = 0.0f;
	target->fadeFlags = FADEF_IN;
	target->fadeNextTime = uis.realtime;
}

/*
=================
Script_FadeOut

Starts a fade-out animation on the target item.
Begins ramping alpha down; when 0, sets QMF_HIDDEN.
=================
*/
static void Script_FadeOut( menucommon_s *item, int numArgs, const char **args ) {
	menucommon_s *target;
	if ( numArgs < 1 ) return;
	target = UI_FindItemByName( item, args[0] );
	if ( !target ) return;

	target->fadeFlags = FADEF_OUT;
	target->fadeNextTime = uis.realtime;
}

/*
=================
Script_SetColor

Sets a runtime color on the calling item.
Syntax: setcolor <forecolor|backcolor> <r> <g> <b> <a>
=================
*/
static void Script_SetColor( menucommon_s *item, int numArgs, const char **args ) {
	vec4_t color;
	if ( numArgs < 5 ) return;

	if ( !UI_ParseColor4( numArgs, args, 1, color ) ) return;

	if ( !Q_stricmp( args[0], "forecolor" ) ) {
		Vector4Copy( color, item->foreColor );
		item->colorFlags |= COLORF_FORE;
	} else if ( !Q_stricmp( args[0], "backcolor" ) ) {
		Vector4Copy( color, item->backColor );
		item->colorFlags |= COLORF_BACK;
	}
}

/*
=================
Script_SetItemColor

Sets a runtime color on a named item.
Syntax: setitemcolor <itemname> <forecolor|backcolor> <r> <g> <b> <a>
=================
*/
static void Script_SetItemColor( menucommon_s *item, int numArgs, const char **args ) {
	menucommon_s *target;
	vec4_t color;
	if ( numArgs < 6 ) return;

	target = UI_FindItemByName( item, args[0] );
	if ( !target ) return;

	if ( !UI_ParseColor4( numArgs, args, 2, color ) ) return;

	if ( !Q_stricmp( args[1], "forecolor" ) ) {
		Vector4Copy( color, target->foreColor );
		target->colorFlags |= COLORF_FORE;
	} else if ( !Q_stricmp( args[1], "backcolor" ) ) {
		Vector4Copy( color, target->backColor );
		target->colorFlags |= COLORF_BACK;
	}
}

static void Script_SetFocus( menucommon_s *item, int numArgs, const char **args ) {
	menucommon_s *target;
	if ( numArgs < 1 || !item->parent ) return;
	target = UI_FindItemByName( item, args[0] );
	if ( target ) {
		Menu_SetCursorToItem( item->parent, target );
	}
}

/*
=================
Script_Transition

Animates an item from one rect to another over a duration.
Syntax: transition <itemname> <fromX> <fromY> <fromW> <fromH> <toX> <toY> <toW> <toH> <durationMs> <steps>
=================
*/
static void Script_Transition( menucommon_s *item, int numArgs, const char **args ) {
	menucommon_s *target;
	if ( numArgs < 10 ) return;

	target = UI_FindItemByName( item, args[0] );
	if ( !target ) return;

	target->transFromRect[0] = atof( args[1] );
	target->transFromRect[1] = atof( args[2] );
	target->transFromRect[2] = atof( args[3] );
	target->transFromRect[3] = atof( args[4] );
	target->transToRect[0]   = atof( args[5] );
	target->transToRect[1]   = atof( args[6] );
	target->transToRect[2]   = atof( args[7] );
	target->transToRect[3]   = atof( args[8] );
	target->transDuration    = atoi( args[9] );
	target->transStartTime   = uis.realtime;
	target->transFlags       = TRANSF_ACTIVE;

	/* set initial position */
	target->x = (int)target->transFromRect[0];
	target->y = (int)target->transFromRect[1];
}

/* ── Command table ──────────────────────────────────────── */

static const scriptCommand_t scriptCommands[] = {
	{ "show",         Script_Show },
	{ "hide",         Script_Hide },
	{ "open",         Script_Open },
	{ "close",        Script_Close },
	{ "setcvar",      Script_SetCvar },
	{ "exec",         Script_Exec },
	{ "play",         Script_Play },
	{ "playlooped",   Script_PlayLooped },
	{ "stopmusic",    Script_StopMusic },
	{ "fadein",       Script_FadeIn },
	{ "fadeout",      Script_FadeOut },
	{ "setcolor",     Script_SetColor },
	{ "setitemcolor", Script_SetItemColor },
	{ "setfocus",     Script_SetFocus },
	{ "transition",   Script_Transition },
	{ NULL, NULL }
};

/*
=================
UI_RunScript

Parses and executes a semicolon-separated script string.
Example: "play sound/misc/menu2.wav ; fadein targetItem ; setcvar ui_state 1"
=================
*/
void UI_RunScript( menucommon_s *item, const char *script )
{
	char	token[MAX_STRING_CHARS];
	const char *args[8];
	int		numArgs;
	int		i;
	const char *p;

	if ( !script || !script[0] ) return;

	p = script;

	while ( *p ) {
		while ( *p == ' ' || *p == '\t' || *p == ';' ) p++;
		if ( !*p ) break;

		/* read command name */
		i = 0;
		while ( *p && *p != ' ' && *p != '\t' && *p != ';' && i < (int)sizeof(token) - 1 ) {
			token[i++] = *p++;
		}
		token[i] = '\0';
		if ( !token[0] ) break;

		/* read arguments (up to 8, stop at semicolon) */
		numArgs = 0;
		while ( numArgs < 8 ) {
			static char argBuf[8][256];
			while ( *p == ' ' || *p == '\t' ) p++;
			if ( !*p || *p == ';' ) break;

			i = 0;
			if ( *p == '"' ) {
				p++;
				while ( *p && *p != '"' && i < 255 ) argBuf[numArgs][i++] = *p++;
				if ( *p == '"' ) p++;
			} else {
				while ( *p && *p != ' ' && *p != '\t' && *p != ';' && i < 255 ) argBuf[numArgs][i++] = *p++;
			}
			argBuf[numArgs][i] = '\0';
			args[numArgs] = argBuf[numArgs];
			numArgs++;
		}

		/* dispatch */
		for ( i = 0; scriptCommands[i].name; i++ ) {
			if ( !Q_stricmp( token, scriptCommands[i].name ) ) {
				scriptCommands[i].handler( item, numArgs, args );
				break;
			}
		}
	}
}
