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

#include "server.h"
#include "../qcommon/cm_local.h"
#include "../game/bg_public.h"

static int s_sv_reload_mod = -1;

void SV_SyncReloadTracker( void ) {
	if ( sv_maxclients && sv_gametype && sv_pure ) {
		s_sv_reload_mod = sv_maxclients->modificationCount
		                + sv_gametype->modificationCount
		                + sv_pure->modificationCount;
	}
}

/*
===============================================================================

OPERATOR CONSOLE ONLY COMMANDS

These commands can only be entered from stdin or by a remote operator datagram
===============================================================================
*/


/*
==================
SV_GetPlayerByHandle

Returns the player with player id or name from Cmd_Argv(1)
==================
*/
client_t *SV_GetPlayerByHandle( void ) {
	// make sure server is running
	if ( !com_sv_running->integer ) {
		return NULL;
	}

	if ( Cmd_Argc() < 2 ) {
		Com_Log( SEV_INFO, LOG_CAT_SERVER, "No player specified.\n" );
		return NULL;
	}

	const char *s = Cmd_Argv(1);

	// Check whether this is a numeric player handle
	int i;
	for(i = 0; s[i] >= '0' && s[i] <= '9'; i++);

	if(!s[i])
	{
		int plid = atoi(s);

		// Check for numeric playerid match
		if(plid >= 0 && plid < sv.maxclients)
		{
			client_t *cl = &svs.clients[plid];

			if (cl->state >= CS_CONNECTED)
				return cl;
		}
	}

	// check for a name match
	char cleanName[ MAX_NAME_LENGTH ];
	client_t *cl = svs.clients;
	for ( i = 0; i < sv.maxclients; i++, cl++ ) {
		if ( cl->state < CS_CONNECTED ) {
			continue;
		}
		if ( !Q_stricmp( cl->name, s ) ) {
			return cl;
		}

		Q_strncpyz( cleanName, cl->name, sizeof(cleanName) );
		Q_CleanStr( cleanName );
		if ( !Q_stricmp( cleanName, s ) ) {
			return cl;
		}
	}

	Com_Log( SEV_INFO, LOG_CAT_SERVER, "Player %s is not on the server\n", s );

	return NULL;
}


/*
==================
SV_GetPlayerByNum

Returns the player with idnum from Cmd_Argv(1)
==================
*/
static client_t *SV_GetPlayerByNum( void ) {
	// make sure server is running
	if ( !com_sv_running->integer ) {
		return NULL;
	}

	if ( Cmd_Argc() < 2 ) {
		Com_Log( SEV_INFO, LOG_CAT_SERVER, "No player specified.\n" );
		return NULL;
	}

	const char *s = Cmd_Argv(1);

	for (int i = 0; s[i]; i++) {
		if (s[i] < '0' || s[i] > '9') {
			Com_Log( SEV_INFO, LOG_CAT_SERVER, "Bad slot number: %s\n", s);
			return NULL;
		}
	}
	int idnum = atoi( s );
	if ( idnum < 0 || idnum >= sv.maxclients ) {
		Com_Log( SEV_INFO, LOG_CAT_SERVER, "Bad client slot: %i\n", idnum );
		return NULL;
	}

	client_t *cl = &svs.clients[idnum];
	if ( cl->state < CS_CONNECTED ) {
		Com_Log( SEV_INFO, LOG_CAT_SERVER, "Client %i is not active\n", idnum );
		return NULL;
	}
	return cl;
}

//=========================================================


/*
==================
SV_Map_f

Restart the server on a different map
==================
*/
static void SV_Map_f( void ) {
	const char *map = Cmd_Argv(1);
	if ( !map || !*map ) {
		return;
	}

	// make sure the level exists before trying to change, so that
	// a typo at the server console won't end the game
	char expanded[MAX_QPATH];
	Com_sprintf( expanded, sizeof( expanded ), "maps/%s.bsp", map );
	// bypass pure check so we can open downloaded map
	FS_BypassPure();
	int len = FS_FOpenFileRead( expanded, NULL, qfalse );
	FS_RestorePure();
	if ( len == -1 ) {
		Com_Log( SEV_INFO, LOG_CAT_SERVER, "Can't find map %s\n", expanded );
		return;
	}

	// force latched values to get set
	Cvar_Get ("g_gametype", "0", CVAR_SERVERINFO | CVAR_USERINFO | CVAR_LATCH );

	// save the map name here cause on a map restart we reload the config.cfg
	// and thus nuke the arguments of the map command
	char mapname[MAX_QPATH];
	Q_strncpyz(mapname, map, sizeof(mapname));

	// start up the map
	// FIXME(@eser) second argument "killBots" should be enabled in single player
	SV_SpawnServer( mapname, qfalse );
}


/*
================
SV_MapRestart_f

Completely restarts a level, but doesn't send a new gamestate to the clients.
This allows fair starts with variable load times.
================
*/
static void SV_MapRestart_f( void ) {
	// make sure we aren't restarting twice in the same frame
	if ( com_frameTime == sv.restartedServerId ) {
		return;
	}

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Log( SEV_INFO, LOG_CAT_SERVER, "Server is not running.\n" );
		return;
	}

	if ( sv.restartTime != 0 ) {
		return;
	}

	int delay = (Cmd_Argc() > 1) ? atoi( Cmd_Argv(1) ) : 5;

	if ( delay != 0 && Cvar_VariableIntegerValue( "g_minPlayers" ) == 0 ) {
		sv.restartTime = sv.time + delay * 1000;
		if ( sv.restartTime == 0 ) {
			sv.restartTime = 1;
		}
		SV_SetConfigstring( CS_WARMUP, va( "%i", sv.restartTime ) );
		return;
	}

	// check for changes in variables that can't just be restarted
	// check for maxclients change
	{
		int cur = sv_maxclients->modificationCount + sv_gametype->modificationCount + sv_pure->modificationCount;
		if ( s_sv_reload_mod == -1 ) {
			s_sv_reload_mod = cur; // first call: initialize without triggering
		} else if ( cur != s_sv_reload_mod ) {
			char mapname[MAX_QPATH];
			s_sv_reload_mod = cur;
			Com_Log( SEV_INFO, LOG_CAT_SERVER, "variable change -- restarting.\n" );
			Q_strncpyz( mapname, Cvar_VariableString( "mapname" ), sizeof( mapname ) );
			SV_SpawnServer( mapname, qfalse );
			return;
		}
	}

	// toggle the server bit so clients can detect that a
	// map_restart has happened
	svs.snapFlagServerBit ^= SNAPFLAG_SERVERCOUNT;

	// generate a new restartedServerid
	sv.restartedServerId = com_frameTime;

	// if a map_restart occurs while a client is changing maps, we need
	// to give them the correct time so that when they finish loading
	// they don't violate the backwards time check in cl_cgame.c
	int i;
	for ( i = 0; i < sv.maxclients; i++ ) {
		if ( svs.clients[i].state == CS_PRIMED ) {
			svs.clients[i].oldServerTime = sv.restartTime;
		}
	}

	// reset all the vm data in place without changing memory allocation
	// note that we do NOT set sv.state = SS_LOADING, so configstrings that
	// had been changed from their default values will generate broadcast updates
	sv.state = SS_LOADING;
	sv.restarting = qtrue;

	// make sure that level time is not zero
	//sv.time = sv.time ? sv.time : 8;

	SV_RestartGameProgs();

	// run a few frames to allow everything to settle
	for ( int i = 0; i < 3; i++ )
	{
		Cbuf_Wait();
		sv.time += 100;
		VM_Call( gvm, 1, GAME_RUN_FRAME, sv.time );
	}

	sv.state = SS_GAME;
	sv.restarting = qfalse;

	// connect and begin all the clients
	for ( i = 0; i < sv.maxclients; i++ ) {
		client_t *client = &svs.clients[i];

		// send the new gamestate to all connected clients
		if ( client->state < CS_CONNECTED ) {
			continue;
		}

		qboolean isBot = (client->netchan.remoteAddress.type == NA_BOT) ? qtrue : qfalse;

		// add the map_restart command
		SV_AddServerCommand( client, "map_restart\n" );

		// connect the client again, without the firstTime flag
		const char *denied = GVM_ArgPtr( VM_Call( gvm, 3, GAME_CLIENT_CONNECT, i, qfalse, isBot ) );
		if ( denied ) {
			// this generally shouldn't happen, because the client
			// was connected before the level change
			SV_DropClient( client, denied );
			Com_Log( SEV_INFO, LOG_CAT_SERVER, "SV_MapRestart_f(%d): dropped client %i - denied!\n", delay, i );
			continue;
		}

		if ( client->state == CS_ACTIVE ) {
			SV_ClientEnterWorld( client );
		}
	}

	// run another frame to allow things to look at all the players
	Cbuf_Wait();
	sv.time += 100;
	VM_Call( gvm, 1, GAME_RUN_FRAME, sv.time );
	svs.time += 100;

	for ( i = 0; i < sv.maxclients; i++ ) {
		client_t *client = &svs.clients[i];
		if ( client->state >= CS_PRIMED ) {
			// accept usercmds starting from current server time only
			// to emulate original behavior which dropped pre-restart commands via serverid check
			memset( &client->lastUsercmd, 0x0, sizeof( client->lastUsercmd ) );
			client->lastUsercmd.serverTime = sv.time - 1;
		}
	}
}


