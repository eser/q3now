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
	CG_FontSelect(0);
	CG_ModernDrawString(x + 8, y + 8, num, colorWhite, 32, 40, SCREEN_WIDTH, DS_HLEFT | DS_SHADOW, NULL);
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
	int font = 2; // sansman default

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
					CG_DrawPic(iconx, y - (32 - BIGCHAR_HEIGHT) / 2, 32, 32, cgs.media.botSkillShaders[ ci->botSkill - 1 ]);
				}
				else
				{
					CG_DrawPic(iconx, y, 16, 16, cgs.media.botSkillShaders[ ci->botSkill - 1 ]);
				}
			}
		}

		// draw the wins / losses
		if (cgs.gametype == GT_TOURNAMENT)
		{
			int score_x = iconx;

			Com_sprintf(string, sizeof(string), "%i/%i", ci->wins, ci->losses);
			CG_DrawSmallString(score_x, y, string, color, DS_HLEFT, font);
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
		        || cgs.gametype >= GT_TEAM)
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
		CG_FillRect(SB_SCORELINE_X + BIGCHAR_WIDTH + (SB_RATING_WIDTH / 2.0), y,
		            640 - SB_SCORELINE_X - BIGCHAR_WIDTH, BIGCHAR_HEIGHT + 1, hcolor);
	}

	CG_FontSelect(0);
	CG_ModernDrawString(128, y, string, color, 16, 16, SCREEN_WIDTH, DS_HLEFT | DS_SHADOW, NULL);

	CG_FontSelect(2);
	CG_ModernDrawString(128 + 16 * 16, y, ci->name, color, 16, 16, 256, DS_HLEFT | DS_SHADOW | DS_PROPORTIONAL, NULL);

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
	int font = 2; // sansman default
	int proportional = 0; // cg_scoreboardBE.integer is 0

	if (score->client < 0 || score->client >= cgs.maxclients)
	{
		Com_Printf("Bad score->client: %i\n", score->client);
		return;
	}
	ci = &cgs.clientinfo[score->client];

	iconx = SB_BOTICON_X + BIGCHAR_WIDTH;
	headx = SB_HEAD_X + BIGCHAR_WIDTH;

	CG_FontSelect(font);

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
					CG_DrawPic(iconx, y - (32 - bHeight) / 2, 32, 32, cgs.media.botSkillShaders[ci->botSkill - 1]);
				}
				else
				{
					CG_DrawPic(iconx, y, 16, 16, cgs.media.botSkillShaders[ci->botSkill - 1]);
				}
			}
		}

		if (cgs.gametype == GT_TOURNAMENT)
		{
			int score_x = iconx;

			Com_sprintf(string, sizeof(string), "%i/%i", ci->wins, ci->losses);
			CG_ModernDrawStringNew(score_x, y, string, color, colorBlack, bWidth, bHeight, SCREEN_WIDTH, DS_SHADOW | proportional, NULL, NULL, NULL);
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
		if (cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR || cgs.gametype >= GT_TEAM)
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
		CG_FillRect(SB_SCORELINE_X + BIGCHAR_WIDTH + (SB_RATING_WIDTH / 2.0), y,
		            640 - SB_SCORELINE_X - bWidth, bHeight + 1, hcolor);
	}

	CG_ModernDrawStringNew(206, y, string, color, colorBlack, bWidth, bHeight, SCREEN_WIDTH,
	                    DS_HRIGHT | DS_SHADOW | proportional, NULL, NULL, NULL);
	CG_ModernDrawStringNew(286, y, string2, color, colorBlack, bWidth, bHeight, SCREEN_WIDTH,
	                    DS_HRIGHT | DS_SHADOW | proportional, NULL, NULL, NULL);
	CG_ModernDrawStringNew(366, y, string3, color, colorBlack, bWidth, bHeight, SCREEN_WIDTH,
	                    DS_HRIGHT | DS_SHADOW | proportional, NULL, NULL, NULL);

	CG_ModernDrawStringNew(128 + 4 + 16 * 16, y, ci->name, color, colorBlack, bWidth, bHeight, 256,
	                    DS_SHADOW | proportional, NULL, NULL, NULL);


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
	int font = 2; // sansman default

	// don't draw amuthing if the menu or console is up
	if (cg_paused.integer)
	{
		return qfalse;
	}

	if (cgs.gametype == GT_SINGLE_PLAYER && cg.predictedPlayerState.pm_type == PM_INTERMISSION)
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
		CG_DrawBigString(SCREEN_WIDTH / 2, 40, "^3Demo Playback", CG_ColorFromAlpha(fade), DS_HCENTER | DS_PROPORTIONAL | DS_SHADOW, font);
	}
	// fragged by ... line
	else if (cg.killerName[0])
	{
		s = va("Fragged by %s", cg.killerName);
		CG_DrawBigString(SCREEN_WIDTH / 2, 40, s, CG_ColorFromAlpha(fade), DS_HCENTER | DS_PROPORTIONAL | DS_SHADOW, font);
	}

	// current rank
	if (cgs.gametype < GT_TEAM)
	{
		if (cg.snap->ps.persistant[PERS_TEAM] != TEAM_SPECTATOR)
		{
			s = va("%s place with %i",
			       CG_PlaceString(cg.snap->ps.persistant[PERS_RANK] + 1),
			       cg.snap->ps.persistant[PERS_SCORE]);
			CG_DrawBigString(SCREEN_WIDTH / 2, 60, s, CG_ColorFromAlpha(fade), DS_HCENTER | DS_PROPORTIONAL | DS_SHADOW, font);
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

		CG_DrawBigString(SCREEN_WIDTH / 2, 60, s, CG_ColorFromAlpha(fade), DS_HCENTER | DS_PROPORTIONAL | DS_SHADOW, font);
	}

	// scoreboard
	y = SB_HEADER;

	CG_DrawPic(SB_SCORE_X + (SB_RATING_WIDTH / 2.0), y, 64, 32, cgs.media.scoreboardScore);
	CG_DrawPic(SB_PING_X - (SB_RATING_WIDTH / 2.0), y, 64, 32, cgs.media.scoreboardPing);
	CG_DrawPic(SB_TIME_X - (SB_RATING_WIDTH / 2.0), y, 64, 32, cgs.media.scoreboardTime);
	CG_DrawPic(SB_NAME_X - (SB_RATING_WIDTH / 2.0), y, 64, 32, cgs.media.scoreboardName);

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

	if (cgs.gametype >= GT_TEAM)
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
	if (cgs.gametype == GT_SINGLE_PLAYER && cg.predictedPlayerState.pm_type == PM_INTERMISSION)
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
		CG_FillRect(panelX, 34, panelW, panelH, bgColor);
	}

	/* Header: title */
	CG_FontSelect(2);
	y = 40;
	if (cg.demoPlayback)
	{
		CG_ModernDrawString(SCREEN_WIDTH / 2.0f, y, "^3Demo Playback", *color, 14, 18, SCREEN_WIDTH, DS_HCENTER | DS_PROPORTIONAL | DS_SHADOW, NULL);
	}
	else if (cg.killerName[0])
	{
		s = va("Fragged by %s", cg.killerName);
		CG_ModernDrawString(SCREEN_WIDTH / 2.0f, y, s, *color, 14, 18, SCREEN_WIDTH, DS_HCENTER | DS_PROPORTIONAL | DS_SHADOW, NULL);
	}
	else
	{
		CG_ModernDrawString(SCREEN_WIDTH / 2.0f, y, "Free For All", *color, 14, 18, SCREEN_WIDTH, DS_HCENTER | DS_PROPORTIONAL | DS_SHADOW, NULL);
	}

	/* Header: placement */
	y = 62;
	if (cg.snap->ps.persistant[PERS_TEAM] != TEAM_SPECTATOR)
	{
		s = va("%s place with %i",
		       CG_PlaceString(cg.snap->ps.persistant[PERS_RANK] + 1),
		       cg.snap->ps.persistant[PERS_SCORE]);
		CG_ModernDrawString(SCREEN_WIDTH / 2.0f, y, s, *color, 10, 14, SCREEN_WIDTH, DS_HCENTER | DS_PROPORTIONAL | DS_SHADOW, NULL);
	}

	/* thin separator line below header */
	{
		vec4_t lineColor = {0.3f, 0.3f, 0.4f, 0.6f};
		CG_FillRect(panelX + 4, 80, panelW - 8, 1, lineColor);
	}

	/* Column headers */
	CG_FontSelect(0);
	hdrColor[0] = 0.7f; hdrColor[1] = 0.8f; hdrColor[2] = 0.9f; hdrColor[3] = (*color)[3];
	y = (int)hdrY;
	CG_ModernDrawString(colRank,  y, "#",     hdrColor, 8, 12, SCREEN_WIDTH, DS_HLEFT | DS_SHADOW, NULL);
	CG_ModernDrawString(colName,  y, "PLAYER",hdrColor, 8, 12, SCREEN_WIDTH, DS_HLEFT | DS_SHADOW, NULL);
	CG_ModernDrawString(colScore, y, "SCORE", hdrColor, 8, 12, SCREEN_WIDTH, DS_HRIGHT | DS_SHADOW, NULL);
	CG_ModernDrawString(colKD,    y, "K/D",   hdrColor, 8, 12, SCREEN_WIDTH, DS_HRIGHT | DS_SHADOW, NULL);
	CG_ModernDrawString(colDmg,   y, "DMG",   hdrColor, 8, 12, SCREEN_WIDTH, DS_HRIGHT | DS_SHADOW, NULL);
	CG_ModernDrawString(colWeap + 16, y, "WEAP", hdrColor, 8, 12, SCREEN_WIDTH, DS_HCENTER | DS_SHADOW, NULL);
	CG_ModernDrawString(colAcc,   y, "ACC%",  hdrColor, 8, 12, SCREEN_WIDTH, DS_HRIGHT | DS_SHADOW, NULL);
	CG_ModernDrawString(colPing,  y, "PING",  hdrColor, 8, 12, SCREEN_WIDTH, DS_HRIGHT | DS_SHADOW, NULL);

	/* separator below column header */
	{
		vec4_t lineColor = {0.3f, 0.3f, 0.4f, 0.4f};
		CG_FillRect(panelX + 4, hdrY + 13, panelW - 8, 1, lineColor);
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
			CG_FillRect(panelX + 2, (float)y - 1, panelW - 4, rowH, hlColor);
		}

		/* alternating row shading */
		if (rank % 2 == 0)
		{
			vec4_t altColor = {0.1f, 0.1f, 0.15f, 0.2f};
			CG_FillRect(panelX + 2, (float)y - 1, panelW - 4, rowH, altColor);
		}

		/* rank number */
		CG_FontSelect(0);
		Com_sprintf(tmpStr, sizeof(tmpStr), "%i", rank);
		CG_ModernDrawString(colRank + 10, y, tmpStr, *color, 10, 14, SCREEN_WIDTH, DS_HRIGHT | DS_SHADOW, NULL);

		/* head icon */
		if (score->client == cg.snap->ps.clientNum)
			headAngles[YAW] = (float)cg.time / 14 + 180;
		else
			headAngles[YAW] = 180;
		CG_DrawHead(colHead, y + 1, 14, 14, score->client, headAngles);

		/* player name (proportional, truncated) */
		CG_FontSelect(2);
		CG_ModernDrawString(colName, y + 1, ci->name, *color, 8, 13, 180, DS_HLEFT | DS_PROPORTIONAL | DS_SHADOW, NULL);

		/* score */
		CG_FontSelect(0);
		if (score->ping == -1)
		{
			CG_ModernDrawString(colScore, y, "^2...", *color, 10, 14, SCREEN_WIDTH, DS_HRIGHT | DS_SHADOW, NULL);
			y += (int)rowH;
			continue;
		}

		Com_sprintf(tmpStr, sizeof(tmpStr), "%i", score->score);
		CG_ModernDrawString(colScore, y, tmpStr, *color, 10, 14, SCREEN_WIDTH, DS_HRIGHT | DS_SHADOW, NULL);

		/* K/D */
		Com_sprintf(tmpStr, sizeof(tmpStr), "%i/%i", score->score, score->deaths);
		CG_ModernDrawString(colKD, y + 2, tmpStr, *color, 8, 12, SCREEN_WIDTH, DS_HRIGHT | DS_SHADOW, NULL);

		/* total damage */
		totalDmg = 0;
		for (j = ATT_NONE + 1; j < ATT_NUM_ATTACKS; j++) {
			totalDmg += cgs.attackStats[score->client][j].damage;
		}
		Com_sprintf(tmpStr, sizeof(tmpStr), "%i", totalDmg);
		CG_ModernDrawString(colDmg, y + 2, tmpStr, *color, 8, 12, SCREEN_WIDTH, DS_HRIGHT | DS_SHADOW, NULL);

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
			CG_DrawPic(colWeap + 8, y + 1, 14, 14, cg_weapons[bestWp].weaponIcon);
		}

		/* accuracy (colored) */
		acc = score->accuracy;
		if (acc > 50)
			Com_sprintf(tmpStr, sizeof(tmpStr), "^2%i%%", acc);
		else if (acc > 30)
			Com_sprintf(tmpStr, sizeof(tmpStr), "^3%i%%", acc);
		else
			Com_sprintf(tmpStr, sizeof(tmpStr), "^1%i%%", acc);
		CG_ModernDrawString(colAcc, y + 2, tmpStr, *color, 8, 12, SCREEN_WIDTH, DS_HRIGHT | DS_SHADOW, NULL);

		/* ping (colored) */
		if (score->ping < 40)
			pingColor = 2;
		else if (score->ping < 100)
			pingColor = 3;
		else
			pingColor = 1;
		Com_sprintf(tmpStr, sizeof(tmpStr), "^%i%i", pingColor, score->ping);
		CG_ModernDrawString(colPing, y + 2, tmpStr, *color, 8, 12, SCREEN_WIDTH, DS_HRIGHT | DS_SHADOW, NULL);

		y += (int)rowH;
	}

	/* separator before spectators */
	if (numSpecs > 0)
	{
		vec4_t lineColor = {0.3f, 0.3f, 0.4f, 0.4f};
		vec4_t specBg = {0.05f, 0.05f, 0.1f, 0.4f};
		int specY, specCount, si;

		y += 6;
		CG_FillRect(panelX + 4, (float)y, panelW - 8, 1, lineColor);
		y += 4;

		CG_FontSelect(2);
		CG_ModernDrawString(SCREEN_WIDTH / 2.0f, y, "Spectators", hdrColor, 8, 12, SCREEN_WIDTH, DS_HCENTER | DS_SHADOW, NULL);
		y += 14;

		specY = y;
		specCount = 0;
		CG_FontSelect(2);
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
			CG_ModernDrawString(sx, sy, ci->name, *color, 7, 10, 220, DS_HLEFT | DS_PROPORTIONAL | DS_SHADOW, NULL);
			specCount++;
		}
	}

	if (++cg.deferredPlayerLoading > 10)
		CG_LoadDeferredPlayers();

	return qtrue;
}

