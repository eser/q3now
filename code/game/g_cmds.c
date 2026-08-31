// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
#include "g_local.h"
#include "g_behavior.h"
#include "wired/bots/g_wiredintel.h"
LOG_DECLARE_CHANNEL( ch_game, "game" );

#if FEAT_TA_VOICECHAT
#include "../qcommon/menudef.h" // for the voice chats
#endif

static void G_SendBStats( int sourceClient, int recipientClient );

/*
==================
DeathmatchScoreboardMessage

==================
*/
void DeathmatchScoreboardMessage( gentity_t *ent )
{
	char	   entry[1024];
	char	   string[1000];
	int		   stringlength;
	int		   i, j;
	gclient_t *cl;
	int		   numSorted, scoreFlags, accuracy, perfect;

	// don't send scores to bots, they don't parse it
	if ( ent->r.svFlags & SVF_BOT ) {
		return;
	}

	// send the latest information on all clients
	string[0]	 = 0;
	stringlength = 0;
	scoreFlags	 = 0;

	numSorted = level.numConnectedClients;

	for ( i = 0; i < numSorted; i++ ) {
		int ping;

		cl = &level.clients[level.sortedClients[i]];

		if ( cl->pers.connected == CON_CONNECTING ) {
			ping = -1;
		} else {
			ping = cl->ps.ping < 999 ? cl->ps.ping : 999;
		}

		if ( cl->accuracy_shots ) {
			accuracy = cl->accuracy_hits * 100 / cl->accuracy_shots;
		} else {
			accuracy = 0;
		}
		perfect = ( cl->ps.persistant[PERS_RANK] == 0 && cl->ps.persistant[PERS_KILLED] == 0 ) ? 1 : 0;

		Com_sprintf( entry, sizeof( entry ), " %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i",
					 level.sortedClients[i], cl->ps.persistant[PERS_SCORE], ping,
					 ( level.time - cl->pers.enterTime ) / 60000, scoreFlags,
					 g_entities[level.sortedClients[i]].s.powerups, accuracy, cl->ps.persistant[PERS_IMPRESSIVE_COUNT],
					 cl->ps.persistant[PERS_EXCELLENT_COUNT], cl->ps.persistant[PERS_GAUNTLET_FRAG_COUNT],
					 cl->ps.persistant[PERS_DEFEND_COUNT], cl->ps.persistant[PERS_ASSIST_COUNT], perfect,
					 cl->ps.persistant[PERS_CAPTURES], cl->ps.persistant[PERS_KILLED],
					 cl->ps.persistant[PERS_KILLING_SPREE_COUNT], cl->ps.persistant[PERS_RAMPAGE_COUNT],
					 cl->ps.persistant[PERS_MASSACRE_COUNT], cl->ps.persistant[PERS_UNSTOPPABLE_COUNT] );
		j = strlen( entry );
		if ( stringlength + j >= sizeof( string ) )
			break;
		strcpy( string + stringlength, entry );
		stringlength += j;
	}

	trap_SendServerCommand( ent - g_entities, va( "scores %i %i %i%s", i, level.teamScores[TEAM_RED],
												  level.teamScores[TEAM_BLUE], string ) );

	/* auto-send per-attack stats for all players to the requesting client */
	{
		int k;
		for ( k = 0; k < level.maxclients; k++ ) {
			if ( level.clients[k].pers.connected == CON_CONNECTED ) {
				G_SendBStats( k, ent - g_entities );
			}
		}
	}
}


/*
==================
Cmd_Score_f

Request current scoreboard information
==================
*/
void Cmd_Score_f( gentity_t *ent )
{
	DeathmatchScoreboardMessage( ent );
}


/*
==================
CheatsOk
==================
*/
qboolean CheatsOk( gentity_t *ent )
{
	if ( !g_cheats.integer ) {
		trap_SendServerCommand( ent - g_entities, "print \"Cheats are not enabled on this server.\n\"" );
		return qfalse;
	}
	if ( ent->health <= 0 ) {
		trap_SendServerCommand( ent - g_entities, "print \"You must be alive to use this command.\n\"" );
		return qfalse;
	}
	return qtrue;
}


/*
==================
ConcatArgs
==================
*/
char *ConcatArgs( int start )
{
	int			i, c, tlen;
	static char line[MAX_STRING_CHARS];
	int			len;
	char		arg[MAX_STRING_CHARS];

	len = 0;
	c	= trap_Argc();
	for ( i = start; i < c; i++ ) {
		trap_Argv( i, arg, sizeof( arg ) );
		tlen = strlen( arg );
		if ( len + tlen >= MAX_STRING_CHARS - 1 ) {
			break;
		}
		memcpy( line + len, arg, tlen );
		len += tlen;
		if ( i != c - 1 ) {
			line[len] = ' ';
			len++;
		}
	}

	line[len] = 0;

	return line;
}


/*
==================
StringIsInteger
==================
*/
qboolean StringIsInteger( const char *s )
{
	int		 i;
	int		 len;
	qboolean foundDigit;

	len		   = strlen( s );
	foundDigit = qfalse;

	for ( i = 0; i < len; i++ ) {
		if ( !isdigit( s[i] ) ) {
			return qfalse;
		}

		foundDigit = qtrue;
	}

	return foundDigit;
}


/*
==================
ClientNumberFromString

Returns a player number for either a number or name string
Returns -1 if invalid
==================
*/
int ClientNumberFromString( gentity_t *to, char *s, qboolean checkNums, qboolean checkNames )
{
	gclient_t *cl;
	int		   idnum;
	char	   cleanName[MAX_STRING_CHARS];

	if ( checkNums ) {
		// numeric values could be slot numbers
		if ( StringIsInteger( s ) ) {
			idnum = atoi( s );
			if ( idnum >= 0 && idnum < level.maxclients ) {
				cl = &level.clients[idnum];
				if ( cl->pers.connected == CON_CONNECTED ) {
					return idnum;
				}
			}
		}
	}

	if ( checkNames ) {
		// check for a name match
		for ( idnum = 0, cl = level.clients; idnum < level.maxclients; idnum++, cl++ ) {
			if ( cl->pers.connected != CON_CONNECTED ) {
				continue;
			}
			Q_strncpyz( cleanName, cl->pers.netname, sizeof( cleanName ) );
			Q_CleanStr( cleanName );
			if ( !Q_stricmp( cleanName, s ) ) {
				return idnum;
			}
		}
	}

	trap_SendServerCommand( to - g_entities, va( "print \"User %s is not on the server\n\"", s ) );
	return -1;
}

/*
==================
Cmd_Give_f

Give items to a client
==================
*/
void Cmd_Give_f( gentity_t *ent )
{
	char	  *name;
	gitem_t	  *it;
	int		   i;
	qboolean   give_all;
	gentity_t *it_ent;
	trace_t	   trace;

	if ( !CheatsOk( ent ) ) {
		return;
	}

	name = ConcatArgs( 1 );

	if ( Q_stricmp( name, "all" ) == 0 )
		give_all = qtrue;
	else
		give_all = qfalse;

	if ( give_all || Q_stricmp( name, "health" ) == 0 ) {
		ent->health = MAX_HEALTH;
		if ( !give_all )
			return;
	}

	if ( give_all || Q_stricmp( name, "weapons" ) == 0 ) {
		ent->client->ps.stats[STAT_WEAPONS] = ( 1 << WP_NUM_WEAPONS ) - 1 - ( 1 << WP_NONE );
		if ( !give_all )
			return;
	}

	if ( give_all || Q_stricmp( name, "ammo" ) == 0 ) {
		for ( i = WP_NONE + 1; i < WP_NUM_WEAPONS; i++ ) {
			ent->client->ps.ammo[i] = bg_weaponlist[i].maxAmmunition;
		}
		if ( !give_all )
			return;
	}

	if ( give_all || Q_stricmp( name, "armor" ) == 0 ) {
		ent->client->ps.stats[STAT_ARMOR]	   = MAX_ARMOR;
		ent->client->ps.stats[STAT_ARMORCLASS] = ARM_NUM_ARMOR - 1;

		if ( !give_all )
			return;
	}

	if ( Q_stricmp( name, "excellent" ) == 0 ) {
		ent->client->ps.persistant[PERS_EXCELLENT_COUNT]++;
		return;
	}
	if ( Q_stricmp( name, "impressive" ) == 0 ) {
		ent->client->ps.persistant[PERS_IMPRESSIVE_COUNT]++;
		return;
	}
	if ( Q_stricmp( name, "gauntletaward" ) == 0 ) {
		ent->client->ps.persistant[PERS_GAUNTLET_FRAG_COUNT]++;
		return;
	}
	if ( Q_stricmp( name, "defend" ) == 0 ) {
		ent->client->ps.persistant[PERS_DEFEND_COUNT]++;
		return;
	}
	if ( Q_stricmp( name, "assist" ) == 0 ) {
		ent->client->ps.persistant[PERS_ASSIST_COUNT]++;
		return;
	}

	// spawn a specific item right on the player
	if ( !give_all ) {
		it = BG_FindItem( name );
		if ( !it ) {
			return;
		}

		it_ent = G_Spawn();
		VectorCopy( ent->r.currentOrigin, it_ent->s.origin );
		it_ent->classname = it->classname;
		G_SpawnItem( it_ent, it );
		FinishSpawningItem( it_ent );
		memset( &trace, 0, sizeof( trace ) );
		Touch_Item( it_ent, ent, &trace );
		if ( it_ent->inuse ) {
			G_FreeEntity( it_ent );
		}
	}
}


/*
==================
Cmd_God_f

Sets client to godmode

argv(0) god
==================
*/
void Cmd_God_f( gentity_t *ent )
{
	char *msg;

	if ( !CheatsOk( ent ) ) {
		return;
	}

	ent->flags ^= FL_GODMODE;
	if ( !( ent->flags & FL_GODMODE ) )
		msg = "godmode OFF\n";
	else
		msg = "godmode ON\n";

	trap_SendServerCommand( ent - g_entities, va( "print \"%s\"", msg ) );
}


/*
==================
Cmd_Cloak_f

Sets client to cloak

argv(0) cloak
==================
*/
void Cmd_Cloak_f( gentity_t *ent )
{
	char *msg;

	if ( !CheatsOk( ent ) ) {
		return;
	}

	ent->flags ^= FL_CLOAK;
	if ( !( ent->flags & FL_CLOAK ) )
		msg = "cloak OFF\n";
	else
		msg = "cloak ON\n";

	trap_SendServerCommand( ent - g_entities, va( "print \"%s\"", msg ) );
}


/*
==================
Cmd_Notarget_f

Sets client to notarget

argv(0) notarget
==================
*/
void Cmd_Notarget_f( gentity_t *ent )
{
	char *msg;

	if ( !CheatsOk( ent ) ) {
		return;
	}

	ent->flags ^= FL_NOTARGET;
	if ( !( ent->flags & FL_NOTARGET ) )
		msg = "notarget OFF\n";
	else
		msg = "notarget ON\n";

	trap_SendServerCommand( ent - g_entities, va( "print \"%s\"", msg ) );
}


/*
==================
Cmd_Noclip_f

argv(0) noclip
==================
*/
void Cmd_Noclip_f( gentity_t *ent )
{
	char *msg;

	if ( !CheatsOk( ent ) ) {
		return;
	}

	if ( ent->client->noclip ) {
		msg = "noclip OFF\n";
	} else {
		msg = "noclip ON\n";
	}
	ent->client->noclip = !ent->client->noclip;

	trap_SendServerCommand( ent - g_entities, va( "print \"%s\"", msg ) );
}


/*
==================
Cmd_LevelShot_f

This is just to help generate the level pictures
for the menus.  It goes to the intermission immediately
and sends over a command to the client to resize the view,
hide the scoreboard, and take a special screenshot
==================
*/
void Cmd_LevelShot_f( gentity_t *ent )
{
	if ( !ent->client->pers.localClient ) {
		trap_SendServerCommand( ent - g_entities,
								"print \"The levelshot command must be executed by a local client\n\"" );
		return;
	}

	if ( !CheatsOk( ent ) )
		return;

	BeginIntermission();
	trap_SendServerCommand( ent - g_entities, "clientLevelShot" );
}


/*
==================
Cmd_TeamTask_f
==================
*/
void Cmd_TeamTask_f( gentity_t *ent )
{
	char userinfo[MAX_INFO_STRING];
	char arg[MAX_TOKEN_CHARS];
	int	 task;
	int	 client = ent->client - level.clients;

	if ( trap_Argc() != 2 ) {
		return;
	}
	trap_Argv( 1, arg, sizeof( arg ) );
	task = atoi( arg );

	trap_GetUserinfo( client, userinfo, sizeof( userinfo ) );
	Info_SetValueForKey( userinfo, "teamtask", va( "%d", task ) );
	trap_SetUserinfo( client, userinfo );
	ClientUserinfoChanged( client );
}


