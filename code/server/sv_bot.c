/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2024 Wired engine contributors

This file is part of the Wired Engine (derived from idTech 3 & 4 source
code and community around it). It is free software released under the terms
of the GNU General Public License version 2 or (at your option) any later
version.

Quake III Arena, q3now, Wired Engine and the rest are licensed under the
**GNU General Public License, version 2 or later (GPL-2.0-or-later)**.
The full license text is in `LICENSE` and `THIRD_PARTY_LICENSES.md` at the
repository root.
===========================================================================
*/
// sv_bot.c

#include "server.h"
#include "../botlib/botlib.h"
/* Phase 5: log channels */
LOG_DECLARE_CHANNEL( ch_botlib, "botlib" );
LOG_DECLARE_CHANNEL( ch_server, "server" );

typedef struct bot_debugpoly_s
{
	int inuse;
	int color;
	int numPoints;
	vec3_t points[128];
} bot_debugpoly_t;

static bot_debugpoly_t *debugpolygons;
static int bot_maxdebugpolys;

extern botlib_export_t	*botlib_export;
int	bot_enable;


/*
==================
SV_BotAllocateClient
==================
*/
int SV_BotAllocateClient( void ) {
	int			i;
	client_t	*cl;

	// find a client slot
	for ( i = 0, cl = svs.clients; i < sv.maxclients; i++, cl++ ) {
		if ( cl->state == CS_FREE ) {
			break;
		}
	}

	if ( i == sv.maxclients ) {
		return -1;
	}

	cl->gentity = SV_GentityNum( i );
	cl->gentity->s.number = i;
	cl->state = CS_ACTIVE;
	cl->lastPacketTime = svs.time;
	cl->snapshotMsec = 1000 / sv_fps->integer;
	cl->netchan.remoteAddress.type = NA_BOT;
	cl->rate = 0;

	cl->tld[0] = '\0';
	cl->country = "BOT";

	return i;
}


/*
==================
SV_BotFreeClient
==================
*/
void SV_BotFreeClient( int clientNum ) {
	client_t	*cl;

	if ( (unsigned) clientNum >= sv.maxclients ) {
		Com_Terminate( TERM_CLIENT_DROP, "SV_BotFreeClient: bad clientNum: %i", clientNum );
	}

	cl = &svs.clients[clientNum];
	cl->state = CS_FREE;
	cl->name[0] = '\0';
	if ( cl->gentity ) {
		cl->gentity->r.svFlags &= ~SVF_BOT;
	}
}


/*
==================
BotDrawDebugPolygons
==================
*/
void BotDrawDebugPolygons(void (*drawPoly)(int color, int numPoints, float *points), int value) {
	static cvar_t *bot_debug, *bot_groundonly, *bot_reachability, *bot_highlightarea;

	if (!debugpolygons)
		return;
	//bot debugging
	if (!bot_debug) bot_debug = Cvar_Get("bot_debug", "0", 0);
	//
	if (bot_enable && bot_debug->integer) {
		//show reachabilities
		if (!bot_reachability) bot_reachability = Cvar_Get("bot_reachability", "0", 0);
		//show ground faces only
		if (!bot_groundonly) bot_groundonly = Cvar_Get("bot_groundonly", "1", 0);
		//get the hightlight area
		if (!bot_highlightarea) bot_highlightarea = Cvar_Get("bot_highlightarea", "0", 0);
		//
		int parm0 = 0;
		if (svs.clients[0].lastUsercmd.buttons & BUTTON_ATTACK_PRI || svs.clients[0].lastUsercmd.buttons & BUTTON_ATTACK_SEC) parm0 |= 1;
		if (bot_reachability->integer) parm0 |= 2;
		if (bot_groundonly->integer) parm0 |= 4;
		botlib_export->BotLibVarSet("bot_highlightarea", bot_highlightarea->string);
		botlib_export->Test(parm0, NULL, svs.clients[0].gentity->r.currentOrigin,
			svs.clients[0].gentity->r.currentAngles);
	} //end if
	//draw all debug polys
	for (int i = 0; i < bot_maxdebugpolys; i++) {
		bot_debugpoly_t *poly = &debugpolygons[i];
		if (!poly->inuse) continue;
		drawPoly(poly->color, poly->numPoints, (float *) poly->points);
		//Com_Log( SEV_INFO, LOG_CH(ch_server), "poly %i, numpoints = %d\n", i, poly->numPoints);
	}
}