/*
=================
CG_ModernDrawTourneyScoreboard

CPMA-style tournament scoreboard with per-attack stats.
=================
*/

/* helper: draw one fighter's per-attack stats panel */
static void CG_DrawTourneyFighterPanel(int px, int py, int pw, score_t *sc, float fade)
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
	CG_FillRect((float)px, (float)py, (float)pw, 400.0f, panelBg);

	/* header area background */
	hdrBg[0] = 0.12f; hdrBg[1] = 0.12f; hdrBg[2] = 0.2f; hdrBg[3] = 0.5f;
	CG_FillRect((float)px, (float)py, (float)pw, 70.0f, hdrBg);

	/* head icon */
	VectorClear(headAngles);
	headAngles[YAW] = (sc->client == cg.snap->ps.clientNum) ? ((float)cg.time / 14 + 180) : 180;
	CG_DrawHead(px + 4, py + 4, 48, 48, sc->client, headAngles);

	/* player name */
	CG_FontSelect(2);
	textColor[0] = textColor[1] = textColor[2] = 1.0f; textColor[3] = fade;
	CG_ModernDrawString(px + 58, py + 6, ci->name, textColor, 12, 16, pw - 64, DS_HLEFT | DS_PROPORTIONAL | DS_SHADOW, NULL);

	/* big score */
	CG_FontSelect(0);
	Com_sprintf(tmpStr, sizeof(tmpStr), "%i", sc->score);
	CG_ModernDrawString(px + pw - 8, py + 2, tmpStr, textColor, 20, 28, SCREEN_WIDTH, DS_HRIGHT | DS_SHADOW, NULL);

	/* sub-header: acc, W/L, ping */
	CG_FontSelect(0);
	Com_sprintf(tmpStr, sizeof(tmpStr), "Acc: %i%%  W:%i L:%i  Ping: %ims",
	            sc->accuracy, ci->wins, ci->losses, sc->ping);
	CG_ModernDrawString(px + 58, py + 28, tmpStr, textColor, 6, 10, pw - 64, DS_HLEFT | DS_SHADOW, NULL);

	/* award badges */
	{
		int bx = px + 58;
		int by = py + 42;
		vec4_t awardColor;
		awardColor[0] = 1.0f; awardColor[1] = 0.85f; awardColor[2] = 0.0f; awardColor[3] = fade;
		CG_FontSelect(0);
		if (sc->excellentCount > 0)
		{
			Com_sprintf(tmpStr, sizeof(tmpStr), "^3Exc:%i", sc->excellentCount);
			CG_ModernDrawString(bx, by, tmpStr, awardColor, 6, 9, SCREEN_WIDTH, DS_HLEFT | DS_SHADOW, NULL);
			bx += 48;
		}
		if (sc->impressiveCount > 0)
		{
			Com_sprintf(tmpStr, sizeof(tmpStr), "^3Imp:%i", sc->impressiveCount);
			CG_ModernDrawString(bx, by, tmpStr, awardColor, 6, 9, SCREEN_WIDTH, DS_HLEFT | DS_SHADOW, NULL);
			bx += 48;
		}
		if (sc->guantletCount > 0)
		{
			Com_sprintf(tmpStr, sizeof(tmpStr), "^3Gnt:%i", sc->guantletCount);
			CG_ModernDrawString(bx, by, tmpStr, awardColor, 6, 9, SCREEN_WIDTH, DS_HLEFT | DS_SHADOW, NULL);
		}
	}

	/* separator */
	lineColor[0] = 0.4f; lineColor[1] = 0.4f; lineColor[2] = 0.5f; lineColor[3] = 0.5f;
	CG_FillRect((float)px + 4, (float)(py + 70), (float)(pw - 8), 1.0f, lineColor);

	/* weapon table column headers */
	wy = py + 76;
	{
		vec4_t colHdr;
		colHdr[0] = 0.6f; colHdr[1] = 0.7f; colHdr[2] = 0.8f; colHdr[3] = fade;
		CG_FontSelect(0);
		CG_ModernDrawString(cKD,   wy, "K/D",  colHdr, 6, 10, SCREEN_WIDTH, DS_HRIGHT | DS_SHADOW, NULL);
		CG_ModernDrawString(cEff,  wy, "EFF%", colHdr, 6, 10, SCREEN_WIDTH, DS_HRIGHT | DS_SHADOW, NULL);
		/* icon column - no header */
		CG_ModernDrawString(cHits, wy, "HITS/ATTS", colHdr, 6, 10, SCREEN_WIDTH, DS_HRIGHT | DS_SHADOW, NULL);
		CG_ModernDrawString(cAcc,  wy, "ACC%", colHdr, 6, 10, SCREEN_WIDTH, DS_HRIGHT | DS_SHADOW, NULL);
	}

	CG_FillRect((float)px + 4, (float)(wy + 11), (float)(pw - 8), 1.0f, lineColor);
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
			CG_FillRect((float)px + 2, (float)wy - 1, (float)(pw - 4), 14.0f, altBg);
		}

		CG_FontSelect(0);

		/* K/D */
		Com_sprintf(tmpStr, sizeof(tmpStr), "%i/%i", aKills, aDeaths);
		CG_ModernDrawString(cKD, wy, tmpStr, textColor, 7, 11, SCREEN_WIDTH, DS_HRIGHT | DS_SHADOW, NULL);

		/* efficiency */
		eff = (aKills + aDeaths > 0) ? (aKills * 100 / (aKills + aDeaths)) : 0;
		if (aKills > 0 && aDeaths == 0) eff = 100;
		Com_sprintf(tmpStr, sizeof(tmpStr), "%i%%", eff);
		CG_ModernDrawString(cEff, wy, tmpStr, textColor, 7, 11, SCREEN_WIDTH, DS_HRIGHT | DS_SHADOW, NULL);

		/* weapon icon */
		weap = bg_attacklist[att].weapon;
		if (weap != WP_NONE && cg_weapons[weap].weaponIcon)
		{
			CG_DrawPic((float)(cIcon), (float)(wy), 12.0f, 12.0f, cg_weapons[weap].weaponIcon);
		}

		/* hits/shots */
		Com_sprintf(tmpStr, sizeof(tmpStr), "%i/%i", aHits, aShots);
		CG_ModernDrawString(cHits, wy, tmpStr, textColor, 7, 11, SCREEN_WIDTH, DS_HRIGHT | DS_SHADOW, NULL);

		/* accuracy */
		acc = (aShots > 0) ? (aHits * 100 / aShots) : 0;
		if (acc > 50)
			Com_sprintf(tmpStr, sizeof(tmpStr), "^2%i%%", acc);
		else if (acc > 30)
			Com_sprintf(tmpStr, sizeof(tmpStr), "^3%i%%", acc);
		else
			Com_sprintf(tmpStr, sizeof(tmpStr), "^1%i%%", acc);
		CG_ModernDrawString(cAcc, wy, tmpStr, textColor, 7, 11, SCREEN_WIDTH, DS_HRIGHT | DS_SHADOW, NULL);

		wy += 14;
	}

	/* summary row */
	CG_FillRect((float)px + 4, (float)(wy - 1), (float)(pw - 8), 1.0f, lineColor);
	wy += 3;
	{
		vec4_t sumColor;
		sumColor[0] = 0.9f; sumColor[1] = 0.9f; sumColor[2] = 1.0f; sumColor[3] = fade;
		CG_FontSelect(0);

		Com_sprintf(tmpStr, sizeof(tmpStr), "%i/%i", totalKills, totalDeaths);
		CG_ModernDrawString(cKD, wy, tmpStr, sumColor, 7, 11, SCREEN_WIDTH, DS_HRIGHT | DS_SHADOW, NULL);

		eff = (totalKills + totalDeaths > 0) ? (totalKills * 100 / (totalKills + totalDeaths)) : 0;
		Com_sprintf(tmpStr, sizeof(tmpStr), "%i%%", eff);
		CG_ModernDrawString(cEff, wy, tmpStr, sumColor, 7, 11, SCREEN_WIDTH, DS_HRIGHT | DS_SHADOW, NULL);

		/* "TOTAL" label in icon column */
		CG_ModernDrawString(cIcon + 6, wy, "TOTAL", sumColor, 5, 9, SCREEN_WIDTH, DS_HLEFT | DS_SHADOW, NULL);

		Com_sprintf(tmpStr, sizeof(tmpStr), "%i/%i", totalHits, totalShots);
		CG_ModernDrawString(cHits, wy, tmpStr, sumColor, 7, 11, SCREEN_WIDTH, DS_HRIGHT | DS_SHADOW, NULL);

		acc = (totalShots > 0) ? (totalHits * 100 / totalShots) : 0;
		Com_sprintf(tmpStr, sizeof(tmpStr), "%i%%", acc);
		CG_ModernDrawString(cAcc, wy, tmpStr, sumColor, 7, 11, SCREEN_WIDTH, DS_HRIGHT | DS_SHADOW, NULL);
	}

	/* clip the panel background to actual content height */
	/* (we drew 400px tall; overdraw is hidden by the dark full-screen bg) */
	(void)wy;
}

