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
// sv_client.c -- server code for dealing with clients

#include "server.h"
#include "../wired/net/wn_public.h"

static void SV_CloseDownload( client_t *cl );

static qboolean SV_WiredNetWriteU16( byte *buf, int bufsize, int *offset, uint16_t value )
{
	if ( *offset + 2 > bufsize ) return qfalse;
	buf[(*offset)++] = (byte)( value & 0xFF );
	buf[(*offset)++] = (byte)( ( value >> 8 ) & 0xFF );
	return qtrue;
}

static qboolean SV_WiredNetWriteU32( byte *buf, int bufsize, int *offset, uint32_t value )
{
	if ( *offset + 4 > bufsize ) return qfalse;
	buf[(*offset)++] = (byte)( value & 0xFF );
	buf[(*offset)++] = (byte)( ( value >> 8 ) & 0xFF );
	buf[(*offset)++] = (byte)( ( value >> 16 ) & 0xFF );
	buf[(*offset)++] = (byte)( ( value >> 24 ) & 0xFF );
	return qtrue;
}

static qboolean SV_WiredNetWriteS32( byte *buf, int bufsize, int *offset, int value )
{
	return SV_WiredNetWriteU32( buf, bufsize, offset, (uint32_t)value );
}

static qboolean SV_WiredNetWriteBytes( byte *buf, int bufsize, int *offset, const void *data, int len )
{
	if ( *offset + len > bufsize ) return qfalse;
	Com_Memcpy( buf + *offset, data, (size_t)len );
	*offset += len;
	return qtrue;
}

static qboolean SV_WiredNetWriteString( byte *buf, int bufsize, int *offset, const char *s )
{
	int len = (int)strlen( s ) + 1;
	return SV_WiredNetWriteU16( buf, bufsize, offset, (uint16_t)len )
		&& SV_WiredNetWriteBytes( buf, bufsize, offset, s, len );
}

static qboolean SV_WiredNetWriteFloat( byte *buf, int bufsize, int *offset, float value )
{
	uint32_t bits;
	Com_Memcpy( &bits, &value, sizeof( bits ) );
	return SV_WiredNetWriteU32( buf, bufsize, offset, bits );
}

static qboolean SV_WiredNetWriteTrajectory( byte *buf, int bufsize, int *offset, const trajectory_t *tr )
{
	int i;
	if ( !SV_WiredNetWriteS32( buf, bufsize, offset, tr->trType ) ||
		!SV_WiredNetWriteS32( buf, bufsize, offset, tr->trTime ) ||
		!SV_WiredNetWriteS32( buf, bufsize, offset, tr->trDuration ) ) {
		return qfalse;
	}
	for ( i = 0; i < 3; i++ ) if ( !SV_WiredNetWriteFloat( buf, bufsize, offset, tr->trBase[i] ) ) return qfalse;
	for ( i = 0; i < 3; i++ ) if ( !SV_WiredNetWriteFloat( buf, bufsize, offset, tr->trDelta[i] ) ) return qfalse;
	return qtrue;
}

static qboolean SV_WiredNetWriteEntityState( byte *buf, int bufsize, int *offset, const entityState_t *es )
{
	int i;
	if ( !SV_WiredNetWriteS32( buf, bufsize, offset, es->number ) ||
		!SV_WiredNetWriteS32( buf, bufsize, offset, es->eType ) ||
		!SV_WiredNetWriteS32( buf, bufsize, offset, es->eFlags ) ||
		!SV_WiredNetWriteTrajectory( buf, bufsize, offset, &es->pos ) ||
		!SV_WiredNetWriteTrajectory( buf, bufsize, offset, &es->apos ) ||
		!SV_WiredNetWriteS32( buf, bufsize, offset, es->time ) ||
		!SV_WiredNetWriteS32( buf, bufsize, offset, es->time2 ) ) {
		return qfalse;
	}
	for ( i = 0; i < 3; i++ ) if ( !SV_WiredNetWriteFloat( buf, bufsize, offset, es->origin[i] ) ) return qfalse;
	for ( i = 0; i < 3; i++ ) if ( !SV_WiredNetWriteFloat( buf, bufsize, offset, es->origin2[i] ) ) return qfalse;
	for ( i = 0; i < 3; i++ ) if ( !SV_WiredNetWriteFloat( buf, bufsize, offset, es->angles[i] ) ) return qfalse;
	for ( i = 0; i < 3; i++ ) if ( !SV_WiredNetWriteFloat( buf, bufsize, offset, es->angles2[i] ) ) return qfalse;
	return SV_WiredNetWriteS32( buf, bufsize, offset, es->otherEntityNum )
		&& SV_WiredNetWriteS32( buf, bufsize, offset, es->otherEntityNum2 )
		&& SV_WiredNetWriteS32( buf, bufsize, offset, es->groundEntityNum )
		&& SV_WiredNetWriteS32( buf, bufsize, offset, es->constantLight )
		&& SV_WiredNetWriteS32( buf, bufsize, offset, es->loopSound )
		&& SV_WiredNetWriteS32( buf, bufsize, offset, es->modelindex )
		&& SV_WiredNetWriteS32( buf, bufsize, offset, es->modelindex2 )
		&& SV_WiredNetWriteS32( buf, bufsize, offset, es->clientNum )
		&& SV_WiredNetWriteS32( buf, bufsize, offset, es->frame )
		&& SV_WiredNetWriteS32( buf, bufsize, offset, es->solid )
		&& SV_WiredNetWriteS32( buf, bufsize, offset, es->event )
		&& SV_WiredNetWriteS32( buf, bufsize, offset, es->eventParm )
		&& SV_WiredNetWriteS32( buf, bufsize, offset, es->powerups )
		&& SV_WiredNetWriteS32( buf, bufsize, offset, es->weapon )
		&& SV_WiredNetWriteS32( buf, bufsize, offset, es->legsAnim )
		&& SV_WiredNetWriteS32( buf, bufsize, offset, es->torsoAnim )
		&& SV_WiredNetWriteS32( buf, bufsize, offset, es->generic1 );
}

static qboolean SV_WiredNetBeginSection( byte *buf, int bufsize, int *offset, int type, int *lenPos )
{
	if ( *offset + 5 > bufsize ) return qfalse;
	buf[(*offset)++] = (byte)type;
	*lenPos = *offset;
	*offset += 4;
	return qtrue;
}

static void SV_WiredNetFinishSection( byte *buf, int lenPos, int sectionStart, int offset )
{
	uint32_t len = (uint32_t)( offset - sectionStart );
	buf[lenPos + 0] = (byte)( len & 0xFF );
	buf[lenPos + 1] = (byte)( ( len >> 8 ) & 0xFF );
	buf[lenPos + 2] = (byte)( ( len >> 16 ) & 0xFF );
	buf[lenPos + 3] = (byte)( ( len >> 24 ) & 0xFF );
}

static qboolean SV_WiredNetWriteBootstrap( client_t *client, byte *buf, int bufsize, int *outLen )
{
	int offset = 0;
	int lenPos;
	int sectionStart;
	int start;
	const svEntity_t *svEnt;
	char systemInfo[BIG_INFO_STRING];
	int count;

	buf[offset++] = WN_BOOTSTRAP_MSG_STATE;

	if ( !SV_WiredNetBeginSection( buf, bufsize, &offset, WN_BOOTSTRAP_SEC_ACK, &lenPos ) ) return qfalse;
	sectionStart = offset;
	if ( !SV_WiredNetWriteS32( buf, bufsize, &offset, client->lastClientCommand ) ) return qfalse;
	SV_WiredNetFinishSection( buf, lenPos, sectionStart, offset );

	if ( !SV_WiredNetBeginSection( buf, bufsize, &offset, WN_BOOTSTRAP_SEC_SERVER_CMDS, &lenPos ) ) return qfalse;
	sectionStart = offset;
	count = client->reliableSequence - client->reliableAcknowledge;
	if ( !SV_WiredNetWriteU16( buf, bufsize, &offset, (uint16_t)count ) ) return qfalse;
	for ( start = 0; start < count; start++ ) {
		const int index = client->reliableAcknowledge + 1 + start;
		if ( !SV_WiredNetWriteS32( buf, bufsize, &offset, index ) ||
			!SV_WiredNetWriteString( buf, bufsize, &offset,
				client->reliableCommands[index & (MAX_RELIABLE_COMMANDS-1)] ) ) {
			return qfalse;
		}
	}
	SV_WiredNetFinishSection( buf, lenPos, sectionStart, offset );

	if ( !SV_WiredNetBeginSection( buf, bufsize, &offset, WN_BOOTSTRAP_SEC_CONFIGSTRINGS, &lenPos ) ) return qfalse;
	sectionStart = offset;
	count = 0;
	for ( start = 0; start < MAX_CONFIGSTRINGS; start++ ) {
		if ( *sv.configstrings[start] != '\0' ) count++;
	}
	if ( !SV_WiredNetWriteU16( buf, bufsize, &offset, (uint16_t)count ) ) return qfalse;
	for ( start = 0; start < MAX_CONFIGSTRINGS; start++ ) {
		const char *value;
		if ( *sv.configstrings[start] == '\0' ) continue;
		value = sv.configstrings[start];
		if ( start == CS_SYSTEMINFO && sv.pure != sv_pure->integer ) {
			Q_strncpyz( systemInfo, value, sizeof( systemInfo ) );
			Info_SetValueForKey_s( systemInfo, sizeof( systemInfo ), "sv_pure", va( "%i", sv.pure ) );
			value = systemInfo;
		}
		if ( !SV_WiredNetWriteU16( buf, bufsize, &offset, (uint16_t)start ) ||
			!SV_WiredNetWriteString( buf, bufsize, &offset, value ) ) {
			return qfalse;
		}
	}
	SV_WiredNetFinishSection( buf, lenPos, sectionStart, offset );

	if ( !SV_WiredNetBeginSection( buf, bufsize, &offset, WN_BOOTSTRAP_SEC_BASELINES, &lenPos ) ) return qfalse;
	sectionStart = offset;
	count = 0;
	for ( start = 0; start < MAX_GENTITIES; start++ ) {
		if ( sv.baselineUsed[start] ) count++;
	}
	if ( !SV_WiredNetWriteU16( buf, bufsize, &offset, (uint16_t)count ) ) return qfalse;
	for ( start = 0; start < MAX_GENTITIES; start++ ) {
		if ( !sv.baselineUsed[start] ) continue;
		svEnt = &sv.svEntities[start];
		if ( !SV_WiredNetWriteU16( buf, bufsize, &offset, (uint16_t)start ) ||
			!SV_WiredNetWriteEntityState( buf, bufsize, &offset, &svEnt->baseline ) ) {
			return qfalse;
		}
	}
	SV_WiredNetFinishSection( buf, lenPos, sectionStart, offset );

	if ( !SV_WiredNetBeginSection( buf, bufsize, &offset, WN_BOOTSTRAP_SEC_CLIENT_INFO, &lenPos ) ) return qfalse;
	sectionStart = offset;
	if ( !SV_WiredNetWriteS32( buf, bufsize, &offset, client - svs.clients ) ||
		!SV_WiredNetWriteS32( buf, bufsize, &offset, sv.checksumFeed ) ) {
		return qfalse;
	}
	SV_WiredNetFinishSection( buf, lenPos, sectionStart, offset );

	*outLen = offset;
	return qtrue;
}


