// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//

// this file holds commands that can be executed by the server console, but not remote clients

#include "g_local.h"
#include "g_entity_metadata.h"
#include "wired/bots/g_wiredintel.h"
LOG_DECLARE_CHANNEL( ch_game, "game" );


/*
==============================================================================

PACKET FILTERING


You can add or remove addresses from the filter list with:

addip <ip>
removeip <ip>

The ip address is specified in dot format, and you can use '*' to match any value
so you can specify an entire class C network with "addip 192.246.40.*"

Removeip will only remove an address specified exactly the same way.  You cannot addip a subnet, then removeip a single host.

listip
Prints the current list of filters.

g_filterban <0 or 1>

If 1 (the default), then ip addresses matching the current list will be prohibited from entering the game.  This is the default setting.

If 0, then only addresses matching the list will be allowed.  This lets you easily set up a private game, or a game that only allows players from your local network.

TTimo NOTE: for persistence, bans are stored in g_banIPs cvar MAX_CVAR_VALUE_STRING
The size of the cvar string buffer is limiting the banning to around 20 masks
this could be improved by putting some g_banIPs2 g_banIps3 etc. maybe
still, you should rely on PB for banning instead

==============================================================================
*/

typedef struct ipFilter_s
{
	unsigned	mask;
	unsigned	compare;
} ipFilter_t;

#define	MAX_IPFILTERS	1024

static ipFilter_t	ipFilters[MAX_IPFILTERS];
static int			numIPFilters;

/*
=================
StringToFilter
=================
*/
static qboolean StringToFilter (char *s, ipFilter_t *f)
{
	char	num[128];
	int		i, j;
	byte	b[4];
	byte	m[4];

	for (i=0 ; i<4 ; i++)
	{
		b[i] = 0;
		m[i] = 0;
	}

	for (i=0 ; i<4 ; i++)
	{
		if (*s < '0' || *s > '9')
		{
			if (*s == '*') // 'match any'
			{
				// b[i] and m[i] to 0
				s++;
				if (!*s)
					break;
				s++;
				continue;
			}
			Com_Log( SEV_INFO, LOG_CH(ch_game), "Bad filter address: %s\n", s );
			return qfalse;
		}

		j = 0;
		while (*s >= '0' && *s <= '9')
		{
			num[j++] = *s++;
		}
		num[j] = 0;
		b[i] = atoi(num);
		m[i] = 255;

		if (!*s)
			break;
		s++;
	}

	f->mask = *(unsigned *)m;
	f->compare = *(unsigned *)b;

	return qtrue;
}

/*
=================
UpdateIPBans
=================
*/
static void UpdateIPBans (void)
{
	byte	b[4] = {0};
	byte	m[4] = {0};
	int		i,j;
	QS_LOCAL(iplist_final, MAX_CVAR_VALUE_STRING);
	QS_LOCAL(ip, 64);
	for (i = 0 ; i < numIPFilters ; i++)
	{
		if (ipFilters[i].compare == 0xffffffff)
			continue;

		*(unsigned *)b = ipFilters[i].compare;
		*(unsigned *)m = ipFilters[i].mask;
		QS_Clear(&ip);
		for (j = 0 ; j < 4 ; j++)
		{
			if (m[j]!=255)
				QS_Append(&ip, "*");
			else
				QS_Appendf(&ip, "%i", b[j]);
			QS_Append(&ip, (j<3) ? "." : " ");
		}
		if (QS_Remaining(&iplist_final) >= QS_Len(&ip))
		{
			QS_Append(&iplist_final, QS_CStr(&ip));
		}
		else
		{
			Com_Log( SEV_INFO, LOG_CH(ch_game), "g_banIPs overflowed at MAX_CVAR_VALUE_STRING\n");
			break;
		}
	}

	trap_Cvar_Set( "g_banIPs", QS_CStr(&iplist_final) );
}

/*
=================
G_FilterPacket
=================
*/
qboolean G_FilterPacket (const char *from)
{
	int		i;
	unsigned	in;
	byte m[4] = {0};
	const char *p;

	i = 0;
	p = from;
	while (*p && i < 4) {
		m[i] = 0;
		while (*p >= '0' && *p <= '9') {
			m[i] = m[i]*10 + (*p - '0');
			p++;
		}
		if (!*p || *p == ':')
			break;
		i++, p++;
	}

	in = *(unsigned *)m;

	for (i=0 ; i<numIPFilters ; i++)
		if ( (in & ipFilters[i].mask) == ipFilters[i].compare)
			return g_filterBan.integer != 0;

	return g_filterBan.integer == 0;
}