qboolean CG_ModernDrawTourneyScoreboard(void)
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
	CG_FillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, bgColor);

	textColor[0] = textColor[1] = textColor[2] = 1.0f;
	textColor[3] = (*color)[3];

	/* large timer at top */
	{
		int msec = cg.time - cgs.levelStartTime;
		int secs = msec / 1000;
		int mins = secs / 60;
		secs %= 60;
		s = va("%i:%02i", mins, secs);
		CG_FontSelect(0);
		CG_ModernDrawString(SCREEN_WIDTH / 2.0f, 8, s, textColor, 20, 28, SCREEN_WIDTH, DS_HCENTER | DS_SHADOW, NULL);
	}

	/* fragged by */
	if (cg.killerName[0] && !cg.demoPlayback)
	{
		s = va("Fragged by %s", cg.killerName);
		CG_FontSelect(2);
		CG_ModernDrawString(SCREEN_WIDTH / 2.0f, 38, s, colorYellow, 8, 12, SCREEN_WIDTH, DS_HCENTER | DS_PROPORTIONAL | DS_SHADOW, NULL);
	}

	/* two side-by-side panels */
	localClient = qfalse;
	if (fighter[0] >= 0)
	{
		sc = &cg.scores[fighter[0]];
		if (sc->client == cg.snap->ps.clientNum)
			localClient = qtrue;
		CG_DrawTourneyFighterPanel(8, 56, 304, sc, (*color)[3]);
	}
	if (fighter[1] >= 0)
	{
		sc = &cg.scores[fighter[1]];
		if (sc->client == cg.snap->ps.clientNum)
			localClient = qtrue;
		CG_DrawTourneyFighterPanel(328, 56, 304, sc, (*color)[3]);
	}

	/* "VS" between panels */
	{
		vec4_t vsColor;
		vsColor[0] = 0.5f; vsColor[1] = 0.5f; vsColor[2] = 0.6f; vsColor[3] = (*color)[3] * 0.6f;
		CG_FontSelect(0);
		CG_ModernDrawString(SCREEN_WIDTH / 2.0f, 80, "vs", vsColor, 10, 14, SCREEN_WIDTH, DS_HCENTER | DS_SHADOW, NULL);
	}

	/* spectators at bottom */
	y = 460;
	specCount = 0;
	CG_FontSelect(2);
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
		CG_FontSelect(2);
		CG_ModernDrawString(SCREEN_WIDTH / 2.0f, y - 12, "Spectators", specHdr, 7, 10, SCREEN_WIDTH, DS_HCENTER | DS_SHADOW, NULL);

		drewSpect = 0;
		for (i = 0; i < cg.numScores; i++)
		{
			ci = &cgs.clientinfo[cg.scores[i].client];
			if (cg.demoPlayback && !ci->infoValid)
				continue;
			if (ci->team != TEAM_SPECTATOR)
				continue;

			sx = 40 + (drewSpect % 4) * 148;
			CG_ModernDrawString(sx, y + (drewSpect / 4) * 10, ci->name, textColor, 6, 9, 140, DS_HLEFT | DS_PROPORTIONAL | DS_SHADOW, NULL);
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
		CG_FontSelect(0);
		CG_ModernDrawString(SCREEN_WIDTH / 2.0f, 468, s, footColor, 5, 8, SCREEN_WIDTH, DS_HCENTER | DS_SHADOW, NULL);
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
	float       x;
	vec4_t      color;

	color[0] = 1;
	color[1] = 1;
	color[2] = 1;
	color[3] = 1;

	x = 0.5 * (640 - GIANT_WIDTH * CG_DrawStrlen(string));

	CG_DrawStringExt(x, y, string, color, qtrue, qtrue, GIANT_WIDTH, GIANT_HEIGHT, 0);
}