/*
==================
SV_IsBanned

Check whether a certain address is banned
==================
*/
#ifdef USE_BANS

static qboolean SV_IsBanned( const netadr_t *from, qboolean isexception )
{
	int index;
	serverBan_t *curban;

	if(!isexception)
	{
		// If this is a query for a ban, first check whether the client is excepted
		if(SV_IsBanned(from, qtrue))
			return qfalse;
	}

	for(index = 0; index < serverBansCount; index++)
	{
		curban = &serverBans[index];

		if(curban->isexception == isexception)
		{
			if(NET_CompareBaseAdrMask(&curban->ip, from, curban->subnet))
				return qtrue;
		}
	}

	return qfalse;
}
#endif


/*
==================
SV_SetClientTLD
==================
*/
#pragma pack(push,1)

typedef struct iprange_s {
	uint32_t from;
	uint32_t to;
} iprange_t;

typedef struct iprange_tld_s {
	char tld[2];
} iprange_tld_t;

#pragma pack(pop)

static qboolean ipdb_loaded;
static iprange_t *ipdb_range;
static iprange_tld_t *ipdb_tld;
static int num_tlds;

typedef struct tld_info_s {
	const char *tld;
	const char *country;
} tld_info_t;

static const tld_info_t tld_info[] = {
#include "tlds.h"
};

/*
==================
SV_FreeIP4DB
==================
*/
void SV_FreeIP4DB( void )
{
	if ( ipdb_range )
		Z_Free( ipdb_range );

	ipdb_loaded = qfalse;
	ipdb_range = NULL;
	ipdb_tld = NULL;
}


/*
==================
SV_LoadIP4DB

Loads geoip database into memory
==================
*/
static qboolean SV_LoadIP4DB( const char *filename )
{
	fileHandle_t fh = FS_INVALID_HANDLE;
	uint32_t last_ip;
	void *buf;
	int len, res, i;

	len = FS_SV_FOpenFileRead( filename, &fh );

	if ( len <= 0 )
	{
		if ( fh != FS_INVALID_HANDLE )
			FS_FCloseFile( fh );
		return qfalse;
	}

	if ( len % 10 ) // should be a power of IP4:IP4:TLD2
	{
		Com_DPrintf( "%s(%s): invalid file size %i\n", __func__, filename, len );
		if ( fh != FS_INVALID_HANDLE )
			FS_FCloseFile( fh );
		return qfalse;
	}

	SV_FreeIP4DB();

	buf = Z_Malloc( len );

	res = FS_Read( buf, len, fh );
	FS_FCloseFile( fh );

	if ( res != len ) {
		Z_Free( buf );
		return qfalse;
	}

	// check integrity of loaded database
	last_ip = 0;
	num_tlds = len / 10;

	// database format:
	// [range1][range2]...[rangeN]
	// [tld1][tld2]...[tldN]

	ipdb_range = (iprange_t*)buf;
	ipdb_tld = (iprange_tld_t*)(ipdb_range + num_tlds);

	for ( i = 0; i < num_tlds; i++ )
	{
#ifdef Q3_LITTLE_ENDIAN
		ipdb_range[i].from = LongSwap( ipdb_range[i].from );
		ipdb_range[i].to = LongSwap( ipdb_range[i].to );
#endif
		if ( last_ip && last_ip >= ipdb_range[i].from )
			break;
		if ( ipdb_range[i].from > ipdb_range[i].to )
			break;
		if ( ipdb_tld[i].tld[0] < 'A' || ipdb_tld[i].tld[0] > 'Z' || ipdb_tld[i].tld[1] < 'A' || ipdb_tld[i].tld[1] > 'Z' )
			break;
		last_ip = ipdb_range[i].to;
	}

	if ( i != num_tlds ) {
			Com_Printf( S_COLOR_YELLOW "invalid ip4db entry #%i: range=[%08x..%08x], tld=%c%c\n",
				i, ipdb_range[i].from, ipdb_range[i].to, ipdb_tld[i].tld[0], ipdb_tld[i].tld[1] );
			SV_FreeIP4DB();
			return qtrue; // to not try to load it again
	}

	Com_Printf( "ip4db: %i entries loaded\n", num_tlds );
	return qtrue;
}


static void SV_SetTLD( char *str, const netadr_t *from, qboolean isLAN )
{
	const iprange_t *e;
	int lo, hi, m;
	uint32_t ip;

	str[0] = '\0';

	if ( sv_clientTLD->integer == 0 )
		return;

	if ( isLAN )
	{
		strcpy( str, "**" );
		return;
	}

	if ( from->type != NA_IP ) // ipv4-only
		return;

	if ( !ipdb_loaded )
		ipdb_loaded = SV_LoadIP4DB( "ip4db.dat" );

	if ( !ipdb_range )
		return;

	lo = 0;
	hi = num_tlds;

	// big-endian to host-endian
#ifdef Q3_LITTLE_ENDIAN
	ip =  from->ipv._4[3] | from->ipv._4[2] << 8 | from->ipv._4[1] << 16 | from->ipv._4[0] << 24;
#else
	ip =  from->ipv._4[0] | from->ipv._4[1] << 8 | from->ipv._4[2] << 16 | from->ipv._4[3] << 24;
#endif

	// binary search
	while ( lo <= hi )
	{
		m = ( lo + hi ) / 2;
		e = ipdb_range + m;
		if ( ip >= e->from && ip <= e->to )
		{
			const iprange_tld_t *tld = ipdb_tld + m;
			str[0] = tld->tld[0];
			str[1] = tld->tld[1];
			str[2] = '\0';
			return;
		}

		if ( e->from > ip )
			hi = m - 1;
		else
			lo = m + 1;
	}
}


static int seqs[ MAX_CLIENTS ];

static void SV_SaveSequences( void ) {
	int i;
	for ( i = 0; i < sv.maxclients; i++ ) {
		seqs[i] = svs.clients[i].reliableSequence;
	}
}


static void SV_InjectLocation( const char *tld, const char *country ) {
	const char *cmd;
	char *str;
	int i, n;
	for ( i = 0; i < sv.maxclients; i++ ) {
		if ( seqs[i] != svs.clients[i].reliableSequence ) {
			for ( n = seqs[i]; n != svs.clients[i].reliableSequence + 1; n++ ) {
				cmd = svs.clients[i].reliableCommands[n & (MAX_RELIABLE_COMMANDS-1)];
				str = strstr( cmd, "connected\n\"" );
				if ( str && str[11] == '\0' && str < cmd + 512 ) {
					if ( *tld == '\0' )
						sprintf( str, S_COLOR_WHITE "connected (%s)\n\"", country );
					else
						sprintf( str, S_COLOR_WHITE "connected (" S_COLOR_RED "%s" S_COLOR_WHITE ", %s)\n\"", tld, country );
					break;
				}
			}
		}
	}
}


static const char *SV_FindCountry( const char *tld ) {
	int i;

	if ( *tld == '\0' )
		return "Unknown Location";

	for ( i = 0; i < ARRAY_LEN( tld_info ); i++ ) {
		if ( !strcmp( tld, tld_info[i].tld ) ) {
			return tld_info[i].country;
		}
	}

	return "Unknown Location";
}


static const char *SV_GetStateName( clientState_t state ) {
	switch ( state ) {
		case CS_FREE:      return "CS_FREE";
		case CS_ZOMBIE:    return "CS_ZOMBIE";
		case CS_CONNECTED: return "CS_CONNECTED";
		case CS_PRIMED:    return "CS_PRIMED";
		case CS_ACTIVE:    return "CS_ACTIVE";
		default:           return "CS_UNKNOWN";
	}
}


void SV_PrintClientStateChange( const client_t *cl, clientState_t newState ) {

	if ( cl->state == newState ) {
		return;
	}

#ifndef _DEBUG
	if ( com_developer->integer == 0 ) {
		return;
	}
#endif // !_DEBUG

	if ( cl->name[0] != '\0' ) {
		Com_Printf( "Going from %s to %s for %s\n", SV_GetStateName( cl->state ), SV_GetStateName( newState ), cl->name );
	} else {
		Com_Printf( "Going from %s to %s for client %d\n", SV_GetStateName( cl->state ), SV_GetStateName( newState ), (int)(cl - svs.clients) );
	}
	
}


/*
==================
SV_OnPlayerConnect

Called on the main thread (from WN_DrainPendingConnects) when a QUIC game
client has completed the Stream 0 binary TLV handshake. TLV ACCEPT was already
sent by WN_GameHandleHandshake — this function finalises the slot and notifies
the game VM.

Lean first-pass: fresh slot allocation, no IP-based reconnect detection.
Reconnect detection will use conn_handle_t (picoquic callback_close + new) —
added after Phase B passes its test gate.
==================
*/
/* Forward declaration: SV_SendClientGameState is defined later in this file
 * but must be called from SV_OnPlayerConnect for the QUIC fast-path. */
static void SV_SendClientGameState( client_t *client );

