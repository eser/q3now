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
	modernhudGlobalContext_t* gctx;
	int index;
} modernHudElementChat_t;

void* CG_ModernHUDElementChatCreate(const modernhudConfig_t* config, int line)
{
	modernHudElementChat_t* element;

	ModernHUD_ELEMENT_INIT(element, config);

	element->gctx = CG_ModernHUDGetContext();
	element->index = line;
	CG_ModernHUDTextMakeContext(&element->config, &element->ctx);
	CG_ModernHUDFillAndFrameForText(&element->config, &element->ctx);
	element->ctx.width = (int)config->rect.value[2];

	return element;
}

void CG_ModernHUDElementChatRoutine(void* context)
{
	modernHudElementChat_t* element = (modernHudElementChat_t*)context;

	int index = ((element->gctx->chat.index - 1) - (element->index - 1)) % ModernHUD_MAX_CHAT_LINES;
	modernhudChatEntry_t* entry = &element->gctx->chat.line[index];

	if (entry->message[0] == 0)
	{
		return;
	}

	// forceChat not supported

	if (entry->time == 0)
	{
		return;
	}

	element->ctx.text = entry->message;
	WHUD_FADE_AND_PRINT( &element->config, &element->ctx, &entry->time );
}

#endif // FEAT_WIRED_UI
