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
//
// cg_scoreboard -- draw the scoreboard on top of the game screen
#include "cg_local.h"
#include "cg_modern_private.h"

vec4_t scoreboard_rtColor = {1, 0, 0, 1};
vec4_t scoreboard_btColor = {0, 0, 1, 1};

// Scoreboard color flags (bitflags: 0=none, 1=body/header, 2=title, 3=both)
int customScoreboardColorIsSet_red;
int customScoreboardColorIsSet_blue;
int customScoreboardColorIsSet_spec;

vec4_t scoreboard_rtColorBody = {1, 0, 0, 1};
vec4_t scoreboard_btColorBody = {0, 0, 1, 1};
vec4_t scoreboard_rtColorHeader = {1, 0, 0, 1};
vec4_t scoreboard_btColorHeader = {0, 0, 1, 1};
vec4_t scoreboard_rtColorTitle = {1, 0, 0, 1};
vec4_t scoreboard_btColorTitle = {0, 0, 1, 1};
vec4_t scoreboard_specColorTitle = {0.66f, 0.66f, 0.66f, 1};
vec4_t scoreboard_specColor = {0.66f, 0.66f, 0.66f, 1};

// Local default border size for powerup frame
static const vec4_t defaultBorderSize = {2, 2, 2, 2};

#define SCOREBOARD_X        (0)

#define SB_HEADER           86
#define SB_TOP              (SB_HEADER+32)

// Where the status bar starts, so we don't overwrite it
#define SB_STATUSBAR        420

#define SB_NORMAL_HEIGHT    40
#define SB_INTER_HEIGHT     16 // interleaved height

#define SB_MAXCLIENTS_NORMAL  ((SB_STATUSBAR - SB_TOP) / SB_NORMAL_HEIGHT)
#define SB_MAXCLIENTS_INTER   ((SB_STATUSBAR - SB_TOP) / SB_INTER_HEIGHT - 1)

// Used when interleaved

#define SB_LEFT_BOTICON_X   (SCOREBOARD_X+0)
#define SB_LEFT_HEAD_X      (SCOREBOARD_X+32)
#define SB_RIGHT_BOTICON_X  (SCOREBOARD_X+64)
#define SB_RIGHT_HEAD_X     (SCOREBOARD_X+96)
// Normal
#define SB_BOTICON_X        (SCOREBOARD_X+32)
#define SB_HEAD_X           (SCOREBOARD_X+64)

#define SB_SCORELINE_X      80

#define SB_RATING_WIDTH     (6 * BIGCHAR_WIDTH) // width 6
#define SB_SCORE_X          (SB_SCORELINE_X + BIGCHAR_WIDTH) // width 6
#define SB_RATING_X         (SB_SCORELINE_X + 6 * BIGCHAR_WIDTH) // width 6
#define SB_PING_X           (SB_SCORELINE_X + 12 * BIGCHAR_WIDTH + 8) // width 5
#define SB_TIME_X           (SB_SCORELINE_X + 17 * BIGCHAR_WIDTH + 8) // width 5
#define SB_NAME_X           (SB_SCORELINE_X + 22 * BIGCHAR_WIDTH) // width 15

// The new and improved score board
//
// In cases where the number of clients is high, the score board heads are interleaved
// here's the layout

//
//	0   32   80  112  144   240  320  400   <-- pixel position
//  bot head bot head score ping time name
//
//  wins/losses are drawn on bot icon now

qboolean localClient = 0; // true if local client has been displayed

/* forward declarations */
int CG_ModernDrawTeamScores(int x, int y, int team, float fade, int maxScores);


int sumScoresBlue;
int sumScoresRed;
int sumPingBlue;
int sumPingRed;
int sumThawsBlue;
int sumThawsRed;

// Simple inline replacement for CG_ModernAdjustTeamColor
// Darkens color and sets alpha for scoreboard backgrounds
static void CG_ScoreboardAdjustTeamColor(const vec4_t src, vec4_t dst) {
	dst[0] = src[0] * 0.5f;
	dst[1] = src[1] * 0.5f;
	dst[2] = src[2] * 0.5f;
	dst[3] = 0.33f;
}

// Simple inline replacement for CG_ModernDrawField
// Draws a large number (team score) at given position
static void CG_ScoreboardDrawField(int x, int y, int value) {
	char num[16];
	Com_sprintf(num, sizeof(num), "%d", value);
	trap_R_DrawTextNorm(num, (x + 8) * NORM_HSCALE, (y + 8) * NORM_VSCALE, FONT_DISPLAY, (40) * NORM_VSCALE, colorWhite, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW);
}


/*
=================
CG_DrawScoreboard
=================
*/
static void CG_DrawClientScore(int y, score_t *score, float *color, float fade, qboolean largeFormat)
{
	char    string[1024];
	vec3_t  headAngles;
	clientInfo_t    *ci;
	int iconx, headx;

	if (score->client < 0 || score->client >= cgs.maxclients)
	{
		Com_Printf("Bad score->client: %i\n", score->client);
		return;
	}

	ci = &cgs.clientinfo[score->client];

	iconx = SB_BOTICON_X + BIGCHAR_WIDTH; //48
	headx = SB_HEAD_X + BIGCHAR_WIDTH; // 80

	// draw the handicap or bot skill marker (unless player has flag)
	if (ci->powerups & (1 << PW_NEUTRALFLAG))
	{
		if (largeFormat)
		{
			CG_DrawFlagModel(iconx, y - (32 - BIGCHAR_HEIGHT) / 2, 32, 32, TEAM_FREE, qfalse);
		}
		else
		{
			CG_DrawFlagModel(iconx, y, 16, 16, TEAM_FREE, qfalse);
		}
	}
	else if (ci->powerups & (1 << PW_REDFLAG))
	{
		if (largeFormat)
		{
			CG_DrawFlagModel(iconx, y - (32 - BIGCHAR_HEIGHT) / 2, 32, 32, TEAM_RED, qfalse);
		}
		else
		{
			CG_DrawFlagModel(iconx, y, 16, 16, TEAM_RED, qfalse);
		}
	}
	else if (ci->powerups & (1 << PW_BLUEFLAG))
	{
		if (largeFormat)
		{
			CG_DrawFlagModel(iconx, y - (32 - BIGCHAR_HEIGHT) / 2, 32, 32, TEAM_BLUE, qfalse);
		}
		else
		{
			CG_DrawFlagModel(iconx, y, 16, 16, TEAM_BLUE, qfalse);
		}
	}
	else
	{
		if (ci->botSkill > 0 && ci->botSkill <= 5)
		{
			if (cg_drawIcons.integer)
			{
				if (largeFormat)
				{
					CG_DrawPicNorm((iconx) * NORM_HSCALE, (y - (32 - BIGCHAR_HEIGHT) / 2) * NORM_VSCALE, (32) * NORM_HSCALE, (32) * NORM_VSCALE, cgs.media.botSkillShaders[ ci->botSkill - 1 ]);
				}
				else
				{
					CG_DrawPicNorm((iconx) * NORM_HSCALE, (y) * NORM_VSCALE, (16) * NORM_HSCALE, (16) * NORM_VSCALE, cgs.media.botSkillShaders[ ci->botSkill - 1 ]);
				}
			}
		}

		// draw the wins / losses
		if (cgs.gametype == GT_DUEL)
		{
			int score_x = iconx;

			Com_sprintf(string, sizeof(string), "%i/%i", ci->wins, ci->losses);
			trap_R_DrawTextNorm(string, (score_x) * NORM_HSCALE, (y) * NORM_VSCALE, FONT_UI, (12) * NORM_VSCALE, color, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW);
		}

	}

	// draw the face
	VectorClear(headAngles);

	headAngles[YAW] = 180;

	if (score->client == cg.snap->ps.clientNum)
	{
		headAngles[YAW] = (float)cg.time / 14 + 180;
	}
	else
	{
		headAngles[YAW] = 180;
	}

	if (largeFormat)
	{
		CG_DrawHead(headx, y - (float)(ICON_SIZE - BIGCHAR_HEIGHT) / 2, ICON_SIZE, ICON_SIZE, score->client, headAngles);
	}
	else
	{
		CG_DrawHead(headx, y, 16, 16, score->client, headAngles);
	}
	// draw the score line
	if (score->ping == -1)
	{
		Com_sprintf(string, sizeof(string), "^2 connecting^7    ");
	}
	else if (ci->team == TEAM_SPECTATOR)
	{
		Com_sprintf(string, sizeof(string), "^5 SPECT^7 %3i %4i", score->ping, score->time);
	}
	else
	{
		Com_sprintf(string, sizeof(string), "^7%5i %4i %4i", score->score, score->ping, score->time);
	}

	// highlight your position
	if (score->client == cg.snap->ps.clientNum)
	{
		float   hcolor[4];
		int     rank;

		localClient = qtrue;

		if (cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR
		        || cgs.gametypeIsTeamGame)
		{
			rank = -1;
		}
		else
		{
			rank = cg.snap->ps.persistant[PERS_RANK] & ~RANK_TIED_FLAG;
		}
		if (rank == 0)
		{
			hcolor[0] = 0;
			hcolor[1] = 0;
			hcolor[2] = 0.7f;
		}
		else if (rank == 1)
		{
			hcolor[0] = 0.7f;
			hcolor[1] = 0;
			hcolor[2] = 0;
		}
		else if (rank == 2)
		{
			hcolor[0] = 0.7f;
			hcolor[1] = 0.7f;
			hcolor[2] = 0;
		}
		else
		{
			hcolor[0] = 0.7f;
			hcolor[1] = 0.7f;
			hcolor[2] = 0.7f;
		}

		hcolor[3] = fade * 0.7;
		CG_FillRectNorm((SB_SCORELINE_X + BIGCHAR_WIDTH + (SB_RATING_WIDTH / 2.0)) * NORM_HSCALE, y * NORM_VSCALE,
		            (640 - SB_SCORELINE_X - BIGCHAR_WIDTH) * NORM_HSCALE, (BIGCHAR_HEIGHT + 1) * NORM_VSCALE, hcolor);
	}

	trap_R_DrawTextNorm(string, (128) * NORM_HSCALE, (y) * NORM_VSCALE, FONT_DISPLAY, (16) * NORM_VSCALE, color, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW);

	trap_R_DrawTextNorm(ci->name, (128 + 16 * 16) * NORM_HSCALE, (y) * NORM_VSCALE, FONT_UI, (16) * NORM_VSCALE, color, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW);

	// add the "ready" marker for intermission exiting
	if (cg.warmup == 0 && cg.predictedPlayerState.pm_type != PM_INTERMISSION)
	{
		return;
	}
	// STAT_CLIENTS_READY not available yet in q3now
	(void)0;
}