#if FEAT_DROP_ITEMS
/*
=================
Cmd_Drop_f
Drop current weapon or flag. Bitmask cvar g_dropEnable:
  bit 0 = weapon, bit 1 = flag, bit 2 = ammo, bit 3 = health. (11D)
=================
*/
#define DROP_WEAPON 1
#define DROP_FLAG	2
#define DROP_AMMO	4
#define DROP_HEALTH 8
void Cmd_Drop_f( gentity_t *ent )
{
	char arg[MAX_TOKEN_CHARS];
	int	 enable;

	if ( !g_dropEnable.integer ) {
		trap_SendServerCommand( ent->s.number, "print \"Item dropping is disabled.\n\"" );
		return;
	}
	if ( ent->client->sess.sessionTeam == TEAM_SPECTATOR || ent->health <= 0 ) {
		return;
	}

	enable = g_dropEnable.integer;
	trap_Argv( 1, arg, sizeof( arg ) );

	if ( !arg[0] || Q_stricmp( arg, "weapon" ) == 0 ) {
		// drop current weapon
		gitem_t *item;
		int		 weapon;

		if ( !( enable & DROP_WEAPON ) ) {
			trap_SendServerCommand( ent->s.number, "print \"Weapon dropping is disabled.\n\"" );
			return;
		}
		weapon = ent->s.weapon;
		if ( weapon <= WP_GAUNTLET || weapon >= WP_NUM_WEAPONS ) {
			trap_SendServerCommand( ent->s.number, "print \"Cannot drop this weapon.\n\"" );
			return;
		}
		item = BG_FindItemForWeapon( weapon );
		if ( item ) {
			gentity_t *drop				 = Drop_Item( ent, item, 0 );
			drop->r.ownerNum			 = ent->s.number;
			ent->client->ps.ammo[weapon] = 0;
			ent->client->ps.stats[STAT_WEAPONS] &= ~( 1 << weapon );
			// switch to next available weapon
			ent->client->ps.weapon = WP_MACHINEGUN;
		}
	} else if ( Q_stricmp( arg, "flag" ) == 0 ) {
		gitem_t *item = NULL;
		if ( !( enable & DROP_FLAG ) ) {
			trap_SendServerCommand( ent->s.number, "print \"Flag dropping is disabled.\n\"" );
			return;
		}
		if ( ent->client->ps.powerups[PW_REDFLAG] ) {
			item								 = BG_FindItemForPowerup( PW_REDFLAG );
			ent->client->ps.powerups[PW_REDFLAG] = 0;
		} else if ( ent->client->ps.powerups[PW_BLUEFLAG] ) {
			item								  = BG_FindItemForPowerup( PW_BLUEFLAG );
			ent->client->ps.powerups[PW_BLUEFLAG] = 0;
		}
		if ( item ) {
			gentity_t *drop	 = Drop_Item( ent, item, 0 );
			drop->r.ownerNum = ent->s.number;
		} else {
			trap_SendServerCommand( ent->s.number, "print \"You are not carrying a flag.\n\"" );
		}
	} else {
		trap_SendServerCommand( ent->s.number, "print \"Usage: drop [weapon|flag]\n\"" );
	}
}
#endif

/*
=================
Cmd_Kill_f
=================
*/
void Cmd_Kill_f( gentity_t *ent )
{
	if ( ent->client->sess.sessionTeam == TEAM_SPECTATOR ) {
		return;
	}
	if ( ent->health <= 0 ) {
		return;
	}
	ent->flags &= ~FL_GODMODE;
	ent->client->ps.stats[STAT_HEALTH] = ent->health = GIB_HEALTH;
	player_die( ent, ent, ent, 100000, MOD_SUICIDE );
}

/*
=================
BroadcastTeamChange

Let everyone know about a team change
=================
*/
void BroadcastTeamChange( gclient_t *client, int oldTeam )
{
	if ( g_gametype.integer == GT_LASTMANSTANDING ) {
		return;
	}

	if ( client->sess.sessionTeam == TEAM_RED ) {
		trap_SendServerCommand(
			-1, va( "cp \"" S_COLOR_GREEN "%s" S_COLOR_WHITE " joined the red team.\n\"", client->pers.netname ) );
	} else if ( client->sess.sessionTeam == TEAM_BLUE ) {
		trap_SendServerCommand(
			-1, va( "cp \"" S_COLOR_GREEN "%s" S_COLOR_WHITE " joined the blue team.\n\"", client->pers.netname ) );
	} else if ( client->sess.sessionTeam == TEAM_SPECTATOR && oldTeam != TEAM_SPECTATOR ) {
		trap_SendServerCommand(
			-1, va( "cp \"" S_COLOR_GREEN "%s" S_COLOR_WHITE " joined the spectators.\n\"", client->pers.netname ) );
	} else if ( client->sess.sessionTeam == TEAM_FREE ) {
		trap_SendServerCommand(
			-1, va( "cp \"" S_COLOR_GREEN "%s" S_COLOR_WHITE " joined the battle.\n\"", client->pers.netname ) );
	}
}

/*
=================
SetTeam
=================
*/
void SetTeam( gentity_t *ent, const char *s )
{
	int				 team, oldTeam;
	gclient_t		*client;
	int				 clientNum;
	spectatorState_t specState;
	int				 specClient;
	int				 teamLeader;

	//
	// see what change is requested
	//
	client = ent->client;

	// stateless clients are permanently spectators — reject all team changes
	if ( client->sess.isStatelessClient ) {
		trap_SendServerCommand( ent->s.number, "print \"Stateless clients cannot join a team.\n\"" );
		return;
	}

	clientNum  = client - level.clients;
	specClient = 0;
	specState  = SPECTATOR_NOT;
	if ( !Q_stricmp( s, "scoreboard" ) || !Q_stricmp( s, "score" ) ) {
		team	  = TEAM_SPECTATOR;
		specState = SPECTATOR_SCOREBOARD;
	} else if ( !Q_stricmp( s, "follow1" ) ) {
		team	   = TEAM_SPECTATOR;
		specState  = SPECTATOR_FOLLOW;
		specClient = -1;
	} else if ( !Q_stricmp( s, "follow2" ) ) {
		team	   = TEAM_SPECTATOR;
		specState  = SPECTATOR_FOLLOW;
		specClient = -2;
	} else if ( !Q_stricmp( s, "spectator" ) || !Q_stricmp( s, "s" ) ) {
		team	  = TEAM_SPECTATOR;
		specState = SPECTATOR_FREE;
	} else if ( g_gametype.integer >= GT_TDM ) {
		// if running a team game, assign player to one of the teams
		specState = SPECTATOR_NOT;
		if ( !Q_stricmp( s, "red" ) || !Q_stricmp( s, "r" ) ) {
			team = TEAM_RED;
		} else if ( !Q_stricmp( s, "blue" ) || !Q_stricmp( s, "b" ) ) {
			team = TEAM_BLUE;
		} else {
			// pick the team with the least number of players
			team = PickTeam( clientNum );
		}

		if ( g_teamForceBalance.integer && !client->pers.localClient && !( ent->r.svFlags & SVF_BOT ) ) {
			int counts[TEAM_NUM_TEAMS];

			counts[TEAM_BLUE] = TeamCount( clientNum, TEAM_BLUE );
			counts[TEAM_RED]  = TeamCount( clientNum, TEAM_RED );

			// We allow a spread of two
			if ( team == TEAM_RED && counts[TEAM_RED] - counts[TEAM_BLUE] > 1 ) {
				trap_SendServerCommand( clientNum, "cp \"Red team has too many players.\n\"" );
				return; // ignore the request
			}
			if ( team == TEAM_BLUE && counts[TEAM_BLUE] - counts[TEAM_RED] > 1 ) {
				trap_SendServerCommand( clientNum, "cp \"Blue team has too many players.\n\"" );
				return; // ignore the request
			}

			// It's ok, the team we are switching to has less or same number of players
		}

	} else {
		// force them to spectators if there aren't any spots free
		team = TEAM_FREE;
	}

	// override decision if limiting the players
	if ( ( g_gametype.integer == GT_DUEL ) && level.numNonSpectatorClients >= 2 ) {
		team = TEAM_SPECTATOR;
	} else if ( g_gametype.integer == GT_LASTMANSTANDING && level.warmupTime == 0 ) {
		team = TEAM_SPECTATOR;
	} else if ( g_maxGameClients.integer > 0 && level.numNonSpectatorClients >= g_maxGameClients.integer ) {
		team = TEAM_SPECTATOR;
	}

	//
	// decide if we will allow the change
	//
	oldTeam = client->sess.sessionTeam;
	if ( team == oldTeam && team != TEAM_SPECTATOR ) {
		return;
	}

	//
	// execute the team change
	//

	// if the player was dead leave the body, but only if they're actually in game
	if ( client->ps.stats[STAT_HEALTH] <= 0 && client->pers.connected == CON_CONNECTED ) {
		CopyToBodyQue( ent );
	}

	// he starts at 'base'
	client->pers.teamState.state = TEAM_BEGIN;
	if ( oldTeam != TEAM_SPECTATOR ) {
		// Kill him (makes sure he loses flags, etc)
		ent->flags &= ~FL_GODMODE;
		ent->client->ps.stats[STAT_HEALTH] = ent->health = 0;
		player_die( ent, ent, ent, 100000, MOD_SUICIDE );
	}

	// they go to the end of the line for tournements
	if ( team == TEAM_SPECTATOR && oldTeam != team )
		AddTournamentQueue( client );

	client->sess.sessionTeam	 = team;
	client->sess.spectatorState	 = specState;
	client->sess.spectatorClient = specClient;

	client->sess.teamLeader = qfalse;
	if ( team == TEAM_RED || team == TEAM_BLUE ) {
		teamLeader = TeamLeader( team );
		// if there is no team leader or the team leader is a bot and this client is not a bot
		if ( teamLeader == -1 ||
			 ( !( g_entities[clientNum].r.svFlags & SVF_BOT ) && ( g_entities[teamLeader].r.svFlags & SVF_BOT ) ) ) {
			SetLeader( team, clientNum );
		}
	}
	// make sure there is a team leader on the team the player came from
	if ( oldTeam == TEAM_RED || oldTeam == TEAM_BLUE ) {
		CheckTeamLeader( oldTeam );
	}

	// get and distribute relevant parameters
	ClientUserinfoChanged( clientNum );

	// client hasn't spawned yet, they sent an early team command, teampref userinfo, or g_autoJoin is enabled
	if ( client->pers.connected != CON_CONNECTED ) {
		return;
	}

	BroadcastTeamChange( client, oldTeam );

	ClientBegin( clientNum );
}

/*
=================
StopFollowing

If the client being followed leaves the game, or you just want to drop
to free floating spectator mode
=================
*/
void StopFollowing( gentity_t *ent )
{
	ent->client->ps.persistant[PERS_TEAM] = TEAM_SPECTATOR;
	ent->client->sess.sessionTeam		  = TEAM_SPECTATOR;
	ent->client->sess.spectatorState	  = SPECTATOR_FREE;
	ent->client->ps.pm_flags &= ~PMF_FOLLOW;
	ent->r.svFlags &= ~SVF_BOT;
	ent->client->ps.clientNum = ent - g_entities;

	SetClientViewAngle( ent, ent->client->ps.viewangles );

	// don't use dead view angles
	if ( ent->client->ps.stats[STAT_HEALTH] <= 0 ) {
		ent->client->ps.stats[STAT_HEALTH] = 1;
	}
}

/*
=================
Cmd_Team_f
=================
*/
void Cmd_Team_f( gentity_t *ent )
{
	int	 oldTeam;
	char s[MAX_TOKEN_CHARS];

	if ( trap_Argc() != 2 ) {
		oldTeam = ent->client->sess.sessionTeam;
		switch ( oldTeam ) {
		case TEAM_BLUE:
			trap_SendServerCommand( ent - g_entities, "print \"Blue team\n\"" );
			break;
		case TEAM_RED:
			trap_SendServerCommand( ent - g_entities, "print \"Red team\n\"" );
			break;
		case TEAM_FREE:
			trap_SendServerCommand( ent - g_entities, "print \"Free team\n\"" );
			break;
		case TEAM_SPECTATOR:
			trap_SendServerCommand( ent - g_entities, "print \"Spectator team\n\"" );
			break;
		}
		return;
	}

	if ( ent->client->switchTeamTime > level.time ) {
		trap_SendServerCommand( ent - g_entities, "print \"May not switch teams more than once per 5 seconds.\n\"" );
		return;
	}

	// if they are playing a duel game, count as a loss
	if ( ( g_gametype.integer == GT_DUEL ) && ent->client->sess.sessionTeam == TEAM_FREE ) {
		ent->client->sess.losses++;
	}

	trap_Argv( 1, s, sizeof( s ) );

	SetTeam( ent, s );

	ent->client->switchTeamTime = level.time + 5000;
}


