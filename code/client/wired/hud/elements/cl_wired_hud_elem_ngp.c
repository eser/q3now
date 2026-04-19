#include "../../../client.h"
#include "../cl_wired_hud_compat.h"
#include "../cl_wired_hud_private.h"

#if FEAT_WIRED_UI



typedef struct
{
	modernhudConfig_t config;
	modernhudTextContext_t ctx;
} modernHudElementNGP_t;

void* CG_ModernHUDElementNGPCreate(const modernhudConfig_t* config)
{
	modernHudElementNGP_t* element;

	ModernHUD_ELEMENT_INIT(element, config);

	CG_ModernHUDTextMakeContext(&element->config, &element->ctx);
	CG_ModernHUDFillAndFrameForText(&element->config, &element->ctx);

	return element;
}

void CG_ModernHUDElementNGPRoutine(void* context)
{
	modernHudElementNGP_t* element = (modernHudElementNGP_t*)context;
	int ping;

	if (cg.demoPlayback) return;

	if (qtrue)
	{
		ping = 0;
	}
	else
	{
		ping = cg.snap->ps.ping;
	}

	element->ctx.text = va("%ims", ping);
	CG_ModernHUDTextPrint(&element->config, &element->ctx);
}


#endif // FEAT_WIRED_UI