/*
=================
CG_BEDrawClientScore
=================
*/
static void CG_BEDrawClientScore(int y, score_t *score, float *color, float fade, qboolean largeFormat)
{
	char string[1024], string2[1024], string3[1024];
	vec3_t headAngles;
	clientInfo_t *ci;
	int iconx, headx;
	float bWidth = 14;
	float bHeight = 16;

	if (score->client < 0 || score->client >= cgs.maxclients)
	{
		Com_Printf("Bad score->client: %i\n", score->client);
		return;
	}
	ci = &cgs.clientinfo[score->client];

	iconx = SB_BOTICON_X + BIGCHAR_WIDTH;
	headx = SB_HEAD_X + BIGCHAR_WIDTH;

	if (ci->powerups & (1 << PW_NEUTRALFLAG) ||
	        ci->powerups & (1 << PW_REDFLAG) ||
	        ci->powerups & (1 << PW_BLUEFLAG))
	{
		int team = TEAM_FREE;
		if (ci->powerups & (1 << PW_REDFLAG))
		{
			team = TEAM_RED;
		}
		else if (ci->powerups & (1 << PW_BLUEFLAG))
		{
			team = TEAM_BLUE;
		}
		if (largeFormat)
		{
			CG_DrawFlagModel(iconx, y - (32 - bHeight) / 2, 32, 32, team, qfalse);
		}
		else
		{
			CG_DrawFlagModel(iconx, y, 16, 16, team, qfalse);
		}
	}
	else
	{
		if (ci->botSkill > 0 && ci->botSkill <= 5)
		{
			if (cg_drawIcons.integer)
			{
				if (largeFormat)
				{
					CG_DrawPicNorm((iconx) * NORM_HSCALE, (y - (32 - bHeight) / 2) * NORM_VSCALE, (32) * NORM_HSCALE, (32) * NORM_VSCALE, cgs.media.botSkillShaders[ci->botSkill - 1]);
				}
				else
				{
					CG_DrawPicNorm((iconx) * NORM_HSCALE, (y) * NORM_VSCALE, (16) * NORM_HSCALE, (16) * NORM_VSCALE, cgs.media.botSkillShaders[ci->botSkill - 1]);
				}
			}
		}

		if (cgs.gametype == GT_DUEL)
		{
			int score_x = iconx;

			Com_sprintf(string, sizeof(string), "%i/%i", ci->wins, ci->losses);
			trap_R_DrawTextNorm(string, (score_x) * NORM_HSCALE, (y) * NORM_VSCALE, FONT_DISPLAY, (bHeight) * NORM_VSCALE, color, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW);
		}
	}

	VectorClear(headAngles);
	headAngles[YAW] = (score->client == cg.snap->ps.clientNum)
	                  ? ((float)cg.time / 14 + 180) : 180;

	if (largeFormat)
		CG_DrawHead(headx, y - (float)(ICON_SIZE - BIGCHAR_HEIGHT) / 2, ICON_SIZE, ICON_SIZE, score->client, headAngles);
	else
		CG_DrawHead(headx, y, 16, 16, score->client, headAngles);

	if (score->ping == -1)
	{
		Com_sprintf(string, sizeof(string), "^2 connecting^7    ");
	}
	else if (ci->team == TEAM_SPECTATOR)
	{
		Com_sprintf(string, sizeof(string), "^5 SPECT^7");
		Com_sprintf(string2, sizeof(string2), "%i", score->ping);
		Com_sprintf(string3, sizeof(string3), "%i", score->time);
	}
	else
	{
		Com_sprintf(string, sizeof(string), "^7%i", score->score);
		Com_sprintf(string2, sizeof(string2), "%i", score->ping);
		Com_sprintf(string3, sizeof(string3), "%i", score->time);
	}


	if (score->client == cg.snap->ps.clientNum)
	{
		float hcolor[4];
		int rank;

		localClient = qtrue;
		if (cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR || cgs.gametypeIsTeamGame)
			rank = -1;
		else
			rank = cg.snap->ps.persistant[PERS_RANK] & ~RANK_TIED_FLAG;

		switch (rank)
		{
			case 0:
				hcolor[0] = 0;
				hcolor[1] = 0;
				hcolor[2] = 0.7f;
				break;
			case 1:
				hcolor[0] = 0.7f;
				hcolor[1] = 0;
				hcolor[2] = 0;
				break;
			case 2:
				hcolor[0] = 0.7f;
				hcolor[1] = 0.7f;
				hcolor[2] = 0;
				break;
			default:
				hcolor[0] = 0.7f;
				hcolor[1] = 0.7f;
				hcolor[2] = 0.7f;
				break;
		}
		hcolor[3] = fade * 0.7;
		CG_FillRectNorm((SB_SCORELINE_X + BIGCHAR_WIDTH + (SB_RATING_WIDTH / 2.0)) * NORM_HSCALE, y * NORM_VSCALE,
		            (640 - SB_SCORELINE_X - bWidth) * NORM_HSCALE, (bHeight + 1) * NORM_VSCALE, hcolor);
	}

	trap_R_DrawTextNorm(string, (206) * NORM_HSCALE, (y) * NORM_VSCALE, FONT_DISPLAY, (bHeight) * NORM_VSCALE, color, TEXT_ALIGN_RIGHT, TEXT_DROPSHADOW);
	trap_R_DrawTextNorm(string2, (286) * NORM_HSCALE, (y) * NORM_VSCALE, FONT_DISPLAY, (bHeight) * NORM_VSCALE, color, TEXT_ALIGN_RIGHT, TEXT_DROPSHADOW);
	trap_R_DrawTextNorm(string3, (366) * NORM_HSCALE, (y) * NORM_VSCALE, FONT_DISPLAY, (bHeight) * NORM_VSCALE, color, TEXT_ALIGN_RIGHT, TEXT_DROPSHADOW);

	trap_R_DrawTextNorm(ci->name, (128 + 4 + 16 * 16) * NORM_HSCALE, (y) * NORM_VSCALE, FONT_DISPLAY, (bHeight) * NORM_VSCALE, color, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW);


	if (cg.warmup != 0 || cg.predictedPlayerState.pm_type == PM_INTERMISSION)
	{
		// STAT_CLIENTS_READY not available yet in q3now
		(void)0;
	}
}


static void CG_DrawClientScoreNew(int y, score_t *score, float *color, float fade, qboolean largeFormat)
{
	// cg_scoreboardBE.integer is 0, always use standard path
	CG_DrawClientScore(y, score, color, fade, largeFormat);
}

/*
=================
CG_TeamScoreboard
isn't for team. Uses only for single player modes
=================
*/
int CG_TeamScoreboard(int y, team_t team, float fade, int maxClients, int lineHeight)
{
	int     i;
	score_t *score;
	vec4_t   color1;
	int     count;
	clientInfo_t    *ci;

	color1[0] = color1[1] = color1[2] = 1.0;
	color1[3] = fade;

	count = 0;
	for (i = 0 ; i < cg.numScores && count < maxClients ; i++)
	{
		score = &cg.scores[i];
		ci = &cgs.clientinfo[score->client];
		if (team == ci->team)
		{
			CG_DrawClientScoreNew(y + lineHeight * count, score, color1, fade, lineHeight == 40);
			++count;
		}
	}
	return count;
}

/*
=================
CG_DrawScoreboard

Draw the normal in-game scoreboard
=================
*/
qboolean CG_DrawOldScoreboard(void)
{
	int     x, y, w, i, n1, n2;
	float   fade;
	float   *fadeColor;
	const char *s;
	int maxClients;
	int lineHeight;
	int topBorderSize, bottomBorderSize;

	// don't draw amuthing if the menu or console is up
	if (cg_paused.integer)
	{
		return qfalse;
	}

	if (cg.warmup && !cg.showScores && cg.predictedPlayerState.pm_type != PM_INTERMISSION)
	{
		return qfalse;
	}

	if (cg.showScores || cg.predictedPlayerState.pm_type == PM_DEAD ||
	        cg.predictedPlayerState.pm_type == PM_INTERMISSION)
	{
		fade = 1.0f; // cg_scoreTransparency.value
		fadeColor = colorWhite;
	}
	else
	{
		fadeColor = CG_FadeColor(cg.scoreFadeTime, FADE_TIME);

		if (!fadeColor)
		{
			// next time scoreboard comes up, don't print killer
			cg.killerName[0] = 0;
			return qfalse;
		}
		fade = *fadeColor;
	}

	if (cg.demoPlayback != 0)
	{
		trap_R_DrawTextNorm("^3Demo Playback", (640 / 2) * NORM_HSCALE, (40) * NORM_VSCALE, FONT_DISPLAY, (16) * NORM_VSCALE, CG_ColorFromAlpha(fade), TEXT_ALIGN_CENTER, TEXT_DROPSHADOW);
	}
	// fragged by ... line
	else if (cg.killerName[0])
	{
		s = va("Fragged by %s", cg.killerName);
		trap_R_DrawTextNorm(s, (640 / 2) * NORM_HSCALE, (40) * NORM_VSCALE, FONT_DISPLAY, (16) * NORM_VSCALE, CG_ColorFromAlpha(fade), TEXT_ALIGN_CENTER, TEXT_DROPSHADOW);
	}

	// current rank
	if (!cgs.gametypeIsTeamGame)
	{
		if (cg.snap->ps.persistant[PERS_TEAM] != TEAM_SPECTATOR)
		{
			s = va("%s place with %i",
			       CG_PlaceString(cg.snap->ps.persistant[PERS_RANK] + 1),
			       cg.snap->ps.persistant[PERS_SCORE]);
			trap_R_DrawTextNorm(s, (640 / 2) * NORM_HSCALE, (60) * NORM_VSCALE, FONT_DISPLAY, (16) * NORM_VSCALE, CG_ColorFromAlpha(fade), TEXT_ALIGN_CENTER, TEXT_DROPSHADOW);
		}
	}
	else
	{
		if (cg.teamScores[0] == cg.teamScores[1])
		{
			s = va("Teams are tied at %i", cg.teamScores[0]);
		}
		else if (cg.teamScores[0] >= cg.teamScores[1])
		{
			s = va("Red leads %i to %i", cg.teamScores[0], cg.teamScores[1]);
		}
		else
		{
			s = va("Blue leads %i to %i", cg.teamScores[1], cg.teamScores[0]);
		}

		trap_R_DrawTextNorm(s, (640 / 2) * NORM_HSCALE, (60) * NORM_VSCALE, FONT_DISPLAY, (16) * NORM_VSCALE, CG_ColorFromAlpha(fade), TEXT_ALIGN_CENTER, TEXT_DROPSHADOW);
	}

	// scoreboard
	y = SB_HEADER;

	CG_DrawPicNorm((SB_SCORE_X + (SB_RATING_WIDTH / 2.0)) * NORM_HSCALE, (y) * NORM_VSCALE, (64) * NORM_HSCALE, (32) * NORM_VSCALE, cgs.media.scoreboardScore);
	CG_DrawPicNorm((SB_PING_X - (SB_RATING_WIDTH / 2.0)) * NORM_HSCALE, (y) * NORM_VSCALE, (64) * NORM_HSCALE, (32) * NORM_VSCALE, cgs.media.scoreboardPing);
	CG_DrawPicNorm((SB_TIME_X - (SB_RATING_WIDTH / 2.0)) * NORM_HSCALE, (y) * NORM_VSCALE, (64) * NORM_HSCALE, (32) * NORM_VSCALE, cgs.media.scoreboardTime);
	CG_DrawPicNorm((SB_NAME_X - (SB_RATING_WIDTH / 2.0)) * NORM_HSCALE, (y) * NORM_VSCALE, (64) * NORM_HSCALE, (32) * NORM_VSCALE, cgs.media.scoreboardName);

	y = SB_TOP;

	// If there are more than SB_MAXCLIENTS_NORMAL, use the interleaved scores
	if (cg.numScores > SB_MAXCLIENTS_NORMAL)
	{
		maxClients = SB_MAXCLIENTS_INTER;
		lineHeight = SB_INTER_HEIGHT;
		topBorderSize = 8;
		bottomBorderSize = 16;
	}
	else
	{
		maxClients = SB_MAXCLIENTS_NORMAL;
		lineHeight = SB_NORMAL_HEIGHT;
		topBorderSize = 16;
		bottomBorderSize = 16;
	}

	localClient = qfalse;

	if (cgs.gametypeIsTeamGame)
	{
		//
		// teamplay scoreboard
		//
		y += lineHeight / 2;

		if (cg.teamScores[0] >= cg.teamScores[1])
		{
			n1 = CG_TeamScoreboard(y, TEAM_RED, fade, maxClients, lineHeight);
			CG_DrawTeamBackground(0, y - topBorderSize, 640, n1 * lineHeight + bottomBorderSize, 0.33f, TEAM_RED);
			y += (n1 * lineHeight) + BIGCHAR_HEIGHT;
			maxClients -= n1;
			n2 = CG_TeamScoreboard(y, TEAM_BLUE, fade, maxClients, lineHeight);
			CG_DrawTeamBackground(0, y - topBorderSize, 640, n2 * lineHeight + bottomBorderSize, 0.33f, TEAM_BLUE);
			y += (n2 * lineHeight) + BIGCHAR_HEIGHT;
			maxClients -= n2;
		}
		else
		{
			n1 = CG_TeamScoreboard(y, TEAM_BLUE, fade, maxClients, lineHeight);
			CG_DrawTeamBackground(0, y - topBorderSize, 640, n1 * lineHeight + bottomBorderSize, 0.33f, TEAM_BLUE);
			y += (n1 * lineHeight) + BIGCHAR_HEIGHT;
			maxClients -= n1;
			n2 = CG_TeamScoreboard(y, TEAM_RED, fade, maxClients, lineHeight);
			CG_DrawTeamBackground(0, y - topBorderSize, 640, n2 * lineHeight + bottomBorderSize, 0.33f, TEAM_RED);
			y += (n2 * lineHeight) + BIGCHAR_HEIGHT;
			maxClients -= n2;

		}

		n1 = CG_TeamScoreboard(y, TEAM_6, fade, maxClients, lineHeight);
		y += n1 * lineHeight + BIGCHAR_HEIGHT;
		n1 = CG_TeamScoreboard(y, TEAM_7, fade, maxClients, lineHeight);
		y += n1 * lineHeight + BIGCHAR_HEIGHT;
		n1 = CG_TeamScoreboard(y, TEAM_SPECTATOR, fade, maxClients, lineHeight);
		y += n1 * lineHeight + BIGCHAR_HEIGHT;
	}
	else
	{
		//
		// free for all scoreboard
		//
		n1 = CG_TeamScoreboard(y, TEAM_FREE, fade, maxClients, lineHeight);
		y += (n1 * lineHeight) + BIGCHAR_HEIGHT;
		n2 = CG_TeamScoreboard(y, TEAM_SPECTATOR, fade, maxClients - n1, lineHeight);
		y += (n2 * lineHeight) + BIGCHAR_HEIGHT;
	}

	if (!localClient)
	{
		// draw local client at the bottom
		for (i = 0 ; i < cg.numScores ; i++)
		{
			if (cg.scores[i].client == cg.snap->ps.clientNum)
			{
				CG_DrawClientScoreNew(y, &cg.scores[i], fadeColor, fade, lineHeight == SB_NORMAL_HEIGHT);
				break;
			}
		}
	}

	// load any models that have been deferred
	if (++cg.deferredPlayerLoading > 10)
	{
		CG_LoadDeferredPlayers();
	}

	return qtrue;
}

