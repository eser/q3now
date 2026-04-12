/* cl_wired_hud_elem_score.c -- Score display HUD elements (OWN / NME / MAX)
   Uses numeric team IDs (0=free, 1=red, 2=blue, 3=spectator) and
   pre-computed bridge fields to avoid game enum dependencies. */
#include "../../../client.h"
#include "../cl_wired_hud_compat.h"
#include "../cl_wired_hud_private.h"

#if FEAT_WIRED_UI

typedef enum
{
	ModernHUD_ELEMENT_SCORE_OWN,
	ModernHUD_ELEMENT_SCORE_NME,
	ModernHUD_ELEMENT_SCORE_MAX,
} modernHudElementScoreType_t;

typedef struct
{
	modernhudConfig_t config;
	modernhudTextContext_t ctx;
	modernHudElementScoreType_t type;
} modernHudElementScore;

static void* CG_ModernHUDElementScoreCreate(const modernhudConfig_t* config, modernHudElementScoreType_t type)
{
	modernHudElementScore* element;

	ModernHUD_ELEMENT_INIT(element, config);

	if (!element->config.color.isSet)
	{
		element->config.color.isSet = qtrue;
		element->config.color.value.type = MODERNHUD_COLOR_RGBA;
		Vector4Set(element->config.color.value.rgba, 1, 0.7, 0, 1);
	}

	if (!element->config.text.isSet)
	{
		element->config.text.isSet = qtrue;
		Q_strncpyz(element->config.text.value, "%i", sizeof(element->config.text.value));
	}

	CG_ModernHUDTextMakeContext(&element->config, &element->ctx);
	CG_ModernHUDFillAndFrameForText(&element->config, &element->ctx);

	element->type = type;

	return element;
}

void* CG_ModernHUDElementScoreOWNCreate(const modernhudConfig_t* config)
{
	return CG_ModernHUDElementScoreCreate(config, ModernHUD_ELEMENT_SCORE_OWN);
}

void* CG_ModernHUDElementScoreNMECreate(const modernhudConfig_t* config)
{
	return CG_ModernHUDElementScoreCreate(config, ModernHUD_ELEMENT_SCORE_NME);
}

void* CG_ModernHUDElementScoreMAXCreate(const modernhudConfig_t* config)
{
	return CG_ModernHUDElementScoreCreate(config, ModernHUD_ELEMENT_SCORE_MAX);
}

static qboolean CG_ModernHUDScoresGetMax(int* scores)
{
	*scores = cgs.scorelimit;

	return *scores > 0;
}

static qboolean CG_ModernHUDScoresGetOWN(int* scores)
{
	int side = (int)CG_ModernHUDGetOurActiveTeam();

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

static qboolean CG_ModernHUDScoresGetNME(int* scores)
{
	int side = (int)CG_ModernHUDGetOurActiveTeam();

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

static qboolean CG_ModernHUDScoresShouldUseColor2(modernHudElementScoreType_t type)
{
	int side = (int)CG_ModernHUDGetOurActiveTeam();
	int playerScore;

	if (side != 0 /* free */)
		return qfalse;

	playerScore = cg.snap->ps.persistant[PERS_SCORE];

	switch (type)
	{
		case ModernHUD_ELEMENT_SCORE_OWN:
			return (playerScore == cgs.scores1);

		case ModernHUD_ELEMENT_SCORE_NME:
			return (cgs.scores1 != playerScore);

		default:
			return qfalse;
	}
}

void CG_ModernHUDElementScoreRoutine(void* context)
{
	modernHudElementScore* element = (modernHudElementScore*)context;
	int scores;
	qboolean result = qfalse;

	switch (element->type)
	{
		case ModernHUD_ELEMENT_SCORE_OWN:
			result = CG_ModernHUDScoresGetOWN(&scores);
			break;

		case ModernHUD_ELEMENT_SCORE_NME:
			result = CG_ModernHUDScoresGetNME(&scores);

			if (!result && ModernHUD_CHECK_SHOW_EMPTY(element) &&
			        (int)CG_ModernHUDGetOurActiveTeam() == 0 /* free */)
			{
				scores = 0;
				result = qtrue;
			}
			break;

		case ModernHUD_ELEMENT_SCORE_MAX:
			result = CG_ModernHUDScoresGetMax(&scores);
			break;
	}

	if (!result)
		return;

	element->ctx.text = va(element->config.text.value, scores);

	if (element->config.color2.isSet && CG_ModernHUDScoresShouldUseColor2(element->type))
	{
		Vector4Copy(element->config.color2.value.rgba, element->ctx.color);
		CG_ModernHUDTextPrintNew(&element->config, &element->ctx, qfalse);
	}
	else
	{
		CG_ModernHUDTextPrint(&element->config, &element->ctx);
	}
}


void CG_ModernHUDElementScoreDestroy(void* context)
{
	if (context)
	{
		Z_Free(context);
	}
}

#endif /* FEAT_WIRED_UI */