/*
==================
BotImport_Print
==================
*/
static __attribute__ ((format (printf, 2, 3))) void QDECL BotImport_Print(int type, const char *fmt, ...)
{
	char str[2048];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(str, sizeof(str), fmt, ap);
	va_end(ap);

	switch(type) {
		case PRT_MESSAGE: {
			Com_Log( SEV_DEBUG, LOG_CH(ch_botlib), "%s", str);
			break;
		}
		case PRT_WARNING: {
			Com_Log( SEV_WARN, LOG_CH(ch_botlib), S_COLOR_WARNING "Warning: %s", str);
			break;
		}
		case PRT_ERROR: {
			Com_Log( SEV_ERROR, LOG_CH(ch_botlib), S_COLOR_ERROR "Error: %s", str);
			break;
		}
		case PRT_FATAL: {
			Com_Log( SEV_FATAL, LOG_CH(ch_botlib), S_COLOR_ERROR "Fatal: %s", str);
			break;
		}
		case PRT_EXIT: {
			Com_Terminate( TERM_CLIENT_DROP, S_COLOR_ERROR "Exit: %s", str);
			break;
		}
		default: {
			Com_Log( SEV_DEBUG, LOG_CH(ch_botlib), "unknown print type\n");
			break;
		}
	}
}

/*
==================
BotImport_Trace
==================
*/
static void BotImport_Trace(bsp_trace_t *bsptrace, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int passent, int contentmask) {
	trace_t trace;

	SV_Trace(&trace, start, mins, maxs, end, passent, contentmask, qfalse);
	//copy the trace information
	bsptrace->allsolid = trace.allsolid;
	bsptrace->startsolid = trace.startsolid;
	bsptrace->fraction = trace.fraction;
	VectorCopy(trace.endpos, bsptrace->endpos);
	bsptrace->plane.dist = trace.plane.dist;
	VectorCopy(trace.plane.normal, bsptrace->plane.normal);
	bsptrace->plane.signbits = trace.plane.signbits;
	bsptrace->plane.type = trace.plane.type;
	bsptrace->surface.value = 0;
	bsptrace->surface.flags = trace.surfaceFlags;
	bsptrace->ent = trace.entityNum;
	bsptrace->exp_dist = 0;
	bsptrace->sidenum = 0;
	bsptrace->contents = 0;
}

/*
==================
BotImport_EntityTrace
==================
*/
static void BotImport_EntityTrace(bsp_trace_t *bsptrace, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int entnum, int contentmask) {
	trace_t trace;

	if ( (unsigned)entnum > MAX_GENTITIES - 1 ) {
		entnum = ENTITYNUM_NONE;
	}

	SV_ClipToEntity(&trace, start, mins, maxs, end, entnum, contentmask, qfalse);
	//copy the trace information
	bsptrace->allsolid = trace.allsolid;
	bsptrace->startsolid = trace.startsolid;
	bsptrace->fraction = trace.fraction;
	VectorCopy(trace.endpos, bsptrace->endpos);
	bsptrace->plane.dist = trace.plane.dist;
	VectorCopy(trace.plane.normal, bsptrace->plane.normal);
	bsptrace->plane.signbits = trace.plane.signbits;
	bsptrace->plane.type = trace.plane.type;
	bsptrace->surface.value = 0;
	bsptrace->surface.flags = trace.surfaceFlags;
	bsptrace->ent = trace.entityNum;
	bsptrace->exp_dist = 0;
	bsptrace->sidenum = 0;
	bsptrace->contents = 0;
}


/*
==================
BotImport_PointContents
==================
*/
static int BotImport_PointContents(vec3_t point) {
	return SV_PointContents(point, -1);
}

/*
==================
BotImport_inPVS
==================
*/
static int BotImport_inPVS(vec3_t p1, vec3_t p2) {
	return SV_inPVS (p1, p2);
}

/*
==================
BotImport_BSPEntityData
==================
*/
static char *BotImport_BSPEntityData(void) {
	return CM_EntityString();
}

