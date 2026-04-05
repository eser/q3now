/*
===========================================================================
cl_wired_hud_elem_scoreboard.c — Scoreboard widget renderers

ITEM_TYPE_SCORELIST  (type 20): rich scoreboard list with per-cell coloring.
ITEM_TYPE_DUELBOARD  (type 21): CPMA-style duel scoreboard, two fighter panels.

The widget position/size comes from the .menu itemDef rect. The feeder ID
determines team filtering. Layout is data-driven from wiredHud->scores[].

Usage in .menu:
  itemDef { type 20  feeder 11  rect 0 0 500 300  forecolor 1 1 1 1 }
  itemDef { type 21  rect 0 0 1.0 1.0  forecolor 1 1 1 1 }
===========================================================================
*/

#include "../client.h"
#include "cl_wired_hud.h"
#include "cl_wired_hud_private.h"
#include "cl_wired_fonts.h"
#include "cl_wired_draw.h"
#include "../../qcommon/menudef.h"

#if FEAT_WIRED_UI

#define SB_ROW_H  18.0f

void WiredHud_DrawScorelistWidget( float ox, float oy, float ow, float oh,
	int feederID, const vec4_t textColor )
{
	int i, rank, numRows;
	float y;
	char tmp[128];
	const vec4_t colorWhite = { 1, 1, 1, 1 };
	vec4_t hdrColor;
	int teamFilter = -1;

	if ( !wiredHud || !wiredHud->valid || wiredHud->numScores <= 0 ) return;

	if ( feederID == FEEDER_REDTEAM_LIST )  teamFilter = TEAM_RED;
	if ( feederID == FEEDER_BLUETEAM_LIST ) teamFilter = TEAM_BLUE;

	/* relative column offsets within the widget rect */
	float scale    = ow / 500.0f;
	float colRank  = ox + 6  * scale;
	float colName  = ox + 34 * scale;
	float colScore = ox + 200 * scale;
	float colKD    = ox + 250 * scale;
	float colDmg   = ox + 300 * scale;
	float colAtt   = ox + 340 * scale;
	float colAcc   = ox + 400 * scale;
	float colPing  = ox + 470 * scale;

	/* ── column headers ───────────────────────────────────────────── */
	hdrColor[0] = 0.7f; hdrColor[1] = 0.8f; hdrColor[2] = 0.9f; hdrColor[3] = 1.0f;
	y = oy;
	CG_FontSelect( 0 );
	CG_ModernDrawString( colRank,       y, "#",      hdrColor, 8, 12, (int)ow, DS_HLEFT | DS_SHADOW, NULL );
	CG_ModernDrawString( colName,       y, "PLAYER", hdrColor, 8, 12, (int)ow, DS_HLEFT | DS_SHADOW, NULL );
	CG_ModernDrawString( colScore,      y, "SCORE",  hdrColor, 8, 12, (int)ow, DS_HRIGHT | DS_SHADOW, NULL );
	CG_ModernDrawString( colKD,         y, "K/D",    hdrColor, 8, 12, (int)ow, DS_HRIGHT | DS_SHADOW, NULL );
	CG_ModernDrawString( colDmg,        y, "DMG",    hdrColor, 8, 12, (int)ow, DS_HRIGHT | DS_SHADOW, NULL );
	CG_ModernDrawString( colAtt + 16 * scale, y, "ATT", hdrColor, 8, 12, (int)ow, DS_HCENTER | DS_SHADOW, NULL );
	CG_ModernDrawString( colAcc,        y, "ACC%",  hdrColor, 8, 12, (int)ow, DS_HRIGHT | DS_SHADOW, NULL );
	CG_ModernDrawString( colPing,       y, "PING",   hdrColor, 8, 12, (int)ow, DS_HRIGHT | DS_SHADOW, NULL );

	/* separator below headers */
	{
		vec4_t lineColor = { 0.3f, 0.3f, 0.4f, 0.4f };
		WUI_FillRect( ox + 4, y + 13, ow - 8, 1, lineColor );
	}

	/* ── player rows ──────────────────────────────────────────────── */
	y = oy + 16;
	rank = 0;
	numRows = (int)( ( oh - 16 ) / SB_ROW_H );

	for ( i = 0; i < wiredHud->numScores && i < WIRED_HUD_MAX_SCORES && rank < numRows; i++ ) {
		wiredHudScore_t *sc = &wiredHud->scores[i];
		int clientNum = sc->client;
		int acc, pingColor;

		if ( clientNum < 0 || clientNum >= WIRED_HUD_MAX_CLIENTS ) continue;
		if ( !wiredHud->clients[clientNum].infoValid ) continue;
		if ( sc->team == TEAM_SPECTATOR ) continue;
		if ( teamFilter >= 0 && sc->team != teamFilter ) continue;

		rank++;

		/* highlight current player */
		if ( clientNum == wiredHud->clientNum ) {
			vec4_t hlColor = { 0.15f, 0.2f, 0.4f, 0.35f };
			WUI_FillRect( ox + 2, y - 1, ow - 4, SB_ROW_H, hlColor );
		}

		/* alternating row shade */
		if ( rank % 2 == 0 ) {
			vec4_t altColor = { 0.1f, 0.1f, 0.15f, 0.2f };
			WUI_FillRect( ox + 2, y - 1, ow - 4, SB_ROW_H, altColor );
		}

		/* rank */
		CG_FontSelect( 0 );
		Com_sprintf( tmp, sizeof( tmp ), "%i", rank );
		CG_ModernDrawString( colRank + 10, y, tmp, colorWhite,
			10, 14, (int)ow, DS_HRIGHT | DS_SHADOW, NULL );

		/* player name */
		CG_FontSelect( 2 );
		CG_ModernDrawString( colName, y + 1, wiredHud->clients[clientNum].name, colorWhite,
			8, 13, (int)( colScore - colName - 8 ), DS_HLEFT | DS_PROPORTIONAL | DS_SHADOW, NULL );

		/* connecting — show "..." */
		if ( sc->ping == -1 ) {
			CG_FontSelect( 0 );
			CG_ModernDrawString( colScore, y, "^2...", colorWhite,
				10, 14, (int)ow, DS_HRIGHT | DS_SHADOW, NULL );
			y += SB_ROW_H;
			continue;
		}

		/* score */
		CG_FontSelect( 0 );
		Com_sprintf( tmp, sizeof( tmp ), "%i", sc->score );
		CG_ModernDrawString( colScore, y, tmp, colorWhite,
			10, 14, (int)ow, DS_HRIGHT | DS_SHADOW, NULL );

		/* K/D */
		Com_sprintf( tmp, sizeof( tmp ), "%i/%i", sc->score, sc->deaths );
		CG_ModernDrawString( colKD, y + 2, tmp, colorWhite,
			8, 12, (int)ow, DS_HRIGHT | DS_SHADOW, NULL );

		/* total damage (abbreviated: 1.2k, 15.3k, etc.) */
		if ( sc->totalDamage >= 100000 )
			Com_sprintf( tmp, sizeof( tmp ), "%ik", sc->totalDamage / 1000 );
		else if ( sc->totalDamage >= 10000 )
			Com_sprintf( tmp, sizeof( tmp ), "%i.%ik", sc->totalDamage / 1000,
				( sc->totalDamage % 1000 ) / 100 );
		else if ( sc->totalDamage >= 1000 )
			Com_sprintf( tmp, sizeof( tmp ), "%i.%ik", sc->totalDamage / 1000,
				( sc->totalDamage % 1000 ) / 100 );
		else
			Com_sprintf( tmp, sizeof( tmp ), "%i", sc->totalDamage );
		CG_ModernDrawString( colDmg, y + 2, tmp, colorWhite,
			8, 12, (int)ow, DS_HRIGHT | DS_SHADOW, NULL );

		/* best attack icon */
		{
			static int dbgWp = 0;
			if ( dbgWp++ % 600 == 0 )
				Com_Printf( "SB: client=%d bestWp=%d icon=%d dmg=%d\n",
					clientNum, sc->bestAttack,
					sc->bestAttack > 0 && sc->bestAttack < ATT_NUM_ATTACKS ? wiredHud->attackIcons[sc->bestAttack] : -1,
					sc->totalDamage );
		}
		if ( sc->bestAttack > ATT_NONE && sc->bestAttack < ATT_NUM_ATTACKS
			 && wiredHud->attackIcons[sc->bestAttack] ) {
			re.SetColor( colorWhite );
			WUI_DrawPic( colAtt + 8 * scale, y + 1, 14, 14,
				wiredHud->attackIcons[sc->bestAttack] );
			re.SetColor( NULL );
		}

		/* accuracy (green >50%, yellow 30-50%, red <30%) */
		acc = sc->accuracy;
		if ( acc > 50 )
			Com_sprintf( tmp, sizeof( tmp ), "^2%i%%", acc );
		else if ( acc > 30 )
			Com_sprintf( tmp, sizeof( tmp ), "^3%i%%", acc );
		else
			Com_sprintf( tmp, sizeof( tmp ), "^1%i%%", acc );
		CG_ModernDrawString( colAcc, y + 2, tmp, colorWhite,
			8, 12, (int)ow, DS_HRIGHT | DS_SHADOW, NULL );

		/* ping (green <40, yellow <100, red >=100) */
		if ( sc->ping < 40 )
			pingColor = 2;
		else if ( sc->ping < 100 )
			pingColor = 3;
		else
			pingColor = 1;
		Com_sprintf( tmp, sizeof( tmp ), "^%i%i", pingColor, sc->ping );
		CG_ModernDrawString( colPing, y + 2, tmp, colorWhite,
			8, 12, (int)ow, DS_HRIGHT | DS_SHADOW, NULL );

		y += SB_ROW_H;
	}

	/* ── spectators (only for FEEDER_SCOREBOARD — all players) ──── */
	if ( teamFilter < 0 ) {
		int numSpecs = 0, specCount = 0;
		float specY;

		for ( i = 0; i < wiredHud->numScores && i < WIRED_HUD_MAX_SCORES; i++ ) {
			if ( wiredHud->scores[i].team == TEAM_SPECTATOR )
				numSpecs++;
		}

		if ( numSpecs > 0 ) {
			vec4_t lineColor = { 0.3f, 0.3f, 0.4f, 0.4f };

			y += 6;
			WUI_FillRect( ox + 4, y, ow - 8, 1, lineColor );
			y += 4;

			CG_FontSelect( 2 );
			CG_ModernDrawString( ox + ow / 2.0f, y, "Spectators", hdrColor,
				8, 12, (int)ow, DS_HCENTER | DS_SHADOW, NULL );
			y += 14;

			specY = y;
			CG_FontSelect( 2 );
			for ( i = 0; i < wiredHud->numScores && i < WIRED_HUD_MAX_SCORES; i++ ) {
				int sx, cn;
				float sy;
				if ( wiredHud->scores[i].team != TEAM_SPECTATOR ) continue;
				cn = wiredHud->scores[i].client;
				if ( cn < 0 || cn >= WIRED_HUD_MAX_CLIENTS ) continue;
				if ( !wiredHud->clients[cn].infoValid ) continue;

				sx = ( specCount % 2 == 0 ) ? (int)( ox + 20 ) : (int)( ox + ow / 2 + 10 );
				sy = specY + ( specCount / 2 ) * 10;
				CG_ModernDrawString( sx, sy, wiredHud->clients[cn].name, colorWhite,
					7, 10, (int)( ow / 2 - 20 ), DS_HLEFT | DS_PROPORTIONAL | DS_SHADOW, NULL );
				specCount++;
			}
		}
	}
}

