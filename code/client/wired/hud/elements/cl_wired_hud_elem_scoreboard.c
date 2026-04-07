/*
===========================================================================
cl_wired_hud_elem_scoreboard.c — Scoreboard widget renderers

All layout is relative to the widget rect (ox, oy, ow, oh) received from
the layout engine. Sizes and offsets are fractions of ow (width) or oh
(height). No magic pixel numbers. No 640x480 references.

ITEM_TYPE_SCORELIST  (type 20): FFA/TDM scoreboard with stat columns.
ITEM_TYPE_DUELBOARD  (type 21): two-panel duel scoreboard.
===========================================================================
*/

#include "../../../client.h"
#include "cl_wired_hud.h"
#include "../cl_wired_hud_private.h"
#include "cl_wired_text.h"
#include "cl_wired_draw.h"
#include "../../../../qcommon/menudef.h"

#if FEAT_WIRED_UI

/* ── CG_PlaceString replica (cgame function, not accessible client-side) ── */
static const char *WUI_PlaceString( int rank ) {
	static char str[64];
	const char *s, *t;

	if ( rank & RANK_TIED_FLAG ) {
		rank &= ~RANK_TIED_FLAG;
		t = "Tied for ";
	} else {
		t = "";
	}

	if ( rank == 1 )      s = S_COLOR_BLUE "1st" S_COLOR_WHITE;
	else if ( rank == 2 ) s = S_COLOR_RED "2nd" S_COLOR_WHITE;
	else if ( rank == 3 ) s = S_COLOR_YELLOW "3rd" S_COLOR_WHITE;
	else if ( rank == 11 ) s = "11th";
	else if ( rank == 12 ) s = "12th";
	else if ( rank == 13 ) s = "13th";
	else if ( rank % 10 == 1 ) s = va( "%ist", rank );
	else if ( rank % 10 == 2 ) s = va( "%ind", rank );
	else if ( rank % 10 == 3 ) s = va( "%ird", rank );
	else s = va( "%ith", rank );

	Com_sprintf( str, sizeof( str ), "%s%s", t, s );
	return str;
}

/* ════════════════════════════════════════════════════════════════════════
   SCORELIST WIDGET (type 20)
   ════════════════════════════════════════════════════════════════════════ */