/*
==================
SV_Kick_f

Kick a user off of the server  FIXME: move to game
==================
*/
static void SV_Kick_f( void ) {
	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Log( SEV_INFO, LOG_CAT_SERVER, "Server is not running.\n" );
		return;
	}

	if ( Cmd_Argc() != 2 ) {
		Com_Log( SEV_INFO, LOG_CAT_SERVER, "Usage: kick <player name>\nkick all = kick everyone\nkick allbots = kick all bots\n");
		return;
	}

	client_t *cl = SV_GetPlayerByHandle();
	if ( !cl ) {
		if ( !Q_stricmp( Cmd_Argv( 1 ), "all" ) ) {
			client_t *iter = svs.clients;
			for ( int i = 0; i < sv.maxclients; i++, iter++ ) {
				if ( iter->state < CS_CONNECTED ) {
					continue;
				}
				if ( iter->netchan.remoteAddress.type == NA_LOOPBACK ) {
					continue;
				}
				SV_DropClient( iter, "was kicked" );
				iter->lastPacketTime = svs.time;	// in case there is a funny zombie
			}
		}
		else if ( !Q_stricmp( Cmd_Argv( 1 ), "allbots" ) ) {
			client_t *iter = svs.clients;
			for ( int i = 0; i < sv.maxclients; i++, iter++ ) {
				if ( iter->state < CS_CONNECTED ) {
					continue;
				}
				if ( iter->netchan.remoteAddress.type != NA_BOT ) {
					continue;
				}
				SV_DropClient( iter, "was kicked" );
				iter->lastPacketTime = svs.time;	// in case there is a funny zombie
			}
		}
		return;
	}
	if ( cl->netchan.remoteAddress.type == NA_LOOPBACK ) {
		Com_Log( SEV_INFO, LOG_CAT_SERVER, "Cannot kick host player\n" );
		return;
	}

	SV_DropClient( cl, "was kicked" );
	cl->lastPacketTime = svs.time;	// in case there is a funny zombie
}

/*
==================
SV_KickBots_f

Kick all bots off of the server
==================
*/
static void SV_KickBots_f( void ) {
	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Log( SEV_INFO, LOG_CAT_SERVER, "Server is not running.\n");
		return;
	}

	client_t *cl = svs.clients;
	for( int i = 0; i < sv.maxclients; i++, cl++ ) {
		if ( cl->state < CS_CONNECTED ) {
			continue;
		}

		if ( cl->netchan.remoteAddress.type != NA_BOT ) {
			continue;
		}

		SV_DropClient( cl, "was kicked" );
		cl->lastPacketTime = svs.time; // in case there is a funny zombie
	}
}
/*
==================
SV_KickAll_f

Kick all users off of the server
==================
*/
static void SV_KickAll_f( void ) {
	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Log( SEV_INFO, LOG_CAT_SERVER, "Server is not running.\n" );
		return;
	}

	client_t *cl = svs.clients;
	for( int i = 0; i < sv.maxclients; i++, cl++ ) {
		if ( cl->state < CS_CONNECTED ) {
			continue;
		}

		if ( cl->netchan.remoteAddress.type == NA_LOOPBACK ) {
			continue;
		}

		SV_DropClient( cl, "was kicked" );
		cl->lastPacketTime = svs.time; // in case there is a funny zombie
	}
}

/*
==================
SV_KickNum_f

Kick a user off of the server
==================
*/
static void SV_KickNum_f( void ) {
	client_t	*cl;

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Log( SEV_INFO, LOG_CAT_SERVER, "Server is not running.\n" );
		return;
	}

	if ( Cmd_Argc() != 2 ) {
		Com_Log( SEV_INFO, LOG_CAT_SERVER, "Usage: %s <client number>\n", Cmd_Argv(0));
		return;
	}

	cl = SV_GetPlayerByNum();
	if ( !cl ) {
		return;
	}
	if ( cl->netchan.remoteAddress.type == NA_LOOPBACK ) {
		Com_Log( SEV_INFO, LOG_CAT_SERVER, "Cannot kick host player\n");
		return;
	}

	SV_DropClient( cl, "was kicked" );
	cl->lastPacketTime = svs.time;	// in case there is a funny zombie
}

#ifndef STANDALONE
// these functions require the auth server which of course is not available anymore for stand-alone games.

#ifdef USE_BANS
/*
==================
SV_Ban_f

Ban a user from being able to play on this server through the auth
server
==================
*/
static void SV_Ban_f( void ) {
	client_t	*cl;

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Log( SEV_INFO, LOG_CAT_SERVER, "Server is not running.\n" );
		return;
	}

	if ( Cmd_Argc() != 2 ) {
		Com_Log( SEV_INFO, LOG_CAT_SERVER, "Usage: banUser <player name>\n");
		return;
	}

	cl = SV_GetPlayerByHandle();

	if (!cl) {
		return;
	}

	if( cl->netchan.remoteAddress.type == NA_LOOPBACK ) {
		Com_Log( SEV_INFO, LOG_CAT_SERVER, "Cannot kick host player\n");
		return;
	}

	// Phase 6.4: id authorize server is gone — these legacy banUser/banClient
	// commands are dead. Use SV_AddBanToList()/SV_RehashBans_f() (also gated
	// behind USE_BANS) for local file-based banning instead.
	Com_Log( SEV_INFO, LOG_CAT_SERVER, "banUser is deprecated; use SV_RehashBans/SV_BanAddr instead\n" );
}

/*
==================
SV_BanNum_f

Ban a user from being able to play on this server through the auth
server
==================
*/
static void SV_BanNum_f( void ) {
	client_t	*cl;

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Log( SEV_INFO, LOG_CAT_SERVER, "Server is not running.\n" );
		return;
	}

	if ( Cmd_Argc() != 2 ) {
		Com_Log( SEV_INFO, LOG_CAT_SERVER, "Usage: banClient <client number>\n");
		return;
	}

	cl = SV_GetPlayerByNum();
	if ( !cl ) {
		return;
	}
	if( cl->netchan.remoteAddress.type == NA_LOOPBACK ) {
		Com_Log( SEV_INFO, LOG_CAT_SERVER, "Cannot kick host player\n");
		return;
	}

	// Phase 6.4: id authorize server is gone — see SV_Ban_f for details.
	Com_Log( SEV_INFO, LOG_CAT_SERVER, "banClient is deprecated; use SV_RehashBans/SV_BanAddr instead\n" );
}

#endif // USE_BANS
#endif // !COM_STANDALONE

#ifdef USE_BANS
/*
==================
SV_RehashBans_f

Load saved bans from file.
==================
*/
static void SV_RehashBans_f(void)
{
	// make sure server is running
	if ( !com_sv_running->integer ) {
		return;
	}

	serverBansCount = 0;

	if(!sv_banFile->string || !*sv_banFile->string)
		return;

	char filepath[MAX_QPATH];
	Com_sprintf(filepath, sizeof(filepath), "%s/%s", FS_GetCurrentGameDir(), sv_banFile->string);

	fileHandle_t readfrom;
	int filelen = FS_SV_FOpenFileRead(filepath, &readfrom);
	if(filelen >= 0)
	{
		if(filelen < 2)
		{
			// Don't bother if file is too short.
			FS_FCloseFile(readfrom);
			return;
		}

		char *textbuf = Z_Malloc(filelen);
		char *curpos = textbuf;

		int res = FS_Read(textbuf, filelen, readfrom);
		FS_FCloseFile(readfrom);

		if (res != filelen) {
			Z_Free(textbuf);
			return;
		}

		const char *endpos = textbuf + filelen;

		int index;
		for(index = 0; index < SERVER_MAXBANS && curpos + 2 < endpos; index++)
		{
			// find the end of the address string
			char *maskpos;
			for(maskpos = curpos + 2; maskpos < endpos && *maskpos != ' '; maskpos++);

			if(maskpos + 1 >= endpos)
				break;

			*maskpos = '\0';
			maskpos++;

			// find the end of the subnet specifier
			char *newlinepos;
			for(newlinepos = maskpos; newlinepos < endpos && *newlinepos != '\n'; newlinepos++);

			if(newlinepos >= endpos)
				break;

			*newlinepos = '\0';

			if(NET_StringToAdr(curpos + 2, &serverBans[index].ip, NA_UNSPEC))
			{
				serverBans[index].isexception = (curpos[0] != '0');
				serverBans[index].subnet = atoi(maskpos);

				if(serverBans[index].ip.type == NA_IP &&
				   (serverBans[index].subnet < 1 || serverBans[index].subnet > 32))
				{
					serverBans[index].subnet = 32;
				}
				else if(serverBans[index].ip.type == NA_IP6 &&
					(serverBans[index].subnet < 1 || serverBans[index].subnet > 128))
				{
					serverBans[index].subnet = 128;
				}
			}

			curpos = newlinepos + 1;
		}

		serverBansCount = index;

		Z_Free(textbuf);
	}
}