/*
=================
CG_ModernDrawFFAScoreboard

Quake Live style FFA scoreboard with weapon stats columns.
=================
*/
qboolean CG_ModernDrawFFAScoreboard(void)
{
	vec4_t *color;
	vec4_t bgColor, rowColor, hdrColor;
	int y, i, j, rank;
	int numPlayers, numSpecs;
	const char *s;
	score_t *score;
	clientInfo_t *ci;
	vec3_t headAngles;

	/* panel geometry */
	const float panelX = 70.0f;
	const float panelW = 500.0f;
	const float rowH = 18.0f;
	const float hdrY = 100.0f;
	const float firstRowY = 116.0f;

	/* column X positions (relative to panelX) */
	const float colRank  = panelX + 6;
	const float colHead  = panelX + 18;
	const float colName  = panelX + 34;
	const float colScore = panelX + 200;
	const float colKD    = panelX + 250;
	const float colDmg   = panelX + 300;
	const float colWeap  = panelX + 340;
	const float colAcc   = panelX + 400;
	const float colPing  = panelX + 470;

	if (cg_paused.integer)
		return qfalse;
	if (cg.warmup && !cg.showScores && cg.predictedPlayerState.pm_type != PM_INTERMISSION)
		return qfalse;

	if (!cg.showScores && cg.predictedPlayerState.pm_type != PM_DEAD && cg.predictedPlayerState.pm_type != PM_INTERMISSION)
	{
		color = (vec4_t *)CG_FadeColor(cg.scoreFadeTime, FADE_TIME);
	}
	else
	{
		color = &colorWhite;
	}

	if (!color)
	{
		cg.killerName[0] = 0;
		return qfalse;
	}

	/* count players and spectators */
	numPlayers = 0;
	numSpecs = 0;
	for (i = 0; i < cg.numScores; i++)
	{
		ci = &cgs.clientinfo[cg.scores[i].client];
		if (cg.demoPlayback && !ci->infoValid)
			continue;
		if (ci->team == TEAM_SPECTATOR)
			numSpecs++;
		else
			numPlayers++;
	}

	/* dark panel background */
	bgColor[0] = 0.05f; bgColor[1] = 0.05f; bgColor[2] = 0.1f; bgColor[3] = 0.7f;
	{
		float panelH = (firstRowY - 40) + numPlayers * rowH + 20;
		if (numSpecs > 0)
			panelH += 30 + ((numSpecs + 1) / 2) * 10;
		CG_FillRectNorm((panelX) * NORM_HSCALE, (34) * NORM_VSCALE, (panelW) * NORM_HSCALE, (panelH) * NORM_VSCALE, bgColor);
	}

	/* Header: title */
	y = 40;
	if (cg.demoPlayback)
	{
		trap_R_DrawTextNorm("^3Demo Playback", (640 / 2.0f) * NORM_HSCALE, (y) * NORM_VSCALE, FONT_DISPLAY, (18) * NORM_VSCALE, *color, TEXT_ALIGN_CENTER, TEXT_DROPSHADOW);
	}
	else if (cg.killerName[0])
	{
		s = va("Fragged by %s", cg.killerName);
		trap_R_DrawTextNorm(s, (640 / 2.0f) * NORM_HSCALE, (y) * NORM_VSCALE, FONT_DISPLAY, (18) * NORM_VSCALE, *color, TEXT_ALIGN_CENTER, TEXT_DROPSHADOW);
	}
	else
	{
		trap_R_DrawTextNorm(bg_gametypelist[cgs.gametype].name, (640 / 2.0f) * NORM_HSCALE, (y) * NORM_VSCALE, FONT_DISPLAY, (18) * NORM_VSCALE, *color, TEXT_ALIGN_CENTER, TEXT_DROPSHADOW);
	}

	/* Header: placement */
	y = 62;
	if (cg.snap->ps.persistant[PERS_TEAM] != TEAM_SPECTATOR)
	{
		s = va("%s place with %i",
		       CG_PlaceString(cg.snap->ps.persistant[PERS_RANK] + 1),
		       cg.snap->ps.persistant[PERS_SCORE]);
		trap_R_DrawTextNorm(s, (640 / 2.0f) * NORM_HSCALE, (y) * NORM_VSCALE, FONT_UI, (14) * NORM_VSCALE, *color, TEXT_ALIGN_CENTER, TEXT_DROPSHADOW);
	}

	/* thin separator line below header */
	{
		vec4_t lineColor = {0.3f, 0.3f, 0.4f, 0.6f};
		CG_FillRectNorm((panelX + 4) * NORM_HSCALE, (80) * NORM_VSCALE, (panelW - 8) * NORM_HSCALE, (1) * NORM_VSCALE, lineColor);
	}

	/* Column headers */
	hdrColor[0] = 0.7f; hdrColor[1] = 0.8f; hdrColor[2] = 0.9f; hdrColor[3] = (*color)[3];
	y = (int)hdrY;
	trap_R_DrawTextNorm("#", (     colRank) * NORM_HSCALE, (     y) * NORM_VSCALE, FONT_UI, (12) * NORM_VSCALE, hdrColor, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW);
	trap_R_DrawTextNorm("PLAYER", (colName) * NORM_HSCALE, (     y) * NORM_VSCALE, FONT_UI, (12) * NORM_VSCALE, hdrColor, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW);
	trap_R_DrawTextNorm("SCORE", ( colScore) * NORM_HSCALE, (    y) * NORM_VSCALE, FONT_UI, (12) * NORM_VSCALE, hdrColor, TEXT_ALIGN_RIGHT, TEXT_DROPSHADOW);
	trap_R_DrawTextNorm("K/D", (   colKD) * NORM_HSCALE, (       y) * NORM_VSCALE, FONT_UI, (12) * NORM_VSCALE, hdrColor, TEXT_ALIGN_RIGHT, TEXT_DROPSHADOW);
	trap_R_DrawTextNorm("DMG", (   colDmg) * NORM_HSCALE, (      y) * NORM_VSCALE, FONT_UI, (12) * NORM_VSCALE, hdrColor, TEXT_ALIGN_RIGHT, TEXT_DROPSHADOW);
	trap_R_DrawTextNorm("WEAP", (  colWeap + 16) * NORM_HSCALE, (y) * NORM_VSCALE, FONT_UI, (12) * NORM_VSCALE, hdrColor, TEXT_ALIGN_CENTER, TEXT_DROPSHADOW);
	trap_R_DrawTextNorm("ACC%", (  colAcc) * NORM_HSCALE, (      y) * NORM_VSCALE, FONT_UI, (12) * NORM_VSCALE, hdrColor, TEXT_ALIGN_RIGHT, TEXT_DROPSHADOW);
	trap_R_DrawTextNorm("PING", (  colPing) * NORM_HSCALE, (     y) * NORM_VSCALE, FONT_UI, (12) * NORM_VSCALE, hdrColor, TEXT_ALIGN_RIGHT, TEXT_DROPSHADOW);

	/* separator below column header */
	{
		vec4_t lineColor = {0.3f, 0.3f, 0.4f, 0.4f};
		CG_FillRectNorm((panelX + 4) * NORM_HSCALE, (hdrY + 13) * NORM_VSCALE, (panelW - 8) * NORM_HSCALE, (1) * NORM_VSCALE, lineColor);
	}

	/* Player rows */
	VectorClear(headAngles);
	headAngles[YAW] = 180;
	localClient = qfalse;
	rank = 0;
	y = (int)firstRowY;

	for (i = 0; i < cg.numScores; i++)
	{
		int totalDmg, bestAtt, bestAttKills, acc, pingColor;
		char tmpStr[128];

		score = &cg.scores[i];
		ci = &cgs.clientinfo[score->client];

		if (cg.demoPlayback && !ci->infoValid)
			continue;
		if (ci->team == TEAM_SPECTATOR)
			continue;

		rank++;

		/* highlight current player row */
		if (score->client == cg.snap->ps.clientNum)
		{
			vec4_t hlColor = {0.15f, 0.2f, 0.4f, 0.35f};
			localClient = qtrue;
			CG_FillRectNorm((panelX + 2) * NORM_HSCALE, ((float)y - 1) * NORM_VSCALE, (panelW - 4) * NORM_HSCALE, (rowH) * NORM_VSCALE, hlColor);
		}

		/* alternating row shading */
		if (rank % 2 == 0)
		{
			vec4_t altColor = {0.1f, 0.1f, 0.15f, 0.2f};
			CG_FillRectNorm((panelX + 2) * NORM_HSCALE, ((float)y - 1) * NORM_VSCALE, (panelW - 4) * NORM_HSCALE, (rowH) * NORM_VSCALE, altColor);
		}

		/* rank number */
		Com_sprintf(tmpStr, sizeof(tmpStr), "%i", rank);
		trap_R_DrawTextNorm(tmpStr, (colRank + 10) * NORM_HSCALE, (y) * NORM_VSCALE, FONT_UI, (14) * NORM_VSCALE, *color, TEXT_ALIGN_RIGHT, TEXT_DROPSHADOW);

		/* head icon */
		if (score->client == cg.snap->ps.clientNum)
			headAngles[YAW] = (float)cg.time / 14 + 180;
		else
			headAngles[YAW] = 180;
		CG_DrawHead(colHead, y + 1, 14, 14, score->client, headAngles);

		/* player name (proportional, truncated) */
		trap_R_DrawTextNorm(ci->name, (colName) * NORM_HSCALE, (y + 1) * NORM_VSCALE, FONT_UI, (13) * NORM_VSCALE, *color, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW);

		/* score */
		if (score->ping == -1)
		{
			trap_R_DrawTextNorm("^2...", (colScore) * NORM_HSCALE, (y) * NORM_VSCALE, FONT_UI, (14) * NORM_VSCALE, *color, TEXT_ALIGN_RIGHT, TEXT_DROPSHADOW);
			y += (int)rowH;
			continue;
		}

		Com_sprintf(tmpStr, sizeof(tmpStr), "%i", score->score);
		trap_R_DrawTextNorm(tmpStr, (colScore) * NORM_HSCALE, (y) * NORM_VSCALE, FONT_UI, (14) * NORM_VSCALE, *color, TEXT_ALIGN_RIGHT, TEXT_DROPSHADOW);

		/* K/D */
		Com_sprintf(tmpStr, sizeof(tmpStr), "%i/%i", score->score, score->deaths);
		trap_R_DrawTextNorm(tmpStr, (colKD) * NORM_HSCALE, (y + 2) * NORM_VSCALE, FONT_UI, (12) * NORM_VSCALE, *color, TEXT_ALIGN_RIGHT, TEXT_DROPSHADOW);

		/* total damage */
		totalDmg = 0;
		for (j = ATT_NONE + 1; j < ATT_NUM_ATTACKS; j++) {
			totalDmg += cgs.attackStats[score->client][j].damage;
		}
		Com_sprintf(tmpStr, sizeof(tmpStr), "%i", totalDmg);
		trap_R_DrawTextNorm(tmpStr, (colDmg) * NORM_HSCALE, (y + 2) * NORM_VSCALE, FONT_UI, (12) * NORM_VSCALE, *color, TEXT_ALIGN_RIGHT, TEXT_DROPSHADOW);

		/* best weapon icon (by kills) */
		bestAtt = ATT_NONE;
		bestAttKills = 0;
		for (j = ATT_NONE + 1; j < ATT_NUM_ATTACKS; j++)
		{
			if (cgs.attackStats[score->client][j].kills > bestAttKills)
			{
				bestAttKills = cgs.attackStats[score->client][j].kills;
				bestAtt = j;
			}
		}

		int bestWp = bg_attacklist[bestAtt].weapon;
		if ( bestWp != WP_NONE && cg_weapons[bestWp].weaponIcon ) {
			CG_DrawPicNorm((colWeap + 8) * NORM_HSCALE, (y + 1) * NORM_VSCALE, (14) * NORM_HSCALE, (14) * NORM_VSCALE, cg_weapons[bestWp].weaponIcon);
		}

		/* accuracy (colored) */
		acc = score->accuracy;
		if (acc > 50)
			Com_sprintf(tmpStr, sizeof(tmpStr), "^2%i%%", acc);
		else if (acc > 30)
			Com_sprintf(tmpStr, sizeof(tmpStr), "^3%i%%", acc);
		else
			Com_sprintf(tmpStr, sizeof(tmpStr), "^1%i%%", acc);
		trap_R_DrawTextNorm(tmpStr, (colAcc) * NORM_HSCALE, (y + 2) * NORM_VSCALE, FONT_UI, (12) * NORM_VSCALE, *color, TEXT_ALIGN_RIGHT, TEXT_DROPSHADOW);

		/* ping (colored) */
		if (score->ping < 40)
			pingColor = 2;
		else if (score->ping < 100)
			pingColor = 3;
		else
			pingColor = 1;
		Com_sprintf(tmpStr, sizeof(tmpStr), "^%i%i", pingColor, score->ping);
		trap_R_DrawTextNorm(tmpStr, (colPing) * NORM_HSCALE, (y + 2) * NORM_VSCALE, FONT_UI, (12) * NORM_VSCALE, *color, TEXT_ALIGN_RIGHT, TEXT_DROPSHADOW);

		y += (int)rowH;
	}

	/* separator before spectators */
	if (numSpecs > 0)
	{
		vec4_t lineColor = {0.3f, 0.3f, 0.4f, 0.4f};
		vec4_t specBg = {0.05f, 0.05f, 0.1f, 0.4f};
		int specY, specCount, si;

		y += 6;
		CG_FillRectNorm((panelX + 4) * NORM_HSCALE, ((float)y) * NORM_VSCALE, (panelW - 8) * NORM_HSCALE, (1) * NORM_VSCALE, lineColor);
		y += 4;

		trap_R_DrawTextNorm("Spectators", (640 / 2.0f) * NORM_HSCALE, (y) * NORM_VSCALE, FONT_UI, (12) * NORM_VSCALE, hdrColor, TEXT_ALIGN_CENTER, TEXT_DROPSHADOW);
		y += 14;

		specY = y;
		specCount = 0;
		for (si = 0; si < cg.numScores; si++)
		{
			int sx, sy;
			score = &cg.scores[si];
			ci = &cgs.clientinfo[score->client];
			if (cg.demoPlayback && !ci->infoValid)
				continue;
			if (ci->team != TEAM_SPECTATOR)
				continue;

			sx = (specCount % 2 == 0) ? (int)(panelX + 20) : (int)(panelX + 260);
			sy = specY + (specCount / 2) * 10;
			trap_R_DrawTextNorm(ci->name, (sx) * NORM_HSCALE, (sy) * NORM_VSCALE, FONT_UI, (10) * NORM_VSCALE, *color, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW);
			specCount++;
		}
	}

	if (++cg.deferredPlayerLoading > 10)
		CG_LoadDeferredPlayers();

	return qtrue;
}

