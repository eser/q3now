// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// cg_consolecmds.c -- text commands typed in at the local console, or
// executed by a key binding

#include "cg_local.h"
LOG_DECLARE_CHANNEL( ch_cgame, "cgame" );



void CG_TargetCommand_f( void ) {
	int		targetNum;
	char	test[4];

	targetNum = CG_CrosshairPlayer();
	if ( targetNum == -1 ) {
		return;
	}

	trap_Argv( 1, test, 4 );
	trap_SendClientCommand( va( "gc %i %i", targetNum, atoi( test ) ) );
}


/*
=============
CG_Viewpos_f

Debugging command to print the current camera position + look angles.

The value line is the bare tuple "x y z yaw pitch" (five space-separated numbers,
no parens/colon/prefix) — exactly the argument order Cmd_SetViewpos_f parses
(arg1-3 = origin, arg4 = yaw, arg5 = pitch). So the printed line feeds straight
into `setviewpos` by copy-paste, and is positionally indexable for scripting
(viewpos[0..4]).
=============
*/
static void CG_Viewpos_f (void) {
	Com_Log( SEV_INFO, LOG_CH(ch_cgame), "%i %i %i %i %i\n",
		(int)cg.refdef.vieworg[0],
		(int)cg.refdef.vieworg[1],
		(int)cg.refdef.vieworg[2],
		(int)cg.refdefViewAngles[YAW],
		(int)cg.refdefViewAngles[PITCH] );
}

/*
=================
CG_SceneLoadAndStart

Shared cinematic-scene start sequence, invoked by the sceneplay command (and its
testscene dev alias). `path` is the resolved scripts/scene/<name>.lua path.
Loads the .lua engine-side and POD-ships the parsed wiredScene_t into
cg.sceneDef (trap ships a CLEAN copy every call — re-playing is safe), then
Starts playback on the cg.time clock. Start is destructive (reparameterizes the
eye path in place), so the fresh copy each call keeps it the single Start.
=================
*/
// `actorEntityNums`/`nActors` bind live entities to the scene's look-at target
// slots (positional: actor i -> target slot i); pass NULL/0 for a plain scene
// (byte-identical). Non-static so the "scene" server-command crossing shares it.
void CG_SceneLoadAndStart( const char *path, const int *actorEntityNums, int nActors ) {
	if ( !trap_WiredSceneLoad( path, &cg.sceneDef ) ) {
		Com_Log( SEV_WARN, LOG_CH(ch_cgame), "scene: failed to load '%s'\n", path );
		return;
	}

	// Start on the same cg.time clock the per-frame Eval samples (elapsed =
	// nowMs - startMs). The freshly-shipped sceneDef is a clean copy.
	WiredScenePlayback_Start( &cg.scenePlayback, &cg.sceneDef, cg.time );
	if ( actorEntityNums && nActors > 0 ) {
		WiredScenePlayback_BindActors( &cg.scenePlayback, actorEntityNums, nActors );
	}
	Com_Log( SEV_INFO, LOG_CH(ch_cgame),
		"scene: playing '%s' (%d knots, %.1fs, %d events, %d actors)\n",
		path, cg.sceneDef.eyePath.numKnots, cg.sceneDef.totalTimeSec, cg.sceneDef.numEvents, nActors );
}

/*
=================
CG_ScenePlay_f  (sceneplay <path>)

The cgame target of the scene.play(name) Lua trigger. Receives the already-resolved
scripts/scene/<name>.lua path (the scene binding builds it). Also reachable as the
testscene dev alias.
=================
*/
static void CG_ScenePlay_f( void ) {
	if ( trap_Argc() < 2 ) {
		Com_Log( SEV_INFO, LOG_CH(ch_cgame), "usage: sceneplay <path>\n" );
		return;
	}
	CG_SceneLoadAndStart( CG_Argv( 1 ), NULL, 0 );   /* console play: no actor binding */
}

