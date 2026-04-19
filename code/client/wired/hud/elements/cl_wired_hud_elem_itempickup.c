#include "../../../client.h"
#include "../cl_wired_hud_compat.h"
#include "../cl_wired_hud_private.h"

#if FEAT_WIRED_UI



typedef struct
{
	modernhudConfig_t config;
	modernhudTextContext_t ctx;
} modernHudElementItemPickup_t;

void* CG_ModernHUDElementItemPickupCreate(const modernhudConfig_t* config)
{
	modernHudElementItemPickup_t* element;

	ModernHUD_ELEMENT_INIT(element, config);

	CG_ModernHUDTextMakeContext(&element->config, &element->ctx);
	CG_ModernHUDFillAndFrameForText(&element->config, &element->ctx);

	if (!element->config.time.isSet)
	{
		element->config.time.isSet = qtrue;
		element->config.time.value = 1500;
	}

	return element;
}

void CG_ModernHUDElementItemPickupRoutine(void* context)
{
	modernHudElementItemPickup_t* element = (modernHudElementItemPickup_t*)context;
	qboolean visible;

	visible = CG_ModernHUDGetFadeColor(element->ctx.color_origin, element->ctx.color, &element->config, cg.itemPickupTime);

	if (visible)
	{
		int         mins, seconds, tens;
		int         msec;
		msec = cg.itemPickupTime - cgs.levelStartTime;

		seconds = msec / 1000;
		mins = seconds / 60;
		seconds -= mins * 60;
		tens = seconds / 10;
		seconds -= tens * 10;
		if (bg_itemlist[cg.itemPickup].pickup_name)
		{
			if (element->config.style.value == 2)
			{
				element->ctx.text = va("%i:%i%i", mins, tens, seconds); // only time
			}
			else
			{
				element->ctx.text = va("%i:%i%i %s", mins, tens, seconds, bg_itemlist[cg.itemPickup].pickup_name);
			}

			CG_ModernHUDTextPrint(&element->config, &element->ctx);
		}
	}
}

#endif // FEAT_WIRED_UI