/*
==================
BotImport_BSPModelMinsMaxsOrigin
==================
*/
static void BotImport_BSPModelMinsMaxsOrigin(int modelnum, vec3_t angles, vec3_t outmins, vec3_t outmaxs, vec3_t origin) {
	vec3_t mins, maxs;
	clipHandle_t h = CM_InlineModel(modelnum);
	CM_ModelBounds(h, mins, maxs);
	//if the model is rotated
	if ((angles[0] || angles[1] || angles[2])) {
		// expand for rotation
		float max = RadiusFromBounds(mins, maxs);
		for (int i = 0; i < 3; i++) {
			mins[i] = -max;
			maxs[i] = max;
		}
	}
	if (outmins) VectorCopy(mins, outmins);
	if (outmaxs) VectorCopy(maxs, outmaxs);
	if (origin) VectorClear(origin);
}

/*
==================
BotImport_GetMemory
==================
*/
static void *BotImport_GetMemory(size_t size) {
	void *ptr = Z_TagMalloc( size, TAG_BOTLIB );
	return ptr;
}

/*
==================
BotImport_FreeMemory
==================
*/
static void BotImport_FreeMemory(void *ptr) {
	Z_Free(ptr);
}

/*
=================
BotImport_HunkAlloc
=================
*/
static void *BotImport_HunkAlloc( size_t size ) {
	if( Hunk_CheckMark() ) {
		Com_Terminate( TERM_CLIENT_DROP, "%s(): Alloc with marks already set", __func__ );
	}
	return Hunk_Alloc( size, h_high );
}

/*
==================
BotImport_DebugPolygonCreate
==================
*/
int BotImport_DebugPolygonCreate(int color, int numPoints, vec3_t *points) {
	if (!debugpolygons)
		return 0;

	int i;
	for (i = 1; i < bot_maxdebugpolys; i++) 	{
		if (!debugpolygons[i].inuse)
			break;
	}
	if (i >= bot_maxdebugpolys)
		return 0;
	bot_debugpoly_t *poly = &debugpolygons[i];
	poly->inuse = qtrue;
	poly->color = color;
	poly->numPoints = numPoints;
	memcpy(poly->points, points, numPoints * sizeof(vec3_t));
	//
	return i;
}

/*
==================
BotImport_DebugPolygonShow
==================
*/
static void BotImport_DebugPolygonShow(int id, int color, int numPoints, vec3_t *points) {
	bot_debugpoly_t *poly;

	if ( !debugpolygons )
		return;

	if ( (unsigned) id >= bot_maxdebugpolys )
		return;

	poly = &debugpolygons[id];
	poly->inuse = qtrue;
	poly->color = color;
	poly->numPoints = numPoints;
	memcpy(poly->points, points, numPoints * sizeof(vec3_t));
}

/*
==================
BotImport_DebugPolygonDelete
==================
*/
void BotImport_DebugPolygonDelete(int id)
{
	if ( !debugpolygons )
		return;

	if ( (unsigned) id >= bot_maxdebugpolys )
		return;

	debugpolygons[id].inuse = qfalse;
}

/*
==================
BotImport_DebugLineCreate
==================
*/
static int BotImport_DebugLineCreate(void) {
	vec3_t points[1];
	return BotImport_DebugPolygonCreate(0, 0, points);
}

/*
==================
BotImport_DebugLineDelete
==================
*/
static void BotImport_DebugLineDelete(int line) {
	BotImport_DebugPolygonDelete(line);
}

/*
==================
BotImport_DebugLineShow
==================
*/
static void BotImport_DebugLineShow(int line, vec3_t start, vec3_t end, int color) {
	vec3_t points[4], dir, cross, up = {0, 0, 1};
	float dot;

	VectorCopy(start, points[0]);
	VectorCopy(start, points[1]);
	//points[1][2] -= 2;
	VectorCopy(end, points[2]);
	//points[2][2] -= 2;
	VectorCopy(end, points[3]);


	VectorSubtract(end, start, dir);
	VectorNormalize(dir);
	dot = DotProduct(dir, up);
	if (dot > 0.99 || dot < -0.99) VectorSet(cross, 1, 0, 0);
	else CrossProduct(dir, up, cross);

	VectorNormalize(cross);

	VectorMA(points[0], 2, cross, points[0]);
	VectorMA(points[1], -2, cross, points[1]);
	VectorMA(points[2], -2, cross, points[2]);
	VectorMA(points[3], 2, cross, points[3]);

	BotImport_DebugPolygonShow(line, color, 4, points);
}

/*
==================
SV_BotClientCommand
==================
*/
static void BotClientCommand( int client, const char *command ) {
	if ( (unsigned) client < sv.maxclients ) {
		SV_ExecuteClientCommand( &svs.clients[client], command );
	}
}