/*
=================
CG_SceneStop_f  (scenestop)

The cgame target of scene.stop(). Ends the cinematic now by clearing the gate
CG_SceneActive reads; the normal player view resumes next frame. Hard cut.
=================
*/
static void CG_SceneStop_f( void ) {
	cg.scenePlayback.active = 0;
}

/*
=================
CG_SceneSkip_f  (sceneskip)

The cgame target of scene.skip(). Hard cut today (identical to scenestop) — the
authored end-transition signals are logged-only, so a soft skip has no
observable effect yet. Kept a DISTINCT command so a later upgrade (advance the
clock past the path end to fire the authored end path) is a body-only change.
=================
*/
static void CG_SceneSkip_f( void ) {
	cg.scenePlayback.active = 0;
}


static void CG_ScoresDown_f( void ) {

#if FEAT_TA_UI
		CG_BuildSpectatorString();
#endif
	if ( cg.scoresRequestTime + 2000 < cg.time ) {
		// the scores are more than two seconds out of data,
		// so request new ones
		cg.scoresRequestTime = cg.time;
		trap_SendClientCommand( "score" );

		// leave the current scores up if they were already
		// displayed, but if this is the first hit, clear them out
		if ( !cg.showScores ) {
			cg.showScores = qtrue;
			cg.numScores = 0;
		}
	} else {
		// show the cached contents even if they just pressed if it
		// is within two seconds
		cg.showScores = qtrue;
	}
}

static void CG_ScoresUp_f( void ) {
	if ( cg.showScores ) {
		cg.showScores = qfalse;
		cg.scoreFadeTime = cg.time;
	}
}

static void CG_spWin_f( void) {
    trap_Cvar_Set("cg_cameraOrbit", "2");
    trap_Cvar_Set("cg_cameraOrbitDelay", "35");
    trap_Cvar_Set("cg_thirdPerson", "1");
    trap_Cvar_Set("cg_thirdPersonAngle", "0");
    trap_Cvar_Set("cg_thirdPersonRange", "100");
    //CG_AddBufferedSound(cgs.media.winnerSound);
    //trap_S_StartLocalSound(cgs.media.winnerSound, CHAN_ANNOUNCER);
    CG_CenterPrint("YOU WIN!", 144, 0);
}

static void CG_spLose_f( void) {
    trap_Cvar_Set("cg_cameraOrbit", "2");
    trap_Cvar_Set("cg_cameraOrbitDelay", "35");
    trap_Cvar_Set("cg_thirdPerson", "1");
    trap_Cvar_Set("cg_thirdPersonAngle", "0");
    trap_Cvar_Set("cg_thirdPersonRange", "100");
    //CG_AddBufferedSound(cgs.media.loserSound);
    //trap_S_StartLocalSound(cgs.media.loserSound, CHAN_ANNOUNCER);
    CG_CenterPrint("YOU LOSE...", 144, 0);
}

static void CG_TellTarget_f( void ) {
	int		clientNum;
	char	command[128];
	char	message[128];

	clientNum = CG_CrosshairPlayer();
	if ( clientNum == -1 ) {
		return;
	}

	trap_Args( message, 128 );
	Com_sprintf( command, 128, "tell %i %s", clientNum, message );
	trap_SendClientCommand( command );
}


#if FEAT_TA_UI
static void CG_VoiceTellTarget_f( void ) {
	int		clientNum;
	char	command[128];
	char	message[128];

	clientNum = CG_CrosshairPlayer();
	if ( clientNum == -1 ) {
		return;
	}

	trap_Args( message, 128 );
	Com_sprintf( command, 128, "vtell %i %s", clientNum, message );
	trap_SendClientCommand( command );
}

static void CG_NextTeamMember_f( void ) {
  CG_SelectNextPlayer();
}

static void CG_PrevTeamMember_f( void ) {
  CG_SelectPrevPlayer();
}

