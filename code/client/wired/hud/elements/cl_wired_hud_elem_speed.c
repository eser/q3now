#include "../../../client.h"
#include "../cl_wired_hud_compat.h"
#include "../cl_wired_hud_private.h"

#if FEAT_WIRED_UI



typedef struct
{
	modernhudConfig_t config;
	modernhudTextContext_t ctx;
} modernHudElementPlayerSpeed_t;

void* CG_ModernHUDElementPlayerSpeedCreate(const modernhudConfig_t* config)
{
	modernHudElementPlayerSpeed_t* element;

	ModernHUD_ELEMENT_INIT(element, config);

	CG_ModernHUDTextMakeContext(&element->config, &element->ctx);
	CG_ModernHUDFillAndFrameForText(&element->config, &element->ctx);

	return element;
}

void CG_ModernHUDElementPlayerSpeedRoutine(void* context)
{
	modernHudElementPlayerSpeed_t* element = (modernHudElementPlayerSpeed_t*)context;


	element->ctx.text = va("%dups", (int)cg.xyspeed);


	CG_ModernHUDTextPrint(&element->config, &element->ctx);
}

void CG_ModernHUDElementPlayerSpeedDestroy(void* context)
{
	if (context)
	{
		Z_Free(context);
	}
}
#endif // FEAT_WIRED_UI