void WiredHud_DrawScorelistWidget( float ox, float oy, float ow, float oh,
	int feederID, const vec4_t textColor )
{
	int i, rank, maxRows;
	float y, rowH, hdrH, pad;
	float fontHdr, fontBody, fontSmall;
	char tmp[128];
	const vec4_t colorWhite = { 1, 1, 1, 1 };
	vec4_t hdrColor;

	int teamFilter = -1;

	if ( !wiredHud || !wiredHud->valid ) return;

	if ( feederID == 0x05 /* red team feeder */ )  teamFilter = 1; /* red */
	if ( feederID == 0x06 /* blue team feeder */ ) teamFilter = 2; /* blue */

	/* ── layout constants — all relative to ow/oh ──────────────── */
	pad      = ow * 0.01f;               /* horizontal padding */
	fontHdr  = oh * 0.025f;              /* header label font */
	fontBody = oh * 0.03f;               /* player row font (reduced) */
	fontSmall= oh * 0.022f;             /* small font (rank, ping) */
	hdrH     = oh * 0.04f;              /* header row total height */
	rowH     = oh * 0.045f;             /* data row height */
	maxRows  = (int)( ( oh - hdrH ) / rowH );
	if ( maxRows < 1 ) maxRows = 1;

	float iconSz  = rowH * 0.8f;        /* head icon size */
	float iconGap = ow * 0.01f;         /* gap after head icon */

	/* column x positions — fractions of ow
	   # | [icon] PLAYER | SCORE | K/D | EFF% | DMG | ATT | ACC% | PING */
	float cRank  = ox + ow * 0.01f;
	float cIcon  = ox + ow * 0.05f;     /* head icon column */
	float cName  = cIcon + iconSz + iconGap;  /* name starts after icon */
	float cScore = ox + ow * 0.38f;
	float cKD    = ox + ow * 0.47f;
	float cEff   = ox + ow * 0.56f;     /* EFF% (new) */
	float cDmg   = ox + ow * 0.64f;
	float cAtt   = ox + ow * 0.72f;
	float cAcc   = ox + ow * 0.81f;
	float cPing  = ox + ow * 0.94f;

	/* ── title area: gametype + placement (drawn by widget, not .wmenu items) ── */
	float fontTitle = oh * 0.03f;     /* title font — smaller than body */
	float fontPlace = oh * 0.02f;     /* placement font */
	float titleH = fontTitle * 1.4f;  /* gametype title line height */
	float placeH = fontPlace * 1.3f;  /* placement line height */
	float sepGap = pad;               /* gap before separator */
	float titleAreaH = titleH + placeH + sepGap + 1;  /* total height above column headers */

	/* ── pre-count visible players for background sizing ───────── */
	{
		int visPlayers = 0, specPlayers = 0;
		for ( i = 0; i < wiredHud->numScores && i < WIRED_HUD_MAX_SCORES; i++ ) {
			wiredHudScore_t *sc = &wiredHud->scores[i];
			int cn = sc->client;
			if ( cn < 0 || cn >= WIRED_HUD_MAX_CLIENTS || !wiredHud->clients[cn].infoValid ) continue;
			if ( sc->team == 3 /* spectator */ ) { specPlayers++; continue; }
			if ( teamFilter >= 0 && sc->team != teamFilter ) continue;
			visPlayers++;
		}
		if ( visPlayers > maxRows ) visPlayers = maxRows;
		{
			/* background covers: title + headers + rows + spectators + bottom padding */
			float bgH = titleAreaH + hdrH + visPlayers * rowH + (rowH * 0.7f);
			if ( specPlayers > 0 ) bgH += rowH * 0.3f + fontSmall * 2.5f;
			vec4_t bgColor = { 0.1f, 0.1f, 0.15f, 0.6f };  /* matches mini scoreboard bar */
			WUI_FillRect( ox, oy, ow, bgH, bgColor );
		}
	}

	/* ── draw title: gametype name ──────────────────────────────── */
	y = oy + pad;
	{
		const char *gtName = wiredHud->gametypeName[0] ? wiredHud->gametypeName : "Deathmatch";
		vec4_t titleColor = { 1.0f, 1.0f, 1.0f, 1.0f };
		Text_Draw( gtName, ox + ow * 0.5f, y, FONT_DISPLAY_BOLD,
			fontTitle, titleColor, TEXT_ALIGN_CENTER, TEXT_DROPSHADOW );
		y += titleH;
	}

	/* ── draw placement: "Nth place with X" ────────────────────── */
	if ( wiredHud->clientNum >= 0 ) {
		const char *placeStr = "";
		int myRank = -1, myScore = 0, si;
		for ( si = 0; si < wiredHud->numScores && si < WIRED_HUD_MAX_SCORES; si++ ) {
			if ( wiredHud->scores[si].team == 3 /* spectator */ ) continue;
			if ( teamFilter >= 0 && wiredHud->scores[si].team != teamFilter ) continue;
			myRank++;
			if ( wiredHud->scores[si].client == wiredHud->clientNum ) {
				myScore = wiredHud->scores[si].score;
				break;
			}
		}
		if ( myRank >= 0 ) {
			placeStr = va( "%s place with %d", WUI_PlaceString( myRank + 1 ), myScore );
		} else if ( wiredHud->demoPlayback ) {
			placeStr = "^3Demo Playback";
		}
		if ( placeStr[0] ) {
			vec4_t placeColor = { 1.0f, 1.0f, 1.0f, 1.0f };
			Text_Draw( placeStr, ox + ow * 0.5f, y, FONT_UI,
				fontPlace, placeColor, TEXT_ALIGN_CENTER, TEXT_DROPSHADOW );
		}
		y += placeH;
	}

	/* ── separator below title ─────────────────────────────────── */
	{
		vec4_t sepColor = { 0.3f, 0.3f, 0.4f, 0.6f };
		WUI_FillRect( ox + pad, y, ow - pad * 2, 1, sepColor );
		y += sepGap;
	}

	/* ── column headers ────────────────────────────────────────── */
	hdrColor[0] = 0.7f; hdrColor[1] = 0.8f; hdrColor[2] = 0.9f; hdrColor[3] = 1.0f;

	Text_Draw( "#",      cRank,  y, FONT_UI, fontHdr, hdrColor, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW );
	Text_Draw( "PLAYER", cName,  y, FONT_UI, fontHdr, hdrColor, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW );
	Text_Draw( "SCORE",  cScore, y, FONT_UI, fontHdr, hdrColor, TEXT_ALIGN_RIGHT, TEXT_DROPSHADOW );
	Text_Draw( "K/D",    cKD,    y, FONT_UI, fontHdr, hdrColor, TEXT_ALIGN_RIGHT, TEXT_DROPSHADOW );
	Text_Draw( "EFF%",   cEff,   y, FONT_UI, fontHdr, hdrColor, TEXT_ALIGN_RIGHT, TEXT_DROPSHADOW );
	Text_Draw( "DMG",    cDmg,   y, FONT_UI, fontHdr, hdrColor, TEXT_ALIGN_RIGHT, TEXT_DROPSHADOW );
	Text_Draw( "ATT",    cAtt,   y, FONT_UI, fontHdr, hdrColor, TEXT_ALIGN_CENTER, TEXT_DROPSHADOW );
	Text_Draw( "ACC%",   cAcc,   y, FONT_UI, fontHdr, hdrColor, TEXT_ALIGN_RIGHT, TEXT_DROPSHADOW );
	Text_Draw( "PING",   cPing,  y, FONT_UI, fontHdr, hdrColor, TEXT_ALIGN_RIGHT, TEXT_DROPSHADOW );

	/* advance y past headers */
	y += hdrH;

	/* separator line below header */
	{
		vec4_t lineColor = { 0.3f, 0.3f, 0.4f, 0.4f };
		WUI_FillRect( ox + pad, y - 1, ow - pad * 2, 1, lineColor );
	}

	/* ── player rows ───────────────────────────────────────────── */
	rank = 0;

	for ( i = 0; i < wiredHud->numScores && i < WIRED_HUD_MAX_SCORES && rank < maxRows; i++ ) {
		wiredHudScore_t *sc = &wiredHud->scores[i];
		int cn = sc->client;
		int acc, pingVal;

		if ( cn < 0 || cn >= WIRED_HUD_MAX_CLIENTS ) continue;
		if ( !wiredHud->clients[cn].infoValid ) continue;
		if ( sc->team == 3 /* spectator */ ) continue;
		if ( teamFilter >= 0 && sc->team != teamFilter ) continue;

		rank++;

		/* highlight current player */
		if ( cn == wiredHud->clientNum ) {
			vec4_t hlColor = { 0.15f, 0.2f, 0.4f, 0.35f };
			WUI_FillRect( ox + pad, y, ow - pad * 2, rowH, hlColor );
		}

		/* alternating row shade */
		if ( rank % 2 == 0 ) {
			vec4_t altColor = { 0.1f, 0.1f, 0.15f, 0.2f };
			WUI_FillRect( ox + pad, y, ow - pad * 2, rowH, altColor );
		}

		/* rank number — right-aligned in rank column */
		Com_sprintf( tmp, sizeof( tmp ), "%i", rank );
		Text_Draw( tmp, cRank + ow * 0.03f, y, FONT_UI,
			fontBody, colorWhite, TEXT_ALIGN_RIGHT, TEXT_DROPSHADOW );

		/* head icon */
		if ( wiredHud->headIcons[cn] ) {
			float iconY = y + ( rowH - iconSz ) * 0.5f;
			re.SetColor( colorWhite );
			WUI_DrawPic( cIcon, iconY, iconSz, iconSz, wiredHud->headIcons[cn] );
			re.SetColor( NULL );
		}

		/* player name — starts at cName (after icon space) */
		Text_SetLetterSpacing( 3.0f );
		Text_Draw( wiredHud->clients[cn].name, cName, y, FONT_DISPLAY,
			fontBody, colorWhite, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW );
		Text_SetLetterSpacing( 0.0f );

		/* connecting */
		if ( sc->ping == -1 ) {
			Text_Draw( "^2...", cScore, y, FONT_UI,
				fontBody, colorWhite, TEXT_ALIGN_RIGHT, TEXT_DROPSHADOW );
			y += rowH;
			continue;
		}

		/* score */
		Com_sprintf( tmp, sizeof( tmp ), "%i", sc->score );
		Text_Draw( tmp, cScore, y, FONT_UI,
			fontBody, colorWhite, TEXT_ALIGN_RIGHT, TEXT_DROPSHADOW );

		/* use pre-computed weapon totals from cgame bridge */
		{
			int totalKills = wiredHud->scoreWeaponTotals[i].totalKills;
			int totalShots = wiredHud->scoreWeaponTotals[i].totalShots;
			int totalHits  = wiredHud->scoreWeaponTotals[i].totalHits;

			/* K/D */
			{
				vec4_t kdColor;
				if ( sc->deaths == 0 ) {
					Vector4Set( kdColor, 0.4f, 1.0f, 0.4f, 1.0f );
				} else if ( totalKills >= sc->deaths ) {
					Vector4Set( kdColor, 0.8f, 1.0f, 0.8f, 1.0f );
				} else {
					Vector4Set( kdColor, 1.0f, 0.6f, 0.6f, 1.0f );
				}
				Com_sprintf( tmp, sizeof( tmp ), "%i/%i", totalKills, sc->deaths );
				Text_Draw( tmp, cKD, y, FONT_UI,
					fontBody, kdColor, TEXT_ALIGN_RIGHT, TEXT_DROPSHADOW );
			}

			/* efficiency */
			{
				int eff = ( totalKills + sc->deaths > 0 )
					? (int)( 100.0f * totalKills / ( totalKills + sc->deaths ) ) : 0;
				vec4_t effColor;
				if ( eff >= 60 )      Vector4Set( effColor, 0.4f, 1.0f, 0.4f, 1.0f );
				else if ( eff >= 40 ) Vector4Set( effColor, 1.0f, 0.9f, 0.4f, 1.0f );
				else                  Vector4Copy( colorWhite, effColor );
				Com_sprintf( tmp, sizeof( tmp ), "%i%%", eff );
				Text_Draw( tmp, cEff, y, FONT_UI,
					fontBody, effColor, TEXT_ALIGN_RIGHT, TEXT_DROPSHADOW );
			}

			/* damage */
			{
				int dmg = sc->damageDone;
				if ( dmg >= 100000 )
					Com_sprintf( tmp, sizeof( tmp ), "%ik", dmg / 1000 );
				else if ( dmg >= 10000 )
					Com_sprintf( tmp, sizeof( tmp ), "%.1fk", dmg / 1000.0f );
				else
					Com_sprintf( tmp, sizeof( tmp ), "%i", dmg );
				Text_Draw( tmp, cDmg, y, FONT_UI,
					fontBody, colorWhite, TEXT_ALIGN_RIGHT, TEXT_DROPSHADOW );
			}

			/* best weapon icon (left of ATT number) + attacks count */
			{
				float attTextX = cAtt;
				if ( sc->bestAttack > ATT_NONE && sc->bestAttack < ATT_NUM_ATTACKS
					 && wiredHud->attackIcons[sc->bestAttack] ) {
					float attIconSz = rowH * 0.7f;
					float attIconX = cAtt - attIconSz - ow * 0.005f;
					re.SetColor( colorWhite );
					WUI_DrawPic( attIconX, y + ( rowH - attIconSz ) * 0.5f,
						attIconSz, attIconSz, wiredHud->attackIcons[sc->bestAttack] );
					re.SetColor( NULL );
				}
				Com_sprintf( tmp, sizeof( tmp ), "%i", totalShots );
				Text_Draw( tmp, attTextX, y, FONT_UI,
					fontBody, colorWhite, TEXT_ALIGN_CENTER, TEXT_DROPSHADOW );
			}

			/* accuracy */
			acc = sc->accuracy;
		{
			vec4_t accColor;
			if ( acc >= 50 )      Vector4Set( accColor, 0.4f, 1.0f, 0.4f, 1.0f );
			else if ( acc >= 30 ) Vector4Set( accColor, 1.0f, 0.9f, 0.4f, 1.0f );
			else                  Vector4Copy( colorWhite, accColor );
			Com_sprintf( tmp, sizeof( tmp ), "%i%%", acc );
			Text_Draw( tmp, cAcc, y, FONT_UI,
				fontBody, accColor, TEXT_ALIGN_RIGHT, TEXT_DROPSHADOW );
		}

		} /* end totals block */

		/* award count badges (between acc and ping columns) */
		{
			vec4_t awardColor = { 1.0f, 0.85f, 0.0f, 1.0f };
			float badgeX = cAcc + ow * 0.02f;
			float badgeFont = fontSmall * 0.8f;
			if ( sc->excellentCount > 0 ) {
				Com_sprintf( tmp, sizeof( tmp ), "^3E:%i", sc->excellentCount );
				Text_Draw( tmp, badgeX, y, FONT_UI,
					badgeFont, awardColor, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW );
				badgeX += ow * 0.05f;
			}
			if ( sc->impressiveCount > 0 ) {
				Com_sprintf( tmp, sizeof( tmp ), "^3I:%i", sc->impressiveCount );
				Text_Draw( tmp, badgeX, y, FONT_UI,
					badgeFont, awardColor, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW );
			}
		}

		/* ping */
		pingVal = sc->ping;
		{
			vec4_t pingColor;
			if ( pingVal < 40 )       Vector4Set( pingColor, 0.4f, 1.0f, 0.4f, 1.0f );
			else if ( pingVal < 100 ) Vector4Set( pingColor, 1.0f, 0.9f, 0.4f, 1.0f );
			else                      Vector4Set( pingColor, 1.0f, 0.4f, 0.4f, 1.0f );
			Com_sprintf( tmp, sizeof( tmp ), "%i", pingVal );
			Text_Draw( tmp, cPing, y, FONT_UI,
				fontBody, pingColor, TEXT_ALIGN_RIGHT, TEXT_DROPSHADOW );
		}

		y += rowH;
	}

	/* ── spectator list ────────────────────────────────────────── */
	{
		float specY = y + rowH * 0.3f;
		float specX = ox + pad;
		int specCount = 0;

		for ( i = 0; i < wiredHud->numScores && i < WIRED_HUD_MAX_SCORES; i++ ) {
			wiredHudScore_t *sc = &wiredHud->scores[i];
			int cn = sc->client;
			if ( cn < 0 || cn >= WIRED_HUD_MAX_CLIENTS ) continue;
			if ( !wiredHud->clients[cn].infoValid ) continue;
			if ( sc->team != 3 /* spectator */ ) continue;
			specCount++;
		}

		if ( specCount > 0 ) {
			vec4_t specHdr = { 0.5f, 0.5f, 0.6f, 0.8f };
			vec4_t specName = { 0.6f, 0.6f, 0.7f, 0.7f };

			Text_Draw( "Spectators", ox + ow * 0.5f, specY, FONT_UI,
				fontSmall, specHdr, TEXT_ALIGN_CENTER, TEXT_DROPSHADOW );

			specY += fontSmall * 1.2f;
			specX = ox + pad;

			for ( i = 0; i < wiredHud->numScores && i < WIRED_HUD_MAX_SCORES; i++ ) {
				wiredHudScore_t *sc = &wiredHud->scores[i];
				int cn = sc->client;
				float nameW;
				if ( cn < 0 || cn >= WIRED_HUD_MAX_CLIENTS ) continue;
				if ( !wiredHud->clients[cn].infoValid ) continue;
				if ( sc->team != 3 /* spectator */ ) continue;

				nameW = Text_Measure( wiredHud->clients[cn].name, FONT_UI, fontSmall );

				if ( specX + nameW > ox + ow - pad ) {
					specX = ox + pad;
					specY += fontSmall * 1.3f;
				}

				Text_Draw( wiredHud->clients[cn].name, specX, specY, FONT_UI,
					fontSmall, specName, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW );

				specX += nameW + ow * 0.02f;
			}
		}
	}
}