// ASS U ME's enumeration order as far as task specific orders, OFFENSE is zero, CAMP is last
//
static void CG_NextOrder_f( void ) {
	clientInfo_t *ci = cgs.clientinfo + cg.snap->ps.clientNum;
	if (ci) {
		if (!ci->teamLeader && sortedTeamPlayers[cg_currentSelectedPlayer.integer] != cg.snap->ps.clientNum) {
			return;
		}
	}
	if (cgs.currentOrder < TEAMTASK_CAMP) {
		cgs.currentOrder++;

		if (cgs.currentOrder == TEAMTASK_RETRIEVE) {
			if (!CG_OtherTeamHasFlag()) {
				cgs.currentOrder++;
			}
		}

		if (cgs.currentOrder == TEAMTASK_ESCORT) {
			if (!CG_YourTeamHasFlag()) {
				cgs.currentOrder++;
			}
		}

	} else {
		cgs.currentOrder = TEAMTASK_OFFENSE;
	}
	cgs.orderPending = qtrue;
	cgs.orderTime = cg.time + 3000;
}


static void CG_ConfirmOrder_f (void ) {
	trap_SendConsoleCommand(va("cmd vtell %d %s\n", cgs.acceptLeader, VOICECHAT_YES));
	trap_SendConsoleCommand("+button5; wait; -button5");
	if (cg.time < cgs.acceptOrderTime) {
		trap_SendClientCommand(va("teamtask %d\n", cgs.acceptTask));
		cgs.acceptOrderTime = 0;
	}
}

static void CG_DenyOrder_f (void ) {
	trap_SendConsoleCommand(va("cmd vtell %d %s\n", cgs.acceptLeader, VOICECHAT_NO));
	trap_SendConsoleCommand("+button6; wait; -button6");
	if (cg.time < cgs.acceptOrderTime) {
		cgs.acceptOrderTime = 0;
	}
}

static void CG_TaskOffense_f (void ) {
	if (cgs.gametype == GT_CTF || cgs.gametype == GT_1FCTF) {
		trap_SendConsoleCommand(va("cmd vsay_team %s\n", VOICECHAT_ONGETFLAG));
	} else {
		trap_SendConsoleCommand(va("cmd vsay_team %s\n", VOICECHAT_ONOFFENSE));
	}
	trap_SendClientCommand(va("teamtask %d\n", TEAMTASK_OFFENSE));
}

static void CG_TaskDefense_f (void ) {
	trap_SendConsoleCommand(va("cmd vsay_team %s\n", VOICECHAT_ONDEFENSE));
	trap_SendClientCommand(va("teamtask %d\n", TEAMTASK_DEFENSE));
}

static void CG_TaskPatrol_f (void ) {
	trap_SendConsoleCommand(va("cmd vsay_team %s\n", VOICECHAT_ONPATROL));
	trap_SendClientCommand(va("teamtask %d\n", TEAMTASK_PATROL));
}

static void CG_TaskCamp_f (void ) {
	trap_SendConsoleCommand(va("cmd vsay_team %s\n", VOICECHAT_ONCAMPING));
	trap_SendClientCommand(va("teamtask %d\n", TEAMTASK_CAMP));
}

static void CG_TaskFollow_f (void ) {
	trap_SendConsoleCommand(va("cmd vsay_team %s\n", VOICECHAT_ONFOLLOW));
	trap_SendClientCommand(va("teamtask %d\n", TEAMTASK_FOLLOW));
}

static void CG_TaskRetrieve_f (void ) {
	trap_SendConsoleCommand(va("cmd vsay_team %s\n", VOICECHAT_ONRETURNFLAG));
	trap_SendClientCommand(va("teamtask %d\n", TEAMTASK_RETRIEVE));
}

