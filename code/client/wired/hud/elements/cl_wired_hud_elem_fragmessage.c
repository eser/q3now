#include "../../../client.h"
#include "../cl_wired_hud_compat.h"
#include "../cl_wired_hud_private.h"

#if FEAT_WIRED_UI



typedef struct
{
	modernhudConfig_t config;
	modernhudTextContext_t ctx;
	int* time;
} modernHudElementFragMessage_t;

void* CG_ModernHUDElementFragMessageCreate(const modernhudConfig_t* config)
{
	modernHudElementFragMessage_t* element;
	modernhudGlobalContext_t* gctx;

	ModernHUD_ELEMENT_INIT(element, config);

	if (!element->config.time.isSet)
	{
		element->config.time.isSet = qtrue;
		element->config.time.value = 2000;
	}

	gctx = CG_ModernHUDGetContext();
	element->time = &gctx->fragmessage.time;

	CG_ModernHUDTextMakeContext(&element->config, &element->ctx);
	CG_ModernHUDFillAndFrameForText(&element->config, &element->ctx);

	element->ctx.text = gctx->fragmessage.message;

	return element;
}

void CG_ModernHUDElementFragMessageRoutine(void* context)
{
	modernHudElementFragMessage_t* element = (modernHudElementFragMessage_t*)context;

	if (!*element->time)
	{
		return;
	}

	if (!CG_ModernHUDGetFadeColor(element->ctx.color_origin, element->ctx.color, &element->config, *element->time))
	{
		*element->time = 0;
		return;
	}

	CG_ModernHUDTextPrint(&element->config, &element->ctx);
}

void CG_ModernHUDElementFragMessageDestroy(void* context)
{
	if (context)
	{
		Z_Free(context);
	}
}


#endif // FEAT_WIRED_UI
