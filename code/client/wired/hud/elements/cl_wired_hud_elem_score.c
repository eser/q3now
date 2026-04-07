/* cl_wired_hud_elem_score.c -- Score display HUD elements (OWN / NME / MAX)
   Uses numeric team IDs (0=free, 1=red, 2=blue, 3=spectator) and
   pre-computed bridge fields to avoid game enum dependencies. */
#include "../../../client.h"
#include "../cl_wired_hud_compat.h"
#include "../cl_wired_hud_private.h"

#if FEAT_WIRED_UI

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
	*scores = cgs.scorelimit;

	return *scores > 0;
}

static qboolean CG_SHUDScoresGetOWN(int* scores)
{
	int side = (int)CG_SHUDGetOurActiveTeam();

	switch (side)
	{
		case 0: /* free */
			*scores = cg.snap->ps.persistant[PERS_SCORE];
			return qtrue;
		case 1: /* red */
			*scores = cgs.scores1;
			return *scores != SCORE_NOT_PRESENT;
		case 2: /* blue */
			*scores = cgs.scores2;
			return *scores != SCORE_NOT_PRESENT;
		default:
			break;
	}
	return qfalse;
}

static qboolean CG_SHUDScoresGetNME(int* scores)
{
	int side = (int)CG_SHUDGetOurActiveTeam();

	switch (side)
	{
		case 0: /* free */
		{
			int playerScore = cg.snap->ps.persistant[PERS_SCORE];
			if (playerScore == cgs.scores1)
				*scores = cgs.scores2;
			else
				*scores = cgs.scores1;
			return *scores != SCORE_NOT_PRESENT;
		}
		case 1: /* red */
			*scores = cgs.scores2;
			return *scores != SCORE_NOT_PRESENT;
		case 2: /* blue */
			*scores = cgs.scores1;
			return *scores != SCORE_NOT_PRESENT;
		default:
			break;
	}
	return qfalse;
}

static qboolean CG_SHUDScoresShouldUseColor2(shudElementScoreType_t type)
{
	int side = (int)CG_SHUDGetOurActiveTeam();
	int playerScore;

	if (side != 0 /* free */)
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

	switch (element->type)
	{
		case SHUD_ELEMENT_SCORE_OWN:
			result = CG_SHUDScoresGetOWN(&scores);
			break;

		case SHUD_ELEMENT_SCORE_NME:
			result = CG_SHUDScoresGetNME(&scores);

			if (!result && SHUD_CHECK_SHOW_EMPTY(element) &&
			        (int)CG_SHUDGetOurActiveTeam() == 0 /* free */)
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

#endif /* FEAT_WIRED_UI */