void SV_OnPlayerConnect( conn_handle_t conn, const char *userinfo )
{
	char        tld[3];
	client_t   *cl, *newcl;
	int         i, clientNum, count;
	intptr_t    denied;
	netadr_t    from;

	/* R4 guard: server is mid-spawn — gvm may be NULL (P1..P2) or baselines
	   not yet built (P3..P4).  Re-queue the connection so it is retried each
	   frame until svs.spawn.phase reaches SPAWN_IDLE (end of P5).
	   The QUIC connection stays alive; WN_DrainPendingConnects calls us
	   again next frame. */
	if ( svs.spawn.phase != SPAWN_IDLE ) {
		Com_Printf( "SV_OnPlayerConnect: spawn in progress (phase %d), deferring conn %llu\n",
			(int)svs.spawn.phase, (unsigned long long)conn );
		WN_RequeueConnect( conn, userinfo );
		return;
	}

	Com_DPrintf( "SV_OnPlayerConnect: conn=%llu\n", (unsigned long long)conn );

	if ( !WN_GetAddrByConnHandle( conn, &from ) ) {
		Com_Printf( S_COLOR_YELLOW "SV_OnPlayerConnect: unknown conn handle %llu\n",
			(unsigned long long)conn );
		return;
	}
	/* Find a free slot (select least-recently-freed) */
	newcl = NULL;
	for ( i = 0; i < sv.maxclients; i++ ) {
		cl = &svs.clients[i];
		if ( cl->state == CS_FREE ) {
			if ( newcl == NULL ||
			     svs.time - cl->lastDisconnectTime > svs.time - newcl->lastDisconnectTime )
				newcl = cl;
		}
	}
	if ( !newcl ) {
		Com_Printf( "SV_OnPlayerConnect: server full, dropping conn %llu\n",
			(unsigned long long)conn );
		if ( transport )
			transport->drop_client( conn, "server full" );
		return;
	}

	clientNum = (int)( newcl - svs.clients );
	Com_Memset( newcl, 0, sizeof(*newcl) );

	/* Store QUIC connection handle — used by all future transport calls */
	newcl->quic_conn = conn;

	/* Phase D: netchan replaced by QUIC — only keep the fields still referenced downstream. */
	newcl->netchan.remoteAddress  = from;
	newcl->wn_outgoing_sequence = 0;
	newcl->netchan.incomingSequence = 0;
	newcl->netchan.isLANAddress   = Sys_IsLANAddress( &from );

	Q_strncpyz( newcl->userinfo, userinfo, sizeof(newcl->userinfo) );
	newcl->longstr = qtrue; /* QUIC clients always support long strings */

	SV_SetTLD( tld, &from, Sys_IsLANAddress( &from ) );
	Q_strncpyz( newcl->tld, tld, sizeof(newcl->tld) );
	newcl->country = SV_FindCountry( newcl->tld );

	SV_UserinfoChanged( newcl, qtrue, qfalse );

	if ( sv_clientTLD->integer )
		SV_SaveSequences();

	/* Let the game VM accept or reject */
	denied = VM_Call( gvm, 3, GAME_CLIENT_CONNECT, clientNum, qtrue, qfalse );
	if ( denied ) {
		const char *reason = GVM_ArgPtr( denied );
		Com_Printf( "QUIC: game rejected connection from %s: %s\n",
			NET_AdrToString( &from ), reason );
		if ( transport )
			transport->drop_client( conn, reason );
		Com_Memset( newcl, 0, sizeof(*newcl) );
		return;
	}

	if ( sv_clientTLD->integer )
		SV_InjectLocation( newcl->tld, newcl->country );

	SV_PrintClientStateChange( newcl, CS_CONNECTED );
	newcl->state              = CS_CONNECTED;
	newcl->lastSnapshotTime   = svs.time - 9999;
	newcl->lastPacketTime     = svs.time;
	newcl->lastConnectTime    = svs.time;
	newcl->lastDisconnectTime = svs.time;
	/* QUIC clients are authenticated by TLS — the justConnected anti-spoofing
	 * timer is redundant and would fire before the gamestate exchange completes.
	 * Skip it entirely. */
	newcl->justConnected      = qfalse;
	/* Force gamestate retransmit on first snapshot */
	newcl->gamestateMessageNum = newcl->messageAcknowledge - 1;

	/* Send gamestate immediately: there is no usercmd exchange to trigger it
	 * from SV_ExecuteClientMessage (client is waiting for gamestate to exit
	 * CA_CONNECTING — a deadlock without this direct call). */
	SV_SendClientGameState( newcl );

	Com_DPrintf( "SV_OnPlayerConnect: slot %d assigned to conn=%llu (%s)\n",
		clientNum, (unsigned long long)conn, NET_AdrToString( &from ) );

	/* Heartbeat to master if first or last slot filled */
	count = 0;
	for ( i = 0; i < sv.maxclients; i++ ) {
		if ( svs.clients[i].state >= CS_CONNECTED )
			count++;
	}
	if ( count == 1 || count == sv.maxclients )
		SV_Heartbeat_f();
}

/*
==================
SV_OnPlayerReady

Called on the main thread (from WN_DrainPendingReady) when a QUIC game
client sends TLV 0x05 READY, signalling it has processed the gamestate.
Finds the CS_PRIMED client slot matching the conn handle and calls
SV_ClientEnterWorld to transition it to CS_ACTIVE.
==================
*/
void SV_OnPlayerReady( conn_handle_t conn )
{
	int       i;
	client_t *cl;

	for ( i = 0; i < sv_maxclients->integer; i++ ) {
		cl = &svs.clients[i];
		if ( cl->state == CS_PRIMED && cl->quic_conn == conn ) {
			Com_DPrintf( "QUIC: SV_OnPlayerReady — slot %d (%s)\n", i, cl->name );
			SV_ClientEnterWorld( cl );
			return;
		}
	}
	Com_DPrintf( "QUIC: SV_OnPlayerReady: no CS_PRIMED client for conn %llu\n",
		(unsigned long long)conn );
}

/*
==================
SV_DrainQUICUsercmds

Drain all pending usercmd datagrams sent by QUIC game clients.
Datagram format (written by CL_WritePacket QUIC path, key=0, bitstream):
  [client_tick:u32]  — client's current cmdNumber (newest cmd)
  [snapshot_ack:u32] — last srv_tick the client received (for delta snapshots)
  [cmd_count:u8]     — number of cmds (1–3)
  [delta-cmds]       — MSG_WriteDeltaUsercmdKey with key=0 from nullcmd

Called once per server frame from sv_main.c before SV_DrainQUICReliableCommands.
==================
*/
void SV_DrainQUICUsercmds( void )
{
	byte          dgbuf[2048];
	int           dglen;
	conn_handle_t dgconn;
	int           i;
	client_t     *cl;

	/* Use WN_ServerRecvUsercmd directly — NOT transport->recv_unreliable.
	 * In loopback, transport->recv_unreliable is the client snapshot path
	 * (reads wtcl.recv_queue).  Server user commands live in gc->recv_queue
	 * and must be drained via the dedicated server function. */
	if ( !transport )
		return;

	dglen = (int)sizeof( dgbuf );
	while ( WN_ServerRecvUsercmd( &dgconn, dgbuf, &dglen ) ) {
		if ( dglen >= 13 ) {  /* client_tick(4) + snapshot_ack(4) + serverCmd_ack(4) + cmd_count(1) minimum */
			msg_t         msg;
			int           snapshotAck;
			int           serverCmdAck;
			int           cmdCount;
			int           j;
			static const usercmd_t nullcmd = { 0 };
			usercmd_t     cmds[MAX_PACKET_USERCMDS];
			const usercmd_t *oldcmd;

			cl = NULL;
			for ( j = 0; j < sv_maxclients->integer; j++ ) {
				if ( svs.clients[j].state >= CS_CONNECTED &&
				     svs.clients[j].quic_conn == dgconn ) {
					cl = &svs.clients[j];
					break;
				}
			}

			if ( !cl ) {
				dglen = (int)sizeof( dgbuf );
				continue;
			}

			MSG_Init( &msg, dgbuf, dglen );
			msg.cursize = dglen;
			MSG_Bitstream( &msg );

			/* client_tick:u32 — client cmdNumber, informational */
			(void)MSG_ReadLong( &msg );
			/* snapshot_ack:u32 — last srv snapshot received by client */
			snapshotAck   = MSG_ReadLong( &msg );
			/* serverCmd_ack:u32 — last server command processed by client;
			 * advance reliableAcknowledge so processed commands stop being
			 * re-embedded in snapshots and the overflow guard never fires. */
			serverCmdAck  = MSG_ReadLong( &msg );
			/* cmd_count:u8 */
			cmdCount      = MSG_ReadByte( &msg );

			if ( cmdCount < 1 || cmdCount > MAX_PACKET_USERCMDS ) {
				dglen = (int)sizeof( dgbuf );
				continue;
			}

			/* refresh keep-alive so SV_CheckTimeouts doesn't drop the QUIC client;
			 * netchan clients do this in SV_ExecuteClientMessage but QUIC never goes there */
			cl->lastPacketTime = svs.time;

			/* update snapshot delta baseline — enables delta-compressed snapshots */
			cl->messageAcknowledge = snapshotAck;
			cl->deltaMessage       = snapshotAck;

			/* advance reliableAcknowledge — prevents re-embedding already-processed
			 * server commands and keeps the queue from hitting the overflow guard */
			if ( serverCmdAck - cl->reliableAcknowledge > 0 &&
			     serverCmdAck - cl->reliableSequence <= 0 ) {
				cl->reliableAcknowledge = serverCmdAck;
			}

			WN_DBG( "usercmd recv: client=%s snapAck=%u cmdAck=%d → deltaMessage=%d reliableAck=%d\n",
				cl->name, snapshotAck, serverCmdAck, cl->deltaMessage, cl->reliableAcknowledge );

			/* decode cmds with key=0 (TLS handles confidentiality) */
			oldcmd = &nullcmd;
			for ( i = 0; i < cmdCount; i++ ) {
				MSG_ReadDeltaUsercmdKey( &msg, 0, oldcmd, &cmds[i] );
				oldcmd = &cmds[i];
			}

			if ( cl->state != CS_ACTIVE ) {
				dglen = (int)sizeof( dgbuf );
				continue;
			}

			/* run cmds — skip duplicates already processed */
			for ( i = 0; i < cmdCount; i++ ) {
				if ( cmds[i].serverTime - cmds[cmdCount-1].serverTime > 0 )
					continue;
				if ( cmds[i].serverTime - cl->lastUsercmd.serverTime <= 0 )
					continue;
				SV_ClientThink( cl, &cmds[i] );
			}
		}
		dglen = (int)sizeof( dgbuf );
	}
}