/*
=================
Cmd_Follow_f
=================
*/
void Cmd_Follow_f( gentity_t *ent )
{
	int	 i;
	char arg[MAX_TOKEN_CHARS];

	if ( trap_Argc() != 2 ) {
		if ( ent->client->sess.spectatorState == SPECTATOR_FOLLOW ) {
			StopFollowing( ent );
		}
		return;
	}

	trap_Argv( 1, arg, sizeof( arg ) );
	i = ClientNumberFromString( ent, arg, qtrue, qtrue );
	if ( i == -1 ) {
		return;
	}

	// can't follow self
	if ( &level.clients[i] == ent->client ) {
		return;
	}

	// can't follow another spectator
	if ( level.clients[i].sess.sessionTeam == TEAM_SPECTATOR ) {
		return;
	}

	// if they are playing a duel game, count as a loss
	if ( ( g_gametype.integer == GT_DUEL ) && ent->client->sess.sessionTeam == TEAM_FREE ) {
		ent->client->sess.losses++;
	}

	// first set them to spectator
	if ( ent->client->sess.sessionTeam != TEAM_SPECTATOR ) {
		SetTeam( ent, "spectator" );
	}

	ent->client->sess.spectatorState  = SPECTATOR_FOLLOW;
	ent->client->sess.spectatorClient = i;
}

/*
=================
Cmd_Stop_f
=================
*/
#if FEAT_SCREENSHOT_TOOLS
void Cmd_Stop_f( gentity_t *ent )
{
	if ( ent->s.number != 0 )
		return;
	if ( ent->client->sess.sessionTeam != TEAM_SPECTATOR )
		return;

	if ( level.stopTime ) {
		level.stopTime = 0;
	} else {
		level.stopTime = level.time;
	}
	trap_SetConfigstring( CS_STOPTIME, va( "%d", level.stopTime ) );
}
#endif

/*
=================
Cmd_FollowCycle_f
=================
*/
void Cmd_FollowCycle_f( gentity_t *ent, int dir )
{
	int clientnum;
	int original;

	// if they are playing a duel game, count as a loss
	if ( ( g_gametype.integer == GT_DUEL ) && ent->client->sess.sessionTeam == TEAM_FREE ) {
		ent->client->sess.losses++;
	}
	// first set them to spectator
	if ( ent->client->sess.spectatorState == SPECTATOR_NOT ) {
		SetTeam( ent, "spectator" );
	}

	if ( dir != 1 && dir != -1 ) {
		Com_Terminate( TERM_CLIENT_DROP, "Cmd_FollowCycle_f: bad dir %i", dir );
	}

	// if dedicated follow client, just switch between the two auto clients
	if ( ent->client->sess.spectatorClient < 0 ) {
		if ( ent->client->sess.spectatorClient == -1 ) {
			ent->client->sess.spectatorClient = -2;
		} else if ( ent->client->sess.spectatorClient == -2 ) {
			ent->client->sess.spectatorClient = -1;
		}
		return;
	}

	clientnum = ent->client->sess.spectatorClient;
	original  = clientnum;
	do {
		clientnum += dir;
		if ( clientnum >= level.maxclients ) {
			clientnum = 0;
		}
		if ( clientnum < 0 ) {
			clientnum = level.maxclients - 1;
		}

		// can only follow connected clients
		if ( level.clients[clientnum].pers.connected != CON_CONNECTED ) {
			continue;
		}

		// can't follow another spectator
		if ( level.clients[clientnum].sess.sessionTeam == TEAM_SPECTATOR ) {
			continue;
		}

		// this is good, we can use it
		ent->client->sess.spectatorClient = clientnum;
		ent->client->sess.spectatorState  = SPECTATOR_FOLLOW;
		return;
	} while ( clientnum != original );

	// leave it where it was
}


/*
==================
G_Say
==================
*/

static void G_SayTo( gentity_t *ent, gentity_t *other, int mode, int color, const char *name, const char *message )
{
	if ( !other ) {
		return;
	}
	if ( !other->inuse ) {
		return;
	}
	if ( !other->client ) {
		return;
	}
	if ( other->client->pers.connected != CON_CONNECTED ) {
		return;
	}
	if ( mode == SAY_TEAM && !OnSameTeam( ent, other ) ) {
		return;
	}
	// no chatting to players in duels
	if ( ( g_gametype.integer == GT_DUEL ) && other->client->sess.sessionTeam == TEAM_FREE &&
		 ent->client->sess.sessionTeam != TEAM_FREE ) {
		return;
	}

	trap_SendServerCommand( other - g_entities, va( "%s \"%s%c%c%s\" %d", mode == SAY_TEAM ? "tchat" : "chat", name,
													Q_COLOR_ESCAPE, color, message, (int)( ent - g_entities ) ) );
}

#define EC "\x19"

void G_Say( gentity_t *ent, gentity_t *target, int mode, const char *chatText )
{
	int		   j;
	gentity_t *other;
	int		   color;
	// don't let text be too long for malicious reasons
	char text[MAX_SAY_TEXT];
	char location[64];
	// hold the netname + location + colour-escape prefix without truncating
	// the SAY_TEAM/SAY_TELL location string on maps with long location names
	char name[MAX_NETNAME + sizeof( location ) + 16];

	if ( !g_gametypeIsTeamGame && mode == SAY_TEAM ) {
		mode = SAY_ALL;
	}

	switch ( mode ) {
	default:
	case SAY_ALL:
		G_LogPrintf( "say: %s: %s\n", ent->client->pers.netname, chatText );
		Com_sprintf( name, sizeof( name ), "%s%c%c" EC ": ", ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE );
		color = COLOR_GREEN;
		break;
	case SAY_TEAM:
		G_LogPrintf( "sayteam: %s: %s\n", ent->client->pers.netname, chatText );
		if ( Team_GetLocationMsg( ent, location, sizeof( location ) ) )
			Com_sprintf( name, sizeof( name ), EC "(%s%c%c" EC ") (%s)" EC ": ", ent->client->pers.netname,
						 Q_COLOR_ESCAPE, COLOR_WHITE, location );
		else
			Com_sprintf( name, sizeof( name ), EC "(%s%c%c" EC ")" EC ": ", ent->client->pers.netname, Q_COLOR_ESCAPE,
						 COLOR_WHITE );
		color = COLOR_CYAN;
		break;
	case SAY_TELL:
		if ( target && target->inuse && target->client && g_gametype.integer >= GT_TDM &&
			 target->client->sess.sessionTeam == ent->client->sess.sessionTeam &&
			 Team_GetLocationMsg( ent, location, sizeof( location ) ) )
			Com_sprintf( name, sizeof( name ), EC "[%s%c%c" EC "] (%s)" EC ": ", ent->client->pers.netname,
						 Q_COLOR_ESCAPE, COLOR_WHITE, location );
		else
			Com_sprintf( name, sizeof( name ), EC "[%s%c%c" EC "]" EC ": ", ent->client->pers.netname, Q_COLOR_ESCAPE,
						 COLOR_WHITE );
		color = COLOR_MAGENTA;
		break;
	}

	Q_strncpyz( text, chatText, sizeof( text ) );

	// WiredIntel: process @-addressed directives and colorize mentions before relay
	if ( text[0] == '@' ) {
		wbParseResult_t wbResult;
		WiredIntel_ProcessChat( ent->s.number, text, &wbResult );
		if ( wbResult.hasMentions ) {
			char colorized[MAX_SAY_TEXT];
			WiredIntel_ColorizeMentions( text, colorized, sizeof( colorized ), wbResult.recipientMention,
										 wbResult.targetMention );
			Q_strncpyz( text, colorized, sizeof( text ) );
		}
	}

	if ( target ) {
		G_SayTo( ent, target, mode, color, name, text );
		return;
	}

	// echo the text to the console
	if ( G_ServerIsConsoleOnly() ) {
		Com_Log( SEV_INFO, LOG_CH( ch_game ), "%s%s\n", name, text );
	}

	// send it to all the appropriate clients
	for ( j = 0; j < level.maxclients; j++ ) {
		other = &g_entities[j];
		G_SayTo( ent, other, mode, color, name, text );
	}
}

static void SanitizeChatText( char *text )
{
	for ( int i = 0; text[i]; i++ ) {
		if ( text[i] == '\n' || text[i] == '\r' ) {
			text[i] = ' ';
		}
	}
}


/*
==================
Cmd_Say_f
==================
*/
static void Cmd_Say_f( gentity_t *ent, int mode, qboolean arg0 )
{
	char *p;

	if ( trap_Argc() < 2 && !arg0 ) {
		return;
	}

	if ( arg0 ) {
		p = ConcatArgs( 0 );
	} else {
		p = ConcatArgs( 1 );
	}

	SanitizeChatText( p );

#if FEAT_WIREDNET_OBSERVER
	// WiredNet event: chat message
	trap_WiredNet_EmitChat( ent->s.number, p, ( mode == SAY_TEAM ) );
#endif

	G_Say( ent, NULL, mode, p );
}

/*
==================
Cmd_Tell_f
==================
*/
static void Cmd_Tell_f( gentity_t *ent )
{
	int		   targetNum;
	gentity_t *target;
	char	  *p;
	char	   arg[MAX_TOKEN_CHARS];

	if ( trap_Argc() < 3 ) {
		trap_SendServerCommand( ent - g_entities, "print \"Usage: tell <player id> <message>\n\"" );
		return;
	}

	trap_Argv( 1, arg, sizeof( arg ) );
	targetNum = ClientNumberFromString( ent, arg, qtrue, qtrue );
	if ( targetNum == -1 ) {
		return;
	}

	target = &g_entities[targetNum];
	if ( !target->inuse || !target->client ) {
		return;
	}

	p = ConcatArgs( 2 );

	SanitizeChatText( p );

	G_LogPrintf( "tell: %s to %s: %s\n", ent->client->pers.netname, target->client->pers.netname, p );
	G_Say( ent, target, SAY_TELL, p );
	// don't tell to the player self if it was already directed to this player
	// also don't send the chat back to a bot
	if ( ent != target && !( ent->r.svFlags & SVF_BOT ) ) {
		G_Say( ent, ent, SAY_TELL, p );
	}
}


#if FEAT_TA_VOICECHAT
static void G_VoiceTo( gentity_t *ent, gentity_t *other, int mode, const char *id, qboolean voiceonly )
{
	int	  color;
	char *cmd;

	if ( !other ) {
		return;
	}
	if ( !other->inuse ) {
		return;
	}
	if ( !other->client ) {
		return;
	}
	if ( mode == SAY_TEAM && !OnSameTeam( ent, other ) ) {
		return;
	}
	// no chatting to players in duels
	if ( g_gametype.integer == GT_DUEL ) {
		return;
	}

	if ( mode == SAY_TEAM ) {
		color = COLOR_CYAN;
		cmd	  = "vtchat";
	} else if ( mode == SAY_TELL ) {
		color = COLOR_MAGENTA;
		cmd	  = "vtell";
	} else {
		color = COLOR_GREEN;
		cmd	  = "vchat";
	}

	trap_SendServerCommand( other - g_entities, va( "%s %d %d %d %s", cmd, voiceonly, ent->s.number, color, id ) );
}

void G_Voice( gentity_t *ent, gentity_t *target, int mode, const char *id, qboolean voiceonly )
{
	int		   j;
	gentity_t *other;

	if ( !g_gametypeIsTeamGame && mode == SAY_TEAM ) {
		mode = SAY_ALL;
	}

	if ( target ) {
		G_VoiceTo( ent, target, mode, id, voiceonly );
		return;
	}

	// echo the text to the console
	if ( G_ServerIsConsoleOnly() ) {
		Com_Log( SEV_INFO, LOG_CH( ch_game ), "voice: %s %s\n", ent->client->pers.netname, id );
	}

	// send it to all the appropriate clients
	for ( j = 0; j < level.maxclients; j++ ) {
		other = &g_entities[j];
		G_VoiceTo( ent, other, mode, id, voiceonly );
	}
}