/*
=================
AddIP
=================
*/
static void AddIP( char *str )
{
	int		i;

	for (i = 0 ; i < numIPFilters ; i++)
		if (ipFilters[i].compare == 0xffffffff)
			break;		// free spot
	if (i == numIPFilters)
	{
		if (numIPFilters == MAX_IPFILTERS)
		{
			Com_Log( SEV_INFO, LOG_CH(ch_game), "IP filter list is full\n");
			return;
		}
		numIPFilters++;
	}

	if (!StringToFilter (str, &ipFilters[i]))
		ipFilters[i].compare = 0xffffffffu;

	UpdateIPBans();
}

/*
=================
G_ProcessIPBans
=================
*/
void G_ProcessIPBans(void)
{
	char *s, *t;
	char		str[MAX_CVAR_VALUE_STRING];

	Q_strncpyz( str, g_banIPs.string, sizeof(str) );

	for (t = s = g_banIPs.string; *t; /* */ ) {
		s = strchr(s, ' ');
		if (!s)
			break;
		while (*s == ' ')
			*s++ = 0;
		if (*t)
			AddIP( t );
		t = s;
	}
}


/*
=================
Svcmd_AddIP_f
=================
*/
void Svcmd_AddIP_f (void)
{
	char		str[MAX_TOKEN_CHARS];

	if ( trap_Argc() < 2 ) {
		Com_Log( SEV_INFO, LOG_CH(ch_game), "Usage: addip <ip-mask>\n");
		return;
	}

	trap_Argv( 1, str, sizeof( str ) );

	AddIP( str );

}

/*
=================
Svcmd_RemoveIP_f
=================
*/
void Svcmd_RemoveIP_f (void)
{
	ipFilter_t	f;
	int			i;
	char		str[MAX_TOKEN_CHARS];

	if ( trap_Argc() < 2 ) {
		Com_Log( SEV_INFO, LOG_CH(ch_game), "Usage: removeip <ip-mask>\n");
		return;
	}

	trap_Argv( 1, str, sizeof( str ) );

	if (!StringToFilter (str, &f))
		return;

	for (i=0 ; i<numIPFilters ; i++) {
		if (ipFilters[i].mask == f.mask	&&
			ipFilters[i].compare == f.compare) {
			ipFilters[i].compare = 0xffffffffu;
			Com_Log( SEV_INFO, LOG_CH(ch_game), "Removed.\n");

			UpdateIPBans();
			return;
		}
	}

	Com_Log( SEV_INFO, LOG_CH(ch_game), "Didn't find %s.\n", str );
}

/*
===================
Svcmd_EntityList_f
===================
*/
void	Svcmd_EntityList_f (void) {
	int			e;
	gentity_t		*check;

	check = g_entities;
	for (e = 0; e < level.num_entities ; e++, check++) {
		if ( !check->inuse ) {
			continue;
		}
		Com_Log( SEV_INFO, LOG_CH(ch_game), "%3i:", e);
		switch ( check->s.eType ) {
		case ET_GENERAL:
			Com_Log( SEV_INFO, LOG_CH(ch_game), "ET_GENERAL          ");
			break;
		case ET_PLAYER:
			Com_Log( SEV_INFO, LOG_CH(ch_game), "ET_PLAYER           ");
			break;
		case ET_ITEM:
			Com_Log( SEV_INFO, LOG_CH(ch_game), "ET_ITEM             ");
			break;
		case ET_MISSILE:
			Com_Log( SEV_INFO, LOG_CH(ch_game), "ET_MISSILE          ");
			break;
		case ET_MOVER:
			Com_Log( SEV_INFO, LOG_CH(ch_game), "ET_MOVER            ");
			break;
		case ET_BEAM:
			Com_Log( SEV_INFO, LOG_CH(ch_game), "ET_BEAM             ");
			break;
		case ET_PORTAL:
			Com_Log( SEV_INFO, LOG_CH(ch_game), "ET_PORTAL           ");
			break;
		case ET_SPEAKER:
			Com_Log( SEV_INFO, LOG_CH(ch_game), "ET_SPEAKER          ");
			break;
		case ET_PUSH_TRIGGER:
			Com_Log( SEV_INFO, LOG_CH(ch_game), "ET_PUSH_TRIGGER     ");
			break;
		case ET_TELEPORT_TRIGGER:
			Com_Log( SEV_INFO, LOG_CH(ch_game), "ET_TELEPORT_TRIGGER ");
			break;
		case ET_INVISIBLE:
			Com_Log( SEV_INFO, LOG_CH(ch_game), "ET_INVISIBLE        ");
			break;
		case ET_GRAPPLE:
			Com_Log( SEV_INFO, LOG_CH(ch_game), "ET_GRAPPLE          ");
			break;
		default:
			Com_Log( SEV_INFO, LOG_CH(ch_game), "%3i                 ", check->s.eType);
			break;
		}

		if ( check->classname ) {
			Com_Log( SEV_INFO, LOG_CH(ch_game), "%s", check->classname);
		}
		/* Pure diagnostic, appended to the same line: health is what separates a
		   shoot-to-activate entity from a touch-activated one (a Q1 func_button
		   with health > 0 has no touch handler and must be shot), and the target
		   linkage says what it actuates. Read-only fields on an already-walked
		   entity — nothing here is on any decision path. */
		Com_Log( SEV_INFO, LOG_CH(ch_game), " health=%d", check->health );
		if ( check->targetname ) {
			Com_Log( SEV_INFO, LOG_CH(ch_game), " targetname=%s", check->targetname );
		}
		if ( check->target ) {
			Com_Log( SEV_INFO, LOG_CH(ch_game), " target=%s", check->target );
		}
		Com_Log( SEV_INFO, LOG_CH(ch_game), "\n");
	}
}