/*
==================
SV_DrainQUICReliableCommands

Drain all pending reliable game-command messages from connected QUIC
game clients.  Called once per server frame from sv_main.c.
Reliable commands arrive as null-terminated strings; each is dispatched
to SV_ExecuteClientCommand exactly as if it had come over the netchan
reliable command queue.
==================
*/
void SV_DrainQUICReliableCommands( void )
{
	byte          buf[MAX_MSGLEN];
	int           len;
	conn_handle_t rconn;
	int           rchan;
	int           i;
	client_t     *cl;

	if ( !transport )
		return;

	len = (int)sizeof( buf );
	while ( WN_ServerRecvReliable( &rconn, &rchan, buf, &len ) ) {
		if ( rchan == CHAN_COMMANDS ) {
			/* null-terminate defensively */
			buf[ len < (int)sizeof(buf) ? len : (int)sizeof(buf) - 1 ] = '\0';

			cl = NULL;
			for ( i = 0; i < sv_maxclients->integer; i++ ) {
				if ( svs.clients[i].state >= CS_CONNECTED &&
				     svs.clients[i].quic_conn == rconn ) {
					cl = &svs.clients[i];
					break;
				}
			}

			if ( cl ) {
				/* Legacy netchan tracked per-command sequence numbers and drove
				 * lastClientCommand off the wire seq in SV_ClientCommand. QUIC
				 * streams carry the payload only, so we synthesise the ack by
				 * bumping lastClientCommand once per drained message. This keeps
				 * sv_snapshot.c:822 writing a non-stale value to the client's
				 * reliableAcknowledge field, preventing a retransmit loop where
				 * the client keeps re-sending the same userinfo/say command. */
				cl->lastClientCommand++;
				Q_strncpyz( cl->lastClientCommandString, (char *)buf,
					sizeof( cl->lastClientCommandString ) );
				SV_ExecuteClientCommand( cl, (char *)buf );
			} else {
				Com_DPrintf( "QUIC: CHAN_COMMANDS for unknown conn %llu — dropped\n",
					(unsigned long long)rconn );
			}
		} else if ( rchan == CHAN_MCP ) {
			/* MCP JSON-RPC payload from client via reliable channel.
			 * The primary MCP path is the bidi-stream content-sniff in wn_main.c;
			 * this channel path is for clients that explicitly frame MCP on CHAN_MCP. */
			Com_DPrintf( "QUIC: CHAN_MCP from conn %llu len=%d\n",
				(unsigned long long)rconn, len );
			/* Future: route to WN_ProcessMcpChannelMessage(rconn, buf, len) */
		}
		len = (int)sizeof( buf );
	}
}


/*
=====================
SV_FreeClient

Destructor for data allocated in a client structure
=====================
*/
void SV_FreeClient(client_t *client)
{
	SV_CloseDownload(client);
}


/*
=====================
SV_DropClient

Called when the player is totally leaving the server, either willingly
or unwillingly.  This is NOT called if the entire server is quitting
or crashing -- SV_FinalMessage() will handle that
=====================
*/
void SV_DropClient( client_t *drop, const char *reason ) {
	char	name[ sizeof( drop->name ) ];
	qboolean isBot;
	int		i;

	if ( drop->state == CS_ZOMBIE ) {
		return;		// already dropped
	}

	isBot = drop->netchan.remoteAddress.type == NA_BOT;

	Q_strncpyz( name, drop->name, sizeof( name ) );	// for further DPrintf() because drop->name will be nuked in SV_SetUserinfo()

	// Free all allocated data on the client structure
	SV_FreeClient( drop );

	// tell everyone why they got dropped
	if ( reason ) {
		SV_SendServerCommand( NULL, "print \"%s" S_COLOR_WHITE " %s\n\"", name, reason );
	}

	// call the prog function for removing a client
	// this will remove the body, among other things
	VM_Call( gvm, 1, GAME_CLIENT_DISCONNECT, drop - svs.clients );

	// add the disconnect command
	if ( reason ) {
		SV_SendServerCommand( drop, "disconnect \"%s\"", reason );
	}

	if ( isBot ) {
		SV_BotFreeClient( drop - svs.clients );
	}

	// nuke user info
	SV_SetUserinfo( drop - svs.clients, "" );

	drop->justConnected = qfalse;

	drop->lastDisconnectTime = svs.time;

	if ( isBot ) {
		// bots shouldn't go zombie, as there's no real net connection.
		drop->state = CS_FREE;
	} else {
		Q_strncpyz( drop->name, name, sizeof( name ) );
		SV_PrintClientStateChange( drop, CS_ZOMBIE );
		drop->state = CS_ZOMBIE;		// become free in a few seconds
	}

	if ( !reason ) {
		return;
	}

	// if this was the last client on the server, send a heartbeat
	// to the master so it is known the server is empty
	// send a heartbeat now so the master will get up to date info
	for ( i = 0; i < sv.maxclients; i++ ) {
		if ( svs.clients[i].state >= CS_CONNECTED ) {
			break;
		}
	}
	if ( i == sv.maxclients ) {
		SV_Heartbeat_f();
	}
}


/*
================
SV_RemainingGameState

estimates free space available for additional systeminfo keys
================
*/
int SV_RemainingGameState( void )
{
	int			len;
	int			start, i;
	entityState_t nullstate;
	const svEntity_t *svEnt;
	msg_t		msg;
	byte		msgBuffer[ MAX_MSGLEN_BUF ];

	MSG_Init( &msg, msgBuffer, MAX_MSGLEN );

	MSG_WriteLong( &msg, 7 ); // last client command

	for ( i = 0; i < 256; i++ ) // simulate dummy client commands
		MSG_WriteByte( &msg, i & 127 );

	// send the gamestate
	MSG_WriteByte( &msg, svc_gamestate );
	MSG_WriteLong( &msg, 7 ); // client->reliableSequence

	// write the configstrings
	for ( start = 0 ; start < MAX_CONFIGSTRINGS ; start++ ) {
		if ( start == CS_SERVERINFO ) {
			MSG_WriteByte( &msg, svc_configstring );
			MSG_WriteShort( &msg, start );
			MSG_WriteBigString( &msg, Cvar_InfoString( CVAR_SERVERINFO, NULL ) );
			continue;
		}
		if ( start == CS_SYSTEMINFO ) {
			MSG_WriteByte( &msg, svc_configstring );
			MSG_WriteShort( &msg, start );
			MSG_WriteBigString( &msg, Cvar_InfoString_Big( CVAR_SYSTEMINFO, NULL ) );
			continue;
		}
		if ( sv.configstrings[start][0] ) {
			MSG_WriteByte( &msg, svc_configstring );
			MSG_WriteShort( &msg, start );
			MSG_WriteBigString( &msg, sv.configstrings[start] );
		}
	}

	// write the baselines
	Com_Memset( &nullstate, 0, sizeof( nullstate ) );
	for ( start = 0 ; start < MAX_GENTITIES; start++ ) {
		if ( !sv.baselineUsed[ start ] ) {
			continue;
		}
		svEnt = &sv.svEntities[ start ];
		MSG_WriteByte( &msg, svc_baseline );
		MSG_WriteDeltaEntity( &msg, &nullstate, &svEnt->baseline, qtrue );
	}

	MSG_WriteByte( &msg, svc_EOF );

	MSG_WriteLong( &msg, 7 ); // client num

	// write the checksum feed
	MSG_WriteLong( &msg, sv.checksumFeed );

	// finalize packet
	MSG_WriteByte( &msg, svc_EOF );

	len = PAD( msg.bit, 8 ) / 8;

	// reserve some space for potential userinfo expansion
	len += 512;

	return MAX_MSGLEN - len;
}


/*
================
SV_SendClientGameState

Sends the first message from the server to a connected client.
This will be sent on the initial connection and upon each new map load.

It will be resent if the client acknowledges a later message but has
the wrong gamestate.
================
*/
static void SV_SendClientGameState( client_t *client ) {
	int			start;
	entityState_t nullstate;
	const svEntity_t *svEnt;
	msg_t		msg;
	byte		msgBuffer[ MAX_MSGLEN_BUF ];
	qboolean	csUpdated;

	Com_DPrintf( "SV_SendClientGameState() for %s\n", client->name );

	SV_PrintClientStateChange( client, CS_PRIMED );

	client->state = CS_PRIMED;

	client->downloading = qfalse;

	client->pureAuthentic = qfalse;
	client->gotCP = qfalse;

	// to start generating delta for packet entities
	client->gentity = SV_GentityNum( client - svs.clients );

	// when we receive the first packet from the client, we will
	// notice that it is from a different serverid and that the
	// gamestate message was not just sent, forcing a retransmit
	client->gamestateMessageNum = client->wn_outgoing_sequence;

	// accept usercmds starting from current server time only
	Com_Memset( &client->lastUsercmd, 0x0, sizeof( client->lastUsercmd ) );
	client->lastUsercmd.serverTime = sv.time - 1;

	MSG_Init( &msg, msgBuffer, MAX_MSGLEN );

	// NOTE, MRE: all server->client messages now acknowledge
	// let the client know which reliable clientCommands we have received
	MSG_WriteLong( &msg, client->lastClientCommand );

	// send any server commands waiting to be sent first.
	// we have to do this cause we send the client->reliableSequence
	// with a gamestate and it sets the clc.serverCommandSequence at
	// the client side
	SV_UpdateServerCommandsToClient( client, &msg );

	// send the gamestate
	MSG_WriteByte( &msg, svc_gamestate );
	MSG_WriteLong( &msg, client->reliableSequence );

	// write the configstrings
	csUpdated = qfalse;
	for ( start = 0 ; start < MAX_CONFIGSTRINGS ; start++ ) {
		if ( *sv.configstrings[ start ] != '\0' ) {
			MSG_WriteByte( &msg, svc_configstring );
			MSG_WriteShort( &msg, start );
			if ( start == CS_SYSTEMINFO && sv.pure != sv_pure->integer ) {
				// make sure we send latched sv.pure, not forced cvar value
				char systemInfo[BIG_INFO_STRING];
				Q_strncpyz( systemInfo, sv.configstrings[ start ], sizeof( systemInfo ) );
				Info_SetValueForKey_s( systemInfo, sizeof( systemInfo ), "sv_pure", va( "%i", sv.pure ) );
				MSG_WriteBigString( &msg, systemInfo );
			} else {
				MSG_WriteBigString( &msg, sv.configstrings[start] );
			}
		}
		if ( client->csUpdated[start] ) {
			csUpdated = qtrue;
		}
		client->csUpdated[start] = qfalse;
	}

	if ( client->gamestateAck == GSA_INIT ) {
		// inital submission, accept any messageAcknowledge with matching serverId
		client->gamestateAck = GSA_SENT_ONCE;
	} else {
		if ( client->gamestateAck == GSA_SENT_ONCE && !csUpdated ) {
			// if no configstrings being updated since last submission then assume that we're (re)sending identical gamestate
		} else {
			// expect exact messageAcknowledge
			client->gamestateAck = GSA_SENT_MANY;
		}
	}

	// write the baselines
	Com_Memset( &nullstate, 0, sizeof( nullstate ) );
	for ( start = 0 ; start < MAX_GENTITIES; start++ ) {
		if ( !sv.baselineUsed[ start ] ) {
			continue;
		}
		svEnt = &sv.svEntities[ start ];
		MSG_WriteByte( &msg, svc_baseline );
		MSG_WriteDeltaEntity( &msg, &nullstate, &svEnt->baseline, qtrue );
	}

	MSG_WriteByte( &msg, svc_EOF );

	MSG_WriteLong( &msg, client - svs.clients );

	// write the checksum feed
	MSG_WriteLong( &msg, sv.checksumFeed );

	// it is important to handle gamestate overflow
	// but at this stage client can't process any reliable commands
	// so at least try to inform him in console and release connection slot
	if ( msg.overflowed ) {
		if ( client->netchan.remoteAddress.type == NA_LOOPBACK ) {
			Com_Error( ERR_DROP, "gamestate overflow" );
		} else {
			NET_OutOfBandPrint( NS_SERVER, &client->netchan.remoteAddress, "print\n" S_COLOR_RED "SERVER ERROR: gamestate overflow\n" );
			SV_DropClient( client, "gamestate overflow" );
		}
		return;
	}

	// deliver this to the client
	if ( client->quic_conn != CONN_INVALID && transport ) {
		byte bootbuf[MAX_MSGLEN];
		int bootlen = 0;
		/* WiredNet QUIC client: send typed bootstrap sections on the reliable
		 * bootstrap channel. */
		if ( !SV_WiredNetWriteBootstrap( client, bootbuf, sizeof( bootbuf ), &bootlen ) ) {
			Com_Error( ERR_DROP, "WiredNet bootstrap overflow" );
		}
		transport->send_reliable( client->quic_conn, CHAN_BOOTSTRAP, bootbuf, bootlen );
		return;
	}

	SV_SendMessageToClient( &msg, client );
}