/*
==================
Cmd_Voice_f
==================
*/
static void Cmd_Voice_f( gentity_t *ent, int mode, qboolean arg0, qboolean voiceonly )
{
	char *p;

	if ( trap_Argc() < 2 && !arg0 ) {
		return;
	}

	if ( arg0 ) {
		p = ConcatArgs( 0 );
	} else {
		p = ConcatArgs( 1 );
	}

	SanitizeChatText( p );

	G_Voice( ent, NULL, mode, p, voiceonly );
}

/*
==================
Cmd_VoiceTell_f
==================
*/
static void Cmd_VoiceTell_f( gentity_t *ent, qboolean voiceonly )
{
	int		   targetNum;
	gentity_t *target;
	char	  *id;
	char	   arg[MAX_TOKEN_CHARS];

	if ( trap_Argc() < 3 ) {
		trap_SendServerCommand( ent - g_entities,
								va( "print \"Usage: %s <player id> <voice id>\n\"", voiceonly ? "votell" : "vtell" ) );
		return;
	}

	trap_Argv( 1, arg, sizeof( arg ) );
	targetNum = ClientNumberFromString( ent, arg, qtrue, qtrue );
	if ( targetNum == -1 ) {
		return;
	}

	target = &g_entities[targetNum];
	if ( !target->inuse || !target->client ) {
		return;
	}

	id = ConcatArgs( 2 );

	SanitizeChatText( id );

	G_LogPrintf( "vtell: %s to %s: %s\n", ent->client->pers.netname, target->client->pers.netname, id );
	G_Voice( ent, target, SAY_TELL, id, voiceonly );
	// don't tell to the player self if it was already directed to this player
	// also don't send the chat back to a bot
	if ( ent != target && !( ent->r.svFlags & SVF_BOT ) ) {
		G_Voice( ent, ent, SAY_TELL, id, voiceonly );
	}
}


/*
==================
Cmd_VoiceTaunt_f
==================
*/
static void Cmd_VoiceTaunt_f( gentity_t *ent )
{
	gentity_t *who;

	if ( !ent->client ) {
		return;
	}

	// insult someone who just killed you
	if ( ent->enemy && ent->enemy->client && ent->enemy->client->lastkilled_client == ent->s.number ) {
		// i am a dead corpse
		if ( !( ent->enemy->r.svFlags & SVF_BOT ) ) {
			G_Voice( ent, ent->enemy, SAY_TELL, VOICECHAT_DEATHINSULT, qfalse );
		}
		if ( !( ent->r.svFlags & SVF_BOT ) ) {
			G_Voice( ent, ent, SAY_TELL, VOICECHAT_DEATHINSULT, qfalse );
		}
		ent->enemy = NULL;
		return;
	}
	// insult someone you just killed
	if ( ent->client->lastkilled_client >= 0 && ent->client->lastkilled_client != ent->s.number ) {
		who = g_entities + ent->client->lastkilled_client;
		if ( who->client ) {
			// who is the person I just killed
			if ( who->client->lasthurt_mod == MOD_GAUNTLET || who->client->lasthurt_mod == MOD_GAUNTLET_LUNGE ) {
				if ( !( who->r.svFlags & SVF_BOT ) ) {
					G_Voice( ent, who, SAY_TELL, VOICECHAT_KILLGAUNTLET, qfalse ); // and I killed them with a gauntlet
				}
				if ( !( ent->r.svFlags & SVF_BOT ) ) {
					G_Voice( ent, ent, SAY_TELL, VOICECHAT_KILLGAUNTLET, qfalse );
				}
			} else {
				if ( !( who->r.svFlags & SVF_BOT ) ) {
					G_Voice( ent, who, SAY_TELL, VOICECHAT_KILLINSULT,
							 qfalse ); // and I killed them with something else
				}
				if ( !( ent->r.svFlags & SVF_BOT ) ) {
					G_Voice( ent, ent, SAY_TELL, VOICECHAT_KILLINSULT, qfalse );
				}
			}
			ent->client->lastkilled_client = -1;
			return;
		}
	}

	if ( g_gametype.integer >= GT_TDM ) {
		// praise a team mate who just got a reward
		for ( int i = 0; i < MAX_CLIENTS; i++ ) {
			who = g_entities + i;
			if ( who->client && who != ent && who->client->sess.sessionTeam == ent->client->sess.sessionTeam ) {
				if ( who->client->rewardTime > level.time ) {
					if ( !( who->r.svFlags & SVF_BOT ) ) {
						G_Voice( ent, who, SAY_TELL, VOICECHAT_PRAISE, qfalse );
					}
					if ( !( ent->r.svFlags & SVF_BOT ) ) {
						G_Voice( ent, ent, SAY_TELL, VOICECHAT_PRAISE, qfalse );
					}
					return;
				}
			}
		}
	}

	// just say something
	G_Voice( ent, NULL, SAY_ALL, VOICECHAT_TAUNT, qfalse );
}
#endif


static char *gc_orders[] = { "hold your position", "hold this position", "come here", "cover me",
							 "guard location",	   "search and destroy", "report" };

static const int numgc_orders = ARRAY_LEN( gc_orders );

void Cmd_GameCommand_f( gentity_t *ent )
{
	int		   targetNum;
	gentity_t *target;
	int		   order;
	char	   arg[MAX_TOKEN_CHARS];

	if ( trap_Argc() != 3 ) {
		trap_SendServerCommand( ent - g_entities,
								va( "print \"Usage: gc <player id> <order 0-%d>\n\"", numgc_orders - 1 ) );
		return;
	}

	trap_Argv( 2, arg, sizeof( arg ) );
	order = atoi( arg );

	if ( order < 0 || order >= numgc_orders ) {
		trap_SendServerCommand( ent - g_entities, va( "print \"Bad order: %i\n\"", order ) );
		return;
	}

	trap_Argv( 1, arg, sizeof( arg ) );
	targetNum = ClientNumberFromString( ent, arg, qtrue, qtrue );
	if ( targetNum == -1 ) {
		return;
	}

	target = &g_entities[targetNum];
	if ( !target->inuse || !target->client ) {
		return;
	}

	G_LogPrintf( "tell: %s to %s: %s\n", ent->client->pers.netname, target->client->pers.netname, gc_orders[order] );
	G_Say( ent, target, SAY_TELL, gc_orders[order] );
	// don't tell to the player self if it was already directed to this player
	// also don't send the chat back to a bot
	if ( ent != target && !( ent->r.svFlags & SVF_BOT ) ) {
		G_Say( ent, ent, SAY_TELL, gc_orders[order] );
	}
}

/*
==================
Cmd_Where_f
==================
*/
void Cmd_Where_f( gentity_t *ent )
{
	trap_SendServerCommand( ent - g_entities, va( "print \"%s\n\"", vtos( ent->r.currentOrigin ) ) );
}

/*
==================
G_MapExist

Returns qtrue when maps/<mapname>.bsp can be opened on the server — the file
the "map <name>" command will load. Used to reject a vote for a missing map.
==================
*/
static qboolean G_MapExist( const char *mapname )
{
	char expanded[MAX_QPATH];

	if ( !mapname || !mapname[0] ) {
		return qfalse;
	}
	Com_sprintf( expanded, sizeof( expanded ), "maps/%s.bsp", mapname );
	return G_FileExists( expanded );
}

/*
==================
Cmd_CallVote_f
==================
*/
void Cmd_CallVote_f( gentity_t *ent )
{
	char *c;
	int	  i;
	char  arg1[MAX_STRING_TOKENS];
	char  arg2[MAX_STRING_TOKENS];

	if ( !g_allowVote.integer ) {
		trap_SendServerCommand( ent - g_entities, "print \"Voting not allowed here.\n\"" );
		return;
	}

	if ( level.voteTime ) {
		trap_SendServerCommand( ent - g_entities, "print \"A vote is already in progress.\n\"" );
		return;
	}
	if ( ent->client->pers.voteCount >= MAX_VOTE_COUNT ) {
		trap_SendServerCommand( ent - g_entities, "print \"You have called the maximum number of votes.\n\"" );
		return;
	}

	// eser - callvote cooldown
	if ( ent->client->pers.lastVoteTime && level.time - ent->client->pers.lastVoteTime < 30000 ) {
		trap_SendServerCommand( ent - g_entities,
								va( "print \"Wait %i seconds before calling another vote.\n\"",
									( 30000 - ( level.time - ent->client->pers.lastVoteTime ) ) / 1000 ) );
		return;
	}
	// eser - callvote cooldown

	if ( ent->client->sess.sessionTeam == TEAM_SPECTATOR ) {
		trap_SendServerCommand( ent - g_entities, "print \"Not allowed to call a vote as spectator.\n\"" );
		return;
	}

	// make sure it is a valid command to vote on
	trap_Argv( 1, arg1, sizeof( arg1 ) );
	trap_Argv( 2, arg2, sizeof( arg2 ) );

	// check for command separators in arg2
	for ( c = arg2; *c; ++c ) {
		switch ( *c ) {
		case '\n':
		case '\r':
		case ';':
			trap_SendServerCommand( ent - g_entities, "print \"Invalid vote string.\n\"" );
			return;
			break;
		}
	}

	if ( !Q_stricmp( arg1, "map_restart" ) ) {
	} else if ( !Q_stricmp( arg1, "nextmap" ) ) {
	} else if ( !Q_stricmp( arg1, "map" ) ) {
		// reject a vote for a map the server cannot load, before building voteString
		if ( !G_MapExist( arg2 ) ) {
			trap_SendServerCommand( ent - g_entities, va( "print \"Map '%s' not found on server.\n\"", arg2 ) );
			return;
		}
	} else if ( !Q_stricmp( arg1, "g_gametype" ) ) {
	} else if ( !Q_stricmp( arg1, "kick" ) ) {
	} else if ( !Q_stricmp( arg1, "clientkick" ) ) {
	} else if ( !Q_stricmp( arg1, "g_minPlayers" ) ) {
	} else if ( !Q_stricmp( arg1, "g_scorelimit" ) ) {
	} else if ( !Q_stricmp( arg1, "g_timelimit" ) ) {
		// eser - team shuffle command
	} else if ( !Q_stricmp( arg1, "shuffle" ) ) {
		// eser - team shuffle command
		// eser - vote g_unlagged
	} else if ( !Q_stricmp( arg1, "g_unlagged" ) ) {
		// eser - vote g_unlagged
#if FEAT_ATMOSPHERIC
	} else if ( !Q_stricmp( arg1, "weather" ) ) {
#endif
	} else {
		trap_SendServerCommand( ent - g_entities, "print \"Invalid vote string.\n\"" );
		trap_SendServerCommand(
			ent - g_entities,
			"print \"Vote commands are: map_restart, nextmap, map <mapname>, g_gametype <n>, kick <player>, clientkick "
			"<clientnum>, g_minPlayers <n>, g_scorelimit <frags>, g_timelimit <time>, shuffle, g_unlagged <0|1>"
#if FEAT_ATMOSPHERIC
			" and weather <rain|snow|sleet|hail|dust|ash|cold|fog|storm|clean>"
#endif
			".\n\"" );
		return;
	}

	// if there is still a vote to be executed
	if ( level.voteExecuteTime ) {
		// don't start a vote when map change or restart is in progress
		if ( !Q_stricmpn( level.voteString, "map", 3 ) || !Q_stricmpn( level.voteString, "nextmap", 7 ) ) {
			trap_SendServerCommand( ent - g_entities, "print \"Vote after map change.\n\"" );
			return;
		}

		level.voteExecuteTime = 0;
		trap_SendConsoleCommand( EXEC_APPEND, va( "%s\n", level.voteString ) );
	}

	// special case for g_gametype, check for bad values
	if ( !Q_stricmp( arg1, "g_gametype" ) ) {
		i = atoi( arg2 );
		if ( i < GT_DEATHMATCH || i >= GT_MAX_GAME_TYPE ) {
			trap_SendServerCommand( ent - g_entities, "print \"Invalid gametype.\n\"" );
			return;
		}

		Com_sprintf( level.voteString, sizeof( level.voteString ), "%s %d", arg1, i );
		Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "%s %s", arg1,
					 bg_gametypelist[i].name );
	} else if ( !Q_stricmp( arg1, "map" ) ) {
		// special case for map changes, we want to reset the nextmap setting
		// this allows a player to change maps, but not upset the map rotation
		char s[MAX_STRING_CHARS];

		trap_Cvar_VariableStringBuffer( "nextmap", s, sizeof( s ) );
		if ( *s ) {
			Com_sprintf( level.voteString, sizeof( level.voteString ), "%s %s; set nextmap \"%s\"", arg1, arg2, s );
		} else {
			Com_sprintf( level.voteString, sizeof( level.voteString ), "%s %s", arg1, arg2 );
		}
		Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "%s", level.voteString );
	} else if ( !Q_stricmp( arg1, "nextmap" ) ) {
		char s[MAX_STRING_CHARS];

		trap_Cvar_VariableStringBuffer( "nextmap", s, sizeof( s ) );
		if ( !*s ) {
			trap_SendServerCommand( ent - g_entities, "print \"nextmap not set.\n\"" );
			return;
		}
		Com_sprintf( level.voteString, sizeof( level.voteString ), "vstr nextmap" );
		Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "%s", level.voteString );
	} else if ( !Q_stricmp( arg1, "clientkick" ) || !Q_stricmp( arg1, "kick" ) ) {
		i = ClientNumberFromString( ent, arg2, !Q_stricmp( arg1, "clientkick" ), !Q_stricmp( arg1, "kick" ) );
		if ( i == -1 ) {
			return;
		}

		if ( level.clients[i].pers.localClient ) {
			trap_SendServerCommand( ent - g_entities, "print \"Cannot kick host player.\n\"" );
			return;
		}

		Com_sprintf( level.voteString, sizeof( level.voteString ), "clientkick %d", i );
		Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "kick %s",
					 level.clients[i].pers.netname );
		// eser - team shuffle command
	} else if ( !Q_stricmp( arg1, "shuffle" ) ) {
		Com_sprintf( level.voteString, sizeof( level.voteString ), "shuffle" );
		Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "Shuffle Teams" );
		// eser - team shuffle command
		// eser - vote g_unlagged
	} else if ( !Q_stricmp( arg1, "g_unlagged" ) ) {
		i = atoi( arg2 );
		if ( !arg2[0] || ( i != 0 && i != 1 ) ) {
			trap_SendServerCommand( ent - g_entities, "print \"Valid g_unlagged values: 0 or 1.\n\"" );
			return;
		}
		Com_sprintf( level.voteString, sizeof( level.voteString ), "g_unlagged %d", i );
		Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "%s Unlagged",
					 i ? "Enable" : "Disable" );
		// eser - vote g_unlagged
