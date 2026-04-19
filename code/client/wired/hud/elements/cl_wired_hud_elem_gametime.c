#include "../../../client.h"
#include "../cl_wired_hud_compat.h"
#include "../cl_wired_hud_private.h"

#if FEAT_WIRED_UI



typedef struct
{
	modernhudConfig_t config;
	int timePrev;
	modernhudTextContext_t ctx;
} modernHudElementGameTime_t;

void* CG_ModernHUDElementGameTimeCreate(const modernhudConfig_t* config)
{
	modernHudElementGameTime_t* element;

	ModernHUD_ELEMENT_INIT(element, config);

	CG_ModernHUDTextMakeContext(&element->config, &element->ctx);
	CG_ModernHUDFillAndFrameForText(&element->config, &element->ctx);

	return element;
}

void CG_ModernHUDElementGameTimeRoutine(void* context)
{
	modernHudElementGameTime_t* element = (modernHudElementGameTime_t*)context;


	if (cg_drawTimer.integer)
	{
		int         mins, seconds, tens;
		int         msec;
		msec = cg.time - cgs.levelStartTime;

		if (msec < 0) msec *= -1;

		seconds = msec / 1000;
		mins = seconds / 60;
		seconds -= mins * 60;
		tens = seconds / 10;
		seconds -= tens * 10;

		element->ctx.text = va("%i:%i%i", mins, tens, seconds);
		CG_ModernHUDTextPrint(&element->config, &element->ctx);
	}

}

#endif // FEAT_WIRED_UI
