// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "../../../client.h"
#include "../cl_wired_hud_compat.h"
#include "../cl_wired_hud_private.h"

#if FEAT_WIRED_UI

typedef struct
{
	modernhudConfig_t config;
	modernhudTextContext_t ctx;
} modernHudElementAmmoMessage_t;

void* CG_ModernHUDElementAmmoMessageCreate(const modernhudConfig_t* config)
{
	modernHudElementAmmoMessage_t* element;

	WHUD_ELEMENT_INIT_TEXT( element, config );

	return element;
}

void CG_ModernHUDElementAmmoMessageRoutine(void* context)
{
	modernHudElementAmmoMessage_t* element = (modernHudElementAmmoMessage_t*)context;

	if (cg_drawAmmoWarning.integer == 0)
	{
		return;
	}

	if (!cg.lowAmmoWarning)
	{
		return;
	}

	if (cg.lowAmmoWarning == 2)
	{
		element->ctx.text = "OUT OF AMMO";
	}
	else
	{
		element->ctx.text = "LOW AMMO WARNING";
	}
	CG_ModernHUDTextPrint(&element->config, &element->ctx);
}

#endif // FEAT_WIRED_UI
