#include "../client.h"
#include "cl_wired_hud_compat.h"
#include "cl_wired_hud_private.h"

#if FEAT_WIRED_UI



typedef struct
{
	superhudConfig_t config;
	superhudTextContext_t ctx;
} shudElementPlayerSpeed_t;

void* CG_SHUDElementPlayerSpeedCreate(const superhudConfig_t* config)
{
	shudElementPlayerSpeed_t* element;

	SHUD_ELEMENT_INIT(element, config);

	CG_SHUDTextMakeContext(&element->config, &element->ctx);
	CG_SHUDFillAndFrameForText(&element->config, &element->ctx);

	return element;
}

void CG_SHUDElementPlayerSpeedRoutine(void* context)
{
	shudElementPlayerSpeed_t* element = (shudElementPlayerSpeed_t*)context;


	element->ctx.text = va("%dups", (int)cg.xyspeed);


	CG_SHUDTextPrint(&element->config, &element->ctx);
}

void CG_SHUDElementPlayerSpeedDestroy(void* context)
{
	if (context)
	{
		Z_Free(context);
	}
}
#endif // FEAT_WIRED_UI
