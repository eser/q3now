/*
===========================================================================
cg_chatfilter.c -- Chat ignore system (FEAT_CHAT_FILTER)

Allows players to mute chat from specific clients via /ignore and /unignore.
Inspired by OSP2's chat filter, adapted for q3now's API.
===========================================================================
*/
#include "cg_local.h"
/* Phase 5: log channels */
LOG_DECLARE_CHANNEL( ch_cgame, "cgame" );

#if FEAT_CHAT_FILTER

static qboolean mutedClients[MAX_CLIENTS];

/*
==================
CG_ChatFilterIgnore_f

Console command: /ignore <clientnum|name>
==================
*/
void CG_ChatFilterIgnore_f( void ) {
	char arg[MAX_TOKEN_CHARS];
	int clientNum;
	int i;

	if ( trap_Argc() < 2 ) {
		// no argument: list muted players
		Com_Log( SEV_INFO, LOG_CH(ch_cgame), "Muted players:\n" );
		for ( i = 0; i < MAX_CLIENTS; i++ ) {
			if ( mutedClients[i] && cgs.clientinfo[i].infoValid ) {
				Com_Log( SEV_INFO, LOG_CH(ch_cgame), "  %i: %s\n", i, cgs.clientinfo[i].name );
			}
		}
		return;
	}

	trap_Argv( 1, arg, sizeof( arg ) );

	// try as client number first
	clientNum = atoi( arg );
	if ( clientNum >= 0 && clientNum < MAX_CLIENTS && cgs.clientinfo[clientNum].infoValid ) {
		if ( arg[0] >= '0' && arg[0] <= '9' ) {
			mutedClients[clientNum] = qtrue;
			Com_Log( SEV_INFO, LOG_CH(ch_cgame), "Ignoring %s\n", CG_ClientName( &cgs.clientinfo[clientNum] ) );
			return;
		}
	}

	// try as player name (partial match)
	for ( i = 0; i < MAX_CLIENTS; i++ ) {
		if ( cgs.clientinfo[i].infoValid && Q_stristr( cgs.clientinfo[i].name, arg ) ) {
			mutedClients[i] = qtrue;
			Com_Log( SEV_INFO, LOG_CH(ch_cgame), "Ignoring %s\n", CG_ClientName( &cgs.clientinfo[i] ) );
			return;
		}
	}

	Com_Log( SEV_INFO, LOG_CH(ch_cgame), "Player '%s' not found.\n", arg );
}

/*
==================
CG_ChatFilterUnignore_f

Console command: /unignore <clientnum|name|all>
==================
*/
void CG_ChatFilterUnignore_f( void ) {
	char arg[MAX_TOKEN_CHARS];
	int clientNum;

	if ( trap_Argc() < 2 ) {
		Com_Log( SEV_INFO, LOG_CH(ch_cgame), "Usage: unignore <clientnum|name|all>\n" );
		return;
	}

	trap_Argv( 1, arg, sizeof( arg ) );

	// "all" clears the entire list
	if ( !Q_stricmp( arg, "all" ) ) {
		memset( mutedClients, 0, sizeof( mutedClients ) );
		Com_Log( SEV_INFO, LOG_CH(ch_cgame), "All players unmuted.\n" );
		return;
	}

	// try as client number
	clientNum = atoi( arg );
	if ( clientNum >= 0 && clientNum < MAX_CLIENTS && cgs.clientinfo[clientNum].infoValid ) {
		if ( arg[0] >= '0' && arg[0] <= '9' ) {
			mutedClients[clientNum] = qfalse;
			Com_Log( SEV_INFO, LOG_CH(ch_cgame), "Unignoring %s\n", CG_ClientName( &cgs.clientinfo[clientNum] ) );
			return;
		}
	}

	// try as player name
	for ( int i = 0; i < MAX_CLIENTS; i++ ) {
		if ( cgs.clientinfo[i].infoValid && Q_stristr( cgs.clientinfo[i].name, arg ) ) {
			mutedClients[i] = qfalse;
			Com_Log( SEV_INFO, LOG_CH(ch_cgame), "Unignoring %s\n", CG_ClientName( &cgs.clientinfo[i] ) );
			return;
		}
	}

	Com_Log( SEV_INFO, LOG_CH(ch_cgame), "Player '%s' not found.\n", arg );
}

/*
==================
CG_ChatFilterIsMuted

Returns qtrue if the given client number is muted.
==================
*/
qboolean CG_ChatFilterIsMuted( int clientNum ) {
	if ( clientNum < 0 || clientNum >= MAX_CLIENTS ) {
		return qfalse;
	}
	return mutedClients[clientNum];
}

#endif // FEAT_CHAT_FILTER
