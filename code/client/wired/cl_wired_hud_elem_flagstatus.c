#include "../client.h"
#include "cl_wired_hud_compat.h"
#include "cl_wired_hud_private.h"

#if FEAT_WIRED_UI



enum flagType_t
{
	SHUDFLTYPE_OWN,
	SHUDFLTYPE_NME,
} flagType;

typedef struct
{
	superhudConfig_t config;
	superhudDrawContext_t ctx;
	enum flagType_t flagType;
} shudElementFlagStatus_t;

static void* CG_SHUDElementFlagStatusCreate(const superhudConfig_t* config, enum flagType_t flagType)
{
	shudElementFlagStatus_t* element;

	SHUD_ELEMENT_INIT(element, config);

	CG_SHUDDrawMakeContext(&element->config, &element->ctx);
	Vector4Copy(colorWhite, element->ctx.color);

	element->flagType = flagType;

	return element;
}

void* CG_SHUDElementFlagStatusNMECreate(const superhudConfig_t* config)
{
	return CG_SHUDElementFlagStatusCreate(config, SHUDFLTYPE_NME);
}

void* CG_SHUDElementFlagStatusOWNCreate(const superhudConfig_t* config)
{
	return CG_SHUDElementFlagStatusCreate(config, SHUDFLTYPE_OWN);
}

void CG_SHUDElementFlagStatusRoutine(void* context)
{
	shudElementFlagStatus_t* element = (shudElementFlagStatus_t*)context;
	team_t team;
	gitem_t* item;

	team = cgs.clientinfo[cg.snap->ps.clientNum].team;

	if (element->flagType == SHUDFLTYPE_NME)
	{
		switch (team)
		{
			case TEAM_RED:
				team = TEAM_BLUE;
				break;
			case TEAM_BLUE:
				team = TEAM_RED;
				break;
			case TEAM_FREE:
			case TEAM_SPECTATOR:
			case TEAM_4:
			case TEAM_5:
			case TEAM_6:
			case TEAM_7:
			case TEAM_NUM_TEAMS:
				return;
		}
	}

	if (team == TEAM_RED)
	{
		item = BG_FindItemForPowerup(PW_REDFLAG);
		element->ctx.image = cgs.media.redFlagShader[cgs.redflag];
	}
	else if (team == TEAM_BLUE)
	{
		item = BG_FindItemForPowerup(PW_BLUEFLAG);
		element->ctx.image = cgs.media.blueFlagShader[cgs.blueflag];
	}
	else
	{
		return;
	}

	CG_SHUDFill(&element->config);
	CG_SHUDDrawBorder(&element->config);

	if (item)
	{
		// element->ctx.color[0] = 1.0f;
		// element->ctx.color[1] = 1.0f;
		// element->ctx.color[2] = 1.0f;
		// element->ctx.color[3] = 1.0f;
		CG_SHUDDrawStretchPic(element->ctx.coord, element->ctx.coordPicture, element->ctx.color, element->ctx.image);
	}
}

void CG_SHUDElementFlagStatusDestroy(void* context)
{
	if (context)
	{
		Z_Free(context);
	}
}

#endif // FEAT_WIRED_UI