/*
=================
CG_ModernDrawDuelScoreboard

CPMA-style tournament scoreboard with per-attack stats.
=================
*/

/* helper: draw one fighter's per-attack stats panel */
static void CG_DrawDuelFighterPanel(int px, int py, int pw, score_t *sc, float fade)
{
	clientInfo_t *ci;
	vec3_t headAngles;
	vec4_t panelBg, hdrBg, lineColor, textColor;
	char tmpStr[128];
	int att, wy, totalHits, totalShots, totalKills, totalDeaths;
	int acc, eff;

	/* column positions within panel */
	int cKD   = px + 44;
	int cEff  = px + 84;
	int cIcon = px + 104;
	int cHits = px + 170;
	int cAcc  = px + pw - 8;

	ci = &cgs.clientinfo[sc->client];

	/* panel background */
	panelBg[0] = 0.08f; panelBg[1] = 0.08f; panelBg[2] = 0.12f; panelBg[3] = 0.65f;
	CG_FillRectNorm(((float)px) * NORM_HSCALE, ((float)py) * NORM_VSCALE, ((float)pw) * NORM_HSCALE, (400.0f) * NORM_VSCALE, panelBg);

	/* header area background */
	hdrBg[0] = 0.12f; hdrBg[1] = 0.12f; hdrBg[2] = 0.2f; hdrBg[3] = 0.5f;
	CG_FillRectNorm(((float)px) * NORM_HSCALE, ((float)py) * NORM_VSCALE, ((float)pw) * NORM_HSCALE, (70.0f) * NORM_VSCALE, hdrBg);

	/* head icon */
	VectorClear(headAngles);
	headAngles[YAW] = (sc->client == cg.snap->ps.clientNum) ? ((float)cg.time / 14 + 180) : 180;
	CG_DrawHead(px + 4, py + 4, 48, 48, sc->client, headAngles);

	/* player name */
	textColor[0] = textColor[1] = textColor[2] = 1.0f; textColor[3] = fade;
	trap_R_DrawTextNorm(ci->name, (px + 58) * NORM_HSCALE, (py + 6) * NORM_VSCALE, FONT_UI, (16) * NORM_VSCALE, textColor, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW);

	/* big score */
	Com_sprintf(tmpStr, sizeof(tmpStr), "%i", sc->score);
	trap_R_DrawTextNorm(tmpStr, (px + pw - 8) * NORM_HSCALE, (py + 2) * NORM_VSCALE, FONT_DISPLAY, (28) * NORM_VSCALE, textColor, TEXT_ALIGN_RIGHT, TEXT_DROPSHADOW);

	/* sub-header: acc, W/L, ping */
	Com_sprintf(tmpStr, sizeof(tmpStr), "Acc: %i%%  W:%i L:%i  Ping: %ims",
	            sc->accuracy, ci->wins, ci->losses, sc->ping);
	trap_R_DrawTextNorm(tmpStr, (px + 58) * NORM_HSCALE, (py + 28) * NORM_VSCALE, FONT_UI, (10) * NORM_VSCALE, textColor, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW);

	/* award badges */
	{
		int bx = px + 58;
		int by = py + 42;
		vec4_t awardColor;
		awardColor[0] = 1.0f; awardColor[1] = 0.85f; awardColor[2] = 0.0f; awardColor[3] = fade;
		if (sc->excellentCount > 0)
		{
			Com_sprintf(tmpStr, sizeof(tmpStr), "^3Exc:%i", sc->excellentCount);
			trap_R_DrawTextNorm(tmpStr, (bx) * NORM_HSCALE, (by) * NORM_VSCALE, FONT_UI, (9) * NORM_VSCALE, awardColor, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW);
			bx += 48;
		}
		if (sc->impressiveCount > 0)
		{
			Com_sprintf(tmpStr, sizeof(tmpStr), "^3Imp:%i", sc->impressiveCount);
			trap_R_DrawTextNorm(tmpStr, (bx) * NORM_HSCALE, (by) * NORM_VSCALE, FONT_UI, (9) * NORM_VSCALE, awardColor, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW);
			bx += 48;
		}
		if (sc->guantletCount > 0)
		{
			Com_sprintf(tmpStr, sizeof(tmpStr), "^3Gnt:%i", sc->guantletCount);
			trap_R_DrawTextNorm(tmpStr, (bx) * NORM_HSCALE, (by) * NORM_VSCALE, FONT_UI, (9) * NORM_VSCALE, awardColor, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW);
		}
	}

	/* separator */
	lineColor[0] = 0.4f; lineColor[1] = 0.4f; lineColor[2] = 0.5f; lineColor[3] = 0.5f;
	CG_FillRectNorm(((float)px + 4) * NORM_HSCALE, ((float)(py + 70)) * NORM_VSCALE, ((float)(pw - 8)) * NORM_HSCALE, (1.0f) * NORM_VSCALE, lineColor);

	/* weapon table column headers */
	wy = py + 76;
	{
		vec4_t colHdr;
		colHdr[0] = 0.6f; colHdr[1] = 0.7f; colHdr[2] = 0.8f; colHdr[3] = fade;
		trap_R_DrawTextNorm("K/D", (      cKD) * NORM_HSCALE, (  wy) * NORM_VSCALE, FONT_UI, (10) * NORM_VSCALE, colHdr, TEXT_ALIGN_RIGHT, TEXT_DROPSHADOW);
		trap_R_DrawTextNorm("EFF%", (     cEff) * NORM_HSCALE, ( wy) * NORM_VSCALE, FONT_UI, (10) * NORM_VSCALE, colHdr, TEXT_ALIGN_RIGHT, TEXT_DROPSHADOW);
		/* icon column - no header */
		trap_R_DrawTextNorm("HITS/ATTS", (cHits) * NORM_HSCALE, (wy) * NORM_VSCALE, FONT_UI, (10) * NORM_VSCALE, colHdr, TEXT_ALIGN_RIGHT, TEXT_DROPSHADOW);
		trap_R_DrawTextNorm("ACC%", (     cAcc) * NORM_HSCALE, ( wy) * NORM_VSCALE, FONT_UI, (10) * NORM_VSCALE, colHdr, TEXT_ALIGN_RIGHT, TEXT_DROPSHADOW);
	}

	CG_FillRectNorm(((float)px + 4) * NORM_HSCALE, ((float)(wy + 11)) * NORM_VSCALE, ((float)(pw - 8)) * NORM_HSCALE, (1.0f) * NORM_VSCALE, lineColor);
	wy += 14;

	/* per-weapon rows */
	totalHits = 0; totalShots = 0; totalKills = 0; totalDeaths = 0;
	for (att = ATT_NONE + 1; att < ATT_NUM_ATTACKS; att++)
	{
		int weap;
		int aHits   = cgs.attackStats[sc->client][att].hits;
		int aShots  = cgs.attackStats[sc->client][att].shots;
		int aKills  = cgs.attackStats[sc->client][att].kills;
		int aDeaths = cgs.attackStats[sc->client][att].deaths;

		if (aShots == 0 && aKills == 0 && aDeaths == 0)
			continue;

		totalHits += aHits;
		totalShots += aShots;
		totalKills += aKills;
		totalDeaths += aDeaths;

		/* alternating row bg */
		if (((wy - py) / 14) % 2 == 0)
		{
			vec4_t altBg = {0.1f, 0.1f, 0.15f, 0.25f};
			CG_FillRectNorm(((float)px + 2) * NORM_HSCALE, ((float)wy - 1) * NORM_VSCALE, ((float)(pw - 4)) * NORM_HSCALE, (14.0f) * NORM_VSCALE, altBg);
		}

		/* K/D */
		Com_sprintf(tmpStr, sizeof(tmpStr), "%i/%i", aKills, aDeaths);
		trap_R_DrawTextNorm(tmpStr, (cKD) * NORM_HSCALE, (wy) * NORM_VSCALE, FONT_UI, (11) * NORM_VSCALE, textColor, TEXT_ALIGN_RIGHT, TEXT_DROPSHADOW);

		/* efficiency */
		eff = (aKills + aDeaths > 0) ? (aKills * 100 / (aKills + aDeaths)) : 0;
		if (aKills > 0 && aDeaths == 0) eff = 100;
		Com_sprintf(tmpStr, sizeof(tmpStr), "%i%%", eff);
		trap_R_DrawTextNorm(tmpStr, (cEff) * NORM_HSCALE, (wy) * NORM_VSCALE, FONT_UI, (11) * NORM_VSCALE, textColor, TEXT_ALIGN_RIGHT, TEXT_DROPSHADOW);

		/* weapon icon */
		weap = bg_attacklist[att].weapon;
		if (weap != WP_NONE && cg_weapons[weap].weaponIcon)
		{
			CG_DrawPicNorm(((float)(cIcon)) * NORM_HSCALE, ((float)(wy)) * NORM_VSCALE, (12.0f) * NORM_HSCALE, (12.0f) * NORM_VSCALE, cg_weapons[weap].weaponIcon);
		}

		/* hits/shots */
		Com_sprintf(tmpStr, sizeof(tmpStr), "%i/%i", aHits, aShots);
		trap_R_DrawTextNorm(tmpStr, (cHits) * NORM_HSCALE, (wy) * NORM_VSCALE, FONT_UI, (11) * NORM_VSCALE, textColor, TEXT_ALIGN_RIGHT, TEXT_DROPSHADOW);

		/* accuracy */
		acc = (aShots > 0) ? (aHits * 100 / aShots) : 0;
		if (acc > 50)
			Com_sprintf(tmpStr, sizeof(tmpStr), "^2%i%%", acc);
		else if (acc > 30)
			Com_sprintf(tmpStr, sizeof(tmpStr), "^3%i%%", acc);
		else
			Com_sprintf(tmpStr, sizeof(tmpStr), "^1%i%%", acc);
		trap_R_DrawTextNorm(tmpStr, (cAcc) * NORM_HSCALE, (wy) * NORM_VSCALE, FONT_UI, (11) * NORM_VSCALE, textColor, TEXT_ALIGN_RIGHT, TEXT_DROPSHADOW);

		wy += 14;
	}

	/* summary row */
	CG_FillRectNorm(((float)px + 4) * NORM_HSCALE, ((float)(wy - 1)) * NORM_VSCALE, ((float)(pw - 8)) * NORM_HSCALE, (1.0f) * NORM_VSCALE, lineColor);
	wy += 3;
	{
		vec4_t sumColor;
		sumColor[0] = 0.9f; sumColor[1] = 0.9f; sumColor[2] = 1.0f; sumColor[3] = fade;

		Com_sprintf(tmpStr, sizeof(tmpStr), "%i/%i", totalKills, totalDeaths);
		trap_R_DrawTextNorm(tmpStr, (cKD) * NORM_HSCALE, (wy) * NORM_VSCALE, FONT_UI, (11) * NORM_VSCALE, sumColor, TEXT_ALIGN_RIGHT, TEXT_DROPSHADOW);

		eff = (totalKills + totalDeaths > 0) ? (totalKills * 100 / (totalKills + totalDeaths)) : 0;
		Com_sprintf(tmpStr, sizeof(tmpStr), "%i%%", eff);
		trap_R_DrawTextNorm(tmpStr, (cEff) * NORM_HSCALE, (wy) * NORM_VSCALE, FONT_UI, (11) * NORM_VSCALE, sumColor, TEXT_ALIGN_RIGHT, TEXT_DROPSHADOW);

		/* "TOTAL" label in icon column */
		trap_R_DrawTextNorm("TOTAL", (cIcon + 6) * NORM_HSCALE, (wy) * NORM_VSCALE, FONT_UI, (9) * NORM_VSCALE, sumColor, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW);

		Com_sprintf(tmpStr, sizeof(tmpStr), "%i/%i", totalHits, totalShots);
		trap_R_DrawTextNorm(tmpStr, (cHits) * NORM_HSCALE, (wy) * NORM_VSCALE, FONT_UI, (11) * NORM_VSCALE, sumColor, TEXT_ALIGN_RIGHT, TEXT_DROPSHADOW);

		acc = (totalShots > 0) ? (totalHits * 100 / totalShots) : 0;
		Com_sprintf(tmpStr, sizeof(tmpStr), "%i%%", acc);
		trap_R_DrawTextNorm(tmpStr, (cAcc) * NORM_HSCALE, (wy) * NORM_VSCALE, FONT_UI, (11) * NORM_VSCALE, sumColor, TEXT_ALIGN_RIGHT, TEXT_DROPSHADOW);
	}

	/* clip the panel background to actual content height */
	/* (we drew 400px tall; overdraw is hidden by the dark full-screen bg) */
	(void)wy;
}

