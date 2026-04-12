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

	ModernHUD_ELEMENT_INIT(element, config);

	CG_ModernHUDTextMakeContext(&element->config, &element->ctx);
	CG_ModernHUDFillAndFrameForText(&element->config, &element->ctx);

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

void CG_ModernHUDElementSpecMessageDestroy(void* context)
{
	if (context)
	{
		Z_Free(context);
	}
}
#endif // FEAT_WIRED_UI