/*
==================
SV_BotFrame
==================
*/
void SV_BotFrame( int time ) {
	if (!bot_enable) return;
	//NOTE: maybe the game is already shutdown
	if (!gvm) return;
	VM_Call( gvm, 1, BOTAI_START_FRAME, time );
}

/*
===============
SV_BotLibSetup
===============
*/
int SV_BotLibSetup( void ) {
	if (!bot_enable) {
		return 0;
	}

	if ( !botlib_export ) {
		Com_Log( SEV_INFO, LOG_CH(ch_botlib), S_COLOR_ERROR "Error: SV_BotLibSetup without SV_BotInitBotLib\n" );
		return -1;
	}

	return botlib_export->BotLibSetup();
}

/*
===============
SV_ShutdownBotLib

Called when either the entire server is being killed, or
it is changing to a different game directory.
===============
*/
int SV_BotLibShutdown( void ) {

	if ( !botlib_export ) {
		return -1;
	}

	return botlib_export->BotLibShutdown();
}

static const cvarDesc_t botDescs[] = {
	CVAR_BOOL(   "bot_enable",            "1",   0,          "Enable the bot." ),
	CVAR_BOOL(   "bot_developer",         "0",   CVAR_CHEAT, "Bot developer mode." ),
	CVAR_BOOL(   "bot_debug",             "0",   CVAR_CHEAT, "Enable bot debugging." ),
	CVAR_INT(    "bot_maxdebugpolys",     "2",   0,          "Maximum number of debug polygons.", 0, 65536 ),
	CVAR_BOOL(   "bot_groundonly",        "1",   0,          "Only show ground faces of areas." ),
	CVAR_BOOL(   "bot_reachability",      "0",   0,          "Show all reachabilities to other areas." ),
	CVAR_BOOL(   "bot_visualizejumppads", "0",   CVAR_CHEAT, "Show jumppads." ),
	CVAR_BOOL(   "bot_forceclustering",   "0",   0,          "Force cluster calculations." ),
	CVAR_BOOL(   "bot_forcereachability", "0",   0,          "Force reachability calculations." ),
	CVAR_BOOL(   "bot_forcewrite",        "0",   0,          "Force writing AAS file." ),
	CVAR_BOOL(   "bot_aasoptimize",       "0",   0,          "Disable AAS file optimisation." ),
	CVAR_BOOL(   "bot_saveroutingcache",  "0",   0,          "Save routing cache." ),
	CVAR_INT(    "bot_thinktime",         "100", 0,          "Milliseconds the bots think per frame.", 0, 0 ),
	CVAR_BOOL(   "bot_reloadcharacters",  "0",   0,          "Reload the bot characters each time." ),
	CVAR_BOOL(   "bot_testichat",         "0",   0,          "Test ichats." ),
	CVAR_BOOL(   "bot_testrchat",         "0",   0,          "Test rchats." ),
	CVAR_BOOL(   "bot_testsolid",         "0",   CVAR_CHEAT, "Test for solid areas." ),
	CVAR_BOOL(   "bot_testclusters",      "0",   CVAR_CHEAT, "Test the AAS clusters." ),
	CVAR_BOOL(   "bot_fastchat",          "0",   0,          "Fast chatting bots." ),
	CVAR_BOOL(   "bot_nochat",            "0",   0,          "Disable bot chats." ),
	CVAR_BOOL(   "bot_pause",             "0",   CVAR_CHEAT, "Pause the bots thinking." ),
	CVAR_BOOL(   "bot_report",            "0",   CVAR_CHEAT, "Get a full report in CTF." ),
	CVAR_INT(    "bot_minplayers",        "0",   0,          "Minimum players in a team or the game.", 0, 0 ),
	CVAR_STRING( "bot_interbreedchar",    "",    CVAR_CHEAT, "Bot character used for interbreeding." ),
	CVAR_INT(    "bot_interbreedbots",    "10",  CVAR_CHEAT, "Number of bots used for interbreeding.", 0, 0 ),
	CVAR_INT(    "bot_interbreedcycle",   "20",  CVAR_CHEAT, "Bot interbreeding cycle.", 0, 0 ),
	CVAR_STRING( "bot_interbreedwrite",   "",    CVAR_CHEAT, "Write interbreeded bots to this file." ),
	CVAR_BOOL(   "bot_highlightarea",     "0",   0,          "Highlight a specific AAS area for debugging." ),
};

