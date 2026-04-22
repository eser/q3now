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

	WHUD_ELEMENT_INIT_TEXT( element, config );

	return element;
}

void CG_ModernHUDElementNGPRoutine(void* context)
{
	modernHudElementNGP_t* element = (modernHudElementNGP_t*)context;

	if (cg.demoPlayback) return;

	int ping;
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
