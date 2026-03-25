/*
===========================================================================
cl_wired_hud_elem_scoreboard.c — Scorelist widget renderer

ITEM_TYPE_SCORELIST (type 20): a rich scoreboard list widget for .menu files.
Renders rank, name, score, K/D, DMG, best weapon icon, colored accuracy,
colored ping.

The widget position/size comes from the .menu itemDef rect. The feeder ID
determines team filtering. Layout is data-driven from wiredHud->scores[].

Usage in .menu:
  itemDef { type 20  feeder 11  rect 0 0 500 300  forecolor 1 1 1 1 }
===========================================================================
*/

#include "../client.h"
#include "cl_wired_hud.h"
#include "cl_wired_hud_private.h"
#include "cl_wired_fonts.h"
#include "../../ui/menudef.h"

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
	float colWeap  = ox + 340 * scale;
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
	CG_ModernDrawString( colWeap + 16 * scale, y, "WP", hdrColor, 8, 12, (int)ow, DS_HCENTER | DS_SHADOW, NULL );
	CG_ModernDrawString( colAcc,        y, "ACC%",  hdrColor, 8, 12, (int)ow, DS_HRIGHT | DS_SHADOW, NULL );
	CG_ModernDrawString( colPing,       y, "PING",   hdrColor, 8, 12, (int)ow, DS_HRIGHT | DS_SHADOW, NULL );

	/* separator below headers */
	{
		vec4_t lineColor = { 0.3f, 0.3f, 0.4f, 0.4f };
		SCR_FillRect( ox + 4, y + 13, ow - 8, 1, lineColor );
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
			SCR_FillRect( ox + 2, y - 1, ow - 4, SB_ROW_H, hlColor );
		}

		/* alternating row shade */
		if ( rank % 2 == 0 ) {
			vec4_t altColor = { 0.1f, 0.1f, 0.15f, 0.2f };
			SCR_FillRect( ox + 2, y - 1, ow - 4, SB_ROW_H, altColor );
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

		/* total damage */
		Com_sprintf( tmp, sizeof( tmp ), "%i", sc->totalDamage );
		CG_ModernDrawString( colDmg, y + 2, tmp, colorWhite,
			8, 12, (int)ow, DS_HRIGHT | DS_SHADOW, NULL );

		/* best weapon icon */
		{
			static int dbgWp = 0;
			if ( dbgWp++ % 600 == 0 )
				Com_Printf( "SB: client=%d bestWp=%d icon=%d dmg=%d\n",
					clientNum, sc->bestWeapon,
					sc->bestWeapon > 0 && sc->bestWeapon < MAX_WEAPONS ? wiredHud->weaponIcons[sc->bestWeapon] : -1,
					sc->totalDamage );
		}
		if ( sc->bestWeapon > 0 && sc->bestWeapon < MAX_WEAPONS
			 && wiredHud->weaponIcons[sc->bestWeapon] ) {
			re.SetColor( colorWhite );
			SCR_DrawPic( colWeap + 8 * scale, y + 1, 14, 14,
				wiredHud->weaponIcons[sc->bestWeapon] );
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
			SCR_FillRect( ox + 4, y, ow - 8, 1, lineColor );
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

#endif /* FEAT_WIRED_UI */
