/*
===========================================================================
cl_wired_hud_compat.c — Compatibility layer implementation

Syncs wired_cg / wired_cgs compat structs from wiredHudState_t each frame.
Provides stub implementations of cgame helper functions.
===========================================================================
*/

#include "../../client.h"
#include "cl_wired_hud.h"

#if FEAT_WIRED_UI

// we need the compat types WITHOUT redefining cg/cgs macros for THIS file
#include "cl_wired_hud_compat.h"

// undo the macros so we can declare the actual globals
#undef cg
#undef cgs
#undef cg_weapons
#undef cg_MaxlocationWidth
#undef cg_drawTimer
#undef cg_drawAmmoWarning
#undef cg_items

wiredCgCompat_t     wired_cg;
wiredCgsCompat_t    wired_cgs;
wiredWeaponCompat_t wired_cg_weapons[32]; /* generic weapon buffer */
wiredItemCompat_t   wired_cg_items[256];
#undef cg_drawRewards
#undef cg_drawCrosshair
#undef cg_drawCrosshairNames
#undef cg_crosshairHealth
#undef numSortedTeamPlayers
#undef sortedTeamPlayers

vmCvar_t wired_MaxlocationWidth;
vmCvar_t wired_drawTimer;
vmCvar_t wired_drawAmmoWarning;
vmCvar_t wired_drawRewards;
vmCvar_t wired_drawCrosshair;
vmCvar_t wired_drawCrosshairNames;
vmCvar_t wired_crosshairHealth;
int wired_numSortedTeamPlayers;
int wired_sortedTeamPlayers[WIRED_HUD_MAX_TEAMOVERLAY];

// ── SuperHUD global context (event state) ────────────────────────────
// This holds chat/frag/obituary/powerup event state — pushed from cgame
// via separate events (not the per-frame state bridge).
// For now, a static instance — events will be wired in a later step.

#include "cl_wired_hud_private.h"

static superhudGlobalContext_t wired_superhudContext;

superhudGlobalContext_t* CG_SHUDGetContext( void ) {
	return &wired_superhudContext;
}

// CG_SHUDGetAmmo provided by cl_wired_hud_draw.c (migrated from cg_superhud_util.c)

// bg_misc.c trap stub — it declares trap_Cvar_VariableStringBuffer as extern
// In client context, route to Cvar_VariableStringBuffer directly
void trap_Cvar_VariableStringBuffer( const char *var_name, char *buffer, int bufsize ) {
	Cvar_VariableStringBuffer( var_name, buffer, bufsize );
}

// ── sync compat structs from wiredHudState ───────────────────────────

