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

	WHUD_ELEMENT_INIT_TEXT( element, config );

	return element;
}

void CG_ModernHUDElementPlayerSpeedRoutine(void* context)
{
	modernHudElementPlayerSpeed_t* element = (modernHudElementPlayerSpeed_t*)context;


	element->ctx.text = va("%dups", (int)cg.xyspeed);


	CG_ModernHUDTextPrint(&element->config, &element->ctx);
}

#endif // FEAT_WIRED_UI
