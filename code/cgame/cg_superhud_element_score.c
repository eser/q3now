#include "cg_local.h"
#include "cg_superhud_private.h"


typedef enum
{
	SHUD_ELEMENT_SCORE_OWN,
	SHUD_ELEMENT_SCORE_NME,
	SHUD_ELEMENT_SCORE_MAX,
} shudElementScoreType_t;

typedef struct
{
	superhudConfig_t config;
	superhudTextContext_t ctx;
	shudElementScoreType_t type;
} shudElementScore;

static void* CG_SHUDElementScoreCreate(const superhudConfig_t* config, shudElementScoreType_t type)
{
	shudElementScore* element;

	SHUD_ELEMENT_INIT(element, config);

	//load defaults
	if (!element->config.color.isSet)
	{
		element->config.color.isSet = qtrue;
		element->config.color.value.type = SUPERHUD_COLOR_RGBA;
		Vector4Set(element->config.color.value.rgba, 1, 0.7, 0, 1);
	}

	if (!element->config.text.isSet)
	{
		element->config.text.isSet = qtrue;
		Q_strncpyz(element->config.text.value, "%i", sizeof(element->config.text.value));
	}

	CG_SHUDTextMakeContext(&element->config, &element->ctx);
	CG_SHUDFillAndFrameForText(&element->config, &element->ctx);

	element->type = type;

	return element;
}

void* CG_SHUDElementScoreOWNCreate(const superhudConfig_t* config)
{
	return CG_SHUDElementScoreCreate(config, SHUD_ELEMENT_SCORE_OWN);
}

void* CG_SHUDElementScoreNMECreate(const superhudConfig_t* config)
{
	return CG_SHUDElementScoreCreate(config, SHUD_ELEMENT_SCORE_NME);
}

void* CG_SHUDElementScoreMAXCreate(const superhudConfig_t* config)
{
	return CG_SHUDElementScoreCreate(config, SHUD_ELEMENT_SCORE_MAX);
}

static qboolean CG_SHUDScoresGetMax(int* scores)
{
	if (cgs.gametype == GT_CTF)
	{
		*scores = cgs.capturelimit;
	}
	else
	{
		*scores = cgs.fraglimit;
	}

	return *scores > 0;
}

static qboolean CG_SHUDScoresGetOWN(int* scores)
{
	team_t team = CG_SHUDGetOurActiveTeam();

	switch (team)
	{
		case TEAM_FREE:
			/* FFA: OWN = your actual score */
			*scores = cg.snap->ps.persistant[PERS_SCORE];
			return qtrue;
		case TEAM_RED:
			*scores = cgs.scores1;
			return *scores != SCORE_NOT_PRESENT;
		case TEAM_BLUE:
			*scores = cgs.scores2;
			return *scores != SCORE_NOT_PRESENT;
		case TEAM_SPECTATOR:
		case TEAM_4:
		case TEAM_5:
		case TEAM_6:
		case TEAM_7:
		case TEAM_NUM_TEAMS:
			break;
	}
	return qfalse;
}

static qboolean CG_SHUDScoresGetNME(int* scores)
{
	team_t team = CG_SHUDGetOurActiveTeam();

	switch (team)
	{
		case TEAM_FREE:
		{
			/* FFA: NME = leading opponent's score.
			   If we ARE the leader (our score == scores1), show scores2.
			   Otherwise show scores1 (the leader's score). */
			int playerScore = cg.snap->ps.persistant[PERS_SCORE];
			if (playerScore == cgs.scores1)
				*scores = cgs.scores2;
			else
				*scores = cgs.scores1;
			return *scores != SCORE_NOT_PRESENT;
		}
		case TEAM_RED:
			*scores = cgs.scores2;
			return *scores != SCORE_NOT_PRESENT;
		case TEAM_BLUE:
			*scores = cgs.scores1;
			return *scores != SCORE_NOT_PRESENT;
		case TEAM_SPECTATOR:
		case TEAM_4:
		case TEAM_5:
		case TEAM_6:
		case TEAM_7:
		case TEAM_NUM_TEAMS:
			break;
	}
	return qfalse;
}

static qboolean CG_SHUDScoresShouldUseColor2(shudElementScoreType_t type)
{
	team_t team = CG_SHUDGetOurActiveTeam();
	int playerScore;
	
	if (team != TEAM_FREE)
		return qfalse;
		
	playerScore = cg.snap->ps.persistant[PERS_SCORE];
	
	switch (type)
	{
		case SHUD_ELEMENT_SCORE_OWN:
			return (playerScore == cgs.scores1);
			
		case SHUD_ELEMENT_SCORE_NME:
			return (cgs.scores1 != playerScore);
		
		default:
			return qfalse;
	}
}

void CG_SHUDElementScoreRoutine(void* context)
{
	shudElementScore* element = (shudElementScore*)context;
	int scores;
	qboolean result = qfalse;
	int playerScore;
	qboolean shouldUseColor2;
	switch (element->type)
	{
		case SHUD_ELEMENT_SCORE_OWN:
			result = CG_SHUDScoresGetOWN(&scores);
			break;

		case SHUD_ELEMENT_SCORE_NME:
			result = CG_SHUDScoresGetNME(&scores);

			if (!result && SHUD_CHECK_SHOW_EMPTY(element) &&
			        CG_SHUDGetOurActiveTeam() == TEAM_FREE)
			{
				scores = 0;
				result = qtrue;
			}
			break;

		case SHUD_ELEMENT_SCORE_MAX:
			result = CG_SHUDScoresGetMax(&scores);
			break;
	}

	if (!result)
		return;

	element->ctx.text = va(element->config.text.value, scores);
	
	if (element->config.color2.isSet && CG_SHUDScoresShouldUseColor2(element->type))
	{
		Vector4Copy(element->config.color2.value.rgba, element->ctx.color);
		CG_SHUDTextPrintNew(&element->config, &element->ctx, qfalse);
	}
	else
	{
		CG_SHUDTextPrint(&element->config, &element->ctx);
	}
}


void CG_SHUDElementScoreDestroy(void* context)
{
	if (context)
	{
		Z_Free(context);
	}
}

