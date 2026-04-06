/* cl_wired_hud_elem_powerup.c -- Powerup icon and timer HUD elements
   Reads pre-computed powerup data from wiredHud->activePowerups[].
   cgame iterates game powerup arrays and pushes results; client displays. */
#include "../client.h"
#include "cl_wired_hud_compat.h"
#include "cl_wired_hud_private.h"

#if FEAT_WIRED_UI

enum shudPWType
{
	SHUDPWTYPE_TIME,
	SHUDPWTYPE_ICON,
};

typedef struct
{
	superhudConfig_t config;
	superhudTextContext_t textCtx;
	superhudDrawContext_t drawCtx;
	enum shudPWType pwType;
	int pwIndex;
} shudElementPowerupContext;

static void* CG_SHUDElementPwCreate(const superhudConfig_t* config, enum shudPWType pwType, int pwIndex)
{
	shudElementPowerupContext* element;

	SHUD_ELEMENT_INIT(element, config);

	element->pwType = pwType;
	element->pwIndex = pwIndex;

	if (pwType == SHUDPWTYPE_TIME)
	{
		CG_SHUDTextMakeContext(&element->config, &element->textCtx);
		CG_SHUDFillAndFrameForText(&element->config, &element->textCtx);
	}
	else
	{
		CG_SHUDDrawMakeContext(&element->config, &element->drawCtx);
	}

	return element;
}

void* CG_SHUDElementPwTime1Create(const superhudConfig_t* config)
{
	return CG_SHUDElementPwCreate(config, SHUDPWTYPE_TIME, 1);
}

void* CG_SHUDElementPwTime2Create(const superhudConfig_t* config)
{
	return CG_SHUDElementPwCreate(config, SHUDPWTYPE_TIME, 2);
}

void* CG_SHUDElementPwTime3Create(const superhudConfig_t* config)
{
	return CG_SHUDElementPwCreate(config, SHUDPWTYPE_TIME, 3);
}

void* CG_SHUDElementPwTime4Create(const superhudConfig_t* config)
{
	return CG_SHUDElementPwCreate(config, SHUDPWTYPE_TIME, 4);
}

void* CG_SHUDElementPwTime5Create(const superhudConfig_t* config)
{
	return CG_SHUDElementPwCreate(config, SHUDPWTYPE_TIME, 5);
}

void* CG_SHUDElementPwTime6Create(const superhudConfig_t* config)
{
	return CG_SHUDElementPwCreate(config, SHUDPWTYPE_TIME, 6);
}

void* CG_SHUDElementPwTime7Create(const superhudConfig_t* config)
{
	return CG_SHUDElementPwCreate(config, SHUDPWTYPE_TIME, 7);
}

void* CG_SHUDElementPwTime8Create(const superhudConfig_t* config)
{
	return CG_SHUDElementPwCreate(config, SHUDPWTYPE_TIME, 8);
}

void* CG_SHUDElementPwIcon1Create(const superhudConfig_t* config)
{
	return CG_SHUDElementPwCreate(config, SHUDPWTYPE_ICON, 1);
}

void* CG_SHUDElementPwIcon2Create(const superhudConfig_t* config)
{
	return CG_SHUDElementPwCreate(config, SHUDPWTYPE_ICON, 2);
}

void* CG_SHUDElementPwIcon3Create(const superhudConfig_t* config)
{
	return CG_SHUDElementPwCreate(config, SHUDPWTYPE_ICON, 3);
}

void* CG_SHUDElementPwIcon4Create(const superhudConfig_t* config)
{
	return CG_SHUDElementPwCreate(config, SHUDPWTYPE_ICON, 4);
}

void* CG_SHUDElementPwIcon5Create(const superhudConfig_t* config)
{
	return CG_SHUDElementPwCreate(config, SHUDPWTYPE_ICON, 5);
}

void* CG_SHUDElementPwIcon6Create(const superhudConfig_t* config)
{
	return CG_SHUDElementPwCreate(config, SHUDPWTYPE_ICON, 6);
}

void* CG_SHUDElementPwIcon7Create(const superhudConfig_t* config)
{
	return CG_SHUDElementPwCreate(config, SHUDPWTYPE_ICON, 7);
}

void* CG_SHUDElementPwIcon8Create(const superhudConfig_t* config)
{
	return CG_SHUDElementPwCreate(config, SHUDPWTYPE_ICON, 8);
}

void CG_SHUDElementPwRoutine(void* context)
{
	shudElementPowerupContext* element = (shudElementPowerupContext*)context;
	int maxPW;
	int idx;

	if (!wiredHud || !wiredHud->valid) return;

	maxPW = wiredHud->activePowerupCount;
	idx = element->pwIndex - 1;   /* pwIndex is 1-based */

	if (idx < 0 || idx >= maxPW)
	{
		return;
	}

	CG_SHUDFill(&element->config);
	CG_SHUDDrawBorder(&element->config);

	if (element->pwType == SHUDPWTYPE_TIME)
	{
		if (!wiredHud->activePowerups[idx].isHoldable)
		{
			element->textCtx.text = va("%d", wiredHud->activePowerups[idx].timeLeft);
			CG_SHUDTextPrint(&element->config, &element->textCtx);
		}
	}
	else
	{
		element->drawCtx.image = wiredHud->activePowerups[idx].icon;
		CG_SHUDDrawStretchPicCtx(&element->config, &element->drawCtx);
	}
}

void CG_SHUDElementPwDestroy(void* context)
{
	if (context)
	{
		Z_Free(context);
	}
}
#endif /* FEAT_WIRED_UI */