/*
===========================================================================
WiredHud_DrawDuelBoard — CPMA-style duel scoreboard with two fighter panels

ITEM_TYPE_DUELBOARD (type 21): renders two side-by-side fighter panels.
Each panel shows player name, score, W/L, accuracy, awards, per-weapon
stats table, and ping. Center area shows "vs" text and match timer.

Usage in .menu:
  itemDef { type 21  rect 0 0 1.0 1.0  forecolor 1 1 1 1 }
===========================================================================
*/

#define DUEL_ROW_H     14.0f
#define DUEL_PANEL_W   300.0f

static void WiredHud_DrawDuelFighterPanel( float px, float py, float pw,
	wiredHudScore_t *sc, float alpha )
{
	int clientNum, w, acc, eff, pingColor;
	int totalHits, totalShots, totalKills, totalDeaths;
	float wy;
	char tmp[128];
	const vec4_t colorWhite = { 1, 1, 1, 1 };
	vec4_t textColor, hdrColor, lineColor, awardColor;

	clientNum = sc->client;
	if ( clientNum < 0 || clientNum >= WIRED_HUD_MAX_CLIENTS ) return;
	if ( !wiredHud->clients[clientNum].infoValid ) return;

	textColor[0] = textColor[1] = textColor[2] = 1.0f; textColor[3] = 1.0f * alpha;
	hdrColor[0] = 0.6f; hdrColor[1] = 0.7f; hdrColor[2] = 0.8f; hdrColor[3] = 1.0f * alpha;
	lineColor[0] = 0.4f; lineColor[1] = 0.4f; lineColor[2] = 0.5f; lineColor[3] = 0.5f * alpha;
	awardColor[0] = 1.0f; awardColor[1] = 0.85f; awardColor[2] = 0.0f; awardColor[3] = 1.0f * alpha;

	/* panel background — compact height to leave room for spectators */
	{
		vec4_t panelBg = { 0.08f, 0.08f, 0.12f, 0.65f };
		panelBg[3] *= alpha;
		WUI_FillRect( px, py, pw, 280.0f, panelBg );
	}

	/* header area background */
	{
		vec4_t hdrBg = { 0.12f, 0.12f, 0.2f, 0.5f };
		hdrBg[3] *= alpha;
		WUI_FillRect( px, py, pw, 52.0f, hdrBg );
	}

	/* player head icon */
	if ( wiredHud->headIcons[clientNum] ) {
		vec4_t iconColor = { 1, 1, 1, 1 };
		iconColor[3] = alpha;
		re.SetColor( iconColor );
		WUI_DrawPic( px + 6, py + 4, 28.0f, 28.0f, wiredHud->headIcons[clientNum] );
		re.SetColor( NULL );
	}

	/* player name (shifted right for head icon) */
	CG_FontSelect( 2 );
	CG_ModernDrawString( px + 38, py + 6, wiredHud->clients[clientNum].name,
		textColor, 12, 16, (int)( pw - 46 ),
		DS_HLEFT | DS_PROPORTIONAL | DS_SHADOW, NULL );

	/* big score */
	CG_FontSelect( 0 );
	Com_sprintf( tmp, sizeof( tmp ), "%i", sc->score );
	CG_ModernDrawString( px + pw - 8, py + 2, tmp, textColor,
		20, 28, (int)pw, DS_HRIGHT | DS_SHADOW, NULL );

	/* sub-header: acc, W/L, ping (shifted right for head icon) */
	CG_FontSelect( 0 );
	Com_sprintf( tmp, sizeof( tmp ), "Acc: %i%%  W:%i L:%i  Ping: %ims",
		sc->accuracy, sc->wins, sc->losses, sc->ping );
	CG_ModernDrawString( px + 38, py + 28, tmp, textColor,
		6, 10, (int)( pw - 46 ), DS_HLEFT | DS_SHADOW, NULL );

	/* award badges */
	{
		float bx = px + 38;
		float by = py + 40;
		CG_FontSelect( 0 );
		if ( sc->excellentCount > 0 ) {
			Com_sprintf( tmp, sizeof( tmp ), "^3Exc:%i", sc->excellentCount );
			CG_ModernDrawString( bx, by, tmp, awardColor, 6, 9,
				(int)pw, DS_HLEFT | DS_SHADOW, NULL );
			bx += 48;
		}
		if ( sc->impressiveCount > 0 ) {
			Com_sprintf( tmp, sizeof( tmp ), "^3Imp:%i", sc->impressiveCount );
			CG_ModernDrawString( bx, by, tmp, awardColor, 6, 9,
				(int)pw, DS_HLEFT | DS_SHADOW, NULL );
			bx += 48;
		}
		if ( sc->guantletCount > 0 ) {
			Com_sprintf( tmp, sizeof( tmp ), "^3Gnt:%i", sc->guantletCount );
			CG_ModernDrawString( bx, by, tmp, awardColor, 6, 9,
				(int)pw, DS_HLEFT | DS_SHADOW, NULL );
		}
	}

	/* separator below header */
	WUI_FillRect( px + 4, py + 52, pw - 8, 1.0f, lineColor );

	/* weapon table column positions — adjusted for alignment */
	{
		float cKills = px + 36;
		float cEff   = px + 76;
		float cIcon  = px + 96;
		float cHits  = px + 200;
		float cAcc   = px + pw - 8;

		/* column headers */
		wy = py + 66;
		CG_FontSelect( 0 );
		CG_ModernDrawString( cKills,   wy, "K/D",       hdrColor, 6, 10, (int)pw, DS_HRIGHT | DS_SHADOW, NULL );
		CG_ModernDrawString( cEff,  wy, "EFF%",      hdrColor, 6, 10, (int)pw, DS_HRIGHT | DS_SHADOW, NULL );
		CG_ModernDrawString( cHits, wy, "HITS/ATTS", hdrColor, 6, 10, (int)pw, DS_HRIGHT | DS_SHADOW, NULL );
		CG_ModernDrawString( cAcc,  wy, "ACC%",      hdrColor, 6, 10, (int)pw, DS_HRIGHT | DS_SHADOW, NULL );

		WUI_FillRect( px + 4, wy + 11, pw - 8, 1.0f, lineColor );
		wy += 14;

		/* per-weapon rows */
		totalHits = 0; totalShots = 0; totalKills = 0; totalDeaths = 0;
		for ( w = WP_NONE + 1; w < WP_NUM_WEAPONS && w < WIRED_MAX_WEAPONS; w++ ) {
			int wHits   = sc->weaponStats[w].hits;
			int wShots  = sc->weaponStats[w].shots;
			int wKills  = sc->weaponStats[w].kills;
			int wDeaths = sc->weaponStats[w].deaths;

			if ( wShots == 0 && wKills == 0 && wDeaths == 0 )
				continue;

			totalHits += wHits;
			totalShots += wShots;
			totalKills += wKills;
			totalDeaths += wDeaths;

			/* alternating row bg */
			if ( ((int)( wy - py ) / 14) % 2 == 0 ) {
				vec4_t altBg = { 0.1f, 0.1f, 0.15f, 0.25f };
				WUI_FillRect( px + 2, wy - 1, pw - 4, DUEL_ROW_H, altBg );
			}

			CG_FontSelect( 0 );

			/* K/D */
			Com_sprintf( tmp, sizeof( tmp ), "%i/%i", wKills, wDeaths );
			CG_ModernDrawString( cKills, wy, tmp, textColor, 7, 11,
				(int)pw, DS_HRIGHT | DS_SHADOW, NULL );

			/* efficiency */
			eff = ( wKills > 0 && wShots > 0 ) ? ( wKills * 100 / wShots ) : 0;
			Com_sprintf( tmp, sizeof( tmp ), "%i%%", eff );
			CG_ModernDrawString( cEff, wy, tmp, textColor, 7, 11,
				(int)pw, DS_HRIGHT | DS_SHADOW, NULL );

			/* weapon icon */
			if ( wiredHud->weaponIcons[w] ) {
				re.SetColor( colorWhite );
				WUI_DrawPic( cIcon, wy, 12.0f, 12.0f, wiredHud->weaponIcons[w] );
				re.SetColor( NULL );
			} else {
				/* fallback: weapon name from bg_weaponlist */
				CG_ModernDrawString( cIcon, wy, bg_weaponlist[w].shortname ?
					bg_weaponlist[w].shortname : bg_weaponlist[w].name,
					hdrColor, 5, 9, 60, DS_HLEFT | DS_SHADOW, NULL );
			}

			/* hits/shots */
			Com_sprintf( tmp, sizeof( tmp ), "%i/%i", wHits, wShots );
			CG_ModernDrawString( cHits, wy, tmp, textColor, 7, 11,
				(int)pw, DS_HRIGHT | DS_SHADOW, NULL );

			/* accuracy */
			acc = ( wShots > 0 ) ? ( wHits * 100 / wShots ) : 0;
			if ( acc > 50 )
				Com_sprintf( tmp, sizeof( tmp ), "^2%i%%", acc );
			else if ( acc > 30 )
				Com_sprintf( tmp, sizeof( tmp ), "^3%i%%", acc );
			else
				Com_sprintf( tmp, sizeof( tmp ), "^1%i%%", acc );
			CG_ModernDrawString( cAcc, wy, tmp, colorWhite, 7, 11,
				(int)pw, DS_HRIGHT | DS_SHADOW, NULL );

			wy += DUEL_ROW_H;
		}

		/* summary row */
		WUI_FillRect( px + 4, wy - 1, pw - 8, 1.0f, lineColor );
		wy += 3;
		{
			vec4_t sumColor = { 0.9f, 0.9f, 1.0f, 1.0f };
			CG_FontSelect( 0 );

			Com_sprintf( tmp, sizeof( tmp ), "%i/%i", totalKills, totalDeaths );
			CG_ModernDrawString( cKills, wy, tmp, sumColor, 7, 11,
				(int)pw, DS_HRIGHT | DS_SHADOW, NULL );

			CG_ModernDrawString( cIcon, wy, "TOTAL", sumColor, 5, 9,
				60, DS_HLEFT | DS_SHADOW, NULL );

			Com_sprintf( tmp, sizeof( tmp ), "%i/%i", totalHits, totalShots );
			CG_ModernDrawString( cHits, wy, tmp, sumColor, 7, 11,
				(int)pw, DS_HRIGHT | DS_SHADOW, NULL );

			acc = ( totalShots > 0 ) ? ( totalHits * 100 / totalShots ) : 0;
			Com_sprintf( tmp, sizeof( tmp ), "%i%%", acc );
			CG_ModernDrawString( cAcc, wy, tmp, sumColor, 7, 11,
				(int)pw, DS_HRIGHT | DS_SHADOW, NULL );
		}
	}

	/* ping at bottom of panel */
	{
		CG_FontSelect( 0 );
		if ( sc->ping < 40 )
			pingColor = 2;
		else if ( sc->ping < 100 )
			pingColor = 3;
		else
			pingColor = 1;
		Com_sprintf( tmp, sizeof( tmp ), "Ping: ^%i%i", pingColor, sc->ping );
		CG_ModernDrawString( px + 8, py + 266, tmp, textColor,
			6, 10, (int)pw, DS_HLEFT | DS_SHADOW, NULL );
	}
}