/*
==================
SV_BotInitCvars
==================
*/
void SV_BotInitCvars(void) {
	Cvar_RegisterTable( botDescs, ARRAY_LEN( botDescs ), NULL );
}

/*
==================
SV_BotInitBotLib
==================
*/
void SV_BotInitBotLib(void) {
	botlib_import_t	botlib_import;

	if (debugpolygons) Z_Free(debugpolygons);
	bot_maxdebugpolys = Cvar_VariableIntegerValue("bot_maxdebugpolys");
	debugpolygons = Z_Malloc(sizeof(bot_debugpoly_t) * bot_maxdebugpolys);

	botlib_import.Print = BotImport_Print;
	botlib_import.Trace = BotImport_Trace;
	botlib_import.EntityTrace = BotImport_EntityTrace;
	botlib_import.PointContents = BotImport_PointContents;
	botlib_import.inPVS = BotImport_inPVS;
	botlib_import.BSPEntityData = BotImport_BSPEntityData;
	botlib_import.BSPModelMinsMaxsOrigin = BotImport_BSPModelMinsMaxsOrigin;
	botlib_import.BotClientCommand = BotClientCommand;

	//memory management
	botlib_import.GetMemory = BotImport_GetMemory;
	botlib_import.FreeMemory = BotImport_FreeMemory;
	botlib_import.AvailableMemory = Z_AvailableMemory;
	botlib_import.HunkAlloc = BotImport_HunkAlloc;

	// file system access
	botlib_import.FS_FOpenFile = FS_FOpenFileByMode;
	botlib_import.FS_Read = FS_Read;
	botlib_import.FS_Write = FS_Write;
	botlib_import.FS_FCloseFile = FS_FCloseFile;
	botlib_import.FS_Seek = FS_Seek;

	//debug lines
	botlib_import.DebugLineCreate = BotImport_DebugLineCreate;
	botlib_import.DebugLineDelete = BotImport_DebugLineDelete;
	botlib_import.DebugLineShow = BotImport_DebugLineShow;

	//debug polygons
	botlib_import.DebugPolygonCreate = BotImport_DebugPolygonCreate;
	botlib_import.DebugPolygonDelete = BotImport_DebugPolygonDelete;

	botlib_import.Sys_Milliseconds = Sys_Milliseconds;

	botlib_export = (botlib_export_t *)GetBotLibAPI( BOTLIB_API_VERSION, &botlib_import );
	assert(botlib_export); 	// somehow we end up with a zero import.
}


//
//  * * * BOT AI CODE IS BELOW THIS POINT * * *
//

/*
==================
SV_BotGetConsoleMessage
==================
*/
int SV_BotGetConsoleMessage( int client, char *buf, int size )
{
	if ( (unsigned) client < sv.maxclients ) {
		client_t* cl;
		int index;

		cl = &svs.clients[client];
		cl->lastPacketTime = svs.time;

		if ( cl->reliableAcknowledge == cl->reliableSequence ) {
			return qfalse;
		}

		cl->reliableAcknowledge++;
		index = cl->reliableAcknowledge & ( MAX_RELIABLE_COMMANDS - 1 );

		if ( !cl->reliableCommands[index][0] ) {
			return qfalse;
		}

		Q_strncpyz( buf, cl->reliableCommands[index], size );
		return qtrue;
	}
	return qfalse;
}


#if 0
/*
==================
EntityInPVS
==================
*/
int EntityInPVS( int client, int entityNum ) {
	client_t			*cl;
	clientSnapshot_t	*frame;

	cl = &svs.clients[client];
	frame = &cl->frames[cl->wn_outgoing_sequence & PACKET_MASK];
	for ( int i = 0; i < frame->num_entities; i++ )	{
		if ( svs.snapshotEntities[(frame->first_entity + i) % svs.numSnapshotEntities].number == entityNum ) {
			return qtrue;
		}
	}
	return qfalse;
}
#endif


/*
==================
SV_BotGetSnapshotEntity
==================
*/
int SV_BotGetSnapshotEntity( int client, int sequence ) {
	if ( (unsigned) client < sv.maxclients ) {
		const client_t* cl = &svs.clients[client];
		const clientSnapshot_t* frame = &cl->frames[cl->wn_outgoing_sequence & PACKET_MASK];
		if ( (unsigned) sequence >= frame->num_entities ) {
			return -1;
		}
		return frame->ents[sequence]->number;
	}
	return -1;
}
