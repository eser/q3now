#include "../../../client.h"
#include "../cl_wired_hud_compat.h"
#include "../cl_wired_hud_private.h"

#if FEAT_WIRED_UI



typedef struct
{
	modernhudConfig_t config;
	modernhudTextContext_t ctx;
	int* time;
	const char* msg;
} modernHudElementRankMessage_t;

void* CG_ModernHUDElementRankMessageCreate(const modernhudConfig_t* config)
{
	modernHudElementRankMessage_t* element;
	modernhudGlobalContext_t* gctx;

	ModernHUD_ELEMENT_INIT(element, config);

	if (!element->config.time.isSet)
	{
		element->config.time.isSet = qtrue;
		element->config.time.value = 2000;
	}

	gctx = CG_ModernHUDGetContext();

	CG_ModernHUDTextMakeContext(&element->config, &element->ctx);
	CG_ModernHUDFillAndFrameForText(&element->config, &element->ctx);

	element->time = &gctx->rankmessage.time;
	element->ctx.text = gctx->rankmessage.message;

	return element;
}

void CG_ModernHUDElementRankMessageRoutine(void* context)
{
	modernHudElementRankMessage_t* element = (modernHudElementRankMessage_t*)context;

	if (!*element->time)
	{
		return;
	}

	WHUD_FADE_AND_PRINT( &element->config, &element->ctx, element->time );
}

#endif // FEAT_WIRED_UI