#if FEAT_ATMOSPHERIC
	} else if ( !Q_stricmp( arg1, "weather" ) ) {
		if ( !arg2[0] || !Q_stricmp( arg2, "clean" ) ) {
			Com_sprintf( level.voteString, sizeof( level.voteString ), "g_envWeather \"\"" );
			Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "Weather: Clean" );
		} else if ( !Q_stricmp( arg2, "rain" ) ) {
			Com_sprintf( level.voteString, sizeof( level.voteString ), "g_envWeather \"rain\"" );
			Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "Weather: Rain" );
		} else if ( !Q_stricmp( arg2, "snow" ) ) {
			Com_sprintf( level.voteString, sizeof( level.voteString ), "g_envWeather \"snow\"" );
			Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "Weather: Snow" );
		} else if ( !Q_stricmp( arg2, "sleet" ) ) {
			Com_sprintf( level.voteString, sizeof( level.voteString ), "g_envWeather \"sleet\"" );
			Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "Weather: Sleet" );
		} else if ( !Q_stricmp( arg2, "hail" ) ) {
			Com_sprintf( level.voteString, sizeof( level.voteString ), "g_envWeather \"hail\"" );
			Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "Weather: Hail" );
		} else if ( !Q_stricmp( arg2, "dust" ) || !Q_stricmp( arg2, "ash" ) ) {
			Com_sprintf( level.voteString, sizeof( level.voteString ), "g_envWeather \"%s\"", arg2 );
			Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "Weather: %s", arg2 );
		} else if ( !Q_stricmp( arg2, "cold" ) ) {
			Com_sprintf( level.voteString, sizeof( level.voteString ), "g_envWeather \"cold\"" );
			Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "Atmosphere: Cold" );
		} else if ( !Q_stricmp( arg2, "fog" ) ) {
			Com_sprintf( level.voteString, sizeof( level.voteString ), "g_envWeather \"fog\"" );
			Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "Atmosphere: Fog" );
		} else if ( !Q_stricmp( arg2, "storm" ) ) {
			Com_sprintf( level.voteString, sizeof( level.voteString ), "g_envWeather \"storm\"" );
			Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "Atmosphere: Storm" );
		} else {
			trap_SendServerCommand(
				ent - g_entities,
				"print \"Valid weather types: rain, snow, sleet, hail, dust, ash, cold, fog, storm, clean.\n\"" );
			return;
		}
#endif
	} else {
		Com_sprintf( level.voteString, sizeof( level.voteString ), "%s \"%s\"", arg1, arg2 );
		Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "%s", level.voteString );
	}

	trap_SendServerCommand(
		-1, va( "print \"" S_COLOR_GREEN "%s" S_COLOR_WHITE " called a vote.\n\"", ent->client->pers.netname ) );

	// eser - callvote cooldown
	ent->client->pers.lastVoteTime = level.time;
	// eser - callvote cooldown

	// start the voting, the caller automatically votes yes
	level.voteTime = level.time;
	level.voteYes  = 1;
	level.voteNo   = 0;

	for ( i = 0; i < level.maxclients; i++ ) {
		level.clients[i].ps.eFlags &= ~EF_VOTED;
	}
	ent->client->ps.eFlags |= EF_VOTED;

	trap_SetConfigstring( CS_VOTE_TIME, va( "%i", level.voteTime ) );
	trap_SetConfigstring( CS_VOTE_STRING, level.voteDisplayString );
	trap_SetConfigstring( CS_VOTE_YES, va( "%i", level.voteYes ) );
	trap_SetConfigstring( CS_VOTE_NO, va( "%i", level.voteNo ) );
}

/*
==================
Cmd_Vote_f
==================
*/
void Cmd_Vote_f( gentity_t *ent )
{
	char msg[64];

	if ( !level.voteTime ) {
		trap_SendServerCommand( ent - g_entities, "print \"No vote in progress.\n\"" );
		return;
	}
	if ( ent->client->ps.eFlags & EF_VOTED ) {
		trap_SendServerCommand( ent - g_entities, "print \"Vote already cast.\n\"" );
		return;
	}
	if ( ent->client->sess.sessionTeam == TEAM_SPECTATOR ) {
		trap_SendServerCommand( ent - g_entities, "print \"Not allowed to vote as spectator.\n\"" );
		return;
	}

	trap_SendServerCommand( ent - g_entities, "print \"Vote cast.\n\"" );

	ent->client->ps.eFlags |= EF_VOTED;

	trap_Argv( 1, msg, sizeof( msg ) );

	if ( tolower( msg[0] ) == 'y' || msg[0] == '1' ) {
		level.voteYes++;
		trap_SetConfigstring( CS_VOTE_YES, va( "%i", level.voteYes ) );
	} else {
		level.voteNo++;
		trap_SetConfigstring( CS_VOTE_NO, va( "%i", level.voteNo ) );
	}

	// a majority will be determined in CheckVote, which will also account
	// for players entering or leaving
}

/*
==================
Cmd_CallTeamVote_f
==================
*/
void Cmd_CallTeamVote_f( gentity_t *ent )
{
	char *c;
	int	  i, team, cs_offset;
	char  arg1[MAX_STRING_TOKENS];
	char  arg2[MAX_STRING_TOKENS];

	team = ent->client->sess.sessionTeam;
	if ( team == TEAM_RED )
		cs_offset = 0;
	else if ( team == TEAM_BLUE )
		cs_offset = 1;
	else
		return;

	if ( !g_allowVote.integer ) {
		trap_SendServerCommand( ent - g_entities, "print \"Voting not allowed here.\n\"" );
		return;
	}

	if ( level.teamVoteTime[cs_offset] ) {
		trap_SendServerCommand( ent - g_entities, "print \"A team vote is already in progress.\n\"" );
		return;
	}
	if ( ent->client->pers.teamVoteCount >= MAX_VOTE_COUNT ) {
		trap_SendServerCommand( ent - g_entities, "print \"You have called the maximum number of team votes.\n\"" );
		return;
	}
	if ( ent->client->sess.sessionTeam == TEAM_SPECTATOR ) {
		trap_SendServerCommand( ent - g_entities, "print \"Not allowed to call a vote as spectator.\n\"" );
		return;
	}

	// make sure it is a valid command to vote on
	trap_Argv( 1, arg1, sizeof( arg1 ) );
	arg2[0] = '\0';
	for ( i = 2; i < trap_Argc(); i++ ) {
		if ( i > 2 )
			strcat( arg2, " " );
		trap_Argv( i, &arg2[strlen( arg2 )], sizeof( arg2 ) - strlen( arg2 ) );
	}

	// check for command separators in arg2
	for ( c = arg2; *c; ++c ) {
		switch ( *c ) {
		case '\n':
		case '\r':
		case ';':
			trap_SendServerCommand( ent - g_entities, "print \"Invalid vote string.\n\"" );
			return;
			break;
		}
	}

	if ( !Q_stricmp( arg1, "leader" ) ) {
		char netname[MAX_NETNAME], leader[MAX_NETNAME];

		if ( !arg2[0] ) {
			i = ent->client->ps.clientNum;
		} else {
			// numeric values are just slot numbers
			for ( i = 0; i < 3; i++ ) {
				if ( !arg2[i] || arg2[i] < '0' || arg2[i] > '9' )
					break;
			}
			if ( i >= 3 || !arg2[i] ) {
				i = atoi( arg2 );
				if ( i < 0 || i >= level.maxclients ) {
					trap_SendServerCommand( ent - g_entities, va( "print \"Bad client slot: %i\n\"", i ) );
					return;
				}

				if ( !g_entities[i].inuse ) {
					trap_SendServerCommand( ent - g_entities, va( "print \"Client %i is not active\n\"", i ) );
					return;
				}
			} else {
				Q_strncpyz( leader, arg2, sizeof( leader ) );
				Q_CleanStr( leader );
				for ( i = 0; i < level.maxclients; i++ ) {
					if ( level.clients[i].pers.connected == CON_DISCONNECTED )
						continue;
					if ( level.clients[i].sess.sessionTeam != team )
						continue;
					Q_strncpyz( netname, level.clients[i].pers.netname, sizeof( netname ) );
					Q_CleanStr( netname );
					if ( !Q_stricmp( netname, leader ) ) {
						break;
					}
				}
				if ( i >= level.maxclients ) {
					trap_SendServerCommand( ent - g_entities,
											va( "print \"%s is not a valid player on your team.\n\"", arg2 ) );
					return;
				}
			}
		}
		Com_sprintf( arg2, sizeof( arg2 ), "%d", i );
	} else {
		trap_SendServerCommand( ent - g_entities, "print \"Invalid vote string.\n\"" );
		trap_SendServerCommand( ent - g_entities, "print \"Team vote commands are: leader <player>.\n\"" );
		return;
	}

	Com_sprintf( level.teamVoteString[cs_offset], sizeof( level.teamVoteString[cs_offset] ), "%s %s", arg1, arg2 );

	for ( i = 0; i < level.maxclients; i++ ) {
		if ( level.clients[i].pers.connected == CON_DISCONNECTED )
			continue;
		if ( level.clients[i].sess.sessionTeam == team )
			trap_SendServerCommand( i, va( "print \"" S_COLOR_GREEN "%s" S_COLOR_WHITE " called a team vote.\n\"",
										   ent->client->pers.netname ) );
	}

	// start the voting, the caller automatically votes yes
	level.teamVoteTime[cs_offset] = level.time;
	level.teamVoteYes[cs_offset]  = 1;
	level.teamVoteNo[cs_offset]	  = 0;

	for ( i = 0; i < level.maxclients; i++ ) {
		if ( level.clients[i].sess.sessionTeam == team )
			level.clients[i].ps.eFlags &= ~EF_TEAMVOTED;
	}
	ent->client->ps.eFlags |= EF_TEAMVOTED;

	trap_SetConfigstring( CS_TEAMVOTE_TIME + cs_offset, va( "%i", level.teamVoteTime[cs_offset] ) );
	trap_SetConfigstring( CS_TEAMVOTE_STRING + cs_offset, level.teamVoteString[cs_offset] );
	trap_SetConfigstring( CS_TEAMVOTE_YES + cs_offset, va( "%i", level.teamVoteYes[cs_offset] ) );
	trap_SetConfigstring( CS_TEAMVOTE_NO + cs_offset, va( "%i", level.teamVoteNo[cs_offset] ) );
}