gclient_t	*ClientForString( const char *s ) {
	gclient_t	*cl;
	int			i;
	int			idnum;

	// numeric values are just slot numbers
	if ( s[0] >= '0' && s[0] <= '9' ) {
		idnum = atoi( s );
		if ( idnum < 0 || idnum >= level.maxclients ) {
			Com_Log( SEV_INFO, LOG_CH(ch_game), "Bad client slot: %i\n", idnum );
			return NULL;
		}

		cl = &level.clients[idnum];
		if ( cl->pers.connected == CON_DISCONNECTED ) {
			Com_Log( SEV_INFO, LOG_CH(ch_game), "Client %i is not connected\n", idnum );
			return NULL;
		}
		return cl;
	}

	// check for a name match
	for ( i=0 ; i < level.maxclients ; i++ ) {
		cl = &level.clients[i];
		if ( cl->pers.connected == CON_DISCONNECTED ) {
			continue;
		}
		if ( !Q_stricmp( cl->pers.netname, s ) ) {
			return cl;
		}
	}

	Com_Log( SEV_INFO, LOG_CH(ch_game), "User %s is not on the server\n", s );

	return NULL;
}

/*
===================
Svcmd_ForceTeam_f

forceteam <player> <team>
===================
*/
void	Svcmd_ForceTeam_f( void ) {
	gclient_t	*cl;
	char		str[MAX_TOKEN_CHARS];

	if ( trap_Argc() < 3 ) {
		Com_Log( SEV_INFO, LOG_CH(ch_game), "Usage: forceteam <player> <team>\n");
		return;
	}

	// find the player
	trap_Argv( 1, str, sizeof( str ) );
	cl = ClientForString( str );
	if ( !cl ) {
		return;
	}

	// set the team
	trap_Argv( 2, str, sizeof( str ) );
	SetTeam( &g_entities[cl - level.clients], str );
}

char	*ConcatArgs( int start );

/*
=================
ConsoleCommand

=================
*/


static void Svcmd_Q3nowEngine_f( void ) {
    char buf[256];
    trap_Cvar_VariableStringBuffer( "version", buf, sizeof(buf) );
    Com_Log( SEV_INFO, LOG_CH(ch_game), "Engine:      %s\n", buf );
    trap_Cvar_VariableStringBuffer( "cl_renderer", buf, sizeof(buf) );
    Com_Log( SEV_INFO, LOG_CH(ch_game), "Renderer:    %s\n", buf );
    trap_Cvar_VariableStringBuffer( "com_maxfps", buf, sizeof(buf) );
    Com_Log( SEV_INFO, LOG_CH(ch_game), "com_maxfps:  %s\n", buf );
}