qboolean CG_ModernDrawDuelScoreboard(void)
{
	vec4_t *color;
	vec4_t bgColor, textColor;
	int y, i;
	int fighter[2];
	int numFighters = 0;
	int drewSpect, specCount;
	const char *s;
	score_t *sc;
	clientInfo_t *ci;

	if (cg_paused.integer)
		return qfalse;
	if (cg.warmup && !cg.showScores && cg.predictedPlayerState.pm_type != PM_INTERMISSION)
		return qfalse;

	if (!cg.showScores && cg.predictedPlayerState.pm_type != PM_DEAD && cg.predictedPlayerState.pm_type != PM_INTERMISSION)
	{
		color = (vec4_t *)CG_FadeColor(cg.scoreFadeTime, FADE_TIME);
	}
	else
	{
		color = &colorWhite;
	}

	if (!color)
	{
		cg.killerName[0] = 0;
		return qfalse;
	}

	/* find the two fighters */
	fighter[0] = fighter[1] = -1;
	for (i = 0; i < cg.numScores && numFighters < 2; i++)
	{
		ci = &cgs.clientinfo[cg.scores[i].client];
		if (ci->team != TEAM_SPECTATOR)
		{
			fighter[numFighters++] = i;
		}
	}

	/* full dark background */
	bgColor[0] = 0.02f; bgColor[1] = 0.02f; bgColor[2] = 0.05f; bgColor[3] = 0.85f;
	CG_FillRectNorm((0) * NORM_HSCALE, (0) * NORM_VSCALE, (640) * NORM_HSCALE, (480) * NORM_VSCALE, bgColor);

	textColor[0] = textColor[1] = textColor[2] = 1.0f;
	textColor[3] = (*color)[3];

	/* large timer at top */
	{
		int msec = cg.time - cgs.levelStartTime;
		int secs = msec / 1000;
		int mins = secs / 60;
		secs %= 60;
		s = va("%i:%02i", mins, secs);
		trap_R_DrawTextNorm(s, (640 / 2.0f) * NORM_HSCALE, (8) * NORM_VSCALE, FONT_DISPLAY, (28) * NORM_VSCALE, textColor, TEXT_ALIGN_CENTER, TEXT_DROPSHADOW);
	}

	/* fragged by */
	if (cg.killerName[0] && !cg.demoPlayback)
	{
		s = va("Fragged by %s", cg.killerName);
		trap_R_DrawTextNorm(s, (640 / 2.0f) * NORM_HSCALE, (38) * NORM_VSCALE, FONT_UI, (12) * NORM_VSCALE, colorYellow, TEXT_ALIGN_CENTER, TEXT_DROPSHADOW);
	}

	/* two side-by-side panels */
	localClient = qfalse;
	if (fighter[0] >= 0)
	{
		sc = &cg.scores[fighter[0]];
		if (sc->client == cg.snap->ps.clientNum)
			localClient = qtrue;
		CG_DrawDuelFighterPanel(8, 56, 304, sc, (*color)[3]);
	}
	if (fighter[1] >= 0)
	{
		sc = &cg.scores[fighter[1]];
		if (sc->client == cg.snap->ps.clientNum)
			localClient = qtrue;
		CG_DrawDuelFighterPanel(328, 56, 304, sc, (*color)[3]);
	}

	/* "VS" between panels */
	{
		vec4_t vsColor;
		vsColor[0] = 0.5f; vsColor[1] = 0.5f; vsColor[2] = 0.6f; vsColor[3] = (*color)[3] * 0.6f;
		trap_R_DrawTextNorm("vs", (640 / 2.0f) * NORM_HSCALE, (80) * NORM_VSCALE, FONT_DISPLAY, (14) * NORM_VSCALE, vsColor, TEXT_ALIGN_CENTER, TEXT_DROPSHADOW);
	}

	/* spectators at bottom */
	y = 460;
	specCount = 0;
	for (i = 0; i < cg.numScores; i++)
	{
		ci = &cgs.clientinfo[cg.scores[i].client];
		if (cg.demoPlayback && !ci->infoValid)
			continue;
		if (ci->team != TEAM_SPECTATOR)
			continue;
		specCount++;
	}

	if (specCount > 0)
	{
		vec4_t specHdr;
		int sx;
		specHdr[0] = 0.6f; specHdr[1] = 0.6f; specHdr[2] = 0.7f; specHdr[3] = (*color)[3];

		y = 450 - ((specCount + 3) / 4) * 10;
		trap_R_DrawTextNorm("Spectators", (640 / 2.0f) * NORM_HSCALE, (y - 12) * NORM_VSCALE, FONT_UI, (10) * NORM_VSCALE, specHdr, TEXT_ALIGN_CENTER, TEXT_DROPSHADOW);

		drewSpect = 0;
		for (i = 0; i < cg.numScores; i++)
		{
			ci = &cgs.clientinfo[cg.scores[i].client];
			if (cg.demoPlayback && !ci->infoValid)
				continue;
			if (ci->team != TEAM_SPECTATOR)
				continue;

			sx = 40 + (drewSpect % 4) * 148;
			trap_R_DrawTextNorm(ci->name, (sx) * NORM_HSCALE, (y + (drewSpect / 4) * 10) * NORM_VSCALE, FONT_UI, (9) * NORM_VSCALE, textColor, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW);
			drewSpect++;
		}
	}

	/* server info footer */
	{
		vec4_t footColor;
		footColor[0] = 0.4f; footColor[1] = 0.4f; footColor[2] = 0.5f; footColor[3] = (*color)[3] * 0.7f;
		s = va("%s // %s // Tournament",
		       Info_ValueForKey(CG_ConfigString(CS_SERVERINFO), "sv_hostname"),
		       cgs.mapname);
		trap_R_DrawTextNorm(s, (640 / 2.0f) * NORM_HSCALE, (468) * NORM_VSCALE, FONT_UI, (8) * NORM_VSCALE, footColor, TEXT_ALIGN_CENTER, TEXT_DROPSHADOW);
	}

	if (++cg.deferredPlayerLoading > 10)
		CG_LoadDeferredPlayers();

	return qtrue;
}

//================================================================================

/*
================
CG_CenterGiantLine
================
*/
static void CG_CenterGiantLine(float y, const char *string)
{
	vec4_t      color;

	color[0] = 1;
	color[1] = 1;
	color[2] = 1;
	color[3] = 1;

	trap_R_DrawTextNorm(string, (640 / 2.0f) * NORM_HSCALE, (y) * NORM_VSCALE, FONT_DISPLAY, (GIANT_HEIGHT) * NORM_VSCALE, color, TEXT_ALIGN_CENTER, TEXT_DROPSHADOW);
}

/*
=================
CG_DrawDuelScoreboard

Draw the oversize scoreboard for tournements
=================
*/
void CG_DrawDuelScoreboard(void)
{
	const char      *s;
	vec4_t          color;
	int             min, tens, ones;
	clientInfo_t    *ci;
	int             y;
	int             i;

	// request more scores regularly
	if (cg.scoresRequestTime + 2000 < cg.time)
	{
		cg.scoresRequestTime = cg.time;
		if (!cg.demoPlayback)
		{
			trap_SendClientCommand("score");
		}
	}

	color[0] = 1;
	color[1] = 1;
	color[2] = 1;
	color[3] = 1;

	// draw the dialog background
	color[0] = color[1] = color[2] = 0;
	color[3] = 1;
	CG_FillRectNorm((0) * NORM_HSCALE, (0) * NORM_VSCALE, (640) * NORM_HSCALE, (480) * NORM_VSCALE, color);

	// print the mesage of the day
	s = CG_ConfigString(CS_MOTD);
	if (!s[0])
	{
		s = "Scoreboard";
	}

	// print optional title
	CG_CenterGiantLine(8, s);

	// print server time
	ones = cg.time / 1000;
	min = ones / 60;
	ones %= 60;
	tens = ones / 10;
	ones %= 10;
	s = va("%i:%i%i", min, tens, ones);

	CG_CenterGiantLine(64, s);


	// print the two scores

	y = 160;
	if (cgs.gametypeIsTeamGame)
	{
		//
		// teamplay scoreboard
		//
		trap_R_DrawTextNorm("Red Team", (8) * NORM_HSCALE, (y) * NORM_VSCALE, FONT_DISPLAY, (GIANT_HEIGHT) * NORM_VSCALE, color, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW);
		s = va("%i", cg.teamScores[0]);
		trap_R_DrawTextNorm(s, (632) * NORM_HSCALE, (y) * NORM_VSCALE, FONT_DISPLAY, (GIANT_HEIGHT) * NORM_VSCALE, color, TEXT_ALIGN_RIGHT, TEXT_DROPSHADOW);

		y += 64;

		trap_R_DrawTextNorm("Blue Team", (8) * NORM_HSCALE, (y) * NORM_VSCALE, FONT_DISPLAY, (GIANT_HEIGHT) * NORM_VSCALE, color, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW);
		s = va("%i", cg.teamScores[1]);
		trap_R_DrawTextNorm(s, (632) * NORM_HSCALE, (y) * NORM_VSCALE, FONT_DISPLAY, (GIANT_HEIGHT) * NORM_VSCALE, color, TEXT_ALIGN_RIGHT, TEXT_DROPSHADOW);
	}
	else
	{
		//
		// free for all scoreboard
		//
		for (i = 0 ; i < MAX_CLIENTS ; i++)
		{
			ci = &cgs.clientinfo[i];
			if (!ci->infoValid)
			{
				continue;
			}
			if (ci->team != TEAM_FREE)
			{
				continue;
			}

			trap_R_DrawTextNorm(CG_ClientName( ci ), (8) * NORM_HSCALE, (y) * NORM_VSCALE, FONT_DISPLAY, (GIANT_HEIGHT) * NORM_VSCALE, color, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW);
			s = va("%i", ci->score);
			trap_R_DrawTextNorm(s, (632) * NORM_HSCALE, (y) * NORM_VSCALE, FONT_DISPLAY, (GIANT_HEIGHT) * NORM_VSCALE, color, TEXT_ALIGN_RIGHT, TEXT_DROPSHADOW);
			y += 64;
		}
	}
}