/*
==================
Cmd_TeamVote_f
==================
*/
void Cmd_TeamVote_f( gentity_t *ent )
{
	int	 team, cs_offset;
	char msg[64];

	team = ent->client->sess.sessionTeam;
	if ( team == TEAM_RED )
		cs_offset = 0;
	else if ( team == TEAM_BLUE )
		cs_offset = 1;
	else
		return;

	if ( !level.teamVoteTime[cs_offset] ) {
		trap_SendServerCommand( ent - g_entities, "print \"No team vote in progress.\n\"" );
		return;
	}
	if ( ent->client->ps.eFlags & EF_TEAMVOTED ) {
		trap_SendServerCommand( ent - g_entities, "print \"Team vote already cast.\n\"" );
		return;
	}
	if ( ent->client->sess.sessionTeam == TEAM_SPECTATOR ) {
		trap_SendServerCommand( ent - g_entities, "print \"Not allowed to vote as spectator.\n\"" );
		return;
	}

	trap_SendServerCommand( ent - g_entities, "print \"Team vote cast.\n\"" );

	ent->client->ps.eFlags |= EF_TEAMVOTED;

	trap_Argv( 1, msg, sizeof( msg ) );

	if ( tolower( msg[0] ) == 'y' || msg[0] == '1' ) {
		level.teamVoteYes[cs_offset]++;
		trap_SetConfigstring( CS_TEAMVOTE_YES + cs_offset, va( "%i", level.teamVoteYes[cs_offset] ) );
	} else {
		level.teamVoteNo[cs_offset]++;
		trap_SetConfigstring( CS_TEAMVOTE_NO + cs_offset, va( "%i", level.teamVoteNo[cs_offset] ) );
	}

	// a majority will be determined in TeamCheckVote, which will also account
	// for players entering or leaving
}


/*
=================
Cmd_SetViewpos_f
=================
*/
void Cmd_SetViewpos_f( gentity_t *ent )
{
	vec3_t origin, angles;
	char   buffer[MAX_TOKEN_CHARS];
	int	   i;

	if ( !g_cheats.integer ) {
		trap_SendServerCommand( ent - g_entities, "print \"Cheats are not enabled on this server.\n\"" );
		return;
	}
	// 5 args = x y z yaw; an optional 6th sets pitch (look up/down), used by the
	// automated shadow gates to aim the test camera at the floor.
	if ( trap_Argc() != 5 && trap_Argc() != 6 ) {
		trap_SendServerCommand( ent - g_entities, "print \"usage: setviewpos x y z yaw [pitch]\n\"" );
		return;
	}

	VectorClear( angles );
	for ( i = 0; i < 3; i++ ) {
		trap_Argv( i + 1, buffer, sizeof( buffer ) );
		origin[i] = atof( buffer );
	}

	trap_Argv( 4, buffer, sizeof( buffer ) );
	angles[YAW] = atof( buffer );

	if ( trap_Argc() == 6 ) {
		trap_Argv( 5, buffer, sizeof( buffer ) );
		angles[PITCH] = atof( buffer );
	}

	TeleportPlayer( ent, origin, angles, 400 );
}

#if FEAT_RECAST_NAVMESH
/*
=================
Cmd_NavWalkTest_f

Dev-only: walk a virtual point from the player's current origin to a goal
<x y z> over the client-independent nav seam, to prove the steering core paths a
non-client agent end-to-end. Reports whether the point reached the goal and how
many steps it took. Cheat-gated; does nothing on a live server.
=================
*/
static void Cmd_NavWalkTest_f( gentity_t *ent )
{
	vec3_t	 goal;
	char	 buffer[MAX_TOKEN_CHARS];
	int		 i, steps = 0;
	qboolean reached;

	if ( !g_cheats.integer ) {
		trap_SendServerCommand( ent - g_entities, "print \"Cheats are not enabled on this server.\n\"" );
		return;
	}
	if ( trap_Argc() != 4 ) {
		trap_SendServerCommand( ent - g_entities, "print \"usage: nav_walktest x y z\n\"" );
		return;
	}

	for ( i = 0; i < 3; i++ ) {
		trap_Argv( i + 1, buffer, sizeof( buffer ) );
		goal[i] = atof( buffer );
	}

	reached = BotNav_WalkTest( ent->client->ps.origin, goal, &steps );

	trap_SendServerCommand( ent - g_entities, va( "print \"walktest: %s in %d steps\n\"",
												  reached ? "reached goal" : "unreachable/stuck", steps ) );
}

/*
=================
Cmd_NavSpawnFollower_f

Dev-only: spawn a non-client nav-follower at the player's origin and send it to
a goal <x y z>. The follower walks the navmesh via the trajectory runner (no
client slot, no AI — movement only). Cheat-gated.
=================
*/
static void Cmd_NavSpawnFollower_f( gentity_t *ent )
{
	vec3_t	   goal, spawnOrigin;
	char	   buffer[MAX_TOKEN_CHARS];
	int		   i, agentType = 0;
	gentity_t *follower;

	if ( !g_cheats.integer ) {
		trap_SendServerCommand( ent - g_entities, "print \"Cheats are not enabled on this server.\n\"" );
		return;
	}
	if ( trap_Argc() != 4 && trap_Argc() != 5 ) {
		trap_SendServerCommand( ent - g_entities,
								"print \"usage: nav_spawnfollower x y z [agentType 0=player 1=small 2=large]\n\"" );
		return;
	}

	for ( i = 0; i < 3; i++ ) {
		trap_Argv( i + 1, buffer, sizeof( buffer ) );
		goal[i] = atof( buffer );
	}
	if ( trap_Argc() == 5 ) {
		trap_Argv( 4, buffer, sizeof( buffer ) );
		agentType = atoi( buffer );
	}

	follower = G_Spawn();
	if ( !follower ) {
		trap_SendServerCommand( ent - g_entities, "print \"nav-follower: no free entity\n\"" );
		return;
	}
	follower->classname = "nav_follower";
	VectorCopy( ent->client->ps.origin, spawnOrigin );
	VectorCopy( spawnOrigin, follower->s.pos.trBase );
	VectorCopy( spawnOrigin, follower->r.currentOrigin );
	VectorSet( follower->r.mins, -15, -15, -24 );
	VectorSet( follower->r.maxs, 15, 15, 32 );

	if ( !Nav_StartFollower( follower, goal, agentType ) ) {
		trap_SendServerCommand( ent - g_entities, "print \"nav-follower: could not start (pool full)\n\"" );
		G_FreeEntity( follower );
		return;
	}

	trap_LinkEntity( follower );
	trap_SendServerCommand( ent - g_entities, "print \"nav-follower: spawned, walking to goal\n\"" );
}
#endif

#if FEAT_MONSTER_AI
/*
=================
G_SpawnBehaviorMonster

Spawn one non-client behavior monster at `origin`, seed `enemyNum` as its enemy,
and start its FSM (aiThink). If characterHandle > 0, bind it to that character
via the monster Lua trap; the bind return tells us whether the character carries
a Lua "decide" override (2) or not (1), which sets luaDecide so the FSM either
consults Lua for state selection (opt-in) or stays pure C. Returns the entity, or
NULL if no free entity / behavior pool full.
=================
*/
gentity_t *G_SpawnBehaviorMonster( const vec3_t origin, int enemyNum, int startHealth, int characterHandle,
								   const char *characterName )
{
	gentity_t *mob = G_Spawn();
	if ( !mob )
		return NULL;

	// One name drives the monster's whole identity: its primary model and collision hull
	// come from the character manifest, while characters/<name>/tag carries the explicit
	// render identity to cgame. A caller that passes no name gets the soldier; a caller
	// that passes "dog" gets the dog with zero asset-path knowledge in the game module.
	const char *monsterName = ( characterName && characterName[0] ) ? characterName : "soldier";

	mob->classname = "monster_behavior";
	mob->s.eType   = ET_GENERAL;
	VectorCopy( origin, mob->s.pos.trBase );
	VectorCopy( origin, mob->r.currentOrigin );
	mob->s.pos.trType = TR_STATIONARY;

	// Collision hull: read the monster's own size from its character manifest (model.bbox).
	// A creature carries its own size — a dog is wider and lower than a soldier. This
	// default human-ish hull is only a safety net for a monster whose manifest omits bbox;
	// the shipped monsters (soldier, dog, …) each declare their own, so the default is
	// overwritten below. (Keep it equal to the soldier's historical hull for safety.)
	VectorSet( mob->r.mins, -15, -15, -24 );
	VectorSet( mob->r.maxs, 15, 15, 32 );
	{
		char   key[MAX_QPATH];
		char   buf[128];
		vec3_t mins, maxs;
		Com_sprintf( key, sizeof( key ), "char:%s:bbox", monsterName );
		if ( trap_GetValue( buf, sizeof( buf ), key ) &&
			 sscanf( buf, "%f %f %f %f %f %f", &mins[0], &mins[1], &mins[2], &maxs[0], &maxs[1], &maxs[2] ) == 6 ) {
			VectorCopy( mins, mob->r.mins );
			VectorCopy( maxs, mob->r.maxs );
		}
	}

#if FEAT_RECAST_NAVMESH
	// Movement mode: a flyer/swimmer steers in 3D and is not ground-clamped. Read the
	// manifest's `movement` string once and resolve it to the nav-follower's mode; absent
	// or unrecognized → ground (every existing monster is unchanged). String→enum here,
	// never per frame.
	mob->navMovement = NAVMOVE_GROUND;
	{
		char key[MAX_QPATH];
		char buf[16];
		Com_sprintf( key, sizeof( key ), "char:%s:movement", monsterName );
		if ( trap_GetValue( buf, sizeof( buf ), key ) && buf[0] ) {
			if ( !Q_stricmp( buf, "fly" ) ) {
				mob->navMovement = NAVMOVE_FLY;
			} else if ( !Q_stricmp( buf, "swim" ) ) {
				mob->navMovement = NAVMOVE_SWIM;
			} else if ( Q_stricmp( buf, "ground" ) != 0 ) {
				Com_Log( SEV_DEBUG, LOG_CH( ch_game ), "monster '%s': unrecognized movement '%s' — using ground\n",
						 monsterName, buf );
			}
		}
	}
#endif

#if FEAT_MONSTER_AI
	// Attack mode: a ranged monster fires a projectile aimed in 3D instead of a melee
	// swing. Read the manifest's `attack` string once; absent or unrecognized → melee
	// (every existing monster is unchanged). String→enum here, never per frame.
	mob->attackMode = ATTACK_MELEE;
	{
		char key[MAX_QPATH];
		char buf[16];
		Com_sprintf( key, sizeof( key ), "char:%s:attack", monsterName );
		if ( trap_GetValue( buf, sizeof( buf ), key ) && buf[0] ) {
			if ( !Q_stricmp( buf, "ranged" ) ) {
				mob->attackMode = ATTACK_RANGED;
			} else if ( Q_stricmp( buf, "melee" ) != 0 ) {
				Com_Log( SEV_DEBUG, LOG_CH( ch_game ), "monster '%s': unrecognized attack '%s' — using melee\n",
						 monsterName, buf );
			}
		}
	}
#endif

	mob->clipmask	= MASK_PLAYERSOLID;
	mob->r.contents = CONTENTS_BODY;
	mob->takedamage = qtrue;
	mob->health		= startHealth < 1 ? 1 : startHealth;
	mob->die		= Behavior_MonsterDie; /* else G_Damage NULL-derefs on kill */

	// Publish the exact primary model selected from the merged character manifest. The
	// Q1 .mdl frame names still drive client-side MANIM derivation, but the authoritative
	// asset now has one canonical characters/ path instead of a duplicated package copy.
	{
		char modelPath[MAX_QPATH];
		char key[MAX_QPATH];
		Com_sprintf( key, sizeof( key ), "char:%s:primary_model", monsterName );
		if ( !trap_GetValue( modelPath, sizeof( modelPath ), key ) || !modelPath[0] ) {
			Com_Log( SEV_WARN, LOG_CH( ch_game ),
				"monster '%s': character manifest has no loadable primary model\n", monsterName );
			G_FreeEntity( mob );
			return NULL;
		}
		mob->s.modelindex = G_ModelIndex( modelPath );
	}
	mob->s.legsAnim = MANIM_STAND;

	// Name the character to the client for the character render path. The client reads
	// this CS_MODELS path back (via modelindex2) and derives the slug to load the
	// character's body; the primary modelindex above stays the .mdl so a creature with
	// no loadable character body still renders through the client's CG_General fallback.
	// modelindex2 is a pure data channel here — ET_GENERAL never draws a second model.
	{
		char tagPath[MAX_QPATH];
		Com_sprintf( tagPath, sizeof( tagPath ), "characters/%s/tag", monsterName );
		mob->s.modelindex2 = G_ModelIndex( tagPath );
	}

	if ( !Behavior_AcquireState( mob ) ) {
		G_FreeEntity( mob );
		return NULL;
	}

	mob->behaviorState->enemy		 = enemyNum;
	mob->behaviorState->state		 = BSTATE_IDLE; /* Decide() picks the real state */
	mob->behaviorState->nextThink	 = level.time;
	mob->behaviorState->stateEntered = level.time;
	mob->behaviorState->luaDecide	 = qfalse; /* pure C unless a Lua-fn binds */
	mob->behaviorState->difficultySkill =
		Com_Clamp( 1, 5, trap_Cvar_VariableIntegerValue( "g_skill" ) );

	// Optional Lua-decide opt-in: bind a character. The trap returns 2 when the
	// bound character carries a Lua "decide" override, 1 when it does not, 0 on
	// failure. Only a "2" flips the monster onto the Lua-decide path.
	if ( characterHandle > 0 ) {
		int bound					  = trap_MonsterLuaBind( mob->s.number, characterHandle );
		mob->behaviorState->luaDecide = ( bound == 2 );
	}

	mob->aiThink = qtrue;
	trap_LinkEntity( mob );

	// Census: this is the single chokepoint for all monster creation (map-placed,
	// console-spawned, script-spawned), so counting a successful spawn here is exact.
	level.numMonstersSpawned++;
	return mob;
}