/*
==================
SV_WriteBans

Save bans to file.
==================
*/
static void SV_WriteBans(void)
{
	if(!sv_banFile->string || !*sv_banFile->string)
		return;

	char filepath[MAX_QPATH];
	Com_sprintf(filepath, sizeof(filepath), "%s/%s", FS_GetCurrentGameDir(), sv_banFile->string);

	fileHandle_t writeto = FS_SV_FOpenFileWrite(filepath);
	if(writeto)
	{
		char writebuf[128];
		serverBan_t *curban;

		for(int index = 0; index < serverBansCount; index++)
		{
			curban = &serverBans[index];

			Com_sprintf(writebuf, sizeof(writebuf), "%d %s %d\n",
				    curban->isexception, NET_AdrToString(&curban->ip), curban->subnet);
			FS_Write(writebuf, strlen(writebuf), writeto);
		}

		FS_FCloseFile(writeto);
	}
}

/*
==================
SV_DelBanEntryFromList

Remove a ban or an exception from the list.
==================
*/

static qboolean SV_DelBanEntryFromList(int index)
{
	if(index == serverBansCount - 1)
		serverBansCount--;
	else if(index < ARRAY_LEN(serverBans) - 1)
	{
		memmove(serverBans + index, serverBans + index + 1, (serverBansCount - index - 1) * sizeof(*serverBans));
		serverBansCount--;
	}
	else
		return qtrue;

	return qfalse;
}

/*
==================
SV_ParseCIDRNotation

Parse a CIDR notation type string and return a netadr_t and suffix by reference
==================
*/

static qboolean SV_ParseCIDRNotation(netadr_t *dest, int *mask, const char *adrstr)
{
	char *suffix;

	suffix = strchr(adrstr, '/');
	if(suffix)
	{
		*suffix = '\0';
		suffix++;
	}

	if(!NET_StringToAdr(adrstr, dest, NA_UNSPEC))
		return qtrue;

	if(suffix)
	{
		*mask = atoi(suffix);

		if(dest->type == NA_IP)
		{
			if(*mask < 1 || *mask > 32)
				*mask = 32;
		}
		else
		{
			if(*mask < 1 || *mask > 128)
				*mask = 128;
		}
	}
	else if(dest->type == NA_IP)
		*mask = 32;
	else
		*mask = 128;

	return qfalse;
}

/*
==================
SV_AddBanToList

Ban a user from being able to play on this server based on his ip address.
==================
*/

static void SV_AddBanToList(qboolean isexception)
{
	char addy2[NET_ADDRSTRMAXLEN];
	netadr_t ip;
	int mask;
	serverBan_t *curban;

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Log( SEV_INFO, LOG_CAT_SERVER, "Server is not running.\n" );
		return;
	}

	int argc = Cmd_Argc();

	if(argc < 2 || argc > 3)
	{
		Com_Log( SEV_INFO, LOG_CAT_SERVER, "Usage: %s (ip[/subnet] | clientnum [subnet])\n", Cmd_Argv(0));
		return;
	}

	if(serverBansCount >= ARRAY_LEN(serverBans))
	{
		Com_Log( SEV_INFO, LOG_CAT_SERVER, "Error: Maximum number of bans/exceptions exceeded.\n");
		return;
	}

	const char *banstring = Cmd_Argv(1);

	if(strchr(banstring, '.') || strchr(banstring, ':'))
	{
		// This is an ip address, not a client num.

		if(SV_ParseCIDRNotation(&ip, &mask, banstring))
		{
			Com_Log( SEV_INFO, LOG_CAT_SERVER, "Error: Invalid address %s\n", banstring);
			return;
		}
	}
	else
	{
		client_t *cl;

		// client num.

		cl = SV_GetPlayerByNum();

		if(!cl)
		{
			Com_Log( SEV_INFO, LOG_CAT_SERVER, "Error: Playernum %s does not exist.\n", Cmd_Argv(1));
			return;
		}

		ip = cl->netchan.remoteAddress;

		if(argc == 3)
		{
			mask = atoi(Cmd_Argv(2));

			if(ip.type == NA_IP)
			{
				if(mask < 1 || mask > 32)
					mask = 32;
			}
			else
			{
				if(mask < 1 || mask > 128)
					mask = 128;
			}
		}
		else
			mask = (ip.type == NA_IP6) ? 128 : 32;
	}

	if(ip.type != NA_IP && ip.type != NA_IP6)
	{
		Com_Log( SEV_INFO, LOG_CAT_SERVER, "Error: Can ban players connected via the internet only.\n");
		return;
	}

	// first check whether a conflicting ban exists that would supersede the new one.
	for(int index = 0; index < serverBansCount; index++)
	{
		curban = &serverBans[index];

		if(curban->subnet <= mask)
		{
			if((curban->isexception || !isexception) && NET_CompareBaseAdrMask(&curban->ip, ip, &curban->subnet))
			{
				Q_strncpyz(addy2, NET_AdrToString(&ip), sizeof(addy2));

				Com_Log( SEV_INFO, LOG_CAT_SERVER, "Error: %s %s/%d supersedes %s %s/%d\n", curban->isexception ? "Exception" : "Ban",
					   NET_AdrToString(&curban->ip), curban->subnet,
					   isexception ? "exception" : "ban", addy2, mask);
				return;
			}
		}
		if(curban->subnet >= mask)
		{
			if(!curban->isexception && isexception && NET_CompareBaseAdrMask(&curban->ip, &ip, mask))
			{
				Q_strncpyz(addy2, NET_AdrToString(&curban->ip), sizeof(addy2));

				Com_Log( SEV_INFO, LOG_CAT_SERVER, "Error: %s %s/%d supersedes already existing %s %s/%d\n", isexception ? "Exception" : "Ban",
					   NET_AdrToString(&ip), mask,
					   curban->isexception ? "exception" : "ban", addy2, curban->subnet);
				return;
			}
		}
	}

	// now delete bans that are superseded by the new one
	int index = 0;
	while(index < serverBansCount)
	{
		curban = &serverBans[index];

		if(curban->subnet > mask && (!curban->isexception || isexception) && NET_CompareBaseAdrMask(&curban->ip, &ip, mask))
			SV_DelBanEntryFromList(index);
		else
			index++;
	}

	serverBans[serverBansCount].ip = ip;
	serverBans[serverBansCount].subnet = mask;
	serverBans[serverBansCount].isexception = isexception;

	serverBansCount++;

	SV_WriteBans();

	Com_Log( SEV_INFO, LOG_CAT_SERVER, "Added %s: %s/%d\n", isexception ? "ban exception" : "ban",
		   NET_AdrToString(&ip), mask);
}

/*
==================
SV_DelBanFromList

Remove a ban or an exception from the list.
==================
*/

static void SV_DelBanFromList(qboolean isexception)
{
	int count = 0, mask;
	netadr_t ip;

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Log( SEV_INFO, LOG_CAT_SERVER, "Server is not running.\n" );
		return;
	}

	if(Cmd_Argc() != 2)
	{
		Com_Log( SEV_INFO, LOG_CAT_SERVER, "Usage: %s (ip[/subnet] | num)\n", Cmd_Argv(0));
		return;
	}

	const char *banstring = Cmd_Argv(1);

	if(strchr(banstring, '.') || strchr(banstring, ':'))
	{
		serverBan_t *curban;

		if(SV_ParseCIDRNotation(&ip, &mask, banstring))
		{
			Com_Log( SEV_INFO, LOG_CAT_SERVER, "Error: Invalid address %s\n", banstring);
			return;
		}

		int index = 0;

		while(index < serverBansCount)
		{
			curban = &serverBans[index];

			if(curban->isexception == isexception		&&
			   curban->subnet >= mask 			&&
			   NET_CompareBaseAdrMask(&curban->ip, &ip, mask))
			{
				Com_Log( SEV_INFO, LOG_CAT_SERVER, "Deleting %s %s/%d\n",
					   isexception ? "exception" : "ban",
					   NET_AdrToString(&curban->ip), curban->subnet);

				SV_DelBanEntryFromList(index);
			}
			else
				index++;
		}
	}
	else
	{
		int todel = atoi(Cmd_Argv(1));

		if(todel < 1 || todel > serverBansCount)
		{
			Com_Log( SEV_INFO, LOG_CAT_SERVER, "Error: Invalid ban number given\n");
			return;
		}

		for(int index = 0; index < serverBansCount; index++)
		{
			if(serverBans[index].isexception == isexception)
			{
				count++;

				if(count == todel)
				{
					Com_Log( SEV_INFO, LOG_CAT_SERVER, "Deleting %s %s/%d\n",
					   isexception ? "exception" : "ban",
					   NET_AdrToString(&serverBans[index].ip), serverBans[index].subnet);

					SV_DelBanEntryFromList(index);

					break;
				}
			}
		}
	}

	SV_WriteBans();
}


