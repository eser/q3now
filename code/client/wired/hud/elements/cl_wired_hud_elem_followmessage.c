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
} modernHudElementFollowMessage_t;

void* CG_ModernHUDElementFollowMessageCreate(const modernhudConfig_t* config)
{
	modernHudElementFollowMessage_t* element;

	WHUD_ELEMENT_INIT_TEXT( element, config );

	return element;
}

void CG_ModernHUDElementFollowMessageRoutine(void* context)
{
	modernHudElementFollowMessage_t* element = (modernHudElementFollowMessage_t*)context;
	const char* str;

	if (!CG_IsFollowing())
	{
		return;
	}

	str = cgs.clientinfo[cg.snap->ps.clientNum].name;
	element->ctx.text = va("Following ^7%s", str);

	CG_ModernHUDTextPrint(&element->config, &element->ctx);
}

#endif // FEAT_WIRED_UI