/*
=================
Cmd_SpawnMonster_f

Dev-only: spawn one (or a count of) non-client behavior monster(s) in front of
the invoking player, seeded with the player as enemy, running the native FSM
(Battle/Hunt/TakeCover). Cheat-gated.

Usage: spawnmonster [name] [count] [startHealth]
  name          the monster to spawn (dog, knight, ... ; default soldier). Drives its
                model, character render, and collision size from characters/<name>.
  count         number of monsters (default 1)
  startHealth   starting health (default 100; low → immediate take-cover)
=================
*/
static void Cmd_SpawnMonster_f( gentity_t *ent )
{
	vec3_t fwd, base, origin;
	char   buffer[MAX_TOKEN_CHARS];
	char   charName[MAX_QPATH];
	int	   count = 1, startHealth = 100, i, spawned = 0;
	int	   characterHandle = 0;
	int	   argBase		   = 1; /* first numeric arg (shifts by 1 if a name leads) */

	charName[0] = '\0';

	if ( !g_cheats.integer ) {
		trap_SendServerCommand( ent - g_entities, "print \"Cheats are not enabled on this server.\n\"" );
		return;
	}

	// A leading non-numeric argument is the monster name (spawnmonster dog [count]
	// [health]); a leading number keeps the classic count-first form (spawnmonster 3).
	if ( trap_Argc() >= 2 ) {
		trap_Argv( 1, buffer, sizeof( buffer ) );
		if ( buffer[0] && !isdigit( (unsigned char)buffer[0] ) && buffer[0] != '-' ) {
			Q_strncpyz( charName, buffer, sizeof( charName ) );
			argBase = 2;

			// Optionally bind the same character for the Lua-decide FSM opt-in (a monster
			// whose bot/main.lua defines decide()). trap_BotLoadCharacter returns a NEGATIVE
			// handle for a Lua character; the monster bind wants the raw positive handle.
			{
				char path[MAX_QPATH];
				int	 botChar;
				Com_sprintf( path, sizeof( path ), "characters/%s/main.lua", charName );
				botChar = trap_BotLoadCharacter( path, 3.0f );
				if ( botChar < 0 ) {
					characterHandle = -botChar;
				}
			}
		}
	}

	if ( trap_Argc() >= argBase + 1 ) {
		trap_Argv( argBase, buffer, sizeof( buffer ) );
		count = atoi( buffer );
	}
	if ( trap_Argc() >= argBase + 2 ) {
		trap_Argv( argBase + 1, buffer, sizeof( buffer ) );
		startHealth = atoi( buffer );
	}
	if ( count < 1 )
		count = 1;

	// Base spawn point a little in front of the invoking player.
	AngleVectors( ent->client->ps.viewangles, fwd, NULL, NULL );
	fwd[2] = 0.0f;
	VectorNormalize( fwd );
	VectorMA( ent->client->ps.origin, 200.0f, fwd, base );

	for ( i = 0; i < count; i++ ) {
		// spread multiple monsters so they do not all telefrag on one point
		VectorCopy( base, origin );
		origin[0] += ( i % 10 ) * 40.0f;
		origin[1] += ( i / 10 ) * 40.0f;
		if ( G_SpawnBehaviorMonster( origin, ent->s.number, startHealth, characterHandle, charName ) ) {
			spawned++;
		}
	}

	trap_SendServerCommand( ent - g_entities,
							va( "print \"monster: spawned %d/%d (health %d, %s) — running FSM\n\"", spawned, count,
								startHealth, characterHandle > 0 ? "lua-bound" : "pure-C" ) );
}

/*
=================
Cmd_SpawnScript_f

Dev-only: drive a scripted monster from a set-piece script file. Drops a named
nav-marker in front of the player (the script's gotomarker target), then spawns
a scripted monster a little to the side and binds it to the script. The monster
runs the verb list — walk to the marker, hold, wait for player-sight, attack.
Cheat-gated.

Usage: spawnscript [scriptPath]   (default scripts/setpieces/boss_intro.script)
=================
*/
static void Cmd_SpawnScript_f( gentity_t *ent )
{
	vec3_t	   fwd, markerPos, monPos;
	char	   path[MAX_QPATH];
	gentity_t *marker, *mob;

	if ( !g_cheats.integer ) {
		trap_SendServerCommand( ent - g_entities, "print \"Cheats are not enabled on this server.\n\"" );
		return;
	}

	if ( trap_Argc() >= 2 ) {
		trap_Argv( 1, path, sizeof( path ) );
	} else {
		Q_strncpyz( path, "scripts/setpieces/boss_intro.script", sizeof( path ) );
	}

	AngleVectors( ent->client->ps.viewangles, fwd, NULL, NULL );
	fwd[2] = 0.0f;
	VectorNormalize( fwd );

	// Marker 400u ahead (the gotomarker destination); monster 150u ahead + offset.
	VectorMA( ent->client->ps.origin, 400.0f, fwd, markerPos );
	VectorMA( ent->client->ps.origin, 150.0f, fwd, monPos );
	monPos[1] += 60.0f;

	marker = G_Spawn();
	if ( marker ) {
		marker->classname  = "target_position";
		marker->targetname = "script_marker_a";
		VectorCopy( markerPos, marker->s.origin );
		VectorCopy( markerPos, marker->r.currentOrigin );
		trap_LinkEntity( marker );
	}

	// Optional 2nd arg: the character the scripted monster renders + sizes as (e.g.
	// "boss" for the Chthon encounter). Absent → the spawn's soldier default.
	{
		char		charArg[MAX_QPATH];
		const char *charName = NULL;
		if ( trap_Argc() >= 3 ) {
			trap_Argv( 2, charArg, sizeof( charArg ) );
			if ( charArg[0] )
				charName = charArg;
		}
		mob = Script_SpawnDriven( path, monPos, ent->s.number, charName );
	}
	if ( mob ) {
		trap_SendServerCommand( ent - g_entities,
								va( "print \"script: driving monster %d from '%s'\n\"", mob->s.number, path ) );
	} else {
		trap_SendServerCommand( ent - g_entities, va( "print \"script: failed to spawn from '%s'\n\"", path ) );
	}
}
#endif


/*
=================
Cmd_Stats_f
=================
*/
void Cmd_Stats_f( gentity_t *ent )
{
}

// eser - admin mode
/*
================
Cmd_Admin_f

Admin commands: /adm kick <player>, /adm mute <player>, /adm map <mapname>
Admin status is validated by comparing the "password" userinfo key
against g_adminPassword cvar.
================
*/
void Cmd_Admin_f( gentity_t *ent )
{
	char subcmd[MAX_TOKEN_CHARS];
	char arg[MAX_TOKEN_CHARS];
	int	 clientNum = ent->s.number;

	if ( !ent->client->isAdmin ) {
		trap_SendServerCommand( clientNum, "print \"You are not an admin.\n\"" );
		return;
	}

	if ( trap_Argc() < 2 ) {
		trap_SendServerCommand( clientNum, "print \"Usage: adm <kick|mute|map> <arg>\n\"" );
		return;
	}

	trap_Argv( 1, subcmd, sizeof( subcmd ) );
	trap_Argv( 2, arg, sizeof( arg ) );

	if ( Q_stricmp( subcmd, "kick" ) == 0 ) {
		int i;
		if ( !arg[0] ) {
			trap_SendServerCommand( clientNum, "print \"Usage: adm kick <player>\n\"" );
			return;
		}
		for ( i = 0; i < level.maxclients; i++ ) {
			if ( level.clients[i].pers.connected != CON_CONNECTED )
				continue;
			if ( Q_stricmp( level.clients[i].pers.netname, arg ) == 0 ) {
				trap_DropClient( i, "Kicked by admin" );
				trap_SendServerCommand(
					-1, va( "print \"%s" S_COLOR_WHITE " was kicked by admin.\n\"", level.clients[i].pers.netname ) );
				return;
			}
		}
		trap_SendServerCommand( clientNum, va( "print \"Player '%s' not found.\n\"", arg ) );
	} else if ( Q_stricmp( subcmd, "mute" ) == 0 ) {
		if ( !arg[0] ) {
			trap_SendServerCommand( clientNum, "print \"Usage: adm mute <player>\n\"" );
			return;
		}
		for ( int i = 0; i < level.maxclients; i++ ) {
			if ( level.clients[i].pers.connected != CON_CONNECTED )
				continue;
			if ( Q_stricmp( level.clients[i].pers.netname, arg ) == 0 ) {
				// mute via EF_TALK flag (prevents chat)
				g_entities[i].s.eFlags ^= EF_TALK;
				trap_SendServerCommand( -1, va( "print \"%s" S_COLOR_WHITE " was %s by admin.\n\"",
												level.clients[i].pers.netname,
												( g_entities[i].s.eFlags & EF_TALK ) ? "muted" : "unmuted" ) );
				return;
			}
		}
		trap_SendServerCommand( clientNum, va( "print \"Player '%s' not found.\n\"", arg ) );
	} else if ( Q_stricmp( subcmd, "map" ) == 0 ) {
		if ( !arg[0] ) {
			trap_SendServerCommand( clientNum, "print \"Usage: adm map <mapname>\n\"" );
			return;
		}
		trap_SendServerCommand( -1, va( "print \"Admin changing map to %s.\n\"", arg ) );
		trap_SendConsoleCommand( EXEC_APPEND, va( "map %s\n", arg ) );
	} else {
		trap_SendServerCommand( clientNum, "print \"Unknown admin command. Use: kick, mute, map\n\"" );
	}
}
// eser - admin mode

#if FEAT_RANKED_QUEUE
/*
================
Cmd_Queue_f

Player joins/leaves the matchmaking queue.
================
*/
void Cmd_Queue_f( gentity_t *ent )
{
	int clientNum = ent->s.number;

	if ( !g_ranked.integer ) {
		trap_SendServerCommand( clientNum, "print \"Ranked mode is not enabled.\n\"" );
		return;
	}

	ent->client->queued = !ent->client->queued;
	if ( ent->client->queued ) {
		trap_SendServerCommand( clientNum, "print \"You have joined the ranked queue.\n\"" );
		trap_SendServerCommand( -1,
								va( "print \"%s" S_COLOR_WHITE " joined the queue.\n\"", ent->client->pers.netname ) );
	} else {
		trap_SendServerCommand( clientNum, "print \"You have left the ranked queue.\n\"" );
	}
}
#endif // FEAT_RANKED_QUEUE