/*
=================
CG_ModernDrawClientScore
=================
*/
static void CG_ModernDrawClientScore(int x, int y, const score_t *score, const float *color, float fade)
{
	char string[1024];
	clientInfo_t *ci;
	vec3_t  headAngles;

	if (score->client < 0 || score->client >= cgs.maxclients)
	{
		Com_Printf("Bad score->client: %i\n", score->client);
		return;
	}

	ci = &cgs.clientinfo[score->client];

	if (score->client == cg.snap->ps.clientNum)
	{
		vec4_t ourColor;
		localClient = qtrue;
		ourColor[0] = 0.7f;
		ourColor[1] = 0.7f;
		ourColor[2] = 0.7f;
		ourColor[3] = 0.2f * fade;
		CG_FillRectNorm((x + 8) * NORM_HSCALE, (y) * NORM_VSCALE, (304.0f) * NORM_HSCALE, (17.0f) * NORM_VSCALE, ourColor);
	}
	if (ci->powerups & (1 << PW_REDFLAG))
	{
		CG_DrawFlagModel(x + 4, y, 16.0f, 16.0f, TEAM_RED, qfalse);
	}
	else if (ci->powerups & (1 << PW_BLUEFLAG))
	{
		CG_DrawFlagModel(x + 4, y, 16.0f, 16.0f, TEAM_BLUE, qfalse);
	}
	trap_R_SetColor(NULL);

	VectorClear(headAngles);
	headAngles[1] = 180.0f;
	CG_DrawHead(x + 22, y, 16.0f, 16.0f, score->client, headAngles);

	// frozen foe tag not available in q3now

	if (score->ping == -1)
	{
		Com_sprintf(string, 1024, " ^2connecting^7      %s", ci->name);
		trap_R_DrawTextNorm(string, (x + 34) * NORM_HSCALE, (y + 2) * NORM_VSCALE, FONT_UI, (12) * NORM_VSCALE, colorWhite, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW);
	}
	else
	{
		int pingColor = 1;
		if (score->ping < 40)
		{
			pingColor = 7;
		}
		else if (score->ping < 70)
		{
			pingColor = 2;
		}
		else if (score->ping < 100)
		{
			pingColor = 3;
		}
		else if (score->ping < 200)
		{
			pingColor = 8;
		}
		else if (score->ping < 400)
		{
			pingColor = 6;
		}
		Com_sprintf(string, 1024, "%3i", score->score);
		trap_R_DrawTextNorm(string, (x + 46) * NORM_HSCALE, (y) * NORM_VSCALE, FONT_DISPLAY, (16) * NORM_VSCALE, color, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW);
		if (cgs.gametype == GT_TDM)
		{
			if (CG_ModernIsGameTypeFreeze())
			{
				Com_sprintf(string, 1024, "%3i", score->scoreFlags);
			}
			else
			{
				Com_sprintf(string, 1024, "^%i%3i", score->scoreFlags < 0 ? 3 : 7, score->scoreFlags);
			}
			trap_R_DrawTextNorm(string, (x + 90) * NORM_HSCALE, (y + 4) * NORM_VSCALE, FONT_UI, (12) * NORM_VSCALE, color, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW);
		}
		else
		{
			trap_R_DrawTextNorm(" 0", (x + 90) * NORM_HSCALE, (y + 4) * NORM_VSCALE, FONT_UI, (12) * NORM_VSCALE, color, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW);
		}
		Com_sprintf(string, 1024, "^%i%3i", pingColor, score->ping);
		trap_R_DrawTextNorm(string, (x + 118) * NORM_HSCALE, (y) * NORM_VSCALE, FONT_DISPLAY, (16) * NORM_VSCALE, color, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW);
		Com_sprintf(string, 1024, "%3i", score->time);
		trap_R_DrawTextNorm(string, (x + 150) * NORM_HSCALE, (y) * NORM_VSCALE, FONT_DISPLAY, (16) * NORM_VSCALE, color, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW);
		Com_sprintf(string, 1024, "%s", ci->name);
		trap_R_DrawTextNorm(string, (x + 202) * NORM_HSCALE, (y + 4) * NORM_VSCALE, FONT_UI, (12) * NORM_VSCALE, color, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW);

	}
	if (!cg.warmup && cg.predictedPlayerState.pm_type != PM_INTERMISSION)
	{
		return;
	}
	// STAT_CLIENTS_READY not available yet in q3now
	return;
}

static void CG_ModernDrawPowerupFrame(int x, int y, const clientInfo_t *ci)
{
	vec4_t borderSize;
	qboolean drawAny = qfalse;

	team_t myTeam = cgs.clientinfo[cg.clientNum].team;

	float x1 = (float)x + 8;
	float y1 = (float)y;
	float w1 = 304.0f;
	float h1 = 17.0f;

	if (myTeam != TEAM_SPECTATOR && ci->team != myTeam && ci->team != TEAM_SPECTATOR)
	{
		return;
	}

	Vector4Copy(defaultBorderSize, borderSize);

	if (ci->powerups & (1 << PW_QUAD))
	{
		drawAny = qtrue;
		CG_ModernDrawFrame(x1, y1, w1, h1, borderSize, colorCyan, qtrue);
	}

	if (ci->powerups & (1 << PW_BERSERK))
	{
		drawAny = qtrue;
		CG_ModernDrawFrame(x1, y1, w1, h1, borderSize, colorRed, qtrue);
	}

	if (ci->powerups & (1 << PW_BATTLESUIT))
	{
		x1 += borderSize[0];
		y1 += borderSize[1];
		w1 -= borderSize[0] + borderSize[2];
		h1 -= borderSize[1] + borderSize[3];
		drawAny = qtrue;
		CG_ModernDrawFrame(x1, y1, w1, h1, borderSize, colorOrange, qtrue);
	}

	if (ci->powerups & (1 << PW_HASTE))
	{
		x1 += borderSize[0];
		y1 += borderSize[1];
		w1 -= borderSize[0] + borderSize[2];
		h1 -= borderSize[1] + borderSize[3];
		drawAny = qtrue;
		CG_ModernDrawFrame(x1, y1, w1, h1, borderSize, colorYellow, qtrue);
	}

	if (ci->powerups & (1 << PW_INVIS))
	{
		x1 += borderSize[0];
		y1 += borderSize[1];
		w1 -= borderSize[0] + borderSize[2];
		h1 -= borderSize[1] + borderSize[3];
		drawAny = qtrue;
		CG_ModernDrawFrame(x1, y1, w1, h1, borderSize, colorWhite, qtrue);
	}

	if (ci->powerups & (1 << PW_REGEN))
	{
		x1 += borderSize[0];
		y1 += borderSize[1];
		w1 -= borderSize[0] + borderSize[2];
		h1 -= borderSize[1] + borderSize[3];
		drawAny = qtrue;
		CG_ModernDrawFrame(x1, y1, w1, h1, borderSize, colorRed, qtrue);
	}

	if (ci->powerups & (1 << PW_FLIGHT))
	{
		x1 += borderSize[0];
		y1 += borderSize[1];
		w1 -= borderSize[0] + borderSize[2];
		h1 -= borderSize[1] + borderSize[3];
		drawAny = qtrue;
		CG_ModernDrawFrame(x1, y1, w1, h1, borderSize, colorMagenta, qtrue);
	}

	if (!drawAny)
	{
		return;
	}
}


static void CG_BEDrawTeamClientScore(int x, int y, const score_t *score, const float *color, float fade)
{
	char string[1024];
	float mHeight = 12;
	float bHeight = 16;
	clientInfo_t *ci;
	vec3_t  headAngles;

	if (score->client < 0 || score->client >= cgs.maxclients)
	{
		Com_Printf("Bad score->client: %i\n", score->client);
		return;
	}

	ci = &cgs.clientinfo[score->client];
	if (score->client == cg.snap->ps.clientNum)
	{
		vec4_t ourColor;
		localClient = qtrue;
		ourColor[0] = 0.7f;
		ourColor[1] = 0.7f;
		ourColor[2] = 0.7f;
		ourColor[3] = 0.2f * fade;
		CG_FillRectNorm((x + 8) * NORM_HSCALE, (y) * NORM_VSCALE, (304.0f) * NORM_HSCALE, (17.0f) * NORM_VSCALE, ourColor);
	}
	if (ci->powerups & (1 << PW_REDFLAG))
	{
		CG_DrawFlagModel(x + 4, y, 16.0f, 16.0f, TEAM_RED, qfalse);
	}
	else if (ci->powerups & (1 << PW_BLUEFLAG))
	{
		CG_DrawFlagModel(x + 4, y, 16.0f, 16.0f, TEAM_BLUE, qfalse);
	}
	trap_R_SetColor(NULL);

	CG_ModernDrawPowerupFrame(x, y, ci);

	VectorClear(headAngles);
	headAngles[1] = 180.0f;
	CG_DrawHead(x + 22, y, 16.0f, 16.0f, score->client, headAngles);

	// frozen foe tag not available in q3now

	if (score->ping == -1)
	{
		Com_sprintf(string, 1024, " ^2connecting^7      %s", ci->name);
		trap_R_DrawTextNorm(string, (x + 34) * NORM_HSCALE, (y + 2) * NORM_VSCALE, FONT_UI, (mHeight) * NORM_VSCALE, colorWhite, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW);
	}
	else
	{
		int pingColor = 1;
		if (score->ping < 40)
		{
			pingColor = 7;
		}
		else if (score->ping < 70)
		{
			pingColor = 2;
		}
		else if (score->ping < 100)
		{
			pingColor = 3;
		}
		else if (score->ping < 200)
		{
			pingColor = 8;
		}
		else if (score->ping < 400)
		{
			pingColor = 6;
		}
		Com_sprintf(string, 1024, "%3i", score->score);
		trap_R_DrawTextNorm(string, (x + 80) * NORM_HSCALE, (y) * NORM_VSCALE, FONT_DISPLAY, (bHeight) * NORM_VSCALE, color, TEXT_ALIGN_RIGHT, TEXT_DROPSHADOW);
		if (cgs.gametype == GT_TDM)
		{
			if (CG_ModernIsGameTypeFreeze())
			{
				Com_sprintf(string, 1024, "%i", score->scoreFlags);
			}
			else
			{
				Com_sprintf(string, 1024, "^%i%3i", score->scoreFlags < 0 ? 3 : 7, score->scoreFlags);
			}
			trap_R_DrawTextNorm(string, (x + 112) * NORM_HSCALE, (y + 4) * NORM_VSCALE, FONT_UI, (mHeight) * NORM_VSCALE, color, TEXT_ALIGN_RIGHT, TEXT_DROPSHADOW);
		}
		else
		{
			trap_R_DrawTextNorm("0", (x + 112) * NORM_HSCALE, (y + 4) * NORM_VSCALE, FONT_UI, (mHeight) * NORM_VSCALE, color, TEXT_ALIGN_RIGHT, TEXT_DROPSHADOW);
		}
		Com_sprintf(string, 1024, "^%i%3i", pingColor, score->ping);
		trap_R_DrawTextNorm(string, (x + 152) * NORM_HSCALE, (y) * NORM_VSCALE, FONT_DISPLAY, (bHeight) * NORM_VSCALE, color, TEXT_ALIGN_RIGHT, TEXT_DROPSHADOW);

		Com_sprintf(string, 1024, "%i", score->time);
		trap_R_DrawTextNorm(string, (x + 184) * NORM_HSCALE, (y) * NORM_VSCALE, FONT_DISPLAY, (bHeight) * NORM_VSCALE, color, TEXT_ALIGN_RIGHT, TEXT_DROPSHADOW);

		Com_sprintf(string, 1024, "%s", ci->name);
		trap_R_DrawTextNorm(string, (x + 202) * NORM_HSCALE, (y + 4) * NORM_VSCALE, FONT_UI, (mHeight) * NORM_VSCALE, color, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW);

	}
	if (!cg.warmup && cg.predictedPlayerState.pm_type != PM_INTERMISSION)
	{
		return;
	}
	// STAT_CLIENTS_READY not available yet in q3now
	return;
}

static void CG_ModernDrawClientScoreNew(int x, int y, const score_t *score, const float *color, float fade)
{
	// cg_scoreboardBE.integer is 0, use standard path
	CG_ModernDrawClientScore(x, y, score, color, fade);
}


int CG_ModernDrawTeamScores(int x, int y, int team, float fade, int maxScores)
{
	int i;
	int maxClients;
	int scoresPrinted = 0;
	clientInfo_t *ci;
	score_t *score;
	vec4_t color = { 1.0f, 1.0f, 1.0f, 1.0f };
	const vec4_t readyColor = { 0.5f, 0.5f, 0.5f, 0.5f };
	color[3] = fade;

	maxClients = cg.numScores;
	for (i = 0, scoresPrinted = 0; i < maxClients && scoresPrinted < maxScores; ++i)
	{
		score = &cg.scores[i];
		ci = &cgs.clientinfo[score->client];

		// In demo mode, if clientinfo is not valid, skip this entry
		if (cg.demoPlayback && !ci->infoValid)
		{
			continue;
		}

		if (ci->team != team)
		{
			continue;
		}

		if (team == TEAM_RED)
		{
			sumPingRed += score->ping;
			sumScoresRed += score->score;
			sumThawsRed += score->scoreFlags;
		}
		else if (team == TEAM_BLUE)
		{
			sumPingBlue += score->ping;
			sumScoresBlue += score->score;
			sumThawsBlue += score->scoreFlags;
		}

		if (team == TEAM_SPECTATOR)
		{
			int tmp = scoresPrinted / 2;
			if (scoresPrinted % 2 == 0)
			{
				CG_ModernDrawClientScoreNew(x, y + 18 * tmp - 18, score, color, fade);
			}
			else
			{
				CG_ModernDrawClientScoreNew(x + 320, y + 18 * tmp - 18, score, color, fade);
			}
			++scoresPrinted;
		}
		else
		{
			if (ci->team == team)
			{
				CG_ModernDrawClientScoreNew(x, y + 18 * scoresPrinted++, score, color, fade);
			}
		}
	}

	return scoresPrinted;
}


