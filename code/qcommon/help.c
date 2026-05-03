/*
===========================================================================
help.c — CNQ3-style help system backport

Provides three user-facing commands:
  help <name>         Print the description, flags, range, and current value
                      of the named cvar or command.
  man  <name>         Alias for help.
  searchhelp <pat>    List every cvar/command whose name or description
                      contains the pattern (substring, case-insensitive).

The implementation relies on q3now's existing cvar->description field
(registered via Cvar_SetDescription).  It deliberately avoids the cnq3
"common_help.h"/"client_help.h" string-table machinery because q3now
already stores descriptions inline on the cvar struct.

This file also exposes `Help_LookupText` used by cl_console.c to render
a help panel below the input line when con_drawHelp is set.
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"

/* con_drawHelp bitmask flags — mirror cnq3 */
#define HELP_FLAG_ENABLE	1	/* draw at all */
#define HELP_FLAG_ALWAYS	2	/* draw even if the token has no help text */
#define HELP_FLAG_MODULES	4	/* show [cvar|cmd] tag */
#define HELP_FLAG_ATTRIBS	8	/* show archive/latch/rom flag letters */

static cvar_t *con_drawHelp;


/*
============
Help_FormatFlags

Produces a small human-readable flag string like "[archive, latch]".
Writes nothing when the cvar has no notable flags.
============
*/
static void Help_FormatFlags( const cvar_t *var, char *out, int outSize )
{
	int used = 0;

	out[0] = '\0';

	if ( var == NULL )
		return;

	if ( var->flags & CVAR_ARCHIVE ) {
		used += Com_sprintf( out + used, outSize - used, "%sarchive", used ? ", " : "" );
	}
	if ( var->flags & CVAR_LATCH ) {
		used += Com_sprintf( out + used, outSize - used, "%slatch", used ? ", " : "" );
	}
	if ( var->flags & CVAR_ROM ) {
		used += Com_sprintf( out + used, outSize - used, "%sreadonly", used ? ", " : "" );
	}
	if ( var->flags & CVAR_INIT ) {
		used += Com_sprintf( out + used, outSize - used, "%sinit", used ? ", " : "" );
	}
	if ( var->flags & CVAR_USERINFO ) {
		used += Com_sprintf( out + used, outSize - used, "%suserinfo", used ? ", " : "" );
	}
	if ( var->flags & CVAR_SERVERINFO ) {
		used += Com_sprintf( out + used, outSize - used, "%sserverinfo", used ? ", " : "" );
	}
	if ( var->flags & CVAR_SYSTEMINFO ) {
		used += Com_sprintf( out + used, outSize - used, "%ssysteminfo", used ? ", " : "" );
	}
	if ( var->flags & CVAR_CHEAT ) {
		used += Com_sprintf( out + used, outSize - used, "%scheat", used ? ", " : "" );
	}
}


static const char *Help_TypeName( cvarType_t type )
{
	switch ( type ) {
	case CVT_STRING: return "string";
	case CVT_BOOL:   return "bool";
	case CVT_INT:    return "integer";
	case CVT_FLOAT:  return "float";
	case CVT_ENUM:   return "enum";
	default:         return "unknown";
	}
}


