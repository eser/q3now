/* cl_wired_hud_elem_flagstatus.c -- Flag status HUD elements (OWN / NME)
   Uses pre-computed flag shader handles from wiredHud bridge. Numeric
   team IDs (0=free, 1=red, 2=blue, 3=spectator) avoid enum dependencies. */
#include "../../../client.h"
#include "../cl_wired_hud_compat.h"
#include "../cl_wired_hud_private.h"

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
	int side;

	if (!wiredHud || !wiredHud->valid) return;

	side = cgs.clientinfo[cg.snap->ps.clientNum].team;

	if (element->flagType == SHUDFLTYPE_NME)
	{
		switch (side)
		{
			case 1: /* red -> show blue flag */
				side = 2;
				break;
			case 2: /* blue -> show red flag */
				side = 1;
				break;
			default:
				return;
		}
	}

	if (side == 1) /* red */
	{
		int idx = wiredHud->redflag;
		if (idx < 0 || idx > 2) idx = 0;
		element->ctx.image = wiredHud->redFlagShader[idx];
	}
	else if (side == 2) /* blue */
	{
		int idx = wiredHud->blueflag;
		if (idx < 0 || idx > 2) idx = 0;
		element->ctx.image = wiredHud->blueFlagShader[idx];
	}
	else
	{
		return;
	}

	CG_SHUDFill(&element->config);
	CG_SHUDDrawBorder(&element->config);

	if (element->ctx.image)
	{
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

#endif /* FEAT_WIRED_UI */
