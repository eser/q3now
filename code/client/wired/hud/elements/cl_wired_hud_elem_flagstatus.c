/* cl_wired_hud_elem_flagstatus.c -- Flag status HUD elements (OWN / NME)
   Uses pre-computed flag shader handles from wiredHud bridge. Numeric
   team IDs (0=free, 1=red, 2=blue, 3=spectator) avoid enum dependencies. */
#include "../../../client.h"
#include "../cl_wired_hud_compat.h"
#include "../cl_wired_hud_private.h"

#if FEAT_WIRED_UI

enum flagType_t
{
	ModernHUDFLTYPE_OWN,
	ModernHUDFLTYPE_NME,
} flagType;

typedef struct
{
	modernhudConfig_t config;
	modernhudDrawContext_t ctx;
	enum flagType_t flagType;
} modernHudElementFlagStatus_t;

static void* CG_ModernHUDElementFlagStatusCreate(const modernhudConfig_t* config, enum flagType_t flagType)
{
	modernHudElementFlagStatus_t* element;

	ModernHUD_ELEMENT_INIT(element, config);

	CG_ModernHUDDrawMakeContext(&element->config, &element->ctx);
	Vector4Copy(colorWhite, element->ctx.color);

	element->flagType = flagType;

	return element;
}

void* CG_ModernHUDElementFlagStatusNMECreate(const modernhudConfig_t* config)
{
	return CG_ModernHUDElementFlagStatusCreate(config, ModernHUDFLTYPE_NME);
}

void* CG_ModernHUDElementFlagStatusOWNCreate(const modernhudConfig_t* config)
{
	return CG_ModernHUDElementFlagStatusCreate(config, ModernHUDFLTYPE_OWN);
}

void CG_ModernHUDElementFlagStatusRoutine(void* context)
{
	modernHudElementFlagStatus_t* element = (modernHudElementFlagStatus_t*)context;
	int side;

	side = cgs.clientinfo[cg.snap->ps.clientNum].team;

	if (element->flagType == ModernHUDFLTYPE_NME)
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

	CG_ModernHUDFill(&element->config);
	CG_ModernHUDDrawBorder(&element->config);

	if (element->ctx.image)
	{
		CG_ModernHUDDrawStretchPic(element->ctx.coord, element->ctx.coordPicture, element->ctx.color, element->ctx.image);
	}
}

#endif /* FEAT_WIRED_UI */