/*
==================
SV_ClientEnterWorld
==================
*/
void SV_ClientEnterWorld( client_t *client ) {
	sharedEntity_t *ent;
	qboolean isBot;
	int clientNum;

	isBot = client->netchan.remoteAddress.type == NA_BOT;

	if ( !isBot ) {
		SV_PrintClientStateChange( client, CS_ACTIVE );
	} else {
		// client->serverId = sv.serverId;
	}

	client->state = CS_ACTIVE;
	client->gamestateAck = GSA_ACKED;

	client->oldServerTime = 0;

	// resend all configstrings using the cs commands since these are
	// no longer sent when the client is CS_PRIMED
	if ( !isBot ) {
		SV_UpdateConfigstrings( client );
	}

	// set up the entity for the client
	clientNum = client - svs.clients;
	ent = SV_GentityNum( clientNum );
	ent->s.number = clientNum;
	client->gentity = ent;

	/* Force delta reset so the first snapshot is a full snapshot.
	 *
	 * QUIC clients: set deltaMessage = 0, not (wn_outgoing_sequence - PACKET_BACKUP+1).
	 * The latter is negative early in a session; cast to uint32 it sets bit 31 of the
	 * wire delta_base field, which collides with the fragment flag (0x80000000).
	 * deltaMessage=0 delta-compresses from frame[0] (zeroed) = full snapshot on wire. */
	if ( client->quic_conn != CONN_INVALID ) {
		client->deltaMessage = 0;
	} else {
		client->deltaMessage = client->wn_outgoing_sequence - (PACKET_BACKUP + 1);
	}
	client->lastSnapshotTime = svs.time - 9999; // generate a snapshot immediately

	// call the game begin function
	VM_Call( gvm, 1, GAME_CLIENT_BEGIN, clientNum );
}


/*
============================================================

CLIENT COMMAND EXECUTION

============================================================
*/

/*
==================
SV_CloseDownload

clear/free any download vars
==================
*/
static void SV_CloseDownload( client_t *cl ) {
	int i;

	// EOF
	if ( cl->download != FS_INVALID_HANDLE ) {
		FS_FCloseFile( cl->download );
		cl->download = FS_INVALID_HANDLE;
	}

	*cl->downloadName = '\0';

	// Free the temporary buffer space
	for (i = 0; i < MAX_DOWNLOAD_WINDOW; i++) {
		if (cl->downloadBlocks[i]) {
			Z_Free( cl->downloadBlocks[i] );
			cl->downloadBlocks[i] = NULL;
		}
	}

}


/*
==================
SV_StopDownload_f

Abort a download if in progress
==================
*/
static void SV_StopDownload_f( client_t *cl ) {
	if (*cl->downloadName)
		Com_DPrintf( "clientDownload: %d : file \"%s\" aborted\n", (int) (cl - svs.clients), cl->downloadName );

	SV_CloseDownload( cl );
}


/*
==================
SV_DoneDownload_f

Downloads are finished
==================
*/
static void SV_DoneDownload_f( client_t *cl ) {
	if ( cl->state == CS_ACTIVE )
		return;

	Com_DPrintf( "clientDownload: %s Done\n", cl->name );

	// resend the game state to update any clients that entered during the download
	SV_SendClientGameState( cl );
}


/*
==================
SV_NextDownload_f

The argument will be the last acknowledged block from the client, it should be
the same as cl->downloadClientBlock
==================
*/
static void SV_NextDownload_f( client_t *cl )
{
	int block = atoi( Cmd_Argv(1) );

	if (block == cl->downloadClientBlock) {
		Com_DPrintf( "clientDownload: %d : client acknowledge of block %d\n", (int) (cl - svs.clients), block );

		// Find out if we are done.  A zero-length block indicates EOF
		if (cl->downloadBlockSize[cl->downloadClientBlock % MAX_DOWNLOAD_WINDOW] == 0) {
			Com_Printf( "clientDownload: %d : file \"%s\" completed\n", (int) (cl - svs.clients), cl->downloadName );
			SV_CloseDownload( cl );
			return;
		}

		cl->downloadSendTime = svs.time;
		cl->downloadClientBlock++;
		return;
	}
	// We aren't getting an acknowledge for the correct block, drop the client
	// FIXME: this is bad... the client will never parse the disconnect message
	//			because the cgame isn't loaded yet
	SV_DropClient( cl, "broken download" );
}


/*
==================
SV_BeginDownload_f
==================
*/
static void SV_BeginDownload_f( client_t *cl ) {
	if ( cl->state == CS_ACTIVE )
		return;

	// Kill any existing download
	SV_CloseDownload( cl );

	// cl->downloadName is non-zero now, SV_WriteDownloadToClient will see this and open
	// the file itself
	Q_strncpyz( cl->downloadName, Cmd_Argv(1), sizeof(cl->downloadName) );

	SV_PrintClientStateChange( cl, CS_CONNECTED );
	cl->state = CS_CONNECTED;
	cl->gentity = NULL;

	cl->downloading = qtrue;

	if ( cl->gamestateAck == GSA_ACKED ) {
		cl->gamestateAck = GSA_SENT_ONCE;
	}
}