/*
============
Help_PrintCvar

Print all the known information about a cvar to the console.
============
*/
static void Help_PrintCvar( const cvar_t *var )
{
	char flagStr[256];

	Com_Log( SEV_INFO, LOG_CAT_SYSTEM,
		S_COLOR_YELLOW "cvar " S_COLOR_WHITE "%s" S_COLOR_WHITE " (%s)\n",
		var->name, Help_TypeName( var->type ) );

	if ( var->description && var->description[0] ) {
		Com_Log( SEV_INFO, LOG_CAT_SYSTEM, "  %s\n", var->description );
	}

	Com_Log( SEV_INFO, LOG_CAT_SYSTEM, "  current: \"%s\"\n", var->string );
	if ( var->resetString && var->resetString[0] ) {
		Com_Log( SEV_INFO, LOG_CAT_SYSTEM, "  default: \"%s\"\n", var->resetString );
	}
	if ( var->latchedString && var->latchedString[0] ) {
		Com_Log( SEV_INFO, LOG_CAT_SYSTEM, "  latched: \"%s\"\n", var->latchedString );
	}

	// Type-specific info
	switch ( var->type ) {
	case CVT_BOOL:
		Com_Log( SEV_INFO, LOG_CAT_SYSTEM, "  values:  0/1 (also accepts true/false, yes/no, on/off)\n" );
		break;
	case CVT_INT:
		if ( var->typeMin != var->typeMax ) {
			Com_Log( SEV_INFO, LOG_CAT_SYSTEM, "  range:   %d — %d\n",
				(int)var->typeMin, (int)var->typeMax );
		} else {
			if ( var->mins ) Com_Log( SEV_INFO, LOG_CAT_SYSTEM, "  min:     %s\n", var->mins );
			if ( var->maxs ) Com_Log( SEV_INFO, LOG_CAT_SYSTEM, "  max:     %s\n", var->maxs );
		}
		break;
	case CVT_FLOAT:
		if ( var->typeMin != var->typeMax ) {
			Com_Log( SEV_INFO, LOG_CAT_SYSTEM, "  range:   %g — %g\n",
				var->typeMin, var->typeMax );
		} else {
			if ( var->mins ) Com_Log( SEV_INFO, LOG_CAT_SYSTEM, "  min:     %s\n", var->mins );
			if ( var->maxs ) Com_Log( SEV_INFO, LOG_CAT_SYSTEM, "  max:     %s\n", var->maxs );
		}
		break;
	case CVT_ENUM:
		if ( var->enumValues && var->enumCount > 0 ) {
			char evbuf[512];
			int ei, en = 0;
			evbuf[0] = '\0';
			for ( ei = 0; ei < var->enumCount; ei++ ) {
				int elen;
				if ( ei > 0 && en < (int)sizeof(evbuf) - 3 ) {
					evbuf[en++] = ',';
					evbuf[en++] = ' ';
				}
				elen = (int)strlen( var->enumValues[ei] );
				if ( en + elen >= (int)sizeof(evbuf) - 1 ) break;
				memcpy( evbuf + en, var->enumValues[ei], elen );
				en += elen;
			}
			evbuf[en] = '\0';
			Com_Log( SEV_INFO, LOG_CAT_SYSTEM, "  values:  %s\n", evbuf );
		}
		break;
	default:
		if ( var->mins ) Com_Log( SEV_INFO, LOG_CAT_SYSTEM, "  min:     %s\n", var->mins );
		if ( var->maxs ) Com_Log( SEV_INFO, LOG_CAT_SYSTEM, "  max:     %s\n", var->maxs );
		break;
	}

	Help_FormatFlags( var, flagStr, sizeof( flagStr ) );
	if ( flagStr[0] ) {
		Com_Log( SEV_INFO, LOG_CAT_SYSTEM, "  flags:   %s\n", flagStr );
	}
}


/*
============
Help_PrintCommand
============
*/
static void Help_PrintCommand( const char *name )
{
	Com_Log( SEV_INFO, LOG_CAT_SYSTEM, S_COLOR_YELLOW "command " S_COLOR_WHITE "%s\n", name );
	Com_Log( SEV_INFO, LOG_CAT_SYSTEM, "  (no description available)\n" );
}


/*
============
Cmd_Help_f

help <name>  /  man <name>
============
*/
static void Cmd_Help_f( void )
{
	const char *arg0 = Cmd_Argv( 0 );

	if ( Cmd_Argc() != 2 ) {
		Com_Log( SEV_INFO, LOG_CAT_SYSTEM, "usage: %s <cvar|cmd>\n", arg0 );
		return;
	}

	const char *name = Cmd_Argv( 1 );
	cvar_t *var = Cvar_FindVarPublic( name );
	if ( var != NULL ) {
		Help_PrintCvar( var );
		return;
	}

	if ( Cmd_Exists( name ) ) {
		Help_PrintCommand( name );
		return;
	}

	Com_Log( SEV_INFO, LOG_CAT_SYSTEM, "No cvar or command named '%s'.\n", name );
}


/*
============
Help_SearchCvarCallback
============
*/
typedef struct {
	const char	*pattern;
	int			matches;
} helpSearchCtx_t;

static void Help_SearchCvarCallback( cvar_t *var, void *userdata )
{
	helpSearchCtx_t *ctx = (helpSearchCtx_t *)userdata;
	qboolean match = qfalse;

	if ( Q_stristr( var->name, ctx->pattern ) ) {
		match = qtrue;
	} else if ( var->description && Q_stristr( var->description, ctx->pattern ) ) {
		match = qtrue;
	}

	if ( match ) {
		Com_Log( SEV_INFO, LOG_CAT_SYSTEM, "  " S_COLOR_YELLOW "cvar " S_COLOR_WHITE "%s" S_COLOR_WHITE, var->name );
		if ( var->description && var->description[0] ) {
			Com_Log( SEV_INFO, LOG_CAT_SYSTEM, " - %s", var->description );
		}
		Com_Log( SEV_INFO, LOG_CAT_SYSTEM, "\n" );
		ctx->matches++;
	}
}

static void Help_SearchCmdCallback( const char *cmd_name, void *userdata )
{
	helpSearchCtx_t *ctx = (helpSearchCtx_t *)userdata;

	if ( Q_stristr( cmd_name, ctx->pattern ) ) {
		Com_Log( SEV_INFO, LOG_CAT_SYSTEM, "  " S_COLOR_YELLOW "command " S_COLOR_WHITE "%s\n", cmd_name );
		ctx->matches++;
	}
}