static void CG_TaskEscort_f (void ) {
	trap_SendConsoleCommand(va("cmd vsay_team %s\n", VOICECHAT_ONFOLLOWCARRIER));
	trap_SendClientCommand(va("teamtask %d\n", TEAMTASK_ESCORT));
}

static void CG_TaskOwnFlag_f (void ) {
	trap_SendConsoleCommand(va("cmd vsay_team %s\n", VOICECHAT_IHAVEFLAG));
}

static void CG_TauntKillInsult_f (void ) {
	trap_SendConsoleCommand("cmd vsay kill_insult\n");
}

static void CG_TauntPraise_f (void ) {
	trap_SendConsoleCommand("cmd vsay praise\n");
}

static void CG_TauntTaunt_f (void ) {
	trap_SendConsoleCommand("cmd vtaunt\n");
}

static void CG_TauntDeathInsult_f (void ) {
	trap_SendConsoleCommand("cmd vsay death_insult\n");
}

static void CG_TauntGauntlet_f (void ) {
	trap_SendConsoleCommand("cmd vsay kill_gauntlet\n");
}

static void CG_TaskSuicide_f (void ) {
	int		clientNum;
	char	command[128];

	clientNum = CG_CrosshairPlayer();
	if ( clientNum == -1 ) {
		return;
	}

	Com_sprintf( command, 128, "tell %i suicide", clientNum );
	trap_SendClientCommand( command );
}



/*
==================
CG_TeamMenu_f
==================
*/
/*
static void CG_TeamMenu_f( void ) {
  if (trap_Key_GetCatcher() & KEYCATCH_CGAME) {
    CG_EventHandling(CGAME_EVENT_NONE);
    trap_Key_SetCatcher(0);
  } else {
    CG_EventHandling(CGAME_EVENT_TEAMMENU);
    //trap_Key_SetCatcher(KEYCATCH_CGAME);
  }
}
*/

/*
==================
CG_EditHud_f
==================
*/
/*
static void CG_EditHud_f( void ) {
  //cls.keyCatchers ^= KEYCATCH_CGAME;
  //VM_Call (cgvm, CG_EVENT_HANDLING, (cls.keyCatchers & KEYCATCH_CGAME) ? CGAME_EVENT_EDITHUD : CGAME_EVENT_NONE);
}
*/

#endif

/*
==================
CG_StartOrbit_f
==================
*/

#if FEAT_THIRD_PERSON
static void CG_ThirdPersonDown_f( void ) {
	if ( cg.thirdPersonHeld ) {
		return;
	}
	cg.thirdPersonHeld = qtrue;
}
static void CG_ThirdPersonUp_f( void ) {
	if ( !cg.thirdPersonHeld ) {
		return;
	}
	cg.thirdPersonHeld = qfalse;
}
#endif

static void CG_StartOrbit_f( void ) {
	if (cg_cameraOrbit.value != 0) {
		trap_Cvar_Set ("cg_cameraOrbit", "0");
		trap_Cvar_Set("cg_thirdPerson", "0");
	} else {
		trap_Cvar_Set("cg_cameraOrbit", "5");
		trap_Cvar_Set("cg_thirdPerson", "1");
		trap_Cvar_Set("cg_thirdPersonAngle", "0");
		trap_Cvar_Set("cg_thirdPersonRange", "100");
	}
}

/* Exercises the production semantic cgame -> client WiredFX path without
 * duplicating any recipe choreography in the harness. */
static void CG_WiredFxTestRocket_f( void ) {
	vec3_t origin;
	vec3_t normal;
	impactSound_t material = IMPACTSOUND_DEFAULT;
	qboolean freeAir = qfalse;
	qboolean underwater = qfalse;
	const char *argument = CG_Argv( 1 );

	if ( !Q_stricmp( argument, "metal" ) ) material = IMPACTSOUND_METAL;
	else if ( !Q_stricmp( argument, "flesh" ) ) material = IMPACTSOUND_FLESH;
	else if ( !Q_stricmp( argument, "air" ) ) freeAir = qtrue;
	else if ( !Q_stricmp( argument, "water" ) ) underwater = qtrue;
	VectorMA( cg.refdef.vieworg, 192.0f, cg.refdef.viewaxis[0], origin );
	VectorScale( cg.refdef.viewaxis[0], -1.0f, normal );
	CG_WiredFx_RocketExplosion( origin, normal, material, freeAir, underwater );
}

