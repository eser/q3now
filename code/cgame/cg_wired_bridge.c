/*
===========================================================================
cg_wired_bridge.c — Wired UI: cgame-to-client state bridge

Fills wiredHudState_t from cgame globals and pushes it to the client each
frame via trap_WiredUI_PushHudState(). This is the ONLY file that reads
from cgame game state for HUD purposes after Phase 3 migration.
===========================================================================
*/

#include "cg_local.h"

#if FEAT_WIRED_UI

void CG_WiredHudPushState( void ) {
	wiredHudState_t state;
	int i;

	Com_Memset( &state, 0, sizeof( state ) );

	if ( !cg.snap ) return;

	// ── player state ─────────────────────────────────────────────────
	Com_Memcpy( state.stats, cg.snap->ps.stats, sizeof( state.stats ) );
	Com_Memcpy( state.persistant, cg.snap->ps.persistant, sizeof( state.persistant ) );
	Com_Memcpy( state.powerups, cg.snap->ps.powerups, sizeof( state.powerups ) );
	Com_Memcpy( state.ammo, cg.snap->ps.ammo, sizeof( state.ammo ) );
	state.weapon      = cg.snap->ps.weapon;
	state.weaponstate = cg.snap->ps.weaponstate;
	state.clientNum   = cg.snap->ps.clientNum;
	state.ping        = cg.snap->ps.ping;
	state.xyspeed     = cg.xyspeed;
	state.health      = cg.snap->ps.stats[STAT_HEALTH];
	state.armor       = cg.snap->ps.stats[STAT_ARMOR];

	// ── timing ───────────────────────────────────────────────────────
	state.time        = cg.time;
	state.frametime   = cg.frametime;
	state.realtime    = trap_Milliseconds();

	// ── match state ──────────────────────────────────────────────────
	state.gametype      = cgs.gametype;
	state.scores1       = cgs.scores1;
	state.scores2       = cgs.scores2;
	state.fraglimit     = cgs.fraglimit;
	state.capturelimit  = cgs.capturelimit;
	state.timelimit     = cgs.timelimit;
	state.maxclients    = cgs.maxclients;
	state.warmup        = cg.warmup;
	state.levelStartTime = cgs.levelStartTime;
	state.showScores    = cg.showScores;
	state.demoPlayback  = cg.demoPlayback;
	state.intermission  = ( cg.snap->ps.pm_type == PM_INTERMISSION );

	// ── team ─────────────────────────────────────────────────────────
	state.ourTeam = cgs.clientinfo[cg.clientNum].team;
	state.blueflag = cgs.blueflag;
	state.redflag  = cgs.redflag;

	// ── crosshair ────────────────────────────────────────────────────
	state.crosshairClientNum  = cg.crosshairClientNum;
	state.crosshairClientTime = cg.crosshairClientTime;

	// ── player extras ────────────────────────────────────────────────
	state.lowAmmoWarning        = cg.lowAmmoWarning;
	state.renderingThirdPerson  = cg.renderingThirdPerson;
	Com_Memcpy( &state.predictedPlayerState, &cg.predictedPlayerState, sizeof( playerState_t ) );

	// ── rewards ──────────────────────────────────────────────────────
	state.rewardStack = cg.rewardStack;
	state.rewardTime  = cg.rewardTime;
	for ( i = 0; i < WIRED_HUD_MAX_REWARDSTACK && i < MAX_REWARDSTACK; i++ ) {
		state.rewardCount[i]  = cg.rewardCount[i];
		state.rewardShader[i] = cg.rewardShader[i];
		state.rewardSound[i]  = cg.rewardSound[i];
	}

	// ── vote ─────────────────────────────────────────────────────────
	state.voteTime     = cgs.voteTime;
	state.voteYes      = cgs.voteYes;
	state.voteNo       = cgs.voteNo;
	state.voteModified = cgs.voteModified;
	Q_strncpyz( state.voteString, cgs.voteString, sizeof( state.voteString ) );

	// ── client info ──────────────────────────────────────────────────
	for ( i = 0; i < cgs.maxclients && i < WIRED_HUD_MAX_CLIENTS; i++ ) {
		if ( cgs.clientinfo[i].infoValid ) {
			Q_strncpyz( state.clients[i].name, cgs.clientinfo[i].name, sizeof( state.clients[i].name ) );
			state.clients[i].team      = cgs.clientinfo[i].team;
			state.clients[i].health    = cgs.clientinfo[i].health;
			state.clients[i].armor     = cgs.clientinfo[i].armor;
			state.clients[i].weapon    = cgs.clientinfo[i].curWeapon;
			state.clients[i].location  = cgs.clientinfo[i].location;
			state.clients[i].infoValid = qtrue;
		}
	}

	// ── events ───────────────────────────────────────────────────────
	state.itemPickup     = cg.itemPickup;
	state.itemPickupTime = cg.itemPickupTime;

	// ── media ────────────────────────────────────────────────────────
	state.whiteShader       = cgs.media.whiteShader;
	state.deferShader       = cgs.media.deferShader;
	state.noammoShader      = cgs.media.noammoShader;
	state.combatArmorIcon   = cgs.media.combatArmorIcon;
	state.heavyArmorIcon    = cgs.media.heavyArmorIcon;
	state.jacketArmorIcon   = cgs.media.jacketArmorIcon;
	state.talkSound         = cgs.media.talkSound;
	for ( i = 0; i < 3; i++ ) {
		state.redFlagShader[i]  = cgs.media.redFlagShader[i];
		state.blueFlagShader[i] = cgs.media.blueFlagShader[i];
	}
	for ( i = 0; i < MAX_WEAPONS; i++ ) {
		state.weaponIcons[i] = cg_weapons[i].weaponIcon;
	}

	state.valid = qtrue;

	trap_WiredUI_PushHudState( &state );
}

#endif // FEAT_WIRED_UI