/*
==================
SV_ListBans_f

List all bans and exceptions on console
==================
*/

static void SV_ListBans_f(void)
{
	int count;

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Log( SEV_INFO, LOG_CAT_SERVER, "Server is not running.\n" );
		return;
	}

	// List all bans
	for(int index = count = 0; index < serverBansCount; index++)
	{
		serverBan_t *ban = &serverBans[index];
		if(!ban->isexception)
		{
			count++;

			Com_Log( SEV_INFO, LOG_CAT_SERVER, "Ban #%d: %s/%d\n", count,
				    NET_AdrToString(&ban->ip), ban->subnet);
		}
	}
	// List all exceptions
	for(int index = count = 0; index < serverBansCount; index++)
	{
		serverBan_t *ban = &serverBans[index];
		if(ban->isexception)
		{
			count++;

			Com_Log( SEV_INFO, LOG_CAT_SERVER, "Except #%d: %s/%d\n", count,
				    NET_AdrToString(&ban->ip), ban->subnet);
		}
	}
}

/*
==================
SV_FlushBans_f

Delete all bans and exceptions.
==================
*/

static void SV_FlushBans_f(void)
{
	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Log( SEV_INFO, LOG_CAT_SERVER, "Server is not running.\n" );
		return;
	}

	serverBansCount = 0;

	// empty the ban file.
	SV_WriteBans();

	Com_Log( SEV_INFO, LOG_CAT_SERVER, "All bans and exceptions have been deleted.\n");
}

static void SV_BanAddr_f(void)
{
	SV_AddBanToList(qfalse);
}

static void SV_ExceptAddr_f(void)
{
	SV_AddBanToList(qtrue);
}

static void SV_BanDel_f(void)
{
	SV_DelBanFromList(qfalse);
}

static void SV_ExceptDel_f(void)
{
	SV_DelBanFromList(qtrue);
}

#endif // USE_BANS

/*
** SV_Strlen -- skips color escape codes
*/
int SV_Strlen( const char *str ) {
	const char *s = str;
	int count = 0;

	while ( *s ) {
		if ( Q_IsColorString( s ) ) {
			s += 2;
		} else {
			count++;
			s++;
		}
	}

	return count;
}


/*
================
SV_Status_f
================
*/
static void SV_Status_f( void ) {
	int i, j, l;
	const client_t *cl;
	const char *s;
	int max_namelength;
	int max_addrlength;
	char names[ MAX_CLIENTS * MAX_NAME_LENGTH ], *np[ MAX_CLIENTS ], nl[ MAX_CLIENTS ], *nc;
	char addrs[ MAX_CLIENTS * 48 ], *ap[ MAX_CLIENTS ], al[ MAX_CLIENTS ], *ac;

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Log( SEV_INFO, LOG_CAT_SERVER, "Server is not running.\n" );
		return;
	}

	max_namelength = 4; // strlen( "name" )
	max_addrlength = 7; // strlen( "address" )

	nc = names; *nc = '\0';
	ac = addrs; *ac = '\0';

	memset( np, 0, sizeof( np ) );
	memset( nl, 0, sizeof( nl ) );

	memset( ap, 0, sizeof( ap ) );
	memset( al, 0, sizeof( al ) );

	// first pass: save and determine max.lengths of name/address fields
	for ( i = 0, cl = svs.clients; i < sv.maxclients; i++, cl++ )
	{
		if ( cl->state == CS_FREE )
			continue;

		l = strlen( cl->name ) + 1;
		strcpy( nc, cl->name );
		np[ i ] = nc; nc += l;			// name pointer in name buffer
		nl[ i ] = SV_Strlen( cl->name );// name length without color sequences
		if ( nl[ i ] > max_namelength )
			max_namelength = nl[ i ];

		s = NET_AdrToString( &cl->netchan.remoteAddress );
		l = strlen( s ) + 1;
		strcpy( ac, s );
		ap[ i ] = ac; ac += l;			// address pointer in address buffer
		al[ i ] = l - 1;				// address length
		if ( al[ i ] > max_addrlength )
			max_addrlength = al[ i ];
	}

	Com_Log( SEV_INFO, LOG_CAT_SERVER, "map: %s\n", sv_mapname->string );

#if 0
	Com_Log( SEV_INFO, LOG_CAT_SERVER, "cl score ping name                        address                     rate\n" );
	Com_Log( SEV_INFO, LOG_CAT_SERVER, "-- ----- ---- --------------------------- --------------------------- -----\n" );
#else // variable-length fields
	Com_Log( SEV_INFO, LOG_CAT_SERVER, "cl score ping name" );
	for ( i = 0; i < max_namelength - 4; i++ )
		Com_Log( SEV_INFO, LOG_CAT_SERVER, " " );
	Com_Log( SEV_INFO, LOG_CAT_SERVER, " address" );
	for ( i = 0; i < max_addrlength - 7; i++ )
		Com_Log( SEV_INFO, LOG_CAT_SERVER, " " );
	Com_Log( SEV_INFO, LOG_CAT_SERVER, " rate\n" );

	Com_Log( SEV_INFO, LOG_CAT_SERVER, "-- ----- ---- " );
	for ( i = 0; i < max_namelength; i++ )
		Com_Log( SEV_INFO, LOG_CAT_SERVER, "-" );
	Com_Log( SEV_INFO, LOG_CAT_SERVER, " " );
	for ( i = 0; i < max_addrlength; i++ )
		Com_Log( SEV_INFO, LOG_CAT_SERVER, "-" );
	Com_Log( SEV_INFO, LOG_CAT_SERVER, " -----\n" );
#endif

	for ( i = 0, cl = svs.clients; i < sv.maxclients; i++, cl++ )
	{
		if ( cl->state == CS_FREE )
			continue;

		Com_Log( SEV_INFO, LOG_CAT_SERVER, "%2i ", i ); // id
		const playerState_t *ps = SV_GameClientNum( i );
		Com_Log( SEV_INFO, LOG_CAT_SERVER, "%5i ", ps->persistant[PERS_SCORE] );

		// ping/status
		if ( cl->state == CS_PRIMED )
			Com_Log( SEV_INFO, LOG_CAT_SERVER, " PRM " );
		else if ( cl->state == CS_CONNECTED )
			Com_Log( SEV_INFO, LOG_CAT_SERVER, " CON " );
		else if ( cl->state == CS_ZOMBIE )
			Com_Log( SEV_INFO, LOG_CAT_SERVER, " ZMB " );
		else
			Com_Log( SEV_INFO, LOG_CAT_SERVER, "%4i ", cl->ping < 999 ? cl->ping : 999 );

		// variable-length name field
		s = np[ i ];
		Com_Log( SEV_INFO, LOG_CAT_SERVER, "%s", s );
		l = max_namelength - nl[ i ];
		for ( j = 0; j < l; j++ )
			Com_Log( SEV_INFO, LOG_CAT_SERVER, " " );

		// variable-length address field
		s = ap[ i ];
		Com_Log( SEV_INFO, LOG_CAT_SERVER, S_COLOR_WHITE " %s", s );
		l = max_addrlength - al[ i ];
		for ( j = 0; j < l; j++ )
			Com_Log( SEV_INFO, LOG_CAT_SERVER, " " );

		// rate
		Com_Log( SEV_INFO, LOG_CAT_SERVER, " %5i\n", cl->rate );
	}

	Com_Log( SEV_INFO, LOG_CAT_SERVER, "\n" );
}