static void CG_WiredFxTestHitscan_f( void ) {
	hitscanImpactMaterial_t material = HITSCAN_IMPACT_DEFAULT;
	const char *argument = CG_Argv( 1 );

	if ( !Q_stricmp( argument, "metal" ) ) material = HITSCAN_IMPACT_METAL;
	else if ( !Q_stricmp( argument, "dust" ) ) material = HITSCAN_IMPACT_DUST;
	CG_TestHitscanImpact( PROJ_MACHINEGUN, material );
}

static void CG_WiredFxTestShotgun_f( void ) {
	hitscanImpactMaterial_t material = HITSCAN_IMPACT_DEFAULT;
	const char *argument = CG_Argv( 1 );

	if ( !Q_stricmp( argument, "metal" ) ) material = HITSCAN_IMPACT_METAL;
	else if ( !Q_stricmp( argument, "dust" ) ) material = HITSCAN_IMPACT_DUST;
	CG_TestHitscanImpact( PROJ_SHOTGUN, material );
}

/*
static void CG_Camera_f( void ) {
	char name[1024];
	trap_Argv( 1, name, sizeof(name));
	if (trap_loadCamera(name)) {
		cg.cameraMode = qtrue;
		trap_startCamera(cg.time);
	} else {
		Com_Log( SEV_INFO, LOG_CH(ch_cgame), "Unable to load camera %s\n",name);
	}
}
*/


typedef struct {
	char	*cmd;
	void	(*function)(void);
} consoleCommand_t;

static consoleCommand_t	commands[] = {
	{ "testgun", CG_TestGun_f },
	{ "testmodel", CG_TestModel_f },
	{ "nextframe", CG_TestModelNextFrame_f },
	{ "prevframe", CG_TestModelPrevFrame_f },
	{ "nextskin", CG_TestModelNextSkin_f },
	{ "prevskin", CG_TestModelPrevSkin_f },
	{ "viewpos", CG_Viewpos_f },
	{ "sceneplay", CG_ScenePlay_f },
	{ "scenestop", CG_SceneStop_f },
	{ "sceneskip", CG_SceneSkip_f },
	{ "testscene", CG_ScenePlay_f },   // dev alias for sceneplay (takes a full path)
	{ "+scores", CG_ScoresDown_f },
	{ "-scores", CG_ScoresUp_f },
	{ "+zoom", CG_ZoomDown_f },
	{ "-zoom", CG_ZoomUp_f },
#if FEAT_THIRD_PERSON
	{ "+thirdperson", CG_ThirdPersonDown_f },
	{ "-thirdperson", CG_ThirdPersonUp_f },
#endif
	{ "weapnext", CG_NextWeapon_f },
	{ "weapprev", CG_PrevWeapon_f },
	{ "weapon", CG_Weapon_f },
	{ "weapongrabbed", CG_WeaponGrabbed_f },
	{ "tcmd", CG_TargetCommand_f },
	{ "tell_target", CG_TellTarget_f },
#if FEAT_TA_UI
	{ "nextTeamMember", CG_NextTeamMember_f },
	{ "prevTeamMember", CG_PrevTeamMember_f },
	{ "nextOrder", CG_NextOrder_f },
	{ "confirmOrder", CG_ConfirmOrder_f },
	{ "denyOrder", CG_DenyOrder_f },
	{ "vtell_target", CG_VoiceTellTarget_f },
	{ "taskOffense", CG_TaskOffense_f },
	{ "taskDefense", CG_TaskDefense_f },
	{ "taskPatrol", CG_TaskPatrol_f },
	{ "taskCamp", CG_TaskCamp_f },
	{ "taskFollow", CG_TaskFollow_f },
	{ "taskRetrieve", CG_TaskRetrieve_f },
	{ "taskEscort", CG_TaskEscort_f },
	{ "taskSuicide", CG_TaskSuicide_f },
	{ "taskOwnFlag", CG_TaskOwnFlag_f },
	{ "tauntKillInsult", CG_TauntKillInsult_f },
	{ "tauntPraise", CG_TauntPraise_f },
	{ "tauntTaunt", CG_TauntTaunt_f },
	{ "tauntDeathInsult", CG_TauntDeathInsult_f },
	{ "tauntGauntlet", CG_TauntGauntlet_f },
#endif
    { "spWin", CG_spWin_f },
    { "spLose", CG_spLose_f },
	{ "startOrbit", CG_StartOrbit_f },
	{ "wiredFxTestRocket", CG_WiredFxTestRocket_f },
	{ "wiredFxTestHitscan", CG_WiredFxTestHitscan_f },
	{ "wiredFxTestShotgun", CG_WiredFxTestShotgun_f },
	//{ "camera", CG_Camera_f },
	{ "loaddeferred", CG_LoadDeferredPlayers },
#if FEAT_CHAT_FILTER
	{ "ignore", CG_ChatFilterIgnore_f },
	{ "unignore", CG_ChatFilterUnignore_f },
#endif
};


