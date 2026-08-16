// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
===========================================================================
cg_wired_bridge.c — Wired UI: cgame-to-client state bridge

Fills wiredHudState_t from cgame globals and pushes it to the client each
frame via trap_WiredUI_PushHudState(). This is the ONLY file that reads
from cgame game state for HUD purposes after the migration.
===========================================================================
*/

#include "cg_local.h"
#include "cg_wired_store.h"

#if FEAT_WIRED_UI

/* Resolve a weapon's on-disk crosshair-script directory token from its world
 * model path, e.g. "models/weapons2/machinegun/machinegun.md3" -> "machinegun".
 * The procedural-crosshair loader looks up base/weapons/<token>/crosshair.lua.
 * Uses world_model[0]'s parent-directory basename — this matches the spec's
 * file-layout convention for the authored reference weapons (machinegun,
 * lightning). NOTE: a handful of weapons use abbreviated model dirs
 * (rocketlauncher -> "rocketl", grenadelauncher -> "grenadel"); a future
 * canonical short-name table would normalize those, but v1 ships only the two
 * reference scripts, both of which resolve cleanly here. Returns "" when no
 * model path exists (e.g. WP_NONE) so the loader falls back to the default. */
static void CG_CrosshairWeaponToken( int weapon, char *out, int outSize ) {
	const gitem_t *item;
	const char    *path;
	const char    *slash, *prev;

	out[0] = '\0';
	if ( weapon <= WP_NONE || weapon >= WP_NUM_WEAPONS ) return;

	item = BG_FindItemForWeapon( weapon );
	if ( !item || !item->world_model[0] || !item->world_model[0][0] ) return;

	/* parent-directory basename: scan for the last and second-to-last '/' */
	path  = item->world_model[0];
	slash = strrchr( path, '/' );
	if ( !slash || slash == path ) return;
	prev = slash - 1;
	while ( prev > path && *prev != '/' ) prev--;
	if ( *prev == '/' ) prev++;
	{
		int len = (int)( slash - prev );
		if ( len <= 0 || len >= outSize ) return;
		Q_strncpyz( out, prev, len + 1 );
	}
}