/*
============
Cmd_SearchHelp_f
============
*/
static void Cmd_SearchHelp_f( void )
{
	helpSearchCtx_t ctx;

	if ( Cmd_Argc() != 2 ) {
		Com_Log( SEV_INFO, LOG_CAT_SYSTEM, "usage: searchhelp <pattern>\n" );
		return;
	}

	ctx.pattern = Cmd_Argv( 1 );
	ctx.matches = 0;

	Cvar_ForEach( Help_SearchCvarCallback, &ctx );
	Cmd_ForEachName( Help_SearchCmdCallback, &ctx );

	Com_Log( SEV_INFO, LOG_CAT_SYSTEM, "%i match%s for '%s'.\n",
		ctx.matches, ctx.matches == 1 ? "" : "es", ctx.pattern );
}


/*
============
Help_LookupText

Used by cl_console.c to render a help panel below the input line.
Fills `out` with a single-line summary for the given token and returns
qtrue if something was written.

Style is controlled by con_drawHelp's bit flags:
  bit 1 (HELP_FLAG_ENABLE)  required to render anything
  bit 2 (HELP_FLAG_ALWAYS)  render the token even if it has no help
  bit 4 (HELP_FLAG_MODULES) prepend the type tag ("cvar" / "cmd")
  bit 8 (HELP_FLAG_ATTRIBS) append the flag letters (A/L/R/...)
============
*/
qboolean Help_LookupText( const char *token, char *out, int outSize )
{
	if ( out == NULL || outSize <= 0 )
		return qfalse;
	out[0] = '\0';

	if ( token == NULL || token[0] == '\0' )
		return qfalse;

	if ( con_drawHelp == NULL )
		return qfalse;

	int flags = con_drawHelp->integer;
	if ( !( flags & HELP_FLAG_ENABLE ) )
		return qfalse;

	cvar_t *var = Cvar_FindVarPublic( token );
	if ( var != NULL ) {
		qboolean haveText = ( var->description && var->description[0] ) ? qtrue : qfalse;
		if ( !haveText && !( flags & HELP_FLAG_ALWAYS ) )
			return qfalse;

		char attribBuf[32];
		attribBuf[0] = '\0';
		if ( flags & HELP_FLAG_ATTRIBS ) {
			int ap = 0;
			if ( var->flags & CVAR_ARCHIVE )  attribBuf[ap++] = 'A';
			if ( var->flags & CVAR_LATCH )    attribBuf[ap++] = 'L';
			if ( var->flags & CVAR_ROM )      attribBuf[ap++] = 'R';
			if ( var->flags & CVAR_CHEAT )    attribBuf[ap++] = 'C';
			if ( var->flags & CVAR_USERINFO ) attribBuf[ap++] = 'U';
			attribBuf[ap] = '\0';
		}

		if ( flags & HELP_FLAG_MODULES ) {
			Com_sprintf( out, outSize, "[cvar%s%s] %s: %s",
				attribBuf[0] ? " " : "", attribBuf,
				var->name,
				haveText ? var->description : "(no help)" );
		} else {
			Com_sprintf( out, outSize, "%s%s%s: %s",
				var->name,
				attribBuf[0] ? " " : "",
				attribBuf,
				haveText ? var->description : "(no help)" );
		}
		return qtrue;
	}

	if ( Cmd_Exists( token ) ) {
		if ( flags & HELP_FLAG_MODULES ) {
			Com_sprintf( out, outSize, "[cmd] %s", token );
		} else {
			Com_sprintf( out, outSize, "%s (command)", token );
		}
		return qtrue;
	}

	return qfalse;
}


/*
============
Help_IsKnownCvar / Help_IsKnownCommand

Used by the console input renderer for syntax highlighting.
============
*/
qboolean Help_IsKnownCvar( const char *token )
{
	if ( token == NULL || token[0] == '\0' )
		return qfalse;
	return Cvar_FindVarPublic( token ) != NULL ? qtrue : qfalse;
}

qboolean Help_IsKnownCommand( const char *token )
{
	if ( token == NULL || token[0] == '\0' )
		return qfalse;
	return Cmd_Exists( token );
}


/*
============
Help_Init

Registers the help/man/searchhelp commands and the con_drawHelp cvar.
Called from Com_Init after Cmd_Init.
============
*/
void Help_Init( void )
{
	static const cvarDesc_t d = CVAR_INT( "con_drawHelp", "1", CVAR_ARCHIVE,
		"Console help panel bitmask:\n"
		"  1 = enable\n"
		"  2 = show even when no help text\n"
		"  4 = show module (cvar|cmd)\n"
		"  8 = show attribute letters (A/L/R/C/U)", 0, 15 );
	con_drawHelp = Cvar_Register( &d );

	Cmd_AddCommand( "help", Cmd_Help_f );
	Cmd_AddCommand( "man", Cmd_Help_f );
	Cmd_AddCommand( "searchhelp", Cmd_SearchHelp_f );
}