/*
==================
SV_WriteDownloadToClient

Check to see if the client wants a file, open it if needed and start pumping the client
Fill up msg with data, return number of download blocks added
==================
*/
static int SV_WriteDownloadToClient( client_t *cl )
{
	int curindex;
	int unreferenced = 1;
	char errorMessage[1024];
	char pakbuf[MAX_QPATH], *pakptr;
	int numRefPaks;
	msg_t msg;
	byte msgBuffer[MAX_DOWNLOAD_BLKSIZE*2+8];

	if ( cl->download == FS_INVALID_HANDLE ) {
		qboolean isProprietary = qfalse;
 		// Chop off filename extension.
		Q_strncpyz( pakbuf, cl->downloadName, sizeof( pakbuf ) );
		pakptr = strrchr( pakbuf, '.' );

		if(pakptr)
		{
			*pakptr = '\0';

			// Check for pk3 filename extension
			if ( !Q_stricmp( pakptr + 1, "pk3" ) )
			{
				// Check whether the file appears in the list of referenced
				// paks to prevent downloading of arbitrary files.
				Cmd_TokenizeStringIgnoreQuotes( sv_referencedPakNames->string );
				numRefPaks = Cmd_Argc();

				for(curindex = 0; curindex < numRefPaks; curindex++)
				{
					if(!FS_FilenameCompare(Cmd_Argv(curindex), pakbuf))
					{
						unreferenced = 0;

						// now that we know the file is referenced,
						// check whether it's legal to download it.
						isProprietary = FS_isProprietary(pakbuf);

						break;
					}
				}
			}
		}

		cl->download = FS_INVALID_HANDLE;

		// We open the file here
		if ( !(sv_allowDownload->integer & DLF_ENABLE) ||
			(sv_allowDownload->integer & DLF_NO_UDP) ||
			isProprietary || unreferenced ||
			( cl->downloadSize = FS_SV_FOpenFileRead( cl->downloadName, &cl->download ) ) < 0 ) {

			// cannot auto-download file
			if(unreferenced)
			{
				Com_Printf("clientDownload: %d : \"%s\" is not referenced and cannot be downloaded.\n", (int) (cl - svs.clients), cl->downloadName);
				Com_sprintf(errorMessage, sizeof(errorMessage), "File \"%s\" is not referenced and cannot be downloaded.", cl->downloadName);
			}
			else if (isProprietary) {
				Com_Printf("clientDownload: %d : \"%s\" cannot download proprietary files\n", (int) (cl - svs.clients), cl->downloadName);
				Com_sprintf(errorMessage, sizeof(errorMessage), "Cannot autodownload proprietary file \"%s\"", cl->downloadName);
			}
			else if ( !(sv_allowDownload->integer & DLF_ENABLE) ||
				(sv_allowDownload->integer & DLF_NO_UDP) ) {

				Com_Printf("clientDownload: %d : \"%s\" download disabled\n", (int) (cl - svs.clients), cl->downloadName);
				if ( sv.pure != 0 ) {
					Com_sprintf(errorMessage, sizeof(errorMessage), "Could not download \"%s\" because autodownloading is disabled on the server.\n\n"
										"You will need to get this file elsewhere before you "
										"can connect to this pure server.\n", cl->downloadName);
				} else {
					Com_sprintf(errorMessage, sizeof(errorMessage), "Could not download \"%s\" because autodownloading is disabled on the server.\n\n"
                    "The server you are connecting to is not a pure server, "
                    "set autodownload to No in your settings and you might be "
                    "able to join the game anyway.\n", cl->downloadName);
				}
			} else {
        // NOTE TTimo this is NOT supposed to happen unless bug in our filesystem scheme?
        //   if the pk3 is referenced, it must have been found somewhere in the filesystem
				Com_Printf("clientDownload: %d : \"%s\" file not found on server\n", (int) (cl - svs.clients), cl->downloadName);
				Com_sprintf(errorMessage, sizeof(errorMessage), "File \"%s\" not found on server for autodownloading.\n", cl->downloadName);
			}

			MSG_Init( &msg, msgBuffer, sizeof( msgBuffer ) - 8 );
			MSG_WriteLong( &msg, cl->lastClientCommand );

			MSG_WriteByte( &msg, svc_download );
			MSG_WriteShort( &msg, 0 ); // client is expecting block zero
			MSG_WriteLong( &msg, -1 ); // illegal file size
			MSG_WriteString( &msg, errorMessage );

			MSG_WriteByte( &msg, svc_EOF );
			if ( cl->quic_conn != CONN_INVALID && transport ) {
				byte dlbuf[1024 + 16];
				int  msglen = 0;
				int  errlen = (int)strlen( errorMessage );
				if ( errlen > 1023 )
					errlen = 1023;
				dlbuf[msglen++] = WN_DOWNLOAD_MSG_ERROR;
				dlbuf[msglen++] = (byte)( errlen & 0xFF );
				dlbuf[msglen++] = (byte)( ( errlen >> 8 ) & 0xFF );
				Com_Memcpy( dlbuf + msglen, errorMessage, (size_t)errlen );
				msglen += errlen;
				transport->send_reliable( cl->quic_conn, CHAN_DOWNLOAD, dlbuf, msglen );
			}

			*cl->downloadName = '\0';

			if ( cl->download != FS_INVALID_HANDLE ) {
				FS_FCloseFile( cl->download );
				cl->download = FS_INVALID_HANDLE;
			}

			return 1;
		}

		Com_Printf( "clientDownload: %d : beginning \"%s\"\n", (int) (cl - svs.clients), cl->downloadName );

		cl->downloadCurrentBlock = cl->downloadClientBlock = cl->downloadXmitBlock = 0;
		cl->downloadCount = 0;
		cl->downloadEOF = qfalse;
	}

	// Perform any reads that we need to
	while (cl->downloadCurrentBlock - cl->downloadClientBlock < MAX_DOWNLOAD_WINDOW &&
		cl->downloadSize != cl->downloadCount) {

		curindex = (cl->downloadCurrentBlock % MAX_DOWNLOAD_WINDOW);

		if (!cl->downloadBlocks[curindex])
			cl->downloadBlocks[curindex] = Z_Malloc( MAX_DOWNLOAD_BLKSIZE );

		cl->downloadBlockSize[curindex] = FS_Read( cl->downloadBlocks[curindex], MAX_DOWNLOAD_BLKSIZE, cl->download );

		if (cl->downloadBlockSize[curindex] <= 0) {
			// EOF right now
			cl->downloadCount = cl->downloadSize;
			break;
		}

		cl->downloadCount += cl->downloadBlockSize[curindex];

		// Load in next block
		cl->downloadCurrentBlock++;
	}

	// Check to see if we have eof condition and add the EOF block
	if (cl->downloadCount == cl->downloadSize &&
		!cl->downloadEOF &&
		cl->downloadCurrentBlock - cl->downloadClientBlock < MAX_DOWNLOAD_WINDOW) {

		cl->downloadBlockSize[cl->downloadCurrentBlock % MAX_DOWNLOAD_WINDOW] = 0;
		cl->downloadCurrentBlock++;

		cl->downloadEOF = qtrue;  // We have added the EOF block
	}

	if (cl->downloadClientBlock == cl->downloadCurrentBlock)
		return 0; // Nothing to transmit

	// Write out the next section of the file, if we have already reached our window,
	// automatically start retransmitting
	if (cl->downloadXmitBlock == cl->downloadCurrentBlock)
	{
		// We have transmitted the complete window, should we start resending?
		if (svs.time - cl->downloadSendTime > 1000)
			cl->downloadXmitBlock = cl->downloadClientBlock;
		else
			return 0;
	}

	// Send current block
	curindex = (cl->downloadXmitBlock % MAX_DOWNLOAD_WINDOW);

	MSG_Init( &msg, msgBuffer, sizeof( msgBuffer ) - 8 );
	MSG_WriteLong( &msg, cl->lastClientCommand );

	MSG_WriteByte( &msg, svc_download );
	MSG_WriteShort( &msg, cl->downloadXmitBlock );

	// block zero is special, contains file size
	if ( cl->downloadXmitBlock == 0 )
		MSG_WriteLong( &msg, cl->downloadSize );

	MSG_WriteShort( &msg, cl->downloadBlockSize[curindex] );

	// Write the block
	if ( cl->downloadBlockSize[curindex] > 0 )
		MSG_WriteData( &msg, cl->downloadBlocks[curindex], cl->downloadBlockSize[curindex] );

	MSG_WriteByte( &msg, svc_EOF );

	if ( cl->quic_conn != CONN_INVALID && transport ) {
		byte dlbuf[MAX_DOWNLOAD_BLKSIZE + 16];
		int  msglen = 0;
		int  blockSize = cl->downloadBlockSize[curindex];
		dlbuf[msglen++] = WN_DOWNLOAD_MSG_BLOCK;
		dlbuf[msglen++] = (byte)( cl->downloadXmitBlock & 0xFF );
		dlbuf[msglen++] = (byte)( ( cl->downloadXmitBlock >> 8 ) & 0xFF );
		dlbuf[msglen++] = (byte)( blockSize & 0xFF );
		dlbuf[msglen++] = (byte)( ( blockSize >> 8 ) & 0xFF );
		if ( cl->downloadXmitBlock == 0 ) {
			dlbuf[msglen++] = (byte)( cl->downloadSize & 0xFF );
			dlbuf[msglen++] = (byte)( ( cl->downloadSize >> 8 ) & 0xFF );
			dlbuf[msglen++] = (byte)( ( cl->downloadSize >> 16 ) & 0xFF );
			dlbuf[msglen++] = (byte)( ( cl->downloadSize >> 24 ) & 0xFF );
		}
		if ( blockSize > 0 ) {
			Com_Memcpy( dlbuf + msglen, cl->downloadBlocks[curindex], (size_t)blockSize );
			msglen += blockSize;
		}
		transport->send_reliable( cl->quic_conn, CHAN_DOWNLOAD, dlbuf, msglen );
	}

	Com_DPrintf( "clientDownload: %d : writing block %d\n", (int) (cl - svs.clients), cl->downloadXmitBlock );

	// Move on to the next block
	// It will get sent with next snap shot.  The rate will keep us in line.
	cl->downloadXmitBlock++;
	cl->downloadSendTime = svs.time;

	return 1;
}


/*
==================
SV_SendQueuedMessages

Send one round of fragments, or queued messages to all clients that have data pending.
Return the shortest time interval for sending next packet to client
==================
*/
int SV_SendQueuedMessages( void )
{
	/* Phase D: QUIC handles fragmentation and flow control internally.
	   No application-level fragment queue exists. */
	return -1;
}


/*
==================
SV_SendDownloadMessages

Send one round of download messages to all clients
==================
*/
int SV_SendDownloadMessages( void )
{
	int i, numDLs = 0;
	client_t *cl;

	for( i = 0; i < sv.maxclients; i++ )
	{
		cl = &svs.clients[ i ];
		if ( cl->state >= CS_CONNECTED && *cl->downloadName )
		{
			numDLs += SV_WriteDownloadToClient( cl );
		}
	}

	return numDLs;
}


/*
=================
SV_Disconnect_f

The client is going to disconnect, so remove the connection immediately  FIXME: move to game?
=================
*/
static void SV_Disconnect_f( client_t *cl ) {
	SV_DropClient( cl, "disconnected" );
}


/*
=================
SV_VerifyPaks_f

If we are pure, disconnect the client if they do no meet the following conditions:

1. the first two checksums match our view of cgame and ui
2. there are no any additional checksums that we do not have

This routine would be a bit simpler with a goto but i abstained

=================
*/
static void SV_VerifyPaks_f( client_t *cl ) {
	int nChkSum1, nClientPaks, i, j, nCurArg;
	int nClientChkSum[512];
	const char *pArg;
	qboolean bGood = qtrue;

	// if we are pure, we "expect" the client to load certain things from
	// certain pk3 files, namely we want the client to have loaded the
	// ui and cgame that we think should be loaded based on the pure setting
	//
	if ( sv.pure != 0 ) {

		nChkSum1 = 0;

		// we run the game, so determine which cgame the client "should" be running
		bGood = FS_FileIsInPAK( "vm/cgame.qvm", &nChkSum1, NULL );
		// bGood &= FS_FileIsInPAK( "vm/ui.qvm", &nChkSum2, NULL );

		nClientPaks = Cmd_Argc();

		if ( nClientPaks > ARRAY_LEN( nClientChkSum ) )
			nClientPaks = ARRAY_LEN( nClientChkSum );

		// start at arg 2 ( skip serverId cl_paks )
		nCurArg = 1;

		pArg = Cmd_Argv(nCurArg++);
		if ( !*pArg ) {
			bGood = qfalse;
		}
		else
		{
			// we may get incoming cp sequences from a previous serverId, which we need to ignore
			if ( atoi( pArg ) != sv.serverId /* || !cl->gamestateAcked */ ) {
				Com_DPrintf( "ignoring outdated cp command from client %s\n", cl->name );
				return;
			}
		}

		// we basically use this while loop to avoid using 'goto' :)
		while (bGood) {

			// must be at least 6: "cl_paks cgame ui @ firstref ... numChecksums"
			// numChecksums is encoded
			if (nClientPaks < 6) {
				bGood = qfalse;
				break;
			}
			// verify first to be the cgame checksum
			pArg = Cmd_Argv(nCurArg++);
			if ( !*pArg || *pArg == '@' || atoi(pArg) != nChkSum1 ) {
				bGood = qfalse;
				break;
			}
			// should be sitting at the delimeter now
			pArg = Cmd_Argv(nCurArg++);
			if (*pArg != '@') {
				bGood = qfalse;
				break;
			}
			// store checksums since tokenization is not re-entrant
			for (i = 0; nCurArg < nClientPaks; i++) {
				nClientChkSum[i] = atoi(Cmd_Argv(nCurArg++));
			}

			// store number to compare against (minus one cause the last is the number of checksums)
			nClientPaks = i - 1;

			// make sure none of the client check sums are the same
			// so the client can't send 5 the same checksums
			for (i = 0; i < nClientPaks; i++) {
				for (j = 0; j < nClientPaks; j++) {
					if (i == j)
						continue;
					if (nClientChkSum[i] == nClientChkSum[j]) {
						bGood = qfalse;
						break;
					}
				}
				if (bGood == qfalse)
					break;
			}
			if (bGood == qfalse)
				break;

			// check if the client has provided any pure checksums of pk3 files not loaded by the server
			for ( i = 0; i < nClientPaks; i++ ) {
				if ( !FS_IsPureChecksum( nClientChkSum[i] ) ) {
					bGood = qfalse;
					break;
				}
			}
			if ( bGood == qfalse ) {
				break;
			}

			// check if the number of checksums was correct
			nChkSum1 = sv.checksumFeed;
			for (i = 0; i < nClientPaks; i++) {
				nChkSum1 ^= nClientChkSum[i];
			}
			nChkSum1 ^= nClientPaks;
			if (nChkSum1 != nClientChkSum[nClientPaks]) {
				bGood = qfalse;
				break;
			}

			// break out
			break;
		}

		cl->gotCP = qtrue;

		if ( bGood ) {
			cl->pureAuthentic = qtrue;
		} else {
			cl->pureAuthentic = qfalse;
			cl->lastSnapshotTime = svs.time - 9999; // generate a snapshot immediately
			cl->state = CS_ZOMBIE; // skip delta generation
			SV_SendClientSnapshot( cl );
			cl->state = CS_ACTIVE;
			SV_DropClient( cl, "Unpure client detected. Invalid .PK3 files referenced!" );
		}
	}
}


