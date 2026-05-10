/*
===========================================================================
g_stats.c -- JSON match stats export (7B / FEAT_JSON_STATS)

Writes a JSON file after each match with player stats.
Output: stats/<serverId>_<timestamp>.json

Guarded by FEAT_JSON_STATS — compiled out when disabled.
===========================================================================
*/
#include "g_local.h"
/* Phase 5: log channels */
LOG_DECLARE_CHANNEL( ch_game, "game" );

#if FEAT_JSON_STATS

// ── JSON helpers ────────────────────────────────────────────────────────

static void json_write( fileHandle_t f, const char *s ) {
	trap_FS_Write( s, strlen( s ), f );
}

static void json_writestring( fileHandle_t f, const char *key, const char *val ) {
	char buf[1024];
	char escaped[512];
	int i, j;

	// escape special chars
	for ( i = 0, j = 0; val[i] && j < (int)sizeof(escaped) - 2; i++ ) {
		if ( val[i] == '"' || val[i] == '\\' ) {
			escaped[j++] = '\\';
		}
		if ( val[i] >= 32 ) {
			escaped[j++] = val[i];
		}
	}
	escaped[j] = '\0';

	Com_sprintf( buf, sizeof(buf), "\"%s\": \"%s\"", key, escaped );
	json_write( f, buf );
}

static void json_writeint( fileHandle_t f, const char *key, int val ) {
	char buf[128];
	Com_sprintf( buf, sizeof(buf), "\"%s\": %i", key, val );
	json_write( f, buf );
}

// ── Main export ─────────────────────────────────────────────────────────

/*
==================
G_WriteStatsJSON

Called at end of match (ExitLevel). Writes match stats to a JSON file.
==================
*/
void G_WriteStatsJSON( void ) {
	fileHandle_t	f;
	char			filename[MAX_QPATH];
	char			serverId[64];
	qtime_t			now;
	int				i, playerCount;
	gclient_t		*cl;
	qboolean		first;

	if ( !g_exportStats.integer ) {
		return;
	}

	trap_Cvar_VariableStringBuffer( "g_exportStatsServerId", serverId, sizeof(serverId) );
	if ( !serverId[0] ) {
		Q_strncpyz( serverId, "q3now", sizeof(serverId) );
	}

	// count human players
	playerCount = 0;
	for ( i = 0; i < level.maxclients; i++ ) {
		cl = level.clients + i;
		if ( cl->pers.connected != CON_CONNECTED ) continue;
		if ( g_entities[i].r.svFlags & SVF_BOT ) continue;
		playerCount++;
	}
	if ( playerCount == 0 ) {
		return;
	}

	// generate timestamped filename
	trap_RealTime( &now );
	Com_sprintf( filename, sizeof(filename), "stats/%s_%04d-%02d-%02d-%02d%02d%02d.json",
		serverId,
		now.tm_year + 1900, now.tm_mon + 1, now.tm_mday,
		now.tm_hour, now.tm_min, now.tm_sec );

	trap_FS_FOpenFile( filename, &f, FS_WRITE );
	if ( !f ) {
		Com_Log( SEV_INFO, LOG_CH(ch_game), "G_WriteStatsJSON: could not open %s\n", filename );
		return;
	}

	// ── write JSON ──

	json_write( f, "{\n" );

	// match info
	json_write( f, "  " ); json_writestring( f, "server", serverId ); json_write( f, ",\n" );
	json_write( f, "  " ); json_writeint( f, "gametype", g_gametype.integer ); json_write( f, ",\n" );
	json_write( f, "  " ); json_writeint( f, "scorelimit", g_scorelimit.integer ); json_write( f, ",\n" );
	json_write( f, "  " ); json_writeint( f, "timelimit", g_timelimit.integer ); json_write( f, ",\n" );

	{
		char mapname[MAX_QPATH];
		trap_Cvar_VariableStringBuffer( "mapname", mapname, sizeof(mapname) );
		json_write( f, "  " ); json_writestring( f, "map", mapname ); json_write( f, ",\n" );
	}

	json_write( f, "  " ); json_writeint( f, "duration", level.time / 1000 ); json_write( f, ",\n" );

	if ( g_gametype.integer >= GT_TDM ) {
		json_write( f, "  " ); json_writeint( f, "score_red", level.teamScores[TEAM_RED] ); json_write( f, ",\n" );
		json_write( f, "  " ); json_writeint( f, "score_blue", level.teamScores[TEAM_BLUE] ); json_write( f, ",\n" );
	}

	// players array
	json_write( f, "  \"players\": [\n" );

	first = qtrue;
	for ( i = 0; i < level.maxclients; i++ ) {
		cl = level.clients + i;
		if ( cl->pers.connected != CON_CONNECTED ) continue;

		if ( !first ) json_write( f, ",\n" );
		first = qfalse;

		json_write( f, "    {\n" );
		json_write( f, "      " ); json_writestring( f, "name", cl->pers.netname ); json_write( f, ",\n" );
		json_write( f, "      " ); json_writeint( f, "score", cl->ps.persistant[PERS_SCORE] ); json_write( f, ",\n" );
		json_write( f, "      " ); json_writeint( f, "kills", cl->ps.persistant[PERS_SCORE] ); json_write( f, ",\n" );
		json_write( f, "      " ); json_writeint( f, "deaths", cl->ps.persistant[PERS_KILLED] ); json_write( f, ",\n" );
		json_write( f, "      " ); json_writeint( f, "damage_given", cl->ps.persistant[PERS_HITS] ); json_write( f, ",\n" );
		json_write( f, "      " ); json_writeint( f, "impressive", cl->ps.persistant[PERS_IMPRESSIVE_COUNT] ); json_write( f, ",\n" );
		json_write( f, "      " ); json_writeint( f, "excellent", cl->ps.persistant[PERS_EXCELLENT_COUNT] ); json_write( f, ",\n" );
		json_write( f, "      " ); json_writeint( f, "gauntlet", cl->ps.persistant[PERS_GAUNTLET_FRAG_COUNT] ); json_write( f, ",\n" );
		json_write( f, "      " ); json_writeint( f, "defends", cl->ps.persistant[PERS_DEFEND_COUNT] ); json_write( f, ",\n" );
		json_write( f, "      " ); json_writeint( f, "assists", cl->ps.persistant[PERS_ASSIST_COUNT] ); json_write( f, ",\n" );
		json_write( f, "      " ); json_writeint( f, "captures", cl->ps.persistant[PERS_CAPTURES] ); json_write( f, ",\n" );
		json_write( f, "      " ); json_writeint( f, "team", cl->sess.sessionTeam ); json_write( f, ",\n" );
		json_write( f, "      " ); json_writeint( f, "playtime", ( level.time - cl->pers.enterTime ) / 1000 ); json_write( f, ",\n" );
		json_write( f, "      " ); json_writeint( f, "is_bot", ( g_entities[i].r.svFlags & SVF_BOT ) ? 1 : 0 ); json_write( f, "\n" );
		json_write( f, "    }" );
	}

	json_write( f, "\n  ]\n" );
	json_write( f, "}\n" );

	trap_FS_FCloseFile( f );

	Com_Log( SEV_INFO, LOG_CH(ch_game), "Stats exported to %s (%i players)\n", filename, playerCount );
}

#endif // FEAT_JSON_STATS
