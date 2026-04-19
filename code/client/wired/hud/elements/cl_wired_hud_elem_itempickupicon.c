#include "../../../client.h"
#include "../cl_wired_hud_compat.h"
#include "../cl_wired_hud_private.h"

#if FEAT_WIRED_UI

typedef struct
{
	modernhudConfig_t config;
	modernhudDrawContext_t ctx;
} modernHudElementStatusbarItemPickupIcon;

void* CG_ModernHUDElementItemPickupIconCreate(const modernhudConfig_t* config)
{
	modernHudElementStatusbarItemPickupIcon* element;

	ModernHUD_ELEMENT_INIT(element, config);

	CG_ModernHUDDrawMakeContext(&element->config, &element->ctx);

	if (!element->config.time.isSet)
	{
		element->config.time.isSet = qtrue;
		element->config.time.value = 1500;
	}

	return element;
}

void CG_ModernHUDElementItemPickupIconRoutine(void* context)
{
	modernHudElementStatusbarItemPickupIcon* element = (modernHudElementStatusbarItemPickupIcon*)context;

	if (CG_ModernHUDGetFadeColor(element->ctx.color_origin, element->ctx.color, &element->config, cg.itemPickupTime))
	{
		element->ctx.image = cg_items[cg.itemPickup].icon;
		CG_ModernHUDFill(&element->config);
		CG_ModernHUDDrawBorder(&element->config);

		CG_RegisterItemVisuals(cg.itemPickup);
		CG_ModernHUDDrawStretchPicCtx(&element->config, &element->ctx);
	}
}

#endif // FEAT_WIRED_UI