/*
=================
SV_ResetPureClient_f
=================
*/
static void SV_ResetPureClient_f( client_t *cl ) {
	cl->pureAuthentic = qfalse;
	cl->gotCP = qfalse;
}


/*
=================
SV_UserinfoChanged

Pull specific info from a newly changed userinfo string
into a more C friendly form.
=================
*/
void SV_UserinfoChanged( client_t *cl, qboolean updateUserinfo, qboolean runFilter ) {
	char buf[ MAX_NAME_LENGTH ];
	const char *val;
	const char *ip;
	int	i;

	if ( cl->netchan.remoteAddress.type == NA_BOT ) {
		cl->lastSnapshotTime = svs.time - 9999; // generate a snapshot immediately
		cl->snapshotMsec = 1000 / sv_fps->integer;
		cl->rate = 0;
		return;
	}

	// rate command

	// if the client is on the same subnet as the server and we aren't running an
	// internet public server, assume they don't need a rate choke
	if ( cl->netchan.remoteAddress.type == NA_LOOPBACK || ( cl->netchan.isLANAddress && com_dedicated->integer != 2 && sv_lanForceRate->integer ) ) {
		cl->rate = 0; // lans should not rate limit
	} else {
		val = Info_ValueForKey( cl->userinfo, "rate" );
		if ( val[0] )
			cl->rate = atoi( val );
		else
			cl->rate = 10000; // was 3000

		if ( sv_maxRate->integer ) {
			if ( cl->rate > sv_maxRate->integer )
				cl->rate = sv_maxRate->integer;
		}

		if ( sv_minRate->integer ) {
			if ( cl->rate < sv_minRate->integer )
				cl->rate = sv_minRate->integer;
		}
	}

	// snaps command
	val = Info_ValueForKey( cl->userinfo, "snaps" );
	if ( val[0] && !NET_IsLocalAddress( &cl->netchan.remoteAddress ) )
		i = atoi( val );
	else
		i = sv_fps->integer; // sync with server

	// range check
	if ( i < 1 )
		i = 1;
	else if ( i > sv_fps->integer )
		i = sv_fps->integer;

	i = 1000 / i; // from FPS to milliseconds

	if ( i != cl->snapshotMsec )
	{
		// Reset last sent snapshot so we avoid desync between server frame time and snapshot send time
		cl->lastSnapshotTime = svs.time - 9999; // generate a snapshot immediately
		cl->snapshotMsec = i;
	}

	if ( !updateUserinfo )
		return;

	// name for C code
	val = Info_ValueForKey( cl->userinfo, "name" );
	// truncate if it is too long as it may cause memory corruption
	if ( gvm->forceDataMask && strlen( val ) >= sizeof( buf ) ) {
		Q_strncpyz( buf, val, sizeof( buf ) );
		Info_SetValueForKey( cl->userinfo, "name", buf );
		val = buf;
	}
	Q_strncpyz( cl->name, val, sizeof( cl->name ) );

	// TTimo
	// maintain the IP information
	// the banning code relies on this being consistently present
	if ( NET_IsLocalAddress( &cl->netchan.remoteAddress ) )
		ip = "localhost";
	else
		ip = NET_AdrToString( &cl->netchan.remoteAddress );

	if ( !Info_SetValueForKey( cl->userinfo, "ip", ip ) )
		SV_DropClient( cl, "userinfo string length exceeded" );

	Info_SetValueForKey( cl->userinfo, "tld", cl->tld );

	if ( runFilter )
	{
		val = SV_RunFilters( cl->userinfo, &cl->netchan.remoteAddress );
		if ( *val != '\0' )
		{
			SV_DropClient( cl, val );
		}
	}
}


/*
==================
SV_UpdateUserinfo_f
==================
*/
static void SV_UpdateUserinfo_f( client_t *cl ) {
	const char *info;

	info = Cmd_Argv( 1 );

	if ( Cmd_Argc() != 2 || *info == '\0' ) {
		// this is something erroneous, client should never send that
		return;
	}

	Q_strncpyz( cl->userinfo, info, sizeof( cl->userinfo ) );

	SV_UserinfoChanged( cl, qtrue, qtrue ); // update userinfo, run filter
	// call prog code to allow overrides
	VM_Call( gvm, 1, GAME_CLIENT_USERINFO_CHANGED, cl - svs.clients );
}

extern int SV_Strlen( const char *str );

/*
==================
SV_PrintLocations_f
==================
*/
void SV_PrintLocations_f( client_t *client ) {
	int i, len;
	client_t *cl;
	int max_namelength;
	int max_ctrylength;
	char line[128];
	char buf[1400-4-8], *s;
	char filln[MAX_NAME_LENGTH];
	char fillc[64];

	if ( !svs.clients )
		return;

	max_namelength = 4; // strlen( "name" )
	max_ctrylength = 7; // strlen( "country" )

	// first pass: save and determine max.lengths of name/address fields
	for ( i = 0, cl = svs.clients; i < sv.maxclients; i++, cl++ )
	{
		if ( cl->state == CS_FREE )
			continue;

		len = SV_Strlen( cl->name );// name length without color sequences
		if ( len > max_namelength )
			max_namelength = len;

		len = strlen( cl->country );
		if ( len > max_ctrylength )
			max_ctrylength = len;
	}

	s = buf; *s = '\0';
	memset( filln, '-',  max_namelength ); filln[max_namelength] = '\0';
	memset( fillc, '-',  max_ctrylength ); fillc[max_ctrylength] = '\0';
	// Start this on a new line to be viewed properly in console
	s = Q_stradd( s, "\n" );
	Com_sprintf( line, sizeof( line ), "ID %-*s CC Country\n", max_namelength, "Name" );
	s = Q_stradd( s, line );
	Com_sprintf( line, sizeof( line ), "-- %s -- %s\n", filln, fillc );
	s = Q_stradd( s, line );

	for ( i = 0, cl = svs.clients; i < sv.maxclients; i++, cl++ )
	{
		if ( cl->state == CS_FREE )
			continue;

		len = Com_sprintf( line, sizeof( line ), "%2i %s%-*s" S_COLOR_WHITE " %2s %s\n",
			i, cl->name, max_namelength-SV_Strlen(cl->name), "", cl->tld, cl->country );

		if ( s - buf + len >= sizeof( buf )-1 ) // flush accumulated buffer
		{
			if ( client )
				NET_OutOfBandPrint( NS_SERVER, &client->netchan.remoteAddress, "print\n%s", buf );
			else
				Com_Printf( "%s", buf );

			s = buf; *s = '\0';
		}

		s = Q_stradd( s, line );
	}

	if ( buf[0] )
	{
		if ( client )
			NET_OutOfBandPrint( NS_SERVER, &client->netchan.remoteAddress, "print\n%s", buf );
		else
			Com_Printf( "%s", buf );
	}
}


typedef struct {
	const char *name;
	void (*func)( client_t *cl );
} ucmd_t;

static const ucmd_t ucmds[] = {
	{"userinfo", SV_UpdateUserinfo_f},
	{"disconnect", SV_Disconnect_f},
	{"cp", SV_VerifyPaks_f},
	{"vdr", SV_ResetPureClient_f},
	{"download", SV_BeginDownload_f},
	{"nextdl", SV_NextDownload_f},
	{"stopdl", SV_StopDownload_f},
	{"donedl", SV_DoneDownload_f},
	{"locations", SV_PrintLocations_f},

	{NULL, NULL}
};


/*
================
SV_FloodProtect
================
*/
static qboolean SV_FloodProtect( client_t *cl ) {
	if ( sv_floodProtect->integer ) {
		return SVC_RateLimit( &cl->cmd_rate, 8, 500 );
	} else {
		return qfalse;
	}
}