// Called by the MCP bot_say tool via Cbuf_ExecuteText("bot_say_console ...").
// Passes the message to WiredIntel with senderClient = -1 (console authority),
// which bypasses BotAuthorizeOrder team checks and targets all bots.
static void Svcmd_BotSayConsole_f( void ) {
	const char     *msg;
	wbParseResult_t result;

	if ( trap_Argc() < 2 ) {
		Com_Log( SEV_INFO, LOG_CH(ch_game), "Usage: bot_say_console <message>\n" );
		return;
	}

	msg = ConcatArgs( 1 );
	if ( !msg || !msg[0] ) return;

	/* Default ACK — overwritten by BotReceiveDirective if a bot accepts/rejects */
	trap_Cvar_Set( "wiredbot_ack", "no @mention matched any active bot" );

	WiredIntel_ProcessChat( -1, msg, &result );

	/* If an @mention was parsed but no bot client matched the name, say so */
	if ( result.hasMentions && result.numRecipients == 0 ) {
		trap_Cvar_Set( "wiredbot_ack",
		               va( "no active bot named '%s'", result.recipientMention ) );
	}
}

static void Svcmd_Lightstyle_f( void ) {
	char slotStr[8];
	char pattern[LIGHTSTYLE_PATTERN_MAX + 1];
	int  style;

	if ( trap_Argc() < 3 ) {
		Com_Log( SEV_INFO, LOG_CH(ch_game),
		         "Usage: lightstyle <slot> <pattern>\n"
		         "  slot: 0-63; pattern: lowercase a-z string (empty string = off)\n" );
		return;
	}

	trap_Argv( 1, slotStr, sizeof( slotStr ) );
	trap_Argv( 2, pattern, sizeof( pattern ) );
	style = atoi( slotStr );

	if ( !G_SetLightstyle( style, pattern ) ) {
		Com_Log( SEV_INFO, LOG_CH(ch_game),
		         "lightstyle: invalid slot %d or pattern '%s'\n", style, pattern );
		return;
	}

	Com_Log( SEV_INFO, LOG_CH(ch_game), "lightstyle: style %d set to '%s'\n", style, pattern );
}

// Dev command: remap one shader to another at runtime (cheat-gated). Wires the
// existing AddRemap + BuildShaderStateConfig + CS_SHADERSTATE path — the same
// mechanism map entities use — to the console, so a material can be swapped on a
// live surface without a map edit (e.g. dropping a pbrMap test material onto a
// stock floor to exercise the base-pass IBL ambient). Session-scoped: the remap
// lives in the configstring until map reload.
static void Svcmd_RemapShader_f( void ) {
	char from[MAX_QPATH];
	char to[MAX_QPATH];
	char offStr[16];

	if ( !g_cheats.integer ) {
		Com_Log( SEV_INFO, LOG_CH(ch_game), "remapshader is cheat-protected (sv_cheats 1)\n" );
		return;
	}
	if ( trap_Argc() < 3 ) {
		Com_Log( SEV_INFO, LOG_CH(ch_game),
		         "Usage: remapshader <fromShader> <toShader> [timeOffset]\n" );
		return;
	}
	trap_Argv( 1, from, sizeof( from ) );
	trap_Argv( 2, to, sizeof( to ) );
	trap_Argv( 3, offStr, sizeof( offStr ) );

	AddRemap( from, to, atof( offStr ) );
	trap_SetConfigstring( CS_SHADERSTATE, BuildShaderStateConfig() );
	Com_Log( SEV_INFO, LOG_CH(ch_game), "remapshader: '%s' -> '%s'\n", from, to );
}