/*
=================
CG_DrawTourneyScoreboard

Draw the oversize scoreboard for tournements
=================
*/
void CG_DrawTourneyScoreboard(void)
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
	CG_FillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, color);

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
	if (cgs.gametype >= GT_TEAM)
	{
		//
		// teamplay scoreboard
		//
		CG_DrawStringExt(8, y, "Red Team", color, qtrue, qtrue, GIANT_WIDTH, GIANT_HEIGHT, 0);
		s = va("%i", cg.teamScores[0]);
		CG_DrawStringExt(632 - GIANT_WIDTH * strlen(s), y, s, color, qtrue, qtrue, GIANT_WIDTH, GIANT_HEIGHT, 0);

		y += 64;

		CG_DrawStringExt(8, y, "Blue Team", color, qtrue, qtrue, GIANT_WIDTH, GIANT_HEIGHT, 0);
		s = va("%i", cg.teamScores[1]);
		CG_DrawStringExt(632 - GIANT_WIDTH * strlen(s), y, s, color, qtrue, qtrue, GIANT_WIDTH, GIANT_HEIGHT, 0);
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

			CG_DrawStringExt(8, y, CG_ClientName( ci ), color, qtrue, qtrue, GIANT_WIDTH, GIANT_HEIGHT, 0);
			s = va("%i", ci->score);
			CG_DrawStringExt(632 - GIANT_WIDTH * strlen(s), y, s, color, qtrue, qtrue, GIANT_WIDTH, GIANT_HEIGHT, 0);
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
		CG_FillRect(x + 8, y, 304.0f, 17.0f, ourColor);
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
		CG_ModernDrawString(x + 34, y + 2, string, colorWhite, 8, 12, SCREEN_WIDTH, DS_HLEFT | DS_SHADOW, NULL);
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
		CG_ModernDrawString(x + 46, y, string, color, 12, 16, SCREEN_WIDTH, DS_HLEFT | DS_SHADOW, NULL);
		if (cgs.gametype == GT_TEAM)
		{
			if (CG_ModernIsGameTypeFreeze())
			{
				Com_sprintf(string, 1024, "%3i", score->scoreFlags);
			}
			else
			{
				Com_sprintf(string, 1024, "^%i%3i", score->scoreFlags < 0 ? 3 : 7, score->scoreFlags);
			}
			CG_ModernDrawString(x + 90, y + 4, string, color, 8, 12, SCREEN_WIDTH, DS_HLEFT | DS_SHADOW, NULL);
		}
		else
		{
			CG_ModernDrawString(x + 90, y + 4, " 0", color, 8, 12, SCREEN_WIDTH, DS_HLEFT | DS_SHADOW, NULL);
		}
		Com_sprintf(string, 1024, "^%i%3i", pingColor, score->ping);
		CG_ModernDrawString(x + 118, y, string, color, 12, 16, SCREEN_WIDTH, DS_HLEFT | DS_SHADOW, NULL);
		Com_sprintf(string, 1024, "%3i", score->time);
		CG_ModernDrawString(x + 150, y, string, color, 12, 16, SCREEN_WIDTH, DS_HLEFT | DS_SHADOW, NULL);
		Com_sprintf(string, 1024, "%s", ci->name);
		CG_ModernDrawString(x + 202, y + 4, string, color, 8, 12, 102, DS_HLEFT | DS_SHADOW, NULL);

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

	CG_AdjustFrom640(&x1, &y1, &w1, &h1);

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
	float lWidth = 6, lHeight = 10;
	float mWidth = 8, mHeight = 12;
	float bWidth = 12, bHeight = 16;
	clientInfo_t *ci;
	vec3_t  headAngles;
	int font = 2; // sansman default
	int proportional = 0;

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
		CG_FillRect(x + 8, y, 304.0f, 17.0f, ourColor);
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

	CG_FontSelect(font);

	VectorClear(headAngles);
	headAngles[1] = 180.0f;
	CG_DrawHead(x + 22, y, 16.0f, 16.0f, score->client, headAngles);

	// frozen foe tag not available in q3now

	if (score->ping == -1)
	{
		Com_sprintf(string, 1024, " ^2connecting^7      %s", ci->name);
		CG_ModernDrawStringNew(x + 34, y + 2, string, colorWhite, colorBlack, mWidth, mHeight, SCREEN_WIDTH, DS_HLEFT | proportional | DS_SHADOW, NULL, NULL, NULL);
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
		CG_ModernDrawStringNew(x + 80, y, string, color, colorBlack, bWidth, bHeight, SCREEN_WIDTH, DS_HRIGHT | proportional | DS_SHADOW, NULL, NULL, NULL);
		if (cgs.gametype == GT_TEAM)
		{
			if (CG_ModernIsGameTypeFreeze())
			{
				Com_sprintf(string, 1024, "%i", score->scoreFlags);
			}
			else
			{
				Com_sprintf(string, 1024, "^%i%3i", score->scoreFlags < 0 ? 3 : 7, score->scoreFlags);
			}
			CG_ModernDrawStringNew(x + 112, y + 4, string, color, colorBlack, mWidth, mHeight, SCREEN_WIDTH, DS_HRIGHT | proportional | DS_SHADOW, NULL, NULL, NULL);
		}
		else
		{
			CG_ModernDrawStringNew(x + 112, y + 4, "0", color, colorBlack, mWidth, mHeight, SCREEN_WIDTH, DS_HRIGHT | proportional | DS_SHADOW, NULL, NULL, NULL);
		}
		Com_sprintf(string, 1024, "^%i%3i", pingColor, score->ping);
		CG_ModernDrawStringNew(x + 152, y, string, color, colorBlack, bWidth, bHeight, SCREEN_WIDTH, DS_HRIGHT | proportional | DS_SHADOW, NULL, NULL, NULL);

		Com_sprintf(string, 1024, "%i", score->time);
		CG_ModernDrawStringNew(x + 184, y, string, color, colorBlack, bWidth, bHeight, SCREEN_WIDTH, DS_HRIGHT | proportional | DS_SHADOW, NULL, NULL, NULL);

		Com_sprintf(string, 1024, "%s", ci->name);
		CG_ModernDrawStringNew(x + 202, y + 4, string, color, colorBlack, mWidth, mHeight, 102, DS_HLEFT | proportional | DS_SHADOW, NULL, NULL, NULL);

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
	if ((cgs.gametype == GT_SINGLE_PLAYER) && (cg.predictedPlayerState.pm_type == PM_INTERMISSION))
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
		CG_DrawBigString(SCREEN_WIDTH / 2, y, "^3Demo Playback", CG_ColorFromAlpha((*color)[0]), DS_HCENTER | DS_PROPORTIONAL | DS_SHADOW, 2);
	}
	else if (cg.killerName[0])
	{
		CG_DrawBigString(SCREEN_WIDTH / 2, y, va("Fragged by %s", cg.killerName), CG_ColorFromAlpha((*color)[0]), DS_HCENTER | DS_PROPORTIONAL | DS_SHADOW, 2);
	}


	y = 64;

	CG_ScoreboardAdjustTeamColor(scoreboard_rtColor, colorRect);
	CG_FillRect(8.0f, (float)y, 304.0f, 48.0f, colorRect);

	CG_ScoreboardAdjustTeamColor(scoreboard_btColor, colorRect);
	CG_FillRect(328.0f, (float)y, 304.0f, 48.0f, colorRect);

	CG_ScoreboardDrawField(8, y, cg.teamScores[0]);
	trap_R_SetColor(NULL);
	CG_ScoreboardDrawField(328, y, cg.teamScores[1]);
	trap_R_SetColor(NULL);
	y = 116;

	CG_FontSelect(0);

	{
		const char *tmpStr;
		const char *tmpArgStr;
		if (cgs.gametype == GT_TEAM)
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
		CG_ModernDrawString(40, y, tmpStr, colorWhite, 8, 12, SCREEN_WIDTH, DS_HLEFT | DS_SHADOW, NULL);
	}

	{
		const char *tmpStr;
		const char *tmpArgStr;
		if (cgs.gametype == GT_TEAM)
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
		CG_ModernDrawString(360, y, tmpStr, colorWhite, 8, 12, SCREEN_WIDTH, DS_HLEFT | DS_SHADOW, NULL);
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
			CG_ModernDrawString(116, 64, tmpStr, colorWhite, 8, 16, SCREEN_WIDTH, DS_HLEFT | DS_SHADOW, NULL);
			Com_sprintf(string, 128, "^3%3i^7  %2i  %3i", sumScoresRed, drewRed, sumPingRed / drewRed);
			CG_ModernDrawString(116, 80, string, colorWhite, 16, 20, SCREEN_WIDTH, DS_HLEFT | DS_SHADOW, NULL);
		}
		else
		{
			if (cgs.gametype == GT_TEAM && !CG_ModernIsGameTypeFreeze())
			{
				tmpStr = va("^1Players  AvgPing");
				CG_ModernDrawString(104, 64, tmpStr, colorWhite, 8, 16, SCREEN_WIDTH, DS_HLEFT | DS_SHADOW, NULL);
				Com_sprintf(string, 128, " %2i  %3i", drewRed, sumPingRed / drewRed);
				CG_ModernDrawString(88, 80, string, colorWhite, 16, 20, SCREEN_WIDTH, DS_HLEFT | DS_SHADOW, NULL);
			}
			else if (CG_ModernIsGameTypeFreeze())
			{
				tmpStr = va("^1Scores   Thaws Players");
				CG_ModernDrawString(80, 64, tmpStr, colorWhite, 8, 16, SCREEN_WIDTH, DS_HLEFT | DS_SHADOW, NULL);
				Com_sprintf(string, 128, " %3i %3i  %2i", sumScoresRed, sumThawsRed, drewRed);
				CG_ModernDrawString(64, 80, string, colorWhite, 16, 20, SCREEN_WIDTH, DS_HLEFT | DS_SHADOW, NULL);
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
			CG_ModernDrawString(436, 64, tmpStr, colorWhite, 8, 16, SCREEN_WIDTH, DS_HLEFT | DS_SHADOW, NULL);
			Com_sprintf(string, 128, "^3%3i^7  %2i  %3i", sumScoresBlue, drewBlue, sumPingBlue / drewBlue);
			CG_ModernDrawString(436, 80, string, colorWhite, 16, 20, SCREEN_WIDTH, DS_HLEFT | DS_SHADOW, NULL);
		}
		else if (cgs.gametype == GT_TEAM && !CG_ModernIsGameTypeFreeze())
		{
			tmpStr = va("^4Players  AvgPing");
			CG_ModernDrawString(424, 64, tmpStr, colorWhite, 8, 16, SCREEN_WIDTH, DS_HLEFT | DS_SHADOW, NULL);
			Com_sprintf(string, 128, " %2i  %3i", drewBlue, sumPingBlue / drewBlue);
			CG_ModernDrawString(408, 80, string, colorWhite, 16, 20, SCREEN_WIDTH, DS_HLEFT | DS_SHADOW, NULL);
		}
		else if (CG_ModernIsGameTypeFreeze())
		{
			tmpStr = va("^4Scores   Thaws Players");
			CG_ModernDrawString(400, 64, tmpStr, colorWhite, 8, 16, SCREEN_WIDTH, DS_HLEFT | DS_SHADOW, NULL);
			Com_sprintf(string, 128, " %3i %3i  %2i", sumScoresBlue, sumThawsBlue, drewBlue);
			CG_ModernDrawString(384, 80, string, colorWhite, 16, 20, SCREEN_WIDTH, DS_HLEFT | DS_SHADOW, NULL);
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
		CG_ModernDrawString(60, y - 14, "^1Red Team Spectator", colorWhite, 8, 12, SCREEN_WIDTH, DS_HLEFT | DS_SHADOW, NULL);
	}
	if (drewBlue != 0)
	{
		CG_ModernDrawString(380, y - 14, "^4Blue Team Spectator", colorWhite, 8, 12, SCREEN_WIDTH, DS_HLEFT | DS_SHADOW, NULL);
	}

	{
		int max;
		vec4_t bgColor;

		max = drewRed < drewBlue ? drewBlue : drewRed;

		y = y + 18 * max + 36;

		CG_ScoreboardAdjustTeamColor(scoreboard_rtColor, bgColor);
		CG_FillRect(8.0f, 112.0f, 304.0f, (float)y - 148, bgColor);

		CG_ScoreboardAdjustTeamColor(scoreboard_btColor, bgColor);
		CG_FillRect(328.0f, 112.0f, 304.0f, (float)y - 148, bgColor);

		drewSpect = CG_ModernDrawTeamScores(0, y, TEAM_SPECTATOR, (*color)[0], 24);

		if (drewSpect)
		{
			CG_ModernDrawString(SCREEN_WIDTH / 2.0f, y - 32, "Spectator", colorWhite,
			                 8, 12, SCREEN_WIDTH, DS_HCENTER | DS_SHADOW, NULL);

			if ((customScoreboardColorIsSet_spec & 1) == 0)
			{
				bgColor[0] = bgColor[1] = bgColor[2] = 0.5f;
			}
			else
			{
				Vector4Copy(scoreboard_specColor, bgColor);
			}
			bgColor[3] = 0.15f;

			CG_FillRect(8.0f, (float)(y - 34), 624.0f, (float)(9 * drewSpect + 29), bgColor);
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
	else if (cgs.gametype == GT_TEAM && !CG_ModernIsGameTypeFreeze())
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

		CG_ModernDrawStringNew(baseX + posX[i], sb_row1Y, labels[i], curColor, colorBlack,
		                    sb_mWidth2, sb_mHeight2, SCREEN_WIDTH,
		                    DS_HRIGHT | DS_SHADOW | sb_proportional, NULL, NULL, NULL);

		CG_ModernDrawStringNew(baseX + posX[i], sb_row2Y, values[i], colorWhite, colorBlack,
		                    sb_bWidth2, sb_bHeight2, SCREEN_WIDTH,
		                    DS_HRIGHT | DS_SHADOW | sb_proportional, NULL, NULL, NULL);
	}
}


static void CG_ModernDrawScoreHeader(float baseX, float y, vec4_t colorBody, vec4_t shadowColor, int mWidth_p, int mHeight_p, int screenWidth, int proportional_p)
{
	const char *label1 = "Score";
	const char *label2 = (cgs.gametype == GT_TEAM)
	                     ? (CG_ModernIsGameTypeFreeze() ? "THW" : "NET")
	                     : "PL";
	const char *label3 = "Ping";
	const char *label4 = "Min";
	const char *label5 = "Name";

	vec4_t headerColor1;

	Vector4Copy(colorBody, headerColor1);

	CG_ModernDrawStringNew(baseX + sb_pos1X, y, label1, headerColor1, shadowColor, mWidth_p, mHeight_p, screenWidth,
	                    DS_HRIGHT | proportional_p | DS_SHADOW, NULL, NULL, NULL);
	CG_ModernDrawStringNew(baseX + sb_pos2X, y, label2, colorWhite, shadowColor, mWidth_p, mHeight_p, screenWidth,
	                    DS_HRIGHT | proportional_p | DS_SHADOW, NULL, NULL, NULL);
	CG_ModernDrawStringNew(baseX + sb_pos3X, y, label3, colorWhite, shadowColor, mWidth_p, mHeight_p, screenWidth,
	                    DS_HRIGHT | proportional_p | DS_SHADOW, NULL, NULL, NULL);
	CG_ModernDrawStringNew(baseX + sb_pos4X, y, label4, colorWhite, shadowColor, mWidth_p, mHeight_p, screenWidth,
	                    DS_HRIGHT | proportional_p | DS_SHADOW, NULL, NULL, NULL);
	CG_ModernDrawStringNew(baseX + sb_pos5X, y, label5, colorWhite, shadowColor, mWidth_p, mHeight_p, screenWidth,
	                    DS_HLEFT | proportional_p | DS_SHADOW, NULL, NULL, NULL);
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
	int font = 2; // sansman default

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

	if ((cgs.gametype == GT_SINGLE_PLAYER) && (cg.predictedPlayerState.pm_type == PM_INTERMISSION))
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

	CG_FontSelect(font);

	if (cg.demoPlayback)
	{
		CG_ModernDrawStringNew(SCREEN_WIDTH / 2.0f, y, "^3Demo Playback", *color, colorBlack, sb_bWidth, sb_bHeight, SCREEN_WIDTH, DS_HCENTER | sb_proportional | DS_SHADOW, NULL, NULL, NULL);
	}
	else if (cg.killerName[0])
	{
		CG_ModernDrawStringNew(SCREEN_WIDTH / 2.0f, y, va("Fragged by %s", cg.killerName), *color, colorBlack, sb_mWidth, sb_bHeight, SCREEN_WIDTH, DS_HCENTER | sb_proportional | DS_SHADOW, NULL, NULL, NULL);
	}

	SetScoreboardColors(&rtColorHeader, &rtColorBody, &btColorHeader, &btColorBody);

	// Header background
	y = 64;

	CG_ScoreboardAdjustTeamColor(rtColorHeader, colorRect);
	colorRect[3] *= 1.5;
	CG_FillRect(8.0f, (float)y, 304.0f, 48.0f, colorRect);

	CG_ScoreboardAdjustTeamColor(btColorHeader, colorRect);
	colorRect[3] *= 1.5;
	CG_FillRect(328.0f, (float)y, 304.0f, 48.0f, colorRect);

	// main team scores -- use large text style (cg_scoreboardBE & 2 would use CG_ModernDrawField, but we use 0)
	CG_ModernDrawStringNew(sb_leftX - 32, 87, va("%d", cg.teamScores[0]), colorWhite, colorBlack, 42, 60, SCREEN_WIDTH, DS_HLEFT | sb_proportional | DS_SHADOW | DS_VCENTER, NULL, NULL, NULL);
	CG_ModernDrawStringNew(sb_rightX - 32, 87, va("%d", cg.teamScores[1]), colorWhite, colorBlack, 42, 60, SCREEN_WIDTH, DS_HLEFT | sb_proportional | DS_SHADOW | DS_VCENTER, NULL, NULL, NULL);

	y = 116;
	// Header text
	CG_ModernDrawScoreHeader(sb_leftX, y, (customScoreboardColorIsSet_red & 2) ? scoreboard_rtColorTitle : rtColorBody, colorBlack, sb_mWidth, sb_mHeight, SCREEN_WIDTH, sb_proportional);
	CG_ModernDrawScoreHeader(sb_rightX, y, (customScoreboardColorIsSet_blue & 2) ? scoreboard_btColorTitle : btColorBody, colorBlack, sb_mWidth, sb_mHeight, SCREEN_WIDTH, sb_proportional);

	y = 140;
	// Team score lines
	drewRed = CG_ModernDrawTeamScores(0, y, TEAM_RED, (*color)[0], 32);
	drewBlue = CG_ModernDrawTeamScores(320, y, TEAM_BLUE, (*color)[0], 32);

	if (drewRed)
	{
		float baseX = (cgs.gametype >= GT_CTF) ? (sb_leftX + 76) :
		              (cgs.gametype == GT_TEAM && !CG_ModernIsGameTypeFreeze()) ? (sb_leftX + 64) :
		              (CG_ModernIsGameTypeFreeze()) ? (sb_leftX + 40) : sb_leftX;
		CG_ModernDrawTeamSummary(baseX, drewRed, sumScoresRed, sumThawsRed, sumPingRed, (customScoreboardColorIsSet_red & 2) ? scoreboard_rtColorTitle : rtColorHeader);
	}

	if (drewBlue)
	{
		float baseX = (cgs.gametype >= GT_CTF) ? (sb_rightX + 76) :
		              (cgs.gametype == GT_TEAM && !CG_ModernIsGameTypeFreeze()) ? (sb_rightX + 64) :
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
			CG_ModernDrawStringNew(sb_leftX + 20, y - 14, "Red", rtColorHeader, colorBlack,
			                    sb_mWidth, sb_mHeight, SCREEN_WIDTH, DS_HLEFT | DS_SHADOW | sb_proportional, NULL, NULL, NULL);
			CG_ModernDrawStringNew(sb_leftX + 60, y - 14, "Team", colorWhite, colorBlack,
			                    sb_mWidth, sb_mHeight, SCREEN_WIDTH, DS_HLEFT | DS_SHADOW | sb_proportional, NULL, NULL, NULL);
			CG_ModernDrawStringNew(sb_leftX + 100, y - 14, "Spectator", colorWhite, colorBlack,
			                    sb_mWidth, sb_mHeight, SCREEN_WIDTH, DS_HLEFT | DS_SHADOW | sb_proportional, NULL, NULL, NULL);
		}

		if (drewBlue)
		{
			CG_ModernDrawStringNew(sb_rightX + 20, y - 14, "Blue", btColorHeader, colorBlack,
			                    sb_mWidth, sb_mHeight, SCREEN_WIDTH, DS_HLEFT | DS_SHADOW | sb_proportional, NULL, NULL, NULL);
			CG_ModernDrawStringNew(sb_rightX + 60, y - 14, "Team", colorWhite, colorBlack,
			                    sb_mWidth, sb_mHeight, SCREEN_WIDTH, DS_HLEFT | DS_SHADOW | sb_proportional, NULL, NULL, NULL);
			CG_ModernDrawStringNew(sb_rightX + 100, y - 14, "Spectator", colorWhite, colorBlack,
			                    sb_mWidth, sb_mHeight, SCREEN_WIDTH, DS_HLEFT | DS_SHADOW | sb_proportional, NULL, NULL, NULL);
		}
	}

	// BODY
	{
		int max = (drewRed > drewBlue) ? drewRed : drewBlue;
		vec4_t bgColor;

		y += 18 * max + 36;

		CG_ScoreboardAdjustTeamColor(rtColorBody, bgColor);
		CG_FillRect(8.0f, 112.0f, 304.0f, (float)(y - 148), bgColor);

		CG_ScoreboardAdjustTeamColor(btColorBody, bgColor);
		CG_FillRect(328.0f, 112.0f, 304.0f, (float)(y - 148), bgColor);

		drewSpect = CG_ModernDrawTeamScores(0, y, TEAM_SPECTATOR, (*color)[0], 24);

		if (drewSpect)
		{
			CG_ModernDrawString(SCREEN_WIDTH / 2.0f, y - 32, "Spectator", (customScoreboardColorIsSet_spec & 2) ? scoreboard_specColorTitle : colorWhite,
			                 8, 12, SCREEN_WIDTH, DS_HCENTER | DS_SHADOW | sb_proportional, NULL);

			if ((customScoreboardColorIsSet_spec & 1) == 0)
			{
				bgColor[0] = bgColor[1] = bgColor[2] = 0.5f;
			}
			else
			{
				Vector4Copy(scoreboard_specColor, bgColor);
			}
			bgColor[3] = 0.15f;

			CG_FillRect(8.0f, (float)(y - 34), 624.0f, (float)(9 * drewSpect + 29), bgColor);
		}
	}

	return qtrue;
}