/*
==================
SV_ExecuteClientCommand

Also called by bot code
==================
*/
qboolean SV_ExecuteClientCommand( client_t *cl, const char *s ) {
	const ucmd_t *ucmd;
	qboolean bFloodProtect;
	qboolean isBot;

	Cmd_TokenizeString( s );

	// malicious users may try using too many string commands
	// to lag other players.  If we decide that we want to stall
	// the command, we will stop processing the rest of the packet,
	// including the usercmd.  This causes flooders to lag themselves
	// but not other people

	// We don't do this when the client hasn't been active yet since it's
	// normal to spam a lot of commands when downloading
	isBot = cl->netchan.remoteAddress.type == NA_BOT ? qtrue: qfalse;
	bFloodProtect = !isBot && cl->state >= CS_ACTIVE;

	// see if it is a server level command
	for ( ucmd = ucmds; ucmd->name; ucmd++ ) {
		if ( !strcmp( Cmd_Argv(0), ucmd->name ) ) {
			if ( ucmd->func == SV_UpdateUserinfo_f ) {
				if ( bFloodProtect ) {
					if ( SVC_RateLimit( &cl->info_rate, 5, 1000 ) ) {
						return qfalse; // lag flooder
					}
				}
			} else if ( ucmd->func == SV_PrintLocations_f && !sv_clientTLD->integer ) {
				continue; // bypass this command to the gamecode
			}
			ucmd->func( cl );
			bFloodProtect = qfalse;
			break;
		}
	}

	// if ( !isBot && ( !cl->gamestateAcked || sv.serverId != cl->serverId ) ) {
	//		Com_Printf( "%s: ignoring pre map_restart / outdated client command '%s'\n", cl->name, s );
	//	return qtrue;
	// }

#ifndef DEDICATED
	if ( !com_cl_running->integer && bFloodProtect && SV_FloodProtect( cl ) ) {
#else
	if ( bFloodProtect && SV_FloodProtect( cl ) ) {
#endif
		// ignore any other text messages from this client but let them keep playing
		Com_DPrintf( "client text ignored for %s: %s\n", cl->name, Cmd_Argv(0) );
	} else {
		// pass unknown strings to the game
		if ( !ucmd->name && sv.state == SS_GAME && cl->state >= CS_PRIMED ) {
			if ( gvm->forceDataMask )
				Cmd_Args_Sanitize( "\n\r;" ); // handle ';' for OSP
			else
				Cmd_Args_Sanitize( "\n\r" );
			VM_Call( gvm, 1, GAME_CLIENT_COMMAND, cl - svs.clients );
		}
	}

	return qtrue;
}


/*
===============
SV_ClientCommand
===============
*/
static qboolean SV_ClientCommand( client_t *cl, msg_t *msg ) {
	int		seq;
	const char	*s;

	seq = MSG_ReadLong( msg );
	s = MSG_ReadString( msg );

	// see if we have already executed it
	if ( seq - cl->lastClientCommand <= 0 ) {
		return qtrue;
	}

	Com_DPrintf( "clientCommand: %s : %i : %s\n", cl->name, seq, s );

	// drop the connection if we have somehow lost commands
	if ( seq - cl->lastClientCommand > 1 ) {
		Com_Printf( "Client %s lost %i clientCommands\n", cl->name, seq - cl->lastClientCommand - 1 );
		SV_DropClient( cl, "Lost reliable commands" );
		return qfalse;
	}

	if ( !SV_ExecuteClientCommand( cl, s ) ) {
		return qfalse;
	}

	cl->lastClientCommand = seq;
	Q_strncpyz( cl->lastClientCommandString, s, sizeof( cl->lastClientCommandString ) );

	return qtrue; // continue processing
}


//==================================================================================


/*
==================
SV_ClientThink

Also called by bot code
==================
*/
void SV_ClientThink (client_t *cl, usercmd_t *cmd) {
	cl->lastUsercmd = *cmd;

	if ( cl->state != CS_ACTIVE ) {
		return;		// may have been kicked during the last usercmd
	}

	VM_Call( gvm, 1, GAME_CLIENT_THINK, cl - svs.clients );
}


/*
==================
SV_UserMove

The message usually contains all the movement commands
that were in the last three packets, so that the information
in dropped packets can be recovered.

On very fast clients, there may be multiple usercmd packed into
each of the backup packets.
==================
*/
static void SV_UserMove( client_t *cl, msg_t *msg, qboolean delta ) {
	int			i, key;
	int			cmdCount;
	static const usercmd_t nullcmd = { 0 };
	usercmd_t	cmds[MAX_PACKET_USERCMDS], *cmd;
	const usercmd_t *oldcmd;

	if ( delta ) {
		cl->deltaMessage = cl->messageAcknowledge;
	} else {
		cl->deltaMessage = cl->wn_outgoing_sequence - ( PACKET_BACKUP + 1 ); // force delta reset
	}

	cmdCount = MSG_ReadByte( msg );

	if ( cmdCount < 1 ) {
		Com_Printf( "cmdCount < 1\n" );
		return;
	}

	if ( cmdCount > MAX_PACKET_USERCMDS ) {
		Com_Printf( "cmdCount > MAX_PACKET_USERCMDS\n" );
		return;
	}

	// use the checksum feed in the key
	key = sv.checksumFeed;
	// also use the message acknowledge
	key ^= cl->messageAcknowledge;
	// also use the last acknowledged server command in the key
	key ^= MSG_HashKey(cl->reliableCommands[ cl->reliableAcknowledge & (MAX_RELIABLE_COMMANDS-1) ], 32);

	oldcmd = &nullcmd;
	for ( i = 0 ; i < cmdCount ; i++ ) {
		cmd = &cmds[i];
		MSG_ReadDeltaUsercmdKey( msg, key, oldcmd, cmd );
		oldcmd = cmd;
	}

	// save time for ping calculation
	if ( cl->frames[ cl->messageAcknowledge & PACKET_MASK ].messageAcked == 0 ) {
		cl->frames[ cl->messageAcknowledge & PACKET_MASK ].messageAcked = Sys_Milliseconds();
	}

	// if this is the first usercmd we have received
	// this gamestate, put the client into the world
	if ( cl->state == CS_PRIMED ) {
		if ( sv.pure != 0 && !cl->gotCP ) {
			// we didn't get a cp yet, don't assume anything and just send the gamestate all over again
			if ( !SVC_RateLimit( &cl->gamestate_rate, 2, 1000 ) ) {
				Com_DPrintf( "%s: didn't get cp command, resending gamestate\n", cl->name );
				SV_SendClientGameState( cl );
			}
			return;
		}
		SV_ClientEnterWorld( cl );
		// the moves can be processed normally
	}

	// a bad cp command was sent, drop the client
	if ( sv.pure != 0 && !cl->pureAuthentic ) {
		SV_DropClient( cl, "Cannot validate pure client!" );
		return;
	}

	if ( cl->state != CS_ACTIVE ) {
		cl->deltaMessage = cl->wn_outgoing_sequence - ( PACKET_BACKUP + 1 ); // force delta reset
		return;
	}

	// usually, the first couple commands will be duplicates
	// of ones we have previously received, but the servertimes
	// in the commands will cause them to be immediately discarded
	for ( i = 0; i < cmdCount; i++ ) {
		// if this is a cmd from before a map_restart ignore it
		if ( cmds[i].serverTime - cmds[cmdCount-1].serverTime > 0 ) {
			continue;
		}
		// extremely lagged or cmd from before a map_restart
		//if ( cmds[i].serverTime > svs.time + 3000 ) {
		//	continue;
		//}
		// don't execute if this is an old cmd which is already executed
		// these old cmds are included when cl_packetdup > 0
		//if ( cmds[i].serverTime <= cl->lastUsercmd.serverTime ) {
		if ( cmds[i].serverTime - cl->lastUsercmd.serverTime <= 0 ) {
			continue;
		}
		SV_ClientThink( cl, &cmds[ i ] );
	}
}


/*
===========================================================================

USER CMD EXECUTION

===========================================================================
*/

/*
===================
SV_AcknowledgeGamestate
===================
*/
static qboolean SV_AcknowledgeGamestate( client_t *cl, int serverId )
{
	if ( serverId == sv.serverId ) {
		const int messageDelta = cl->messageAcknowledge - cl->gamestateMessageNum;
		// accept either exact message delta or any positive delta with known identical gamestate sent before
		if ( messageDelta == 0 || ( messageDelta > 0 && cl->gamestateAck == GSA_SENT_ONCE ) ) {
			cl->gamestateAck = GSA_ACKED;
			// this client has acknowledged the new gamestate so it's
			// safe to start sending it the real time again
			Com_DPrintf( "%s acknowledged gamestate\n", cl->name );
			cl->oldServerTime = 0;
			return qtrue;
		}
	}
	return qfalse;
}


/*
===================
SV_ExecuteClientMessage

Parse a client packet
===================
*/
void SV_ExecuteClientMessage( client_t *cl, msg_t *msg ) {
	int	c;
	int	serverId;
	int reliableAcknowledge;

	MSG_Bitstream( msg );

	serverId = MSG_ReadLong( msg );

	cl->messageAcknowledge = MSG_ReadLong( msg );

	//if ( cl->messageAcknowledge < 0 ) {
	if ( cl->wn_outgoing_sequence - cl->messageAcknowledge <= 0 ) {
		// usually only hackers create messages like this
		// it is more annoying for them to let them hanging
#ifdef _DEBUG
		SV_DropClient( cl, "DEBUG: illegible client message" );
#endif
		return;
	}

	reliableAcknowledge = MSG_ReadLong( msg );

	if ( cl->reliableSequence - reliableAcknowledge < 0 ) {
#ifdef _DEBUG
		SV_DropClient( cl, "DEBUG: illegible client message" );
#endif
		return;
	}

	// NOTE: when the client message is fux0red the acknowledgement numbers
	// can be out of range, this could cause the server to send thousands of server
	// commands which the server thinks are not yet acknowledged in SV_UpdateServerCommandsToClient
	if ( cl->reliableSequence - reliableAcknowledge > MAX_RELIABLE_COMMANDS ) {
		// usually only hackers create messages like this
		// it is more annoying for them to let them hanging
#ifdef _DEBUG
		SV_DropClient( cl, "DEBUG: illegible client message" );
#else
		Com_Printf( S_COLOR_YELLOW "WARNING: dropping %i commands from %s\n", cl->reliableSequence - cl->reliableAcknowledge, cl->name );
#endif
		cl->reliableAcknowledge = cl->reliableSequence;
		return;
	}

	cl->reliableAcknowledge = reliableAcknowledge;

	cl->justConnected = qfalse;

	// cl->serverId = serverId;

	// if this is a usercmd from a previous gamestate,
	// ignore it or retransmit the current gamestate
	//
	// if the client was downloading, let it stay at whatever serverId and
	// gamestate it was at.  This allows it to keep downloading even when
	// the gamestate changes.  After the download is finished, we'll
	// notice and send it a new game state
	//
	if ( cl->state == CS_CONNECTED ) {
		if ( !cl->downloading ) {
			// send initial gamestate, client may not acknowledge it in next command but start downloading after SV_ClientCommand()
			if ( cl->netchan.remoteAddress.type == NA_LOOPBACK || !SVC_RateLimit( &cl->gamestate_rate, 1, 1000 ) ) {
				SV_SendClientGameState( cl );
			}
			return;
		}
	} else if ( cl->gamestateAck != GSA_ACKED ) {
		// early check for gamestate acknowledge
		SV_AcknowledgeGamestate( cl, serverId );
	}
	// else if ( cl->state == CS_PRIMED ) {
		// in case of download intention client replies with (messageAcknowledge - gamestateMessageNum) >= 0 and (serverId == sv.serverId), sv.serverId can drift away later
		// in case of lost gamestate client replies with (messageAcknowledge - gamestateMessageNum) > 0 and (serverId == sv.serverId)
		// in case of disconnect/etc. client replies with any serverId
	//}

	// read optional clientCommand strings
	do {
		c = MSG_ReadByte( msg );
		if ( c != clc_clientCommand ) {
			break;
		}
		if ( !SV_ClientCommand( cl, msg ) ) {
			return;	// we couldn't execute it because of the flood protection
		}
		if ( cl->state == CS_ZOMBIE ) {
			return;	// disconnect command
		}
	} while ( 1 );

	if ( cl->gamestateAck != GSA_ACKED ) {
		// late check for gamestate acknowledge & resend
		if ( cl->state == CS_PRIMED ) {
			if ( !SV_AcknowledgeGamestate( cl, serverId ) ) {
				Com_DPrintf( "%s: dropped gamestate, resending\n", cl->name );
				if ( !SVC_RateLimit( &cl->gamestate_rate, 1, 1000 ) ) {
					SV_SendClientGameState( cl );
				}
				return; // message delta or serverId mismatch
			}
		} else {
			return; // cl->state <= CS_CONNECTED
		}
	}

	// read the usercmd_t
	if ( c == clc_move ) {
		SV_UserMove( cl, msg, qtrue );
	} else if ( c == clc_moveNoDelta ) {
		SV_UserMove( cl, msg, qfalse );
	} else if ( c != clc_EOF ) {
		Com_Printf( "WARNING: bad command byte %i for client %i\n", c, (int) (cl - svs.clients) );
	}
//	if ( msg->readcount != msg->cursize ) {
//		Com_Printf( "WARNING: Junk at end of packet for client %i\n", cl - svs.clients );
//	}
}