/*
==================
SV_ConSay_f
==================
*/
static void SV_ConSay_f( void ) {
	char	text[MAX_STRING_CHARS];

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Log( SEV_INFO, LOG_CAT_SERVER, "Server is not running.\n" );
		return;
	}

	if ( Cmd_Argc () < 2 ) {
		return;
	}

	char *p = Cmd_ArgsFrom( 1 );
	int len = (int)strlen( p );

	if ( len > 1000 ) {
		return;
	}

	if ( *p == '"' ) {
		p[len-1] = '\0';
		p++;
	}

	strcpy( text, "console: " );
	strcat( text, p );

	SV_SendServerCommand( NULL, "chat \"%s\"", text );
}


/*
==================
SV_ConTell_f
==================
*/
static void SV_ConTell_f( void ) {
	char	text[MAX_STRING_CHARS];

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Log( SEV_INFO, LOG_CAT_SERVER, "Server is not running.\n" );
		return;
	}

	if ( Cmd_Argc() < 3 ) {
		Com_Log( SEV_INFO, LOG_CAT_SERVER, "Usage: tell <client number> <text>\n" );
		return;
	}

	client_t *cl = SV_GetPlayerByNum();
	if ( !cl ) {
		return;
	}

	char *p = Cmd_ArgsFrom( 2 );
	int len = (int)strlen( p );

	if ( len > 1000 ) {
		return;
	}

	if ( *p == '"' ) {
		p[len-1] = '\0';
		p++;
	}

	strcpy( text, S_COLOR_MAGENTA "console: " );
	strcat( text, p );

	Com_Log( SEV_INFO, LOG_CAT_SERVER, "%s\n", text );
	SV_SendServerCommand( cl, "chat \"%s\"", text );
}


/*
==================
SV_Heartbeat_f

Also called by SV_DropClient, SV_DirectConnect, and SV_SpawnServer
==================
*/
void SV_Heartbeat_f( void ) {
	svs.nextHeartbeatTime = svs.time;
}


/*
===========
SV_Serverinfo_f

Examine the serverinfo string
===========
*/
static void SV_Serverinfo_f( void ) {
	const char *info;

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Log( SEV_INFO, LOG_CAT_SERVER, "Server is not running.\n" );
		return;
	}

	Com_Log( SEV_INFO, LOG_CAT_SERVER, "Server info settings:\n");
	info = sv.configstrings[ CS_SERVERINFO ];
	if ( info ) {
		Info_Print( info );
	}
}


/*
===========
SV_Systeminfo_f

Examine the systeminfo string
===========
*/
static void SV_Systeminfo_f( void ) {
	const char *info;
	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Log( SEV_INFO, LOG_CAT_SERVER, "Server is not running.\n" );
		return;
	}
	Com_Log( SEV_INFO, LOG_CAT_SERVER, "System info settings:\n" );
	info = sv.configstrings[ CS_SYSTEMINFO ];
	if ( info ) {
		Info_Print( info );
	}
}


/*
===========
SV_DumpUser_f

Examine all a users info strings
===========
*/
static void SV_DumpUser_f( void ) {
	client_t	*cl;

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Log( SEV_INFO, LOG_CAT_SERVER, "Server is not running.\n" );
		return;
	}

	if ( Cmd_Argc() != 2 ) {
		Com_Log( SEV_INFO, LOG_CAT_SERVER, "Usage: dumpuser <userid>\n");
		return;
	}

	cl = SV_GetPlayerByHandle();
	if ( !cl ) {
		return;
	}

	Com_Log( SEV_INFO, LOG_CAT_SERVER, "userinfo\n" );
	Com_Log( SEV_INFO, LOG_CAT_SERVER, "--------\n" );
	Info_Print( cl->userinfo );
}


/*
=================
SV_KillServer
=================
*/
static void SV_KillServer_f( void ) {
	SV_Shutdown( "killserver" );
}


/*
=================
SV_Locations
=================
*/
static void SV_Locations_f( void ) {

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Log( SEV_INFO, LOG_CAT_SERVER, "Server is not running.\n" );
		return;
	}

	if ( !sv_clientTLD->integer ) {
		Com_Log( SEV_INFO, LOG_CAT_SERVER, "Disabled on this server.\n" );
		return;
	}

	SV_PrintLocations_f( NULL );
}

//===========================================================

/*
==================
SV_CompleteMapName
==================
*/
static void SV_CompleteMapName( const char *args, int argNum ) {
	if ( argNum == 2 ) 	{
		if ( sv.pure != 0 ) {
			Field_CompleteFilename( "maps", "bsp", qtrue, FS_MATCH_PK3s | FS_MATCH_STICK );
		} else {
			Field_CompleteFilename( "maps", "bsp", qtrue, FS_MATCH_ANY | FS_MATCH_STICK );
		}
	}
}


/*
==================
SV_ClientNameCompletion

CNQ3 backport Phase 6: enumerates the names of every connected client and
invokes the supplied callback once per name.  Used by the tab-completion
handler of admin commands like kick, banUser, dumpuser and clientkick.
==================*/
static void SV_ClientNameCompletion( void (*callback)( const char *s ) ) {
	if ( callback == NULL ) {
		return;
	}

	/* Only meaningful while a server is running and the client table is
	 * allocated.  Silently do nothing otherwise so the completion system
	 * can still fall back to the usual cvar/cmd matching. */
	if ( com_sv_running == NULL || !com_sv_running->integer ) {
		return;
	}
	if ( svs.clients == NULL ) {
		return;
	}

	int maxClients = sv.maxclients;
	if ( maxClients <= 0 && sv_maxclients != NULL ) {
		maxClients = sv_maxclients->integer;
	}
	if ( maxClients < 0 ) {
		maxClients = 0;
	}
	if ( maxClients > MAX_CLIENTS ) {
		maxClients = MAX_CLIENTS;
	}

	for ( int i = 0; i < maxClients; i++ ) {
		const client_t *cl = &svs.clients[i];
		if ( cl->state < CS_CONNECTED ) {
			continue;
		}
		if ( cl->name[0] == '\0' ) {
			continue;
		}
		callback( cl->name );
	}
}


/*
==================
SV_CompleteClientName

CNQ3 backport Phase 6: tab completion handler for admin commands that take
a connected client's name as the first argument.
==================
*/
static void SV_CompleteClientName( const char *args, int argNum ) {
	if ( argNum != 2 ) {
		return;
	}
	Field_CompleteList( SV_ClientNameCompletion );
}


#if FEAT_MEMSTATS
/*
======================
SV_GetClientMemStats
======================
*/
clientMemStats_t SV_GetClientMemStats( int clientNum ) {
	clientMemStats_t s;

	memset( &s, 0, sizeof( s ) );
	if ( clientNum < 0 || clientNum >= sv.maxclients ) {
		return s;
	}
	const client_t *cl = &svs.clients[clientNum];
	if ( cl->state == CS_FREE ) {
		return s;
	}

	s.snapshotBytes = cl->frames[cl->deltaMessage & PACKET_MASK].messageSize;

	for ( int i = 0; i < MAX_DOWNLOAD_WINDOW; i++ ) {
		if ( cl->downloadBlocks[i] ) {
			s.downloadBytes += MAX_DOWNLOAD_BLKSIZE;
		}
	}

	s.totalBytes = s.snapshotBytes + s.downloadBytes;
	return s;
}


/*
======================
SV_MemInfoClients_f
======================
*/
static void SV_MemInfoClients_f( void ) {
	int totalSnap = 0, totalDl = 0, totalAll = 0;

	if ( !com_sv_running->integer ) {
		Com_Log( SEV_INFO, LOG_CAT_SERVER, "Server is not running.\n" );
		return;
	}

	int connected = 0;
	const client_t *cl = svs.clients;
	for ( int i = 0; i < sv.maxclients; i++, cl++ ) {
		if ( cl->state >= CS_CONNECTED ) connected++;
	}

	Com_Log( SEV_INFO, LOG_CAT_SERVER, "──────────────────────────────────────────\n" );
	Com_Log( SEV_INFO, LOG_CAT_SERVER, "CLIENT MEMORY (%i connected):\n", connected );
	Com_Log( SEV_INFO, LOG_CAT_SERVER, "  %-3s  %-20s  %8s  %8s  %8s\n",
		"#", "Name", "Snapshot", "Download", "Total" );

	cl = svs.clients;
	for ( int i = 0; i < sv.maxclients; i++, cl++ ) {
		if ( cl->state < CS_CONNECTED ) continue;
		clientMemStats_t ms = SV_GetClientMemStats( i );

		Com_Log( SEV_INFO, LOG_CAT_SERVER, "  %-3i  %-20s  %6.1f KB  %6.1f KB  %6.1f KB\n",
			i, cl->name,
			ms.snapshotBytes / 1024.0f,
			ms.downloadBytes / 1024.0f,
			ms.totalBytes    / 1024.0f );

		totalSnap += ms.snapshotBytes;
		totalDl   += ms.downloadBytes;
		totalAll  += ms.totalBytes;
	}

	Com_Log( SEV_INFO, LOG_CAT_SERVER, "  %-3s  %-20s  %6.1f KB  %6.1f KB  %6.1f KB\n",
		"---", "Total",
		totalSnap / 1024.0f,
		totalDl   / 1024.0f,
		totalAll  / 1024.0f );

	if ( connected > 0 ) {
		float avgSnap = totalSnap / (float)connected;
		float avgDl   = totalDl   / (float)connected;
		float avgAll  = totalAll  / (float)connected;
		Com_Log( SEV_INFO, LOG_CAT_SERVER, "  Projected (128p): snap %.0f KB  dl %.0f KB  total %.0f KB\n",
			avgSnap * 128.0f / 1024.0f,
			avgDl   * 128.0f / 1024.0f,
			avgAll  * 128.0f / 1024.0f );
	}
	Com_Log( SEV_INFO, LOG_CAT_SERVER, "──────────────────────────────────────────\n" );
}
#endif // FEAT_MEMSTATS