/*
=================
Cmd_BStats_f

Send per-attack stats for the requesting client.
Format: bstats <weaponBitmask> [<hits> <shots> <kills> <deaths> <damage>] per weapon
=================
*/
/* Send bstats for a specific source client TO a specific recipient */
static void G_SendBStats( int sourceClient, int recipientClient )
{
	char	   string[1400];
	int		   stringlength;
	int		   i;
	int		   weaponMask;
	gclient_t *cl;

	if ( sourceClient < 0 || sourceClient >= level.maxclients )
		return;
	cl = &level.clients[sourceClient];
	if ( cl->pers.connected != CON_CONNECTED )
		return;

	weaponMask = 0;
	for ( i = ATT_NONE + 1; i < ATT_NUM_ATTACKS; i++ ) {
		if ( cl->attackStats[i].shots || cl->attackStats[i].hits || cl->attackStats[i].kills ||
			 cl->attackStats[i].deaths || cl->attackStats[i].damage ) {
			weaponMask |= ( 1 << i );
		}
	}

	if ( !weaponMask )
		return;

	stringlength = Com_sprintf( string, sizeof( string ), "bstats %i %i", sourceClient, weaponMask );

	for ( i = ATT_NONE + 1; i < ATT_NUM_ATTACKS; i++ ) {
		if ( weaponMask & ( 1 << i ) ) {
			char entry[128];
			int	 len = Com_sprintf( entry, sizeof( entry ), " %i %i %i %i %i", cl->attackStats[i].hits,
									cl->attackStats[i].shots, cl->attackStats[i].kills, cl->attackStats[i].deaths,
									cl->attackStats[i].damage );
			if ( stringlength + len >= (int)sizeof( string ) )
				break;
			strcpy( string + stringlength, entry );
			stringlength += len;
		}
	}

	trap_SendServerCommand( recipientClient, string );
}

void Cmd_BStats_f( gentity_t *ent )
{
	if ( !ent->client )
		return;
	G_SendBStats( ent->s.number, ent - g_entities );
}

/*
=================
Cmd_BotSay_f
=================
Stateless-client-only command: routes a chat message to WiredIntel without
going through the normal G_Say path.  The message must start with '@'.
Regular clients attempting this command are rejected.
*/
static void Cmd_BotSay_f( gentity_t *ent )
{
	char			msg[MAX_SAY_TEXT];
	wbParseResult_t result;
	const char	   *args;

	if ( !ent->client || !ent->client->sess.isStatelessClient ) {
		trap_SendServerCommand( ent->s.number, "print \"bot_say: stateless client only\n\"" );
		return;
	}

	args = ConcatArgs( 1 );
	if ( !args || !args[0] ) {
		return;
	}
	Q_strncpyz( msg, args, sizeof( msg ) );

	WiredIntel_ProcessChat( ent->s.number, msg, &result );

	/* echo to sender so it can confirm the command was received */
	trap_SendServerCommand( ent->s.number, va( "print \"[cmd] %s\n\"", msg ) );
}

static void Cmd_HoldNext_f( gentity_t *ent )
{
	if ( !ent->client )
		return;
	G_HoldableAdvanceSelected( ent, 1 );
}

static void Cmd_HoldPrev_f( gentity_t *ent )
{
	if ( !ent->client )
		return;
	G_HoldableAdvanceSelected( ent, -1 );
}

/*
=================
ClientCommand
=================
*/
void ClientCommand( int clientNum )
{
	gentity_t *ent;
	char	   cmd[MAX_TOKEN_CHARS];

	ent = g_entities + clientNum;
	if ( !ent->client || ent->client->pers.connected != CON_CONNECTED ) {
		if ( ent->client && ent->client->pers.localClient ) {
			// Handle early team command sent by UI when starting a local
			// team play game.
			trap_Argv( 0, cmd, sizeof( cmd ) );
			if ( Q_stricmp( cmd, "team" ) == 0 ) {
				Cmd_Team_f( ent );
			}
		}
		return; // not fully in game yet
	}


	trap_Argv( 0, cmd, sizeof( cmd ) );

	if ( Q_stricmp( cmd, "say" ) == 0 ) {
		Cmd_Say_f( ent, SAY_ALL, qfalse );
		return;
	}
	if ( Q_stricmp( cmd, "say_team" ) == 0 ) {
		Cmd_Say_f( ent, SAY_TEAM, qfalse );
		return;
	}
	if ( Q_stricmp( cmd, "bot_say" ) == 0 ) {
		Cmd_BotSay_f( ent );
		return;
	}
	if ( Q_stricmp( cmd, "tell" ) == 0 ) {
		Cmd_Tell_f( ent );
		return;
	}
#if FEAT_TA_VOICECHAT
	if ( Q_stricmp( cmd, "vsay" ) == 0 ) {
		Cmd_Voice_f( ent, SAY_ALL, qfalse, qfalse );
		return;
	}
	if ( Q_stricmp( cmd, "vsay_team" ) == 0 ) {
		Cmd_Voice_f( ent, SAY_TEAM, qfalse, qfalse );
		return;
	}
	if ( Q_stricmp( cmd, "vtell" ) == 0 ) {
		Cmd_VoiceTell_f( ent, qfalse );
		return;
	}
	if ( Q_stricmp( cmd, "vosay" ) == 0 ) {
		Cmd_Voice_f( ent, SAY_ALL, qfalse, qtrue );
		return;
	}
	if ( Q_stricmp( cmd, "vosay_team" ) == 0 ) {
		Cmd_Voice_f( ent, SAY_TEAM, qfalse, qtrue );
		return;
	}
	if ( Q_stricmp( cmd, "votell" ) == 0 ) {
		Cmd_VoiceTell_f( ent, qtrue );
		return;
	}
	if ( Q_stricmp( cmd, "vtaunt" ) == 0 ) {
		Cmd_VoiceTaunt_f( ent );
		return;
	}
#endif
	if ( Q_stricmp( cmd, "score" ) == 0 ) {
		Cmd_Score_f( ent );
		return;
	}
	if ( Q_stricmp( cmd, "bstats" ) == 0 ) {
		Cmd_BStats_f( ent );
		return;
	}

	// ignore all other commands when at intermission
	if ( level.intermissiontime ) {
		Cmd_Say_f( ent, qfalse, qtrue );
		return;
	}

	if ( Q_stricmp( cmd, "give" ) == 0 )
		Cmd_Give_f( ent );
	else if ( Q_stricmp( cmd, "god" ) == 0 )
		Cmd_God_f( ent );
	else if ( Q_stricmp( cmd, "cloak" ) == 0 )
		Cmd_Cloak_f( ent );
	else if ( Q_stricmp( cmd, "notarget" ) == 0 )
		Cmd_Notarget_f( ent );
	else if ( Q_stricmp( cmd, "noclip" ) == 0 )
		Cmd_Noclip_f( ent );
	else if ( Q_stricmp( cmd, "kill" ) == 0 )
		Cmd_Kill_f( ent );
	else if ( Q_stricmp( cmd, "teamtask" ) == 0 )
		Cmd_TeamTask_f( ent );
	else if ( Q_stricmp( cmd, "levelshot" ) == 0 )
		Cmd_LevelShot_f( ent );
	else if ( Q_stricmp( cmd, "follow" ) == 0 )
		Cmd_Follow_f( ent );
	else if ( Q_stricmp( cmd, "follownext" ) == 0 )
		Cmd_FollowCycle_f( ent, 1 );
	else if ( Q_stricmp( cmd, "followprev" ) == 0 )
		Cmd_FollowCycle_f( ent, -1 );
	else if ( Q_stricmp( cmd, "team" ) == 0 )
		Cmd_Team_f( ent );
	else if ( Q_stricmp( cmd, "where" ) == 0 )
		Cmd_Where_f( ent );
	else if ( Q_stricmp( cmd, "callvote" ) == 0 )
		Cmd_CallVote_f( ent );
	else if ( Q_stricmp( cmd, "vote" ) == 0 )
		Cmd_Vote_f( ent );
	else if ( Q_stricmp( cmd, "callteamvote" ) == 0 )
		Cmd_CallTeamVote_f( ent );
	else if ( Q_stricmp( cmd, "teamvote" ) == 0 )
		Cmd_TeamVote_f( ent );
	else if ( Q_stricmp( cmd, "gc" ) == 0 )
		Cmd_GameCommand_f( ent );
	else if ( Q_stricmp( cmd, "setviewpos" ) == 0 )
		Cmd_SetViewpos_f( ent );
#if FEAT_RECAST_NAVMESH
	else if ( Q_stricmp( cmd, "nav_walktest" ) == 0 )
		Cmd_NavWalkTest_f( ent );
	else if ( Q_stricmp( cmd, "nav_spawnfollower" ) == 0 )
		Cmd_NavSpawnFollower_f( ent );
#endif
#if FEAT_MONSTER_AI
	else if ( Q_stricmp( cmd, "spawnmonster" ) == 0 )
		Cmd_SpawnMonster_f( ent );
	else if ( Q_stricmp( cmd, "spawnscript" ) == 0 )
		Cmd_SpawnScript_f( ent );
#endif
	else if ( Q_stricmp( cmd, "stats" ) == 0 )
		Cmd_Stats_f( ent );
#if FEAT_PING_LOCATION
	else if ( Q_stricmp( cmd, "ping" ) == 0 ) {
		// ping location (4G): trace from view, broadcast to team
		if ( g_gametype.integer >= GT_TDM ) {
			vec3_t	   forward, right, up, muzzle, end;
			trace_t	   trace;
			gentity_t *ping;

			AngleVectors( ent->client->ps.viewangles, forward, right, up );
			CalcMuzzlePoint( ent, forward, right, up, muzzle );
			VectorMA( muzzle, 16384, forward, end );
			trap_Trace( &trace, muzzle, NULL, NULL, end, ent->s.number, MASK_SHOT );

			ping				   = G_TempEntity( trace.endpos, EV_PING_LOCATION );
			ping->s.otherEntityNum = ent->s.number;
			// restrict to team only
			ping->r.svFlags |= SVF_BROADCAST;
		} else {
			trap_SendServerCommand( clientNum, "print \"Ping is only available in team games.\n\"" );
		}
	}
#endif
#if FEAT_READY_UP
	else if ( Q_stricmp( cmd, "ready" ) == 0 ) {
		// ready-up (4E): toggle ready state during warmup
		if ( !g_startWhenReady.integer ) {
			trap_SendServerCommand( clientNum, "print \"Ready-up is not enabled on this server.\n\"" );
		} else if ( level.warmupTime == 0 ) {
			trap_SendServerCommand( clientNum, "print \"Match already started.\n\"" );
		} else {
			ent->client->ready = !ent->client->ready;
			trap_SendServerCommand( -1, va( "print \"%s" S_COLOR_WHITE " is %s.\n\"", ent->client->pers.netname,
											ent->client->ready ? "READY" : "NOT READY" ) );
			G_SendReadymask( -1 );
		}
	}
#endif
#if FEAT_TOURNAMENT_PAUSE
	else if ( Q_stricmp( cmd, "pause" ) == 0 ) {
		// tournament pause (10C): pause the game
		if ( !g_allowTimeout.integer ) {
			trap_SendServerCommand( clientNum, "print \"Timeouts are not enabled on this server.\n\"" );
		} else if ( level.intermissiontime ) {
			trap_SendServerCommand( clientNum, "print \"Cannot pause during intermission.\n\"" );
		} else if ( level.paused ) {
			trap_SendServerCommand( clientNum, "print \"Game is already paused.\n\"" );
		} else {
			level.paused	= qtrue;
			level.pauseTime = level.time;
			trap_SendServerCommand( -1, "cp \"Game Paused\"" );
			trap_SendServerCommand(
				-1, va( "print \"%s" S_COLOR_WHITE " paused the game.\n\"", ent->client->pers.netname ) );
		}
	} else if ( Q_stricmp( cmd, "unpause" ) == 0 ) {
		// tournament pause (10C): resume the game
		if ( !level.paused ) {
			trap_SendServerCommand( clientNum, "print \"Game is not paused.\n\"" );
		} else {
			level.paused	= qfalse;
			level.pauseTime = 0;
			trap_SendServerCommand( -1, "cp \"Game Resumed\"" );
			trap_SendServerCommand(
				-1, va( "print \"%s" S_COLOR_WHITE " resumed the game.\n\"", ent->client->pers.netname ) );
		}
	}
#endif
	else if ( Q_stricmp( cmd, "holdnext" ) == 0 )
		Cmd_HoldNext_f( ent );
	else if ( Q_stricmp( cmd, "holdprev" ) == 0 )
		Cmd_HoldPrev_f( ent );
	// eser - admin mode
	else if ( Q_stricmp( cmd, "adm" ) == 0 )
		Cmd_Admin_f( ent );
// eser - admin mode
#if FEAT_DROP_ITEMS
	else if ( Q_stricmp( cmd, "drop" ) == 0 )
		Cmd_Drop_f( ent );
#endif
#if FEAT_RANKED_QUEUE
	else if ( Q_stricmp( cmd, "queue" ) == 0 )
		Cmd_Queue_f( ent );
#endif
#if FEAT_SCREENSHOT_TOOLS
	else if ( Q_stricmp( cmd, "stop" ) == 0 )
		Cmd_Stop_f( ent );
#endif
	else
		trap_SendServerCommand( clientNum, va( "print \"unknown cmd %s\n\"", cmd ) );
}
