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

static void* CG_ModernHUDElementChatCreate(const modernhudConfig_t* config, int line)
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

void* CG_ModernHUDElementChat1Create(const modernhudConfig_t* config)
{
	return CG_ModernHUDElementChatCreate(config, 1);
}

void* CG_ModernHUDElementChat2Create(const modernhudConfig_t* config)
{
	return CG_ModernHUDElementChatCreate(config, 2);
}

void* CG_ModernHUDElementChat3Create(const modernhudConfig_t* config)
{
	return CG_ModernHUDElementChatCreate(config, 3);
}

void* CG_ModernHUDElementChat4Create(const modernhudConfig_t* config)
{
	return CG_ModernHUDElementChatCreate(config, 4);
}

void* CG_ModernHUDElementChat5Create(const modernhudConfig_t* config)
{
	return CG_ModernHUDElementChatCreate(config, 5);
}

void* CG_ModernHUDElementChat6Create(const modernhudConfig_t* config)
{
	return CG_ModernHUDElementChatCreate(config, 6);
}

void* CG_ModernHUDElementChat7Create(const modernhudConfig_t* config)
{
	return CG_ModernHUDElementChatCreate(config, 7);
}

void* CG_ModernHUDElementChat8Create(const modernhudConfig_t* config)
{
	return CG_ModernHUDElementChatCreate(config, 8);
}

void* CG_ModernHUDElementChat9Create(const modernhudConfig_t* config)
{
	return CG_ModernHUDElementChatCreate(config, 9);
}

void* CG_ModernHUDElementChat10Create(const modernhudConfig_t* config)
{
	return CG_ModernHUDElementChatCreate(config, 10);
}

void* CG_ModernHUDElementChat11Create(const modernhudConfig_t* config)
{
	return CG_ModernHUDElementChatCreate(config, 11);
}

void* CG_ModernHUDElementChat12Create(const modernhudConfig_t* config)
{
	return CG_ModernHUDElementChatCreate(config, 12);
}

void* CG_ModernHUDElementChat13Create(const modernhudConfig_t* config)
{
	return CG_ModernHUDElementChatCreate(config, 13);
}

void* CG_ModernHUDElementChat14Create(const modernhudConfig_t* config)
{
	return CG_ModernHUDElementChatCreate(config, 14);
}

void* CG_ModernHUDElementChat15Create(const modernhudConfig_t* config)
{
	return CG_ModernHUDElementChatCreate(config, 15);
}

void* CG_ModernHUDElementChat16Create(const modernhudConfig_t* config)
{
	return CG_ModernHUDElementChatCreate(config, 16);
}

void CG_ModernHUDElementChatRoutine(void* context)
{
	modernHudElementChat_t* element = (modernHudElementChat_t*)context;
	modernhudChatEntry_t* entry;
	int index;

	index = ((element->gctx->chat.index - 1) - (element->index - 1)) % ModernHUD_MAX_CHAT_LINES;

	entry = &element->gctx->chat.line[index];

	if (entry->message[0] == 0)
	{
		return;
	}

	// forceChat not supported in q3now

	if (entry->time == 0)
	{
		return;
	}

	if (!CG_ModernHUDGetFadeColor(element->ctx.color_origin, element->ctx.color, &element->config, entry->time))
	{
		entry->time = 0;
		return;
	}

	element->ctx.text = entry->message;
	CG_ModernHUDTextPrint(&element->config, &element->ctx);
}

void CG_ModernHUDElementChatDestroy(void* context)
{
	if (context)
	{
		Z_Free(context);
	}
}
#endif // FEAT_WIRED_UI