/*
==================
SV_PosCheck_f

Debug command: dumps a detailed spatial/physics report for client 0.
Usage: poscheck [range]   (range defaults to 256)
==================
*/

#define POSCHECK_DEFAULT_RANGE      256
#define POSCHECK_MAX_ENTITIES       32
#define POSCHECK_MAX_BRUSHES        16
#define POSCHECK_AXIS_OFFSET        8
#define POSCHECK_DOWN_TRACE_FACTOR  4

static const char *PosCheck_PmTypeName( int pm_type ) {
	switch ( pm_type ) {
		case PM_NORMAL:      return "PM_NORMAL";
		case PM_NOCLIP:      return "PM_NOCLIP";
		case PM_SPECTATOR:   return "PM_SPECTATOR";
		case PM_DEAD:        return "PM_DEAD";
		case PM_FREEZE:      return "PM_FREEZE";
		case PM_INTERMISSION: return "PM_INTERMISSION";
		default:             return "PM_UNKNOWN";
	}
}

static void PosCheck_DecodeContents( int contents, char *buf, int bufsize ) {
	struct { int bit; const char *name; } table[] = {
		{ CONTENTS_SOLID,       "SOLID" },
		{ CONTENTS_LAVA,        "LAVA" },
		{ CONTENTS_SLIME,       "SLIME" },
		{ CONTENTS_WATER,       "WATER" },
		{ CONTENTS_FOG,         "FOG" },
		{ CONTENTS_PLAYERCLIP,  "PLAYERCLIP" },
		{ CONTENTS_MONSTERCLIP, "MONSTERCLIP" },
		{ CONTENTS_BODY,        "BODY" },
		{ CONTENTS_CORPSE,      "CORPSE" },
		{ CONTENTS_TRIGGER,     "TRIGGER" },
		{ CONTENTS_NODROP,      "NODROP" },
	};
	int n = (int)(sizeof(table)/sizeof(table[0]));
	int i, first = 1;
	buf[0] = '\0';
	if ( !contents ) {
		Q_strncpyz( buf, "empty", bufsize );
		return;
	}
	for ( i = 0; i < n; i++ ) {
		if ( contents & table[i].bit ) {
			int remain = bufsize - (int)strlen(buf) - 1;
			if ( remain <= 0 ) break;
			if ( !first ) {
				strncat( buf, "|", remain );
				remain--;
			}
			if ( remain > 0 )
				strncat( buf, table[i].name, remain );
			first = 0;
		}
	}
}