qboolean CG_ModernDrawScoretable(void)
{
	vec4_t *color;
	vec4_t colorRect;
	int drewRed;
	int drewBlue;
	int drewSpect;
	int y;
	sumScoresBlue = 0;
	sumScoresRed = 0;
	sumPingBlue = 0;
	sumPingRed = 0;
	sumThawsBlue = 0;
	sumThawsRed = 0;

	if (cg_paused.integer)
	{
		return qfalse;
	}
	if (cg.warmup && !cg.showScores && cg.predictedPlayerState.pm_type != PM_INTERMISSION)
	{
		return qfalse;
	}
	if (!cg.showScores && cg.predictedPlayerState.pm_type != PM_DEAD && cg.predictedPlayerState.pm_type != PM_INTERMISSION)
	{
		color = (vec4_t *)CG_FadeColor(cg.scoreFadeTime, 0xc8);
	}
	else
	{
		color = &colorWhite;
	}

	if (color == NULL)
	{
		cg.killerName[0] = 0;
		return qfalse;
	}
	y = 40;
	if (cg.demoPlayback)
	{
		trap_R_DrawTextNorm("^3Demo Playback", (640 / 2) * NORM_HSCALE, (y) * NORM_VSCALE, FONT_DISPLAY, (16) * NORM_VSCALE, CG_ColorFromAlpha((*color)[0]), TEXT_ALIGN_CENTER, TEXT_DROPSHADOW);
	}
	else if (cg.killerName[0])
	{
		trap_R_DrawTextNorm(va("Fragged by %s", cg.killerName), (640 / 2) * NORM_HSCALE, (y) * NORM_VSCALE, FONT_DISPLAY, (16) * NORM_VSCALE, CG_ColorFromAlpha((*color)[0]), TEXT_ALIGN_CENTER, TEXT_DROPSHADOW);
	}


	y = 64;

	CG_ScoreboardAdjustTeamColor(scoreboard_rtColor, colorRect);
	CG_FillRectNorm((8.0f) * NORM_HSCALE, ((float)y) * NORM_VSCALE, (304.0f) * NORM_HSCALE, (48.0f) * NORM_VSCALE, colorRect);

	CG_ScoreboardAdjustTeamColor(scoreboard_btColor, colorRect);
	CG_FillRectNorm((328.0f) * NORM_HSCALE, ((float)y) * NORM_VSCALE, (304.0f) * NORM_HSCALE, (48.0f) * NORM_VSCALE, colorRect);

	CG_ScoreboardDrawField(8, y, cg.teamScores[0]);
	trap_R_SetColor(NULL);
	CG_ScoreboardDrawField(328, y, cg.teamScores[1]);
	trap_R_SetColor(NULL);
	y = 116;

	{
		const char *tmpStr;
		const char *tmpArgStr;
		if (cgs.gametype == GT_TDM)
		{
			if (CG_ModernIsGameTypeFreeze())
			{
				tmpArgStr = "THW";
			}
			else
			{
				tmpArgStr = "NET";
			}
		}
		else
		{
			tmpArgStr = "PL ";
		}
		tmpStr = va("^1Score %s Ping Min  Name", tmpArgStr);
		trap_R_DrawTextNorm(tmpStr, (40) * NORM_HSCALE, (y) * NORM_VSCALE, FONT_UI, (12) * NORM_VSCALE, colorWhite, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW);
	}

	{
		const char *tmpStr;
		const char *tmpArgStr;
		if (cgs.gametype == GT_TDM)
		{
			if (CG_ModernIsGameTypeFreeze())
			{
				tmpArgStr = "THW";
			}
			else
			{
				tmpArgStr = "NET";
			}
		}
		else
		{
			tmpArgStr = "PL ";
		}
		tmpStr = va("^4Score %s Ping Min  Name", tmpArgStr);
		trap_R_DrawTextNorm(tmpStr, (360) * NORM_HSCALE, (y) * NORM_VSCALE, FONT_UI, (12) * NORM_VSCALE, colorWhite, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW);
	}

	y = 140;
	drewRed = CG_ModernDrawTeamScores(0, y, TEAM_RED, (*color)[0], 12);
	drewBlue = CG_ModernDrawTeamScores(320, y, TEAM_BLUE, (*color)[0], 12);

	if (drewRed)
	{
		char string[128];
		const char *tmpStr;
		if (cgs.gametype >= GT_CTF)
		{
			tmpStr = va("^1Points  Players  AvgPing");
			trap_R_DrawTextNorm(tmpStr, (116) * NORM_HSCALE, (64) * NORM_VSCALE, FONT_DISPLAY, (16) * NORM_VSCALE, colorWhite, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW);
			Com_sprintf(string, 128, "^3%3i^7  %2i  %3i", sumScoresRed, drewRed, sumPingRed / drewRed);
			trap_R_DrawTextNorm(string, (116) * NORM_HSCALE, (80) * NORM_VSCALE, FONT_DISPLAY, (20) * NORM_VSCALE, colorWhite, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW);
		}
		else
		{
			if (cgs.gametype == GT_TDM && !CG_ModernIsGameTypeFreeze())
			{
				tmpStr = va("^1Players  AvgPing");
				trap_R_DrawTextNorm(tmpStr, (104) * NORM_HSCALE, (64) * NORM_VSCALE, FONT_DISPLAY, (16) * NORM_VSCALE, colorWhite, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW);
				Com_sprintf(string, 128, " %2i  %3i", drewRed, sumPingRed / drewRed);
				trap_R_DrawTextNorm(string, (88) * NORM_HSCALE, (80) * NORM_VSCALE, FONT_DISPLAY, (20) * NORM_VSCALE, colorWhite, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW);
			}
			else if (CG_ModernIsGameTypeFreeze())
			{
				tmpStr = va("^1Scores   Thaws Players");
				trap_R_DrawTextNorm(tmpStr, (80) * NORM_HSCALE, (64) * NORM_VSCALE, FONT_DISPLAY, (16) * NORM_VSCALE, colorWhite, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW);
				Com_sprintf(string, 128, " %3i %3i  %2i", sumScoresRed, sumThawsRed, drewRed);
				trap_R_DrawTextNorm(string, (64) * NORM_HSCALE, (80) * NORM_VSCALE, FONT_DISPLAY, (20) * NORM_VSCALE, colorWhite, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW);
			}
		}
	}

	if (drewBlue)
	{
		char string[128];
		const char *tmpStr;
		if (cgs.gametype >= GT_CTF)
		{
			tmpStr = va("^4Points  Players  AvgPing");
			trap_R_DrawTextNorm(tmpStr, (436) * NORM_HSCALE, (64) * NORM_VSCALE, FONT_DISPLAY, (16) * NORM_VSCALE, colorWhite, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW);
			Com_sprintf(string, 128, "^3%3i^7  %2i  %3i", sumScoresBlue, drewBlue, sumPingBlue / drewBlue);
			trap_R_DrawTextNorm(string, (436) * NORM_HSCALE, (80) * NORM_VSCALE, FONT_DISPLAY, (20) * NORM_VSCALE, colorWhite, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW);
		}
		else if (cgs.gametype == GT_TDM && !CG_ModernIsGameTypeFreeze())
		{
			tmpStr = va("^4Players  AvgPing");
			trap_R_DrawTextNorm(tmpStr, (424) * NORM_HSCALE, (64) * NORM_VSCALE, FONT_DISPLAY, (16) * NORM_VSCALE, colorWhite, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW);
			Com_sprintf(string, 128, " %2i  %3i", drewBlue, sumPingBlue / drewBlue);
			trap_R_DrawTextNorm(string, (408) * NORM_HSCALE, (80) * NORM_VSCALE, FONT_DISPLAY, (20) * NORM_VSCALE, colorWhite, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW);
		}
		else if (CG_ModernIsGameTypeFreeze())
		{
			tmpStr = va("^4Scores   Thaws Players");
			trap_R_DrawTextNorm(tmpStr, (400) * NORM_HSCALE, (64) * NORM_VSCALE, FONT_DISPLAY, (16) * NORM_VSCALE, colorWhite, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW);
			Com_sprintf(string, 128, " %3i %3i  %2i", sumScoresBlue, sumThawsBlue, drewBlue);
			trap_R_DrawTextNorm(string, (384) * NORM_HSCALE, (80) * NORM_VSCALE, FONT_DISPLAY, (20) * NORM_VSCALE, colorWhite, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW);
		}
	}




	{
		int max;

		max = drewRed < drewBlue ? drewBlue : drewRed;
		y += 18 * max + 18;

		drewRed  = CG_ModernDrawTeamScores(0, y, TEAM_6, (*color)[0], 8);
		drewBlue = CG_ModernDrawTeamScores(320, y, TEAM_7, (*color)[0], 8);
	}

	if (drewRed != 0)
	{
		trap_R_DrawTextNorm("^1Red Team Spectator", (60) * NORM_HSCALE, (y - 14) * NORM_VSCALE, FONT_UI, (12) * NORM_VSCALE, colorWhite, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW);
	}
	if (drewBlue != 0)
	{
		trap_R_DrawTextNorm("^4Blue Team Spectator", (380) * NORM_HSCALE, (y - 14) * NORM_VSCALE, FONT_UI, (12) * NORM_VSCALE, colorWhite, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW);
	}

	{
		int max;
		vec4_t bgColor;

		max = drewRed < drewBlue ? drewBlue : drewRed;

		y = y + 18 * max + 36;

		CG_ScoreboardAdjustTeamColor(scoreboard_rtColor, bgColor);
		CG_FillRectNorm((8.0f) * NORM_HSCALE, (112.0f) * NORM_VSCALE, (304.0f) * NORM_HSCALE, ((float)y - 148) * NORM_VSCALE, bgColor);

		CG_ScoreboardAdjustTeamColor(scoreboard_btColor, bgColor);
		CG_FillRectNorm((328.0f) * NORM_HSCALE, (112.0f) * NORM_VSCALE, (304.0f) * NORM_HSCALE, ((float)y - 148) * NORM_VSCALE, bgColor);

		drewSpect = CG_ModernDrawTeamScores(0, y, TEAM_SPECTATOR, (*color)[0], 24);

		if (drewSpect)
		{
			trap_R_DrawTextNorm("Spectator", (640 / 2.0f) * NORM_HSCALE, (y - 32) * NORM_VSCALE, FONT_UI, (12) * NORM_VSCALE, colorWhite, TEXT_ALIGN_CENTER, TEXT_DROPSHADOW);

			if ((customScoreboardColorIsSet_spec & 1) == 0)
			{
				bgColor[0] = bgColor[1] = bgColor[2] = 0.5f;
			}
			else
			{
				Vector4Copy(scoreboard_specColor, bgColor);
			}
			bgColor[3] = 0.15f;

			CG_FillRectNorm((8.0f) * NORM_HSCALE, ((float)(y - 34)) * NORM_VSCALE, (624.0f) * NORM_HSCALE, ((float)(9 * drewSpect + 29)) * NORM_VSCALE, bgColor);
		}
	}

	return qtrue;
}

void SetScoreboardColors(vec4_t *rtColorHeader, vec4_t *rtColorBody, vec4_t *btColorHeader, vec4_t *btColorBody)
{
	// Fix bitwise check for red
	if ((customScoreboardColorIsSet_red & 1) == 0)
	{
		Vector4Copy(scoreboard_rtColor, *rtColorHeader);
		Vector4Copy(scoreboard_rtColor, *rtColorBody);
	}
	else
	{
		Vector4Copy(scoreboard_rtColorHeader, *rtColorHeader);
		Vector4Copy(scoreboard_rtColorBody, *rtColorBody);
	}

	// Fix bitwise check for blue
	if ((customScoreboardColorIsSet_blue & 1) == 0)
	{
		Vector4Copy(scoreboard_btColor, *btColorHeader);
		Vector4Copy(scoreboard_btColor, *btColorBody);
	}
	else
	{
		Vector4Copy(scoreboard_btColorHeader, *btColorHeader);
		Vector4Copy(scoreboard_btColorBody, *btColorBody);
	}
}

// Global layout variables for BE team scoretable
static int sb_proportional = 0;
static float sb_bWidth = 16, sb_bHeight = 16;
static float sb_bWidth2 = 16, sb_bHeight2 = 20;
static float sb_mWidth = 8, sb_mHeight = 12;
static float sb_mWidth2 = 8, sb_mHeight2 = 16;
static float sb_leftX = 40, sb_rightX = 360;
static float sb_pos1X = 40;
static float sb_pos2X = 72;
static float sb_pos3X = 112;
static float sb_pos4X = 144;
static float sb_pos5X = 162;
static float sb_titlePos1X = 48;
static float sb_titlePos2X = 112;
static float sb_titlePos3X = 176;
static float sb_row1Y = 64;
static float sb_row2Y = 80;

