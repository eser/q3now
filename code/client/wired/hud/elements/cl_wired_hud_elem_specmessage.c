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
} modernHudElementSpecMessage_t;

void* CG_ModernHUDElementSpecMessageCreate(const modernhudConfig_t* config)
{
	modernHudElementSpecMessage_t* element;

	WHUD_ELEMENT_INIT_TEXT( element, config );

	element->ctx.text = "^1SPECTATOR";
	return element;
}

void CG_ModernHUDElementSpecMessageRoutine(void* context)
{
	modernHudElementSpecMessage_t* element = (modernHudElementSpecMessage_t*)context;

	if (wired_IsSpectator())
	{
		CG_ModernHUDTextPrint(&element->config, &element->ctx);
	}
}

#endif // FEAT_WIRED_UI