static void SV_PosCheck_f( void ) {
	int            range;
	client_t      *cl;
	playerState_t *ps;
	sharedEntity_t *gent;
	vec3_t         origin;
	vec3_t         boxMins, boxMaxs;
	char           contBuf[256];
	int            i, j;

	/* Parse optional range argument */
	range = ( Cmd_Argc() > 1 ) ? atoi( Cmd_Argv(1) ) : POSCHECK_DEFAULT_RANGE;
	if ( range <= 0 ) range = POSCHECK_DEFAULT_RANGE;

	/* Check server is running a game */
	if ( sv.state != SS_GAME ) {
		Com_Log( SEV_INFO, LOG_CAT_SERVER, "poscheck: server is not in SS_GAME state.\n" );
		return;
	}

	/* Check client 0 is connected */
	if ( !svs.clients ) {
		Com_Log( SEV_INFO, LOG_CAT_SERVER, "poscheck: svs.clients is NULL.\n" );
		return;
	}
	cl = &svs.clients[0];
	if ( cl->state < CS_CONNECTED ) {
		Com_Log( SEV_INFO, LOG_CAT_SERVER, "poscheck: client 0 is not connected (state=%d).\n", cl->state );
		return;
	}

	ps   = SV_GameClientNum( 0 );
	gent = SV_GentityNum( 0 );
	VectorCopy( ps->origin, origin );

	/* ── Header ─────────────────────────────────────────────── */
	Com_Log( SEV_INFO, LOG_CAT_SERVER, "══════════════════════════════════════════\n" );
	Com_Log( SEV_INFO, LOG_CAT_SERVER, "POSCHECK  map=%-20s  time=%d  range=%d\n",
		sv_mapname ? sv_mapname->string : "?", sv.time, range );
	Com_Log( SEV_INFO, LOG_CAT_SERVER, "  client=0  name=\"%s\"\n", cl->name );
	Com_Log( SEV_INFO, LOG_CAT_SERVER, "══════════════════════════════════════════\n" );

	/* ── Section 1: Player state ─────────────────────────────── */
	Com_Log( SEV_INFO, LOG_CAT_SERVER, "─── 1. Player state ───\n" );
	Com_Log( SEV_INFO, LOG_CAT_SERVER, "  origin       : %.2f %.2f %.2f\n",
		origin[0], origin[1], origin[2] );
	Com_Log( SEV_INFO, LOG_CAT_SERVER, "  velocity     : %.2f %.2f %.2f  |v|=%.2f\n",
		ps->velocity[0], ps->velocity[1], ps->velocity[2],
		VectorLength( ps->velocity ) );
	Com_Log( SEV_INFO, LOG_CAT_SERVER, "  bbox mins    : %.2f %.2f %.2f\n",
		gent->r.mins[0], gent->r.mins[1], gent->r.mins[2] );
	Com_Log( SEV_INFO, LOG_CAT_SERVER, "  bbox maxs    : %.2f %.2f %.2f\n",
		gent->r.maxs[0], gent->r.maxs[1], gent->r.maxs[2] );
	{
		int gen = ps->groundEntityNum;
		if ( gen == ENTITYNUM_NONE )
			Com_Log( SEV_INFO, LOG_CAT_SERVER, "  groundEntity : airborne\n" );
		else if ( gen == ENTITYNUM_WORLD )
			Com_Log( SEV_INFO, LOG_CAT_SERVER, "  groundEntity : world\n" );
		else
			Com_Log( SEV_INFO, LOG_CAT_SERVER, "  groundEntity : entity %d\n", gen );
	}
	Com_Log( SEV_INFO, LOG_CAT_SERVER, "  pm_type      : %s (%d)\n",
		PosCheck_PmTypeName( ps->pm_type ), ps->pm_type );
	Com_Log( SEV_INFO, LOG_CAT_SERVER, "  health       : %d\n", ps->stats[STAT_HEALTH] );
	Com_Log( SEV_INFO, LOG_CAT_SERVER, "  armor        : %d\n", ps->stats[STAT_ARMOR] );

	/* ── Section 2: Axis traces ──────────────────────────────── */
	Com_Log( SEV_INFO, LOG_CAT_SERVER, "─── 2. Axis traces (range=%d) ───\n", range );
	{
		static const vec3_t dirs[6] = {
			{ 1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}
		};
		static const char *dnames[6] = { "+X","-X","+Y","-Y","+Z","-Z" };
		const vec3_t *pmins = (const vec3_t *)gent->r.mins;
		const vec3_t *pmaxs = (const vec3_t *)gent->r.maxs;
		int mask = MASK_PLAYERSOLID;

		for ( i = 0; i < 6; i++ ) {
			vec3_t end;
			trace_t tr;
			end[0] = origin[0] + dirs[i][0]*range;
			end[1] = origin[1] + dirs[i][1]*range;
			end[2] = origin[2] + dirs[i][2]*range;

			/* point trace */
			SV_Trace( &tr, origin, vec3_origin, vec3_origin, end,
				0, mask, qfalse );
			Com_Log( SEV_INFO, LOG_CAT_SERVER, "  %s point: frac=%.3f endpos=%.1f %.1f %.1f "
				"norm=%.2f %.2f %.2f dist=%.2f ent=%d as=%d ss=%d\n",
				dnames[i], tr.fraction,
				tr.endpos[0], tr.endpos[1], tr.endpos[2],
				tr.plane.normal[0], tr.plane.normal[1], tr.plane.normal[2],
				tr.plane.dist, tr.entityNum,
				(int)tr.allsolid, (int)tr.startsolid );

			/* box trace */
			SV_Trace( &tr, origin, *pmins, *pmaxs, end,
				0, mask, qfalse );
			Com_Log( SEV_INFO, LOG_CAT_SERVER, "  %s box  : frac=%.3f endpos=%.1f %.1f %.1f "
				"norm=%.2f %.2f %.2f dist=%.2f ent=%d as=%d ss=%d\n",
				dnames[i], tr.fraction,
				tr.endpos[0], tr.endpos[1], tr.endpos[2],
				tr.plane.normal[0], tr.plane.normal[1], tr.plane.normal[2],
				tr.plane.dist, tr.entityNum,
				(int)tr.allsolid, (int)tr.startsolid );
		}

		/* Extra deep downward trace */
		{
			vec3_t end;
			trace_t tr;
			int deepRange = range * POSCHECK_DOWN_TRACE_FACTOR;
			end[0] = origin[0];
			end[1] = origin[1];
			end[2] = origin[2] - deepRange;
			SV_Trace( &tr, origin, *pmins, *pmaxs, end,
				0, mask, qfalse );
			Com_Log( SEV_INFO, LOG_CAT_SERVER, "  -Z deep(box,range=%d): frac=%.3f endpos=%.1f %.1f %.1f "
				"norm=%.2f %.2f %.2f ent=%d as=%d ss=%d\n",
				deepRange, tr.fraction,
				tr.endpos[0], tr.endpos[1], tr.endpos[2],
				tr.plane.normal[0], tr.plane.normal[1], tr.plane.normal[2],
				tr.entityNum, (int)tr.allsolid, (int)tr.startsolid );
		}
	}

	/* ── Section 3: PointContents at 9 sample points ─────────── */
	Com_Log( SEV_INFO, LOG_CAT_SERVER, "─── 3. PointContents samples ───\n" );
	{
		vec3_t samples[9];
		const char *labels[9] = {
			"origin","feet","head","+X","-X","+Y","-Y","+Z","-Z"
		};
		float off = POSCHECK_AXIS_OFFSET;
		VectorCopy( origin, samples[0] );
		VectorSet( samples[1], origin[0], origin[1], origin[2] + gent->r.mins[2] );
		VectorSet( samples[2], origin[0], origin[1], origin[2] + gent->r.maxs[2] );
		VectorSet( samples[3], origin[0]+off, origin[1], origin[2] );
		VectorSet( samples[4], origin[0]-off, origin[1], origin[2] );
		VectorSet( samples[5], origin[0], origin[1]+off, origin[2] );
		VectorSet( samples[6], origin[0], origin[1]-off, origin[2] );
		VectorSet( samples[7], origin[0], origin[1], origin[2]+off );
		VectorSet( samples[8], origin[0], origin[1], origin[2]-off );
		for ( i = 0; i < 9; i++ ) {
			int c = CM_PointContents( samples[i], 0 );
			PosCheck_DecodeContents( c, contBuf, sizeof(contBuf) );
			Com_Log( SEV_INFO, LOG_CAT_SERVER, "  %-8s : 0x%08x  %s\n", labels[i], c, contBuf );
		}
	}

	/* ── Section 4: Surrounding entities ─────────────────────── */
	Com_Log( SEV_INFO, LOG_CAT_SERVER, "─── 4. Area entities (range=%d) ───\n", range );
	{
		int   entList[POSCHECK_MAX_ENTITIES*4];
		int   numEnts;
		vec3_t qmins, qmaxs;

		VectorSet( qmins, origin[0]-range, origin[1]-range, origin[2]-range );
		VectorSet( qmaxs, origin[0]+range, origin[1]+range, origin[2]+range );
		numEnts = SV_AreaEntities( qmins, qmaxs, entList,
			POSCHECK_MAX_ENTITIES*4 );

		/* collect, sort by distance, cap */
		typedef struct { int num; float dist; } EntEntry;
		EntEntry entries[POSCHECK_MAX_ENTITIES*4];
		int entCount = 0;
		for ( i = 0; i < numEnts && entCount < POSCHECK_MAX_ENTITIES*4; i++ ) {
			int entNum = entList[i];
			sharedEntity_t *se = SV_GentityNum( entNum );
			if ( !se->r.linked ) continue;
			vec3_t delta;
			VectorSubtract( se->r.currentOrigin, origin, delta );
			entries[entCount].num  = entNum;
			entries[entCount].dist = VectorLength( delta );
			entCount++;
		}
		/* simple insertion sort by dist */
		for ( i = 1; i < entCount; i++ ) {
			EntEntry tmp = entries[i];
			j = i-1;
			while ( j >= 0 && entries[j].dist > tmp.dist ) {
				entries[j+1] = entries[j];
				j--;
			}
			entries[j+1] = tmp;
		}
		if ( entCount > POSCHECK_MAX_ENTITIES ) entCount = POSCHECK_MAX_ENTITIES;
		Com_Log( SEV_INFO, LOG_CAT_SERVER, "  found %d entities (showing up to %d):\n",
			numEnts, POSCHECK_MAX_ENTITIES );
		for ( i = 0; i < entCount; i++ ) {
			int entNum = entries[i].num;
			sharedEntity_t *se = SV_GentityNum( entNum );
			Com_Log( SEV_INFO, LOG_CAT_SERVER,
				"  [%d] ent=%d dist=%.1f origin=%.1f %.1f %.1f "
				"mins=%.1f %.1f %.1f maxs=%.1f %.1f %.1f contents=0x%x\n",
				i, entNum, entries[i].dist,
				se->r.currentOrigin[0], se->r.currentOrigin[1], se->r.currentOrigin[2],
				se->r.mins[0], se->r.mins[1], se->r.mins[2],
				se->r.maxs[0], se->r.maxs[1], se->r.maxs[2],
				se->r.contents );
		}
	}

	/* ── Section 5: Surrounding brushes via CM_BoxLeafnums ───── */
	Com_Log( SEV_INFO, LOG_CAT_SERVER, "─── 5. Surrounding brushes (range=%d) ───\n", range );
	{
		int   leafList[256];
		int   numLeafs, lastLeaf;
		int   brushSeen[POSCHECK_MAX_BRUSHES*4];
		int   brushCount = 0;

		VectorSet( boxMins, origin[0]-range, origin[1]-range, origin[2]-range );
		VectorSet( boxMaxs, origin[0]+range, origin[1]+range, origin[2]+range );
		numLeafs = CM_BoxLeafnums( boxMins, boxMaxs, leafList, 256, &lastLeaf );

		/* Walk each leaf's brushes, deduplicate */
		for ( i = 0; i < numLeafs; i++ ) {
			int leafNum = leafList[i];
			if ( leafNum < 0 || leafNum >= cm.numLeafs ) continue;
			const cLeaf_t *leaf = &cm.leafs[leafNum];
			int k;
			for ( k = leaf->firstLeafBrush;
				  k < leaf->firstLeafBrush + leaf->numLeafBrushes; k++ ) {
				int brushIdx = cm.leafbrushes[k];
				/* dedup check */
				int found = 0, m;
				for ( m = 0; m < brushCount; m++ ) {
					if ( brushSeen[m] == brushIdx ) { found = 1; break; }
				}
				if ( !found && brushCount < POSCHECK_MAX_BRUSHES*4 ) {
					brushSeen[brushCount++] = brushIdx;
				}
			}
		}

		/* collect entries sorted by dist to brush center */
		typedef struct { int idx; float dist; } BrushEntry;
		BrushEntry bEntries[POSCHECK_MAX_BRUSHES*4];
		int bCount = 0;
		for ( i = 0; i < brushCount && i < POSCHECK_MAX_BRUSHES*4; i++ ) {
			int bi = brushSeen[i];
			if ( bi < 0 || bi >= cm.numBrushes ) continue;
			const cbrush_t *b = &cm.brushes[bi];
			/* center of brush AABB */
			vec3_t center;
			center[0] = 0.5f*(b->bounds[0][0] + b->bounds[1][0]);
			center[1] = 0.5f*(b->bounds[0][1] + b->bounds[1][1]);
			center[2] = 0.5f*(b->bounds[0][2] + b->bounds[1][2]);
			vec3_t delta;
			VectorSubtract( center, origin, delta );
			bEntries[bCount].idx  = bi;
			bEntries[bCount].dist = VectorLength( delta );
			bCount++;
		}
		/* sort by dist */
		for ( i = 1; i < bCount; i++ ) {
			BrushEntry tmp = bEntries[i];
			j = i-1;
			while ( j >= 0 && bEntries[j].dist > tmp.dist ) {
				bEntries[j+1] = bEntries[j];
				j--;
			}
			bEntries[j+1] = tmp;
		}
		if ( bCount > POSCHECK_MAX_BRUSHES ) bCount = POSCHECK_MAX_BRUSHES;
		Com_Log( SEV_INFO, LOG_CAT_SERVER, "  found %d unique brushes (showing up to %d):\n",
			brushCount, POSCHECK_MAX_BRUSHES );
		for ( i = 0; i < bCount; i++ ) {
			int bi = bEntries[i].idx;
			const cbrush_t *b = &cm.brushes[bi];
			const char *shaderName = "(none)";
			if ( b->shaderNum >= 0 && b->shaderNum < cm.numShaders &&
				 cm.shaders[b->shaderNum].shader[0] != '\0' ) {
				shaderName = cm.shaders[b->shaderNum].shader;
			}
			PosCheck_DecodeContents( b->contents, contBuf, sizeof(contBuf) );
			Com_Log( SEV_INFO, LOG_CAT_SERVER,
				"  brush[%d] dist=%.1f contents=0x%x(%s) sides=%d "
				"bounds=(%.1f %.1f %.1f)-(%.1f %.1f %.1f) shader=%s\n",
				bi, bEntries[i].dist, b->contents, contBuf, b->numsides,
				b->bounds[0][0], b->bounds[0][1], b->bounds[0][2],
				b->bounds[1][0], b->bounds[1][1], b->bounds[1][2],
				shaderName );
			/* Print each side's plane */
			if ( b->sides && b->numsides > 0 ) {
				int s;
				for ( s = 0; s < b->numsides; s++ ) {
					const cbrushside_t *side = &b->sides[s];
					if ( side->plane ) {
						Com_Log( SEV_INFO, LOG_CAT_SERVER,
							"    side[%d] normal=%.3f %.3f %.3f dist=%.3f\n",
							s,
							side->plane->normal[0],
							side->plane->normal[1],
							side->plane->normal[2],
							side->plane->dist );
					}
				}
			}
		}
	}

	/* ── Section 6: Cluster / PVS ────────────────────────────── */
	Com_Log( SEV_INFO, LOG_CAT_SERVER, "─── 6. Cluster / PVS ───\n" );
	{
		int leafNum = CM_PointLeafnum( origin );
		int cluster = CM_LeafCluster( leafNum );
		int area    = CM_LeafArea( leafNum );
		int totalClusters = CM_NumClusters();
		Com_Log( SEV_INFO, LOG_CAT_SERVER, "  leafnum=%d  cluster=%d  area=%d  totalClusters=%d\n",
			leafNum, cluster, area, totalClusters );
		if ( cluster >= 0 ) {
			byte *pvs = CM_ClusterPVS( cluster );
			int visCount = 0;
			if ( pvs ) {
				/* Count set bits across all cluster bytes */
				int clusterBytes = ( totalClusters + 7 ) >> 3;
				int b;
				for ( b = 0; b < clusterBytes; b++ ) {
					byte byt = pvs[b];
					while ( byt ) {
						visCount += ( byt & 1 );
						byt >>= 1;
					}
				}
			}
			Com_Log( SEV_INFO, LOG_CAT_SERVER, "  PVS: %d/%d clusters visible\n",
				visCount, totalClusters );
		}
	}

	Com_Log( SEV_INFO, LOG_CAT_SERVER, "══════════════════════════════════════════\n" );
}