qboolean	ConsoleCommand( void ) {
	char	cmd[MAX_TOKEN_CHARS];

	trap_Argv( 0, cmd, sizeof( cmd ) );

	if ( Q_stricmp (cmd, "entitylist") == 0 ) {
		Svcmd_EntityList_f();
		return qtrue;
	}

	if ( Q_stricmp( cmd, "entityinspect" ) == 0 ) {
		char entityArg[MAX_TOKEN_CHARS];
		char fieldArg[MAX_TOKEN_CHARS];
		if ( trap_Argc() < 2 ) {
			Com_Log( SEV_INFO, LOG_CH(ch_game),
				"Usage: entityinspect <entitynum> [field]\n" );
			return qtrue;
		}
		trap_Argv( 1, entityArg, sizeof( entityArg ) );
		fieldArg[0] = '\0';
		if ( trap_Argc() >= 3 )
			trap_Argv( 2, fieldArg, sizeof( fieldArg ) );
		return G_EntityMetadataInspect( atoi( entityArg ), fieldArg );
	}

	

	if ( Q_stricmp (cmd, "forceteam") == 0 ) {
		Svcmd_ForceTeam_f();
		return qtrue;
	}

	if (Q_stricmp (cmd, "game_memory") == 0) {
		Svcmd_GameMem_f();
		return qtrue;
	}

	if ( Q_stricmp (cmd, "q3now_engine") == 0 ) {
		Svcmd_Q3nowEngine_f();
		return qtrue;
	}

	if ( Q_stricmp (cmd, "objective") == 0 ) {
		return G_Objectives_Command();
	}

	if ( Q_stricmp (cmd, "savegame") == 0 || Q_stricmp (cmd, "loadgame") == 0 ||
	     Q_stricmp (cmd, "savepersistent") == 0 || Q_stricmp (cmd, "loadpersistent") == 0 ) {
		return G_Save_Command( cmd );
	}

	if (Q_stricmp (cmd, "addbot") == 0) {
		Svcmd_AddBot_f();
		return qtrue;
	}

	if (Q_stricmp (cmd, "botlist") == 0) {
		Svcmd_BotList_f();
		return qtrue;
	}

	if (Q_stricmp (cmd, "addip") == 0) {
		Svcmd_AddIP_f();
		return qtrue;
	}

	if (Q_stricmp (cmd, "removeip") == 0) {
		Svcmd_RemoveIP_f();
		return qtrue;
	}

	if (Q_stricmp (cmd, "listip") == 0) {
		trap_SendConsoleCommand( EXEC_NOW, "g_banIPs\n" );
		return qtrue;
	}

	if ( Q_stricmp( cmd, "lightstyle" ) == 0 ) {
		Svcmd_Lightstyle_f();
		return qtrue;
	}

	if ( Q_stricmp( cmd, "remapshader" ) == 0 ) {
		Svcmd_RemapShader_f();
		return qtrue;
	}

	if ( Q_stricmp( cmd, "bot_order" ) == 0 ) {
		char botname[MAX_NETNAME];
		if ( trap_Argc() < 3 ) {
			Com_Log( SEV_INFO, LOG_CH(ch_game), "Usage: bot_order <botname> <order>\n" );
			return qtrue;
		}
		trap_Argv( 1, botname, sizeof( botname ) );
		BotDirective_ConsoleOrder( botname, ConcatArgs( 2 ) );
		return qtrue;
	}

	// Assign a client to a squad, or clear it. Same name lookup as autopilot and
	// bot_order, and like autopilot it scans CLIENTS rather than botstates[] —
	// a human must be assignable, and a human has no bot_state_t.
	//
	// This is the caller G_SameSquad was written for and never had. The rule
	// itself already handles the interesting case: its precedence says two
	// entities with the SAME non-zero squad are allies, and that this is the one
	// rule which overrides sess.sessionTeam. So assigning a companion and its
	// target the same squad makes them allied even in a free-for-all gametype,
	// where the legacy team test returns false for every pair and a bot would
	// otherwise designate the player it is escorting as an enemy.
	//
	// Squad 0 stays the no-squad sentinel and the default, so any client that is
	// never named by this command behaves exactly as before.
	if ( Q_stricmp( cmd, "squad" ) == 0 ) {
		char who[MAX_NETNAME];
		char arg[MAX_TOKEN_CHARS];
		char clean[MAX_NETNAME];
		int  squadNum;
		int  i;

		if ( trap_Argc() < 3 ) {
			Com_Log( SEV_INFO, LOG_CH(ch_game), "Usage: squad <playername> <number|none>\n" );
			return qtrue;
		}
		trap_Argv( 1, who, sizeof( who ) );
		trap_Argv( 2, arg, sizeof( arg ) );
		squadNum = ( Q_stricmp( arg, "none" ) == 0 ) ? 0 : atoi( arg );
		if ( squadNum < 0 ) {
			Com_Log( SEV_INFO, LOG_CH(ch_game), "squad: number must be >= 0 (0 or 'none' clears)\n" );
			return qtrue;
		}

		for ( i = 0; i < level.maxclients; i++ ) {
			if ( level.clients[i].pers.connected != CON_CONNECTED ) {
				continue;
			}
			Q_strncpyz( clean, level.clients[i].pers.netname, sizeof( clean ) );
			Q_CleanStr( clean );
			if ( Q_stricmpn( clean, who, strlen( who ) ) == 0 ) {
				g_entities[i].squad = squadNum;
				if ( squadNum ) {
					Com_Log( SEV_INFO, LOG_CH(ch_game), "squad: %s assigned to squad %d\n",
								level.clients[i].pers.netname, squadNum );
				} else {
					Com_Log( SEV_INFO, LOG_CH(ch_game), "squad: %s cleared (no squad)\n",
								level.clients[i].pers.netname );
				}
				return qtrue;
			}
		}
		Com_Log( SEV_INFO, LOG_CH(ch_game), "squad: no connected player named '%s'\n", who );
		return qtrue;
	}

	// Hand a client's entity to the bot brain, or take it back. Same name lookup
	// as bot_order (cleaned netname, case-insensitive prefix match), but it scans
	// CLIENTS rather than botstates[] because the whole point is to address a
	// human — a client that has no bot_state_t until autopilot gives it one.
	if ( Q_stricmp( cmd, "autopilot" ) == 0 ) {
		char     who[MAX_NETNAME];
		char     arg[MAX_TOKEN_CHARS];
		char     clean[MAX_NETNAME];
		qboolean enable;
		int      i;

		if ( trap_Argc() < 3 ) {
			Com_Log( SEV_INFO, LOG_CH(ch_game), "Usage: autopilot <playername> <on|off>\n" );
			return qtrue;
		}
		trap_Argv( 1, who, sizeof( who ) );
		trap_Argv( 2, arg, sizeof( arg ) );
		enable = ( Q_stricmp( arg, "on" ) == 0 || atoi( arg ) != 0 ) ? qtrue : qfalse;

		for ( i = 0; i < level.maxclients; i++ ) {
			if ( level.clients[i].pers.connected != CON_CONNECTED ) {
				continue;
			}
			Q_strncpyz( clean, level.clients[i].pers.netname, sizeof( clean ) );
			Q_CleanStr( clean );
			if ( Q_stricmpn( clean, who, strlen( who ) ) == 0 ) {
				if ( G_SetAutopilot( &g_entities[i], enable ) ) {
					Com_Log( SEV_INFO, LOG_CH(ch_game), "autopilot %s for %s\n",
								enable ? "ENABLED" : "disabled",
								level.clients[i].pers.netname );
				} else {
					Com_Log( SEV_INFO, LOG_CH(ch_game),
								"autopilot: no change for %s (already %s, or no brain available)\n",
								level.clients[i].pers.netname, enable ? "on" : "off" );
				}
				return qtrue;
			}
		}
		Com_Log( SEV_INFO, LOG_CH(ch_game), "autopilot: no connected player named '%s'\n", who );
		return qtrue;
	}

	// Deterministic placement of a named bot at a world origin. Cheat-gated —
	// used by the playthrough harness to start the bot on a known real,
	// nav-connected spawn instead of the seeded settle point (which can land on
	// a disconnected nav pocket). Not a shipped gameplay command.
	if ( Q_stricmp( cmd, "bot_teleport" ) == 0 ) {
		char botname[MAX_NETNAME];
		char arg[MAX_TOKEN_CHARS];
		vec3_t origin;
		float  yaw = 0.0f;
		if ( !g_cheats.integer ) {
			Com_Log( SEV_INFO, LOG_CH(ch_game), "bot_teleport is cheat-protected (sv_cheats 1)\n" );
			return qtrue;
		}
		if ( trap_Argc() < 5 ) {
			Com_Log( SEV_INFO, LOG_CH(ch_game), "Usage: bot_teleport <botname> <x> <y> <z> [yaw]\n" );
			return qtrue;
		}
		trap_Argv( 1, botname, sizeof( botname ) );
		trap_Argv( 2, arg, sizeof( arg ) ); origin[0] = atof( arg );
		trap_Argv( 3, arg, sizeof( arg ) ); origin[1] = atof( arg );
		trap_Argv( 4, arg, sizeof( arg ) ); origin[2] = atof( arg );
		if ( trap_Argc() >= 6 ) {
			trap_Argv( 5, arg, sizeof( arg ) ); yaw = atof( arg );
		}
		BotDirective_ConsoleTeleport( botname, origin, yaw );
		return qtrue;
	}

	if ( Q_stricmp( cmd, "bot_say_console" ) == 0 ) {
		Svcmd_BotSayConsole_f();
		return qtrue;
	}

	if ( G_ServerIsConsoleOnly() ) {
		if (Q_stricmp (cmd, "say") == 0) {
			trap_SendServerCommand( -1, va("print \"server: %s\n\"", ConcatArgs(1) ) );
			return qtrue;
		}
		// everything else will also be printed as a say command
		trap_SendServerCommand( -1, va("print \"server: %s\n\"", ConcatArgs(0) ) );
		return qtrue;
	}

	return qfalse;
}