static void WiredHud_DrawDuelEmptyPanel( float px, float py, float pw, float alpha )
{
	vec4_t panelBg = { 0.08f, 0.08f, 0.12f, 0.4f * alpha };
	vec4_t textColor = { 0.5f, 0.5f, 0.6f, 0.8f * alpha };

	WUI_FillRect( px, py, pw, 280.0f, panelBg );

	CG_FontSelect( 0 );
	CG_ModernDrawString( px + pw / 2.0f, py + 140, "Waiting...",
		textColor, 10, 14, (int)pw, DS_HCENTER | DS_SHADOW, NULL );
}

void WiredHud_DrawDuelBoard( float ox, float oy, float ow, float oh )
{
	static int duelIntermissionStart = 0;
	static qboolean wasIntermission = qfalse;

	int i, numFighters;
	wiredHudScore_t *fighters[2] = { NULL, NULL };
	char tmp[128];
	const vec4_t colorWhite = { 1, 1, 1, 1 };
	vec4_t vsColor;
	float panelW, leftX, rightX, panelY, centerX;
	float panelAlpha = 1.0f;

	if ( !wiredHud || !wiredHud->valid ) return;

	/* find up to 2 non-spectator players */
	numFighters = 0;
	for ( i = 0; i < wiredHud->numScores && i < WIRED_HUD_MAX_SCORES && numFighters < 2; i++ ) {
		if ( wiredHud->scores[i].team == TEAM_SPECTATOR ) continue;
		if ( wiredHud->scores[i].client < 0 || wiredHud->scores[i].client >= WIRED_HUD_MAX_CLIENTS ) continue;
		if ( !wiredHud->clients[wiredHud->scores[i].client].infoValid ) continue;
		fighters[numFighters++] = &wiredHud->scores[i];
	}

	/* layout constants */
	panelW  = DUEL_PANEL_W;
	leftX   = ox + 8;
	rightX  = ox + ow - panelW - 8;
	panelY  = oy + 44;
	centerX = ox + ow / 2.0f;

	/* ── intermission: animated winner banner + match duration ── */
	if ( wiredHud->intermission ) {
		vec4_t goldColor  = { 1.0f, 0.85f, 0.0f, 1.0f };
		vec4_t titleColor = { 0.9f, 0.9f, 1.0f, 1.0f };
		vec4_t durColor   = { 0.7f, 0.8f, 0.9f, 1.0f };
		int winner = -1; /* 0=left wins, 1=right wins, -1=draw */
		int animElapsed;
		float titleAlpha, winnerAlpha;

		/* track intermission start for animation timing */
		if ( !wasIntermission ) {
			duelIntermissionStart = wiredHud->time;
			wasIntermission = qtrue;
		}
		animElapsed = wiredHud->time - duelIntermissionStart;
		if ( animElapsed < 0 ) animElapsed = 0;

		/* Phase 1 (0-500ms): "MATCH OVER" fades in */
		titleAlpha = ( animElapsed < 500 ) ? (float)animElapsed / 500.0f : 1.0f;

		/* Phase 2 (500-1200ms): "WINNER"/"DRAW" fades in */
		if ( animElapsed < 500 ) {
			winnerAlpha = 0.0f;
		} else if ( animElapsed < 1200 ) {
			winnerAlpha = (float)( animElapsed - 500 ) / 700.0f;
		} else {
			winnerAlpha = 1.0f;
		}

		/* Fighter panels fade in starting at ~300ms over 600ms */
		if ( animElapsed < 300 ) {
			panelAlpha = 0.0f;
		} else if ( animElapsed < 900 ) {
			panelAlpha = (float)( animElapsed - 300 ) / 600.0f;
		} else {
			panelAlpha = 1.0f;
		}

		/* apply title alpha */
		titleColor[3] = titleAlpha;
		durColor[3] *= titleAlpha;

		/* determine winner */
		if ( fighters[0] && fighters[1] ) {
			if ( fighters[0]->score > fighters[1]->score )
				winner = 0;
			else if ( fighters[1]->score > fighters[0]->score )
				winner = 1;
		} else if ( fighters[0] ) {
			winner = 0;
		} else if ( fighters[1] ) {
			winner = 1;
		}

		/* "MATCH OVER" title */
		CG_FontSelect( 0 );
		CG_ModernDrawString( centerX, oy + 2, "MATCH OVER", titleColor,
			12, 16, (int)ow, DS_HCENTER | DS_SHADOW, NULL );

		/* winner banner or draw */
		goldColor[3] = winnerAlpha;
		if ( winner == 0 ) {
			/* WINNER above left panel */
			CG_FontSelect( 0 );
			CG_ModernDrawString( leftX + panelW / 2.0f, oy + 20, "WINNER",
				goldColor, 14, 20, (int)panelW, DS_HCENTER | DS_SHADOW, NULL );
		} else if ( winner == 1 ) {
			/* WINNER above right panel */
			CG_FontSelect( 0 );
			CG_ModernDrawString( rightX + panelW / 2.0f, oy + 20, "WINNER",
				goldColor, 14, 20, (int)panelW, DS_HCENTER | DS_SHADOW, NULL );
		} else {
			/* DRAW centered */
			CG_FontSelect( 0 );
			CG_ModernDrawString( centerX, oy + 20, "DRAW",
				goldColor, 14, 20, (int)ow, DS_HCENTER | DS_SHADOW, NULL );
		}

		/* match duration */
		if ( wiredHud->levelStartTime > 0 ) {
			int elapsed = ( wiredHud->time - wiredHud->levelStartTime ) / 1000;
			int mins, secs;
			if ( elapsed < 0 ) elapsed = 0;
			mins = elapsed / 60;
			secs = elapsed % 60;
			Com_sprintf( tmp, sizeof( tmp ), "Match Duration: %i:%02i", mins, secs );
			CG_ModernDrawString( centerX, oy + 42, tmp, durColor, 8, 12,
				(int)ow, DS_HCENTER | DS_SHADOW, NULL );
		}

		/* shift panels down to accommodate the banner */
		panelY = oy + 60;
	} else {
		/* reset intermission tracking when not in intermission */
		wasIntermission = qfalse;
		duelIntermissionStart = 0;

		/* ── center top: "vs" and match timer ──────────────────── */
		vsColor[0] = 0.9f; vsColor[1] = 0.2f; vsColor[2] = 0.2f; vsColor[3] = 1.0f;
		CG_FontSelect( 0 );
		CG_ModernDrawString( centerX, oy + 6, "vs", vsColor, 14, 20,
			(int)ow, DS_HCENTER | DS_SHADOW, NULL );

		/* match timer */
		if ( wiredHud->levelStartTime > 0 ) {
			int elapsed = ( wiredHud->time - wiredHud->levelStartTime ) / 1000;
			int mins, secs;
			if ( elapsed < 0 ) elapsed = 0;
			mins = elapsed / 60;
			secs = elapsed % 60;
			Com_sprintf( tmp, sizeof( tmp ), "%i:%02i", mins, secs );
			CG_ModernDrawString( centerX, oy + 28, tmp, colorWhite, 8, 12,
				(int)ow, DS_HCENTER | DS_SHADOW, NULL );
		}
	}

	/* ── left fighter panel ────────────────────────────────────── */
	if ( fighters[0] )
		WiredHud_DrawDuelFighterPanel( leftX, panelY, panelW, fighters[0], panelAlpha );
	else
		WiredHud_DrawDuelEmptyPanel( leftX, panelY, panelW, panelAlpha );

	/* ── right fighter panel ───────────────────────────────────── */
	if ( fighters[1] )
		WiredHud_DrawDuelFighterPanel( rightX, panelY, panelW, fighters[1], panelAlpha );
	else
		WiredHud_DrawDuelEmptyPanel( rightX, panelY, panelW, panelAlpha );

	/* ── spectators at bottom ──────────────────────────────────── */
	{
		int numSpecs = 0, specCount = 0;
		float specY;
		vec4_t hdrColor = { 0.7f, 0.8f, 0.9f, 1.0f };

		for ( i = 0; i < wiredHud->numScores && i < WIRED_HUD_MAX_SCORES; i++ ) {
			if ( wiredHud->scores[i].team == TEAM_SPECTATOR )
				numSpecs++;
		}

		if ( numSpecs > 0 ) {
			vec4_t lineColor = { 0.3f, 0.3f, 0.4f, 0.4f };

			specY = panelY + 286;
			WUI_FillRect( ox + 20, specY, ow - 40, 1, lineColor );
			specY += 4;

			CG_FontSelect( 2 );
			CG_ModernDrawString( centerX, specY, "Spectators", hdrColor,
				8, 12, (int)ow, DS_HCENTER | DS_SHADOW, NULL );
			specY += 14;

			CG_FontSelect( 2 );
			for ( i = 0; i < wiredHud->numScores && i < WIRED_HUD_MAX_SCORES; i++ ) {
				int cn;
				float sx, sy;
				if ( wiredHud->scores[i].team != TEAM_SPECTATOR ) continue;
				cn = wiredHud->scores[i].client;
				if ( cn < 0 || cn >= WIRED_HUD_MAX_CLIENTS ) continue;
				if ( !wiredHud->clients[cn].infoValid ) continue;

				sx = ( specCount % 3 == 0 ) ? ox + 20 :
					 ( specCount % 3 == 1 ) ? centerX - 60 : centerX + 80;
				sy = specY + ( specCount / 3 ) * 10;
				CG_ModernDrawString( sx, sy, wiredHud->clients[cn].name,
					colorWhite, 7, 10, (int)( ow / 3 - 20 ),
					DS_HLEFT | DS_PROPORTIONAL | DS_SHADOW, NULL );
				specCount++;
			}
		}
	}
}

#endif /* FEAT_WIRED_UI */