/*
==================
SV_AddOperatorCommands
==================
*/
void SV_AddOperatorCommands( void ) {
	static qboolean	initialized;

	if ( initialized ) {
		return;
	}
	initialized = qtrue;

	Cmd_AddCommand ("heartbeat", SV_Heartbeat_f);
	Cmd_AddCommand ("kick", SV_Kick_f);
	/* CNQ3 backport Phase 6: client-name aware auto-completion */
	Cmd_SetCommandCompletionFunc( "kick", SV_CompleteClientName );
#ifndef STANDALONE
#ifdef USE_BANS
	if(!Cvar_VariableIntegerValue("com_standalone"))
	{
		Cmd_AddCommand ("banUser", SV_Ban_f);
		Cmd_SetCommandCompletionFunc( "banUser", SV_CompleteClientName );
		Cmd_AddCommand ("banClient", SV_BanNum_f);
		Cmd_SetCommandCompletionFunc( "banClient", SV_CompleteClientName );
	}
#endif
#endif
	Cmd_AddCommand ("kickbots", SV_KickBots_f);
	Cmd_AddCommand ("kickall", SV_KickAll_f);
	Cmd_AddCommand ("kicknum", SV_KickNum_f);
	Cmd_AddCommand ("clientkick", SV_KickNum_f); // Legacy command
	Cmd_SetCommandCompletionFunc( "clientkick", SV_CompleteClientName );
	Cmd_AddCommand ("status", SV_Status_f);
	Cmd_AddCommand ("dumpuser", SV_DumpUser_f);
	Cmd_SetCommandCompletionFunc( "dumpuser", SV_CompleteClientName );
	/*
	 * "mute" is exposed by the game VM rather than the engine; we still
	 * register a completion callback so that if the game registers a
	 * top-level "mute" command the tab-completion handler picks it up.
	 * Cmd_SetCommandCompletionFunc() is a silent no-op when the command
	 * doesn't exist, so this is safe even when mute is only a sub-command
	 * of /adm.
	 */
	Cmd_SetCommandCompletionFunc( "mute", SV_CompleteClientName );
	Cmd_AddCommand ("map_restart", SV_MapRestart_f);
	Cmd_AddCommand ("sectorlist", SV_SectorList_f);
	Cmd_AddCommand ("map", SV_Map_f);
	Cmd_SetCommandCompletionFunc( "map", SV_CompleteMapName );
	Cmd_AddCommand ("killserver", SV_KillServer_f);
#ifdef USE_BANS
	Cmd_AddCommand("rehashbans", SV_RehashBans_f);
	Cmd_AddCommand("listbans", SV_ListBans_f);
	Cmd_AddCommand("banaddr", SV_BanAddr_f);
	Cmd_AddCommand("exceptaddr", SV_ExceptAddr_f);
	Cmd_AddCommand("bandel", SV_BanDel_f);
	Cmd_AddCommand("exceptdel", SV_ExceptDel_f);
	Cmd_AddCommand("flushbans", SV_FlushBans_f);
#endif
	Cmd_AddCommand( "filter", SV_AddFilter_f );
	Cmd_AddCommand( "filtercmd", SV_AddFilterCmd_f );
	Cmd_AddCommand( "bot_verify_character", SV_BotVerifyCharacter_f );
	Cmd_AddCommand( "bot_debug_weapons", SV_BotDebugWeapons_f );
#if FEAT_MEMSTATS
	Cmd_AddCommand( "meminfo_clients", SV_MemInfoClients_f );
#endif
#ifdef _DEBUG
	Cmd_AddCommand( "poscheck", SV_PosCheck_f );
#endif
}


/*
==================
SV_RemoveOperatorCommands
==================
*/
void SV_RemoveOperatorCommands( void ) {
#if 0
	// removing these won't let the server start again
	Cmd_RemoveCommand ("heartbeat");
	Cmd_RemoveCommand ("kick");
	Cmd_RemoveCommand ("kicknum");
	Cmd_RemoveCommand ("clientkick");
	Cmd_RemoveCommand ("kickall");
	Cmd_RemoveCommand ("kickbots");
	Cmd_RemoveCommand ("banUser");
	Cmd_RemoveCommand ("banClient");
	Cmd_RemoveCommand ("status");
	Cmd_RemoveCommand ("dumpuser");
	Cmd_RemoveCommand ("map_restart");
	Cmd_RemoveCommand ("sectorlist");
#endif
}


void SV_AddDedicatedCommands( void )
{
	Cmd_AddCommand( "serverinfo", SV_Serverinfo_f );
	Cmd_AddCommand( "systeminfo", SV_Systeminfo_f );
	Cmd_AddCommand( "tell", SV_ConTell_f );
	Cmd_AddCommand( "say", SV_ConSay_f );
	Cmd_AddCommand( "locations", SV_Locations_f );
}


void SV_RemoveDedicatedCommands( void )
{
	Cmd_RemoveCommand( "serverinfo" );
	Cmd_RemoveCommand( "systeminfo" );
	Cmd_RemoveCommand( "tell" );
	Cmd_RemoveCommand( "say" );
	Cmd_RemoveCommand( "locations" );
}