/* ════════════════════════════════════════════════════════════════════════
   DUEL BOARD (type 21)
   Two side-by-side fighter panels with stats.
   ════════════════════════════════════════════════════════════════════════ */

static void DrawDuelPanel( float px, float py, float pw, float ph,
	int clientNum, const vec4_t panelColor )
{
	char tmp[128];
	const vec4_t colorWhite = { 1, 1, 1, 1 };
	vec4_t nameColor = { 0.9f, 0.95f, 1.0f, 1.0f };
	vec4_t dimColor = { 0.5f, 0.5f, 0.6f, 0.8f };

	float fontTitle = ph * 0.06f;
	float fontBody  = ph * 0.045f;
	float fontSmall = ph * 0.035f;
	float pad  = pw * 0.03f;
	float rowH = ph * 0.055f;
	float y;

	if ( clientNum < 0 || clientNum >= WIRED_HUD_MAX_CLIENTS ) return;
	if ( !wiredHud->clients[clientNum].infoValid ) return;

	/* panel background */
	{
		vec4_t bg;
		Vector4Copy( panelColor, bg );
		bg[3] = 0.15f;
		WUI_FillRect( px, py, pw, ph, bg );
	}

	/* panel border */
	{
		vec4_t border;
		Vector4Copy( panelColor, border );
		border[3] = 0.4f;
		WUI_FillRect( px, py, pw, 1, border );
		WUI_FillRect( px, py + ph - 1, pw, 1, border );
		WUI_FillRect( px, py, 1, ph, border );
		WUI_FillRect( px + pw - 1, py, 1, ph, border );
	}

	y = py + pad;

	/* head icon + player name */
	{
		float nameX = px + pad;
		float headSz = ph * 0.12f;  /* large and prominent for duel face-off */
		if ( wiredHud->headIcons[clientNum] ) {
			re.SetColor( colorWhite );
			WUI_DrawPic( px + pad, y, headSz, headSz, wiredHud->headIcons[clientNum] );
			re.SetColor( NULL );
			nameX = px + pad + headSz + pw * 0.02f;
		}
		Text_SetLetterSpacing( 3.0f );
		Text_Draw( wiredHud->clients[clientNum].name, nameX, y, FONT_DISPLAY,
			fontTitle, nameColor, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW );
		Text_SetLetterSpacing( 0.0f );
	}
	y += fontTitle * 1.3f;

	/* find this player's score entry */
	{
		int si;
		wiredHudScore_t *sc = NULL;
		for ( si = 0; si < wiredHud->numScores; si++ ) {
			if ( wiredHud->scores[si].client == clientNum ) {
				sc = &wiredHud->scores[si];
				break;
			}
		}

		if ( !sc ) return;

		/* W:L record */
		Com_sprintf( tmp, sizeof( tmp ), "W:L %i:%i", sc->wins, sc->losses );
		Text_Draw( tmp, px + pad, y, FONT_UI,
			fontSmall * 0.85f, dimColor, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW );
		y += fontSmall;

		/* score */
		Com_sprintf( tmp, sizeof( tmp ), "%i", sc->score );
		Text_Draw( "Score", px + pad, y, FONT_UI,
			fontSmall, dimColor, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW );
		Text_Draw( tmp, px + pw - pad, y, FONT_UI,
			fontBody, colorWhite, TEXT_ALIGN_RIGHT, TEXT_DROPSHADOW );
		y += rowH;

		/* K/D — use pre-computed weapon totals */
		{
			int totalKills = wiredHud->scoreWeaponTotals[si].totalKills;
			Com_sprintf( tmp, sizeof( tmp ), "%i / %i", totalKills, sc->deaths );
			Text_Draw( "K / D", px + pad, y, FONT_UI,
				fontSmall, dimColor, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW );
			Text_Draw( tmp, px + pw - pad, y, FONT_UI,
				fontBody, colorWhite, TEXT_ALIGN_RIGHT, TEXT_DROPSHADOW );
			y += rowH;
		}

		/* accuracy */
		Com_sprintf( tmp, sizeof( tmp ), "%i%%", sc->accuracy );
		Text_Draw( "Accuracy", px + pad, y, FONT_UI,
			fontSmall, dimColor, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW );
		Text_Draw( tmp, px + pw - pad, y, FONT_UI,
			fontBody, colorWhite, TEXT_ALIGN_RIGHT, TEXT_DROPSHADOW );
		y += rowH;

		/* damage */
		Com_sprintf( tmp, sizeof( tmp ), "%i", sc->damageDone );
		Text_Draw( "Damage", px + pad, y, FONT_UI,
			fontSmall, dimColor, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW );
		Text_Draw( tmp, px + pw - pad, y, FONT_UI,
			fontBody, colorWhite, TEXT_ALIGN_RIGHT, TEXT_DROPSHADOW );
		y += rowH;

		/* ping */
		Com_sprintf( tmp, sizeof( tmp ), "%i ms", sc->ping );
		Text_Draw( "Ping", px + pad, y, FONT_UI,
			fontSmall, dimColor, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW );
		Text_Draw( tmp, px + pw - pad, y, FONT_UI,
			fontBody, colorWhite, TEXT_ALIGN_RIGHT, TEXT_DROPSHADOW );
		y += rowH;

		/* per-weapon stats */
		{
			int w, hasWeapons = 0;
			float wpY;
			int wpSlots = (int)(sizeof(sc->weaponStats) / sizeof(sc->weaponStats[0]));
			for ( w = 0; w < wpSlots; w++ ) {
				if ( sc->weaponStats[w].shots > 0 || sc->weaponStats[w].kills > 0 )
					hasWeapons = 1;
			}

			if ( hasWeapons ) {
				int totalHits = 0, totalShots = 0, totalWpKills = 0, totalWpDeaths = 0;

				/* separator */
				{
					vec4_t lineColor = { 0.3f, 0.3f, 0.4f, 0.3f };
					WUI_FillRect( px + pad, y, pw - pad * 2, 1, lineColor );
				}
				y += rowH * 0.3f;

				/* weapon column headers */
				Text_Draw( "Weapon", px + pad, y, FONT_UI,
					fontSmall * 0.85f, dimColor, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW );
				Text_Draw( "Hits/Att", px + pw * 0.55f, y, FONT_UI,
					fontSmall * 0.85f, dimColor, TEXT_ALIGN_RIGHT, TEXT_DROPSHADOW );
				Text_Draw( "Acc%", px + pw - pad, y, FONT_UI,
					fontSmall * 0.85f, dimColor, TEXT_ALIGN_RIGHT, TEXT_DROPSHADOW );

				y += fontSmall;
				wpY = y;

				for ( w = 0; w < wpSlots; w++ ) {
					wiredWeaponStats_t *ws = &sc->weaponStats[w];
					int wpAcc;
					vec4_t wpColor;
					float wpNameX;

					if ( ws->shots <= 0 && ws->kills <= 0 ) continue;

					totalHits += ws->hits;
					totalShots += ws->shots;
					totalWpKills += ws->kills;
					totalWpDeaths += ws->deaths;

					/* weapon icon + name */
					wpNameX = px + pad;
					if ( wiredHud->weaponIcons[w] ) {
						float wpIconSz = fontSmall * 0.9f;
						re.SetColor( colorWhite );
						WUI_DrawPic( wpNameX, wpY, wpIconSz, wpIconSz, wiredHud->weaponIcons[w] );
						re.SetColor( NULL );
						wpNameX += wpIconSz + pw * 0.01f;
					}
					{
						static const char *wpNames[] = { "", "G", "MG", "SG", "GL", "RL", "LG", "RG", "PG", "BFG", "", "NG", "PL", "CG", "HMG", "" };
						int numNames = (int)(sizeof(wpNames) / sizeof(wpNames[0]));
						const char *wpName = ( w < numNames && wpNames[w][0] ) ? wpNames[w] : va( "w%i", w );
						Text_Draw( wpName, wpNameX, wpY, FONT_UI,
							fontSmall * 0.85f, colorWhite, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW );
					}

					/* hits/shots */
					Com_sprintf( tmp, sizeof( tmp ), "%i/%i", ws->hits, ws->shots );
					Text_Draw( tmp, px + pw * 0.55f, wpY, FONT_UI,
						fontSmall * 0.85f, colorWhite, TEXT_ALIGN_RIGHT, TEXT_DROPSHADOW );

					/* accuracy */
					wpAcc = ( ws->shots > 0 ) ? (int)( 100.0f * ws->hits / ws->shots ) : 0;
					if ( wpAcc >= 50 )      Vector4Set( wpColor, 0.4f, 1.0f, 0.4f, 1.0f );
					else if ( wpAcc >= 30 ) Vector4Set( wpColor, 1.0f, 0.9f, 0.4f, 1.0f );
					else                    Vector4Copy( colorWhite, wpColor );
					Com_sprintf( tmp, sizeof( tmp ), "%i%%", wpAcc );
					Text_Draw( tmp, px + pw - pad, wpY, FONT_UI,
						fontSmall * 0.85f, wpColor, TEXT_ALIGN_RIGHT, TEXT_DROPSHADOW );

					wpY += fontSmall;
				}

				/* TOTAL summary row */
				{
					vec4_t lineColor2 = { 0.3f, 0.3f, 0.4f, 0.3f };
					vec4_t sumColor = { 0.9f, 0.9f, 1.0f, 1.0f };
					int totAcc;

					WUI_FillRect( px + pad, wpY, pw - pad * 2, 1, lineColor2 );
					wpY += fontSmall * 0.3f;

					Text_Draw( "TOTAL", px + pad, wpY, FONT_UI,
						fontSmall * 0.85f, sumColor, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW );

					Com_sprintf( tmp, sizeof( tmp ), "%i/%i", totalHits, totalShots );
					Text_Draw( tmp, px + pw * 0.55f, wpY, FONT_UI,
						fontSmall * 0.85f, sumColor, TEXT_ALIGN_RIGHT, TEXT_DROPSHADOW );

					totAcc = ( totalShots > 0 ) ? (int)( 100.0f * totalHits / totalShots ) : 0;
					Com_sprintf( tmp, sizeof( tmp ), "%i%%", totAcc );
					Text_Draw( tmp, px + pw - pad, wpY, FONT_UI,
						fontSmall * 0.85f, sumColor, TEXT_ALIGN_RIGHT, TEXT_DROPSHADOW );
				}
			}
		}
	}
}