void CG_WiredHudPushState( void ) {
	wiredHudState_t state;
	int i;

	memset( &state, 0, sizeof( state ) );

	if ( !cg.snap ) return;

	// ── player state ─────────────────────────────────────────────────
	memcpy( state.stats, cg.snap->ps.stats, sizeof( state.stats ) );
	memcpy( state.persistant, cg.snap->ps.persistant, sizeof( state.persistant ) );
	memcpy( state.powerups, cg.snap->ps.powerups, sizeof( state.powerups ) );
	memcpy( state.ammo, cg.snap->ps.ammo, sizeof( state.ammo ) );
	state.weapon      = cg.snap->ps.weapon;
	state.weaponstate = cg.snap->ps.weaponstate;
	state.clientNum   = cg.snap->ps.clientNum;
	state.ping        = cg.snap->ps.ping;
	state.xyspeed     = cg.xyspeed;
	state.health      = cg.snap->ps.stats[STAT_HEALTH];
	state.armor       = cg.snap->ps.stats[STAT_ARMOR];

	// pre-compute health/armor colors (cgame owns game logic, client just renders)
	{
		int eff = BG_GetEffectiveHealth( cg.snap->ps.stats[STAT_HEALTH],
			cg.snap->ps.stats[STAT_ARMORCLASS], cg.snap->ps.stats[STAT_ARMOR] );
		state.effectiveHealth = eff;
		BG_GetColorForAmount( eff, state.healthColor );
		BG_GetColorForAmount( cg.snap->ps.stats[STAT_ARMOR], state.armorColor );
	}

	// ── timing ───────────────────────────────────────────────────────
	state.time        = cg.time;
	state.frametime   = cg.frametime;
	state.realtime    = trap_Milliseconds();

	// ── match state ──────────────────────────────────────────────────
	state.gametype      = cgs.gametype;
	state.scores1       = cgs.scores1;
	state.scores2       = cgs.scores2;
	state.scorelimit    = cgs.scorelimit;
	state.timelimit     = cgs.timelimit;
	state.maxclients    = cgs.maxclients;
	state.warmup        = cg.warmup;
	state.levelStartTime = cgs.levelStartTime;
	state.showScores    = ( cg.showScores || ( cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR &&
		  ( cg.snap->ps.pm_flags & PMF_SCOREBOARD ) ) );
	state.demoPlayback  = cg.demoPlayback;
	state.intermission  = ( cg.snap->ps.pm_type == PM_INTERMISSION );
	// cinematic director: hide the entire game HUD while a scene is playing with
	// its HUD flag off (the default). Client-view-only — re-derived each frame,
	// so the HUD returns automatically on scene-end (a 'hud' event can opt it back
	// on mid-scene). This is the Wired-UI equivalent of the legacy 2D suppress.
	state.sceneHudHidden = (qboolean)( cg.scenePlayback.active && !cg.scenePlayback.hudVisible );

	{
		int cmdNum;
		usercmd_t cmd;
		cmdNum = trap_GetCurrentCmdNumber() - CMD_BACKUP + 1;
		trap_GetUserCmd( cmdNum, &cmd );
		state.connectionInterrupted = ( !cg_paused.integer
		                                && cmd.serverTime > cg.snap->ps.commandTime
		                                && cmd.serverTime <= cg.time );
	}

	// ── team ─────────────────────────────────────────────────────────
	state.ourTeam = cgs.clientinfo[cg.clientNum].team;
	state.blueflag = cgs.blueflag;
	state.redflag  = cgs.redflag;

	// ── pre-computed game mode state ─────────────────────────────────
	{
		int activeTeam = cgs.clientinfo[cg.clientNum].team;
		int redCount = 0, blueCount = 0;

		if ( activeTeam == TEAM_SPECTATOR && cg.snap ) {
			activeTeam = cgs.clientinfo[cg.snap->ps.clientNum].team;
		}
		state.ourActiveTeam = activeTeam;
		state.isOurTeamBlue = ( activeTeam == TEAM_BLUE );
		state.isSpectator   = ( cgs.clientinfo[cg.clientNum].team == TEAM_SPECTATOR );
		state.isTeamGame    = ( cgs.gametypeIsTeamGame );
		state.isDuel        = ( cgs.gametype == GT_DUEL );

		if ( cgs.gametype >= 0 && cgs.gametype < GT_MAX_GAME_TYPE ) {
			Q_strncpyz( state.gametypeName, bg_gametypelist[cgs.gametype].name,
				sizeof( state.gametypeName ) );
		} else {
			Q_strncpyz( state.gametypeName, "Unknown", sizeof( state.gametypeName ) );
		}

		/* count players per team for team-count elements */
		for ( i = 0; i < cgs.maxclients && i < WIRED_HUD_MAX_CLIENTS; i++ ) {
			if ( cgs.clientinfo[i].infoValid ) {
				if ( cgs.clientinfo[i].team == TEAM_RED )  redCount++;
				if ( cgs.clientinfo[i].team == TEAM_BLUE ) blueCount++;
			}
		}
		if ( activeTeam == TEAM_RED ) {
			state.ownTeamCount   = redCount;
			state.enemyTeamCount = blueCount;
		} else if ( activeTeam == TEAM_BLUE ) {
			state.ownTeamCount   = blueCount;
			state.enemyTeamCount = redCount;
		} else {
			state.ownTeamCount   = 0;
			state.enemyTeamCount = 0;
		}
	}

	// ── crosshair ────────────────────────────────────────────────────
	state.crosshairClientNum  = cg.crosshairClientNum;
	state.crosshairClientTime = cg.crosshairClientTime;

	// pre-compute crosshair rendering state (client just draws).
	// WA-1: third-person no longer suppresses the Wired crosshair — it now DRAWS
	// at a world-derived offset (computed below), replacing the retired imperative
	// RT_SPRITE. Suppression is kept ONLY for the genuine no-crosshair cases
	// (crosshair disabled / spectator).
	// cg_crosshairAlpha is a float; .integer truncates a fractional alpha
	// (0<a<1) to 0 → crosshair wrongly vanishes. Test the float value (the
	// color path at :187 already uses .value), so any positive alpha shows.
	if ( cg_crosshairAlpha.value <= 0.0f ||
	     cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR ) {
		state.crosshair.shaderIndex = -1;
	} else {
		vec4_t xhairColor;
		float w, f;

		// cg_crosshairHealth: health-gradient colour; else cg_crosshairColor
		// ("R G B" 0..1; non-triplet falls back to white).
		if ( cg_crosshairHealth.integer ) {
			int eff = BG_GetEffectiveHealth(
				cg.snap->ps.stats[STAT_HEALTH],
				cg.snap->ps.stats[STAT_ARMORCLASS],
				cg.snap->ps.stats[STAT_ARMOR] );
			BG_GetColorForAmount( eff, xhairColor );
		} else if ( sscanf( cg_crosshairColor.string, "%f %f %f",
				&xhairColor[0], &xhairColor[1], &xhairColor[2] ) != 3 ) {
			xhairColor[0] = xhairColor[1] = xhairColor[2] = 1.0f;
		}

		xhairColor[3] = Com_Clamp( 0.0f, 1.0f, cg_crosshairAlpha.value );
		// rocket launcher helix: amber tint during alt-fire cooldown
		if ( cg.snap->ps.weapon == WP_ROCKET_LAUNCHER
			&& bg_weaponlist[WP_ROCKET_LAUNCHER].attackAlt != ATT_NONE
			&& cg.predictedPlayerState.weaponTime > 0
			&& cg.predictedPlayerState.weaponTime > bg_attacklist[ATT_ROCKET_LAUNCHER_PRIMARY].reloadTime ) {
			xhairColor[0] = 1.0f;   // full red
			xhairColor[1] = 0.65f;  // amber
			xhairColor[2] = 0.0f;   // no blue
		}
		Vector4Copy( xhairColor, state.crosshair.color );

		w = cg_crosshairSize.value;
		f = (float)( cg.time - cg.itemPickupBlendTime );
		if ( f > 0 && f < ITEM_BLOB_TIME ) {
			f /= ITEM_BLOB_TIME;
			w *= ( 1.0f + f );
		}
		state.crosshair.size = w;

		/* WA-1: stage the crosshair's world-anchored screen OFFSET (from center,
		 * real pixels) so the Wired UI element draws it where bullets land. cgame
		 * computes WHERE; Wired UI draws WHAT (inversion of control). First-person:
		 * camera-forward trace (impact projects to centre → offset 0,0 →
		 * byte-identical). Third-person: muzzle-trace along ps.viewangles (the
		 * bullet aim, not the camera) → offset to the real impact point. The trace
		 * passes the local clientNum so the player's own body isn't hit. The
		 * world→pixels conversion (640x480 virtual → real) lives in
		 * CG_WorldToScreenPixels; here we subtract screen centre for the offset.
		 * Behind-camera (aiming back at own camera in 3rd-person) → centre fallback. */
		{
			vec3_t  traceStart, traceDir, traceEnd;
			trace_t tr;
			float   xPx, yPx;

			if ( cg.renderingThirdPerson ) {
				CG_CalcMuzzlePoint( cg.snap->ps.clientNum, traceStart );
				AngleVectors( cg.snap->ps.viewangles, traceDir, NULL, NULL );
			} else {
				VectorCopy( cg.refdef.vieworg, traceStart );
				VectorCopy( cg.refdef.viewaxis[0], traceDir );
			}
			VectorMA( traceStart, 8192.0f, traceDir, traceEnd );
			CG_Trace( &tr, traceStart, NULL, NULL, traceEnd,
				cg.snap->ps.clientNum, MASK_SHOT );

			if ( CG_WorldToScreenPixels( tr.endpos, &xPx, &yPx ) ) {
				state.crosshair.x = xPx - (float)cgs.glconfig.vidWidth  * 0.5f;
				state.crosshair.y = yPx - (float)cgs.glconfig.vidHeight * 0.5f;
			} else {
				state.crosshair.x = 0.0f;
				state.crosshair.y = 0.0f;
			}
		}
	}

	// ── player extras ────────────────────────────────────────────────
	state.lowAmmoWarning        = cg.lowAmmoWarning;
	state.renderingThirdPerson  = cg.renderingThirdPerson;
	memcpy( &state.predictedPlayerState, &cg.predictedPlayerState, sizeof( playerState_t ) );

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

	// ── team vote ────────────────────────────────────────────────────
	// cgs.teamVote*[] is a 2-slot array keyed by the CS_TEAMVOTE_* offset
	// convention from g_main.c:CheckTeamVote (0 = TEAM_RED, 1 = TEAM_BLUE).
	// Resolve the local player's slot here so the client HUD only ever sees
	// "is there a team vote *I* can answer". Players on neither team get a
	// zeroed block, which the element treats as inactive.
	{
		int voteTeam = cgs.clientinfo[cg.clientNum].team;

		// while following, show the followed player's team vote
		if ( voteTeam == TEAM_SPECTATOR && cg.snap ) {
			voteTeam = cgs.clientinfo[cg.snap->ps.clientNum].team;
		}

		if ( voteTeam == TEAM_RED || voteTeam == TEAM_BLUE ) {
			int slot = ( voteTeam == TEAM_RED ) ? 0 : 1;

			state.teamVoteTime     = cgs.teamVoteTime[slot];
			state.teamVoteYes      = cgs.teamVoteYes[slot];
			state.teamVoteNo       = cgs.teamVoteNo[slot];
			state.teamVoteModified = cgs.teamVoteModified[slot];
			Q_strncpyz( state.teamVoteString, cgs.teamVoteString[slot],
				sizeof( state.teamVoteString ) );
		} else {
			state.teamVoteTime      = 0;
			state.teamVoteYes       = 0;
			state.teamVoteNo        = 0;
			state.teamVoteModified  = qfalse;
			state.teamVoteString[0] = '\0';
		}
	}

	Q_strncpyz( state.killerName, cg.killerName, sizeof( state.killerName ) );

	// ── client info ──────────────────────────────────────────────────
	for ( i = 0; i < cgs.maxclients && i < WIRED_HUD_MAX_CLIENTS; i++ ) {
		if ( cgs.clientinfo[i].infoValid ) {
			Q_strncpyz( state.clients[i].name, cgs.clientinfo[i].name, sizeof( state.clients[i].name ) );
			state.clients[i].team        = cgs.clientinfo[i].team;
			state.clients[i].health      = cgs.clientinfo[i].health;
			state.clients[i].armor       = cgs.clientinfo[i].armor;
			state.clients[i].armorClass  = cgs.clientinfo[i].armorClass;
			state.clients[i].weapon      = cgs.clientinfo[i].curWeapon;
			state.clients[i].curWeapon   = cgs.clientinfo[i].curWeapon;
			state.clients[i].location    = cgs.clientinfo[i].location;
			state.clients[i].infoValid   = qtrue;
		}
	}

	// ── team overlay ─────────────────────────────────────────────────
	state.numSortedTeamPlayers = numSortedTeamPlayers;
	for ( i = 0; i < numSortedTeamPlayers && i < WIRED_HUD_MAX_TEAMOVERLAY; i++ ) {
		state.sortedTeamPlayers[i] = sortedTeamPlayers[i];
	}

	// ── scoreboard scores (pre-sorted by server rank order) ─────────
	// Clamp numScores to what the loop below actually copies (capped at
	// WIRED_HUD_MAX_SCORES). Publishing the full cg.numScores would let a
	// consumer that iterates `si < numScores` read scores[] entries never
	// populated here — stale/out-of-bounds score data on a full server.
	state.numScores = ( cg.numScores < WIRED_HUD_MAX_SCORES ) ? cg.numScores : WIRED_HUD_MAX_SCORES;
	for ( i = 0; i < cg.numScores && i < WIRED_HUD_MAX_SCORES; i++ ) {
		state.scores[i].client          = cg.scores[i].client;
		state.scores[i].score           = cg.scores[i].score;
		state.scores[i].ping            = cg.scores[i].ping;
		state.scores[i].time            = cg.scores[i].time;
		state.scores[i].scoreFlags      = cg.scores[i].scoreFlags;
		state.scores[i].powerUps        = cg.scores[i].powerUps;
		state.scores[i].accuracy        = cg.scores[i].accuracy;
		state.scores[i].impressiveCount = cg.scores[i].impressiveCount;
		state.scores[i].excellentCount  = cg.scores[i].excellentCount;
		state.scores[i].guantletCount   = cg.scores[i].guantletCount;
		state.scores[i].defendCount     = cg.scores[i].defendCount;
		state.scores[i].assistCount     = cg.scores[i].assistCount;
		state.scores[i].captures        = cg.scores[i].captures;
		state.scores[i].deaths          = cg.scores[i].deaths;
		state.scores[i].team            = cg.scores[i].team;
		state.scores[i].perfect         = cg.scores[i].perfect;
		state.scores[i].killingSpreeCount = cg.scores[i].killingSpreeCount;
		state.scores[i].rampageCount      = cg.scores[i].rampageCount;
		state.scores[i].massacreCount     = cg.scores[i].massacreCount;
		state.scores[i].unstoppableCount  = cg.scores[i].unstoppableCount;

		// tournament / duel stats from clientInfo
		{
			int clientNum = cg.scores[i].client;
			if ( clientNum >= 0 && clientNum < MAX_CLIENTS ) {
				state.scores[i].wins   = cgs.clientinfo[clientNum].wins;
				state.scores[i].losses = cgs.clientinfo[clientNum].losses;
			}
		}

		// pre-compute total damage and best attack from attackStats
		{
			int j, clientNum = cg.scores[i].client;
			int totalDmg = 0, bestAtt = ATT_NONE, bestAttKills = 0;
			if ( clientNum >= 0 && clientNum < MAX_CLIENTS ) {
				for ( j = ATT_NONE + 1; j < ATT_NUM_ATTACKS; j++ ) {
					totalDmg += cgs.attackStats[clientNum][j].damage;
					if ( cgs.attackStats[clientNum][j].kills > bestAttKills ) {
						bestAttKills = cgs.attackStats[clientNum][j].kills;
						bestAtt = j;
					}
				}
			}
			state.scores[i].totalDamage = totalDmg;
			state.scores[i].damageDone  = totalDmg;   // same aggregate
			state.scores[i].damageTaken = 0;           // not tracked per-client in cgame yet
			state.scores[i].bestAttack  = bestAtt;
		}

		// per-weapon stats: aggregate attack stats by weapon
		{
			int j, w, clientNum = cg.scores[i].client;
			if ( clientNum >= 0 && clientNum < MAX_CLIENTS ) {
				for ( j = ATT_NONE + 1; j < ATT_NUM_ATTACKS; j++ ) {
					w = bg_attacklist[j].weapon;
					if ( w > 0 && w < WIRED_MAX_WEAPONS ) {
						state.scores[i].weaponStats[w].hits   += cgs.attackStats[clientNum][j].hits;
						state.scores[i].weaponStats[w].shots  += cgs.attackStats[clientNum][j].shots;
						state.scores[i].weaponStats[w].kills  += cgs.attackStats[clientNum][j].kills;
						state.scores[i].weaponStats[w].deaths += cgs.attackStats[clientNum][j].deaths;
					}
				}
			}
		}
	}

	/* ── pre-compute per-score weapon totals ──────────────────────────── */
	for ( i = 0; i < cg.numScores && i < WIRED_HUD_MAX_SCORES; i++ ) {
		int w, tK = 0, tS = 0, tH = 0;
		for ( w = 0; w < WIRED_MAX_WEAPONS; w++ ) {
			tK += state.scores[i].weaponStats[w].kills;
			tS += state.scores[i].weaponStats[w].shots;
			tH += state.scores[i].weaponStats[w].hits;
		}
		state.scoreWeaponTotals[i].totalKills = tK;
		state.scoreWeaponTotals[i].totalShots = tS;
		state.scoreWeaponTotals[i].totalHits  = tH;
	}

	// ── events ───────────────────────────────────────────────────────
	state.itemPickup     = cg.itemPickup;
	state.itemPickupTime = cg.itemPickupTime;

	// ── media ────────────────────────────────────────────────────────
	state.whiteShader       = cgs.media.whiteShader;
	state.deferShader       = cgs.media.deferShader;
	state.noammoShader      = cgs.media.noammoShader;
	state.healthIcon        = cgs.media.healthIcon;
	state.heavyArmorIcon    = cgs.media.heavyArmorIcon;
	state.combatArmorIcon   = cgs.media.combatArmorIcon;
	state.jacketArmorIcon   = cgs.media.jacketArmorIcon;
	state.talkSound         = cgs.media.talkSound;
	for ( i = 0; i < 3; i++ ) {
		state.redFlagShader[i]  = cgs.media.redFlagShader[i];
		state.blueFlagShader[i] = cgs.media.blueFlagShader[i];
	}
	for ( i = WP_NONE + 1; i < WP_NUM_WEAPONS; i++ ) {
		state.weaponIcons[i] = cg_weapons[i].weaponIcon;
		state.ammoIcons[i]   = cg_weapons[i].ammoIcon;
	}
	for ( i = ATT_NONE + 1; i < ATT_NUM_ATTACKS; i++ ) {
		state.attackIcons[i] = cg_weapons[bg_attacklist[i].weapon].weaponIcon;
	}

	// item icons (for item pickup, powerup display)
	for ( i = 0; i < bg_numItems && i < WIRED_HUD_MAX_ITEMS; i++ ) {
		state.itemIcons[i] = cg_items[i].icon;
	}

	// head model icons (2D player head icons for scoreboard)
	for ( i = 0; i < cgs.maxclients && i < WIRED_HUD_MAX_CLIENTS; i++ ) {
		state.headIcons[i] = cgs.clientinfo[i].modelIcon;
	}

	/* ── pre-computed weapon list (owned weapons for HUD display) ───── */
	{
		int w, weaponCount = 0;
		int statWeapons = cg.snap->ps.stats[STAT_WEAPONS];
		int curWeap = cg.snap->ps.weapon;
		/* HUD redesign (Eser): include the GAUNTLET in the carousel — it is a
		   real weapon and its absence from the selection list read as a gap.
		   Its ammo is -1 (infinite); the widget shows it as ∞ (see the ammo
		   binding below for the value-based -1 → "∞" rule). */
		for ( w = WP_GAUNTLET; w < WP_NUM_WEAPONS && weaponCount < WIRED_MAX_WEAPONS; w++ ) {
			if ( statWeapons & ( 1 << w ) ) {
				int ammoVal = cg.snap->ps.ammo[w];
				/* Keep the infinite sentinel (-1) intact so the carousel slot can
				   render ∞; only clamp the finite upper bound. */
				if ( ammoVal > 999 ) ammoVal = 999;
				state.weaponList[weaponCount].id       = w;
				state.weaponList[weaponCount].icon      = cg_weapons[w].weaponIcon;
				state.weaponList[weaponCount].ammoIcon   = cg_weapons[w].ammoIcon;
				state.weaponList[weaponCount].ammo       = ammoVal;
				state.weaponList[weaponCount].selected   = ( w == curWeap ) ? qtrue : qfalse;
				weaponCount++;
			}
		}
		state.weaponListCount = weaponCount;
		/* HUD redesign (Eser): carousel show/fade gate — the client compares
		   wired_cg.time against this to show the list only for ~1.5s after a
		   weapon switch, then fade it out (classic Q3 behaviour). */
		state.weaponSelectTime = cg.weaponSelectTime;
	}

	/* ── pre-computed active powerups ────────────────────────────────── */
	{
		int pw, pwCount = 0;
		/* timed powerups: sorted by time remaining, skip flags (t>999000) */
		for ( pw = PW_NONE + 1; pw < PW_NUM_POWERUPS && pwCount < 8; pw++ ) {
			int t;
			if ( !cg.snap->ps.powerups[pw] ) continue;
			t = cg.snap->ps.powerups[pw] - cg.time;
			if ( t < 0 || t > 999000 ) continue;
			{
				gitem_t *item = BG_FindItemForPowerup( pw );
				if ( item && item->icon ) {
					state.activePowerups[pwCount].icon = trap_R_RegisterShader( item->icon );
				} else {
					state.activePowerups[pwCount].icon = 0;
				}
			}
			state.activePowerups[pwCount].timeLeft = ( t + 999 ) / 1000;
			state.activePowerups[pwCount].isHoldable = qfalse;
			pwCount++;
		}
		state.activePowerupCount = pwCount;
	}

	/* holdable inventory list */
	{
		int bits = cg.snap->ps.stats[STAT_HOLDABLE_BITS];
		int count = 0;
		for ( int hi = 1; hi < HI_NUM_HOLDABLE; hi++ ) {
			if ( !( bits & BG_HOLDABLE_BIT( hi ) ) ) continue;
			state.holdableList[count].id       = (holdable_t)hi;
			state.holdableList[count].selected = ( bg_itemlist[cg.snap->ps.stats[STAT_HOLDABLE_ITEM]].giTag == hi );

			/* safe lookup: cg_items[] is indexed by bg_itemlist position, not holdable_t */
			gitem_t *item = NULL;
			for ( int j = 0; j < bg_numItems; j++ ) {
				if ( bg_itemlist[j].giType == IT_HOLDABLE && bg_itemlist[j].giTag == hi ) {
					item = &bg_itemlist[j];
					break;
				}
			}

			if ( item && cg_items[item - bg_itemlist].icon ) {
				state.holdableList[count].icon     = cg_items[item - bg_itemlist].icon;
				state.holdableList[count].label[0] = '\0';
			} else {
				state.holdableList[count].icon = 0;
				switch ( hi ) {
				case HI_KEY_GOLD:   Q_strncpyz( state.holdableList[count].label, "GOLD KEY",   16 ); break;
				case HI_KEY_SILVER: Q_strncpyz( state.holdableList[count].label, "SILVER KEY", 16 ); break;
				default:            Q_strncpyz( state.holdableList[count].label, item ? item->pickup_name : "ITEM", 16 ); break;
				}
			}
			count++;
		}
		state.holdableListCount = count;
	}

	// ── data bindings (named stat bundles for generic HUD elements) ──
	{
		int b = 0;
		int hp = cg.snap->ps.stats[STAT_HEALTH];
		int ap = cg.snap->ps.stats[STAT_ARMOR];
		int wp = cg.snap->ps.weapon;
		int ammoVal;
		// ps.weapon is server-controlled; clamp before it indexes ps.ammo[] (and
		// cg_weapons[] below). Out-of-range → WP_NONE (no weapon shown).
		if ( (unsigned)wp >= WP_NUM_WEAPONS ) wp = WP_NONE;
		ammoVal = cg.snap->ps.ammo[wp];

		// health binding
		Q_strncpyz( state.bindings[b].name, "health", sizeof( state.bindings[b].name ) );
		Com_sprintf( state.bindings[b].text, sizeof( state.bindings[b].text ), "%d", hp > 0 ? hp : 0 );
		Vector4Copy( state.healthColor, state.bindings[b].color );
		state.bindings[b].icon = cgs.media.healthIcon;
		state.bindings[b].percent = Com_Clamp( 0.0f, 1.0f, hp / 100.0f );
		state.bindings[b].visible = qtrue;
		b++;

		// armor binding
		Q_strncpyz( state.bindings[b].name, "armor", sizeof( state.bindings[b].name ) );
		Com_sprintf( state.bindings[b].text, sizeof( state.bindings[b].text ), "%d", ap > 0 ? ap : 0 );
		Vector4Copy( state.armorColor, state.bindings[b].color );
		switch ( cg.snap->ps.stats[STAT_ARMORCLASS] ) {
			case ARM_HEAVY:  state.bindings[b].icon = cgs.media.heavyArmorIcon; break;
			case ARM_COMBAT: state.bindings[b].icon = cgs.media.combatArmorIcon; break;
			case ARM_JACKET: state.bindings[b].icon = cgs.media.jacketArmorIcon; break;
			default:         state.bindings[b].icon = cgs.media.combatArmorIcon; break;
		}
		state.bindings[b].percent = Com_Clamp( 0.0f, 1.0f, ap / 200.0f );
		state.bindings[b].visible = ( ap > 0 );
		b++;

		// ammo binding — value-based ∞ for infinite weapons (mirrors the
		// WUI_Stage path below; keep the two in sync). ammoVal < 0 = infinite.
		Q_strncpyz( state.bindings[b].name, "ammo", sizeof( state.bindings[b].name ) );
		if ( ammoVal < 0 )
			Q_strncpyz( state.bindings[b].text, "\xE2\x88\x9E", sizeof( state.bindings[b].text ) ); /* UTF-8 ∞ */
		else
			Com_sprintf( state.bindings[b].text, sizeof( state.bindings[b].text ), "%d", ammoVal > 0 ? ammoVal : 0 );
		if ( ammoVal == 0 ) Vector4Set( state.bindings[b].color, 1, 0, 0, 1 );
		else                Vector4Set( state.bindings[b].color, 1, 1, 1, 1 );
		state.bindings[b].icon = cg_weapons[wp].ammoIcon;
		{
			/* bar fills against the CURRENT weapon's max ammo, not a fixed 200
			   (GL max 50 → 50/50 reads full). maxAmmunition -1 (no cap: none/
			   gauntlet) → full bar. Keep in sync with the WUI_Stage copy below. */
			int maxAmmo = bg_weaponlist[wp].maxAmmunition;
			state.bindings[b].percent = ( ammoVal < 0 || maxAmmo <= 0 ) ? 1.0f
				: Com_Clamp( 0.0f, 1.0f, (float)ammoVal / (float)maxAmmo );
		}
		state.bindings[b].visible = ( wp != WP_NONE );
		b++;

		state.numBindings = b;
	}

	/* ── Write player state to Wired Store ──────────────────────────── */
	{
		int hp, ap, wp, ammoVal;
		char buf[64];
		vec4_t color;

		hp = cg.snap->ps.stats[STAT_HEALTH];
		ap = cg.snap->ps.stats[STAT_ARMOR];
		wp = cg.snap->ps.weapon;
		// ps.weapon is server-controlled; clamp before it indexes ps.ammo[] (and
		// cg_weapons[] below). Out-of-range → WP_NONE (no weapon shown).
		if ( (unsigned)wp >= WP_NUM_WEAPONS ) wp = WP_NONE;
		ammoVal = cg.snap->ps.ammo[wp];

		/* ── player.health ──────────────────────────────── */
		Com_sprintf( buf, sizeof( buf ), "%d", hp > 0 ? hp : 0 );
		WUI_Stage_SetString( "player.health.text", buf );
		WUI_Stage_SetFloat( "player.health.value", (float)hp );
		WUI_Stage_SetFloat( "player.health.percent", Com_Clamp( 0.0f, 1.0f, hp / 100.0f ) );

		/* health color from effective health */
		BG_GetColorForAmount( state.effectiveHealth, color );
		WUI_Stage_SetColor( "player.health.color", color );

		/* health icon */
		WUI_Stage_SetIcon( "player.health.icon", cgs.media.healthIcon );

		/* health semantic state */
		if ( state.effectiveHealth < 25 ) {
			WUI_Stage_SetState( "player.health.state", "critical" );
		} else if ( state.effectiveHealth < 50 ) {
			WUI_Stage_SetState( "player.health.state", "warning" );
		} else if ( hp > 100 ) {
			WUI_Stage_SetState( "player.health.state", "overhealed" );
		} else {
			WUI_Stage_SetState( "player.health.state", "normal" );
		}

		/* ── player.armor ───────────────────────────────── */
		Com_sprintf( buf, sizeof( buf ), "%d", ap > 0 ? ap : 0 );
		WUI_Stage_SetString( "player.armor.text", buf );
		WUI_Stage_SetFloat( "player.armor.value", (float)ap );
		WUI_Stage_SetFloat( "player.armor.percent", Com_Clamp( 0.0f, 1.0f, ap / 200.0f ) );

		/* armor color */
		BG_GetColorForAmount( ap, color );
		WUI_Stage_SetColor( "player.armor.color", color );

		/* armor icon by armor class */
		{
			qhandle_t armorIcon;
			int armorClass = cg.snap->ps.stats[STAT_ARMORCLASS];
			if ( armorClass == ARM_HEAVY ) {
				armorIcon = cgs.media.heavyArmorIcon;
			} else if ( armorClass == ARM_COMBAT ) {
				armorIcon = cgs.media.combatArmorIcon;
			} else { // if ( armorClass == ARM_JACKET ) {
				armorIcon = cgs.media.jacketArmorIcon;
			}
			WUI_Stage_SetIcon( "player.armor.icon", armorIcon );
		}

		/* armor semantic state */
		if ( ap <= 0 ) {
			WUI_Stage_SetState( "player.armor.state", "neutral" );
		} else if ( ap < 50 ) {
			WUI_Stage_SetState( "player.armor.state", "warning" );
		} else {
			WUI_Stage_SetState( "player.armor.state", "normal" );
		}

		/* ── player.ammo ────────────────────────────────── */
		/* HUD redesign (Eser): infinite-ammo weapons (ammoVal < 0, e.g. the
		   gauntlet) show the ∞ glyph instead of a number, and the readout stays
		   VISIBLE rather than hidden. The decision is VALUE-based (ammoVal < 0),
		   not name-based (was: wp == WP_GAUNTLET) — so any infinite weapon (a
		   mod's infinite-ammo cvar, a future weapon) is covered with no extra
		   code. ∞ = U+221E, present in the oxanium atlas (charset_latin.txt). */
		if ( ammoVal < 0 ) {
			Q_strncpyz( buf, "\xE2\x88\x9E", sizeof( buf ) ); /* UTF-8 ∞ (U+221E) */
		} else {
			Com_sprintf( buf, sizeof( buf ), "%d", ammoVal > 0 ? ammoVal : 0 );
		}
		WUI_Stage_SetString( "player.ammo.text", buf );
		WUI_Stage_SetFloat( "player.ammo.value", (float)ammoVal );
		/* bar fills against the CURRENT weapon's max ammo, not a fixed 200 (GL max
		   50 → 50/50 reads full). infinite → full bar; finite → proportional. Com_Clamp
		   would floor the -1 sentinel to 0 (empty bar), so special-case the infinite
		   fill (and the -1 no-cap maxAmmunition). Keep in sync with the bindings copy. */
		{
			int maxAmmo = bg_weaponlist[wp].maxAmmunition;
			WUI_Stage_SetFloat( "player.ammo.percent",
				( ammoVal < 0 || maxAmmo <= 0 ) ? 1.0f
					: Com_Clamp( 0.0f, 1.0f, (float)ammoVal / (float)maxAmmo ) );
		}

		/* ammo color: amber-neutral for infinite, red only for a real empty (0) */
		if ( ammoVal == 0 ) {
			Vector4Set( color, 1.0f, 0.0f, 0.0f, 1.0f );
		} else {
			Vector4Set( color, 1.0f, 1.0f, 1.0f, 1.0f );
		}
		WUI_Stage_SetColor( "player.ammo.color", color );

		/* ammo icon */
		WUI_Stage_SetIcon( "player.ammo.icon", cg_weapons[wp].ammoIcon );

		/* ammo semantic state + visibility. Only a true no-weapon (WP_NONE)
		   hides the readout now; the gauntlet (infinite) stays shown as ∞. */
		if ( wp == WP_NONE ) {
			WUI_Stage_SetState( "player.ammo.state", "neutral" );
			WUI_Stage_SetString( "player.ammo.visible", "0" );
		} else if ( ammoVal < 0 ) {
			WUI_Stage_SetState( "player.ammo.state", "normal" );
			WUI_Stage_SetString( "player.ammo.visible", "1" );
		} else if ( ammoVal == 0 ) {
			WUI_Stage_SetState( "player.ammo.state", "critical" );
			WUI_Stage_SetString( "player.ammo.visible", "1" );
		} else if ( cg.lowAmmoWarning >= 1 ) {
			WUI_Stage_SetState( "player.ammo.state", "warning" );
			WUI_Stage_SetString( "player.ammo.visible", "1" );
		} else {
			WUI_Stage_SetState( "player.ammo.state", "normal" );
			WUI_Stage_SetString( "player.ammo.visible", "1" );
		}

		/* ── player.weapon ──────────────────────────────── */
		WUI_Stage_SetInt( "player.weapon.current", wp );
		WUI_Stage_SetString( "player.weapon.name", bg_weaponlist[wp].name );
		/* D3: short uppercase tag (e.g. "RL") for the compact bottom-right ammo
		 * panel caption — the full .name ("Rocket Launcher") overran it. */
		{
			char shortUp[8];
			int  si;
			const char *sn = bg_weaponlist[wp].shortname;
			for ( si = 0; si < (int)sizeof(shortUp) - 1 && sn && sn[si]; si++ ) {
				char c = sn[si];
				shortUp[si] = ( c >= 'a' && c <= 'z' ) ? (char)( c - 'a' + 'A' ) : c;
			}
			shortUp[si] = '\0';
			WUI_Stage_SetString( "player.weapon.shortname", shortUp );
		}
		WUI_Stage_SetIcon( "player.weapon.icon", cg_weapons[wp].weaponIcon );

		/* ── match.score ────────────────────────────────── */
		WUI_Stage_SetInt( "match.score.own", cg.snap->ps.persistant[PERS_SCORE] );
		WUI_Stage_SetInt( "match.score.red", cgs.scores1 );
		WUI_Stage_SetInt( "match.score.blue", cgs.scores2 );

		/* ── Write scoreboard data to Wired Store ──────────────────── */
		{
			int si, totalKills;
			char key[128];

			WUI_Stage_SetInt( "game.scores.count", state.numScores );
			WUI_Stage_SetInt( "game.scores.gametype", state.gametype );

			for ( si = 0; si < state.numScores && si < WIRED_HUD_MAX_SCORES; si++ ) {
				wiredHudScore_t *sc = &state.scores[si];
				int cn = sc->client;

				/* name */
				Com_sprintf( key, sizeof(key), "game.scores.%d.name", si );
				if ( cn >= 0 && cn < WIRED_HUD_MAX_CLIENTS ) {
					WUI_Stage_SetString( key, state.clients[cn].name );
				} else {
					WUI_Stage_SetString( key, "???" );
				}

				/* team */
				Com_sprintf( key, sizeof(key), "game.scores.%d.team", si );
				WUI_Stage_SetInt( key, sc->team );

				/* highlight (local player) */
				Com_sprintf( key, sizeof(key), "game.scores.%d.highlight", si );
				WUI_Stage_SetFloat( key, (cn == state.clientNum) ? 1.0f : 0.0f );

				/* client index */
				Com_sprintf( key, sizeof(key), "game.scores.%d.client", si );
				WUI_Stage_SetInt( key, cn );

				/* rank */
				Com_sprintf( key, sizeof(key), "game.scores.%d.rank", si );
				Com_sprintf( buf, sizeof(buf), "%d", si + 1 );
				WUI_Stage_SetString( key, buf );

				/* score */
				Com_sprintf( key, sizeof(key), "game.scores.%d.score", si );
				Com_sprintf( buf, sizeof(buf), "%d", sc->score );
				WUI_Stage_SetString( key, buf );

				/* K/D */
				totalKills = 0;
				{
					int w;
					for ( w = 0; w < WIRED_MAX_WEAPONS; w++ ) {
						totalKills += sc->weaponStats[w].kills;
					}
				}

				Com_sprintf( key, sizeof(key), "game.scores.%d.kd", si );
				Com_sprintf( buf, sizeof(buf), "%d/%d", totalKills, sc->deaths );
				WUI_Stage_SetString( key, buf );

				/* K/D color */
				Com_sprintf( key, sizeof(key), "game.scores.%d.kdcolor", si );
				if ( sc->deaths == 0 && totalKills > 0 ) {
					Vector4Set( color, 0.2f, 1.0f, 0.2f, 1.0f ); /* bright green */
				} else if ( totalKills >= sc->deaths ) {
					Vector4Set( color, 0.6f, 1.0f, 0.6f, 1.0f ); /* light green */
				} else {
					Vector4Set( color, 1.0f, 0.4f, 0.4f, 1.0f ); /* red */
				}
				WUI_Stage_SetColor( key, color );

				/* efficiency */
				{
					int eff;
					if ( totalKills + sc->deaths > 0 ) {
						eff = (int)( 100.0f * totalKills / (totalKills + sc->deaths) );
					} else {
						eff = 0;
					}
					Com_sprintf( key, sizeof(key), "game.scores.%d.eff", si );
					Com_sprintf( buf, sizeof(buf), "%d%%", eff );
					WUI_Stage_SetString( key, buf );

					Com_sprintf( key, sizeof(key), "game.scores.%d.effcolor", si );
					if ( eff >= 60 ) {
						Vector4Set( color, 0.2f, 1.0f, 0.2f, 1.0f );
					} else if ( eff >= 40 ) {
						Vector4Set( color, 1.0f, 1.0f, 0.2f, 1.0f );
					} else {
						Vector4Set( color, 1.0f, 1.0f, 1.0f, 1.0f );
					}
					WUI_Stage_SetColor( key, color );
				}

				/* damage */
				Com_sprintf( key, sizeof(key), "game.scores.%d.damage", si );
				if ( sc->damageDone >= 100000 ) {
					Com_sprintf( buf, sizeof(buf), "%dk", sc->damageDone / 1000 );
				} else if ( sc->damageDone >= 10000 ) {
					Com_sprintf( buf, sizeof(buf), "%.1fk", sc->damageDone / 1000.0f );
				} else {
					Com_sprintf( buf, sizeof(buf), "%d", sc->damageDone );
				}
				WUI_Stage_SetString( key, buf );

				/* accuracy */
				Com_sprintf( key, sizeof(key), "game.scores.%d.acc", si );
				Com_sprintf( buf, sizeof(buf), "%d%%", sc->accuracy );
				WUI_Stage_SetString( key, buf );

				Com_sprintf( key, sizeof(key), "game.scores.%d.acccolor", si );
				if ( sc->accuracy >= 50 ) {
					Vector4Set( color, 0.2f, 1.0f, 0.2f, 1.0f );
				} else if ( sc->accuracy >= 30 ) {
					Vector4Set( color, 1.0f, 1.0f, 0.2f, 1.0f );
				} else {
					Vector4Set( color, 1.0f, 1.0f, 1.0f, 1.0f );
				}
				WUI_Stage_SetColor( key, color );

				/* ping */
				Com_sprintf( key, sizeof(key), "game.scores.%d.ping", si );
				if ( sc->ping == -1 ) {
					WUI_Stage_SetString( key, "..." );
				} else {
					Com_sprintf( buf, sizeof(buf), "%d", sc->ping );
					WUI_Stage_SetString( key, buf );
				}

				Com_sprintf( key, sizeof(key), "game.scores.%d.pingcolor", si );
				if ( sc->ping < 0 ) {
					Vector4Set( color, 0.2f, 1.0f, 0.2f, 1.0f );
				} else if ( sc->ping < 40 ) {
					Vector4Set( color, 0.2f, 1.0f, 0.2f, 1.0f );
				} else if ( sc->ping < 100 ) {
					Vector4Set( color, 1.0f, 1.0f, 0.2f, 1.0f );
				} else {
					Vector4Set( color, 1.0f, 0.3f, 0.3f, 1.0f );
				}
				WUI_Stage_SetColor( key, color );
			}
		}

		/* ── game mode state ────────────────────────────────────── */
		if ( cgs.gametype == GT_DUEL ) {
			WUI_Stage_SetString( "game.warmup.message", "^BWaiting for Opponent" );
		} else {
			WUI_Stage_SetString( "game.warmup.message", "^BWaiting for Players" );
		}

		/* ── crosshair target state ─────────────────────────────── */
		{
			int xhairClient = cg.crosshairClientNum;
			int myTeam = cg.snap->ps.persistant[PERS_TEAM];
			qboolean isTeammate = qfalse;
			qboolean showName = qtrue;
			qboolean targetExists = qfalse;
			int targetArmorClass = 0;

			if ( xhairClient >= 0 && xhairClient < MAX_CLIENTS &&
			     cgs.clientinfo[xhairClient].infoValid ) {
				int targetTeam = cgs.clientinfo[xhairClient].team;
				isTeammate = ( targetTeam != TEAM_FREE && targetTeam == myTeam );
				targetExists = qtrue;
				targetArmorClass = cgs.clientinfo[xhairClient].armorClass;

				/* showName: hide enemy names in team games when cvar == 2 */
				if ( cgs.gametypeIsTeamGame && cg_drawCrosshairNames.integer == 2 ) {
					if ( targetTeam != myTeam ) {
						showName = qfalse;
					}
				}
			}

			WUI_Stage_SetInt( "crosshair.isTeammate", isTeammate ? 1 : 0 );
			WUI_Stage_SetInt( "crosshair.showName", showName ? 1 : 0 );

			/* ── procedural-crosshair state (Wired Crosshair) ──
			 * Self-state + instantaneous trace target, staged for the
			 * client-side Lua update(state) pipeline. recoil is DEFERRED
			 * (real bg_ weapon model is a separate workstream) — not staged;
			 * the pipeline defaults it to 0. */
			{
				int wp = cg.snap->ps.weapon;
				char wtoken[64];
				qboolean isCrouching =
					( cg.predictedPlayerState.pm_flags & PMF_DUCKED ) ? qtrue : qfalse;

				CG_CrosshairWeaponToken( wp, wtoken, sizeof( wtoken ) );

				WUI_Stage_SetString( "crosshair.weapon", wtoken );
				WUI_Stage_SetInt( "crosshair.weapon_status", cg.snap->ps.weaponstate );
				WUI_Stage_SetFloat( "crosshair.speed", cg.xyspeed );
				WUI_Stage_SetInt( "crosshair.is_firing",
					( cg.snap->ps.weaponstate == WEAPON_FIRING ) ? 1 : 0 );
				WUI_Stage_SetInt( "crosshair.is_crouching", isCrouching ? 1 : 0 );
				WUI_Stage_SetFloat( "crosshair.charge", 0.0f );

				/* RS-5: stage the REAL deterministic fire-cone size (0..1 normalized)
				 * for the reticle to display — NOT a cosmetic proxy. This is the
				 * SAME BG_CalcWeaponSpreadNormalized the RS-3 server cone derives
				 * from, so the displayed spread matches the bullets (zero bandwidth:
				 * recomputed client-side from the already-synced fireRampStartTime).
				 * 🔴 Read from cg.predictedPlayerState + cg.time (the predicted ramp,
				 * maintained by cg_predict's PM_Weapon) — NOT cg.snap->ps, which lags
				 * the snapshot and would desync the reticle from the server cone. The
				 * active attack is derived the way the server selects its fire fn:
				 * a burst (burstRoundsRemaining>0) is the alt attack, else the
				 * primary. Returns 0 for no-cone weapons → no fire-spread shown. */
				{
					const playerState_t *pps = &cg.predictedPlayerState;
					int   w         = pps->weapon;
					int   attackIdx = ( pps->burstRoundsRemaining > 0 )
					                      ? bg_weaponlist[w].attackAlt
					                      : bg_weaponlist[w].attack;
					float coneN     = BG_CalcWeaponSpreadNormalized( pps, attackIdx, cg.time );
					WUI_Stage_SetFloat( "crosshair.recoil", coneN );
				}

				WUI_Stage_SetInt( "crosshair.target.exists", targetExists ? 1 : 0 );
				WUI_Stage_SetInt( "crosshair.target.armorClass", targetArmorClass );
			}
		}

		/* ── spectator list ─────────────────────────────────────── */
		{
			char specList[MAX_STRING_CHARS];
			int len = 0;
			int ci2;
			qboolean hasSpecs = qfalse;

			specList[0] = '\0';
			for ( ci2 = 0; ci2 < cgs.maxclients && ci2 < MAX_CLIENTS; ci2++ ) {
				if ( cgs.clientinfo[ci2].infoValid &&
				     cgs.clientinfo[ci2].team == TEAM_SPECTATOR ) {
					const char *name = cgs.clientinfo[ci2].name;
					if ( !name || !name[0] ) continue;
					if ( hasSpecs ) {
						Q_strncpyz( specList + len, ", ",
						            (int)sizeof(specList) - len );
						len = strlen( specList );
						if ( len >= (int)sizeof(specList) - 1 ) break;
					}
					Q_strncpyz( specList + len, name,
					            (int)sizeof(specList) - len );
					len = strlen( specList );
					if ( len >= (int)sizeof(specList) - 1 ) break;
					hasSpecs = qtrue;
				}
			}
			WUI_Stage_SetString( "game.spectators.list", specList );
		}

		/* ── bot coaching directives ──────────────────────────────────── */
		{
			static char dirList[MAX_STRING_CHARS];
			int len = 0;
			int ci;
			dirList[0] = '\0';
			if ( cg_drawBotDirectives.integer ) {
			for ( ci = 0; ci < MAX_CLIENTS; ci++ ) {
				const char *dirText;
				char line[128];
				if ( !cgs.clientinfo[ci].infoValid || !cgs.clientinfo[ci].botSkill ) continue;
				switch ( cg_botDirectives[ci].type ) {
					case 0:  dirText = "^7-";             break;
					case 1:  Com_sprintf( line, sizeof(line), "^6@ %s", cg_botDirectives[ci].targetName ); dirText = line; break;
					case 2:  dirText = "^5# Defend";      break;
					case 3:  dirText = "^5# Camp";        break;
					case 4:  dirText = "^5# Patrol";      break;
					case 5:  Com_sprintf( line, sizeof(line), "^3> %s", cg_botDirectives[ci].targetName ); dirText = line; break;
					case 6:  dirText = "^2< Rush Base";   break;
					case 7:  dirText = "^2< Return Flag"; break;
					case 8:  dirText = "^1X Attack Base"; break;
					case 9:  Com_sprintf( line, sizeof(line), "^1X %s", cg_botDirectives[ci].targetName ); dirText = line; break;
					case 10: dirText = "^3# Harvest";     break;
					default: continue;
				}
				{
					char entry[160];
					Com_sprintf( entry, sizeof(entry), "%s  %s\n", cgs.clientinfo[ci].name, dirText );
					Q_strncpyz( dirList + len, entry, sizeof(dirList) - len );
					len = strlen( dirList );
					if ( len >= (int)sizeof(dirList) - 1 ) break;
				}
			}
			} // if ( cg_drawBotDirectives.integer )
			WUI_Stage_SetString( "game.bots.directives", dirList );
		}

		/* Flush all staged entries in one batch syscall */
		WUI_Stage_Flush();

		/* WA-3: the world-anchored MARKER lists (markers.plums, markers.botdir)
		 * are now Begin/Push/FLUSHED as atomic cycles in the scene-build phase
		 * (CG_AddLocalEntities) — the single-list staging scratch holds one list at
		 * a time, so two lists can't both defer to a single flush here. The marker
		 * flush therefore moved out of this bridge (it is NOT tied to the scalar
		 * WUI_Stage_Flush above). */
	}

	// ── per-attack stats for local player ────────────────────────────
	{
		int clientNum = cg.snap->ps.clientNum;
		if ( clientNum >= 0 && clientNum < MAX_CLIENTS ) {
			for ( i = ATT_NONE + 1; i < ATT_NUM_ATTACKS; i++ ) {
				state.attackStats[i].hits   = cgs.attackStats[clientNum][i].hits;
				state.attackStats[i].shots  = cgs.attackStats[clientNum][i].shots;
				state.attackStats[i].kills  = cgs.attackStats[clientNum][i].kills;
				state.attackStats[i].deaths = cgs.attackStats[clientNum][i].deaths;
				state.attackStats[i].damage = cgs.attackStats[clientNum][i].damage;
			}
		}
	}

	// ── lagometer ────────────────────────────────────────────────────
	{
		// lagometer_t is a static in cg_draw.c — access via extern
		typedef struct {
			int frameSamples[WIRED_LAG_SAMPLES];
			int frameCount;
			int snapshotFlags[WIRED_LAG_SAMPLES];
			int snapshotSamples[WIRED_LAG_SAMPLES];
			int snapshotCount;
		} lagometer_t;
		extern lagometer_t lagometer;

		memcpy( state.lagometer.frameSamples, lagometer.frameSamples, sizeof( state.lagometer.frameSamples ) );
		state.lagometer.frameCount = lagometer.frameCount;
		memcpy( state.lagometer.snapshotFlags, lagometer.snapshotFlags, sizeof( state.lagometer.snapshotFlags ) );
		memcpy( state.lagometer.snapshotSamples, lagometer.snapshotSamples, sizeof( state.lagometer.snapshotSamples ) );
		state.lagometer.snapshotCount = lagometer.snapshotCount;
	}
	state.localServer = cgs.localServer;

	// ── TA compat: CG_SHOW_* display flags ──────────────────────────
	// These use the actual hex values from menudef.h that TA .menu files embed
	{
		unsigned int f = 0;
		int team = cg.snap->ps.persistant[PERS_TEAM];

		if ( cgs.blueflag == 2 /* FLAG_TAKEN_RED */ )   f |= 0x00000001; // CG_SHOW_BLUE_TEAM_HAS_REDFLAG
		if ( cgs.redflag == 1 /* FLAG_TAKEN_BLUE */ )   f |= 0x00000002; // CG_SHOW_RED_TEAM_HAS_BLUEFLAG
		if ( cgs.gametypeIsTeamGame )                   f |= 0x00000004; // CG_SHOW_ANYTEAMGAME
		if ( cgs.gametype == GT_HARVESTER )             f |= 0x00000008; // CG_SHOW_HARVESTER
		if ( cgs.gametype == GT_1FCTF )                 f |= 0x00000010; // CG_SHOW_ONEFLAG
		if ( cgs.gametype == GT_CTF )                   f |= 0x00000020; // CG_SHOW_CTF
		if ( cgs.gametype == GT_OBELISK )               f |= 0x00000040; // CG_SHOW_OBELISK
		if ( state.health < 25 )                        f |= 0x00000080; // CG_SHOW_HEALTHCRITICAL
		// if ( cgs.gametype == GT_SINGLE_PLAYER )         f |= 0x00000100; // CG_SHOW_SINGLEPLAYER
		if ( cgs.gametype == GT_DUEL )                  f |= 0x00000200; // CG_SHOW_DUEL
		if ( state.health >= 25 )                       f |= 0x00004000; // CG_SHOW_HEALTHOK
		if ( cg.snap->ps.powerups[PW_REDFLAG] ||
		     cg.snap->ps.powerups[PW_BLUEFLAG] )        f |= 0x00000800; // CG_SHOW_IF_PLAYER_HAS_FLAG
		if ( !cgs.gametypeIsTeamGame )                  f |= 0x00080000; // CG_SHOW_ANYNONTEAMGAME

		state.cgShowFlags = f;
	}

	// ── TA compat: UI_SHOW_* display flags ──────────────────────────
	{
		unsigned int uf = 0;
		int myScore = cg.snap->ps.persistant[PERS_SCORE];
		// simple leader check: are we #1?
		if ( myScore >= state.scores1 && myScore > 0 )   uf |= 0x00000001; // UI_SHOW_LEADER
		if ( myScore < state.scores1 )                   uf |= 0x00000002; // UI_SHOW_NOTLEADER
		if ( !cgs.gametypeIsTeamGame )                   uf |= 0x00000008; // UI_SHOW_ANYNONTEAMGAME
		if ( cgs.gametypeIsTeamGame )                    uf |= 0x00000010; // UI_SHOW_ANYTEAMGAME

		state.uiShowFlags = uf;
	}

	state.valid = qtrue;

	trap_WiredUI_PushHudState( &state );
}

#endif // FEAT_WIRED_UI
