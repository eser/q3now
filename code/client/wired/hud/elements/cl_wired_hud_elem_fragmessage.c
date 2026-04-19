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

	WHUD_FADE_AND_PRINT( &element->config, &element->ctx, element->time );
}


#endif // FEAT_WIRED_UI