/*
=================
CG_ConsoleCommand

The string has been tokenized and can be retrieved with
Cmd_Argc() / Cmd_Argv()
=================
*/
qboolean CG_ConsoleCommand( void ) {
	const char	*cmd;

	cmd = CG_Argv(0);

	for ( int i = 0 ; i < ARRAY_LEN( commands ) ; i++ ) {
		if ( !Q_stricmp( cmd, commands[i].cmd ) ) {
			commands[i].function();
			return qtrue;
		}
	}

	return qfalse;
}


/*
=================
CG_InitConsoleCommands

Let the client system know about all of our commands
so it can perform tab completion
=================
*/
void CG_InitConsoleCommands( void ) {
	for ( int i = 0 ; i < ARRAY_LEN( commands ) ; i++ ) {
		trap_AddCommand( commands[i].cmd );
	}

	//
	// the game server will interpret these commands, which will be automatically
	// forwarded to the server after they are not recognized locally
	//
	trap_AddCommand ("kill");
	trap_AddCommand ("say");
	trap_AddCommand ("say_team");
	trap_AddCommand ("tell");
#if FEAT_TA_UI
	trap_AddCommand ("vsay");
	trap_AddCommand ("vsay_team");
	trap_AddCommand ("vtell");
	trap_AddCommand ("vtaunt");
	trap_AddCommand ("vosay");
	trap_AddCommand ("vosay_team");
	trap_AddCommand ("votell");
#endif
	trap_AddCommand ("give");
	trap_AddCommand ("god");
	trap_AddCommand ("notarget");
	trap_AddCommand ("noclip");
	trap_AddCommand ("where");
	trap_AddCommand ("team");
	trap_AddCommand ("follow");
	trap_AddCommand ("follownext");
	trap_AddCommand ("followprev");
	trap_AddCommand ("levelshot");
	trap_AddCommand ("addbot");
	trap_AddCommand ("setviewpos");
	trap_AddCommand ("callvote");
	trap_AddCommand ("vote");
	trap_AddCommand ("callteamvote");
	trap_AddCommand ("teamvote");
	trap_AddCommand ("stats");
#if FEAT_PING_LOCATION
	trap_AddCommand ("ping");
#endif
#if FEAT_READY_UP
	trap_AddCommand ("ready");
#endif
	trap_AddCommand ("teamtask");
	trap_AddCommand ("loaddeferred");
}