void WiredHud_SyncCompat( void ) {
	int i;

	if ( !wiredHud->valid ) return;

	// cg.* fields
	wired_cg.clientNum            = wiredHud->clientNum;
	wired_cg.time                 = wiredHud->time;
	wired_cg.frametime            = wiredHud->frametime;
	wired_cg.xyspeed              = wiredHud->xyspeed;
	wired_cg.warmup               = wiredHud->warmup;
	wired_cg.showScores           = wiredHud->showScores;
	wired_cg.demoPlayback         = wiredHud->demoPlayback;
	wired_cg.renderingThirdPerson = wiredHud->renderingThirdPerson;
	wired_cg.crosshairClientNum   = wiredHud->crosshairClientNum;
	wired_cg.crosshairClientTime  = wiredHud->crosshairClientTime;
	wired_cg.lowAmmoWarning       = wiredHud->lowAmmoWarning;
	wired_cg.itemPickup           = wiredHud->itemPickup;
	wired_cg.itemPickupTime       = wiredHud->itemPickupTime;
	Com_Memcpy( &wired_cg.predictedPlayerState, &wiredHud->predictedPlayerState, sizeof( playerState_t ) );

	// cg.snap proxy — point to internal storage
	Com_Memcpy( &wired_cg._snapData.ps, &wiredHud->predictedPlayerState, sizeof( playerState_t ) );
	// overlay actual snapshot stats (more accurate than predicted for HUD display)
	Com_Memcpy( wired_cg._snapData.ps.stats, wiredHud->stats, sizeof( wiredHud->stats ) );
	Com_Memcpy( wired_cg._snapData.ps.persistant, wiredHud->persistant, sizeof( wiredHud->persistant ) );
	Com_Memcpy( wired_cg._snapData.ps.powerups, wiredHud->powerups, sizeof( wiredHud->powerups ) );
	Com_Memcpy( wired_cg._snapData.ps.ammo, wiredHud->ammo, sizeof( wiredHud->ammo ) );
	wired_cg._snapData.ps.weapon      = wiredHud->weapon;
	wired_cg._snapData.ps.weaponstate = wiredHud->weaponstate;
	wired_cg._snapData.ps.clientNum   = wiredHud->clientNum;
	wired_cg._snapData.ping           = wiredHud->ping;
	wired_cg.snap = &wired_cg._snapData;

	// rewards
	wired_cg.rewardStack = wiredHud->rewardStack;
	wired_cg.rewardTime  = wiredHud->rewardTime;
	for ( i = 0; i < WIRED_HUD_MAX_REWARDSTACK; i++ ) {
		wired_cg.rewardCount[i]  = wiredHud->rewardCount[i];
		wired_cg.rewardShader[i] = wiredHud->rewardShader[i];
		wired_cg.rewardSound[i]  = wiredHud->rewardSound[i];
	}

	// cgs.* fields
	wired_cgs.gametype       = wiredHud->gametype;
	wired_cgs.scores1        = wiredHud->scores1;
	wired_cgs.scores2        = wiredHud->scores2;
	wired_cgs.scorelimit     = wiredHud->scorelimit;
	wired_cgs.timelimit      = wiredHud->timelimit;
	wired_cgs.maxclients     = wiredHud->maxclients;
	wired_cgs.levelStartTime = wiredHud->levelStartTime;
	wired_cgs.blueflag       = wiredHud->blueflag;
	wired_cgs.redflag        = wiredHud->redflag;
	wired_cgs.voteTime       = wiredHud->voteTime;
	wired_cgs.voteYes        = wiredHud->voteYes;
	wired_cgs.voteNo         = wiredHud->voteNo;
	wired_cgs.voteModified   = wiredHud->voteModified;
	Q_strncpyz( wired_cgs.voteString, wiredHud->voteString, sizeof( wired_cgs.voteString ) );

	// glconfig — copy from client
	Com_Memcpy( &wired_cgs.glconfig, &cls.glconfig, sizeof( glconfig_t ) );

	// media
	wired_cgs.media.whiteShader       = wiredHud->whiteShader;
	wired_cgs.media.deferShader       = wiredHud->deferShader;
	wired_cgs.media.noammoShader      = wiredHud->noammoShader;
	wired_cgs.media.healthIcon        = wiredHud->healthIcon;
	wired_cgs.media.heavyArmorIcon    = wiredHud->heavyArmorIcon;
	wired_cgs.media.combatArmorIcon   = wiredHud->combatArmorIcon;
	wired_cgs.media.jacketArmorIcon   = wiredHud->jacketArmorIcon;
	wired_cgs.media.talkSound         = wiredHud->talkSound;
	for ( i = 0; i < 3; i++ ) {
		wired_cgs.media.redFlagShader[i]  = wiredHud->redFlagShader[i];
		wired_cgs.media.blueFlagShader[i] = wiredHud->blueFlagShader[i];
	}

	// clientinfo
	for ( i = 0; i < WIRED_HUD_MAX_CLIENTS; i++ ) {
		Q_strncpyz( wired_cgs.clientinfo[i].name, wiredHud->clients[i].name, MAX_QPATH );
		wired_cgs.clientinfo[i].team      = wiredHud->clients[i].team;
		wired_cgs.clientinfo[i].health    = wiredHud->clients[i].health;
		wired_cgs.clientinfo[i].armor     = wiredHud->clients[i].armor;
		wired_cgs.clientinfo[i].curWeapon = wiredHud->clients[i].curWeapon;
		wired_cgs.clientinfo[i].location  = wiredHud->clients[i].location;
		wired_cgs.clientinfo[i].infoValid = wiredHud->clients[i].infoValid;
	}

	// team overlay sorting
	wired_numSortedTeamPlayers = wiredHud->numSortedTeamPlayers;
	for ( i = 0; i < wiredHud->numSortedTeamPlayers && i < WIRED_HUD_MAX_TEAMOVERLAY; i++ ) {
		wired_sortedTeamPlayers[i] = wiredHud->sortedTeamPlayers[i];
	}

	/* weapon + ammo icons — copy as many slots as both buffers can hold */
	{
		int maxSlots = (int)(sizeof(wired_cg_weapons) / sizeof(wired_cg_weapons[0]));
		int srcSlots = (int)(sizeof(wiredHud->weaponIcons) / sizeof(wiredHud->weaponIcons[0]));
		if ( srcSlots < maxSlots ) maxSlots = srcSlots;
		for ( i = 1; i < maxSlots; i++ ) {
			wired_cg_weapons[i].weaponIcon = wiredHud->weaponIcons[i];
			wired_cg_weapons[i].ammoIcon   = wiredHud->ammoIcons[i];
		}
	}

	// item icons (for item pickup, powerup display)
	for ( i = 0; i < WIRED_HUD_MAX_ITEMS; i++ ) {
		wired_cg_items[i].icon = wiredHud->itemIcons[i];
	}
}

// ── helper stubs ─────────────────────────────────────────────────────

const char *wired_ConfigString( int index ) {
	if ( index < 0 || index >= MAX_CONFIGSTRINGS ) {
		return "";
	}
	if ( !cl.gameState.stringOffsets[index] ) {
		return "";
	}
	return cl.gameState.stringData + cl.gameState.stringOffsets[index];
}

void wired_GetColorForAmount( int amount, vec4_t hcolor ) {
	float t = Com_Clamp(0, 100, amount) / 100.0f;

	hcolor[0] = 1;
	hcolor[1] = MIN(t * 2, 1.0);
	hcolor[2] = MAX(t * 2 - 1.0, 0);
	hcolor[3] = 1;
}

qboolean wired_IsFollowing( void ) {
	return ( wired_cg.snap && wired_cg.snap->ps.pm_flags & PMF_FOLLOW );
}

qboolean wired_IsSpectator( void ) {
	return wiredHud->isSpectator;
}

qboolean wired_IsGameTypeFreeze( void ) {
	// check g_freeze cvar — if set and team game, it's freeze tag
	return qfalse;  // simplified — full check needs g_freeze cvar state
}

#endif // FEAT_WIRED_UI