void WiredHud_DrawDuelBoard( float ox, float oy, float ow, float oh )
{
	static int s_intermissionStartTime = 0;

	float panelW, panelH, gap, panelY;
	float fontTitle, fontBody;
	vec4_t redPanel  = { 0.8f, 0.2f, 0.2f, 1.0f };
	vec4_t bluePanel = { 0.2f, 0.2f, 0.8f, 1.0f };
	const vec4_t colorWhite = { 1, 1, 1, 1 };
	vec4_t vsColor = { 0.6f, 0.6f, 0.7f, 0.8f };
	int p1 = -1, p2 = -1;
	int p1score = 0, p2score = 0;
	int i;
	char tmp[128];
	float panelAlpha = 1.0f;
	float centerX;

	if ( !wiredHud || !wiredHud->valid ) return;

	/* intermission timing */
	if ( wiredHud->intermission ) {
		if ( !s_intermissionStartTime ) s_intermissionStartTime = wiredHud->realtime;
	} else {
		s_intermissionStartTime = 0;
	}

	/* find the two duelists (first two non-spectator scores) */
	for ( i = 0; i < wiredHud->numScores && i < WIRED_HUD_MAX_SCORES; i++ ) {
		if ( wiredHud->scores[i].team == 3 /* spectator */ ) continue;
		if ( p1 < 0 ) {
			p1 = wiredHud->scores[i].client;
			p1score = wiredHud->scores[i].score;
			continue;
		}
		if ( p2 < 0 ) {
			p2 = wiredHud->scores[i].client;
			p2score = wiredHud->scores[i].score;
			break;
		}
	}

	fontTitle = oh * 0.04f;
	fontBody  = oh * 0.03f;
	gap       = ow * 0.03f;
	panelW    = ( ow - gap * 3 ) * 0.5f;
	panelH    = oh * 0.85f;
	panelY    = oy + oh * 0.08f;
	centerX   = ox + ow * 0.5f;

	/* ── intermission: animated winner banner + match duration ── */
	if ( wiredHud->intermission && s_intermissionStartTime ) {
		vec4_t goldColor  = { 1.0f, 0.85f, 0.0f, 1.0f };
		vec4_t titleColor = { 0.9f, 0.9f, 1.0f, 1.0f };
		vec4_t durColor   = { 0.7f, 0.8f, 0.9f, 1.0f };
		int elapsed = wiredHud->realtime - s_intermissionStartTime;
		float titleAlpha, winnerAlpha;
		int winner; /* 0=p1 wins, 1=p2 wins, -1=draw */

		if ( elapsed < 0 ) elapsed = 0;

		/* Phase 1 (0-500ms): "MATCH OVER" fades in */
		titleAlpha = ( elapsed < 500 ) ? (float)elapsed / 500.0f : 1.0f;

		/* Phase 2 (500-1200ms): "WINNER"/"DRAW" fades in */
		if ( elapsed < 500 )
			winnerAlpha = 0.0f;
		else if ( elapsed < 1200 )
			winnerAlpha = (float)( elapsed - 500 ) / 700.0f;
		else
			winnerAlpha = 1.0f;

		/* Panel fade-in: starts at 300ms, full at 900ms */
		if ( elapsed < 300 )
			panelAlpha = 0.0f;
		else if ( elapsed < 900 )
			panelAlpha = (float)( elapsed - 300 ) / 600.0f;
		else
			panelAlpha = 1.0f;

		titleColor[3] = titleAlpha;
		durColor[3] *= titleAlpha;

		/* determine winner */
		if ( p1 >= 0 && p2 >= 0 ) {
			if ( p1score > p2score ) winner = 0;
			else if ( p2score > p1score ) winner = 1;
			else winner = -1;
		} else if ( p1 >= 0 ) {
			winner = 0;
		} else if ( p2 >= 0 ) {
			winner = 1;
		} else {
			winner = -1;
		}

		/* "MATCH OVER" title */
		Text_Draw( "MATCH OVER", centerX, oy, FONT_UI,
			fontTitle * 1.1f, titleColor, TEXT_ALIGN_CENTER, TEXT_DROPSHADOW );

		/* winner banner or draw */
		goldColor[3] = winnerAlpha;
		if ( winner == 0 ) {
			Text_Draw( "WINNER", ox + gap + panelW * 0.5f, oy + oh * 0.04f, FONT_UI,
				fontTitle * 1.3f, goldColor, TEXT_ALIGN_CENTER, TEXT_DROPSHADOW );
		} else if ( winner == 1 ) {
			Text_Draw( "WINNER", ox + gap * 2 + panelW + panelW * 0.5f, oy + oh * 0.04f, FONT_UI,
				fontTitle * 1.3f, goldColor, TEXT_ALIGN_CENTER, TEXT_DROPSHADOW );
		} else {
			Text_Draw( "DRAW", centerX, oy + oh * 0.04f, FONT_UI,
				fontTitle * 1.3f, goldColor, TEXT_ALIGN_CENTER, TEXT_DROPSHADOW );
		}

		/* match duration at bottom */
		if ( wiredHud->levelStartTime > 0 ) {
			int matchElapsed = ( wiredHud->time - wiredHud->levelStartTime ) / 1000;
			int mins, secs;
			if ( matchElapsed < 0 ) matchElapsed = 0;
			mins = matchElapsed / 60;
			secs = matchElapsed % 60;
			Com_sprintf( tmp, sizeof( tmp ), "Match Duration: %i:%02i", mins, secs );
			Text_Draw( tmp, centerX, oy + oh * 0.93f, FONT_UI,
				fontBody, durColor, TEXT_ALIGN_CENTER, TEXT_DROPSHADOW );
		}

		/* shift panels down to make room for banner */
		panelY = oy + oh * 0.10f;
		panelH = oh * 0.80f;
	} else {
		/* normal (non-intermission) title + vs */
		Text_Draw( "DUEL", centerX, oy, FONT_DISPLAY_BOLD,
			fontTitle, colorWhite, TEXT_ALIGN_CENTER, TEXT_DROPSHADOW );

		Text_Draw( "vs", centerX, panelY + panelH * 0.15f, FONT_UI,
			fontBody, vsColor, TEXT_ALIGN_CENTER, TEXT_DROPSHADOW );
	}

	/* ── apply panel alpha to panel colors ─────────────────────── */
	redPanel[3]  *= panelAlpha;
	bluePanel[3] *= panelAlpha;

	/* left panel (player 1) */
	if ( p1 >= 0 ) {
		DrawDuelPanel( ox + gap, panelY, panelW, panelH, p1, redPanel );
	} else {
		vec4_t emptyBg = { 0.1f, 0.1f, 0.15f, 0.3f };
		emptyBg[3] *= panelAlpha;
		WUI_FillRect( ox + gap, panelY, panelW, panelH, emptyBg );
		Text_Draw( "Waiting...", ox + gap + panelW * 0.5f, panelY + panelH * 0.45f, FONT_UI,
			fontBody, vsColor, TEXT_ALIGN_CENTER, TEXT_DROPSHADOW );
	}

	/* right panel (player 2) */
	if ( p2 >= 0 ) {
		DrawDuelPanel( ox + gap * 2 + panelW, panelY, panelW, panelH, p2, bluePanel );
	} else {
		vec4_t emptyBg = { 0.1f, 0.1f, 0.15f, 0.3f };
		emptyBg[3] *= panelAlpha;
		WUI_FillRect( ox + gap * 2 + panelW, panelY, panelW, panelH, emptyBg );
		Text_Draw( "Waiting...", ox + gap * 2 + panelW + panelW * 0.5f, panelY + panelH * 0.45f, FONT_UI,
			fontBody, vsColor, TEXT_ALIGN_CENTER, TEXT_DROPSHADOW );
	}

	/* spectator list at bottom */
	{
		float specY = oy + oh * 0.95f;
		float specX = ox + gap;
		int specCount = 0;

		for ( i = 0; i < wiredHud->numScores && i < WIRED_HUD_MAX_SCORES; i++ ) {
			if ( wiredHud->scores[i].team == 3 /* spectator */ ) specCount++;
		}

		if ( specCount > 0 ) {
			vec4_t specHdr = { 0.5f, 0.5f, 0.6f, 0.8f };
			vec4_t specName = { 0.6f, 0.6f, 0.7f, 0.7f };
			(void)specX;
			(void)specName;
			Text_Draw( "Spectators", centerX, specY, FONT_UI,
				fontBody * 0.8f, specHdr, TEXT_ALIGN_CENTER, TEXT_DROPSHADOW );
		}
	}
}

#endif /* FEAT_WIRED_UI */
