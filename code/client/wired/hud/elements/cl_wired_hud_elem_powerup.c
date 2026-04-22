/* cl_wired_hud_elem_powerup.c -- Powerup icon and timer HUD elements
   Reads pre-computed powerup data from wiredHud->activePowerups[].
   cgame iterates game powerup arrays and pushes results; client displays. */
#include "../../../client.h"
#include "../cl_wired_hud_compat.h"
#include "../cl_wired_hud_private.h"

#if FEAT_WIRED_UI

enum modernHudPWType
{
	ModernHUDPWTYPE_TIME,
	ModernHUDPWTYPE_ICON,
};

typedef struct
{
	modernhudConfig_t config;
	modernhudTextContext_t textCtx;
	modernhudDrawContext_t drawCtx;
	enum modernHudPWType pwType;
	int pwIndex;
} modernHudElementPowerupContext;

static void* CG_ModernHUDElementPwCreate(const modernhudConfig_t* config, enum modernHudPWType pwType, int pwIndex)
{
	modernHudElementPowerupContext* element;

	ModernHUD_ELEMENT_INIT(element, config);

	element->pwType = pwType;
	element->pwIndex = pwIndex;

	if (pwType == ModernHUDPWTYPE_TIME)
	{
		CG_ModernHUDTextMakeContext(&element->config, &element->textCtx);
		CG_ModernHUDFillAndFrameForText(&element->config, &element->textCtx);
	}
	else
	{
		CG_ModernHUDDrawMakeContext(&element->config, &element->drawCtx);
	}

	return element;
}

void* CG_ModernHUDElementPwTimeCreate(const modernhudConfig_t* config, int index)
{
	return CG_ModernHUDElementPwCreate(config, ModernHUDPWTYPE_TIME, index);
}

void* CG_ModernHUDElementPwIconCreate(const modernhudConfig_t* config, int index)
{
	return CG_ModernHUDElementPwCreate(config, ModernHUDPWTYPE_ICON, index);
}

void CG_ModernHUDElementPwRoutine(void* context)
{
	modernHudElementPowerupContext* element = (modernHudElementPowerupContext*)context;
	int maxPW = wiredHud->activePowerupCount;
	int idx = element->pwIndex - 1;   /* pwIndex is 1-based */

	if (idx < 0 || idx >= maxPW)
	{
		return;
	}

	CG_ModernHUDFill(&element->config);
	CG_ModernHUDDrawBorder(&element->config);

	if (element->pwType == ModernHUDPWTYPE_TIME)
	{
		if (!wiredHud->activePowerups[idx].isHoldable)
		{
			element->textCtx.text = va("%d", wiredHud->activePowerups[idx].timeLeft);
			CG_ModernHUDTextPrint(&element->config, &element->textCtx);
		}
	}
	else
	{
		element->drawCtx.image = wiredHud->activePowerups[idx].icon;
		CG_ModernHUDDrawStretchPicCtx(&element->config, &element->drawCtx);
	}
}

#endif /* FEAT_WIRED_UI */
