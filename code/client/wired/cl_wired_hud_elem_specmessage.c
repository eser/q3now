#include "../client.h"
#include "cl_wired_hud_compat.h"
#include "cl_wired_hud_private.h"

#if FEAT_WIRED_UI



typedef struct
{
	superhudConfig_t config;
	superhudTextContext_t ctx;
} shudElementSpecMessage_t;

void* CG_SHUDElementSpecMessageCreate(const superhudConfig_t* config)
{
	shudElementSpecMessage_t* element;

	SHUD_ELEMENT_INIT(element, config);

	CG_SHUDTextMakeContext(&element->config, &element->ctx);
	CG_SHUDFillAndFrameForText(&element->config, &element->ctx);

	element->ctx.text = "^1SPECTATOR";
	return element;
}

void CG_SHUDElementSpecMessageRoutine(void* context)
{
	shudElementSpecMessage_t* element = (shudElementSpecMessage_t*)context;

	if (CG_IsSpectatorOnScreen())
	{
		CG_SHUDTextPrint(&element->config, &element->ctx);
	}
}

void CG_SHUDElementSpecMessageDestroy(void* context)
{
	if (context)
	{
		Z_Free(context);
	}
}
#endif // FEAT_WIRED_UI