static void CG_ModernDrawTeamSummary(
    float baseX,
    int drewPlayers,
    int sumScores,
    int sumThaws,
    int sumPing,
    const vec4_t titleColor)
{
	const char *labels[3];
	char values[3][128];
	float posX[3];
	int i, count = 0;
	vec4_t curColor;

	if (cgs.gametype >= GT_CTF)
	{
		labels[count] = "Points";
		Com_sprintf(values[count], sizeof(values[count]), "%i^7", sumScores);
		posX[count] = sb_titlePos1X;
		count++;

		labels[count] = "Players";
		Com_sprintf(values[count], sizeof(values[count]), "%i", drewPlayers);
		posX[count] = sb_titlePos2X;
		count++;

		labels[count] = "AvgPing";
		Com_sprintf(values[count], sizeof(values[count]), "%i", sumPing / drewPlayers);
		posX[count] = sb_titlePos3X;
		count++;
	}
	else if (cgs.gametype == GT_TDM && !CG_ModernIsGameTypeFreeze())
	{
		labels[count] = "Players";
		Com_sprintf(values[count], sizeof(values[count]), "%i", drewPlayers);
		posX[count] = sb_titlePos2X;
		count++;

		labels[count] = "AvgPing";
		Com_sprintf(values[count], sizeof(values[count]), "%i", sumPing / drewPlayers);
		posX[count] = sb_titlePos3X;
		count++;
	}
	else if (CG_ModernIsGameTypeFreeze())
	{
		labels[count] = "Scores";
		Com_sprintf(values[count], sizeof(values[count]), "%i", sumScores);
		posX[count] = sb_titlePos1X;
		count++;

		labels[count] = "Thaws";
		Com_sprintf(values[count], sizeof(values[count]), "%i", sumThaws);
		posX[count] = sb_titlePos2X;
		count++;

		labels[count] = "Players";
		Com_sprintf(values[count], sizeof(values[count]), "%i", drewPlayers);
		posX[count] = sb_titlePos3X;
		count++;
	}

	for (i = 0; i < count; i++)
	{
		Vector4Copy(titleColor, curColor);

		trap_R_DrawTextNorm(labels[i], (baseX + posX[i]) * NORM_HSCALE, (sb_row1Y) * NORM_VSCALE, FONT_DISPLAY, (sb_mHeight2) * NORM_VSCALE, curColor, TEXT_ALIGN_RIGHT, TEXT_DROPSHADOW);

		trap_R_DrawTextNorm(values[i], (baseX + posX[i]) * NORM_HSCALE, (sb_row2Y) * NORM_VSCALE, FONT_DISPLAY, (sb_bHeight2) * NORM_VSCALE, colorWhite, TEXT_ALIGN_RIGHT, TEXT_DROPSHADOW);
	}
}


static void CG_ModernDrawScoreHeader(float baseX, float y, vec4_t colorBody, vec4_t shadowColor, int mWidth_p, int mHeight_p, int screenWidth, int proportional_p)
{
	const char *label1 = "Score";
	const char *label2 = (cgs.gametype == GT_TDM)
	                     ? (CG_ModernIsGameTypeFreeze() ? "THW" : "NET")
	                     : "PL";
	const char *label3 = "Ping";
	const char *label4 = "Min";
	const char *label5 = "Name";

	vec4_t headerColor1;

	Vector4Copy(colorBody, headerColor1);

	trap_R_DrawTextNorm(label1, (baseX + sb_pos1X) * NORM_HSCALE, (y) * NORM_VSCALE, FONT_UI, (mHeight_p) * NORM_VSCALE, headerColor1, TEXT_ALIGN_RIGHT, TEXT_DROPSHADOW);
	trap_R_DrawTextNorm(label2, (baseX + sb_pos2X) * NORM_HSCALE, (y) * NORM_VSCALE, FONT_UI, (mHeight_p) * NORM_VSCALE, colorWhite, TEXT_ALIGN_RIGHT, TEXT_DROPSHADOW);
	trap_R_DrawTextNorm(label3, (baseX + sb_pos3X) * NORM_HSCALE, (y) * NORM_VSCALE, FONT_UI, (mHeight_p) * NORM_VSCALE, colorWhite, TEXT_ALIGN_RIGHT, TEXT_DROPSHADOW);
	trap_R_DrawTextNorm(label4, (baseX + sb_pos4X) * NORM_HSCALE, (y) * NORM_VSCALE, FONT_UI, (mHeight_p) * NORM_VSCALE, colorWhite, TEXT_ALIGN_RIGHT, TEXT_DROPSHADOW);
	trap_R_DrawTextNorm(label5, (baseX + sb_pos5X) * NORM_HSCALE, (y) * NORM_VSCALE, FONT_UI, (mHeight_p) * NORM_VSCALE, colorWhite, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW);
}


qboolean CG_BEDrawTeamScoretable(void)
{
	vec4_t *color;
	vec4_t colorRect;
	vec4_t rtColorHeader, rtColorBody;
	vec4_t btColorHeader, btColorBody;
	int drewRed;
	int drewBlue;
	int drewSpect;
	int y;
	sumScoresBlue = 0;
	sumScoresRed = 0;
	sumPingBlue = 0;
	sumPingRed = 0;
	sumThawsBlue = 0;
	sumThawsRed = 0;

	if (cg_paused.integer)
	{
		return qfalse;
	}

	if (cg.warmup && !cg.showScores && cg.predictedPlayerState.pm_type != PM_INTERMISSION)
	{
		return qfalse;
	}

	if (!cg.showScores && cg.predictedPlayerState.pm_type != PM_DEAD && cg.predictedPlayerState.pm_type != PM_INTERMISSION)
	{
		color = (vec4_t *)CG_FadeColor(cg.scoreFadeTime, 0xc8);
	}
	else
	{
		color = &colorWhite;
	}

	if (color == NULL)
	{
		cg.killerName[0] = 0;
		return qfalse;
	}

	y = 40;

	sb_proportional = 0;

	if (cg.demoPlayback)
	{
		trap_R_DrawTextNorm("^3Demo Playback", (640 / 2.0f) * NORM_HSCALE, (y) * NORM_VSCALE, FONT_DISPLAY, (sb_bHeight) * NORM_VSCALE, *color, TEXT_ALIGN_CENTER, TEXT_DROPSHADOW);
	}
	else if (cg.killerName[0])
	{
		trap_R_DrawTextNorm(va("Fragged by %s", cg.killerName), (640 / 2.0f) * NORM_HSCALE, (y) * NORM_VSCALE, FONT_DISPLAY, (sb_bHeight) * NORM_VSCALE, *color, TEXT_ALIGN_CENTER, TEXT_DROPSHADOW);
	}

	SetScoreboardColors(&rtColorHeader, &rtColorBody, &btColorHeader, &btColorBody);

	// Header background
	y = 64;

	CG_ScoreboardAdjustTeamColor(rtColorHeader, colorRect);
	colorRect[3] *= 1.5;
	CG_FillRectNorm((8.0f) * NORM_HSCALE, ((float)y) * NORM_VSCALE, (304.0f) * NORM_HSCALE, (48.0f) * NORM_VSCALE, colorRect);

	CG_ScoreboardAdjustTeamColor(btColorHeader, colorRect);
	colorRect[3] *= 1.5;
	CG_FillRectNorm((328.0f) * NORM_HSCALE, ((float)y) * NORM_VSCALE, (304.0f) * NORM_HSCALE, (48.0f) * NORM_VSCALE, colorRect);

	// main team scores -- use large text style (cg_scoreboardBE & 2 would use CG_ModernDrawField, but we use 0)
	trap_R_DrawTextNorm(va("%d", cg.teamScores[0]), (sb_leftX - 32) * NORM_HSCALE, (87) * NORM_VSCALE, FONT_DISPLAY, (60) * NORM_VSCALE, colorWhite, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW);
	trap_R_DrawTextNorm(va("%d", cg.teamScores[1]), (sb_rightX - 32) * NORM_HSCALE, (87) * NORM_VSCALE, FONT_DISPLAY, (60) * NORM_VSCALE, colorWhite, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW);

	y = 116;
	// Header text
	CG_ModernDrawScoreHeader(sb_leftX, y, (customScoreboardColorIsSet_red & 2) ? scoreboard_rtColorTitle : rtColorBody, colorBlack, sb_mWidth, sb_mHeight, 640, sb_proportional);
	CG_ModernDrawScoreHeader(sb_rightX, y, (customScoreboardColorIsSet_blue & 2) ? scoreboard_btColorTitle : btColorBody, colorBlack, sb_mWidth, sb_mHeight, 640, sb_proportional);

	y = 140;
	// Team score lines
	drewRed = CG_ModernDrawTeamScores(0, y, TEAM_RED, (*color)[0], 32);
	drewBlue = CG_ModernDrawTeamScores(320, y, TEAM_BLUE, (*color)[0], 32);

	if (drewRed)
	{
		float baseX = (cgs.gametype >= GT_CTF) ? (sb_leftX + 76) :
		              (cgs.gametype == GT_TDM && !CG_ModernIsGameTypeFreeze()) ? (sb_leftX + 64) :
		              (CG_ModernIsGameTypeFreeze()) ? (sb_leftX + 40) : sb_leftX;
		CG_ModernDrawTeamSummary(baseX, drewRed, sumScoresRed, sumThawsRed, sumPingRed, (customScoreboardColorIsSet_red & 2) ? scoreboard_rtColorTitle : rtColorHeader);
	}

	if (drewBlue)
	{
		float baseX = (cgs.gametype >= GT_CTF) ? (sb_rightX + 76) :
		              (cgs.gametype == GT_TDM && !CG_ModernIsGameTypeFreeze()) ? (sb_rightX + 64) :
		              (CG_ModernIsGameTypeFreeze()) ? (sb_rightX + 40) : sb_rightX;
		CG_ModernDrawTeamSummary(baseX, drewBlue, sumScoresBlue, sumThawsBlue, sumPingBlue, (customScoreboardColorIsSet_blue & 2) ? scoreboard_btColorTitle : btColorHeader);
	}

	{
		int max = (drewRed < drewBlue) ? drewBlue : drewRed;
		y += 18 * max + 18;

		drewRed = CG_ModernDrawTeamScores(0, y, TEAM_6, (*color)[0], 32);
		drewBlue = CG_ModernDrawTeamScores(320, y, TEAM_7, (*color)[0], 32);
	}

	if (drewRed || drewBlue)
	{
		if (drewRed)
		{
			trap_R_DrawTextNorm("Red", (sb_leftX + 20) * NORM_HSCALE, (y - 14) * NORM_VSCALE, FONT_UI, (sb_mHeight) * NORM_VSCALE, rtColorHeader, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW);
			trap_R_DrawTextNorm("Team", (sb_leftX + 60) * NORM_HSCALE, (y - 14) * NORM_VSCALE, FONT_UI, (sb_mHeight) * NORM_VSCALE, colorWhite, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW);
			trap_R_DrawTextNorm("Spectator", (sb_leftX + 100) * NORM_HSCALE, (y - 14) * NORM_VSCALE, FONT_UI, (sb_mHeight) * NORM_VSCALE, colorWhite, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW);
		}

		if (drewBlue)
		{
			trap_R_DrawTextNorm("Blue", (sb_rightX + 20) * NORM_HSCALE, (y - 14) * NORM_VSCALE, FONT_UI, (sb_mHeight) * NORM_VSCALE, btColorHeader, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW);
			trap_R_DrawTextNorm("Team", (sb_rightX + 60) * NORM_HSCALE, (y - 14) * NORM_VSCALE, FONT_UI, (sb_mHeight) * NORM_VSCALE, colorWhite, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW);
			trap_R_DrawTextNorm("Spectator", (sb_rightX + 100) * NORM_HSCALE, (y - 14) * NORM_VSCALE, FONT_UI, (sb_mHeight) * NORM_VSCALE, colorWhite, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW);
		}
	}

	// BODY
	{
		int max = (drewRed > drewBlue) ? drewRed : drewBlue;
		vec4_t bgColor;

		y += 18 * max + 36;

		CG_ScoreboardAdjustTeamColor(rtColorBody, bgColor);
		CG_FillRectNorm((8.0f) * NORM_HSCALE, (112.0f) * NORM_VSCALE, (304.0f) * NORM_HSCALE, ((float)(y - 148)) * NORM_VSCALE, bgColor);

		CG_ScoreboardAdjustTeamColor(btColorBody, bgColor);
		CG_FillRectNorm((328.0f) * NORM_HSCALE, (112.0f) * NORM_VSCALE, (304.0f) * NORM_HSCALE, ((float)(y - 148)) * NORM_VSCALE, bgColor);

		drewSpect = CG_ModernDrawTeamScores(0, y, TEAM_SPECTATOR, (*color)[0], 24);

		if (drewSpect)
		{
			trap_R_DrawTextNorm("Spectator", (640 / 2.0f) * NORM_HSCALE, (y - 32) * NORM_VSCALE, FONT_UI, (12) * NORM_VSCALE, (customScoreboardColorIsSet_spec & 2) ? scoreboard_specColorTitle : colorWhite, TEXT_ALIGN_CENTER, TEXT_DROPSHADOW);

			if ((customScoreboardColorIsSet_spec & 1) == 0)
			{
				bgColor[0] = bgColor[1] = bgColor[2] = 0.5f;
			}
			else
			{
				Vector4Copy(scoreboard_specColor, bgColor);
			}
			bgColor[3] = 0.15f;

			CG_FillRectNorm((8.0f) * NORM_HSCALE, ((float)(y - 34)) * NORM_VSCALE, (624.0f) * NORM_HSCALE, ((float)(9 * drewSpect + 29)) * NORM_VSCALE, bgColor);
		}
	}

	return qtrue;
}
